/*
 *  Copyright (c) 2012 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <arm_neon.h>

#include "vp8/encoder/denoising.h"
#include "vpx_mem/vpx_mem.h"
#include "./vp8_rtcd.h"

/*
 * The filter function was modified to reduce the computational complexity.
 *
 * Step 1:
 *  Instead of applying tap coefficients for each pixel, we calculated the
 *  pixel adjustments vs. pixel diff value ahead of time.
 *     adjustment = filtered_value - current_raw
 *                = (filter_coefficient * diff + 128) >> 8
 *  where
 *     filter_coefficient = (255 << 8) / (256 + ((abs_diff * 330) >> 3));
 *     filter_coefficient += filter_coefficient /
 *                           (3 + motion_magnitude_adjustment);
 *     filter_coefficient is clamped to 0 ~ 255.
 *
 * Step 2:
 *  The adjustment vs. diff curve becomes flat very quick when diff increases.
 *  This allowed us to use only several levels to approximate the curve without
 *  changing the filtering algorithm too much.
 *  The adjustments were further corrected by checking the motion magnitude.
 *  The levels used are:
 *      diff          level       adjustment w/o       adjustment w/
 *                               motion correction    motion correction
 *      [-255, -16]     3              -6                   -7
 *      [-15, -8]       2              -4                   -5
 *      [-7, -4]        1              -3                   -4
 *      [-3, 3]         0              diff                 diff
 *      [4, 7]          1               3                    4
 *      [8, 15]         2               4                    5
 *      [16, 255]       3               6                    7
 */

