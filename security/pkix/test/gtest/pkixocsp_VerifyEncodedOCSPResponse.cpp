/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2014 Mozilla Contributors
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

#include "nss.h"
#include "nssgtest.h"
#include "pkix/pkix.h"
#include "pkixtestutil.h"
#include "prinit.h"
#include "secerr.h"

using namespace mozilla::pkix;
using namespace mozilla::pkix::test;

const uint16_t END_ENTITY_MAX_LIFETIME_IN_DAYS = 10;

class OCSPTestTrustDomain : public TrustDomain
{
public:
  OCSPTestTrustDomain()
  {
  }

  virtual SECStatus GetCertTrust(EndEntityOrCA endEntityOrCA,
                                 const CertPolicyId&,
                                 const SECItem& candidateCert,
                         /*out*/ TrustLevel* trustLevel)
  {
    EXPECT_EQ(endEntityOrCA, EndEntityOrCA::MustBeEndEntity);
    EXPECT_TRUE(trustLevel);
    *trustLevel = TrustLevel::InheritsTrust;
    return SECSuccess;
  }

  virtual SECStatus FindIssuer(const SECItem&, IssuerChecker&, PRTime)
  {
    ADD_FAILURE();
    PR_SetError(SEC_ERROR_LIBRARY_FAILURE, 0);
    return SECFailure;
  }

  virtual SECStatus CheckRevocation(EndEntityOrCA endEntityOrCA, const CertID&,
                                    PRTime time,
                                    /*optional*/ const SECItem*,
                                    /*optional*/ const SECItem*)
  {
    // TODO: I guess mozilla::pkix should support revocation of designated
    // OCSP responder eventually, but we don't now, so this function should
    // never get called.
    ADD_FAILURE();
    PR_SetError(SEC_ERROR_LIBRARY_FAILURE, 0);
    return SECFailure;
  }

  virtual SECStatus IsChainValid(const DERArray&)
  {
    ADD_FAILURE();
    PR_SetError(SEC_ERROR_LIBRARY_FAILURE, 0);
    return SECFailure;
  }

  virtual SECStatus VerifySignedData(const SignedDataWithSignature& signedData,
                                     const SECItem& subjectPublicKeyInfo)
  {
    return ::mozilla::pkix::VerifySignedData(signedData, subjectPublicKeyInfo,
                                             nullptr);
  }

  virtual SECStatus DigestBuf(const SECItem& item, /*out*/ uint8_t* digestBuf,
                              size_t digestBufLen)
  {
    return ::mozilla::pkix::DigestBuf(item, digestBuf, digestBufLen);
  }

private:
  OCSPTestTrustDomain(const OCSPTestTrustDomain&) /*delete*/;
  void operator=(const OCSPTestTrustDomain&) /*delete*/;
};

namespace {
char const* const rootName = "CN=Test CA 1";
void deleteCertID(CertID* certID) { delete certID; }
} // unnamed namespace

class pkixocsp_VerifyEncodedResponse : public NSSTest
{
public:
  static bool SetUpTestCaseInner()
  {
    ScopedSECKEYPublicKey rootPublicKey;
    if (GenerateKeyPair(rootPublicKey, rootPrivateKey) != SECSuccess) {
      return false;
    }
    rootSPKI = SECKEY_EncodeDERSubjectPublicKeyInfo(rootPublicKey.get());
    if (!rootSPKI) {
      return false;
    }

    return true;
  }

  static void SetUpTestCase()
  {
    NSSTest::SetUpTestCase();
    if (!SetUpTestCaseInner()) {
      PR_Abort();
    }
  }

  void SetUp()
  {
    NSSTest::SetUp();

    const SECItem* rootNameDER = ASCIIToDERName(arena.get(), rootName);
    if (!rootNameDER) {
      PR_Abort();
    }
    const SECItem*
      endEntitySerialNumber(CreateEncodedSerialNumber(arena.get(),
                                                      ++rootIssuedCount));
    if (!endEntitySerialNumber) {
      PR_Abort();
    }
    endEntityCertID = new (std::nothrow) CertID(*rootNameDER, *rootSPKI,
                                                *endEntitySerialNumber);
    if (!endEntityCertID) {
      PR_Abort();
    }
  }

  static ScopedSECKEYPrivateKey rootPrivateKey;
  static ScopedSECItem rootSPKI;
  static long rootIssuedCount;

  OCSPTestTrustDomain trustDomain;
  // endEntityCertID references items owned by arena and rootSPKI.
  ScopedPtr<CertID, deleteCertID> endEntityCertID;
};

/*static*/ ScopedSECKEYPrivateKey
              pkixocsp_VerifyEncodedResponse::rootPrivateKey;
/*static*/ ScopedSECItem pkixocsp_VerifyEncodedResponse::rootSPKI;
/*static*/ long pkixocsp_VerifyEncodedResponse::rootIssuedCount = 0;

///////////////////////////////////////////////////////////////////////////////
// responseStatus

struct WithoutResponseBytes
{
  uint8_t responseStatus;
  PRErrorCode expectedError;
};

static const WithoutResponseBytes WITHOUT_RESPONSEBYTES[] = {
  { OCSPResponseContext::successful, SEC_ERROR_OCSP_MALFORMED_RESPONSE },
  { OCSPResponseContext::malformedRequest, SEC_ERROR_OCSP_MALFORMED_REQUEST },
  { OCSPResponseContext::internalError, SEC_ERROR_OCSP_SERVER_ERROR },
  { OCSPResponseContext::tryLater, SEC_ERROR_OCSP_TRY_SERVER_LATER },
  { 4/*unused*/, SEC_ERROR_OCSP_UNKNOWN_RESPONSE_STATUS },
  { OCSPResponseContext::sigRequired, SEC_ERROR_OCSP_REQUEST_NEEDS_SIG },
  { OCSPResponseContext::unauthorized, SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST },
  { OCSPResponseContext::unauthorized + 1,
    SEC_ERROR_OCSP_UNKNOWN_RESPONSE_STATUS
  },
};

class pkixocsp_VerifyEncodedResponse_WithoutResponseBytes
  : public pkixocsp_VerifyEncodedResponse
  , public ::testing::WithParamInterface<WithoutResponseBytes>
{
protected:
  SECItem* CreateEncodedOCSPErrorResponse(uint8_t status)
  {
    static const SECItem EMPTY = { siBuffer, nullptr, 0 };
    OCSPResponseContext context(arena.get(),
                                CertID(EMPTY, EMPTY, EMPTY),
                                oneDayBeforeNow);
    context.responseStatus = status;
    context.skipResponseBytes = true;
    return CreateEncodedOCSPResponse(context);
  }
};

TEST_P(pkixocsp_VerifyEncodedResponse_WithoutResponseBytes, CorrectErrorCode)
{
  SECItem* response(CreateEncodedOCSPErrorResponse(
                      GetParam().responseStatus));
  ASSERT_TRUE(response);
  bool expired;
  ASSERT_SECFailure(GetParam().expectedError,
                    VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID, now,
                                              END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                              *response, expired));
}

INSTANTIATE_TEST_CASE_P(pkixocsp_VerifyEncodedResponse_WithoutResponseBytes,
                        pkixocsp_VerifyEncodedResponse_WithoutResponseBytes,
                        testing::ValuesIn(WITHOUT_RESPONSEBYTES));
