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

 WebRtcIlbcfix_CbMemEnergyCalc.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_CB_MEM_ENERGY_CALC_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_CB_MEM_ENERGY_CALC_H_

void WebRtcIlbcfix_CbMemEnergyCalc(
    WebRtc_Word32 energy,   /* (i) input start energy */
    WebRtc_Word16 range,   /* (i) number of iterations */
    WebRtc_Word16 *ppi,   /* (i) input pointer 1 */
    WebRtc_Word16 *ppo,   /* (i) input pointer 2 */
    WebRtc_Word16 *energyW16,  /* (o) Energy in the CB vectors */
    WebRtc_Word16 *energyShifts, /* (o) Shift value of the energy */
    WebRtc_Word16 scale,   /* (i) The scaling of all energy values */
    WebRtc_Word16 base_size  /* (i) Index to where the energy values should be stored */
                                   );

#endif
