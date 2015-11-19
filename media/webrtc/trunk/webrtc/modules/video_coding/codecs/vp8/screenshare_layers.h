/* Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
*
*  Use of this source code is governed by a BSD-style license
*  that can be found in the LICENSE file in the root of the source
*  tree. An additional intellectual property rights grant can be found
*  in the file PATENTS.  All contributing project authors may
*  be found in the AUTHORS file in the root of the source tree.
*/
#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_SCREENSHARE_LAYERS_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_SCREENSHARE_LAYERS_H_

#include <list>

#include "vpx/vpx_encoder.h"

#include "webrtc/modules/video_coding/codecs/vp8/temporal_layers.h"
#include "webrtc/modules/video_coding/utility/include/frame_dropper.h"
#include "webrtc/typedefs.h"

namespace webrtc {

struct CodecSpecificInfoVP8;

class ScreenshareLayers : public TemporalLayers {
 public:
  static const double kMaxTL0FpsReduction;
  static const double kAcceptableTargetOvershoot;

  ScreenshareLayers(int num_temporal_layers,
                    uint8_t initial_tl0_pic_idx,
                    FrameDropper* tl0_frame_dropper,
                    FrameDropper* tl1_frame_dropper);
  virtual ~ScreenshareLayers() {}

  // Returns the recommended VP8 encode flags needed. May refresh the decoder
  // and/or update the reference buffers.
  virtual int EncodeFlags(uint32_t timestamp);

  virtual bool ConfigureBitrates(int bitrate_kbit,
                                 int max_bitrate_kbit,
                                 int framerate,
                                 vpx_codec_enc_cfg_t* cfg);

  virtual void PopulateCodecSpecific(bool base_layer_sync,
                                     CodecSpecificInfoVP8 *vp8_info,
                                     uint32_t timestamp);

  virtual void FrameEncoded(unsigned int size, uint32_t timestamp);

  virtual int CurrentLayerId() const;

 private:
  void CalculateFramerate(uint32_t timestamp);
  bool TimeToSync(uint32_t timestamp) const;

  FrameDropper* tl0_frame_dropper_;
  FrameDropper* tl1_frame_dropper_;
  int number_of_temporal_layers_;
  bool last_base_layer_sync_;
  uint8_t tl0_pic_idx_;
  int active_layer_;
  std::list<uint32_t> timestamp_list_;
  int framerate_;
  int64_t last_sync_timestamp_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_SCREENSHARE_LAYERS_H_
