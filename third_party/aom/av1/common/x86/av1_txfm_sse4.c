#include "config/aom_dsp_rtcd.h"

#include "av1/common/av1_txfm.h"
#include "av1/common/x86/av1_txfm_sse4.h"

void av1_round_shift_array_sse4_1(int32_t *arr, int size, int bit) {
  __m128i *const vec = (__m128i *)arr;
  const int vec_size = size >> 2;
  av1_round_shift_array_32_sse4_1(vec, vec, vec_size, bit);
}
