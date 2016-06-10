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

jclass AndroidGeckoEvent::jGeckoEventClass = 0;
jfieldID AndroidGeckoEvent::jActionField = 0;
jfieldID AndroidGeckoEvent::jTypeField = 0;
jfieldID AndroidGeckoEvent::jTimeField = 0;
jfieldID AndroidGeckoEvent::jPoints = 0;
jfieldID AndroidGeckoEvent::jPointIndicies = 0;
jfieldID AndroidGeckoEvent::jPressures = 0;
jfieldID AndroidGeckoEvent::jToolTypes = 0;
jfieldID AndroidGeckoEvent::jPointRadii = 0;
jfieldID AndroidGeckoEvent::jOrientations = 0;
jfieldID AndroidGeckoEvent::jXField = 0;
jfieldID AndroidGeckoEvent::jYField = 0;
jfieldID AndroidGeckoEvent::jZField = 0;
jfieldID AndroidGeckoEvent::jWField = 0;
jfieldID AndroidGeckoEvent::jDistanceField = 0;
jfieldID AndroidGeckoEvent::jRectField = 0;

jfieldID AndroidGeckoEvent::jCharactersField = 0;
jfieldID AndroidGeckoEvent::jCharactersExtraField = 0;
jfieldID AndroidGeckoEvent::jDataField = 0;
jfieldID AndroidGeckoEvent::jMetaStateField = 0;
jfieldID AndroidGeckoEvent::jFlagsField = 0;
jfieldID AndroidGeckoEvent::jCountField = 0;
jfieldID AndroidGeckoEvent::jPointerIndexField = 0;
jfieldID AndroidGeckoEvent::jLocationField = 0;
jfieldID AndroidGeckoEvent::jConnectionTypeField = 0;
jfieldID AndroidGeckoEvent::jIsWifiField = 0;
jfieldID AndroidGeckoEvent::jDHCPGatewayField = 0;
jfieldID AndroidGeckoEvent::jScreenOrientationField = 0;
jfieldID AndroidGeckoEvent::jScreenAngleField = 0;
jfieldID AndroidGeckoEvent::jByteBufferField = 0;
jfieldID AndroidGeckoEvent::jWidthField = 0;
jfieldID AndroidGeckoEvent::jHeightField = 0;
jfieldID AndroidGeckoEvent::jIDField = 0;
jfieldID AndroidGeckoEvent::jGamepadButtonField = 0;
jfieldID AndroidGeckoEvent::jGamepadButtonPressedField = 0;
jfieldID AndroidGeckoEvent::jGamepadButtonValueField = 0;
jfieldID AndroidGeckoEvent::jGamepadValuesField = 0;

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

RefCountedJavaObject::~RefCountedJavaObject() {
    if (mObject)
        GetEnvForThread()->DeleteGlobalRef(mObject);
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
}

