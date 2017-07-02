// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/embedder_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class FPDFRenderPatternEmbeddertest : public EmbedderTest {};

TEST_F(FPDFRenderPatternEmbeddertest, LoadError_547706) {
  // Test shading where object is a dictionary instead of a stream.
  EXPECT_TRUE(OpenDocument("bug_547706.pdf"));
  FPDF_PAGE page = LoadPage(0);
  FPDF_BITMAP bitmap = RenderPage(page);
  CompareBitmap(bitmap, 612, 792, "1940568c9ba33bac5d0b1ee9558c76b3");
  FPDFBitmap_Destroy(bitmap);
  UnloadPage(page);
}
