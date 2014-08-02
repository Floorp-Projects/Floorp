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

#ifndef mozilla_pkix__Time_h
#define mozilla_pkix__Time_h

#include <ctime>
#include <limits>
#include <stdint.h>

#include "pkix/Result.h"

namespace mozilla { namespace pkix {

// Time with a range from the first second of year 0 (AD) through at least the
// last second of year 9999, which is the range of legal times in X.509 and
// OCSP. This type has second-level precision. The time zone is always UTC.
//
// Pass by value, not by reference.
class Time
{
public:
  // Construct an uninitilized instance.
  //
  // This will fail to compile because there is no default constructor:
  //    Time x;
  //
  // This will succeed, leaving the time uninitialized:
  //    Time x(Time::uninitialized);
  enum Uninitialized { uninitialized };
  Time(Uninitialized) { }

  bool operator==(const Time& other) const
  {
    return elapsedSecondsAD == other.elapsedSecondsAD;
  }
  bool operator>(const Time& other) const
  {
    return elapsedSecondsAD > other.elapsedSecondsAD;
  }
  bool operator>=(const Time& other) const
  {
    return elapsedSecondsAD >= other.elapsedSecondsAD;
  }
  bool operator<(const Time& other) const
  {
    return elapsedSecondsAD < other.elapsedSecondsAD;
  }
  bool operator<=(const Time& other) const
  {
    return elapsedSecondsAD <= other.elapsedSecondsAD;
  }

  Result AddSeconds(uint64_t seconds)
  {
    if (std::numeric_limits<uint64_t>::max() - elapsedSecondsAD
          < seconds) {
      return Result::FATAL_ERROR_INVALID_ARGS; // integer overflow
    }
    elapsedSecondsAD += seconds;
    return Success;
  }

  Result SubtractSeconds(uint64_t seconds)
  {
    if (seconds > elapsedSecondsAD) {
      return Result::FATAL_ERROR_INVALID_ARGS; // integer overflow
    }
    elapsedSecondsAD -= seconds;
    return Success;
  }

  static const uint64_t ONE_DAY_IN_SECONDS
    = UINT64_C(24) * UINT64_C(60) * UINT64_C(60);

private:
  // This constructor is hidden to prevent accidents like this:
  //
  //    Time foo(time_t t)
  //    {
  //      // WRONG! 1970-01-01-00:00:00 == time_t(0), but not Time(0)!
  //      return Time(t);
  //    }
  explicit Time(uint64_t elapsedSecondsAD)
    : elapsedSecondsAD(elapsedSecondsAD)
  {
  }
  friend Time TimeFromElapsedSecondsAD(uint64_t);

  uint64_t elapsedSecondsAD;
};

inline Time TimeFromElapsedSecondsAD(uint64_t elapsedSecondsAD)
{
  return Time(elapsedSecondsAD);
}

Time Now();

} } // namespace mozilla::pkix

#endif // mozilla_pkix__Time_h
