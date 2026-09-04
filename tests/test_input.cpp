#include "ami/include/keysym_ami.hpp"
#include "ikvm_args.hpp"
#include "ikvm_input.hpp"
#include "ikvm_server.hpp"
#include "ikvm_video.hpp"
#include "scancodes.hpp"

#ifdef FAIL
#undef FAIL
#endif
#ifdef ERROR
#undef ERROR
#endif

#include <fcntl.h>
#include <rfb/keysym.h>
#include <rfb/rfb.h>
#include <unistd.h>

#include <cstring>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

namespace ikvm
{

// ---------------------------------------------------------------------------
// InputConstructorTest — verify field initialisation.
// HID stream open is skipped via -DTEST guard in ikvm_input.cpp.
// ---------------------------------------------------------------------------
class InputConstructorTest : public ::testing::Test
{
  protected:
    Input input{"/dev/hidg0", "/dev/hidg1", "musb-hdrc.0"};
};

TEST_F(InputConstructorTest, InitialLedState_IsInitialLedStateConstant)
{
    EXPECT_EQ(input.getkeyboardLedState(), INITIAL_LED_STATE);
}

// ---------------------------------------------------------------------------
// InputEmptyPathsTest — construction with empty paths must not throw
// ---------------------------------------------------------------------------
TEST(InputEmptyPathsTest, EmptyPaths_DoNotThrow)
{
    EXPECT_NO_THROW({ Input inp("", "", ""); });
}

TEST(InputEmptyPathsTest, NonEmptyPaths_DoNotThrow)
{
    EXPECT_NO_THROW({ Input inp("/dev/hidg0", "/dev/hidg1", "musb-hdrc.0"); });
}

// ---------------------------------------------------------------------------
// InputLedStateTest — LED constant values are defined correctly
// ---------------------------------------------------------------------------
TEST(InputLedStateTest, InitialLedState_Is0xFF)
{
    EXPECT_EQ(INITIAL_LED_STATE, 0xFFu);
}

TEST(InputLedStateTest, FreshInput_LedState_MatchesInitial)
{
    Input inp("", "", "");
    EXPECT_EQ(inp.getkeyboardLedState(), INITIAL_LED_STATE);
}

// ---------------------------------------------------------------------------
// InputMultipleInstancesTest — each Input instance has independent LED state
// ---------------------------------------------------------------------------
TEST(InputMultipleInstancesTest, TwoInstances_SameLedState)
{
    Input a("", "", "");
    Input b("/dev/hidg0", "/dev/hidg1", "udc");

    EXPECT_EQ(a.getkeyboardLedState(), b.getkeyboardLedState());
    EXPECT_EQ(a.getkeyboardLedState(), INITIAL_LED_STATE);
}

// ---------------------------------------------------------------------------
// InputConnectTest — connect() with missing hub path logs error and returns;
//                    with a non-empty udcName the stream write fails silently.
// ---------------------------------------------------------------------------
TEST(InputConnectTest, Connect_UdcEmpty_HubPathMissing_HandlesError)
{
    // udcName="" → scans hub path that doesn't exist in Docker → catches
    // filesystem_error and returns cleanly
    Input inp("", "", "");
    EXPECT_NO_THROW(inp.connect());
}

TEST(InputConnectTest, Connect_UdcSpecified_StreamClosedFails_Silently)
{
    // udcName="test_udc" → writes to hidUdcStream (not open in TEST mode)
    // → write fails silently (no exceptions set) → opens kbd/ptr paths (empty)
    Input inp("", "", "test_udc");
    EXPECT_NO_THROW(inp.connect());
}

// ---------------------------------------------------------------------------
// InputDisconnectTest — disconnect() with all fds=-1 writes empty to stream
// ---------------------------------------------------------------------------
TEST(InputDisconnectTest, Disconnect_NegativeFds_CompletesWithoutCrash)
{
    // fds both -1 → no close(); hidUdcStream write fails silently in test mode
    Input inp("", "", "");
    EXPECT_NO_THROW(inp.disconnect());
}

TEST(InputDisconnectTest, Disconnect_AfterConnect_CompletesWithoutCrash)
{
    Input inp("", "", "test_udc");
    inp.connect(); // completes (stream write silently fails)
    EXPECT_NO_THROW(inp.disconnect());
}

// ---------------------------------------------------------------------------
// InputSendWakeupTest — sendWakeupPacket() with all fds=-1 is a safe no-op
// ---------------------------------------------------------------------------
TEST(InputSendWakeupTest, SendWakeupPacket_NegativeFds_NoWrite)
{
    // pointerFd < 0 → skips pointer wakeup
    // keyboardFd < 0 → skips keyboard wakeup
    Input inp("", "", "");
    EXPECT_NO_THROW(inp.sendWakeupPacket());
}

// ---------------------------------------------------------------------------
// InputKeyEventTest — keyEvent() with keyboardFd=-1 returns before writing
// ---------------------------------------------------------------------------
TEST(InputKeyEventTest, KeyEvent_KeyboardFdNegative_ReturnsEarly)
{
    Input inp("", "", "");
    Server::ClientData cd{0, &inp};

    // Zero-initialize rfbClientRec; only clientData is accessed before early
    // return when keyboardFd < 0
    rfbClientRec fakeClient;
    memset(&fakeClient, 0, sizeof(rfbClientRec));
    fakeClient.clientData = &cd;

    EXPECT_NO_THROW(
        Input::keyEvent(static_cast<rfbBool>(1), XK_a, &fakeClient));
    EXPECT_NO_THROW(
        Input::keyEvent(static_cast<rfbBool>(0), XK_a, &fakeClient));
}

TEST(InputKeyEventTest, KeyEvent_ModifierKey_ReturnsEarlyBeforeWrite)
{
    Input inp("", "", "");
    Server::ClientData cd{0, &inp};
    rfbClientRec fakeClient;
    memset(&fakeClient, 0, sizeof(rfbClientRec));
    fakeClient.clientData = &cd;

    // XK_Shift_L is a modifier key — maps to mod bit, not scancode
    EXPECT_NO_THROW(
        Input::keyEvent(static_cast<rfbBool>(1), XK_Shift_L, &fakeClient));
}

// ---------------------------------------------------------------------------
// InputPointerEventTest — pointerEvent() with pointerFd=-1 returns early via
//                         #ifdef TEST guard in ikvm_input.cpp (no Server
//                         needed)
// ---------------------------------------------------------------------------
TEST(InputPointerEventTest, PointerEvent_PointerFdNegative_ReturnsEarly)
{
    Input inp("", "", "");
    Server::ClientData cd{0, &inp};

    rfbClientRec fakeClient;
    memset(&fakeClient, 0, sizeof(rfbClientRec));
    fakeClient.clientData = &cd;
    // fakeClient.screen is null — #ifdef TEST guard in pointerEvent returns
    // before accessing cl->screen when pointerFd < 0
    EXPECT_NO_THROW(Input::pointerEvent(0, 0, 0, &fakeClient));
    EXPECT_NO_THROW(Input::pointerEvent(1, 50, 100, &fakeClient));
}

// ---------------------------------------------------------------------------
// InputKeyEventFdTest — keyEvent() with a valid fd exercises keyToScancode,
//                       keyToMod, writeKeyboard, and readKeyBoardOutReport.
//                       /dev/null as fd: writes succeed; reads return EOF.
// ---------------------------------------------------------------------------
TEST(InputKeyEventFdTest, KeyEvent_ValidFd_CoversKeyLogic)
{
    Input inp("", "", "");
    int devnull = open("/dev/null", O_RDWR);
    ASSERT_GE(devnull, 0);
    inp.setTestKeyboardFd(devnull);

    Server::ClientData cd{0, &inp};
    rfbClientRec fakeClient;
    memset(&fakeClient, 0, sizeof(rfbClientRec));
    fakeClient.clientData = &cd;

    // Regular key down/up — exercises keyToScancode and writeKeyboard
    EXPECT_NO_THROW(
        Input::keyEvent(static_cast<rfbBool>(1), XK_a, &fakeClient));
    EXPECT_NO_THROW(
        Input::keyEvent(static_cast<rfbBool>(0), XK_a, &fakeClient));

    // Modifier key down/up — exercises keyToMod
    EXPECT_NO_THROW(
        Input::keyEvent(static_cast<rfbBool>(1), XK_Shift_L, &fakeClient));
    EXPECT_NO_THROW(
        Input::keyEvent(static_cast<rfbBool>(0), XK_Shift_L, &fakeClient));

    // Function keys — exercises XK_F1..F12 branch in keyToScancode
    EXPECT_NO_THROW(
        Input::keyEvent(static_cast<rfbBool>(1), XK_F1, &fakeClient));
    EXPECT_NO_THROW(
        Input::keyEvent(static_cast<rfbBool>(0), XK_F1, &fakeClient));

    // Numeric key — exercises '1'..'9' branch
    EXPECT_NO_THROW(
        Input::keyEvent(static_cast<rfbBool>(1), XK_1, &fakeClient));
    EXPECT_NO_THROW(
        Input::keyEvent(static_cast<rfbBool>(0), XK_1, &fakeClient));

    // Special keys — exercises switch-case branch
    EXPECT_NO_THROW(
        Input::keyEvent(static_cast<rfbBool>(1), XK_Return, &fakeClient));
    EXPECT_NO_THROW(
        Input::keyEvent(static_cast<rfbBool>(0), XK_Return, &fakeClient));

    EXPECT_NO_THROW(
        Input::keyEvent(static_cast<rfbBool>(1), XK_space, &fakeClient));
    EXPECT_NO_THROW(
        Input::keyEvent(static_cast<rfbBool>(0), XK_space, &fakeClient));

    inp.setTestKeyboardFd(-1);
    close(devnull);
}

// ---------------------------------------------------------------------------
// InputSendWakeupFdTest — sendWakeupPacket() with valid fds executes all paths
// ---------------------------------------------------------------------------
TEST(InputSendWakeupFdTest, SendWakeupPacket_ValidFds_ExecutesFullPath)
{
    Input inp("", "", "");
    int devnull = open("/dev/null", O_RDWR);
    ASSERT_GE(devnull, 0);
    inp.setTestPointerFd(devnull);
    inp.setTestKeyboardFd(devnull);

    // Both fds valid → pointer wakeup runs, keyboard wakeup runs
    EXPECT_NO_THROW(inp.sendWakeupPacket());

    inp.setTestPointerFd(-1);
    inp.setTestKeyboardFd(-1);
    close(devnull);
}

// ---------------------------------------------------------------------------
// InputKeyEventExtendedTest — exercises more branches in keyToScancode():
//   KP_F1-F4, KP_1-9, and the switch's individual cases.
// ---------------------------------------------------------------------------
TEST(InputKeyEventExtendedTest, KeyEvent_ManyKeyTypes_CoversKeyToScancodeSwitch)
{
    Input inp("", "", "");
    int devnull = open("/dev/null", O_RDWR);
    ASSERT_GE(devnull, 0);
    inp.setTestKeyboardFd(devnull);

    Server::ClientData cd{0, &inp};
    rfbClientRec fakeClient;
    memset(&fakeClient, 0, sizeof(rfbClientRec));
    fakeClient.clientData = &cd;

    // Uppercase letters A-Z (different branch from a-z: 'A' = 0x41)
    Input::keyEvent(static_cast<rfbBool>(1), static_cast<rfbKeySym>('A'),
                    &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), static_cast<rfbKeySym>('A'),
                    &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), static_cast<rfbKeySym>('Z'),
                    &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), static_cast<rfbKeySym>('Z'),
                    &fakeClient);

    // Keypad function keys (XK_KP_F1..F4 branch)
    Input::keyEvent(static_cast<rfbBool>(1), XK_KP_F1, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_KP_F1, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_KP_F4, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_KP_F4, &fakeClient);

    // Keypad digits 1-9 (XK_KP_1..9 branch)
    Input::keyEvent(static_cast<rfbBool>(1), XK_KP_1, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_KP_1, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_KP_9, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_KP_9, &fakeClient);

    // F12 (XK_F12 — end of F-key range)
    Input::keyEvent(static_cast<rfbBool>(1), XK_F12, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_F12, &fakeClient);

    // Switch-case special keys
    Input::keyEvent(static_cast<rfbBool>(1), XK_Tab, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_Tab, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_Escape, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_Escape, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_BackSpace, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_BackSpace, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_minus, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_minus, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_equal, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_equal, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_bracketleft, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_bracketleft, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_semicolon, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_semicolon, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_apostrophe, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_apostrophe, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_grave, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_grave, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_backslash, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_backslash, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_comma, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_comma, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_period, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_period, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_slash, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_slash, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_KP_0, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_KP_0, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_KP_Enter, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_KP_Enter, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_Delete, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_Delete, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_Insert, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_Insert, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_Home, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_Home, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_End, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_End, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_Page_Up, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_Page_Up, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_Page_Down, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_Page_Down, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_Right, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_Right, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_Left, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_Left, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_Down, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_Down, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_Up, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_Up, &fakeClient);
    // Alt and Control modifiers (keyToMod branches)
    Input::keyEvent(static_cast<rfbBool>(1), XK_Alt_L, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_Alt_L, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_Control_L, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_Control_L, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(1), XK_Control_R, &fakeClient);
    Input::keyEvent(static_cast<rfbBool>(0), XK_Control_R, &fakeClient);

    inp.setTestKeyboardFd(-1);
    close(devnull);
}

