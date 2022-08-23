// Copyright 2021 Google LLC
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// RISC-V V vectors (length not known at compile time).
// External include guard in highway.h - see comment there.

#include <riscv_vector.h>
#include <stddef.h>
#include <stdint.h>

#include "hwy/base.h"
#include "hwy/ops/shared-inl.h"

HWY_BEFORE_NAMESPACE();
namespace hwy {
namespace HWY_NAMESPACE {

template <class V>
struct DFromV_t {};  // specialized in macros
template <class V>
using DFromV = typename DFromV_t<RemoveConst<V>>::type;

template <class V>
using TFromV = TFromD<DFromV<V>>;

// Enables the overload if Pow2 is in [min, max].
#define HWY_RVV_IF_POW2_IN(D, min, max) \
  hwy::EnableIf<(min) <= Pow2(D()) && Pow2(D()) <= (max)>* = nullptr

template <typename T, size_t N, int kPow2>
constexpr size_t MLenFromD(Simd<T, N, kPow2> /* tag */) {
  // Returns divisor = type bits / LMUL. Folding *8 into the ScaleByPower
  // argument enables fractional LMUL < 1. Limit to 64 because that is the
  // largest value for which vbool##_t are defined.
  return HWY_MIN(64, sizeof(T) * 8 * 8 / detail::ScaleByPower(8, kPow2));
}

// ================================================== MACROS

// Generate specializations and function definitions using X macros. Although
// harder to read and debug, writing everything manually is too bulky.

namespace detail {  // for code folding

// For all mask sizes MLEN: (1/Nth of a register, one bit per lane)
// The first two arguments are SEW and SHIFT such that SEW >> SHIFT = MLEN.
#define HWY_RVV_FOREACH_B(X_MACRO, NAME, OP) \
  X_MACRO(64, 0, 64, NAME, OP)               \
  X_MACRO(32, 0, 32, NAME, OP)               \
  X_MACRO(16, 0, 16, NAME, OP)               \
  X_MACRO(8, 0, 8, NAME, OP)                 \
  X_MACRO(8, 1, 4, NAME, OP)                 \
  X_MACRO(8, 2, 2, NAME, OP)                 \
  X_MACRO(8, 3, 1, NAME, OP)

// For given SEW, iterate over one of LMULS: _TRUNC, _EXT, _ALL. This allows
// reusing type lists such as HWY_RVV_FOREACH_U for _ALL (the usual case) or
// _EXT (for Combine). To achieve this, we HWY_CONCAT with the LMULS suffix.
//
// Precompute SEW/LMUL => MLEN to allow token-pasting the result. For the same
// reason, also pass the double-width and half SEW and LMUL (suffixed D and H,
// respectively). "__" means there is no corresponding LMUL (e.g. LMULD for m8).
// Args: BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT, MLEN, NAME, OP

// LMULS = _TRUNC: truncatable (not the smallest LMUL)
#define HWY_RVV_FOREACH_08_TRUNC(X_MACRO, BASE, CHAR, NAME, OP)            \
  X_MACRO(BASE, CHAR, 8, 16, __, mf4, mf2, mf8, -2, /*MLEN=*/32, NAME, OP) \
  X_MACRO(BASE, CHAR, 8, 16, __, mf2, m1, mf4, -1, /*MLEN=*/16, NAME, OP)  \
  X_MACRO(BASE, CHAR, 8, 16, __, m1, m2, mf2, 0, /*MLEN=*/8, NAME, OP)     \
  X_MACRO(BASE, CHAR, 8, 16, __, m2, m4, m1, 1, /*MLEN=*/4, NAME, OP)      \
  X_MACRO(BASE, CHAR, 8, 16, __, m4, m8, m2, 2, /*MLEN=*/2, NAME, OP)      \
  X_MACRO(BASE, CHAR, 8, 16, __, m8, __, m4, 3, /*MLEN=*/1, NAME, OP)

#define HWY_RVV_FOREACH_16_TRUNC(X_MACRO, BASE, CHAR, NAME, OP)           \
  X_MACRO(BASE, CHAR, 16, 32, 8, mf2, m1, mf4, -1, /*MLEN=*/32, NAME, OP) \
  X_MACRO(BASE, CHAR, 16, 32, 8, m1, m2, mf2, 0, /*MLEN=*/16, NAME, OP)   \
  X_MACRO(BASE, CHAR, 16, 32, 8, m2, m4, m1, 1, /*MLEN=*/8, NAME, OP)     \
  X_MACRO(BASE, CHAR, 16, 32, 8, m4, m8, m2, 2, /*MLEN=*/4, NAME, OP)     \
  X_MACRO(BASE, CHAR, 16, 32, 8, m8, __, m4, 3, /*MLEN=*/2, NAME, OP)

#define HWY_RVV_FOREACH_32_TRUNC(X_MACRO, BASE, CHAR, NAME, OP)          \
  X_MACRO(BASE, CHAR, 32, 64, 16, m1, m2, mf2, 0, /*MLEN=*/32, NAME, OP) \
  X_MACRO(BASE, CHAR, 32, 64, 16, m2, m4, m1, 1, /*MLEN=*/16, NAME, OP)  \
  X_MACRO(BASE, CHAR, 32, 64, 16, m4, m8, m2, 2, /*MLEN=*/8, NAME, OP)   \
  X_MACRO(BASE, CHAR, 32, 64, 16, m8, __, m4, 3, /*MLEN=*/4, NAME, OP)

#define HWY_RVV_FOREACH_64_TRUNC(X_MACRO, BASE, CHAR, NAME, OP)         \
  X_MACRO(BASE, CHAR, 64, __, 32, m2, m4, m1, 1, /*MLEN=*/32, NAME, OP) \
  X_MACRO(BASE, CHAR, 64, __, 32, m4, m8, m2, 2, /*MLEN=*/16, NAME, OP) \
  X_MACRO(BASE, CHAR, 64, __, 32, m8, __, m4, 3, /*MLEN=*/8, NAME, OP)

// LMULS = _DEMOTE: can demote from SEW*LMUL to SEWH*LMULH.
#define HWY_RVV_FOREACH_08_DEMOTE(X_MACRO, BASE, CHAR, NAME, OP)           \
  X_MACRO(BASE, CHAR, 8, 16, __, mf4, mf2, mf8, -2, /*MLEN=*/32, NAME, OP) \
  X_MACRO(BASE, CHAR, 8, 16, __, mf2, m1, mf4, -1, /*MLEN=*/16, NAME, OP)  \
  X_MACRO(BASE, CHAR, 8, 16, __, m1, m2, mf2, 0, /*MLEN=*/8, NAME, OP)     \
  X_MACRO(BASE, CHAR, 8, 16, __, m2, m4, m1, 1, /*MLEN=*/4, NAME, OP)      \
  X_MACRO(BASE, CHAR, 8, 16, __, m4, m8, m2, 2, /*MLEN=*/2, NAME, OP)      \
  X_MACRO(BASE, CHAR, 8, 16, __, m8, __, m4, 3, /*MLEN=*/1, NAME, OP)

#define HWY_RVV_FOREACH_16_DEMOTE(X_MACRO, BASE, CHAR, NAME, OP)           \
  X_MACRO(BASE, CHAR, 16, 32, 8, mf4, mf2, mf8, -2, /*MLEN=*/64, NAME, OP) \
  X_MACRO(BASE, CHAR, 16, 32, 8, mf2, m1, mf4, -1, /*MLEN=*/32, NAME, OP)  \
  X_MACRO(BASE, CHAR, 16, 32, 8, m1, m2, mf2, 0, /*MLEN=*/16, NAME, OP)    \
  X_MACRO(BASE, CHAR, 16, 32, 8, m2, m4, m1, 1, /*MLEN=*/8, NAME, OP)      \
  X_MACRO(BASE, CHAR, 16, 32, 8, m4, m8, m2, 2, /*MLEN=*/4, NAME, OP)      \
  X_MACRO(BASE, CHAR, 16, 32, 8, m8, __, m4, 3, /*MLEN=*/2, NAME, OP)

#define HWY_RVV_FOREACH_32_DEMOTE(X_MACRO, BASE, CHAR, NAME, OP)           \
  X_MACRO(BASE, CHAR, 32, 64, 16, mf2, m1, mf4, -1, /*MLEN=*/64, NAME, OP) \
  X_MACRO(BASE, CHAR, 32, 64, 16, m1, m2, mf2, 0, /*MLEN=*/32, NAME, OP)   \
  X_MACRO(BASE, CHAR, 32, 64, 16, m2, m4, m1, 1, /*MLEN=*/16, NAME, OP)    \
  X_MACRO(BASE, CHAR, 32, 64, 16, m4, m8, m2, 2, /*MLEN=*/8, NAME, OP)     \
  X_MACRO(BASE, CHAR, 32, 64, 16, m8, __, m4, 3, /*MLEN=*/4, NAME, OP)

#define HWY_RVV_FOREACH_64_DEMOTE(X_MACRO, BASE, CHAR, NAME, OP)         \
  X_MACRO(BASE, CHAR, 64, __, 32, m1, m2, mf2, 0, /*MLEN=*/64, NAME, OP) \
  X_MACRO(BASE, CHAR, 64, __, 32, m2, m4, m1, 1, /*MLEN=*/32, NAME, OP)  \
  X_MACRO(BASE, CHAR, 64, __, 32, m4, m8, m2, 2, /*MLEN=*/16, NAME, OP)  \
  X_MACRO(BASE, CHAR, 64, __, 32, m8, __, m4, 3, /*MLEN=*/8, NAME, OP)

// LMULS = _LE2: <= 2
#define HWY_RVV_FOREACH_08_LE2(X_MACRO, BASE, CHAR, NAME, OP)              \
  X_MACRO(BASE, CHAR, 8, 16, __, mf8, mf4, __, -3, /*MLEN=*/64, NAME, OP)  \
  X_MACRO(BASE, CHAR, 8, 16, __, mf4, mf2, mf8, -2, /*MLEN=*/32, NAME, OP) \
  X_MACRO(BASE, CHAR, 8, 16, __, mf2, m1, mf4, -1, /*MLEN=*/16, NAME, OP)  \
  X_MACRO(BASE, CHAR, 8, 16, __, m1, m2, mf2, 0, /*MLEN=*/8, NAME, OP)     \
  X_MACRO(BASE, CHAR, 8, 16, __, m2, m4, m1, 1, /*MLEN=*/4, NAME, OP)

#define HWY_RVV_FOREACH_16_LE2(X_MACRO, BASE, CHAR, NAME, OP)              \
  X_MACRO(BASE, CHAR, 16, 32, 8, mf4, mf2, mf8, -2, /*MLEN=*/64, NAME, OP) \
  X_MACRO(BASE, CHAR, 16, 32, 8, mf2, m1, mf4, -1, /*MLEN=*/32, NAME, OP)  \
  X_MACRO(BASE, CHAR, 16, 32, 8, m1, m2, mf2, 0, /*MLEN=*/16, NAME, OP)    \
  X_MACRO(BASE, CHAR, 16, 32, 8, m2, m4, m1, 1, /*MLEN=*/8, NAME, OP)

#define HWY_RVV_FOREACH_32_LE2(X_MACRO, BASE, CHAR, NAME, OP)              \
  X_MACRO(BASE, CHAR, 32, 64, 16, mf2, m1, mf4, -1, /*MLEN=*/64, NAME, OP) \
  X_MACRO(BASE, CHAR, 32, 64, 16, m1, m2, mf2, 0, /*MLEN=*/32, NAME, OP)   \
  X_MACRO(BASE, CHAR, 32, 64, 16, m2, m4, m1, 1, /*MLEN=*/16, NAME, OP)

#define HWY_RVV_FOREACH_64_LE2(X_MACRO, BASE, CHAR, NAME, OP)            \
  X_MACRO(BASE, CHAR, 64, __, 32, m1, m2, mf2, 0, /*MLEN=*/64, NAME, OP) \
  X_MACRO(BASE, CHAR, 64, __, 32, m2, m4, m1, 1, /*MLEN=*/32, NAME, OP)

// LMULS = _EXT: not the largest LMUL
#define HWY_RVV_FOREACH_08_EXT(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_08_LE2(X_MACRO, BASE, CHAR, NAME, OP)       \
  X_MACRO(BASE, CHAR, 8, 16, __, m4, m8, m2, 2, /*MLEN=*/2, NAME, OP)

#define HWY_RVV_FOREACH_16_EXT(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_16_LE2(X_MACRO, BASE, CHAR, NAME, OP)       \
  X_MACRO(BASE, CHAR, 16, 32, 8, m4, m8, m2, 2, /*MLEN=*/4, NAME, OP)

#define HWY_RVV_FOREACH_32_EXT(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_32_LE2(X_MACRO, BASE, CHAR, NAME, OP)       \
  X_MACRO(BASE, CHAR, 32, 64, 16, m4, m8, m2, 2, /*MLEN=*/8, NAME, OP)

#define HWY_RVV_FOREACH_64_EXT(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_64_LE2(X_MACRO, BASE, CHAR, NAME, OP)       \
  X_MACRO(BASE, CHAR, 64, __, 32, m4, m8, m2, 2, /*MLEN=*/16, NAME, OP)

// LMULS = _ALL (2^MinPow2() <= LMUL <= 8)
#define HWY_RVV_FOREACH_08_ALL(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_08_EXT(X_MACRO, BASE, CHAR, NAME, OP)       \
  X_MACRO(BASE, CHAR, 8, 16, __, m8, __, m4, 3, /*MLEN=*/1, NAME, OP)

#define HWY_RVV_FOREACH_16_ALL(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_16_EXT(X_MACRO, BASE, CHAR, NAME, OP)       \
  X_MACRO(BASE, CHAR, 16, 32, 8, m8, __, m4, 3, /*MLEN=*/2, NAME, OP)

#define HWY_RVV_FOREACH_32_ALL(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_32_EXT(X_MACRO, BASE, CHAR, NAME, OP)       \
  X_MACRO(BASE, CHAR, 32, 64, 16, m8, __, m4, 3, /*MLEN=*/4, NAME, OP)

#define HWY_RVV_FOREACH_64_ALL(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_64_EXT(X_MACRO, BASE, CHAR, NAME, OP)       \
  X_MACRO(BASE, CHAR, 64, __, 32, m8, __, m4, 3, /*MLEN=*/8, NAME, OP)

// 'Virtual' LMUL. This upholds the Highway guarantee that vectors are at least
// 128 bit and LowerHalf is defined whenever there are at least 2 lanes, even
// though RISC-V LMUL must be at least SEW/64 (notice that this rules out
// LMUL=1/2 for SEW=64). To bridge the gap, we add overloads for kPow2 equal to
// one less than should be supported, with all other parameters (vector type
// etc.) unchanged. For D with the lowest kPow2 ('virtual LMUL'), Lanes()
// returns half of what it usually would.
//
// Notice that we can only add overloads whenever there is a D argument: those
// are unique with respect to non-virtual-LMUL overloads because their kPow2
// template argument differs. Otherwise, there is no actual vuint64mf2_t, and
// defining another overload with the same LMUL would be an error. Thus we have
// a separate _VIRT category for HWY_RVV_FOREACH*, and the common case is
// _ALL_VIRT (meaning the regular LMUL plus the VIRT overloads), used in most
// functions that take a D.

#define HWY_RVV_FOREACH_08_VIRT(X_MACRO, BASE, CHAR, NAME, OP)

#define HWY_RVV_FOREACH_16_VIRT(X_MACRO, BASE, CHAR, NAME, OP) \
  X_MACRO(BASE, CHAR, 16, 32, 8, mf4, mf2, mf8, -3, /*MLEN=*/64, NAME, OP)

#define HWY_RVV_FOREACH_32_VIRT(X_MACRO, BASE, CHAR, NAME, OP) \
  X_MACRO(BASE, CHAR, 32, 64, 16, mf2, m1, mf4, -2, /*MLEN=*/64, NAME, OP)

#define HWY_RVV_FOREACH_64_VIRT(X_MACRO, BASE, CHAR, NAME, OP) \
  X_MACRO(BASE, CHAR, 64, __, 32, m1, m2, mf2, -1, /*MLEN=*/64, NAME, OP)

// ALL + VIRT
#define HWY_RVV_FOREACH_08_ALL_VIRT(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_08_ALL(X_MACRO, BASE, CHAR, NAME, OP)            \
  HWY_RVV_FOREACH_08_VIRT(X_MACRO, BASE, CHAR, NAME, OP)

#define HWY_RVV_FOREACH_16_ALL_VIRT(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_16_ALL(X_MACRO, BASE, CHAR, NAME, OP)            \
  HWY_RVV_FOREACH_16_VIRT(X_MACRO, BASE, CHAR, NAME, OP)

#define HWY_RVV_FOREACH_32_ALL_VIRT(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_32_ALL(X_MACRO, BASE, CHAR, NAME, OP)            \
  HWY_RVV_FOREACH_32_VIRT(X_MACRO, BASE, CHAR, NAME, OP)

#define HWY_RVV_FOREACH_64_ALL_VIRT(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_64_ALL(X_MACRO, BASE, CHAR, NAME, OP)            \
  HWY_RVV_FOREACH_64_VIRT(X_MACRO, BASE, CHAR, NAME, OP)

// LE2 + VIRT
#define HWY_RVV_FOREACH_08_LE2_VIRT(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_08_LE2(X_MACRO, BASE, CHAR, NAME, OP)            \
  HWY_RVV_FOREACH_08_VIRT(X_MACRO, BASE, CHAR, NAME, OP)

#define HWY_RVV_FOREACH_16_LE2_VIRT(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_16_LE2(X_MACRO, BASE, CHAR, NAME, OP)            \
  HWY_RVV_FOREACH_16_VIRT(X_MACRO, BASE, CHAR, NAME, OP)

#define HWY_RVV_FOREACH_32_LE2_VIRT(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_32_LE2(X_MACRO, BASE, CHAR, NAME, OP)            \
  HWY_RVV_FOREACH_32_VIRT(X_MACRO, BASE, CHAR, NAME, OP)

#define HWY_RVV_FOREACH_64_LE2_VIRT(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_64_LE2(X_MACRO, BASE, CHAR, NAME, OP)            \
  HWY_RVV_FOREACH_64_VIRT(X_MACRO, BASE, CHAR, NAME, OP)

// EXT + VIRT
#define HWY_RVV_FOREACH_08_EXT_VIRT(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_08_EXT(X_MACRO, BASE, CHAR, NAME, OP)            \
  HWY_RVV_FOREACH_08_VIRT(X_MACRO, BASE, CHAR, NAME, OP)

#define HWY_RVV_FOREACH_16_EXT_VIRT(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_16_EXT(X_MACRO, BASE, CHAR, NAME, OP)            \
  HWY_RVV_FOREACH_16_VIRT(X_MACRO, BASE, CHAR, NAME, OP)

#define HWY_RVV_FOREACH_32_EXT_VIRT(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_32_EXT(X_MACRO, BASE, CHAR, NAME, OP)            \
  HWY_RVV_FOREACH_32_VIRT(X_MACRO, BASE, CHAR, NAME, OP)

#define HWY_RVV_FOREACH_64_EXT_VIRT(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_64_EXT(X_MACRO, BASE, CHAR, NAME, OP)            \
  HWY_RVV_FOREACH_64_VIRT(X_MACRO, BASE, CHAR, NAME, OP)

// DEMOTE + VIRT
#define HWY_RVV_FOREACH_08_DEMOTE_VIRT(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_08_DEMOTE(X_MACRO, BASE, CHAR, NAME, OP)            \
  HWY_RVV_FOREACH_08_VIRT(X_MACRO, BASE, CHAR, NAME, OP)

#define HWY_RVV_FOREACH_16_DEMOTE_VIRT(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_16_DEMOTE(X_MACRO, BASE, CHAR, NAME, OP)            \
  HWY_RVV_FOREACH_16_VIRT(X_MACRO, BASE, CHAR, NAME, OP)

#define HWY_RVV_FOREACH_32_DEMOTE_VIRT(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_32_DEMOTE(X_MACRO, BASE, CHAR, NAME, OP)            \
  HWY_RVV_FOREACH_32_VIRT(X_MACRO, BASE, CHAR, NAME, OP)

#define HWY_RVV_FOREACH_64_DEMOTE_VIRT(X_MACRO, BASE, CHAR, NAME, OP) \
  HWY_RVV_FOREACH_64_DEMOTE(X_MACRO, BASE, CHAR, NAME, OP)            \
  HWY_RVV_FOREACH_64_VIRT(X_MACRO, BASE, CHAR, NAME, OP)

// SEW for unsigned:
#define HWY_RVV_FOREACH_U08(X_MACRO, NAME, OP, LMULS) \
  HWY_CONCAT(HWY_RVV_FOREACH_08, LMULS)(X_MACRO, uint, u, NAME, OP)
#define HWY_RVV_FOREACH_U16(X_MACRO, NAME, OP, LMULS) \
  HWY_CONCAT(HWY_RVV_FOREACH_16, LMULS)(X_MACRO, uint, u, NAME, OP)
#define HWY_RVV_FOREACH_U32(X_MACRO, NAME, OP, LMULS) \
  HWY_CONCAT(HWY_RVV_FOREACH_32, LMULS)(X_MACRO, uint, u, NAME, OP)
#define HWY_RVV_FOREACH_U64(X_MACRO, NAME, OP, LMULS) \
  HWY_CONCAT(HWY_RVV_FOREACH_64, LMULS)(X_MACRO, uint, u, NAME, OP)

// SEW for signed:
#define HWY_RVV_FOREACH_I08(X_MACRO, NAME, OP, LMULS) \
  HWY_CONCAT(HWY_RVV_FOREACH_08, LMULS)(X_MACRO, int, i, NAME, OP)
#define HWY_RVV_FOREACH_I16(X_MACRO, NAME, OP, LMULS) \
  HWY_CONCAT(HWY_RVV_FOREACH_16, LMULS)(X_MACRO, int, i, NAME, OP)
#define HWY_RVV_FOREACH_I32(X_MACRO, NAME, OP, LMULS) \
  HWY_CONCAT(HWY_RVV_FOREACH_32, LMULS)(X_MACRO, int, i, NAME, OP)
#define HWY_RVV_FOREACH_I64(X_MACRO, NAME, OP, LMULS) \
  HWY_CONCAT(HWY_RVV_FOREACH_64, LMULS)(X_MACRO, int, i, NAME, OP)

// SEW for float:
#if HWY_HAVE_FLOAT16
#define HWY_RVV_FOREACH_F16(X_MACRO, NAME, OP, LMULS) \
  HWY_CONCAT(HWY_RVV_FOREACH_16, LMULS)(X_MACRO, float, f, NAME, OP)
#else
#define HWY_RVV_FOREACH_F16(X_MACRO, NAME, OP, LMULS)
#endif
#define HWY_RVV_FOREACH_F32(X_MACRO, NAME, OP, LMULS) \
  HWY_CONCAT(HWY_RVV_FOREACH_32, LMULS)(X_MACRO, float, f, NAME, OP)
#define HWY_RVV_FOREACH_F64(X_MACRO, NAME, OP, LMULS) \
  HWY_CONCAT(HWY_RVV_FOREACH_64, LMULS)(X_MACRO, float, f, NAME, OP)

// Commonly used type/SEW groups:
#define HWY_RVV_FOREACH_UI08(X_MACRO, NAME, OP, LMULS) \
  HWY_RVV_FOREACH_U08(X_MACRO, NAME, OP, LMULS)        \
  HWY_RVV_FOREACH_I08(X_MACRO, NAME, OP, LMULS)

#define HWY_RVV_FOREACH_UI16(X_MACRO, NAME, OP, LMULS) \
  HWY_RVV_FOREACH_U16(X_MACRO, NAME, OP, LMULS)        \
  HWY_RVV_FOREACH_I16(X_MACRO, NAME, OP, LMULS)

#define HWY_RVV_FOREACH_UI32(X_MACRO, NAME, OP, LMULS) \
  HWY_RVV_FOREACH_U32(X_MACRO, NAME, OP, LMULS)        \
  HWY_RVV_FOREACH_I32(X_MACRO, NAME, OP, LMULS)

#define HWY_RVV_FOREACH_UI64(X_MACRO, NAME, OP, LMULS) \
  HWY_RVV_FOREACH_U64(X_MACRO, NAME, OP, LMULS)        \
  HWY_RVV_FOREACH_I64(X_MACRO, NAME, OP, LMULS)

#define HWY_RVV_FOREACH_UI3264(X_MACRO, NAME, OP, LMULS) \
  HWY_RVV_FOREACH_UI32(X_MACRO, NAME, OP, LMULS)         \
  HWY_RVV_FOREACH_UI64(X_MACRO, NAME, OP, LMULS)

#define HWY_RVV_FOREACH_U163264(X_MACRO, NAME, OP, LMULS) \
  HWY_RVV_FOREACH_U16(X_MACRO, NAME, OP, LMULS)           \
  HWY_RVV_FOREACH_U32(X_MACRO, NAME, OP, LMULS)           \
  HWY_RVV_FOREACH_U64(X_MACRO, NAME, OP, LMULS)

#define HWY_RVV_FOREACH_I163264(X_MACRO, NAME, OP, LMULS) \
  HWY_RVV_FOREACH_I16(X_MACRO, NAME, OP, LMULS)           \
  HWY_RVV_FOREACH_I32(X_MACRO, NAME, OP, LMULS)           \
  HWY_RVV_FOREACH_I64(X_MACRO, NAME, OP, LMULS)

#define HWY_RVV_FOREACH_UI163264(X_MACRO, NAME, OP, LMULS) \
  HWY_RVV_FOREACH_U163264(X_MACRO, NAME, OP, LMULS)        \
  HWY_RVV_FOREACH_I163264(X_MACRO, NAME, OP, LMULS)

#define HWY_RVV_FOREACH_F3264(X_MACRO, NAME, OP, LMULS) \
  HWY_RVV_FOREACH_F32(X_MACRO, NAME, OP, LMULS)         \
  HWY_RVV_FOREACH_F64(X_MACRO, NAME, OP, LMULS)

// For all combinations of SEW:
#define HWY_RVV_FOREACH_U(X_MACRO, NAME, OP, LMULS) \
  HWY_RVV_FOREACH_U08(X_MACRO, NAME, OP, LMULS)     \
  HWY_RVV_FOREACH_U16(X_MACRO, NAME, OP, LMULS)     \
  HWY_RVV_FOREACH_U32(X_MACRO, NAME, OP, LMULS)     \
  HWY_RVV_FOREACH_U64(X_MACRO, NAME, OP, LMULS)

#define HWY_RVV_FOREACH_I(X_MACRO, NAME, OP, LMULS) \
  HWY_RVV_FOREACH_I08(X_MACRO, NAME, OP, LMULS)     \
  HWY_RVV_FOREACH_I16(X_MACRO, NAME, OP, LMULS)     \
  HWY_RVV_FOREACH_I32(X_MACRO, NAME, OP, LMULS)     \
  HWY_RVV_FOREACH_I64(X_MACRO, NAME, OP, LMULS)

#define HWY_RVV_FOREACH_F(X_MACRO, NAME, OP, LMULS) \
  HWY_RVV_FOREACH_F16(X_MACRO, NAME, OP, LMULS)     \
  HWY_RVV_FOREACH_F3264(X_MACRO, NAME, OP, LMULS)

// Commonly used type categories:
#define HWY_RVV_FOREACH_UI(X_MACRO, NAME, OP, LMULS) \
  HWY_RVV_FOREACH_U(X_MACRO, NAME, OP, LMULS)        \
  HWY_RVV_FOREACH_I(X_MACRO, NAME, OP, LMULS)

#define HWY_RVV_FOREACH(X_MACRO, NAME, OP, LMULS) \
  HWY_RVV_FOREACH_U(X_MACRO, NAME, OP, LMULS)     \
  HWY_RVV_FOREACH_I(X_MACRO, NAME, OP, LMULS)     \
  HWY_RVV_FOREACH_F(X_MACRO, NAME, OP, LMULS)

// Assemble types for use in x-macros
#define HWY_RVV_T(BASE, SEW) BASE##SEW##_t
#define HWY_RVV_D(BASE, SEW, N, SHIFT) Simd<HWY_RVV_T(BASE, SEW), N, SHIFT>
#define HWY_RVV_V(BASE, SEW, LMUL) v##BASE##SEW##LMUL##_t
#define HWY_RVV_M(MLEN) vbool##MLEN##_t

}  // namespace detail

// Until we have full intrinsic support for fractional LMUL, mixed-precision
// code can use LMUL 1..8 (adequate unless they need many registers).
#define HWY_SPECIALIZE(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT, \
                       MLEN, NAME, OP)                                         \
  template <>                                                                  \
  struct DFromV_t<HWY_RVV_V(BASE, SEW, LMUL)> {                                \
    using Lane = HWY_RVV_T(BASE, SEW);                                         \
    using type = ScalableTag<Lane, SHIFT>;                                     \
  };

HWY_RVV_FOREACH(HWY_SPECIALIZE, _, _, _ALL)
#undef HWY_SPECIALIZE

// ------------------------------ Lanes

// WARNING: we want to query VLMAX/sizeof(T), but this actually changes VL!
// vlenb is not exposed through intrinsics and vreadvl is not VLMAX.
#define HWY_RVV_LANES(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT, \
                      MLEN, NAME, OP)                                         \
  template <size_t N>                                                         \
  HWY_API size_t NAME(HWY_RVV_D(BASE, SEW, N, SHIFT) d) {                     \
    size_t actual = v##OP##SEW##LMUL();                                       \
    /* Common case of full vectors: avoid any extra instructions. */          \
    /* actual includes LMUL, so do not shift again. */                        \
    if (detail::IsFull(d)) return actual;                                     \
    /* Check for virtual LMUL, e.g. "uint16mf8_t" (not provided by */         \
    /* intrinsics). In this case the actual LMUL is 1/4, so divide by */      \
    /* another factor of two. */                                              \
    if (detail::ScaleByPower(128 / SEW, SHIFT) == 1) actual >>= 1;            \
    return HWY_MIN(actual, N);                                                \
  }

