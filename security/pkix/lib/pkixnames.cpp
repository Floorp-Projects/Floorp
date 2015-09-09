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

// This code implements RFC6125-ish name matching, RFC5280-ish name constraint
// checking, and related things.
//
// In this code, identifiers are classified as either "presented" or
// "reference" identifiers are defined in
// http://tools.ietf.org/html/rfc6125#section-1.8. A "presented identifier" is
// one in the subjectAltName of the certificate, or sometimes within a CN of
// the certificate's subject. The "reference identifier" is the one we are
// being asked to match the certificate against. When checking name
// constraints, the reference identifier is the entire encoded name constraint
// extension value.

#include "pkixcheck.h"
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
enum class GeneralNameType : uint8_t
{
  // Note that these values are NOT contiguous. Some values have the
  // der::CONSTRUCTED bit set while others do not.
  // (The der::CONSTRUCTED bit is for types where the value is a SEQUENCE.)
  otherName = der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 0,
  rfc822Name = der::CONTEXT_SPECIFIC | 1,
  dNSName = der::CONTEXT_SPECIFIC | 2,
  x400Address = der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 3,
  directoryName = der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 4,
  ediPartyName = der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 5,
  uniformResourceIdentifier = der::CONTEXT_SPECIFIC | 6,
  iPAddress = der::CONTEXT_SPECIFIC | 7,
  registeredID = der::CONTEXT_SPECIFIC | 8,
  // nameConstraints is a pseudo-GeneralName used to signify that a
  // reference ID is actually the entire name constraint extension.
  nameConstraints = 0xff
};

inline Result
ReadGeneralName(Reader& reader,
                /*out*/ GeneralNameType& generalNameType,
                /*out*/ Input& value)
{
  uint8_t tag;
  Result rv = der::ReadTagAndGetValue(reader, tag, value);
  if (rv != Success) {
    return rv;
  }
  switch (tag) {
    case static_cast<uint8_t>(GeneralNameType::otherName):
      generalNameType = GeneralNameType::otherName;
      break;
    case static_cast<uint8_t>(GeneralNameType::rfc822Name):
      generalNameType = GeneralNameType::rfc822Name;
      break;
    case static_cast<uint8_t>(GeneralNameType::dNSName):
      generalNameType = GeneralNameType::dNSName;
      break;
    case static_cast<uint8_t>(GeneralNameType::x400Address):
      generalNameType = GeneralNameType::x400Address;
      break;
    case static_cast<uint8_t>(GeneralNameType::directoryName):
      generalNameType = GeneralNameType::directoryName;
      break;
    case static_cast<uint8_t>(GeneralNameType::ediPartyName):
      generalNameType = GeneralNameType::ediPartyName;
      break;
    case static_cast<uint8_t>(GeneralNameType::uniformResourceIdentifier):
      generalNameType = GeneralNameType::uniformResourceIdentifier;
      break;
    case static_cast<uint8_t>(GeneralNameType::iPAddress):
      generalNameType = GeneralNameType::iPAddress;
      break;
    case static_cast<uint8_t>(GeneralNameType::registeredID):
      generalNameType = GeneralNameType::registeredID;
      break;
    default:
      return Result::ERROR_BAD_DER;
  }
  return Success;
}

enum class FallBackToSearchWithinSubject { No = 0, Yes = 1 };

enum class MatchResult
{
  NoNamesOfGivenType = 0,
  Mismatch = 1,
  Match = 2
};

Result SearchNames(const Input* subjectAltName, Input subject,
                   GeneralNameType referenceIDType,
                   Input referenceID,
                   FallBackToSearchWithinSubject fallBackToCommonName,
                   /*out*/ MatchResult& match);
Result SearchWithinRDN(Reader& rdn,
                       GeneralNameType referenceIDType,
                       Input referenceID,
                       FallBackToSearchWithinSubject fallBackToEmailAddress,
                       FallBackToSearchWithinSubject fallBackToCommonName,
                       /*in/out*/ MatchResult& match);
Result MatchAVA(Input type,
                uint8_t valueEncodingTag,
                Input presentedID,
                GeneralNameType referenceIDType,
                Input referenceID,
                FallBackToSearchWithinSubject fallBackToEmailAddress,
                FallBackToSearchWithinSubject fallBackToCommonName,
                /*in/out*/ MatchResult& match);
Result ReadAVA(Reader& rdn,
               /*out*/ Input& type,
               /*out*/ uint8_t& valueTag,
               /*out*/ Input& value);
void MatchSubjectPresentedIDWithReferenceID(GeneralNameType presentedIDType,
                                            Input presentedID,
                                            GeneralNameType referenceIDType,
                                            Input referenceID,
                                            /*in/out*/ MatchResult& match);

Result MatchPresentedIDWithReferenceID(GeneralNameType presentedIDType,
                                       Input presentedID,
                                       GeneralNameType referenceIDType,
                                       Input referenceID,
                                       /*in/out*/ MatchResult& matchResult);
Result CheckPresentedIDConformsToConstraints(GeneralNameType referenceIDType,
                                             Input presentedID,
                                             Input nameConstraints);

uint8_t LocaleInsensitveToLower(uint8_t a);
bool StartsWithIDNALabel(Input id);

enum class IDRole
{
  ReferenceID = 0,
  PresentedID = 1,
  NameConstraint = 2,
};

enum class AllowWildcards { No = 0, Yes = 1 };

// DNSName constraints implicitly allow subdomain matching when there is no
// leading dot ("foo.example.com" matches a constraint of "example.com"), but
// RFC822Name constraints only allow subdomain matching when there is a leading
// dot ("foo.example.com" does not match "example.com" but does match
// ".example.com").
enum class AllowDotlessSubdomainMatches { No = 0, Yes = 1 };

bool IsValidDNSID(Input hostname, IDRole idRole,
                  AllowWildcards allowWildcards);

Result MatchPresentedDNSIDWithReferenceDNSID(
         Input presentedDNSID,
         AllowWildcards allowWildcards,
         AllowDotlessSubdomainMatches allowDotlessSubdomainMatches,
         IDRole referenceDNSIDRole,
         Input referenceDNSID,
         /*out*/ bool& matches);

Result MatchPresentedRFC822NameWithReferenceRFC822Name(
         Input presentedRFC822Name, IDRole referenceRFC822NameRole,
         Input referenceRFC822Name, /*out*/ bool& matches);

} // unnamed namespace

bool IsValidReferenceDNSID(Input hostname);
bool IsValidPresentedDNSID(Input hostname);
bool ParseIPv4Address(Input hostname, /*out*/ uint8_t (&out)[4]);
bool ParseIPv6Address(Input hostname, /*out*/ uint8_t (&out)[16]);