TEST(InputKeyEventExtendedTest, KeyEvent_TightVncUppercasePath_DoesNotThrow)
{
    Input inp("", "", "");
    int devnull = open("/dev/null", O_RDWR);
    ASSERT_GE(devnull, 0);
    inp.setTestKeyboardFd(devnull);

    Server::ClientData cd{0, &inp};
    rfbClientRec fakeClient;
    memset(&fakeClient, 0, sizeof(rfbClientRec));
    fakeClient.clientData = &cd;
    fakeClient.tightEncodingSupport = 1;
    fakeClient.enableCursorPosUpdates = 1;

    EXPECT_NO_THROW(Input::keyEvent(static_cast<rfbBool>(1),
                                    static_cast<rfbKeySym>('A'), &fakeClient));
    EXPECT_NO_THROW(Input::keyEvent(static_cast<rfbBool>(0),
                                    static_cast<rfbKeySym>('A'), &fakeClient));

    inp.setTestKeyboardFd(-1);
    close(devnull);
}

TEST(InputKeyEventExtendedTest, KeyEvent_ShiftedSymbols_CoversSwitchAliases)
{
    Input inp("", "", "");
    int devnull = open("/dev/null", O_RDWR);
    ASSERT_GE(devnull, 0);
    inp.setTestKeyboardFd(devnull);

    Server::ClientData cd{0, &inp};
    rfbClientRec fakeClient;
    memset(&fakeClient, 0, sizeof(rfbClientRec));
    fakeClient.clientData = &cd;

    EXPECT_NO_THROW(
        Input::keyEvent(static_cast<rfbBool>(1), XK_exclam, &fakeClient));
    EXPECT_NO_THROW(
        Input::keyEvent(static_cast<rfbBool>(0), XK_exclam, &fakeClient));
    EXPECT_NO_THROW(
        Input::keyEvent(static_cast<rfbBool>(1), XK_question, &fakeClient));
    EXPECT_NO_THROW(
        Input::keyEvent(static_cast<rfbBool>(0), XK_question, &fakeClient));
    EXPECT_NO_THROW(
        Input::keyEvent(static_cast<rfbBool>(1), XK_braceright, &fakeClient));
    EXPECT_NO_THROW(
        Input::keyEvent(static_cast<rfbBool>(0), XK_braceright, &fakeClient));

    inp.setTestKeyboardFd(-1);
    close(devnull);
}

