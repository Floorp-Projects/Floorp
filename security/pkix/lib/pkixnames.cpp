/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2014 Mozilla Contributors
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

// This code attempts to implement RFC6125 name matching.
//
// In this code, identifiers are classified as either "presented" or
// "reference" identifiers are defined in
// http://tools.ietf.org/html/rfc6125#section-1.8. A "presented identifier" is
// one in the subjectAltName of the certificate, or sometimes within a CN of
// the certificate's subject. The "reference identifier" is the one we are
// being asked to match the certificate against.
//
// On Windows and maybe other platforms, OS-provided IP address parsing
// functions might fail if the protocol (IPv4 or IPv6) has been disabled, so we
// can't rely on them.

#include "pkix/bind.h"
#include "pkixutil.h"

namespace mozilla { namespace pkix {

namespace {

// GeneralName ::= CHOICE {
//      otherName                       [0]     OtherName,
//      rfc822Name                      [1]     IA5String,
//      dNSName                         [2]     IA5String,
//      x400Address                     [3]     ORAddress,
//      directoryName                   [4]     Name,
//      ediPartyName                    [5]     EDIPartyName,
//      uniformResourceIdentifier       [6]     IA5String,
//      iPAddress                       [7]     OCTET STRING,
//      registeredID                    [8]     OBJECT IDENTIFIER }
MOZILLA_PKIX_ENUM_CLASS GeneralNameType : uint8_t
{
  dNSName = der::CONTEXT_SPECIFIC | 2,
  iPAddress = der::CONTEXT_SPECIFIC | 7,
};

MOZILLA_PKIX_ENUM_CLASS FallBackToCommonName { No = 0, Yes = 1 };

Result SearchForName(const Input* subjectAltName, Input subject,
                     GeneralNameType referenceIDType,
                     Input referenceID,
                     FallBackToCommonName fallBackToCommonName,
                     /*out*/ bool& foundMatch);
Result SearchWithinRDN(Reader& rdn,
                       GeneralNameType referenceIDType,
                       Input referenceID,
                       /*in/out*/ bool& foundMatch);
Result SearchWithinAVA(Reader& rdn,
                       GeneralNameType referenceIDType,
                       Input referenceID,
                       /*in/out*/ bool& foundMatch);

Result MatchPresentedIDWithReferenceID(GeneralNameType referenceIDType,
                                       Input presentedID,
                                       Input referenceID,
                                       /*out*/ bool& foundMatch);

uint8_t LocaleInsensitveToLower(uint8_t a);
bool StartsWithIDNALabel(Input id);

MOZILLA_PKIX_ENUM_CLASS ValidDNSIDMatchType
{
  ReferenceID = 0,
  PresentedID = 1,
};

bool IsValidDNSID(Input hostname, ValidDNSIDMatchType matchType);

} // unnamed namespace

bool IsValidReferenceDNSID(Input hostname);
bool IsValidPresentedDNSID(Input hostname);
bool ParseIPv4Address(Input hostname, /*out*/ uint8_t (&out)[4]);
bool ParseIPv6Address(Input hostname, /*out*/ uint8_t (&out)[16]);
bool PresentedDNSIDMatchesReferenceDNSID(Input presentedDNSID,
                                         Input referenceDNSID);

// Verify that the given end-entity cert, which is assumed to have been already
// validated with BuildCertChain, is valid for the given hostname. hostname is
// assumed to be a string representation of an IPv4 address, an IPv6 addresss,
// or a normalized ASCII (possibly punycode) DNS name.
Result
CheckCertHostname(Input endEntityCertDER, Input hostname)
{
  BackCert cert(endEntityCertDER, EndEntityOrCA::MustBeEndEntity, nullptr);
  Result rv = cert.Init();
  if (rv != Success) {
    return rv;
  }

  const Input* subjectAltName(cert.GetSubjectAltName());
  Input subject(cert.GetSubject());

  // For backward compatibility with legacy certificates, we fall back to
  // searching for a name match in the subject common name for DNS names and
  // IPv4 addresses. We don't do so for IPv6 addresses because we do not think
  // there are many certificates that would need such fallback, and because
  // comparisons of string representations of IPv6 addresses are particularly
  // error prone due to the syntactic flexibility that IPv6 addresses have.
  //
  // IPv4 and IPv6 addresses are represented using the same type of GeneralName
  // (iPAddress); they are differentiated by the lengths of the values.
  bool found;
  uint8_t ipv6[16];
  uint8_t ipv4[4];
  if (IsValidReferenceDNSID(hostname)) {
    rv = SearchForName(subjectAltName, subject, GeneralNameType::dNSName,
                       hostname, FallBackToCommonName::Yes, found);
  } else if (ParseIPv6Address(hostname, ipv6)) {
    rv = SearchForName(subjectAltName, subject, GeneralNameType::iPAddress,
                       Input(ipv6), FallBackToCommonName::No, found);
  } else if (ParseIPv4Address(hostname, ipv4)) {
    rv = SearchForName(subjectAltName, subject, GeneralNameType::iPAddress,
                       Input(ipv4), FallBackToCommonName::Yes, found);
  } else {
    return Result::ERROR_BAD_CERT_DOMAIN;
  }
  if (rv != Success) {
    return rv;
  }
  if (!found) {
    return Result::ERROR_BAD_CERT_DOMAIN;
  }
  return Success;
}

namespace {

Result
SearchForName(/*optional*/ const Input* subjectAltName,
              Input subject,
              GeneralNameType referenceIDType,
              Input referenceID,
              FallBackToCommonName fallBackToCommonName,
              /*out*/ bool& foundMatch)
{
  Result rv;

  foundMatch = false;

  // RFC 6125 says "A client MUST NOT seek a match for a reference identifier
  // of CN-ID if the presented identifiers include a DNS-ID, SRV-ID, URI-ID, or
  // any application-specific identifier types supported by the client."
  // Accordingly, we only consider CN-IDs if there are no DNS-IDs in the
  // subjectAltName.
  //
  // RFC 6125 says that IP addresses are out of scope, but for backward
  // compatibility we accept them, by considering IP addresses to be an
  // "application-specific identifier type supported by the client."
  //
  // TODO(bug XXXXXXX): Consider strengthening this check to "A client MUST NOT
  // seek a match for a reference identifier of CN-ID if the certificate
  // contains a subjectAltName extension."
  //
  // TODO(bug XXXXXXX): Consider dropping support for IP addresses as
  // identifiers completely.
  bool hasAtLeastOneDNSNameOrIPAddressSAN = false;

  if (subjectAltName) {
    Reader altNames;
    rv = der::ExpectTagAndGetValueAtEnd(*subjectAltName, der::SEQUENCE,
                                        altNames);
    if (rv != Success) {
      return rv;
    }

    // do { ... } while(...) because subjectAltName isn't allowed to be empty.
    do {
      uint8_t tag;
      Input presentedID;
      rv = der::ReadTagAndGetValue(altNames, tag, presentedID);
      if (rv != Success) {
        return rv;
      }
      if (tag == static_cast<uint8_t>(referenceIDType)) {
        rv = MatchPresentedIDWithReferenceID(referenceIDType, presentedID,
                                             referenceID, foundMatch);
        if (rv != Success) {
          return rv;
        }
        if (foundMatch) {
          return Success;
        }
      }
      if (tag == static_cast<uint8_t>(GeneralNameType::dNSName) ||
          tag == static_cast<uint8_t>(GeneralNameType::iPAddress)) {
        hasAtLeastOneDNSNameOrIPAddressSAN = true;
      }
    } while (!altNames.AtEnd());
  }

  if (hasAtLeastOneDNSNameOrIPAddressSAN ||
      fallBackToCommonName != FallBackToCommonName::Yes) {
    return Success;
  }

  // Attempt to match the reference ID against the CN-ID, which we consider to
  // be the most-specific CN AVA in the subject field.
  //
  // https://tools.ietf.org/html/rfc6125#section-2.3.1 says:
  //
  //   To reduce confusion, in this specification we avoid such terms and
  //   instead use the terms provided under Section 1.8; in particular, we
  //   do not use the term "(most specific) Common Name field in the subject
  //   field" from [HTTP-TLS] and instead state that a CN-ID is a Relative
  //   Distinguished Name (RDN) in the certificate subject containing one
  //   and only one attribute-type-and-value pair of type Common Name (thus
  //   removing the possibility that an RDN might contain multiple AVAs
  //   (Attribute Value Assertions) of type CN, one of which could be
  //   considered "most specific").
  //
  // https://tools.ietf.org/html/rfc6125#section-7.4 says:
  //
  //   [...] Although it would be preferable to
  //   forbid multiple CN-IDs entirely, there are several reasons at this
  //   time why this specification states that they SHOULD NOT (instead of
  //   MUST NOT) be included [...]
  //
  // Consequently, it is unclear what to do when there are multiple CNs in the
  // subject, regardless of whether there "SHOULD NOT" be.
  //
  // NSS's CERT_VerifyCertName mostly follows RFC2818 in this instance, which
  // says:
  //
  //   If a subjectAltName extension of type dNSName is present, that MUST
  //   be used as the identity. Otherwise, the (most specific) Common Name
  //   field in the Subject field of the certificate MUST be used.
  //
  //   [...]
  //
  //   In some cases, the URI is specified as an IP address rather than a
  //   hostname. In this case, the iPAddress subjectAltName must be present
  //   in the certificate and must exactly match the IP in the URI.
  //
  // (The main difference from RFC2818 is that NSS's CERT_VerifyCertName also
  // matches IP addresses in the most-specific CN.)
  //
  // NSS's CERT_VerifyCertName finds the most specific CN via
  // CERT_GetCommoName, which uses CERT_GetLastNameElement. Note that many
  // NSS-based applications, including Gecko, also use CERT_GetCommonName. It
  // is likely that other, non-NSS-based, applications also expect only the
  // most specific CN to be matched against the reference ID.
  //
  // "A Layman's Guide to a Subset of ASN.1, BER, and DER" and other sources
  // agree that an RDNSequence is ordered from most significant (least
  // specific) to least significant (most specific), as do other references.
  //
  // However, Chromium appears to use the least-specific (first) CN instead of
  // the most-specific; see https://crbug.com/366957. Also, MSIE and some other
  // popular implementations apparently attempt to match the reference ID
  // against any/all CNs in the subject. Since we're trying to phase out the
  // use of CN-IDs, we intentionally avoid trying to match MSIE's more liberal
  // behavior.

  // Name ::= CHOICE { -- only one possibility for now --
  //   rdnSequence  RDNSequence }
  //
  // RDNSequence ::= SEQUENCE OF RelativeDistinguishedName
  //
  // RelativeDistinguishedName ::=
  //   SET SIZE (1..MAX) OF AttributeTypeAndValue
  Reader subjectReader(subject);
  return der::NestedOf(subjectReader, der::SEQUENCE, der::SET,
                       der::EmptyAllowed::Yes,
                       bind(SearchWithinRDN, _1, referenceIDType,
                            referenceID, ref(foundMatch)));
}

// RelativeDistinguishedName ::=
//   SET SIZE (1..MAX) OF AttributeTypeAndValue
//
// AttributeTypeAndValue ::= SEQUENCE {
//   type     AttributeType,
//   value    AttributeValue }
Result
SearchWithinRDN(Reader& rdn,
                GeneralNameType referenceIDType,
                Input referenceID,
                /*in/out*/ bool& foundMatch)
{
  do {
    Result rv = der::Nested(rdn, der::SEQUENCE,
                            bind(SearchWithinAVA, _1, referenceIDType,
                                 referenceID, ref(foundMatch)));
    if (rv != Success) {
      return rv;
    }
  } while (!rdn.AtEnd());

  return Success;
}

// AttributeTypeAndValue ::= SEQUENCE {
//   type     AttributeType,
//   value    AttributeValue }
//
// AttributeType ::= OBJECT IDENTIFIER
//
// AttributeValue ::= ANY -- DEFINED BY AttributeType
//
// DirectoryString ::= CHOICE {
//       teletexString           TeletexString (SIZE (1..MAX)),
//       printableString         PrintableString (SIZE (1..MAX)),
//       universalString         UniversalString (SIZE (1..MAX)),
//       utf8String              UTF8String (SIZE (1..MAX)),
//       bmpString               BMPString (SIZE (1..MAX)) }
Result
SearchWithinAVA(Reader& rdn,
                GeneralNameType referenceIDType,
                Input referenceID,
                /*in/out*/ bool& foundMatch)
{
  // id-at OBJECT IDENTIFIER ::= { joint-iso-ccitt(2) ds(5) 4 }
  // id-at-commonName AttributeType ::= { id-at 3 }
  // python DottedOIDToCode.py id-at-commonName 2.5.4.3
  static const uint8_t id_at_commonName[] = {
    0x55, 0x04, 0x03
  };

  // AttributeTypeAndValue ::= SEQUENCE {
  //   type     AttributeType,
  //   value    AttributeValue }
  //
  // AttributeType ::= OBJECT IDENTIFIER
  //
  // AttributeValue ::= ANY -- DEFINED BY AttributeType
  //
  // DirectoryString ::= CHOICE {
  //       teletexString           TeletexString (SIZE (1..MAX)),
  //       printableString         PrintableString (SIZE (1..MAX)),
  //       universalString         UniversalString (SIZE (1..MAX)),
  //       utf8String              UTF8String (SIZE (1..MAX)),
  //       bmpString               BMPString (SIZE (1..MAX)) }
  Reader type;
  Result rv = der::ExpectTagAndGetValue(rdn, der::OIDTag, type);
  if (rv != Success) {
    return rv;
  }

  // We're only interested in CN attributes.
  if (!type.MatchRest(id_at_commonName)) {
    rdn.SkipToEnd();
    return Success;
  }

  // We might have previously found a match. Now that we've found another CN,
  // we no longer consider that previous match to be a match, so "forget" about
  // it.
  foundMatch = false;

  uint8_t valueEncodingTag;
  Input presentedID;
  rv = der::ReadTagAndGetValue(rdn, valueEncodingTag, presentedID);
  if (rv != Success) {
    return rv;
  }

  // PrintableString is a subset of ASCII that contains all the characters
  // allowed in CN-IDs except '*'. Although '*' is illegal, there are many
  // real-world certificates that are encoded this way, so we accept it.
  //
  // In the case of UTF8String, we rely on the fact that in UTF-8 the octets in
  // a multi-byte encoding of a code point are always distinct from ASCII. Any
  // non-ASCII byte in a UTF-8 string causes us to fail to match. We make no
  // attempt to detect or report malformed UTF-8 (e.g. incomplete or overlong
  // encodings of code points, or encodings of invalid code points).
  //
  // TeletexString is supported as long as it does not contain any escape
  // sequences, which are not supported. We'll reject escape sequences as
  // invalid characters in names, which means we only accept strings that are
  // in the default character set, which is a superset of ASCII. Note that NSS
  // actually treats TeletexString as ISO-8859-1. Many certificates that have
  // wildcard CN-IDs (e.g. "*.example.com") use TeletexString because
  // PrintableString is defined to not allow '*' and because, at one point in
  // history, UTF8String was too new to use for compatibility reasons.
  //
  // UniversalString and BMPString are also deprecated, and they are a little
  // harder to support because they are not single-byte ASCII superset
  // encodings, so we don't bother.
  if (valueEncodingTag != der::PrintableString &&
      valueEncodingTag != der::UTF8String &&
      valueEncodingTag != der::TeletexString) {
    return Success;
  }

  switch (referenceIDType)
  {
    case GeneralNameType::dNSName:
      foundMatch = PresentedDNSIDMatchesReferenceDNSID(presentedID,
                                                       referenceID);
      break;
    case GeneralNameType::iPAddress:
    {
      // We don't fall back to matching CN-IDs for IPv6 addresses, so we'll
      // never get here for an IPv6 address.
      assert(referenceID.GetLength() == 4);
      uint8_t ipv4[4];
      foundMatch = ParseIPv4Address(presentedID, ipv4) &&
                   InputsAreEqual(Input(ipv4), referenceID);
      break;
    }
    default:
      return NotReached("unexpected referenceIDType in SearchWithinAVA",
                        Result::FATAL_ERROR_INVALID_ARGS);
  }

  return Success;
}

Result
MatchPresentedIDWithReferenceID(GeneralNameType nameType,
                                Input presentedID,
                                Input referenceID,
                                /*out*/ bool& foundMatch)
{
  foundMatch = false;

  switch (nameType) {
    case GeneralNameType::dNSName:
      foundMatch = PresentedDNSIDMatchesReferenceDNSID(presentedID,
                                                       referenceID);
      break;
    case GeneralNameType::iPAddress:
      foundMatch = InputsAreEqual(presentedID, referenceID);
      break;
    default:
      return NotReached("Invalid nameType for SearchType::CheckName",
                        Result::FATAL_ERROR_INVALID_ARGS);
  }
  return Success;
}

} // unnamed namespace

// We do not distinguish between a syntactically-invalid presentedDNSID and one
// that is syntactically valid but does not match referenceDNSID; in both
// cases, the result is false.
//
// We assume that both presentedDNSID and referenceDNSID are encoded in such a
// way that US-ASCII (7-bit) characters are encoded in one byte and no encoding
// of a non-US-ASCII character contains a code point in the range 0-127. For
// example, UTF-8 is OK but UTF-16 is not.
//
// RFC6125 says that a wildcard label may be of the form <x>*<y>.<DNSID>, where
// <x> and/or <y> may be empty. However, NSS requires <y> to be empty, and we
// follow NSS's stricter policy by accepting wildcards only of the form
// <x>*.<DNSID>, where <x> may be empty.
bool
PresentedDNSIDMatchesReferenceDNSID(Input presentedDNSID, Input referenceDNSID)
{
  if (!IsValidPresentedDNSID(presentedDNSID)) {
    return false;
  }
  if (!IsValidReferenceDNSID(referenceDNSID)) {
    return false;
  }

  Reader presented(presentedDNSID);
  Reader reference(referenceDNSID);
  bool isFirstPresentedByte = true;
  do {
    uint8_t presentedByte;
    Result rv = presented.Read(presentedByte);
    if (rv != Success) {
      return false;
    }
    if (presentedByte == '*') {
      // RFC 6125 is unclear about whether "www*.example.org" matches
      // "www.example.org". The Chromium test suite has this test:
      //
      //    { false, "w.bar.foo.com", "w*.bar.foo.com" },
      //
      // We agree with Chromium by forbidding "*" from expanding to the empty
      // string.
      do {
        uint8_t referenceByte;
        rv = reference.Read(referenceByte);
        if (rv != Success) {
          return false;
        }
      } while (!reference.Peek('.'));

      // We also don't allow a non-IDN presented ID label to match an IDN
      // reference ID label, except when the entire presented ID label is "*".
      // This avoids confusion when matching a presented ID like
      // "xn-*.example.org" against "xn--www.example.org" (which attempts to
      // abuse the punycode syntax) or "www-*.example.org" against
      // "xn--www--ep4c4a2kpf" (which makes sense to match, semantically, but
      // no implementations actually do).
      if (!isFirstPresentedByte && StartsWithIDNALabel(referenceDNSID)) {
        return false;
      }
    } else {
      // Allow an absolute presented DNS ID to match a relative reference DNS
      // ID.
      if (reference.AtEnd() && presented.AtEnd() && presentedByte == '.') {
        return true;
      }

      uint8_t referenceByte;
      rv = reference.Read(referenceByte);
      if (rv != Success) {
        return false;
      }
      if (LocaleInsensitveToLower(presentedByte) !=
          LocaleInsensitveToLower(referenceByte)) {
        return false;
      }
    }
    isFirstPresentedByte = false;
  } while (!presented.AtEnd());

  // Allow a relative presented DNS ID to match an absolute reference DNS ID.
  if (!reference.AtEnd()) {
    uint8_t referenceByte;
    Result rv = reference.Read(referenceByte);
    if (rv != Success) {
      return false;
    }
    if (referenceByte != '.') {
      return false;
    }
    if (!reference.AtEnd()) {
      return false;
    }
  }

  return true;
}

namespace {

// We avoid isdigit because it is locale-sensitive. See
// http://pubs.opengroup.org/onlinepubs/009695399/functions/tolower.html.
inline uint8_t
LocaleInsensitveToLower(uint8_t a)
{
  if (a >= 'A' && a <= 'Z') { // unlikely
    return static_cast<uint8_t>(
             static_cast<uint8_t>(a - static_cast<uint8_t>('A')) +
             static_cast<uint8_t>('a'));
  }
  return a;
}

bool
StartsWithIDNALabel(Input id)
{
  static const uint8_t IDN_ALABEL_PREFIX[4] = { 'x', 'n', '-', '-' };
  Reader input(id);
  for (size_t i = 0; i < sizeof(IDN_ALABEL_PREFIX); ++i) {
    uint8_t b;
    if (input.Read(b) != Success) {
      return false;
    }
    if (b != IDN_ALABEL_PREFIX[i]) {
      return false;
    }
  }
  return true;
}

bool
ReadIPv4AddressComponent(Reader& input, bool lastComponent,
                         /*out*/ uint8_t& valueOut)
{
  size_t length = 0;
  unsigned int value = 0; // Must be larger than uint8_t.

  for (;;) {
    if (input.AtEnd() && lastComponent) {
      break;
    }

    uint8_t b;
    if (input.Read(b) != Success) {
      return false;
    }

    if (b >= '0' && b <= '9') {
      if (value == 0 && length > 0) {
        return false; // Leading zeros are not allowed.
      }
      value = (value * 10) + (b - '0');
      if (value > 255) {
        return false; // Component's value is too large.
      }
      ++length;
    } else if (!lastComponent && b == '.') {
      break;
    } else {
      return false; // Invalid character.
    }
  }

  if (length == 0) {
    return false; // empty components not allowed
  }

  valueOut = static_cast<uint8_t>(value);
  return true;
}

} // unnamed namespace

bool
ParseIPv4Address(Input hostname, /*out*/ uint8_t (&out)[4])
{
  Reader input(hostname);
  return ReadIPv4AddressComponent(input, false, out[0]) &&
         ReadIPv4AddressComponent(input, false, out[1]) &&
         ReadIPv4AddressComponent(input, false, out[2]) &&
         ReadIPv4AddressComponent(input, true, out[3]);
}

namespace {

bool
FinishIPv6Address(/*in/out*/ uint8_t (&address)[16], int numComponents,
                  int contractionIndex)
{
  assert(numComponents >= 0);
  assert(numComponents <= 8);
  assert(contractionIndex >= -1);
  assert(contractionIndex <= 8);
  assert(contractionIndex <= numComponents);
  if (!(numComponents >= 0 &&
        numComponents <= 8 &&
        contractionIndex >= -1 &&
        contractionIndex <= 8 &&
        contractionIndex <= numComponents)) {
    return false;
  }

  if (contractionIndex == -1) {
    // no contraction
    return numComponents == 8;
  }

  if (numComponents >= 8) {
    return false; // no room left to expand the contraction.
  }

  // Shift components that occur after the contraction over.
  int componentsToMove = numComponents - contractionIndex;
  memmove(address + (2u * (8 - componentsToMove)),
          address + (2u * contractionIndex),
          componentsToMove * 2u);
  // Fill in the contracted area with zeros.
  memset(address + (2u * contractionIndex), 0u,
         (8u - numComponents) * 2u);

  return true;
}

} // unnamed namespace


bool
ParseIPv6Address(Input hostname, /*out*/ uint8_t (&out)[16])
{
  Reader input(hostname);

  int currentComponentIndex = 0;
  int contractionIndex = -1;

  if (input.Peek(':')) {
    // A valid input can only start with ':' if there is a contraction at the
    // beginning.
    uint8_t b;
    if (input.Read(b) != Success || b != ':') {
      assert(false);
      return false;
    }
    if (input.Read(b) != Success) {
      return false;
    }
    if (b != ':') {
      return false;
    }
    contractionIndex = 0;
  }

  for (;;) {
    // If we encounter a '.' then we'll have to backtrack to parse the input
    // from startOfComponent to the end of the input as an IPv4 address.
    Reader::Mark startOfComponent(input.GetMark());
    uint16_t componentValue = 0;
    size_t componentLength = 0;
    while (!input.AtEnd() && !input.Peek(':')) {
      uint8_t value;
      uint8_t b;
      if (input.Read(b) != Success) {
        assert(false);
        return false;
      }
      switch (b) {
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
          value = static_cast<uint8_t>(b - static_cast<uint8_t>('0'));
          break;
        case 'a': case 'b': case 'c': case 'd': case 'e': case 'f':
          value = static_cast<uint8_t>(b - static_cast<uint8_t>('a') +
                                       UINT8_C(10));
          break;
        case 'A': case 'B': case 'C': case 'D': case 'E': case 'F':
          value = static_cast<uint8_t>(b - static_cast<uint8_t>('A') +
                                       UINT8_C(10));
          break;
        case '.':
        {
          // A dot indicates we hit a IPv4-syntax component. Backtrack, parsing
          // the input from startOfComponent to the end of the input as an IPv4
          // address, and then combine it with the other components.

          if (currentComponentIndex > 6) {
            return false; // Too many components before the IPv4 component
          }

          input.SkipToEnd();
          Input ipv4Component;
          if (input.GetInput(startOfComponent, ipv4Component) != Success) {
            return false;
          }
          uint8_t (*ipv4)[4] =
            reinterpret_cast<uint8_t(*)[4]>(&out[2 * currentComponentIndex]);
          if (!ParseIPv4Address(ipv4Component, *ipv4)) {
            return false;
          }
          assert(input.AtEnd());
          currentComponentIndex += 2;

          return FinishIPv6Address(out, currentComponentIndex,
                                   contractionIndex);
        }
        default:
          return false;
      }
      if (componentLength >= 4) {
        // component too long
        return false;
      }
      ++componentLength;
      componentValue = (componentValue * 0x10u) + value;
    }

    if (currentComponentIndex >= 8) {
      return false; // too many components
    }

    if (componentLength == 0) {
      if (input.AtEnd() && currentComponentIndex == contractionIndex) {
        if (contractionIndex == 0) {
          // don't accept "::"
          return false;
        }
        return FinishIPv6Address(out, currentComponentIndex,
                                 contractionIndex);
      }
      return false;
    }

    out[2 * currentComponentIndex] =
      static_cast<uint8_t>(componentValue / 0x100);
    out[(2 * currentComponentIndex) + 1] =
      static_cast<uint8_t>(componentValue % 0x100);

    ++currentComponentIndex;

    if (input.AtEnd()) {
      return FinishIPv6Address(out, currentComponentIndex,
                               contractionIndex);
    }

    uint8_t b;
    if (input.Read(b) != Success || b != ':') {
      assert(false);
      return false;
    }

    if (input.Peek(':')) {
      // Contraction
      if (contractionIndex != -1) {
        return false; // multiple contractions are not allowed.
      }
      uint8_t b;
      if (input.Read(b) != Success || b != ':') {
        assert(false);
        return false;
      }
      contractionIndex = currentComponentIndex;
      if (input.AtEnd()) {
        // "::" at the end of the input.
        return FinishIPv6Address(out, currentComponentIndex,
                                 contractionIndex);
      }
    }
  }
}

bool
IsValidReferenceDNSID(Input hostname)
{
  return IsValidDNSID(hostname, ValidDNSIDMatchType::ReferenceID);
}

bool
IsValidPresentedDNSID(Input hostname)
{
  return IsValidDNSID(hostname, ValidDNSIDMatchType::PresentedID);
}

namespace {

bool
IsValidDNSID(Input hostname, ValidDNSIDMatchType matchType)
{
  if (hostname.GetLength() > 253) {
    return false;
  }

  Reader input(hostname);

  bool allowWildcard = matchType == ValidDNSIDMatchType::PresentedID;
  bool isWildcard = false;
  size_t dotCount = 0;

  size_t labelLength = 0;
  bool labelIsAllNumeric = false;
  bool labelIsWildcard = false;
  bool labelEndsWithHyphen = false;

  do {
    static const size_t MAX_LABEL_LENGTH = 63;

    uint8_t b;
    if (input.Read(b) != Success) {
      return false;
    }
    if (labelIsWildcard) {
      // Like NSS, be stricter than RFC6125 requires by insisting that the
      // "*" must be the last character in the label. This also prevents
      // multiple "*" in the label.
      if (b != '.') {
        return false;
      }
    }
    switch (b) {
      case '-':
        if (labelLength == 0) {
          return false; // Labels must not start with a hyphen.
        }
        labelIsAllNumeric = false;
        labelEndsWithHyphen = true;
        ++labelLength;
        if (labelLength > MAX_LABEL_LENGTH) {
          return false;
        }
        break;

      // We avoid isdigit because it is locale-sensitive. See
      // http://pubs.opengroup.org/onlinepubs/009695399/functions/isdigit.html
      case '0': case '5':
      case '1': case '6':
      case '2': case '7':
      case '3': case '8':
      case '4': case '9':
        if (labelLength == 0) {
          labelIsAllNumeric = true;
        }
        labelEndsWithHyphen = false;
        ++labelLength;
        if (labelLength > MAX_LABEL_LENGTH) {
          return false;
        }
        break;

      // We avoid using islower/isupper/tolower/toupper or similar things, to
      // avoid any possibility of this code being locale-sensitive. See
      // http://pubs.opengroup.org/onlinepubs/009695399/functions/isupper.html
      case 'a': case 'A': case 'n': case 'N':
      case 'b': case 'B': case 'o': case 'O':
      case 'c': case 'C': case 'p': case 'P':
      case 'd': case 'D': case 'q': case 'Q':
      case 'e': case 'E': case 'r': case 'R':
      case 'f': case 'F': case 's': case 'S':
      case 'g': case 'G': case 't': case 'T':
      case 'h': case 'H': case 'u': case 'U':
      case 'i': case 'I': case 'v': case 'V':
      case 'j': case 'J': case 'w': case 'W':
      case 'k': case 'K': case 'x': case 'X':
      case 'l': case 'L': case 'y': case 'Y':
      case 'm': case 'M': case 'z': case 'Z':
        labelIsAllNumeric = false;
        labelEndsWithHyphen = false;
        ++labelLength;
        if (labelLength > MAX_LABEL_LENGTH) {
          return false;
        }
        break;

      case '*':
        if (!allowWildcard) {
          return false;
        }
        labelIsWildcard = true;
        isWildcard = true;
        labelIsAllNumeric = false;
        labelEndsWithHyphen = false;
        ++labelLength;
        if (labelLength > MAX_LABEL_LENGTH) {
          return false;
        }
        break;

      case '.':
        ++dotCount;
        if (labelLength == 0) {
          return false;
        }
        if (labelEndsWithHyphen) {
          return false; // Labels must not end with a hyphen.
        }
        allowWildcard = false; // only allowed in the first label.
        labelIsWildcard = false;
        labelLength = 0;
        break;

      default:
        return false; // Invalid character.
    }
  } while (!input.AtEnd());

  if (labelEndsWithHyphen) {
    return false; // Labels must not end with a hyphen.
  }

  if (labelIsAllNumeric) {
    return false; // Last label must not be all numeric.
  }

  if (isWildcard) {
    // If the DNS ID ends with a dot, the last dot signifies an absolute ID.
    size_t labelCount = (labelLength == 0) ? dotCount : (dotCount + 1);

    // Like NSS, require at least two labels to follow the wildcard label.
    //
    // TODO(bug XXXXXXX): Allow the TrustDomain to control this on a
    // per-eTLD+1 basis, similar to Chromium. Even then, it might be better to
    // still enforce that there are at least two labels after the wildcard.
    if (labelCount < 3) {
      return false;
    }
    // XXX: RFC6125 says that we shouldn't accept wildcards within an IDN
    // A-Label. The consequence of this is that we effectively discriminate
    // against users of languages that cannot be encoded with ASCII.
    if (StartsWithIDNALabel(hostname)) {
      return false;
    }

    // TODO(bug XXXXXXX): Wildcards are not allowed for EV certificates.
    // Provide an option to indicate whether wildcards should be matched, for
    // the purpose of helping the application enforce this.
  }

  return true;
}

} // unnamed namespace

} } // namespace mozilla::pkix
