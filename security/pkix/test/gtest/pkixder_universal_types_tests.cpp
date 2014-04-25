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

class pkixder_universal_types_tests : public ::testing::Test
{
protected:
  virtual void SetUp()
  {
    PR_SetError(0, 0);
  }
};

TEST_F(pkixder_universal_types_tests, BooleanTrue01)
{
  const uint8_t DER_BOOLEAN_TRUE_01[] = {
    0x01,                       // INTEGER
    0x01,                       // length
    0x01                        // invalid
  };

  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_BOOLEAN_TRUE_01, sizeof DER_BOOLEAN_TRUE_01));

  bool value = false;
  ASSERT_EQ(Failure, Boolean(input, value));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(pkixder_universal_types_tests, BooleanTrue42)
{
  const uint8_t DER_BOOLEAN_TRUE_42[] = {
    0x01,                       // INTEGER
    0x01,                       // length
    0x42                        // invalid
  };

  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_BOOLEAN_TRUE_42, sizeof DER_BOOLEAN_TRUE_42));

  bool value = false;
  ASSERT_EQ(Failure, Boolean(input, value));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(pkixder_universal_types_tests, BooleanTrueFF)
{
  const uint8_t DER_BOOLEAN_TRUE_FF[] = {
    0x01,                       // INTEGER
    0x01,                       // length
    0xff                        // true
  };

  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_BOOLEAN_TRUE_FF, sizeof DER_BOOLEAN_TRUE_FF));

  bool value = false;
  ASSERT_EQ(Success, Boolean(input, value));
  ASSERT_EQ(true, value);
}

TEST_F(pkixder_universal_types_tests, BooleanFalse)
{
  const uint8_t DER_BOOLEAN_FALSE[] = {
    0x01,                       // INTEGER
    0x01,                       // length
    0x00                        // false
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_BOOLEAN_FALSE, sizeof DER_BOOLEAN_FALSE));

  bool value = true;
  ASSERT_EQ(Success, Boolean(input, value));
  ASSERT_EQ(false, value);
}

TEST_F(pkixder_universal_types_tests, BooleanInvalidLength)
{
  const uint8_t DER_BOOLEAN_INVALID_LENGTH[] = {
    0x01,                       // INTEGER
    0x02,                       // length
    0x42, 0x42                  // invalid
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_BOOLEAN_INVALID_LENGTH,
                                sizeof DER_BOOLEAN_INVALID_LENGTH));

  bool value = true;
  ASSERT_EQ(Failure, Boolean(input, value));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(pkixder_universal_types_tests, BooleanInvalidZeroLength)
{
  const uint8_t DER_BOOLEAN_INVALID_ZERO_LENGTH[] = {
    0x01,                       // INTEGER
    0x00                        // length
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_BOOLEAN_INVALID_ZERO_LENGTH,
                                sizeof DER_BOOLEAN_INVALID_ZERO_LENGTH));

  bool value = true;
  ASSERT_EQ(Failure, Boolean(input, value));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(pkixder_universal_types_tests, Enumerated)
{
  const uint8_t DER_ENUMERATED[] = {
    0x0a,                       // INTEGER
    0x01,                       // length
    0x42                        // value
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_ENUMERATED, sizeof DER_ENUMERATED));

  uint8_t value = 0;
  ASSERT_EQ(Success, Enumerated(input, value));
  ASSERT_EQ(0x42, value);
}

TEST_F(pkixder_universal_types_tests, EnumeratedNotShortestPossibleDER)
{
  const uint8_t DER_ENUMERATED[] = {
    0x0a,                       // INTEGER
    0x02,                       // length
    0x00, 0x01                  // value
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_ENUMERATED, sizeof DER_ENUMERATED));
  uint8_t value = 0;
  ASSERT_EQ(Failure, Enumerated(input, value));
}

TEST_F(pkixder_universal_types_tests, EnumeratedOutOfAcceptedRange)
{
  // Although this is a valid ENUMERATED value according to ASN.1, we
  // intentionally don't support these large values because there are no
  // ENUMERATED values in X.509 certs or OCSP this large, and we're trying to
  // keep the parser simple and fast.
  const uint8_t DER_ENUMERATED_INVALID_LENGTH[] = {
    0x0a,                       // INTEGER
    0x02,                       // length
    0x12, 0x34                  // value
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_ENUMERATED_INVALID_LENGTH,
                                sizeof DER_ENUMERATED_INVALID_LENGTH));

  uint8_t value = 0;
  ASSERT_EQ(Failure, Enumerated(input, value));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(pkixder_universal_types_tests, EnumeratedInvalidZeroLength)
{
  const uint8_t DER_ENUMERATED_INVALID_ZERO_LENGTH[] = {
    0x0a,                       // INTEGER
    0x00                        // length
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_ENUMERATED_INVALID_ZERO_LENGTH,
                                sizeof DER_ENUMERATED_INVALID_ZERO_LENGTH));

  uint8_t value = 0;
  ASSERT_EQ(Failure, Enumerated(input, value));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

// TODO: Test all acceptable timestamp formats. Find out what formats
// are being used by looking at large collection of certs.
TEST_F(pkixder_universal_types_tests, GeneralizedTimeOffset)
{
  const uint8_t DER_GENERALIZED_TIME_OFFSET[] = {
    0x18,
    19,
    '1', '9', '9', '1', '0', '5', '0', '6', '1', '6', '4', '5', '4', '0', '-',
    '0', '7', '0', '0'
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_GENERALIZED_TIME_OFFSET,
                                sizeof DER_GENERALIZED_TIME_OFFSET));

  PRTime value = 0;
  ASSERT_EQ(Success, GeneralizedTime(input, value));
  ASSERT_EQ(673573540000000, value);
}

