/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "AndroidJavaWrappers.h"
#include "AndroidBridge.h"

using namespace mozilla;

jclass AndroidGeckoEvent::jGeckoEventClass = 0;
jfieldID AndroidGeckoEvent::jActionField = 0;
jfieldID AndroidGeckoEvent::jTypeField = 0;
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
jfieldID AndroidGeckoEvent::jFlagsField = 0;
jfieldID AndroidGeckoEvent::jUnicodeCharField = 0;
jfieldID AndroidGeckoEvent::jRepeatCountField = 0;
jfieldID AndroidGeckoEvent::jOffsetField = 0;
jfieldID AndroidGeckoEvent::jCountField = 0;
jfieldID AndroidGeckoEvent::jPointerIndexField = 0;
jfieldID AndroidGeckoEvent::jRangeTypeField = 0;
jfieldID AndroidGeckoEvent::jRangeStylesField = 0;
jfieldID AndroidGeckoEvent::jRangeForeColorField = 0;
jfieldID AndroidGeckoEvent::jRangeBackColorField = 0;
jfieldID AndroidGeckoEvent::jLocationField = 0;
jfieldID AndroidGeckoEvent::jBandwidthField = 0;
jfieldID AndroidGeckoEvent::jCanBeMeteredField = 0;
jfieldID AndroidGeckoEvent::jScreenOrientationField = 0;

jclass AndroidPoint::jPointClass = 0;
jfieldID AndroidPoint::jXField = 0;
jfieldID AndroidPoint::jYField = 0;

jclass AndroidRect::jRectClass = 0;
jfieldID AndroidRect::jBottomField = 0;
jfieldID AndroidRect::jLeftField = 0;
jfieldID AndroidRect::jRightField = 0;
jfieldID AndroidRect::jTopField = 0;

jclass AndroidLocation::jLocationClass = 0;
jmethodID AndroidLocation::jGetLatitudeMethod = 0;
jmethodID AndroidLocation::jGetLongitudeMethod = 0;
jmethodID AndroidLocation::jGetAltitudeMethod = 0;
jmethodID AndroidLocation::jGetAccuracyMethod = 0;
jmethodID AndroidLocation::jGetBearingMethod = 0;
jmethodID AndroidLocation::jGetSpeedMethod = 0;
jmethodID AndroidLocation::jGetTimeMethod = 0;

jclass AndroidGeckoLayerClient::jGeckoLayerClientClass = 0;
jmethodID AndroidGeckoLayerClient::jSetFirstPaintViewport = 0;
jmethodID AndroidGeckoLayerClient::jSetPageSize = 0;
jmethodID AndroidGeckoLayerClient::jSyncViewportInfoMethod = 0;
jmethodID AndroidGeckoLayerClient::jCreateFrameMethod = 0;
jmethodID AndroidGeckoLayerClient::jActivateProgramMethod = 0;
jmethodID AndroidGeckoLayerClient::jDeactivateProgramMethod = 0;

jclass AndroidLayerRendererFrame::jLayerRendererFrameClass = 0;
jmethodID AndroidLayerRendererFrame::jBeginDrawingMethod = 0;
jmethodID AndroidLayerRendererFrame::jDrawBackgroundMethod = 0;
jmethodID AndroidLayerRendererFrame::jDrawForegroundMethod = 0;
jmethodID AndroidLayerRendererFrame::jEndDrawingMethod = 0;

jclass AndroidViewTransform::jViewTransformClass = 0;
jfieldID AndroidViewTransform::jXField = 0;
jfieldID AndroidViewTransform::jYField = 0;
jfieldID AndroidViewTransform::jScaleField = 0;

jclass AndroidGeckoSurfaceView::jGeckoSurfaceViewClass = 0;
jmethodID AndroidGeckoSurfaceView::jBeginDrawingMethod = 0;
jmethodID AndroidGeckoSurfaceView::jEndDrawingMethod = 0;
jmethodID AndroidGeckoSurfaceView::jDraw2DBitmapMethod = 0;
jmethodID AndroidGeckoSurfaceView::jDraw2DBufferMethod = 0;
jmethodID AndroidGeckoSurfaceView::jGetSoftwareDrawBitmapMethod = 0;
jmethodID AndroidGeckoSurfaceView::jGetSoftwareDrawBufferMethod = 0;
jmethodID AndroidGeckoSurfaceView::jGetSurfaceMethod = 0;
jmethodID AndroidGeckoSurfaceView::jGetHolderMethod = 0;

#define initInit() jclass jClass

// note that this also sets jClass
#define getClassGlobalRef(cname) \
    (jClass = jclass(jEnv->NewGlobalRef(jEnv->FindClass(cname))))

#define getField(fname, ftype) \
    ((jfieldID) jEnv->GetFieldID(jClass, fname, ftype))

