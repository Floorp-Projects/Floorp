#pragma once

#include "intgemm/intgemm_config.h"
#include "intrinsics.h"
#include "types.h"
#include "utils.h"
#include "vec_traits.h"

#include <cstdlib>

#define KERNELS_THIS_IS_SSE2
#include "kernels/implementations.inl"
#undef KERNELS_THIS_IS_SSE2

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
#define KERNELS_THIS_IS_AVX2
#include "kernels/implementations.inl"
#undef KERNELS_THIS_IS_AVX2
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
#define KERNELS_THIS_IS_AVX512BW
#include "kernels/implementations.inl"
#undef KERNELS_THIS_IS_AVX512BW
#endif

