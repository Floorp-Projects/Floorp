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
 * This file contains the function WebRtcSpl_ComplexFFT().
 * The description header can be found in signal_processing_library.h
 *
 */

#include "signal_processing_library.h"

#define CFFTSFT 14
#define CFFTRND 1
#define CFFTRND2 16384

#define CIFFTSFT 14
#define CIFFTRND 1

static const int16_t kSinTable1024[] = {
      0,    201,    402,    603,    804,   1005,   1206,   1406,
   1607,   1808,   2009,   2209,   2410,   2610,   2811,   3011,
   3211,   3411,   3611,   3811,   4011,   4210,   4409,   4608,
   4807,   5006,   5205,   5403,   5601,   5799,   5997,   6195,
   6392,   6589,   6786,   6982,   7179,   7375,   7571,   7766,
   7961,   8156,   8351,   8545,   8739,   8932,   9126,   9319,
   9511,   9703,   9895,  10087,  10278,  10469,  10659,  10849,
  11038,  11227,  11416,  11604,  11792,  11980,  12166,  12353,
  12539,  12724,  12909,  13094,  13278,  13462,  13645,  13827,
  14009,  14191,  14372,  14552,  14732,  14911,  15090,  15268,
  15446,  15623,  15799,  15975,  16150,  16325,  16499,  16672,
  16845,  17017,  17189,  17360,  17530,  17699,  17868,  18036,
  18204,  18371,  18537,  18702,  18867,  19031,  19194,  19357,
  19519,  19680,  19840,  20000,  20159,  20317,  20474,  20631,
  20787,  20942,  21096,  21249,  21402,  21554,  21705,  21855,
  22004,  22153,  22301,  22448,  22594,  22739,  22883,  23027,
  23169,  23311,  23452,  23592,  23731,  23869,  24006,  24143,
  24278,  24413,  24546,  24679,  24811,  24942,  25072,  25201,
  25329,  25456,  25582,  25707,  25831,  25954,  26077,  26198,
  26318,  26437,  26556,  26673,  26789,  26905,  27019,  27132,
  27244,  27355,  27466,  27575,  27683,  27790,  27896,  28001,
  28105,  28208,  28309,  28410,  28510,  28608,  28706,  28802,
  28897,  28992,  29085,  29177,  29268,  29358,  29446,  29534,
  29621,  29706,  29790,  29873,  29955,  30036,  30116,  30195,
  30272,  30349,  30424,  30498,  30571,  30643,  30713,  30783,
  30851,  30918,  30984,  31049,
  31113,  31175,  31236,  31297,
  31356,  31413,  31470,  31525,  31580,  31633,  31684,  31735,
  31785,  31833,  31880,  31926,  31970,  32014,  32056,  32097,
  32137,  32176,  32213,  32249,  32284,  32318,  32350,  32382,
  32412,  32441,  32468,  32495,  32520,  32544,  32567,  32588,
  32609,  32628,  32646,  32662,  32678,  32692,  32705,  32717,
  32727,  32736,  32744,  32751,  32757,  32761,  32764,  32766,
  32767,  32766,  32764,  32761,  32757,  32751,  32744,  32736,
  32727,  32717,  32705,  32692,  32678,  32662,  32646,  32628,
  32609,  32588,  32567,  32544,  32520,  32495,  32468,  32441,
  32412,  32382,  32350,  32318,  32284,  32249,  32213,  32176,
  32137,  32097,  32056,  32014,  31970,  31926,  31880,  31833,
  31785,  31735,  31684,  31633,  31580,  31525,  31470,  31413,
  31356,  31297,  31236,  31175,  31113,  31049,  30984,  30918,
  30851,  30783,  30713,  30643,  30571,  30498,  30424,  30349,
  30272,  30195,  30116,  30036,  29955,  29873,  29790,  29706,
  29621,  29534,  29446,  29358,  29268,  29177,  29085,  28992,
  28897,  28802,  28706,  28608,  28510,  28410,  28309,  28208,
  28105,  28001,  27896,  27790,  27683,  27575,  27466,  27355,
  27244,  27132,  27019,  26905,  26789,  26673,  26556,  26437,
  26318,  26198,  26077,  25954,  25831,  25707,  25582,  25456,
  25329,  25201,  25072,  24942,  24811,  24679,  24546,  24413,
  24278,  24143,  24006,  23869,  23731,  23592,  23452,  23311,
  23169,  23027,  22883,  22739,  22594,  22448,  22301,  22153,
  22004,  21855,  21705,  21554,  21402,  21249,  21096,  20942,
  20787,  20631,  20474,  20317,  20159,  20000,  19840,  19680,
  19519,  19357,  19194,  19031,  18867,  18702,  18537,  18371,
  18204,  18036,  17868,  17699,  17530,  17360,  17189,  17017,
  16845,  16672,  16499,  16325,  16150,  15975,  15799,  15623,
  15446,  15268,  15090,  14911,  14732,  14552,  14372,  14191,
  14009,  13827,  13645,  13462,  13278,  13094,  12909,  12724,
  12539,  12353,  12166,  11980,  11792,  11604,  11416,  11227,
  11038,  10849,  10659,  10469,  10278,  10087,   9895,   9703,
   9511,   9319,   9126,   8932,   8739,   8545,   8351,   8156,
   7961,   7766,   7571,   7375,   7179,   6982,   6786,   6589,
   6392,   6195,   5997,   5799,   5601,   5403,   5205,   5006,
   4807,   4608,   4409,   4210,   4011,   3811,   3611,   3411,
   3211,   3011,   2811,   2610,   2410,   2209,   2009,   1808,
   1607,   1406,   1206,   1005,    804,    603,    402,    201,
      0,   -201,   -402,   -603,   -804,  -1005,  -1206,  -1406,
  -1607,  -1808,  -2009,  -2209,  -2410,  -2610,  -2811,  -3011,
  -3211,  -3411,  -3611,  -3811,  -4011,  -4210,  -4409,  -4608,
  -4807,  -5006,  -5205,  -5403,  -5601,  -5799,  -5997,  -6195,
  -6392,  -6589,  -6786,  -6982,  -7179,  -7375,  -7571,  -7766,
  -7961,  -8156,  -8351,  -8545,  -8739,  -8932,  -9126,  -9319,
  -9511,  -9703,  -9895, -10087, -10278, -10469, -10659, -10849,
 -11038, -11227, -11416, -11604, -11792, -11980, -12166, -12353,
 -12539, -12724, -12909, -13094, -13278, -13462, -13645, -13827,
 -14009, -14191, -14372, -14552, -14732, -14911, -15090, -15268,
 -15446, -15623, -15799, -15975, -16150, -16325, -16499, -16672,
 -16845, -17017, -17189, -17360, -17530, -17699, -17868, -18036,
 -18204, -18371, -18537, -18702, -18867, -19031, -19194, -19357,
 -19519, -19680, -19840, -20000, -20159, -20317, -20474, -20631,
 -20787, -20942, -21096, -21249, -21402, -21554, -21705, -21855,
 -22004, -22153, -22301, -22448, -22594, -22739, -22883, -23027,
 -23169, -23311, -23452, -23592, -23731, -23869, -24006, -24143,
 -24278, -24413, -24546, -24679, -24811, -24942, -25072, -25201,
 -25329, -25456, -25582, -25707, -25831, -25954, -26077, -26198,
 -26318, -26437, -26556, -26673, -26789, -26905, -27019, -27132,
 -27244, -27355, -27466, -27575, -27683, -27790, -27896, -28001,
 -28105, -28208, -28309, -28410, -28510, -28608, -28706, -28802,
 -28897, -28992, -29085, -29177, -29268, -29358, -29446, -29534,
 -29621, -29706, -29790, -29873, -29955, -30036, -30116, -30195,
 -30272, -30349, -30424, -30498, -30571, -30643, -30713, -30783,
 -30851, -30918, -30984, -31049, -31113, -31175, -31236, -31297,
 -31356, -31413, -31470, -31525, -31580, -31633, -31684, -31735,
 -31785, -31833, -31880, -31926, -31970, -32014, -32056, -32097,
 -32137, -32176, -32213, -32249, -32284, -32318, -32350, -32382,
 -32412, -32441, -32468, -32495, -32520, -32544, -32567, -32588,
 -32609, -32628, -32646, -32662, -32678, -32692, -32705, -32717,
 -32727, -32736, -32744, -32751, -32757, -32761, -32764, -32766,
 -32767, -32766, -32764, -32761, -32757, -32751, -32744, -32736,
 -32727, -32717, -32705, -32692, -32678, -32662, -32646, -32628,
 -32609, -32588, -32567, -32544, -32520, -32495, -32468, -32441,
 -32412, -32382, -32350, -32318, -32284, -32249, -32213, -32176,
 -32137, -32097, -32056, -32014, -31970, -31926, -31880, -31833,
 -31785, -31735, -31684, -31633, -31580, -31525, -31470, -31413,
 -31356, -31297, -31236, -31175, -31113, -31049, -30984, -30918,
 -30851, -30783, -30713, -30643, -30571, -30498, -30424, -30349,
 -30272, -30195, -30116, -30036, -29955, -29873, -29790, -29706,
 -29621, -29534, -29446, -29358, -29268, -29177, -29085, -28992,
 -28897, -28802, -28706, -28608, -28510, -28410, -28309, -28208,
 -28105, -28001, -27896, -27790, -27683, -27575, -27466, -27355,
 -27244, -27132, -27019, -26905, -26789, -26673, -26556, -26437,
 -26318, -26198, -26077, -25954, -25831, -25707, -25582, -25456,
 -25329, -25201, -25072, -24942, -24811, -24679, -24546, -24413,
 -24278, -24143, -24006, -23869, -23731, -23592, -23452, -23311,
 -23169, -23027, -22883, -22739, -22594, -22448, -22301, -22153,
 -22004, -21855, -21705, -21554, -21402, -21249, -21096, -20942,
 -20787, -20631, -20474, -20317, -20159, -20000, -19840, -19680,
 -19519, -19357, -19194, -19031, -18867, -18702, -18537, -18371,
 -18204, -18036, -17868, -17699, -17530, -17360, -17189, -17017,
 -16845, -16672, -16499, -16325, -16150, -15975, -15799, -15623,
 -15446, -15268, -15090, -14911, -14732, -14552, -14372, -14191,
 -14009, -13827, -13645, -13462, -13278, -13094, -12909, -12724,
 -12539, -12353, -12166, -11980, -11792, -11604, -11416, -11227,
 -11038, -10849, -10659, -10469, -10278, -10087,  -9895,  -9703,
  -9511,  -9319,  -9126,  -8932,  -8739,  -8545,  -8351,  -8156,
  -7961,  -7766,  -7571,  -7375,  -7179,  -6982,  -6786,  -6589,
  -6392,  -6195,  -5997,  -5799,  -5601,  -5403,  -5205,  -5006,
  -4807,  -4608,  -4409,  -4210,  -4011,  -3811,  -3611,  -3411,
  -3211,  -3011,  -2811,  -2610,  -2410,  -2209,  -2009,  -1808,
  -1607,  -1406,  -1206,  -1005,   -804,   -603,   -402,   -201
};

