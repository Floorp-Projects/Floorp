#pragma once
/* Main interface for integer matrix multiplication.
 *
 * We are computing C = A * B with an optional scaling factor.
 *
 * A is typically activations.
 * Rows a multiple of 1 (no restriction)
 * Columns a multiple of 64 for 8-bit or 32 for 16-bit.
 * Use PrepareA to prepare A for multiplication.  This is meant to be fast.
 *
 * B is typically fixed model parameters.
 * Rows a multiple of 64 for 8-bit or 32 for 16-bit.
 * Columns a multiple of: 8
 * Use PrepareB to prepare B for multiplication.  This is slower, with the
 * intention that it will be prepared once and remembered.
 *
 * C is row major.
 *
 * Once both A and B are prepared, call Multiply.
 *
 * All memory (A, B, and C in float or prepared form) must be 64-byte aligned.
 * It's easy to write code that works on your CPU with lower alignment, but
 * breaks on AVX512.
 *
 * When preparing, you provide a quantization multiplier.  Values will be
 * multiplied by this then rounded to an integer.
 * For 16-bit neural networks, Jacob Devlin recommends 1024.0.
 * For 8-bit, use 127 / largest absolute value.
 *
 * Note that quantization saturates.  However, 16-bit does accumulation in
 * 32-bit which can overflow if you use too big of a multiplier.
 *
 * The multiply routine expects an unquantization multiplier.
 * This should be unquant_mult = 1.0 / (A_quant_mult * B_quant_mult).
 * Where A_quant_mult is what you passed to PrepareA and B_quant_mult is what you
 * passed to PrepareB.
 *
 * Feel free to multiply in a scaling factor to compute C = \lambda A * B by
 * passing unquant_mult = \lambda / (A_quant_mult * B_quant_mult).
 */

#include <cstdint>

#include "types.h"
#include "sse2_gemm.h"
#include "ssse3_gemm.h"
#include "avx2_gemm.h"
#include "avx512_gemm.h"
#include "avx512vnni_gemm.h"

/* Dispatch to functions based on runtime CPUID.  This adds one call-by-variable to each call. */

namespace intgemm {

void UnsupportedCPUError();

struct Unsupported_16bit {
  static void Quantize(const float *, int16_t *, float, Index) {
    UnsupportedCPUError();
  }
  static void PrepareB(const float *, int16_t *, float, Index, Index) {
    UnsupportedCPUError();
  }
  static void PrepareBQuantizedTransposed(const int16_t *, int16_t *, Index, Index) {
    UnsupportedCPUError();
  }
  static void PrepareBTransposed(const float *, int16_t *, float, Index, Index) {
    UnsupportedCPUError();
  }
  static void SelectColumnsB(const int16_t *, int16_t *, Index, const Index *, const Index *) {
    UnsupportedCPUError();
  }
  template <typename Callback>
  static void Multiply(const int16_t *, const int16_t *, Index, Index, Index, Callback) {
    UnsupportedCPUError();
  }
  constexpr static const char *const kName = "16-bit Unsupported";
};

struct Unsupported_8bit {
  static void Quantize(const float *, int8_t *, float, Index) {
    UnsupportedCPUError();
  }
  static void QuantizeU(const float *, uint8_t *, float, Index) {
    UnsupportedCPUError();
  }
  static void PrepareA(const float *, int8_t *, float, Index, Index) {
    UnsupportedCPUError();
  }
  static void PrepareBQuantizedTransposed(const int8_t *, int8_t *, Index, Index) {
    UnsupportedCPUError();
  }
  static void PrepareBTransposed(const float *, int8_t *, float, Index, Index) {
    UnsupportedCPUError();
  }
  static void PrepareB(const float *, int8_t *, float, Index, Index) {
    UnsupportedCPUError();
  }
  template<class Callback>
  static void PrepareBias(const int8_t *, Index, Index, Callback) {
    UnsupportedCPUError();
  }
  static void SelectColumnsB(const int8_t *, int8_t *, Index, const Index *, const Index *) {
    UnsupportedCPUError();
  }
  template <typename Callback>
  static void Multiply(const int8_t *, const int8_t *, Index, Index, Index, Callback) {
    UnsupportedCPUError();
  }
  template<class Callback>
  static void Multiply8Shift(const uint8_t *, const int8_t *, Index, Index, Index, Callback) {
    UnsupportedCPUError();
  }

