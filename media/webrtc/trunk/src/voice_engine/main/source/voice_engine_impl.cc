/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
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
    VoiceEngine* ve = reinterpret_cast<VoiceEngine*> (self);
    if (ve != NULL)
    {
        gVoiceEngineInstanceCounter++;
    }
    return ve;
}
} // extern "C"

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

bool VoiceEngine::Delete(VoiceEngine*& voiceEngine, bool ignoreRefCounters)
{
    if (voiceEngine == NULL)
    {
        return false;
    }

    VoiceEngineImpl* s = reinterpret_cast<VoiceEngineImpl*> (voiceEngine);
    VoEBaseImpl* base = s;

    WEBRTC_TRACE(kTraceApiCall, kTraceVoice, -1,
                 "VoiceEngine::Delete(voiceEngine=0x%p, ignoreRefCounters=%d)",
                 voiceEngine, ignoreRefCounters);

    if (!ignoreRefCounters)
    {
        if (base->GetCount() != 0)
        {
            WEBRTC_TRACE(kTraceCritical, kTraceVoice, -1,
                         "VoEBase reference counter is %d => memory will not "
                             "be released properly!", base->GetCount());
            return false;
        }
#ifdef WEBRTC_VOICE_ENGINE_CODEC_API
        VoECodecImpl* codec = s;
        if (codec->GetCount() != 0)
        {
            WEBRTC_TRACE(kTraceCritical, kTraceVoice, -1,
                         "VoECodec reference counter is %d => memory will not "
                             "be released properly!", codec->GetCount());
            return false;
        }
#endif

#ifdef WEBRTC_VOICE_ENGINE_DTMF_API
        VoEDtmfImpl* dtmf = s;
        if (dtmf->GetCount() != 0)
        {
            WEBRTC_TRACE(kTraceCritical, kTraceVoice, -1,
                         "VoEDtmf reference counter is %d =>"
                             "memory will not be released properly!",
                         dtmf->GetCount());
            return false;
        }
#endif

#ifdef WEBRTC_VOICE_ENGINE_ENCRYPTION_API
        VoEEncryptionImpl* encrypt = s;
        if (encrypt->GetCount() != 0)
        {
            WEBRTC_TRACE(kTraceCritical, kTraceVoice, -1,
                         "VoEEncryption reference counter is %d => "
                             "memory will not be released properly!",
                         encrypt->GetCount());
            return false;
        }
#endif

#ifdef WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API
        VoEExternalMediaImpl* extmedia = s;
        if (extmedia->GetCount() != 0)
        {
            WEBRTC_TRACE(kTraceCritical, kTraceVoice, -1,
                         "VoEExternalMedia reference counter is %d => "
                             "memory will not be released properly!",
                         extmedia->GetCount());
            return false;
        }
#endif

#ifdef WEBRTC_VOICE_ENGINE_CALL_REPORT_API
        VoECallReportImpl* report = s;
        if (report->GetCount() != 0)
        {
            WEBRTC_TRACE(kTraceCritical, kTraceVoice, -1,
                         "VoECallReport reference counter is %d => memory "
                             "will not be released properly!",
                         report->GetCount());
            return false;
        }
#endif

#ifdef WEBRTC_VOICE_ENGINE_FILE_API
        VoEFileImpl* file = s;
        if (file->GetCount() != 0)
        {
            WEBRTC_TRACE(
                         kTraceCritical,
                         kTraceVoice,
                         -1,
                         "VoEFile reference counter is %d => memory will not "
                         "be released properly!",
                         file->GetCount());
            return false;
        }
#endif

#ifdef WEBRTC_VOICE_ENGINE_HARDWARE_API
        VoEHardwareImpl* hware = s;
        if (hware->GetCount() != 0)
        {
            WEBRTC_TRACE(kTraceCritical, kTraceVoice, -1,
                         "VoEHardware reference counter is %d => memory will "
                         "not be released properly!", hware->GetCount());
            return false;
        }
#endif

#ifdef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API
        VoENetEqStatsImpl* neteqst = s;
        if (neteqst->GetCount() != 0)
        {
            WEBRTC_TRACE(kTraceCritical, kTraceVoice, -1,
                         "VoENetEqStats reference counter is %d => "
                             "memory will not be released properly!",
                         neteqst->GetCount());
            return false;
        }
#endif

#ifdef WEBRTC_VOICE_ENGINE_NETWORK_API
        VoENetworkImpl* netw = s;
        if (netw->GetCount() != 0)
        {
            WEBRTC_TRACE(kTraceCritical, kTraceVoice, -1,
                         "VoENetworkImpl reference counter is %d => memory "
                         "will not be released properly!", netw->GetCount());
            return false;
        }
#endif

#ifdef WEBRTC_VOICE_ENGINE_RTP_RTCP_API
        VoERTP_RTCPImpl* rtcp = s;
        if (rtcp->GetCount() != 0)
        {
            WEBRTC_TRACE(kTraceCritical, kTraceVoice, -1,
                         "VoERTP_RTCP reference counter is %d =>"
                             "memory will not be released properly!",
                         rtcp->GetCount());
            return false;
        }
#endif

#ifdef WEBRTC_VOICE_ENGINE_VIDEO_SYNC_API
        VoEVideoSyncImpl* vsync = s;
        if (vsync->GetCount() != 0)
        {
            WEBRTC_TRACE(kTraceCritical, kTraceVoice, -1,
                         "VoEVideoSync reference counter is %d => "
                             "memory will not be released properly!",
                         vsync->GetCount());
            return false;
        }
#endif

#ifdef WEBRTC_VOICE_ENGINE_VOLUME_CONTROL_API
        VoEVolumeControlImpl* volume = s;
        if (volume->GetCount() != 0)
        {
            WEBRTC_TRACE(kTraceCritical, kTraceVoice, -1,
                         "VoEVolumeControl reference counter is %d =>"
                             "memory will not be released properly!",
                         volume->GetCount());
            return false;
        }
#endif

#ifdef WEBRTC_VOICE_ENGINE_AUDIO_PROCESSING_API
        VoEAudioProcessingImpl* apm = s;
        if (apm->GetCount() != 0)
        {
            WEBRTC_TRACE(kTraceCritical, kTraceVoice, -1,
                         "VoEAudioProcessing reference counter is %d => "
                             "memory will not be released properly!",
                         apm->GetCount());
            return false;
        }
#endif
        WEBRTC_TRACE(kTraceInfo, kTraceVoice, -1,
                     "all reference counters are zero => deleting the "
                     "VoiceEngine instance...");

    } // if (!ignoreRefCounters)
    else
    {
        WEBRTC_TRACE(kTraceInfo, kTraceVoice, -1,
                     "reference counters are ignored => deleting the "
                     "VoiceEngine instance...");
    }

    delete s;
    voiceEngine = NULL;

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
