/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#ifndef WIN32
# include <unistd.h>
#endif
#ifdef __APPLE__
#include <mach/mach_init.h>
#endif
#include "onyxd_int.h"
#include "vpx_mem/vpx_mem.h"
#include "threading.h"

#include "loopfilter.h"
#include "extend.h"
#include "vpx_ports/vpx_timer.h"
#include "detokenize.h"
#include "reconinter.h"
#include "reconintra_mt.h"

extern void mb_init_dequantizer(VP8D_COMP *pbi, MACROBLOCKD *xd);
extern void clamp_mvs(MACROBLOCKD *xd);
extern void vp8_build_uvmvs(MACROBLOCKD *x, int fullpixel);

#if CONFIG_RUNTIME_CPU_DETECT
#define RTCD_VTABLE(x) (&(pbi)->common.rtcd.x)
#else
#define RTCD_VTABLE(x) NULL
#endif

void vp8_setup_decoding_thread_data(VP8D_COMP *pbi, MACROBLOCKD *xd, MB_ROW_DEC *mbrd, int count)
{
#if CONFIG_MULTITHREAD
    VP8_COMMON *const pc = & pbi->common;
    int i, j;

    for (i = 0; i < count; i++)
    {
        MACROBLOCKD *mbd = &mbrd[i].mbd;
#if CONFIG_RUNTIME_CPU_DETECT
        mbd->rtcd = xd->rtcd;
#endif
        mbd->subpixel_predict        = xd->subpixel_predict;
        mbd->subpixel_predict8x4     = xd->subpixel_predict8x4;
        mbd->subpixel_predict8x8     = xd->subpixel_predict8x8;
        mbd->subpixel_predict16x16   = xd->subpixel_predict16x16;

        mbd->mode_info_context = pc->mi   + pc->mode_info_stride * (i + 1);
        mbd->mode_info_stride  = pc->mode_info_stride;

        mbd->frame_type = pc->frame_type;
        mbd->frames_since_golden      = pc->frames_since_golden;
        mbd->frames_till_alt_ref_frame  = pc->frames_till_alt_ref_frame;

        mbd->pre = pc->yv12_fb[pc->lst_fb_idx];
        mbd->dst = pc->yv12_fb[pc->new_fb_idx];

        vp8_setup_block_dptrs(mbd);
        vp8_build_block_doffsets(mbd);
        mbd->segmentation_enabled    = xd->segmentation_enabled;
        mbd->mb_segement_abs_delta     = xd->mb_segement_abs_delta;
        vpx_memcpy(mbd->segment_feature_data, xd->segment_feature_data, sizeof(xd->segment_feature_data));

        /*signed char ref_lf_deltas[MAX_REF_LF_DELTAS];*/
        vpx_memcpy(mbd->ref_lf_deltas, xd->ref_lf_deltas, sizeof(xd->ref_lf_deltas));
        /*signed char mode_lf_deltas[MAX_MODE_LF_DELTAS];*/
        vpx_memcpy(mbd->mode_lf_deltas, xd->mode_lf_deltas, sizeof(xd->mode_lf_deltas));
        /*unsigned char mode_ref_lf_delta_enabled;
        unsigned char mode_ref_lf_delta_update;*/
        mbd->mode_ref_lf_delta_enabled    = xd->mode_ref_lf_delta_enabled;
        mbd->mode_ref_lf_delta_update    = xd->mode_ref_lf_delta_update;

        mbd->current_bc = &pbi->bc2;

        for (j = 0; j < 25; j++)
        {
            mbd->block[j].dequant = xd->block[j].dequant;
        }
    }

    for (i=0; i< pc->mb_rows; i++)
        pbi->mt_current_mb_col[i]=-1;
#else
    (void) pbi;
    (void) xd;
    (void) mbrd;
    (void) count;
#endif
}


