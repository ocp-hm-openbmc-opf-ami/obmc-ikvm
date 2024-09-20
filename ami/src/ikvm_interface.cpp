/*
 * ****************************************************************************
 *
 * KVM Dbus interface
 * Filename : ikvm_interface.cpp
 *
 * @brief Implementation of KVM Dbus Interface
 *
 * Author: Amlana Bhuyan [amlanab@ami.com]
 *
 * ****************************************************************************
 */
#include "ami/include/ikvm_interface.hpp"

namespace ikvm
{
Interface::Interface(sdbusplus::asio::object_server& objserver) :
    server(objserver)
{}

void Interface::addInterfaces()
{
    addScreenshotInterface();
}

void Interface::addScreenshotInterface()
{
    kvmScrnshotIface =
        server.add_interface(kvmObjPath.c_str(), scrnshotInterface.c_str());

    kvmScrnshotIface->register_property(
        "Trigger", true, sdbusplus::asio::PropertyPermission::readOnly);

    kvmScrnshotIface->register_method(
        "TriggerScreenshot", [this](int scrnshotReqType) {
            return Interface::TriggerScreenshot(scrnshotReqType);
        });

    kvmScrnshotIface->initialize();
}

std::string Interface::TriggerScreenshot(int scrnshotReqType)
{
    std::string status = "Failure";

    if (scrnshotReqType == 1)
    {
        kvmScrnshotIface->signal_property("Trigger");
        status = "Success";
    }
    else
    {
        status = "Failure :invalid input value, require: 1";
    }

    return status;
}

} // namespace ikvm
