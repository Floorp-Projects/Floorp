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
#include "ProfilerMarkerPayload.h"
#include "jsapi.h"
#include "js/Initialization.h"
#include "mozilla/UniquePtrExtensions.h"
#include "ProfileJSONWriter.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"

#include <string.h>

// Note: profiler_init() has already been called in XRE_main(), so we can't
// test it here. Likewise for profiler_shutdown(), and AutoProfilerInit
// (which is just an RAII wrapper for profiler_init() and profiler_shutdown()).

using namespace mozilla;

typedef Vector<const char*> StrVec;

static void
InactiveFeaturesAndParamsCheck()
{
  int entries;
  double interval;
  uint32_t features;
  StrVec filters;

  ASSERT_TRUE(!profiler_is_active());
  ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::GPU));
  ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::Privacy));
  ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::Restyle));

  profiler_get_start_params(&entries, &interval, &features, &filters);

  ASSERT_TRUE(entries == 0);
  ASSERT_TRUE(interval == 0);
  ASSERT_TRUE(features == 0);
  ASSERT_TRUE(filters.empty());
}

static void
ActiveParamsCheck(int aEntries, double aInterval, uint32_t aFeatures,
                  const char** aFilters, size_t aFiltersLen)
{
  int entries;
  double interval;
  uint32_t features;
  StrVec filters;

  profiler_get_start_params(&entries, &interval, &features, &filters);

  ASSERT_TRUE(entries == aEntries);
  ASSERT_TRUE(interval == aInterval);
  ASSERT_TRUE(features == aFeatures);
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
    uint32_t features = ProfilerFeature::JS | ProfilerFeature::Threads;
    const char* filters[] = { "GeckoMain", "Compositor" };

    profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                   features, filters, MOZ_ARRAY_LENGTH(filters));

    ASSERT_TRUE(profiler_is_active());
    ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::GPU));
    ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::Privacy));
    ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::Restyle));

    ActiveParamsCheck(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                      features, filters, MOZ_ARRAY_LENGTH(filters));

    profiler_stop();

    InactiveFeaturesAndParamsCheck();
  }

  // Try some different features and filters.
  {
    uint32_t features = ProfilerFeature::GPU | ProfilerFeature::Privacy;
    const char* filters[] = { "GeckoMain", "Foo", "Bar" };

    profiler_start(999999, 3,
                   features, filters, MOZ_ARRAY_LENGTH(filters));

    ASSERT_TRUE(profiler_is_active());
    ASSERT_TRUE(profiler_feature_active(ProfilerFeature::GPU));
    ASSERT_TRUE(profiler_feature_active(ProfilerFeature::Privacy));
    ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::Restyle));

    // Profiler::Threads is added because filters has multiple entries.
    ActiveParamsCheck(999999, 3,
                      features | ProfilerFeature::Threads,
                      filters, MOZ_ARRAY_LENGTH(filters));

    profiler_stop();

    InactiveFeaturesAndParamsCheck();
  }

  // Try all supported features, and filters that match all threads.
  {
    uint32_t availableFeatures = profiler_get_available_features();
    const char* filters[] = { "" };

    profiler_start(88888, 10,
                   availableFeatures, filters, MOZ_ARRAY_LENGTH(filters));

    ASSERT_TRUE(profiler_is_active());
    ASSERT_TRUE(profiler_feature_active(ProfilerFeature::GPU));
    ASSERT_TRUE(profiler_feature_active(ProfilerFeature::Privacy));
    ASSERT_TRUE(profiler_feature_active(ProfilerFeature::Restyle));

    ActiveParamsCheck(88888, 10,
                      availableFeatures, filters, MOZ_ARRAY_LENGTH(filters));

    // Don't call profiler_stop() here.
  }

  // Try no features, and filters that match no threads.
  {
    uint32_t features = 0;
    const char* filters[] = { "NoThreadWillMatchThis" };

    // Second profiler_start() call in a row without an intervening
    // profiler_stop(); this will do an implicit profiler_stop() and restart.
    profiler_start(0, 0,
                   features, filters, MOZ_ARRAY_LENGTH(filters));

    ASSERT_TRUE(profiler_is_active());
    ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::GPU));
    ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::Privacy));
    ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::Restyle));

    // Entries and intervals go to defaults if 0 is specified.
    ActiveParamsCheck(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                      features | ProfilerFeature::Threads,
                      filters, MOZ_ARRAY_LENGTH(filters));

    profiler_stop();

    InactiveFeaturesAndParamsCheck();

    // These calls are no-ops.
    profiler_stop();
    profiler_stop();

    InactiveFeaturesAndParamsCheck();
  }
}

