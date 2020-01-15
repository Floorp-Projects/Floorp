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
#ifndef __Hacl_Curve25519_H
#define __Hacl_Curve25519_H

typedef uint64_t Hacl_Bignum_Constants_limb;

typedef FStar_UInt128_t Hacl_Bignum_Constants_wide;

typedef uint64_t Hacl_Bignum_Parameters_limb;

typedef FStar_UInt128_t Hacl_Bignum_Parameters_wide;

typedef uint32_t Hacl_Bignum_Parameters_ctr;

typedef uint64_t *Hacl_Bignum_Parameters_felem;

typedef FStar_UInt128_t *Hacl_Bignum_Parameters_felem_wide;

typedef void *Hacl_Bignum_Parameters_seqelem;

typedef void *Hacl_Bignum_Parameters_seqelem_wide;

typedef FStar_UInt128_t Hacl_Bignum_Wide_t;

typedef uint64_t Hacl_Bignum_Limb_t;

extern void Hacl_Bignum_lemma_diff(Prims_int x0, Prims_int x1, Prims_pos x2);

typedef uint64_t *Hacl_EC_Point_point;

typedef uint8_t *Hacl_EC_Ladder_SmallLoop_uint8_p;

typedef uint8_t *Hacl_EC_Ladder_uint8_p;

typedef uint8_t *Hacl_EC_Format_uint8_p;

void Hacl_EC_crypto_scalarmult(uint8_t *mypublic, uint8_t *secret, uint8_t *basepoint);

typedef uint8_t *Hacl_Curve25519_uint8_p;

void Hacl_Curve25519_crypto_scalarmult(uint8_t *mypublic, uint8_t *secret, uint8_t *basepoint);
#endif
