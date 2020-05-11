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

#include "kremlin/internal/types.h"
#include "kremlin/lowstar_endianness.h"
#include <string.h>
#include <stdbool.h>

#ifndef __Hacl_Chacha20_H
#define __Hacl_Chacha20_H

#include "Hacl_Kremlib.h"

extern const uint32_t Hacl_Impl_Chacha20_Vec_chacha20_constants[4U];

void
Hacl_Chacha20_chacha20_encrypt(
    uint32_t len,
    uint8_t *out,
    uint8_t *text,
    uint8_t *key,
    uint8_t *n,
    uint32_t ctr);

void
Hacl_Chacha20_chacha20_decrypt(
    uint32_t len,
    uint8_t *out,
    uint8_t *cipher,
    uint8_t *key,
    uint8_t *n,
    uint32_t ctr);

#define __Hacl_Chacha20_H_DEFINED
#endif
