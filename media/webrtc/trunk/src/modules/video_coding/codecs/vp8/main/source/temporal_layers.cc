/* Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/

#include "temporal_layers.h"

#include <stdlib.h>
#include <string.h>
#include <cassert>

#include "modules/interface/module_common_types.h"
#include "modules/video_coding/codecs/interface/video_codec_interface.h"
#include "modules/video_coding/codecs/vp8/main/interface/vp8_common_types.h"

#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"

namespace webrtc {

TemporalLayers::TemporalLayers(int numberOfTemporalLayers)
    : number_of_temporal_layers_(numberOfTemporalLayers),
      temporal_ids_length_(0),
      temporal_pattern_length_(0),
      tl0_pic_idx_(rand()),
      pattern_idx_(255) {
  assert(kMaxTemporalStreams >= numberOfTemporalLayers);
  memset(temporal_ids_, 0, sizeof(temporal_ids_));
  memset(temporal_pattern_, 0, sizeof(temporal_pattern_));
}

bool TemporalLayers::ConfigureBitrates(int bitrateKbit,
                                       vpx_codec_enc_cfg_t* cfg) {
  switch (number_of_temporal_layers_) {
    case 0:
    case 1:
      // Do nothing.
      break;
    case 2:
      temporal_ids_length_ = 2;
      temporal_ids_[0] = 0;
      temporal_ids_[1] = 1;
      cfg->ts_number_layers = number_of_temporal_layers_;
      cfg->ts_periodicity = temporal_ids_length_;
      // Split stream 60% 40%.
      // Bitrate API for VP8 is the agregated bitrate for all lower layers.
      cfg->ts_target_bitrate[0] = bitrateKbit * kVp8LayerRateAlloction[1][0];
      cfg->ts_target_bitrate[1] = bitrateKbit;
      cfg->ts_rate_decimator[0] = 2;
      cfg->ts_rate_decimator[1] = 1;
      memcpy(cfg->ts_layer_id,
             temporal_ids_,
             sizeof(unsigned int) * temporal_ids_length_);
      temporal_pattern_length_ = 8;
      temporal_pattern_[0] = kTemporalUpdateLast;
      temporal_pattern_[1] = kTemporalUpdateGoldenWithoutDependency;
      temporal_pattern_[2] = kTemporalUpdateLast;
      temporal_pattern_[3] = kTemporalUpdateGolden;
      temporal_pattern_[4] = kTemporalUpdateLast;
      temporal_pattern_[5] = kTemporalUpdateGolden;
      temporal_pattern_[6] = kTemporalUpdateLast;
      temporal_pattern_[7] = kTemporalUpdateNoneNoRefAltref;
      break;
    case 3:
      temporal_ids_length_ = 4;
      temporal_ids_[0] = 0;
      temporal_ids_[1] = 2;
      temporal_ids_[2] = 1;
      temporal_ids_[3] = 2;
      cfg->ts_number_layers = number_of_temporal_layers_;
      cfg->ts_periodicity = temporal_ids_length_;
      // Split stream 40% 20% 40%.
      // Bitrate API for VP8 is the agregated bitrate for all lower layers.
      cfg->ts_target_bitrate[0] = bitrateKbit * kVp8LayerRateAlloction[2][0];
      cfg->ts_target_bitrate[1] = bitrateKbit * kVp8LayerRateAlloction[2][1];
      cfg->ts_target_bitrate[2] = bitrateKbit;
      cfg->ts_rate_decimator[0] = 4;
      cfg->ts_rate_decimator[1] = 2;
      cfg->ts_rate_decimator[2] = 1;
      memcpy(cfg->ts_layer_id,
             temporal_ids_,
             sizeof(unsigned int) * temporal_ids_length_);
      temporal_pattern_length_ = 8;
      temporal_pattern_[0] = kTemporalUpdateLast;
      temporal_pattern_[1] = kTemporalUpdateAltrefWithoutDependency;
      temporal_pattern_[2] = kTemporalUpdateGoldenWithoutDependency;
      temporal_pattern_[3] = kTemporalUpdateAltref;
      temporal_pattern_[4] = kTemporalUpdateLast;
      temporal_pattern_[5] = kTemporalUpdateAltref;
      temporal_pattern_[6] = kTemporalUpdateGolden;
      temporal_pattern_[7] = kTemporalUpdateNone;
      break;
    case 4:
      temporal_ids_length_ = 8;
      temporal_ids_[0] = 0;
      temporal_ids_[1] = 3;
      temporal_ids_[2] = 2;
      temporal_ids_[3] = 3;
      temporal_ids_[4] = 1;
      temporal_ids_[5] = 3;
      temporal_ids_[6] = 2;
      temporal_ids_[7] = 3;
      // Split stream 25% 15% 20% 40%.
      // Bitrate API for VP8 is the agregated bitrate for all lower layers.
      cfg->ts_number_layers = 4;
      cfg->ts_periodicity = temporal_ids_length_;
      cfg->ts_target_bitrate[0] = bitrateKbit * kVp8LayerRateAlloction[3][0];
      cfg->ts_target_bitrate[1] = bitrateKbit * kVp8LayerRateAlloction[3][1];
      cfg->ts_target_bitrate[2] = bitrateKbit * kVp8LayerRateAlloction[3][2];
      cfg->ts_target_bitrate[3] = bitrateKbit;
      cfg->ts_rate_decimator[0] = 8;
      cfg->ts_rate_decimator[1] = 4;
      cfg->ts_rate_decimator[2] = 2;
      cfg->ts_rate_decimator[3] = 1;
      memcpy(cfg->ts_layer_id,
             temporal_ids_,
             sizeof(unsigned int) * temporal_ids_length_);
      temporal_pattern_length_ = 16;
      temporal_pattern_[0] = kTemporalUpdateLast;
      temporal_pattern_[1] = kTemporalUpdateNone;
      temporal_pattern_[2] = kTemporalUpdateAltrefWithoutDependency;
      temporal_pattern_[3] = kTemporalUpdateNone;
      temporal_pattern_[4] = kTemporalUpdateGoldenWithoutDependency;
      temporal_pattern_[5] = kTemporalUpdateNone;
      temporal_pattern_[6] = kTemporalUpdateAltref;
      temporal_pattern_[7] = kTemporalUpdateNone;
      temporal_pattern_[8] = kTemporalUpdateLast;
      temporal_pattern_[9] = kTemporalUpdateNone;
      temporal_pattern_[10] = kTemporalUpdateAltref;
      temporal_pattern_[11] = kTemporalUpdateNone;
      temporal_pattern_[12] = kTemporalUpdateGolden;
      temporal_pattern_[13] = kTemporalUpdateNone;
      temporal_pattern_[14] = kTemporalUpdateAltref;
      temporal_pattern_[15] = kTemporalUpdateNone;
      break;
    default:
      assert(false);
      return false;
  }
  return true;
}

int TemporalLayers::EncodeFlags() {
  assert(number_of_temporal_layers_ > 1);
  assert(kMaxTemporalPattern >= temporal_pattern_length_);
  assert(0 < temporal_pattern_length_);

  int flags = 0;
  int patternIdx = ++pattern_idx_ % temporal_pattern_length_;
  assert(kMaxTemporalPattern >= patternIdx);
  switch (temporal_pattern_[patternIdx]) {
    case kTemporalUpdateLast:
      flags |= VP8_EFLAG_NO_UPD_GF;
      flags |= VP8_EFLAG_NO_UPD_ARF;
      flags |= VP8_EFLAG_NO_REF_GF;
      flags |= VP8_EFLAG_NO_REF_ARF;
      break;
    case kTemporalUpdateGoldenWithoutDependency:
      flags |= VP8_EFLAG_NO_REF_GF;
      // Deliberately no break here.
    case kTemporalUpdateGolden:
      flags |= VP8_EFLAG_NO_REF_ARF;
      flags |= VP8_EFLAG_NO_UPD_ARF;
      flags |= VP8_EFLAG_NO_UPD_LAST;
      break;
    case kTemporalUpdateAltrefWithoutDependency:
      flags |= VP8_EFLAG_NO_REF_ARF;
      flags |= VP8_EFLAG_NO_REF_GF;
      // Deliberately no break here.
    case kTemporalUpdateAltref:
      flags |= VP8_EFLAG_NO_UPD_GF;
      flags |= VP8_EFLAG_NO_UPD_LAST;
      break;
    case kTemporalUpdateNoneNoRefAltref:
      flags |= VP8_EFLAG_NO_REF_ARF;
      // Deliberately no break here.
    case kTemporalUpdateNone:
      flags |= VP8_EFLAG_NO_UPD_GF;
      flags |= VP8_EFLAG_NO_UPD_ARF;
      flags |= VP8_EFLAG_NO_UPD_LAST;
      flags |= VP8_EFLAG_NO_UPD_ENTROPY;
      break;
  }
  return flags;
}

void TemporalLayers::PopulateCodecSpecific(bool key_frame,
                                           CodecSpecificInfoVP8 *vp8_info) {
  assert(number_of_temporal_layers_ > 1);
  assert(0 < temporal_ids_length_);

  if (key_frame) {
    // Keyframe is always temporal layer 0
    vp8_info->temporalIdx = 0;
  } else {
    vp8_info->temporalIdx = temporal_ids_[pattern_idx_ % temporal_ids_length_];
  }
  TemporalReferences temporal_reference =
      temporal_pattern_[pattern_idx_ % temporal_pattern_length_];

  if (temporal_reference == kTemporalUpdateAltrefWithoutDependency ||
      temporal_reference == kTemporalUpdateGoldenWithoutDependency ||
      (temporal_reference == kTemporalUpdateNone &&
      number_of_temporal_layers_ == 4)) {
    vp8_info->layerSync = true;
  } else {
    vp8_info->layerSync = false;
  }

  if (vp8_info->temporalIdx == 0) {
    tl0_pic_idx_++;
  }
  vp8_info->tl0PicIdx = tl0_pic_idx_;
}
}  // namespace webrtc