void vp8mt_decode_macroblock(VP8D_COMP *pbi, MACROBLOCKD *xd, int mb_row, int mb_col)
{
#if CONFIG_MULTITHREAD
    int eobtotal = 0;
    int i, do_clamp = xd->mode_info_context->mbmi.need_to_clamp_mvs;
    VP8_COMMON *pc = &pbi->common;

    if (xd->mode_info_context->mbmi.mb_skip_coeff)
    {
        vp8_reset_mb_tokens_context(xd);
    }
    else
    {
        eobtotal = vp8_decode_mb_tokens(pbi, xd);
    }

    /* Perform temporary clamping of the MV to be used for prediction */
    if (do_clamp)
    {
        clamp_mvs(xd);
    }

    xd->mode_info_context->mbmi.dc_diff = 1;

    if (xd->mode_info_context->mbmi.mode != B_PRED && xd->mode_info_context->mbmi.mode != SPLITMV && eobtotal == 0)
    {
        xd->mode_info_context->mbmi.dc_diff = 0;

        /*mt_skip_recon_mb(pbi, xd, mb_row, mb_col);*/
        if (xd->frame_type == KEY_FRAME  ||  xd->mode_info_context->mbmi.ref_frame == INTRA_FRAME)
        {
            vp8mt_build_intra_predictors_mbuv_s(pbi, xd, mb_row, mb_col);
            vp8mt_build_intra_predictors_mby_s(pbi, xd, mb_row, mb_col);
        }
        else
        {
            vp8_build_inter_predictors_mb_s(xd);
        }
        return;
    }

    if (xd->segmentation_enabled)
        mb_init_dequantizer(pbi, xd);

    /* do prediction */
    if (xd->frame_type == KEY_FRAME  ||  xd->mode_info_context->mbmi.ref_frame == INTRA_FRAME)
    {
        vp8mt_build_intra_predictors_mbuv(pbi, xd, mb_row, mb_col);

        if (xd->mode_info_context->mbmi.mode != B_PRED)
        {
            vp8mt_build_intra_predictors_mby(pbi, xd, mb_row, mb_col);
        } else {
            vp8mt_intra_prediction_down_copy(pbi, xd, mb_row, mb_col);
        }
    }
    else
    {
        vp8_build_inter_predictors_mb(xd);
    }

    /* dequantization and idct */
    if (xd->mode_info_context->mbmi.mode != B_PRED && xd->mode_info_context->mbmi.mode != SPLITMV)
    {
        BLOCKD *b = &xd->block[24];
        DEQUANT_INVOKE(&pbi->dequant, block)(b);

        /* do 2nd order transform on the dc block */
        if (xd->eobs[24] > 1)
        {
            IDCT_INVOKE(RTCD_VTABLE(idct), iwalsh16)(&b->dqcoeff[0], b->diff);
            ((int *)b->qcoeff)[0] = 0;
            ((int *)b->qcoeff)[1] = 0;
            ((int *)b->qcoeff)[2] = 0;
            ((int *)b->qcoeff)[3] = 0;
            ((int *)b->qcoeff)[4] = 0;
            ((int *)b->qcoeff)[5] = 0;
            ((int *)b->qcoeff)[6] = 0;
            ((int *)b->qcoeff)[7] = 0;
        }
        else
        {
            IDCT_INVOKE(RTCD_VTABLE(idct), iwalsh1)(&b->dqcoeff[0], b->diff);
            ((int *)b->qcoeff)[0] = 0;
        }

        DEQUANT_INVOKE (&pbi->dequant, dc_idct_add_y_block)
                        (xd->qcoeff, xd->block[0].dequant,
                         xd->predictor, xd->dst.y_buffer,
                         xd->dst.y_stride, xd->eobs, xd->block[24].diff);
    }
    else if ((xd->frame_type == KEY_FRAME  ||  xd->mode_info_context->mbmi.ref_frame == INTRA_FRAME) && xd->mode_info_context->mbmi.mode == B_PRED)
    {
        for (i = 0; i < 16; i++)
        {
            BLOCKD *b = &xd->block[i];
            vp8mt_predict_intra4x4(pbi, xd, b->bmi.mode, b->predictor, mb_row, mb_col, i);

            if (xd->eobs[i] > 1)
            {
                DEQUANT_INVOKE(&pbi->dequant, idct_add)
                    (b->qcoeff, b->dequant,  b->predictor,
                    *(b->base_dst) + b->dst, 16, b->dst_stride);
            }
            else
            {
                IDCT_INVOKE(RTCD_VTABLE(idct), idct1_scalar_add)
                    (b->qcoeff[0] * b->dequant[0], b->predictor,
                    *(b->base_dst) + b->dst, 16, b->dst_stride);
                ((int *)b->qcoeff)[0] = 0;
            }
        }
    }
    else
    {
        DEQUANT_INVOKE (&pbi->dequant, idct_add_y_block)
                        (xd->qcoeff, xd->block[0].dequant,
                         xd->predictor, xd->dst.y_buffer,
                         xd->dst.y_stride, xd->eobs);
    }

    DEQUANT_INVOKE (&pbi->dequant, idct_add_uv_block)
                    (xd->qcoeff+16*16, xd->block[16].dequant,
                     xd->predictor+16*16, xd->dst.u_buffer, xd->dst.v_buffer,
                     xd->dst.uv_stride, xd->eobs+16);
#else
    (void) pbi;
    (void) xd;
    (void) mb_row;
    (void) mb_col;
#endif
}


