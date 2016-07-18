/*
 *  Copyright 2012 The LibYuv Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS. All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdlib.h>
#include <time.h>

#include "libyuv/cpu_id.h"
#include "libyuv/rotate_argb.h"
#include "libyuv/row.h"
#include "../unit_test/unit_test.h"

namespace libyuv {

void TestRotateBpp(int src_width, int src_height,
                   int dst_width, int dst_height,
                   libyuv::RotationMode mode,
                   int benchmark_iterations,
                   const int kBpp) {
  if (src_width < 1) {
    src_width = 1;
  }
  if (src_height < 1) {
    src_height = 1;
  }
  if (dst_width < 1) {
    dst_width = 1;
  }
  if (dst_height < 1) {
    dst_height = 1;
  }
  int src_stride_argb = src_width * kBpp;
  int src_argb_plane_size = src_stride_argb * src_height;
  align_buffer_64(src_argb, src_argb_plane_size);
  for (int i = 0; i < src_argb_plane_size; ++i) {
    src_argb[i] = random() & 0xff;
  }

  int dst_stride_argb = dst_width * kBpp;
  int dst_argb_plane_size = dst_stride_argb * dst_height;
  align_buffer_64(dst_argb_c, dst_argb_plane_size);
  align_buffer_64(dst_argb_opt, dst_argb_plane_size);
  memset(dst_argb_c, 2, dst_argb_plane_size);
  memset(dst_argb_opt, 3, dst_argb_plane_size);

  if (kBpp == 1) {
    MaskCpuFlags(0);  // Disable all CPU optimization.
    RotatePlane(src_argb, src_stride_argb,
                dst_argb_c, dst_stride_argb,
                src_width, src_height, mode);

    MaskCpuFlags(-1);  // Enable all CPU optimization.
    for (int i = 0; i < benchmark_iterations; ++i) {
      RotatePlane(src_argb, src_stride_argb,
                  dst_argb_opt, dst_stride_argb,
                  src_width, src_height, mode);
    }
  } else if (kBpp == 4) {
    MaskCpuFlags(0);  // Disable all CPU optimization.
    ARGBRotate(src_argb, src_stride_argb,
               dst_argb_c, dst_stride_argb,
               src_width, src_height, mode);

    MaskCpuFlags(-1);  // Enable all CPU optimization.
    for (int i = 0; i < benchmark_iterations; ++i) {
      ARGBRotate(src_argb, src_stride_argb,
                 dst_argb_opt, dst_stride_argb,
                 src_width, src_height, mode);
    }
  }

  // Rotation should be exact.
  for (int i = 0; i < dst_argb_plane_size; ++i) {
    EXPECT_EQ(dst_argb_c[i], dst_argb_opt[i]);
  }

  free_aligned_buffer_64(dst_argb_c);
  free_aligned_buffer_64(dst_argb_opt);
  free_aligned_buffer_64(src_argb);
}

static void ARGBTestRotate(int src_width, int src_height,
                           int dst_width, int dst_height,
                           libyuv::RotationMode mode,
                           int benchmark_iterations) {
  TestRotateBpp(src_width, src_height,
                dst_width, dst_height,
                mode, benchmark_iterations, 4);
}

TEST_F(libyuvTest, ARGBRotate0) {
  ARGBTestRotate(benchmark_width_, benchmark_height_,
                 benchmark_width_, benchmark_height_,
                 kRotate0, benchmark_iterations_);
}

TEST_F(libyuvTest, ARGBRotate90) {
  ARGBTestRotate(benchmark_width_, benchmark_height_,
                 benchmark_height_, benchmark_width_,
                 kRotate90, benchmark_iterations_);
}

TEST_F(libyuvTest, ARGBRotate180) {
  ARGBTestRotate(benchmark_width_, benchmark_height_,
                 benchmark_width_, benchmark_height_,
                 kRotate180, benchmark_iterations_);
}

TEST_F(libyuvTest, ARGBRotate270) {
  ARGBTestRotate(benchmark_width_, benchmark_height_,
                 benchmark_height_, benchmark_width_,
                 kRotate270, benchmark_iterations_);
}

TEST_F(libyuvTest, ARGBRotate0_Odd) {
  ARGBTestRotate(benchmark_width_ - 3, benchmark_height_ - 1,
                 benchmark_width_ - 3, benchmark_height_ - 1,
                 kRotate0, benchmark_iterations_);
}

TEST_F(libyuvTest, ARGBRotate90_Odd) {
  ARGBTestRotate(benchmark_width_ - 3, benchmark_height_ - 1,
                 benchmark_height_ - 1, benchmark_width_ - 3,
                 kRotate90, benchmark_iterations_);
}

TEST_F(libyuvTest, ARGBRotate180_Odd) {
  ARGBTestRotate(benchmark_width_ - 3, benchmark_height_ - 1,
                 benchmark_width_ - 3, benchmark_height_ - 1,
                 kRotate180, benchmark_iterations_);
}

TEST_F(libyuvTest, ARGBRotate270_Odd) {
  ARGBTestRotate(benchmark_width_ - 3, benchmark_height_ - 1,
                 benchmark_height_ - 1, benchmark_width_ - 3,
                 kRotate270, benchmark_iterations_);
}

static void TestRotatePlane(int src_width, int src_height,
                            int dst_width, int dst_height,
                            libyuv::RotationMode mode,
                            int benchmark_iterations) {
  TestRotateBpp(src_width, src_height,
                dst_width, dst_height,
                mode, benchmark_iterations, 1);
}

TEST_F(libyuvTest, RotatePlane0) {
  TestRotatePlane(benchmark_width_, benchmark_height_,
                  benchmark_width_, benchmark_height_,
                  kRotate0, benchmark_iterations_);
}

TEST_F(libyuvTest, RotatePlane90) {
  TestRotatePlane(benchmark_width_, benchmark_height_,
                  benchmark_height_, benchmark_width_,
                  kRotate90, benchmark_iterations_);
}

TEST_F(libyuvTest, RotatePlane180) {
  TestRotatePlane(benchmark_width_, benchmark_height_,
                  benchmark_width_, benchmark_height_,
                  kRotate180, benchmark_iterations_);
}

TEST_F(libyuvTest, RotatePlane270) {
  TestRotatePlane(benchmark_width_, benchmark_height_,
                  benchmark_height_, benchmark_width_,
                  kRotate270, benchmark_iterations_);
}

TEST_F(libyuvTest, RotatePlane0_Odd) {
  TestRotatePlane(benchmark_width_ - 3, benchmark_height_ - 1,
                  benchmark_width_ - 3, benchmark_height_ - 1,
                  kRotate0, benchmark_iterations_);
}

TEST_F(libyuvTest, RotatePlane90_Odd) {
  TestRotatePlane(benchmark_width_ - 3, benchmark_height_ - 1,
                  benchmark_height_ - 1, benchmark_width_ - 3,
                  kRotate90, benchmark_iterations_);
}

TEST_F(libyuvTest, RotatePlane180_Odd) {
  TestRotatePlane(benchmark_width_ - 3, benchmark_height_ - 1,
                  benchmark_width_ - 3, benchmark_height_ - 1,
                  kRotate180, benchmark_iterations_);
}

TEST_F(libyuvTest, RotatePlane270_Odd) {
  TestRotatePlane(benchmark_width_ - 3, benchmark_height_ - 1,
                  benchmark_height_ - 1, benchmark_width_ - 3,
                  kRotate270, benchmark_iterations_);
}

}  // namespace libyuv
