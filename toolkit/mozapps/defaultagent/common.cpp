/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "common.h"

#include <windows.h>

ULONGLONG GetCurrentTimestamp() {
  FILETIME filetime;
  GetSystemTimeAsFileTime(&filetime);
  ULARGE_INTEGER integerTime;
  integerTime.u.LowPart = filetime.dwLowDateTime;
  integerTime.u.HighPart = filetime.dwHighDateTime;
  return integerTime.QuadPart;
}

// Passing a zero as the second argument (or omitting it) causes the function
// to get the current time rather than using a passed value.
ULONGLONG SecondsPassedSince(ULONGLONG initialTime,
                             ULONGLONG currentTime /* = 0 */) {
  if (currentTime == 0) {
    currentTime = GetCurrentTimestamp();
  }
  // Since this is returning an unsigned value, let's make sure we don't try to
  // return anything negative
  if (initialTime >= currentTime) {
    return 0;
  }

  // These timestamps are expressed in 100-nanosecond intervals
  return (currentTime - initialTime) / 10  // To microseconds
         / 1000                            // To milliseconds
         / 1000;                           // To seconds
}
