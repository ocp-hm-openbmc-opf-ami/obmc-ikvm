/*
 * ****************************************************************************
 *
 * AMI Extension of Server Class
 * Filename : ikvm_server_ami.cpp
 *
 * @brief AMI Extension of Server Class to support various Feature
 * Implementation.
 *
 *
 * Author: Amlana Bhuyan [amlanab@ami.com]
 *
 * ****************************************************************************
 */

#include "ami/include/ikvm_utils.hpp"
#include "ikvm_server.hpp"

namespace ikvm
{
void Server::updatePowerSaveMode(int status)
{
    if ((status == 0) || (status == 1))
    {
        auto bus = sdbusplus::bus::new_system();
        auto methodCall = bus.new_method_call(
            "xyz.openbmc_project.Settings",
            "/xyz/openbmc_project/logging/settings", "xyz.openbmc_project.USB",
            "SetUSBPowerSaveMode");
        methodCall.append(status);
        bus.call(methodCall);
    }
}

} // namespace ikvm