#define getMethod(fname, ftype) \
    ((jmethodID) jEnv->GetMethodID(jClass, fname, ftype))

void
mozilla::InitAndroidJavaWrappers(JNIEnv *jEnv)
{
    AndroidGeckoEvent::InitGeckoEventClass(jEnv);
    AndroidPoint::InitPointClass(jEnv);
    AndroidLocation::InitLocationClass(jEnv);
    AndroidRect::InitRectClass(jEnv);
    AndroidGeckoLayerClient::InitGeckoLayerClientClass(jEnv);
    AndroidLayerRendererFrame::InitLayerRendererFrameClass(jEnv);
    AndroidViewTransform::InitViewTransformClass(jEnv);
    AndroidGeckoSurfaceView::InitGeckoSurfaceViewClass(jEnv);
}

void
AndroidGeckoEvent::InitGeckoEventClass(JNIEnv *jEnv)
{
    initInit();

    jGeckoEventClass = getClassGlobalRef("org/mozilla/gecko/GeckoEvent");

    jActionField = getField("mAction", "I");
    jTypeField = getField("mType", "I");
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
    jFlagsField = getField("mFlags", "I");
    jUnicodeCharField = getField("mUnicodeChar", "I");
    jRepeatCountField = getField("mRepeatCount", "I");
    jOffsetField = getField("mOffset", "I");
    jCountField = getField("mCount", "I");
    jPointerIndexField = getField("mPointerIndex", "I");
    jRangeTypeField = getField("mRangeType", "I");
    jRangeStylesField = getField("mRangeStyles", "I");
    jRangeForeColorField = getField("mRangeForeColor", "I");
    jRangeBackColorField = getField("mRangeBackColor", "I");
    jLocationField = getField("mLocation", "Landroid/location/Location;");
    jBandwidthField = getField("mBandwidth", "D");
    jCanBeMeteredField = getField("mCanBeMetered", "Z");
    jScreenOrientationField = getField("mScreenOrientation", "S");
}

