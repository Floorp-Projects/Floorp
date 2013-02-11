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

 WebRtcIlbcfix_CbMemEnergy.h

******************************************************************/

#ifndef WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_CB_MEM_ENERGY_H_
#define WEBRTC_MODULES_AUDIO_CODING_CODECS_ILBC_MAIN_SOURCE_CB_MEM_ENERGY_H_

void WebRtcIlbcfix_CbMemEnergy(
    WebRtc_Word16 range,
    WebRtc_Word16 *CB,   /* (i) The CB memory (1:st section) */
    WebRtc_Word16 *filteredCB,  /* (i) The filtered CB memory (2:nd section) */
    WebRtc_Word16 lMem,   /* (i) Length of the CB memory */
    WebRtc_Word16 lTarget,   /* (i) Length of the target vector */
    WebRtc_Word16 *energyW16,  /* (o) Energy in the CB vectors */
    WebRtc_Word16 *energyShifts, /* (o) Shift value of the energy */
    WebRtc_Word16 scale,   /* (i) The scaling of all energy values */
    WebRtc_Word16 base_size  /* (i) Index to where the energy values should be stored */
                               );

#endif
