/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidJavaWrappers.h"
#include "AndroidBridge.h"
#include "AndroidBridgeUtilities.h"
#include "nsIDOMKeyEvent.h"
#include "nsIWidget.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/TouchEvents.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::widget::android;

jclass AndroidGeckoEvent::jGeckoEventClass = 0;
jfieldID AndroidGeckoEvent::jActionField = 0;
jfieldID AndroidGeckoEvent::jTypeField = 0;
jfieldID AndroidGeckoEvent::jAckNeededField = 0;
jfieldID AndroidGeckoEvent::jTimeField = 0;
jfieldID AndroidGeckoEvent::jPoints = 0;
jfieldID AndroidGeckoEvent::jPointIndicies = 0;
jfieldID AndroidGeckoEvent::jPressures = 0;
jfieldID AndroidGeckoEvent::jPointRadii = 0;
jfieldID AndroidGeckoEvent::jOrientations = 0;
jfieldID AndroidGeckoEvent::jXField = 0;
jfieldID AndroidGeckoEvent::jYField = 0;
jfieldID AndroidGeckoEvent::jZField = 0;
jfieldID AndroidGeckoEvent::jDistanceField = 0;
jfieldID AndroidGeckoEvent::jRectField = 0;
jfieldID AndroidGeckoEvent::jNativeWindowField = 0;

jfieldID AndroidGeckoEvent::jCharactersField = 0;
jfieldID AndroidGeckoEvent::jCharactersExtraField = 0;
jfieldID AndroidGeckoEvent::jDataField = 0;
jfieldID AndroidGeckoEvent::jDOMPrintableKeyValueField = 0;
jfieldID AndroidGeckoEvent::jKeyCodeField = 0;
jfieldID AndroidGeckoEvent::jScanCodeField = 0;
jfieldID AndroidGeckoEvent::jMetaStateField = 0;
jfieldID AndroidGeckoEvent::jDomKeyLocationField = 0;
jfieldID AndroidGeckoEvent::jFlagsField = 0;
jfieldID AndroidGeckoEvent::jUnicodeCharField = 0;
jfieldID AndroidGeckoEvent::jBaseUnicodeCharField = 0;
jfieldID AndroidGeckoEvent::jRepeatCountField = 0;
jfieldID AndroidGeckoEvent::jCountField = 0;
jfieldID AndroidGeckoEvent::jStartField = 0;
jfieldID AndroidGeckoEvent::jEndField = 0;
jfieldID AndroidGeckoEvent::jPointerIndexField = 0;
jfieldID AndroidGeckoEvent::jRangeTypeField = 0;
jfieldID AndroidGeckoEvent::jRangeStylesField = 0;
jfieldID AndroidGeckoEvent::jRangeLineStyleField = 0;
jfieldID AndroidGeckoEvent::jRangeBoldLineField = 0;
jfieldID AndroidGeckoEvent::jRangeForeColorField = 0;
jfieldID AndroidGeckoEvent::jRangeBackColorField = 0;
jfieldID AndroidGeckoEvent::jRangeLineColorField = 0;
jfieldID AndroidGeckoEvent::jLocationField = 0;
jfieldID AndroidGeckoEvent::jConnectionTypeField = 0;
jfieldID AndroidGeckoEvent::jIsWifiField = 0;
jfieldID AndroidGeckoEvent::jDHCPGatewayField = 0;
jfieldID AndroidGeckoEvent::jScreenOrientationField = 0;
jfieldID AndroidGeckoEvent::jByteBufferField = 0;
jfieldID AndroidGeckoEvent::jWidthField = 0;
jfieldID AndroidGeckoEvent::jHeightField = 0;
jfieldID AndroidGeckoEvent::jIDField = 0;
jfieldID AndroidGeckoEvent::jGamepadButtonField = 0;
jfieldID AndroidGeckoEvent::jGamepadButtonPressedField = 0;
jfieldID AndroidGeckoEvent::jGamepadButtonValueField = 0;
jfieldID AndroidGeckoEvent::jGamepadValuesField = 0;
jfieldID AndroidGeckoEvent::jPrefNamesField = 0;

jclass AndroidGeckoEvent::jDomKeyLocationClass = 0;
jfieldID AndroidGeckoEvent::jDomKeyLocationValueField = 0;

jclass AndroidPoint::jPointClass = 0;
jfieldID AndroidPoint::jXField = 0;
jfieldID AndroidPoint::jYField = 0;

jclass AndroidRect::jRectClass = 0;
jfieldID AndroidRect::jBottomField = 0;
jfieldID AndroidRect::jLeftField = 0;
jfieldID AndroidRect::jRightField = 0;
jfieldID AndroidRect::jTopField = 0;

jclass AndroidRectF::jRectClass = 0;
jfieldID AndroidRectF::jBottomField = 0;
jfieldID AndroidRectF::jLeftField = 0;
jfieldID AndroidRectF::jRightField = 0;
jfieldID AndroidRectF::jTopField = 0;

