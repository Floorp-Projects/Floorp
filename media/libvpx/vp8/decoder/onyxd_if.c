/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "onyxc_int.h"
#if CONFIG_POSTPROC
#include "postproc.h"
#endif
#include "onyxd.h"
#include "onyxd_int.h"
#include "vpx_mem/vpx_mem.h"
#include "alloccommon.h"
#include "vpx_scale/yv12extend.h"
#include "loopfilter.h"
#include "swapyv12buffer.h"
#include "g_common.h"
#include "threading.h"
#include "decoderthreading.h"
#include <stdio.h>

#include "quant_common.h"
#include "vpx_scale/vpxscale.h"
#include "systemdependent.h"
#include "vpx_ports/vpx_timer.h"
#include "detokenize.h"

extern void vp8_init_loop_filter(VP8_COMMON *cm);
extern void vp8cx_init_de_quantizer(VP8D_COMP *pbi);

#if CONFIG_DEBUG
void vp8_recon_write_yuv_frame(unsigned char *name, YV12_BUFFER_CONFIG *s)
{
    FILE *yuv_file = fopen((char *)name, "ab");
    unsigned char *src = s->y_buffer;
    int h = s->y_height;

    do
    {
        fwrite(src, s->y_width, 1,  yuv_file);
        src += s->y_stride;
    }
    while (--h);

    src = s->u_buffer;
    h = s->uv_height;

    do
    {
        fwrite(src, s->uv_width, 1,  yuv_file);
        src += s->uv_stride;
    }
    while (--h);

    src = s->v_buffer;
    h = s->uv_height;

    do
    {
        fwrite(src, s->uv_width, 1, yuv_file);
        src += s->uv_stride;
    }
    while (--h);

    fclose(yuv_file);
}
#endif

void vp8dx_initialize()
{
    static int init_done = 0;

    if (!init_done)
    {
        vp8_initialize_common();
        vp8_scale_machine_specific_config();
        init_done = 1;
    }
}


VP8D_PTR vp8dx_create_decompressor(VP8D_CONFIG *oxcf)
{
    VP8D_COMP *pbi = vpx_memalign(32, sizeof(VP8D_COMP));

    if (!pbi)
        return NULL;

    vpx_memset(pbi, 0, sizeof(VP8D_COMP));

    if (setjmp(pbi->common.error.jmp))
    {
        pbi->common.error.setjmp = 0;
        vp8dx_remove_decompressor(pbi);
        return 0;
    }

    pbi->common.error.setjmp = 1;
    vp8dx_initialize();

    vp8_create_common(&pbi->common);
    vp8_dmachine_specific_config(pbi);

    pbi->common.current_video_frame = 0;
    pbi->ready_for_new_data = 1;

    pbi->CPUFreq = 0; //vp8_get_processor_freq();
    pbi->max_threads = oxcf->max_threads;
    vp8_decoder_create_threads(pbi);

    //vp8cx_init_de_quantizer() is first called here. Add check in frame_init_dequantizer() to avoid
    // unnecessary calling of vp8cx_init_de_quantizer() for every frame.
    vp8cx_init_de_quantizer(pbi);

    {
        VP8_COMMON *cm = &pbi->common;

        vp8_init_loop_filter(cm);
        cm->last_frame_type = KEY_FRAME;
        cm->last_filter_type = cm->filter_type;
        cm->last_sharpness_level = cm->sharpness_level;
    }

#if CONFIG_ARM_ASM_DETOK
    vp8_init_detokenizer(pbi);
#endif
    pbi->common.error.setjmp = 0;
    return (VP8D_PTR) pbi;
}


void vp8dx_remove_decompressor(VP8D_PTR ptr)
{
    VP8D_COMP *pbi = (VP8D_COMP *) ptr;

    if (!pbi)
        return;

    vp8_decoder_remove_threads(pbi);
    vp8_remove_common(&pbi->common);
    vpx_free(pbi);
}


void vp8dx_set_setting(VP8D_PTR comp, VP8D_SETTING oxst, int x)
{
    VP8D_COMP *pbi = (VP8D_COMP *) comp;

    (void) pbi;
    (void) x;

    switch (oxst)
    {
    case VP8D_OK:
        break;
    }
}

