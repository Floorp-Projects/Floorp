/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <emmintrin.h>  /* SSE2 */
#include "vp9/common/vp9_loopfilter.h"
#include "vpx_ports/emmintrin_compat.h"

static void mb_lpf_horizontal_edge_w_sse2_8(unsigned char *s,
                                            int p,
                                            const unsigned char *_blimit,
                                            const unsigned char *_limit,
                                            const unsigned char *_thresh) {
  __m128i mask, hev, flat, flat2;
  const __m128i zero = _mm_set1_epi16(0);
  const __m128i one = _mm_set1_epi8(1);
  __m128i q7p7, q6p6, q5p5, q4p4, q3p3, q2p2, q1p1, q0p0, p0q0, p1q1;
  __m128i abs_p1p0;
  const unsigned int extended_thresh = _thresh[0] * 0x01010101u;
  const unsigned int extended_limit  = _limit[0]  * 0x01010101u;
  const unsigned int extended_blimit = _blimit[0] * 0x01010101u;
  const __m128i thresh =
      _mm_shuffle_epi32(_mm_cvtsi32_si128((int)extended_thresh), 0);
  const __m128i limit =
      _mm_shuffle_epi32(_mm_cvtsi32_si128((int)extended_limit), 0);
  const __m128i blimit =
      _mm_shuffle_epi32(_mm_cvtsi32_si128((int)extended_blimit), 0);

  q4p4 = _mm_loadl_epi64((__m128i *)(s - 5 * p));
  q4p4 = _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(q4p4),
                                       (__m64 *)(s + 4 * p)));
  q3p3 = _mm_loadl_epi64((__m128i *)(s - 4 * p));
  q3p3 = _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(q3p3),
                                       (__m64 *)(s + 3 * p)));
  q2p2 = _mm_loadl_epi64((__m128i *)(s - 3 * p));
  q2p2 = _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(q2p2),
                                       (__m64 *)(s + 2 * p)));
  q1p1 = _mm_loadl_epi64((__m128i *)(s - 2 * p));
  q1p1 = _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(q1p1),
                                       (__m64 *)(s + 1 * p)));
  p1q1 = _mm_shuffle_epi32(q1p1, 78);
  q0p0 = _mm_loadl_epi64((__m128i *)(s - 1 * p));
  q0p0 = _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(q0p0),
                                       (__m64 *)(s - 0 * p)));
  p0q0 = _mm_shuffle_epi32(q0p0, 78);

  {
    __m128i abs_p1q1, abs_p0q0, abs_q1q0, fe, ff, work;
    abs_p1p0 = _mm_or_si128(_mm_subs_epu8(q1p1, q0p0),
                            _mm_subs_epu8(q0p0, q1p1));
    abs_q1q0 =  _mm_srli_si128(abs_p1p0, 8);
    fe = _mm_set1_epi8(0xfe);
    ff = _mm_cmpeq_epi8(abs_p1p0, abs_p1p0);
    abs_p0q0 = _mm_or_si128(_mm_subs_epu8(q0p0, p0q0),
                            _mm_subs_epu8(p0q0, q0p0));
    abs_p1q1 = _mm_or_si128(_mm_subs_epu8(q1p1, p1q1),
                            _mm_subs_epu8(p1q1, q1p1));
    flat = _mm_max_epu8(abs_p1p0, abs_q1q0);
    hev = _mm_subs_epu8(flat, thresh);
    hev = _mm_xor_si128(_mm_cmpeq_epi8(hev, zero), ff);

    abs_p0q0 =_mm_adds_epu8(abs_p0q0, abs_p0q0);
    abs_p1q1 = _mm_srli_epi16(_mm_and_si128(abs_p1q1, fe), 1);
    mask = _mm_subs_epu8(_mm_adds_epu8(abs_p0q0, abs_p1q1), blimit);
    mask = _mm_xor_si128(_mm_cmpeq_epi8(mask, zero), ff);
    // mask |= (abs(p0 - q0) * 2 + abs(p1 - q1) / 2  > blimit) * -1;
    mask = _mm_max_epu8(abs_p1p0, mask);
    // mask |= (abs(p1 - p0) > limit) * -1;
    // mask |= (abs(q1 - q0) > limit) * -1;

    work = _mm_max_epu8(_mm_or_si128(_mm_subs_epu8(q2p2, q1p1),
                                     _mm_subs_epu8(q1p1, q2p2)),
                        _mm_or_si128(_mm_subs_epu8(q3p3, q2p2),
                                     _mm_subs_epu8(q2p2, q3p3)));
    mask = _mm_max_epu8(work, mask);
    mask = _mm_max_epu8(mask, _mm_srli_si128(mask, 8));
    mask = _mm_subs_epu8(mask, limit);
    mask = _mm_cmpeq_epi8(mask, zero);
  }

  // lp filter
  {
    const __m128i t4 = _mm_set1_epi8(4);
    const __m128i t3 = _mm_set1_epi8(3);
    const __m128i t80 = _mm_set1_epi8(0x80);
    const __m128i t1 = _mm_set1_epi16(0x1);
    __m128i qs1ps1 = _mm_xor_si128(q1p1, t80);
    __m128i qs0ps0 = _mm_xor_si128(q0p0, t80);
    __m128i qs0 = _mm_xor_si128(p0q0, t80);
    __m128i qs1 = _mm_xor_si128(p1q1, t80);
    __m128i filt;
    __m128i work_a;
    __m128i filter1, filter2;
    __m128i flat2_q6p6, flat2_q5p5, flat2_q4p4, flat2_q3p3, flat2_q2p2;
    __m128i flat2_q1p1, flat2_q0p0, flat_q2p2, flat_q1p1, flat_q0p0;

    filt = _mm_and_si128(_mm_subs_epi8(qs1ps1, qs1), hev);
    work_a = _mm_subs_epi8(qs0, qs0ps0);
    filt = _mm_adds_epi8(filt, work_a);
    filt = _mm_adds_epi8(filt, work_a);
    filt = _mm_adds_epi8(filt, work_a);
    /* (vp9_filter + 3 * (qs0 - ps0)) & mask */
    filt = _mm_and_si128(filt, mask);

    filter1 = _mm_adds_epi8(filt, t4);
    filter2 = _mm_adds_epi8(filt, t3);

    filter1 = _mm_unpacklo_epi8(zero, filter1);
    filter1 = _mm_srai_epi16(filter1, 0xB);
    filter2 = _mm_unpacklo_epi8(zero, filter2);
    filter2 = _mm_srai_epi16(filter2, 0xB);

    /* Filter1 >> 3 */
    filt = _mm_packs_epi16(filter2, _mm_subs_epi16(zero, filter1));
    qs0ps0 = _mm_xor_si128(_mm_adds_epi8(qs0ps0, filt), t80);

    /* filt >> 1 */
    filt = _mm_adds_epi16(filter1, t1);
    filt = _mm_srai_epi16(filt, 1);
    filt = _mm_andnot_si128(_mm_srai_epi16(_mm_unpacklo_epi8(zero, hev), 0x8),
                            filt);
    filt = _mm_packs_epi16(filt, _mm_subs_epi16(zero, filt));
    qs1ps1 = _mm_xor_si128(_mm_adds_epi8(qs1ps1, filt), t80);
    // loopfilter done

    {
      __m128i work;
      flat = _mm_max_epu8(_mm_or_si128(_mm_subs_epu8(q2p2, q0p0),
                                       _mm_subs_epu8(q0p0, q2p2)),
                          _mm_or_si128(_mm_subs_epu8(q3p3, q0p0),
                                       _mm_subs_epu8(q0p0, q3p3)));
      flat = _mm_max_epu8(abs_p1p0, flat);
      flat = _mm_max_epu8(flat, _mm_srli_si128(flat, 8));
      flat = _mm_subs_epu8(flat, one);
      flat = _mm_cmpeq_epi8(flat, zero);
      flat = _mm_and_si128(flat, mask);

      q5p5 = _mm_loadl_epi64((__m128i *)(s - 6 * p));
      q5p5 = _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(q5p5),
                                           (__m64 *)(s + 5 * p)));

      q6p6 = _mm_loadl_epi64((__m128i *)(s - 7 * p));
      q6p6 = _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(q6p6),
                                           (__m64 *)(s + 6 * p)));

      flat2 = _mm_max_epu8(_mm_or_si128(_mm_subs_epu8(q4p4, q0p0),
                                        _mm_subs_epu8(q0p0, q4p4)),
                           _mm_or_si128(_mm_subs_epu8(q5p5, q0p0),
                                        _mm_subs_epu8(q0p0, q5p5)));

      q7p7 = _mm_loadl_epi64((__m128i *)(s - 8 * p));
      q7p7 = _mm_castps_si128(_mm_loadh_pi(_mm_castsi128_ps(q7p7),
                                           (__m64 *)(s + 7 * p)));

      work = _mm_max_epu8(_mm_or_si128(_mm_subs_epu8(q6p6, q0p0),
                                       _mm_subs_epu8(q0p0, q6p6)),
                          _mm_or_si128(_mm_subs_epu8(q7p7, q0p0),
                                       _mm_subs_epu8(q0p0, q7p7)));

      flat2 = _mm_max_epu8(work, flat2);
      flat2 = _mm_max_epu8(flat2, _mm_srli_si128(flat2, 8));
      flat2 = _mm_subs_epu8(flat2, one);
      flat2 = _mm_cmpeq_epi8(flat2, zero);
      flat2 = _mm_and_si128(flat2, flat);  // flat2 & flat & mask
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // flat and wide flat calculations
    {
      const __m128i eight = _mm_set1_epi16(8);
      const __m128i four = _mm_set1_epi16(4);
      __m128i p7_16, p6_16, p5_16, p4_16, p3_16, p2_16, p1_16, p0_16;
      __m128i q7_16, q6_16, q5_16, q4_16, q3_16, q2_16, q1_16, q0_16;
      __m128i pixelFilter_p, pixelFilter_q;
      __m128i pixetFilter_p2p1p0, pixetFilter_q2q1q0;
      __m128i sum_p7, sum_q7, sum_p3, sum_q3, res_p, res_q;

      p7_16 = _mm_unpacklo_epi8(q7p7, zero);;
      p6_16 = _mm_unpacklo_epi8(q6p6, zero);
      p5_16 = _mm_unpacklo_epi8(q5p5, zero);
      p4_16 = _mm_unpacklo_epi8(q4p4, zero);
      p3_16 = _mm_unpacklo_epi8(q3p3, zero);
      p2_16 = _mm_unpacklo_epi8(q2p2, zero);
      p1_16 = _mm_unpacklo_epi8(q1p1, zero);
      p0_16 = _mm_unpacklo_epi8(q0p0, zero);
      q0_16 = _mm_unpackhi_epi8(q0p0, zero);
      q1_16 = _mm_unpackhi_epi8(q1p1, zero);
      q2_16 = _mm_unpackhi_epi8(q2p2, zero);
      q3_16 = _mm_unpackhi_epi8(q3p3, zero);
      q4_16 = _mm_unpackhi_epi8(q4p4, zero);
      q5_16 = _mm_unpackhi_epi8(q5p5, zero);
      q6_16 = _mm_unpackhi_epi8(q6p6, zero);
      q7_16 = _mm_unpackhi_epi8(q7p7, zero);

      pixelFilter_p = _mm_add_epi16(_mm_add_epi16(p6_16, p5_16),
                                    _mm_add_epi16(p4_16, p3_16));
      pixelFilter_q = _mm_add_epi16(_mm_add_epi16(q6_16, q5_16),
                                    _mm_add_epi16(q4_16, q3_16));

      pixetFilter_p2p1p0 = _mm_add_epi16(p0_16, _mm_add_epi16(p2_16, p1_16));
      pixelFilter_p =  _mm_add_epi16(pixelFilter_p, pixetFilter_p2p1p0);

      pixetFilter_q2q1q0 = _mm_add_epi16(q0_16, _mm_add_epi16(q2_16, q1_16));
      pixelFilter_q =  _mm_add_epi16(pixelFilter_q, pixetFilter_q2q1q0);
      pixelFilter_p =  _mm_add_epi16(eight, _mm_add_epi16(pixelFilter_p,
                                                         pixelFilter_q));
      pixetFilter_p2p1p0 =   _mm_add_epi16(four,
                                           _mm_add_epi16(pixetFilter_p2p1p0,
                                                         pixetFilter_q2q1q0));
      res_p = _mm_srli_epi16(_mm_add_epi16(pixelFilter_p,
                                           _mm_add_epi16(p7_16, p0_16)), 4);
      res_q = _mm_srli_epi16(_mm_add_epi16(pixelFilter_p,
                                           _mm_add_epi16(q7_16, q0_16)), 4);
      flat2_q0p0 = _mm_packus_epi16(res_p, res_q);
      res_p = _mm_srli_epi16(_mm_add_epi16(pixetFilter_p2p1p0,
                                           _mm_add_epi16(p3_16, p0_16)), 3);
      res_q = _mm_srli_epi16(_mm_add_epi16(pixetFilter_p2p1p0,
                                           _mm_add_epi16(q3_16, q0_16)), 3);

      flat_q0p0 = _mm_packus_epi16(res_p, res_q);

      sum_p7 = _mm_add_epi16(p7_16, p7_16);
      sum_q7 = _mm_add_epi16(q7_16, q7_16);
      sum_p3 = _mm_add_epi16(p3_16, p3_16);
      sum_q3 = _mm_add_epi16(q3_16, q3_16);

      pixelFilter_q = _mm_sub_epi16(pixelFilter_p, p6_16);
      pixelFilter_p = _mm_sub_epi16(pixelFilter_p, q6_16);
      res_p = _mm_srli_epi16(_mm_add_epi16(pixelFilter_p,
                             _mm_add_epi16(sum_p7, p1_16)), 4);
      res_q = _mm_srli_epi16(_mm_add_epi16(pixelFilter_q,
                             _mm_add_epi16(sum_q7, q1_16)), 4);
      flat2_q1p1 = _mm_packus_epi16(res_p, res_q);

      pixetFilter_q2q1q0 = _mm_sub_epi16(pixetFilter_p2p1p0, p2_16);
      pixetFilter_p2p1p0 = _mm_sub_epi16(pixetFilter_p2p1p0, q2_16);
      res_p = _mm_srli_epi16(_mm_add_epi16(pixetFilter_p2p1p0,
                             _mm_add_epi16(sum_p3, p1_16)), 3);
      res_q = _mm_srli_epi16(_mm_add_epi16(pixetFilter_q2q1q0,
                             _mm_add_epi16(sum_q3, q1_16)), 3);
      flat_q1p1 = _mm_packus_epi16(res_p, res_q);

      sum_p7 = _mm_add_epi16(sum_p7, p7_16);
      sum_q7 = _mm_add_epi16(sum_q7, q7_16);
      sum_p3 = _mm_add_epi16(sum_p3, p3_16);
      sum_q3 = _mm_add_epi16(sum_q3, q3_16);

      pixelFilter_p = _mm_sub_epi16(pixelFilter_p, q5_16);
      pixelFilter_q = _mm_sub_epi16(pixelFilter_q, p5_16);
      res_p = _mm_srli_epi16(_mm_add_epi16(pixelFilter_p,
                             _mm_add_epi16(sum_p7, p2_16)), 4);
      res_q = _mm_srli_epi16(_mm_add_epi16(pixelFilter_q,
                             _mm_add_epi16(sum_q7, q2_16)), 4);
      flat2_q2p2 = _mm_packus_epi16(res_p, res_q);

      pixetFilter_p2p1p0 = _mm_sub_epi16(pixetFilter_p2p1p0, q1_16);
      pixetFilter_q2q1q0 = _mm_sub_epi16(pixetFilter_q2q1q0, p1_16);

      res_p = _mm_srli_epi16(_mm_add_epi16(pixetFilter_p2p1p0,
                                           _mm_add_epi16(sum_p3, p2_16)), 3);
      res_q = _mm_srli_epi16(_mm_add_epi16(pixetFilter_q2q1q0,
                                           _mm_add_epi16(sum_q3, q2_16)), 3);
      flat_q2p2 = _mm_packus_epi16(res_p, res_q);

      sum_p7 = _mm_add_epi16(sum_p7, p7_16);
      sum_q7 = _mm_add_epi16(sum_q7, q7_16);
      pixelFilter_p = _mm_sub_epi16(pixelFilter_p, q4_16);
      pixelFilter_q = _mm_sub_epi16(pixelFilter_q, p4_16);
      res_p = _mm_srli_epi16(_mm_add_epi16(pixelFilter_p,
                             _mm_add_epi16(sum_p7, p3_16)), 4);
      res_q = _mm_srli_epi16(_mm_add_epi16(pixelFilter_q,
                             _mm_add_epi16(sum_q7, q3_16)), 4);
      flat2_q3p3 = _mm_packus_epi16(res_p, res_q);

      sum_p7 = _mm_add_epi16(sum_p7, p7_16);
      sum_q7 = _mm_add_epi16(sum_q7, q7_16);
      pixelFilter_p = _mm_sub_epi16(pixelFilter_p, q3_16);
      pixelFilter_q = _mm_sub_epi16(pixelFilter_q, p3_16);
      res_p = _mm_srli_epi16(_mm_add_epi16(pixelFilter_p,
                             _mm_add_epi16(sum_p7, p4_16)), 4);
      res_q = _mm_srli_epi16(_mm_add_epi16(pixelFilter_q,
                             _mm_add_epi16(sum_q7, q4_16)), 4);
      flat2_q4p4 = _mm_packus_epi16(res_p, res_q);

      sum_p7 = _mm_add_epi16(sum_p7, p7_16);
      sum_q7 = _mm_add_epi16(sum_q7, q7_16);
      pixelFilter_p = _mm_sub_epi16(pixelFilter_p, q2_16);
      pixelFilter_q = _mm_sub_epi16(pixelFilter_q, p2_16);
      res_p = _mm_srli_epi16(_mm_add_epi16(pixelFilter_p,
                             _mm_add_epi16(sum_p7, p5_16)), 4);
      res_q = _mm_srli_epi16(_mm_add_epi16(pixelFilter_q,
                             _mm_add_epi16(sum_q7, q5_16)), 4);
      flat2_q5p5 = _mm_packus_epi16(res_p, res_q);

      sum_p7 = _mm_add_epi16(sum_p7, p7_16);
      sum_q7 = _mm_add_epi16(sum_q7, q7_16);
      pixelFilter_p = _mm_sub_epi16(pixelFilter_p, q1_16);
      pixelFilter_q = _mm_sub_epi16(pixelFilter_q, p1_16);
      res_p = _mm_srli_epi16(_mm_add_epi16(pixelFilter_p,
                             _mm_add_epi16(sum_p7, p6_16)), 4);
      res_q = _mm_srli_epi16(_mm_add_epi16(pixelFilter_q,
                             _mm_add_epi16(sum_q7, q6_16)), 4);
      flat2_q6p6 = _mm_packus_epi16(res_p, res_q);
    }
    // wide flat
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    flat = _mm_shuffle_epi32(flat, 68);
    flat2 = _mm_shuffle_epi32(flat2, 68);

    q2p2 = _mm_andnot_si128(flat, q2p2);
    flat_q2p2 = _mm_and_si128(flat, flat_q2p2);
    q2p2 = _mm_or_si128(q2p2, flat_q2p2);

    qs1ps1 = _mm_andnot_si128(flat, qs1ps1);
    flat_q1p1 = _mm_and_si128(flat, flat_q1p1);
    q1p1 = _mm_or_si128(qs1ps1, flat_q1p1);

    qs0ps0 = _mm_andnot_si128(flat, qs0ps0);
    flat_q0p0 = _mm_and_si128(flat, flat_q0p0);
    q0p0 = _mm_or_si128(qs0ps0, flat_q0p0);

    q6p6 = _mm_andnot_si128(flat2, q6p6);
    flat2_q6p6 = _mm_and_si128(flat2, flat2_q6p6);
    q6p6 = _mm_or_si128(q6p6, flat2_q6p6);
    _mm_storel_epi64((__m128i *)(s - 7 * p), q6p6);
    _mm_storeh_pi((__m64 *)(s + 6 * p), _mm_castsi128_ps(q6p6));

    q5p5 = _mm_andnot_si128(flat2, q5p5);
    flat2_q5p5 = _mm_and_si128(flat2, flat2_q5p5);
    q5p5 = _mm_or_si128(q5p5, flat2_q5p5);
    _mm_storel_epi64((__m128i *)(s - 6 * p), q5p5);
    _mm_storeh_pi((__m64 *)(s + 5 * p), _mm_castsi128_ps(q5p5));

    q4p4 = _mm_andnot_si128(flat2, q4p4);
    flat2_q4p4 = _mm_and_si128(flat2, flat2_q4p4);
    q4p4 = _mm_or_si128(q4p4, flat2_q4p4);
    _mm_storel_epi64((__m128i *)(s - 5 * p), q4p4);
    _mm_storeh_pi((__m64 *)(s + 4 * p), _mm_castsi128_ps(q4p4));

    q3p3 = _mm_andnot_si128(flat2, q3p3);
    flat2_q3p3 = _mm_and_si128(flat2, flat2_q3p3);
    q3p3 = _mm_or_si128(q3p3, flat2_q3p3);
    _mm_storel_epi64((__m128i *)(s - 4 * p), q3p3);
    _mm_storeh_pi((__m64 *)(s + 3 * p), _mm_castsi128_ps(q3p3));

    q2p2 = _mm_andnot_si128(flat2, q2p2);
    flat2_q2p2 = _mm_and_si128(flat2, flat2_q2p2);
    q2p2 = _mm_or_si128(q2p2, flat2_q2p2);
    _mm_storel_epi64((__m128i *)(s - 3 * p), q2p2);
    _mm_storeh_pi((__m64 *)(s + 2 * p), _mm_castsi128_ps(q2p2));

    q1p1 = _mm_andnot_si128(flat2, q1p1);
    flat2_q1p1 = _mm_and_si128(flat2, flat2_q1p1);
    q1p1 = _mm_or_si128(q1p1, flat2_q1p1);
    _mm_storel_epi64((__m128i *)(s - 2 * p), q1p1);
    _mm_storeh_pi((__m64 *)(s + 1 * p), _mm_castsi128_ps(q1p1));

    q0p0 = _mm_andnot_si128(flat2, q0p0);
    flat2_q0p0 = _mm_and_si128(flat2, flat2_q0p0);
    q0p0 = _mm_or_si128(q0p0, flat2_q0p0);
    _mm_storel_epi64((__m128i *)(s - 1 * p), q0p0);
    _mm_storeh_pi((__m64 *)(s - 0 * p),  _mm_castsi128_ps(q0p0));
  }
}