// This is used by the pkixnames_tests.cpp tests.
Result
MatchPresentedDNSIDWithReferenceDNSID(Input presentedDNSID,
                                      Input referenceDNSID,
                                      /*out*/ bool& matches)
{
  return MatchPresentedDNSIDWithReferenceDNSID(
           presentedDNSID, AllowWildcards::Yes,
           AllowDotlessSubdomainMatches::Yes, IDRole::ReferenceID,
           referenceDNSID, matches);
}

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
  MatchResult match;
  uint8_t ipv6[16];
  uint8_t ipv4[4];
  if (IsValidReferenceDNSID(hostname)) {
    rv = SearchNames(subjectAltName, subject, GeneralNameType::dNSName,
                     hostname, FallBackToSearchWithinSubject::Yes, match);
  } else if (ParseIPv6Address(hostname, ipv6)) {
    rv = SearchNames(subjectAltName, subject, GeneralNameType::iPAddress,
                     Input(ipv6), FallBackToSearchWithinSubject::No, match);
  } else if (ParseIPv4Address(hostname, ipv4)) {
    rv = SearchNames(subjectAltName, subject, GeneralNameType::iPAddress,
                     Input(ipv4), FallBackToSearchWithinSubject::Yes, match);
  } else {
    return Result::ERROR_BAD_CERT_DOMAIN;
  }
  if (rv != Success) {
    return rv;
  }
  switch (match) {
    case MatchResult::NoNamesOfGivenType: // fall through
    case MatchResult::Mismatch:
      return Result::ERROR_BAD_CERT_DOMAIN;
    case MatchResult::Match:
      return Success;
    MOZILLA_PKIX_UNREACHABLE_DEFAULT_ENUM
  }
}

// 4.2.1.10. Name Constraints
Result
CheckNameConstraints(Input encodedNameConstraints,
                     const BackCert& firstChild,
                     KeyPurposeId requiredEKUIfPresent)
{
  for (const BackCert* child = &firstChild; child; child = child->childCert) {
    FallBackToSearchWithinSubject fallBackToCommonName
      = (child->endEntityOrCA == EndEntityOrCA::MustBeEndEntity &&
         requiredEKUIfPresent == KeyPurposeId::id_kp_serverAuth)
      ? FallBackToSearchWithinSubject::Yes
      : FallBackToSearchWithinSubject::No;

    MatchResult match;
    Result rv = SearchNames(child->GetSubjectAltName(), child->GetSubject(),
                            GeneralNameType::nameConstraints,
                            encodedNameConstraints, fallBackToCommonName,
                            match);
    if (rv != Success) {
      return rv;
    }
    switch (match) {
      case MatchResult::Match: // fall through
      case MatchResult::NoNamesOfGivenType:
        break;
      case MatchResult::Mismatch:
        return Result::ERROR_CERT_NOT_IN_NAME_SPACE;
    }
  }

  return Success;
}

