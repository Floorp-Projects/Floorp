/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef AndroidJavaWrappers_h__
#define AndroidJavaWrappers_h__

#include <jni.h>
#include <android/input.h>
#include <android/log.h>

#include "nsGeoPosition.h"
#include "nsPoint.h"
#include "nsRect.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/gfx/Rect.h"
#include "mozilla/dom/Touch.h"
#include "InputData.h"
#include "Units.h"

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
class nsIWidget;

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
    float GetOffsetX(JNIEnv *env);
    float GetOffsetY(JNIEnv *env);

private:
    static jclass jViewTransformClass;
    static jfieldID jXField;
    static jfieldID jYField;
    static jfieldID jScaleField;
    static jfieldID jFixedLayerMarginLeft;
    static jfieldID jFixedLayerMarginTop;
    static jfieldID jFixedLayerMarginRight;
    static jfieldID jFixedLayerMarginBottom;
    static jfieldID jOffsetXField;
    static jfieldID jOffsetYField;
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

    void SetFirstPaintViewport(const LayerIntPoint& aOffset, float aZoom, const LayerIntRect& aPageRect, const CSSRect& aCssPageRect);
    void SetPageRect(const CSSRect& aCssPageRect);
    void SyncViewportInfo(const LayerIntRect& aDisplayPort, float aDisplayResolution, bool aLayersUpdated,
                          ScreenPoint& aScrollOffset, float& aScaleX, float& aScaleY,
                          gfx::Margin& aFixedLayerMargins, gfx::Point& aOffset);
    void SyncFrameMetrics(const gfx::Point& aScrollOffset, float aZoom, const CSSRect& aCssPageRect,
                          bool aLayersUpdated, const gfx::Rect& aDisplayPort, float aDisplayResolution,
                          bool aIsFirstPaint, gfx::Margin& aFixedLayerMargins, gfx::Point& aOffset);
    bool ProgressiveUpdateCallback(bool aHasPendingNewThebesContent, const gfx::Rect& aDisplayPort, float aDisplayResolution, bool aDrawingCritical, gfx::Rect& aViewport, float& aScaleX, float& aScaleY);
    bool CreateFrame(AutoLocalJNIFrame *jniFrame, AndroidLayerRendererFrame& aFrame);
    bool ActivateProgram(AutoLocalJNIFrame *jniFrame);
    bool DeactivateProgram(AutoLocalJNIFrame *jniFrame);
    void GetDisplayPort(AutoLocalJNIFrame *jniFrame, bool aPageSizeUpdate, bool aIsBrowserContentDisplayed, int32_t tabId, nsIAndroidViewport* metrics, nsIAndroidDisplayport** displayPort);
    void ContentDocumentChanged(AutoLocalJNIFrame *jniFrame);
    bool IsContentDocumentDisplayed(AutoLocalJNIFrame *jniFrame);

protected:
    static jclass jGeckoLayerClientClass;
    static jmethodID jSetFirstPaintViewport;
    static jmethodID jSetPageRect;
    static jmethodID jSyncViewportInfoMethod;
    static jmethodID jSyncFrameMetricsMethod;
    static jmethodID jCreateFrameMethod;
    static jmethodID jActivateProgramMethod;
    static jmethodID jDeactivateProgramMethod;
    static jmethodID jGetDisplayPort;
    static jmethodID jContentDocumentChanged;
    static jmethodID jIsContentDocumentDisplayed;
    static jmethodID jProgressiveUpdateCallbackMethod;

public:
    static jclass jViewportClass;
    static jclass jDisplayportClass;
    static jmethodID jViewportCtor;
    static jfieldID jDisplayportPosition;
    static jfieldID jDisplayportResolution;
};

enum {
    // These keycode masks are not defined in android/keycodes.h:
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
    AKEYCODE_LANGUAGE_SWITCH    = 204,
    AKEYCODE_MANNER_MODE        = 205,
    AKEYCODE_3D_MODE            = 206,
    AKEYCODE_CONTACTS           = 207,
    AKEYCODE_CALENDAR           = 208,
    AKEYCODE_MUSIC              = 209,
    AKEYCODE_CALCULATOR         = 210,
    AKEYCODE_ZENKAKU_HANKAKU    = 211,
    AKEYCODE_EISU               = 212,
    AKEYCODE_MUHENKAN           = 213,
    AKEYCODE_HENKAN             = 214,
    AKEYCODE_KATAKANA_HIRAGANA  = 215,
    AKEYCODE_YEN                = 216,
    AKEYCODE_RO                 = 217,
    AKEYCODE_KANA               = 218,
    AKEYCODE_ASSIST             = 219,

    AMETA_CTRL_ON               = 0x00001000,
    AMETA_CTRL_LEFT_ON          = 0x00002000,
    AMETA_CTRL_RIGHT_ON         = 0x00004000,
    AMETA_META_ON               = 0x00010000,
    AMETA_META_LEFT_ON          = 0x00020000,
    AMETA_META_RIGHT_ON         = 0x00040000,

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
private:
    AndroidGeckoEvent() {
    }

    void Init(JNIEnv *jenv, jobject jobj);
    void Init(int aType);
    void Init(AndroidGeckoEvent *aResizeEvent);

public:
    static void InitGeckoEventClass(JNIEnv *jEnv);

    static AndroidGeckoEvent* MakeNativePoke() {
        AndroidGeckoEvent *event = new AndroidGeckoEvent();
        event->Init(NATIVE_POKE);
        return event;
    }

    static AndroidGeckoEvent* MakeIMEEvent(int aAction) {
        AndroidGeckoEvent *event = new AndroidGeckoEvent();
        event->Init(IME_EVENT);
        event->mAction = aAction;
        return event;
    }

    static AndroidGeckoEvent* MakeDrawEvent(const nsIntRect& aRect) {
        AndroidGeckoEvent *event = new AndroidGeckoEvent();
        event->Init(DRAW);
        event->mRect = aRect;
        return event;
    }

    static AndroidGeckoEvent* MakeFromJavaObject(JNIEnv *jenv, jobject jobj) {
        AndroidGeckoEvent *event = new AndroidGeckoEvent();
        event->Init(jenv, jobj);
        return event;
    }

    static AndroidGeckoEvent* CopyResizeEvent(AndroidGeckoEvent *aResizeEvent) {
        AndroidGeckoEvent *event = new AndroidGeckoEvent();
        event->Init(aResizeEvent);
        return event;
    }

    static AndroidGeckoEvent* MakeBroadcastEvent(const nsCString& topic, const nsCString& data) {
        AndroidGeckoEvent* event = new AndroidGeckoEvent();
        event->Init(BROADCAST);
        CopyUTF8toUTF16(topic, event->mCharacters);
        CopyUTF8toUTF16(data, event->mCharactersExtra);
        return event;
    }

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
    bool IsAltPressed() const { return (mMetaState & AMETA_ALT_MASK) != 0; }
    bool IsShiftPressed() const { return (mMetaState & AMETA_SHIFT_MASK) != 0; }
    bool IsCtrlPressed() const { return (mMetaState & AMETA_CTRL_MASK) != 0; }
    bool IsMetaPressed() const { return (mMetaState & AMETA_META_MASK) != 0; }
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
    nsTouchEvent MakeTouchEvent(nsIWidget* widget);
    MultiTouchInput MakeMultiTouchInput(nsIWidget* widget);
    void UnionRect(nsIntRect const& aRect);

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