jclass AndroidLocation::jLocationClass = 0;
jmethodID AndroidLocation::jGetLatitudeMethod = 0;
jmethodID AndroidLocation::jGetLongitudeMethod = 0;
jmethodID AndroidLocation::jGetAltitudeMethod = 0;
jmethodID AndroidLocation::jGetAccuracyMethod = 0;
jmethodID AndroidLocation::jGetBearingMethod = 0;
jmethodID AndroidLocation::jGetSpeedMethod = 0;
jmethodID AndroidLocation::jGetTimeMethod = 0;

jclass AndroidLayerRendererFrame::jLayerRendererFrameClass = 0;
jmethodID AndroidLayerRendererFrame::jBeginDrawingMethod = 0;
jmethodID AndroidLayerRendererFrame::jDrawBackgroundMethod = 0;
jmethodID AndroidLayerRendererFrame::jDrawForegroundMethod = 0;
jmethodID AndroidLayerRendererFrame::jEndDrawingMethod = 0;

RefCountedJavaObject::~RefCountedJavaObject() {
    if (mObject)
        GetJNIForThread()->DeleteGlobalRef(mObject);
    mObject = nullptr;
}

void
mozilla::InitAndroidJavaWrappers(JNIEnv *jEnv)
{
    AndroidGeckoEvent::InitGeckoEventClass(jEnv);
    AndroidPoint::InitPointClass(jEnv);
    AndroidLocation::InitLocationClass(jEnv);
    AndroidRect::InitRectClass(jEnv);
    AndroidRectF::InitRectFClass(jEnv);
    AndroidLayerRendererFrame::InitLayerRendererFrameClass(jEnv);
    InitStubs(jEnv);
}

void
AndroidGeckoEvent::InitGeckoEventClass(JNIEnv *jEnv)
{
    initInit();

    jGeckoEventClass = getClassGlobalRef("org/mozilla/gecko/GeckoEvent");

    jActionField = getField("mAction", "I");
    jTypeField = getField("mType", "I");
    jAckNeededField = getField("mAckNeeded", "Z");
    jTimeField = getField("mTime", "J");
    jPoints = getField("mPoints", "[Landroid/graphics/Point;");
    jPointIndicies = getField("mPointIndicies", "[I");
    jOrientations = getField("mOrientations", "[F");
    jPressures = getField("mPressures", "[F");
    jPointRadii = getField("mPointRadii", "[Landroid/graphics/Point;");
    jXField = getField("mX", "D");
    jYField = getField("mY", "D");
    jZField = getField("mZ", "D");
    jRectField = getField("mRect", "Landroid/graphics/Rect;");

    jCharactersField = getField("mCharacters", "Ljava/lang/String;");
    jCharactersExtraField = getField("mCharactersExtra", "Ljava/lang/String;");
    jDataField = getField("mData", "Ljava/lang/String;");
    jKeyCodeField = getField("mKeyCode", "I");
    jScanCodeField = getField("mScanCode", "I");
    jMetaStateField = getField("mMetaState", "I");
    jDomKeyLocationField = getField("mDomKeyLocation", "Lorg/mozilla/gecko/GeckoEvent$DomKeyLocation;");
    jFlagsField = getField("mFlags", "I");
    jUnicodeCharField = getField("mUnicodeChar", "I");
    jBaseUnicodeCharField = getField("mBaseUnicodeChar", "I");
    jDOMPrintableKeyValueField = getField("mDOMPrintableKeyValue", "I");
    jRepeatCountField = getField("mRepeatCount", "I");
    jCountField = getField("mCount", "I");
    jStartField = getField("mStart", "I");
    jEndField = getField("mEnd", "I");
    jPointerIndexField = getField("mPointerIndex", "I");
    jRangeTypeField = getField("mRangeType", "I");
    jRangeStylesField = getField("mRangeStyles", "I");
    jRangeLineStyleField = getField("mRangeLineStyle", "I");
    jRangeBoldLineField = getField("mRangeBoldLine", "Z");
    jRangeForeColorField = getField("mRangeForeColor", "I");
    jRangeBackColorField = getField("mRangeBackColor", "I");
    jRangeLineColorField = getField("mRangeLineColor", "I");
    jLocationField = getField("mLocation", "Landroid/location/Location;");
    jConnectionTypeField = getField("mConnectionType", "I");
    jIsWifiField = getField("mIsWifi", "Z");
    jDHCPGatewayField = getField("mDHCPGateway", "I");
    jScreenOrientationField = getField("mScreenOrientation", "S");
    jByteBufferField = getField("mBuffer", "Ljava/nio/ByteBuffer;");
    jWidthField = getField("mWidth", "I");
    jHeightField = getField("mHeight", "I");
    jIDField = getField("mID", "I");
    jGamepadButtonField = getField("mGamepadButton", "I");
    jGamepadButtonPressedField = getField("mGamepadButtonPressed", "Z");
    jGamepadButtonValueField = getField("mGamepadButtonValue", "F");
    jGamepadValuesField = getField("mGamepadValues", "[F");
    jPrefNamesField = getField("mPrefNames", "[Ljava/lang/String;");

    // Init GeckoEvent.DomKeyLocation enum
    jDomKeyLocationClass = getClassGlobalRef("org/mozilla/gecko/GeckoEvent$DomKeyLocation");
    jDomKeyLocationValueField = getField("value", "I");
}

