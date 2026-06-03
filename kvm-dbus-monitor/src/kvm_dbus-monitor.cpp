/*
 * ****************************************************************************
 *
 * KVM Dbus Monitor for async events
 * Filename : kvm_dbus-monitor.hpp
 *
 * @brief Implementation of KVM async dbus signal monitor
 *
 * Author: Amlana Bhuyan [amlanab@ami.com]
 *
 * ****************************************************************************
 */

#include "kvm_dbus-monitor.hpp"

namespace kvmDbus
{

using namespace phosphor::logging;

void Monitor::initialize(
    const std::shared_ptr<sdbusplus::asio::connection>& conn)
{
    try
    {
        kvmDbus::loadJson();

        triggerEvents = (uint32_t)
            jsonData["VideoRecord"]["TriggerSettings"]["TriggeringEvents"];

        // Add matchers for monitoring various D-bus events

        matchers.emplace_back(bsodErrorEventMonitor(conn));
        matchers.emplace_back(tempSensCritMonitor(conn));
        matchers.emplace_back(tempSensNonCritMonitor(conn));
        matchers.emplace_back(tempSensNonRecovMonitor(conn));
        matchers.emplace_back(voltSensCritMonitor(conn));
        matchers.emplace_back(voltSensNonCritMonitor(conn));
        matchers.emplace_back(voltSensNonRecovMonitor(conn));
        matchers.emplace_back(hostPowerOptMonitor(conn));
        matchers.emplace_back(hostForcedShutdownMonitor(conn));
        matchers.emplace_back(lpcResetMonitor(conn));
        matchers.emplace_back(fanSensWarnMonitor(conn));
        matchers.emplace_back(fanSensCritMonitor(conn));
        matchers.emplace_back(fanRemovalMonitor(conn));
        matchers.emplace_back(watchdogTimeoutMonitor(conn));
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Error Initiallizing [monitor]",
                        entry("ERROR=%s", e.what()));
        return;
    }
}

void Monitor::AsyncRecordTrigger(
    std::shared_ptr<sdbusplus::asio::connection> conn, std::string recType)
{
    conn->async_method_call(
        [this](const boost::system::error_code& ec,
               const std::string& response) {
            if (ec)
            {
                log<level::ERR>("Failed to call method:  ");
                return;
            }
            else if (response != "Success")
            {
                log<level::DEBUG>("Failed to call method:  ");
                log<level::DEBUG>("Error : ",
                                  entry("ERROR=%s", response.c_str()));
                return;
            }
            log<level::DEBUG>("Video record Triggered Succesfully");
        },
        "xyz.openbmc_project.Kvm", "/xyz/openbmc_project/Kvm",
        "xyz.openbmc_project.Kvm.VideoRecord", "TriggerRecord", recType);
}

sdbusplus::bus::match_t Monitor::bsodErrorEventMonitor(
    std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto bsodMatcherCallback = [conn, this](sdbusplus::message_t& msg) {
        try
        {
            std::string discreteInterface;
            boost::container::flat_map<std::string, std::variant<uint16_t>>
                bsodProperties;
            msg.read(discreteInterface, bsodProperties);

            uint16_t osState =
                std::get<uint16_t>(bsodProperties.begin()->second);

            if (osState == BSOD)
            {
                int32_t scrnshotType = 1;
                conn->async_method_call(
                    [this](const boost::system::error_code& ec,
                           const std::string& response) {
                        if (ec)
                        {
                            log<level::ERR>("Failed to call screenshot");
                            return;
                        }
                        else if (response != "Success")
                        {
                            log<level::ERR>("Failed to call method:  ");
                            log<level::ERR>(
                                "Error : ",
                                entry("ERROR=%s", response.c_str()));
                            return;
                        }
                        log<level::DEBUG>("ScreenShot triggered successfully");
                    },
                    "xyz.openbmc_project.Kvm", "/xyz/openbmc_project/Kvm",
                    "xyz.openbmc_project.Kvm.Screenshot", "TriggerScreenshot",
                    scrnshotType);

                AsyncRecordTrigger(conn, "Start");
            }
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("Error handling BSOD Event signal ");
            log<level::ERR>("Error : ", entry("ERROR=%s", e.what()));
        }
    };

    sdbusplus::bus::match_t bsodMatcher(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged',path_namespace='" +
            bsodObjPathNamespace + "',arg0namespace='" + bsodInterface + "'",
        std::move(bsodMatcherCallback));

    return bsodMatcher;
}

