/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/TextEvents.h"

#include "nsWindow.h"
#include "nsQtKeyUtils.h"

using namespace mozilla;
using namespace mozilla::widget;

struct nsKeyConverter
{
    int vkCode; // Platform independent key code
    int keysym; // Qt key code
};

static struct nsKeyConverter nsKeycodes[] =
{
//  { NS_VK_CANCEL,        Qt::Key_Cancel },
    { NS_VK_BACK,          Qt::Key_Backspace },
    { NS_VK_TAB,           Qt::Key_Tab },
    { NS_VK_TAB,           Qt::Key_Backtab },
//  { NS_VK_CLEAR,         Qt::Key_Clear },
    { NS_VK_RETURN,        Qt::Key_Return },
    { NS_VK_RETURN,        Qt::Key_Enter },
    { NS_VK_SHIFT,         Qt::Key_Shift },
    { NS_VK_CONTROL,       Qt::Key_Control },
    { NS_VK_ALT,           Qt::Key_Alt },
    { NS_VK_PAUSE,         Qt::Key_Pause },
    { NS_VK_CAPS_LOCK,     Qt::Key_CapsLock },
    { NS_VK_ESCAPE,        Qt::Key_Escape },
    { NS_VK_SPACE,         Qt::Key_Space },
    { NS_VK_PAGE_UP,       Qt::Key_PageUp },
    { NS_VK_PAGE_DOWN,     Qt::Key_PageDown },
    { NS_VK_END,           Qt::Key_End },
    { NS_VK_HOME,          Qt::Key_Home },
    { NS_VK_LEFT,          Qt::Key_Left },
    { NS_VK_UP,            Qt::Key_Up },
    { NS_VK_RIGHT,         Qt::Key_Right },
    { NS_VK_DOWN,          Qt::Key_Down },
    { NS_VK_PRINTSCREEN,   Qt::Key_Print },
    { NS_VK_INSERT,        Qt::Key_Insert },
    { NS_VK_DELETE,        Qt::Key_Delete },
    { NS_VK_HELP,          Qt::Key_Help },

    { NS_VK_0,             Qt::Key_0 },
    { NS_VK_1,             Qt::Key_1 },
    { NS_VK_2,             Qt::Key_2 },
    { NS_VK_3,             Qt::Key_3 },
    { NS_VK_4,             Qt::Key_4 },
    { NS_VK_5,             Qt::Key_5 },
    { NS_VK_6,             Qt::Key_6 },
    { NS_VK_7,             Qt::Key_7 },
    { NS_VK_8,             Qt::Key_8 },
    { NS_VK_9,             Qt::Key_9 },

    { NS_VK_SEMICOLON,     Qt::Key_Semicolon },
    { NS_VK_EQUALS,        Qt::Key_Equal },

    { NS_VK_A,             Qt::Key_A },
    { NS_VK_B,             Qt::Key_B },
    { NS_VK_C,             Qt::Key_C },
    { NS_VK_D,             Qt::Key_D },
    { NS_VK_E,             Qt::Key_E },
    { NS_VK_F,             Qt::Key_F },
    { NS_VK_G,             Qt::Key_G },
    { NS_VK_H,             Qt::Key_H },
    { NS_VK_I,             Qt::Key_I },
    { NS_VK_J,             Qt::Key_J },
    { NS_VK_K,             Qt::Key_K },
    { NS_VK_L,             Qt::Key_L },
    { NS_VK_M,             Qt::Key_M },
    { NS_VK_N,             Qt::Key_N },
    { NS_VK_O,             Qt::Key_O },
    { NS_VK_P,             Qt::Key_P },
    { NS_VK_Q,             Qt::Key_Q },
    { NS_VK_R,             Qt::Key_R },
    { NS_VK_S,             Qt::Key_S },
    { NS_VK_T,             Qt::Key_T },
    { NS_VK_U,             Qt::Key_U },
    { NS_VK_V,             Qt::Key_V },
    { NS_VK_W,             Qt::Key_W },
    { NS_VK_X,             Qt::Key_X },
    { NS_VK_Y,             Qt::Key_Y },
    { NS_VK_Z,             Qt::Key_Z },

