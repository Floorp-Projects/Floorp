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

#include "nssgtest.h"
#include "pkix/pkix.h"
#include "pkix/pkixnss.h"
#include "pkixder.h"
#include "pkixtestutil.h"
#include "secerr.h"

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

  virtual Result IsChainValid(const DERArray&)
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
    return ::mozilla::pkix::DigestBuf(item, digestBuf, digestBufLen);
  }

  virtual Result CheckPublicKey(Input subjectPublicKeyInfo)
  {
    return ::mozilla::pkix::CheckPublicKey(subjectPublicKeyInfo);
  }
};

class pkixocsp_CreateEncodedOCSPRequest : public NSSTest
{
protected:
  // These SECItems are allocated in arena, and so will be auto-cleaned.
  SECItem* unsupportedLongSerialNumber;
  SECItem* longestRequiredSerialNumber;

  void SetUp()
  {
    static const uint8_t UNSUPPORTED_LEN = 128; // must be larger than 127
    // tag + length + value is 1 + 2 + UNSUPPORTED_LEN
    unsupportedLongSerialNumber = SECITEM_AllocItem(arena.get(), nullptr,
                                                    1 + 2 + UNSUPPORTED_LEN);
    memset(unsupportedLongSerialNumber->data, 0,
           unsupportedLongSerialNumber->len);
    unsupportedLongSerialNumber->data[0] = der::INTEGER;
    // Encoding the length takes two bytes: one byte to indicate that a
    // second byte follows, and the second byte to indicate the length.
    unsupportedLongSerialNumber->data[1] = 0x80 + 1;
    unsupportedLongSerialNumber->data[2] = UNSUPPORTED_LEN;
    unsupportedLongSerialNumber->data[3] = 0x01; // value is 0x010000...00

    static const uint8_t LONGEST_REQUIRED_LEN = 20;
    // tag + length + value is 1 + 1 + LONGEST_REQUIRED_LEN
    longestRequiredSerialNumber = SECITEM_AllocItem(arena.get(), nullptr,
                                    1 + 1 + LONGEST_REQUIRED_LEN);
    memset(longestRequiredSerialNumber->data, 0,
           longestRequiredSerialNumber->len);
    longestRequiredSerialNumber->data[0] = der::INTEGER;
    longestRequiredSerialNumber->data[1] = LONGEST_REQUIRED_LEN;
    longestRequiredSerialNumber->data[2] = 0x01; // value is 0x010000...00
  }

  // The resultant issuerDER and issuerSPKI are owned by the arena.
  void MakeIssuerCertIDComponents(const char* issuerASCII,
                                  /*out*/ Input& issuerDER,
                                  /*out*/ Input& issuerSPKI)
  {
    const SECItem* issuerDERSECItem = ASCIIToDERName(arena.get(), issuerASCII);
    ASSERT_TRUE(issuerDERSECItem);
    ASSERT_EQ(Success,
              issuerDER.Init(issuerDERSECItem->data, issuerDERSECItem->len));

    ScopedSECKEYPublicKey issuerPublicKey;
    ScopedSECKEYPrivateKey issuerPrivateKey;
    ASSERT_EQ(Success, GenerateKeyPair(issuerPublicKey, issuerPrivateKey));

    ScopedSECItem issuerSPKIOriginal(
      SECKEY_EncodeDERSubjectPublicKeyInfo(issuerPublicKey.get()));
    ASSERT_TRUE(issuerSPKIOriginal);

    SECItem issuerSPKICopy;
    ASSERT_EQ(SECSuccess,
              SECITEM_CopyItem(arena.get(), &issuerSPKICopy,
                               issuerSPKIOriginal.get()));
    ASSERT_EQ(Success,
              issuerSPKI.Init(issuerSPKICopy.data, issuerSPKICopy.len));
  }

  CreateEncodedOCSPRequestTrustDomain trustDomain;
};

// Test that the large length of the child serial number causes
// CreateEncodedOCSPRequest to fail.
TEST_F(pkixocsp_CreateEncodedOCSPRequest, ChildCertLongSerialNumberTest)
{
  Input issuerDER;
  Input issuerSPKI;
  MakeIssuerCertIDComponents("CN=CA", issuerDER, issuerSPKI);
  Input serialNumber;
  ASSERT_EQ(Success, serialNumber.Init(unsupportedLongSerialNumber->data,
                                       unsupportedLongSerialNumber->len));
  uint8_t ocspRequest[OCSP_REQUEST_MAX_LENGTH];
  size_t ocspRequestLength;
  ASSERT_EQ(Result::ERROR_BAD_DER,
            CreateEncodedOCSPRequest(trustDomain,
                                     CertID(issuerDER, issuerSPKI,
                                            serialNumber),
                                     ocspRequest, ocspRequestLength));
}

// Test that CreateEncodedOCSPRequest handles the longest serial number that
// it's required to support (i.e. 20 octets).
TEST_F(pkixocsp_CreateEncodedOCSPRequest, LongestSupportedSerialNumberTest)
{
  Input issuerDER;
  Input issuerSPKI;
  MakeIssuerCertIDComponents("CN=CA", issuerDER, issuerSPKI);
  Input serialNumber;
  ASSERT_EQ(Success, serialNumber.Init(longestRequiredSerialNumber->data,
                                       longestRequiredSerialNumber->len));
  uint8_t ocspRequest[OCSP_REQUEST_MAX_LENGTH];
  size_t ocspRequestLength;
  ASSERT_EQ(Success,
            CreateEncodedOCSPRequest(trustDomain,
                                     CertID(issuerDER, issuerSPKI,
                                            serialNumber),
                                     ocspRequest, ocspRequestLength));
}
