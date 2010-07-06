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
#include "segmentation_common.h"
#include "quant_common.h"
#include "vpx_scale/vpxscale.h"
#include "systemdependent.h"
#include "vpx_ports/vpx_timer.h"


extern void vp8_init_loop_filter(VP8_COMMON *cm);

extern void vp8cx_init_de_quantizer(VP8D_COMP *pbi);

// DEBUG code
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

    if (ref_frame_flag == VP8_LAST_FLAG)
        vp8_yv12_copy_frame_ptr(&cm->last_frame, sd);

    else if (ref_frame_flag == VP8_GOLD_FLAG)
        vp8_yv12_copy_frame_ptr(&cm->golden_frame, sd);

    else if (ref_frame_flag == VP8_ALT_FLAG)
        vp8_yv12_copy_frame_ptr(&cm->alt_ref_frame, sd);

    else
        return -1;

    return 0;
}
int vp8dx_set_reference(VP8D_PTR ptr, VP8_REFFRAME ref_frame_flag, YV12_BUFFER_CONFIG *sd)
{
    VP8D_COMP *pbi = (VP8D_COMP *) ptr;
    VP8_COMMON *cm = &pbi->common;

    if (ref_frame_flag == VP8_LAST_FLAG)
        vp8_yv12_copy_frame_ptr(sd, &cm->last_frame);

    else if (ref_frame_flag == VP8_GOLD_FLAG)
        vp8_yv12_copy_frame_ptr(sd, &cm->golden_frame);

    else if (ref_frame_flag == VP8_ALT_FLAG)
        vp8_yv12_copy_frame_ptr(sd, &cm->alt_ref_frame);

    else
        return -1;

    return 0;
}

//For ARM NEON, d8-d15 are callee-saved registers, and need to be saved by us.
#if HAVE_ARMV7
extern void vp8_push_neon(INT64 *store);
extern void vp8_pop_neon(INT64 *store);
static INT64 dx_store_reg[8];
#endif
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

    if (setjmp(pbi->common.error.jmp))
    {
        pbi->common.error.setjmp = 0;
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
        return retcode;
    }

    // Update the GF useage maps.
    vp8_update_gf_useage_maps(cm, &pbi->mb);

    if (pbi->b_multithreaded_lf && pbi->common.filter_level != 0)
        vp8_stop_lfthread(pbi);

    if (cm->refresh_last_frame)
    {
        vp8_swap_yv12_buffer(&cm->last_frame, &cm->new_frame);

        cm->frame_to_show = &cm->last_frame;
    }
    else
    {
        cm->frame_to_show = &cm->new_frame;
    }

    if (!pbi->b_multithreaded_lf)
    {
        struct vpx_usec_timer lpftimer;
        vpx_usec_timer_start(&lpftimer);
        // Apply the loop filter if appropriate.

        if (cm->filter_level > 0)
        {
            vp8_loop_filter_frame(cm, &pbi->mb, cm->filter_level);
            cm->last_frame_type = cm->frame_type;
            cm->last_filter_type = cm->filter_type;
            cm->last_sharpness_level = cm->sharpness_level;

        }

        vpx_usec_timer_mark(&lpftimer);
        pbi->time_loop_filtering += vpx_usec_timer_elapsed(&lpftimer);
    }

    vp8_yv12_extend_frame_borders_ptr(cm->frame_to_show);

#if 0
    // DEBUG code
    //vp8_recon_write_yuv_frame("recon.yuv", cm->frame_to_show);
    if (cm->current_video_frame <= 5)
        write_dx_frame_to_file(cm->frame_to_show, cm->current_video_frame);
#endif

    // If any buffer copy / swaping is signalled it should be done here.
    if (cm->copy_buffer_to_arf)
    {
        if (cm->copy_buffer_to_arf == 1)
        {
            if (cm->refresh_last_frame)
                vp8_yv12_copy_frame_ptr(&cm->new_frame, &cm->alt_ref_frame);
            else
                vp8_yv12_copy_frame_ptr(&cm->last_frame, &cm->alt_ref_frame);
        }
        else if (cm->copy_buffer_to_arf == 2)
            vp8_yv12_copy_frame_ptr(&cm->golden_frame, &cm->alt_ref_frame);
    }

    if (cm->copy_buffer_to_gf)
    {
        if (cm->copy_buffer_to_gf == 1)
        {
            if (cm->refresh_last_frame)
                vp8_yv12_copy_frame_ptr(&cm->new_frame, &cm->golden_frame);
            else
                vp8_yv12_copy_frame_ptr(&cm->last_frame, &cm->golden_frame);
        }
        else if (cm->copy_buffer_to_gf == 2)
            vp8_yv12_copy_frame_ptr(&cm->alt_ref_frame, &cm->golden_frame);
    }

    // Should the golden or alternate reference frame be refreshed?
    if (cm->refresh_golden_frame || cm->refresh_alt_ref_frame)
    {
        if (cm->refresh_golden_frame)
            vp8_yv12_copy_frame_ptr(cm->frame_to_show, &cm->golden_frame);

        if (cm->refresh_alt_ref_frame)
            vp8_yv12_copy_frame_ptr(cm->frame_to_show, &cm->alt_ref_frame);

        //vpx_log("Decoder: recovery frame received \n");

        // Update data structures that monitors GF useage
        vpx_memset(cm->gf_active_flags, 1, (cm->mb_rows * cm->mb_cols));
        cm->gf_active_count = cm->mb_rows * cm->mb_cols;
    }

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
