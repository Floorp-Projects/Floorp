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
#include "onyxd_int.h"
#include "vpx_mem/vpx_mem.h"
#include "threading.h"

#include "loopfilter.h"
#include "extend.h"
#include "vpx_ports/vpx_timer.h"

extern void vp8_decode_mb_row(VP8D_COMP *pbi,
                              VP8_COMMON *pc,
                              int mb_row,
                              MACROBLOCKD *xd);

extern void vp8_build_uvmvs(MACROBLOCKD *x, int fullpixel);
extern void vp8_decode_macroblock(VP8D_COMP *pbi, MACROBLOCKD *xd);

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
        mbd->gf_active_ptr            = xd->gf_active_ptr;

        mbd->mode_info        = pc->mi - 1;
        mbd->mode_info_context = pc->mi   + pc->mode_info_stride * (i + 1);
        mbd->mode_info_stride  = pc->mode_info_stride;

        mbd->frame_type = pc->frame_type;
        mbd->frames_since_golden      = pc->frames_since_golden;
        mbd->frames_till_alt_ref_frame  = pc->frames_till_alt_ref_frame;

        mbd->pre = pc->last_frame;
        mbd->dst = pc->new_frame;




        vp8_setup_block_dptrs(mbd);
        vp8_build_block_doffsets(mbd);
        mbd->segmentation_enabled    = xd->segmentation_enabled;
        mbd->mb_segement_abs_delta     = xd->mb_segement_abs_delta;
        vpx_memcpy(mbd->segment_feature_data, xd->segment_feature_data, sizeof(xd->segment_feature_data));

        mbd->mbmi.mode = DC_PRED;
        mbd->mbmi.uv_mode = DC_PRED;

        mbd->current_bc = &pbi->bc2;

        for (j = 0; j < 25; j++)
        {
            mbd->block[j].dequant = xd->block[j].dequant;
        }
    }

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
    ENTROPY_CONTEXT mb_row_left_context[4][4];

    while (1)
    {
        if (pbi->b_multithreaded_rd == 0)
            break;

        //if(WaitForSingleObject(pbi->h_event_mbrdecoding[ithread], INFINITE) == WAIT_OBJECT_0)
        if (sem_wait(&pbi->h_event_mbrdecoding[ithread]) == 0)
        {
            if (pbi->b_multithreaded_rd == 0)
                break;
            else
            {
                VP8_COMMON *pc = &pbi->common;
                int mb_row       = mbrd->mb_row;
                MACROBLOCKD *xd = &mbrd->mbd;

                //printf("ithread:%d mb_row %d\n", ithread, mb_row);
                int i;
                int recon_yoffset, recon_uvoffset;
                int mb_col;
                int recon_y_stride = pc->last_frame.y_stride;
                int recon_uv_stride = pc->last_frame.uv_stride;

                volatile int *last_row_current_mb_col;

                if (ithread > 0)
                    last_row_current_mb_col = &pbi->mb_row_di[ithread-1].current_mb_col;
                else
                    last_row_current_mb_col = &pbi->current_mb_col_main;

                recon_yoffset = mb_row * recon_y_stride * 16;
                recon_uvoffset = mb_row * recon_uv_stride * 8;
                // reset above block coeffs

                xd->above_context[Y1CONTEXT] = pc->above_context[Y1CONTEXT];
                xd->above_context[UCONTEXT ] = pc->above_context[UCONTEXT];
                xd->above_context[VCONTEXT ] = pc->above_context[VCONTEXT];
                xd->above_context[Y2CONTEXT] = pc->above_context[Y2CONTEXT];
                xd->left_context = mb_row_left_context;
                vpx_memset(mb_row_left_context, 0, sizeof(mb_row_left_context));
                xd->up_available = (mb_row != 0);

                xd->mb_to_top_edge = -((mb_row * 16)) << 3;
                xd->mb_to_bottom_edge = ((pc->mb_rows - 1 - mb_row) * 16) << 3;

                for (mb_col = 0; mb_col < pc->mb_cols; mb_col++)
                {

                    while (mb_col > (*last_row_current_mb_col - 1) && *last_row_current_mb_col != pc->mb_cols - 1)
                    {
                        x86_pause_hint();
                        thread_sleep(0);
                    }

                    // Take a copy of the mode and Mv information for this macroblock into the xd->mbmi
                    vpx_memcpy(&xd->mbmi, &xd->mode_info_context->mbmi, 32); //sizeof(MB_MODE_INFO) );

                    if (xd->mbmi.mode == SPLITMV || xd->mbmi.mode == B_PRED)
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

                    xd->dst.y_buffer = pc->new_frame.y_buffer + recon_yoffset;
                    xd->dst.u_buffer = pc->new_frame.u_buffer + recon_uvoffset;
                    xd->dst.v_buffer = pc->new_frame.v_buffer + recon_uvoffset;

                    xd->left_available = (mb_col != 0);

                    // Select the appropriate reference frame for this MB
                    if (xd->mbmi.ref_frame == LAST_FRAME)
                    {
                        xd->pre.y_buffer = pc->last_frame.y_buffer + recon_yoffset;
                        xd->pre.u_buffer = pc->last_frame.u_buffer + recon_uvoffset;
                        xd->pre.v_buffer = pc->last_frame.v_buffer + recon_uvoffset;
                    }
                    else if (xd->mbmi.ref_frame == GOLDEN_FRAME)
                    {
                        // Golden frame reconstruction buffer
                        xd->pre.y_buffer = pc->golden_frame.y_buffer + recon_yoffset;
                        xd->pre.u_buffer = pc->golden_frame.u_buffer + recon_uvoffset;
                        xd->pre.v_buffer = pc->golden_frame.v_buffer + recon_uvoffset;
                    }
                    else
                    {
                        // Alternate reference frame reconstruction buffer
                        xd->pre.y_buffer = pc->alt_ref_frame.y_buffer + recon_yoffset;
                        xd->pre.u_buffer = pc->alt_ref_frame.u_buffer + recon_uvoffset;
                        xd->pre.v_buffer = pc->alt_ref_frame.v_buffer + recon_uvoffset;
                    }

                    vp8_build_uvmvs(xd, pc->full_pixel);

                    vp8dx_bool_decoder_fill(xd->current_bc);
                    vp8_decode_macroblock(pbi, xd);


                    recon_yoffset += 16;
                    recon_uvoffset += 8;

                    ++xd->mode_info_context;  /* next mb */

                    xd->gf_active_ptr++;      // GF useage flag for next MB

                    xd->above_context[Y1CONTEXT] += 4;
                    xd->above_context[UCONTEXT ] += 2;
                    xd->above_context[VCONTEXT ] += 2;
                    xd->above_context[Y2CONTEXT] ++;
                    pbi->mb_row_di[ithread].current_mb_col = mb_col;

                }

                // adjust to the next row of mbs
                vp8_extend_mb_row(
                    &pc->new_frame,
                    xd->dst.y_buffer + 16, xd->dst.u_buffer + 8, xd->dst.v_buffer + 8
                );

                ++xd->mode_info_context;      /* skip prediction column */

                // since we have multithread
                xd->mode_info_context += xd->mode_info_stride * pbi->decoding_thread_count;

                //memcpy(&pbi->lpfmb, &pbi->mb, sizeof(pbi->mb));
                if ((mb_row & 1) == 1)
                {
                    pbi->last_mb_row_decoded = mb_row;
                    //printf("S%d", pbi->last_mb_row_decoded);
                }

                if (ithread == (pbi->decoding_thread_count - 1) || mb_row == pc->mb_rows - 1)
                {
                    //SetEvent(pbi->h_event_main);
                    sem_post(&pbi->h_event_main);

                }
            }
        }
    }

