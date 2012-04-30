/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "vpx_config.h"
#include "encodemb.h"
#include "encodemv.h"
#include "vp8/common/common.h"
#include "onyx_int.h"
#include "vp8/common/extend.h"
#include "vp8/common/entropymode.h"
#include "vp8/common/quant_common.h"
#include "segmentation.h"
#include "vp8/common/setupintrarecon.h"
#include "encodeintra.h"
#include "vp8/common/reconinter.h"
#include "rdopt.h"
#include "pickinter.h"
#include "vp8/common/findnearmv.h"
#include "vp8/common/reconintra.h"
#include <stdio.h>
#include <limits.h>
#include "vp8/common/subpixel.h"
#include "vp8/common/invtrans.h"
#include "vpx_ports/vpx_timer.h"

#if CONFIG_RUNTIME_CPU_DETECT
#define RTCD(x)     &cpi->common.rtcd.x
#define IF_RTCD(x)  (x)
#else
#define RTCD(x)     NULL
#define IF_RTCD(x)  NULL
#endif
extern void vp8_stuff_mb(VP8_COMP *cpi, MACROBLOCKD *x, TOKENEXTRA **t) ;
extern void vp8_calc_ref_frame_costs(int *ref_frame_cost,
                                     int prob_intra,
                                     int prob_last,
                                     int prob_garf
                                    );
extern void vp8_convert_rfct_to_prob(VP8_COMP *const cpi);
extern void vp8cx_initialize_me_consts(VP8_COMP *cpi, int QIndex);
extern void vp8_auto_select_speed(VP8_COMP *cpi);
extern void vp8cx_init_mbrthread_data(VP8_COMP *cpi,
                                      MACROBLOCK *x,
                                      MB_ROW_COMP *mbr_ei,
                                      int mb_row,
                                      int count);
void vp8_build_block_offsets(MACROBLOCK *x);
void vp8_setup_block_ptrs(MACROBLOCK *x);
int vp8cx_encode_inter_macroblock(VP8_COMP *cpi, MACROBLOCK *x, TOKENEXTRA **t, int recon_yoffset, int recon_uvoffset, int mb_row, int mb_col);
int vp8cx_encode_intra_macro_block(VP8_COMP *cpi, MACROBLOCK *x, TOKENEXTRA **t, int mb_row, int mb_col);
static void adjust_act_zbin( VP8_COMP *cpi, MACROBLOCK *x );

