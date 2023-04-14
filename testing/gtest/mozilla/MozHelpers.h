#ifndef TESTING_GTEST_MOZILLA_HELPERS_H_
#define TESTING_GTEST_MOZILLA_HELPERS_H_

#include "gtest/gtest.h"

#include "mozilla/UniquePtr.h"
#include "nsCOMPtr.h"
#include "nsServiceManagerUtils.h"
#include "nsICrashReporter.h"

#if defined(DEBUG) && !defined(XP_WIN) && !defined(ANDROID)
#  define HAS_GDB_SLEEP_DURATION 1
extern unsigned int _gdb_sleep_duration;
#endif

namespace mozilla::gtest {

#if defined(HAS_GDB_SLEEP_DURATION)
#  define ZERO_GDB_SLEEP() _gdb_sleep_duration = 0;

#  define SAVE_GDB_SLEEP(v)  \
    v = _gdb_sleep_duration; \
    ZERO_GDB_SLEEP();
#  define RESTORE_GDB_SLEEP(v) _gdb_sleep_duration = v;

// Some use needs to be in the global namespace
#  define SAVE_GDB_SLEEP_GLOBAL(v) \
    v = ::_gdb_sleep_duration;     \
    ZERO_GDB_SLEEP();
#  define RESTORE_GDB_SLEEP_GLOBAL(v) ::_gdb_sleep_duration = v;

#  define SAVE_GDB_SLEEP_LOCAL()          \
    unsigned int _old_gdb_sleep_duration; \
    SAVE_GDB_SLEEP(_old_gdb_sleep_duration);
#  define RESTORE_GDB_SLEEP_LOCAL() RESTORE_GDB_SLEEP(_old_gdb_sleep_duration);

#else  // defined(HAS_GDB_SLEEP_DURATION)

#  define ZERO_GDB_SLEEP() ;

#  define SAVE_GDB_SLEEP(v)
#  define SAVE_GDB_SLEEP_GLOBAL(v)
#  define SAVE_GDB_SLEEP_LOCAL()
#  define RESTORE_GDB_SLEEP(v)
#  define RESTORE_GDB_SLEEP_GLOBAL(v)
#  define RESTORE_GDB_SLEEP_LOCAL()
#endif  // defined(HAS_GDB_SLEEP_DURATION)

// Death tests are too slow on OSX because of the system crash reporter.
#if !defined(XP_DARWIN)
// Wrap ASSERT_DEATH_IF_SUPPORTED to disable the crash reporter
// when entering the subprocess, so that the expected crashes don't
// create a minidump that the gtest harness will interpret as an error.
#  define ASSERT_DEATH_WRAP(a, b)                 \
    ASSERT_DEATH_IF_SUPPORTED(                    \
        {                                         \
          mozilla::gtest::DisableCrashReporter(); \
          a;                                      \
        },                                        \
        b)
#else
#  define ASSERT_DEATH_WRAP(a, b)
#endif

void DisableCrashReporter();

/**
 * Exit mode used for ScopedTestResultReporter.
 */
enum class ExitMode {
  // The user of the reporter handles exit.
  NoExit,
  // As the reporter goes out of scope, exit with ExitCode().
  ExitOnDtor,
};

/**
 * Status used by ScopedTestResultReporter.
 */
enum class TestResultStatus : int {
  Pass = 0,
  NonFatalFailure = 1,
  FatalFailure = 2,
};

inline int ExitCode(TestResultStatus aStatus) {
  return static_cast<int>(aStatus);
}

/**
 * This is a helper that reports test results to stderr in death test child
 * processes, since that is normally disabled by default (with no way of
 * enabling).
 *
 * Note that for this to work as intended the death test child has to, on
 * failure, exit with an exit code that is unexpected to the death test parent,
 * so the parent can mark the test case as failed.
 *
 * If the parent expects a graceful exit (code 0), ExitCode() can be used with
 * Status() to exit the child process.
 *
 * For simplicity the reporter can exit automatically as it goes out of scope,
 * when created with ExitMode::ExitOnDtor.
 */
class ScopedTestResultReporter {
 public:
  virtual ~ScopedTestResultReporter() = default;

  /**
   * The aggregate status of all observed test results.
   */
  virtual TestResultStatus Status() const = 0;

  static UniquePtr<ScopedTestResultReporter> Create(ExitMode aExitMode);
};

}  // namespace mozilla::gtest

#endif  // TESTING_GTEST_MOZILLA_HELPERS_H_
