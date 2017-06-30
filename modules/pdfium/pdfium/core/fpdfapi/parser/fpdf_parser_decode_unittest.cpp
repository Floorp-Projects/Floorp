// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfapi/parser/fpdf_parser_decode.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/test_support.h"

TEST(fpdf_parser_decode, A85Decode) {
  pdfium::DecodeTestData test_data[] = {
      // Empty src string.
      STR_IN_OUT_CASE("", "", 0),
      // Empty content in src string.
      STR_IN_OUT_CASE("~>", "", 0),
      // Regular conversion.
      STR_IN_OUT_CASE("FCfN8~>", "test", 7),
      // End at the ending mark.
      STR_IN_OUT_CASE("FCfN8~>FCfN8", "test", 7),
      // Skip whitespaces.
      STR_IN_OUT_CASE("\t F C\r\n \tf N 8 ~>", "test", 17),
      // No ending mark.
      STR_IN_OUT_CASE("@3B0)DJj_BF*)>@Gp#-s", "a funny story :)", 20),
      // Non-multiple length.
      STR_IN_OUT_CASE("12A", "2k", 3),
      // Stop at unknown characters.
      STR_IN_OUT_CASE("FCfN8FCfN8vw", "testtest", 11),
  };
  for (size_t i = 0; i < FX_ArraySize(test_data); ++i) {
    pdfium::DecodeTestData* ptr = &test_data[i];
    uint8_t* result = nullptr;
    uint32_t result_size = 0;
    EXPECT_EQ(ptr->processed_size,
              A85Decode(ptr->input, ptr->input_size, result, result_size))
        << "for case " << i;
    ASSERT_EQ(ptr->expected_size, result_size);
    for (size_t j = 0; j < result_size; ++j) {
      EXPECT_EQ(ptr->expected[j], result[j]) << "for case " << i << " char "
                                             << j;
    }
    FX_Free(result);
  }
}

TEST(fpdf_parser_decode, HexDecode) {
  pdfium::DecodeTestData test_data[] = {
      // Empty src string.
      STR_IN_OUT_CASE("", "", 0),
      // Empty content in src string.
      STR_IN_OUT_CASE(">", "", 1),
      // Only whitespaces in src string.
      STR_IN_OUT_CASE("\t   \r\n>", "", 7),
      // Regular conversion.
      STR_IN_OUT_CASE("12Ac>zzz", "\x12\xac", 5),
      // Skip whitespaces.
      STR_IN_OUT_CASE("12 Ac\t02\r\nBF>zzz>", "\x12\xac\x02\xbf", 13),
      // Non-multiple length.
      STR_IN_OUT_CASE("12A>zzz", "\x12\xa0", 4),
      // Skips unknown characters.
      STR_IN_OUT_CASE("12tk  \tAc>zzz", "\x12\xac", 10),
      // No ending mark.
      STR_IN_OUT_CASE("12AcED3c3456", "\x12\xac\xed\x3c\x34\x56", 12),
  };
  for (size_t i = 0; i < FX_ArraySize(test_data); ++i) {
    pdfium::DecodeTestData* ptr = &test_data[i];
    uint8_t* result = nullptr;
    uint32_t result_size = 0;
    EXPECT_EQ(ptr->processed_size,
              HexDecode(ptr->input, ptr->input_size, result, result_size))
        << "for case " << i;
    ASSERT_EQ(ptr->expected_size, result_size);
    for (size_t j = 0; j < result_size; ++j) {
      EXPECT_EQ(ptr->expected[j], result[j]) << "for case " << i << " char "
                                             << j;
    }
    FX_Free(result);
  }
}

TEST(fpdf_parser_decode, EncodeText) {
  struct EncodeTestData {
    const FX_WCHAR* input;
    const FX_CHAR* expected_output;
    FX_STRSIZE expected_length;
  } test_data[] = {
      // Empty src string.
      {L"", "", 0},
      // ASCII text.
      {L"the quick\tfox", "the quick\tfox", 13},
      // Unicode text.
      {L"\x0330\x0331", "\xFE\xFF\x03\x30\x03\x31", 6},
      // More Unicode text.
      {L"\x7F51\x9875\x0020\x56FE\x7247\x0020"
       L"\x8D44\x8BAF\x66F4\x591A\x0020\x00BB",
       "\xFE\xFF\x7F\x51\x98\x75\x00\x20\x56\xFE\x72\x47\x00"
       "\x20\x8D\x44\x8B\xAF\x66\xF4\x59\x1A\x00\x20\x00\xBB",
       26},
  };

  for (size_t i = 0; i < FX_ArraySize(test_data); ++i) {
    const auto& test_case = test_data[i];
    CFX_ByteString output = PDF_EncodeText(test_case.input);
    ASSERT_EQ(test_case.expected_length, output.GetLength()) << "for case "
                                                             << i;
    const FX_CHAR* str_ptr = output.c_str();
    for (FX_STRSIZE j = 0; j < test_case.expected_length; ++j) {
      EXPECT_EQ(test_case.expected_output[j], str_ptr[j]) << "for case " << i
                                                          << " char " << j;
    }
  }
}
