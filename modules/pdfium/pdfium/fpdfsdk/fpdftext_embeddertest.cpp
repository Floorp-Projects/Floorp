// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "core/fxcrt/fx_basic.h"
#include "public/fpdf_text.h"
#include "public/fpdfview.h"
#include "testing/embedder_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/test_support.h"

namespace {

bool check_unsigned_shorts(const char* expected,
                           const unsigned short* actual,
                           size_t length) {
  if (length > strlen(expected) + 1) {
    return false;
  }
  for (size_t i = 0; i < length; ++i) {
    if (actual[i] != static_cast<unsigned short>(expected[i])) {
      return false;
    }
  }
  return true;
}

}  // namespace

class FPDFTextEmbeddertest : public EmbedderTest {};

TEST_F(FPDFTextEmbeddertest, Text) {
  EXPECT_TRUE(OpenDocument("hello_world.pdf"));
  FPDF_PAGE page = LoadPage(0);
  EXPECT_TRUE(page);

  FPDF_TEXTPAGE textpage = FPDFText_LoadPage(page);
  EXPECT_TRUE(textpage);

  static const char expected[] = "Hello, world!\r\nGoodbye, world!";
  unsigned short fixed_buffer[128];
  memset(fixed_buffer, 0xbd, sizeof(fixed_buffer));

  // Check includes the terminating NUL that is provided.
  int num_chars = FPDFText_GetText(textpage, 0, 128, fixed_buffer);
  ASSERT_GE(num_chars, 0);
  EXPECT_EQ(sizeof(expected), static_cast<size_t>(num_chars));
  EXPECT_TRUE(check_unsigned_shorts(expected, fixed_buffer, sizeof(expected)));

  // Count does not include the terminating NUL in the string literal.
  EXPECT_EQ(sizeof(expected) - 1,
            static_cast<size_t>(FPDFText_CountChars(textpage)));
  for (size_t i = 0; i < sizeof(expected) - 1; ++i) {
    EXPECT_EQ(static_cast<unsigned int>(expected[i]),
              FPDFText_GetUnicode(textpage, i))
        << " at " << i;
  }

  EXPECT_EQ(12.0, FPDFText_GetFontSize(textpage, 0));
  EXPECT_EQ(16.0, FPDFText_GetFontSize(textpage, 15));

  double left = 0.0;
  double right = 0.0;
  double bottom = 0.0;
  double top = 0.0;
  FPDFText_GetCharBox(textpage, 4, &left, &right, &bottom, &top);
  EXPECT_NEAR(41.071, left, 0.001);
  EXPECT_NEAR(46.243, right, 0.001);
  EXPECT_NEAR(49.844, bottom, 0.001);
  EXPECT_NEAR(55.520, top, 0.001);

  EXPECT_EQ(4, FPDFText_GetCharIndexAtPos(textpage, 42.0, 50.0, 1.0, 1.0));
  EXPECT_EQ(-1, FPDFText_GetCharIndexAtPos(textpage, 0.0, 0.0, 1.0, 1.0));
  EXPECT_EQ(-1, FPDFText_GetCharIndexAtPos(textpage, 199.0, 199.0, 1.0, 1.0));

  // Test out of range indicies.
  EXPECT_EQ(-1,
            FPDFText_GetCharIndexAtPos(textpage, 42.0, 10000000.0, 1.0, 1.0));
  EXPECT_EQ(-1, FPDFText_GetCharIndexAtPos(textpage, -1.0, 50.0, 1.0, 1.0));

  // Count does not include the terminating NUL in the string literal.
  EXPECT_EQ(2, FPDFText_CountRects(textpage, 0, sizeof(expected) - 1));

  left = 0.0;
  right = 0.0;
  bottom = 0.0;
  top = 0.0;
  FPDFText_GetRect(textpage, 1, &left, &top, &right, &bottom);
  EXPECT_NEAR(20.847, left, 0.001);
  EXPECT_NEAR(135.167, right, 0.001);
  EXPECT_NEAR(96.655, bottom, 0.001);
  EXPECT_NEAR(116.000, top, 0.001);

  // Test out of range indicies set outputs to (0.0, 0.0, 0.0, 0.0).
  left = -1.0;
  right = -1.0;
  bottom = -1.0;
  top = -1.0;
  FPDFText_GetRect(textpage, -1, &left, &top, &right, &bottom);
  EXPECT_EQ(0.0, left);
  EXPECT_EQ(0.0, right);
  EXPECT_EQ(0.0, bottom);
  EXPECT_EQ(0.0, top);

  left = -2.0;
  right = -2.0;
  bottom = -2.0;
  top = -2.0;
  FPDFText_GetRect(textpage, 2, &left, &top, &right, &bottom);
  EXPECT_EQ(0.0, left);
  EXPECT_EQ(0.0, right);
  EXPECT_EQ(0.0, bottom);
  EXPECT_EQ(0.0, top);

  EXPECT_EQ(9, FPDFText_GetBoundedText(textpage, 41.0, 56.0, 82.0, 48.0, 0, 0));

  // Extract starting at character 4 as above.
  memset(fixed_buffer, 0xbd, sizeof(fixed_buffer));
  EXPECT_EQ(1, FPDFText_GetBoundedText(textpage, 41.0, 56.0, 82.0, 48.0,
                                       fixed_buffer, 1));
  EXPECT_TRUE(check_unsigned_shorts(expected + 4, fixed_buffer, 1));
  EXPECT_EQ(0xbdbd, fixed_buffer[1]);

  memset(fixed_buffer, 0xbd, sizeof(fixed_buffer));
  EXPECT_EQ(9, FPDFText_GetBoundedText(textpage, 41.0, 56.0, 82.0, 48.0,
                                       fixed_buffer, 9));
  EXPECT_TRUE(check_unsigned_shorts(expected + 4, fixed_buffer, 9));
  EXPECT_EQ(0xbdbd, fixed_buffer[9]);

  memset(fixed_buffer, 0xbd, sizeof(fixed_buffer));
  EXPECT_EQ(10, FPDFText_GetBoundedText(textpage, 41.0, 56.0, 82.0, 48.0,
                                        fixed_buffer, 128));
  EXPECT_TRUE(check_unsigned_shorts(expected + 4, fixed_buffer, 9));
  EXPECT_EQ(0u, fixed_buffer[9]);
  EXPECT_EQ(0xbdbd, fixed_buffer[10]);

  FPDFText_ClosePage(textpage);
  UnloadPage(page);
}

