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
#if __ANDROID_API__ < 18
    AKEYCODE_BRIGHTNESS_DOWN    = 220,
    AKEYCODE_BRIGHTNESS_UP      = 221,
#endif
#if __ANDROID_API__ < 19
    AKEYCODE_MEDIA_AUDIO_TRACK  = 222,
#endif
#if __ANDROID_API__ < 20
    AKEYCODE_SLEEP              = 223,
    AKEYCODE_WAKEUP             = 224,
#endif
#if __ANDROID_API__ < 21
    AKEYCODE_PAIRING                       = 225,
    AKEYCODE_MEDIA_TOP_MENU                = 226,
    AKEYCODE_11                            = 227,
    AKEYCODE_12                            = 228,
    AKEYCODE_LAST_CHANNEL                  = 229,
    AKEYCODE_TV_DATA_SERVICE               = 230,
    AKEYCODE_VOICE_ASSIST                  = 231,
    AKEYCODE_TV_RADIO_SERVICE              = 232,
    AKEYCODE_TV_TELETEXT                   = 233,
    AKEYCODE_TV_NUMBER_ENTRY               = 234,
    AKEYCODE_TV_TERRESTRIAL_ANALOG         = 235,
    AKEYCODE_TV_TERRESTRIAL_DIGITAL        = 236,
    AKEYCODE_TV_SATELLITE                  = 237,
    AKEYCODE_TV_SATELLITE_BS               = 238,
    AKEYCODE_TV_SATELLITE_CS               = 239,
    AKEYCODE_TV_SATELLITE_SERVICE          = 240,
    AKEYCODE_TV_NETWORK                    = 241,
    AKEYCODE_TV_ANTENNA_CABLE              = 242,
    AKEYCODE_TV_INPUT_HDMI_1               = 243,
    AKEYCODE_TV_INPUT_HDMI_2               = 244,
    AKEYCODE_TV_INPUT_HDMI_3               = 245,
    AKEYCODE_TV_INPUT_HDMI_4               = 246,
    AKEYCODE_TV_INPUT_COMPOSITE_1          = 247,
    AKEYCODE_TV_INPUT_COMPOSITE_2          = 248,
    AKEYCODE_TV_INPUT_COMPONENT_1          = 249,
    AKEYCODE_TV_INPUT_COMPONENT_2          = 250,
    AKEYCODE_TV_INPUT_VGA_1                = 251,
    AKEYCODE_TV_AUDIO_DESCRIPTION          = 252,
    AKEYCODE_TV_AUDIO_DESCRIPTION_MIX_UP   = 253,
    AKEYCODE_TV_AUDIO_DESCRIPTION_MIX_DOWN = 254,
    AKEYCODE_TV_ZOOM_MODE                  = 255,
    AKEYCODE_TV_CONTENTS_MENU              = 256,
    AKEYCODE_TV_MEDIA_CONTEXT_MENU         = 257,
    AKEYCODE_TV_TIMER_PROGRAMMING          = 258,
    AKEYCODE_HELP                          = 259,
#endif
#if __ANDROID_API__ < 23
    AKEYCODE_NAVIGATE_PREVIOUS  = 260,
    AKEYCODE_NAVIGATE_NEXT      = 261,
    AKEYCODE_NAVIGATE_IN        = 262,
    AKEYCODE_NAVIGATE_OUT       = 263,
#endif
#if __ANDROID_API__ < 24
    AKEYCODE_STEM_PRIMARY       = 264,
    AKEYCODE_STEM_1             = 265,
    AKEYCODE_STEM_2             = 266,
    AKEYCODE_STEM_3             = 267,
    AKEYCODE_DPAD_UP_LEFT       = 268,
    AKEYCODE_DPAD_DOWN_LEFT     = 269,
    AKEYCODE_DPAD_UP_RIGHT      = 270,
    AKEYCODE_DPAD_DOWN_RIGHT    = 271,
#endif
#if __ANDROID_API__ < 23
    AKEYCODE_MEDIA_SKIP_FORWARD  = 272,
    AKEYCODE_MEDIA_SKIP_BACKWARD = 273,
    AKEYCODE_MEDIA_STEP_FORWARD  = 274,
    AKEYCODE_MEDIA_STEP_BACKWARD = 275,
#endif
#if __ANDROID_API__ < 24
    AKEYCODE_SOFT_SLEEP         = 276,
    AKEYCODE_CUT                = 277,
    AKEYCODE_COPY               = 278,
    AKEYCODE_PASTE              = 279,
#endif
#if __ANDROID_API__ < 25
    AKEYCODE_SYSTEM_NAVIGATION_UP    = 280,
    AKEYCODE_SYSTEM_NAVIGATION_DOWN  = 281,
    AKEYCODE_SYSTEM_NAVIGATION_LEFT  = 282,
    AKEYCODE_SYSTEM_NAVIGATION_RIGHT = 283,
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
