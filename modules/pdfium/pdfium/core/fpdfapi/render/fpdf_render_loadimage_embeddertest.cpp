// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/embedder_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class FPDFRenderLoadImageEmbeddertest : public EmbedderTest {};

TEST_F(FPDFRenderLoadImageEmbeddertest, Bug_554151) {
  // Test scanline downsampling with a BitsPerComponent of 4.
  // Should not crash.
  EXPECT_TRUE(OpenDocument("bug_554151.pdf"));
  FPDF_PAGE page = LoadPage(0);
  EXPECT_NE(nullptr, page);
  FPDF_BITMAP bitmap = RenderPage(page);
  CompareBitmap(bitmap, 612, 792, "a14d7ee573c1b2456d7bf6b7762823cf");
  FPDFBitmap_Destroy(bitmap);
  UnloadPage(page);
}

TEST_F(FPDFRenderLoadImageEmbeddertest, Bug_557223) {
  // Should not crash
  EXPECT_TRUE(OpenDocument("bug_557223.pdf"));
  FPDF_PAGE page = LoadPage(0);
  EXPECT_NE(nullptr, page);
  FPDF_BITMAP bitmap = RenderPage(page);
  CompareBitmap(bitmap, 24, 24, "dc0ea1b743c2edb22c597cadc8537f7b");
  FPDFBitmap_Destroy(bitmap);
  UnloadPage(page);
}

TEST_F(FPDFRenderLoadImageEmbeddertest, Bug_603518) {
  // Should not crash
  EXPECT_TRUE(OpenDocument("bug_603518.pdf"));
  FPDF_PAGE page = LoadPage(0);
  EXPECT_NE(nullptr, page);
  FPDF_BITMAP bitmap = RenderPage(page);
  CompareBitmap(bitmap, 749, 749, "b9e75190cdc5edf0069a408744ca07dc");
  FPDFBitmap_Destroy(bitmap);
  UnloadPage(page);
}
