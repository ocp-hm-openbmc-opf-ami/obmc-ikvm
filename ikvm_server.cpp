#include "ikvm_server.hpp"

#include <arpa/inet.h>
#include <linux/videodev2.h>
#include <rfb/rfbproto.h>

#include <boost/crc.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#define ROUND_DOWN(x, r) ((x) & ~((r) - 1))

#define DEFAULT_IP "~"        // Loopback IP address
#define USER_NAME "local"     // Default user
#define KVM 0                 // KVM session type
#define PRIV_LEVEL_ADMIN 0x04 // Privilege level for admin
#define KVM_DEFAULT_USER_ID 0 // Default user ID
#define LOGOUT 1              // Reason for session unregister
#define MOUNTING_METHOD ""    // Empty mounting method

namespace ikvm
{

using namespace phosphor::logging;
using namespace sdbusplus::xyz::openbmc_project::Common::Error;

Server::Server(const Args& args, Input& i, Video& v) :
    pendingResize(false), frameCounter(0), numClients(0), input(i), video(v)
{
    std::string ip("localhost");
    const Args::CommandLine& commandLine = args.getCommandLine();
    int argc = commandLine.argc;

    server = rfbGetScreen(&argc, commandLine.argv, video.getWidth(),
                          video.getHeight(), Video::bitsPerSample,
                          Video::samplesPerPixel, Video::bytesPerPixel);

    if (!server)
    {
        log<level::ERR>("Failed to get VNC screen due to invalid arguments");
        elog<InvalidArgument>(
            xyz::openbmc_project::Common::InvalidArgument::ARGUMENT_NAME(""),
            xyz::openbmc_project::Common::InvalidArgument::ARGUMENT_VALUE(""));
    }

    framebuffer.resize(
        video.getHeight() * video.getWidth() * Video::bytesPerPixel, 0);

    server->screenData = this;
    server->desktopName = "OneTree IKVM";
    server->frameBuffer = framebuffer.data();
    server->newClientHook = newClient;
    server->cursor = rfbMakeXCursor(cursorWidth, cursorHeight, (char*)cursor,
                                    (char*)cursorMask);
    server->cursor->xhot = 1;
    server->cursor->yhot = 1;

    rfbStringToAddr(&ip[0], &server->listenInterface);

    rfbInitServer(server);

    rfbMarkRectAsModified(server, 0, 0, video.getWidth(), video.getHeight());

    server->kbdAddEvent = Input::keyEvent;
    server->ptrAddEvent = Input::pointerEvent;

    processTime = (1000000 / video.getFrameRate()) - 100;

    calcFrameCRC = args.getCalcFrameCRC();
}

Server::~Server()
{
    rfbScreenCleanup(server);
}

void Server::resize()
{
    if (frameCounter > video.getFrameRate())
    {
        doResize();
    }
    else
    {
        pendingResize = true;
    }
}

void Server::run()
{
    rfbProcessEvents(server, processTime);

    if (server->clientHead)
    {
        frameCounter++;
        if (pendingResize && frameCounter > video.getFrameRate())
        {
            doResize();
            pendingResize = false;
        }
    }
}

void Server::sendFrame()
{
    char* data = video.getData();
    rfbClientIteratorPtr it;
    rfbClientPtr cl;
    int64_t frame_crc = -1;
    bool frame_sent = false;
    Server* serverdata = (Server*)server->screenData;

    if (!data || pendingResize)
    {
        return;
    }

    it = rfbGetClientIterator(server);

    while ((cl = rfbClientIteratorNext(it)))
    {
        ClientData* cd = (ClientData*)cl->clientData;
        rfbFramebufferUpdateMsg* fu = (rfbFramebufferUpdateMsg*)cl->updateBuf;
        auto i = video.buffersDone.front();
        auto currentTime = std::chrono::steady_clock::now();

        if (!cd)
        {
            continue;
        }

        /* For session Timeout Implementation*/
        auto timeSinceLastActive =
            std::chrono::duration_cast<std::chrono::seconds>(
                currentTime - cd->lastActivityTime);

        /* Once the timeSinceLastActive surpasses the timeout value, the client
         * will be disconnected */
        if (timeSinceLastActive >= timeoutValue)
        {
            rfbCloseClient(cl);
            continue;
        }

        /* Disconnecting the clients immediately when KVM has been disabled from
         * WebUI*/
        if (kvmStatus)
        {
            handleKVMServiceDisabled(cl->screen);
            rfbCloseClient(cl);
            continue;
        }

        /* Disconnect the clients when unregister happen from other services*/
        if (cd->sessionId)
        {
            bool found = false;
            for (const auto& id : activeSessionIDs)
            {
                if (cd->sessionId == id)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                rfbCloseClient(cl);
            }
        }

        if (cd->skipFrame)
        {
            cd->skipFrame--;
            continue;
        }

        if (!cd->needUpdate)
        {
            continue;
        }

        if (!(data[video.getFrameSize(i) - 2] == 255 &&
              data[video.getFrameSize(i) - 1] == 217))
        {
            video.releaseFrames();
            video.getFrame();
            continue;
        }

        if (calcFrameCRC)
        {
            if (frame_crc == -1)
            {
                /* JFIF header contains some varying data so skip it for
                 * checksum calculation */
                frame_crc =
                    boost::crc<32, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF, true,
                               true>(data + 0x30, video.getFrameSize(i) - 0x30);
            }

            if (cd->last_crc == frame_crc)
            {
                video.releaseFrames();
                video.getFrame();
                continue;
            }

            cd->last_crc = frame_crc;
        }

        cd->needUpdate = false;
        frame_sent = true;

        if (serverdata->input.getkeyboardLedState() == INITIAL_LED_STATE)
        {
            /*
             * ============================================================
             * When Keyboard LED state in Server is at INITIAL_LED_STATE
             * Server sends fake keyevent to Host for getting the original
             * Host KeyboardLED State
             *
             *  NOTE: 0xFF7F : keysym value of NumLock key
             * ============================================================
             */
            serverdata->input.keyEvent(true, 0xFF7F, cl);
            serverdata->input.keyEvent(false, 0xFF7F, cl);
            serverdata->input.keyEvent(true, 0xFF7F, cl);
            serverdata->input.keyEvent(false, 0xFF7F, cl);
        }
        /* Provide extra rectangle for the HostKeyboard LED
         * state data */
        if (cl->enableKeyboardLedState)
        {
            fu->nRects = 0xFFFF;
        }
        else
        {
            fu->nRects = Swap16IfLE(1);
        }

        if (cl->enableLastRectEncoding)
        {
            fu->nRects = 0xFFFF;
        }
        else
        {
            fu->nRects = Swap16IfLE(1);
        }

        switch (video.getPixelformat())
        {
            case V4L2_PIX_FMT_RGB24:
                framebuffer.assign(data, data + video.getFrameSize());
                rfbMarkRectAsModified(server, 0, 0, video.getWidth(),
                                      video.getHeight());
                break;

            case V4L2_PIX_FMT_JPEG:
                fu->type = rfbFramebufferUpdate;
                cl->ublen = sz_rfbFramebufferUpdateMsg;
                rfbSendUpdateBuf(cl);
                if (cl->tightEncodingSupport)
                {
                    cl->tightEncoding = rfbEncodingTight;
                }
                else
                    cl->tightEncoding = rfbEncodingJPEG;

                if (video.getFormat() == 2)
                {
                    v4l2_rect r = video.getBoundingBox(i);

                    rfbSendTightHeader(cl, r.left, r.top, r.width, r.height);
                }
                else
                {
                    rfbSendTightHeader(cl, 0, 0, video.getWidth(),
                                       video.getHeight());
                }
                if (cl->tightEncodingSupport)
                {
                    cl->updateBuf[cl->ublen++] = (char)(rfbTightJpeg << 4);
                }
                rfbSendCompressedDataTight(cl, data, video.getFrameSize(i));
                rfbSendUpdateBuf(cl);
                break;

            default:
                break;
        }

        /* Send the Host LED status to client */
        if (cl->enableKeyboardLedState)
        {
            if ((cl->lastKeyboardLedState) !=
                (serverdata->input.getkeyboardLedState()))
            {
                log<level::DEBUG>(
                    " \n === Host Keyboard LED status changed ==== \n",
                    entry("FROM: %d ----> TO: %d", cl->lastKeyboardLedState,
                          serverdata->input.getkeyboardLedState()));

                cl->lastKeyboardLedState =
                    serverdata->input.getkeyboardLedState();

                rfbSendKeyboardLedState(cl);
            }
        }
        if (cl->enableLastRectEncoding)
        {
            rfbSendLastRectMarker(cl);
        }
        rfbSendUpdateBuf(cl);
    }

    rfbReleaseClientIterator(it);

    if (frame_sent)
        video.releaseFrames();
}

void Server::clientFramebufferUpdateRequest(
    rfbClientPtr cl, rfbFramebufferUpdateRequestMsg* furMsg)
{
    ClientData* cd = (ClientData*)cl->clientData;

    if (!cd)
        return;

    // Ignore the furMsg info. This service uses full frame update always.
    (void)furMsg;

    cd->needUpdate = true;
}

void Server::clientGone(rfbClientPtr cl)
{
    Server* server = (Server*)cl->screen->screenData;
    ClientData* cd = (ClientData*)cl->clientData;

    /* Method call for unregistering */
    for (const auto& id : activeSessionIDs)
    {
        if (cd->sessionId == id)
        {
            auto busUnRegister = sdbusplus::bus::new_default_system();
            auto m = busUnRegister.new_method_call(
                smgrService.c_str(), smgrObjPath.c_str(), smgrIface.c_str(),
                "SessionUnregister");
            uint8_t sessionType = KVM;
            int reason = LOGOUT;

            m.append(cd->sessionId, sessionType, reason);
            auto reply = busUnRegister.call(m);
            bool status = false;

            reply.read(status);
        }
    }

    delete (ClientData*)cl->clientData;
    cl->clientData = nullptr;

    if (server->numClients-- == 1)
    {
        server->input.disconnect();
        updatePowerSaveMode(1);
        rfbMarkRectAsModified(server->server, 0, 0, server->video.getWidth(),
                              server->video.getHeight());
    }
}

enum rfbNewClientAction Server::newClient(rfbClientPtr cl)
{
    Server* server = (Server*)cl->screen->screenData;

