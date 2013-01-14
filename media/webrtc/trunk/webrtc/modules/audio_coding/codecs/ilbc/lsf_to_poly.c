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

 WebRtcIlbcfix_Lsf2Poly.c

******************************************************************/

#include "defines.h"
#include "lsf_to_lsp.h"
#include "get_lsp_poly.h"
#include "constants.h"

void WebRtcIlbcfix_Lsf2Poly(
    WebRtc_Word16 *a,     /* (o) predictor coefficients (order = 10) in Q12 */
    WebRtc_Word16 *lsf    /* (i) line spectral frequencies in Q13 */
                            ) {
  WebRtc_Word32 f[2][6]; /* f[0][] and f[1][] corresponds to
                            F1(z) and F2(z) respectivly */
  WebRtc_Word32 *f1ptr, *f2ptr;
  WebRtc_Word16 *a1ptr, *a2ptr;
  WebRtc_Word32 tmpW32;
  WebRtc_Word16 lsp[10];
  int i;

  /* Convert lsf to lsp */
  WebRtcIlbcfix_Lsf2Lsp(lsf, lsp, LPC_FILTERORDER);

  /* Get F1(z) and F2(z) from the lsp */
  f1ptr=f[0];
  f2ptr=f[1];
  WebRtcIlbcfix_GetLspPoly(&lsp[0],f1ptr);
  WebRtcIlbcfix_GetLspPoly(&lsp[1],f2ptr);

  /* for i = 5 down to 1
     Compute f1[i] += f1[i-1];
     and     f2[i] += f2[i-1];
  */
  f1ptr=&f[0][5];
  f2ptr=&f[1][5];
  for (i=5; i>0; i--)
  {
    (*f1ptr) += (*(f1ptr-1));
    (*f2ptr) -= (*(f2ptr-1));
    f1ptr--;
    f2ptr--;
  }

  /* Get the A(z) coefficients
     a[0] = 1.0
     for i = 1 to 5
     a[i] = (f1[i] + f2[i] + round)>>13;
     for i = 1 to 5
     a[11-i] = (f1[i] - f2[i] + round)>>13;
  */
  a[0]=4096;
  a1ptr=&a[1];
  a2ptr=&a[10];
  f1ptr=&f[0][1];
  f2ptr=&f[1][1];
  for (i=5; i>0; i--)
  {
    tmpW32 = (*f1ptr) + (*f2ptr);
    (*a1ptr) = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32((tmpW32+4096),13);

    tmpW32 = (*f1ptr) - (*f2ptr);
    (*a2ptr) = (WebRtc_Word16)WEBRTC_SPL_RSHIFT_W32((tmpW32+4096),13);

    a1ptr++;
    a2ptr--;
    f1ptr++;
    f2ptr++;
  }

  return;
}