sdbusplus::bus::match_t Monitor::tempSensCritMonitor(
    const std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto tempCritCallback = [conn, this](sdbusplus::message_t& msg) {
        try
        {
            kvmDbus::loadJson();
            triggerEvents = (uint32_t)
                jsonData["VideoRecord"]["TriggerSettings"]["TriggeringEvents"];
            if (!(triggerEvents.test(triggerEvent::criticalTmpVolt)))
            {
                log<level::DEBUG>(
                    " Critical temp: not Selected as Triggering event");
                return;
            }

            std::string interfaceName;
            boost::container::flat_map<std::string, std::variant<bool>>
                scrnshotProperty;
            msg.read(interfaceName, scrnshotProperty);

            for (const auto& entry : scrnshotProperty)
            {
                if (entry.first == "CriticalAlarmHigh" ||
                    entry.first == "CriticalAlarmLow")
                {
                    if (std::get<bool>(entry.second))
                    {
                        AsyncRecordTrigger(conn, "Start");
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("Error : ", entry("ERROR=%s", e.what()));
        }
    };

    sdbusplus::bus::match_t tempCritMatcher(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged',path_namespace='" +
            tempObjPathNamespace + "',arg0namespace='" + critInterface + "'",
        std::move(tempCritCallback));

    return tempCritMatcher;
}

sdbusplus::bus::match_t Monitor::tempSensNonCritMonitor(
    const std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto tempNonCritCallback = [conn, this](sdbusplus::message_t& msg) {
        try
        {
            kvmDbus::loadJson();
            triggerEvents = (uint32_t)
                jsonData["VideoRecord"]["TriggerSettings"]["TriggeringEvents"];
            if (!(triggerEvents.test(triggerEvent::nonCriticalTmpVolt)))
            {
                log<level::DEBUG>(
                    " Warning temp: not Selected as Triggering event");
                return;
            }

            std::string interfaceName;
            boost::container::flat_map<std::string, std::variant<bool>>
                scrnshotProperty;
            msg.read(interfaceName, scrnshotProperty);

            for (const auto& entry : scrnshotProperty)
            {
                if (entry.first == "WarningAlarmHigh" ||
                    entry.first == "WarningAlarmLow")
                {
                    if (std::get<bool>(entry.second))
                    {
                        AsyncRecordTrigger(conn, "Start");
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("Error : ", entry("ERROR=%s", e.what()));
        }
    };

    sdbusplus::bus::match_t tempNonCritMatcher(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged',path_namespace='" +
            tempObjPathNamespace + "',arg0namespace='" + nonCritInterface + "'",
        std::move(tempNonCritCallback));

    return tempNonCritMatcher;
}

sdbusplus::bus::match_t Monitor::tempSensNonRecovMonitor(
    const std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto tempNonRecovCallback = [conn, this](sdbusplus::message_t& msg) {
        try
        {
            kvmDbus::loadJson();
            triggerEvents = (uint32_t)
                jsonData["VideoRecord"]["TriggerSettings"]["TriggeringEvents"];
            if (!(triggerEvents.test(triggerEvent::nonRecovTmpVolt)))
            {
                log<level::DEBUG>(
                    " Non-Recoverable temp: not Selected as Triggering event");
                return;
            }

            std::string interfaceName;
            boost::container::flat_map<std::string, std::variant<bool>>
                scrnshotProperty;
            msg.read(interfaceName, scrnshotProperty);

            for (const auto& entry : scrnshotProperty)
            {
                if (entry.first == "NonRecoverableAlarmHigh" ||
                    entry.first == "NonRecoverableAlarmLow")
                {
                    if (std::get<bool>(entry.second))
                    {
                        AsyncRecordTrigger(conn, "Start");
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("Error : ", entry("ERROR=%s", e.what()));
        }
    };

    sdbusplus::bus::match_t tempNonRecovMatcher(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged',path_namespace='" +
            tempObjPathNamespace + "',arg0namespace='" + nonRecovInterface +
            "'",
        std::move(tempNonRecovCallback));

    return tempNonRecovMatcher;
}

sdbusplus::bus::match_t Monitor::voltSensCritMonitor(
    const std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto voltCritCallback = [conn, this](sdbusplus::message_t& msg) {
        try
        {
            kvmDbus::loadJson();
            triggerEvents = (uint32_t)
                jsonData["VideoRecord"]["TriggerSettings"]["TriggeringEvents"];
            if (!(triggerEvents.test(triggerEvent::criticalTmpVolt)))
            {
                log<level::DEBUG>(
                    "Critical Voltage: not Selected as Triggering event");
                return;
            }

            std::string interfaceName;
            boost::container::flat_map<std::string, std::variant<bool>>
                scrnshotProperty;
            msg.read(interfaceName, scrnshotProperty);

            for (const auto& entry : scrnshotProperty)
            {
                if (entry.first == "CriticalAlarmHigh" ||
                    entry.first == "CriticalAlarmLow")
                {
                    if (std::get<bool>(entry.second))
                    {
                        AsyncRecordTrigger(conn, "Start");
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("Error : ", entry("ERROR=%s", e.what()));
        }
    };

    sdbusplus::bus::match_t voltCritMatcher(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged',path_namespace='" +
            voltObjPathNamespace + "',arg0namespace='" + critInterface + "'",
        std::move(voltCritCallback));

    return voltCritMatcher;
}

sdbusplus::bus::match_t Monitor::voltSensNonCritMonitor(
    const std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto voltNonCritCallback = [conn, this](sdbusplus::message_t& msg) {
        try
        {
            kvmDbus::loadJson();
            triggerEvents = (uint32_t)
                jsonData["VideoRecord"]["TriggerSettings"]["TriggeringEvents"];
            if (!(triggerEvents.test(triggerEvent::nonCriticalTmpVolt)))
            {
                log<level::DEBUG>(
                    "Warning Voltage: not Selected as Triggering event");
                return;
            }

            std::string interfaceName;
            boost::container::flat_map<std::string, std::variant<bool>>
                scrnshotProperty;
            msg.read(interfaceName, scrnshotProperty);

            for (const auto& entry : scrnshotProperty)
            {
                if (entry.first == "WarningAlarmHigh" ||
                    entry.first == "WarningAlarmLow")
                {
                    if (std::get<bool>(entry.second))
                    {
                        AsyncRecordTrigger(conn, "Start");
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("Error : ", entry("ERROR=%s", e.what()));
        }
    };

    sdbusplus::bus::match_t voltNonCritMatcher(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged',path_namespace='" +
            voltObjPathNamespace + "',arg0namespace='" + nonCritInterface + "'",
        std::move(voltNonCritCallback));

    return voltNonCritMatcher;
}

sdbusplus::bus::match_t Monitor::voltSensNonRecovMonitor(
    const std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto voltNonRecovCallback = [conn, this](sdbusplus::message_t& msg) {
        try
        {
            kvmDbus::loadJson();
            triggerEvents = (uint32_t)
                jsonData["VideoRecord"]["TriggerSettings"]["TriggeringEvents"];
            if (!(triggerEvents.test(triggerEvent::nonRecovTmpVolt)))
            {
                log<level::DEBUG>(
                    "Non-Recoverable Voltage: not Selected as Triggering event");
                return;
            }

            std::string interfaceName;
            boost::container::flat_map<std::string, std::variant<bool>>
                scrnshotProperty;
            msg.read(interfaceName, scrnshotProperty);

            for (const auto& entry : scrnshotProperty)
            {
                if (entry.first == "NonRecoverableAlarmHigh" ||
                    entry.first == "NonRecoverableAlarmLow")
                {
                    if (std::get<bool>(entry.second))
                    {
                        AsyncRecordTrigger(conn, "Start");
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("Error : ", entry("ERROR=%s", e.what()));
        }
    };

    sdbusplus::bus::match_t voltNonRecovMatcher(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged',path_namespace='" +
            voltObjPathNamespace + "',arg0namespace='" + nonRecovInterface +
            "'",
        std::move(voltNonRecovCallback));

    return voltNonRecovMatcher;
}

sdbusplus::bus::match_t Monitor::hostPowerOptMonitor(
    const std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto hostPowerOptCallback = [conn, this](sdbusplus::message_t& msg) {
        try
        {
            kvmDbus::loadJson();
            triggerEvents = (uint32_t)
                jsonData["VideoRecord"]["TriggerSettings"]["TriggeringEvents"];

            if (!(triggerEvents.test(triggerEvent::chassisPowerOn)) &&
                !(triggerEvents.test(triggerEvent::chassisPowerOff)) &&
                !(triggerEvents.test(triggerEvent::chassisReset)))
            {
                log<level::DEBUG>(
                    "Power Operations: not Selected as Triggering event");
                return;
            }

            std::string interfaceName;
            boost::container::flat_map<std::string, std::variant<std::string>>
                hostPowerProperty;
            msg.read(interfaceName, hostPowerProperty);

            for (const auto& entry : hostPowerProperty)
            {
                if (entry.first == "RequestedHostTransition")
                {
                    std::string transitType =
                        std::get<std::string>(entry.second);
                    if (transitType.find("On") != std::string::npos)
                    {
                        if (!(triggerEvents.test(triggerEvent::chassisPowerOn)))
                        {
                            log<level::DEBUG>(
                                "Power ON: not Selected as Triggering event");
                            return;
                        }
                    }
                    else if (transitType.find("Off") != std::string::npos)
                    {
                        if (!(triggerEvents.test(
                                triggerEvent::chassisPowerOff)))
                        {
                            log<level::DEBUG>(
                                "Power OFF: not Selected as Triggering event");
                            return;
                        }
                    }
                    else if (transitType.find("Reboot") != std::string::npos)
                    {
                        if (!(triggerEvents.test(triggerEvent::chassisReset)))
                        {
                            log<level::DEBUG>(
                                "Power Reset:not Selected as Triggering event");
                            return;
                        }
                    }
                    /* If code reached this point atleast one of the power
                     * is enabled as triggerring event,hence trigger auto
                     * video Record
                     */
                    AsyncRecordTrigger(conn, "Start");
                }
            }
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("Error : ", entry("ERROR=%s", e.what()));
        }
    };

    sdbusplus::bus::match_t hostPowerOptMatcher(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged',path_namespace='" +
            hostStateObjpath + "',arg0namespace='" + hostStateInterface + "'",
        std::move(hostPowerOptCallback));

    return hostPowerOptMatcher;
}

sdbusplus::bus::match_t Monitor::hostForcedShutdownMonitor(
    const std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto hostForcedShutdowncallback = [conn, this](sdbusplus::message_t& msg) {
        try
        {
            kvmDbus::loadJson();
            triggerEvents = (uint32_t)
                jsonData["VideoRecord"]["TriggerSettings"]["TriggeringEvents"];

            if (!(triggerEvents.test(triggerEvent::chassisPowerOff)))
            {
                log<level::DEBUG>(
                    "Power Off Operation: not Selected as Triggering event");
                return;
            }
            std::string interfaceName;
            boost::container::flat_map<std::string, std::variant<std::string>>
                chassisPowerProperty;
            msg.read(interfaceName, chassisPowerProperty);

            for (const auto& entry : chassisPowerProperty)
            {
                if (entry.first == "RequestedPowerTransition")
                {
                    std::string transitType =
                        std::get<std::string>(entry.second);

                    if (transitType.find("Off") != std::string::npos)
                    {
                        if (!(triggerEvents.test(
                                triggerEvent::chassisPowerOff)))
                        {
                            log<level::DEBUG>(
                                "Power OFF: not Selected as Triggering event");
                            return;
                        }
                    }
                    AsyncRecordTrigger(conn, "Start");
                }
                else if (entry.first == "CurrentPowerState")
                {
                    std::string powerState =
                        std::get<std::string>(entry.second);

                    if (powerState.find("Off") != std::string::npos)
                    {
                        if (!(triggerEvents.test(
                                triggerEvent::chassisPowerOff)))
                        {
                            log<level::DEBUG>(
                                "Chassis Power OFF: not Selected as Triggering event");
                            return;
                        }

                        AsyncRecordTrigger(conn, "Start");
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("Error : ", entry("ERROR=%s", e.what()));
        }
    };

    sdbusplus::bus::match_t hostForcedShutdownMatcher(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged',path_namespace='" +
            chassisObjpath + "',arg0namespace='" + chassisInterface + "'",
        std::move(hostForcedShutdowncallback));

    return hostForcedShutdownMatcher;
}

sdbusplus::bus::match_t Monitor::lpcResetMonitor(
    const std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto lpcResetCallback = [conn, this](sdbusplus::message_t& msg) {
        try
        {
            kvmDbus::loadJson();
            triggerEvents = (uint32_t)
                jsonData["VideoRecord"]["TriggerSettings"]["TriggeringEvents"];
            if (!(triggerEvents.test(triggerEvent::lPCReset)))
            {
                log<level::DEBUG>("lpcReset: not Selected as Triggering event");
                return;
            }

            std::string interfaceName;

            using TayPropertyType = std::tuple<uint64_t, std::vector<uint8_t>>;
            boost::container::flat_map<std::string,
                                       std::variant<TayPropertyType>>
                lpcProperty;

            msg.read(interfaceName, lpcProperty);

            for (const auto& entry : lpcProperty)
            {
                const auto& value = entry.second;

                if (std::holds_alternative<TayPropertyType>(value))
                {
                    const auto& [timestamp, byteArray] =
                        std::get<TayPropertyType>(value);

                    if (timestamp == 0 || byteArray.empty())
                    {
                        log<level::DEBUG>(
                            "lpcReset: video record not Triggering");
                    }
                    else
                    {
                        log<level::DEBUG>("lpcReset: Triggered");
                        AsyncRecordTrigger(conn, "Start");
                    }
                }
                else
                {
                    log<level::DEBUG>("lpcReset: Unexpected  type of property");
                }
            }
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("Error handling lpcProperty signal",
                            entry("ERROR=%s", e.what()));
            return;
        }
    };

    sdbusplus::bus::match_t lpcResetMatcher(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged',path_namespace='" + lpcpath +
            "',arg0namespace='" + lpcInterface + "'",
        std::move(lpcResetCallback));

    return lpcResetMatcher;
}

sdbusplus::bus::match_t Monitor::fanSensWarnMonitor(
    const std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto fanWarnCallback = [conn, this](sdbusplus::message_t& msg) {
        try
        {
            kvmDbus::loadJson();
            triggerEvents = (uint32_t)
                jsonData["VideoRecord"]["TriggerSettings"]["TriggeringEvents"];
            if (!(triggerEvents.test(triggerEvent::fanstatechanged)))
            {
                log<level::DEBUG>(
                    "Fan Warning: not Selected as Triggering event");
                return;
            }

            // Suppress threshold alarms after fan reconnect
            // to avoid false AVR triggers during fan spin-up
            std::string fanPath = msg.get_path();
            auto it = fanReconnectTimes.find(fanPath);
            if (it != fanReconnectTimes.end())
            {
                auto elapsed = std::chrono::steady_clock::now() - it->second;
                if (elapsed < std::chrono::seconds(fanReconnectSuppressionSecs))
                {
                    log<level::DEBUG>(
                        "Fan Warning: suppressed during reconnect spin-up",
                        entry("FAN_PATH=%s", fanPath.c_str()));
                    return;
                }
                fanReconnectTimes.erase(it);
            }

            std::string interfaceName;
            boost::container::flat_map<std::string, std::variant<bool>>
                fanProperty;
            msg.read(interfaceName, fanProperty);

            for (const auto& entry : fanProperty)
            {
                if (entry.first == "WarningAlarmHigh" ||
                    entry.first == "WarningAlarmLow")
                {
                    if (std::get<bool>(entry.second))
                    {
                        AsyncRecordTrigger(conn, "Start");
                        return;
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("Error handling Fan Warning signal",
                            entry("ERROR=%s", e.what()));
        }
    };

    sdbusplus::bus::match_t fanWarnMatcher(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged',path_namespace='" +
            fanObjPathNamespace + "',arg0namespace='" + nonCritInterface + "'",
        std::move(fanWarnCallback));

    return fanWarnMatcher;
}

sdbusplus::bus::match_t Monitor::fanSensCritMonitor(
    const std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto fanCritCallback = [conn, this](sdbusplus::message_t& msg) {
        try
        {
            kvmDbus::loadJson();
            triggerEvents = (uint32_t)
                jsonData["VideoRecord"]["TriggerSettings"]["TriggeringEvents"];
            if (!(triggerEvents.test(triggerEvent::fanstatechanged)))
            {
                log<level::DEBUG>(
                    "Fan Critical: not Selected as Triggering event");
                return;
            }

            // Suppress threshold alarms after fan reconnect
            // to avoid false AVR triggers during fan spin-up
            std::string fanPath = msg.get_path();
            auto it = fanReconnectTimes.find(fanPath);
            if (it != fanReconnectTimes.end())
            {
                auto elapsed = std::chrono::steady_clock::now() - it->second;
                if (elapsed < std::chrono::seconds(fanReconnectSuppressionSecs))
                {
                    log<level::DEBUG>(
                        "Fan Critical: suppressed during reconnect spin-up",
                        entry("FAN_PATH=%s", fanPath.c_str()));
                    return;
                }
                fanReconnectTimes.erase(it);
            }

            std::string interfaceName;
            boost::container::flat_map<std::string, std::variant<bool>>
                fanProperty;
            msg.read(interfaceName, fanProperty);

            for (const auto& entry : fanProperty)
            {
                if (entry.first == "CriticalAlarmHigh" ||
                    entry.first == "CriticalAlarmLow")
                {
                    if (std::get<bool>(entry.second))
                    {
                        AsyncRecordTrigger(conn, "Start");
                        return;
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("Error handling Fan Critical signal",
                            entry("ERROR=%s", e.what()));
        }
    };

    sdbusplus::bus::match_t fanCritMatcher(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged',path_namespace='" +
            fanObjPathNamespace + "',arg0namespace='" + critInterface + "'",
        std::move(fanCritCallback));

    return fanCritMatcher;
}

sdbusplus::bus::match_t Monitor::fanRemovalMonitor(
    const std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto fanRemovalCallback = [conn, this](sdbusplus::message_t& msg) {
        try
        {
            // *** Read payload FIRST to correctly consume the message ***
            bool fanRunning = true;
            msg.read(fanRunning);
            std::string fanPath = msg.get_path();

            // Fan reconnected — record time for spin-up suppression window
            if (fanRunning)
            {
                fanReconnectTimes[fanPath] = std::chrono::steady_clock::now();
                log<level::DEBUG>(
                    "Fan Reconnected - No AVR trigger (suppressing threshold alarms)",
                    entry("FAN_PATH=%s", fanPath.c_str()),
                    entry("SUPPRESSION_SECS=%u", fanReconnectSuppressionSecs));
                return;
            }

            // Fan is NOT running — check trigger config
            kvmDbus::loadJson();
            triggerEvents = (uint32_t)
                jsonData["VideoRecord"]["TriggerSettings"]["TriggeringEvents"];

            if (!(triggerEvents.test(triggerEvent::fanstatechanged)))
            {
                log<level::DEBUG>(
                    "Fan Removal: not Selected as Triggering event");
                return;
            }

            // Guard: FanRunning signal also fires during host power
            // on/off/reset event. Only trigger AVR when host is Running.
            try
            {
                auto hostStateMsg = conn->new_method_call(
                    "xyz.openbmc_project.State.Host", hostStateObjpath.c_str(),
                    "org.freedesktop.DBus.Properties", "Get");
                hostStateMsg.append(hostStateInterface,
                                    std::string("CurrentHostState"));

                auto hostStateResp = conn->call(hostStateMsg);
                std::variant<std::string> hostStateVariant;
                hostStateResp.read(hostStateVariant);
                std::string hostState = std::get<std::string>(hostStateVariant);

                if (hostState.find("Running") == std::string::npos)
                {
                    log<level::DEBUG>(
                        "Fan Removal: Host not Running - ignoring signal",
                        entry("HOST_STATE=%s", hostState.c_str()));
                    return;
                }
            }
            catch (const std::exception& e)
            {
                log<level::ERR>(
                    "Fan Removal: Failed to read host state - skipping trigger",
                    entry("ERROR=%s", e.what()));
                return;
            }

            // Explicit guard — fanRunning must be false before triggering AVR
            if (!fanRunning)
            {
                log<level::INFO>("Fan Removal/Fault detected - Triggering AVR",
                                 entry("FAN_PATH=%s", fanPath.c_str()));
                AsyncRecordTrigger(conn, "Start");
            }
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("Error handling Fan Removal signal",
                            entry("ERROR=%s", e.what()));
        }
    };

    // Direct "FanRunning" signal monitor for fan removal events.
    sdbusplus::bus::match_t fanRemovalMatcher(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',path_namespace='" + fanObjPathNamespace +
            "',interface='" + fanStatusInterface + "',member='FanRunning'",
        std::move(fanRemovalCallback));

    return fanRemovalMatcher;
}

sdbusplus::bus::match_t Monitor::watchdogTimeoutMonitor(
    const std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto watchdogCallback = [conn, this](sdbusplus::message_t& msg) {
        try
        {
            kvmDbus::loadJson();
            triggerEvents = (uint32_t)
                jsonData["VideoRecord"]["TriggerSettings"]["TriggeringEvents"];

            if (!(triggerEvents.test(triggerEvent::watchdogTimer)))
            {
                log<level::DEBUG>(
                    "Watchdog Timeout: not Selected as Triggering event");
                return;
            }

            // Signal payload carries the ExpireAction string
            // e.g. "xyz.openbmc_project.State.Watchdog.Action.None"
            std::string expireAction;
            msg.read(expireAction);

            log<level::INFO>("Watchdog Timeout signal received",
                             entry("ACTION=%s", expireAction.c_str()));

            AsyncRecordTrigger(conn, "Start");
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("Error handling Watchdog Timeout signal",
                            entry("ERROR=%s", e.what()));
        }
    };

    // Direct signal match for Watchdog "Timeout" signal.
    sdbusplus::bus::match_t watchdogMatcher(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',interface='" + wdogInterface + "',member='Timeout'",
        std::move(watchdogCallback));

    return watchdogMatcher;
}

} // namespace kvmDbus
