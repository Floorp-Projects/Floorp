/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "content_analysis.h"

#include <emmintrin.h>
#include <math.h>

namespace webrtc {

int32_t
VPMContentAnalysis::TemporalDiffMetric_SSE2()
{
    uint32_t numPixels = 0;       // counter for # of pixels

    const uint8_t* imgBufO = _origFrame + _border*_width + _border;
    const uint8_t* imgBufP = _prevFrame + _border*_width + _border;

    const int32_t width_end = ((_width - 2*_border) & -16) + _border;

    __m128i sad_64   = _mm_setzero_si128();
    __m128i sum_64   = _mm_setzero_si128();
    __m128i sqsum_64 = _mm_setzero_si128();
    const __m128i z  = _mm_setzero_si128();

    for(uint16_t i = 0; i < (_height - 2*_border); i += _skipNum)
    {
        __m128i sqsum_32  = _mm_setzero_si128();

        const uint8_t *lineO = imgBufO;
        const uint8_t *lineP = imgBufP;

        // Work on 16 pixels at a time.  For HD content with a width of 1920
        // this loop will run ~67 times (depending on border).  Maximum for
        // abs(o-p) and sum(o) will be 255. _mm_sad_epu8 produces 2 64 bit
        // results which are then accumulated.  There is no chance of
        // rollover for these two accumulators.
        // o*o will have a maximum of 255*255 = 65025.  This will roll over
        // a 16 bit accumulator as 67*65025 > 65535, but will fit in a
        // 32 bit accumulator.
        for(uint16_t j = 0; j < width_end - _border; j += 16)
        {
            const __m128i o = _mm_loadu_si128((__m128i*)(lineO));
            const __m128i p = _mm_loadu_si128((__m128i*)(lineP));

            lineO += 16;
            lineP += 16;

            // abs pixel difference between frames
            sad_64 = _mm_add_epi64 (sad_64, _mm_sad_epu8(o, p));

            // sum of all pixels in frame
            sum_64 = _mm_add_epi64 (sum_64, _mm_sad_epu8(o, z));

            // squared sum of all pixels in frame
            const __m128i olo = _mm_unpacklo_epi8(o,z);
            const __m128i ohi = _mm_unpackhi_epi8(o,z);

            const __m128i sqsum_32_lo = _mm_madd_epi16(olo, olo);
            const __m128i sqsum_32_hi = _mm_madd_epi16(ohi, ohi);

            sqsum_32 = _mm_add_epi32(sqsum_32, sqsum_32_lo);
            sqsum_32 = _mm_add_epi32(sqsum_32, sqsum_32_hi);
        }

        // Add to 64 bit running sum as to not roll over.
        sqsum_64 = _mm_add_epi64(sqsum_64,
                                _mm_add_epi64(_mm_unpackhi_epi32(sqsum_32,z),
                                              _mm_unpacklo_epi32(sqsum_32,z)));

        imgBufO += _width * _skipNum;
        imgBufP += _width * _skipNum;
        numPixels += (width_end - _border);
    }

    __m128i sad_final_128;
    __m128i sum_final_128;
    __m128i sqsum_final_128;

    // bring sums out of vector registers and into integer register
    // domain, summing them along the way
    _mm_store_si128 (&sad_final_128, sad_64);
    _mm_store_si128 (&sum_final_128, sum_64);
    _mm_store_si128 (&sqsum_final_128, sqsum_64);

    uint64_t *sad_final_64 =
                   reinterpret_cast<uint64_t*>(&sad_final_128);
    uint64_t *sum_final_64 =
                   reinterpret_cast<uint64_t*>(&sum_final_128);
    uint64_t *sqsum_final_64 =
                   reinterpret_cast<uint64_t*>(&sqsum_final_128);

    const uint32_t pixelSum = sum_final_64[0] + sum_final_64[1];
    const uint64_t pixelSqSum = sqsum_final_64[0] + sqsum_final_64[1];
    const uint32_t tempDiffSum = sad_final_64[0] + sad_final_64[1];

    // default
    _motionMagnitude = 0.0f;

    if (tempDiffSum == 0)
    {
        return VPM_OK;
    }

    // normalize over all pixels
    const float tempDiffAvg = (float)tempDiffSum / (float)(numPixels);
    const float pixelSumAvg = (float)pixelSum / (float)(numPixels);
    const float pixelSqSumAvg = (float)pixelSqSum / (float)(numPixels);
    float contrast = pixelSqSumAvg - (pixelSumAvg * pixelSumAvg);

    if (contrast > 0.0)
    {
        contrast = sqrt(contrast);
       _motionMagnitude = tempDiffAvg/contrast;
    }

    return VPM_OK;
}

int32_t
VPMContentAnalysis::ComputeSpatialMetrics_SSE2()
{
    const uint8_t* imgBuf = _origFrame + _border*_width;
    const int32_t width_end = ((_width - 2*_border) & -16) + _border;

    __m128i se_32  = _mm_setzero_si128();
    __m128i sev_32 = _mm_setzero_si128();
    __m128i seh_32 = _mm_setzero_si128();
    __m128i msa_32 = _mm_setzero_si128();
    const __m128i z = _mm_setzero_si128();

    // Error is accumulated as a 32 bit value.  Looking at HD content with a
    // height of 1080 lines, or about 67 macro blocks.  If the 16 bit row
    // value is maxed out at 65529 for every row, 65529*1080 = 70777800, which
    // will not roll over a 32 bit accumulator.
    // _skipNum is also used to reduce the number of rows
    for(int32_t i = 0; i < (_height - 2*_border); i += _skipNum)
    {
        __m128i se_16  = _mm_setzero_si128();
        __m128i sev_16 = _mm_setzero_si128();
        __m128i seh_16 = _mm_setzero_si128();
        __m128i msa_16 = _mm_setzero_si128();

        // Row error is accumulated as a 16 bit value.  There are 8
        // accumulators.  Max value of a 16 bit number is 65529.  Looking
        // at HD content, 1080p, has a width of 1920, 120 macro blocks.
        // A mb at a time is processed at a time.  Absolute max error at
        // a point would be abs(0-255+255+255+255) which equals 1020.
        // 120*1020 = 122400.  The probability of hitting this is quite low
        // on well behaved content.  A specially crafted image could roll over.
        // _border could also be adjusted to concentrate on just the center of
        // the images for an HD capture in order to reduce the possiblity of
        // rollover.
        const uint8_t *lineTop = imgBuf - _width + _border;
        const uint8_t *lineCen = imgBuf + _border;
        const uint8_t *lineBot = imgBuf + _width + _border;

        for(int32_t j = 0; j < width_end - _border; j += 16)
        {
            const __m128i t = _mm_loadu_si128((__m128i*)(lineTop));
            const __m128i l = _mm_loadu_si128((__m128i*)(lineCen - 1));
            const __m128i c = _mm_loadu_si128((__m128i*)(lineCen));
            const __m128i r = _mm_loadu_si128((__m128i*)(lineCen + 1));
            const __m128i b = _mm_loadu_si128((__m128i*)(lineBot));

            lineTop += 16;
            lineCen += 16;
            lineBot += 16;

            // center pixel unpacked
            __m128i clo = _mm_unpacklo_epi8(c,z);
            __m128i chi = _mm_unpackhi_epi8(c,z);

            // left right pixels unpacked and added together
            const __m128i lrlo = _mm_add_epi16(_mm_unpacklo_epi8(l,z),
                                               _mm_unpacklo_epi8(r,z));
            const __m128i lrhi = _mm_add_epi16(_mm_unpackhi_epi8(l,z),
                                               _mm_unpackhi_epi8(r,z));

            // top & bottom pixels unpacked and added together
            const __m128i tblo = _mm_add_epi16(_mm_unpacklo_epi8(t,z),
                                               _mm_unpacklo_epi8(b,z));
            const __m128i tbhi = _mm_add_epi16(_mm_unpackhi_epi8(t,z),
                                               _mm_unpackhi_epi8(b,z));

            // running sum of all pixels
            msa_16 = _mm_add_epi16(msa_16, _mm_add_epi16(chi, clo));

            clo = _mm_slli_epi16(clo, 1);
            chi = _mm_slli_epi16(chi, 1);
            const __m128i sevtlo = _mm_subs_epi16(clo, tblo);
            const __m128i sevthi = _mm_subs_epi16(chi, tbhi);
            const __m128i sehtlo = _mm_subs_epi16(clo, lrlo);
            const __m128i sehthi = _mm_subs_epi16(chi, lrhi);

            clo = _mm_slli_epi16(clo, 1);
            chi = _mm_slli_epi16(chi, 1);
            const __m128i setlo = _mm_subs_epi16(clo,
                                                 _mm_add_epi16(lrlo, tblo));
            const __m128i sethi = _mm_subs_epi16(chi,
                                                 _mm_add_epi16(lrhi, tbhi));

            // Add to 16 bit running sum
            se_16  = _mm_add_epi16(se_16,
                                   _mm_max_epi16(setlo,
                                                 _mm_subs_epi16(z, setlo)));
            se_16  = _mm_add_epi16(se_16,
                                   _mm_max_epi16(sethi,
                                                 _mm_subs_epi16(z, sethi)));
            sev_16 = _mm_add_epi16(sev_16,
                                   _mm_max_epi16(sevtlo,
                                                 _mm_subs_epi16(z, sevtlo)));
            sev_16 = _mm_add_epi16(sev_16,
                                   _mm_max_epi16(sevthi,
                                                 _mm_subs_epi16(z, sevthi)));
            seh_16 = _mm_add_epi16(seh_16,
                                   _mm_max_epi16(sehtlo,
                                                 _mm_subs_epi16(z, sehtlo)));
            seh_16 = _mm_add_epi16(seh_16,
                                   _mm_max_epi16(sehthi,
                                                 _mm_subs_epi16(z, sehthi)));
        }

        // Add to 32 bit running sum as to not roll over.
        se_32  = _mm_add_epi32(se_32,
                               _mm_add_epi32(_mm_unpackhi_epi16(se_16,z),
                                             _mm_unpacklo_epi16(se_16,z)));
        sev_32 = _mm_add_epi32(sev_32,
                               _mm_add_epi32(_mm_unpackhi_epi16(sev_16,z),
                                             _mm_unpacklo_epi16(sev_16,z)));
        seh_32 = _mm_add_epi32(seh_32,
                               _mm_add_epi32(_mm_unpackhi_epi16(seh_16,z),
                                             _mm_unpacklo_epi16(seh_16,z)));
        msa_32 = _mm_add_epi32(msa_32,
                               _mm_add_epi32(_mm_unpackhi_epi16(msa_16,z),
                                             _mm_unpacklo_epi16(msa_16,z)));

        imgBuf += _width * _skipNum;
    }

    __m128i se_128;
    __m128i sev_128;
    __m128i seh_128;
    __m128i msa_128;

    // bring sums out of vector registers and into integer register
    // domain, summing them along the way
    _mm_store_si128 (&se_128,
                     _mm_add_epi64(_mm_unpackhi_epi32(se_32,z),
                                   _mm_unpacklo_epi32(se_32,z)));
    _mm_store_si128 (&sev_128,
                     _mm_add_epi64(_mm_unpackhi_epi32(sev_32,z),
                                   _mm_unpacklo_epi32(sev_32,z)));
    _mm_store_si128 (&seh_128,
                     _mm_add_epi64(_mm_unpackhi_epi32(seh_32,z),
                                   _mm_unpacklo_epi32(seh_32,z)));
    _mm_store_si128 (&msa_128,
                     _mm_add_epi64(_mm_unpackhi_epi32(msa_32,z),
                                   _mm_unpacklo_epi32(msa_32,z)));

    uint64_t *se_64 =
                   reinterpret_cast<uint64_t*>(&se_128);
    uint64_t *sev_64 =
                   reinterpret_cast<uint64_t*>(&sev_128);
    uint64_t *seh_64 =
                   reinterpret_cast<uint64_t*>(&seh_128);
    uint64_t *msa_64 =
                   reinterpret_cast<uint64_t*>(&msa_128);

    const uint32_t spatialErrSum  = se_64[0] + se_64[1];
    const uint32_t spatialErrVSum = sev_64[0] + sev_64[1];
    const uint32_t spatialErrHSum = seh_64[0] + seh_64[1];
    const uint32_t pixelMSA = msa_64[0] + msa_64[1];

    // normalize over all pixels
    const float spatialErr  = (float)(spatialErrSum >> 2);
    const float spatialErrH = (float)(spatialErrHSum >> 1);
    const float spatialErrV = (float)(spatialErrVSum >> 1);
    const float norm = (float)pixelMSA;

    // 2X2:
    _spatialPredErr = spatialErr / norm;

    // 1X2:
    _spatialPredErrH = spatialErrH / norm;

    // 2X1:
    _spatialPredErrV = spatialErrV / norm;

    return VPM_OK;
}

}  // namespace webrtc
