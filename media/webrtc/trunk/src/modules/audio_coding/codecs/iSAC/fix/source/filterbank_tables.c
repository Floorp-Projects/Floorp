/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * filterbank_tables.c
 *
 * This file contains variables that are used in
 * filterbanks.c
 *
 */

#include "filterbank_tables.h"
#include "settings.h"


/* HPstcoeff_in_Q14 = {a1, a2, b1 - b0 * a1, b2 - b0 * a2};
 * In float, they are:
 * {-1.94895953203325f, 0.94984516000000f, -0.05101826139794f, 0.05015484000000f};
 */
const WebRtc_Word16 WebRtcIsacfix_kHpStCoeffInQ30[8] = {
  -31932,  16189,  /* Q30 hi/lo pair */
  15562,  17243,  /* Q30 hi/lo pair */
  -26748, -17186,  /* Q35 hi/lo pair */
  26296, -27476   /* Q35 hi/lo pair */
};

/* HPstcoeff_out_1_Q14 = {a1, a2, b1 - b0 * a1, b2 - b0 * a2};
 * In float, they are:
 * {-1.99701049409000f, 0.99714204490000f, 0.01701049409000f, -0.01704204490000f};
 */
const WebRtc_Word16 WebRtcIsacfix_kHPStCoeffOut1Q30[8] = {
  -32719, -1306,  /* Q30 hi/lo pair */
  16337, 11486,  /* Q30 hi/lo pair */
  8918, 26078,  /* Q35 hi/lo pair */
  -8935,  3956   /* Q35 hi/lo pair */
};

/* HPstcoeff_out_2_Q14 = {a1, a2, b1 - b0 * a1, b2 - b0 * a2};
 * In float, they are:
 * {-1.98645294509837f, 0.98672435560000f, 0.00645294509837f, -0.00662435560000f};
 */
const WebRtc_Word16 WebRtcIsacfix_kHPStCoeffOut2Q30[8] = {
  -32546, -2953,  /* Q30 hi/lo pair */
  16166, 32233,  /* Q30 hi/lo pair */
  3383, 13217,  /* Q35 hi/lo pair */
  -3473, -4597   /* Q35 hi/lo pair */
};

/* The upper channel all-pass filter factors */
const WebRtc_Word16 WebRtcIsacfix_kUpperApFactorsQ15[2] = {
  1137, 12537
};

/* The lower channel all-pass filter factors */
const WebRtc_Word16 WebRtcIsacfix_kLowerApFactorsQ15[2] = {
  5059, 24379
};
