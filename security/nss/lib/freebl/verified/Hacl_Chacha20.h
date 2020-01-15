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
#ifndef __Hacl_Chacha20_H
#define __Hacl_Chacha20_H

typedef uint32_t Hacl_Impl_Xor_Lemmas_u32;

typedef uint8_t Hacl_Impl_Xor_Lemmas_u8;

typedef uint8_t *Hacl_Lib_LoadStore32_uint8_p;

typedef uint32_t Hacl_Impl_Chacha20_u32;

typedef uint32_t Hacl_Impl_Chacha20_h32;

typedef uint8_t *Hacl_Impl_Chacha20_uint8_p;

typedef uint32_t *Hacl_Impl_Chacha20_state;

typedef uint32_t Hacl_Impl_Chacha20_idx;

typedef struct
{
    void *k;
    void *n;
} Hacl_Impl_Chacha20_log_t_;

typedef void *Hacl_Impl_Chacha20_log_t;

typedef uint32_t Hacl_Lib_Create_h32;

typedef uint8_t *Hacl_Chacha20_uint8_p;

typedef uint32_t Hacl_Chacha20_uint32_t;

void Hacl_Chacha20_chacha20_key_block(uint8_t *block, uint8_t *k, uint8_t *n1, uint32_t ctr);

/*
  This function implements Chacha20

  val chacha20 :
  output:uint8_p ->
  plain:uint8_p{ disjoint output plain } ->
  len:uint32_t{ v len = length output /\ v len = length plain } ->
  key:uint8_p{ length key = 32 } ->
  nonce:uint8_p{ length nonce = 12 } ->
  ctr:uint32_t{ v ctr + length plain / 64 < pow2 32 } ->
  Stack unit
    (requires
      fun h -> live h output /\ live h plain /\ live h nonce /\ live h key)
    (ensures
      fun h0 _ h1 ->
        live h1 output /\ live h0 plain /\ modifies_1 output h0 h1 /\
        live h0 nonce /\
        live h0 key /\
        h1.[ output ] ==
        chacha20_encrypt_bytes h0.[ key ] h0.[ nonce ] (v ctr) h0.[ plain ])
*/
void
Hacl_Chacha20_chacha20(
    uint8_t *output,
    uint8_t *plain,
    uint32_t len,
    uint8_t *k,
    uint8_t *n1,
    uint32_t ctr);
#endif