TEST_F(FPDFTextEmbeddertest, TextSearch) {
  EXPECT_TRUE(OpenDocument("hello_world.pdf"));
  FPDF_PAGE page = LoadPage(0);
  EXPECT_TRUE(page);

  FPDF_TEXTPAGE textpage = FPDFText_LoadPage(page);
  EXPECT_TRUE(textpage);

  std::unique_ptr<unsigned short, pdfium::FreeDeleter> nope =
      GetFPDFWideString(L"nope");
  std::unique_ptr<unsigned short, pdfium::FreeDeleter> world =
      GetFPDFWideString(L"world");
  std::unique_ptr<unsigned short, pdfium::FreeDeleter> world_caps =
      GetFPDFWideString(L"WORLD");
  std::unique_ptr<unsigned short, pdfium::FreeDeleter> world_substr =
      GetFPDFWideString(L"orld");

  // No occurences of "nope" in test page.
  FPDF_SCHHANDLE search = FPDFText_FindStart(textpage, nope.get(), 0, 0);
  EXPECT_TRUE(search);
  EXPECT_EQ(0, FPDFText_GetSchResultIndex(search));
  EXPECT_EQ(0, FPDFText_GetSchCount(search));

  // Advancing finds nothing.
  EXPECT_FALSE(FPDFText_FindNext(search));
  EXPECT_EQ(0, FPDFText_GetSchResultIndex(search));
  EXPECT_EQ(0, FPDFText_GetSchCount(search));

  // Retreating finds nothing.
  EXPECT_FALSE(FPDFText_FindPrev(search));
  EXPECT_EQ(0, FPDFText_GetSchResultIndex(search));
  EXPECT_EQ(0, FPDFText_GetSchCount(search));
  FPDFText_FindClose(search);

  // Two occurences of "world" in test page.
  search = FPDFText_FindStart(textpage, world.get(), 0, 2);
  EXPECT_TRUE(search);

  // Remains not found until advanced.
  EXPECT_EQ(0, FPDFText_GetSchResultIndex(search));
  EXPECT_EQ(0, FPDFText_GetSchCount(search));

  // First occurence of "world" in this test page.
  EXPECT_TRUE(FPDFText_FindNext(search));
  EXPECT_EQ(7, FPDFText_GetSchResultIndex(search));
  EXPECT_EQ(5, FPDFText_GetSchCount(search));

  // Last occurence of "world" in this test page.
  EXPECT_TRUE(FPDFText_FindNext(search));
  EXPECT_EQ(24, FPDFText_GetSchResultIndex(search));
  EXPECT_EQ(5, FPDFText_GetSchCount(search));

  // Found position unchanged when fails to advance.
  EXPECT_FALSE(FPDFText_FindNext(search));
  EXPECT_EQ(24, FPDFText_GetSchResultIndex(search));
  EXPECT_EQ(5, FPDFText_GetSchCount(search));

  // Back to first occurence.
  EXPECT_TRUE(FPDFText_FindPrev(search));
  EXPECT_EQ(7, FPDFText_GetSchResultIndex(search));
  EXPECT_EQ(5, FPDFText_GetSchCount(search));

  // Found position unchanged when fails to retreat.
  EXPECT_FALSE(FPDFText_FindPrev(search));
  EXPECT_EQ(7, FPDFText_GetSchResultIndex(search));
  EXPECT_EQ(5, FPDFText_GetSchCount(search));
  FPDFText_FindClose(search);

  // Exact search unaffected by case sensitiity and whole word flags.
  search = FPDFText_FindStart(textpage, world.get(),
                              FPDF_MATCHCASE | FPDF_MATCHWHOLEWORD, 0);
  EXPECT_TRUE(search);
  EXPECT_TRUE(FPDFText_FindNext(search));
  EXPECT_EQ(7, FPDFText_GetSchResultIndex(search));
  EXPECT_EQ(5, FPDFText_GetSchCount(search));
  FPDFText_FindClose(search);

  // Default is case-insensitive, so matching agaist caps works.
  search = FPDFText_FindStart(textpage, world_caps.get(), 0, 0);
  EXPECT_TRUE(search);
  EXPECT_TRUE(FPDFText_FindNext(search));
  EXPECT_EQ(7, FPDFText_GetSchResultIndex(search));
  EXPECT_EQ(5, FPDFText_GetSchCount(search));
  FPDFText_FindClose(search);

  // But can be made case sensitive, in which case this fails.
  search = FPDFText_FindStart(textpage, world_caps.get(), FPDF_MATCHCASE, 0);
  EXPECT_FALSE(FPDFText_FindNext(search));
  EXPECT_EQ(0, FPDFText_GetSchResultIndex(search));
  EXPECT_EQ(0, FPDFText_GetSchCount(search));
  FPDFText_FindClose(search);

  // Default is match anywhere within word, so matching substirng works.
  search = FPDFText_FindStart(textpage, world_substr.get(), 0, 0);
  EXPECT_TRUE(FPDFText_FindNext(search));
  EXPECT_EQ(8, FPDFText_GetSchResultIndex(search));
  EXPECT_EQ(4, FPDFText_GetSchCount(search));
  FPDFText_FindClose(search);

  // But can be made to mach word boundaries, in which case this fails.
  search =
      FPDFText_FindStart(textpage, world_substr.get(), FPDF_MATCHWHOLEWORD, 0);
  EXPECT_FALSE(FPDFText_FindNext(search));
  // TODO(tsepez): investigate strange index/count values in this state.
  FPDFText_FindClose(search);

  FPDFText_ClosePage(textpage);
  UnloadPage(page);
}

