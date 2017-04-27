/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests a lot of the profiler_*() functions in GeckoProfiler.h.
// Most of the tests just check that nothing untoward (e.g. crashes, deadlocks)
// happens when calling these functions. They don't do much inspection of
// profiler internals.

#include "gtest/gtest.h"

#include "GeckoProfiler.h"
#include "ProfilerMarkers.h"
#include "jsapi.h"
#include "js/Initialization.h"
#include "mozilla/UniquePtrExtensions.h"
#include "ProfileJSONWriter.h"

#include <string.h>

// Note: profiler_init() has already been called in XRE_main(), so we can't
// test it here. Likewise for profiler_shutdown(), and GeckoProfilerInitRAII
// (which is just an RAII wrapper for profiler_init() and profiler_shutdown()).

using namespace mozilla;

typedef Vector<const char*> StrVec;

void
InactiveFeaturesAndParamsCheck()
{
  int entries;
  double interval;
  StrVec features;
  StrVec filters;

  ASSERT_TRUE(!profiler_is_active());
  ASSERT_TRUE(!profiler_feature_active("gpu"));
  ASSERT_TRUE(!profiler_feature_active("privacy"));
  ASSERT_TRUE(!profiler_feature_active("restyle"));

  profiler_get_start_params(&entries, &interval, &features, &filters);

  ASSERT_TRUE(entries == 0);
  ASSERT_TRUE(interval == 0);
  ASSERT_TRUE(features.empty());
  ASSERT_TRUE(filters.empty());
}

void
ActiveParamsCheck(int aEntries, double aInterval,
                  const char** aFeatures, size_t aFeaturesLen,
                  const char** aFilters, size_t aFiltersLen)
{
  int entries;
  double interval;
  StrVec features;
  StrVec filters;

  profiler_get_start_params(&entries, &interval, &features, &filters);

  ASSERT_TRUE(entries == aEntries);
  ASSERT_TRUE(interval == aInterval);
  ASSERT_TRUE(features.length() == aFeaturesLen);
  for (size_t i = 0; i < aFeaturesLen; i++) {
    ASSERT_TRUE(strcmp(features[i], aFeatures[i]) == 0);
  }
  ASSERT_TRUE(filters.length() == aFiltersLen);
  for (size_t i = 0; i < aFiltersLen; i++) {
    ASSERT_TRUE(strcmp(filters[i], aFilters[i]) == 0);
  }
}

