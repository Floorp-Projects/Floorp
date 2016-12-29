/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_capture/android/device_info_android.h"

#include <algorithm>
#include <string>
#include <sstream>
#include <vector>

#include "webrtc/modules/utility/include/helpers_android.h"
#include "webrtc/modules/video_capture/android/video_capture_android.h"
#include "webrtc/system_wrappers/include/logging.h"
#include "webrtc/system_wrappers/include/ref_count.h"
#include "webrtc/system_wrappers/include/trace.h"

#include "AndroidJNIWrapper.h"

namespace webrtc {

namespace videocapturemodule {

// Helper for storing lists of pairs of ints.  Used e.g. for resolutions & FPS
// ranges.
typedef std::pair<int, int> IntPair;
typedef std::vector<IntPair> IntPairs;

static std::string IntPairsToString(const IntPairs& pairs, char separator) {
  std::stringstream stream;
  for (size_t i = 0; i < pairs.size(); ++i) {
    if (i > 0) {
      stream << ", ";
    }
    stream << "(" << pairs[i].first << separator << pairs[i].second << ")";
  }
  return stream.str();
}

struct AndroidCameraInfo {
  std::string name;
  bool front_facing;
  int orientation;
  IntPairs resolutions;  // Pairs are: (width,height).
  // Pairs are (min,max) in units of FPS*1000 ("milli-frame-per-second").
  IntPairs mfpsRanges;

  std::string ToString() {
    std::stringstream stream;
    stream << "Name: [" << name << "], MFPS ranges: ["
           << IntPairsToString(mfpsRanges, ':')
           << "], front_facing: " << front_facing
           << ", orientation: " << orientation << ", resolutions: ["
           << IntPairsToString(resolutions, 'x') << "]";
    return stream.str();
  }
};

// Camera info; populated during DeviceInfoAndroid::Refresh()
static std::vector<AndroidCameraInfo>* g_camera_info = NULL;

static JavaVM* g_jvm_dev_info = NULL;

// Set |*index| to the index of |name| in g_camera_info or return false if no
// match found.
static bool FindCameraIndexByName(const std::string& name, size_t* index) {
  for (size_t i = 0; i < g_camera_info->size(); ++i) {
    if (g_camera_info->at(i).name == name) {
      *index = i;
      return true;
    }
  }
  return false;
}

// Returns a pointer to the named member of g_camera_info, or NULL if no match
// is found.
static AndroidCameraInfo* FindCameraInfoByName(const std::string& name) {
  size_t index = 0;
  if (FindCameraIndexByName(name, &index)) {
    return &g_camera_info->at(index);
  }
  return NULL;
}

// static
void DeviceInfoAndroid::Initialize(JavaVM* javaVM) {
  // TODO(henrike): this "if" would make a lot more sense as an assert, but
  // Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_GetVideoEngine() and
  // Java_org_webrtc_videoengineapp_ViEAndroidJavaAPI_Terminate() conspire to
  // prevent this.  Once that code is made to only
  // VideoEngine::SetAndroidObjects() once per process, this can turn into an
  // assert.
  if (g_camera_info) {
    return;
  }

  g_jvm_dev_info = javaVM;
  BuildDeviceList();
}

void DeviceInfoAndroid::BuildDeviceList() {
  if (!g_jvm_dev_info) {
    return;
  }

  AttachThreadScoped ats(g_jvm_dev_info);
  JNIEnv* jni = ats.env();

  g_camera_info = new std::vector<AndroidCameraInfo>();
  jclass j_info_class =
    jsjni_GetGlobalClassRef("org/webrtc/videoengine/VideoCaptureDeviceInfoAndroid");
  jclass j_cap_class =
    jsjni_GetGlobalClassRef("org/webrtc/videoengine/CaptureCapabilityAndroid");
  assert(j_info_class);
  jmethodID j_initialize = jni->GetStaticMethodID(
    j_info_class, "getDeviceInfo",
    "()[Lorg/webrtc/videoengine/CaptureCapabilityAndroid;");
  jarray j_camera_caps = static_cast<jarray>(
    jni->CallStaticObjectMethod(j_info_class, j_initialize));

  const jsize capLength = jni->GetArrayLength(j_camera_caps);

  jfieldID widthField = jni->GetFieldID(j_cap_class, "width", "[I");
  jfieldID heightField = jni->GetFieldID(j_cap_class, "height", "[I");
  jfieldID maxFpsField = jni->GetFieldID(j_cap_class, "maxMilliFPS", "I");
  jfieldID minFpsField = jni->GetFieldID(j_cap_class, "minMilliFPS", "I");
  jfieldID orientationField = jni->GetFieldID(j_cap_class, "orientation", "I");
  jfieldID frontFacingField = jni->GetFieldID(j_cap_class, "frontFacing", "Z");
  jfieldID nameField =
      jni->GetFieldID(j_cap_class, "name", "Ljava/lang/String;");
  if (widthField == NULL
      || heightField == NULL
      || maxFpsField == NULL
      || minFpsField == NULL
      || orientationField == NULL
      || frontFacingField == NULL
      || nameField == NULL) {
    WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoCapture, -1,
                 "%s: Failed to get field Id.", __FUNCTION__);
    return;
  }