THREAD_FUNCTION vp8_thread_decoding_proc(void *p_data)
{
#if CONFIG_MULTITHREAD
    int ithread = ((DECODETHREAD_DATA *)p_data)->ithread;
    VP8D_COMP *pbi = (VP8D_COMP *)(((DECODETHREAD_DATA *)p_data)->ptr1);
    MB_ROW_DEC *mbrd = (MB_ROW_DEC *)(((DECODETHREAD_DATA *)p_data)->ptr2);
    ENTROPY_CONTEXT_PLANES mb_row_left_context;

    while (1)
    {
        if (pbi->b_multithreaded_rd == 0)
            break;

        /*if(WaitForSingleObject(pbi->h_event_start_decoding[ithread], INFINITE) == WAIT_OBJECT_0)*/
        if (sem_wait(&pbi->h_event_start_decoding[ithread]) == 0)
        {
            if (pbi->b_multithreaded_rd == 0)
                break;
            else
            {
                VP8_COMMON *pc = &pbi->common;
                MACROBLOCKD *xd = &mbrd->mbd;

                int mb_row;
                int num_part = 1 << pbi->common.multi_token_partition;
                volatile int *last_row_current_mb_col;
                int nsync = pbi->sync_range;

                for (mb_row = ithread+1; mb_row < pc->mb_rows; mb_row += (pbi->decoding_thread_count + 1))
                {
                    int i;
                    int recon_yoffset, recon_uvoffset;
                    int mb_col;
                    int ref_fb_idx = pc->lst_fb_idx;
                    int dst_fb_idx = pc->new_fb_idx;
                    int recon_y_stride = pc->yv12_fb[ref_fb_idx].y_stride;
                    int recon_uv_stride = pc->yv12_fb[ref_fb_idx].uv_stride;

                    int filter_level;
                    loop_filter_info *lfi = pc->lf_info;
                    int alt_flt_enabled = xd->segmentation_enabled;
                    int Segment;

                    pbi->mb_row_di[ithread].mb_row = mb_row;
                    pbi->mb_row_di[ithread].mbd.current_bc =  &pbi->mbc[mb_row%num_part];

                    last_row_current_mb_col = &pbi->mt_current_mb_col[mb_row -1];

                    recon_yoffset = mb_row * recon_y_stride * 16;
                    recon_uvoffset = mb_row * recon_uv_stride * 8;
                    /* reset above block coeffs */

                    xd->above_context = pc->above_context;
                    xd->left_context = &mb_row_left_context;
                    vpx_memset(&mb_row_left_context, 0, sizeof(mb_row_left_context));
                    xd->up_available = (mb_row != 0);

                    xd->mb_to_top_edge = -((mb_row * 16)) << 3;
                    xd->mb_to_bottom_edge = ((pc->mb_rows - 1 - mb_row) * 16) << 3;

                    for (mb_col = 0; mb_col < pc->mb_cols; mb_col++)
                    {
                        if ((mb_col & (nsync-1)) == 0)
                        {
                            while (mb_col > (*last_row_current_mb_col - nsync) && *last_row_current_mb_col != pc->mb_cols - 1)
                            {
                                x86_pause_hint();
                                thread_sleep(0);
                            }
                        }

                        if (xd->mode_info_context->mbmi.mode == SPLITMV || xd->mode_info_context->mbmi.mode == B_PRED)
                        {
                            for (i = 0; i < 16; i++)
                            {
                                BLOCKD *d = &xd->block[i];
                                vpx_memcpy(&d->bmi, &xd->mode_info_context->bmi[i], sizeof(B_MODE_INFO));
                            }
                        }

                        if(pbi->common.filter_level)
                        {
                            /*update loopfilter info*/
                            Segment = (alt_flt_enabled) ? xd->mode_info_context->mbmi.segment_id : 0;
                            filter_level = pbi->mt_baseline_filter_level[Segment];
                            /* Distance of Mb to the various image edges.
                             * These are specified to 8th pel as they are always compared to values that are in 1/8th pel units
                             * Apply any context driven MB level adjustment
                             */
                            vp8_adjust_mb_lf_value(xd, &filter_level);
                        }

                        /* Distance of Mb to the various image edges.
                         * These are specified to 8th pel as they are always compared to values that are in 1/8th pel units
                         */
                        xd->mb_to_left_edge = -((mb_col * 16) << 3);
                        xd->mb_to_right_edge = ((pc->mb_cols - 1 - mb_col) * 16) << 3;

                        xd->dst.y_buffer = pc->yv12_fb[dst_fb_idx].y_buffer + recon_yoffset;
                        xd->dst.u_buffer = pc->yv12_fb[dst_fb_idx].u_buffer + recon_uvoffset;
                        xd->dst.v_buffer = pc->yv12_fb[dst_fb_idx].v_buffer + recon_uvoffset;

                        xd->left_available = (mb_col != 0);

                        /* Select the appropriate reference frame for this MB */
                        if (xd->mode_info_context->mbmi.ref_frame == LAST_FRAME)
                            ref_fb_idx = pc->lst_fb_idx;
                        else if (xd->mode_info_context->mbmi.ref_frame == GOLDEN_FRAME)
                            ref_fb_idx = pc->gld_fb_idx;
                        else
                            ref_fb_idx = pc->alt_fb_idx;

                        xd->pre.y_buffer = pc->yv12_fb[ref_fb_idx].y_buffer + recon_yoffset;
                        xd->pre.u_buffer = pc->yv12_fb[ref_fb_idx].u_buffer + recon_uvoffset;
                        xd->pre.v_buffer = pc->yv12_fb[ref_fb_idx].v_buffer + recon_uvoffset;

                        vp8_build_uvmvs(xd, pc->full_pixel);
                        vp8mt_decode_macroblock(pbi, xd, mb_row, mb_col);

                        if (pbi->common.filter_level)
                        {
                            if( mb_row != pc->mb_rows-1 )
                            {
                                /* Save decoded MB last row data for next-row decoding */
                                vpx_memcpy((pbi->mt_yabove_row[mb_row + 1] + 32 + mb_col*16), (xd->dst.y_buffer + 15 * recon_y_stride), 16);
                                vpx_memcpy((pbi->mt_uabove_row[mb_row + 1] + 16 + mb_col*8), (xd->dst.u_buffer + 7 * recon_uv_stride), 8);
                                vpx_memcpy((pbi->mt_vabove_row[mb_row + 1] + 16 + mb_col*8), (xd->dst.v_buffer + 7 * recon_uv_stride), 8);
                            }

                            /* save left_col for next MB decoding */
                            if(mb_col != pc->mb_cols-1)
                            {
                                MODE_INFO *next = xd->mode_info_context +1;

                                if (xd->frame_type == KEY_FRAME  ||  next->mbmi.ref_frame == INTRA_FRAME)
                                {
                                    for (i = 0; i < 16; i++)
                                        pbi->mt_yleft_col[mb_row][i] = xd->dst.y_buffer [i* recon_y_stride + 15];
                                    for (i = 0; i < 8; i++)
                                    {
                                        pbi->mt_uleft_col[mb_row][i] = xd->dst.u_buffer [i* recon_uv_stride + 7];
                                        pbi->mt_vleft_col[mb_row][i] = xd->dst.v_buffer [i* recon_uv_stride + 7];
                                    }
                                }
                            }

                          /* loopfilter on this macroblock. */
                            if (filter_level)
                            {
                                if (mb_col > 0)
                                    pc->lf_mbv(xd->dst.y_buffer, xd->dst.u_buffer, xd->dst.v_buffer, recon_y_stride, recon_uv_stride, &lfi[filter_level], pc->simpler_lpf);

                                if (xd->mode_info_context->mbmi.dc_diff > 0)
                                    pc->lf_bv(xd->dst.y_buffer, xd->dst.u_buffer, xd->dst.v_buffer, recon_y_stride, recon_uv_stride, &lfi[filter_level], pc->simpler_lpf);

                                /* don't apply across umv border */
                                if (mb_row > 0)
                                    pc->lf_mbh(xd->dst.y_buffer, xd->dst.u_buffer, xd->dst.v_buffer, recon_y_stride, recon_uv_stride, &lfi[filter_level], pc->simpler_lpf);

                                if (xd->mode_info_context->mbmi.dc_diff > 0)
                                    pc->lf_bh(xd->dst.y_buffer, xd->dst.u_buffer, xd->dst.v_buffer, recon_y_stride, recon_uv_stride, &lfi[filter_level], pc->simpler_lpf);
                            }
                        }

                        recon_yoffset += 16;
                        recon_uvoffset += 8;

                        ++xd->mode_info_context;  /* next mb */

                        xd->above_context++;

                        /*pbi->mb_row_di[ithread].current_mb_col = mb_col;*/
                        pbi->mt_current_mb_col[mb_row] = mb_col;
                    }

                    /* adjust to the next row of mbs */
                    if (pbi->common.filter_level)
                    {
                        if(mb_row != pc->mb_rows-1)
                        {
                            int lasty = pc->yv12_fb[ref_fb_idx].y_width + VP8BORDERINPIXELS;
                            int lastuv = (pc->yv12_fb[ref_fb_idx].y_width>>1) + (VP8BORDERINPIXELS>>1);

                            for (i = 0; i < 4; i++)
                            {
                                pbi->mt_yabove_row[mb_row +1][lasty + i] = pbi->mt_yabove_row[mb_row +1][lasty -1];
                                pbi->mt_uabove_row[mb_row +1][lastuv + i] = pbi->mt_uabove_row[mb_row +1][lastuv -1];
                                pbi->mt_vabove_row[mb_row +1][lastuv + i] = pbi->mt_vabove_row[mb_row +1][lastuv -1];
                            }
                        }
                    } else
                        vp8_extend_mb_row(&pc->yv12_fb[dst_fb_idx], xd->dst.y_buffer + 16, xd->dst.u_buffer + 8, xd->dst.v_buffer + 8);

                    ++xd->mode_info_context;      /* skip prediction column */

                    /* since we have multithread */
                    xd->mode_info_context += xd->mode_info_stride * pbi->decoding_thread_count;
                }
            }
        }
        /*  add this to each frame */
        if ((mbrd->mb_row == pbi->common.mb_rows-1) || ((mbrd->mb_row == pbi->common.mb_rows-2) && (pbi->common.mb_rows % (pbi->decoding_thread_count+1))==1))
        {
            /*SetEvent(pbi->h_event_end_decoding);*/
            sem_post(&pbi->h_event_end_decoding);
        }
    }
#else
    (void) p_data;
#endif

    return 0 ;
}


