#include "ikvm_video.hpp"

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/Device/error.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>

#define V4L2_PIX_FMT_FLAG_PARTIAL_JPG 0x00000004

namespace ikvm
{
const int Video::bitsPerSample(8);
const int Video::bytesPerPixel(4);
const int Video::samplesPerPixel(3);

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::File::Error;
using namespace sdbusplus::xyz::openbmc_project::Common::Device::Error;

Video::Video(const std::string& p, Input& input, int fr, int sub, int fmt) :
    resizeAfterOpen(false), timingsError(false), fd(-1), frameRate(fr),
    height(600), width(800), subSampling(sub), input(input), format(fmt),
    originalFormat(fmt), path(p), pixelformat(V4L2_PIX_FMT_JPEG)
{}

Video::~Video()
{
    stop();
}

char* Video::getData()
{
    if (buffersDone.empty())
    {
        return nullptr;
    }

    return (char*)buffers[buffersDone.front()].data;
}

char* Video::getData(unsigned int i)
{
    if (i >= buffers.size())
    {
        return nullptr;
    }
    return (char*)buffers[i].data;
}

void Video::getFrame()
{
    int rc(0);
    int fd_flags;
    v4l2_buffer buf;
    fd_set fds;
    timeval tv;
    v4l2_selection comp = {.type = V4L2_BUF_TYPE_VIDEO_CAPTURE,
                           .target = V4L2_SEL_TGT_CROP_DEFAULT};

    if (fd < 0)
    {
        return;
    }

    // Don't get more new frames until we run out of previous ones
    if (!buffersDone.empty())
    {
        return;
    }

    FD_ZERO(&fds);
    FD_SET(fd, &fds);

    tv.tv_sec = 1;
    tv.tv_usec = 0;

    memset(&buf, 0, sizeof(v4l2_buffer));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    // Switch to non-blocking in order to safely dequeue all buffers; if the
    // video signal is lost while blocking to dequeue, the video driver may
    // wait forever if signal is not re-acquired
    fd_flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, fd_flags | O_NONBLOCK);

    rc = select(fd + 1, &fds, NULL, NULL, &tv);
    if (rc > 0)
    {
        do
        {
            rc = ioctl(fd, VIDIOC_DQBUF, &buf);
            if (rc >= 0)
            {
                buffers[buf.index].queued = false;

                if (!(buf.flags & V4L2_BUF_FLAG_ERROR))
                {
                    buffers[buf.index].payload = buf.bytesused;
                    buffers[buf.index].sequence = buf.sequence;
                    if (format == 2)
                    {
                        rc = ioctl(fd, VIDIOC_G_SELECTION, &comp);
                        if (rc)
                        {
                            log<level::ERR>("Failed to get selection box",
                                            entry("ERROR=%s", strerror(errno)));
                            comp.r.left = 0;
                            comp.r.top = 0;
                            comp.r.width = width;
                            comp.r.height = height;
                        }
                        buffers[buf.index].box = comp.r;
                    }
                    buffersDone.push_back(buf.index);
                }
                else
                {
                    buffers[buf.index].payload = 0;
                    qbuf(buf.index);
                }
            }
        } while (rc >= 0);
    }

    fcntl(fd, F_SETFL, fd_flags);
}

void Video::qbuf(int i)
{
    int rc;
    v4l2_buffer buf;
    if (!buffers[i].queued)
    {
        memset(&buf, 0, sizeof(v4l2_buffer));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        rc = ioctl(fd, VIDIOC_QBUF, &buf);
        if (rc)
        {
            log<level::ERR>("Failed to queue buffer",
                            entry("ERROR=%s", strerror(errno)));
        }
        else
        {
            buffers[i].queued = true;
        }
    }
}

void Video::releaseFrames()
{
    int i;

    if (!buffersDone.empty())
    {
        i = buffersDone.front();
        buffersDone.pop_front();
        qbuf(i);
    }
}

bool Video::needsResize()
{
    int rc;
    v4l2_dv_timings timings;

    if (fd < 0)
    {
        return false;
    }

    if (resizeAfterOpen)
    {
        return true;
    }

    memset(&timings, 0, sizeof(v4l2_dv_timings));
    rc = ioctl(fd, VIDIOC_QUERY_DV_TIMINGS, &timings);
    if (rc < 0)
    {
        if (!timingsError)
        {
            log<level::ERR>("Failed to query timings",
                            entry("ERROR=%s", strerror(errno)));
            timingsError = true;
        }

        restart();
        return false;
    }
    else
    {
        timingsError = false;
    }

    if (timings.bt.width != width || timings.bt.height != height)
    {
        width = timings.bt.width;
        height = timings.bt.height;

        if (!width || !height)
        {
            log<level::ERR>("Failed to get new resolution",
                            entry("WIDTH=%d", width),
                            entry("HEIGHT=%d", height));
            elog<Open>(
                xyz::openbmc_project::Common::File::Open::ERRNO(-EPROTO),
                xyz::openbmc_project::Common::File::Open::PATH(path.c_str()));
        }

        buffersDone.clear();
        return true;
    }

    return false;
}

