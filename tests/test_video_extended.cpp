// SPDX-License-Identifier: Apache-2.0

#include "fake_v4l2.hpp"
#include "ikvm_input.hpp"
#include "ikvm_video.hpp"
#include "video_alg.hpp"

#include <filesystem>
#include <vector>

#include <gtest/gtest.h>

namespace ikvm
{
namespace
{
constexpr int defaultWidth = 800;
constexpr int defaultHeight = 600;

TEST(VideoExtendedEoi, HasJpegEoi_ValidAndInvalidMarkers_DetectsCorrectly)
{
    const uint8_t valid[] = {0x01, 0x02, 0xFF, 0xD9};
    const uint8_t invalid[] = {0x01, 0x02, 0xFF, 0xD8};
    EXPECT_TRUE(hasJpegEoi(valid, sizeof(valid)));
    EXPECT_FALSE(hasJpegEoi(invalid, sizeof(invalid)));
    EXPECT_FALSE(hasJpegEoi(static_cast<const char*>(nullptr), 0));
}

TEST(VideoExtendedCrc,
     Crc32SkipJfifHeader_HeaderBytesDiffer_IgnoresAndProducesSameCrc)
{
    std::vector<char> first(96, 0);
    std::vector<char> second(96, 0);
    for (size_t index = 16; index < first.size(); ++index)
    {
        first[index] = static_cast<char>((index * 13) & 0xFF);
        second[index] = first[index];
    }
    for (size_t index = 0; index < 16; ++index)
    {
        first[index] = static_cast<char>(index);
        second[index] = static_cast<char>(0xAA ^ index);
    }

    EXPECT_EQ(crc32SkipJfifHeader(first.data(), first.size(), 16),
              crc32SkipJfifHeader(second.data(), second.size(), 16));
    EXPECT_EQ(crc32SkipJfifHeader(first.data(), first.size(), first.size()),
              0U);
}

struct ClipCase
{
    v4l2_rect input;
    int width;
    int height;
    v4l2_rect expected;
};

class VideoExtendedClip : public testing::TestWithParam<ClipCase>
{};

TEST_P(VideoExtendedClip, ClipRect_VariousRectangles_ClampsToFrameBounds)
{
    const auto& testCase = GetParam();
    const auto actual =
        clipRect(testCase.input, testCase.width, testCase.height);
    EXPECT_EQ(actual.left, testCase.expected.left);
    EXPECT_EQ(actual.top, testCase.expected.top);
    EXPECT_EQ(actual.width, testCase.expected.width);
    EXPECT_EQ(actual.height, testCase.expected.height);
}

INSTANTIATE_TEST_SUITE_P(
    ClipCases, VideoExtendedClip,
    testing::Values(ClipCase{{10, 20, 100, 50}, 1920, 1080, {10, 20, 100, 50}},
                    ClipCase{{-5, -7, 30, 40}, 640, 480, {0, 0, 30, 40}},
                    ClipCase{{620, 470, 50, 40}, 640, 480, {620, 470, 20, 10}},
                    ClipCase{{700, 500, 100, 50}, 640, 480, {640, 480, 0, 0}},
                    ClipCase{{10, 10, 100, 100}, 0, 0, {0, 0, 0, 0}}));

class VideoExtendedV4L2 : public testing::Test
{
  protected:
    Input input;
    std::unique_ptr<Video> video;

    void SetUp() override
    {
        test::fakeV4L2.reset();
        amiWrapLog = {};
        test::fakeV4L2.buffers[0] = {};
        test::fakeV4L2.buffers[1] = {};
        test::fakeV4L2.buffers[2] = {};
        test::fakeV4L2.buffers[0].bytesUsed = 120;
        test::fakeV4L2.buffers[0].sequence = 42;
        test::fakeV4L2.dequeueIndices = {0};
        video = std::make_unique<Video>("/dev/video0", input, 30, 0, 0);
    }

