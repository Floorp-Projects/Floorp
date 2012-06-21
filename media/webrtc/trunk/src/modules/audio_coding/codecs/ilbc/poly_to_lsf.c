/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/******************************************************************

 iLBC Speech Coder ANSI-C Source Code

 WebRtcIlbcfix_Poly2Lsf.c

******************************************************************/

#include "defines.h"
#include "constants.h"
#include "poly_to_lsp.h"
#include "lsp_to_lsf.h"

void WebRtcIlbcfix_Poly2Lsf(
    WebRtc_Word16 *lsf,   /* (o) lsf coefficients (Q13) */
    WebRtc_Word16 *a    /* (i) A coefficients (Q12) */
                            ) {
  WebRtc_Word16 lsp[10];
  WebRtcIlbcfix_Poly2Lsp(a, lsp, (WebRtc_Word16*)WebRtcIlbcfix_kLspMean);
  WebRtcIlbcfix_Lsp2Lsf(lsp, lsf, 10);
}
