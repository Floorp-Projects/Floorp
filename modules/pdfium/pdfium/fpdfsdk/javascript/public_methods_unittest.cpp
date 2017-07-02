// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fpdfsdk/javascript/PublicMethods.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/test_support.h"

TEST(CJS_PublicMethods, IsNumber) {
  // TODO(weili): Check whether results from case 0, 1, 10, 15 are intended.
  struct {
    const wchar_t* input;
    bool expected;
  } test_data[] = {
      // Empty string.
      {L"", true},
      // Only whitespaces.
      {L"  ", true},
      // Content with invalid characters.
      {L"xyz00", false},
      {L"1%", false},
      // Hex string.
      {L"0x234", false},
      // Signed numbers.
      {L"+123", true},
      {L"-98765", true},
      // Numbers with whitespaces.
      {L"  345 ", true},
      // Float numbers.
      {L"-1e5", false},
      {L"-2e", false},
      {L"e-5", true},
      {L"0.023", true},
      {L".356089", true},
      {L"1e-9", true},
      {L"-1.23e+23", true},
      // Numbers with commas.
      {L"1,000,000", false},
      {L"560,024", true},
      // Regular numbers.
      {L"0", true},
      {L"0123", true},
      {L"9876123", true},
  };
  for (size_t i = 0; i < FX_ArraySize(test_data); ++i) {
    EXPECT_EQ(test_data[i].expected,
              CJS_PublicMethods::IsNumber(test_data[i].input))
        << "for case " << i;
  }
}
