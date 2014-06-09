/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_processing/main/interface/video_processing.h"
#include "webrtc/modules/video_processing/main/source/video_decimator.h"
#include "webrtc/system_wrappers/interface/tick_util.h"

#define VD_MIN(a, b) ((a) < (b)) ? (a) : (b)

namespace webrtc {

VPMVideoDecimator::VPMVideoDecimator()
    : overshoot_modifier_(0),
      drop_count_(0),
      keep_count_(0),
      target_frame_rate_(30),
      incoming_frame_rate_(0.0f),
      max_frame_rate_(30),
      incoming_frame_times_(),
      enable_temporal_decimation_(true) {
  Reset();
}

VPMVideoDecimator::~VPMVideoDecimator() {}

void VPMVideoDecimator::Reset()  {
  overshoot_modifier_ = 0;
  drop_count_ = 0;
  keep_count_ = 0;
  target_frame_rate_ = 30;
  incoming_frame_rate_ = 0.0f;
  max_frame_rate_ = 30;
  memset(incoming_frame_times_, 0, sizeof(incoming_frame_times_));
  enable_temporal_decimation_ = true;
}

void VPMVideoDecimator::EnableTemporalDecimation(bool enable) {
  enable_temporal_decimation_ = enable;
}

int32_t VPMVideoDecimator::SetMaxFramerate(uint32_t max_frame_rate) {
  if (max_frame_rate == 0) return VPM_PARAMETER_ERROR;

  max_frame_rate_ = max_frame_rate;

  if (target_frame_rate_ > max_frame_rate_)
    target_frame_rate_ = max_frame_rate_;

  return VPM_OK;
}

int32_t VPMVideoDecimator::SetTargetframe_rate(uint32_t frame_rate) {
  if (frame_rate == 0) return VPM_PARAMETER_ERROR;

  if (frame_rate > max_frame_rate_) {
    // Override.
    target_frame_rate_ = max_frame_rate_;
  } else {
    target_frame_rate_ = frame_rate;
  }
  return VPM_OK;
}

bool VPMVideoDecimator::DropFrame() {
  if (!enable_temporal_decimation_) return false;

  if (incoming_frame_rate_ <= 0) return false;

  const uint32_t incomingframe_rate =
      static_cast<uint32_t>(incoming_frame_rate_ + 0.5f);

  if (target_frame_rate_ == 0) return true;

  bool drop = false;
  if (incomingframe_rate > target_frame_rate_) {
    int32_t overshoot =
        overshoot_modifier_ + (incomingframe_rate - target_frame_rate_);
    if (overshoot < 0) {
      overshoot = 0;
      overshoot_modifier_ = 0;
    }

    if (overshoot && 2 * overshoot < (int32_t) incomingframe_rate) {
      if (drop_count_) {  // Just got here so drop to be sure.
          drop_count_ = 0;
          return true;
      }
      const uint32_t dropVar = incomingframe_rate / overshoot;

      if (keep_count_ >= dropVar) {
          drop = true;
          overshoot_modifier_ = -((int32_t) incomingframe_rate % overshoot) / 3;
          keep_count_ = 1;
      } else {
          keep_count_++;
      }
    } else {
      keep_count_ = 0;
      const uint32_t dropVar = overshoot / target_frame_rate_;
      if (drop_count_ < dropVar) {
          drop = true;
          drop_count_++;
      } else {
          overshoot_modifier_ = overshoot % target_frame_rate_;
          drop = false;
          drop_count_ = 0;
      }
    }
  }
  return drop;
}


uint32_t VPMVideoDecimator::Decimatedframe_rate() {
ProcessIncomingframe_rate(TickTime::MillisecondTimestamp());
  if (!enable_temporal_decimation_) {
    return static_cast<uint32_t>(incoming_frame_rate_ + 0.5f);
  }
  return VD_MIN(target_frame_rate_,
      static_cast<uint32_t>(incoming_frame_rate_ + 0.5f));
}

uint32_t VPMVideoDecimator::Inputframe_rate() {
  ProcessIncomingframe_rate(TickTime::MillisecondTimestamp());
  return static_cast<uint32_t>(incoming_frame_rate_ + 0.5f);
}

void VPMVideoDecimator::UpdateIncomingframe_rate() {
  int64_t now = TickTime::MillisecondTimestamp();
  if (incoming_frame_times_[0] == 0) {
    // First no shift.
  } else {
    // Shift.
    for (int i = kFrameCountHistory_size - 2; i >= 0; i--) {
        incoming_frame_times_[i+1] = incoming_frame_times_[i];
    }
  }
  incoming_frame_times_[0] = now;
  ProcessIncomingframe_rate(now);
}

void VPMVideoDecimator::ProcessIncomingframe_rate(int64_t now) {
  int32_t num = 0;
  int32_t nrOfFrames = 0;
  for (num = 1; num < (kFrameCountHistory_size - 1); num++) {
    // Don't use data older than 2sec.
    if (incoming_frame_times_[num] <= 0 ||
        now - incoming_frame_times_[num] > kFrameHistoryWindowMs) {
      break;
    } else {
      nrOfFrames++;
    }
  }
  if (num > 1) {
    int64_t diff = now - incoming_frame_times_[num-1];
    incoming_frame_rate_ = 1.0;
    if (diff > 0) {
      incoming_frame_rate_ = nrOfFrames * 1000.0f / static_cast<float>(diff);
    }
  } else {
    incoming_frame_rate_ = static_cast<float>(nrOfFrames);
  }
}

}  // namespace webrtc
