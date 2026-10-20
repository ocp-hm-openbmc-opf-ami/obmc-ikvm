#include "ami/include/ikvm_monitor.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/container/flat_map.hpp>

#include <chrono>
#include <memory>
#include <string>
#include <variant>
#include <vector>

#include <gtest/gtest.h>

namespace ikvm
{
namespace
{
template <typename Properties>
void emitPropertiesChanged(sdbusplus::asio::connection& connection,
                           const std::string& path,
                           const std::string& interface,
                           const Properties& properties)
{
    auto& bus = static_cast<sdbusplus::bus::bus&>(connection);
    auto message = bus.new_signal(
        path.c_str(), "org.freedesktop.DBus.Properties", "PropertiesChanged");
    message.append(interface, properties, std::vector<std::string>{});
    message.signal_send();
    bus.flush();
}

void pump(boost::asio::io_context& io)
{
    io.restart();
    io.run_for(std::chrono::milliseconds(20));
}

class MonitorTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        scrnshotFlag.store(false);
        videoRecFlag.store(false);
        recThreadStatus.store(false);
        activeSessionIDs.clear();
        timeoutValue = std::chrono::seconds(0);
        hostPowerState = "Unknown";
        isKvmDisabled = false;
        active = false;
    }

    void TearDown() override
    {
        scrnshotFlag.store(false);
        videoRecFlag.store(false);
        recThreadStatus.store(false);
        activeSessionIDs.clear();
        isKvmDisabled = false;
        active = false;
    }

    boost::asio::io_context io;
    std::shared_ptr<sdbusplus::asio::connection> connection =
        std::make_shared<sdbusplus::asio::connection>(
            io, sdbusplus::bus::new_default_user());
    Monitor monitor;
};
} // namespace

TEST_F(MonitorTest, Initialize_FlagsSet_RegistersMatchersAndResetsFlags)
{
    scrnshotFlag.store(true);
    videoRecFlag.store(true);
    recThreadStatus.store(true);

    monitor.initialize(connection);

#ifdef MULTI_HOST_DEFAULT_MODE
    EXPECT_EQ(monitor.testMatcherCount(), 5u);
#else
    EXPECT_EQ(monitor.testMatcherCount(), 6u);
#endif
    EXPECT_FALSE(scrnshotFlag.load());
    EXPECT_FALSE(videoRecFlag.load());
    EXPECT_FALSE(recThreadStatus.load());
}

TEST_F(MonitorTest, ScreenshotMonitor_TriggerEmitted_SetsFlag)
{
    auto matcher = monitor.screenshotMonitor(connection);
    boost::container::flat_map<std::string, std::variant<bool>> properties;
    properties.emplace("Trigger", true);

    emitPropertiesChanged(*connection, kvmObjPath, scrnshotInterface,
                          properties);
    pump(io);

    EXPECT_TRUE(scrnshotFlag.load());
}

TEST_F(MonitorTest, SessionMonitor_NewSessions_ReplacesActiveIds)
{
    activeSessionIDs = {1};
    auto matcher = monitor.sessionMonitor(connection);
    sessionRet sessions{
        std::make_tuple(static_cast<uint8_t>(3), std::string("198.51.100.3"),
                        std::string("user3"), static_cast<uint8_t>(0),
                        static_cast<uint8_t>(4), static_cast<uint8_t>(8)),
        std::make_tuple(static_cast<uint8_t>(7), std::string("198.51.100.7"),
                        std::string("user7"), static_cast<uint8_t>(0),
                        static_cast<uint8_t>(4), static_cast<uint8_t>(9)),
    };
    boost::container::flat_map<std::string, propertyValue> properties;
    properties.emplace("KvmSessionInfo", sessions);

    emitPropertiesChanged(*connection, smgrKVMObjPath, smgrKVMIface,
                          properties);
    pump(io);

    EXPECT_EQ(activeSessionIDs, (std::vector<uint8_t>{3, 7}));
}

TEST_F(MonitorTest, SessionTimeout_PropertyEmitted_UpdatesValue)
{
    auto matcher = monitor.sessionTimeout(connection);
    boost::container::flat_map<std::string, std::variant<uint64_t>> properties;
    properties.emplace("SessionTimeOut", uint64_t{3600});

    emitPropertiesChanged(*connection, serviceMgrKvmObjPath, serviceMgrIface,
                          properties);
    pump(io);

    EXPECT_EQ(timeoutValue, std::chrono::seconds(3600));
}

TEST_F(MonitorTest, PowerStatMonitor_StateTransitions_MapsCorrectly)
{
    auto matcher = monitor.powerStatMonitor(connection);
    boost::container::flat_map<std::string, std::variant<std::string>>
        properties;
    properties.emplace("CurrentPowerState", "State.Off");
    emitPropertiesChanged(*connection, pwrStatObjPath, pwrStatIface,
                          properties);
    pump(io);
    EXPECT_EQ(hostPowerState, "Off");

    properties["CurrentPowerState"] = std::string("State.On");
    emitPropertiesChanged(*connection, pwrStatObjPath, pwrStatIface,
                          properties);
    pump(io);
    EXPECT_EQ(hostPowerState, "On");
}

TEST_F(MonitorTest, MonitoringKvmStatus_RunningChanges_UsesOneWayLatch)
{
    auto matcher = monitor.monitoringKvmStatus(connection);
    boost::container::flat_map<std::string, std::variant<bool, uint64_t>>
        properties;
    properties.emplace("Running", false);
    emitPropertiesChanged(*connection, serviceMgrKvmObjPath, serviceMgrIface,
                          properties);
    pump(io);
    ASSERT_TRUE(isKvmDisabled);

    properties["Running"] = true;
    emitPropertiesChanged(*connection, serviceMgrKvmObjPath, serviceMgrIface,
                          properties);
    pump(io);
    EXPECT_TRUE(isKvmDisabled);
}

TEST_F(MonitorTest, VideoRecordMonitor_RecordStatusWithGate_HonorsActiveGate)
{
    auto matcher = monitor.videoRecordMonitor(connection);
    boost::container::flat_map<std::string, std::variant<bool>> properties;
    properties.emplace("RecordStatus", true);

    emitPropertiesChanged(*connection, kvmObjPath, videoRecInterface,
                          properties);
    pump(io);
    EXPECT_FALSE(videoRecFlag.load());

    active = true;
    emitPropertiesChanged(*connection, kvmObjPath, videoRecInterface,
                          properties);
    pump(io);
    EXPECT_TRUE(videoRecFlag.load());

    properties["RecordStatus"] = false;
    emitPropertiesChanged(*connection, kvmObjPath, videoRecInterface,
                          properties);
    pump(io);
    EXPECT_FALSE(videoRecFlag.load());
}
} // namespace ikvm
