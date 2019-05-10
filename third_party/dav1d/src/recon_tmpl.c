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

#include <string.h>
#include <stdio.h>

#include "common/attributes.h"
#include "common/bitdepth.h"
#include "common/dump.h"
#include "common/intops.h"
#include "common/mem.h"

#include "src/cdef_apply.h"
#include "src/ctx.h"
#include "src/ipred_prepare.h"
#include "src/lf_apply.h"
#include "src/lr_apply.h"
#include "src/recon.h"
#include "src/scan.h"
#include "src/tables.h"
#include "src/wedge.h"

static unsigned read_golomb(MsacContext *const msac) {
    int len = 0;
    unsigned val = 1;

    while (!dav1d_msac_decode_bool_equi(msac) && len < 32) len++;
    while (len--) val = (val << 1) | dav1d_msac_decode_bool_equi(msac);

    return val - 1;
}

static int decode_coefs(Dav1dTileContext *const t,
                        uint8_t *const a, uint8_t *const l,
                        const enum RectTxfmSize tx, const enum BlockSize bs,
                        const Av1Block *const b, const int intra,
                        const int plane, coef *cf,
                        enum TxfmType *const txtp, uint8_t *res_ctx)
{
    Dav1dTileState *const ts = t->ts;
    const int chroma = !!plane;
    const Dav1dFrameContext *const f = t->f;
    const TxfmInfo *const t_dim = &dav1d_txfm_dimensions[tx];
    const int dbg = DEBUG_BLOCK_INFO && plane && 0;

    if (dbg) printf("Start: r=%d\n", ts->msac.rng);

    // does this block have any non-zero coefficients
    const int sctx = get_coef_skip_ctx(t_dim, bs, a, l, chroma, f->cur.p.layout);
    const int all_skip = dav1d_msac_decode_bool_adapt(&ts->msac,
                             ts->cdf.coef.skip[t_dim->ctx][sctx]);
    if (dbg)
    printf("Post-non-zero[%d][%d][%d]: r=%d\n",
           t_dim->ctx, sctx, all_skip, ts->msac.rng);
    if (all_skip) {
        *res_ctx = 0x40;
        *txtp = f->frame_hdr->segmentation.lossless[b->seg_id] ? WHT_WHT :
                                                                DCT_DCT;
        return -1;
    }

    // transform type (chroma: derived, luma: explicitly coded)
    if (chroma) {
        if (intra) {
            *txtp = get_uv_intra_txtp(b->uv_mode, tx, f->frame_hdr, b->seg_id);
        } else {
            const enum TxfmType y_txtp = *txtp;
            *txtp = get_uv_inter_txtp(t_dim, y_txtp, f->frame_hdr, b->seg_id);
        }
    } else {
        const enum TxfmTypeSet set = get_ext_txtp_set(tx, !intra,
                                                      f->frame_hdr, b->seg_id);
        const unsigned set_cnt = dav1d_tx_type_count[set];
        unsigned idx;
        if (set_cnt == 1) {
            idx = 0;
        } else {
            const int set_idx = dav1d_tx_type_set_index[!intra][set];
            const enum IntraPredMode y_mode_nofilt = intra ? b->y_mode == FILTER_PRED ?
                dav1d_filter_mode_to_y_mode[b->y_angle] : b->y_mode : 0;
            uint16_t *const txtp_cdf = intra ?
                       ts->cdf.m.txtp_intra[set_idx][t_dim->min][y_mode_nofilt] :
                       ts->cdf.m.txtp_inter[set_idx][t_dim->min];
            idx = (set_cnt <= 8 ? dav1d_msac_decode_symbol_adapt8 :
                     dav1d_msac_decode_symbol_adapt16)(&ts->msac, txtp_cdf, set_cnt);

            if (dbg)
            printf("Post-txtp[%d->%d][%d->%d][%d][%d->%d]: r=%d\n",
                   set, set_idx, tx, t_dim->min, intra ? (int)y_mode_nofilt : -1,
                   idx, dav1d_tx_types_per_set[set][idx], ts->msac.rng);
        }
        *txtp = dav1d_tx_types_per_set[set][idx];
    }

    // find end-of-block (eob)
    int eob_bin;
    const int tx2dszctx = imin(t_dim->lw, TX_32X32) + imin(t_dim->lh, TX_32X32);
    const enum TxClass tx_class = dav1d_tx_type_class[*txtp];
    const int is_1d = tx_class != TX_CLASS_2D;
    switch (tx2dszctx) {
#define case_sz(sz, bin, ns) \
    case sz: { \
        uint16_t *const eob_bin_cdf = ts->cdf.coef.eob_bin_##bin[chroma][is_1d]; \
        eob_bin = dav1d_msac_decode_symbol_adapt##ns(&ts->msac, eob_bin_cdf, 5 + sz); \
        break; \
    }
    case_sz(0,   16,  4);
    case_sz(1,   32,  8);
    case_sz(2,   64,  8);
    case_sz(3,  128,  8);
    case_sz(4,  256, 16);
    case_sz(5,  512, 16);
    case_sz(6, 1024, 16);
#undef case_sz
    }
    if (dbg)
    printf("Post-eob_bin_%d[%d][%d][%d]: r=%d\n",
           16 << tx2dszctx, chroma, is_1d, eob_bin, ts->msac.rng);
    int eob;
    if (eob_bin > 1) {
        eob = 1 << (eob_bin - 1);
        uint16_t *const eob_hi_bit_cdf =
            ts->cdf.coef.eob_hi_bit[t_dim->ctx][chroma][eob_bin];
        const int eob_hi_bit = dav1d_msac_decode_bool_adapt(&ts->msac,
                                                            eob_hi_bit_cdf);
        if (dbg)
        printf("Post-eob_hi_bit[%d][%d][%d][%d]: r=%d\n",
               t_dim->ctx, chroma, eob_bin, eob_hi_bit, ts->msac.rng);
        unsigned mask = eob >> 1;
        if (eob_hi_bit) eob |= mask;
        for (mask >>= 1; mask; mask >>= 1) {
            const int eob_bit = dav1d_msac_decode_bool_equi(&ts->msac);
            if (eob_bit) eob |= mask;
        }
        if (dbg)
        printf("Post-eob[%d]: r=%d\n", eob, ts->msac.rng);
    } else {
        eob = eob_bin;
    }

    // base tokens
    uint16_t (*const br_cdf)[5] =
        ts->cdf.coef.br_tok[imin(t_dim->ctx, 3)][chroma];
    const int16_t *const scan = dav1d_scans[tx][tx_class];
    uint8_t levels[36 * 36];
    ptrdiff_t stride = 4 * (imin(t_dim->h, 8) + 1);
    memset(levels, 0, stride * 4 * (imin(t_dim->w, 8) + 1));
    const int shift = 2 + imin(t_dim->lh, 3), mask = 4 * imin(t_dim->h, 8) - 1;
    unsigned cul_level = 0;
    for (int i = eob, is_last = 1; i >= 0; i--, is_last = 0) {
        const int rc = scan[i], x = rc >> shift, y = rc & mask;

        // lo tok
        const int ctx = get_coef_nz_ctx(levels, i, rc, is_last, tx, tx_class);
        uint16_t *const lo_cdf = is_last ?
            ts->cdf.coef.eob_base_tok[t_dim->ctx][chroma][ctx] :
            ts->cdf.coef.base_tok[t_dim->ctx][chroma][ctx];
        int tok = dav1d_msac_decode_symbol_adapt4(&ts->msac, lo_cdf,
                                                  4 - is_last) + is_last;
        if (dbg)
        printf("Post-lo_tok[%d][%d][%d][%d=%d=%d]: r=%d\n",
               t_dim->ctx, chroma, ctx, i, rc, tok, ts->msac.rng);
        if (!tok) continue;

        // hi tok
        if (tok == 3) {
            const int br_ctx = get_br_ctx(levels, rc, tx, tx_class);
            do {
                const int tok_br = dav1d_msac_decode_symbol_adapt4(&ts->msac,
                                       br_cdf[br_ctx], 4);
                if (dbg)
                printf("Post-hi_tok[%d][%d][%d][%d=%d=%d->%d]: r=%d\n",
                       imin(t_dim->ctx, 3), chroma, br_ctx,
                       i, rc, tok_br, tok, ts->msac.rng);
                tok += tok_br;
                if (tok_br < 3) break;
            } while (tok < 15);
        }

        cf[rc] = tok;
        levels[x * stride + y] = (uint8_t) cf[rc];
    }

    // residual and sign
    int dc_sign = 1;
    const int lossless = f->frame_hdr->segmentation.lossless[b->seg_id];
    const uint16_t *const dq_tbl = ts->dq[b->seg_id][plane];
    const uint8_t *const qm_tbl = f->qm[lossless || is_1d || *txtp == IDTX][tx][plane];
    const int dq_shift = imax(0, t_dim->ctx - 2);
    const int bitdepth = BITDEPTH == 8 ? 8 : f->cur.p.bpc;
    const int cf_min = -(1 << (7 + bitdepth));
    const int cf_max = (1 << (7 + bitdepth)) - 1;
    for (int i = 0; i <= eob; i++) {
        const int rc = scan[i];
        int tok = cf[rc];
        if (!tok) continue;
        int dq;

        // sign
        int sign;
        if (i == 0) {
            const int dc_sign_ctx = get_dc_sign_ctx(t_dim, a, l);
            uint16_t *const dc_sign_cdf =
                ts->cdf.coef.dc_sign[chroma][dc_sign_ctx];
            sign = dav1d_msac_decode_bool_adapt(&ts->msac, dc_sign_cdf);
            if (dbg)
            printf("Post-dc_sign[%d][%d][%d]: r=%d\n",
                   chroma, dc_sign_ctx, sign, ts->msac.rng);
            dc_sign = sign ? 0 : 2;
            dq = (dq_tbl[0] * qm_tbl[0] + 16) >> 5;
        } else {
            sign = dav1d_msac_decode_bool_equi(&ts->msac);
            if (dbg)
            printf("Post-sign[%d=%d=%d]: r=%d\n", i, rc, sign, ts->msac.rng);
            dq = (dq_tbl[1] * qm_tbl[rc] + 16) >> 5;
        }

        // residual
        if (tok == 15) {
            tok += read_golomb(&ts->msac);
            if (dbg)
            printf("Post-residual[%d=%d=%d->%d]: r=%d\n",
                   i, rc, tok - 15, tok, ts->msac.rng);
        }

        // coefficient parsing, see 5.11.39
        tok &= 0xfffff;

        // dequant, see 7.12.3
        cul_level += tok;
        tok = (((int64_t)dq * tok) & 0xffffff) >> dq_shift;
        cf[rc] = iclip(sign ? -tok : tok, cf_min, cf_max);
    }

