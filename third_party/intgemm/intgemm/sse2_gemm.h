#pragma once

#include "kernels.h"
#include "multiply.h"
#include "types.h"

#include <cstdint>

// 8 bit is in ssse3_gemm.h

namespace intgemm {
namespace SSE2 {

INTGEMM_SSE2 inline __m128i QuantizerGrab(const float *input, const __m128 quant_mult_reg) {
  return kernels::quantize(loadu_ps<__m128>(input), quant_mult_reg);
}

INTGEMM_SELECT_COL_B(INTGEMM_SSE2, __m128i)

class QuantizeTile16 {
  public:
    INTGEMM_SSE2 static inline Register Consecutive(__m128 mult_reg, const float *input) {
      return Tile(mult_reg, input, input + 4);
    }

    INTGEMM_SSE2 static inline Register ConsecutiveWithWrapping(__m128 mult_reg, const float *input, Index cols_left, Index cols, Index row_step) {
      return Tile(mult_reg,
        input,
        input + 4 + (cols_left <= 4 ? cols * (row_step - 1) : 0));
    }

    INTGEMM_SSE2 static inline Register ForReshape(__m128 mult_reg, const float *input, int) {
      return Consecutive(mult_reg, input);
    }

  private:
    INTGEMM_SSE2 static inline Register Tile(__m128 mult_reg, const float *input0, const float *input1) {
      __m128i g0 = kernels::quantize(loadu_ps<__m128>(input0), mult_reg);
      __m128i g1 = kernels::quantize(loadu_ps<__m128>(input1), mult_reg);
      return _mm_packs_epi32(g0, g1);
    }
};

// This should be pure SSE2 (and below).
struct Kernels16 {
  typedef int16_t Integer;

  // Currently A is prepared by quantization but this could theoretically change.
  INTGEMM_SSE2 static inline void PrepareA(const float *input, int16_t *output, float quant_mult, Index rows, Index cols) {
    Quantize(input, output, quant_mult, rows * cols);
  }

  INTGEMM_SSE2 static void Quantize(const float *input, int16_t *output, float quant_mult, Index size) {
    assert(size % 8 == 0);
    assert(reinterpret_cast<uintptr_t>(input) % 16 == 0);
    assert(reinterpret_cast<uintptr_t>(output) % 16 == 0);
    FRegister q = set1_ps<FRegister>(quant_mult);
    const float *end = input + size;
    for (; input != end; input += 8, output += 8) {
      *reinterpret_cast<__m128i*>(output) = QuantizeTile16::Consecutive(q, input);
    }
  }

  // Tile size for B; B must be a multiple of this block size.
  static const Index kBTileRow = 8;
  static const Index kBTileCol = 8;

  INTGEMM_PREPARE_B_16(INTGEMM_SSE2, QuantizeTile16)
  INTGEMM_PREPARE_B_QUANTIZED_TRANSPOSED(INTGEMM_SSE2, int16_t)
  INTGEMM_PREPARE_B_TRANSPOSED(INTGEMM_SSE2, QuantizeTile16, int16_t)

  INTGEMM_SSE2 static void SelectColumnsB(const int16_t *input, int16_t *output, Index rows, const Index *cols_begin, const Index *cols_end) {
    //TODO #DEFINE
    SelectColumnsOfB((const __m128i*)input, (__m128i*)output, rows * 2, cols_begin, cols_end);
  }
  INTGEMM_MULTIPLY16(__m128i, INTGEMM_SSE2, CPUType::SSE2)

  constexpr static const char *const kName = "16-bit SSE2";

  static const CPUType kUses = CPUType::SSE2;
};

} // namespace SSE2
} // namespace intgemm
