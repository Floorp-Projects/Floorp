/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_coding/main/source/acm_dtmf_playout.h"

#include "webrtc/modules/audio_coding/main/source/acm_common_defs.h"
#include "webrtc/modules/audio_coding/main/source/acm_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq.h"
#include "webrtc/modules/audio_coding/neteq/interface/webrtc_neteq_help_macros.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

#ifndef WEBRTC_CODEC_AVT

ACMDTMFPlayout::ACMDTMFPlayout(
    WebRtc_Word16 /* codec_id */) {
  return;
}

ACMDTMFPlayout::~ACMDTMFPlayout() {
  return;
}

WebRtc_Word16 ACMDTMFPlayout::InternalEncode(
    WebRtc_UWord8* /* bitstream */,
    WebRtc_Word16* /* bitstream_len_byte */) {
  return -1;
}

WebRtc_Word16 ACMDTMFPlayout::DecodeSafe(
    WebRtc_UWord8* /* bitstream */,
    WebRtc_Word16 /* bitstream_len_byte */,
    WebRtc_Word16* /* audio */,
    WebRtc_Word16* /* audio_samples */,
    WebRtc_Word8* /* speech_type */) {
  return -1;
}

WebRtc_Word16 ACMDTMFPlayout::InternalInitEncoder(
    WebRtcACMCodecParams* /* codec_params */) {
  return -1;
}

WebRtc_Word16 ACMDTMFPlayout::InternalInitDecoder(
    WebRtcACMCodecParams* /* codec_params */) {
  return -1;
}

WebRtc_Word32 ACMDTMFPlayout::CodecDef(WebRtcNetEQ_CodecDef& /* codec_def */,
                                       const CodecInst& /* codec_inst */) {
  return -1;
}

ACMGenericCodec* ACMDTMFPlayout::CreateInstance(void) {
  return NULL;
}

WebRtc_Word16 ACMDTMFPlayout::InternalCreateEncoder() {
  return -1;
}

WebRtc_Word16 ACMDTMFPlayout::InternalCreateDecoder() {
  return -1;
}

void ACMDTMFPlayout::InternalDestructEncoderInst(void* /* ptr_inst */) {
  return;
}

void ACMDTMFPlayout::DestructEncoderSafe() {
  return;
}

void ACMDTMFPlayout::DestructDecoderSafe() {
  return;
}

#else     //===================== Actual Implementation =======================

ACMDTMFPlayout::ACMDTMFPlayout(WebRtc_Word16 codec_id) {
  codec_id_ = codec_id;
}

ACMDTMFPlayout::~ACMDTMFPlayout() {
  return;
}

WebRtc_Word16 ACMDTMFPlayout::InternalEncode(
    WebRtc_UWord8* /* bitstream */,
    WebRtc_Word16* /* bitstream_len_byte */) {
  return 0;
}

WebRtc_Word16 ACMDTMFPlayout::DecodeSafe(
    WebRtc_UWord8* /* bitstream */,
    WebRtc_Word16 /* bitstream_len_byte */,
    WebRtc_Word16* /* audio */,
    WebRtc_Word16* /* audio_samples */,
    WebRtc_Word8* /* speech_type */) {
  return 0;
}

WebRtc_Word16 ACMDTMFPlayout::InternalInitEncoder(
    WebRtcACMCodecParams* /* codec_params */) {
  // This codec does not need initialization,
  // DTMFPlayout has no instance
  return 0;
}

WebRtc_Word16 ACMDTMFPlayout::InternalInitDecoder(
    WebRtcACMCodecParams* /* codec_params */) {
  // This codec does not need initialization,
  // DTMFPlayout has no instance
  return 0;
}

WebRtc_Word32 ACMDTMFPlayout::CodecDef(WebRtcNetEQ_CodecDef& codec_def,
                                       const CodecInst& codec_inst) {
  // Fill up the structure by calling
  // "SET_CODEC_PAR" & "SET_AVT_FUNCTION."
  // Then call NetEQ to add the codec to it's
  // database.
  SET_CODEC_PAR((codec_def), kDecoderAVT, codec_inst.pltype, NULL, 8000);
  SET_AVT_FUNCTIONS((codec_def));
  return 0;
}

ACMGenericCodec* ACMDTMFPlayout::CreateInstance(void) {
  return NULL;
}

WebRtc_Word16 ACMDTMFPlayout::InternalCreateEncoder() {
  // DTMFPlayout has no instance
  return 0;
}

WebRtc_Word16 ACMDTMFPlayout::InternalCreateDecoder() {
  // DTMFPlayout has no instance
  return 0;
}

void ACMDTMFPlayout::InternalDestructEncoderInst(void* /* ptr_inst */) {
  // DTMFPlayout has no instance
  return;
}

void ACMDTMFPlayout::DestructEncoderSafe() {
  // DTMFPlayout has no instance
  return;
}

void ACMDTMFPlayout::DestructDecoderSafe() {
  // DTMFPlayout has no instance
  return;
}

#endif

}  // namespace webrtc
