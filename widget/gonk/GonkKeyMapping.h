/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef GONKKEYMAPPING_H
#define GONKKEYMAPPING_H

/* See libui/KeycodeLabels.h for the mapping */
static const unsigned long kKeyMapping[] = {
    0,
    0, // SOFT_LEFT
    0, // SOFT_RIGHT
    NS_VK_HOME, // HOME
    NS_VK_ESCAPE,
    0, // CALL
    0, // ENDCALL
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
    NS_VK_DELETE,
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
    0, // HEADSETHOOK
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
    0, // FORWARD_DEL
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
    NS_VK_ENTER,
    NS_VK_EQUALS,
    // There are more but we don't map them
};
#endif /* GONKKEYMAPPING_H */
