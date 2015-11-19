/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_CODEC_MANAGER_H_
#define WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_CODEC_MANAGER_H_

#include "webrtc/base/constructormagic.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/modules/audio_coding/main/acm2/acm_codec_database.h"
#include "webrtc/modules/audio_coding/main/interface/audio_coding_module_typedefs.h"
#include "webrtc/common_types.h"

namespace webrtc {

class AudioDecoder;

namespace acm2 {

class ACMGenericCodec;
class AudioCodingModuleImpl;

class CodecManager final {
 public:
  explicit CodecManager(AudioCodingModuleImpl* acm);
  ~CodecManager();

  int RegisterSendCodec(const CodecInst& send_codec);

  int SendCodec(CodecInst* current_codec) const;

  int RegisterReceiveCodec(const CodecInst& receive_codec);

  bool SetCopyRed(bool enable);

  int SetVAD(bool enable_dtx, bool enable_vad, ACMVADMode mode);

  void VAD(bool* dtx_enabled, bool* vad_enabled, ACMVADMode* mode) const;

  int SetCodecFEC(bool enable_codec_fec);

  bool stereo_send() const { return stereo_send_; }

  bool red_enabled() const { return red_enabled_; }

  bool codec_fec_enabled() const { return codec_fec_enabled_; }

  ACMGenericCodec* current_encoder() { return current_encoder_; }

  const ACMGenericCodec* current_encoder() const { return current_encoder_; }

 private:
  void SetCngPayloadType(int sample_rate_hz, int payload_type);

  void SetRedPayloadType(int sample_rate_hz, int payload_type);

  // Get a pointer to AudioDecoder of the given codec. For some codecs, e.g.
  // iSAC, encoding and decoding have to be performed on a shared
  // codec-instance. By calling this method, we get the codec-instance that ACM
  // owns, then pass that to NetEq. This way, we perform both encoding and
  // decoding on the same codec-instance. Furthermore, ACM would have control
  // over decoder functionality if required. If |codec| does not share an
  // instance between encoder and decoder, the |*decoder| is set NULL.
  // The field ACMCodecDB::CodecSettings.owns_decoder indicates that if a
  // codec owns the decoder-instance. For such codecs |*decoder| should be a
  // valid pointer, otherwise it will be NULL.
  int GetAudioDecoder(const CodecInst& codec,
                      int codec_id,
                      int mirror_id,
                      AudioDecoder** decoder);

  AudioCodingModuleImpl* acm_;
  rtc::ThreadChecker thread_checker_;
  uint8_t cng_nb_pltype_;
  uint8_t cng_wb_pltype_;
  uint8_t cng_swb_pltype_;
  uint8_t cng_fb_pltype_;
  uint8_t red_nb_pltype_;
  bool stereo_send_;
  bool vad_enabled_;
  bool dtx_enabled_;
  ACMVADMode vad_mode_;
  ACMGenericCodec* current_encoder_;
  CodecInst send_codec_inst_;
  bool red_enabled_;
  bool codec_fec_enabled_;
  ACMGenericCodec* codecs_[ACMCodecDB::kMaxNumCodecs];
  int mirror_codec_idx_[ACMCodecDB::kMaxNumCodecs];

  DISALLOW_COPY_AND_ASSIGN(CodecManager);
};

}  // namespace acm2
}  // namespace webrtc
#endif  // WEBRTC_MODULES_AUDIO_CODING_MAIN_ACM2_CODEC_MANAGER_H_
