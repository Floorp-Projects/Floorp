/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_device/android/audio_manager.h"
#include "webrtc/modules/audio_device/android/audio_track_jni.h"

#include <android/log.h>

#include "webrtc/base/arraysize.h"
#include "webrtc/base/checks.h"

#define TAG "AudioTrackJni"
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)

namespace webrtc {

static JavaVM* g_jvm = NULL;
static jobject g_context = NULL;
static jclass g_audio_track_class = NULL;

void AudioTrackJni::SetAndroidAudioDeviceObjects(void* jvm, void* context) {
  ALOGD("SetAndroidAudioDeviceObjects%s", GetThreadInfo().c_str());

  CHECK(jvm);
  CHECK(context);

  g_jvm = reinterpret_cast<JavaVM*>(jvm);
  JNIEnv* jni = GetEnv(g_jvm);
  CHECK(jni) << "AttachCurrentThread must be called on this tread";

  g_context = NewGlobalRef(jni, reinterpret_cast<jobject>(context));
  jclass local_class = FindClass(
      jni, "org/webrtc/voiceengine/WebRtcAudioTrack");
  g_audio_track_class = reinterpret_cast<jclass>(
      NewGlobalRef(jni, local_class));
  jni->DeleteLocalRef(local_class);
  CHECK_EXCEPTION(jni);

  // Register native methods with the WebRtcAudioTrack class. These methods
  // are declared private native in WebRtcAudioTrack.java.
  JNINativeMethod native_methods[] = {
      {"nativeCacheDirectBufferAddress", "(Ljava/nio/ByteBuffer;J)V",
          reinterpret_cast<void*>(
       &webrtc::AudioTrackJni::CacheDirectBufferAddress)},
      {"nativeGetPlayoutData", "(IJ)V",
          reinterpret_cast<void*>(&webrtc::AudioTrackJni::GetPlayoutData)}};
  jni->RegisterNatives(g_audio_track_class,
                       native_methods, arraysize(native_methods));
  CHECK_EXCEPTION(jni) << "Error during RegisterNatives";
}

// TODO(henrika): figure out if it is required to call this method? If so,
// ensure that is is always called as part of the destruction phase.
void AudioTrackJni::ClearAndroidAudioDeviceObjects() {
  ALOGD("ClearAndroidAudioDeviceObjects%s", GetThreadInfo().c_str());
  JNIEnv* jni = GetEnv(g_jvm);
  CHECK(jni) << "AttachCurrentThread must be called on this tread";
  jni->UnregisterNatives(g_audio_track_class);
  CHECK_EXCEPTION(jni) << "Error during UnregisterNatives";
  DeleteGlobalRef(jni, g_audio_track_class);
  g_audio_track_class = NULL;
  DeleteGlobalRef(jni, g_context);
  g_context = NULL;
  g_jvm = NULL;
}

// TODO(henrika): possible extend usage of AudioManager and add it as member.
AudioTrackJni::AudioTrackJni(AudioManager* audio_manager)
    : audio_parameters_(audio_manager->GetPlayoutAudioParameters()),
      j_audio_track_(NULL),
      direct_buffer_address_(NULL),
      direct_buffer_capacity_in_bytes_(0),
      frames_per_buffer_(0),
      initialized_(false),
      playing_(false),
      audio_device_buffer_(NULL),
      delay_in_milliseconds_(0) {
  ALOGD("ctor%s", GetThreadInfo().c_str());
  DCHECK(audio_parameters_.is_valid());
  CHECK(HasDeviceObjects());
  CreateJavaInstance();
  // Detach from this thread since we want to use the checker to verify calls
  // from the Java based audio thread.
  thread_checker_java_.DetachFromThread();
}

AudioTrackJni::~AudioTrackJni() {
  ALOGD("~dtor%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  Terminate();
  AttachThreadScoped ats(g_jvm);
  JNIEnv* jni = ats.env();
  jni->DeleteGlobalRef(j_audio_track_);
  j_audio_track_ = NULL;
}

int32_t AudioTrackJni::Init() {
  ALOGD("Init%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  return 0;
}

int32_t AudioTrackJni::Terminate() {
  ALOGD("Terminate%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  StopPlayout();
  return 0;
}

int32_t AudioTrackJni::InitPlayout() {
  ALOGD("InitPlayout%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(!initialized_);
  DCHECK(!playing_);
  if (initialized_ || playing_) {
    return -1;
  }
  AttachThreadScoped ats(g_jvm);
  JNIEnv* jni = ats.env();
  jmethodID initPlayoutID = GetMethodID(
      jni, g_audio_track_class, "InitPlayout", "(II)I");
  jint delay_in_milliseconds = jni->CallIntMethod(
      j_audio_track_, initPlayoutID, audio_parameters_.sample_rate(),
      audio_parameters_.channels());
  CHECK_EXCEPTION(jni);
  if (delay_in_milliseconds < 0) {
    ALOGE("InitPlayout failed!");
    return -1;
  }
  delay_in_milliseconds_ = delay_in_milliseconds;
  ALOGD("delay_in_milliseconds: %d", delay_in_milliseconds);
  initialized_ = true;
  return 0;
}

int32_t AudioTrackJni::StartPlayout() {
  ALOGD("StartPlayout%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(initialized_);
  DCHECK(!playing_);
  if (!initialized_ || playing_) {
    return -1;
  }
  AttachThreadScoped ats(g_jvm);
  JNIEnv* jni = ats.env();
  jmethodID startPlayoutID = GetMethodID(
      jni, g_audio_track_class, "StartPlayout", "()Z");
  jboolean res = jni->CallBooleanMethod(j_audio_track_, startPlayoutID);
  CHECK_EXCEPTION(jni);
  if (!res) {
    ALOGE("StartPlayout failed!");
    return -1;
  }
  playing_ = true;
  return 0;
}

int32_t AudioTrackJni::StopPlayout() {
  ALOGD("StopPlayout%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!initialized_ || !playing_) {
    return 0;
  }
  AttachThreadScoped ats(g_jvm);
  JNIEnv* jni = ats.env();
  jmethodID stopPlayoutID = GetMethodID(
      jni, g_audio_track_class, "StopPlayout", "()Z");
  jboolean res = jni->CallBooleanMethod(j_audio_track_, stopPlayoutID);
  CHECK_EXCEPTION(jni);
  if (!res) {
    ALOGE("StopPlayout failed!");
    return -1;
  }
  // If we don't detach here, we will hit a DCHECK in OnDataIsRecorded() next
  // time StartRecording() is called since it will create a new Java thread.
  thread_checker_java_.DetachFromThread();
  initialized_ = false;
  playing_ = false;
  return 0;
}

int AudioTrackJni::SpeakerVolumeIsAvailable(bool& available) {
  available = true;
  return 0;
}

int AudioTrackJni::SetSpeakerVolume(uint32_t volume) {
  ALOGD("SetSpeakerVolume(%d)%s", volume, GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  AttachThreadScoped ats(g_jvm);
  JNIEnv* jni = ats.env();
  jmethodID setStreamVolume = GetMethodID(
      jni, g_audio_track_class, "SetStreamVolume", "(I)Z");
  jboolean res = jni->CallBooleanMethod(
      j_audio_track_, setStreamVolume, volume);
  CHECK_EXCEPTION(jni);
  return res ? 0 : -1;
}

int AudioTrackJni::MaxSpeakerVolume(uint32_t& max_volume) const {
  ALOGD("MaxSpeakerVolume%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  AttachThreadScoped ats(g_jvm);
  JNIEnv* jni = ats.env();
  jmethodID getStreamMaxVolume = GetMethodID(
      jni, g_audio_track_class, "GetStreamMaxVolume", "()I");
  jint max_vol = jni->CallIntMethod(j_audio_track_, getStreamMaxVolume);
  CHECK_EXCEPTION(jni);
  max_volume = max_vol;
  return 0;
}

int AudioTrackJni::MinSpeakerVolume(uint32_t& min_volume) const {
  ALOGD("MaxSpeakerVolume%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  min_volume = 0;
  return 0;
}

int AudioTrackJni::SpeakerVolume(uint32_t& volume) const {
  ALOGD("SpeakerVolume%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  AttachThreadScoped ats(g_jvm);
  JNIEnv* jni = ats.env();
  jmethodID getStreamVolume = GetMethodID(
      jni, g_audio_track_class, "GetStreamVolume", "()I");
  jint stream_volume = jni->CallIntMethod(j_audio_track_, getStreamVolume);
  CHECK_EXCEPTION(jni);
  volume = stream_volume;
  return 0;
}

// TODO(henrika): possibly add stereo support.
void AudioTrackJni::AttachAudioBuffer(AudioDeviceBuffer* audioBuffer) {
  ALOGD("AttachAudioBuffer%s", GetThreadInfo().c_str());
  DCHECK(thread_checker_.CalledOnValidThread());
  audio_device_buffer_ = audioBuffer;
  const int sample_rate_hz = audio_parameters_.sample_rate();
  ALOGD("SetPlayoutSampleRate(%d)", sample_rate_hz);
  audio_device_buffer_->SetPlayoutSampleRate(sample_rate_hz);
  const int channels = audio_parameters_.channels();
  ALOGD("SetPlayoutChannels(%d)", channels);
  audio_device_buffer_->SetPlayoutChannels(channels);
}

int32_t AudioTrackJni::PlayoutDelay(uint16_t& delayMS) const {
  // No need for thread check or locking since we set |delay_in_milliseconds_|
  // only once  (on the creating thread) during initialization.
  delayMS = delay_in_milliseconds_;
  return 0;
}

int AudioTrackJni::PlayoutDelayMs() {
  // This method can be called from the Java based AudioRecordThread but we
  // don't need locking since it is only set once (on the main thread) during
  // initialization.
  return delay_in_milliseconds_;
}

void JNICALL AudioTrackJni::CacheDirectBufferAddress(
    JNIEnv* env, jobject obj, jobject byte_buffer, jlong nativeAudioTrack) {
  webrtc::AudioTrackJni* this_object =
      reinterpret_cast<webrtc::AudioTrackJni*> (nativeAudioTrack);
  this_object->OnCacheDirectBufferAddress(env, byte_buffer);
}

void AudioTrackJni::OnCacheDirectBufferAddress(
    JNIEnv* env, jobject byte_buffer) {
  ALOGD("OnCacheDirectBufferAddress");
  DCHECK(thread_checker_.CalledOnValidThread());
  direct_buffer_address_ =
      env->GetDirectBufferAddress(byte_buffer);
  jlong capacity = env->GetDirectBufferCapacity(byte_buffer);
  ALOGD("direct buffer capacity: %lld", capacity);
  direct_buffer_capacity_in_bytes_ = static_cast<int> (capacity);
  frames_per_buffer_ = direct_buffer_capacity_in_bytes_ / kBytesPerFrame;
  ALOGD("frames_per_buffer: %d", frames_per_buffer_);
}

void JNICALL AudioTrackJni::GetPlayoutData(
  JNIEnv* env, jobject obj, jint length, jlong nativeAudioTrack) {
  webrtc::AudioTrackJni* this_object =
      reinterpret_cast<webrtc::AudioTrackJni*> (nativeAudioTrack);
  this_object->OnGetPlayoutData(length);
}

// This method is called on a high-priority thread from Java. The name of
// the thread is 'AudioRecordTrack'.
void AudioTrackJni::OnGetPlayoutData(int length) {
  DCHECK(thread_checker_java_.CalledOnValidThread());
  DCHECK_EQ(frames_per_buffer_, length / kBytesPerFrame);
  if (!audio_device_buffer_) {
    ALOGE("AttachAudioBuffer has not been called!");
    return;
  }
  // Pull decoded data (in 16-bit PCM format) from jitter buffer.
  int samples = audio_device_buffer_->RequestPlayoutData(frames_per_buffer_);
  if (samples <= 0) {
    ALOGE("AudioDeviceBuffer::RequestPlayoutData failed!");
    return;
  }
  DCHECK_EQ(samples, frames_per_buffer_);
  // Copy decoded data into common byte buffer to ensure that it can be
  // written to the Java based audio track.
  samples = audio_device_buffer_->GetPlayoutData(direct_buffer_address_);
  DCHECK_EQ(length, kBytesPerFrame * samples);
}

bool AudioTrackJni::HasDeviceObjects() {
  return (g_jvm && g_context && g_audio_track_class);
}

void AudioTrackJni::CreateJavaInstance() {
  ALOGD("CreateJavaInstance");
  AttachThreadScoped ats(g_jvm);
  JNIEnv* jni = ats.env();
  jmethodID constructorID = GetMethodID(
      jni, g_audio_track_class, "<init>", "(Landroid/content/Context;J)V");
  j_audio_track_ = jni->NewObject(g_audio_track_class,
                                  constructorID,
                                  g_context,
                                  reinterpret_cast<intptr_t>(this));
  CHECK_EXCEPTION(jni) << "Error during NewObject";
  CHECK(j_audio_track_);
  j_audio_track_ = jni->NewGlobalRef(j_audio_track_);
  CHECK_EXCEPTION(jni) << "Error during NewGlobalRef";
  CHECK(j_audio_track_);
}

}  // namespace webrtc
