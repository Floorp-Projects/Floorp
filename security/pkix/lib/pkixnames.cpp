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

uint8_t LocaleInsensitveToLower(uint8_t a);
bool StartsWithIDNALabel(Input id);

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
// The referenceDNSIDWasVerifiedAsValid parameter must be the result of calling
// IsValidDNSName.
//
// RFC6125 says that a wildcard label may be of the form <x>*<y>.<DNSID>, where
// <x> and/or <y> may be empty. However, NSS requires <y> to be empty, and we
// follow NSS's stricter policy by accepting wildcards only of the form
// <x>*.<DNSID>, where <x> may be empty.
bool
PresentedDNSIDMatchesReferenceDNSID(Input presentedDNSID,
                                    Input referenceDNSID,
                                    bool referenceDNSIDWasVerifiedAsValid)
{
  assert(referenceDNSIDWasVerifiedAsValid);
  if (!referenceDNSIDWasVerifiedAsValid) {
    return false;
  }

  Reader presented(presentedDNSID);
  Reader reference(referenceDNSID);

  size_t currentLabel = 0;
  bool hasWildcardLabel = false;
  bool lastPresentedByteWasDot = false;
  bool firstPresentedByteIsWildcard = presented.Peek('*');

  do {
    uint8_t presentedByte;
    if (presented.Read(presentedByte) != Success) {
      return false; // Reject completely empty input.
    }
    if (presentedByte == '*' && currentLabel == 0) {
      hasWildcardLabel = true;

      // Like NSS, be stricter than RFC6125 requires by insisting that the
      // "*" must be the last character in the label. This also prevents
      // multiple "*" in the label.
      if (!presented.Peek('.')) {
        return false;
      }

      // RFC6125 says that we shouldn't accept wildcards within an IDN A-Label.
      //
      // We also don't allow a non-IDN presented ID label to match an IDN
      // reference ID label, except when the entire presented ID label is "*".
      // This avoids confusion when matching a presented ID like
      // "xn-*.example.org" against "xn--www.example.org" (which attempts to
      // abuse the punycode syntax) or "www-*.example.org" against
      // "xn--www--ep4c4a2kpf" (which makes sense to match, semantically, but
      // no implementations actually do).
      //
      // XXX: The consequence of this is that we effectively discriminate
      // against users of languages that cannot be encoded with ASCII.
      if (!firstPresentedByteIsWildcard) {
        if (StartsWithIDNALabel(presentedDNSID)) {
          return false;
        }
        if (StartsWithIDNALabel(referenceDNSID)) {
          return false;
        }
      }

      // RFC 6125 is unclear about whether "www*.example.org" matches
      // "www.example.org". The Chromium test suite has this test:
      //
      //    { false, "w.bar.foo.com", "w*.bar.foo.com" },
      //
      // We agree with Chromium by forbidding "*" from expanding to the empty
      // string.
      uint8_t referenceByte;
      if (reference.Read(referenceByte) != Success) {
        return false;
      }
      if (referenceByte == '.') {
        return false;
      }
      while (!reference.Peek('.')) {
        if (reference.Read(referenceByte) != Success) {
          return false;
        }
      }
    } else {
      if (presentedByte == '.') {
        // This check is needed to prevent ".." at the end of the presented ID
        // from being accepted.
        if (lastPresentedByteWasDot) {
          return false;
        }
        lastPresentedByteWasDot = true;

        if (!presented.AtEnd()) {
          ++currentLabel;
        }
      } else {
        lastPresentedByteWasDot = false;
      }

      // The presented ID may have a terminating dot '.' to mark it as
      // absolute, and it still matches a reference ID without that
      // terminating dot.
      if (presentedByte != '.' || !presented.AtEnd() || !reference.AtEnd()) {
        uint8_t referenceByte;
        if (reference.Read(referenceByte) != Success) {
          return false;
        }
        if (LocaleInsensitveToLower(presentedByte) !=
            LocaleInsensitveToLower(referenceByte)) {
          return false;
        }
      }
    }
  } while (!presented.AtEnd());

  // The reference ID may have a terminating dot '.' to mark it as absolute,
  // and a presented ID without that terminating dot still matches it.
  static const uint8_t DOT[1] = { '.' };
  if (!reference.AtEnd() && !reference.MatchRest(DOT)) {
    return false;
  }

  if (hasWildcardLabel) {
    // Like NSS, we require at least two labels after the wildcard.
    if (currentLabel < 2) {
      return false;
    }

    // TODO(bug XXXXXXX): Allow the TrustDomain to control this on a
    // per-eTLD+1 basis, similar to Chromium. Even then, it might be better to
    // still enforce that there are at least two labels after the wildcard.

    // TODO(bug XXXXXXX): Wildcards are not allowed for EV certificates.
    // Provide an option to indicate whether wildcards should be matched, for
    // the purpose of helping the application enforce this.
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
IsValidDNSName(Input hostname)
{
  if (hostname.GetLength() > 253) {
    return false;
  }

  Reader input(hostname);
  size_t labelLength = 0;
  bool labelIsAllNumeric = false;
  bool endsWithHyphen = false;

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
        endsWithHyphen = true;
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
        endsWithHyphen = false;
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
        endsWithHyphen = false;
        ++labelLength;
        if (labelLength > MAX_LABEL_LENGTH) {
          return false;
        }
        break;

      case '.':
        if (labelLength == 0) {
          return false;
        }
        if (endsWithHyphen) {
          return false; // Labels must not end with a hyphen.
        }
        labelLength = 0;
        break;

      default:
        return false; // Invalid character.
    }
  } while (!input.AtEnd());

  if (endsWithHyphen) {
    return false; // Labels must not end with a hyphen.
  }

  if (labelIsAllNumeric) {
    return false; // Last label must not be all numeric.
  }

  return true;
}

} } // namespace mozilla::pkix