void Video::resize()
{
    int rc;
    unsigned int i;
    bool needsResizeCall(false);
    v4l2_buf_type type(V4L2_BUF_TYPE_VIDEO_CAPTURE);
    v4l2_requestbuffers req;

    if (fd < 0)
    {
        return;
    }

    if (resizeAfterOpen)
    {
        resizeAfterOpen = false;
        return;
    }

    for (i = 0; i < buffers.size(); ++i)
    {
        if (buffers[i].data)
        {
            needsResizeCall = true;
            break;
        }
    }

    if (needsResizeCall)
    {
        rc = ioctl(fd, VIDIOC_STREAMOFF, &type);
        if (rc)
        {
            log<level::ERR>("Failed to stop streaming",
                            entry("ERROR=%s", strerror(errno)));
            elog<ReadFailure>(
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_ERRNO(errno),
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_DEVICE_PATH(path.c_str()));
        }
    }

    for (i = 0; i < buffers.size(); ++i)
    {
        if (buffers[i].data)
        {
            munmap(buffers[i].data, buffers[i].size);
            buffers[i].data = nullptr;
            buffers[i].queued = false;
        }
    }

    if (needsResizeCall)
    {
        v4l2_dv_timings timings;

        memset(&req, 0, sizeof(v4l2_requestbuffers));
        req.count = 0;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        rc = ioctl(fd, VIDIOC_REQBUFS, &req);
        if (rc < 0)
        {
            log<level::ERR>("Failed to zero streaming buffers",
                            entry("ERROR=%s", strerror(errno)));
            elog<ReadFailure>(
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_ERRNO(errno),
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_DEVICE_PATH(path.c_str()));
        }

        memset(&timings, 0, sizeof(v4l2_dv_timings));
        rc = ioctl(fd, VIDIOC_QUERY_DV_TIMINGS, &timings);
        if (rc < 0)
        {
            log<level::ERR>("Failed to query timings, restart",
                            entry("ERROR=%s", strerror(errno)));
            restart();
            return;
        }

        rc = ioctl(fd, VIDIOC_S_DV_TIMINGS, &timings);
        if (rc < 0)
        {
            log<level::ERR>("Failed to set timings",
                            entry("ERROR=%s", strerror(errno)));
            elog<ReadFailure>(
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_ERRNO(errno),
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_DEVICE_PATH(path.c_str()));
        }

        buffers.clear();
    }

    memset(&req, 0, sizeof(v4l2_requestbuffers));
    req.count = 3;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    rc = ioctl(fd, VIDIOC_REQBUFS, &req);
    if (rc < 0 || req.count < 2)
    {
        log<level::ERR>("Failed to request streaming buffers",
                        entry("ERROR=%s", strerror(errno)));
        elog<ReadFailure>(
            xyz::openbmc_project::Common::Device::ReadFailure::CALLOUT_ERRNO(
                errno),
            xyz::openbmc_project::Common::Device::ReadFailure::
                CALLOUT_DEVICE_PATH(path.c_str()));
    }

    buffers.resize(req.count);

    for (i = 0; i < buffers.size(); ++i)
    {
        v4l2_buffer buf;

        memset(&buf, 0, sizeof(v4l2_buffer));
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;

        rc = ioctl(fd, VIDIOC_QUERYBUF, &buf);
        if (rc < 0)
        {
            log<level::ERR>("Failed to query buffer",
                            entry("ERROR=%s", strerror(errno)));
            elog<ReadFailure>(
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_ERRNO(errno),
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_DEVICE_PATH(path.c_str()));
        }

        buffers[i].data = mmap(NULL, buf.length, PROT_READ | PROT_WRITE,
                               MAP_SHARED, fd, buf.m.offset);
        if (buffers[i].data == MAP_FAILED)
        {
            log<level::ERR>("Failed to mmap buffer",
                            entry("ERROR=%s", strerror(errno)));
            elog<ReadFailure>(
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_ERRNO(errno),
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_DEVICE_PATH(path.c_str()));
        }

        buffers[i].size = buf.length;

        rc = ioctl(fd, VIDIOC_QBUF, &buf);
        if (rc < 0)
        {
            log<level::ERR>("Failed to queue buffer",
                            entry("ERROR=%s", strerror(errno)));
            elog<ReadFailure>(
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_ERRNO(errno),
                xyz::openbmc_project::Common::Device::ReadFailure::
                    CALLOUT_DEVICE_PATH(path.c_str()));
        }

        buffers[i].queued = true;
    }

    rc = ioctl(fd, VIDIOC_STREAMON, &type);
    if (rc)
    {
        log<level::ERR>("Failed to start streaming",
                        entry("ERROR=%s", strerror(errno)));
        elog<ReadFailure>(
            xyz::openbmc_project::Common::Device::ReadFailure::CALLOUT_ERRNO(
                errno),
            xyz::openbmc_project::Common::Device::ReadFailure::
                CALLOUT_DEVICE_PATH(path.c_str()));
    }
}

