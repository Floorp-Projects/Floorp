/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "voice_engine_impl.h"
#include "trace.h"

#ifdef WEBRTC_ANDROID
extern "C"
{
extern WebRtc_Word32 SetAndroidAudioDeviceObjects(
    void* javaVM, void* env, void* context);
} // extern "C"
#endif

namespace webrtc
{

// Counter to be ensure that we can add a correct ID in all static trace
// methods. It is not the nicest solution, especially not since we already
// have a counter in VoEBaseImpl. In other words, there is room for
// improvement here.
static WebRtc_Word32 gVoiceEngineInstanceCounter = 0;

extern "C"
{
WEBRTC_DLLEXPORT VoiceEngine* GetVoiceEngine();

VoiceEngine* GetVoiceEngine()
{
    VoiceEngineImpl* self = new VoiceEngineImpl();
    VoiceEngine* ve = reinterpret_cast<VoiceEngine*>(self);
    if (ve != NULL)
    {
        self->AddRef();  // First reference.  Released in VoiceEngine::Delete.
        gVoiceEngineInstanceCounter++;
    }
    return ve;
}
} // extern "C"

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

VoiceEngine* VoiceEngine::Create()
{
#if (defined _WIN32)
    HMODULE hmod_ = LoadLibrary(TEXT("VoiceEngineTestingDynamic.dll"));

    if (hmod_)
    {
        typedef VoiceEngine* (*PfnGetVoiceEngine)(void);
        PfnGetVoiceEngine pfn = (PfnGetVoiceEngine)GetProcAddress(
                hmod_,"GetVoiceEngine");
        if (pfn)
        {
            VoiceEngine* self = pfn();
            return (self);
        }
    }
#endif

    return GetVoiceEngine();
}

int VoiceEngine::SetTraceFilter(const unsigned int filter)
{
    WEBRTC_TRACE(kTraceApiCall, kTraceVoice,
                 VoEId(gVoiceEngineInstanceCounter, -1),
                 "SetTraceFilter(filter=0x%x)", filter);

    // Remember old filter
    WebRtc_UWord32 oldFilter = 0;
    Trace::LevelFilter(oldFilter);

    // Set new filter
    WebRtc_Word32 ret = Trace::SetLevelFilter(filter);

    // If previous log was ignored, log again after changing filter
    if (kTraceNone == oldFilter)
    {
        WEBRTC_TRACE(kTraceApiCall, kTraceVoice, -1,
                     "SetTraceFilter(filter=0x%x)", filter);
    }

    return (ret);
}

int VoiceEngine::SetTraceFile(const char* fileNameUTF8,
                              const bool addFileCounter)
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

    VoiceEngineImpl* s = reinterpret_cast<VoiceEngineImpl*>(voiceEngine);
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
#if defined(WEBRTC_ANDROID) && !defined(WEBRTC_ANDROID_OPENSLES)
    // modules/audio_device/main/source/android/audio_device_android_jni.cc
    // contains the actual implementation.
    return SetAndroidAudioDeviceObjects(javaVM, env, context);
#else
    return -1;
#endif
}

} //namespace webrtc