void vp8_decoder_create_threads(VP8D_COMP *pbi)
{
#if CONFIG_MULTITHREAD
    int core_count = 0;
    int ithread;
    int i;

    pbi->b_multithreaded_rd = 0;
    pbi->allocated_decoding_thread_count = 0;
    core_count = (pbi->max_threads > 16) ? 16 : pbi->max_threads;

    if (core_count > 1)
    {
        pbi->b_multithreaded_rd = 1;
        pbi->decoding_thread_count = core_count -1;

        CHECK_MEM_ERROR(pbi->h_decoding_thread, vpx_malloc(sizeof(pthread_t) * pbi->decoding_thread_count));
        CHECK_MEM_ERROR(pbi->h_event_start_decoding, vpx_malloc(sizeof(sem_t) * pbi->decoding_thread_count));
        CHECK_MEM_ERROR(pbi->mb_row_di, vpx_memalign(32, sizeof(MB_ROW_DEC) * pbi->decoding_thread_count));
        vpx_memset(pbi->mb_row_di, 0, sizeof(MB_ROW_DEC) * pbi->decoding_thread_count);
        CHECK_MEM_ERROR(pbi->de_thread_data, vpx_malloc(sizeof(DECODETHREAD_DATA) * pbi->decoding_thread_count));

        for (ithread = 0; ithread < pbi->decoding_thread_count; ithread++)
        {
            sem_init(&pbi->h_event_start_decoding[ithread], 0, 0);

            pbi->de_thread_data[ithread].ithread  = ithread;
            pbi->de_thread_data[ithread].ptr1     = (void *)pbi;
            pbi->de_thread_data[ithread].ptr2     = (void *) &pbi->mb_row_di[ithread];

            pthread_create(&pbi->h_decoding_thread[ithread], 0, vp8_thread_decoding_proc, (&pbi->de_thread_data[ithread]));
        }

        sem_init(&pbi->h_event_end_decoding, 0, 0);

        pbi->allocated_decoding_thread_count = pbi->decoding_thread_count;
    }

#else
    (void) pbi;
#endif
}