int WebRtcSpl_ComplexFFT(int16_t frfi[], int stages, int mode)
{
    int i, j, l, k, istep, n, m;
    int16_t wr, wi;
    int32_t tr32, ti32, qr32, qi32;

    /* The 1024-value is a constant given from the size of kSinTable1024[],
     * and should not be changed depending on the input parameter 'stages'
     */
    n = 1 << stages;
    if (n > 1024)
        return -1;

    l = 1;
    k = 10 - 1; /* Constant for given kSinTable1024[]. Do not change
         depending on the input parameter 'stages' */

    if (mode == 0)
    {
        // mode==0: Low-complexity and Low-accuracy mode
        while (l < n)
        {
            istep = l << 1;

            for (m = 0; m < l; ++m)
            {
                j = m << k;

                /* The 256-value is a constant given as 1/4 of the size of
                 * kSinTable1024[], and should not be changed depending on the input
                 * parameter 'stages'. It will result in 0 <= j < N_SINE_WAVE/2
                 */
                wr = kSinTable1024[j + 256];
                wi = -kSinTable1024[j];

                for (i = m; i < n; i += istep)
                {
                    j = i + l;

                    tr32 = WEBRTC_SPL_RSHIFT_W32((WEBRTC_SPL_MUL_16_16(wr, frfi[2 * j])
                            - WEBRTC_SPL_MUL_16_16(wi, frfi[2 * j + 1])), 15);

                    ti32 = WEBRTC_SPL_RSHIFT_W32((WEBRTC_SPL_MUL_16_16(wr, frfi[2 * j + 1])
                            + WEBRTC_SPL_MUL_16_16(wi, frfi[2 * j])), 15);

                    qr32 = (int32_t)frfi[2 * i];
                    qi32 = (int32_t)frfi[2 * i + 1];
                    frfi[2 * j] = (int16_t)WEBRTC_SPL_RSHIFT_W32(qr32 - tr32, 1);
                    frfi[2 * j + 1] = (int16_t)WEBRTC_SPL_RSHIFT_W32(qi32 - ti32, 1);
                    frfi[2 * i] = (int16_t)WEBRTC_SPL_RSHIFT_W32(qr32 + tr32, 1);
                    frfi[2 * i + 1] = (int16_t)WEBRTC_SPL_RSHIFT_W32(qi32 + ti32, 1);
                }
            }

            --k;
            l = istep;

        }

    } else
    {
        // mode==1: High-complexity and High-accuracy mode
        while (l < n)
        {
            istep = l << 1;

            for (m = 0; m < l; ++m)
            {
                j = m << k;

                /* The 256-value is a constant given as 1/4 of the size of
                 * kSinTable1024[], and should not be changed depending on the input
                 * parameter 'stages'. It will result in 0 <= j < N_SINE_WAVE/2
                 */
                wr = kSinTable1024[j + 256];
                wi = -kSinTable1024[j];

#ifdef WEBRTC_ARCH_ARM_V7
                int32_t wri = 0;
                int32_t frfi_r = 0;
                __asm __volatile("pkhbt %0, %1, %2, lsl #16" : "=r"(wri) :
                    "r"((int32_t)wr), "r"((int32_t)wi));
#endif

                for (i = m; i < n; i += istep)
                {
                    j = i + l;

#ifdef WEBRTC_ARCH_ARM_V7
                    __asm __volatile(
                      "pkhbt %[frfi_r], %[frfi_even], %[frfi_odd], lsl #16\n\t"
                      "smlsd %[tr32], %[wri], %[frfi_r], %[cfftrnd]\n\t"
                      "smladx %[ti32], %[wri], %[frfi_r], %[cfftrnd]\n\t"
                      :[frfi_r]"+r"(frfi_r),
                       [tr32]"=r"(tr32),
                       [ti32]"=r"(ti32)
                      :[frfi_even]"r"((int32_t)frfi[2*j]),
                       [frfi_odd]"r"((int32_t)frfi[2*j +1]),
                       [wri]"r"(wri),
                       [cfftrnd]"r"(CFFTRND)
                    );
    
#else
                    tr32 = WEBRTC_SPL_MUL_16_16(wr, frfi[2 * j])
                            - WEBRTC_SPL_MUL_16_16(wi, frfi[2 * j + 1]) + CFFTRND;

                    ti32 = WEBRTC_SPL_MUL_16_16(wr, frfi[2 * j + 1])
                            + WEBRTC_SPL_MUL_16_16(wi, frfi[2 * j]) + CFFTRND;
#endif

                    tr32 = WEBRTC_SPL_RSHIFT_W32(tr32, 15 - CFFTSFT);
                    ti32 = WEBRTC_SPL_RSHIFT_W32(ti32, 15 - CFFTSFT);

                    qr32 = ((int32_t)frfi[2 * i]) << CFFTSFT;
                    qi32 = ((int32_t)frfi[2 * i + 1]) << CFFTSFT;

                    frfi[2 * j] = (int16_t)WEBRTC_SPL_RSHIFT_W32(
                            (qr32 - tr32 + CFFTRND2), 1 + CFFTSFT);
                    frfi[2 * j + 1] = (int16_t)WEBRTC_SPL_RSHIFT_W32(
                            (qi32 - ti32 + CFFTRND2), 1 + CFFTSFT);
                    frfi[2 * i] = (int16_t)WEBRTC_SPL_RSHIFT_W32(
                            (qr32 + tr32 + CFFTRND2), 1 + CFFTSFT);
                    frfi[2 * i + 1] = (int16_t)WEBRTC_SPL_RSHIFT_W32(
                            (qi32 + ti32 + CFFTRND2), 1 + CFFTSFT);
                }
            }

            --k;
            l = istep;
        }
    }
    return 0;
}

