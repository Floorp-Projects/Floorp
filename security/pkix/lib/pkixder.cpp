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

#include "pkixder.h"
#include "pkix/bind.h"
#include "cert.h"

namespace mozilla { namespace pkix { namespace der {

// not inline
Result
Fail(PRErrorCode errorCode)
{
  PR_SetError(errorCode, 0);
  return Failure;
}

namespace internal {

// Too complicated to be inline
Result
ExpectTagAndGetLength(Input& input, uint8_t expectedTag, uint16_t& length)
{
  PR_ASSERT((expectedTag & 0x1F) != 0x1F); // high tag number form not allowed

  uint8_t tag;
  if (input.Read(tag) != Success) {
    return Failure;
  }

  if (tag != expectedTag) {
    return Fail(SEC_ERROR_BAD_DER);
  }

  // The short form of length is a single byte with the high order bit set
  // to zero. The long form of length is one byte with the high order bit
  // set, followed by N bytes, where N is encoded in the lowest 7 bits of
  // the first byte.
  uint8_t length1;
  if (input.Read(length1) != Success) {
    return Failure;
  }
  if (!(length1 & 0x80)) {
    length = length1;
  } else if (length1 == 0x81) {
    uint8_t length2;
    if (input.Read(length2) != Success) {
      return Failure;
    }
    if (length2 < 128) {
      // Not shortest possible encoding
      return Fail(SEC_ERROR_BAD_DER);
    }
    length = length2;
  } else if (length1 == 0x82) {
    if (input.Read(length) != Success) {
      return Failure;
    }
    if (length < 256) {
      // Not shortest possible encoding
      return Fail(SEC_ERROR_BAD_DER);
    }
  } else {
    // We don't support lengths larger than 2^16 - 1.
    return Fail(SEC_ERROR_BAD_DER);
  }

  // Ensure the input is long enough for the length it says it has.
  return input.EnsureLength(length);
}

} // namespace internal

Result
SignedData(Input& input, /*out*/ Input& tbs, /*out*/ CERTSignedData& signedData)
{
  Input::Mark mark(input.GetMark());

  if (ExpectTagAndGetValue(input, SEQUENCE, tbs) != Success) {
    return Failure;
  }

  if (input.GetSECItem(siBuffer, mark, signedData.data) != Success) {
    return Failure;
  }

  if (AlgorithmIdentifier(input, signedData.signatureAlgorithm) != Success) {
    return Failure;
  }

  if (ExpectTagAndGetValue(input, BIT_STRING, signedData.signature)
        != Success) {
    return Failure;
  }
  if (signedData.signature.len == 0) {
    return Fail(SEC_ERROR_BAD_SIGNATURE);
  }
  unsigned int unusedBitsAtEnd = signedData.signature.data[0];
  // XXX: Really the constraint should be that unusedBitsAtEnd must be less
  // than 7. But, we suspect there are no real-world OCSP responses or X.509
  // certificates with non-zero unused bits. It seems like NSS assumes this in
  // various places, so we enforce it too in order to simplify this code. If we
  // find compatibility issues, we'll know we're wrong and we'll have to figure
  // out how to shift the bits around.
  if (unusedBitsAtEnd != 0) {
    return Fail(SEC_ERROR_BAD_SIGNATURE);
  }
  ++signedData.signature.data;
  --signedData.signature.len;
  signedData.signature.len = (signedData.signature.len << 3); // Bytes to bits

  return Success;
}

static inline Result
ReadDigit(Input& input, /*out*/ int& value)
{
  uint8_t b;
  if (input.Read(b) != Success) {
    return Fail(SEC_ERROR_INVALID_TIME);
  }
  if (b < '0' || b > '9') {
    return Fail(SEC_ERROR_INVALID_TIME);
  }
  value = b - '0';
  return Success;
}

static inline Result
ReadTwoDigits(Input& input, int minValue, int maxValue, /*out*/ int& value)
{
  int hi;
  if (ReadDigit(input, hi) != Success) {
    return Failure;
  }
  int lo;
  if (ReadDigit(input, lo) != Success) {
    return Failure;
  }
  value = (hi * 10) + lo;
  if (value < minValue || value > maxValue) {
    return Fail(SEC_ERROR_INVALID_TIME);
  }
  return Success;
}

inline int
daysBeforeYear(int year)
{
  return (365 * (year - 1))
       + ((year - 1) / 4)    // leap years are every 4 years,
       - ((year - 1) / 100)  // except years divisible by 100,
       + ((year - 1) / 400); // except years divisible by 400.
}

// We parse GeneralizedTime and UTCTime according to RFC 5280 and we do not
// accept all time formats allowed in the ASN.1 spec. That is,
// GeneralizedTime must always be in the format YYYYMMDDHHMMSSZ and UTCTime
// must always be in the format YYMMDDHHMMSSZ. Timezone formats of the form
// +HH:MM or -HH:MM or NOT accepted.
Result
TimeChoice(SECItemType type, Input& input, /*out*/ PRTime& time)
{
  int days;

  int yearHi;
  int yearLo;
  if (type == siGeneralizedTime) {
    if (ReadTwoDigits(input, 0, 99, yearHi) != Success) {
      return Failure;
    }
    if (ReadTwoDigits(input, 0, 99, yearLo) != Success) {
      return Failure;
    }
  } else if (type == siUTCTime) {
    if (ReadTwoDigits(input, 0, 99, yearLo) != Success) {
      return Failure;
    }
    yearHi = yearLo >= 50 ? 19 : 20;
  } else {
    PR_NOT_REACHED("invalid tag given to TimeChoice");
    return Fail(SEC_ERROR_INVALID_TIME);
  }
  int year = (yearHi * 100) + yearLo;
  if (year < 1970) {
    // We don't support dates before January 1, 1970 because that is the epoch.
    return Fail(SEC_ERROR_INVALID_TIME); // TODO: better error code
  }
  if (year > 1970) {
    // This is NOT equivalent to daysBeforeYear(year - 1970) because the
    // leap year calculations in daysBeforeYear only works on absolute years.
    days = daysBeforeYear(year) - daysBeforeYear(1970);
    // We subtract 1 because we're interested in knowing how many days there
    // were *before* the given year, relative to 1970.
  } else {
    days = 0;
  }

  int month;
  if (ReadTwoDigits(input, 1, 12, month) != Success) {
    return Failure;
  }
  int daysInMonth;
  static const int jan = 31;
  const int feb = ((year % 4 == 0) &&
                   ((year % 100 != 0) || (year % 400 == 0)))
                ? 29
                : 28;
  static const int mar = 31;
  static const int apr = 30;
  static const int may = 31;
  static const int jun = 30;
  static const int jul = 31;
  static const int aug = 31;
  static const int sep = 30;
  static const int oct = 31;
  static const int nov = 30;
  static const int dec = 31;
  switch (month) {
    case 1:  daysInMonth = jan; break;
    case 2:  daysInMonth = feb; days += jan; break;
    case 3:  daysInMonth = mar; days += jan + feb; break;
    case 4:  daysInMonth = apr; days += jan + feb + mar; break;
    case 5:  daysInMonth = may; days += jan + feb + mar + apr; break;
    case 6:  daysInMonth = jun; days += jan + feb + mar + apr + may; break;
    case 7:  daysInMonth = jul; days += jan + feb + mar + apr + may + jun;
             break;
    case 8:  daysInMonth = aug; days += jan + feb + mar + apr + may + jun +
                                        jul;
             break;
    case 9:  daysInMonth = sep; days += jan + feb + mar + apr + may + jun +
                                        jul + aug;
             break;
    case 10: daysInMonth = oct; days += jan + feb + mar + apr + may + jun +
                                        jul + aug + sep;
             break;
    case 11: daysInMonth = nov; days += jan + feb + mar + apr + may + jun +
                                        jul + aug + sep + oct;
             break;
    case 12: daysInMonth = dec; days += jan + feb + mar + apr + may + jun +
                                        jul + aug + sep + oct + nov;
             break;
    default:
      PR_NOT_REACHED("month already bounds-checked by ReadTwoDigits");
      return Fail(PR_INVALID_STATE_ERROR);
  }

  int dayOfMonth;
  if (ReadTwoDigits(input, 1, daysInMonth, dayOfMonth) != Success) {
    return Failure;
  }
  days += dayOfMonth - 1;

  int hours;
  if (ReadTwoDigits(input, 0, 23, hours) != Success) {
    return Failure;
  }
  int minutes;
  if (ReadTwoDigits(input, 0, 59, minutes) != Success) {
    return Failure;
  }
  int seconds;
  if (ReadTwoDigits(input, 0, 59, seconds) != Success) {
    return Failure;
  }

  uint8_t b;
  if (input.Read(b) != Success) {
    return Fail(SEC_ERROR_INVALID_TIME);
  }
  if (b != 'Z') {
    return Fail(SEC_ERROR_INVALID_TIME); // TODO: better error code?
  }
  if (End(input) != Success) {
    return Fail(SEC_ERROR_INVALID_TIME);
  }

  int64_t totalSeconds = (static_cast<int64_t>(days) * 24 * 60 * 60) +
                         (static_cast<int64_t>(hours)     * 60 * 60) +
                         (static_cast<int64_t>(minutes)        * 60) +
                         seconds;

  time = totalSeconds * PR_USEC_PER_SEC;
  return Success;
}

} } } // namespace mozilla::pkix::der