    // context
    *res_ctx = imin(cul_level, 63) | (dc_sign << 6);

    return eob;
}

static void read_coef_tree(Dav1dTileContext *const t,
                           const enum BlockSize bs, const Av1Block *const b,
                           const enum RectTxfmSize ytx, const int depth,
                           const uint16_t *const tx_split,
                           const int x_off, const int y_off, pixel *dst)
{
    const Dav1dFrameContext *const f = t->f;
    Dav1dTileState *const ts = t->ts;
    const Dav1dDSPContext *const dsp = f->dsp;
    const TxfmInfo *const t_dim = &dav1d_txfm_dimensions[ytx];
    const int txw = t_dim->w, txh = t_dim->h;

    /* y_off can be larger than 3 since lossless blocks use TX_4X4 but can't
     * be splitted. Aviods an undefined left shift. */
    if (depth < 2 && tx_split[depth] &&
        tx_split[depth] & (1 << (y_off * 4 + x_off)))
    {
        const enum RectTxfmSize sub = t_dim->sub;
        const TxfmInfo *const sub_t_dim = &dav1d_txfm_dimensions[sub];
        const int txsw = sub_t_dim->w, txsh = sub_t_dim->h;

        read_coef_tree(t, bs, b, sub, depth + 1, tx_split,
                       x_off * 2 + 0, y_off * 2 + 0, dst);
        t->bx += txsw;
        if (txw >= txh && t->bx < f->bw)
            read_coef_tree(t, bs, b, sub, depth + 1, tx_split, x_off * 2 + 1,
                           y_off * 2 + 0, dst ? &dst[4 * txsw] : NULL);
        t->bx -= txsw;
        t->by += txsh;
        if (txh >= txw && t->by < f->bh) {
            if (dst)
                dst += 4 * txsh * PXSTRIDE(f->cur.stride[0]);
            read_coef_tree(t, bs, b, sub, depth + 1, tx_split,
                           x_off * 2 + 0, y_off * 2 + 1, dst);
            t->bx += txsw;
            if (txw >= txh && t->bx < f->bw)
                read_coef_tree(t, bs, b, sub, depth + 1, tx_split, x_off * 2 + 1,
                               y_off * 2 + 1, dst ? &dst[4 * txsw] : NULL);
            t->bx -= txsw;
        }
        t->by -= txsh;
    } else {
        const int bx4 = t->bx & 31, by4 = t->by & 31;
        enum TxfmType txtp;
        uint8_t cf_ctx;
        int eob;
        coef *cf;
        struct CodedBlockInfo *cbi;

        if (f->frame_thread.pass) {
            cf = ts->frame_thread.cf;
            ts->frame_thread.cf += imin(t_dim->w, 8) * imin(t_dim->h, 8) * 16;
            cbi = &f->frame_thread.cbi[t->by * f->b4_stride + t->bx];
        } else {
            cf = t->cf;
        }
        if (f->frame_thread.pass != 2) {
            eob = decode_coefs(t, &t->a->lcoef[bx4], &t->l.lcoef[by4],
                               ytx, bs, b, 0, 0, cf, &txtp, &cf_ctx);
            if (DEBUG_BLOCK_INFO)
                printf("Post-y-cf-blk[tx=%d,txtp=%d,eob=%d]: r=%d\n",
                       ytx, txtp, eob, ts->msac.rng);
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
            rep_macro(type, t->dir lcoef, off, mul * cf_ctx)
#define default_memset(dir, diridx, off, sz) \
            memset(&t->dir lcoef[off], cf_ctx, sz)
            case_set_upto16_with_default(imin(txh, f->bh - t->by), l., 1, by4);
            case_set_upto16_with_default(imin(txw, f->bw - t->bx), a->, 0, bx4);
#undef default_memset
#undef set_ctx
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
            for (int y = 0; y < txh; y++) { \
                rep_macro(type, txtp_map, 0, mul * txtp); \
                txtp_map += 32; \
            }
            uint8_t *txtp_map = &t->txtp_map[by4 * 32 + bx4];
            case_set_upto16(txw,,,);
#undef set_ctx
            if (f->frame_thread.pass == 1) {
                cbi->eob[0] = eob;
                cbi->txtp[0] = txtp;
            }
        } else {
            eob = cbi->eob[0];
            txtp = cbi->txtp[0];
        }
        if (!(f->frame_thread.pass & 1)) {
            assert(dst);
            if (eob >= 0) {
                if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS)
                    coef_dump(cf, imin(t_dim->h, 8) * 4, imin(t_dim->w, 8) * 4, 3, "dq");
                dsp->itx.itxfm_add[ytx][txtp](dst, f->cur.stride[0], cf, eob
                                              HIGHBD_CALL_SUFFIX);
                if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS)
                    hex_dump(dst, f->cur.stride[0], t_dim->w * 4, t_dim->h * 4, "recon");
            }
        }
    }
}

void bytefn(dav1d_read_coef_blocks)(Dav1dTileContext *const t,
                                    const enum BlockSize bs, const Av1Block *const b)
{
    const Dav1dFrameContext *const f = t->f;
    const int ss_ver = f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
    const int bx4 = t->bx & 31, by4 = t->by & 31;
    const int cbx4 = bx4 >> ss_hor, cby4 = by4 >> ss_ver;
    const uint8_t *const b_dim = dav1d_block_dimensions[bs];
    const int bw4 = b_dim[0], bh4 = b_dim[1];
    const int cbw4 = (bw4 + ss_hor) >> ss_hor, cbh4 = (bh4 + ss_ver) >> ss_ver;
    const int has_chroma = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I400 &&
                           (bw4 > ss_hor || t->bx & 1) &&
                           (bh4 > ss_ver || t->by & 1);

    if (b->skip) {
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
        rep_macro(type, t->dir lcoef, off, mul * 0x40)
        case_set(bh4, l., 1, by4);
        case_set(bw4, a->, 0, bx4);
#undef set_ctx
        if (has_chroma) {
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
            rep_macro(type, t->dir ccoef[0], off, mul * 0x40); \
            rep_macro(type, t->dir ccoef[1], off, mul * 0x40)
            case_set(cbh4, l., 1, cby4);
            case_set(cbw4, a->, 0, cbx4);
#undef set_ctx
        }
        return;
    }

    Dav1dTileState *const ts = t->ts;
    const int w4 = imin(bw4, f->bw - t->bx), h4 = imin(bh4, f->bh - t->by);
    const int cw4 = (w4 + ss_hor) >> ss_hor, ch4 = (h4 + ss_ver) >> ss_ver;
    assert(f->frame_thread.pass == 1);
    assert(!b->skip);
    const TxfmInfo *const uv_t_dim = &dav1d_txfm_dimensions[b->uvtx];
    const TxfmInfo *const t_dim = &dav1d_txfm_dimensions[b->intra ? b->tx : b->max_ytx];

    for (int init_y = 0; init_y < h4; init_y += 16) {
        for (int init_x = 0; init_x < w4; init_x += 16) {
            const int sub_h4 = imin(h4, 16 + init_y);
            const int sub_w4 = imin(w4, init_x + 16);
            int y_off = !!init_y, y, x;
            for (y = init_y, t->by += init_y; y < sub_h4;
                 y += t_dim->h, t->by += t_dim->h, y_off++)
            {
                struct CodedBlockInfo *const cbi =
                    &f->frame_thread.cbi[t->by * f->b4_stride];
                int x_off = !!init_x;
                for (x = init_x, t->bx += init_x; x < sub_w4;
                     x += t_dim->w, t->bx += t_dim->w, x_off++)
                {
                    if (!b->intra) {
                        read_coef_tree(t, bs, b, b->max_ytx, 0, b->tx_split,
                                       x_off, y_off, NULL);
                    } else {
                        uint8_t cf_ctx = 0x40;
                        enum TxfmType txtp;
                        const int eob = cbi[t->bx].eob[0] =
                            decode_coefs(t, &t->a->lcoef[bx4 + x],
                                         &t->l.lcoef[by4 + y], b->tx, bs, b, 1,
                                         0, ts->frame_thread.cf, &txtp, &cf_ctx);
                        if (DEBUG_BLOCK_INFO)
                            printf("Post-y-cf-blk[tx=%d,txtp=%d,eob=%d]: r=%d\n",
                                   b->tx, txtp, eob, ts->msac.rng);
                        cbi[t->bx].txtp[0] = txtp;
                        ts->frame_thread.cf += imin(t_dim->w, 8) * imin(t_dim->h, 8) * 16;
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
                        rep_macro(type, t->dir lcoef, off, mul * cf_ctx)
#define default_memset(dir, diridx, off, sz) \
                        memset(&t->dir lcoef[off], cf_ctx, sz)
                        case_set_upto16_with_default(imin(t_dim->h, f->bh - t->by),
                                                     l., 1, by4 + y);
                        case_set_upto16_with_default(imin(t_dim->w, f->bw - t->bx),
                                                     a->, 0, bx4 + x);
#undef default_memset
#undef set_ctx
                    }
                }
                t->bx -= x;
            }
            t->by -= y;

            if (!has_chroma) continue;

            const int sub_ch4 = imin(ch4, (init_y + 16) >> ss_ver);
            const int sub_cw4 = imin(cw4, (init_x + 16) >> ss_hor);
            for (int pl = 0; pl < 2; pl++) {
                for (y = init_y >> ss_ver, t->by += init_y; y < sub_ch4;
                     y += uv_t_dim->h, t->by += uv_t_dim->h << ss_ver)
                {
                    struct CodedBlockInfo *const cbi =
                        &f->frame_thread.cbi[t->by * f->b4_stride];
                    for (x = init_x >> ss_hor, t->bx += init_x; x < sub_cw4;
                         x += uv_t_dim->w, t->bx += uv_t_dim->w << ss_hor)
                    {
                        uint8_t cf_ctx = 0x40;
                        enum TxfmType txtp;
                        if (!b->intra)
                            txtp = t->txtp_map[(by4 + (y << ss_ver)) * 32 +
                                                bx4 + (x << ss_hor)];
                        const int eob = cbi[t->bx].eob[1 + pl] =
                            decode_coefs(t, &t->a->ccoef[pl][cbx4 + x],
                                         &t->l.ccoef[pl][cby4 + y], b->uvtx, bs,
                                         b, b->intra, 1 + pl, ts->frame_thread.cf,
                                         &txtp, &cf_ctx);
                        if (DEBUG_BLOCK_INFO)
                            printf("Post-uv-cf-blk[pl=%d,tx=%d,"
                                   "txtp=%d,eob=%d]: r=%d\n",
                                   pl, b->uvtx, txtp, eob, ts->msac.rng);
                        cbi[t->bx].txtp[1 + pl] = txtp;
                        ts->frame_thread.cf += uv_t_dim->w * uv_t_dim->h * 16;
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
                        rep_macro(type, t->dir ccoef[pl], off, mul * cf_ctx)
#define default_memset(dir, diridx, off, sz) \
                        memset(&t->dir ccoef[pl][off], cf_ctx, sz)
                        case_set_upto16_with_default( \
                                 imin(uv_t_dim->h, (f->bh - t->by + ss_ver) >> ss_ver),
                                 l., 1, cby4 + y);
                        case_set_upto16_with_default( \
                                 imin(uv_t_dim->w, (f->bw - t->bx + ss_hor) >> ss_hor),
                                 a->, 0, cbx4 + x);
#undef default_memset
#undef set_ctx
                    }
                    t->bx -= x << ss_hor;
                }
                t->by -= y << ss_ver;
            }
        }
    }
}

