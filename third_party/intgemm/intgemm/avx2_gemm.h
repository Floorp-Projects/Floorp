#pragma once

#include "intgemm/intgemm_config.h"

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2

#include "interleave.h"
#include "kernels.h"
#include "multiply.h"
#include "types.h"

#include <cstdint>
#include <cstring>

namespace intgemm {
namespace AVX2 {

INTGEMM_AVX2 inline Register QuantizerGrab(const float *input, const __m256 quant_mult_reg) {
  return kernels::quantize(loadu_ps<FRegister>(input), quant_mult_reg);
}

INTGEMM_SELECT_COL_B(INTGEMM_AVX2, __m256i)

class QuantizeTile16 {
  public:
    INTGEMM_AVX2 static inline Register Consecutive(FRegister mult_reg, const float *input) {
      return Tile(mult_reg, input, input + 8);
    }

    INTGEMM_AVX2 static inline Register ConsecutiveWithWrapping(FRegister mult_reg, const float *input, Index cols_left, Index cols, Index row_step) {
      return Tile(mult_reg,
        input,
        input + 8 + (cols_left <= 8 ? cols * (row_step - 1) : 0));
    }

    INTGEMM_AVX2 static inline Register ForReshape(FRegister mult_reg, const float *input, Index cols) {
      // 8 rows in the first 128-bit register, 8 in the second register.
      return Tile(mult_reg, input, input + 8 * cols);
    }

  private:
    INTGEMM_AVX2 static inline Register Tile(FRegister mult_reg, const float *input0, const float *input1) {
      Register g0 = QuantizerGrab(input0, mult_reg);
      Register g1 = QuantizerGrab(input1, mult_reg);
      Register packed = _mm256_packs_epi32(g0, g1);
      // Reorder the packed values because Intel does 0 1 2 3 8 9 10 11 4 5 6 7 12 13 14 15.
      // Technically this could be removed if the PrepareB did the same reordering internally.
      return _mm256_permute4x64_epi64(packed, 0xd8 /* 0, 2, 1, 3 */);
    }
};

struct Kernels16 {
  typedef int16_t Integer;

  // Currently A is prepared by quantization but this could theoretically change.
  INTGEMM_AVX2 static inline void PrepareA(const float *input, int16_t *output, float quant_mult, Index rows, Index cols) {
    Quantize(input, output, quant_mult, rows * cols);
  }

  // Just quantize everything in order.
  INTGEMM_AVX2 static void Quantize(const float *input, int16_t *output, float quant_mult, Index size) {
    assert(size % 16 == 0);
    assert(reinterpret_cast<uintptr_t>(input) % 32 == 0);
    FRegister q = set1_ps<FRegister>(quant_mult);
    const float *end = input + size;
    for (; input != end; input += 16, output += 16) {
      *reinterpret_cast<__m256i*>(output) = QuantizeTile16::Consecutive(q, input);
    }
  }

  // Tile size for B; B must be a multiple of this block size.
  static const Index kBTileRow = 16;
  static const Index kBTileCol = 8;
/*
  INTGEMM_AVX2 static void PrepareB(const float *input, int16_t *output, float quant_mult, Index rows, Index cols) {
    PrepareBFor16(input, output, AVX2::QuantizeTile16(quant_mult), rows, cols);
  }*/
  INTGEMM_PREPARE_B_16(INTGEMM_AVX2, AVX2::QuantizeTile16)
  INTGEMM_PREPARE_B_QUANTIZED_TRANSPOSED(INTGEMM_AVX2, int16_t)
  INTGEMM_PREPARE_B_TRANSPOSED(INTGEMM_AVX2, AVX2::QuantizeTile16, int16_t)

  INTGEMM_AVX2 static void SelectColumnsB(const int16_t *input, int16_t *output, Index rows, const Index *cols_begin, const Index *cols_end) {
    AVX2::SelectColumnsOfB((const __m256i*)input, (__m256i*)output, rows * 2, cols_begin, cols_end);
  }