TEST_F(pkixder_universal_types_tests, GeneralizedTimeGMT)
{
  const uint8_t DER_GENERALIZED_TIME_GMT[] = {
    0x18,
    15,
    '1', '9', '9', '1', '0', '5', '0', '6', '1', '6', '4', '5', '4', '0', 'Z'
  };

  Input input;
  Result rv1 = input.Init(DER_GENERALIZED_TIME_GMT,
                          sizeof DER_GENERALIZED_TIME_GMT);
  ASSERT_EQ(Success, rv1);

  PRTime value = 0;
  Result rv2 = GeneralizedTime(input, value);
  ASSERT_EQ(Success, rv2);
  ASSERT_EQ(673548340000000, value);
}

TEST_F(pkixder_universal_types_tests, GeneralizedTimeInvalidZeroLength)
{
  const uint8_t DER_GENERALIZED_TIME_INVALID_ZERO_LENGTH[] = {
    0x18,
    0x00
  };

  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_GENERALIZED_TIME_INVALID_ZERO_LENGTH,
                       sizeof DER_GENERALIZED_TIME_INVALID_ZERO_LENGTH));

  PRTime value = 0;
  ASSERT_EQ(Failure, GeneralizedTime(input, value));
  ASSERT_EQ(SEC_ERROR_INVALID_TIME, PR_GetError());
}

TEST_F(pkixder_universal_types_tests, Integer)
{
  const uint8_t DER_INTEGUR[] = {
    0x02,                       // INTEGER
    0x04,                       // length
    0x11, 0x22, 0x33, 0x44      // 0x11223344
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_INTEGUR, sizeof DER_INTEGUR));

  const uint8_t expectedItemData[] = { 0x11, 0x22, 0x33, 0x44 };

  SECItem item;
  memset(&item, 0x00, sizeof item);

  ASSERT_EQ(Success, Integer(input, item));

  ASSERT_EQ(siBuffer, item.type);
  ASSERT_EQ((size_t) 4, item.len);
  ASSERT_TRUE(item.data);
  ASSERT_EQ(0, memcmp(item.data, expectedItemData, sizeof expectedItemData));
}

TEST_F(pkixder_universal_types_tests, OneByte)
{
  const uint8_t DER_INTEGUR[] = {
    0x02,                       // INTEGER
    0x01,                       // length
    0x11                        // 0x11
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_INTEGUR, sizeof DER_INTEGUR));

  const uint8_t expectedItemData[] = { 0x11 };

  SECItem item;
  memset(&item, 0x00, sizeof item);

  ASSERT_EQ(Success, Integer(input, item));

  ASSERT_EQ(siBuffer, item.type);
  ASSERT_EQ((size_t) 1, item.len);
  ASSERT_TRUE(item.data);
  ASSERT_EQ(0, memcmp(item.data, expectedItemData, sizeof expectedItemData));
}

TEST_F(pkixder_universal_types_tests, IntegerTruncated)
{
  const uint8_t DER_INTEGER_TRUNCATED[] = {
    0x02,                       // INTEGER
    0x04,                       // length
    0x11, 0x22                  // 0x1122
    // MISSING DATA HERE
  };

  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_INTEGER_TRUNCATED, sizeof DER_INTEGER_TRUNCATED));

  SECItem item;
  memset(&item, 0x00, sizeof item);

  ASSERT_EQ(Failure, Integer(input, item));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());

  ASSERT_EQ(0, item.type);
  ASSERT_EQ(0, item.len);
}

TEST_F(pkixder_universal_types_tests, IntegerZeroLength)
{
  const uint8_t DER_INTEGER_ZERO_LENGTH[] = {
    0x02,                       // INTEGER
    0x00                        // length
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_INTEGER_ZERO_LENGTH,
                                sizeof DER_INTEGER_ZERO_LENGTH));

  SECItem item;
  ASSERT_EQ(Failure, Integer(input, item));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(pkixder_universal_types_tests, IntegerOverlyLong1)
{
  const uint8_t DER_INTEGER_OVERLY_LONG1[] = {
    0x02,                       // INTEGER
    0x02,                       // length
    0x00, 0x01                  //
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_INTEGER_OVERLY_LONG1,
                                sizeof DER_INTEGER_OVERLY_LONG1));

  SECItem item;
  ASSERT_EQ(Failure, Integer(input, item));
}

TEST_F(pkixder_universal_types_tests, IntegerOverlyLong2)
{
  const uint8_t DER_INTEGER_OVERLY_LONG2[] = {
    0x02,                       // INTEGER
    0x02,                       // length
    0xff, 0x80                  //
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_INTEGER_OVERLY_LONG2,
                                sizeof DER_INTEGER_OVERLY_LONG2));

  SECItem item;
  ASSERT_EQ(Failure, Integer(input, item));
}

TEST_F(pkixder_universal_types_tests, Null)
{
  const uint8_t DER_NUL[] = {
    0x05,
    0x00
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_NUL, sizeof DER_NUL));
  ASSERT_EQ(Success, Null(input));
}

TEST_F(pkixder_universal_types_tests, NullWithBadLength)
{
  const uint8_t DER_NULL_BAD_LENGTH[] = {
    0x05,
    0x01,
    0x00
  };

  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_NULL_BAD_LENGTH, sizeof DER_NULL_BAD_LENGTH));

  ASSERT_EQ(Failure, Null(input));
}

TEST_F(pkixder_universal_types_tests, OID)
{
  const uint8_t DER_VALID_OID[] = {
    0x06,
    0x09,
    0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x01
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_VALID_OID, sizeof DER_VALID_OID));

  const uint8_t expectedOID[] = {
    0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x01
  };

  ASSERT_EQ(Success, OID(input, expectedOID));
}

} // unnamed namespace
