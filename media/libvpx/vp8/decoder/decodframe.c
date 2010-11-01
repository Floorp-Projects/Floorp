/*
 *  Copyright (c) 2010 The VP8 project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "onyxd_int.h"
#include "header.h"
#include "reconintra.h"
#include "reconintra4x4.h"
#include "recon.h"
#include "reconinter.h"
#include "dequantize.h"
#include "detokenize.h"
#include "invtrans.h"
#include "alloccommon.h"
#include "entropymode.h"
#include "quant_common.h"

#include "setupintrarecon.h"
#include "demode.h"
#include "decodemv.h"
#include "extend.h"
#include "vpx_mem/vpx_mem.h"
#include "idct.h"
#include "dequantize.h"
#include "predictdc.h"
#include "threading.h"
#include "decoderthreading.h"
#include "dboolhuff.h"

#include <assert.h>
#include <stdio.h>

void vp8cx_init_de_quantizer(VP8D_COMP *pbi)
{
    int r, c;
    int i;
    int Q;
    VP8_COMMON *const pc = & pbi->common;

    for (Q = 0; Q < QINDEX_RANGE; Q++)
    {
        pc->Y1dequant[Q][0][0] = (short)vp8_dc_quant(Q, pc->y1dc_delta_q);
        pc->Y2dequant[Q][0][0] = (short)vp8_dc2quant(Q, pc->y2dc_delta_q);
        pc->UVdequant[Q][0][0] = (short)vp8_dc_uv_quant(Q, pc->uvdc_delta_q);

        // all the ac values = ;
        for (i = 1; i < 16; i++)
        {
            int rc = vp8_default_zig_zag1d[i];
            r = (rc >> 2);
            c = (rc & 3);

            pc->Y1dequant[Q][r][c] = (short)vp8_ac_yquant(Q);
            pc->Y2dequant[Q][r][c] = (short)vp8_ac2quant(Q, pc->y2ac_delta_q);
            pc->UVdequant[Q][r][c] = (short)vp8_ac_uv_quant(Q, pc->uvac_delta_q);
        }
    }
}

static void mb_init_dequantizer(VP8D_COMP *pbi, MACROBLOCKD *xd)
{
    int i;
    int QIndex;
    MB_MODE_INFO *mbmi = &xd->mode_info_context->mbmi;
    VP8_COMMON *const pc = & pbi->common;

    // Decide whether to use the default or alternate baseline Q value.
    if (xd->segmentation_enabled)
    {
        // Abs Value
        if (xd->mb_segement_abs_delta == SEGMENT_ABSDATA)
            QIndex = xd->segment_feature_data[MB_LVL_ALT_Q][mbmi->segment_id];

        // Delta Value
        else
        {
            QIndex = pc->base_qindex + xd->segment_feature_data[MB_LVL_ALT_Q][mbmi->segment_id];
            QIndex = (QIndex >= 0) ? ((QIndex <= MAXQ) ? QIndex : MAXQ) : 0;    // Clamp to valid range
        }
    }
    else
        QIndex = pc->base_qindex;

    // Set up the block level dequant pointers
    for (i = 0; i < 16; i++)
    {
        xd->block[i].dequant = pc->Y1dequant[QIndex];
    }

    for (i = 16; i < 24; i++)
    {
        xd->block[i].dequant = pc->UVdequant[QIndex];
    }

    xd->block[24].dequant = pc->Y2dequant[QIndex];

}

#if CONFIG_RUNTIME_CPU_DETECT
#define RTCD_VTABLE(x) (&(pbi)->common.rtcd.x)
#else
#define RTCD_VTABLE(x) NULL
#endif

//skip_recon_mb() is Modified: Instead of writing the result to predictor buffer and then copying it
// to dst buffer, we can write the result directly to dst buffer. This eliminates unnecessary copy.
static void skip_recon_mb(VP8D_COMP *pbi, MACROBLOCKD *xd)
{
    if (xd->frame_type == KEY_FRAME  ||  xd->mode_info_context->mbmi.ref_frame == INTRA_FRAME)
    {

        vp8_build_intra_predictors_mbuv_s(xd);
        vp8_build_intra_predictors_mby_s_ptr(xd);

    }
    else
    {
        vp8_build_inter_predictors_mb_s(xd);
    }
}

static void clamp_mv_to_umv_border(MV *mv, const MACROBLOCKD *xd)
{
    /* If the MV points so far into the UMV border that no visible pixels
     * are used for reconstruction, the subpel part of the MV can be
     * discarded and the MV limited to 16 pixels with equivalent results.
     *
     * This limit kicks in at 19 pixels for the top and left edges, for
     * the 16 pixels plus 3 taps right of the central pixel when subpel
     * filtering. The bottom and right edges use 16 pixels plus 2 pixels
     * left of the central pixel when filtering.
     */
    if (mv->col < (xd->mb_to_left_edge - (19 << 3)))
        mv->col = xd->mb_to_left_edge - (16 << 3);
    else if (mv->col > xd->mb_to_right_edge + (18 << 3))
        mv->col = xd->mb_to_right_edge + (16 << 3);

    if (mv->row < (xd->mb_to_top_edge - (19 << 3)))
        mv->row = xd->mb_to_top_edge - (16 << 3);
    else if (mv->row > xd->mb_to_bottom_edge + (18 << 3))
        mv->row = xd->mb_to_bottom_edge + (16 << 3);
}

