/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
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

#define MAX_ROWS 256

extern void vp8_decode_mb_row(VP8D_COMP *pbi,
                              VP8_COMMON *pc,
                              int mb_row,
                              MACROBLOCKD *xd);

extern void vp8_build_uvmvs(MACROBLOCKD *x, int fullpixel);
extern void vp8_decode_macroblock(VP8D_COMP *pbi, MACROBLOCKD *xd);

void vp8_thread_loop_filter(VP8D_COMP *pbi, MB_ROW_DEC *mbrd, int ithread);

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

        mbd->mode_info        = pc->mi - 1;
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

        mbd->current_bc = &pbi->bc2;

        for (j = 0; j < 25; j++)
        {
            mbd->block[j].dequant = xd->block[j].dequant;
        }
    }

    for (i=0; i< pc->mb_rows; i++)
        pbi->current_mb_col[i]=-1;
#else
    (void) pbi;
    (void) xd;
    (void) mbrd;
    (void) count;
#endif
}

void vp8_setup_loop_filter_thread_data(VP8D_COMP *pbi, MACROBLOCKD *xd, MB_ROW_DEC *mbrd, int count)
{
#if CONFIG_MULTITHREAD
    VP8_COMMON *const pc = & pbi->common;
    int i, j;

    for (i = 0; i < count; i++)
    {
        MACROBLOCKD *mbd = &mbrd[i].mbd;
//#if CONFIG_RUNTIME_CPU_DETECT
//        mbd->rtcd = xd->rtcd;
//#endif

        //mbd->subpixel_predict        = xd->subpixel_predict;
        //mbd->subpixel_predict8x4     = xd->subpixel_predict8x4;
        //mbd->subpixel_predict8x8     = xd->subpixel_predict8x8;
        //mbd->subpixel_predict16x16   = xd->subpixel_predict16x16;

        mbd->mode_info        = pc->mi - 1;
        mbd->mode_info_context = pc->mi   + pc->mode_info_stride * (i + 1);
        mbd->mode_info_stride  = pc->mode_info_stride;

        //mbd->frame_type = pc->frame_type;
        //mbd->frames_since_golden      = pc->frames_since_golden;
        //mbd->frames_till_alt_ref_frame  = pc->frames_till_alt_ref_frame;

        //mbd->pre = pc->yv12_fb[pc->lst_fb_idx];
        //mbd->dst = pc->yv12_fb[pc->new_fb_idx];

        //vp8_setup_block_dptrs(mbd);
        //vp8_build_block_doffsets(mbd);
        mbd->segmentation_enabled    = xd->segmentation_enabled;  //
        mbd->mb_segement_abs_delta     = xd->mb_segement_abs_delta;  //
        vpx_memcpy(mbd->segment_feature_data, xd->segment_feature_data, sizeof(xd->segment_feature_data));   //

        //signed char ref_lf_deltas[MAX_REF_LF_DELTAS];
        vpx_memcpy(mbd->ref_lf_deltas, xd->ref_lf_deltas, sizeof(xd->ref_lf_deltas));
        //signed char mode_lf_deltas[MAX_MODE_LF_DELTAS];
        vpx_memcpy(mbd->mode_lf_deltas, xd->mode_lf_deltas, sizeof(xd->mode_lf_deltas));
        //unsigned char mode_ref_lf_delta_enabled;
        //unsigned char mode_ref_lf_delta_update;
        mbd->mode_ref_lf_delta_enabled    = xd->mode_ref_lf_delta_enabled;
        mbd->mode_ref_lf_delta_update    = xd->mode_ref_lf_delta_update;

        //mbd->mbmi.mode = DC_PRED;
        //mbd->mbmi.uv_mode = DC_PRED;
        //mbd->current_bc = &pbi->bc2;

        //for (j = 0; j < 25; j++)
        //{
        //    mbd->block[j].dequant = xd->block[j].dequant;
        //}
    }

    for (i=0; i< pc->mb_rows; i++)
        pbi->current_mb_col[i]=-1;
#else
    (void) pbi;
    (void) xd;
    (void) mbrd;
    (void) count;
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
        int current_filter_level = 0;

        if (pbi->b_multithreaded_rd == 0)
            break;

        //if(WaitForSingleObject(pbi->h_event_start_decoding[ithread], INFINITE) == WAIT_OBJECT_0)
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

                for (mb_row = ithread+1; mb_row < pc->mb_rows; mb_row += (pbi->decoding_thread_count + 1))
                {
                    int i;
                    int recon_yoffset, recon_uvoffset;
                    int mb_col;
                    int ref_fb_idx = pc->lst_fb_idx;
                    int dst_fb_idx = pc->new_fb_idx;
                    int recon_y_stride = pc->yv12_fb[ref_fb_idx].y_stride;
                    int recon_uv_stride = pc->yv12_fb[ref_fb_idx].uv_stride;

                    pbi->mb_row_di[ithread].mb_row = mb_row;
                    pbi->mb_row_di[ithread].mbd.current_bc =  &pbi->mbc[mb_row%num_part];

                    last_row_current_mb_col = &pbi->current_mb_col[mb_row -1];

                    recon_yoffset = mb_row * recon_y_stride * 16;
                    recon_uvoffset = mb_row * recon_uv_stride * 8;
                    // reset above block coeffs

                    xd->above_context = pc->above_context;
                    xd->left_context = &mb_row_left_context;
                    vpx_memset(&mb_row_left_context, 0, sizeof(mb_row_left_context));
                    xd->up_available = (mb_row != 0);

                    xd->mb_to_top_edge = -((mb_row * 16)) << 3;
                    xd->mb_to_bottom_edge = ((pc->mb_rows - 1 - mb_row) * 16) << 3;

                    for (mb_col = 0; mb_col < pc->mb_cols; mb_col++)
                    {
                        if ((mb_col & 7) == 0)
                        {
                            while (mb_col > (*last_row_current_mb_col - 8) && *last_row_current_mb_col != pc->mb_cols - 1)
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

                        // Distance of Mb to the various image edges.
                        // These specified to 8th pel as they are always compared to values that are in 1/8th pel units
                        xd->mb_to_left_edge = -((mb_col * 16) << 3);
                        xd->mb_to_right_edge = ((pc->mb_cols - 1 - mb_col) * 16) << 3;

                        xd->dst.y_buffer = pc->yv12_fb[dst_fb_idx].y_buffer + recon_yoffset;
                        xd->dst.u_buffer = pc->yv12_fb[dst_fb_idx].u_buffer + recon_uvoffset;
                        xd->dst.v_buffer = pc->yv12_fb[dst_fb_idx].v_buffer + recon_uvoffset;

                        xd->left_available = (mb_col != 0);

                        // Select the appropriate reference frame for this MB
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

                        vp8_decode_macroblock(pbi, xd);

                        recon_yoffset += 16;
                        recon_uvoffset += 8;

                        ++xd->mode_info_context;  /* next mb */

                        xd->above_context++;

                        //pbi->mb_row_di[ithread].current_mb_col = mb_col;
                        pbi->current_mb_col[mb_row] = mb_col;
                    }

                    // adjust to the next row of mbs
                    vp8_extend_mb_row(
                    &pc->yv12_fb[dst_fb_idx],
                    xd->dst.y_buffer + 16, xd->dst.u_buffer + 8, xd->dst.v_buffer + 8
                    );

                    ++xd->mode_info_context;      /* skip prediction column */

                    // since we have multithread
                    xd->mode_info_context += xd->mode_info_stride * pbi->decoding_thread_count;

                    pbi->last_mb_row_decoded = mb_row;

                }
            }
        }

        // If |pbi->common.filter_level| is 0 the value can change in-between
        // the sem_post and the check to call vp8_thread_loop_filter.
        current_filter_level = pbi->common.filter_level;

        //  add this to each frame
        if ((mbrd->mb_row == pbi->common.mb_rows-1) || ((mbrd->mb_row == pbi->common.mb_rows-2) && (pbi->common.mb_rows % (pbi->decoding_thread_count+1))==1))
        {
            //SetEvent(pbi->h_event_end_decoding);
            sem_post(&pbi->h_event_end_decoding);
        }

        if ((pbi->b_multithreaded_lf) && (current_filter_level))
            vp8_thread_loop_filter(pbi, mbrd, ithread);

    }
