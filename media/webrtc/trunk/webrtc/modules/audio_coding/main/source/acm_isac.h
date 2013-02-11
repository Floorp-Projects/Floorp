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
  explicit ACMISAC(WebRtc_Word16 codec_id);
  ~ACMISAC();

  // for FEC
  ACMGenericCodec* CreateInstance(void);

  WebRtc_Word16 InternalEncode(WebRtc_UWord8* bitstream,
                               WebRtc_Word16* bitstream_len_byte);

  WebRtc_Word16 InternalInitEncoder(WebRtcACMCodecParams *codec_params);

  WebRtc_Word16 InternalInitDecoder(WebRtcACMCodecParams *codec_params);

  WebRtc_Word16 DeliverCachedIsacData(WebRtc_UWord8* bitstream,
                                      WebRtc_Word16* bitstream_len_byte,
                                      WebRtc_UWord32* timestamp,
                                      WebRtcACMEncodingType* encoding_type,
                                      const WebRtc_UWord16 isac_rate,
                                      const WebRtc_UWord8 isac_bwestimate);

  WebRtc_Word16 DeliverCachedData(WebRtc_UWord8* /* bitstream */,
                                  WebRtc_Word16* /* bitstream_len_byte */,
                                  WebRtc_UWord32* /* timestamp */,
                                  WebRtcACMEncodingType* /* encoding_type */) {
    return -1;
  }

  WebRtc_Word16 UpdateDecoderSampFreq(WebRtc_Word16 codec_id);

  WebRtc_Word16 UpdateEncoderSampFreq(WebRtc_UWord16 samp_freq_hz);

  WebRtc_Word16 EncoderSampFreq(WebRtc_UWord16& samp_freq_hz);

  WebRtc_Word32 ConfigISACBandwidthEstimator(
      const WebRtc_UWord8 init_frame_size_msec,
      const WebRtc_UWord16 init_rate_bit_per_sec,
      const bool enforce_frame_size);

  WebRtc_Word32 SetISACMaxPayloadSize(
      const WebRtc_UWord16 max_payload_len_bytes);

  WebRtc_Word32 SetISACMaxRate(const WebRtc_UWord32 max_rate_bit_per_sec);

  WebRtc_Word16 REDPayloadISAC(const WebRtc_Word32 isac_rate,
                               const WebRtc_Word16 isac_bw_estimate,
                               WebRtc_UWord8* payload,
                               WebRtc_Word16* payload_len_bytes);

 protected:
  WebRtc_Word16 DecodeSafe(WebRtc_UWord8* bitstream,
                           WebRtc_Word16 bitstream_len_byte,
                           WebRtc_Word16* audio,
                           WebRtc_Word16* audio_samples,
                           WebRtc_Word8* speech_type);

  WebRtc_Word32 CodecDef(WebRtcNetEQ_CodecDef& codec_def,
                         const CodecInst& codec_inst);

  void DestructEncoderSafe();

  void DestructDecoderSafe();

  WebRtc_Word16 SetBitRateSafe(const WebRtc_Word32 bit_rate);

  WebRtc_Word32 GetEstimatedBandwidthSafe();

  WebRtc_Word32 SetEstimatedBandwidthSafe(WebRtc_Word32 estimated_bandwidth);

  WebRtc_Word32 GetRedPayloadSafe(WebRtc_UWord8* red_payload,
                                  WebRtc_Word16* payload_bytes);

  WebRtc_Word16 InternalCreateEncoder();

  WebRtc_Word16 InternalCreateDecoder();

  void InternalDestructEncoderInst(void* ptr_inst);

  WebRtc_Word16 Transcode(WebRtc_UWord8* bitstream,
                          WebRtc_Word16* bitstream_len_byte,
                          WebRtc_Word16 q_bwe,
                          WebRtc_Word32 rate,
                          bool is_red);

  void CurrentRate(WebRtc_Word32& rate_bit_per_sec);

  void UpdateFrameLen();

  bool DecoderParamsSafe(WebRtcACMCodecParams *dec_params,
                         const WebRtc_UWord8 payload_type);

  void SaveDecoderParamSafe(const WebRtcACMCodecParams* codec_params);

  ACMISACInst* codec_inst_ptr_;
  bool is_enc_initialized_;
  IsacCodingMode isac_coding_mode_;
  bool enforce_frame_size_;
  WebRtc_Word32 isac_current_bn_;
  WebRtc_UWord16 samples_in_10ms_audio_;
  WebRtcACMCodecParams decoder_params_32khz_;
};

}  // namespace

#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_SOURCE_ACM_ISAC_H_