  for (jsize i = 0; i < capLength; i++) {
    jobject capabilityElement = jni->GetObjectArrayElement(
        (jobjectArray) j_camera_caps,
        i);

    AndroidCameraInfo info;
    jstring camName = static_cast<jstring>(jni->GetObjectField(capabilityElement,
                                                               nameField));
    const char* camChars = jni->GetStringUTFChars(camName, nullptr);
    info.name = std::string(camChars);
    jni->ReleaseStringUTFChars(camName, camChars);

    info.orientation = jni->GetIntField(capabilityElement, orientationField);
    info.front_facing = jni->GetBooleanField(capabilityElement, frontFacingField);
    jint min_mfps = jni->GetIntField(capabilityElement, minFpsField);
    jint max_mfps = jni->GetIntField(capabilityElement, maxFpsField);

    jintArray widthResArray =
        static_cast<jintArray>(jni->GetObjectField(capabilityElement, widthField));
    jintArray heightResArray =
        static_cast<jintArray>(jni->GetObjectField(capabilityElement, heightField));

    const jsize numRes = jni->GetArrayLength(widthResArray);

    jint *widths = jni->GetIntArrayElements(widthResArray, nullptr);
    jint *heights = jni->GetIntArrayElements(heightResArray, nullptr);

    for (jsize j = 0; j < numRes; ++j) {
        info.resolutions.push_back(std::make_pair(widths[j], heights[j]));
    }

    info.mfpsRanges.push_back(std::make_pair(min_mfps, max_mfps));
    g_camera_info->push_back(info);

    jni->ReleaseIntArrayElements(widthResArray, widths, JNI_ABORT);
    jni->ReleaseIntArrayElements(heightResArray, heights, JNI_ABORT);
  }

