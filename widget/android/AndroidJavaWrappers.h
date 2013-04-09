/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidJavaWrappers_h__
#define AndroidJavaWrappers_h__

#include <jni.h>
#include <android/log.h>

#include "nsGeoPosition.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/gfx/Rect.h"

//#define FORCE_ALOG 1

#ifndef ALOG
#if defined(DEBUG) || defined(FORCE_ALOG)
#define ALOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gecko" , ## args)
#else
#define ALOG(args...) ((void)0)
#endif
#endif

class nsIAndroidDisplayport;
class nsIAndroidViewport;

namespace mozilla {

class AndroidGeckoLayerClient;
class AutoLocalJNIFrame;

void InitAndroidJavaWrappers(JNIEnv *jEnv);

/*
 * Note: do not store global refs to any WrappedJavaObject;
 * these are live only during a particular JNI method, as
 * NewGlobalRef is -not- called on the jobject.
 *
 * If this is needed, WrappedJavaObject can be extended to
 * handle it.
 */

class RefCountedJavaObject {
public:
    RefCountedJavaObject(JNIEnv* env, jobject obj) : mRefCnt(0), mObject(env->NewGlobalRef(obj)) {}

    ~RefCountedJavaObject();

    int32_t AddRef() { return ++mRefCnt; }

    int32_t Release() {
        int32_t refcnt = --mRefCnt;
        if (refcnt == 0)
            delete this;
        return refcnt;
    }

    jobject GetObject() { return mObject; }
private:
    int32_t mRefCnt;
    jobject mObject;
};

class WrappedJavaObject {
public:
    WrappedJavaObject() :
        wrapped_obj(0)
    { }

    WrappedJavaObject(jobject jobj) {
        Init(jobj);
    }

    void Init(jobject jobj) {
        wrapped_obj = jobj;
    }

    bool isNull() const {
        return wrapped_obj == 0;
    }

    jobject wrappedObject() const {
        return wrapped_obj;
    }

protected:
    jobject wrapped_obj;
};

class AndroidPoint : public WrappedJavaObject
{
public:
    static void InitPointClass(JNIEnv *jEnv);

    AndroidPoint() { }
    AndroidPoint(JNIEnv *jenv, jobject jobj) {
        Init(jenv, jobj);
    }

    void Init(JNIEnv *jenv, jobject jobj);

    int X() { return mX; }
    int Y() { return mY; }

protected:
    int mX;
    int mY;

    static jclass jPointClass;
    static jfieldID jXField;
    static jfieldID jYField;
};

class AndroidRect : public WrappedJavaObject
{
public:
    static void InitRectClass(JNIEnv *jEnv);

    AndroidRect() { }
    AndroidRect(JNIEnv *jenv, jobject jobj) {
        Init(jenv, jobj);
    }

    void Init(JNIEnv *jenv, jobject jobj);

    int Bottom() { return mBottom; }
    int Left() { return mLeft; }
    int Right() { return mRight; }
    int Top() { return mTop; }
    int Width() { return mRight - mLeft; }
    int Height() { return mBottom - mTop; }

protected:
    int mBottom;
    int mLeft;
    int mRight;
    int mTop;

    static jclass jRectClass;
    static jfieldID jBottomField;
    static jfieldID jLeftField;
    static jfieldID jRightField;
    static jfieldID jTopField;
};

class AndroidRectF : public WrappedJavaObject
{
public:
    static void InitRectFClass(JNIEnv *jEnv);

    AndroidRectF() { }
    AndroidRectF(JNIEnv *jenv, jobject jobj) {
        Init(jenv, jobj);
    }

    void Init(JNIEnv *jenv, jobject jobj);

    float Bottom() { return mBottom; }
    float Left() { return mLeft; }
    float Right() { return mRight; }
    float Top() { return mTop; }
    float Width() { return mRight - mLeft; }
    float Height() { return mBottom - mTop; }

protected:
    float mBottom;
    float mLeft;
    float mRight;
    float mTop;

