/* -*- Mode: c++; c-basic-offset: 2; tab-width: 4; indent-tabs-mode: nil; -*-
 * vim: set sw=2 ts=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "GeckoEditableSupport.h"

#include "AndroidRect.h"
#include "KeyEvent.h"
#include "PuppetWidget.h"
#include "nsIContent.h"

#include "mozilla/dom/ContentChild.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/Preferences.h"
#include "mozilla/TextComposition.h"
#include "mozilla/TextEventDispatcherListener.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/BrowserChild.h"

#include <android/api-level.h>
#include <android/input.h>
#include <android/log.h>

#ifdef DEBUG_ANDROID_IME
#  define ALOGIME(args...) \
    __android_log_print(ANDROID_LOG_INFO, "GeckoEditableSupport", ##args)
#else
#  define ALOGIME(args...) \
    do {                   \
    } while (0)
#endif

template <>
const char nsWindow::NativePtr<mozilla::widget::GeckoEditableSupport>::sName[] =
    "GeckoEditableSupport";

enum {
  AKEYCODE_dummy,  // Avoid enum without declarations.

// These keycode masks are not defined in android/keycodes.h:
// Note that the NDK unified headers always define these constants, so we
// need to ensure we're _not_ using unified headers, by checking for the
// absence of __ANDROID_API_X__ macros (e.g. __ANDROID_API_L__), which are
// only defined by the unified headers.
#if __ANDROID_API__ < 13 && !defined(__ANDROID_API_I__)
  AKEYCODE_ESCAPE = 111,
  AKEYCODE_FORWARD_DEL = 112,
  AKEYCODE_CTRL_LEFT = 113,
  AKEYCODE_CTRL_RIGHT = 114,
  AKEYCODE_CAPS_LOCK = 115,
  AKEYCODE_SCROLL_LOCK = 116,
  AKEYCODE_META_LEFT = 117,
  AKEYCODE_META_RIGHT = 118,
  AKEYCODE_FUNCTION = 119,
  AKEYCODE_SYSRQ = 120,
  AKEYCODE_BREAK = 121,
  AKEYCODE_MOVE_HOME = 122,
  AKEYCODE_MOVE_END = 123,
  AKEYCODE_INSERT = 124,
  AKEYCODE_FORWARD = 125,
  AKEYCODE_MEDIA_PLAY = 126,
  AKEYCODE_MEDIA_PAUSE = 127,
  AKEYCODE_MEDIA_CLOSE = 128,
  AKEYCODE_MEDIA_EJECT = 129,
  AKEYCODE_MEDIA_RECORD = 130,
  AKEYCODE_F1 = 131,
  AKEYCODE_F2 = 132,
  AKEYCODE_F3 = 133,
  AKEYCODE_F4 = 134,
  AKEYCODE_F5 = 135,
  AKEYCODE_F6 = 136,
  AKEYCODE_F7 = 137,
  AKEYCODE_F8 = 138,
  AKEYCODE_F9 = 139,
  AKEYCODE_F10 = 140,
  AKEYCODE_F11 = 141,
  AKEYCODE_F12 = 142,
  AKEYCODE_NUM_LOCK = 143,
  AKEYCODE_NUMPAD_0 = 144,
  AKEYCODE_NUMPAD_1 = 145,
  AKEYCODE_NUMPAD_2 = 146,
  AKEYCODE_NUMPAD_3 = 147,
  AKEYCODE_NUMPAD_4 = 148,
  AKEYCODE_NUMPAD_5 = 149,
  AKEYCODE_NUMPAD_6 = 150,
  AKEYCODE_NUMPAD_7 = 151,
  AKEYCODE_NUMPAD_8 = 152,
  AKEYCODE_NUMPAD_9 = 153,
  AKEYCODE_NUMPAD_DIVIDE = 154,
  AKEYCODE_NUMPAD_MULTIPLY = 155,
  AKEYCODE_NUMPAD_SUBTRACT = 156,
  AKEYCODE_NUMPAD_ADD = 157,
  AKEYCODE_NUMPAD_DOT = 158,
  AKEYCODE_NUMPAD_COMMA = 159,
  AKEYCODE_NUMPAD_ENTER = 160,
  AKEYCODE_NUMPAD_EQUALS = 161,
  AKEYCODE_NUMPAD_LEFT_PAREN = 162,
  AKEYCODE_NUMPAD_RIGHT_PAREN = 163,
  AKEYCODE_VOLUME_MUTE = 164,
  AKEYCODE_INFO = 165,
  AKEYCODE_CHANNEL_UP = 166,
  AKEYCODE_CHANNEL_DOWN = 167,
  AKEYCODE_ZOOM_IN = 168,
  AKEYCODE_ZOOM_OUT = 169,
  AKEYCODE_TV = 170,
  AKEYCODE_WINDOW = 171,
  AKEYCODE_GUIDE = 172,
  AKEYCODE_DVR = 173,
  AKEYCODE_BOOKMARK = 174,
  AKEYCODE_CAPTIONS = 175,
  AKEYCODE_SETTINGS = 176,
  AKEYCODE_TV_POWER = 177,
  AKEYCODE_TV_INPUT = 178,
  AKEYCODE_STB_POWER = 179,
  AKEYCODE_STB_INPUT = 180,
  AKEYCODE_AVR_POWER = 181,
  AKEYCODE_AVR_INPUT = 182,
  AKEYCODE_PROG_RED = 183,
  AKEYCODE_PROG_GREEN = 184,
  AKEYCODE_PROG_YELLOW = 185,
  AKEYCODE_PROG_BLUE = 186,
  AKEYCODE_APP_SWITCH = 187,
  AKEYCODE_BUTTON_1 = 188,
  AKEYCODE_BUTTON_2 = 189,
  AKEYCODE_BUTTON_3 = 190,
  AKEYCODE_BUTTON_4 = 191,
  AKEYCODE_BUTTON_5 = 192,
  AKEYCODE_BUTTON_6 = 193,
  AKEYCODE_BUTTON_7 = 194,
  AKEYCODE_BUTTON_8 = 195,
  AKEYCODE_BUTTON_9 = 196,
  AKEYCODE_BUTTON_10 = 197,
  AKEYCODE_BUTTON_11 = 198,
  AKEYCODE_BUTTON_12 = 199,
  AKEYCODE_BUTTON_13 = 200,
  AKEYCODE_BUTTON_14 = 201,
  AKEYCODE_BUTTON_15 = 202,
  AKEYCODE_BUTTON_16 = 203,
#endif
#if __ANDROID_API__ < 14 && !defined(__ANDROID_API_I__)
  AKEYCODE_LANGUAGE_SWITCH = 204,
  AKEYCODE_MANNER_MODE = 205,
  AKEYCODE_3D_MODE = 206,
#endif
#if __ANDROID_API__ < 15 && !defined(__ANDROID_API_J__)
  AKEYCODE_CONTACTS = 207,
  AKEYCODE_CALENDAR = 208,
  AKEYCODE_MUSIC = 209,
  AKEYCODE_CALCULATOR = 210,
#endif
#if __ANDROID_API__ < 16 && !defined(__ANDROID_API_J__)
  AKEYCODE_ZENKAKU_HANKAKU = 211,
  AKEYCODE_EISU = 212,
  AKEYCODE_MUHENKAN = 213,
  AKEYCODE_HENKAN = 214,
  AKEYCODE_KATAKANA_HIRAGANA = 215,
  AKEYCODE_YEN = 216,
  AKEYCODE_RO = 217,
  AKEYCODE_KANA = 218,
  AKEYCODE_ASSIST = 219,
#endif
#if __ANDROID_API__ < 18 && !defined(__ANDROID_API_J_MR2__)
  AKEYCODE_BRIGHTNESS_DOWN = 220,
  AKEYCODE_BRIGHTNESS_UP = 221,
#endif
#if __ANDROID_API__ < 19 && !defined(__ANDROID_API_K__)
  AKEYCODE_MEDIA_AUDIO_TRACK = 222,
#endif
#if __ANDROID_API__ < 20 && !defined(__ANDROID_API_L__)
  AKEYCODE_SLEEP = 223,
  AKEYCODE_WAKEUP = 224,
#endif
#if __ANDROID_API__ < 21 && !defined(__ANDROID_API_L__)
  AKEYCODE_PAIRING = 225,
  AKEYCODE_MEDIA_TOP_MENU = 226,
  AKEYCODE_11 = 227,
  AKEYCODE_12 = 228,
  AKEYCODE_LAST_CHANNEL = 229,
  AKEYCODE_TV_DATA_SERVICE = 230,
  AKEYCODE_VOICE_ASSIST = 231,
  AKEYCODE_TV_RADIO_SERVICE = 232,
  AKEYCODE_TV_TELETEXT = 233,
  AKEYCODE_TV_NUMBER_ENTRY = 234,
  AKEYCODE_TV_TERRESTRIAL_ANALOG = 235,
  AKEYCODE_TV_TERRESTRIAL_DIGITAL = 236,
  AKEYCODE_TV_SATELLITE = 237,
  AKEYCODE_TV_SATELLITE_BS = 238,
  AKEYCODE_TV_SATELLITE_CS = 239,
  AKEYCODE_TV_SATELLITE_SERVICE = 240,
  AKEYCODE_TV_NETWORK = 241,
  AKEYCODE_TV_ANTENNA_CABLE = 242,
  AKEYCODE_TV_INPUT_HDMI_1 = 243,
  AKEYCODE_TV_INPUT_HDMI_2 = 244,
  AKEYCODE_TV_INPUT_HDMI_3 = 245,
  AKEYCODE_TV_INPUT_HDMI_4 = 246,
  AKEYCODE_TV_INPUT_COMPOSITE_1 = 247,
  AKEYCODE_TV_INPUT_COMPOSITE_2 = 248,
  AKEYCODE_TV_INPUT_COMPONENT_1 = 249,
  AKEYCODE_TV_INPUT_COMPONENT_2 = 250,
  AKEYCODE_TV_INPUT_VGA_1 = 251,
  AKEYCODE_TV_AUDIO_DESCRIPTION = 252,
  AKEYCODE_TV_AUDIO_DESCRIPTION_MIX_UP = 253,
  AKEYCODE_TV_AUDIO_DESCRIPTION_MIX_DOWN = 254,
  AKEYCODE_TV_ZOOM_MODE = 255,
  AKEYCODE_TV_CONTENTS_MENU = 256,
  AKEYCODE_TV_MEDIA_CONTEXT_MENU = 257,
  AKEYCODE_TV_TIMER_PROGRAMMING = 258,
  AKEYCODE_HELP = 259,
#endif
#if __ANDROID_API__ < 23 && !defined(__ANDROID_API_M__)
  AKEYCODE_NAVIGATE_PREVIOUS = 260,
  AKEYCODE_NAVIGATE_NEXT = 261,
  AKEYCODE_NAVIGATE_IN = 262,
  AKEYCODE_NAVIGATE_OUT = 263,
  AKEYCODE_MEDIA_SKIP_FORWARD = 272,
  AKEYCODE_MEDIA_SKIP_BACKWARD = 273,
  AKEYCODE_MEDIA_STEP_FORWARD = 274,
  AKEYCODE_MEDIA_STEP_BACKWARD = 275,
#endif
#if __ANDROID_API__ < 24 && !defined(__ANDROID_API_N__)
  AKEYCODE_STEM_PRIMARY = 264,
  AKEYCODE_STEM_1 = 265,
  AKEYCODE_STEM_2 = 266,
  AKEYCODE_STEM_3 = 267,
  AKEYCODE_DPAD_UP_LEFT = 268,
  AKEYCODE_DPAD_DOWN_LEFT = 269,
  AKEYCODE_DPAD_UP_RIGHT = 270,
  AKEYCODE_DPAD_DOWN_RIGHT = 271,
  AKEYCODE_SOFT_SLEEP = 276,
  AKEYCODE_CUT = 277,
  AKEYCODE_COPY = 278,
  AKEYCODE_PASTE = 279,
#endif
#if __ANDROID_API__ < 25 && !defined(__ANDROID_API_N_MR1__)
  AKEYCODE_SYSTEM_NAVIGATION_UP = 280,
  AKEYCODE_SYSTEM_NAVIGATION_DOWN = 281,
  AKEYCODE_SYSTEM_NAVIGATION_LEFT = 282,
  AKEYCODE_SYSTEM_NAVIGATION_RIGHT = 283,
#endif
};

static uint32_t ConvertAndroidKeyCodeToDOMKeyCode(int32_t androidKeyCode) {
  // Special-case alphanumeric keycodes because they are most common.
  if (androidKeyCode >= AKEYCODE_A && androidKeyCode <= AKEYCODE_Z) {
    return androidKeyCode - AKEYCODE_A + NS_VK_A;
  }

  if (androidKeyCode >= AKEYCODE_0 && androidKeyCode <= AKEYCODE_9) {
    return androidKeyCode - AKEYCODE_0 + NS_VK_0;
  }

  switch (androidKeyCode) {
    // KEYCODE_UNKNOWN (0) ... KEYCODE_HOME (3)
    case AKEYCODE_BACK:
      return NS_VK_ESCAPE;
    // KEYCODE_CALL (5) ... KEYCODE_POUND (18)
    case AKEYCODE_DPAD_UP:
      return NS_VK_UP;
    case AKEYCODE_DPAD_DOWN:
      return NS_VK_DOWN;
    case AKEYCODE_DPAD_LEFT:
      return NS_VK_LEFT;
    case AKEYCODE_DPAD_RIGHT:
      return NS_VK_RIGHT;
    case AKEYCODE_DPAD_CENTER:
      return NS_VK_RETURN;
    case AKEYCODE_VOLUME_UP:
      return NS_VK_VOLUME_UP;
    case AKEYCODE_VOLUME_DOWN:
      return NS_VK_VOLUME_DOWN;
    // KEYCODE_VOLUME_POWER (26) ... KEYCODE_Z (54)
    case AKEYCODE_COMMA:
      return NS_VK_COMMA;
    case AKEYCODE_PERIOD:
      return NS_VK_PERIOD;
    case AKEYCODE_ALT_LEFT:
      return NS_VK_ALT;
    case AKEYCODE_ALT_RIGHT:
      return NS_VK_ALT;
    case AKEYCODE_SHIFT_LEFT:
      return NS_VK_SHIFT;
    case AKEYCODE_SHIFT_RIGHT:
      return NS_VK_SHIFT;
    case AKEYCODE_TAB:
      return NS_VK_TAB;
    case AKEYCODE_SPACE:
      return NS_VK_SPACE;
    // KEYCODE_SYM (63) ... KEYCODE_ENVELOPE (65)
    case AKEYCODE_ENTER:
      return NS_VK_RETURN;
    case AKEYCODE_DEL:
      return NS_VK_BACK;  // Backspace
    case AKEYCODE_GRAVE:
      return NS_VK_BACK_QUOTE;
    // KEYCODE_MINUS (69)
    case AKEYCODE_EQUALS:
      return NS_VK_EQUALS;
    case AKEYCODE_LEFT_BRACKET:
      return NS_VK_OPEN_BRACKET;
    case AKEYCODE_RIGHT_BRACKET:
      return NS_VK_CLOSE_BRACKET;
    case AKEYCODE_BACKSLASH:
      return NS_VK_BACK_SLASH;
    case AKEYCODE_SEMICOLON:
      return NS_VK_SEMICOLON;
    // KEYCODE_APOSTROPHE (75)
    case AKEYCODE_SLASH:
      return NS_VK_SLASH;
    // KEYCODE_AT (77) ... KEYCODE_MEDIA_FAST_FORWARD (90)
    case AKEYCODE_MUTE:
      return NS_VK_VOLUME_MUTE;
    case AKEYCODE_PAGE_UP:
      return NS_VK_PAGE_UP;
    case AKEYCODE_PAGE_DOWN:
      return NS_VK_PAGE_DOWN;
    // KEYCODE_PICTSYMBOLS (94) ... KEYCODE_BUTTON_MODE (110)
    case AKEYCODE_ESCAPE:
      return NS_VK_ESCAPE;
    case AKEYCODE_FORWARD_DEL:
      return NS_VK_DELETE;
    case AKEYCODE_CTRL_LEFT:
      return NS_VK_CONTROL;
    case AKEYCODE_CTRL_RIGHT:
      return NS_VK_CONTROL;
    case AKEYCODE_CAPS_LOCK:
      return NS_VK_CAPS_LOCK;
    case AKEYCODE_SCROLL_LOCK:
      return NS_VK_SCROLL_LOCK;
    // KEYCODE_META_LEFT (117) ... KEYCODE_FUNCTION (119)
    case AKEYCODE_SYSRQ:
      return NS_VK_PRINTSCREEN;
    case AKEYCODE_BREAK:
      return NS_VK_PAUSE;
    case AKEYCODE_MOVE_HOME:
      return NS_VK_HOME;
    case AKEYCODE_MOVE_END:
      return NS_VK_END;
    case AKEYCODE_INSERT:
      return NS_VK_INSERT;
    // KEYCODE_FORWARD (125) ... KEYCODE_MEDIA_RECORD (130)
    case AKEYCODE_F1:
      return NS_VK_F1;
    case AKEYCODE_F2:
      return NS_VK_F2;
    case AKEYCODE_F3:
      return NS_VK_F3;
    case AKEYCODE_F4:
      return NS_VK_F4;
    case AKEYCODE_F5:
      return NS_VK_F5;
    case AKEYCODE_F6:
      return NS_VK_F6;
    case AKEYCODE_F7:
      return NS_VK_F7;
    case AKEYCODE_F8:
      return NS_VK_F8;
    case AKEYCODE_F9:
      return NS_VK_F9;
    case AKEYCODE_F10:
      return NS_VK_F10;
    case AKEYCODE_F11:
      return NS_VK_F11;
    case AKEYCODE_F12:
      return NS_VK_F12;
    case AKEYCODE_NUM_LOCK:
      return NS_VK_NUM_LOCK;
    case AKEYCODE_NUMPAD_0:
      return NS_VK_NUMPAD0;
    case AKEYCODE_NUMPAD_1:
      return NS_VK_NUMPAD1;
    case AKEYCODE_NUMPAD_2:
      return NS_VK_NUMPAD2;
    case AKEYCODE_NUMPAD_3:
      return NS_VK_NUMPAD3;
    case AKEYCODE_NUMPAD_4:
      return NS_VK_NUMPAD4;
    case AKEYCODE_NUMPAD_5:
      return NS_VK_NUMPAD5;
    case AKEYCODE_NUMPAD_6:
      return NS_VK_NUMPAD6;
    case AKEYCODE_NUMPAD_7:
      return NS_VK_NUMPAD7;
    case AKEYCODE_NUMPAD_8:
      return NS_VK_NUMPAD8;
    case AKEYCODE_NUMPAD_9:
      return NS_VK_NUMPAD9;
    case AKEYCODE_NUMPAD_DIVIDE:
      return NS_VK_DIVIDE;
    case AKEYCODE_NUMPAD_MULTIPLY:
      return NS_VK_MULTIPLY;
    case AKEYCODE_NUMPAD_SUBTRACT:
      return NS_VK_SUBTRACT;
    case AKEYCODE_NUMPAD_ADD:
      return NS_VK_ADD;
    case AKEYCODE_NUMPAD_DOT:
      return NS_VK_DECIMAL;
    case AKEYCODE_NUMPAD_COMMA:
      return NS_VK_SEPARATOR;
    case AKEYCODE_NUMPAD_ENTER:
      return NS_VK_RETURN;
    case AKEYCODE_NUMPAD_EQUALS:
      return NS_VK_EQUALS;
    // KEYCODE_NUMPAD_LEFT_PAREN (162) ... KEYCODE_CALCULATOR (210)

    // Needs to confirm the behavior.  If the key switches the open state
    // of Japanese IME (or switches input character between Hiragana and
    // Roman numeric characters), then, it might be better to use
    // NS_VK_KANJI which is used for Alt+Zenkaku/Hankaku key on Windows.
    case AKEYCODE_ZENKAKU_HANKAKU:
      return 0;
    case AKEYCODE_EISU:
      return NS_VK_EISU;
    case AKEYCODE_MUHENKAN:
      return NS_VK_NONCONVERT;
    case AKEYCODE_HENKAN:
      return NS_VK_CONVERT;
    case AKEYCODE_KATAKANA_HIRAGANA:
      return 0;
    case AKEYCODE_YEN:
      return NS_VK_BACK_SLASH;  // Same as other platforms.
    case AKEYCODE_RO:
      return NS_VK_BACK_SLASH;  // Same as other platforms.
    case AKEYCODE_KANA:
      return NS_VK_KANA;
    case AKEYCODE_ASSIST:
      return NS_VK_HELP;

    // the A key is the action key for gamepad devices.
    case AKEYCODE_BUTTON_A:
      return NS_VK_RETURN;

    default:
      ALOG(
          "ConvertAndroidKeyCodeToDOMKeyCode: "
          "No DOM keycode for Android keycode %d",
          int(androidKeyCode));
      return 0;
  }
}

static KeyNameIndex ConvertAndroidKeyCodeToKeyNameIndex(
    int32_t keyCode, int32_t action, int32_t domPrintableKeyValue) {
  // Special-case alphanumeric keycodes because they are most common.
  if (keyCode >= AKEYCODE_A && keyCode <= AKEYCODE_Z) {
    return KEY_NAME_INDEX_USE_STRING;
  }

  if (keyCode >= AKEYCODE_0 && keyCode <= AKEYCODE_9) {
    return KEY_NAME_INDEX_USE_STRING;
  }

  switch (keyCode) {
#define NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, aKeyNameIndex) \
  case aNativeKey:                                                     \
    return aKeyNameIndex;

#include "NativeKeyToDOMKeyName.h"

#undef NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX

    // KEYCODE_0 (7) ... KEYCODE_9 (16)
    case AKEYCODE_STAR:   // '*' key
    case AKEYCODE_POUND:  // '#' key

      // KEYCODE_A (29) ... KEYCODE_Z (54)

    case AKEYCODE_COMMA:   // ',' key
    case AKEYCODE_PERIOD:  // '.' key
    case AKEYCODE_SPACE:
    case AKEYCODE_GRAVE:          // '`' key
    case AKEYCODE_MINUS:          // '-' key
    case AKEYCODE_EQUALS:         // '=' key
    case AKEYCODE_LEFT_BRACKET:   // '[' key
    case AKEYCODE_RIGHT_BRACKET:  // ']' key
    case AKEYCODE_BACKSLASH:      // '\' key
    case AKEYCODE_SEMICOLON:      // ';' key
    case AKEYCODE_APOSTROPHE:     // ''' key
    case AKEYCODE_SLASH:          // '/' key
    case AKEYCODE_AT:             // '@' key
    case AKEYCODE_PLUS:           // '+' key

    case AKEYCODE_NUMPAD_0:
    case AKEYCODE_NUMPAD_1:
    case AKEYCODE_NUMPAD_2:
    case AKEYCODE_NUMPAD_3:
    case AKEYCODE_NUMPAD_4:
    case AKEYCODE_NUMPAD_5:
    case AKEYCODE_NUMPAD_6:
    case AKEYCODE_NUMPAD_7:
    case AKEYCODE_NUMPAD_8:
    case AKEYCODE_NUMPAD_9:
    case AKEYCODE_NUMPAD_DIVIDE:
    case AKEYCODE_NUMPAD_MULTIPLY:
    case AKEYCODE_NUMPAD_SUBTRACT:
    case AKEYCODE_NUMPAD_ADD:
    case AKEYCODE_NUMPAD_DOT:
    case AKEYCODE_NUMPAD_COMMA:
    case AKEYCODE_NUMPAD_EQUALS:
    case AKEYCODE_NUMPAD_LEFT_PAREN:
    case AKEYCODE_NUMPAD_RIGHT_PAREN:

    case AKEYCODE_YEN:  // yen sign key
    case AKEYCODE_RO:   // Japanese Ro key
      return KEY_NAME_INDEX_USE_STRING;

    case AKEYCODE_NUM:  // XXX Not sure
    case AKEYCODE_PICTSYMBOLS:

    case AKEYCODE_BUTTON_A:
    case AKEYCODE_BUTTON_B:
    case AKEYCODE_BUTTON_C:
    case AKEYCODE_BUTTON_X:
    case AKEYCODE_BUTTON_Y:
    case AKEYCODE_BUTTON_Z:
    case AKEYCODE_BUTTON_L1:
    case AKEYCODE_BUTTON_R1:
    case AKEYCODE_BUTTON_L2:
    case AKEYCODE_BUTTON_R2:
    case AKEYCODE_BUTTON_THUMBL:
    case AKEYCODE_BUTTON_THUMBR:
    case AKEYCODE_BUTTON_START:
    case AKEYCODE_BUTTON_SELECT:
    case AKEYCODE_BUTTON_MODE:

    case AKEYCODE_MEDIA_CLOSE:

    case AKEYCODE_BUTTON_1:
    case AKEYCODE_BUTTON_2:
    case AKEYCODE_BUTTON_3:
    case AKEYCODE_BUTTON_4:
    case AKEYCODE_BUTTON_5:
    case AKEYCODE_BUTTON_6:
    case AKEYCODE_BUTTON_7:
    case AKEYCODE_BUTTON_8:
    case AKEYCODE_BUTTON_9:
    case AKEYCODE_BUTTON_10:
    case AKEYCODE_BUTTON_11:
    case AKEYCODE_BUTTON_12:
    case AKEYCODE_BUTTON_13:
    case AKEYCODE_BUTTON_14:
    case AKEYCODE_BUTTON_15:
    case AKEYCODE_BUTTON_16:
      return KEY_NAME_INDEX_Unidentified;

    case AKEYCODE_UNKNOWN:
      MOZ_ASSERT(action != AKEY_EVENT_ACTION_MULTIPLE,
                 "Don't call this when action is AKEY_EVENT_ACTION_MULTIPLE!");
      // It's actually an unknown key if the action isn't ACTION_MULTIPLE.
      // However, it might cause text input.  So, let's check the value.
      return domPrintableKeyValue ? KEY_NAME_INDEX_USE_STRING
                                  : KEY_NAME_INDEX_Unidentified;

    default:
      ALOG(
          "ConvertAndroidKeyCodeToKeyNameIndex: "
          "No DOM key name index for Android keycode %d",
          keyCode);
      return KEY_NAME_INDEX_Unidentified;
  }
}

static CodeNameIndex ConvertAndroidScanCodeToCodeNameIndex(int32_t scanCode) {
  switch (scanCode) {
#define NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX(aNativeKey, aCodeNameIndex) \
  case aNativeKey:                                                       \
    return aCodeNameIndex;

#include "NativeKeyToDOMCodeName.h"

#undef NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX

    default:
      return CODE_NAME_INDEX_UNKNOWN;
  }
}

static void InitKeyEvent(WidgetKeyboardEvent& aEvent, int32_t aAction,
                         int32_t aKeyCode, int32_t aScanCode,
                         int32_t aMetaState, int64_t aTime,
                         int32_t aDomPrintableKeyValue, int32_t aRepeatCount,
                         int32_t aFlags) {
  const uint32_t domKeyCode = ConvertAndroidKeyCodeToDOMKeyCode(aKeyCode);

  aEvent.mModifiers = nsWindow::GetModifiers(aMetaState);
  aEvent.mKeyCode = domKeyCode;

  aEvent.mIsRepeat =
      (aEvent.mMessage == eKeyDown || aEvent.mMessage == eKeyPress) &&
      ((aFlags & sdk::KeyEvent::FLAG_LONG_PRESS) || aRepeatCount);

  aEvent.mKeyNameIndex = ConvertAndroidKeyCodeToKeyNameIndex(
      aKeyCode, aAction, aDomPrintableKeyValue);
  aEvent.mCodeNameIndex = ConvertAndroidScanCodeToCodeNameIndex(aScanCode);

  if (aEvent.mKeyNameIndex == KEY_NAME_INDEX_USE_STRING &&
      aDomPrintableKeyValue) {
    aEvent.mKeyValue = char16_t(aDomPrintableKeyValue);
  }

  aEvent.mLocation =
      WidgetKeyboardEvent::ComputeLocationFromCodeValue(aEvent.mCodeNameIndex);
  aEvent.mTime = aTime;
  aEvent.mTimeStamp = nsWindow::GetEventTimeStamp(aTime);
}

static nscolor ConvertAndroidColor(uint32_t aArgb) {
  return NS_RGBA((aArgb & 0x00ff0000) >> 16, (aArgb & 0x0000ff00) >> 8,
                 (aArgb & 0x000000ff), (aArgb & 0xff000000) >> 24);
}

static jni::ObjectArray::LocalRef ConvertRectArrayToJavaRectFArray(
    const nsTArray<LayoutDeviceIntRect>& aRects,
    const CSSToLayoutDeviceScale aScale) {
  const size_t length = aRects.Length();
  auto rects = jni::ObjectArray::New<sdk::RectF>(length);

  for (size_t i = 0; i < length; i++) {
    const LayoutDeviceIntRect& tmp = aRects[i];

    // Character bounds in CSS units.
    auto rect = sdk::RectF::New(tmp.x / aScale.scale, tmp.y / aScale.scale,
                                (tmp.x + tmp.width) / aScale.scale,
                                (tmp.y + tmp.height) / aScale.scale);
    rects->SetElement(i, rect);
  }
  return rects;
}

namespace mozilla {
namespace widget {

bool GeckoEditableSupport::sDispatchKeyEventsInCompositionForAnyApps = false;

NS_IMPL_ISUPPORTS(GeckoEditableSupport, TextEventDispatcherListener,
                  nsISupportsWeakReference)

void GeckoEditableSupport::ObservePrefs() {
  static bool sIsObservingPref = false;
  if (sIsObservingPref) {
    return;
  }
  sIsObservingPref = true;
  Preferences::AddBoolVarCache(
      &sDispatchKeyEventsInCompositionForAnyApps,
      "intl.ime.hack.on_any_apps.fire_key_events_for_composition", false);
}

RefPtr<TextComposition> GeckoEditableSupport::GetComposition() const {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  return widget ? IMEStateManager::GetTextCompositionFor(widget) : nullptr;
}

bool GeckoEditableSupport::RemoveComposition(RemoveCompositionFlag aFlag) {
  if (!mDispatcher || !mDispatcher->IsComposing()) {
    return false;
  }

  nsEventStatus status = nsEventStatus_eIgnore;

  NS_ENSURE_SUCCESS(BeginInputTransaction(mDispatcher), false);
  mDispatcher->CommitComposition(
      status, aFlag == CANCEL_IME_COMPOSITION ? &EmptyString() : nullptr);
  return true;
}

void GeckoEditableSupport::OnKeyEvent(int32_t aAction, int32_t aKeyCode,
                                      int32_t aScanCode, int32_t aMetaState,
                                      int32_t aKeyPressMetaState, int64_t aTime,
                                      int32_t aDomPrintableKeyValue,
                                      int32_t aRepeatCount, int32_t aFlags,
                                      bool aIsSynthesizedImeKey,
                                      jni::Object::Param aOriginalEvent) {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  RefPtr<TextEventDispatcher> dispatcher =
      mDispatcher ? mDispatcher.get()
                  : widget ? widget->GetTextEventDispatcher() : nullptr;
  NS_ENSURE_TRUE_VOID(dispatcher && widget);

  if (!aIsSynthesizedImeKey && mWindow) {
    mWindow->UserActivity();
  } else if (aIsSynthesizedImeKey && mIMEMaskEventsCount > 0) {
    // Don't synthesize editor keys when not focused.
    return;
  }

  EventMessage msg;
  if (aAction == sdk::KeyEvent::ACTION_DOWN) {
    msg = eKeyDown;
  } else if (aAction == sdk::KeyEvent::ACTION_UP) {
    msg = eKeyUp;
  } else if (aAction == sdk::KeyEvent::ACTION_MULTIPLE) {
    // Keys with multiple action are handled in Java,
    // and we should never see one here
    MOZ_CRASH("Cannot handle key with multiple action");
  } else {
    NS_WARNING("Unknown key action event");
    return;
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  WidgetKeyboardEvent event(true, msg, widget);
  InitKeyEvent(event, aAction, aKeyCode, aScanCode, aMetaState, aTime,
               aDomPrintableKeyValue, aRepeatCount, aFlags);

  if (nsIWidget::UsePuppetWidgets()) {
    // Don't use native key bindings.
    event.PreventNativeKeyBindings();
  }

  if (aIsSynthesizedImeKey) {
    // Keys synthesized by Java IME code are saved in the mIMEKeyEvents
    // array until the next IME_REPLACE_TEXT event, at which point
    // these keys are dispatched in sequence.
    mIMEKeyEvents.AppendElement(UniquePtr<WidgetEvent>(event.Duplicate()));
  } else {
    RemoveComposition();
    NS_ENSURE_SUCCESS_VOID(BeginInputTransaction(dispatcher));
    dispatcher->DispatchKeyboardEvent(msg, event, status);
    if (widget->Destroyed() || status == nsEventStatus_eConsumeNoDefault) {
      // Skip default processing.
      return;
    }
    mEditable->OnDefaultKeyEvent(aOriginalEvent);
  }

  // Only send keypress after keydown.
  if (msg != eKeyDown) {
    return;
  }

  WidgetKeyboardEvent pressEvent(true, eKeyPress, widget);
  InitKeyEvent(pressEvent, aAction, aKeyCode, aScanCode, aKeyPressMetaState,
               aTime, aDomPrintableKeyValue, aRepeatCount, aFlags);

  if (nsIWidget::UsePuppetWidgets()) {
    // Don't use native key bindings.
    pressEvent.PreventNativeKeyBindings();
  }

  if (aIsSynthesizedImeKey) {
    mIMEKeyEvents.AppendElement(UniquePtr<WidgetEvent>(pressEvent.Duplicate()));
  } else {
    dispatcher->MaybeDispatchKeypressEvents(pressEvent, status);
  }
}

/*
 * Send dummy key events for pages that are unaware of input events,
 * to provide web compatibility for pages that depend on key events.
 */
