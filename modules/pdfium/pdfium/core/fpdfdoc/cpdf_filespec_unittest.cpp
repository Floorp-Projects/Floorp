// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "core/fpdfapi/parser/cpdf_dictionary.h"
#include "core/fpdfapi/parser/cpdf_name.h"
#include "core/fpdfapi/parser/cpdf_string.h"
#include "core/fpdfdoc/cpdf_filespec.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/test_support.h"

TEST(cpdf_filespec, EncodeDecodeFileName) {
  std::vector<pdfium::NullTermWstrFuncTestData> test_data = {
    // Empty src string.
    {L"", L""},
    // only file name.
    {L"test.pdf", L"test.pdf"},
#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
    // With drive identifier.
    {L"r:\\pdfdocs\\spec.pdf", L"/r/pdfdocs/spec.pdf"},
    // Relative path.
    {L"My Document\\test.pdf", L"My Document/test.pdf"},
    // Absolute path without drive identifier.
    {L"\\pdfdocs\\spec.pdf", L"//pdfdocs/spec.pdf"},
    // Absolute path with double backslashes.
    {L"\\\\pdfdocs\\spec.pdf", L"/pdfdocs/spec.pdf"},
// Network resource name. It is not supported yet.
// {L"pclib/eng:\\pdfdocs\\spec.pdf", L"/pclib/eng/pdfdocs/spec.pdf"},
#elif _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
    // Absolute path with colon separator.
    {L"Mac HD:PDFDocs:spec.pdf", L"/Mac HD/PDFDocs/spec.pdf"},
    // Relative path with colon separator.
    {L"PDFDocs:spec.pdf", L"PDFDocs/spec.pdf"},
#else
    // Relative path.
    {L"./docs/test.pdf", L"./docs/test.pdf"},
    // Relative path with parent dir.
    {L"../test_docs/test.pdf", L"../test_docs/test.pdf"},
    // Absolute path.
    {L"/usr/local/home/test.pdf", L"/usr/local/home/test.pdf"},
#endif
  };
  for (const auto& data : test_data) {
    CFX_WideString encoded_str = CPDF_FileSpec::EncodeFileName(data.input);
    EXPECT_TRUE(encoded_str == data.expected);
    // DecodeFileName is the reverse procedure of EncodeFileName.
    CFX_WideString decoded_str = CPDF_FileSpec::DecodeFileName(data.expected);
    EXPECT_TRUE(decoded_str == data.input);
  }
}