static void mb_lpf_horizontal_edge_w_sse2_16(unsigned char *s,
                                             int p,
                                             const unsigned char *_blimit,
                                             const unsigned char *_limit,
                                             const unsigned char *_thresh) {
  DECLARE_ALIGNED(16, unsigned char, flat2_op[7][16]);
  DECLARE_ALIGNED(16, unsigned char, flat2_oq[7][16]);

  DECLARE_ALIGNED(16, unsigned char, flat_op[3][16]);
  DECLARE_ALIGNED(16, unsigned char, flat_oq[3][16]);

  DECLARE_ALIGNED(16, unsigned char, ap[8][16]);
  DECLARE_ALIGNED(16, unsigned char, aq[8][16]);


  __m128i mask, hev, flat, flat2;
  const __m128i zero = _mm_set1_epi16(0);
  const __m128i one = _mm_set1_epi8(1);
  __m128i p7, p6, p5;
  __m128i p4, p3, p2, p1, p0, q0, q1, q2, q3, q4;
  __m128i q5, q6, q7;
  int i = 0;
  const unsigned int extended_thresh = _thresh[0] * 0x01010101u;
  const unsigned int extended_limit  = _limit[0]  * 0x01010101u;
  const unsigned int extended_blimit = _blimit[0] * 0x01010101u;
  const __m128i thresh =
      _mm_shuffle_epi32(_mm_cvtsi32_si128((int)extended_thresh), 0);
  const __m128i limit =
      _mm_shuffle_epi32(_mm_cvtsi32_si128((int)extended_limit), 0);
  const __m128i blimit =
      _mm_shuffle_epi32(_mm_cvtsi32_si128((int)extended_blimit), 0);

  p4 = _mm_loadu_si128((__m128i *)(s - 5 * p));
  p3 = _mm_loadu_si128((__m128i *)(s - 4 * p));
  p2 = _mm_loadu_si128((__m128i *)(s - 3 * p));
  p1 = _mm_loadu_si128((__m128i *)(s - 2 * p));
  p0 = _mm_loadu_si128((__m128i *)(s - 1 * p));
  q0 = _mm_loadu_si128((__m128i *)(s - 0 * p));
  q1 = _mm_loadu_si128((__m128i *)(s + 1 * p));
  q2 = _mm_loadu_si128((__m128i *)(s + 2 * p));
  q3 = _mm_loadu_si128((__m128i *)(s + 3 * p));
  q4 = _mm_loadu_si128((__m128i *)(s + 4 * p));

  _mm_store_si128((__m128i *)ap[4], p4);
  _mm_store_si128((__m128i *)ap[3], p3);
  _mm_store_si128((__m128i *)ap[2], p2);
  _mm_store_si128((__m128i *)ap[1], p1);
  _mm_store_si128((__m128i *)ap[0], p0);
  _mm_store_si128((__m128i *)aq[4], q4);
  _mm_store_si128((__m128i *)aq[3], q3);
  _mm_store_si128((__m128i *)aq[2], q2);
  _mm_store_si128((__m128i *)aq[1], q1);
  _mm_store_si128((__m128i *)aq[0], q0);


  {
    const __m128i abs_p1p0 = _mm_or_si128(_mm_subs_epu8(p1, p0),
                                          _mm_subs_epu8(p0, p1));
    const __m128i abs_q1q0 = _mm_or_si128(_mm_subs_epu8(q1, q0),
                                          _mm_subs_epu8(q0, q1));
    const __m128i fe = _mm_set1_epi8(0xfe);
    const __m128i ff = _mm_cmpeq_epi8(abs_p1p0, abs_p1p0);
    __m128i abs_p0q0 = _mm_or_si128(_mm_subs_epu8(p0, q0),
                                    _mm_subs_epu8(q0, p0));
    __m128i abs_p1q1 = _mm_or_si128(_mm_subs_epu8(p1, q1),
                                    _mm_subs_epu8(q1, p1));
    __m128i work;
    flat = _mm_max_epu8(abs_p1p0, abs_q1q0);
    hev = _mm_subs_epu8(flat, thresh);
    hev = _mm_xor_si128(_mm_cmpeq_epi8(hev, zero), ff);

    abs_p0q0 =_mm_adds_epu8(abs_p0q0, abs_p0q0);
    abs_p1q1 = _mm_srli_epi16(_mm_and_si128(abs_p1q1, fe), 1);
    mask = _mm_subs_epu8(_mm_adds_epu8(abs_p0q0, abs_p1q1), blimit);
    mask = _mm_xor_si128(_mm_cmpeq_epi8(mask, zero), ff);
    // mask |= (abs(p0 - q0) * 2 + abs(p1 - q1) / 2  > blimit) * -1;
    mask = _mm_max_epu8(flat, mask);
    // mask |= (abs(p1 - p0) > limit) * -1;
    // mask |= (abs(q1 - q0) > limit) * -1;
    work = _mm_max_epu8(_mm_or_si128(_mm_subs_epu8(p2, p1),
                                     _mm_subs_epu8(p1, p2)),
                         _mm_or_si128(_mm_subs_epu8(p3, p2),
                                      _mm_subs_epu8(p2, p3)));
    mask = _mm_max_epu8(work, mask);
    work = _mm_max_epu8(_mm_or_si128(_mm_subs_epu8(q2, q1),
                                     _mm_subs_epu8(q1, q2)),
                         _mm_or_si128(_mm_subs_epu8(q3, q2),
                                      _mm_subs_epu8(q2, q3)));
    mask = _mm_max_epu8(work, mask);
    mask = _mm_subs_epu8(mask, limit);
    mask = _mm_cmpeq_epi8(mask, zero);
  }

  // lp filter
  {
    const __m128i t4 = _mm_set1_epi8(4);
    const __m128i t3 = _mm_set1_epi8(3);
    const __m128i t80 = _mm_set1_epi8(0x80);
    const __m128i te0 = _mm_set1_epi8(0xe0);
    const __m128i t1f = _mm_set1_epi8(0x1f);
    const __m128i t1 = _mm_set1_epi8(0x1);
    const __m128i t7f = _mm_set1_epi8(0x7f);

    __m128i ps1 = _mm_xor_si128(p1, t80);
    __m128i ps0 = _mm_xor_si128(p0, t80);
    __m128i qs0 = _mm_xor_si128(q0, t80);
    __m128i qs1 = _mm_xor_si128(q1, t80);
    __m128i filt;
    __m128i work_a;
    __m128i filter1, filter2;

    filt = _mm_and_si128(_mm_subs_epi8(ps1, qs1), hev);
    work_a = _mm_subs_epi8(qs0, ps0);
    filt = _mm_adds_epi8(filt, work_a);
    filt = _mm_adds_epi8(filt, work_a);
    filt = _mm_adds_epi8(filt, work_a);
    /* (vp9_filter + 3 * (qs0 - ps0)) & mask */
    filt = _mm_and_si128(filt, mask);

    filter1 = _mm_adds_epi8(filt, t4);
    filter2 = _mm_adds_epi8(filt, t3);

    /* Filter1 >> 3 */
    work_a = _mm_cmpgt_epi8(zero, filter1);
    filter1 = _mm_srli_epi16(filter1, 3);
    work_a = _mm_and_si128(work_a, te0);
    filter1 = _mm_and_si128(filter1, t1f);
    filter1 = _mm_or_si128(filter1, work_a);
    qs0 = _mm_xor_si128(_mm_subs_epi8(qs0, filter1), t80);

    /* Filter2 >> 3 */
    work_a = _mm_cmpgt_epi8(zero, filter2);
    filter2 = _mm_srli_epi16(filter2, 3);
    work_a = _mm_and_si128(work_a, te0);
    filter2 = _mm_and_si128(filter2, t1f);
    filter2 = _mm_or_si128(filter2, work_a);
    ps0 = _mm_xor_si128(_mm_adds_epi8(ps0, filter2), t80);

    /* filt >> 1 */
    filt = _mm_adds_epi8(filter1, t1);
    work_a = _mm_cmpgt_epi8(zero, filt);
    filt = _mm_srli_epi16(filt, 1);
    work_a = _mm_and_si128(work_a, t80);
    filt = _mm_and_si128(filt, t7f);
    filt = _mm_or_si128(filt, work_a);
    filt = _mm_andnot_si128(hev, filt);
    ps1 = _mm_xor_si128(_mm_adds_epi8(ps1, filt), t80);
    qs1 = _mm_xor_si128(_mm_subs_epi8(qs1, filt), t80);
    // loopfilter done

    {
      __m128i work;
      work = _mm_max_epu8(_mm_or_si128(_mm_subs_epu8(p2, p0),
                                       _mm_subs_epu8(p0, p2)),
                           _mm_or_si128(_mm_subs_epu8(q2, q0),
                                        _mm_subs_epu8(q0, q2)));
      flat = _mm_max_epu8(work, flat);
      work = _mm_max_epu8(_mm_or_si128(_mm_subs_epu8(p3, p0),
                                       _mm_subs_epu8(p0, p3)),
                           _mm_or_si128(_mm_subs_epu8(q3, q0),
                                        _mm_subs_epu8(q0, q3)));
      flat = _mm_max_epu8(work, flat);
      work = _mm_max_epu8(_mm_or_si128(_mm_subs_epu8(p4, p0),
                                       _mm_subs_epu8(p0, p4)),
                           _mm_or_si128(_mm_subs_epu8(q4, q0),
                                        _mm_subs_epu8(q0, q4)));
      flat = _mm_subs_epu8(flat, one);
      flat = _mm_cmpeq_epi8(flat, zero);
      flat = _mm_and_si128(flat, mask);

      p5 = _mm_loadu_si128((__m128i *)(s - 6 * p));
      q5 = _mm_loadu_si128((__m128i *)(s + 5 * p));
      flat2 = _mm_max_epu8(_mm_or_si128(_mm_subs_epu8(p5, p0),
                                       _mm_subs_epu8(p0, p5)),
                           _mm_or_si128(_mm_subs_epu8(q5, q0),
                                        _mm_subs_epu8(q0, q5)));
      _mm_store_si128((__m128i *)ap[5], p5);
      _mm_store_si128((__m128i *)aq[5], q5);
      flat2 = _mm_max_epu8(work, flat2);
      p6 = _mm_loadu_si128((__m128i *)(s - 7 * p));
      q6 = _mm_loadu_si128((__m128i *)(s + 6 * p));
      work = _mm_max_epu8(_mm_or_si128(_mm_subs_epu8(p6, p0),
                                       _mm_subs_epu8(p0, p6)),
                           _mm_or_si128(_mm_subs_epu8(q6, q0),
                                        _mm_subs_epu8(q0, q6)));
      _mm_store_si128((__m128i *)ap[6], p6);
      _mm_store_si128((__m128i *)aq[6], q6);
      flat2 = _mm_max_epu8(work, flat2);

      p7 = _mm_loadu_si128((__m128i *)(s - 8 * p));
      q7 = _mm_loadu_si128((__m128i *)(s + 7 * p));
      work = _mm_max_epu8(_mm_or_si128(_mm_subs_epu8(p7, p0),
                                       _mm_subs_epu8(p0, p7)),
                           _mm_or_si128(_mm_subs_epu8(q7, q0),
                                        _mm_subs_epu8(q0, q7)));
      _mm_store_si128((__m128i *)ap[7], p7);
      _mm_store_si128((__m128i *)aq[7], q7);
      flat2 = _mm_max_epu8(work, flat2);
      flat2 = _mm_subs_epu8(flat2, one);
      flat2 = _mm_cmpeq_epi8(flat2, zero);
      flat2 = _mm_and_si128(flat2, flat);  // flat2 & flat & mask
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // flat and wide flat calculations
    {
      const __m128i eight = _mm_set1_epi16(8);
      const __m128i four = _mm_set1_epi16(4);
      __m128i temp_flat2 = flat2;
      unsigned char *src = s;
      int i = 0;
      do {
        __m128i workp_shft;
        __m128i a, b, c;

        unsigned int off = i * 8;
        p7 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(ap[7] + off)), zero);
        p6 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(ap[6] + off)), zero);
        p5 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(ap[5] + off)), zero);
        p4 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(ap[4] + off)), zero);
        p3 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(ap[3] + off)), zero);
        p2 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(ap[2] + off)), zero);
        p1 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(ap[1] + off)), zero);
        p0 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(ap[0] + off)), zero);
        q0 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(aq[0] + off)), zero);
        q1 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(aq[1] + off)), zero);
        q2 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(aq[2] + off)), zero);
        q3 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(aq[3] + off)), zero);
        q4 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(aq[4] + off)), zero);
        q5 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(aq[5] + off)), zero);
        q6 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(aq[6] + off)), zero);
        q7 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(aq[7] + off)), zero);

        c = _mm_sub_epi16(_mm_slli_epi16(p7, 3), p7);  // p7 * 7
        c = _mm_add_epi16(_mm_slli_epi16(p6, 1), _mm_add_epi16(p4, c));

        b = _mm_add_epi16(_mm_add_epi16(p3, four), _mm_add_epi16(p3, p2));
        a = _mm_add_epi16(p3, _mm_add_epi16(p2, p1));
        a = _mm_add_epi16(_mm_add_epi16(p0, q0), a);

        _mm_storel_epi64((__m128i *)&flat_op[2][i*8],
                         _mm_packus_epi16(_mm_srli_epi16(_mm_add_epi16(a, b), 3)
                                          , b));

        c = _mm_add_epi16(_mm_add_epi16(p5, eight), c);
        workp_shft = _mm_srli_epi16(_mm_add_epi16(a, c), 4);
        _mm_storel_epi64((__m128i *)&flat2_op[6][i*8],
                         _mm_packus_epi16(workp_shft, workp_shft));

        a = _mm_add_epi16(q1, a);
        b = _mm_add_epi16(_mm_sub_epi16(b, _mm_add_epi16(p3, p2)), p1);
        _mm_storel_epi64((__m128i *)&flat_op[1][i*8],
                         _mm_packus_epi16(_mm_srli_epi16(_mm_add_epi16(a, b), 3)
                                          , b));

        c = _mm_add_epi16(_mm_sub_epi16(c, _mm_add_epi16(p7, p6)), p5);
        workp_shft = _mm_srli_epi16(_mm_add_epi16(a, c), 4);
        _mm_storel_epi64((__m128i *)&flat2_op[5][i*8],
                         _mm_packus_epi16(workp_shft, workp_shft));

        a = _mm_add_epi16(q2, a);
        b = _mm_add_epi16(_mm_sub_epi16(b, _mm_add_epi16(p3, p1)), p0);
        _mm_storel_epi64((__m128i *)&flat_op[0][i*8],
                         _mm_packus_epi16(_mm_srli_epi16(_mm_add_epi16(a, b), 3)
                                          , b));

        c = _mm_add_epi16(_mm_sub_epi16(c, _mm_add_epi16(p7, p5)), p4);
        workp_shft = _mm_srli_epi16(_mm_add_epi16(a, c), 4);
        _mm_storel_epi64((__m128i *)&flat2_op[4][i*8],
                         _mm_packus_epi16(workp_shft, workp_shft));

        a = _mm_add_epi16(q3, a);
        b = _mm_add_epi16(_mm_sub_epi16(b, _mm_add_epi16(p3, p0)), q0);
        _mm_storel_epi64((__m128i *)&flat_oq[0][i*8],
                         _mm_packus_epi16(_mm_srli_epi16(_mm_add_epi16(a, b), 3)
                                          , b));

        c = _mm_add_epi16(_mm_sub_epi16(c, _mm_add_epi16(p7, p4)), p3);
        workp_shft = _mm_srli_epi16(_mm_add_epi16(a, c), 4);
        _mm_storel_epi64((__m128i *)&flat2_op[3][i*8],
                         _mm_packus_epi16(workp_shft, workp_shft));

        b = _mm_add_epi16(q3, b);
        b = _mm_add_epi16(_mm_sub_epi16(b, _mm_add_epi16(p2, q0)), q1);
        _mm_storel_epi64((__m128i *)&flat_oq[1][i*8],
                         _mm_packus_epi16(_mm_srli_epi16(_mm_add_epi16(a, b), 3)
                                          , b));

        c = _mm_add_epi16(q4, c);
        c = _mm_add_epi16(_mm_sub_epi16(c, _mm_add_epi16(p7, p3)), p2);
        workp_shft = _mm_srli_epi16(_mm_add_epi16(a, c), 4);
        _mm_storel_epi64((__m128i *)&flat2_op[2][i*8],
                         _mm_packus_epi16(workp_shft, workp_shft));

        b = _mm_add_epi16(q3, b);
        b = _mm_add_epi16(_mm_sub_epi16(b, _mm_add_epi16(p1, q1)), q2);
        _mm_storel_epi64((__m128i *)&flat_oq[2][i*8],
                         _mm_packus_epi16(_mm_srli_epi16(_mm_add_epi16(a, b), 3)
                                          , b));
        a = _mm_add_epi16(q5, a);
        c = _mm_add_epi16(_mm_sub_epi16(c, _mm_add_epi16(p7, p2)), p1);
        workp_shft = _mm_srli_epi16(_mm_add_epi16(a, c), 4);
        _mm_storel_epi64((__m128i *)&flat2_op[1][i*8],
                         _mm_packus_epi16(workp_shft, workp_shft));

        a = _mm_add_epi16(q6, a);
        c = _mm_add_epi16(_mm_sub_epi16(c, _mm_add_epi16(p7, p1)), p0);
        workp_shft = _mm_srli_epi16(_mm_add_epi16(a, c), 4);
        _mm_storel_epi64((__m128i *)&flat2_op[0][i*8],
                         _mm_packus_epi16(workp_shft, workp_shft));

        a = _mm_add_epi16(q7, a);
        c = _mm_add_epi16(_mm_sub_epi16(c, _mm_add_epi16(p7, p0)), q0);
        workp_shft = _mm_srli_epi16(_mm_add_epi16(a, c), 4);
        _mm_storel_epi64((__m128i *)&flat2_oq[0][i*8],
                         _mm_packus_epi16(workp_shft, workp_shft));

        a = _mm_add_epi16(q7, a);
        c = _mm_add_epi16(_mm_sub_epi16(c, _mm_add_epi16(p6, q0)), q1);
        workp_shft = _mm_srli_epi16(_mm_add_epi16(a, c), 4);
        _mm_storel_epi64((__m128i *)&flat2_oq[1][i*8],
                         _mm_packus_epi16(workp_shft, workp_shft));

        a = _mm_add_epi16(q7, a);
        c = _mm_add_epi16(_mm_sub_epi16(c, _mm_add_epi16(p5, q1)), q2);
        workp_shft = _mm_srli_epi16(_mm_add_epi16(a, c), 4);
        _mm_storel_epi64((__m128i *)&flat2_oq[2][i*8],
                         _mm_packus_epi16(workp_shft, workp_shft));

        a = _mm_add_epi16(q7, a);
        c = _mm_add_epi16(_mm_sub_epi16(c, _mm_add_epi16(p4, q2)), q3);
        workp_shft = _mm_srli_epi16(_mm_add_epi16(a, c), 4);
        _mm_storel_epi64((__m128i *)&flat2_oq[3][i*8],
                         _mm_packus_epi16(workp_shft, workp_shft));

        a = _mm_add_epi16(q7, a);
        c = _mm_add_epi16(_mm_sub_epi16(c, _mm_add_epi16(p3, q3)), q4);
        workp_shft = _mm_srli_epi16(_mm_add_epi16(a, c), 4);
        _mm_storel_epi64((__m128i *)&flat2_oq[4][i*8],
                         _mm_packus_epi16(workp_shft, workp_shft));

        a = _mm_add_epi16(q7, a);
        c = _mm_add_epi16(_mm_sub_epi16(c, _mm_add_epi16(p2, q4)), q5);
        workp_shft = _mm_srli_epi16(_mm_add_epi16(a, c), 4);
        _mm_storel_epi64((__m128i *)&flat2_oq[5][i*8],
                         _mm_packus_epi16(workp_shft, workp_shft));

        a = _mm_add_epi16(q7, a);
        c = _mm_add_epi16(_mm_sub_epi16(c, _mm_add_epi16(p1, q5)), q6);
        workp_shft = _mm_srli_epi16(_mm_add_epi16(a, c), 4);
        _mm_storel_epi64((__m128i *)&flat2_oq[6][i*8],
                         _mm_packus_epi16(workp_shft, workp_shft));

        temp_flat2 = _mm_srli_si128(temp_flat2, 8);
        src += 8;
      } while (++i < 2);
    }
    // wide flat
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

    work_a = _mm_load_si128((__m128i *)ap[2]);
    p2 = _mm_load_si128((__m128i *)flat_op[2]);
    work_a = _mm_andnot_si128(flat, work_a);
    p2 = _mm_and_si128(flat, p2);
    p2 = _mm_or_si128(work_a, p2);
    _mm_store_si128((__m128i *)flat_op[2], p2);

    p1 = _mm_load_si128((__m128i *)flat_op[1]);
    work_a = _mm_andnot_si128(flat, ps1);
    p1 = _mm_and_si128(flat, p1);
    p1 = _mm_or_si128(work_a, p1);
    _mm_store_si128((__m128i *)flat_op[1], p1);

    p0 = _mm_load_si128((__m128i *)flat_op[0]);
    work_a = _mm_andnot_si128(flat, ps0);
    p0 = _mm_and_si128(flat, p0);
    p0 = _mm_or_si128(work_a, p0);
    _mm_store_si128((__m128i *)flat_op[0], p0);

    q0 = _mm_load_si128((__m128i *)flat_oq[0]);
    work_a = _mm_andnot_si128(flat, qs0);
    q0 = _mm_and_si128(flat, q0);
    q0 = _mm_or_si128(work_a, q0);
    _mm_store_si128((__m128i *)flat_oq[0], q0);

    q1 = _mm_load_si128((__m128i *)flat_oq[1]);
    work_a = _mm_andnot_si128(flat, qs1);
    q1 = _mm_and_si128(flat, q1);
    q1 = _mm_or_si128(work_a, q1);
    _mm_store_si128((__m128i *)flat_oq[1], q1);

    work_a = _mm_load_si128((__m128i *)aq[2]);
    q2 = _mm_load_si128((__m128i *)flat_oq[2]);
    work_a = _mm_andnot_si128(flat, work_a);
    q2 = _mm_and_si128(flat, q2);
    q2 = _mm_or_si128(work_a, q2);
    _mm_store_si128((__m128i *)flat_oq[2], q2);

    // write out op6 - op3
    {
      unsigned char *dst = (s - 7 * p);
      for (i = 6; i > 2; i--) {
        __m128i flat2_output;
        work_a = _mm_load_si128((__m128i *)ap[i]);
        flat2_output = _mm_load_si128((__m128i *)flat2_op[i]);
        work_a = _mm_andnot_si128(flat2, work_a);
        flat2_output = _mm_and_si128(flat2, flat2_output);
        work_a = _mm_or_si128(work_a, flat2_output);
        _mm_storeu_si128((__m128i *)dst, work_a);
        dst += p;
      }
    }

    work_a = _mm_load_si128((__m128i *)flat_op[2]);
    p2 = _mm_load_si128((__m128i *)flat2_op[2]);
    work_a = _mm_andnot_si128(flat2, work_a);
    p2 = _mm_and_si128(flat2, p2);
    p2 = _mm_or_si128(work_a, p2);
    _mm_storeu_si128((__m128i *)(s - 3 * p), p2);

    work_a = _mm_load_si128((__m128i *)flat_op[1]);
    p1 = _mm_load_si128((__m128i *)flat2_op[1]);
    work_a = _mm_andnot_si128(flat2, work_a);
    p1 = _mm_and_si128(flat2, p1);
    p1 = _mm_or_si128(work_a, p1);
    _mm_storeu_si128((__m128i *)(s - 2 * p), p1);

    work_a = _mm_load_si128((__m128i *)flat_op[0]);
    p0 = _mm_load_si128((__m128i *)flat2_op[0]);
    work_a = _mm_andnot_si128(flat2, work_a);
    p0 = _mm_and_si128(flat2, p0);
    p0 = _mm_or_si128(work_a, p0);
    _mm_storeu_si128((__m128i *)(s - 1 * p), p0);

    work_a = _mm_load_si128((__m128i *)flat_oq[0]);
    q0 = _mm_load_si128((__m128i *)flat2_oq[0]);
    work_a = _mm_andnot_si128(flat2, work_a);
    q0 = _mm_and_si128(flat2, q0);
    q0 = _mm_or_si128(work_a, q0);
    _mm_storeu_si128((__m128i *)(s - 0 * p), q0);

    work_a = _mm_load_si128((__m128i *)flat_oq[1]);
    q1 = _mm_load_si128((__m128i *)flat2_oq[1]);
    work_a = _mm_andnot_si128(flat2, work_a);
    q1 = _mm_and_si128(flat2, q1);
    q1 = _mm_or_si128(work_a, q1);
    _mm_storeu_si128((__m128i *)(s + 1 * p), q1);

    work_a = _mm_load_si128((__m128i *)flat_oq[2]);
    q2 = _mm_load_si128((__m128i *)flat2_oq[2]);
    work_a = _mm_andnot_si128(flat2, work_a);
    q2 = _mm_and_si128(flat2, q2);
    q2 = _mm_or_si128(work_a, q2);
    _mm_storeu_si128((__m128i *)(s + 2 * p), q2);

    // write out oq3 - oq7
    {
      unsigned char *dst = (s + 3 * p);
      for (i = 3; i < 7; i++) {
        __m128i flat2_output;
        work_a = _mm_load_si128((__m128i *)aq[i]);
        flat2_output = _mm_load_si128((__m128i *)flat2_oq[i]);
        work_a = _mm_andnot_si128(flat2, work_a);
        flat2_output = _mm_and_si128(flat2, flat2_output);
        work_a = _mm_or_si128(work_a, flat2_output);
        _mm_storeu_si128((__m128i *)dst, work_a);
        dst += p;
      }
    }
  }
}

