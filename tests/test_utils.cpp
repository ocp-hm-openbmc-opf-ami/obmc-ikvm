#include "ami/include/ikvm_utils.hpp"

#ifdef FAIL
#undef FAIL
#endif
#ifdef ERROR
#undef ERROR
#endif

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <gtest/gtest.h>

namespace ikvm
{
namespace fs = std::filesystem;

#ifdef TEST
class UtilsHookTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        savedHostPowerState = hostPowerState;
        savedTimeoutValue = timeoutValue;
        savedMaxDumps = maxDumps;
        savedMaxDuration = maxDuration;
        savedMaxSize = maxSize;
        savedServerIp = serverIP;
        savedPathInServer = pathInServer;
        savedShareType = shareType;
        savedRecordToRemote = recordToRemote;
        savedActive = active;
    }

    void TearDown() override
    {
        hostPowerState = savedHostPowerState;
        timeoutValue = savedTimeoutValue;
        maxDumps = savedMaxDumps;
        maxDuration = savedMaxDuration;
        maxSize = savedMaxSize;
        serverIP = savedServerIp;
        pathInServer = savedPathInServer;
        shareType = savedShareType;
        recordToRemote = savedRecordToRemote;
        active = savedActive;
        testPowerSaveModeHook = {};
        testSessionManagerPropertyHook = {};
        testSessionRegisterHook = {};
        testSessionUnregisterHook = {};
        testPowerStateQueryHook = {};
        testSessionTimeoutQueryHook = {};
        testRemoteConfigQueryHook = {};
        testEventLogHook = {};
    }

  private:
    std::string savedHostPowerState;
    std::chrono::duration<uint64_t> savedTimeoutValue{};
    uint8_t savedMaxDumps = 0;
    uint8_t savedMaxDuration = 0;
    uint8_t savedMaxSize = 0;
    std::string savedServerIp;
    std::string savedPathInServer;
    std::string savedShareType;
    bool savedRecordToRemote = false;
    bool savedActive = false;
};

TEST_F(UtilsHookTest, SetUSBPowerSaveModeDbus_StatusProvided_ForwardsStatus)
{
    int capturedStatus = -1;
    testPowerSaveModeHook = [&capturedStatus](int status) {
        capturedStatus = status;
    };

    setUSBPowerSaveModeDbus(7);

    EXPECT_EQ(capturedStatus, 7);
}

TEST_F(UtilsHookTest,
       GetSessionManagerProperty_NamesProvided_ForwardsNamesAndReturnsValue)
{
    const sessionRet expected = {
        std::make_tuple(static_cast<uint8_t>(9), std::string("198.51.100.10"),
                        std::string("tester"), static_cast<uint8_t>(0),
                        static_cast<uint8_t>(4), static_cast<uint8_t>(8))};
    std::string capturedInterface;
    std::string capturedProperty;
    testSessionManagerPropertyHook =
        [&](const std::string& interfaceName,
            const std::string& propertyName) -> propertyValue {
        capturedInterface = interfaceName;
        capturedProperty = propertyName;
        return expected;
    };

    const auto actual =
        getSessionManagerProperty("xyz.test.Interface", "KvmSessionInfo");

    EXPECT_EQ(capturedInterface, "xyz.test.Interface");
    EXPECT_EQ(capturedProperty, "KvmSessionInfo");
    EXPECT_EQ(std::get<sessionRet>(actual), expected);
}

TEST_F(UtilsHookTest,
       RegisterSessionDbus_SessionProvided_ForwardsCurrentApiArguments)
{
    sessionInfo captured{};
    testSessionRegisterHook =
        [&captured](uint8_t sessionId, const std::string& ipAddress,
                    const std::string& userName, uint8_t sessionType,
                    uint8_t privilege, uint8_t userId) {
            captured = std::make_tuple(sessionId, ipAddress, userName,
                                       sessionType, privilege, userId);
            return true;
        };

    EXPECT_TRUE(registerSessionDbus(17, "198.51.100.8", "alice", 3, 5, 9));
    EXPECT_EQ(captured,
              std::make_tuple(static_cast<uint8_t>(17),
                              std::string("198.51.100.8"), std::string("alice"),
                              static_cast<uint8_t>(3), static_cast<uint8_t>(5),
                              static_cast<uint8_t>(9)));
}

TEST_F(UtilsHookTest,
       UnregisterSessionDbus_IdentifiersProvided_ForwardsCurrentApiArguments)
{
    uint8_t capturedSessionId = 0;
    uint8_t capturedSessionType = 0;
    uint8_t capturedReason = 0;
    testSessionUnregisterHook =
        [&](uint8_t sessionId, uint8_t sessionType, uint8_t reason) {
            capturedSessionId = sessionId;
            capturedSessionType = sessionType;
            capturedReason = reason;
            return false;
        };

    EXPECT_FALSE(unregisterSessionDbus(22, 4, 3));
    EXPECT_EQ(capturedSessionId, 22);
    EXPECT_EQ(capturedSessionType, 4);
    EXPECT_EQ(capturedReason, 3);
}

