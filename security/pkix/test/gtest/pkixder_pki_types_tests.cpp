/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Copyright 2013 Mozilla Foundation
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
#include <gtest/gtest.h>

#include "pkix/bind.h"
#include "pkixder.h"

using namespace mozilla::pkix::der;

namespace {

class pkixder_pki_types_tests : public ::testing::Test
{
protected:
  virtual void SetUp()
  {
    PR_SetError(0, 0);
  }
};

TEST_F(pkixder_pki_types_tests, AlgorithmIdentifierNoParams)
{
  const uint8_t DER_ALGORITHM_IDENTIFIER_NO_PARAMS[] = {
    0x06, 0x04, 0xde, 0xad, 0xbe, 0xef   // OID
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_ALGORITHM_IDENTIFIER_NO_PARAMS,
                                sizeof DER_ALGORITHM_IDENTIFIER_NO_PARAMS));

  const uint8_t expectedAlgorithmID[] = {
    0xde, 0xad, 0xbe, 0xef
  };

  SECAlgorithmID algorithmID;
  ASSERT_EQ(Success, AlgorithmIdentifier(input, algorithmID));

  ASSERT_EQ(sizeof expectedAlgorithmID, algorithmID.algorithm.len);
  ASSERT_EQ(0, memcmp(algorithmID.algorithm.data, expectedAlgorithmID,
                      sizeof expectedAlgorithmID));

  ASSERT_EQ(0, algorithmID.parameters.len);
  ASSERT_FALSE(algorithmID.parameters.data);
}

TEST_F(pkixder_pki_types_tests, AlgorithmIdentifierNullParams)
{
  const uint8_t DER_ALGORITHM_IDENTIFIER_NULL_PARAMS[] = {
    0x30, 0x08,                         // SEQUENCE
    0x06, 0x04, 0xde, 0xad, 0xbe, 0xef, // OID
    0x05, 0x00                          // NULL
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_ALGORITHM_IDENTIFIER_NULL_PARAMS,
                                sizeof DER_ALGORITHM_IDENTIFIER_NULL_PARAMS));

  uint16_t length;
  ASSERT_EQ(Success, ExpectTagAndGetLength(input, SEQUENCE, length));

  Input nested;
  ASSERT_EQ(Success, input.Skip(length, nested));

  const uint8_t expectedAlgorithmID[] = {
    0xde, 0xad, 0xbe, 0xef
  };

  SECAlgorithmID algorithmID;
  ASSERT_EQ(Success, AlgorithmIdentifier(nested, algorithmID));

  ASSERT_EQ(sizeof expectedAlgorithmID, algorithmID.algorithm.len);
  ASSERT_TRUE(memcmp(algorithmID.algorithm.data, expectedAlgorithmID,
                     sizeof expectedAlgorithmID) == 0);

  ASSERT_EQ((size_t) 0, algorithmID.parameters.len);
  ASSERT_EQ(NULL, algorithmID.parameters.data);
}

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
  ASSERT_EQ(Failure, CertificateSerialNumber(input, item));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(pkixder_pki_types_tests, OptionalVersionV1)
{
  const uint8_t DER_OPTIONAL_VERSION_V1[] = {
    0xa0, 0x03,                   // context specific 0
    0x02, 0x01, 0x00              // INTEGER(0)
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_OPTIONAL_VERSION_V1,
                                sizeof DER_OPTIONAL_VERSION_V1));

  uint8_t version = 99;
  // TODO(bug 982783): An explicit value of 1 is not allowed, because it is not
  // the shortest possible encoding!
  ASSERT_EQ(Success, OptionalVersion(input, version));
  ASSERT_EQ(v1, version);
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

  uint8_t version = 99;
  ASSERT_EQ(Success, OptionalVersion(input, version));
  ASSERT_EQ(v2, version);
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

  uint8_t version = 99;
  ASSERT_EQ(Success, OptionalVersion(input, version));
  ASSERT_EQ(v3, version);
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

  uint8_t version = 99;
  ASSERT_EQ(Success, OptionalVersion(input, version));
  ASSERT_EQ(0x42, version);
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

  uint8_t version = 99;
  ASSERT_EQ(Failure, OptionalVersion(input, version));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(pkixder_pki_types_tests, OptionalVersionMissing)
{
  const uint8_t DER_OPTIONAL_VERSION_MISSING[] = {
    0x02, 0x11, 0x22              // INTEGER
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_OPTIONAL_VERSION_MISSING,
                                sizeof DER_OPTIONAL_VERSION_MISSING));

  uint8_t version = 99;
  ASSERT_EQ(Success, OptionalVersion(input, version));
  ASSERT_EQ(v1, version);
}
} // unnamed namespace
