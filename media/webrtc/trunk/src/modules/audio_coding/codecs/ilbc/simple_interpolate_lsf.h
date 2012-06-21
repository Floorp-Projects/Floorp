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

 WebRtcIlbcfix_SimpleInterpolateLsf.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_SIMPLE_INTERPOLATE_LSF_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_SIMPLE_INTERPOLATE_LSF_H_

#include "defines.h"

/*----------------------------------------------------------------*
 *  lsf interpolator (subrutine to LPCencode)
 *---------------------------------------------------------------*/

void WebRtcIlbcfix_SimpleInterpolateLsf(
    WebRtc_Word16 *syntdenum, /* (o) the synthesis filter denominator
                                   resulting from the quantized
                                   interpolated lsf Q12 */
    WebRtc_Word16 *weightdenum, /* (o) the weighting filter denominator
                                   resulting from the unquantized
                                   interpolated lsf Q12 */
    WebRtc_Word16 *lsf,  /* (i) the unquantized lsf coefficients Q13 */
    WebRtc_Word16 *lsfdeq,  /* (i) the dequantized lsf coefficients Q13 */
    WebRtc_Word16 *lsfold,  /* (i) the unquantized lsf coefficients of
                                           the previous signal frame Q13 */
    WebRtc_Word16 *lsfdeqold, /* (i) the dequantized lsf coefficients of the
                                   previous signal frame Q13 */
    WebRtc_Word16 length,  /* (i) should equate FILTERORDER */
    iLBC_Enc_Inst_t *iLBCenc_inst
    /* (i/o) the encoder state structure */
                                        );

#endif
