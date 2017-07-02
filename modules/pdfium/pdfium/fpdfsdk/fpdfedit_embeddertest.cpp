// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "core/fpdfapi/font/cpdf_font.h"
#include "core/fpdfapi/page/cpdf_page.h"
#include "core/fpdfapi/parser/cpdf_array.h"
#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_stream.h"
#include "core/fxcrt/fx_system.h"
#include "fpdfsdk/fsdk_define.h"
#include "public/fpdf_edit.h"
#include "public/fpdfview.h"
#include "testing/embedder_test.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/test_support.h"

class FPDFEditEmbeddertest : public EmbedderTest, public TestSaver {};

namespace {

const char kExpectedPDF[] =
    "%PDF-1.7\r\n"
    "%\xA1\xB3\xC5\xD7\r\n"
    "1 0 obj\r\n"
    "<</Pages 2 0 R /Type/Catalog>>\r\n"
    "endobj\r\n"
    "2 0 obj\r\n"
    "<</Count 1/Kids\\[ 4 0 R \\]/Type/Pages>>\r\n"
    "endobj\r\n"
    "3 0 obj\r\n"
    "<</CreationDate\\(D:.*\\)/Creator\\(PDFium\\)>>\r\n"
    "endobj\r\n"
    "4 0 obj\r\n"
    "<</Contents 5 0 R /MediaBox\\[ 0 0 640 480\\]"
    "/Parent 2 0 R /Resources<<>>/Rotate 0/Type/Page"
    ">>\r\n"
    "endobj\r\n"
    "5 0 obj\r\n"
    "<</Filter/FlateDecode/Length 8>>stream\r\n"
    // Character '_' is matching '\0' (see comment below).
    "x\x9C\x3____\x1\r\n"
    "endstream\r\n"
    "endobj\r\n"
    "xref\r\n"
    "0 6\r\n"
    "0000000000 65535 f\r\n"
    "0000000017 00000 n\r\n"
    "0000000066 00000 n\r\n"
    "0000000122 00000 n\r\n"
    "0000000192 00000 n\r\n"
    "0000000301 00000 n\r\n"
    "trailer\r\n"
    "<<\r\n"
    "/Root 1 0 R\r\n"
    "/Info 3 0 R\r\n"
    "/Size 6/ID\\[<.*><.*>\\]>>\r\n"
    "startxref\r\n"
    "379\r\n"
    "%%EOF\r\n";

int GetBlockFromString(void* param,
                       unsigned long pos,
                       unsigned char* buf,
                       unsigned long size) {
  std::string* new_file = static_cast<std::string*>(param);
  if (!new_file || pos + size < pos)
    return 0;

  unsigned long file_size = new_file->size();
  if (pos + size > file_size)
    return 0;

  memcpy(buf, new_file->data() + pos, size);
  return 1;
}

}  // namespace

TEST_F(FPDFEditEmbeddertest, EmptyCreation) {
  EXPECT_TRUE(CreateEmptyDocument());
  FPDF_PAGE page = FPDFPage_New(document(), 0, 640.0, 480.0);
  EXPECT_NE(nullptr, page);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_TRUE(FPDF_SaveAsCopy(document(), this, 0));

  // The MatchesRegexp doesn't support embedded NUL ('\0') characters. They are
  // replaced by '_' for the purpose of the test.
  std::string result = GetString();
  std::replace(result.begin(), result.end(), '\0', '_');
  EXPECT_THAT(result, testing::MatchesRegex(
                          std::string(kExpectedPDF, sizeof(kExpectedPDF))));
  FPDF_ClosePage(page);
}

