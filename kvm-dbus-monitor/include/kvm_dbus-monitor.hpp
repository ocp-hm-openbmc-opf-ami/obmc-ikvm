/*
 * ****************************************************************************
 *
 * KVM Dbus Monitor for async events
 * Filename : kvm_dbus-monitor.hpp
 *
 * @brief Implementation of KVM async dbus signal monitor for various
 * Events.
 *
 * Author: Amlana Bhuyan [amlanab@ami.com]
 *
 * ****************************************************************************
 */

#pragma once

#include "kvm_dbus-utils.hpp"

#include <boost/container/flat_map.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/bus/match.hpp>
#include <sdbusplus/exception.hpp>
#include <sdbusplus/message.hpp>
#include <sdbusplus/server/interface.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>

#include <filesystem>
#include <iostream>
#include <map>
#include <memory>
#include <variant>
#include <vector>

namespace fs = std::filesystem;

namespace kvmDbus
{

/*
 * @class Monitor
 * @brief adds reqiured monitor operations for async
 *  D-bus monitor
 */

class Monitor
{
  public:
    /*
     * @brief Constructs the Monitor object
     */
    Monitor() = default;
    ~Monitor() = default;
    Monitor(const Monitor&) = default;
    Monitor& operator=(const Monitor&) = default;
    Monitor(Monitor&&) = default;
    Monitor& operator=(Monitor&&) = default;

    /*
     *
     *  @brief Initialiser for monitor class
     *
     *  @param[in]conn Pointer to Dbus Connection
     */

    void initialize(const std::shared_ptr<sdbusplus::asio::connection>& conn);

    /*
     *
     *  @brief D-Bus Signal Monitor for bsodErrorEvent
     *
     *  @param[in]conn Pointer to Dbus Connection
     */
    sdbusplus::bus::match_t bsodErrorEventMonitor(
        const std::shared_ptr<sdbusplus::asio::connection> conn);

    /*
     *
     *  @brief D-Bus Async Dbus method call for
     *  Video Record Start/Stop
     *
     *  @param[in]conn Pointer to Dbus Connection
     *  @param[in]recType Start/ Stop
     */
    void AsyncRecordTrigger(std::shared_ptr<sdbusplus::asio::connection> conn,
                            std::string recType);

    /*
     *
     *  @brief D-Bus Signal Monitor for Critical Temperature Events.
     *
     *  @param[in]conn Pointer to Dbus Connection
     */
    sdbusplus::bus::match_t tempSensCritMonitor(
        const std::shared_ptr<sdbusplus::asio::connection> conn);

    /*
     *
     *  @brief D-Bus Signal Monitor for Non-Critical Temperature Events.
     *
     *  @param[in]conn Pointer to Dbus Connection
     */
    sdbusplus::bus::match_t tempSensNonCritMonitor(
        const std::shared_ptr<sdbusplus::asio::connection> conn);

    /*
     *
     *  @brief D-Bus Signal Monitor for Critical Voltage Event.
     *
     *  @param[in]conn Pointer to Dbus Connection
     */
    sdbusplus::bus::match_t voltSensCritMonitor(
        const std::shared_ptr<sdbusplus::asio::connection> conn);

    /*
     *
     *  @brief D-Bus Signal Monitor for Non-Critical Voltage Events.
     *
     *  @param[in]conn Pointer to Dbus Connection
     */
    sdbusplus::bus::match_t voltSensNonCritMonitor(
        const std::shared_ptr<sdbusplus::asio::connection> conn);

    /*
     *
     *  @brief D-Bus Signal Monitor for Host
     *  power Opearions.
     *
     *  @param[in]conn Pointer to Dbus Connection
     */
    sdbusplus::bus::match_t hostPowerOptMonitor(
        const std::shared_ptr<sdbusplus::asio::connection> conn);

    /*
     *
     *  @brief D-Bus Signal Monitor for LPC Events.
     *
     *  @param[in]conn Pointer to Dbus Connection
     */
    sdbusplus::bus::match_t lpcResetMonitor(
        const std::shared_ptr<sdbusplus::asio::connection> conn);

  private:
    std::vector<sdbusplus::bus::match_t> matchers;
    triggerEvents32 triggerEvents{};
    // Mask for bits 0 to (RESERVED - 1)
    uint32_t reservedMask = (1 << triggerEvent::RESERVED) - 1;
};

} // namespace kvmDbus
