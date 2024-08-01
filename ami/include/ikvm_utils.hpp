#pragma once
/*
 * ****************************************************************************
 *
 * KVM Utilities
 * Filename : ikvm_utils.hpp
 *
 * @brief Implementation of various utilities
 *  used by different classes under ikvm namespace.
 *
 * Author: Amlana Bhuyan [amlanab@ami.com]
 *
 * ****************************************************************************
 */
#include <string.h>

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

#include <atomic>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <variant>

namespace ikvm
{
namespace fs = std::filesystem;
using namespace phosphor::logging;

extern const char* DBUS_PROPERTIES_INTERFACE;
/*@brief common objectpath for kvm dbus interface */
extern const std::string kvmObjPath;
/*@brief  well-known name for kvm service */
extern const std::string kvmServiceName;
/*@brief screenshot interface name */
extern const std::string scrnshotInterface;

/*@brief required parameter for BSOD monitor */
extern const std::string bsodObjPath;
extern const std::string bsodTarget;
extern const std::string bsodSennType;

/*@brief Screenshot flag for capturing screenshot*/
extern std::atomic<bool> scrnshotFlag;

/*@brief Screenshot store paths*/
extern const std::string bsodAsJpeg;
extern const std::string bsodDir;

/*@brief pointer to Screenshot interface */
extern std::shared_ptr<sdbusplus::asio::dbus_interface> kvmScrnshotIface;

/*@brief set the time duration for session timeout*/
extern std::chrono::duration<uint64_t> timeoutValue;
#define DEFAULT_TIMEOUT_VALUE 86401

/*@brief session manager DBus- details*/
extern const std::string smgrService;
extern const std::string smgrObjPath;
extern const std::string smgrIface;
extern const std::string smgrKVMIface;

/*@brief service manager DBus- details*/
extern const std::string serviceMgrService;
extern const std::string serviceMgrKvmObjPath;
extern const std::string serviceMgrIface;

using sessionInfo = std::tuple<uint8_t, std::string, std::string, uint8_t,
                               uint8_t, uint8_t, std::string>;
using sessionRet = std::vector<sessionInfo>;
using propertyValue = std::variant<sessionRet>;

extern std::vector<uint8_t> activeSessionIDs;

/*@brief Host Power status D-Bus details*/
extern const std::string pwrStatService;
extern const std::string pwrStatObjPath;
extern const std::string pwrStatIface;
/* @brief Holds the Host Power status
 *
 * @param[value] "Off": The host is powered off
 * @param[value] "On": The host is powered on
 * @param[value] "Unknown": The host power state is unknown
 */
extern std::string hostPowerState;

/*@brief NO SIGNAL image stored Path */
extern const char* NO_SIGNAL_IMG_PATH;
/*@brief POWER OFF image stored Path */
extern const char* POWER_OFF_IMG_PATH;

/*
 * ==========================================================
 * <<<<<<<<<<<<<<<<<< UTILITY METHODS >>>>>>>>>>>>>>>>>>>>>>
 * ==========================================================
 */

/* @brief Wrapper method for essential utility methods */
void createUtilities();
/*
 * @brief Create  directory in Persistent memory of BMC.
 * @param[in] path - path to new directory
 */
bool isDir(const std::string& path);
/*
 * @brief Gets the initial value of hostPowerState from external service.
 */
void powerStatusInit();

/*
 * @brief Gets the updated session timeout value from external service..
 */
void sessionTimeout();

} // namespace ikvm
