// SPDX-License-Identifier: Apache-2.0

#include "fake_v4l2.hpp"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/select.h>

#include <cerrno>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <string>

namespace
{
bool isVideoDevice(const char* path)
{
    return path != nullptr && std::strncmp(path, "/dev/video", 10) == 0;
}

int wrappedOpen(const char* path, int flags, int mode, bool hasMode);
int dispatchFcntl(int fd, int command, long argument, bool hasArgument);
int dispatchIoctl(int fd, unsigned long request, void* argument);
} // namespace

extern "C"
{
int __real_open(const char* path, int flags, ...);
int __real_close(int fd);
int __real_fcntl(int fd, int command, ...);
int __real_ioctl(int fd, unsigned long request, ...);

bool
    __wrap__ZN4ikvm5isDirERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE(
        const std::string& path)
{
    ++ikvm::amiWrapLog.isDirCalls;
    ikvm::amiWrapLog.isDirPaths.push_back(path);
    return ikvm::amiWrapLog.isDirResult;
}

void
    __wrap__ZN4ikvm15eventLogSupportERKNSt7__cxx1112basic_stringIcSt11char_traitsIcESaIcEEE(
        const std::string& message)
{
    ++ikvm::amiWrapLog.eventLogCalls;
    ikvm::amiWrapLog.eventLogMessages.push_back(message);
}

int __wrap_open(const char* path, int flags, ...)
{
    const bool hasMode = (flags & O_CREAT) != 0;
    int mode = 0;
    if (hasMode)
    {
        va_list arguments;
        va_start(arguments, flags);
        mode = va_arg(arguments, int);
        va_end(arguments);
    }
    return wrappedOpen(path, flags, mode, hasMode);
}

int __wrap_close(int fd)
{
    if (fd == test::fakeV4L2.fakeFd)
    {
        ++test::fakeV4L2.stats.closeCalls;
        test::fakeV4L2.fakeFd = -1;
        return 0;
    }
    return __real_close(fd);
}

int __wrap_fcntl(int fd, int command, ...)
{
    const bool hasArgument = command != F_GETFL;
    int argument = 0;
    if (hasArgument)
    {
        va_list arguments;
        va_start(arguments, command);
        argument = va_arg(arguments, int);
        va_end(arguments);
    }
    return dispatchFcntl(fd, command, argument, hasArgument);
}

int __wrap_select(int, fd_set*, fd_set*, fd_set*, timeval*)
{
    ++test::fakeV4L2.stats.selectCalls;
    return test::fakeV4L2.selectReady ? 1 : 0;
}

void* __wrap_mmap(void*, size_t length, int, int, int fd, off_t offset)
{
    auto& fake = test::fakeV4L2;
    if (fd != fake.fakeFd)
    {
        errno = EBADF;
        return MAP_FAILED;
    }
    ++fake.stats.mmapCalls;
    if (fake.mmapFails)
    {
        errno = ENOMEM;
        return MAP_FAILED;
    }
    void* address = std::malloc(length);
    fake.stats.mappedRegions[address] = {address, length, fd, offset};
    return address;
}

int __wrap_munmap(void* address, size_t)
{
    auto& fake = test::fakeV4L2;
    ++fake.stats.munmapCalls;
    const auto region = fake.stats.mappedRegions.find(address);
    if (address == nullptr || address == MAP_FAILED ||
        region == fake.stats.mappedRegions.end())
    {
        errno = EINVAL;
        return -1;
    }
    fake.stats.unmappedRegions.push_back(region->second);
    fake.stats.mappedRegions.erase(region);
    std::free(address);
    return 0;
}

int __wrap_ioctl(int fd, unsigned long request, ...)
{
    va_list arguments;
    va_start(arguments, request);
    void* argument = va_arg(arguments, void*);
    va_end(arguments);
    return dispatchIoctl(fd, request, argument);
}

int __wrap___open64(const char* path, int flags, ...)
{
    const bool hasMode = (flags & O_CREAT) != 0;
    int mode = 0;
    if (hasMode)
    {
        va_list arguments;
        va_start(arguments, flags);
        mode = va_arg(arguments, int);
        va_end(arguments);
    }
    return wrappedOpen(path, flags, mode, hasMode);
}

int __wrap_open64(const char* path, int flags, ...)
{
    const bool hasMode = (flags & O_CREAT) != 0;
    int mode = 0;
    if (hasMode)
    {
        va_list arguments;
        va_start(arguments, flags);
        mode = va_arg(arguments, int);
        va_end(arguments);
    }
    return wrappedOpen(path, flags, mode, hasMode);
}

int __wrap___close(int fd)
{
    return __wrap_close(fd);
}

int __wrap___ioctl(int fd, unsigned long request, ...)
{
    va_list arguments;
    va_start(arguments, request);
    void* argument = va_arg(arguments, void*);
    va_end(arguments);
    return dispatchIoctl(fd, request, argument);
}

int __wrap___ioctl_time64(int fd, unsigned long request, ...)
{
    va_list arguments;
    va_start(arguments, request);
    void* argument = va_arg(arguments, void*);
    va_end(arguments);
    return dispatchIoctl(fd, request, argument);
}

void* __wrap_mmap64(void* address, size_t length, int protection, int flags,
                    int fd, off_t offset)
{
    return __wrap_mmap(address, length, protection, flags, fd, offset);
}

int __wrap___munmap(void* address, size_t length)
{
    return __wrap_munmap(address, length);
}

int __wrap___select(int count, fd_set* read, fd_set* write, fd_set* error,
                    timeval* timeout)
{
    return __wrap_select(count, read, write, error, timeout);
}

int __wrap___select64(int count, fd_set* read, fd_set* write, fd_set* error,
                      void*)
{
    return __wrap_select(count, read, write, error, nullptr);
}

int __wrap_fcntl64(int fd, int command, ...)
{
    const bool hasArgument = command != F_GETFL;
    int argument = 0;
    if (hasArgument)
    {
        va_list arguments;
        va_start(arguments, command);
        argument = va_arg(arguments, int);
        va_end(arguments);
    }
    return dispatchFcntl(fd, command, argument, hasArgument);
}

int __wrap___fcntl64(int fd, int command, ...)
{
    const bool hasArgument = command != F_GETFL;
    int argument = 0;
    if (hasArgument)
    {
        va_list arguments;
        va_start(arguments, command);
        argument = va_arg(arguments, int);
        va_end(arguments);
    }
    return dispatchFcntl(fd, command, argument, hasArgument);
}

int __wrap___fcntl_time64(int fd, int command, ...)
{
    const bool hasArgument = command != F_GETFL;
    int argument = 0;
    if (hasArgument)
    {
        va_list arguments;
        va_start(arguments, command);
        argument = va_arg(arguments, int);
        va_end(arguments);
    }
    return dispatchFcntl(fd, command, argument, hasArgument);
}
} // extern "C"

