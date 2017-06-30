// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/fpdfview.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/test_support.h"

TEST(FPDFView, DoubleInit) {
  FPDF_InitLibrary();
  FPDF_InitLibrary();
  FPDF_DestroyLibrary();
}

TEST(FPDFView, DoubleDestroy) {
  FPDF_InitLibrary();
  FPDF_DestroyLibrary();
  FPDF_DestroyLibrary();
}
