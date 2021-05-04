// Copyright (c) the JPEG XL Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "lib/jxl/linalg.h"

#include <stdlib.h>

#include <cmath>
#include <deque>
#include <utility>
#include <vector>

#include "lib/jxl/base/status.h"
#include "lib/jxl/common.h"
#include "lib/jxl/image_ops.h"

namespace jxl {

void AssertSymmetric(const ImageD& A) {
#if JXL_ENABLE_ASSERT
  JXL_ASSERT(A.xsize() == A.ysize());
  for (size_t i = 0; i < A.xsize(); ++i) {
    for (size_t j = i + 1; j < A.xsize(); ++j) {
      JXL_ASSERT(std::abs(A.Row(i)[j] - A.Row(j)[i]) < 1e-15);
    }
  }
#endif
}

void Diagonalize2x2(const double a0, const double a1, const double b, double* c,
                    double* s) {
  if (std::abs(b) < 1e-15) {
    *c = 1.0;
    *s = 0.0;
    return;
  }
  double phi = std::atan2(2 * b, a1 - a0);
  double theta = b > 0.0 ? 0.5 * phi : 0.5 * phi + Pi(1.0);
  *c = std::cos(theta);
  *s = std::sin(theta);
}

void GivensRotation(const double x, const double y, double* c, double* s) {
  if (y == 0.0) {
    *c = x < 0.0 ? -1.0 : 1.0;
    *s = 0.0;
  } else {
    const double h = hypot(x, y);
    const double d = 1.0 / h;
    *c = x * d;
    *s = -y * d;
  }
}

void RotateMatrixCols(ImageD* const JXL_RESTRICT U, int i, int j, double c,
                      double s) {
  JXL_ASSERT(U->xsize() == U->ysize());
  const size_t N = U->xsize();
  double* const JXL_RESTRICT u_i = U->Row(i);
  double* const JXL_RESTRICT u_j = U->Row(j);
  std::vector<double> rot_i, rot_j;
  rot_i.reserve(N);
  rot_j.reserve(N);
  for (size_t k = 0; k < N; ++k) {
    rot_i.push_back(u_i[k] * c - u_j[k] * s);
    rot_j.push_back(u_i[k] * s + u_j[k] * c);
  }
  for (size_t k = 0; k < N; ++k) {
    u_i[k] = rot_i[k];
    u_j[k] = rot_j[k];
  }
}
void HouseholderReflector(const size_t N, const double* x, double* u) {
  const double sigma = x[0] <= 0.0 ? 1.0 : -1.0;
  u[0] = x[0] - sigma * std::sqrt(DotProduct(N, x, x));
  for (size_t k = 1; k < N; ++k) {
    u[k] = x[k];
  }
  double u_norm = 1.0 / std::sqrt(DotProduct(N, u, u));
  for (size_t k = 0; k < N; ++k) {
    u[k] *= u_norm;
  }
}

void ConvertToTridiagonal(const ImageD& A, ImageD* const JXL_RESTRICT T,
                          ImageD* const JXL_RESTRICT U) {
  AssertSymmetric(A);
  const size_t N = A.xsize();
  *U = Identity<double>(A.xsize());
  *T = CopyImage(A);
  std::vector<ImageD> u_stack;
  for (size_t k = 0; k + 2 < N; ++k) {
    if (DotProduct(N - k - 2, &T->Row(k)[k + 2], &T->Row(k)[k + 2]) > 1e-15) {
      ImageD u(N, 1);
      ZeroFillImage(&u);
      HouseholderReflector(N - k - 1, &T->Row(k)[k + 1], &u.Row(0)[k + 1]);
      ImageD v = MatMul(*T, u);
      double scale = DotProduct(u, v);
      v = LinComb(2.0, v, -2.0 * scale, u);
      SubtractFrom(MatMul(u, Transpose(v)), T);
      SubtractFrom(MatMul(v, Transpose(u)), T);
      u_stack.emplace_back(std::move(u));
    }
  }
  while (!u_stack.empty()) {
    const ImageD& u = u_stack.back();
    ImageD v = MatMul(Transpose(*U), u);
    SubtractFrom(ScaleImage(2.0, MatMul(u, Transpose(v))), U);
    u_stack.pop_back();
  }
}

double WilkinsonShift(const double a0, const double a1, const double b) {
  const double d = 0.5 * (a0 - a1);
  if (d == 0.0) {
    return a1 - std::abs(b);
  }
  const double sign_d = d > 0.0 ? 1.0 : -1.0;
  return a1 - b * b / (d + sign_d * hypotf(d, b));
}

void ImplicitQRStep(ImageD* const JXL_RESTRICT U, double* const JXL_RESTRICT a,
                    double* const JXL_RESTRICT b, int m0, int m1) {
  JXL_ASSERT(m1 - m0 > 2);
  double x = a[m0] - WilkinsonShift(a[m1 - 2], a[m1 - 1], b[m1 - 1]);
  double y = b[m0 + 1];
  for (int k = m0; k < m1 - 1; ++k) {
    double c, s;
    GivensRotation(x, y, &c, &s);
    const double w = c * x - s * y;
    const double d = a[k] - a[k + 1];
    const double z = (2 * c * b[k + 1] + d * s) * s;
    a[k] -= z;
    a[k + 1] += z;
    b[k + 1] = d * c * s + (c * c - s * s) * b[k + 1];
    x = b[k + 1];
    if (k > m0) {
      b[k] = w;
    }
    if (k < m1 - 2) {
      y = -s * b[k + 2];
      b[k + 2] *= c;
    }
    RotateMatrixCols(U, k, k + 1, c, s);
  }
}

void ScanInterval(const double* const JXL_RESTRICT a,
                  const double* const JXL_RESTRICT b, int istart,
                  const int iend, const double eps,
                  std::deque<std::pair<int, int> >* intervals) {
  for (int k = istart; k < iend; ++k) {
    if ((k + 1 == iend) ||
        std::abs(b[k + 1]) < eps * (std::abs(a[k]) + std::abs(a[k + 1]))) {
      if (k > istart) {
        intervals->push_back(std::make_pair(istart, k + 1));
      }
      istart = k + 1;
    }
  }
}

void ConvertToDiagonal(const ImageD& A, ImageD* const JXL_RESTRICT diag,
                       ImageD* const JXL_RESTRICT U) {
  AssertSymmetric(A);
  const size_t N = A.xsize();
  ImageD T;
  ConvertToTridiagonal(A, &T, U);
  // From now on, the algorithm keeps the transformed matrix tri-diagonal,
  // so we only need to keep track of the diagonal and the off-diagonal entries.
  std::vector<double> a(N);
  std::vector<double> b(N);
  for (size_t k = 0; k < N; ++k) {
    a[k] = T.Row(k)[k];
    if (k > 0) b[k] = T.Row(k)[k - 1];
  }
  // Run the symmetric tri-diagonal QR algorithm with implicit Wilkinson shift.
  const double kEpsilon = 1e-14;
  std::deque<std::pair<int, int> > intervals;
  ScanInterval(&a[0], &b[0], 0, N, kEpsilon, &intervals);
  while (!intervals.empty()) {
    const int istart = intervals[0].first;
    const int iend = intervals[0].second;
    intervals.pop_front();
    if (iend == istart + 2) {
      double& a0 = a[istart];
      double& a1 = a[istart + 1];
      double& b1 = b[istart + 1];
      double c, s;
      Diagonalize2x2(a0, a1, b1, &c, &s);
      const double d = a0 - a1;
      const double z = (2 * c * b1 + d * s) * s;
      a0 -= z;
      a1 += z;
      b1 = 0.0;
      RotateMatrixCols(U, istart, istart + 1, c, s);
    } else {
      ImplicitQRStep(U, &a[0], &b[0], istart, iend);
      ScanInterval(&a[0], &b[0], istart, iend, kEpsilon, &intervals);
    }
  }
  *diag = ImageD(N, 1);
  double* const JXL_RESTRICT diag_row = diag->Row(0);
  for (size_t k = 0; k < N; ++k) {
    diag_row[k] = a[k];
  }
}

void ComputeQRFactorization(const ImageD& A, ImageD* const JXL_RESTRICT Q,
                            ImageD* const JXL_RESTRICT R) {
  JXL_ASSERT(A.xsize() == A.ysize());
  const size_t N = A.xsize();
  *Q = Identity<double>(N);
  *R = CopyImage(A);
  std::vector<ImageD> u_stack;
  for (size_t k = 0; k + 1 < N; ++k) {
    if (DotProduct(N - k - 1, &R->Row(k)[k + 1], &R->Row(k)[k + 1]) > 1e-15) {
      ImageD u(N, 1);
      FillImage(0.0, &u);
      HouseholderReflector(N - k, &R->Row(k)[k], &u.Row(0)[k]);
      ImageD v = MatMul(Transpose(u), *R);
      SubtractFrom(ScaleImage(2.0, MatMul(u, v)), R);
      u_stack.emplace_back(std::move(u));
    }
  }
  while (!u_stack.empty()) {
    const ImageD& u = u_stack.back();
    ImageD v = MatMul(Transpose(u), *Q);
    SubtractFrom(ScaleImage(2.0, MatMul(u, v)), Q);
    u_stack.pop_back();
  }
}
}  // namespace jxl
