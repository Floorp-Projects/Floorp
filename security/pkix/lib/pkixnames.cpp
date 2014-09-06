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

// This code attempts to implement RFC6125 name matching. It also attempts to
// give the same results as the Chromium implementation
// (X509Certificate::VerifyHostname) when both are given clean input (no
// leading whitespace, etc.)
//
// On Windows and maybe other platforms, OS-provided IP address parsing
// functions might fail if the protocol (IPv4 or IPv6) has been disabled, so we
// can't rely on them.

#include "pkix/bind.h"
#include "pkixutil.h"

namespace mozilla { namespace pkix {

namespace {

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
