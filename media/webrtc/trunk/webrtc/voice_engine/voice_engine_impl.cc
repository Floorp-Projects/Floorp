/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if defined(WEBRTC_ANDROID)
#include "webrtc/modules/audio_device/android/audio_device_template.h"
#include "webrtc/modules/audio_device/android/audio_record_jni.h"
#include "webrtc/modules/audio_device/android/audio_track_jni.h"
#include "webrtc/modules/audio_device/android/opensles_input.h"
#include "webrtc/modules/audio_device/android/opensles_output.h"
#endif

#include "webrtc/modules/audio_coding/main/interface/audio_coding_module.h"
#include "webrtc/system_wrappers/interface/trace.h"
#include "webrtc/voice_engine/voice_engine_impl.h"

namespace webrtc
{

// Counter to be ensure that we can add a correct ID in all static trace
// methods. It is not the nicest solution, especially not since we already
// have a counter in VoEBaseImpl. In other words, there is room for
// improvement here.
static int32_t gVoiceEngineInstanceCounter = 0;

VoiceEngine* GetVoiceEngine(const Config* config, bool owns_config)
{
#if (defined _WIN32)
  HMODULE hmod = LoadLibrary(TEXT("VoiceEngineTestingDynamic.dll"));

  if (hmod) {
    typedef VoiceEngine* (*PfnGetVoiceEngine)(void);
    PfnGetVoiceEngine pfn = (PfnGetVoiceEngine)GetProcAddress(
        hmod,"GetVoiceEngine");
    if (pfn) {
      VoiceEngine* self = pfn();
      if (owns_config) {
        delete config;
      }
      return (self);
    }
  }
#endif

    VoiceEngineImpl* self = new VoiceEngineImpl(config, owns_config);
    if (self != NULL)
    {
        self->AddRef();  // First reference.  Released in VoiceEngine::Delete.
        gVoiceEngineInstanceCounter++;
    }
    return self;
}

int VoiceEngineImpl::AddRef() {
  return ++_ref_count;
}

// This implements the Release() method for all the inherited interfaces.
int VoiceEngineImpl::Release() {
  int new_ref = --_ref_count;
  assert(new_ref >= 0);
  if (new_ref == 0) {
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, -1,
                 "VoiceEngineImpl self deleting (voiceEngine=0x%p)",
                 this);

    delete this;
  }

  return new_ref;
}

VoiceEngine* VoiceEngine::Create() {
  Config* config = new Config();
  config->Set<AudioCodingModuleFactory>(new AudioCodingModuleFactory());

  return GetVoiceEngine(config, true);
}

VoiceEngine* VoiceEngine::Create(const Config& config) {
  return GetVoiceEngine(&config, false);
}

int VoiceEngine::SetTraceFilter(unsigned int filter)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice,
                 VoEId(gVoiceEngineInstanceCounter, -1),
                 "SetTraceFilter(filter=0x%x)", filter);

    // Remember old filter
    uint32_t oldFilter = Trace::level_filter();
    Trace::set_level_filter(filter);

    // If previous log was ignored, log again after changing filter
    if (kTraceNone == oldFilter)
    {
        WEBRTC_TRACE(kTraceApiCall, kTraceVoice, -1,
                     "SetTraceFilter(filter=0x%x)", filter);
    }

    return 0;
}

int VoiceEngine::SetTraceFile(const char* fileNameUTF8,
                              bool addFileCounter)
{
    int ret = Trace::SetTraceFile(fileNameUTF8, addFileCounter);
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice,
                 VoEId(gVoiceEngineInstanceCounter, -1),
                 "SetTraceFile(fileNameUTF8=%s, addFileCounter=%d)",
                 fileNameUTF8, addFileCounter);
    return (ret);
}

int VoiceEngine::SetTraceCallback(TraceCallback* callback)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice,
                 VoEId(gVoiceEngineInstanceCounter, -1),
                 "SetTraceCallback(callback=0x%x)", callback);
    return (Trace::SetTraceCallback(callback));
}

bool VoiceEngine::Delete(VoiceEngine*& voiceEngine)
{
    if (voiceEngine == NULL)
        return false;

    VoiceEngineImpl* s = static_cast<VoiceEngineImpl*>(voiceEngine);
    // Release the reference that was added in GetVoiceEngine.
    int ref = s->Release();
    voiceEngine = NULL;

    if (ref != 0) {
        WEBRTC_TRACE(kTraceWarning, kTraceVoice, -1,
            "VoiceEngine::Delete did not release the very last reference.  "
            "%d references remain.", ref);
    }

    return true;
}

int VoiceEngine::SetAndroidObjects(void* javaVM, void* env, void* context)
{
#ifdef WEBRTC_ANDROID
#ifdef WEBRTC_ANDROID_OPENSLES
  typedef AudioDeviceTemplate<OpenSlesInput, OpenSlesOutput>
      AudioDeviceInstance;
#else
  typedef AudioDeviceTemplate<AudioRecordJni, AudioTrackJni>
      AudioDeviceInstance;
#endif
  if (javaVM && env && context) {
    AudioDeviceInstance::SetAndroidAudioDeviceObjects(javaVM, env, context);
  } else {
    AudioDeviceInstance::ClearAndroidAudioDeviceObjects();
  }
  return 0;
#else
  return -1;
#endif
}

}  // namespace webrtc