static int mc(Dav1dTileContext *const t,
              pixel *const dst8, int16_t *const dst16, const ptrdiff_t dst_stride,
              const int bw4, const int bh4,
              const int bx, const int by, const int pl,
              const mv mv, const Dav1dThreadPicture *const refp, const int refidx,
              const enum Filter2d filter_2d)
{
    assert((dst8 != NULL) ^ (dst16 != NULL));
    const Dav1dFrameContext *const f = t->f;
    const int ss_ver = !!pl && f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = !!pl && f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
    const int h_mul = 4 >> ss_hor, v_mul = 4 >> ss_ver;
    const int mvx = mv.x, mvy = mv.y;
    const int mx = mvx & (15 >> !ss_hor), my = mvy & (15 >> !ss_ver);
    ptrdiff_t ref_stride = refp->p.stride[!!pl];
    const pixel *ref;

    if (refp->p.p.w == f->cur.p.w && refp->p.p.h == f->cur.p.h) {
        const int dx = bx * h_mul + (mvx >> (3 + ss_hor));
        const int dy = by * v_mul + (mvy >> (3 + ss_ver));
        int w, h;

        if (refp->p.data[0] != f->cur.data[0]) { // i.e. not for intrabc
            if (dav1d_thread_picture_wait(refp, dy + bh4 * v_mul + !!my * 4,
                                          PLANE_TYPE_Y + !!pl))
            {
                return -1;
            }
            w = (f->cur.p.w + ss_hor) >> ss_hor;
            h = (f->cur.p.h + ss_ver) >> ss_ver;
        } else {
            w = f->bw * 4 >> ss_hor;
            h = f->bh * 4 >> ss_ver;
        }
        if (dx < !!mx * 3 || dy < !!my * 3 ||
            dx + bw4 * h_mul + !!mx * 4 > w ||
            dy + bh4 * v_mul + !!my * 4 > h)
        {
            f->dsp->mc.emu_edge(bw4 * h_mul + !!mx * 7, bh4 * v_mul + !!my * 7,
                                w, h, dx - !!mx * 3, dy - !!my * 3,
                                t->emu_edge, 192 * sizeof(pixel),
                                refp->p.data[pl], ref_stride);
            ref = &t->emu_edge[192 * !!my * 3 + !!mx * 3];
            ref_stride = 192 * sizeof(pixel);
        } else {
            ref = ((pixel *) refp->p.data[pl]) + PXSTRIDE(ref_stride) * dy + dx;
        }

        if (dst8 != NULL) {
            f->dsp->mc.mc[filter_2d](dst8, dst_stride, ref, ref_stride, bw4 * h_mul,
                                     bh4 * v_mul, mx << !ss_hor, my << !ss_ver
                                     HIGHBD_CALL_SUFFIX);
        } else {
            f->dsp->mc.mct[filter_2d](dst16, ref, ref_stride, bw4 * h_mul,
                                      bh4 * v_mul, mx << !ss_hor, my << !ss_ver
                                      HIGHBD_CALL_SUFFIX);
        }
    } else {
        assert(refp != &f->sr_cur);

        int orig_pos_y = (by * v_mul << 4) + mvy * (1 << !ss_ver);
        int orig_pos_x = (bx * h_mul << 4) + mvx * (1 << !ss_hor);
#define scale_mv(res, val, scale) do { \
            const int64_t tmp = (int64_t)(val) * scale + (scale - 0x4000) * 8; \
            res = apply_sign64((int) ((llabs(tmp) + 128) >> 8), tmp) + 32;     \
        } while (0)
        int pos_y, pos_x;
        scale_mv(pos_x, orig_pos_x, f->svc[refidx][0].scale);
        scale_mv(pos_y, orig_pos_y, f->svc[refidx][1].scale);
#undef scale_mv
        const int left = pos_x >> 10;
        const int top = pos_y >> 10;
        const int right =
            ((pos_x + (bw4 * h_mul - 1) * f->svc[refidx][0].step) >> 10) + 1;
        const int bottom =
            ((pos_y + (bh4 * v_mul - 1) * f->svc[refidx][1].step) >> 10) + 1;

        if (dav1d_thread_picture_wait(refp, bottom + 4, PLANE_TYPE_Y + !!pl))
            return -1;
        if (DEBUG_BLOCK_INFO)
            printf("Off %dx%d [%d,%d,%d], size %dx%d [%d,%d]\n",
                   left, top, orig_pos_x, f->svc[refidx][0].scale, refidx,
                   right-left, bottom-top,
                   f->svc[refidx][0].step, f->svc[refidx][1].step);

        const int w = (refp->p.p.w + ss_hor) >> ss_hor;
        const int h = (refp->p.p.h + ss_ver) >> ss_ver;
        if (left < 3 || top < 3 || right + 4 > w || bottom + 4 > h) {
            f->dsp->mc.emu_edge(right - left + 7, bottom - top + 7,
                                w, h, left - 3, top - 3,
                                t->emu_edge, 320 * sizeof(pixel),
                                refp->p.data[pl], ref_stride);
            ref = &t->emu_edge[320 * 3 + 3];
            ref_stride = 320 * sizeof(pixel);
            if (DEBUG_BLOCK_INFO) printf("Emu\n");
        } else {
            ref = ((pixel *) refp->p.data[pl]) + PXSTRIDE(ref_stride) * top + left;
        }

        if (dst8 != NULL) {
            f->dsp->mc.mc_scaled[filter_2d](dst8, dst_stride, ref, ref_stride,
                                            bw4 * h_mul, bh4 * v_mul,
                                            pos_x & 0x3ff, pos_y & 0x3ff,
                                            f->svc[refidx][0].step,
                                            f->svc[refidx][1].step
                                            HIGHBD_CALL_SUFFIX);
        } else {
            f->dsp->mc.mct_scaled[filter_2d](dst16, ref, ref_stride,
                                             bw4 * h_mul, bh4 * v_mul,
                                             pos_x & 0x3ff, pos_y & 0x3ff,
                                             f->svc[refidx][0].step,
                                             f->svc[refidx][1].step
                                             HIGHBD_CALL_SUFFIX);
        }
    }

    return 0;
}

static int obmc(Dav1dTileContext *const t,
                pixel *const dst, const ptrdiff_t dst_stride,
                const uint8_t *const b_dim, const int pl,
                const int bx4, const int by4, const int w4, const int h4)
{
    assert(!(t->bx & 1) && !(t->by & 1));
    const Dav1dFrameContext *const f = t->f;
    const refmvs *const r = &f->mvs[t->by * f->b4_stride + t->bx];
    pixel *const lap = t->scratch.lap;
    const int ss_ver = !!pl && f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = !!pl && f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
    const int h_mul = 4 >> ss_hor, v_mul = 4 >> ss_ver;
    int res;

    if (t->by > t->ts->tiling.row_start &&
        (!pl || b_dim[0] * h_mul + b_dim[1] * v_mul >= 16))
    {
        for (int i = 0, x = 0; x < w4 && i < imin(b_dim[2], 4); ) {
            // only odd blocks are considered for overlap handling, hence +1
            const refmvs *const a_r = &r[x - f->b4_stride + 1];
            const uint8_t *const a_b_dim =
                dav1d_block_dimensions[dav1d_sbtype_to_bs[a_r->sb_type]];

            if (a_r->ref[0] > 0) {
                const int ow4 = iclip(a_b_dim[0], 2, b_dim[0]);
                const int oh4 = imin(b_dim[1], 16) >> 1;
                res = mc(t, lap, NULL, ow4 * h_mul * sizeof(pixel), ow4, oh4,
                         t->bx + x, t->by, pl, a_r->mv[0],
                         &f->refp[a_r->ref[0] - 1], a_r->ref[0] - 1,
                         dav1d_filter_2d[t->a->filter[1][bx4 + x + 1]][t->a->filter[0][bx4 + x + 1]]);
                if (res) return res;
                f->dsp->mc.blend_h(&dst[x * h_mul], dst_stride, lap,
                                   h_mul * ow4, v_mul * oh4);
                i++;
            }
            x += imax(a_b_dim[0], 2);
        }
    }

    if (t->bx > t->ts->tiling.col_start)
        for (int i = 0, y = 0; y < h4 && i < imin(b_dim[3], 4); ) {
            // only odd blocks are considered for overlap handling, hence +1
            const refmvs *const l_r = &r[(y + 1) * f->b4_stride - 1];
            const uint8_t *const l_b_dim =
                dav1d_block_dimensions[dav1d_sbtype_to_bs[l_r->sb_type]];

            if (l_r->ref[0] > 0) {
                const int ow4 = imin(b_dim[0], 16) >> 1;
                const int oh4 = iclip(l_b_dim[1], 2, b_dim[1]);
                res = mc(t, lap, NULL, h_mul * ow4 * sizeof(pixel), ow4, oh4,
                         t->bx, t->by + y, pl, l_r->mv[0],
                         &f->refp[l_r->ref[0] - 1], l_r->ref[0] - 1,
                         dav1d_filter_2d[t->l.filter[1][by4 + y + 1]][t->l.filter[0][by4 + y + 1]]);
                if (res) return res;
                f->dsp->mc.blend_v(&dst[y * v_mul * PXSTRIDE(dst_stride)],
                                   dst_stride, lap, h_mul * ow4, v_mul * oh4);
                i++;
            }
            y += imax(l_b_dim[1], 2);
        }
    return 0;
}

