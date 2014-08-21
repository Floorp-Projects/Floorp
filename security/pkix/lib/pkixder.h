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
// matches the given criteria; they return Failure with the input mark in an
// undefined state if the input does not match the criteria.
//
// Match* functions advance the input mark and return true if the input matches
// the given criteria; they return false without changing the input mark if the
// input does not match the criteria.
//
// Skip* functions unconditionally advance the input mark and return Success if
// they are able to do so; otherwise they return Failure with the input mark in
// an undefined state.

#include "pkix/enumclass.h"
#include "pkix/nullptr.h"

#include "prerror.h"
#include "prlog.h"
#include "secder.h"
#include "secerr.h"
#include "secoidt.h"
#include "stdint.h"

typedef struct CERTSignedDataStr CERTSignedData;

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
  GENERALIZED_TIME = UNIVERSAL | 0x18,
  SEQUENCE = UNIVERSAL | CONSTRUCTED | 0x30,
};

enum Result
{
  Failure = -1,
  Success = 0
};

MOZILLA_PKIX_ENUM_CLASS EmptyAllowed { No = 0, Yes = 1 };

Result Fail(PRErrorCode errorCode);

class Input
{
public:
  Input()
    : input(nullptr)
    , end(nullptr)
  {
  }

  Result Init(const uint8_t* data, size_t len)
  {
    if (input) {
      // already initialized
      return Fail(SEC_ERROR_INVALID_ARGS);
    }
    if (!data || len > 0xffffu) {
      // input too large
      return Fail(SEC_ERROR_BAD_DER);
    }

    // XXX: this->input = input bug was not caught by tests! Why not?
    //      this->end = end bug was not caught by tests! Why not?
    this->input = data;
    this->end = data + len;

    return Success;
  }

  Result Expect(const uint8_t* expected, uint16_t expectedLen)
  {
    if (EnsureLength(expectedLen) != Success) {
      return Failure;
    }
    if (memcmp(input, expected, expectedLen)) {
      return Fail(SEC_ERROR_BAD_DER);
    }
    input += expectedLen;
    return Success;
  }

  bool Peek(uint8_t expectedByte) const
  {
    return input < end && *input == expectedByte;
  }

  Result Read(uint8_t& out)
  {
    if (EnsureLength(1) != Success) {
      return Failure;
    }
    out = *input++;
    return Success;
  }

  Result Read(uint16_t& out)
  {
    if (EnsureLength(2) != Success) {
      return Failure;
    }
    out = *input++;
    out <<= 8u;
    out |= *input++;
    return Success;
  }

  template <uint16_t N>
  bool MatchRest(const uint8_t (&toMatch)[N])
  {
    // Normally we use EnsureLength which compares (input + len < end), but
    // here we want to be sure that there is nothing following the matched
    // bytes
    if (static_cast<size_t>(end - input) != N) {
      return false;
    }
    if (memcmp(input, toMatch, N)) {
      return false;
    }
    input += N;
    return true;
  }

  template <uint16_t N>
  bool MatchTLV(uint8_t tag, uint16_t len, const uint8_t (&value)[N])
  {
    static_assert(N <= 127, "buffer larger than largest length supported");
    if (len > N) {
      PR_NOT_REACHED("overflow prevented dynamically instead of statically");
      return false;
    }
    uint16_t totalLen = 2u + len;
    if (EnsureLength(totalLen) != Success) {
      return false;
    }
    if (*input != tag) {
      return false;
    }
    if (*(input + 1) != len) {
      return false;
    }
    if (memcmp(input + 2, value, len)) {
      return false;
    }
    input += totalLen;
    return true;
  }

  Result Skip(uint16_t len)
  {
    if (EnsureLength(len) != Success) {
      return Failure;
    }
    input += len;
    return Success;
  }

  Result Skip(uint16_t len, Input& skippedInput)
  {
    if (EnsureLength(len) != Success) {
      return Failure;
    }
    if (skippedInput.Init(input, len) != Success) {
      return Failure;
    }
    input += len;
    return Success;
  }

  Result Skip(uint16_t len, SECItem& skippedItem)
  {
    if (EnsureLength(len) != Success) {
      return Failure;
    }
    skippedItem.type = siBuffer;
    skippedItem.data = const_cast<uint8_t*>(input);
    skippedItem.len = len;
    input += len;
    return Success;
  }

  void SkipToEnd()
  {
    input = end;
  }

