#include "ikvm_manager.hpp"

#include <phosphor-logging/log.hpp>

#include <thread>

namespace fs = std::filesystem;

namespace ikvm
{

using namespace phosphor::logging;

Manager::Manager(const Args& args) :
    continueExecuting(true), serverDone(false), videoDone(true),
    input(args.getKeyboardPath(), args.getPointerPath(), args.getUdcName()),
    video(args.getVideoPath(), input, args.getFrameRate(),
          args.getSubsampling(), args.getFormat()),
    server(args, input, video), monitor()
{}

void Manager::run()
{
    createUtilities();
    auto conn = std::make_shared<sdbusplus::asio::connection>(io);
    conn->request_name(kvmServiceName.c_str());
    sdbusplus::asio::object_server objServer(conn);

    Interface interface(objServer);
    interface.addInterfaces();

    sdbusplus::bus::match_t bsodMatcher = monitor.bsodErrorEventMonitor(conn);
    sdbusplus::bus::match_t screenshotMatcher = monitor.screenshotMonitor(conn);
    sdbusplus::bus::match_t triggerSignal = monitor.sessionMonitor(conn);
    sdbusplus::bus::match_t captutreTimeout = monitor.sessionTimeout(conn);
    sdbusplus::bus::match_t powerStatMatcher = monitor.powerStatMonitor(conn);

    std::thread run(serverThread, this);
    std::thread runStatusUpdate(statusUpdateThread, this);
    io.run();

    runStatusUpdate.join();
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
        if (manager->server.wantsFrame() || scrnshotFlag.load())
        {
            manager->video.start();

            if (scrnshotFlag.load())
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

            if (scrnshotFlag.load())
            {
                if (manager->video.getFormat() != 2)
                {
                    manager->video.screenShot(bsodAsJpeg);
                    scrnshotFlag.store(false);
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
            manager->waitServer();
            manager->videoDone = false;
            manager->video.resize();
            manager->server.resize();
            manager->setVideoDone();
        }
        else
        {
            manager->setVideoDone();
            manager->waitServer();
        }
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
