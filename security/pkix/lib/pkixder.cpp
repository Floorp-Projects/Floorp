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
#include "pkixutil.h"

namespace mozilla { namespace pkix { namespace der {

// Too complicated to be inline
Result
ReadTagAndGetValue(Reader& input, /*out*/ uint8_t& tag, /*out*/ Input& value)
{
  Result rv;

  rv = input.Read(tag);
  if (rv != Success) {
    return rv;
  }
  if ((tag & 0x1F) == 0x1F) {
    return Result::ERROR_BAD_DER; // high tag number form not allowed
  }

  uint16_t length;

  // The short form of length is a single byte with the high order bit set
  // to zero. The long form of length is one byte with the high order bit
  // set, followed by N bytes, where N is encoded in the lowest 7 bits of
  // the first byte.
  uint8_t length1;
  rv = input.Read(length1);
  if (rv != Success) {
    return rv;
  }
  if (!(length1 & 0x80)) {
    length = length1;
  } else if (length1 == 0x81) {
    uint8_t length2;
    rv = input.Read(length2);
    if (rv != Success) {
      return rv;
    }
    if (length2 < 128) {
      // Not shortest possible encoding
      return Result::ERROR_BAD_DER;
    }
    length = length2;
  } else if (length1 == 0x82) {
    rv = input.Read(length);
    if (rv != Success) {
      return rv;
    }
    if (length < 256) {
      // Not shortest possible encoding
      return Result::ERROR_BAD_DER;
    }
  } else {
    // We don't support lengths larger than 2^16 - 1.
    return Result::ERROR_BAD_DER;
  }

  return input.Skip(length, value);
}

static Result
OptionalNull(Reader& input)
{
  if (input.Peek(NULLTag)) {
    return Null(input);
  }
  return Success;
}

namespace {

Result
DigestAlgorithmOIDValue(Reader& algorithmID,
                        /*out*/ DigestAlgorithm& algorithm)
{
  // RFC 4055 Section 2.1
  // python DottedOIDToCode.py id-sha1 1.3.14.3.2.26
  static const uint8_t id_sha1[] = {
    0x2b, 0x0e, 0x03, 0x02, 0x1a
  };
  // python DottedOIDToCode.py id-sha256 2.16.840.1.101.3.4.2.1
  static const uint8_t id_sha256[] = {
    0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01
  };
  // python DottedOIDToCode.py id-sha384 2.16.840.1.101.3.4.2.2
  static const uint8_t id_sha384[] = {
    0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02
  };
  // python DottedOIDToCode.py id-sha512 2.16.840.1.101.3.4.2.3
  static const uint8_t id_sha512[] = {
    0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03
  };

  // Matching is attempted based on a rough estimate of the commonality of the
  // algorithm, to minimize the number of MatchRest calls.
  if (algorithmID.MatchRest(id_sha1)) {
    algorithm = DigestAlgorithm::sha1;
  } else if (algorithmID.MatchRest(id_sha256)) {
    algorithm = DigestAlgorithm::sha256;
  } else if (algorithmID.MatchRest(id_sha384)) {
    algorithm = DigestAlgorithm::sha384;
  } else if (algorithmID.MatchRest(id_sha512)) {
    algorithm = DigestAlgorithm::sha512;
  } else {
    return Result::ERROR_INVALID_ALGORITHM;
  }

  return Success;
}

Result
SignatureAlgorithmOIDValue(Reader& algorithmID,
                           /*out*/ SignatureAlgorithm& algorithm)
{
  // RFC 5758 Section 3.1 (id-dsa-with-sha224 is intentionally excluded)
  // python DottedOIDToCode.py id-dsa-with-sha256 2.16.840.1.101.3.4.3.2
  static const uint8_t id_dsa_with_sha256[] = {
    0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x03, 0x02
  };

  // RFC 5758 Section 3.2 (ecdsa-with-SHA224 is intentionally excluded)
  // python DottedOIDToCode.py ecdsa-with-SHA256 1.2.840.10045.4.3.2
  static const uint8_t ecdsa_with_SHA256[] = {
    0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x02
  };
  // python DottedOIDToCode.py ecdsa-with-SHA384 1.2.840.10045.4.3.3
  static const uint8_t ecdsa_with_SHA384[] = {
    0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x03
  };
  // python DottedOIDToCode.py ecdsa-with-SHA512 1.2.840.10045.4.3.4
  static const uint8_t ecdsa_with_SHA512[] = {
    0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x03, 0x04
  };

  // RFC 4055 Section 5 (sha224WithRSAEncryption is intentionally excluded)
  // python DottedOIDToCode.py sha256WithRSAEncryption 1.2.840.113549.1.1.11
  static const uint8_t sha256WithRSAEncryption[] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b
  };
  // python DottedOIDToCode.py sha384WithRSAEncryption 1.2.840.113549.1.1.12
  static const uint8_t sha384WithRSAEncryption[] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0c
  };
  // python DottedOIDToCode.py sha512WithRSAEncryption 1.2.840.113549.1.1.13
  static const uint8_t sha512WithRSAEncryption[] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0d
  };