HWY_RVV_FOREACH(HWY_RVV_LANES, Lanes, setvlmax_e, _ALL_VIRT)
#undef HWY_RVV_LANES

template <size_t N, int kPow2>
HWY_API size_t Lanes(Simd<bfloat16_t, N, kPow2> /* tag*/) {
  return Lanes(Simd<uint16_t, N, kPow2>());
}

// ------------------------------ Common x-macros

// Last argument to most intrinsics. Use when the op has no d arg of its own,
// which means there is no user-specified cap.
#define HWY_RVV_AVL(SEW, SHIFT) \
  Lanes(ScalableTag<HWY_RVV_T(uint, SEW), SHIFT>())

// vector = f(vector), e.g. Not
#define HWY_RVV_RETV_ARGV(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, \
                          SHIFT, MLEN, NAME, OP)                           \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) NAME(HWY_RVV_V(BASE, SEW, LMUL) v) {  \
    return v##OP##_v_##CHAR##SEW##LMUL(v, HWY_RVV_AVL(SEW, SHIFT));        \
  }

// vector = f(vector, scalar), e.g. detail::AddS
#define HWY_RVV_RETV_ARGVS(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, \
                           SHIFT, MLEN, NAME, OP)                           \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                        \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) a, HWY_RVV_T(BASE, SEW) b) {          \
    return v##OP##_##CHAR##SEW##LMUL(a, b, HWY_RVV_AVL(SEW, SHIFT));        \
  }

// vector = f(vector, vector), e.g. Add
#define HWY_RVV_RETV_ARGVV(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, \
                           SHIFT, MLEN, NAME, OP)                           \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                        \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) a, HWY_RVV_V(BASE, SEW, LMUL) b) {    \
    return v##OP##_vv_##CHAR##SEW##LMUL(a, b, HWY_RVV_AVL(SEW, SHIFT));     \
  }

// mask = f(mask)
#define HWY_RVV_RETM_ARGM(SEW, SHIFT, MLEN, NAME, OP) \
  HWY_API HWY_RVV_M(MLEN) NAME(HWY_RVV_M(MLEN) m) {   \
    return vm##OP##_m_b##MLEN(m, ~0ull);              \
  }

// ================================================== INIT

// ------------------------------ Set

#define HWY_RVV_SET(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT, \
                    MLEN, NAME, OP)                                         \
  template <size_t N>                                                       \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                        \
      NAME(HWY_RVV_D(BASE, SEW, N, SHIFT) d, HWY_RVV_T(BASE, SEW) arg) {    \
    return v##OP##_##CHAR##SEW##LMUL(arg, Lanes(d));                        \
  }

HWY_RVV_FOREACH_UI(HWY_RVV_SET, Set, mv_v_x, _ALL_VIRT)
HWY_RVV_FOREACH_F(HWY_RVV_SET, Set, fmv_v_f, _ALL_VIRT)
#undef HWY_RVV_SET

// Treat bfloat16_t as uint16_t (using the previously defined Set overloads);
// required for Zero and VFromD.
template <size_t N, int kPow2>
decltype(Set(Simd<uint16_t, N, kPow2>(), 0)) Set(Simd<bfloat16_t, N, kPow2> d,
                                                 bfloat16_t arg) {
  return Set(RebindToUnsigned<decltype(d)>(), arg.bits);
}

template <class D>
using VFromD = decltype(Set(D(), TFromD<D>()));

// ------------------------------ Zero

template <typename T, size_t N, int kPow2>
HWY_API VFromD<Simd<T, N, kPow2>> Zero(Simd<T, N, kPow2> d) {
  return Set(d, T(0));
}

// ------------------------------ Undefined

// RVV vundefined is 'poisoned' such that even XORing a _variable_ initialized
// by it gives unpredictable results. It should only be used for maskoff, so
// keep it internal. For the Highway op, just use Zero (single instruction).
namespace detail {
#define HWY_RVV_UNDEFINED(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, \
                          SHIFT, MLEN, NAME, OP)                           \
  template <size_t N>                                                      \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                       \
      NAME(HWY_RVV_D(BASE, SEW, N, SHIFT) /* tag */) {                     \
    return v##OP##_##CHAR##SEW##LMUL(); /* no AVL */                       \
  }

HWY_RVV_FOREACH(HWY_RVV_UNDEFINED, Undefined, undefined, _ALL)
#undef HWY_RVV_UNDEFINED
}  // namespace detail

template <class D>
HWY_API VFromD<D> Undefined(D d) {
  return Zero(d);
}

// ------------------------------ BitCast

namespace detail {

// Halves LMUL. (Use LMUL arg for the source so we can use _TRUNC.)
#define HWY_RVV_TRUNC(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT, \
                      MLEN, NAME, OP)                                         \
  HWY_API HWY_RVV_V(BASE, SEW, LMULH) NAME(HWY_RVV_V(BASE, SEW, LMUL) v) {    \
    return v##OP##_v_##CHAR##SEW##LMUL##_##CHAR##SEW##LMULH(v); /* no AVL */  \
  }
HWY_RVV_FOREACH(HWY_RVV_TRUNC, Trunc, lmul_trunc, _TRUNC)
#undef HWY_RVV_TRUNC

// Doubles LMUL to `d2` (the arg is only necessary for _VIRT).
#define HWY_RVV_EXT(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT,  \
                    MLEN, NAME, OP)                                          \
  template <size_t N>                                                        \
  HWY_API HWY_RVV_V(BASE, SEW, LMULD)                                        \
      NAME(HWY_RVV_D(BASE, SEW, N, SHIFT + 1) /* d2 */,                      \
           HWY_RVV_V(BASE, SEW, LMUL) v) {                                   \
    return v##OP##_v_##CHAR##SEW##LMUL##_##CHAR##SEW##LMULD(v); /* no AVL */ \
  }
HWY_RVV_FOREACH(HWY_RVV_EXT, Ext, lmul_ext, _EXT)
#undef HWY_RVV_EXT

// For virtual LMUL e.g. 'uint32mf4_t', the return type should be mf2, which is
// the same as the actual input type.
#define HWY_RVV_EXT_VIRT(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, \
                         SHIFT, MLEN, NAME, OP)                           \
  template <size_t N>                                                     \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                      \
      NAME(HWY_RVV_D(BASE, SEW, N, SHIFT + 1) /* d2 */,                   \
           HWY_RVV_V(BASE, SEW, LMUL) v) {                                \
    return v;                                                             \
  }
HWY_RVV_FOREACH(HWY_RVV_EXT_VIRT, Ext, lmul_ext, _VIRT)
#undef HWY_RVV_EXT_VIRT

// For BitCastToByte, the D arg is only to prevent duplicate definitions caused
// by _ALL_VIRT.

// There is no reinterpret from u8 <-> u8, so just return.
#define HWY_RVV_CAST_U8(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, \
                        SHIFT, MLEN, NAME, OP)                           \
  template <typename T, size_t N>                                        \
  HWY_API vuint8##LMUL##_t BitCastToByte(Simd<T, N, SHIFT> /* d */,      \
                                         vuint8##LMUL##_t v) {           \
    return v;                                                            \
  }                                                                      \
  template <size_t N>                                                    \
  HWY_API vuint8##LMUL##_t BitCastFromByte(                              \
      HWY_RVV_D(BASE, SEW, N, SHIFT) /* d */, vuint8##LMUL##_t v) {      \
    return v;                                                            \
  }

// For i8, need a single reinterpret (HWY_RVV_CAST_IF does two).
#define HWY_RVV_CAST_I8(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, \
                        SHIFT, MLEN, NAME, OP)                           \
  template <typename T, size_t N>                                        \
  HWY_API vuint8##LMUL##_t BitCastToByte(Simd<T, N, SHIFT> /* d */,      \
                                         vint8##LMUL##_t v) {            \
    return vreinterpret_v_i8##LMUL##_u8##LMUL(v);                        \
  }                                                                      \
  template <size_t N>                                                    \
  HWY_API vint8##LMUL##_t BitCastFromByte(                               \
      HWY_RVV_D(BASE, SEW, N, SHIFT) /* d */, vuint8##LMUL##_t v) {      \
    return vreinterpret_v_u8##LMUL##_i8##LMUL(v);                        \
  }

// Separate u/i because clang only provides signed <-> unsigned reinterpret for
// the same SEW.
#define HWY_RVV_CAST_U(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT, \
                       MLEN, NAME, OP)                                         \
  template <typename T, size_t N>                                              \
  HWY_API vuint8##LMUL##_t BitCastToByte(Simd<T, N, SHIFT> /* d */,            \
                                         HWY_RVV_V(BASE, SEW, LMUL) v) {       \
    return v##OP##_v_##CHAR##SEW##LMUL##_u8##LMUL(v);                          \
  }                                                                            \
  template <size_t N>                                                          \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) BitCastFromByte(                          \
      HWY_RVV_D(BASE, SEW, N, SHIFT) /* d */, vuint8##LMUL##_t v) {            \
    return v##OP##_v_u8##LMUL##_##CHAR##SEW##LMUL(v);                          \
  }

// Signed/Float: first cast to/from unsigned
#define HWY_RVV_CAST_IF(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, \
                        SHIFT, MLEN, NAME, OP)                           \
  template <typename T, size_t N>                                        \
  HWY_API vuint8##LMUL##_t BitCastToByte(Simd<T, N, SHIFT> /* d */,      \
                                         HWY_RVV_V(BASE, SEW, LMUL) v) { \
    return v##OP##_v_u##SEW##LMUL##_u8##LMUL(                            \
        v##OP##_v_##CHAR##SEW##LMUL##_u##SEW##LMUL(v));                  \
  }                                                                      \
  template <size_t N>                                                    \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) BitCastFromByte(                    \
      HWY_RVV_D(BASE, SEW, N, SHIFT) /* d */, vuint8##LMUL##_t v) {      \
    return v##OP##_v_u##SEW##LMUL##_##CHAR##SEW##LMUL(                   \
        v##OP##_v_u8##LMUL##_u##SEW##LMUL(v));                           \
  }

// Additional versions for virtual LMUL using LMULH for byte vectors.
#define HWY_RVV_CAST_VIRT_U(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, \
                            SHIFT, MLEN, NAME, OP)                           \
  template <typename T, size_t N>                                            \
  HWY_API vuint8##LMULH##_t BitCastToByte(Simd<T, N, SHIFT> /* d */,         \
                                          HWY_RVV_V(BASE, SEW, LMUL) v) {    \
    return detail::Trunc(v##OP##_v_##CHAR##SEW##LMUL##_u8##LMUL(v));         \
  }                                                                          \
  template <size_t N>                                                        \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) BitCastFromByte(                        \
      HWY_RVV_D(BASE, SEW, N, SHIFT) /* d */, vuint8##LMULH##_t v) {         \
    HWY_RVV_D(uint, 8, N, SHIFT + 1) d2;                                     \
    const vuint8##LMUL##_t v2 = detail::Ext(d2, v);                          \
    return v##OP##_v_u8##LMUL##_##CHAR##SEW##LMUL(v2);                       \
  }

// Signed/Float: first cast to/from unsigned
#define HWY_RVV_CAST_VIRT_IF(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, \
                             SHIFT, MLEN, NAME, OP)                           \
  template <typename T, size_t N>                                             \
  HWY_API vuint8##LMULH##_t BitCastToByte(Simd<T, N, SHIFT> /* d */,          \
                                          HWY_RVV_V(BASE, SEW, LMUL) v) {     \
    return detail::Trunc(v##OP##_v_u##SEW##LMUL##_u8##LMUL(                   \
        v##OP##_v_##CHAR##SEW##LMUL##_u##SEW##LMUL(v)));                      \
  }                                                                           \
  template <size_t N>                                                         \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) BitCastFromByte(                         \
      HWY_RVV_D(BASE, SEW, N, SHIFT) /* d */, vuint8##LMULH##_t v) {          \
    HWY_RVV_D(uint, 8, N, SHIFT + 1) d2;                                      \
    const vuint8##LMUL##_t v2 = detail::Ext(d2, v);                           \
    return v##OP##_v_u##SEW##LMUL##_##CHAR##SEW##LMUL(                        \
        v##OP##_v_u8##LMUL##_u##SEW##LMUL(v2));                               \
  }