void
AndroidLocation::InitLocationClass(JNIEnv *jEnv)
{
    initInit();

    jLocationClass = getClassGlobalRef("android/location/Location");
    jGetLatitudeMethod = getMethod("getLatitude", "()D");
    jGetLongitudeMethod = getMethod("getLongitude", "()D");
    jGetAltitudeMethod = getMethod("getAltitude", "()D");
    jGetAccuracyMethod = getMethod("getAccuracy", "()F");
    jGetBearingMethod = getMethod("getBearing", "()F");
    jGetSpeedMethod = getMethod("getSpeed", "()F");
    jGetTimeMethod = getMethod("getTime", "()J");
}

nsGeoPosition*
AndroidLocation::CreateGeoPosition(JNIEnv *jenv, jobject jobj)
{
    AutoLocalJNIFrame jniFrame(jenv);

    double latitude  = jenv->CallDoubleMethod(jobj, jGetLatitudeMethod);
    if (jniFrame.CheckForException()) return nullptr;
    double longitude = jenv->CallDoubleMethod(jobj, jGetLongitudeMethod);
    if (jniFrame.CheckForException()) return nullptr;
    double altitude  = jenv->CallDoubleMethod(jobj, jGetAltitudeMethod);
    if (jniFrame.CheckForException()) return nullptr;
    float  accuracy  = jenv->CallFloatMethod (jobj, jGetAccuracyMethod);
    if (jniFrame.CheckForException()) return nullptr;
    float  bearing   = jenv->CallFloatMethod (jobj, jGetBearingMethod);
    if (jniFrame.CheckForException()) return nullptr;
    float  speed     = jenv->CallFloatMethod (jobj, jGetSpeedMethod);
    if (jniFrame.CheckForException()) return nullptr;
    long long time   = jenv->CallLongMethod  (jobj, jGetTimeMethod);
    if (jniFrame.CheckForException()) return nullptr;

    return new nsGeoPosition(latitude, longitude,
                             altitude, accuracy,
                             accuracy, bearing,
                             speed,    time);
}

void
AndroidPoint::InitPointClass(JNIEnv *jEnv)
{
    initInit();

    jPointClass = getClassGlobalRef("android/graphics/Point");

    jXField = getField("x", "I");
    jYField = getField("y", "I");
}

void
AndroidRect::InitRectClass(JNIEnv *jEnv)
{
    initInit();

    jRectClass = getClassGlobalRef("android/graphics/Rect");

    jBottomField = getField("bottom", "I");
    jLeftField = getField("left", "I");
    jTopField = getField("top", "I");
    jRightField = getField("right", "I");
}

void
AndroidRectF::InitRectFClass(JNIEnv *jEnv)
{
    initInit();

    jRectClass = getClassGlobalRef("android/graphics/RectF");

    jBottomField = getField("bottom", "F");
    jLeftField = getField("left", "F");
    jTopField = getField("top", "F");
    jRightField = getField("right", "F");
}

void
AndroidLayerRendererFrame::InitLayerRendererFrameClass(JNIEnv *jEnv)
{
    initInit();

    jLayerRendererFrameClass = getClassGlobalRef("org/mozilla/gecko/gfx/LayerRenderer$Frame");

    jBeginDrawingMethod = getMethod("beginDrawing", "()V");
    jDrawBackgroundMethod = getMethod("drawBackground", "()V");
    jDrawForegroundMethod = getMethod("drawForeground", "()V");
    jEndDrawingMethod = getMethod("endDrawing", "()V");
}

#undef initInit
#undef initClassGlobalRef
#undef getField
#undef getMethod

void
AndroidGeckoEvent::ReadPointArray(nsTArray<nsIntPoint> &points,
                                  JNIEnv *jenv,
                                  jfieldID field,
                                  int32_t count)
{
    jobjectArray jObjArray = (jobjectArray)jenv->GetObjectField(wrapped_obj, field);
    for (int32_t i = 0; i < count; i++) {
        jobject jObj = jenv->GetObjectArrayElement(jObjArray, i);
        AndroidPoint jpoint(jenv, jObj);

        nsIntPoint p(jpoint.X(), jpoint.Y());
        points.AppendElement(p);
    }
}

void
AndroidGeckoEvent::ReadIntArray(nsTArray<int> &aVals,
                                JNIEnv *jenv,
                                jfieldID field,
                                int32_t count)
{
    jintArray jIntArray = (jintArray)jenv->GetObjectField(wrapped_obj, field);
    jint *vals = jenv->GetIntArrayElements(jIntArray, nullptr);
    for (int32_t i = 0; i < count; i++) {
        aVals.AppendElement(vals[i]);
    }
    jenv->ReleaseIntArrayElements(jIntArray, vals, JNI_ABORT);
}