  INTGEMM_MULTIPLY16(__m256i, INTGEMM_AVX2, CPUType::AVX2)

  constexpr static const char *const kName = "16-bit AVX2";

  static const CPUType kUses = CPUType::AVX2;
};

/* Read 8 floats at a time from input0, input1, input2, and input3.  Quantize
 * them to 8-bit by multiplying with quant_mult_reg then rounding. Concatenate
 * the result into one register and return it.
 */
class QuantizeTile8 {
  public:
    INTGEMM_AVX2 static inline Register Consecutive(FRegister quant_mult, const float *input) {
      return Tile(quant_mult, input, input + 8, input + 16, input + 24);
    }

    INTGEMM_AVX2 static inline Register ConsecutiveU(FRegister quant_mult, const float *input) {
      return TileU(quant_mult, input, input + 8, input + 16, input + 24);
    }

    INTGEMM_AVX2 static inline Register ConsecutiveWithWrapping(FRegister quant_mult, const float *input, Index cols_left, Index cols, Index row_step) {
      const float* inputs[4];
      for (Index i = 0; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        while (cols_left < sizeof(Register) / sizeof(float)) {
          input += cols * (row_step - 1);
          cols_left += cols;
        }
        inputs[i] = input;
        input += sizeof(Register) / sizeof(float);
        cols_left -= sizeof(Register) / sizeof(float);
      }
      return Tile(quant_mult, inputs[0], inputs[1], inputs[2], inputs[3]);
    }

    INTGEMM_AVX2 static inline Register ForReshape(FRegister quant_mult, const float *input, Index cols) {
      // Put higher rows in the second half of the register.  These will jumble
      // around in the same way then conveniently land in the right place.
      return Tile(quant_mult, input, input + 2 * cols, input + 16 * cols, input + 18 * cols);
    }

    INTGEMM_AVX2 static inline __m256i Tile(FRegister quant_mult, const float *input0, const float *input1, const float *input2, const float *input3) {
      // Looking at the assembly, gcc has pulled this outside the loops calling this.
      const __m256i neg127 = _mm256_set1_epi8(-127);
      const __m256i shuffle_param = _mm256_set_epi32(7, 3, 6, 2, 5, 1, 4, 0);
      // Grab 4 registers at a time in 32-bit format.
      __m256i g0 = AVX2::QuantizerGrab(input0, quant_mult);
      __m256i g1 = AVX2::QuantizerGrab(input1, quant_mult);
      __m256i g2 = AVX2::QuantizerGrab(input2, quant_mult);
      __m256i g3 = AVX2::QuantizerGrab(input3, quant_mult);
      // Pack 32-bit to 16-bit.
      __m256i packed0 = _mm256_packs_epi32(g0, g1);
      __m256i packed1 = _mm256_packs_epi32(g2, g3);
      // Pack 16-bit to 8-bit.
      __m256i packed = _mm256_packs_epi16(packed0, packed1);
      // Ban -128.
      packed = _mm256_max_epi8(packed, neg127);
      // Currently in 0 1 2 3 8 9 10 11 16 17 18 19 24 25 26 27 4 5 6 7 12 13 14 15 20 21 22 23 28 29 30 31
      // Or as 32-bit integers 0 2 4 6 1 3 5 7
      // Technically this could be removed so long as the rows are bigger than 16
      // and the values are only used for GEMM.
      return _mm256_permutevar8x32_epi32(packed, shuffle_param);
    }