HWY_RVV_FOREACH_U08(HWY_RVV_CAST_U8, _, reinterpret, _ALL)
HWY_RVV_FOREACH_I08(HWY_RVV_CAST_I8, _, reinterpret, _ALL)
HWY_RVV_FOREACH_U163264(HWY_RVV_CAST_U, _, reinterpret, _ALL)
HWY_RVV_FOREACH_I163264(HWY_RVV_CAST_IF, _, reinterpret, _ALL)
HWY_RVV_FOREACH_F(HWY_RVV_CAST_IF, _, reinterpret, _ALL)
HWY_RVV_FOREACH_U163264(HWY_RVV_CAST_VIRT_U, _, reinterpret, _VIRT)
HWY_RVV_FOREACH_I163264(HWY_RVV_CAST_VIRT_IF, _, reinterpret, _VIRT)
HWY_RVV_FOREACH_F(HWY_RVV_CAST_VIRT_IF, _, reinterpret, _VIRT)

#undef HWY_RVV_CAST_U8
#undef HWY_RVV_CAST_I8
#undef HWY_RVV_CAST_U
#undef HWY_RVV_CAST_IF
#undef HWY_RVV_CAST_VIRT_U
#undef HWY_RVV_CAST_VIRT_IF

template <size_t N, int kPow2>
HWY_INLINE VFromD<Simd<uint16_t, N, kPow2>> BitCastFromByte(
    Simd<bfloat16_t, N, kPow2> /* d */, VFromD<Simd<uint8_t, N, kPow2>> v) {
  return BitCastFromByte(Simd<uint16_t, N, kPow2>(), v);
}

}  // namespace detail

template <class D, class FromV>
HWY_API VFromD<D> BitCast(D d, FromV v) {
  return detail::BitCastFromByte(d, detail::BitCastToByte(d, v));
}

namespace detail {

template <class V, class DU = RebindToUnsigned<DFromV<V>>>
HWY_INLINE VFromD<DU> BitCastToUnsigned(V v) {
  return BitCast(DU(), v);
}

}  // namespace detail

// ------------------------------ Iota

namespace detail {

#define HWY_RVV_IOTA(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT,  \
                     MLEN, NAME, OP)                                          \
  template <size_t N>                                                         \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) NAME(HWY_RVV_D(BASE, SEW, N, SHIFT) d) { \
    return v##OP##_##CHAR##SEW##LMUL(Lanes(d));                               \
  }

HWY_RVV_FOREACH_U(HWY_RVV_IOTA, Iota0, id_v, _ALL_VIRT)
#undef HWY_RVV_IOTA

template <class D, class DU = RebindToUnsigned<D>>
HWY_INLINE VFromD<DU> Iota0(const D /*d*/) {
  return BitCastToUnsigned(Iota0(DU()));
}

}  // namespace detail

// ================================================== LOGICAL

// ------------------------------ Not

HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGV, Not, not, _ALL)

template <class V, HWY_IF_FLOAT_V(V)>
HWY_API V Not(const V v) {
  using DF = DFromV<V>;
  using DU = RebindToUnsigned<DF>;
  return BitCast(DF(), Not(BitCast(DU(), v)));
}

// ------------------------------ And

// Non-vector version (ideally immediate) for use with Iota0
namespace detail {
HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGVS, AndS, and_vx, _ALL)
}  // namespace detail

HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGVV, And, and, _ALL)

template <class V, HWY_IF_FLOAT_V(V)>
HWY_API V And(const V a, const V b) {
  using DF = DFromV<V>;
  using DU = RebindToUnsigned<DF>;
  return BitCast(DF(), And(BitCast(DU(), a), BitCast(DU(), b)));
}

// ------------------------------ Or

HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGVV, Or, or, _ALL)

template <class V, HWY_IF_FLOAT_V(V)>
HWY_API V Or(const V a, const V b) {
  using DF = DFromV<V>;
  using DU = RebindToUnsigned<DF>;
  return BitCast(DF(), Or(BitCast(DU(), a), BitCast(DU(), b)));
}

// ------------------------------ Xor

// Non-vector version (ideally immediate) for use with Iota0
namespace detail {
HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGVS, XorS, xor_vx, _ALL)
}  // namespace detail

HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGVV, Xor, xor, _ALL)

template <class V, HWY_IF_FLOAT_V(V)>
HWY_API V Xor(const V a, const V b) {
  using DF = DFromV<V>;
  using DU = RebindToUnsigned<DF>;
  return BitCast(DF(), Xor(BitCast(DU(), a), BitCast(DU(), b)));
}

// ------------------------------ AndNot

template <class V>
HWY_API V AndNot(const V not_a, const V b) {
  return And(Not(not_a), b);
}

// ------------------------------ Or3

template <class V>
HWY_API V Or3(V o1, V o2, V o3) {
  return Or(o1, Or(o2, o3));
}

// ------------------------------ OrAnd

template <class V>
HWY_API V OrAnd(const V o, const V a1, const V a2) {
  return Or(o, And(a1, a2));
}

// ------------------------------ CopySign

HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGVV, CopySign, fsgnj, _ALL)

template <class V>
HWY_API V CopySignToAbs(const V abs, const V sign) {
  // RVV can also handle abs < 0, so no extra action needed.
  return CopySign(abs, sign);
}

// ================================================== ARITHMETIC

// ------------------------------ Add

namespace detail {
HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGVS, AddS, add_vx, _ALL)
HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGVS, AddS, fadd_vf, _ALL)
HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGVS, ReverseSubS, rsub_vx, _ALL)
HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGVS, ReverseSubS, frsub_vf, _ALL)
}  // namespace detail

HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGVV, Add, add, _ALL)
HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGVV, Add, fadd, _ALL)

// ------------------------------ Sub
HWY_RVV_FOREACH_UI(HWY_RVV_RETV_ARGVV, Sub, sub, _ALL)
HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGVV, Sub, fsub, _ALL)

// ------------------------------ SaturatedAdd

HWY_RVV_FOREACH_U08(HWY_RVV_RETV_ARGVV, SaturatedAdd, saddu, _ALL)
HWY_RVV_FOREACH_U16(HWY_RVV_RETV_ARGVV, SaturatedAdd, saddu, _ALL)

HWY_RVV_FOREACH_I08(HWY_RVV_RETV_ARGVV, SaturatedAdd, sadd, _ALL)
HWY_RVV_FOREACH_I16(HWY_RVV_RETV_ARGVV, SaturatedAdd, sadd, _ALL)

// ------------------------------ SaturatedSub

HWY_RVV_FOREACH_U08(HWY_RVV_RETV_ARGVV, SaturatedSub, ssubu, _ALL)
HWY_RVV_FOREACH_U16(HWY_RVV_RETV_ARGVV, SaturatedSub, ssubu, _ALL)

HWY_RVV_FOREACH_I08(HWY_RVV_RETV_ARGVV, SaturatedSub, ssub, _ALL)
HWY_RVV_FOREACH_I16(HWY_RVV_RETV_ARGVV, SaturatedSub, ssub, _ALL)

// ------------------------------ AverageRound

// TODO(janwas): check vxrm rounding mode
HWY_RVV_FOREACH_U08(HWY_RVV_RETV_ARGVV, AverageRound, aaddu, _ALL)
HWY_RVV_FOREACH_U16(HWY_RVV_RETV_ARGVV, AverageRound, aaddu, _ALL)

// ------------------------------ ShiftLeft[Same]

// Intrinsics do not define .vi forms, so use .vx instead.
#define HWY_RVV_SHIFT(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT, \
                      MLEN, NAME, OP)                                         \
  template <int kBits>                                                        \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) NAME(HWY_RVV_V(BASE, SEW, LMUL) v) {     \
    return v##OP##_vx_##CHAR##SEW##LMUL(v, kBits, HWY_RVV_AVL(SEW, SHIFT));   \
  }                                                                           \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                          \
      NAME##Same(HWY_RVV_V(BASE, SEW, LMUL) v, int bits) {                    \
    return v##OP##_vx_##CHAR##SEW##LMUL(v, static_cast<uint8_t>(bits),        \
                                        HWY_RVV_AVL(SEW, SHIFT));             \
  }

HWY_RVV_FOREACH_UI(HWY_RVV_SHIFT, ShiftLeft, sll, _ALL)

// ------------------------------ ShiftRight[Same]

HWY_RVV_FOREACH_U(HWY_RVV_SHIFT, ShiftRight, srl, _ALL)
HWY_RVV_FOREACH_I(HWY_RVV_SHIFT, ShiftRight, sra, _ALL)

#undef HWY_RVV_SHIFT

// ------------------------------ SumsOf8 (ShiftRight, Add)
template <class VU8>
HWY_API VFromD<Repartition<uint64_t, DFromV<VU8>>> SumsOf8(const VU8 v) {
  const DFromV<VU8> du8;
  const RepartitionToWide<decltype(du8)> du16;
  const RepartitionToWide<decltype(du16)> du32;
  const RepartitionToWide<decltype(du32)> du64;
  using VU16 = VFromD<decltype(du16)>;

  const VU16 vFDB97531 = ShiftRight<8>(BitCast(du16, v));
  const VU16 vECA86420 = detail::AndS(BitCast(du16, v), 0xFF);
  const VU16 sFE_DC_BA_98_76_54_32_10 = Add(vFDB97531, vECA86420);

  const VU16 szz_FE_zz_BA_zz_76_zz_32 =
      BitCast(du16, ShiftRight<16>(BitCast(du32, sFE_DC_BA_98_76_54_32_10)));
  const VU16 sxx_FC_xx_B8_xx_74_xx_30 =
      Add(sFE_DC_BA_98_76_54_32_10, szz_FE_zz_BA_zz_76_zz_32);
  const VU16 szz_zz_xx_FC_zz_zz_xx_74 =
      BitCast(du16, ShiftRight<32>(BitCast(du64, sxx_FC_xx_B8_xx_74_xx_30)));
  const VU16 sxx_xx_xx_F8_xx_xx_xx_70 =
      Add(sxx_FC_xx_B8_xx_74_xx_30, szz_zz_xx_FC_zz_zz_xx_74);
  return detail::AndS(BitCast(du64, sxx_xx_xx_F8_xx_xx_xx_70), 0xFFFFull);
}

// ------------------------------ RotateRight
template <int kBits, class V>
HWY_API V RotateRight(const V v) {
  constexpr size_t kSizeInBits = sizeof(TFromV<V>) * 8;
  static_assert(0 <= kBits && kBits < kSizeInBits, "Invalid shift count");
  if (kBits == 0) return v;
  return Or(ShiftRight<kBits>(v), ShiftLeft<kSizeInBits - kBits>(v));
}

// ------------------------------ Shl
#define HWY_RVV_SHIFT_VV(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH,   \
                         SHIFT, MLEN, NAME, OP)                             \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                        \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) v, HWY_RVV_V(BASE, SEW, LMUL) bits) { \
    return v##OP##_vv_##CHAR##SEW##LMUL(v, bits, HWY_RVV_AVL(SEW, SHIFT));  \
  }

HWY_RVV_FOREACH_U(HWY_RVV_SHIFT_VV, Shl, sll, _ALL)

#define HWY_RVV_SHIFT_II(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH,   \
                         SHIFT, MLEN, NAME, OP)                             \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                        \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) v, HWY_RVV_V(BASE, SEW, LMUL) bits) { \
    return v##OP##_vv_##CHAR##SEW##LMUL(v, detail::BitCastToUnsigned(bits), \
                                        HWY_RVV_AVL(SEW, SHIFT));           \
  }

HWY_RVV_FOREACH_I(HWY_RVV_SHIFT_II, Shl, sll, _ALL)

// ------------------------------ Shr

HWY_RVV_FOREACH_U(HWY_RVV_SHIFT_VV, Shr, srl, _ALL)
HWY_RVV_FOREACH_I(HWY_RVV_SHIFT_II, Shr, sra, _ALL)

#undef HWY_RVV_SHIFT_II
#undef HWY_RVV_SHIFT_VV

// ------------------------------ Min

HWY_RVV_FOREACH_U(HWY_RVV_RETV_ARGVV, Min, minu, _ALL)
HWY_RVV_FOREACH_I(HWY_RVV_RETV_ARGVV, Min, min, _ALL)
HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGVV, Min, fmin, _ALL)

// ------------------------------ Max

namespace detail {

HWY_RVV_FOREACH_U(HWY_RVV_RETV_ARGVS, MaxS, maxu_vx, _ALL)
HWY_RVV_FOREACH_I(HWY_RVV_RETV_ARGVS, MaxS, max_vx, _ALL)
HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGVS, MaxS, fmax_vf, _ALL)

}  // namespace detail

HWY_RVV_FOREACH_U(HWY_RVV_RETV_ARGVV, Max, maxu, _ALL)
HWY_RVV_FOREACH_I(HWY_RVV_RETV_ARGVV, Max, max, _ALL)
HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGVV, Max, fmax, _ALL)

// ------------------------------ Mul

// Only for internal use (Highway only promises Mul for 16/32-bit inputs).
// Used by MulLower.
namespace detail {
HWY_RVV_FOREACH_U64(HWY_RVV_RETV_ARGVV, Mul, mul, _ALL)
}  // namespace detail

HWY_RVV_FOREACH_UI16(HWY_RVV_RETV_ARGVV, Mul, mul, _ALL)
HWY_RVV_FOREACH_UI32(HWY_RVV_RETV_ARGVV, Mul, mul, _ALL)
HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGVV, Mul, fmul, _ALL)

// ------------------------------ MulHigh

// Only for internal use (Highway only promises MulHigh for 16-bit inputs).
// Used by MulEven; vwmul does not work for m8.
namespace detail {
HWY_RVV_FOREACH_I32(HWY_RVV_RETV_ARGVV, MulHigh, mulh, _ALL)
HWY_RVV_FOREACH_U32(HWY_RVV_RETV_ARGVV, MulHigh, mulhu, _ALL)
HWY_RVV_FOREACH_U64(HWY_RVV_RETV_ARGVV, MulHigh, mulhu, _ALL)
}  // namespace detail

HWY_RVV_FOREACH_U16(HWY_RVV_RETV_ARGVV, MulHigh, mulhu, _ALL)
HWY_RVV_FOREACH_I16(HWY_RVV_RETV_ARGVV, MulHigh, mulh, _ALL)

// ------------------------------ MulFixedPoint15
HWY_RVV_FOREACH_I16(HWY_RVV_RETV_ARGVV, MulFixedPoint15, smul, _ALL)

// ------------------------------ Div
HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGVV, Div, fdiv, _ALL)

// ------------------------------ ApproximateReciprocal
HWY_RVV_FOREACH_F32(HWY_RVV_RETV_ARGV, ApproximateReciprocal, frec7, _ALL)

// ------------------------------ Sqrt
HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGV, Sqrt, fsqrt, _ALL)

// ------------------------------ ApproximateReciprocalSqrt
HWY_RVV_FOREACH_F32(HWY_RVV_RETV_ARGV, ApproximateReciprocalSqrt, frsqrt7, _ALL)

// ------------------------------ MulAdd
// Note: op is still named vv, not vvv.
#define HWY_RVV_FMA(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT,    \
                    MLEN, NAME, OP)                                            \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                           \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) mul, HWY_RVV_V(BASE, SEW, LMUL) x,       \
           HWY_RVV_V(BASE, SEW, LMUL) add) {                                   \
    return v##OP##_vv_##CHAR##SEW##LMUL(add, mul, x, HWY_RVV_AVL(SEW, SHIFT)); \
  }

HWY_RVV_FOREACH_F(HWY_RVV_FMA, MulAdd, fmacc, _ALL)

// ------------------------------ NegMulAdd
HWY_RVV_FOREACH_F(HWY_RVV_FMA, NegMulAdd, fnmsac, _ALL)

// ------------------------------ MulSub
HWY_RVV_FOREACH_F(HWY_RVV_FMA, MulSub, fmsac, _ALL)

// ------------------------------ NegMulSub
HWY_RVV_FOREACH_F(HWY_RVV_FMA, NegMulSub, fnmacc, _ALL)

#undef HWY_RVV_FMA

// ================================================== COMPARE

// Comparisons set a mask bit to 1 if the condition is true, else 0. The XX in
// vboolXX_t is a power of two divisor for vector bits. SLEN 8 / LMUL 1 = 1/8th
// of all bits; SLEN 8 / LMUL 4 = half of all bits.

// mask = f(vector, vector)
#define HWY_RVV_RETM_ARGVV(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, \
                           SHIFT, MLEN, NAME, OP)                           \
  HWY_API HWY_RVV_M(MLEN)                                                   \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) a, HWY_RVV_V(BASE, SEW, LMUL) b) {    \
    return v##OP##_vv_##CHAR##SEW##LMUL##_b##MLEN(a, b,                     \
                                                  HWY_RVV_AVL(SEW, SHIFT)); \
  }

// mask = f(vector, scalar)
#define HWY_RVV_RETM_ARGVS(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH,    \
                           SHIFT, MLEN, NAME, OP)                              \
  HWY_API HWY_RVV_M(MLEN)                                                      \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) a, HWY_RVV_T(BASE, SEW) b) {             \
    return v##OP##_##CHAR##SEW##LMUL##_b##MLEN(a, b, HWY_RVV_AVL(SEW, SHIFT)); \
  }

// ------------------------------ Eq
HWY_RVV_FOREACH_UI(HWY_RVV_RETM_ARGVV, Eq, mseq, _ALL)
HWY_RVV_FOREACH_F(HWY_RVV_RETM_ARGVV, Eq, mfeq, _ALL)

namespace detail {
HWY_RVV_FOREACH_UI(HWY_RVV_RETM_ARGVS, EqS, mseq_vx, _ALL)
HWY_RVV_FOREACH_F(HWY_RVV_RETM_ARGVS, EqS, mfeq_vf, _ALL)
}  // namespace detail

// ------------------------------ Ne
HWY_RVV_FOREACH_UI(HWY_RVV_RETM_ARGVV, Ne, msne, _ALL)
HWY_RVV_FOREACH_F(HWY_RVV_RETM_ARGVV, Ne, mfne, _ALL)

namespace detail {
HWY_RVV_FOREACH_UI(HWY_RVV_RETM_ARGVS, NeS, msne_vx, _ALL)
HWY_RVV_FOREACH_F(HWY_RVV_RETM_ARGVS, NeS, mfne_vf, _ALL)
}  // namespace detail

// ------------------------------ Lt
HWY_RVV_FOREACH_U(HWY_RVV_RETM_ARGVV, Lt, msltu, _ALL)
HWY_RVV_FOREACH_I(HWY_RVV_RETM_ARGVV, Lt, mslt, _ALL)
HWY_RVV_FOREACH_F(HWY_RVV_RETM_ARGVV, Lt, mflt, _ALL)

namespace detail {
HWY_RVV_FOREACH_I(HWY_RVV_RETM_ARGVS, LtS, mslt_vx, _ALL)
HWY_RVV_FOREACH_U(HWY_RVV_RETM_ARGVS, LtS, msltu_vx, _ALL)
HWY_RVV_FOREACH_F(HWY_RVV_RETM_ARGVS, LtS, mflt_vf, _ALL)
}  // namespace detail

// ------------------------------ Le
HWY_RVV_FOREACH_F(HWY_RVV_RETM_ARGVV, Le, mfle, _ALL)

#undef HWY_RVV_RETM_ARGVV
#undef HWY_RVV_RETM_ARGVS

// ------------------------------ Gt/Ge

template <class V>
HWY_API auto Ge(const V a, const V b) -> decltype(Le(a, b)) {
  return Le(b, a);
}

template <class V>
HWY_API auto Gt(const V a, const V b) -> decltype(Lt(a, b)) {
  return Lt(b, a);
}

// ------------------------------ TestBit
template <class V>
HWY_API auto TestBit(const V a, const V bit) -> decltype(Eq(a, bit)) {
  return detail::NeS(And(a, bit), 0);
}

// ------------------------------ Not
// NOLINTNEXTLINE
HWY_RVV_FOREACH_B(HWY_RVV_RETM_ARGM, Not, not )

// ------------------------------ And

// mask = f(mask_a, mask_b) (note arg2,arg1 order!)
#define HWY_RVV_RETM_ARGMM(SEW, SHIFT, MLEN, NAME, OP)                 \
  HWY_API HWY_RVV_M(MLEN) NAME(HWY_RVV_M(MLEN) a, HWY_RVV_M(MLEN) b) { \
    return vm##OP##_mm_b##MLEN(b, a, HWY_RVV_AVL(SEW, SHIFT));         \
  }

HWY_RVV_FOREACH_B(HWY_RVV_RETM_ARGMM, And, and)

// ------------------------------ AndNot
HWY_RVV_FOREACH_B(HWY_RVV_RETM_ARGMM, AndNot, andn)

// ------------------------------ Or
HWY_RVV_FOREACH_B(HWY_RVV_RETM_ARGMM, Or, or)

// ------------------------------ Xor
HWY_RVV_FOREACH_B(HWY_RVV_RETM_ARGMM, Xor, xor)

#undef HWY_RVV_RETM_ARGMM

