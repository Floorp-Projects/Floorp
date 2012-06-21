/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "mcu.h"

#include <string.h> /* to define NULL */

/*
 * Initializes MCU with read address and write address
 */
int WebRtcNetEQ_McuAddressInit(MCUInst_t *inst, void * Data2McuAddress,
                               void * Data2DspAddress, void *main_inst)
{

    inst->pw16_readAddress = (WebRtc_Word16*) Data2McuAddress;
    inst->pw16_writeAddress = (WebRtc_Word16*) Data2DspAddress;
    inst->main_inst = main_inst;

    inst->millisecondsPerCall = 10;

    /* Do expansions in the beginning */
    if (inst->pw16_writeAddress != NULL) inst->pw16_writeAddress[0] = DSP_INSTR_EXPAND;

    return (0);
}

