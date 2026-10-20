#pragma once

#include "connection.hpp"

#include <memory>

namespace sdbusplus::asio
{

class object_server
{
  public:
    explicit object_server(std::shared_ptr<connection>) {}
};

} // namespace sdbusplus::asio
