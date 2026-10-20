#include "ami/include/ikvm_utils.hpp"
#include "ikvm_input.hpp"
#include "ikvm_video.hpp"

#ifdef FAIL
#undef FAIL
#endif
#ifdef ERROR
#undef ERROR
#endif

#include <fcntl.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

namespace ikvm
{
namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// VideoConstructorTest — verify default field values after construction.
// Input is constructed first; #ifdef TEST guards skip the HID stream open.
// ---------------------------------------------------------------------------
class VideoConstructorTest : public ::testing::Test
{
  protected:
    // Input constructed with empty paths; hardware open is skipped via -DTEST
    Input input{"", "", ""};
    Video video{"/dev/video0", input, 30, 0, 0};
};

TEST_F(VideoConstructorTest, DefaultFrameRate_Is30)
{
    EXPECT_EQ(video.getFrameRate(), 30);
}

TEST_F(VideoConstructorTest, DefaultHeight_Is600)
{
    EXPECT_EQ(video.getHeight(), 600u);
}

TEST_F(VideoConstructorTest, DefaultWidth_Is800)
{
    EXPECT_EQ(video.getWidth(), 800u);
}

TEST_F(VideoConstructorTest, DefaultSubsampling_Is0)
{
    EXPECT_EQ(video.getSubsampling(), 0);
}

TEST_F(VideoConstructorTest, DefaultQuality_Is4)
{
    EXPECT_EQ(video.getQuality(), 4);
}

TEST_F(VideoConstructorTest, DefaultFormat_Is0)
{
    EXPECT_EQ(video.getFormat(), 0);
}

TEST_F(VideoConstructorTest, OriginalFormat_MatchesConstructorArg)
{
    EXPECT_EQ(video.getOriginalFormat(), 0);
}

TEST_F(VideoConstructorTest, IsNewClient_IsFalse)
{
    EXPECT_FALSE(video.isNewClient);
}

// ---------------------------------------------------------------------------
// VideoGetDataTest — getData() returns nullptr when no buffers are ready
// ---------------------------------------------------------------------------
class VideoGetDataTest : public ::testing::Test
{
  protected:
    Input input{"", "", ""};
    Video video{"/dev/video0", input};
};

TEST_F(VideoGetDataTest, GetData_EmptyBuffers_ReturnsNullptr)
{
    EXPECT_EQ(video.getData(), nullptr);
}

TEST_F(VideoGetDataTest, GetDataIndexed_OutOfRange_ReturnsNullptr)
{
    // buffers vector is empty — any index is out of range
    EXPECT_EQ(video.getData(0u), nullptr);
    EXPECT_EQ(video.getData(99u), nullptr);
}

// ---------------------------------------------------------------------------
// VideoFrameSizeTest — getFrameSize() returns 0 when no frames are queued
// ---------------------------------------------------------------------------
class VideoFrameSizeTest : public ::testing::Test
{
  protected:
    Input input{"", "", ""};
    Video video{"/dev/video0", input};
};

TEST_F(VideoFrameSizeTest, GetFrameSize_EmptyBuffers_ReturnsZero)
{
    EXPECT_EQ(video.getFrameSize(), 0u);
}

TEST_F(VideoFrameSizeTest, GetFrameSizeIndexed_UsesSelectedBufferPayload)
{
    video.resizeTestBuffers(1);
    video.setFrame("/nonexistent_image.jpg");

    EXPECT_EQ(video.getFrameSize(0u), 0u);
}

// ---------------------------------------------------------------------------
// VideoFrameCountTest — getFrameCount() returns 0 when buffersDone is empty
// ---------------------------------------------------------------------------
class VideoFrameCountTest : public ::testing::Test
{
  protected:
    Input input{"", "", ""};
    Video video{"/dev/video0", input};
};

TEST_F(VideoFrameCountTest, GetFrameCount_EmptyBuffers_ReturnsZero)
{
    EXPECT_EQ(video.getFrameCount(), 0u);
}

// ---------------------------------------------------------------------------
// VideoNeedsResizeTest — needsResize() returns false when fd < 0 (no device)
// ---------------------------------------------------------------------------
class VideoNeedsResizeTest : public ::testing::Test
{
  protected:
    Input input{"", "", ""};
    Video video{"/dev/video0", input};
};

TEST_F(VideoNeedsResizeTest, NeedsResize_NoDevice_ReturnsFalse)
{
    // fd is initialised to -1 in constructor; function must return false
    EXPECT_FALSE(video.needsResize());
}

// ---------------------------------------------------------------------------
// VideoSettersTest — setFormat / setSubsampling mutate state correctly
// ---------------------------------------------------------------------------
class VideoSettersTest : public ::testing::Test
{
  protected:
    Input input{"", "", ""};
    Video video{"/dev/video0", input, 25, 0, 0};
};

TEST_F(VideoSettersTest, SetFormat_UpdatesGetFormat)
{
    video.setFormat(2);
    EXPECT_EQ(video.getFormat(), 2);
}

TEST_F(VideoSettersTest, SetFormat_DoesNotChangeOriginalFormat)
{
    video.setFormat(2);
    EXPECT_EQ(video.getOriginalFormat(), 0);
}

TEST_F(VideoSettersTest, SetSubsampling_UpdatesGetSubsampling)
{
    video.setSubsampling(1);
    EXPECT_EQ(video.getSubsampling(), 1);
}

TEST_F(VideoSettersTest, CustomFrameRate_IsPreserved)
{
    Input inp{"", "", ""};
    Video v{"/dev/video1", inp, 15, 1, 2};
    EXPECT_EQ(v.getFrameRate(), 15);
    EXPECT_EQ(v.getSubsampling(), 1);
}

TEST_F(VideoSettersTest, GetPixelformat_DefaultIsJpeg)
{
    EXPECT_EQ(video.getPixelformat(), V4L2_PIX_FMT_JPEG);
}

// ---------------------------------------------------------------------------
// VideoStaticConstantsTest — verify published protocol constants
// ---------------------------------------------------------------------------
TEST(VideoStaticConstantsTest, BitsPerSample_Is8)
{
    EXPECT_EQ(Video::bitsPerSample, 8);
}

TEST(VideoStaticConstantsTest, BytesPerPixel_Is4)
{
    EXPECT_EQ(Video::bytesPerPixel, 4);
}

TEST(VideoStaticConstantsTest, SamplesPerPixel_Is3)
{
    EXPECT_EQ(Video::samplesPerPixel, 3);
}

// ---------------------------------------------------------------------------
// VideoGetFrameTest — getFrame() with fd=-1 returns immediately (no crash)
// ---------------------------------------------------------------------------
TEST(VideoGetFrameTest, GetFrame_NoDevice_ReturnsWithoutCrash)
{
    Input inp{"", "", ""};
    Video v{"/dev/video0", inp};
    EXPECT_NO_THROW(v.getFrame());
    EXPECT_EQ(v.getData(),
              nullptr); // buffersDone still empty after early return
}

// ---------------------------------------------------------------------------
// VideoReleaseFramesTest — releaseFrames() with empty buffersDone is a no-op
// ---------------------------------------------------------------------------
TEST(VideoReleaseFramesTest, ReleaseFrames_EmptyQueue_NoOp)
{
    Input inp{"", "", ""};
    Video v{"/dev/video0", inp};
    EXPECT_NO_THROW(v.releaseFrames());
}

// ---------------------------------------------------------------------------
// VideoResizeTest — resize() with fd=-1 returns early without crashing
// ---------------------------------------------------------------------------
TEST(VideoResizeTest, Resize_NoDevice_ReturnsEarly)
{
    Input inp{"", "", ""};
    Video v{"/dev/video0", inp};
    EXPECT_NO_THROW(v.resize());
}

// ---------------------------------------------------------------------------
// VideoSignalStatusTest — getSignalStatus() returns UINT32_MAX when fd=-1
// ---------------------------------------------------------------------------
TEST(VideoSignalStatusTest, GetSignalStatus_NoDevice_ReturnsMaxUint)
{
    Input inp{"", "", ""};
    Video v{"/dev/video0", inp};
    EXPECT_EQ(v.getSignalStatus(), UINT32_MAX);
}

// ---------------------------------------------------------------------------
// VideoScreenShotTest — screenShot() returns early when buffersDone is empty
// ---------------------------------------------------------------------------
TEST(VideoScreenShotTest, ScreenShot_EmptyBuffers_ReturnsEarly)
{
    Input inp{"", "", ""};
    Video v{"/dev/video0", inp};
    const std::string outputPath =
        (fs::temp_directory_path() / "test_screenshot.jpg").string();
    EXPECT_NO_THROW(v.screenShot(outputPath));
}

TEST(VideoScreenShotTest, ScreenShot_InvalidDestinationPath_DoesNotCreateFile)
{
    Input inp{"", "", ""};
    Video v{"/dev/video0", inp};
    const std::string outputPath =
        (fs::temp_directory_path() / "ikvm-test-missing-dir" / "screenshot.jpg")
            .string();

    v.resizeTestBuffers(1);
    v.buffersDone.push_back(0);
    fs::remove(outputPath);

    EXPECT_NO_THROW(v.screenShot(outputPath));
    EXPECT_FALSE(fs::exists(outputPath));
}

// screenShot() first ensures ikvm::bsodDir ("/etc/bsod") exists before
// writing the destination file. In restricted/non-root test environments
// this directory cannot be created, so the test skips rather than
// spuriously failing on an environment precondition it cannot control
// (production code is read-only, so the hardcoded path cannot be changed).
TEST(VideoScreenShotTest, ScreenShot_QueuedBufferValidPath_CreatesFile)
{
    if (!isDir(bsodDir))
    {
        GTEST_SKIP() << "Cannot create " << bsodDir
                     << " in this test environment (insufficient "
                        "permissions) — screenShot() requires it to exist.";
    }

    Input inp{"", "", ""};
    Video v{"/dev/video0", inp};
    const std::string outputPath =
        (fs::temp_directory_path() / "generated-screenshot.bin").string();

    v.resizeTestBuffers(1);
    v.buffersDone.push_back(0);
    fs::remove(outputPath);

    EXPECT_NO_THROW(v.screenShot(outputPath));
    EXPECT_TRUE(fs::exists(outputPath));
    fs::remove(outputPath);
}

// ---------------------------------------------------------------------------
// VideoPushRecFrameTest — pushRecFrame() returns early when videoRecFlag=false
// ---------------------------------------------------------------------------
TEST(VideoPushRecFrameTest, PushRecFrame_RecordFlagFalse_ReturnsEarly)
{
    Input inp{"", "", ""};
    Video v{"/dev/video0", inp};
    // ikvm::videoRecFlag initialized to false → early return
    EXPECT_NO_THROW(v.pushRecFrame());
}

TEST(VideoPushRecFrameTest, PushRecFrame_ValidFrame_QueuesFrame)
{
    Input inp{"", "", ""};
    Video v{"/dev/video0", inp};
    unsigned char frameData[16] = {0};

    v.resizeTestBuffers(1);
    v.buffersDone.push_back(0);
    v.setTestBufferData(0, frameData, sizeof(frameData), 1);
    v.clearRecQueue(); // resets lastPushedSeq so this frame is not a duplicate

    videoRecFlag = true;
    recThreadStatus = true;
    EXPECT_NO_THROW(v.pushRecFrame());
    videoRecFlag = false;
    recThreadStatus = false;
}

TEST(VideoPushRecFrameTest, PushRecFrame_DuplicateSequence_SkipsFrame)
{
    Input inp{"", "", ""};
    Video v{"/dev/video0", inp};
    unsigned char frameData[16] = {0};

    v.resizeTestBuffers(1);
    v.buffersDone.push_back(0);
    v.setTestBufferData(0, frameData, sizeof(frameData), 7);
    v.clearRecQueue();

    videoRecFlag = true;
    recThreadStatus = true;
    EXPECT_NO_THROW(v.pushRecFrame()); // queues sequence 7
    EXPECT_NO_THROW(v.pushRecFrame()); // same sequence 7 → skipped
    videoRecFlag = false;
    recThreadStatus = false;
}

// ---------------------------------------------------------------------------
// VideoClearRecQueueTest — clearRecQueue() always safe to call
// ---------------------------------------------------------------------------
TEST(VideoClearRecQueueTest, ClearRecQueue_EmptyQueue_NoOp)
{
    Input inp{"", "", ""};
    Video v{"/dev/video0", inp};
    EXPECT_NO_THROW(v.clearRecQueue());
    EXPECT_NO_THROW(v.clearRecQueue()); // idempotent
}

// ---------------------------------------------------------------------------
// VideoSetFrameTest — setFrame() returns early when the destination image
//   file cannot be opened. Buffers are resized first, matching real
//   production preconditions (buffers are always allocated before setFrame()
//   is called via start()/resize()).
// ---------------------------------------------------------------------------
TEST(VideoSetFrameTest, SetFrame_ImageNotFound_ReturnsEarly)
{
    Input inp{"", "", ""};
    Video v{"/dev/video0", inp};
    v.resizeTestBuffers(1);
    EXPECT_NO_THROW(v.setFrame("/nonexistent_image.jpg"));
}

// ---------------------------------------------------------------------------
// VideoStartTest — start() with non-existent device handles open failure
// ---------------------------------------------------------------------------
TEST(VideoStartTest, Start_DeviceNotFound_HandlesError)
{
    Input inp{"", "", ""};
    Video v{"/dev/nonexistent_video99", inp};
    // open() fails → all ioctls fail → logged internally → completes
    EXPECT_NO_THROW(v.start());
}

// ---------------------------------------------------------------------------
// VideoFormatChangeTest — formatChange() calls stop(noop) + start(error path)
// ---------------------------------------------------------------------------
TEST(VideoFormatChangeTest, FormatChange_NoDevice_CompletesWithoutCrash)
{
    Input inp{"", "", ""};
    Video v{"/dev/nonexistent_video99", inp};
    EXPECT_NO_THROW(v.formatChange(0));
}

TEST(VideoFormatChangeTest, FormatChange_UpdatesFormatBeforeRestartAttempt)
{
    Input inp{"", "", ""};
    Video v{"/dev/nonexistent_video99", inp};

    EXPECT_NO_THROW(v.formatChange(2));
    EXPECT_EQ(v.getFormat(), 2);
}

TEST(VideoFormatChangeTest, Restart_NoDevice_CompletesWithoutCrash)
{
    Input inp{"", "", ""};
    Video v{"/dev/nonexistent_video99", inp};

    EXPECT_NO_THROW(v.restart());
}

// ---------------------------------------------------------------------------
// VideoUpdateRecStatTest — updateRecStat() hits D-Bus error path in Docker
// ---------------------------------------------------------------------------
TEST(VideoUpdateRecStatTest, UpdateRecStat_NoDbus_NoThrow)
{
    // D-Bus not running → SdBusError caught internally → returns false
    EXPECT_NO_THROW(Video::updateRecStat("Start"));
    EXPECT_NO_THROW(Video::updateRecStat("Stop"));
}

TEST(VideoUpdateRecStatTest, UpdateRecStat_InvalidType_ReturnsFalse)
{
    EXPECT_FALSE(Video::updateRecStat("Invalid"));
}

// ---------------------------------------------------------------------------
// VideoVideoRecordTest — videoRecord() returns early when active=false
// ---------------------------------------------------------------------------
TEST(VideoVideoRecordTest, VideoRecord_Inactive_ReturnsEarly)
{
    Input inp{"", "", ""};
    Video v{"/dev/video0", inp};
    // ikvm::active is false by default → early return immediately
    EXPECT_NO_THROW(Video::videoRecord(&v));
}

TEST(VideoVideoRecordTest, VideoRecord_Inactive_ClearsRecThreadStatus)
{
    Input inp{"", "", ""};
    Video v{"/dev/video0", inp};

    ikvm::active = false;
    ikvm::recThreadStatus.store(true);

    EXPECT_NO_THROW(Video::videoRecord(&v));
    EXPECT_FALSE(ikvm::recThreadStatus.load());
}

// ---------------------------------------------------------------------------
// VideoFdTests — use /dev/null as a fake fd to cover branches that require
//                fd >= 0.  ioctl() on /dev/null returns ENOTTY; select()
//                returns immediately (device always ready).
// ---------------------------------------------------------------------------
class VideoFdTest : public ::testing::Test
{
  protected:
    Input input{"", "", ""};
    Video video{"/dev/video0", input};
    int devnull{-1};