void
AndroidGeckoSurfaceView::InitGeckoSurfaceViewClass(JNIEnv *jEnv)
{
#ifndef MOZ_JAVA_COMPOSITOR
    initInit();

    jGeckoSurfaceViewClass = getClassGlobalRef("org/mozilla/gecko/GeckoSurfaceView");

    jBeginDrawingMethod = getMethod("beginDrawing", "()I");
    jGetSoftwareDrawBitmapMethod = getMethod("getSoftwareDrawBitmap", "()Landroid/graphics/Bitmap;");
    jGetSoftwareDrawBufferMethod = getMethod("getSoftwareDrawBuffer", "()Ljava/nio/ByteBuffer;");
    jEndDrawingMethod = getMethod("endDrawing", "()V");
    jDraw2DBitmapMethod = getMethod("draw2D", "(Landroid/graphics/Bitmap;II)V");
    jDraw2DBufferMethod = getMethod("draw2D", "(Ljava/nio/ByteBuffer;I)V");
    jGetSurfaceMethod = getMethod("getSurface", "()Landroid/view/Surface;");
    jGetHolderMethod = getMethod("getHolder", "()Landroid/view/SurfaceHolder;");
#endif
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
AndroidGeckoLayerClient::InitGeckoLayerClientClass(JNIEnv *jEnv)
{
#ifdef MOZ_JAVA_COMPOSITOR
    initInit();

    jGeckoLayerClientClass = getClassGlobalRef("org/mozilla/gecko/gfx/GeckoLayerClient");

    jSetFirstPaintViewport = getMethod("setFirstPaintViewport", "(FFFFFFF)V");
    jSetPageSize = getMethod("setPageSize", "(FFFFF)V");
    jSyncViewportInfoMethod = getMethod("syncViewportInfo",
                                        "(IIIIFZ)Lorg/mozilla/gecko/gfx/ViewTransform;");
    jCreateFrameMethod = getMethod("createFrame", "()Lorg/mozilla/gecko/gfx/LayerRenderer$Frame;");
    jActivateProgramMethod = getMethod("activateProgram", "()V");
    jDeactivateProgramMethod = getMethod("deactivateProgram", "()V");
#endif
}

void
AndroidLayerRendererFrame::InitLayerRendererFrameClass(JNIEnv *jEnv)
{
#ifdef MOZ_JAVA_COMPOSITOR
    initInit();

    jLayerRendererFrameClass = getClassGlobalRef("org/mozilla/gecko/gfx/LayerRenderer$Frame");

    jBeginDrawingMethod = getMethod("beginDrawing", "()V");
    jDrawBackgroundMethod = getMethod("drawBackground", "()V");
    jDrawForegroundMethod = getMethod("drawForeground", "()V");
    jEndDrawingMethod = getMethod("endDrawing", "()V");
#endif
}

void
AndroidViewTransform::InitViewTransformClass(JNIEnv *jEnv)
{
#ifdef MOZ_JAVA_COMPOSITOR
    initInit();

    jViewTransformClass = getClassGlobalRef("org/mozilla/gecko/gfx/ViewTransform");

    jXField = getField("x", "F");
    jYField = getField("y", "F");
    jScaleField = getField("scale", "F");
#endif
}

#undef initInit
#undef initClassGlobalRef
#undef getField
#undef getMethod

void
AndroidGeckoEvent::ReadPointArray(nsTArray<nsIntPoint> &points,
                                  JNIEnv *jenv,
                                  jfieldID field,
                                  PRUint32 count)
{
    jobjectArray jObjArray = (jobjectArray)jenv->GetObjectField(wrapped_obj, field);
    for (PRInt32 i = 0; i < count; i++) {
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
                                PRUint32 count)
{
    jintArray jIntArray = (jintArray)jenv->GetObjectField(wrapped_obj, field);
    jint *vals = jenv->GetIntArrayElements(jIntArray, false);
    for (PRInt32 i = 0; i < count; i++) {
        aVals.AppendElement(vals[i]);
    }
    jenv->ReleaseIntArrayElements(jIntArray, vals, JNI_ABORT);
}

void
AndroidGeckoEvent::ReadFloatArray(nsTArray<float> &aVals,
                                  JNIEnv *jenv,
                                  jfieldID field,
                                  PRUint32 count)
{
    jfloatArray jFloatArray = (jfloatArray)jenv->GetObjectField(wrapped_obj, field);
    jfloat *vals = jenv->GetFloatArrayElements(jFloatArray, false);
    for (PRInt32 i = 0; i < count; i++) {
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

    switch (mType) {
        case SIZE_CHANGED:
            ReadPointArray(mPoints, jenv, jPoints, 2);
            break;

        case KEY_EVENT:
            mTime = jenv->GetLongField(jobj, jTimeField);
            mMetaState = jenv->GetIntField(jobj, jMetaStateField);
            mFlags = jenv->GetIntField(jobj, jFlagsField);
            mKeyCode = jenv->GetIntField(jobj, jKeyCodeField);
            mUnicodeChar = jenv->GetIntField(jobj, jUnicodeCharField);
            mRepeatCount = jenv->GetIntField(jobj, jRepeatCountField);
            ReadCharactersField(jenv);
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
            if (mAction == IME_GET_TEXT || mAction == IME_SET_SELECTION) {
                mOffset = jenv->GetIntField(jobj, jOffsetField);
                mCount = jenv->GetIntField(jobj, jCountField);
            } else if (mAction == IME_SET_TEXT || mAction == IME_ADD_RANGE) {
                if (mAction == IME_SET_TEXT)
                    ReadCharactersField(jenv);
                mOffset = jenv->GetIntField(jobj, jOffsetField);
                mCount = jenv->GetIntField(jobj, jCountField);
                mRangeType = jenv->GetIntField(jobj, jRangeTypeField);
                mRangeStyles = jenv->GetIntField(jobj, jRangeStylesField);
                mRangeForeColor =
                    jenv->GetIntField(jobj, jRangeForeColorField);
                mRangeBackColor =
                    jenv->GetIntField(jobj, jRangeBackColorField);
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

        case ACTIVITY_STOPPING:
        case ACTIVITY_START:
        case ACTIVITY_PAUSING:
        case ACTIVITY_RESUMING: {
            mFlags = jenv->GetIntField(jobj, jFlagsField);
            break;
        }

        case SCREENSHOT: {
            mMetaState = jenv->GetIntField(jobj, jMetaStateField);
            mFlags = jenv->GetIntField(jobj, jFlagsField);
            ReadPointArray(mPoints, jenv, jPoints, 4);
            break;
        }

        case PAINT_LISTEN_START_EVENT: {
            mMetaState = jenv->GetIntField(jobj, jMetaStateField);
            break;
        }

        case SCREENORIENTATION_CHANGED: {
            mScreenOrientation = jenv->GetShortField(jobj, jScreenOrientationField);
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

void
AndroidGeckoEvent::Init(int x1, int y1, int x2, int y2)
{
    mType = DRAW;
    mRect.SetEmpty();
}

void
AndroidGeckoEvent::Init(AndroidGeckoEvent *aResizeEvent)
{
    NS_ASSERTION(aResizeEvent->Type() == SIZE_CHANGED, "Init called on non-SIZE_CHANGED event");

    mType = FORCED_RESIZE;
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
    NS_ASSERTION(wrapped_obj == nsnull, "Init called on non-null wrapped_obj!");
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
    NS_ABORT_IF_FALSE(wrapped_obj == nsnull, "Init called on non-null wrapped_obj!");
    wrapped_obj = jobj;
}

void
AndroidGeckoSurfaceView::Init(jobject jobj)
{
    NS_ASSERTION(wrapped_obj == nsnull, "Init called on non-null wrapped_obj!");

    wrapped_obj = jobj;
}

int
AndroidGeckoSurfaceView::BeginDrawing()
{
    NS_ASSERTION(!isNull(), "BeginDrawing called on null surfaceview!");

    JNIEnv *env = AndroidBridge::GetJNIEnv();
    if (!env)
        return 0;

    AutoLocalJNIFrame jniFrame(env, 0);

    int ret = env->CallIntMethod(wrapped_obj, jBeginDrawingMethod);
    if (jniFrame.CheckForException())
        return 0;

    return ret;
}

void
AndroidGeckoSurfaceView::EndDrawing()
{
    JNIEnv *env = AndroidBridge::GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallVoidMethod(wrapped_obj, jEndDrawingMethod);
}

void
AndroidGeckoSurfaceView::Draw2D(jobject bitmap, int width, int height)
{
    JNIEnv *env = AndroidBridge::GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallVoidMethod(wrapped_obj, jDraw2DBitmapMethod, bitmap, width, height);
}

void
AndroidGeckoSurfaceView::Draw2D(jobject buffer, int stride)
{
    JNIEnv *env = AndroidBridge::GetJNIEnv();
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    env->CallVoidMethod(wrapped_obj, jDraw2DBufferMethod, buffer, stride);
}

void
AndroidGeckoLayerClient::SetFirstPaintViewport(float aOffsetX, float aOffsetY, float aZoom, float aPageWidth, float aPageHeight,
                                               float aCssPageWidth, float aCssPageHeight)
{
    NS_ASSERTION(!isNull(), "SetFirstPaintViewport called on null layer client!");
    JNIEnv *env = GetJNIForThread();    // this is called on the compositor thread
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    return env->CallVoidMethod(wrapped_obj, jSetFirstPaintViewport, aOffsetX, aOffsetY, aZoom, aPageWidth, aPageHeight,
                               aCssPageWidth, aCssPageHeight);
}

void
AndroidGeckoLayerClient::SetPageSize(float aZoom, float aPageWidth, float aPageHeight, float aCssPageWidth, float aCssPageHeight)
{
    NS_ASSERTION(!isNull(), "SetPageSize called on null layer client!");
    JNIEnv *env = GetJNIForThread();    // this is called on the compositor thread
    if (!env)
        return;

    AutoLocalJNIFrame jniFrame(env, 0);
    return env->CallVoidMethod(wrapped_obj, jSetPageSize, aZoom, aPageWidth, aPageHeight, aCssPageWidth, aCssPageHeight);
}

void
AndroidGeckoLayerClient::SyncViewportInfo(const nsIntRect& aDisplayPort, float aDisplayResolution, bool aLayersUpdated,
                                          nsIntPoint& aScrollOffset, float& aScaleX, float& aScaleY)
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
}

jobject
AndroidGeckoSurfaceView::GetSoftwareDrawBitmap(AutoLocalJNIFrame *jniFrame)
{
    if (!jniFrame || !jniFrame->GetEnv())
        return nsnull;

    jobject ret = jniFrame->GetEnv()->CallObjectMethod(wrapped_obj, jGetSoftwareDrawBitmapMethod);
    if (jniFrame->CheckForException())
        return nsnull;

    return ret;
}

jobject
AndroidGeckoSurfaceView::GetSoftwareDrawBuffer(AutoLocalJNIFrame *jniFrame)
{
    if (!jniFrame || !jniFrame->GetEnv())
        return nsnull;

    jobject ret = jniFrame->GetEnv()->CallObjectMethod(wrapped_obj, jGetSoftwareDrawBufferMethod);
    if (jniFrame->CheckForException())
        return nsnull;

    return ret;
}

jobject
AndroidGeckoSurfaceView::GetSurface(AutoLocalJNIFrame *jniFrame)
{
    if (!jniFrame || !jniFrame->GetEnv())
        return nsnull;

    jobject ret = jniFrame->GetEnv()->CallObjectMethod(wrapped_obj, jGetSurfaceMethod);
    if (jniFrame->CheckForException())
        return nsnull;

    return ret;
}

jobject
AndroidGeckoSurfaceView::GetSurfaceHolder(AutoLocalJNIFrame *jniFrame)
{
    if (!jniFrame || !jniFrame->GetEnv())
        return nsnull;

    jobject ret = jniFrame->GetEnv()->CallObjectMethod(wrapped_obj, jGetHolderMethod);
    if (jniFrame->CheckForException())
        return nsnull;

    return ret;
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
AndroidRect::Init(JNIEnv *jenv, jobject jobj)
{
    NS_ASSERTION(wrapped_obj == nsnull, "Init called on non-null wrapped_obj!");

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