void GeckoEditableSupport::SendIMEDummyKeyEvent(nsIWidget* aWidget,
                                                EventMessage msg) {
  nsEventStatus status = nsEventStatus_eIgnore;
  MOZ_ASSERT(mDispatcher);

  WidgetKeyboardEvent event(true, msg, aWidget);
  event.mTime = PR_Now() / 1000;
  // TODO: If we can know scan code of the key event which caused replacing
  //       composition string, we should set mCodeNameIndex here.  Then,
  //       we should rename this method because it becomes not a "dummy"
  //       keyboard event.
  event.mKeyCode = NS_VK_PROCESSKEY;
  event.mKeyNameIndex = KEY_NAME_INDEX_Process;
  // KeyboardEvents marked as "processed by IME" shouldn't cause any edit
  // actions.  So, we should set their native key binding to none before
  // dispatch to avoid crash on PuppetWidget and avoid running redundant
  // path to look for native key bindings.
  event.PreventNativeKeyBindings();
  NS_ENSURE_SUCCESS_VOID(BeginInputTransaction(mDispatcher));
  mDispatcher->DispatchKeyboardEvent(msg, event, status);
}

void GeckoEditableSupport::AddIMETextChange(const IMETextChange& aChange) {
  mIMETextChanges.AppendElement(aChange);

  // We may not be in the middle of flushing,
  // in which case this flag is meaningless.
  mIMETextChangedDuringFlush = true;

  // Now that we added a new range we need to go back and
  // update all the ranges before that.
  // Ranges that have offsets which follow this new range
  // need to be updated to reflect new offsets
  const int32_t delta = aChange.mNewEnd - aChange.mOldEnd;
  for (int32_t i = mIMETextChanges.Length() - 2; i >= 0; i--) {
    IMETextChange& previousChange = mIMETextChanges[i];
    if (previousChange.mStart > aChange.mOldEnd) {
      previousChange.mStart += delta;
      previousChange.mOldEnd += delta;
      previousChange.mNewEnd += delta;
    }
  }

  // Now go through all ranges to merge any ranges that are connected
  // srcIndex is the index of the range to merge from
  // dstIndex is the index of the range to potentially merge into
  int32_t srcIndex = mIMETextChanges.Length() - 1;
  int32_t dstIndex = srcIndex;

  while (--dstIndex >= 0) {
    IMETextChange& src = mIMETextChanges[srcIndex];
    IMETextChange& dst = mIMETextChanges[dstIndex];
    // When merging a more recent change into an older
    // change, we need to compare recent change's (start, oldEnd)
    // range to the older change's (start, newEnd)
    if (src.mOldEnd < dst.mStart || dst.mNewEnd < src.mStart) {
      // No overlap between ranges
      continue;
    }
    // When merging two ranges, there are generally four posibilities:
    // [----(----]----), (----[----]----),
    // [----(----)----], (----[----)----]
    // where [----] is the first range and (----) is the second range
    // As seen above, the start of the merged range is always the lesser
    // of the two start offsets. OldEnd and NewEnd then need to be
    // adjusted separately depending on the case. In any case, the change
    // in text length of the merged range should be the sum of text length
    // changes of the two original ranges, i.e.,
    // newNewEnd - newOldEnd == newEnd1 - oldEnd1 + newEnd2 - oldEnd2
    dst.mStart = std::min(dst.mStart, src.mStart);
    if (src.mOldEnd < dst.mNewEnd) {
      // New range overlaps or is within previous range; merge
      dst.mNewEnd += src.mNewEnd - src.mOldEnd;
    } else {  // src.mOldEnd >= dst.mNewEnd
      // New range overlaps previous range; merge
      dst.mOldEnd += src.mOldEnd - dst.mNewEnd;
      dst.mNewEnd = src.mNewEnd;
    }
    // src merged to dst; delete src.
    mIMETextChanges.RemoveElementAt(srcIndex);
    // Any ranges that we skip over between src and dst are not mergeable
    // so we can safely continue the merge starting at dst
    srcIndex = dstIndex;
  }
}

