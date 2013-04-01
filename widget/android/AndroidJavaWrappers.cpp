/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidJavaWrappers.h"
#include "AndroidBridge.h"
#include "nsIAndroidBridge.h"

using namespace mozilla;

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
jfieldID AndroidGeckoEvent::jKeyCodeField = 0;
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
jfieldID AndroidGeckoEvent::jBandwidthField = 0;
jfieldID AndroidGeckoEvent::jCanBeMeteredField = 0;
jfieldID AndroidGeckoEvent::jScreenOrientationField = 0;
jfieldID AndroidGeckoEvent::jByteBufferField = 0;
jfieldID AndroidGeckoEvent::jWidthField = 0;
jfieldID AndroidGeckoEvent::jHeightField = 0;

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

jclass AndroidGeckoLayerClient::jGeckoLayerClientClass = 0;
jclass AndroidGeckoLayerClient::jViewportClass = 0;
jclass AndroidGeckoLayerClient::jDisplayportClass = 0;
jmethodID AndroidGeckoLayerClient::jSetFirstPaintViewport = 0;
jmethodID AndroidGeckoLayerClient::jSetPageRect = 0;
jmethodID AndroidGeckoLayerClient::jSyncViewportInfoMethod = 0;
jmethodID AndroidGeckoLayerClient::jCreateFrameMethod = 0;
jmethodID AndroidGeckoLayerClient::jActivateProgramMethod = 0;
jmethodID AndroidGeckoLayerClient::jDeactivateProgramMethod = 0;
jmethodID AndroidGeckoLayerClient::jGetDisplayPort = 0;
jmethodID AndroidGeckoLayerClient::jViewportCtor = 0;
jfieldID AndroidGeckoLayerClient::jDisplayportPosition = 0;
jfieldID AndroidGeckoLayerClient::jDisplayportResolution = 0;
jmethodID AndroidGeckoLayerClient::jProgressiveUpdateCallbackMethod = 0;

jclass AndroidLayerRendererFrame::jLayerRendererFrameClass = 0;
jmethodID AndroidLayerRendererFrame::jBeginDrawingMethod = 0;
jmethodID AndroidLayerRendererFrame::jDrawBackgroundMethod = 0;
jmethodID AndroidLayerRendererFrame::jDrawForegroundMethod = 0;
jmethodID AndroidLayerRendererFrame::jEndDrawingMethod = 0;

jclass AndroidViewTransform::jViewTransformClass = 0;
jfieldID AndroidViewTransform::jXField = 0;
jfieldID AndroidViewTransform::jYField = 0;
jfieldID AndroidViewTransform::jScaleField = 0;
jfieldID AndroidViewTransform::jFixedLayerMarginLeft = 0;
jfieldID AndroidViewTransform::jFixedLayerMarginTop = 0;
jfieldID AndroidViewTransform::jFixedLayerMarginRight = 0;
jfieldID AndroidViewTransform::jFixedLayerMarginBottom = 0;

jclass AndroidProgressiveUpdateData::jProgressiveUpdateDataClass = 0;
jfieldID AndroidProgressiveUpdateData::jXField = 0;
jfieldID AndroidProgressiveUpdateData::jYField = 0;
jfieldID AndroidProgressiveUpdateData::jWidthField = 0;
jfieldID AndroidProgressiveUpdateData::jHeightField = 0;
jfieldID AndroidProgressiveUpdateData::jScaleField = 0;
jfieldID AndroidProgressiveUpdateData::jShouldAbortField = 0;

static jclass GetClassGlobalRef(JNIEnv* env, const char* className)
{
    jobject classLocalRef = env->FindClass(className);
    if (!classLocalRef) {
        ALOG(">>> FATAL JNI ERROR! FindClass(className=\"%s\") failed. Did "
             "ProGuard optimize away a non-public class?", className);
        env->ExceptionDescribe();
        MOZ_CRASH();
    }

    jobject classGlobalRef = env->NewGlobalRef(classLocalRef);
    if (!classGlobalRef) {
        env->ExceptionDescribe();
        MOZ_CRASH();
    }

    // Local ref no longer necessary because we have a global ref.
    env->DeleteLocalRef(classLocalRef);
    classLocalRef = NULL;

    return static_cast<jclass>(classGlobalRef);
}

