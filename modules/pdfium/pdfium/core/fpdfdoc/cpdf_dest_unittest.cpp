// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_null.h"
#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfdoc/cpdf_dest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/test_support.h"
#include "third_party/base/ptr_util.h"

TEST(cpdf_dest, GetXYZ) {
  bool hasX;
  bool hasY;
  bool hasZoom;
  float x;
  float y;
  float zoom;

  auto dest = pdfium::MakeUnique<CPDF_Dest>();
  EXPECT_FALSE(dest->GetXYZ(&hasX, &hasY, &hasZoom, &x, &y, &zoom));

  auto array = pdfium::MakeUnique<CPDF_Array>();
  array->AddNew<CPDF_Number>(0);  // Page Index.
  array->AddNew<CPDF_Name>("XYZ");
  array->AddNew<CPDF_Number>(4);  // X

  // Not enough entries.
  dest = pdfium::MakeUnique<CPDF_Dest>(array.get());
  EXPECT_FALSE(dest->GetXYZ(&hasX, &hasY, &hasZoom, &x, &y, &zoom));

  array->AddNew<CPDF_Number>(5);  // Y
  array->AddNew<CPDF_Number>(6);  // Zoom.

  dest = pdfium::MakeUnique<CPDF_Dest>(array.get());
  EXPECT_TRUE(dest->GetXYZ(&hasX, &hasY, &hasZoom, &x, &y, &zoom));
  EXPECT_TRUE(hasX);
  EXPECT_TRUE(hasY);
  EXPECT_TRUE(hasZoom);
  EXPECT_EQ(4, x);
  EXPECT_EQ(5, y);
  EXPECT_EQ(6, zoom);

  // Set zoom to 0.
  array->SetNewAt<CPDF_Number>(4, 0);
  dest = pdfium::MakeUnique<CPDF_Dest>(array.get());
  EXPECT_TRUE(dest->GetXYZ(&hasX, &hasY, &hasZoom, &x, &y, &zoom));
  EXPECT_FALSE(hasZoom);

  // Set values to null.
  array->SetNewAt<CPDF_Null>(2);
  array->SetNewAt<CPDF_Null>(3);
  array->SetNewAt<CPDF_Null>(4);
  dest = pdfium::MakeUnique<CPDF_Dest>(array.get());
  EXPECT_TRUE(dest->GetXYZ(&hasX, &hasY, &hasZoom, &x, &y, &zoom));
  EXPECT_FALSE(hasX);
  EXPECT_FALSE(hasY);
  EXPECT_FALSE(hasZoom);
}
