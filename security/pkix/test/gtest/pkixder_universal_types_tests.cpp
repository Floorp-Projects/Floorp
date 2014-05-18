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

#include <limits>
#include <vector>
#include <gtest/gtest.h>

#include "pkix/bind.h"
#include "pkixder.h"
#include "stdint.h"

using namespace mozilla::pkix::der;
using namespace std;

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
    0x01,                       // BOOLEAN
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
    0x01,                       // BOOLEAN
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

static const uint8_t DER_BOOLEAN_TRUE[] = {
  0x01,                       // BOOLEAN
  0x01,                       // length
  0xff                        // true
};

TEST_F(pkixder_universal_types_tests, BooleanTrueFF)
{
  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_BOOLEAN_TRUE, sizeof DER_BOOLEAN_TRUE));

  bool value = false;
  ASSERT_EQ(Success, Boolean(input, value));
  ASSERT_TRUE(value);
}

TEST_F(pkixder_universal_types_tests, BooleanFalse)
{
  const uint8_t DER_BOOLEAN_FALSE[] = {
    0x01,                       // BOOLEAN
    0x01,                       // length
    0x00                        // false
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_BOOLEAN_FALSE, sizeof DER_BOOLEAN_FALSE));

  bool value = true;
  ASSERT_EQ(Success, Boolean(input, value));
  ASSERT_FALSE(value);
}

TEST_F(pkixder_universal_types_tests, BooleanInvalidLength)
{
  const uint8_t DER_BOOLEAN_INVALID_LENGTH[] = {
    0x01,                       // BOOLEAN
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
    0x01,                       // BOOLEAN
    0x00                        // length
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER_BOOLEAN_INVALID_ZERO_LENGTH,
                                sizeof DER_BOOLEAN_INVALID_ZERO_LENGTH));

  bool value = true;
  ASSERT_EQ(Failure, Boolean(input, value));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

// OptionalBoolean implements decoding of OPTIONAL BOOLEAN DEFAULT FALSE.
// If the field is present, it must be a valid encoding of a BOOLEAN with
// value TRUE. If the field is not present, it defaults to FALSE. For
// compatibility reasons, OptionalBoolean can be told to accept an encoding
// where the field is present with value FALSE (this is technically not a
// valid DER encoding).
TEST_F(pkixder_universal_types_tests, OptionalBooleanValidEncodings)
{
  const uint8_t DER_OPTIONAL_BOOLEAN_PRESENT_TRUE[] = {
    0x01,                       // BOOLEAN
    0x01,                       // length
    0xff                        // true
  };

  Input input1;
  ASSERT_EQ(Success, input1.Init(DER_OPTIONAL_BOOLEAN_PRESENT_TRUE,
                                 sizeof DER_OPTIONAL_BOOLEAN_PRESENT_TRUE));
  bool value = false;
  ASSERT_EQ(Success, OptionalBoolean(input1, false, value)) <<
    "Should accept the only valid encoding of a present OPTIONAL BOOLEAN";
  ASSERT_TRUE(value);
  ASSERT_TRUE(input1.AtEnd());

  // The OPTIONAL BOOLEAN is omitted in this data.
  const uint8_t DER_INTEGER_05[] = {
    0x02,                       // INTEGER
    0x01,                       // length
    0x05
  };

  Input input2;
  ASSERT_EQ(Success, input2.Init(DER_INTEGER_05, sizeof DER_INTEGER_05));
  value = true;
  ASSERT_EQ(Success, OptionalBoolean(input2, false, value)) <<
    "Should accept a valid encoding of an omitted OPTIONAL BOOLEAN";
  ASSERT_FALSE(value);
  ASSERT_FALSE(input2.AtEnd());

  Input input3;
  ASSERT_EQ(Success, input3.Init(reinterpret_cast<const uint8_t*>(""), 0));
  value = true;
  ASSERT_EQ(Success, OptionalBoolean(input3, false, value)) <<
    "Should accept another valid encoding of an omitted OPTIONAL BOOLEAN";
  ASSERT_FALSE(value);
  ASSERT_TRUE(input3.AtEnd());
}

