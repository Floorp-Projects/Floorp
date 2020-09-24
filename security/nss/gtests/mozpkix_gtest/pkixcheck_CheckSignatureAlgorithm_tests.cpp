/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2015 Mozilla Contributors
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

using namespace mozilla::pkix;
using namespace mozilla::pkix::test;

namespace mozilla { namespace pkix {

extern Result CheckSignatureAlgorithm(
                TrustDomain& trustDomain, EndEntityOrCA endEntityOrCA,
                Time notBefore,
                const der::SignedDataWithSignature& signedData,
                Input signatureValue);

} } // namespace mozilla::pkix

struct CheckSignatureAlgorithmTestParams
{
  ByteString signatureAlgorithmValue;
  ByteString signatureValue;
  unsigned int signatureLengthInBytes;
  Result expectedResult;
};

::std::ostream& operator<<(::std::ostream& os,
                           const CheckSignatureAlgorithmTestParams&)
{
  return os << "TODO (bug 1318770)";
}

#define BS(s) ByteString(s, MOZILLA_PKIX_ARRAY_LENGTH(s))

// python DottedOIDToCode.py --tlv sha256WithRSAEncryption 1.2.840.113549.1.1.11
static const uint8_t tlv_sha256WithRSAEncryption[] = {
  0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b
};

// Same as tlv_sha256WithRSAEncryption, except one without the "0x0b" and with
// the DER length decreased accordingly.
static const uint8_t tlv_sha256WithRSAEncryption_truncated[] = {
  0x06, 0x08, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01
};

// python DottedOIDToCode.py --tlv sha-1WithRSAEncryption 1.2.840.113549.1.1.5
static const uint8_t tlv_sha_1WithRSAEncryption[] = {
  0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05
};

// python DottedOIDToCode.py --tlv sha1WithRSASignature 1.3.14.3.2.29
static const uint8_t tlv_sha1WithRSASignature[] = {
  0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1d
};

// python DottedOIDToCode.py --tlv md5WithRSAEncryption 1.2.840.113549.1.1.4
static const uint8_t tlv_md5WithRSAEncryption[] = {
  0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x04
};

static const CheckSignatureAlgorithmTestParams
  CHECKSIGNATUREALGORITHM_TEST_PARAMS[] =
{
  { // Both algorithm IDs are empty
    ByteString(),
    ByteString(),
    2048 / 8,
    Result::ERROR_BAD_DER,
  },
  { // signatureAlgorithm is empty, signature is supported.
    ByteString(),
    BS(tlv_sha256WithRSAEncryption),
    2048 / 8,
    Result::ERROR_BAD_DER,
  },
  { // signatureAlgorithm is supported, signature is empty.
    BS(tlv_sha256WithRSAEncryption),
    ByteString(),
    2048 / 8,
    Result::ERROR_BAD_DER,
  },
  { // Algorithms match, both are supported.
    BS(tlv_sha256WithRSAEncryption),
    BS(tlv_sha256WithRSAEncryption),
    2048 / 8,
    Success
  },
  { // Algorithms do not match because signatureAlgorithm is truncated.
    BS(tlv_sha256WithRSAEncryption_truncated),
    BS(tlv_sha256WithRSAEncryption),
    2048 / 8,
    Result::ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED
  },
  { // Algorithms do not match because signature is truncated.
    BS(tlv_sha256WithRSAEncryption),
    BS(tlv_sha256WithRSAEncryption_truncated),
    2048 / 8,
    Result::ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED
  },
  { // Algorithms do not match, both are supported.
    BS(tlv_sha_1WithRSAEncryption),
    BS(tlv_sha256WithRSAEncryption),
    2048 / 8,
    Result::ERROR_SIGNATURE_ALGORITHM_MISMATCH,
  },
  { // Algorithms do not match, both are supported.
    BS(tlv_sha256WithRSAEncryption),
    BS(tlv_sha_1WithRSAEncryption),
    2048 / 8,
    Result::ERROR_SIGNATURE_ALGORITHM_MISMATCH,
  },
  { // Algorithms match, both are unsupported.
    BS(tlv_md5WithRSAEncryption),
    BS(tlv_md5WithRSAEncryption),
    2048 / 8,
    Result::ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED
  },
  { // signatureAlgorithm is unsupported, signature is supported.
    BS(tlv_md5WithRSAEncryption),
    BS(tlv_sha256WithRSAEncryption),
    2048 / 8,
    Result::ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED
  },
  { // signatureAlgorithm is supported, signature is unsupported.
    BS(tlv_sha256WithRSAEncryption),
    BS(tlv_md5WithRSAEncryption),
    2048 / 8,
    Result::ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED
  },
  { // Both have the optional NULL parameter.
    BS(tlv_sha256WithRSAEncryption) + TLV(der::NULLTag, ByteString()),
    BS(tlv_sha256WithRSAEncryption) + TLV(der::NULLTag, ByteString()),
    2048 / 8,
    Success
  },
  { // signatureAlgorithm has the optional NULL parameter, signature doesn't.
    BS(tlv_sha256WithRSAEncryption) + TLV(der::NULLTag, ByteString()),
    BS(tlv_sha256WithRSAEncryption),
    2048 / 8,
    Success
  },
  { // signatureAlgorithm does not have the optional NULL parameter, signature
    // does.
    BS(tlv_sha256WithRSAEncryption),
    BS(tlv_sha256WithRSAEncryption) + TLV(der::NULLTag, ByteString()),
    2048 / 8,
    Success
  },
  { // The different OIDs for RSA-with-SHA1 we support are semantically
    // equivalent.
    BS(tlv_sha1WithRSASignature),
    BS(tlv_sha_1WithRSAEncryption),
    2048 / 8,
    Success,
  },
  { // The different OIDs for RSA-with-SHA1 we support are semantically
    // equivalent (opposite order).
    BS(tlv_sha_1WithRSAEncryption),
    BS(tlv_sha1WithRSASignature),
    2048 / 8,
    Success,
  },
  { // Algorithms match, both are supported, key size is not a multile of 128
    // bits. This test verifies that we're not wrongly rounding up the
    // signature size like we did in the original patch for bug 1131767.
    BS(tlv_sha256WithRSAEncryption),
    BS(tlv_sha256WithRSAEncryption),
    (2048 / 8) - 1,
    Success
  },
};

