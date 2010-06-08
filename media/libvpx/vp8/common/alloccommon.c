/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license 
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may 
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "vpx_ports/config.h"
#include "blockd.h"
#include "vpx_mem/vpx_mem.h"
#include "onyxc_int.h"
#include "findnearmv.h"
#include "entropymode.h"
#include "systemdependent.h"
#include "vpxerrors.h"


extern  void vp8_init_scan_order_mask();

void vp8_update_mode_info_border(MODE_INFO *mi, int rows, int cols)
{
    int i;
    vpx_memset(mi - cols - 1, 0, sizeof(MODE_INFO) * cols + 1);

    for (i = 0; i < rows; i++)
    {
        vpx_memset(&mi[i*cols-1], 0, sizeof(MODE_INFO));
    }
}
void vp8_de_alloc_frame_buffers(VP8_COMMON *oci)
{
    vp8_yv12_de_alloc_frame_buffer(&oci->temp_scale_frame);
    vp8_yv12_de_alloc_frame_buffer(&oci->new_frame);
    vp8_yv12_de_alloc_frame_buffer(&oci->last_frame);
    vp8_yv12_de_alloc_frame_buffer(&oci->golden_frame);
    vp8_yv12_de_alloc_frame_buffer(&oci->alt_ref_frame);
    vp8_yv12_de_alloc_frame_buffer(&oci->post_proc_buffer);

    vpx_free(oci->above_context[Y1CONTEXT]);
    vpx_free(oci->above_context[UCONTEXT]);
    vpx_free(oci->above_context[VCONTEXT]);
    vpx_free(oci->above_context[Y2CONTEXT]);
    vpx_free(oci->mip);

    oci->above_context[Y1CONTEXT] = 0;
    oci->above_context[UCONTEXT]  = 0;
    oci->above_context[VCONTEXT]  = 0;
    oci->above_context[Y2CONTEXT] = 0;
    oci->mip = 0;

    // Structure used to minitor GF useage
    if (oci->gf_active_flags != 0)
        vpx_free(oci->gf_active_flags);

    oci->gf_active_flags = 0;
}