static int warp_affine(Dav1dTileContext *const t,
                       pixel *dst8, int16_t *dst16, const ptrdiff_t dstride,
                       const uint8_t *const b_dim, const int pl,
                       const Dav1dThreadPicture *const refp,
                       const Dav1dWarpedMotionParams *const wmp)
{
    assert((dst8 != NULL) ^ (dst16 != NULL));
    const Dav1dFrameContext *const f = t->f;
    const Dav1dDSPContext *const dsp = f->dsp;
    const int ss_ver = !!pl && f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = !!pl && f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
    const int h_mul = 4 >> ss_hor, v_mul = 4 >> ss_ver;
    assert(!((b_dim[0] * h_mul) & 7) && !((b_dim[1] * v_mul) & 7));
    const int32_t *const mat = wmp->matrix;
    const int width = (refp->p.p.w + ss_hor) >> ss_hor;
    const int height = (refp->p.p.h + ss_ver) >> ss_ver;

    for (int y = 0; y < b_dim[1] * v_mul; y += 8) {
        for (int x = 0; x < b_dim[0] * h_mul; x += 8) {
            // calculate transformation relative to center of 8x8 block in
            // luma pixel units
            const int src_x = t->bx * 4 + ((x + 4) << ss_hor);
            const int src_y = t->by * 4 + ((y + 4) << ss_ver);
            const int mvx = (mat[2] * src_x + mat[3] * src_y + mat[0]) >> ss_hor;
            const int mvy = (mat[4] * src_x + mat[5] * src_y + mat[1]) >> ss_ver;

            const int dx = (mvx >> 16) - 4;
            const int mx = ((mvx & 0xffff) - wmp->alpha * 4 -
                                             wmp->beta  * 7) & ~0x3f;
            const int dy = (mvy >> 16) - 4;
            const int my = ((mvy & 0xffff) - wmp->gamma * 4 -
                                             wmp->delta * 4) & ~0x3f;

            const pixel *ref_ptr;
            ptrdiff_t ref_stride = refp->p.stride[!!pl];

            if (dav1d_thread_picture_wait(refp, dy + 4 + 8,
                                          PLANE_TYPE_Y + !!pl))
            {
                return -1;
            }
            if (dx < 3 || dx + 8 + 4 > width || dy < 3 || dy + 8 + 4 > height) {
                f->dsp->mc.emu_edge(15, 15, width, height, dx - 3, dy - 3,
                                    t->emu_edge, 192 * sizeof(pixel),
                                    refp->p.data[pl], ref_stride);
                ref_ptr = &t->emu_edge[192 * 3 + 3];
                ref_stride = 192 * sizeof(pixel);
            } else {
                ref_ptr = ((pixel *) refp->p.data[pl]) + PXSTRIDE(ref_stride) * dy + dx;
            }
            if (dst16 != NULL)
                dsp->mc.warp8x8t(&dst16[x], dstride, ref_ptr, ref_stride,
                                 wmp->abcd, mx, my HIGHBD_CALL_SUFFIX);
            else
                dsp->mc.warp8x8(&dst8[x], dstride, ref_ptr, ref_stride,
                                wmp->abcd, mx, my HIGHBD_CALL_SUFFIX);
        }
        if (dst8) dst8  += 8 * PXSTRIDE(dstride);
        else      dst16 += 8 * dstride;
    }
    return 0;
}