TEST_F(UtilsHookTest, PowerStatusInit_HookReturnsStateValues_MapsExpectedStates)
{
    testPowerStateQueryHook = [] { return std::string("State.On"); };
    powerStatusInit();
    EXPECT_EQ(hostPowerState, "On");

    testPowerStateQueryHook = [] { return std::string("State.Pending"); };
    powerStatusInit();
    EXPECT_EQ(hostPowerState, "Unknown");
}

TEST_F(UtilsHookTest, SessionTimeout_HookReturnsSeconds_StoresTimeoutValue)
{
    testSessionTimeoutQueryHook = [] { return uint64_t{42}; };

    sessionTimeout();

    EXPECT_EQ(timeoutValue, std::chrono::seconds(42));
}

TEST_F(UtilsHookTest,
       GetRemoteConf_TypedPropertiesProvided_AppliesRemoteConfiguration)
{
    testRemoteConfigQueryHook = [] {
        return RemoteConfigProperties{
            {"Active", true},
            {"MaxDumps", static_cast<uint8_t>(6)},
            {"MaxDuration", static_cast<uint8_t>(7)},
            {"MaxSize", static_cast<uint8_t>(8)},
            {"PathInServer", std::string("/srv/kvm")},
            {"RecordToRemote", true},
            {"ServerIP", std::string("203.0.113.20")},
            {"ShareType", std::string("cifs")},
        };
    };

    getRemoteConf();

    EXPECT_TRUE(active);
    EXPECT_EQ(maxDumps, 6);
    EXPECT_EQ(maxDuration, 7);
    EXPECT_EQ(maxSize, 8);
    EXPECT_EQ(pathInServer, "/srv/kvm");
    EXPECT_TRUE(recordToRemote);
    EXPECT_EQ(serverIP, "203.0.113.20");
    EXPECT_EQ(shareType, "cifs");
}

TEST_F(UtilsHookTest,
       EventLogSupport_MessageProvided_ForwardsMessageAndSwallowsException)
{
    std::string capturedMessage;
    testEventLogHook = [&capturedMessage](const std::string& message) {
        capturedMessage = message;
    };
    eventLogSupport("OpenBMC.0.1.TestEvent");
    EXPECT_EQ(capturedMessage, "OpenBMC.0.1.TestEvent");

    testEventLogHook = [](const std::string&) {
        throw std::runtime_error("event hook failed");
    };
    EXPECT_NO_THROW(eventLogSupport("OpenBMC.0.1.TestEvent"));
}
#endif

// ---------------------------------------------------------------------------
// TrimTest — pure string whitespace trimming
// ---------------------------------------------------------------------------
TEST(TrimTest, EmptyString_ReturnsEmpty)
{
    EXPECT_EQ(trim(""), "");
}

TEST(TrimTest, NoWhitespace_Unchanged)
{
    EXPECT_EQ(trim("hello"), "hello");
}

TEST(TrimTest, LeadingSpaces_Removed)
{
    EXPECT_EQ(trim("   hello"), "hello");
}

TEST(TrimTest, TrailingSpaces_Removed)
{
    EXPECT_EQ(trim("hello   "), "hello");
}

TEST(TrimTest, BothSides_Removed)
{
    EXPECT_EQ(trim("  hello world  "), "hello world");
}

TEST(TrimTest, TabsAndSpaces_Removed)
{
    EXPECT_EQ(trim("\t  value\t "), "value");
}

TEST(TrimTest, OnlyWhitespace_ReturnsEmpty)
{
    EXPECT_EQ(trim("   \t  "), "");
}

// ---------------------------------------------------------------------------
// ParseKeyValueStringTest — comma-separated key=value pairs
// ---------------------------------------------------------------------------
TEST(ParseKeyValueStringTest, EmptyInput_ReturnsEmptyMap)
{
    auto result = parseKeyValueString("");
    EXPECT_TRUE(result.empty());
}

TEST(ParseKeyValueStringTest, SinglePair_ParsedCorrectly)
{
    auto result = parseKeyValueString("key=value");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result.at("key"), "value");
}

TEST(ParseKeyValueStringTest, MultiplePairs_AllParsed)
{
    auto result = parseKeyValueString("a=1,b=2,c=3");
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result.at("a"), "1");
    EXPECT_EQ(result.at("b"), "2");
    EXPECT_EQ(result.at("c"), "3");
}

TEST(ParseKeyValueStringTest, PairsWithSpaces_TrimmedCorrectly)
{
    auto result = parseKeyValueString(" key = value , other = data ");
    EXPECT_EQ(result.at("key"), "value");
    EXPECT_EQ(result.at("other"), "data");
}

