// SPDX-License-Identifier: MIT

#include "kvm_dbus-monitor.hpp"
#include "kvm_dbus-utils.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/container/flat_map.hpp>
#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <tuple>
#include <variant>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace kvmDbus
{
namespace
{

const std::string jsonPath = KVM_DBUS_TEST_JSON_PATH;

class MockRecordInterface
{
  public:
    MOCK_METHOD(void, triggerRecord, (const std::string&));

    void registerWith(sdbusplus::asio::object_server& server)
    {
        interface = server.add_interface("/xyz/openbmc_project/Kvm",
                                         "xyz.openbmc_project.Kvm.VideoRecord");
        interface->register_method("TriggerRecord",
                                   [this](const std::string& type) {
                                       triggerRecord(type);
                                       return response;
                                   });
        interface->initialize();
    }

    std::string response = "Success";
    std::shared_ptr<sdbusplus::asio::dbus_interface> interface;
};

class MockScreenshotInterface
{
  public:
    MOCK_METHOD(void, triggerScreenshot, (int));

    void registerWith(sdbusplus::asio::object_server& server)
    {
        interface = server.add_interface("/xyz/openbmc_project/Kvm",
                                         "xyz.openbmc_project.Kvm.Screenshot");
        interface->register_method("TriggerScreenshot", [this](int type) {
            triggerScreenshot(type);
            return response;
        });
        interface->initialize();
    }

    std::string response = "Success";
    std::shared_ptr<sdbusplus::asio::dbus_interface> interface;
};

sdbusplus::bus_t& asBus(sdbusplus::asio::connection& connection)
{
    return static_cast<sdbusplus::bus_t&>(connection);
}

void storeTriggerEvents(uint32_t events)
{
    std::ofstream(jsonPath) << nlohmann::json{
        {"VideoRecord",
         {{"TriggerSettings",
           {{"TriggeringEvents",
             events}}}}}}.dump(4);
    ASSERT_EQ(loadJson(), 0);
}

template <typename Properties>
void emitPropertiesChanged(sdbusplus::asio::connection& connection,
                           const std::string& path,
                           const std::string& interface,
                           const Properties& properties)
{
    auto message =
        asBus(connection)
            .new_signal(path.c_str(), "org.freedesktop.DBus.Properties",
                        "PropertiesChanged");
    message.append(interface, properties, std::vector<std::string>{});
    message.signal_send();
    asBus(connection).flush();
}

template <typename Value>
void emitSignal(sdbusplus::asio::connection& connection,
                const std::string& path, const std::string& interface,
                const std::string& member, const Value& value)
{
    auto message =
        asBus(connection)
            .new_signal(path.c_str(), interface.c_str(), member.c_str());
    message.append(value);
    message.signal_send();
    asBus(connection).flush();
}

class MonitorFixture : public testing::Test
{
  protected:
    void SetUp() override
    {
        storeTriggerEvents(0xFFFFFFFFU);
        io = std::make_unique<boost::asio::io_context>();
        connection = std::make_shared<sdbusplus::asio::connection>(
            *io, sdbusplus::bus::new_default_user());
        server = std::make_unique<sdbusplus::asio::object_server>(connection);
        ASSERT_NO_THROW(connection->request_name(ServiceName.c_str()));
        record.registerWith(*server);
    }

    void TearDown() override
    {
        std::filesystem::remove(jsonPath);
    }

    void drain()
    {
        io->restart();
        io->run_for(std::chrono::milliseconds(20));
    }

    std::unique_ptr<boost::asio::io_context> io;
    std::shared_ptr<sdbusplus::asio::connection> connection;
    std::unique_ptr<sdbusplus::asio::object_server> server;
    MockRecordInterface record;
};

static_assert(std::is_default_constructible_v<Monitor>);
static_assert(std::is_move_constructible_v<Monitor>);

TEST_F(MonitorFixture, Initialize_ValidJson_RegistersMatchersAndLoadsEvents)
{
    Monitor monitor;
    monitor.initialize(connection);

    EXPECT_EQ(monitor.testMatcherCount(), 14U);
    EXPECT_EQ(monitor.testTriggerEvents(), 0xFFFFFFFFU);
}

TEST_F(MonitorFixture, Initialize_MissingJson_RegistersNoMatchers)
{
    std::filesystem::remove(jsonPath);
    jsonData = nullptr;
    Monitor monitor;

    EXPECT_NO_THROW(monitor.initialize(connection));
    EXPECT_EQ(monitor.testMatcherCount(), 0U);
}

struct ThresholdCase
{
    enum class MonitorType
    {
        tempCritical,
        tempWarning,
        tempNonRecoverable,
        voltCritical,
        voltWarning,
        voltNonRecoverable,
        fanCritical,
        fanWarning,
    };

    MonitorType monitor;
    uint32_t event;
    const char* path;
    const char* interface;
    const char* property;
};

class ThresholdMonitorTest :
    public MonitorFixture,
    public testing::WithParamInterface<ThresholdCase>
{
  protected:
    sdbusplus::bus::match_t makeMatcher(Monitor& monitor)
    {
        switch (GetParam().monitor)
        {
            case ThresholdCase::MonitorType::tempCritical:
                return monitor.tempSensCritMonitor(connection);
            case ThresholdCase::MonitorType::tempWarning:
                return monitor.tempSensNonCritMonitor(connection);
            case ThresholdCase::MonitorType::tempNonRecoverable:
                return monitor.tempSensNonRecovMonitor(connection);
            case ThresholdCase::MonitorType::voltCritical:
                return monitor.voltSensCritMonitor(connection);
            case ThresholdCase::MonitorType::voltWarning:
                return monitor.voltSensNonCritMonitor(connection);
            case ThresholdCase::MonitorType::voltNonRecoverable:
                return monitor.voltSensNonRecovMonitor(connection);
            case ThresholdCase::MonitorType::fanCritical:
                return monitor.fanSensCritMonitor(connection);
            case ThresholdCase::MonitorType::fanWarning:
                return monitor.fanSensWarnMonitor(connection);
        }
        throw std::logic_error("unknown threshold monitor");
    }
};

TEST_P(ThresholdMonitorTest,
       ThresholdMonitor_EnabledAlarmTrue_TriggersRecording)
{
    storeTriggerEvents(1U << GetParam().event);
    EXPECT_CALL(record, triggerRecord("Start")).Times(1);
    Monitor monitor;
    auto matcher = makeMatcher(monitor);
    boost::container::flat_map<std::string, std::variant<bool>> properties;
    properties.emplace(GetParam().property, true);

    emitPropertiesChanged(*connection, GetParam().path, GetParam().interface,
                          properties);
    drain();
}

TEST_P(ThresholdMonitorTest,
       ThresholdMonitor_DisabledOrAlarmFalse_DoesNotTrigger)
{
    storeTriggerEvents(0);
    EXPECT_CALL(record, triggerRecord(testing::_)).Times(0);
    Monitor monitor;
    auto matcher = makeMatcher(monitor);
    boost::container::flat_map<std::string, std::variant<bool>> properties;
    properties.emplace(GetParam().property, true);
    emitPropertiesChanged(*connection, GetParam().path, GetParam().interface,
                          properties);
    drain();

    storeTriggerEvents(1U << GetParam().event);
    properties[GetParam().property] = false;
    emitPropertiesChanged(*connection, GetParam().path, GetParam().interface,
                          properties);
    drain();
}

INSTANTIATE_TEST_SUITE_P(
    CurrentThresholds, ThresholdMonitorTest,
    testing::Values(
        ThresholdCase{ThresholdCase::MonitorType::tempCritical,
                      triggerEvent::criticalTmpVolt,
                      "/xyz/openbmc_project/sensors/temperature/cpu0",
                      "xyz.openbmc_project.Sensor.Threshold.Critical",
                      "CriticalAlarmLow"},
        ThresholdCase{ThresholdCase::MonitorType::tempWarning,
                      triggerEvent::nonCriticalTmpVolt,
                      "/xyz/openbmc_project/sensors/temperature/cpu0",
                      "xyz.openbmc_project.Sensor.Threshold.Warning",
                      "WarningAlarmHigh"},
        ThresholdCase{ThresholdCase::MonitorType::tempNonRecoverable,
                      triggerEvent::nonRecovTmpVolt,
                      "/xyz/openbmc_project/sensors/temperature/cpu0",
                      "xyz.openbmc_project.Sensor.Threshold.NonRecoverable",
                      "NonRecoverableAlarmHigh"},
        ThresholdCase{ThresholdCase::MonitorType::voltCritical,
                      triggerEvent::criticalTmpVolt,
                      "/xyz/openbmc_project/sensors/voltage/vcore",
                      "xyz.openbmc_project.Sensor.Threshold.Critical",
                      "CriticalAlarmHigh"},
        ThresholdCase{ThresholdCase::MonitorType::voltWarning,
                      triggerEvent::nonCriticalTmpVolt,
                      "/xyz/openbmc_project/sensors/voltage/vcore",
                      "xyz.openbmc_project.Sensor.Threshold.Warning",
                      "WarningAlarmLow"},
        ThresholdCase{ThresholdCase::MonitorType::voltNonRecoverable,
                      triggerEvent::nonRecovTmpVolt,
                      "/xyz/openbmc_project/sensors/voltage/vcore",
                      "xyz.openbmc_project.Sensor.Threshold.NonRecoverable",
                      "NonRecoverableAlarmLow"},
        ThresholdCase{ThresholdCase::MonitorType::fanCritical,
                      triggerEvent::fanstatechanged,
                      "/xyz/openbmc_project/sensors/fan_tach/fan0",
                      "xyz.openbmc_project.Sensor.Threshold.Critical",
                      "CriticalAlarmLow"},
        ThresholdCase{ThresholdCase::MonitorType::fanWarning,
                      triggerEvent::fanstatechanged,
                      "/xyz/openbmc_project/sensors/fan_tach/fan0",
                      "xyz.openbmc_project.Sensor.Threshold.Warning",
                      "WarningAlarmLow"}));

TEST_F(MonitorFixture,
       BsodErrorEventMonitor_BsodOffsetEmitted_TriggersScreenshotAndRecording)
{
    MockScreenshotInterface screenshot;
    screenshot.registerWith(*server);
    EXPECT_CALL(record, triggerRecord("Start")).Times(1);
    EXPECT_CALL(screenshot, triggerScreenshot(1)).Times(1);
    Monitor monitor;
    auto matcher = monitor.bsodErrorEventMonitor(connection);
    boost::container::flat_map<std::string, std::variant<uint16_t>> properties;
    properties.emplace("Offset", BSOD);

    emitPropertiesChanged(*connection, bsodObjPathNamespace + "/os0",
                          bsodInterface, properties);
    drain();
}

TEST_F(MonitorFixture,
       BsodErrorEventMonitor_NonBsodOffset_DoesNotTriggerServices)
{
    MockScreenshotInterface screenshot;
    screenshot.registerWith(*server);
    EXPECT_CALL(record, triggerRecord(testing::_)).Times(0);
    EXPECT_CALL(screenshot, triggerScreenshot(testing::_)).Times(0);
    Monitor monitor;
    auto matcher = monitor.bsodErrorEventMonitor(connection);
    boost::container::flat_map<std::string, std::variant<uint16_t>> properties;
    properties.emplace("Offset", static_cast<uint16_t>(1));

    emitPropertiesChanged(*connection, bsodObjPathNamespace + "/os0",
                          bsodInterface, properties);
    drain();
}

TEST_F(MonitorFixture, AsyncRecordTrigger_FailureReply_HandlesWithoutCrash)
{
    record.response = "Failure";
    EXPECT_CALL(record, triggerRecord("Start")).Times(1);
    Monitor monitor;

    monitor.AsyncRecordTrigger(connection, "Start");
    drain();
}

TEST_F(MonitorFixture,
       TempSensCritMonitor_EventsReloadedBetweenSignals_UpdatesInternalState)
{
    storeTriggerEvents(1U << triggerEvent::criticalTmpVolt);
    EXPECT_CALL(record, triggerRecord("Start")).Times(1);
    Monitor monitor;
    auto matcher = monitor.tempSensCritMonitor(connection);
    boost::container::flat_map<std::string, std::variant<bool>> properties;
    properties.emplace("CriticalAlarmHigh", true);

    emitPropertiesChanged(*connection, tempObjPathNamespace + "/temperature0",
                          critInterface, properties);
    drain();
    EXPECT_EQ(monitor.testTriggerEvents(), 1U << triggerEvent::criticalTmpVolt);

    storeTriggerEvents(0);
    emitPropertiesChanged(*connection, tempObjPathNamespace + "/temperature1",
                          critInterface, properties);
    drain();
    EXPECT_EQ(monitor.testTriggerEvents(), 0U);
}

class HostTransitionTest :
    public MonitorFixture,
    public testing::WithParamInterface<std::tuple<uint32_t, const char*>>
{};

TEST_P(HostTransitionTest,
       HostPowerOptMonitor_EnabledTransition_TriggersRecording)
{
    storeTriggerEvents(1U << std::get<0>(GetParam()));
    EXPECT_CALL(record, triggerRecord("Start")).Times(1);
    Monitor monitor;
    auto matcher = monitor.hostPowerOptMonitor(connection);
    boost::container::flat_map<std::string, std::variant<std::string>> props;
    props.emplace("RequestedHostTransition", std::get<1>(GetParam()));

    emitPropertiesChanged(*connection, hostStateObjpath + "/host0",
                          hostStateInterface, props);
    drain();
}

TEST_P(HostTransitionTest,
       HostPowerOptMonitor_DisabledTransition_DoesNotTrigger)
{
    const auto selectedEvent =
        std::get<0>(GetParam()) == triggerEvent::chassisPowerOn
            ? triggerEvent::chassisPowerOff
            : triggerEvent::chassisPowerOn;
    storeTriggerEvents(1U << selectedEvent);
    EXPECT_CALL(record, triggerRecord(testing::_)).Times(0);
    Monitor monitor;
    auto matcher = monitor.hostPowerOptMonitor(connection);
    boost::container::flat_map<std::string, std::variant<std::string>> props;
    props.emplace("RequestedHostTransition", std::get<1>(GetParam()));

    emitPropertiesChanged(*connection, hostStateObjpath + "/host0",
                          hostStateInterface, props);
    drain();
}

INSTANTIATE_TEST_SUITE_P(
    PowerOperations, HostTransitionTest,
    testing::Values(
        std::make_tuple(triggerEvent::chassisPowerOn,
                        "xyz.openbmc_project.State.Host.Transition.On"),
        std::make_tuple(triggerEvent::chassisPowerOff,
                        "xyz.openbmc_project.State.Host.Transition.Off"),
        std::make_tuple(triggerEvent::chassisReset,
                        "xyz.openbmc_project.State.Host.Transition.Reboot")));

TEST_F(MonitorFixture,
       ForcedShutdownAndLpcReset_BothEventsEnabled_TriggersRecordingTwice)
{
    storeTriggerEvents(
        (1U << triggerEvent::chassisPowerOff) | (1U << triggerEvent::lPCReset));
    EXPECT_CALL(record, triggerRecord("Start")).Times(2);
    Monitor monitor;
    auto powerMatcher = monitor.hostForcedShutdownMonitor(connection);
    auto lpcMatcher = monitor.lpcResetMonitor(connection);

    boost::container::flat_map<std::string, std::variant<std::string>> power;
    power.emplace("CurrentPowerState",
                  "xyz.openbmc_project.State.Chassis.PowerState.Off");
    emitPropertiesChanged(*connection, chassisObjpath + "/chassis0",
                          chassisInterface, power);
    drain();

    using PostCode = std::tuple<uint64_t, std::vector<uint8_t>>;
    boost::container::flat_map<std::string, std::variant<PostCode>> postcode;
    postcode.emplace("PostCode",
                     std::make_tuple(123U, std::vector<uint8_t>{1}));
    emitPropertiesChanged(*connection, lpcpath, lpcInterface, postcode);
    drain();
}

TEST_F(MonitorFixture, HostForcedShutdownMonitor_NonOffState_DoesNotTrigger)
{
    storeTriggerEvents(1U << triggerEvent::chassisPowerOff);
    EXPECT_CALL(record, triggerRecord(testing::_)).Times(0);
    Monitor monitor;
    auto matcher = monitor.hostForcedShutdownMonitor(connection);
    boost::container::flat_map<std::string, std::variant<std::string>> props;
    props.emplace("CurrentPowerState",
                  "xyz.openbmc_project.State.Chassis.PowerState.On");
    emitPropertiesChanged(*connection, chassisObjpath + "/chassis0",
                          chassisInterface, props);
    drain();
}

TEST_F(MonitorFixture, LpcResetMonitor_DisabledOrEmptyPostCode_DoesNotTrigger)
{
    EXPECT_CALL(record, triggerRecord(testing::_)).Times(0);
    Monitor monitor;
    auto matcher = monitor.lpcResetMonitor(connection);
    using PostCode = std::tuple<uint64_t, std::vector<uint8_t>>;
    boost::container::flat_map<std::string, std::variant<PostCode>> props;
    props.emplace("PostCode", std::make_tuple(123U, std::vector<uint8_t>{1}));

    storeTriggerEvents(0);
    emitPropertiesChanged(*connection, lpcpath, lpcInterface, props);
    drain();

    storeTriggerEvents(1U << triggerEvent::lPCReset);
    props["PostCode"] = std::make_tuple(123U, std::vector<uint8_t>{});
    emitPropertiesChanged(*connection, lpcpath, lpcInterface, props);
    drain();
}

TEST_F(MonitorFixture, LpcResetMonitor_UnexpectedPropertyType_Ignored)
{
    storeTriggerEvents(1U << triggerEvent::lPCReset);
    EXPECT_CALL(record, triggerRecord(testing::_)).Times(0);
    Monitor monitor;
    auto matcher = monitor.lpcResetMonitor(connection);
    boost::container::flat_map<std::string, std::variant<std::string>> props;
    props.emplace("Data", "unexpected");

    emitPropertiesChanged(*connection, lpcpath, lpcInterface, props);
    drain();
}

TEST_F(MonitorFixture, FanRemovalMonitor_FanRunningSignal_DoesNotTrigger)
{
    storeTriggerEvents(1U << triggerEvent::fanstatechanged);
    EXPECT_CALL(record, triggerRecord(testing::_)).Times(0);
    Monitor monitor;
    auto removalMatcher = monitor.fanRemovalMonitor(connection);

    emitSignal(*connection, fanObjPathNamespace + "/fan0", fanStatusInterface,
               "FanRunning", true);
    drain();
}

TEST_F(MonitorFixture,
       WatchdogTimeoutMonitor_TimeoutSignalWithEventBit_TriggersRecording)
{
    EXPECT_CALL(record, triggerRecord("Start")).Times(1);
    Monitor monitor;
    auto matcher = monitor.watchdogTimeoutMonitor(connection);

    storeTriggerEvents(0);
    emitSignal(*connection, wdogObjPath, wdogInterface, "Timeout",
               "xyz.openbmc_project.State.Watchdog.Action.None");
    drain();

    storeTriggerEvents(1U << triggerEvent::watchdogTimer);
    emitSignal(*connection, wdogObjPath, wdogInterface, "Timeout",
               "xyz.openbmc_project.State.Watchdog.Action.None");
    drain();
}

} // namespace
} // namespace kvmDbus
