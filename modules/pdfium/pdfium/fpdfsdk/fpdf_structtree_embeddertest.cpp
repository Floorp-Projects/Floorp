// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcrt/fx_string.h"
#include "public/fpdf_structtree.h"
#include "testing/embedder_test.h"
#include "testing/test_support.h"

class FPDFStructTreeEmbeddertest : public EmbedderTest, public TestSaver {};

TEST_F(FPDFStructTreeEmbeddertest, GetAltText) {
  ASSERT_TRUE(OpenDocument("tagged_alt_text.pdf"));
  FPDF_PAGE page = LoadPage(0);
  ASSERT_TRUE(page);

  FPDF_STRUCTTREE struct_tree = FPDF_StructTree_GetForPage(page);
  ASSERT_TRUE(struct_tree);
  ASSERT_EQ(1, FPDF_StructTree_CountChildren(struct_tree));

  FPDF_STRUCTELEMENT element = FPDF_StructTree_GetChildAtIndex(struct_tree, -1);
  EXPECT_EQ(nullptr, element);
  element = FPDF_StructTree_GetChildAtIndex(struct_tree, 1);
  EXPECT_EQ(nullptr, element);
  element = FPDF_StructTree_GetChildAtIndex(struct_tree, 0);
  ASSERT_NE(nullptr, element);
  EXPECT_EQ(0U, FPDF_StructElement_GetAltText(element, nullptr, 0));

  ASSERT_EQ(1, FPDF_StructElement_CountChildren(element));
  FPDF_STRUCTELEMENT child_element =
      FPDF_StructElement_GetChildAtIndex(element, -1);
  EXPECT_EQ(nullptr, child_element);
  child_element = FPDF_StructElement_GetChildAtIndex(element, 1);
  EXPECT_EQ(nullptr, child_element);
  child_element = FPDF_StructElement_GetChildAtIndex(element, 0);
  ASSERT_NE(nullptr, child_element);
  EXPECT_EQ(0U, FPDF_StructElement_GetAltText(child_element, nullptr, 0));

  ASSERT_EQ(1, FPDF_StructElement_CountChildren(child_element));
  FPDF_STRUCTELEMENT gchild_element =
      FPDF_StructElement_GetChildAtIndex(child_element, -1);
  EXPECT_EQ(nullptr, gchild_element);
  gchild_element = FPDF_StructElement_GetChildAtIndex(child_element, 1);
  EXPECT_EQ(nullptr, gchild_element);
  gchild_element = FPDF_StructElement_GetChildAtIndex(child_element, 0);
  ASSERT_NE(nullptr, gchild_element);
  ASSERT_EQ(24U, FPDF_StructElement_GetAltText(gchild_element, nullptr, 0));

  unsigned short buffer[12];
  memset(buffer, 0, sizeof(buffer));
  // Deliberately pass in a small buffer size to make sure |buffer| remains
  // untouched.
  ASSERT_EQ(24U, FPDF_StructElement_GetAltText(gchild_element, buffer, 1));
  for (size_t i = 0; i < FX_ArraySize(buffer); ++i)
    EXPECT_EQ(0U, buffer[i]);

  ASSERT_EQ(24U, FPDF_StructElement_GetAltText(gchild_element, buffer,
                                               sizeof(buffer)));
  const FX_WCHAR kExpected[] = L"Black Image";
  EXPECT_EQ(CFX_WideString(kExpected),
            CFX_WideString::FromUTF16LE(buffer, FXSYS_len(kExpected)));

  ASSERT_EQ(1, FPDF_StructElement_CountChildren(gchild_element));
  FPDF_STRUCTELEMENT ggchild_element =
      FPDF_StructElement_GetChildAtIndex(gchild_element, 0);
  EXPECT_EQ(nullptr, ggchild_element);

  FPDF_StructTree_Close(struct_tree);
  FPDF_ClosePage(page);
}
