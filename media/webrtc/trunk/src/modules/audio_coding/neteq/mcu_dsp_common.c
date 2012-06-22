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
 * Communication between MCU and DSP sides.
 */

#include "mcu_dsp_common.h"

#include <string.h>

/* Initialize instances with read and write address */
int WebRtcNetEQ_DSPinit(MainInst_t *inst)
{
    int res = 0;

    res |= WebRtcNetEQ_AddressInit(&inst->DSPinst, NULL, NULL, inst);
    res |= WebRtcNetEQ_McuAddressInit(&inst->MCUinst, NULL, NULL, inst);

    return res;

}

/* The DSP side will call this function to interrupt the MCU side */
int WebRtcNetEQ_DSP2MCUinterrupt(MainInst_t *inst, WebRtc_Word16 *pw16_shared_mem)
{
    inst->MCUinst.pw16_readAddress = pw16_shared_mem;
    inst->MCUinst.pw16_writeAddress = pw16_shared_mem;
    return WebRtcNetEQ_SignalMcu(&inst->MCUinst);
}
