/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_logging_h
#define mozilla_logging_h

#include "prlog.h"

#include "mozilla/Assertions.h"

// This file is a placeholder for a replacement to the NSPR logging framework
// that is defined in prlog.h. Currently it is just a pass through, but as
// work progresses more functionality will be swapped out in favor of
// mozilla logging implementations.

namespace mozilla {

// While not a 100% mapping to PR_LOG's numeric values, mozilla::LogLevel does
// maintain a direct mapping for the Disabled, Debug and Verbose levels.
//
// Mappings of LogLevel to PR_LOG's numeric values:
//
//   +---------+------------------+-----------------+
//   | Numeric | NSPR Logging     | Mozilla Logging |
//   +---------+------------------+-----------------+
//   |       0 | PR_LOG_NONE      | Disabled        |
//   |       1 | PR_LOG_ALWAYS    | Error           |
//   |       2 | PR_LOG_ERROR     | Warning         |
//   |       3 | PR_LOG_WARNING   | Info            |
//   |       4 | PR_LOG_DEBUG     | Debug           |
//   |       5 | PR_LOG_DEBUG + 1 | Verbose         |
//   +---------+------------------+-----------------+
//
enum class LogLevel {
  Disabled = 0,
  Error,
  Warning,
  Info,
  Debug,
  Verbose,
};

namespace detail {

inline bool log_test(const PRLogModuleInfo* module, LogLevel level) {
  MOZ_ASSERT(level != LogLevel::Disabled);
  return module && module->level >= static_cast<int>(level);
}

}

}

#define MOZ_LOG_TEST(_module,_level) mozilla::detail::log_test(_module, _level)

#define MOZ_LOG(_module,_level,_args)     \
  PR_BEGIN_MACRO             \
    if (MOZ_LOG_TEST(_module,_level)) { \
      PR_LogPrint _args;         \
    }                     \
  PR_END_MACRO

#undef PR_LOG
#undef PR_LOG_TEST

#endif // mozilla_logging_h