class pkixcheck_CheckSignatureAlgorithm
  : public ::testing::Test
  , public ::testing::WithParamInterface<CheckSignatureAlgorithmTestParams>
{
};

class pkixcheck_CheckSignatureAlgorithm_TrustDomain final
  : public EverythingFailsByDefaultTrustDomain
{
public:
  explicit pkixcheck_CheckSignatureAlgorithm_TrustDomain(
             unsigned int aPublicKeySizeInBits)
    : publicKeySizeInBits(aPublicKeySizeInBits)
    , checkedDigestAlgorithm(false)
    , checkedModulusSizeInBits(false)
  {
  }

  Result CheckSignatureDigestAlgorithm(DigestAlgorithm, EndEntityOrCA, Time)
    override
  {
    checkedDigestAlgorithm = true;
    return Success;
  }

  Result CheckRSAPublicKeyModulusSizeInBits(EndEntityOrCA endEntityOrCA,
                                            unsigned int modulusSizeInBits)
    override
  {
    EXPECT_EQ(EndEntityOrCA::MustBeEndEntity, endEntityOrCA);
    EXPECT_EQ(publicKeySizeInBits, modulusSizeInBits);
    checkedModulusSizeInBits = true;
    return Success;
  }

  const unsigned int publicKeySizeInBits;
  bool checkedDigestAlgorithm;
  bool checkedModulusSizeInBits;
};

TEST_P(pkixcheck_CheckSignatureAlgorithm, CheckSignatureAlgorithm)
{
  const Time now(Now());
  const CheckSignatureAlgorithmTestParams& params(GetParam());

  Input signatureValueInput;
  ASSERT_EQ(Success,
            signatureValueInput.Init(params.signatureValue.data(),
                                     params.signatureValue.length()));

  pkixcheck_CheckSignatureAlgorithm_TrustDomain
    trustDomain(params.signatureLengthInBytes * 8);

  der::SignedDataWithSignature signedData;
  ASSERT_EQ(Success,
            signedData.algorithm.Init(params.signatureAlgorithmValue.data(),
                                      params.signatureAlgorithmValue.length()));

  ByteString dummySignature(params.signatureLengthInBytes, 0xDE);
  ASSERT_EQ(Success,
            signedData.signature.Init(dummySignature.data(),
                                      dummySignature.length()));

  ASSERT_EQ(params.expectedResult,
            CheckSignatureAlgorithm(trustDomain, EndEntityOrCA::MustBeEndEntity,
                                    now, signedData, signatureValueInput));
  ASSERT_EQ(params.expectedResult == Success,
            trustDomain.checkedDigestAlgorithm);
  ASSERT_EQ(params.expectedResult == Success,
            trustDomain.checkedModulusSizeInBits);
}

