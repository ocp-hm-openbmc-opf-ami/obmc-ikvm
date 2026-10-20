// SPDX-License-Identifier: Apache-2.0

#include "ami/include/keysym_ami.hpp"
#include "ikvm_input.hpp"
#include "scancodes.hpp"

#include <rfb/keysym.h>

#include <cstdint>
#include <utility>

#include <gtest/gtest.h>

using Case = std::pair<rfbKeySym, uint8_t>;

class KeyToScancodeParam : public ::testing::TestWithParam<Case>
{};

TEST_P(KeyToScancodeParam, KeyToScancode_ValidKey_MapsCorrectly)
{
    const auto [key, expected] = GetParam();
    EXPECT_EQ(ikvm::Input::testKeyToScancode(key), expected);
}

INSTANTIATE_TEST_SUITE_P(
    Alphabet, KeyToScancodeParam,
    ::testing::Values(
        Case{'A', USBHID_KEY_A}, Case{'a', USBHID_KEY_A},
        Case{'B', USBHID_KEY_B}, Case{'b', USBHID_KEY_B},
        Case{'C', USBHID_KEY_C}, Case{'c', USBHID_KEY_C},
        Case{'D', USBHID_KEY_D}, Case{'d', USBHID_KEY_D},
        Case{'E', USBHID_KEY_E}, Case{'e', USBHID_KEY_E},
        Case{'F', USBHID_KEY_F}, Case{'f', USBHID_KEY_F},
        Case{'G', USBHID_KEY_G}, Case{'g', USBHID_KEY_G},
        Case{'H', USBHID_KEY_H}, Case{'h', USBHID_KEY_H},
        Case{'I', USBHID_KEY_I}, Case{'i', USBHID_KEY_I},
        Case{'J', USBHID_KEY_J}, Case{'j', USBHID_KEY_J},
        Case{'K', USBHID_KEY_K}, Case{'k', USBHID_KEY_K},
        Case{'L', USBHID_KEY_L}, Case{'l', USBHID_KEY_L},
        Case{'M', USBHID_KEY_M}, Case{'m', USBHID_KEY_M},
        Case{'N', USBHID_KEY_N}, Case{'n', USBHID_KEY_N},
        Case{'O', USBHID_KEY_O}, Case{'o', USBHID_KEY_O},
        Case{'P', USBHID_KEY_P}, Case{'p', USBHID_KEY_P},
        Case{'Q', USBHID_KEY_Q}, Case{'q', USBHID_KEY_Q},
        Case{'R', USBHID_KEY_R}, Case{'r', USBHID_KEY_R},
        Case{'S', USBHID_KEY_S}, Case{'s', USBHID_KEY_S},
        Case{'T', USBHID_KEY_T}, Case{'t', USBHID_KEY_T},
        Case{'U', USBHID_KEY_U}, Case{'u', USBHID_KEY_U},
        Case{'V', USBHID_KEY_V}, Case{'v', USBHID_KEY_V},
        Case{'W', USBHID_KEY_W}, Case{'w', USBHID_KEY_W},
        Case{'X', USBHID_KEY_X}, Case{'x', USBHID_KEY_X},
        Case{'Y', USBHID_KEY_Y}, Case{'y', USBHID_KEY_Y},
        Case{'Z', USBHID_KEY_Z}, Case{'z', USBHID_KEY_Z}));

INSTANTIATE_TEST_SUITE_P(
    Digits, KeyToScancodeParam,
    ::testing::Values(Case{'1', USBHID_KEY_1}, Case{'2', USBHID_KEY_2},
                      Case{'3', USBHID_KEY_3}, Case{'4', USBHID_KEY_4},
                      Case{'5', USBHID_KEY_5}, Case{'6', USBHID_KEY_6},
                      Case{'7', USBHID_KEY_7}, Case{'8', USBHID_KEY_8},
                      Case{'9', USBHID_KEY_9}));

TEST(DigitsSpecial, KeyToScancode_ZeroKey_MapsToUsbhidKey0)
{
    EXPECT_EQ(ikvm::Input::testKeyToScancode(XK_0), USBHID_KEY_0);
}

