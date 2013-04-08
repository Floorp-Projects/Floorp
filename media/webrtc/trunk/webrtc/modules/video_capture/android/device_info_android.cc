/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "device_info_android.h"

#include <stdio.h>

#include "ref_count.h"
#include "trace.h"
#include "video_capture_android.h"

#include "AndroidJNIWrapper.h"

namespace webrtc
{

namespace videocapturemodule
{

VideoCaptureModule::DeviceInfo*
VideoCaptureImpl::CreateDeviceInfo (const WebRtc_Word32 id) {
  videocapturemodule::DeviceInfoAndroid *deviceInfo =
      new videocapturemodule::DeviceInfoAndroid(id);
  if (deviceInfo && deviceInfo->Init() != 0) {
    delete deviceInfo;
    deviceInfo = NULL;
  }
  return deviceInfo;
}

DeviceInfoAndroid::DeviceInfoAndroid(const WebRtc_Word32 id) :
    DeviceInfoImpl(id) {
}

WebRtc_Word32 DeviceInfoAndroid::Init() {
  return 0;
}

DeviceInfoAndroid::~DeviceInfoAndroid() {
}

WebRtc_UWord32 DeviceInfoAndroid::NumberOfDevices() {
  JNIEnv *env;
  jclass javaCmDevInfoClass;
  jobject javaCmDevInfoObject;
  bool attached = false;
  if (VideoCaptureAndroid::AttachAndUseAndroidDeviceInfoObjects(
          env,
          javaCmDevInfoClass,
          javaCmDevInfoObject,
          attached) != 0)
    return 0;

  WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, _id,
               "%s GetMethodId", __FUNCTION__);
  // get the method ID for the Android Java GetDeviceUniqueName name.
  jmethodID cid = env->GetMethodID(javaCmDevInfoClass,
                                   "NumberOfDevices",
                                   "()I");

  jint numberOfDevices = 0;
  if (cid != NULL) {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideoCapture, _id,
                 "%s Calling Number of devices", __FUNCTION__);
    numberOfDevices = env->CallIntMethod(javaCmDevInfoObject, cid);
  }
  VideoCaptureAndroid::ReleaseAndroidDeviceInfoObjects(attached);

  if (numberOfDevices > 0)
    return numberOfDevices;
  return 0;
}

WebRtc_Word32 DeviceInfoAndroid::GetDeviceName(
    WebRtc_UWord32 deviceNumber,
    char* deviceNameUTF8,
    WebRtc_UWord32 deviceNameLength,
    char* deviceUniqueIdUTF8,
    WebRtc_UWord32 deviceUniqueIdUTF8Length,
    char* /*productUniqueIdUTF8*/,
    WebRtc_UWord32 /*productUniqueIdUTF8Length*/) {

  JNIEnv *env;
  jclass javaCmDevInfoClass;
  jobject javaCmDevInfoObject;
  WebRtc_Word32 result = 0;
  bool attached = false;
  if (VideoCaptureAndroid::AttachAndUseAndroidDeviceInfoObjects(
          env,
          javaCmDevInfoClass,
          javaCmDevInfoObject,
          attached)!= 0)
    return -1;

  // get the method ID for the Android Java GetDeviceUniqueName name.
  jmethodID cid = env->GetMethodID(javaCmDevInfoClass, "GetDeviceUniqueName",
                                   "(I)Ljava/lang/String;");
  if (cid != NULL) {
    jobject javaDeviceNameObj = env->CallObjectMethod(javaCmDevInfoObject,
                                                      cid, deviceNumber);
    if (javaDeviceNameObj == NULL) {
      WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                   "%s: Failed to get device name for device %d.",
                   __FUNCTION__, (int) deviceNumber);
      result = -1;
    } else {
      jboolean isCopy;
      const char* javaDeviceNameChar = env->GetStringUTFChars(
          (jstring) javaDeviceNameObj
          ,&isCopy);
      const jsize javaDeviceNameCharLength =
          env->GetStringUTFLength((jstring) javaDeviceNameObj);
      if ((WebRtc_UWord32) javaDeviceNameCharLength <
          deviceUniqueIdUTF8Length) {
        memcpy(deviceUniqueIdUTF8,
               javaDeviceNameChar,
               javaDeviceNameCharLength + 1);
      }
      else {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture,
                     _id, "%s: deviceUniqueIdUTF8 to short.",
                     __FUNCTION__);
        result = -1;
      }
      if ((WebRtc_UWord32) javaDeviceNameCharLength < deviceNameLength) {
        memcpy(deviceNameUTF8,
               javaDeviceNameChar,
               javaDeviceNameCharLength + 1);
      }
      env->ReleaseStringUTFChars((jstring) javaDeviceNameObj,
                                 javaDeviceNameChar);
    }  // javaDeviceNameObj == NULL

  }
  else {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, -1,
                 "%s: Failed to find GetDeviceUniqueName function id",
                 __FUNCTION__);
    result = -1;
  }

  VideoCaptureAndroid::ReleaseAndroidDeviceInfoObjects(attached);

  WEBRTC_TRACE(webrtc::kTraceStateInfo, webrtc::kTraceVideoCapture, -1,
               "%s: result %d", __FUNCTION__, (int) result);
  return result;

}