/* A version of the above function for chroma block MVs.*/
static void clamp_uvmv_to_umv_border(MV *mv, const MACROBLOCKD *xd)
{
    if (2*mv->col < (xd->mb_to_left_edge - (19 << 3)))
        mv->col = (xd->mb_to_left_edge - (16 << 3)) >> 1;
    else if (2*mv->col > xd->mb_to_right_edge + (18 << 3))
        mv->col = (xd->mb_to_right_edge + (16 << 3)) >> 1;

    if (2*mv->row < (xd->mb_to_top_edge - (19 << 3)))
        mv->row = (xd->mb_to_top_edge - (16 << 3)) >> 1;
    else if (2*mv->row > xd->mb_to_bottom_edge + (18 << 3))
        mv->row = (xd->mb_to_bottom_edge + (16 << 3)) >> 1;
}

static void clamp_mvs(MACROBLOCKD *xd)
{
    if (xd->mode_info_context->mbmi.mode == SPLITMV)
    {
        int i;

        for (i=0; i<16; i++)
            clamp_mv_to_umv_border(&xd->block[i].bmi.mv.as_mv, xd);
        for (i=16; i<24; i++)
            clamp_uvmv_to_umv_border(&xd->block[i].bmi.mv.as_mv, xd);
    }
    else
    {
        clamp_mv_to_umv_border(&xd->mode_info_context->mbmi.mv.as_mv, xd);
        clamp_uvmv_to_umv_border(&xd->block[16].bmi.mv.as_mv, xd);
    }

}