int WebRtcSpl_ComplexIFFT(int16_t frfi[], int stages, int mode)
{
    int i, j, l, k, istep, n, m, scale, shift;
    int16_t wr, wi;
    int32_t tr32, ti32, qr32, qi32;
    int32_t tmp32, round2;

    /* The 1024-value is a constant given from the size of kSinTable1024[],
     * and should not be changed depending on the input parameter 'stages'
     */
    n = 1 << stages;
    if (n > 1024)
        return -1;

    scale = 0;

    l = 1;
    k = 10 - 1; /* Constant for given kSinTable1024[]. Do not change
         depending on the input parameter 'stages' */

    while (l < n)
    {
        // variable scaling, depending upon data
        shift = 0;
        round2 = 8192;

        tmp32 = (int32_t)WebRtcSpl_MaxAbsValueW16(frfi, 2 * n);
        if (tmp32 > 13573)
        {
            shift++;
            scale++;
            round2 <<= 1;
        }
        if (tmp32 > 27146)
        {
            shift++;
            scale++;
            round2 <<= 1;
        }

        istep = l << 1;

        if (mode == 0)
        {
            // mode==0: Low-complexity and Low-accuracy mode
            for (m = 0; m < l; ++m)
            {
                j = m << k;

                /* The 256-value is a constant given as 1/4 of the size of
                 * kSinTable1024[], and should not be changed depending on the input
                 * parameter 'stages'. It will result in 0 <= j < N_SINE_WAVE/2
                 */
                wr = kSinTable1024[j + 256];
                wi = kSinTable1024[j];

                for (i = m; i < n; i += istep)
                {
                    j = i + l;

                    tr32 = WEBRTC_SPL_RSHIFT_W32((WEBRTC_SPL_MUL_16_16_RSFT(wr, frfi[2 * j], 0)
                            - WEBRTC_SPL_MUL_16_16_RSFT(wi, frfi[2 * j + 1], 0)), 15);

                    ti32 = WEBRTC_SPL_RSHIFT_W32(
                            (WEBRTC_SPL_MUL_16_16_RSFT(wr, frfi[2 * j + 1], 0)
                                    + WEBRTC_SPL_MUL_16_16_RSFT(wi,frfi[2*j],0)), 15);

                    qr32 = (int32_t)frfi[2 * i];
                    qi32 = (int32_t)frfi[2 * i + 1];
                    frfi[2 * j] = (int16_t)WEBRTC_SPL_RSHIFT_W32(qr32 - tr32, shift);
                    frfi[2 * j + 1] = (int16_t)WEBRTC_SPL_RSHIFT_W32(qi32 - ti32, shift);
                    frfi[2 * i] = (int16_t)WEBRTC_SPL_RSHIFT_W32(qr32 + tr32, shift);
                    frfi[2 * i + 1] = (int16_t)WEBRTC_SPL_RSHIFT_W32(qi32 + ti32, shift);
                }
            }
        } else
        {
            // mode==1: High-complexity and High-accuracy mode

            for (m = 0; m < l; ++m)
            {
                j = m << k;

                /* The 256-value is a constant given as 1/4 of the size of
                 * kSinTable1024[], and should not be changed depending on the input
                 * parameter 'stages'. It will result in 0 <= j < N_SINE_WAVE/2
                 */
                wr = kSinTable1024[j + 256];
                wi = kSinTable1024[j];

#ifdef WEBRTC_ARCH_ARM_V7
                int32_t wri = 0;
                int32_t frfi_r = 0;
                __asm __volatile("pkhbt %0, %1, %2, lsl #16" : "=r"(wri) :
                    "r"((int32_t)wr), "r"((int32_t)wi));
#endif

                for (i = m; i < n; i += istep)
                {
                    j = i + l;

#ifdef WEBRTC_ARCH_ARM_V7
                    __asm __volatile(
                      "pkhbt %[frfi_r], %[frfi_even], %[frfi_odd], lsl #16\n\t"
                      "smlsd %[tr32], %[wri], %[frfi_r], %[cifftrnd]\n\t"
                      "smladx %[ti32], %[wri], %[frfi_r], %[cifftrnd]\n\t"
                      :[frfi_r]"+r"(frfi_r),
                       [tr32]"=r"(tr32),
                       [ti32]"=r"(ti32)
                      :[frfi_even]"r"((int32_t)frfi[2*j]),
                       [frfi_odd]"r"((int32_t)frfi[2*j +1]),
                       [wri]"r"(wri),
                       [cifftrnd]"r"(CIFFTRND)
                    );
#else

                    tr32 = WEBRTC_SPL_MUL_16_16(wr, frfi[2 * j])
                            - WEBRTC_SPL_MUL_16_16(wi, frfi[2 * j + 1]) + CIFFTRND;

                    ti32 = WEBRTC_SPL_MUL_16_16(wr, frfi[2 * j + 1])
                            + WEBRTC_SPL_MUL_16_16(wi, frfi[2 * j]) + CIFFTRND;
#endif
                    tr32 = WEBRTC_SPL_RSHIFT_W32(tr32, 15 - CIFFTSFT);
                    ti32 = WEBRTC_SPL_RSHIFT_W32(ti32, 15 - CIFFTSFT);

                    qr32 = ((int32_t)frfi[2 * i]) << CIFFTSFT;
                    qi32 = ((int32_t)frfi[2 * i + 1]) << CIFFTSFT;

                    frfi[2 * j] = (int16_t)WEBRTC_SPL_RSHIFT_W32((qr32 - tr32+round2),
                                                                       shift+CIFFTSFT);
                    frfi[2 * j + 1] = (int16_t)WEBRTC_SPL_RSHIFT_W32(
                            (qi32 - ti32 + round2), shift + CIFFTSFT);
                    frfi[2 * i] = (int16_t)WEBRTC_SPL_RSHIFT_W32((qr32 + tr32 + round2),
                                                                       shift + CIFFTSFT);
                    frfi[2 * i + 1] = (int16_t)WEBRTC_SPL_RSHIFT_W32(
                            (qi32 + ti32 + round2), shift + CIFFTSFT);
                }
            }

        }
        --k;
        l = istep;
    }
    return scale;
}
