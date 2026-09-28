#pragma once

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>

#include <memory>

namespace ikvm
{

class Monitor
{
  public:
    void initialize(const std::shared_ptr<sdbusplus::asio::connection>&) {}
};

} // namespace ikvm