void bytefn(dav1d_recon_b_intra)(Dav1dTileContext *const t, const enum BlockSize bs,
                                 const enum EdgeFlags intra_edge_flags,
                                 const Av1Block *const b)
{
    Dav1dTileState *const ts = t->ts;
    const Dav1dFrameContext *const f = t->f;
    const Dav1dDSPContext *const dsp = f->dsp;
    const int bx4 = t->bx & 31, by4 = t->by & 31;
    const int ss_ver = f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
    const int cbx4 = bx4 >> ss_hor, cby4 = by4 >> ss_ver;
    const uint8_t *const b_dim = dav1d_block_dimensions[bs];
    const int bw4 = b_dim[0], bh4 = b_dim[1];
    const int w4 = imin(bw4, f->bw - t->bx), h4 = imin(bh4, f->bh - t->by);
    const int cw4 = (w4 + ss_hor) >> ss_hor, ch4 = (h4 + ss_ver) >> ss_ver;
    const int has_chroma = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I400 &&
                           (bw4 > ss_hor || t->bx & 1) &&
                           (bh4 > ss_ver || t->by & 1);
    const TxfmInfo *const t_dim = &dav1d_txfm_dimensions[b->tx];
    const TxfmInfo *const uv_t_dim = &dav1d_txfm_dimensions[b->uvtx];

    // coefficient coding
    ALIGN_STK_32(pixel, edge_buf, 257,);
    pixel *const edge = edge_buf + 128;
    const int cbw4 = (bw4 + ss_hor) >> ss_hor, cbh4 = (bh4 + ss_ver) >> ss_ver;

    const int intra_edge_filter_flag = f->seq_hdr->intra_edge_filter << 10;

    for (int init_y = 0; init_y < h4; init_y += 16) {
        for (int init_x = 0; init_x < w4; init_x += 16) {
            if (b->pal_sz[0]) {
                pixel *dst = ((pixel *) f->cur.data[0]) +
                             4 * (t->by * PXSTRIDE(f->cur.stride[0]) + t->bx);
                const uint8_t *pal_idx;
                if (f->frame_thread.pass) {
                    pal_idx = ts->frame_thread.pal_idx;
                    ts->frame_thread.pal_idx += bw4 * bh4 * 16;
                } else {
                    pal_idx = t->scratch.pal_idx;
                }
                const uint16_t *const pal = f->frame_thread.pass ?
                    f->frame_thread.pal[((t->by >> 1) + (t->bx & 1)) * (f->b4_stride >> 1) +
                                        ((t->bx >> 1) + (t->by & 1))][0] : t->pal[0];
                f->dsp->ipred.pal_pred(dst, f->cur.stride[0], pal,
                                       pal_idx, bw4 * 4, bh4 * 4);
                if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS)
                    hex_dump(dst, PXSTRIDE(f->cur.stride[0]),
                             bw4 * 4, bh4 * 4, "y-pal-pred");
            }

            const int intra_flags = (sm_flag(t->a, bx4) |
                                     sm_flag(&t->l, by4) |
                                     intra_edge_filter_flag);
            const int sb_has_tr = init_x + 16 < w4 ? 1 : init_y ? 0 :
                              intra_edge_flags & EDGE_I444_TOP_HAS_RIGHT;
            const int sb_has_bl = init_x ? 0 : init_y + 16 < h4 ? 1 :
                              intra_edge_flags & EDGE_I444_LEFT_HAS_BOTTOM;
            int y, x;
            const int sub_h4 = imin(h4, 16 + init_y);
            const int sub_w4 = imin(w4, init_x + 16);
            for (y = init_y, t->by += init_y; y < sub_h4;
                 y += t_dim->h, t->by += t_dim->h)
            {
                pixel *dst = ((pixel *) f->cur.data[0]) +
                               4 * (t->by * PXSTRIDE(f->cur.stride[0]) +
                                    t->bx + init_x);
                for (x = init_x, t->bx += init_x; x < sub_w4;
                     x += t_dim->w, t->bx += t_dim->w)
                {
                    if (b->pal_sz[0]) goto skip_y_pred;

                    int angle = b->y_angle;
                    const enum EdgeFlags edge_flags =
                        (((y > init_y || !sb_has_tr) && (x + t_dim->w >= sub_w4)) ?
                             0 : EDGE_I444_TOP_HAS_RIGHT) |
                        ((x > init_x || (!sb_has_bl && y + t_dim->h >= sub_h4)) ?
                             0 : EDGE_I444_LEFT_HAS_BOTTOM);
                    const pixel *top_sb_edge = NULL;
                    if (!(t->by & (f->sb_step - 1))) {
                        top_sb_edge = f->ipred_edge[0];
                        const int sby = t->by >> f->sb_shift;
                        top_sb_edge += f->sb128w * 128 * (sby - 1);
                    }
                    const enum IntraPredMode m =
                        bytefn(dav1d_prepare_intra_edges)(t->bx,
                                                          t->bx > ts->tiling.col_start,
                                                          t->by,
                                                          t->by > ts->tiling.row_start,
                                                          ts->tiling.col_end,
                                                          ts->tiling.row_end,
                                                          edge_flags, dst,
                                                          f->cur.stride[0], top_sb_edge,
                                                          b->y_mode, &angle,
                                                          t_dim->w, t_dim->h,
                                                          f->seq_hdr->intra_edge_filter,
                                                          edge HIGHBD_CALL_SUFFIX);
                    dsp->ipred.intra_pred[m](dst, f->cur.stride[0], edge,
                                             t_dim->w * 4, t_dim->h * 4,
                                             angle | intra_flags,
                                             4 * f->bw - 4 * t->bx,
                                             4 * f->bh - 4 * t->by
                                             HIGHBD_CALL_SUFFIX);

                    if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS) {
                        hex_dump(edge - t_dim->h * 4, t_dim->h * 4,
                                 t_dim->h * 4, 2, "l");
                        hex_dump(edge, 0, 1, 1, "tl");
                        hex_dump(edge + 1, t_dim->w * 4,
                                 t_dim->w * 4, 2, "t");
                        hex_dump(dst, f->cur.stride[0],
                                 t_dim->w * 4, t_dim->h * 4, "y-intra-pred");
                    }

                skip_y_pred: {}
                    if (!b->skip) {
                        coef *cf;
                        int eob;
                        enum TxfmType txtp;
                        if (f->frame_thread.pass) {
                            cf = ts->frame_thread.cf;
                            ts->frame_thread.cf += imin(t_dim->w, 8) * imin(t_dim->h, 8) * 16;
                            const struct CodedBlockInfo *const cbi =
                                &f->frame_thread.cbi[t->by * f->b4_stride + t->bx];
                            eob = cbi->eob[0];
                            txtp = cbi->txtp[0];
                        } else {
                            uint8_t cf_ctx;
                            cf = t->cf;
                            eob = decode_coefs(t, &t->a->lcoef[bx4 + x],
                                               &t->l.lcoef[by4 + y], b->tx, bs,
                                               b, 1, 0, cf, &txtp, &cf_ctx);
                            if (DEBUG_BLOCK_INFO)
                                printf("Post-y-cf-blk[tx=%d,txtp=%d,eob=%d]: r=%d\n",
                                       b->tx, txtp, eob, ts->msac.rng);
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
                            rep_macro(type, t->dir lcoef, off, mul * cf_ctx)
#define default_memset(dir, diridx, off, sz) \
                            memset(&t->dir lcoef[off], cf_ctx, sz)
                            case_set_upto16_with_default(imin(t_dim->h, f->bh - t->by), \
                                                         l., 1, by4 + y);
                            case_set_upto16_with_default(imin(t_dim->w, f->bw - t->bx), \
                                                         a->, 0, bx4 + x);
#undef default_memset
#undef set_ctx
                        }
                        if (eob >= 0) {
                            if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS)
                                coef_dump(cf, imin(t_dim->h, 8) * 4,
                                          imin(t_dim->w, 8) * 4, 3, "dq");
                            dsp->itx.itxfm_add[b->tx]
                                              [txtp](dst,
                                                     f->cur.stride[0],
                                                     cf, eob HIGHBD_CALL_SUFFIX);
                            if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS)
                                hex_dump(dst, f->cur.stride[0],
                                         t_dim->w * 4, t_dim->h * 4, "recon");
                        }
                    } else if (!f->frame_thread.pass) {
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
                        rep_macro(type, t->dir lcoef, off, mul * 0x40)
                        case_set_upto16(t_dim->h, l., 1, by4 + y);
                        case_set_upto16(t_dim->w, a->, 0, bx4 + x);
#undef set_ctx
                    }
                    dst += 4 * t_dim->w;
                }
                t->bx -= x;
            }
            t->by -= y;

            if (!has_chroma) continue;

            const ptrdiff_t stride = f->cur.stride[1];

            if (b->uv_mode == CFL_PRED) {
                assert(!init_x && !init_y);

                int16_t *const ac = t->scratch.ac;
                pixel *y_src = ((pixel *) f->cur.data[0]) + 4 * (t->bx & ~ss_hor) +
                                 4 * (t->by & ~ss_ver) * PXSTRIDE(f->cur.stride[0]);
                const ptrdiff_t uv_off = 4 * ((t->bx >> ss_hor) +
                                              (t->by >> ss_ver) * PXSTRIDE(stride));
                pixel *const uv_dst[2] = { ((pixel *) f->cur.data[1]) + uv_off,
                                           ((pixel *) f->cur.data[2]) + uv_off };

                const int furthest_r =
                    ((cw4 << ss_hor) + t_dim->w - 1) & ~(t_dim->w - 1);
                const int furthest_b =
                    ((ch4 << ss_ver) + t_dim->h - 1) & ~(t_dim->h - 1);
                dsp->ipred.cfl_ac[f->cur.p.layout - 1](ac, y_src, f->cur.stride[0],
                                                         cbw4 - (furthest_r >> ss_hor),
                                                         cbh4 - (furthest_b >> ss_ver),
                                                         cbw4 * 4, cbh4 * 4);
                for (int pl = 0; pl < 2; pl++) {
                    if (!b->cfl_alpha[pl]) continue;
                    int angle = 0;
                    const pixel *top_sb_edge = NULL;
                    if (!((t->by & ~ss_ver) & (f->sb_step - 1))) {
                        top_sb_edge = f->ipred_edge[pl + 1];
                        const int sby = t->by >> f->sb_shift;
                        top_sb_edge += f->sb128w * 128 * (sby - 1);
                    }
                    const int xpos = t->bx >> ss_hor, ypos = t->by >> ss_ver;
                    const int xstart = ts->tiling.col_start >> ss_hor;
                    const int ystart = ts->tiling.row_start >> ss_ver;
                    const enum IntraPredMode m =
                        bytefn(dav1d_prepare_intra_edges)(xpos, xpos > xstart,
                                                          ypos, ypos > ystart,
                                                          ts->tiling.col_end >> ss_hor,
                                                          ts->tiling.row_end >> ss_ver,
                                                          0, uv_dst[pl], stride,
                                                          top_sb_edge, DC_PRED, &angle,
                                                          uv_t_dim->w, uv_t_dim->h, 0,
                                                          edge HIGHBD_CALL_SUFFIX);
                    dsp->ipred.cfl_pred[m](uv_dst[pl], stride, edge,
                                           uv_t_dim->w * 4,
                                           uv_t_dim->h * 4,
                                           ac, b->cfl_alpha[pl]
                                           HIGHBD_CALL_SUFFIX);
                }
                if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS) {
                    ac_dump(ac, 4*cbw4, 4*cbh4, "ac");
                    hex_dump(uv_dst[0], stride, cbw4 * 4, cbh4 * 4, "u-cfl-pred");
                    hex_dump(uv_dst[1], stride, cbw4 * 4, cbh4 * 4, "v-cfl-pred");
                }
            } else if (b->pal_sz[1]) {
                ptrdiff_t uv_dstoff = 4 * ((t->bx >> ss_hor) +
                                           (t->by >> ss_ver) * PXSTRIDE(f->cur.stride[1]));
                const uint8_t *pal_idx;
                if (f->frame_thread.pass) {
                    pal_idx = ts->frame_thread.pal_idx;
                    ts->frame_thread.pal_idx += cbw4 * cbh4 * 16;
                } else {
                    pal_idx = &t->scratch.pal_idx[bw4 * bh4 * 16];
                }
                const uint16_t *const pal_u = f->frame_thread.pass ?
                    f->frame_thread.pal[((t->by >> 1) + (t->bx & 1)) * (f->b4_stride >> 1) +
                                        ((t->bx >> 1) + (t->by & 1))][1] : t->pal[1];
                f->dsp->ipred.pal_pred(((pixel *) f->cur.data[1]) + uv_dstoff,
                                       f->cur.stride[1], pal_u,
                                       pal_idx, cbw4 * 4, cbh4 * 4);
                const uint16_t *const pal_v = f->frame_thread.pass ?
                    f->frame_thread.pal[((t->by >> 1) + (t->bx & 1)) * (f->b4_stride >> 1) +
                                        ((t->bx >> 1) + (t->by & 1))][2] : t->pal[2];
                f->dsp->ipred.pal_pred(((pixel *) f->cur.data[2]) + uv_dstoff,
                                       f->cur.stride[1], pal_v,
                                       pal_idx, cbw4 * 4, cbh4 * 4);
                if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS) {
                    hex_dump(((pixel *) f->cur.data[1]) + uv_dstoff,
                             PXSTRIDE(f->cur.stride[1]),
                             cbw4 * 4, cbh4 * 4, "u-pal-pred");
                    hex_dump(((pixel *) f->cur.data[2]) + uv_dstoff,
                             PXSTRIDE(f->cur.stride[1]),
                             cbw4 * 4, cbh4 * 4, "v-pal-pred");
                }
            }

            const int sm_uv_fl = sm_uv_flag(t->a, cbx4) |
                                 sm_uv_flag(&t->l, cby4);
            const int uv_sb_has_tr =
                ((init_x + 16) >> ss_hor) < cw4 ? 1 : init_y ? 0 :
                intra_edge_flags & (EDGE_I420_TOP_HAS_RIGHT >> (f->cur.p.layout - 1));
            const int uv_sb_has_bl =
                init_x ? 0 : ((init_y + 16) >> ss_ver) < ch4 ? 1 :
                intra_edge_flags & (EDGE_I420_LEFT_HAS_BOTTOM >> (f->cur.p.layout - 1));
            const int sub_ch4 = imin(ch4, (init_y + 16) >> ss_ver);
            const int sub_cw4 = imin(cw4, (init_x + 16) >> ss_hor);
            for (int pl = 0; pl < 2; pl++) {
                for (y = init_y >> ss_ver, t->by += init_y; y < sub_ch4;
                     y += uv_t_dim->h, t->by += uv_t_dim->h << ss_ver)
                {
                    pixel *dst = ((pixel *) f->cur.data[1 + pl]) +
                                   4 * ((t->by >> ss_ver) * PXSTRIDE(stride) +
                                        ((t->bx + init_x) >> ss_hor));
                    for (x = init_x >> ss_hor, t->bx += init_x; x < sub_cw4;
                         x += uv_t_dim->w, t->bx += uv_t_dim->w << ss_hor)
                    {
                        if ((b->uv_mode == CFL_PRED && b->cfl_alpha[pl]) ||
                            b->pal_sz[1])
                        {
                            goto skip_uv_pred;
                        }

                        int angle = b->uv_angle;
                        // this probably looks weird because we're using
                        // luma flags in a chroma loop, but that's because
                        // prepare_intra_edges() expects luma flags as input
                        const enum EdgeFlags edge_flags =
                            (((y > (init_y >> ss_ver) || !uv_sb_has_tr) &&
                              (x + uv_t_dim->w >= sub_cw4)) ?
                                 0 : EDGE_I444_TOP_HAS_RIGHT) |
                            ((x > (init_x >> ss_hor) ||
                              (!uv_sb_has_bl && y + uv_t_dim->h >= sub_ch4)) ?
                                 0 : EDGE_I444_LEFT_HAS_BOTTOM);
                        const pixel *top_sb_edge = NULL;
                        if (!((t->by & ~ss_ver) & (f->sb_step - 1))) {
                            top_sb_edge = f->ipred_edge[1 + pl];
                            const int sby = t->by >> f->sb_shift;
                            top_sb_edge += f->sb128w * 128 * (sby - 1);
                        }
                        const enum IntraPredMode uv_mode =
                             b->uv_mode == CFL_PRED ? DC_PRED : b->uv_mode;
                        const int xpos = t->bx >> ss_hor, ypos = t->by >> ss_ver;
                        const int xstart = ts->tiling.col_start >> ss_hor;
                        const int ystart = ts->tiling.row_start >> ss_ver;
                        const enum IntraPredMode m =
                            bytefn(dav1d_prepare_intra_edges)(xpos, xpos > xstart,
                                                              ypos, ypos > ystart,
                                                              ts->tiling.col_end >> ss_hor,
                                                              ts->tiling.row_end >> ss_ver,
                                                              edge_flags, dst, stride,
                                                              top_sb_edge, uv_mode,
                                                              &angle, uv_t_dim->w,
                                                              uv_t_dim->h,
                                                              f->seq_hdr->intra_edge_filter,
                                                              edge HIGHBD_CALL_SUFFIX);
                        angle |= intra_edge_filter_flag;
                        dsp->ipred.intra_pred[m](dst, stride, edge,
                                                 uv_t_dim->w * 4,
                                                 uv_t_dim->h * 4,
                                                 angle | sm_uv_fl,
                                                 (4 * f->bw + ss_hor -
                                                  4 * (t->bx & ~ss_hor)) >> ss_hor,
                                                 (4 * f->bh + ss_ver -
                                                  4 * (t->by & ~ss_ver)) >> ss_ver
                                                 HIGHBD_CALL_SUFFIX);
                        if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS) {
                            hex_dump(edge - uv_t_dim->h * 4, uv_t_dim->h * 4,
                                     uv_t_dim->h * 4, 2, "l");
                            hex_dump(edge, 0, 1, 1, "tl");
                            hex_dump(edge + 1, uv_t_dim->w * 4,
                                     uv_t_dim->w * 4, 2, "t");
                            hex_dump(dst, stride, uv_t_dim->w * 4,
                                     uv_t_dim->h * 4, pl ? "v-intra-pred" : "u-intra-pred");
                        }

                    skip_uv_pred: {}
                        if (!b->skip) {
                            enum TxfmType txtp;
                            int eob;
                            coef *cf;
                            if (f->frame_thread.pass) {
                                cf = ts->frame_thread.cf;
                                ts->frame_thread.cf += uv_t_dim->w * uv_t_dim->h * 16;
                                const struct CodedBlockInfo *const cbi =
                                    &f->frame_thread.cbi[t->by * f->b4_stride + t->bx];
                                eob = cbi->eob[pl + 1];
                                txtp = cbi->txtp[pl + 1];
                            } else {
                                uint8_t cf_ctx;
                                cf = t->cf;
                                eob = decode_coefs(t, &t->a->ccoef[pl][cbx4 + x],
                                                   &t->l.ccoef[pl][cby4 + y],
                                                   b->uvtx, bs, b, 1, 1 + pl, cf,
                                                   &txtp, &cf_ctx);
                                if (DEBUG_BLOCK_INFO)
                                    printf("Post-uv-cf-blk[pl=%d,tx=%d,"
                                           "txtp=%d,eob=%d]: r=%d [x=%d,cbx4=%d]\n",
                                           pl, b->uvtx, txtp, eob, ts->msac.rng, x, cbx4);
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
                                rep_macro(type, t->dir ccoef[pl], off, mul * cf_ctx)
#define default_memset(dir, diridx, off, sz) \
                                memset(&t->dir ccoef[pl][off], cf_ctx, sz)
                                case_set_upto16_with_default( \
                                         imin(uv_t_dim->h, (f->bh - t->by + ss_ver) >> ss_ver),
                                         l., 1, cby4 + y);
                                case_set_upto16_with_default( \
                                         imin(uv_t_dim->w, (f->bw - t->bx + ss_hor) >> ss_hor),
                                         a->, 0, cbx4 + x);
#undef default_memset
#undef set_ctx
                            }
                            if (eob >= 0) {
                                if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS)
                                    coef_dump(cf, uv_t_dim->h * 4,
                                              uv_t_dim->w * 4, 3, "dq");
                                dsp->itx.itxfm_add[b->uvtx]
                                                  [txtp](dst, stride,
                                                         cf, eob HIGHBD_CALL_SUFFIX);
                                if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS)
                                    hex_dump(dst, stride, uv_t_dim->w * 4,
                                             uv_t_dim->h * 4, "recon");
                            }
                        } else if (!f->frame_thread.pass) {
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
                            rep_macro(type, t->dir ccoef[pl], off, mul * 0x40)
                            case_set_upto16(uv_t_dim->h, l., 1, cby4 + y);
                            case_set_upto16(uv_t_dim->w, a->, 0, cbx4 + x);
#undef set_ctx
                        }
                        dst += uv_t_dim->w * 4;
                    }
                    t->bx -= x << ss_hor;
                }
                t->by -= y << ss_ver;
            }
        }
    }
}

