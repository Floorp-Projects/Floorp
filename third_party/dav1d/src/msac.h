/*
 * Copyright (c) 2001-2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef __DAV1D_SRC_MSAC_H__
#define __DAV1D_SRC_MSAC_H__

#include <stdint.h>
#include <stdlib.h>

typedef struct MsacContext {
    const uint8_t *buf, *end, *bptr;
    int32_t tell_offs;
    uint32_t dif;
    uint16_t rng;
    int16_t cnt;
    int error;
} MsacContext;

void msac_init(MsacContext *c, const uint8_t *data, size_t sz);
unsigned msac_decode_symbol(MsacContext *c, const uint16_t *cdf,
                            const unsigned n_symbols);
unsigned msac_decode_bool(MsacContext *c, unsigned cdf);
unsigned msac_decode_bools(MsacContext *c, unsigned l);
int msac_decode_subexp(MsacContext *c, int ref, unsigned n, unsigned k);
int msac_decode_uniform(MsacContext *c, unsigned n);
void update_cdf(uint16_t *cdf, unsigned val, unsigned nsymbs);

static inline unsigned msac_decode_symbol_adapt(MsacContext *const c,
                                                uint16_t *const cdf,
                                                const unsigned n_symbols)
{
    const unsigned val = msac_decode_symbol(c, cdf, n_symbols);
    update_cdf(cdf, val, n_symbols);
    return val;
}

static inline unsigned msac_decode_bool_adapt(MsacContext *const c,
                                              uint16_t *const cdf)
{
    const unsigned bit = msac_decode_bool(c, *cdf);
    uint16_t bak_cdf[3] = { cdf[0], 0, cdf[1] };
    update_cdf(bak_cdf, bit, 2);
    cdf[0] = bak_cdf[0];
    cdf[1] = bak_cdf[2];
    return bit;
}

#endif /* __DAV1D_SRC_MSAC_H__ */
