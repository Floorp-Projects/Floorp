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

 WebRtcIlbcfix_GetCbVec.c

******************************************************************/

#include "defines.h"
#include "constants.h"
#include "create_augmented_vec.h"

/*----------------------------------------------------------------*
 *  Construct codebook vector for given index.
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_GetCbVec(
    WebRtc_Word16 *cbvec,   /* (o) Constructed codebook vector */
    WebRtc_Word16 *mem,   /* (i) Codebook buffer */
    WebRtc_Word16 index,   /* (i) Codebook index */
    WebRtc_Word16 lMem,   /* (i) Length of codebook buffer */
    WebRtc_Word16 cbveclen   /* (i) Codebook vector length */
                            ){
  WebRtc_Word16 k, base_size;
  WebRtc_Word16 lag;
  /* Stack based */
  WebRtc_Word16 tempbuff2[SUBL+5];

  /* Determine size of codebook sections */

  base_size=lMem-cbveclen+1;

  if (cbveclen==SUBL) {
    base_size+=WEBRTC_SPL_RSHIFT_W16(cbveclen,1);
  }

  /* No filter -> First codebook section */

  if (index<lMem-cbveclen+1) {

    /* first non-interpolated vectors */

    k=index+cbveclen;
    /* get vector */
    WEBRTC_SPL_MEMCPY_W16(cbvec, mem+lMem-k, cbveclen);

  } else if (index < base_size) {

    /* Calculate lag */

    k=(WebRtc_Word16)WEBRTC_SPL_MUL_16_16(2, (index-(lMem-cbveclen+1)))+cbveclen;

    lag=WEBRTC_SPL_RSHIFT_W16(k, 1);

    WebRtcIlbcfix_CreateAugmentedVec(lag, mem+lMem, cbvec);

  }

  /* Higher codebbok section based on filtering */

  else {

    WebRtc_Word16 memIndTest;

    /* first non-interpolated vectors */

    if (index-base_size<lMem-cbveclen+1) {

      /* Set up filter memory, stuff zeros outside memory buffer */

      memIndTest = lMem-(index-base_size+cbveclen);

      WebRtcSpl_MemSetW16(mem-CB_HALFFILTERLEN, 0, CB_HALFFILTERLEN);
      WebRtcSpl_MemSetW16(mem+lMem, 0, CB_HALFFILTERLEN);

      /* do filtering to get the codebook vector */

      WebRtcSpl_FilterMAFastQ12(
          &mem[memIndTest+4], cbvec, (WebRtc_Word16*)WebRtcIlbcfix_kCbFiltersRev,
          CB_FILTERLEN, cbveclen);
    }

    /* interpolated vectors */

    else {
      /* Stuff zeros outside memory buffer  */
      memIndTest = lMem-cbveclen-CB_FILTERLEN;
      WebRtcSpl_MemSetW16(mem+lMem, 0, CB_HALFFILTERLEN);

      /* do filtering */
      WebRtcSpl_FilterMAFastQ12(
          &mem[memIndTest+7], tempbuff2, (WebRtc_Word16*)WebRtcIlbcfix_kCbFiltersRev,
          CB_FILTERLEN, (WebRtc_Word16)(cbveclen+5));

      /* Calculate lag index */
      lag = (cbveclen<<1)-20+index-base_size-lMem-1;

      WebRtcIlbcfix_CreateAugmentedVec(lag, tempbuff2+SUBL+5, cbvec);
    }
  }
}