void vp8mt_de_alloc_temp_buffers(VP8D_COMP *pbi, int mb_rows)
{
#if CONFIG_MULTITHREAD
    VP8_COMMON *const pc = & pbi->common;
    int i;

    if (pbi->b_multithreaded_rd)
    {
        if (pbi->mt_current_mb_col)
        {
            vpx_free(pbi->mt_current_mb_col);
            pbi->mt_current_mb_col = NULL ;
        }

        /* Free above_row buffers. */
        if (pbi->mt_yabove_row)
        {
            for (i=0; i< mb_rows; i++)
            {
                if (pbi->mt_yabove_row[i])
                {
                    vpx_free(pbi->mt_yabove_row[i]);
                    pbi->mt_yabove_row[i] = NULL ;
                }
            }
            vpx_free(pbi->mt_yabove_row);
            pbi->mt_yabove_row = NULL ;
        }

        if (pbi->mt_uabove_row)
        {
            for (i=0; i< mb_rows; i++)
            {
                if (pbi->mt_uabove_row[i])
                {
                    vpx_free(pbi->mt_uabove_row[i]);
                    pbi->mt_uabove_row[i] = NULL ;
                }
            }
            vpx_free(pbi->mt_uabove_row);
            pbi->mt_uabove_row = NULL ;
        }

        if (pbi->mt_vabove_row)
        {
            for (i=0; i< mb_rows; i++)
            {
                if (pbi->mt_vabove_row[i])
                {
                    vpx_free(pbi->mt_vabove_row[i]);
                    pbi->mt_vabove_row[i] = NULL ;
                }
            }
            vpx_free(pbi->mt_vabove_row);
            pbi->mt_vabove_row = NULL ;
        }

        /* Free left_col buffers. */
        if (pbi->mt_yleft_col)
        {
            for (i=0; i< mb_rows; i++)
            {
                if (pbi->mt_yleft_col[i])
                {
                    vpx_free(pbi->mt_yleft_col[i]);
                    pbi->mt_yleft_col[i] = NULL ;
                }
            }
            vpx_free(pbi->mt_yleft_col);
            pbi->mt_yleft_col = NULL ;
        }

        if (pbi->mt_uleft_col)
        {
            for (i=0; i< mb_rows; i++)
            {
                if (pbi->mt_uleft_col[i])
                {
                    vpx_free(pbi->mt_uleft_col[i]);
                    pbi->mt_uleft_col[i] = NULL ;
                }
            }
            vpx_free(pbi->mt_uleft_col);
            pbi->mt_uleft_col = NULL ;
        }

        if (pbi->mt_vleft_col)
        {
            for (i=0; i< mb_rows; i++)
            {
                if (pbi->mt_vleft_col[i])
                {
                    vpx_free(pbi->mt_vleft_col[i]);
                    pbi->mt_vleft_col[i] = NULL ;
                }
            }
            vpx_free(pbi->mt_vleft_col);
            pbi->mt_vleft_col = NULL ;
        }
    }
#else
    (void) pbi;
#endif
}


int vp8mt_alloc_temp_buffers(VP8D_COMP *pbi, int width, int prev_mb_rows)
{
#if CONFIG_MULTITHREAD
    VP8_COMMON *const pc = & pbi->common;
    int i;
    int uv_width;

    if (pbi->b_multithreaded_rd)
    {
        vp8mt_de_alloc_temp_buffers(pbi, prev_mb_rows);

        /* our internal buffers are always multiples of 16 */
        if ((width & 0xf) != 0)
            width += 16 - (width & 0xf);

        if (width < 640) pbi->sync_range = 1;
        else if (width <= 1280) pbi->sync_range = 8;
        else if (width <= 2560) pbi->sync_range =16;
        else pbi->sync_range = 32;

        uv_width = width >>1;

        /* Allocate an int for each mb row. */
        CHECK_MEM_ERROR(pbi->mt_current_mb_col, vpx_malloc(sizeof(int) * pc->mb_rows));

        /* Allocate memory for above_row buffers. */
        CHECK_MEM_ERROR(pbi->mt_yabove_row, vpx_malloc(sizeof(unsigned char *) * pc->mb_rows));
        for (i=0; i< pc->mb_rows; i++)
            CHECK_MEM_ERROR(pbi->mt_yabove_row[i], vpx_calloc(sizeof(unsigned char) * (width + (VP8BORDERINPIXELS<<1)), 1));

        CHECK_MEM_ERROR(pbi->mt_uabove_row, vpx_malloc(sizeof(unsigned char *) * pc->mb_rows));
        for (i=0; i< pc->mb_rows; i++)
            CHECK_MEM_ERROR(pbi->mt_uabove_row[i], vpx_calloc(sizeof(unsigned char) * (uv_width + VP8BORDERINPIXELS), 1));

        CHECK_MEM_ERROR(pbi->mt_vabove_row, vpx_malloc(sizeof(unsigned char *) * pc->mb_rows));
        for (i=0; i< pc->mb_rows; i++)
            CHECK_MEM_ERROR(pbi->mt_vabove_row[i], vpx_calloc(sizeof(unsigned char) * (uv_width + VP8BORDERINPIXELS), 1));

        /* Allocate memory for left_col buffers. */
        CHECK_MEM_ERROR(pbi->mt_yleft_col, vpx_malloc(sizeof(unsigned char *) * pc->mb_rows));
        for (i=0; i< pc->mb_rows; i++)
            CHECK_MEM_ERROR(pbi->mt_yleft_col[i], vpx_calloc(sizeof(unsigned char) * 16, 1));

        CHECK_MEM_ERROR(pbi->mt_uleft_col, vpx_malloc(sizeof(unsigned char *) * pc->mb_rows));
        for (i=0; i< pc->mb_rows; i++)
            CHECK_MEM_ERROR(pbi->mt_uleft_col[i], vpx_calloc(sizeof(unsigned char) * 8, 1));

        CHECK_MEM_ERROR(pbi->mt_vleft_col, vpx_malloc(sizeof(unsigned char *) * pc->mb_rows));
        for (i=0; i< pc->mb_rows; i++)
            CHECK_MEM_ERROR(pbi->mt_vleft_col[i], vpx_calloc(sizeof(unsigned char) * 8, 1));
    }
    return 0;
#else
    (void) pbi;
    (void) width;
#endif
}


