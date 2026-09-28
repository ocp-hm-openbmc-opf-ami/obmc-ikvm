// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <linux/videodev2.h>
#include <sys/types.h>

#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace test
{
struct FakeBuffer
{
    uint32_t bytesUsed = 0;
    uint32_t sequence = 0;
    uint32_t flags = 0;
    uint32_t offset = 0;
    bool queued = false;
};

struct MmapRegion
{
    void* address = nullptr;
    size_t length = 0;
    int fd = -1;
    off_t offset = 0;
};

struct WrapStats
{
    int openCalls = 0;
    int closeCalls = 0;
    int fcntlGetCalls = 0;
    int fcntlSetCalls = 0;
    int selectCalls = 0;
    int reqbufCalls = 0;
    int querybufCalls = 0;
    int qbufCalls = 0;
    int dqbufCalls = 0;
    int streamonCalls = 0;
    int streamoffCalls = 0;
    int sFmtCalls = 0;
    int gFmtCalls = 0;
    int sParmCalls = 0;
    int sCtrlCalls = 0;
    int gSelectionCalls = 0;
    int queryTimingsCalls = 0;
    int sTimingsCalls = 0;
    int mmapCalls = 0;
    int munmapCalls = 0;
    int fcntlFlags = 0;
    bool hasLastFormat = false;
    v4l2_format lastFormat{};
    std::map<void*, MmapRegion> mappedRegions;
    std::vector<MmapRegion> unmappedRegions;
};

struct FakeV4L2
{
    int nextFd = 73;
    int fakeFd = -1;
    bool openFails = false;
    bool selectReady = true;
    bool querycapFails = false;
    bool enuminputFails = false;
    bool querybufFails = false;
    bool getFormatFails = false;
    bool setFormatFails = false;
    bool mmapFails = false;
    bool setParmFails = false;
    int setControlFailures = 0;
    bool qbufFails = false;
    bool streamonFails = false;
    bool streamoffFails = false;
    bool getSelectionFails = false;
    bool timingsAvailable = true;
    bool setTimingsFails = false;
    uint32_t capabilities = V4L2_CAP_VIDEO_CAPTURE | V4L2_CAP_STREAMING;
    uint32_t pixelFormat = V4L2_PIX_FMT_JPEG;
    uint32_t formatWidth = 1280;
    uint32_t formatHeight = 720;
    uint32_t returnedPixelFormat = 0;
    uint32_t timingsWidth = 1024;
    uint32_t timingsHeight = 768;
    uint32_t inputStatus = V4L2_IN_ST_NO_SIGNAL;
    uint32_t requestedBufferCount = 3;
    std::deque<uint32_t> dequeueIndices;
    std::map<uint32_t, FakeBuffer> buffers;
    v4l2_rect selectionRect{10, 20, 300, 200};
    WrapStats stats;

    void reset()
    {
        *this = FakeV4L2{};
    }
};

extern FakeV4L2 fakeV4L2;
} // namespace test

namespace ikvm
{
struct AmiWrapLog
{
    int isDirCalls = 0;
    std::vector<std::string> isDirPaths;
    int eventLogCalls = 0;
    std::vector<std::string> eventLogMessages;
    bool isDirResult = true;
};

extern AmiWrapLog amiWrapLog;
} // namespace ikvm
