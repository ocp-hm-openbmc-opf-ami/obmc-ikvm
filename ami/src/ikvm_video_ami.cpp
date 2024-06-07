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
    setFormat(newformat);
    start();
}

void Video::screenShot(const std::string& screenShotPath)
{
    if (buffersDone.front() < 0)
    {
        log<level::ERR>("Buffer front empty");
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
} // namespace ikvm
