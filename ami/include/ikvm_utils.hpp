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
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <regex>
#include <stdexcept>
#include <variant>
#include <sstream>
#include <algorithm>

/* @brief Implementation of IVTP extension
 *
 * Extension to the Remote Framebuffer (RFB) protocol by incorporating AMI’s
 * Intelligent Video Transfer Protocol (IVTP) to enhance communication
 * capabilities between the KVM server and clients (H5Viewer & JViewer
 *
 * Refer MegaRAC OneTree™ - RFB Extension - Intelligent Video Transfer Protocol
 * Design document for additional information
 */

/* Define macros for handling IVTP */
#define SERVER_CUT_TEXT 3
#define CLIENT_CUT_TEXT 6

constexpr uint32_t IVTP_HEADER_SIZE = 4;
constexpr uint32_t IVTP_NUMBER_SIZE = 2;
constexpr uint32_t IVTP_PAYLOAD_LENGTH_HOLDER_SIZE = 4;
constexpr uint32_t IVTP_STATUS_SIZE = 2;
constexpr uint32_t IVTP_MIN_SIZE = 12;
constexpr uint32_t IVTP_MAX_WAIT_CYCLES = 100;

#define IVTP_STOP_SESSION_IMMEDIATE 0x0008

/* Define macros for sending the error code as status */
#define STOP_SESSION_TIMED_OUT 0x0009
#define STOP_SESSION_IMMEDIATE 0x0002

const short IVTP_VALIDATE_VIDEO_SESSION = 0x0012;
const short IVTP_GET_WEB_TOKEN = 0x0015;
const short IVTP_SESSION_ACCEPTED = 0x0017;

struct IVTPMessage
{
    std::string header = "";    // 4-byte header
    uint16_t num = 0;           // Message number/type
    uint32_t payloadLength = 0; // Length of payload
    uint16_t status = 0;        // Status code
    std::string payload = "";   // Payload data
    bool valid = false;         // set true if the message is valid

    // Default constructor, added for completeness
    IVTPMessage() = default;
};

namespace ikvm
{
namespace fs = std::filesystem;
using namespace phosphor::logging;

extern const char* DBUS_PROPERTIES_INTERFACE;
/*@brief common objectpath for kvm dbus interface */
extern const std::string kvmObjPath;
extern const std::string videoRecObjPath;
/*@brief  well-known name for kvm service */
extern const std::string kvmServiceName;
/*@brief screenshot interface name */
extern const std::string scrnshotInterface;

/*@brief KVM Dbus Details */
extern const std::string videoRecInterface;
extern const std::string remoteStorageInterface;
extern const std::string prEventInterface;

/*@brief required parameter for BSOD monitor */
extern const std::string bsodObjPath;
extern const std::string bsodTarget;
extern const std::string bsodSennType;

/*@brief Screenshot flag for capturing screenshot*/
extern std::atomic<bool> scrnshotFlag;

/*@brief flag for Initiating the Server*/
extern std::atomic<bool> InitFlag;

/*@brief Screenshot store paths*/
extern const std::string bsodAsJpeg;
extern const std::string bsodDir;

/*@brief pointer to Screenshot interface */
extern std::shared_ptr<sdbusplus::asio::dbus_interface> kvmScrnshotIface;
/*@brief pointer to Video Record interface */
extern std::shared_ptr<sdbusplus::asio::dbus_interface> kvmScrnRecIface;

/*@brief set the time duration for session timeout*/
extern std::chrono::duration<uint64_t> timeoutValue;
#define DEFAULT_TIMEOUT_VALUE 86401

/*@brief session manager DBus- details*/
extern const std::string smgrService;
extern const std::string smgrObjPath;
extern const std::string smgrIface;
extern const std::string smgrKVMIface;
extern const std::string smgrWebIface;

/*@brief service manager DBus- details*/
extern const std::string serviceMgrService;
extern const std::string serviceMgrKvmObjPath;
extern const std::string serviceMgrIface;

using sessionInfo = std::tuple<uint8_t, std::string, std::string, uint8_t,
                               uint8_t, uint8_t, std::string>;
using sessionRet = std::vector<sessionInfo>;
using propertyValue = std::variant<sessionRet>;

using PropertyValue =
    std::variant<int, uint8_t, int16_t, int32_t, int64_t, uint16_t, uint32_t,
                 uint64_t, double, std::string, bool>;

using credentialVariant = std::variant<int32_t, sdbusplus::message::unix_fd>;

extern std::vector<uint8_t> activeSessionIDs;

/*@brief Host Power status D-Bus details*/
extern const std::string pwrStatService;
extern const std::string pwrStatObjPath;
extern const std::string pwrStatIface;

/*@brief Event Log D-Bus details */
extern const std::string eventLogService;
extern const std::string eventLogObjPath;
extern const std::string eventLogIface;

/* @brief Holds the Host Power status
 *
 * @param[value] "Off": The host is powered off
 * @param[value] "On": The host is powered on
 * @param[value] "Unknown": The host power state is unknown
 */
extern std::string hostPowerState;

/*@brief storing the KVM status */
extern bool isKvmDisabled;

/*@brief NO SIGNAL image stored Path */
extern const char* NO_SIGNAL_IMG_PATH;
/*@brief POWER OFF image stored Path */
extern const char* POWER_OFF_IMG_PATH;

/*@brief video Record flags */
extern std::atomic<bool> videoRecFlag;
extern std::atomic<bool> recThreadStatus;
extern const std::string recProcessDir;
extern const std::string screenRecPath;

/*@brief Video Remote Storage Config. */
extern uint8_t maxDumps;
extern uint8_t maxDuration;
extern uint8_t maxSize;
extern std::string serverIP;
extern std::string pathInServer;
extern std::string shareType;
extern std::string options;
extern bool recordToRemote;
extern bool active;

/*@brief 2700 specific Config. */
static constexpr const char* usbVirtualHubPath2700A0 =
        "/sys/bus/platform/devices/12011000.usb-vhub";
static constexpr const char* usbVirtualHubPath2700A1 =
        "/sys/bus/platform/devices/12060000.usb-vhub";

static constexpr int NO_VALID_FRAME_COUNT_THRESHOLD = 2;
static constexpr int NO_VALID_FRAME_COUNT_RESET = 0;
extern bool isAst2700Platform;

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

/*
 * @brief Gets Video Remote Storage latest Configurations
 */
void getRemoteConf();

/*
 * @brief Extracts the session ID from the session information string.
 *
 * @param[in] infoStr - The session information string.
 * @return The extracted session ID as an 8-bit unsigned integer.
 */
uint8_t extractSessionId(const std::string& infoStr);

/*
 * @brief Trims leading and trailing whitespace from a string.
 *
 * @param[in] s - The input string to be trimmed.
 * @return A new string with leading and trailing whitespace removed.
 */
std::string trim(const std::string& s);

/*
 * @brief Parses a key-value string into a map.
 *
 * @param[in] input - The input string containing key-value pairs.
 * @return A map where keys are strings and values are strings.
 */
std::map<std::string, std::string> parseKeyValueString(const std::string& input);

/*
 * @brief Creates an event log entry with the given message.
 *
 * @param[in] msg - The message to log.
 */
void eventLogSupport(const std::string& msg);

} // namespace ikvm
