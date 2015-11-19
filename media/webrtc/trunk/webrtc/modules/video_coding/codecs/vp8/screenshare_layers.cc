/* Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#include "webrtc/modules/video_coding/codecs/vp8/screenshare_layers.h"

#include <stdlib.h>

#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"
#include "webrtc/modules/video_coding/codecs/interface/video_codec_interface.h"

namespace webrtc {

enum { kOneSecond90Khz = 90000 };

const double ScreenshareLayers::kMaxTL0FpsReduction = 2.5;
const double ScreenshareLayers::kAcceptableTargetOvershoot = 2.0;

ScreenshareLayers::ScreenshareLayers(int num_temporal_layers,
                                     uint8_t initial_tl0_pic_idx,
                                     FrameDropper* tl0_frame_dropper,
                                     FrameDropper* tl1_frame_dropper)
    : tl0_frame_dropper_(tl0_frame_dropper),
      tl1_frame_dropper_(tl1_frame_dropper),
      number_of_temporal_layers_(num_temporal_layers),
      last_base_layer_sync_(false),
      tl0_pic_idx_(initial_tl0_pic_idx),
      active_layer_(0),
      framerate_(5),
      last_sync_timestamp_(-1) {
  assert(num_temporal_layers > 0);
  assert(num_temporal_layers <= 2);
  assert(tl0_frame_dropper && tl1_frame_dropper);
}

int ScreenshareLayers::CurrentLayerId() const {
  // Codec does not use temporal layers for screenshare.
  return 0;
}

int ScreenshareLayers::EncodeFlags(uint32_t timestamp) {
  if (number_of_temporal_layers_ <= 1) {
    // No flags needed for 1 layer screenshare.
    return 0;
  }
  CalculateFramerate(timestamp);
  int flags = 0;
  // Note that ARF on purpose isn't used in this scheme since it is allocated
  // for the last key frame to make key frame caching possible.
  if (tl0_frame_dropper_->DropFrame()) {
    // Must drop TL0, encode TL1 instead.
    if (tl1_frame_dropper_->DropFrame()) {
      // Must drop both TL0 and TL1.
      flags = -1;
    } else {
      active_layer_ = 1;
      if (TimeToSync(timestamp)) {
        last_sync_timestamp_ = timestamp;
        // Allow predicting from only TL0 to allow participants to switch to the
        // high bitrate stream. This means predicting only from the LAST
        // reference frame, but only updating GF to not corrupt TL0.
        flags = VP8_EFLAG_NO_REF_ARF;
        flags |= VP8_EFLAG_NO_REF_GF;
        flags |= VP8_EFLAG_NO_UPD_ARF;
        flags |= VP8_EFLAG_NO_UPD_LAST;
      } else {
        // Allow predicting from both TL0 and TL1.
        flags = VP8_EFLAG_NO_REF_ARF;
        flags |= VP8_EFLAG_NO_UPD_ARF;
        flags |= VP8_EFLAG_NO_UPD_LAST;
      }
    }
  } else {
    active_layer_ = 0;
    // Since this is TL0 we only allow updating and predicting from the LAST
    // reference frame.
    flags = VP8_EFLAG_NO_UPD_GF;
    flags |= VP8_EFLAG_NO_UPD_ARF;
    flags |= VP8_EFLAG_NO_REF_GF;
    flags |= VP8_EFLAG_NO_REF_ARF;
  }
  // Make sure both frame droppers leak out bits.
  tl0_frame_dropper_->Leak(framerate_);
  tl1_frame_dropper_->Leak(framerate_);
  return flags;
}

bool ScreenshareLayers::ConfigureBitrates(int bitrate_kbit,
                                          int max_bitrate_kbit,
                                          int framerate,
                                          vpx_codec_enc_cfg_t* cfg) {
  if (framerate > 0)
    framerate_ = framerate;

  tl0_frame_dropper_->SetRates(bitrate_kbit, framerate_);
  tl1_frame_dropper_->SetRates(max_bitrate_kbit, framerate_);

  if (cfg != nullptr) {
    // Calculate a codec target bitrate. This may be higher than TL0, gaining
    // quality at the expense of frame rate at TL0. Constraints:
    // - TL0 frame rate should not be less than framerate / kMaxTL0FpsReduction.
    // - Target rate * kAcceptableTargetOvershoot should not exceed TL1 rate.
    double target_bitrate =
        std::min(bitrate_kbit * kMaxTL0FpsReduction,
                 max_bitrate_kbit / kAcceptableTargetOvershoot);
    cfg->rc_target_bitrate =
        std::max(static_cast<unsigned int>(bitrate_kbit),
                 static_cast<unsigned int>(target_bitrate + 0.5));
  }

  return true;
}

void ScreenshareLayers::FrameEncoded(unsigned int size, uint32_t timestamp) {
  if (active_layer_ == 0) {
    tl0_frame_dropper_->Fill(size, true);
  }
  tl1_frame_dropper_->Fill(size, true);
}

void ScreenshareLayers::PopulateCodecSpecific(bool base_layer_sync,
                                              CodecSpecificInfoVP8 *vp8_info,
                                              uint32_t timestamp) {
  if (number_of_temporal_layers_ == 1) {
    vp8_info->temporalIdx = kNoTemporalIdx;
    vp8_info->layerSync = false;
    vp8_info->tl0PicIdx = kNoTl0PicIdx;
  } else {
    vp8_info->temporalIdx = active_layer_;
    if (base_layer_sync) {
      vp8_info->temporalIdx = 0;
      last_sync_timestamp_ = timestamp;
    } else if (last_base_layer_sync_ && vp8_info->temporalIdx != 0) {
      // Regardless of pattern the frame after a base layer sync will always
      // be a layer sync.
      last_sync_timestamp_ = timestamp;
    }
    vp8_info->layerSync = (last_sync_timestamp_ == timestamp);
    if (vp8_info->temporalIdx == 0) {
      tl0_pic_idx_++;
    }
    last_base_layer_sync_ = base_layer_sync;
    vp8_info->tl0PicIdx = tl0_pic_idx_;
  }
}

bool ScreenshareLayers::TimeToSync(uint32_t timestamp) const {
  const uint32_t timestamp_diff = timestamp - last_sync_timestamp_;
  return last_sync_timestamp_ < 0 || timestamp_diff > kOneSecond90Khz;
}

void ScreenshareLayers::CalculateFramerate(uint32_t timestamp) {
  timestamp_list_.push_front(timestamp);
  // Remove timestamps older than 1 second from the list.
  uint32_t timestamp_diff = timestamp - timestamp_list_.back();
  while (timestamp_diff > kOneSecond90Khz) {
    timestamp_list_.pop_back();
    timestamp_diff = timestamp - timestamp_list_.back();
  }
  // If we have encoded frames within the last second, that number of frames
  // is a reasonable first estimate of the framerate.
  framerate_ = timestamp_list_.size();
  if (timestamp_diff > 0) {
    // Estimate the framerate by dividing the number of timestamp diffs with
    // the sum of the timestamp diffs (with rounding).
    framerate_ = (kOneSecond90Khz * (timestamp_list_.size() - 1) +
        timestamp_diff / 2) / timestamp_diff;
  }
}

}  // namespace webrtc
