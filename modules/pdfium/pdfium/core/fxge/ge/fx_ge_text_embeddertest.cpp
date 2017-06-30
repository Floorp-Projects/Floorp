// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/embedder_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class FXGETextEmbedderTest : public EmbedderTest {};

TEST_F(FXGETextEmbedderTest, BadItalic) {
  // Shouldn't crash.
  EXPECT_TRUE(OpenDocument("bug_601362.pdf"));
  FPDF_PAGE page = LoadPage(0);
  EXPECT_NE(nullptr, page);
  FPDF_BITMAP bitmap = RenderPage(page);
  FPDFBitmap_Destroy(bitmap);
  UnloadPage(page);
}