int bytefn(dav1d_recon_b_inter)(Dav1dTileContext *const t, const enum BlockSize bs,
                                 const Av1Block *const b)
{
    Dav1dTileState *const ts = t->ts;
    const Dav1dFrameContext *const f = t->f;
    const Dav1dDSPContext *const dsp = f->dsp;
    const int bx4 = t->bx & 31, by4 = t->by & 31;
    const int ss_ver = f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    const int ss_hor = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
    const int cbx4 = bx4 >> ss_hor, cby4 = by4 >> ss_ver;
    const uint8_t *const b_dim = dav1d_block_dimensions[bs];
    const int bw4 = b_dim[0], bh4 = b_dim[1];
    const int w4 = imin(bw4, f->bw - t->bx), h4 = imin(bh4, f->bh - t->by);
    const int has_chroma = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I400 &&
                           (bw4 > ss_hor || t->bx & 1) &&
                           (bh4 > ss_ver || t->by & 1);
    const int chr_layout_idx = f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I400 ? 0 :
                               DAV1D_PIXEL_LAYOUT_I444 - f->cur.p.layout;
    int res;

    // prediction
    const int cbh4 = (bh4 + ss_ver) >> ss_ver, cbw4 = (bw4 + ss_hor) >> ss_hor;
    pixel *dst = ((pixel *) f->cur.data[0]) +
        4 * (t->by * PXSTRIDE(f->cur.stride[0]) + t->bx);
    const ptrdiff_t uvdstoff =
        4 * ((t->bx >> ss_hor) + (t->by >> ss_ver) * PXSTRIDE(f->cur.stride[1]));
    if (!(f->frame_hdr->frame_type & 1)) {
        // intrabc
        assert(!f->frame_hdr->super_res.enabled);
        res = mc(t, dst, NULL, f->cur.stride[0], bw4, bh4, t->bx, t->by, 0,
                 b->mv[0], &f->sr_cur, 0 /* unused */, FILTER_2D_BILINEAR);
        if (res) return res;
        if (has_chroma) for (int pl = 1; pl < 3; pl++) {
            res = mc(t, ((pixel *)f->cur.data[pl]) + uvdstoff, NULL, f->cur.stride[1],
                     bw4 << (bw4 == ss_hor), bh4 << (bh4 == ss_ver),
                     t->bx & ~ss_hor, t->by & ~ss_ver, pl, b->mv[0],
                     &f->sr_cur, 0 /* unused */, FILTER_2D_BILINEAR);
            if (res) return res;
        }
    } else if (b->comp_type == COMP_INTER_NONE) {
        const Dav1dThreadPicture *const refp = &f->refp[b->ref[0]];
        const enum Filter2d filter_2d = b->filter2d;

        if (imin(bw4, bh4) > 1 &&
            ((b->inter_mode == GLOBALMV && f->gmv_warp_allowed[b->ref[0]]) ||
             (b->motion_mode == MM_WARP && t->warpmv.type > DAV1D_WM_TYPE_TRANSLATION)))
        {
            res = warp_affine(t, dst, NULL, f->cur.stride[0], b_dim, 0, refp,
                              b->motion_mode == MM_WARP ? &t->warpmv :
                                  &f->frame_hdr->gmv[b->ref[0]]);
            if (res) return res;
        } else {
            res = mc(t, dst, NULL, f->cur.stride[0],
                     bw4, bh4, t->bx, t->by, 0, b->mv[0], refp, b->ref[0], filter_2d);
            if (res) return res;
            if (b->motion_mode == MM_OBMC) {
                res = obmc(t, dst, f->cur.stride[0], b_dim, 0, bx4, by4, w4, h4);
                if (res) return res;
            }
        }
        if (b->interintra_type) {
            ALIGN_STK_32(pixel, tl_edge_buf, 65,);
            pixel *const tl_edge = tl_edge_buf + 32;
            enum IntraPredMode m = b->interintra_mode == II_SMOOTH_PRED ?
                                   SMOOTH_PRED : b->interintra_mode;
            pixel *const tmp = t->scratch.interintra;
            int angle = 0;
            const pixel *top_sb_edge = NULL;
            if (!(t->by & (f->sb_step - 1))) {
                top_sb_edge = f->ipred_edge[0];
                const int sby = t->by >> f->sb_shift;
                top_sb_edge += f->sb128w * 128 * (sby - 1);
            }
            m = bytefn(dav1d_prepare_intra_edges)(t->bx, t->bx > ts->tiling.col_start,
                                                  t->by, t->by > ts->tiling.row_start,
                                                  ts->tiling.col_end, ts->tiling.row_end,
                                                  0, dst, f->cur.stride[0], top_sb_edge,
                                                  m, &angle, bw4, bh4, 0, tl_edge
                                                  HIGHBD_CALL_SUFFIX);
            dsp->ipred.intra_pred[m](tmp, 4 * bw4 * sizeof(pixel),
                                     tl_edge, bw4 * 4, bh4 * 4, 0, 0, 0
                                     HIGHBD_CALL_SUFFIX);
            const uint8_t *const ii_mask =
                b->interintra_type == INTER_INTRA_BLEND ?
                     dav1d_ii_masks[bs][0][b->interintra_mode] :
                     dav1d_wedge_masks[bs][0][0][b->wedge_idx];
            dsp->mc.blend(dst, f->cur.stride[0], tmp,
                          bw4 * 4, bh4 * 4, ii_mask);
        }

        if (!has_chroma) goto skip_inter_chroma_pred;

        // sub8x8 derivation
        int is_sub8x8 = bw4 == ss_hor || bh4 == ss_ver;
        refmvs *r;
        if (is_sub8x8) {
            assert(ss_hor == 1);
            r = &f->mvs[t->by * f->b4_stride + t->bx];
            if (bw4 == 1) is_sub8x8 &= r[-1].ref[0] > 0;
            if (bh4 == ss_ver) is_sub8x8 &= r[-f->b4_stride].ref[0] > 0;
            if (bw4 == 1 && bh4 == ss_ver)
                is_sub8x8 &= r[-(1 + f->b4_stride)].ref[0] > 0;
        }

        // chroma prediction
        if (is_sub8x8) {
            assert(ss_hor == 1);
            ptrdiff_t h_off = 0, v_off = 0;
            if (bw4 == 1 && bh4 == ss_ver) {
                for (int pl = 0; pl < 2; pl++) {
                    res = mc(t, ((pixel *) f->cur.data[1 + pl]) + uvdstoff,
                             NULL, f->cur.stride[1],
                             bw4, bh4, t->bx - 1, t->by - 1, 1 + pl,
                             r[-(f->b4_stride + 1)].mv[0],
                             &f->refp[r[-(f->b4_stride + 1)].ref[0] - 1],
                             r[-(f->b4_stride + 1)].ref[0] - 1,
                             f->frame_thread.pass != 2 ? t->tl_4x4_filter :
                                 f->frame_thread.b[((t->by - 1) * f->b4_stride) + t->bx - 1].filter2d);
                    if (res) return res;
                }
                v_off = 2 * PXSTRIDE(f->cur.stride[1]);
                h_off = 2;
            }
            if (bw4 == 1) {
                const enum Filter2d left_filter_2d =
                    dav1d_filter_2d[t->l.filter[1][by4]][t->l.filter[0][by4]];
                for (int pl = 0; pl < 2; pl++) {
                    res = mc(t, ((pixel *) f->cur.data[1 + pl]) + uvdstoff + v_off, NULL,
                             f->cur.stride[1], bw4, bh4, t->bx - 1,
                             t->by, 1 + pl, r[-1].mv[0], &f->refp[r[-1].ref[0] - 1],
                             r[-1].ref[0] - 1,
                             f->frame_thread.pass != 2 ? left_filter_2d :
                                 f->frame_thread.b[(t->by * f->b4_stride) + t->bx - 1].filter2d);
                    if (res) return res;
                }
                h_off = 2;
            }
            if (bh4 == ss_ver) {
                const enum Filter2d top_filter_2d =
                    dav1d_filter_2d[t->a->filter[1][bx4]][t->a->filter[0][bx4]];
                for (int pl = 0; pl < 2; pl++) {
                    res = mc(t, ((pixel *) f->cur.data[1 + pl]) + uvdstoff + h_off, NULL,
                             f->cur.stride[1], bw4, bh4, t->bx, t->by - 1,
                             1 + pl, r[-f->b4_stride].mv[0],
                             &f->refp[r[-f->b4_stride].ref[0] - 1],
                             r[-f->b4_stride].ref[0] - 1,
                             f->frame_thread.pass != 2 ? top_filter_2d :
                                 f->frame_thread.b[((t->by - 1) * f->b4_stride) + t->bx].filter2d);
                    if (res) return res;
                }
                v_off = 2 * PXSTRIDE(f->cur.stride[1]);
            }
            for (int pl = 0; pl < 2; pl++) {
                res = mc(t, ((pixel *) f->cur.data[1 + pl]) + uvdstoff + h_off + v_off, NULL, f->cur.stride[1],
                         bw4, bh4, t->bx, t->by, 1 + pl, b->mv[0],
                         refp, b->ref[0], filter_2d);
                if (res) return res;
            }
        } else {
            if (imin(cbw4, cbh4) > 1 &&
                ((b->inter_mode == GLOBALMV && f->gmv_warp_allowed[b->ref[0]]) ||
                 (b->motion_mode == MM_WARP && t->warpmv.type > DAV1D_WM_TYPE_TRANSLATION)))
            {
                for (int pl = 0; pl < 2; pl++) {
                    res = warp_affine(t, ((pixel *) f->cur.data[1 + pl]) + uvdstoff, NULL,
                                      f->cur.stride[1], b_dim, 1 + pl, refp,
                                      b->motion_mode == MM_WARP ? &t->warpmv :
                                          &f->frame_hdr->gmv[b->ref[0]]);
                    if (res) return res;
                }
            } else {
                for (int pl = 0; pl < 2; pl++) {
                    res = mc(t, ((pixel *) f->cur.data[1 + pl]) + uvdstoff,
                             NULL, f->cur.stride[1],
                             bw4 << (bw4 == ss_hor), bh4 << (bh4 == ss_ver),
                             t->bx & ~ss_hor, t->by & ~ss_ver,
                             1 + pl, b->mv[0], refp, b->ref[0], filter_2d);
                    if (res) return res;
                    if (b->motion_mode == MM_OBMC) {
                        res = obmc(t, ((pixel *) f->cur.data[1 + pl]) + uvdstoff,
                                   f->cur.stride[1], b_dim, 1 + pl, bx4, by4, w4, h4);
                        if (res) return res;
                    }
                }
            }
            if (b->interintra_type) {
                // FIXME for 8x32 with 4:2:2 subsampling, this probably does
                // the wrong thing since it will select 4x16, not 4x32, as a
                // transform size...
                const uint8_t *const ii_mask =
                    b->interintra_type == INTER_INTRA_BLEND ?
                         dav1d_ii_masks[bs][chr_layout_idx][b->interintra_mode] :
                         dav1d_wedge_masks[bs][chr_layout_idx][0][b->wedge_idx];

                for (int pl = 0; pl < 2; pl++) {
                    pixel *const tmp = t->scratch.interintra;
                    ALIGN_STK_32(pixel, tl_edge_px, 65,);
                    pixel *const tl_edge = &tl_edge_px[32];
                    enum IntraPredMode m =
                        b->interintra_mode == II_SMOOTH_PRED ?
                        SMOOTH_PRED : b->interintra_mode;
                    int angle = 0;
                    pixel *const uvdst = ((pixel *) f->cur.data[1 + pl]) + uvdstoff;
                    const pixel *top_sb_edge = NULL;
                    if (!(t->by & (f->sb_step - 1))) {
                        top_sb_edge = f->ipred_edge[pl + 1];
                        const int sby = t->by >> f->sb_shift;
                        top_sb_edge += f->sb128w * 128 * (sby - 1);
                    }
                    m = bytefn(dav1d_prepare_intra_edges)(t->bx >> ss_hor,
                                                          (t->bx >> ss_hor) >
                                                              (ts->tiling.col_start >> ss_hor),
                                                          t->by >> ss_ver,
                                                          (t->by >> ss_ver) >
                                                              (ts->tiling.row_start >> ss_ver),
                                                          ts->tiling.col_end >> ss_hor,
                                                          ts->tiling.row_end >> ss_ver,
                                                          0, uvdst, f->cur.stride[1],
                                                          top_sb_edge, m,
                                                          &angle, cbw4, cbh4, 0, tl_edge
                                                          HIGHBD_CALL_SUFFIX);
                    dsp->ipred.intra_pred[m](tmp, cbw4 * 4 * sizeof(pixel),
                                             tl_edge, cbw4 * 4, cbh4 * 4, 0, 0, 0
                                             HIGHBD_CALL_SUFFIX);
                    dsp->mc.blend(uvdst, f->cur.stride[1], tmp,
                                  cbw4 * 4, cbh4 * 4, ii_mask);
                }
            }
        }

    skip_inter_chroma_pred: {}
        t->tl_4x4_filter = filter_2d;
    } else {
        const enum Filter2d filter_2d = b->filter2d;
        // Maximum super block size is 128x128
        int16_t (*tmp)[128 * 128] = (int16_t (*)[128 * 128]) t->scratch.compinter;
        int jnt_weight;
        uint8_t *const seg_mask = t->scratch_seg_mask;
        const uint8_t *mask;

        for (int i = 0; i < 2; i++) {
            const Dav1dThreadPicture *const refp = &f->refp[b->ref[i]];

            if (b->inter_mode == GLOBALMV_GLOBALMV && f->gmv_warp_allowed[b->ref[i]]) {
                res = warp_affine(t, NULL, tmp[i], bw4 * 4, b_dim, 0, refp,
                                  &f->frame_hdr->gmv[b->ref[i]]);
                if (res) return res;
            } else {
                res = mc(t, NULL, tmp[i], 0, bw4, bh4, t->bx, t->by, 0,
                         b->mv[i], refp, b->ref[i], filter_2d);
                if (res) return res;
            }
        }
        switch (b->comp_type) {
        case COMP_INTER_AVG:
            dsp->mc.avg(dst, f->cur.stride[0], tmp[0], tmp[1],
                        bw4 * 4, bh4 * 4 HIGHBD_CALL_SUFFIX);
            break;
        case COMP_INTER_WEIGHTED_AVG:
            jnt_weight = f->jnt_weights[b->ref[0]][b->ref[1]];
            dsp->mc.w_avg(dst, f->cur.stride[0], tmp[0], tmp[1],
                          bw4 * 4, bh4 * 4, jnt_weight HIGHBD_CALL_SUFFIX);
            break;
        case COMP_INTER_SEG:
            dsp->mc.w_mask[chr_layout_idx](dst, f->cur.stride[0],
                                           tmp[b->mask_sign], tmp[!b->mask_sign],
                                           bw4 * 4, bh4 * 4, seg_mask,
                                           b->mask_sign HIGHBD_CALL_SUFFIX);
            mask = seg_mask;
            break;
        case COMP_INTER_WEDGE:
            mask = dav1d_wedge_masks[bs][0][0][b->wedge_idx];
            dsp->mc.mask(dst, f->cur.stride[0],
                         tmp[b->mask_sign], tmp[!b->mask_sign],
                         bw4 * 4, bh4 * 4, mask HIGHBD_CALL_SUFFIX);
            if (has_chroma)
                mask = dav1d_wedge_masks[bs][chr_layout_idx][b->mask_sign][b->wedge_idx];
            break;
        }

        // chroma
        if (has_chroma) for (int pl = 0; pl < 2; pl++) {
            for (int i = 0; i < 2; i++) {
                const Dav1dThreadPicture *const refp = &f->refp[b->ref[i]];
                if (b->inter_mode == GLOBALMV_GLOBALMV &&
                    imin(cbw4, cbh4) > 1 && f->gmv_warp_allowed[b->ref[i]])
                {
                    res = warp_affine(t, NULL, tmp[i], bw4 * 4 >> ss_hor,
                                      b_dim, 1 + pl,
                                      refp, &f->frame_hdr->gmv[b->ref[i]]);
                    if (res) return res;
                } else {
                    res = mc(t, NULL, tmp[i], 0, bw4, bh4, t->bx, t->by,
                             1 + pl, b->mv[i], refp, b->ref[i], filter_2d);
                    if (res) return res;
                }
            }
            pixel *const uvdst = ((pixel *) f->cur.data[1 + pl]) + uvdstoff;
            switch (b->comp_type) {
            case COMP_INTER_AVG:
                dsp->mc.avg(uvdst, f->cur.stride[1], tmp[0], tmp[1],
                            bw4 * 4 >> ss_hor, bh4 * 4 >> ss_ver
                            HIGHBD_CALL_SUFFIX);
                break;
            case COMP_INTER_WEIGHTED_AVG:
                dsp->mc.w_avg(uvdst, f->cur.stride[1], tmp[0], tmp[1],
                              bw4 * 4 >> ss_hor, bh4 * 4 >> ss_ver, jnt_weight
                              HIGHBD_CALL_SUFFIX);
                break;
            case COMP_INTER_WEDGE:
            case COMP_INTER_SEG:
                dsp->mc.mask(uvdst, f->cur.stride[1],
                             tmp[b->mask_sign], tmp[!b->mask_sign],
                             bw4 * 4 >> ss_hor, bh4 * 4 >> ss_ver, mask
                             HIGHBD_CALL_SUFFIX);
                break;
            }
        }
    }

    if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS) {
        hex_dump(dst, f->cur.stride[0], b_dim[0] * 4, b_dim[1] * 4, "y-pred");
        if (has_chroma) {
            hex_dump(&((pixel *) f->cur.data[1])[uvdstoff], f->cur.stride[1],
                     cbw4 * 4, cbh4 * 4, "u-pred");
            hex_dump(&((pixel *) f->cur.data[2])[uvdstoff], f->cur.stride[1],
                     cbw4 * 4, cbh4 * 4, "v-pred");
        }
    }

    const int cw4 = (w4 + ss_hor) >> ss_hor, ch4 = (h4 + ss_ver) >> ss_ver;

    if (b->skip) {
        // reset coef contexts
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
        rep_macro(type, t->dir lcoef, off, mul * 0x40)
        case_set(bh4, l., 1, by4);
        case_set(bw4, a->, 0, bx4);
#undef set_ctx
        if (has_chroma) {
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
            rep_macro(type, t->dir ccoef[0], off, mul * 0x40); \
            rep_macro(type, t->dir ccoef[1], off, mul * 0x40)
            case_set(cbh4, l., 1, cby4);
            case_set(cbw4, a->, 0, cbx4);
#undef set_ctx
        }
        return 0;
    }

    const TxfmInfo *const uvtx = &dav1d_txfm_dimensions[b->uvtx];
    const TxfmInfo *const ytx = &dav1d_txfm_dimensions[b->max_ytx];

    for (int init_y = 0; init_y < bh4; init_y += 16) {
        for (int init_x = 0; init_x < bw4; init_x += 16) {
            // coefficient coding & inverse transforms
            int y_off = !!init_y, y;
            dst += PXSTRIDE(f->cur.stride[0]) * 4 * init_y;
            for (y = init_y, t->by += init_y; y < imin(h4, init_y + 16);
                 y += ytx->h, y_off++)
            {
                int x, x_off = !!init_x;
                for (x = init_x, t->bx += init_x; x < imin(w4, init_x + 16);
                     x += ytx->w, x_off++)
                {
                    read_coef_tree(t, bs, b, b->max_ytx, 0, b->tx_split,
                                   x_off, y_off, &dst[x * 4]);
                    t->bx += ytx->w;
                }
                dst += PXSTRIDE(f->cur.stride[0]) * 4 * ytx->h;
                t->bx -= x;
                t->by += ytx->h;
            }
            dst -= PXSTRIDE(f->cur.stride[0]) * 4 * y;
            t->by -= y;

            // chroma coefs and inverse transform
            if (has_chroma) for (int pl = 0; pl < 2; pl++) {
                pixel *uvdst = ((pixel *) f->cur.data[1 + pl]) + uvdstoff +
                    (PXSTRIDE(f->cur.stride[1]) * init_y * 4 >> ss_ver);
                for (y = init_y >> ss_ver, t->by += init_y;
                     y < imin(ch4, (init_y + 16) >> ss_ver); y += uvtx->h)
                {
                    int x;
                    for (x = init_x >> ss_hor, t->bx += init_x;
                         x < imin(cw4, (init_x + 16) >> ss_hor); x += uvtx->w)
                    {
                        coef *cf;
                        int eob;
                        enum TxfmType txtp;
                        if (f->frame_thread.pass) {
                            cf = ts->frame_thread.cf;
                            ts->frame_thread.cf += uvtx->w * uvtx->h * 16;
                            const struct CodedBlockInfo *const cbi =
                                &f->frame_thread.cbi[t->by * f->b4_stride + t->bx];
                            eob = cbi->eob[1 + pl];
                            txtp = cbi->txtp[1 + pl];
                        } else {
                            uint8_t cf_ctx;
                            cf = t->cf;
                            txtp = t->txtp_map[(by4 + (y << ss_ver)) * 32 +
                                                bx4 + (x << ss_hor)];
                            eob = decode_coefs(t, &t->a->ccoef[pl][cbx4 + x],
                                               &t->l.ccoef[pl][cby4 + y],
                                               b->uvtx, bs, b, 0, 1 + pl,
                                               cf, &txtp, &cf_ctx);
                            if (DEBUG_BLOCK_INFO)
                                printf("Post-uv-cf-blk[pl=%d,tx=%d,"
                                       "txtp=%d,eob=%d]: r=%d\n",
                                       pl, b->uvtx, txtp, eob, ts->msac.rng);
#define set_ctx(type, dir, diridx, off, mul, rep_macro) \
                            rep_macro(type, t->dir ccoef[pl], off, mul * cf_ctx)
#define default_memset(dir, diridx, off, sz) \
                            memset(&t->dir ccoef[pl][off], cf_ctx, sz)
                            case_set_upto16_with_default( \
                                     imin(uvtx->h, (f->bh - t->by + ss_ver) >> ss_ver),
                                     l., 1, cby4 + y);
                            case_set_upto16_with_default( \
                                     imin(uvtx->w, (f->bw - t->bx + ss_hor) >> ss_hor),
                                     a->, 0, cbx4 + x);
#undef default_memset
#undef set_ctx
                        }
                        if (eob >= 0) {
                            if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS)
                                coef_dump(cf, uvtx->h * 4, uvtx->w * 4, 3, "dq");
                            dsp->itx.itxfm_add[b->uvtx]
                                              [txtp](&uvdst[4 * x],
                                                     f->cur.stride[1],
                                                     cf, eob HIGHBD_CALL_SUFFIX);
                            if (DEBUG_BLOCK_INFO && DEBUG_B_PIXELS)
                                hex_dump(&uvdst[4 * x], f->cur.stride[1],
                                         uvtx->w * 4, uvtx->h * 4, "recon");
                        }
                        t->bx += uvtx->w << ss_hor;
                    }
                    uvdst += PXSTRIDE(f->cur.stride[1]) * 4 * uvtx->h;
                    t->bx -= x << ss_hor;
                    t->by += uvtx->h << ss_ver;
                }
                t->by -= y << ss_ver;
            }
        }
    }
    return 0;
}

