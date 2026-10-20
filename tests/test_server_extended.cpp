#include "ami/include/ikvm_utils.hpp"

#include <arpa/inet.h>
#include <rfb/rfb.h>
#include <rfb/rfbproto.h>

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#pragma push_macro("private")
#undef private
#define private public
#include "fake_v4l2.hpp"
#include "ikvm_server.hpp"
#include "ikvm_video.hpp"
#pragma pop_macro("private")

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
std::vector<uint8_t> lastWriteBuffer;
std::vector<std::vector<uint8_t>> writeBuffers;
std::vector<rfbClientPtr> closedClients;
std::vector<rfbClientPtr> updateClients;
int writeCount = 0;
int processEventsCount = 0;
int newFramebufferCount = 0;
int markModifiedCount = 0;
int compressedDataCount = 0;
bool writeShouldFail = false;

struct ClientIterator
{
    rfbClientPtr current;
};

std::vector<char> makeIvtpPacket(uint16_t number, uint16_t status,
                                 const std::string& payload)
{
    std::vector<char> packet;
    packet.insert(packet.end(), {'I', 'V', 'T', 'P'});
    const uint16_t numberBe = htons(number);
    packet.insert(packet.end(), reinterpret_cast<const char*>(&numberBe),
                  reinterpret_cast<const char*>(&numberBe) + sizeof(numberBe));
    const uint32_t lengthBe = htonl(payload.size());
    packet.insert(packet.end(), reinterpret_cast<const char*>(&lengthBe),
                  reinterpret_cast<const char*>(&lengthBe) + sizeof(lengthBe));
    const uint16_t statusBe = htons(status);
    packet.insert(packet.end(), reinterpret_cast<const char*>(&statusBe),
                  reinterpret_cast<const char*>(&statusBe) + sizeof(statusBe));
    packet.insert(packet.end(), payload.begin(), payload.end());
    return packet;
}
} // namespace

extern "C" int rfbWriteExact(rfbClientPtr, const char* buffer, int length)
{
    lastWriteBuffer.assign(reinterpret_cast<const uint8_t*>(buffer),
                           reinterpret_cast<const uint8_t*>(buffer) + length);
    writeBuffers.push_back(lastWriteBuffer);
    writeCount++;
    return writeShouldFail ? -1 : length;
}

extern "C" void rfbCloseClient(rfbClientPtr client)
{
    closedClients.push_back(client);
}

extern "C" rfbBool rfbProcessEvents(rfbScreenInfoPtr, long int)
{
    processEventsCount++;
    return TRUE;
}

extern "C" rfbBool rfbSendUpdateBuf(rfbClientPtr client)
{
    updateClients.push_back(client);
    return TRUE;
}

extern "C" rfbBool rfbSendKeyboardLedState(rfbClientPtr)
{
    return TRUE;
}

extern "C" rfbBool rfbSendLastRectMarker(rfbClientPtr)
{
    return TRUE;
}

extern "C" void rfbDefaultPtrAddEvent(int, int, int, rfbClientPtr) {}

extern "C" void rfbNewFramebuffer(rfbScreenInfoPtr, char*, int, int, int, int,
                                  int)
{
    newFramebufferCount++;
}

extern "C" void rfbScreenCleanup(rfbScreenInfoPtr) {}

extern "C" rfbBool rfbSendTightHeader(rfbClientPtr, int, int, int, int)
{
    return TRUE;
}

extern "C" rfbBool rfbSendCompressedDataTight(rfbClientPtr, char*, int)
{
    compressedDataCount++;
    return TRUE;
}

extern "C" void rfbMarkRectAsModified(rfbScreenInfoPtr, int, int, int, int)
{
    markModifiedCount++;
}

extern "C" rfbClientIteratorPtr rfbGetClientIterator(rfbScreenInfoPtr screen)
{
    return reinterpret_cast<rfbClientIteratorPtr>(
        new ClientIterator{screen->clientHead});
}

