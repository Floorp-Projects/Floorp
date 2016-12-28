/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/android/audio_manager.h"

#include <utility>

#include <android/log.h>

#include "webrtc/base/arraysize.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/modules/audio_device/android/audio_common.h"
#include "webrtc/modules/utility/include/helpers_android.h"

#define TAG "AudioManager"
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

namespace webrtc {

// AudioManager::JavaAudioManager implementation
AudioManager::JavaAudioManager::JavaAudioManager(
    NativeRegistration* native_reg,
    rtc::scoped_ptr<GlobalRef> audio_manager)
    : audio_manager_(std::move(audio_manager)),
      init_(native_reg->GetMethodId("init", "()Z")),
      dispose_(native_reg->GetMethodId("dispose", "()V")),
      is_communication_mode_enabled_(
          native_reg->GetMethodId("isCommunicationModeEnabled", "()Z")),
      is_device_blacklisted_for_open_sles_usage_(
          native_reg->GetMethodId("isDeviceBlacklistedForOpenSLESUsage",
                                  "()Z")) {
  ALOGD("JavaAudioManager::ctor%s", GetThreadInfo().c_str());
}

AudioManager::JavaAudioManager::~JavaAudioManager() {
  ALOGD("JavaAudioManager::dtor%s", GetThreadInfo().c_str());
}

bool AudioManager::JavaAudioManager::Init() {
  return audio_manager_->CallBooleanMethod(init_);
}

void AudioManager::JavaAudioManager::Close() {
  audio_manager_->CallVoidMethod(dispose_);
}

bool AudioManager::JavaAudioManager::IsCommunicationModeEnabled() {
  return audio_manager_->CallBooleanMethod(is_communication_mode_enabled_);
}

bool AudioManager::JavaAudioManager::IsDeviceBlacklistedForOpenSLESUsage() {
  return audio_manager_->CallBooleanMethod(
      is_device_blacklisted_for_open_sles_usage_);
}

// AudioManager implementation
AudioManager::AudioManager()
    : j_environment_(JVM::GetInstance()->environment()),
      audio_layer_(AudioDeviceModule::kPlatformDefaultAudio),
      initialized_(false),
      hardware_aec_(false),
      hardware_agc_(false),
      hardware_ns_(false),
      low_latency_playout_(false),
      delay_estimate_in_milliseconds_(0) {
  ALOGD("ctor%s", GetThreadInfo().c_str());
  RTC_CHECK(j_environment_);
  JNINativeMethod native_methods[] = {
      {"nativeCacheAudioParameters",
       "(IIZZZZIIJ)V",
       reinterpret_cast<void*>(&webrtc::AudioManager::CacheAudioParameters)}};
  j_native_registration_ = j_environment_->RegisterNatives(
      "org/webrtc/voiceengine/WebRtcAudioManager",
      native_methods, arraysize(native_methods));
  j_audio_manager_.reset(new JavaAudioManager(
      j_native_registration_.get(),
      j_native_registration_->NewObject(
          "<init>", "(Landroid/content/Context;J)V",
          JVM::GetInstance()->context(), PointerTojlong(this))));
}

AudioManager::~AudioManager() {
  ALOGD("~dtor%s", GetThreadInfo().c_str());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  Close();
}

void AudioManager::SetActiveAudioLayer(
    AudioDeviceModule::AudioLayer audio_layer) {
  ALOGD("SetActiveAudioLayer(%d)%s", audio_layer, GetThreadInfo().c_str());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(!initialized_);
  // Store the currenttly utilized audio layer.
  audio_layer_ = audio_layer;
  // The delay estimate can take one of two fixed values depending on if the
  // device supports low-latency output or not. However, it is also possible
  // that the user explicitly selects the high-latency audio path, hence we use
  // the selected |audio_layer| here to set the delay estimate.
  delay_estimate_in_milliseconds_ =
      (audio_layer == AudioDeviceModule::kAndroidJavaAudio) ?
      kHighLatencyModeDelayEstimateInMilliseconds :
      kLowLatencyModeDelayEstimateInMilliseconds;
  ALOGD("delay_estimate_in_milliseconds: %d", delay_estimate_in_milliseconds_);
}

bool AudioManager::Init() {
  ALOGD("Init%s", GetThreadInfo().c_str());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  RTC_DCHECK(!initialized_);
  RTC_DCHECK_NE(audio_layer_, AudioDeviceModule::kPlatformDefaultAudio);
  if (!j_audio_manager_->Init()) {
    ALOGE("init failed!");
    return false;
  }
  initialized_ = true;
  return true;
}

bool AudioManager::Close() {
  ALOGD("Close%s", GetThreadInfo().c_str());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_)
    return true;
  j_audio_manager_->Close();
  initialized_ = false;
  return true;
}

