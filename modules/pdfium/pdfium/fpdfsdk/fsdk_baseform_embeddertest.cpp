// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fpdfsdk/cba_annotiterator.h"
#include "fpdfsdk/cpdfsdk_annot.h"
#include "fpdfsdk/cpdfsdk_formfillenvironment.h"
#include "fpdfsdk/fsdk_define.h"
#include "testing/embedder_test.h"
#include "testing/embedder_test_mock_delegate.h"
#include "testing/embedder_test_timer_handling_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

void CheckRect(const CFX_FloatRect& actual, const CFX_FloatRect& expected) {
  EXPECT_EQ(expected.left, actual.left);
  EXPECT_EQ(expected.bottom, actual.bottom);
  EXPECT_EQ(expected.right, actual.right);
  EXPECT_EQ(expected.top, actual.top);
}

}  // namespace

class FSDKBaseFormEmbeddertest : public EmbedderTest {};

TEST_F(FSDKBaseFormEmbeddertest, CBA_AnnotIterator) {
  EXPECT_TRUE(OpenDocument("annotiter.pdf"));
  FPDF_PAGE page0 = LoadPage(0);
  FPDF_PAGE page1 = LoadPage(1);
  FPDF_PAGE page2 = LoadPage(2);
  EXPECT_TRUE(page0);
  EXPECT_TRUE(page1);
  EXPECT_TRUE(page2);

  CFX_FloatRect LeftBottom(200, 200, 220, 220);
  CFX_FloatRect RightBottom(400, 201, 420, 221);
  CFX_FloatRect LeftTop(201, 400, 221, 420);
  CFX_FloatRect RightTop(401, 401, 421, 421);

  CPDFSDK_FormFillEnvironment* pFormFillEnv =
      static_cast<CPDFSDK_FormFillEnvironment*>(form_handle());

  {
    // Page 0 specifies "row order".
    CBA_AnnotIterator iter(pFormFillEnv->GetPageView(0),
                           CPDF_Annot::Subtype::WIDGET);
    CPDFSDK_Annot* pAnnot = iter.GetFirstAnnot();
    CheckRect(pAnnot->GetRect(), RightTop);
    pAnnot = iter.GetNextAnnot(pAnnot);
    CheckRect(pAnnot->GetRect(), LeftTop);
    pAnnot = iter.GetNextAnnot(pAnnot);
    CheckRect(pAnnot->GetRect(), RightBottom);
    pAnnot = iter.GetNextAnnot(pAnnot);
    CheckRect(pAnnot->GetRect(), LeftBottom);
    pAnnot = iter.GetNextAnnot(pAnnot);
    EXPECT_EQ(iter.GetFirstAnnot(), pAnnot);

    pAnnot = iter.GetLastAnnot();
    CheckRect(pAnnot->GetRect(), LeftBottom);
    pAnnot = iter.GetPrevAnnot(pAnnot);
    CheckRect(pAnnot->GetRect(), RightBottom);
    pAnnot = iter.GetPrevAnnot(pAnnot);
    CheckRect(pAnnot->GetRect(), LeftTop);
    pAnnot = iter.GetPrevAnnot(pAnnot);
    CheckRect(pAnnot->GetRect(), RightTop);
    pAnnot = iter.GetPrevAnnot(pAnnot);
    EXPECT_EQ(iter.GetLastAnnot(), pAnnot);
  }
  {
    // Page 1 specifies "column order"
    CBA_AnnotIterator iter(pFormFillEnv->GetPageView(1),
                           CPDF_Annot::Subtype::WIDGET);
    CPDFSDK_Annot* pAnnot = iter.GetFirstAnnot();
    CheckRect(pAnnot->GetRect(), RightTop);
    pAnnot = iter.GetNextAnnot(pAnnot);
    CheckRect(pAnnot->GetRect(), RightBottom);
    pAnnot = iter.GetNextAnnot(pAnnot);
    CheckRect(pAnnot->GetRect(), LeftTop);
    pAnnot = iter.GetNextAnnot(pAnnot);
    CheckRect(pAnnot->GetRect(), LeftBottom);
    pAnnot = iter.GetNextAnnot(pAnnot);
    EXPECT_EQ(iter.GetFirstAnnot(), pAnnot);

    pAnnot = iter.GetLastAnnot();
    CheckRect(pAnnot->GetRect(), LeftBottom);
    pAnnot = iter.GetPrevAnnot(pAnnot);
    CheckRect(pAnnot->GetRect(), LeftTop);
    pAnnot = iter.GetPrevAnnot(pAnnot);
    CheckRect(pAnnot->GetRect(), RightBottom);
    pAnnot = iter.GetPrevAnnot(pAnnot);
    CheckRect(pAnnot->GetRect(), RightTop);
    pAnnot = iter.GetPrevAnnot(pAnnot);
    EXPECT_EQ(iter.GetLastAnnot(), pAnnot);
  }
  {
    // Page 2 specifies "struct order"
    CBA_AnnotIterator iter(pFormFillEnv->GetPageView(2),
                           CPDF_Annot::Subtype::WIDGET);
    CPDFSDK_Annot* pAnnot = iter.GetFirstAnnot();
    CheckRect(pAnnot->GetRect(), LeftBottom);
    pAnnot = iter.GetNextAnnot(pAnnot);
    CheckRect(pAnnot->GetRect(), RightTop);
    pAnnot = iter.GetNextAnnot(pAnnot);
    CheckRect(pAnnot->GetRect(), LeftTop);
    pAnnot = iter.GetNextAnnot(pAnnot);
    CheckRect(pAnnot->GetRect(), RightBottom);
    pAnnot = iter.GetNextAnnot(pAnnot);
    EXPECT_EQ(iter.GetFirstAnnot(), pAnnot);

    pAnnot = iter.GetLastAnnot();
    CheckRect(pAnnot->GetRect(), RightBottom);
    pAnnot = iter.GetPrevAnnot(pAnnot);
    CheckRect(pAnnot->GetRect(), LeftTop);
    pAnnot = iter.GetPrevAnnot(pAnnot);
    CheckRect(pAnnot->GetRect(), RightTop);
    pAnnot = iter.GetPrevAnnot(pAnnot);
    CheckRect(pAnnot->GetRect(), LeftBottom);
    pAnnot = iter.GetPrevAnnot(pAnnot);
    EXPECT_EQ(iter.GetLastAnnot(), pAnnot);
  }
  UnloadPage(page2);
  UnloadPage(page1);
  UnloadPage(page0);
}