TEST(GeckoProfiler, FeaturesAndParams)
{
  InactiveFeaturesAndParamsCheck();

  // Try a couple of features and filters.
  {
    const char* features[] = { "js", "threads" };
    const char* filters[] = { "GeckoMain", "Compositor" };

    profiler_start(PROFILE_DEFAULT_ENTRIES, PROFILE_DEFAULT_INTERVAL,
                   features, MOZ_ARRAY_LENGTH(features),
                   filters, MOZ_ARRAY_LENGTH(filters));

    ASSERT_TRUE(profiler_is_active());
    ASSERT_TRUE(!profiler_feature_active("gpu"));
    ASSERT_TRUE(!profiler_feature_active("privacy"));
    ASSERT_TRUE(!profiler_feature_active("restyle"));

    ActiveParamsCheck(PROFILE_DEFAULT_ENTRIES, PROFILE_DEFAULT_INTERVAL,
                      features, MOZ_ARRAY_LENGTH(features),
                      filters, MOZ_ARRAY_LENGTH(filters));

    profiler_stop();

    InactiveFeaturesAndParamsCheck();
  }

  // Try some different features and filters.
  {
    const char* features[] = { "gpu", "privacy", "no-such-feature" };
    const char* filters[] = { "GeckoMain", "Foo", "Bar" };

    profiler_start(999999, 3,
                   features, MOZ_ARRAY_LENGTH(features),
                   filters, MOZ_ARRAY_LENGTH(filters));

    // Nb: we did specify "privacy", but profiler_feature_active() doesn't
    // support querying "privacy".
    ASSERT_TRUE(profiler_is_active());
    ASSERT_TRUE(profiler_feature_active("gpu"));
    ASSERT_TRUE(!profiler_feature_active("privacy"));
    ASSERT_TRUE(!profiler_feature_active("restyle"));

    ActiveParamsCheck(999999, 3,
                      features, MOZ_ARRAY_LENGTH(features),
                      filters, MOZ_ARRAY_LENGTH(filters));

    profiler_stop();

    InactiveFeaturesAndParamsCheck();
  }

  // Try all supported features, and filters that match all threads.
  {
    const char** allSupportedFeatures = profiler_get_features();
    int n = 0;
    while (allSupportedFeatures[n]) {
      n++;
    }
    // There are 11 features supported on all platforms, and 2 features
    // supported on some platforms.
    ASSERT_TRUE(11 <= n && n <= 13);

    const char* filters[] = { "" };

    profiler_start(88888, 10,
                   allSupportedFeatures, n,
                   filters, MOZ_ARRAY_LENGTH(filters));

    ASSERT_TRUE(profiler_is_active());
    ASSERT_TRUE(profiler_feature_active("gpu"));
    ASSERT_TRUE(!profiler_feature_active("privacy"));
    ASSERT_TRUE(profiler_feature_active("restyle"));

    ActiveParamsCheck(88888, 10,
                      allSupportedFeatures, n,
                      filters, MOZ_ARRAY_LENGTH(filters));

    // Don't call profiler_stop() here.
  }

  // Try no features, and filters that match no threads.
  {
    const char* features[] = { "" };
    const char* filters[] = { "NoThreadWillMatchThis" };

    // Second profiler_start() call in a row without an intervening
    // profiler_stop(); this will do an implicit profiler_stop() and restart.
    profiler_start(0, 0,
                   features, 0,
                   filters, MOZ_ARRAY_LENGTH(filters));

    ASSERT_TRUE(profiler_is_active());
    ASSERT_TRUE(!profiler_feature_active("gpu"));
    ASSERT_TRUE(!profiler_feature_active("privacy"));
    ASSERT_TRUE(!profiler_feature_active("restyle"));

    // Entries and intervals go to defaults if 0 is specified.
    ActiveParamsCheck(PROFILE_DEFAULT_ENTRIES, PROFILE_DEFAULT_INTERVAL,
                      features, 0,
                      filters, MOZ_ARRAY_LENGTH(filters));

    profiler_stop();

    InactiveFeaturesAndParamsCheck();

    // These calls are no-ops.
    profiler_stop();
    profiler_stop();

    InactiveFeaturesAndParamsCheck();
  }
}

TEST(GeckoProfiler, GetBacktrace)
{
  ASSERT_TRUE(!profiler_get_backtrace());

  {
    const char* features[] = { "stackwalk" };
    const char* filters[] = { "GeckoMain" };

    profiler_start(PROFILE_DEFAULT_ENTRIES, PROFILE_DEFAULT_INTERVAL,
                   features, MOZ_ARRAY_LENGTH(features),
                   filters, MOZ_ARRAY_LENGTH(filters));

    // These will be destroyed while the profiler is active.
    static const int N = 100;
    {
      UniqueProfilerBacktrace u[N];
      for (int i = 0; i < N; i++) {
        u[i] = profiler_get_backtrace();
        ASSERT_TRUE(u[i]);
      }
    }

    // These will be destroyed after the profiler stops.
    UniqueProfilerBacktrace u[N];
    for (int i = 0; i < N; i++) {
      u[i] = profiler_get_backtrace();
      ASSERT_TRUE(u[i]);
    }

    profiler_stop();
  }

  {
    const char* features[] = { "privacy" };
    const char* filters[] = { "GeckoMain" };

    profiler_start(PROFILE_DEFAULT_ENTRIES, PROFILE_DEFAULT_INTERVAL,
                   features, MOZ_ARRAY_LENGTH(features),
                   filters, MOZ_ARRAY_LENGTH(filters));

    // No backtraces obtained when the "privacy" feature is set.
    ASSERT_TRUE(!profiler_get_backtrace());

    profiler_stop();
  }

  ASSERT_TRUE(!profiler_get_backtrace());
}