WebRtc_Word32 DeviceInfoAndroid::CreateCapabilityMap(
    const char* deviceUniqueIdUTF8) {
  MapItem* item = NULL;
  while ((item = _captureCapabilities.Last())) {
    delete (VideoCaptureCapability*) item->GetItem();
    _captureCapabilities.Erase(item);
  }

  JNIEnv *env;
  jclass javaCmDevInfoClass;
  jobject javaCmDevInfoObject;
  bool attached = false;
  if (VideoCaptureAndroid::AttachAndUseAndroidDeviceInfoObjects(
          env,
          javaCmDevInfoClass,
          javaCmDevInfoObject,
          attached) != 0)
    return -1;

  // Find the capability class
  jclass javaCapClass = jsjni_GetGlobalClassRef(AndroidJavaCaptureCapabilityClass);
  if (javaCapClass == NULL) {
    VideoCaptureAndroid::ReleaseAndroidDeviceInfoObjects(attached);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                 "%s: Can't find java class VideoCaptureCapabilityAndroid.",
                 __FUNCTION__);
    return -1;
  }

  // get the method ID for the Android Java GetCapabilityArray .
  char signature[256];
  sprintf(signature,
          "(Ljava/lang/String;)[L%s;",
          AndroidJavaCaptureCapabilityClass);
  jmethodID cid = env->GetMethodID(javaCmDevInfoClass,
                                   "GetCapabilityArray",
                                   signature);
  if (cid == NULL) {
    VideoCaptureAndroid::ReleaseAndroidDeviceInfoObjects(attached);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                 "%s: Can't find method GetCapabilityArray.", __FUNCTION__);
    return -1;
  }
  // Create a jstring so we can pass the deviceUniquName to the java method.
  jstring capureIdString = env->NewStringUTF((char*) deviceUniqueIdUTF8);

  if (capureIdString == NULL) {
    VideoCaptureAndroid::ReleaseAndroidDeviceInfoObjects(attached);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                 "%s: Can't create string for  method GetCapabilityArray.",
                 __FUNCTION__);
    return -1;
  }
  // Call the java class and get an array with capabilities back.
  jobject javaCapabilitiesObj = env->CallObjectMethod(javaCmDevInfoObject,
                                                      cid, capureIdString);
  if (!javaCapabilitiesObj) {
    VideoCaptureAndroid::ReleaseAndroidDeviceInfoObjects(attached);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                 "%s: Failed to call java GetCapabilityArray.",
                 __FUNCTION__);
    return -1;
  }

  jfieldID widthField = env->GetFieldID(javaCapClass, "width", "I");
  jfieldID heigtField = env->GetFieldID(javaCapClass, "height", "I");
  jfieldID maxFpsField = env->GetFieldID(javaCapClass, "maxFPS", "I");
  if (widthField == NULL || heigtField == NULL || maxFpsField == NULL) {
    VideoCaptureAndroid::ReleaseAndroidDeviceInfoObjects(attached);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                 "%s: Failed to get field Id.", __FUNCTION__);
    return -1;
  }

  const jsize numberOfCapabilities =
      env->GetArrayLength((jarray) javaCapabilitiesObj);

  for (jsize i = 0; i < numberOfCapabilities; ++i) {
    VideoCaptureCapability *cap = new VideoCaptureCapability();
    jobject capabilityElement = env->GetObjectArrayElement(
        (jobjectArray) javaCapabilitiesObj,
        i);

    cap->width = env->GetIntField(capabilityElement, widthField);
    cap->height = env->GetIntField(capabilityElement, heigtField);
    cap->expectedCaptureDelay = _expectedCaptureDelay;
    cap->rawType = kVideoNV21;
    cap->maxFPS = env->GetIntField(capabilityElement, maxFpsField);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                 "%s: Cap width %d, height %d, fps %d", __FUNCTION__,
                 cap->width, cap->height, cap->maxFPS);
    _captureCapabilities.Insert(i, cap);
  }

  _lastUsedDeviceNameLength = strlen((char*) deviceUniqueIdUTF8);
  _lastUsedDeviceName = (char*) realloc(_lastUsedDeviceName,
                                        _lastUsedDeviceNameLength + 1);
  memcpy(_lastUsedDeviceName,
         deviceUniqueIdUTF8,
         _lastUsedDeviceNameLength + 1);

  VideoCaptureAndroid::ReleaseAndroidDeviceInfoObjects(attached);
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideoCapture, _id,
               "CreateCapabilityMap %d", _captureCapabilities.Size());

  env->DeleteGlobalRef(javaCapClass);

  return _captureCapabilities.Size();
}

