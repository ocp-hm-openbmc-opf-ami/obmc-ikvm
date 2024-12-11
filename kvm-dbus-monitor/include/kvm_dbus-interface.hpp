/*
 * ****************************************************************************
 *
 * KVM Dbus interface
 * Filename : kvm_dbus-interface.hpp
 *
 * @brief Implementation of KVM Dbus Interface
 *
 * Author: Amlana Bhuyan [amlanab@ami.com]
 *
 * ****************************************************************************
 */

#pragma once

#include "kvm_dbus-utils.hpp"

#include <string.h>

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
#include <thread>
#include <vector>

namespace kvmDbus
{

/*
 * @class Interface
 * @brief adds Interface for Dbus communication
 *
 */
class Interface
{
  public:
    /*@brief Interface constructor*/
    Interface(sdbusplus::asio::object_server& objserver);

    ~Interface() = default;
    /*@brief Wrapper function for Dbus Intefaraces intialization */
    void initialize();

    /*@brief adds dbus Intefarace for Screenshot*/
    void addScreenshotInterface();

    /*@brief adds dbus Intefarace for Video Record*/
    void addVideoRecordInterface();

    /*@brief adds dbus Intefarace for video Record Settings */
    void addscrnRecTriggInterface();

    /*@brief adds dbus Intefarace for Remote video Record Store */
    void addscrnRecRmtStoreInterface();

    /*@brief adds dbus Intefarace for Pre-Event video Record */
    void addscrnRecPreEvntInterface();

    /*
     * @brief Implementation of dbus method TriggerScreenshot
     *
     * @param[in] scrnshotReqType - Screenshot Request type
     * currently  value:1 is only valid for trigger
     *
     */
    std::string TriggerScreenshot(int scrnshotReqType);

    /*
     * @brief Implementation of dbus method TriggerScreenshot
     *
     * @param [in] videoRecReqType - Video Record Request type
     *  allowed values:
     *    Start - raise flag for Starting Video record
     *    Stop - raise flag for Stoping Video record
     */
    std::string TriggerVideoRecord(std::string videoRecReqType);

    /*
     * @brief Implementation of dbus method for Updating Triggering Events
     *
     * @param[in] triggerEvents : each bit represents an triggering Event
     * [check "triggerEvent" enum for the Triggering Events and corresponding
     * bit]
     */
    std::string UpdateTriggeringEvents(const uint32_t& triggerEvents);

    /*
     * @brief Implementation of dbus method for Updating Trigger Date and Time
     *
     * @param[in] date : YYYY-MM-DD
     * @param[in] time : hh:mm:ss
     */
    std::string UpdateTriggerDateTime(std::string date, std::string time);

    /*
     * @brief Implementation of dbus method for Enabling remote Storage
     *
     * @param[in] recordToRemote : boolean
     *
     */
    std::string EnableRemoteStorage(bool recordToRemote);

    /*
     * @brief Implementation of dbus method for Updating Remote
     * storage info.
     *
     * @param[in] maxDumps : unit_8 [allowed value 1]
     * @param[in] maxDuration :  [uint8_t, 1 <= allowed value <=20 in Seconds]
     * @param[in] maxSize: [uint8_t,1 <= allowed value <=10 in MB]
     * @param[in] serverIP [string]
     * @param[in] pathInServer[string]
     * @param[in] shareType[string]
     * @param[in] credFd[VARIANT<INT32_t,UNIX_FD>]
     */
    std::string UpdateRemoteStorageInfo(uint8_t maxDumps, uint8_t maxDuration,
                                        uint8_t maxSize, std::string serverIP,
                                        std::string pathInServer,
                                        std::string shareType,
                                        credentialVariant credFd);
    /*
     * @brief Implementation of dbus method for Updating Remote
     * storage info.
     *
     * @param[in] compressMode : 1:High 2:Normal 3:Low 4:No
     * @param[in] fps : allowed values : 1,2,3,4
     * @param[in] maxDuration [uint8_t, 1 <= allowed value <=20 in Seconds]
     * @param[in] videoQuality : allowed values : 1,2,3,4,5
     *
     */
    std::string UpdatePreEventTriggerInfo(uint8_t compressMode, uint8_t fps,
                                          uint8_t maxDuration,
                                          uint8_t videoQuality);

  private:
    sdbusplus::asio::object_server& server;
    std::shared_ptr<sdbusplus::asio::dbus_interface> scrnshotIface = nullptr;
    std::shared_ptr<sdbusplus::asio::dbus_interface> ScrnRecIface = nullptr;
    std::shared_ptr<sdbusplus::asio::dbus_interface> scrnRecTriggIface =
        nullptr;
    std::shared_ptr<sdbusplus::asio::dbus_interface> scrnRecRmtStoreIface =
        nullptr;
    std::shared_ptr<sdbusplus::asio::dbus_interface> scrnRecPreEvntIface =
        nullptr;
};

} // namespace kvmDbus