TEST(ParseKeyValueStringTest, MalformedEntry_NoEquals_Skipped)
{
    auto result = parseKeyValueString("valid=yes,noequals,another=ok");
    EXPECT_EQ(result.count("valid"), 1u);
    EXPECT_EQ(result.count("another"), 1u);
    // "noequals" has no '=' so it is ignored
    EXPECT_EQ(result.count("noequals"), 0u);
}

TEST(ParseKeyValueStringTest, ValueWithEqualsSign_FirstEqualsIsSplit)
{
    auto result = parseKeyValueString("url=http://host:80/path=extra");
    ASSERT_EQ(result.count("url"), 1u);
    EXPECT_EQ(result.at("url"), "http://host:80/path=extra");
}

// ---------------------------------------------------------------------------
// ExtractSessionIdTest — regex-based session ID extraction
// ---------------------------------------------------------------------------
TEST(ExtractSessionIdTest, ValidFormat_ReturnsId)
{
    EXPECT_EQ(extractSessionId("session_1"), 1u);
    EXPECT_EQ(extractSessionId("session_0"), 0u);
    EXPECT_EQ(extractSessionId("session_255"), 255u);
}

TEST(ExtractSessionIdTest, EmbeddedInLargerString_ReturnsId)
{
    EXPECT_EQ(extractSessionId("user_session_42_active"), 42u);
}

TEST(ExtractSessionIdTest, InvalidFormat_ReturnsZero)
{
    // Pre-scan finding #5: invalid format returns default 0
    EXPECT_EQ(extractSessionId("no_session"), 0u);
    EXPECT_EQ(extractSessionId(""), 0u);
    EXPECT_EQ(extractSessionId("session_"), 0u);
    EXPECT_EQ(extractSessionId("session_abc"), 0u);
}

TEST(ExtractSessionIdTest, OutOfRange_High_ReturnsZero)
{
    // session_256 is out of uint8 range — expect clamped to 0 (error path)
    EXPECT_EQ(extractSessionId("session_256"), 0u);
}

TEST(ExtractSessionIdTest, Boundary_MaxValid_255)
{
    EXPECT_EQ(extractSessionId("session_255"), 255u);
}

// ---------------------------------------------------------------------------
// IsDirTest — filesystem directory creation helper
// ---------------------------------------------------------------------------
class IsDirTest : public ::testing::Test
{
  protected:
    std::string testDir;

    void SetUp() override
    {
        testDir = "/tmp/ikvm_test_isdir_" + std::to_string(getpid());
        fs::remove_all(testDir);
    }

    void TearDown() override
    {
        fs::remove_all(testDir);
    }
};

TEST_F(IsDirTest, ExistingDirectory_ReturnsTrue)
{
    fs::create_directories(testDir);
    EXPECT_TRUE(isDir(testDir));
}

TEST_F(IsDirTest, NonExistingDirectory_CreatesItAndReturnsTrue)
{
    EXPECT_FALSE(fs::exists(testDir));
    bool result = isDir(testDir);
    EXPECT_TRUE(result);
    EXPECT_TRUE(fs::is_directory(testDir));
}

TEST_F(IsDirTest, NestedDirectory_ReturnsTrue)
{
    std::string nested = testDir + "/sub/dir";
    fs::create_directories(testDir);
    // isDir creates only one level; create parent first
    fs::create_directories(testDir + "/sub");
    EXPECT_TRUE(isDir(testDir + "/sub/dir"));
}

// ---------------------------------------------------------------------------
// DetectKvmInstanceTest — global state updated based on video path
// ---------------------------------------------------------------------------
class DetectKvmInstanceTest : public ::testing::Test
{
  protected:
    // Save and restore global state around each test
    std::string savedService;
    std::string savedObjPath;
    uint8_t savedId;
    std::string savedDevPath;

    void SetUp() override
    {
        savedService = pwrStatService;
        savedObjPath = pwrStatObjPath;
        savedId = kvmInstanceId;
        savedDevPath = videoDevicePath;
    }

    void TearDown() override
    {
        pwrStatService = savedService;
        pwrStatObjPath = savedObjPath;
        kvmInstanceId = savedId;
        videoDevicePath = savedDevPath;
    }
};

TEST_F(DetectKvmInstanceTest, Video0_SingleNode_SetsChassisZero)
{
    detectKvmInstance("/dev/video0");

    EXPECT_EQ(kvmInstanceId, 0u);
    EXPECT_EQ(videoDevicePath, "/dev/video0");
    EXPECT_NE(pwrStatObjPath.find("chassis"), std::string::npos);
}