#else
    (void) p_data;
#endif

    return 0 ;
}


void vp8_thread_loop_filter(VP8D_COMP *pbi, MB_ROW_DEC *mbrd, int ithread)
{
#if CONFIG_MULTITHREAD

        if (sem_wait(&pbi->h_event_start_lpf[ithread]) == 0)
        {
           // if (pbi->b_multithreaded_lf == 0) // we're shutting down      ????
           //     break;
           // else
            {
                VP8_COMMON *cm  = &pbi->common;
                MACROBLOCKD *mbd = &mbrd->mbd;
                int default_filt_lvl = pbi->common.filter_level;

                YV12_BUFFER_CONFIG *post = cm->frame_to_show;
                loop_filter_info *lfi = cm->lf_info;
                //int frame_type = cm->frame_type;

                int mb_row;
                int mb_col;

                int filter_level;
                int alt_flt_enabled = mbd->segmentation_enabled;

                int i;
                unsigned char *y_ptr, *u_ptr, *v_ptr;

                volatile int *last_row_current_mb_col;

                // Set up the buffer pointers
                y_ptr = post->y_buffer + post->y_stride  * 16 * (ithread +1);
                u_ptr = post->u_buffer + post->uv_stride *  8 * (ithread +1);
                v_ptr = post->v_buffer + post->uv_stride *  8 * (ithread +1);

                // vp8_filter each macro block
                for (mb_row = ithread+1; mb_row < cm->mb_rows; mb_row+= (pbi->decoding_thread_count + 1))
                {
                    last_row_current_mb_col = &pbi->current_mb_col[mb_row -1];

                    for (mb_col = 0; mb_col < cm->mb_cols; mb_col++)
                    {
                        int Segment = (alt_flt_enabled) ? mbd->mode_info_context->mbmi.segment_id : 0;

                        if ((mb_col & 7) == 0)
                        {
                            while (mb_col > (*last_row_current_mb_col-8) && *last_row_current_mb_col != cm->mb_cols - 1)
                            {
                                x86_pause_hint();
                                thread_sleep(0);
                            }
                        }

                        filter_level = pbi->mt_baseline_filter_level[Segment];

                        // Apply any context driven MB level adjustment
                        vp8_adjust_mb_lf_value(mbd, &filter_level);

                        if (filter_level)
                        {
                            if (mb_col > 0)
                                cm->lf_mbv(y_ptr, u_ptr, v_ptr, post->y_stride, post->uv_stride, &lfi[filter_level], cm->simpler_lpf);

                            if (mbd->mode_info_context->mbmi.dc_diff > 0)
                                cm->lf_bv(y_ptr, u_ptr, v_ptr, post->y_stride, post->uv_stride, &lfi[filter_level], cm->simpler_lpf);

                            // don't apply across umv border
                            if (mb_row > 0)
                                cm->lf_mbh(y_ptr, u_ptr, v_ptr, post->y_stride, post->uv_stride, &lfi[filter_level], cm->simpler_lpf);

                            if (mbd->mode_info_context->mbmi.dc_diff > 0)
                                cm->lf_bh(y_ptr, u_ptr, v_ptr, post->y_stride, post->uv_stride, &lfi[filter_level], cm->simpler_lpf);
                        }

                        y_ptr += 16;
                        u_ptr += 8;
                        v_ptr += 8;

                        mbd->mode_info_context++;     // step to next MB
                        pbi->current_mb_col[mb_row] = mb_col;
                    }

                    mbd->mode_info_context++;         // Skip border mb

                    y_ptr += post->y_stride  * 16 * (pbi->decoding_thread_count + 1) - post->y_width;
                    u_ptr += post->uv_stride *  8 * (pbi->decoding_thread_count + 1) - post->uv_width;
                    v_ptr += post->uv_stride *  8 * (pbi->decoding_thread_count + 1) - post->uv_width;

                    mbd->mode_info_context += pbi->decoding_thread_count * mbd->mode_info_stride;         // Skip border mb
                }
            }
        }

        //  add this to each frame
        if ((mbrd->mb_row == pbi->common.mb_rows-1) || ((mbrd->mb_row == pbi->common.mb_rows-2) && (pbi->common.mb_rows % (pbi->decoding_thread_count+1))==1))
        {
          sem_post(&pbi->h_event_end_lpf);
        }
#else
    (void) pbi;
#endif
}

