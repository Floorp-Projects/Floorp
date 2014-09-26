/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2013 Mozilla Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "gtest/gtest.h"
#include "pkix/pkix.h"
#include "pkixder.h"
#include "pkixtestutil.h"

using namespace mozilla::pkix;
using namespace mozilla::pkix::test;

class CreateEncodedOCSPRequestTrustDomain : public TrustDomain
{
private:
  virtual Result GetCertTrust(EndEntityOrCA, const CertPolicyId&,
                              Input, /*out*/ TrustLevel&)
  {
    ADD_FAILURE();
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  virtual Result FindIssuer(Input, IssuerChecker&, Time)
  {
    ADD_FAILURE();
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  virtual Result CheckRevocation(EndEntityOrCA, const CertID&, Time,
                                 const Input*, const Input*)
  {
    ADD_FAILURE();
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  virtual Result IsChainValid(const DERArray&, Time)
  {
    ADD_FAILURE();
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  virtual Result VerifySignedData(const SignedDataWithSignature&,
                                  Input)
  {
    ADD_FAILURE();
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  virtual Result DigestBuf(Input item, /*out*/ uint8_t *digestBuf,
                           size_t digestBufLen)
  {
    return TestDigestBuf(item, digestBuf, digestBufLen);
  }

  virtual Result CheckPublicKey(Input subjectPublicKeyInfo)
  {
    ADD_FAILURE();
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
};

class pkixocsp_CreateEncodedOCSPRequest : public ::testing::Test
{
protected:
  void MakeIssuerCertIDComponents(const char* issuerASCII,
                                  /*out*/ ByteString& issuerDER,
                                  /*out*/ ByteString& issuerSPKI)
  {
    issuerDER = CNToDERName(issuerASCII);
    ASSERT_NE(ENCODING_FAILED, issuerDER);

    ScopedTestKeyPair keyPair(GenerateKeyPair());
    ASSERT_TRUE(keyPair);
    issuerSPKI = keyPair->subjectPublicKeyInfo;
  }

  CreateEncodedOCSPRequestTrustDomain trustDomain;
};

// Test that the large length of the child serial number causes
// CreateEncodedOCSPRequest to fail.
TEST_F(pkixocsp_CreateEncodedOCSPRequest, ChildCertLongSerialNumberTest)
{
  static const uint8_t UNSUPPORTED_LEN = 128; // must be larger than 127

  ByteString serialNumberString;
  // tag + length + value is 1 + 2 + UNSUPPORTED_LEN
  // Encoding the length takes two bytes: one byte to indicate that a
  // second byte follows, and the second byte to indicate the length.
  serialNumberString.push_back(0x80 + 1);
  serialNumberString.push_back(UNSUPPORTED_LEN);
  // value is 0x010000...00
  serialNumberString.push_back(0x01);
  for (size_t i = 1; i < UNSUPPORTED_LEN; ++i) {
    serialNumberString.push_back(0x00);
  }

  ByteString issuerDER;
  ByteString issuerSPKI;
  ASSERT_NO_FATAL_FAILURE(MakeIssuerCertIDComponents("CA", issuerDER,
                                                     issuerSPKI));

  Input issuer;
  ASSERT_EQ(Success, issuer.Init(issuerDER.data(), issuerDER.length()));

  Input spki;
  ASSERT_EQ(Success, spki.Init(issuerSPKI.data(), issuerSPKI.length()));

  Input serialNumber;
  ASSERT_EQ(Success, serialNumber.Init(serialNumberString.data(),
                                       serialNumberString.length()));

  uint8_t ocspRequest[OCSP_REQUEST_MAX_LENGTH];
  size_t ocspRequestLength;
  ASSERT_EQ(Result::ERROR_BAD_DER,
            CreateEncodedOCSPRequest(trustDomain,
                                     CertID(issuer, spki, serialNumber),
                                     ocspRequest, ocspRequestLength));
}

// Test that CreateEncodedOCSPRequest handles the longest serial number that
// it's required to support (i.e. 20 octets).
TEST_F(pkixocsp_CreateEncodedOCSPRequest, LongestSupportedSerialNumberTest)
{
  static const uint8_t LONGEST_REQUIRED_LEN = 20;

  ByteString serialNumberString;
  // tag + length + value is 1 + 1 + LONGEST_REQUIRED_LEN
  serialNumberString.push_back(der::INTEGER);
  serialNumberString.push_back(LONGEST_REQUIRED_LEN);
  serialNumberString.push_back(0x01);
  // value is 0x010000...00
  for (size_t i = 1; i < LONGEST_REQUIRED_LEN; ++i) {
    serialNumberString.push_back(0x00);
  }

  ByteString issuerDER;
  ByteString issuerSPKI;
  ASSERT_NO_FATAL_FAILURE(MakeIssuerCertIDComponents("CA", issuerDER,
                                                     issuerSPKI));

  Input issuer;
  ASSERT_EQ(Success, issuer.Init(issuerDER.data(), issuerDER.length()));

  Input spki;
  ASSERT_EQ(Success, spki.Init(issuerSPKI.data(), issuerSPKI.length()));

  Input serialNumber;
  ASSERT_EQ(Success, serialNumber.Init(serialNumberString.data(),
                                       serialNumberString.length()));

  uint8_t ocspRequest[OCSP_REQUEST_MAX_LENGTH];
  size_t ocspRequestLength;
  ASSERT_EQ(Success,
            CreateEncodedOCSPRequest(trustDomain,
                                     CertID(issuer, spki, serialNumber),
                                     ocspRequest, ocspRequestLength));
}
