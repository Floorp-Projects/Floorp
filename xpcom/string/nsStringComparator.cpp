/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ctype.h>
#include "nsAString.h"
#include "plstr.h"

#include "nsTStringComparator.cpp"

int nsCaseInsensitiveCStringComparator(const char* aLhs, const char* aRhs,
                                       size_t aLhsLength, size_t aRhsLength) {
#if defined(LIBFUZZER) && defined(LINUX)
  // Make sure libFuzzer can see this string compare by calling the POSIX
  // native function which is intercepted. We also call this if the lengths
  // don't match so libFuzzer can at least see a partial string, but we throw
  // away the result afterwards again.
  int32_t result =
      int32_t(strncasecmp(aLhs, aRhs, std::min(aLhsLength, aRhsLength)));

  if (aLhsLength != aRhsLength) {
    return (aLhsLength > aRhsLength) ? 1 : -1;
  }
#else
  if (aLhsLength != aRhsLength) {
    return (aLhsLength > aRhsLength) ? 1 : -1;
  }
  int32_t result = int32_t(PL_strncasecmp(aLhs, aRhs, aLhsLength));
#endif
  // Egads. PL_strncasecmp is returning *very* negative numbers.
  // Some folks expect -1,0,1, so let's temper its enthusiasm.
  if (result < 0) {
    result = -1;
  }
  return result;
}
