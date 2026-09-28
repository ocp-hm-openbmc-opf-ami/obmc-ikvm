#pragma once

#include "ikvm_args.hpp"
#include "ikvm_input.hpp"
#include "ikvm_video.hpp"

#include <gmock/gmock.h>

namespace ikvm
{

class Server
{
  public:
    Server(const Args&, Input&, Video&) {}

    MOCK_METHOD(void, run, ());
    MOCK_METHOD(void, sendFrame, ());
    MOCK_METHOD(void, resize, ());
    MOCK_METHOD(bool, wantsFrame, (), (const));
};

} // namespace ikvm