void vp9_mb_lpf_horizontal_edge_w_sse2(unsigned char *s,
                                       int p,
                                       const unsigned char *_blimit,
                                       const unsigned char *_limit,
                                       const unsigned char *_thresh,
                                       int count) {
  if (count == 1)
    mb_lpf_horizontal_edge_w_sse2_8(s, p, _blimit, _limit, _thresh);
  else
    mb_lpf_horizontal_edge_w_sse2_16(s, p, _blimit, _limit, _thresh);
}

void vp9_mbloop_filter_horizontal_edge_sse2(unsigned char *s,
                                            int p,
                                            const unsigned char *_blimit,
                                            const unsigned char *_limit,
                                            const unsigned char *_thresh,
                                            int count) {
  DECLARE_ALIGNED(16, unsigned char, flat_op2[16]);
  DECLARE_ALIGNED(16, unsigned char, flat_op1[16]);
  DECLARE_ALIGNED(16, unsigned char, flat_op0[16]);
  DECLARE_ALIGNED(16, unsigned char, flat_oq2[16]);
  DECLARE_ALIGNED(16, unsigned char, flat_oq1[16]);
  DECLARE_ALIGNED(16, unsigned char, flat_oq0[16]);
  __m128i mask, hev, flat;
  const __m128i zero = _mm_set1_epi16(0);
  __m128i p3, p2, p1, p0, q0, q1, q2, q3;
  const unsigned int extended_thresh = _thresh[0] * 0x01010101u;
  const unsigned int extended_limit  = _limit[0]  * 0x01010101u;
  const unsigned int extended_blimit = _blimit[0] * 0x01010101u;
  const __m128i thresh =
      _mm_shuffle_epi32(_mm_cvtsi32_si128((int)extended_thresh), 0);
  const __m128i limit =
      _mm_shuffle_epi32(_mm_cvtsi32_si128((int)extended_limit), 0);
  const __m128i blimit =
      _mm_shuffle_epi32(_mm_cvtsi32_si128((int)extended_blimit), 0);

  (void)count;
  p3 = _mm_loadl_epi64((__m128i *)(s - 4 * p));
  p2 = _mm_loadl_epi64((__m128i *)(s - 3 * p));
  p1 = _mm_loadl_epi64((__m128i *)(s - 2 * p));
  p0 = _mm_loadl_epi64((__m128i *)(s - 1 * p));
  q0 = _mm_loadl_epi64((__m128i *)(s - 0 * p));
  q1 = _mm_loadl_epi64((__m128i *)(s + 1 * p));
  q2 = _mm_loadl_epi64((__m128i *)(s + 2 * p));
  q3 = _mm_loadl_epi64((__m128i *)(s + 3 * p));
  {
    const __m128i abs_p1p0 = _mm_or_si128(_mm_subs_epu8(p1, p0),
                                          _mm_subs_epu8(p0, p1));
    const __m128i abs_q1q0 = _mm_or_si128(_mm_subs_epu8(q1, q0),
                                          _mm_subs_epu8(q0, q1));
    const __m128i one = _mm_set1_epi8(1);
    const __m128i fe = _mm_set1_epi8(0xfe);
    const __m128i ff = _mm_cmpeq_epi8(abs_p1p0, abs_p1p0);
    __m128i abs_p0q0 = _mm_or_si128(_mm_subs_epu8(p0, q0),
                                    _mm_subs_epu8(q0, p0));
    __m128i abs_p1q1 = _mm_or_si128(_mm_subs_epu8(p1, q1),
                                    _mm_subs_epu8(q1, p1));
    __m128i work;
    flat = _mm_max_epu8(abs_p1p0, abs_q1q0);
    hev = _mm_subs_epu8(flat, thresh);
    hev = _mm_xor_si128(_mm_cmpeq_epi8(hev, zero), ff);

    abs_p0q0 =_mm_adds_epu8(abs_p0q0, abs_p0q0);
    abs_p1q1 = _mm_srli_epi16(_mm_and_si128(abs_p1q1, fe), 1);
    mask = _mm_subs_epu8(_mm_adds_epu8(abs_p0q0, abs_p1q1), blimit);
    mask = _mm_xor_si128(_mm_cmpeq_epi8(mask, zero), ff);
    // mask |= (abs(p0 - q0) * 2 + abs(p1 - q1) / 2  > blimit) * -1;
    mask = _mm_max_epu8(flat, mask);
    // mask |= (abs(p1 - p0) > limit) * -1;
    // mask |= (abs(q1 - q0) > limit) * -1;
    work = _mm_max_epu8(_mm_or_si128(_mm_subs_epu8(p2, p1),
                                     _mm_subs_epu8(p1, p2)),
                         _mm_or_si128(_mm_subs_epu8(p3, p2),
                                      _mm_subs_epu8(p2, p3)));
    mask = _mm_max_epu8(work, mask);
    work = _mm_max_epu8(_mm_or_si128(_mm_subs_epu8(q2, q1),
                                     _mm_subs_epu8(q1, q2)),
                         _mm_or_si128(_mm_subs_epu8(q3, q2),
                                      _mm_subs_epu8(q2, q3)));
    mask = _mm_max_epu8(work, mask);
    mask = _mm_subs_epu8(mask, limit);
    mask = _mm_cmpeq_epi8(mask, zero);

    work = _mm_max_epu8(_mm_or_si128(_mm_subs_epu8(p2, p0),
                                     _mm_subs_epu8(p0, p2)),
                         _mm_or_si128(_mm_subs_epu8(q2, q0),
                                      _mm_subs_epu8(q0, q2)));
    flat = _mm_max_epu8(work, flat);
    work = _mm_max_epu8(_mm_or_si128(_mm_subs_epu8(p3, p0),
                                     _mm_subs_epu8(p0, p3)),
                         _mm_or_si128(_mm_subs_epu8(q3, q0),
                                      _mm_subs_epu8(q0, q3)));
    flat = _mm_max_epu8(work, flat);
    flat = _mm_subs_epu8(flat, one);
    flat = _mm_cmpeq_epi8(flat, zero);
    flat = _mm_and_si128(flat, mask);
  }
  {
    const __m128i four = _mm_set1_epi16(4);
    unsigned char *src = s;
    {
      __m128i workp_a, workp_b, workp_shft;
      p3 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(src - 4 * p)), zero);
      p2 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(src - 3 * p)), zero);
      p1 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(src - 2 * p)), zero);
      p0 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(src - 1 * p)), zero);
      q0 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(src - 0 * p)), zero);
      q1 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(src + 1 * p)), zero);
      q2 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(src + 2 * p)), zero);
      q3 = _mm_unpacklo_epi8(_mm_loadl_epi64((__m128i *)(src + 3 * p)), zero);

      workp_a = _mm_add_epi16(_mm_add_epi16(p3, p3), _mm_add_epi16(p2, p1));
      workp_a = _mm_add_epi16(_mm_add_epi16(workp_a, four), p0);
      workp_b = _mm_add_epi16(_mm_add_epi16(q0, p2), p3);
      workp_shft = _mm_srli_epi16(_mm_add_epi16(workp_a, workp_b), 3);
      _mm_storel_epi64((__m128i *)&flat_op2[0],
                       _mm_packus_epi16(workp_shft, workp_shft));

      workp_b = _mm_add_epi16(_mm_add_epi16(q0, q1), p1);
      workp_shft = _mm_srli_epi16(_mm_add_epi16(workp_a, workp_b), 3);
      _mm_storel_epi64((__m128i *)&flat_op1[0],
                       _mm_packus_epi16(workp_shft, workp_shft));

      workp_a = _mm_add_epi16(_mm_sub_epi16(workp_a, p3), q2);
      workp_b = _mm_add_epi16(_mm_sub_epi16(workp_b, p1), p0);
      workp_shft = _mm_srli_epi16(_mm_add_epi16(workp_a, workp_b), 3);
      _mm_storel_epi64((__m128i *)&flat_op0[0],
                       _mm_packus_epi16(workp_shft, workp_shft));

      workp_a = _mm_add_epi16(_mm_sub_epi16(workp_a, p3), q3);
      workp_b = _mm_add_epi16(_mm_sub_epi16(workp_b, p0), q0);
      workp_shft = _mm_srli_epi16(_mm_add_epi16(workp_a, workp_b), 3);
      _mm_storel_epi64((__m128i *)&flat_oq0[0],
                       _mm_packus_epi16(workp_shft, workp_shft));

      workp_a = _mm_add_epi16(_mm_sub_epi16(workp_a, p2), q3);
      workp_b = _mm_add_epi16(_mm_sub_epi16(workp_b, q0), q1);
      workp_shft = _mm_srli_epi16(_mm_add_epi16(workp_a, workp_b), 3);
      _mm_storel_epi64((__m128i *)&flat_oq1[0],
                       _mm_packus_epi16(workp_shft, workp_shft));

      workp_a = _mm_add_epi16(_mm_sub_epi16(workp_a, p1), q3);
      workp_b = _mm_add_epi16(_mm_sub_epi16(workp_b, q1), q2);
      workp_shft = _mm_srli_epi16(_mm_add_epi16(workp_a, workp_b), 3);
      _mm_storel_epi64((__m128i *)&flat_oq2[0],
                       _mm_packus_epi16(workp_shft, workp_shft));
    }
  }
  // lp filter
  {
    const __m128i t4 = _mm_set1_epi8(4);
    const __m128i t3 = _mm_set1_epi8(3);
    const __m128i t80 = _mm_set1_epi8(0x80);
    const __m128i te0 = _mm_set1_epi8(0xe0);
    const __m128i t1f = _mm_set1_epi8(0x1f);
    const __m128i t1 = _mm_set1_epi8(0x1);
    const __m128i t7f = _mm_set1_epi8(0x7f);

    const __m128i ps1 = _mm_xor_si128(_mm_loadl_epi64((__m128i *)(s - 2 * p)),
                                      t80);
    const __m128i ps0 = _mm_xor_si128(_mm_loadl_epi64((__m128i *)(s - 1 * p)),
                                      t80);
    const __m128i qs0 = _mm_xor_si128(_mm_loadl_epi64((__m128i *)(s + 0 * p)),
                                      t80);
    const __m128i qs1 = _mm_xor_si128(_mm_loadl_epi64((__m128i *)(s + 1 * p)),
                                      t80);
    __m128i filt;
    __m128i work_a;
    __m128i filter1, filter2;

    filt = _mm_and_si128(_mm_subs_epi8(ps1, qs1), hev);
    work_a = _mm_subs_epi8(qs0, ps0);
    filt = _mm_adds_epi8(filt, work_a);
    filt = _mm_adds_epi8(filt, work_a);
    filt = _mm_adds_epi8(filt, work_a);
    /* (vp9_filter + 3 * (qs0 - ps0)) & mask */
    filt = _mm_and_si128(filt, mask);

    filter1 = _mm_adds_epi8(filt, t4);
    filter2 = _mm_adds_epi8(filt, t3);

    /* Filter1 >> 3 */
    work_a = _mm_cmpgt_epi8(zero, filter1);
    filter1 = _mm_srli_epi16(filter1, 3);
    work_a = _mm_and_si128(work_a, te0);
    filter1 = _mm_and_si128(filter1, t1f);
    filter1 = _mm_or_si128(filter1, work_a);

    /* Filter2 >> 3 */
    work_a = _mm_cmpgt_epi8(zero, filter2);
    filter2 = _mm_srli_epi16(filter2, 3);
    work_a = _mm_and_si128(work_a, te0);
    filter2 = _mm_and_si128(filter2, t1f);
    filter2 = _mm_or_si128(filter2, work_a);

    /* filt >> 1 */
    filt = _mm_adds_epi8(filter1, t1);
    work_a = _mm_cmpgt_epi8(zero, filt);
    filt = _mm_srli_epi16(filt, 1);
    work_a = _mm_and_si128(work_a, t80);
    filt = _mm_and_si128(filt, t7f);
    filt = _mm_or_si128(filt, work_a);

    filt = _mm_andnot_si128(hev, filt);

    work_a = _mm_xor_si128(_mm_subs_epi8(qs0, filter1), t80);
    q0 = _mm_loadl_epi64((__m128i *)flat_oq0);
    work_a = _mm_andnot_si128(flat, work_a);
    q0 = _mm_and_si128(flat, q0);
    q0 = _mm_or_si128(work_a, q0);

    work_a = _mm_xor_si128(_mm_subs_epi8(qs1, filt), t80);
    q1 = _mm_loadl_epi64((__m128i *)flat_oq1);
    work_a = _mm_andnot_si128(flat, work_a);
    q1 = _mm_and_si128(flat, q1);
    q1 = _mm_or_si128(work_a, q1);

    work_a = _mm_loadu_si128((__m128i *)(s + 2 * p));
    q2 = _mm_loadl_epi64((__m128i *)flat_oq2);
    work_a = _mm_andnot_si128(flat, work_a);
    q2 = _mm_and_si128(flat, q2);
    q2 = _mm_or_si128(work_a, q2);

    work_a = _mm_xor_si128(_mm_adds_epi8(ps0, filter2), t80);
    p0 = _mm_loadl_epi64((__m128i *)flat_op0);
    work_a = _mm_andnot_si128(flat, work_a);
    p0 = _mm_and_si128(flat, p0);
    p0 = _mm_or_si128(work_a, p0);

    work_a = _mm_xor_si128(_mm_adds_epi8(ps1, filt), t80);
    p1 = _mm_loadl_epi64((__m128i *)flat_op1);
    work_a = _mm_andnot_si128(flat, work_a);
    p1 = _mm_and_si128(flat, p1);
    p1 = _mm_or_si128(work_a, p1);

    work_a = _mm_loadu_si128((__m128i *)(s - 3 * p));
    p2 = _mm_loadl_epi64((__m128i *)flat_op2);
    work_a = _mm_andnot_si128(flat, work_a);
    p2 = _mm_and_si128(flat, p2);
    p2 = _mm_or_si128(work_a, p2);

    _mm_storel_epi64((__m128i *)(s - 3 * p), p2);
    _mm_storel_epi64((__m128i *)(s - 2 * p), p1);
    _mm_storel_epi64((__m128i *)(s - 1 * p), p0);
    _mm_storel_epi64((__m128i *)(s + 0 * p), q0);
    _mm_storel_epi64((__m128i *)(s + 1 * p), q1);
    _mm_storel_epi64((__m128i *)(s + 2 * p), q2);
  }
}