TEST(GeckoProfiler, DifferentThreads)
{
  InactiveFeaturesAndParamsCheck();

  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_NewNamedThread("GeckoProfGTest", getter_AddRefs(thread));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  // Control the profiler on a background thread and verify flags on the
  // main thread.
  {
    uint32_t features = ProfilerFeature::JS | ProfilerFeature::Threads;
    const char* filters[] = { "GeckoMain", "Compositor" };

    thread->Dispatch(NS_NewRunnableFunction([&]() {
      profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                    features, filters, MOZ_ARRAY_LENGTH(filters));
    }), NS_DISPATCH_SYNC);

    ASSERT_TRUE(profiler_is_active());
    ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::GPU));
    ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::Privacy));
    ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::Restyle));

    ActiveParamsCheck(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                      features, filters, MOZ_ARRAY_LENGTH(filters));

    thread->Dispatch(NS_NewRunnableFunction([&]() {
      profiler_stop();
    }), NS_DISPATCH_SYNC);

    InactiveFeaturesAndParamsCheck();
  }

  // Control the profiler on the main thread and verify flags on a
  // background thread.
  {
    uint32_t features = ProfilerFeature::JS | ProfilerFeature::Threads;
    const char* filters[] = { "GeckoMain", "Compositor" };

    profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                  features, filters, MOZ_ARRAY_LENGTH(filters));

    thread->Dispatch(NS_NewRunnableFunction([&]() {
      ASSERT_TRUE(profiler_is_active());
      ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::GPU));
      ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::Privacy));
      ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::Restyle));

      ActiveParamsCheck(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                        features, filters, MOZ_ARRAY_LENGTH(filters));
    }), NS_DISPATCH_SYNC);

    profiler_stop();

    thread->Dispatch(NS_NewRunnableFunction([&]() {
      InactiveFeaturesAndParamsCheck();
    }), NS_DISPATCH_SYNC);
  }

  thread->Shutdown();
}

TEST(GeckoProfiler, GetBacktrace)
{
  ASSERT_TRUE(!profiler_get_backtrace());

  {
    uint32_t features = ProfilerFeature::StackWalk;
    const char* filters[] = { "GeckoMain" };

    profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                   features, filters, MOZ_ARRAY_LENGTH(filters));

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
    uint32_t features = ProfilerFeature::Privacy;
    const char* filters[] = { "GeckoMain" };

    profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                   features, filters, MOZ_ARRAY_LENGTH(filters));

    // No backtraces obtained when ProfilerFeature::Privacy is set.
    ASSERT_TRUE(!profiler_get_backtrace());

    profiler_stop();
  }

  ASSERT_TRUE(!profiler_get_backtrace());
}

TEST(GeckoProfiler, Pause)
{
  uint32_t features = ProfilerFeature::StackWalk;
  const char* filters[] = { "GeckoMain" };

  ASSERT_TRUE(!profiler_is_paused());

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                 features, filters, MOZ_ARRAY_LENGTH(filters));

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

// A class that keeps track of how many instances have been created, streamed,
// and destroyed.
class GTestMarkerPayload : public ProfilerMarkerPayload
{
public:
  explicit GTestMarkerPayload(int aN)
    : mN(aN)
  {
    sNumCreated++;
  }

  virtual ~GTestMarkerPayload() { sNumDestroyed++; }

  virtual void StreamPayload(SpliceableJSONWriter& aWriter,
                             const mozilla::TimeStamp& aStartTime,
                             UniqueStacks& aUniqueStacks) override
  {
    StreamCommonProps("gtest", aWriter, aStartTime, aUniqueStacks);
    char buf[64];
    SprintfLiteral(buf, "gtest-%d", mN);
    aWriter.IntProperty(buf, mN);
    sNumStreamed++;
  }

private:
  int mN;

public:
  // The number of GTestMarkerPayload instances that have been created,
  // streamed, and destroyed.
  static int sNumCreated;
  static int sNumStreamed;
  static int sNumDestroyed;
};

int GTestMarkerPayload::sNumCreated = 0;
int GTestMarkerPayload::sNumStreamed = 0;
int GTestMarkerPayload::sNumDestroyed = 0;

