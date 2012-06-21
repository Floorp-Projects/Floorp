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

 WebRtcIlbcfix_SwapBytes.c

******************************************************************/

#include "defines.h"

/*----------------------------------------------------------------*
 * Swap bytes (to simplify operations on Little Endian machines)
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_SwapBytes(
    WebRtc_UWord16 *sequence,   /* (i/o) the sequence to swap */
    WebRtc_Word16 wordLength   /* (i) number or WebRtc_UWord16 to swap */
                              ) {
  int k;
  WebRtc_UWord16 temp=0;
  for( k=wordLength; k>0; k-- ) {
    temp = (*sequence >> 8)|(*sequence << 8);
    *sequence++ = temp;
    //*sequence++ = (*sequence >> 8) | (*sequence << 8);
  }
}