static INLINE void transpose8x16(unsigned char *in0, unsigned char *in1,
                                 int in_p, unsigned char *out, int out_p) {
  __m128i x0, x1, x2, x3, x4, x5, x6, x7;
  __m128i x8, x9, x10, x11, x12, x13, x14, x15;

  /* Read in 16 lines */
  x0 = _mm_loadl_epi64((__m128i *)in0);
  x8 = _mm_loadl_epi64((__m128i *)in1);
  x1 = _mm_loadl_epi64((__m128i *)(in0 + in_p));
  x9 = _mm_loadl_epi64((__m128i *)(in1 + in_p));
  x2 = _mm_loadl_epi64((__m128i *)(in0 + 2 * in_p));
  x10 = _mm_loadl_epi64((__m128i *)(in1 + 2 * in_p));
  x3 = _mm_loadl_epi64((__m128i *)(in0 + 3*in_p));
  x11 = _mm_loadl_epi64((__m128i *)(in1 + 3*in_p));
  x4 = _mm_loadl_epi64((__m128i *)(in0 + 4*in_p));
  x12 = _mm_loadl_epi64((__m128i *)(in1 + 4*in_p));
  x5 = _mm_loadl_epi64((__m128i *)(in0 + 5*in_p));
  x13 = _mm_loadl_epi64((__m128i *)(in1 + 5*in_p));
  x6 = _mm_loadl_epi64((__m128i *)(in0 + 6*in_p));
  x14 = _mm_loadl_epi64((__m128i *)(in1 + 6*in_p));
  x7 = _mm_loadl_epi64((__m128i *)(in0 + 7*in_p));
  x15 = _mm_loadl_epi64((__m128i *)(in1 + 7*in_p));

  x0 = _mm_unpacklo_epi8(x0, x1);
  x1 = _mm_unpacklo_epi8(x2, x3);
  x2 = _mm_unpacklo_epi8(x4, x5);
  x3 = _mm_unpacklo_epi8(x6, x7);

  x8 = _mm_unpacklo_epi8(x8, x9);
  x9 = _mm_unpacklo_epi8(x10, x11);
  x10 = _mm_unpacklo_epi8(x12, x13);
  x11 = _mm_unpacklo_epi8(x14, x15);

  x4 = _mm_unpacklo_epi16(x0, x1);
  x5 = _mm_unpacklo_epi16(x2, x3);
  x12 = _mm_unpacklo_epi16(x8, x9);
  x13 = _mm_unpacklo_epi16(x10, x11);

  x6 = _mm_unpacklo_epi32(x4, x5);
  x7 = _mm_unpackhi_epi32(x4, x5);
  x14 = _mm_unpacklo_epi32(x12, x13);
  x15 = _mm_unpackhi_epi32(x12, x13);

  /* Store first 4-line result */
  _mm_storeu_si128((__m128i *)out, _mm_unpacklo_epi64(x6, x14));
  _mm_storeu_si128((__m128i *)(out + out_p), _mm_unpackhi_epi64(x6, x14));
  _mm_storeu_si128((__m128i *)(out + 2 * out_p), _mm_unpacklo_epi64(x7, x15));
  _mm_storeu_si128((__m128i *)(out + 3 * out_p), _mm_unpackhi_epi64(x7, x15));

  x4 = _mm_unpackhi_epi16(x0, x1);
  x5 = _mm_unpackhi_epi16(x2, x3);
  x12 = _mm_unpackhi_epi16(x8, x9);
  x13 = _mm_unpackhi_epi16(x10, x11);

  x6 = _mm_unpacklo_epi32(x4, x5);
  x7 = _mm_unpackhi_epi32(x4, x5);
  x14 = _mm_unpacklo_epi32(x12, x13);
  x15 = _mm_unpackhi_epi32(x12, x13);

  /* Store second 4-line result */
  _mm_storeu_si128((__m128i *)(out + 4 * out_p), _mm_unpacklo_epi64(x6, x14));
  _mm_storeu_si128((__m128i *)(out + 5 * out_p), _mm_unpackhi_epi64(x6, x14));
  _mm_storeu_si128((__m128i *)(out + 6 * out_p), _mm_unpacklo_epi64(x7, x15));
  _mm_storeu_si128((__m128i *)(out + 7 * out_p), _mm_unpackhi_epi64(x7, x15));
}

