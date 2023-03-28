#pragma once

#include "intgemm/intgemm_config.h"
#include "intrinsics.h"
#include "types.h"

#include <algorithm>
#include <cassert>

namespace intgemm {

/*
 * Interleave vectors.
 */
#define INTGEMM_INTERLEAVE_N(target, type, N) \
target static inline void Interleave##N(type &first, type &second) { \
  type temp = unpacklo_epi##N(first, second); \
  second = unpackhi_epi##N(first, second); \
  first = temp; \
}

#define INTGEMM_INTERLEAVE(target, type) \
INTGEMM_INTERLEAVE_N(target, type, 8) \
INTGEMM_INTERLEAVE_N(target, type, 16) \
INTGEMM_INTERLEAVE_N(target, type, 32) \
INTGEMM_INTERLEAVE_N(target, type, 64)

INTGEMM_INTERLEAVE(INTGEMM_SSE2, __m128i)

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
INTGEMM_INTERLEAVE(INTGEMM_AVX2, __m256i)
#endif
#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
INTGEMM_INTERLEAVE(INTGEMM_AVX512BW, __m512i)
#endif

/*
 * Swap vectors.
 */
#define INTGEMM_SWAP(target, Register) \
target static inline void Swap(Register &a, Register &b) { \
  Register tmp = a; \
  a = b; \
  b = tmp; \
} \

INTGEMM_SWAP(INTGEMM_SSE2, __m128i)
#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
INTGEMM_SWAP(INTGEMM_AVX2, __m256i)
#endif
#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
/* Only INTGEMM_AVX512F is necessary but due to GCC 5.4 bug we have to set INTGEMM_AVX512BW */
INTGEMM_SWAP(INTGEMM_AVX512BW, __m512i)
#endif

/* Transpose registers containing 8 packed 16-bit integers.
 * Each 128-bit lane is handled independently.
 */
#define INTGEMM_TRANSPOSE16(target, Register) \
target static inline void Transpose16InLane(Register &r0, Register &r1, Register &r2, Register &r3, Register &r4, Register &r5, Register &r6, Register &r7) { \
  /* r0: columns 0 1 2 3 4 5 6 7 from row 0
     r1: columns 0 1 2 3 4 5 6 7 from row 1*/ \
  Interleave16(r0, r1); \
  Interleave16(r2, r3); \
  Interleave16(r4, r5); \
  Interleave16(r6, r7); \
  /* r0: columns 0 0 1 1 2 2 3 3 from rows 0 and 1
     r1: columns 4 4 5 5 6 6 7 7 from rows 0 and 1
     r2: columns 0 0 1 1 2 2 3 3 from rows 2 and 3
     r3: columns 4 4 5 5 6 6 7 7 from rows 2 and 3
     r4: columns 0 0 1 1 2 2 3 3 from rows 4 and 5
     r5: columns 4 4 5 5 6 6 7 7 from rows 4 and 5
     r6: columns 0 0 1 1 2 2 3 3 from rows 6 and 7
     r7: columns 4 4 5 5 6 6 7 7 from rows 6 and 7*/ \
  Interleave32(r0, r2); \
  Interleave32(r1, r3); \
  Interleave32(r4, r6); \
  Interleave32(r5, r7); \
  /* r0: columns 0 0 0 0 1 1 1 1 from rows 0, 1, 2, and 3
     r1: columns 4 4 4 4 5 5 5 5 from rows 0, 1, 2, and 3
     r2: columns 2 2 2 2 3 3 3 3 from rows 0, 1, 2, and 3
     r3: columns 6 6 6 6 7 7 7 7 from rows 0, 1, 2, and 3
     r4: columns 0 0 0 0 1 1 1 1 from rows 4, 5, 6, and 7
     r5: columns 4 4 4 4 5 5 5 5 from rows 4, 5, 6, and 7
     r6: columns 2 2 2 2 3 3 3 3 from rows 4, 5, 6, and 7
     r7: columns 6 6 6 6 7 7 7 7 from rows 4, 5, 6, and 7*/ \
  Interleave64(r0, r4); \
  Interleave64(r1, r5); \
  Interleave64(r2, r6); \
  Interleave64(r3, r7); \
  /* r0: columns 0 0 0 0 0 0 0 0 from rows 0 through 7
     r1: columns 4 4 4 4 4 4 4 4 from rows 0 through 7
     r2: columns 2 2 2 2 2 2 2 2 from rows 0 through 7
     r3: columns 6 6 6 6 6 6 6 6 from rows 0 through 7
     r4: columns 1 1 1 1 1 1 1 1 from rows 0 through 7
     r5: columns 5 5 5 5 5 5 5 5 from rows 0 through 7*/ \
  /* Empirically gcc is able to remove these movs and just rename the outputs of Interleave64. */ \
  Swap(r1, r4); \
  Swap(r3, r6); \
} \

INTGEMM_TRANSPOSE16(INTGEMM_SSE2, __m128i)
#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
INTGEMM_TRANSPOSE16(INTGEMM_AVX2, __m256i)
#endif
#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
/* Only INTGEMM_AVX512F is necessary but due to GCC 5.4 bug we have to set INTGEMM_AVX512BW */
INTGEMM_TRANSPOSE16(INTGEMM_AVX512BW, __m512i)
#endif

