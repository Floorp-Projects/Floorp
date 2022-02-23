#pragma once

#include "interleave.h"
#include "kernels.h"
#include "multiply.h"
#include "types.h"

#include <cstdint>
#include <cstring>

// 16-bit is in sse2_gemm.h

namespace intgemm {
namespace SSSE3 {

INTGEMM_SSSE3 inline __m128i QuantizerGrab(const float *input, const __m128 quant_mult_reg) {
  return kernels::quantize(loadu_ps<__m128>(input), quant_mult_reg);
}

INTGEMM_SELECT_COL_B(INTGEMM_SSSE3, __m128i)

class QuantizeTile8 {
  public:
    INTGEMM_SSSE3 static inline Register ForReshape(FRegister mult_reg, const float *input, Index cols) {
      // Skip a row.
      return Tile(mult_reg, input, input + 4, input + 2 * cols, input + 2 * cols + 4);
    }

    INTGEMM_SSSE3 static inline Register Consecutive(FRegister mult_reg, const float *input) {
      return Tile(mult_reg, input, input + 4, input + 8, input + 12);
    }

    INTGEMM_SSSE3 static inline Register ConsecutiveU(FRegister mult_reg, const float *input) {
      return TileU(mult_reg, input, input + 4, input + 8, input + 12);
    }

    INTGEMM_SSSE3 static inline Register ConsecutiveWithWrapping(FRegister mult_reg, const float *input, Index cols_left, Index cols, Index row_step) {
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
      return Tile(mult_reg, inputs[0], inputs[1], inputs[2], inputs[3]);
    }

    // Quantize 16xfloat into 16xint8_t
    INTGEMM_SSSE3 static inline __m128i Tile(FRegister mult_reg, const float *input0, const float *input1, const float *input2, const float *input3) {
      const __m128i neg128 = _mm_set1_epi8(-128);
      __m128i g0 = QuantizerGrab(input0, mult_reg);
      __m128i g1 = QuantizerGrab(input1, mult_reg);
      __m128i g2 = QuantizerGrab(input2, mult_reg);
      __m128i g3 = QuantizerGrab(input3, mult_reg);
      __m128i packed0 = _mm_packs_epi32(g0, g1);
      __m128i packed1 = _mm_packs_epi32(g2, g3);
      __m128i packed = _mm_packs_epi16(packed0, packed1);
      /* Ban -128.
       * Don't use the SSE4.1 instruction _mm_max_epi8(packed, neg127).  Instead,
       * use SSE2 instructions _mm_cmpeq_epi8 and _mm_sub_epi8.
       * The first generates 0xff for fields -128.
       * The second subtracts 0xff from -128 which has the effect of converting
       * to -127.
       */
      // packed = _mm_max_epi8(packed, neg127);
      __m128i evils = _mm_cmpeq_epi8(packed, neg128);
      return _mm_sub_epi8(packed, evils);
      // No permute needed.  packs is in order for SSE.
    }

  private:
    INTGEMM_SSSE3 static inline __m128i TileU(FRegister mult_reg, const float *input0, const float *input1, const float *input2, const float *input3) {
      const __m128i neg128 = _mm_set1_epi8(-128);
      const __m128i pos127 = _mm_set1_epi8(127);
      __m128i g0 = QuantizerGrab(input0, mult_reg);
      __m128i g1 = QuantizerGrab(input1, mult_reg);
      __m128i g2 = QuantizerGrab(input2, mult_reg);
      __m128i g3 = QuantizerGrab(input3, mult_reg);
      __m128i packed0 = _mm_packs_epi32(g0, g1);
      __m128i packed1 = _mm_packs_epi32(g2, g3);
      __m128i packed = _mm_packs_epi16(packed0, packed1);
      /* Ban -128.
       * Don't use the SSE4.1 instruction _mm_max_epi8(packed, neg127).  Instead,
       * use SSE2 instructions _mm_cmpeq_epi8 and _mm_sub_epi8.
       * The first generates 0xff for fields -128.
       * The second subtracts 0xff from -128 which has the effect of converting
       * to -127.
       */
      // packed = _mm_max_epi8(packed, neg127);
      __m128i evils = _mm_cmpeq_epi8(packed, neg128);
      return _mm_add_epi8(_mm_sub_epi8(packed, evils), pos127);
      // No permute needed.  packs is in order for SSE.
    }
};

// pmaddubsw (the 8-bit multiply) is SSSE3, so pedantically that's the version we need.
struct Kernels8 {
  typedef int8_t Integer;

  // Currently A is prepared by quantization but this could theoretically change.
  INTGEMM_SSSE3 static inline void PrepareA(const float *input, int8_t *output, float quant_mult, Index rows, Index cols) {
    Quantize(input, output, quant_mult, rows * cols);
  }

 private:
  INTGEMM_QUANTIZE_THREAD(INTGEMM_SSSE3)
 public:
  INTGEMM_QUANTIZE(INTGEMM_SSSE3)

  // Version with unsigned int + 127
  // Currently A is prepared by quantization but this could theoretically change.
  INTGEMM_SSSE3 static inline void PrepareA(const float *input, uint8_t *output, float quant_mult, Index rows, Index cols) {
    QuantizeU(input, output, quant_mult, rows * cols);
  }

  INTGEMM_SSSE3 static void QuantizeU(const float *input, uint8_t *output, float quant_mult, Index size) {
    assert(size % 16 == 0);
    assert(reinterpret_cast<uintptr_t>(input) % 16 == 0);
    assert(reinterpret_cast<uintptr_t>(output) % 16 == 0);
    FRegister q = set1_ps<FRegister>(quant_mult);
    const float *end = input + size;
    for (; input != end; input += 16, output += 16) {
      *reinterpret_cast<__m128i*>(output) = QuantizeTile8::ConsecutiveU(q, input);
    }
  }

  // Tile size for B; B must be a multiple of this block size.
  static const Index kBTileRow = 16;
  static const Index kBTileCol = 8;

  INTGEMM_PREPARE_B_8(INTGEMM_SSSE3, SSSE3::QuantizeTile8)
  INTGEMM_PREPARE_B_QUANTIZED_TRANSPOSED(INTGEMM_SSSE3, int8_t)
  INTGEMM_PREPARE_B_TRANSPOSED(INTGEMM_SSSE3, QuantizeTile8, int8_t)

  INTGEMM_SSSE3 static void SelectColumnsB(const int8_t *input, int8_t *output, Index rows, const Index *cols_begin, const Index *cols_end) {
    SSSE3::SelectColumnsOfB((const __m128i*)input, (__m128i*)output, rows, cols_begin, cols_end);
  }

  INTGEMM_MULTIPLY8(__m128i, INTGEMM_SSSE3, CPUType::SSE2)

  INTGEMM_MULTIPLY8SHIFT(__m128i, INTGEMM_SSSE3, CPUType::SSE2)

  INTGEMM_PREPAREBIASFOR8(__m128i, INTGEMM_SSSE3, CPUType::SSE2)

  constexpr static const char *const kName = "8-bit SSSE3";

  static const CPUType kUses = CPUType::SSSE3;
};

} // namespace SSSE3
} // namespace intgemm