#else
    (void) p_data;
#endif

    return 0 ;
}

THREAD_FUNCTION vp8_thread_loop_filter(void *p_data)
{
#if CONFIG_MULTITHREAD
    VP8D_COMP *pbi = (VP8D_COMP *)p_data;

    while (1)
    {
        if (pbi->b_multithreaded_lf == 0)
            break;

        //printf("before waiting for start_lpf\n");

        //if(WaitForSingleObject(pbi->h_event_start_lpf, INFINITE) == WAIT_OBJECT_0)
        if (sem_wait(&pbi->h_event_start_lpf) == 0)
        {
            if (pbi->b_multithreaded_lf == 0) // we're shutting down
                break;
            else
            {

                VP8_COMMON *cm  = &pbi->common;
                MACROBLOCKD *mbd = &pbi->lpfmb;
                int default_filt_lvl = pbi->common.filter_level;

                YV12_BUFFER_CONFIG *post = &cm->new_frame;
                loop_filter_info *lfi = cm->lf_info;

                int mb_row;
                int mb_col;


                int baseline_filter_level[MAX_MB_SEGMENTS];
                int filter_level;
                int alt_flt_enabled = mbd->segmentation_enabled;

                int i;
                unsigned char *y_ptr, *u_ptr, *v_ptr;

                volatile int *last_mb_row_decoded = &pbi->last_mb_row_decoded;

                //MODE_INFO * this_mb_mode_info = cm->mi;
                mbd->mode_info_context = cm->mi;          // Point at base of Mb MODE_INFO list

                // Note the baseline filter values for each segment
                if (alt_flt_enabled)
                {
                    for (i = 0; i < MAX_MB_SEGMENTS; i++)
                    {
                        if (mbd->mb_segement_abs_delta == SEGMENT_ABSDATA)
                            baseline_filter_level[i] = mbd->segment_feature_data[MB_LVL_ALT_LF][i];
                        else
                        {
                            baseline_filter_level[i] = default_filt_lvl + mbd->segment_feature_data[MB_LVL_ALT_LF][i];
                            baseline_filter_level[i] = (baseline_filter_level[i] >= 0) ? ((baseline_filter_level[i] <= MAX_LOOP_FILTER) ? baseline_filter_level[i] : MAX_LOOP_FILTER) : 0;  // Clamp to valid range
                        }
                    }
                }
                else
                {
                    for (i = 0; i < MAX_MB_SEGMENTS; i++)
                        baseline_filter_level[i] = default_filt_lvl;
                }

                // Initialize the loop filter for this frame.
                vp8_init_loop_filter(cm);

                // Set up the buffer pointers
                y_ptr = post->y_buffer;
                u_ptr = post->u_buffer;
                v_ptr = post->v_buffer;

                // vp8_filter each macro block
                for (mb_row = 0; mb_row < cm->mb_rows; mb_row++)
                {

                    while (mb_row >= *last_mb_row_decoded)
                    {
                        x86_pause_hint();
                        thread_sleep(0);
                    }

                    //printf("R%d", mb_row);
                    for (mb_col = 0; mb_col < cm->mb_cols; mb_col++)
                    {
                        int Segment = (alt_flt_enabled) ? mbd->mode_info_context->mbmi.segment_id : 0;

                        filter_level = baseline_filter_level[Segment];

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

                    }

                    y_ptr += post->y_stride  * 16 - post->y_width;
                    u_ptr += post->uv_stride *  8 - post->uv_width;
                    v_ptr += post->uv_stride *  8 - post->uv_width;

                    mbd->mode_info_context++;         // Skip border mb
                }

                //printf("R%d\n", mb_row);
                // When done, signal main thread that ME is finished
                //SetEvent(pbi->h_event_lpf);
                sem_post(&pbi->h_event_lpf);
            }

        }
    }

#else
    (void) p_data;
#endif
    return 0;
}

