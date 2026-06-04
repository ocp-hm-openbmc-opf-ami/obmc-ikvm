/*
 * ****************************************************************************
 *
 * AMI Extension of Video Class
 * Filename : ikvm_video_ami.cpp
 *
 * @brief AMI Extension of Video Class to support various Feature
 * Implementation.
 *
 *
 * Author: Amlana Bhuyan [amlanab@ami.com]
 *
 * ****************************************************************************
 */
#include "ami/include/ikvm_utils.hpp"
#include "ikvm_video.hpp"

#include <linux/videodev2.h>

#include <atomic>
#include <chrono>
#include <cstdlib> // for system()
#include <exception>
#include <stdexcept>
#include <thread>

namespace ikvm
{
uint32_t Video::getSignalStatus()
{
    int rc(0);
    v4l2_input in;

    if (fd < 0)
    {
        return UINT32_MAX;
    }

    memset(&in, 0, sizeof(v4l2_input));
    // Query the first input
    in.index = 0;

    rc = ioctl(fd, VIDIOC_ENUMINPUT, &in);
    if (rc < 0)
    {
        log<level::ERR>("Failed to query VIDIOC_ENUMINPUT ",
                        entry("ERROR=%s", strerror(errno)));
        return UINT32_MAX;
    }

    log<level::INFO>(" Input Video INFO :",
                     entry(" Signal Status : %u", in.status));

    return in.status;
}

void Video::formatChange(int newformat)
{
    stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    setFormat(newformat);
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    start();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void Video::screenShot(const std::string& screenShotPath)
{
    if (buffersDone.empty() || buffersDone.front() < 0)
    {
        log<level::ERR>("Buffer front empty");
        return;
    }
    if (!(isDir(bsodDir)))
    {
        log<level::ERR>("Unable to create destination Path");
        return;
    }

    try
    {
        std::ofstream screenshot(screenShotPath,
                                 std::ios::out | std::ios::binary);
        if (!screenshot)
        {
            log<level::ERR>(" Failed to create destination file",
                            entry("FILE_PATH=%s", screenShotPath.c_str()));
            return;
        }

        uint32_t stat = getSignalStatus();

        // Lambda function to avoid boiler plate code
        auto copyImageToScreenshot = [&](const std::string& imagePath) {
            std::ifstream imageFile(imagePath, std::ios::binary);
            if (imageFile)
            {
                screenshot << imageFile.rdbuf();
                imageFile.close();
            }
            else
            {
                log<level::ERR>("Failed to open image file",
                                entry("IMAGE_PATH=%s", imagePath.c_str()));
            }
        };

        if (stat == UINT32_MAX)
        {
            log<level::ERR>("ERROR in getting HOST Signal status");
            copyImageToScreenshot(NO_SIGNAL_IMG_PATH);
        }
        else if (stat == V4L2_IN_ST_NO_SIGNAL)
        {
            if (hostPowerState == "Off")
            {
                copyImageToScreenshot(POWER_OFF_IMG_PATH);
                log<level::INFO>("[screenshot] Host POWER OFF ");
            }
            else
            {
                copyImageToScreenshot(NO_SIGNAL_IMG_PATH);
                log<level::INFO>("[screenshot] Host NO SIGNAL ");
            }
        }
        else
        {
            auto& buff = buffers[buffersDone.front()];
            screenshot.write(reinterpret_cast<char*>(buff.data), buff.size);
            log<level::INFO>("[screenshot] Host Video Stream Buffer ");
        }

        screenshot.close();
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Error in handling screenshot ",
                        entry("ERROR=%s", e.what()));
    }
}

void Video::setFrame(const char* ImgPath)
{
    static int sequenceNumber = 1;
    size_t size = 0;

    buffers[0].queued = false;
    std::ifstream file(ImgPath, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        log<level::ERR>("Failed to open image file",
                        entry("ERROR=%s", strerror(errno)));
        return;
    }

    size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> buffer(size);
    std::fill(buffer.begin(), buffer.end(), 0);

    if (!file.read(buffer.data(),
                   size)) // Reading the image file into the buffer
    {
        log<level::ERR>("Failed to read image file",
                        entry("ERROR=%s", strerror(errno)));
        file.close();
        return;
    }
    file.close(); // Close the file after reading

    if (!buffers[0].queued)
    {
        memcpy(buffers[0].data, buffer.data(), size);
        buffers[0].payload = size;
        buffers[0].sequence = sequenceNumber++;
        buffers[0].box = {0, 0, static_cast<unsigned int>(width),
                          static_cast<unsigned int>(height)};
        buffers[0].queued = true;
        buffersDone.push_back(0);
    }
}

void Video::pushRecFrame()
{
    if (!videoRecFlag.load() || !recThreadStatus.load())
        return;

    if (buffersDone.empty())
        return;

    auto i = buffersDone.front();
    char* data = getData(i);
    size_t size = getFrameSize(i);
    if (!data || size == 0)
        return;

    {
        std::lock_guard<std::mutex> lk(recMutex);
        // Skip if same V4L2 frame (sendFrame skipped releaseFrames); guard is
        // inside lock to avoid data race with clearRecQueue().
        if (buffers[i].sequence == lastPushedSeq)
            return;
        if (recFrameQueue.size() < 30)
        {
            recFrameQueue.emplace_back(data, data + size);
            lastPushedSeq = buffers[i].sequence;
        }
    }
}

void Video::clearRecQueue()
{
    std::lock_guard<std::mutex> lk(recMutex);
    recFrameQueue.clear();
    lastPushedSeq = UINT32_MAX; // reset so the first frame of next recording is
                                // always captured
}

void Video::videoRecord(Video* video)
{
    if (!(ikvm::active))
    {
        log<level::ERR>(
            "Remote Storage not Active(Available) requires reconfiguration");
        Video::updateRecStat("Stop");
        recThreadStatus.store(false);
        return;
    }
    if (!(isDir(recProcessDir)))
    {
        log<level::ERR>("Unable to create destination Path");
        Video::updateRecStat("Stop");
        recThreadStatus.store(false);
        return;
    }

    size_t outputSize = 0;
    size_t maxSize = 7 * 1024 * 1024;
    auto recDuration = std::chrono::seconds(10);

    if (ikvm::recordToRemote)
    {
        recDuration = std::chrono::seconds(ikvm::maxDuration);
        maxSize = (ikvm::maxSize) * 1024 * 1024;
    }

    video->clearRecQueue();

    try
    {
        /*clear the previous data*/
        std::ofstream screenRec(ikvm::screenRecPath,
                                std::ios::out | std::ios::trunc);
        screenRec.close();
        outputSize = fs::file_size(ikvm::screenRecPath);

        screenRec.open(ikvm::screenRecPath,
                       std::ios::out | std::ios::binary | std::ios::app);

        log<level::INFO>("recording Started...");

        // Log the event of video recording start
        ikvm::eventLogSupport("OpenBMC.0.1.KVMAVRStart");

        // Fixed-period clock: guarantees exactly maxDuration×frameRate frames
        // written.
        auto frameInterval =
            std::chrono::microseconds(1000000 / video->getFrameRate());

        // Wait for first frame before starting clock so the full duration
        // budget isn't consumed while statusUpdateThread is busy in
        // sendFrame().
        std::vector<char> lastFrameData;
        while (videoRecFlag.load() && lastFrameData.empty())
        {
            {
                std::lock_guard<std::mutex> lk(video->recMutex);
                if (!video->recFrameQueue.empty())
                {
                    lastFrameData = std::move(video->recFrameQueue.front());
                    video->recFrameQueue.clear();
                }
            }
            if (lastFrameData.empty())
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (lastFrameData.empty())
        {
            // videoRecFlag was cleared before first frame arrived
            screenRec.close();
            Video::updateRecStat("Stop");
            recThreadStatus.store(false);
            video->clearRecQueue();
            return;
        }

        auto recStart = std::chrono::steady_clock::now();
        auto nextTick = recStart + frameInterval;

        // Write first frame immediately, then proceed with timed loop.
        outputSize += lastFrameData.size();
        screenRec.write(lastFrameData.data(), lastFrameData.size());

        while (videoRecFlag.load())
        {
            std::this_thread::sleep_until(nextTick);
            nextTick += frameInterval;

            if (std::chrono::steady_clock::now() - recStart >= recDuration)
            {
                videoRecFlag.store(false);
                log<level::INFO>("recording stopped...");
                // Log the event of video recording stop
                ikvm::eventLogSupport("OpenBMC.0.1.KVMAVRStop");
                break;
            }

            // Take latest frame for this tick; pad with last frame if none
            // arrived.
            {
                std::lock_guard<std::mutex> lk(video->recMutex);
                if (!video->recFrameQueue.empty())
                {
                    lastFrameData = std::move(video->recFrameQueue.back());
                    video->recFrameQueue.clear();
                }
            }

            outputSize += lastFrameData.size();

            if (outputSize > maxSize)
            {
                videoRecFlag.store(false);
                log<level::INFO>("recording stopped[MaxSize reached]...");
                break;
            }

            screenRec.write(lastFrameData.data(), lastFrameData.size());
        }
        screenRec.close();

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        /*
         * At this point it is safe to send record flag to false
         * interface
         *
         */
        if (!Video::updateRecStat("Stop"))
        {
            throw std::runtime_error("failed in D-bus call");
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        recThreadStatus.store(false);
        video->clearRecQueue();
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        // As the D bus failed not calling it during clean-up

        log<level::ERR>(" Exception caught in dbus call");
        log<level::ERR>("Error: ", entry("ERROR=%s", e.what()));

        videoRecFlag.store(false);
        recThreadStatus.store(false);
        video->clearRecQueue();

        return;
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Exception caught during video record");
        log<level::ERR>("Error : ", entry("ERROR=%s", e.what()));

        videoRecFlag.store(false);
        Video::updateRecStat("Stop");
        recThreadStatus.store(false);
        video->clearRecQueue();

        return;
    }

    log<level::DEBUG>(" Recording process completed successfully ");
}

bool Video::updateRecStat(std::string recType)
{
    bool status = false;

    try
    {
        auto bus = sdbusplus::bus::new_default_system();

        auto msg = bus.new_method_call(
            "xyz.openbmc_project.Kvm", "/xyz/openbmc_project/Kvm",
            "xyz.openbmc_project.Kvm.VideoRecord", "TriggerRecord");

        std::string result;
        msg.append(recType);
        auto reply = bus.call(msg);
        reply.read(result);

        if (result == "Success")
        {
            status = true;
        }
        else
        {
            throw std::runtime_error(result);
        }
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>(" Exception caught in dbus call");
        log<level::ERR>("Error: ", entry("ERROR=%s", e.what()));
        status = false;
        // return;
    }
    catch (const std::exception& e)
    {
        log<level::ERR>(" Exception caught in handling Host power state");
        log<level::ERR>("Error: ", entry("ERROR=%s", e.what()));
        status = false;
    }

    return status;
}

} // namespace ikvm
