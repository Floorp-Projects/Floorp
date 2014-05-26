/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef GONKKEYMAPPING_H
#define GONKKEYMAPPING_H

#include "libui/android_keycodes.h"
#include "mozilla/EventForwards.h"

namespace mozilla {
namespace widget {

/* See libui/KeycodeLabels.h for the mapping */
static const unsigned long kKeyMapping[] = {
    0,
    0, // SOFT_LEFT
    0, // SOFT_RIGHT
    NS_VK_HOME, // HOME
    0, // BACK
    0, // CALL
    NS_VK_SLEEP, // ENDCALL
    NS_VK_0,
    NS_VK_1,
    NS_VK_2,
    NS_VK_3,
    NS_VK_4,
    NS_VK_5,
    NS_VK_6,
    NS_VK_7,
    NS_VK_8,
    NS_VK_9,
    NS_VK_ASTERISK,
    NS_VK_HASH,
    NS_VK_UP,
    NS_VK_DOWN,
    NS_VK_LEFT,
    NS_VK_RIGHT,
    NS_VK_SELECT,
    NS_VK_PAGE_UP,   // VOLUME_UP
    NS_VK_PAGE_DOWN, // VOLUME_DOWN
    NS_VK_SLEEP,     // POWER
    NS_VK_PRINTSCREEN, // CAMERA
    NS_VK_CLEAR,
    NS_VK_A,
    NS_VK_B,
    NS_VK_C,
    NS_VK_D,
    NS_VK_E,
    NS_VK_F,
    NS_VK_G,
    NS_VK_H,
    NS_VK_I,
    NS_VK_J,
    NS_VK_K,
    NS_VK_L,
    NS_VK_M,
    NS_VK_N,
    NS_VK_O,
    NS_VK_P,
    NS_VK_Q,
    NS_VK_R,
    NS_VK_S,
    NS_VK_T,
    NS_VK_U,
    NS_VK_V,
    NS_VK_W,
    NS_VK_X,
    NS_VK_Y,
    NS_VK_Z,
    NS_VK_COMMA,
    NS_VK_PERIOD,
    0,
    0,
    0,
    0,
    NS_VK_TAB,
    NS_VK_SPACE,
    NS_VK_META, // SYM
    0, // EXPLORER
    0, // ENVELOPE
    NS_VK_RETURN, // ENTER
    NS_VK_BACK,
    NS_VK_BACK_QUOTE, // GRAVE
    NS_VK_HYPHEN_MINUS,
    NS_VK_EQUALS,
    NS_VK_OPEN_BRACKET,
    NS_VK_CLOSE_BRACKET,
    NS_VK_BACK_SLASH,
    NS_VK_SEMICOLON,
    NS_VK_QUOTE,
    NS_VK_SLASH,
    NS_VK_AT,
    0, // NUM
    NS_VK_F1, // HEADSETHOOK
    0, // FOCUS
    NS_VK_PLUS,
    NS_VK_CONTEXT_MENU,
    0, // NOTIFICATION
    NS_VK_F5, // SEARCH
    0, // MEDIA_PLAY_PAUSE
    0, // MEDIA_STOP
    0, // MEDIA_NEXT
    0, // MEDIA_PREVIOUS
    0, // MEDIA_REWIND
    0, // MEDIA_FAST_FORWARD
    0, // MUTE
    0, // PAGE_UP
    0, // PAGE_DOWN
    0, // PICTSYMBOLS
    0, // SWITCH_CHARSET
    0, // BUTTON_A
    0, // BUTTON_B
    0, // BUTTON_C
    0, // BUTTON_X
    0, // BUTTON_Y
    0, // BUTTON_Z
    0, // BUTTON_L1
    0, // BUTTON_R1
    0, // BUTTON_L2
    0, // BUTTON_R2
    0, // BUTTON_THUMBL
    0, // BUTTON_THUMBR
    0, // BUTTON_START
    0, // BUTTON_SELECT
    0, // BUTTON_MODE
    0, // ESCAPE
    NS_VK_DELETE,
    0, // CTRL_LEFT
    0, // CTRL_RIGHT
    NS_VK_CAPS_LOCK,
    NS_VK_SCROLL_LOCK,
    0, // META_LEFT
    0, // META_RIGHT
    0, // FUNCTION
    0, // SYSRQ
    0, // BREAK
    NS_VK_HOME, // MOVE_HOME
    NS_VK_END,
    NS_VK_INSERT,
    0, // FORWARD
    0, // MEDIA_PLAY
    0, // MEDIA_PAUSE
    0, // MEDIA_CLOSE
    0, // MEDIA_EJECT
    0, // MEDIA_RECORD
    NS_VK_F1,
    NS_VK_F2,
    NS_VK_F3,
    NS_VK_F4,
    NS_VK_F5,
    NS_VK_F6,
    NS_VK_F7,
    NS_VK_F8,
    NS_VK_F9,
    NS_VK_F10,
    NS_VK_F11,
    NS_VK_F12,
    NS_VK_NUM_LOCK,
    NS_VK_NUMPAD0,
    NS_VK_NUMPAD1,
    NS_VK_NUMPAD2,
    NS_VK_NUMPAD3,
    NS_VK_NUMPAD4,
    NS_VK_NUMPAD5,
    NS_VK_NUMPAD6,
    NS_VK_NUMPAD7,
    NS_VK_NUMPAD8,
    NS_VK_NUMPAD9,
    NS_VK_DIVIDE,
    NS_VK_MULTIPLY,
    NS_VK_SUBTRACT,
    NS_VK_ADD,
    NS_VK_PERIOD,
    NS_VK_COMMA,
    NS_VK_RETURN,
    NS_VK_EQUALS,
    // There are more but we don't map them
};

static KeyNameIndex GetKeyNameIndex(int aKeyCode)
{
    switch (aKeyCode) {
#define NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX(aNativeKey, aKeyNameIndex) \
    case aNativeKey: return aKeyNameIndex;

#include "NativeKeyToDOMKeyName.h"

#undef NS_NATIVE_KEY_TO_DOM_KEY_NAME_INDEX

    case AKEYCODE_0:
    case AKEYCODE_1:
    case AKEYCODE_2:
    case AKEYCODE_3:
    case AKEYCODE_4:
    case AKEYCODE_5:
    case AKEYCODE_6:
    case AKEYCODE_7:
    case AKEYCODE_8:
    case AKEYCODE_9:
    case AKEYCODE_STAR:
    case AKEYCODE_POUND:
    case AKEYCODE_A:
    case AKEYCODE_B:
    case AKEYCODE_C:
    case AKEYCODE_D:
    case AKEYCODE_E:
    case AKEYCODE_F:
    case AKEYCODE_G:
    case AKEYCODE_H:
    case AKEYCODE_I:
    case AKEYCODE_J:
    case AKEYCODE_K:
    case AKEYCODE_L:
    case AKEYCODE_M:
    case AKEYCODE_N:
    case AKEYCODE_O:
    case AKEYCODE_P:
    case AKEYCODE_Q:
    case AKEYCODE_R:
    case AKEYCODE_S:
    case AKEYCODE_T:
    case AKEYCODE_U:
    case AKEYCODE_V:
    case AKEYCODE_W:
    case AKEYCODE_X:
    case AKEYCODE_Y:
    case AKEYCODE_Z:
    case AKEYCODE_COMMA:
    case AKEYCODE_PERIOD:
    case AKEYCODE_SPACE:
    case AKEYCODE_GRAVE:
    case AKEYCODE_MINUS:
    case AKEYCODE_EQUALS:
    case AKEYCODE_LEFT_BRACKET:
    case AKEYCODE_RIGHT_BRACKET:
    case AKEYCODE_BACKSLASH:
    case AKEYCODE_SEMICOLON:
    case AKEYCODE_APOSTROPHE:
    case AKEYCODE_SLASH:
    case AKEYCODE_AT:
    case AKEYCODE_PLUS:
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
        return KEY_NAME_INDEX_USE_STRING;

    default:
        return KEY_NAME_INDEX_Unidentified;
    }
}

static CodeNameIndex GetCodeNameIndex(int aScanCode)
{
    switch (aScanCode) {
#define NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX(aNativeKey, aCodeNameIndex) \
    case aNativeKey: return aCodeNameIndex;

#include "NativeKeyToDOMCodeName.h"

#undef NS_NATIVE_KEY_TO_DOM_CODE_NAME_INDEX

    default:
        return CODE_NAME_INDEX_UNKNOWN;
    }
}

} // namespace widget
} // namespace mozilla

#endif /* GONKKEYMAPPING_H */
