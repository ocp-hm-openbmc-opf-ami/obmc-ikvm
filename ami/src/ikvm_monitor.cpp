/*
 * ****************************************************************************
 *
 * KVM server Monitor for dbus async events
 * Filename : ikvm_monitor.cpp
 *
 * @brief Implementation of KVM async dbus signal monitor
 *
 * Author: Amlana Bhuyan [amlanab@ami.com]
 *
 * ****************************************************************************
 */

#include "ami/include/ikvm_monitor.hpp"

#include "ikvm_server.hpp"

namespace ikvm
{

using namespace phosphor::logging;

Monitor::Monitor()
{
    scrnshotFlag.store(false);
}

sdbusplus::bus::match_t Monitor::bsodErrorEventMonitor(
    std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto bsodMatcherCallback = [&conn, this](sdbusplus::message_t& msg) {
        try
        {
            std::string discreteInterface;
            boost::container::flat_map<std::string, std::variant<uint16_t>>
                bsodProperties;
            msg.read(discreteInterface, bsodProperties);

            uint16_t offset =
                std::get<uint16_t>(bsodProperties.begin()->second);

            /*
             * offset value 2 indicates Run-time Critical Stop/BSOD
             * Data Reference
             *  IPMI Spec
             *   -> Sensor Type Codes and Data
             *      -> Table Sensor Type Codes
             *         -> OS Stop / Shutdown
             *            -> 01h Run-time Critical Stop
             */
            if (offset == 2)
            {
                scrnshotFlag.store(true);
            }
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("Error handling BSOD flag signal",
                            entry("ERROR=%s", e.what()));
        }
    };

    sdbusplus::bus::match_t bsodMatcher(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged',path_namespace='" +
            bsodTarget + "',arg0namespace='" + bsodSennType + "'",
        std::move(bsodMatcherCallback));

    return bsodMatcher;
}

sdbusplus::bus::match_t Monitor::screenshotMonitor(
    std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto screenshotCallback = [&conn, this](sdbusplus::message_t& msg) {
        try
        {
            std::string interfaceName;
            boost::container::flat_map<std::string, std::variant<bool>>
                scrnshotProperty;
            msg.read(interfaceName, scrnshotProperty);

            for (const auto& entry : scrnshotProperty)
            {
                if (entry.first == "Trigger")
                {
                    if (std::get<bool>(entry.second))
                    {
                        scrnshotFlag.store(true);
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("Error handling Screenshot trigger signal",
                            entry("ERROR=%s", e.what()));
        }
    };

    sdbusplus::bus::match_t screenshotMatcher(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged',path='" + kvmObjPath + "'," +
            "arg0namespace='" + scrnshotInterface + "'",
        std::move(screenshotCallback));

    return screenshotMatcher;
}

sdbusplus::bus::match_t
    Monitor::sessionMonitor(std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto sessionCallback = [&conn](sdbusplus::message_t& msg) {
        try
        {
            sessionRet updatedlist;
            std::string interfaceName;
            std::vector<uint8_t> UpdatedSessionIDs;

            boost::container::flat_map<std::string, propertyValue>
                sessionProperty;
            msg.read(interfaceName, sessionProperty);

            if (interfaceName == smgrKVMIface)
            {
                for (const auto& entry : sessionProperty)
                {
                    if (entry.first == "KvmSessionInfo")
                    {
                        updatedlist = std::get<sessionRet>(entry.second);
                        for (const auto& tuple : updatedlist)
                        {
                            // Extract session ID
                            uint8_t sessionID = std::get<0>(tuple);
                            UpdatedSessionIDs.push_back(sessionID);
                        }
                        // Update active session IDs with the updated session
                        // IDs
                        activeSessionIDs.assign(UpdatedSessionIDs.begin(),
                                                UpdatedSessionIDs.end());
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("Error handling for session monitor signal",
                            entry("ERROR=%s", e.what()));
        }
    };

    sdbusplus::bus::match_t triggerSignal(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged',path='" + smgrObjPath +
            "',arg0namespace='" + smgrKVMIface + "'",
        std::move(sessionCallback));

    return triggerSignal;
}

sdbusplus::bus::match_t Monitor::sessionTimeout(
    const std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto timeoutCallback = [&conn](sdbusplus::message_t& msg) {
        try
        {
            std::string interfaceName;
            boost::container::flat_map<std::string, std::variant<uint64_t>>
                properties;
            msg.read(interfaceName, properties);

            if (interfaceName == serviceMgrIface)
            {
                auto it = properties.find("SessionTimeOut");
                if (it != properties.end())
                {
                    timeoutValue = std::chrono::duration<uint64_t>(
                        std::get<uint64_t>(it->second));
                }
            }
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("Error handling for service manager signal",
                            entry("ERROR=%s", e.what()));
        }
    };

    sdbusplus::bus::match_t captureTimeout(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged',path='" +
            serviceMgrKvmObjPath + "',arg0namespace='" + serviceMgrIface + "'",
        std::move(timeoutCallback));

    return captureTimeout;
}

sdbusplus::bus::match_t
    Monitor::powerStatMonitor(std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto pwrStatCallback = [&conn, this](sdbusplus::message_t& msg) {
        try
        {
            std::string interfaceName;
            boost::container::flat_map<std::string, std::variant<std::string>>
                pwrStatProperty;
            msg.read(interfaceName, pwrStatProperty);

            for (const auto& entry : pwrStatProperty)
            {
                if (entry.first == "CurrentPowerState")
                {
                    if (std::get<std::string>(entry.second).find("Off") !=
                        std::string::npos)
                    {
                        hostPowerState = "Off";
                    }
                    else if (std::get<std::string>(entry.second).find("On") !=
                             std::string::npos)
                    {
                        hostPowerState = "On";
                    }
                }
            }
        }
        catch (const sdbusplus::exception::SdBusError& e)
        {
            log<level::ERR>(
                "Failed to handle hostPowerStatusMonitor D-Bus signal",
                entry("ERROR=%s", e.what()));
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("Error handling hostPowerStatusMonitor",
                            entry("ERROR=%s", e.what()));
        }
    };

    sdbusplus::bus::match_t powerStatMatcher(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged',path='" + pwrStatObjPath +
            "'," + "arg0namespace='" + pwrStatIface + "'",
        std::move(pwrStatCallback));

    return powerStatMatcher;
}

sdbusplus::bus::match_t Monitor::monitoringKVMService(
    const std::shared_ptr<sdbusplus::asio::connection> conn)
{
    auto serviceStatusCallback = [&conn](sdbusplus::message_t& msg) {
        try
        {
            std::string interfaceName;
            boost::container::flat_map<std::string, std::variant<bool, uint64_t>>
                properties;
            msg.read(interfaceName, properties);

            if (interfaceName == serviceMgrIface)
            {
                // Check for the Enabled property
                auto enabledIt = properties.find("Enabled");
                if (enabledIt != properties.end())
                {
                    bool enabled = std::get<bool>(enabledIt->second);
                    if (!enabled)
                    {
                        // Update the KVM status
                        kvmStatus = true;
                    }
                }
            }
        }
        catch (const std::exception& e)
        {
            log<level::ERR>("Error handling service manager signal",
                            entry("ERROR=%s", e.what()));
        }
    };

    sdbusplus::bus::match_t monitorKvmService(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged',path='" +
            serviceMgrKvmObjPath + "',arg0namespace='" + serviceMgrIface + "'",
        std::move(serviceStatusCallback));

    return monitorKvmService;
}

} // namespace ikvm