    cl->clientData = new ClientData(ROUND_DOWN(server->video.getFrameRate(), 8),
                                    &server->input);
    cl->clientGoneHook = clientGone;
    cl->clientFramebufferUpdateRequestHook = clientFramebufferUpdateRequest;

    ClientData* cd = (ClientData*)cl->clientData;

    updatePowerSaveMode(0); // Disable power saving mode

    /* Method call for Registering */
    auto busRegister = sdbusplus::bus::new_default_system();
    auto m =
        busRegister.new_method_call(smgrService.c_str(), smgrObjPath.c_str(),
                                    smgrIface.c_str(), "SessionRegister");
    std::string ipAdress = DEFAULT_IP;
    std::string userName = USER_NAME;
    uint8_t sessionType = KVM;
    uint8_t privilege = PRIV_LEVEL_ADMIN;
    uint8_t userId = KVM_DEFAULT_USER_ID;
    std::string mountingMethod = MOUNTING_METHOD;

    propertyValue propertyval;

    m.append(cd->sessionId, ipAdress, userName, sessionType, privilege, userId,
             mountingMethod);

    auto reply = busRegister.call(m);
    bool status = false;
    reply.read(status);

    if (status)
    {
        auto msg2 = busRegister.new_method_call(
            smgrService.c_str(), smgrObjPath.c_str(), DBUS_PROPERTIES_INTERFACE,
            "Get");

        msg2.append(smgrKVMIface, "KvmSessionInfo");

        auto reply1 = busRegister.call(msg2);
        reply1.read(propertyval);

        if (std::holds_alternative<sessionRet>(propertyval))
        {
            sessionRet& vec = std::get<sessionRet>(propertyval);
            if (!vec.empty())
            {
                const auto& latestEntry = vec.back(); /* Get the last element */
                cd->sessionId = static_cast<uint8_t>(std::get<0>(latestEntry));
                // Add the session ID to the vector
                activeSessionIDs.push_back(cd->sessionId);
            }
        }
    }

    if (!server->numClients++)
    {
        server->input.connect();
        server->pendingResize = false;
        server->frameCounter = 0;
    }

    return RFB_CLIENT_ACCEPT;
}

void Server::doResize()
{
    rfbClientIteratorPtr it;
    rfbClientPtr cl;

    framebuffer.resize(
        video.getHeight() * video.getWidth() * Video::bytesPerPixel, 0);

    rfbNewFramebuffer(server, framebuffer.data(), video.getWidth(),
                      video.getHeight(), Video::bitsPerSample,
                      Video::samplesPerPixel, Video::bytesPerPixel);
    rfbMarkRectAsModified(server, 0, 0, video.getWidth(), video.getHeight());

    it = rfbGetClientIterator(server);

    while ((cl = rfbClientIteratorNext(it)))
    {
        ClientData* cd = (ClientData*)cl->clientData;

        if (!cd)
        {
            continue;
        }

        // let skipFrame round-down per interval of aspeed's I frame
        // delay video updates to give the client time to resize
        cd->skipFrame = video.getFrameRate();
    }

    rfbReleaseClientIterator(it);
}

} // namespace ikvm
