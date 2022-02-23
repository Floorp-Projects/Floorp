#if defined(WASM)
// No header for CPUID since it's hard-coded.
#elif defined(__INTEL_COMPILER)
#include <immintrin.h>
#elif defined(_MSC_VER)
#include <intrin.h>
#else
// Assume GCC and clang style.
#include <cpuid.h>
#endif

#include "intgemm.h"
#include "stats.h"

#include <stdio.h>
#include <stdlib.h>

namespace intgemm {

namespace {

// Return the maximum CPU model that's found and supported at compile time.
CPUType RealCPUID() {
#if defined(WASM)
  // emscripten does SSE4.1 but we only use up to SSSE3.
  return CPUType::SSSE3;
#elif defined(__INTEL_COMPILER)
#  ifdef INTGEMM_COMPILER_SUPPORTS_AVX512VNNI
  if (_may_i_use_cpu_feature(_FEATURE_AVX512_VNNI)) return CPUType::AVX512VNNI;
#  endif
#  ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
  if (_may_i_use_cpu_feature(_FEATURE_AVX512BW)) return CPUType::AVX512BW;
#  endif
#  ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
  if (_may_i_use_cpu_feature(_FEATURE_AVX2)) return CPUType::AVX2;
#  endif
  if (_may_i_use_cpu_feature(_FEATURE_SSSE3)) return CPUType::SSSE3;
  if (_may_i_use_cpu_feature(_FEATURE_SSE2)) return CPUType::SSE2;
  return CPUType::UNSUPPORTED;
#else
// Not emscripten, not Intel compiler
#  if defined(_MSC_VER)
  int regs[4];
  int &eax = regs[0], &ebx = regs[1], &ecx = regs[2], &edx = regs[3];
  __cpuid(regs, 0);
  int m = eax;
#  else
  /* gcc and clang.
   * If intgemm is compiled by gcc 6.4.1 then dlopened into an executable
   * compiled by gcc 7.3.0, there will be a undefined symbol __cpu_info.
   * Work around this by calling the intrinsics more directly instead of
   * __builtin_cpu_supports.
   *
   * clang 6.0.0-1ubuntu2 supports vnni but doesn't have
   *   __builtin_cpu_supports("avx512vnni")
   * so use the hand-coded CPUID for clang.
   */
  unsigned int m = __get_cpuid_max(0, 0);
  unsigned int eax, ebx, ecx, edx;
#  endif
  if (m >= 7) {
#  if defined(_MSC_VER)
    __cpuid(regs, 7);
#  else
    __cpuid_count(7, 0, eax, ebx, ecx, edx);
#  endif
#  ifdef INTGEMM_COMPILER_SUPPORTS_AVX512VNNI
    if (ecx & (1 << 11)) return CPUType::AVX512VNNI;
#  endif
#  ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
    if (ebx & (1 << 30)) return CPUType::AVX512BW;
#  endif
#  ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
    if (ebx & (1 << 5)) return CPUType::AVX2;
#   endif
  }
  if (m >= 1) {
#  if defined(_MSC_VER)
    __cpuid(regs, 1);
#  else
    __cpuid_count(1, 0, eax, ebx, ecx, edx);
#  endif
    if (ecx & (1 << 9)) return CPUType::SSSE3;
    if (edx & (1 << 26)) return CPUType::SSE2;
  }
  return CPUType::UNSUPPORTED;
#endif
}

#ifdef INTGEMM_CPUID_ENVIRONMENT
CPUType EnvironmentCPUID() {
#  if defined(_MSC_VER)
  char env_override[11];
  size_t len = 0;
  if (getenv_s(&len, env_override, sizeof(env_override), "INTGEMM_CPUID")) return CPUType::AVX512VNNI;
  if (!len) return CPUType::AVX512VNNI;
#  else
  const char *env_override = getenv("INTGEMM_CPUID");
  if (!env_override) return CPUType::AVX512VNNI; /* This will be capped to actual ID */
#  endif
  if (!strcmp(env_override, "AVX512VNNI")) return CPUType::AVX512VNNI;
  if (!strcmp(env_override, "AVX512BW")) return CPUType::AVX512BW;
  if (!strcmp(env_override, "AVX2")) return CPUType::AVX2;
  if (!strcmp(env_override, "SSSE3")) return CPUType::SSSE3;
  if (!strcmp(env_override, "SSE2")) return CPUType::SSE2;
  fprintf(stderr, "Ignoring unrecognized INTGEMM_CPUID %s\n", env_override);
  return CPUType::AVX512VNNI;
}
#endif

} // namespace

CPUType GetCPUID() {
  static const CPUType kLocalCPU =
#ifdef INTGEMM_CPUID_ENVIRONMENT
    std::min(RealCPUID(), EnvironmentCPUID());
#else
    RealCPUID();
#endif
  return kLocalCPU;
}

const CPUType kCPU = GetCPUID();

void UnsupportedCPUError() {
#if (defined(_MSC_VER) && !defined(__clang__)) ? (_HAS_EXCEPTIONS) : (__EXCEPTIONS)
  throw UnsupportedCPU();
#else
  fprintf(stderr, "intgemm does not support this CPU.\n");
  abort();
#endif
}

float Unsupported_MaxAbsolute(const float * /*begin*/, const float * /*end*/) {
  UnsupportedCPUError();
  return 0.0f;
}

MeanStd Unsupported_VectorMeanStd(const float * /*begin*/, const float * /*end*/, bool /*absolute*/) {
  UnsupportedCPUError();
  return MeanStd();
}

void (*Int16::Quantize)(const float *input, int16_t *output, float quant_mult, Index size) = ChooseCPU(AVX512BW::Kernels16::Quantize, AVX512BW::Kernels16::Quantize, AVX2::Kernels16::Quantize, SSE2::Kernels16::Quantize, SSE2::Kernels16::Quantize, Unsupported_16bit::Quantize);

void (*Int16::PrepareB)(const float *input, int16_t *output, float quant_mult, Index rows, Index cols) = ChooseCPU(AVX512BW::Kernels16::PrepareB, AVX512BW::Kernels16::PrepareB, AVX2::Kernels16::PrepareB, SSE2::Kernels16::PrepareB, SSE2::Kernels16::PrepareB, Unsupported_16bit::PrepareB);

void (*Int16::PrepareBQuantizedTransposed)(const int16_t *input, int16_t *output, Index inner, Index B_untransposed_cols) = ChooseCPU(AVX512BW::Kernels16::PrepareBQuantizedTransposed, AVX512BW::Kernels16::PrepareBQuantizedTransposed, AVX2::Kernels16::PrepareBQuantizedTransposed, SSE2::Kernels16::PrepareBQuantizedTransposed, SSE2::Kernels16::PrepareBQuantizedTransposed, Unsupported_16bit::PrepareBQuantizedTransposed);

void (*Int16::PrepareBTransposed)(const float *input, int16_t *output, float quant_mult, Index inner, Index B_untransposed_cols) = ChooseCPU(AVX512BW::Kernels16::PrepareBTransposed, AVX512BW::Kernels16::PrepareBTransposed, AVX2::Kernels16::PrepareBTransposed, SSE2::Kernels16::PrepareBTransposed, SSE2::Kernels16::PrepareBTransposed, Unsupported_16bit::PrepareBTransposed);

void (*Int16::SelectColumnsB)(const int16_t *input, int16_t *output, Index rows, const Index *cols_begin, const Index *cols_end) = ChooseCPU(AVX512BW::Kernels16::SelectColumnsB, AVX512BW::Kernels16::SelectColumnsB, AVX2::Kernels16::SelectColumnsB, SSE2::Kernels16::SelectColumnsB, SSE2::Kernels16::SelectColumnsB, Unsupported_16bit::SelectColumnsB);

const char *const Int16::kName = ChooseCPU(AVX512BW::Kernels16::kName, AVX512BW::Kernels16::kName, AVX2::Kernels16::kName, SSE2::Kernels16::kName, SSE2::Kernels16::kName, Unsupported_16bit::kName);

void (*Int8::Quantize)(const float *input, int8_t *output, float quant_mult, Index size) = ChooseCPU(AVX512VNNI::Kernels8::Quantize, AVX512BW::Kernels8::Quantize, AVX2::Kernels8::Quantize, SSSE3::Kernels8::Quantize, Unsupported_8bit::Quantize, Unsupported_8bit::Quantize);

void (*Int8::QuantizeU)(const float *input, uint8_t *output, float quant_mult, Index size) = ChooseCPU(AVX512VNNI::Kernels8::QuantizeU, AVX512BW::Kernels8::QuantizeU, AVX2::Kernels8::QuantizeU, SSSE3::Kernels8::QuantizeU, Unsupported_8bit::QuantizeU, Unsupported_8bit::QuantizeU);

void (*Int8::PrepareB)(const float *input, int8_t *output, float quant_mult, Index rows, Index cols) = ChooseCPU(AVX512VNNI::Kernels8::PrepareB, AVX512BW::Kernels8::PrepareB, AVX2::Kernels8::PrepareB, SSSE3::Kernels8::PrepareB, Unsupported_8bit::PrepareB, Unsupported_8bit::PrepareB);

void (*Int8::PrepareBQuantizedTransposed)(const int8_t *input, int8_t *output, Index inner, Index B_untransposed_cols) = ChooseCPU(AVX512BW::Kernels8::PrepareBQuantizedTransposed, AVX512BW::Kernels8::PrepareBQuantizedTransposed, AVX2::Kernels8::PrepareBQuantizedTransposed, SSSE3::Kernels8::PrepareBQuantizedTransposed, Unsupported_8bit::PrepareBQuantizedTransposed, Unsupported_8bit::PrepareBQuantizedTransposed);

void (*Int8::PrepareBTransposed)(const float *input, int8_t *output, float quant_mult, Index inner, Index B_untransposed_cols) = ChooseCPU(AVX512BW::Kernels8::PrepareBTransposed, AVX512BW::Kernels8::PrepareBTransposed, AVX2::Kernels8::PrepareBTransposed, SSSE3::Kernels8::PrepareBTransposed, Unsupported_8bit::PrepareBTransposed, Unsupported_8bit::PrepareBTransposed);

void (*Int8::SelectColumnsB)(const int8_t *input, int8_t *output, Index rows, const Index *cols_begin, const Index *cols_end) = ChooseCPU(AVX512VNNI::Kernels8::SelectColumnsB, AVX512BW::Kernels8::SelectColumnsB, AVX2::Kernels8::SelectColumnsB, SSSE3::Kernels8::SelectColumnsB, Unsupported_8bit::SelectColumnsB, Unsupported_8bit::SelectColumnsB);

const char *const Int8::kName = ChooseCPU(AVX512VNNI::Kernels8::kName, AVX512BW::Kernels8::kName, AVX2::Kernels8::kName, SSSE3::Kernels8::kName, Unsupported_8bit::kName, Unsupported_8bit::kName);

void (*Int8Shift::QuantizeU)(const float *input, uint8_t *output, float quant_mult, Index size) = ChooseCPU(AVX512VNNI::Kernels8::QuantizeU, AVX512BW::Kernels8::QuantizeU, AVX2::Kernels8::QuantizeU, SSSE3::Kernels8::QuantizeU, Unsupported_8bit::QuantizeU, Unsupported_8bit::QuantizeU);

const char *const Int8Shift::kName = ChooseCPU(AVX512VNNI::Kernels8::kName, AVX512BW::Kernels8::kName, AVX2::Kernels8::kName, SSSE3::Kernels8::kName, Unsupported_8bit::kName, Unsupported_8bit::kName);

#if !defined(INTGEMM_COMPILER_SUPPORTS_AVX2)
namespace AVX2{
using SSE2::MaxAbsolute;
using SSE2::VectorMeanStd;
} // namespace AVX2
#endif
#if !defined(INTGEMM_COMPILER_SUPPORTS_AVX512BW)
namespace AVX512BW {
using AVX2::MaxAbsolute;
using AVX2::VectorMeanStd;
} // namespace AVX512BW
#endif

float (*MaxAbsolute)(const float *begin, const float *end) = ChooseCPU(AVX512BW::MaxAbsolute, AVX512BW::MaxAbsolute, AVX2::MaxAbsolute, SSE2::MaxAbsolute, SSE2::MaxAbsolute, Unsupported_MaxAbsolute);

MeanStd (*VectorMeanStd)(const float *begin, const float *end, bool absolute) = ChooseCPU(AVX512BW::VectorMeanStd, AVX512BW::VectorMeanStd, AVX2::VectorMeanStd, SSE2::VectorMeanStd, SSE2::VectorMeanStd, Unsupported_VectorMeanStd);

constexpr const char *const Unsupported_16bit::kName;
constexpr const char *const Unsupported_8bit::kName;
constexpr const char *const SSE2::Kernels16::kName;
constexpr const char *const SSSE3::Kernels8::kName;
#ifdef INTGEMM_COMPILER_SUPPORTS_AVX2
constexpr const char *const AVX2::Kernels8::kName;
constexpr const char *const AVX2::Kernels16::kName;
#endif
#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512BW
constexpr const char *const AVX512BW::Kernels8::kName;
constexpr const char *const AVX512BW::Kernels16::kName;
#endif
#ifdef INTGEMM_COMPILER_SUPPORTS_AVX512VNNI
constexpr const char *const AVX512VNNI::Kernels8::kName;
#endif

}