void
AndroidGeckoEvent::ReadFloatArray(nsTArray<float> &aVals,
                                  JNIEnv *jenv,
                                  jfieldID field,
                                  int32_t count)
{
    jfloatArray jFloatArray = (jfloatArray)jenv->GetObjectField(wrapped_obj, field);
    jfloat *vals = jenv->GetFloatArrayElements(jFloatArray, nullptr);
    for (int32_t i = 0; i < count; i++) {
        aVals.AppendElement(vals[i]);
    }
    jenv->ReleaseFloatArrayElements(jFloatArray, vals, JNI_ABORT);
}

void
AndroidGeckoEvent::ReadStringArray(nsTArray<nsString> &array,
                                   JNIEnv *jenv,
                                   jfieldID field)
{
    jarray jArray = (jarray)jenv->GetObjectField(wrapped_obj, field);
    jsize length = jenv->GetArrayLength(jArray);
    jobjectArray jStringArray = (jobjectArray)jArray;
    nsString *strings = array.AppendElements(length);
    for (jsize i = 0; i < length; ++i) {
        jstring javastring = (jstring) jenv->GetObjectArrayElement(jStringArray, i);
        ReadStringFromJString(strings[i], jenv, javastring);
    }
}

void
AndroidGeckoEvent::ReadRectField(JNIEnv *jenv)
{
    AndroidRect r(jenv, jenv->GetObjectField(wrappedObject(), jRectField));
    if (!r.isNull()) {
        mRect.SetRect(r.Left(),
                      r.Top(),
                      r.Width(),
                      r.Height());
    } else {
        mRect.SetEmpty();
    }
}

void
AndroidGeckoEvent::ReadStringFromJString(nsString &aString, JNIEnv *jenv,
                                         jstring s)
{
    if (!s) {
        aString.SetIsVoid(true);
        return;
    }

    int len = jenv->GetStringLength(s);
    aString.SetLength(len);
    jenv->GetStringRegion(s, 0, len, reinterpret_cast<jchar*>(aString.BeginWriting()));
}

void
AndroidGeckoEvent::ReadCharactersField(JNIEnv *jenv)
{
    jstring s = (jstring) jenv->GetObjectField(wrapped_obj, jCharactersField);
    ReadStringFromJString(mCharacters, jenv, s);
}

void
AndroidGeckoEvent::ReadCharactersExtraField(JNIEnv *jenv)
{
    jstring s = (jstring) jenv->GetObjectField(wrapped_obj, jCharactersExtraField);
    ReadStringFromJString(mCharactersExtra, jenv, s);
}

void
AndroidGeckoEvent::ReadDataField(JNIEnv *jenv)
{
    jstring s = (jstring) jenv->GetObjectField(wrapped_obj, jDataField);
    ReadStringFromJString(mData, jenv, s);
}

void
AndroidGeckoEvent::UnionRect(nsIntRect const& aRect)
{
    mRect = aRect.Union(mRect);
}

uint32_t
AndroidGeckoEvent::ReadDomKeyLocation(JNIEnv* jenv, jobject jGeckoEventObj)
{
    jobject enumObject = jenv->GetObjectField(jGeckoEventObj,
                                             jDomKeyLocationField);
    MOZ_ASSERT(enumObject);
    int enumValue = jenv->GetIntField(enumObject, jDomKeyLocationValueField);
    MOZ_ASSERT(enumValue >= nsIDOMKeyEvent::DOM_KEY_LOCATION_STANDARD &&
               enumValue <= nsIDOMKeyEvent::DOM_KEY_LOCATION_JOYSTICK);
    return static_cast<uint32_t>(enumValue);
}

