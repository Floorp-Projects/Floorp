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

#include "pkixgtest.h"

#include "mozpkix/pkixder.h"

#include "secoid.h"

using namespace mozilla::pkix;
using namespace mozilla::pkix::test;

const uint16_t END_ENTITY_MAX_LIFETIME_IN_DAYS = 10;

// Note that CheckRevocation is never called for OCSP signing certificates.
class OCSPTestTrustDomain : public DefaultCryptoTrustDomain
{
public:
  OCSPTestTrustDomain() { }

  Result GetCertTrust(EndEntityOrCA endEntityOrCA, const CertPolicyId&,
                      Input, /*out*/ TrustLevel& trustLevel)
                      /*non-final*/ override
  {
    EXPECT_EQ(endEntityOrCA, EndEntityOrCA::MustBeEndEntity);
    trustLevel = TrustLevel::InheritsTrust;
    return Success;
  }

  virtual void NoteAuxiliaryExtension(AuxiliaryExtension extension,
                                      Input extensionData) override
  {
    if (extension == AuxiliaryExtension::SCTListFromOCSPResponse) {
      signedCertificateTimestamps = InputToByteString(extensionData);
    } else {
      // We do not currently expect to receive any other extension here.
      ADD_FAILURE();
    }
  }

  ByteString signedCertificateTimestamps;
};

namespace {
char const* const rootName = "Test CA 1";
} // namespace

class pkixocsp_VerifyEncodedResponse : public ::testing::Test
{
public:
  static void SetUpTestSuite()
  {
    rootKeyPair.reset(GenerateKeyPair());
    if (!rootKeyPair) {
      abort();
    }
  }

  void SetUp()
  {
    rootNameDER = CNToDERName(rootName);
    if (ENCODING_FAILED(rootNameDER)) {
      abort();
    }
    Input rootNameDERInput;
    if (rootNameDERInput.Init(rootNameDER.data(), rootNameDER.length())
          != Success) {
      abort();
    }

    serialNumberDER =
      CreateEncodedSerialNumber(static_cast<long>(++rootIssuedCount));
    if (ENCODING_FAILED(serialNumberDER)) {
      abort();
    }
    Input serialNumberDERInput;
    if (serialNumberDERInput.Init(serialNumberDER.data(),
                                  serialNumberDER.length()) != Success) {
      abort();
    }

    Input rootSPKIDER;
    if (rootSPKIDER.Init(rootKeyPair->subjectPublicKeyInfo.data(),
                         rootKeyPair->subjectPublicKeyInfo.length())
          != Success) {
      abort();
    }
    endEntityCertID.reset(new (std::nothrow) CertID(rootNameDERInput, rootSPKIDER,
                                                    serialNumberDERInput));
    if (!endEntityCertID) {
      abort();
    }
  }

  static ScopedTestKeyPair rootKeyPair;
  static uint32_t rootIssuedCount;
  OCSPTestTrustDomain trustDomain;

  // endEntityCertID references rootKeyPair, rootNameDER, and serialNumberDER.
  ByteString rootNameDER;
  ByteString serialNumberDER;
  // endEntityCertID references rootKeyPair, rootNameDER, and serialNumberDER.
  ScopedCertID endEntityCertID;
};

/*static*/ ScopedTestKeyPair pkixocsp_VerifyEncodedResponse::rootKeyPair;
/*static*/ uint32_t pkixocsp_VerifyEncodedResponse::rootIssuedCount = 0;

///////////////////////////////////////////////////////////////////////////////
// responseStatus

struct WithoutResponseBytes
{
  uint8_t responseStatus;
  Result expectedError;
};

static const WithoutResponseBytes WITHOUT_RESPONSEBYTES[] = {
  { OCSPResponseContext::successful, Result::ERROR_OCSP_MALFORMED_RESPONSE },
  { OCSPResponseContext::malformedRequest, Result::ERROR_OCSP_MALFORMED_REQUEST },
  { OCSPResponseContext::internalError, Result::ERROR_OCSP_SERVER_ERROR },
  { OCSPResponseContext::tryLater, Result::ERROR_OCSP_TRY_SERVER_LATER },
  { 4/*unused*/, Result::ERROR_OCSP_UNKNOWN_RESPONSE_STATUS },
  { OCSPResponseContext::sigRequired, Result::ERROR_OCSP_REQUEST_NEEDS_SIG },
  { OCSPResponseContext::unauthorized, Result::ERROR_OCSP_UNAUTHORIZED_REQUEST },
  { OCSPResponseContext::unauthorized + 1,
    Result::ERROR_OCSP_UNKNOWN_RESPONSE_STATUS
  },
};

class pkixocsp_VerifyEncodedResponse_WithoutResponseBytes
  : public pkixocsp_VerifyEncodedResponse
  , public ::testing::WithParamInterface<WithoutResponseBytes>
{
protected:
  ByteString CreateEncodedOCSPErrorResponse(uint8_t status)
  {
    static const Input EMPTY;
    OCSPResponseContext context(CertID(EMPTY, EMPTY, EMPTY),
                                oneDayBeforeNow);
    context.responseStatus = status;
    context.skipResponseBytes = true;
    return CreateEncodedOCSPResponse(context);
  }
};

