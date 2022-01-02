/* MIT License
 *
 * Copyright (c) 2016-2020 INRIA, CMU and Microsoft Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __Hacl_Poly1305_32_H
#define __Hacl_Poly1305_32_H

#if defined(__cplusplus)
extern "C" {
#endif

#include "kremlin/internal/types.h"
#include "kremlin/lowstar_endianness.h"
#include <string.h>
#include <stdbool.h>

#include "Hacl_Kremlib.h"

extern uint32_t Hacl_Poly1305_32_blocklen;

typedef uint64_t *Hacl_Poly1305_32_poly1305_ctx;

void Hacl_Poly1305_32_poly1305_init(uint64_t *ctx, uint8_t *key);

void Hacl_Poly1305_32_poly1305_update1(uint64_t *ctx, uint8_t *text);

void Hacl_Poly1305_32_poly1305_update(uint64_t *ctx, uint32_t len, uint8_t *text);

void Hacl_Poly1305_32_poly1305_finish(uint8_t *tag, uint8_t *key, uint64_t *ctx);

void Hacl_Poly1305_32_poly1305_mac(uint8_t *tag, uint32_t len, uint8_t *text, uint8_t *key);

#if defined(__cplusplus)
}
#endif

#define __Hacl_Poly1305_32_H_DEFINED
#endif
