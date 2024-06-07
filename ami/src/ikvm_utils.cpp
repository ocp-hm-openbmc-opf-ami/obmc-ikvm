/*
 * ****************************************************************************
 *
 * KVM Utilities
 * Filename : ikvm_utils.cpp
 *
 * @brief Implementation of various utilities
 *  used by different classes under ikvm namespace.
 *
 * Author: Amlana Bhuyan [amlanab@ami.com]
 *
 * ****************************************************************************
 */
#include "ami/include/ikvm_utils.hpp"

namespace ikvm
{
const char* DBUS_PROPERTIES_INTERFACE = "org.freedesktop.DBus.Properties";

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

const std::string pwrStatService = "xyz.openbmc_project.State.Chassis";
const std::string pwrStatObjPath = "/xyz/openbmc_project/state/chassis0";
const std::string pwrStatIface = "xyz.openbmc_project.State.Chassis";
std::string hostPowerState = "Unknown";

const char* NO_SIGNAL_IMG_PATH = "/etc/NO_SIGNAL.jpg";
const char* POWER_OFF_IMG_PATH = "/etc/POWER_OFF.jpg";

/*
 * ===============================================================
 *  <<<<<<<<<<<<<<< UTILITY METHOD DEFINATIONS >>>>>>>>>>>>>>>>>>
 * ===============================================================
 */
void createUtilities()
{
    isDir(bsodDir);
    powerStatusInit();
}

bool isDir(const std::string& path)
{
    try
    {
        if (!(fs::is_directory(path.c_str())))
        {
            fs::create_directory(path.c_str());
        }
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Failed to create folder", entry("ERROR=%s", e.what()));
        return false;
    }
    return true;
}

void powerStatusInit()
{
    try
    {
        auto busPowerStat = sdbusplus::bus::new_default_system();
        auto msgPowerStat = busPowerStat.new_method_call(
            pwrStatService.c_str(), pwrStatObjPath.c_str(),
            DBUS_PROPERTIES_INTERFACE, "Get");

        msgPowerStat.append(pwrStatIface.c_str(), "CurrentPowerState");

        auto reply = busPowerStat.call(msgPowerStat);

        if (reply.is_method_error())
        {
            log<level::ERR>("D-Bus method call error.");
            hostPowerState = "Unknown";
            return;
        }

        std::variant<std::string> powerStr;
        reply.read(powerStr);

        // Initialize hostPowerState
        if (auto pws = std::get_if<std::string>(&powerStr))
        {
            if (pws->find("Off") != std::string::npos)
            {
                hostPowerState = "Off";
            }
            else if (pws->find("On") != std::string::npos)
            {
                hostPowerState = "On";
            }
            else
            {
                hostPowerState = "Unknown";
                log<level::ERR>("Unexpected power state");
            }
        }
        else
        {
            hostPowerState = "Unknown";
            log<level::ERR>("Unexpected variant type for power state.");
        }

        log<level::INFO>("[updated]",
                         entry("hostPowerState: %s ", hostPowerState.c_str()));
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>(" D-Bus call Failed", entry("ERROR=%s", e.what()));
        hostPowerState = "Unknown";
        return;
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Error handling for Host power state ",
                        entry("ERROR=%s", e.what()));
        hostPowerState = "Unknown";
        return;
    }
}

} // namespace ikvm