TEST(GeckoProfiler, Markers)
{
  uint32_t features = ProfilerFeature::StackWalk;
  const char* filters[] = { "GeckoMain" };

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                 features, filters, MOZ_ARRAY_LENGTH(filters));

  profiler_tracing("A", "B", TRACING_EVENT);
  profiler_tracing("A", "C", TRACING_INTERVAL_START);
  profiler_tracing("A", "C", TRACING_INTERVAL_END);

  UniqueProfilerBacktrace bt = profiler_get_backtrace();
  profiler_tracing("B", "A", Move(bt), TRACING_EVENT);

  {
    AutoProfilerTracing tracing("C", "A");
  }

  profiler_add_marker("M1");
  profiler_add_marker("M2",
                      MakeUnique<TracingMarkerPayload>("C", TRACING_EVENT));
  PROFILER_MARKER("M3");
  PROFILER_MARKER_PAYLOAD(
    "M4",
    MakeUnique<TracingMarkerPayload>("C", TRACING_EVENT,
                                     profiler_get_backtrace()));

  for (int i = 0; i < 10; i++) {
    PROFILER_MARKER_PAYLOAD("M5", MakeUnique<GTestMarkerPayload>(i));
  }

  // Sleep briefly to ensure a sample is taken and the pending markers are
  // processed.
  PR_Sleep(PR_MillisecondsToInterval(500));

  SpliceableChunkedJSONWriter w;
  ASSERT_TRUE(profiler_stream_json_for_this_process(w));

  UniquePtr<char[]> profile = w.WriteFunc()->CopyData();

  // The GTestMarkerPayloads should have been created and streamed, but not yet
  // destroyed.
  ASSERT_TRUE(GTestMarkerPayload::sNumCreated == 10);
  ASSERT_TRUE(GTestMarkerPayload::sNumStreamed == 10);
  ASSERT_TRUE(GTestMarkerPayload::sNumDestroyed == 0);
  for (int i = 0; i < 10; i++) {
    char buf[64];
    SprintfLiteral(buf, "\"gtest-%d\"", i);
    ASSERT_TRUE(strstr(profile.get(), buf));
  }

  profiler_stop();

  // The GTestMarkerPayloads should have been destroyed.
  ASSERT_TRUE(GTestMarkerPayload::sNumDestroyed == 10);

  for (int i = 0; i < 10; i++) {
    PROFILER_MARKER_PAYLOAD("M5", MakeUnique<GTestMarkerPayload>(i));
  }

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                 features, filters, MOZ_ARRAY_LENGTH(filters));

  ASSERT_TRUE(profiler_stream_json_for_this_process(w));

  profiler_stop();

  // The second set of GTestMarkerPayloads should not have been streamed.
  ASSERT_TRUE(GTestMarkerPayload::sNumCreated == 20);
  ASSERT_TRUE(GTestMarkerPayload::sNumStreamed == 10);
  ASSERT_TRUE(GTestMarkerPayload::sNumDestroyed == 20);
}

TEST(GeckoProfiler, Time)
{
  uint32_t features = ProfilerFeature::StackWalk;
  const char* filters[] = { "GeckoMain" };

  double t1 = profiler_time();
  double t2 = profiler_time();
  ASSERT_TRUE(t1 <= t2);

  // profiler_start() restarts the timer used by profiler_time().
  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                 features, filters, MOZ_ARRAY_LENGTH(filters));

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
  uint32_t features = ProfilerFeature::StackWalk;
  const char* filters[] = { "GeckoMain" };

  ASSERT_TRUE(!profiler_get_profile());

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                 features, filters, MOZ_ARRAY_LENGTH(filters));

  UniquePtr<char[]> profile = profiler_get_profile();
  ASSERT_TRUE(profile && profile[0] == '{');

  profiler_stop();

  ASSERT_TRUE(!profiler_get_profile());
}

static void
JSONOutputCheck(const char* aOutput)
{
  // Check that various expected strings are in the JSON.

  ASSERT_TRUE(aOutput);
  ASSERT_TRUE(aOutput[0] == '{');

  ASSERT_TRUE(strstr(aOutput, "\"libs\""));

  ASSERT_TRUE(strstr(aOutput, "\"meta\""));
  ASSERT_TRUE(strstr(aOutput, "\"version\""));
  ASSERT_TRUE(strstr(aOutput, "\"startTime\""));

  ASSERT_TRUE(strstr(aOutput, "\"threads\""));
  ASSERT_TRUE(strstr(aOutput, "\"GeckoMain\""));
  ASSERT_TRUE(strstr(aOutput, "\"samples\""));
  ASSERT_TRUE(strstr(aOutput, "\"markers\""));
  ASSERT_TRUE(strstr(aOutput, "\"stackTable\""));
  ASSERT_TRUE(strstr(aOutput, "\"frameTable\""));
  ASSERT_TRUE(strstr(aOutput, "\"stringTable\""));
}

