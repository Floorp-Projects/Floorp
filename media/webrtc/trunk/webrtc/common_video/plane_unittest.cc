/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "common_video/plane.h"

#include <math.h>
#include <string.h>

#include "gtest/gtest.h"

namespace webrtc {

TEST(TestPlane, CreateEmptyPlaneValues) {
  Plane plane;
  int size, stride;
  EXPECT_EQ(0, plane.allocated_size());
  EXPECT_EQ(0, plane.stride());
  EXPECT_TRUE(plane.IsZeroSize());
  size = 0;
  stride = 20;
  EXPECT_EQ(-1, plane.CreateEmptyPlane(size, stride, 1));
  EXPECT_EQ(-1, plane.CreateEmptyPlane(10, stride, size));
  size  = 20;
  stride = 0;
  EXPECT_EQ(-1, plane.CreateEmptyPlane(size, stride, size));
  stride = 20;
  EXPECT_EQ(0, plane.CreateEmptyPlane(size, stride, size));
  EXPECT_EQ(size, plane.allocated_size());
  EXPECT_EQ(stride, plane.stride());
  EXPECT_FALSE(plane.IsZeroSize());
}

TEST(TestPlane, ResetSize) {
  Plane plane;
  EXPECT_TRUE(plane.IsZeroSize());
  int allocated_size, plane_size, stride;
  EXPECT_EQ(0, plane.allocated_size());
  allocated_size = 30;
  plane_size = 20;
  stride = 10;
  EXPECT_EQ(0, plane.CreateEmptyPlane(allocated_size, stride, plane_size));
  EXPECT_EQ(allocated_size, plane.allocated_size());
  EXPECT_FALSE(plane.IsZeroSize());
  plane.ResetSize();
  EXPECT_TRUE(plane.IsZeroSize());
}

TEST(TestPlane, PlaneCopy) {
  Plane plane1, plane2;
  // Copy entire plane.
  plane1.CreateEmptyPlane(100, 10, 100);
  int size1 = plane1.allocated_size();
  int size2 = 30;
  plane2.CreateEmptyPlane(50, 15, size2);
  int stride1 = plane1.stride();
  int stride2 = plane2.stride();
  plane1.Copy(plane2);
  // Smaller size - keep buffer size as is.
  EXPECT_EQ(size1, plane1.allocated_size());
  EXPECT_EQ(stride2, plane1.stride());
  plane2.Copy(plane1);
  // Verify increment of allocated size.
  EXPECT_EQ(plane1.allocated_size(), plane2.allocated_size());
  EXPECT_EQ(stride2, plane2.stride());
  // Copy buffer.
  uint8_t buffer1[100];
  size1 = 80;
  memset(&buffer1, 0, size1);
  plane2.Copy(size1, stride1, buffer1);
  EXPECT_GE(plane2.allocated_size(), size1);
  EXPECT_EQ(0, memcmp(buffer1, plane2.buffer(), size1));
}

TEST(TestPlane, PlaneSwap) {
  Plane plane1, plane2;
  int size1, size2, stride1, stride2;
  plane1.CreateEmptyPlane(100, 10, 100);
  plane2.CreateEmptyPlane(50, 15, 50);
  size1 = plane1.allocated_size();
  stride1 = plane1.stride();
  stride2 = plane2.stride();
  size2 = plane2.allocated_size();
  plane1.Swap(plane2);
  EXPECT_EQ(size1, plane2.allocated_size());
  EXPECT_EQ(size2, plane1.allocated_size());
  EXPECT_EQ(stride2, plane1.stride());
  EXPECT_EQ(stride1, plane2.stride());
}

}  // namespace webrtc
