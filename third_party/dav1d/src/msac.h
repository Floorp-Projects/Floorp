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

#ifndef DAV1D_SRC_MSAC_H
#define DAV1D_SRC_MSAC_H

#include <stdint.h>
#include <stdlib.h>

/* Using uint32_t should be faster on 32 bit systems, in theory, maybe */
typedef uint64_t ec_win;

typedef struct MsacContext {
    const uint8_t *buf_pos;
    const uint8_t *buf_end;
    ec_win dif;
    unsigned rng;
    int cnt;
    int allow_update_cdf;
} MsacContext;

void dav1d_msac_init(MsacContext *s, const uint8_t *data, size_t sz,
                     int disable_cdf_update_flag);
unsigned dav1d_msac_decode_symbol_adapt_c(MsacContext *s, uint16_t *cdf,
                                          size_t n_symbols);
unsigned dav1d_msac_decode_bool_equi(MsacContext *s);
unsigned dav1d_msac_decode_bool(MsacContext *s, unsigned f);
unsigned dav1d_msac_decode_bool_adapt(MsacContext *s, uint16_t *cdf);
unsigned dav1d_msac_decode_bools(MsacContext *s, unsigned n);
int dav1d_msac_decode_subexp(MsacContext *s, int ref, int n, unsigned k);
int dav1d_msac_decode_uniform(MsacContext *s, unsigned n);

/* Supported n_symbols ranges: adapt4: 1-5, adapt8: 1-8, adapt16: 4-16 */
#if ARCH_AARCH64 && HAVE_ASM
unsigned dav1d_msac_decode_symbol_adapt4_neon(MsacContext *s, uint16_t *cdf,
                                              size_t n_symbols);
unsigned dav1d_msac_decode_symbol_adapt8_neon(MsacContext *s, uint16_t *cdf,
                                              size_t n_symbols);
unsigned dav1d_msac_decode_symbol_adapt16_neon(MsacContext *s, uint16_t *cdf,
                                               size_t n_symbols);
#define dav1d_msac_decode_symbol_adapt4  dav1d_msac_decode_symbol_adapt4_neon
#define dav1d_msac_decode_symbol_adapt8  dav1d_msac_decode_symbol_adapt8_neon
#define dav1d_msac_decode_symbol_adapt16 dav1d_msac_decode_symbol_adapt16_neon
#elif ARCH_X86_64 && HAVE_ASM
unsigned dav1d_msac_decode_symbol_adapt4_sse2(MsacContext *s, uint16_t *cdf,
                                              size_t n_symbols);
unsigned dav1d_msac_decode_symbol_adapt8_sse2(MsacContext *s, uint16_t *cdf,
                                              size_t n_symbols);
unsigned dav1d_msac_decode_symbol_adapt16_sse2(MsacContext *s, uint16_t *cdf,
                                               size_t n_symbols);
#define dav1d_msac_decode_symbol_adapt4  dav1d_msac_decode_symbol_adapt4_sse2
#define dav1d_msac_decode_symbol_adapt8  dav1d_msac_decode_symbol_adapt8_sse2
#define dav1d_msac_decode_symbol_adapt16 dav1d_msac_decode_symbol_adapt16_sse2
#else
#define dav1d_msac_decode_symbol_adapt4  dav1d_msac_decode_symbol_adapt_c
#define dav1d_msac_decode_symbol_adapt8  dav1d_msac_decode_symbol_adapt_c
#define dav1d_msac_decode_symbol_adapt16 dav1d_msac_decode_symbol_adapt_c
#endif

#endif /* DAV1D_SRC_MSAC_H */
