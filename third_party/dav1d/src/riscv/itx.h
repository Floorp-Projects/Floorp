/*
 * Copyright © 2018, VideoLAN and dav1d authors
 * Copyright © 2023, Nathan Egge
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

#include "src/cpu.h"
#include "src/itx.h"

#define decl_itx2_fns(w, h, opt) \
decl_itx_fn(BF(dav1d_inv_txfm_add_dct_dct_##w##x##h, opt)); \
decl_itx_fn(BF(dav1d_inv_txfm_add_identity_identity_##w##x##h, opt))

#define decl_itx12_fns(w, h, opt) \
decl_itx2_fns(w, h, opt); \
decl_itx_fn(BF(dav1d_inv_txfm_add_dct_adst_##w##x##h, opt)); \
decl_itx_fn(BF(dav1d_inv_txfm_add_dct_flipadst_##w##x##h, opt)); \
decl_itx_fn(BF(dav1d_inv_txfm_add_dct_identity_##w##x##h, opt)); \
decl_itx_fn(BF(dav1d_inv_txfm_add_adst_dct_##w##x##h, opt)); \
decl_itx_fn(BF(dav1d_inv_txfm_add_adst_adst_##w##x##h, opt)); \
decl_itx_fn(BF(dav1d_inv_txfm_add_adst_flipadst_##w##x##h, opt)); \
decl_itx_fn(BF(dav1d_inv_txfm_add_flipadst_dct_##w##x##h, opt)); \
decl_itx_fn(BF(dav1d_inv_txfm_add_flipadst_adst_##w##x##h, opt)); \
decl_itx_fn(BF(dav1d_inv_txfm_add_flipadst_flipadst_##w##x##h, opt)); \
decl_itx_fn(BF(dav1d_inv_txfm_add_identity_dct_##w##x##h, opt))

#define decl_itx16_fns(w, h, opt) \
decl_itx12_fns(w, h, opt); \
decl_itx_fn(BF(dav1d_inv_txfm_add_adst_identity_##w##x##h, opt)); \
decl_itx_fn(BF(dav1d_inv_txfm_add_flipadst_identity_##w##x##h, opt)); \
decl_itx_fn(BF(dav1d_inv_txfm_add_identity_adst_##w##x##h, opt)); \
decl_itx_fn(BF(dav1d_inv_txfm_add_identity_flipadst_##w##x##h, opt))

#define decl_itx17_fns(w, h, opt) \
decl_itx16_fns(w, h, opt); \
decl_itx_fn(BF(dav1d_inv_txfm_add_wht_wht_##w##x##h, opt))

#define decl_itx_fns(ext) \
decl_itx17_fns( 4,  4, ext); \
decl_itx16_fns( 8,  8, ext); \
decl_itx16_fns(16, 16, ext)

decl_itx_fns(rvv);

static ALWAYS_INLINE void itx_dsp_init_riscv(Dav1dInvTxfmDSPContext *const c, int const bpc) {
#define assign_itx_fn(pfx, w, h, type, type_enum, ext) \
    c->itxfm_add[pfx##TX_##w##X##h][type_enum] = \
        BF(dav1d_inv_txfm_add_##type##_##w##x##h, ext)

#define assign_itx1_fn(pfx, w, h, ext) \
    assign_itx_fn(pfx, w, h, dct_dct,           DCT_DCT,           ext)

#define assign_itx2_fn(pfx, w, h, ext) \
    assign_itx1_fn(pfx, w, h, ext); \
    assign_itx_fn(pfx, w, h, identity_identity, IDTX,              ext)

#define assign_itx12_fn(pfx, w, h, ext) \
    assign_itx2_fn(pfx, w, h, ext); \
    assign_itx_fn(pfx, w, h, dct_adst,          ADST_DCT,          ext); \
    assign_itx_fn(pfx, w, h, dct_flipadst,      FLIPADST_DCT,      ext); \
    assign_itx_fn(pfx, w, h, dct_identity,      H_DCT,             ext); \
    assign_itx_fn(pfx, w, h, adst_dct,          DCT_ADST,          ext); \
    assign_itx_fn(pfx, w, h, adst_adst,         ADST_ADST,         ext); \
    assign_itx_fn(pfx, w, h, adst_flipadst,     FLIPADST_ADST,     ext); \
    assign_itx_fn(pfx, w, h, flipadst_dct,      DCT_FLIPADST,      ext); \
    assign_itx_fn(pfx, w, h, flipadst_adst,     ADST_FLIPADST,     ext); \
    assign_itx_fn(pfx, w, h, flipadst_flipadst, FLIPADST_FLIPADST, ext); \
    assign_itx_fn(pfx, w, h, identity_dct,      V_DCT,             ext)

#define assign_itx16_fn(pfx, w, h, ext) \
    assign_itx12_fn(pfx, w, h, ext); \
    assign_itx_fn(pfx, w, h, adst_identity,     H_ADST,            ext); \
    assign_itx_fn(pfx, w, h, flipadst_identity, H_FLIPADST,        ext); \
    assign_itx_fn(pfx, w, h, identity_adst,     V_ADST,            ext); \
    assign_itx_fn(pfx, w, h, identity_flipadst, V_FLIPADST,        ext)

#define assign_itx17_fn(pfx, w, h, ext) \
    assign_itx16_fn(pfx, w, h, ext); \
    assign_itx_fn(pfx, w, h, wht_wht,           WHT_WHT,           ext)

  const unsigned flags = dav1d_get_cpu_flags();

  if (!(flags & DAV1D_RISCV_CPU_FLAG_V)) return;

#if BITDEPTH == 8
  assign_itx17_fn( ,  4,  4, rvv);
  assign_itx16_fn( ,  8,  8, rvv);
  assign_itx12_fn( , 16, 16, rvv);
#endif
}
