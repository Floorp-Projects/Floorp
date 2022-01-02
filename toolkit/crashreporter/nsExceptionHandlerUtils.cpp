/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsExceptionHandlerUtils.h"

#include "double-conversion/double-conversion.h"

// Format a non-negative double to a string, without using C-library functions,
// which need to be avoided (.e.g. bug 1240160, comment 10).  Return false if
// we failed to get the formatting done correctly.
bool SimpleNoCLibDtoA(double aValue, char* aBuffer, int aBufferLength) {
  // aBufferLength is the size of the buffer.  Be paranoid.
  aBuffer[aBufferLength - 1] = '\0';

  if (aValue < 0) {
    return false;
  }

  int length, point, i;
  bool sign;
  bool ok = true;
  double_conversion::DoubleToStringConverter::DoubleToAscii(
      aValue, double_conversion::DoubleToStringConverter::SHORTEST, 8, aBuffer,
      aBufferLength, &sign, &length, &point);

  // length does not account for the 0 terminator.
  if (length > point && (length + 1) < (aBufferLength - 1)) {
    // We have to insert a decimal point.  Not worried about adding a leading
    // zero in the < 1 (point == 0) case.
    aBuffer[length + 1] = '\0';
    for (i = length; i > point; i -= 1) {
      aBuffer[i] = aBuffer[i - 1];
    }
    aBuffer[i] = '.';  // Not worried about locales
  } else if (length < point) {
    // Trailing zeros scenario
    for (i = length; i < point; i += 1) {
      if (i >= aBufferLength - 2) {
        ok = false;
      }
      aBuffer[i] = '0';
    }
    aBuffer[i] = '\0';
  }
  return ok;
}
