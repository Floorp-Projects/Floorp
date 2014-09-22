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
#include "pkixtestutil.h"
#include "stdint.h"

using namespace mozilla::pkix;
using namespace mozilla::pkix::der;
using namespace mozilla::pkix::test;
using namespace std;

namespace {

class pkixder_universal_types_tests : public ::testing::Test { };

TEST_F(pkixder_universal_types_tests, BooleanTrue01)
{
  const uint8_t DER_BOOLEAN_TRUE_01[] = {
    0x01,                       // BOOLEAN
    0x01,                       // length
    0x01                        // invalid
  };
  Input input(DER_BOOLEAN_TRUE_01);
  Reader reader(input);
  bool value = false;
  ASSERT_EQ(Result::ERROR_BAD_DER, Boolean(reader, value));
}

TEST_F(pkixder_universal_types_tests, BooleanTrue42)
{
  const uint8_t DER_BOOLEAN_TRUE_42[] = {
    0x01,                       // BOOLEAN
    0x01,                       // length
    0x42                        // invalid
  };
  Input input(DER_BOOLEAN_TRUE_42);
  Reader reader(input);
  bool value = false;
  ASSERT_EQ(Result::ERROR_BAD_DER, Boolean(reader, value));
}

static const uint8_t DER_BOOLEAN_TRUE[] = {
  0x01,                       // BOOLEAN
  0x01,                       // length
  0xff                        // true
};

TEST_F(pkixder_universal_types_tests, BooleanTrueFF)
{
  Input input(DER_BOOLEAN_TRUE);
  Reader reader(input);
  bool value = false;
  ASSERT_EQ(Success, Boolean(reader, value));
  ASSERT_TRUE(value);
}

TEST_F(pkixder_universal_types_tests, BooleanFalse)
{
  const uint8_t DER_BOOLEAN_FALSE[] = {
    0x01,                       // BOOLEAN
    0x01,                       // length
    0x00                        // false
  };
  Input input(DER_BOOLEAN_FALSE);
  Reader reader(input);

  bool value = true;
  ASSERT_EQ(Success, Boolean(reader, value));
  ASSERT_FALSE(value);
}

TEST_F(pkixder_universal_types_tests, BooleanInvalidLength)
{
  const uint8_t DER_BOOLEAN_INVALID_LENGTH[] = {
    0x01,                       // BOOLEAN
    0x02,                       // length
    0x42, 0x42                  // invalid
  };
  Input input(DER_BOOLEAN_INVALID_LENGTH);
  Reader reader(input);

  bool value = true;
  ASSERT_EQ(Result::ERROR_BAD_DER, Boolean(reader, value));
}

TEST_F(pkixder_universal_types_tests, BooleanInvalidZeroLength)
{
  const uint8_t DER_BOOLEAN_INVALID_ZERO_LENGTH[] = {
    0x01,                       // BOOLEAN
    0x00                        // length
  };
  Input input(DER_BOOLEAN_INVALID_ZERO_LENGTH);
  Reader reader(input);

  bool value = true;
  ASSERT_EQ(Result::ERROR_BAD_DER, Boolean(reader, value));
}

// OptionalBoolean implements decoding of OPTIONAL BOOLEAN DEFAULT FALSE.
// If the field is present, it must be a valid encoding of a BOOLEAN with
// value TRUE. If the field is not present, it defaults to FALSE. For
// compatibility reasons, OptionalBoolean also accepts encodings where the field
// is present with value FALSE (this is technically not a valid DER encoding).
TEST_F(pkixder_universal_types_tests, OptionalBooleanValidEncodings)
{
  {
    const uint8_t DER_OPTIONAL_BOOLEAN_PRESENT_TRUE[] = {
      0x01,                       // BOOLEAN
      0x01,                       // length
      0xff                        // true
    };
    Input input(DER_OPTIONAL_BOOLEAN_PRESENT_TRUE);
    Reader reader(input);
    bool value = false;
    ASSERT_EQ(Success, OptionalBoolean(reader, value)) <<
      "Should accept the only valid encoding of a present OPTIONAL BOOLEAN";
    ASSERT_TRUE(value);
    ASSERT_TRUE(reader.AtEnd());
  }

  {
    // The OPTIONAL BOOLEAN is omitted in this data.
    const uint8_t DER_INTEGER_05[] = {
      0x02,                       // INTEGER
      0x01,                       // length
      0x05
    };
    Input input(DER_INTEGER_05);
    Reader reader(input);
    bool value = true;
    ASSERT_EQ(Success, OptionalBoolean(reader, value)) <<
      "Should accept a valid encoding of an omitted OPTIONAL BOOLEAN";
    ASSERT_FALSE(value);
    ASSERT_FALSE(reader.AtEnd());
  }

  {
    Input input;
    ASSERT_EQ(Success, input.Init(reinterpret_cast<const uint8_t*>(""), 0));
    Reader reader(input);
    bool value = true;
    ASSERT_EQ(Success, OptionalBoolean(reader, value)) <<
      "Should accept another valid encoding of an omitted OPTIONAL BOOLEAN";
    ASSERT_FALSE(value);
    ASSERT_TRUE(reader.AtEnd());
  }
}

TEST_F(pkixder_universal_types_tests, OptionalBooleanInvalidEncodings)
{
  const uint8_t DER_OPTIONAL_BOOLEAN_PRESENT_FALSE[] = {
    0x01,                       // BOOLEAN
    0x01,                       // length
    0x00                        // false
  };

  {
    Input input(DER_OPTIONAL_BOOLEAN_PRESENT_FALSE);
    Reader reader(input);
    bool value = true;
    ASSERT_EQ(Success, OptionalBoolean(reader, value)) <<
      "Should accept an invalid, default-value encoding of OPTIONAL BOOLEAN";
    ASSERT_FALSE(value);
    ASSERT_TRUE(reader.AtEnd());
  }

  const uint8_t DER_OPTIONAL_BOOLEAN_PRESENT_42[] = {
    0x01,                       // BOOLEAN
    0x01,                       // length
    0x42                        // (invalid value for a BOOLEAN)
  };

  {
    Input input(DER_OPTIONAL_BOOLEAN_PRESENT_42);
    Reader reader(input);
    bool value;
    ASSERT_EQ(Result::ERROR_BAD_DER, OptionalBoolean(reader, value)) <<
      "Should reject an invalid-valued encoding of OPTIONAL BOOLEAN";
  }
}

TEST_F(pkixder_universal_types_tests, Enumerated)
{
  const uint8_t DER_ENUMERATED[] = {
    0x0a,                       // ENUMERATED
    0x01,                       // length
    0x42                        // value
  };
  Input input(DER_ENUMERATED);
  Reader reader(input);

  uint8_t value = 0;
  ASSERT_EQ(Success, Enumerated(reader, value));
  ASSERT_EQ(0x42, value);
}

TEST_F(pkixder_universal_types_tests, EnumeratedNotShortestPossibleDER)
{
  const uint8_t DER_ENUMERATED[] = {
    0x0a,                       // ENUMERATED
    0x02,                       // length
    0x00, 0x01                  // value
  };
  Input input(DER_ENUMERATED);
  Reader reader(input);

  uint8_t value = 0;
  ASSERT_EQ(Result::ERROR_BAD_DER, Enumerated(reader, value));
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
  Input input(DER_ENUMERATED_INVALID_LENGTH);
  Reader reader(input);

  uint8_t value = 0;
  ASSERT_EQ(Result::ERROR_BAD_DER, Enumerated(reader, value));
}

TEST_F(pkixder_universal_types_tests, EnumeratedInvalidZeroLength)
{
  const uint8_t DER_ENUMERATED_INVALID_ZERO_LENGTH[] = {
    0x0a,                       // ENUMERATED
    0x00                        // length
  };
  Input input(DER_ENUMERATED_INVALID_ZERO_LENGTH);
  Reader reader(input);

  uint8_t value = 0;
  ASSERT_EQ(Result::ERROR_BAD_DER, Enumerated(reader, value));
}

////////////////////////////////////////
// GeneralizedTime and TimeChoice
//
// From RFC 5280 section 4.1.2.5.2
//
//   For the purposes of this profile, GeneralizedTime values MUST be
//   expressed in Greenwich Mean Time (Zulu) and MUST include seconds
//   (i.e., times are YYYYMMDDHHMMSSZ), even where the number of seconds
//   is zero.  GeneralizedTime values MUST NOT include fractional seconds.
//
// And from from RFC 6960 (OCSP) section 4.2.2.1:
//
//   Responses can contain four times -- thisUpdate, nextUpdate,
//   producedAt, and revocationTime.  The semantics of these fields are
//   defined in Section 2.4.  The format for GeneralizedTime is as
//   specified in Section 4.1.2.5.2 of [RFC5280].
//
// So while we can could accept other ASN1 (ITU-T X.680) encodings for
// GeneralizedTime we should not accept them, and breaking reading of these
// other encodings is actually encouraged.

// e.g. TWO_CHARS(53) => '5', '3'
#define TWO_CHARS(t) static_cast<uint8_t>('0' + ((t) / 10u)), \
                     static_cast<uint8_t>('0' + ((t) % 10u))

// Calls TimeChoice on the UTCTime variant of the given generalized time.
template <uint16_t LENGTH>
Result
TimeChoiceForEquivalentUTCTime(const uint8_t (&generalizedTimeDER)[LENGTH],
                               /*out*/ Time& value)
{
  static_assert(LENGTH >= 4,
                "TimeChoiceForEquivalentUTCTime input too small");
  uint8_t utcTimeDER[LENGTH - 2];
  utcTimeDER[0] = 0x17; // tag UTCTime
  utcTimeDER[1] = LENGTH - 1/*tag*/ - 1/*value*/ - 2/*century*/;
  // Copy the value except for the first two digits of the year
  for (size_t i = 2; i < LENGTH - 2; ++i) {
    utcTimeDER[i] = generalizedTimeDER[i + 2];
  }

  Input input(utcTimeDER);
  Reader reader(input);
  return TimeChoice(reader, value);
}

template <uint16_t LENGTH>
void
ExpectGoodTime(Time expectedValue,
               const uint8_t (&generalizedTimeDER)[LENGTH])
{
  // GeneralizedTime
  {
    Input input(generalizedTimeDER);
    Reader reader(input);
    Time value(Time::uninitialized);
    ASSERT_EQ(Success, GeneralizedTime(reader, value));
    EXPECT_EQ(expectedValue, value);
  }

  // TimeChoice: GeneralizedTime
  {
    Input input(generalizedTimeDER);
    Reader reader(input);
    Time value(Time::uninitialized);
    ASSERT_EQ(Success, TimeChoice(reader, value));
    EXPECT_EQ(expectedValue, value);
  }

  // TimeChoice: UTCTime
  {
    Time value(Time::uninitialized);
    ASSERT_EQ(Success,
              TimeChoiceForEquivalentUTCTime(generalizedTimeDER, value));
    EXPECT_EQ(expectedValue, value);
  }
}

template <uint16_t LENGTH>
void
ExpectBadTime(const uint8_t (&generalizedTimeDER)[LENGTH])
{
  // GeneralizedTime
  {
    Input input(generalizedTimeDER);
    Reader reader(input);
    Time value(Time::uninitialized);
    ASSERT_EQ(Result::ERROR_INVALID_TIME, GeneralizedTime(reader, value));
  }

  // TimeChoice: GeneralizedTime
  {
    Input input(generalizedTimeDER);
    Reader reader(input);
    Time value(Time::uninitialized);
    ASSERT_EQ(Result::ERROR_INVALID_TIME, TimeChoice(reader, value));
  }

  // TimeChoice: UTCTime
  {
    Time value(Time::uninitialized);
    ASSERT_EQ(Result::ERROR_INVALID_TIME,
              TimeChoiceForEquivalentUTCTime(generalizedTimeDER, value));
  }
}

// Control value: a valid time
TEST_F(pkixder_universal_types_tests, ValidControl)
{
  const uint8_t GT_DER[] = {
    0x18,                           // Generalized Time
    15,                             // Length = 15
    '1', '9', '9', '1', '0', '5', '0', '6', '1', '6', '4', '5', '4', '0', 'Z'
  };
  ExpectGoodTime(YMDHMS(1991, 5, 6, 16, 45, 40), GT_DER);
}

TEST_F(pkixder_universal_types_tests, TimeTimeZoneOffset)
{
  const uint8_t DER_GENERALIZED_TIME_OFFSET[] = {
    0x18,                           // Generalized Time
    19,                             // Length = 19
    '1', '9', '9', '1', '0', '5', '0', '6', '1', '6', '4', '5', '4', '0', '-',
    '0', '7', '0', '0'
  };
  ExpectBadTime(DER_GENERALIZED_TIME_OFFSET);
}

TEST_F(pkixder_universal_types_tests, TimeInvalidZeroLength)
{
  const uint8_t DER_GENERALIZED_TIME_INVALID_ZERO_LENGTH[] = {
    0x18,                           // GeneralizedTime
    0x00                            // Length = 0
  };

  Time value(Time::uninitialized);

  // GeneralizedTime
  Input gtBuf(DER_GENERALIZED_TIME_INVALID_ZERO_LENGTH);
  Reader gt(gtBuf);
  ASSERT_EQ(Result::ERROR_INVALID_TIME, GeneralizedTime(gt, value));

  // TimeChoice: GeneralizedTime
  Input tc_gt_buf(DER_GENERALIZED_TIME_INVALID_ZERO_LENGTH);
  Reader tc_gt(tc_gt_buf);
  ASSERT_EQ(Result::ERROR_INVALID_TIME, TimeChoice(tc_gt, value));

  // TimeChoice: UTCTime
  const uint8_t DER_UTCTIME_INVALID_ZERO_LENGTH[] = {
    0x17, // UTCTime
    0x00  // Length = 0
  };
  Input tc_utc_buf(DER_UTCTIME_INVALID_ZERO_LENGTH);
  Reader tc_utc(tc_utc_buf);
  ASSERT_EQ(Result::ERROR_INVALID_TIME, TimeChoice(tc_utc, value));
}

// A non zulu time should fail
TEST_F(pkixder_universal_types_tests, TimeInvalidLocal)
{
  const uint8_t DER_GENERALIZED_TIME_INVALID_LOCAL[] = {
    0x18,                           // Generalized Time
    14,                             // Length = 14
    '1', '9', '9', '1', '0', '5', '0', '6', '1', '6', '4', '5', '4', '0'
  };
  ExpectBadTime(DER_GENERALIZED_TIME_INVALID_LOCAL);
}

// A time missing seconds and zulu should fail
TEST_F(pkixder_universal_types_tests, TimeInvalidTruncated)
{
  const uint8_t DER_GENERALIZED_TIME_INVALID_TRUNCATED[] = {
    0x18,                           // Generalized Time
    12,                             // Length = 12
    '1', '9', '9', '1', '0', '5', '0', '6', '1', '6', '4', '5'
  };
  ExpectBadTime(DER_GENERALIZED_TIME_INVALID_TRUNCATED);
}

TEST_F(pkixder_universal_types_tests, TimeNoSeconds)
{
  const uint8_t DER_GENERALIZED_TIME_NO_SECONDS[] = {
    0x18,                           // Generalized Time
    13,                             // Length = 13
    '1', '9', '9', '1', '0', '5', '0', '6', '1', '6', '4', '5', 'Z'
  };
  ExpectBadTime(DER_GENERALIZED_TIME_NO_SECONDS);
}

TEST_F(pkixder_universal_types_tests, TimeInvalidPrefixedYear)
{
  const uint8_t DER_GENERALIZED_TIME_INVALID_PREFIXED_YEAR[] = {
    0x18,                           // Generalized Time
    16,                             // Length = 16
    ' ', '1', '9', '9', '1', '0', '1', '0', '1', '0', '1', '0', '1', '0', '1', 'Z'
  };
  ExpectBadTime(DER_GENERALIZED_TIME_INVALID_PREFIXED_YEAR);
}

TEST_F(pkixder_universal_types_tests, TimeTooManyDigits)
{
  const uint8_t DER_GENERALIZED_TIME_TOO_MANY_DIGITS[] = {
    0x18,                           // Generalized Time
    16,                             // Length = 16
    '1', '1', '1', '1', '1', '0', '1', '0', '1', '0', '1', '0', '1', '0', '1', 'Z'
  };
  ExpectBadTime(DER_GENERALIZED_TIME_TOO_MANY_DIGITS);
}

// In order to ensure we we don't run into any trouble with conversions to and
// from time_t we only accept times from 1970 onwards.
TEST_F(pkixder_universal_types_tests, GeneralizedTimeYearValidRange)
{
  // Note that by using the last second of the last day of the year, we're also
  // effectively testing all the accumulated conversions from Gregorian to to
  // Julian time, including in particular the effects of leap years.

  for (uint16_t i = 1970; i <= 9999; ++i) {
    const uint8_t DER[] = {
      0x18,                           // Generalized Time
      15,                             // Length = 15
      TWO_CHARS(i / 100), TWO_CHARS(i % 100), // YYYY
      '1', '2', '3', '1', // 12-31
      '2', '3', '5', '9', '5', '9', 'Z' // 23:59:59Z
    };

    Time expectedValue = YMDHMS(i, 12, 31, 23, 59, 59);

    // We have to test GeneralizedTime separately from UTCTime instead of using
    // ExpectGooDtime because the range of UTCTime is less than the range of
    // GeneralizedTime.

    // GeneralizedTime
    {
      Input input(DER);
      Reader reader(input);
      Time value(Time::uninitialized);
      ASSERT_EQ(Success, GeneralizedTime(reader, value));
      EXPECT_EQ(expectedValue, value);
    }

    // TimeChoice: GeneralizedTime
    {
      Input input(DER);
      Reader reader(input);
      Time value(Time::uninitialized);
      ASSERT_EQ(Success, TimeChoice(reader, value));
      EXPECT_EQ(expectedValue, value);
    }

    // TimeChoice: UTCTime, which is limited to years less than 2049.
    if (i <= 2049) {
      Time value(Time::uninitialized);
      ASSERT_EQ(Success, TimeChoiceForEquivalentUTCTime(DER, value));
      EXPECT_EQ(expectedValue, value);
    }
  }
}

// In order to ensure we we don't run into any trouble with conversions to and
// from time_t we only accept times from 1970 onwards.
TEST_F(pkixder_universal_types_tests, TimeYearInvalid1969)
{
  static const uint8_t DER[] = {
    0x18,                           // Generalized Time
    15,                             // Length = 15
    '1', '9', '6', '9', '1', '2', '3', '1', // !!!1969!!!-12-31
    '2', '3', '5', '9', '5', '9', 'Z' // 23:59:59Z
  };
  ExpectBadTime(DER);
}

static const uint8_t DAYS_IN_MONTH[] = {
  0,  // unused
  31, // January
  28, // February (leap years tested separately)
  31, // March
  30, // April
  31, // May
  30, // Jun
  31, // July
  31, // August
  30, // September
  31, // October
  30, // November
  31, // December
};

TEST_F(pkixder_universal_types_tests, TimeMonthDaysValidRange)
{
  for (uint8_t month = 1; month <= 12; ++month) {
    for (uint8_t day = 1; day <= DAYS_IN_MONTH[month]; ++day) {
      const uint8_t DER[] = {
        0x18,                           // Generalized Time
        15,                             // Length = 15
        '2', '0', '1', '5', TWO_CHARS(month), TWO_CHARS(day), // (2015-mm-dd)
        '1', '6', '4', '5', '4', '0', 'Z' // 16:45:40
      };
      ExpectGoodTime(YMDHMS(2015, month, day, 16, 45, 40), DER);
    }
  }
}

TEST_F(pkixder_universal_types_tests, TimeMonthInvalid0)
{
  static const uint8_t DER[] = {
    0x18,                           // Generalized Time
    15,                             // Length = 15
    '2', '0', '1', '5', '0', '0', '1', '5', // 2015-!!!00!!!-15
    '1', '6', '4', '5', '4', '0', 'Z' // 16:45:40
  };
  ExpectBadTime(DER);
}

TEST_F(pkixder_universal_types_tests, TimeMonthInvalid13)
{
  const uint8_t DER_GENERALIZED_TIME_13TH_MONTH[] = {
    0x18,                           // Generalized Time
    15,                             // Length = 15
    '1', '9', '9', '1', //YYYY (1991)
    '1', '3', //MM 13th month of the year
    '0', '6', '1', '6', '4', '5', '4', '0', 'Z'
  };
  ExpectBadTime(DER_GENERALIZED_TIME_13TH_MONTH);
}

TEST_F(pkixder_universal_types_tests, TimeDayInvalid0)
{
  static const uint8_t DER[] = {
    0x18,                           // Generalized Time
    15,                             // Length = 15
    '2', '0', '1', '5', '0', '1', '0', '0', // 2015-01-!!!00!!!
    '1', '6', '4', '5', '4', '0', 'Z' // 16:45:40
  };
  ExpectBadTime(DER);
}

TEST_F(pkixder_universal_types_tests, TimeMonthDayInvalidPastEndOfMonth)
{
  for (uint8_t month = 1; month <= 12; ++month) {
    const uint8_t DER[] = {
      0x18,                           // Generalized Time
      15,                             // Length = 15
      '1', '9', '9', '1', // YYYY 1991
      TWO_CHARS(month), // MM
      TWO_CHARS(1 + (month == 2 ? 29 : DAYS_IN_MONTH[month])), // !!!DD!!!
      '1', '6', '4', '5', '4', '0', 'Z' // 16:45:40
    };
    ExpectBadTime(DER);
  }
}

TEST_F(pkixder_universal_types_tests, TimeMonthFebLeapYear2016)
{
  static const uint8_t DER[] = {
    0x18,                           // Generalized Time
    15,                             // Length = 15
    '2', '0', '1', '6', '0', '2', '2', '9', // 2016-02-29
    '1', '6', '4', '5', '4', '0', 'Z' // 16:45:40
  };
  ExpectGoodTime(YMDHMS(2016, 2, 29, 16, 45, 40), DER);
}

TEST_F(pkixder_universal_types_tests, TimeMonthFebLeapYear2000)
{
  static const uint8_t DER[] = {
    0x18,                           // Generalized Time
    15,                             // Length = 15
    '2', '0', '0', '0', '0', '2', '2', '9', // 2000-02-29
    '1', '6', '4', '5', '4', '0', 'Z' // 16:45:40
  };
  ExpectGoodTime(YMDHMS(2000, 2, 29, 16, 45, 40), DER);
}

TEST_F(pkixder_universal_types_tests, TimeMonthFebLeapYear2400)
{
  static const uint8_t DER[] = {
    0x18,                           // Generalized Time
    15,                             // Length = 15
    '2', '4', '0', '0', '0', '2', '2', '9', // 2400-02-29
    '1', '6', '4', '5', '4', '0', 'Z' // 16:45:40
  };

  // We don't use ExpectGoodTime here because UTCTime can't represent 2400.

  Time expectedValue = YMDHMS(2400, 2, 29, 16, 45, 40);

  // GeneralizedTime
  {
    Input input(DER);
    Reader reader(input);
    Time value(Time::uninitialized);
    ASSERT_EQ(Success, GeneralizedTime(reader, value));
    EXPECT_EQ(expectedValue, value);
  }

  // TimeChoice: GeneralizedTime
  {
    Input input(DER);
    Reader reader(input);
    Time value(Time::uninitialized);
    ASSERT_EQ(Success, TimeChoice(reader, value));
    EXPECT_EQ(expectedValue, value);
  }
}

TEST_F(pkixder_universal_types_tests, TimeMonthFebNotLeapYear2014)
{
  static const uint8_t DER[] = {
    0x18,                           // Generalized Time
    15,                             // Length = 15
    '2', '0', '1', '4', '0', '2', '2', '9', // 2014-02-29
    '1', '6', '4', '5', '4', '0', 'Z' // 16:45:40
  };
  ExpectBadTime(DER);
}

TEST_F(pkixder_universal_types_tests, TimeMonthFebNotLeapYear2100)
{
  static const uint8_t DER[] = {
    0x18,                           // Generalized Time
    15,                             // Length = 15
    '2', '1', '0', '0', '0', '2', '2', '9', // 2100-02-29
    '1', '6', '4', '5', '4', '0', 'Z' // 16:45:40
  };

  // We don't use ExpectBadTime here because UTCTime can't represent 2100.

  // GeneralizedTime
  {
    Input input(DER);
    Reader reader(input);
    Time value(Time::uninitialized);
    ASSERT_EQ(Result::ERROR_INVALID_TIME, GeneralizedTime(reader, value));
  }

  // TimeChoice: GeneralizedTime
  {
    Input input(DER);
    Reader reader(input);
    Time value(Time::uninitialized);
    ASSERT_EQ(Result::ERROR_INVALID_TIME, TimeChoice(reader, value));
  }
}

TEST_F(pkixder_universal_types_tests, TimeHoursValidRange)
{
  for (uint8_t i = 0; i <= 23; ++i) {
    const uint8_t DER[] = {
      0x18,                           // Generalized Time
      15,                             // Length = 15
      '2', '0', '1', '2', '0', '6', '3', '0', // YYYYMMDD (2012-06-30)
      TWO_CHARS(i), '5', '9', '0', '1', 'Z' // HHMMSSZ (!!!!ii!!!!:59:01 Zulu)
    };
    ExpectGoodTime(YMDHMS(2012, 6, 30, i, 59, 1), DER);
  }
}

TEST_F(pkixder_universal_types_tests, TimeHoursInvalid_24_00_00)
{
  static const uint8_t DER[] = {
    0x18,                           // Generalized Time
    15,                             // Length = 15
    '2', '0', '1', '2', '0', '6', '3', '0', // YYYYMMDD (2012-06-30)
    '2', '4', '0', '0', '0', '0', 'Z' // HHMMSSZ (!!24!!:00:00 Zulu)
  };
  ExpectBadTime(DER);
}

TEST_F(pkixder_universal_types_tests, TimeMinutesValidRange)
{
  for (uint8_t i = 0; i <= 59; ++i) {
    const uint8_t DER[] = {
      0x18,                           // Generalized Time
      15,                             // Length = 15
      '2', '0', '1', '2', '0', '6', '3', '0', // YYYYMMDD (2012-06-30)
      '2', '3', TWO_CHARS(i), '0', '1', 'Z' // HHMMSSZ (23:!!!!ii!!!!:01 Zulu)
    };
    ExpectGoodTime(YMDHMS(2012, 6, 30, 23, i, 1), DER);
  }
}

TEST_F(pkixder_universal_types_tests, TimeMinutesInvalid60)
{
  const uint8_t DER[] = {
    0x18,                           // Generalized Time
    15,                             // Length = 15
    '2', '0', '1', '2', '0', '6', '3', '0', // YYYYMMDD (2012-06-30)
    '2', '3', '6', '0', '5', '9', 'Z' // HHMMSSZ (23:!!!60!!!:01 Zulu)
  };
  ExpectBadTime(DER);
}

TEST_F(pkixder_universal_types_tests, TimeSecondsValidRange)
{
  for (uint8_t i = 0; i <= 59; ++i) {
    const uint8_t DER[] = {
      0x18,                           // Generalized Time
      15,                             // Length = 15
      '2', '0', '1', '2', '0', '6', '3', '0', // YYYYMMDD (2012-06-30)
      '2', '3', '5', '9', TWO_CHARS(i), 'Z' // HHMMSSZ (23:59:!!!!ii!!!! Zulu)
    };
    ExpectGoodTime(YMDHMS(2012, 6, 30, 23, 59, i), DER);
  }
}

// No Leap Seconds (60)
TEST_F(pkixder_universal_types_tests, TimeSecondsInvalid60)
{
  static const uint8_t DER[] = {
    0x18,                           // Generalized Time
    15,                             // Length = 15
    '2', '0', '1', '2', '0', '6', '3', '0', // YYYYMMDD (2012-06-30)
    '2', '3', '5', '9', '6', '0', 'Z' // HHMMSSZ (23:59:!!!!60!!!! Zulu)
  };
  ExpectBadTime(DER);
}

// No Leap Seconds (61)
TEST_F(pkixder_universal_types_tests, TimeSecondsInvalid61)
{
  static const uint8_t DER[] = {
    0x18,                           // Generalized Time
    15,                             // Length = 15
    '2', '0', '1', '2', '0', '6', '3', '0', // YYYYMMDD (2012-06-30)
    '2', '3', '5', '9', '6', '1', 'Z' // HHMMSSZ (23:59:!!!!61!!!! Zulu)
  };
  ExpectBadTime(DER);
}

TEST_F(pkixder_universal_types_tests, TimeInvalidZulu)
{
  const uint8_t DER_GENERALIZED_TIME_INVALID_ZULU[] = {
    0x18,                           // Generalized Time
    15,                             // Length = 15
    '2', '0', '1', '2', '0', '6', '3', '0', // YYYYMMDD (2012-06-30)
    '2', '3', '5', '9', '5', '9', 'z' // HHMMSSZ (23:59:59 !!!z!!!) should be Z
  };
  ExpectBadTime(DER_GENERALIZED_TIME_INVALID_ZULU);
}

TEST_F(pkixder_universal_types_tests, TimeInvalidExtraData)
{
  const uint8_t DER_GENERALIZED_TIME_INVALID_EXTRA_DATA[] = {
    0x18,                           // Generalized Time
    16,                             // Length = 16
    '2', '0', '1', '2', '0', '6', '3', '0', // YYYYMMDD (2012-06-30)
    '2', '3', '5', '9', '5', '9', 'Z', // HHMMSSZ (23:59:59Z)
    0 // Extra null character
  };
  ExpectBadTime(DER_GENERALIZED_TIME_INVALID_EXTRA_DATA);
}

TEST_F(pkixder_universal_types_tests, TimeInvalidCenturyChar)
{
  const uint8_t DER_GENERALIZED_TIME_INVALID_CENTURY_CHAR[] = {
    0x18,                           // Generalized Time
    15,                             // Length = 15
    'X', '9', '9', '1', '1', '2', '0', '6', // YYYYMMDD (X991-12-06)
    '1', '6', '4', '5', '4', '0', 'Z' // HHMMSSZ (16:45:40Z)
  };

  // We can't use ExpectBadTime here, because ExpectBadTime requires
  // consistent results for GeneralizedTime and UTCTime, but the results
  // for this input are different.

  // GeneralizedTime
  {
    Input input(DER_GENERALIZED_TIME_INVALID_CENTURY_CHAR);
    Reader reader(input);
    Time value(Time::uninitialized);
    ASSERT_EQ(Result::ERROR_INVALID_TIME, GeneralizedTime(reader, value));
  }

  // TimeChoice: GeneralizedTime
  {
    Input input(DER_GENERALIZED_TIME_INVALID_CENTURY_CHAR);
    Reader reader(input);
    Time value(Time::uninitialized);
    ASSERT_EQ(Result::ERROR_INVALID_TIME, TimeChoice(reader, value));
  }

  // This test is not applicable to TimeChoice: UTCTime
}

TEST_F(pkixder_universal_types_tests, TimeInvalidYearChar)
{
  const uint8_t DER_GENERALIZED_TIME_INVALID_YEAR_CHAR[] = {
    0x18,                           // Generalized Time
    15,                             // Length = 15
    '1', '9', '9', 'I', '0', '1', '0', '6', // YYYYMMDD (199I-12-06)
    '1', '6', '4', '5', '4', '0', 'Z' // HHMMSSZ (16:45:40Z)
  };
  ExpectBadTime(DER_GENERALIZED_TIME_INVALID_YEAR_CHAR);
}

TEST_F(pkixder_universal_types_tests, GeneralizedTimeInvalidMonthChar)
{
  const uint8_t DER_GENERALIZED_TIME_INVALID_MONTH_CHAR[] = {
    0x18,                           // Generalized Time
    15,                             // Length = 15
    '1', '9', '9', '1', '0', 'I', '0', '6', // YYYYMMDD (1991-0I-06)
    '1', '6', '4', '5', '4', '0', 'Z' // HHMMSSZ (16:45:40Z)
  };
  ExpectBadTime(DER_GENERALIZED_TIME_INVALID_MONTH_CHAR);
}

TEST_F(pkixder_universal_types_tests, TimeInvalidDayChar)
{
  const uint8_t DER_GENERALIZED_TIME_INVALID_DAY_CHAR[] = {
    0x18,                           // Generalized Time
    15,                             // Length = 15
    '1', '9', '9', '1', '0', '1', '0', 'S', // YYYYMMDD (1991-01-0S)
    '1', '6', '4', '5', '4', '0', 'Z' // HHMMSSZ (16:45:40Z)
  };
  ExpectBadTime(DER_GENERALIZED_TIME_INVALID_DAY_CHAR);
}

TEST_F(pkixder_universal_types_tests, TimeInvalidFractionalSeconds)
{
  const uint8_t DER_GENERALIZED_TIME_INVALID_FRACTIONAL_SECONDS[] = {
    0x18,                           // Generalized Time
    17,                             // Length = 17
    '1', '9', '9', '1', '0', '1', '0', '1', // YYYYMMDD (1991-01-01)
    '1', '6', '4', '5', '4', '0', '.', '3', 'Z' // HHMMSS.FFF (16:45:40.3Z)
  };
  ExpectBadTime(DER_GENERALIZED_TIME_INVALID_FRACTIONAL_SECONDS);
}

TEST_F(pkixder_universal_types_tests, Integer_0_127)
{
  for (uint8_t i = 0; i <= 127; ++i) {
    const uint8_t DER[] = {
      0x02, // INTEGER
      0x01, // length
      i,    // value
    };
    Input input(DER);
    Reader reader(input);

    uint8_t value = i + 1; // initialize with a value that is NOT i.
    ASSERT_EQ(Success, Integer(reader, value));
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
  Input input(DER);
  Reader reader(input);

  uint8_t value;
  ASSERT_EQ(Result::ERROR_BAD_DER, Integer(reader, value));
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
  Input input(DER);
  Reader reader(input);

  uint8_t value;
  ASSERT_EQ(Result::ERROR_BAD_DER, Integer(reader, value));
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
  Input input(DER);
  Reader reader(input);

  uint8_t value;
  ASSERT_EQ(Result::ERROR_BAD_DER, Integer(reader, value));
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
  Input input(DER);
  Reader reader(input);

  uint8_t value;
  ASSERT_EQ(Result::ERROR_BAD_DER, Integer(reader, value));
}

TEST_F(pkixder_universal_types_tests, IntegerTruncatedOneByte)
{
  const uint8_t DER_INTEGER_TRUNCATED[] = {
    0x02,                       // INTEGER
    0x01,                       // length
    // MISSING DATA HERE
  };
  Input input(DER_INTEGER_TRUNCATED);
  Reader reader(input);

  uint8_t value;
  ASSERT_EQ(Result::ERROR_BAD_DER, Integer(reader, value));
}

TEST_F(pkixder_universal_types_tests, IntegerTruncatedLarge)
{
  const uint8_t DER_INTEGER_TRUNCATED[] = {
    0x02,                       // INTEGER
    0x04,                       // length
    0x11, 0x22                  // 0x1122
    // MISSING DATA HERE
  };
  Input input(DER_INTEGER_TRUNCATED);
  Reader reader(input);

  uint8_t value;
  ASSERT_EQ(Result::ERROR_BAD_DER, Integer(reader, value));
}

TEST_F(pkixder_universal_types_tests, IntegerZeroLength)
{
  const uint8_t DER_INTEGER_ZERO_LENGTH[] = {
    0x02,                       // INTEGER
    0x00                        // length
  };
  Input input(DER_INTEGER_ZERO_LENGTH);
  Reader reader(input);

  uint8_t value;
  ASSERT_EQ(Result::ERROR_BAD_DER, Integer(reader, value));
}

TEST_F(pkixder_universal_types_tests, IntegerOverlyLong1)
{
  const uint8_t DER_INTEGER_OVERLY_LONG1[] = {
    0x02,                       // INTEGER
    0x02,                       // length
    0x00, 0x01                  //
  };
  Input input(DER_INTEGER_OVERLY_LONG1);
  Reader reader(input);

  uint8_t value;
  ASSERT_EQ(Result::ERROR_BAD_DER, Integer(reader, value));
}

TEST_F(pkixder_universal_types_tests, IntegerOverlyLong2)
{
  const uint8_t DER_INTEGER_OVERLY_LONG2[] = {
    0x02,                       // INTEGER
    0x02,                       // length
    0xff, 0x80                  //
  };
  Input input(DER_INTEGER_OVERLY_LONG2);
  Reader reader(input);

  uint8_t value;
  ASSERT_EQ(Result::ERROR_BAD_DER, Integer(reader, value));
}

TEST_F(pkixder_universal_types_tests, OptionalIntegerSupportedDefault)
{
  // The input is a BOOLEAN and not INTEGER for the input so we'll not parse
  // anything and instead use the default value.
  Input input(DER_BOOLEAN_TRUE);
  Reader reader(input);

  long value = 1;
  ASSERT_EQ(Success, OptionalInteger(reader, -1, value));
  ASSERT_EQ(-1, value);
  bool boolValue;
  ASSERT_EQ(Success, Boolean(reader, boolValue));
}

TEST_F(pkixder_universal_types_tests, OptionalIntegerUnsupportedDefault)
{
  // The same as the previous test, except with an unsupported default value
  // passed in.
  Input input(DER_BOOLEAN_TRUE);
  Reader reader(input);

  long value;
  ASSERT_EQ(Result::FATAL_ERROR_INVALID_ARGS, OptionalInteger(reader, 0, value));
}

TEST_F(pkixder_universal_types_tests, OptionalIntegerSupportedDefaultAtEnd)
{
  static const uint8_t dummy = 1;
  Input input;
  ASSERT_EQ(Success, input.Init(&dummy, 0));
  Reader reader(input);

  long value = 1;
  ASSERT_EQ(Success, OptionalInteger(reader, -1, value));
  ASSERT_EQ(-1, value);
}

TEST_F(pkixder_universal_types_tests, OptionalIntegerNonDefaultValue)
{
  static const uint8_t DER[] = {
    0x02, // INTEGER
    0x01, // length
    0x00
  };
  Input input(DER);
  Reader reader(input);

  long value = 2;
  ASSERT_EQ(Success, OptionalInteger(reader, -1, value));
  ASSERT_EQ(0, value);
  ASSERT_TRUE(reader.AtEnd());
}

TEST_F(pkixder_universal_types_tests, Null)
{
  const uint8_t DER_NUL[] = {
    0x05,
    0x00
  };
  Input input(DER_NUL);
  Reader reader(input);

  ASSERT_EQ(Success, Null(reader));
}

TEST_F(pkixder_universal_types_tests, NullWithBadLength)
{
  const uint8_t DER_NULL_BAD_LENGTH[] = {
    0x05,
    0x01,
    0x00
  };
  Input input(DER_NULL_BAD_LENGTH);
  Reader reader(input);

  ASSERT_EQ(Result::ERROR_BAD_DER, Null(reader));
}

TEST_F(pkixder_universal_types_tests, OID)
{
  const uint8_t DER_VALID_OID[] = {
    0x06,
    0x09,
    0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x01
  };
  Input input(DER_VALID_OID);
  Reader reader(input);

  const uint8_t expectedOID[] = {
    0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x01
  };

  ASSERT_EQ(Success, OID(reader, expectedOID));
}

} // unnamed namespace