void vp8_decoder_remove_threads(VP8D_COMP *pbi)
{
#if CONFIG_MULTITHREAD

    /* shutdown MB Decoding thread; */
    if (pbi->b_multithreaded_rd)
    {
        int i;

        pbi->b_multithreaded_rd = 0;

        /* allow all threads to exit */
        for (i = 0; i < pbi->allocated_decoding_thread_count; i++)
        {
            sem_post(&pbi->h_event_start_decoding[i]);
            pthread_join(pbi->h_decoding_thread[i], NULL);
        }

        for (i = 0; i < pbi->allocated_decoding_thread_count; i++)
        {
            sem_destroy(&pbi->h_event_start_decoding[i]);
        }

        sem_destroy(&pbi->h_event_end_decoding);

        if (pbi->h_decoding_thread)
        {
            vpx_free(pbi->h_decoding_thread);
            pbi->h_decoding_thread = NULL;
        }

        if (pbi->h_event_start_decoding)
        {
            vpx_free(pbi->h_event_start_decoding);
            pbi->h_event_start_decoding = NULL;
        }

        if (pbi->mb_row_di)
        {
            vpx_free(pbi->mb_row_di);
            pbi->mb_row_di = NULL ;
        }

        if (pbi->de_thread_data)
        {
            vpx_free(pbi->de_thread_data);
            pbi->de_thread_data = NULL;
        }
    }
#else
    (void) pbi;
#endif
}


void vp8mt_lpf_init( VP8D_COMP *pbi, int default_filt_lvl)
{
#if CONFIG_MULTITHREAD
    VP8_COMMON *cm  = &pbi->common;
    MACROBLOCKD *mbd = &pbi->mb;
    /*YV12_BUFFER_CONFIG *post = &cm->new_frame;*/  /*frame_to_show;*/
    loop_filter_info *lfi = cm->lf_info;
    FRAME_TYPE frame_type = cm->frame_type;

    /*int mb_row;
    int mb_col;
    int baseline_filter_level[MAX_MB_SEGMENTS];*/
    int filter_level;
    int alt_flt_enabled = mbd->segmentation_enabled;

    int i;
    /*unsigned char *y_ptr, *u_ptr, *v_ptr;*/

    /* Note the baseline filter values for each segment */
    if (alt_flt_enabled)
    {
        for (i = 0; i < MAX_MB_SEGMENTS; i++)
        {
            /* Abs value */
            if (mbd->mb_segement_abs_delta == SEGMENT_ABSDATA)
                pbi->mt_baseline_filter_level[i] = mbd->segment_feature_data[MB_LVL_ALT_LF][i];
            /* Delta Value */
            else
            {
                pbi->mt_baseline_filter_level[i] = default_filt_lvl + mbd->segment_feature_data[MB_LVL_ALT_LF][i];
                pbi->mt_baseline_filter_level[i] = (pbi->mt_baseline_filter_level[i] >= 0) ? ((pbi->mt_baseline_filter_level[i] <= MAX_LOOP_FILTER) ? pbi->mt_baseline_filter_level[i] : MAX_LOOP_FILTER) : 0;  /* Clamp to valid range */
            }
        }
    }
    else
    {
        for (i = 0; i < MAX_MB_SEGMENTS; i++)
            pbi->mt_baseline_filter_level[i] = default_filt_lvl;
    }

    /* Initialize the loop filter for this frame. */
    if ((cm->last_filter_type != cm->filter_type) || (cm->last_sharpness_level != cm->sharpness_level))
        vp8_init_loop_filter(cm);
    else if (frame_type != cm->last_frame_type)
        vp8_frame_init_loop_filter(lfi, frame_type);
#else
    (void) pbi;
    (void) default_filt_lvl;
#endif
}


