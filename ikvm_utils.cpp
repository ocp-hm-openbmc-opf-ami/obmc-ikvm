/*
 * ****************************************************************
 *
 * KVM Utilities
 * Filename : ikvm_utils.cpp
 *
 * @brief Implementation of various utilities
 *  used by different classes under ikvm namespace.
 *
 * Author: Amlana Bhuyan [amlanab@ami.com]
 *
 * *****************************************************************
 */
#include "ikvm_utils.hpp"

namespace ikvm
{
std::atomic<bool> scrnshotFlag{false};

const std::string kvmObjPath = "/xyz/openbmc_project/Kvm";
const std::string kvmServiceName = "xyz.openbmc_project.Kvm";

const std::string scrnshotInterface = "xyz.openbmc_project.Kvm.Screenshot";

const std::string bsodObjPath = "/xyz/openbmc_project/sensors/os/";
const std::string bsodTarget = "/xyz/openbmc_project/sensors/os";
const std::string bsodSennType = "xyz.openbmc_project.Sensor.State";

const std::string bsodAsJpeg = "/etc/bsod/screenShotBSOD.jpeg";
const std::string bsodDir = "/etc/bsod";

std::shared_ptr<sdbusplus::asio::dbus_interface> kvmScrnshotIface = nullptr;
std::chrono::duration<int64_t> timeoutValue = std::chrono::seconds(1800);

} // namespace ikvm