extern "C" rfbClientPtr rfbClientIteratorNext(rfbClientIteratorPtr iterator)
{
    auto* state = reinterpret_cast<ClientIterator*>(iterator);
    if (state == nullptr || state->current == nullptr)
    {
        return nullptr;
    }
    rfbClientPtr current = state->current;
    state->current = state->current->next;
    return current;
}

extern "C" void rfbReleaseClientIterator(rfbClientIteratorPtr iterator)
{
    delete reinterpret_cast<ClientIterator*>(iterator);
}

extern "C" rfbScreenInfoPtr rfbGetScreen(int*, char**, int, int, int, int, int)
{
    return new rfbScreenInfo{};
}

extern "C" void rfbInitServer(rfbScreenInfoPtr) {}

extern "C" rfbCursorPtr rfbMakeXCursor(int, int, char*, char*)
{
    return new rfbCursor{};
}

extern "C" int rfbStringToAddr(char*, in_addr_t* address)
{
    if (address != nullptr)
    {
        *address = 0;
    }
    return 1;
}

class ServerExtendedTest : public testing::Test
{
  protected:
    void SetUp() override
    {
        lastWriteBuffer.clear();
        writeBuffers.clear();
        closedClients.clear();
        updateClients.clear();
        writeCount = 0;
        processEventsCount = 0;
        newFramebufferCount = 0;
        markModifiedCount = 0;
        compressedDataCount = 0;
        writeShouldFail = false;
        ikvm::activeSessionIDs.clear();
        ikvm::isKvmDisabled = false;
        ikvm::isAst2700Platform = false;
        ikvm::timeoutValue = std::chrono::seconds(DEFAULT_TIMEOUT_VALUE);
        ikvm::testSessionManagerPropertyHook = {};
        ikvm::testSessionRegisterHook = {};
        ikvm::testSessionUnregisterHook = {};
        ikvm::testPowerSaveModeHook = {};
        ikvm::testEventLogHook = {};
        ikvm::amiWrapLog.eventLogCalls = 0;
        ikvm::amiWrapLog.eventLogMessages.clear();
    }

    struct Context
    {
        ikvm::Input input;
        ikvm::Video video;
        ikvm::Server server;
        rfbScreenInfo screen{};
        rfbClientRec client{};
        ikvm::Server::ClientData* data;

        Context() : video(input, 640, 480, 30), server(input, video)
        {
            screen.screenData = &server;
            screen.clientHead = &client;
            screen.width = 640;
            screen.height = 480;
            server.server = &screen;
            client.screen = &screen;
            data = new ikvm::Server::ClientData(0, &input);
            client.clientData = data;
        }

        ~Context()
        {
            delete data;
        }
    };

    struct FrameContext : Context
    {
        std::vector<char> frame;

        FrameContext()
        {
            video.buffers.resize(1);
            video.buffers[0].queued = true;
            video.buffers[0].box = {1, 2, 32, 24};
        }

        void setFrame(std::vector<char> bytes)
        {
            frame = std::move(bytes);
            video.buffers[0].data = frame.data();
            video.buffers[0].size = frame.size();
            video.buffers[0].payload = frame.size();
            video.buffers[0].sequence = 1;
            video.buffersDone = {0};
        }
    };
};

TEST_F(ServerExtendedTest, ParseIvtpBuffer_ValidPacket_PreservesAllFields)
{
    auto packet = makeIvtpPacket(0x1234, 0x55AA, "session_7");
    auto message = ikvm::Server::parseIvtpBuffer(packet.data(), packet.size());
    ASSERT_TRUE(message.valid);
    EXPECT_EQ(message.num, 0x1234);
    EXPECT_EQ(message.status, 0x55AA);
    EXPECT_EQ(message.payloadLength, 9u);
    EXPECT_EQ(message.payload, "session_7");
}

