// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfapi/parser/fpdf_parser_decode.h"

#include <cstring>
#include <string>

#include "core/fxcrt/fx_basic.h"
#include "testing/embedder_test.h"
#include "testing/fx_string_testhelpers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/test_support.h"

class FPDFParserDecodeEmbeddertest : public EmbedderTest {};

// NOTE: python's zlib.compress() and zlib.decompress() may be useful for
// external validation of the FlateEncode/FlateDecode test cases.

TEST_F(FPDFParserDecodeEmbeddertest, FlateEncode) {
  pdfium::StrFuncTestData flate_encode_cases[] = {
      STR_IN_OUT_CASE("", "\x78\x9c\x03\x00\x00\x00\x00\x01"),
      STR_IN_OUT_CASE(" ", "\x78\x9c\x53\x00\x00\x00\x21\x00\x21"),
      STR_IN_OUT_CASE("123", "\x78\x9c\x33\x34\x32\x06\x00\01\x2d\x00\x97"),
      STR_IN_OUT_CASE("\x00\xff", "\x78\x9c\x63\xf8\x0f\x00\x01\x01\x01\x00"),
      STR_IN_OUT_CASE(
          "1 0 0 -1 29 763 cm\n0 0 555 735 re\nW n\nq\n0 0 555 734.394 re\n"
          "W n\nq\n0.8009 0 0 0.8009 0 0 cm\n1 1 1 RG 1 1 1 rg\n/G0 gs\n"
          "0 0 693 917 re\nf\nQ\nQ\n",
          "\x78\x9c\x33\x54\x30\x00\x42\x5d\x43\x05\x23\x4b\x05\x73\x33\x63"
          "\x85\xe4\x5c\x2e\x90\x80\xa9\xa9\xa9\x82\xb9\xb1\xa9\x42\x51\x2a"
          "\x57\xb8\x42\x1e\x57\x21\x92\xa0\x89\x9e\xb1\xa5\x09\x92\x84\x9e"
          "\x85\x81\x81\x25\xd8\x14\x24\x26\xd0\x18\x43\x05\x10\x0c\x72\x57"
          "\x80\x30\x8a\xd2\xb9\xf4\xdd\x0d\x14\xd2\x8b\xc1\x46\x99\x59\x1a"
          "\x2b\x58\x1a\x9a\x83\x8c\x49\xe3\x0a\x04\x42\x00\x37\x4c\x1b\x42"),
  };

  for (size_t i = 0; i < FX_ArraySize(flate_encode_cases); ++i) {
    const pdfium::StrFuncTestData& data = flate_encode_cases[i];
    unsigned char* buf = nullptr;
    unsigned int buf_size;
    EXPECT_TRUE(FlateEncode(data.input, data.input_size, &buf, &buf_size));
    ASSERT_TRUE(buf);
    EXPECT_EQ(std::string((const char*)data.expected, data.expected_size),
              std::string((const char*)buf, buf_size))
        << " for case " << i;
    FX_Free(buf);
  }
}

TEST_F(FPDFParserDecodeEmbeddertest, FlateDecode) {
  pdfium::DecodeTestData flate_decode_cases[] = {
      STR_IN_OUT_CASE("", "", 0),
      STR_IN_OUT_CASE("preposterous nonsense", "", 2),
      STR_IN_OUT_CASE("\x78\x9c\x03\x00\x00\x00\x00\x01", "", 8),
      STR_IN_OUT_CASE("\x78\x9c\x53\x00\x00\x00\x21\x00\x21", " ", 9),
      STR_IN_OUT_CASE("\x78\x9c\x33\x34\x32\x06\x00\01\x2d\x00\x97", "123", 11),
      STR_IN_OUT_CASE("\x78\x9c\x63\xf8\x0f\x00\x01\x01\x01\x00", "\x00\xff",
                      10),
      STR_IN_OUT_CASE(
          "\x78\x9c\x33\x54\x30\x00\x42\x5d\x43\x05\x23\x4b\x05\x73\x33\x63"
          "\x85\xe4\x5c\x2e\x90\x80\xa9\xa9\xa9\x82\xb9\xb1\xa9\x42\x51\x2a"
          "\x57\xb8\x42\x1e\x57\x21\x92\xa0\x89\x9e\xb1\xa5\x09\x92\x84\x9e"
          "\x85\x81\x81\x25\xd8\x14\x24\x26\xd0\x18\x43\x05\x10\x0c\x72\x57"
          "\x80\x30\x8a\xd2\xb9\xf4\xdd\x0d\x14\xd2\x8b\xc1\x46\x99\x59\x1a"
          "\x2b\x58\x1a\x9a\x83\x8c\x49\xe3\x0a\x04\x42\x00\x37\x4c\x1b\x42",
          "1 0 0 -1 29 763 cm\n0 0 555 735 re\nW n\nq\n0 0 555 734.394 re\n"
          "W n\nq\n0.8009 0 0 0.8009 0 0 cm\n1 1 1 RG 1 1 1 rg\n/G0 gs\n"
          "0 0 693 917 re\nf\nQ\nQ\n",
          96),
  };

  for (size_t i = 0; i < FX_ArraySize(flate_decode_cases); ++i) {
    const pdfium::DecodeTestData& data = flate_decode_cases[i];
    unsigned char* result = nullptr;
    unsigned int result_size;
    EXPECT_EQ(data.processed_size,
              FlateDecode(data.input, data.input_size, result, result_size))
        << " for case " << i;
    ASSERT_TRUE(result);
    EXPECT_EQ(std::string((const char*)data.expected, data.expected_size),
              std::string((const char*)result, result_size))
        << " for case " << i;
    FX_Free(result);
  }
}

TEST_F(FPDFParserDecodeEmbeddertest, Bug_552046) {
  // Tests specifying multiple image filters for a stream. Should not cause a
  // crash when rendered.
  EXPECT_TRUE(OpenDocument("bug_552046.pdf"));
  FPDF_PAGE page = LoadPage(0);
  FPDF_BITMAP bitmap = RenderPage(page);
  CompareBitmap(bitmap, 612, 792, "1940568c9ba33bac5d0b1ee9558c76b3");
  FPDFBitmap_Destroy(bitmap);
  UnloadPage(page);
}

TEST_F(FPDFParserDecodeEmbeddertest, Bug_555784) {
  // Tests bad input to the run length decoder that caused a heap overflow.
  // Should not cause a crash when rendered.
  EXPECT_TRUE(OpenDocument("bug_555784.pdf"));
  FPDF_PAGE page = LoadPage(0);
  FPDF_BITMAP bitmap = RenderPage(page);
  CompareBitmap(bitmap, 612, 792, "1940568c9ba33bac5d0b1ee9558c76b3");
  FPDFBitmap_Destroy(bitmap);
  UnloadPage(page);
}

TEST_F(FPDFParserDecodeEmbeddertest, Bug_455199) {
  // Tests object numbers with a value > 01000000.
  // Should open successfully.
  EXPECT_TRUE(OpenDocument("bug_455199.pdf"));
  FPDF_PAGE page = LoadPage(0);
  FPDF_BITMAP bitmap = RenderPage(page);
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  // Rendering on Mac is inconsistent, so skip this check.
  const char* const kExpectedMd5sum = nullptr;
#else
  const char kExpectedMd5sum[] = "6f9f0fd903da177babb24dd50a806a56";
#endif
  CompareBitmap(bitmap, 200, 200, kExpectedMd5sum);
  FPDFBitmap_Destroy(bitmap);
  UnloadPage(page);
}
