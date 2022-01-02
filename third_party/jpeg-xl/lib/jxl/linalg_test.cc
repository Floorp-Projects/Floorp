// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

#include "lib/jxl/linalg.h"

#include "gtest/gtest.h"
#include "lib/jxl/image_test_utils.h"

namespace jxl {
namespace {

template <typename T>
Plane<T> RandomMatrix(const size_t xsize, const size_t ysize, Rng& rng,
                      const T vmin, const T vmax) {
  Plane<T> A(xsize, ysize);
  GenerateImage(rng, &A, vmin, vmax);
  return A;
}

template <typename T>
Plane<T> RandomSymmetricMatrix(const size_t N, Rng& rng, const T vmin,
                               const T vmax) {
  Plane<T> A = RandomMatrix<T>(N, N, rng, vmin, vmax);
  for (size_t i = 0; i < N; ++i) {
    for (size_t j = 0; j < i; ++j) {
      A.Row(j)[i] = A.Row(i)[j];
    }
  }
  return A;
}
void VerifyMatrixEqual(const ImageD& A, const ImageD& B, const double eps) {
  ASSERT_EQ(A.xsize(), B.xsize());
  ASSERT_EQ(A.ysize(), B.ysize());
  for (size_t y = 0; y < A.ysize(); ++y) {
    for (size_t x = 0; x < A.xsize(); ++x) {
      ASSERT_NEAR(A.Row(y)[x], B.Row(y)[x], eps);
    }
  }
}

void VerifyOrthogonal(const ImageD& A, const double eps) {
  VerifyMatrixEqual(Identity<double>(A.xsize()), MatMul(Transpose(A), A), eps);
}

void VerifyTridiagonal(const ImageD& T, const double eps) {
  ASSERT_EQ(T.xsize(), T.ysize());
  for (size_t i = 0; i < T.xsize(); ++i) {
    for (size_t j = i + 2; j < T.xsize(); ++j) {
      ASSERT_NEAR(T.Row(i)[j], 0.0, eps);
      ASSERT_NEAR(T.Row(j)[i], 0.0, eps);
    }
  }
}

void VerifyUpperTriangular(const ImageD& R, const double eps) {
  ASSERT_EQ(R.xsize(), R.ysize());
  for (size_t i = 0; i < R.xsize(); ++i) {
    for (size_t j = i + 1; j < R.xsize(); ++j) {
      ASSERT_NEAR(R.Row(i)[j], 0.0, eps);
    }
  }
}

TEST(LinAlgTest, ConvertToTridiagonal) {
  {
    ImageD I = Identity<double>(5);
    ImageD T, U;
    ConvertToTridiagonal(I, &T, &U);
    VerifyMatrixEqual(I, T, 1e-15);
    VerifyMatrixEqual(I, U, 1e-15);
  }
  {
    ImageD A = Identity<double>(5);
    A.Row(0)[1] = A.Row(1)[0] = 2.0;
    A.Row(0)[4] = A.Row(4)[0] = 3.0;
    A.Row(2)[3] = A.Row(3)[2] = 2.0;
    A.Row(3)[4] = A.Row(4)[3] = 2.0;
    ImageD U, d;
    ConvertToDiagonal(A, &d, &U);
    VerifyOrthogonal(U, 1e-12);
    VerifyMatrixEqual(A, MatMul(U, MatMul(Diagonal(d), Transpose(U))), 1e-12);
  }
  Rng rng(0);
  for (int N = 2; N < 100; ++N) {
    ImageD A = RandomSymmetricMatrix(N, rng, -1.0, 1.0);
    ImageD T, U;
    ConvertToTridiagonal(A, &T, &U);
    VerifyOrthogonal(U, 1e-12);
    VerifyTridiagonal(T, 1e-12);
    VerifyMatrixEqual(A, MatMul(U, MatMul(T, Transpose(U))), 1e-12);
  }
}

TEST(LinAlgTest, ConvertToDiagonal) {
  {
    ImageD I = Identity<double>(5);
    ImageD U, d;
    ConvertToDiagonal(I, &d, &U);
    VerifyMatrixEqual(I, U, 1e-15);
    for (int k = 0; k < 5; ++k) {
      ASSERT_NEAR(d.Row(0)[k], 1.0, 1e-15);
    }
  }
  {
    ImageD A = Identity<double>(5);
    A.Row(0)[1] = A.Row(1)[0] = 2.0;
    A.Row(2)[3] = A.Row(3)[2] = 2.0;
    A.Row(3)[4] = A.Row(4)[3] = 2.0;
    ImageD U, d;
    ConvertToDiagonal(A, &d, &U);
    VerifyOrthogonal(U, 1e-12);
    VerifyMatrixEqual(A, MatMul(U, MatMul(Diagonal(d), Transpose(U))), 1e-12);
  }
  Rng rng(0);
  for (int N = 2; N < 100; ++N) {
    ImageD A = RandomSymmetricMatrix(N, rng, -1.0, 1.0);
    ImageD U, d;
    ConvertToDiagonal(A, &d, &U);
    VerifyOrthogonal(U, 1e-12);
    VerifyMatrixEqual(A, MatMul(U, MatMul(Diagonal(d), Transpose(U))), 1e-12);
  }
}

TEST(LinAlgTest, ComputeQRFactorization) {
  {
    ImageD I = Identity<double>(5);
    ImageD Q, R;
    ComputeQRFactorization(I, &Q, &R);
    VerifyMatrixEqual(I, Q, 1e-15);
    VerifyMatrixEqual(I, R, 1e-15);
  }
  Rng rng(0);
  for (int N = 2; N < 100; ++N) {
    ImageD A = RandomMatrix(N, N, rng, -1.0, 1.0);
    ImageD Q, R;
    ComputeQRFactorization(A, &Q, &R);
    VerifyOrthogonal(Q, 1e-12);
    VerifyUpperTriangular(R, 1e-12);
    VerifyMatrixEqual(A, MatMul(Q, R), 1e-12);
  }
}

}  // namespace
}  // namespace jxl