  // RFC 3279 Section 2.2.1
  // python DottedOIDToCode.py sha-1WithRSAEncryption 1.2.840.113549.1.1.5
  static const uint8_t sha_1WithRSAEncryption[] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x05
  };

  // NIST Open Systems Environment (OSE) Implementor's Workshop (OIW)
  // http://www.oiw.org/agreements/stable/12s-9412.txt (no longer works).
  // http://www.imc.org/ietf-pkix/old-archive-97/msg01166.html
  // We need to support this this non-PKIX OID for compatibility.
  // python DottedOIDToCode.py sha1WithRSASignature 1.3.14.3.2.29
  static const uint8_t sha1WithRSASignature[] = {
    0x2b, 0x0e, 0x03, 0x02, 0x1d
  };

  // RFC 3279 Section 2.2.2
  // python DottedOIDToCode.py id-dsa-with-sha1 1.2.840.10040.4.3
  static const uint8_t id_dsa_with_sha1[] = {
    0x2a, 0x86, 0x48, 0xce, 0x38, 0x04, 0x03
  };

  // RFC 3279 Section 2.2.3
  // python DottedOIDToCode.py ecdsa-with-SHA1 1.2.840.10045.4.1
  static const uint8_t ecdsa_with_SHA1[] = {
    0x2a, 0x86, 0x48, 0xce, 0x3d, 0x04, 0x01
  };

  // RFC 5758 Section 3.1 (DSA with SHA-2), RFC 3279 Section 2.2.2 (DSA with
  // SHA-1), RFC 5758 Section 3.2 (ECDSA with SHA-2), and RFC 3279
  // Section 2.2.3 (ECDSA with SHA-1) all say that parameters must be omitted.
  //
  // RFC 4055 Section 5 and RFC 3279 Section 2.2.1 both say that parameters for
  // RSA must be encoded as NULL; we relax that requirement by allowing the
  // NULL to be omitted, to match all the other signature algorithms we support
  // and for compatibility.

  // Matching is attempted based on a rough estimate of the commonality of the
  // algorithm, to minimize the number of MatchRest calls.
  if (algorithmID.MatchRest(sha256WithRSAEncryption)) {
    algorithm = SignatureAlgorithm::rsa_pkcs1_with_sha256;
  } else if (algorithmID.MatchRest(ecdsa_with_SHA256)) {
    algorithm = SignatureAlgorithm::ecdsa_with_sha256;
  } else if (algorithmID.MatchRest(sha_1WithRSAEncryption)) {
    algorithm = SignatureAlgorithm::rsa_pkcs1_with_sha1;
  } else if (algorithmID.MatchRest(ecdsa_with_SHA1)) {
    algorithm = SignatureAlgorithm::ecdsa_with_sha1;
  } else if (algorithmID.MatchRest(ecdsa_with_SHA384)) {
    algorithm = SignatureAlgorithm::ecdsa_with_sha384;
  } else if (algorithmID.MatchRest(ecdsa_with_SHA512)) {
    algorithm = SignatureAlgorithm::ecdsa_with_sha512;
  } else if (algorithmID.MatchRest(sha384WithRSAEncryption)) {
    algorithm = SignatureAlgorithm::rsa_pkcs1_with_sha384;
  } else if (algorithmID.MatchRest(sha512WithRSAEncryption)) {
    algorithm = SignatureAlgorithm::rsa_pkcs1_with_sha512;
  } else if (algorithmID.MatchRest(id_dsa_with_sha1)) {
    algorithm = SignatureAlgorithm::dsa_with_sha1;
  } else if (algorithmID.MatchRest(id_dsa_with_sha256)) {
    algorithm = SignatureAlgorithm::dsa_with_sha256;
  } else if (algorithmID.MatchRest(sha1WithRSASignature)) {
    // XXX(bug 1042479): recognize this old OID for compatibility.
    algorithm = SignatureAlgorithm::rsa_pkcs1_with_sha1;
  } else {
    // Any MD5-based signature algorithm, or any unknown signature algorithm.
    return Result::ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED;
  }

