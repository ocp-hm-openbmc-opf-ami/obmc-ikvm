// SPDX-License-Identifier: MIT

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#undef FAIL

#include "kvm_dbus-interface.hpp"
#include "kvm_dbus-utils.hpp"

#include <unistd.h>

#include <boost/asio/io_context.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus.hpp>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

namespace kvmDbus
{
namespace
{

const std::string jsonPath = KVM_DBUS_TEST_JSON_PATH;
const std::string mountsPath = KVM_DBUS_TEST_MOUNTS;
const std::string dropinRoot = IKVM_UT_DROPIN_DIR_PREFIX;

nlohmann::json baselineJson()
{
    return {{"Kvm",
             {{"Screenshot", {{"Trigger", false}}},
              {"VideoRecord", {{"RecordStatus", false}}}}},
            {"VideoRecord",
             {{"TriggerSettings",
               {{"TriggeringEvents", 0}, {"Date", ""}, {"Time", ""}}},
              {"RemoteStorage",
               {{"RecordToRemote", false},
                {"MaxDumps", 1},
                {"MaxDuration", 1},
                {"MaxSize", 1},
                {"ServerIP", ""},
                {"PathInServer", ""},
                {"ShareType", "nfs"},
                {"Active", false}}},
              {"PreEventRecording",
               {{"CompressMode", 1},
                {"FPS", 1},
                {"MaxDuration", 1},
                {"VideoQuality", 1}}}}}};
}

void storeJson(const nlohmann::json& value)
{
    std::ofstream(jsonPath) << value.dump(4);
}

int readablePipe(const std::string& content)
{
    int pipeFds[2] = {-1, -1};
    if (pipe(pipeFds) != 0)
    {
        return -1;
    }
    const auto bytes = write(pipeFds[1], content.data(), content.size());
    static_cast<void>(bytes);
    close(pipeFds[1]);
    return pipeFds[0];
}

int unreadablePipe()
{
    int pipeFds[2] = {-1, -1};
    if (pipe(pipeFds) != 0)
    {
        return -1;
    }
    close(pipeFds[0]);
    return pipeFds[1];
}

class InterfaceFixture : public testing::Test
{
  protected:
    void SetUp() override
    {
        storeJson(baselineJson());
        ASSERT_EQ(loadJson(), 0);
        std::ofstream(mountsPath);

        io = std::make_unique<boost::asio::io_context>();
        connection = std::make_shared<sdbusplus::asio::connection>(
            *io, sdbusplus::bus::new_default_user());
        server = std::make_unique<sdbusplus::asio::object_server>(connection);
        ASSERT_NO_THROW(connection->request_name(ServiceName.c_str()));

        testIsMountedFromRemoteHook = {};
        testCreateMountDirectoryHook = {};
        testMountRemoteShareHook = {};
        testUnmountRemoteShareHook = {};
        testSystemdReloadHook = {};
        testSystemdStartUnitHook = {};
    }

    void TearDown() override
    {
        testIsMountedFromRemoteHook = {};
        testCreateMountDirectoryHook = {};
        testMountRemoteShareHook = {};
        testUnmountRemoteShareHook = {};
        testSystemdReloadHook = {};
        testSystemdStartUnitHook = {};
        std::filesystem::remove(jsonPath);
        std::filesystem::remove(mountsPath);
        std::filesystem::remove_all(dropinRoot);
    }

