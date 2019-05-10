/*
 * Copyright © 2019, VideoLAN and dav1d authors
 * Copyright © 2019, Two Orioles, LLC
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

#include "tests/checkasm/checkasm.h"

#include "src/cpu.h"
#include "src/msac.h"

#include <string.h>

/* The normal code doesn't use function pointers */
typedef unsigned (*decode_symbol_adapt_fn)(MsacContext *s, uint16_t *cdf,
                                           size_t n_symbols);

typedef struct {
    decode_symbol_adapt_fn symbol_adapt4;
    decode_symbol_adapt_fn symbol_adapt8;
    decode_symbol_adapt_fn symbol_adapt16;
} MsacDSPContext;

static void randomize_cdf(uint16_t *const cdf, int n) {
    for (int i = 16; i > n; i--)
        cdf[i] = rnd(); /* randomize padding */
    cdf[n] = cdf[n-1] = 0;
    while (--n > 0)
        cdf[n-1] = cdf[n] + rnd() % (32768 - cdf[n] - n) + 1;
}

/* memcmp() on structs can have weird behavior due to padding etc. */
static int msac_cmp(const MsacContext *const a, const MsacContext *const b) {
    return a->buf_pos != b->buf_pos || a->buf_end != b->buf_end ||
           a->dif != b->dif || a->rng != b->rng || a->cnt != b->cnt ||
           a->allow_update_cdf != b->allow_update_cdf;
}

#define CHECK_SYMBOL_ADAPT(n, n_min, n_max) do {                           \
    if (check_func(c->symbol_adapt##n, "msac_decode_symbol_adapt%d", n)) { \
        for (int cdf_update = 0; cdf_update <= 1; cdf_update++) {          \
            for (int ns = n_min; ns <= n_max; ns++) {                      \
                dav1d_msac_init(&s_c, buf, sizeof(buf), !cdf_update);      \
                s_a = s_c;                                                 \
                randomize_cdf(cdf[0], ns);                                 \
                memcpy(cdf[1], cdf[0], sizeof(*cdf));                      \
                for (int i = 0; i < 64; i++) {                             \
                    unsigned c_res = call_ref(&s_c, cdf[0], ns);           \
                    unsigned a_res = call_new(&s_a, cdf[1], ns);           \
                    if (c_res != a_res || msac_cmp(&s_c, &s_a) ||          \
                        memcmp(cdf[0], cdf[1], sizeof(**cdf) * (ns + 1)))  \
                    {                                                      \
                        fail();                                            \
                    }                                                      \
                }                                                          \
                if (cdf_update && ns == n)                                 \
                    bench_new(&s_a, cdf[0], n);                            \
            }                                                              \
        }                                                                  \
    }                                                                      \
} while (0)

static void check_decode_symbol_adapt(MsacDSPContext *const c) {
    /* Use an aligned CDF buffer for more consistent benchmark
     * results, and a misaligned one for checking correctness. */
    ALIGN_STK_16(uint16_t, cdf, 2, [17]);
    MsacContext s_c, s_a;
    uint8_t buf[1024];
    for (int i = 0; i < 1024; i++)
        buf[i] = rnd();

    declare_func(unsigned, MsacContext *s, uint16_t *cdf, size_t n_symbols);
    CHECK_SYMBOL_ADAPT( 4, 1,  5);
    CHECK_SYMBOL_ADAPT( 8, 1,  8);
    CHECK_SYMBOL_ADAPT(16, 4, 16);
    report("decode_symbol_adapt");
}

void checkasm_check_msac(void) {
    MsacDSPContext c;
    c.symbol_adapt4  = dav1d_msac_decode_symbol_adapt_c;
    c.symbol_adapt8  = dav1d_msac_decode_symbol_adapt_c;
    c.symbol_adapt16 = dav1d_msac_decode_symbol_adapt_c;

#if ARCH_AARCH64 && HAVE_ASM
    if (dav1d_get_cpu_flags() & DAV1D_ARM_CPU_FLAG_NEON) {
        c.symbol_adapt4  = dav1d_msac_decode_symbol_adapt4_neon;
        c.symbol_adapt8  = dav1d_msac_decode_symbol_adapt8_neon;
        c.symbol_adapt16 = dav1d_msac_decode_symbol_adapt16_neon;
    }
#elif ARCH_X86_64 && HAVE_ASM
    if (dav1d_get_cpu_flags() & DAV1D_X86_CPU_FLAG_SSE2) {
        c.symbol_adapt4  = dav1d_msac_decode_symbol_adapt4_sse2;
        c.symbol_adapt8  = dav1d_msac_decode_symbol_adapt8_sse2;
        c.symbol_adapt16 = dav1d_msac_decode_symbol_adapt16_sse2;
    }
#endif

    check_decode_symbol_adapt(&c);
}