/* Tranpose registers containing 16 packed 8-bit integers.
 * Each 128-bit lane is handled independently.
 */
template <class Register> static inline void Transpose8InLane(
    Register &r0, Register &r1, Register &r2, Register &r3, Register &r4, Register &r5, Register &r6, Register &r7,
    Register &r8, Register &r9, Register &r10, Register &r11, Register &r12, Register &r13, Register &r14, Register &r15) {
  // Get 8-bit values to 16-bit values so they can travel together.
  Interleave8(r0, r1);
  // r0: columns 0 0 1 1 2 2 3 3 4 4 5 5 6 6 7 7 from rows 0 and 1.
  // r1: columns 8 8 9 9 10 10 11 11 12 12 13 13 14 14 15 15 from rows 0 and 1.
  Interleave8(r2, r3);
  // r2: columns 0 0 1 1 2 2 3 3 4 4 5 5 6 6 7 7 from rows 2 and 3.
  Interleave8(r4, r5);
  Interleave8(r6, r7);
  Interleave8(r8, r9);
  Interleave8(r10, r11);
  Interleave8(r12, r13);
  Interleave8(r14, r15);
  Transpose16InLane(r0, r2, r4, r6, r8, r10, r12, r14);
  Transpose16InLane(r1, r3, r5, r7, r9, r11, r13, r15);
  // Permute into correct order.  This is free because the outputs just get pemuted.
  Register tmp;
  tmp = r2;
  r2 = r4;
  r4 = r8;
  r8 = r1;
  r1 = tmp;
  tmp = r3;
  r3 = r6;
  r6 = r12;
  r12 = r9;
  r9 = tmp;
  tmp = r5;
  r5 = r10;
  r10 = tmp;
  tmp = r7;
  r7 = r14;
  r14 = r13;
  r13 = r11;
  r11 = tmp;
}

// PREPARE B: quantize and rearrange.  B is presumed to be constantparameters
// so we can take our time rearranging it in order to save during the multiply.
//
// We presume B starts in row-major order.
//
// In INTGEMM_AVX2, a register holds 32 8-bit values or 16 16-bit values and we want
// that many values from the same column in the register.
//
// The multiplier reads 8 rows at a time and we want these reads to be
// contiguous.
//
// Each 8x32 (for 8-bit) or 8x16 (for 16-bit) tile of B is transposed.
// The tiles are stored in column major order.
//
// For INTGEMM_AVX2, this matrix shows what index each value of B will be stored at:
//   0  16 ... 240
//   1  17 ... 241
//   2  18 ... 242
//   3  19 ... 243
//   4  20 ... 244
//   5  21 ... 245
//   6  22 ... 246
//   7  23 ... 247
//   8  24 ... 248
//   9  25 ... 249
//  10  26 ... 250
//  11  27 ... 251
//  12  28 ... 252
//  13  29 ... 253
//  14  30 ... 254
//  15  31 ... 255
// 256 272
// 257 273
// ... ...
#define INTGEMM_PREPARE_B_8(target, QuantClass) \
target static inline void PrepareB(const float *input, int8_t *output_shadow, float quant_mult, Index rows, Index cols) { \
  FRegister q = set1_ps<FRegister>(quant_mult); \
  /* Currently all multipliers have a stride of 8 columns.*/ \
  const Index kColStride = 8; \
  assert(cols % kColStride == 0); \
  assert(rows % sizeof(Register) == 0); \
  assert(reinterpret_cast<uintptr_t>(input) % sizeof(Register) == 0); \
  Register *output = reinterpret_cast<Register*>(output_shadow); \
  assert(reinterpret_cast<uintptr_t>(output) % sizeof(Register) == 0); \
  for (Index c = 0; c < cols; c += kColStride) { \
    for (Index r = 0; r < rows; r += sizeof(Register), output += 8) { \
      /* Quantize and perform a transpose with height sizeof(Register) and width 8. \
         This isn't quite Transpose8InLane because it's half the number of columns, \
         so each register starts with two rows instead of being one row. \
         The quantizers know to skip a row.*/ \
      output[0] = QuantClass::ForReshape(q, input + cols * (r    ) + c, cols); \
      output[1] = QuantClass::ForReshape(q, input + cols * (r + 1) + c, cols); \
      output[2] = QuantClass::ForReshape(q, input + cols * (r + 4) + c, cols); \
      output[3] = QuantClass::ForReshape(q, input + cols * (r + 5) + c, cols); \
      output[4] = QuantClass::ForReshape(q, input + cols * (r + 8) + c, cols); \
      output[5] = QuantClass::ForReshape(q, input + cols * (r + 9) + c, cols); \
      output[6] = QuantClass::ForReshape(q, input + cols * (r + 12) + c, cols); \
      output[7] = QuantClass::ForReshape(q, input + cols * (r + 13) + c, cols); \
      Interleave8(output[0], output[1]); \
      Interleave8(output[2], output[3]); \
      Interleave8(output[4], output[5]); \
      Interleave8(output[6], output[7]); \
      Transpose16InLane(output[0], output[1], output[2], output[3], output[4], output[5], output[6], output[7]); \
    } \
  } \
} \