    std::unique_ptr<boost::asio::io_context> io;
    std::shared_ptr<sdbusplus::asio::connection> connection;
    std::unique_ptr<sdbusplus::asio::object_server> server;
};

TEST_F(InterfaceFixture,
       TriggerScreenshot_ValidAndInvalidTypes_AcceptsOnlyValid)
{
    Interface interface(*server);
    interface.addScreenshotInterface();

    EXPECT_EQ(interface.TriggerScreenshot(1), "Success");
    EXPECT_THAT(interface.TriggerScreenshot(2), testing::HasSubstr("Failure"));
}

TEST_F(InterfaceFixture, TriggerVideoRecord_StartAndStopCommands_PersistsStatus)
{
    auto config = baselineJson();
    config["VideoRecord"]["RemoteStorage"]["Active"] = true;
    storeJson(config);
    ASSERT_EQ(loadJson(), 0);

    Interface interface(*server);
    interface.addVideoRecordInterface();
    EXPECT_EQ(interface.TriggerVideoRecord("Start"), "Success");
    ASSERT_EQ(loadJson(), 0);
    EXPECT_TRUE(jsonData["Kvm"]["VideoRecord"]["RecordStatus"]);

    EXPECT_EQ(interface.TriggerVideoRecord("Stop"), "Success");
    ASSERT_EQ(loadJson(), 0);
    EXPECT_FALSE(jsonData["Kvm"]["VideoRecord"]["RecordStatus"]);
}

TEST_F(InterfaceFixture,
       TriggerVideoRecord_InvalidStateAndCommand_RejectsWithError)
{
    Interface interface(*server);
    interface.addVideoRecordInterface();

    EXPECT_THAT(interface.TriggerVideoRecord("Start"),
                testing::HasSubstr("Failure"));
    EXPECT_THAT(interface.TriggerVideoRecord("Stop"),
                testing::HasSubstr("Failure"));
    EXPECT_EQ(interface.TriggerVideoRecord("Bogus"), "Unknown");
}

TEST_F(InterfaceFixture, TriggerVideoRecord_AlreadyRunning_RejectsWithError)
{
    auto config = baselineJson();
    config["VideoRecord"]["RemoteStorage"]["Active"] = true;
    config["Kvm"]["VideoRecord"]["RecordStatus"] = true;
    storeJson(config);
    ASSERT_EQ(loadJson(), 0);

    Interface interface(*server);
    interface.addVideoRecordInterface();
    EXPECT_THAT(interface.TriggerVideoRecord("Start"),
                testing::HasSubstr("already in progress"));
    ASSERT_EQ(loadJson(), 0);
    EXPECT_TRUE(jsonData["Kvm"]["VideoRecord"]["RecordStatus"]);
}

TEST_F(InterfaceFixture, UpdateTriggeringEvents_ValidValue_PersistsToJson)
{
    Interface interface(*server);
    interface.addscrnRecTriggInterface();

    EXPECT_EQ(interface.UpdateTriggeringEvents(0xA5A5), "Success");
    ASSERT_EQ(loadJson(), 0);
    EXPECT_EQ(jsonData["VideoRecord"]["TriggerSettings"]["TriggeringEvents"],
              0xA5A5);
}

TEST_F(InterfaceFixture,
       UpdateTriggerDateTime_NewDateTime_CreatesDropinAndHooksSystemd)
{
    int reloadCalls = 0;
    int startCalls = 0;
    testSystemdReloadHook = [&reloadCalls] { ++reloadCalls; };
    testSystemdStartUnitHook =
        [&startCalls](const std::string& unit, const std::string& mode) {
            EXPECT_EQ(unit, "auto-video-trigger.timer");
            EXPECT_EQ(mode, "replace");
            ++startCalls;
        };

    Interface interface(*server);
    interface.addscrnRecTriggInterface();
    EXPECT_EQ(interface.UpdateTriggerDateTime("2099-12-31", "23:59:59"),
              "Success");
    EXPECT_EQ(reloadCalls, 1);
    EXPECT_EQ(startCalls, 1);

    const auto dropin =
        dropinRoot +
        "/auto-video-trigger.timer.d/10_OnCalendar_Auto_Video.conf";
    std::ifstream input(dropin);
    const std::string content((std::istreambuf_iterator<char>(input)),
                              std::istreambuf_iterator<char>());
    EXPECT_THAT(content, testing::HasSubstr("OnCalendar=2099-12-31 23:59:59"));
}

TEST_F(InterfaceFixture, EnableRemoteStorage_EnableAndDisable_PersistsInJson)
{
    Interface interface(*server);
    interface.addscrnRecRmtStoreInterface();

    EXPECT_EQ(interface.EnableRemoteStorage(true), "Success");
    ASSERT_EQ(loadJson(), 0);
    EXPECT_TRUE(jsonData["VideoRecord"]["RemoteStorage"]["RecordToRemote"]);
    EXPECT_EQ(interface.EnableRemoteStorage(false), "Success");
    ASSERT_EQ(loadJson(), 0);
    EXPECT_FALSE(jsonData["VideoRecord"]["RemoteStorage"]["RecordToRemote"]);
}

TEST_F(InterfaceFixture,
       UpdateRemoteStorageInfo_DisabledOrInvalidParams_RejectsWithError)
{
    Interface interface(*server);
    interface.addscrnRecRmtStoreInterface();
    credentialVariant credential = static_cast<int32_t>(-1);
    EXPECT_THAT(interface.UpdateRemoteStorageInfo(1, 5, 10, "10.0.0.1",
                                                  "/share", "nfs", credential),
                testing::HasSubstr("Failure"));

    auto config = baselineJson();
    config["VideoRecord"]["RemoteStorage"]["RecordToRemote"] = true;
    storeJson(config);
    ASSERT_EQ(loadJson(), 0);
    EXPECT_THAT(interface.UpdateRemoteStorageInfo(2, 0, 150, "10.0.0.1",
                                                  "/share", "bad", credential),
                testing::HasSubstr("invalid values"));
}

TEST_F(InterfaceFixture,
       UpdateRemoteStorageInfo_NfsWithValidParams_MountsAndPersistsConfig)
{
    auto config = baselineJson();
    config["VideoRecord"]["RemoteStorage"]["RecordToRemote"] = true;
    storeJson(config);
    ASSERT_EQ(loadJson(), 0);

    testCreateMountDirectoryHook = [](const std::string& path, int mode) {
        EXPECT_EQ(path, "/tmp/video");
        EXPECT_EQ(mode, 0755);
        return 0;
    };
    testIsMountedFromRemoteHook = [](const std::string&) { return false; };
    testMountRemoteShareHook =
        [](const std::string& source, const std::string& target,
           const std::string& type, unsigned long, const std::string& options) {
            EXPECT_EQ(source, ":/export");
            EXPECT_EQ(target, "/tmp/video");
            EXPECT_EQ(type, "nfs");
            EXPECT_EQ(options, "addr=10.0.0.1");
            return 0;
        };

    Interface interface(*server);
    interface.addscrnRecRmtStoreInterface();
    EXPECT_EQ(
        interface.UpdateRemoteStorageInfo(1, 20, 100, "10.0.0.1", "/export",
                                          "nfs", static_cast<int32_t>(-1)),
        "Success");
    ASSERT_EQ(loadJson(), 0);
    EXPECT_TRUE(jsonData["VideoRecord"]["RemoteStorage"]["Active"]);
    EXPECT_EQ(jsonData["VideoRecord"]["RemoteStorage"]["MaxDuration"], 20);
}

TEST_F(InterfaceFixture,
       UpdateRemoteStorageInfo_NfsWithInvalidCredential_RejectsAndHandlesError)
{
    auto config = baselineJson();
    config["VideoRecord"]["RemoteStorage"]["RecordToRemote"] = true;
    storeJson(config);
    ASSERT_EQ(loadJson(), 0);

    Interface interface(*server);
    interface.addscrnRecRmtStoreInterface();
    EXPECT_THAT(
        interface.UpdateRemoteStorageInfo(1, 5, 10, "10.0.0.1", "/export",
                                          "nfs", static_cast<int32_t>(-2)),
        testing::HasSubstr("expected value: -1 for nfs"));

    testCreateMountDirectoryHook = [](const std::string&, int) { return 0; };
    testIsMountedFromRemoteHook = [](const std::string&) { return true; };
    testUnmountRemoteShareHook = [](const std::string&) {
        errno = EBUSY;
        return -1;
    };
    EXPECT_THAT(
        interface.UpdateRemoteStorageInfo(1, 5, 10, "10.0.0.1", "/export",
                                          "nfs", static_cast<int32_t>(-1)),
        testing::HasSubstr("Device or resource busy"));
}

TEST_F(InterfaceFixture,
       UpdateRemoteStorageInfo_CifsWithMalformedFd_RejectsWithError)
{
    auto config = baselineJson();
    config["VideoRecord"]["RemoteStorage"]["RecordToRemote"] = true;
    storeJson(config);
    ASSERT_EQ(loadJson(), 0);

    Interface interface(*server);
    interface.addscrnRecRmtStoreInterface();
    EXPECT_THAT(
        interface.UpdateRemoteStorageInfo(1, 5, 10, "10.0.0.1", "/share",
                                          "cifs", static_cast<int32_t>(3)),
        testing::HasSubstr("Invalid FD type"));

    const int fd = readablePipe(std::string("user\0pass", 9));
    ASSERT_GE(fd, 0);
    credentialVariant credential = sdbusplus::message::unix_fd{fd};
    EXPECT_THAT(interface.UpdateRemoteStorageInfo(1, 5, 10, "10.0.0.1",
                                                  "/share", "cifs", credential),
                testing::HasSubstr("Malformed extra data"));
    close(fd);
}

TEST_F(InterfaceFixture,
       UpdateRemoteStorageInfo_CifsWithMissingFd_RejectsWithError)
{
    auto config = baselineJson();
    config["VideoRecord"]["RemoteStorage"]["RecordToRemote"] = true;
    storeJson(config);
    ASSERT_EQ(loadJson(), 0);

    Interface interface(*server);
    interface.addscrnRecRmtStoreInterface();
    EXPECT_THAT(
        interface.UpdateRemoteStorageInfo(1, 5, 10, "10.0.0.1", "/share",
                                          "cifs", static_cast<int32_t>(-1)),
        testing::HasSubstr("Invalid Fd provided"));

    const int fd = unreadablePipe();
    ASSERT_GE(fd, 0);
    credentialVariant credential = sdbusplus::message::unix_fd{fd};
    EXPECT_THAT(interface.UpdateRemoteStorageInfo(1, 5, 10, "10.0.0.1",
                                                  "/share", "cifs", credential),
                testing::HasSubstr("Error reading from file descriptor"));
    close(fd);
}

TEST_F(
    InterfaceFixture,
    UpdateRemoteStorageInfo_CifsWithValidCredential_UnmountsAndPersistsConfig)
{
    auto config = baselineJson();
    config["VideoRecord"]["RemoteStorage"]["RecordToRemote"] = true;
    storeJson(config);
    ASSERT_EQ(loadJson(), 0);
    const int fd = readablePipe(std::string("user\0pass\0", 10));
    ASSERT_GE(fd, 0);

    int mountedChecks = 0;
    int unmountCalls = 0;
    testCreateMountDirectoryHook = [](const std::string&, int) { return 0; };
    testIsMountedFromRemoteHook = [&mountedChecks](const std::string&) {
        return mountedChecks++ == 0;
    };
    testUnmountRemoteShareHook = [&unmountCalls](const std::string& path) {
        EXPECT_EQ(path, "/tmp/video");
        ++unmountCalls;
        return 0;
    };
    testMountRemoteShareHook =
        [](const std::string& source, const std::string& target,
           const std::string& type, unsigned long, const std::string& options) {
            EXPECT_EQ(source, "//10.0.0.2/share");
            EXPECT_EQ(target, "/tmp/video");
            EXPECT_EQ(type, "cifs");
            EXPECT_THAT(options,
                        testing::HasSubstr("username=user,password=pass"));
            return 0;
        };

    Interface interface(*server);
    interface.addscrnRecRmtStoreInterface();
    credentialVariant credential = sdbusplus::message::unix_fd{fd};
    EXPECT_EQ(interface.UpdateRemoteStorageInfo(1, 6, 11, "10.0.0.2", "/share",
                                                "cifs", credential),
              "Success");
    EXPECT_EQ(unmountCalls, 1);
    ASSERT_EQ(loadJson(), 0);
    EXPECT_EQ(jsonData["VideoRecord"]["RemoteStorage"]["ShareType"], "cifs");
    EXPECT_TRUE(jsonData["VideoRecord"]["RemoteStorage"]["Active"]);
    close(fd);
}

TEST_F(InterfaceFixture,
       UpdateRemoteStorageInfo_MountDirectoryFails_RunsCleanupAndReturnsError)
{
    auto config = baselineJson();
    config["VideoRecord"]["RemoteStorage"]["RecordToRemote"] = true;
    storeJson(config);
    ASSERT_EQ(loadJson(), 0);

    int unmountCalls = 0;
    testCreateMountDirectoryHook = [](const std::string&, int) {
        errno = EPERM;
        return -1;
    };
    testIsMountedFromRemoteHook = [](const std::string&) { return true; };
    testUnmountRemoteShareHook = [&unmountCalls](const std::string&) {
        ++unmountCalls;
        return 0;
    };

    Interface interface(*server);
    interface.addscrnRecRmtStoreInterface();
    EXPECT_THAT(
        interface.UpdateRemoteStorageInfo(1, 5, 10, "10.0.0.1", "/export",
                                          "nfs", static_cast<int32_t>(-1)),
        testing::HasSubstr("Error creating mount point"));
    EXPECT_EQ(unmountCalls, 1);
}

TEST_F(InterfaceFixture,
       UpdateRemoteStorageInfo_MountFails_ResetsDefaultsAndReturnsError)
{
    auto config = baselineJson();
    config["VideoRecord"]["RemoteStorage"]["RecordToRemote"] = true;
    storeJson(config);
    ASSERT_EQ(loadJson(), 0);
    testCreateMountDirectoryHook = [](const std::string&, int) { return 0; };
    testIsMountedFromRemoteHook = [](const std::string&) { return false; };
    testMountRemoteShareHook =
        [](const std::string&, const std::string&, const std::string&,
           unsigned long, const std::string&) { return -1; };

    Interface interface(*server);
    interface.addscrnRecRmtStoreInterface();
    EXPECT_THAT(
        interface.UpdateRemoteStorageInfo(1, 5, 10, "10.0.0.1", "/export",
                                          "nfs", static_cast<int32_t>(-1)),
        testing::HasSubstr("Failure"));
    ASSERT_EQ(loadJson(), 0);
    EXPECT_EQ(jsonData["VideoRecord"]["RemoteStorage"]["ServerIP"], "");
    EXPECT_EQ(jsonData["VideoRecord"]["RemoteStorage"]["MaxDuration"], 1);
    EXPECT_FALSE(jsonData["VideoRecord"]["RemoteStorage"]["Active"]);
}

TEST_F(InterfaceFixture,
       UpdateRemoteStorageInfo_CleanupThrows_ContainsExceptionAndReturnsError)
{
    auto config = baselineJson();
    config["VideoRecord"]["RemoteStorage"]["RecordToRemote"] = true;
    storeJson(config);
    ASSERT_EQ(loadJson(), 0);
    testCreateMountDirectoryHook = [](const std::string&, int) {
        errno = EPERM;
        return -1;
    };
    testIsMountedFromRemoteHook = [](const std::string&) -> bool {
        throw std::runtime_error("cleanup failure");
    };

    Interface interface(*server);
    interface.addscrnRecRmtStoreInterface();
    EXPECT_NO_THROW({
        EXPECT_THAT(
            interface.UpdateRemoteStorageInfo(1, 5, 10, "10.0.0.1", "/export",
                                              "nfs", static_cast<int32_t>(-1)),
            testing::HasSubstr("Error creating mount point"));
    });
}

class PreEventValidationTest :
    public InterfaceFixture,
    public testing::WithParamInterface<
        std::tuple<uint8_t, uint8_t, uint8_t, uint8_t, const char*>>
{};

TEST_P(PreEventValidationTest,
       UpdatePreEventTriggerInfo_InvalidRanges_RejectsWithErrorMessage)
{
    auto config = baselineJson();
    config["VideoRecord"]["TriggerSettings"]["TriggeringEvents"] =
        1U << triggerEvent::preEventVideoRec;
    storeJson(config);
    ASSERT_EQ(loadJson(), 0);

    Interface interface(*server);
    interface.addscrnRecPreEvntInterface();
    const auto& [compressMode, fps, duration, quality, expected] = GetParam();
    EXPECT_THAT(interface.UpdatePreEventTriggerInfo(compressMode, fps, duration,
                                                    quality),
                testing::HasSubstr(expected));
}

INSTANTIATE_TEST_SUITE_P(
    InvalidFields, PreEventValidationTest,
    testing::Values(std::make_tuple(6, 1, 1, 1, "[arg1]compressMode"),
                    std::make_tuple(1, 0, 1, 1, "[arg2]fps"),
                    std::make_tuple(1, 1, 21, 1, "[arg3]maxDuration"),
                    std::make_tuple(1, 1, 1, 6, "[arg4]videoQuality")));

TEST_F(InterfaceFixture,
       UpdatePreEventTriggerInfo_RequiredBitAndValidValues_PersistsToJson)
{
    Interface interface(*server);
    interface.addscrnRecPreEvntInterface();
    EXPECT_THAT(interface.UpdatePreEventTriggerInfo(2, 2, 5, 3),
                testing::HasSubstr("Failure"));

    auto config = baselineJson();
    config["VideoRecord"]["TriggerSettings"]["TriggeringEvents"] =
        1U << triggerEvent::preEventVideoRec;
    storeJson(config);
    ASSERT_EQ(loadJson(), 0);
    EXPECT_EQ(interface.UpdatePreEventTriggerInfo(2, 2, 5, 3), "Success");
    ASSERT_EQ(loadJson(), 0);
    EXPECT_EQ(jsonData["VideoRecord"]["PreEventRecording"]["VideoQuality"], 3);
}

TEST_F(InterfaceFixture,
       UpdateTriggerDateTime_ExistingDropin_RetainsAndUpdatesContent)
{
    const auto directory = dropinRoot + "/auto-video-trigger.timer.d";
    const auto dropin = directory + "/10_OnCalendar_Auto_Video.conf";
    std::filesystem::create_directories(directory);
    std::ofstream(dropin) << "[Timer]\nOnCalendar=2000-01-01 00:00:00\n"
                             "# retained\n";
    testSystemdReloadHook = [] {};
    testSystemdStartUnitHook = [](const std::string&, const std::string&) {};

    Interface interface(*server);
    interface.addscrnRecTriggInterface();
    EXPECT_EQ(interface.UpdateTriggerDateTime("2099-07-01", "06:45:00"),
              "Success");

    std::ifstream input(dropin);
    const std::string content((std::istreambuf_iterator<char>(input)),
                              std::istreambuf_iterator<char>());
    EXPECT_THAT(content, testing::HasSubstr("# retained"));
    EXPECT_THAT(content, testing::HasSubstr("OnCalendar=2099-07-01 06:45:00"));
    EXPECT_THAT(content,
                testing::Not(testing::HasSubstr("OnCalendar=2000-01-01")));
}

TEST_F(InterfaceFixture, Initialize_ValidJson_WiresAllInterfaces)
{
    auto config = baselineJson();
    config["VideoRecord"]["RemoteStorage"]["Active"] = true;
    storeJson(config);
    ASSERT_EQ(loadJson(), 0);

    Interface interface(*server);
    ASSERT_NO_THROW(interface.initialize());
    EXPECT_EQ(interface.TriggerScreenshot(1), "Success");
    EXPECT_EQ(interface.TriggerVideoRecord("Start"), "Success");
    EXPECT_EQ(interface.UpdateTriggeringEvents(1), "Success");
    EXPECT_EQ(interface.EnableRemoteStorage(true), "Success");
}

TEST_F(InterfaceFixture, Initialize_MissingJson_CompletesWithoutError)
{
    std::filesystem::remove(jsonPath);
    jsonData = nullptr;
    Interface interface(*server);

    EXPECT_NO_THROW(interface.initialize());
}

} // namespace
} // namespace kvmDbus
