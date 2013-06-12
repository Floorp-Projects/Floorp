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
 * Implementation of the peak detection used for finding correlation peaks.
 */

#include "dsp_helpfunctions.h"

#include "signal_processing_library.h"

/* Table of constants used in parabolic fit function WebRtcNetEQ_PrblFit */
const int16_t WebRtcNetEQ_kPrblCf[17][3] = { { 120, 32, 64 }, { 140, 44, 75 },
                                                    { 150, 50, 80 }, { 160, 57, 85 },
                                                    { 180, 72, 96 }, { 200, 89, 107 },
                                                    { 210, 98, 112 }, { 220, 108, 117 },
                                                    { 240, 128, 128 }, { 260, 150, 139 },
                                                    { 270, 162, 144 }, { 280, 174, 149 },
                                                    { 300, 200, 160 }, { 320, 228, 171 },
                                                    { 330, 242, 176 }, { 340, 257, 181 },
                                                    { 360, 288, 192 } };

int16_t WebRtcNetEQ_PeakDetection(int16_t *pw16_data, int16_t w16_dataLen,
                                  int16_t w16_nmbPeaks, int16_t fs_mult,
                                  int16_t *pw16_winIndex,
                                  int16_t *pw16_winValue)
{
    /* Local variables */
    int i;
    int16_t w16_tmp;
    int16_t w16_tmp2;
    int16_t indMin = 0;
    int16_t indMax = 0;

    /* Peak detection */

    for (i = 0; i <= (w16_nmbPeaks - 1); i++)
    {
        if (w16_nmbPeaks == 1)
        {
            /*
             * Single peak
             * The parabola fit assumes that an extra point is available; worst case it gets
             * a zero on the high end of the signal.
             */
            w16_dataLen++;
        }

        pw16_winIndex[i] = WebRtcSpl_MaxIndexW16(pw16_data, (int16_t) (w16_dataLen - 1));

        if (i != w16_nmbPeaks - 1)
        {
            w16_tmp = pw16_winIndex[i] - 2; /* *fs_mult; */
            indMin = WEBRTC_SPL_MAX(0, w16_tmp);
            w16_tmp = pw16_winIndex[i] + 2; /* *fs_mult; */
            w16_tmp2 = w16_dataLen - 1;
            indMax = WEBRTC_SPL_MIN(w16_tmp2, w16_tmp);
        }

        if ((pw16_winIndex[i] != 0) && (pw16_winIndex[i] != (w16_dataLen - 2)))
        {
            /* Parabola fit*/
            WebRtcNetEQ_PrblFit(&(pw16_data[pw16_winIndex[i] - 1]), &(pw16_winIndex[i]),
                &(pw16_winValue[i]), fs_mult);
        }
        else
        {
            if (pw16_winIndex[i] == (w16_dataLen - 2))
            {
                if (pw16_data[pw16_winIndex[i]] > pw16_data[pw16_winIndex[i] + 1])
                {
                    WebRtcNetEQ_PrblFit(&(pw16_data[pw16_winIndex[i] - 1]),
                        &(pw16_winIndex[i]), &(pw16_winValue[i]), fs_mult);
                }
                else if (pw16_data[pw16_winIndex[i]] <= pw16_data[pw16_winIndex[i] + 1])
                {
                    pw16_winValue[i] = (pw16_data[pw16_winIndex[i]]
                        + pw16_data[pw16_winIndex[i] + 1]) >> 1; /* lin approx */
                    pw16_winIndex[i] = (pw16_winIndex[i] * 2 + 1) * fs_mult;
                }
            }
            else
            {
                pw16_winValue[i] = pw16_data[pw16_winIndex[i]];
                pw16_winIndex[i] = pw16_winIndex[i] * 2 * fs_mult;
            }
        }

        if (i != w16_nmbPeaks - 1)
        {
            WebRtcSpl_MemSetW16(&(pw16_data[indMin]), 0, (indMax - indMin + 1));
            /* for (j=indMin; j<=indMax; j++) pw16_data[j] = 0; */
        }
    }

    return 0;
}

