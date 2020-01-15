/* Copyright 2016-2018 INRIA and Microsoft Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "kremlib.h"
#ifndef __Hacl_Chacha20_Vec128_H
#define __Hacl_Chacha20_Vec128_H

#include "vec128.h"

typedef uint32_t Hacl_Impl_Xor_Lemmas_u32;

typedef uint8_t Hacl_Impl_Xor_Lemmas_u8;

typedef uint32_t Hacl_Impl_Chacha20_Vec128_State_u32;

typedef uint32_t Hacl_Impl_Chacha20_Vec128_State_h32;

typedef uint8_t *Hacl_Impl_Chacha20_Vec128_State_uint8_p;

typedef vec *Hacl_Impl_Chacha20_Vec128_State_state;

typedef uint32_t Hacl_Impl_Chacha20_Vec128_u32;

typedef uint32_t Hacl_Impl_Chacha20_Vec128_h32;

typedef uint8_t *Hacl_Impl_Chacha20_Vec128_uint8_p;

typedef uint32_t Hacl_Impl_Chacha20_Vec128_idx;

typedef struct
{
    void *k;
    void *n;
    uint32_t ctr;
} Hacl_Impl_Chacha20_Vec128_log_t_;

typedef void *Hacl_Impl_Chacha20_Vec128_log_t;

typedef uint8_t *Hacl_Chacha20_Vec128_uint8_p;

void
Hacl_Chacha20_Vec128_chacha20(
    uint8_t *output,
    uint8_t *plain,
    uint32_t len,
    uint8_t *k,
    uint8_t *n1,
    uint32_t ctr);
#endif