WebRtc_Word32 DeviceInfoAndroid::GetOrientation(
    const char* deviceUniqueIdUTF8,
    VideoCaptureRotation& orientation) {
  JNIEnv *env;
  jclass javaCmDevInfoClass;
  jobject javaCmDevInfoObject;
  bool attached = false;
  if (VideoCaptureAndroid::AttachAndUseAndroidDeviceInfoObjects(
          env,
          javaCmDevInfoClass,
          javaCmDevInfoObject,
          attached) != 0)
    return -1;

  // get the method ID for the Android Java GetOrientation .
  jmethodID cid = env->GetMethodID(javaCmDevInfoClass, "GetOrientation",
                                   "(Ljava/lang/String;)I");
  if (cid == NULL) {
    VideoCaptureAndroid::ReleaseAndroidDeviceInfoObjects(attached);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                 "%s: Can't find method GetOrientation.", __FUNCTION__);
    return -1;
  }
  // Create a jstring so we can pass the deviceUniquName to the java method.
  jstring capureIdString = env->NewStringUTF((char*) deviceUniqueIdUTF8);
  if (capureIdString == NULL) {
    VideoCaptureAndroid::ReleaseAndroidDeviceInfoObjects(attached);
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, _id,
                 "%s: Can't create string for  method GetCapabilityArray.",
                 __FUNCTION__);
    return -1;
  }
  // Call the java class and get the orientation.
  jint jorientation = env->CallIntMethod(javaCmDevInfoObject, cid,
                                         capureIdString);
  VideoCaptureAndroid::ReleaseAndroidDeviceInfoObjects(attached);

  WebRtc_Word32 retValue = 0;
  switch (jorientation) {
    case -1: // Error
      orientation = kCameraRotate0;
      retValue = -1;
      break;
    case 0:
      orientation = kCameraRotate0;
      break;
    case 90:
      orientation = kCameraRotate90;
      break;
    case 180:
      orientation = kCameraRotate180;
      break;
    case 270:
      orientation = kCameraRotate270;
      break;
    case 360:
      orientation = kCameraRotate0;
      break;
  }
  return retValue;
}

}  // namespace videocapturemodule
}  // namespace webrtc