static jfieldID GetFieldID(JNIEnv* env, jclass jClass,
                           const char* fieldName, const char* fieldType)
{
    jfieldID fieldID = env->GetFieldID(jClass, fieldName, fieldType);
    if (!fieldID) {
        ALOG(">>> FATAL JNI ERROR! GetFieldID(fieldName=\"%s\", "
             "fieldType=\"%s\") failed. Did ProGuard optimize away a non-"
             "public field?", fieldName, fieldType);
        env->ExceptionDescribe();
        MOZ_CRASH();
    }
    return fieldID;
}

static jmethodID GetMethodID(JNIEnv* env, jclass jClass,
                             const char* methodName, const char* methodType)
{
    jmethodID methodID = env->GetMethodID(jClass, methodName, methodType);
    if (!methodID) {
        ALOG(">>> FATAL JNI ERROR! GetMethodID(methodName=\"%s\", "
             "methodType=\"%s\") failed. Did ProGuard optimize away a non-"
             "public method?", methodName, methodType);
        env->ExceptionDescribe();
        MOZ_CRASH();
    }
    return methodID;
}

#define initInit() jclass jClass

// note that this also sets jClass
#define getClassGlobalRef(cname) \
    (jClass = GetClassGlobalRef(jEnv, cname))

#define getField(fname, ftype) \
    GetFieldID(jEnv, jClass, fname, ftype)

#define getMethod(fname, ftype) \
    GetMethodID(jEnv, jClass, fname, ftype)

RefCountedJavaObject::~RefCountedJavaObject() {
    if (mObject)
        GetJNIForThread()->DeleteGlobalRef(mObject);
    mObject = NULL;
}