// ---------------------------------------------------------------------------
// InputDisconnectFdTest — with valid fds set, disconnect() closes both
// ---------------------------------------------------------------------------
TEST(InputDisconnectFdTest, Disconnect_WithOpenFds_CoversFdCloseBranches)
{
    Input inp("", "", "");
    int dn1 = open("/dev/null", O_RDWR);
    int dn2 = open("/dev/null", O_RDWR);
    ASSERT_GE(dn1, 0);
    ASSERT_GE(dn2, 0);
    inp.setTestKeyboardFd(dn1);
    inp.setTestPointerFd(dn2);
    // disconnect(): keyboardFd >= 0 → close(dn1); pointerFd >= 0 → close(dn2)
    EXPECT_NO_THROW(inp.disconnect());
    // Both fds are now closed by disconnect(); fds set to -1 internally
}

// ---------------------------------------------------------------------------
// InputConnectPathTest — non-empty paths cause open() attempts in connect()
// ---------------------------------------------------------------------------
TEST(InputConnectPathTest, Connect_WithNonEmptyPaths_CoversOpenAttempts)
{
    // keyboard/pointer paths exist but open() fails; udcName="test_udc"
    Input inp("/dev/nonexistent_hidg0", "/dev/nonexistent_hidg1", "test_udc");
    // connect(): !keyboardPath.empty() → open() → ENOENT → log error
    //            !pointerPath.empty()  → open() → ENOENT → log error
    EXPECT_NO_THROW(inp.connect());
}

