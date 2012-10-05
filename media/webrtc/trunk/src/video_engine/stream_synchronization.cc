/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "video_engine/stream_synchronization.h"
#include "system_wrappers/interface/trace.h"

namespace webrtc {

enum { kMaxVideoDiffMs = 80 };
enum { kMaxAudioDiffMs = 80 };
enum { kMaxDelay = 1500 };

const float FracMS = 4.294967296E6f;

struct ViESyncDelay {
  ViESyncDelay() {
    extra_video_delay_ms = 0;
    last_video_delay_ms = 0;
    extra_audio_delay_ms = 0;
    last_sync_delay = 0;
    network_delay = 120;
  }

  int extra_video_delay_ms;
  int last_video_delay_ms;
  int extra_audio_delay_ms;
  int last_sync_delay;
  int network_delay;
};

StreamSynchronization::StreamSynchronization(int audio_channel_id,
                                             int video_channel_id)
    : channel_delay_(new ViESyncDelay),
      audio_channel_id_(audio_channel_id),
      video_channel_id_(video_channel_id) {}

StreamSynchronization::~StreamSynchronization() {
  delete channel_delay_;
}

int StreamSynchronization::ComputeDelays(const Measurements& audio,
                                         int current_audio_delay_ms,
                                         int* extra_audio_delay_ms,
                                         const Measurements& video,
                                         int* total_video_delay_target_ms) {
  // ReceivedNTPxxx is NTP at sender side when sent.
  // RTCPArrivalTimexxx is NTP at receiver side when received.
  // can't use ConvertNTPTimeToMS since calculation can be
  //  negative
  int NTPdiff = (audio.received_ntp_secs - video.received_ntp_secs)
                * 1000;  // ms
  float ntp_diff_frac = audio.received_ntp_frac / FracMS -
        video.received_ntp_frac / FracMS;
  if (ntp_diff_frac > 0.0f)
    NTPdiff += static_cast<int>(ntp_diff_frac + 0.5f);
  else
    NTPdiff += static_cast<int>(ntp_diff_frac - 0.5f);

  int RTCPdiff = (audio.rtcp_arrivaltime_secs - video.rtcp_arrivaltime_secs)
                 * 1000;  // ms
  float rtcp_diff_frac = audio.rtcp_arrivaltime_frac / FracMS -
        video.rtcp_arrivaltime_frac / FracMS;
  if (rtcp_diff_frac > 0.0f)
    RTCPdiff += static_cast<int>(rtcp_diff_frac + 0.5f);
  else
    RTCPdiff += static_cast<int>(rtcp_diff_frac - 0.5f);

  int diff = NTPdiff - RTCPdiff;
  // if diff is + video is behind
  if (diff < -1000 || diff > 1000) {
    // unresonable ignore value.
    return -1;
  }
  channel_delay_->network_delay = diff;

  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, video_channel_id_,
               "Audio delay is: %d for voice channel: %d",
               current_audio_delay_ms, audio_channel_id_);
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, video_channel_id_,
               "Network delay diff is: %d for voice channel: %d",
               channel_delay_->network_delay, audio_channel_id_);
  // Calculate the difference between the lowest possible video delay and
  // the current audio delay.
  int current_diff_ms = *total_video_delay_target_ms - current_audio_delay_ms +
      channel_delay_->network_delay;
  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, video_channel_id_,
               "Current diff is: %d for audio channel: %d",
               current_diff_ms, audio_channel_id_);

  int video_delay_ms = 0;
  if (current_diff_ms > 0) {
    // The minimum video delay is longer than the current audio delay.
    // We need to decrease extra video delay, if we have added extra delay
    // earlier, or add extra audio delay.
    if (channel_delay_->extra_video_delay_ms > 0) {
      // We have extra delay added to ViE. Reduce this delay before adding
      // extra delay to VoE.

      // This is the desired delay, we can't reduce more than this.
      video_delay_ms = *total_video_delay_target_ms;

      // Check that we don't reduce the delay more than what is allowed.
      if (video_delay_ms <
          channel_delay_->last_video_delay_ms - kMaxVideoDiffMs) {
        video_delay_ms =
            channel_delay_->last_video_delay_ms - kMaxVideoDiffMs;
        channel_delay_->extra_video_delay_ms =
            video_delay_ms - *total_video_delay_target_ms;
      } else {
        channel_delay_->extra_video_delay_ms = 0;
      }
      channel_delay_->last_video_delay_ms = video_delay_ms;
      channel_delay_->last_sync_delay = -1;
      channel_delay_->extra_audio_delay_ms = 0;
    } else {  // channel_delay_->extra_video_delay_ms > 0
      // We have no extra video delay to remove, increase the audio delay.
      if (channel_delay_->last_sync_delay >= 0) {
        // We have increased the audio delay earlier, increase it even more.
        int audio_diff_ms = current_diff_ms / 2;
        if (audio_diff_ms > kMaxAudioDiffMs) {
          // We only allow a maximum change of KMaxAudioDiffMS for audio
          // due to NetEQ maximum changes.
          audio_diff_ms = kMaxAudioDiffMs;
        }
        // Increase the audio delay
        channel_delay_->extra_audio_delay_ms += audio_diff_ms;

        // Don't set a too high delay.
        if (channel_delay_->extra_audio_delay_ms > kMaxDelay) {
          channel_delay_->extra_audio_delay_ms = kMaxDelay;
        }

        // Don't add any extra video delay.
        video_delay_ms = *total_video_delay_target_ms;
        channel_delay_->extra_video_delay_ms = 0;
        channel_delay_->last_video_delay_ms = video_delay_ms;
        channel_delay_->last_sync_delay = 1;
      } else {  // channel_delay_->last_sync_delay >= 0
        // First time after a delay change, don't add any extra delay.
        // This is to not toggle back and forth too much.
        channel_delay_->extra_audio_delay_ms = 0;
        // Set minimum video delay
        video_delay_ms = *total_video_delay_target_ms;
        channel_delay_->extra_video_delay_ms = 0;
        channel_delay_->last_video_delay_ms = video_delay_ms;
        channel_delay_->last_sync_delay = 0;
      }
    }
  } else {  // if (current_diffMS > 0)
    // The minimum video delay is lower than the current audio delay.
    // We need to decrease possible extra audio delay, or
    // add extra video delay.

    if (channel_delay_->extra_audio_delay_ms > 0) {
      // We have extra delay in VoiceEngine
      // Start with decreasing the voice delay
      int audio_diff_ms = current_diff_ms / 2;
      if (audio_diff_ms < -1 * kMaxAudioDiffMs) {
        // Don't change the delay too much at once.
        audio_diff_ms = -1 * kMaxAudioDiffMs;
      }
      // Add the negative difference.
      channel_delay_->extra_audio_delay_ms += audio_diff_ms;

      if (channel_delay_->extra_audio_delay_ms < 0) {
        // Negative values not allowed.
        channel_delay_->extra_audio_delay_ms = 0;
        channel_delay_->last_sync_delay = 0;
      } else {
        // There is more audio delay to use for the next round.
        channel_delay_->last_sync_delay = 1;
      }

      // Keep the video delay at the minimum values.
      video_delay_ms = *total_video_delay_target_ms;
      channel_delay_->extra_video_delay_ms = 0;
      channel_delay_->last_video_delay_ms = video_delay_ms;
    } else {  // channel_delay_->extra_audio_delay_ms > 0
      // We have no extra delay in VoiceEngine, increase the video delay.
      channel_delay_->extra_audio_delay_ms = 0;

      // Make the difference positive.
      int video_diff_ms = -1 * current_diff_ms;

      // This is the desired delay.
      video_delay_ms = *total_video_delay_target_ms + video_diff_ms;
      if (video_delay_ms > channel_delay_->last_video_delay_ms) {
        if (video_delay_ms >
            channel_delay_->last_video_delay_ms + kMaxVideoDiffMs) {
          // Don't increase the delay too much at once
          video_delay_ms =
              channel_delay_->last_video_delay_ms + kMaxVideoDiffMs;
        }
        // Verify we don't go above the maximum allowed delay
        if (video_delay_ms > kMaxDelay) {
          video_delay_ms = kMaxDelay;
        }
      } else {
        if (video_delay_ms <
            channel_delay_->last_video_delay_ms - kMaxVideoDiffMs) {
          // Don't decrease the delay too much at once
          video_delay_ms =
              channel_delay_->last_video_delay_ms - kMaxVideoDiffMs;
        }
        // Verify we don't go below the minimum delay
        if (video_delay_ms < *total_video_delay_target_ms) {
          video_delay_ms = *total_video_delay_target_ms;
        }
      }
      // Store the values
      channel_delay_->extra_video_delay_ms =
          video_delay_ms - *total_video_delay_target_ms;
      channel_delay_->last_video_delay_ms = video_delay_ms;
      channel_delay_->last_sync_delay = -1;
    }
  }

  WEBRTC_TRACE(webrtc::kTraceInfo, webrtc::kTraceVideo, video_channel_id_,
      "Sync video delay %d ms for video channel and audio delay %d for audio "
      "channel %d",
      video_delay_ms, channel_delay_->extra_audio_delay_ms, audio_channel_id_);

  *extra_audio_delay_ms = channel_delay_->extra_audio_delay_ms;

  if (video_delay_ms < 0) {
    video_delay_ms = 0;
  }
  *total_video_delay_target_ms =
      (*total_video_delay_target_ms  >  video_delay_ms) ?
      *total_video_delay_target_ms : video_delay_ms;
  return 0;
}
}  // namespace webrtc
