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
 * Function were the sample rate is set.
 */

#include "mcu.h"

#include "dtmf_buffer.h"
#include "neteq_error_codes.h"

int WebRtcNetEQ_McuSetFs(MCUInst_t *inst, WebRtc_UWord16 fs)
{
    WebRtc_Word16 ok = 0;

    switch (fs)
    {
        case 8000:
        {
#ifdef NETEQ_ATEVENT_DECODE
            ok = WebRtcNetEQ_DtmfDecoderInit(&inst->DTMF_inst, 8000, 560);
#endif
            inst->timestampsPerCall = inst->millisecondsPerCall * 8;
            break;
        }

#ifdef NETEQ_WIDEBAND
        case 16000:
        {
#ifdef NETEQ_ATEVENT_DECODE
            ok = WebRtcNetEQ_DtmfDecoderInit(&inst->DTMF_inst, 16000, 1120);
#endif
            inst->timestampsPerCall = inst->millisecondsPerCall * 16;
            break;
        }
#endif

#ifdef NETEQ_32KHZ_WIDEBAND
        case 32000:
        {
#ifdef NETEQ_ATEVENT_DECODE
            ok = WebRtcNetEQ_DtmfDecoderInit(&inst->DTMF_inst, 32000, 2240);
#endif
            inst->timestampsPerCall = inst->millisecondsPerCall * 32;
            break;
        }
#endif

#ifdef NETEQ_48KHZ_WIDEBAND
        case 48000:
        {
#ifdef NETEQ_ATEVENT_DECODE
            ok = WebRtcNetEQ_DtmfDecoderInit(&inst->DTMF_inst, 48000, 3360);
#endif
            inst->timestampsPerCall = inst->millisecondsPerCall * 48;
            break;
        }
#endif

        default:
        {
            /* Not supported yet */
            return CODEC_DB_UNSUPPORTED_FS;
        }
    } /* end switch */

    inst->fs = fs;

    return ok;
}