  return Success;
}

template <typename OidValueParser, typename Algorithm>
Result
AlgorithmIdentifier(OidValueParser oidValueParser, Reader& input,
                    /*out*/ Algorithm& algorithm)
{
  Reader value;
  Result rv = ExpectTagAndGetValue(input, SEQUENCE, value);
  if (rv != Success) {
    return rv;
  }

  Reader algorithmID;
  rv = ExpectTagAndGetValue(value, der::OIDTag, algorithmID);
  if (rv != Success) {
    return rv;
  }
  rv = oidValueParser(algorithmID, algorithm);
  if (rv != Success) {
    return rv;
  }

  rv = OptionalNull(value);
  if (rv != Success) {
    return rv;
  }

  return End(value);
}

} // unnamed namespace

Result
SignatureAlgorithmIdentifier(Reader& input,
                             /*out*/ SignatureAlgorithm& algorithm)
{
  return AlgorithmIdentifier(SignatureAlgorithmOIDValue, input, algorithm);
}

Result
DigestAlgorithmIdentifier(Reader& input, /*out*/ DigestAlgorithm& algorithm)
{
  return AlgorithmIdentifier(DigestAlgorithmOIDValue, input, algorithm);
}

Result
SignedData(Reader& input, /*out*/ Reader& tbs,
           /*out*/ SignedDataWithSignature& signedData)
{
  Reader::Mark mark(input.GetMark());

  Result rv;
  rv = ExpectTagAndGetValue(input, SEQUENCE, tbs);
  if (rv != Success) {
    return rv;
  }

  rv = input.GetInput(mark, signedData.data);
  if (rv != Success) {
    return rv;
  }

  rv = SignatureAlgorithmIdentifier(input, signedData.algorithm);
  if (rv != Success) {
    return rv;
  }

  rv = BitStringWithNoUnusedBits(input, signedData.signature);
  if (rv == Result::ERROR_BAD_DER) {
    rv = Result::ERROR_BAD_SIGNATURE;
  }
  return rv;
}

Result
BitStringWithNoUnusedBits(Reader& input, /*out*/ Input& value)
{
  Reader valueWithUnusedBits;
  Result rv = ExpectTagAndGetValue(input, BIT_STRING, valueWithUnusedBits);
  if (rv != Success) {
    return rv;
  }

  uint8_t unusedBitsAtEnd;
  if (valueWithUnusedBits.Read(unusedBitsAtEnd) != Success) {
    return Result::ERROR_BAD_DER;
  }
  // XXX: Really the constraint should be that unusedBitsAtEnd must be less
  // than 7. But, we suspect there are no real-world values in OCSP responses
  // or certificates with non-zero unused bits. It seems like NSS assumes this
  // in various places, so we enforce it too in order to simplify this code. If
  // we find compatibility issues, we'll know we're wrong and we'll have to
  // figure out how to shift the bits around.
  if (unusedBitsAtEnd != 0) {
    return Result::ERROR_BAD_DER;
  }
  Reader::Mark mark(valueWithUnusedBits.GetMark());
  valueWithUnusedBits.SkipToEnd();
  return valueWithUnusedBits.GetInput(mark, value);
}

static inline Result
ReadDigit(Reader& input, /*out*/ unsigned int& value)
{
  uint8_t b;
  if (input.Read(b) != Success) {
    return Result::ERROR_INVALID_TIME;
  }
  if (b < '0' || b > '9') {
    return Result::ERROR_INVALID_TIME;
  }
  value = static_cast<unsigned int>(b - static_cast<uint8_t>('0'));
  return Success;
}