void GeckoEditableSupport::PostFlushIMEChanges() {
  if (!mIMETextChanges.IsEmpty() || mIMESelectionChanged) {
    // Already posted
    return;
  }

  RefPtr<GeckoEditableSupport> self(this);

  nsAppShell::PostEvent([this, self] {
    nsCOMPtr<nsIWidget> widget = GetWidget();
    if (widget && !widget->Destroyed()) {
      FlushIMEChanges();
    }
  });
}

void GeckoEditableSupport::FlushIMEChanges(FlushChangesFlag aFlags) {
  // Only send change notifications if we are *not* masking events,
  // i.e. if we have a focused editor,
  NS_ENSURE_TRUE_VOID(!mIMEMaskEventsCount);

  if (mIMEDelaySynchronizeReply && mIMEActiveCompositionCount > 0) {
    // We are still expecting more composition events to be handled. Once
    // that happens, FlushIMEChanges will be called again.
    return;
  }

  nsCOMPtr<nsIWidget> widget = GetWidget();
  NS_ENSURE_TRUE_VOID(widget);

  struct TextRecord {
    nsString text;
    int32_t start;
    int32_t oldEnd;
    int32_t newEnd;
  };
  AutoTArray<TextRecord, 4> textTransaction;
  textTransaction.SetCapacity(mIMETextChanges.Length());

  nsEventStatus status = nsEventStatus_eIgnore;
  mIMETextChangedDuringFlush = false;

  auto shouldAbort = [=](bool aForce) -> bool {
    if (!aForce && !mIMETextChangedDuringFlush) {
      return false;
    }
    // A query event could have triggered more text changes to come in, as
    // indicated by our flag. If that happens, try flushing IME changes
    // again.
    if (aFlags == FLUSH_FLAG_NONE) {
      FlushIMEChanges(FLUSH_FLAG_RETRY);
    } else {
      // Don't retry if already retrying, to avoid infinite loops.
      __android_log_print(ANDROID_LOG_WARN, "GeckoEditableSupport",
                          "Already retrying IME flush");
    }
    return true;
  };

  for (const IMETextChange& change : mIMETextChanges) {
    if (change.mStart == change.mOldEnd && change.mStart == change.mNewEnd) {
      continue;
    }

    WidgetQueryContentEvent event(true, eQueryTextContent, widget);

    if (change.mNewEnd != change.mStart) {
      event.InitForQueryTextContent(change.mStart,
                                    change.mNewEnd - change.mStart);
      widget->DispatchEvent(&event, status);

      if (shouldAbort(NS_WARN_IF(!event.mSucceeded))) {
        return;
      }
    }

    textTransaction.AppendElement(TextRecord{
        event.mReply.mString, change.mStart, change.mOldEnd, change.mNewEnd});
  }

  int32_t selStart = -1;
  int32_t selEnd = -1;

  if (mIMESelectionChanged) {
    WidgetQueryContentEvent event(true, eQuerySelectedText, widget);
    widget->DispatchEvent(&event, status);

    if (shouldAbort(NS_WARN_IF(!event.mSucceeded))) {
      return;
    }

    selStart = int32_t(event.GetSelectionStart());
    selEnd = int32_t(event.GetSelectionEnd());

    if (aFlags == FLUSH_FLAG_RECOVER) {
      // Sometimes we get out-of-bounds selection during recovery.
      // Limit the offsets so we don't crash.
      for (const TextRecord& record : textTransaction) {
        const int32_t end = record.start + record.text.Length();
        selStart = std::min(selStart, end);
        selEnd = std::min(selEnd, end);
      }
    }
  }

  JNIEnv* const env = jni::GetGeckoThreadEnv();
  auto flushOnException = [=]() -> bool {
    if (!env->ExceptionCheck()) {
      return false;
    }
    if (aFlags != FLUSH_FLAG_RECOVER) {
      // First time seeing an exception; try flushing text.
      env->ExceptionClear();
      __android_log_print(ANDROID_LOG_WARN, "GeckoEditableSupport",
                          "Recovering from IME exception");
      FlushIMEText(FLUSH_FLAG_RECOVER);
    } else {
      // Give up because we've already tried.
#ifdef RELEASE_OR_BETA
      env->ExceptionClear();
#else
      MOZ_CATCH_JNI_EXCEPTION(env);
#endif
    }
    return true;
  };

  // Commit the text change and selection change transaction.
  mIMETextChanges.Clear();

  for (const TextRecord& record : textTransaction) {
    mEditable->OnTextChange(record.text, record.start, record.oldEnd,
                            record.newEnd);
    if (flushOnException()) {
      return;
    }
  }

  while (mIMEDelaySynchronizeReply && mIMEActiveSynchronizeCount) {
    mIMEActiveSynchronizeCount--;
    mEditable->NotifyIME(EditableListener::NOTIFY_IME_REPLY_EVENT);
  }
  mIMEDelaySynchronizeReply = false;
  mIMEActiveSynchronizeCount = 0;
  mIMEActiveCompositionCount = 0;

  if (mIMESelectionChanged) {
    mIMESelectionChanged = false;
    mEditable->OnSelectionChange(selStart, selEnd);
    flushOnException();
  }
}

