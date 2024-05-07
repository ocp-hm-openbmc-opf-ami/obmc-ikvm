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
std::chrono::duration<uint64_t> timeoutValue = std::chrono::seconds(900);
const std::string smgrService = "xyz.openbmc_project.SessionManager";
const std::string smgrObjPath = "/xyz/openbmc_project/SessionManager";
const std::string smgrIface = "xyz.openbmc_project.SessionManager";
const std::string smgrKVMIface = "xyz.openbmc_project.SessionManager.Kvm";

const std::string serviceMgrKvmObjPath =
    "/xyz/openbmc_project/control/service/start_2dipkvm";
const std::string serviceMgrIface =
    "xyz.openbmc_project.Control.Service.Attributes";

std::vector<uint8_t> activeSessionIDs;
} // namespace ikvm
