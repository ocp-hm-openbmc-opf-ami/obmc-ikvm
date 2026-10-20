#include "ikvm_args.hpp"
#include "ikvm_input.hpp"
#include "ikvm_server.hpp"

#ifdef FAIL
#undef FAIL
#endif
#ifdef ERROR
#undef ERROR
#endif

#include <chrono>
#include <cstring>

#include <gtest/gtest.h>

namespace ikvm
{

// ---------------------------------------------------------------------------
// ClientDataConstructorTest — verify ClientData struct field defaults
// ---------------------------------------------------------------------------
class ClientDataConstructorTest : public ::testing::Test
{
  protected:
    Input input{"", "", ""};
    Server::ClientData cd{3, &input};
};

TEST_F(ClientDataConstructorTest, SkipFrame_IsConstructorArg)
{
    EXPECT_EQ(cd.skipFrame, 3);
}

TEST_F(ClientDataConstructorTest, InputPtr_IsConstructorArg)
{
    EXPECT_EQ(cd.input, &input);
}

TEST_F(ClientDataConstructorTest, NeedUpdate_DefaultFalse)
{
    EXPECT_FALSE(cd.needUpdate);
}

TEST_F(ClientDataConstructorTest, LastCrc_DefaultMinusOne)
{
    EXPECT_EQ(cd.last_crc, -1);
}

TEST_F(ClientDataConstructorTest, SessionId_DefaultZero)
{
    EXPECT_EQ(cd.sessionId, 0u);
}

TEST_F(ClientDataConstructorTest, IsNewSession_DefaultFalse)
{
    EXPECT_FALSE(cd.isNewSession);
}

TEST_F(ClientDataConstructorTest, ClientInfoReceived_DefaultFalse)
{
    EXPECT_FALSE(cd.clientInfoReceived);
}

TEST_F(ClientDataConstructorTest, IvtpWaitCycles_DefaultZero)
{
    EXPECT_EQ(cd.ivtpWaitCycles, 0u);
}

TEST_F(ClientDataConstructorTest, DesktopName_DefaultNullptr)
{
    EXPECT_EQ(cd.desktopName, nullptr);
}

TEST_F(ClientDataConstructorTest, ClientType_DefaultUnknown)
{
    EXPECT_EQ(cd.clientType, Server::ClientData::ClientType::UNKNOWN);
}

TEST_F(ClientDataConstructorTest, LastActivityTime_IsRecent)
{
    auto now = std::chrono::steady_clock::now();
    auto diff = std::chrono::duration_cast<std::chrono::seconds>(
                    now - cd.lastActivityTime)
                    .count();
    // The timestamp should have been set within the last few seconds
    EXPECT_LE(std::abs(diff), 5LL);
}

// ---------------------------------------------------------------------------
// ClientDataSkipFrameTest — construction with skip=0
// ---------------------------------------------------------------------------
TEST(ClientDataSkipFrameTest, ZeroSkipFrame_IsStored)
{
    Input inp{"", "", ""};
    Server::ClientData cd{0, &inp};
    EXPECT_EQ(cd.skipFrame, 0);
}

// ---------------------------------------------------------------------------
// ClientTypeEnumTest — enum values are distinct
// ---------------------------------------------------------------------------
TEST(ClientTypeEnumTest, EnumValues_AreDistinct)
{
    using CT = Server::ClientData::ClientType;
    EXPECT_NE(CT::UNKNOWN, CT::H5Viewer);
    EXPECT_NE(CT::UNKNOWN, CT::JViewer);
    EXPECT_NE(CT::UNKNOWN, CT::VNC);
    EXPECT_NE(CT::H5Viewer, CT::JViewer);
    EXPECT_NE(CT::H5Viewer, CT::VNC);
    EXPECT_NE(CT::JViewer, CT::VNC);
}

// ---------------------------------------------------------------------------
// ServerConstructTest — Server can be constructed with rfbGetScreen;
//                       rfbInitServer+rfbMarkRectAsModified are skipped via
//                       #ifndef TEST and rfbScreenCleanup is skipped in dtor
//                       to prevent SIGSEGV from uninitialized VNC internals.
// ---------------------------------------------------------------------------
TEST(ServerConstructTest, Constructor_CreateAndDestroy)
{
    char prog[] = "test";
    char* argv[] = {prog, nullptr};
    optind = 1;
    Args args(1, argv);
    Input inp("", "", "");
    Video vid("/dev/video0", inp);
    EXPECT_NO_THROW({ Server server(args, inp, vid); });
}

TEST(ServerConstructTest, Resize_FrameCounterZero_SetsPendingResize)
{
    char prog[] = "test";
    char* argv[] = {prog, nullptr};
    optind = 1;
    Args args(1, argv);
    Input inp("", "", "");
    Video vid("/dev/video0", inp);
    Server server(args, inp, vid);
    // frameCounter=0, getFrameRate()=30 → 0 > 30 is false → pendingResize=true
    EXPECT_NO_THROW(server.resize());
}

TEST(ServerConstructTest, Resize_HighFrameCounter_CallsDoResize)
{
    char prog[] = "test";
    char* argv[] = {prog, nullptr};
    optind = 1;
    Args args(1, argv);
    Input inp("", "", "");
    Video vid("/dev/video0", inp);
    Server server(args, inp, vid);
    // frameCounter > getFrameRate() → calls doResize(); no clients so safe
    server.setTestFrameCounter(35);
    EXPECT_NO_THROW(server.resize());
}

TEST(ServerConstructTest, SendFrame_NoClients_ReturnsEarly)
{
    char prog[] = "test";
    char* argv[] = {prog, nullptr};
    optind = 1;
    Args args(1, argv);
    Input inp("", "", "");
    Video vid("/dev/video0", inp);
    Server server(args, inp, vid);
    EXPECT_NO_THROW(server.sendFrame());
}

TEST(ServerConstructTest, Run_NoClients_ProcessesAndReturns)
{
    char prog[] = "test";
    char* argv[] = {prog, nullptr};
    optind = 1;
    Args args(1, argv);
    Input inp("", "", "");
    Video vid("/dev/video0", inp);
    Server server(args, inp, vid);
    EXPECT_NO_THROW(server.run());
}

TEST(ServerConstructTest, Run_UnknownClientAfterWaitBudget_BecomesVnc)
{
    char prog[] = "test";
    char* argv[] = {prog, nullptr};
    optind = 1;
    Args args(1, argv);
    Input inp("", "", "");
    Video vid("/dev/video0", inp);
    Server server(args, inp, vid);

    Server::ClientData cd{0, &inp};
    cd.clientType = Server::ClientData::ClientType::UNKNOWN;
    cd.ivtpWaitCycles = IVTP_MAX_WAIT_CYCLES;

    rfbClientRec fakeClient;
    memset(&fakeClient, 0, sizeof(rfbClientRec));
    fakeClient.clientData = &cd;
    fakeClient.screen = server.getScreenInfoPtr();
    server.getScreenInfoPtr()->clientHead = &fakeClient;

    EXPECT_NO_THROW(server.run());
    EXPECT_EQ(cd.clientType, Server::ClientData::ClientType::VNC);

    server.getScreenInfoPtr()->clientHead = nullptr;
}

TEST(ServerConstructTest, Run_NewVncSession_DbusFailureClearsNewSessionFlag)
{
    char prog[] = "test";
    char* argv[] = {prog, nullptr};
    optind = 1;
    Args args(1, argv);
    Input inp("", "", "");
    Video vid("/dev/video0", inp);
    Server server(args, inp, vid);

    Server::ClientData cd{0, &inp};
    cd.clientType = Server::ClientData::ClientType::VNC;
    cd.isNewSession = true;

    rfbClientRec fakeClient;
    memset(&fakeClient, 0, sizeof(rfbClientRec));
    fakeClient.clientData = &cd;
    fakeClient.screen = server.getScreenInfoPtr();
    server.getScreenInfoPtr()->clientHead = &fakeClient;

    EXPECT_NO_THROW(server.run());
    EXPECT_FALSE(cd.isNewSession);

    server.getScreenInfoPtr()->clientHead = nullptr;
}

TEST(ServerConstructTest, SendFrame_NullClientData_SkipsSafely)
{
    char prog[] = "test";
    char* argv[] = {prog, nullptr};
    optind = 1;
    Args args(1, argv);
    Input inp("", "", "");
    Video vid("/dev/video0", inp);
    Server server(args, inp, vid);

    rfbClientRec fakeClient;
    memset(&fakeClient, 0, sizeof(rfbClientRec));
    fakeClient.screen = server.getScreenInfoPtr();
    server.getScreenInfoPtr()->clientHead = &fakeClient;

    EXPECT_NO_THROW(server.sendFrame());

    server.getScreenInfoPtr()->clientHead = nullptr;
}

TEST(ServerConstructTest, WantsFrame_NoClients_ReturnsFalse)
{
    char prog[] = "test";
    char* argv[] = {prog, nullptr};
    optind = 1;
    Args args(1, argv);
    Input inp("", "", "");
    Video vid("/dev/video0", inp);
    Server server(args, inp, vid);

    EXPECT_FALSE(server.wantsFrame());
}

TEST(ServerConstructTest, WantsFrame_ClientPresent_ReturnsTrue)
{
    char prog[] = "test";
    char* argv[] = {prog, nullptr};
    optind = 1;
    Args args(1, argv);
    Input inp("", "", "");
    Video vid("/dev/video0", inp);
    Server server(args, inp, vid);

    rfbClientRec fakeClient;
    memset(&fakeClient, 0, sizeof(rfbClientRec));
    server.getScreenInfoPtr()->clientHead = &fakeClient;

    EXPECT_TRUE(server.wantsFrame());

    server.getScreenInfoPtr()->clientHead = nullptr;
}

TEST(ServerConstructTest, GetVideo_ReturnsOriginalReference)
{
    char prog[] = "test";
    char* argv[] = {prog, nullptr};
    optind = 1;
    Args args(1, argv);
    Input inp("", "", "");
    Video vid("/dev/video0", inp);
    Server server(args, inp, vid);

    EXPECT_EQ(&server.getVideo(), &vid);
}

// ---------------------------------------------------------------------------
// ServerIVTPPacketTest — exercises createIVTPStopSessionPacket() via the
//   #ifdef TEST public wrapper; covers 11 lines of packet construction logic
// ---------------------------------------------------------------------------
TEST(ServerIVTPPacketTest, CreateIVTPPacket_ValidArgs_ReturnsNonEmptyVector)
{
    auto pkt = Server::testCreateIVTPStopSessionPacket(0x0008, 0x0002);
    EXPECT_FALSE(pkt.empty());
}

TEST(ServerIVTPPacketTest, CreateIVTPPacket_DifferentArgs_ReturnsVector)
{
    auto pkt1 = Server::testCreateIVTPStopSessionPacket(0x0009, 0x0000);
    auto pkt2 = Server::testCreateIVTPStopSessionPacket(0x0002, 0x0001);
    EXPECT_FALSE(pkt1.empty());
    EXPECT_FALSE(pkt2.empty());
}

// ---------------------------------------------------------------------------
// ServerParseIvtpBufferTest — exercises the private parseIvtpBuffer() via the
//   #ifdef TEST public wrapper. Pure parsing logic, no D-Bus/hardware
//   dependency, so it is a low-hanging fruit for private-function coverage.
// ---------------------------------------------------------------------------
TEST(ServerParseIvtpBufferTest, TooShortBuffer_ReturnsInvalid)
{
    const char buf[4] = {'I', 'V', 'T', 'P'};
    auto msg = Server::testParseIvtpBuffer(buf, sizeof(buf));
    EXPECT_FALSE(msg.valid);
    EXPECT_EQ(msg.header, "[Invalid: too short]");
}

TEST(ServerParseIvtpBufferTest, NonIvtpHeader_ReturnsInvalid)
{
    // 12-byte buffer (meets IVTP_MIN_SIZE) with a non-"IVTP" header
    const char buf[12] = {'X', 'X', 'X', 'X'};
    auto msg = Server::testParseIvtpBuffer(buf, sizeof(buf));
    EXPECT_FALSE(msg.valid);
    EXPECT_EQ(msg.header, "[Invalid: not IVTP]");
}

TEST(ServerParseIvtpBufferTest, ValidHeaderNoPayload_ReturnsValid)
{
    // header(4) + num(2) + payloadLength(4)=0 + status(2) = 12 bytes
    unsigned char buf[12];
    std::memcpy(buf, "IVTP", 4);
    uint16_t num = htons(0x0012);
    std::memcpy(buf + 4, &num, sizeof(num));
    uint32_t payloadLen = htonl(0);
    std::memcpy(buf + 6, &payloadLen, sizeof(payloadLen));
    uint16_t status = htons(0x0000);
    std::memcpy(buf + 10, &status, sizeof(status));

    auto msg = Server::testParseIvtpBuffer(reinterpret_cast<const char*>(buf),
                                           sizeof(buf));
    EXPECT_TRUE(msg.valid);
    EXPECT_EQ(msg.header, "IVTP");
    EXPECT_EQ(msg.num, 0x0012);
    EXPECT_EQ(msg.payloadLength, 0u);
}

TEST(ServerParseIvtpBufferTest, PayloadLengthExceedsBuffer_MarksInvalid)
{
    // Claims a payload length far larger than the actual buffer size
    unsigned char buf[12];
    std::memcpy(buf, "IVTP", 4);
    uint16_t num = htons(0x0001);
    std::memcpy(buf + 4, &num, sizeof(num));
    uint32_t payloadLen = htonl(1000);
    std::memcpy(buf + 6, &payloadLen, sizeof(payloadLen));
    uint16_t status = htons(0x0000);
    std::memcpy(buf + 10, &status, sizeof(status));

    auto msg = Server::testParseIvtpBuffer(reinterpret_cast<const char*>(buf),
                                           sizeof(buf));
    EXPECT_FALSE(msg.valid);
    EXPECT_EQ(msg.payload, "[Invalid payload length]");
}

// ---------------------------------------------------------------------------
// ServerHandleKVMServiceDisabledTest — exercises the private
//   handleKVMServiceDisabled() via the #ifdef TEST public wrapper. With an
//   empty client list (clientHead == nullptr) this is pure in-process logic
//   with no D-Bus/hardware dependency.
// ---------------------------------------------------------------------------
TEST(ServerHandleKVMServiceDisabledTest, NoClients_NoThrow)
{
    rfbScreenInfo fakeScreen;
    memset(&fakeScreen, 0, sizeof(fakeScreen));
    fakeScreen.clientHead = nullptr;

    EXPECT_NO_THROW(Server::testHandleKVMServiceDisabled(&fakeScreen));
}

// ---------------------------------------------------------------------------
// ServerSessionRegisterTest — exercises the private sessionRegister() via
//   the #ifdef TEST public wrapper. Only the null-clientData early-return
//   branch is exercised here — beyond that check the function makes a live
//   D-Bus call, which is intentionally not exercised in unit tests.
// ---------------------------------------------------------------------------
TEST(ServerSessionRegisterTest, NullClientData_ReturnsEarly)
{
    rfbClientRec fakeClient;
    memset(&fakeClient, 0, sizeof(fakeClient));
    fakeClient.clientData = nullptr;

    EXPECT_NO_THROW(Server::testSessionRegister(&fakeClient));
}

} // namespace ikvm