int vp8_alloc_frame_buffers(VP8_COMMON *oci, int width, int height)
{
    vp8_de_alloc_frame_buffers(oci);

    // our internal buffers are always multiples of 16
    if ((width & 0xf) != 0)
        width += 16 - (width & 0xf);

    if ((height & 0xf) != 0)
        height += 16 - (height & 0xf);


    if (vp8_yv12_alloc_frame_buffer(&oci->temp_scale_frame,   width, 16, VP8BORDERINPIXELS) < 0)
    {
        vp8_de_alloc_frame_buffers(oci);
        return ALLOC_FAILURE;
    }


    if (vp8_yv12_alloc_frame_buffer(&oci->new_frame,   width, height, VP8BORDERINPIXELS) < 0)
    {
        vp8_de_alloc_frame_buffers(oci);
        return ALLOC_FAILURE;
    }

    if (vp8_yv12_alloc_frame_buffer(&oci->last_frame,  width, height, VP8BORDERINPIXELS) < 0)
    {
        vp8_de_alloc_frame_buffers(oci);
        return ALLOC_FAILURE;
    }

    if (vp8_yv12_alloc_frame_buffer(&oci->golden_frame, width, height, VP8BORDERINPIXELS) < 0)
    {
        vp8_de_alloc_frame_buffers(oci);
        return ALLOC_FAILURE;
    }

    if (vp8_yv12_alloc_frame_buffer(&oci->alt_ref_frame, width, height, VP8BORDERINPIXELS) < 0)
    {
        vp8_de_alloc_frame_buffers(oci);
        return ALLOC_FAILURE;
    }

    if (vp8_yv12_alloc_frame_buffer(&oci->post_proc_buffer, width, height, VP8BORDERINPIXELS) < 0)
    {
        vp8_de_alloc_frame_buffers(oci);
        return ALLOC_FAILURE;
    }

    oci->mb_rows = height >> 4;
    oci->mb_cols = width >> 4;
    oci->MBs = oci->mb_rows * oci->mb_cols;
    oci->mode_info_stride = oci->mb_cols + 1;
    oci->mip = vpx_calloc((oci->mb_cols + 1) * (oci->mb_rows + 1), sizeof(MODE_INFO));

    if (!oci->mip)
    {
        vp8_de_alloc_frame_buffers(oci);
        return ALLOC_FAILURE;
    }

    oci->mi = oci->mip + oci->mode_info_stride + 1;


    oci->above_context[Y1CONTEXT] = vpx_calloc(sizeof(ENTROPY_CONTEXT) * oci->mb_cols * 4 , 1);

    if (!oci->above_context[Y1CONTEXT])
    {
        vp8_de_alloc_frame_buffers(oci);
        return ALLOC_FAILURE;
    }

    oci->above_context[UCONTEXT]  = vpx_calloc(sizeof(ENTROPY_CONTEXT) * oci->mb_cols * 2 , 1);

    if (!oci->above_context[UCONTEXT])
    {
        vp8_de_alloc_frame_buffers(oci);
        return ALLOC_FAILURE;
    }

    oci->above_context[VCONTEXT]  = vpx_calloc(sizeof(ENTROPY_CONTEXT) * oci->mb_cols * 2 , 1);

    if (!oci->above_context[VCONTEXT])
    {
        vp8_de_alloc_frame_buffers(oci);
        return ALLOC_FAILURE;
    }

    oci->above_context[Y2CONTEXT] = vpx_calloc(sizeof(ENTROPY_CONTEXT) * oci->mb_cols     , 1);

    if (!oci->above_context[Y2CONTEXT])
    {
        vp8_de_alloc_frame_buffers(oci);
        return ALLOC_FAILURE;
    }

    vp8_update_mode_info_border(oci->mi, oci->mb_rows, oci->mb_cols);

    // Structures used to minitor GF usage
    if (oci->gf_active_flags != 0)
        vpx_free(oci->gf_active_flags);

    oci->gf_active_flags = (unsigned char *)vpx_calloc(oci->mb_rows * oci->mb_cols, 1);

    if (!oci->gf_active_flags)
    {
        vp8_de_alloc_frame_buffers(oci);
        return ALLOC_FAILURE;
    }

    oci->gf_active_count = oci->mb_rows * oci->mb_cols;

    return 0;
}
void vp8_setup_version(VP8_COMMON *cm)
{
    switch (cm->version)
    {
    case 0:
        cm->no_lpf = 0;
        cm->simpler_lpf = 0;
        cm->use_bilinear_mc_filter = 0;
        cm->full_pixel = 0;
        break;
    case 1:
        cm->no_lpf = 0;
        cm->simpler_lpf = 1;
        cm->use_bilinear_mc_filter = 1;
        cm->full_pixel = 0;
        break;
    case 2:
        cm->no_lpf = 1;
        cm->simpler_lpf = 0;
        cm->use_bilinear_mc_filter = 1;
        cm->full_pixel = 0;
        break;
    case 3:
        cm->no_lpf = 1;
        cm->simpler_lpf = 1;
        cm->use_bilinear_mc_filter = 1;
        cm->full_pixel = 1;
        break;
    default:
        //4,5,6,7 are reserved for future use
        cm->no_lpf = 0;
        cm->simpler_lpf = 0;
        cm->use_bilinear_mc_filter = 0;
        cm->full_pixel = 0;
        break;
    }
}
void vp8_create_common(VP8_COMMON *oci)
{
    vp8_machine_specific_config(oci);
    vp8_default_coef_probs(oci);
    vp8_init_mbmode_probs(oci);
    vp8_default_bmode_probs(oci->fc.bmode_prob);

    oci->mb_no_coeff_skip = 1;
    oci->no_lpf = 0;
    oci->simpler_lpf = 0;
    oci->use_bilinear_mc_filter = 0;
    oci->full_pixel = 0;
    oci->multi_token_partition = ONE_PARTITION;
    oci->clr_type = REG_YUV;
    oci->clamp_type = RECON_CLAMP_REQUIRED;

    // Initialise reference frame sign bias structure to defaults
    vpx_memset(oci->ref_frame_sign_bias, 0, sizeof(oci->ref_frame_sign_bias));

    // Default disable buffer to buffer copying
    oci->copy_buffer_to_gf = 0;
    oci->copy_buffer_to_arf = 0;
}

void vp8_remove_common(VP8_COMMON *oci)
{
    vp8_de_alloc_frame_buffers(oci);
}

void vp8_initialize_common()
{
    vp8_coef_tree_initialize();

    vp8_entropy_mode_init();

    vp8_init_scan_order_mask();

}
