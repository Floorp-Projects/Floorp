/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidJavaWrappers_h__
#define AndroidJavaWrappers_h__

#include <jni.h>
#include <android/input.h>
#include <android/log.h>
#include <android/api-level.h>

#include "nsRect.h"
#include "nsString.h"
#include "nsTArray.h"
#include "nsIAndroidBridge.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/dom/Touch.h"
#include "mozilla/EventForwards.h"
#include "InputData.h"
#include "Units.h"
#include "FrameMetrics.h"

//#define FORCE_ALOG 1

class nsIAndroidDisplayport;
class nsIWidget;

namespace mozilla {

enum {
    // These keycode masks are not defined in android/keycodes.h:
#if __ANDROID_API__ < 13
    AKEYCODE_ESCAPE             = 111,
    AKEYCODE_FORWARD_DEL        = 112,
    AKEYCODE_CTRL_LEFT          = 113,
    AKEYCODE_CTRL_RIGHT         = 114,
    AKEYCODE_CAPS_LOCK          = 115,
    AKEYCODE_SCROLL_LOCK        = 116,
    AKEYCODE_META_LEFT          = 117,
    AKEYCODE_META_RIGHT         = 118,
    AKEYCODE_FUNCTION           = 119,
    AKEYCODE_SYSRQ              = 120,
    AKEYCODE_BREAK              = 121,
    AKEYCODE_MOVE_HOME          = 122,
    AKEYCODE_MOVE_END           = 123,
    AKEYCODE_INSERT             = 124,
    AKEYCODE_FORWARD            = 125,
    AKEYCODE_MEDIA_PLAY         = 126,
    AKEYCODE_MEDIA_PAUSE        = 127,
    AKEYCODE_MEDIA_CLOSE        = 128,
    AKEYCODE_MEDIA_EJECT        = 129,
    AKEYCODE_MEDIA_RECORD       = 130,
    AKEYCODE_F1                 = 131,
    AKEYCODE_F2                 = 132,
    AKEYCODE_F3                 = 133,
    AKEYCODE_F4                 = 134,
    AKEYCODE_F5                 = 135,
    AKEYCODE_F6                 = 136,
    AKEYCODE_F7                 = 137,
    AKEYCODE_F8                 = 138,
    AKEYCODE_F9                 = 139,
    AKEYCODE_F10                = 140,
    AKEYCODE_F11                = 141,
    AKEYCODE_F12                = 142,
    AKEYCODE_NUM_LOCK           = 143,
    AKEYCODE_NUMPAD_0           = 144,
    AKEYCODE_NUMPAD_1           = 145,
    AKEYCODE_NUMPAD_2           = 146,
    AKEYCODE_NUMPAD_3           = 147,
    AKEYCODE_NUMPAD_4           = 148,
    AKEYCODE_NUMPAD_5           = 149,
    AKEYCODE_NUMPAD_6           = 150,
    AKEYCODE_NUMPAD_7           = 151,
    AKEYCODE_NUMPAD_8           = 152,
    AKEYCODE_NUMPAD_9           = 153,
    AKEYCODE_NUMPAD_DIVIDE      = 154,
    AKEYCODE_NUMPAD_MULTIPLY    = 155,
    AKEYCODE_NUMPAD_SUBTRACT    = 156,
    AKEYCODE_NUMPAD_ADD         = 157,
    AKEYCODE_NUMPAD_DOT         = 158,
    AKEYCODE_NUMPAD_COMMA       = 159,
    AKEYCODE_NUMPAD_ENTER       = 160,
    AKEYCODE_NUMPAD_EQUALS      = 161,
    AKEYCODE_NUMPAD_LEFT_PAREN  = 162,
    AKEYCODE_NUMPAD_RIGHT_PAREN = 163,
    AKEYCODE_VOLUME_MUTE        = 164,
    AKEYCODE_INFO               = 165,
    AKEYCODE_CHANNEL_UP         = 166,
    AKEYCODE_CHANNEL_DOWN       = 167,
    AKEYCODE_ZOOM_IN            = 168,
    AKEYCODE_ZOOM_OUT           = 169,
    AKEYCODE_TV                 = 170,
    AKEYCODE_WINDOW             = 171,
    AKEYCODE_GUIDE              = 172,
    AKEYCODE_DVR                = 173,
    AKEYCODE_BOOKMARK           = 174,
    AKEYCODE_CAPTIONS           = 175,
    AKEYCODE_SETTINGS           = 176,
    AKEYCODE_TV_POWER           = 177,
    AKEYCODE_TV_INPUT           = 178,
    AKEYCODE_STB_POWER          = 179,
    AKEYCODE_STB_INPUT          = 180,
    AKEYCODE_AVR_POWER          = 181,
    AKEYCODE_AVR_INPUT          = 182,
    AKEYCODE_PROG_RED           = 183,
    AKEYCODE_PROG_GREEN         = 184,
    AKEYCODE_PROG_YELLOW        = 185,
    AKEYCODE_PROG_BLUE          = 186,
    AKEYCODE_APP_SWITCH         = 187,
    AKEYCODE_BUTTON_1           = 188,
    AKEYCODE_BUTTON_2           = 189,
    AKEYCODE_BUTTON_3           = 190,
    AKEYCODE_BUTTON_4           = 191,
    AKEYCODE_BUTTON_5           = 192,
    AKEYCODE_BUTTON_6           = 193,
    AKEYCODE_BUTTON_7           = 194,
    AKEYCODE_BUTTON_8           = 195,
    AKEYCODE_BUTTON_9           = 196,
    AKEYCODE_BUTTON_10          = 197,
    AKEYCODE_BUTTON_11          = 198,
    AKEYCODE_BUTTON_12          = 199,
    AKEYCODE_BUTTON_13          = 200,
    AKEYCODE_BUTTON_14          = 201,
    AKEYCODE_BUTTON_15          = 202,
    AKEYCODE_BUTTON_16          = 203,
#endif
#if __ANDROID_API__ < 14
    AKEYCODE_LANGUAGE_SWITCH    = 204,
    AKEYCODE_MANNER_MODE        = 205,
    AKEYCODE_3D_MODE            = 206,
#endif
#if __ANDROID_API__ < 15
    AKEYCODE_CONTACTS           = 207,
    AKEYCODE_CALENDAR           = 208,
    AKEYCODE_MUSIC              = 209,
    AKEYCODE_CALCULATOR         = 210,
#endif
#if __ANDROID_API__ < 16
    AKEYCODE_ZENKAKU_HANKAKU    = 211,
    AKEYCODE_EISU               = 212,
    AKEYCODE_MUHENKAN           = 213,
    AKEYCODE_HENKAN             = 214,
    AKEYCODE_KATAKANA_HIRAGANA  = 215,
    AKEYCODE_YEN                = 216,
    AKEYCODE_RO                 = 217,
    AKEYCODE_KANA               = 218,
    AKEYCODE_ASSIST             = 219,
#endif