    { NS_VK_NUMPAD0,       Qt::Key_0 },
    { NS_VK_NUMPAD1,       Qt::Key_1 },
    { NS_VK_NUMPAD2,       Qt::Key_2 },
    { NS_VK_NUMPAD3,       Qt::Key_3 },
    { NS_VK_NUMPAD4,       Qt::Key_4 },
    { NS_VK_NUMPAD5,       Qt::Key_5 },
    { NS_VK_NUMPAD6,       Qt::Key_6 },
    { NS_VK_NUMPAD7,       Qt::Key_7 },
    { NS_VK_NUMPAD8,       Qt::Key_8 },
    { NS_VK_NUMPAD9,       Qt::Key_9 },
    { NS_VK_MULTIPLY,      Qt::Key_Asterisk },
    { NS_VK_ADD,           Qt::Key_Plus },
//  { NS_VK_SEPARATOR,     Qt::Key_Separator },
    { NS_VK_SUBTRACT,      Qt::Key_Minus },
    { NS_VK_DECIMAL,       Qt::Key_Period },
    { NS_VK_DIVIDE,        Qt::Key_Slash },
    { NS_VK_F1,            Qt::Key_F1 },
    { NS_VK_F2,            Qt::Key_F2 },
    { NS_VK_F3,            Qt::Key_F3 },
    { NS_VK_F4,            Qt::Key_F4 },
    { NS_VK_F5,            Qt::Key_F5 },
    { NS_VK_F6,            Qt::Key_F6 },
    { NS_VK_F7,            Qt::Key_F7 },
    { NS_VK_F8,            Qt::Key_F8 },
    { NS_VK_F9,            Qt::Key_F9 },
    { NS_VK_F10,           Qt::Key_F10 },
    { NS_VK_F11,           Qt::Key_F11 },
    { NS_VK_F12,           Qt::Key_F12 },
    { NS_VK_F13,           Qt::Key_F13 },
    { NS_VK_F14,           Qt::Key_F14 },
    { NS_VK_F15,           Qt::Key_F15 },
    { NS_VK_F16,           Qt::Key_F16 },
    { NS_VK_F17,           Qt::Key_F17 },
    { NS_VK_F18,           Qt::Key_F18 },
    { NS_VK_F19,           Qt::Key_F19 },
    { NS_VK_F20,           Qt::Key_F20 },
    { NS_VK_F21,           Qt::Key_F21 },
    { NS_VK_F22,           Qt::Key_F22 },
    { NS_VK_F23,           Qt::Key_F23 },
    { NS_VK_F24,           Qt::Key_F24 },

    { NS_VK_NUM_LOCK,      Qt::Key_NumLock },
    { NS_VK_SCROLL_LOCK,   Qt::Key_ScrollLock },

    { NS_VK_COMMA,         Qt::Key_Comma },
    { NS_VK_PERIOD,        Qt::Key_Period },
    { NS_VK_SLASH,         Qt::Key_Slash },
    { NS_VK_BACK_QUOTE,    Qt::Key_QuoteLeft },
    { NS_VK_OPEN_BRACKET,  Qt::Key_ParenLeft },
    { NS_VK_CLOSE_BRACKET, Qt::Key_ParenRight },
    { NS_VK_QUOTE,         Qt::Key_QuoteDbl },

    { NS_VK_META,          Qt::Key_Meta }
};

int
QtKeyCodeToDOMKeyCode(int aKeysym)
{
    unsigned int i;

    // First, try to handle alphanumeric input, not listed in nsKeycodes:
    // most likely, more letters will be getting typed in than things in
    // the key list, so we will look through these first.

    // since X has different key symbols for upper and lowercase letters and
    // mozilla does not, convert gdk's to mozilla's
    if (aKeysym >= Qt::Key_A && aKeysym <= Qt::Key_Z)
        return aKeysym - Qt::Key_A + NS_VK_A;

    // numbers
    if (aKeysym >= Qt::Key_0 && aKeysym <= Qt::Key_9)
        return aKeysym - Qt::Key_0 + NS_VK_0;

    // keypad numbers
//    if (aKeysym >= Qt::Key_KP_0 && aKeysym <= Qt::Key_KP_9)
//        return aKeysym - Qt::Key_KP_0 + NS_VK_NUMPAD0;

    // misc other things
    for (i = 0; i < ArrayLength(nsKeycodes); i++) {
        if (nsKeycodes[i].keysym == aKeysym)
            return(nsKeycodes[i].vkCode);
    }

    // function keys
    if (aKeysym >= Qt::Key_F1 && aKeysym <= Qt::Key_F24)
        return aKeysym - Qt::Key_F1 + NS_VK_F1;

    return((int)0);
}

