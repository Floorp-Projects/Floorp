/***********************************************************************
Copyright (c) 2006-2012 IETF Trust and Skype Limited. All rights reserved.

This file is extracted from RFC6716. Please see that RFC for additional
information.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
- Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
- Neither the name of Internet Society, IETF or IETF Trust, nor the
names of specific contributors, may be used to endorse or promote
products derived from this software without specific prior written
permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS”
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
***********************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "main.h"
#include "PLC.h"

/****************/
/* Decode frame */
/****************/
opus_int silk_decode_frame(
    silk_decoder_state          *psDec,                         /* I/O  Pointer to Silk decoder state               */
    ec_dec                      *psRangeDec,                    /* I/O  Compressor data structure                   */
    opus_int16                  pOut[],                         /* O    Pointer to output speech frame              */
    opus_int32                  *pN,                            /* O    Pointer to size of output frame             */
    opus_int                    lostFlag,                       /* I    0: no loss, 1 loss, 2 decode fec            */
    opus_int                    condCoding                      /* I    The type of conditional coding to use       */
)
{
    silk_decoder_control sDecCtrl;
    opus_int         L, mv_len, ret = 0;
    opus_int         pulses[ MAX_FRAME_LENGTH ];

    L = psDec->frame_length;
    sDecCtrl.LTP_scale_Q14 = 0;

    /* Safety checks */
    silk_assert( L > 0 && L <= MAX_FRAME_LENGTH );

    if(   lostFlag == FLAG_DECODE_NORMAL ||
        ( lostFlag == FLAG_DECODE_LBRR && psDec->LBRR_flags[ psDec->nFramesDecoded ] == 1 ) )
    {
        /*********************************************/
        /* Decode quantization indices of side info  */
        /*********************************************/
        silk_decode_indices( psDec, psRangeDec, psDec->nFramesDecoded, lostFlag, condCoding );

        /*********************************************/
        /* Decode quantization indices of excitation */
        /*********************************************/
        silk_decode_pulses( psRangeDec, pulses, psDec->indices.signalType,
                psDec->indices.quantOffsetType, psDec->frame_length );

        /********************************************/
        /* Decode parameters and pulse signal       */
        /********************************************/
        silk_decode_parameters( psDec, &sDecCtrl, condCoding );

        /* Update length. Sampling frequency may have changed */
        L = psDec->frame_length;

        /********************************************************/
        /* Run inverse NSQ                                      */
        /********************************************************/
        silk_decode_core( psDec, &sDecCtrl, pOut, pulses );

        /********************************************************/
        /* Update PLC state                                     */
        /********************************************************/
        silk_PLC( psDec, &sDecCtrl, pOut, 0 );

        psDec->lossCnt = 0;
        psDec->prevSignalType = psDec->indices.signalType;
        silk_assert( psDec->prevSignalType >= 0 && psDec->prevSignalType <= 2 );

        /* A frame has been decoded without errors */
        psDec->first_frame_after_reset = 0;
    } else {
        /* Handle packet loss by extrapolation */
        silk_PLC( psDec, &sDecCtrl, pOut, 1 );
    }

    /*************************/
    /* Update output buffer. */
    /*************************/
    silk_assert( psDec->ltp_mem_length >= psDec->frame_length );
    mv_len = psDec->ltp_mem_length - psDec->frame_length;
    silk_memmove( psDec->outBuf, &psDec->outBuf[ psDec->frame_length ], mv_len * sizeof(opus_int16) );
    silk_memcpy( &psDec->outBuf[ mv_len ], pOut, psDec->frame_length * sizeof( opus_int16 ) );

    /****************************************************************/
    /* Ensure smooth connection of extrapolated and good frames     */
    /****************************************************************/
    silk_PLC_glue_frames( psDec, pOut, L );

    /************************************************/
    /* Comfort noise generation / estimation        */
    /************************************************/
    silk_CNG( psDec, &sDecCtrl, pOut, L );

    /* Update some decoder state variables */
    psDec->lagPrev = sDecCtrl.pitchL[ psDec->nb_subfr - 1 ];

    /* Set output frame length */
    *pN = L;

    return ret;
}