void
AndroidGeckoEvent::InitGeckoEventClass(JNIEnv *jEnv)
{
    AutoJNIClass geckoEvent(jEnv, "org/mozilla/gecko/GeckoEvent");
    jGeckoEventClass = geckoEvent.getGlobalRef();

    jActionField = geckoEvent.getField("mAction", "I");
    jTypeField = geckoEvent.getField("mType", "I");
    jTimeField = geckoEvent.getField("mTime", "J");
    jPoints = geckoEvent.getField("mPoints", "[Landroid/graphics/Point;");
    jPointIndicies = geckoEvent.getField("mPointIndicies", "[I");
    jOrientations = geckoEvent.getField("mOrientations", "[F");
    jPressures = geckoEvent.getField("mPressures", "[F");
    jToolTypes = geckoEvent.getField("mToolTypes", "[I");
    jPointRadii = geckoEvent.getField("mPointRadii", "[Landroid/graphics/Point;");
    jXField = geckoEvent.getField("mX", "D");
    jYField = geckoEvent.getField("mY", "D");
    jZField = geckoEvent.getField("mZ", "D");
    jWField = geckoEvent.getField("mW", "D");
    jRectField = geckoEvent.getField("mRect", "Landroid/graphics/Rect;");

    jCharactersField = geckoEvent.getField("mCharacters", "Ljava/lang/String;");
    jCharactersExtraField = geckoEvent.getField("mCharactersExtra", "Ljava/lang/String;");
    jDataField = geckoEvent.getField("mData", "Ljava/lang/String;");
    jMetaStateField = geckoEvent.getField("mMetaState", "I");
    jFlagsField = geckoEvent.getField("mFlags", "I");
    jCountField = geckoEvent.getField("mCount", "I");
    jPointerIndexField = geckoEvent.getField("mPointerIndex", "I");
    jLocationField = geckoEvent.getField("mLocation", "Landroid/location/Location;");
    jConnectionTypeField = geckoEvent.getField("mConnectionType", "I");
    jIsWifiField = geckoEvent.getField("mIsWifi", "Z");
    jDHCPGatewayField = geckoEvent.getField("mDHCPGateway", "I");
    jScreenOrientationField = geckoEvent.getField("mScreenOrientation", "S");
    jScreenAngleField = geckoEvent.getField("mScreenAngle", "S");
    jByteBufferField = geckoEvent.getField("mBuffer", "Ljava/nio/ByteBuffer;");
    jWidthField = geckoEvent.getField("mWidth", "I");
    jHeightField = geckoEvent.getField("mHeight", "I");
    jIDField = geckoEvent.getField("mID", "I");
    jGamepadButtonField = geckoEvent.getField("mGamepadButton", "I");
    jGamepadButtonPressedField = geckoEvent.getField("mGamepadButtonPressed", "Z");
    jGamepadButtonValueField = geckoEvent.getField("mGamepadButtonValue", "F");
    jGamepadValuesField = geckoEvent.getField("mGamepadValues", "[F");
}

void
AndroidLocation::InitLocationClass(JNIEnv *jEnv)
{
    AutoJNIClass location(jEnv, "android/location/Location");
    jLocationClass = location.getGlobalRef();
    jGetLatitudeMethod = location.getMethod("getLatitude", "()D");
    jGetLongitudeMethod = location.getMethod("getLongitude", "()D");
    jGetAltitudeMethod = location.getMethod("getAltitude", "()D");
    jGetAccuracyMethod = location.getMethod("getAccuracy", "()F");
    jGetBearingMethod = location.getMethod("getBearing", "()F");
    jGetSpeedMethod = location.getMethod("getSpeed", "()F");
    jGetTimeMethod = location.getMethod("getTime", "()J");
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
    AutoJNIClass point(jEnv, "android/graphics/Point");
    jPointClass = point.getGlobalRef();

    jXField = point.getField("x", "I");
    jYField = point.getField("y", "I");
}

void
AndroidRect::InitRectClass(JNIEnv *jEnv)
{
    AutoJNIClass rect(jEnv, "android/graphics/Rect");
    jRectClass = rect.getGlobalRef();

    jBottomField = rect.getField("bottom", "I");
    jLeftField = rect.getField("left", "I");
    jTopField = rect.getField("top", "I");
    jRightField = rect.getField("right", "I");
}

void
AndroidRectF::InitRectFClass(JNIEnv *jEnv)
{
    AutoJNIClass rect(jEnv, "android/graphics/RectF");
    jRectClass = rect.getGlobalRef();

    jBottomField = rect.getField("bottom", "F");
    jLeftField = rect.getField("left", "F");
    jTopField = rect.getField("top", "F");
    jRightField = rect.getField("right", "F");
}

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

