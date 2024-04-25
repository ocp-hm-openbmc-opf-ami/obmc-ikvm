/*
 * ****************************************************************
 *
 * KVM server Monitor for dbus async events
 * Filename : ikvm_monitor.cpp
 *
 * @brief Implementation of KVM async dbus signal monitor
 *
 * Author: Amlana Bhuyan [amlanab@ami.com]
 *
 * *****************************************************************
 */

#include "ikvm_monitor.hpp"

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
            std::vector<uint16_t> UpdatedSessionIDs;

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
                            uint16_t sessionID = std::get<0>(tuple);
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
            log<level::ERR>("Error handling Screenshot trigger signal",
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

void Monitor::createUtilities()
{
    isDir(bsodDir);
}

bool Monitor::isDir(const std::string& path)
{
    try
    {
        if (!(fs::is_directory(path.c_str())))
        {
            fs::create_directory(path.c_str());
        }
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Failed to create folder", entry("ERROR=%s", e.what()));
        return false;
    }
    return true;
}

} // namespace ikvm