void vp8_decoder_create_threads(VP8D_COMP *pbi)
{
#if CONFIG_MULTITHREAD
    int core_count = 0;
    int ithread;

    pbi->b_multithreaded_rd = 0;
    pbi->b_multithreaded_lf = 0;
    pbi->allocated_decoding_thread_count = 0;
    core_count = (pbi->max_threads > 16) ? 16 : pbi->max_threads; //vp8_get_proc_core_count();
    if (core_count > 1)
    {
        sem_init(&pbi->h_event_lpf, 0, 0);
        sem_init(&pbi->h_event_start_lpf, 0, 0);
        pbi->b_multithreaded_lf = 1;
        pthread_create(&pbi->h_thread_lpf, 0, vp8_thread_loop_filter, (pbi));
    }

    if (core_count > 1)
    {
        pbi->b_multithreaded_rd = 1;
        pbi->decoding_thread_count = core_count - 1;

        CHECK_MEM_ERROR(pbi->h_decoding_thread, vpx_malloc(sizeof(pthread_t) * pbi->decoding_thread_count));
        CHECK_MEM_ERROR(pbi->h_event_mbrdecoding, vpx_malloc(sizeof(sem_t) * pbi->decoding_thread_count));
        CHECK_MEM_ERROR(pbi->mb_row_di, vpx_memalign(32, sizeof(MB_ROW_DEC) * pbi->decoding_thread_count));
        vpx_memset(pbi->mb_row_di, 0, sizeof(MB_ROW_DEC) * pbi->decoding_thread_count);
        CHECK_MEM_ERROR(pbi->de_thread_data, vpx_malloc(sizeof(DECODETHREAD_DATA) * pbi->decoding_thread_count));

        for (ithread = 0; ithread < pbi->decoding_thread_count; ithread++)
        {
            sem_init(&pbi->h_event_mbrdecoding[ithread], 0, 0);

            pbi->de_thread_data[ithread].ithread  = ithread;
            pbi->de_thread_data[ithread].ptr1     = (void *)pbi;
            pbi->de_thread_data[ithread].ptr2     = (void *) &pbi->mb_row_di[ithread];

            pthread_create(&pbi->h_decoding_thread[ithread], 0, vp8_thread_decoding_proc, (&pbi->de_thread_data[ithread]));

        }

        sem_init(&pbi->h_event_main, 0, 0);
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
        pbi->b_multithreaded_lf = 0;
        sem_post(&pbi->h_event_start_lpf);
        pthread_join(pbi->h_thread_lpf, 0);
        sem_destroy(&pbi->h_event_start_lpf);
    }

    //shutdown MB Decoding thread;
    if (pbi->b_multithreaded_rd)
    {
        pbi->b_multithreaded_rd = 0;
        // allow all threads to exit
        {
            int i;

            for (i = 0; i < pbi->allocated_decoding_thread_count; i++)
            {

                sem_post(&pbi->h_event_mbrdecoding[i]);
                pthread_join(pbi->h_decoding_thread[i], NULL);
            }
        }
        {

            int i;
            for (i = 0; i < pbi->allocated_decoding_thread_count; i++)
            {
                sem_destroy(&pbi->h_event_mbrdecoding[i]);
            }


        }

        sem_destroy(&pbi->h_event_main);

        if (pbi->h_decoding_thread)
        {
            vpx_free(pbi->h_decoding_thread);
            pbi->h_decoding_thread = NULL;
        }

        if (pbi->h_event_mbrdecoding)
        {
            vpx_free(pbi->h_event_mbrdecoding);
            pbi->h_event_mbrdecoding = NULL;
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


void vp8_start_lfthread(VP8D_COMP *pbi)
{
#if CONFIG_MULTITHREAD
    memcpy(&pbi->lpfmb, &pbi->mb, sizeof(pbi->mb));
    pbi->last_mb_row_decoded = 0;
    sem_post(&pbi->h_event_start_lpf);
#else
    (void) pbi;
#endif
}

void vp8_stop_lfthread(VP8D_COMP *pbi)
{
#if CONFIG_MULTITHREAD
    struct vpx_usec_timer timer;

    vpx_usec_timer_start(&timer);

    sem_wait(&pbi->h_event_lpf);

    vpx_usec_timer_mark(&timer);
    pbi->time_loop_filtering += vpx_usec_timer_elapsed(&timer);
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

    vp8_setup_decoding_thread_data(pbi, xd, pbi->mb_row_di, pbi->decoding_thread_count);

    for (mb_row = 0; mb_row < pc->mb_rows; mb_row += (pbi->decoding_thread_count + 1))
    {
        int i;
        pbi->current_mb_col_main = -1;

        xd->current_bc = &pbi->mbc[ibc];
        ibc++ ;

        if (ibc == num_part)
            ibc = 0;

        for (i = 0; i < pbi->decoding_thread_count; i++)
        {
            if ((mb_row + i + 1) >= pc->mb_rows)
                break;

            pbi->mb_row_di[i].mb_row = mb_row + i + 1;
            pbi->mb_row_di[i].mbd.current_bc =  &pbi->mbc[ibc];
            ibc++;

            if (ibc == num_part)
                ibc = 0;

            pbi->mb_row_di[i].current_mb_col = -1;
            sem_post(&pbi->h_event_mbrdecoding[i]);
        }

        vp8_decode_mb_row(pbi, pc, mb_row, xd);

        xd->mode_info_context += xd->mode_info_stride * pbi->decoding_thread_count;

        if (mb_row < pc->mb_rows - 1)
        {
            sem_wait(&pbi->h_event_main);
        }
    }

    pbi->last_mb_row_decoded = mb_row;
#else
    (void) pbi;
    (void) xd;
#endif
}