    static jclass jRectClass;
    static jfieldID jBottomField;
    static jfieldID jLeftField;
    static jfieldID jRightField;
    static jfieldID jTopField;
};

class AndroidViewTransform : public WrappedJavaObject {
public:
    static void InitViewTransformClass(JNIEnv *jEnv);

    void Init(jobject jobj);

    AndroidViewTransform() {}
    AndroidViewTransform(jobject jobj) { Init(jobj); }

    float GetX(JNIEnv *env);
    float GetY(JNIEnv *env);
    float GetScale(JNIEnv *env);
    void GetFixedLayerMargins(JNIEnv *env, gfx::Margin &aFixedLayerMargins);

private:
    static jclass jViewTransformClass;
    static jfieldID jXField;
    static jfieldID jYField;
    static jfieldID jScaleField;
    static jfieldID jFixedLayerMarginLeft;
    static jfieldID jFixedLayerMarginTop;
    static jfieldID jFixedLayerMarginRight;
    static jfieldID jFixedLayerMarginBottom;
};

class AndroidProgressiveUpdateData : public WrappedJavaObject {
public:
    static void InitProgressiveUpdateDataClass(JNIEnv *jEnv);

    void Init(jobject jobj);

    AndroidProgressiveUpdateData() {}
    AndroidProgressiveUpdateData(jobject jobj) { Init(jobj); }

    float GetX(JNIEnv *env);
    float GetY(JNIEnv *env);
    float GetWidth(JNIEnv *env);
    float GetHeight(JNIEnv *env);
    float GetScale(JNIEnv *env);
    bool GetShouldAbort(JNIEnv *env);

private:
    static jclass jProgressiveUpdateDataClass;
    static jfieldID jXField;
    static jfieldID jYField;
    static jfieldID jWidthField;
    static jfieldID jHeightField;
    static jfieldID jScaleField;
    static jfieldID jShouldAbortField;
};

class AndroidLayerRendererFrame : public WrappedJavaObject {
public:
    static void InitLayerRendererFrameClass(JNIEnv *jEnv);

    void Init(JNIEnv *env, jobject jobj);
    void Dispose(JNIEnv *env);

    bool BeginDrawing(AutoLocalJNIFrame *jniFrame);
    bool DrawBackground(AutoLocalJNIFrame *jniFrame);
    bool DrawForeground(AutoLocalJNIFrame *jniFrame);
    bool EndDrawing(AutoLocalJNIFrame *jniFrame);

private:
    static jclass jLayerRendererFrameClass;
    static jmethodID jBeginDrawingMethod;
    static jmethodID jDrawBackgroundMethod;
    static jmethodID jDrawForegroundMethod;
    static jmethodID jEndDrawingMethod;
};

class AndroidGeckoLayerClient : public WrappedJavaObject {
public:
    static void InitGeckoLayerClientClass(JNIEnv *jEnv);

    void Init(jobject jobj);

    AndroidGeckoLayerClient() {}
    AndroidGeckoLayerClient(jobject jobj) { Init(jobj); }

