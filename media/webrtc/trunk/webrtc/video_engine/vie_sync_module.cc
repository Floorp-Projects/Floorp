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
#include "video_engine/vie_channel.h"
#include "voice_engine/include/voe_video_sync.h"

namespace webrtc {

enum { kSyncInterval = 1000};

int UpdateMeasurements(StreamSynchronization::Measurements* stream,
                       const RtpRtcp* rtp_rtcp) {
  stream->latest_timestamp = rtp_rtcp->RemoteTimestamp();
  stream->latest_receive_time_ms = rtp_rtcp->LocalTimeOfRemoteTimeStamp();
  synchronization::RtcpMeasurement measurement;
  if (0 != rtp_rtcp->RemoteNTP(&measurement.ntp_secs,
                               &measurement.ntp_frac,
                               NULL,
                               NULL,
                               &measurement.rtp_timestamp)) {
    return -1;
  }
  if (measurement.ntp_secs == 0 && measurement.ntp_frac == 0) {
    return -1;
  }
  for (synchronization::RtcpList::iterator it = stream->rtcp.begin();
       it != stream->rtcp.end(); ++it) {
    if (measurement.ntp_secs == (*it).ntp_secs &&
        measurement.ntp_frac == (*it).ntp_frac) {
      // This RTCP has already been added to the list.
      return 0;
    }
  }
  // We need two RTCP SR reports to map between RTP and NTP. More than two will
  // not improve the mapping.
  if (stream->rtcp.size() == 2) {
    stream->rtcp.pop_back();
  }
  stream->rtcp.push_front(measurement);
  return 0;
}

ViESyncModule::ViESyncModule(VideoCodingModule* vcm,
                             ViEChannel* vie_channel)
    : data_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      vcm_(vcm),
      vie_channel_(vie_channel),
      video_rtp_rtcp_(NULL),
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
  video_rtp_rtcp_ = video_rtcp_module;
  sync_.reset(new StreamSynchronization(voe_channel_id, vie_channel_->Id()));

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
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, vie_channel_->Id(),
               "Video delay (JB + decoder) is %d ms",
               total_video_delay_target_ms);

  if (voe_channel_id_ == -1) {
    return 0;
  }
  assert(video_rtp_rtcp_ && voe_sync_interface_);
  assert(sync_.get());

  int current_audio_delay_ms = 0;
  if (voe_sync_interface_->GetDelayEstimate(voe_channel_id_,
                                            current_audio_delay_ms) != 0) {
    // Could not get VoE delay value, probably not a valid channel Id.
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo, vie_channel_->Id(),
                 "%s: VE_GetDelayEstimate error for voice_channel %d",
                 __FUNCTION__, voe_channel_id_);
    return 0;
  }

  // VoiceEngine report delay estimates even when not started, ignore if the
  // reported value is lower than 40 ms.
  if (current_audio_delay_ms < 40) {
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, vie_channel_->Id(),
                 "A/V Sync: Audio delay < 40, skipping.");
    return 0;
  }

  RtpRtcp* voice_rtp_rtcp = NULL;
  if (0 != voe_sync_interface_->GetRtpRtcp(voe_channel_id_, voice_rtp_rtcp)) {
    return 0;
  }
  assert(voice_rtp_rtcp);

  if (UpdateMeasurements(&video_measurement_, video_rtp_rtcp_) != 0) {
    return 0;
  }

  if (UpdateMeasurements(&audio_measurement_, voice_rtp_rtcp) != 0) {
    return 0;
  }

  int relative_delay_ms;
  // Calculate how much later or earlier the audio stream is compared to video.
  if (!sync_->ComputeRelativeDelay(audio_measurement_, video_measurement_,
                                   &relative_delay_ms)) {
    return 0;
  }

  int extra_audio_delay_ms = 0;
  // Calculate the necessary extra audio delay and desired total video
  // delay to get the streams in sync.
  if (!sync_->ComputeDelays(relative_delay_ms,
                            current_audio_delay_ms,
                            &extra_audio_delay_ms,
                            &total_video_delay_target_ms)) {
    return 0;
  }
  if (voe_sync_interface_->SetMinimumPlayoutDelay(
      voe_channel_id_, extra_audio_delay_ms) == -1) {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideo, vie_channel_->Id(),
                 "Error setting voice delay");
  }
  vcm_->SetMinimumPlayoutDelay(total_video_delay_target_ms);
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, vie_channel_->Id(),
               "New Video delay target is: %d", total_video_delay_target_ms);
  return 0;
}

}  // namespace webrtc