#define INTGEMM_PREPARE_B_16(target, QuantClass) \
target static inline void PrepareB(const float *input, int16_t *output_shadow, float quant_mult, Index rows, Index cols) { \
  FRegister q = set1_ps<FRegister>(quant_mult); \
  assert(cols % 8 == 0); \
  assert(rows % (sizeof(Register) / sizeof(int16_t)) == 0); \
  assert(reinterpret_cast<uintptr_t>(input) % sizeof(Register) == 0); \
  Register *output = reinterpret_cast<Register*>(output_shadow); \
  assert(reinterpret_cast<uintptr_t>(output) % sizeof(Register) == 0); \
  for (Index c = 0; c < cols; c += 8) { \
    for (Index r = 0; r < rows; r += (sizeof(Register) / sizeof(int16_t)), output += 8) { \
      /* gcc unrolls this loop and uses registers for output[k]*/ \
      for (Index k = 0; k < 8; ++k) { \
        output[k] = QuantClass::ForReshape(q, input + cols * (r + k) + c, cols); \
      } \
      Transpose16InLane(output[0], output[1], output[2], output[3], output[4], output[5], output[6], output[7]); \
    } \
  } \
}

/*
 * Prepare B matrix.
 * B matrix has to be transposed and quantized.
 * Cols has to be a multiple of sizeof(Register) / sizeof(Integer).
 *
 * cols and rows describe size of transposed B.
 */
#define INTGEMM_PREPARE_B_QUANTIZED_TRANSPOSED(target, Integer) \
target static inline void PrepareBQuantizedTransposed(const Integer* input, Integer* output, Index cols, Index rows) { \
  const Index RegisterElems = sizeof(Register) / sizeof(Integer); \
  const Index kColStride = 8; \
  \
  assert(cols % RegisterElems == 0); \
  assert(rows % kColStride == 0); \
  assert(reinterpret_cast<uintptr_t>(input) % sizeof(Register) == 0); \
  assert(reinterpret_cast<uintptr_t>(output) % sizeof(Register) == 0); \
  \
  Register* output_it = reinterpret_cast<Register*>(output); \
  for (Index r = 0; r < rows; r += kColStride) \
    for (Index c = 0; c < cols; c += RegisterElems) \
      for (Index ri = 0; ri < 8; ++ri) \
        *output_it++ = *reinterpret_cast<const Register*>(input + (r + ri) * cols + c); \
}

/*
 * Prepare B matrix.
 * B matrix has to be transposed.
 * Cols has to be a multiple of sizeof(Register) / sizeof(float).
 *
 * cols and rows describe size of transposed B.
 */
#define INTGEMM_PREPARE_B_TRANSPOSED(target, Quantizer, Integer) \
target static inline void PrepareBTransposed(const float* input, Integer* output, float quant_mult, Index cols, Index rows) { \
  const Index RegisterElemsInt = sizeof(Register) / sizeof(Integer); \
  const Index kColStride = 8; \
  \
  assert(cols % (sizeof(Register) / sizeof(float)) == 0); \
  assert(rows % kColStride == 0); \
  assert(reinterpret_cast<uintptr_t>(input) % sizeof(Register) == 0); \
  assert(reinterpret_cast<uintptr_t>(output) % sizeof(Register) == 0); \
  \
  FRegister q = set1_ps<FRegister>(quant_mult); \
  Register* output_it = reinterpret_cast<Register*>(output); \
  Index r = 0; \
  Index c = 0; \
  while (r < rows) { \
    for (Index ri = 0; ri < 8; ++ri) \
      *output_it++ = Quantizer::ConsecutiveWithWrapping(q, input + (r + ri) * cols + c, cols - c, cols, 8); \
    c += RegisterElemsInt; \
    while (c >= cols) { \
      r += kColStride; \
      c -= cols; \
    } \
  } \
}

/* Select columns of B from PrepareB format to PrepareB format.
 */
#define INTGEMM_SELECT_COL_B(target, Register) \
target static inline void SelectColumnsOfB(const Register *input, Register *output, Index rows_bytes /* number of bytes in a row */, const Index *cols_begin, const Index *cols_end) { \
  assert(rows_bytes % sizeof(Register) == 0); \
  assert((cols_end - cols_begin) % 8 == 0);  \
  /* Do columns for multiples of 8.*/ \
  Index register_rows = rows_bytes / sizeof(Register); \
  const Register *starts[8]; \
  for (; cols_begin != cols_end; cols_begin += 8) { \
    for (Index k = 0; k < 8; ++k) { \
      starts[k] = input + (cols_begin[k] & 7) + (cols_begin[k] & ~7) * register_rows; \
    } \
    for (Index r = 0; r < register_rows; ++r) { \
      for (Index k = 0; k < 8; ++k) { \
        *(output++) = *starts[k]; \
        starts[k] += 8; \
      } \
    } \
  } \
}

} // namespace intgemm