TEST(InputPointerEventTest, PointerEvent_WithServerAndScrollButtons_NoThrow)
{
    char prog[] = "test";
    char* argv[] = {prog, nullptr};
    optind = 1;

    Args args(1, argv);
    Input inp("", "", "");
    Video video("/dev/video0", inp);
    Server server(args, inp, video);
    int devnull = open("/dev/null", O_RDWR);
    ASSERT_GE(devnull, 0);
    inp.setTestPointerFd(devnull);

    Server::ClientData cd{0, &inp};
    rfbClientRec fakeClient;
    memset(&fakeClient, 0, sizeof(rfbClientRec));
    fakeClient.clientData = &cd;
    fakeClient.screen = server.getScreenInfoPtr();

    EXPECT_NO_THROW(Input::pointerEvent(8, -1, -1, &fakeClient));
    EXPECT_NO_THROW(Input::pointerEvent(16, 100, 200, &fakeClient));
    EXPECT_NO_THROW(Input::pointerEvent(1, 50, 60, &fakeClient));

    inp.setTestPointerFd(-1);
    close(devnull);
}

// ---------------------------------------------------------------------------
// InputKeyToScancodeTest — exercises the private keyToScancode() via the
//   #ifdef TEST public wrapper. Pure lookup-table logic, no D-Bus/hardware
//   dependency, so it is a low-hanging fruit for private-function branch
//   coverage. Table-driven to cover every switch case in a single test.
// ---------------------------------------------------------------------------
TEST(InputKeyToScancodeTest, LetterKeys_MapToLetterScancodes)
{
    EXPECT_EQ(Input::testKeyToScancode('a'), USBHID_KEY_A);
    EXPECT_EQ(Input::testKeyToScancode('A'), USBHID_KEY_A);
    EXPECT_EQ(Input::testKeyToScancode('z'), USBHID_KEY_Z);
    EXPECT_EQ(Input::testKeyToScancode('Z'), USBHID_KEY_Z);
}