  private:
    //A version that produces uint8_ts
    INTGEMM_AVX2 static inline Register TileU(FRegister quant_mult, const float *input0, const float *input1, const float *input2, const float *input3) {
      // Looking at the assembly, gcc has pulled this outside the loops calling this.
      const __m256i neg127 = _mm256_set1_epi8(-127);
      const __m256i pos127 = _mm256_set1_epi8(127);
      const __m256i shuffle_param = _mm256_set_epi32(7, 3, 6, 2, 5, 1, 4, 0);
      // Grab 4 registers at a time in 32-bit format.
      __m256i g0 = AVX2::QuantizerGrab(input0, quant_mult);
      __m256i g1 = AVX2::QuantizerGrab(input1, quant_mult);
      __m256i g2 = AVX2::QuantizerGrab(input2, quant_mult);
      __m256i g3 = AVX2::QuantizerGrab(input3, quant_mult);
      // Pack 32-bit to 16-bit.
      __m256i packed0 = _mm256_packs_epi32(g0, g1);
      __m256i packed1 = _mm256_packs_epi32(g2, g3);
      // Pack 16-bit to 8-bit.
      __m256i packed = _mm256_packs_epi16(packed0, packed1);
      // Ban -128.
      packed = _mm256_max_epi8(packed, neg127); //Could be removed  if we use +128
      packed = _mm256_add_epi8(packed, pos127);
      // Currently in 0 1 2 3 8 9 10 11 16 17 18 19 24 25 26 27 4 5 6 7 12 13 14 15 20 21 22 23 28 29 30 31
      // Or as 32-bit integers 0 2 4 6 1 3 5 7
      // Technically this could be removed so long as the rows are bigger than 16
      // and the values are only used for GEMM.
      return _mm256_permutevar8x32_epi32(packed, shuffle_param);
    }
};

struct Kernels8 {
  typedef int8_t Integer;

  // Currently A is prepared by quantization but this could theoretically change.
  INTGEMM_AVX2 static inline void PrepareA(const float *input, int8_t *output, float quant_mult, Index rows, Index cols) {
    Quantize(input, output, quant_mult, rows * cols);
  }
 private:
  INTGEMM_QUANTIZE_THREAD(INTGEMM_AVX2)
 public:
  INTGEMM_QUANTIZE(INTGEMM_AVX2)

  // Currently A is prepared by quantization but this could theoretically change.
  INTGEMM_AVX2 static inline void PrepareA(const float *input, uint8_t *output, float quant_mult, Index rows, Index cols) {
    QuantizeU(input, output, quant_mult, rows * cols);
  }

  // Just quantize everything in order.
  INTGEMM_AVX2 static void QuantizeU(const float *input, uint8_t *output, float quant_mult, Index size) {
    assert(size % 32 == 0);
    assert(reinterpret_cast<uintptr_t>(input) % 32 == 0);
    FRegister q = set1_ps<FRegister>(quant_mult);
    const float *end = input + size;
    for (; input != end; input += 32, output += 32) {
      *reinterpret_cast<__m256i*>(output) = QuantizeTile8::ConsecutiveU(q, input);
    }
  }

  // Tile size for B; B must be a multiple of this block size.
  static const Index kBTileRow = 32;
  static const Index kBTileCol = 8;

  INTGEMM_PREPARE_B_8(INTGEMM_AVX2, AVX2::QuantizeTile8)
  INTGEMM_PREPARE_B_QUANTIZED_TRANSPOSED(INTGEMM_AVX2, int8_t)
  INTGEMM_PREPARE_B_TRANSPOSED(INTGEMM_AVX2, AVX2::QuantizeTile8, int8_t)

  INTGEMM_AVX2 static void SelectColumnsB(const int8_t *input, int8_t *output, Index rows, const Index *cols_begin, const Index *cols_end) {
    AVX2::SelectColumnsOfB((const __m256i*)input, (__m256i*)output, rows, cols_begin, cols_end);
  }

  INTGEMM_MULTIPLY8(__m256i, INTGEMM_AVX2, CPUType::AVX2)

  INTGEMM_MULTIPLY8SHIFT(__m256i, INTGEMM_AVX2, CPUType::AVX2)

  INTGEMM_PREPAREBIASFOR8(__m256i, INTGEMM_AVX2, CPUType::AVX2)
  
  constexpr static const char *const kName = "8-bit AVX2";

  static const CPUType kUses = CPUType::AVX2;
};

} // namespace AVX2
} // namespace intgemm

#endif