int16_t WebRtcNetEQ_PrblFit(int16_t *pw16_3pts, int16_t *pw16_Ind,
                            int16_t *pw16_outVal, int16_t fs_mult)
{
    /* Variables */
    int32_t Num, Den;
    int32_t temp;
    int16_t flag, stp, strt, lmt;
    uint16_t PFind[13];

    if (fs_mult == 1)
    {
        PFind[0] = 0;
        PFind[1] = 8;
        PFind[2] = 16;
    }
    else if (fs_mult == 2)
    {
        PFind[0] = 0;
        PFind[1] = 4;
        PFind[2] = 8;
        PFind[3] = 12;
        PFind[4] = 16;
    }
    else if (fs_mult == 4)
    {
        PFind[0] = 0;
        PFind[1] = 2;
        PFind[2] = 4;
        PFind[3] = 6;
        PFind[4] = 8;
        PFind[5] = 10;
        PFind[6] = 12;
        PFind[7] = 14;
        PFind[8] = 16;
    }
    else
    {
        PFind[0] = 0;
        PFind[1] = 1;
        PFind[2] = 3;
        PFind[3] = 4;
        PFind[4] = 5;
        PFind[5] = 7;
        PFind[6] = 8;
        PFind[7] = 9;
        PFind[8] = 11;
        PFind[9] = 12;
        PFind[10] = 13;
        PFind[11] = 15;
        PFind[12] = 16;
    }

    /*	Num = -3*pw16_3pts[0] + 4*pw16_3pts[1] - pw16_3pts[2]; */
    /*	Den =    pw16_3pts[0] - 2*pw16_3pts[1] + pw16_3pts[2]; */
    Num = WEBRTC_SPL_MUL_16_16(pw16_3pts[0],-3) + WEBRTC_SPL_MUL_16_16(pw16_3pts[1],4)
        - pw16_3pts[2];

    Den = pw16_3pts[0] + WEBRTC_SPL_MUL_16_16(pw16_3pts[1],-2) + pw16_3pts[2];

    temp = (int32_t) WEBRTC_SPL_MUL(Num, (int32_t)120); /* need 32_16 really */
    flag = 1;
    stp = WebRtcNetEQ_kPrblCf[PFind[fs_mult]][0] - WebRtcNetEQ_kPrblCf[PFind[fs_mult - 1]][0];
    strt = (WebRtcNetEQ_kPrblCf[PFind[fs_mult]][0]
        + WebRtcNetEQ_kPrblCf[PFind[fs_mult - 1]][0]) >> 1;

    if (temp < (int32_t) WEBRTC_SPL_MUL(-Den,(int32_t)strt))
    {
        lmt = strt - stp;
        while (flag)
        {
            if ((flag == fs_mult) || (temp
                > (int32_t) WEBRTC_SPL_MUL(-Den,(int32_t)lmt)))
            {
                *pw16_outVal
                    = (int16_t)
                    (((int32_t) ((int32_t) WEBRTC_SPL_MUL(Den,(int32_t)WebRtcNetEQ_kPrblCf[PFind[fs_mult-flag]][1])
                        + (int32_t) WEBRTC_SPL_MUL(Num,(int32_t)WebRtcNetEQ_kPrblCf[PFind[fs_mult-flag]][2])
                        + WEBRTC_SPL_MUL_16_16(pw16_3pts[0],256))) >> 8);
                *pw16_Ind = (*pw16_Ind) * (fs_mult << 1) - flag;
                flag = 0;
            }
            else
            {
                flag++;
                lmt -= stp;
            }
        }
    }
    else if (temp > (int32_t) WEBRTC_SPL_MUL(-Den,(int32_t)(strt+stp)))
    {
        lmt = strt + (stp << 1);
        while (flag)
        {
            if ((flag == fs_mult) || (temp
                < (int32_t) WEBRTC_SPL_MUL(-Den,(int32_t)lmt)))
            {
                int32_t temp_term_1, temp_term_2, temp_term_3;

                temp_term_1 = WEBRTC_SPL_MUL(Den,
                    (int32_t) WebRtcNetEQ_kPrblCf[PFind[fs_mult+flag]][1]);
                temp_term_2 = WEBRTC_SPL_MUL(Num,
                    (int32_t) WebRtcNetEQ_kPrblCf[PFind[fs_mult+flag]][2]);
                temp_term_3 = WEBRTC_SPL_MUL_16_16(pw16_3pts[0],256);

                *pw16_outVal
                    = (int16_t) ((temp_term_1 + temp_term_2 + temp_term_3) >> 8);

                *pw16_Ind = (*pw16_Ind) * (fs_mult << 1) + flag;
                flag = 0;
            }
            else
            {
                flag++;
                lmt += stp;
            }
        }

    }
    else
    {
        *pw16_outVal = pw16_3pts[1];
        *pw16_Ind = (*pw16_Ind) * 2 * fs_mult;
    }

    return 0;
}

