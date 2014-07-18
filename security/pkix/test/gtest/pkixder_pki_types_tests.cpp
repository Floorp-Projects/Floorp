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

#include <functional>
#include <vector>

#include "nssgtest.h"
#include "pkix/pkixtypes.h"
#include "pkixder.h"

using namespace mozilla::pkix;
using namespace mozilla::pkix::der;
using namespace mozilla::pkix::test;

namespace {

class pkixder_pki_types_tests : public ::testing::Test { };

TEST_F(pkixder_pki_types_tests, CertificateSerialNumber)
{
  const uint8_t DER_CERT_SERIAL[] = {
    0x02,                       // INTEGER
    8,                          // length
    0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_CERT_SERIAL, sizeof DER_CERT_SERIAL));

  SECItem item;
  ASSERT_EQ(Success, CertificateSerialNumber(input, item));

  ASSERT_EQ(sizeof DER_CERT_SERIAL - 2, item.len);
  ASSERT_TRUE(memcmp(item.data, DER_CERT_SERIAL + 2,
                     sizeof DER_CERT_SERIAL - 2) == 0);
}

TEST_F(pkixder_pki_types_tests, CertificateSerialNumberLongest)
{
  const uint8_t DER_CERT_SERIAL_LONGEST[] = {
    0x02,                       // INTEGER
    20,                         // length
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_CERT_SERIAL_LONGEST,
                                sizeof DER_CERT_SERIAL_LONGEST));

  SECItem item;
  ASSERT_EQ(Success, CertificateSerialNumber(input, item));

  ASSERT_EQ(sizeof DER_CERT_SERIAL_LONGEST - 2, item.len);
  ASSERT_TRUE(memcmp(item.data, DER_CERT_SERIAL_LONGEST + 2,
                     sizeof DER_CERT_SERIAL_LONGEST - 2) == 0);
}

TEST_F(pkixder_pki_types_tests, CertificateSerialNumberCrazyLong)
{
  const uint8_t DER_CERT_SERIAL_CRAZY_LONG[] = {
    0x02,                       // INTEGER
    32,                         // length
    1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_CERT_SERIAL_CRAZY_LONG,
                                sizeof DER_CERT_SERIAL_CRAZY_LONG));

  SECItem item;
  ASSERT_EQ(Success, CertificateSerialNumber(input, item));
}

TEST_F(pkixder_pki_types_tests, CertificateSerialNumberZeroLength)
{
  const uint8_t DER_CERT_SERIAL_ZERO_LENGTH[] = {
    0x02,                       // INTEGER
    0x00                        // length
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_CERT_SERIAL_ZERO_LENGTH,
                                sizeof DER_CERT_SERIAL_ZERO_LENGTH));

  SECItem item;
  ASSERT_EQ(Result::ERROR_BAD_DER, CertificateSerialNumber(input, item));
}

TEST_F(pkixder_pki_types_tests, OptionalVersionV1ExplicitEncodingAllowed)
{
  const uint8_t DER_OPTIONAL_VERSION_V1[] = {
    0xa0, 0x03,                   // context specific 0
    0x02, 0x01, 0x00              // INTEGER(0)
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_OPTIONAL_VERSION_V1,
                                sizeof DER_OPTIONAL_VERSION_V1));

  // XXX(bug 1031093): We shouldn't accept an explicit encoding of v1, but we
  // do here for compatibility reasons.
  // Version version;
  // ASSERT_EQ(Result::ERROR_BAD_DER, OptionalVersion(input, version));
  der::Version version = der::Version::v3;
  ASSERT_EQ(Success, OptionalVersion(input, version));
  ASSERT_EQ(der::Version::v1, version);
}

TEST_F(pkixder_pki_types_tests, OptionalVersionV2)
{
  const uint8_t DER_OPTIONAL_VERSION_V2[] = {
    0xa0, 0x03,                   // context specific 0
    0x02, 0x01, 0x01              // INTEGER(1)
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_OPTIONAL_VERSION_V2,
                                sizeof DER_OPTIONAL_VERSION_V2));

  der::Version version = der::Version::v1;
  ASSERT_EQ(Success, OptionalVersion(input, version));
  ASSERT_EQ(der::Version::v2, version);
}

TEST_F(pkixder_pki_types_tests, OptionalVersionV3)
{
  const uint8_t DER_OPTIONAL_VERSION_V3[] = {
    0xa0, 0x03,                   // context specific 0
    0x02, 0x01, 0x02              // INTEGER(2)
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_OPTIONAL_VERSION_V3,
                                sizeof DER_OPTIONAL_VERSION_V3));

  der::Version version = der::Version::v1;
  ASSERT_EQ(Success, OptionalVersion(input, version));
  ASSERT_EQ(der::Version::v3, version);
}

TEST_F(pkixder_pki_types_tests, OptionalVersionUnknown)
{
  const uint8_t DER_OPTIONAL_VERSION_INVALID[] = {
    0xa0, 0x03,                   // context specific 0
    0x02, 0x01, 0x42              // INTEGER(0x42)
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_OPTIONAL_VERSION_INVALID,
                                sizeof DER_OPTIONAL_VERSION_INVALID));

  der::Version version = der::Version::v1;
  ASSERT_EQ(Result::ERROR_BAD_DER, OptionalVersion(input, version));
}

