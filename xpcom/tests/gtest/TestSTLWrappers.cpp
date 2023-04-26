#include <stdio.h>

#include <algorithm>
#ifndef mozilla_algorithm_h
#  error "failed to wrap <algorithm>"
#endif

#include <vector>
#ifndef mozilla_vector_h
#  error "failed to wrap <vector>"
#endif

// gcc errors out if we |try ... catch| with -fno-exceptions, but we
// can still test on windows
#ifdef _MSC_VER
// C4530 will be generated whenever try...catch is used without
// enabling exceptions. We know we don't enbale exceptions.
#  pragma warning(disable : 4530)
#  define TRY try
#  define CATCH(e) catch (e)
#else
#  define TRY
#  define CATCH(e) if (0)
#endif

#include "gtest/gtest.h"

#include "mozilla/gtest/MozHelpers.h"

void ShouldAbort() {
  ZERO_GDB_SLEEP();

  mozilla::gtest::DisableCrashReporter();

  std::vector<int> v;

  TRY {
    // v.at(1) on empty v should abort; NOT throw an exception

    (void)v.at(1);
  }
  CATCH(const std::out_of_range&) {
    fputs("TEST-FAIL | TestSTLWrappers.cpp | caught an exception?\n", stderr);
    return;
  }

  fputs("TEST-FAIL | TestSTLWrappers.cpp | didn't abort()?\n", stderr);
}

#if defined(XP_WIN) || (defined(XP_MACOSX) && !defined(MOZ_DEBUG))
TEST(STLWrapper, DISABLED_ShouldAbortDeathTest)
#else
TEST(STLWrapper, ShouldAbortDeathTest)
#endif
{
  ASSERT_DEATH_IF_SUPPORTED(ShouldAbort(),
#ifdef __GLIBCXX__
                            // Only libstdc++ will print this message.
                            "terminate called after throwing an instance of "
                            "'std::out_of_range'|vector::_M_range_check"
#else
                            ""
#endif
  );
}
