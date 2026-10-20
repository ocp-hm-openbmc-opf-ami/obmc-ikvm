#pragma once

#include <linux/videodev2.h>

#include <string>

#include <gmock/gmock.h>

namespace ikvm
{

class Input;

class Video
{
  public:
    Video(const std::string&, Input&, int, int, int) {}

    MOCK_METHOD(void, start, ());
    MOCK_METHOD(void, stop, ());
    MOCK_METHOD(bool, needsResize, ());
    MOCK_METHOD(void, resize, ());
    MOCK_METHOD(void, releaseFrames, ());
    MOCK_METHOD(void, getFrame, ());
    MOCK_METHOD(void, pushRecFrame, ());
    MOCK_METHOD(int, getFormat, (), (const));
    MOCK_METHOD(int, getOriginalFormat, (), (const));
    MOCK_METHOD(void, formatChange, (int));
    MOCK_METHOD(int, getSignalStatus, ());
    MOCK_METHOD(void, setFrame, (const char*));
    MOCK_METHOD(void, screenShot, (const std::string&));

    static void videoRecord(Video*) {}

    bool isNewClient = false;
};

} // namespace ikvm
