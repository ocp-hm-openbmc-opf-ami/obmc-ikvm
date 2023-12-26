#include "ikvm_server.hpp"

#include <linux/videodev2.h>
#include <rfb/rfbproto.h>

#include <boost/crc.hpp>
#include <phosphor-logging/elog-errors.hpp>
#include <phosphor-logging/elog.hpp>
#include <phosphor-logging/log.hpp>
#include <xyz/openbmc_project/Common/error.hpp>

#define ROUND_DOWN(x, r) ((x) & ~((r)-1))

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
    server->desktopName = "OpenBMC IKVM";
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

        if (!cd)
        {
            continue;
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

        if (calcFrameCRC)
        {
            if (frame_crc == -1)
            {
                /* JFIF header contains some varying data so skip it for
                 * checksum calculation */
                frame_crc = boost::crc<32, 0x04C11DB7, 0xFFFFFFFF, 0xFFFFFFFF,
                                       true, true>(data + 0x30,
                                                   video.getFrameSize() - 0x30);
            }

            if (cd->last_crc == frame_crc)
            {
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
                if (rfbTightEncodingSupported())
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
                if (rfbTightEncodingSupported())
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

    delete (ClientData*)cl->clientData;
    cl->clientData = nullptr;

    if (server->numClients-- == 1)
    {
        server->input.disconnect();
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