    void SetFirstPaintViewport(const nsIntPoint& aOffset, float aZoom, const nsIntRect& aPageRect, const gfx::Rect& aCssPageRect);
    void SetPageRect(const gfx::Rect& aCssPageRect);
    void SyncViewportInfo(const nsIntRect& aDisplayPort, float aDisplayResolution, bool aLayersUpdated,
                          nsIntPoint& aScrollOffset, float& aScaleX, float& aScaleY,
                          gfx::Margin& aFixedLayerMargins);
    bool ProgressiveUpdateCallback(bool aHasPendingNewThebesContent, const gfx::Rect& aDisplayPort, float aDisplayResolution, bool aDrawingCritical, gfx::Rect& aViewport, float& aScaleX, float& aScaleY);
    bool CreateFrame(AutoLocalJNIFrame *jniFrame, AndroidLayerRendererFrame& aFrame);
    bool ActivateProgram(AutoLocalJNIFrame *jniFrame);
    bool DeactivateProgram(AutoLocalJNIFrame *jniFrame);
    void GetDisplayPort(AutoLocalJNIFrame *jniFrame, bool aPageSizeUpdate, bool aIsBrowserContentDisplayed, int32_t tabId, nsIAndroidViewport* metrics, nsIAndroidDisplayport** displayPort);

protected:
    static jclass jGeckoLayerClientClass;
    static jmethodID jSetFirstPaintViewport;
    static jmethodID jSetPageRect;
    static jmethodID jSyncViewportInfoMethod;
    static jmethodID jCreateFrameMethod;
    static jmethodID jActivateProgramMethod;
    static jmethodID jDeactivateProgramMethod;
    static jmethodID jGetDisplayPort;
    static jmethodID jProgressiveUpdateCallbackMethod;

public:
    static jclass jViewportClass;
    static jclass jDisplayportClass;
    static jmethodID jViewportCtor;
    static jfieldID jDisplayportPosition;
    static jfieldID jDisplayportResolution;
};

class AndroidKeyEvent
{
public:
    enum {
        KEYCODE_UNKNOWN            = 0,
        KEYCODE_SOFT_LEFT          = 1,
        KEYCODE_SOFT_RIGHT         = 2,
        KEYCODE_HOME               = 3,
        KEYCODE_BACK               = 4,
        KEYCODE_CALL               = 5,
        KEYCODE_ENDCALL            = 6,
        KEYCODE_0                  = 7,
        KEYCODE_1                  = 8,
        KEYCODE_2                  = 9,
        KEYCODE_3                  = 10,
        KEYCODE_4                  = 11,
        KEYCODE_5                  = 12,
        KEYCODE_6                  = 13,
        KEYCODE_7                  = 14,
        KEYCODE_8                  = 15,
        KEYCODE_9                  = 16,
        KEYCODE_STAR               = 17,
        KEYCODE_POUND              = 18,
        KEYCODE_DPAD_UP            = 19,
        KEYCODE_DPAD_DOWN          = 20,
        KEYCODE_DPAD_LEFT          = 21,
        KEYCODE_DPAD_RIGHT         = 22,
        KEYCODE_DPAD_CENTER        = 23,
        KEYCODE_VOLUME_UP          = 24,
        KEYCODE_VOLUME_DOWN        = 25,
        KEYCODE_POWER              = 26,
        KEYCODE_CAMERA             = 27,
        KEYCODE_CLEAR              = 28,
        KEYCODE_A                  = 29,
        KEYCODE_B                  = 30,
        KEYCODE_C                  = 31,
        KEYCODE_D                  = 32,
        KEYCODE_E                  = 33,
        KEYCODE_F                  = 34,
        KEYCODE_G                  = 35,
        KEYCODE_H                  = 36,
        KEYCODE_I                  = 37,
        KEYCODE_J                  = 38,
        KEYCODE_K                  = 39,
        KEYCODE_L                  = 40,
        KEYCODE_M                  = 41,
        KEYCODE_N                  = 42,
        KEYCODE_O                  = 43,
        KEYCODE_P                  = 44,
        KEYCODE_Q                  = 45,
        KEYCODE_R                  = 46,
        KEYCODE_S                  = 47,
        KEYCODE_T                  = 48,
        KEYCODE_U                  = 49,
        KEYCODE_V                  = 50,
        KEYCODE_W                  = 51,
        KEYCODE_X                  = 52,
        KEYCODE_Y                  = 53,
        KEYCODE_Z                  = 54,
        KEYCODE_COMMA              = 55,
        KEYCODE_PERIOD             = 56,
        KEYCODE_ALT_LEFT           = 57,
        KEYCODE_ALT_RIGHT          = 58,
        KEYCODE_SHIFT_LEFT         = 59,
        KEYCODE_SHIFT_RIGHT        = 60,
        KEYCODE_TAB                = 61,
        KEYCODE_SPACE              = 62,
        KEYCODE_SYM                = 63,
        KEYCODE_EXPLORER           = 64,
        KEYCODE_ENVELOPE           = 65,
        KEYCODE_ENTER              = 66,
        KEYCODE_DEL                = 67,
        KEYCODE_GRAVE              = 68,
        KEYCODE_MINUS              = 69,
        KEYCODE_EQUALS             = 70,
        KEYCODE_LEFT_BRACKET       = 71,
        KEYCODE_RIGHT_BRACKET      = 72,
        KEYCODE_BACKSLASH          = 73,
        KEYCODE_SEMICOLON          = 74,
        KEYCODE_APOSTROPHE         = 75,
        KEYCODE_SLASH              = 76,
        KEYCODE_AT                 = 77,
        KEYCODE_NUM                = 78,
        KEYCODE_HEADSETHOOK        = 79,
        KEYCODE_FOCUS              = 80,
        KEYCODE_PLUS               = 81,
        KEYCODE_MENU               = 82,
        KEYCODE_NOTIFICATION       = 83,
        KEYCODE_SEARCH             = 84,
        KEYCODE_MEDIA_PLAY_PAUSE   = 85,
        KEYCODE_MEDIA_STOP         = 86,
        KEYCODE_MEDIA_NEXT         = 87,
        KEYCODE_MEDIA_PREVIOUS     = 88,
        KEYCODE_MEDIA_REWIND       = 89,
        KEYCODE_MEDIA_FAST_FORWARD = 90,
        KEYCODE_MUTE               = 91,
        KEYCODE_PAGE_UP            = 92,
        KEYCODE_PAGE_DOWN          = 93,
        KEYCODE_PICTSYMBOLS        = 94,
        KEYCODE_SWITCH_CHARSET     = 95,
        KEYCODE_BUTTON_A           = 96,
        KEYCODE_BUTTON_B           = 97,
        KEYCODE_BUTTON_C           = 98,
        KEYCODE_BUTTON_X           = 99,
        KEYCODE_BUTTON_Y           = 100,
        KEYCODE_BUTTON_Z           = 101,
        KEYCODE_BUTTON_L1          = 102,
        KEYCODE_BUTTON_R1          = 103,
        KEYCODE_BUTTON_L2          = 104,
        KEYCODE_BUTTON_R2          = 105,
        KEYCODE_BUTTON_THUMBL      = 106,
        KEYCODE_BUTTON_THUMBR      = 107,
        KEYCODE_BUTTON_START       = 108,
        KEYCODE_BUTTON_SELECT      = 109,
        KEYCODE_BUTTON_MODE        = 110,
        KEYCODE_ESCAPE             = 111,
        KEYCODE_FORWARD_DEL        = 112,
        KEYCODE_CTRL_LEFT          = 113,
        KEYCODE_CTRL_RIGHT         = 114,
        KEYCODE_CAPS_LOCK          = 115,
        KEYCODE_SCROLL_LOCK        = 116,
        KEYCODE_META_LEFT          = 117,
        KEYCODE_META_RIGHT         = 118,
        KEYCODE_FUNCTION           = 119,
        KEYCODE_SYSRQ              = 120,
        KEYCODE_BREAK              = 121,
        KEYCODE_MOVE_HOME          = 122,
        KEYCODE_MOVE_END           = 123,
        KEYCODE_INSERT             = 124,
        KEYCODE_FORWARD            = 125,
        KEYCODE_MEDIA_PLAY         = 126,
        KEYCODE_MEDIA_PAUSE        = 127,
        KEYCODE_MEDIA_CLOSE        = 128,
        KEYCODE_MEDIA_EJECT        = 129,
        KEYCODE_MEDIA_RECORD       = 130,
        KEYCODE_F1                 = 131,
        KEYCODE_F2                 = 132,
        KEYCODE_F3                 = 133,
        KEYCODE_F4                 = 134,
        KEYCODE_F5                 = 135,
        KEYCODE_F6                 = 136,
        KEYCODE_F7                 = 137,
        KEYCODE_F8                 = 138,
        KEYCODE_F9                 = 139,
        KEYCODE_F10                = 140,
        KEYCODE_F11                = 141,
        KEYCODE_F12                = 142,
        KEYCODE_NUM_LOCK           = 143,
        KEYCODE_NUMPAD_0           = 144,
        KEYCODE_NUMPAD_1           = 145,
        KEYCODE_NUMPAD_2           = 146,
        KEYCODE_NUMPAD_3           = 147,
        KEYCODE_NUMPAD_4           = 148,
        KEYCODE_NUMPAD_5           = 149,
        KEYCODE_NUMPAD_6           = 150,
        KEYCODE_NUMPAD_7           = 151,
        KEYCODE_NUMPAD_8           = 152,
        KEYCODE_NUMPAD_9           = 153,
        KEYCODE_NUMPAD_DIVIDE      = 154,
        KEYCODE_NUMPAD_MULTIPLY    = 155,
        KEYCODE_NUMPAD_SUBTRACT    = 156,
        KEYCODE_NUMPAD_ADD         = 157,
        KEYCODE_NUMPAD_DOT         = 158,
        KEYCODE_NUMPAD_COMMA       = 159,
        KEYCODE_NUMPAD_ENTER       = 160,
        KEYCODE_NUMPAD_EQUALS      = 161,
        KEYCODE_NUMPAD_LEFT_PAREN  = 162,
        KEYCODE_NUMPAD_RIGHT_PAREN = 163,
        KEYCODE_VOLUME_MUTE        = 164,
        KEYCODE_INFO               = 165,
        KEYCODE_CHANNEL_UP         = 166,
        KEYCODE_CHANNEL_DOWN       = 167,
        KEYCODE_ZOOM_IN            = 168,
        KEYCODE_ZOOM_OUT           = 169,
        KEYCODE_TV                 = 170,
        KEYCODE_WINDOW             = 171,
        KEYCODE_GUIDE              = 172,
        KEYCODE_DVR                = 173,
        KEYCODE_BOOKMARK           = 174,
        KEYCODE_CAPTIONS           = 175,
        KEYCODE_SETTINGS           = 176,
        KEYCODE_TV_POWER           = 177,
        KEYCODE_TV_INPUT           = 178,
        KEYCODE_STB_POWER          = 179,
        KEYCODE_STB_INPUT          = 180,
        KEYCODE_AVR_POWER          = 181,
        KEYCODE_AVR_INPUT          = 182,
        KEYCODE_PROG_RED           = 183,
        KEYCODE_PROG_GREEN         = 184,
        KEYCODE_PROG_YELLOW        = 185,
        KEYCODE_PROG_BLUE          = 186,
        KEYCODE_APP_SWITCH         = 187,
        KEYCODE_BUTTON_1           = 188,
        KEYCODE_BUTTON_2           = 189,
        KEYCODE_BUTTON_3           = 190,
        KEYCODE_BUTTON_4           = 191,
        KEYCODE_BUTTON_5           = 192,
        KEYCODE_BUTTON_6           = 193,
        KEYCODE_BUTTON_7           = 194,
        KEYCODE_BUTTON_8           = 195,
        KEYCODE_BUTTON_9           = 196,
        KEYCODE_BUTTON_10          = 197,
        KEYCODE_BUTTON_11          = 198,
        KEYCODE_BUTTON_12          = 199,
        KEYCODE_BUTTON_13          = 200,
        KEYCODE_BUTTON_14          = 201,
        KEYCODE_BUTTON_15          = 202,
        KEYCODE_BUTTON_16          = 203,
        KEYCODE_LANGUAGE_SWITCH    = 204,
        KEYCODE_MANNER_MODE        = 205,
        KEYCODE_3D_MODE            = 206,
        KEYCODE_CONTACTS           = 207,
        KEYCODE_CALENDAR           = 208,
        KEYCODE_MUSIC              = 209,
        KEYCODE_CALCULATOR         = 210,

