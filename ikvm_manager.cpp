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
    sdbusplus::asio::object_server objServer(conn);

    monitor.initialize(conn);

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
        if (manager->server.wantsFrame() || scrnshotFlag.load() ||
            videoRecFlag.load() || InitFlag.load())
        {
            manager->video.start();

            if (scrnshotFlag.load() || manager->video.isNewClient ||
                videoRecFlag.load())
            {
                if (manager->video.getFormat() == 2)
                {
                    log<level::DEBUG>("switching to standard jpeg...");
                    manager->video.formatChange(0);
                }
            }
            else if (manager->video.getFormat() !=
                     manager->video.getOriginalFormat())
            {
                if (!recThreadStatus.load())
                {
                    log<level::DEBUG>(
                        "Reverting to Original Streaming format...");
                    manager->video.formatChange(
                        manager->video.getOriginalFormat());
                }
            }

            if (manager->video.getSignalStatus() == V4L2_IN_ST_NO_SIGNAL)
            {
                if (hostPowerState == "Off")
                {
                    manager->video.setFrame(POWER_OFF_IMG_PATH);
                }
                else
                {
                    manager->video.setFrame(NO_SIGNAL_IMG_PATH);
                }
            }
            else
            {
                manager->video.getFrame();
            }

            if (scrnshotFlag.load())
            {
                if (manager->video.getFormat() != 2)
                {
                    manager->video.screenShot(bsodAsJpeg);
                    scrnshotFlag.store(false);
                }
            }
            if (videoRecFlag.load())
            {
                if (!recThreadStatus.load())
                {
                    if (manager->video.getFormat() != 2)
                    {
                        recThreadStatus.store(true);
                        // Create and start the record thread
                        std::thread t(&Video::videoRecord, &manager->video);
                        // Detach the thread for non-blocking proceeding
                        t.detach();
                    }
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
            if (InitFlag.load())
            {
                log<level::DEBUG>("Init flag Downed");
                InitFlag.store(false);
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