static INLINE void transpose(unsigned char *src[], int in_p,
                             unsigned char *dst[], int out_p,
                             int num_8x8_to_transpose) {
  int idx8x8 = 0;
  __m128i x0, x1, x2, x3, x4, x5, x6, x7;
  do {
    unsigned char *in = src[idx8x8];
    unsigned char *out = dst[idx8x8];

    x0 = _mm_loadl_epi64((__m128i *)(in + 0*in_p));  // 00 01 02 03 04 05 06 07
    x1 = _mm_loadl_epi64((__m128i *)(in + 1*in_p));  // 10 11 12 13 14 15 16 17
    x2 = _mm_loadl_epi64((__m128i *)(in + 2*in_p));  // 20 21 22 23 24 25 26 27
    x3 = _mm_loadl_epi64((__m128i *)(in + 3*in_p));  // 30 31 32 33 34 35 36 37
    x4 = _mm_loadl_epi64((__m128i *)(in + 4*in_p));  // 40 41 42 43 44 45 46 47
    x5 = _mm_loadl_epi64((__m128i *)(in + 5*in_p));  // 50 51 52 53 54 55 56 57
    x6 = _mm_loadl_epi64((__m128i *)(in + 6*in_p));  // 60 61 62 63 64 65 66 67
    x7 = _mm_loadl_epi64((__m128i *)(in + 7*in_p));  // 70 71 72 73 74 75 76 77
    // 00 10 01 11 02 12 03 13 04 14 05 15 06 16 07 17
    x0 = _mm_unpacklo_epi8(x0, x1);
    // 20 30 21 31 22 32 23 33 24 34 25 35 26 36 27 37
    x1 = _mm_unpacklo_epi8(x2, x3);
    // 40 50 41 51 42 52 43 53 44 54 45 55 46 56 47 57
    x2 = _mm_unpacklo_epi8(x4, x5);
    // 60 70 61 71 62 72 63 73 64 74 65 75 66 76 67 77
    x3 = _mm_unpacklo_epi8(x6, x7);
    // 00 10 20 30 01 11 21 31 02 12 22 32 03 13 23 33
    x4 = _mm_unpacklo_epi16(x0, x1);
    // 40 50 60 70 41 51 61 71 42 52 62 72 43 53 63 73
    x5 = _mm_unpacklo_epi16(x2, x3);
    // 00 10 20 30 40 50 60 70 01 11 21 31 41 51 61 71
    x6 = _mm_unpacklo_epi32(x4, x5);
    // 02 12 22 32 42 52 62 72 03 13 23 33 43 53 63 73
    x7 = _mm_unpackhi_epi32(x4, x5);

    _mm_storel_pd((double *)(out + 0*out_p),
                  _mm_castsi128_pd(x6));  // 00 10 20 30 40 50 60 70
    _mm_storeh_pd((double *)(out + 1*out_p),
                  _mm_castsi128_pd(x6));  // 01 11 21 31 41 51 61 71
    _mm_storel_pd((double *)(out + 2*out_p),
                  _mm_castsi128_pd(x7));  // 02 12 22 32 42 52 62 72
    _mm_storeh_pd((double *)(out + 3*out_p),
                  _mm_castsi128_pd(x7));  // 03 13 23 33 43 53 63 73

    // 04 14 24 34 05 15 25 35 06 16 26 36 07 17 27 37
    x4 = _mm_unpackhi_epi16(x0, x1);
    // 44 54 64 74 45 55 65 75 46 56 66 76 47 57 67 77
    x5 = _mm_unpackhi_epi16(x2, x3);
    // 04 14 24 34 44 54 64 74 05 15 25 35 45 55 65 75
    x6 = _mm_unpacklo_epi32(x4, x5);
    // 06 16 26 36 46 56 66 76 07 17 27 37 47 57 67 77
    x7 = _mm_unpackhi_epi32(x4, x5);

    _mm_storel_pd((double *)(out + 4*out_p),
                  _mm_castsi128_pd(x6));  // 04 14 24 34 44 54 64 74
    _mm_storeh_pd((double *)(out + 5*out_p),
                  _mm_castsi128_pd(x6));  // 05 15 25 35 45 55 65 75
    _mm_storel_pd((double *)(out + 6*out_p),
                  _mm_castsi128_pd(x7));  // 06 16 26 36 46 56 66 76
    _mm_storeh_pd((double *)(out + 7*out_p),
                  _mm_castsi128_pd(x7));  // 07 17 27 37 47 57 67 77
  } while (++idx8x8 < num_8x8_to_transpose);
}