INSTANTIATE_TEST_SUITE_P(
    ShiftedDigits, KeyToScancodeParam,
    ::testing::Values(
        Case{XK_exclam, USBHID_KEY_1}, Case{XK_at, USBHID_KEY_2},
        Case{XK_numbersign, USBHID_KEY_3}, Case{XK_dollar, USBHID_KEY_4},
        Case{XK_percent, USBHID_KEY_5}, Case{XK_asciicircum, USBHID_KEY_6},
        Case{XK_ampersand, USBHID_KEY_7}, Case{XK_asterisk, USBHID_KEY_8},
        Case{XK_parenleft, USBHID_KEY_9}, Case{XK_parenright, USBHID_KEY_0}));

INSTANTIATE_TEST_SUITE_P(
    FunctionKeys, KeyToScancodeParam,
    ::testing::Values(Case{XK_F1, USBHID_KEY_F1}, Case{XK_F2, USBHID_KEY_F2},
                      Case{XK_F3, USBHID_KEY_F3}, Case{XK_F4, USBHID_KEY_F4},
                      Case{XK_F5, USBHID_KEY_F5}, Case{XK_F6, USBHID_KEY_F6},
                      Case{XK_F7, USBHID_KEY_F7}, Case{XK_F8, USBHID_KEY_F8},
                      Case{XK_F9, USBHID_KEY_F9}, Case{XK_F10, USBHID_KEY_F10},
                      Case{XK_F11, USBHID_KEY_F11},
                      Case{XK_F12, USBHID_KEY_F12}));

INSTANTIATE_TEST_SUITE_P(KeypadFKeys, KeyToScancodeParam,
                         ::testing::Values(Case{XK_KP_F1, USBHID_KEY_F1},
                                           Case{XK_KP_F2, USBHID_KEY_F2},
                                           Case{XK_KP_F3, USBHID_KEY_F3},
                                           Case{XK_KP_F4, USBHID_KEY_F4}));

INSTANTIATE_TEST_SUITE_P(
    KeypadDigits, KeyToScancodeParam,
    ::testing::Values(
        Case{XK_KP_1, USBHID_KEY_KP_1}, Case{XK_KP_2, USBHID_KEY_KP_2},
        Case{XK_KP_3, USBHID_KEY_KP_3}, Case{XK_KP_4, USBHID_KEY_KP_4},
        Case{XK_KP_5, USBHID_KEY_KP_5}, Case{XK_KP_6, USBHID_KEY_KP_6},
        Case{XK_KP_7, USBHID_KEY_KP_7}, Case{XK_KP_8, USBHID_KEY_KP_8},
        Case{XK_KP_9, USBHID_KEY_KP_9}, Case{XK_KP_0, USBHID_KEY_KP_0}));

INSTANTIATE_TEST_SUITE_P(
    KeypadOps, KeyToScancodeParam,
    ::testing::Values(Case{XK_KP_Enter, USBHID_KEY_KP_ENTER},
                      Case{XK_KP_Equal, USBHID_KEY_KP_EQUAL},
                      Case{XK_KP_Multiply, USBHID_KEY_KP_MULTIPLY},
                      Case{XK_KP_Add, USBHID_KEY_KP_ADD},
                      Case{XK_KP_Subtract, USBHID_KEY_KP_SUBTRACT},
                      Case{XK_KP_Decimal, USBHID_KEY_KP_DECIMAL},
                      Case{XK_KP_Divide, USBHID_KEY_KP_DIVIDE}));