int vp8_denoiser_filter_neon(YV12_BUFFER_CONFIG *mc_running_avg,
                             YV12_BUFFER_CONFIG *running_avg,
                             MACROBLOCK *signal, unsigned int motion_magnitude,
                             int y_offset, int uv_offset) {
    /* If motion_magnitude is small, making the denoiser more aggressive by
     * increasing the adjustment for each level, level1 adjustment is
     * increased, the deltas stay the same.
     */
    const uint8x16_t v_level1_adjustment = vdupq_n_u8(
        (motion_magnitude <= MOTION_MAGNITUDE_THRESHOLD) ? 4 : 3);
    const uint8x16_t v_delta_level_1_and_2 = vdupq_n_u8(1);
    const uint8x16_t v_delta_level_2_and_3 = vdupq_n_u8(2);
    const uint8x16_t v_level1_threshold = vdupq_n_u8(4);
    const uint8x16_t v_level2_threshold = vdupq_n_u8(8);
    const uint8x16_t v_level3_threshold = vdupq_n_u8(16);

    /* Local variables for array pointers and strides. */
    unsigned char *sig = signal->thismb;
    int            sig_stride = 16;
    unsigned char *mc_running_avg_y = mc_running_avg->y_buffer + y_offset;
    int            mc_running_avg_y_stride = mc_running_avg->y_stride;
    unsigned char *running_avg_y = running_avg->y_buffer + y_offset;
    int            running_avg_y_stride = running_avg->y_stride;

    /* Go over lines. */
    int i;
    int sum_diff = 0;
    for (i = 0; i < 16; ++i) {
        int8x16_t v_sum_diff = vdupq_n_s8(0);
        uint8x16_t v_running_avg_y;

        /* Load inputs. */
        const uint8x16_t v_sig = vld1q_u8(sig);
        const uint8x16_t v_mc_running_avg_y = vld1q_u8(mc_running_avg_y);

        /* Calculate absolute difference and sign masks. */
        const uint8x16_t v_abs_diff      = vabdq_u8(v_sig, v_mc_running_avg_y);
        const uint8x16_t v_diff_pos_mask = vcltq_u8(v_sig, v_mc_running_avg_y);
        const uint8x16_t v_diff_neg_mask = vcgtq_u8(v_sig, v_mc_running_avg_y);

        /* Figure out which level that put us in. */
        const uint8x16_t v_level1_mask = vcleq_u8(v_level1_threshold,
                                                  v_abs_diff);
        const uint8x16_t v_level2_mask = vcleq_u8(v_level2_threshold,
                                                  v_abs_diff);
        const uint8x16_t v_level3_mask = vcleq_u8(v_level3_threshold,
                                                  v_abs_diff);

        /* Calculate absolute adjustments for level 1, 2 and 3. */
        const uint8x16_t v_level2_adjustment = vandq_u8(v_level2_mask,
                                                        v_delta_level_1_and_2);
        const uint8x16_t v_level3_adjustment = vandq_u8(v_level3_mask,
                                                        v_delta_level_2_and_3);
        const uint8x16_t v_level1and2_adjustment = vaddq_u8(v_level1_adjustment,
            v_level2_adjustment);
        const uint8x16_t v_level1and2and3_adjustment = vaddq_u8(
            v_level1and2_adjustment, v_level3_adjustment);

        /* Figure adjustment absolute value by selecting between the absolute
         * difference if in level0 or the value for level 1, 2 and 3.
         */
        const uint8x16_t v_abs_adjustment = vbslq_u8(v_level1_mask,
            v_level1and2and3_adjustment, v_abs_diff);

        /* Calculate positive and negative adjustments. Apply them to the signal
         * and accumulate them. Adjustments are less than eight and the maximum
         * sum of them (7 * 16) can fit in a signed char.
         */
        const uint8x16_t v_pos_adjustment = vandq_u8(v_diff_pos_mask,
                                                     v_abs_adjustment);
        const uint8x16_t v_neg_adjustment = vandq_u8(v_diff_neg_mask,
                                                     v_abs_adjustment);
        v_running_avg_y = vqaddq_u8(v_sig, v_pos_adjustment);
        v_running_avg_y = vqsubq_u8(v_running_avg_y, v_neg_adjustment);
        v_sum_diff = vqaddq_s8(v_sum_diff,
                               vreinterpretq_s8_u8(v_pos_adjustment));
        v_sum_diff = vqsubq_s8(v_sum_diff,
                               vreinterpretq_s8_u8(v_neg_adjustment));

        /* Store results. */
        vst1q_u8(running_avg_y, v_running_avg_y);

        /* Sum all the accumulators to have the sum of all pixel differences
         * for this macroblock.
         */
        {
            int s0 = vgetq_lane_s8(v_sum_diff,  0) +
                     vgetq_lane_s8(v_sum_diff,  1) +
                     vgetq_lane_s8(v_sum_diff,  2) +
                     vgetq_lane_s8(v_sum_diff,  3);
            int s1 = vgetq_lane_s8(v_sum_diff,  4) +
                     vgetq_lane_s8(v_sum_diff,  5) +
                     vgetq_lane_s8(v_sum_diff,  6) +
                     vgetq_lane_s8(v_sum_diff,  7);
            int s2 = vgetq_lane_s8(v_sum_diff,  8) +
                     vgetq_lane_s8(v_sum_diff,  9) +
                     vgetq_lane_s8(v_sum_diff, 10) +
                     vgetq_lane_s8(v_sum_diff, 11);
            int s3 = vgetq_lane_s8(v_sum_diff, 12) +
                     vgetq_lane_s8(v_sum_diff, 13) +
                     vgetq_lane_s8(v_sum_diff, 14) +
                     vgetq_lane_s8(v_sum_diff, 15);
            sum_diff += s0 + s1+ s2 + s3;
        }

        /* Update pointers for next iteration. */
        sig += sig_stride;
        mc_running_avg_y += mc_running_avg_y_stride;
        running_avg_y += running_avg_y_stride;
    }

    /* Too much adjustments => copy block. */
    if (abs(sum_diff) > SUM_DIFF_THRESHOLD)
        return COPY_BLOCK;

    /* Tell above level that block was filtered. */
    vp8_copy_mem16x16(running_avg->y_buffer + y_offset, running_avg_y_stride,
                      signal->thismb, sig_stride);
    return FILTER_BLOCK;
}