TEST(InputKeyToScancodeTest, DigitKeys_MapToDigitScancodes)
{
    EXPECT_EQ(Input::testKeyToScancode('1'), USBHID_KEY_1);
    EXPECT_EQ(Input::testKeyToScancode('9'), USBHID_KEY_9);
}

TEST(InputKeyToScancodeTest, FunctionKeys_MapToFKeyScancodes)
{
    EXPECT_EQ(Input::testKeyToScancode(XK_F1), USBHID_KEY_F1);
    EXPECT_EQ(Input::testKeyToScancode(XK_F12), USBHID_KEY_F12);
}

TEST(InputKeyToScancodeTest, KeypadFunctionKeys_MapToFKeyScancodes)
{
    EXPECT_EQ(Input::testKeyToScancode(XK_KP_F1), USBHID_KEY_F1);
    EXPECT_EQ(Input::testKeyToScancode(XK_KP_F4), USBHID_KEY_F4);
}

TEST(InputKeyToScancodeTest, KeypadDigitKeys_MapToDigitScancodes)
{
    EXPECT_EQ(Input::testKeyToScancode(XK_KP_1), USBHID_KEY_KP_1);
    EXPECT_EQ(Input::testKeyToScancode(XK_KP_9), USBHID_KEY_KP_9);
}

