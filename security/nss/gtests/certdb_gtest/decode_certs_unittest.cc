/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "cert.h"
#include "prerror.h"
#include "secerr.h"

class DecodeCertsTest : public ::testing::Test {};

TEST_F(DecodeCertsTest, EmptyCertPackage) {
  // This represents a PKCS#7 ContentInfo with a contentType of
  // '2.16.840.1.113730.2.5' (Netscape data-type cert-sequence) and a content
  // consisting of an empty SEQUENCE. This is valid ASN.1, but it contains no
  // certificates, so CERT_DecodeCertFromPackage should just return a null
  // pointer.
  unsigned char emptyCertPackage[] = {0x30, 0x0f, 0x06, 0x09, 0x60, 0x86,
                                      0x48, 0x01, 0x86, 0xf8, 0x42, 0x02,
                                      0x05, 0xa0, 0x02, 0x30, 0x00};
  EXPECT_EQ(nullptr, CERT_DecodeCertFromPackage(
                         reinterpret_cast<char*>(emptyCertPackage),
                         sizeof(emptyCertPackage)));
  EXPECT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(DecodeCertsTest, EmptySignedData) {
  // This represents a PKCS#7 ContentInfo of contentType
  // 1.2.840.113549.1.7.2 (signedData) with missing content.
  unsigned char emptySignedData[] = {0x30, 0x80, 0x06, 0x09, 0x2a, 0x86,
                                     0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07,
                                     0x02, 0x00, 0x00, 0x05, 0x00};

  EXPECT_EQ(nullptr,
            CERT_DecodeCertFromPackage(reinterpret_cast<char*>(emptySignedData),
                                       sizeof(emptySignedData)));
  EXPECT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}