        ACTION_DOWN                = 0,
        ACTION_UP                  = 1,
        ACTION_MULTIPLE            = 2,
        META_ALT_ON                = 0x00000002,
        META_ALT_LEFT_ON           = 0x00000010,
        META_ALT_RIGHT_ON          = 0x00000020,
        META_ALT_MASK              = META_ALT_RIGHT_ON | META_ALT_LEFT_ON | META_ALT_ON,
        META_SHIFT_ON              = 0x00000001,
        META_SHIFT_LEFT_ON         = 0x00000040,
        META_SHIFT_RIGHT_ON        = 0x00000080,
        META_SHIFT_MASK            = META_SHIFT_RIGHT_ON | META_SHIFT_LEFT_ON | META_SHIFT_ON,
        META_CTRL_ON               = 0x00001000,
        META_CTRL_LEFT_ON          = 0x00002000,
        META_CTRL_RIGHT_ON         = 0x00004000,
        META_CTRL_MASK             = META_CTRL_RIGHT_ON | META_CTRL_LEFT_ON | META_CTRL_ON,
        META_META_ON               = 0x00010000,
        META_META_LEFT_ON          = 0x00020000,
        META_META_RIGHT_ON         = 0x00040000,
        META_META_MASK             = META_META_RIGHT_ON | META_META_LEFT_ON | META_META_ON,
        META_SYM_ON                = 0x00000004,
        FLAG_WOKE_HERE             = 0x00000001,
        FLAG_SOFT_KEYBOARD         = 0x00000002,
        FLAG_KEEP_TOUCH_MODE       = 0x00000004,
        FLAG_FROM_SYSTEM           = 0x00000008,
        FLAG_EDITOR_ACTION         = 0x00000010,
        FLAG_CANCELED              = 0x00000020,
        FLAG_VIRTUAL_HARD_KEY      = 0x00000040,
        FLAG_LONG_PRESS            = 0x00000080,
        FLAG_CANCELED_LONG_PRESS   = 0x00000100,
        FLAG_TRACKING              = 0x00000200,
        FLAG_START_TRACKING        = 0x40000000,
        dummy_java_enum_list_end
    };
};

class AndroidMotionEvent
{
public:
    enum {
        ACTION_MASK = 0xff,
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
        ACTION_POINTER_ID_MASK = 0xff00,
        ACTION_POINTER_ID_SHIFT = 8,
        EDGE_TOP = 0x00000001,
        EDGE_BOTTOM = 0x00000002,
        EDGE_LEFT = 0x00000004,
        EDGE_RIGHT = 0x00000008,
        SAMPLE_X = 0,
        SAMPLE_Y = 1,
        SAMPLE_PRESSURE = 2,
        SAMPLE_SIZE = 3,
        NUM_SAMPLE_DATA = 4,
        dummy_java_enum_list_end
    };
};

class AndroidLocation : public WrappedJavaObject
{
public:
    static void InitLocationClass(JNIEnv *jEnv);
    static nsGeoPosition* CreateGeoPosition(JNIEnv *jenv, jobject jobj);
    static jclass jLocationClass;
    static jmethodID jGetLatitudeMethod;
    static jmethodID jGetLongitudeMethod;
    static jmethodID jGetAltitudeMethod;
    static jmethodID jGetAccuracyMethod;
    static jmethodID jGetBearingMethod;
    static jmethodID jGetSpeedMethod;
    static jmethodID jGetTimeMethod;
};

class AndroidGeckoEvent : public WrappedJavaObject
{
public:
    static void InitGeckoEventClass(JNIEnv *jEnv);

