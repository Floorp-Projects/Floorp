/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "acm_dtmf_playout.h"
#include "acm_common_defs.h"
#include "acm_neteq.h"
#include "trace.h"
#include "webrtc_neteq.h"
#include "webrtc_neteq_help_macros.h"

namespace webrtc {

#ifndef WEBRTC_CODEC_AVT

ACMDTMFPlayout::ACMDTMFPlayout(
    WebRtc_Word16 /* codecID */) {
  return;
}

ACMDTMFPlayout::~ACMDTMFPlayout() {
  return;
}

WebRtc_Word16 ACMDTMFPlayout::InternalEncode(
    WebRtc_UWord8* /* bitStream */,
    WebRtc_Word16* /* bitStreamLenByte */) {
  return -1;
}

WebRtc_Word16 ACMDTMFPlayout::DecodeSafe(WebRtc_UWord8* /* bitStream */,
                                         WebRtc_Word16 /* bitStreamLenByte */,
                                         WebRtc_Word16* /* audio */,
                                         WebRtc_Word16* /* audioSamples */,
                                         WebRtc_Word8* /* speechType */) {
  return -1;
}

WebRtc_Word16 ACMDTMFPlayout::InternalInitEncoder(
    WebRtcACMCodecParams* /* codecParams */) {
  return -1;
}

WebRtc_Word16 ACMDTMFPlayout::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */) {
  return -1;
}

WebRtc_Word32 ACMDTMFPlayout::CodecDef(WebRtcNetEQ_CodecDef& /* codecDef */,
                                       const CodecInst& /* codecInst */) {
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

void ACMDTMFPlayout::InternalDestructEncoderInst(void* /* ptrInst */) {
  return;
}

void ACMDTMFPlayout::DestructEncoderSafe() {
  return;
}

void ACMDTMFPlayout::DestructDecoderSafe() {
  return;
}

#else     //===================== Actual Implementation =======================

ACMDTMFPlayout::ACMDTMFPlayout(WebRtc_Word16 codecID) {
  _codecID = codecID;
}

ACMDTMFPlayout::~ACMDTMFPlayout() {
  return;
}

WebRtc_Word16 ACMDTMFPlayout::InternalEncode(
    WebRtc_UWord8* /* bitStream */,
    WebRtc_Word16* /* bitStreamLenByte */) {
  return 0;
}

WebRtc_Word16 ACMDTMFPlayout::DecodeSafe(WebRtc_UWord8* /* bitStream */,
                                         WebRtc_Word16 /* bitStreamLenByte */,
                                         WebRtc_Word16* /* audio */,
                                         WebRtc_Word16* /* audioSamples */,
                                         WebRtc_Word8* /* speechType */) {
  return 0;
}

WebRtc_Word16 ACMDTMFPlayout::InternalInitEncoder(
    WebRtcACMCodecParams* /* codecParams */) {
  // This codec does not need initialization,
  // DTMFPlayout has no instance
  return 0;
}

WebRtc_Word16 ACMDTMFPlayout::InternalInitDecoder(
    WebRtcACMCodecParams* /* codecParams */) {
  // This codec does not need initialization,
  // DTMFPlayout has no instance
  return 0;
}

WebRtc_Word32 ACMDTMFPlayout::CodecDef(WebRtcNetEQ_CodecDef& codecDef,
                                       const CodecInst& codecInst) {
  // Fill up the structure by calling
  // "SET_CODEC_PAR" & "SET_AVT_FUNCTION."
  // Then call NetEQ to add the codec to it's
  // database.
  SET_CODEC_PAR((codecDef), kDecoderAVT, codecInst.pltype, NULL, 8000);
  SET_AVT_FUNCTIONS((codecDef));
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

void ACMDTMFPlayout::InternalDestructEncoderInst(void* /* ptrInst */) {
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

} // namespace webrtc