TEST_F(ServerExtendedTest,
       CreateIVTPStopSessionPacket_ValidParams_UsesImmediateAndStatus)
{
    auto packet = ikvm::Server::createIVTPStopSessionPacket(0x9999, 0x00AA);
    ASSERT_EQ(packet.size(), 12u);
    uint16_t number = 0;
    uint16_t status = 0;
    std::memcpy(&number, packet.data() + 4, sizeof(number));
    std::memcpy(&status, packet.data() + 10, sizeof(status));
    EXPECT_EQ(ntohs(number), IVTP_STOP_SESSION_IMMEDIATE);
    EXPECT_EQ(ntohs(status), 0x00AA);
}

TEST_F(ServerExtendedTest,
       SendIVTPMessageToClient_ValidPacket_WrapsInCutTextMessage)
{
    Context context;
    auto packet = ikvm::Server::createIVTPStopSessionPacket(1, 2);
    ikvm::Server::sendIVTPMessageToClient(&context.client, packet.data(),
                                          packet.size());
    ASSERT_EQ(writeCount, 1);
    ASSERT_EQ(lastWriteBuffer.size(), packet.size() + 8);
    EXPECT_EQ(lastWriteBuffer[0], 3);
    uint32_t length = 0;
    std::memcpy(&length, lastWriteBuffer.data() + 4, sizeof(length));
    EXPECT_EQ(ntohl(length), packet.size());
    EXPECT_TRUE(std::equal(lastWriteBuffer.begin() + 8, lastWriteBuffer.end(),
                           packet.begin()));
}

TEST_F(ServerExtendedTest, SendIVTPMessageToClient_WriteFails_ContainsException)
{
    Context context;
    auto packet = ikvm::Server::createIVTPStopSessionPacket(1, 2);
    writeShouldFail = true;
    EXPECT_NO_THROW(ikvm::Server::sendIVTPMessageToClient(
        &context.client, packet.data(), packet.size()));
    EXPECT_EQ(writeCount, 1);
}

TEST_F(ServerExtendedTest,
       SendDisconnectMessageToClients_MultipleClients_ReachesAll)
{
    Context context;
    rfbClientRec second{};
    second.screen = &context.screen;
    context.client.next = &second;
    ikvm::Server::sendDisconnectMessageToClients(&context.screen, 1, 2);
    ASSERT_EQ(writeBuffers.size(), 2u);
    EXPECT_EQ(writeBuffers[0], writeBuffers[1]);
}

TEST_F(ServerExtendedTest, SendFrame_ServiceDisabled_BroadcastsAndClosesClient)
{
    FrameContext context;
    context.data->needUpdate = true;
    context.setFrame({'J', 'P', 'E', 'G', 0, 1, char(0xFF), char(0xD9)});
    ikvm::isKvmDisabled = true;
    context.server.sendFrame();
    EXPECT_EQ(writeCount, 1);
    ASSERT_EQ(closedClients.size(), 1u);
    EXPECT_EQ(closedClients[0], &context.client);
}

TEST_F(ServerExtendedTest, SessionTimeOut_ClientIdle_ClosesWithTimeoutMessage)
{
    Context context;
    context.data->lastActivityTime =
        std::chrono::steady_clock::now() - std::chrono::seconds(5);
    ikvm::timeoutValue = std::chrono::seconds(1);
    ikvm::Server::sessionTimeOut(&context.client);
    EXPECT_EQ(writeCount, 1);
    ASSERT_EQ(closedClients.size(), 1u);
    EXPECT_EQ(closedClients[0], &context.client);
}

TEST_F(ServerExtendedTest, SessionTimeOut_ClientActive_DoesNotCloseClient)
{
    Context context;
    context.data->lastActivityTime = std::chrono::steady_clock::now();
    ikvm::timeoutValue = std::chrono::seconds(30);
    ikvm::Server::sessionTimeOut(&context.client);
    EXPECT_TRUE(closedClients.empty());
    EXPECT_EQ(writeCount, 0);
}