#ifdef MODE_STATS
unsigned int inter_y_modes[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
unsigned int inter_uv_modes[4] = {0, 0, 0, 0};
unsigned int inter_b_modes[15]  = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
unsigned int y_modes[5]   = {0, 0, 0, 0, 0};
unsigned int uv_modes[4]  = {0, 0, 0, 0};
unsigned int b_modes[14]  = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
#endif


/* activity_avg must be positive, or flat regions could get a zero weight
 *  (infinite lambda), which confounds analysis.
 * This also avoids the need for divide by zero checks in
 *  vp8_activity_masking().
 */
#define VP8_ACTIVITY_AVG_MIN (64)

/* This is used as a reference when computing the source variance for the
 *  purposes of activity masking.
 * Eventually this should be replaced by custom no-reference routines,
 *  which will be faster.
 */
static const unsigned char VP8_VAR_OFFS[16]=
{
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128
};


// Original activity measure from Tim T's code.
static unsigned int tt_activity_measure( VP8_COMP *cpi, MACROBLOCK *x )
{
    unsigned int act;
    unsigned int sse;
    /* TODO: This could also be done over smaller areas (8x8), but that would
     *  require extensive changes elsewhere, as lambda is assumed to be fixed
     *  over an entire MB in most of the code.
     * Another option is to compute four 8x8 variances, and pick a single
     *  lambda using a non-linear combination (e.g., the smallest, or second
     *  smallest, etc.).
     */
    act =     VARIANCE_INVOKE(&cpi->common.rtcd.variance, var16x16)(x->src.y_buffer,
                    x->src.y_stride, VP8_VAR_OFFS, 0, &sse);
    act = act<<4;

    /* If the region is flat, lower the activity some more. */
    if (act < 8<<12)
        act = act < 5<<12 ? act : 5<<12;

    return act;
}

// Stub for alternative experimental activity measures.
static unsigned int alt_activity_measure( VP8_COMP *cpi,
                                          MACROBLOCK *x, int use_dc_pred )
{
    return vp8_encode_intra(cpi,x, use_dc_pred);
}


// Measure the activity of the current macroblock
// What we measure here is TBD so abstracted to this function
#define ALT_ACT_MEASURE 1
static unsigned int mb_activity_measure( VP8_COMP *cpi, MACROBLOCK *x,
                                  int mb_row, int mb_col)
{
    unsigned int mb_activity;

    if  ( ALT_ACT_MEASURE )
    {
        int use_dc_pred = (mb_col || mb_row) && (!mb_col || !mb_row);

        // Or use and alternative.
        mb_activity = alt_activity_measure( cpi, x, use_dc_pred );
    }
    else
    {
        // Original activity measure from Tim T's code.
        mb_activity = tt_activity_measure( cpi, x );
    }

    if ( mb_activity < VP8_ACTIVITY_AVG_MIN )
        mb_activity = VP8_ACTIVITY_AVG_MIN;

    return mb_activity;
}

// Calculate an "average" mb activity value for the frame
#define ACT_MEDIAN 0
static void calc_av_activity( VP8_COMP *cpi, int64_t activity_sum )
{
#if ACT_MEDIAN
    // Find median: Simple n^2 algorithm for experimentation
    {
        unsigned int median;
        unsigned int i,j;
        unsigned int * sortlist;
        unsigned int tmp;

        // Create a list to sort to
        CHECK_MEM_ERROR(sortlist,
                        vpx_calloc(sizeof(unsigned int),
                        cpi->common.MBs));

        // Copy map to sort list
        vpx_memcpy( sortlist, cpi->mb_activity_map,
                    sizeof(unsigned int) * cpi->common.MBs );


        // Ripple each value down to its correct position
        for ( i = 1; i < cpi->common.MBs; i ++ )
        {
            for ( j = i; j > 0; j -- )
            {
                if ( sortlist[j] < sortlist[j-1] )
                {
                    // Swap values
                    tmp = sortlist[j-1];
                    sortlist[j-1] = sortlist[j];
                    sortlist[j] = tmp;
                }
                else
                    break;
            }
        }

        // Even number MBs so estimate median as mean of two either side.
        median = ( 1 + sortlist[cpi->common.MBs >> 1] +
                   sortlist[(cpi->common.MBs >> 1) + 1] ) >> 1;

        cpi->activity_avg = median;

        vpx_free(sortlist);
    }
#else
    // Simple mean for now
    cpi->activity_avg = (unsigned int)(activity_sum/cpi->common.MBs);
#endif

    if (cpi->activity_avg < VP8_ACTIVITY_AVG_MIN)
        cpi->activity_avg = VP8_ACTIVITY_AVG_MIN;

    // Experimental code: return fixed value normalized for several clips
    if  ( ALT_ACT_MEASURE )
        cpi->activity_avg = 100000;
}

#define USE_ACT_INDEX   0
#define OUTPUT_NORM_ACT_STATS   0

#if USE_ACT_INDEX
// Calculate and activity index for each mb
static void calc_activity_index( VP8_COMP *cpi, MACROBLOCK *x )
{
    VP8_COMMON *const cm = & cpi->common;
    int mb_row, mb_col;

    int64_t act;
    int64_t a;
    int64_t b;

#if OUTPUT_NORM_ACT_STATS
    FILE *f = fopen("norm_act.stt", "a");
    fprintf(f, "\n%12d\n", cpi->activity_avg );
#endif

    // Reset pointers to start of activity map
    x->mb_activity_ptr = cpi->mb_activity_map;

    // Calculate normalized mb activity number.
    for (mb_row = 0; mb_row < cm->mb_rows; mb_row++)
    {
        // for each macroblock col in image
        for (mb_col = 0; mb_col < cm->mb_cols; mb_col++)
        {
            // Read activity from the map
            act = *(x->mb_activity_ptr);

            // Calculate a normalized activity number
            a = act + 4*cpi->activity_avg;
            b = 4*act + cpi->activity_avg;

            if ( b >= a )
                *(x->activity_ptr) = (int)((b + (a>>1))/a) - 1;
            else
                *(x->activity_ptr) = 1 - (int)((a + (b>>1))/b);

#if OUTPUT_NORM_ACT_STATS
            fprintf(f, " %6d", *(x->mb_activity_ptr));
#endif
            // Increment activity map pointers
            x->mb_activity_ptr++;
        }

#if OUTPUT_NORM_ACT_STATS
        fprintf(f, "\n");
#endif

    }

#if OUTPUT_NORM_ACT_STATS
    fclose(f);
#endif

}
#endif

// Loop through all MBs. Note activity of each, average activity and
// calculate a normalized activity for each
static void build_activity_map( VP8_COMP *cpi )
{
    MACROBLOCK *const x = & cpi->mb;
    MACROBLOCKD *xd = &x->e_mbd;
    VP8_COMMON *const cm = & cpi->common;

#if ALT_ACT_MEASURE
    YV12_BUFFER_CONFIG *new_yv12 = &cm->yv12_fb[cm->new_fb_idx];
    int recon_yoffset;
    int recon_y_stride = new_yv12->y_stride;
#endif

    int mb_row, mb_col;
    unsigned int mb_activity;
    int64_t activity_sum = 0;

    // for each macroblock row in image
    for (mb_row = 0; mb_row < cm->mb_rows; mb_row++)
    {
#if ALT_ACT_MEASURE
        // reset above block coeffs
        xd->up_available = (mb_row != 0);
        recon_yoffset = (mb_row * recon_y_stride * 16);
#endif
        // for each macroblock col in image
        for (mb_col = 0; mb_col < cm->mb_cols; mb_col++)
        {
#if ALT_ACT_MEASURE
            xd->dst.y_buffer = new_yv12->y_buffer + recon_yoffset;
            xd->left_available = (mb_col != 0);
            recon_yoffset += 16;
#endif
            //Copy current mb to a buffer
            RECON_INVOKE(&xd->rtcd->recon, copy16x16)(x->src.y_buffer, x->src.y_stride, x->thismb, 16);

            // measure activity
            mb_activity = mb_activity_measure( cpi, x, mb_row, mb_col );

            // Keep frame sum
            activity_sum += mb_activity;

            // Store MB level activity details.
            *x->mb_activity_ptr = mb_activity;

            // Increment activity map pointer
            x->mb_activity_ptr++;

            // adjust to the next column of source macroblocks
            x->src.y_buffer += 16;
        }


        // adjust to the next row of mbs
        x->src.y_buffer += 16 * x->src.y_stride - 16 * cm->mb_cols;

#if ALT_ACT_MEASURE
        //extend the recon for intra prediction
        vp8_extend_mb_row(new_yv12, xd->dst.y_buffer + 16,
                          xd->dst.u_buffer + 8, xd->dst.v_buffer + 8);
#endif

    }

    // Calculate an "average" MB activity
    calc_av_activity(cpi, activity_sum);

#if USE_ACT_INDEX
    // Calculate an activity index number of each mb
    calc_activity_index( cpi, x );
#endif

}

// Macroblock activity masking
void vp8_activity_masking(VP8_COMP *cpi, MACROBLOCK *x)
{
#if USE_ACT_INDEX
    x->rdmult += *(x->mb_activity_ptr) * (x->rdmult >> 2);
    x->errorperbit = x->rdmult * 100 /(110 * x->rddiv);
    x->errorperbit += (x->errorperbit==0);
#else
    int64_t a;
    int64_t b;
    int64_t act = *(x->mb_activity_ptr);

    // Apply the masking to the RD multiplier.
    a = act + (2*cpi->activity_avg);
    b = (2*act) + cpi->activity_avg;

    x->rdmult = (unsigned int)(((int64_t)x->rdmult*b + (a>>1))/a);
    x->errorperbit = x->rdmult * 100 /(110 * x->rddiv);
    x->errorperbit += (x->errorperbit==0);
#endif

    // Activity based Zbin adjustment
    adjust_act_zbin(cpi, x);
}

static
void encode_mb_row(VP8_COMP *cpi,
                   VP8_COMMON *cm,
                   int mb_row,
                   MACROBLOCK  *x,
                   MACROBLOCKD *xd,
                   TOKENEXTRA **tp,
                   int *segment_counts,
                   int *totalrate)
{
    int recon_yoffset, recon_uvoffset;
    int mb_col;
    int ref_fb_idx = cm->lst_fb_idx;
    int dst_fb_idx = cm->new_fb_idx;
    int recon_y_stride = cm->yv12_fb[ref_fb_idx].y_stride;
    int recon_uv_stride = cm->yv12_fb[ref_fb_idx].uv_stride;
    int map_index = (mb_row * cpi->common.mb_cols);

#if CONFIG_MULTITHREAD
    const int nsync = cpi->mt_sync_range;
    const int rightmost_col = cm->mb_cols - 1;
    volatile const int *last_row_current_mb_col;

    if ((cpi->b_multi_threaded != 0) && (mb_row != 0))
        last_row_current_mb_col = &cpi->mt_current_mb_col[mb_row - 1];
    else
        last_row_current_mb_col = &rightmost_col;
#endif

    // reset above block coeffs
    xd->above_context = cm->above_context;

    xd->up_available = (mb_row != 0);
    recon_yoffset = (mb_row * recon_y_stride * 16);
    recon_uvoffset = (mb_row * recon_uv_stride * 8);

    cpi->tplist[mb_row].start = *tp;
    //printf("Main mb_row = %d\n", mb_row);

    // Distance of Mb to the top & bottom edges, specified in 1/8th pel
    // units as they are always compared to values that are in 1/8th pel units
    xd->mb_to_top_edge = -((mb_row * 16) << 3);
    xd->mb_to_bottom_edge = ((cm->mb_rows - 1 - mb_row) * 16) << 3;

    // Set up limit values for vertical motion vector components
    // to prevent them extending beyond the UMV borders
    x->mv_row_min = -((mb_row * 16) + (VP8BORDERINPIXELS - 16));
    x->mv_row_max = ((cm->mb_rows - 1 - mb_row) * 16)
                        + (VP8BORDERINPIXELS - 16);

    // Set the mb activity pointer to the start of the row.
    x->mb_activity_ptr = &cpi->mb_activity_map[map_index];

    // for each macroblock col in image
    for (mb_col = 0; mb_col < cm->mb_cols; mb_col++)
    {
        // Distance of Mb to the left & right edges, specified in
        // 1/8th pel units as they are always compared to values
        // that are in 1/8th pel units
        xd->mb_to_left_edge = -((mb_col * 16) << 3);
        xd->mb_to_right_edge = ((cm->mb_cols - 1 - mb_col) * 16) << 3;

        // Set up limit values for horizontal motion vector components
        // to prevent them extending beyond the UMV borders
        x->mv_col_min = -((mb_col * 16) + (VP8BORDERINPIXELS - 16));
        x->mv_col_max = ((cm->mb_cols - 1 - mb_col) * 16)
                            + (VP8BORDERINPIXELS - 16);

        xd->dst.y_buffer = cm->yv12_fb[dst_fb_idx].y_buffer + recon_yoffset;
        xd->dst.u_buffer = cm->yv12_fb[dst_fb_idx].u_buffer + recon_uvoffset;
        xd->dst.v_buffer = cm->yv12_fb[dst_fb_idx].v_buffer + recon_uvoffset;
        xd->left_available = (mb_col != 0);

        x->rddiv = cpi->RDDIV;
        x->rdmult = cpi->RDMULT;

        //Copy current mb to a buffer
        RECON_INVOKE(&xd->rtcd->recon, copy16x16)(x->src.y_buffer, x->src.y_stride, x->thismb, 16);

#if CONFIG_MULTITHREAD
        if ((cpi->b_multi_threaded != 0) && (mb_row != 0))
        {
            if ((mb_col & (nsync - 1)) == 0)
            {
                while (mb_col > (*last_row_current_mb_col - nsync)
                        && (*last_row_current_mb_col) != (cm->mb_cols - 1))
                {
                    x86_pause_hint();
                    thread_sleep(0);
                }
            }
        }
#endif

        if(cpi->oxcf.tuning == VP8_TUNE_SSIM)
            vp8_activity_masking(cpi, x);

        // Is segmentation enabled
        // MB level adjustment to quantizer
        if (xd->segmentation_enabled)
        {
            // Code to set segment id in xd->mbmi.segment_id for current MB (with range checking)
            if (cpi->segmentation_map[map_index+mb_col] <= 3)
                xd->mode_info_context->mbmi.segment_id = cpi->segmentation_map[map_index+mb_col];
            else
                xd->mode_info_context->mbmi.segment_id = 0;

            vp8cx_mb_init_quantizer(cpi, x, 1);
        }
        else
            xd->mode_info_context->mbmi.segment_id = 0;         // Set to Segment 0 by default

        x->active_ptr = cpi->active_map + map_index + mb_col;

        if (cm->frame_type == KEY_FRAME)
        {
            *totalrate += vp8cx_encode_intra_macro_block(cpi, x, tp, mb_row, mb_col);
#ifdef MODE_STATS
            y_modes[xd->mbmi.mode] ++;
#endif
        }
        else
        {
            *totalrate += vp8cx_encode_inter_macroblock(cpi, x, tp, recon_yoffset, recon_uvoffset, mb_row, mb_col);

#ifdef MODE_STATS
            inter_y_modes[xd->mbmi.mode] ++;

            if (xd->mbmi.mode == SPLITMV)
            {
                int b;

                for (b = 0; b < xd->mbmi.partition_count; b++)
                {
                    inter_b_modes[x->partition->bmi[b].mode] ++;
                }
            }

#endif

            // Count of last ref frame 0,0 useage
            if ((xd->mode_info_context->mbmi.mode == ZEROMV) && (xd->mode_info_context->mbmi.ref_frame == LAST_FRAME))
                cpi->inter_zz_count ++;

            // Special case code for cyclic refresh
            // If cyclic update enabled then copy xd->mbmi.segment_id; (which may have been updated based on mode
            // during vp8cx_encode_inter_macroblock()) back into the global sgmentation map
            if ((cpi->current_layer == 0) &&
                (cpi->cyclic_refresh_mode_enabled && xd->segmentation_enabled))
            {
                cpi->segmentation_map[map_index+mb_col] = xd->mode_info_context->mbmi.segment_id;

                // If the block has been refreshed mark it as clean (the magnitude of the -ve influences how long it will be before we consider another refresh):
                // Else if it was coded (last frame 0,0) and has not already been refreshed then mark it as a candidate for cleanup next time (marked 0)
                // else mark it as dirty (1).
                if (xd->mode_info_context->mbmi.segment_id)
                    cpi->cyclic_refresh_map[map_index+mb_col] = -1;
                else if ((xd->mode_info_context->mbmi.mode == ZEROMV) && (xd->mode_info_context->mbmi.ref_frame == LAST_FRAME))
                {
                    if (cpi->cyclic_refresh_map[map_index+mb_col] == 1)
                        cpi->cyclic_refresh_map[map_index+mb_col] = 0;
                }
                else
                    cpi->cyclic_refresh_map[map_index+mb_col] = 1;

            }
        }

        cpi->tplist[mb_row].stop = *tp;

        // Increment pointer into gf useage flags structure.
        x->gf_active_ptr++;

        // Increment the activity mask pointers.
        x->mb_activity_ptr++;

        // adjust to the next column of macroblocks
        x->src.y_buffer += 16;
        x->src.u_buffer += 8;
        x->src.v_buffer += 8;

        recon_yoffset += 16;
        recon_uvoffset += 8;

        // Keep track of segment useage
        segment_counts[xd->mode_info_context->mbmi.segment_id] ++;

        // skip to next mb
        xd->mode_info_context++;
        x->partition_info++;

        xd->above_context++;
#if CONFIG_MULTITHREAD
        if (cpi->b_multi_threaded != 0)
        {
            cpi->mt_current_mb_col[mb_row] = mb_col;
        }
#endif
    }

    //extend the recon for intra prediction
    vp8_extend_mb_row(
        &cm->yv12_fb[dst_fb_idx],
        xd->dst.y_buffer + 16,
        xd->dst.u_buffer + 8,
        xd->dst.v_buffer + 8);

    // this is to account for the border
    xd->mode_info_context++;
    x->partition_info++;

#if CONFIG_MULTITHREAD
    if ((cpi->b_multi_threaded != 0) && (mb_row == cm->mb_rows - 1))
    {
        sem_post(&cpi->h_event_end_encoding); /* signal frame encoding end */
    }
#endif
}

void init_encode_frame_mb_context(VP8_COMP *cpi)
{
    MACROBLOCK *const x = & cpi->mb;
    VP8_COMMON *const cm = & cpi->common;
    MACROBLOCKD *const xd = & x->e_mbd;

    // GF active flags data structure
    x->gf_active_ptr = (signed char *)cpi->gf_active_flags;

    // Activity map pointer
    x->mb_activity_ptr = cpi->mb_activity_map;

    x->act_zbin_adj = 0;

    x->partition_info = x->pi;

    xd->mode_info_context = cm->mi;
    xd->mode_info_stride = cm->mode_info_stride;

    xd->frame_type = cm->frame_type;

    xd->frames_since_golden = cm->frames_since_golden;
    xd->frames_till_alt_ref_frame = cm->frames_till_alt_ref_frame;

    // reset intra mode contexts
    if (cm->frame_type == KEY_FRAME)
        vp8_init_mbmode_probs(cm);

    // Copy data over into macro block data sturctures.
    x->src = * cpi->Source;
    xd->pre = cm->yv12_fb[cm->lst_fb_idx];
    xd->dst = cm->yv12_fb[cm->new_fb_idx];

    // set up frame for intra coded blocks
    vp8_setup_intra_recon(&cm->yv12_fb[cm->new_fb_idx]);

    vp8_build_block_offsets(x);

    vp8_setup_block_dptrs(&x->e_mbd);

    vp8_setup_block_ptrs(x);

    xd->mode_info_context->mbmi.mode = DC_PRED;
    xd->mode_info_context->mbmi.uv_mode = DC_PRED;

    xd->left_context = &cm->left_context;

    vp8_zero(cpi->count_mb_ref_frame_usage)
    vp8_zero(cpi->ymode_count)
    vp8_zero(cpi->uv_mode_count)

    x->mvc = cm->fc.mvc;

    vpx_memset(cm->above_context, 0,
               sizeof(ENTROPY_CONTEXT_PLANES) * cm->mb_cols);

    // Special case treatment when GF and ARF are not sensible options for reference
    if (cpi->ref_frame_flags == VP8_LAST_FLAG)
        vp8_calc_ref_frame_costs(xd->ref_frame_cost,
                                 cpi->prob_intra_coded,255,128);
    else if ((cpi->oxcf.number_of_layers > 1) &&
               (cpi->ref_frame_flags == VP8_GOLD_FLAG))
        vp8_calc_ref_frame_costs(xd->ref_frame_cost,
                                 cpi->prob_intra_coded,1,255);
    else if ((cpi->oxcf.number_of_layers > 1) &&
                (cpi->ref_frame_flags == VP8_ALT_FLAG))
        vp8_calc_ref_frame_costs(xd->ref_frame_cost,
                                 cpi->prob_intra_coded,1,1);
    else
        vp8_calc_ref_frame_costs(xd->ref_frame_cost,
                                 cpi->prob_intra_coded,
                                 cpi->prob_last_coded,
                                 cpi->prob_gf_coded);

    xd->fullpixel_mask = 0xffffffff;
    if(cm->full_pixel)
        xd->fullpixel_mask = 0xfffffff8;
}

void vp8_encode_frame(VP8_COMP *cpi)
{
    int mb_row;
    MACROBLOCK *const x = & cpi->mb;
    VP8_COMMON *const cm = & cpi->common;
    MACROBLOCKD *const xd = & x->e_mbd;

    TOKENEXTRA *tp = cpi->tok;
    int segment_counts[MAX_MB_SEGMENTS];
    int totalrate;

    vpx_memset(segment_counts, 0, sizeof(segment_counts));
    totalrate = 0;

    if (cpi->compressor_speed == 2)
    {
        if (cpi->oxcf.cpu_used < 0)
            cpi->Speed = -(cpi->oxcf.cpu_used);
        else
            vp8_auto_select_speed(cpi);
    }

    // Functions setup for all frame types so we can use MC in AltRef
    if (cm->mcomp_filter_type == SIXTAP)
    {
        xd->subpixel_predict        = SUBPIX_INVOKE(
                                        &cpi->common.rtcd.subpix, sixtap4x4);
        xd->subpixel_predict8x4     = SUBPIX_INVOKE(
                                        &cpi->common.rtcd.subpix, sixtap8x4);
        xd->subpixel_predict8x8     = SUBPIX_INVOKE(
                                        &cpi->common.rtcd.subpix, sixtap8x8);
        xd->subpixel_predict16x16   = SUBPIX_INVOKE(
                                        &cpi->common.rtcd.subpix, sixtap16x16);
    }
    else
    {
        xd->subpixel_predict        = SUBPIX_INVOKE(
                                        &cpi->common.rtcd.subpix, bilinear4x4);
        xd->subpixel_predict8x4     = SUBPIX_INVOKE(
                                        &cpi->common.rtcd.subpix, bilinear8x4);
        xd->subpixel_predict8x8     = SUBPIX_INVOKE(
                                        &cpi->common.rtcd.subpix, bilinear8x8);
        xd->subpixel_predict16x16   = SUBPIX_INVOKE(
                                      &cpi->common.rtcd.subpix, bilinear16x16);
    }

    // Reset frame count of inter 0,0 motion vector useage.
    cpi->inter_zz_count = 0;

    vpx_memset(segment_counts, 0, sizeof(segment_counts));

    cpi->prediction_error = 0;
    cpi->intra_error = 0;
    cpi->skip_true_count = 0;
    cpi->skip_false_count = 0;

#if 0
    // Experimental code
    cpi->frame_distortion = 0;
    cpi->last_mb_distortion = 0;
#endif

    xd->mode_info_context = cm->mi;

    vp8_zero(cpi->MVcount);
    vp8_zero(cpi->coef_counts);

    vp8cx_frame_init_quantizer(cpi);

    vp8_initialize_rd_consts(cpi,
                             vp8_dc_quant(cm->base_qindex, cm->y1dc_delta_q));

    vp8cx_initialize_me_consts(cpi, cm->base_qindex);

    if(cpi->oxcf.tuning == VP8_TUNE_SSIM)
    {
        // Initialize encode frame context.
        init_encode_frame_mb_context(cpi);

        // Build a frame level activity map
        build_activity_map(cpi);
    }

    // re-initencode frame context.
    init_encode_frame_mb_context(cpi);

    {
        struct vpx_usec_timer  emr_timer;
        vpx_usec_timer_start(&emr_timer);

#if CONFIG_MULTITHREAD
        if (cpi->b_multi_threaded)
        {
            int i;

            vp8cx_init_mbrthread_data(cpi, x, cpi->mb_row_ei, 1,  cpi->encoding_thread_count);

            for (i = 0; i < cm->mb_rows; i++)
                cpi->mt_current_mb_col[i] = -1;

            for (i = 0; i < cpi->encoding_thread_count; i++)
            {
                sem_post(&cpi->h_event_start_encoding[i]);
            }

            for (mb_row = 0; mb_row < cm->mb_rows; mb_row += (cpi->encoding_thread_count + 1))
            {
                vp8_zero(cm->left_context)

                tp = cpi->tok + mb_row * (cm->mb_cols * 16 * 24);

                encode_mb_row(cpi, cm, mb_row, x, xd, &tp, segment_counts, &totalrate);

                // adjust to the next row of mbs
                x->src.y_buffer += 16 * x->src.y_stride * (cpi->encoding_thread_count + 1) - 16 * cm->mb_cols;
                x->src.u_buffer +=  8 * x->src.uv_stride * (cpi->encoding_thread_count + 1) - 8 * cm->mb_cols;
                x->src.v_buffer +=  8 * x->src.uv_stride * (cpi->encoding_thread_count + 1) - 8 * cm->mb_cols;

                xd->mode_info_context += xd->mode_info_stride * cpi->encoding_thread_count;
                x->partition_info  += xd->mode_info_stride * cpi->encoding_thread_count;
                x->gf_active_ptr   += cm->mb_cols * cpi->encoding_thread_count;

            }

            sem_wait(&cpi->h_event_end_encoding); /* wait for other threads to finish */

            cpi->tok_count = 0;

            for (mb_row = 0; mb_row < cm->mb_rows; mb_row ++)
            {
                cpi->tok_count += cpi->tplist[mb_row].stop - cpi->tplist[mb_row].start;
            }

            if (xd->segmentation_enabled)
            {
                int i, j;

                if (xd->segmentation_enabled)
                {

                    for (i = 0; i < cpi->encoding_thread_count; i++)
                    {
                        for (j = 0; j < 4; j++)
                            segment_counts[j] += cpi->mb_row_ei[i].segment_counts[j];
                    }
                }
            }

            for (i = 0; i < cpi->encoding_thread_count; i++)
            {
                totalrate += cpi->mb_row_ei[i].totalrate;
            }

        }
        else
#endif
        {
            // for each macroblock row in image
            for (mb_row = 0; mb_row < cm->mb_rows; mb_row++)
            {

                vp8_zero(cm->left_context)

                encode_mb_row(cpi, cm, mb_row, x, xd, &tp, segment_counts, &totalrate);

                // adjust to the next row of mbs
                x->src.y_buffer += 16 * x->src.y_stride - 16 * cm->mb_cols;
                x->src.u_buffer += 8 * x->src.uv_stride - 8 * cm->mb_cols;
                x->src.v_buffer += 8 * x->src.uv_stride - 8 * cm->mb_cols;
            }

            cpi->tok_count = tp - cpi->tok;

        }

        vpx_usec_timer_mark(&emr_timer);
        cpi->time_encode_mb_row += vpx_usec_timer_elapsed(&emr_timer);

    }


    // Work out the segment probabilites if segmentation is enabled
    if (xd->segmentation_enabled)
    {
        int tot_count;
        int i;

        // Set to defaults
        vpx_memset(xd->mb_segment_tree_probs, 255 , sizeof(xd->mb_segment_tree_probs));

        tot_count = segment_counts[0] + segment_counts[1] + segment_counts[2] + segment_counts[3];

        if (tot_count)
        {
            xd->mb_segment_tree_probs[0] = ((segment_counts[0] + segment_counts[1]) * 255) / tot_count;

            tot_count = segment_counts[0] + segment_counts[1];

            if (tot_count > 0)
            {
                xd->mb_segment_tree_probs[1] = (segment_counts[0] * 255) / tot_count;
            }

            tot_count = segment_counts[2] + segment_counts[3];

            if (tot_count > 0)
                xd->mb_segment_tree_probs[2] = (segment_counts[2] * 255) / tot_count;

            // Zero probabilities not allowed
            for (i = 0; i < MB_FEATURE_TREE_PROBS; i ++)
            {
                if (xd->mb_segment_tree_probs[i] == 0)
                    xd->mb_segment_tree_probs[i] = 1;
            }
        }
    }

    // 256 rate units to the bit
    cpi->projected_frame_size = totalrate >> 8;   // projected_frame_size in units of BYTES

    // Make a note of the percentage MBs coded Intra.
    if (cm->frame_type == KEY_FRAME)
    {
        cpi->this_frame_percent_intra = 100;
    }
    else
    {
        int tot_modes;

        tot_modes = cpi->count_mb_ref_frame_usage[INTRA_FRAME]
                    + cpi->count_mb_ref_frame_usage[LAST_FRAME]
                    + cpi->count_mb_ref_frame_usage[GOLDEN_FRAME]
                    + cpi->count_mb_ref_frame_usage[ALTREF_FRAME];

        if (tot_modes)
            cpi->this_frame_percent_intra = cpi->count_mb_ref_frame_usage[INTRA_FRAME] * 100 / tot_modes;

    }

#if 0
    {
        int cnt = 0;
        int flag[2] = {0, 0};

        for (cnt = 0; cnt < MVPcount; cnt++)
        {
            if (cm->fc.pre_mvc[0][cnt] != cm->fc.mvc[0][cnt])
            {
                flag[0] = 1;
                vpx_memcpy(cm->fc.pre_mvc[0], cm->fc.mvc[0], MVPcount);
                break;
            }
        }

        for (cnt = 0; cnt < MVPcount; cnt++)
        {
            if (cm->fc.pre_mvc[1][cnt] != cm->fc.mvc[1][cnt])
            {
                flag[1] = 1;
                vpx_memcpy(cm->fc.pre_mvc[1], cm->fc.mvc[1], MVPcount);
                break;
            }
        }

        if (flag[0] || flag[1])
            vp8_build_component_cost_table(cpi->mb.mvcost, (const MV_CONTEXT *) cm->fc.mvc, flag);
    }
#endif

    // Adjust the projected reference frame useage probability numbers to reflect
    // what we have just seen. This may be usefull when we make multiple itterations
    // of the recode loop rather than continuing to use values from the previous frame.
    if ((cm->frame_type != KEY_FRAME) && ((cpi->oxcf.number_of_layers > 1) ||
        (!cm->refresh_alt_ref_frame && !cm->refresh_golden_frame)))
    {
      vp8_convert_rfct_to_prob(cpi);
    }

#if 0
    // Keep record of the total distortion this time around for future use
    cpi->last_frame_distortion = cpi->frame_distortion;
#endif

}
void vp8_setup_block_ptrs(MACROBLOCK *x)
{
    int r, c;
    int i;

    for (r = 0; r < 4; r++)
    {
        for (c = 0; c < 4; c++)
        {
            x->block[r*4+c].src_diff = x->src_diff + r * 4 * 16 + c * 4;
        }
    }

    for (r = 0; r < 2; r++)
    {
        for (c = 0; c < 2; c++)
        {
            x->block[16 + r*2+c].src_diff = x->src_diff + 256 + r * 4 * 8 + c * 4;
        }
    }


    for (r = 0; r < 2; r++)
    {
        for (c = 0; c < 2; c++)
        {
            x->block[20 + r*2+c].src_diff = x->src_diff + 320 + r * 4 * 8 + c * 4;
        }
    }

    x->block[24].src_diff = x->src_diff + 384;


    for (i = 0; i < 25; i++)
    {
        x->block[i].coeff = x->coeff + i * 16;
    }
}

void vp8_build_block_offsets(MACROBLOCK *x)
{
    int block = 0;
    int br, bc;

    vp8_build_block_doffsets(&x->e_mbd);

    // y blocks
    x->thismb_ptr = &x->thismb[0];
    for (br = 0; br < 4; br++)
    {
        for (bc = 0; bc < 4; bc++)
        {
            BLOCK *this_block = &x->block[block];
            //this_block->base_src = &x->src.y_buffer;
            //this_block->src_stride = x->src.y_stride;
            //this_block->src = 4 * br * this_block->src_stride + 4 * bc;
            this_block->base_src = &x->thismb_ptr;
            this_block->src_stride = 16;
            this_block->src = 4 * br * 16 + 4 * bc;
            ++block;
        }
    }

    // u blocks
    for (br = 0; br < 2; br++)
    {
        for (bc = 0; bc < 2; bc++)
        {
            BLOCK *this_block = &x->block[block];
            this_block->base_src = &x->src.u_buffer;
            this_block->src_stride = x->src.uv_stride;
            this_block->src = 4 * br * this_block->src_stride + 4 * bc;
            ++block;
        }
    }

    // v blocks
    for (br = 0; br < 2; br++)
    {
        for (bc = 0; bc < 2; bc++)
        {
            BLOCK *this_block = &x->block[block];
            this_block->base_src = &x->src.v_buffer;
            this_block->src_stride = x->src.uv_stride;
            this_block->src = 4 * br * this_block->src_stride + 4 * bc;
            ++block;
        }
    }
}

static void sum_intra_stats(VP8_COMP *cpi, MACROBLOCK *x)
{
    const MACROBLOCKD *xd = & x->e_mbd;
    const MB_PREDICTION_MODE m = xd->mode_info_context->mbmi.mode;
    const MB_PREDICTION_MODE uvm = xd->mode_info_context->mbmi.uv_mode;

#ifdef MODE_STATS
    const int is_key = cpi->common.frame_type == KEY_FRAME;

    ++ (is_key ? uv_modes : inter_uv_modes)[uvm];

    if (m == B_PRED)
    {
        unsigned int *const bct = is_key ? b_modes : inter_b_modes;

        int b = 0;

        do
        {
            ++ bct[xd->block[b].bmi.mode];
        }
        while (++b < 16);
    }

#endif

    ++cpi->ymode_count[m];
    ++cpi->uv_mode_count[uvm];

}

// Experimental stub function to create a per MB zbin adjustment based on
// some previously calculated measure of MB activity.
static void adjust_act_zbin( VP8_COMP *cpi, MACROBLOCK *x )
{
#if USE_ACT_INDEX
    x->act_zbin_adj = *(x->mb_activity_ptr);
#else
    int64_t a;
    int64_t b;
    int64_t act = *(x->mb_activity_ptr);

    // Apply the masking to the RD multiplier.
    a = act + 4*cpi->activity_avg;
    b = 4*act + cpi->activity_avg;

    if ( act > cpi->activity_avg )
        x->act_zbin_adj = (int)(((int64_t)b + (a>>1))/a) - 1;
    else
        x->act_zbin_adj = 1 - (int)(((int64_t)a + (b>>1))/b);
#endif
}

int vp8cx_encode_intra_macro_block(VP8_COMP *cpi, MACROBLOCK *x, TOKENEXTRA **t,
                                   int mb_row, int mb_col)
{
    MACROBLOCKD *xd = &x->e_mbd;
    int rate;

    if (cpi->sf.RD && cpi->compressor_speed != 2)
        vp8_rd_pick_intra_mode(cpi, x, &rate);
    else
        vp8_pick_intra_mode(cpi, x, &rate);

    if(cpi->oxcf.tuning == VP8_TUNE_SSIM)
    {
        adjust_act_zbin( cpi, x );
        vp8_update_zbin_extra(cpi, x);
    }

    if (x->e_mbd.mode_info_context->mbmi.mode == B_PRED)
        vp8_encode_intra4x4mby(IF_RTCD(&cpi->rtcd), x);
    else
        vp8_encode_intra16x16mby(IF_RTCD(&cpi->rtcd), x);

    vp8_encode_intra16x16mbuv(IF_RTCD(&cpi->rtcd), x);

    sum_intra_stats(cpi, x);
    vp8_tokenize_mb(cpi, &x->e_mbd, t);

    if (xd->mode_info_context->mbmi.mode != B_PRED)
        vp8_inverse_transform_mby(xd, IF_RTCD(&cpi->common.rtcd));

    DEQUANT_INVOKE (&cpi->common.rtcd.dequant, idct_add_uv_block)
                    (xd->qcoeff+16*16, xd->dequant_uv,
                     xd->dst.u_buffer, xd->dst.v_buffer,
                     xd->dst.uv_stride, xd->eobs+16);
    return rate;
}
#ifdef SPEEDSTATS
extern int cnt_pm;
#endif

extern void vp8_fix_contexts(MACROBLOCKD *x);

int vp8cx_encode_inter_macroblock
(
    VP8_COMP *cpi, MACROBLOCK *x, TOKENEXTRA **t,
    int recon_yoffset, int recon_uvoffset,
    int mb_row, int mb_col
)
{
    MACROBLOCKD *const xd = &x->e_mbd;
    int intra_error = 0;
    int rate;
    int distortion;

    x->skip = 0;

    if (xd->segmentation_enabled)
        x->encode_breakout = cpi->segment_encode_breakout[xd->mode_info_context->mbmi.segment_id];
    else
        x->encode_breakout = cpi->oxcf.encode_breakout;

    if (cpi->sf.RD)
    {
        int zbin_mode_boost_enabled = cpi->zbin_mode_boost_enabled;

        /* Are we using the fast quantizer for the mode selection? */
        if(cpi->sf.use_fastquant_for_pick)
        {
            cpi->mb.quantize_b      = QUANTIZE_INVOKE(&cpi->rtcd.quantize,
                                                      fastquantb);
            cpi->mb.quantize_b_pair = QUANTIZE_INVOKE(&cpi->rtcd.quantize,
                                                      fastquantb_pair);

            /* the fast quantizer does not use zbin_extra, so
             * do not recalculate */
            cpi->zbin_mode_boost_enabled = 0;
        }
        vp8_rd_pick_inter_mode(cpi, x, recon_yoffset, recon_uvoffset, &rate,
                               &distortion, &intra_error);

        /* switch back to the regular quantizer for the encode */
        if (cpi->sf.improved_quant)
        {
            cpi->mb.quantize_b      = QUANTIZE_INVOKE(&cpi->rtcd.quantize,
                                                      quantb);
            cpi->mb.quantize_b_pair = QUANTIZE_INVOKE(&cpi->rtcd.quantize,
                                                      quantb_pair);
        }

        /* restore cpi->zbin_mode_boost_enabled */
        cpi->zbin_mode_boost_enabled = zbin_mode_boost_enabled;

    }
    else
    {
        vp8_pick_inter_mode(cpi, x, recon_yoffset, recon_uvoffset, &rate,
                            &distortion, &intra_error, mb_row, mb_col);
    }

    cpi->prediction_error += distortion;
    cpi->intra_error += intra_error;

    if(cpi->oxcf.tuning == VP8_TUNE_SSIM)
    {
        // Adjust the zbin based on this MB rate.
        adjust_act_zbin( cpi, x );
    }

#if 0
    // Experimental RD code
    cpi->frame_distortion += distortion;
    cpi->last_mb_distortion = distortion;
#endif

    // MB level adjutment to quantizer setup
    if (xd->segmentation_enabled)
    {
        // If cyclic update enabled
        if (cpi->current_layer == 0 && cpi->cyclic_refresh_mode_enabled)
        {
            // Clear segment_id back to 0 if not coded (last frame 0,0)
            if ((xd->mode_info_context->mbmi.segment_id == 1) &&
                ((xd->mode_info_context->mbmi.ref_frame != LAST_FRAME) || (xd->mode_info_context->mbmi.mode != ZEROMV)))
            {
                xd->mode_info_context->mbmi.segment_id = 0;

                /* segment_id changed, so update */
                vp8cx_mb_init_quantizer(cpi, x, 1);
            }
        }
    }

    {
        // Experimental code. Special case for gf and arf zeromv modes.
        // Increase zbin size to supress noise
        cpi->zbin_mode_boost = 0;
        if (cpi->zbin_mode_boost_enabled)
        {
            if ( xd->mode_info_context->mbmi.ref_frame != INTRA_FRAME )
            {
                if (xd->mode_info_context->mbmi.mode == ZEROMV)
                {
                    if (xd->mode_info_context->mbmi.ref_frame != LAST_FRAME)
                        cpi->zbin_mode_boost = GF_ZEROMV_ZBIN_BOOST;
                    else
                        cpi->zbin_mode_boost = LF_ZEROMV_ZBIN_BOOST;
                }
                else if (xd->mode_info_context->mbmi.mode == SPLITMV)
                    cpi->zbin_mode_boost = 0;
                else
                    cpi->zbin_mode_boost = MV_ZBIN_BOOST;
            }
        }

        /* The fast quantizer doesn't use zbin_extra, only do so with
         * the regular quantizer. */
        if (cpi->sf.improved_quant)
            vp8_update_zbin_extra(cpi, x);
    }

    cpi->count_mb_ref_frame_usage[xd->mode_info_context->mbmi.ref_frame] ++;

    if (xd->mode_info_context->mbmi.ref_frame == INTRA_FRAME)
    {
        vp8_encode_intra16x16mbuv(IF_RTCD(&cpi->rtcd), x);

        if (xd->mode_info_context->mbmi.mode == B_PRED)
        {
            vp8_encode_intra4x4mby(IF_RTCD(&cpi->rtcd), x);
        }
        else
        {
            vp8_encode_intra16x16mby(IF_RTCD(&cpi->rtcd), x);
        }

        sum_intra_stats(cpi, x);
    }
    else
    {
        int ref_fb_idx;

        if (xd->mode_info_context->mbmi.ref_frame == LAST_FRAME)
            ref_fb_idx = cpi->common.lst_fb_idx;
        else if (xd->mode_info_context->mbmi.ref_frame == GOLDEN_FRAME)
            ref_fb_idx = cpi->common.gld_fb_idx;
        else
            ref_fb_idx = cpi->common.alt_fb_idx;

        xd->pre.y_buffer = cpi->common.yv12_fb[ref_fb_idx].y_buffer + recon_yoffset;
        xd->pre.u_buffer = cpi->common.yv12_fb[ref_fb_idx].u_buffer + recon_uvoffset;
        xd->pre.v_buffer = cpi->common.yv12_fb[ref_fb_idx].v_buffer + recon_uvoffset;

        if (!x->skip)
        {
            vp8_encode_inter16x16(IF_RTCD(&cpi->rtcd), x);

            // Clear mb_skip_coeff if mb_no_coeff_skip is not set
            if (!cpi->common.mb_no_coeff_skip)
                xd->mode_info_context->mbmi.mb_skip_coeff = 0;

        }
        else
            vp8_build_inter16x16_predictors_mb(xd, xd->dst.y_buffer,
                                           xd->dst.u_buffer, xd->dst.v_buffer,
                                           xd->dst.y_stride, xd->dst.uv_stride);

    }

    if (!x->skip)
    {
        vp8_tokenize_mb(cpi, xd, t);

        if (xd->mode_info_context->mbmi.mode != B_PRED)
            vp8_inverse_transform_mby(xd, IF_RTCD(&cpi->common.rtcd));

        DEQUANT_INVOKE (&cpi->common.rtcd.dequant, idct_add_uv_block)
                        (xd->qcoeff+16*16, xd->dequant_uv,
                         xd->dst.u_buffer, xd->dst.v_buffer,
                         xd->dst.uv_stride, xd->eobs+16);
    }
    else
    {
        if (cpi->common.mb_no_coeff_skip)
        {
            xd->mode_info_context->mbmi.mb_skip_coeff = 1;
            cpi->skip_true_count ++;
            vp8_fix_contexts(xd);
        }
        else
        {
            vp8_stuff_mb(cpi, xd, t);
            xd->mode_info_context->mbmi.mb_skip_coeff = 0;
            cpi->skip_false_count ++;
        }
    }

    return rate;
}