void
AndroidGeckoEvent::Init(JNIEnv *jenv, jobject jobj)
{
    NS_ASSERTION(!wrapped_obj, "Init called on non-null wrapped_obj!");

    wrapped_obj = jobj;

    if (!jobj)
        return;

    mAction = jenv->GetIntField(jobj, jActionField);
    mType = jenv->GetIntField(jobj, jTypeField);
    mAckNeeded = jenv->GetBooleanField(jobj, jAckNeededField);

    switch (mType) {
        case SIZE_CHANGED:
            ReadPointArray(mPoints, jenv, jPoints, 2);
            break;

        case KEY_EVENT:
        case IME_KEY_EVENT:
            mTime = jenv->GetLongField(jobj, jTimeField);
            mMetaState = jenv->GetIntField(jobj, jMetaStateField);
            mDomKeyLocation = ReadDomKeyLocation(jenv, jobj);
            mFlags = jenv->GetIntField(jobj, jFlagsField);
            mKeyCode = jenv->GetIntField(jobj, jKeyCodeField);
            mScanCode = jenv->GetIntField(jobj, jScanCodeField);
            mUnicodeChar = jenv->GetIntField(jobj, jUnicodeCharField);
            mBaseUnicodeChar = jenv->GetIntField(jobj, jBaseUnicodeCharField);
            mDOMPrintableKeyValue =
                jenv->GetIntField(jobj, jDOMPrintableKeyValueField);
            mRepeatCount = jenv->GetIntField(jobj, jRepeatCountField);
            ReadCharactersField(jenv);
            break;

        case NATIVE_GESTURE_EVENT:
            mTime = jenv->GetLongField(jobj, jTimeField);
            mMetaState = jenv->GetIntField(jobj, jMetaStateField);
            mCount = jenv->GetIntField(jobj, jCountField);
            ReadPointArray(mPoints, jenv, jPoints, mCount);
            mX = jenv->GetDoubleField(jobj, jXField);

            break;

        case MOTION_EVENT:
            mTime = jenv->GetLongField(jobj, jTimeField);
            mMetaState = jenv->GetIntField(jobj, jMetaStateField);
            mCount = jenv->GetIntField(jobj, jCountField);
            mPointerIndex = jenv->GetIntField(jobj, jPointerIndexField);

            ReadPointArray(mPointRadii, jenv, jPointRadii, mCount);
            ReadFloatArray(mOrientations, jenv, jOrientations, mCount);
            ReadFloatArray(mPressures, jenv, jPressures, mCount);
            ReadPointArray(mPoints, jenv, jPoints, mCount);
            ReadIntArray(mPointIndicies, jenv, jPointIndicies, mCount);

            break;

        case IME_EVENT:
            mStart = jenv->GetIntField(jobj, jStartField);
            mEnd = jenv->GetIntField(jobj, jEndField);

            if (mAction == IME_REPLACE_TEXT) {
                ReadCharactersField(jenv);
            } else if (mAction == IME_UPDATE_COMPOSITION ||
                    mAction == IME_ADD_COMPOSITION_RANGE) {
                mRangeType = jenv->GetIntField(jobj, jRangeTypeField);
                mRangeStyles = jenv->GetIntField(jobj, jRangeStylesField);
                mRangeLineStyle =
                    jenv->GetIntField(jobj, jRangeLineStyleField);
                mRangeBoldLine =
                    jenv->GetBooleanField(jobj, jRangeBoldLineField);
                mRangeForeColor =
                    jenv->GetIntField(jobj, jRangeForeColorField);
                mRangeBackColor =
                    jenv->GetIntField(jobj, jRangeBackColorField);
                mRangeLineColor =
                    jenv->GetIntField(jobj, jRangeLineColorField);
            }
            break;

        case SENSOR_EVENT:
             mX = jenv->GetDoubleField(jobj, jXField);
             mY = jenv->GetDoubleField(jobj, jYField);
             mZ = jenv->GetDoubleField(jobj, jZField);
             mFlags = jenv->GetIntField(jobj, jFlagsField);
             mMetaState = jenv->GetIntField(jobj, jMetaStateField);
             break;

        case LOCATION_EVENT: {
            jobject location = jenv->GetObjectField(jobj, jLocationField);
            mGeoPosition = AndroidLocation::CreateGeoPosition(jenv, location);
            break;
        }

        case LOAD_URI: {
            ReadCharactersField(jenv);
            ReadCharactersExtraField(jenv);
            break;
        }

        case VIEWPORT:
        case BROADCAST: {
            ReadCharactersField(jenv);
            ReadCharactersExtraField(jenv);
            break;
        }

        case NETWORK_CHANGED: {
            mConnectionType = jenv->GetIntField(jobj, jConnectionTypeField);
            mIsWifi = jenv->GetBooleanField(jobj, jIsWifiField);
            mDHCPGateway = jenv->GetIntField(jobj, jDHCPGatewayField);
            break;
        }

        case VISITED: {
            ReadCharactersField(jenv);
            break;
        }

        case THUMBNAIL: {
            mMetaState = jenv->GetIntField(jobj, jMetaStateField);
            ReadPointArray(mPoints, jenv, jPoints, 1);
            mByteBuffer = new RefCountedJavaObject(jenv, jenv->GetObjectField(jobj, jByteBufferField));
            break;
        }

        case SCREENORIENTATION_CHANGED: {
            mScreenOrientation = jenv->GetShortField(jobj, jScreenOrientationField);
            break;
        }

        case COMPOSITOR_CREATE: {
            mWidth = jenv->GetIntField(jobj, jWidthField);
            mHeight = jenv->GetIntField(jobj, jHeightField);
            break;
        }

        case CALL_OBSERVER: {
            ReadCharactersField(jenv);
            ReadCharactersExtraField(jenv);
            ReadDataField(jenv);
            break;
        }

        case REMOVE_OBSERVER: {
            ReadCharactersField(jenv);
            break;
        }

        case LOW_MEMORY: {
            mMetaState = jenv->GetIntField(jobj, jMetaStateField);
            break;
        }

        case NETWORK_LINK_CHANGE: {
            ReadCharactersField(jenv);
            break;
        }

        case TELEMETRY_HISTOGRAM_ADD: {
            ReadCharactersField(jenv);
            mCount = jenv->GetIntField(jobj, jCountField);
            break;
        }

        case TELEMETRY_UI_SESSION_START: {
            ReadCharactersField(jenv);
            mTime = jenv->GetLongField(jobj, jTimeField);
            break;
        }

        case TELEMETRY_UI_SESSION_STOP: {
            ReadCharactersField(jenv);
            ReadCharactersExtraField(jenv);
            mTime = jenv->GetLongField(jobj, jTimeField);
            break;
        }

        case TELEMETRY_UI_EVENT: {
            ReadCharactersField(jenv);
            ReadCharactersExtraField(jenv);
            ReadDataField(jenv);
            mTime = jenv->GetLongField(jobj, jTimeField);
            break;
        }

        case GAMEPAD_ADDREMOVE: {
            mID = jenv->GetIntField(jobj, jIDField);
            break;
        }

        case GAMEPAD_DATA: {
            mID = jenv->GetIntField(jobj, jIDField);
            if (mAction == ACTION_GAMEPAD_BUTTON) {
                mGamepadButton = jenv->GetIntField(jobj, jGamepadButtonField);
                mGamepadButtonPressed = jenv->GetBooleanField(jobj, jGamepadButtonPressedField);
                mGamepadButtonValue = jenv->GetFloatField(jobj, jGamepadButtonValueField);
            } else if (mAction == ACTION_GAMEPAD_AXES) {
                // Flags is a bitfield of valid entries in gamepadvalues
                mFlags = jenv->GetIntField(jobj, jFlagsField);
                mCount = jenv->GetIntField(jobj, jCountField);
                ReadFloatArray(mGamepadValues, jenv, jGamepadValuesField, mCount);
            }
            break;
        }

        case PREFERENCES_OBSERVE:
        case PREFERENCES_GET: {
            ReadStringArray(mPrefNames, jenv, jPrefNamesField);
            mCount = jenv->GetIntField(jobj, jCountField);
            break;
        }

        case PREFERENCES_REMOVE_OBSERVERS: {
            mCount = jenv->GetIntField(jobj, jCountField);
            break;
        }

        default:
            break;
    }

#ifdef DEBUG_ANDROID_EVENTS
    ALOG("AndroidGeckoEvent: %p : %d", (void*)jobj, mType);
#endif
}

