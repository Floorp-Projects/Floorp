/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2018, Two Orioles, LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>

#include "dav1d/data.h"

#include "common/intops.h"

#include "src/decode.h"
#include "src/getbits.h"
#include "src/levels.h"
#include "src/obu.h"
#include "src/ref.h"
#include "src/warpmv.h"

static int parse_seq_hdr(Dav1dContext *const c, GetBits *const gb) {
    const uint8_t *const init_ptr = gb->ptr;
    Av1SequenceHeader *const hdr = &c->seq_hdr;

#define DEBUG_SEQ_HDR 0

    hdr->profile = dav1d_get_bits(gb, 3);
    if (hdr->profile > 2) goto error;
#if DEBUG_SEQ_HDR
    printf("SEQHDR: post-profile: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    hdr->still_picture = dav1d_get_bits(gb, 1);
    hdr->reduced_still_picture_header = dav1d_get_bits(gb, 1);
    if (hdr->reduced_still_picture_header && !hdr->still_picture) goto error;
#if DEBUG_SEQ_HDR
    printf("SEQHDR: post-stillpicture_flags: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    if (hdr->reduced_still_picture_header) {
        hdr->timing_info_present = 0;
        hdr->decoder_model_info_present = 0;
        hdr->display_model_info_present = 0;
        hdr->num_operating_points = 1;
        hdr->operating_points[0].idc = 0;
        hdr->operating_points[0].major_level = dav1d_get_bits(gb, 3);
        hdr->operating_points[0].minor_level = dav1d_get_bits(gb, 2);
        hdr->operating_points[0].tier = 0;
        hdr->operating_points[0].decoder_model_param_present = 0;
        hdr->operating_points[0].display_model_param_present = 0;
    } else {
        hdr->timing_info_present = dav1d_get_bits(gb, 1);
        if (hdr->timing_info_present) {
            hdr->num_units_in_tick = dav1d_get_bits(gb, 32);
            hdr->time_scale = dav1d_get_bits(gb, 32);
            hdr->equal_picture_interval = dav1d_get_bits(gb, 1);
            if (hdr->equal_picture_interval)
                hdr->num_ticks_per_picture = dav1d_get_vlc(gb) + 1;

            hdr->decoder_model_info_present = dav1d_get_bits(gb, 1);
            if (hdr->decoder_model_info_present) {
                hdr->encoder_decoder_buffer_delay_length = dav1d_get_bits(gb, 5) + 1;
                hdr->num_units_in_decoding_tick = dav1d_get_bits(gb, 32);
                hdr->buffer_removal_delay_length = dav1d_get_bits(gb, 5) + 1;
                hdr->frame_presentation_delay_length = dav1d_get_bits(gb, 5) + 1;
            }
        } else {
            hdr->decoder_model_info_present = 0;
        }
#if DEBUG_SEQ_HDR
        printf("SEQHDR: post-timinginfo: off=%ld\n",
               (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

        hdr->display_model_info_present = dav1d_get_bits(gb, 1);
        hdr->num_operating_points = dav1d_get_bits(gb, 5) + 1;
        for (int i = 0; i < c->seq_hdr.num_operating_points; i++) {
            struct Av1SequenceHeaderOperatingPoint *const op =
                &hdr->operating_points[i];
            op->idc = dav1d_get_bits(gb, 12);
            op->major_level = 2 + dav1d_get_bits(gb, 3);
            op->minor_level = dav1d_get_bits(gb, 2);
            op->tier = op->major_level > 3 ? dav1d_get_bits(gb, 1) : 0;
            op->decoder_model_param_present =
                hdr->decoder_model_info_present && dav1d_get_bits(gb, 1);
            if (op->decoder_model_param_present) {
                op->decoder_buffer_delay =
                    dav1d_get_bits(gb, hdr->encoder_decoder_buffer_delay_length);
                op->encoder_buffer_delay =
                    dav1d_get_bits(gb, hdr->encoder_decoder_buffer_delay_length);
                op->low_delay_mode = dav1d_get_bits(gb, 1);
            }
            op->display_model_param_present =
                hdr->display_model_info_present && dav1d_get_bits(gb, 1);
            if (op->display_model_param_present) {
                op->initial_display_delay = dav1d_get_bits(gb, 4) + 1;
            }
        }
#if DEBUG_SEQ_HDR
        printf("SEQHDR: post-operating-points: off=%ld\n",
               (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif
    }

    hdr->width_n_bits = dav1d_get_bits(gb, 4) + 1;
    hdr->height_n_bits = dav1d_get_bits(gb, 4) + 1;
    hdr->max_width = dav1d_get_bits(gb, hdr->width_n_bits) + 1;
    hdr->max_height = dav1d_get_bits(gb, hdr->height_n_bits) + 1;
#if DEBUG_SEQ_HDR
    printf("SEQHDR: post-size: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif
    hdr->frame_id_numbers_present =
        hdr->reduced_still_picture_header ? 0 : dav1d_get_bits(gb, 1);
    if (hdr->frame_id_numbers_present) {
        hdr->delta_frame_id_n_bits = dav1d_get_bits(gb, 4) + 2;
        hdr->frame_id_n_bits = dav1d_get_bits(gb, 3) + hdr->delta_frame_id_n_bits + 1;
    }
#if DEBUG_SEQ_HDR
    printf("SEQHDR: post-frame-id-numbers-present: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    hdr->sb128 = dav1d_get_bits(gb, 1);
    hdr->filter_intra = dav1d_get_bits(gb, 1);
    hdr->intra_edge_filter = dav1d_get_bits(gb, 1);
    if (hdr->reduced_still_picture_header) {
        hdr->inter_intra = 0;
        hdr->masked_compound = 0;
        hdr->warped_motion = 0;
        hdr->dual_filter = 0;
        hdr->order_hint = 0;
        hdr->jnt_comp = 0;
        hdr->ref_frame_mvs = 0;
        hdr->order_hint_n_bits = 0;
        hdr->screen_content_tools = ADAPTIVE;
        hdr->force_integer_mv = ADAPTIVE;
    } else {
        hdr->inter_intra = dav1d_get_bits(gb, 1);
        hdr->masked_compound = dav1d_get_bits(gb, 1);
        hdr->warped_motion = dav1d_get_bits(gb, 1);
        hdr->dual_filter = dav1d_get_bits(gb, 1);
        hdr->order_hint = dav1d_get_bits(gb, 1);
        if (hdr->order_hint) {
            hdr->jnt_comp = dav1d_get_bits(gb, 1);
            hdr->ref_frame_mvs = dav1d_get_bits(gb, 1);
        } else {
            hdr->jnt_comp = 0;
            hdr->ref_frame_mvs = 0;
            hdr->order_hint_n_bits = 0;
        }
        hdr->screen_content_tools = dav1d_get_bits(gb, 1) ? ADAPTIVE : dav1d_get_bits(gb, 1);
    #if DEBUG_SEQ_HDR
        printf("SEQHDR: post-screentools: off=%ld\n",
               (gb->ptr - init_ptr) * 8 - gb->bits_left);
    #endif
        hdr->force_integer_mv = hdr->screen_content_tools ?
                                dav1d_get_bits(gb, 1) ? ADAPTIVE : dav1d_get_bits(gb, 1) : 2;
        if (hdr->order_hint)
            hdr->order_hint_n_bits = dav1d_get_bits(gb, 3) + 1;
    }
    hdr->super_res = dav1d_get_bits(gb, 1);
    hdr->cdef = dav1d_get_bits(gb, 1);
    hdr->restoration = dav1d_get_bits(gb, 1);
#if DEBUG_SEQ_HDR
    printf("SEQHDR: post-featurebits: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    const int hbd = dav1d_get_bits(gb, 1);
    hdr->bpc = hdr->profile == 2 && hbd ? 10U + 2 * dav1d_get_bits(gb, 1) : 8U + 2 * hbd;
    hdr->hbd = hdr->bpc > 8;
    const int monochrome = hdr->profile != 1 ? dav1d_get_bits(gb, 1) : 0;
    hdr->color_description_present = dav1d_get_bits(gb, 1);
    if (hdr->color_description_present) {
        hdr->pri = dav1d_get_bits(gb, 8);
        hdr->trc = dav1d_get_bits(gb, 8);
        hdr->mtrx = dav1d_get_bits(gb, 8);
    } else {
        hdr->pri = DAV1D_COLOR_PRI_UNKNOWN;
        hdr->trc = DAV1D_TRC_UNKNOWN;
        hdr->mtrx = DAV1D_MC_UNKNOWN;
    }
    if (monochrome) {
        hdr->color_range = dav1d_get_bits(gb, 1);
        hdr->layout = DAV1D_PIXEL_LAYOUT_I400;
        hdr->chr = DAV1D_CHR_UNKNOWN;
        hdr->separate_uv_delta_q = 0;
    } else if (hdr->pri == DAV1D_COLOR_PRI_BT709 &&
               hdr->trc == DAV1D_TRC_SRGB &&
               hdr->mtrx == DAV1D_MC_IDENTITY)
    {
        hdr->layout = DAV1D_PIXEL_LAYOUT_I444;
        hdr->color_range = 1;
        if (hdr->profile != 1 && !(hdr->profile == 2 && hdr->bpc == 12))
            goto error;
    } else {
        hdr->color_range = dav1d_get_bits(gb, 1);
        switch (hdr->profile) {
        case 0: hdr->layout = DAV1D_PIXEL_LAYOUT_I420; break;
        case 1: hdr->layout = DAV1D_PIXEL_LAYOUT_I444; break;
        case 2:
            if (hdr->bpc == 12) {
                hdr->layout = dav1d_get_bits(gb, 1) ?
                              dav1d_get_bits(gb, 1) ? DAV1D_PIXEL_LAYOUT_I420 :
                                                      DAV1D_PIXEL_LAYOUT_I422 :
                                                      DAV1D_PIXEL_LAYOUT_I444;
            } else
                hdr->layout = DAV1D_PIXEL_LAYOUT_I422;
            break;
        }
        if (hdr->layout == DAV1D_PIXEL_LAYOUT_I420)
            hdr->chr = dav1d_get_bits(gb, 2);
        hdr->separate_uv_delta_q = dav1d_get_bits(gb, 1);
    }
#if DEBUG_SEQ_HDR
    printf("SEQHDR: post-colorinfo: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    c->seq_hdr.film_grain_present = dav1d_get_bits(gb, 1);
#if DEBUG_SEQ_HDR
    printf("SEQHDR: post-filmgrain: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    dav1d_get_bits(gb, 1); // dummy bit

    return dav1d_flush_get_bits(gb) - init_ptr;

error:
    fprintf(stderr, "Error parsing sequence header\n");
    return -EINVAL;
}

static int read_frame_size(Dav1dContext *const c, GetBits *const gb,
                           const int use_ref)
{
    const Av1SequenceHeader *const seqhdr = &c->seq_hdr;
    Av1FrameHeader *const hdr = &c->frame_hdr;

    if (use_ref) {
        for (int i = 0; i < 7; i++) {
            if (dav1d_get_bits(gb, 1)) {
                Dav1dThreadPicture *const ref =
                    &c->refs[c->frame_hdr.refidx[i]].p;
                if (!ref->p.data[0]) return -1;
                // FIXME render_* may be wrong
                hdr->render_width = hdr->width = ref->p.p.w;
                hdr->render_height = hdr->height = ref->p.p.h;
                hdr->super_res = 0; // FIXME probably wrong
                return 0;
            }
        }
    }

    if (hdr->frame_size_override) {
        hdr->width = dav1d_get_bits(gb, seqhdr->width_n_bits) + 1;
        hdr->height = dav1d_get_bits(gb, seqhdr->height_n_bits) + 1;
    } else {
        hdr->width = seqhdr->max_width;
        hdr->height = seqhdr->max_height;
    }
    hdr->super_res = seqhdr->super_res && dav1d_get_bits(gb, 1);
    if (hdr->super_res) return -1; // FIXME
    hdr->have_render_size = dav1d_get_bits(gb, 1);
    if (hdr->have_render_size) {
        hdr->render_width = dav1d_get_bits(gb, seqhdr->width_n_bits) + 1;
        hdr->render_height = dav1d_get_bits(gb, seqhdr->height_n_bits) + 1;
    } else {
        hdr->render_width = hdr->width;
        hdr->render_height = hdr->height;
    }
    return 0;
}

static inline int tile_log2(int sz, int tgt) {
    int k;
    for (k = 0; (sz << k) < tgt; k++) ;
    return k;
}

static const Av1LoopfilterModeRefDeltas default_mode_ref_deltas = {
    .mode_delta = { 0, 0 },
    .ref_delta = { 1, 0, 0, 0, -1, 0, -1, -1 },
};

static int parse_frame_hdr(Dav1dContext *const c, GetBits *const gb,
                           const int have_trailing_bit)
{
    const uint8_t *const init_ptr = gb->ptr;
    const Av1SequenceHeader *const seqhdr = &c->seq_hdr;
    Av1FrameHeader *const hdr = &c->frame_hdr;
    int res;

#define DEBUG_FRAME_HDR 0

    hdr->show_existing_frame =
        !seqhdr->reduced_still_picture_header && dav1d_get_bits(gb, 1);
#if DEBUG_FRAME_HDR
    printf("HDR: post-show_existing_frame: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif
    if (hdr->show_existing_frame) {
        hdr->existing_frame_idx = dav1d_get_bits(gb, 3);
        if (seqhdr->decoder_model_info_present && !seqhdr->equal_picture_interval)
            hdr->frame_presentation_delay = dav1d_get_bits(gb, seqhdr->frame_presentation_delay_length);
        if (seqhdr->frame_id_numbers_present)
            hdr->frame_id = dav1d_get_bits(gb, seqhdr->frame_id_n_bits);
        goto end;
    }

    hdr->frame_type = seqhdr->reduced_still_picture_header ? DAV1D_FRAME_TYPE_KEY : dav1d_get_bits(gb, 2);
    hdr->show_frame = seqhdr->reduced_still_picture_header || dav1d_get_bits(gb, 1);
    if (hdr->show_frame) {
        if (seqhdr->decoder_model_info_present && !seqhdr->equal_picture_interval)
            hdr->frame_presentation_delay = dav1d_get_bits(gb, seqhdr->frame_presentation_delay_length);
    } else
        hdr->showable_frame = dav1d_get_bits(gb, 1);
    hdr->error_resilient_mode =
        (hdr->frame_type == DAV1D_FRAME_TYPE_KEY && hdr->show_frame) ||
        hdr->frame_type == DAV1D_FRAME_TYPE_SWITCH ||
        seqhdr->reduced_still_picture_header || dav1d_get_bits(gb, 1);
#if DEBUG_FRAME_HDR
    printf("HDR: post-frametype_bits: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif
    hdr->disable_cdf_update = dav1d_get_bits(gb, 1);
    hdr->allow_screen_content_tools = seqhdr->screen_content_tools == ADAPTIVE ?
                                 dav1d_get_bits(gb, 1) : seqhdr->screen_content_tools;
    if (hdr->allow_screen_content_tools)
        hdr->force_integer_mv = seqhdr->force_integer_mv == ADAPTIVE ?
                                dav1d_get_bits(gb, 1) : seqhdr->force_integer_mv;
    else
        hdr->force_integer_mv = 0;

    if (!(hdr->frame_type & 1))
        hdr->force_integer_mv = 1;

    if (seqhdr->frame_id_numbers_present)
        hdr->frame_id = dav1d_get_bits(gb, seqhdr->frame_id_n_bits);

    hdr->frame_size_override = seqhdr->reduced_still_picture_header ? 0 :
                               hdr->frame_type == DAV1D_FRAME_TYPE_SWITCH ? 1 : dav1d_get_bits(gb, 1);
#if DEBUG_FRAME_HDR
    printf("HDR: post-frame_size_override_flag: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif
    hdr->frame_offset = seqhdr->order_hint ?
                        dav1d_get_bits(gb, seqhdr->order_hint_n_bits) : 0;
    hdr->primary_ref_frame = !hdr->error_resilient_mode && hdr->frame_type & 1 ?
                             dav1d_get_bits(gb, 3) : PRIMARY_REF_NONE;

    if (seqhdr->decoder_model_info_present) {
        hdr->buffer_removal_time_present = dav1d_get_bits(gb, 1);
        if (hdr->buffer_removal_time_present) {
            for (int i = 0; i < c->seq_hdr.num_operating_points; i++) {
                const struct Av1SequenceHeaderOperatingPoint *const seqop = &seqhdr->operating_points[i];
                struct Av1FrameHeaderOperatingPoint *const op = &hdr->operating_points[i];
                if (seqop->decoder_model_param_present) {
                    int in_temporal_layer = (seqop->idc >>  0 /* FIXME: temporal_id */ ) & 1;
                    int in_spatial_layer  = (seqop->idc >> (0 /* FIXME: spatial_id */ + 8)) & 1;
                    if (!seqop->idc || in_temporal_layer || in_spatial_layer)
                        op->buffer_removal_time = dav1d_get_bits(gb, seqhdr->buffer_removal_delay_length);
                }
            }
        }
    }

    if (hdr->frame_type == DAV1D_FRAME_TYPE_KEY ||
        hdr->frame_type == DAV1D_FRAME_TYPE_INTRA)
    {
        hdr->refresh_frame_flags = (hdr->frame_type == DAV1D_FRAME_TYPE_KEY &&
                                    hdr->show_frame) ? 0xff : dav1d_get_bits(gb, 8);
        if (hdr->refresh_frame_flags != 0xff && hdr->error_resilient_mode && seqhdr->order_hint)
            for (int i = 0; i < 8; i++)
                dav1d_get_bits(gb, seqhdr->order_hint_n_bits);
        if ((res = read_frame_size(c, gb, 0)) < 0) goto error;
        hdr->allow_intrabc = hdr->allow_screen_content_tools &&
                             /* FIXME: no superres scaling && */ dav1d_get_bits(gb, 1);
        hdr->use_ref_frame_mvs = 0;
    } else {
        hdr->allow_intrabc = 0;
        hdr->refresh_frame_flags = hdr->frame_type == DAV1D_FRAME_TYPE_SWITCH ? 0xff :
                                   dav1d_get_bits(gb, 8);
        if (hdr->error_resilient_mode && seqhdr->order_hint)
            for (int i = 0; i < 8; i++)
                dav1d_get_bits(gb, seqhdr->order_hint_n_bits);
        hdr->frame_ref_short_signaling =
            seqhdr->order_hint && dav1d_get_bits(gb, 1);
        if (hdr->frame_ref_short_signaling) goto error; // FIXME
        for (int i = 0; i < 7; i++) {
            hdr->refidx[i] = dav1d_get_bits(gb, 3);
            if (seqhdr->frame_id_numbers_present)
                dav1d_get_bits(gb, seqhdr->delta_frame_id_n_bits);
        }
        const int use_ref = !hdr->error_resilient_mode &&
                            hdr->frame_size_override;
        if ((res = read_frame_size(c, gb, use_ref)) < 0) goto error;
        hdr->hp = !hdr->force_integer_mv && dav1d_get_bits(gb, 1);
        hdr->subpel_filter_mode = dav1d_get_bits(gb, 1) ? FILTER_SWITCHABLE :
                                                          dav1d_get_bits(gb, 2);
        hdr->switchable_motion_mode = dav1d_get_bits(gb, 1);
        hdr->use_ref_frame_mvs = !hdr->error_resilient_mode &&
            seqhdr->ref_frame_mvs && seqhdr->order_hint &&
            hdr->frame_type & 1 && dav1d_get_bits(gb, 1);
    }
#if DEBUG_FRAME_HDR
    printf("HDR: post-frametype-specific-bits: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    hdr->refresh_context = !seqhdr->reduced_still_picture_header &&
                           !hdr->disable_cdf_update && !dav1d_get_bits(gb, 1);
#if DEBUG_FRAME_HDR
    printf("HDR: post-refresh_context: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    // tile data
    hdr->tiling.uniform = dav1d_get_bits(gb, 1);
    const int sbsz_min1 = (64 << seqhdr->sb128) - 1;
    int sbsz_log2 = 6 + seqhdr->sb128;
    int sbw = (hdr->width + sbsz_min1) >> sbsz_log2;
    int sbh = (hdr->height + sbsz_min1) >> sbsz_log2;
    int max_tile_width_sb = 4096 >> sbsz_log2, max_tile_height_sb;
    int max_tile_area_sb = 4096 * 2304 >> (2 * sbsz_log2);
    hdr->tiling.min_log2_cols = tile_log2(max_tile_width_sb, sbw);
    hdr->tiling.max_log2_cols = tile_log2(1, imin(sbw, 1024));
    hdr->tiling.max_log2_rows = tile_log2(1, imin(sbh, 1024));
    int min_log2_tiles = imax(tile_log2(max_tile_area_sb, sbw * sbh),
                              hdr->tiling.min_log2_cols);
    if (hdr->tiling.uniform) {
        for (hdr->tiling.log2_cols = hdr->tiling.min_log2_cols;
             hdr->tiling.log2_cols < hdr->tiling.max_log2_cols && dav1d_get_bits(gb, 1);
             hdr->tiling.log2_cols++) ;
        const int tile_w = 1 + ((sbw - 1) >> hdr->tiling.log2_cols);
        hdr->tiling.cols = 0;
        for (int sbx = 0; sbx < sbw; sbx += tile_w, hdr->tiling.cols++)
            hdr->tiling.col_start_sb[hdr->tiling.cols] = sbx;
        hdr->tiling.min_log2_rows =
            imax(min_log2_tiles - hdr->tiling.log2_cols, 0);
        max_tile_height_sb = sbh >> hdr->tiling.min_log2_rows;

        for (hdr->tiling.log2_rows = hdr->tiling.min_log2_rows;
             hdr->tiling.log2_rows < hdr->tiling.max_log2_rows && dav1d_get_bits(gb, 1);
             hdr->tiling.log2_rows++) ;
        const int tile_h = 1 + ((sbh - 1) >> hdr->tiling.log2_rows);
        hdr->tiling.rows = 0;
        for (int sby = 0; sby < sbh; sby += tile_h, hdr->tiling.rows++)
            hdr->tiling.row_start_sb[hdr->tiling.rows] = sby;
    } else {
        hdr->tiling.cols = 0;
        int widest_tile = 0, max_tile_area_sb = sbw * sbh;
        for (int sbx = 0; sbx < sbw; hdr->tiling.cols++) {
            const int tile_width_sb = imin(sbw - sbx, max_tile_width_sb);
            const int tile_w = (tile_width_sb > 1) ?
                                   1 + dav1d_get_uniform(gb, tile_width_sb) :
                                   1;
            hdr->tiling.col_start_sb[hdr->tiling.cols] = sbx;
            sbx += tile_w;
            widest_tile = imax(widest_tile, tile_w);
        }
        hdr->tiling.log2_cols = tile_log2(1, hdr->tiling.cols);
        if (min_log2_tiles) max_tile_area_sb >>= min_log2_tiles + 1;
        max_tile_height_sb = imax(max_tile_area_sb / widest_tile, 1);

        hdr->tiling.rows = 0;
        for (int sby = 0; sby < sbh; hdr->tiling.rows++) {
            const int tile_height_sb = imin(sbh - sby, max_tile_height_sb);
            const int tile_h = (tile_height_sb > 1) ?
                                   1 + dav1d_get_uniform(gb, tile_height_sb) :
                                   1;
            hdr->tiling.row_start_sb[hdr->tiling.rows] = sby;
            sby += tile_h;
        }
        hdr->tiling.log2_rows = tile_log2(1, hdr->tiling.rows);
    }
    hdr->tiling.col_start_sb[hdr->tiling.cols] = sbw;
    hdr->tiling.row_start_sb[hdr->tiling.rows] = sbh;
    if (hdr->tiling.log2_cols || hdr->tiling.log2_rows) {
        hdr->tiling.update = dav1d_get_bits(gb, hdr->tiling.log2_cols +
                                                hdr->tiling.log2_rows);
        if (hdr->tiling.update >= hdr->tiling.cols * hdr->tiling.rows)
            goto error;
        hdr->tiling.n_bytes = dav1d_get_bits(gb, 2) + 1;
    } else {
        hdr->tiling.n_bytes = hdr->tiling.update = 0;
    }
#if DEBUG_FRAME_HDR
    printf("HDR: post-tiling: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    // quant data
    hdr->quant.yac = dav1d_get_bits(gb, 8);
    hdr->quant.ydc_delta = dav1d_get_bits(gb, 1) ? dav1d_get_sbits(gb, 6) : 0;
    if (seqhdr->layout != DAV1D_PIXEL_LAYOUT_I400) {
        hdr->quant.udc_delta = dav1d_get_bits(gb, 1) ? dav1d_get_sbits(gb, 6) : 0;
        hdr->quant.uac_delta = dav1d_get_bits(gb, 1) ? dav1d_get_sbits(gb, 6) : 0;
        if (seqhdr->separate_uv_delta_q) {
            hdr->quant.vdc_delta = dav1d_get_bits(gb, 1) ? dav1d_get_sbits(gb, 6) : 0;
            hdr->quant.vac_delta = dav1d_get_bits(gb, 1) ? dav1d_get_sbits(gb, 6) : 0;
        } else {
            hdr->quant.vdc_delta = hdr->quant.udc_delta;
            hdr->quant.vac_delta = hdr->quant.uac_delta;
        }
    }
#if DEBUG_FRAME_HDR
    printf("HDR: post-quant: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif
    hdr->quant.qm = dav1d_get_bits(gb, 1);
    if (hdr->quant.qm) {
        hdr->quant.qm_y = dav1d_get_bits(gb, 4);
        hdr->quant.qm_u = dav1d_get_bits(gb, 4);
        hdr->quant.qm_v =
            seqhdr->separate_uv_delta_q ? (int)dav1d_get_bits(gb, 4) :
                                          hdr->quant.qm_u;
    }
#if DEBUG_FRAME_HDR
    printf("HDR: post-qm: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    // segmentation data
    hdr->segmentation.enabled = dav1d_get_bits(gb, 1);
    if (hdr->segmentation.enabled) {
        if (hdr->primary_ref_frame == PRIMARY_REF_NONE) {
            hdr->segmentation.update_map = 1;
            hdr->segmentation.temporal = 0;
            hdr->segmentation.update_data = 1;
        } else {
            hdr->segmentation.update_map = dav1d_get_bits(gb, 1);
            hdr->segmentation.temporal =
                hdr->segmentation.update_map ? dav1d_get_bits(gb, 1) : 0;
            hdr->segmentation.update_data = dav1d_get_bits(gb, 1);
        }

        if (hdr->segmentation.update_data) {
            hdr->segmentation.seg_data.preskip = 0;
            hdr->segmentation.seg_data.last_active_segid = -1;
            for (int i = 0; i < NUM_SEGMENTS; i++) {
                Av1SegmentationData *const seg =
                    &hdr->segmentation.seg_data.d[i];
                if (dav1d_get_bits(gb, 1)) {
                    seg->delta_q = dav1d_get_sbits(gb, 8);
                    hdr->segmentation.seg_data.last_active_segid = i;
                } else {
                    seg->delta_q = 0;
                }
                if (dav1d_get_bits(gb, 1)) {
                    seg->delta_lf_y_v = dav1d_get_sbits(gb, 6);
                    hdr->segmentation.seg_data.last_active_segid = i;
                } else {
                    seg->delta_lf_y_v = 0;
                }
                if (dav1d_get_bits(gb, 1)) {
                    seg->delta_lf_y_h = dav1d_get_sbits(gb, 6);
                    hdr->segmentation.seg_data.last_active_segid = i;
                } else {
                    seg->delta_lf_y_h = 0;
                }
                if (dav1d_get_bits(gb, 1)) {
                    seg->delta_lf_u = dav1d_get_sbits(gb, 6);
                    hdr->segmentation.seg_data.last_active_segid = i;
                } else {
                    seg->delta_lf_u = 0;
                }
                if (dav1d_get_bits(gb, 1)) {
                    seg->delta_lf_v = dav1d_get_sbits(gb, 6);
                    hdr->segmentation.seg_data.last_active_segid = i;
                } else {
                    seg->delta_lf_v = 0;
                }
                if (dav1d_get_bits(gb, 1)) {
                    seg->ref = dav1d_get_bits(gb, 3);
                    hdr->segmentation.seg_data.last_active_segid = i;
                    hdr->segmentation.seg_data.preskip = 1;
                } else {
                    seg->ref = -1;
                }
                if ((seg->skip = dav1d_get_bits(gb, 1))) {
                    hdr->segmentation.seg_data.last_active_segid = i;
                    hdr->segmentation.seg_data.preskip = 1;
                }
                if ((seg->globalmv = dav1d_get_bits(gb, 1))) {
                    hdr->segmentation.seg_data.last_active_segid = i;
                    hdr->segmentation.seg_data.preskip = 1;
                }
            }
        } else if (hdr->primary_ref_frame == PRIMARY_REF_NONE) {
            memset(&hdr->segmentation.seg_data, 0, sizeof(Av1SegmentationDataSet));
        } else {
            const int pri_ref = hdr->refidx[hdr->primary_ref_frame];
            hdr->segmentation.seg_data = c->refs[pri_ref].seg_data;
        }
    } else if (hdr->primary_ref_frame == PRIMARY_REF_NONE) {
        memset(&hdr->segmentation.seg_data, 0, sizeof(Av1SegmentationDataSet));
    } else {
        const int pri_ref = hdr->refidx[hdr->primary_ref_frame];
        hdr->segmentation.seg_data = c->refs[pri_ref].seg_data;
    }
#if DEBUG_FRAME_HDR
    printf("HDR: post-segmentation: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    // delta q
    hdr->delta.q.present = hdr->quant.yac ? dav1d_get_bits(gb, 1) : 0;
    hdr->delta.q.res_log2 = hdr->delta.q.present ? dav1d_get_bits(gb, 2) : 0;
    hdr->delta.lf.present = hdr->delta.q.present && !hdr->allow_intrabc &&
                            dav1d_get_bits(gb, 1);
    hdr->delta.lf.res_log2 = hdr->delta.lf.present ? dav1d_get_bits(gb, 2) : 0;
    hdr->delta.lf.multi = hdr->delta.lf.present ? dav1d_get_bits(gb, 1) : 0;
#if DEBUG_FRAME_HDR
    printf("HDR: post-delta_q_lf_flags: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    // derive lossless flags
    const int delta_lossless = !hdr->quant.ydc_delta && !hdr->quant.udc_delta &&
        !hdr->quant.uac_delta && !hdr->quant.vdc_delta && !hdr->quant.vac_delta;
    hdr->all_lossless = 1;
    for (int i = 0; i < NUM_SEGMENTS; i++) {
        hdr->segmentation.qidx[i] = hdr->segmentation.enabled ?
            iclip_u8(hdr->quant.yac + hdr->segmentation.seg_data.d[i].delta_q) :
            hdr->quant.yac;
        hdr->segmentation.lossless[i] =
            !hdr->segmentation.qidx[i] && delta_lossless;
        hdr->all_lossless &= hdr->segmentation.lossless[i];
    }

    // loopfilter
    if (hdr->all_lossless || hdr->allow_intrabc) {
        hdr->loopfilter.level_y[0] = hdr->loopfilter.level_y[1] = 0;
        hdr->loopfilter.level_u = hdr->loopfilter.level_v = 0;
        hdr->loopfilter.sharpness = 0;
        hdr->loopfilter.mode_ref_delta_enabled = 1;
        hdr->loopfilter.mode_ref_delta_update = 1;
        hdr->loopfilter.mode_ref_deltas = default_mode_ref_deltas;
    } else {
        hdr->loopfilter.level_y[0] = dav1d_get_bits(gb, 6);
        hdr->loopfilter.level_y[1] = dav1d_get_bits(gb, 6);
        if (seqhdr->layout != DAV1D_PIXEL_LAYOUT_I400 &&
            (hdr->loopfilter.level_y[0] || hdr->loopfilter.level_y[1]))
        {
            hdr->loopfilter.level_u = dav1d_get_bits(gb, 6);
            hdr->loopfilter.level_v = dav1d_get_bits(gb, 6);
        }
        hdr->loopfilter.sharpness = dav1d_get_bits(gb, 3);

        if (hdr->primary_ref_frame == PRIMARY_REF_NONE) {
            hdr->loopfilter.mode_ref_deltas = default_mode_ref_deltas;
        } else {
            const int ref = hdr->refidx[hdr->primary_ref_frame];
            hdr->loopfilter.mode_ref_deltas = c->refs[ref].lf_mode_ref_deltas;
        }
        hdr->loopfilter.mode_ref_delta_enabled = dav1d_get_bits(gb, 1);
        if (hdr->loopfilter.mode_ref_delta_enabled) {
            hdr->loopfilter.mode_ref_delta_update = dav1d_get_bits(gb, 1);
            if (hdr->loopfilter.mode_ref_delta_update) {
                for (int i = 0; i < 8; i++)
                    if (dav1d_get_bits(gb, 1))
                        hdr->loopfilter.mode_ref_deltas.ref_delta[i] =
                            dav1d_get_sbits(gb, 6);
                for (int i = 0; i < 2; i++)
                    if (dav1d_get_bits(gb, 1))
                        hdr->loopfilter.mode_ref_deltas.mode_delta[i] =
                            dav1d_get_sbits(gb, 6);
            }
        }
    }
#if DEBUG_FRAME_HDR
    printf("HDR: post-lpf: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    // cdef
    if (!hdr->all_lossless && seqhdr->cdef && !hdr->allow_intrabc) {
        hdr->cdef.damping = dav1d_get_bits(gb, 2) + 3;
        hdr->cdef.n_bits = dav1d_get_bits(gb, 2);
        for (int i = 0; i < (1 << hdr->cdef.n_bits); i++) {
            hdr->cdef.y_strength[i] = dav1d_get_bits(gb, 6);
            if (seqhdr->layout != DAV1D_PIXEL_LAYOUT_I400)
                hdr->cdef.uv_strength[i] = dav1d_get_bits(gb, 6);
        }
    } else {
        hdr->cdef.n_bits = 0;
        hdr->cdef.y_strength[0] = 0;
        hdr->cdef.uv_strength[0] = 0;
    }
#if DEBUG_FRAME_HDR
    printf("HDR: post-cdef: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    // restoration
    if (!hdr->all_lossless && seqhdr->restoration && !hdr->allow_intrabc) {
        hdr->restoration.type[0] = dav1d_get_bits(gb, 2);
        if (seqhdr->layout != DAV1D_PIXEL_LAYOUT_I400) {
            hdr->restoration.type[1] = dav1d_get_bits(gb, 2);
            hdr->restoration.type[2] = dav1d_get_bits(gb, 2);
        } else {
            hdr->restoration.type[1] =
            hdr->restoration.type[2] = RESTORATION_NONE;
        }

        if (hdr->restoration.type[0] || hdr->restoration.type[1] ||
            hdr->restoration.type[2])
        {
            // Log2 of the restoration unit size.
            hdr->restoration.unit_size[0] = 6 + seqhdr->sb128;
            if (dav1d_get_bits(gb, 1)) {
                hdr->restoration.unit_size[0]++;
                if (!seqhdr->sb128)
                    hdr->restoration.unit_size[0] += dav1d_get_bits(gb, 1);
            }
            hdr->restoration.unit_size[1] = hdr->restoration.unit_size[0];
            if ((hdr->restoration.type[1] || hdr->restoration.type[2]) &&
                seqhdr->layout == DAV1D_PIXEL_LAYOUT_I420)
            {
                hdr->restoration.unit_size[1] -= dav1d_get_bits(gb, 1);
            }
        } else {
            hdr->restoration.unit_size[0] = 8;
        }
    } else {
        hdr->restoration.type[0] = RESTORATION_NONE;
        hdr->restoration.type[1] = RESTORATION_NONE;
        hdr->restoration.type[2] = RESTORATION_NONE;
    }
#if DEBUG_FRAME_HDR
    printf("HDR: post-restoration: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    hdr->txfm_mode = hdr->all_lossless ? TX_4X4_ONLY :
                     dav1d_get_bits(gb, 1) ? TX_SWITCHABLE : TX_LARGEST;
#if DEBUG_FRAME_HDR
    printf("HDR: post-txfmmode: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif
    hdr->switchable_comp_refs = hdr->frame_type & 1 ? dav1d_get_bits(gb, 1) : 0;
#if DEBUG_FRAME_HDR
    printf("HDR: post-refmode: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif
    hdr->skip_mode_allowed = 0;
    if (hdr->switchable_comp_refs && hdr->frame_type & 1 && seqhdr->order_hint) {
        const unsigned poc = hdr->frame_offset;
        unsigned off_before[2] = { 0xFFFFFFFF, 0xFFFFFFFF };
        int off_after = -1;
        int off_before_idx[2], off_after_idx;
        for (int i = 0; i < 7; i++) {
            const unsigned refpoc = c->refs[hdr->refidx[i]].p.p.poc;

            const int diff = get_poc_diff(seqhdr->order_hint_n_bits, refpoc, poc);
            if (diff > 0) {
                if (off_after == -1 || get_poc_diff(seqhdr->order_hint_n_bits,
                                                    off_after, refpoc) > 0)
                {
                    off_after = refpoc;
                    off_after_idx = i;
                }
            } else if (diff < 0) {
                if (off_before[0] == 0xFFFFFFFFU ||
                    get_poc_diff(seqhdr->order_hint_n_bits,
                                 refpoc, off_before[0]) > 0)
                {
                    off_before[1] = off_before[0];
                    off_before[0] = refpoc;
                    off_before_idx[1] = off_before_idx[0];
                    off_before_idx[0] = i;
                } else if (refpoc != off_before[0] &&
                           (off_before[1] == 0xFFFFFFFFU ||
                            get_poc_diff(seqhdr->order_hint_n_bits,
                                         refpoc, off_before[1]) > 0))
                {
                    off_before[1] = refpoc;
                    off_before_idx[1] = i;
                }
            }
        }

        if (off_before[0] != 0xFFFFFFFFU && off_after != -1) {
            hdr->skip_mode_refs[0] = imin(off_before_idx[0], off_after_idx);
            hdr->skip_mode_refs[1] = imax(off_before_idx[0], off_after_idx);
            hdr->skip_mode_allowed = 1;
        } else if (off_before[0] != 0xFFFFFFFFU &&
                   off_before[1] != 0xFFFFFFFFU)
        {
            hdr->skip_mode_refs[0] = imin(off_before_idx[0], off_before_idx[1]);
            hdr->skip_mode_refs[1] = imax(off_before_idx[0], off_before_idx[1]);
            hdr->skip_mode_allowed = 1;
        }
    }
    hdr->skip_mode_enabled = hdr->skip_mode_allowed ? dav1d_get_bits(gb, 1) : 0;
#if DEBUG_FRAME_HDR
    printf("HDR: post-extskip: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif
    hdr->warp_motion = !hdr->error_resilient_mode && hdr->frame_type & 1 &&
        seqhdr->warped_motion && dav1d_get_bits(gb, 1);
#if DEBUG_FRAME_HDR
    printf("HDR: post-warpmotionbit: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif
    hdr->reduced_txtp_set = dav1d_get_bits(gb, 1);
#if DEBUG_FRAME_HDR
    printf("HDR: post-reducedtxtpset: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    for (int i = 0; i < 7; i++)
        hdr->gmv[i] = dav1d_default_wm_params;

    if (hdr->frame_type & 1) {
        for (int i = 0; i < 7; i++) {
            hdr->gmv[i].type = !dav1d_get_bits(gb, 1) ? WM_TYPE_IDENTITY :
                                dav1d_get_bits(gb, 1) ? WM_TYPE_ROT_ZOOM :
                                dav1d_get_bits(gb, 1) ? WM_TYPE_TRANSLATION :
                                                  WM_TYPE_AFFINE;

            if (hdr->gmv[i].type == WM_TYPE_IDENTITY) continue;

            const WarpedMotionParams *const ref_gmv =
                hdr->primary_ref_frame == PRIMARY_REF_NONE ? &dav1d_default_wm_params :
                &c->refs[hdr->refidx[hdr->primary_ref_frame]].gmv[i];
            int32_t *const mat = hdr->gmv[i].matrix;
            const int32_t *const ref_mat = ref_gmv->matrix;
            int bits, shift;

            if (hdr->gmv[i].type >= WM_TYPE_ROT_ZOOM) {
                mat[2] = (1 << 16) + 2 *
                    dav1d_get_bits_subexp(gb, (ref_mat[2] - (1 << 16)) >> 1, 12);
                mat[3] = 2 * dav1d_get_bits_subexp(gb, ref_mat[3] >> 1, 12);

                bits = 12;
                shift = 10;
            } else {
                bits = 9 - !hdr->hp;
                shift = 13 + !hdr->hp;
            }

            if (hdr->gmv[i].type == WM_TYPE_AFFINE) {
                mat[4] = 2 * dav1d_get_bits_subexp(gb, ref_mat[4] >> 1, 12);
                mat[5] = (1 << 16) + 2 *
                    dav1d_get_bits_subexp(gb, (ref_mat[5] - (1 << 16)) >> 1, 12);
            } else {
                mat[4] = -mat[3];
                mat[5] = mat[2];
            }

            mat[0] = dav1d_get_bits_subexp(gb, ref_mat[0] >> shift, bits) * (1 << shift);
            mat[1] = dav1d_get_bits_subexp(gb, ref_mat[1] >> shift, bits) * (1 << shift);

            if (dav1d_get_shear_params(&hdr->gmv[i]))
                goto error;
        }
    }
#if DEBUG_FRAME_HDR
    printf("HDR: post-gmv: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

    hdr->film_grain.present = seqhdr->film_grain_present &&
                              (hdr->show_frame || hdr->showable_frame) &&
                              dav1d_get_bits(gb, 1);
    if (hdr->film_grain.present) {
        hdr->film_grain.seed = dav1d_get_bits(gb, 16);
        hdr->film_grain.update = hdr->frame_type != DAV1D_FRAME_TYPE_INTER || dav1d_get_bits(gb, 1);
        if (!hdr->film_grain.update) {
            const int refidx = dav1d_get_bits(gb, 3);
            int i;
            for (i = 0; i < 7; i++)
                if (hdr->refidx[i] == refidx)
                    break;
            if (i == 7) goto error;
            hdr->film_grain.data = c->refs[refidx].film_grain;
        } else {
            Av1FilmGrainData *const fgd = &hdr->film_grain.data;

            fgd->num_y_points = dav1d_get_bits(gb, 4);
            if (fgd->num_y_points > 14) goto error;
            for (int i = 0; i < fgd->num_y_points; i++) {
                fgd->y_points[i][0] = dav1d_get_bits(gb, 8);
                if (i && fgd->y_points[i - 1][0] >= fgd->y_points[i][0])
                    goto error;
                fgd->y_points[i][1] = dav1d_get_bits(gb, 8);
            }

            fgd->chroma_scaling_from_luma =
                seqhdr->layout != DAV1D_PIXEL_LAYOUT_I400 && dav1d_get_bits(gb, 1);
            if (seqhdr->layout == DAV1D_PIXEL_LAYOUT_I400 ||
                fgd->chroma_scaling_from_luma ||
                (seqhdr->layout == DAV1D_PIXEL_LAYOUT_I420 && !fgd->num_y_points))
            {
                fgd->num_uv_points[0] = fgd->num_uv_points[1] = 0;
            } else for (int pl = 0; pl < 2; pl++) {
                fgd->num_uv_points[pl] = dav1d_get_bits(gb, 4);
                if (fgd->num_uv_points[pl] > 10) goto error;
                for (int i = 0; i < fgd->num_uv_points[pl]; i++) {
                    fgd->uv_points[pl][i][0] = dav1d_get_bits(gb, 8);
                    if (i && fgd->uv_points[pl][i - 1][0] >= fgd->uv_points[pl][i][0])
                        goto error;
                    fgd->uv_points[pl][i][1] = dav1d_get_bits(gb, 8);
                }
            }

            if (seqhdr->layout == DAV1D_PIXEL_LAYOUT_I420 &&
                !!fgd->num_uv_points[0] != !!fgd->num_uv_points[1])
            {
                goto error;
            }

            fgd->scaling_shift = dav1d_get_bits(gb, 2) + 8;
            fgd->ar_coeff_lag = dav1d_get_bits(gb, 2);
            const int num_y_pos = 2 * fgd->ar_coeff_lag * (fgd->ar_coeff_lag + 1);
            if (fgd->num_y_points)
                for (int i = 0; i < num_y_pos; i++)
                    fgd->ar_coeffs_y[i] = dav1d_get_bits(gb, 8) - 128;
            for (int pl = 0; pl < 2; pl++)
                if (fgd->num_uv_points[pl] || fgd->chroma_scaling_from_luma) {
                    const int num_uv_pos = num_y_pos + !!fgd->num_y_points;
                    for (int i = 0; i < num_uv_pos; i++)
                        fgd->ar_coeffs_uv[pl][i] = dav1d_get_bits(gb, 8) - 128;
                }
            fgd->ar_coeff_shift = dav1d_get_bits(gb, 2) + 6;
            fgd->grain_scale_shift = dav1d_get_bits(gb, 2);
            for (int pl = 0; pl < 2; pl++)
                if (fgd->num_uv_points[pl]) {
                    fgd->uv_mult[pl] = dav1d_get_bits(gb, 8);
                    fgd->uv_luma_mult[pl] = dav1d_get_bits(gb, 8);
                    fgd->uv_offset[pl] = dav1d_get_bits(gb, 9);
                }
            fgd->overlap_flag = dav1d_get_bits(gb, 1);
            fgd->clip_to_restricted_range = dav1d_get_bits(gb, 1);
        }
    } else {
        memset(&hdr->film_grain.data, 0, sizeof(hdr->film_grain));
    }
#if DEBUG_FRAME_HDR
    printf("HDR: post-filmgrain: off=%ld\n",
           (gb->ptr - init_ptr) * 8 - gb->bits_left);
#endif

end:

    if (have_trailing_bit)
        dav1d_get_bits(gb, 1); // dummy bit

    return dav1d_flush_get_bits(gb) - init_ptr;

error:
    fprintf(stderr, "Error parsing frame header\n");
    return -EINVAL;
}

static int parse_tile_hdr(Dav1dContext *const c, GetBits *const gb) {
    const uint8_t *const init_ptr = gb->ptr;

    int have_tile_pos = 0;
    const int n_tiles = c->frame_hdr.tiling.cols * c->frame_hdr.tiling.rows;
    if (n_tiles > 1)
        have_tile_pos = dav1d_get_bits(gb, 1);

    if (have_tile_pos) {
        const int n_bits = c->frame_hdr.tiling.log2_cols +
                           c->frame_hdr.tiling.log2_rows;
        c->tile[c->n_tile_data].start = dav1d_get_bits(gb, n_bits);
        c->tile[c->n_tile_data].end = dav1d_get_bits(gb, n_bits);
    } else {
        c->tile[c->n_tile_data].start = 0;
        c->tile[c->n_tile_data].end = n_tiles - 1;
    }

    return dav1d_flush_get_bits(gb) - init_ptr;
}

int dav1d_parse_obus(Dav1dContext *const c, Dav1dData *const in) {
    GetBits gb;
    int res;

    dav1d_init_get_bits(&gb, in->data, in->sz);

    // obu header
    dav1d_get_bits(&gb, 1); // obu_forbidden_bit
    const enum ObuType type = dav1d_get_bits(&gb, 4);
    const int has_extension = dav1d_get_bits(&gb, 1);
    const int has_length_field = dav1d_get_bits(&gb, 1);
    if (!has_length_field) goto error;
    dav1d_get_bits(&gb, 1); // reserved
    if (has_extension) {
        dav1d_get_bits(&gb, 3); // temporal_layer_id
        dav1d_get_bits(&gb, 2); // enhancement_layer_id
        dav1d_get_bits(&gb, 3); // reserved
    }

    // obu length field
    unsigned len = 0, more, i = 0;
    do {
        more = dav1d_get_bits(&gb, 1);
        unsigned bits = dav1d_get_bits(&gb, 7);
        if (i <= 3 || (i == 4 && bits < (1 << 4)))
            len |= bits << (i * 7);
        else if (bits)
            goto error;
        if (more && ++i == 8) goto error;
    } while (more);
    if (gb.error) goto error;

    unsigned off = dav1d_flush_get_bits(&gb) - in->data;
    const unsigned init_off = off;
    if (len > in->sz - off) goto error;

    switch (type) {
    case OBU_SEQ_HDR:
        if ((res = parse_seq_hdr(c, &gb)) < 0)
            return res;
        if ((unsigned)res != len) goto error;
        c->have_seq_hdr = 1;
        c->have_frame_hdr = 0;
        break;
    case OBU_REDUNDANT_FRAME_HDR:
        if (c->have_frame_hdr) break;
        // fall-through
    case OBU_FRAME:
    case OBU_FRAME_HDR:
        if (!c->have_seq_hdr) goto error;
        if ((res = parse_frame_hdr(c, &gb, type != OBU_FRAME)) < 0)
            return res;
        c->have_frame_hdr = 1;
        for (int n = 0; n < c->n_tile_data; n++)
            dav1d_data_unref(&c->tile[n].data);
        c->n_tile_data = 0;
        c->n_tiles = 0;
        if (type != OBU_FRAME) break;
        if (c->frame_hdr.show_existing_frame) goto error;
        off += res;
        // fall-through
    case OBU_TILE_GRP:
        if (!c->have_frame_hdr) goto error;
        if (c->n_tile_data >= 256) goto error;
        if ((res = parse_tile_hdr(c, &gb)) < 0)
            return res;
        off += res;
        if (off > len + init_off)
            goto error;
        dav1d_ref_inc(in->ref);
        c->tile[c->n_tile_data].data.ref = in->ref;
        c->tile[c->n_tile_data].data.data = in->data + off;
        c->tile[c->n_tile_data].data.sz = len + init_off - off;
        // ensure tile groups are in order and sane, see 6.10.1
        if (c->tile[c->n_tile_data].start > c->tile[c->n_tile_data].end ||
            c->tile[c->n_tile_data].start != c->n_tiles)
        {
            for (int i = 0; i <= c->n_tile_data; i++)
                dav1d_data_unref(&c->tile[i].data);
            c->n_tile_data = 0;
            c->n_tiles = 0;
            goto error;
        }
        c->n_tiles += 1 + c->tile[c->n_tile_data].end -
                          c->tile[c->n_tile_data].start;
        c->n_tile_data++;
        break;
    case OBU_PADDING:
    case OBU_TD:
    case OBU_METADATA:
        // ignore OBUs we don't care about
        break;
    default:
        fprintf(stderr, "Unknown OBU type %d of size %u\n", type, len);
        return -EINVAL;
    }

    if (c->have_seq_hdr && c->have_frame_hdr &&
        c->n_tiles == c->frame_hdr.tiling.cols * c->frame_hdr.tiling.rows)
    {
        if (!c->n_tile_data)
            return -EINVAL;
        if ((res = dav1d_submit_frame(c)) < 0)
            return res;
        assert(!c->n_tile_data);
        c->have_frame_hdr = 0;
        c->n_tiles = 0;
    } else if (c->have_seq_hdr && c->have_frame_hdr &&
               c->frame_hdr.show_existing_frame)
    {
        if (c->n_fc == 1) {
            dav1d_picture_ref(&c->out,
                              &c->refs[c->frame_hdr.existing_frame_idx].p.p);
        } else {
            // need to append this to the frame output queue
            const unsigned next = c->frame_thread.next++;
            if (c->frame_thread.next == c->n_fc)
                c->frame_thread.next = 0;

            Dav1dFrameContext *const f = &c->fc[next];
            pthread_mutex_lock(&f->frame_thread.td.lock);
            while (f->n_tile_data > 0)
                pthread_cond_wait(&f->frame_thread.td.cond,
                                  &f->frame_thread.td.lock);
            Dav1dThreadPicture *const out_delayed =
                &c->frame_thread.out_delayed[next];
            if (out_delayed->p.data[0]) {
                if (out_delayed->visible && !out_delayed->flushed)
                    dav1d_picture_ref(&c->out, &out_delayed->p);
                dav1d_thread_picture_unref(out_delayed);
            }
            dav1d_thread_picture_ref(out_delayed,
                                     &c->refs[c->frame_hdr.existing_frame_idx].p);
            out_delayed->visible = 1;
            out_delayed->flushed = 0;
            pthread_mutex_unlock(&f->frame_thread.td.lock);
        }
        c->have_frame_hdr = 0;
        if (c->refs[c->frame_hdr.existing_frame_idx].p.p.p.type == DAV1D_FRAME_TYPE_KEY) {
            const int r = c->frame_hdr.existing_frame_idx;
            for (int i = 0; i < 8; i++) {
                if (i == c->frame_hdr.existing_frame_idx) continue;

                if (c->refs[i].p.p.data[0])
                    dav1d_thread_picture_unref(&c->refs[i].p);
                dav1d_thread_picture_ref(&c->refs[i].p, &c->refs[r].p);

                if (c->cdf[i].cdf) dav1d_cdf_thread_unref(&c->cdf[i]);
                dav1d_init_states(&c->cdf[i], c->refs[r].qidx);

                c->refs[i].lf_mode_ref_deltas = c->refs[r].lf_mode_ref_deltas;
                c->refs[i].seg_data = c->refs[r].seg_data;
                for (int j = 0; j < 7; j++)
                    c->refs[i].gmv[j] = dav1d_default_wm_params;
                c->refs[i].film_grain = c->refs[r].film_grain;

                if (c->refs[i].segmap)
                    dav1d_ref_dec(c->refs[i].segmap);
                c->refs[i].segmap = c->refs[r].segmap;
                if (c->refs[r].segmap)
                    dav1d_ref_inc(c->refs[r].segmap);
                if (c->refs[i].refmvs)
                    dav1d_ref_dec(c->refs[i].refmvs);
                c->refs[i].refmvs = NULL;
                c->refs[i].qidx = c->refs[r].qidx;
            }
        }
    }

    return len + init_off;

error:
    fprintf(stderr, "Error parsing OBU data\n");
    return -EINVAL;
}
