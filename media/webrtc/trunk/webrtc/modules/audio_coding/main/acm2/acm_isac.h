/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_ACM_ISAC_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_ACM_ISAC_H_

#include "webrtc/modules/audio_coding/main/acm2/acm_generic_codec.h"

namespace webrtc {

struct ACMISACInst;
class AcmAudioDecoderIsac;

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

  int16_t InternalEncode(uint8_t* bitstream, int16_t* bitstream_len_byte);

  int16_t InternalInitEncoder(WebRtcACMCodecParams* codec_params);

  int16_t InternalInitDecoder(WebRtcACMCodecParams* codec_params);

  int16_t UpdateDecoderSampFreq(int16_t codec_id);

  int16_t UpdateEncoderSampFreq(uint16_t samp_freq_hz);

  int16_t EncoderSampFreq(uint16_t* samp_freq_hz);

  int32_t ConfigISACBandwidthEstimator(const uint8_t init_frame_size_msec,
                                       const uint16_t init_rate_bit_per_sec,
                                       const bool enforce_frame_size);

  int32_t SetISACMaxPayloadSize(const uint16_t max_payload_len_bytes);

  int32_t SetISACMaxRate(const uint32_t max_rate_bit_per_sec);

  int16_t REDPayloadISAC(const int32_t isac_rate,
                         const int16_t isac_bw_estimate,
                         uint8_t* payload,
                         int16_t* payload_len_bytes);

 protected:
  void DestructEncoderSafe();

  int16_t SetBitRateSafe(const int32_t bit_rate);

  int32_t GetEstimatedBandwidthSafe();

  int32_t SetEstimatedBandwidthSafe(int32_t estimated_bandwidth);

  int32_t GetRedPayloadSafe(uint8_t* red_payload, int16_t* payload_bytes);

  int16_t InternalCreateEncoder();

  void InternalDestructEncoderInst(void* ptr_inst);

  int16_t Transcode(uint8_t* bitstream,
                    int16_t* bitstream_len_byte,
                    int16_t q_bwe,
                    int32_t rate,
                    bool is_red);

  void CurrentRate(int32_t* rate_bit_per_sec);

  void UpdateFrameLen();

  virtual AudioDecoder* Decoder(int codec_id);

  ACMISACInst* codec_inst_ptr_;
  bool is_enc_initialized_;
  IsacCodingMode isac_coding_mode_;
  bool enforce_frame_size_;
  int32_t isac_current_bn_;
  uint16_t samples_in_10ms_audio_;
  AcmAudioDecoderIsac* audio_decoder_;
  bool decoder_initialized_;
};

}  // namespace webrtc

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_ACM_ISAC_H_
