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
#include "entropymode.h"
#include "findnearmv.h"


int vp8_read_bmode(vp8_reader *bc, const vp8_prob *p)
{
    const int i = vp8_treed_read(bc, vp8_bmode_tree, p);

    return i;
}


int vp8_read_ymode(vp8_reader *bc, const vp8_prob *p)
{
    const int i = vp8_treed_read(bc, vp8_ymode_tree, p);

    return i;
}

int vp8_kfread_ymode(vp8_reader *bc, const vp8_prob *p)
{
    const int i = vp8_treed_read(bc, vp8_kf_ymode_tree, p);

    return i;
}



int vp8_read_uv_mode(vp8_reader *bc, const vp8_prob *p)
{
    const int i = vp8_treed_read(bc, vp8_uv_mode_tree, p);

    return i;
}

void vp8_read_mb_features(vp8_reader *r, MB_MODE_INFO *mi, MACROBLOCKD *x)
{
    // Is segmentation enabled
    if (x->segmentation_enabled && x->update_mb_segmentation_map)
    {
        // If so then read the segment id.
        if (vp8_read(r, x->mb_segment_tree_probs[0]))
            mi->segment_id = (unsigned char)(2 + vp8_read(r, x->mb_segment_tree_probs[2]));
        else
            mi->segment_id = (unsigned char)(vp8_read(r, x->mb_segment_tree_probs[1]));
    }
}

void vp8_kfread_modes(VP8D_COMP *pbi)
{
    VP8_COMMON *const cp = & pbi->common;
    vp8_reader *const bc = & pbi->bc;

    MODE_INFO *m = cp->mi;
    const int ms = cp->mode_info_stride;

    int mb_row = -1;
    vp8_prob prob_skip_false = 0;

    if (cp->mb_no_coeff_skip)
        prob_skip_false = (vp8_prob)(vp8_read_literal(bc, 8));

    while (++mb_row < cp->mb_rows)
    {
        int mb_col = -1;

        while (++mb_col < cp->mb_cols)
        {
            MB_PREDICTION_MODE y_mode;

            // Read the Macroblock segmentation map if it is being updated explicitly this frame (reset to 0 above by default)
            // By default on a key frame reset all MBs to segment 0
            m->mbmi.segment_id = 0;

            if (pbi->mb.update_mb_segmentation_map)
                vp8_read_mb_features(bc, &m->mbmi, &pbi->mb);

            // Read the macroblock coeff skip flag if this feature is in use, else default to 0
            if (cp->mb_no_coeff_skip)
                m->mbmi.mb_skip_coeff = vp8_read(bc, prob_skip_false);
            else
                m->mbmi.mb_skip_coeff = 0;

            y_mode = (MB_PREDICTION_MODE) vp8_kfread_ymode(bc, cp->kf_ymode_prob);

            m->mbmi.ref_frame = INTRA_FRAME;

            if ((m->mbmi.mode = y_mode) == B_PRED)
            {
                int i = 0;

                do
                {
                    const B_PREDICTION_MODE A = vp8_above_bmi(m, i, ms)->mode;
                    const B_PREDICTION_MODE L = vp8_left_bmi(m, i)->mode;

                    m->bmi[i].mode = (B_PREDICTION_MODE) vp8_read_bmode(bc, cp->kf_bmode_prob [A] [L]);
                }
                while (++i < 16);
            }
            else
            {
                int BMode;
                int i = 0;

                switch (y_mode)
                {
                case DC_PRED:
                    BMode = B_DC_PRED;
                    break;
                case V_PRED:
                    BMode = B_VE_PRED;
                    break;
                case H_PRED:
                    BMode = B_HE_PRED;
                    break;
                case TM_PRED:
                    BMode = B_TM_PRED;
                    break;
                default:
                    BMode = B_DC_PRED;
                    break;
                }

                do
                {
                    m->bmi[i].mode = (B_PREDICTION_MODE)BMode;
                }
                while (++i < 16);
            }

            (m++)->mbmi.uv_mode = (MB_PREDICTION_MODE)vp8_read_uv_mode(bc, cp->kf_uv_mode_prob);
        }

        m++; // skip the border
    }
}
