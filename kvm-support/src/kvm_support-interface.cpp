/*
 * ****************************************************************************
 *
 * Dbus interface for KVM support
 * Filename : kvm_support-interface.cpp
 *
 * @brief Implementation of KVM Dbus Interface
 *
 * Author: Amlana Bhuyan [amlanab@ami.com]
 *
 * ****************************************************************************
 */

#include "kvm_support-interface.hpp"
namespace kvmSupport
{


Interface::Interface(sdbusplus::asio::object_server& objserver) :
    server(objserver)
{}

void Interface::initialize()
{
    addScreenshotInterface();
    addScreenRecordInterface();
    addscrnRecTriggInterface();
    addscrnRecRmtStoreInterface();
    addscrnRecPreEvntInterface();
}

void Interface::addScreenshotInterface()
{
    scrnshotIface = server.add_interface(ObjPath.c_str(),
                                            scrnshotInterface.c_str());

    scrnshotIface->register_property(
        "Trigger", true, sdbusplus::asio::PropertyPermission::readOnly);

    scrnshotIface->register_method("TriggerScreenshot",
                                      [this](int scrnshotReqType) {
        return Interface::TriggerScreenshot(scrnshotReqType);
    });

    scrnshotIface->initialize();
}

std::string Interface::TriggerScreenshot(int scrnshotReqType)
{
    std::string status = "Failure";

    if (scrnshotReqType == 1)
    {
        scrnshotIface->signal_property("Trigger");
        status = "Success";
    }
    else
    {
        status = "Failure :invalid input value, require: 1";
    }

    return status;
}

void Interface::addScreenRecordInterface()
{
    scrnshotIface = server.add_interface(ObjPath.c_str(),
                                            scrnRecInterface.c_str());

    scrnshotIface->register_property(
        "RecordStatus", false, sdbusplus::asio::PropertyPermission::readOnly);
    
    scrnshotIface->register_method("ScreenRecord",
                                      [this](std::string scrnRecReqType) {
        return Interface::TriggerScreenRecord(scrnRecReqType);
    });

    scrnshotIface->initialize();
}

std::string Interface::TriggerScreenRecord(std::string scrnshotReqType)
{
    std::string status = "Unknown";
    try
    {
        if (scrnshotReqType == "Start")
        {
            scrnshotIface->set_property("Record", true);

            status = "Success";
        }
        else if (scrnshotReqType == "Stop")
        {
            scrnshotIface->set_property("Record", false);
            scrnshotIface->signal_property("Record");
            status = "Success";
        }
    }
    catch (const std::exception& e)
    {
            status = "Failure";
            log<level::ERR>("Error handling BSOD flag signal",
                            entry("ERROR=%s", e.what()));
    }

    return status;
}
void Interface::addscrnRecTriggInterface()
{
    scrnRecTriggIface = server.add_interface(RecObjPath.c_str(),
                                            scrnRecTriggInterface.c_str());

    
    uint32_t initialValue = 0;
    scrnRecTriggIface->register_property<uint32_t>(
        "TriggeringEvents", initialValue, sdbusplus::asio::PropertyPermission::readWrite);

    scrnRecTriggIface->initialize();
}

void Interface::addscrnRecRmtStoreInterface()
{
    scrnRecRmtStoreIface = server.add_interface(RecObjPath.c_str(),
                                            scrnRecRmtStoreInterface.c_str());


    scrnRecRmtStoreIface->register_property(
        "RecordToRemote", false, sdbusplus::asio::PropertyPermission::readWrite); 

    uint8_t InitValue = 10;   
    scrnRecRmtStoreIface->register_property(
        "MaxDuration", InitValue, sdbusplus::asio::PropertyPermission::readWrite);
    
    InitValue = 5;
    scrnRecRmtStoreIface->register_property(
        "Maxsize", InitValue, sdbusplus::asio::PropertyPermission::readWrite);
    
    std::string initialValue = {};
    scrnRecRmtStoreIface->register_property<std::string>(
        "ServerIP", initialValue, sdbusplus::asio::PropertyPermission::readWrite);

    scrnRecRmtStoreIface->register_property<std::string>(
        "PathInServer", initialValue, sdbusplus::asio::PropertyPermission::readWrite);

    scrnRecRmtStoreIface->initialize();
}

void Interface::addscrnRecPreEvntInterface()
{
    scrnRecPreEvntIface = server.add_interface(RecObjPath.c_str(),
                                            scrnRecPreEvntInterface.c_str());

    scrnRecPreEvntIface->register_property(
        "Enable", false, sdbusplus::asio::PropertyPermission::readWrite);

    uint8_t InitValue = 10;
    scrnRecPreEvntIface->register_property(
        "MaxDuration", InitValue, sdbusplus::asio::PropertyPermission::readWrite);

    InitValue = 1;
    scrnRecPreEvntIface->register_property(
        "FPS", InitValue, sdbusplus::asio::PropertyPermission::readWrite);
    InitValue = 0;
    scrnRecPreEvntIface->register_property(
        "VideoQuality", InitValue, sdbusplus::asio::PropertyPermission::readWrite);

    scrnRecPreEvntIface->register_property(
        "CompressMode", InitValue, sdbusplus::asio::PropertyPermission::readWrite);

    scrnRecPreEvntIface->initialize();
}
} // namespace ikvm