    AndroidGeckoEvent(int aType) {
        Init(aType);
    }
    AndroidGeckoEvent(int aType, int aAction) {
        Init(aType, aAction);
    }
    AndroidGeckoEvent(int aType, const nsIntRect &aRect) {
        Init(aType, aRect);
    }
    AndroidGeckoEvent(JNIEnv *jenv, jobject jobj) {
        Init(jenv, jobj);
    }
    AndroidGeckoEvent(AndroidGeckoEvent *aResizeEvent) {
        Init(aResizeEvent);
    }

    void Init(JNIEnv *jenv, jobject jobj);
    void Init(int aType);
    void Init(int aType, int aAction);
    void Init(int aType, const nsIntRect &aRect);
    void Init(AndroidGeckoEvent *aResizeEvent);

    int Action() { return mAction; }
    int Type() { return mType; }
    bool AckNeeded() { return mAckNeeded; }
    int64_t Time() { return mTime; }
    const nsTArray<nsIntPoint>& Points() { return mPoints; }
    const nsTArray<int>& PointIndicies() { return mPointIndicies; }
    const nsTArray<float>& Pressures() { return mPressures; }
    const nsTArray<float>& Orientations() { return mOrientations; }
    const nsTArray<nsIntPoint>& PointRadii() { return mPointRadii; }
    double X() { return mX; }
    double Y() { return mY; }
    double Z() { return mZ; }
    const nsIntRect& Rect() { return mRect; }
    nsAString& Characters() { return mCharacters; }
    nsAString& CharactersExtra() { return mCharactersExtra; }
    int KeyCode() { return mKeyCode; }
    int MetaState() { return mMetaState; }
    uint32_t DomKeyLocation() { return mDomKeyLocation; }
    bool IsAltPressed() const { return (mMetaState & AndroidKeyEvent::META_ALT_MASK) != 0; }
    bool IsShiftPressed() const { return (mMetaState & AndroidKeyEvent::META_SHIFT_MASK) != 0; }
    bool IsCtrlPressed() const { return (mMetaState & AndroidKeyEvent::META_CTRL_MASK) != 0; }
    bool IsMetaPressed() const { return (mMetaState & AndroidKeyEvent::META_META_MASK) != 0; }
    int Flags() { return mFlags; }
    int UnicodeChar() { return mUnicodeChar; }
    int BaseUnicodeChar() { return mBaseUnicodeChar; }
    int RepeatCount() const { return mRepeatCount; }
    int Count() { return mCount; }
    int Start() { return mStart; }
    int End() { return mEnd; }
    int PointerIndex() { return mPointerIndex; }
    int RangeType() { return mRangeType; }
    int RangeStyles() { return mRangeStyles; }
    int RangeLineStyle() { return mRangeLineStyle; }
    bool RangeBoldLine() { return mRangeBoldLine; }
    int RangeForeColor() { return mRangeForeColor; }
    int RangeBackColor() { return mRangeBackColor; }
    int RangeLineColor() { return mRangeLineColor; }
    nsGeoPosition* GeoPosition() { return mGeoPosition; }
    double Bandwidth() { return mBandwidth; }
    bool CanBeMetered() { return mCanBeMetered; }
    short ScreenOrientation() { return mScreenOrientation; }
    RefCountedJavaObject* ByteBuffer() { return mByteBuffer; }
    int Width() { return mWidth; }
    int Height() { return mHeight; }

protected:
    int mAction;
    int mType;
    bool mAckNeeded;
    int64_t mTime;
    nsTArray<nsIntPoint> mPoints;
    nsTArray<nsIntPoint> mPointRadii;
    nsTArray<int> mPointIndicies;
    nsTArray<float> mOrientations;
    nsTArray<float> mPressures;
    nsIntRect mRect;
    int mFlags, mMetaState;
    uint32_t mDomKeyLocation;
    int mKeyCode, mUnicodeChar, mBaseUnicodeChar;
    int mRepeatCount;
    int mCount;
    int mStart, mEnd;
    int mRangeType, mRangeStyles, mRangeLineStyle;
    bool mRangeBoldLine;
    int mRangeForeColor, mRangeBackColor, mRangeLineColor;
    double mX, mY, mZ;
    int mPointerIndex;
    nsString mCharacters, mCharactersExtra;
    nsRefPtr<nsGeoPosition> mGeoPosition;
    double mBandwidth;
    bool mCanBeMetered;
    short mScreenOrientation;
    nsRefPtr<RefCountedJavaObject> mByteBuffer;
    int mWidth, mHeight;

