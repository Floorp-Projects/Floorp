// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/fpdf_ppo.h"

#include "core/fxcrt/fx_basic.h"
#include "public/fpdf_edit.h"
#include "public/fpdfview.h"
#include "testing/embedder_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/test_support.h"

namespace {

class FPDFPPOEmbeddertest : public EmbedderTest {};

}  // namespace

TEST_F(FPDFPPOEmbeddertest, NoViewerPreferences) {
  EXPECT_TRUE(OpenDocument("hello_world.pdf"));

  FPDF_DOCUMENT output_doc = FPDF_CreateNewDocument();
  EXPECT_TRUE(output_doc);
  EXPECT_FALSE(FPDF_CopyViewerPreferences(output_doc, document()));
  FPDF_CloseDocument(output_doc);
}

TEST_F(FPDFPPOEmbeddertest, ViewerPreferences) {
  EXPECT_TRUE(OpenDocument("viewer_ref.pdf"));

  FPDF_DOCUMENT output_doc = FPDF_CreateNewDocument();
  EXPECT_TRUE(output_doc);
  EXPECT_TRUE(FPDF_CopyViewerPreferences(output_doc, document()));
  FPDF_CloseDocument(output_doc);
}

TEST_F(FPDFPPOEmbeddertest, ImportPages) {
  EXPECT_TRUE(OpenDocument("viewer_ref.pdf"));

  FPDF_PAGE page = LoadPage(0);
  EXPECT_TRUE(page);

  FPDF_DOCUMENT output_doc = FPDF_CreateNewDocument();
  EXPECT_TRUE(output_doc);
  EXPECT_TRUE(FPDF_CopyViewerPreferences(output_doc, document()));
  EXPECT_TRUE(FPDF_ImportPages(output_doc, document(), "1", 0));
  EXPECT_EQ(1, FPDF_GetPageCount(output_doc));
  FPDF_CloseDocument(output_doc);

  UnloadPage(page);
}

TEST_F(FPDFPPOEmbeddertest, BadRanges) {
  EXPECT_TRUE(OpenDocument("viewer_ref.pdf"));

  FPDF_PAGE page = LoadPage(0);
  EXPECT_TRUE(page);

  FPDF_DOCUMENT output_doc = FPDF_CreateNewDocument();
  EXPECT_TRUE(output_doc);
  EXPECT_FALSE(FPDF_ImportPages(output_doc, document(), "clams", 0));
  EXPECT_FALSE(FPDF_ImportPages(output_doc, document(), "0", 0));
  EXPECT_FALSE(FPDF_ImportPages(output_doc, document(), "42", 0));
  EXPECT_FALSE(FPDF_ImportPages(output_doc, document(), "1,2", 0));
  EXPECT_FALSE(FPDF_ImportPages(output_doc, document(), "1-2", 0));
  EXPECT_FALSE(FPDF_ImportPages(output_doc, document(), ",1", 0));
  EXPECT_FALSE(FPDF_ImportPages(output_doc, document(), "1,", 0));
  EXPECT_FALSE(FPDF_ImportPages(output_doc, document(), "1-", 0));
  EXPECT_FALSE(FPDF_ImportPages(output_doc, document(), "-1", 0));
  EXPECT_FALSE(FPDF_ImportPages(output_doc, document(), "-,0,,,1-", 0));
  FPDF_CloseDocument(output_doc);

  UnloadPage(page);
}

TEST_F(FPDFPPOEmbeddertest, GoodRanges) {
  EXPECT_TRUE(OpenDocument("viewer_ref.pdf"));

  FPDF_PAGE page = LoadPage(0);
  EXPECT_TRUE(page);

  FPDF_DOCUMENT output_doc = FPDF_CreateNewDocument();
  EXPECT_TRUE(output_doc);
  EXPECT_TRUE(FPDF_CopyViewerPreferences(output_doc, document()));
  EXPECT_TRUE(FPDF_ImportPages(output_doc, document(), "1,1,1,1", 0));
  EXPECT_TRUE(FPDF_ImportPages(output_doc, document(), "1-1", 0));
  EXPECT_EQ(5, FPDF_GetPageCount(output_doc));
  FPDF_CloseDocument(output_doc);

  UnloadPage(page);
}

TEST_F(FPDFPPOEmbeddertest, BUG_664284) {
  EXPECT_TRUE(OpenDocument("bug_664284.pdf"));

  FPDF_PAGE page = LoadPage(0);
  EXPECT_TRUE(page);

  FPDF_DOCUMENT output_doc = FPDF_CreateNewDocument();
  EXPECT_TRUE(output_doc);
  EXPECT_TRUE(FPDF_ImportPages(output_doc, document(), "1", 0));
  FPDF_CloseDocument(output_doc);

  UnloadPage(page);
}
