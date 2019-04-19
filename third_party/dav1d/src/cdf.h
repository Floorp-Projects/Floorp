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

#ifndef DAV1D_SRC_CDF_H
#define DAV1D_SRC_CDF_H

#include <stdint.h>

#include "src/levels.h"
#include "src/ref.h"
#include "src/thread_data.h"

/* Buffers padded to [8] or [16] for SIMD where needed. */

typedef struct CdfModeContext {
    uint16_t y_mode[4][N_INTRA_PRED_MODES + 1 + 2];
    uint16_t use_filter_intra[N_BS_SIZES][2];
    uint16_t filter_intra[5 + 1];
    uint16_t uv_mode[2][N_INTRA_PRED_MODES][N_UV_INTRA_PRED_MODES + 1 + 1];
    uint16_t angle_delta[8][8];
    uint16_t filter[2][8][DAV1D_N_SWITCHABLE_FILTERS + 1];
    uint16_t newmv_mode[6][2];
    uint16_t globalmv_mode[2][2];
    uint16_t refmv_mode[6][2];
    uint16_t drl_bit[3][2];
    uint16_t comp_inter_mode[8][N_COMP_INTER_PRED_MODES + 1];
    uint16_t intra[4][2];
    uint16_t comp[5][2];
    uint16_t comp_dir[5][2];
    uint16_t jnt_comp[6][2];
    uint16_t mask_comp[6][2];
    uint16_t wedge_comp[9][2];
    uint16_t wedge_idx[9][16 + 1];
    uint16_t interintra[7][2];
    uint16_t interintra_mode[4][5];
    uint16_t interintra_wedge[7][2];
    uint16_t ref[6][3][2];
    uint16_t comp_fwd_ref[3][3][2];
    uint16_t comp_bwd_ref[2][3][2];
    uint16_t comp_uni_ref[3][3][2];
    uint16_t txsz[N_TX_SIZES - 1][3][4];
    uint16_t txpart[7][3][2];
    uint16_t txtp_inter[4][N_TX_SIZES][N_TX_TYPES + 1];
    uint16_t txtp_intra[3][N_TX_SIZES][N_INTRA_PRED_MODES][N_TX_TYPES + 1];
    uint16_t skip[3][2];
    uint16_t skip_mode[3][2];
    uint16_t partition[N_BL_LEVELS][4][N_PARTITIONS + 1 + 5];
    uint16_t seg_pred[3][2];
    uint16_t seg_id[3][DAV1D_MAX_SEGMENTS + 1];
    uint16_t cfl_sign[8 + 1];
    uint16_t cfl_alpha[6][16 + 1];
    uint16_t restore_wiener[2];
    uint16_t restore_sgrproj[2];
    uint16_t restore_switchable[3 + 1];
    uint16_t delta_q[4 + 1];
    uint16_t delta_lf[5][4 + 1];
    uint16_t obmc[N_BS_SIZES][2];
    uint16_t motion_mode[N_BS_SIZES][3 + 1];
    uint16_t pal_y[7][3][2];
    uint16_t pal_uv[2][2];
    uint16_t pal_sz[2][7][7 + 1];
    uint16_t color_map[2][7][5][8 + 1];
    uint16_t intrabc[2];
} CdfModeContext;

typedef struct CdfCoefContext {
    uint16_t skip[N_TX_SIZES][13][2];
    uint16_t eob_bin_16[2][2][6];
    uint16_t eob_bin_32[2][2][7 + 1];
    uint16_t eob_bin_64[2][2][8];
    uint16_t eob_bin_128[2][2][9];
    uint16_t eob_bin_256[2][2][10 + 6];
    uint16_t eob_bin_512[2][2][11 + 5];
    uint16_t eob_bin_1024[2][2][12 + 4];
    uint16_t eob_hi_bit[N_TX_SIZES][2][11 /*22*/][2];
    uint16_t eob_base_tok[N_TX_SIZES][2][4][4];
    uint16_t base_tok[N_TX_SIZES][2][41][5];
    uint16_t dc_sign[2][3][2];
    uint16_t br_tok[4 /*5*/][2][21][5];
} CdfCoefContext;

typedef struct CdfMvComponent {
    uint16_t classes[11 + 1 + 4];
    uint16_t class0[2];
    uint16_t classN[10][2];
    uint16_t class0_fp[2][4 + 1];
    uint16_t classN_fp[4 + 1];
    uint16_t class0_hp[2];
    uint16_t classN_hp[2];
    uint16_t sign[2];
} CdfMvComponent;

typedef struct CdfMvContext {
    CdfMvComponent comp[2];
    uint16_t joint[N_MV_JOINTS + 1];
} CdfMvContext;

typedef struct CdfContext {
    CdfModeContext m;
    uint16_t kfym[5][5][N_INTRA_PRED_MODES + 1 + 2];
    CdfCoefContext coef;
    CdfMvContext mv, dmv;
} CdfContext;

typedef struct CdfThreadContext {
    Dav1dRef *ref; ///< allocation origin
    union {
        CdfContext *cdf; // if ref != NULL
        unsigned qcat; // if ref == NULL, from static CDF tables
    } data;
    struct thread_data *t;
    atomic_uint *progress;
} CdfThreadContext;

void dav1d_cdf_thread_init_static(CdfThreadContext *cdf, int qidx);
int dav1d_cdf_thread_alloc(CdfThreadContext *cdf, struct thread_data *t);
void dav1d_cdf_thread_copy(CdfContext *dst, const CdfThreadContext *src);
void dav1d_cdf_thread_ref(CdfThreadContext *dst, CdfThreadContext *src);
void dav1d_cdf_thread_unref(CdfThreadContext *cdf);
void dav1d_cdf_thread_update(const Dav1dFrameHeader *hdr, CdfContext *dst,
                             const CdfContext *src);

/*
 * These are binary signals (so a signal is either "done" or "not done").
 */
void dav1d_cdf_thread_wait(CdfThreadContext *cdf);
void dav1d_cdf_thread_signal(CdfThreadContext *cdf);

#endif /* DAV1D_SRC_CDF_H */