TEST(GeckoProfiler, Pause)
{
  const char* features[] = { "stackwalk" };
  const char* filters[] = { "GeckoMain" };

  ASSERT_TRUE(!profiler_is_paused());

  profiler_start(PROFILE_DEFAULT_ENTRIES, PROFILE_DEFAULT_INTERVAL,
                 features, MOZ_ARRAY_LENGTH(features),
                 filters, MOZ_ARRAY_LENGTH(filters));

  ASSERT_TRUE(!profiler_is_paused());

  uint32_t currPos1, entries1, generation1;
  uint32_t currPos2, entries2, generation2;

  // Check that we are writing samples while not paused.
  profiler_get_buffer_info(&currPos1, &entries1, &generation1);
  PR_Sleep(PR_MillisecondsToInterval(500));
  profiler_get_buffer_info(&currPos2, &entries2, &generation2);
  ASSERT_TRUE(currPos1 != currPos2);

  profiler_pause();

  ASSERT_TRUE(profiler_is_paused());

  // Check that we are not writing samples while paused.
  profiler_get_buffer_info(&currPos1, &entries1, &generation1);
  PR_Sleep(PR_MillisecondsToInterval(500));
  profiler_get_buffer_info(&currPos2, &entries2, &generation2);
  ASSERT_TRUE(currPos1 == currPos2);

  profiler_resume();

  ASSERT_TRUE(!profiler_is_paused());

  profiler_stop();

  ASSERT_TRUE(!profiler_is_paused());
}

TEST(GeckoProfiler, Markers)
{
  const char* features[] = { "stackwalk" };
  const char* filters[] = { "GeckoMain" };

  profiler_start(PROFILE_DEFAULT_ENTRIES, PROFILE_DEFAULT_INTERVAL,
                 features, MOZ_ARRAY_LENGTH(features),
                 filters, MOZ_ARRAY_LENGTH(filters));

  profiler_tracing("A", "B", TRACING_EVENT);
  profiler_tracing("A", "C", TRACING_INTERVAL_START);
  profiler_tracing("A", "C", TRACING_INTERVAL_END);

  UniqueProfilerBacktrace bt = profiler_get_backtrace();
  profiler_tracing("B", "A", Move(bt), TRACING_EVENT);

  {
    GeckoProfilerTracingRAII tracing("C", "A");

    profiler_log("X");  // Just a specialized form of profiler_tracing().
  }

  profiler_add_marker("M1");
  profiler_add_marker(
    "M2", new ProfilerMarkerTracing("C", TRACING_EVENT));
  PROFILER_MARKER("M3");
  PROFILER_MARKER_PAYLOAD(
    "M4", new ProfilerMarkerTracing("C", TRACING_EVENT,
                                    profiler_get_backtrace()));

  profiler_stop();
}

TEST(GeckoProfiler, Time)
{
  const char* features[] = { "stackwalk" };
  const char* filters[] = { "GeckoMain" };

  double t1 = profiler_time();
  double t2 = profiler_time();
  ASSERT_TRUE(t1 <= t2);

  // profiler_start() restarts the timer used by profiler_time().
  profiler_start(PROFILE_DEFAULT_ENTRIES, PROFILE_DEFAULT_INTERVAL,
                 features, MOZ_ARRAY_LENGTH(features),
                 filters, MOZ_ARRAY_LENGTH(filters));

  double t3 = profiler_time();
  double t4 = profiler_time();
  ASSERT_TRUE(t3 <= t4);

  profiler_stop();

  double t5 = profiler_time();
  double t6 = profiler_time();
  ASSERT_TRUE(t4 <= t5 && t1 <= t6);
}