void vp8_decoder_create_threads(VP8D_COMP *pbi)
{
#if CONFIG_MULTITHREAD
    int core_count = 0;
    int ithread;

    pbi->b_multithreaded_rd = 0;
    pbi->b_multithreaded_lf = 0;
    pbi->allocated_decoding_thread_count = 0;
    core_count = (pbi->max_threads > 16) ? 16 : pbi->max_threads;

    if (core_count > 1)
    {
        pbi->b_multithreaded_rd = 1;
        pbi->b_multithreaded_lf = 1;  // this can be merged with pbi->b_multithreaded_rd ?
        pbi->decoding_thread_count = core_count -1;

        CHECK_MEM_ERROR(pbi->h_decoding_thread, vpx_malloc(sizeof(pthread_t) * pbi->decoding_thread_count));
        CHECK_MEM_ERROR(pbi->h_event_start_decoding, vpx_malloc(sizeof(sem_t) * pbi->decoding_thread_count));
        CHECK_MEM_ERROR(pbi->mb_row_di, vpx_memalign(32, sizeof(MB_ROW_DEC) * pbi->decoding_thread_count));
        vpx_memset(pbi->mb_row_di, 0, sizeof(MB_ROW_DEC) * pbi->decoding_thread_count);
        CHECK_MEM_ERROR(pbi->de_thread_data, vpx_malloc(sizeof(DECODETHREAD_DATA) * pbi->decoding_thread_count));

        CHECK_MEM_ERROR(pbi->current_mb_col, vpx_malloc(sizeof(int) * MAX_ROWS));  // pc->mb_rows));
        CHECK_MEM_ERROR(pbi->h_event_start_lpf, vpx_malloc(sizeof(sem_t) * pbi->decoding_thread_count));

        for (ithread = 0; ithread < pbi->decoding_thread_count; ithread++)
        {
            sem_init(&pbi->h_event_start_decoding[ithread], 0, 0);
            sem_init(&pbi->h_event_start_lpf[ithread], 0, 0);

            pbi->de_thread_data[ithread].ithread  = ithread;
            pbi->de_thread_data[ithread].ptr1     = (void *)pbi;
            pbi->de_thread_data[ithread].ptr2     = (void *) &pbi->mb_row_di[ithread];

            pthread_create(&pbi->h_decoding_thread[ithread], 0, vp8_thread_decoding_proc, (&pbi->de_thread_data[ithread]));
        }

        sem_init(&pbi->h_event_end_decoding, 0, 0);
        sem_init(&pbi->h_event_end_lpf, 0, 0);

        pbi->allocated_decoding_thread_count = pbi->decoding_thread_count;
    }

#else
    (void) pbi;
#endif
}