TEST(GeckoProfiler, StreamJSONForThisProcess)
{
  uint32_t features = ProfilerFeature::StackWalk;
  const char* filters[] = { "GeckoMain" };

  SpliceableChunkedJSONWriter w;
  ASSERT_TRUE(!profiler_stream_json_for_this_process(w));

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                 features, filters, MOZ_ARRAY_LENGTH(filters));

  w.Start(SpliceableJSONWriter::SingleLineStyle);
  ASSERT_TRUE(profiler_stream_json_for_this_process(w));
  w.End();

  UniquePtr<char[]> profile = w.WriteFunc()->CopyData();

  JSONOutputCheck(profile.get());

  profiler_stop();

  ASSERT_TRUE(!profiler_stream_json_for_this_process(w));
}

TEST(GeckoProfiler, StreamJSONForThisProcessThreaded)
{
  // Same as the previous test, but calling some things on background threads.
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_NewNamedThread("GeckoProfGTest", getter_AddRefs(thread));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  uint32_t features = ProfilerFeature::StackWalk;
  const char* filters[] = { "GeckoMain" };

  SpliceableChunkedJSONWriter w;
  ASSERT_TRUE(!profiler_stream_json_for_this_process(w));

  // Start the profiler on the main thread.
  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                features, filters, MOZ_ARRAY_LENGTH(filters));

  // Call profiler_stream_json_for_this_process on a background thread.
  thread->Dispatch(NS_NewRunnableFunction([&]() {
    w.Start(SpliceableJSONWriter::SingleLineStyle);
    ASSERT_TRUE(profiler_stream_json_for_this_process(w));
    w.End();
  }), NS_DISPATCH_SYNC);

  UniquePtr<char[]> profile = w.WriteFunc()->CopyData();

  JSONOutputCheck(profile.get());

  // Stop the profiler and call profiler_stream_json_for_this_process on a
  // background thread.
  thread->Dispatch(NS_NewRunnableFunction([&]() {
    profiler_stop();
    ASSERT_TRUE(!profiler_stream_json_for_this_process(w));
  }), NS_DISPATCH_SYNC);
  thread->Shutdown();

  // Call profiler_stream_json_for_this_process on the main thread.
  ASSERT_TRUE(!profiler_stream_json_for_this_process(w));
}

TEST(GeckoProfiler, PseudoStack)
{
  uint32_t features = ProfilerFeature::StackWalk;
  const char* filters[] = { "GeckoMain" };

  PROFILER_LABEL("A", "B", js::ProfileEntry::Category::OTHER);
  PROFILER_LABEL_FUNC(js::ProfileEntry::Category::OTHER);

  UniqueFreePtr<char> dynamic(strdup("dynamic"));
  {
    PROFILER_LABEL_DYNAMIC("A", "C", js::ProfileEntry::Category::JS,
                           dynamic.get());

    profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                   features, filters, MOZ_ARRAY_LENGTH(filters));

    ASSERT_TRUE(profiler_get_backtrace());
  }

  AutoProfilerLabel label1("A", nullptr, 888,
                           js::ProfileEntry::Category::STORAGE);
  AutoProfilerLabel label2("A", dynamic.get(), 888,
                           js::ProfileEntry::Category::NETWORK);
  ASSERT_TRUE(profiler_get_backtrace());

  profiler_stop();

  ASSERT_TRUE(!profiler_get_profile());
}

TEST(GeckoProfiler, Bug1355807)
{
  uint32_t features = ProfilerFeature::JS;
  const char* manyThreadsFilter[] = { "" };
  const char* fewThreadsFilter[] = { "GeckoMain" };

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                 features,
                 manyThreadsFilter, MOZ_ARRAY_LENGTH(manyThreadsFilter));

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                 features,
                 fewThreadsFilter, MOZ_ARRAY_LENGTH(fewThreadsFilter));

  // In bug 1355807 this caused an assertion failure in StopJSSampling().
  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                 features,
                 fewThreadsFilter, MOZ_ARRAY_LENGTH(fewThreadsFilter));

  profiler_stop();
}