void Video::start()
{
    int rc;
    size_t oldHeight = height;
    size_t oldWidth = width;
    v4l2_capability cap;
    v4l2_format fmt;
    v4l2_streamparm sparm;
    v4l2_control ctrl;

    if (fd >= 0)
    {
        return;
    }

    input.sendWakeupPacket();

    fd = open(path.c_str(), O_RDWR);
    if (fd < 0)
    {
        log<level::ERR>("Failed to open video device",
                        entry("PATH=%s", path.c_str()),
                        entry("ERROR=%s", strerror(errno)));
        elog<Open>(
            xyz::openbmc_project::Common::File::Open::ERRNO(errno),
            xyz::openbmc_project::Common::File::Open::PATH(path.c_str()));
    }

    memset(&cap, 0, sizeof(v4l2_capability));
    rc = ioctl(fd, VIDIOC_QUERYCAP, &cap);
    if (rc < 0)
    {
        log<level::ERR>("Failed to query video device capabilities",
                        entry("ERROR=%s", strerror(errno)));
        elog<ReadFailure>(
            xyz::openbmc_project::Common::Device::ReadFailure::CALLOUT_ERRNO(
                errno),
            xyz::openbmc_project::Common::Device::ReadFailure::
                CALLOUT_DEVICE_PATH(path.c_str()));
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE) ||
        !(cap.capabilities & V4L2_CAP_STREAMING))
    {
        log<level::ERR>("Video device doesn't support this application");
        elog<Open>(
            xyz::openbmc_project::Common::File::Open::ERRNO(errno),
            xyz::openbmc_project::Common::File::Open::PATH(path.c_str()));
    }

    memset(&fmt, 0, sizeof(v4l2_format));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    rc = ioctl(fd, VIDIOC_G_FMT, &fmt);
    if (rc < 0)
    {
        log<level::ERR>("Failed to query video device format",
                        entry("ERROR=%s", strerror(errno)));
        elog<ReadFailure>(
            xyz::openbmc_project::Common::Device::ReadFailure::CALLOUT_ERRNO(
                errno),
            xyz::openbmc_project::Common::Device::ReadFailure::
                CALLOUT_DEVICE_PATH(path.c_str()));
    }

    switch (format)
    {
        case 2:
            fmt.fmt.pix.flags = V4L2_PIX_FMT_FLAG_PARTIAL_JPG;
        default:
        case 0:
            fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_JPEG;
            break;
    }
    rc = ioctl(fd, VIDIOC_S_FMT, &fmt);
    if (rc < 0)
    {
        log<level::ERR>("Failed to set video device format",
                        entry("ERROR=%s", strerror(errno)));
        elog<ReadFailure>(
            xyz::openbmc_project::Common::Device::ReadFailure::CALLOUT_ERRNO(
                errno),
            xyz::openbmc_project::Common::Device::ReadFailure::
                CALLOUT_DEVICE_PATH(path.c_str()));
    }

    memset(&sparm, 0, sizeof(v4l2_streamparm));
    sparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    sparm.parm.capture.timeperframe.numerator = 1;
    sparm.parm.capture.timeperframe.denominator = frameRate;
    rc = ioctl(fd, VIDIOC_S_PARM, &sparm);
    if (rc < 0)
    {
        log<level::WARNING>("Failed to set video device frame rate",
                            entry("ERROR=%s", strerror(errno)));
    }

    ctrl.id = V4L2_CID_JPEG_CHROMA_SUBSAMPLING;
    ctrl.value = subSampling ? V4L2_JPEG_CHROMA_SUBSAMPLING_420
                             : V4L2_JPEG_CHROMA_SUBSAMPLING_444;
    rc = ioctl(fd, VIDIOC_S_CTRL, &ctrl);
    if (rc < 0)
    {
        log<level::WARNING>("Failed to set video jpeg subsampling",
                            entry("ERROR=%s", strerror(errno)));
    }

    height = fmt.fmt.pix.height;
    width = fmt.fmt.pix.width;
    pixelformat = fmt.fmt.pix.pixelformat;

    if (pixelformat != V4L2_PIX_FMT_RGB24 && pixelformat != V4L2_PIX_FMT_JPEG)
    {
        log<level::ERR>("Pixel Format not supported",
                        entry("PIXELFORMAT=%d", pixelformat));
    }

    resize();

    if (oldHeight != height || oldWidth != width)
    {
        resizeAfterOpen = true;
    }
}

void Video::stop()
{
    int rc;
    unsigned int i;
    v4l2_buf_type type(V4L2_BUF_TYPE_VIDEO_CAPTURE);

    if (fd < 0)
    {
        return;
    }

    buffersDone.clear();

    rc = ioctl(fd, VIDIOC_STREAMOFF, &type);
    if (rc)
    {
        log<level::ERR>("Failed to stop streaming",
                        entry("ERROR=%s", strerror(errno)));
    }

    for (i = 0; i < buffers.size(); ++i)
    {
        if (buffers[i].data)
        {
            munmap(buffers[i].data, buffers[i].size);
            buffers[i].data = nullptr;
            buffers[i].queued = false;
        }
    }

    close(fd);
    fd = -1;
}

} // namespace ikvm
