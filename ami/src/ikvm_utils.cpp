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
std::atomic<bool> InitFlag{true};

const std::string kvmObjPath = "/xyz/openbmc_project/Kvm";
const std::string kvmServiceName = "xyz.openbmc_project.Kvm";
const std::string videoRecObjPath = "/xyz/openbmc_project/Kvm/VideoRecord";

const std::string scrnshotInterface = "xyz.openbmc_project.Kvm.Screenshot";
const std::string videoRecInterface = "xyz.openbmc_project.Kvm.VideoRecord";
const std::string remoteStorageInterface =
    "xyz.openbmc_project.Kvm.VideoRecord";

const std::string scrnRecTriggInterface =
    "xyz.openbmc_project.Kvm.VideoRecord.TriggerSettings";
const std::string scrnRecRmtStoreInterface =
    "xyz.openbmc_project.Kvm.VideoRecord.RemoteStorage";
const std::string scrnRecPreEvntInterface =
    "xyz.openbmc_project.Kvm.VideoRecord.PreEventRecording";

const std::string bsodObjPath = "/xyz/openbmc_project/sensors/os/";
const std::string bsodTarget = "/xyz/openbmc_project/sensors/os";
const std::string bsodSennType = "xyz.openbmc_project.Sensor.State";

const std::string bsodAsJpeg = "/etc/bsod/screenShotBSOD.jpeg";
const std::string bsodDir = "/etc/bsod";

std::shared_ptr<sdbusplus::asio::dbus_interface> kvmScrnshotIface = nullptr;
std::shared_ptr<sdbusplus::asio::dbus_interface> kvmScrnRecIface = nullptr;

std::chrono::duration<uint64_t> timeoutValue =
    std::chrono::seconds(DEFAULT_TIMEOUT_VALUE);
const std::string smgrService = "xyz.openbmc_project.SessionManager";
const std::string smgrObjPath = "/xyz/openbmc_project/SessionManager";
const std::string smgrIface = "xyz.openbmc_project.SessionManager";
const std::string smgrKVMIface = "xyz.openbmc_project.SessionManager.Kvm";

const std::string serviceMgrService =
    "xyz.openbmc_project.Control.Service.Manager";
const std::string serviceMgrKvmObjPath =
    "/xyz/openbmc_project/control/service/start_2dipkvm";
const std::string serviceMgrIface =
    "xyz.openbmc_project.Control.Service.Attributes";

std::vector<uint8_t> activeSessionIDs;

const std::string pwrStatService = "xyz.openbmc_project.State.Chassis";
const std::string pwrStatObjPath = "/xyz/openbmc_project/state/chassis0";
const std::string pwrStatIface = "xyz.openbmc_project.State.Chassis";
std::string hostPowerState = "Unknown";

bool isKvmDisabled = false;

const char* NO_SIGNAL_IMG_PATH = "/etc/NO_SIGNAL.jpg";
const char* POWER_OFF_IMG_PATH = "/etc/POWER_OFF.jpg";

std::atomic<bool> videoRecFlag{false};
std::atomic<bool> recThreadStatus{false};
const std::string recProcessDir = "/tmp/video";
const std::string screenRecPath = "/tmp/video/video.dat";

uint8_t maxDumps = 1;
uint8_t maxDuration = 1;
uint8_t maxSize = 1;
std::string serverIP = "";
std::string pathInServer = "";
std::string shareType = "nfs";
std::string options = "";
bool recordToRemote = false;
bool active = false;
/*
 * ===============================================================
 *  <<<<<<<<<<<<<<< UTILITY METHOD DEFINATIONS >>>>>>>>>>>>>>>>>>
 * ===============================================================
 */
void createUtilities()
{
    isDir(bsodDir);
    isDir(recProcessDir);
    powerStatusInit();
    sessionTimeout();
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

void sessionTimeout()
{
    try
    {
        auto busSessTimoutValue = sdbusplus::bus::new_default_system();
        auto msgSessTimoutValue = busSessTimoutValue.new_method_call(
            serviceMgrService.c_str(), serviceMgrKvmObjPath.c_str(),
            DBUS_PROPERTIES_INTERFACE, "Get");
        msgSessTimoutValue.append(serviceMgrIface.c_str(), "SessionTimeOut");

        auto reply = busSessTimoutValue.call(msgSessTimoutValue);

        if (reply.is_method_error())
        {
            log<level::ERR>("D-Bus method call error.");
            return;
        }

        // Extract the value from the reply
        std::variant<uint64_t> sessionTimeoutValue;
        reply.read(sessionTimeoutValue);

        uint64_t timeoutSeconds = std::get<uint64_t>(sessionTimeoutValue);
        timeoutValue = std::chrono::seconds(timeoutSeconds);
    }

    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>("D-Bus call Failed", entry("ERROR=%s", e.what()));
        return;
    }

    catch (const std::exception& e)
    {
        log<level::ERR>("Error handling for session timeout",
                        entry("ERROR=%s", e.what()));
        return;
    }
}

void getRemoteConf()
{
    try
    {
        auto busRemotConf = sdbusplus::bus::new_default_system();
        auto msgRemoteConf = busRemotConf.new_method_call(
            kvmServiceName.c_str(), videoRecObjPath.c_str(),
            DBUS_PROPERTIES_INTERFACE, "GetAll");

        msgRemoteConf.append(scrnRecRmtStoreInterface.c_str());

        auto reply = busRemotConf.call(msgRemoteConf);

        if (reply.is_method_error())
        {
            log<level::ERR>("D-Bus method call error.");
            return;
        }

        // Parse the returned dictionary of properties yyysssv
        std::map<std::string, std::variant<uint8_t, std::string, bool>>
            properties;
        reply.read(properties);

        // Retrive required properties value
        for (const auto& [key, value] : properties)
        {
            if (key == "Active")
            {
                if (auto v = std::get_if<bool>(&value))
                {
                    ikvm::active = *v;
                }
            }
            else if (key == "MaxDumps")
            {
                if (auto v = std::get_if<uint8_t>(&value))
                {
                    ikvm::maxDumps = *v;
                }
            }
            else if (key == "MaxDuration")
            {
                if (auto v = std::get_if<uint8_t>(&value))
                {
                    ikvm::maxDuration = *v;
                }
            }
            else if (key == "MaxSize")
            {
                if (auto v = std::get_if<uint8_t>(&value))
                {
                    ikvm::maxSize = *v;
                }
            }
            else if (key == "PathInServer")
            {
                if (auto v = std::get_if<std::string>(&value))
                {
                    ikvm::pathInServer = *v;
                }
            }
            else if (key == "RecordToRemote")
            {
                if (auto v = std::get_if<bool>(&value))
                {
                    ikvm::recordToRemote = *v;
                }
            }
            else if (key == "ServerIP")
            {
                if (auto v = std::get_if<std::string>(&value))
                {
                    ikvm::serverIP = *v;
                }
            }
            else if (key == "ShareType")
            {
                if (auto v = std::get_if<std::string>(&value))
                {
                    ikvm::shareType = *v;
                }
            }
            else
            {
                log<level::DEBUG>("Unknown data...");
            }
        }
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>(" D-Bus call Failed", entry("ERROR=%s", e.what()));
        maxDuration = 1;
        maxSize = 1;
        serverIP = "";
        pathInServer = "";
        shareType = "";
        recordToRemote = false;
        active = false;
        return;
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Error in fetching video remote Storage config.",
                        entry("ERROR=%s", e.what()));
        return;
    }
}

} // namespace ikvm
