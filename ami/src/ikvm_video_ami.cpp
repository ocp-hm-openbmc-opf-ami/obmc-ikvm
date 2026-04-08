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

    auto frameRate = video->getFrameRate();
    auto delay = (1000000 / frameRate) - 100;
    size_t outputSize = 0;
    size_t maxSize = 7 * 1024 * 1024;
    int count = 0;
    int loopcount = 0;
    auto recDuration = std::chrono::seconds(10);
    auto recStart = std::chrono::steady_clock::now();

    // Load No Signal Image into buffer
    size_t noSignalImageSize = 0;
    std::vector<char> noSignalImageBuffer;

    std::ifstream noSignalImage(NO_SIGNAL_IMG_PATH,
                                std::ios::binary | std::ios::ate);
    if (noSignalImage)
    {
        noSignalImageSize = static_cast<size_t>(noSignalImage.tellg());
        if (noSignalImageSize > 0)
        {
            noSignalImageBuffer.resize(noSignalImageSize);
            noSignalImage.seekg(0, std::ios::beg);
            if (!noSignalImage.read(
                    noSignalImageBuffer.data(),
                    static_cast<std::streamsize>(noSignalImageSize)))
            {
                noSignalImageSize = 0;
                noSignalImageBuffer.clear();
                noSignalImageBuffer.shrink_to_fit();
                log<level::DEBUG>(
                    "Failed to read No Signal image file. Proceeding without No Signal image");
            }
        }
    }
    if (ikvm::recordToRemote)
    {
        recDuration = std::chrono::seconds(ikvm::maxDuration);
        maxSize = (ikvm::maxSize) * 1024 * 1024;
    }

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

        recStart = std::chrono::steady_clock::now();
        while (videoRecFlag.load())
        {
            if (std::chrono::steady_clock::now() - recStart <= recDuration)
            {
                loopcount++;
                char* data = nullptr;
                size_t size = 0;

                if (!(video->buffersDone.empty() ||
                      video->buffersDone.front() < 0))
                {
                    auto i = video->buffersDone.front();
                    data = video->getData(i);
                    size = video->getFrameSize(i);
                }

                if (!data)
                {
                    if (!noSignalImageBuffer.empty() && noSignalImageSize > 0)
                    {
                        data = noSignalImageBuffer.data();
                        size = noSignalImageSize;
                    }
                    else
                    {
                        continue;
                    }
                }

                count++;
                outputSize += size;
                if (outputSize > maxSize)
                {
                    videoRecFlag.store(false);
                    log<level::INFO>("recording stopped[MaxSize reached]...");
                    continue;
                }

                screenRec.write(data, size);
                log<level::DEBUG>("Host screen Record in progress...");
                std::this_thread::sleep_for(std::chrono::microseconds(delay));
            }
            else
            {
                videoRecFlag.store(false);
                log<level::INFO>("recording stopped...");
                // Log the event of video recording stop
                ikvm::eventLogSupport("OpenBMC.0.1.KVMAVRStop");
            }
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
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        // As the D bus failed not calling it during clean-up

        log<level::ERR>(" Exception caught in dbus call");
        log<level::ERR>("Error: ", entry("ERROR=%s", e.what()));

        videoRecFlag.store(false);
        recThreadStatus.store(false);

        return;
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Exception caught during video record");
        log<level::ERR>("Error : ", entry("ERROR=%s", e.what()));

        videoRecFlag.store(false);
        Video::updateRecStat("Stop");
        recThreadStatus.store(false);

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
