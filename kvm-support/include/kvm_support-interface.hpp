/*
 * ****************************************************************************
 *
 * Dbus interface for KVM support
 * Filename : kvm_support-interface.hpp
 *
 * @brief Implementation of KVM Dbus Interface
 *
 * Author: Amlana Bhuyan [amlanab@ami.com]
 *
 * ****************************************************************************
 */
#pragma once

#include <chrono>
#include <csignal>
#include <fstream>
#include <future>
#include <iostream>
#include <set>
#include <thread>
#include <vector>

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


#include <iostream>
#include <map>

namespace kvmSupport
{
using namespace phosphor::logging;
namespace fs = std::filesystem;

const std::string ObjPath = "/xyz/openbmc_project/KvmSupport";
const std::string ServiceName = "xyz.openbmc_project.KvmSupport";
const std::string RecObjPath = "/xyz/openbmc_project/KvmSupport/ScreenRecord";

const std::string scrnshotInterface = "xyz.openbmc_project.KvmSupport.Screenshot";
const std::string scrnRecInterface = "xyz.openbmc_project.KvmSupport.ScreenRecord";

const std::string scrnRecTriggInterface = "xyz.openbmc_project.KvmSupport.ScreenRecord.TriggerSettings";
const std::string scrnRecRmtStoreInterface = "xyz.openbmc_project.KvmSupport.ScreenRecord.RemoteStorage";
const std::string scrnRecPreEvntInterface = "xyz.openbmc_project.KvmSupport.ScreenRecord.PreEventRecording";

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

    /*@brief Wrapper function for Dbus Intefaraces intialization */
    void initialize();

    /*@brief  adds dbus Intefarace for Screenshot*/
    void addScreenshotInterface();

    /*@brief  adds dbus Intefarace for Screen Record*/
    void addScreenRecordInterface();
    
    void addscrnRecTriggInterface();
    void addscrnRecRmtStoreInterface();
    void addscrnRecPreEvntInterface();
    /*
     * @brief Implementation of dbus method TriggerScreenshot
     *
     * @param[in] scrnshotReqType - Screenshot Request type
     * currently  value:1 is only valid for trigger
     *
     */
    std::string TriggerScreenshot(int scrnshotReqType);

    std::string TriggerScreenRecord(std::string scrnRecReqType);

  private:
    sdbusplus::asio::object_server& server;
    std::shared_ptr<sdbusplus::asio::dbus_interface> scrnshotIface = nullptr;
    std::shared_ptr<sdbusplus::asio::dbus_interface> ScrnRecIface = nullptr;
    std::shared_ptr<sdbusplus::asio::dbus_interface> scrnRecTriggIface = nullptr;
    std::shared_ptr<sdbusplus::asio::dbus_interface> scrnRecRmtStoreIface = nullptr;
    std::shared_ptr<sdbusplus::asio::dbus_interface> scrnRecPreEvntIface = nullptr;

};

} // namespace ikvm
