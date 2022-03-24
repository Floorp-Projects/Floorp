/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "nss.h"
#include "base.h"
#include "secerr.h"

namespace nss_test {

class Utf8Test : public ::testing::Test {};

// Tests nssUTF8_Length rejects overlong forms, surrogates, etc.
TEST_F(Utf8Test, Utf8Length) {
  PRStatus status;

  EXPECT_EQ(0u, nssUTF8_Length("", &status));
  EXPECT_EQ(PR_SUCCESS, status);

  // U+0000..U+007F
  EXPECT_EQ(1u, nssUTF8_Length("\x01", &status));
  EXPECT_EQ(PR_SUCCESS, status);
  EXPECT_EQ(1u, nssUTF8_Length("\x7F", &status));
  EXPECT_EQ(PR_SUCCESS, status);

  // lone trailing byte
  EXPECT_EQ(0u, nssUTF8_Length("\x80", &status));
  EXPECT_EQ(PR_FAILURE, status);
  EXPECT_EQ(NSS_ERROR_INVALID_STRING, NSS_GetError());
  EXPECT_EQ(0u, nssUTF8_Length("\xBF", &status));
  EXPECT_EQ(PR_FAILURE, status);
  EXPECT_EQ(NSS_ERROR_INVALID_STRING, NSS_GetError());

  // overlong U+0000..U+007F
  EXPECT_EQ(0u, nssUTF8_Length("\xC0\x80", &status));
  EXPECT_EQ(PR_FAILURE, status);
  EXPECT_EQ(NSS_ERROR_INVALID_STRING, NSS_GetError());
  EXPECT_EQ(0u, nssUTF8_Length("\xC1\xBF", &status));
  EXPECT_EQ(PR_FAILURE, status);
  EXPECT_EQ(NSS_ERROR_INVALID_STRING, NSS_GetError());

  // U+0080..U+07FF
  EXPECT_EQ(2u, nssUTF8_Length("\xC2\x80", &status));
  EXPECT_EQ(PR_SUCCESS, status);
  EXPECT_EQ(2u, nssUTF8_Length("\xDF\xBF", &status));
  EXPECT_EQ(PR_SUCCESS, status);

  // overlong U+0000..U+07FF
  EXPECT_EQ(0u, nssUTF8_Length("\xE0\x80\x80", &status));
  EXPECT_EQ(PR_FAILURE, status);
  EXPECT_EQ(NSS_ERROR_INVALID_STRING, NSS_GetError());
  EXPECT_EQ(0u, nssUTF8_Length("\xE0\x9F\xBF", &status));
  EXPECT_EQ(PR_FAILURE, status);
  EXPECT_EQ(NSS_ERROR_INVALID_STRING, NSS_GetError());

  // U+0800..U+D7FF
  EXPECT_EQ(3u, nssUTF8_Length("\xE0\xA0\x80", &status));
  EXPECT_EQ(PR_SUCCESS, status);
  EXPECT_EQ(3u, nssUTF8_Length("\xE0\xBF\xBF", &status));
  EXPECT_EQ(PR_SUCCESS, status);
  EXPECT_EQ(3u, nssUTF8_Length("\xE1\x80\x80", &status));
  EXPECT_EQ(PR_SUCCESS, status);
  EXPECT_EQ(3u, nssUTF8_Length("\xEC\xBF\xBF", &status));
  EXPECT_EQ(PR_SUCCESS, status);
  EXPECT_EQ(3u, nssUTF8_Length("\xED\x80\x80", &status));
  EXPECT_EQ(PR_SUCCESS, status);
  EXPECT_EQ(3u, nssUTF8_Length("\xED\x9F\xBF", &status));
  EXPECT_EQ(PR_SUCCESS, status);

  // lone surrogate
  EXPECT_EQ(0u, nssUTF8_Length("\xED\xA0\x80", &status));
  EXPECT_EQ(PR_FAILURE, status);
  EXPECT_EQ(NSS_ERROR_INVALID_STRING, NSS_GetError());
  EXPECT_EQ(0u, nssUTF8_Length("\xED\xBF\xBF", &status));
  EXPECT_EQ(PR_FAILURE, status);
  EXPECT_EQ(NSS_ERROR_INVALID_STRING, NSS_GetError());

  // U+E000..U+FFFF
  EXPECT_EQ(3u, nssUTF8_Length("\xEE\x80\x80", &status));
  EXPECT_EQ(PR_SUCCESS, status);
  EXPECT_EQ(3u, nssUTF8_Length("\xEF\xBF\xBF", &status));
  EXPECT_EQ(PR_SUCCESS, status);

  // overlong U+0000..U+FFFF
  EXPECT_EQ(0u, nssUTF8_Length("\xF0\x80\x80\x80", &status));
  EXPECT_EQ(PR_FAILURE, status);
  EXPECT_EQ(NSS_ERROR_INVALID_STRING, NSS_GetError());
  EXPECT_EQ(0u, nssUTF8_Length("\xF0\x8F\xBF\xBF", &status));
  EXPECT_EQ(PR_FAILURE, status);
  EXPECT_EQ(NSS_ERROR_INVALID_STRING, NSS_GetError());

  // U+10000..U+10FFFF
  EXPECT_EQ(4u, nssUTF8_Length("\xF0\x90\x80\x80", &status));
  EXPECT_EQ(PR_SUCCESS, status);
  EXPECT_EQ(4u, nssUTF8_Length("\xF0\xBF\xBF\xBF", &status));
  EXPECT_EQ(PR_SUCCESS, status);
  EXPECT_EQ(4u, nssUTF8_Length("\xF1\x80\x80\x80", &status));
  EXPECT_EQ(PR_SUCCESS, status);
  EXPECT_EQ(4u, nssUTF8_Length("\xF3\xBF\xBF\xBF", &status));
  EXPECT_EQ(PR_SUCCESS, status);
  EXPECT_EQ(4u, nssUTF8_Length("\xF4\x80\x80\x80", &status));
  EXPECT_EQ(PR_SUCCESS, status);
  EXPECT_EQ(4u, nssUTF8_Length("\xF4\x8F\xBF\xBF", &status));
  EXPECT_EQ(PR_SUCCESS, status);

  // out of Unicode range
  EXPECT_EQ(0u, nssUTF8_Length("\xF4\x90\x80\x80", &status));
  EXPECT_EQ(PR_FAILURE, status);
  EXPECT_EQ(NSS_ERROR_INVALID_STRING, NSS_GetError());
  EXPECT_EQ(0u, nssUTF8_Length("\xF4\xBF\xBF\xBF", &status));
  EXPECT_EQ(PR_FAILURE, status);
  EXPECT_EQ(NSS_ERROR_INVALID_STRING, NSS_GetError());
  EXPECT_EQ(0u, nssUTF8_Length("\xF5\x80\x80\x80", &status));
  EXPECT_EQ(PR_FAILURE, status);
  EXPECT_EQ(NSS_ERROR_INVALID_STRING, NSS_GetError());
  EXPECT_EQ(0u, nssUTF8_Length("\xF7\xBF\xBF\xBF", &status));
  EXPECT_EQ(PR_FAILURE, status);
  EXPECT_EQ(NSS_ERROR_INVALID_STRING, NSS_GetError());

  // former 5-byte sequence
  EXPECT_EQ(0u, nssUTF8_Length("\xF8\x80\x80\x80\x80", &status));
  EXPECT_EQ(PR_FAILURE, status);
  EXPECT_EQ(NSS_ERROR_INVALID_STRING, NSS_GetError());
  EXPECT_EQ(0u, nssUTF8_Length("\xFB\xBF\xBF\xBF\xBF", &status));
  EXPECT_EQ(PR_FAILURE, status);
  EXPECT_EQ(NSS_ERROR_INVALID_STRING, NSS_GetError());

  // former 6-byte sequence
  EXPECT_EQ(0u, nssUTF8_Length("\xFC\x80\x80\x80\x80\x80", &status));
  EXPECT_EQ(PR_FAILURE, status);
  EXPECT_EQ(NSS_ERROR_INVALID_STRING, NSS_GetError());
  EXPECT_EQ(0u, nssUTF8_Length("\xFD\xBF\xBF\xBF\xBF\xBF", &status));
  EXPECT_EQ(PR_FAILURE, status);
  EXPECT_EQ(NSS_ERROR_INVALID_STRING, NSS_GetError());

  // invalid lead byte
  EXPECT_EQ(0u, nssUTF8_Length("\xFE", &status));
  EXPECT_EQ(PR_FAILURE, status);
  EXPECT_EQ(NSS_ERROR_INVALID_STRING, NSS_GetError());
  EXPECT_EQ(0u, nssUTF8_Length("\xFF", &status));
  EXPECT_EQ(PR_FAILURE, status);
  EXPECT_EQ(NSS_ERROR_INVALID_STRING, NSS_GetError());

  nss_DestroyErrorStack();
}
}