static inline Result
ReadTwoDigits(Reader& input, unsigned int minValue, unsigned int maxValue,
              /*out*/ unsigned int& value)
{
  unsigned int hi;
  Result rv = ReadDigit(input, hi);
  if (rv != Success) {
    return rv;
  }
  unsigned int lo;
  rv = ReadDigit(input, lo);
  if (rv != Success) {
    return rv;
  }
  value = (hi * 10) + lo;
  if (value < minValue || value > maxValue) {
    return Result::ERROR_INVALID_TIME;
  }
  return Success;
}

namespace internal {

// We parse GeneralizedTime and UTCTime according to RFC 5280 and we do not
// accept all time formats allowed in the ASN.1 spec. That is,
// GeneralizedTime must always be in the format YYYYMMDDHHMMSSZ and UTCTime
// must always be in the format YYMMDDHHMMSSZ. Timezone formats of the form
// +HH:MM or -HH:MM or NOT accepted.
Result
TimeChoice(Reader& tagged, uint8_t expectedTag, /*out*/ Time& time)
{
  unsigned int days;

  Reader input;
  Result rv = ExpectTagAndGetValue(tagged, expectedTag, input);
  if (rv != Success) {
    return rv;
  }

  unsigned int yearHi;
  unsigned int yearLo;
  if (expectedTag == GENERALIZED_TIME) {
    rv = ReadTwoDigits(input, 0, 99, yearHi);
    if (rv != Success) {
      return rv;
    }
    rv = ReadTwoDigits(input, 0, 99, yearLo);
    if (rv != Success) {
      return rv;
    }
  } else if (expectedTag == UTCTime) {
    rv = ReadTwoDigits(input, 0, 99, yearLo);
    if (rv != Success) {
      return rv;
    }
    yearHi = yearLo >= 50u ? 19u : 20u;
  } else {
    return NotReached("invalid tag given to TimeChoice",
                      Result::ERROR_INVALID_TIME);
  }
  unsigned int year = (yearHi * 100u) + yearLo;
  if (year < 1970u) {
    // We don't support dates before January 1, 1970 because that is the epoch.
    return Result::ERROR_INVALID_TIME;
  }
  days = DaysBeforeYear(year);

  unsigned int month;
  rv = ReadTwoDigits(input, 1u, 12u, month);
  if (rv != Success) {
    return rv;
  }
  unsigned int daysInMonth;
  static const unsigned int jan = 31u;
  const unsigned int feb = ((year % 4u == 0u) &&
                           ((year % 100u != 0u) || (year % 400u == 0u)))
                         ? 29u
                         : 28u;
  static const unsigned int mar = 31u;
  static const unsigned int apr = 30u;
  static const unsigned int may = 31u;
  static const unsigned int jun = 30u;
  static const unsigned int jul = 31u;
  static const unsigned int aug = 31u;
  static const unsigned int sep = 30u;
  static const unsigned int oct = 31u;
  static const unsigned int nov = 30u;
  static const unsigned int dec = 31u;
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
      return NotReached("month already bounds-checked by ReadTwoDigits",
                        Result::FATAL_ERROR_INVALID_STATE);
  }

  unsigned int dayOfMonth;
  rv = ReadTwoDigits(input, 1u, daysInMonth, dayOfMonth);
  if (rv != Success) {
    return rv;
  }
  days += dayOfMonth - 1;

  unsigned int hours;
  rv = ReadTwoDigits(input, 0u, 23u, hours);
  if (rv != Success) {
    return rv;
  }
  unsigned int minutes;
  rv = ReadTwoDigits(input, 0u, 59u, minutes);
  if (rv != Success) {
    return rv;
  }
  unsigned int seconds;
  rv = ReadTwoDigits(input, 0u, 59u, seconds);
  if (rv != Success) {
    return rv;
  }

  uint8_t b;
  if (input.Read(b) != Success) {
    return Result::ERROR_INVALID_TIME;
  }
  if (b != 'Z') {
    return Result::ERROR_INVALID_TIME;
  }
  if (End(input) != Success) {
    return Result::ERROR_INVALID_TIME;
  }

  uint64_t totalSeconds = (static_cast<uint64_t>(days) * 24u * 60u * 60u) +
                          (static_cast<uint64_t>(hours)      * 60u * 60u) +
                          (static_cast<uint64_t>(minutes)          * 60u) +
                          seconds;

  time = TimeFromElapsedSecondsAD(totalSeconds);
  return Success;
}

} // namespace internal

} } } // namespace mozilla::pkix::der