// Test that the page has characters despite a bad stream length.
TEST_F(FPDFTextEmbeddertest, StreamLengthPastEndOfFile) {
  EXPECT_TRUE(OpenDocument("bug_57.pdf"));
  FPDF_PAGE page = LoadPage(0);
  EXPECT_TRUE(page);

  FPDF_TEXTPAGE textpage = FPDFText_LoadPage(page);
  EXPECT_TRUE(textpage);
  EXPECT_EQ(13, FPDFText_CountChars(textpage));

  FPDFText_ClosePage(textpage);
  UnloadPage(page);
}

TEST_F(FPDFTextEmbeddertest, WebLinks) {
  EXPECT_TRUE(OpenDocument("weblinks.pdf"));
  FPDF_PAGE page = LoadPage(0);
  EXPECT_TRUE(page);

  FPDF_TEXTPAGE textpage = FPDFText_LoadPage(page);
  EXPECT_TRUE(textpage);

  FPDF_PAGELINK pagelink = FPDFLink_LoadWebLinks(textpage);
  EXPECT_TRUE(pagelink);

  // Page contains two HTTP-style URLs.
  EXPECT_EQ(2, FPDFLink_CountWebLinks(pagelink));

  // Only a terminating NUL required for bogus links.
  EXPECT_EQ(1, FPDFLink_GetURL(pagelink, 2, nullptr, 0));
  EXPECT_EQ(1, FPDFLink_GetURL(pagelink, 1400, nullptr, 0));
  EXPECT_EQ(1, FPDFLink_GetURL(pagelink, -1, nullptr, 0));

  // Query the number of characters required for each link (incl NUL).
  EXPECT_EQ(25, FPDFLink_GetURL(pagelink, 0, nullptr, 0));
  EXPECT_EQ(26, FPDFLink_GetURL(pagelink, 1, nullptr, 0));

  static const char expected_url[] = "http://example.com?q=foo";
  static const size_t expected_len = sizeof(expected_url);
  unsigned short fixed_buffer[128];

  // Retrieve a link with too small a buffer.  Buffer will not be
  // NUL-terminated, but must not be modified past indicated length,
  // so pre-fill with a pattern to check write bounds.
  memset(fixed_buffer, 0xbd, sizeof(fixed_buffer));
  EXPECT_EQ(1, FPDFLink_GetURL(pagelink, 0, fixed_buffer, 1));
  EXPECT_TRUE(check_unsigned_shorts(expected_url, fixed_buffer, 1));
  EXPECT_EQ(0xbdbd, fixed_buffer[1]);

  // Check buffer that doesn't have space for a terminating NUL.
  memset(fixed_buffer, 0xbd, sizeof(fixed_buffer));
  EXPECT_EQ(static_cast<int>(expected_len - 1),
            FPDFLink_GetURL(pagelink, 0, fixed_buffer, expected_len - 1));
  EXPECT_TRUE(
      check_unsigned_shorts(expected_url, fixed_buffer, expected_len - 1));
  EXPECT_EQ(0xbdbd, fixed_buffer[expected_len - 1]);

  // Retreive link with exactly-sized buffer.
  memset(fixed_buffer, 0xbd, sizeof(fixed_buffer));
  EXPECT_EQ(static_cast<int>(expected_len),
            FPDFLink_GetURL(pagelink, 0, fixed_buffer, expected_len));
  EXPECT_TRUE(check_unsigned_shorts(expected_url, fixed_buffer, expected_len));
  EXPECT_EQ(0u, fixed_buffer[expected_len - 1]);
  EXPECT_EQ(0xbdbd, fixed_buffer[expected_len]);

  // Retreive link with ample-sized-buffer.
  memset(fixed_buffer, 0xbd, sizeof(fixed_buffer));
  EXPECT_EQ(static_cast<int>(expected_len),
            FPDFLink_GetURL(pagelink, 0, fixed_buffer, 128));
  EXPECT_TRUE(check_unsigned_shorts(expected_url, fixed_buffer, expected_len));
  EXPECT_EQ(0u, fixed_buffer[expected_len - 1]);
  EXPECT_EQ(0xbdbd, fixed_buffer[expected_len]);

  // Each link rendered in a single rect in this test page.
  EXPECT_EQ(1, FPDFLink_CountRects(pagelink, 0));
  EXPECT_EQ(1, FPDFLink_CountRects(pagelink, 1));

  // Each link rendered in a single rect in this test page.
  EXPECT_EQ(0, FPDFLink_CountRects(pagelink, -1));
  EXPECT_EQ(0, FPDFLink_CountRects(pagelink, 2));
  EXPECT_EQ(0, FPDFLink_CountRects(pagelink, 10000));

  // Check boundary of valid link index with valid rect index.
  double left = 0.0;
  double right = 0.0;
  double top = 0.0;
  double bottom = 0.0;
  FPDFLink_GetRect(pagelink, 0, 0, &left, &top, &right, &bottom);
  EXPECT_NEAR(50.791, left, 0.001);
  EXPECT_NEAR(187.963, right, 0.001);
  EXPECT_NEAR(97.624, bottom, 0.001);
  EXPECT_NEAR(108.736, top, 0.001);

  // Check that valid link with invalid rect index leaves parameters unchanged.
  left = -1.0;
  right = -1.0;
  top = -1.0;
  bottom = -1.0;
  FPDFLink_GetRect(pagelink, 0, 1, &left, &top, &right, &bottom);
  EXPECT_EQ(-1.0, left);
  EXPECT_EQ(-1.0, right);
  EXPECT_EQ(-1.0, bottom);
  EXPECT_EQ(-1.0, top);

  // Check that invalid link index leaves parameters unchanged.
  left = -2.0;
  right = -2.0;
  top = -2.0;
  bottom = -2.0;
  FPDFLink_GetRect(pagelink, -1, 0, &left, &top, &right, &bottom);
  EXPECT_EQ(-2.0, left);
  EXPECT_EQ(-2.0, right);
  EXPECT_EQ(-2.0, bottom);
  EXPECT_EQ(-2.0, top);

  FPDFLink_CloseWebLinks(pagelink);
  FPDFText_ClosePage(textpage);
  UnloadPage(page);
}

