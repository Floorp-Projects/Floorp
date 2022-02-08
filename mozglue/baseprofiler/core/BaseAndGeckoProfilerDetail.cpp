/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BaseAndGeckoProfilerDetail.h"

#include <cstring>
#include <string>
#include <string_view>

namespace mozilla::profiler::detail {

constexpr std::string_view scPidPrefix = "pid:";

[[nodiscard]] MFBT_API bool FilterHasPid(
    const char* aFilter, baseprofiler::BaseProfilerProcessId aPid) {
  if (strncmp(aFilter, scPidPrefix.data(), scPidPrefix.length()) != 0) {
    // The filter is not starting with "pid:".
    return false;
  }

  const std::string pidString = std::to_string(aPid.ToNumber());
  return pidString == (aFilter + scPidPrefix.length());
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
  const std::string pidString = std::to_string(aPid.ToNumber());
  for (const char* const filter : aFilters) {
    if (pidString == (filter + scPidPrefix.length())) {
      // Our pid is present, so it's not excluded.
      return false;
    }
  }
  // Our pid was not in a list of only pids, so it's excluded.
  return true;
}

}  // namespace mozilla::profiler::detail