void vp8mt_decode_mb_rows( VP8D_COMP *pbi, MACROBLOCKD *xd)
{
#if CONFIG_MULTITHREAD
    int mb_row;
    VP8_COMMON *pc = &pbi->common;

    int ibc = 0;
    int num_part = 1 << pbi->common.multi_token_partition;
    int i, j;
    volatile int *last_row_current_mb_col = NULL;
    int nsync = pbi->sync_range;

    int filter_level;
    loop_filter_info *lfi = pc->lf_info;
    int alt_flt_enabled = xd->segmentation_enabled;
    int Segment;

    if(pbi->common.filter_level)
    {
        /* Set above_row buffer to 127 for decoding first MB row */
        vpx_memset(pbi->mt_yabove_row[0] + VP8BORDERINPIXELS-1, 127, pc->yv12_fb[pc->lst_fb_idx].y_width + 5);
        vpx_memset(pbi->mt_uabove_row[0] + (VP8BORDERINPIXELS>>1)-1, 127, (pc->yv12_fb[pc->lst_fb_idx].y_width>>1) +5);
        vpx_memset(pbi->mt_vabove_row[0] + (VP8BORDERINPIXELS>>1)-1, 127, (pc->yv12_fb[pc->lst_fb_idx].y_width>>1) +5);

        for (i=1; i<pc->mb_rows; i++)
        {
            vpx_memset(pbi->mt_yabove_row[i] + VP8BORDERINPIXELS-1, (unsigned char)129, 1);
            vpx_memset(pbi->mt_uabove_row[i] + (VP8BORDERINPIXELS>>1)-1, (unsigned char)129, 1);
            vpx_memset(pbi->mt_vabove_row[i] + (VP8BORDERINPIXELS>>1)-1, (unsigned char)129, 1);
        }

        /* Set left_col to 129 initially */
        for (i=0; i<pc->mb_rows; i++)
        {
            vpx_memset(pbi->mt_yleft_col[i], (unsigned char)129, 16);
            vpx_memset(pbi->mt_uleft_col[i], (unsigned char)129, 8);
            vpx_memset(pbi->mt_vleft_col[i], (unsigned char)129, 8);
        }
        vp8mt_lpf_init(pbi, pc->filter_level);
    }

    vp8_setup_decoding_thread_data(pbi, xd, pbi->mb_row_di, pbi->decoding_thread_count);

    for (i = 0; i < pbi->decoding_thread_count; i++)
        sem_post(&pbi->h_event_start_decoding[i]);

    for (mb_row = 0; mb_row < pc->mb_rows; mb_row += (pbi->decoding_thread_count + 1))
    {
        int i;

        xd->current_bc = &pbi->mbc[mb_row%num_part];

        /* vp8_decode_mb_row(pbi, pc, mb_row, xd); */
        {
            int i;
            int recon_yoffset, recon_uvoffset;
            int mb_col;
            int ref_fb_idx = pc->lst_fb_idx;
            int dst_fb_idx = pc->new_fb_idx;
            int recon_y_stride = pc->yv12_fb[ref_fb_idx].y_stride;
            int recon_uv_stride = pc->yv12_fb[ref_fb_idx].uv_stride;

           /* volatile int *last_row_current_mb_col = NULL; */
            if (mb_row > 0)
                last_row_current_mb_col = &pbi->mt_current_mb_col[mb_row -1];

            vpx_memset(&pc->left_context, 0, sizeof(pc->left_context));
            recon_yoffset = mb_row * recon_y_stride * 16;
            recon_uvoffset = mb_row * recon_uv_stride * 8;
            /* reset above block coeffs */

            xd->above_context = pc->above_context;
            xd->up_available = (mb_row != 0);

            xd->mb_to_top_edge = -((mb_row * 16)) << 3;
            xd->mb_to_bottom_edge = ((pc->mb_rows - 1 - mb_row) * 16) << 3;

            for (mb_col = 0; mb_col < pc->mb_cols; mb_col++)
            {
                if ( mb_row > 0 && (mb_col & (nsync-1)) == 0){
                    while (mb_col > (*last_row_current_mb_col - nsync) && *last_row_current_mb_col != pc->mb_cols - 1)
                    {
                        x86_pause_hint();
                        thread_sleep(0);
                    }
                }

                if (xd->mode_info_context->mbmi.mode == SPLITMV || xd->mode_info_context->mbmi.mode == B_PRED)
                {
                    for (i = 0; i < 16; i++)
                    {
                        BLOCKD *d = &xd->block[i];
                        vpx_memcpy(&d->bmi, &xd->mode_info_context->bmi[i], sizeof(B_MODE_INFO));
                    }
                }

                if(pbi->common.filter_level)
                {
                    /* update loopfilter info */
                    Segment = (alt_flt_enabled) ? xd->mode_info_context->mbmi.segment_id : 0;
                    filter_level = pbi->mt_baseline_filter_level[Segment];
                    /* Distance of Mb to the various image edges.
                     * These are specified to 8th pel as they are always compared to values that are in 1/8th pel units
                     * Apply any context driven MB level adjustment
                     */
                    vp8_adjust_mb_lf_value(xd, &filter_level);
                }

                /* Distance of Mb to the various image edges.
                 * These are specified to 8th pel as they are always compared to values that are in 1/8th pel units
                 */
                xd->mb_to_left_edge = -((mb_col * 16) << 3);
                xd->mb_to_right_edge = ((pc->mb_cols - 1 - mb_col) * 16) << 3;

                xd->dst.y_buffer = pc->yv12_fb[dst_fb_idx].y_buffer + recon_yoffset;
                xd->dst.u_buffer = pc->yv12_fb[dst_fb_idx].u_buffer + recon_uvoffset;
                xd->dst.v_buffer = pc->yv12_fb[dst_fb_idx].v_buffer + recon_uvoffset;

                xd->left_available = (mb_col != 0);

                /* Select the appropriate reference frame for this MB */
                if (xd->mode_info_context->mbmi.ref_frame == LAST_FRAME)
                    ref_fb_idx = pc->lst_fb_idx;
                else if (xd->mode_info_context->mbmi.ref_frame == GOLDEN_FRAME)
                    ref_fb_idx = pc->gld_fb_idx;
                else
                    ref_fb_idx = pc->alt_fb_idx;

                xd->pre.y_buffer = pc->yv12_fb[ref_fb_idx].y_buffer + recon_yoffset;
                xd->pre.u_buffer = pc->yv12_fb[ref_fb_idx].u_buffer + recon_uvoffset;
                xd->pre.v_buffer = pc->yv12_fb[ref_fb_idx].v_buffer + recon_uvoffset;

                vp8_build_uvmvs(xd, pc->full_pixel);
                vp8mt_decode_macroblock(pbi, xd, mb_row, mb_col);

                if (pbi->common.filter_level)
                {
                    /* Save decoded MB last row data for next-row decoding */
                    if(mb_row != pc->mb_rows-1)
                    {
                        vpx_memcpy((pbi->mt_yabove_row[mb_row +1] + 32 + mb_col*16), (xd->dst.y_buffer + 15 * recon_y_stride), 16);
                        vpx_memcpy((pbi->mt_uabove_row[mb_row +1] + 16 + mb_col*8), (xd->dst.u_buffer + 7 * recon_uv_stride), 8);
                        vpx_memcpy((pbi->mt_vabove_row[mb_row +1] + 16 + mb_col*8), (xd->dst.v_buffer + 7 * recon_uv_stride), 8);
                    }

                    /* save left_col for next MB decoding */
                    if(mb_col != pc->mb_cols-1)
                    {
                        MODE_INFO *next = xd->mode_info_context +1;

                        if (xd->frame_type == KEY_FRAME  ||  next->mbmi.ref_frame == INTRA_FRAME)
                        {
                            for (i = 0; i < 16; i++)
                                pbi->mt_yleft_col[mb_row][i] = xd->dst.y_buffer [i* recon_y_stride + 15];
                            for (i = 0; i < 8; i++)
                            {
                                pbi->mt_uleft_col[mb_row][i] = xd->dst.u_buffer [i* recon_uv_stride + 7];
                                pbi->mt_vleft_col[mb_row][i] = xd->dst.v_buffer [i* recon_uv_stride + 7];
                            }
                        }
                    }

                    /* loopfilter on this macroblock. */
                    if (filter_level)
                    {
                        if (mb_col > 0)
                            pc->lf_mbv(xd->dst.y_buffer, xd->dst.u_buffer, xd->dst.v_buffer, recon_y_stride, recon_uv_stride, &lfi[filter_level], pc->simpler_lpf);

                        if (xd->mode_info_context->mbmi.dc_diff > 0)
                            pc->lf_bv(xd->dst.y_buffer, xd->dst.u_buffer, xd->dst.v_buffer, recon_y_stride, recon_uv_stride, &lfi[filter_level], pc->simpler_lpf);

                        /* don't apply across umv border */
                        if (mb_row > 0)
                            pc->lf_mbh(xd->dst.y_buffer, xd->dst.u_buffer, xd->dst.v_buffer, recon_y_stride, recon_uv_stride, &lfi[filter_level], pc->simpler_lpf);

                        if (xd->mode_info_context->mbmi.dc_diff > 0)
                            pc->lf_bh(xd->dst.y_buffer, xd->dst.u_buffer, xd->dst.v_buffer, recon_y_stride, recon_uv_stride, &lfi[filter_level], pc->simpler_lpf);
                    }
                }

                recon_yoffset += 16;
                recon_uvoffset += 8;

                ++xd->mode_info_context;  /* next mb */

                xd->above_context++;

                pbi->mt_current_mb_col[mb_row] = mb_col;
            }

            /* adjust to the next row of mbs */
            if (pbi->common.filter_level)
            {
                if(mb_row != pc->mb_rows-1)
                {
                    int lasty = pc->yv12_fb[ref_fb_idx].y_width + VP8BORDERINPIXELS;
                    int lastuv = (pc->yv12_fb[ref_fb_idx].y_width>>1) + (VP8BORDERINPIXELS>>1);

                    for (i = 0; i < 4; i++)
                    {
                        pbi->mt_yabove_row[mb_row +1][lasty + i] = pbi->mt_yabove_row[mb_row +1][lasty -1];
                        pbi->mt_uabove_row[mb_row +1][lastuv + i] = pbi->mt_uabove_row[mb_row +1][lastuv -1];
                        pbi->mt_vabove_row[mb_row +1][lastuv + i] = pbi->mt_vabove_row[mb_row +1][lastuv -1];
                    }
                }
            }else
                vp8_extend_mb_row(&pc->yv12_fb[dst_fb_idx], xd->dst.y_buffer + 16, xd->dst.u_buffer + 8, xd->dst.v_buffer + 8);

            ++xd->mode_info_context;      /* skip prediction column */
        }
        xd->mode_info_context += xd->mode_info_stride * pbi->decoding_thread_count;
    }

    sem_wait(&pbi->h_event_end_decoding);   /* add back for each frame */
#else
    (void) pbi;
    (void) xd;
#endif
}