  Result EnsureLength(uint16_t len)
  {
    if (static_cast<size_t>(end - input) < len) {
      return Fail(SEC_ERROR_BAD_DER);
    }
    return Success;
  }

  bool AtEnd() const { return input == end; }

  class Mark
  {
  private:
    friend class Input;
    Mark(const Input& input, const uint8_t* mark) : input(input), mark(mark) { }
    const Input& input;
    const uint8_t* const mark;
    void operator=(const Mark&) /* = delete */;
  };

  Mark GetMark() const { return Mark(*this, input); }

  Result GetSECItem(SECItemType type, const Mark& mark, /*out*/ SECItem& item)
  {
    if (&mark.input != this || mark.mark > input) {
      PR_NOT_REACHED("invalid mark");
      return Fail(SEC_ERROR_INVALID_ARGS);
    }
    item.type = type;
    item.data = const_cast<uint8_t*>(mark.mark);
    item.len = static_cast<decltype(item.len)>(input - mark.mark);
    return Success;
  }

private:
  const uint8_t* input;
  const uint8_t* end;

  Input(const Input&) /* = delete */;
  void operator=(const Input&) /* = delete */;
};

inline Result
ExpectTagAndLength(Input& input, uint8_t expectedTag, uint8_t expectedLength)
{
  PR_ASSERT((expectedTag & 0x1F) != 0x1F); // high tag number form not allowed
  PR_ASSERT(expectedLength < 128); // must be a single-byte length

  uint16_t tagAndLength;
  if (input.Read(tagAndLength) != Success) {
    return Failure;
  }

  uint16_t expectedTagAndLength = static_cast<uint16_t>(expectedTag << 8);
  expectedTagAndLength |= expectedLength;

  if (tagAndLength != expectedTagAndLength) {
    return Fail(SEC_ERROR_BAD_DER);
  }

  return Success;
}

namespace internal {

Result
ExpectTagAndGetLength(Input& input, uint8_t expectedTag, uint16_t& length);

} // namespace internal

inline Result
ExpectTagAndSkipLength(Input& input, uint8_t expectedTag)
{
  uint16_t ignored;
  return internal::ExpectTagAndGetLength(input, expectedTag, ignored);
}

inline Result
ExpectTagAndSkipValue(Input& input, uint8_t tag)
{
  uint16_t length;
  if (internal::ExpectTagAndGetLength(input, tag, length) != Success) {
    return Failure;
  }
  return input.Skip(length);
}

inline Result
ExpectTagAndGetValue(Input& input, uint8_t tag, /*out*/ SECItem& value)
{
  uint16_t length;
  if (internal::ExpectTagAndGetLength(input, tag, length) != Success) {
    return Failure;
  }
  return input.Skip(length, value);
}

inline Result
ExpectTagAndGetValue(Input& input, uint8_t tag, /*out*/ Input& value)
{
  uint16_t length;
  if (internal::ExpectTagAndGetLength(input, tag, length) != Success) {
    return Failure;
  }
  return input.Skip(length, value);
}

inline Result
End(Input& input)
{
  if (!input.AtEnd()) {
    return Fail(SEC_ERROR_BAD_DER);
  }

  return Success;
}

