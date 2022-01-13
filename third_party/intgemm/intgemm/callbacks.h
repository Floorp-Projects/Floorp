#pragma once

#include "callbacks/configs.h"
#include "callbacks/output_buffer_info.h"

#include "intgemm/intgemm_config.h"
#include "intrinsics.h"
#include "kernels.h"
#include "types.h"
#include "utils.h"
#include "vec_traits.h"

#define CALLBACKS_THIS_IS_SSE2
#include "callbacks/implementations.inl"
#undef CALLBACKS_THIS_IS_SSE2

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
#define CALLBACKS_THIS_IS_AVX2
#include "callbacks/implementations.inl"
#undef CALLBACKS_THIS_IS_AVX2
#endif

#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
#define CALLBACKS_THIS_IS_AVX512BW
#include "callbacks/implementations.inl"
#undef CALLBACKS_THIS_IS_AVX512BW
#endif