  constexpr static const char *const kName = "8-bit Unsupported";
};

#ifndef INTGEMM_COMPILER_SUPPORTS_AVX512VNNI
// These won't ever be called in this capacity, but it does let the code below compile.
namespace AVX512VNNI {
typedef Unsupported_8bit Kernels8;
} // namespace AVX512VNNI
#endif
#ifndef INTGEMM_COMPILER_SUPPORTS_AVX512BW
namespace AVX512BW {
typedef Unsupported_8bit Kernels8;
typedef Unsupported_16bit Kernels16;
} // namespace AVX512BW
#endif
#ifndef INTGEMM_COMPILER_SUPPORTS_AVX2
namespace AVX2 {
typedef Unsupported_8bit Kernels8;
typedef Unsupported_16bit Kernels16;
} // namespace AVX2
#endif

CPUType GetCPUID();

/* Returns:
 * axx512vnni if the CPU supports AVX512VNNI
 *
 * avx512bw if the CPU supports AVX512BW
 *
 * avx2 if the CPU supports AVX2
 *
 * ssse3 if the CPU supports SSSE3 (this distinction from SSE2 matters for 8-bit)
 *
 * sse2 if the CPU supports SSE2
 *
 * unsupported otherwise
 */
template <class T> T ChooseCPU(T avx512vnni, T avx512bw, T avx2, T ssse3, T sse2, T unsupported) {
  const T ret[] = {unsupported, sse2, ssse3, avx2, avx512bw, avx512vnni};
  return ret[(int)GetCPUID()];
}

struct TileInfo {
  const Index a_rows;
  const Index a_cols;
  const Index b_rows;
  const Index b_cols;
};

/*
 * 8-bit matrix multiplication
 */
struct Int8 {
  using Integer = int8_t;

  // A's size must be a multiple of 1x64, B's size must be a multiple of 64x8.
  static constexpr TileInfo tile_info{1, 64, 64, 8};

  // Currently A is prepared by quantization but this could theoretically change.
  // A's columns must be a multiple of 8.
  // The number of rows is anything.
  static inline void PrepareA(const float *input, int8_t *output, float quant_mult, Index rows, Index cols) {
    Quantize(input, output, quant_mult, rows * cols);
  }

  // Multiply floats by quant_mult then convert to 8-bit integers with saturation.
  static void (*Quantize)(const float *input, int8_t *output, float quant_mult, Index size);

  // Multiply floats by quant_mult then convert to 8-bit integers with saturation.
  // A version that adds 127 to each number, making sure that all numbers are positive
  static void (*QuantizeU)(const float *input, uint8_t *output, float quant_mult, Index size);

  // Warning: the output of PrepareB depends on the CPU.
  // It will match the Multiply function on the same CPU though.
  static void (*PrepareB)(const float *input, int8_t *output, float quant_mult, Index rows, Index cols);

  // Convert from a B that was already transposed (routine not provided) and
  // quantized (e.g. with Quantize) to the CPU-dependent format used for
  // Multiply.  This is useful for storing a quantized model on disk then in a
  // CPU-independent fashion.
  static void (*PrepareBQuantizedTransposed)(const int8_t *input, int8_t *output, Index inner, Index B_untransposed_cols);

  // Convert from a B that was already transposed (routine not provided) to
  // the CPU-dependent format used for Multiply.  This is useful for storing
  // a quantized model on disk then in a CPU-independent fashion.
  static void (*PrepareBTransposed)(const float *input, int8_t *output, float quant_mul, Index inner, Index B_untransposed_cols);

  // Select columns from a prepared B matrix.  The number of selected columns must be a multiple of 8.
  static void (*SelectColumnsB)(const int8_t *input, int8_t *output, Index rows, const Index *cols_begin, const Index *cols_end);

  // Multiply C = A * B, presuming A and B have been prepared.
  template <typename Callback>
  static void Multiply(const int8_t *A, const int8_t *B, Index A_rows, Index width, Index B_cols, Callback callback) {
    MultiplyImpl<Callback>::run(A, B, A_rows, width, B_cols, callback);
  }

