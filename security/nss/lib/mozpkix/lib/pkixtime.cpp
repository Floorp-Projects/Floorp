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

#include "mozpkix/Time.h"
#include "mozpkix/pkixutil.h"

#ifdef _WINDOWS
#ifdef _MSC_VER
#pragma warning(push, 3)
#endif
#include "windows.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#else
#include "sys/time.h"
#endif

namespace mozilla { namespace pkix {

Time
Now()
{
  uint64_t seconds;

#ifdef _WINDOWS
  // "Contains a 64-bit value representing the number of 100-nanosecond
  // intervals since January 1, 1601 (UTC)."
  //   - http://msdn.microsoft.com/en-us/library/windows/desktop/ms724284(v=vs.85).aspx
  FILETIME ft;
  GetSystemTimeAsFileTime(&ft);
  uint64_t ft64 = (static_cast<uint64_t>(ft.dwHighDateTime) << 32) |
                  ft.dwLowDateTime;
  seconds = (DaysBeforeYear(1601) * Time::ONE_DAY_IN_SECONDS) +
            ft64 / (1000u * 1000u * 1000u / 100u);
#else
  // "The gettimeofday() function shall obtain the current time, expressed as
  // seconds and microseconds since the Epoch."
  //   - http://pubs.opengroup.org/onlinepubs/009695399/functions/gettimeofday.html
  timeval tv;
  (void) gettimeofday(&tv, nullptr);
  seconds = (DaysBeforeYear(1970) * Time::ONE_DAY_IN_SECONDS) +
            static_cast<uint64_t>(tv.tv_sec);
#endif

  return TimeFromElapsedSecondsAD(seconds);
}

Time
TimeFromEpochInSeconds(uint64_t secondsSinceEpoch)
{
  uint64_t seconds = (DaysBeforeYear(1970) * Time::ONE_DAY_IN_SECONDS) +
                     secondsSinceEpoch;
  return TimeFromElapsedSecondsAD(seconds);
}

Result
SecondsSinceEpochFromTime(Time time, uint64_t* outSeconds)
{
  if (!outSeconds) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }
  Time epoch = TimeFromEpochInSeconds(0);
  if (time < epoch) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }
  *outSeconds = Duration(time, epoch).durationInSeconds;
  return Result::Success;
}

} } // namespace mozilla::pkix
