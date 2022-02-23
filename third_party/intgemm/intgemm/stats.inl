/* This file is included multiple times, once per architecture. */
#if defined(INTGEMM_THIS_IS_AVX512DQ)
#define INTGEMM_ARCH AVX512BW
#define INTGEMM_TARGET INTGEMM_AVX512DQ
#elif defined(INTGEMM_THIS_IS_AVX2)
#define INTGEMM_ARCH AVX2
#define INTGEMM_TARGET INTGEMM_AVX2
#elif defined(INTGEMM_THIS_IS_SSE2)
#define INTGEMM_ARCH SSE2
#define INTGEMM_TARGET INTGEMM_SSE2
#else
#error Included with unexpected architecture
#endif

namespace intgemm {
namespace INTGEMM_ARCH {

/* Compute the maximum absolute value over floats aligned to register size.
 * Do not call this function directly; it's a subroutine of MaxAbsolute.
 */
INTGEMM_TARGET static inline float MaxAbsoluteThread(const FRegister *begin, const FRegister *end) {
  FRegister highest = setzero_ps<FRegister>();
  const FRegister abs_mask = cast_ps(set1_epi32<Register>(kFloatAbsoluteMask));
#pragma omp for
  for (const FRegister *i = begin; i < end; ++i) {
    FRegister reg = and_ps(abs_mask, *i);
    highest = max_ps(highest, reg);
  }
  return MaxFloat32(highest);
}

/* Compute the maximum absolute value of an array of floats.
 * begin_float must be aligned to a multiple of the register size.
*/
INTGEMM_TARGET static inline float MaxAbsolute(const float *begin_float, const float *end_float) {
  assert(reinterpret_cast<uintptr_t>(begin_float) % sizeof(FRegister) == 0);
  const float *end_reg = end_float - (reinterpret_cast<uintptr_t>(end_float) % sizeof(FRegister)) / sizeof(float);
  float ret = 0.0;
#pragma omp parallel reduction(max:ret) num_threads(std::max<int>(1, std::min<int>(omp_get_max_threads(), (end_float - begin_float) / 16384)))
  {
    float shard_max = MaxAbsoluteThread(
        reinterpret_cast<const FRegister*>(begin_float),
        reinterpret_cast<const FRegister*>(end_reg));
    ret = std::max(ret, shard_max);
  }
  /* Overhang. The beginning was aligned so if there's any overhang we're
   * allowed to read the next full register.  Then mask that to 0. */
#if defined(INTGEMM_THIS_IS_AVX512DQ)
  if (end_float != end_reg) {
    const FRegister abs_mask = cast_ps(set1_epi32<Register>(kFloatAbsoluteMask));
    __mmask16 mask = (1 << (end_float - end_reg)) - 1;
    FRegister masked = _mm512_maskz_and_ps(mask, abs_mask, *reinterpret_cast<const FRegister*>(end_reg));
    ret = std::max(ret, MaxFloat32(masked));
  }
#else
  for (const float *i = end_reg; i < end_float; ++i) {
    ret = std::max(ret, std::fabs(*i));
  }
#endif
  return ret;
}

/* Computes the euclidean norm and returns the mean and the standard deviation. Optionally it can be the mean and standard deviation in absolute terms. */
INTGEMM_TARGET static inline MeanStd VectorMeanStd(const float *begin_float, const float *end_float, bool absolute) {
  assert(end_float > begin_float);
  assert((end_float - begin_float) % (sizeof(FRegister) / sizeof(float)) == 0);
  size_t num_items = end_float - begin_float;
  const FRegister *begin = reinterpret_cast<const FRegister*>(begin_float);
  const FRegister *end = reinterpret_cast<const FRegister*>(end_float);
  FRegister squares = set1_ps<FRegister>(0);
  FRegister sums = set1_ps<FRegister>(0);
  if (absolute) {
    const FRegister abs_mask = cast_ps(set1_epi32<Register>(kFloatAbsoluteMask));
    for (; begin != end; begin++) {
      FRegister vec = and_ps(abs_mask, *begin);
      squares = add_ps(squares, mul_ps(vec, vec));
      sums = add_ps(sums, vec);
    }
  } else {
    for (; begin != end; begin++) {
      FRegister vec = *begin;
      squares = add_ps(squares, mul_ps(vec, vec));
      sums = add_ps(sums, vec);
    }
  }
  float squares_sum = AddFloat32(squares);
  float normal_sums = AddFloat32(sums);
  MeanStd ret;
  ret.mean = normal_sums/num_items;
  ret.stddev = std::sqrt((squares_sum/num_items) - (ret.mean*ret.mean));
  return ret;
}

} // namespace INTGEMM_ARCH
} // namespace intgemm

#undef INTGEMM_ARCH
#undef INTGEMM_TARGET