TEST_F(pkixder_universal_types_tests, OptionalBooleanInvalidEncodings)
{
  const uint8_t DER_OPTIONAL_BOOLEAN_PRESENT_FALSE[] = {
    0x01,                       // BOOLEAN
    0x01,                       // length
    0x00                        // false
  };

  Input input1;
  ASSERT_EQ(Success, input1.Init(DER_OPTIONAL_BOOLEAN_PRESENT_FALSE,
                                 sizeof DER_OPTIONAL_BOOLEAN_PRESENT_FALSE));
  bool value;
  // If the second parameter to OptionalBoolean is false, invalid encodings
  // that include the field even when it is the DEFAULT FALSE are rejected.
  bool allowInvalidEncodings = false;
  ASSERT_EQ(Failure, OptionalBoolean(input1, allowInvalidEncodings, value)) <<
    "Should reject an invalid encoding of present OPTIONAL BOOLEAN";
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());

  Input input2;
  ASSERT_EQ(Success, input2.Init(DER_OPTIONAL_BOOLEAN_PRESENT_FALSE,
                                 sizeof DER_OPTIONAL_BOOLEAN_PRESENT_FALSE));
  value = true;
  // If the second parameter to OptionalBoolean is true, invalid encodings
  // that include the field even when it is the DEFAULT FALSE are accepted.
  allowInvalidEncodings = true;
  ASSERT_EQ(Success, OptionalBoolean(input2, allowInvalidEncodings, value)) <<
    "Should now accept an invalid encoding of present OPTIONAL BOOLEAN";
  ASSERT_FALSE(value);
  ASSERT_TRUE(input2.AtEnd());

  const uint8_t DER_OPTIONAL_BOOLEAN_PRESENT_42[] = {
    0x01,                       // BOOLEAN
    0x01,                       // length
    0x42                        // (invalid value for a BOOLEAN)
  };

  Input input3;
  ASSERT_EQ(Success, input3.Init(DER_OPTIONAL_BOOLEAN_PRESENT_42,
                                 sizeof DER_OPTIONAL_BOOLEAN_PRESENT_42));
  // Even with the second parameter to OptionalBoolean as true, encodings
  // of BOOLEAN that are invalid altogether are rejected.
  ASSERT_EQ(Failure, OptionalBoolean(input3, allowInvalidEncodings, value)) <<
    "Should reject another invalid encoding of present OPTIONAL BOOLEAN";
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(pkixder_universal_types_tests, Enumerated)
{
  const uint8_t DER_ENUMERATED[] = {
    0x0a,                       // ENUMERATED
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
    0x0a,                       // ENUMERATED
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
    0x0a,                       // ENUMERATED
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
    0x0a,                       // ENUMERATED
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

TEST_F(pkixder_universal_types_tests, Integer_0_127)
{
  for (uint8_t i = 0; i <= 127; ++i) {
    const uint8_t DER[] = {
      0x02, // INTEGER
      0x01, // length
      i,    // value
    };

    Input input;
    ASSERT_EQ(Success, input.Init(DER, sizeof DER));

    uint8_t value = i + 1; // initialize with a value that is NOT i.
    ASSERT_EQ(Success, Integer(input, value));
    ASSERT_EQ(i, value);
  }
}

TEST_F(pkixder_universal_types_tests, Integer_Negative1)
{
  // This is a valid integer value but our integer parser cannot parse
  // negative values.

  static const uint8_t DER[] = {
    0x02, // INTEGER
    0x01, // length
    0xff, // -1 (two's complement)
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER, sizeof DER));

  uint8_t value;
  ASSERT_EQ(Failure, Integer(input, value));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(pkixder_universal_types_tests, Integer_Negative128)
{
  // This is a valid integer value but our integer parser cannot parse
  // negative values.

  static const uint8_t DER[] = {
    0x02, // INTEGER
    0x01, // length
    0x80, // -128 (two's complement)
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER, sizeof DER));

  uint8_t value;
  ASSERT_EQ(Failure, Integer(input, value));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(pkixder_universal_types_tests, Integer_128)
{
  // This is a valid integer value but our integer parser cannot parse
  // values that require more than one byte to encode.

  static const uint8_t DER[] = {
    0x02, // INTEGER
    0x02, // length
    0x00, 0x80 // 128
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER, sizeof DER));

  uint8_t value;
  ASSERT_EQ(Failure, Integer(input, value));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(pkixder_universal_types_tests, Integer11223344)
{
  // This is a valid integer value but our integer parser cannot parse
  // values that require more than one byte to be encoded.

  static const uint8_t DER[] = {
    0x02,                       // INTEGER
    0x04,                       // length
    0x11, 0x22, 0x33, 0x44      // 0x11223344
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER, sizeof DER));

  uint8_t value;
  ASSERT_EQ(Failure, Integer(input, value));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(pkixder_universal_types_tests, IntegerTruncatedOneByte)
{
  const uint8_t DER_INTEGER_TRUNCATED[] = {
    0x02,                       // INTEGER
    0x01,                       // length
    // MISSING DATA HERE
  };

  Input input;
  ASSERT_EQ(Success,
            input.Init(DER_INTEGER_TRUNCATED, sizeof DER_INTEGER_TRUNCATED));

  uint8_t value;
  ASSERT_EQ(Failure, Integer(input, value));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(pkixder_universal_types_tests, IntegerTruncatedLarge)
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

  uint8_t value;
  ASSERT_EQ(Failure, Integer(input, value));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
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
  uint8_t value;
  ASSERT_EQ(Failure, Integer(input, value));
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
  uint8_t value;
  ASSERT_EQ(Failure, Integer(input, value));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
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
  uint8_t value;
  ASSERT_EQ(Failure, Integer(input, value));
  ASSERT_EQ(SEC_ERROR_BAD_DER, PR_GetError());
}

TEST_F(pkixder_universal_types_tests, OptionalIntegerSupportedDefault)
{
  // The input is a BOOLEAN and not INTEGER for the input so we'll not parse
  // anything and instead use the default value.
  Input input;
  ASSERT_EQ(Success, input.Init(DER_BOOLEAN_TRUE, sizeof DER_BOOLEAN_TRUE));
  long value = 1;
  ASSERT_EQ(Success, OptionalInteger(input, -1, value));
  ASSERT_EQ(-1, value);
  bool boolValue;
  ASSERT_EQ(Success, Boolean(input, boolValue));
}

TEST_F(pkixder_universal_types_tests, OptionalIntegerUnsupportedDefault)
{
  // The same as the previous test, except with an unsupported default value
  // passed in.
  Input input;
  ASSERT_EQ(Success, input.Init(DER_BOOLEAN_TRUE, sizeof DER_BOOLEAN_TRUE));
  long value;
  ASSERT_EQ(Failure, OptionalInteger(input, 0, value));
  ASSERT_EQ(SEC_ERROR_INVALID_ARGS, PR_GetError());
}

TEST_F(pkixder_universal_types_tests, OptionalIntegerSupportedDefaultAtEnd)
{
  static const uint8_t dummy = 1;

  Input input;
  ASSERT_EQ(Success, input.Init(&dummy, 0));
  long value = 1;
  ASSERT_EQ(Success, OptionalInteger(input, -1, value));
  ASSERT_EQ(-1, value);
}

TEST_F(pkixder_universal_types_tests, OptionalIntegerNonDefaultValue)
{
  static const uint8_t DER[] = {
    0x02, // INTEGER
    0x01, // length
    0x00
  };

  Input input;
  ASSERT_EQ(Success, input.Init(DER, sizeof DER));
  long value = 2;
  ASSERT_EQ(Success, OptionalInteger(input, -1, value));
  ASSERT_EQ(0, value);
  ASSERT_TRUE(input.AtEnd());
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