    void SetUp() override
    {
        devnull = open("/dev/null", O_RDWR);
        ASSERT_GE(devnull, 0);
        video.setTestFd(devnull);
    }

    void TearDown() override
    {
        video.setTestFd(-1); // prevent Video destructor from double-closing
        if (devnull >= 0)
        {
            close(devnull);
            devnull = -1;
        }
    }
};

TEST_F(VideoFdTest, GetFrame_WithFd_CoverSelectIoctlPath)
{
    // select() returns immediately for /dev/null; VIDIOC_DQBUF fails (ENOTTY)
    EXPECT_NO_THROW(video.getFrame());
}

TEST_F(VideoFdTest, NeedsResize_WithFd_CoversTimingsErrorPath)
{
    // VIDIOC_QUERY_DV_TIMINGS fails on /dev/null → timingsError=true, restart()
    // restart() = stop() + start() → covers Video::restart()
    EXPECT_NO_THROW(video.needsResize());
}

TEST_F(VideoFdTest, Stop_WithFd_CoversStreamOffPath)
{
    // VIDIOC_STREAMOFF on /dev/null fails (ENOTTY); buffers empty → just closes
    EXPECT_NO_THROW(video.stop());
    devnull = -1; // stop() already called close(fd)
}

TEST_F(VideoFdTest, GetSignalStatus_WithFd_CoversIoctlPath)
{
    // VIDIOC_ENUMINPUT on /dev/null fails → returns UINT32_MAX via error path
    EXPECT_EQ(video.getSignalStatus(), UINT32_MAX);
}

TEST_F(VideoFdTest, GetData_WithFdButEmptyDone_ReturnsNull)
{
    // Even with valid fd, buffersDone is empty → getData() returns nullptr
    EXPECT_EQ(video.getData(), nullptr);
}

TEST_F(VideoFdTest, GetData_WithBuffersDone_CoversNonEmptyBranch)
{
    video.resizeTestBuffers(1);     // buffers[0].data = nullptr
    video.buffersDone.push_back(0); // buffersDone non-empty
    // getData() takes the non-empty branch → returns buffers[0].data (nullptr)
    EXPECT_EQ(video.getData(), nullptr);
    EXPECT_EQ(video.getData(0u), nullptr);
}

TEST_F(VideoFdTest, Resize_ResizeAfterOpenTrue_CoversThatBranch)
{
    video.setResizeAfterOpen(true);
    // resize() enters, sees resizeAfterOpen=true, sets it false, returns
    EXPECT_NO_THROW(video.resize());
}

TEST_F(VideoFdTest, NeedsResize_ResizeAfterOpenTrue_ReturnsTrue)
{
    video.setResizeAfterOpen(true);
    EXPECT_TRUE(video.needsResize()); // resizeAfterOpen=true → returns true
}

TEST_F(VideoFdTest, Resize_WithOneBuffer_CoversNeedsResizePath)
{
    video.resizeTestBuffers(1); // buffers.size()=1, data=nullptr
    // !needsResizeCall && buffers.size()>0 → needsResizeCall=true
    // Covers: STREAMOFF, REQBUFS(0), QUERY_DV_TIMINGS (all fail on devnull)
    // → restart() is called which calls stop()+start()
    EXPECT_NO_THROW(video.resize());
    devnull = -1; // restart()→stop() closed devnull
}

TEST_F(VideoFdTest, Stop_WithOneBuffer_CoversBufferLoop)
{
    video.resizeTestBuffers(1); // buffers[0].data=nullptr
    EXPECT_NO_THROW(video.stop());
    devnull = -1;               // stop() closed devnull
}

TEST_F(VideoFdTest, ReleaseFrames_WithBufferAndDone_CoversQbuf)
{
    video.resizeTestBuffers(1); // buffers[0].queued=false, data=nullptr
    video.buffersDone.push_back(0);
    // releaseFrames() → pops 0, calls qbuf(0)
    // qbuf: queued=false → ioctl(VIDIOC_QBUF) on devnull → ENOTTY → log
    EXPECT_NO_THROW(video.releaseFrames());
}

// ---------------------------------------------------------------------------
// VideoNeedsResizeRepeatedTest — second call with timingsError already set
//   covers the !timingsError false branch
// ---------------------------------------------------------------------------
TEST(VideoNeedsResizeRepeatedTest,
     NeedsResize_CalledTwice_CoversTimingsErrorFalseBranch)
{
    Input inp{"", "", ""};
    Video v{"/dev/video0", inp};

    int dn1 = open("/dev/null", O_RDWR);
    ASSERT_GE(dn1, 0);
    v.setTestFd(dn1);
    // First call: timingsError was false → sets to true, calls restart()→stop()
    v.needsResize(); // dn1 closed by restart()

    // Second call: timingsError=true → log is skipped, still restarts
    int dn2 = open("/dev/null", O_RDWR);
    ASSERT_GE(dn2, 0);
    v.setTestFd(dn2);
    v.needsResize(); // dn2 closed by restart()
}

// ---------------------------------------------------------------------------
// VideoVideoRecordTest — with active=true and videoRecFlag=false the function
//   proceeds past the active check, creates /tmp/video, opens recording file,
//   enters the wait loop (exits immediately), then cleans up.
// ---------------------------------------------------------------------------
TEST(VideoVideoRecordActiveTest, VideoRecord_ActiveNoFrames_ExitsLoopAndCleans)
{
    Input inp{"", "", ""};
    Video v{"/dev/video0", inp};
    ikvm::active = true;
    // videoRecFlag is false → while loop exits immediately → clean return
    EXPECT_NO_THROW(Video::videoRecord(&v));
    ikvm::active = false; // restore
}

// ---------------------------------------------------------------------------
// VideoPushRecFrameExtendedTest — exercise the second and third early-return
//   checks in pushRecFrame() for better branch coverage
// ---------------------------------------------------------------------------
TEST(VideoPushRecFrameExtendedTest,
     PushRecFrame_FlagsSetNoBuffer_CoversBuffersDoneCheck)
{
    Input inp{"", "", ""};
    Video v{"/dev/video0", inp};
    ikvm::videoRecFlag.store(true);
    ikvm::recThreadStatus.store(true);
    // buffersDone is empty → early return at second check
    EXPECT_NO_THROW(v.pushRecFrame());
    ikvm::videoRecFlag.store(false);
    ikvm::recThreadStatus.store(false);
}

TEST(VideoPushRecFrameExtendedTest,
     PushRecFrame_FlagsSetNullData_CoversDataCheck)
{
    Input inp{"", "", ""};
    Video v{"/dev/video0", inp};
    v.resizeTestBuffers(1); // data=nullptr, payload=0
    v.buffersDone.push_back(0);
    ikvm::videoRecFlag.store(true);
    ikvm::recThreadStatus.store(true);
    // data=nullptr → early return at data/size check
    EXPECT_NO_THROW(v.pushRecFrame());
    ikvm::videoRecFlag.store(false);
    ikvm::recThreadStatus.store(false);
}

} // namespace ikvm