// ------------------------------ IfThenElse
#define HWY_RVV_IF_THEN_ELSE(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH,  \
                             SHIFT, MLEN, NAME, OP)                            \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                           \
      NAME(HWY_RVV_M(MLEN) m, HWY_RVV_V(BASE, SEW, LMUL) yes,                  \
           HWY_RVV_V(BASE, SEW, LMUL) no) {                                    \
    return v##OP##_vvm_##CHAR##SEW##LMUL(m, no, yes, HWY_RVV_AVL(SEW, SHIFT)); \
  }

HWY_RVV_FOREACH(HWY_RVV_IF_THEN_ELSE, IfThenElse, merge, _ALL)

#undef HWY_RVV_IF_THEN_ELSE

// ------------------------------ IfThenElseZero
template <class M, class V>
HWY_API V IfThenElseZero(const M mask, const V yes) {
  return IfThenElse(mask, yes, Zero(DFromV<V>()));
}

// ------------------------------ IfThenZeroElse

#define HWY_RVV_IF_THEN_ZERO_ELSE(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, \
                                  LMULH, SHIFT, MLEN, NAME, OP)             \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                        \
      NAME(HWY_RVV_M(MLEN) m, HWY_RVV_V(BASE, SEW, LMUL) no) {              \
    return v##OP##_##CHAR##SEW##LMUL(m, no, 0, HWY_RVV_AVL(SEW, SHIFT));    \
  }

HWY_RVV_FOREACH_UI(HWY_RVV_IF_THEN_ZERO_ELSE, IfThenZeroElse, merge_vxm, _ALL)
HWY_RVV_FOREACH_F(HWY_RVV_IF_THEN_ZERO_ELSE, IfThenZeroElse, fmerge_vfm, _ALL)

#undef HWY_RVV_IF_THEN_ZERO_ELSE

// ------------------------------ MaskFromVec

template <class V>
HWY_API auto MaskFromVec(const V v) -> decltype(Eq(v, v)) {
  return detail::NeS(v, 0);
}

template <class D>
using MFromD = decltype(MaskFromVec(Zero(D())));

template <class D, typename MFrom>
HWY_API MFromD<D> RebindMask(const D /*d*/, const MFrom mask) {
  // No need to check lane size/LMUL are the same: if not, casting MFrom to
  // MFromD<D> would fail.
  return mask;
}

// ------------------------------ VecFromMask

namespace detail {
#define HWY_RVV_VEC_FROM_MASK(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, \
                              SHIFT, MLEN, NAME, OP)                           \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                           \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) v0, HWY_RVV_M(MLEN) m) {                 \
    return v##OP##_##CHAR##SEW##LMUL##_m(m, v0, v0, 1,                         \
                                         HWY_RVV_AVL(SEW, SHIFT));             \
  }

HWY_RVV_FOREACH_UI(HWY_RVV_VEC_FROM_MASK, SubS, sub_vx, _ALL)
#undef HWY_RVV_VEC_FROM_MASK
}  // namespace detail

template <class D, HWY_IF_NOT_FLOAT_D(D)>
HWY_API VFromD<D> VecFromMask(const D d, MFromD<D> mask) {
  return detail::SubS(Zero(d), mask);
}

template <class D, HWY_IF_FLOAT_D(D)>
HWY_API VFromD<D> VecFromMask(const D d, MFromD<D> mask) {
  return BitCast(d, VecFromMask(RebindToUnsigned<D>(), mask));
}

// ------------------------------ IfVecThenElse (MaskFromVec)

template <class V>
HWY_API V IfVecThenElse(const V mask, const V yes, const V no) {
  return IfThenElse(MaskFromVec(mask), yes, no);
}

// ------------------------------ ZeroIfNegative
template <class V>
HWY_API V ZeroIfNegative(const V v) {
  return IfThenZeroElse(detail::LtS(v, 0), v);
}

// ------------------------------ BroadcastSignBit
template <class V>
HWY_API V BroadcastSignBit(const V v) {
  return ShiftRight<sizeof(TFromV<V>) * 8 - 1>(v);
}

// ------------------------------ IfNegativeThenElse (BroadcastSignBit)
template <class V>
HWY_API V IfNegativeThenElse(V v, V yes, V no) {
  static_assert(IsSigned<TFromV<V>>(), "Only works for signed/float");
  const DFromV<V> d;
  const RebindToSigned<decltype(d)> di;

  MFromD<decltype(d)> m =
      MaskFromVec(BitCast(d, BroadcastSignBit(BitCast(di, v))));
  return IfThenElse(m, yes, no);
}

// ------------------------------ FindFirstTrue

#define HWY_RVV_FIND_FIRST_TRUE(SEW, SHIFT, MLEN, NAME, OP) \
  template <class D>                                        \
  HWY_API intptr_t FindFirstTrue(D d, HWY_RVV_M(MLEN) m) {  \
    static_assert(MLenFromD(d) == MLEN, "Type mismatch");   \
    return vfirst_m_b##MLEN(m, Lanes(d));                   \
  }

HWY_RVV_FOREACH_B(HWY_RVV_FIND_FIRST_TRUE, _, _)
#undef HWY_RVV_FIND_FIRST_TRUE

// ------------------------------ AllFalse
template <class D>
HWY_API bool AllFalse(D d, MFromD<D> m) {
  return FindFirstTrue(d, m) < 0;
}

// ------------------------------ AllTrue

#define HWY_RVV_ALL_TRUE(SEW, SHIFT, MLEN, NAME, OP)      \
  template <class D>                                      \
  HWY_API bool AllTrue(D d, HWY_RVV_M(MLEN) m) {          \
    static_assert(MLenFromD(d) == MLEN, "Type mismatch"); \
    return AllFalse(d, vmnot_m_b##MLEN(m, Lanes(d)));     \
  }

HWY_RVV_FOREACH_B(HWY_RVV_ALL_TRUE, _, _)
#undef HWY_RVV_ALL_TRUE

// ------------------------------ CountTrue

#define HWY_RVV_COUNT_TRUE(SEW, SHIFT, MLEN, NAME, OP)    \
  template <class D>                                      \
  HWY_API size_t CountTrue(D d, HWY_RVV_M(MLEN) m) {      \
    static_assert(MLenFromD(d) == MLEN, "Type mismatch"); \
    return vcpop_m_b##MLEN(m, Lanes(d));                  \
  }

HWY_RVV_FOREACH_B(HWY_RVV_COUNT_TRUE, _, _)
#undef HWY_RVV_COUNT_TRUE

// ================================================== MEMORY

// ------------------------------ Load

#define HWY_RVV_LOAD(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT, \
                     MLEN, NAME, OP)                                         \
  template <size_t N>                                                        \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                         \
      NAME(HWY_RVV_D(BASE, SEW, N, SHIFT) d,                                 \
           const HWY_RVV_T(BASE, SEW) * HWY_RESTRICT p) {                    \
    return v##OP##SEW##_v_##CHAR##SEW##LMUL(p, Lanes(d));                    \
  }
HWY_RVV_FOREACH(HWY_RVV_LOAD, Load, le, _ALL_VIRT)
#undef HWY_RVV_LOAD

// There is no native BF16, treat as uint16_t.
template <size_t N, int kPow2>
HWY_API VFromD<Simd<uint16_t, N, kPow2>> Load(
    Simd<bfloat16_t, N, kPow2> d, const bfloat16_t* HWY_RESTRICT p) {
  return Load(RebindToUnsigned<decltype(d)>(),
              reinterpret_cast<const uint16_t * HWY_RESTRICT>(p));
}

template <size_t N, int kPow2>
HWY_API void Store(VFromD<Simd<uint16_t, N, kPow2>> v,
                   Simd<bfloat16_t, N, kPow2> d, bfloat16_t* HWY_RESTRICT p) {
  Store(v, RebindToUnsigned<decltype(d)>(),
        reinterpret_cast<uint16_t * HWY_RESTRICT>(p));
}

// ------------------------------ LoadU

// RVV only requires lane alignment, not natural alignment of the entire vector.
template <class D>
HWY_API VFromD<D> LoadU(D d, const TFromD<D>* HWY_RESTRICT p) {
  return Load(d, p);
}

// ------------------------------ MaskedLoad

#define HWY_RVV_MASKED_LOAD(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, \
                            SHIFT, MLEN, NAME, OP)                           \
  template <size_t N>                                                        \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                         \
      NAME(HWY_RVV_M(MLEN) m, HWY_RVV_D(BASE, SEW, N, SHIFT) d,              \
           const HWY_RVV_T(BASE, SEW) * HWY_RESTRICT p) {                    \
    return v##OP##SEW##_v_##CHAR##SEW##LMUL##_m(m, Zero(d), p, Lanes(d));    \
  }
HWY_RVV_FOREACH(HWY_RVV_MASKED_LOAD, MaskedLoad, le, _ALL_VIRT)
#undef HWY_RVV_MASKED_LOAD

// ------------------------------ Store

#define HWY_RVV_STORE(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT, \
                      MLEN, NAME, OP)                                         \
  template <size_t N>                                                         \
  HWY_API void NAME(HWY_RVV_V(BASE, SEW, LMUL) v,                             \
                    HWY_RVV_D(BASE, SEW, N, SHIFT) d,                         \
                    HWY_RVV_T(BASE, SEW) * HWY_RESTRICT p) {                  \
    return v##OP##SEW##_v_##CHAR##SEW##LMUL(p, v, Lanes(d));                  \
  }
HWY_RVV_FOREACH(HWY_RVV_STORE, Store, se, _ALL_VIRT)
#undef HWY_RVV_STORE

// ------------------------------ BlendedStore

#define HWY_RVV_BLENDED_STORE(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, \
                              SHIFT, MLEN, NAME, OP)                           \
  template <size_t N>                                                          \
  HWY_API void NAME(HWY_RVV_V(BASE, SEW, LMUL) v, HWY_RVV_M(MLEN) m,           \
                    HWY_RVV_D(BASE, SEW, N, SHIFT) d,                          \
                    HWY_RVV_T(BASE, SEW) * HWY_RESTRICT p) {                   \
    return v##OP##SEW##_v_##CHAR##SEW##LMUL##_m(m, p, v, Lanes(d));            \
  }
HWY_RVV_FOREACH(HWY_RVV_BLENDED_STORE, BlendedStore, se, _ALL_VIRT)
#undef HWY_RVV_BLENDED_STORE

namespace detail {

#define HWY_RVV_STOREN(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT, \
                       MLEN, NAME, OP)                                         \
  template <size_t N>                                                          \
  HWY_API void NAME(size_t count, HWY_RVV_V(BASE, SEW, LMUL) v,                \
                    HWY_RVV_D(BASE, SEW, N, SHIFT) /* d */,                    \
                    HWY_RVV_T(BASE, SEW) * HWY_RESTRICT p) {                   \
    return v##OP##SEW##_v_##CHAR##SEW##LMUL(p, v, count);                      \
  }
HWY_RVV_FOREACH(HWY_RVV_STOREN, StoreN, se, _ALL_VIRT)
#undef HWY_RVV_STOREN

}  // namespace detail

// ------------------------------ StoreU

// RVV only requires lane alignment, not natural alignment of the entire vector.
template <class V, class D>
HWY_API void StoreU(const V v, D d, TFromD<D>* HWY_RESTRICT p) {
  Store(v, d, p);
}

// ------------------------------ Stream
template <class V, class D, typename T>
HWY_API void Stream(const V v, D d, T* HWY_RESTRICT aligned) {
  Store(v, d, aligned);
}

// ------------------------------ ScatterOffset

#define HWY_RVV_SCATTER(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, \
                        SHIFT, MLEN, NAME, OP)                           \
  template <size_t N>                                                    \
  HWY_API void NAME(HWY_RVV_V(BASE, SEW, LMUL) v,                        \
                    HWY_RVV_D(BASE, SEW, N, SHIFT) d,                    \
                    HWY_RVV_T(BASE, SEW) * HWY_RESTRICT base,            \
                    HWY_RVV_V(int, SEW, LMUL) offset) {                  \
    return v##OP##ei##SEW##_v_##CHAR##SEW##LMUL(                         \
        base, detail::BitCastToUnsigned(offset), v, Lanes(d));           \
  }
HWY_RVV_FOREACH(HWY_RVV_SCATTER, ScatterOffset, sux, _ALL_VIRT)
#undef HWY_RVV_SCATTER

// ------------------------------ ScatterIndex

template <class D, HWY_IF_LANE_SIZE_D(D, 4)>
HWY_API void ScatterIndex(VFromD<D> v, D d, TFromD<D>* HWY_RESTRICT base,
                          const VFromD<RebindToSigned<D>> index) {
  return ScatterOffset(v, d, base, ShiftLeft<2>(index));
}

template <class D, HWY_IF_LANE_SIZE_D(D, 8)>
HWY_API void ScatterIndex(VFromD<D> v, D d, TFromD<D>* HWY_RESTRICT base,
                          const VFromD<RebindToSigned<D>> index) {
  return ScatterOffset(v, d, base, ShiftLeft<3>(index));
}

// ------------------------------ GatherOffset

#define HWY_RVV_GATHER(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT, \
                       MLEN, NAME, OP)                                         \
  template <size_t N>                                                          \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                           \
      NAME(HWY_RVV_D(BASE, SEW, N, SHIFT) d,                                   \
           const HWY_RVV_T(BASE, SEW) * HWY_RESTRICT base,                     \
           HWY_RVV_V(int, SEW, LMUL) offset) {                                 \
    return v##OP##ei##SEW##_v_##CHAR##SEW##LMUL(                               \
        base, detail::BitCastToUnsigned(offset), Lanes(d));                    \
  }
HWY_RVV_FOREACH(HWY_RVV_GATHER, GatherOffset, lux, _ALL_VIRT)
#undef HWY_RVV_GATHER

// ------------------------------ GatherIndex

template <class D, HWY_IF_LANE_SIZE_D(D, 4)>
HWY_API VFromD<D> GatherIndex(D d, const TFromD<D>* HWY_RESTRICT base,
                              const VFromD<RebindToSigned<D>> index) {
  return GatherOffset(d, base, ShiftLeft<2>(index));
}

template <class D, HWY_IF_LANE_SIZE_D(D, 8)>
HWY_API VFromD<D> GatherIndex(D d, const TFromD<D>* HWY_RESTRICT base,
                              const VFromD<RebindToSigned<D>> index) {
  return GatherOffset(d, base, ShiftLeft<3>(index));
}

// ------------------------------ LoadInterleaved2

// Per-target flag to prevent generic_ops-inl.h from defining LoadInterleaved2.
#ifdef HWY_NATIVE_LOAD_STORE_INTERLEAVED
#undef HWY_NATIVE_LOAD_STORE_INTERLEAVED
#else
#define HWY_NATIVE_LOAD_STORE_INTERLEAVED
#endif

#define HWY_RVV_LOAD2(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT, \
                      MLEN, NAME, OP)                                         \
  template <size_t N>                                                         \
  HWY_API void NAME(HWY_RVV_D(BASE, SEW, N, SHIFT) d,                         \
                    const HWY_RVV_T(BASE, SEW) * HWY_RESTRICT unaligned,      \
                    HWY_RVV_V(BASE, SEW, LMUL) & v0,                          \
                    HWY_RVV_V(BASE, SEW, LMUL) & v1) {                        \
    v##OP##e##SEW##_v_##CHAR##SEW##LMUL(&v0, &v1, unaligned, Lanes(d));       \
  }
// Segments are limited to 8 registers, so we can only go up to LMUL=2.
HWY_RVV_FOREACH(HWY_RVV_LOAD2, LoadInterleaved2, lseg2, _LE2_VIRT)
#undef HWY_RVV_LOAD2

// ------------------------------ LoadInterleaved3

#define HWY_RVV_LOAD3(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT, \
                      MLEN, NAME, OP)                                         \
  template <size_t N>                                                         \
  HWY_API void NAME(HWY_RVV_D(BASE, SEW, N, SHIFT) d,                         \
                    const HWY_RVV_T(BASE, SEW) * HWY_RESTRICT unaligned,      \
                    HWY_RVV_V(BASE, SEW, LMUL) & v0,                          \
                    HWY_RVV_V(BASE, SEW, LMUL) & v1,                          \
                    HWY_RVV_V(BASE, SEW, LMUL) & v2) {                        \
    v##OP##e##SEW##_v_##CHAR##SEW##LMUL(&v0, &v1, &v2, unaligned, Lanes(d));  \
  }
// Segments are limited to 8 registers, so we can only go up to LMUL=2.
HWY_RVV_FOREACH(HWY_RVV_LOAD3, LoadInterleaved3, lseg3, _LE2_VIRT)
#undef HWY_RVV_LOAD3

// ------------------------------ LoadInterleaved4

#define HWY_RVV_LOAD4(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT, \
                      MLEN, NAME, OP)                                         \
  template <size_t N>                                                         \
  HWY_API void NAME(                                                          \
      HWY_RVV_D(BASE, SEW, N, SHIFT) d,                                       \
      const HWY_RVV_T(BASE, SEW) * HWY_RESTRICT aligned,                      \
      HWY_RVV_V(BASE, SEW, LMUL) & v0, HWY_RVV_V(BASE, SEW, LMUL) & v1,       \
      HWY_RVV_V(BASE, SEW, LMUL) & v2, HWY_RVV_V(BASE, SEW, LMUL) & v3) {     \
    v##OP##e##SEW##_v_##CHAR##SEW##LMUL(&v0, &v1, &v2, &v3, aligned,          \
                                        Lanes(d));                            \
  }
// Segments are limited to 8 registers, so we can only go up to LMUL=2.
HWY_RVV_FOREACH(HWY_RVV_LOAD4, LoadInterleaved4, lseg4, _LE2_VIRT)
#undef HWY_RVV_LOAD4

// ------------------------------ StoreInterleaved2

#define HWY_RVV_STORE2(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT, \
                       MLEN, NAME, OP)                                         \
  template <size_t N>                                                          \
  HWY_API void NAME(HWY_RVV_V(BASE, SEW, LMUL) v0,                             \
                    HWY_RVV_V(BASE, SEW, LMUL) v1,                             \
                    HWY_RVV_D(BASE, SEW, N, SHIFT) d,                          \
                    HWY_RVV_T(BASE, SEW) * HWY_RESTRICT unaligned) {           \
    v##OP##e##SEW##_v_##CHAR##SEW##LMUL(unaligned, v0, v1, Lanes(d));          \
  }
// Segments are limited to 8 registers, so we can only go up to LMUL=2.
HWY_RVV_FOREACH(HWY_RVV_STORE2, StoreInterleaved2, sseg2, _LE2_VIRT)
#undef HWY_RVV_STORE2

// ------------------------------ StoreInterleaved3

#define HWY_RVV_STORE3(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT, \
                       MLEN, NAME, OP)                                         \
  template <size_t N>                                                          \
  HWY_API void NAME(                                                           \
      HWY_RVV_V(BASE, SEW, LMUL) v0, HWY_RVV_V(BASE, SEW, LMUL) v1,            \
      HWY_RVV_V(BASE, SEW, LMUL) v2, HWY_RVV_D(BASE, SEW, N, SHIFT) d,         \
      HWY_RVV_T(BASE, SEW) * HWY_RESTRICT unaligned) {                         \
    v##OP##e##SEW##_v_##CHAR##SEW##LMUL(unaligned, v0, v1, v2, Lanes(d));      \
  }
// Segments are limited to 8 registers, so we can only go up to LMUL=2.
HWY_RVV_FOREACH(HWY_RVV_STORE3, StoreInterleaved3, sseg3, _LE2_VIRT)
#undef HWY_RVV_STORE3

// ------------------------------ StoreInterleaved4

#define HWY_RVV_STORE4(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT, \
                       MLEN, NAME, OP)                                         \
  template <size_t N>                                                          \
  HWY_API void NAME(                                                           \
      HWY_RVV_V(BASE, SEW, LMUL) v0, HWY_RVV_V(BASE, SEW, LMUL) v1,            \
      HWY_RVV_V(BASE, SEW, LMUL) v2, HWY_RVV_V(BASE, SEW, LMUL) v3,            \
      HWY_RVV_D(BASE, SEW, N, SHIFT) d,                                        \
      HWY_RVV_T(BASE, SEW) * HWY_RESTRICT aligned) {                           \
    v##OP##e##SEW##_v_##CHAR##SEW##LMUL(aligned, v0, v1, v2, v3, Lanes(d));    \
  }
// Segments are limited to 8 registers, so we can only go up to LMUL=2.
HWY_RVV_FOREACH(HWY_RVV_STORE4, StoreInterleaved4, sseg4, _LE2_VIRT)
#undef HWY_RVV_STORE4

// ================================================== CONVERT

// ------------------------------ PromoteTo

// SEW is for the input so we can use F16 (no-op if not supported).
#define HWY_RVV_PROMOTE(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH,     \
                        SHIFT, MLEN, NAME, OP)                               \
  template <size_t N>                                                        \
  HWY_API HWY_RVV_V(BASE, SEWD, LMULD) NAME(                                 \
      HWY_RVV_D(BASE, SEWD, N, SHIFT + 1) d, HWY_RVV_V(BASE, SEW, LMUL) v) { \
    return OP##CHAR##SEWD##LMULD(v, Lanes(d));                               \
  }

