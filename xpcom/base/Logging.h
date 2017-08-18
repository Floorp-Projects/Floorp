/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_logging_h
#define mozilla_logging_h

#include <string.h>
#include <stdarg.h>

#include "mozilla/Assertions.h"
#include "mozilla/Atomics.h"
#include "mozilla/Attributes.h"
#include "mozilla/Likely.h"

// We normally have logging enabled everywhere, but measurements showed that
// having logging enabled on Android is quite expensive (hundreds of kilobytes
// for both the format strings for logging and the code to perform all the
// logging calls).  Because retrieving logs from a mobile device is
// comparatively more difficult for Android than it is for desktop and because
// desktop machines tend to be less space/bandwidth-constrained than Android
// devices, we've chosen to leave logging enabled on desktop, but disabled on
// Android.  Given that logging can still be useful for development purposes,
// however, we leave logging enabled on Android developer builds.
#if !defined(ANDROID) || !defined(RELEASE_OR_BETA)
#define MOZ_LOGGING_ENABLED 1
#else
#define MOZ_LOGGING_ENABLED 0
#endif

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
  ~LogModule() { ::free(mName); }

  /**
   * Retrieves the module with the given name. If it does not already exist
   * it will be created.
   *
   * @param aName The name of the module.
   * @return A log module for the given name. This may be shared.
   */
  static LogModule* Get(const char* aName);

  static void Init();

  /**
   * Sets the log file to the given filename.
   */
  static void SetLogFile(const char* aFilename);

  /**
   * @param aBuffer - pointer to a buffer
   * @param aLength - the length of the buffer
   *
   * @return the actual length of the filepath.
   */
  static uint32_t GetLogFile(char *aBuffer, size_t aLength);

  /**
   * @param aAddTimestamp If we should log a time stamp with every message.
   */
  static void SetAddTimestamp(bool aAddTimestamp);

  /**
   * @param aIsSync If we should flush the file after every logged message.
   */
  static void SetIsSync(bool aIsSync);

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

  /**
   * Print a log message for this module.
   */
  void Printv(LogLevel aLevel, const char* aFmt, va_list aArgs) const MOZ_FORMAT_PRINTF(3, 0);

  /**
   * Retrieves the module name.
   */
  const char* Name() const { return mName; }

private:
  friend class LogModuleManager;

  explicit LogModule(const char* aName, LogLevel aLevel)
    : mName(strdup(aName)), mLevel(aLevel)
  {
  }

  LogModule(LogModule&) = delete;
  LogModule& operator=(const LogModule&) = delete;

  char* mName;
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
  explicit constexpr LazyLogModule(const char* aLogName)
    : mLogName(aLogName)
    , mLog(nullptr)
  {
  }

  operator LogModule*();

private:
  const char* const mLogName;
  Atomic<LogModule*, ReleaseAcquire> mLog;
};

namespace detail {

inline bool log_test(const LogModule* module, LogLevel level) {
  MOZ_ASSERT(level != LogLevel::Disabled);
  return module && module->ShouldLog(level);
}

void log_print(const LogModule* aModule,
               LogLevel aLevel,
               const char* aFmt, ...) MOZ_FORMAT_PRINTF(3, 4);
} // namespace detail

} // namespace mozilla


// Helper macro used convert MOZ_LOG's third parameter, |_args|, from a
// parenthesized form to a varargs form. For example:
//   ("%s", "a message") => "%s", "a message"
#define MOZ_LOG_EXPAND_ARGS(...) __VA_ARGS__

#if MOZ_LOGGING_ENABLED
#define MOZ_LOG_TEST(_module,_level) mozilla::detail::log_test(_module, _level)
#else
// Define away MOZ_LOG_TEST here so the compiler will fold away entire
// logging blocks via dead code elimination, e.g.:
//
//   if (MOZ_LOG_TEST(...)) {
//     ...compute things to log and log them...
//   }
#define MOZ_LOG_TEST(_module,_level) false
#endif

// The natural definition of the MOZ_LOG macro would expand to:
//
//   do {
//     if (MOZ_LOG_TEST(_module, _level)) {
//       mozilla::detail::log_print(_module, ...);
//     }
//   } while (0)
//
// However, since _module is a LazyLogModule, and we need to call
// LazyLogModule::operator() to get a LogModule* for the MOZ_LOG_TEST
// macro and for the logging call, we'll wind up doing *two* calls, one
// for each, rather than a single call.  The compiler is not able to
// fold the two calls into one, and the extra call can have a
// significant effect on code size.  (Making LazyLogModule::operator() a
// `const` function does not have any effect.)
//
// Therefore, we will have to make a single call ourselves.  But again,
// the natural definition:
//
//   do {
//     ::mozilla::LogModule* real_module = _module;
//     if (MOZ_LOG_TEST(real_module, _level)) {
//       mozilla::detail::log_print(real_module, ...);
//     }
//   } while (0)
//
// also has a problem: if logging is disabled, then we will call
// LazyLogModule::operator() unnecessarily, and the compiler will not be
// able to optimize away the call as dead code.  We would like to avoid
// such a scenario, as the whole point of disabling logging is for the
// logging statements to not generate any code.
//
// Therefore, we need different definitions of MOZ_LOG, depending on
// whether logging is enabled or not.  (We need an actual definition of
// MOZ_LOG even when logging is disabled to ensure the compiler sees that
// variables only used during logging code are actually used, even if the
// code will never be executed.)  Hence, the following code.
#if MOZ_LOGGING_ENABLED
#define MOZ_LOG(_module,_level,_args)                                         \
  do {                                                                        \
    const ::mozilla::LogModule* moz_real_module = _module;                    \
    if (MOZ_LOG_TEST(moz_real_module,_level)) {                               \
      mozilla::detail::log_print(moz_real_module, _level, MOZ_LOG_EXPAND_ARGS _args); \
    }                                                                         \
  } while (0)
#else
#define MOZ_LOG(_module,_level,_args)                                         \
  do {                                                                        \
    if (MOZ_LOG_TEST(_module,_level)) {                        \
      mozilla::detail::log_print(_module, _level, MOZ_LOG_EXPAND_ARGS _args); \
    }                                                                         \
  } while (0)
#endif

// This #define is a Logging.h-only knob!  Don't encourage people to get fancy
// with their log definitions by exporting it outside of Logging.h.
#undef MOZ_LOGGING_ENABLED

#endif // mozilla_logging_h