    void ReadIntArray(nsTArray<int> &aVals,
                      JNIEnv *jenv,
                      jfieldID field,
                      int32_t count);
    void ReadFloatArray(nsTArray<float> &aVals,
                        JNIEnv *jenv,
                        jfieldID field,
                        int32_t count);
    void ReadPointArray(nsTArray<nsIntPoint> &mPoints,
                        JNIEnv *jenv,
                        jfieldID field,
                        int32_t count);
    void ReadRectField(JNIEnv *jenv);
    void ReadCharactersField(JNIEnv *jenv);
    void ReadCharactersExtraField(JNIEnv *jenv);

    uint32_t ReadDomKeyLocation(JNIEnv* jenv, jobject jGeckoEventObj);

    static jclass jGeckoEventClass;
    static jfieldID jActionField;
    static jfieldID jTypeField;
    static jfieldID jAckNeededField;
    static jfieldID jTimeField;
    static jfieldID jPoints;
    static jfieldID jPointIndicies;
    static jfieldID jOrientations;
    static jfieldID jPressures;
    static jfieldID jPointRadii;
    static jfieldID jXField;
    static jfieldID jYField;
    static jfieldID jZField;
    static jfieldID jDistanceField;
    static jfieldID jRectField;
    static jfieldID jNativeWindowField;