HWY_RVV_FOREACH_U08(HWY_RVV_PROMOTE, PromoteTo, vzext_vf2_, _EXT_VIRT)
HWY_RVV_FOREACH_U16(HWY_RVV_PROMOTE, PromoteTo, vzext_vf2_, _EXT_VIRT)
HWY_RVV_FOREACH_U32(HWY_RVV_PROMOTE, PromoteTo, vzext_vf2_, _EXT_VIRT)
HWY_RVV_FOREACH_I08(HWY_RVV_PROMOTE, PromoteTo, vsext_vf2_, _EXT_VIRT)
HWY_RVV_FOREACH_I16(HWY_RVV_PROMOTE, PromoteTo, vsext_vf2_, _EXT_VIRT)
HWY_RVV_FOREACH_I32(HWY_RVV_PROMOTE, PromoteTo, vsext_vf2_, _EXT_VIRT)
HWY_RVV_FOREACH_F16(HWY_RVV_PROMOTE, PromoteTo, vfwcvt_f_f_v_, _EXT_VIRT)
HWY_RVV_FOREACH_F32(HWY_RVV_PROMOTE, PromoteTo, vfwcvt_f_f_v_, _EXT_VIRT)
#undef HWY_RVV_PROMOTE

// The above X-macro cannot handle 4x promotion nor type switching.
// TODO(janwas): use BASE2 arg to allow the latter.
#define HWY_RVV_PROMOTE(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN, LMUL, LMUL_IN, \
                        SHIFT, ADD)                                            \
  template <size_t N>                                                          \
  HWY_API HWY_RVV_V(BASE, BITS, LMUL)                                          \
      PromoteTo(HWY_RVV_D(BASE, BITS, N, SHIFT + ADD) d,                       \
                HWY_RVV_V(BASE_IN, BITS_IN, LMUL_IN) v) {                      \
    return OP##CHAR##BITS##LMUL(v, Lanes(d));                                  \
  }

#define HWY_RVV_PROMOTE_X2(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN)        \
  HWY_RVV_PROMOTE(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN, m1, mf2, -2, 1) \
  HWY_RVV_PROMOTE(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN, m1, mf2, -1, 1) \
  HWY_RVV_PROMOTE(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN, m2, m1, 0, 1)   \
  HWY_RVV_PROMOTE(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN, m4, m2, 1, 1)   \
  HWY_RVV_PROMOTE(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN, m8, m4, 2, 1)

#define HWY_RVV_PROMOTE_X4(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN)         \
  HWY_RVV_PROMOTE(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN, mf2, mf8, -3, 2) \
  HWY_RVV_PROMOTE(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN, m1, mf4, -2, 2)  \
  HWY_RVV_PROMOTE(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN, m2, mf2, -1, 2)  \
  HWY_RVV_PROMOTE(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN, m4, m1, 0, 2)    \
  HWY_RVV_PROMOTE(OP, BASE, CHAR, BITS, BASE_IN, BITS_IN, m8, m2, 1, 2)

HWY_RVV_PROMOTE_X4(vzext_vf4_, uint, u, 32, uint, 8)
HWY_RVV_PROMOTE_X4(vsext_vf4_, int, i, 32, int, 8)

// i32 to f64
HWY_RVV_PROMOTE_X2(vfwcvt_f_x_v_, float, f, 64, int, 32)

#undef HWY_RVV_PROMOTE_X4
#undef HWY_RVV_PROMOTE_X2
#undef HWY_RVV_PROMOTE

// Unsigned to signed: cast for unsigned promote.
template <size_t N, int kPow2>
HWY_API auto PromoteTo(Simd<int16_t, N, kPow2> d,
                       VFromD<Rebind<uint8_t, decltype(d)>> v)
    -> VFromD<decltype(d)> {
  return BitCast(d, PromoteTo(RebindToUnsigned<decltype(d)>(), v));
}

template <size_t N, int kPow2>
HWY_API auto PromoteTo(Simd<int32_t, N, kPow2> d,
                       VFromD<Rebind<uint8_t, decltype(d)>> v)
    -> VFromD<decltype(d)> {
  return BitCast(d, PromoteTo(RebindToUnsigned<decltype(d)>(), v));
}

template <size_t N, int kPow2>
HWY_API auto PromoteTo(Simd<int32_t, N, kPow2> d,
                       VFromD<Rebind<uint16_t, decltype(d)>> v)
    -> VFromD<decltype(d)> {
  return BitCast(d, PromoteTo(RebindToUnsigned<decltype(d)>(), v));
}

template <size_t N, int kPow2>
HWY_API auto PromoteTo(Simd<float32_t, N, kPow2> d,
                       VFromD<Rebind<bfloat16_t, decltype(d)>> v)
    -> VFromD<decltype(d)> {
  const RebindToSigned<decltype(d)> di32;
  const Rebind<uint16_t, decltype(d)> du16;
  return BitCast(d, ShiftLeft<16>(PromoteTo(di32, BitCast(du16, v))));
}

// ------------------------------ DemoteTo U

// SEW is for the source so we can use _DEMOTE.
#define HWY_RVV_DEMOTE(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT, \
                       MLEN, NAME, OP)                                         \
  template <size_t N>                                                          \
  HWY_API HWY_RVV_V(BASE, SEWH, LMULH) NAME(                                   \
      HWY_RVV_D(BASE, SEWH, N, SHIFT - 1) d, HWY_RVV_V(BASE, SEW, LMUL) v) {   \
    return OP##CHAR##SEWH##LMULH(v, 0, Lanes(d));                              \
  }                                                                            \
  template <size_t N>                                                          \
  HWY_API HWY_RVV_V(BASE, SEWH, LMULH) NAME##Shr16(                            \
      HWY_RVV_D(BASE, SEWH, N, SHIFT - 1) d, HWY_RVV_V(BASE, SEW, LMUL) v) {   \
    return OP##CHAR##SEWH##LMULH(v, 16, Lanes(d));                             \
  }

// Unsigned -> unsigned (also used for bf16)
namespace detail {
HWY_RVV_FOREACH_U16(HWY_RVV_DEMOTE, DemoteTo, vnclipu_wx_, _DEMOTE_VIRT)
HWY_RVV_FOREACH_U32(HWY_RVV_DEMOTE, DemoteTo, vnclipu_wx_, _DEMOTE_VIRT)
}  // namespace detail

// SEW is for the source so we can use _DEMOTE.
#define HWY_RVV_DEMOTE_I_TO_U(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, \
                              SHIFT, MLEN, NAME, OP)                           \
  template <size_t N>                                                          \
  HWY_API HWY_RVV_V(uint, SEWH, LMULH) NAME(                                   \
      HWY_RVV_D(uint, SEWH, N, SHIFT - 1) d, HWY_RVV_V(int, SEW, LMUL) v) {    \
    /* First clamp negative numbers to zero to match x86 packus. */            \
    return detail::DemoteTo(d, detail::BitCastToUnsigned(detail::MaxS(v, 0))); \
  }
HWY_RVV_FOREACH_I32(HWY_RVV_DEMOTE_I_TO_U, DemoteTo, _, _DEMOTE_VIRT)
HWY_RVV_FOREACH_I16(HWY_RVV_DEMOTE_I_TO_U, DemoteTo, _, _DEMOTE_VIRT)
#undef HWY_RVV_DEMOTE_I_TO_U

template <size_t N>
HWY_API vuint8mf8_t DemoteTo(Simd<uint8_t, N, -3> d, const vint32mf2_t v) {
  return vnclipu_wx_u8mf8(DemoteTo(Simd<uint16_t, N, -2>(), v), 0, Lanes(d));
}
template <size_t N>
HWY_API vuint8mf4_t DemoteTo(Simd<uint8_t, N, -2> d, const vint32m1_t v) {
  return vnclipu_wx_u8mf4(DemoteTo(Simd<uint16_t, N, -1>(), v), 0, Lanes(d));
}
template <size_t N>
HWY_API vuint8mf2_t DemoteTo(Simd<uint8_t, N, -1> d, const vint32m2_t v) {
  return vnclipu_wx_u8mf2(DemoteTo(Simd<uint16_t, N, 0>(), v), 0, Lanes(d));
}
template <size_t N>
HWY_API vuint8m1_t DemoteTo(Simd<uint8_t, N, 0> d, const vint32m4_t v) {
  return vnclipu_wx_u8m1(DemoteTo(Simd<uint16_t, N, 1>(), v), 0, Lanes(d));
}
template <size_t N>
HWY_API vuint8m2_t DemoteTo(Simd<uint8_t, N, 1> d, const vint32m8_t v) {
  return vnclipu_wx_u8m2(DemoteTo(Simd<uint16_t, N, 2>(), v), 0, Lanes(d));
}

HWY_API vuint8mf8_t U8FromU32(const vuint32mf2_t v) {
  const size_t avl = Lanes(ScalableTag<uint8_t, -3>());
  return vnclipu_wx_u8mf8(vnclipu_wx_u16mf4(v, 0, avl), 0, avl);
}
HWY_API vuint8mf4_t U8FromU32(const vuint32m1_t v) {
  const size_t avl = Lanes(ScalableTag<uint8_t, -2>());
  return vnclipu_wx_u8mf4(vnclipu_wx_u16mf2(v, 0, avl), 0, avl);
}
HWY_API vuint8mf2_t U8FromU32(const vuint32m2_t v) {
  const size_t avl = Lanes(ScalableTag<uint8_t, -1>());
  return vnclipu_wx_u8mf2(vnclipu_wx_u16m1(v, 0, avl), 0, avl);
}
HWY_API vuint8m1_t U8FromU32(const vuint32m4_t v) {
  const size_t avl = Lanes(ScalableTag<uint8_t, 0>());
  return vnclipu_wx_u8m1(vnclipu_wx_u16m2(v, 0, avl), 0, avl);
}
HWY_API vuint8m2_t U8FromU32(const vuint32m8_t v) {
  const size_t avl = Lanes(ScalableTag<uint8_t, 1>());
  return vnclipu_wx_u8m2(vnclipu_wx_u16m4(v, 0, avl), 0, avl);
}

// ------------------------------ Truncations

template <size_t N>
HWY_API vuint8mf8_t TruncateTo(Simd<uint8_t, N, -3> d,
                               const VFromD<Simd<uint64_t, N, 0>> v) {
  const size_t avl = Lanes(d);
  const vuint64m1_t v1 = vand(v, 0xFF, avl);
  const vuint32mf2_t v2 = vnclipu_wx_u32mf2(v1, 0, avl);
  const vuint16mf4_t v3 = vnclipu_wx_u16mf4(v2, 0, avl);
  return vnclipu_wx_u8mf8(v3, 0, avl);
}

template <size_t N>
HWY_API vuint8mf4_t TruncateTo(Simd<uint8_t, N, -2> d,
                               const VFromD<Simd<uint64_t, N, 1>> v) {
  const size_t avl = Lanes(d);
  const vuint64m2_t v1 = vand(v, 0xFF, avl);
  const vuint32m1_t v2 = vnclipu_wx_u32m1(v1, 0, avl);
  const vuint16mf2_t v3 = vnclipu_wx_u16mf2(v2, 0, avl);
  return vnclipu_wx_u8mf4(v3, 0, avl);
}

template <size_t N>
HWY_API vuint8mf2_t TruncateTo(Simd<uint8_t, N, -1> d,
                               const VFromD<Simd<uint64_t, N, 2>> v) {
  const size_t avl = Lanes(d);
  const vuint64m4_t v1 = vand(v, 0xFF, avl);
  const vuint32m2_t v2 = vnclipu_wx_u32m2(v1, 0, avl);
  const vuint16m1_t v3 = vnclipu_wx_u16m1(v2, 0, avl);
  return vnclipu_wx_u8mf2(v3, 0, avl);
}

template <size_t N>
HWY_API vuint8m1_t TruncateTo(Simd<uint8_t, N, 0> d,
                              const VFromD<Simd<uint64_t, N, 3>> v) {
  const size_t avl = Lanes(d);
  const vuint64m8_t v1 = vand(v, 0xFF, avl);
  const vuint32m4_t v2 = vnclipu_wx_u32m4(v1, 0, avl);
  const vuint16m2_t v3 = vnclipu_wx_u16m2(v2, 0, avl);
  return vnclipu_wx_u8m1(v3, 0, avl);
}

template <size_t N>
HWY_API vuint16mf4_t TruncateTo(Simd<uint16_t, N, -2> d,
                                const VFromD<Simd<uint64_t, N, 0>> v) {
  const size_t avl = Lanes(d);
  const vuint64m1_t v1 = vand(v, 0xFFFF, avl);
  const vuint32mf2_t v2 = vnclipu_wx_u32mf2(v1, 0, avl);
  return vnclipu_wx_u16mf4(v2, 0, avl);
}

template <size_t N>
HWY_API vuint16mf2_t TruncateTo(Simd<uint16_t, N, -1> d,
                                const VFromD<Simd<uint64_t, N, 1>> v) {
  const size_t avl = Lanes(d);
  const vuint64m2_t v1 = vand(v, 0xFFFF, avl);
  const vuint32m1_t v2 = vnclipu_wx_u32m1(v1, 0, avl);
  return vnclipu_wx_u16mf2(v2, 0, avl);
}

template <size_t N>
HWY_API vuint16m1_t TruncateTo(Simd<uint16_t, N, 0> d,
                               const VFromD<Simd<uint64_t, N, 2>> v) {
  const size_t avl = Lanes(d);
  const vuint64m4_t v1 = vand(v, 0xFFFF, avl);
  const vuint32m2_t v2 = vnclipu_wx_u32m2(v1, 0, avl);
  return vnclipu_wx_u16m1(v2, 0, avl);
}

template <size_t N>
HWY_API vuint16m2_t TruncateTo(Simd<uint16_t, N, 1> d,
                               const VFromD<Simd<uint64_t, N, 3>> v) {
  const size_t avl = Lanes(d);
  const vuint64m8_t v1 = vand(v, 0xFFFF, avl);
  const vuint32m4_t v2 = vnclipu_wx_u32m4(v1, 0, avl);
  return vnclipu_wx_u16m2(v2, 0, avl);
}

template <size_t N>
HWY_API vuint32mf2_t TruncateTo(Simd<uint32_t, N, -1> d,
                                const VFromD<Simd<uint64_t, N, 0>> v) {
  const size_t avl = Lanes(d);
  const vuint64m1_t v1 = vand(v, 0xFFFFFFFFu, avl);
  return vnclipu_wx_u32mf2(v1, 0, avl);
}

template <size_t N>
HWY_API vuint32m1_t TruncateTo(Simd<uint32_t, N, 0> d,
                               const VFromD<Simd<uint64_t, N, 1>> v) {
  const size_t avl = Lanes(d);
  const vuint64m2_t v1 = vand(v, 0xFFFFFFFFu, avl);
  return vnclipu_wx_u32m1(v1, 0, avl);
}

template <size_t N>
HWY_API vuint32m2_t TruncateTo(Simd<uint32_t, N, 1> d,
                               const VFromD<Simd<uint64_t, N, 2>> v) {
  const size_t avl = Lanes(d);
  const vuint64m4_t v1 = vand(v, 0xFFFFFFFFu, avl);
  return vnclipu_wx_u32m2(v1, 0, avl);
}

template <size_t N>
HWY_API vuint32m4_t TruncateTo(Simd<uint32_t, N, 2> d,
                               const VFromD<Simd<uint64_t, N, 3>> v) {
  const size_t avl = Lanes(d);
  const vuint64m8_t v1 = vand(v, 0xFFFFFFFFu, avl);
  return vnclipu_wx_u32m4(v1, 0, avl);
}

template <size_t N>
HWY_API vuint8mf8_t TruncateTo(Simd<uint8_t, N, -3> d,
                               const VFromD<Simd<uint32_t, N, -1>> v) {
  const size_t avl = Lanes(d);
  const vuint32mf2_t v1 = vand(v, 0xFF, avl);
  const vuint16mf4_t v2 = vnclipu_wx_u16mf4(v1, 0, avl);
  return vnclipu_wx_u8mf8(v2, 0, avl);
}

template <size_t N>
HWY_API vuint8mf4_t TruncateTo(Simd<uint8_t, N, -2> d,
                               const VFromD<Simd<uint32_t, N, 0>> v) {
  const size_t avl = Lanes(d);
  const vuint32m1_t v1 = vand(v, 0xFF, avl);
  const vuint16mf2_t v2 = vnclipu_wx_u16mf2(v1, 0, avl);
  return vnclipu_wx_u8mf4(v2, 0, avl);
}

template <size_t N>
HWY_API vuint8mf2_t TruncateTo(Simd<uint8_t, N, -1> d,
                               const VFromD<Simd<uint32_t, N, 1>> v) {
  const size_t avl = Lanes(d);
  const vuint32m2_t v1 = vand(v, 0xFF, avl);
  const vuint16m1_t v2 = vnclipu_wx_u16m1(v1, 0, avl);
  return vnclipu_wx_u8mf2(v2, 0, avl);
}

template <size_t N>
HWY_API vuint8m1_t TruncateTo(Simd<uint8_t, N, 0> d,
                              const VFromD<Simd<uint32_t, N, 2>> v) {
  const size_t avl = Lanes(d);
  const vuint32m4_t v1 = vand(v, 0xFF, avl);
  const vuint16m2_t v2 = vnclipu_wx_u16m2(v1, 0, avl);
  return vnclipu_wx_u8m1(v2, 0, avl);
}

template <size_t N>
HWY_API vuint8m2_t TruncateTo(Simd<uint8_t, N, 1> d,
                              const VFromD<Simd<uint32_t, N, 3>> v) {
  const size_t avl = Lanes(d);
  const vuint32m8_t v1 = vand(v, 0xFF, avl);
  const vuint16m4_t v2 = vnclipu_wx_u16m4(v1, 0, avl);
  return vnclipu_wx_u8m2(v2, 0, avl);
}

template <size_t N>
HWY_API vuint16mf4_t TruncateTo(Simd<uint16_t, N, -2> d,
                                const VFromD<Simd<uint32_t, N, -1>> v) {
  const size_t avl = Lanes(d);
  const vuint32mf2_t v1 = vand(v, 0xFFFF, avl);
  return vnclipu_wx_u16mf4(v1, 0, avl);
}

template <size_t N>
HWY_API vuint16mf2_t TruncateTo(Simd<uint16_t, N, -1> d,
                                const VFromD<Simd<uint32_t, N, 0>> v) {
  const size_t avl = Lanes(d);
  const vuint32m1_t v1 = vand(v, 0xFFFF, avl);
  return vnclipu_wx_u16mf2(v1, 0, avl);
}

template <size_t N>
HWY_API vuint16m1_t TruncateTo(Simd<uint16_t, N, 0> d,
                               const VFromD<Simd<uint32_t, N, 1>> v) {
  const size_t avl = Lanes(d);
  const vuint32m2_t v1 = vand(v, 0xFFFF, avl);
  return vnclipu_wx_u16m1(v1, 0, avl);
}

template <size_t N>
HWY_API vuint16m2_t TruncateTo(Simd<uint16_t, N, 1> d,
                               const VFromD<Simd<uint32_t, N, 2>> v) {
  const size_t avl = Lanes(d);
  const vuint32m4_t v1 = vand(v, 0xFFFF, avl);
  return vnclipu_wx_u16m2(v1, 0, avl);
}

template <size_t N>
HWY_API vuint16m4_t TruncateTo(Simd<uint16_t, N, 2> d,
                               const VFromD<Simd<uint32_t, N, 3>> v) {
  const size_t avl = Lanes(d);
  const vuint32m8_t v1 = vand(v, 0xFFFF, avl);
  return vnclipu_wx_u16m4(v1, 0, avl);
}

template <size_t N>
HWY_API vuint8mf8_t TruncateTo(Simd<uint8_t, N, -3> d,
                               const VFromD<Simd<uint16_t, N, -2>> v) {
  const size_t avl = Lanes(d);
  const vuint16mf4_t v1 = vand(v, 0xFF, avl);
  return vnclipu_wx_u8mf8(v1, 0, avl);
}

template <size_t N>
HWY_API vuint8mf4_t TruncateTo(Simd<uint8_t, N, -2> d,
                               const VFromD<Simd<uint16_t, N, -1>> v) {
  const size_t avl = Lanes(d);
  const vuint16mf2_t v1 = vand(v, 0xFF, avl);
  return vnclipu_wx_u8mf4(v1, 0, avl);
}

template <size_t N>
HWY_API vuint8mf2_t TruncateTo(Simd<uint8_t, N, -1> d,
                               const VFromD<Simd<uint16_t, N, 0>> v) {
  const size_t avl = Lanes(d);
  const vuint16m1_t v1 = vand(v, 0xFF, avl);
  return vnclipu_wx_u8mf2(v1, 0, avl);
}

template <size_t N>
HWY_API vuint8m1_t TruncateTo(Simd<uint8_t, N, 0> d,
                              const VFromD<Simd<uint16_t, N, 1>> v) {
  const size_t avl = Lanes(d);
  const vuint16m2_t v1 = vand(v, 0xFF, avl);
  return vnclipu_wx_u8m1(v1, 0, avl);
}