namespace {

// SearchNames is used by CheckCertHostname and CheckNameConstraints.
//
// When called during name constraint checking, referenceIDType is
// GeneralNameType::nameConstraints and referenceID is the entire encoded name
// constraints extension value.
//
// The main benefit of using the exact same code paths for both is that we
// ensure consistency between name validation and name constraint enforcement
// regarding thing like "Which CN attributes should be considered as potential
// CN-IDs" and "Which character sets are acceptable for CN-IDs?" If the name
// matching and the name constraint enforcement logic were out of sync on these
// issues (e.g. if name matching were to consider all subject CN attributes,
// but name constraints were only enforced on the most specific subject CN),
// trivial name constraint bypasses could result.

Result
SearchNames(/*optional*/ const Input* subjectAltName,
            Input subject,
            GeneralNameType referenceIDType,
            Input referenceID,
            FallBackToSearchWithinSubject fallBackToCommonName,
            /*out*/ MatchResult& match)
{
  Result rv;

  match = MatchResult::NoNamesOfGivenType;

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

  if (subjectAltName) {
    Reader altNames;
    rv = der::ExpectTagAndGetValueAtEnd(*subjectAltName, der::SEQUENCE,
                                        altNames);
    if (rv != Success) {
      return rv;
    }

    // According to RFC 5280, "If the subjectAltName extension is present, the
    // sequence MUST contain at least one entry." For compatibility reasons, we
    // do not enforce this. See bug 1143085.
    while (!altNames.AtEnd()) {
      GeneralNameType presentedIDType;
      Input presentedID;
      rv = ReadGeneralName(altNames, presentedIDType, presentedID);
      if (rv != Success) {
        return rv;
      }

      rv = MatchPresentedIDWithReferenceID(presentedIDType, presentedID,
                                           referenceIDType, referenceID,
                                           match);
      if (rv != Success) {
        return rv;
      }
      if (referenceIDType != GeneralNameType::nameConstraints &&
          match == MatchResult::Match) {
        return Success;
      }
      if (presentedIDType == GeneralNameType::dNSName ||
          presentedIDType == GeneralNameType::iPAddress) {
        fallBackToCommonName = FallBackToSearchWithinSubject::No;
      }
    }
  }

  if (referenceIDType == GeneralNameType::nameConstraints) {
    rv = CheckPresentedIDConformsToConstraints(GeneralNameType::directoryName,
                                               subject, referenceID);
    if (rv != Success) {
      return rv;
    }
  }

  FallBackToSearchWithinSubject fallBackToEmailAddress;
  if (!subjectAltName &&
      (referenceIDType == GeneralNameType::rfc822Name ||
       referenceIDType == GeneralNameType::nameConstraints)) {
    fallBackToEmailAddress = FallBackToSearchWithinSubject::Yes;
  } else {
    fallBackToEmailAddress = FallBackToSearchWithinSubject::No;
  }

  // Short-circuit the parsing of the subject name if we're not going to match
  // any names in it
  if (fallBackToEmailAddress == FallBackToSearchWithinSubject::No &&
      fallBackToCommonName == FallBackToSearchWithinSubject::No) {
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
                       der::EmptyAllowed::Yes, [&](Reader& r) {
    return SearchWithinRDN(r, referenceIDType, referenceID,
                          fallBackToEmailAddress, fallBackToCommonName, match);
  });
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
                FallBackToSearchWithinSubject fallBackToEmailAddress,
                FallBackToSearchWithinSubject fallBackToCommonName,
                /*in/out*/ MatchResult& match)
{
  do {
    Input type;
    uint8_t valueTag;
    Input value;
    Result rv = ReadAVA(rdn, type, valueTag, value);
    if (rv != Success) {
      return rv;
    }
    rv = MatchAVA(type, valueTag, value, referenceIDType, referenceID,
                  fallBackToEmailAddress, fallBackToCommonName, match);
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
MatchAVA(Input type, uint8_t valueEncodingTag, Input presentedID,
         GeneralNameType referenceIDType,
         Input referenceID,
         FallBackToSearchWithinSubject fallBackToEmailAddress,
         FallBackToSearchWithinSubject fallBackToCommonName,
         /*in/out*/ MatchResult& match)
{
  // Try to match the  CN as a DNSName or an IPAddress.
  //
  // id-at-commonName        AttributeType ::= { id-at 3 }
  //
  // -- Naming attributes of type X520CommonName:
  // --   X520CommonName ::= DirectoryName (SIZE (1..ub-common-name))
  // --
  // -- Expanded to avoid parameterized type:
  // X520CommonName ::= CHOICE {
  //       teletexString     TeletexString   (SIZE (1..ub-common-name)),
  //       printableString   PrintableString (SIZE (1..ub-common-name)),
  //       universalString   UniversalString (SIZE (1..ub-common-name)),
  //       utf8String        UTF8String      (SIZE (1..ub-common-name)),
  //       bmpString         BMPString       (SIZE (1..ub-common-name)) }
  //
  // python DottedOIDToCode.py id-at-commonName 2.5.4.3
  static const uint8_t id_at_commonName[] = {
    0x55, 0x04, 0x03
  };
  if (fallBackToCommonName == FallBackToSearchWithinSubject::Yes &&
      InputsAreEqual(type, Input(id_at_commonName))) {
    // We might have previously found a match. Now that we've found another CN,
    // we no longer consider that previous match to be a match, so "forget" about
    // it.
    match = MatchResult::NoNamesOfGivenType;

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

    if (IsValidPresentedDNSID(presentedID)) {
      MatchSubjectPresentedIDWithReferenceID(GeneralNameType::dNSName,
                                             presentedID, referenceIDType,
                                             referenceID, match);
    } else {
      // We don't match CN-IDs for IPv6 addresses.
      // MatchSubjectPresentedIDWithReferenceID ensures that it won't match an
      // IPv4 address with an IPv6 address, so we don't need to check that
      // referenceID is an IPv4 address here.
      uint8_t ipv4[4];
      if (ParseIPv4Address(presentedID, ipv4)) {
        MatchSubjectPresentedIDWithReferenceID(GeneralNameType::iPAddress,
                                               Input(ipv4), referenceIDType,
                                               referenceID, match);
      }
    }

    // Regardless of whether there was a match, we keep going in case we find
    // another CN later. If we do find another one, then this match/mismatch
    // will be ignored, because we only care about the most specific CN.

    return Success;
  }

  // Match an email address against an emailAddress attribute in the
  // subject.
  //
  // id-emailAddress      AttributeType ::= { pkcs-9 1 }
  //
  // EmailAddress ::=     IA5String (SIZE (1..ub-emailaddress-length))
  //
  // python DottedOIDToCode.py id-emailAddress 1.2.840.113549.1.9.1
  static const uint8_t id_emailAddress[] = {
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x01
  };
  if (fallBackToEmailAddress == FallBackToSearchWithinSubject::Yes &&
      InputsAreEqual(type, Input(id_emailAddress))) {
    if (referenceIDType == GeneralNameType::rfc822Name &&
        match == MatchResult::Match) {
      // We already found a match; we don't need to match another one
      return Success;
    }
    if (valueEncodingTag != der::IA5String) {
      return Result::ERROR_BAD_DER;
    }
    return MatchPresentedIDWithReferenceID(GeneralNameType::rfc822Name,
                                           presentedID, referenceIDType,
                                           referenceID, match);
  }

  return Success;
}

void
MatchSubjectPresentedIDWithReferenceID(GeneralNameType presentedIDType,
                                       Input presentedID,
                                       GeneralNameType referenceIDType,
                                       Input referenceID,
                                       /*in/out*/ MatchResult& match)
{
  Result rv = MatchPresentedIDWithReferenceID(presentedIDType, presentedID,
                                              referenceIDType, referenceID,
                                              match);
  if (rv != Success) {
    match = MatchResult::Mismatch;
  }
}

Result
MatchPresentedIDWithReferenceID(GeneralNameType presentedIDType,
                                Input presentedID,
                                GeneralNameType referenceIDType,
                                Input referenceID,
                                /*out*/ MatchResult& matchResult)
{
  if (referenceIDType == GeneralNameType::nameConstraints) {
    // matchResult is irrelevant when checking name constraints; only the
    // pass/fail result of CheckPresentedIDConformsToConstraints matters.
    return CheckPresentedIDConformsToConstraints(presentedIDType, presentedID,
                                                 referenceID);
  }

  if (presentedIDType != referenceIDType) {
    matchResult = MatchResult::Mismatch;
    return Success;
  }

  Result rv;
  bool foundMatch;

  switch (referenceIDType) {
    case GeneralNameType::dNSName:
      rv = MatchPresentedDNSIDWithReferenceDNSID(
             presentedID, AllowWildcards::Yes,
             AllowDotlessSubdomainMatches::Yes, IDRole::ReferenceID,
             referenceID, foundMatch);
      break;

    case GeneralNameType::iPAddress:
      foundMatch = InputsAreEqual(presentedID, referenceID);
      rv = Success;
      break;

    case GeneralNameType::rfc822Name:
      rv = MatchPresentedRFC822NameWithReferenceRFC822Name(
             presentedID, IDRole::ReferenceID, referenceID, foundMatch);
      break;

    case GeneralNameType::directoryName:
      // TODO: At some point, we may add APIs for matching DirectoryNames.
      // fall through

    case GeneralNameType::otherName: // fall through
    case GeneralNameType::x400Address: // fall through
    case GeneralNameType::ediPartyName: // fall through
    case GeneralNameType::uniformResourceIdentifier: // fall through
    case GeneralNameType::registeredID: // fall through
    case GeneralNameType::nameConstraints:
      return NotReached("unexpected nameType for SearchType::Match",
                        Result::FATAL_ERROR_INVALID_ARGS);

    MOZILLA_PKIX_UNREACHABLE_DEFAULT_ENUM
 }

  if (rv != Success) {
    return rv;
  }
  matchResult = foundMatch ? MatchResult::Match : MatchResult::Mismatch;
  return Success;
}

enum class NameConstraintsSubtrees : uint8_t
{
  permittedSubtrees = der::CONSTRUCTED | der::CONTEXT_SPECIFIC | 0,
  excludedSubtrees  = der::CONSTRUCTED | der::CONTEXT_SPECIFIC | 1
};

Result CheckPresentedIDConformsToNameConstraintsSubtrees(
         GeneralNameType presentedIDType,
         Input presentedID,
         Reader& nameConstraints,
         NameConstraintsSubtrees subtreesType);
Result MatchPresentedIPAddressWithConstraint(Input presentedID,
                                             Input iPAddressConstraint,
                                             /*out*/ bool& foundMatch);
Result MatchPresentedDirectoryNameWithConstraint(
         NameConstraintsSubtrees subtreesType, Input presentedID,
         Input directoryNameConstraint, /*out*/ bool& matches);

Result
CheckPresentedIDConformsToConstraints(
  GeneralNameType presentedIDType,
  Input presentedID,
  Input encodedNameConstraints)
{
  // NameConstraints ::= SEQUENCE {
  //      permittedSubtrees       [0]     GeneralSubtrees OPTIONAL,
  //      excludedSubtrees        [1]     GeneralSubtrees OPTIONAL }
  Reader nameConstraints;
  Result rv = der::ExpectTagAndGetValueAtEnd(encodedNameConstraints,
                                             der::SEQUENCE, nameConstraints);
  if (rv != Success) {
    return rv;
  }

  // RFC 5280 says "Conforming CAs MUST NOT issue certificates where name
  // constraints is an empty sequence. That is, either the permittedSubtrees
  // field or the excludedSubtrees MUST be present."
  if (nameConstraints.AtEnd()) {
    return Result::ERROR_BAD_DER;
  }

  rv = CheckPresentedIDConformsToNameConstraintsSubtrees(
         presentedIDType, presentedID, nameConstraints,
         NameConstraintsSubtrees::permittedSubtrees);
  if (rv != Success) {
    return rv;
  }

  rv = CheckPresentedIDConformsToNameConstraintsSubtrees(
         presentedIDType, presentedID, nameConstraints,
         NameConstraintsSubtrees::excludedSubtrees);
  if (rv != Success) {
    return rv;
  }

  return der::End(nameConstraints);
}

Result
CheckPresentedIDConformsToNameConstraintsSubtrees(
  GeneralNameType presentedIDType,
  Input presentedID,
  Reader& nameConstraints,
  NameConstraintsSubtrees subtreesType)
{
  if (!nameConstraints.Peek(static_cast<uint8_t>(subtreesType))) {
    return Success;
  }

  Reader subtrees;
  Result rv = der::ExpectTagAndGetValue(nameConstraints,
                                        static_cast<uint8_t>(subtreesType),
                                        subtrees);
  if (rv != Success) {
    return rv;
  }

  bool hasPermittedSubtreesMatch = false;
  bool hasPermittedSubtreesMismatch = false;

  // GeneralSubtrees ::= SEQUENCE SIZE (1..MAX) OF GeneralSubtree
  //
  // do { ... } while(...) because subtrees isn't allowed to be empty.
  do {
    // GeneralSubtree ::= SEQUENCE {
    //      base                    GeneralName,
    //      minimum         [0]     BaseDistance DEFAULT 0,
    //      maximum         [1]     BaseDistance OPTIONAL }
    Reader subtree;
    rv = ExpectTagAndGetValue(subtrees, der::SEQUENCE, subtree);
    if (rv != Success) {
      return rv;
    }
    GeneralNameType nameConstraintType;
    Input base;
    rv = ReadGeneralName(subtree, nameConstraintType, base);
    if (rv != Success) {
      return rv;
    }
    // http://tools.ietf.org/html/rfc5280#section-4.2.1.10: "Within this
    // profile, the minimum and maximum fields are not used with any name
    // forms, thus, the minimum MUST be zero, and maximum MUST be absent."
    //
    // Since the default value isn't allowed to be encoded according to the DER
    // encoding rules for DEFAULT, this is equivalent to saying that neither
    // minimum or maximum must be encoded.
    rv = der::End(subtree);
    if (rv != Success) {
      return rv;
    }

    if (presentedIDType == nameConstraintType) {
      bool matches;

      switch (presentedIDType) {
        case GeneralNameType::dNSName:
          rv = MatchPresentedDNSIDWithReferenceDNSID(
                 presentedID, AllowWildcards::Yes,
                 AllowDotlessSubdomainMatches::Yes, IDRole::NameConstraint,
                 base, matches);
          if (rv != Success) {
            return rv;
          }
          break;

        case GeneralNameType::iPAddress:
          rv = MatchPresentedIPAddressWithConstraint(presentedID, base,
                                                     matches);
          if (rv != Success) {
            return rv;
          }
          break;

        case GeneralNameType::directoryName:
          rv = MatchPresentedDirectoryNameWithConstraint(subtreesType,
                                                         presentedID, base,
                                                         matches);
          if (rv != Success) {
            return rv;
          }
          break;

        case GeneralNameType::rfc822Name:
          rv = MatchPresentedRFC822NameWithReferenceRFC822Name(
                 presentedID, IDRole::NameConstraint, base, matches);
          if (rv != Success) {
            return rv;
          }
          break;

        // RFC 5280 says "Conforming CAs [...] SHOULD NOT impose name
        // constraints on the x400Address, ediPartyName, or registeredID
        // name forms. It also says "Applications conforming to this profile
        // [...] SHOULD be able to process name constraints that are imposed
        // on [...] uniformResourceIdentifier [...]", but we don't bother.
        //
        // TODO: Ask to have spec updated to say ""Conforming CAs [...] SHOULD
        // NOT impose name constraints on the otherName, x400Address,
        // ediPartyName, uniformResourceIdentifier, or registeredID name
        // forms."
        case GeneralNameType::otherName: // fall through
        case GeneralNameType::x400Address: // fall through
        case GeneralNameType::ediPartyName: // fall through
        case GeneralNameType::uniformResourceIdentifier: // fall through
        case GeneralNameType::registeredID: // fall through
          return Result::ERROR_CERT_NOT_IN_NAME_SPACE;

        case GeneralNameType::nameConstraints:
          return NotReached("invalid presentedIDType",
                            Result::FATAL_ERROR_LIBRARY_FAILURE);

        MOZILLA_PKIX_UNREACHABLE_DEFAULT_ENUM
      }

      switch (subtreesType) {
        case NameConstraintsSubtrees::permittedSubtrees:
          if (matches) {
            hasPermittedSubtreesMatch = true;
          } else {
            hasPermittedSubtreesMismatch = true;
          }
          break;
        case NameConstraintsSubtrees::excludedSubtrees:
          if (matches) {
            return Result::ERROR_CERT_NOT_IN_NAME_SPACE;
          }
          break;
      }
    }
  } while (!subtrees.AtEnd());

  if (hasPermittedSubtreesMismatch && !hasPermittedSubtreesMatch) {
    // If there was any entry of the given type in permittedSubtrees, then it
    // required that at least one of them must match. Since none of them did,
    // we have a failure.
    return Result::ERROR_CERT_NOT_IN_NAME_SPACE;
  }

  return Success;
}

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
//
// An relative presented DNS ID matches both an absolute reference ID and a
// relative reference ID. Absolute presented DNS IDs are not supported:
//
//      Presented ID   Reference ID  Result
//      -------------------------------------
//      example.com    example.com   Match
//      example.com.   example.com   Mismatch
//      example.com    example.com.  Match
//      example.com.   example.com.  Mismatch
//
// There are more subtleties documented inline in the code.
//
// Name constraints ///////////////////////////////////////////////////////////
//
// This is all RFC 5280 has to say about DNSName constraints:
//
//     DNS name restrictions are expressed as host.example.com.  Any DNS
//     name that can be constructed by simply adding zero or more labels to
//     the left-hand side of the name satisfies the name constraint.  For
//     example, www.host.example.com would satisfy the constraint but
//     host1.example.com would not.
//
// This lack of specificity has lead to a lot of uncertainty regarding
// subdomain matching. In particular, the following questions have been
// raised and answered:
//
//     Q: Does a presented identifier equal (case insensitive) to the name
//        constraint match the constraint? For example, does the presented
//        ID "host.example.com" match a "host.example.com" constraint?
//     A: Yes. RFC5280 says "by simply adding zero or more labels" and this
//        is the case of adding zero labels.
//
//     Q: When the name constraint does not start with ".", do subdomain
//        presented identifiers match it? For example, does the presented
//        ID "www.host.example.com" match a "host.example.com" constraint?
//     A: Yes. RFC5280 says "by simply adding zero or more labels" and this
//        is the case of adding more than zero labels. The example is the
//        one from RFC 5280.
//
//     Q: When the name constraint does not start with ".", does a
//        non-subdomain prefix match it? For example, does "bigfoo.bar.com"
//        match "foo.bar.com"? [4]
//     A: No. We interpret RFC 5280's language of "adding zero or more labels"
//        to mean that whole labels must be prefixed.
//
//     (Note that the above three scenarios are the same as the RFC 6265
//     domain matching rules [0].)
//
//     Q: Is a name constraint that starts with "." valid, and if so, what
//        semantics does it have? For example, does a presented ID of
//        "www.example.com" match a constraint of ".example.com"? Does a
//        presented ID of "example.com" match a constraint of ".example.com"?
//     A: This implementation, NSS[1], and SChannel[2] all support a
//        leading ".", but OpenSSL[3] does not yet. Amongst the
//        implementations that support it, a leading "." is legal and means
//        the same thing as when the "." is omitted, EXCEPT that a
//        presented identifier equal (case insensitive) to the name
//        constraint is not matched; i.e. presented DNSName identifiers
//        must be subdomains. Some CAs in Mozilla's CA program (e.g. HARICA)
//        have name constraints with the leading "." in their root
//        certificates. The name constraints imposed on DCISS by Mozilla also
//        have the it, so supporting this is a requirement for backward
//        compatibility, even if it is not yet standardized. So, for example, a
//        presented ID of "www.example.com" matches a constraint of
//        ".example.com" but a presented ID of "example.com" does not.
//
//     Q: Is there a way to prevent subdomain matches?
//     A: Yes.
//
//        Some people have proposed that dNSName constraints that do not
//        start with a "." should be restricted to exact (case insensitive)
//        matches. However, such a change of semantics from what RFC5280
//        specifies would be a non-backward-compatible change in the case of
//        permittedSubtrees constraints, and it would be a security issue for
//        excludedSubtrees constraints.
//
//        However, it can be done with a combination of permittedSubtrees and
//        excludedSubtrees, e.g. "example.com" in permittedSubtrees and
//        ".example.com" in excudedSubtrees.
//
//     Q: Are name constraints allowed to be specified as absolute names?
//        For example, does a presented ID of "example.com" match a name
//        constraint of "example.com." and vice versa.
//     A: Absolute names are not supported as presented IDs or name
//        constraints. Only reference IDs may be absolute.
//
//     Q: Is "" a valid DNSName constraints? If so, what does it mean?
//     A: Yes. Any valid presented DNSName can be formed "by simply adding zero
//        or more labels to the left-hand side" of "". In particular, an
//        excludedSubtrees DNSName constraint of "" forbids all DNSNames.
//
//     Q: Is "." a valid DNSName constraints? If so, what does it mean?
//     A: No, because absolute names are not allowed (see above).
//
// [0] RFC 6265 (Cookies) Domain Matching rules:
//     http://tools.ietf.org/html/rfc6265#section-5.1.3
// [1] NSS source code:
//     https://mxr.mozilla.org/nss/source/lib/certdb/genname.c?rev=2a7348f013cb#1209
// [2] Description of SChannel's behavior from Microsoft:
//     http://www.imc.org/ietf-pkix/mail-archive/msg04668.html
// [3] Proposal to add such support to OpenSSL:
//     http://www.mail-archive.com/openssl-dev%40openssl.org/msg36204.html
//     https://rt.openssl.org/Ticket/Display.html?id=3562
// [4] Feedback on the lack of clarify in the definition that never got
//     incorporated into the spec:
//     https://www.ietf.org/mail-archive/web/pkix/current/msg21192.html
Result
MatchPresentedDNSIDWithReferenceDNSID(
  Input presentedDNSID,
  AllowWildcards allowWildcards,
  AllowDotlessSubdomainMatches allowDotlessSubdomainMatches,
  IDRole referenceDNSIDRole,
  Input referenceDNSID,
  /*out*/ bool& matches)
{
  if (!IsValidDNSID(presentedDNSID, IDRole::PresentedID, allowWildcards)) {
    return Result::ERROR_BAD_DER;
  }

  if (!IsValidDNSID(referenceDNSID, referenceDNSIDRole, AllowWildcards::No)) {
    return Result::ERROR_BAD_DER;
  }

  Reader presented(presentedDNSID);
  Reader reference(referenceDNSID);

  switch (referenceDNSIDRole)
  {
    case IDRole::ReferenceID:
      break;

    case IDRole::NameConstraint:
    {
      if (presentedDNSID.GetLength() > referenceDNSID.GetLength()) {
        if (referenceDNSID.GetLength() == 0) {
          // An empty constraint matches everything.
          matches = true;
          return Success;
        }
        // If the reference ID starts with a dot then skip the prefix of
        // of the presented ID and start the comparison at the position of that
        // dot. Examples:
        //
        //                                       Matches     Doesn't Match
        //     -----------------------------------------------------------
        //       original presented ID:  www.example.com    badexample.com
        //                     skipped:  www                ba
        //     presented ID w/o prefix:     .example.com      dexample.com
        //                reference ID:     .example.com      .example.com
        //
        // If the reference ID does not start with a dot then we skip the
        // prefix of the presented ID but also verify that the prefix ends with
        // a dot. Examples:
        //
        //                                       Matches     Doesn't Match
        //     -----------------------------------------------------------
        //       original presented ID:  www.example.com    badexample.com
        //                     skipped:  www                ba
        //                 must be '.':     .                 d
        //     presented ID w/o prefix:      example.com       example.com
        //                reference ID:      example.com       example.com
        //
        if (reference.Peek('.')) {
          if (presented.Skip(static_cast<Input::size_type>(
                               presentedDNSID.GetLength() -
                                 referenceDNSID.GetLength())) != Success) {
            return NotReached("skipping subdomain failed",
                              Result::FATAL_ERROR_LIBRARY_FAILURE);
          }
        } else if (allowDotlessSubdomainMatches ==
                   AllowDotlessSubdomainMatches::Yes) {
          if (presented.Skip(static_cast<Input::size_type>(
                               presentedDNSID.GetLength() -
                                 referenceDNSID.GetLength() - 1)) != Success) {
            return NotReached("skipping subdomains failed",
                              Result::FATAL_ERROR_LIBRARY_FAILURE);
          }
          uint8_t b;
          if (presented.Read(b) != Success) {
            return NotReached("reading from presentedDNSID failed",
                              Result::FATAL_ERROR_LIBRARY_FAILURE);
          }
          if (b != '.') {
            matches = false;
            return Success;
          }
        }
      }
      break;
    }

    case IDRole::PresentedID: // fall through
      return NotReached("IDRole::PresentedID is not a valid referenceDNSIDRole",
                        Result::FATAL_ERROR_INVALID_ARGS);
  }

  // We only allow wildcard labels that consist only of '*'.
  if (presented.Peek('*')) {
    if (presented.Skip(1) != Success) {
      return NotReached("Skipping '*' failed",
                        Result::FATAL_ERROR_LIBRARY_FAILURE);
    }
    do {
      // This will happen if reference is a single, relative label
      if (reference.AtEnd()) {
        matches = false;
        return Success;
      }
      uint8_t referenceByte;
      if (reference.Read(referenceByte) != Success) {
        return NotReached("invalid reference ID",
                          Result::FATAL_ERROR_INVALID_ARGS);
      }
    } while (!reference.Peek('.'));
  }

  for (;;) {
    uint8_t presentedByte;
    if (presented.Read(presentedByte) != Success) {
      matches = false;
      return Success;
    }
    uint8_t referenceByte;
    if (reference.Read(referenceByte) != Success) {
      matches = false;
      return Success;
    }
    if (LocaleInsensitveToLower(presentedByte) !=
        LocaleInsensitveToLower(referenceByte)) {
      matches = false;
      return Success;
    }
    if (presented.AtEnd()) {
      // Don't allow presented IDs to be absolute.
      if (presentedByte == '.') {
        return Result::ERROR_BAD_DER;
      }
      break;
    }
  }

  // Allow a relative presented DNS ID to match an absolute reference DNS ID,
  // unless we're matching a name constraint.
  if (!reference.AtEnd()) {
    if (referenceDNSIDRole != IDRole::NameConstraint) {
      uint8_t referenceByte;
      if (reference.Read(referenceByte) != Success) {
        return NotReached("read failed but not at end",
                          Result::FATAL_ERROR_LIBRARY_FAILURE);
      }
      if (referenceByte != '.') {
        matches = false;
        return Success;
      }
    }
    if (!reference.AtEnd()) {
      matches = false;
      return Success;
    }
  }

  matches = true;
  return Success;
}

// https://tools.ietf.org/html/rfc5280#section-4.2.1.10 says:
//
//     For IPv4 addresses, the iPAddress field of GeneralName MUST contain
//     eight (8) octets, encoded in the style of RFC 4632 (CIDR) to represent
//     an address range [RFC4632].  For IPv6 addresses, the iPAddress field
//     MUST contain 32 octets similarly encoded.  For example, a name
//     constraint for "class C" subnet 192.0.2.0 is represented as the
//     octets C0 00 02 00 FF FF FF 00, representing the CIDR notation
//     192.0.2.0/24 (mask 255.255.255.0).
Result
MatchPresentedIPAddressWithConstraint(Input presentedID,
                                      Input iPAddressConstraint,
                                      /*out*/ bool& foundMatch)
{
  if (presentedID.GetLength() != 4 && presentedID.GetLength() != 16) {
    return Result::ERROR_BAD_DER;
  }
  if (iPAddressConstraint.GetLength() != 8 &&
      iPAddressConstraint.GetLength() != 32) {
    return Result::ERROR_BAD_DER;
  }

  // an IPv4 address never matches an IPv6 constraint, and vice versa.
  if (presentedID.GetLength() * 2 != iPAddressConstraint.GetLength()) {
    foundMatch = false;
    return Success;
  }

  Reader constraint(iPAddressConstraint);
  Reader constraintAddress;
  Result rv = constraint.Skip(iPAddressConstraint.GetLength() / 2u,
                              constraintAddress);
  if (rv != Success) {
    return rv;
  }
  Reader constraintMask;
  rv = constraint.Skip(iPAddressConstraint.GetLength() / 2u, constraintMask);
  if (rv != Success) {
    return rv;
  }
  rv = der::End(constraint);
  if (rv != Success) {
    return rv;
  }

  Reader presented(presentedID);
  do {
    uint8_t presentedByte;
    rv = presented.Read(presentedByte);
    if (rv != Success) {
      return rv;
    }
    uint8_t constraintAddressByte;
    rv = constraintAddress.Read(constraintAddressByte);
    if (rv != Success) {
      return rv;
    }
    uint8_t constraintMaskByte;
    rv = constraintMask.Read(constraintMaskByte);
    if (rv != Success) {
      return rv;
    }
    foundMatch =
      ((presentedByte ^ constraintAddressByte) & constraintMaskByte) == 0;
  } while (foundMatch && !presented.AtEnd());

  return Success;
}

// AttributeTypeAndValue ::= SEQUENCE {
//   type     AttributeType,
//   value    AttributeValue }
//
// AttributeType ::= OBJECT IDENTIFIER
//
// AttributeValue ::= ANY -- DEFINED BY AttributeType
Result
ReadAVA(Reader& rdn,
        /*out*/ Input& type,
        /*out*/ uint8_t& valueTag,
        /*out*/ Input& value)
{
  return der::Nested(rdn, der::SEQUENCE, [&](Reader& ava) -> Result {
    Result rv = der::ExpectTagAndGetValue(ava, der::OIDTag, type);
    if (rv != Success) {
      return rv;
    }
    rv = der::ReadTagAndGetValue(ava, valueTag, value);
    if (rv != Success) {
      return rv;
    }
    return Success;
  });
}

// Names are sequences of RDNs. RDNS are sets of AVAs. That means that RDNs are
// unordered, so in theory we should match RDNs with equivalent AVAs that are
// in different orders. Within the AVAs are DirectoryNames that are supposed to
// be compared according to LDAP stringprep normalization rules (e.g.
// normalizing whitespace), consideration of different character encodings,
// etc. Indeed, RFC 5280 says we MUST deal with all of that.
//
// In practice, many implementations, including NSS, only match Names in a way
// that only meets a subset of the requirements of RFC 5280. Those
// normalization and character encoding conversion steps appear to be
// unnecessary for processing real-world certificates, based on experience from
// having used NSS in Firefox for many years.
//
// RFC 5280 also says "CAs issuing certificates with a restriction of the form
// directoryName SHOULD NOT rely on implementation of the full
// ISO DN name comparison algorithm. This implies name restrictions MUST
// be stated identically to the encoding used in the subject field or
// subjectAltName extension." It goes on to say, in the security
// considerations:
//
//     In addition, name constraints for distinguished names MUST be stated
//     identically to the encoding used in the subject field or
//     subjectAltName extension.  If not, then name constraints stated as
//     excludedSubtrees will not match and invalid paths will be accepted
//     and name constraints expressed as permittedSubtrees will not match
//     and valid paths will be rejected.  To avoid acceptance of invalid
//     paths, CAs SHOULD state name constraints for distinguished names as
//     permittedSubtrees wherever possible.
//
// For permittedSubtrees, the MUST-level requirement is relaxed for
// compatibility in the case of PrintableString and UTF8String. That is, if a
// name constraint has been encoded using UTF8String and the presented ID has
// been encoded with a PrintableString (or vice-versa), they are considered to
// match if they are equal everywhere except for the tag identifying the
// encoding. See bug 1150114.
//
// For excludedSubtrees, we simply prohibit any non-empty directoryName
// constraint to ensure we are not being too lenient. We support empty
// DirectoryName constraints in excludedSubtrees so that a CA can say "Do not
// allow any DirectoryNames in issued certificates."
Result
MatchPresentedDirectoryNameWithConstraint(NameConstraintsSubtrees subtreesType,
                                          Input presentedID,
                                          Input directoryNameConstraint,
                                          /*out*/ bool& matches)
{
  Reader constraintRDNs;
  Result rv = der::ExpectTagAndGetValueAtEnd(directoryNameConstraint,
                                             der::SEQUENCE, constraintRDNs);
  if (rv != Success) {
    return rv;
  }
  Reader presentedRDNs;
  rv = der::ExpectTagAndGetValueAtEnd(presentedID, der::SEQUENCE,
                                      presentedRDNs);
  if (rv != Success) {
    return rv;
  }

  switch (subtreesType) {
    case NameConstraintsSubtrees::permittedSubtrees:
      break; // dealt with below
    case NameConstraintsSubtrees::excludedSubtrees:
      if (!constraintRDNs.AtEnd() || !presentedRDNs.AtEnd()) {
        return Result::ERROR_CERT_NOT_IN_NAME_SPACE;
      }
      matches = true;
      return Success;
  }

  for (;;) {
    // The AVAs have to be fully equal, but the constraint RDNs just need to be
    // a prefix of the presented RDNs.
    if (constraintRDNs.AtEnd()) {
      matches = true;
      return Success;
    }
    if (presentedRDNs.AtEnd()) {
      matches = false;
      return Success;
    }
    Reader constraintRDN;
    rv = der::ExpectTagAndGetValue(constraintRDNs, der::SET, constraintRDN);
    if (rv != Success) {
      return rv;
    }
    Reader presentedRDN;
    rv = der::ExpectTagAndGetValue(presentedRDNs, der::SET, presentedRDN);
    if (rv != Success) {
      return rv;
    }
    while (!constraintRDN.AtEnd() && !presentedRDN.AtEnd()) {
      Input constraintType;
      uint8_t constraintValueTag;
      Input constraintValue;
      rv = ReadAVA(constraintRDN, constraintType, constraintValueTag,
                   constraintValue);
      if (rv != Success) {
        return rv;
      }
      Input presentedType;
      uint8_t presentedValueTag;
      Input presentedValue;
      rv = ReadAVA(presentedRDN, presentedType, presentedValueTag,
                   presentedValue);
      if (rv != Success) {
        return rv;
      }
      // TODO (bug 1155767): verify that if an AVA is a PrintableString it
      // consists only of characters valid for PrintableStrings.
      bool avasMatch =
        InputsAreEqual(constraintType, presentedType) &&
        InputsAreEqual(constraintValue, presentedValue) &&
        (constraintValueTag == presentedValueTag ||
         (constraintValueTag == der::Tag::UTF8String &&
          presentedValueTag == der::Tag::PrintableString) ||
         (constraintValueTag == der::Tag::PrintableString &&
          presentedValueTag == der::Tag::UTF8String));
      if (!avasMatch) {
        matches = false;
        return Success;
      }
    }
    if (!constraintRDN.AtEnd() || !presentedRDN.AtEnd()) {
      matches = false;
      return Success;
    }
  }
}

// RFC 5280 says:
//
//     The format of an rfc822Name is a "Mailbox" as defined in Section 4.1.2
//     of [RFC2821]. A Mailbox has the form "Local-part@Domain".  Note that a
//     Mailbox has no phrase (such as a common name) before it, has no comment
//     (text surrounded in parentheses) after it, and is not surrounded by "<"
//     and ">".  Rules for encoding Internet mail addresses that include
//     internationalized domain names are specified in Section 7.5.
//
// and:
//
//     A name constraint for Internet mail addresses MAY specify a
//     particular mailbox, all addresses at a particular host, or all
//     mailboxes in a domain.  To indicate a particular mailbox, the
//     constraint is the complete mail address.  For example,
//     "root@example.com" indicates the root mailbox on the host
//     "example.com".  To indicate all Internet mail addresses on a
//     particular host, the constraint is specified as the host name.  For
//     example, the constraint "example.com" is satisfied by any mail
//     address at the host "example.com".  To specify any address within a
//     domain, the constraint is specified with a leading period (as with
//     URIs).  For example, ".example.com" indicates all the Internet mail
//     addresses in the domain "example.com", but not Internet mail
//     addresses on the host "example.com".

bool
IsValidRFC822Name(Input input)
{
  Reader reader(input);

  // Local-part@.
  bool startOfAtom = true;
  for (;;) {
    uint8_t presentedByte;
    if (reader.Read(presentedByte) != Success) {
      return false;
    }
    switch (presentedByte) {
      // atext is defined in https://tools.ietf.org/html/rfc2822#section-3.2.4
      case 'A': case 'a': case 'N': case 'n': case '0': case '!': case '#':
      case 'B': case 'b': case 'O': case 'o': case '1': case '$': case '%':
      case 'C': case 'c': case 'P': case 'p': case '2': case '&': case '\'':
      case 'D': case 'd': case 'Q': case 'q': case '3': case '*': case '+':
      case 'E': case 'e': case 'R': case 'r': case '4': case '-': case '/':
      case 'F': case 'f': case 'S': case 's': case '5': case '=': case '?':
      case 'G': case 'g': case 'T': case 't': case '6': case '^': case '_':
      case 'H': case 'h': case 'U': case 'u': case '7': case '`': case '{':
      case 'I': case 'i': case 'V': case 'v': case '8': case '|': case '}':
      case 'J': case 'j': case 'W': case 'w': case '9': case '~':
      case 'K': case 'k': case 'X': case 'x':
      case 'L': case 'l': case 'Y': case 'y':
      case 'M': case 'm': case 'Z': case 'z':
        startOfAtom = false;
        break;

      case '.':
        if (startOfAtom) {
          return false;
        }
        startOfAtom = true;
        break;

      case '@':
      {
        if (startOfAtom) {
          return false;
        }
        Input domain;
        if (reader.SkipToEnd(domain) != Success) {
          return false;
        }
        return IsValidDNSID(domain, IDRole::PresentedID, AllowWildcards::No);
      }

      default:
        return false;
    }
  }
}

Result
MatchPresentedRFC822NameWithReferenceRFC822Name(Input presentedRFC822Name,
                                                IDRole referenceRFC822NameRole,
                                                Input referenceRFC822Name,
                                                /*out*/ bool& matches)
{
  if (!IsValidRFC822Name(presentedRFC822Name)) {
    return Result::ERROR_BAD_DER;
  }
  Reader presented(presentedRFC822Name);

  switch (referenceRFC822NameRole)
  {
    case IDRole::PresentedID:
      return Result::FATAL_ERROR_INVALID_ARGS;

    case IDRole::ReferenceID:
      break;

    case IDRole::NameConstraint:
    {
      if (InputContains(referenceRFC822Name, '@')) {
        // The constraint is of the form "Local-part@Domain".
        break;
      }

      // The constraint is of the form "example.com" or ".example.com".

      // Skip past the '@' in the presented ID.
      for (;;) {
        uint8_t presentedByte;
        if (presented.Read(presentedByte) != Success) {
          return Result::FATAL_ERROR_LIBRARY_FAILURE;
        }
        if (presentedByte == '@') {
          break;
        }
      }

      Input presentedDNSID;
      if (presented.SkipToEnd(presentedDNSID) != Success) {
        return Result::FATAL_ERROR_LIBRARY_FAILURE;
      }

      return MatchPresentedDNSIDWithReferenceDNSID(
               presentedDNSID, AllowWildcards::No,
               AllowDotlessSubdomainMatches::No, IDRole::NameConstraint,
               referenceRFC822Name, matches);
    }
  }

  if (!IsValidRFC822Name(referenceRFC822Name)) {
    return Result::ERROR_BAD_DER;
  }

  Reader reference(referenceRFC822Name);

  for (;;) {
    uint8_t presentedByte;
    if (presented.Read(presentedByte) != Success) {
      matches = reference.AtEnd();
      return Success;
    }
    uint8_t referenceByte;
    if (reference.Read(referenceByte) != Success) {
      matches = false;
      return Success;
    }
    if (LocaleInsensitveToLower(presentedByte) !=
        LocaleInsensitveToLower(referenceByte)) {
      matches = false;
      return Success;
    }
  }
}

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

// On Windows and maybe other platforms, OS-provided IP address parsing
// functions might fail if the protocol (IPv4 or IPv6) has been disabled, so we
// can't rely on them.
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
  size_t componentsToMove = static_cast<size_t>(numComponents -
                                                contractionIndex);
  memmove(address + (2u * static_cast<size_t>(8 - componentsToMove)),
          address + (2u * static_cast<size_t>(contractionIndex)),
          componentsToMove * 2u);
  // Fill in the contracted area with zeros.
  std::fill_n(address + 2u * static_cast<size_t>(contractionIndex),
              (8u - static_cast<size_t>(numComponents)) * 2u, static_cast<uint8_t>(0u));

  return true;
}

} // unnamed namespace

// On Windows and maybe other platforms, OS-provided IP address parsing
// functions might fail if the protocol (IPv4 or IPv6) has been disabled, so we
// can't rely on them.
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
  return IsValidDNSID(hostname, IDRole::ReferenceID, AllowWildcards::No);
}