void vp8_decode_macroblock(VP8D_COMP *pbi, MACROBLOCKD *xd)
{
    int eobtotal = 0;
    int i, do_clamp = xd->mode_info_context->mbmi.need_to_clamp_mvs;

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
        skip_recon_mb(pbi, xd);
        return;
    }

    if (xd->segmentation_enabled)
        mb_init_dequantizer(pbi, xd);

    // do prediction
    if (xd->frame_type == KEY_FRAME  ||  xd->mode_info_context->mbmi.ref_frame == INTRA_FRAME)
    {
        vp8_build_intra_predictors_mbuv(xd);

        if (xd->mode_info_context->mbmi.mode != B_PRED)
        {
            vp8_build_intra_predictors_mby_ptr(xd);
        } else {
            vp8_intra_prediction_down_copy(xd);
        }
    }
    else
    {
        vp8_build_inter_predictors_mb(xd);
    }

    // dequantization and idct
    if (xd->mode_info_context->mbmi.mode != B_PRED && xd->mode_info_context->mbmi.mode != SPLITMV)
    {
        BLOCKD *b = &xd->block[24];
        DEQUANT_INVOKE(&pbi->dequant, block)(b);

        // do 2nd order transform on the dc block
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
                        (xd->qcoeff, &xd->block[0].dequant[0][0],
                         xd->predictor, xd->dst.y_buffer,
                         xd->dst.y_stride, xd->eobs, xd->block[24].diff);
    }
    else if ((xd->frame_type == KEY_FRAME  ||  xd->mode_info_context->mbmi.ref_frame == INTRA_FRAME) && xd->mode_info_context->mbmi.mode == B_PRED)
    {
        for (i = 0; i < 16; i++)
        {

            BLOCKD *b = &xd->block[i];
            vp8_predict_intra4x4(b, b->bmi.mode, b->predictor);

            if (xd->eobs[i] > 1)
            {
                DEQUANT_INVOKE(&pbi->dequant, idct_add)
                    (b->qcoeff, &b->dequant[0][0],  b->predictor,
                    *(b->base_dst) + b->dst, 16, b->dst_stride);
            }
            else
            {
                IDCT_INVOKE(RTCD_VTABLE(idct), idct1_scalar_add)
                    (b->qcoeff[0] * b->dequant[0][0], b->predictor,
                    *(b->base_dst) + b->dst, 16, b->dst_stride);
                ((int *)b->qcoeff)[0] = 0;
            }
        }

    }
    else
    {
        DEQUANT_INVOKE (&pbi->dequant, idct_add_y_block)
                        (xd->qcoeff, &xd->block[0].dequant[0][0],
                         xd->predictor, xd->dst.y_buffer,
                         xd->dst.y_stride, xd->eobs);
    }

    DEQUANT_INVOKE (&pbi->dequant, idct_add_uv_block)
                    (xd->qcoeff+16*16, &xd->block[16].dequant[0][0],
                     xd->predictor+16*16, xd->dst.u_buffer, xd->dst.v_buffer,
                     xd->dst.uv_stride, xd->eobs+16);
}

static int get_delta_q(vp8_reader *bc, int prev, int *q_update)
{
    int ret_val = 0;

    if (vp8_read_bit(bc))
    {
        ret_val = vp8_read_literal(bc, 4);

        if (vp8_read_bit(bc))
            ret_val = -ret_val;
    }

    /* Trigger a quantizer update if the delta-q value has changed */
    if (ret_val != prev)
        *q_update = 1;

    return ret_val;
}

#ifdef PACKET_TESTING
#include <stdio.h>
FILE *vpxlog = 0;
#endif



void vp8_decode_mb_row(VP8D_COMP *pbi,
                       VP8_COMMON *pc,
                       int mb_row,
                       MACROBLOCKD *xd)
{

    int i;
    int recon_yoffset, recon_uvoffset;
    int mb_col;
    int ref_fb_idx = pc->lst_fb_idx;
    int dst_fb_idx = pc->new_fb_idx;
    int recon_y_stride = pc->yv12_fb[ref_fb_idx].y_stride;
    int recon_uv_stride = pc->yv12_fb[ref_fb_idx].uv_stride;

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

        /*
        if(pc->current_video_frame==0 &&mb_col==1 && mb_row==0)
        pbi->debugoutput =1;
        else
        pbi->debugoutput =0;
        */
        vp8_decode_macroblock(pbi, xd);


        recon_yoffset += 16;
        recon_uvoffset += 8;

        ++xd->mode_info_context;  /* next mb */

        xd->above_context++;

        pbi->current_mb_col_main = mb_col;
    }

    // adjust to the next row of mbs
    vp8_extend_mb_row(
        &pc->yv12_fb[dst_fb_idx],
        xd->dst.y_buffer + 16, xd->dst.u_buffer + 8, xd->dst.v_buffer + 8
    );

    ++xd->mode_info_context;      /* skip prediction column */

    pbi->last_mb_row_decoded = mb_row;
}