// Regression test for https://crbug.com/667012
TEST_F(FPDFEditEmbeddertest, RasterizePDF) {
  const char kAllBlackMd5sum[] = "5708fc5c4a8bd0abde99c8e8f0390615";

  // Get the bitmap for the original document/
  FPDF_BITMAP orig_bitmap;
  {
    EXPECT_TRUE(OpenDocument("black.pdf"));
    FPDF_PAGE orig_page = LoadPage(0);
    EXPECT_NE(nullptr, orig_page);
    orig_bitmap = RenderPage(orig_page);
    CompareBitmap(orig_bitmap, 612, 792, kAllBlackMd5sum);
    UnloadPage(orig_page);
  }

  // Create a new document from |orig_bitmap| and save it.
  {
    FPDF_DOCUMENT temp_doc = FPDF_CreateNewDocument();
    FPDF_PAGE temp_page = FPDFPage_New(temp_doc, 0, 612, 792);

    // Add the bitmap to an image object and add the image object to the output
    // page.
    FPDF_PAGEOBJECT temp_img = FPDFPageObj_NewImgeObj(temp_doc);
    EXPECT_TRUE(FPDFImageObj_SetBitmap(&temp_page, 1, temp_img, orig_bitmap));
    EXPECT_TRUE(FPDFImageObj_SetMatrix(temp_img, 612, 0, 0, 792, 0, 0));
    FPDFPage_InsertObject(temp_page, temp_img);
    EXPECT_TRUE(FPDFPage_GenerateContent(temp_page));
    EXPECT_TRUE(FPDF_SaveAsCopy(temp_doc, this, 0));
    FPDF_ClosePage(temp_page);
    FPDF_CloseDocument(temp_doc);
  }
  FPDFBitmap_Destroy(orig_bitmap);

  // Get the generated content. Make sure it is at least as big as the original
  // PDF.
  std::string new_file = GetString();
  EXPECT_GT(new_file.size(), 923U);

  // Read |new_file| in, and verify its rendered bitmap.
  {
    FPDF_FILEACCESS file_access;
    memset(&file_access, 0, sizeof(file_access));
    file_access.m_FileLen = new_file.size();
    file_access.m_GetBlock = GetBlockFromString;
    file_access.m_Param = &new_file;

    FPDF_DOCUMENT new_doc = FPDF_LoadCustomDocument(&file_access, nullptr);
    EXPECT_EQ(1, FPDF_GetPageCount(document_));
    FPDF_PAGE new_page = FPDF_LoadPage(new_doc, 0);
    EXPECT_NE(nullptr, new_page);
    FPDF_BITMAP new_bitmap = RenderPage(new_page);
    CompareBitmap(new_bitmap, 612, 792, kAllBlackMd5sum);
    FPDF_ClosePage(new_page);
    FPDF_CloseDocument(new_doc);
    FPDFBitmap_Destroy(new_bitmap);
  }
}

