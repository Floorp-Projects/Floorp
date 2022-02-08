/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BaseAndGeckoProfilerDetail.h"

#include <limits>
#include <string_view>

namespace mozilla::profiler::detail {

constexpr std::string_view scPidPrefix = "pid:";

// Convert a C string to a BaseProfilerProcessId. Return unspecified
// BaseProfilerProcessId if the string is not exactly a valid pid.
static baseprofiler::BaseProfilerProcessId StringToPid(const char* aString) {
  if (!aString || aString[0] == '\0') {
    // Null or empty.
    return baseprofiler::BaseProfilerProcessId{};
  }

  if (aString[0] == '0') {
    if (aString[1] != '\0') {
      // Don't accept leading zeroes.
      return baseprofiler::BaseProfilerProcessId{};
    }
    return baseprofiler::BaseProfilerProcessId::FromNumber(0);
  }

  using PidNumber = baseprofiler::BaseProfilerProcessId::NumberType;
  PidNumber pid = 0;
  for (;;) {
    const char c = *aString;
    if (c == '\0') {
      break;
    }
    if (c < '0' || c > '9') {
      // Only accept decimal digits.
      return baseprofiler::BaseProfilerProcessId{};
    }
    static_assert(!std::numeric_limits<PidNumber>::is_signed,
                  "The following relies on unsigned arithmetic");
    PidNumber newPid = pid * 10u + PidNumber(c - '0');
    if (newPid < pid) {
      // Unsigned overflow.
      return baseprofiler::BaseProfilerProcessId{};
    }
    pid = newPid;
    ++aString;
  }
  return baseprofiler::BaseProfilerProcessId::FromNumber(pid);
}

[[nodiscard]] MFBT_API bool FilterHasPid(
    const char* aFilter, baseprofiler::BaseProfilerProcessId aPid) {
  if (strncmp(aFilter, scPidPrefix.data(), scPidPrefix.length()) != 0) {
    // The filter is not starting with "pid:".
    return false;
  }

  return StringToPid(aFilter + scPidPrefix.length()) == aPid;
}

[[nodiscard]] MFBT_API bool FiltersExcludePid(
    Span<const char* const> aFilters,
    baseprofiler::BaseProfilerProcessId aPid) {
  if (aFilters.empty()) {
    return false;
  }

  // First, check if the list only contains "pid:..." strings.
  for (const char* const filter : aFilters) {
    if (strncmp(filter, scPidPrefix.data(), scPidPrefix.length()) != 0) {
      // At least one filter is *not* a "pid:...", our pid is not excluded.
      return false;
    }
  }

  // Here, all filters start with "pid:". Check if the given pid is included.
  for (const char* const filter : aFilters) {
    if (StringToPid(filter + scPidPrefix.length()) == aPid) {
      // Our pid is present, so it's not excluded.
      return false;
    }
  }
  // Our pid was not in a list of only pids, so it's excluded.
  return true;
}

}  // namespace mozilla::profiler::detail
