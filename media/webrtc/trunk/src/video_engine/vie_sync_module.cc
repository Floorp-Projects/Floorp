/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vie_sync_module.h"

#include "critical_section_wrapper.h"
#include "rtp_rtcp.h"
#include "trace.h"
#include "video_coding.h"
#include "voe_video_sync.h"

namespace webrtc {

enum { kSyncInterval = 1000};
enum { kMaxVideoDiffMs = 80 };
enum { kMaxAudioDiffMs = 80 };
enum { kMaxDelay = 1500 };

ViESyncModule::ViESyncModule(int id, VideoCodingModule& vcm,
                             RtpRtcp& rtcp_module)
    : data_cs_(CriticalSectionWrapper::CreateCriticalSection()),
      id_(id),
      vcm_(vcm),
      rtcp_module_(rtcp_module),
      voe_channel_id_(-1),
      voe_sync_interface_(NULL),
      last_sync_time_(TickTime::Now()) {
}

ViESyncModule::~ViESyncModule() {
}

int ViESyncModule::SetVoiceChannel(int voe_channel_id,
                                   VoEVideoSync* voe_sync_interface) {
  CriticalSectionScoped cs(data_cs_.get());
  voe_channel_id_ = voe_channel_id;
  voe_sync_interface_ = voe_sync_interface;
  rtcp_module_.DeRegisterSyncModule();

  if (!voe_sync_interface) {
    voe_channel_id_ = -1;
    if (voe_channel_id >= 0) {
      // Trying to set a voice channel but no interface exist.
      return -1;
    }
    return 0;
  }
  RtpRtcp* voe_rtp_rtcp = NULL;
  voe_sync_interface->GetRtpRtcp(voe_channel_id_, voe_rtp_rtcp);
  return rtcp_module_.RegisterSyncModule(voe_rtp_rtcp);
}

int ViESyncModule::VoiceChannel() {
  return voe_channel_id_;
}

void ViESyncModule::SetNetworkDelay(int network_delay) {
  channel_delay_.network_delay = network_delay;
}

WebRtc_Word32 ViESyncModule::Version(char* version,
                                     WebRtc_UWord32& remaining_buffer_in_bytes,
                                     WebRtc_UWord32& position) const {
  if (version == NULL) {
    WEBRTC_TRACE(webrtc::kTraceWarning, webrtc::kTraceVideo, -1,
                 "Invalid in argument to ViESyncModule Version()");
    return -1;
  }
  char our_version[] = "ViESyncModule 1.1.0";
  WebRtc_UWord32 our_length = (WebRtc_UWord32) strlen(our_version);
  if (remaining_buffer_in_bytes < our_length + 1) {
    return -1;
  }
  memcpy(version, our_version, our_length);
  version[our_length] = '\0';
  remaining_buffer_in_bytes -= (our_length + 1);
  position += (our_length + 1);
  return 0;
}

WebRtc_Word32 ViESyncModule::ChangeUniqueId(const WebRtc_Word32 id) {
  id_ = id;
  return 0;
}

WebRtc_Word32 ViESyncModule::TimeUntilNextProcess() {
  return (WebRtc_Word32)(kSyncInterval -
                         (TickTime::Now() - last_sync_time_).Milliseconds());
}

WebRtc_Word32 ViESyncModule::Process() {
  CriticalSectionScoped cs(data_cs_.get());
  last_sync_time_ = TickTime::Now();

  int total_video_delay_target_ms = vcm_.Delay();
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, id_,
               "Video delay (JB + decoder) is %d ms",
               total_video_delay_target_ms);

  if (voe_channel_id_ == -1) {
    return 0;
  }

  int current_audio_delay_ms = 0;
  if (voe_sync_interface_->GetDelayEstimate(voe_channel_id_,
                                            current_audio_delay_ms) != 0) {
    // Could not get VoE delay value, probably not a valid channel Id.
    WEBRTC_TRACE(webrtc::kTraceStream, webrtc::kTraceVideo, id_,
                 "%s: VE_GetDelayEstimate error for voice_channel %d",
                 __FUNCTION__, total_video_delay_target_ms, voe_channel_id_);
    return 0;
  }