TEST_F(FPDFEditEmbeddertest, AddPaths) {
  // Start with a blank page
  FPDF_DOCUMENT doc = FPDF_CreateNewDocument();
  FPDF_PAGE page = FPDFPage_New(doc, 0, 612, 792);

  // We will first add a red rectangle
  FPDF_PAGEOBJECT red_rect = FPDFPageObj_CreateNewRect(10, 10, 20, 20);
  ASSERT_NE(nullptr, red_rect);
  // Expect false when trying to set colors out of range
  EXPECT_FALSE(FPDFPath_SetStrokeColor(red_rect, 100, 100, 100, 300));
  EXPECT_FALSE(FPDFPath_SetFillColor(red_rect, 200, 256, 200, 0));

  // Fill rectangle with red and insert to the page
  EXPECT_TRUE(FPDFPath_SetFillColor(red_rect, 255, 0, 0, 255));
  EXPECT_TRUE(FPDFPath_SetDrawMode(red_rect, FPDF_FILLMODE_ALTERNATE, 0));
  FPDFPage_InsertObject(page, red_rect);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  FPDF_BITMAP page_bitmap = RenderPage(page);
  CompareBitmap(page_bitmap, 612, 792, "66d02eaa6181e2c069ce2ea99beda497");
  FPDFBitmap_Destroy(page_bitmap);

  // Now add to that a green rectangle with some medium alpha
  FPDF_PAGEOBJECT green_rect = FPDFPageObj_CreateNewRect(100, 100, 40, 40);
  EXPECT_TRUE(FPDFPath_SetFillColor(green_rect, 0, 255, 0, 128));
  EXPECT_TRUE(FPDFPath_SetDrawMode(green_rect, FPDF_FILLMODE_WINDING, 0));
  FPDFPage_InsertObject(page, green_rect);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  page_bitmap = RenderPage(page);
  CompareBitmap(page_bitmap, 612, 792, "7b0b87604594e773add528fae567a558");
  FPDFBitmap_Destroy(page_bitmap);

  // Add a black triangle.
  FPDF_PAGEOBJECT black_path = FPDFPageObj_CreateNewPath(400, 100);
  EXPECT_TRUE(FPDFPath_SetFillColor(black_path, 0, 0, 0, 200));
  EXPECT_TRUE(FPDFPath_SetDrawMode(black_path, FPDF_FILLMODE_ALTERNATE, 0));
  EXPECT_TRUE(FPDFPath_LineTo(black_path, 400, 200));
  EXPECT_TRUE(FPDFPath_LineTo(black_path, 300, 100));
  EXPECT_TRUE(FPDFPath_Close(black_path));
  FPDFPage_InsertObject(page, black_path);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  page_bitmap = RenderPage(page);
  CompareBitmap(page_bitmap, 612, 792, "eadc8020a14dfcf091da2688733d8806");
  FPDFBitmap_Destroy(page_bitmap);

  // Now add a more complex blue path.
  FPDF_PAGEOBJECT blue_path = FPDFPageObj_CreateNewPath(200, 200);
  EXPECT_TRUE(FPDFPath_SetFillColor(blue_path, 0, 0, 255, 255));
  EXPECT_TRUE(FPDFPath_SetDrawMode(blue_path, FPDF_FILLMODE_WINDING, 0));
  EXPECT_TRUE(FPDFPath_LineTo(blue_path, 230, 230));
  EXPECT_TRUE(FPDFPath_BezierTo(blue_path, 250, 250, 280, 280, 300, 300));
  EXPECT_TRUE(FPDFPath_LineTo(blue_path, 325, 325));
  EXPECT_TRUE(FPDFPath_LineTo(blue_path, 350, 325));
  EXPECT_TRUE(FPDFPath_BezierTo(blue_path, 375, 330, 390, 360, 400, 400));
  EXPECT_TRUE(FPDFPath_Close(blue_path));
  FPDFPage_InsertObject(page, blue_path);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  page_bitmap = RenderPage(page);
  CompareBitmap(page_bitmap, 612, 792, "9823e1a21bd9b72b6a442ba4f12af946");
  FPDFBitmap_Destroy(page_bitmap);

  // Now save the result, closing the page and document
  EXPECT_TRUE(FPDF_SaveAsCopy(doc, this, 0));
  FPDF_ClosePage(page);
  FPDF_CloseDocument(doc);
  std::string new_file = GetString();

  // Render the saved result
  FPDF_FILEACCESS file_access;
  memset(&file_access, 0, sizeof(file_access));
  file_access.m_FileLen = new_file.size();
  file_access.m_GetBlock = GetBlockFromString;
  file_access.m_Param = &new_file;
  FPDF_DOCUMENT new_doc = FPDF_LoadCustomDocument(&file_access, nullptr);
  ASSERT_NE(nullptr, new_doc);
  EXPECT_EQ(1, FPDF_GetPageCount(new_doc));
  FPDF_PAGE new_page = FPDF_LoadPage(new_doc, 0);
  ASSERT_NE(nullptr, new_page);
  FPDF_BITMAP new_bitmap = RenderPage(new_page);
  CompareBitmap(new_bitmap, 612, 792, "9823e1a21bd9b72b6a442ba4f12af946");
  FPDFBitmap_Destroy(new_bitmap);
  FPDF_ClosePage(new_page);
  FPDF_CloseDocument(new_doc);
}

