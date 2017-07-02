// Copyright 2015 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdftext/cpdf_linkextract.h"

#include "testing/gtest/include/gtest/gtest.h"

// Class to help test functions in CPDF_LinkExtract class.
class CPDF_TestLinkExtract : public CPDF_LinkExtract {
 public:
  CPDF_TestLinkExtract() : CPDF_LinkExtract(nullptr) {}

 private:
  // Add test cases as friends to access protected member functions.
  // Access CheckMailLink.
  FRIEND_TEST(fpdf_text_int, CheckMailLink);
};

TEST(fpdf_text_int, CheckMailLink) {
  CPDF_TestLinkExtract extractor;
  // Check cases that fail to extract valid mail link.
  const wchar_t* invalid_strs[] = {
      L"",
      L"peter.pan",       // '@' is required.
      L"abc@server",      // Domain name needs at least one '.'.
      L"abc.@gmail.com",  // '.' can not immediately precede '@'.
      L"abc@xyz&q.org",   // Domain name should not contain '&'.
      L"abc@.xyz.org",    // Domain name should not start with '.'.
      L"fan@g..com"       // Domain name should not have consecutive '.'
  };
  for (size_t i = 0; i < FX_ArraySize(invalid_strs); ++i) {
    CFX_WideString text_str(invalid_strs[i]);
    EXPECT_FALSE(extractor.CheckMailLink(text_str));
  }

  // Check cases that can extract valid mail link.
  // An array of {input_string, expected_extracted_email_address}.
  const wchar_t* valid_strs[][2] = {
      {L"peter@abc.d", L"peter@abc.d"},
      {L"red.teddy.b@abc.com", L"red.teddy.b@abc.com"},
      {L"abc_@gmail.com", L"abc_@gmail.com"},  // '_' is ok before '@'.
      {L"dummy-hi@gmail.com",
       L"dummy-hi@gmail.com"},                  // '-' is ok in user name.
      {L"a..df@gmail.com", L"df@gmail.com"},    // Stop at consecutive '.'.
      {L".john@yahoo.com", L"john@yahoo.com"},  // Remove heading '.'.
      {L"abc@xyz.org?/", L"abc@xyz.org"},       // Trim ending invalid chars.
      {L"fan{abc@xyz.org", L"abc@xyz.org"},     // Trim beginning invalid chars.
      {L"fan@g.com..", L"fan@g.com"},           // Trim the ending periods.
      {L"CAP.cap@Gmail.Com", L"CAP.cap@Gmail.Com"},  // Keep the original case.
  };
  for (size_t i = 0; i < FX_ArraySize(valid_strs); ++i) {
    CFX_WideString text_str(valid_strs[i][0]);
    CFX_WideString expected_str(L"mailto:");
    expected_str += valid_strs[i][1];
    EXPECT_TRUE(extractor.CheckMailLink(text_str));
    EXPECT_STREQ(text_str.c_str(), expected_str.c_str());
  }
}