int
DOMKeyCodeToQtKeyCode(int aKeysym)
{
    unsigned int i;

    // First, try to handle alphanumeric input, not listed in nsKeycodes:
    // most likely, more letters will be getting typed in than things in
    // the key list, so we will look through these first.

    if (aKeysym >= NS_VK_A && aKeysym <= NS_VK_Z)
      // gdk and DOM both use the ASCII codes for these keys.
      return aKeysym;

    // numbers
    if (aKeysym >= NS_VK_0 && aKeysym <= NS_VK_9)
      // gdk and DOM both use the ASCII codes for these keys.
      return aKeysym - Qt::Key_0 + NS_VK_0;

    // keypad numbers
    if (aKeysym >= NS_VK_NUMPAD0 && aKeysym <= NS_VK_NUMPAD9) {
      NS_ERROR("keypad numbers conversion not implemented");
      //return aKeysym - NS_VK_NUMPAD0 + Qt::Key_KP_0;
      return 0;
    }

    // misc other things
    for (i = 0; i < ArrayLength(nsKeycodes); ++i) {
      if (nsKeycodes[i].vkCode == aKeysym) {
        return nsKeycodes[i].keysym;
      }
    }

    // function keys
    if (aKeysym >= NS_VK_F1 && aKeysym <= NS_VK_F9)
      return aKeysym - NS_VK_F1 + Qt::Key_F1;

    return 0;
}