INSTANTIATE_TEST_SUITE_P(
    Punctuation, KeyToScancodeParam,
    ::testing::Values(
        Case{XK_minus, USBHID_KEY_MINUS}, Case{XK_underscore, USBHID_KEY_MINUS},
        Case{XK_plus, USBHID_KEY_EQUAL}, Case{XK_equal, USBHID_KEY_EQUAL},
        Case{XK_bracketleft, USBHID_KEY_LEFTBRACE},
        Case{XK_braceleft, USBHID_KEY_LEFTBRACE},
        Case{XK_bracketright, USBHID_KEY_RIGHTBRACE},
        Case{XK_braceright, USBHID_KEY_RIGHTBRACE},
        Case{XK_backslash, USBHID_KEY_BACKSLASH},
        Case{XK_bar, USBHID_KEY_BACKSLASH},
        Case{XK_colon, USBHID_KEY_SEMICOLON},
        Case{XK_semicolon, USBHID_KEY_SEMICOLON},
        Case{XK_quotedbl, USBHID_KEY_APOSTROPHE},
        Case{XK_apostrophe, USBHID_KEY_APOSTROPHE},
        Case{XK_grave, USBHID_KEY_GRAVE}, Case{XK_asciitilde, USBHID_KEY_GRAVE},
        Case{XK_comma, USBHID_KEY_COMMA}, Case{XK_less, USBHID_KEY_COMMA},
        Case{XK_period, USBHID_KEY_DOT}, Case{XK_greater, USBHID_KEY_DOT},
        Case{XK_slash, USBHID_KEY_SLASH}, Case{XK_question, USBHID_KEY_SLASH},
        Case{XK_space, USBHID_KEY_SPACE}, Case{XK_KP_Space, USBHID_KEY_SPACE},
        Case{XK_Tab, USBHID_KEY_TAB}, Case{XK_KP_Tab, USBHID_KEY_TAB},
        Case{XK_Return, USBHID_KEY_RETURN}, Case{XK_Escape, USBHID_KEY_ESC},
        Case{XK_BackSpace, USBHID_KEY_BACKSPACE}));

INSTANTIATE_TEST_SUITE_P(
    NavigationAndLocks, KeyToScancodeParam,
    ::testing::Values(
        Case{XK_Insert, USBHID_KEY_INSERT},
        Case{XK_KP_Insert, USBHID_KEY_INSERT}, Case{XK_Home, USBHID_KEY_HOME},
        Case{XK_KP_Home, USBHID_KEY_HOME}, Case{XK_Page_Up, USBHID_KEY_PAGEUP},
        Case{XK_KP_Page_Up, USBHID_KEY_PAGEUP},
        Case{XK_Delete, USBHID_KEY_DELETE},
        Case{XK_KP_Delete, USBHID_KEY_DELETE}, Case{XK_End, USBHID_KEY_END},
        Case{XK_KP_End, USBHID_KEY_END},
        Case{XK_Page_Down, USBHID_KEY_PAGEDOWN},
        Case{XK_KP_Page_Down, USBHID_KEY_PAGEDOWN},
        Case{XK_Right, USBHID_KEY_RIGHT}, Case{XK_KP_Right, USBHID_KEY_RIGHT},
        Case{XK_Left, USBHID_KEY_LEFT}, Case{XK_KP_Left, USBHID_KEY_LEFT},
        Case{XK_Down, USBHID_KEY_DOWN}, Case{XK_KP_Down, USBHID_KEY_DOWN},
        Case{XK_Up, USBHID_KEY_UP}, Case{XK_KP_Up, USBHID_KEY_UP},
        Case{XK_Caps_Lock, USBHID_KEY_CAPSLOCK},
        Case{XK_Num_Lock, USBHID_KEY_NUMLOCK}, Case{XK_Print, USBHID_KEY_PRINT},
        Case{XK_Scroll_Lock, USBHID_KEY_SCROLLLOCK},
        Case{XK_Pause, USBHID_KEY_PAUSE}));

INSTANTIATE_TEST_SUITE_P(
    LocaleSpecific, KeyToScancodeParam,
    ::testing::Values(Case{XK_Intlbackslash, USBHID_KEY_INTLBACKSLASH},
                      Case{XK_Menu, USBHID_MENU}));

TEST(UnknownKeys, KeyToScancode_UnmappedKey_ReturnsZero)
{
    EXPECT_EQ(ikvm::Input::testKeyToScancode(XK_F13), 0);
    EXPECT_EQ(
        ikvm::Input::testKeyToScancode(static_cast<rfbKeySym>(0x7fffffff)), 0);
}