TEST_F(pkixder_pki_types_tests, OptionalVersionInvalidTooLong)
{
  const uint8_t DER_OPTIONAL_VERSION_INVALID_TOO_LONG[] = {
    0xa0, 0x03,                   // context specific 0
    0x02, 0x02, 0x12, 0x34        // INTEGER(0x1234)
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_OPTIONAL_VERSION_INVALID_TOO_LONG,
                                sizeof DER_OPTIONAL_VERSION_INVALID_TOO_LONG));

  der::Version version;
  ASSERT_EQ(Result::ERROR_BAD_DER, OptionalVersion(input, version));
}

TEST_F(pkixder_pki_types_tests, OptionalVersionMissing)
{
  const uint8_t DER_OPTIONAL_VERSION_MISSING[] = {
    0x02, 0x11, 0x22              // INTEGER
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_OPTIONAL_VERSION_MISSING,
                                sizeof DER_OPTIONAL_VERSION_MISSING));

  der::Version version = der::Version::v3;
  ASSERT_EQ(Success, OptionalVersion(input, version));
  ASSERT_EQ(der::Version::v1, version);
}

static const size_t MAX_ALGORITHM_OID_DER_LENGTH = 13;

template <typename T>
struct AlgorithmIdentifierTestInfo
{
  T algorithm;
  uint8_t der[MAX_ALGORITHM_OID_DER_LENGTH];
  size_t derLength;
};

class pkixder_DigestAlgorithmIdentifier
  : public NSSTest
  , public ::testing::WithParamInterface<
                AlgorithmIdentifierTestInfo<DigestAlgorithm>>
{
};

static const AlgorithmIdentifierTestInfo<DigestAlgorithm>
DIGEST_ALGORITHM_TEST_INFO[] = {
  { DigestAlgorithm::sha512,
    { 0x30, 0x0b, 0x06, 0x09,
      0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03 },
    13
  },
  { DigestAlgorithm::sha384,
    { 0x30, 0x0b, 0x06, 0x09,
      0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02 },
    13
  },
  { DigestAlgorithm::sha256,
    { 0x30, 0x0b, 0x06, 0x09,
      0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01 },
    13
  },
  { DigestAlgorithm::sha1,
    { 0x30, 0x07, 0x06, 0x05,
      0x2b, 0x0e, 0x03, 0x02, 0x1a },
    9
  },
};

TEST_P(pkixder_DigestAlgorithmIdentifier, Valid)
{
  const AlgorithmIdentifierTestInfo<DigestAlgorithm>& param(GetParam());

  {
    Input input;
    ASSERT_EQ(Success, input.Init(param.der, param.derLength));
    DigestAlgorithm alg;
    ASSERT_EQ(Success, DigestAlgorithmIdentifier(input, alg));
    ASSERT_EQ(param.algorithm, alg);
    ASSERT_EQ(Success, End(input));
  }

  {
    uint8_t derWithNullParam[MAX_ALGORITHM_OID_DER_LENGTH + 2];
    memcpy(derWithNullParam, param.der, param.derLength);
    derWithNullParam[1] += 2; // we're going to expand the value by 2 bytes
    derWithNullParam[param.derLength] = 0x05; // NULL tag
    derWithNullParam[param.derLength + 1] = 0x00; // length zero

    Input input;
    ASSERT_EQ(Success, input.Init(derWithNullParam, param.derLength + 2));
    DigestAlgorithm alg;
    ASSERT_EQ(Success, DigestAlgorithmIdentifier(input, alg));
    ASSERT_EQ(param.algorithm, alg);
    ASSERT_EQ(Success, End(input));
  }
}

INSTANTIATE_TEST_CASE_P(pkixder_DigestAlgorithmIdentifier,
                        pkixder_DigestAlgorithmIdentifier,
                        testing::ValuesIn(DIGEST_ALGORITHM_TEST_INFO));

TEST_F(pkixder_DigestAlgorithmIdentifier, Invalid_MD5)
{
  // The OID identifies MD5 (1.2.840.113549.2.5). It is invalid because we
  // don't accept MD5 as a hash algorithm.
  static const uint8_t DER[] = {
    0x30, 0x0a, 0x06, 0x08,
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x02, 0x05
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER, sizeof(DER)));
  DigestAlgorithm alg;
  ASSERT_EQ(Result::ERROR_INVALID_ALGORITHM,
            DigestAlgorithmIdentifier(input, alg));
}

TEST_F(pkixder_DigestAlgorithmIdentifier, Invalid_Digest_ECDSA_WITH_SHA256)
{
  // The OID identifies ecdsa-with-SHA256 (1.2.840.10045.4.3.2). It is invalid
  // because ECDSA-with-SHA256 is not a hash algorithm.
  static const uint8_t DER[] = {
    0x30, 0x0a, 0x06, 0x08,
    0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02, //
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER, sizeof(DER)));
  DigestAlgorithm alg;
  ASSERT_EQ(Result::ERROR_INVALID_ALGORITHM,
            DigestAlgorithmIdentifier(input, alg));
}

