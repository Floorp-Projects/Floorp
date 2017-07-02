// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/fpdf_ext.h"
#include "public/fpdfview.h"
#include "testing/embedder_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class FPDFExtEmbeddertest : public EmbedderTest {};

TEST_F(FPDFExtEmbeddertest, PageModeUnknown) {
  EXPECT_EQ(PAGEMODE_UNKNOWN, FPDFDoc_GetPageMode(nullptr));
}

TEST_F(FPDFExtEmbeddertest, PageModeUseNone) {
  EXPECT_TRUE(OpenDocument("hello_world.pdf"));
  EXPECT_EQ(PAGEMODE_USENONE, FPDFDoc_GetPageMode(document()));
}

TEST_F(FPDFExtEmbeddertest, PageModeUseOutlines) {
  EXPECT_TRUE(OpenDocument("use_outlines.pdf"));
  EXPECT_EQ(PAGEMODE_USEOUTLINES, FPDFDoc_GetPageMode(document()));
}
