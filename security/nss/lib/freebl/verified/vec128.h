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

#ifndef __Vec_H
#define __Vec_H

#ifdef __MSVC__
#define forceinline __forceinline inline
#elif (defined(__GNUC__) || defined(__clang__))
#define forceinline __attribute__((always_inline)) inline
#else
#define forceinline inline
#endif

#if defined(__SSSE3__) || defined(__AVX2__) || defined(__AVX__)

#include <emmintrin.h>
#include <tmmintrin.h>

#define VEC128
#define vec_size 4

typedef __m128i vec;

static forceinline vec
vec_rotate_left_8(vec v)
{
    __m128i x = _mm_set_epi8(14, 13, 12, 15, 10, 9, 8, 11, 6, 5, 4, 7, 2, 1, 0, 3);
    return _mm_shuffle_epi8(v, x);
}

static forceinline vec
vec_rotate_left_16(vec v)
{
    __m128i x = _mm_set_epi8(13, 12, 15, 14, 9, 8, 11, 10, 5, 4, 7, 6, 1, 0, 3, 2);
    return _mm_shuffle_epi8(v, x);
}

static forceinline vec
vec_rotate_left(vec v, unsigned int n)
{
    if (n == 8)
        return vec_rotate_left_8(v);
    if (n == 16)
        return vec_rotate_left_16(v);
    return _mm_xor_si128(_mm_slli_epi32(v, n),
                         _mm_srli_epi32(v, 32 - n));
}

static forceinline vec
vec_rotate_right(vec v, unsigned int n)
{
    return (vec_rotate_left(v, 32 - n));
}

#define vec_shuffle_right(x, n) \
    _mm_shuffle_epi32(x, _MM_SHUFFLE((3 + (n)) % 4, (2 + (n)) % 4, (1 + (n)) % 4, (n) % 4))

#define vec_shuffle_left(x, n) vec_shuffle_right((x), 4 - (n))

static forceinline vec
vec_load_32x4(uint32_t x1, uint32_t x2, uint32_t x3, uint32_t x4)
{
    return _mm_set_epi32(x4, x3, x2, x1);
}

static forceinline vec
vec_load_32x8(uint32_t x1, uint32_t x2, uint32_t x3, uint32_t x4, uint32_t x5, uint32_t x6, uint32_t x7, uint32_t x8)
{
    return _mm_set_epi32(x4, x3, x2, x1);
}

static forceinline vec
vec_load_le(const unsigned char* in)
{
    return _mm_loadu_si128((__m128i*)(in));
}

static forceinline vec
vec_load128_le(const unsigned char* in)
{
    return vec_load_le(in);
}

static forceinline void
vec_store_le(unsigned char* out, vec v)
{
    _mm_storeu_si128((__m128i*)(out), v);
}

static forceinline vec
vec_add(vec v1, vec v2)
{
    return _mm_add_epi32(v1, v2);
}

static forceinline vec
vec_add_u32(vec v1, uint32_t x)
{
    vec v2 = vec_load_32x4(x, 0, 0, 0);
    return _mm_add_epi32(v1, v2);
}

static forceinline vec
vec_increment(vec v1)
{
    vec one = vec_load_32x4(1, 0, 0, 0);
    return _mm_add_epi32(v1, one);
}

static forceinline vec
vec_xor(vec v1, vec v2)
{
    return _mm_xor_si128(v1, v2);
}

#define vec_zero() _mm_set_epi32(0, 0, 0, 0)

#elif defined(__ARM_NEON__) || defined(__ARM_NEON)
#include <arm_neon.h>

typedef uint32x4_t vec;

static forceinline vec
vec_xor(vec v1, vec v2)
{
    return veorq_u32(v1, v2);
}

#define vec_rotate_left(x, n) \
    vsriq_n_u32(vshlq_n_u32((x), (n)), (x), 32 - (n))

#define vec_rotate_right(a, b) \
    vec_rotate_left((b), 32 - (b))

#define vec_shuffle_right(x, n) \
    vextq_u32((x), (x), (n))

#define vec_shuffle_left(a, b) \
    vec_shuffle_right((a), 4 - (b))

static forceinline vec
vec_load_32x4(uint32_t x1, uint32_t x2, uint32_t x3, uint32_t x4)
{
    uint32_t a[4] = { x1, x2, x3, x4 };
    return vld1q_u32(a);
}

static forceinline vec
vec_load_32(uint32_t x1)
{
    uint32_t a[4] = { x1, x1, x1, x1 };
    return vld1q_u32(a);
}

