/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/vie_sync_module.h"

#include "webrtc/modules/rtp_rtcp/interface/rtp_receiver.h"
#include "webrtc/modules/rtp_rtcp/interface/rtp_rtcp.h"
#include "webrtc/modules/video_coding/main/interface/video_coding.h"
#include "webrtc/system_wrappers/interface/critical_section_wrapper.h"
#include "webrtc/system_wrappers/interface/logging.h"
#include "webrtc/system_wrappers/interface/trace_event.h"
#include "webrtc/video_engine/stream_synchronization.h"
#include "webrtc/video_engine/vie_channel.h"
#include "webrtc/voice_engine/include/voe_video_sync.h"

namespace webrtc {

enum { kSyncInterval = 1000};

int UpdateMeasurements(StreamSynchronization::Measurements* stream,
                       const RtpRtcp& rtp_rtcp, const RtpReceiver& receiver) {
  if (!receiver.Timestamp(&stream->latest_timestamp))
    return -1;
  if (!receiver.LastReceivedTimeMs(&stream->latest_receive_time_ms))
    return -1;

  uint32_t ntp_secs = 0;
  uint32_t ntp_frac = 0;
  uint32_t rtp_timestamp = 0;
  if (0 != rtp_rtcp.RemoteNTP(&ntp_secs,
                              &ntp_frac,
                              NULL,
                              NULL,
                              &rtp_timestamp)) {
    return -1;
  }

  bool new_rtcp_sr = false;
  if (!UpdateRtcpList(
      ntp_secs, ntp_frac, rtp_timestamp, &stream->rtcp, &new_rtcp_sr)) {
    return -1;
  }

  return 0;
}

ViESyncModule::ViESyncModule(VideoCodingModule* vcm,
                             ViEChannel* vie_channel)
    : data_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      vcm_(vcm),
      vie_channel_(vie_channel),
      video_receiver_(NULL),
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
                                 RtpRtcp* video_rtcp_module,
                                 RtpReceiver* video_receiver) {
  CriticalSectionScoped cs(data_cs_.get());
  voe_channel_id_ = voe_channel_id;
  voe_sync_interface_ = voe_sync_interface;
  video_receiver_ = video_receiver;
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

int32_t ViESyncModule::TimeUntilNextProcess() {
  return static_cast<int32_t>(kSyncInterval -
      (TickTime::Now() - last_sync_time_).Milliseconds());
}

int32_t ViESyncModule::Process() {
  CriticalSectionScoped cs(data_cs_.get());
  last_sync_time_ = TickTime::Now();

  const int current_video_delay_ms = vcm_->Delay();

  if (voe_channel_id_ == -1) {
    return 0;
  }
  assert(video_rtp_rtcp_ && voe_sync_interface_);
  assert(sync_.get());

  int audio_jitter_buffer_delay_ms = 0;
  int playout_buffer_delay_ms = 0;
  int avsync_offset_ms = 0;
  if (voe_sync_interface_->GetDelayEstimate(voe_channel_id_,
                                            &audio_jitter_buffer_delay_ms,
                                            &playout_buffer_delay_ms,
                                            &avsync_offset_ms) != 0) {
    return 0;
  }
  const int current_audio_delay_ms = audio_jitter_buffer_delay_ms +
      playout_buffer_delay_ms;

  RtpRtcp* voice_rtp_rtcp = NULL;
  RtpReceiver* voice_receiver = NULL;
  if (0 != voe_sync_interface_->GetRtpRtcp(voe_channel_id_, &voice_rtp_rtcp,
                                           &voice_receiver)) {
    return 0;
  }
  assert(voice_rtp_rtcp);
  assert(voice_receiver);

  if (UpdateMeasurements(&video_measurement_, *video_rtp_rtcp_,
                         *video_receiver_) != 0) {
    return 0;
  }

  if (UpdateMeasurements(&audio_measurement_, *voice_rtp_rtcp,
                         *voice_receiver) != 0) {
    return 0;
  }

  int relative_delay_ms;
  int result;
  // Calculate how much later or earlier the audio stream is compared to video.

  result = sync_->ComputeRelativeDelay(audio_measurement_, video_measurement_,
                                       &relative_delay_ms);
  if (!result) {
    return 0;
  }
  voe_sync_interface_->SetCurrentSyncOffset(voe_channel_id_, relative_delay_ms);

  TRACE_COUNTER1("webrtc", "SyncCurrentVideoDelay", current_video_delay_ms);
  TRACE_COUNTER1("webrtc", "SyncCurrentAudioDelay", current_audio_delay_ms);
  TRACE_COUNTER1("webrtc", "SyncRelativeDelay", relative_delay_ms);
  int target_audio_delay_ms = 0;
  int target_video_delay_ms = current_video_delay_ms;
  // Calculate the necessary extra audio delay and desired total video
  // delay to get the streams in sync.
  if (!sync_->ComputeDelays(relative_delay_ms,
                            current_audio_delay_ms,
                            &target_audio_delay_ms,
                            &target_video_delay_ms)) {
    return 0;
  }

  if (voe_sync_interface_->SetMinimumPlayoutDelay(
      voe_channel_id_, target_audio_delay_ms) == -1) {
    LOG(LS_ERROR) << "Error setting voice delay.";
  }
  vcm_->SetMinimumPlayoutDelay(target_video_delay_ms);
  return 0;
}

int ViESyncModule::SetTargetBufferingDelay(int target_delay_ms) {
  CriticalSectionScoped cs(data_cs_.get());
  if (!voe_sync_interface_) {
    LOG(LS_ERROR) << "voe_sync_interface_ NULL, can't set playout delay.";
    return -1;
  }
  sync_->SetTargetBufferingDelay(target_delay_ms);
  // Setting initial playout delay to voice engine (video engine is updated via
  // the VCM interface).
  voe_sync_interface_->SetInitialPlayoutDelay(voe_channel_id_,
                                              target_delay_ms);
  return 0;
}

}  // namespace webrtc