TEST(InputKeyToScancodeTest, SwitchCaseKeys_MapToExpectedScancodes)
{
    const std::vector<std::pair<rfbKeySym, uint8_t>> cases = {
        {XK_exclam, USBHID_KEY_1},
        {XK_at, USBHID_KEY_2},
        {XK_numbersign, USBHID_KEY_3},
        {XK_dollar, USBHID_KEY_4},
        {XK_percent, USBHID_KEY_5},
        {XK_asciicircum, USBHID_KEY_6},
        {XK_ampersand, USBHID_KEY_7},
        {XK_asterisk, USBHID_KEY_8},
        {XK_parenleft, USBHID_KEY_9},
        {XK_0, USBHID_KEY_0},
        {XK_parenright, USBHID_KEY_0},
        {XK_Return, USBHID_KEY_RETURN},
        {XK_Escape, USBHID_KEY_ESC},
        {XK_BackSpace, USBHID_KEY_BACKSPACE},
        {XK_Tab, USBHID_KEY_TAB},
        {XK_KP_Tab, USBHID_KEY_TAB},
        {XK_space, USBHID_KEY_SPACE},
        {XK_KP_Space, USBHID_KEY_SPACE},
        {XK_minus, USBHID_KEY_MINUS},
        {XK_underscore, USBHID_KEY_MINUS},
        {XK_plus, USBHID_KEY_EQUAL},
        {XK_equal, USBHID_KEY_EQUAL},
        {XK_bracketleft, USBHID_KEY_LEFTBRACE},
        {XK_braceleft, USBHID_KEY_LEFTBRACE},
        {XK_bracketright, USBHID_KEY_RIGHTBRACE},
        {XK_braceright, USBHID_KEY_RIGHTBRACE},
        {XK_backslash, USBHID_KEY_BACKSLASH},
        {XK_bar, USBHID_KEY_BACKSLASH},
        {XK_colon, USBHID_KEY_SEMICOLON},
        {XK_semicolon, USBHID_KEY_SEMICOLON},
        {XK_quotedbl, USBHID_KEY_APOSTROPHE},
        {XK_apostrophe, USBHID_KEY_APOSTROPHE},
        {XK_grave, USBHID_KEY_GRAVE},
        {XK_asciitilde, USBHID_KEY_GRAVE},
        {XK_comma, USBHID_KEY_COMMA},
        {XK_less, USBHID_KEY_COMMA},
        {XK_period, USBHID_KEY_DOT},
        {XK_greater, USBHID_KEY_DOT},
        {XK_slash, USBHID_KEY_SLASH},
        {XK_question, USBHID_KEY_SLASH},
        {XK_Caps_Lock, USBHID_KEY_CAPSLOCK},
        {XK_Print, USBHID_KEY_PRINT},
        {XK_Scroll_Lock, USBHID_KEY_SCROLLLOCK},
        {XK_Pause, USBHID_KEY_PAUSE},
        {XK_Insert, USBHID_KEY_INSERT},
        {XK_KP_Insert, USBHID_KEY_INSERT},
        {XK_Home, USBHID_KEY_HOME},
        {XK_KP_Home, USBHID_KEY_HOME},
        {XK_Page_Up, USBHID_KEY_PAGEUP},
        {XK_KP_Page_Up, USBHID_KEY_PAGEUP},
        {XK_Delete, USBHID_KEY_DELETE},
        {XK_KP_Delete, USBHID_KEY_DELETE},
        {XK_End, USBHID_KEY_END},
        {XK_KP_End, USBHID_KEY_END},
        {XK_Page_Down, USBHID_KEY_PAGEDOWN},
        {XK_KP_Page_Down, USBHID_KEY_PAGEDOWN},
        {XK_Right, USBHID_KEY_RIGHT},
        {XK_KP_Right, USBHID_KEY_RIGHT},
        {XK_Left, USBHID_KEY_LEFT},
        {XK_KP_Left, USBHID_KEY_LEFT},
        {XK_Down, USBHID_KEY_DOWN},
        {XK_KP_Down, USBHID_KEY_DOWN},
        {XK_Up, USBHID_KEY_UP},
        {XK_KP_Up, USBHID_KEY_UP},
        {XK_Num_Lock, USBHID_KEY_NUMLOCK},
        {XK_KP_Enter, USBHID_KEY_KP_ENTER},
        {XK_KP_Equal, USBHID_KEY_KP_EQUAL},
        {XK_KP_Multiply, USBHID_KEY_KP_MULTIPLY},
        {XK_KP_Add, USBHID_KEY_KP_ADD},
        {XK_KP_Subtract, USBHID_KEY_KP_SUBTRACT},
        {XK_KP_Decimal, USBHID_KEY_KP_DECIMAL},
        {XK_KP_Divide, USBHID_KEY_KP_DIVIDE},
        {XK_KP_0, USBHID_KEY_KP_0},
        {XK_Intlbackslash, USBHID_KEY_INTLBACKSLASH},
        {XK_Menu, USBHID_MENU},
    };

    for (const auto& [key, expected] : cases)
    {
        SCOPED_TRACE(::testing::Message() << "key=0x" << std::hex << key);
        EXPECT_EQ(Input::testKeyToScancode(key), expected);
    }
}

