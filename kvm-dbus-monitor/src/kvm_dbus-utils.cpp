/*
 * ****************************************************************************
 *
 * KVMDbus Utilities
 * Filename : kvm_dbus-utils.cpp
 *
 * @brief Implementation of various utilities
 *  used by different classes under kvmDbus namespace.
 *
 * Author: Amlana Bhuyan [amlanab@ami.com]
 *
 * ****************************************************************************
 */

#include "kvm_dbus-utils.hpp"

namespace kvmDbus
{

const std::string kvmJsonPath = "/etc/kvm-dbus-monitor.json";

const std::string ObjPath = "/xyz/openbmc_project/Kvm";
const std::string ServiceName = "xyz.openbmc_project.Kvm";
const std::string RecObjPath = "/xyz/openbmc_project/Kvm/VideoRecord";

const std::string scrnshotInterface = "xyz.openbmc_project.Kvm.Screenshot";
const std::string scrnRecInterface = "xyz.openbmc_project.Kvm.VideoRecord";

const std::string scrnRecTriggInterface =
    "xyz.openbmc_project.Kvm.VideoRecord.TriggerSettings";
const std::string scrnRecRmtStoreInterface =
    "xyz.openbmc_project.Kvm.VideoRecord.RemoteStorage";
const std::string scrnRecPreEvntInterface =
    "xyz.openbmc_project.Kvm.VideoRecord.PreEventRecording";

json jsonData = nullptr;

const std::string bsodObjPath =
    "/xyz/openbmc_project/sensors/os/OS_Stop_Status";
const std::string bsodObjPathNamespace = "/xyz/openbmc_project/sensors/os";
const std::string bsodInterface = "xyz.openbmc_project.Sensor.State";

const std::string tempSensorService = "xyz.openbmc_project.HwmonTempSensor";
const std::string tempObjPathNamespace =
    "/xyz/openbmc_project/sensors/temperature";

const std::string voltSensorService = "xyz.openbmc_project.ADCSensor";
const std::string voltObjPathNamespace = "/xyz/openbmc_project/sensors/voltage";

const std::string fanSensorService = "xyz.openbmc_project.FanSensor";
const std::string fanObjPathNamespace = "/xyz/openbmc_project/sensors/fan_tach";
const std::string fanStatusInterface = "xyz.openbmc_project.Sensor.FanStatus";

const std::string wdogService = "xyz.openbmc_project.Watchdog";
const std::string wdogObjPath = "/xyz/openbmc_project/watchdog/host0";
const std::string wdogInterface = "xyz.openbmc_project.Watchdog";

// Duration (in seconds) to suppress fan threshold alarms after a fan
// reconnect event, to avoid false AVR triggers during fan spin-up.
const uint32_t fanReconnectSuppressionSecs = 5;

const std::string critInterface =
    "xyz.openbmc_project.Sensor.Threshold.Critical";
const std::string nonCritInterface =
    "xyz.openbmc_project.Sensor.Threshold.Warning";
const std::string nonRecovInterface =
    "xyz.openbmc_project.Sensor.Threshold.NonRecoverable";

const std::string lpcpath = "/xyz/openbmc_project/state/boot/raw0";
const std::string lpcInterface = "xyz.openbmc_project.State.Boot.Raw";

const std::string hostStateObjpath = "/xyz/openbmc_project/state/host0";
const std::string hostStateInterface = "xyz.openbmc_project.State.Host";

const std::string chassisObjpath = "/xyz/openbmc_project/state/chassis0";
const std::string chassisInterface = "xyz.openbmc_project.State.Chassis";
/*
 * osState value 2 indicates Run-time Critical Stop/BSOD
 * Data Reference
 *  IPMI Spec
 *   -> Sensor Type Codes and Data
 *      -> Table Sensor Type Codes
 *         -> OS Stop / Shutdown
 *            -> 01h Run-time Critical Stop
 */
const uint16_t BSOD = 2;

/*
 * ==========================================================
 * <<<<<<<<<<<<<<<<<< UTILITY METHODS >>>>>>>>>>>>>>>>>>>>>>
 * ==========================================================
 */
int loadJson()
{
    try
    {
        std::ifstream f(kvmJsonPath);
        jsonData = json::parse(f);

        if (jsonData == NULL)
        {
            throw std::runtime_error("json data is empty ");
        }
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("caught Exception in loadjson()");
        log<level::ERR>("Error : ", entry("ERROR=%s", e.what()));

        return -1;
    }

    return 0;
}

int updateJson()
{
    if (std::ofstream ofs(kvmJsonPath); ofs.is_open())
    {
        ofs << jsonData.dump(4);
        ofs.close();
    }
    else
    {
        return -1;
    }

    return 0;
}

bool isMountedFromRemote(const std::string& folderPath)
{
    std::ifstream mountsFile("/proc/mounts");
    std::string line;
    if (!mountsFile.is_open())
    {
        std::cerr << "Could not open /proc/mounts" << std::endl;
        return false;
    }

    while (std::getline(mountsFile, line))
    {
        std::istringstream iss(line);
        std::string source, target, fstype;

        // Read each line in /proc/mounts: format is "source target fstype
        // options dump pass"
        if (iss >> source >> target >> fstype)
        {
            // Check if the target matches the specified folder and if it is an
            // NFS or CIFS mount
            if (target == folderPath && (fstype == "nfs" || fstype == "cifs"))
            {
                return true;
            }
        }
    }

    return false;
}

} // namespace kvmDbus
