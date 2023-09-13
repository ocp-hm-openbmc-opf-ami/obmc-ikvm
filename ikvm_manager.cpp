#include "ikvm_manager.hpp"

#include <thread>

#include <sdbusplus/asio/connection.hpp>
#include <sdbusplus/asio/object_server.hpp>
#include <sdbusplus/bus/match.hpp>
#include <boost/container/flat_map.hpp>
#include <variant>

std::string bsodAsJpeg = "/tmp/screenShotBSOD.jpeg";

namespace ikvm
{
Manager::Manager(const Args& args) :
    continueExecuting(true), serverDone(false), videoDone(true),
    input(args.getKeyboardPath(), args.getPointerPath(), args.getUdcName()),
    video(args.getVideoPath(), input, args.getFrameRate(),
          args.getSubsampling(), args.getFormat()),
    server(args, input, video)
{}

void Manager::run()
{
    /*
     * ===================================================================
     *  D-Bus Signal Monitor for BSOD
     * 
     *  Asynchronous Triggering of BSODflag when Run-time Critical Stop/BSOD
     *  propertiesChanged signal is recived 
     * ===================================================================
     */
    BSODFlag.store(false);

    const std::string objPath = "/xyz/openbmc_project/sensors/os/";
    std::string target = "/xyz/openbmc_project/sensors/os";
    std::string sennType = "xyz.openbmc_project.Sensor.State";

    auto conn = std::make_shared<sdbusplus::asio::connection>(io);
    std::shared_ptr<sdbusplus::bus::match::match> BSODMatcher;

    /* 
     * Callback function to raise BSODflag when propertiesChanged 
     * signal received 
     */    
    auto BSODMatcherCallback = 
        [this, &conn, objPath](sdbusplus::message::message& msg) {
        std::string discreteInterface;
        boost::container::flat_map<std::string, std::variant<uint16_t>> properties;
        msg.read(discreteInterface,properties);

        uint16_t offset = std::get<uint16_t>(properties.begin()->second);
    /* 
     * offset value 2 indicates Run-time Critical Stop/BSOD 
     * Data Reference
     *  IPMI Spec
     *   -> Sensor Type Codes and Data 
     *      -> Table Sensor Type Codes 
     *         -> OS Stop / Shutdown 
     *            -> 01h Run-time Critical Stop
     */    
        if(offset == 2)
        {
            BSODFlag.store(true);
        }
    };
    
    BSODMatcher = std::make_shared<sdbusplus::bus::match::match>(
        static_cast<sdbusplus::bus::bus&>(*conn),
        "type='signal',member='PropertiesChanged',path_namespace='" + target +
            "',arg0namespace='" + sennType + "'",
        std::move(BSODMatcherCallback));

    std::thread run(serverThread, this);
    std::thread runstatusUpdate(statusUpdateThread, this);
    io.run();

    runstatusUpdate.join();
    run.join();
}

void Manager::serverThread(Manager* manager)
{
    while (manager->continueExecuting)
    {
        manager->server.run();
        manager->setServerDone();
        manager->waitVideo();
    }
}

void Manager::statusUpdateThread(Manager* manager)
{
    while (manager->continueExecuting)
    {
        if (manager->server.wantsFrame() || manager->BSODFlag.load())
        {
            manager->video.start();

            if (manager->BSODFlag.load())
            {
                if (manager->video.getFormat() == 2)
                {
                    manager->video.formatChange(0);
                }
            }
            else if (manager->video.getFormat() !=
                     manager->video.getOriginalFormat())
            {
                manager->video.formatChange(manager->video.getOriginalFormat());
            }

            manager->video.getFrame();

            if (manager->BSODFlag.load())
            {
                if (manager->video.getFormat() != 2)
                {
                    manager->video.screenShot(bsodAsJpeg);
                    manager->BSODFlag.store(false);
                }
            }

            if (manager->server.wantsFrame())
            {
                manager->server.sendFrame();
            }
            else
            {
                manager->video.releaseFrames();
            }
        }
        else
        {
            manager->video.stop();
        }

        if (manager->video.needsResize())
        {
            manager->videoDone = false;
            manager->waitServer();
            manager->video.resize();
            manager->server.resize();
            manager->setVideoDone();
        }
        else
        {
            manager->setVideoDone();
            manager->waitServer();
        }

        /*=========================================================
        * following code is added for Debugging Purpose only
        * Do not enable for production Build
        *=========================================================*/
        #if 0 
        if (std::filesystem::exists("/var/BSODtrigger"))
        {
            std::filesystem::remove("/var/BSODtrigger");
            manager->BSODFlag.store(true);
        }
        #endif
        /********************************************************/
    }
}

void Manager::setServerDone()
{
    std::unique_lock<std::mutex> ulock(lock);

    serverDone = true;
    sync.notify_all();
}

void Manager::setVideoDone()
{
    std::unique_lock<std::mutex> ulock(lock);

    videoDone = true;
    sync.notify_all();
}

void Manager::waitServer()
{
    std::unique_lock<std::mutex> ulock(lock);

    while (!serverDone)
    {
        sync.wait(ulock);
    }

    serverDone = false;
}

void Manager::waitVideo()
{
    std::unique_lock<std::mutex> ulock(lock);

    while (!videoDone)
    {
        sync.wait(ulock);
    }

    // don't reset videoDone
}

} // namespace ikvm
