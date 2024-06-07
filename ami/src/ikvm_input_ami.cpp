/*
 * ****************************************************************************
 *
 * AMI Extension of Input Class
 * Filename : ikvm_input_ami.cpp
 *
 * @brief AMI Extension of Input Class to support various Feature
 * Implementation.
 *
 *
 * Author: Amlana Bhuyan [amlanab@ami.com]
 *
 * ****************************************************************************
 */
#include "ami/include/ikvm_utils.hpp"
#include "ikvm_input.hpp"

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>

namespace ikvm
{
using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::File::Error;

int Input::readKeyBoardOutReport()
{
    char buf[KEY_REPORT_LENGTH] = {0};
    int usbfd = -1;
    int ret = 0;
    int cmd_len = -1;

    fd_set rfds;
    usbfd = open(keyboardPath.c_str(), O_RDWR | O_CLOEXEC | O_NONBLOCK);
    if (usbfd < 0)
    {
        log<level::ERR>("Failed to open input device",
                        entry("PATH=%s", keyboardPath.c_str()),
                        entry("ERROR=%s", strerror(errno)));
        elog<Open>(xyz::openbmc_project::Common::File::Open::ERRNO(errno),
                   xyz::openbmc_project::Common::File::Open::PATH(
                       keyboardPath.c_str()));
        return -errno;
    }

    FD_ZERO(&rfds);
    FD_SET(STDIN_FILENO, &rfds);
    FD_SET(usbfd, &rfds);
    ret = select(usbfd + 1, &rfds, NULL, NULL, NULL);

    if (ret < 0)
    {
        log<level::ERR>("select() : ", entry("ERROR=%s", strerror(errno)));
        return -errno;
    }
    if (FD_ISSET(usbfd, &rfds))
    {
        cmd_len = read(usbfd, buf, KEY_REPORT_LENGTH - 1);
        if (cmd_len < 0)
        {
            log<level::ERR>("Failed to read keyboard report",
                            entry("ERROR=%s", strerror(errno)));
            return -errno;
        }

        keyboardLedState.Byte = static_cast<uint8_t>(buf[0]);
    }
    close(usbfd);
    log<level::DEBUG>(
        "\n === keyboardLedState === \n",
        entry("keyboardLedState:%2x", keyboardLedState.Byte),
        entry("NumLock : %s  ",
              (std::string{(keyboardLedState.NumLock ? "ON" : "OFF")}).c_str()),
        entry(
            "CapsLock : %s  ",
            (std::string{(keyboardLedState.CapsLock ? "ON" : "OFF")}).c_str()),
        entry("ScrollLock : %s\n",
              (std::string{(keyboardLedState.ScrollLock ? "ON" : "OFF")})
                  .c_str()));

    return cmd_len;
}

int Input::getkeyboardLedState()
{
    return keyboardLedState.Byte;
}

} // namespace ikvm
