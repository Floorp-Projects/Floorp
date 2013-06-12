/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


/* This header file includes the inline functions for ARM processors in
 * the fix point signal processing library.
 */

#ifndef WEBRTC_SPL_SPL_INL_ARMV7_H_
#define WEBRTC_SPL_SPL_INL_ARMV7_H_

/* TODO(kma): Replace some assembly code with GCC intrinsics
 * (e.g. __builtin_clz).
 */

/* This function produces result that is not bit exact with that by the generic
 * C version in some cases, although the former is at least as accurate as the
 * later.
 */
static __inline WebRtc_Word32 WEBRTC_SPL_MUL_16_32_RSFT16(WebRtc_Word16 a,
                                                          WebRtc_Word32 b) {
  WebRtc_Word32 tmp = 0;
  __asm __volatile ("smulwb %0, %1, %2":"=r"(tmp):"r"(b), "r"(a));
  return tmp;
}

/* This function produces result that is not bit exact with that by the generic
 * C version in some cases, although the former is at least as accurate as the
 * later.
 */
static __inline WebRtc_Word32 WEBRTC_SPL_MUL_32_32_RSFT32(WebRtc_Word16 a,
                                                          WebRtc_Word16 b,
                                                          WebRtc_Word32 c) {
  WebRtc_Word32 tmp = 0;
  __asm __volatile (
    "pkhbt %[tmp], %[b], %[a], lsl #16\n\t"
    "smmulr %[tmp], %[tmp], %[c]\n\t"
    :[tmp]"+r"(tmp)
    :[a]"r"(a),
     [b]"r"(b),
     [c]"r"(c)
  );
  return tmp;
}

static __inline WebRtc_Word32 WEBRTC_SPL_MUL_32_32_RSFT32BI(WebRtc_Word32 a,
                                                            WebRtc_Word32 b) {
  WebRtc_Word32 tmp = 0;
  __asm volatile ("smmulr %0, %1, %2":"=r"(tmp):"r"(a), "r"(b));
  return tmp;
}

static __inline WebRtc_Word32 WEBRTC_SPL_MUL_16_16(WebRtc_Word16 a,
                                                   WebRtc_Word16 b) {
  WebRtc_Word32 tmp = 0;
  __asm __volatile ("smulbb %0, %1, %2":"=r"(tmp):"r"(a), "r"(b));
  return tmp;
}

// TODO(kma): add unit test.
static __inline int32_t WebRtc_MulAccumW16(int16_t a,
                                          int16_t b,
                                          int32_t c) {
  int32_t tmp = 0;
  __asm __volatile ("smlabb %0, %1, %2, %3":"=r"(tmp):"r"(a), "r"(b), "r"(c));
  return tmp;
}

static __inline WebRtc_Word16 WebRtcSpl_AddSatW16(WebRtc_Word16 a,
                                                  WebRtc_Word16 b) {
  WebRtc_Word32 s_sum = 0;

  __asm __volatile ("qadd16 %0, %1, %2":"=r"(s_sum):"r"(a), "r"(b));

  return (WebRtc_Word16) s_sum;
}

/* TODO(kma): find the cause of unittest errors by the next two functions:
 * http://code.google.com/p/webrtc/issues/detail?id=740.
 */
#if 0
static __inline WebRtc_Word32 WebRtcSpl_AddSatW32(WebRtc_Word32 l_var1,
                                                  WebRtc_Word32 l_var2) {
  WebRtc_Word32 l_sum = 0;

  __asm __volatile ("qadd %0, %1, %2":"=r"(l_sum):"r"(l_var1), "r"(l_var2));

  return l_sum;
}

static __inline WebRtc_Word32 WebRtcSpl_SubSatW32(WebRtc_Word32 l_var1,
                                                  WebRtc_Word32 l_var2) {
  WebRtc_Word32 l_sub = 0;

  __asm __volatile ("qsub %0, %1, %2":"=r"(l_sub):"r"(l_var1), "r"(l_var2));

  return l_sub;
}
#endif

static __inline WebRtc_Word16 WebRtcSpl_SubSatW16(WebRtc_Word16 var1,
                                                  WebRtc_Word16 var2) {
  WebRtc_Word32 s_sub = 0;

  __asm __volatile ("qsub16 %0, %1, %2":"=r"(s_sub):"r"(var1), "r"(var2));

  return (WebRtc_Word16)s_sub;
}

static __inline WebRtc_Word16 WebRtcSpl_GetSizeInBits(WebRtc_UWord32 n) {
  WebRtc_Word32 tmp = 0;

  __asm __volatile ("clz %0, %1":"=r"(tmp):"r"(n));

  return (WebRtc_Word16)(32 - tmp);
}

static __inline int WebRtcSpl_NormW32(WebRtc_Word32 a) {
  WebRtc_Word32 tmp = 0;

  if (a == 0) {
    return 0;
  }
  else if (a < 0) {
    a ^= 0xFFFFFFFF;
  }

  __asm __volatile ("clz %0, %1":"=r"(tmp):"r"(a));

  return tmp - 1;
}

static __inline int WebRtcSpl_NormU32(WebRtc_UWord32 a) {
  int tmp = 0;

  if (a == 0) return 0;

  __asm __volatile ("clz %0, %1":"=r"(tmp):"r"(a));

  return tmp;
}

static __inline int WebRtcSpl_NormW16(WebRtc_Word16 a) {
  WebRtc_Word32 tmp = 0;

  if (a == 0) {
    return 0;
  }
  else if (a < 0) {
    a ^= 0xFFFFFFFF;
  }

  __asm __volatile ("clz %0, %1":"=r"(tmp):"r"(a));

  return tmp - 17;
}

// TODO(kma): add unit test.
static __inline WebRtc_Word16 WebRtcSpl_SatW32ToW16(WebRtc_Word32 value32) {
  WebRtc_Word16 out16 = 0;

  __asm __volatile ("ssat %0, #16, %1" : "=r"(out16) : "r"(value32));

  return out16;
}

#endif  // WEBRTC_SPL_SPL_INL_ARMV7_H_