void GeckoEditableSupport::FlushIMEText(FlushChangesFlag aFlags) {
  // Notify Java of the newly focused content
  mIMETextChanges.Clear();
  mIMESelectionChanged = true;

  // Use 'INT32_MAX / 2' here because subsequent text changes might combine
  // with this text change, and overflow might occur if we just use
  // INT32_MAX.
  IMENotification notification(NOTIFY_IME_OF_TEXT_CHANGE);
  notification.mTextChangeData.mStartOffset = 0;
  notification.mTextChangeData.mRemovedEndOffset = INT32_MAX / 2;
  notification.mTextChangeData.mAddedEndOffset = INT32_MAX / 2;
  NotifyIME(mDispatcher, notification);

  FlushIMEChanges(aFlags);
}

void GeckoEditableSupport::UpdateCompositionRects() {
  nsCOMPtr<nsIWidget> widget = GetWidget();
  RefPtr<TextComposition> composition(GetComposition());
  NS_ENSURE_TRUE_VOID(mDispatcher && widget);

  if (!composition) {
    return;
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  uint32_t offset = composition->NativeOffsetOfStartComposition();
  WidgetQueryContentEvent textRects(true, eQueryTextRectArray, widget);
  textRects.InitForQueryTextRectArray(offset, composition->String().Length());
  widget->DispatchEvent(&textRects, status);

  auto rects = ConvertRectArrayToJavaRectFArray(textRects.mReply.mRectArray,
                                                widget->GetDefaultScale());

  mEditable->UpdateCompositionRects(rects);
}

void GeckoEditableSupport::OnImeSynchronize() {
  if (mIMEDelaySynchronizeReply) {
    // If we are waiting for other events to reply,
    // queue this reply as well.
    mIMEActiveSynchronizeCount++;
    return;
  }
  if (!mIMEMaskEventsCount) {
    FlushIMEChanges();
  }
  mEditable->NotifyIME(EditableListener::NOTIFY_IME_REPLY_EVENT);
}

void GeckoEditableSupport::OnImeReplaceText(int32_t aStart, int32_t aEnd,
                                            jni::String::Param aText) {
  if (DoReplaceText(aStart, aEnd, aText)) {
    mIMEDelaySynchronizeReply = true;
  }

  OnImeSynchronize();
}

bool GeckoEditableSupport::DoReplaceText(int32_t aStart, int32_t aEnd,
                                         jni::String::Param aText) {
  ALOGIME("IME: IME_REPLACE_TEXT: text=\"%s\"",
          NS_ConvertUTF16toUTF8(aText->ToString()).get());

  // Return true if processed and we should reply to the OnImeReplaceText
  // event later. Return false if _not_ processed and we should reply to the
  // OnImeReplaceText event now.

  if (mIMEMaskEventsCount > 0) {
    // Not focused; still reply to events, but don't do anything else.
    return false;
  }

  if (mWindow) {
    mWindow->UserActivity();
  }

  /*
      Replace text in Gecko thread from aStart to aEnd with the string text.
  */
  nsCOMPtr<nsIWidget> widget = GetWidget();
  NS_ENSURE_TRUE(mDispatcher && widget, false);
  NS_ENSURE_SUCCESS(BeginInputTransaction(mDispatcher), false);

  RefPtr<TextComposition> composition(GetComposition());
  MOZ_ASSERT(!composition || !composition->IsEditorHandlingEvent());

  nsString string(aText->ToString());
  const bool composing = !mIMERanges->IsEmpty();
  nsEventStatus status = nsEventStatus_eIgnore;
  bool textChanged = composing;

  if (!mIMEKeyEvents.IsEmpty() || !composition || !mDispatcher->IsComposing() ||
      uint32_t(aStart) != composition->NativeOffsetOfStartComposition() ||
      uint32_t(aEnd) != composition->NativeOffsetOfStartComposition() +
                            composition->String().Length()) {
    // Only start a new composition if we have key events,
    // if we don't have an existing composition, or
    // the replaced text does not match our composition.
    textChanged |= RemoveComposition();

    {
      // Use text selection to set target position(s) for
      // insert, or replace, of text.
      WidgetSelectionEvent event(true, eSetSelection, widget);
      event.mOffset = uint32_t(aStart);
      event.mLength = uint32_t(aEnd - aStart);
      event.mExpandToClusterBoundary = false;
      event.mReason = nsISelectionListener::IME_REASON;
      widget->DispatchEvent(&event, status);
    }

    if (!mIMEKeyEvents.IsEmpty()) {
      for (uint32_t i = 0; i < mIMEKeyEvents.Length(); i++) {
        const auto event = mIMEKeyEvents[i]->AsKeyboardEvent();
        // widget for duplicated events is initially nullptr.
        event->mWidget = widget;

        if (event->mMessage != eKeyPress) {
          mDispatcher->DispatchKeyboardEvent(event->mMessage, *event, status);
        } else {
          mDispatcher->MaybeDispatchKeypressEvents(*event, status);
        }
        if (!mDispatcher || widget->Destroyed()) {
          break;
        }
      }
      mIMEKeyEvents.Clear();
      return false;
    }

    if (aStart != aEnd) {
      // Perform a deletion first.
      WidgetContentCommandEvent event(true, eContentCommandDelete, widget);
      event.mTime = PR_Now() / 1000;
      widget->DispatchEvent(&event, status);
      if (!mDispatcher || widget->Destroyed()) {
        return false;
      }
      textChanged = true;
    }
  } else if (composition->String().Equals(string)) {
    /* If the new text is the same as the existing composition text,
     * the NS_COMPOSITION_CHANGE event does not generate a text
     * change notification. However, the Java side still expects
     * one, so we manually generate a notification. */
    IMETextChange dummyChange;
    dummyChange.mStart = aStart;
    dummyChange.mOldEnd = dummyChange.mNewEnd = aEnd;
    PostFlushIMEChanges();
    mIMESelectionChanged = true;
    AddIMETextChange(dummyChange);
    textChanged = true;
  }

  if (sDispatchKeyEventsInCompositionForAnyApps ||
      mInputContext.mMayBeIMEUnaware) {
    SendIMEDummyKeyEvent(widget, eKeyDown);
    if (!mDispatcher || widget->Destroyed()) {
      return false;
    }
  }

  if (composing) {
    mDispatcher->SetPendingComposition(string, mIMERanges);
    mDispatcher->FlushPendingComposition(status);
    mIMEActiveCompositionCount++;
    // Ensure IME ranges are empty.
    mIMERanges->Clear();
  } else if (!string.IsEmpty() || mDispatcher->IsComposing()) {
    mDispatcher->CommitComposition(status, &string);
    mIMEActiveCompositionCount++;
    textChanged = true;
  }
  if (!mDispatcher || widget->Destroyed()) {
    return false;
  }

  if (sDispatchKeyEventsInCompositionForAnyApps ||
      mInputContext.mMayBeIMEUnaware) {
    SendIMEDummyKeyEvent(widget, eKeyUp);
    // Widget may be destroyed after dispatching the above event.
  }
  return textChanged;
}

void GeckoEditableSupport::OnImeAddCompositionRange(
    int32_t aStart, int32_t aEnd, int32_t aRangeType, int32_t aRangeStyle,
    int32_t aRangeLineStyle, bool aRangeBoldLine, int32_t aRangeForeColor,
    int32_t aRangeBackColor, int32_t aRangeLineColor) {
  if (mIMEMaskEventsCount > 0) {
    // Not focused.
    return;
  }

  TextRange range;
  range.mStartOffset = aStart;
  range.mEndOffset = aEnd;
  range.mRangeType = ToTextRangeType(aRangeType);
  range.mRangeStyle.mDefinedStyles = aRangeStyle;
  range.mRangeStyle.mLineStyle = aRangeLineStyle;
  range.mRangeStyle.mIsBoldLine = aRangeBoldLine;
  range.mRangeStyle.mForegroundColor =
      ConvertAndroidColor(uint32_t(aRangeForeColor));
  range.mRangeStyle.mBackgroundColor =
      ConvertAndroidColor(uint32_t(aRangeBackColor));
  range.mRangeStyle.mUnderlineColor =
      ConvertAndroidColor(uint32_t(aRangeLineColor));
  mIMERanges->AppendElement(range);
}

void GeckoEditableSupport::OnImeUpdateComposition(int32_t aStart, int32_t aEnd,
                                                  int32_t aFlags) {
  if (DoUpdateComposition(aStart, aEnd, aFlags)) {
    mIMEDelaySynchronizeReply = true;
  }
}

bool GeckoEditableSupport::DoUpdateComposition(int32_t aStart, int32_t aEnd,
                                               int32_t aFlags) {
  if (mIMEMaskEventsCount > 0) {
    // Not focused.
    return false;
  }

  nsCOMPtr<nsIWidget> widget = GetWidget();
  nsEventStatus status = nsEventStatus_eIgnore;
  NS_ENSURE_TRUE(mDispatcher && widget, false);

  const bool keepCurrent =
      !!(aFlags & java::GeckoEditableChild::FLAG_KEEP_CURRENT_COMPOSITION);

  // A composition with no ranges means we want to set the selection.
  if (mIMERanges->IsEmpty()) {
    if (keepCurrent && mDispatcher->IsComposing()) {
      // Don't set selection if we want to keep current composition.
      return false;
    }

    MOZ_ASSERT(aStart >= 0 && aEnd >= 0);
    const bool compositionChanged = RemoveComposition();

    WidgetSelectionEvent selEvent(true, eSetSelection, widget);
    selEvent.mOffset = std::min(aStart, aEnd);
    selEvent.mLength = std::max(aStart, aEnd) - selEvent.mOffset;
    selEvent.mReversed = aStart > aEnd;
    selEvent.mExpandToClusterBoundary = false;
    widget->DispatchEvent(&selEvent, status);
    return compositionChanged;
  }

  /**
   * Update the composition from aStart to aEnd using information from added
   * ranges. This is only used for visual indication and does not affect the
   * text content.  Only the offsets are specified and not the text content
   * to eliminate the possibility of this event altering the text content
   * unintentionally.
   */
  nsString string;
  RefPtr<TextComposition> composition(GetComposition());
  MOZ_ASSERT(!composition || !composition->IsEditorHandlingEvent());

  if (!composition || !mDispatcher->IsComposing() ||
      uint32_t(aStart) != composition->NativeOffsetOfStartComposition() ||
      uint32_t(aEnd) != composition->NativeOffsetOfStartComposition() +
                            composition->String().Length()) {
    if (keepCurrent) {
      // Don't start a new composition if we want to keep the current one.
      mIMERanges->Clear();
      return false;
    }

    // Only start new composition if we don't have an existing one,
    // or if the existing composition doesn't match the new one.
    RemoveComposition();

    {
      WidgetSelectionEvent event(true, eSetSelection, widget);
      event.mOffset = uint32_t(aStart);
      event.mLength = uint32_t(aEnd - aStart);
      event.mExpandToClusterBoundary = false;
      event.mReason = nsISelectionListener::IME_REASON;
      widget->DispatchEvent(&event, status);
    }

    {
      WidgetQueryContentEvent event(true, eQuerySelectedText, widget);
      widget->DispatchEvent(&event, status);
      MOZ_ASSERT(event.mSucceeded);
      string = event.mReply.mString;
    }
  } else {
    // If the new composition matches the existing composition,
    // reuse the old composition.
    string = composition->String();
  }

#ifdef DEBUG_ANDROID_IME
  const NS_ConvertUTF16toUTF8 data(string);
  const char* text = data.get();
  ALOGIME("IME: IME_SET_TEXT: text=\"%s\", length=%u, range=%u", text,
          string.Length(), mIMERanges->Length());
#endif  // DEBUG_ANDROID_IME

  if (NS_WARN_IF(NS_FAILED(BeginInputTransaction(mDispatcher)))) {
    mIMERanges->Clear();
    return false;
  }
  mDispatcher->SetPendingComposition(string, mIMERanges);
  mDispatcher->FlushPendingComposition(status);
  mIMEActiveCompositionCount++;
  mIMERanges->Clear();
  return true;
}

void GeckoEditableSupport::OnImeRequestCursorUpdates(int aRequestMode) {
  if (aRequestMode == EditableClient::ONE_SHOT) {
    UpdateCompositionRects();
    return;
  }

  mIMEMonitorCursor = (aRequestMode == EditableClient::START_MONITOR);
}

void GeckoEditableSupport::AsyncNotifyIME(int32_t aNotification) {
  RefPtr<GeckoEditableSupport> self(this);

  nsAppShell::PostEvent([this, self, aNotification] {
    if (!mIMEMaskEventsCount) {
      mEditable->NotifyIME(aNotification);
    }
  });
}

nsresult GeckoEditableSupport::NotifyIME(
    TextEventDispatcher* aTextEventDispatcher,
    const IMENotification& aNotification) {
  MOZ_ASSERT(mEditable);

  switch (aNotification.mMessage) {
    case REQUEST_TO_COMMIT_COMPOSITION: {
      ALOGIME("IME: REQUEST_TO_COMMIT_COMPOSITION");

      RemoveComposition(COMMIT_IME_COMPOSITION);
      AsyncNotifyIME(EditableListener::NOTIFY_IME_TO_COMMIT_COMPOSITION);
      break;
    }

    case REQUEST_TO_CANCEL_COMPOSITION: {
      ALOGIME("IME: REQUEST_TO_CANCEL_COMPOSITION");

      RemoveComposition(CANCEL_IME_COMPOSITION);
      AsyncNotifyIME(EditableListener::NOTIFY_IME_TO_CANCEL_COMPOSITION);
      break;
    }

    case NOTIFY_IME_OF_FOCUS: {
      ALOGIME("IME: NOTIFY_IME_OF_FOCUS");

      mIMEFocusCount++;

      RefPtr<GeckoEditableSupport> self(this);
      RefPtr<TextEventDispatcher> dispatcher = aTextEventDispatcher;

      // Post an event because we have to flush the text before sending a
      // focus event, and we may not be able to flush text during the
      // NotifyIME call.
      nsAppShell::PostEvent([this, self, dispatcher] {
        nsCOMPtr<nsIWidget> widget = dispatcher->GetWidget();

        --mIMEMaskEventsCount;
        if (!mIMEFocusCount || !widget || widget->Destroyed()) {
          return;
        }

        mEditable->NotifyIME(EditableListener::NOTIFY_IME_OF_TOKEN);

        if (mIsRemote) {
          if (!mEditableAttached) {
            // Re-attach on focus; see OnRemovedFrom().
            AttachNative(mEditable, this);
            mEditableAttached = true;
          }
          // Because GeckoEditableSupport in content process doesn't
          // manage the active input context, we need to retrieve the
          // input context from the widget, for use by
          // OnImeReplaceText.
          mInputContext = widget->GetInputContext();
        }
        mDispatcher = dispatcher;
        mIMEKeyEvents.Clear();
        FlushIMEText();

        // IME will call requestCursorUpdates after getting context.
        // So reset cursor update mode before getting context.
        mIMEMonitorCursor = false;

        mEditable->NotifyIME(EditableListener::NOTIFY_IME_OF_FOCUS);
      });
      break;
    }

    case NOTIFY_IME_OF_BLUR: {
      ALOGIME("IME: NOTIFY_IME_OF_BLUR");

      mIMEFocusCount--;
      MOZ_ASSERT(mIMEFocusCount >= 0);

      RefPtr<GeckoEditableSupport> self(this);
      nsAppShell::PostEvent([this, self] {
        if (!mIMEFocusCount) {
          mIMEDelaySynchronizeReply = false;
          mIMEActiveSynchronizeCount = 0;
          mIMEActiveCompositionCount = 0;
          mEditable->NotifyIME(EditableListener::NOTIFY_IME_OF_BLUR);
          OnRemovedFrom(mDispatcher);
        }
      });

      // Mask events because we lost focus. Unmask on the next focus.
      mIMEMaskEventsCount++;
      break;
    }

    case NOTIFY_IME_OF_SELECTION_CHANGE: {
      ALOGIME("IME: NOTIFY_IME_OF_SELECTION_CHANGE");

      PostFlushIMEChanges();
      mIMESelectionChanged = true;
      break;
    }

    case NOTIFY_IME_OF_TEXT_CHANGE: {
      ALOGIME("IME: NotifyIMEOfTextChange: s=%d, oe=%d, ne=%d",
              aNotification.mTextChangeData.mStartOffset,
              aNotification.mTextChangeData.mRemovedEndOffset,
              aNotification.mTextChangeData.mAddedEndOffset);

      /* Make sure Java's selection is up-to-date */
      PostFlushIMEChanges();
      mIMESelectionChanged = true;
      AddIMETextChange(IMETextChange(aNotification));
      break;
    }

    case NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED: {
      ALOGIME("IME: NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED");

      // NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED isn't sent per IME call.
      // Receiving this event means that Gecko has already handled all IME
      // composing events in queue.
      //
      if (mIsRemote) {
        OnNotifyIMEOfCompositionEventHandled();
      } else {
        // Also, when receiving this event, mIMEDelaySynchronizeReply won't
        // update yet on non-e10s case since IME event is posted before updating
        // it. So we have to delay handling of this event.
        RefPtr<GeckoEditableSupport> self(this);
        nsAppShell::PostEvent(
            [this, self] { OnNotifyIMEOfCompositionEventHandled(); });
      }
      break;
    }

    default:
      break;
  }
  return NS_OK;
}

void GeckoEditableSupport::OnNotifyIMEOfCompositionEventHandled() {
  // NOTIFY_IME_OF_COMPOSITION_EVENT_HANDLED may be merged with multiple events,
  // so reset count.
  mIMEActiveCompositionCount = 0;
  if (mIMEDelaySynchronizeReply) {
    FlushIMEChanges();
  }

  // Hardware keyboard support requires each string rect.
  if (mIMEMonitorCursor) {
    UpdateCompositionRects();
  }
}

void GeckoEditableSupport::OnRemovedFrom(
    TextEventDispatcher* aTextEventDispatcher) {
  mDispatcher = nullptr;

  if (mIsRemote && mEditable->HasEditableParent()) {
    // When we're remote, detach every time.
    OnDetach(NS_NewRunnableFunction(
        "GeckoEditableSupport::OnRemovedFrom",
        [editable = java::GeckoEditableChild::GlobalRef(mEditable)] {
          DisposeNative(editable);
        }));
  }
}

void GeckoEditableSupport::WillDispatchKeyboardEvent(
    TextEventDispatcher* aTextEventDispatcher,
    WidgetKeyboardEvent& aKeyboardEvent, uint32_t aIndexOfKeypress,
    void* aData) {}

NS_IMETHODIMP_(IMENotificationRequests)
GeckoEditableSupport::GetIMENotificationRequests() {
  return IMENotificationRequests(IMENotificationRequests::NOTIFY_TEXT_CHANGE);
}

void GeckoEditableSupport::SetInputContext(const InputContext& aContext,
                                           const InputContextAction& aAction) {
  MOZ_ASSERT(mEditable);

  ALOGIME("IME: SetInputContext: s=0x%X, 0x%X, action=0x%X, 0x%X",
          aContext.mIMEState.mEnabled, aContext.mIMEState.mOpen, aAction.mCause,
          aAction.mFocusChange);

  mInputContext = aContext;

  if (mInputContext.mIMEState.mEnabled != IMEState::DISABLED &&
      aAction.UserMightRequestOpenVKB()) {
    // Don't reset keyboard when we should simply open the vkb
    mEditable->NotifyIME(EditableListener::NOTIFY_IME_OPEN_VKB);
    return;
  }

  // Post an event to keep calls in order relative to NotifyIME.
  nsAppShell::PostEvent([this, self = RefPtr<GeckoEditableSupport>(this),
                         context = mInputContext, action = aAction] {
    nsCOMPtr<nsIWidget> widget = GetWidget();

    if (!widget || widget->Destroyed()) {
      return;
    }
    NotifyIMEContext(context, action);
  });
}

void GeckoEditableSupport::NotifyIMEContext(const InputContext& aContext,
                                            const InputContextAction& aAction) {
  const bool inPrivateBrowsing = aContext.mInPrivateBrowsing;
  const bool isUserAction =
      aAction.IsHandlingUserInput() || aContext.mHasHandledUserInput;
  const int32_t flags =
      (inPrivateBrowsing ? EditableListener::IME_FLAG_PRIVATE_BROWSING : 0) |
      (isUserAction ? EditableListener::IME_FLAG_USER_ACTION : 0);

  mEditable->NotifyIMEContext(
      aContext.mIMEState.mEnabled, aContext.mHTMLInputType,
      aContext.mHTMLInputInputmode, aContext.mActionHint, flags);
}

InputContext GeckoEditableSupport::GetInputContext() {
  InputContext context = mInputContext;
  context.mIMEState.mOpen = IMEState::OPEN_STATE_NOT_SUPPORTED;
  return context;
}

void GeckoEditableSupport::TransferParent(jni::Object::Param aEditableParent) {
  mEditable->SetParent(aEditableParent);

  // If we are already focused, make sure the new parent has our token
  // and focus information, so it can accept additional calls from us.
  if (mIMEFocusCount > 0) {
    mEditable->NotifyIME(EditableListener::NOTIFY_IME_OF_TOKEN);
    NotifyIMEContext(mInputContext, InputContextAction());
    mEditable->NotifyIME(EditableListener::NOTIFY_IME_OF_FOCUS);
  }

  if (mIsRemote && !mDispatcher) {
    // Detach now if we were only attached temporarily.
    OnRemovedFrom(/* dispatcher */ nullptr);
  }
}

void GeckoEditableSupport::SetOnBrowserChild(dom::BrowserChild* aBrowserChild) {
  MOZ_ASSERT(!XRE_IsParentProcess());
  NS_ENSURE_TRUE_VOID(aBrowserChild);

  const dom::ContentChild* const contentChild =
      dom::ContentChild::GetSingleton();
  RefPtr<widget::PuppetWidget> widget(aBrowserChild->WebWidget());
  NS_ENSURE_TRUE_VOID(contentChild && widget);

  // Get the content/tab ID in order to get the correct
  // IGeckoEditableParent object, which GeckoEditableChild uses to
  // communicate with the parent process.
  const uint64_t contentId = contentChild->GetID();
  const uint64_t tabId = aBrowserChild->GetTabId();
  NS_ENSURE_TRUE_VOID(contentId && tabId);

  RefPtr<widget::TextEventDispatcherListener> listener =
      widget->GetNativeTextEventDispatcherListener();

  if (!listener ||
      listener.get() ==
          static_cast<widget::TextEventDispatcherListener*>(widget)) {
    // We need to set a new listener.
    const auto editableChild = java::GeckoEditableChild::New(
        /* parent */ nullptr, /* default */ false);
    RefPtr<widget::GeckoEditableSupport> editableSupport =
        new widget::GeckoEditableSupport(editableChild);

    // Tell PuppetWidget to use our listener for IME operations.
    widget->SetNativeTextEventDispatcherListener(editableSupport);

    // Temporarily attach so we can receive the initial editable parent.
    AttachNative(editableChild, editableSupport);
    editableSupport->mEditableAttached = true;

    // Connect the new child to a parent that corresponds to the BrowserChild.
    java::GeckoServiceChildProcess::GetEditableParent(editableChild, contentId,
                                                      tabId);
    return;
  }

  // We need to update the existing listener to use the new parent.

  // We expect the existing TextEventDispatcherListener to be a
  // GeckoEditableSupport object, so we perform a sanity check to make
  // sure, by comparing their respective vtable pointers.
  const RefPtr<widget::GeckoEditableSupport> dummy =
      new widget::GeckoEditableSupport(/* child */ nullptr);
  NS_ENSURE_TRUE_VOID(*reinterpret_cast<const uintptr_t*>(listener.get()) ==
                      *reinterpret_cast<const uintptr_t*>(dummy.get()));

  const auto support =
      static_cast<widget::GeckoEditableSupport*>(listener.get());
  if (!support->mEditableAttached) {
    // Temporarily attach so we can receive the initial editable parent.
    AttachNative(support->GetJavaEditable(), support);
    support->mEditableAttached = true;
  }

  // Transfer to a new parent that corresponds to the BrowserChild.
  java::GeckoServiceChildProcess::GetEditableParent(support->GetJavaEditable(),
                                                    contentId, tabId);
}

}  // namespace widget
}  // namespace mozilla