void bytefn(dav1d_filter_sbrow)(Dav1dFrameContext *const f, const int sby) {
    const int sbsz = f->sb_step, sbh = f->sbh;

    if (f->frame_hdr->loopfilter.level_y[0] ||
        f->frame_hdr->loopfilter.level_y[1])
    {
        int start_of_tile_row = 0;
        if (f->frame_hdr->tiling.row_start_sb[f->lf.tile_row] == sby)
            start_of_tile_row = f->lf.tile_row++;
        bytefn(dav1d_loopfilter_sbrow)(f, f->lf.p, f->lf.mask_ptr, sby,
                                       start_of_tile_row);
    }

    if (f->seq_hdr->restoration) {
        // Store loop filtered pixels required by loop restoration
        bytefn(dav1d_lr_copy_lpf)(f, f->lf.p, sby);
    }
    if (f->seq_hdr->cdef) {
        if (sby) {
            const int ss_ver = f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
            pixel *p_up[3] = {
                f->lf.p[0] - 8 * PXSTRIDE(f->cur.stride[0]),
                f->lf.p[1] - (8 * PXSTRIDE(f->cur.stride[1]) >> ss_ver),
                f->lf.p[2] - (8 * PXSTRIDE(f->cur.stride[1]) >> ss_ver),
            };
            bytefn(dav1d_cdef_brow)(f, p_up, f->lf.prev_mask_ptr,
                                    sby * sbsz - 2, sby * sbsz);
        }
        const int n_blks = sbsz - 2 * (sby + 1 < sbh);
        bytefn(dav1d_cdef_brow)(f, f->lf.p, f->lf.mask_ptr, sby * sbsz,
                                imin(sby * sbsz + n_blks, f->bh));
    }
    if (f->frame_hdr->super_res.enabled) {
        const int has_chroma = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I400;
        for (int pl = 0; pl < 1 + 2 * has_chroma; pl++) {
            const int ss_ver = pl && f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
            const int h_start = 8 * !!sby >> ss_ver;
            const ptrdiff_t dst_stride = f->sr_cur.p.stride[!!pl];
            pixel *dst = f->lf.sr_p[pl] - h_start * PXSTRIDE(dst_stride);
            const ptrdiff_t src_stride = f->cur.stride[!!pl];
            const pixel *src = f->lf.p[pl] - h_start * PXSTRIDE(src_stride);
            const int h_end = 4 * (sbsz - 2 * (sby + 1 < sbh)) >> ss_ver;
            const int ss_hor = pl && f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;
            const int dst_w = (f->sr_cur.p.p.w + ss_hor) >> ss_hor;
            const int src_w = (4 * f->bw + ss_hor) >> ss_hor;
            const int img_h = (f->cur.p.h - sbsz * 4 * sby + ss_ver) >> ss_ver;

            f->dsp->mc.resize(dst, dst_stride, src, src_stride, dst_w, src_w,
                              imin(img_h, h_end) + h_start, f->resize_step[!!pl],
                              f->resize_start[!!pl] HIGHBD_CALL_SUFFIX);
        }
    }
    if (f->seq_hdr->restoration) {
        bytefn(dav1d_lr_sbrow)(f, f->lf.sr_p, sby);
    }

    const int ss_ver = f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
    f->lf.p[0] += sbsz * 4 * PXSTRIDE(f->cur.stride[0]);
    f->lf.p[1] += sbsz * 4 * PXSTRIDE(f->cur.stride[1]) >> ss_ver;
    f->lf.p[2] += sbsz * 4 * PXSTRIDE(f->cur.stride[1]) >> ss_ver;
    f->lf.sr_p[0] += sbsz * 4 * PXSTRIDE(f->sr_cur.p.stride[0]);
    f->lf.sr_p[1] += sbsz * 4 * PXSTRIDE(f->sr_cur.p.stride[1]) >> ss_ver;
    f->lf.sr_p[2] += sbsz * 4 * PXSTRIDE(f->sr_cur.p.stride[1]) >> ss_ver;
    f->lf.prev_mask_ptr = f->lf.mask_ptr;
    if ((sby & 1) || f->seq_hdr->sb128) {
        f->lf.mask_ptr += f->sb128w;
    }
}