    AMETA_FUNCTION_ON           = 0x00000008,
    AMETA_CTRL_ON               = 0x00001000,
    AMETA_CTRL_LEFT_ON          = 0x00002000,
    AMETA_CTRL_RIGHT_ON         = 0x00004000,
    AMETA_META_ON               = 0x00010000,
    AMETA_META_LEFT_ON          = 0x00020000,
    AMETA_META_RIGHT_ON         = 0x00040000,
    AMETA_CAPS_LOCK_ON          = 0x00100000,
    AMETA_NUM_LOCK_ON           = 0x00200000,
    AMETA_SCROLL_LOCK_ON        = 0x00400000,

    AMETA_ALT_MASK              = AMETA_ALT_LEFT_ON   | AMETA_ALT_RIGHT_ON   | AMETA_ALT_ON,
    AMETA_CTRL_MASK             = AMETA_CTRL_LEFT_ON  | AMETA_CTRL_RIGHT_ON  | AMETA_CTRL_ON,
    AMETA_META_MASK             = AMETA_META_LEFT_ON  | AMETA_META_RIGHT_ON  | AMETA_META_ON,
    AMETA_SHIFT_MASK            = AMETA_SHIFT_LEFT_ON | AMETA_SHIFT_RIGHT_ON | AMETA_SHIFT_ON,
};

class AndroidMotionEvent
{
public:
    enum {
        ACTION_DOWN = 0,
        ACTION_UP = 1,
        ACTION_MOVE = 2,
        ACTION_CANCEL = 3,
        ACTION_OUTSIDE = 4,
        ACTION_POINTER_DOWN = 5,
        ACTION_POINTER_UP = 6,
        ACTION_HOVER_MOVE = 7,
        ACTION_HOVER_ENTER = 9,
        ACTION_HOVER_EXIT = 10,
        ACTION_MAGNIFY_START = 11,
        ACTION_MAGNIFY = 12,
        ACTION_MAGNIFY_END = 13,
        EDGE_TOP = 0x00000001,
        EDGE_BOTTOM = 0x00000002,
        EDGE_LEFT = 0x00000004,
        EDGE_RIGHT = 0x00000008,
        SAMPLE_X = 0,
        SAMPLE_Y = 1,
        SAMPLE_PRESSURE = 2,
        SAMPLE_SIZE = 3,
        NUM_SAMPLE_DATA = 4,
        TOOL_TYPE_UNKNOWN = 0,
        TOOL_TYPE_FINGER = 1,
        TOOL_TYPE_STYLUS = 2,
        TOOL_TYPE_MOUSE = 3,
        TOOL_TYPE_ERASER = 4,
        dummy_java_enum_list_end
    };
};

class nsJNIString : public nsString
{
public:
    nsJNIString(jstring jstr, JNIEnv *jenv);
};

class nsJNICString : public nsCString
{
public:
    nsJNICString(jstring jstr, JNIEnv *jenv);
};

}

#endif