  int current_diff_ms = 0;
  // Total video delay.
  int video_delay_ms = 0;
  // VoiceEngine report delay estimates even when not started, ignore if the
  // reported value is lower than 40 ms.
  if (current_audio_delay_ms < 40) {
    WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, id_,
                 "A/V Sync: Audio delay < 40, skipping.");
    return 0;
  }

  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, id_,
               "Audio delay is: %d for voice channel: %d",
               current_audio_delay_ms, voe_channel_id_);
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, id_,
               "Network delay diff is: %d for voice channel: %d",
               channel_delay_.network_delay, voe_channel_id_);
  // Calculate the difference between the lowest possible video delay and
  // the current audio delay.
  current_diff_ms = total_video_delay_target_ms - current_audio_delay_ms +
      channel_delay_.network_delay;
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, id_,
               "Current diff is: %d for audio channel: %d",
               current_diff_ms, voe_channel_id_);

  if (current_diff_ms > 0) {
    // The minimum video delay is longer than the current audio delay.
    // We need to decrease extra video delay, if we have added extra delay
    // earlier, or add extra audio delay.
    if (channel_delay_.extra_video_delay_ms > 0) {
      // We have extra delay added to ViE. Reduce this delay before adding
      // extra delay to VoE.

      // This is the desired delay, we can't reduce more than this.
      video_delay_ms = total_video_delay_target_ms;

      // Check that we don't reduce the delay more than what is allowed.
      if (video_delay_ms <
          channel_delay_.last_video_delay_ms - kMaxVideoDiffMs) {
        video_delay_ms =
            channel_delay_.last_video_delay_ms - kMaxVideoDiffMs;
        channel_delay_.extra_video_delay_ms =
            video_delay_ms - total_video_delay_target_ms;
      } else {
        channel_delay_.extra_video_delay_ms = 0;
      }
      channel_delay_.last_video_delay_ms = video_delay_ms;
      channel_delay_.last_sync_delay = -1;
      channel_delay_.extra_audio_delay_ms = 0;
    } else {  // channel_delay_.extra_video_delay_ms > 0
      // We have no extra video delay to remove, increase the audio delay.
      if (channel_delay_.last_sync_delay >= 0) {
        // We have increased the audio delay earlier, increase it even more.
        int audio_diff_ms = current_diff_ms / 2;
        if (audio_diff_ms > kMaxAudioDiffMs) {
          // We only allow a maximum change of KMaxAudioDiffMS for audio
          // due to NetEQ maximum changes.
          audio_diff_ms = kMaxAudioDiffMs;
        }
        // Increase the audio delay
        channel_delay_.extra_audio_delay_ms += audio_diff_ms;

        // Don't set a too high delay.
        if (channel_delay_.extra_audio_delay_ms > kMaxDelay) {
          channel_delay_.extra_audio_delay_ms = kMaxDelay;
        }

        // Don't add any extra video delay.
        video_delay_ms = total_video_delay_target_ms;
        channel_delay_.extra_video_delay_ms = 0;
        channel_delay_.last_video_delay_ms = video_delay_ms;
        channel_delay_.last_sync_delay = 1;
      } else {  // channel_delay_.last_sync_delay >= 0
        // First time after a delay change, don't add any extra delay.
        // This is to not toggle back and forth too much.
        channel_delay_.extra_audio_delay_ms = 0;
        // Set minimum video delay
        video_delay_ms = total_video_delay_target_ms;
        channel_delay_.extra_video_delay_ms = 0;
        channel_delay_.last_video_delay_ms = video_delay_ms;
        channel_delay_.last_sync_delay = 0;
      }
    }
  } else {  // if (current_diffMS > 0)
    // The minimum video delay is lower than the current audio delay.
    // We need to decrease possible extra audio delay, or
    // add extra video delay.

    if (channel_delay_.extra_audio_delay_ms > 0) {
      // We have extra delay in VoiceEngine
      // Start with decreasing the voice delay
      int audio_diff_ms = current_diff_ms / 2;
      if (audio_diff_ms < -1 * kMaxAudioDiffMs) {
        // Don't change the delay too much at once.
        audio_diff_ms = -1 * kMaxAudioDiffMs;
      }
      // Add the negative difference.
      channel_delay_.extra_audio_delay_ms += audio_diff_ms;

      if (channel_delay_.extra_audio_delay_ms < 0) {
        // Negative values not allowed.
        channel_delay_.extra_audio_delay_ms = 0;
        channel_delay_.last_sync_delay = 0;
      } else {
        // There is more audio delay to use for the next round.
        channel_delay_.last_sync_delay = 1;
      }

      // Keep the video delay at the minimum values.
      video_delay_ms = total_video_delay_target_ms;
      channel_delay_.extra_video_delay_ms = 0;
      channel_delay_.last_video_delay_ms = video_delay_ms;
    } else {  // channel_delay_.extra_audio_delay_ms > 0
      // We have no extra delay in VoiceEngine, increase the video delay.
      channel_delay_.extra_audio_delay_ms = 0;

      // Make the difference positive.
      int video_diff_ms = -1 * current_diff_ms;

      // This is the desired delay.
      video_delay_ms = total_video_delay_target_ms + video_diff_ms;
      if (video_delay_ms > channel_delay_.last_video_delay_ms) {
        if (video_delay_ms >
            channel_delay_.last_video_delay_ms + kMaxVideoDiffMs) {
          // Don't increase the delay too much at once
          video_delay_ms =
              channel_delay_.last_video_delay_ms + kMaxVideoDiffMs;
        }
        // Verify we don't go above the maximum allowed delay
        if (video_delay_ms > kMaxDelay) {
          video_delay_ms = kMaxDelay;
        }
      } else {
        if (video_delay_ms <
            channel_delay_.last_video_delay_ms - kMaxVideoDiffMs) {
          // Don't decrease the delay too much at once
          video_delay_ms =
              channel_delay_.last_video_delay_ms - kMaxVideoDiffMs;
        }
        // Verify we don't go below the minimum delay
        if (video_delay_ms < total_video_delay_target_ms) {
          video_delay_ms = total_video_delay_target_ms;
        }
      }
      // Store the values
      channel_delay_.extra_video_delay_ms =
          video_delay_ms - total_video_delay_target_ms;
      channel_delay_.last_video_delay_ms = video_delay_ms;
      channel_delay_.last_sync_delay = -1;
    }
  }

  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, id_,
      "Sync video delay %d ms for video channel and audio delay %d for audio "
      "channel %d",
      video_delay_ms, channel_delay_.extra_audio_delay_ms, voe_channel_id_);

  // Set the extra audio delay.synchronization
  if (voe_sync_interface_->SetMinimumPlayoutDelay(
      voe_channel_id_, channel_delay_.extra_audio_delay_ms) == -1) {
    WEBRTC_TRACE(webrtc::kTraceDebug, webrtc::kTraceVideo, id_,
                 "Error setting voice delay");
  }

  if (video_delay_ms < 0) {
    video_delay_ms = 0;
  }
  total_video_delay_target_ms =
      (total_video_delay_target_ms  >  video_delay_ms) ?
      total_video_delay_target_ms : video_delay_ms;
  vcm_.SetMinimumPlayoutDelay(total_video_delay_target_ms);
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, id_,
               "New Video delay target is: %d", total_video_delay_target_ms);
  return 0;
}

}  // namespace webrtc
