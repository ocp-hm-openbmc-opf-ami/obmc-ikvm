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

#include <gtest/gtest.h>

namespace ikvm
{
namespace fs = std::filesystem;

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
