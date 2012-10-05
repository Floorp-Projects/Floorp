/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_VOICE_ENGINE_IMPL_H
#define WEBRTC_VOICE_ENGINE_VOICE_ENGINE_IMPL_H

#include "atomic32.h"
#include "engine_configurations.h"
#include "voe_base_impl.h"

#ifdef WEBRTC_VOICE_ENGINE_AUDIO_PROCESSING_API
#include "voe_audio_processing_impl.h"
#endif
#ifdef WEBRTC_VOICE_ENGINE_CALL_REPORT_API
#include "voe_call_report_impl.h"
#endif
#ifdef WEBRTC_VOICE_ENGINE_CODEC_API
#include "voe_codec_impl.h"
#endif
#ifdef WEBRTC_VOICE_ENGINE_DTMF_API
#include "voe_dtmf_impl.h"
#endif
#ifdef WEBRTC_VOICE_ENGINE_ENCRYPTION_API
#include "voe_encryption_impl.h"
#endif
#ifdef WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API
#include "voe_external_media_impl.h"
#endif
#ifdef WEBRTC_VOICE_ENGINE_FILE_API
#include "voe_file_impl.h"
#endif
#ifdef WEBRTC_VOICE_ENGINE_HARDWARE_API
#include "voe_hardware_impl.h"
#endif
#ifdef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API
#include "voe_neteq_stats_impl.h"
#endif
#ifdef WEBRTC_VOICE_ENGINE_NETWORK_API
#include "voe_network_impl.h"
#endif
#ifdef WEBRTC_VOICE_ENGINE_RTP_RTCP_API
#include "voe_rtp_rtcp_impl.h"
#endif
#ifdef WEBRTC_VOICE_ENGINE_VIDEO_SYNC_API
#include "voe_video_sync_impl.h"
#endif
#ifdef WEBRTC_VOICE_ENGINE_VOLUME_CONTROL_API
#include "voe_volume_control_impl.h"
#endif

namespace webrtc
{

class VoiceEngineImpl : public voe::SharedData,  // Must be the first base class
#ifdef WEBRTC_VOICE_ENGINE_AUDIO_PROCESSING_API
                        public VoEAudioProcessingImpl,
#endif
#ifdef WEBRTC_VOICE_ENGINE_CALL_REPORT_API
                        public VoECallReportImpl,
#endif
#ifdef WEBRTC_VOICE_ENGINE_CODEC_API
                        public VoECodecImpl,
#endif
#ifdef WEBRTC_VOICE_ENGINE_DTMF_API
                        public VoEDtmfImpl,
#endif
#ifdef WEBRTC_VOICE_ENGINE_ENCRYPTION_API
                        public VoEEncryptionImpl,
#endif
#ifdef WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API
                        public VoEExternalMediaImpl,
#endif
#ifdef WEBRTC_VOICE_ENGINE_FILE_API
                        public VoEFileImpl,
#endif
#ifdef WEBRTC_VOICE_ENGINE_HARDWARE_API
                        public VoEHardwareImpl,
#endif
#ifdef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API
                        public VoENetEqStatsImpl,
#endif
#ifdef WEBRTC_VOICE_ENGINE_NETWORK_API
                        public VoENetworkImpl,
#endif
#ifdef WEBRTC_VOICE_ENGINE_RTP_RTCP_API
                        public VoERTP_RTCPImpl,
#endif
#ifdef WEBRTC_VOICE_ENGINE_VIDEO_SYNC_API
                        public VoEVideoSyncImpl,
#endif
#ifdef WEBRTC_VOICE_ENGINE_VOLUME_CONTROL_API
                        public VoEVolumeControlImpl,
#endif
                        public VoEBaseImpl
{
public:
    VoiceEngineImpl() : 
#ifdef WEBRTC_VOICE_ENGINE_AUDIO_PROCESSING_API
        VoEAudioProcessingImpl(this),
#endif
#ifdef WEBRTC_VOICE_ENGINE_CALL_REPORT_API
        VoECallReportImpl(this),
#endif
#ifdef WEBRTC_VOICE_ENGINE_CODEC_API
        VoECodecImpl(this),
#endif
#ifdef WEBRTC_VOICE_ENGINE_DTMF_API
        VoEDtmfImpl(this),
#endif
#ifdef WEBRTC_VOICE_ENGINE_ENCRYPTION_API
        VoEEncryptionImpl(this),
#endif
#ifdef WEBRTC_VOICE_ENGINE_EXTERNAL_MEDIA_API
        VoEExternalMediaImpl(this),
#endif
#ifdef WEBRTC_VOICE_ENGINE_FILE_API
        VoEFileImpl(this),
#endif
#ifdef WEBRTC_VOICE_ENGINE_HARDWARE_API
        VoEHardwareImpl(this),
#endif
#ifdef WEBRTC_VOICE_ENGINE_NETEQ_STATS_API
        VoENetEqStatsImpl(this),
#endif
#ifdef WEBRTC_VOICE_ENGINE_NETWORK_API
        VoENetworkImpl(this),
#endif
#ifdef WEBRTC_VOICE_ENGINE_RTP_RTCP_API
        VoERTP_RTCPImpl(this),
#endif
#ifdef WEBRTC_VOICE_ENGINE_VIDEO_SYNC_API
        VoEVideoSyncImpl(this),
#endif
#ifdef WEBRTC_VOICE_ENGINE_VOLUME_CONTROL_API
        VoEVolumeControlImpl(this),
#endif
        VoEBaseImpl(this),
        _ref_count(0)
    {
    }
    virtual ~VoiceEngineImpl()
    {
        assert(_ref_count.Value() == 0);
    }

    int AddRef();

    // This implements the Release() method for all the inherited interfaces.
    virtual int Release();

private:
    Atomic32 _ref_count;
};

} // namespace webrtc

#endif // WEBRTC_VOICE_ENGINE_VOICE_ENGINE_IMPL_H