TEST_P(pkixocsp_VerifyEncodedResponse_WithoutResponseBytes, CorrectErrorCode)
{
  ByteString
    responseString(CreateEncodedOCSPErrorResponse(GetParam().responseStatus));
  Input response;
  ASSERT_EQ(Success,
            response.Init(responseString.data(), responseString.length()));
  bool expired;
  ASSERT_EQ(GetParam().expectedError,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID, Now(),
                                      END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
}

INSTANTIATE_TEST_SUITE_P(pkixocsp_VerifyEncodedResponse_WithoutResponseBytes,
                        pkixocsp_VerifyEncodedResponse_WithoutResponseBytes,
                        testing::ValuesIn(WITHOUT_RESPONSEBYTES));

///////////////////////////////////////////////////////////////////////////////
// "successful" responses

namespace {

// Alias for nullptr to aid readability in the code below.
static const char* byKey = nullptr;

} // namespace

class pkixocsp_VerifyEncodedResponse_successful
  : public pkixocsp_VerifyEncodedResponse
{
public:
  void SetUp()
  {
    pkixocsp_VerifyEncodedResponse::SetUp();
  }

  static void SetUpTestSuite()
  {
    pkixocsp_VerifyEncodedResponse::SetUpTestSuite();
  }

  ByteString CreateEncodedOCSPSuccessfulResponse(
                    OCSPResponseContext::CertStatus certStatus,
                    const CertID& certID,
       /*optional*/ const char* signerName,
                    const TestKeyPair& signerKeyPair,
                    time_t producedAt, time_t thisUpdate,
       /*optional*/ const time_t* nextUpdate,
                    const TestSignatureAlgorithm& signatureAlgorithm,
       /*optional*/ const ByteString* certs = nullptr,
       /*optional*/ OCSPResponseExtension* singleExtensions = nullptr,
       /*optional*/ OCSPResponseExtension* responseExtensions = nullptr,
       /*optional*/ DigestAlgorithm certIDHashAlgorithm = DigestAlgorithm::sha1,
       /*optional*/ ByteString certIDHashAlgorithmEncoded = ByteString())
  {
    OCSPResponseContext context(certID, producedAt);
    context.certIDHashAlgorithm = certIDHashAlgorithm;
    context.certIDHashAlgorithmEncoded = certIDHashAlgorithmEncoded;
    if (signerName) {
      context.signerNameDER = CNToDERName(signerName);
      EXPECT_FALSE(ENCODING_FAILED(context.signerNameDER));
    }
    context.signerKeyPair.reset(signerKeyPair.Clone());
    EXPECT_TRUE(context.signerKeyPair.get());
    context.responseStatus = OCSPResponseContext::successful;
    context.producedAt = producedAt;
    context.signatureAlgorithm = signatureAlgorithm;
    context.certs = certs;
    context.singleExtensions = singleExtensions;
    context.responseExtensions = responseExtensions;

    context.certStatus = static_cast<uint8_t>(certStatus);
    context.thisUpdate = thisUpdate;
    context.nextUpdate = nextUpdate ? *nextUpdate : 0;
    context.includeNextUpdate = nextUpdate != nullptr;

    return CreateEncodedOCSPResponse(context);
  }
};

TEST_F(pkixocsp_VerifyEncodedResponse_successful, good_byKey)
{
  ByteString responseString(
               CreateEncodedOCSPSuccessfulResponse(
                         OCSPResponseContext::good, *endEntityCertID, byKey,
                         *rootKeyPair, oneDayBeforeNow,
                         oneDayBeforeNow, &oneDayAfterNow,
                         sha256WithRSAEncryption()));
  Input response;
  ASSERT_EQ(Success,
            response.Init(responseString.data(), responseString.length()));
  bool expired;
  ASSERT_EQ(Success,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID,
                                      Now(), END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
  ASSERT_FALSE(expired);
}

TEST_F(pkixocsp_VerifyEncodedResponse_successful, good_byName)
{
  ByteString responseString(
               CreateEncodedOCSPSuccessfulResponse(
                         OCSPResponseContext::good, *endEntityCertID, rootName,
                         *rootKeyPair, oneDayBeforeNow,
                         oneDayBeforeNow, &oneDayAfterNow,
                         sha256WithRSAEncryption()));
  Input response;
  ASSERT_EQ(Success,
            response.Init(responseString.data(), responseString.length()));
  bool expired;
  ASSERT_EQ(Success,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID, Now(),
                                      END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
  ASSERT_FALSE(expired);
}

TEST_F(pkixocsp_VerifyEncodedResponse_successful, good_byKey_without_nextUpdate)
{
  ByteString responseString(
               CreateEncodedOCSPSuccessfulResponse(
                         OCSPResponseContext::good, *endEntityCertID, byKey,
                         *rootKeyPair, oneDayBeforeNow,
                         oneDayBeforeNow, nullptr,
                         sha256WithRSAEncryption()));
  Input response;
  ASSERT_EQ(Success,
            response.Init(responseString.data(), responseString.length()));
  bool expired;
  ASSERT_EQ(Success,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID, Now(),
                                      END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
  ASSERT_FALSE(expired);
}

TEST_F(pkixocsp_VerifyEncodedResponse_successful, revoked)
{
  ByteString responseString(
               CreateEncodedOCSPSuccessfulResponse(
                         OCSPResponseContext::revoked, *endEntityCertID, byKey,
                         *rootKeyPair, oneDayBeforeNow,
                         oneDayBeforeNow, &oneDayAfterNow,
                         sha256WithRSAEncryption()));
  Input response;
  ASSERT_EQ(Success,
            response.Init(responseString.data(), responseString.length()));
  bool expired;
  ASSERT_EQ(Result::ERROR_REVOKED_CERTIFICATE,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID, Now(),
                                      END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
  ASSERT_FALSE(expired);
}

TEST_F(pkixocsp_VerifyEncodedResponse_successful, unknown)
{
  ByteString responseString(
               CreateEncodedOCSPSuccessfulResponse(
                         OCSPResponseContext::unknown, *endEntityCertID, byKey,
                         *rootKeyPair, oneDayBeforeNow,
                         oneDayBeforeNow, &oneDayAfterNow,
                         sha256WithRSAEncryption()));
  Input response;
  ASSERT_EQ(Success,
            response.Init(responseString.data(), responseString.length()));
  bool expired;
  ASSERT_EQ(Result::ERROR_OCSP_UNKNOWN_CERT,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID, Now(),
                                      END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
  ASSERT_FALSE(expired);
}

TEST_F(pkixocsp_VerifyEncodedResponse_successful,
       good_unsupportedSignatureAlgorithm)
{
  PRUint32 policyMd5;
  ASSERT_EQ(SECSuccess,NSS_GetAlgorithmPolicy(SEC_OID_MD5, &policyMd5));

  /* our encode won't work if MD5 isn't allowed by policy */
  ASSERT_EQ(SECSuccess,
            NSS_SetAlgorithmPolicy(SEC_OID_MD5, NSS_USE_ALG_IN_SIGNATURE, 0));
  ByteString responseString(
               CreateEncodedOCSPSuccessfulResponse(
                         OCSPResponseContext::good, *endEntityCertID, byKey,
                         *rootKeyPair, oneDayBeforeNow,
                         oneDayBeforeNow, &oneDayAfterNow,
                         md5WithRSAEncryption()));
  Input response;
  ASSERT_EQ(Success,
            response.Init(responseString.data(), responseString.length()));
  /* now restore the existing policy */
  ASSERT_EQ(SECSuccess,
           NSS_SetAlgorithmPolicy(SEC_OID_MD5, policyMd5, NSS_USE_ALG_IN_SIGNATURE));
  bool expired;
  ASSERT_EQ(Result::ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID,
                                      Now(), END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
  ASSERT_FALSE(expired);
}

// Added for bug 1079436. The output variable validThrough represents the
// latest time for which VerifyEncodedOCSPResponse will succeed, which is
// different from the nextUpdate time in the OCSP response due to the slop we
// add for time comparisons to deal with clock skew.
TEST_F(pkixocsp_VerifyEncodedResponse_successful, check_validThrough)
{
  ByteString responseString(
               CreateEncodedOCSPSuccessfulResponse(
                         OCSPResponseContext::good, *endEntityCertID, byKey,
                         *rootKeyPair, oneDayBeforeNow,
                         oneDayBeforeNow, &oneDayAfterNow,
                         sha256WithRSAEncryption()));
  Time validThrough(Time::uninitialized);
  {
    Input response;
    ASSERT_EQ(Success,
              response.Init(responseString.data(), responseString.length()));
    bool expired;
    ASSERT_EQ(Success,
              VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID,
                                        Now(), END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                        response, expired, nullptr,
                                        &validThrough));
    ASSERT_FALSE(expired);
    // The response was created to be valid until one day after now, so the
    // value we got for validThrough should be after that.
    Time oneDayAfterNowAsPKIXTime(
          TimeFromEpochInSeconds(static_cast<uint64_t>(oneDayAfterNow)));
    ASSERT_TRUE(validThrough > oneDayAfterNowAsPKIXTime);
  }
  {
    Input response;
    ASSERT_EQ(Success,
              response.Init(responseString.data(), responseString.length()));
    bool expired;
    // Given validThrough from a previous verification, this response should be
    // valid through that time.
    ASSERT_EQ(Success,
              VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID,
                                        validThrough, END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                        response, expired));
    ASSERT_FALSE(expired);
  }
  {
    Time noLongerValid(validThrough);
    ASSERT_EQ(Success, noLongerValid.AddSeconds(1));
    Input response;
    ASSERT_EQ(Success,
              response.Init(responseString.data(), responseString.length()));
    bool expired;
    // The verification time is now after when the response will be considered
    // valid.
    ASSERT_EQ(Result::ERROR_OCSP_OLD_RESPONSE,
              VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID,
                                        noLongerValid, END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                        response, expired));
    ASSERT_TRUE(expired);
  }
}

TEST_F(pkixocsp_VerifyEncodedResponse_successful, ct_extension)
{
  // python DottedOIDToCode.py --tlv
  //   id_ocsp_singleExtensionSctList 1.3.6.1.4.1.11129.2.4.5
  static const uint8_t tlv_id_ocsp_singleExtensionSctList[] = {
    0x06, 0x0a, 0x2b, 0x06, 0x01, 0x04, 0x01, 0xd6, 0x79, 0x02, 0x04, 0x05
  };
  static const uint8_t dummySctList[] = {
    0x01, 0x02, 0x03, 0x04, 0x05
  };

  OCSPResponseExtension ctExtension;
  ctExtension.id = BytesToByteString(tlv_id_ocsp_singleExtensionSctList);
  // SignedCertificateTimestampList structure is encoded as an OCTET STRING
  // within the extension value (see RFC 6962 section 3.3).
  // pkix decodes it internally and returns the actual structure.
  ctExtension.value = TLV(der::OCTET_STRING, BytesToByteString(dummySctList));

  ByteString responseString(
               CreateEncodedOCSPSuccessfulResponse(
                         OCSPResponseContext::good, *endEntityCertID, byKey,
                         *rootKeyPair, oneDayBeforeNow,
                         oneDayBeforeNow, &oneDayAfterNow,
                         sha256WithRSAEncryption(),
                         /*certs*/ nullptr,
                         &ctExtension));
  Input response;
  ASSERT_EQ(Success,
            response.Init(responseString.data(), responseString.length()));

  bool expired;
  ASSERT_EQ(Success,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID,
                                      Now(), END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
  ASSERT_FALSE(expired);
  ASSERT_EQ(BytesToByteString(dummySctList),
            trustDomain.signedCertificateTimestamps);
}

struct CertIDHashAlgorithm
{
  DigestAlgorithm hashAlgorithm;
  ByteString encodedHashAlgorithm;
  Result expectedResult;
};

// python DottedOIDToCode.py --alg id-sha1 1.3.14.3.2.26
static const uint8_t alg_id_sha1[] = {
  0x30, 0x07, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a
};
// python DottedOIDToCode.py --alg id-sha256 2.16.840.1.101.3.4.2.1
static const uint8_t alg_id_sha256[] = {
  0x30, 0x0b, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01
};
static const uint8_t not_an_encoded_hash_oid[] = {
  0x01, 0x02, 0x03, 0x04
};

static const CertIDHashAlgorithm CERTID_HASH_ALGORITHMS[] = {
  { DigestAlgorithm::sha1, ByteString(), Success },
  { DigestAlgorithm::sha256, ByteString(), Success },
  { DigestAlgorithm::sha384, ByteString(), Success },
  { DigestAlgorithm::sha512, ByteString(), Success },
  { DigestAlgorithm::sha256, BytesToByteString(alg_id_sha1),
    Result::ERROR_OCSP_MALFORMED_RESPONSE },
  { DigestAlgorithm::sha1, BytesToByteString(alg_id_sha256),
    Result::ERROR_OCSP_MALFORMED_RESPONSE },
  { DigestAlgorithm::sha1, BytesToByteString(not_an_encoded_hash_oid),
    Result::ERROR_OCSP_MALFORMED_RESPONSE },
};

class pkixocsp_VerifyEncodedResponse_CertIDHashAlgorithm
  : public pkixocsp_VerifyEncodedResponse_successful
  , public ::testing::WithParamInterface<CertIDHashAlgorithm>
{
};

TEST_P(pkixocsp_VerifyEncodedResponse_CertIDHashAlgorithm, CertIDHashAlgorithm)
{
  ByteString responseString(
               CreateEncodedOCSPSuccessfulResponse(
                         OCSPResponseContext::good, *endEntityCertID, byKey,
                         *rootKeyPair, oneDayBeforeNow,
                         oneDayBeforeNow, &oneDayAfterNow,
                         sha256WithRSAEncryption(),
                         nullptr,
                         nullptr,
                         nullptr,
                         GetParam().hashAlgorithm,
                         GetParam().encodedHashAlgorithm));
  Input response;
  ASSERT_EQ(Success,
            response.Init(responseString.data(), responseString.length()));
  bool expired;
  ASSERT_EQ(GetParam().expectedResult,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID,
                                      Now(), END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
  ASSERT_FALSE(expired);
}

INSTANTIATE_TEST_SUITE_P(pkixocsp_VerifyEncodedResponse_CertIDHashAlgorithm,
                        pkixocsp_VerifyEncodedResponse_CertIDHashAlgorithm,
                        testing::ValuesIn(CERTID_HASH_ALGORITHMS));

///////////////////////////////////////////////////////////////////////////////
// indirect responses (signed by a delegated OCSP responder cert)

class pkixocsp_VerifyEncodedResponse_DelegatedResponder
  : public pkixocsp_VerifyEncodedResponse_successful
{
protected:
  // certSubjectName should be unique for each call. This way, we avoid any
  // issues with NSS caching the certificates internally. For the same reason,
  // we generate a new keypair on each call. Either one of these should be
  // sufficient to avoid issues with the NSS cache, but we do both to be
  // cautious.
  //
  // signerName should be byKey to use the byKey ResponderID construction, or
  // another value (usually equal to certSubjectName) to use the byName
  // ResponderID construction.
  //
  // certSignatureAlgorithm specifies the signature algorithm that the
  // certificate will be signed with, not the OCSP response.
  //
  // If signerEKU is omitted, then the certificate will have the
  // id-kp-OCSPSigning EKU. If signerEKU is SEC_OID_UNKNOWN then it will not
  // have any EKU extension. Otherwise, the certificate will have the given
  // EKU.
  ByteString CreateEncodedIndirectOCSPSuccessfulResponse(
               const char* certSubjectName,
               OCSPResponseContext::CertStatus certStatus,
               const char* signerName,
               const TestSignatureAlgorithm& certSignatureAlgorithm,
               /*optional*/ const Input* signerEKUDER = &OCSPSigningEKUDER,
               /*optional, out*/ ByteString* signerDEROut = nullptr)
  {
    assert(certSubjectName);

    const ByteString extensions[] = {
      signerEKUDER
        ? CreateEncodedEKUExtension(*signerEKUDER, Critical::No)
        : ByteString(),
      ByteString()
    };
    ScopedTestKeyPair signerKeyPair(GenerateKeyPair());
    ByteString signerDER(CreateEncodedCertificate(
                           ++rootIssuedCount, certSignatureAlgorithm,
                           rootName, oneDayBeforeNow, oneDayAfterNow,
                           certSubjectName, *signerKeyPair,
                           signerEKUDER ? extensions : nullptr,
                           *rootKeyPair));
    EXPECT_FALSE(ENCODING_FAILED(signerDER));
    if (signerDEROut) {
      *signerDEROut = signerDER;
    }

    ByteString signerNameDER;
    if (signerName) {
      signerNameDER = CNToDERName(signerName);
      EXPECT_FALSE(ENCODING_FAILED(signerNameDER));
    }
    ByteString certs[] = { signerDER, ByteString() };
    return CreateEncodedOCSPSuccessfulResponse(certStatus, *endEntityCertID,
                                               signerName, *signerKeyPair,
                                               oneDayBeforeNow,
                                               oneDayBeforeNow,
                                               &oneDayAfterNow,
                                               sha256WithRSAEncryption(),
                                               certs);
  }

  static ByteString CreateEncodedCertificate(uint32_t serialNumber,
                                             const TestSignatureAlgorithm& signatureAlg,
                                             const char* issuer,
                                             time_t notBefore,
                                             time_t notAfter,
                                             const char* subject,
                                             const TestKeyPair& subjectKeyPair,
                                /*optional*/ const ByteString* extensions,
                                             const TestKeyPair& signerKeyPair)
  {
    ByteString serialNumberDER(CreateEncodedSerialNumber(
                                 static_cast<long>(serialNumber)));
    if (ENCODING_FAILED(serialNumberDER)) {
      return ByteString();
    }
    ByteString issuerDER(CNToDERName(issuer));
    if (ENCODING_FAILED(issuerDER)) {
      return ByteString();
    }
    ByteString subjectDER(CNToDERName(subject));
    if (ENCODING_FAILED(subjectDER)) {
      return ByteString();
    }
    return ::mozilla::pkix::test::CreateEncodedCertificate(
                                    v3, signatureAlg, serialNumberDER,
                                    issuerDER, notBefore, notAfter,
                                    subjectDER, subjectKeyPair, extensions,
                                    signerKeyPair, signatureAlg);
  }

  static const Input OCSPSigningEKUDER;
};

/*static*/ const Input pkixocsp_VerifyEncodedResponse_DelegatedResponder::
  OCSPSigningEKUDER(tlv_id_kp_OCSPSigning);

TEST_F(pkixocsp_VerifyEncodedResponse_DelegatedResponder, good_byKey)
{
  ByteString responseString(
               CreateEncodedIndirectOCSPSuccessfulResponse(
                         "good_indirect_byKey", OCSPResponseContext::good,
                         byKey, sha256WithRSAEncryption()));
  Input response;
  ASSERT_EQ(Success,
            response.Init(responseString.data(), responseString.length()));
  bool expired;
  ASSERT_EQ(Success,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID, Now(),
                                      END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
  ASSERT_FALSE(expired);
}

TEST_F(pkixocsp_VerifyEncodedResponse_DelegatedResponder, good_byName)
{
  ByteString responseString(
               CreateEncodedIndirectOCSPSuccessfulResponse(
                         "good_indirect_byName", OCSPResponseContext::good,
                         "good_indirect_byName", sha256WithRSAEncryption()));
  Input response;
  ASSERT_EQ(Success,
            response.Init(responseString.data(), responseString.length()));
  bool expired;
  ASSERT_EQ(Success,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID, Now(),
                                      END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
  ASSERT_FALSE(expired);
}

TEST_F(pkixocsp_VerifyEncodedResponse_DelegatedResponder,
       good_byKey_missing_signer)
{
  ScopedTestKeyPair missingSignerKeyPair(GenerateKeyPair());
  ASSERT_TRUE(missingSignerKeyPair.get());

  ByteString responseString(
               CreateEncodedOCSPSuccessfulResponse(
                         OCSPResponseContext::good, *endEntityCertID, byKey,
                         *missingSignerKeyPair, oneDayBeforeNow,
                         oneDayBeforeNow, nullptr,
                         sha256WithRSAEncryption()));
  Input response;
  ASSERT_EQ(Success,
            response.Init(responseString.data(), responseString.length()));
  bool expired;
  ASSERT_EQ(Result::ERROR_OCSP_INVALID_SIGNING_CERT,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID, Now(),
                                      END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
  ASSERT_FALSE(expired);
}

TEST_F(pkixocsp_VerifyEncodedResponse_DelegatedResponder,
       good_byName_missing_signer)
{
  ScopedTestKeyPair missingSignerKeyPair(GenerateKeyPair());
  ASSERT_TRUE(missingSignerKeyPair.get());
  ByteString responseString(
               CreateEncodedOCSPSuccessfulResponse(
                         OCSPResponseContext::good, *endEntityCertID,
                         "missing", *missingSignerKeyPair,
                         oneDayBeforeNow, oneDayBeforeNow, nullptr,
                         sha256WithRSAEncryption()));
  Input response;
  ASSERT_EQ(Success,
            response.Init(responseString.data(), responseString.length()));
  bool expired;
  ASSERT_EQ(Result::ERROR_OCSP_INVALID_SIGNING_CERT,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID, Now(),
                                      END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
  ASSERT_FALSE(expired);
}

TEST_F(pkixocsp_VerifyEncodedResponse_DelegatedResponder, good_expired)
{
  static const char* signerName = "good_indirect_expired";

  const ByteString extensions[] = {
    CreateEncodedEKUExtension(OCSPSigningEKUDER, Critical::No),
    ByteString()
  };

  ScopedTestKeyPair signerKeyPair(GenerateKeyPair());
  ByteString signerDER(CreateEncodedCertificate(
                          ++rootIssuedCount, sha256WithRSAEncryption(),
                          rootName,
                          tenDaysBeforeNow,
                          twoDaysBeforeNow,
                          signerName, *signerKeyPair, extensions,
                          *rootKeyPair));
  ASSERT_FALSE(ENCODING_FAILED(signerDER));

  ByteString certs[] = { signerDER, ByteString() };
  ByteString responseString(
               CreateEncodedOCSPSuccessfulResponse(
                         OCSPResponseContext::good, *endEntityCertID,
                         signerName, *signerKeyPair, oneDayBeforeNow,
                         oneDayBeforeNow, &oneDayAfterNow,
                         sha256WithRSAEncryption(), certs));
  Input response;
  ASSERT_EQ(Success,
            response.Init(responseString.data(), responseString.length()));
  bool expired;
  ASSERT_EQ(Result::ERROR_OCSP_INVALID_SIGNING_CERT,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID, Now(),
                                      END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
}

TEST_F(pkixocsp_VerifyEncodedResponse_DelegatedResponder, good_future)
{
  static const char* signerName = "good_indirect_future";

  const ByteString extensions[] = {
    CreateEncodedEKUExtension(OCSPSigningEKUDER, Critical::No),
    ByteString()
  };

  ScopedTestKeyPair signerKeyPair(GenerateKeyPair());
  ByteString signerDER(CreateEncodedCertificate(
                         ++rootIssuedCount, sha256WithRSAEncryption(),
                         rootName,
                         twoDaysAfterNow,
                         tenDaysAfterNow,
                         signerName, *signerKeyPair, extensions,
                         *rootKeyPair));
  ASSERT_FALSE(ENCODING_FAILED(signerDER));

  ByteString certs[] = { signerDER, ByteString() };
  ByteString responseString(
               CreateEncodedOCSPSuccessfulResponse(
                         OCSPResponseContext::good, *endEntityCertID,
                         signerName, *signerKeyPair, oneDayBeforeNow,
                         oneDayBeforeNow, &oneDayAfterNow,
                         sha256WithRSAEncryption(), certs));
  Input response;
  ASSERT_EQ(Success,
            response.Init(responseString.data(), responseString.length()));
  bool expired;
  ASSERT_EQ(Result::ERROR_OCSP_INVALID_SIGNING_CERT,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID, Now(),
                                      END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
  ASSERT_FALSE(expired);
}

TEST_F(pkixocsp_VerifyEncodedResponse_DelegatedResponder, good_no_eku)
{
  ByteString responseString(
               CreateEncodedIndirectOCSPSuccessfulResponse(
                         "good_indirect_wrong_eku",
                         OCSPResponseContext::good, byKey,
                         sha256WithRSAEncryption(), nullptr));
  Input response;
  ASSERT_EQ(Success,
            response.Init(responseString.data(), responseString.length()));
  bool expired;
  ASSERT_EQ(Result::ERROR_OCSP_INVALID_SIGNING_CERT,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID, Now(),
                                      END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
  ASSERT_FALSE(expired);
}

static const Input serverAuthEKUDER(tlv_id_kp_serverAuth);

TEST_F(pkixocsp_VerifyEncodedResponse_DelegatedResponder,
       good_indirect_wrong_eku)
{
  ByteString responseString(
               CreateEncodedIndirectOCSPSuccessfulResponse(
                        "good_indirect_wrong_eku",
                        OCSPResponseContext::good, byKey,
                        sha256WithRSAEncryption(), &serverAuthEKUDER));
  Input response;
  ASSERT_EQ(Success,
            response.Init(responseString.data(), responseString.length()));
  bool expired;
  ASSERT_EQ(Result::ERROR_OCSP_INVALID_SIGNING_CERT,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID, Now(),
                                      END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
  ASSERT_FALSE(expired);
}

// Test that signature of OCSP response signer cert is verified
TEST_F(pkixocsp_VerifyEncodedResponse_DelegatedResponder, good_tampered_eku)
{
  ByteString tamperedResponse(
               CreateEncodedIndirectOCSPSuccessfulResponse(
                         "good_indirect_tampered_eku",
                         OCSPResponseContext::good, byKey,
                         sha256WithRSAEncryption(), &serverAuthEKUDER));
  ASSERT_EQ(Success,
            TamperOnce(tamperedResponse,
                       ByteString(tlv_id_kp_serverAuth,
                                  sizeof(tlv_id_kp_serverAuth)),
                       ByteString(tlv_id_kp_OCSPSigning,
                                  sizeof(tlv_id_kp_OCSPSigning))));
  Input tamperedResponseInput;
  ASSERT_EQ(Success, tamperedResponseInput.Init(tamperedResponse.data(),
                                                tamperedResponse.length()));
  bool expired;
  ASSERT_EQ(Result::ERROR_OCSP_INVALID_SIGNING_CERT,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID, Now(),
                                      END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      tamperedResponseInput, expired));
  ASSERT_FALSE(expired);
}

TEST_F(pkixocsp_VerifyEncodedResponse_DelegatedResponder, good_unknown_issuer)
{
  static const char* subCAName = "good_indirect_unknown_issuer sub-CA";
  static const char* signerName = "good_indirect_unknown_issuer OCSP signer";

  // unknown issuer
  ScopedTestKeyPair unknownKeyPair(GenerateKeyPair());
  ASSERT_TRUE(unknownKeyPair.get());

  // Delegated responder cert signed by unknown issuer
  const ByteString extensions[] = {
    CreateEncodedEKUExtension(OCSPSigningEKUDER, Critical::No),
    ByteString()
  };
  ScopedTestKeyPair signerKeyPair(GenerateKeyPair());
  ByteString signerDER(CreateEncodedCertificate(
                         1, sha256WithRSAEncryption(), subCAName,
                         oneDayBeforeNow, oneDayAfterNow, signerName,
                         *signerKeyPair, extensions, *unknownKeyPair));
  ASSERT_FALSE(ENCODING_FAILED(signerDER));

  // OCSP response signed by that delegated responder
  ByteString certs[] = { signerDER, ByteString() };
  ByteString responseString(
               CreateEncodedOCSPSuccessfulResponse(
                         OCSPResponseContext::good, *endEntityCertID,
                         signerName, *signerKeyPair, oneDayBeforeNow,
                         oneDayBeforeNow, &oneDayAfterNow,
                         sha256WithRSAEncryption(), certs));
  Input response;
  ASSERT_EQ(Success,
            response.Init(responseString.data(), responseString.length()));
  bool expired;
  ASSERT_EQ(Result::ERROR_OCSP_INVALID_SIGNING_CERT,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID, Now(),
                                      END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
  ASSERT_FALSE(expired);
}

// The CA that issued the OCSP responder cert is a sub-CA of the issuer of
// the certificate that the OCSP response is for. That sub-CA cert is included
// in the OCSP response before the OCSP responder cert.
TEST_F(pkixocsp_VerifyEncodedResponse_DelegatedResponder,
       good_indirect_subca_1_first)
{
  static const char* subCAName = "good_indirect_subca_1_first sub-CA";
  static const char* signerName = "good_indirect_subca_1_first OCSP signer";
  static const long zero = 0;

  // sub-CA of root (root is the direct issuer of endEntity)
  const ByteString subCAExtensions[] = {
    CreateEncodedBasicConstraints(true, &zero, Critical::No),
    ByteString()
  };
  ScopedTestKeyPair subCAKeyPair(GenerateKeyPair());
  ByteString subCADER(CreateEncodedCertificate(
                        ++rootIssuedCount, sha256WithRSAEncryption(), rootName,
                        oneDayBeforeNow, oneDayAfterNow, subCAName,
                        *subCAKeyPair, subCAExtensions, *rootKeyPair));
  ASSERT_FALSE(ENCODING_FAILED(subCADER));

  // Delegated responder cert signed by that sub-CA
  const ByteString extensions[] = {
    CreateEncodedEKUExtension(OCSPSigningEKUDER, Critical::No),
    ByteString(),
  };
  ScopedTestKeyPair signerKeyPair(GenerateKeyPair());
  ByteString signerDER(CreateEncodedCertificate(
                         1, sha256WithRSAEncryption(), subCAName,
                         oneDayBeforeNow, oneDayAfterNow, signerName,
                         *signerKeyPair, extensions, *subCAKeyPair));
  ASSERT_FALSE(ENCODING_FAILED(signerDER));

  // OCSP response signed by the delegated responder issued by the sub-CA
  // that is trying to impersonate the root.
  ByteString certs[] = { subCADER, signerDER, ByteString() };
  ByteString responseString(
               CreateEncodedOCSPSuccessfulResponse(
                         OCSPResponseContext::good, *endEntityCertID,
                         signerName, *signerKeyPair, oneDayBeforeNow,
                         oneDayBeforeNow, &oneDayAfterNow,
                         sha256WithRSAEncryption(), certs));
  Input response;
  ASSERT_EQ(Success,
            response.Init(responseString.data(), responseString.length()));
  bool expired;
  ASSERT_EQ(Result::ERROR_OCSP_INVALID_SIGNING_CERT,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID, Now(),
                                      END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
  ASSERT_FALSE(expired);
}

// The CA that issued the OCSP responder cert is a sub-CA of the issuer of
// the certificate that the OCSP response is for. That sub-CA cert is included
// in the OCSP response after the OCSP responder cert.
TEST_F(pkixocsp_VerifyEncodedResponse_DelegatedResponder,
       good_indirect_subca_1_second)
{
  static const char* subCAName = "good_indirect_subca_1_second sub-CA";
  static const char* signerName = "good_indirect_subca_1_second OCSP signer";
  static const long zero = 0;

  // sub-CA of root (root is the direct issuer of endEntity)
  const ByteString subCAExtensions[] = {
    CreateEncodedBasicConstraints(true, &zero, Critical::No),
    ByteString()
  };
  ScopedTestKeyPair subCAKeyPair(GenerateKeyPair());
  ByteString subCADER(CreateEncodedCertificate(++rootIssuedCount,
                                               sha256WithRSAEncryption(),
                                               rootName,
                                               oneDayBeforeNow, oneDayAfterNow,
                                               subCAName, *subCAKeyPair,
                                               subCAExtensions, *rootKeyPair));
  ASSERT_FALSE(ENCODING_FAILED(subCADER));

  // Delegated responder cert signed by that sub-CA
  const ByteString extensions[] = {
    CreateEncodedEKUExtension(OCSPSigningEKUDER, Critical::No),
    ByteString()
  };
  ScopedTestKeyPair signerKeyPair(GenerateKeyPair());
  ByteString signerDER(CreateEncodedCertificate(
                         1, sha256WithRSAEncryption(), subCAName,
                         oneDayBeforeNow, oneDayAfterNow, signerName,
                         *signerKeyPair, extensions, *subCAKeyPair));
  ASSERT_FALSE(ENCODING_FAILED(signerDER));

  // OCSP response signed by the delegated responder issued by the sub-CA
  // that is trying to impersonate the root.
  ByteString certs[] = { signerDER, subCADER, ByteString() };
  ByteString responseString(
                 CreateEncodedOCSPSuccessfulResponse(
                         OCSPResponseContext::good, *endEntityCertID,
                         signerName, *signerKeyPair, oneDayBeforeNow,
                         oneDayBeforeNow, &oneDayAfterNow,
                         sha256WithRSAEncryption(), certs));
  Input response;
  ASSERT_EQ(Success,
            response.Init(responseString.data(), responseString.length()));
  bool expired;
  ASSERT_EQ(Result::ERROR_OCSP_INVALID_SIGNING_CERT,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID, Now(),
                                      END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
  ASSERT_FALSE(expired);
}

TEST_F(pkixocsp_VerifyEncodedResponse_DelegatedResponder,
       good_unsupportedSignatureAlgorithmOnResponder)
{
  // Note that the algorithm ID (md5WithRSAEncryption) identifies the signature
  // algorithm that will be used to sign the certificate that issues the OCSP
  // responses, not the responses themselves.
  PRUint32 policyMd5;
  ASSERT_EQ(SECSuccess,NSS_GetAlgorithmPolicy(SEC_OID_MD5, &policyMd5));

  /* our encode won't work if MD5 isn't allowed by policy */
  ASSERT_EQ(SECSuccess,
            NSS_SetAlgorithmPolicy(SEC_OID_MD5, NSS_USE_ALG_IN_SIGNATURE, 0));
  ByteString responseString(
               CreateEncodedIndirectOCSPSuccessfulResponse(
                         "good_indirect_unsupportedSignatureAlgorithm",
                         OCSPResponseContext::good, byKey,
                         md5WithRSAEncryption()));
  Input response;
  /* now restore the existing policy */
  ASSERT_EQ(Success,
            response.Init(responseString.data(), responseString.length()));
  ASSERT_EQ(SECSuccess,
            NSS_SetAlgorithmPolicy(SEC_OID_MD5, policyMd5, NSS_USE_ALG_IN_SIGNATURE));
  bool expired;
  ASSERT_EQ(Result::ERROR_OCSP_INVALID_SIGNING_CERT,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID, Now(),
                                      END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
}

class pkixocsp_VerifyEncodedResponse_GetCertTrust
  : public pkixocsp_VerifyEncodedResponse_DelegatedResponder {
public:
  void SetUp()
  {
    pkixocsp_VerifyEncodedResponse_DelegatedResponder::SetUp();

    responseString =
        CreateEncodedIndirectOCSPSuccessfulResponse(
          "OCSPGetCertTrustTest Signer", OCSPResponseContext::good,
          byKey, sha256WithRSAEncryption(), &OCSPSigningEKUDER,
          &signerCertDER);
    if (ENCODING_FAILED(responseString)) {
      abort();
    }
    if (response.Init(responseString.data(), responseString.length())
          != Success) {
      abort();
    }
    if (signerCertDER.length() == 0) {
      abort();
    }
  }

  class TrustDomain final : public OCSPTestTrustDomain
  {
  public:
    TrustDomain()
      : certTrustLevel(TrustLevel::InheritsTrust)
    {
    }

    bool SetCertTrust(const ByteString& aCertDER, TrustLevel aCertTrustLevel)
    {
      this->certDER = aCertDER;
      this->certTrustLevel = aCertTrustLevel;
      return true;
    }
  private:
    Result GetCertTrust(EndEntityOrCA endEntityOrCA, const CertPolicyId&,
                        Input candidateCert, /*out*/ TrustLevel& trustLevel)
                        override
    {
      EXPECT_EQ(endEntityOrCA, EndEntityOrCA::MustBeEndEntity);
      EXPECT_FALSE(certDER.empty());
      Input certDERInput;
      EXPECT_EQ(Success, certDERInput.Init(certDER.data(), certDER.length()));
      EXPECT_TRUE(InputsAreEqual(certDERInput, candidateCert));
      trustLevel = certTrustLevel;
      return Success;
    }

    ByteString certDER;
    TrustLevel certTrustLevel;
  };

// trustDomain deliberately shadows the inherited field so that it isn't used
// by accident. See bug 1339921.
// Unfortunately GCC can't parse __has_warning("-Wshadow-field") even if it's
// the latter part of a conjunction that would evaluate to false, so we have to
// wrap it in a separate preprocessor conditional rather than using &&.
#if defined(__clang__)
  #if __has_warning("-Wshadow-field")
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wshadow-field"
  #endif
#endif
  TrustDomain trustDomain;
#if defined(__clang__)
  #if __has_warning("-Wshadow-field")
    #pragma clang diagnostic pop
  #endif
#endif
  ByteString signerCertDER;
  ByteString responseString;
  Input response; // references data in responseString
};

TEST_F(pkixocsp_VerifyEncodedResponse_GetCertTrust, InheritTrust)
{
  ASSERT_TRUE(trustDomain.SetCertTrust(signerCertDER,
                                       TrustLevel::InheritsTrust));
  bool expired;
  ASSERT_EQ(Success,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID, Now(),
                                      END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
  ASSERT_FALSE(expired);
}

TEST_F(pkixocsp_VerifyEncodedResponse_GetCertTrust, TrustAnchor)
{
  ASSERT_TRUE(trustDomain.SetCertTrust(signerCertDER,
                                       TrustLevel::TrustAnchor));
  bool expired;
  ASSERT_EQ(Success,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID, Now(),
                                      END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      response, expired));
  ASSERT_FALSE(expired);
}

TEST_F(pkixocsp_VerifyEncodedResponse_GetCertTrust, ActivelyDistrusted)
{
  ASSERT_TRUE(trustDomain.SetCertTrust(signerCertDER,
                                       TrustLevel::ActivelyDistrusted));
  Input responseInput;
  ASSERT_EQ(Success,
            responseInput.Init(responseString.data(),
                               responseString.length()));
  bool expired;
  ASSERT_EQ(Result::ERROR_OCSP_INVALID_SIGNING_CERT,
            VerifyEncodedOCSPResponse(trustDomain, *endEntityCertID, Now(),
                                      END_ENTITY_MAX_LIFETIME_IN_DAYS,
                                      responseInput, expired));
  ASSERT_FALSE(expired);
}
