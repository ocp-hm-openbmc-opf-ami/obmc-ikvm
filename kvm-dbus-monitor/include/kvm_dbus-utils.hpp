#pragma once
/*
 * ****************************************************************************
 *
 * KVM Utilities
 * Filename : kvm_dbus-utils.hpp
 *
 * @brief Implementation of various utilities used by
 * different classes under kvmDbus namespace.
 *
 * Author: Amlana Bhuyan [amlanab@ami.com]
 *
 * ****************************************************************************
 */

#pragma once

#include <string.h>

#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>
#include <nlohmann/json.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/message.hpp>
#include <sdbusplus/server/interface.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>

#include <bitset>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

namespace kvmDbus
{
using namespace phosphor::logging;
using json = nlohmann::json;
namespace fs = std::filesystem;
using namespace phosphor::logging;
using triggerEvents32 = std::bitset<32>;
using credentialVariant = std::variant<int32_t, sdbusplus::message::unix_fd>;

constexpr const size_t secretLimit = 1024;

enum triggerEvent
{
    criticalTmpVolt = 0,
    nonCriticalTmpVolt = 1,
    nonRecovTmpVolt = 2,
    fanstatechanged = 3,
    watchdogTimer = 4,
    chassisPowerOn = 5,
    chassisPowerOff = 6,
    chassisReset = 7,
    lPCReset = 8,
    dateandTime = 9,
    preEventVideoRec = 10,
    // Add more options as needed in sequence
    RESERVED, // this should always be the last value
};

extern const std::string kvmJsonPath;

extern const std::string ObjPath;
extern const std::string ServiceName;
extern const std::string RecObjPath;

extern const std::string scrnshotInterface;
extern const std::string scrnRecInterface;

extern const std::string scrnRecTriggInterface;
extern const std::string scrnRecRmtStoreInterface;
extern const std::string scrnRecPreEvntInterface;

/*External Services for Monitoring */
extern const std::string bsodObjPath;
extern const std::string bsodObjPathNamespace;
extern const std::string bsodInterface;

extern const std::string tempSensorService;
extern const std::string tempObjPathNamespace;

extern const std::string voltSensorService;
extern const std::string voltObjPathNamespace;

extern const std::string critInterface;
extern const std::string nonCritInterface;

extern const std::string lpcpath;
extern const std::string lpcInterface;

/*
 * osState value 2 indicates Run-time Critical Stop/BSOD
 * Data Reference
 *  IPMI Spec
 *   -> Sensor Type Codes and Data
 *      -> Table Sensor Type Codes
 *         -> OS Stop / Shutdown
 *            -> 01h Run-time Critical Stop
 */
extern const uint16_t BSOD;

extern json jsonData;
/*
 * ==========================================================
 * <<<<<<<<<<<<<<<<<< UTILITY METHODS >>>>>>>>>>>>>>>>>>>>>>
 * ==========================================================
 */

int updateJson();
int loadJson();
bool isMountedFromRemote(const std::string& folderPath);

} // namespace kvmDbus
