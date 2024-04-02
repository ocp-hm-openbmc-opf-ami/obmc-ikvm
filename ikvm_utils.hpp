#pragma once
/*
 * ****************************************************************
 *
 * KVM Utilities
 * Filename : ikvm_utils.hpp
 *
 * @brief Implementation of various utilities
 *  used by different classes under ikvm namespace.
 *
 * Author: Amlana Bhuyan [amlanab@ami.com]
 *
 * *****************************************************************
 */
#include <string.h>

#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus.hpp>
#include <sdbusplus/message.hpp>
#include <sdbusplus/server/interface.hpp>
#include <sdbusplus/server/object.hpp>
#include <xyz/openbmc_project/Common/File/error.hpp>

#include <atomic>
#include <iostream>
#include <map>
#include <memory>

namespace ikvm
{
/*@brief common objectpath for kvm dbus interface */
extern const std::string kvmObjPath;
/*@brief  well-known nam for kvm service */
extern const std::string kvmServiceName;
/*@brief screenshot interface name */
extern const std::string scrnshotInterface;

/*@brief required parameter for BSOD monitor */
extern const std::string bsodObjPath;
extern const std::string bsodTarget;
extern const std::string bsodSennType;

/*@brief Screenshot flag for capturing screenshot*/
extern std::atomic<bool> scrnshotFlag;

/*@brief Screenshot store paths*/
extern const std::string bsodAsJpeg;
extern const std::string bsodDir;

/*@brief pointer to Screenshot interface */
extern std::shared_ptr<sdbusplus::asio::dbus_interface> kvmScrnshotIface;

/*@brief set the time duration for session timeout*/
extern std::chrono::duration<int64_t> timeoutValue;
} // namespace ikvm
