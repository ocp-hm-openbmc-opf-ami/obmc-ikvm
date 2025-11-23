/*
 * ****************************************************************************
 *
 * KVM Dbus interface
 * Filename : kvm_dbus-interface.cpp
 *
 * @brief Implementation of KVM Dbus Interface
 *
 * Author: Amlana Bhuyan [amlanab@ami.com]
 *
 * ****************************************************************************
 */

#include "kvm_dbus-interface.hpp"

#include <linux/videodev2.h>
#include <sys/mount.h>

#include <cstdint>
#include <exception>
#include <stdexcept>
#include <system_error>

namespace kvmDbus
{
Interface::Interface(sdbusplus::asio::object_server& objserver) :
    server(objserver)
{}

void Interface::initialize()
{
    try
    {
        kvmDbus::loadJson();

        addScreenshotInterface();
        addVideoRecordInterface();
        addscrnRecTriggInterface();
        addscrnRecRmtStoreInterface();
        addscrnRecPreEvntInterface();
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Exception caught in interface initialization");
        log<level::ERR>("Error : ", entry("ERROR=%s", e.what()));
        return;
    }
}

void Interface::addScreenshotInterface()
{
    scrnshotIface = server.add_interface(ObjPath.c_str(),
                                         scrnshotInterface.c_str());

    bool initBoolVal = jsonData["Kvm"]["Screenshot"]["Trigger"];

    scrnshotIface->register_property(
        "Trigger", initBoolVal, sdbusplus::asio::PropertyPermission::readOnly);

    scrnshotIface->register_method("TriggerScreenshot",
                                   [this](int scrnshotReqType) {
        return Interface::TriggerScreenshot(scrnshotReqType);
    });

    scrnshotIface->initialize();
}

void Interface::addVideoRecordInterface()
{
    ScrnRecIface = server.add_interface(ObjPath.c_str(),
                                        scrnRecInterface.c_str());

    bool initBoolVal = jsonData["Kvm"]["VideoRecord"]["RecordStatus"];

    ScrnRecIface->register_property(
        "RecordStatus", initBoolVal,
        sdbusplus::asio::PropertyPermission::readOnly);

    ScrnRecIface->register_method("TriggerRecord",
                                  [this](std::string scrnRecReqType) {
        return Interface::TriggerVideoRecord(scrnRecReqType);
    });

    ScrnRecIface->initialize();
}

void Interface::addscrnRecTriggInterface()
{
    scrnRecTriggIface = server.add_interface(RecObjPath.c_str(),
                                             scrnRecTriggInterface.c_str());

    uint32_t initUnit32Val =
        jsonData["VideoRecord"]["TriggerSettings"]["TriggeringEvents"];

    scrnRecTriggIface->register_property<uint32_t>(
        "TriggeringEvents", initUnit32Val,
        sdbusplus::asio::PropertyPermission::readOnly);

    std::string initStrVal = {};

    initStrVal = jsonData["VideoRecord"]["TriggerSettings"]["Date"];
    scrnRecTriggIface->register_property<std::string>(
        "Date", initStrVal, sdbusplus::asio::PropertyPermission::readOnly);

    initStrVal = jsonData["VideoRecord"]["TriggerSettings"]["Time"];
    scrnRecTriggIface->register_property<std::string>(
        "Time", initStrVal, sdbusplus::asio::PropertyPermission::readOnly);

    scrnRecTriggIface->register_method("UpdateTriggeringEvents",
                                       [this](uint32_t triggerEvents) {
        return Interface::UpdateTriggeringEvents(triggerEvents);
    });

    scrnRecTriggIface->register_method(
        "UpdateTriggerDateTime", [this](std::string date, std::string time) {
        return Interface::UpdateTriggerDateTime(date, time);
    });

    scrnRecTriggIface->initialize();
}

void Interface::addscrnRecRmtStoreInterface()
{
    scrnRecRmtStoreIface = server.add_interface(
        RecObjPath.c_str(), scrnRecRmtStoreInterface.c_str());

    bool initBoolVal = false;

    jsonData["VideoRecord"]["RemoteStorage"]["Active"] = initBoolVal;
    // Read existing values from JSON to preserve user settings across reboots
    bool recordToRemoteVal = jsonData["VideoRecord"]["RemoteStorage"]["RecordToRemote"];
    uint8_t maxDumpsVal = jsonData["VideoRecord"]["RemoteStorage"]["MaxDumps"];
    uint8_t maxDurationVal = jsonData["VideoRecord"]["RemoteStorage"]["MaxDuration"];
    uint8_t maxSizeVal = jsonData["VideoRecord"]["RemoteStorage"]["MaxSize"];
    std::string serverIPVal = jsonData["VideoRecord"]["RemoteStorage"]["ServerIP"];
    std::string pathInServerVal = jsonData["VideoRecord"]["RemoteStorage"]["PathInServer"];
    std::string shareTypeVal = jsonData["VideoRecord"]["RemoteStorage"]["ShareType"];

    scrnRecRmtStoreIface->register_property(
        "Active", initBoolVal, sdbusplus::asio::PropertyPermission::readOnly);

    scrnRecRmtStoreIface->register_property(
        "RecordToRemote", recordToRemoteVal,
        sdbusplus::asio::PropertyPermission::readOnly);

    scrnRecRmtStoreIface->register_property(
        "MaxDumps", maxDumpsVal,
        sdbusplus::asio::PropertyPermission::readOnly);

    scrnRecRmtStoreIface->register_property(
        "MaxDuration", maxDurationVal,
        sdbusplus::asio::PropertyPermission::readOnly);

    scrnRecRmtStoreIface->register_property(
        "MaxSize", maxSizeVal, sdbusplus::asio::PropertyPermission::readOnly);

    scrnRecRmtStoreIface->register_property<std::string>(
        "ServerIP", serverIPVal, sdbusplus::asio::PropertyPermission::readOnly);

    scrnRecRmtStoreIface->register_property<std::string>(
        "PathInServer", pathInServerVal,
        sdbusplus::asio::PropertyPermission::readOnly);

    scrnRecRmtStoreIface->register_property<std::string>(
        "ShareType", shareTypeVal, sdbusplus::asio::PropertyPermission::readOnly);

    scrnRecRmtStoreIface->register_method("EnableRemoteStorage",
                                          [this](bool recordToRemote) {
        return Interface::EnableRemoteStorage(recordToRemote);
    });

    scrnRecRmtStoreIface->register_method(
        "UpdateRemoteStorageInfo",
        [this](uint8_t maxDumps, uint8_t maxDuration, uint8_t maxSize,
               std::string serverIP, std::string pathInServer,
               std::string shareType, credentialVariant credFd) {
        return Interface::UpdateRemoteStorageInfo(
            maxDumps, maxDuration, maxSize, serverIP, pathInServer, shareType,
            credFd);
    });

    scrnRecRmtStoreIface->initialize();
}

void Interface::addscrnRecPreEvntInterface()
{
    scrnRecPreEvntIface = server.add_interface(RecObjPath.c_str(),
                                               scrnRecPreEvntInterface.c_str());

    uint8_t initUint8Val = 1;

    initUint8Val = jsonData["VideoRecord"]["PreEventRecording"]["MaxDuration"];
    scrnRecPreEvntIface->register_property(
        "MaxDuration", initUint8Val,
        sdbusplus::asio::PropertyPermission::readOnly);

    initUint8Val = jsonData["VideoRecord"]["PreEventRecording"]["FPS"];
    scrnRecPreEvntIface->register_property(
        "FPS", initUint8Val, sdbusplus::asio::PropertyPermission::readOnly);

    initUint8Val = jsonData["VideoRecord"]["PreEventRecording"]["VideoQuality"];
    scrnRecPreEvntIface->register_property(
        "VideoQuality", initUint8Val,
        sdbusplus::asio::PropertyPermission::readOnly);

    initUint8Val = jsonData["VideoRecord"]["PreEventRecording"]["CompressMode"];
    scrnRecPreEvntIface->register_property(
        "CompressMode", initUint8Val,
        sdbusplus::asio::PropertyPermission::readOnly);

    scrnRecPreEvntIface->register_method(
        "UpdatePreEventTriggerInfo",
        [this](uint8_t compressMode, uint8_t fps, uint8_t maxDuration,
               uint8_t videoQuality) {
        return Interface::UpdatePreEventTriggerInfo(compressMode, fps,
                                                    maxDuration, videoQuality);
    });

    scrnRecPreEvntIface->initialize();
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

std::string Interface::TriggerVideoRecord(std::string videoRecReqType)
{
    std::string status = "Unknown";

    try
    {
        kvmDbus::loadJson();

        if (videoRecReqType == "Start")
        {
            if (jsonData["VideoRecord"]["RemoteStorage"]["Active"] == false)
            {
                throw std::runtime_error(
                    "Remote Storage not Active(Available) requires reconfiguration");
            }

            if (jsonData["Kvm"]["VideoRecord"]["RecordStatus"] == false)
            {
                jsonData["Kvm"]["VideoRecord"]["RecordStatus"] = true;
                kvmDbus::updateJson();
                ScrnRecIface->set_property("RecordStatus", true);
                status = "Success";
            }
            else
            {
                throw std::runtime_error(
                    "Video record already in progress...");
            }
        }
        else if (videoRecReqType == "Stop")
        {
            if (jsonData["Kvm"]["VideoRecord"]["RecordStatus"] == true)
            {
                jsonData["Kvm"]["VideoRecord"]["RecordStatus"] = false;
                kvmDbus::updateJson();
                ScrnRecIface->set_property("RecordStatus", false);

                status = "Success";
            }
            else
            {
                throw std::runtime_error("No video record in progress...");
            }
        }
    }

    catch (const std::exception& e)
    {
        status = "Failure: ";

        log<level::ERR>("Exception caught in handling record flag signal ");
        log<level::ERR>("Error : ", entry("ERROR=%s", e.what()));
        status += e.what();
    }

    return status;
}

std::string Interface::UpdateTriggeringEvents(const uint32_t& triggerEvents)
{
    std::string status = "Unknown";

    try
    {
        kvmDbus::loadJson();
        jsonData["VideoRecord"]["TriggerSettings"]["TriggeringEvents"] =
            triggerEvents;

        kvmDbus::updateJson();

        scrnRecTriggIface->set_property("TriggeringEvents", triggerEvents);
        status = "Success";
    }

    catch (const std::exception& e)
    {
        status = "Failure: ";
        log<level::ERR>("Exception caught in  ");
        log<level::ERR>("Error : ", entry("ERROR=%s", e.what()));
        status += e.what();
    }

    return status;
}

std::string Interface::UpdateTriggerDateTime(std::string date, std::string time)
{
    std::string status = "Unknown";

    try
    {
        kvmDbus::loadJson();

        std::string timer_name = "auto-video-trigger.timer";
        std::string dropin_dir = "/etc/systemd/system/" + timer_name + ".d/";

        std::string dropin_file = dropin_dir + "10_OnCalendar_Auto_Video.conf";
        std::string temp_file = dropin_dir +
                                "10_OnCalendar_Auto_Video.conf.tmp";

        std::string new_datetime = date + " " + time;

        log<level::INFO>("new_datetime recieved");
        log<level::INFO>("Value: ",
                         entry("hostPowerState: %s ", new_datetime.c_str()));

        if (!std::filesystem::exists(dropin_dir))
        {
            std::filesystem::create_directories(dropin_dir);
        }

        std::ifstream in(dropin_file);
        std::ofstream out(temp_file);

        bool onCalendarUpdated = false;

        std::string line;
        while (std::getline(in, line))
        {
            if (line.find("OnCalendar=") != std::string::npos)
            {
                out << "OnCalendar=" << new_datetime << std::endl;
                onCalendarUpdated = true;
            }
            else
            {
                out << line << std::endl;
            }
        }

        if (!onCalendarUpdated)
        {
            out << "[Timer]" << std::endl;
            out << "OnCalendar=" << new_datetime << std::endl;
        }

        in.close();
        out.close();

        // Replacing old drop-in file with the new one
        std::filesystem::rename(temp_file, dropin_file);

        auto bus = sdbusplus::bus::new_system();
        auto reload_call = bus.new_method_call(
            "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
            "org.freedesktop.systemd1.Manager", "Reload");
        bus.call_noreply(reload_call);

        auto start_call = bus.new_method_call(
            "org.freedesktop.systemd1", "/org/freedesktop/systemd1",
            "org.freedesktop.systemd1.Manager", "StartUnit");

        start_call.append(timer_name.c_str(), "replace");

        bus.call_noreply(start_call);

        log<level::INFO>(
            "specific DateTime auto vedio trigger timer initiated successfully ");

        jsonData["VideoRecord"]["TriggerSettings"]["Date"] = date;
        jsonData["VideoRecord"]["TriggerSettings"]["Time"] = time;
        kvmDbus::updateJson();

        scrnRecTriggIface->set_property("Date", date);
        scrnRecTriggIface->set_property("Time", time);
        status = "Success";
    }

    catch (const std::exception& e)
    {
        status = "Failure";
        log<level::ERR>("Exception caught in auto video timer Config.");
        log<level::ERR>("Error : ", entry("ERROR=%s", e.what()));
        status += e.what();
    }

    return status;
}

std::string Interface::EnableRemoteStorage(bool recordToRemote)
{
    std::string status = "Unknown";

    try
    {
        kvmDbus::loadJson();

        jsonData["VideoRecord"]["RemoteStorage"]["RecordToRemote"] =
            recordToRemote;
        kvmDbus::updateJson();
        scrnRecRmtStoreIface->set_property("RecordToRemote", recordToRemote);
        status = "Success";
    }
    catch (const std::exception& e)
    {
        log<level::ERR>("Exception caught in EnableRemoteStorage ");
        log<level::ERR>("Error : ", entry("ERROR=%s", e.what()));
    }
    return status;
}

std::string Interface::UpdateRemoteStorageInfo(
    uint8_t maxDumps, uint8_t maxDuration, uint8_t maxSize,
    std::string serverIP, std::string pathInServer, std::string shareType,
    credentialVariant credFd)
{
    std::string status = "Unknown";
    std::string localPath = "/tmp/video"; // Local mount point

    try
    {
        kvmDbus::loadJson();
        bool isEnabled =
            jsonData["VideoRecord"]["RemoteStorage"]["RecordToRemote"];
        if (isEnabled)
        {
            log<level::DEBUG>("Remote storage enabled... ");

            // Create the target directory if it doesn't exist
            if (mkdir(localPath.c_str(), 0755) == -1 && errno != EEXIST)
            {
                status = "Failure: Error creating mount point";
                // return status;
                throw std::runtime_error(status);
            }

            if (kvmDbus::isMountedFromRemote(localPath))
            {
                log<level::INFO>(
                    "local mount point is mounted with old remote storage");

                if (umount(localPath.c_str()) == 0)
                {
                    log<level::INFO>(
                        " Unmounted successfully: old remote storage");
                }
                else
                {
                    status = "Failure: ";
                    log<level::ERR>("Exception caught in unmounting");
                    status += strerror(errno);
                    return status;
                }
            }

            if ((maxDumps != 1) || (maxDuration < 1 || maxDuration > 20) ||
                (maxSize < 1 || maxSize > 100) ||
                (!(shareType == "nfs" || shareType == "cifs")))
            {
                status = "Failure: invalid values Provided. ";

                if (maxDumps != 1)
                {
                    status += "[arg1]maxDumps: Allowed value 1. ";
                }
                if (maxDuration < 1 || maxDuration > 20)
                {
                    status +=
                        "[arg2]maxDuration: Allowed values range inbetween 1 to 20 (in sec). ";
                }
                if (maxSize < 1 || maxSize > 100)
                {
                    status +=
                        "[arg3]maxDuration: Allowed values range inbetween 1 to 100 (in MB). ";
                }
                if (!(shareType == "nfs" || shareType == "cifs"))
                {
                    status += "[arg6]shareType: Allowed values: nfs, cifs ";
                }
                log<level::ERR>(
                    "local mount point is mounted with old remote storage");
                throw std::runtime_error(status);
            }

            std::string remotePath = "";
            std::string options = "";
            unsigned long mountflags = 0; // Default flags
            int fd = -1;

            if (std::holds_alternative<sdbusplus::message::unix_fd>(credFd))
            {
                fd = std::get<sdbusplus::message::unix_fd>(credFd);
            }
            else
            {
                fd = std::get<int32_t>(credFd);
            }

            if (fd < 0)
            {
                if (shareType == "nfs")
                {
                    if (fd != -1)
                    {
                        log<level::ERR>("file Descriptor Error...");
                        status = "Failure: ";
                        status +=
                            "Invalid Fd provided .expected value: -1 for nfs";
                        throw std::runtime_error(status);
                    }
                }
                else
                {
                    log<level::ERR>("file Descriptor Error...");
                    status = "Failure: ";
                    status += "Invalid Fd provided";

                    throw std::runtime_error(status);
                }
            }

            if (shareType == "cifs")
            {
                std::string user;
                std::string pass;

                if (std::holds_alternative<sdbusplus::message::unix_fd>(credFd))
                {
                    log<level::INFO>("cifs mounting in progress...");
                    std::string buffer(
                        secretLimit, '\0'); // Preallocate with space for data.

                    ssize_t bytesRead = read(
                        fd, &buffer[0],
                        buffer.size() - 1); // Leave space for null terminator.
                    if (bytesRead < 0)
                    {
                        status = "Failure: ";
                        status += "Error reading from file descriptor";
                        log<level::ERR>(
                            "Error reading from file descriptor....");

                        throw std::runtime_error(status);
                    }
                    // Resize the string to the actual number of bytes read.
                    buffer.resize(bytesRead);
                    auto nullCount = std::count(buffer.begin(), buffer.end(),
                                                '\0');

                    if (nullCount != 2)
                    {
                        status = "Failure: ";
                        status += "Malformed extra data";
                        log<level::ERR>("Malformed extra data in credential ");
                        throw std::logic_error(status);
                    }

                    // Extract the username and password using the
                    // null-terminated delimiters.
                    size_t nullPos = buffer.find('\0');
                    std::string username = buffer.substr(0, nullPos);
                    std::string password = buffer.substr(nullPos + 1);

                    options = "username=" + username + ",password=" + password;
                    remotePath = "//" + serverIP + pathInServer;
                    mountflags = 0; // Default flags
                }
                else
                {
                    status = "Failure: ";
                    status += "Invalid FD type";
                    log<level::ERR>("Invalid FD type");

                    throw std::runtime_error(status);
                }
            }
            else
            {
                log<level::INFO>("nfs mounting in progress...");

                options = "addr=" + serverIP;
                remotePath = ":" + pathInServer;
                mountflags = 0; // Default flags
            }

            // Perform the mount
            auto ec = mount(remotePath.c_str(), localPath.c_str(),
                            shareType.c_str(), mountflags, options.c_str());
            if (ec)
            {
                status = "Failure: ";
                log<level::ERR>("Error caught while mounting");

                status += strerror(errno);
                throw std::runtime_error(status);
            }

            jsonData["VideoRecord"]["RemoteStorage"]["MaxDumps"] = maxDumps;
            jsonData["VideoRecord"]["RemoteStorage"]["MaxDuration"] =
                maxDuration;
            jsonData["VideoRecord"]["RemoteStorage"]["MaxSize"] = maxSize;
            jsonData["VideoRecord"]["RemoteStorage"]["ServerIP"] = serverIP;
            jsonData["VideoRecord"]["RemoteStorage"]["PathInServer"] =
                pathInServer;
            jsonData["VideoRecord"]["RemoteStorage"]["ShareType"] = shareType;
            jsonData["VideoRecord"]["RemoteStorage"]["Active"] = true;

            kvmDbus::updateJson();

            scrnRecRmtStoreIface->set_property("MaxDumps", maxDumps);
            scrnRecRmtStoreIface->set_property("MaxDuration", maxDuration);
            scrnRecRmtStoreIface->set_property("MaxSize", maxSize);
            scrnRecRmtStoreIface->set_property("ServerIP", serverIP);
            scrnRecRmtStoreIface->set_property("PathInServer", pathInServer);
            scrnRecRmtStoreIface->set_property("ShareType", shareType);
            scrnRecRmtStoreIface->set_property("Active", true);
            status = "Success";
        }
        else
        {
            status = "Failure:";
            status += "Record To Remote not enabled...";
            log<level::ERR>("Record To Remote not enabled...");

            throw std::runtime_error(status);
        }
    }

    catch (const std::exception& e)
    {
        try
        {
            log<level::ERR>("Exception caught in Remote Storage Update.");
            log<level::ERR>("Error : ", entry("ERROR=%s", e.what()));

            std::string resetStr = {};
            uint8_t resetVal = 1;
            bool resetboolVal = false;
            kvmDbus::loadJson();

            if (kvmDbus::isMountedFromRemote(localPath))
            {
                log<level::INFO>(
                    "local mount point is mounted with old remote storage");
                if (umount(localPath.c_str()) == 0)
                {
                    log<level::INFO>(
                        " Unmounted successfully: old remote storage");
                }
            }

            resetStr = "";
            jsonData["VideoRecord"]["RemoteStorage"]["MaxDumps"] = resetVal;
            scrnRecRmtStoreIface->set_property("MaxDumps", resetVal);

            jsonData["VideoRecord"]["RemoteStorage"]["MaxDuration"] = resetVal;
            scrnRecRmtStoreIface->set_property("MaxDuration", resetVal);

            jsonData["VideoRecord"]["RemoteStorage"]["MaxSize"] = resetVal;
            scrnRecRmtStoreIface->set_property("MaxSize", resetVal);

            jsonData["VideoRecord"]["RemoteStorage"]["ServerIP"] = resetStr;
            scrnRecRmtStoreIface->set_property("ServerIP", resetStr);

            jsonData["VideoRecord"]["RemoteStorage"]["PathInServer"] = resetStr;
            scrnRecRmtStoreIface->set_property("PathInServer", resetStr);

            resetStr = "nfs";
            jsonData["VideoRecord"]["RemoteStorage"]["ShareType"] = resetStr;
            scrnRecRmtStoreIface->set_property("ShareType", resetStr);

            jsonData["VideoRecord"]["RemoteStorage"]["Active"] = resetboolVal;
            scrnRecRmtStoreIface->set_property("Active", resetboolVal);

            kvmDbus::updateJson();
        }
        catch (const std::exception& ec)
        {
            log<level::ERR>(" Error in Exception handling");
            log<level::ERR>("Error : ", entry("ERROR=%s", ec.what()));
        }
    }
    return status;
}

std::string Interface::UpdatePreEventTriggerInfo(uint8_t compressMode,
                                                 uint8_t fps,
                                                 uint8_t maxDuration,
                                                 uint8_t videoQuality)
{
    std::string status = "Unknown";

    try
    {
        kvmDbus::loadJson();

        triggerEvents32 trgEvnts = (uint32_t)
            jsonData["VideoRecord"]["TriggerSettings"]["TriggeringEvents"];

        if (trgEvnts.test(triggerEvent::preEventVideoRec))
        {
            if ((compressMode < 1 || compressMode > 5) ||
                (fps < 1 || fps > 4) || (maxDuration < 1 || maxDuration > 20) ||
                (videoQuality < 1 || videoQuality > 5))
            {
                status = "Failure: invalid values Provided. ";

                if (compressMode < 1 || compressMode > 5)
                {
                    status +=
                        "[arg1]compressMode: Allowed values range inbetween 1 to 5. ";
                }
                if (fps < 1 || fps > 4)
                {
                    status +=
                        "[arg2]fps: Allowed values range inbetween 1 to 4. ";
                }
                if (maxDuration < 1 || maxDuration > 20)
                {
                    status +=
                        "[arg3]maxDuration: Allowed values range inbetween 1 to 20(in sec). ";
                }
                if (videoQuality < 1 || videoQuality > 5)
                {
                    status +=
                        "[arg4]videoQuality: Allowed values range inbetween 1 to 5. ";
                }

                return status;
            }

            jsonData["VideoRecord"]["PreEventRecording"]["CompressMode"] =
                compressMode;
            jsonData["VideoRecord"]["PreEventRecording"]["FPS"] = fps;
            jsonData["VideoRecord"]["PreEventRecording"]["MaxDuration"] =
                maxDuration;
            jsonData["VideoRecord"]["PreEventRecording"]["VideoQuality"] =
                videoQuality;

            kvmDbus::updateJson();

            scrnRecPreEvntIface->set_property("CompressMode", compressMode);
            scrnRecPreEvntIface->set_property("FPS", fps);
            scrnRecPreEvntIface->set_property("MaxDuration", maxDuration);
            scrnRecPreEvntIface->set_property("VideoQuality", videoQuality);

            status = "Success";
        }
        else
        {
            status = "Failure: preEvent trigger not enabled...";
        }
    }
    catch (const std::exception& e)
    {
        status = "Failure: ";
        log<level::ERR>("Exception caught in Pre event triger info update");
        log<level::ERR>("Error : ", entry("ERROR=%s", e.what()));
        status += e.what();
    }

    return status;
}

} // namespace kvmDbus
