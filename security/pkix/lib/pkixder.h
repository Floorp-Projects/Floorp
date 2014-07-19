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

#ifndef mozilla_pkix__pkixder_h
#define mozilla_pkix__pkixder_h

// Expect* functions advance the input mark and return Success if the input
// matches the given criteria; they fail with the input mark in an undefined
// state if the input does not match the criteria.
//
// Match* functions advance the input mark and return true if the input matches
// the given criteria; they return false without changing the input mark if the
// input does not match the criteria.
//
// Skip* functions unconditionally advance the input mark and return Success if
// they are able to do so; otherwise they fail with the input mark in an
// undefined state.

#include "pkix/Input.h"
#include "pkix/pkixtypes.h"
#include "prtime.h"

namespace mozilla { namespace pkix { namespace der {

enum Class
{
   UNIVERSAL = 0 << 6,
// APPLICATION = 1 << 6, // unused
   CONTEXT_SPECIFIC = 2 << 6,
// PRIVATE = 3 << 6 // unused
};

enum Constructed
{
  CONSTRUCTED = 1 << 5
};

enum Tag
{
  BOOLEAN = UNIVERSAL | 0x01,
  INTEGER = UNIVERSAL | 0x02,
  BIT_STRING = UNIVERSAL | 0x03,
  OCTET_STRING = UNIVERSAL | 0x04,
  NULLTag = UNIVERSAL | 0x05,
  OIDTag = UNIVERSAL | 0x06,
  ENUMERATED = UNIVERSAL | 0x0a,
  SEQUENCE = UNIVERSAL | CONSTRUCTED | 0x10, // 0x30
  UTCTime = UNIVERSAL | 0x17,
  GENERALIZED_TIME = UNIVERSAL | 0x18,
};

MOZILLA_PKIX_ENUM_CLASS EmptyAllowed { No = 0, Yes = 1 };

inline Result
ExpectTagAndLength(Input& input, uint8_t expectedTag, uint8_t expectedLength)
{
  PR_ASSERT((expectedTag & 0x1F) != 0x1F); // high tag number form not allowed
  PR_ASSERT(expectedLength < 128); // must be a single-byte length

  uint16_t tagAndLength;
  Result rv = input.Read(tagAndLength);
  if (rv != Success) {
    return rv;
  }

  uint16_t expectedTagAndLength = static_cast<uint16_t>(expectedTag << 8);
  expectedTagAndLength |= expectedLength;

  if (tagAndLength != expectedTagAndLength) {
    return Result::ERROR_BAD_DER;
  }

  return Success;
}

namespace internal {

Result
ExpectTagAndGetLength(Input& input, uint8_t expectedTag, uint16_t& length);

} // namespace internal

inline Result
ExpectTagAndSkipValue(Input& input, uint8_t tag)
{
  uint16_t length;
  Result rv = internal::ExpectTagAndGetLength(input, tag, length);
  if (rv != Success) {
    return rv;
  }
  return input.Skip(length);
}

inline Result
ExpectTagAndGetValue(Input& input, uint8_t tag, /*out*/ InputBuffer& value)
{
  uint16_t length;
  Result rv = internal::ExpectTagAndGetLength(input, tag, length);
  if (rv != Success) {
    return rv;
  }
  return input.Skip(length, value);
}

inline Result
ExpectTagAndGetValue(Input& input, uint8_t tag, /*out*/ Input& value)
{
  uint16_t length;
  Result rv = internal::ExpectTagAndGetLength(input, tag, length);
  if (rv != Success) {
    return rv;
  }
  return input.Skip(length, value);
}

// Like ExpectTagAndGetValue, except the output InputBuffer will contain the
// encoded tag and length along with the value.
inline Result
ExpectTagAndGetTLV(Input& input, uint8_t tag, /*out*/ InputBuffer& tlv)
{
  Input::Mark mark(input.GetMark());
  uint16_t length;
  Result rv = internal::ExpectTagAndGetLength(input, tag, length);
  if (rv != Success) {
    return rv;
  }
  rv = input.Skip(length);
  if (rv != Success) {
    return rv;
  }
  return input.GetInputBuffer(mark, tlv);
}

inline Result
End(Input& input)
{
  if (!input.AtEnd()) {
    return Result::ERROR_BAD_DER;
  }

  return Success;
}

template <typename Decoder>
inline Result
Nested(Input& input, uint8_t tag, Decoder decoder)
{
  Input nested;
  Result rv = ExpectTagAndGetValue(input, tag, nested);
  if (rv != Success) {
    return rv;
  }
  rv = decoder(nested);
  if (rv != Success) {
    return rv;
  }
  return End(nested);
}

template <typename Decoder>
inline Result
Nested(Input& input, uint8_t outerTag, uint8_t innerTag, Decoder decoder)
{
  // XXX: This doesn't work (in VS2010):
  // return Nested(input, outerTag, bind(Nested, _1, innerTag, decoder));

  Input nestedInput;
  Result rv = ExpectTagAndGetValue(input, outerTag, nestedInput);
  if (rv != Success) {
    return rv;
  }
  rv = Nested(nestedInput, innerTag, decoder);
  if (rv != Success) {
    return rv;
  }
  return End(nestedInput);
}

// This can be used to decode constructs like this:
//
//     ...
//     foos SEQUENCE OF Foo,
//     ...
//     Foo ::= SEQUENCE {
//     }
//
// using a call like this:
//
//    rv = NestedOf(input, SEQEUENCE, SEQUENCE, bind(_1, Foo));
//
//    Result Foo(Input& input) {
//    }
//
// In this example, Foo will get called once for each element of foos.
//
template <typename Decoder>
inline Result
NestedOf(Input& input, uint8_t outerTag, uint8_t innerTag,
         EmptyAllowed mayBeEmpty, Decoder decoder)
{
  Input inner;
  Result rv = ExpectTagAndGetValue(input, outerTag, inner);
  if (rv != Success) {
    return rv;
  }

  if (inner.AtEnd()) {
    if (mayBeEmpty != EmptyAllowed::Yes) {
      return Result::ERROR_BAD_DER;
    }
    return Success;
  }

  do {
    rv = Nested(inner, innerTag, decoder);
    if (rv != Success) {
      return rv;
    }
  } while (!inner.AtEnd());

  return Success;
}

// Universal types

namespace internal {

// This parser will only parse values between 0..127. If this range is
// increased then callers will need to be changed.
template <typename T> inline Result
IntegralValue(Input& input, uint8_t tag, T& value)
{
  // Conveniently, all the Integers that we actually have to be able to parse
  // are positive and very small. Consequently, this parser is *much* simpler
  // than a general Integer parser would need to be.
  Result rv = ExpectTagAndLength(input, tag, 1);
  if (rv != Success) {
    return rv;
  }
  uint8_t valueByte;
  rv = input.Read(valueByte);
  if (rv != Success) {
    return rv;
  }
  if (valueByte & 0x80) { // negative
    return Result::ERROR_BAD_DER;
  }
  value = valueByte;
  return Success;
}

} // namespace internal

Result
BitStringWithNoUnusedBits(Input& input, /*out*/ InputBuffer& value);

inline Result
Boolean(Input& input, /*out*/ bool& value)
{
  Result rv = ExpectTagAndLength(input, BOOLEAN, 1);
  if (rv != Success) {
    return rv;
  }

  uint8_t intValue;
  rv = input.Read(intValue);
  if (rv != Success) {
    return rv;
  }
  switch (intValue) {
    case 0: value = false; return Success;
    case 0xFF: value = true; return Success;
    default:
      return Result::ERROR_BAD_DER;
  }
}

// This is for any BOOLEAN DEFAULT FALSE.
// (If it is present and false, this is a bad encoding.)
// TODO(bug 989518): For compatibility reasons, in some places we allow
// invalid encodings with the explicit default value.
inline Result
OptionalBoolean(Input& input, bool allowInvalidExplicitEncoding,
                /*out*/ bool& value)
{
  value = false;
  if (input.Peek(BOOLEAN)) {
    Result rv = Boolean(input, value);
    if (rv != Success) {
      return rv;
    }
    if (!allowInvalidExplicitEncoding && !value) {
      return Result::ERROR_BAD_DER;
    }
  }
  return Success;
}

// This parser will only parse values between 0..127. If this range is
// increased then callers will need to be changed.
inline Result
Enumerated(Input& input, uint8_t& value)
{
  return internal::IntegralValue(input, ENUMERATED | 0, value);
}

namespace internal {

// internal::TimeChoice implements the shared functionality of GeneralizedTime
// and TimeChoice. tag must be either UTCTime or GENERALIZED_TIME.
//
// Only times from 1970-01-01-00:00:00 onward are accepted, in order to
// eliminate the chance for complications in converting times to traditional
// time formats that start at 1970.
Result TimeChoice(Input& input, uint8_t tag, /*out*/ PRTime& time);

} // namespace internal

// Only times from 1970-01-01-00:00:00 onward are accepted, in order to
// eliminate the chance for complications in converting times to traditional
// time formats that start at 1970.
inline Result
GeneralizedTime(Input& input, /*out*/ PRTime& time)
{
  return internal::TimeChoice(input, GENERALIZED_TIME, time);
}

// Only times from 1970-01-01-00:00:00 onward are accepted, in order to
// eliminate the chance for complications in converting times to traditional
// time formats that start at 1970.
inline Result
TimeChoice(Input& input, /*out*/ PRTime& time)
{
  uint8_t expectedTag = input.Peek(UTCTime) ? UTCTime : GENERALIZED_TIME;
  return internal::TimeChoice(input, expectedTag, time);
}

// This parser will only parse values between 0..127. If this range is
// increased then callers will need to be changed.
inline Result
Integer(Input& input, /*out*/ uint8_t& value)
{
  return internal::IntegralValue(input, INTEGER, value);
}

// This parser will only parse values between 0..127. If this range is
// increased then callers will need to be changed. The default value must be
// -1; defaultValue is only a parameter to make it clear in the calling code
// what the default value is.
inline Result
OptionalInteger(Input& input, long defaultValue, /*out*/ long& value)
{
  // If we need to support a different default value in the future, we need to
  // test that parsedValue != defaultValue.
  if (defaultValue != -1) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }

  if (!input.Peek(INTEGER)) {
    value = defaultValue;
    return Success;
  }

  uint8_t parsedValue;
  Result rv = Integer(input, parsedValue);
  if (rv != Success) {
    return rv;
  }
  value = parsedValue;
  return Success;
}

inline Result
Null(Input& input)
{
  return ExpectTagAndLength(input, NULLTag, 0);
}

template <uint8_t Len>
Result
OID(Input& input, const uint8_t (&expectedOid)[Len])
{
  Input value;
  Result rv = ExpectTagAndGetValue(input, OIDTag, value);
  if (rv != Success) {
    return rv;
  }
  if (!value.MatchRest(expectedOid)) {
    return Result::ERROR_BAD_DER;
  }
  return Success;
}

// PKI-specific types

inline Result
CertificateSerialNumber(Input& input, /*out*/ InputBuffer& value)
{
  // http://tools.ietf.org/html/rfc5280#section-4.1.2.2:
  //
  // * "The serial number MUST be a positive integer assigned by the CA to
  //   each certificate."
  // * "Certificate users MUST be able to handle serialNumber values up to 20
  //   octets. Conforming CAs MUST NOT use serialNumber values longer than 20
  //   octets."
  // * "Note: Non-conforming CAs may issue certificates with serial numbers
  //   that are negative or zero.  Certificate users SHOULD be prepared to
  //   gracefully handle such certificates."

  Result rv = ExpectTagAndGetValue(input, INTEGER, value);
  if (rv != Success) {
    return rv;
  }

  if (value.GetLength() == 0) {
    return Result::ERROR_BAD_DER;
  }

  // Check for overly-long encodings. If the first byte is 0x00 then the high
  // bit on the second byte must be 1; otherwise the same *positive* value
  // could be encoded without the leading 0x00 byte. If the first byte is 0xFF
  // then the second byte must NOT have its high bit set; otherwise the same
  // *negative* value could be encoded without the leading 0xFF byte.
  if (value.GetLength() > 1) {
    Input valueInput(value);
    uint8_t firstByte;
    rv = valueInput.Read(firstByte);
    if (rv != Success) {
      return rv;
    }
    uint8_t secondByte;
    rv = valueInput.Read(secondByte);
    if (rv != Success) {
      return rv;
    }
    if ((firstByte == 0x00 && (secondByte & 0x80) == 0) ||
        (firstByte == 0xff && (secondByte & 0x80) != 0)) {
      return Result::ERROR_BAD_DER;
    }
  }

  return Success;
}

// x.509 and OCSP both use this same version numbering scheme, though OCSP
// only supports v1.
MOZILLA_PKIX_ENUM_CLASS Version { v1 = 0, v2 = 1, v3 = 2 };

// X.509 Certificate and OCSP ResponseData both use this
// "[0] EXPLICIT Version DEFAULT <defaultVersion>" construct, but with
// different default versions.
inline Result
OptionalVersion(Input& input, /*out*/ Version& version)
{
  static const uint8_t TAG = CONTEXT_SPECIFIC | CONSTRUCTED | 0;
  if (!input.Peek(TAG)) {
    version = Version::v1;
    return Success;
  }
  Input value;
  Result rv = ExpectTagAndGetValue(input, TAG, value);
  if (rv != Success) {
    return rv;
  }
  uint8_t integerValue;
  rv = Integer(value, integerValue);
  if (rv != Success) {
    return rv;
  }
  rv = End(value);
  if (rv != Success) {
    return rv;
  }
  switch (integerValue) {
    case static_cast<uint8_t>(Version::v3): version = Version::v3; break;
    case static_cast<uint8_t>(Version::v2): version = Version::v2; break;
    // XXX(bug 1031093): We shouldn't accept an explicit encoding of v1, but we
    // do here for compatibility reasons.
    case static_cast<uint8_t>(Version::v1): version = Version::v1; break;
    default:
      return Result::ERROR_BAD_DER;
  }
  return Success;
}

template <typename ExtensionHandler>
inline Result
OptionalExtensions(Input& input, uint8_t tag, ExtensionHandler extensionHandler)
{
  if (!input.Peek(tag)) {
    return Success;
  }

  Result rv;

  Input extensions;
  {
    Input tagged;
    rv = ExpectTagAndGetValue(input, tag, tagged);
    if (rv != Success) {
      return rv;
    }
    rv = ExpectTagAndGetValue(tagged, SEQUENCE, extensions);
    if (rv != Success) {
      return rv;
    }
    rv = End(tagged);
    if (rv != Success) {
      return rv;
    }
  }

  // Extensions ::= SEQUENCE SIZE (1..MAX) OF Extension
  //
  // TODO(bug 997994): According to the specification, there should never be
  // an empty sequence of extensions but we've found OCSP responses that have
  // that (see bug 991898).
  while (!extensions.AtEnd()) {
    Input extension;
    rv = ExpectTagAndGetValue(extensions, SEQUENCE, extension);
    if (rv != Success) {
      return rv;
    }

    // Extension  ::=  SEQUENCE  {
    //      extnID      OBJECT IDENTIFIER,
    //      critical    BOOLEAN DEFAULT FALSE,
    //      extnValue   OCTET STRING
    //      }
    Input extnID;
    rv = ExpectTagAndGetValue(extension, OIDTag, extnID);
    if (rv != Success) {
      return rv;
    }
    bool critical;
    rv = OptionalBoolean(extension, false, critical);
    if (rv != Success) {
      return rv;
    }
    InputBuffer extnValue;
    rv = ExpectTagAndGetValue(extension, OCTET_STRING, extnValue);
    if (rv != Success) {
      return rv;
    }
    rv = End(extension);
    if (rv != Success) {
      return rv;
    }

    bool understood = false;
    rv = extensionHandler(extnID, extnValue, understood);
    if (rv != Success) {
      return rv;
    }
    if (critical && !understood) {
      return Result::ERROR_UNKNOWN_CRITICAL_EXTENSION;
    }
  }

  return Success;
}

Result DigestAlgorithmIdentifier(Input& input,
                                 /*out*/ DigestAlgorithm& algorithm);

Result SignatureAlgorithmIdentifier(Input& input,
                                    /*out*/ SignatureAlgorithm& algorithm);

// Parses a SEQUENCE into tbs and then parses an AlgorithmIdentifier followed
// by a BIT STRING into signedData. This handles the commonality between
// parsing the signed/signature fields of certificates and OCSP responses. In
// the case of an OCSP response, the caller needs to parse the certs
// separately.
//
// Certificate  ::=  SEQUENCE  {
//        tbsCertificate       TBSCertificate,
//        signatureAlgorithm   AlgorithmIdentifier,
//        signatureValue       BIT STRING  }
//
// BasicOCSPResponse       ::= SEQUENCE {
//    tbsResponseData      ResponseData,
//    signatureAlgorithm   AlgorithmIdentifier,
//    signature            BIT STRING,
//    certs            [0] EXPLICIT SEQUENCE OF Certificate OPTIONAL }
Result SignedData(Input& input, /*out*/ Input& tbs,
                  /*out*/ SignedDataWithSignature& signedDataWithSignature);

} } } // namespace mozilla::pkix::der

#endif // mozilla_pkix__pkixder_h
