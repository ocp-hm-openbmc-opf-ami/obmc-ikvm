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

        // Add matchers for monitoring variuou D-bus events

        matchers.emplace_back(bsodErrorEventMonitor(conn));
        matchers.emplace_back(tempSensCritMonitor(conn));
        matchers.emplace_back(tempSensNonCritMonitor(conn));
        matchers.emplace_back(voltSensCritMonitor(conn));
        matchers.emplace_back(voltSensNonCritMonitor(conn));
        matchers.emplace_back(hostPowerOptMonitor(conn));
        matchers.emplace_back(lpcResetMonitor(conn));
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
            log<level::ERR>("Failed to call method:  ");
            log<level::ERR>("Error : ", entry("ERROR=%s", response.c_str()));
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
                        log<level::ERR>("Error : ",
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
                log<level::INFO>(
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
                log<level::INFO>(
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
                log<level::INFO>(
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
            if (!(triggerEvents.test(triggerEvent::criticalTmpVolt)))
            {
                log<level::INFO>(
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

sdbusplus::bus::match_t Monitor::hostPowerOptMonitor(
    const std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto hostPowerOptCallback = [conn, this](sdbusplus::message_t& msg) {
        try
        {
            kvmDbus::loadJson();
            triggerEvents = (uint32_t)
                jsonData["VideoRecord"]["TriggerSettings"]["TriggeringEvents"];

            if (!(triggerEvents.test(triggerEvent::chassisPowerOn)) ||
                !(triggerEvents.test(triggerEvent::chassisPowerOff)) ||
                !(triggerEvents.test(triggerEvent::chassisReset)))
            {
                log<level::INFO>(
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
                            log<level::INFO>(
                                "Power ON: not Selected as Triggering event");
                            return;
                        }
                    }
                    else if (transitType.find("Off") != std::string::npos)
                    {
                        if (!(triggerEvents.test(
                                triggerEvent::chassisPowerOff)))
                        {
                            log<level::INFO>(
                                "Power OFF: not Selected as Triggering event");
                            return;
                        }
                    }
                    else if (transitType.find("Reboot") != std::string::npos)
                    {
                        if (!(triggerEvents.test(triggerEvent::chassisReset)))
                        {
                            log<level::INFO>(
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
            tempObjPathNamespace + "',arg0namespace='" + critInterface + "'",
        std::move(hostPowerOptCallback));

    return hostPowerOptMatcher;
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
                log<level::INFO>("lpcReset: not Selected as Triggering event");
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
                    const auto& [timestamp,
                                 byteArray] = std::get<TayPropertyType>(value);

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

} // namespace kvmDbus
