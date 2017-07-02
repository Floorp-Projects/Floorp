// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/embedder_test.h"
#include "testing/gtest/include/gtest/gtest.h"

class CPDFSecurityHandlerEmbeddertest : public EmbedderTest {};

TEST_F(CPDFSecurityHandlerEmbeddertest, Unencrypted) {
  ASSERT_TRUE(OpenDocument("about_blank.pdf"));
  EXPECT_EQ(0xFFFFFFFF, FPDF_GetDocPermissions(document()));
}

TEST_F(CPDFSecurityHandlerEmbeddertest, UnencryptedWithPassword) {
  ASSERT_TRUE(OpenDocument("about_blank.pdf", "foobar"));
  EXPECT_EQ(0xFFFFFFFF, FPDF_GetDocPermissions(document()));
}

TEST_F(CPDFSecurityHandlerEmbeddertest, NoPassword) {
  EXPECT_FALSE(OpenDocument("encrypted.pdf"));
}

TEST_F(CPDFSecurityHandlerEmbeddertest, BadPassword) {
  EXPECT_FALSE(OpenDocument("encrypted.pdf", "tiger"));
}

TEST_F(CPDFSecurityHandlerEmbeddertest, UserPassword) {
  ASSERT_TRUE(OpenDocument("encrypted.pdf", "1234"));
  EXPECT_EQ(0xFFFFF2C0, FPDF_GetDocPermissions(document()));
}

TEST_F(CPDFSecurityHandlerEmbeddertest, OwnerPassword) {
  ASSERT_TRUE(OpenDocument("encrypted.pdf", "5678"));
  EXPECT_EQ(0xFFFFFFFC, FPDF_GetDocPermissions(document()));
}

TEST_F(CPDFSecurityHandlerEmbeddertest, NoPasswordVersion5) {
  ASSERT_FALSE(OpenDocument("bug_644.pdf"));
}

TEST_F(CPDFSecurityHandlerEmbeddertest, BadPasswordVersion5) {
  ASSERT_FALSE(OpenDocument("bug_644.pdf", "tiger"));
}

TEST_F(CPDFSecurityHandlerEmbeddertest, OwnerPasswordVersion5) {
  ASSERT_TRUE(OpenDocument("bug_644.pdf", "a"));
  EXPECT_EQ(0xFFFFFFFC, FPDF_GetDocPermissions(document()));
}

TEST_F(CPDFSecurityHandlerEmbeddertest, UserPasswordVersion5) {
  ASSERT_TRUE(OpenDocument("bug_644.pdf", "b"));
  EXPECT_EQ(0xFFFFFFFC, FPDF_GetDocPermissions(document()));
}
