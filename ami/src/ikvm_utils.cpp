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

#include <chrono>
#include <thread>

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
const std::string smgrKVMObjPath = "/xyz/openbmc_project/SessionManager/kvm";
const std::string smgrWEBObjPath = "/xyz/openbmc_project/SessionManager/web";
const std::string smgrKVMIface =
    "xyz.openbmc_project.SessionManager.KvmSessionInfo";
const std::string smgrWebIface =
    "xyz.openbmc_project.SessionManager.WebSessionInfo";

const std::string serviceMgrService =
    "xyz.openbmc_project.Control.Service.Manager";
const std::string serviceMgrKvmObjPath =
    "/xyz/openbmc_project/control/service/start_2dipkvm";
const std::string serviceMgrIface =
    "xyz.openbmc_project.Control.Service.Attributes";

std::vector<uint8_t> activeSessionIDs;

std::string pwrStatService = "xyz.openbmc_project.State.Chassis";
std::string pwrStatObjPath = "/xyz/openbmc_project/state/chassis0";
const std::string pwrStatIface = "xyz.openbmc_project.State.Chassis";
std::string hostPowerState = "Unknown";

std::string videoDevicePath = "/dev/video0"; // Default to video0
uint8_t kvmInstanceId = 0; // 0 for kvm (video0), 1 for kvm1 (video1)

const std::string eventLogService = "xyz.openbmc_project.Logging";
const std::string eventLogObjPath = "/xyz/openbmc_project/logging";
const std::string eventLogIface = "xyz.openbmc_project.Logging.Create";
const std::string eventlogServerity =
    "xyz.openbmc_project.Logging.Entry.Level.Informational";

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

bool isAst2700Platform = false;

/*
 * ===============================================================
 *  <<<<<<<<<<<<<<< UTILITY METHOD DEFINATIONS >>>>>>>>>>>>>>>>>>
 * ===============================================================
 */

void detectKvmInstance(const std::string& videoPath)
{
    videoDevicePath = videoPath;

#ifdef MULTI_HOST_DEFAULT_MODE
    // Dual-node: try chassis1/chassis2, fallback to chassis0
    if (videoPath == "/dev/video1")
    {
        kvmInstanceId = 1;
        pwrStatService = "xyz.openbmc_project.State.Chassis2";
        pwrStatObjPath = "/xyz/openbmc_project/state/chassis2";
    }
    else
    {
        kvmInstanceId = 0;
        pwrStatService = "xyz.openbmc_project.State.Chassis1";
        pwrStatObjPath = "/xyz/openbmc_project/state/chassis1";
    }
#else
    // Single-node
    kvmInstanceId = 0;
    pwrStatService = "xyz.openbmc_project.State.Chassis";
    pwrStatObjPath = "/xyz/openbmc_project/state/chassis0";
#endif
}

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
    // Retry up to 5 times with 1 second delays to wait for chassis service
    // to start
    const int maxRetries = 5;
#ifdef MULTI_HOST_DEFAULT_MODE
    bool fallbackAttempted = false;
#endif

    for (int retry = 0; retry < maxRetries; retry++)
    {
        if (retry > 0)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        try
        {
            auto busPowerStat = sdbusplus::bus::new_default_system();
            auto msgPowerStat = busPowerStat.new_method_call(
                pwrStatService.c_str(), pwrStatObjPath.c_str(),
                DBUS_PROPERTIES_INTERFACE, "Get");
            msgPowerStat.append(pwrStatIface.c_str(), "CurrentPowerState");

            auto reply = busPowerStat.call(msgPowerStat);
            std::variant<std::string> powerStr;
            reply.read(powerStr);

            if (auto pws = std::get_if<std::string>(&powerStr))
            {
                if (pws->find("Off") != std::string::npos)
                    hostPowerState = "Off";
                else if (pws->find("On") != std::string::npos)
                    hostPowerState = "On";
                else
                    hostPowerState = "Unknown";

                log<level::DEBUG>("Power state initialized",
                                  entry("STATE=%s", hostPowerState.c_str()),
                                  entry("PATH=%s", pwrStatObjPath.c_str()));
                return; // Successfully got power state
            }
        }
        catch (const sdbusplus::exception::SdBusError& e)
        {
#ifdef MULTI_HOST_DEFAULT_MODE
            // On first failure, try chassis0 fallback once
            if (!fallbackAttempted &&
                (pwrStatObjPath == "/xyz/openbmc_project/state/chassis1" ||
                 pwrStatObjPath == "/xyz/openbmc_project/state/chassis2"))
            {
                pwrStatService = "xyz.openbmc_project.State.Chassis";
                pwrStatObjPath = "/xyz/openbmc_project/state/chassis0";
                fallbackAttempted = true;
                retry = -1; // Reset retry counter for chassis0
                continue;
            }
#endif
        }
    }

    // Failed to get power state after all retries
    hostPowerState = "Unknown";
    log<level::ERR>("Failed to get power state after retries");
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

uint8_t extractSessionId(const std::string& infoStr)
{
    try
    {
        static const std::regex sessionPattern(R"(session_(\d+))");
        std::smatch match;

        if (!std::regex_search(infoStr, match, sessionPattern))
        {
            throw std::invalid_argument(
                "Expected format 'session_N' not found.");
        }

        int sessionId = std::stoi(match[1].str());
        if (sessionId < 0 || sessionId > 255)
            throw std::out_of_range("Session ID must be in range 0-255.");

        return static_cast<uint8_t>(sessionId);
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("extractSessionId exception",
                        entry("ERROR=%s", e.what()));
        return 0; // Return default session ID on error
    }
}

inline std::string trim(const std::string& s)
{
    auto start = s.find_first_not_of(" \t");
    auto end = s.find_last_not_of(" \t");
    return (start == std::string::npos) ? "" : s.substr(start, end - start + 1);
}

// Parse "key=value ,key2=value2" into a map
std::map<std::string, std::string> parseKeyValueString(const std::string& input)
{
    std::map<std::string, std::string> result;
    std::istringstream ss(input);
    std::string pair;
    while (std::getline(ss, pair, ','))
    {
        auto eq = pair.find('=');
        if (eq != std::string::npos)
        {
            std::string key = trim(pair.substr(0, eq));
            std::string value = trim(pair.substr(eq + 1));
            result[key] = value;
        }
    }
    return result;
}

void eventLogSupport(const std::string& msg)
{
    try
    {
        auto bus = sdbusplus::bus::new_default_system();
        sdbusplus::message::message m = bus.new_method_call(
            eventLogService.c_str(), eventLogObjPath.c_str(),
            eventLogIface.c_str(), "Create");
        m.append(msg, eventlogServerity.c_str(),
                 std::map<std::string, std::string>());
        bus.call(m);
    }
    catch (const sdbusplus::exception::SdBusError& e)
    {
        log<level::ERR>("D-Bus call Failed", entry("ERROR=%s", e.what()));
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Error in Event Log support",
                        entry("ERROR=%s", e.what()));
    }
}

} // namespace ikvm
