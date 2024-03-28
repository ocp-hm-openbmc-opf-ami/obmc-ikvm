/*
 * ****************************************************************
 *
 * KVM Dbus interface
 * Filename : ikvm_interface.hpp
 *
 * @brief Implementation of KVM Dbus Interface
 *
 * Author: Amlana Bhuyan [amlanab@ami.com]
 *
 * *****************************************************************
 */
#pragma once

#include "ikvm_utils.hpp"

#include <string.h>

#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/message.hpp>
#include <sdbusplus/server/interface.hpp>
#include <sdbusplus/server/object.hpp>

#include <iostream>
#include <map>

namespace ikvm
{

namespace fs = std::filesystem;

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

    /*@brief Wrapper function for Dbus Intefaraces */
    void addInterfaces();

    /*@brief  adds dbus Intefarace for Screenshot*/
    void addScreenshotInterface();

    /*
     * @brief Implementation of dbus method TriggerScreenshot
     *
     * @param[in] scrnshotReqType - Screenshot Request type
     * currently  value:1 is only valid for trigger
     *
     */
    std::string TriggerScreenshot(int scrnshotReqType);

  private:
    sdbusplus::asio::object_server& server;
};

} // namespace ikvm