int vp8dx_get_setting(VP8D_PTR comp, VP8D_SETTING oxst)
{
    VP8D_COMP *pbi = (VP8D_COMP *) comp;

    (void) pbi;

    switch (oxst)
    {
    case VP8D_OK:
        break;
    }

    return -1;
}

int vp8dx_get_reference(VP8D_PTR ptr, VP8_REFFRAME ref_frame_flag, YV12_BUFFER_CONFIG *sd)
{
    VP8D_COMP *pbi = (VP8D_COMP *) ptr;
    VP8_COMMON *cm = &pbi->common;
    int ref_fb_idx;

    if (ref_frame_flag == VP8_LAST_FLAG)
        ref_fb_idx = cm->lst_fb_idx;
    else if (ref_frame_flag == VP8_GOLD_FLAG)
        ref_fb_idx = cm->gld_fb_idx;
    else if (ref_frame_flag == VP8_ALT_FLAG)
        ref_fb_idx = cm->alt_fb_idx;
    else
        return -1;

    vp8_yv12_copy_frame_ptr(&cm->yv12_fb[ref_fb_idx], sd);

    return 0;
}
int vp8dx_set_reference(VP8D_PTR ptr, VP8_REFFRAME ref_frame_flag, YV12_BUFFER_CONFIG *sd)
{
    VP8D_COMP *pbi = (VP8D_COMP *) ptr;
    VP8_COMMON *cm = &pbi->common;
    int ref_fb_idx;

    if (ref_frame_flag == VP8_LAST_FLAG)
        ref_fb_idx = cm->lst_fb_idx;
    else if (ref_frame_flag == VP8_GOLD_FLAG)
        ref_fb_idx = cm->gld_fb_idx;
    else if (ref_frame_flag == VP8_ALT_FLAG)
        ref_fb_idx = cm->alt_fb_idx;
    else
        return -1;

    vp8_yv12_copy_frame_ptr(sd, &cm->yv12_fb[ref_fb_idx]);

    return 0;
}

//For ARM NEON, d8-d15 are callee-saved registers, and need to be saved by us.
#if HAVE_ARMV7
extern void vp8_push_neon(INT64 *store);
extern void vp8_pop_neon(INT64 *store);
static INT64 dx_store_reg[8];
#endif

static int get_free_fb (VP8_COMMON *cm)
{
    int i;
    for (i = 0; i < NUM_YV12_BUFFERS; i++)
        if (cm->fb_idx_ref_cnt[i] == 0)
            break;

    cm->fb_idx_ref_cnt[i] = 1;
    return i;
}

static void ref_cnt_fb (int *buf, int *idx, int new_idx)
{
    if (buf[*idx] > 0)
        buf[*idx]--;

    *idx = new_idx;

    buf[new_idx]++;
}

// If any buffer copy / swapping is signalled it should be done here.
static int swap_frame_buffers (VP8_COMMON *cm)
{
    int fb_to_update_with, err = 0;

    if (cm->refresh_last_frame)
        fb_to_update_with = cm->lst_fb_idx;
    else
        fb_to_update_with = cm->new_fb_idx;

    // The alternate reference frame or golden frame can be updated
    //  using the new, last, or golden/alt ref frame.  If it
    //  is updated using the newly decoded frame it is a refresh.
    //  An update using the last or golden/alt ref frame is a copy.
    if (cm->copy_buffer_to_arf)
    {
        int new_fb = 0;

        if (cm->copy_buffer_to_arf == 1)
            new_fb = fb_to_update_with;
        else if (cm->copy_buffer_to_arf == 2)
            new_fb = cm->gld_fb_idx;
        else
            err = -1;

        ref_cnt_fb (cm->fb_idx_ref_cnt, &cm->alt_fb_idx, new_fb);
    }

    if (cm->copy_buffer_to_gf)
    {
        int new_fb = 0;

        if (cm->copy_buffer_to_gf == 1)
            new_fb = fb_to_update_with;
        else if (cm->copy_buffer_to_gf == 2)
            new_fb = cm->alt_fb_idx;
        else
            err = -1;

        ref_cnt_fb (cm->fb_idx_ref_cnt, &cm->gld_fb_idx, new_fb);
    }

    if (cm->refresh_golden_frame)
        ref_cnt_fb (cm->fb_idx_ref_cnt, &cm->gld_fb_idx, cm->new_fb_idx);

    if (cm->refresh_alt_ref_frame)
        ref_cnt_fb (cm->fb_idx_ref_cnt, &cm->alt_fb_idx, cm->new_fb_idx);

    if (cm->refresh_last_frame)
    {
        ref_cnt_fb (cm->fb_idx_ref_cnt, &cm->lst_fb_idx, cm->new_fb_idx);

        cm->frame_to_show = &cm->yv12_fb[cm->lst_fb_idx];
    }
    else
        cm->frame_to_show = &cm->yv12_fb[cm->new_fb_idx];

    cm->fb_idx_ref_cnt[cm->new_fb_idx]--;

    return err;
}