template <size_t N>
HWY_API vuint8m2_t TruncateTo(Simd<uint8_t, N, 1> d,
                              const VFromD<Simd<uint16_t, N, 2>> v) {
  const size_t avl = Lanes(d);
  const vuint16m4_t v1 = vand(v, 0xFF, avl);
  return vnclipu_wx_u8m2(v1, 0, avl);
}

template <size_t N>
HWY_API vuint8m4_t TruncateTo(Simd<uint8_t, N, 2> d,
                              const VFromD<Simd<uint16_t, N, 3>> v) {
  const size_t avl = Lanes(d);
  const vuint16m8_t v1 = vand(v, 0xFF, avl);
  return vnclipu_wx_u8m4(v1, 0, avl);
}

// ------------------------------ DemoteTo I

HWY_RVV_FOREACH_I16(HWY_RVV_DEMOTE, DemoteTo, vnclip_wx_, _DEMOTE_VIRT)
HWY_RVV_FOREACH_I32(HWY_RVV_DEMOTE, DemoteTo, vnclip_wx_, _DEMOTE_VIRT)

template <size_t N>
HWY_API vint8mf8_t DemoteTo(Simd<int8_t, N, -3> d, const vint32mf2_t v) {
  return DemoteTo(d, DemoteTo(Simd<int16_t, N, -2>(), v));
}
template <size_t N>
HWY_API vint8mf4_t DemoteTo(Simd<int8_t, N, -2> d, const vint32m1_t v) {
  return DemoteTo(d, DemoteTo(Simd<int16_t, N, -1>(), v));
}
template <size_t N>
HWY_API vint8mf2_t DemoteTo(Simd<int8_t, N, -1> d, const vint32m2_t v) {
  return DemoteTo(d, DemoteTo(Simd<int16_t, N, 0>(), v));
}
template <size_t N>
HWY_API vint8m1_t DemoteTo(Simd<int8_t, N, 0> d, const vint32m4_t v) {
  return DemoteTo(d, DemoteTo(Simd<int16_t, N, 1>(), v));
}
template <size_t N>
HWY_API vint8m2_t DemoteTo(Simd<int8_t, N, 1> d, const vint32m8_t v) {
  return DemoteTo(d, DemoteTo(Simd<int16_t, N, 2>(), v));
}

#undef HWY_RVV_DEMOTE

// ------------------------------ DemoteTo F

// SEW is for the source so we can use _DEMOTE.
#define HWY_RVV_DEMOTE_F(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH,    \
                         SHIFT, MLEN, NAME, OP)                              \
  template <size_t N>                                                        \
  HWY_API HWY_RVV_V(BASE, SEWH, LMULH) NAME(                                 \
      HWY_RVV_D(BASE, SEWH, N, SHIFT - 1) d, HWY_RVV_V(BASE, SEW, LMUL) v) { \
    return OP##SEWH##LMULH(v, Lanes(d));                                     \
  }

#if HWY_HAVE_FLOAT16
HWY_RVV_FOREACH_F32(HWY_RVV_DEMOTE_F, DemoteTo, vfncvt_rod_f_f_w_f,
                    _DEMOTE_VIRT)
#endif
HWY_RVV_FOREACH_F64(HWY_RVV_DEMOTE_F, DemoteTo, vfncvt_rod_f_f_w_f,
                    _DEMOTE_VIRT)
#undef HWY_RVV_DEMOTE_F

// TODO(janwas): add BASE2 arg to allow generating this via DEMOTE_F.
template <size_t N>
HWY_API vint32mf2_t DemoteTo(Simd<int32_t, N, -2> d, const vfloat64m1_t v) {
  return vfncvt_rtz_x_f_w_i32mf2(v, Lanes(d));
}
template <size_t N>
HWY_API vint32mf2_t DemoteTo(Simd<int32_t, N, -1> d, const vfloat64m1_t v) {
  return vfncvt_rtz_x_f_w_i32mf2(v, Lanes(d));
}
template <size_t N>
HWY_API vint32m1_t DemoteTo(Simd<int32_t, N, 0> d, const vfloat64m2_t v) {
  return vfncvt_rtz_x_f_w_i32m1(v, Lanes(d));
}
template <size_t N>
HWY_API vint32m2_t DemoteTo(Simd<int32_t, N, 1> d, const vfloat64m4_t v) {
  return vfncvt_rtz_x_f_w_i32m2(v, Lanes(d));
}
template <size_t N>
HWY_API vint32m4_t DemoteTo(Simd<int32_t, N, 2> d, const vfloat64m8_t v) {
  return vfncvt_rtz_x_f_w_i32m4(v, Lanes(d));
}

template <size_t N, int kPow2>
HWY_API VFromD<Simd<uint16_t, N, kPow2>> DemoteTo(
    Simd<bfloat16_t, N, kPow2> d, VFromD<Simd<float, N, kPow2 + 1>> v) {
  const RebindToUnsigned<decltype(d)> du16;
  const Rebind<uint32_t, decltype(d)> du32;
  return detail::DemoteToShr16(du16, BitCast(du32, v));
}

// ------------------------------ ConvertTo F

#define HWY_RVV_CONVERT(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH,       \
                        SHIFT, MLEN, NAME, OP)                                 \
  template <size_t N>                                                          \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) ConvertTo(                                \
      HWY_RVV_D(BASE, SEW, N, SHIFT) d, HWY_RVV_V(int, SEW, LMUL) v) {         \
    return vfcvt_f_x_v_f##SEW##LMUL(v, Lanes(d));                              \
  }                                                                            \
  /* Truncates (rounds toward zero). */                                        \
  template <size_t N>                                                          \
  HWY_API HWY_RVV_V(int, SEW, LMUL) ConvertTo(HWY_RVV_D(int, SEW, N, SHIFT) d, \
                                              HWY_RVV_V(BASE, SEW, LMUL) v) {  \
    return vfcvt_rtz_x_f_v_i##SEW##LMUL(v, Lanes(d));                          \
  }                                                                            \
// API only requires f32 but we provide f64 for internal use.
HWY_RVV_FOREACH_F(HWY_RVV_CONVERT, _, _, _ALL_VIRT)
#undef HWY_RVV_CONVERT

// Uses default rounding mode. Must be separate because there is no D arg.
#define HWY_RVV_NEAREST(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH,       \
                        SHIFT, MLEN, NAME, OP)                                 \
  HWY_API HWY_RVV_V(int, SEW, LMUL) NearestInt(HWY_RVV_V(BASE, SEW, LMUL) v) { \
    return vfcvt_x_f_v_i##SEW##LMUL(v, HWY_RVV_AVL(SEW, SHIFT));               \
  }
HWY_RVV_FOREACH_F(HWY_RVV_NEAREST, _, _, _ALL)
#undef HWY_RVV_NEAREST

// ================================================== COMBINE

namespace detail {

// For x86-compatible behaviour mandated by Highway API: TableLookupBytes
// offsets are implicitly relative to the start of their 128-bit block.
template <typename T, size_t N, int kPow2>
size_t LanesPerBlock(Simd<T, N, kPow2> d) {
  size_t lpb = 16 / sizeof(T);
  if (IsFull(d)) return lpb;
  // Also honor the user-specified (constexpr) N limit.
  lpb = HWY_MIN(lpb, N);
  // No fraction, we're done.
  if (kPow2 >= 0) return lpb;
  // Fractional LMUL: Lanes(d) may be smaller than lpb, so honor that.
  return HWY_MIN(lpb, Lanes(d));
}

template <class D, class V>
HWY_INLINE V OffsetsOf128BitBlocks(const D d, const V iota0) {
  using T = MakeUnsigned<TFromD<D>>;
  return AndS(iota0, static_cast<T>(~(LanesPerBlock(d) - 1)));
}

template <size_t kLanes, class D>
HWY_INLINE MFromD<D> FirstNPerBlock(D /* tag */) {
  const RebindToUnsigned<D> du;
  const RebindToSigned<D> di;
  using TU = TFromD<decltype(du)>;
  const auto idx_mod = AndS(Iota0(du), static_cast<TU>(LanesPerBlock(du) - 1));
  return LtS(BitCast(di, idx_mod), static_cast<TFromD<decltype(di)>>(kLanes));
}

// vector = f(vector, vector, size_t)
#define HWY_RVV_SLIDE(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT, \
                      MLEN, NAME, OP)                                         \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                          \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) dst, HWY_RVV_V(BASE, SEW, LMUL) src,    \
           size_t lanes) {                                                    \
    return v##OP##_vx_##CHAR##SEW##LMUL(dst, src, lanes,                      \
                                        HWY_RVV_AVL(SEW, SHIFT));             \
  }

HWY_RVV_FOREACH(HWY_RVV_SLIDE, SlideUp, slideup, _ALL)
HWY_RVV_FOREACH(HWY_RVV_SLIDE, SlideDown, slidedown, _ALL)

#undef HWY_RVV_SLIDE

}  // namespace detail

// ------------------------------ ConcatUpperLower
template <class D, class V>
HWY_API V ConcatUpperLower(D d, const V hi, const V lo) {
  return IfThenElse(FirstN(d, Lanes(d) / 2), lo, hi);
}

// ------------------------------ ConcatLowerLower
template <class D, class V>
HWY_API V ConcatLowerLower(D d, const V hi, const V lo) {
  return detail::SlideUp(lo, hi, Lanes(d) / 2);
}

// ------------------------------ ConcatUpperUpper
template <class D, class V>
HWY_API V ConcatUpperUpper(D d, const V hi, const V lo) {
  // Move upper half into lower
  const auto lo_down = detail::SlideDown(lo, lo, Lanes(d) / 2);
  return ConcatUpperLower(d, hi, lo_down);
}

// ------------------------------ ConcatLowerUpper
template <class D, class V>
HWY_API V ConcatLowerUpper(D d, const V hi, const V lo) {
  // Move half of both inputs to the other half
  const auto hi_up = detail::SlideUp(hi, hi, Lanes(d) / 2);
  const auto lo_down = detail::SlideDown(lo, lo, Lanes(d) / 2);
  return ConcatUpperLower(d, hi_up, lo_down);
}

// ------------------------------ Combine
template <class D2, class V>
HWY_API VFromD<D2> Combine(D2 d2, const V hi, const V lo) {
  return detail::SlideUp(detail::Ext(d2, lo), detail::Ext(d2, hi),
                         Lanes(d2) / 2);
}

// ------------------------------ ZeroExtendVector

template <class D2, class V>
HWY_API VFromD<D2> ZeroExtendVector(D2 d2, const V lo) {
  return Combine(d2, Xor(lo, lo), lo);
}

// ------------------------------ Lower/UpperHalf

namespace detail {

// RVV may only support LMUL >= SEW/64; returns whether that holds for D. Note
// that SEW = sizeof(T)*8 and LMUL = 1 << Pow2().
template <class D>
constexpr bool IsSupportedLMUL(D d) {
  return (size_t{1} << (Pow2(d) + 3)) >= sizeof(TFromD<D>);
}

}  // namespace detail

// If IsSupportedLMUL, just 'truncate' i.e. halve LMUL.
template <class DH, hwy::EnableIf<detail::IsSupportedLMUL(DH())>* = nullptr>
HWY_API VFromD<DH> LowerHalf(const DH /* tag */, const VFromD<Twice<DH>> v) {
  return detail::Trunc(v);
}

// Otherwise, there is no corresponding intrinsic type (e.g. vuint64mf2_t), and
// the hardware may set "vill" if we attempt such an LMUL. However, the V
// extension on application processors requires Zvl128b, i.e. VLEN >= 128, so it
// still makes sense to have half of an SEW=64 vector. We instead just return
// the vector, and rely on the kPow2 in DH to halve the return value of Lanes().
template <class DH, class V,
          hwy::EnableIf<!detail::IsSupportedLMUL(DH())>* = nullptr>
HWY_API V LowerHalf(const DH /* tag */, const V v) {
  return v;
}

// Same, but without D arg
template <class V>
HWY_API VFromD<Half<DFromV<V>>> LowerHalf(const V v) {
  return LowerHalf(Half<DFromV<V>>(), v);
}

template <class DH>
HWY_API VFromD<DH> UpperHalf(const DH d2, const VFromD<Twice<DH>> v) {
  return LowerHalf(d2, detail::SlideDown(v, v, Lanes(d2)));
}

// ================================================== SWIZZLE

namespace detail {
// Special instruction for 1 lane is presumably faster?
#define HWY_RVV_SLIDE1(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT, \
                       MLEN, NAME, OP)                                         \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) NAME(HWY_RVV_V(BASE, SEW, LMUL) v) {      \
    return v##OP##_##CHAR##SEW##LMUL(v, 0, HWY_RVV_AVL(SEW, SHIFT));           \
  }

HWY_RVV_FOREACH_UI3264(HWY_RVV_SLIDE1, Slide1Up, slide1up_vx, _ALL)
HWY_RVV_FOREACH_F3264(HWY_RVV_SLIDE1, Slide1Up, fslide1up_vf, _ALL)
HWY_RVV_FOREACH_UI3264(HWY_RVV_SLIDE1, Slide1Down, slide1down_vx, _ALL)
HWY_RVV_FOREACH_F3264(HWY_RVV_SLIDE1, Slide1Down, fslide1down_vf, _ALL)
#undef HWY_RVV_SLIDE1
}  // namespace detail

// ------------------------------ GetLane

#define HWY_RVV_GET_LANE(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, \
                         SHIFT, MLEN, NAME, OP)                           \
  HWY_API HWY_RVV_T(BASE, SEW) NAME(HWY_RVV_V(BASE, SEW, LMUL) v) {       \
    return v##OP##_s_##CHAR##SEW##LMUL##_##CHAR##SEW(v); /* no AVL */     \
  }

HWY_RVV_FOREACH_UI(HWY_RVV_GET_LANE, GetLane, mv_x, _ALL)
HWY_RVV_FOREACH_F(HWY_RVV_GET_LANE, GetLane, fmv_f, _ALL)
#undef HWY_RVV_GET_LANE

// ------------------------------ ExtractLane
template <class V>
HWY_API TFromV<V> ExtractLane(const V v, size_t i) {
  return GetLane(detail::SlideDown(v, v, i));
}

// ------------------------------ InsertLane

template <class V, HWY_IF_NOT_LANE_SIZE_V(V, 1)>
HWY_API V InsertLane(const V v, size_t i, TFromV<V> t) {
  const DFromV<V> d;
  const RebindToUnsigned<decltype(d)> du;  // Iota0 is unsigned only
  using TU = TFromD<decltype(du)>;
  const auto is_i = detail::EqS(detail::Iota0(du), static_cast<TU>(i));
  return IfThenElse(RebindMask(d, is_i), Set(d, t), v);
}

namespace detail {
HWY_RVV_FOREACH_B(HWY_RVV_RETM_ARGM, SetOnlyFirst, sof)
}  // namespace detail

// For 8-bit lanes, Iota0 might overflow.
template <class V, HWY_IF_LANE_SIZE_V(V, 1)>
HWY_API V InsertLane(const V v, size_t i, TFromV<V> t) {
  const DFromV<V> d;
  const auto zero = Zero(d);
  const auto one = Set(d, 1);
  const auto ge_i = Eq(detail::SlideUp(zero, one, i), one);
  const auto is_i = detail::SetOnlyFirst(ge_i);
  return IfThenElse(RebindMask(d, is_i), Set(d, t), v);
}

// ------------------------------ OddEven
template <class V>
HWY_API V OddEven(const V a, const V b) {
  const RebindToUnsigned<DFromV<V>> du;  // Iota0 is unsigned only
  const auto is_even = detail::EqS(detail::AndS(detail::Iota0(du), 1), 0);
  return IfThenElse(is_even, b, a);
}

// ------------------------------ DupEven (OddEven)
template <class V>
HWY_API V DupEven(const V v) {
  const V up = detail::Slide1Up(v);
  return OddEven(up, v);
}

// ------------------------------ DupOdd (OddEven)
template <class V>
HWY_API V DupOdd(const V v) {
  const V down = detail::Slide1Down(v);
  return OddEven(v, down);
}

// ------------------------------ OddEvenBlocks
template <class V>
HWY_API V OddEvenBlocks(const V a, const V b) {
  const RebindToUnsigned<DFromV<V>> du;  // Iota0 is unsigned only
  constexpr size_t kShift = CeilLog2(16 / sizeof(TFromV<V>));
  const auto idx_block = ShiftRight<kShift>(detail::Iota0(du));
  const auto is_even = detail::EqS(detail::AndS(idx_block, 1), 0);
  return IfThenElse(is_even, b, a);
}

// ------------------------------ SwapAdjacentBlocks

template <class V>
HWY_API V SwapAdjacentBlocks(const V v) {
  const DFromV<V> d;
  const size_t lpb = detail::LanesPerBlock(d);
  const V down = detail::SlideDown(v, v, lpb);
  const V up = detail::SlideUp(v, v, lpb);
  return OddEvenBlocks(up, down);
}

// ------------------------------ TableLookupLanes

template <class D, class VI>
HWY_API VFromD<RebindToUnsigned<D>> IndicesFromVec(D d, VI vec) {
  static_assert(sizeof(TFromD<D>) == sizeof(TFromV<VI>), "Index != lane");
  const RebindToUnsigned<decltype(d)> du;  // instead of <D>: avoids unused d.
  const auto indices = BitCast(du, vec);
#if HWY_IS_DEBUG_BUILD
  HWY_DASSERT(AllTrue(du, detail::LtS(indices, Lanes(d))));
#endif
  return indices;
}

template <class D, typename TI>
HWY_API VFromD<RebindToUnsigned<D>> SetTableIndices(D d, const TI* idx) {
  static_assert(sizeof(TFromD<D>) == sizeof(TI), "Index size must match lane");
  return IndicesFromVec(d, LoadU(Rebind<TI, D>(), idx));
}

// <32bit are not part of Highway API, but used in Broadcast. This limits VLMAX
// to 2048! We could instead use vrgatherei16.
#define HWY_RVV_TABLE(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT, \
                      MLEN, NAME, OP)                                         \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                          \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) v, HWY_RVV_V(uint, SEW, LMUL) idx) {    \
    return v##OP##_vv_##CHAR##SEW##LMUL(v, idx, HWY_RVV_AVL(SEW, SHIFT));     \
  }

HWY_RVV_FOREACH(HWY_RVV_TABLE, TableLookupLanes, rgather, _ALL)
#undef HWY_RVV_TABLE

// ------------------------------ ConcatOdd (TableLookupLanes)
template <class D, class V>
HWY_API V ConcatOdd(D d, const V hi, const V lo) {
  const RebindToUnsigned<decltype(d)> du;  // Iota0 is unsigned only
  const auto iota = detail::Iota0(du);
  const auto idx = detail::AddS(Add(iota, iota), 1);
  const auto lo_odd = TableLookupLanes(lo, idx);
  const auto hi_odd = TableLookupLanes(hi, idx);
  return detail::SlideUp(lo_odd, hi_odd, Lanes(d) / 2);
}

// ------------------------------ ConcatEven (TableLookupLanes)
template <class D, class V>
HWY_API V ConcatEven(D d, const V hi, const V lo) {
  const RebindToUnsigned<decltype(d)> du;  // Iota0 is unsigned only
  const auto iota = detail::Iota0(du);
  const auto idx = Add(iota, iota);
  const auto lo_even = TableLookupLanes(lo, idx);
  const auto hi_even = TableLookupLanes(hi, idx);
  return detail::SlideUp(lo_even, hi_even, Lanes(d) / 2);
}

// ------------------------------ Reverse (TableLookupLanes)
template <class D>
HWY_API VFromD<D> Reverse(D /* tag */, VFromD<D> v) {
  const RebindToUnsigned<D> du;
  using TU = TFromD<decltype(du)>;
  const size_t N = Lanes(du);
  const auto idx =
      detail::ReverseSubS(detail::Iota0(du), static_cast<TU>(N - 1));
  return TableLookupLanes(v, idx);
}

// ------------------------------ Reverse2 (RotateRight, OddEven)

// Shifting and adding requires fewer instructions than blending, but casting to
// u32 only works for LMUL in [1/2, 8].
template <class D, HWY_IF_LANE_SIZE_D(D, 2), HWY_RVV_IF_POW2_IN(D, -1, 3)>
HWY_API VFromD<D> Reverse2(D d, const VFromD<D> v) {
  const Repartition<uint32_t, D> du32;
  return BitCast(d, RotateRight<16>(BitCast(du32, v)));
}
// For LMUL < 1/2, we can extend and then truncate.
template <class D, HWY_IF_LANE_SIZE_D(D, 2), HWY_RVV_IF_POW2_IN(D, -3, -2)>
HWY_API VFromD<D> Reverse2(D d, const VFromD<D> v) {
  const Twice<decltype(d)> d2;
  const Twice<decltype(d2)> d4;
  const Repartition<uint32_t, decltype(d4)> du32;
  const auto vx = detail::Ext(d4, detail::Ext(d2, v));
  const auto rx = BitCast(d4, RotateRight<16>(BitCast(du32, vx)));
  return detail::Trunc(detail::Trunc(rx));
}

// Shifting and adding requires fewer instructions than blending, but casting to
// u64 does not work for LMUL < 1.
template <class D, HWY_IF_LANE_SIZE_D(D, 4), HWY_RVV_IF_POW2_IN(D, 0, 3)>
HWY_API VFromD<D> Reverse2(D d, const VFromD<D> v) {
  const Repartition<uint64_t, decltype(d)> du64;
  return BitCast(d, RotateRight<32>(BitCast(du64, v)));
}