static unsigned int read_partition_size(const unsigned char *cx_size)
{
    const unsigned int size =
        cx_size[0] + (cx_size[1] << 8) + (cx_size[2] << 16);
    return size;
}


static void setup_token_decoder(VP8D_COMP *pbi,
                                const unsigned char *cx_data)
{
    int num_part;
    int i;
    VP8_COMMON          *pc = &pbi->common;
    const unsigned char *user_data_end = pbi->Source + pbi->source_sz;
    vp8_reader          *bool_decoder;
    const unsigned char *partition;

    /* Parse number of token partitions to use */
    pc->multi_token_partition = (TOKEN_PARTITION)vp8_read_literal(&pbi->bc, 2);
    num_part = 1 << pc->multi_token_partition;

    /* Set up pointers to the first partition */
    partition = cx_data;
    bool_decoder = &pbi->bc2;

    if (num_part > 1)
    {
        CHECK_MEM_ERROR(pbi->mbc, vpx_malloc(num_part * sizeof(vp8_reader)));
        bool_decoder = pbi->mbc;
        partition += 3 * (num_part - 1);
    }

    for (i = 0; i < num_part; i++)
    {
        const unsigned char *partition_size_ptr = cx_data + i * 3;
        unsigned int         partition_size;

        /* Calculate the length of this partition. The last partition
         * size is implicit.
         */
        if (i < num_part - 1)
        {
            partition_size = read_partition_size(partition_size_ptr);
        }
        else
        {
            partition_size = user_data_end - partition;
        }

        if (user_data_end - partition < partition_size)
            vpx_internal_error(&pc->error, VPX_CODEC_CORRUPT_FRAME,
                               "Truncated packet or corrupt partition "
                               "%d length", i + 1);

        if (vp8dx_start_decode(bool_decoder, IF_RTCD(&pbi->dboolhuff),
                               partition, partition_size))
            vpx_internal_error(&pc->error, VPX_CODEC_MEM_ERROR,
                               "Failed to allocate bool decoder %d", i + 1);

        /* Advance to the next partition */
        partition += partition_size;
        bool_decoder++;
    }

    /* Clamp number of decoder threads */
    if (pbi->decoding_thread_count > num_part - 1)
        pbi->decoding_thread_count = num_part - 1;
}


static void stop_token_decoder(VP8D_COMP *pbi)
{
    int i;
    VP8_COMMON *pc = &pbi->common;

    if (pc->multi_token_partition != ONE_PARTITION)
        vpx_free(pbi->mbc);
}