static forceinline vec
vec_load_32x8(uint32_t x1, uint32_t x2, uint32_t x3, uint32_t x4, uint32_t x5, uint32_t x6, uint32_t x7, uint32_t x8)
{
    return vec_load_32x4(x1, x2, x3, x4);
}

static forceinline vec
vec_load_le(const unsigned char* in)
{
    return vld1q_u32((uint32_t*)in);
}

static forceinline vec
vec_load128_le(const unsigned char* in)
{
    return vec_load_le(in);
}

static forceinline void
vec_store_le(unsigned char* out, vec v)
{
    vst1q_u32((uint32_t*)out, v);
}

static forceinline vec
vec_add(vec v1, vec v2)
{
    return vaddq_u32(v1, v2);
}

static forceinline vec
vec_add_u32(vec v1, uint32_t x)
{
    vec v2 = vec_load_32x4(x, 0, 0, 0);
    return vec_add(v1, v2);
}

static forceinline vec
vec_increment(vec v1)
{
    vec one = vec_load_32x4(1, 0, 0, 0);
    return vec_add(v1, one);
}

#define vec_zero() vec_load_32x4(0, 0, 0, 0)

#else

#define VEC128
#define vec_size 4

typedef struct {
    uint32_t v[4];
} vec;

static forceinline vec
vec_xor(vec v1, vec v2)
{
    vec r;
    r.v[0] = v1.v[0] ^ v2.v[0];
    r.v[1] = v1.v[1] ^ v2.v[1];
    r.v[2] = v1.v[2] ^ v2.v[2];
    r.v[3] = v1.v[3] ^ v2.v[3];
    return r;
}

static forceinline vec
vec_rotate_left(vec v, unsigned int n)
{
    vec r;
    r.v[0] = (v.v[0] << n) ^ (v.v[0] >> (32 - n));
    r.v[1] = (v.v[1] << n) ^ (v.v[1] >> (32 - n));
    r.v[2] = (v.v[2] << n) ^ (v.v[2] >> (32 - n));
    r.v[3] = (v.v[3] << n) ^ (v.v[3] >> (32 - n));
    return r;
}

static forceinline vec
vec_rotate_right(vec v, unsigned int n)
{
    return (vec_rotate_left(v, 32 - n));
}

static forceinline vec
vec_shuffle_right(vec v, unsigned int n)
{
    vec r;
    r.v[0] = v.v[n % 4];
    r.v[1] = v.v[(n + 1) % 4];
    r.v[2] = v.v[(n + 2) % 4];
    r.v[3] = v.v[(n + 3) % 4];
    return r;
}

static forceinline vec
vec_shuffle_left(vec x, unsigned int n)
{
    return vec_shuffle_right(x, 4 - n);
}

static forceinline vec
vec_load_32x4(uint32_t x0, uint32_t x1, uint32_t x2, uint32_t x3)
{
    vec v;
    v.v[0] = x0;
    v.v[1] = x1;
    v.v[2] = x2;
    v.v[3] = x3;
    return v;
}

static forceinline vec
vec_load_32(uint32_t x0)
{
    vec v;
    v.v[0] = x0;
    v.v[1] = x0;
    v.v[2] = x0;
    v.v[3] = x0;
    return v;
}

static forceinline vec
vec_load_le(const uint8_t* in)
{
    vec r;
    r.v[0] = load32_le((uint8_t*)in);
    r.v[1] = load32_le((uint8_t*)in + 4);
    r.v[2] = load32_le((uint8_t*)in + 8);
    r.v[3] = load32_le((uint8_t*)in + 12);
    return r;
}

static forceinline void
vec_store_le(unsigned char* out, vec r)
{
    store32_le(out, r.v[0]);
    store32_le(out + 4, r.v[1]);
    store32_le(out + 8, r.v[2]);
    store32_le(out + 12, r.v[3]);
}

static forceinline vec
vec_load128_le(const unsigned char* in)
{
    return vec_load_le(in);
}

static forceinline vec
vec_add(vec v1, vec v2)
{
    vec r;
    r.v[0] = v1.v[0] + v2.v[0];
    r.v[1] = v1.v[1] + v2.v[1];
    r.v[2] = v1.v[2] + v2.v[2];
    r.v[3] = v1.v[3] + v2.v[3];
    return r;
}

static forceinline vec
vec_add_u32(vec v1, uint32_t x)
{
    vec v2 = vec_load_32x4(x, 0, 0, 0);
    return vec_add(v1, v2);
}

static forceinline vec
vec_increment(vec v1)
{
    vec one = vec_load_32x4(1, 0, 0, 0);
    return vec_add(v1, one);
}

#define vec_zero() vec_load_32x4(0, 0, 0, 0)

#endif

#endif
