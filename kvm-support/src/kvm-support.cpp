#include "kvm_support-interface.hpp"

int main()
{
    boost::asio::io_context ioc;
    auto conn = std::make_shared<sdbusplus::asio::connection>(ioc);
    conn->request_name(kvmSupport::ServiceName.c_str());
    sdbusplus::asio::object_server objServer(conn);

    kvmSupport::Interface interface(objServer);
    
    interface.initialize();

    ioc.run();

    return 0;
}