TEST_F(FPDFEditEmbeddertest, PathOnTopOfText) {
  // Load document with some text
  EXPECT_TRUE(OpenDocument("hello_world.pdf"));
  FPDF_PAGE page = LoadPage(0);
  EXPECT_NE(nullptr, page);

  // Add an opaque rectangle on top of some of the text.
  FPDF_PAGEOBJECT red_rect = FPDFPageObj_CreateNewRect(20, 100, 50, 50);
  EXPECT_TRUE(FPDFPath_SetFillColor(red_rect, 255, 0, 0, 255));
  EXPECT_TRUE(FPDFPath_SetDrawMode(red_rect, FPDF_FILLMODE_ALTERNATE, 0));
  FPDFPage_InsertObject(page, red_rect);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));

  // Add a transparent triangle on top of other part of the text.
  FPDF_PAGEOBJECT black_path = FPDFPageObj_CreateNewPath(20, 50);
  EXPECT_TRUE(FPDFPath_SetFillColor(black_path, 0, 0, 0, 100));
  EXPECT_TRUE(FPDFPath_SetDrawMode(black_path, FPDF_FILLMODE_ALTERNATE, 0));
  EXPECT_TRUE(FPDFPath_LineTo(black_path, 30, 80));
  EXPECT_TRUE(FPDFPath_LineTo(black_path, 40, 10));
  EXPECT_TRUE(FPDFPath_Close(black_path));
  FPDFPage_InsertObject(page, black_path);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));

  // Render and check the result. Text is slightly different on Mac.
  FPDF_BITMAP bitmap = RenderPage(page);
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  const char md5[] = "2f7c0deee10a9490538e195af64beb67";
#else
  const char md5[] = "17c942c76ff229200f2c98073bb60d85";
#endif
  CompareBitmap(bitmap, 200, 200, md5);
  FPDFBitmap_Destroy(bitmap);
  UnloadPage(page);
}

TEST_F(FPDFEditEmbeddertest, AddStrokedPaths) {
  // Start with a blank page
  FPDF_DOCUMENT doc = FPDF_CreateNewDocument();
  FPDF_PAGE page = FPDFPage_New(doc, 0, 612, 792);

  // Add a large stroked rectangle (fill color should not affect it).
  FPDF_PAGEOBJECT rect = FPDFPageObj_CreateNewRect(20, 20, 200, 400);
  EXPECT_TRUE(FPDFPath_SetFillColor(rect, 255, 0, 0, 255));
  EXPECT_TRUE(FPDFPath_SetStrokeColor(rect, 0, 255, 0, 255));
  EXPECT_TRUE(FPDFPath_SetStrokeWidth(rect, 15.0f));
  EXPECT_TRUE(FPDFPath_SetDrawMode(rect, 0, 1));
  FPDFPage_InsertObject(page, rect);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  FPDF_BITMAP page_bitmap = RenderPage(page);
  CompareBitmap(page_bitmap, 612, 792, "64bd31f862a89e0a9e505a5af6efd506");
  FPDFBitmap_Destroy(page_bitmap);

  // Add crossed-checkmark
  FPDF_PAGEOBJECT check = FPDFPageObj_CreateNewPath(300, 500);
  EXPECT_TRUE(FPDFPath_LineTo(check, 400, 400));
  EXPECT_TRUE(FPDFPath_LineTo(check, 600, 600));
  EXPECT_TRUE(FPDFPath_MoveTo(check, 400, 600));
  EXPECT_TRUE(FPDFPath_LineTo(check, 600, 400));
  EXPECT_TRUE(FPDFPath_SetStrokeColor(check, 128, 128, 128, 180));
  EXPECT_TRUE(FPDFPath_SetStrokeWidth(check, 8.35f));
  EXPECT_TRUE(FPDFPath_SetDrawMode(check, 0, 1));
  FPDFPage_InsertObject(page, check);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  page_bitmap = RenderPage(page);
  CompareBitmap(page_bitmap, 612, 792, "4b6f3b9d25c4e194821217d5016c3724");
  FPDFBitmap_Destroy(page_bitmap);

  // Add stroked and filled oval-ish path.
  FPDF_PAGEOBJECT path = FPDFPageObj_CreateNewPath(250, 100);
  EXPECT_TRUE(FPDFPath_BezierTo(path, 180, 166, 180, 233, 250, 300));
  EXPECT_TRUE(FPDFPath_LineTo(path, 255, 305));
  EXPECT_TRUE(FPDFPath_BezierTo(path, 325, 233, 325, 166, 255, 105));
  EXPECT_TRUE(FPDFPath_Close(path));
  EXPECT_TRUE(FPDFPath_SetFillColor(path, 200, 128, 128, 100));
  EXPECT_TRUE(FPDFPath_SetStrokeColor(path, 128, 200, 128, 150));
  EXPECT_TRUE(FPDFPath_SetStrokeWidth(path, 10.5f));
  EXPECT_TRUE(FPDFPath_SetDrawMode(path, FPDF_FILLMODE_ALTERNATE, 1));
  FPDFPage_InsertObject(page, path);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  page_bitmap = RenderPage(page);
  CompareBitmap(page_bitmap, 612, 792, "ff3e6a22326754944cc6e56609acd73b");
  FPDFBitmap_Destroy(page_bitmap);
  FPDF_ClosePage(page);
  FPDF_CloseDocument(doc);
}