void
AndroidGeckoEvent::Init(int aType)
{
    mType = aType;
    mAckNeeded = false;
}

void
AndroidGeckoEvent::Init(AndroidGeckoEvent *aResizeEvent)
{
    NS_ASSERTION(aResizeEvent->Type() == SIZE_CHANGED, "Init called on non-SIZE_CHANGED event");

    mType = FORCED_RESIZE;
    mAckNeeded = false;
    mTime = aResizeEvent->mTime;
    mPoints = aResizeEvent->mPoints; // x,y coordinates
}

WidgetTouchEvent
AndroidGeckoEvent::MakeTouchEvent(nsIWidget* widget)
{
    int type = NS_EVENT_NULL;
    int startIndex = 0;
    int endIndex = Count();

    switch (Action()) {
        case AndroidMotionEvent::ACTION_DOWN:
        case AndroidMotionEvent::ACTION_POINTER_DOWN: {
            type = NS_TOUCH_START;
            break;
        }
        case AndroidMotionEvent::ACTION_MOVE: {
            type = NS_TOUCH_MOVE;
            break;
        }
        case AndroidMotionEvent::ACTION_UP:
        case AndroidMotionEvent::ACTION_POINTER_UP: {
            type = NS_TOUCH_END;
            // for pointer-up events we only want the data from
            // the one pointer that went up
            startIndex = PointerIndex();
            endIndex = startIndex + 1;
            break;
        }
        case AndroidMotionEvent::ACTION_OUTSIDE:
        case AndroidMotionEvent::ACTION_CANCEL: {
            type = NS_TOUCH_CANCEL;
            break;
        }
    }

    WidgetTouchEvent event(true, type, widget);
    if (type == NS_EVENT_NULL) {
        // An event we don't know about
        return event;
    }

    event.modifiers = DOMModifiers();
    event.time = Time();

    const nsIntPoint& offset = widget->WidgetToScreenOffset();
    event.touches.SetCapacity(endIndex - startIndex);
    for (int i = startIndex; i < endIndex; i++) {
        // In this code branch, we are dispatching this event directly
        // into Gecko (as opposed to going through the AsyncPanZoomController),
        // and the Points() array has points in CSS pixels, which we need
        // to convert.
        CSSToLayoutDeviceScale scale = widget->GetDefaultScale();
        nsIntPoint pt(
            (Points()[i].x * scale.scale) - offset.x,
            (Points()[i].y * scale.scale) - offset.y);
        nsIntPoint radii(
            PointRadii()[i].x * scale.scale,
            PointRadii()[i].y * scale.scale);
        nsRefPtr<Touch> t = new Touch(PointIndicies()[i],
                                      pt,
                                      radii,
                                      Orientations()[i],
                                      Pressures()[i]);
        event.touches.AppendElement(t);
    }

    return event;
}

