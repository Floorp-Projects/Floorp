// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/enc_linalg.h"

#include <cstddef>

#include "lib/jxl/base/random.h"
#include "lib/jxl/testing.h"

namespace jxl {
namespace {

Matrix2x2 Diagonal(const Vector2& d) { return {{{d[0], 0.0}, {0.0, d[1]}}}; }

Matrix2x2 Identity() { return Diagonal({1.0, 1.0}); }

Matrix2x2 MatMul(const Matrix2x2& A, const Matrix2x2& B) {
  Matrix2x2 out;
  for (size_t y = 0; y < 2; ++y) {
    for (size_t x = 0; x < 2; ++x) {
      out[y][x] = A[0][x] * B[y][0] + A[1][x] * B[y][1];
    }
  }
  return out;
}

Matrix2x2 Transpose(const Matrix2x2& A) {
  return {{{A[0][0], A[1][0]}, {A[0][1], A[1][1]}}};
}

Matrix2x2 RandomSymmetricMatrix(Rng& rng, const double vmin,
                                const double vmax) {
  Matrix2x2 A;
  A[0][0] = rng.UniformF(vmin, vmax);
  A[0][1] = A[1][0] = rng.UniformF(vmin, vmax);
  A[1][1] = rng.UniformF(vmin, vmax);
  return A;
}

void VerifyMatrixEqual(const Matrix2x2& A, const Matrix2x2& B,
                       const double eps) {
  for (size_t y = 0; y < 2; ++y) {
    for (size_t x = 0; x < 2; ++x) {
      ASSERT_NEAR(A[y][x], B[y][x], eps);
    }
  }
}

void VerifyOrthogonal(const Matrix2x2& A, const double eps) {
  VerifyMatrixEqual(Identity(), MatMul(Transpose(A), A), eps);
}

TEST(LinAlgTest, ConvertToDiagonal) {
  {
    Matrix2x2 I = Identity();
    Matrix2x2 U;
    Vector2 d;
    ConvertToDiagonal(I, d, U);
    VerifyMatrixEqual(I, U, 1e-15);
    for (size_t k = 0; k < 2; ++k) {
      ASSERT_NEAR(d[k], 1.0, 1e-15);
    }
  }
  {
    Matrix2x2 A = Identity();
    A[0][1] = A[1][0] = 2.0;
    Matrix2x2 U;
    Vector2 d;
    ConvertToDiagonal(A, d, U);
    VerifyOrthogonal(U, 1e-12);
    VerifyMatrixEqual(A, MatMul(U, MatMul(Diagonal(d), Transpose(U))), 1e-12);
  }
  {
    Matrix2x2 A;
    A[0] = {0.000208649, 1.13687e-12};
    A[1] = {1.13687e-12, 0.000208649};
    Matrix2x2 U;
    Vector2 d;
    ConvertToDiagonal(A, d, U);
    VerifyOrthogonal(U, 1e-12);
    VerifyMatrixEqual(A, MatMul(U, MatMul(Diagonal(d), Transpose(U))), 1e-11);
  }
  Rng rng(0);
  for (size_t i = 0; i < 1000000; ++i) {
    Matrix2x2 A = RandomSymmetricMatrix(rng, -1.0, 1.0);
    Matrix2x2 U;
    Vector2 d;
    ConvertToDiagonal(A, d, U);
    VerifyOrthogonal(U, 1e-12);
    VerifyMatrixEqual(A, MatMul(U, MatMul(Diagonal(d), Transpose(U))), 5e-10);
  }
}

}  // namespace
}  // namespace jxl
