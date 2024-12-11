#include "kvm_dbus-interface.hpp"
#include "kvm_dbus-monitor.hpp"

int main()
{
    boost::asio::io_context ioc;
    auto conn = std::make_shared<sdbusplus::asio::connection>(ioc);
    conn->request_name(kvmDbus::ServiceName.c_str());
    sdbusplus::asio::object_server objServer(conn);

    kvmDbus::Interface interface(objServer);
    interface.initialize();

    kvmDbus::Monitor monitor;
    monitor.initialize(conn);

    ioc.run();

    return 0;
}