TEST_F(FPDFTextEmbeddertest, GetFontSize) {
  EXPECT_TRUE(OpenDocument("hello_world.pdf"));
  FPDF_PAGE page = LoadPage(0);
  EXPECT_TRUE(page);

  FPDF_TEXTPAGE textpage = FPDFText_LoadPage(page);
  EXPECT_TRUE(textpage);

  const double kExpectedFontsSizes[] = {12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
                                        12, 12, 12, 1,  1,  16, 16, 16, 16, 16,
                                        16, 16, 16, 16, 16, 16, 16, 16, 16, 16};

  int count = FPDFText_CountChars(textpage);
  ASSERT_EQ(FX_ArraySize(kExpectedFontsSizes), static_cast<size_t>(count));
  for (int i = 0; i < count; ++i)
    EXPECT_EQ(kExpectedFontsSizes[i], FPDFText_GetFontSize(textpage, i)) << i;

  FPDFText_ClosePage(textpage);
  UnloadPage(page);
}

TEST_F(FPDFTextEmbeddertest, ToUnicode) {
  EXPECT_TRUE(OpenDocument("bug_583.pdf"));
  FPDF_PAGE page = LoadPage(0);
  EXPECT_TRUE(page);

  FPDF_TEXTPAGE textpage = FPDFText_LoadPage(page);
  EXPECT_TRUE(textpage);

  ASSERT_EQ(1, FPDFText_CountChars(textpage));
  EXPECT_EQ(static_cast<unsigned int>(0), FPDFText_GetUnicode(textpage, 0));

  FPDFText_ClosePage(textpage);
  UnloadPage(page);
}