TEST(InputKeyToScancodeTest, UnmappedKey_ReturnsZero)
{
    // XK_VoidSymbol is not handled by any branch — falls through to the
    // default-initialized scancode of 0.
    EXPECT_EQ(Input::testKeyToScancode(XK_VoidSymbol), 0);
}

// ---------------------------------------------------------------------------
// InputWriteKeyboardTest — exercises the private writeKeyboard() via the
//   #ifdef TEST public wrapper. A /dev/null fd makes the write() syscall
//   succeed deterministically (success branch); an invalid fd (-1) makes it
//   fail with errno != EAGAIN (failure branch) — no D-Bus/hardware needed.
// ---------------------------------------------------------------------------
TEST(InputWriteKeyboardTest, ValidFd_ReturnsTrue)
{
    Input inp{"", "", ""};
    int devnull = open("/dev/null", O_RDWR);
    ASSERT_GE(devnull, 0);
    inp.setTestKeyboardFd(devnull);

    uint8_t report[8] = {0};
    EXPECT_TRUE(inp.testWriteKeyboard(report));

    inp.setTestKeyboardFd(-1);
    close(devnull);
}

TEST(InputWriteKeyboardTest, InvalidFd_ReturnsFalse)
{
    Input inp{"", "", ""};
    inp.setTestKeyboardFd(-1);

    uint8_t report[8] = {0};
    EXPECT_FALSE(inp.testWriteKeyboard(report));
}

// ---------------------------------------------------------------------------
// InputWritePointerTest — same rationale as InputWriteKeyboardTest above.
// ---------------------------------------------------------------------------
TEST(InputWritePointerTest, ValidFd_NoThrow)
{
    Input inp{"", "", ""};
    int devnull = open("/dev/null", O_RDWR);
    ASSERT_GE(devnull, 0);
    inp.setTestPointerFd(devnull);

    uint8_t report[6] = {0};
    EXPECT_NO_THROW(inp.testWritePointer(report));

    inp.setTestPointerFd(-1);
    close(devnull);
}

TEST(InputWritePointerTest, InvalidFd_NoThrow)
{
    Input inp{"", "", ""};
    inp.setTestPointerFd(-1);

    uint8_t report[6] = {0};
    EXPECT_NO_THROW(inp.testWritePointer(report));
}

} // namespace ikvm
