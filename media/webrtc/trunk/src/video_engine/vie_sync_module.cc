/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/vie_sync_module.h"

#include "modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "modules/video_coding/main/interface/video_coding.h"
#include "system_wrappers/interface/critical_section_wrapper.h"
#include "system_wrappers/interface/trace.h"
#include "video_engine/stream_synchronization.h"
#include "voice_engine/include/voe_video_sync.h"

namespace webrtc {

enum { kSyncInterval = 1000};

ViESyncModule::ViESyncModule(const int32_t channel_id, VideoCodingModule* vcm)
    : data_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      channel_id_(channel_id),
      vcm_(vcm),
      video_rtcp_module_(NULL),
      voe_channel_id_(-1),
      voe_sync_interface_(NULL),
      last_sync_time_(TickTime::Now()),
      sync_() {
}

ViESyncModule::~ViESyncModule() {
}

int ViESyncModule::ConfigureSync(int voe_channel_id,
                                 VoEVideoSync* voe_sync_interface,
                                 RtpRtcp* video_rtcp_module) {
  CriticalSectionScoped cs(data_cs_.get());
  voe_channel_id_ = voe_channel_id;
  voe_sync_interface_ = voe_sync_interface;
  video_rtcp_module_ = video_rtcp_module;
  sync_.reset(new StreamSynchronization(voe_channel_id, channel_id_));

  if (!voe_sync_interface) {
    voe_channel_id_ = -1;
    if (voe_channel_id >= 0) {
      // Trying to set a voice channel but no interface exist.
      return -1;
    }
    return 0;
  }
  return 0;
}

int ViESyncModule::VoiceChannel() {
  return voe_channel_id_;
}

WebRtc_Word32 ViESyncModule::TimeUntilNextProcess() {
  return static_cast<WebRtc_Word32>(kSyncInterval -
                         (TickTime::Now() - last_sync_time_).Milliseconds());
}

WebRtc_Word32 ViESyncModule::Process() {
  CriticalSectionScoped cs(data_cs_.get());
  last_sync_time_ = TickTime::Now();

  int total_video_delay_target_ms = vcm_->Delay();
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, channel_id_,
               "Video delay (JB + decoder) is %d ms",
               total_video_delay_target_ms);

  if (voe_channel_id_ == -1) {
    return 0;
  }
  assert(video_rtcp_module_ && voe_sync_interface_);
  assert(sync_.get());

  int current_audio_delay_ms = 0;
  if (voe_sync_interface_->GetDelayEstimate(voe_channel_id_,
                                            current_audio_delay_ms) != 0) {
    // Could not get VoE delay value, probably not a valid channel Id.
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo, channel_id_,
                 "%s: VE_GetDelayEstimate error for voice_channel %d",
                 __FUNCTION__, total_video_delay_target_ms, voe_channel_id_);
    return 0;
  }

  // VoiceEngine report delay estimates even when not started, ignore if the
  // reported value is lower than 40 ms.
  if (current_audio_delay_ms < 40) {
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, channel_id_,
                 "A/V Sync: Audio delay < 40, skipping.");
    return 0;
  }

  RtpRtcp* voice_rtcp_module = NULL;
  if (0 != voe_sync_interface_->GetRtpRtcp(voe_channel_id_,
                                           voice_rtcp_module)) {
    return 0;
  }
  assert(voice_rtcp_module);

  StreamSynchronization::Measurements video;
  if (0 != video_rtcp_module_->RemoteNTP(&video.received_ntp_secs,
                                         &video.received_ntp_frac,
                                         &video.rtcp_arrivaltime_secs,
                                         &video.rtcp_arrivaltime_frac)) {
    // Failed to get video NTP.
    return 0;
  }

  StreamSynchronization::Measurements audio;
  if (0 != voice_rtcp_module->RemoteNTP(&audio.received_ntp_secs,
                                        &audio.received_ntp_frac,
                                        &audio.rtcp_arrivaltime_secs,
                                        &audio.rtcp_arrivaltime_frac)) {
    // Failed to get audio NTP.
    return 0;
  }
  int extra_audio_delay_ms = 0;
  if (sync_->ComputeDelays(audio, current_audio_delay_ms, &extra_audio_delay_ms,
                          video, &total_video_delay_target_ms) != 0) {
    return 0;
  }
  // Set the extra audio delay.synchronization
  if (voe_sync_interface_->SetMinimumPlayoutDelay(
      voe_channel_id_, extra_audio_delay_ms) == -1) {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideo, channel_id_,
                 "Error setting voice delay");
  }
  vcm_->SetMinimumPlayoutDelay(total_video_delay_target_ms);
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, channel_id_,
               "New Video delay target is: %d", total_video_delay_target_ms);
  return 0;
}

}  // namespace webrtc