int vp8dx_receive_compressed_data(VP8D_PTR ptr, unsigned long size, const unsigned char *source, INT64 time_stamp)
{
    VP8D_COMP *pbi = (VP8D_COMP *) ptr;
    VP8_COMMON *cm = &pbi->common;
    int retcode = 0;
    struct vpx_usec_timer timer;

//  if(pbi->ready_for_new_data == 0)
//      return -1;

    if (ptr == 0)
    {
        return -1;
    }

    pbi->common.error.error_code = VPX_CODEC_OK;

    cm->new_fb_idx = get_free_fb (cm);

    if (setjmp(pbi->common.error.jmp))
    {
        pbi->common.error.setjmp = 0;
        if (cm->fb_idx_ref_cnt[cm->new_fb_idx] > 0)
          cm->fb_idx_ref_cnt[cm->new_fb_idx]--;
        return -1;
    }

    pbi->common.error.setjmp = 1;

#if HAVE_ARMV7
    vp8_push_neon(dx_store_reg);
#endif

    vpx_usec_timer_start(&timer);

    //cm->current_video_frame++;
    pbi->Source = source;
    pbi->source_sz = size;

    retcode = vp8_decode_frame(pbi);

    if (retcode < 0)
    {
#if HAVE_ARMV7
        vp8_pop_neon(dx_store_reg);
#endif
        pbi->common.error.error_code = VPX_CODEC_ERROR;
        pbi->common.error.setjmp = 0;
        if (cm->fb_idx_ref_cnt[cm->new_fb_idx] > 0)
          cm->fb_idx_ref_cnt[cm->new_fb_idx]--;
        return retcode;
    }

    if (pbi->b_multithreaded_lf && pbi->common.filter_level != 0)
        vp8_stop_lfthread(pbi);

    if (swap_frame_buffers (cm))
    {
        pbi->common.error.error_code = VPX_CODEC_ERROR;
        pbi->common.error.setjmp = 0;
        return -1;
    }

/*
    if (!pbi->b_multithreaded_lf)
    {
        struct vpx_usec_timer lpftimer;
        vpx_usec_timer_start(&lpftimer);
        // Apply the loop filter if appropriate.

        if (cm->filter_level > 0)
            vp8_loop_filter_frame(cm, &pbi->mb, cm->filter_level);

        vpx_usec_timer_mark(&lpftimer);
        pbi->time_loop_filtering += vpx_usec_timer_elapsed(&lpftimer);
    }else{
      struct vpx_usec_timer lpftimer;
      vpx_usec_timer_start(&lpftimer);
      // Apply the loop filter if appropriate.

      if (cm->filter_level > 0)
          vp8_mt_loop_filter_frame(cm, &pbi->mb, cm->filter_level);

      vpx_usec_timer_mark(&lpftimer);
      pbi->time_loop_filtering += vpx_usec_timer_elapsed(&lpftimer);
    }
    if (cm->filter_level > 0) {
        cm->last_frame_type = cm->frame_type;
        cm->last_filter_type = cm->filter_type;
        cm->last_sharpness_level = cm->sharpness_level;
    }
*/

    if(pbi->common.filter_level)
    {
        struct vpx_usec_timer lpftimer;
        vpx_usec_timer_start(&lpftimer);
        // Apply the loop filter if appropriate.

        if (pbi->b_multithreaded_lf && cm->multi_token_partition != ONE_PARTITION)
            vp8_mt_loop_filter_frame(pbi);   //cm, &pbi->mb, cm->filter_level);
        else
            vp8_loop_filter_frame(cm, &pbi->mb, cm->filter_level);

        vpx_usec_timer_mark(&lpftimer);
        pbi->time_loop_filtering += vpx_usec_timer_elapsed(&lpftimer);

        cm->last_frame_type = cm->frame_type;
        cm->last_filter_type = cm->filter_type;
        cm->last_sharpness_level = cm->sharpness_level;
    }

    vp8_yv12_extend_frame_borders_ptr(cm->frame_to_show);

#if 0
    // DEBUG code
    //vp8_recon_write_yuv_frame("recon.yuv", cm->frame_to_show);
    if (cm->current_video_frame <= 5)
        write_dx_frame_to_file(cm->frame_to_show, cm->current_video_frame);
#endif

    vp8_clear_system_state();

    vpx_usec_timer_mark(&timer);
    pbi->decode_microseconds = vpx_usec_timer_elapsed(&timer);

    pbi->time_decoding += pbi->decode_microseconds;

//  vp8_print_modes_and_motion_vectors( cm->mi, cm->mb_rows,cm->mb_cols, cm->current_video_frame);

    if (cm->show_frame)
        cm->current_video_frame++;

    pbi->ready_for_new_data = 0;
    pbi->last_time_stamp = time_stamp;

#if 0
    {
        int i;
        INT64 earliest_time = pbi->dr[0].time_stamp;
        INT64 latest_time = pbi->dr[0].time_stamp;
        INT64 time_diff = 0;
        int bytes = 0;

        pbi->dr[pbi->common.current_video_frame&0xf].size = pbi->bc.pos + pbi->bc2.pos + 4;;
        pbi->dr[pbi->common.current_video_frame&0xf].time_stamp = time_stamp;

        for (i = 0; i < 16; i++)
        {

            bytes += pbi->dr[i].size;

            if (pbi->dr[i].time_stamp < earliest_time)
                earliest_time = pbi->dr[i].time_stamp;

            if (pbi->dr[i].time_stamp > latest_time)
                latest_time = pbi->dr[i].time_stamp;
        }

        time_diff = latest_time - earliest_time;

        if (time_diff > 0)
        {
            pbi->common.bitrate = 80000.00 * bytes / time_diff  ;
            pbi->common.framerate = 160000000.00 / time_diff ;
        }

    }
#endif

#if HAVE_ARMV7
    vp8_pop_neon(dx_store_reg);
#endif
    pbi->common.error.setjmp = 0;
    return retcode;
}
int vp8dx_get_raw_frame(VP8D_PTR ptr, YV12_BUFFER_CONFIG *sd, INT64 *time_stamp, INT64 *time_end_stamp, int deblock_level,  int noise_level, int flags)
{
    int ret = -1;
    VP8D_COMP *pbi = (VP8D_COMP *) ptr;

    if (pbi->ready_for_new_data == 1)
        return ret;

    // ie no raw frame to show!!!
    if (pbi->common.show_frame == 0)
        return ret;

    pbi->ready_for_new_data = 1;
    *time_stamp = pbi->last_time_stamp;
    *time_end_stamp = 0;

    sd->clrtype = pbi->common.clr_type;
#if CONFIG_POSTPROC
    ret = vp8_post_proc_frame(&pbi->common, sd, deblock_level, noise_level, flags);
#else

    if (pbi->common.frame_to_show)
    {
        *sd = *pbi->common.frame_to_show;
        sd->y_width = pbi->common.Width;
        sd->y_height = pbi->common.Height;
        sd->uv_height = pbi->common.Height / 2;
        ret = 0;
    }
    else
    {
        ret = -1;
    }

#endif //!CONFIG_POSTPROC
    vp8_clear_system_state();
    return ret;
}
