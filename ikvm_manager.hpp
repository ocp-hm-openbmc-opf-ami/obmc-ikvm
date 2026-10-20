#pragma once

#include "ikvm_args.hpp"

#ifdef TEST
#include <ami/include/ikvm_monitor.hpp>
#include <ami/include/ikvm_utils.hpp>
#include <ikvm_input.hpp>
#include <ikvm_server.hpp>
#include <ikvm_video.hpp>
#else
#include "ami/include/ikvm_monitor.hpp"
#include "ami/include/ikvm_utils.hpp"
#include "ikvm_input.hpp"
#include "ikvm_server.hpp"
#include "ikvm_video.hpp"
#endif

#include <boost/asio.hpp>

#include <condition_variable>
#include <mutex>

namespace ikvm
{
/*
 * @class Manager
 * @brief Manages the VNC server by executing threaded loops of RFB operations
 *        and video device operations.
 */
class Manager
{
  public:
    /*
     * @brief Constructs the Manager object
     *
     * @param[in] args - Reference to Args object
     */
    explicit Manager(const Args& args);
    ~Manager() = default;
    Manager(const Manager&) = default;
    Manager& operator=(const Manager&) = default;
    Manager(Manager&&) = default;
    Manager& operator=(Manager&&) = default;

    /* @brief Begins operation of the VNC server */
    void run();

    /* @brief asynchronous external input*/
    boost::asio::io_context io;

#ifdef TEST
    void testSetContinueExecuting(bool value)
    {
        continueExecuting = value;
    }
    bool testGetContinueExecuting() const
    {
        return continueExecuting;
    }
    void testSetServerDoneFlag(bool value)
    {
        serverDone = value;
    }
    bool testGetServerDoneFlag() const
    {
        return serverDone;
    }
    void testSetVideoDoneFlag(bool value)
    {
        videoDone = value;
    }
    bool testGetVideoDoneFlag() const
    {
        return videoDone;
    }
    Server& testServerRef()
    {
        return server;
    }
    Video& testVideoRef()
    {
        return video;
    }
    static void testCallServerThread(Manager* manager)
    {
        serverThread(manager);
    }
    static void testCallStatusUpdateThread(Manager* manager)
    {
        statusUpdateThread(manager);
    }
    void testWaitServer()
    {
        waitServer();
    }
    void testWaitVideo()
    {
        waitVideo();
    }
    void testSetServerDone()
    {
        setServerDone();
    }
    void testSetVideoDone()
    {
        setVideoDone();
    }
#endif

  private:
    /*
     * @brief Thread function to loop the RFB update operations
     *
     * @param[in] manager - Pointer to the Manager object
     */
    static void serverThread(Manager* manager);
    static void statusUpdateThread(Manager* manager);
    /* @brief Notifies thread waiters that RFB operations are complete */
    void setServerDone();
    /* @brief Notifies thread waiters that video operations are complete */
    void setVideoDone();
    /* @brief Blocks until RFB operations complete */
    void waitServer();
    /* @brief Blocks until video operations are complete */
    void waitVideo();

    /*
     * @brief Boolean to indicate whether the application should continue
     *        running
     */
    bool continueExecuting;
    /* @brief Boolean to indicate that RFB operations are complete */
    bool serverDone;
    /* @brief Boolean to indicate that video operations are complete */
    bool videoDone;
    /* @brief Input object */
    Input input;
    /* @brief Video object */
    Video video;
    /* @brief RFB server object */
    Server server;
    /*@brief Monitor object*/
    Monitor monitor;

    /* @brief Condition variable to enable waiting for thread completion */
    std::condition_variable sync;
    /* @brief Mutex for waiting on condition variable safely */
    std::mutex lock;
};

} // namespace ikvm