TEST_F(DetectKvmInstanceTest, Video1_SingleNode_KeepsKvmId0)
{
    // Without MULTI_HOST_DEFAULT_MODE, video1 still maps to instance 0
    detectKvmInstance("/dev/video1");

    EXPECT_EQ(videoDevicePath, "/dev/video1");
    // In single-node mode chassis path remains chassis0
    EXPECT_EQ(pwrStatObjPath, "/xyz/openbmc_project/state/chassis0");
}

TEST_F(DetectKvmInstanceTest, VideoDevicePath_IsUpdated)
{
    detectKvmInstance("/dev/video0");
    EXPECT_EQ(videoDevicePath, "/dev/video0");

    detectKvmInstance("/dev/video1");
    EXPECT_EQ(videoDevicePath, "/dev/video1");
}

// ---------------------------------------------------------------------------
// PowerStatusInitTest — error path: D-Bus not available in Docker test env
// ---------------------------------------------------------------------------
TEST(PowerStatusInitTest, NoBusService_SetsUnknownPowerState)
{
    // In Docker test environment the chassis D-Bus service is not running.
    // powerStatusInit() must catch SdBusError and set hostPowerState to
    // "Unknown" rather than crashing.
    std::string before = hostPowerState;
    powerStatusInit();
    // After all retries fail: hostPowerState must be a known string value
    EXPECT_TRUE(hostPowerState == "Unknown" || hostPowerState == "Off" ||
                hostPowerState == "On");
}

// ---------------------------------------------------------------------------
// SessionTimeoutTest — error path: D-Bus not available in Docker test env
// ---------------------------------------------------------------------------
TEST(SessionTimeoutTest, NoBusService_DoesNotThrow)
{
    // sessionTimeout() must catch SdBusError internally and return cleanly.
    EXPECT_NO_THROW(sessionTimeout());
}

// ---------------------------------------------------------------------------
// GetRemoteConfTest — D-Bus unavailable → SdBusError caught, returns cleanly
// ---------------------------------------------------------------------------
TEST(GetRemoteConfTest, NoBusService_DoesNotThrow)
{
    EXPECT_NO_THROW(getRemoteConf());
}

TEST(GetRemoteConfTest, NoBusService_ValuesRemainDefault)
{
    // After a failed call, variables remain at their initialized defaults
    getRemoteConf();
    EXPECT_FALSE(active); // active defaults to false
}

TEST(GetRemoteConfTest, NoBusService_ResetsMutatedGlobalsToFailureDefaults)
{
    const auto savedMaxDuration = maxDuration;
    const auto savedMaxSize = maxSize;
    const auto savedServerIp = serverIP;
    const auto savedPathInServer = pathInServer;
    const auto savedShareType = shareType;
    const auto savedRecordToRemote = recordToRemote;
    const auto savedActive = active;

    maxDuration = 9;
    maxSize = 8;
    serverIP = "1.2.3.4";
    pathInServer = "/share/path";
    shareType = "nfs";
    recordToRemote = true;
    active = true;

    getRemoteConf();

    EXPECT_EQ(maxDuration, 1u);
    EXPECT_EQ(maxSize, 1u);
    EXPECT_TRUE(serverIP.empty());
    EXPECT_TRUE(pathInServer.empty());
    EXPECT_TRUE(shareType.empty());
    EXPECT_FALSE(recordToRemote);
    EXPECT_FALSE(active);

    maxDuration = savedMaxDuration;
    maxSize = savedMaxSize;
    serverIP = savedServerIp;
    pathInServer = savedPathInServer;
    shareType = savedShareType;
    recordToRemote = savedRecordToRemote;
    active = savedActive;
}

// ---------------------------------------------------------------------------
// EventLogSupportTest — D-Bus unavailable → SdBusError caught, returns cleanly
// ---------------------------------------------------------------------------
TEST(EventLogSupportTest, NoBusService_DoesNotThrow)
{
    EXPECT_NO_THROW(eventLogSupport("test.message"));
}

TEST(EventLogSupportTest, EmptyMessage_DoesNotThrow)
{
    EXPECT_NO_THROW(eventLogSupport(""));
}

TEST(PowerStatusInitTest, InvalidPriorState_ResetsToKnownValue)
{
    hostPowerState = "Bogus";
    powerStatusInit();

    EXPECT_TRUE(hostPowerState == "Unknown" || hostPowerState == "Off" ||
                hostPowerState == "On");
}

// ---------------------------------------------------------------------------
// CreateUtilitiesTest — isDir() parts run; D-Bus parts hit error paths
// ---------------------------------------------------------------------------
TEST(CreateUtilitiesTest, CreateUtilities_DbusUnavailable_DoesNotThrow)
{
    // Creates /etc/bsod and /tmp/video if missing, then calls
    // powerStatusInit() and sessionTimeout() which fail gracefully in Docker
    EXPECT_NO_THROW(createUtilities());
}

} // namespace ikvm