INSTANTIATE_TEST_CASE_P(
  pkixcheck_CheckSignatureAlgorithm, pkixcheck_CheckSignatureAlgorithm,
  testing::ValuesIn(CHECKSIGNATUREALGORITHM_TEST_PARAMS));

class pkixcheck_CheckSignatureAlgorithm_BuildCertChain_TrustDomain
  : public DefaultCryptoTrustDomain
{
public:
  explicit pkixcheck_CheckSignatureAlgorithm_BuildCertChain_TrustDomain(
             const ByteString& aIssuer)
    : issuer(aIssuer)
  {
  }

  Result GetCertTrust(EndEntityOrCA, const CertPolicyId&,
                      Input cert, /*out*/ TrustLevel& trustLevel) override
  {
    trustLevel = InputEqualsByteString(cert, issuer)
               ? TrustLevel::TrustAnchor
               : TrustLevel::InheritsTrust;
    return Success;
  }

  Result FindIssuer(Input, IssuerChecker& checker, Time) override
  {
    EXPECT_FALSE(ENCODING_FAILED(issuer));

    Input issuerInput;
    EXPECT_EQ(Success, issuerInput.Init(issuer.data(), issuer.length()));

    bool keepGoing;
    EXPECT_EQ(Success, checker.Check(issuerInput, nullptr, keepGoing));
    EXPECT_FALSE(keepGoing);

    return Success;
  }

  Result CheckRevocation(EndEntityOrCA, const CertID&, Time, Time, Duration,
                         /*optional*/ const Input*,
                         /*optional*/ const Input*) override
  {
    return Success;
  }

  Result IsChainValid(const DERArray&, Time, const CertPolicyId&) override
  {
    return Success;
  }

  ByteString issuer;
};

// Test that CheckSignatureAlgorithm actually gets called at some point when
// BuildCertChain is called.
TEST_F(pkixcheck_CheckSignatureAlgorithm, BuildCertChain)
{
  ScopedTestKeyPair keyPair(CloneReusedKeyPair());
  ASSERT_TRUE(keyPair.get());

  ByteString issuerExtensions[2];
  issuerExtensions[0] = CreateEncodedBasicConstraints(true, nullptr,
                                                      Critical::No);
  ASSERT_FALSE(ENCODING_FAILED(issuerExtensions[0]));

  ByteString issuer(CreateEncodedCertificate(3,
                                             sha256WithRSAEncryption(),
                                             CreateEncodedSerialNumber(1),
                                             CNToDERName("issuer"),
                                             oneDayBeforeNow, oneDayAfterNow,
                                             CNToDERName("issuer"),
                                             *keyPair,
                                             issuerExtensions,
                                             *keyPair,
                                             sha256WithRSAEncryption()));
  ASSERT_FALSE(ENCODING_FAILED(issuer));

  ByteString subject(CreateEncodedCertificate(3,
                                              sha1WithRSAEncryption(),
                                              CreateEncodedSerialNumber(2),
                                              CNToDERName("issuer"),
                                              oneDayBeforeNow, oneDayAfterNow,
                                              CNToDERName("subject"),
                                              *keyPair,
                                              nullptr,
                                              *keyPair,
                                              sha256WithRSAEncryption()));
  ASSERT_FALSE(ENCODING_FAILED(subject));

  Input subjectInput;
  ASSERT_EQ(Success, subjectInput.Init(subject.data(), subject.length()));
  pkixcheck_CheckSignatureAlgorithm_BuildCertChain_TrustDomain
    trustDomain(issuer);
  Result rv = BuildCertChain(trustDomain, subjectInput, Now(),
                             EndEntityOrCA::MustBeEndEntity,
                             KeyUsage::noParticularKeyUsageRequired,
                             KeyPurposeId::anyExtendedKeyUsage,
                             CertPolicyId::anyPolicy,
                             nullptr);
  ASSERT_EQ(Result::ERROR_SIGNATURE_ALGORITHM_MISMATCH, rv);
}