TEST_F(ServerExtendedTest,
       ClientFramebufferUpdateRequest_FrameRequested_SetsNeedUpdate)
{
    Context context;
    rfbFramebufferUpdateRequestMsg request{};
    ikvm::Server::clientFramebufferUpdateRequest(&context.client, &request);
    EXPECT_TRUE(context.data->needUpdate);
}

TEST_F(ServerExtendedTest, SendFrame_InvalidJpeg_DoesNotAdvanceCrc)
{
    FrameContext context;
    context.server.calcFrameCRC = true;
    context.data->needUpdate = true;
    context.setFrame(std::vector<char>(64, 'a'));
    context.server.sendFrame();
    EXPECT_EQ(context.data->last_crc, -1);
    EXPECT_TRUE(context.data->needUpdate);
    EXPECT_EQ(compressedDataCount, 0);
    EXPECT_TRUE(context.video.buffersDone.empty());
}

TEST_F(ServerExtendedTest, SendFrame_SessionRemoved_ClosesClient)
{
    FrameContext context;
    context.data->needUpdate = true;
    context.data->sessionId = 9;
    context.setFrame({'J', 'P', 'E', 'G', 0, 1, char(0xFF), char(0xD9)});
    context.server.sendFrame();
    ASSERT_EQ(closedClients.size(), 1u);
    EXPECT_EQ(closedClients[0], &context.client);
}

TEST_F(ServerExtendedTest, SendFrame_SkipFrameSet_DecrementsWithoutSending)
{
    FrameContext context;
    context.data->needUpdate = true;
    context.data->skipFrame = 2;
    context.setFrame({'J', 'P', 'E', 'G', 0, 1, char(0xFF), char(0xD9)});
    context.server.sendFrame();
    EXPECT_EQ(context.data->skipFrame, 1);
    EXPECT_TRUE(updateClients.empty());
    EXPECT_FALSE(context.video.buffersDone.empty());
}

TEST_F(ServerExtendedTest, SendFrame_InvalidTrailer_RequeuesWithoutSending)
{
    FrameContext context;
    context.data->needUpdate = true;
    context.setFrame({'J', 'P', 'E', 'G', 0, 1, 0, 0});
    context.server.sendFrame();
    EXPECT_TRUE(context.data->needUpdate);
    EXPECT_TRUE(context.video.buffersDone.empty());
    EXPECT_TRUE(updateClients.empty());
}

TEST_F(ServerExtendedTest,
       Resize_FrameRateExceeded_UpdatesFramebufferAndSkipCount)
{
    Context context;
    context.server.frameCounter = context.video.getFrameRate() + 1;
    context.server.resize();
    EXPECT_EQ(newFramebufferCount, 1);
    EXPECT_EQ(markModifiedCount, 1);
    EXPECT_EQ(context.data->skipFrame, 24);
    EXPECT_FALSE(context.server.pendingResize);
}

TEST_F(ServerExtendedTest, Run_PendingResize_CompletesResize)
{
    Context context;
    context.data->isNewSession = false;
    context.server.pendingResize = true;
    context.server.frameCounter = context.video.getFrameRate() + 1;
    context.server.run();
    EXPECT_EQ(newFramebufferCount, 1);
    EXPECT_FALSE(context.server.pendingResize);
}

TEST_F(ServerExtendedTest,
       ClientCutTextMsgHandler_H5PacketReceived_CapturesWebSessionId)
{
    Context context;
    context.client.tightEncodingSupport = TRUE;
    auto packet = makeIvtpPacket(IVTP_VALIDATE_VIDEO_SESSION,
                                 IVTP_GET_WEB_TOKEN, "session_7");
    ikvm::Server::clientCutTextMsgHandler(&context.client, packet.data(),
                                          packet.size());
    EXPECT_TRUE(context.data->clientInfoReceived);
    EXPECT_EQ(context.data->clientType,
              ikvm::Server::ClientData::ClientType::H5Viewer);
    EXPECT_EQ(context.data->webSessionId, 7);
}

