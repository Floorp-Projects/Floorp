/* -*- Mode: c++; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
jfieldID AndroidGeckoEvent::jAlphaField = 0;
jfieldID AndroidGeckoEvent::jBetaField = 0;
jfieldID AndroidGeckoEvent::jGammaField = 0;
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
jfieldID AndroidGeckoEvent::jOffsetField = 0;
jfieldID AndroidGeckoEvent::jCountField = 0;
jfieldID AndroidGeckoEvent::jPointerIndexField = 0;
jfieldID AndroidGeckoEvent::jRangeTypeField = 0;
jfieldID AndroidGeckoEvent::jRangeStylesField = 0;
jfieldID AndroidGeckoEvent::jRangeForeColorField = 0;
jfieldID AndroidGeckoEvent::jRangeBackColorField = 0;
jfieldID AndroidGeckoEvent::jLocationField = 0;
jfieldID AndroidGeckoEvent::jAddressField = 0;
jfieldID AndroidGeckoEvent::jBandwidthField = 0;
jfieldID AndroidGeckoEvent::jCanBeMeteredField = 0;

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

jclass AndroidAddress::jAddressClass = 0;
jmethodID AndroidAddress::jGetAddressLineMethod;
jmethodID AndroidAddress::jGetAdminAreaMethod;
jmethodID AndroidAddress::jGetCountryNameMethod;
jmethodID AndroidAddress::jGetFeatureNameMethod;
jmethodID AndroidAddress::jGetLocalityMethod;
jmethodID AndroidAddress::jGetPostalCodeMethod;
jmethodID AndroidAddress::jGetPremisesMethod;
jmethodID AndroidAddress::jGetSubAdminAreaMethod;
jmethodID AndroidAddress::jGetSubLocalityMethod;
jmethodID AndroidAddress::jGetSubThoroughfareMethod;
jmethodID AndroidAddress::jGetThoroughfareMethod;

jclass AndroidGeckoLayerClient::jGeckoLayerClientClass = 0;
jmethodID AndroidGeckoLayerClient::jBeginDrawingMethod = 0;
jmethodID AndroidGeckoLayerClient::jEndDrawingMethod = 0;
jmethodID AndroidGeckoLayerClient::jSetFirstPaintViewport = 0;
jmethodID AndroidGeckoLayerClient::jSetPageSize = 0;
jmethodID AndroidGeckoLayerClient::jGetViewTransformMethod = 0;
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
    AndroidAddress::InitAddressClass(jEnv);
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
    jAlphaField = getField("mAlpha", "D");
    jBetaField = getField("mBeta", "D");
    jGammaField = getField("mGamma", "D");
    jXField = getField("mX", "D");
    jYField = getField("mY", "D");
    jZField = getField("mZ", "D");
    jDistanceField = getField("mDistance", "D");
    jRectField = getField("mRect", "Landroid/graphics/Rect;");

    jCharactersField = getField("mCharacters", "Ljava/lang/String;");
    jCharactersExtraField = getField("mCharactersExtra", "Ljava/lang/String;");
    jKeyCodeField = getField("mKeyCode", "I");
    jMetaStateField = getField("mMetaState", "I");
    jFlagsField = getField("mFlags", "I");
    jUnicodeCharField = getField("mUnicodeChar", "I");
    jOffsetField = getField("mOffset", "I");
    jCountField = getField("mCount", "I");
    jPointerIndexField = getField("mPointerIndex", "I");
    jRangeTypeField = getField("mRangeType", "I");
    jRangeStylesField = getField("mRangeStyles", "I");
    jRangeForeColorField = getField("mRangeForeColor", "I");
    jRangeBackColorField = getField("mRangeBackColor", "I");
    jLocationField = getField("mLocation", "Landroid/location/Location;");
    jAddressField = getField("mAddress", "Landroid/location/Address;");
    jBandwidthField = getField("mBandwidth", "D");
    jCanBeMeteredField = getField("mCanBeMetered", "Z");
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

void
AndroidAddress::InitAddressClass(JNIEnv *jEnv)
{
    initInit();

    jAddressClass = getClassGlobalRef("android/location/Address");

    jGetAddressLineMethod = getMethod("getAddressLine", "(I)Ljava/lang/String;");
    jGetAdminAreaMethod = getMethod("getAdminArea", "()Ljava/lang/String;");
    jGetCountryNameMethod = getMethod("getCountryName", "()Ljava/lang/String;");
    jGetFeatureNameMethod = getMethod("getFeatureName", "()Ljava/lang/String;");
    jGetLocalityMethod  = getMethod("getLocality", "()Ljava/lang/String;");
    jGetPostalCodeMethod = getMethod("getPostalCode", "()Ljava/lang/String;");
    jGetPremisesMethod = getMethod("getPremises", "()Ljava/lang/String;");
    jGetSubAdminAreaMethod = getMethod("getSubAdminArea", "()Ljava/lang/String;");
    jGetSubLocalityMethod = getMethod("getSubLocality", "()Ljava/lang/String;");
    jGetSubThoroughfareMethod = getMethod("getSubThoroughfare", "()Ljava/lang/String;");
    jGetThoroughfareMethod = getMethod("getThoroughfare", "()Ljava/lang/String;");
}

nsGeoPositionAddress*
AndroidAddress::CreateGeoPositionAddress(JNIEnv *jenv, jobject jobj)
{
    nsJNIString streetNumber(static_cast<jstring>(jenv->CallObjectMethod(jobj, jGetSubThoroughfareMethod)), jenv);
    nsJNIString street(static_cast<jstring>(jenv->CallObjectMethod(jobj, jGetThoroughfareMethod)), jenv);
    nsJNIString city(static_cast<jstring>(jenv->CallObjectMethod(jobj, jGetLocalityMethod)), jenv);
    nsJNIString county(static_cast<jstring>(jenv->CallObjectMethod(jobj, jGetSubAdminAreaMethod)), jenv);
    nsJNIString country(static_cast<jstring>(jenv->CallObjectMethod(jobj, jGetCountryNameMethod)), jenv);
    nsJNIString premises(static_cast<jstring>(jenv->CallObjectMethod(jobj, jGetPremisesMethod)), jenv);
    nsJNIString postalCode(static_cast<jstring>(jenv->CallObjectMethod(jobj, jGetPostalCodeMethod)), jenv);
    nsJNIString region(static_cast<jstring>(jenv->CallObjectMethod(jobj, jGetAdminAreaMethod, 0)), jenv);

#ifdef DEBUG
    printf_stderr("!!!!!!!!!!!!!! AndroidAddress::CreateGeoPositionAddress:\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n\t%s\n",
                  NS_LossyConvertUTF16toASCII(streetNumber).get(),
                  NS_LossyConvertUTF16toASCII(street).get(),
                  NS_LossyConvertUTF16toASCII(premises).get(),
                  NS_LossyConvertUTF16toASCII(city).get(),
                  NS_LossyConvertUTF16toASCII(county).get(),
                  NS_LossyConvertUTF16toASCII(region).get(),
                  NS_LossyConvertUTF16toASCII(country).get(),
                  NS_LossyConvertUTF16toASCII(postalCode).get());
#endif

    return new nsGeoPositionAddress(streetNumber,
                                    street,
                                    premises,
                                    city,
                                    county,
                                    region,
                                    country,
                                    postalCode);
}

nsGeoPosition*
AndroidLocation::CreateGeoPosition(JNIEnv *jenv, jobject jobj)
{
    double latitude  = jenv->CallDoubleMethod(jobj, jGetLatitudeMethod);
    double longitude = jenv->CallDoubleMethod(jobj, jGetLongitudeMethod);
    double altitude  = jenv->CallDoubleMethod(jobj, jGetAltitudeMethod);
    float  accuracy  = jenv->CallFloatMethod (jobj, jGetAccuracyMethod);
    float  bearing   = jenv->CallFloatMethod (jobj, jGetBearingMethod);
    float  speed     = jenv->CallFloatMethod (jobj, jGetSpeedMethod);
    long long time   = jenv->CallLongMethod  (jobj, jGetTimeMethod);

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

    jBeginDrawingMethod = getMethod("beginDrawing", "(IILjava/lang/String;)Z");
    jEndDrawingMethod = getMethod("endDrawing", "()V");
    jSetFirstPaintViewport = getMethod("setFirstPaintViewport", "(FFFFF)V");
    jSetPageSize = getMethod("setPageSize", "(FFF)V");
    jGetViewTransformMethod = getMethod("getViewTransform",
                                        "()Lorg/mozilla/gecko/gfx/ViewTransform;");
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
        mCharactersExtra.SetIsVoid(PR_TRUE);
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

        case ORIENTATION_EVENT:
            mAlpha = jenv->GetDoubleField(jobj, jAlphaField);
            mBeta = jenv->GetDoubleField(jobj, jBetaField);
            mGamma = jenv->GetDoubleField(jobj, jGammaField);
            break;

       case ACCELERATION_EVENT:
            mX = jenv->GetDoubleField(jobj, jXField);
            mY = jenv->GetDoubleField(jobj, jYField);
            mZ = jenv->GetDoubleField(jobj, jZField);
            break;

        case LOCATION_EVENT: {
            jobject location = jenv->GetObjectField(jobj, jLocationField);
            jobject address  = jenv->GetObjectField(jobj, jAddressField);

            mGeoPosition = AndroidLocation::CreateGeoPosition(jenv, location);
            if (address)
                mGeoAddress = AndroidAddress::CreateGeoPositionAddress(jenv, address);
            break;
        }

        case LOAD_URI: {
            ReadCharactersField(jenv);
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

        case PROXIMITY_EVENT: {
            mDistance = jenv->GetDoubleField(jobj, jDistanceField);
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
            ReadPointArray(mPoints, jenv, jPoints, 2);
        }

        default:
            break;
    }

#ifndef DEBUG_ANDROID_EVENTS
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

    // Register the view transform getter.
    AndroidBridge::Bridge()->SetViewTransformGetter(mViewTransformGetter);
}

void
AndroidLayerRendererFrame::Init(jobject jobj)
{
    if (!isNull()) {
        Dispose();
    }

    wrapped_obj = GetJNIForThread()->NewGlobalRef(jobj);
}

void
AndroidLayerRendererFrame::Dispose()
{
    if (isNull()) {
        return;
    }

    GetJNIForThread()->DeleteGlobalRef(wrapped_obj);
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

    return env->CallIntMethod(wrapped_obj, jBeginDrawingMethod);
}

void
AndroidGeckoSurfaceView::EndDrawing()
{
    JNIEnv *env = AndroidBridge::GetJNIEnv();
    if (!env)
        return;

    env->CallVoidMethod(wrapped_obj, jEndDrawingMethod);
}

void
AndroidGeckoSurfaceView::Draw2D(jobject bitmap, int width, int height)
{
    JNIEnv *env = AndroidBridge::GetJNIEnv();
    if (!env)
        return;

    env->CallVoidMethod(wrapped_obj, jDraw2DBitmapMethod, bitmap, width, height);
}

void
AndroidGeckoSurfaceView::Draw2D(jobject buffer, int stride)
{
    JNIEnv *env = AndroidBridge::GetJNIEnv();
    if (!env)
        return;

    env->CallVoidMethod(wrapped_obj, jDraw2DBufferMethod, buffer, stride);
}

bool
AndroidGeckoLayerClient::BeginDrawing(int aWidth, int aHeight, const nsAString &aMetadata)
{
    NS_ASSERTION(!isNull(), "BeginDrawing() called on null layer client!");
    JNIEnv *env = AndroidBridge::GetJNIEnv();
    if (!env)
        return false;

    AndroidBridge::AutoLocalJNIFrame jniFrame(env);
    jstring jMetadata = env->NewString(nsPromiseFlatString(aMetadata).get(), aMetadata.Length());

    return env->CallBooleanMethod(wrapped_obj, jBeginDrawingMethod, aWidth, aHeight, jMetadata);
}

void
AndroidGeckoLayerClient::EndDrawing()
{
    NS_ASSERTION(!isNull(), "EndDrawing() called on null layer client!");
    JNIEnv *env = AndroidBridge::GetJNIEnv();
    if (!env)
        return;

    AndroidBridge::AutoLocalJNIFrame jniFrame(env);
    return env->CallVoidMethod(wrapped_obj, jEndDrawingMethod);
}

void
AndroidGeckoLayerClient::SetFirstPaintViewport(float aOffsetX, float aOffsetY, float aZoom, float aPageWidth, float aPageHeight)
{
    NS_ASSERTION(!isNull(), "SetFirstPaintViewport called on null layer client!");
    JNIEnv *env = GetJNIForThread();
    if (!env)
        return;

    AndroidBridge::AutoLocalJNIFrame jniFrame(env);
    return env->CallVoidMethod(wrapped_obj, jSetFirstPaintViewport, aOffsetX, aOffsetY, aZoom, aPageWidth, aPageHeight);
}

void
AndroidGeckoLayerClient::SetPageSize(float aZoom, float aPageWidth, float aPageHeight)
{
    NS_ASSERTION(!isNull(), "SetPageSize called on null layer client!");
    JNIEnv *env = GetJNIForThread();
    if (!env)
        return;

    AndroidBridge::AutoLocalJNIFrame jniFrame(env);
    return env->CallVoidMethod(wrapped_obj, jSetPageSize, aZoom, aPageWidth, aPageHeight);
}

jobject
AndroidGeckoSurfaceView::GetSoftwareDrawBitmap()
{
    JNIEnv *env = AndroidBridge::GetJNIEnv();
    if (!env)
        return nsnull;

    return env->CallObjectMethod(wrapped_obj, jGetSoftwareDrawBitmapMethod);
}

jobject
AndroidGeckoSurfaceView::GetSoftwareDrawBuffer()
{
    JNIEnv *env = AndroidBridge::GetJNIEnv();
    if (!env)
        return nsnull;

    return env->CallObjectMethod(wrapped_obj, jGetSoftwareDrawBufferMethod);
}

jobject
AndroidGeckoSurfaceView::GetSurface()
{
    JNIEnv *env = AndroidBridge::GetJNIEnv();
    if (!env)
        return nsnull;

    return env->CallObjectMethod(wrapped_obj, jGetSurfaceMethod);
}

jobject
AndroidGeckoSurfaceView::GetSurfaceHolder()
{
    return GetJNIForThread()->CallObjectMethod(wrapped_obj, jGetHolderMethod);
}

void
AndroidGeckoLayerClient::GetViewTransform(AndroidViewTransform& aViewTransform)
{
    JNIEnv *env = GetJNIForThread();
    NS_ABORT_IF_FALSE(env, "No JNI environment at GetViewTransform()!");
    if (!env) {
        return;
    }

    jobject viewTransformJObj = env->CallObjectMethod(wrapped_obj, jGetViewTransformMethod);
    NS_ABORT_IF_FALSE(viewTransformJObj, "No view transform object!");
    aViewTransform.Init(viewTransformJObj);
}

void
AndroidGeckoLayerClient::CreateFrame(AndroidLayerRendererFrame& aFrame)
{
    JNIEnv *env = GetJNIForThread();
    NS_ABORT_IF_FALSE(env, "No JNI environment at CreateFrame()!");
    if (!env) {
        return;
    }

    jobject frameJObj = env->CallObjectMethod(wrapped_obj, jCreateFrameMethod);
    NS_ABORT_IF_FALSE(frameJObj, "No frame object!");
    aFrame.Init(frameJObj);
}

void
AndroidGeckoLayerClient::ActivateProgram()
{
    JNIEnv *env = GetJNIForThread();
    NS_ABORT_IF_FALSE(env, "No JNI environment at ActivateProgram()!");
    if (!env) {
        return;
    }

    env->CallVoidMethod(wrapped_obj, jActivateProgramMethod);
}

void
AndroidGeckoLayerClient::DeactivateProgram()
{
    JNIEnv *env = GetJNIForThread();
    NS_ABORT_IF_FALSE(env, "No JNI environment at DeactivateProgram()!");
    if (!env) {
        return;
    }

    env->CallVoidMethod(wrapped_obj, jDeactivateProgramMethod);
}

void
AndroidLayerRendererFrame::BeginDrawing()
{
    JNIEnv *env = GetJNIForThread();
    NS_ABORT_IF_FALSE(env, "No JNI environment at BeginDrawing()!");
    if (!env) {
        return;
    }

    env->CallVoidMethod(wrapped_obj, jBeginDrawingMethod);
}

void
AndroidLayerRendererFrame::DrawBackground()
{
    JNIEnv *env = GetJNIForThread();
    NS_ABORT_IF_FALSE(env, "No JNI environment at DrawBackground()!");
    if (!env) {
        return;
    }

    env->CallVoidMethod(wrapped_obj, jDrawBackgroundMethod);
}

void
AndroidLayerRendererFrame::DrawForeground()
{
    JNIEnv *env = GetJNIForThread();
    NS_ABORT_IF_FALSE(env, "No JNI environment at DrawForeground()!");
    if (!env) {
        return;
    }

    env->CallVoidMethod(wrapped_obj, jDrawForegroundMethod);
}

void
AndroidLayerRendererFrame::EndDrawing()
{
    JNIEnv *env = GetJNIForThread();
    NS_ABORT_IF_FALSE(env, "No JNI environment at EndDrawing()!");
    if (!env) {
        return;
    }

    env->CallVoidMethod(wrapped_obj, jEndDrawingMethod);
}

float
AndroidViewTransform::GetX()
{
    JNIEnv *env = GetJNIForThread();
    if (!env)
        return 0.0f;
    return env->GetFloatField(wrapped_obj, jXField);
}

float
AndroidViewTransform::GetY()
{
    JNIEnv *env = GetJNIForThread();
    if (!env)
        return 0.0f;
    return env->GetFloatField(wrapped_obj, jYField);
}

float
AndroidViewTransform::GetScale()
{
    JNIEnv *env = GetJNIForThread();
    if (!env)
        return 0.0f;
    return env->GetFloatField(wrapped_obj, jScaleField);
}

void
AndroidGeckoLayerClientViewTransformGetter::operator()(nsIntPoint& aScrollOffset, float& aScaleX,
                                                         float& aScaleY)
{
    AndroidViewTransform viewTransform;

    AndroidBridge::AutoLocalJNIFrame jniFrame(GetJNIForThread());
    mLayerClient.GetViewTransform(viewTransform);

    aScrollOffset = nsIntPoint(viewTransform.GetX(), viewTransform.GetY());
    aScaleX = aScaleY = viewTransform.GetScale();
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
    if (!jni)
        jni = AndroidBridge::GetJNIEnv();
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