void vp8_decoder_remove_threads(VP8D_COMP *pbi)
{
#if CONFIG_MULTITHREAD

    if (pbi->b_multithreaded_lf)
    {
        int i;
        pbi->b_multithreaded_lf = 0;

        for (i = 0; i < pbi->allocated_decoding_thread_count; i++)
            sem_destroy(&pbi->h_event_start_lpf[i]);

        sem_destroy(&pbi->h_event_end_lpf);
    }

    //shutdown MB Decoding thread;
    if (pbi->b_multithreaded_rd)
    {
        int i;

        pbi->b_multithreaded_rd = 0;

        // allow all threads to exit
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

        if (pbi->h_event_start_lpf)
        {
            vpx_free(pbi->h_event_start_lpf);
            pbi->h_event_start_lpf = NULL;
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

        if (pbi->current_mb_col)
        {
            vpx_free(pbi->current_mb_col);
            pbi->current_mb_col = NULL ;
        }
    }
#else
    (void) pbi;
#endif
}


void vp8_start_lfthread(VP8D_COMP *pbi)
{
#if CONFIG_MULTITHREAD
  /*
    memcpy(&pbi->lpfmb, &pbi->mb, sizeof(pbi->mb));
    pbi->last_mb_row_decoded = 0;
    sem_post(&pbi->h_event_start_lpf);
    */
    (void) pbi;
#else
    (void) pbi;
#endif
}

void vp8_stop_lfthread(VP8D_COMP *pbi)
{
#if CONFIG_MULTITHREAD
  /*
    struct vpx_usec_timer timer;

    vpx_usec_timer_start(&timer);

    sem_wait(&pbi->h_event_end_lpf);

    vpx_usec_timer_mark(&timer);
    pbi->time_loop_filtering += vpx_usec_timer_elapsed(&timer);
    */
    (void) pbi;
#else
    (void) pbi;
#endif
}


void vp8_mtdecode_mb_rows(VP8D_COMP *pbi,
                          MACROBLOCKD *xd)
{
#if CONFIG_MULTITHREAD
    int mb_row;
    VP8_COMMON *pc = &pbi->common;

    int ibc = 0;
    int num_part = 1 << pbi->common.multi_token_partition;
    int i;
    volatile int *last_row_current_mb_col = NULL;

    vp8_setup_decoding_thread_data(pbi, xd, pbi->mb_row_di, pbi->decoding_thread_count);

    for (i = 0; i < pbi->decoding_thread_count; i++)
        sem_post(&pbi->h_event_start_decoding[i]);

    for (mb_row = 0; mb_row < pc->mb_rows; mb_row += (pbi->decoding_thread_count + 1))
    {
        int i;

        xd->current_bc = &pbi->mbc[mb_row%num_part];

        //vp8_decode_mb_row(pbi, pc, mb_row, xd);
        {
            int i;
            int recon_yoffset, recon_uvoffset;
            int mb_col;
            int ref_fb_idx = pc->lst_fb_idx;
            int dst_fb_idx = pc->new_fb_idx;
            int recon_y_stride = pc->yv12_fb[ref_fb_idx].y_stride;
            int recon_uv_stride = pc->yv12_fb[ref_fb_idx].uv_stride;

           // volatile int *last_row_current_mb_col = NULL;
            if (mb_row > 0)
                last_row_current_mb_col = &pbi->current_mb_col[mb_row -1];

            vpx_memset(&pc->left_context, 0, sizeof(pc->left_context));
            recon_yoffset = mb_row * recon_y_stride * 16;
            recon_uvoffset = mb_row * recon_uv_stride * 8;
            // reset above block coeffs

            xd->above_context = pc->above_context;
            xd->up_available = (mb_row != 0);

            xd->mb_to_top_edge = -((mb_row * 16)) << 3;
            xd->mb_to_bottom_edge = ((pc->mb_rows - 1 - mb_row) * 16) << 3;

            for (mb_col = 0; mb_col < pc->mb_cols; mb_col++)
            {
                if ( mb_row > 0 && (mb_col & 7) == 0){
                    while (mb_col > (*last_row_current_mb_col - 8) && *last_row_current_mb_col != pc->mb_cols - 1)
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

                // Distance of Mb to the various image edges.
                // These specified to 8th pel as they are always compared to values that are in 1/8th pel units
                xd->mb_to_left_edge = -((mb_col * 16) << 3);
                xd->mb_to_right_edge = ((pc->mb_cols - 1 - mb_col) * 16) << 3;

                xd->dst.y_buffer = pc->yv12_fb[dst_fb_idx].y_buffer + recon_yoffset;
                xd->dst.u_buffer = pc->yv12_fb[dst_fb_idx].u_buffer + recon_uvoffset;
                xd->dst.v_buffer = pc->yv12_fb[dst_fb_idx].v_buffer + recon_uvoffset;

                xd->left_available = (mb_col != 0);

                // Select the appropriate reference frame for this MB
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

                vp8_decode_macroblock(pbi, xd);

                recon_yoffset += 16;
                recon_uvoffset += 8;

                ++xd->mode_info_context;  /* next mb */

                xd->above_context++;

                //pbi->current_mb_col_main = mb_col;
                pbi->current_mb_col[mb_row] = mb_col;
            }

            // adjust to the next row of mbs
            vp8_extend_mb_row(
                &pc->yv12_fb[dst_fb_idx],
                xd->dst.y_buffer + 16, xd->dst.u_buffer + 8, xd->dst.v_buffer + 8
            );

            ++xd->mode_info_context;      /* skip prediction column */

            pbi->last_mb_row_decoded = mb_row;
        }
        xd->mode_info_context += xd->mode_info_stride * pbi->decoding_thread_count;
    }

    sem_wait(&pbi->h_event_end_decoding);   // add back for each frame
#else
    (void) pbi;
    (void) xd;
#endif
}


void vp8_mt_loop_filter_frame( VP8D_COMP *pbi)
{
#if CONFIG_MULTITHREAD
    VP8_COMMON *cm  = &pbi->common;
    MACROBLOCKD *mbd = &pbi->mb;
    int default_filt_lvl = pbi->common.filter_level;

    YV12_BUFFER_CONFIG *post = cm->frame_to_show;
    loop_filter_info *lfi = cm->lf_info;
    int frame_type = cm->frame_type;

    int mb_row;
    int mb_col;

    int filter_level;
    int alt_flt_enabled = mbd->segmentation_enabled;

    int i;
    unsigned char *y_ptr, *u_ptr, *v_ptr;

    volatile int *last_row_current_mb_col=NULL;

    vp8_setup_loop_filter_thread_data(pbi, mbd, pbi->mb_row_di, pbi->decoding_thread_count);

    mbd->mode_info_context = cm->mi;          // Point at base of Mb MODE_INFO list

    // Note the baseline filter values for each segment
    if (alt_flt_enabled)
    {
        for (i = 0; i < MAX_MB_SEGMENTS; i++)
        {
            // Abs value
            if (mbd->mb_segement_abs_delta == SEGMENT_ABSDATA)
                pbi->mt_baseline_filter_level[i] = mbd->segment_feature_data[MB_LVL_ALT_LF][i];
            // Delta Value
            else
            {
                pbi->mt_baseline_filter_level[i] = default_filt_lvl + mbd->segment_feature_data[MB_LVL_ALT_LF][i];
                pbi->mt_baseline_filter_level[i] = (pbi->mt_baseline_filter_level[i] >= 0) ? ((pbi->mt_baseline_filter_level[i] <= MAX_LOOP_FILTER) ? pbi->mt_baseline_filter_level[i] : MAX_LOOP_FILTER) : 0;  // Clamp to valid range
            }
        }
    }
    else
    {
        for (i = 0; i < MAX_MB_SEGMENTS; i++)
            pbi->mt_baseline_filter_level[i] = default_filt_lvl;
    }

    // Initialize the loop filter for this frame.
    if ((cm->last_filter_type != cm->filter_type) || (cm->last_sharpness_level != cm->sharpness_level))
        vp8_init_loop_filter(cm);
    else if (frame_type != cm->last_frame_type)
        vp8_frame_init_loop_filter(lfi, frame_type);

    for (i = 0; i < pbi->decoding_thread_count; i++)
        sem_post(&pbi->h_event_start_lpf[i]);
        // sem_post(&pbi->h_event_start_lpf);

    // Set up the buffer pointers
    y_ptr = post->y_buffer;
    u_ptr = post->u_buffer;
    v_ptr = post->v_buffer;

    // vp8_filter each macro block
    for (mb_row = 0; mb_row < cm->mb_rows; mb_row+= (pbi->decoding_thread_count + 1))
    {
        if (mb_row > 0)
            last_row_current_mb_col = &pbi->current_mb_col[mb_row -1];

        for (mb_col = 0; mb_col < cm->mb_cols; mb_col++)
        {
            int Segment = (alt_flt_enabled) ? mbd->mode_info_context->mbmi.segment_id : 0;

            if ( mb_row > 0 && (mb_col & 7) == 0){
            // if ( mb_row > 0 ){
                while (mb_col > (*last_row_current_mb_col-8) && *last_row_current_mb_col != cm->mb_cols - 1)
                {
                    x86_pause_hint();
                    thread_sleep(0);
                }
            }

            filter_level = pbi->mt_baseline_filter_level[Segment];

            // Distance of Mb to the various image edges.
            // These specified to 8th pel as they are always compared to values that are in 1/8th pel units
            // Apply any context driven MB level adjustment
            vp8_adjust_mb_lf_value(mbd, &filter_level);

            if (filter_level)
            {
                if (mb_col > 0)
                    cm->lf_mbv(y_ptr, u_ptr, v_ptr, post->y_stride, post->uv_stride, &lfi[filter_level], cm->simpler_lpf);

                if (mbd->mode_info_context->mbmi.dc_diff > 0)
                    cm->lf_bv(y_ptr, u_ptr, v_ptr, post->y_stride, post->uv_stride, &lfi[filter_level], cm->simpler_lpf);

                // don't apply across umv border
                if (mb_row > 0)
                    cm->lf_mbh(y_ptr, u_ptr, v_ptr, post->y_stride, post->uv_stride, &lfi[filter_level], cm->simpler_lpf);

                if (mbd->mode_info_context->mbmi.dc_diff > 0)
                    cm->lf_bh(y_ptr, u_ptr, v_ptr, post->y_stride, post->uv_stride, &lfi[filter_level], cm->simpler_lpf);
            }

            y_ptr += 16;
            u_ptr += 8;
            v_ptr += 8;

            mbd->mode_info_context++;     // step to next MB
            pbi->current_mb_col[mb_row] = mb_col;
        }
        mbd->mode_info_context++;         // Skip border mb

        //update for multi-thread
        y_ptr += post->y_stride  * 16 * (pbi->decoding_thread_count + 1) - post->y_width;
        u_ptr += post->uv_stride *  8 * (pbi->decoding_thread_count + 1) - post->uv_width;
        v_ptr += post->uv_stride *  8 * (pbi->decoding_thread_count + 1) - post->uv_width;
        mbd->mode_info_context += pbi->decoding_thread_count * mbd->mode_info_stride;
    }

    sem_wait(&pbi->h_event_end_lpf);
#else
    (void) pbi;
#endif
}