TEST(GeckoProfiler, GetProfile)
{
  const char* features[] = { "stackwalk" };
  const char* filters[] = { "GeckoMain" };

  ASSERT_TRUE(!profiler_get_profile());

  profiler_start(PROFILE_DEFAULT_ENTRIES, PROFILE_DEFAULT_INTERVAL,
                 features, MOZ_ARRAY_LENGTH(features),
                 filters, MOZ_ARRAY_LENGTH(filters));

  UniquePtr<char[]> profile = profiler_get_profile();
  ASSERT_TRUE(profile && profile[0] == '{');

  profiler_stop();

  ASSERT_TRUE(!profiler_get_profile());
}

TEST(GeckoProfiler, StreamJSONForThisProcess)
{
  const char* features[] = { "stackwalk" };
  const char* filters[] = { "GeckoMain" };

  SpliceableChunkedJSONWriter w;
  ASSERT_TRUE(!profiler_stream_json_for_this_process(w));

  profiler_start(PROFILE_DEFAULT_ENTRIES, PROFILE_DEFAULT_INTERVAL,
                 features, MOZ_ARRAY_LENGTH(features),
                 filters, MOZ_ARRAY_LENGTH(filters));

  w.Start(SpliceableJSONWriter::SingleLineStyle);
  ASSERT_TRUE(profiler_stream_json_for_this_process(w));
  w.End();

  UniquePtr<char[]> profile = w.WriteFunc()->CopyData();
  ASSERT_TRUE(profile && profile[0] == '{');

  profiler_stop();

  ASSERT_TRUE(!profiler_stream_json_for_this_process(w));
}

TEST(GeckoProfiler, PseudoStack)
{
  const char* features[] = { "stackwalk" };
  const char* filters[] = { "GeckoMain" };

  PROFILER_LABEL("A", "B", js::ProfileEntry::Category::OTHER);
  PROFILER_LABEL_FUNC(js::ProfileEntry::Category::OTHER);

  UniqueFreePtr<char> dynamic(strdup("dynamic"));
  {
    PROFILER_LABEL_DYNAMIC("A", "C", js::ProfileEntry::Category::JS,
                           dynamic.get());

    profiler_start(PROFILE_DEFAULT_ENTRIES, PROFILE_DEFAULT_INTERVAL,
                   features, MOZ_ARRAY_LENGTH(features),
                   filters, MOZ_ARRAY_LENGTH(filters));

    ASSERT_TRUE(profiler_get_backtrace());
  }

#if defined(MOZ_GECKO_PROFILER)
  SamplerStackFrameRAII raii1("A", js::ProfileEntry::Category::STORAGE, 888);
  SamplerStackFrameDynamicRAII raii2("A", js::ProfileEntry::Category::STORAGE,
                                     888, dynamic.get());
  void* handle = profiler_call_enter("A", js::ProfileEntry::Category::NETWORK,
                                     this, false, 999);
  ASSERT_TRUE(profiler_get_backtrace());
  profiler_call_exit(handle);

  profiler_call_exit(nullptr);  // a no-op
#endif

  profiler_stop();

  ASSERT_TRUE(!profiler_get_profile());
}

TEST(GeckoProfiler, Bug1355807)
{
  const char* features[] = { "js" };
  const char* manyThreadsFilter[] = { "" };
  const char* fewThreadsFilter[] = { "GeckoMain" };

  profiler_start(PROFILE_DEFAULT_ENTRIES, PROFILE_DEFAULT_INTERVAL,
                 features, MOZ_ARRAY_LENGTH(features),
                 manyThreadsFilter, MOZ_ARRAY_LENGTH(manyThreadsFilter));

  profiler_start(PROFILE_DEFAULT_ENTRIES, PROFILE_DEFAULT_INTERVAL,
                 features, MOZ_ARRAY_LENGTH(features),
                 fewThreadsFilter, MOZ_ARRAY_LENGTH(fewThreadsFilter));

  // In bug 1355807 this caused an assertion failure in stopJSSampling().
  profiler_start(PROFILE_DEFAULT_ENTRIES, PROFILE_DEFAULT_INTERVAL,
                 features, MOZ_ARRAY_LENGTH(features),
                 fewThreadsFilter, MOZ_ARRAY_LENGTH(fewThreadsFilter));

  profiler_stop();
}