static const AlgorithmIdentifierTestInfo<SignatureAlgorithm>
  SIGNATURE_ALGORITHM_TEST_INFO[] =
{
  { SignatureAlgorithm::ecdsa_with_sha512,
    { 0x30, 0x0a, 0x06, 0x08,
      0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x04 },
    12,
  },
  { SignatureAlgorithm::ecdsa_with_sha384,
    { 0x30, 0x0a, 0x06, 0x08,
      0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x03 },
    12,
  },
  { SignatureAlgorithm::ecdsa_with_sha256,
    { 0x30, 0x0a, 0x06, 0x08,
      0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02 },
    12,
  },
  { SignatureAlgorithm::ecdsa_with_sha1,
    { 0x30, 0x09, 0x06, 0x07,
      0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x01 },
    11,
  },

  // RSA
  { SignatureAlgorithm::rsa_pkcs1_with_sha512,
    { 0x30, 0x0b, 0x06, 0x09,
      0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0d },
    13,
  },
  { SignatureAlgorithm::rsa_pkcs1_with_sha384,
    { 0x30, 0x0b, 0x06, 0x09,
      0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0c },
    13,
  },
  { SignatureAlgorithm::rsa_pkcs1_with_sha256,
    { 0x30, 0x0b, 0x06, 0x09,
      0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b },
    13,
  },
  { SignatureAlgorithm::rsa_pkcs1_with_sha1,
    { 0x30, 0x0b, 0x06, 0x09,
      0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05 },
    13,
  },

  // DSA
  { SignatureAlgorithm::dsa_with_sha256,
    { 0x30, 0x0b, 0x06, 0x09,
      0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x03, 0x02 },
    13,
  },
  { SignatureAlgorithm::dsa_with_sha1,
    { 0x30, 0x09, 0x06, 0x07,
      0x2a, 0x86, 0x48, 0xce, 0x38, 0x04, 0x03 },
    11,
  },
};

class pkixder_SignatureAlgorithmIdentifier
  : public NSSTest
  , public ::testing::WithParamInterface<
                AlgorithmIdentifierTestInfo<SignatureAlgorithm>>
{
};

TEST_P(pkixder_SignatureAlgorithmIdentifier, Valid)
{
  const AlgorithmIdentifierTestInfo<SignatureAlgorithm>& param(GetParam());

  {
    Input input;
    ASSERT_EQ(Success, input.Init(param.der, param.derLength));
    SignatureAlgorithm alg;
    ASSERT_EQ(Success, SignatureAlgorithmIdentifier(input, alg));
    ASSERT_EQ(param.algorithm, alg);
    ASSERT_EQ(Success, End(input));
  }

  {
    uint8_t derWithNullParam[MAX_ALGORITHM_OID_DER_LENGTH + 2];
    memcpy(derWithNullParam, param.der, param.derLength);
    derWithNullParam[1] += 2; // we're going to expand the value by 2 bytes
    derWithNullParam[param.derLength] = 0x05; // NULL tag
    derWithNullParam[param.derLength + 1] = 0x00; // length zero

    Input input;
    ASSERT_EQ(Success, input.Init(derWithNullParam, param.derLength + 2));
    SignatureAlgorithm alg;
    ASSERT_EQ(Success, SignatureAlgorithmIdentifier(input, alg));
    ASSERT_EQ(param.algorithm, alg);
    ASSERT_EQ(Success, End(input));
  }
}

INSTANTIATE_TEST_CASE_P(pkixder_SignatureAlgorithmIdentifier,
                        pkixder_SignatureAlgorithmIdentifier,
                        testing::ValuesIn(SIGNATURE_ALGORITHM_TEST_INFO));

TEST_F(pkixder_SignatureAlgorithmIdentifier, Invalid_RSA_With_MD5)
{
  // The OID identifies RSA-with-MD5 (1.2.840.113549.1.1.4). It is invalid
  // because no MD5-based signatures algorithms are supported by the parser.
  static const uint8_t DER[] = {
    0x30, 0x0b, 0x06, 0x09,
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x04
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER, sizeof(DER)));
  SignatureAlgorithm alg;
  ASSERT_EQ(Result::ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED,
            SignatureAlgorithmIdentifier(input, alg));
}

TEST_F(pkixder_SignatureAlgorithmIdentifier, Invalid_SignatureAlgorithm_SHA256)
{
  // The OID identifies id-sha256 (2.16.840.1.101.3.4.2.1). It is invalid
  // because SHA-256 is not a signature algorithm.
  static const uint8_t DER[] = {
    0x30, 0x0b, 0x06, 0x09,
    0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER, sizeof(DER)));
  SignatureAlgorithm alg;
  ASSERT_EQ(Result::ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED,
            SignatureAlgorithmIdentifier(input, alg));
}

} // unnamed namespace