    void TearDown() override
    {
        video.reset();
    }
};

TEST_F(VideoExtendedV4L2, Start_DefaultState_ConfiguresV4L2Fully)
{
    video->start();

    EXPECT_EQ(test::fakeV4L2.stats.openCalls, 1);
    EXPECT_EQ(test::fakeV4L2.stats.gFmtCalls, 1);
    EXPECT_EQ(test::fakeV4L2.stats.sFmtCalls, 1);
    EXPECT_EQ(test::fakeV4L2.stats.reqbufCalls, 1);
    EXPECT_EQ(test::fakeV4L2.stats.querybufCalls, 3);
    EXPECT_EQ(test::fakeV4L2.stats.mmapCalls, 3);
    EXPECT_EQ(test::fakeV4L2.stats.qbufCalls, 3);
    EXPECT_EQ(test::fakeV4L2.stats.streamonCalls, 1);
    EXPECT_NE(video->getData(0), nullptr);
}

TEST_F(VideoExtendedV4L2, Start_OpenFails_SkipsBufferSetup)
{
    test::fakeV4L2.openFails = true;
    video->start();

    EXPECT_EQ(test::fakeV4L2.stats.openCalls, 1);
    EXPECT_EQ(test::fakeV4L2.stats.reqbufCalls, 0);
    video->getFrame();
    EXPECT_EQ(test::fakeV4L2.stats.dqbufCalls, 0);
}

TEST_F(VideoExtendedV4L2, Start_MultipleErrors_ContinuesWithoutFatal)
{
    test::fakeV4L2.querycapFails = true;
    test::fakeV4L2.getFormatFails = true;
    test::fakeV4L2.setFormatFails = true;
    test::fakeV4L2.setParmFails = true;
    test::fakeV4L2.setControlFailures = 2;
    test::fakeV4L2.streamonFails = true;

    video->start();

    EXPECT_EQ(test::fakeV4L2.stats.gFmtCalls, 1);
    EXPECT_EQ(test::fakeV4L2.stats.sFmtCalls, 1);
    EXPECT_EQ(test::fakeV4L2.stats.sParmCalls, 1);
    EXPECT_GE(test::fakeV4L2.stats.sCtrlCalls, 2);
    EXPECT_EQ(test::fakeV4L2.stats.reqbufCalls, 1);
    EXPECT_EQ(test::fakeV4L2.stats.streamonCalls, 1);
}

TEST_F(VideoExtendedV4L2,
       GetFrame_ValidBufferDequeued_CapturesPayloadAndSelection)
{
    video->setFormat(2);
    test::fakeV4L2.selectionRect = {7, 9, 123, 77};
    video->start();
    video->getFrame();

    ASSERT_EQ(video->buffersDone.size(), 1U);
    EXPECT_EQ(video->getFrameSize(), 120U);
    EXPECT_EQ(video->getFrameCount(), 42U);
    EXPECT_NE(video->getData(), nullptr);
    const auto box = video->getBoundingBox(video->buffersDone.front());
    EXPECT_EQ(box.left, 7);
    EXPECT_EQ(box.top, 9);
    EXPECT_EQ(box.width, 123U);
    EXPECT_EQ(box.height, 77U);
}

TEST_F(VideoExtendedV4L2, GetFrame_BufferFlagError_RequeuesWithoutPublishing)
{
    test::fakeV4L2.buffers[0].flags = V4L2_BUF_FLAG_ERROR;
    video->start();
    const int queuesBefore = test::fakeV4L2.stats.qbufCalls;
    video->getFrame();

    EXPECT_TRUE(video->buffersDone.empty());
    EXPECT_EQ(test::fakeV4L2.stats.qbufCalls, queuesBefore + 1);
    EXPECT_EQ(video->getFrameSize(0), 0U);
}

TEST_F(VideoExtendedV4L2, GetFrame_SelectTimeout_DoesNotDequeue)
{
    test::fakeV4L2.selectReady = false;
    video->start();
    video->getFrame();

    EXPECT_TRUE(video->buffersDone.empty());
    EXPECT_EQ(test::fakeV4L2.stats.dqbufCalls, 0);
}

TEST_F(VideoExtendedV4L2, ReleaseFrames_FramePublished_RequeuesAndClears)
{
    video->start();
    video->getFrame();
    ASSERT_FALSE(video->buffersDone.empty());
    const int queuesBefore = test::fakeV4L2.stats.qbufCalls;

    video->releaseFrames();

    EXPECT_TRUE(video->buffersDone.empty());
    EXPECT_EQ(test::fakeV4L2.stats.qbufCalls, queuesBefore + 1);
}

TEST_F(VideoExtendedV4L2, ReleaseFrames_RequeueFails_DropsFrame)
{
    video->start();
    video->getFrame();
    test::fakeV4L2.qbufFails = true;

    video->releaseFrames();

    EXPECT_TRUE(video->buffersDone.empty());
}

TEST_F(VideoExtendedV4L2,
       NeedsResize_TimingsChanged_UpdatesDimensionsAndDropsFrame)
{
    video->start();
    video->getFrame();
    ASSERT_FALSE(video->buffersDone.empty());
    test::fakeV4L2.timingsWidth = 1920;
    test::fakeV4L2.timingsHeight = 1080;

    EXPECT_TRUE(video->needsResize());
    EXPECT_EQ(video->getWidth(), 1920U);
    EXPECT_EQ(video->getHeight(), 1080U);
    EXPECT_TRUE(video->buffersDone.empty());
}

TEST_F(VideoExtendedV4L2, NeedsResize_TimingsUnavailable_RestartsDevice)
{
    video->start();
    test::fakeV4L2.timingsAvailable = false;
    const int opensBefore = test::fakeV4L2.stats.openCalls;
    const int closesBefore = test::fakeV4L2.stats.closeCalls;

    EXPECT_FALSE(video->needsResize());
    EXPECT_GT(test::fakeV4L2.stats.openCalls, opensBefore);
    EXPECT_GT(test::fakeV4L2.stats.closeCalls, closesBefore);
}

TEST_F(VideoExtendedV4L2, Resize_AnyState_ReallocatesAndRestarts)
{
    video->start();
    const int requestsBefore = test::fakeV4L2.stats.reqbufCalls;
    const int streamsBefore = test::fakeV4L2.stats.streamonCalls;

    video->resize();

    EXPECT_GE(test::fakeV4L2.stats.streamoffCalls, 1);
    EXPECT_GT(test::fakeV4L2.stats.reqbufCalls, requestsBefore);
    EXPECT_GT(test::fakeV4L2.stats.streamonCalls, streamsBefore);
}

TEST_F(VideoExtendedV4L2, Stop_ActiveState_UnmapsAllAndCloses)
{
    video->start();
    ASSERT_EQ(test::fakeV4L2.stats.mappedRegions.size(), 3U);

    video->stop();

    EXPECT_EQ(test::fakeV4L2.stats.unmappedRegions.size(), 3U);
    EXPECT_TRUE(test::fakeV4L2.stats.mappedRegions.empty());
    EXPECT_EQ(test::fakeV4L2.stats.closeCalls, 1);
}

TEST_F(VideoExtendedV4L2, Screenshot_DirectoryCheckFails_SkipsFilesystemWrite)
{
    amiWrapLog.isDirResult = false;
    video->start();
    video->getFrame();
    const auto output =
        std::filesystem::temp_directory_path() / "ikvm-video-extended-shot.jpg";
    std::filesystem::remove(output);

    video->screenShot(output.string());

    EXPECT_EQ(amiWrapLog.isDirCalls, 1);
    EXPECT_FALSE(std::filesystem::exists(output));
}
} // namespace
} // namespace ikvm