KeyNameIndex
QtKeyCodeToDOMKeyNameIndex(int aKeysym)
{
    switch (aKeysym) {

#define NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, aKeyNameIndex) \
        case aNativeKey: return aKeyNameIndex;

#include "NativeKeyToDOMKeyName.h"

#undef NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX

        case Qt::Key_Exclam:
        case Qt::Key_QuoteDbl:
        case Qt::Key_NumberSign:
        case Qt::Key_Dollar:
        case Qt::Key_Percent:
        case Qt::Key_Ampersand:
        case Qt::Key_Apostrophe:
        case Qt::Key_ParenLeft:
        case Qt::Key_ParenRight:
        case Qt::Key_Asterisk:
        case Qt::Key_Plus:
        case Qt::Key_Comma:
        case Qt::Key_Minus:
        case Qt::Key_Period:
        case Qt::Key_Slash:
        case Qt::Key_0:
        case Qt::Key_1:
        case Qt::Key_2:
        case Qt::Key_3:
        case Qt::Key_4:
        case Qt::Key_5:
        case Qt::Key_6:
        case Qt::Key_7:
        case Qt::Key_8:
        case Qt::Key_9:
        case Qt::Key_Colon:
        case Qt::Key_Semicolon:
        case Qt::Key_Less:
        case Qt::Key_Equal:
        case Qt::Key_Greater:
        case Qt::Key_Question:
        case Qt::Key_At:
        case Qt::Key_A:
        case Qt::Key_B:
        case Qt::Key_C:
        case Qt::Key_D:
        case Qt::Key_E:
        case Qt::Key_F:
        case Qt::Key_G:
        case Qt::Key_H:
        case Qt::Key_I:
        case Qt::Key_J:
        case Qt::Key_K:
        case Qt::Key_L:
        case Qt::Key_M:
        case Qt::Key_N:
        case Qt::Key_O:
        case Qt::Key_P:
        case Qt::Key_Q:
        case Qt::Key_R:
        case Qt::Key_S:
        case Qt::Key_T:
        case Qt::Key_U:
        case Qt::Key_V:
        case Qt::Key_W:
        case Qt::Key_X:
        case Qt::Key_Y:
        case Qt::Key_Z:
        case Qt::Key_BracketLeft:
        case Qt::Key_Backslash:
        case Qt::Key_BracketRight:
        case Qt::Key_AsciiCircum:
        case Qt::Key_Underscore:
        case Qt::Key_QuoteLeft:
        case Qt::Key_BraceLeft:
        case Qt::Key_Bar:
        case Qt::Key_BraceRight:
        case Qt::Key_AsciiTilde:
        case Qt::Key_exclamdown:
        case Qt::Key_cent:
        case Qt::Key_sterling:
        case Qt::Key_currency:
        case Qt::Key_yen:
        case Qt::Key_brokenbar:
        case Qt::Key_section:
        case Qt::Key_diaeresis:
        case Qt::Key_copyright:
        case Qt::Key_ordfeminine:
        case Qt::Key_guillemotleft:
        case Qt::Key_notsign:
        case Qt::Key_hyphen:
        case Qt::Key_registered:
        case Qt::Key_macron:
        case Qt::Key_degree:
        case Qt::Key_plusminus:
        case Qt::Key_twosuperior:
        case Qt::Key_threesuperior:
        case Qt::Key_acute:
        case Qt::Key_mu:
        case Qt::Key_paragraph:
        case Qt::Key_periodcentered:
        case Qt::Key_cedilla:
        case Qt::Key_onesuperior:
        case Qt::Key_masculine:
        case Qt::Key_guillemotright:
        case Qt::Key_onequarter:
        case Qt::Key_onehalf:
        case Qt::Key_threequarters:
        case Qt::Key_questiondown:
        case Qt::Key_Agrave:
        case Qt::Key_Aacute:
        case Qt::Key_Acircumflex:
        case Qt::Key_Atilde:
        case Qt::Key_Adiaeresis:
        case Qt::Key_Aring:
        case Qt::Key_AE:
        case Qt::Key_Ccedilla:
        case Qt::Key_Egrave:
        case Qt::Key_Eacute:
        case Qt::Key_Ecircumflex:
        case Qt::Key_Ediaeresis:
        case Qt::Key_Igrave:
        case Qt::Key_Iacute:
        case Qt::Key_Icircumflex:
        case Qt::Key_Idiaeresis:
        case Qt::Key_ETH:
        case Qt::Key_Ntilde:
        case Qt::Key_Ograve:
        case Qt::Key_Oacute:
        case Qt::Key_Ocircumflex:
        case Qt::Key_Otilde:
        case Qt::Key_Odiaeresis:
        case Qt::Key_multiply:
        case Qt::Key_Ooblique:
        case Qt::Key_Ugrave:
        case Qt::Key_Uacute:
        case Qt::Key_Ucircumflex:
        case Qt::Key_Udiaeresis:
        case Qt::Key_Yacute:
        case Qt::Key_THORN:
        case Qt::Key_ssharp:
        case Qt::Key_division:
        case Qt::Key_ydiaeresis:
            return KEY_NAME_INDEX_PrintableKey;

        case Qt::Key_Backtab:
        case Qt::Key_Direction_L:
        case Qt::Key_Direction_R:
        case Qt::Key_SingleCandidate:
        case Qt::Key_Hiragana_Katakana:
        case Qt::Key_Zenkaku_Hankaku:
        case Qt::Key_Touroku:
        case Qt::Key_Massyo:
        case Qt::Key_Hangul:
        case Qt::Key_Hangul_Start:
        case Qt::Key_Hangul_End:
        case Qt::Key_Hangul_Hanja:
        case Qt::Key_Hangul_Jamo:
        case Qt::Key_Hangul_Romaja:
        case Qt::Key_Hangul_Jeonja:
        case Qt::Key_Hangul_Banja:
        case Qt::Key_Hangul_PreHanja:
        case Qt::Key_Hangul_PostHanja:
        case Qt::Key_Hangul_Special:
        case Qt::Key_Dead_Belowdot:
        case Qt::Key_Dead_Hook:
        case Qt::Key_Dead_Horn:
        case Qt::Key_TrebleUp:
        case Qt::Key_TrebleDown:
        case Qt::Key_Standby:
        case Qt::Key_OpenUrl:
        case Qt::Key_LaunchMedia:
        case Qt::Key_KeyboardLightOnOff:
        case Qt::Key_KeyboardBrightnessUp:
        case Qt::Key_KeyboardBrightnessDown:
        case Qt::Key_WakeUp:
        case Qt::Key_ScreenSaver:
        case Qt::Key_WWW:
        case Qt::Key_Memo:
        case Qt::Key_LightBulb:
        case Qt::Key_Shop:
        case Qt::Key_History:
        case Qt::Key_AddFavorite:
        case Qt::Key_HotLinks:
        case Qt::Key_Finance:
        case Qt::Key_Community:
        case Qt::Key_BackForward:
        case Qt::Key_ApplicationLeft:
        case Qt::Key_ApplicationRight:
        case Qt::Key_Book:
        case Qt::Key_CD:
        case Qt::Key_Calculator:
        case Qt::Key_ToDoList:
        case Qt::Key_ClearGrab:
        case Qt::Key_Close:
        case Qt::Key_Display:
        case Qt::Key_DOS:
        case Qt::Key_Documents:
        case Qt::Key_Excel:
        case Qt::Key_Explorer:
        case Qt::Key_Game:
        case Qt::Key_Go:
        case Qt::Key_iTouch:
        case Qt::Key_LogOff:
        case Qt::Key_Market:
        case Qt::Key_Meeting:
        case Qt::Key_MenuKB:
        case Qt::Key_MenuPB:
        case Qt::Key_MySites:
        case Qt::Key_News:
        case Qt::Key_OfficeHome:
        case Qt::Key_Option:
        case Qt::Key_Phone:
        case Qt::Key_Calendar:
        case Qt::Key_Reply:
        case Qt::Key_RotateWindows:
        case Qt::Key_RotationPB:
        case Qt::Key_RotationKB:
        case Qt::Key_Save:
        case Qt::Key_Send:
        case Qt::Key_Spell:
        case Qt::Key_SplitScreen:
        case Qt::Key_Support:
        case Qt::Key_TaskPane:
        case Qt::Key_Terminal:
        case Qt::Key_Tools:
        case Qt::Key_Travel:
        case Qt::Key_Video:
        case Qt::Key_Word:
        case Qt::Key_Xfer:
        case Qt::Key_ZoomIn:
        case Qt::Key_ZoomOut:
        case Qt::Key_Away:
        case Qt::Key_Messenger:
        case Qt::Key_WebCam:
        case Qt::Key_MailForward:
        case Qt::Key_Pictures:
        case Qt::Key_Music:
        case Qt::Key_Battery:
        case Qt::Key_Bluetooth:
        case Qt::Key_WLAN:
        case Qt::Key_UWB:
        case Qt::Key_AudioRepeat:
        case Qt::Key_AudioCycleTrack:
        case Qt::Key_Time:
        case Qt::Key_Hibernate:
        case Qt::Key_View:
        case Qt::Key_TopMenu:
        case Qt::Key_PowerDown:
        case Qt::Key_Suspend:
        case Qt::Key_ContrastAdjust:

#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
        case Qt::Key_TouchpadToggle:
        case Qt::Key_TouchpadOn:
        case Qt::Key_TouchpadOff:
#endif

        case Qt::Key_unknown:
        case Qt::Key_Call:
        case Qt::Key_CameraFocus:
        case Qt::Key_Context1:
        case Qt::Key_Context2:
        case Qt::Key_Context3:
        case Qt::Key_Context4:
        case Qt::Key_Flip:
        case Qt::Key_Hangup:
        case Qt::Key_No:
        case Qt::Key_Select:
        case Qt::Key_Yes:
        case Qt::Key_ToggleCallHangup:
        case Qt::Key_VoiceDial:
        case Qt::Key_LastNumberRedial:
        case Qt::Key_Printer:
        case Qt::Key_Sleep:
        default:
            return KEY_NAME_INDEX_Unidentified;
    }
}

