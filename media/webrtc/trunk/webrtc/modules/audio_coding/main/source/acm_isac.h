/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_ISAC_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_ISAC_H_

#include "webrtc/modules/audio_coding/main/source/acm_generic_codec.h"

namespace webrtc {

struct ACMISACInst;

enum IsacCodingMode {
  ADAPTIVE,
  CHANNEL_INDEPENDENT
};

class ACMISAC : public ACMGenericCodec {
 public:
  explicit ACMISAC(int16_t codec_id);
  ~ACMISAC();

  // for FEC
  ACMGenericCodec* CreateInstance(void);

  int16_t InternalEncode(uint8_t* bitstream,
                         int16_t* bitstream_len_byte);

  int16_t InternalInitEncoder(WebRtcACMCodecParams *codec_params);

  int16_t InternalInitDecoder(WebRtcACMCodecParams *codec_params);

  int16_t DeliverCachedIsacData(uint8_t* bitstream,
                                int16_t* bitstream_len_byte,
                                uint32_t* timestamp,
                                WebRtcACMEncodingType* encoding_type,
                                const uint16_t isac_rate,
                                const uint8_t isac_bwestimate);

  int16_t DeliverCachedData(uint8_t* /* bitstream */,
                            int16_t* /* bitstream_len_byte */,
                            uint32_t* /* timestamp */,
                            WebRtcACMEncodingType* /* encoding_type */) {
    return -1;
  }

  int16_t UpdateDecoderSampFreq(int16_t codec_id);

  int16_t UpdateEncoderSampFreq(uint16_t samp_freq_hz);

  int16_t EncoderSampFreq(uint16_t& samp_freq_hz);

  int32_t ConfigISACBandwidthEstimator(
      const uint8_t init_frame_size_msec,
      const uint16_t init_rate_bit_per_sec,
      const bool enforce_frame_size);

  int32_t SetISACMaxPayloadSize(
      const uint16_t max_payload_len_bytes);

  int32_t SetISACMaxRate(const uint32_t max_rate_bit_per_sec);

  int16_t REDPayloadISAC(const int32_t isac_rate,
                         const int16_t isac_bw_estimate,
                         uint8_t* payload,
                         int16_t* payload_len_bytes);

 protected:
  int16_t DecodeSafe(uint8_t* bitstream,
                     int16_t bitstream_len_byte,
                     int16_t* audio,
                     int16_t* audio_samples,
                     int8_t* speech_type);

  int32_t CodecDef(WebRtcNetEQ_CodecDef& codec_def,
                   const CodecInst& codec_inst);

  void DestructEncoderSafe();

  void DestructDecoderSafe();

  int16_t SetBitRateSafe(const int32_t bit_rate);

  int32_t GetEstimatedBandwidthSafe();

  int32_t SetEstimatedBandwidthSafe(int32_t estimated_bandwidth);

  int32_t GetRedPayloadSafe(uint8_t* red_payload,
                            int16_t* payload_bytes);

  int16_t InternalCreateEncoder();

  int16_t InternalCreateDecoder();

  void InternalDestructEncoderInst(void* ptr_inst);

  int16_t Transcode(uint8_t* bitstream,
                    int16_t* bitstream_len_byte,
                    int16_t q_bwe,
                    int32_t rate,
                    bool is_red);

  void CurrentRate(int32_t& rate_bit_per_sec);

  void UpdateFrameLen();

  bool DecoderParamsSafe(WebRtcACMCodecParams *dec_params,
                         const uint8_t payload_type);

  void SaveDecoderParamSafe(const WebRtcACMCodecParams* codec_params);

  ACMISACInst* codec_inst_ptr_;
  bool is_enc_initialized_;
  IsacCodingMode isac_coding_mode_;
  bool enforce_frame_size_;
  int32_t isac_current_bn_;
  uint16_t samples_in_10ms_audio_;
  WebRtcACMCodecParams decoder_params_32khz_;
};

}  // namespace

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_ISAC_H_