void
AndroidGeckoEvent::Init(JNIEnv *jenv, jobject jobj)
{
    NS_ASSERTION(!wrapped_obj, "Init called on non-null wrapped_obj!");

    wrapped_obj = jobj;

    if (!jobj)
        return;

    mAction = jenv->GetIntField(jobj, jActionField);
    mType = jenv->GetIntField(jobj, jTypeField);

    switch (mType) {
        case NATIVE_GESTURE_EVENT:
            mTime = jenv->GetLongField(jobj, jTimeField);
            mMetaState = jenv->GetIntField(jobj, jMetaStateField);
            mCount = jenv->GetIntField(jobj, jCountField);
            ReadPointArray(mPoints, jenv, jPoints, mCount);
            mX = jenv->GetDoubleField(jobj, jXField);

            break;

        case MOTION_EVENT:
        case LONG_PRESS:
            mTime = jenv->GetLongField(jobj, jTimeField);
            mMetaState = jenv->GetIntField(jobj, jMetaStateField);
            mCount = jenv->GetIntField(jobj, jCountField);
            mPointerIndex = jenv->GetIntField(jobj, jPointerIndexField);

            ReadPointArray(mPointRadii, jenv, jPointRadii, mCount);
            ReadFloatArray(mOrientations, jenv, jOrientations, mCount);
            ReadFloatArray(mPressures, jenv, jPressures, mCount);
            ReadIntArray(mToolTypes, jenv, jToolTypes, mCount);
            ReadPointArray(mPoints, jenv, jPoints, mCount);
            ReadIntArray(mPointIndicies, jenv, jPointIndicies, mCount);

            break;

        case SENSOR_EVENT:
             mX = jenv->GetDoubleField(jobj, jXField);
             mY = jenv->GetDoubleField(jobj, jYField);
             mZ = jenv->GetDoubleField(jobj, jZField);
             mW = jenv->GetDoubleField(jobj, jWField);
             mFlags = jenv->GetIntField(jobj, jFlagsField);
             mMetaState = jenv->GetIntField(jobj, jMetaStateField);
             mTime = jenv->GetLongField(jobj, jTimeField);
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

        case VIEWPORT: {
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

        case ZOOMEDVIEW: {
            mX = jenv->GetDoubleField(jobj, jXField);
            mMetaState = jenv->GetIntField(jobj, jMetaStateField);
            ReadPointArray(mPoints, jenv, jPoints, 2);
            mByteBuffer = new RefCountedJavaObject(jenv, jenv->GetObjectField(jobj, jByteBufferField));
            break;
        }

        case SCREENORIENTATION_CHANGED: {
            mScreenOrientation = jenv->GetShortField(jobj, jScreenOrientationField);
            mScreenAngle = jenv->GetShortField(jobj, jScreenAngleField);
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
            ReadCharactersExtraField(jenv);
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
}

bool
AndroidGeckoEvent::CanCoalesceWith(AndroidGeckoEvent* ae)
{
    if (Type() == MOTION_EVENT && ae->Type() == MOTION_EVENT) {
        return Action() == AndroidMotionEvent::ACTION_MOVE
            && ae->Action() == AndroidMotionEvent::ACTION_MOVE;
    } else if (Type() == APZ_INPUT_EVENT && ae->Type() == APZ_INPUT_EVENT) {
        return mApzInput.mType == MultiTouchInput::MULTITOUCH_MOVE
            && ae->mApzInput.mType == MultiTouchInput::MULTITOUCH_MOVE;
    }
    return false;
}

mozilla::layers::ScrollableLayerGuid
AndroidGeckoEvent::ApzGuid()
{
    MOZ_ASSERT(Type() == APZ_INPUT_EVENT);
    return mApzGuid;
}

uint64_t
AndroidGeckoEvent::ApzInputBlockId()
{
    MOZ_ASSERT(Type() == APZ_INPUT_EVENT);
    return mApzInputBlockId;
}

nsEventStatus
AndroidGeckoEvent::ApzEventStatus()
{
    MOZ_ASSERT(Type() == APZ_INPUT_EVENT);
    return mApzEventStatus;
}

WidgetTouchEvent
AndroidGeckoEvent::MakeTouchEvent(nsIWidget* widget)
{
    if (Type() == APZ_INPUT_EVENT) {
        return mApzInput.ToWidgetTouchEvent(widget);
    }

    EventMessage type = eVoidEvent;
    int startIndex = 0;
    int endIndex = Count();

    switch (Action()) {
        case AndroidMotionEvent::ACTION_HOVER_ENTER: {
            if (ToolTypes()[0] == AndroidMotionEvent::TOOL_TYPE_MOUSE) {
                break;
            }
        }
            MOZ_FALLTHROUGH;
        case AndroidMotionEvent::ACTION_DOWN:
            MOZ_FALLTHROUGH;
        case AndroidMotionEvent::ACTION_POINTER_DOWN: {
            type = eTouchStart;
            break;
        }
        case AndroidMotionEvent::ACTION_HOVER_MOVE: {
            if (ToolTypes()[0] == AndroidMotionEvent::TOOL_TYPE_MOUSE) {
                break;
            }
        }
            MOZ_FALLTHROUGH;
        case AndroidMotionEvent::ACTION_MOVE: {
            type = eTouchMove;
            break;
        }
        case AndroidMotionEvent::ACTION_HOVER_EXIT: {
            if (ToolTypes()[0] == AndroidMotionEvent::TOOL_TYPE_MOUSE) {
                break;
            }
        }
            MOZ_FALLTHROUGH;
        case AndroidMotionEvent::ACTION_UP:
            MOZ_FALLTHROUGH;
        case AndroidMotionEvent::ACTION_POINTER_UP: {
            type = eTouchEnd;
            // for pointer-up events we only want the data from
            // the one pointer that went up
            startIndex = PointerIndex();
            endIndex = startIndex + 1;
            break;
        }
        case AndroidMotionEvent::ACTION_OUTSIDE:
            MOZ_FALLTHROUGH;
        case AndroidMotionEvent::ACTION_CANCEL: {
            type = eTouchCancel;
            break;
        }
    }

    WidgetTouchEvent event(true, type, widget);
    if (type == eVoidEvent) {
        // An event we don't know about
        return event;
    }

    event.mModifiers = DOMModifiers();
    event.mTime = Time();

    const LayoutDeviceIntPoint& offset = widget->WidgetToScreenOffset();
    event.mTouches.SetCapacity(endIndex - startIndex);
    for (int i = startIndex; i < endIndex; i++) {
        // In this code branch, we are dispatching this event directly
        // into Gecko (as opposed to going through the AsyncPanZoomController),
        // and the Points() array has points in CSS pixels, which we need
        // to convert.
        CSSToLayoutDeviceScale scale = widget->GetDefaultScale();
        LayoutDeviceIntPoint pt(
            (Points()[i].x * scale.scale) - offset.x,
            (Points()[i].y * scale.scale) - offset.y);
        LayoutDeviceIntPoint radius(
            PointRadii()[i].x * scale.scale,
            PointRadii()[i].y * scale.scale);
        RefPtr<Touch> t = new Touch(PointIndicies()[i],
                                    pt,
                                    radius,
                                    Orientations()[i],
                                    Pressures()[i]);
        event.mTouches.AppendElement(t);
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

    const nsIntPoint& offset = widget->WidgetToScreenOffset().ToUnknownPoint();
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
    EventMessage msg = eVoidEvent;
    if (Points().Length() > 0) {
        switch (Action()) {
            case AndroidMotionEvent::ACTION_HOVER_MOVE:
                msg = eMouseMove;
                break;
            case AndroidMotionEvent::ACTION_HOVER_ENTER:
                msg = eMouseEnterIntoWidget;
                break;
            case AndroidMotionEvent::ACTION_HOVER_EXIT:
                msg = eMouseExitFromWidget;
                break;
            default:
                break;
        }
    }

    WidgetMouseEvent event(true, msg, widget,
                           WidgetMouseEvent::eReal, WidgetMouseEvent::eNormal);

    if (msg == eVoidEvent) {
        // unknown type, or no point data. abort
        return event;
    }

    // XXX can we synthesize different buttons?
    event.button = WidgetMouseEvent::eLeftButton;
    if (msg != eMouseMove) {
        event.mClickCount = 1;
    }
    event.mModifiers = DOMModifiers();
    event.mTime = Time();

    // We are dispatching this event directly into Gecko (as opposed to going
    // through the AsyncPanZoomController), and the Points() array has points
    // in CSS pixels, which we need to convert to LayoutDevice pixels.
    const LayoutDeviceIntPoint& offset = widget->WidgetToScreenOffset();
    CSSToLayoutDeviceScale scale = widget->GetDefaultScale();
    event.mRefPoint =
        LayoutDeviceIntPoint((Points()[0].x * scale.scale) - offset.x,
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

NS_IMPL_ISUPPORTS(nsAndroidDisplayport, nsIAndroidDisplayport)

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
        jni = jni::GetGeckoThreadEnv();
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
        jni = jni::GetGeckoThreadEnv();
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
