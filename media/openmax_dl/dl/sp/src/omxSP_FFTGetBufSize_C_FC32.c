/*
 *  Copyright (c) 2013 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "dl/api/armOMX.h"
#include "dl/api/omxtypes.h"
#include "dl/sp/api/armSP.h"
#include "dl/sp/api/omxSP.h"


/**
 * Function:  omxSP_FFTGetBufSize_C_FC32
 *
 * Description:
 * These functions compute the size of the specification structure
 * required for the length 2^order complex FFT and IFFT functions. The function
 * <FFTGetBufSize_C_FC32> is used in conjunction with the 32-bit functions
 * <FFTFwd_CToC_FC32_Sfs> and <FFTInv_CToC_FC32_Sfs>.
 *
 * Input Arguments:
 *
 *   order - base-2 logarithm of the desired block length; valid in the range
 *            [1,12] ([1,15] if BIG_FFT_TABLE is defined.)
 *
 * Output Arguments:
 *
 *   pSize - pointer to the number of bytes required for the specification
 *            structure
 *
 * Return Value:
 *
 *    OMX_Sts_NoErr - no error
 *
 *
 */

OMXResult omxSP_FFTGetBufSize_C_FC32(OMX_INT order, OMX_INT *pSize) {
  if (!pSize || (order < 1) || (order > TWIDDLE_TABLE_ORDER))
    return OMX_Sts_BadArgErr;
  /*
   * The required size is the same as for C_SC32, because the
   * elements are the same size and because ARMsFFTSpec_SC32 is
   * the same size as ARMsFFTSpec_FC32.
   */
  return omxSP_FFTGetBufSize_C_SC32(order, pSize);
}
