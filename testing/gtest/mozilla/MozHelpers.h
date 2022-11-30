#ifndef TESTING_GTEST_MOZILLA_HELPERS_H_
#define TESTING_GTEST_MOZILLA_HELPERS_H_

#include "gtest/gtest.h"

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

}  // namespace mozilla::gtest

#endif  // TESTING_GTEST_MOZILLA_HELPERS_H_