  jni->DeleteGlobalRef(j_info_class);
  jni->DeleteGlobalRef(j_cap_class);
}

void DeviceInfoAndroid::DeInitialize() {
  if (g_camera_info) {
    delete g_camera_info;
    g_camera_info = NULL;
  }
}

int32_t DeviceInfoAndroid::Refresh() {
  if (!g_camera_info || g_camera_info->size() == 0) {
    DeviceInfoAndroid::BuildDeviceList();
  }
  return 0;
}

VideoCaptureModule::DeviceInfo* VideoCaptureImpl::CreateDeviceInfo(
    const int32_t id) {
  return new videocapturemodule::DeviceInfoAndroid(id);
}

DeviceInfoAndroid::DeviceInfoAndroid(const int32_t id) :
    DeviceInfoImpl(id) {
}

DeviceInfoAndroid::~DeviceInfoAndroid() {
}

bool DeviceInfoAndroid::FindCameraIndex(const char* deviceUniqueIdUTF8,
                                        size_t* index) {
  return FindCameraIndexByName(deviceUniqueIdUTF8, index);
}

int32_t DeviceInfoAndroid::Init() {
  return 0;
}

uint32_t DeviceInfoAndroid::NumberOfDevices() {
  return g_camera_info->size();
}

int32_t DeviceInfoAndroid::GetDeviceName(
    uint32_t deviceNumber,
    char* deviceNameUTF8,
    uint32_t deviceNameLength,
    char* deviceUniqueIdUTF8,
    uint32_t deviceUniqueIdUTF8Length,
    char* /*productUniqueIdUTF8*/,
    uint32_t /*productUniqueIdUTF8Length*/,
    pid_t* /*pid*/) {
  if (deviceNumber >= g_camera_info->size()) {
    return -1;
  }
  const AndroidCameraInfo& info = g_camera_info->at(deviceNumber);
  if (info.name.length() + 1 > deviceNameLength ||
      info.name.length() + 1 > deviceUniqueIdUTF8Length) {
    return -1;
  }
  memcpy(deviceNameUTF8, info.name.c_str(), info.name.length() + 1);
  memcpy(deviceUniqueIdUTF8, info.name.c_str(), info.name.length() + 1);
  return 0;
}

int32_t DeviceInfoAndroid::CreateCapabilityMap(
    const char* deviceUniqueIdUTF8) {
  _captureCapabilities.clear();
  const AndroidCameraInfo* info = FindCameraInfoByName(deviceUniqueIdUTF8);
  if (info == NULL) {
    return -1;
  }

  for (size_t i = 0; i < info->resolutions.size(); ++i) {
    for (size_t j = 0; j < info->mfpsRanges.size(); ++j) {
      const IntPair& size = info->resolutions[i];
      const IntPair& mfpsRange = info->mfpsRanges[j];
      VideoCaptureCapability cap;
      cap.width = size.first;
      cap.height = size.second;
      cap.maxFPS = mfpsRange.second / 1000;
      cap.expectedCaptureDelay = kExpectedCaptureDelay;
      cap.rawType = kVideoNV21;
      _captureCapabilities.push_back(cap);
    }
  }
  return _captureCapabilities.size();
}

int32_t DeviceInfoAndroid::GetOrientation(const char* deviceUniqueIdUTF8,
                                          VideoRotation& orientation) {
  const AndroidCameraInfo* info = FindCameraInfoByName(deviceUniqueIdUTF8);
  if (info == NULL ||
      VideoCaptureImpl::RotationFromDegrees(info->orientation,
                                            &orientation) != 0) {
    return -1;
  }
  return 0;
}

void DeviceInfoAndroid::GetMFpsRange(const char* deviceUniqueIdUTF8,
                                     int max_fps_to_match,
                                     int* min_mfps, int* max_mfps) {
  const AndroidCameraInfo* info = FindCameraInfoByName(deviceUniqueIdUTF8);
  if (info == NULL) {
    return;
  }
  int desired_mfps = max_fps_to_match * 1000;
  int best_diff_mfps = 0;
  LOG(LS_INFO) << "Search for best target mfps " << desired_mfps;
  // Search for best fps range with preference shifted to constant fps modes.
  for (size_t i = 0; i < info->mfpsRanges.size(); ++i) {
    int diff_mfps = abs(info->mfpsRanges[i].first - desired_mfps) +
        abs(info->mfpsRanges[i].second - desired_mfps) +
        (info->mfpsRanges[i].second - info->mfpsRanges[i].first) / 2;
    LOG(LS_INFO) << "Fps range " << info->mfpsRanges[i].first << ":" <<
        info->mfpsRanges[i].second << ". Distance: " << diff_mfps;
    if (i == 0 || diff_mfps < best_diff_mfps) {
      best_diff_mfps = diff_mfps;
      *min_mfps = info->mfpsRanges[i].first;
      *max_mfps = info->mfpsRanges[i].second;
    }
  }
}

}  // namespace videocapturemodule
}  // namespace webrtc