void vp9_mbloop_filter_vertical_edge_sse2(unsigned char *s,
                                          int p,
                                          const unsigned char *blimit,
                                          const unsigned char *limit,
                                          const unsigned char *thresh,
                                          int count) {
  DECLARE_ALIGNED_ARRAY(16, unsigned char, t_dst, 256);
  unsigned char *src[2];
  unsigned char *dst[2];

  (void)count;
  /* Transpose 16x16 */
  transpose8x16(s - 8, s - 8 + p * 8, p, t_dst, 16);
  transpose8x16(s, s + p * 8, p, t_dst + 16 * 8, 16);

  /* Loop filtering */
  vp9_mbloop_filter_horizontal_edge_sse2(t_dst + 8 * 16, 16, blimit, limit,
                                         thresh, 1);
  src[0] = t_dst + 3 * 16;
  src[1] = t_dst + 3 * 16 + 8;

  dst[0] = s - 5;
  dst[1] = s - 5 + p * 8;

  /* Transpose 16x8 */
  transpose(src, 16, dst, p, 2);
}

void vp9_mb_lpf_vertical_edge_w_sse2(unsigned char *s,
                                     int p,
                                     const unsigned char *blimit,
                                     const unsigned char *limit,
                                     const unsigned char *thresh) {
  DECLARE_ALIGNED_ARRAY(16, unsigned char, t_dst, 256);
  unsigned char *src[4];
  unsigned char *dst[4];

  dst[0] = t_dst;
  dst[1] = t_dst + 8 * 16;

  src[0] = s - 8;
  src[1] = s - 8 + 8;

  /* Transpose 16x16 */
  transpose(src, p, dst, 16, 2);

  /* Loop filtering */
  vp9_mb_lpf_horizontal_edge_w_sse2(t_dst + 8 * 16, 16, blimit, limit,
                                    thresh, 1);

  src[0] = t_dst;
  src[1] = t_dst + 8 * 16;

  dst[0] = s - 8;
  dst[1] = s - 8 + 8;

  transpose(src, 16, dst, p, 2);
}
