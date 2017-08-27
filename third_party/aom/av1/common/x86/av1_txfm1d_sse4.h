#ifndef AV1_TXMF1D_SSE2_H_
#define AV1_TXMF1D_SSE2_H_

#include <smmintrin.h>
#include "av1/common/av1_txfm.h"

#ifdef __cplusplus
extern "C" {
#endif

void av1_fdct4_new_sse4_1(const __m128i *input, __m128i *output,
                          const int8_t *cos_bit, const int8_t *stage_range);
void av1_fdct8_new_sse4_1(const __m128i *input, __m128i *output,
                          const int8_t *cos_bit, const int8_t *stage_range);
void av1_fdct16_new_sse4_1(const __m128i *input, __m128i *output,
                           const int8_t *cos_bit, const int8_t *stage_range);
void av1_fdct32_new_sse4_1(const __m128i *input, __m128i *output,
                           const int8_t *cos_bit, const int8_t *stage_range);
void av1_fdct64_new_sse4_1(const __m128i *input, __m128i *output,
                           const int8_t *cos_bit, const int8_t *stage_range);

void av1_fadst4_new_sse4_1(const __m128i *input, __m128i *output,
                           const int8_t *cos_bit, const int8_t *stage_range);
void av1_fadst8_new_sse4_1(const __m128i *input, __m128i *output,
                           const int8_t *cos_bit, const int8_t *stage_range);
void av1_fadst16_new_sse4_1(const __m128i *input, __m128i *output,
                            const int8_t *cos_bit, const int8_t *stage_range);
void av1_fadst32_new_sse4_1(const __m128i *input, __m128i *output,
                            const int8_t *cos_bit, const int8_t *stage_range);

void av1_idct4_new_sse4_1(const __m128i *input, __m128i *output,
                          const int8_t *cos_bit, const int8_t *stage_range);
void av1_idct8_new_sse4_1(const __m128i *input, __m128i *output,
                          const int8_t *cos_bit, const int8_t *stage_range);
void av1_idct16_new_sse4_1(const __m128i *input, __m128i *output,
                           const int8_t *cos_bit, const int8_t *stage_range);
void av1_idct32_new_sse4_1(const __m128i *input, __m128i *output,
                           const int8_t *cos_bit, const int8_t *stage_range);
void av1_idct64_new_sse4_1(const __m128i *input, __m128i *output,
                           const int8_t *cos_bit, const int8_t *stage_range);

void av1_iadst4_new_sse4_1(const __m128i *input, __m128i *output,
                           const int8_t *cos_bit, const int8_t *stage_range);
void av1_iadst8_new_sse4_1(const __m128i *input, __m128i *output,
                           const int8_t *cos_bit, const int8_t *stage_range);
void av1_iadst16_new_sse4_1(const __m128i *input, __m128i *output,
                            const int8_t *cos_bit, const int8_t *stage_range);
void av1_iadst32_new_sse4_1(const __m128i *input, __m128i *output,
                            const int8_t *cos_bit, const int8_t *stage_range);

static INLINE void transpose_32_4x4(int stride, const __m128i *input,
                                    __m128i *output) {
  __m128i temp0 = _mm_unpacklo_epi32(input[0 * stride], input[2 * stride]);
  __m128i temp1 = _mm_unpackhi_epi32(input[0 * stride], input[2 * stride]);
  __m128i temp2 = _mm_unpacklo_epi32(input[1 * stride], input[3 * stride]);
  __m128i temp3 = _mm_unpackhi_epi32(input[1 * stride], input[3 * stride]);

  output[0 * stride] = _mm_unpacklo_epi32(temp0, temp2);
  output[1 * stride] = _mm_unpackhi_epi32(temp0, temp2);
  output[2 * stride] = _mm_unpacklo_epi32(temp1, temp3);
  output[3 * stride] = _mm_unpackhi_epi32(temp1, temp3);
}

// the entire input block can be represent by a grid of 4x4 blocks
// each 4x4 blocks can be represent by 4 vertical __m128i
// we first transpose each 4x4 block internally
// then transpose the grid
static INLINE void transpose_32(int txfm_size, const __m128i *input,
                                __m128i *output) {
  const int num_per_128 = 4;
  const int row_size = txfm_size;
  const int col_size = txfm_size / num_per_128;
  int r, c;

  // transpose each 4x4 block internally
  for (r = 0; r < row_size; r += 4) {
    for (c = 0; c < col_size; c++) {
      transpose_32_4x4(col_size, &input[r * col_size + c],
                       &output[c * 4 * col_size + r / 4]);
    }
  }
}

static INLINE __m128i round_shift_32_sse4_1(__m128i vec, int bit) {
  __m128i tmp, round;
  round = _mm_set1_epi32(1 << (bit - 1));
  tmp = _mm_add_epi32(vec, round);
  return _mm_srai_epi32(tmp, bit);
}

static INLINE void round_shift_array_32_sse4_1(__m128i *input, __m128i *output,
                                               const int size, const int bit) {
  if (bit > 0) {
    int i;
    for (i = 0; i < size; i++) {
      output[i] = round_shift_32_sse4_1(input[i], bit);
    }
  } else {
    int i;
    for (i = 0; i < size; i++) {
      output[i] = _mm_slli_epi32(input[i], -bit);
    }
  }
}

// out0 = in0*w0 + in1*w1
// out1 = -in1*w0 + in0*w1
#define btf_32_sse4_1_type0(w0, w1, in0, in1, out0, out1, bit) \
  do {                                                         \
    __m128i ww0, ww1, in0_w0, in1_w1, in0_w1, in1_w0;          \
    ww0 = _mm_set1_epi32(w0);                                  \
    ww1 = _mm_set1_epi32(w1);                                  \
    in0_w0 = _mm_mullo_epi32(in0, ww0);                        \
    in1_w1 = _mm_mullo_epi32(in1, ww1);                        \
    out0 = _mm_add_epi32(in0_w0, in1_w1);                      \
    out0 = round_shift_32_sse4_1(out0, bit);                   \
    in0_w1 = _mm_mullo_epi32(in0, ww1);                        \
    in1_w0 = _mm_mullo_epi32(in1, ww0);                        \
    out1 = _mm_sub_epi32(in0_w1, in1_w0);                      \
    out1 = round_shift_32_sse4_1(out1, bit);                   \
  } while (0)

// out0 = in0*w0 + in1*w1
// out1 = in1*w0 - in0*w1
#define btf_32_sse4_1_type1(w0, w1, in0, in1, out0, out1, bit) \
  do {                                                         \
    __m128i ww0, ww1, in0_w0, in1_w1, in0_w1, in1_w0;          \
    ww0 = _mm_set1_epi32(w0);                                  \
    ww1 = _mm_set1_epi32(w1);                                  \
    in0_w0 = _mm_mullo_epi32(in0, ww0);                        \
    in1_w1 = _mm_mullo_epi32(in1, ww1);                        \
    out0 = _mm_add_epi32(in0_w0, in1_w1);                      \
    out0 = round_shift_32_sse4_1(out0, bit);                   \
    in0_w1 = _mm_mullo_epi32(in0, ww1);                        \
    in1_w0 = _mm_mullo_epi32(in1, ww0);                        \
    out1 = _mm_sub_epi32(in1_w0, in0_w1);                      \
    out1 = round_shift_32_sse4_1(out1, bit);                   \
  } while (0)

#ifdef __cplusplus
}
#endif

#endif  // AV1_TXMF1D_SSE2_H_
