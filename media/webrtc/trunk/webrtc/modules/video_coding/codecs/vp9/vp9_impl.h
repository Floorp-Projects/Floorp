/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_VP9_IMPL_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_VP9_IMPL_H_

#include "webrtc/modules/video_coding/codecs/vp9/include/vp9.h"

// VPX forward declaration
typedef struct vpx_codec_ctx vpx_codec_ctx_t;
typedef struct vpx_codec_ctx vpx_dec_ctx_t;
typedef struct vpx_codec_enc_cfg vpx_codec_enc_cfg_t;
typedef struct vpx_image vpx_image_t;
typedef struct vpx_ref_frame vpx_ref_frame_t;
struct vpx_codec_cx_pkt;

namespace webrtc {

class VP9EncoderImpl : public VP9Encoder {
 public:
  VP9EncoderImpl();

  virtual ~VP9EncoderImpl();

  virtual int Release() OVERRIDE;

  virtual int InitEncode(const VideoCodec* codec_settings,
                         int number_of_cores,
                         uint32_t max_payload_size) OVERRIDE;

  virtual int Encode(const I420VideoFrame& input_image,
                     const CodecSpecificInfo* codec_specific_info,
                     const std::vector<VideoFrameType>* frame_types) OVERRIDE;

  virtual int RegisterEncodeCompleteCallback(EncodedImageCallback* callback)
  OVERRIDE;

  virtual int SetChannelParameters(uint32_t packet_loss, int rtt) OVERRIDE;

  virtual int SetRates(uint32_t new_bitrate_kbit, uint32_t frame_rate) OVERRIDE;

 private:
  // Call encoder initialize function and set control settings.
  int InitAndSetControlSettings(const VideoCodec* inst);

  void PopulateCodecSpecific(CodecSpecificInfo* codec_specific,
                             const vpx_codec_cx_pkt& pkt,
                             uint32_t timestamp);

  int GetEncodedPartitions(const I420VideoFrame& input_image);

  // Determine maximum target for Intra frames
  //
  // Input:
  //    - optimal_buffer_size : Optimal buffer size
  // Return Value             : Max target size for Intra frames represented as
  //                            percentage of the per frame bandwidth
  uint32_t MaxIntraTarget(uint32_t optimal_buffer_size);

  EncodedImage encoded_image_;
  EncodedImageCallback* encoded_complete_callback_;
  VideoCodec codec_;
  bool inited_;
  int64_t timestamp_;
  uint16_t picture_id_;
  int cpu_speed_;
  uint32_t rc_max_intra_target_;
  vpx_codec_ctx_t* encoder_;
  vpx_codec_enc_cfg_t* config_;
  vpx_image_t* raw_;
};


class VP9DecoderImpl : public VP9Decoder {
 public:
  VP9DecoderImpl();

  virtual ~VP9DecoderImpl();

  virtual int InitDecode(const VideoCodec* inst, int number_of_cores) OVERRIDE;

  virtual int Decode(const EncodedImage& input_image,
                     bool missing_frames,
                     const RTPFragmentationHeader* fragmentation,
                     const CodecSpecificInfo* codec_specific_info,
                     int64_t /*render_time_ms*/) OVERRIDE;

  virtual int RegisterDecodeCompleteCallback(DecodedImageCallback* callback)
  OVERRIDE;

  virtual int Release() OVERRIDE;

  virtual int Reset() OVERRIDE;

 private:
  int ReturnFrame(const vpx_image_t* img, uint32_t timeStamp);

  I420VideoFrame decoded_image_;
  DecodedImageCallback* decode_complete_callback_;
  bool inited_;
  vpx_dec_ctx_t* decoder_;
  VideoCodec codec_;
  bool key_frame_required_;
};
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_VP9_IMPL_H_