  static const char *const kName;

private:
  template <typename Callback>
  struct MultiplyImpl {
    static void (*run)(const int8_t *A, const int8_t *B, Index A_rows, Index width, Index B_cols, Callback callback);
  };
};

template <typename Callback>
void (*Int8::MultiplyImpl<Callback>::run)(const int8_t *A, const int8_t *B, Index A_rows, Index width, Index B_cols, Callback callback) = ChooseCPU(OMPParallelWrap<Callback, AVX512VNNI::Kernels8>, OMPParallelWrap<Callback, AVX512BW::Kernels8>, OMPParallelWrap<Callback, AVX2::Kernels8>, OMPParallelWrap<Callback, SSSE3::Kernels8>, Unsupported_8bit::Multiply<Callback>, Unsupported_8bit::Multiply<Callback>);

/*
 * 8-bit matrix multiplication with shifting A by 127
 */
struct Int8Shift {
  using Integer = int8_t;

  // A's size must be a multiple of 1x64, B's size must be a multiple of 64x8.
  static constexpr TileInfo tile_info{1, 64, 64, 8};

  // Identical to the Int8 Version, except it adds 127 to each number, making sure that all numbers are positive.
  static inline void PrepareA(const float *input, int8_t *output, float quant_mult, Index rows, Index cols) {
    QuantizeU(input, reinterpret_cast<uint8_t *>(output), quant_mult, rows * cols);
  }

  // Multiply floats by quant_mult then convert to 8-bit integers with saturation.
  // A version that adds 127 to each number, making sure that all numbers are positive
  static void (*QuantizeU)(const float *input, uint8_t *output, float quant_mult, Index size);
  
  // Warning: the output of PrepareB depends on the CPU.
  // It will match the Multiply function on the same CPU though.
  static void PrepareB(const float *input, int8_t *output, float quant_mult, Index rows, Index cols) {
    Int8::PrepareB(input, output, quant_mult, rows, cols);
  }

  // Select columns from a prepared B matrix.  The number of selected columns must be a multiple of 8. 
  static void SelectColumnsB(const int8_t *input, int8_t *output, Index rows, const Index *cols_begin, const Index *cols_end) {
    Int8::SelectColumnsB(input, output, rows, cols_begin, cols_end);
  }

  // A slightly faster version compared to the Int8 one (assuming a bias is used) because of better handling of the sign bit
  // Multiply C = A * B + Bias, presuming A, B and Bias have all been prepared (for A, PrepareAnew should be used
  template<class Callback>
  static void Multiply(const int8_t *A, const int8_t *B, Index A_rows, Index width, Index B_cols, Callback callback) {
    MultiplyImpl<Callback>::run((const uint8_t *)A, B, A_rows, width, B_cols, callback);
  }

  // This function prepares the bias for the Multiply routine that does unsigned * signed multiplication.
  // The function takes:
  // a preparedB matrix, width, B_cols and
  // the callback UnquantizeAndAddBiasAndWrite(unquant_mult, Bias_matrix, Bias_matrix)
  // unquant_mult is computed by (-1)*(alpha)*(alpha)/(127.0f);
  template<class Callback>
  static void PrepareBias(const int8_t *B, Index width, Index B_cols, Callback callback) {
    PrepareBiasImpl<Callback>::run(B, width, B_cols, callback);
  }
  
  static const char *const kName;

private:
  template <typename Callback>
  struct MultiplyImpl {
    static void (*run)(const uint8_t *A, const int8_t *B, Index A_rows, Index width, Index B_cols, Callback callback);
  };

  template <typename Callback>
  struct PrepareBiasImpl {
    static void (*run)(const int8_t *B, Index width, Index B_cols, Callback callback);
  };
};

template <class Callback>
void (*Int8Shift::MultiplyImpl<Callback>::run)(const uint8_t *A, const int8_t *B, Index A_rows, Index width, Index B_cols, Callback callback) = ChooseCPU(
    OMPParallelWrap8Shift<Callback, AVX512VNNI::Kernels8>,
    OMPParallelWrap8Shift<Callback, AVX512BW::Kernels8>,
    OMPParallelWrap8Shift<Callback, AVX2::Kernels8>,
    OMPParallelWrap8Shift<Callback, SSSE3::Kernels8>, 
    Unsupported_8bit::Multiply8Shift<Callback>, Unsupported_8bit::Multiply8Shift<Callback>);

template <class Callback>
void (*Int8Shift::PrepareBiasImpl<Callback>::run)(const int8_t *B, Index width, Index B_cols, Callback callback) = ChooseCPU(AVX512VNNI::Kernels8::PrepareBias<Callback>, AVX512BW::Kernels8::PrepareBias<Callback>, AVX2::Kernels8::PrepareBias<Callback>, SSSE3::Kernels8::PrepareBias<Callback>, SSSE3::Kernels8::PrepareBias<Callback>, Unsupported_8bit::PrepareBias);

/*
 * 16-bit matrix multiplication
 */
struct Int16 {
  using Integer = int16_t;

