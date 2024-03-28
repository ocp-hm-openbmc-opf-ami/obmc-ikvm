/*
 * ****************************************************************
 *
 * KVM server Monitor for dbus async events
 * Filename : ikvm_monitor.hpp
 *
 * @brief Implementation of KVM async dbus signal monitor
 *
 * Author: Amlana Bhuyan [amlanab@ami.com]
 *
 * *****************************************************************
 */

#pragma once

#include "ikvm_utils.hpp"

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus/match.hpp>

#include <boost/container/flat_map.hpp>
#include <phosphor-logging/log.hpp>
#include <variant>
#include <iostream>
#include <filesystem>



namespace fs = std::filesystem;

namespace ikvm
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
    explicit Monitor();
    ~Monitor() = default;
    Monitor(const Monitor&) = default;
    Monitor& operator=(const Monitor&) = default;
    Monitor(Monitor&&) = default;
    Monitor& operator=(Monitor&&) = default;

    /*
     *
     *  @brief D-Bus Signal Monitor for bsodErrorEvent
     *
     *  @param[in]conn Pointer to Dbus Connection
     */
    sdbusplus::bus::match_t bsodErrorEventMonitor(const std::shared_ptr<sdbusplus::asio::connection> conn);

    /*
     *
     *  @brief D-Bus Signal Monitor for Screenshot trigger
     *
     *  @param[in]conn Pointer to Dbus Connection
     */
    sdbusplus::bus::match_t screenshotMonitor(const std::shared_ptr<sdbusplus::asio::connection> conn);

    /*
     * @brief Create new directory for kvm  in Persistent
     * memory of BMC if not available
     */
    bool isDir(const std::string& path);
    void createUtilities();
};

} // namespace ikvm