// For fractions, we can extend and then truncate.
template <class D, HWY_IF_LANE_SIZE_D(D, 4), HWY_RVV_IF_POW2_IN(D, -2, -1)>
HWY_API VFromD<D> Reverse2(D d, const VFromD<D> v) {
  const Twice<decltype(d)> d2;
  const Twice<decltype(d2)> d4;
  const Repartition<uint64_t, decltype(d4)> du64;
  const auto vx = detail::Ext(d4, detail::Ext(d2, v));
  const auto rx = BitCast(d4, RotateRight<32>(BitCast(du64, vx)));
  return detail::Trunc(detail::Trunc(rx));
}

template <class D, class V = VFromD<D>, HWY_IF_LANE_SIZE_D(D, 8)>
HWY_API V Reverse2(D /* tag */, const V v) {
  const V up = detail::Slide1Up(v);
  const V down = detail::Slide1Down(v);
  return OddEven(up, down);
}

// ------------------------------ Reverse4 (TableLookupLanes)

template <class D>
HWY_API VFromD<D> Reverse4(D d, const VFromD<D> v) {
  const RebindToUnsigned<D> du;
  const auto idx = detail::XorS(detail::Iota0(du), 3);
  return BitCast(d, TableLookupLanes(BitCast(du, v), idx));
}

// ------------------------------ Reverse8 (TableLookupLanes)

template <class D>
HWY_API VFromD<D> Reverse8(D d, const VFromD<D> v) {
  const RebindToUnsigned<D> du;
  const auto idx = detail::XorS(detail::Iota0(du), 7);
  return BitCast(d, TableLookupLanes(BitCast(du, v), idx));
}

// ------------------------------ ReverseBlocks (Reverse, Shuffle01)
template <class D, class V = VFromD<D>>
HWY_API V ReverseBlocks(D d, V v) {
  const Repartition<uint64_t, D> du64;
  const size_t N = Lanes(du64);
  const auto rev =
      detail::ReverseSubS(detail::Iota0(du64), static_cast<uint64_t>(N - 1));
  // Swap lo/hi u64 within each block
  const auto idx = detail::XorS(rev, 1);
  return BitCast(d, TableLookupLanes(BitCast(du64, v), idx));
}

// ------------------------------ Compress

template <typename T>
struct CompressIsPartition {
  enum { value = 0 };
};

#define HWY_RVV_COMPRESS(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH,     \
                         SHIFT, MLEN, NAME, OP)                               \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                          \
      NAME(HWY_RVV_V(BASE, SEW, LMUL) v, HWY_RVV_M(MLEN) mask) {              \
    return v##OP##_vm_##CHAR##SEW##LMUL(mask, v, v, HWY_RVV_AVL(SEW, SHIFT)); \
  }

HWY_RVV_FOREACH_UI163264(HWY_RVV_COMPRESS, Compress, compress, _ALL)
HWY_RVV_FOREACH_F(HWY_RVV_COMPRESS, Compress, compress, _ALL)
#undef HWY_RVV_COMPRESS

// ------------------------------ CompressNot
template <class V, class M>
HWY_API V CompressNot(V v, const M mask) {
  return Compress(v, Not(mask));
}

// ------------------------------ CompressBlocksNot
template <class V, class M>
HWY_API V CompressBlocksNot(V v, const M mask) {
  return CompressNot(v, mask);
}

// ------------------------------ CompressStore
template <class V, class M, class D>
HWY_API size_t CompressStore(const V v, const M mask, const D d,
                             TFromD<D>* HWY_RESTRICT unaligned) {
  StoreU(Compress(v, mask), d, unaligned);
  return CountTrue(d, mask);
}

// ------------------------------ CompressBlendedStore
template <class V, class M, class D>
HWY_API size_t CompressBlendedStore(const V v, const M mask, const D d,
                                    TFromD<D>* HWY_RESTRICT unaligned) {
  const size_t count = CountTrue(d, mask);
  detail::StoreN(count, Compress(v, mask), d, unaligned);
  return count;
}

// ================================================== BLOCKWISE

// ------------------------------ CombineShiftRightBytes
template <size_t kBytes, class D, class V = VFromD<D>>
HWY_API V CombineShiftRightBytes(const D d, const V hi, V lo) {
  const Repartition<uint8_t, decltype(d)> d8;
  const auto hi8 = BitCast(d8, hi);
  const auto lo8 = BitCast(d8, lo);
  const auto hi_up = detail::SlideUp(hi8, hi8, 16 - kBytes);
  const auto lo_down = detail::SlideDown(lo8, lo8, kBytes);
  const auto is_lo = detail::FirstNPerBlock<16 - kBytes>(d8);
  return BitCast(d, IfThenElse(is_lo, lo_down, hi_up));
}

// ------------------------------ CombineShiftRightLanes
template <size_t kLanes, class D, class V = VFromD<D>>
HWY_API V CombineShiftRightLanes(const D d, const V hi, V lo) {
  constexpr size_t kLanesUp = 16 / sizeof(TFromV<V>) - kLanes;
  const auto hi_up = detail::SlideUp(hi, hi, kLanesUp);
  const auto lo_down = detail::SlideDown(lo, lo, kLanes);
  const auto is_lo = detail::FirstNPerBlock<kLanesUp>(d);
  return IfThenElse(is_lo, lo_down, hi_up);
}

// ------------------------------ Shuffle2301 (ShiftLeft)
template <class V>
HWY_API V Shuffle2301(const V v) {
  const DFromV<V> d;
  static_assert(sizeof(TFromD<decltype(d)>) == 4, "Defined for 32-bit types");
  const Repartition<uint64_t, decltype(d)> du64;
  const auto v64 = BitCast(du64, v);
  return BitCast(d, Or(ShiftRight<32>(v64), ShiftLeft<32>(v64)));
}

// ------------------------------ Shuffle2103
template <class V>
HWY_API V Shuffle2103(const V v) {
  const DFromV<V> d;
  static_assert(sizeof(TFromD<decltype(d)>) == 4, "Defined for 32-bit types");
  return CombineShiftRightLanes<3>(d, v, v);
}

// ------------------------------ Shuffle0321
template <class V>
HWY_API V Shuffle0321(const V v) {
  const DFromV<V> d;
  static_assert(sizeof(TFromD<decltype(d)>) == 4, "Defined for 32-bit types");
  return CombineShiftRightLanes<1>(d, v, v);
}

// ------------------------------ Shuffle1032
template <class V>
HWY_API V Shuffle1032(const V v) {
  const DFromV<V> d;
  static_assert(sizeof(TFromD<decltype(d)>) == 4, "Defined for 32-bit types");
  return CombineShiftRightLanes<2>(d, v, v);
}

// ------------------------------ Shuffle01
template <class V>
HWY_API V Shuffle01(const V v) {
  const DFromV<V> d;
  static_assert(sizeof(TFromD<decltype(d)>) == 8, "Defined for 64-bit types");
  return CombineShiftRightLanes<1>(d, v, v);
}

// ------------------------------ Shuffle0123
template <class V>
HWY_API V Shuffle0123(const V v) {
  return Shuffle2301(Shuffle1032(v));
}

// ------------------------------ TableLookupBytes

// Extends or truncates a vector to match the given d.
namespace detail {

template <typename T, size_t N, int kPow2>
HWY_INLINE auto ChangeLMUL(Simd<T, N, kPow2> d, VFromD<Simd<T, N, kPow2 - 3>> v)
    -> VFromD<decltype(d)> {
  const Simd<T, N, kPow2 - 1> dh;
  const Simd<T, N, kPow2 - 2> dhh;
  return Ext(d, Ext(dh, Ext(dhh, v)));
}
template <typename T, size_t N, int kPow2>
HWY_INLINE auto ChangeLMUL(Simd<T, N, kPow2> d, VFromD<Simd<T, N, kPow2 - 2>> v)
    -> VFromD<decltype(d)> {
  const Simd<T, N, kPow2 - 1> dh;
  return Ext(d, Ext(dh, v));
}
template <typename T, size_t N, int kPow2>
HWY_INLINE auto ChangeLMUL(Simd<T, N, kPow2> d, VFromD<Simd<T, N, kPow2 - 1>> v)
    -> VFromD<decltype(d)> {
  return Ext(d, v);
}

template <typename T, size_t N, int kPow2>
HWY_INLINE auto ChangeLMUL(Simd<T, N, kPow2> d, VFromD<decltype(d)> v)
    -> VFromD<decltype(d)> {
  return v;
}

template <typename T, size_t N, int kPow2>
HWY_INLINE auto ChangeLMUL(Simd<T, N, kPow2> d, VFromD<Simd<T, N, kPow2 + 1>> v)
    -> VFromD<decltype(d)> {
  return Trunc(v);
}
template <typename T, size_t N, int kPow2>
HWY_INLINE auto ChangeLMUL(Simd<T, N, kPow2> d, VFromD<Simd<T, N, kPow2 + 2>> v)
    -> VFromD<decltype(d)> {
  return Trunc(Trunc(v));
}
template <typename T, size_t N, int kPow2>
HWY_INLINE auto ChangeLMUL(Simd<T, N, kPow2> d, VFromD<Simd<T, N, kPow2 + 3>> v)
    -> VFromD<decltype(d)> {
  return Trunc(Trunc(Trunc(v)));
}

}  // namespace detail

template <class VT, class VI>
HWY_API VI TableLookupBytes(const VT vt, const VI vi) {
  const DFromV<VT> dt;  // T=table, I=index.
  const DFromV<VI> di;
  const Repartition<uint8_t, decltype(dt)> dt8;
  const Repartition<uint8_t, decltype(di)> di8;
  // Required for producing half-vectors with table lookups from a full vector.
  // If we instead run at the LMUL of the index vector, lookups into the table
  // would be truncated. Thus we run at the larger of the two LMULs and truncate
  // the result vector to the original index LMUL.
  constexpr int kPow2T = Pow2(dt8);
  constexpr int kPow2I = Pow2(di8);
  const Simd<uint8_t, MaxLanes(di8), HWY_MAX(kPow2T, kPow2I)> dm8;  // m=max
  const auto vmt = detail::ChangeLMUL(dm8, BitCast(dt8, vt));
  const auto vmi = detail::ChangeLMUL(dm8, BitCast(di8, vi));
  auto offsets = detail::OffsetsOf128BitBlocks(dm8, detail::Iota0(dm8));
  // If the table is shorter, wrap around offsets so they do not reference
  // undefined lanes in the newly extended vmt.
  if (kPow2T < kPow2I) {
    offsets = detail::AndS(offsets, static_cast<uint8_t>(Lanes(dt8) - 1));
  }
  const auto out = TableLookupLanes(vmt, Add(vmi, offsets));
  return BitCast(di, detail::ChangeLMUL(di8, out));
}

template <class VT, class VI>
HWY_API VI TableLookupBytesOr0(const VT vt, const VI idx) {
  const DFromV<VI> di;
  const Repartition<int8_t, decltype(di)> di8;
  const auto idx8 = BitCast(di8, idx);
  const auto lookup = TableLookupBytes(vt, idx8);
  return BitCast(di, IfThenZeroElse(detail::LtS(idx8, 0), lookup));
}

// ------------------------------ Broadcast
template <int kLane, class V>
HWY_API V Broadcast(const V v) {
  const DFromV<V> d;
  HWY_DASSERT(0 <= kLane && kLane < detail::LanesPerBlock(d));
  auto idx = detail::OffsetsOf128BitBlocks(d, detail::Iota0(d));
  if (kLane != 0) {
    idx = detail::AddS(idx, kLane);
  }
  return TableLookupLanes(v, idx);
}

// ------------------------------ ShiftLeftLanes

template <size_t kLanes, class D, class V = VFromD<D>>
HWY_API V ShiftLeftLanes(const D d, const V v) {
  const RebindToSigned<decltype(d)> di;
  using TI = TFromD<decltype(di)>;
  const auto shifted = detail::SlideUp(v, v, kLanes);
  // Match x86 semantics by zeroing lower lanes in 128-bit blocks
  const auto idx_mod = detail::AndS(
      detail::Iota0(di), static_cast<TI>(detail::LanesPerBlock(di) - 1));
  const auto clear = detail::LtS(BitCast(di, idx_mod), static_cast<TI>(kLanes));
  return IfThenZeroElse(clear, shifted);
}

template <size_t kLanes, class V>
HWY_API V ShiftLeftLanes(const V v) {
  return ShiftLeftLanes<kLanes>(DFromV<V>(), v);
}

// ------------------------------ ShiftLeftBytes

template <int kBytes, class D>
HWY_API VFromD<D> ShiftLeftBytes(D d, const VFromD<D> v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftLeftLanes<kBytes>(BitCast(d8, v)));
}

template <int kBytes, class V>
HWY_API V ShiftLeftBytes(const V v) {
  return ShiftLeftBytes<kBytes>(DFromV<V>(), v);
}

// ------------------------------ ShiftRightLanes
template <size_t kLanes, typename T, size_t N, int kPow2,
          class V = VFromD<Simd<T, N, kPow2>>>
HWY_API V ShiftRightLanes(const Simd<T, N, kPow2> d, V v) {
  const RebindToSigned<decltype(d)> di;
  using TI = TFromD<decltype(di)>;
  // For partial vectors, clear upper lanes so we shift in zeros.
  if (N <= 16 / sizeof(T)) {
    v = IfThenElseZero(FirstN(d, N), v);
  }

  const auto shifted = detail::SlideDown(v, v, kLanes);
  // Match x86 semantics by zeroing upper lanes in 128-bit blocks
  const size_t lpb = detail::LanesPerBlock(di);
  const auto idx_mod =
      detail::AndS(detail::Iota0(di), static_cast<TI>(lpb - 1));
  const auto keep =
      detail::LtS(BitCast(di, idx_mod), static_cast<TI>(lpb - kLanes));
  return IfThenElseZero(keep, shifted);
}

// ------------------------------ ShiftRightBytes
template <int kBytes, class D, class V = VFromD<D>>
HWY_API V ShiftRightBytes(const D d, const V v) {
  const Repartition<uint8_t, decltype(d)> d8;
  return BitCast(d, ShiftRightLanes<kBytes>(d8, BitCast(d8, v)));
}

// ------------------------------ InterleaveLower

template <class D, class V>
HWY_API V InterleaveLower(D d, const V a, const V b) {
  static_assert(IsSame<TFromD<D>, TFromV<V>>(), "D/V mismatch");
  const RebindToUnsigned<decltype(d)> du;
  using TU = TFromD<decltype(du)>;
  const auto i = detail::Iota0(du);
  const auto idx_mod = ShiftRight<1>(
      detail::AndS(i, static_cast<TU>(detail::LanesPerBlock(du) - 1)));
  const auto idx = Add(idx_mod, detail::OffsetsOf128BitBlocks(d, i));
  const auto is_even = detail::EqS(detail::AndS(i, 1), 0u);
  return IfThenElse(is_even, TableLookupLanes(a, idx),
                    TableLookupLanes(b, idx));
}

template <class V>
HWY_API V InterleaveLower(const V a, const V b) {
  return InterleaveLower(DFromV<V>(), a, b);
}

// ------------------------------ InterleaveUpper

template <class D, class V>
HWY_API V InterleaveUpper(const D d, const V a, const V b) {
  static_assert(IsSame<TFromD<D>, TFromV<V>>(), "D/V mismatch");
  const RebindToUnsigned<decltype(d)> du;
  using TU = TFromD<decltype(du)>;
  const size_t lpb = detail::LanesPerBlock(du);
  const auto i = detail::Iota0(du);
  const auto idx_mod = ShiftRight<1>(detail::AndS(i, static_cast<TU>(lpb - 1)));
  const auto idx_lower = Add(idx_mod, detail::OffsetsOf128BitBlocks(d, i));
  const auto idx = detail::AddS(idx_lower, static_cast<TU>(lpb / 2));
  const auto is_even = detail::EqS(detail::AndS(i, 1), 0u);
  return IfThenElse(is_even, TableLookupLanes(a, idx),
                    TableLookupLanes(b, idx));
}

// ------------------------------ ZipLower

template <class V, class DW = RepartitionToWide<DFromV<V>>>
HWY_API VFromD<DW> ZipLower(DW dw, V a, V b) {
  const RepartitionToNarrow<DW> dn;
  static_assert(IsSame<TFromD<decltype(dn)>, TFromV<V>>(), "D/V mismatch");
  return BitCast(dw, InterleaveLower(dn, a, b));
}

template <class V, class DW = RepartitionToWide<DFromV<V>>>
HWY_API VFromD<DW> ZipLower(V a, V b) {
  return BitCast(DW(), InterleaveLower(a, b));
}

// ------------------------------ ZipUpper
template <class DW, class V>
HWY_API VFromD<DW> ZipUpper(DW dw, V a, V b) {
  const RepartitionToNarrow<DW> dn;
  static_assert(IsSame<TFromD<decltype(dn)>, TFromV<V>>(), "D/V mismatch");
  return BitCast(dw, InterleaveUpper(dn, a, b));
}

// ================================================== REDUCE

// vector = f(vector, zero_m1)
#define HWY_RVV_REDUCE(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, SHIFT, \
                       MLEN, NAME, OP)                                         \
  template <class D>                                                           \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL)                                           \
      NAME(D d, HWY_RVV_V(BASE, SEW, LMUL) v, HWY_RVV_V(BASE, SEW, m1) v0) {   \
    return Set(d, GetLane(v##OP##_vs_##CHAR##SEW##LMUL##_##CHAR##SEW##m1(      \
                      v0, v, v0, Lanes(d))));                                  \
  }

// ------------------------------ SumOfLanes

namespace detail {
HWY_RVV_FOREACH_UI(HWY_RVV_REDUCE, RedSum, redsum, _ALL)
HWY_RVV_FOREACH_F(HWY_RVV_REDUCE, RedSum, fredusum, _ALL)
}  // namespace detail

template <class D>
HWY_API VFromD<D> SumOfLanes(D d, const VFromD<D> v) {
  const auto v0 = Zero(ScalableTag<TFromD<D>>());  // always m1
  return detail::RedSum(d, v, v0);
}

// ------------------------------ MinOfLanes
namespace detail {
HWY_RVV_FOREACH_U(HWY_RVV_REDUCE, RedMin, redminu, _ALL)
HWY_RVV_FOREACH_I(HWY_RVV_REDUCE, RedMin, redmin, _ALL)
HWY_RVV_FOREACH_F(HWY_RVV_REDUCE, RedMin, fredmin, _ALL)
}  // namespace detail

template <class D>
HWY_API VFromD<D> MinOfLanes(D d, const VFromD<D> v) {
  using T = TFromD<D>;
  const ScalableTag<T> d1;  // always m1
  const auto neutral = Set(d1, HighestValue<T>());
  return detail::RedMin(d, v, neutral);
}

// ------------------------------ MaxOfLanes
namespace detail {
HWY_RVV_FOREACH_U(HWY_RVV_REDUCE, RedMax, redmaxu, _ALL)
HWY_RVV_FOREACH_I(HWY_RVV_REDUCE, RedMax, redmax, _ALL)
HWY_RVV_FOREACH_F(HWY_RVV_REDUCE, RedMax, fredmax, _ALL)
}  // namespace detail

template <class D>
HWY_API VFromD<D> MaxOfLanes(D d, const VFromD<D> v) {
  using T = TFromD<D>;
  const ScalableTag<T> d1;  // always m1
  const auto neutral = Set(d1, LowestValue<T>());
  return detail::RedMax(d, v, neutral);
}

#undef HWY_RVV_REDUCE

// ================================================== Ops with dependencies

// ------------------------------ PopulationCount (ShiftRight)

// Handles LMUL >= 2 or capped vectors, which generic_ops-inl cannot.
template <typename V, class D = DFromV<V>, HWY_IF_LANE_SIZE_D(D, 1),
          hwy::EnableIf<Pow2(D()) < 1 || MaxLanes(D()) < 16>* = nullptr>
HWY_API V PopulationCount(V v) {
  // See https://arxiv.org/pdf/1611.07612.pdf, Figure 3
  v = Sub(v, detail::AndS(ShiftRight<1>(v), 0x55));
  v = Add(detail::AndS(ShiftRight<2>(v), 0x33), detail::AndS(v, 0x33));
  return detail::AndS(Add(v, ShiftRight<4>(v)), 0x0F);
}

// ------------------------------ LoadDup128

template <class D, typename T = TFromD<D>>
HWY_API VFromD<D> LoadDup128(D d, const T* const HWY_RESTRICT p) {
  const auto loaded = Load(d, p);
  // Broadcast the first block
  const auto idx = detail::AndS(detail::Iota0(d),
                                static_cast<T>(detail::LanesPerBlock(d) - 1));
  return TableLookupLanes(loaded, idx);
}

// ------------------------------ LoadMaskBits