  // A's size must be a multiple of 1x32, B's size must be a multiple of 32x8.
  static constexpr TileInfo tile_info{1, 32, 32, 8};

  // Currently A is prepared by quantization but this could theoretically change.
  // A's columns must be a multiple of 8.
  // The number of rows is anything.
  static inline void PrepareA(const float *input, int16_t *output, float quant_mult, Index rows, Index cols) {
    Quantize(input, output, quant_mult, rows * cols);
  }

  // Multiply floats by quant_mult then convert to 16-bit integers with saturation.
  // input
  static void (*Quantize)(const float *input, int16_t *output, float quant_mult, Index size);

  // Warning: the output of PrepareB depends on the CPU.
  // It will match the Multiply function on the same CPU though.
  static void (*PrepareB)(const float *input, int16_t *output, float quant_mult, Index rows, Index cols);

  // Convert from a B that was already transposed (routine not provided) and
  // quantized (e.g. with Quantize) to the CPU-dependent format used for
  // Multiply.  This is useful for storing a quantized model on disk then in a
  // CPU-independent fashion.
  static void (*PrepareBQuantizedTransposed)(const int16_t *input, int16_t *output, Index inner, Index B_untransposed_cols);

  // Convert from a B that was already transposed (routine not provided) to
  // the CPU-dependent format used for Multiply.  This is useful for storing
  // a quantized model on disk then in a CPU-independent fashion.
  static void (*PrepareBTransposed)(const float *input, int16_t *output, float quant_mul, Index inner, Index B_untransposed_cols);

  // Select columns from a prepared B matrix.  The number of selected columns must be a multiple of 8. 
  static void (*SelectColumnsB)(const int16_t *input, int16_t *output, Index rows, const Index *cols_begin, const Index *cols_end);

  // Multiply C = A * B, presuming A and B have been prepared.
  template <typename Callback>
  static void Multiply(const int16_t *A, const int16_t *B, Index A_rows, Index width, Index B_cols, Callback callback) {
    MultiplyImpl<Callback>::run(A, B, A_rows, width, B_cols, callback);
  }

  static const char *const kName;

private:
  template <typename Callback>
  struct MultiplyImpl {
    static void (*run)(const int16_t *A, const int16_t *B, Index A_rows, Index width, Index B_cols, Callback callback);
  };
};

template <typename Callback>
void (*Int16::MultiplyImpl<Callback>::run)(const int16_t *A, const int16_t *B, Index A_rows, Index width, Index B_cols, Callback callback) = ChooseCPU(OMPParallelWrap<Callback, AVX512BW::Kernels16> /*TODO VNNI 16-bit. */, OMPParallelWrap<Callback, AVX512BW::Kernels16>, OMPParallelWrap<Callback, AVX2::Kernels16>, OMPParallelWrap<Callback, SSE2::Kernels16>, OMPParallelWrap<Callback, SSE2::Kernels16>, Unsupported_16bit::Multiply<Callback>);

extern const CPUType kCPU;

// Get the maximum absolute value of an array of floats. The number of floats must be a multiple of 16 and 64-byte aligned.
extern float (*MaxAbsolute)(const float *begin, const float *end);

// Get a Quantization value that is equant to the mean of the data +N standard deviations. Use 2 by default
extern MeanStd (*VectorMeanStd)(const float *begin, const float *end, bool);

/* Returns the Mean and the Standard deviation of a vector. 
 * If "absolute" is set to true, it computes the mean and the standard deviation of the absolute values of the vector */
static inline MeanStd GetVectorMeanStd(const float * begin, const float * end, bool absolute=false) {
  return VectorMeanStd(begin, end, absolute);
}


} // namespace intgemm