MultiTouchInput
AndroidGeckoEvent::MakeMultiTouchInput(nsIWidget* widget)
{
    MultiTouchInput::MultiTouchType type = (MultiTouchInput::MultiTouchType)-1;
    int startIndex = 0;
    int endIndex = Count();

    switch (Action()) {
        case AndroidMotionEvent::ACTION_DOWN:
        case AndroidMotionEvent::ACTION_POINTER_DOWN: {
            type = MultiTouchInput::MULTITOUCH_START;
            break;
        }
        case AndroidMotionEvent::ACTION_MOVE: {
            type = MultiTouchInput::MULTITOUCH_MOVE;
            break;
        }
        case AndroidMotionEvent::ACTION_UP:
        case AndroidMotionEvent::ACTION_POINTER_UP: {
            // for pointer-up events we only want the data from
            // the one pointer that went up
            startIndex = PointerIndex();
            endIndex = startIndex + 1;
            type = MultiTouchInput::MULTITOUCH_END;
            break;
        }
        case AndroidMotionEvent::ACTION_OUTSIDE:
        case AndroidMotionEvent::ACTION_CANCEL: {
            type = MultiTouchInput::MULTITOUCH_CANCEL;
            break;
        }
    }

    MultiTouchInput event(type, Time(), TimeStamp(), 0);
    event.modifiers = DOMModifiers();

    if (type < 0) {
        // An event we don't know about
        return event;
    }

    const nsIntPoint& offset = widget->WidgetToScreenOffset();
    event.mTouches.SetCapacity(endIndex - startIndex);
    for (int i = startIndex; i < endIndex; i++) {
        nsIntPoint point = Points()[i] - offset;
        nsIntPoint radius = PointRadii()[i];
        SingleTouchData data(PointIndicies()[i],
                             ScreenIntPoint::FromUnknownPoint(
                               gfx::IntPoint(point.x, point.y)),
                             ScreenSize::FromUnknownSize(
                               gfx::Size(radius.x, radius.y)),
                             Orientations()[i],
                             Pressures()[i]);
        event.mTouches.AppendElement(data);
    }

    return event;
}

WidgetMouseEvent
AndroidGeckoEvent::MakeMouseEvent(nsIWidget* widget)
{
    uint32_t msg = NS_EVENT_NULL;
    if (Points().Length() > 0) {
        switch (Action()) {
            case AndroidMotionEvent::ACTION_HOVER_MOVE:
                msg = NS_MOUSE_MOVE;
                break;
            case AndroidMotionEvent::ACTION_HOVER_ENTER:
                msg = NS_MOUSEENTER;
                break;
            case AndroidMotionEvent::ACTION_HOVER_EXIT:
                msg = NS_MOUSELEAVE;
                break;
            default:
                break;
        }
    }

    WidgetMouseEvent event(true, msg, widget,
                           WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);

    if (msg == NS_EVENT_NULL) {
        // unknown type, or no point data. abort
        return event;
    }

    // XXX can we synthesize different buttons?
    event.button = WidgetMouseEvent::eLeftButton;
    if (msg != NS_MOUSE_MOVE) {
        event.clickCount = 1;
    }
    event.modifiers = DOMModifiers();
    event.time = Time();

    // We are dispatching this event directly into Gecko (as opposed to going
    // through the AsyncPanZoomController), and the Points() array has points
    // in CSS pixels, which we need to convert to LayoutDevice pixels.
    const nsIntPoint& offset = widget->WidgetToScreenOffset();
    CSSToLayoutDeviceScale scale = widget->GetDefaultScale();
    event.refPoint = LayoutDeviceIntPoint((Points()[0].x * scale.scale) - offset.x,
                                          (Points()[0].y * scale.scale) - offset.y);

    return event;
}

Modifiers
AndroidGeckoEvent::DOMModifiers() const
{
    Modifiers result = 0;
    if (mMetaState & AMETA_ALT_MASK) {
        result |= MODIFIER_ALT;
    }
    if (mMetaState & AMETA_SHIFT_MASK) {
        result |= MODIFIER_SHIFT;
    }
    if (mMetaState & AMETA_CTRL_MASK) {
        result |= MODIFIER_CONTROL;
    }
    if (mMetaState & AMETA_META_MASK) {
        result |= MODIFIER_META;
    }
    if (mMetaState & AMETA_FUNCTION_ON) {
        result |= MODIFIER_FN;
    }
    if (mMetaState & AMETA_CAPS_LOCK_ON) {
        result |= MODIFIER_CAPSLOCK;
    }
    if (mMetaState & AMETA_NUM_LOCK_ON) {
        result |= MODIFIER_NUMLOCK;
    }
    if (mMetaState & AMETA_SCROLL_LOCK_ON) {
        result |= MODIFIER_SCROLLLOCK;
    }
    return result;
}