template <typename Decoder>
inline Result
Nested(Input& input, uint8_t tag, Decoder decoder)
{
  Input nested;
  if (ExpectTagAndGetValue(input, tag, nested) != Success) {
    return Failure;
  }
  if (decoder(nested) != Success) {
    return Failure;
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
  if (ExpectTagAndGetValue(input, outerTag, nestedInput) != Success) {
    return Failure;
  }
  if (Nested(nestedInput, innerTag, decoder) != Success) {
    return Failure;
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
  if (ExpectTagAndGetValue(input, outerTag, inner) != Success) {
    return Failure;
  }

  if (inner.AtEnd()) {
    if (mayBeEmpty != EmptyAllowed::Yes) {
      return Fail(SEC_ERROR_BAD_DER);
    }
    return Success;
  }

  do {
    if (Nested(inner, innerTag, decoder) != Success) {
      return Failure;
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
  if (ExpectTagAndLength(input, tag, 1) != Success) {
    return Failure;
  }
  uint8_t valueByte;
  if (input.Read(valueByte) != Success) {
    return Failure;
  }
  if (valueByte & 0x80) { // negative
    return Fail(SEC_ERROR_BAD_DER);
  }
  value = valueByte;
  return Success;
}

} // namespace internal

inline Result
Boolean(Input& input, /*out*/ bool& value)
{
  if (ExpectTagAndLength(input, BOOLEAN, 1) != Success) {
    return Failure;
  }

  uint8_t intValue;
  if (input.Read(intValue) != Success) {
    return Failure;
  }
  switch (intValue) {
    case 0: value = false; return Success;
    case 0xFF: value = true; return Success;
    default:
      return Fail(SEC_ERROR_BAD_DER);
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
    if (Boolean(input, value) != Success) {
      return Failure;
    }
    if (!allowInvalidExplicitEncoding && !value) {
      return Fail(SEC_ERROR_BAD_DER);
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

inline Result
GeneralizedTime(Input& input, PRTime& time)
{
  SECItem encoded;
  if (ExpectTagAndGetValue(input, GENERALIZED_TIME, encoded) != Success) {
    return Failure;
  }
  if (DER_GeneralizedTimeToTime(&time, &encoded) != SECSuccess) {
    return Failure;
  }
  return Success;
}

// This parser will only parse values between 0..127. If this range is
// increased then callers will need to be changed.
inline Result
Integer(Input& input, /*out*/ uint8_t& value)
{
  if (internal::IntegralValue(input, INTEGER, value) != Success) {
    return Failure;
  }
  return Success;
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
    return Fail(SEC_ERROR_INVALID_ARGS);
  }

  if (!input.Peek(INTEGER)) {
    value = defaultValue;
    return Success;
  }

  uint8_t parsedValue;
  if (Integer(input, parsedValue) != Success) {
    return Failure;
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
  if (ExpectTagAndLength(input, OIDTag, Len) != Success) {
    return Failure;
  }

  return input.Expect(expectedOid, Len);
}

// PKI-specific types

// AlgorithmIdentifier  ::=  SEQUENCE  {
//         algorithm               OBJECT IDENTIFIER,
//         parameters              ANY DEFINED BY algorithm OPTIONAL  }
inline Result
AlgorithmIdentifier(Input& input, SECAlgorithmID& algorithmID)
{
  if (ExpectTagAndGetValue(input, OIDTag, algorithmID.algorithm) != Success) {
    return Failure;
  }
  algorithmID.parameters.data = nullptr;
  algorithmID.parameters.len = 0;
  if (input.AtEnd()) {
    return Success;
  }
  return Null(input);
}

inline Result
CertificateSerialNumber(Input& input, /*out*/ SECItem& value)
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

  if (ExpectTagAndGetValue(input, INTEGER, value) != Success) {
    return Failure;
  }

  if (value.len == 0) {
    return Fail(SEC_ERROR_BAD_DER);
  }

  // Check for overly-long encodings. If the first byte is 0x00 then the high
  // bit on the second byte must be 1; otherwise the same *positive* value
  // could be encoded without the leading 0x00 byte. If the first byte is 0xFF
  // then the second byte must NOT have its high bit set; otherwise the same
  // *negative* value could be encoded without the leading 0xFF byte.
  if (value.len > 1) {
    if ((value.data[0] == 0x00 && (value.data[1] & 0x80) == 0) ||
        (value.data[0] == 0xff && (value.data[1] & 0x80) != 0)) {
      return Fail(SEC_ERROR_BAD_DER);
    }
  }

  return Success;
}

// x.509 and OCSP both use this same version numbering scheme, though OCSP
// only supports v1.
enum Version { v1 = 0, v2 = 1, v3 = 2, v4 = 3 };

// X.509 Certificate and OCSP ResponseData both use this
// "[0] EXPLICIT Version DEFAULT <defaultVersion>" construct, but with
// different default versions.
inline Result
OptionalVersion(Input& input, /*out*/ uint8_t& version)
{
  const uint8_t tag = CONTEXT_SPECIFIC | CONSTRUCTED | 0;
  if (!input.Peek(tag)) {
    version = v1;
    return Success;
  }
  if (ExpectTagAndLength(input, tag, 3) != Success) {
    return Failure;
  }
  if (ExpectTagAndLength(input, INTEGER, 1) != Success) {
    return Failure;
  }
  if (input.Read(version) != Success) {
    return Failure;
  }
  if (version & 0x80) { // negative
    return Fail(SEC_ERROR_BAD_DER);
  }
  return Success;
}

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
Result
SignedData(Input& input, /*out*/ Input& tbs, /*out*/ CERTSignedData& signedData);

} } } // namespace mozilla::pkix::der

#endif // mozilla_pkix__pkixder_h