void
mozilla::InitAndroidJavaWrappers(JNIEnv *jEnv)
{
    AndroidGeckoEvent::InitGeckoEventClass(jEnv);
    AndroidPoint::InitPointClass(jEnv);
    AndroidLocation::InitLocationClass(jEnv);
    AndroidRect::InitRectClass(jEnv);
    AndroidRectF::InitRectFClass(jEnv);
    AndroidGeckoLayerClient::InitGeckoLayerClientClass(jEnv);
    AndroidLayerRendererFrame::InitLayerRendererFrameClass(jEnv);
    AndroidViewTransform::InitViewTransformClass(jEnv);
    AndroidProgressiveUpdateData::InitProgressiveUpdateDataClass(jEnv);
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
    jKeyCodeField = getField("mKeyCode", "I");
    jMetaStateField = getField("mMetaState", "I");
    jDomKeyLocationField = getField("mDomKeyLocation", "I");
    jFlagsField = getField("mFlags", "I");
    jUnicodeCharField = getField("mUnicodeChar", "I");
    jBaseUnicodeCharField = getField("mBaseUnicodeChar", "I");
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
    jBandwidthField = getField("mBandwidth", "D");
    jCanBeMeteredField = getField("mCanBeMetered", "Z");
    jScreenOrientationField = getField("mScreenOrientation", "S");
    jByteBufferField = getField("mBuffer", "Ljava/nio/ByteBuffer;");
    jWidthField = getField("mWidth", "I");
    jHeightField = getField("mHeight", "I");
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
    if (jniFrame.CheckForException()) return NULL;
    double longitude = jenv->CallDoubleMethod(jobj, jGetLongitudeMethod);
    if (jniFrame.CheckForException()) return NULL;
    double altitude  = jenv->CallDoubleMethod(jobj, jGetAltitudeMethod);
    if (jniFrame.CheckForException()) return NULL;
    float  accuracy  = jenv->CallFloatMethod (jobj, jGetAccuracyMethod);
    if (jniFrame.CheckForException()) return NULL;
    float  bearing   = jenv->CallFloatMethod (jobj, jGetBearingMethod);
    if (jniFrame.CheckForException()) return NULL;
    float  speed     = jenv->CallFloatMethod (jobj, jGetSpeedMethod);
    if (jniFrame.CheckForException()) return NULL;
    long long time   = jenv->CallLongMethod  (jobj, jGetTimeMethod);
    if (jniFrame.CheckForException()) return NULL;

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
AndroidGeckoLayerClient::InitGeckoLayerClientClass(JNIEnv *jEnv)
{
    initInit();

    jGeckoLayerClientClass = getClassGlobalRef("org/mozilla/gecko/gfx/GeckoLayerClient");

    jSetFirstPaintViewport = getMethod("setFirstPaintViewport", "(FFFFFFFFFFF)V");
    jSetPageRect = getMethod("setPageRect", "(FFFF)V");
    jSyncViewportInfoMethod = getMethod("syncViewportInfo",
                                        "(IIIIFZ)Lorg/mozilla/gecko/gfx/ViewTransform;");
    jCreateFrameMethod = getMethod("createFrame", "()Lorg/mozilla/gecko/gfx/LayerRenderer$Frame;");
    jActivateProgramMethod = getMethod("activateProgram", "()V");
    jDeactivateProgramMethod = getMethod("deactivateProgram", "()V");
    jGetDisplayPort = getMethod("getDisplayPort", "(ZZILorg/mozilla/gecko/gfx/ImmutableViewportMetrics;)Lorg/mozilla/gecko/gfx/DisplayPortMetrics;");

    jViewportClass = GetClassGlobalRef(jEnv, "org/mozilla/gecko/gfx/ImmutableViewportMetrics");
    jViewportCtor = GetMethodID(jEnv, jViewportClass, "<init>", "(FFFFFFFFFFFFF)V");

    jDisplayportClass = GetClassGlobalRef(jEnv, "org/mozilla/gecko/gfx/DisplayPortMetrics");
    jDisplayportPosition = GetFieldID(jEnv, jDisplayportClass, "mPosition", "Landroid/graphics/RectF;");
    jDisplayportResolution = GetFieldID(jEnv, jDisplayportClass, "resolution", "F");
    jProgressiveUpdateCallbackMethod = getMethod("progressiveUpdateCallback",
                                                 "(ZFFFFFZ)Lorg/mozilla/gecko/gfx/ProgressiveUpdateData;");
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

void
AndroidViewTransform::InitViewTransformClass(JNIEnv *jEnv)
{
    initInit();

    jViewTransformClass = getClassGlobalRef("org/mozilla/gecko/gfx/ViewTransform");

    jXField = getField("x", "F");
    jYField = getField("y", "F");
    jScaleField = getField("scale", "F");
    jFixedLayerMarginLeft = getField("fixedLayerMarginLeft", "F");
    jFixedLayerMarginTop = getField("fixedLayerMarginTop", "F");
    jFixedLayerMarginRight = getField("fixedLayerMarginRight", "F");
    jFixedLayerMarginBottom = getField("fixedLayerMarginBottom", "F");
}

void
AndroidProgressiveUpdateData::InitProgressiveUpdateDataClass(JNIEnv *jEnv)
{
    initInit();

    jProgressiveUpdateDataClass = getClassGlobalRef("org/mozilla/gecko/gfx/ProgressiveUpdateData");

    jXField = getField("x", "F");
    jYField = getField("y", "F");
    jWidthField = getField("width", "F");
    jHeightField = getField("height", "F");
    jScaleField = getField("scale", "F");
    jShouldAbortField = getField("abort", "Z");
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
    jint *vals = jenv->GetIntArrayElements(jIntArray, NULL);
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
    jfloat *vals = jenv->GetFloatArrayElements(jFloatArray, NULL);
    for (int32_t i = 0; i < count; i++) {
        aVals.AppendElement(vals[i]);
    }
    jenv->ReleaseFloatArrayElements(jFloatArray, vals, JNI_ABORT);
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
AndroidGeckoEvent::ReadCharactersField(JNIEnv *jenv)
{
    jstring s = (jstring) jenv->GetObjectField(wrapped_obj, jCharactersField);
    if (!s) {
        mCharacters.SetIsVoid(true);
        return;
    }

    int len = jenv->GetStringLength(s);
    mCharacters.SetLength(len);
    jenv->GetStringRegion(s, 0, len, mCharacters.BeginWriting());
}

void
AndroidGeckoEvent::ReadCharactersExtraField(JNIEnv *jenv)
{
    jstring s = (jstring) jenv->GetObjectField(wrapped_obj, jCharactersExtraField);
    if (!s) {
        mCharactersExtra.SetIsVoid(true);
        return;
    }

    int len = jenv->GetStringLength(s);
    mCharactersExtra.SetLength(len);
    jenv->GetStringRegion(s, 0, len, mCharactersExtra.BeginWriting());
}

void
AndroidGeckoEvent::Init(int aType, nsIntRect const& aRect)
{
    mType = aType;
    mAckNeeded = false;
    mRect = aRect;
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
            mDomKeyLocation = jenv->GetIntField(jobj, jDomKeyLocationField);
            mFlags = jenv->GetIntField(jobj, jFlagsField);
            mKeyCode = jenv->GetIntField(jobj, jKeyCodeField);
            mUnicodeChar = jenv->GetIntField(jobj, jUnicodeCharField);
            mBaseUnicodeChar = jenv->GetIntField(jobj, jBaseUnicodeCharField);
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

        case DRAW:
            ReadRectField(jenv);
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
            mBandwidth = jenv->GetDoubleField(jobj, jBandwidthField);
            mCanBeMetered = jenv->GetBooleanField(jobj, jCanBeMeteredField);
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
AndroidGeckoEvent::Init(int aType, int aAction)
{
    mType = aType;
    mAckNeeded = false;
    mAction = aAction;
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
AndroidGeckoLayerClient::Init(jobject jobj)
{
    NS_ASSERTION(wrapped_obj == nullptr, "Init called on non-null wrapped_obj!");
    wrapped_obj = jobj;
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

void
AndroidViewTransform::Init(jobject jobj)
{
    NS_ABORT_IF_FALSE(wrapped_obj == nullptr, "Init called on non-null wrapped_obj!");
    wrapped_obj = jobj;
}

void
AndroidProgressiveUpdateData::Init(jobject jobj)
{
    NS_ABORT_IF_FALSE(wrapped_obj == nullptr, "Init called on non-null wrapped_obj!");
    wrapped_obj = jobj;
}

void
AndroidGeckoLayerClient::SetFirstPaintViewport(const nsIntPoint& aOffset, float aZoom, const nsIntRect& aPageRect, const gfx::Rect& aCssPageRect)
{
    NS_ASSERTION(!isNull(), "SetFirstPaintViewport called on null layer client!");
    JNIEnv *env = GetJNIForThread();    // this is called on the compositor thread
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    return env->CallVoidMethod(wrapped_obj, jSetFirstPaintViewport, (float)aOffset.x, (float)aOffset.y, aZoom,
                               (float)aPageRect.x, (float)aPageRect.y, (float)aPageRect.XMost(), (float)aPageRect.YMost(),
                               aCssPageRect.x, aCssPageRect.y, aCssPageRect.XMost(), aCssPageRect.YMost());
}

void
AndroidGeckoLayerClient::SetPageRect(const gfx::Rect& aCssPageRect)
{
    NS_ASSERTION(!isNull(), "SetPageRect called on null layer client!");
    JNIEnv *env = GetJNIForThread();    // this is called on the compositor thread
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    return env->CallVoidMethod(wrapped_obj, jSetPageRect,
                               aCssPageRect.x, aCssPageRect.y, aCssPageRect.XMost(), aCssPageRect.YMost());
}

void
AndroidGeckoLayerClient::SyncViewportInfo(const nsIntRect& aDisplayPort, float aDisplayResolution, bool aLayersUpdated,
                                          nsIntPoint& aScrollOffset, float& aScaleX, float& aScaleY,
                                          gfx::Margin& aFixedLayerMargins)
{
    NS_ASSERTION(!isNull(), "SyncViewportInfo called on null layer client!");
    JNIEnv *env = GetJNIForThread();    // this is called on the compositor thread
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env);

    jobject viewTransformJObj = env->CallObjectMethod(wrapped_obj, jSyncViewportInfoMethod,
                                                      aDisplayPort.x, aDisplayPort.y,
                                                      aDisplayPort.width, aDisplayPort.height,
                                                      aDisplayResolution, aLayersUpdated);
    if (jniFrame.CheckForException())
        return;

    NS_ABORT_IF_FALSE(viewTransformJObj, "No view transform object!");

    AndroidViewTransform viewTransform;
    viewTransform.Init(viewTransformJObj);

    aScrollOffset = nsIntPoint(viewTransform.GetX(env), viewTransform.GetY(env));
    aScaleX = aScaleY = viewTransform.GetScale(env);
    viewTransform.GetFixedLayerMargins(env, aFixedLayerMargins);
}

bool
AndroidGeckoLayerClient::ProgressiveUpdateCallback(bool aHasPendingNewThebesContent,
                                                   const gfx::Rect& aDisplayPort,
                                                   float aDisplayResolution,
                                                   bool aDrawingCritical,
                                                   gfx::Rect& aViewport,
                                                   float& aScaleX,
                                                   float& aScaleY)
{
    JNIEnv *env = AndroidBridge::GetJNIEnv();
    if (!env)
        return false;

    AutoJObject progressiveUpdateDataJObj(env, env->CallObjectMethod(wrapped_obj,
                                                                     jProgressiveUpdateCallbackMethod,
                                                                     aHasPendingNewThebesContent,
                                                                     (float)aDisplayPort.x,
                                                                     (float)aDisplayPort.y,
                                                                     (float)aDisplayPort.width,
                                                                     (float)aDisplayPort.height,
                                                                     aDisplayResolution,
                                                                     !aDrawingCritical));
    if (env->ExceptionCheck()) {
        env->ExceptionDescribe();
        env->ExceptionClear();
        return false;
    }

    NS_ABORT_IF_FALSE(progressiveUpdateDataJObj, "No progressive update data!");

    AndroidProgressiveUpdateData progressiveUpdateData(progressiveUpdateDataJObj);

    aViewport.x = progressiveUpdateData.GetX(env);
    aViewport.y = progressiveUpdateData.GetY(env);
    aViewport.width = progressiveUpdateData.GetWidth(env);
    aViewport.height = progressiveUpdateData.GetHeight(env);
    aScaleX = aScaleY = progressiveUpdateData.GetScale(env);

    return progressiveUpdateData.GetShouldAbort(env);
}

jobject ConvertToJavaViewportMetrics(JNIEnv* env, nsIAndroidViewport* metrics) {
    float x, y, width, height,
        pageLeft, pageTop, pageRight, pageBottom,
        cssPageLeft, cssPageTop, cssPageRight, cssPageBottom,
        zoom;
    metrics->GetX(&x);
    metrics->GetY(&y);
    metrics->GetWidth(&width);
    metrics->GetHeight(&height);
    metrics->GetPageLeft(&pageLeft);
    metrics->GetPageTop(&pageTop);
    metrics->GetPageRight(&pageRight);
    metrics->GetPageBottom(&pageBottom);
    metrics->GetCssPageLeft(&cssPageLeft);
    metrics->GetCssPageTop(&cssPageTop);
    metrics->GetCssPageRight(&cssPageRight);
    metrics->GetCssPageBottom(&cssPageBottom);
    metrics->GetZoom(&zoom);

    jobject jobj = env->NewObject(AndroidGeckoLayerClient::jViewportClass, AndroidGeckoLayerClient::jViewportCtor,
                                  pageLeft, pageTop, pageRight, pageBottom,
                                  cssPageLeft, cssPageTop, cssPageRight, cssPageBottom,
                                  x, y, x + width, y + height,
                                  zoom);
    return jobj;
}

class nsAndroidDisplayport : public nsIAndroidDisplayport
{
public:
    NS_DECL_ISUPPORTS
    virtual nsresult GetLeft(float *aLeft) { *aLeft = mLeft; return NS_OK; }
    virtual nsresult GetTop(float *aTop) { *aTop = mTop; return NS_OK; }
    virtual nsresult GetRight(float *aRight) { *aRight = mRight; return NS_OK; }
    virtual nsresult GetBottom(float *aBottom) { *aBottom = mBottom; return NS_OK; }
    virtual nsresult GetResolution(float *aResolution) { *aResolution = mResolution; return NS_OK; }
    virtual nsresult SetLeft(float aLeft) { mLeft = aLeft; return NS_OK; }
    virtual nsresult SetTop(float aTop) { mTop = aTop; return NS_OK; }
    virtual nsresult SetRight(float aRight) { mRight = aRight; return NS_OK; }
    virtual nsresult SetBottom(float aBottom) { mBottom = aBottom; return NS_OK; }
    virtual nsresult SetResolution(float aResolution) { mResolution = aResolution; return NS_OK; }

    nsAndroidDisplayport(AndroidRectF aRect, float aResolution):
        mLeft(aRect.Left()), mTop(aRect.Top()), mRight(aRect.Right()), mBottom(aRect.Bottom()), mResolution(aResolution) {}

private:
    ~nsAndroidDisplayport() {}
    float mLeft, mTop, mRight, mBottom, mResolution;
};

NS_IMPL_ISUPPORTS1(nsAndroidDisplayport, nsIAndroidDisplayport)

void createDisplayPort(AutoLocalJNIFrame *jniFrame, jobject jobj, nsIAndroidDisplayport** displayPort) {
    JNIEnv* env = jniFrame->GetEnv();
    AndroidRectF rect(env, env->GetObjectField(jobj, AndroidGeckoLayerClient::jDisplayportPosition));
    if (jniFrame->CheckForException()) return;
    float resolution = env->GetFloatField(jobj, AndroidGeckoLayerClient::jDisplayportResolution);
    if (jniFrame->CheckForException()) return;
    *displayPort = new nsAndroidDisplayport(rect, resolution);
}

void
AndroidGeckoLayerClient::GetDisplayPort(AutoLocalJNIFrame *jniFrame, bool aPageSizeUpdate, bool aIsBrowserContentDisplayed, int32_t tabId, nsIAndroidViewport* metrics, nsIAndroidDisplayport** displayPort)
{
    jobject jmetrics = ConvertToJavaViewportMetrics(jniFrame->GetEnv(), metrics);
    if (jniFrame->CheckForException()) return;
    if (!jmetrics)
        return;
    jobject jobj = jniFrame->GetEnv()->CallObjectMethod(wrapped_obj, jGetDisplayPort, aPageSizeUpdate, aIsBrowserContentDisplayed, tabId, jmetrics);
    if (jniFrame->CheckForException()) return;
    createDisplayPort(jniFrame, jobj, displayPort);
    (*displayPort)->AddRef();
}

bool
AndroidGeckoLayerClient::CreateFrame(AutoLocalJNIFrame *jniFrame, AndroidLayerRendererFrame& aFrame)
{
    if (!jniFrame || !jniFrame->GetEnv())
        return false;

    jobject frameJObj = jniFrame->GetEnv()->CallObjectMethod(wrapped_obj, jCreateFrameMethod);
    if (jniFrame->CheckForException())
        return false;
    NS_ABORT_IF_FALSE(frameJObj, "No frame object!");

    aFrame.Init(jniFrame->GetEnv(), frameJObj);
    return true;
}

bool
AndroidGeckoLayerClient::ActivateProgram(AutoLocalJNIFrame *jniFrame)
{
    if (!jniFrame || !jniFrame->GetEnv())
        return false;

    jniFrame->GetEnv()->CallVoidMethod(wrapped_obj, jActivateProgramMethod);
    if (jniFrame->CheckForException())
        return false;

    return true;
}

bool
AndroidGeckoLayerClient::DeactivateProgram(AutoLocalJNIFrame *jniFrame)
{
    if (!jniFrame || !jniFrame->GetEnv())
        return false;

    jniFrame->GetEnv()->CallVoidMethod(wrapped_obj, jDeactivateProgramMethod);
    if (jniFrame->CheckForException())
        return false;

    return true;
}

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

float
AndroidViewTransform::GetX(JNIEnv *env)
{
    if (!env)
        return 0.0f;
    return env->GetFloatField(wrapped_obj, jXField);
}

float
AndroidViewTransform::GetY(JNIEnv *env)
{
    if (!env)
        return 0.0f;
    return env->GetFloatField(wrapped_obj, jYField);
}

float
AndroidViewTransform::GetScale(JNIEnv *env)
{
    if (!env)
        return 0.0f;
    return env->GetFloatField(wrapped_obj, jScaleField);
}

void
AndroidViewTransform::GetFixedLayerMargins(JNIEnv *env, gfx::Margin &aFixedLayerMargins)
{
    if (!env)
        return;

    aFixedLayerMargins.top = env->GetFloatField(wrapped_obj, jFixedLayerMarginTop);
    aFixedLayerMargins.right = env->GetFloatField(wrapped_obj, jFixedLayerMarginRight);
    aFixedLayerMargins.bottom = env->GetFloatField(wrapped_obj, jFixedLayerMarginBottom);
    aFixedLayerMargins.left = env->GetFloatField(wrapped_obj, jFixedLayerMarginLeft);
}

float
AndroidProgressiveUpdateData::GetX(JNIEnv *env)
{
    if (!env)
        return 0.0f;
    return env->GetFloatField(wrapped_obj, jXField);
}

float
AndroidProgressiveUpdateData::GetY(JNIEnv *env)
{
    if (!env)
        return 0.0f;
    return env->GetFloatField(wrapped_obj, jYField);
}

float
AndroidProgressiveUpdateData::GetWidth(JNIEnv *env)
{
    if (!env)
        return 0.0f;
    return env->GetFloatField(wrapped_obj, jWidthField);
}

float
AndroidProgressiveUpdateData::GetHeight(JNIEnv *env)
{
    if (!env)
        return 0.0f;
    return env->GetFloatField(wrapped_obj, jHeightField);
}

float
AndroidProgressiveUpdateData::GetScale(JNIEnv *env)
{
    if (!env)
        return 0.0f;
    return env->GetFloatField(wrapped_obj, jScaleField);
}

bool
AndroidProgressiveUpdateData::GetShouldAbort(JNIEnv *env)
{
    if (!env)
        return false;
    return env->GetBooleanField(wrapped_obj, jShouldAbortField);
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
        if (!jni) {
            SetIsVoid(true);
            return;
        }
    }
    const jchar* jCharPtr = jni->GetStringChars(jstr, NULL);

    if (!jCharPtr) {
        SetIsVoid(true);
        return;
    }

    jsize len = jni->GetStringLength(jstr);

    if (len <= 0) {
        SetIsVoid(true);
    } else {
        Assign(jCharPtr, len);
    }
    jni->ReleaseStringChars(jstr, jCharPtr);
}
