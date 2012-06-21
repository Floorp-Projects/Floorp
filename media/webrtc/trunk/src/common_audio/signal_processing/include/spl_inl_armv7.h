/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


// This header file includes the inline functions for ARM processors in
// the fix point signal processing library.

#ifndef WEBRTC_SPL_SPL_INL_ARMV7_H_
#define WEBRTC_SPL_SPL_INL_ARMV7_H_

// TODO(kma): Replace some assembly code with GCC intrinsics
// (e.g. __builtin_clz).

static __inline WebRtc_Word32 WEBRTC_SPL_MUL_16_32_RSFT16(WebRtc_Word16 a,
                                                          WebRtc_Word32 b) {
  WebRtc_Word32 tmp;
  __asm__("smulwb %0, %1, %2":"=r"(tmp):"r"(b), "r"(a));
  return tmp;
}

static __inline WebRtc_Word32 WEBRTC_SPL_MUL_32_32_RSFT32(WebRtc_Word16 a,
                                                          WebRtc_Word16 b,
                                                          WebRtc_Word32 c) {
  WebRtc_Word32 tmp;
  __asm__("pkhbt %0, %1, %2, lsl #16" : "=r"(tmp) : "r"(b), "r"(a));
  __asm__("smmul %0, %1, %2":"=r"(tmp):"r"(tmp), "r"(c));
  return tmp;
}

static __inline WebRtc_Word32 WEBRTC_SPL_MUL_32_32_RSFT32BI(WebRtc_Word32 a,
                                                            WebRtc_Word32 b) {
  WebRtc_Word32 tmp;
  __asm__("smmul %0, %1, %2":"=r"(tmp):"r"(a), "r"(b));
  return tmp;
}

static __inline WebRtc_Word32 WEBRTC_SPL_MUL_16_16(WebRtc_Word16 a,
                                                   WebRtc_Word16 b) {
  WebRtc_Word32 tmp;
  __asm__("smulbb %0, %1, %2":"=r"(tmp):"r"(a), "r"(b));
  return tmp;
}

static __inline int32_t WebRtc_MulAccumW16(int16_t a,
                                          int16_t b,
                                          int32_t c) {
  int32_t tmp = 0;
  __asm__("smlabb %0, %1, %2, %3":"=r"(tmp):"r"(a), "r"(b), "r"(c));
  return tmp;
}

static __inline WebRtc_Word16 WebRtcSpl_AddSatW16(WebRtc_Word16 a,
                                                  WebRtc_Word16 b) {
  WebRtc_Word32 s_sum;

  __asm__("qadd16 %0, %1, %2":"=r"(s_sum):"r"(a), "r"(b));

  return (WebRtc_Word16) s_sum;
}

static __inline WebRtc_Word32 WebRtcSpl_AddSatW32(WebRtc_Word32 l_var1,
                                                  WebRtc_Word32 l_var2) {
  WebRtc_Word32 l_sum;

  __asm__("qadd %0, %1, %2":"=r"(l_sum):"r"(l_var1), "r"(l_var2));

  return l_sum;
}

static __inline WebRtc_Word16 WebRtcSpl_SubSatW16(WebRtc_Word16 var1,
                                                  WebRtc_Word16 var2) {
  WebRtc_Word32 s_sub;

  __asm__("qsub16 %0, %1, %2":"=r"(s_sub):"r"(var1), "r"(var2));

  return (WebRtc_Word16)s_sub;
}

static __inline WebRtc_Word32 WebRtcSpl_SubSatW32(WebRtc_Word32 l_var1,
                                                  WebRtc_Word32 l_var2) {
  WebRtc_Word32 l_sub;

  __asm__("qsub %0, %1, %2":"=r"(l_sub):"r"(l_var1), "r"(l_var2));

  return l_sub;
}

static __inline WebRtc_Word16 WebRtcSpl_GetSizeInBits(WebRtc_UWord32 n) {
  WebRtc_Word32 tmp;

  __asm__("clz %0, %1":"=r"(tmp):"r"(n));

  return (WebRtc_Word16)(32 - tmp);
}

static __inline int WebRtcSpl_NormW32(WebRtc_Word32 a) {
  WebRtc_Word32 tmp;

  if (a <= 0) a ^= 0xFFFFFFFF;

  __asm__("clz %0, %1":"=r"(tmp):"r"(a));

  return tmp - 1;
}

static __inline int WebRtcSpl_NormU32(WebRtc_UWord32 a) {
  int tmp;

  if (a == 0) return 0;

  __asm__("clz %0, %1":"=r"(tmp):"r"(a));

  return tmp;
}

static __inline int WebRtcSpl_NormW16(WebRtc_Word16 a) {
  WebRtc_Word32 tmp;

  if (a <= 0) a ^= 0xFFFFFFFF;

  __asm__("clz %0, %1":"=r"(tmp):"r"(a));

  return tmp - 17;
}

static __inline WebRtc_Word16 WebRtcSpl_SatW32ToW16(WebRtc_Word32 value32) {
  WebRtc_Word16 out16;

  __asm__("ssat %r0, #16, %r1" : "=r"(out16) : "r"(value32));

  return out16;
}
#endif  // WEBRTC_SPL_SPL_INL_ARMV7_H_
