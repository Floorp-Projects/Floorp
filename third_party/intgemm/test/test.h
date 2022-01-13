#pragma once

#include "intgemm/intgemm_config.h"

#include "3rd_party/catch.hpp"
#include "../intgemm/intgemm.h"
#include "../intgemm/aligned.h"

#include <cmath>
#include <sstream>
#include <iostream>
#include <iomanip>

#define CHECK_MESSAGE(cond, msg) do { INFO(msg); CHECK(cond); } while(0)
#define CHECK_FALSE_MESSAGE(cond, msg) do { INFO(msg); CHECK_FALSE(cond); } while(0)
#define REQUIRE_MESSAGE(cond, msg) do { INFO(msg); REQUIRE(cond); } while(0)
#define REQUIRE_FALSE_MESSAGE(cond, msg) do { INFO(msg); REQUIRE_FALSE(cond); } while(0)

#define CHECK_EPS(actual, expected, epsilon) \
  do { \
    if (std::fabs((actual) - (expected)) < epsilon) { SUCCEED(); } \
    else { CHECK((actual) == (expected)); } \
  } while(0)

#define KERNEL_TEST_CASE(name) TEST_CASE("Kernel: " name, "[kernel_test]")

namespace intgemm {

template <typename Type>
void Compare(const Type* reference, const Type* actual, Index size) {
  for (Index i = 0; i < size; ++i) {
    INFO("Inaccurate at " << i << ' ' << reference[i] << ' ' << actual[i]);
    CHECK(reference[i] == actual[i]);
  }
}

template <typename Type>
void CompareEps(const Type* reference, const Type* actual, Index size, Type epsilon) {
  for (Index i = 0; i < size; ++i) {
    INFO("Inaccurate at " << i << ' ' << reference[i] << ' ' << actual[i]);
    // Ratio to maximum value.
    float threshold = epsilon * std::max<float>(0.01f, std::fabs(reference[i]));
    CHECK(std::fabs(reference[i] - actual[i]) < threshold);
  }
}

void CompareMSE(const float *float_ref, const float *int_ref, const float *int_test,
                std::size_t size, std::string test_info, float int_tolerance,
                float float_tolerance, float MSE_float_tolerance, float MSE_int_tolerance);

template <typename Type>
std::string PrintMatrix(const Type *mem, Index rows, Index cols) {
  std::ostringstream out;
  for (Index r = 0; r < rows; ++r) {
    for (Index c = 0; c < cols; ++c) {
      out << std::setw(4) << (int64_t) mem[r * cols + c] << ' ';
    }
    out << '\n';
  }
  return out.str();
}

/*
 * References
 */
namespace references {

// Quantize
template <typename Type>
void Quantize(const float* input, Type* output, float quant_mult, Index size) {
  for (Index i = 0; i < size; ++i) {
    float value = roundf(input[i] * quant_mult);
    value = std::max<float>(std::numeric_limits<Type>::min(), value);
    value = std::min<float>(std::numeric_limits<Type>::max(), value);
    output[i] = value;
  }
}

/*
 * Multiply C = A x B
 *
 * Notes: A and B has to be both integers or both floating points.
 *
 * Callback takes two arguments:
 *   - Intermediate value of multiplication 1 row times 1 column - it's int32_t or double based on types A and B.
 *   - Object containing information about position in output matrix - callbacks::OutputBufferInfo.
 */
template <typename TypeA, typename TypeB, typename TypeC, typename LambdaCallback,
          typename std::enable_if<
            (std::is_integral<TypeA>::value && std::is_integral<TypeB>::value) ||
            (std::is_floating_point<TypeA>::value && std::is_floating_point<TypeB>::value)
          >::type* = nullptr>
void Multiply(const TypeA* A, const TypeB* B, TypeC* C, Index A_rows, Index width, Index B_cols, LambdaCallback callback) {
  using IntermediateType = typename std::conditional<std::is_integral<TypeA>::value, int32_t, double>::type;

  for (Index r = 0; r < A_rows; ++r) {
    for (Index c = 0; c < B_cols; ++c) {
      IntermediateType sum = 0;
      for (Index k = 0; k < width; ++k) {
        sum += IntermediateType(A[r * width + k]) * IntermediateType(B[k * B_cols + c]);
      }
      C[r * B_cols + c] = callback(sum, {r, c, A_rows, B_cols});
    }
  }
}

// Matrix rearragement
template <typename Type>
void Rearragement(const Type* input, Type* output, Index simd, Index unroll, Index rows, Index cols) {
  for (Index c = 0; c < cols; c += unroll) {
    for (Index r = 0; r < rows; r += simd) {
      for (Index i = 0; i < unroll; ++i)
        for (Index j = 0; j < simd; ++j)
          output[simd * i + j] = input[cols * r + c + cols * j + i];

      output += unroll * simd;
    }
  }
}

// Transpose
template <typename Type>
void Transpose(const Type* input, Type* output, Index rows, Index cols) {
  for (Index r = 0; r < rows; ++r) {
    for (Index c = 0; c < cols; ++c) {
      output[rows * c + r] = input[cols * r + c];
    }
  }
}

} // namespace references
} // namespace intgemm