bool
IsValidPresentedDNSID(Input hostname)
{
  return IsValidDNSID(hostname, IDRole::PresentedID, AllowWildcards::Yes);
}

namespace {

// RFC 5280 Section 4.2.1.6 says that a dNSName "MUST be in the 'preferred name
// syntax', as specified by Section 3.5 of [RFC1034] and as modified by Section
// 2.1 of [RFC1123]" except "a dNSName of ' ' MUST NOT be used." Additionally,
// we allow underscores for compatibility with existing practice.
bool
IsValidDNSID(Input hostname, IDRole idRole, AllowWildcards allowWildcards)
{
  if (hostname.GetLength() > 253) {
    return false;
  }

  Reader input(hostname);

  if (idRole == IDRole::NameConstraint && input.AtEnd()) {
    return true;
  }

  size_t dotCount = 0;
  size_t labelLength = 0;
  bool labelIsAllNumeric = false;
  bool labelEndsWithHyphen = false;

  // Only presented IDs are allowed to have wildcard labels. And, like
  // Chromium, be stricter than RFC 6125 requires by insisting that a
  // wildcard label consist only of '*'.
  bool isWildcard = allowWildcards == AllowWildcards::Yes && input.Peek('*');
  bool isFirstByte = !isWildcard;
  if (isWildcard) {
    Result rv = input.Skip(1);
    if (rv != Success) {
      assert(false);
      return false;
    }

    uint8_t b;
    rv = input.Read(b);
    if (rv != Success) {
      return false;
    }
    if (b != '.') {
      return false;
    }
    ++dotCount;
  }

  do {
    static const size_t MAX_LABEL_LENGTH = 63;

    uint8_t b;
    if (input.Read(b) != Success) {
      return false;
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
      // We allow underscores for compatibility with existing practices.
      // See bug 1136616.
      case '_':
        labelIsAllNumeric = false;
        labelEndsWithHyphen = false;
        ++labelLength;
        if (labelLength > MAX_LABEL_LENGTH) {
          return false;
        }
        break;

      case '.':
        ++dotCount;
        if (labelLength == 0 &&
            (idRole != IDRole::NameConstraint || !isFirstByte)) {
          return false;
        }
        if (labelEndsWithHyphen) {
          return false; // Labels must not end with a hyphen.
        }
        labelLength = 0;
        break;

      default:
        return false; // Invalid character.
    }
    isFirstByte = false;
  } while (!input.AtEnd());

  // Only reference IDs, not presented IDs or name constraints, may be
  // absolute.
  if (labelLength == 0 && idRole != IDRole::ReferenceID) {
    return false;
  }

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