void bytefn(dav1d_backup_ipred_edge)(Dav1dTileContext *const t) {
    const Dav1dFrameContext *const f = t->f;
    Dav1dTileState *const ts = t->ts;
    const int sby = t->by >> f->sb_shift;
    const int sby_off = f->sb128w * 128 * sby;
    const int x_off = ts->tiling.col_start;

    const pixel *const y =
        ((const pixel *) f->cur.data[0]) + x_off * 4 +
                    ((t->by + f->sb_step) * 4 - 1) * PXSTRIDE(f->cur.stride[0]);
    pixel_copy(&f->ipred_edge[0][sby_off + x_off * 4], y,
               4 * (ts->tiling.col_end - x_off));

    if (f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I400) {
        const int ss_ver = f->cur.p.layout == DAV1D_PIXEL_LAYOUT_I420;
        const int ss_hor = f->cur.p.layout != DAV1D_PIXEL_LAYOUT_I444;

        const ptrdiff_t uv_off = (x_off * 4 >> ss_hor) +
            (((t->by + f->sb_step) * 4 >> ss_ver) - 1) * PXSTRIDE(f->cur.stride[1]);
        for (int pl = 1; pl <= 2; pl++)
            pixel_copy(&f->ipred_edge[pl][sby_off + (x_off * 4 >> ss_hor)],
                       &((const pixel *) f->cur.data[pl])[uv_off],
                       4 * (ts->tiling.col_end - x_off) >> ss_hor);
    }
}
