// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/fpdf_edit.h"

#include "core/fpdfapi/cpdf_modulemgr.h"
#include "testing/gtest/include/gtest/gtest.h"

class PDFEditTest : public testing::Test {
  void SetUp() override {
    CPDF_ModuleMgr* module_mgr = CPDF_ModuleMgr::Get();
    module_mgr->InitPageModule();
  }

  void TearDown() override { CPDF_ModuleMgr::Destroy(); }
};

TEST_F(PDFEditTest, InsertObjectWithInvalidPage) {
  FPDF_DOCUMENT doc = FPDF_CreateNewDocument();
  FPDF_PAGE page = FPDFPage_New(doc, 0, 100, 100);
  EXPECT_EQ(0, FPDFPage_CountObject(page));

  FPDFPage_InsertObject(nullptr, nullptr);
  EXPECT_EQ(0, FPDFPage_CountObject(page));

  FPDFPage_InsertObject(page, nullptr);
  EXPECT_EQ(0, FPDFPage_CountObject(page));

  FPDF_PAGEOBJECT page_image = FPDFPageObj_NewImgeObj(doc);
  FPDFPage_InsertObject(nullptr, page_image);
  EXPECT_EQ(0, FPDFPage_CountObject(page));

  FPDF_ClosePage(page);
  FPDF_CloseDocument(doc);
}

TEST_F(PDFEditTest, NewImgeObj) {
  FPDF_DOCUMENT doc = FPDF_CreateNewDocument();
  FPDF_PAGE page = FPDFPage_New(doc, 0, 100, 100);
  EXPECT_EQ(0, FPDFPage_CountObject(page));

  FPDF_PAGEOBJECT page_image = FPDFPageObj_NewImgeObj(doc);
  FPDFPage_InsertObject(page, page_image);
  EXPECT_EQ(1, FPDFPage_CountObject(page));
  EXPECT_TRUE(FPDFPage_GenerateContent(page));

  FPDF_ClosePage(page);
  FPDF_CloseDocument(doc);
}

TEST_F(PDFEditTest, NewImgeObjGenerateContent) {
  FPDF_DOCUMENT doc = FPDF_CreateNewDocument();
  FPDF_PAGE page = FPDFPage_New(doc, 0, 100, 100);
  EXPECT_EQ(0, FPDFPage_CountObject(page));

  constexpr int kBitmapSize = 50;
  FPDF_BITMAP bitmap = FPDFBitmap_Create(kBitmapSize, kBitmapSize, 0);
  FPDFBitmap_FillRect(bitmap, 0, 0, kBitmapSize, kBitmapSize, 0x00000000);
  EXPECT_EQ(kBitmapSize, FPDFBitmap_GetWidth(bitmap));
  EXPECT_EQ(kBitmapSize, FPDFBitmap_GetHeight(bitmap));

  FPDF_PAGEOBJECT page_image = FPDFPageObj_NewImgeObj(doc);
  ASSERT_TRUE(FPDFImageObj_SetBitmap(&page, 0, page_image, bitmap));
  ASSERT_TRUE(
      FPDFImageObj_SetMatrix(page_image, kBitmapSize, 0, 0, kBitmapSize, 0, 0));
  FPDFPage_InsertObject(page, page_image);
  EXPECT_EQ(1, FPDFPage_CountObject(page));
  EXPECT_TRUE(FPDFPage_GenerateContent(page));

  FPDFBitmap_Destroy(bitmap);
  FPDF_ClosePage(page);
  FPDF_CloseDocument(doc);
}