void
AndroidPoint::Init(JNIEnv *jenv, jobject jobj)
{
    if (jobj) {
        mX = jenv->GetIntField(jobj, jXField);
        mY = jenv->GetIntField(jobj, jYField);
    } else {
        mX = 0;
        mY = 0;
    }
}

void
AndroidLayerRendererFrame::Init(JNIEnv *env, jobject jobj)
{
    if (!isNull()) {
        Dispose(env);
    }

    wrapped_obj = env->NewGlobalRef(jobj);
}

void
AndroidLayerRendererFrame::Dispose(JNIEnv *env)
{
    if (isNull()) {
        return;
    }

    env->DeleteGlobalRef(wrapped_obj);
    wrapped_obj = 0;
}

NS_IMPL_ISUPPORTS(nsAndroidDisplayport, nsIAndroidDisplayport)

bool
AndroidLayerRendererFrame::BeginDrawing(AutoLocalJNIFrame *jniFrame)
{
    if (!jniFrame || !jniFrame->GetEnv())
        return false;

    jniFrame->GetEnv()->CallVoidMethod(wrapped_obj, jBeginDrawingMethod);
    if (jniFrame->CheckForException())
        return false;

    return true;
}

bool
AndroidLayerRendererFrame::DrawBackground(AutoLocalJNIFrame *jniFrame)
{
    if (!jniFrame || !jniFrame->GetEnv())
        return false;

    jniFrame->GetEnv()->CallVoidMethod(wrapped_obj, jDrawBackgroundMethod);
    if (jniFrame->CheckForException())
        return false;

    return true;
}

bool
AndroidLayerRendererFrame::DrawForeground(AutoLocalJNIFrame *jniFrame)
{
    if (!jniFrame || !jniFrame->GetEnv())
        return false;

    jniFrame->GetEnv()->CallVoidMethod(wrapped_obj, jDrawForegroundMethod);
    if (jniFrame->CheckForException())
        return false;

    return true;
}

bool
AndroidLayerRendererFrame::EndDrawing(AutoLocalJNIFrame *jniFrame)
{
    if (!jniFrame || !jniFrame->GetEnv())
        return false;

    jniFrame->GetEnv()->CallVoidMethod(wrapped_obj, jEndDrawingMethod);
    if (jniFrame->CheckForException())
        return false;

    return true;
}

void
AndroidRect::Init(JNIEnv *jenv, jobject jobj)
{
    NS_ASSERTION(wrapped_obj == nullptr, "Init called on non-null wrapped_obj!");

    wrapped_obj = jobj;

    if (jobj) {
        mTop = jenv->GetIntField(jobj, jTopField);
        mLeft = jenv->GetIntField(jobj, jLeftField);
        mRight = jenv->GetIntField(jobj, jRightField);
        mBottom = jenv->GetIntField(jobj, jBottomField);
    } else {
        mTop = 0;
        mLeft = 0;
        mRight = 0;
        mBottom = 0;
    }
}

void
AndroidRectF::Init(JNIEnv *jenv, jobject jobj)
{
    NS_ASSERTION(wrapped_obj == nullptr, "Init called on non-null wrapped_obj!");

    wrapped_obj = jobj;

    if (jobj) {
        mTop = jenv->GetFloatField(jobj, jTopField);
        mLeft = jenv->GetFloatField(jobj, jLeftField);
        mRight = jenv->GetFloatField(jobj, jRightField);
        mBottom = jenv->GetFloatField(jobj, jBottomField);
    } else {
        mTop = 0;
        mLeft = 0;
        mRight = 0;
        mBottom = 0;
    }
}

nsJNIString::nsJNIString(jstring jstr, JNIEnv *jenv)
{
    if (!jstr) {
        SetIsVoid(true);
        return;
    }
    JNIEnv *jni = jenv;
    if (!jni) {
        jni = AndroidBridge::GetJNIEnv();
    }
    const jchar* jCharPtr = jni->GetStringChars(jstr, nullptr);

    if (!jCharPtr) {
        SetIsVoid(true);
        return;
    }

    jsize len = jni->GetStringLength(jstr);

    if (len <= 0) {
        SetIsVoid(true);
    } else {
        Assign(reinterpret_cast<const char16_t*>(jCharPtr), len);
    }
    jni->ReleaseStringChars(jstr, jCharPtr);
}

nsJNICString::nsJNICString(jstring jstr, JNIEnv *jenv)
{
    if (!jstr) {
        SetIsVoid(true);
        return;
    }
    JNIEnv *jni = jenv;
    if (!jni) {
        jni = AndroidBridge::GetJNIEnv();
    }
    const char* jCharPtr = jni->GetStringUTFChars(jstr, nullptr);

    if (!jCharPtr) {
        SetIsVoid(true);
        return;
    }

    jsize len = jni->GetStringUTFLength(jstr);

    if (len <= 0) {
        SetIsVoid(true);
    } else {
        Assign(jCharPtr, len);
    }
    jni->ReleaseStringUTFChars(jstr, jCharPtr);
}