TEST_F(FPDFEditEmbeddertest, AddStandardFontText) {
  // Start with a blank page
  FPDF_DOCUMENT doc = FPDF_CreateNewDocument();
  FPDF_PAGE page = FPDFPage_New(doc, 0, 612, 792);

  // Add some text to the page
  FPDF_PAGEOBJECT text1 = FPDFPageObj_NewTextObj(doc, "Arial", 12.0f);
  EXPECT_TRUE(text1);
  EXPECT_TRUE(FPDFText_SetText(text1, "I'm at the bottom of the page"));
  FPDFPageObj_Transform(text1, 1, 0, 0, 1, 20, 20);
  FPDFPage_InsertObject(page, text1);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  FPDF_BITMAP page_bitmap = RenderPage(page);
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  const char md5[] = "e19c90395d73cb9f37a6c3b0e8b18a9e";
#else
  const char md5[] = "7c3a36ba7cec01688a16a14bfed9ecfc";
#endif
  CompareBitmap(page_bitmap, 612, 792, md5);
  FPDFBitmap_Destroy(page_bitmap);

  // Try another font
  FPDF_PAGEOBJECT text2 =
      FPDFPageObj_NewTextObj(doc, "TimesNewRomanBold", 15.0f);
  EXPECT_TRUE(text2);
  EXPECT_TRUE(FPDFText_SetText(text2, "Hi, I'm Bold. Times New Roman Bold."));
  FPDFPageObj_Transform(text2, 1, 0, 0, 1, 100, 600);
  FPDFPage_InsertObject(page, text2);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  page_bitmap = RenderPage(page);
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  const char md5_2[] = "8e1c43dca6be68d364dbc283f5521041";
#else
  const char md5_2[] = "e0e0873e3a2634a6394a431a51ce90ff";
#endif
  CompareBitmap(page_bitmap, 612, 792, md5_2);
  FPDFBitmap_Destroy(page_bitmap);

  // And some randomly transformed text
  FPDF_PAGEOBJECT text3 = FPDFPageObj_NewTextObj(doc, "Courier-Bold", 20.0f);
  EXPECT_TRUE(text3);
  EXPECT_TRUE(FPDFText_SetText(text3, "Can you read me? <:)>"));
  FPDFPageObj_Transform(text3, 1, 1.5, 2, 0.5, 200, 200);
  FPDFPage_InsertObject(page, text3);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  page_bitmap = RenderPage(page);
#if _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
  const char md5_3[] = "c6e5df448428793c7e4b0c820bd8c85e";
#else
  const char md5_3[] = "903ee10b6a9f0be51ecad0a1a0eeb171";
#endif
  CompareBitmap(page_bitmap, 612, 792, md5_3);
  FPDFBitmap_Destroy(page_bitmap);

  // TODO(npm): Why are there issues with text rotated by 90 degrees?
  // TODO(npm): FPDF_SaveAsCopy not giving the desired result after this.
  FPDF_ClosePage(page);
  FPDF_CloseDocument(doc);
}