TEST(cpdf_filespec, GetFileName) {
  {
    // String object.
    pdfium::NullTermWstrFuncTestData test_data = {
#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
      L"/C/docs/test.pdf",
      L"C:\\docs\\test.pdf"
#elif _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
      L"/Mac HD/docs/test.pdf",
      L"Mac HD:docs:test.pdf"
#else
      L"/docs/test.pdf",
      L"/docs/test.pdf"
#endif
    };
    std::unique_ptr<CPDF_Object> str_obj(
        new CPDF_String(nullptr, test_data.input));
    CPDF_FileSpec file_spec(str_obj.get());
    CFX_WideString file_name;
    EXPECT_TRUE(file_spec.GetFileName(&file_name));
    EXPECT_TRUE(file_name == test_data.expected);
  }
  {
    // Dictionary object.
    pdfium::NullTermWstrFuncTestData test_data[5] = {
#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
      {L"/C/docs/test.pdf", L"C:\\docs\\test.pdf"},
      {L"/D/docs/test.pdf", L"D:\\docs\\test.pdf"},
      {L"/E/docs/test.pdf", L"E:\\docs\\test.pdf"},
      {L"/F/docs/test.pdf", L"F:\\docs\\test.pdf"},
      {L"/G/docs/test.pdf", L"G:\\docs\\test.pdf"},
#elif _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
      {L"/Mac HD/docs1/test.pdf", L"Mac HD:docs1:test.pdf"},
      {L"/Mac HD/docs2/test.pdf", L"Mac HD:docs2:test.pdf"},
      {L"/Mac HD/docs3/test.pdf", L"Mac HD:docs3:test.pdf"},
      {L"/Mac HD/docs4/test.pdf", L"Mac HD:docs4:test.pdf"},
      {L"/Mac HD/docs5/test.pdf", L"Mac HD:docs5:test.pdf"},
#else
      {L"/docs/a/test.pdf", L"/docs/a/test.pdf"},
      {L"/docs/b/test.pdf", L"/docs/b/test.pdf"},
      {L"/docs/c/test.pdf", L"/docs/c/test.pdf"},
      {L"/docs/d/test.pdf", L"/docs/d/test.pdf"},
      {L"/docs/e/test.pdf", L"/docs/e/test.pdf"},
#endif
    };
    // Keyword fields in reverse order of precedence to retrieve the file name.
    const char* const keywords[5] = {"Unix", "Mac", "DOS", "F", "UF"};
    std::unique_ptr<CPDF_Dictionary> dict_obj(new CPDF_Dictionary());
    CPDF_FileSpec file_spec(dict_obj.get());
    CFX_WideString file_name;
    for (int i = 0; i < 5; ++i) {
      dict_obj->SetNewFor<CPDF_String>(keywords[i], test_data[i].input);
      EXPECT_TRUE(file_spec.GetFileName(&file_name));
      EXPECT_TRUE(file_name == test_data[i].expected);
    }

    // With all the former fields and 'FS' field suggests 'URL' type.
    dict_obj->SetNewFor<CPDF_String>("FS", "URL", false);
    EXPECT_TRUE(file_spec.GetFileName(&file_name));
    // Url string is not decoded.
    EXPECT_TRUE(file_name == test_data[4].input);
  }
  {
    // Invalid object.
    std::unique_ptr<CPDF_Object> name_obj(new CPDF_Name(nullptr, "test.pdf"));
    CPDF_FileSpec file_spec(name_obj.get());
    CFX_WideString file_name;
    EXPECT_FALSE(file_spec.GetFileName(&file_name));
  }
}

TEST(cpdf_filespec, SetFileName) {
  pdfium::NullTermWstrFuncTestData test_data = {
#if _FXM_PLATFORM_ == _FXM_PLATFORM_WINDOWS_
    L"C:\\docs\\test.pdf",
    L"/C/docs/test.pdf"
#elif _FXM_PLATFORM_ == _FXM_PLATFORM_APPLE_
    L"Mac HD:docs:test.pdf",
    L"/Mac HD/docs/test.pdf"
#else
    L"/docs/test.pdf",
    L"/docs/test.pdf"
#endif
  };
  // String object.
  std::unique_ptr<CPDF_Object> str_obj(new CPDF_String(nullptr, L"babababa"));
  CPDF_FileSpec file_spec1(str_obj.get());
  file_spec1.SetFileName(test_data.input);
  // Check internal object value.
  CFX_ByteString str = CFX_ByteString::FromUnicode(test_data.expected);
  EXPECT_TRUE(str == str_obj->GetString());
  // Check we can get the file name back.
  CFX_WideString file_name;
  EXPECT_TRUE(file_spec1.GetFileName(&file_name));
  EXPECT_TRUE(file_name == test_data.input);

  // Dictionary object.
  std::unique_ptr<CPDF_Dictionary> dict_obj(new CPDF_Dictionary());
  CPDF_FileSpec file_spec2(dict_obj.get());
  file_spec2.SetFileName(test_data.input);
  // Check internal object value.
  file_name = dict_obj->GetUnicodeTextFor("F");
  EXPECT_TRUE(file_name == test_data.expected);
  file_name = dict_obj->GetUnicodeTextFor("UF");
  EXPECT_TRUE(file_name == test_data.expected);
  // Check we can get the file name back.
  EXPECT_TRUE(file_spec2.GetFileName(&file_name));
  EXPECT_TRUE(file_name == test_data.input);
}
