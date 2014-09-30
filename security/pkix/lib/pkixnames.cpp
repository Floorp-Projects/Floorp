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

#include "pkix/Input.h"

namespace mozilla { namespace pkix {

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