TEST_F(FPDFEditEmbeddertest, DoubleGenerating) {
  // Start with a blank page
  FPDF_DOCUMENT doc = FPDF_CreateNewDocument();
  FPDF_PAGE page = FPDFPage_New(doc, 0, 612, 792);

  // Add a red rectangle with some non-default alpha
  FPDF_PAGEOBJECT rect = FPDFPageObj_CreateNewRect(10, 10, 100, 100);
  EXPECT_TRUE(FPDFPath_SetFillColor(rect, 255, 0, 0, 128));
  EXPECT_TRUE(FPDFPath_SetDrawMode(rect, FPDF_FILLMODE_WINDING, 0));
  FPDFPage_InsertObject(page, rect);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));

  // Check the ExtGState
  CPDF_Page* the_page = CPDFPageFromFPDFPage(page);
  CPDF_Dictionary* graphics_dict =
      the_page->m_pResources->GetDictFor("ExtGState");
  ASSERT_TRUE(graphics_dict);
  EXPECT_EQ(1, static_cast<int>(graphics_dict->GetCount()));

  // Check the bitmap
  FPDF_BITMAP page_bitmap = RenderPage(page);
  CompareBitmap(page_bitmap, 612, 792, "5384da3406d62360ffb5cac4476fff1c");
  FPDFBitmap_Destroy(page_bitmap);

  // Never mind, my new favorite color is blue, increase alpha
  EXPECT_TRUE(FPDFPath_SetFillColor(rect, 0, 0, 255, 180));
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_EQ(2, static_cast<int>(graphics_dict->GetCount()));

  // Check that bitmap displays changed content
  page_bitmap = RenderPage(page);
  CompareBitmap(page_bitmap, 612, 792, "2e51656f5073b0bee611d9cd086aa09c");
  FPDFBitmap_Destroy(page_bitmap);

  // And now generate, without changes
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_EQ(2, static_cast<int>(graphics_dict->GetCount()));
  page_bitmap = RenderPage(page);
  CompareBitmap(page_bitmap, 612, 792, "2e51656f5073b0bee611d9cd086aa09c");
  FPDFBitmap_Destroy(page_bitmap);

  // Add some text to the page
  FPDF_PAGEOBJECT text = FPDFPageObj_NewTextObj(doc, "Arial", 12.0f);
  EXPECT_TRUE(FPDFText_SetText(text, "Something something #text# something"));
  FPDFPageObj_Transform(text, 1, 0, 0, 1, 300, 300);
  FPDFPage_InsertObject(page, text);
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  CPDF_Dictionary* font_dict = the_page->m_pResources->GetDictFor("Font");
  ASSERT_TRUE(font_dict);
  EXPECT_EQ(1, static_cast<int>(font_dict->GetCount()));

  // Generate yet again, check dicts are reasonably sized
  EXPECT_TRUE(FPDFPage_GenerateContent(page));
  EXPECT_EQ(2, static_cast<int>(graphics_dict->GetCount()));
  EXPECT_EQ(1, static_cast<int>(font_dict->GetCount()));
  FPDF_ClosePage(page);
  FPDF_CloseDocument(doc);
}