namespace
{
int wrappedOpen(const char* path, int flags, int mode, bool hasMode)
{
    auto& fake = test::fakeV4L2;
    if (!isVideoDevice(path))
    {
        return hasMode ? __real_open(path, flags, mode)
                       : __real_open(path, flags);
    }
    ++fake.stats.openCalls;
    if (fake.openFails)
    {
        errno = ENODEV;
        return -1;
    }
    fake.fakeFd = fake.nextFd;
    return fake.fakeFd;
}

int dispatchFcntl(int fd, int command, long argument, bool hasArgument)
{
    auto& fake = test::fakeV4L2;
    if (fd != fake.fakeFd)
    {
        return hasArgument ? __real_fcntl(fd, command, argument)
                           : __real_fcntl(fd, command);
    }
    if (command == F_GETFL)
    {
        ++fake.stats.fcntlGetCalls;
        return fake.stats.fcntlFlags;
    }
    if (command == F_SETFL)
    {
        ++fake.stats.fcntlSetCalls;
        fake.stats.fcntlFlags = static_cast<int>(argument);
        return 0;
    }
    errno = EINVAL;
    return -1;
}

int dispatchIoctl(int fd, unsigned long request, void* argument)
{
    auto& fake = test::fakeV4L2;
    if (fd != fake.fakeFd)
    {
        return __real_ioctl(fd, request, argument);
    }

    switch (request)
    {
        case VIDIOC_QUERYCAP:
        {
            if (fake.querycapFails)
            {
                errno = EIO;
                return -1;
            }
            auto* capability = static_cast<v4l2_capability*>(argument);
            std::memset(capability, 0, sizeof(*capability));
            capability->capabilities = fake.capabilities;
            return 0;
        }
        case VIDIOC_G_FMT:
        {
            ++fake.stats.gFmtCalls;
            if (fake.getFormatFails)
            {
                errno = EIO;
                return -1;
            }
            auto* format = static_cast<v4l2_format*>(argument);
            std::memset(format, 0, sizeof(*format));
            format->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            format->fmt.pix.width = fake.formatWidth;
            format->fmt.pix.height = fake.formatHeight;
            format->fmt.pix.pixelformat = fake.pixelFormat;
            return 0;
        }
        case VIDIOC_S_FMT:
        {
            ++fake.stats.sFmtCalls;
            if (fake.setFormatFails)
            {
                errno = EIO;
                return -1;
            }
            auto* format = static_cast<v4l2_format*>(argument);
            if (fake.returnedPixelFormat != 0)
            {
                format->fmt.pix.pixelformat = fake.returnedPixelFormat;
            }
            fake.stats.lastFormat = *format;
            fake.stats.hasLastFormat = true;
            return 0;
        }
        case VIDIOC_S_PARM:
            ++fake.stats.sParmCalls;
            if (fake.setParmFails)
            {
                errno = EIO;
                return -1;
            }
            return 0;
        case VIDIOC_S_CTRL:
            ++fake.stats.sCtrlCalls;
            if (fake.setControlFailures > 0)
            {
                --fake.setControlFailures;
                errno = EIO;
                return -1;
            }
            return 0;
        case VIDIOC_REQBUFS:
        {
            ++fake.stats.reqbufCalls;
            auto* requestBuffers = static_cast<v4l2_requestbuffers*>(argument);
            if (requestBuffers->count != 0)
            {
                requestBuffers->count = fake.requestedBufferCount;
            }
            return requestBuffers->count < 2 && requestBuffers->count != 0
                       ? -1
                       : 0;
        }
        case VIDIOC_QUERYBUF:
        {
            ++fake.stats.querybufCalls;
            auto* buffer = static_cast<v4l2_buffer*>(argument);
            if (fake.querybufFails ||
                buffer->index >= fake.requestedBufferCount)
            {
                errno = EIO;
                return -1;
            }
            auto& fakeBuffer = fake.buffers[buffer->index];
            fakeBuffer.offset = buffer->index * 4096;
            buffer->length = 256 * 1024;
            buffer->m.offset = fakeBuffer.offset;
            return 0;
        }
        case VIDIOC_QBUF:
        {
            ++fake.stats.qbufCalls;
            auto* buffer = static_cast<v4l2_buffer*>(argument);
            if (fake.qbufFails)
            {
                errno = EIO;
                return -1;
            }
            fake.buffers[buffer->index].queued = true;
            return 0;
        }
        case VIDIOC_DQBUF:
        {
            ++fake.stats.dqbufCalls;
            if (fake.dequeueIndices.empty())
            {
                errno = EAGAIN;
                return -1;
            }
            auto* buffer = static_cast<v4l2_buffer*>(argument);
            const uint32_t index = fake.dequeueIndices.front();
            fake.dequeueIndices.pop_front();
            const auto& fakeBuffer = fake.buffers[index];
            buffer->index = index;
            buffer->bytesused = fakeBuffer.bytesUsed;
            buffer->sequence = fakeBuffer.sequence;
            buffer->flags = fakeBuffer.flags;
            return 0;
        }
        case VIDIOC_STREAMON:
            ++fake.stats.streamonCalls;
            if (fake.streamonFails)
            {
                errno = EIO;
                return -1;
            }
            return 0;
        case VIDIOC_STREAMOFF:
            ++fake.stats.streamoffCalls;
            if (fake.streamoffFails)
            {
                errno = EIO;
                return -1;
            }
            return 0;
        case VIDIOC_G_SELECTION:
        {
            ++fake.stats.gSelectionCalls;
            if (fake.getSelectionFails)
            {
                errno = EIO;
                return -1;
            }
            static_cast<v4l2_selection*>(argument)->r = fake.selectionRect;
            return 0;
        }
        case VIDIOC_QUERY_DV_TIMINGS:
        {
            ++fake.stats.queryTimingsCalls;
            if (!fake.timingsAvailable)
            {
                errno = EPROTO;
                return -1;
            }
            auto* timings = static_cast<v4l2_dv_timings*>(argument);
            std::memset(timings, 0, sizeof(*timings));
            timings->bt.width = fake.timingsWidth;
            timings->bt.height = fake.timingsHeight;
            return 0;
        }
        case VIDIOC_S_DV_TIMINGS:
            ++fake.stats.sTimingsCalls;
            if (fake.setTimingsFails)
            {
                errno = EIO;
                return -1;
            }
            return 0;
        case VIDIOC_ENUMINPUT:
        {
            if (fake.enuminputFails)
            {
                errno = EIO;
                return -1;
            }
            auto* input = static_cast<v4l2_input*>(argument);
            std::memset(input, 0, sizeof(*input));
            input->status = fake.inputStatus;
            return 0;
        }
        default:
            errno = ENOTTY;
            return -1;
    }
}
} // namespace
