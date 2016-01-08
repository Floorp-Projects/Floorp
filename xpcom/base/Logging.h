/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_logging_h
#define mozilla_logging_h

#include "prlog.h"

#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Likely.h"

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

/**
 * Safely converts an integer into a valid LogLevel.
 */
LogLevel ToLogLevel(int32_t aLevel);

class LogModule
{
public:
  /**
   * Retrieves the module with the given name. If it does not already exist
   * it will be created.
   *
   * @param aName The name of the module.
   * @return A log module for the given name. This may be shared.
   */
#if !defined(MOZILLA_XPCOMRT_API)
  static LogModule* Get(const char* aName);
#else
  // For simplicity, libxpcomrt doesn't supoort logging.
  static LogModule* Get(const char* aName) { return nullptr; }
#endif

  static void Init();

  /**
   * Indicates whether or not the given log level is enabled.
   */
  bool ShouldLog(LogLevel aLevel) const { return mLevel >= aLevel; }

  /**
   * Retrieves the log module's current level.
   */
  LogLevel Level() const { return mLevel; }

  /**
   * Sets the log module's level.
   */
  void SetLevel(LogLevel level) { mLevel = level; }

private:
  friend class LogModuleManager;

  explicit LogModule(LogLevel aLevel) : mLevel(aLevel) {}

  LogModule(LogModule&) = delete;
  LogModule& operator=(const LogModule&) = delete;

  Atomic<LogLevel, Relaxed> mLevel;
};

/**
 * Helper class that lazy loads the given log module. This is safe to use for
 * declaring static references to log modules and can be used as a replacement
 * for accessing a LogModule directly.
 *
 * Example usage:
 *   static LazyLogModule sLayoutLog("layout");
 *
 *   void Foo() {
 *     MOZ_LOG(sLayoutLog, LogLevel::Verbose, ("Entering foo"));
 *   }
 */
class LazyLogModule final
{
public:
  explicit MOZ_CONSTEXPR LazyLogModule(const char* aLogName)
    : mLogName(aLogName)
    , mLog(nullptr)
  {
  }

  operator LogModule*()
  {
    // NB: The use of an atomic makes the reading and assignment of mLog
    //     thread-safe. There is a small chance that mLog will be set more
    //     than once, but that's okay as it will be set to the same LogModule
    //     instance each time. Also note LogModule::Get is thread-safe.
    LogModule* tmp = mLog;
    if (MOZ_UNLIKELY(!tmp)) {
      tmp = LogModule::Get(mLogName);
      mLog = tmp;
    }

    return tmp;
  }

private:
  const char* const mLogName;
  Atomic<LogModule*, ReleaseAcquire> mLog;
};

namespace detail {

inline bool log_test(const PRLogModuleInfo* module, LogLevel level) {
  MOZ_ASSERT(level != LogLevel::Disabled);
  return module && module->level >= static_cast<int>(level);
}

inline bool log_test(const LogModule* module, LogLevel level) {
  MOZ_ASSERT(level != LogLevel::Disabled);
  return module && module->ShouldLog(level);
}

} // namespace detail

} // namespace mozilla

#define MOZ_LOG_TEST(_module,_level) mozilla::detail::log_test(_module, _level)

#define MOZ_LOG(_module,_level,_args)     \
  PR_BEGIN_MACRO             \
    if (MOZ_LOG_TEST(_module,_level)) { \
      PR_LogPrint _args;         \
    }                     \
  PR_END_MACRO

#undef PR_LOG
#undef PR_LOG_TEST

/*
 * __func__ was standardized in C++11 and is supported by clang, gcc, and MSVC
 * 2015. Here we polyfill __func__ for earlier versions of MSVC.
 * http://blogs.msdn.com/b/vcblog/archive/2015/06/19/c-11-14-17-features-in-vs-2015-rtm.aspx
 */
#ifdef _MSC_VER
#  if _MSC_VER < 1900
#    define __func__ __FUNCTION__
#  endif
#endif

#endif // mozilla_logging_h
