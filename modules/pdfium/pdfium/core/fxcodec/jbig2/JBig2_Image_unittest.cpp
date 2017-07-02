// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(tsepez) this requires a lot more testing.

#include <stdint.h>

#include "core/fxcodec/jbig2/JBig2_Image.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int32_t kWidthPixels = 80;
const int32_t kWidthBytes = 10;
const int32_t kStrideBytes = kWidthBytes + 1;  // For testing stride != width.
const int32_t kHeightLines = 20;
const int32_t kLargerHeightLines = 100;
const int32_t kTooLargeHeightLines = 40000000;

}  // namespace

TEST(fxcodec, JBig2ImageCreate) {
  CJBig2_Image img(kWidthPixels, kHeightLines);
  img.setPixel(0, 0, true);
  img.setPixel(kWidthPixels - 1, kHeightLines - 1, false);
  EXPECT_EQ(kWidthPixels, img.width());
  EXPECT_EQ(kHeightLines, img.height());
  EXPECT_TRUE(img.getPixel(0, 0));
  EXPECT_FALSE(img.getPixel(kWidthPixels - 1, kHeightLines - 1));
}

TEST(fxcodec, JBig2ImageCreateTooBig) {
  CJBig2_Image img(kWidthPixels, kTooLargeHeightLines);
  EXPECT_EQ(0, img.width());
  EXPECT_EQ(0, img.height());
  EXPECT_EQ(nullptr, img.m_pData);
}

TEST(fxcodec, JBig2ImageCreateExternal) {
  uint8_t buf[kHeightLines * kStrideBytes];
  CJBig2_Image img(kWidthPixels, kHeightLines, kStrideBytes, buf);
  img.setPixel(0, 0, true);
  img.setPixel(kWidthPixels - 1, kHeightLines - 1, false);
  EXPECT_EQ(kWidthPixels, img.width());
  EXPECT_EQ(kHeightLines, img.height());
  EXPECT_TRUE(img.getPixel(0, 0));
  EXPECT_FALSE(img.getPixel(kWidthPixels - 1, kHeightLines - 1));
}

TEST(fxcodec, JBig2ImageCreateExternalTooBig) {
  uint8_t buf[kHeightLines * kStrideBytes];
  CJBig2_Image img(kWidthPixels, kTooLargeHeightLines, kStrideBytes, buf);
  EXPECT_EQ(0, img.width());
  EXPECT_EQ(0, img.height());
  EXPECT_EQ(nullptr, img.m_pData);
}

TEST(fxcodec, JBig2ImageExpand) {
  CJBig2_Image img(kWidthPixels, kHeightLines);
  img.setPixel(0, 0, true);
  img.setPixel(kWidthPixels - 1, kHeightLines - 1, false);
  img.expand(kLargerHeightLines, true);
  EXPECT_EQ(kWidthPixels, img.width());
  EXPECT_EQ(kLargerHeightLines, img.height());
  EXPECT_TRUE(img.getPixel(0, 0));
  EXPECT_FALSE(img.getPixel(kWidthPixels - 1, kHeightLines - 1));
  EXPECT_TRUE(img.getPixel(kWidthPixels - 1, kLargerHeightLines - 1));
}

TEST(fxcodec, JBig2ImageExpandTooBig) {
  CJBig2_Image img(kWidthPixels, kHeightLines);
  img.setPixel(0, 0, true);
  img.setPixel(kWidthPixels - 1, kHeightLines - 1, false);
  img.expand(kTooLargeHeightLines, true);
  EXPECT_EQ(kWidthPixels, img.width());
  EXPECT_EQ(kHeightLines, img.height());
  EXPECT_TRUE(img.getPixel(0, 0));
  EXPECT_FALSE(img.getPixel(kWidthPixels - 1, kHeightLines - 1));
}

TEST(fxcodec, JBig2ImageExpandExternal) {
  uint8_t buf[kHeightLines * kStrideBytes];
  CJBig2_Image img(kWidthPixels, kHeightLines, kStrideBytes, buf);
  img.setPixel(0, 0, true);
  img.setPixel(kWidthPixels - 1, kHeightLines - 1, false);
  img.expand(kLargerHeightLines, true);
  EXPECT_EQ(kWidthPixels, img.width());
  EXPECT_EQ(kLargerHeightLines, img.height());
  EXPECT_TRUE(img.getPixel(0, 0));
  EXPECT_FALSE(img.getPixel(kWidthPixels - 1, kHeightLines - 1));
  EXPECT_TRUE(img.getPixel(kWidthPixels - 1, kLargerHeightLines - 1));
}

TEST(fxcodec, JBig2ImageExpandExternalTooBig) {
  uint8_t buf[kHeightLines * kStrideBytes];
  CJBig2_Image img(kWidthPixels, kHeightLines, kStrideBytes, buf);
  img.setPixel(0, 0, true);
  img.setPixel(kWidthPixels - 1, kHeightLines - 1, false);
  img.expand(kTooLargeHeightLines, true);
  EXPECT_EQ(kWidthPixels, img.width());
  EXPECT_EQ(kHeightLines, img.height());
  EXPECT_TRUE(img.getPixel(0, 0));
  EXPECT_FALSE(img.getPixel(kWidthPixels - 1, kHeightLines - 1));
}