TEST_F(FPDFEditEmbeddertest, Type1Font) {
  // Create a new document
  FPDF_DOCUMENT doc = FPDF_CreateNewDocument();
  CPDF_Document* document = reinterpret_cast<CPDF_Document*>(doc);

  // Get Times New Roman Bold as a Type 1 font
  CPDF_Font* times_bold = CPDF_Font::GetStockFont(document, "Times-Bold");
  uint8_t* data = times_bold->m_Font.GetFontData();
  uint32_t size = times_bold->m_Font.GetSize();
  FPDF_FONT font = FPDFText_LoadType1Font(doc, data, size);
  ASSERT_TRUE(font);
  CPDF_Font* type1_font = reinterpret_cast<CPDF_Font*>(font);
  EXPECT_TRUE(type1_font->IsType1Font());

  // Check that the font dictionary has the required keys according to the spec
  CPDF_Dictionary* font_dict = type1_font->GetFontDict();
  EXPECT_EQ("Font", font_dict->GetStringFor("Type"));
  EXPECT_EQ("Type1", font_dict->GetStringFor("Subtype"));
  EXPECT_EQ("Times New Roman Bold", font_dict->GetStringFor("BaseFont"));
  ASSERT_TRUE(font_dict->KeyExist("FirstChar"));
  ASSERT_TRUE(font_dict->KeyExist("LastChar"));
  EXPECT_EQ(32, font_dict->GetIntegerFor("FirstChar"));
  EXPECT_EQ(65532, font_dict->GetIntegerFor("LastChar"));
  ASSERT_TRUE(font_dict->KeyExist("Widths"));
  CPDF_Array* widths_array = font_dict->GetArrayFor("Widths");
  EXPECT_EQ(65501U, widths_array->GetCount());
  EXPECT_EQ(250, widths_array->GetNumberAt(0));
  EXPECT_EQ(0, widths_array->GetNumberAt(8172));
  EXPECT_EQ(1000, widths_array->GetNumberAt(65500));
  ASSERT_TRUE(font_dict->KeyExist("FontDescriptor"));
  CPDF_Dictionary* font_desc = font_dict->GetDictFor("FontDescriptor");
  EXPECT_EQ("FontDescriptor", font_desc->GetStringFor("Type"));
  EXPECT_EQ(font_dict->GetStringFor("BaseFont"),
            font_desc->GetStringFor("FontName"));

  // Check that the font descriptor has the required keys according to the spec
  ASSERT_TRUE(font_desc->KeyExist("Flags"));
  int font_flags = font_desc->GetIntegerFor("Flags");
  EXPECT_TRUE(font_flags & FXFONT_BOLD);
  EXPECT_TRUE(font_flags & FXFONT_NONSYMBOLIC);
  ASSERT_TRUE(font_desc->KeyExist("FontBBox"));
  EXPECT_EQ(4U, font_desc->GetArrayFor("FontBBox")->GetCount());
  EXPECT_TRUE(font_desc->KeyExist("ItalicAngle"));
  EXPECT_TRUE(font_desc->KeyExist("Ascent"));
  EXPECT_TRUE(font_desc->KeyExist("Descent"));
  EXPECT_TRUE(font_desc->KeyExist("CapHeight"));
  EXPECT_TRUE(font_desc->KeyExist("StemV"));
  ASSERT_TRUE(font_desc->KeyExist("FontFile"));

  // Check that the font stream is the one that was provided
  CPDF_Stream* font_stream = font_desc->GetStreamFor("FontFile");
  ASSERT_EQ(size, font_stream->GetRawSize());
  uint8_t* stream_data = font_stream->GetRawData();
  for (size_t i = 0; i < size; i++)
    EXPECT_EQ(data[i], stream_data[i]);

  // Close document
  FPDF_CloseDocument(doc);
}
