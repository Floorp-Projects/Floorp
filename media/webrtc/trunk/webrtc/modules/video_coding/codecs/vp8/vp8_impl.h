/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 *
 * WEBRTC VP8 wrapper interface
 */

#ifndef WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_IMPL_H_
#define WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_IMPL_H_

#include "webrtc/modules/video_coding/codecs/vp8/include/vp8.h"
#include "webrtc/modules/video_coding/utility/quality_scaler.h"

// VPX forward declaration
typedef struct vpx_codec_ctx vpx_codec_ctx_t;
typedef struct vpx_codec_ctx vpx_dec_ctx_t;
typedef struct vpx_codec_enc_cfg vpx_codec_enc_cfg_t;
typedef struct vpx_image vpx_image_t;
typedef struct vpx_ref_frame vpx_ref_frame_t;
struct vpx_codec_cx_pkt;

namespace webrtc {

class TemporalLayers;
class ReferencePictureSelection;

class VP8EncoderImpl : public VP8Encoder {
 public:
  VP8EncoderImpl();

  virtual ~VP8EncoderImpl();

  virtual int Release();

  virtual int InitEncode(const VideoCodec* codec_settings,
                         int number_of_cores,
                         uint32_t max_payload_size);

  virtual int Encode(const I420VideoFrame& input_image,
                     const CodecSpecificInfo* codec_specific_info,
                     const std::vector<VideoFrameType>* frame_types);

  virtual int RegisterEncodeCompleteCallback(EncodedImageCallback* callback);

  virtual int SetChannelParameters(uint32_t packet_loss, int rtt);

  virtual int SetRates(uint32_t new_bitrate_kbit, uint32_t frame_rate);

 private:
  // Call encoder initialize function and set control settings.
  int InitAndSetControlSettings(const VideoCodec* inst);

  // Update frame size for codec.
  int UpdateCodecFrameSize(const I420VideoFrame& input_image);

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
  bool feedback_mode_;
  int cpu_speed_;
  uint32_t rc_max_intra_target_;
  int token_partitions_;
  ReferencePictureSelection* rps_;
  TemporalLayers* temporal_layers_;
  vpx_codec_ctx_t* encoder_;
  vpx_codec_enc_cfg_t* config_;
  vpx_image_t* raw_;
  QualityScaler quality_scaler_;
};  // end of VP8Encoder class


class VP8DecoderImpl : public VP8Decoder {
 public:
  VP8DecoderImpl();

  virtual ~VP8DecoderImpl();

  virtual int InitDecode(const VideoCodec* inst, int number_of_cores);

  virtual int Decode(const EncodedImage& input_image,
                     bool missing_frames,
                     const RTPFragmentationHeader* fragmentation,
                     const CodecSpecificInfo* codec_specific_info,
                     int64_t /*render_time_ms*/);

  virtual int RegisterDecodeCompleteCallback(DecodedImageCallback* callback);

  virtual int Release();

  virtual int Reset();

  virtual VideoDecoder* Copy();

 private:
  // Copy reference image from this _decoder to the _decoder in copyTo. Set
  // which frame type to copy in _refFrame->frame_type before the call to
  // this function.
  int CopyReference(VP8Decoder* copy);

  int DecodePartitions(const EncodedImage& input_image,
                       const RTPFragmentationHeader* fragmentation);

  int ReturnFrame(const vpx_image_t* img,
                  uint32_t timeStamp,
                  int64_t ntp_time_ms);

  I420VideoFrame decoded_image_;
  DecodedImageCallback* decode_complete_callback_;
  bool inited_;
  bool feedback_mode_;
  vpx_dec_ctx_t* decoder_;
  VideoCodec codec_;
  EncodedImage last_keyframe_;
  int image_format_;
  vpx_ref_frame_t* ref_frame_;
  int propagation_cnt_;
  bool mfqe_enabled_;
  bool key_frame_required_;
};  // end of VP8Decoder class
}  // namespace webrtc

#endif  // WEBRTC_MODULES_VIDEO_CODING_CODECS_VP8_IMPL_H_