static void init_frame(VP8D_COMP *pbi)
{
    VP8_COMMON *const pc = & pbi->common;
    MACROBLOCKD *const xd  = & pbi->mb;

    if (pc->frame_type == KEY_FRAME)
    {
        // Various keyframe initializations
        vpx_memcpy(pc->fc.mvc, vp8_default_mv_context, sizeof(vp8_default_mv_context));

        vp8_init_mbmode_probs(pc);

        vp8_default_coef_probs(pc);
        vp8_kf_default_bmode_probs(pc->kf_bmode_prob);

        // reset the segment feature data to 0 with delta coding (Default state).
        vpx_memset(xd->segment_feature_data, 0, sizeof(xd->segment_feature_data));
        xd->mb_segement_abs_delta = SEGMENT_DELTADATA;

       // reset the mode ref deltasa for loop filter
        vpx_memset(xd->ref_lf_deltas, 0, sizeof(xd->ref_lf_deltas));
        vpx_memset(xd->mode_lf_deltas, 0, sizeof(xd->mode_lf_deltas));

        // All buffers are implicitly updated on key frames.
        pc->refresh_golden_frame = 1;
        pc->refresh_alt_ref_frame = 1;
        pc->copy_buffer_to_gf = 0;
        pc->copy_buffer_to_arf = 0;

        // Note that Golden and Altref modes cannot be used on a key frame so
        // ref_frame_sign_bias[] is undefined and meaningless
        pc->ref_frame_sign_bias[GOLDEN_FRAME] = 0;
        pc->ref_frame_sign_bias[ALTREF_FRAME] = 0;
    }
    else
    {
        if (!pc->use_bilinear_mc_filter)
            pc->mcomp_filter_type = SIXTAP;
        else
            pc->mcomp_filter_type = BILINEAR;

        // To enable choice of different interploation filters
        if (pc->mcomp_filter_type == SIXTAP)
        {
            xd->subpixel_predict      = SUBPIX_INVOKE(RTCD_VTABLE(subpix), sixtap4x4);
            xd->subpixel_predict8x4   = SUBPIX_INVOKE(RTCD_VTABLE(subpix), sixtap8x4);
            xd->subpixel_predict8x8   = SUBPIX_INVOKE(RTCD_VTABLE(subpix), sixtap8x8);
            xd->subpixel_predict16x16 = SUBPIX_INVOKE(RTCD_VTABLE(subpix), sixtap16x16);
        }
        else
        {
            xd->subpixel_predict      = SUBPIX_INVOKE(RTCD_VTABLE(subpix), bilinear4x4);
            xd->subpixel_predict8x4   = SUBPIX_INVOKE(RTCD_VTABLE(subpix), bilinear8x4);
            xd->subpixel_predict8x8   = SUBPIX_INVOKE(RTCD_VTABLE(subpix), bilinear8x8);
            xd->subpixel_predict16x16 = SUBPIX_INVOKE(RTCD_VTABLE(subpix), bilinear16x16);
        }
    }

    xd->left_context = &pc->left_context;
    xd->mode_info_context = pc->mi;
    xd->frame_type = pc->frame_type;
    xd->mode_info_context->mbmi.mode = DC_PRED;
    xd->mode_info_stride = pc->mode_info_stride;
}