TEST_F(ServerExtendedTest,
       ClientCutTextMsgHandler_JViewerPacketReceived_CapturesClientInfo)
{
    Context context;
    context.client.tightEncodingSupport = FALSE;
    auto packet = makeIvtpPacket(
        IVTP_VALIDATE_VIDEO_SESSION, IVTP_SESSION_ACCEPTED,
        "ipAdress=10.0.0.42,userName=alice,privilege=4,userId=6");
    ikvm::Server::clientCutTextMsgHandler(&context.client, packet.data(),
                                          packet.size());
    EXPECT_TRUE(context.data->clientInfoReceived);
    EXPECT_EQ(context.data->clientType,
              ikvm::Server::ClientData::ClientType::JViewer);
    EXPECT_EQ(std::get<1>(context.data->clientInfo), "10.0.0.42");
    EXPECT_EQ(std::get<2>(context.data->clientInfo), "alice");
    EXPECT_EQ(std::get<4>(context.data->clientInfo), 4);
    EXPECT_EQ(std::get<5>(context.data->clientInfo), 6);
}

TEST_F(ServerExtendedTest, ClientCutTextMsgHandler_MismatchedEncoding_Rejects)
{
    Context context;
    context.client.tightEncodingSupport = FALSE;
    auto packet = makeIvtpPacket(IVTP_VALIDATE_VIDEO_SESSION,
                                 IVTP_GET_WEB_TOKEN, "session_7");
    ikvm::Server::clientCutTextMsgHandler(&context.client, packet.data(),
                                          packet.size());
    EXPECT_FALSE(context.data->clientInfoReceived);
    EXPECT_EQ(context.data->clientType,
              ikvm::Server::ClientData::ClientType::UNKNOWN);
}

TEST_F(ServerExtendedTest, ClientCutTextMsgHandler_MalformedOrUnknown_Rejects)
{
    Context context;
    auto malformed =
        makeIvtpPacket(IVTP_VALIDATE_VIDEO_SESSION, IVTP_GET_WEB_TOKEN, "abc");
    const uint32_t badLength = htonl(999);
    std::memcpy(malformed.data() + 6, &badLength, sizeof(badLength));
    ikvm::Server::clientCutTextMsgHandler(&context.client, malformed.data(),
                                          malformed.size());
    auto unknown = makeIvtpPacket(0x9999, 1, "data");
    ikvm::Server::clientCutTextMsgHandler(&context.client, unknown.data(),
                                          unknown.size());
    EXPECT_FALSE(context.data->clientInfoReceived);
    EXPECT_EQ(context.data->clientType,
              ikvm::Server::ClientData::ClientType::UNKNOWN);
}

TEST_F(ServerExtendedTest, SessionRegister_H5Client_RegistersWithSixFieldApi)
{
    Context context;
    context.data->isNewSession = true;
    context.data->webSessionId = 7;
    context.data->clientType = ikvm::Server::ClientData::ClientType::H5Viewer;
    const ikvm::sessionRet webSessions = {std::make_tuple(
        uint8_t{7}, std::string("10.0.0.42"), std::string("alice"), uint8_t{0},
        uint8_t{5}, uint8_t{9})};
    const ikvm::sessionRet kvmSessions = {std::make_tuple(
        uint8_t{11}, std::string("10.0.0.42"), std::string("alice"), uint8_t{0},
        uint8_t{5}, uint8_t{9})};
    int registerCalls = 0;
    ikvm::testSessionManagerPropertyHook =
        [webSessions,
         kvmSessions](const std::string& interface,
                      const std::string& property) -> ikvm::propertyValue {
        if (interface == ikvm::smgrWebIface && property == "WebSessionInfo")
        {
            return webSessions;
        }
        return kvmSessions;
    };
    ikvm::testSessionRegisterHook =
        [&registerCalls](uint8_t sessionId, const std::string& address,
                         const std::string& user, uint8_t type,
                         uint8_t privilege, uint8_t userId) {
            registerCalls++;
            EXPECT_EQ(sessionId, DEFAULT_SID);
            EXPECT_EQ(address, "10.0.0.42");
            EXPECT_EQ(user, "alice");
            EXPECT_EQ(type, KVM);
            EXPECT_EQ(privilege, 5);
            EXPECT_EQ(userId, 9);
            return true;
        };
    ikvm::Server::sessionRegister(&context.client);
    EXPECT_EQ(registerCalls, 1);
    EXPECT_EQ(context.data->sessionId, 11);
    EXPECT_FALSE(context.data->isNewSession);
    EXPECT_THAT(ikvm::activeSessionIDs, testing::ElementsAre(11));
}

