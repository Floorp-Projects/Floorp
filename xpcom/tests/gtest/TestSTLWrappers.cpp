#include <stdio.h>

#include <algorithm>
#ifndef mozilla_algorithm_h
#  error "failed to wrap <algorithm>"
#endif

#include <vector>
#ifndef mozilla_vector_h
#  error "failed to wrap <vector>"
#endif

#ifdef MOZ_CRASHREPORTER
#include "nsCOMPtr.h"
#include "nsICrashReporter.h"
#include "nsServiceManagerUtils.h"
#endif

// gcc errors out if we |try ... catch| with -fno-exceptions, but we
// can still test on windows
#ifdef _MSC_VER
   // C4530 will be generated whenever try...catch is used without
   // enabling exceptions. We know we don't enbale exceptions.
#  pragma warning( disable : 4530 )
#  define TRY       try
#  define CATCH(e)  catch (e)
#else
#  define TRY
#  define CATCH(e)  if (0)
#endif


#if defined(XP_UNIX)
extern unsigned int _gdb_sleep_duration;
#endif

void ShouldAbort()
{
#if defined(XP_UNIX)
    _gdb_sleep_duration = 0;
#endif

#ifdef MOZ_CRASHREPORTER
    nsCOMPtr<nsICrashReporter> crashreporter =
        do_GetService("@mozilla.org/toolkit/crash-reporter;1");
    if (crashreporter) {
      crashreporter->SetEnabled(false);
    }
#endif

    std::vector<int> v;
    int rv = 1;

    TRY {
      // v.at(1) on empty v should abort; NOT throw an exception

      // (Do some arithmetic with result of v.at() to avoid
      // compiler warnings for unused variable/result.)
      rv += v.at(1) ? 1 : 2;
    } CATCH(const std::out_of_range&) {
      fputs("TEST-FAIL | TestSTLWrappers.cpp | caught an exception?\n",
            stderr);
      return;
    }

    fputs("TEST-FAIL | TestSTLWrappers.cpp | didn't abort()?\n",
          stderr);
    return;
}

#ifdef XP_WIN
TEST(STLWrapper, DISABLED_ShouldAbortDeathTest)
#else
TEST(STLWrapper, ShouldAbortDeathTest)
#endif
{
  ASSERT_DEATH_IF_SUPPORTED(ShouldAbort(), "terminate called after throwing an instance of 'std::out_of_range'|vector::_M_range_check");
}