bool AudioManager::IsCommunicationModeEnabled() const {
  ALOGD("IsCommunicationModeEnabled()");
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return j_audio_manager_->IsCommunicationModeEnabled();
}

bool AudioManager::IsAcousticEchoCancelerSupported() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return hardware_aec_;
}

bool AudioManager::IsAutomaticGainControlSupported() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return hardware_agc_;
}

bool AudioManager::IsNoiseSuppressorSupported() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return hardware_ns_;
}

bool AudioManager::IsLowLatencyPlayoutSupported() const {
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  ALOGD("IsLowLatencyPlayoutSupported()");
  // Some devices are blacklisted for usage of OpenSL ES even if they report
  // that low-latency playout is supported. See b/21485703 for details.
  return j_audio_manager_->IsDeviceBlacklistedForOpenSLESUsage() ?
      false : low_latency_playout_;
}

int AudioManager::GetDelayEstimateInMilliseconds() const {
  return delay_estimate_in_milliseconds_;
}

void JNICALL AudioManager::CacheAudioParameters(JNIEnv* env,
                                                jobject obj,
                                                jint sample_rate,
                                                jint channels,
                                                jboolean hardware_aec,
                                                jboolean hardware_agc,
                                                jboolean hardware_ns,
                                                jboolean low_latency_output,
                                                jint output_buffer_size,
                                                jint input_buffer_size,
                                                jlong native_audio_manager) {
  webrtc::AudioManager* this_object =
      reinterpret_cast<webrtc::AudioManager*>(native_audio_manager);
  this_object->OnCacheAudioParameters(
      env, sample_rate, channels, hardware_aec, hardware_agc, hardware_ns,
      low_latency_output, output_buffer_size, input_buffer_size);
}

void AudioManager::OnCacheAudioParameters(JNIEnv* env,
                                          jint sample_rate,
                                          jint channels,
                                          jboolean hardware_aec,
                                          jboolean hardware_agc,
                                          jboolean hardware_ns,
                                          jboolean low_latency_output,
                                          jint output_buffer_size,
                                          jint input_buffer_size) {
  ALOGD("OnCacheAudioParameters%s", GetThreadInfo().c_str());
  ALOGD("hardware_aec: %d", hardware_aec);
  ALOGD("hardware_agc: %d", hardware_agc);
  ALOGD("hardware_ns: %d", hardware_ns);
  ALOGD("low_latency_output: %d", low_latency_output);
  ALOGD("sample_rate: %d", sample_rate);
  ALOGD("channels: %d", channels);
  ALOGD("output_buffer_size: %d", output_buffer_size);
  ALOGD("input_buffer_size: %d", input_buffer_size);
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  hardware_aec_ = hardware_aec;
  hardware_agc_ = hardware_agc;
  hardware_ns_ = hardware_ns;
  low_latency_playout_ = low_latency_output;
  // TODO(henrika): add support for stereo output.
  playout_parameters_.reset(sample_rate, static_cast<size_t>(channels),
                            static_cast<size_t>(output_buffer_size));
  record_parameters_.reset(sample_rate, static_cast<size_t>(channels),
                           static_cast<size_t>(input_buffer_size));
}

const AudioParameters& AudioManager::GetPlayoutAudioParameters() {
  RTC_CHECK(playout_parameters_.is_valid());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return playout_parameters_;
}

const AudioParameters& AudioManager::GetRecordAudioParameters() {
  RTC_CHECK(record_parameters_.is_valid());
  RTC_DCHECK(thread_checker_.CalledOnValidThread());
  return record_parameters_;
}

}  // namespace webrtc