TEST_F(ServerExtendedTest, Run_VncClientNewSession_RegistersSession)
{
    Context context;
    context.data->isNewSession = true;
    context.data->clientType = ikvm::Server::ClientData::ClientType::VNC;
    int registerCalls = 0;
    ikvm::testSessionManagerPropertyHook =
        [](const std::string&, const std::string&) -> ikvm::propertyValue {
        return ikvm::sessionRet{};
    };
    ikvm::testSessionRegisterHook =
        [&registerCalls](uint8_t, const std::string& address,
                         const std::string& user, uint8_t, uint8_t, uint8_t) {
            registerCalls++;
            EXPECT_EQ(address, DEFAULT_IP);
            EXPECT_EQ(user, USER_NAME);
            return true;
        };
    context.server.run();
    EXPECT_EQ(registerCalls, 1);
    EXPECT_FALSE(context.data->isNewSession);
}

TEST_F(ServerExtendedTest, NewClient_MultipleClients_AssignsRolesCorrectly)
{
    ikvm::Input input;
    ikvm::Video video(input, 640, 480, 30);
    ikvm::Server server(input, video);
    rfbScreenInfo screen{};
    screen.screenData = &server;
    server.server = &screen;
    rfbClientRec first{}, second{};
    first.screen = &screen;
    second.screen = &screen;

    ASSERT_EQ(ikvm::Server::newClient(&first), RFB_CLIENT_ACCEPT);
    ASSERT_EQ(ikvm::Server::newClient(&second), RFB_CLIENT_ACCEPT);
    EXPECT_FALSE(first.viewOnly);
    EXPECT_TRUE(second.viewOnly);
    ASSERT_NE(first.desktopName, nullptr);
    ASSERT_NE(second.desktopName, nullptr);
    EXPECT_EQ(std::string(first.desktopName), "OneTree IKVM");
    EXPECT_THAT(std::string(second.desktopName),
                testing::HasSubstr("View Only"));

    ikvm::Server::clientGone(&second);
    ikvm::Server::clientGone(&first);
}

TEST_F(ServerExtendedTest, NewClient_ThirdClient_RefusesConnection)
{
    ikvm::Input input;
    ikvm::Video video(input, 640, 480, 30);
    ikvm::Server server(input, video);
    rfbScreenInfo screen{};
    screen.screenData = &server;
    server.server = &screen;
    server.numClients = 2;
    rfbClientRec client{};
    client.screen = &screen;
    EXPECT_EQ(ikvm::Server::newClient(&client), RFB_CLIENT_REFUSE);
}

TEST_F(ServerExtendedTest, ClientGone_TrackedSession_UnregistersSession)
{
    Context context;
    context.server.numClients = 1;
    context.data->sessionId = 23;
    ikvm::activeSessionIDs = {23};
    int unregisterCalls = 0;
    ikvm::testSessionUnregisterHook =
        [&unregisterCalls](uint8_t sessionId, uint8_t sessionType,
                           uint8_t reason) {
            unregisterCalls++;
            EXPECT_EQ(sessionId, 23);
            EXPECT_EQ(sessionType, KVM);
            EXPECT_EQ(reason, LOGOUT);
            return true;
        };
    ikvm::Server::clientGone(&context.client);
    context.data = nullptr;
    EXPECT_EQ(unregisterCalls, 1);
    EXPECT_EQ(context.client.clientData, nullptr);
}