int vp8_decode_frame(VP8D_COMP *pbi)
{
    vp8_reader *const bc = & pbi->bc;
    VP8_COMMON *const pc = & pbi->common;
    MACROBLOCKD *const xd  = & pbi->mb;
    const unsigned char *data = (const unsigned char *)pbi->Source;
    const unsigned char *const data_end = data + pbi->source_sz;
    unsigned int first_partition_length_in_bytes;

    int mb_row;
    int i, j, k, l;
    const int *const mb_feature_data_bits = vp8_mb_feature_data_bits;

    if (data_end - data < 3)
        vpx_internal_error(&pc->error, VPX_CODEC_CORRUPT_FRAME,
                           "Truncated packet");
    pc->frame_type = (FRAME_TYPE)(data[0] & 1);
    pc->version = (data[0] >> 1) & 7;
    pc->show_frame = (data[0] >> 4) & 1;
    first_partition_length_in_bytes =
        (data[0] | (data[1] << 8) | (data[2] << 16)) >> 5;
    data += 3;

    if (data_end - data < first_partition_length_in_bytes)
        vpx_internal_error(&pc->error, VPX_CODEC_CORRUPT_FRAME,
                           "Truncated packet or corrupt partition 0 length");
    vp8_setup_version(pc);

    if (pc->frame_type == KEY_FRAME)
    {
        const int Width = pc->Width;
        const int Height = pc->Height;

        // vet via sync code
        if (data[0] != 0x9d || data[1] != 0x01 || data[2] != 0x2a)
            vpx_internal_error(&pc->error, VPX_CODEC_UNSUP_BITSTREAM,
                               "Invalid frame sync code");

        pc->Width = (data[3] | (data[4] << 8)) & 0x3fff;
        pc->horiz_scale = data[4] >> 6;
        pc->Height = (data[5] | (data[6] << 8)) & 0x3fff;
        pc->vert_scale = data[6] >> 6;
        data += 7;

        if (Width != pc->Width  ||  Height != pc->Height)
        {
            if (pc->Width <= 0)
            {
                pc->Width = Width;
                vpx_internal_error(&pc->error, VPX_CODEC_CORRUPT_FRAME,
                                   "Invalid frame width");
            }

            if (pc->Height <= 0)
            {
                pc->Height = Height;
                vpx_internal_error(&pc->error, VPX_CODEC_CORRUPT_FRAME,
                                   "Invalid frame height");
            }

            if (vp8_alloc_frame_buffers(pc, pc->Width, pc->Height))
                vpx_internal_error(&pc->error, VPX_CODEC_MEM_ERROR,
                                   "Failed to allocate frame buffers");
        }
    }

    if (pc->Width == 0 || pc->Height == 0)
    {
        return -1;
    }

    init_frame(pbi);

    if (vp8dx_start_decode(bc, IF_RTCD(&pbi->dboolhuff),
                           data, data_end - data))
        vpx_internal_error(&pc->error, VPX_CODEC_MEM_ERROR,
                           "Failed to allocate bool decoder 0");
    if (pc->frame_type == KEY_FRAME) {
        pc->clr_type    = (YUV_TYPE)vp8_read_bit(bc);
        pc->clamp_type  = (CLAMP_TYPE)vp8_read_bit(bc);
    }

    // Is segmentation enabled
    xd->segmentation_enabled = (unsigned char)vp8_read_bit(bc);

    if (xd->segmentation_enabled)
    {
        // Signal whether or not the segmentation map is being explicitly updated this frame.
        xd->update_mb_segmentation_map = (unsigned char)vp8_read_bit(bc);
        xd->update_mb_segmentation_data = (unsigned char)vp8_read_bit(bc);

        if (xd->update_mb_segmentation_data)
        {
            xd->mb_segement_abs_delta = (unsigned char)vp8_read_bit(bc);

            vpx_memset(xd->segment_feature_data, 0, sizeof(xd->segment_feature_data));

            // For each segmentation feature (Quant and loop filter level)
            for (i = 0; i < MB_LVL_MAX; i++)
            {
                for (j = 0; j < MAX_MB_SEGMENTS; j++)
                {
                    // Frame level data
                    if (vp8_read_bit(bc))
                    {
                        xd->segment_feature_data[i][j] = (signed char)vp8_read_literal(bc, mb_feature_data_bits[i]);

                        if (vp8_read_bit(bc))
                            xd->segment_feature_data[i][j] = -xd->segment_feature_data[i][j];
                    }
                    else
                        xd->segment_feature_data[i][j] = 0;
                }
            }
        }

        if (xd->update_mb_segmentation_map)
        {
            // Which macro block level features are enabled
            vpx_memset(xd->mb_segment_tree_probs, 255, sizeof(xd->mb_segment_tree_probs));

            // Read the probs used to decode the segment id for each macro block.
            for (i = 0; i < MB_FEATURE_TREE_PROBS; i++)
            {
                // If not explicitly set value is defaulted to 255 by memset above
                if (vp8_read_bit(bc))
                    xd->mb_segment_tree_probs[i] = (vp8_prob)vp8_read_literal(bc, 8);
            }
        }
    }

    // Read the loop filter level and type
    pc->filter_type = (LOOPFILTERTYPE) vp8_read_bit(bc);
    pc->filter_level = vp8_read_literal(bc, 6);
    pc->sharpness_level = vp8_read_literal(bc, 3);

    // Read in loop filter deltas applied at the MB level based on mode or ref frame.
    xd->mode_ref_lf_delta_update = 0;
    xd->mode_ref_lf_delta_enabled = (unsigned char)vp8_read_bit(bc);

    if (xd->mode_ref_lf_delta_enabled)
    {
        // Do the deltas need to be updated
        xd->mode_ref_lf_delta_update = (unsigned char)vp8_read_bit(bc);

        if (xd->mode_ref_lf_delta_update)
        {
            // Send update
            for (i = 0; i < MAX_REF_LF_DELTAS; i++)
            {
                if (vp8_read_bit(bc))
                {
                    //sign = vp8_read_bit( bc );
                    xd->ref_lf_deltas[i] = (signed char)vp8_read_literal(bc, 6);

                    if (vp8_read_bit(bc))        // Apply sign
                        xd->ref_lf_deltas[i] = xd->ref_lf_deltas[i] * -1;
                }
            }

            // Send update
            for (i = 0; i < MAX_MODE_LF_DELTAS; i++)
            {
                if (vp8_read_bit(bc))
                {
                    //sign = vp8_read_bit( bc );
                    xd->mode_lf_deltas[i] = (signed char)vp8_read_literal(bc, 6);

                    if (vp8_read_bit(bc))        // Apply sign
                        xd->mode_lf_deltas[i] = xd->mode_lf_deltas[i] * -1;
                }
            }
        }
    }

    setup_token_decoder(pbi, data + first_partition_length_in_bytes);
    xd->current_bc = &pbi->bc2;

    // Read the default quantizers.
    {
        int Q, q_update;

        Q = vp8_read_literal(bc, 7);  // AC 1st order Q = default
        pc->base_qindex = Q;
        q_update = 0;
        pc->y1dc_delta_q = get_delta_q(bc, pc->y1dc_delta_q, &q_update);
        pc->y2dc_delta_q = get_delta_q(bc, pc->y2dc_delta_q, &q_update);
        pc->y2ac_delta_q = get_delta_q(bc, pc->y2ac_delta_q, &q_update);
        pc->uvdc_delta_q = get_delta_q(bc, pc->uvdc_delta_q, &q_update);
        pc->uvac_delta_q = get_delta_q(bc, pc->uvac_delta_q, &q_update);

        if (q_update)
            vp8cx_init_de_quantizer(pbi);

        // MB level dequantizer setup
        mb_init_dequantizer(pbi, &pbi->mb);
    }

    // Determine if the golden frame or ARF buffer should be updated and how.
    // For all non key frames the GF and ARF refresh flags and sign bias
    // flags must be set explicitly.
    if (pc->frame_type != KEY_FRAME)
    {
        // Should the GF or ARF be updated from the current frame
        pc->refresh_golden_frame = vp8_read_bit(bc);
        pc->refresh_alt_ref_frame = vp8_read_bit(bc);

        // Buffer to buffer copy flags.
        pc->copy_buffer_to_gf = 0;

        if (!pc->refresh_golden_frame)
            pc->copy_buffer_to_gf = vp8_read_literal(bc, 2);

        pc->copy_buffer_to_arf = 0;

        if (!pc->refresh_alt_ref_frame)
            pc->copy_buffer_to_arf = vp8_read_literal(bc, 2);

        pc->ref_frame_sign_bias[GOLDEN_FRAME] = vp8_read_bit(bc);
        pc->ref_frame_sign_bias[ALTREF_FRAME] = vp8_read_bit(bc);
    }

    pc->refresh_entropy_probs = vp8_read_bit(bc);
    if (pc->refresh_entropy_probs == 0)
    {
        vpx_memcpy(&pc->lfc, &pc->fc, sizeof(pc->fc));
    }

    pc->refresh_last_frame = pc->frame_type == KEY_FRAME  ||  vp8_read_bit(bc);

    if (0)
    {
        FILE *z = fopen("decodestats.stt", "a");
        fprintf(z, "%6d F:%d,G:%d,A:%d,L:%d,Q:%d\n",
                pc->current_video_frame,
                pc->frame_type,
                pc->refresh_golden_frame,
                pc->refresh_alt_ref_frame,
                pc->refresh_last_frame,
                pc->base_qindex);
        fclose(z);
    }


    {
        // read coef probability tree

        for (i = 0; i < BLOCK_TYPES; i++)
            for (j = 0; j < COEF_BANDS; j++)
                for (k = 0; k < PREV_COEF_CONTEXTS; k++)
                    for (l = 0; l < MAX_ENTROPY_TOKENS - 1; l++)
                    {

                        vp8_prob *const p = pc->fc.coef_probs [i][j][k] + l;

                        if (vp8_read(bc, vp8_coef_update_probs [i][j][k][l]))
                        {
                            *p = (vp8_prob)vp8_read_literal(bc, 8);

                        }
                    }
    }

    vpx_memcpy(&xd->pre, &pc->yv12_fb[pc->lst_fb_idx], sizeof(YV12_BUFFER_CONFIG));
    vpx_memcpy(&xd->dst, &pc->yv12_fb[pc->new_fb_idx], sizeof(YV12_BUFFER_CONFIG));

    // set up frame new frame for intra coded blocks
    vp8_setup_intra_recon(&pc->yv12_fb[pc->new_fb_idx]);

    vp8_setup_block_dptrs(xd);

    vp8_build_block_doffsets(xd);

    // clear out the coeff buffer
    vpx_memset(xd->qcoeff, 0, sizeof(xd->qcoeff));

    // Read the mb_no_coeff_skip flag
    pc->mb_no_coeff_skip = (int)vp8_read_bit(bc);

    if (pc->frame_type == KEY_FRAME)
        vp8_kfread_modes(pbi);
    else
        vp8_decode_mode_mvs(pbi);

    vpx_memset(pc->above_context, 0, sizeof(ENTROPY_CONTEXT_PLANES) * pc->mb_cols);

    vpx_memcpy(&xd->block[0].bmi, &xd->mode_info_context->bmi[0], sizeof(B_MODE_INFO));


    if (pbi->b_multithreaded_lf && pc->filter_level != 0)
        vp8_start_lfthread(pbi);

    if (pbi->b_multithreaded_rd && pc->multi_token_partition != ONE_PARTITION)
    {
        vp8_mtdecode_mb_rows(pbi, xd);
    }
    else
    {
        int ibc = 0;
        int num_part = 1 << pc->multi_token_partition;

        // Decode the individual macro block
        for (mb_row = 0; mb_row < pc->mb_rows; mb_row++)
        {

            if (num_part > 1)
            {
                xd->current_bc = & pbi->mbc[ibc];
                ibc++;

                if (ibc == num_part)
                    ibc = 0;
            }

            vp8_decode_mb_row(pbi, pc, mb_row, xd);
        }

        pbi->last_mb_row_decoded = mb_row;
    }


    stop_token_decoder(pbi);

    // vpx_log("Decoder: Frame Decoded, Size Roughly:%d bytes  \n",bc->pos+pbi->bc2.pos);

    // If this was a kf or Gf note the Q used
    if ((pc->frame_type == KEY_FRAME) ||
         pc->refresh_golden_frame || pc->refresh_alt_ref_frame)
    {
        pc->last_kf_gf_q = pc->base_qindex;
    }

    if (pc->refresh_entropy_probs == 0)
    {
        vpx_memcpy(&pc->fc, &pc->lfc, sizeof(pc->fc));
    }

#ifdef PACKET_TESTING
    {
        FILE *f = fopen("decompressor.VP8", "ab");
        unsigned int size = pbi->bc2.pos + pbi->bc.pos + 8;
        fwrite((void *) &size, 4, 1, f);
        fwrite((void *) pbi->Source, size, 1, f);
        fclose(f);
    }
#endif

    return 0;
}