// Support all combinations of T and SHIFT(LMUL) without explicit overloads for
// each. First overload for MLEN=1..64.
namespace detail {

// Maps D to MLEN (wrapped in SizeTag), such that #mask_bits = VLEN/MLEN. MLEN
// increases with lane size and decreases for increasing LMUL. Cap at 64, the
// largest supported by HWY_RVV_FOREACH_B (and intrinsics), for virtual LMUL
// e.g. vuint16mf8_t: (8*2 << 3) == 128.
template <class D>
using MaskTag = hwy::SizeTag<HWY_MIN(
    64, detail::ScaleByPower(8 * sizeof(TFromD<D>), -Pow2(D())))>;

#define HWY_RVV_LOAD_MASK_BITS(SEW, SHIFT, MLEN, NAME, OP)                \
  HWY_INLINE HWY_RVV_M(MLEN)                                              \
      NAME(hwy::SizeTag<MLEN> /* tag */, const uint8_t* bits, size_t N) { \
    return OP##_v_b##MLEN(bits, N);                                       \
  }
HWY_RVV_FOREACH_B(HWY_RVV_LOAD_MASK_BITS, LoadMaskBits, vlm)
#undef HWY_RVV_LOAD_MASK_BITS
}  // namespace detail

template <class D, class MT = detail::MaskTag<D>>
HWY_API auto LoadMaskBits(D d, const uint8_t* bits)
    -> decltype(detail::LoadMaskBits(MT(), bits, Lanes(d))) {
  return detail::LoadMaskBits(MT(), bits, Lanes(d));
}

// ------------------------------ StoreMaskBits
#define HWY_RVV_STORE_MASK_BITS(SEW, SHIFT, MLEN, NAME, OP)               \
  template <class D>                                                      \
  HWY_API size_t NAME(D d, HWY_RVV_M(MLEN) m, uint8_t* bits) {            \
    const size_t N = Lanes(d);                                            \
    OP##_v_b##MLEN(bits, m, N);                                           \
    /* Non-full byte, need to clear the undefined upper bits. */          \
    /* Use MaxLanes and sizeof(T) to move some checks to compile-time. */ \
    constexpr bool kLessThan8 =                                           \
        detail::ScaleByPower(16 / sizeof(TFromD<D>), Pow2(d)) < 8;        \
    if (MaxLanes(d) < 8 || (kLessThan8 && N < 8)) {                       \
      const int mask = (1 << N) - 1;                                      \
      bits[0] = static_cast<uint8_t>(bits[0] & mask);                     \
    }                                                                     \
    return (N + 7) / 8;                                                   \
  }
HWY_RVV_FOREACH_B(HWY_RVV_STORE_MASK_BITS, StoreMaskBits, vsm)
#undef HWY_RVV_STORE_MASK_BITS

// ------------------------------ CompressBits, CompressBitsStore (LoadMaskBits)

template <class V>
HWY_INLINE V CompressBits(V v, const uint8_t* HWY_RESTRICT bits) {
  return Compress(v, LoadMaskBits(DFromV<V>(), bits));
}

template <class D>
HWY_API size_t CompressBitsStore(VFromD<D> v, const uint8_t* HWY_RESTRICT bits,
                                 D d, TFromD<D>* HWY_RESTRICT unaligned) {
  return CompressStore(v, LoadMaskBits(d, bits), d, unaligned);
}

// ------------------------------ FirstN (Iota0, Lt, RebindMask, SlideUp)

// Disallow for 8-bit because Iota is likely to overflow.
template <class D, HWY_IF_NOT_LANE_SIZE_D(D, 1)>
HWY_API MFromD<D> FirstN(const D d, const size_t n) {
  const RebindToSigned<D> di;
  using TI = TFromD<decltype(di)>;
  return RebindMask(
      d, detail::LtS(BitCast(di, detail::Iota0(d)), static_cast<TI>(n)));
}

template <class D, HWY_IF_LANE_SIZE_D(D, 1)>
HWY_API MFromD<D> FirstN(const D d, const size_t n) {
  const auto zero = Zero(d);
  const auto one = Set(d, 1);
  return Eq(detail::SlideUp(one, zero, n), one);
}

// ------------------------------ Neg (Sub)

template <class V, HWY_IF_SIGNED_V(V)>
HWY_API V Neg(const V v) {
  return detail::ReverseSubS(v, 0);
}

// vector = f(vector), but argument is repeated
#define HWY_RVV_RETV_ARGV2(BASE, CHAR, SEW, SEWD, SEWH, LMUL, LMULD, LMULH, \
                           SHIFT, MLEN, NAME, OP)                           \
  HWY_API HWY_RVV_V(BASE, SEW, LMUL) NAME(HWY_RVV_V(BASE, SEW, LMUL) v) {   \
    return v##OP##_vv_##CHAR##SEW##LMUL(v, v, HWY_RVV_AVL(SEW, SHIFT));     \
  }

HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGV2, Neg, fsgnjn, _ALL)

// ------------------------------ Abs (Max, Neg)

template <class V, HWY_IF_SIGNED_V(V)>
HWY_API V Abs(const V v) {
  return Max(v, Neg(v));
}

HWY_RVV_FOREACH_F(HWY_RVV_RETV_ARGV2, Abs, fsgnjx, _ALL)

#undef HWY_RVV_RETV_ARGV2

// ------------------------------ AbsDiff (Abs, Sub)
template <class V>
HWY_API V AbsDiff(const V a, const V b) {
  return Abs(Sub(a, b));
}

// ------------------------------ Round  (NearestInt, ConvertTo, CopySign)

// IEEE-754 roundToIntegralTiesToEven returns floating-point, but we do not have
// a dedicated instruction for that. Rounding to integer and converting back to
// float is correct except when the input magnitude is large, in which case the
// input was already an integer (because mantissa >> exponent is zero).

namespace detail {
enum RoundingModes { kNear, kTrunc, kDown, kUp };

template <class V>
HWY_INLINE auto UseInt(const V v) -> decltype(MaskFromVec(v)) {
  return detail::LtS(Abs(v), MantissaEnd<TFromV<V>>());
}

}  // namespace detail

template <class V>
HWY_API V Round(const V v) {
  const DFromV<V> df;

  const auto integer = NearestInt(v);  // round using current mode
  const auto int_f = ConvertTo(df, integer);

  return IfThenElse(detail::UseInt(v), CopySign(int_f, v), v);
}

// ------------------------------ Trunc (ConvertTo)
template <class V>
HWY_API V Trunc(const V v) {
  const DFromV<V> df;
  const RebindToSigned<decltype(df)> di;

  const auto integer = ConvertTo(di, v);  // round toward 0
  const auto int_f = ConvertTo(df, integer);

  return IfThenElse(detail::UseInt(v), CopySign(int_f, v), v);
}

// ------------------------------ Ceil
template <class V>
HWY_API V Ceil(const V v) {
  asm volatile("fsrm %0" ::"r"(detail::kUp));
  const auto ret = Round(v);
  asm volatile("fsrm %0" ::"r"(detail::kNear));
  return ret;
}

// ------------------------------ Floor
template <class V>
HWY_API V Floor(const V v) {
  asm volatile("fsrm %0" ::"r"(detail::kDown));
  const auto ret = Round(v);
  asm volatile("fsrm %0" ::"r"(detail::kNear));
  return ret;
}

// ------------------------------ Floating-point classification (Ne)

// vfclass does not help because it would require 3 instructions (to AND and
// then compare the bits), whereas these are just 1-3 integer instructions.

template <class V>
HWY_API MFromD<DFromV<V>> IsNaN(const V v) {
  return Ne(v, v);
}

template <class V, class D = DFromV<V>>
HWY_API MFromD<D> IsInf(const V v) {
  const D d;
  const RebindToSigned<decltype(d)> di;
  using T = TFromD<D>;
  const VFromD<decltype(di)> vi = BitCast(di, v);
  // 'Shift left' to clear the sign bit, check for exponent=max and mantissa=0.
  return RebindMask(d, detail::EqS(Add(vi, vi), hwy::MaxExponentTimes2<T>()));
}

// Returns whether normal/subnormal/zero.
template <class V, class D = DFromV<V>>
HWY_API MFromD<D> IsFinite(const V v) {
  const D d;
  const RebindToUnsigned<decltype(d)> du;
  const RebindToSigned<decltype(d)> di;  // cheaper than unsigned comparison
  using T = TFromD<D>;
  const VFromD<decltype(du)> vu = BitCast(du, v);
  // 'Shift left' to clear the sign bit, then right so we can compare with the
  // max exponent (cannot compare with MaxExponentTimes2 directly because it is
  // negative and non-negative floats would be greater).
  const VFromD<decltype(di)> exp =
      BitCast(di, ShiftRight<hwy::MantissaBits<T>() + 1>(Add(vu, vu)));
  return RebindMask(d, detail::LtS(exp, hwy::MaxExponentField<T>()));
}

// ------------------------------ Iota (ConvertTo)

template <class D, HWY_IF_UNSIGNED_D(D)>
HWY_API VFromD<D> Iota(const D d, TFromD<D> first) {
  return detail::AddS(detail::Iota0(d), first);
}

template <class D, HWY_IF_SIGNED_D(D)>
HWY_API VFromD<D> Iota(const D d, TFromD<D> first) {
  const RebindToUnsigned<D> du;
  return detail::AddS(BitCast(d, detail::Iota0(du)), first);
}

template <class D, HWY_IF_FLOAT_D(D)>
HWY_API VFromD<D> Iota(const D d, TFromD<D> first) {
  const RebindToUnsigned<D> du;
  const RebindToSigned<D> di;
  return detail::AddS(ConvertTo(d, BitCast(di, detail::Iota0(du))), first);
}

// ------------------------------ MulEven/Odd (Mul, OddEven)

template <class V, HWY_IF_LANE_SIZE_V(V, 4), class D = DFromV<V>,
          class DW = RepartitionToWide<D>>
HWY_API VFromD<DW> MulEven(const V a, const V b) {
  const auto lo = Mul(a, b);
  const auto hi = detail::MulHigh(a, b);
  return BitCast(DW(), OddEven(detail::Slide1Up(hi), lo));
}

// There is no 64x64 vwmul.
template <class V, HWY_IF_LANE_SIZE_V(V, 8)>
HWY_INLINE V MulEven(const V a, const V b) {
  const auto lo = detail::Mul(a, b);
  const auto hi = detail::MulHigh(a, b);
  return OddEven(detail::Slide1Up(hi), lo);
}

template <class V, HWY_IF_LANE_SIZE_V(V, 8)>
HWY_INLINE V MulOdd(const V a, const V b) {
  const auto lo = detail::Mul(a, b);
  const auto hi = detail::MulHigh(a, b);
  return OddEven(hi, detail::Slide1Down(lo));
}

// ------------------------------ ReorderDemote2To (OddEven)

template <size_t N, int kPow2>
HWY_API VFromD<Simd<uint16_t, N, kPow2>> ReorderDemote2To(
    Simd<bfloat16_t, N, kPow2> dbf16,
    VFromD<RepartitionToWide<decltype(dbf16)>> a,
    VFromD<RepartitionToWide<decltype(dbf16)>> b) {
  const RebindToUnsigned<decltype(dbf16)> du16;
  const RebindToUnsigned<DFromV<decltype(a)>> du32;
  const VFromD<decltype(du32)> b_in_even = ShiftRight<16>(BitCast(du32, b));
  return BitCast(dbf16, OddEven(BitCast(du16, a), BitCast(du16, b_in_even)));
}

// ------------------------------ ReorderWidenMulAccumulate (MulAdd, ZipLower)

template <class DF>
using DU16FromDF = RepartitionToNarrow<RebindToUnsigned<DF>>;

template <size_t N, int kPow2>
HWY_API auto ReorderWidenMulAccumulate(Simd<float, N, kPow2> df32,
                                       VFromD<DU16FromDF<decltype(df32)>> a,
                                       VFromD<DU16FromDF<decltype(df32)>> b,
                                       const VFromD<decltype(df32)> sum0,
                                       VFromD<decltype(df32)>& sum1)
    -> VFromD<decltype(df32)> {
  const DU16FromDF<decltype(df32)> du16;
  const RebindToUnsigned<decltype(df32)> du32;
  using VU32 = VFromD<decltype(du32)>;
  const VFromD<decltype(du16)> zero = Zero(du16);
  const VU32 a0 = ZipLower(du32, zero, BitCast(du16, a));
  const VU32 a1 = ZipUpper(du32, zero, BitCast(du16, a));
  const VU32 b0 = ZipLower(du32, zero, BitCast(du16, b));
  const VU32 b1 = ZipUpper(du32, zero, BitCast(du16, b));
  sum1 = MulAdd(BitCast(df32, a1), BitCast(df32, b1), sum1);
  return MulAdd(BitCast(df32, a0), BitCast(df32, b0), sum0);
}

// ------------------------------ Lt128
template <class D>
HWY_INLINE MFromD<D> Lt128(D d, const VFromD<D> a, const VFromD<D> b) {
  static_assert(!IsSigned<TFromD<D>>() && sizeof(TFromD<D>) == 8, "Use u64");
  // Truth table of Eq and Compare for Hi and Lo u64.
  // (removed lines with (=H && cH) or (=L && cL) - cannot both be true)
  // =H =L cH cL  | out = cH | (=H & cL)
  //  0  0  0  0  |  0
  //  0  0  0  1  |  0
  //  0  0  1  0  |  1
  //  0  0  1  1  |  1
  //  0  1  0  0  |  0
  //  0  1  0  1  |  0
  //  0  1  1  0  |  1
  //  1  0  0  0  |  0
  //  1  0  0  1  |  1
  //  1  1  0  0  |  0
  const VFromD<D> eqHL = VecFromMask(d, Eq(a, b));
  const VFromD<D> ltHL = VecFromMask(d, Lt(a, b));
  // Shift leftward so L can influence H.
  const VFromD<D> ltLx = detail::Slide1Up(ltHL);
  const VFromD<D> vecHx = OrAnd(ltHL, eqHL, ltLx);
  // Replicate H to its neighbor.
  return MaskFromVec(OddEven(vecHx, detail::Slide1Down(vecHx)));
}

// ------------------------------ Lt128Upper
template <class D>
HWY_INLINE MFromD<D> Lt128Upper(D d, const VFromD<D> a, const VFromD<D> b) {
  static_assert(!IsSigned<TFromD<D>>() && sizeof(TFromD<D>) == 8, "Use u64");
  const VFromD<D> ltHL = VecFromMask(d, Lt(a, b));
  // Replicate H to its neighbor.
  return MaskFromVec(OddEven(ltHL, detail::Slide1Down(ltHL)));
}

// ------------------------------ Eq128
template <class D>
HWY_INLINE MFromD<D> Eq128(D d, const VFromD<D> a, const VFromD<D> b) {
  static_assert(!IsSigned<TFromD<D>>() && sizeof(TFromD<D>) == 8, "Use u64");
  const VFromD<D> eqHL = VecFromMask(d, Eq(a, b));
  const VFromD<D> eqLH = Reverse2(d, eqHL);
  return MaskFromVec(And(eqHL, eqLH));
}

// ------------------------------ Eq128Upper
template <class D>
HWY_INLINE MFromD<D> Eq128Upper(D d, const VFromD<D> a, const VFromD<D> b) {
  static_assert(!IsSigned<TFromD<D>>() && sizeof(TFromD<D>) == 8, "Use u64");
  const VFromD<D> eqHL = VecFromMask(d, Eq(a, b));
  // Replicate H to its neighbor.
  return MaskFromVec(OddEven(eqHL, detail::Slide1Down(eqHL)));
}

// ------------------------------ Min128, Max128 (Lt128)

template <class D>
HWY_INLINE VFromD<D> Min128(D /* tag */, const VFromD<D> a, const VFromD<D> b) {
  const VFromD<D> aXH = detail::Slide1Down(a);
  const VFromD<D> bXH = detail::Slide1Down(b);
  const VFromD<D> minHL = Min(a, b);
  const MFromD<D> ltXH = Lt(aXH, bXH);
  const MFromD<D> eqXH = Eq(aXH, bXH);
  // If the upper lane is the decider, take lo from the same reg.
  const VFromD<D> lo = IfThenElse(ltXH, a, b);
  // The upper lane is just minHL; if they are equal, we also need to use the
  // actual min of the lower lanes.
  return OddEven(minHL, IfThenElse(eqXH, minHL, lo));
}

template <class D>
HWY_INLINE VFromD<D> Max128(D /* tag */, const VFromD<D> a, const VFromD<D> b) {
  const VFromD<D> aXH = detail::Slide1Down(a);
  const VFromD<D> bXH = detail::Slide1Down(b);
  const VFromD<D> maxHL = Max(a, b);
  const MFromD<D> ltXH = Lt(aXH, bXH);
  const MFromD<D> eqXH = Eq(aXH, bXH);
  // If the upper lane is the decider, take lo from the same reg.
  const VFromD<D> lo = IfThenElse(ltXH, b, a);
  // The upper lane is just maxHL; if they are equal, we also need to use the
  // actual min of the lower lanes.
  return OddEven(maxHL, IfThenElse(eqXH, maxHL, lo));
}

template <class D>
HWY_INLINE VFromD<D> Min128Upper(D d, VFromD<D> a, VFromD<D> b) {
  return IfThenElse(Lt128Upper(d, a, b), a, b);
}

template <class D>
HWY_INLINE VFromD<D> Max128Upper(D d, VFromD<D> a, VFromD<D> b) {
  return IfThenElse(Lt128Upper(d, b, a), a, b);
}

// ================================================== END MACROS
namespace detail {  // for code folding
#undef HWY_RVV_AVL
#undef HWY_RVV_D
#undef HWY_RVV_FOREACH
#undef HWY_RVV_FOREACH_08_ALL
#undef HWY_RVV_FOREACH_08_ALL_VIRT
#undef HWY_RVV_FOREACH_08_DEMOTE
#undef HWY_RVV_FOREACH_08_DEMOTE_VIRT
#undef HWY_RVV_FOREACH_08_EXT
#undef HWY_RVV_FOREACH_08_EXT_VIRT
#undef HWY_RVV_FOREACH_08_TRUNC
#undef HWY_RVV_FOREACH_08_VIRT
#undef HWY_RVV_FOREACH_16_ALL
#undef HWY_RVV_FOREACH_16_ALL_VIRT
#undef HWY_RVV_FOREACH_16_DEMOTE
#undef HWY_RVV_FOREACH_16_DEMOTE_VIRT
#undef HWY_RVV_FOREACH_16_EXT
#undef HWY_RVV_FOREACH_16_EXT_VIRT
#undef HWY_RVV_FOREACH_16_TRUNC
#undef HWY_RVV_FOREACH_16_VIRT
#undef HWY_RVV_FOREACH_32_ALL
#undef HWY_RVV_FOREACH_32_ALL_VIRT
#undef HWY_RVV_FOREACH_32_DEMOTE
#undef HWY_RVV_FOREACH_32_DEMOTE_VIRT
#undef HWY_RVV_FOREACH_32_EXT
#undef HWY_RVV_FOREACH_32_EXT_VIRT
#undef HWY_RVV_FOREACH_32_TRUNC
#undef HWY_RVV_FOREACH_32_VIRT
#undef HWY_RVV_FOREACH_64_ALL
#undef HWY_RVV_FOREACH_64_ALL_VIRT
#undef HWY_RVV_FOREACH_64_DEMOTE
#undef HWY_RVV_FOREACH_64_DEMOTE_VIRT
#undef HWY_RVV_FOREACH_64_EXT
#undef HWY_RVV_FOREACH_64_EXT_VIRT
#undef HWY_RVV_FOREACH_64_TRUNC
#undef HWY_RVV_FOREACH_64_VIRT
#undef HWY_RVV_FOREACH_B
#undef HWY_RVV_FOREACH_F
#undef HWY_RVV_FOREACH_F16
#undef HWY_RVV_FOREACH_F32
#undef HWY_RVV_FOREACH_F3264
#undef HWY_RVV_FOREACH_F64
#undef HWY_RVV_FOREACH_I
#undef HWY_RVV_FOREACH_I08
#undef HWY_RVV_FOREACH_I16
#undef HWY_RVV_FOREACH_I163264
#undef HWY_RVV_FOREACH_I32
#undef HWY_RVV_FOREACH_I64
#undef HWY_RVV_FOREACH_U
#undef HWY_RVV_FOREACH_U08
#undef HWY_RVV_FOREACH_U16
#undef HWY_RVV_FOREACH_U163264
#undef HWY_RVV_FOREACH_U32
#undef HWY_RVV_FOREACH_U64
#undef HWY_RVV_FOREACH_UI
#undef HWY_RVV_FOREACH_UI08
#undef HWY_RVV_FOREACH_UI16
#undef HWY_RVV_FOREACH_UI163264
#undef HWY_RVV_FOREACH_UI32
#undef HWY_RVV_FOREACH_UI3264
#undef HWY_RVV_FOREACH_UI64
#undef HWY_RVV_M
#undef HWY_RVV_RETM_ARGM
#undef HWY_RVV_RETV_ARGV
#undef HWY_RVV_RETV_ARGVS
#undef HWY_RVV_RETV_ARGVV
#undef HWY_RVV_T
#undef HWY_RVV_V
}  // namespace detail
// NOLINTNEXTLINE(google-readability-namespace-comments)
}  // namespace HWY_NAMESPACE
}  // namespace hwy
HWY_AFTER_NAMESPACE();