    static jfieldID jCharactersField;
    static jfieldID jCharactersExtraField;
    static jfieldID jKeyCodeField;
    static jfieldID jMetaStateField;
    static jfieldID jDomKeyLocationField;
    static jfieldID jFlagsField;
    static jfieldID jCountField;
    static jfieldID jStartField;
    static jfieldID jEndField;
    static jfieldID jPointerIndexField;
    static jfieldID jUnicodeCharField;
    static jfieldID jBaseUnicodeCharField;
    static jfieldID jRepeatCountField;
    static jfieldID jRangeTypeField;
    static jfieldID jRangeStylesField;
    static jfieldID jRangeLineStyleField;
    static jfieldID jRangeBoldLineField;
    static jfieldID jRangeForeColorField;
    static jfieldID jRangeBackColorField;
    static jfieldID jRangeLineColorField;
    static jfieldID jLocationField;

    static jfieldID jBandwidthField;
    static jfieldID jCanBeMeteredField;

    static jfieldID jScreenOrientationField;
    static jfieldID jByteBufferField;

    static jfieldID jWidthField;
    static jfieldID jHeightField;

    static jclass jDomKeyLocationClass;
    static jfieldID jDomKeyLocationValueField;

public:
    enum {
        NATIVE_POKE = 0,
        KEY_EVENT = 1,
        MOTION_EVENT = 2,
        SENSOR_EVENT = 3,
        LOCATION_EVENT = 5,
        IME_EVENT = 6,
        DRAW = 7,
        SIZE_CHANGED = 8,
        APP_BACKGROUNDING = 9,
        APP_FOREGROUNDING = 10,
        LOAD_URI = 12,
        NOOP = 15,
        FORCED_RESIZE = 16, // used internally in nsAppShell/nsWindow
        BROADCAST = 19,
        VIEWPORT = 20,
        VISITED = 21,
        NETWORK_CHANGED = 22,
        THUMBNAIL = 25,
        SCREENORIENTATION_CHANGED = 27,
        COMPOSITOR_CREATE = 28,
        COMPOSITOR_PAUSE = 29,
        COMPOSITOR_RESUME = 30,
        NATIVE_GESTURE_EVENT = 31,
        IME_KEY_EVENT = 32,
        dummy_java_enum_list_end
    };

    enum {
        // Internal Gecko events
        IME_FLUSH_CHANGES = -2,
        IME_UPDATE_CONTEXT = -1,
        // Events from Java to Gecko
        IME_SYNCHRONIZE = 0,
        IME_REPLACE_TEXT = 1,
        IME_SET_SELECTION = 2,
        IME_ADD_COMPOSITION_RANGE = 3,
        IME_UPDATE_COMPOSITION = 4,
        IME_REMOVE_COMPOSITION = 5,
        IME_ACKNOWLEDGE_FOCUS = 6,
        dummy_ime_enum_list_end
    };
};

class nsJNIString : public nsString
{
public:
    nsJNIString(jstring jstr, JNIEnv *jenv);
};

}

#endif
