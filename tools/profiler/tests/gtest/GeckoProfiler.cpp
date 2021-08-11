/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests a lot of the profiler_*() functions in GeckoProfiler.h.
// Most of the tests just check that nothing untoward (e.g. crashes, deadlocks)
// happens when calling these functions. They don't do much inspection of
// profiler internals.

#include "GeckoProfiler.h"
#include "mozilla/ProfilerMarkerTypes.h"
#include "mozilla/ProfilerMarkers.h"
#include "NetworkMarker.h"
#include "platform.h"
#include "ProfileBuffer.h"

#include "js/Initialization.h"
#include "js/Printf.h"
#include "jsapi.h"
#include "json/json.h"
#include "mozilla/Atomics.h"
#include "mozilla/BlocksRingBuffer.h"
#include "mozilla/DataMutex.h"
#include "mozilla/ProfileBufferEntrySerializationGeckoExtensions.h"
#include "mozilla/ProfileJSONWriter.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "nsIChannelEventSink.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"

#include "gtest/gtest.h"

#include <cstring>
#include <set>
#include <thread>

// Note: profiler_init() has already been called in XRE_main(), so we can't
// test it here. Likewise for profiler_shutdown(), and AutoProfilerInit
// (which is just an RAII wrapper for profiler_init() and profiler_shutdown()).

using namespace mozilla;

TEST(GeckoProfiler, ProfilerUtils)
{
  static_assert(std::is_same_v<decltype(profiler_current_process_id()),
                               ProfilerProcessId>);
#ifdef MOZ_GECKO_PROFILER
  EXPECT_TRUE(profiler_current_process_id().IsSpecified());
#else
  EXPECT_FALSE(profiler_current_process_id().IsSpecified());
#endif

  static_assert(
      std::is_same_v<decltype(profiler_current_thread_id()), ProfilerThreadId>);
#ifdef MOZ_GECKO_PROFILER
  ProfilerThreadId mainTestThreadId = profiler_current_thread_id();
  EXPECT_TRUE(mainTestThreadId.IsSpecified());

  ProfilerThreadId mainThreadId = profiler_main_thread_id();
  if (!mainThreadId.IsSpecified()) {
    // Special case: This may happen if the profiler has not yet been
    // initialized. We only need to set scProfilerMainThreadId.
    mozilla::profiler::detail::scProfilerMainThreadId = mainTestThreadId;
    // After which `profiler_main_thread_id` should work.
    mainThreadId = profiler_main_thread_id();
  }
  EXPECT_TRUE(mainThreadId.IsSpecified());

  EXPECT_EQ(mainThreadId, mainTestThreadId)
      << "Test should run on the main thread";
  EXPECT_TRUE(profiler_is_main_thread());

  std::thread testThread([&]() {
    const ProfilerThreadId testThreadId = profiler_current_thread_id();
    EXPECT_TRUE(testThreadId.IsSpecified());
    EXPECT_NE(testThreadId, mainThreadId);
    EXPECT_FALSE(profiler_is_main_thread());
  });
  testThread.join();
#else
  EXPECT_FALSE(profiler_current_thread_id().IsSpecified());
  EXPECT_FALSE(profiler_main_thread_id().IsSpecified());
  EXPECT_FALSE(profiler_is_main_thread());
#endif
}

TEST(BaseProfiler, BlocksRingBuffer)
{
  constexpr uint32_t MBSize = 256;
  uint8_t buffer[MBSize * 3];
  for (size_t i = 0; i < MBSize * 3; ++i) {
    buffer[i] = uint8_t('A' + i);
  }
  BlocksRingBuffer rb(BlocksRingBuffer::ThreadSafety::WithMutex,
                      &buffer[MBSize], MakePowerOfTwo32<MBSize>());

  {
    nsCString cs("nsCString"_ns);
    nsString s(u"nsString"_ns);
    nsAutoCString acs("nsAutoCString"_ns);
    nsAutoString as(u"nsAutoString"_ns);
    nsAutoCStringN<8> acs8("nsAutoCStringN"_ns);
    nsAutoStringN<8> as8(u"nsAutoStringN"_ns);
    JS::UniqueChars jsuc = JS_smprintf("%s", "JS::UniqueChars");

    rb.PutObjects(cs, s, acs, as, acs8, as8, jsuc);
  }

  rb.ReadEach([](ProfileBufferEntryReader& aER) {
    ASSERT_EQ(aER.ReadObject<nsCString>(), "nsCString"_ns);
    ASSERT_EQ(aER.ReadObject<nsString>(), u"nsString"_ns);
    ASSERT_EQ(aER.ReadObject<nsAutoCString>(), "nsAutoCString"_ns);
    ASSERT_EQ(aER.ReadObject<nsAutoString>(), u"nsAutoString"_ns);
    ASSERT_EQ(aER.ReadObject<nsAutoCStringN<8>>(), "nsAutoCStringN"_ns);
    ASSERT_EQ(aER.ReadObject<nsAutoStringN<8>>(), u"nsAutoStringN"_ns);
    auto jsuc2 = aER.ReadObject<JS::UniqueChars>();
    ASSERT_TRUE(!!jsuc2);
    ASSERT_TRUE(strcmp(jsuc2.get(), "JS::UniqueChars") == 0);
  });

  // Everything around the sub-buffer should be unchanged.
  for (size_t i = 0; i < MBSize; ++i) {
    ASSERT_EQ(buffer[i], uint8_t('A' + i));
  }
  for (size_t i = MBSize * 2; i < MBSize * 3; ++i) {
    ASSERT_EQ(buffer[i], uint8_t('A' + i));
  }
}

// Common JSON checks.

// Does the GETTER return a non-null TYPE? (Non-critical)
#define EXPECT_HAS_JSON(GETTER, TYPE)                                         \
  do {                                                                        \
    if ((GETTER).isNull()) {                                                  \
      EXPECT_FALSE((GETTER).isNull()) << #GETTER " doesn't exist or is null"; \
    } else if (!(GETTER).is##TYPE()) {                                        \
      EXPECT_TRUE((GETTER).is##TYPE())                                        \
          << #GETTER " didn't return type " #TYPE;                            \
    }                                                                         \
  } while (false)

// Does the GETTER return a non-null TYPE? (Critical)
#define ASSERT_HAS_JSON(GETTER, TYPE) \
  do {                                \
    ASSERT_FALSE((GETTER).isNull());  \
    ASSERT_TRUE((GETTER).is##TYPE()); \
  } while (false)

// Does the GETTER return a non-null TYPE? (Critical)
// If yes, store the value into VARIABLE.
#define GET_JSON(VARIABLE, GETTER, TYPE) \
  ASSERT_HAS_JSON(GETTER, TYPE);         \
  const Json::Value& VARIABLE = (GETTER)

// Checks that the GETTER's value is present, is of the expected TYPE, and has
// the expected VALUE. (Non-critical)
#define EXPECT_EQ_JSON(GETTER, TYPE, VALUE)                                   \
  do {                                                                        \
    if ((GETTER).isNull()) {                                                  \
      EXPECT_FALSE((GETTER).isNull()) << #GETTER " doesn't exist or is null"; \
    } else if (!(GETTER).is##TYPE()) {                                        \
      EXPECT_TRUE((GETTER).is##TYPE())                                        \
          << #GETTER " didn't return type " #TYPE;                            \
    } else {                                                                  \
      EXPECT_EQ((GETTER).as##TYPE(), (VALUE));                                \
    }                                                                         \
  } while (false)

// Checks that the GETTER's value is present, and is a valid index into the
// STRINGTABLE array, pointing at the expected STRING.
#define EXPECT_EQ_STRINGTABLE(GETTER, STRINGTABLE, STRING)                    \
  do {                                                                        \
    if ((GETTER).isNull()) {                                                  \
      EXPECT_FALSE((GETTER).isNull()) << #GETTER " doesn't exist or is null"; \
    } else if (!(GETTER).isUInt()) {                                          \
      EXPECT_TRUE((GETTER).isUInt()) << #GETTER " didn't return an index";    \
    } else {                                                                  \
      EXPECT_LT((GETTER).asUInt(), (STRINGTABLE).size());                     \
      EXPECT_EQ_JSON((STRINGTABLE)[(GETTER).asUInt()], String, (STRING));     \
    }                                                                         \
  } while (false)

#define EXPECT_JSON_ARRAY_CONTAINS(GETTER, TYPE, VALUE)                       \
  do {                                                                        \
    if ((GETTER).isNull()) {                                                  \
      EXPECT_FALSE((GETTER).isNull()) << #GETTER " doesn't exist or is null"; \
    } else if (!(GETTER).isArray()) {                                         \
      EXPECT_TRUE((GETTER).is##TYPE()) << #GETTER " is not an array";         \
    } else if (const Json::ArrayIndex size = (GETTER).size(); size == 0u) {   \
      EXPECT_NE(size, 0u) << #GETTER " is an empty array";                    \
    } else {                                                                  \
      bool found = false;                                                     \
      for (Json::ArrayIndex i = 0; i < size; ++i) {                           \
        if (!(GETTER)[i].is##TYPE()) {                                        \
          EXPECT_TRUE((GETTER)[i].is##TYPE())                                 \
              << #GETTER "[" << i << "] is not " #TYPE;                       \
          break;                                                              \
        }                                                                     \
        if ((GETTER)[i].as##TYPE() == (VALUE)) {                              \
          found = true;                                                       \
          break;                                                              \
        }                                                                     \
      }                                                                       \
      EXPECT_TRUE(found) << #GETTER " doesn't contain " #VALUE;               \
    }                                                                         \
  } while (false)

// Check that the given process root contains all the expected properties.
static void JSONRootCheck(const Json::Value& aRoot,
                          bool aWithMainThread = true) {
  ASSERT_TRUE(aRoot.isObject());

  EXPECT_HAS_JSON(aRoot["libs"], Array);

  GET_JSON(meta, aRoot["meta"], Object);
  EXPECT_HAS_JSON(meta["version"], UInt);
  EXPECT_HAS_JSON(meta["startTime"], Double);

  EXPECT_HAS_JSON(aRoot["pages"], Array);

  EXPECT_HAS_JSON(aRoot["profilerOverhead"], Object);

  GET_JSON(threads, aRoot["threads"], Array);
  const Json::ArrayIndex threadCount = threads.size();
  for (Json::ArrayIndex i = 0; i < threadCount; ++i) {
    GET_JSON(thread, threads[i], Object);
    EXPECT_HAS_JSON(thread["processType"], String);
    EXPECT_HAS_JSON(thread["name"], String);
    EXPECT_HAS_JSON(thread["registerTime"], Double);
    EXPECT_HAS_JSON(thread["samples"], Object);
    EXPECT_HAS_JSON(thread["markers"], Object);
    EXPECT_HAS_JSON(thread["pid"], UInt);
    EXPECT_HAS_JSON(thread["tid"], UInt);
    EXPECT_HAS_JSON(thread["stackTable"], Object);
    EXPECT_HAS_JSON(thread["frameTable"], Object);
    EXPECT_HAS_JSON(thread["stringTable"], Array);
  }

  if (aWithMainThread) {
    ASSERT_GT(threadCount, 0u);
    GET_JSON(thread0, threads[0], Object);
    EXPECT_EQ_JSON(thread0["name"], String, "GeckoMain");
  }

  EXPECT_HAS_JSON(aRoot["pausedRanges"], Array);

  const Json::Value& processes = aRoot["processes"];
  if (!processes.isNull()) {
    ASSERT_TRUE(processes.isArray());
    const Json::ArrayIndex processCount = processes.size();
    for (Json::ArrayIndex i = 0; i < processCount; ++i) {
      GET_JSON(process, processes[i], Object);
      JSONRootCheck(process, aWithMainThread);
    }
  }
}

// Check that various expected top properties are in the JSON, and then call the
// provided `aJSONCheckFunction` with the JSON root object.
template <typename JSONCheckFunction>
void JSONOutputCheck(const char* aOutput,
                     JSONCheckFunction&& aJSONCheckFunction) {
  ASSERT_NE(aOutput, nullptr);

  // Extract JSON.
  Json::Value parsedRoot;
  Json::CharReaderBuilder builder;
  const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  ASSERT_TRUE(
      reader->parse(aOutput, strchr(aOutput, '\0'), &parsedRoot, nullptr));

  JSONRootCheck(parsedRoot);

  std::forward<JSONCheckFunction>(aJSONCheckFunction)(parsedRoot);
}

typedef Vector<const char*> StrVec;

static void InactiveFeaturesAndParamsCheck() {
  int entries;
  Maybe<double> duration;
  double interval;
  uint32_t features;
  StrVec filters;
  uint64_t activeTabID;

  ASSERT_TRUE(!profiler_is_active());
  ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::MainThreadIO));
  ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::NativeAllocations));

  profiler_get_start_params(&entries, &duration, &interval, &features, &filters,
                            &activeTabID);

  ASSERT_TRUE(entries == 0);
  ASSERT_TRUE(duration == Nothing());
  ASSERT_TRUE(interval == 0);
  ASSERT_TRUE(features == 0);
  ASSERT_TRUE(filters.empty());
  ASSERT_TRUE(activeTabID == 0);
}

static void ActiveParamsCheck(int aEntries, double aInterval,
                              uint32_t aFeatures, const char** aFilters,
                              size_t aFiltersLen, uint64_t aActiveTabID,
                              const Maybe<double>& aDuration = Nothing()) {
  int entries;
  Maybe<double> duration;
  double interval;
  uint32_t features;
  StrVec filters;
  uint64_t activeTabID;

  profiler_get_start_params(&entries, &duration, &interval, &features, &filters,
                            &activeTabID);

  ASSERT_TRUE(entries == aEntries);
  ASSERT_TRUE(duration == aDuration);
  ASSERT_TRUE(interval == aInterval);
  ASSERT_TRUE(features == aFeatures);
  ASSERT_TRUE(filters.length() == aFiltersLen);
  ASSERT_TRUE(activeTabID == aActiveTabID);
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
    const char* filters[] = {"GeckoMain", "Compositor"};

    profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                   features, filters, MOZ_ARRAY_LENGTH(filters), 100,
                   Some(PROFILER_DEFAULT_DURATION));

    ASSERT_TRUE(profiler_is_active());
    ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::MainThreadIO));
    ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::IPCMessages));

    ActiveParamsCheck(PROFILER_DEFAULT_ENTRIES.Value(),
                      PROFILER_DEFAULT_INTERVAL, features, filters,
                      MOZ_ARRAY_LENGTH(filters), 100,
                      Some(PROFILER_DEFAULT_DURATION));

    profiler_stop();

    InactiveFeaturesAndParamsCheck();
  }

  // Try some different features and filters.
  {
    uint32_t features =
        ProfilerFeature::MainThreadIO | ProfilerFeature::IPCMessages;
    const char* filters[] = {"GeckoMain", "Foo", "Bar"};

    // Testing with some arbitrary buffer size (as could be provided by
    // external code), which we convert to the appropriate power of 2.
    profiler_start(PowerOfTwo32(999999), 3, features, filters,
                   MOZ_ARRAY_LENGTH(filters), 123, Some(25.0));

    ASSERT_TRUE(profiler_is_active());
    ASSERT_TRUE(profiler_feature_active(ProfilerFeature::MainThreadIO));
    ASSERT_TRUE(profiler_feature_active(ProfilerFeature::IPCMessages));

    // Profiler::Threads is added because filters has multiple entries.
    ActiveParamsCheck(PowerOfTwo32(999999).Value(), 3,
                      features | ProfilerFeature::Threads, filters,
                      MOZ_ARRAY_LENGTH(filters), 123, Some(25.0));

    profiler_stop();

    InactiveFeaturesAndParamsCheck();
  }

  // Try with no duration
  {
    uint32_t features =
        ProfilerFeature::MainThreadIO | ProfilerFeature::IPCMessages;
    const char* filters[] = {"GeckoMain", "Foo", "Bar"};

    profiler_start(PowerOfTwo32(999999), 3, features, filters,
                   MOZ_ARRAY_LENGTH(filters), 0, Nothing());

    ASSERT_TRUE(profiler_is_active());
    ASSERT_TRUE(profiler_feature_active(ProfilerFeature::MainThreadIO));
    ASSERT_TRUE(profiler_feature_active(ProfilerFeature::IPCMessages));

    // Profiler::Threads is added because filters has multiple entries.
    ActiveParamsCheck(PowerOfTwo32(999999).Value(), 3,
                      features | ProfilerFeature::Threads, filters,
                      MOZ_ARRAY_LENGTH(filters), 0, Nothing());

    profiler_stop();

    InactiveFeaturesAndParamsCheck();
  }

  // Try all supported features, and filters that match all threads.
  {
    uint32_t availableFeatures = profiler_get_available_features();
    const char* filters[] = {""};

    profiler_start(PowerOfTwo32(88888), 10, availableFeatures, filters,
                   MOZ_ARRAY_LENGTH(filters), 0, Some(15.0));

    ASSERT_TRUE(profiler_is_active());
    ASSERT_TRUE(profiler_feature_active(ProfilerFeature::MainThreadIO));
    ASSERT_TRUE(profiler_feature_active(ProfilerFeature::IPCMessages));

    ActiveParamsCheck(PowerOfTwo32(88888).Value(), 10, availableFeatures,
                      filters, MOZ_ARRAY_LENGTH(filters), 0, Some(15.0));

    // Don't call profiler_stop() here.
  }

  // Try no features, and filters that match no threads.
  {
    uint32_t features = 0;
    const char* filters[] = {"NoThreadWillMatchThis"};

    // Second profiler_start() call in a row without an intervening
    // profiler_stop(); this will do an implicit profiler_stop() and restart.
    profiler_start(PowerOfTwo32(0), 0, features, filters,
                   MOZ_ARRAY_LENGTH(filters), 0, Some(0.0));

    ASSERT_TRUE(profiler_is_active());
    ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::MainThreadIO));
    ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::IPCMessages));

    // Entries and intervals go to defaults if 0 is specified.
    ActiveParamsCheck(PROFILER_DEFAULT_ENTRIES.Value(),
                      PROFILER_DEFAULT_INTERVAL,
                      features | ProfilerFeature::Threads, filters,
                      MOZ_ARRAY_LENGTH(filters), 0, Nothing());

    profiler_stop();

    InactiveFeaturesAndParamsCheck();

    // These calls are no-ops.
    profiler_stop();
    profiler_stop();

    InactiveFeaturesAndParamsCheck();
  }
}

TEST(GeckoProfiler, EnsureStarted)
{
  InactiveFeaturesAndParamsCheck();

  uint32_t features = ProfilerFeature::JS | ProfilerFeature::Threads;
  const char* filters[] = {"GeckoMain", "Compositor"};
  {
    // Inactive -> Active
    profiler_ensure_started(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                            features, filters, MOZ_ARRAY_LENGTH(filters), 0,
                            Some(PROFILER_DEFAULT_DURATION));

    ActiveParamsCheck(
        PROFILER_DEFAULT_ENTRIES.Value(), PROFILER_DEFAULT_INTERVAL, features,
        filters, MOZ_ARRAY_LENGTH(filters), 0, Some(PROFILER_DEFAULT_DURATION));
  }

  {
    // Active -> Active with same settings

    Maybe<ProfilerBufferInfo> info0 = profiler_get_buffer_info();
    ASSERT_TRUE(info0->mRangeEnd > 0);

    // First, write some samples into the buffer.
    PR_Sleep(PR_MillisecondsToInterval(500));

    Maybe<ProfilerBufferInfo> info1 = profiler_get_buffer_info();
    ASSERT_TRUE(info1->mRangeEnd > info0->mRangeEnd);

    // Call profiler_ensure_started with the same settings as before.
    // This operation must not clear our buffer!
    profiler_ensure_started(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                            features, filters, MOZ_ARRAY_LENGTH(filters), 0,
                            Some(PROFILER_DEFAULT_DURATION));

    ActiveParamsCheck(
        PROFILER_DEFAULT_ENTRIES.Value(), PROFILER_DEFAULT_INTERVAL, features,
        filters, MOZ_ARRAY_LENGTH(filters), 0, Some(PROFILER_DEFAULT_DURATION));

    // Check that our position in the buffer stayed the same or advanced, but
    // not by much, and the range-start after profiler_ensure_started shouldn't
    // have passed the range-end before.
    Maybe<ProfilerBufferInfo> info2 = profiler_get_buffer_info();
    ASSERT_TRUE(info2->mRangeEnd >= info1->mRangeEnd);
    ASSERT_TRUE(info2->mRangeEnd - info1->mRangeEnd <
                info1->mRangeEnd - info0->mRangeEnd);
    ASSERT_TRUE(info2->mRangeStart < info1->mRangeEnd);
  }

  {
    // Active -> Active with *different* settings

    Maybe<ProfilerBufferInfo> info1 = profiler_get_buffer_info();

    // Call profiler_ensure_started with a different feature set than the one
    // it's currently running with. This is supposed to stop and restart the
    // profiler, thereby discarding the buffer contents.
    uint32_t differentFeatures = features | ProfilerFeature::Leaf;
    profiler_ensure_started(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                            differentFeatures, filters,
                            MOZ_ARRAY_LENGTH(filters), 0);

    ActiveParamsCheck(PROFILER_DEFAULT_ENTRIES.Value(),
                      PROFILER_DEFAULT_INTERVAL, differentFeatures, filters,
                      MOZ_ARRAY_LENGTH(filters), 0);

    // Check the the buffer was cleared, so its range-start should be at/after
    // its range-end before.
    Maybe<ProfilerBufferInfo> info2 = profiler_get_buffer_info();
    ASSERT_TRUE(info2->mRangeStart >= info1->mRangeEnd);
  }

  {
    // Active -> Inactive

    profiler_stop();

    InactiveFeaturesAndParamsCheck();
  }
}

TEST(GeckoProfiler, MultiRegistration)
{
  // This whole test only checks that function calls don't crash, they don't
  // actually verify that threads get profiled or not.
  char top;
  profiler_register_thread("Main thread again", &top);

  {
    std::thread thread([]() {
      char top;
      profiler_register_thread("thread, no unreg", &top);
    });
    thread.join();
  }

  {
    std::thread thread([]() { profiler_unregister_thread(); });
    thread.join();
  }

  {
    std::thread thread([]() {
      char top;
      profiler_register_thread("thread 1st", &top);
      profiler_unregister_thread();
      profiler_register_thread("thread 2nd", &top);
      profiler_unregister_thread();
    });
    thread.join();
  }

  {
    std::thread thread([]() {
      char top;
      profiler_register_thread("thread once", &top);
      profiler_register_thread("thread again", &top);
      profiler_unregister_thread();
    });
    thread.join();
  }

  {
    std::thread thread([]() {
      char top;
      profiler_register_thread("thread to unreg twice", &top);
      profiler_unregister_thread();
      profiler_unregister_thread();
    });
    thread.join();
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
    const char* filters[] = {"GeckoMain", "Compositor"};

    thread->Dispatch(
        NS_NewRunnableFunction("GeckoProfiler_DifferentThreads_Test::TestBody",
                               [&]() {
                                 profiler_start(PROFILER_DEFAULT_ENTRIES,
                                                PROFILER_DEFAULT_INTERVAL,
                                                features, filters,
                                                MOZ_ARRAY_LENGTH(filters), 0);
                               }),
        NS_DISPATCH_SYNC);

    ASSERT_TRUE(profiler_is_active());
    ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::MainThreadIO));
    ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::IPCMessages));

    ActiveParamsCheck(PROFILER_DEFAULT_ENTRIES.Value(),
                      PROFILER_DEFAULT_INTERVAL, features, filters,
                      MOZ_ARRAY_LENGTH(filters), 0);

    thread->Dispatch(
        NS_NewRunnableFunction("GeckoProfiler_DifferentThreads_Test::TestBody",
                               [&]() { profiler_stop(); }),
        NS_DISPATCH_SYNC);

    InactiveFeaturesAndParamsCheck();
  }

  // Control the profiler on the main thread and verify flags on a
  // background thread.
  {
    uint32_t features = ProfilerFeature::JS | ProfilerFeature::Threads;
    const char* filters[] = {"GeckoMain", "Compositor"};

    profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                   features, filters, MOZ_ARRAY_LENGTH(filters), 0);

    thread->Dispatch(
        NS_NewRunnableFunction(
            "GeckoProfiler_DifferentThreads_Test::TestBody",
            [&]() {
              ASSERT_TRUE(profiler_is_active());
              ASSERT_TRUE(
                  !profiler_feature_active(ProfilerFeature::MainThreadIO));
              ASSERT_TRUE(
                  !profiler_feature_active(ProfilerFeature::IPCMessages));

              ActiveParamsCheck(PROFILER_DEFAULT_ENTRIES.Value(),
                                PROFILER_DEFAULT_INTERVAL, features, filters,
                                MOZ_ARRAY_LENGTH(filters), 0);
            }),
        NS_DISPATCH_SYNC);

    profiler_stop();

    thread->Dispatch(
        NS_NewRunnableFunction("GeckoProfiler_DifferentThreads_Test::TestBody",
                               [&]() { InactiveFeaturesAndParamsCheck(); }),
        NS_DISPATCH_SYNC);
  }

  thread->Shutdown();
}

TEST(GeckoProfiler, GetBacktrace)
{
  ASSERT_TRUE(!profiler_get_backtrace());

  {
    uint32_t features = ProfilerFeature::StackWalk;
    const char* filters[] = {"GeckoMain"};

    profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                   features, filters, MOZ_ARRAY_LENGTH(filters), 0);

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

  ASSERT_TRUE(!profiler_get_backtrace());
}

TEST(GeckoProfiler, Pause)
{
  uint32_t features = ProfilerFeature::StackWalk;
  const char* filters[] = {"GeckoMain"};

  ASSERT_TRUE(!profiler_is_paused());
  ASSERT_TRUE(!profiler_can_accept_markers());

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL, features,
                 filters, MOZ_ARRAY_LENGTH(filters), 0);

  ASSERT_TRUE(!profiler_is_paused());
  ASSERT_TRUE(profiler_can_accept_markers());

  // Check that we are writing samples while not paused.
  Maybe<ProfilerBufferInfo> info1 = profiler_get_buffer_info();
  PR_Sleep(PR_MillisecondsToInterval(500));
  Maybe<ProfilerBufferInfo> info2 = profiler_get_buffer_info();
  ASSERT_TRUE(info1->mRangeEnd != info2->mRangeEnd);

  // Check that we are writing markers while not paused.
  info1 = profiler_get_buffer_info();
  PROFILER_MARKER_UNTYPED("Not paused", OTHER, {});
  info2 = profiler_get_buffer_info();
  ASSERT_TRUE(info1->mRangeEnd != info2->mRangeEnd);

  profiler_pause();

  ASSERT_TRUE(profiler_is_paused());
  ASSERT_TRUE(!profiler_can_accept_markers());

  // Check that we are not writing samples while paused.
  info1 = profiler_get_buffer_info();
  PR_Sleep(PR_MillisecondsToInterval(500));
  info2 = profiler_get_buffer_info();
  ASSERT_TRUE(info1->mRangeEnd == info2->mRangeEnd);

  // Check that we are now writing markers while paused.
  info1 = profiler_get_buffer_info();
  PROFILER_MARKER_UNTYPED("Paused", OTHER, {});
  info2 = profiler_get_buffer_info();
  ASSERT_TRUE(info1->mRangeEnd == info2->mRangeEnd);
  PROFILER_MARKER_UNTYPED("Paused v2", OTHER, {});
  Maybe<ProfilerBufferInfo> info3 = profiler_get_buffer_info();
  ASSERT_TRUE(info2->mRangeEnd == info3->mRangeEnd);

  profiler_resume();

  ASSERT_TRUE(!profiler_is_paused());
  ASSERT_TRUE(profiler_can_accept_markers());

  profiler_stop();

  ASSERT_TRUE(!profiler_is_paused());
  ASSERT_TRUE(!profiler_can_accept_markers());
}

TEST(GeckoProfiler, Markers)
{
  uint32_t features = ProfilerFeature::StackWalk;
  const char* filters[] = {"GeckoMain"};

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL, features,
                 filters, MOZ_ARRAY_LENGTH(filters), 0);

  PROFILER_MARKER("tracing event", OTHER, {}, Tracing, "A");
  PROFILER_MARKER("tracing start", OTHER, MarkerTiming::IntervalStart(),
                  Tracing, "A");
  PROFILER_MARKER("tracing end", OTHER, MarkerTiming::IntervalEnd(), Tracing,
                  "A");

  auto bt = profiler_capture_backtrace();
  PROFILER_MARKER("tracing event with stack", OTHER,
                  MarkerStack::TakeBacktrace(std::move(bt)), Tracing, "B");

  { AUTO_PROFILER_TRACING_MARKER("C", "auto tracing", OTHER); }

  PROFILER_MARKER_UNTYPED("M1", OTHER, {});
  PROFILER_MARKER_UNTYPED("M3", OTHER, {});

  // Create three strings: two that are the maximum allowed length, and one that
  // is one char longer.
  static const size_t kMax = ProfileBuffer::kMaxFrameKeyLength;
  UniquePtr<char[]> okstr1 = MakeUnique<char[]>(kMax);
  UniquePtr<char[]> okstr2 = MakeUnique<char[]>(kMax);
  UniquePtr<char[]> longstr = MakeUnique<char[]>(kMax + 1);
  UniquePtr<char[]> longstrCut = MakeUnique<char[]>(kMax + 1);
  for (size_t i = 0; i < kMax; i++) {
    okstr1[i] = 'a';
    okstr2[i] = 'b';
    longstr[i] = 'c';
    longstrCut[i] = 'c';
  }
  okstr1[kMax - 1] = '\0';
  okstr2[kMax - 1] = '\0';
  longstr[kMax] = '\0';
  longstrCut[kMax] = '\0';
  // Should be output as-is.
  AUTO_PROFILER_LABEL_DYNAMIC_CSTR("", LAYOUT, "");
  AUTO_PROFILER_LABEL_DYNAMIC_CSTR("", LAYOUT, okstr1.get());
  // Should be output as label + space + okstr2.
  AUTO_PROFILER_LABEL_DYNAMIC_CSTR("okstr2", LAYOUT, okstr2.get());
  // Should be output with kMax length, ending with "...\0".
  AUTO_PROFILER_LABEL_DYNAMIC_CSTR("", LAYOUT, longstr.get());
  ASSERT_EQ(longstrCut[kMax - 4], 'c');
  longstrCut[kMax - 4] = '.';
  ASSERT_EQ(longstrCut[kMax - 3], 'c');
  longstrCut[kMax - 3] = '.';
  ASSERT_EQ(longstrCut[kMax - 2], 'c');
  longstrCut[kMax - 2] = '.';
  ASSERT_EQ(longstrCut[kMax - 1], 'c');
  longstrCut[kMax - 1] = '\0';

  // Test basic markers 2.0.
  EXPECT_TRUE(
      profiler_add_marker("default-templated markers 2.0 with empty options",
                          geckoprofiler::category::OTHER, {}));

  PROFILER_MARKER_UNTYPED(
      "default-templated markers 2.0 with option", OTHER,
      MarkerStack::TakeBacktrace(profiler_capture_backtrace()));

  PROFILER_MARKER("explicitly-default-templated markers 2.0 with empty options",
                  OTHER, {}, NoPayload);

  EXPECT_TRUE(profiler_add_marker(
      "explicitly-default-templated markers 2.0 with option",
      geckoprofiler::category::OTHER, {},
      ::geckoprofiler::markers::NoPayload{}));

  // Used in markers below.
  TimeStamp ts1 = TimeStamp::Now();

  // Sleep briefly to ensure a sample is taken and the pending markers are
  // processed.
  PR_Sleep(PR_MillisecondsToInterval(500));

  // Used in markers below.
  TimeStamp ts2 = TimeStamp::Now();
  // ts1 and ts2 should be different thanks to the sleep.
  EXPECT_NE(ts1, ts2);

  // Test most marker payloads.

  // Keep this one first! (It's used to record `ts1` and `ts2`, to compare
  // to serialized numbers in other markers.)
  EXPECT_TRUE(profiler_add_marker("FirstMarker", geckoprofiler::category::OTHER,
                                  MarkerTiming::Interval(ts1, ts2),
                                  geckoprofiler::markers::TextMarker{},
                                  "First Marker"));

  // User-defined marker type with different properties, and fake schema.
  struct GtestMarker {
    static constexpr Span<const char> MarkerTypeName() {
      return MakeStringSpan("markers-gtest");
    }
    static void StreamJSONMarkerData(
        mozilla::baseprofiler::SpliceableJSONWriter& aWriter, int aInt,
        double aDouble, const mozilla::ProfilerString8View& aText,
        const mozilla::ProfilerString8View& aUniqueText,
        const mozilla::TimeStamp& aTime) {
      aWriter.NullProperty("null");
      aWriter.BoolProperty("bool-false", false);
      aWriter.BoolProperty("bool-true", true);
      aWriter.IntProperty("int", aInt);
      aWriter.DoubleProperty("double", aDouble);
      aWriter.StringProperty("text", aText);
      aWriter.UniqueStringProperty("unique text", aUniqueText);
      aWriter.UniqueStringProperty("unique text again", aUniqueText);
      aWriter.TimeProperty("time", aTime);
    }
    static mozilla::MarkerSchema MarkerTypeDisplay() {
      // Note: This is an test function that is not intended to actually output
      // that correctly matches StreamJSONMarkerData data above! Instead we only
      // test that it outputs the expected JSON at the end.
      using MS = mozilla::MarkerSchema;
      MS schema{MS::Location::markerChart,      MS::Location::markerTable,
                MS::Location::timelineOverview, MS::Location::timelineMemory,
                MS::Location::timelineIPC,      MS::Location::timelineFileIO,
                MS::Location::stackChart};
      // All label functions.
      schema.SetChartLabel("chart label");
      schema.SetTooltipLabel("tooltip label");
      schema.SetTableLabel("table label");
      // All data functions, all formats, all "searchable" values.
      schema.AddKeyFormat("key with url", MS::Format::url);
      schema.AddKeyLabelFormat("key with label filePath", "label filePath",
                               MS::Format::filePath);
      schema.AddKeyFormatSearchable("key with string not-searchable",
                                    MS::Format::string,
                                    MS::Searchable::notSearchable);
      schema.AddKeyLabelFormatSearchable("key with label duration searchable",
                                         "label duration", MS::Format::duration,
                                         MS::Searchable::searchable);
      schema.AddKeyFormat("key with time", MS::Format::time);
      schema.AddKeyFormat("key with seconds", MS::Format::seconds);
      schema.AddKeyFormat("key with milliseconds", MS::Format::milliseconds);
      schema.AddKeyFormat("key with microseconds", MS::Format::microseconds);
      schema.AddKeyFormat("key with nanoseconds", MS::Format::nanoseconds);
      schema.AddKeyFormat("key with bytes", MS::Format::bytes);
      schema.AddKeyFormat("key with percentage", MS::Format::percentage);
      schema.AddKeyFormat("key with integer", MS::Format::integer);
      schema.AddKeyFormat("key with decimal", MS::Format::decimal);
      schema.AddStaticLabelValue("static label", "static value");
      return schema;
    }
  };
  EXPECT_TRUE(
      profiler_add_marker("Gtest custom marker", geckoprofiler::category::OTHER,
                          MarkerTiming::Interval(ts1, ts2), GtestMarker{}, 42,
                          43.0, "gtest text", "gtest unique text", ts1));

  // User-defined marker type with no data, special frontend schema.
  struct GtestSpecialMarker {
    static constexpr Span<const char> MarkerTypeName() {
      return MakeStringSpan("markers-gtest-special");
    }
    static void StreamJSONMarkerData(
        mozilla::baseprofiler::SpliceableJSONWriter& aWriter) {}
    static mozilla::MarkerSchema MarkerTypeDisplay() {
      return mozilla::MarkerSchema::SpecialFrontendLocation{};
    }
  };
  EXPECT_TRUE(profiler_add_marker("Gtest special marker",
                                  geckoprofiler::category::OTHER, {},
                                  GtestSpecialMarker{}));

  // User-defined marker type that is never used, so it shouldn't appear in the
  // output.
  struct GtestUnusedMarker {
    static constexpr Span<const char> MarkerTypeName() {
      return MakeStringSpan("markers-gtest-unused");
    }
    static void StreamJSONMarkerData(
        mozilla::baseprofiler::SpliceableJSONWriter& aWriter) {}
    static mozilla::MarkerSchema MarkerTypeDisplay() {
      return mozilla::MarkerSchema::SpecialFrontendLocation{};
    }
  };

  // Make sure the compiler doesn't complain about this unused struct.
  mozilla::Unused << GtestUnusedMarker{};

  // Other markers in alphabetical order of payload class names.

  nsCOMPtr<nsIURI> uri;
  ASSERT_TRUE(
      NS_SUCCEEDED(NS_NewURI(getter_AddRefs(uri), "http://mozilla.org/"_ns)));
  // The marker name will be "Load <aChannelId>: <aURI>".
  profiler_add_network_marker(
      /* nsIURI* aURI */ uri,
      /* const nsACString& aRequestMethod */ "GET"_ns,
      /* int32_t aPriority */ 34,
      /* uint64_t aChannelId */ 1,
      /* NetworkLoadType aType */ net::NetworkLoadType::LOAD_START,
      /* mozilla::TimeStamp aStart */ ts1,
      /* mozilla::TimeStamp aEnd */ ts2,
      /* int64_t aCount */ 56,
      /* mozilla::net::CacheDisposition aCacheDisposition */
      net::kCacheHit,
      /* uint64_t aInnerWindowID */ 78
      /* const mozilla::net::TimingStruct* aTimings = nullptr */
      /* mozilla::UniquePtr<mozilla::ProfileChunkedBuffer> aSource =
         nullptr */
      /* const mozilla::Maybe<nsDependentCString>& aContentType =
         mozilla::Nothing() */
      /* nsIURI* aRedirectURI = nullptr */
      /* uint64_t aRedirectChannelId = 0 */
  );

  profiler_add_network_marker(
      /* nsIURI* aURI */ uri,
      /* const nsACString& aRequestMethod */ "GET"_ns,
      /* int32_t aPriority */ 34,
      /* uint64_t aChannelId */ 2,
      /* NetworkLoadType aType */ net::NetworkLoadType::LOAD_STOP,
      /* mozilla::TimeStamp aStart */ ts1,
      /* mozilla::TimeStamp aEnd */ ts2,
      /* int64_t aCount */ 56,
      /* mozilla::net::CacheDisposition aCacheDisposition */
      net::kCacheUnresolved,
      /* uint64_t aInnerWindowID */ 78,
      /* const mozilla::net::TimingStruct* aTimings = nullptr */ nullptr,
      /* mozilla::UniquePtr<mozilla::ProfileChunkedBuffer> aSource =
         nullptr */
      nullptr,
      /* const mozilla::Maybe<nsDependentCString>& aContentType =
         mozilla::Nothing() */
      Some(nsDependentCString("text/html")),
      /* nsIURI* aRedirectURI = nullptr */ nullptr,
      /* uint64_t aRedirectChannelId = 0 */ 0);

  nsCOMPtr<nsIURI> redirectURI;
  ASSERT_TRUE(NS_SUCCEEDED(
      NS_NewURI(getter_AddRefs(redirectURI), "http://example.com/"_ns)));
  profiler_add_network_marker(
      /* nsIURI* aURI */ uri,
      /* const nsACString& aRequestMethod */ "GET"_ns,
      /* int32_t aPriority */ 34,
      /* uint64_t aChannelId */ 3,
      /* NetworkLoadType aType */ net::NetworkLoadType::LOAD_REDIRECT,
      /* mozilla::TimeStamp aStart */ ts1,
      /* mozilla::TimeStamp aEnd */ ts2,
      /* int64_t aCount */ 56,
      /* mozilla::net::CacheDisposition aCacheDisposition */
      net::kCacheUnresolved,
      /* uint64_t aInnerWindowID */ 78,
      /* const mozilla::net::TimingStruct* aTimings = nullptr */ nullptr,
      /* mozilla::UniquePtr<mozilla::ProfileChunkedBuffer> aSource =
         nullptr */
      nullptr,
      /* const mozilla::Maybe<nsDependentCString>& aContentType =
         mozilla::Nothing() */
      mozilla::Nothing(),
      /* nsIURI* aRedirectURI = nullptr */ redirectURI,
      /* uint32_t aRedirectFlags = 0 */
      nsIChannelEventSink::REDIRECT_TEMPORARY,
      /* uint64_t aRedirectChannelId = 0 */ 103);

  profiler_add_network_marker(
      /* nsIURI* aURI */ uri,
      /* const nsACString& aRequestMethod */ "GET"_ns,
      /* int32_t aPriority */ 34,
      /* uint64_t aChannelId */ 4,
      /* NetworkLoadType aType */ net::NetworkLoadType::LOAD_REDIRECT,
      /* mozilla::TimeStamp aStart */ ts1,
      /* mozilla::TimeStamp aEnd */ ts2,
      /* int64_t aCount */ 56,
      /* mozilla::net::CacheDisposition aCacheDisposition */
      net::kCacheUnresolved,
      /* uint64_t aInnerWindowID */ 78,
      /* const mozilla::net::TimingStruct* aTimings = nullptr */ nullptr,
      /* mozilla::UniquePtr<mozilla::ProfileChunkedBuffer> aSource =
         nullptr */
      nullptr,
      /* const mozilla::Maybe<nsDependentCString>& aContentType =
         mozilla::Nothing() */
      mozilla::Nothing(),
      /* nsIURI* aRedirectURI = nullptr */ redirectURI,
      /* uint32_t aRedirectFlags = 0 */
      nsIChannelEventSink::REDIRECT_PERMANENT,
      /* uint64_t aRedirectChannelId = 0 */ 104);

  profiler_add_network_marker(
      /* nsIURI* aURI */ uri,
      /* const nsACString& aRequestMethod */ "GET"_ns,
      /* int32_t aPriority */ 34,
      /* uint64_t aChannelId */ 5,
      /* NetworkLoadType aType */ net::NetworkLoadType::LOAD_REDIRECT,
      /* mozilla::TimeStamp aStart */ ts1,
      /* mozilla::TimeStamp aEnd */ ts2,
      /* int64_t aCount */ 56,
      /* mozilla::net::CacheDisposition aCacheDisposition */
      net::kCacheUnresolved,
      /* uint64_t aInnerWindowID */ 78,
      /* const mozilla::net::TimingStruct* aTimings = nullptr */ nullptr,
      /* mozilla::UniquePtr<mozilla::ProfileChunkedBuffer> aSource =
         nullptr */
      nullptr,
      /* const mozilla::Maybe<nsDependentCString>& aContentType =
         mozilla::Nothing() */
      mozilla::Nothing(),
      /* nsIURI* aRedirectURI = nullptr */ redirectURI,
      /* uint32_t aRedirectFlags = 0 */ nsIChannelEventSink::REDIRECT_INTERNAL,
      /* uint64_t aRedirectChannelId = 0 */ 105);

  profiler_add_network_marker(
      /* nsIURI* aURI */ uri,
      /* const nsACString& aRequestMethod */ "GET"_ns,
      /* int32_t aPriority */ 34,
      /* uint64_t aChannelId */ 6,
      /* NetworkLoadType aType */ net::NetworkLoadType::LOAD_REDIRECT,
      /* mozilla::TimeStamp aStart */ ts1,
      /* mozilla::TimeStamp aEnd */ ts2,
      /* int64_t aCount */ 56,
      /* mozilla::net::CacheDisposition aCacheDisposition */
      net::kCacheUnresolved,
      /* uint64_t aInnerWindowID */ 78,
      /* const mozilla::net::TimingStruct* aTimings = nullptr */ nullptr,
      /* mozilla::UniquePtr<mozilla::ProfileChunkedBuffer> aSource =
         nullptr */
      nullptr,
      /* const mozilla::Maybe<nsDependentCString>& aContentType =
         mozilla::Nothing() */
      mozilla::Nothing(),
      /* nsIURI* aRedirectURI = nullptr */ redirectURI,
      /* uint32_t aRedirectFlags = 0 */ nsIChannelEventSink::REDIRECT_INTERNAL |
          nsIChannelEventSink::REDIRECT_STS_UPGRADE,
      /* uint64_t aRedirectChannelId = 0 */ 106);

  EXPECT_TRUE(profiler_add_marker(
      "Text in main thread with stack", geckoprofiler::category::OTHER,
      {MarkerStack::Capture(), MarkerTiming::Interval(ts1, ts2)},
      geckoprofiler::markers::TextMarker{}, ""));
  EXPECT_TRUE(profiler_add_marker(
      "Text from main thread with stack", geckoprofiler::category::OTHER,
      MarkerOptions(MarkerThreadId::MainThread(), MarkerStack::Capture()),
      geckoprofiler::markers::TextMarker{}, ""));

  std::thread registeredThread([]() {
    AUTO_PROFILER_REGISTER_THREAD("Marker test sub-thread");
    // Marker in non-profiled thread won't be stored.
    EXPECT_TRUE(profiler_add_marker(
        "Text in registered thread with stack", geckoprofiler::category::OTHER,
        MarkerStack::Capture(), geckoprofiler::markers::TextMarker{}, ""));
    // Marker will be stored in main thread, with stack from registered thread.
    EXPECT_TRUE(profiler_add_marker(
        "Text from registered thread with stack",
        geckoprofiler::category::OTHER,
        MarkerOptions(MarkerThreadId::MainThread(), MarkerStack::Capture()),
        geckoprofiler::markers::TextMarker{}, ""));
  });
  registeredThread.join();

  std::thread unregisteredThread([]() {
    // Marker in unregistered thread won't be stored.
    EXPECT_TRUE(profiler_add_marker("Text in unregistered thread with stack",
                                    geckoprofiler::category::OTHER,
                                    MarkerStack::Capture(),
                                    geckoprofiler::markers::TextMarker{}, ""));
    // Marker will be stored in main thread, but stack cannot be captured in an
    // unregistered thread.
    EXPECT_TRUE(profiler_add_marker(
        "Text from unregistered thread with stack",
        geckoprofiler::category::OTHER,
        MarkerOptions(MarkerThreadId::MainThread(), MarkerStack::Capture()),
        geckoprofiler::markers::TextMarker{}, ""));
  });
  unregisteredThread.join();

  EXPECT_TRUE(profiler_add_marker("Tracing", geckoprofiler::category::OTHER, {},
                                  geckoprofiler::markers::Tracing{},
                                  "category"));

  EXPECT_TRUE(profiler_add_marker("Text", geckoprofiler::category::OTHER, {},
                                  geckoprofiler::markers::TextMarker{},
                                  "Text text"));

  EXPECT_TRUE(profiler_add_marker(
      "MediaSample", geckoprofiler::category::OTHER, {},
      geckoprofiler::markers::MediaSampleMarker{}, 123, 456));

  SpliceableChunkedJSONWriter w;
  w.Start();
  EXPECT_TRUE(::profiler_stream_json_for_this_process(w));
  w.End();

  UniquePtr<char[]> profile = w.ChunkedWriteFunc().CopyData();
  ASSERT_TRUE(!!profile.get());

  // Expected markers, in order.
  enum State {
    S_tracing_event,
    S_tracing_start,
    S_tracing_end,
    S_tracing_event_with_stack,
    S_tracing_auto_tracing_start,
    S_tracing_auto_tracing_end,
    S_M1,
    S_M3,
    S_Markers2DefaultEmptyOptions,
    S_Markers2DefaultWithOptions,
    S_Markers2ExplicitDefaultEmptyOptions,
    S_Markers2ExplicitDefaultWithOptions,
    S_FirstMarker,
    S_CustomMarker,
    S_SpecialMarker,
    S_NetworkMarkerPayload_start,
    S_NetworkMarkerPayload_stop,
    S_NetworkMarkerPayload_redirect_temporary,
    S_NetworkMarkerPayload_redirect_permanent,
    S_NetworkMarkerPayload_redirect_internal,
    S_NetworkMarkerPayload_redirect_internal_sts,

    S_TextWithStack,
    S_TextToMTWithStack,
    S_RegThread_TextToMTWithStack,
    S_UnregThread_TextToMTWithStack,

    S_LAST,
  } state = State(0);

  // These will be set when first read from S_FirstMarker, then
  // compared in following markers.
  // TODO: Compute these values from the timestamps.
  double ts1Double = 0.0;
  double ts2Double = 0.0;

  JSONOutputCheck(profile.get(), [&](const Json::Value& root) {
    {
      GET_JSON(threads, root["threads"], Array);
      ASSERT_EQ(threads.size(), 1u);

      {
        GET_JSON(thread0, threads[0], Object);

        // Keep a reference to the string table in this block, it will be used
        // below.
        GET_JSON(stringTable, thread0["stringTable"], Array);
        ASSERT_TRUE(stringTable.isArray());

        // Test the expected labels in the string table.
        bool foundEmpty = false;
        bool foundOkstr1 = false;
        bool foundOkstr2 = false;
        const std::string okstr2Label = std::string("okstr2 ") + okstr2.get();
        bool foundTooLong = false;
        for (const auto& s : stringTable) {
          ASSERT_TRUE(s.isString());
          std::string sString = s.asString();
          if (sString.empty()) {
            EXPECT_FALSE(foundEmpty);
            foundEmpty = true;
          } else if (sString == okstr1.get()) {
            EXPECT_FALSE(foundOkstr1);
            foundOkstr1 = true;
          } else if (sString == okstr2Label) {
            EXPECT_FALSE(foundOkstr2);
            foundOkstr2 = true;
          } else if (sString == longstrCut.get()) {
            EXPECT_FALSE(foundTooLong);
            foundTooLong = true;
          } else {
            EXPECT_NE(sString, longstr.get());
          }
        }
        EXPECT_TRUE(foundEmpty);
        EXPECT_TRUE(foundOkstr1);
        EXPECT_TRUE(foundOkstr2);
        EXPECT_TRUE(foundTooLong);

        {
          GET_JSON(markers, thread0["markers"], Object);

          {
            GET_JSON(data, markers["data"], Array);

            for (const Json::Value& marker : data) {
              // Name the indexes into the marker tuple:
              // [name, startTime, endTime, phase, category, payload]
              const unsigned int NAME = 0u;
              const unsigned int START_TIME = 1u;
              const unsigned int END_TIME = 2u;
              const unsigned int PHASE = 3u;
              const unsigned int CATEGORY = 4u;
              const unsigned int PAYLOAD = 5u;

              const unsigned int PHASE_INSTANT = 0;
              const unsigned int PHASE_INTERVAL = 1;
              const unsigned int PHASE_START = 2;
              const unsigned int PHASE_END = 3;

              const unsigned int SIZE_WITHOUT_PAYLOAD = 5u;
              const unsigned int SIZE_WITH_PAYLOAD = 6u;

              ASSERT_TRUE(marker.isArray());
              // The payload is optional.
              ASSERT_GE(marker.size(), SIZE_WITHOUT_PAYLOAD);
              ASSERT_LE(marker.size(), SIZE_WITH_PAYLOAD);

              // root.threads[0].markers.data[i] is an array with 5 or 6
              // elements.

              ASSERT_TRUE(marker[NAME].isUInt());  // name id
              GET_JSON(name, stringTable[marker[NAME].asUInt()], String);
              std::string nameString = name.asString();

              EXPECT_TRUE(marker[START_TIME].isNumeric());
              EXPECT_TRUE(marker[END_TIME].isNumeric());
              EXPECT_TRUE(marker[PHASE].isUInt());
              EXPECT_TRUE(marker[PHASE].asUInt() < 4);
              EXPECT_TRUE(marker[CATEGORY].isUInt());

#define EXPECT_TIMING_INSTANT                  \
  EXPECT_NE(marker[START_TIME].asDouble(), 0); \
  EXPECT_EQ(marker[END_TIME].asDouble(), 0);   \
  EXPECT_EQ(marker[PHASE].asUInt(), PHASE_INSTANT);
#define EXPECT_TIMING_INTERVAL                 \
  EXPECT_NE(marker[START_TIME].asDouble(), 0); \
  EXPECT_NE(marker[END_TIME].asDouble(), 0);   \
  EXPECT_EQ(marker[PHASE].asUInt(), PHASE_INTERVAL);
#define EXPECT_TIMING_START                    \
  EXPECT_NE(marker[START_TIME].asDouble(), 0); \
  EXPECT_EQ(marker[END_TIME].asDouble(), 0);   \
  EXPECT_EQ(marker[PHASE].asUInt(), PHASE_START);
#define EXPECT_TIMING_END                      \
  EXPECT_EQ(marker[START_TIME].asDouble(), 0); \
  EXPECT_NE(marker[END_TIME].asDouble(), 0);   \
  EXPECT_EQ(marker[PHASE].asUInt(), PHASE_END);

#define EXPECT_TIMING_INSTANT_AT(t)            \
  EXPECT_EQ(marker[START_TIME].asDouble(), t); \
  EXPECT_EQ(marker[END_TIME].asDouble(), 0);   \
  EXPECT_EQ(marker[PHASE].asUInt(), PHASE_INSTANT);
#define EXPECT_TIMING_INTERVAL_AT(start, end)      \
  EXPECT_EQ(marker[START_TIME].asDouble(), start); \
  EXPECT_EQ(marker[END_TIME].asDouble(), end);     \
  EXPECT_EQ(marker[PHASE].asUInt(), PHASE_INTERVAL);
#define EXPECT_TIMING_START_AT(start)              \
  EXPECT_EQ(marker[START_TIME].asDouble(), start); \
  EXPECT_EQ(marker[END_TIME].asDouble(), 0);       \
  EXPECT_EQ(marker[PHASE].asUInt(), PHASE_START);
#define EXPECT_TIMING_END_AT(end)              \
  EXPECT_EQ(marker[START_TIME].asDouble(), 0); \
  EXPECT_EQ(marker[END_TIME].asDouble(), end); \
  EXPECT_EQ(marker[PHASE].asUInt(), PHASE_END);

              if (marker.size() == SIZE_WITHOUT_PAYLOAD) {
                // root.threads[0].markers.data[i] is an array with 5 elements,
                // so there is no payload.
                if (nameString == "M1") {
                  ASSERT_EQ(state, S_M1);
                  state = State(state + 1);
                } else if (nameString == "M3") {
                  ASSERT_EQ(state, S_M3);
                  state = State(state + 1);
                } else if (nameString ==
                           "default-templated markers 2.0 with empty options") {
                  EXPECT_EQ(state, S_Markers2DefaultEmptyOptions);
                  state = State(S_Markers2DefaultEmptyOptions + 1);
// TODO: Re-enable this when bug 1646714 lands, and check for stack.
#if 0
              } else if (nameString ==
                         "default-templated markers 2.0 with option") {
                EXPECT_EQ(state, S_Markers2DefaultWithOptions);
                state = State(S_Markers2DefaultWithOptions + 1);
#endif
                } else if (nameString ==
                           "explicitly-default-templated markers 2.0 with "
                           "empty "
                           "options") {
                  EXPECT_EQ(state, S_Markers2ExplicitDefaultEmptyOptions);
                  state = State(S_Markers2ExplicitDefaultEmptyOptions + 1);
                } else if (nameString ==
                           "explicitly-default-templated markers 2.0 with "
                           "option") {
                  EXPECT_EQ(state, S_Markers2ExplicitDefaultWithOptions);
                  state = State(S_Markers2ExplicitDefaultWithOptions + 1);
                }
              } else {
                // root.threads[0].markers.data[i] is an array with 6 elements,
                // so there is a payload.
                GET_JSON(payload, marker[PAYLOAD], Object);

                // root.threads[0].markers.data[i][PAYLOAD] is an object
                // (payload).

                // It should at least have a "type" string.
                GET_JSON(type, payload["type"], String);
                std::string typeString = type.asString();

                if (nameString == "tracing event") {
                  EXPECT_EQ(state, S_tracing_event);
                  state = State(S_tracing_event + 1);
                  EXPECT_EQ(typeString, "tracing");
                  EXPECT_TIMING_INSTANT;
                  EXPECT_EQ_JSON(payload["category"], String, "A");
                  EXPECT_TRUE(payload["stack"].isNull());

                } else if (nameString == "tracing start") {
                  EXPECT_EQ(state, S_tracing_start);
                  state = State(S_tracing_start + 1);
                  EXPECT_EQ(typeString, "tracing");
                  EXPECT_TIMING_START;
                  EXPECT_EQ_JSON(payload["category"], String, "A");
                  EXPECT_TRUE(payload["stack"].isNull());

                } else if (nameString == "tracing end") {
                  EXPECT_EQ(state, S_tracing_end);
                  state = State(S_tracing_end + 1);
                  EXPECT_EQ(typeString, "tracing");
                  EXPECT_TIMING_END;
                  EXPECT_EQ_JSON(payload["category"], String, "A");
                  EXPECT_TRUE(payload["stack"].isNull());

                } else if (nameString == "tracing event with stack") {
                  EXPECT_EQ(state, S_tracing_event_with_stack);
                  state = State(S_tracing_event_with_stack + 1);
                  EXPECT_EQ(typeString, "tracing");
                  EXPECT_TIMING_INSTANT;
                  EXPECT_EQ_JSON(payload["category"], String, "B");
                  EXPECT_TRUE(payload["stack"].isObject());

                } else if (nameString == "auto tracing") {
                  switch (state) {
                    case S_tracing_auto_tracing_start:
                      state = State(S_tracing_auto_tracing_start + 1);
                      EXPECT_EQ(typeString, "tracing");
                      EXPECT_TIMING_START;
                      EXPECT_EQ_JSON(payload["category"], String, "C");
                      EXPECT_TRUE(payload["stack"].isNull());
                      break;
                    case S_tracing_auto_tracing_end:
                      state = State(S_tracing_auto_tracing_end + 1);
                      EXPECT_EQ(typeString, "tracing");
                      EXPECT_TIMING_END;
                      EXPECT_EQ_JSON(payload["category"], String, "C");
                      ASSERT_TRUE(payload["stack"].isNull());
                      break;
                    default:
                      EXPECT_TRUE(state == S_tracing_auto_tracing_start ||
                                  state == S_tracing_auto_tracing_end);
                      break;
                  }

                } else if (nameString ==
                           "default-templated markers 2.0 with option") {
                  // TODO: Remove this when bug 1646714 lands.
                  EXPECT_EQ(state, S_Markers2DefaultWithOptions);
                  state = State(S_Markers2DefaultWithOptions + 1);
                  EXPECT_EQ(typeString, "NoPayloadUserData");
                  EXPECT_FALSE(payload["stack"].isNull());

                } else if (nameString == "FirstMarker") {
                  // Record start and end times, to compare with timestamps in
                  // following markers.
                  EXPECT_EQ(state, S_FirstMarker);
                  ts1Double = marker[START_TIME].asDouble();
                  ts2Double = marker[END_TIME].asDouble();
                  state = State(S_FirstMarker + 1);
                  EXPECT_EQ(typeString, "Text");
                  EXPECT_EQ_JSON(payload["name"], String, "First Marker");

                } else if (nameString == "Gtest custom marker") {
                  EXPECT_EQ(state, S_CustomMarker);
                  state = State(S_CustomMarker + 1);
                  EXPECT_EQ(typeString, "markers-gtest");
                  EXPECT_EQ(payload.size(), 1u + 9u);
                  EXPECT_TRUE(payload["null"].isNull());
                  EXPECT_EQ_JSON(payload["bool-false"], Bool, false);
                  EXPECT_EQ_JSON(payload["bool-true"], Bool, true);
                  EXPECT_EQ_JSON(payload["int"], Int64, 42);
                  EXPECT_EQ_JSON(payload["double"], Double, 43.0);
                  EXPECT_EQ_JSON(payload["text"], String, "gtest text");
                  // Unique strings can be fetched from the string table.
                  ASSERT_TRUE(payload["unique text"].isUInt());
                  auto textIndex = payload["unique text"].asUInt();
                  GET_JSON(uniqueText, stringTable[textIndex], String);
                  ASSERT_TRUE(uniqueText.isString());
                  ASSERT_EQ(uniqueText.asString(), "gtest unique text");
                  // The duplicate unique text should have the exact same index.
                  EXPECT_EQ_JSON(payload["unique text again"], UInt, textIndex);
                  EXPECT_EQ_JSON(payload["time"], Double, ts1Double);

                } else if (nameString == "Gtest special marker") {
                  EXPECT_EQ(state, S_SpecialMarker);
                  state = State(S_SpecialMarker + 1);
                  EXPECT_EQ(typeString, "markers-gtest-special");
                  EXPECT_EQ(payload.size(), 1u) << "Only 'type' in the payload";

                } else if (nameString == "Load 1: http://mozilla.org/") {
                  EXPECT_EQ(state, S_NetworkMarkerPayload_start);
                  state = State(S_NetworkMarkerPayload_start + 1);
                  EXPECT_EQ(typeString, "Network");
                  EXPECT_EQ_JSON(payload["startTime"], Double, ts1Double);
                  EXPECT_EQ_JSON(payload["endTime"], Double, ts2Double);
                  EXPECT_EQ_JSON(payload["id"], Int64, 1);
                  EXPECT_EQ_JSON(payload["URI"], String, "http://mozilla.org/");
                  EXPECT_EQ_JSON(payload["requestMethod"], String, "GET");
                  EXPECT_EQ_JSON(payload["pri"], Int64, 34);
                  EXPECT_EQ_JSON(payload["count"], Int64, 56);
                  EXPECT_EQ_JSON(payload["cache"], String, "Hit");
                  EXPECT_TRUE(payload["RedirectURI"].isNull());
                  EXPECT_TRUE(payload["redirectType"].isNull());
                  EXPECT_TRUE(payload["isHttpToHttpsRedirect"].isNull());
                  EXPECT_TRUE(payload["redirectId"].isNull());
                  EXPECT_TRUE(payload["contentType"].isNull());

                } else if (nameString == "Load 2: http://mozilla.org/") {
                  EXPECT_EQ(state, S_NetworkMarkerPayload_stop);
                  state = State(S_NetworkMarkerPayload_stop + 1);
                  EXPECT_EQ(typeString, "Network");
                  EXPECT_EQ_JSON(payload["startTime"], Double, ts1Double);
                  EXPECT_EQ_JSON(payload["endTime"], Double, ts2Double);
                  EXPECT_EQ_JSON(payload["id"], Int64, 2);
                  EXPECT_EQ_JSON(payload["URI"], String, "http://mozilla.org/");
                  EXPECT_EQ_JSON(payload["requestMethod"], String, "GET");
                  EXPECT_EQ_JSON(payload["pri"], Int64, 34);
                  EXPECT_EQ_JSON(payload["count"], Int64, 56);
                  EXPECT_EQ_JSON(payload["cache"], String, "Unresolved");
                  EXPECT_TRUE(payload["RedirectURI"].isNull());
                  EXPECT_TRUE(payload["redirectType"].isNull());
                  EXPECT_TRUE(payload["isHttpToHttpsRedirect"].isNull());
                  EXPECT_TRUE(payload["redirectId"].isNull());
                  EXPECT_EQ_JSON(payload["contentType"], String, "text/html");

                } else if (nameString == "Load 3: http://mozilla.org/") {
                  EXPECT_EQ(state, S_NetworkMarkerPayload_redirect_temporary);
                  state = State(S_NetworkMarkerPayload_redirect_temporary + 1);
                  EXPECT_EQ(typeString, "Network");
                  EXPECT_EQ_JSON(payload["startTime"], Double, ts1Double);
                  EXPECT_EQ_JSON(payload["endTime"], Double, ts2Double);
                  EXPECT_EQ_JSON(payload["id"], Int64, 3);
                  EXPECT_EQ_JSON(payload["URI"], String, "http://mozilla.org/");
                  EXPECT_EQ_JSON(payload["requestMethod"], String, "GET");
                  EXPECT_EQ_JSON(payload["pri"], Int64, 34);
                  EXPECT_EQ_JSON(payload["count"], Int64, 56);
                  EXPECT_EQ_JSON(payload["cache"], String, "Unresolved");
                  EXPECT_EQ_JSON(payload["RedirectURI"], String,
                                 "http://example.com/");
                  EXPECT_EQ_JSON(payload["redirectType"], String, "Temporary");
                  EXPECT_EQ_JSON(payload["isHttpToHttpsRedirect"], Bool, false);
                  EXPECT_EQ_JSON(payload["redirectId"], Int64, 103);
                  EXPECT_TRUE(payload["contentType"].isNull());

                } else if (nameString == "Load 4: http://mozilla.org/") {
                  EXPECT_EQ(state, S_NetworkMarkerPayload_redirect_permanent);
                  state = State(S_NetworkMarkerPayload_redirect_permanent + 1);
                  EXPECT_EQ(typeString, "Network");
                  EXPECT_EQ_JSON(payload["startTime"], Double, ts1Double);
                  EXPECT_EQ_JSON(payload["endTime"], Double, ts2Double);
                  EXPECT_EQ_JSON(payload["id"], Int64, 4);
                  EXPECT_EQ_JSON(payload["URI"], String, "http://mozilla.org/");
                  EXPECT_EQ_JSON(payload["requestMethod"], String, "GET");
                  EXPECT_EQ_JSON(payload["pri"], Int64, 34);
                  EXPECT_EQ_JSON(payload["count"], Int64, 56);
                  EXPECT_EQ_JSON(payload["cache"], String, "Unresolved");
                  EXPECT_EQ_JSON(payload["RedirectURI"], String,
                                 "http://example.com/");
                  EXPECT_EQ_JSON(payload["redirectType"], String, "Permanent");
                  EXPECT_EQ_JSON(payload["isHttpToHttpsRedirect"], Bool, false);
                  EXPECT_EQ_JSON(payload["redirectId"], Int64, 104);
                  EXPECT_TRUE(payload["contentType"].isNull());

                } else if (nameString == "Load 5: http://mozilla.org/") {
                  EXPECT_EQ(state, S_NetworkMarkerPayload_redirect_internal);
                  state = State(S_NetworkMarkerPayload_redirect_internal + 1);
                  EXPECT_EQ(typeString, "Network");
                  EXPECT_EQ_JSON(payload["startTime"], Double, ts1Double);
                  EXPECT_EQ_JSON(payload["endTime"], Double, ts2Double);
                  EXPECT_EQ_JSON(payload["id"], Int64, 5);
                  EXPECT_EQ_JSON(payload["URI"], String, "http://mozilla.org/");
                  EXPECT_EQ_JSON(payload["requestMethod"], String, "GET");
                  EXPECT_EQ_JSON(payload["pri"], Int64, 34);
                  EXPECT_EQ_JSON(payload["count"], Int64, 56);
                  EXPECT_EQ_JSON(payload["cache"], String, "Unresolved");
                  EXPECT_EQ_JSON(payload["RedirectURI"], String,
                                 "http://example.com/");
                  EXPECT_EQ_JSON(payload["redirectType"], String, "Internal");
                  EXPECT_EQ_JSON(payload["isHttpToHttpsRedirect"], Bool, false);
                  EXPECT_EQ_JSON(payload["redirectId"], Int64, 105);
                  EXPECT_TRUE(payload["contentType"].isNull());

                } else if (nameString == "Load 6: http://mozilla.org/") {
                  EXPECT_EQ(state,
                            S_NetworkMarkerPayload_redirect_internal_sts);
                  state =
                      State(S_NetworkMarkerPayload_redirect_internal_sts + 1);
                  EXPECT_EQ(typeString, "Network");
                  EXPECT_EQ_JSON(payload["startTime"], Double, ts1Double);
                  EXPECT_EQ_JSON(payload["endTime"], Double, ts2Double);
                  EXPECT_EQ_JSON(payload["id"], Int64, 6);
                  EXPECT_EQ_JSON(payload["URI"], String, "http://mozilla.org/");
                  EXPECT_EQ_JSON(payload["requestMethod"], String, "GET");
                  EXPECT_EQ_JSON(payload["pri"], Int64, 34);
                  EXPECT_EQ_JSON(payload["count"], Int64, 56);
                  EXPECT_EQ_JSON(payload["cache"], String, "Unresolved");
                  EXPECT_EQ_JSON(payload["RedirectURI"], String,
                                 "http://example.com/");
                  EXPECT_EQ_JSON(payload["redirectType"], String, "Internal");
                  EXPECT_EQ_JSON(payload["isHttpToHttpsRedirect"], Bool, true);
                  EXPECT_EQ_JSON(payload["redirectId"], Int64, 106);
                  EXPECT_TRUE(payload["contentType"].isNull());

                } else if (nameString == "Text in main thread with stack") {
                  EXPECT_EQ(state, S_TextWithStack);
                  state = State(S_TextWithStack + 1);
                  EXPECT_EQ(typeString, "Text");
                  EXPECT_FALSE(payload["stack"].isNull());
                  EXPECT_TIMING_INTERVAL_AT(ts1Double, ts2Double);
                  EXPECT_EQ_JSON(payload["name"], String, "");

                } else if (nameString == "Text from main thread with stack") {
                  EXPECT_EQ(state, S_TextToMTWithStack);
                  state = State(S_TextToMTWithStack + 1);
                  EXPECT_EQ(typeString, "Text");
                  EXPECT_FALSE(payload["stack"].isNull());
                  EXPECT_EQ_JSON(payload["name"], String, "");

                } else if (nameString ==
                           "Text in registered thread with stack") {
                  ADD_FAILURE()
                      << "Unexpected 'Text in registered thread with stack'";

                } else if (nameString ==
                           "Text from registered thread with stack") {
                  EXPECT_EQ(state, S_RegThread_TextToMTWithStack);
                  state = State(S_RegThread_TextToMTWithStack + 1);
                  EXPECT_EQ(typeString, "Text");
                  EXPECT_FALSE(payload["stack"].isNull());
                  EXPECT_EQ_JSON(payload["name"], String, "");

                } else if (nameString ==
                           "Text in unregistered thread with stack") {
                  ADD_FAILURE()
                      << "Unexpected 'Text in unregistered thread with stack'";

                } else if (nameString ==
                           "Text from unregistered thread with stack") {
                  EXPECT_EQ(state, S_UnregThread_TextToMTWithStack);
                  state = State(S_UnregThread_TextToMTWithStack + 1);
                  EXPECT_EQ(typeString, "Text");
                  EXPECT_TRUE(payload["stack"].isNull());
                  EXPECT_EQ_JSON(payload["name"], String, "");
                }
              }  // marker with payload
            }    // for (marker : data)
          }      // markers.data
        }        // markers
      }          // thread0
    }            // threads
    // We should have read all expected markers.
    EXPECT_EQ(state, S_LAST);

    {
      GET_JSON(meta, root["meta"], Object);

      {
        GET_JSON(markerSchema, meta["markerSchema"], Array);

        std::set<std::string> testedSchemaNames;

        for (const Json::Value& schema : markerSchema) {
          GET_JSON(name, schema["name"], String);
          const std::string nameString = name.asString();

          GET_JSON(display, schema["display"], Array);

          GET_JSON(data, schema["data"], Array);

          EXPECT_TRUE(
              testedSchemaNames
                  .insert(std::string(nameString.data(), nameString.size()))
                  .second)
              << "Each schema name should be unique (inserted once in the set)";

          if (nameString == "Text") {
            EXPECT_EQ(display.size(), 2u);
            EXPECT_EQ(display[0u].asString(), "marker-chart");
            EXPECT_EQ(display[1u].asString(), "marker-table");

            ASSERT_EQ(data.size(), 1u);

            ASSERT_TRUE(data[0u].isObject());
            EXPECT_EQ_JSON(data[0u]["key"], String, "name");
            EXPECT_EQ_JSON(data[0u]["label"], String, "Details");
            EXPECT_EQ_JSON(data[0u]["format"], String, "string");

          } else if (nameString == "NoPayloadUserData") {
            // TODO: Remove this when bug 1646714 lands.
            EXPECT_EQ(display.size(), 2u);
            EXPECT_EQ(display[0u].asString(), "marker-chart");
            EXPECT_EQ(display[1u].asString(), "marker-table");

            ASSERT_EQ(data.size(), 0u);

          } else if (nameString == "FileIO") {
            // These are defined in ProfilerIOInterposeObserver.cpp

          } else if (nameString == "tracing") {
            EXPECT_EQ(display.size(), 3u);
            EXPECT_EQ(display[0u].asString(), "marker-chart");
            EXPECT_EQ(display[1u].asString(), "marker-table");
            EXPECT_EQ(display[2u].asString(), "timeline-overview");

            ASSERT_EQ(data.size(), 1u);

            ASSERT_TRUE(data[0u].isObject());
            EXPECT_EQ_JSON(data[0u]["key"], String, "category");
            EXPECT_EQ_JSON(data[0u]["label"], String, "Type");
            EXPECT_EQ_JSON(data[0u]["format"], String, "string");

          } else if (nameString == "BHR-detected hang") {
            EXPECT_EQ(display.size(), 2u);
            EXPECT_EQ(display[0u].asString(), "marker-chart");
            EXPECT_EQ(display[1u].asString(), "marker-table");

            ASSERT_EQ(data.size(), 0u);

          } else if (nameString == "MainThreadLongTask") {
            EXPECT_EQ(display.size(), 2u);
            EXPECT_EQ(display[0u].asString(), "marker-chart");
            EXPECT_EQ(display[1u].asString(), "marker-table");

            ASSERT_EQ(data.size(), 1u);

            ASSERT_TRUE(data[0u].isObject());
            EXPECT_EQ_JSON(data[0u]["key"], String, "category");
            EXPECT_EQ_JSON(data[0u]["label"], String, "Type");
            EXPECT_EQ_JSON(data[0u]["format"], String, "string");

          } else if (nameString == "Log") {
            EXPECT_EQ(display.size(), 1u);
            EXPECT_EQ(display[0u].asString(), "marker-table");

            ASSERT_EQ(data.size(), 2u);

            ASSERT_TRUE(data[0u].isObject());
            EXPECT_EQ_JSON(data[0u]["key"], String, "module");
            EXPECT_EQ_JSON(data[0u]["label"], String, "Module");
            EXPECT_EQ_JSON(data[0u]["format"], String, "string");

            ASSERT_TRUE(data[1u].isObject());
            EXPECT_EQ_JSON(data[1u]["key"], String, "name");
            EXPECT_EQ_JSON(data[1u]["label"], String, "Name");
            EXPECT_EQ_JSON(data[1u]["format"], String, "string");

          } else if (nameString == "MediaSample") {
            EXPECT_EQ(display.size(), 2u);
            EXPECT_EQ(display[0u].asString(), "marker-chart");
            EXPECT_EQ(display[1u].asString(), "marker-table");

            ASSERT_EQ(data.size(), 2u);

            ASSERT_TRUE(data[0u].isObject());
            EXPECT_EQ_JSON(data[0u]["key"], String, "sampleStartTimeUs");
            EXPECT_EQ_JSON(data[0u]["label"], String, "Sample start time");
            EXPECT_EQ_JSON(data[0u]["format"], String, "microseconds");

            ASSERT_TRUE(data[1u].isObject());
            EXPECT_EQ_JSON(data[1u]["key"], String, "sampleEndTimeUs");
            EXPECT_EQ_JSON(data[1u]["label"], String, "Sample end time");
            EXPECT_EQ_JSON(data[1u]["format"], String, "microseconds");

          } else if (nameString == "Budget") {
            EXPECT_EQ(display.size(), 2u);
            EXPECT_EQ(display[0u].asString(), "marker-chart");
            EXPECT_EQ(display[1u].asString(), "marker-table");

            ASSERT_EQ(data.size(), 0u);

          } else if (nameString == "markers-gtest") {
            EXPECT_EQ(display.size(), 7u);
            EXPECT_EQ(display[0u].asString(), "marker-chart");
            EXPECT_EQ(display[1u].asString(), "marker-table");
            EXPECT_EQ(display[2u].asString(), "timeline-overview");
            EXPECT_EQ(display[3u].asString(), "timeline-memory");
            EXPECT_EQ(display[4u].asString(), "timeline-ipc");
            EXPECT_EQ(display[5u].asString(), "timeline-fileio");
            EXPECT_EQ(display[6u].asString(), "stack-chart");

            EXPECT_EQ_JSON(schema["chartLabel"], String, "chart label");
            EXPECT_EQ_JSON(schema["tooltipLabel"], String, "tooltip label");
            EXPECT_EQ_JSON(schema["tableLabel"], String, "table label");

            ASSERT_EQ(data.size(), 14u);

            ASSERT_TRUE(data[0u].isObject());
            EXPECT_EQ_JSON(data[0u]["key"], String, "key with url");
            EXPECT_TRUE(data[0u]["label"].isNull());
            EXPECT_EQ_JSON(data[0u]["format"], String, "url");
            EXPECT_TRUE(data[0u]["searchable"].isNull());

            ASSERT_TRUE(data[1u].isObject());
            EXPECT_EQ_JSON(data[1u]["key"], String, "key with label filePath");
            EXPECT_EQ_JSON(data[1u]["label"], String, "label filePath");
            EXPECT_EQ_JSON(data[1u]["format"], String, "file-path");
            EXPECT_TRUE(data[1u]["searchable"].isNull());

            ASSERT_TRUE(data[2u].isObject());
            EXPECT_EQ_JSON(data[2u]["key"], String,
                           "key with string not-searchable");
            EXPECT_TRUE(data[2u]["label"].isNull());
            EXPECT_EQ_JSON(data[2u]["format"], String, "string");
            EXPECT_EQ_JSON(data[2u]["searchable"], Bool, false);

            ASSERT_TRUE(data[3u].isObject());
            EXPECT_EQ_JSON(data[3u]["key"], String,
                           "key with label duration searchable");
            EXPECT_TRUE(data[3u]["label duration"].isNull());
            EXPECT_EQ_JSON(data[3u]["format"], String, "duration");
            EXPECT_EQ_JSON(data[3u]["searchable"], Bool, true);

            ASSERT_TRUE(data[4u].isObject());
            EXPECT_EQ_JSON(data[4u]["key"], String, "key with time");
            EXPECT_TRUE(data[4u]["label"].isNull());
            EXPECT_EQ_JSON(data[4u]["format"], String, "time");
            EXPECT_TRUE(data[4u]["searchable"].isNull());

            ASSERT_TRUE(data[5u].isObject());
            EXPECT_EQ_JSON(data[5u]["key"], String, "key with seconds");
            EXPECT_TRUE(data[5u]["label"].isNull());
            EXPECT_EQ_JSON(data[5u]["format"], String, "seconds");
            EXPECT_TRUE(data[5u]["searchable"].isNull());

            ASSERT_TRUE(data[6u].isObject());
            EXPECT_EQ_JSON(data[6u]["key"], String, "key with milliseconds");
            EXPECT_TRUE(data[6u]["label"].isNull());
            EXPECT_EQ_JSON(data[6u]["format"], String, "milliseconds");
            EXPECT_TRUE(data[6u]["searchable"].isNull());

            ASSERT_TRUE(data[7u].isObject());
            EXPECT_EQ_JSON(data[7u]["key"], String, "key with microseconds");
            EXPECT_TRUE(data[7u]["label"].isNull());
            EXPECT_EQ_JSON(data[7u]["format"], String, "microseconds");
            EXPECT_TRUE(data[7u]["searchable"].isNull());

            ASSERT_TRUE(data[8u].isObject());
            EXPECT_EQ_JSON(data[8u]["key"], String, "key with nanoseconds");
            EXPECT_TRUE(data[8u]["label"].isNull());
            EXPECT_EQ_JSON(data[8u]["format"], String, "nanoseconds");
            EXPECT_TRUE(data[8u]["searchable"].isNull());

            ASSERT_TRUE(data[9u].isObject());
            EXPECT_EQ_JSON(data[9u]["key"], String, "key with bytes");
            EXPECT_TRUE(data[9u]["label"].isNull());
            EXPECT_EQ_JSON(data[9u]["format"], String, "bytes");
            EXPECT_TRUE(data[9u]["searchable"].isNull());

            ASSERT_TRUE(data[10u].isObject());
            EXPECT_EQ_JSON(data[10u]["key"], String, "key with percentage");
            EXPECT_TRUE(data[10u]["label"].isNull());
            EXPECT_EQ_JSON(data[10u]["format"], String, "percentage");
            EXPECT_TRUE(data[10u]["searchable"].isNull());

            ASSERT_TRUE(data[11u].isObject());
            EXPECT_EQ_JSON(data[11u]["key"], String, "key with integer");
            EXPECT_TRUE(data[11u]["label"].isNull());
            EXPECT_EQ_JSON(data[11u]["format"], String, "integer");
            EXPECT_TRUE(data[11u]["searchable"].isNull());

            ASSERT_TRUE(data[12u].isObject());
            EXPECT_EQ_JSON(data[12u]["key"], String, "key with decimal");
            EXPECT_TRUE(data[12u]["label"].isNull());
            EXPECT_EQ_JSON(data[12u]["format"], String, "decimal");
            EXPECT_TRUE(data[12u]["searchable"].isNull());

            ASSERT_TRUE(data[13u].isObject());
            EXPECT_EQ_JSON(data[13u]["label"], String, "static label");
            EXPECT_EQ_JSON(data[13u]["value"], String, "static value");

          } else if (nameString == "markers-gtest-special") {
            EXPECT_EQ(display.size(), 0u);
            ASSERT_EQ(data.size(), 0u);

          } else if (nameString == "markers-gtest-unused") {
            ADD_FAILURE() << "Schema for GtestUnusedMarker should not be here";

          } else {
            printf("FYI: Unknown marker schema '%s'\n", nameString.c_str());
          }
        }

        // Check that we've got all expected schema.
        EXPECT_TRUE(testedSchemaNames.find("Text") != testedSchemaNames.end());
        EXPECT_TRUE(testedSchemaNames.find("tracing") !=
                    testedSchemaNames.end());
        EXPECT_TRUE(testedSchemaNames.find("MediaSample") !=
                    testedSchemaNames.end());
      }  // markerSchema
    }    // meta
  });

  Maybe<ProfilerBufferInfo> info = profiler_get_buffer_info();
  EXPECT_TRUE(info.isSome());
  printf("Profiler buffer range: %llu .. %llu (%llu bytes)\n",
         static_cast<unsigned long long>(info->mRangeStart),
         static_cast<unsigned long long>(info->mRangeEnd),
         // sizeof(ProfileBufferEntry) == 9
         (static_cast<unsigned long long>(info->mRangeEnd) -
          static_cast<unsigned long long>(info->mRangeStart)) *
             9);
  printf("Stats:         min(us) .. mean(us) .. max(us)  [count]\n");
  printf("- Intervals:   %7.1f .. %7.1f  .. %7.1f  [%u]\n",
         info->mIntervalsUs.min, info->mIntervalsUs.sum / info->mIntervalsUs.n,
         info->mIntervalsUs.max, info->mIntervalsUs.n);
  printf("- Overheads:   %7.1f .. %7.1f  .. %7.1f  [%u]\n",
         info->mOverheadsUs.min, info->mOverheadsUs.sum / info->mOverheadsUs.n,
         info->mOverheadsUs.max, info->mOverheadsUs.n);
  printf("  - Locking:   %7.1f .. %7.1f  .. %7.1f  [%u]\n",
         info->mLockingsUs.min, info->mLockingsUs.sum / info->mLockingsUs.n,
         info->mLockingsUs.max, info->mLockingsUs.n);
  printf("  - Clearning: %7.1f .. %7.1f  .. %7.1f  [%u]\n",
         info->mCleaningsUs.min, info->mCleaningsUs.sum / info->mCleaningsUs.n,
         info->mCleaningsUs.max, info->mCleaningsUs.n);
  printf("  - Counters:  %7.1f .. %7.1f  .. %7.1f  [%u]\n",
         info->mCountersUs.min, info->mCountersUs.sum / info->mCountersUs.n,
         info->mCountersUs.max, info->mCountersUs.n);
  printf("  - Threads:   %7.1f .. %7.1f  .. %7.1f  [%u]\n",
         info->mThreadsUs.min, info->mThreadsUs.sum / info->mThreadsUs.n,
         info->mThreadsUs.max, info->mThreadsUs.n);

  profiler_stop();

  // Try to add markers while the profiler is stopped.
  PROFILER_MARKER_UNTYPED("marker after profiler_stop", OTHER);

  // Warning: this could be racy
  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL, features,
                 filters, MOZ_ARRAY_LENGTH(filters), 0);

  // This last marker shouldn't get streamed.
  SpliceableChunkedJSONWriter w2;
  w2.Start();
  EXPECT_TRUE(::profiler_stream_json_for_this_process(w2));
  w2.End();
  UniquePtr<char[]> profile2 = w.ChunkedWriteFunc().CopyData();
  ASSERT_TRUE(!!profile2.get());
  EXPECT_TRUE(
      std::string_view(profile2.get()).find("marker after profiler_stop") ==
      std::string_view::npos);

  profiler_stop();
}

#define COUNTER_NAME "TestCounter"
#define COUNTER_DESCRIPTION "Test of counters in profiles"
#define COUNTER_NAME2 "Counter2"
#define COUNTER_DESCRIPTION2 "Second Test of counters in profiles"

PROFILER_DEFINE_COUNT_TOTAL(TestCounter, COUNTER_NAME, COUNTER_DESCRIPTION);
PROFILER_DEFINE_COUNT_TOTAL(TestCounter2, COUNTER_NAME2, COUNTER_DESCRIPTION2);

TEST(GeckoProfiler, Counters)
{
  uint32_t features = ProfilerFeature::Threads;
  const char* filters[] = {"GeckoMain", "Compositor"};

  // Inactive -> Active
  profiler_ensure_started(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                          features, filters, MOZ_ARRAY_LENGTH(filters), 0);

  AUTO_PROFILER_COUNT_TOTAL(TestCounter, 10);
  PR_Sleep(PR_MillisecondsToInterval(200));
  AUTO_PROFILER_COUNT_TOTAL(TestCounter, 7);
  PR_Sleep(PR_MillisecondsToInterval(200));
  AUTO_PROFILER_COUNT_TOTAL(TestCounter, -17);
  PR_Sleep(PR_MillisecondsToInterval(200));

  // Verify we got counters in the output
  SpliceableChunkedJSONWriter w;
  ASSERT_TRUE(::profiler_stream_json_for_this_process(w));

  UniquePtr<char[]> profile = w.ChunkedWriteFunc().CopyData();

  // counter name and description should appear as is.
  ASSERT_TRUE(strstr(profile.get(), COUNTER_NAME));
  ASSERT_TRUE(strstr(profile.get(), COUNTER_DESCRIPTION));
  ASSERT_FALSE(strstr(profile.get(), COUNTER_NAME2));
  ASSERT_FALSE(strstr(profile.get(), COUNTER_DESCRIPTION2));

  AUTO_PROFILER_COUNT_TOTAL(TestCounter2, 10);
  PR_Sleep(PR_MillisecondsToInterval(200));

  ASSERT_TRUE(::profiler_stream_json_for_this_process(w));

  profile = w.ChunkedWriteFunc().CopyData();
  ASSERT_TRUE(strstr(profile.get(), COUNTER_NAME));
  ASSERT_TRUE(strstr(profile.get(), COUNTER_DESCRIPTION));
  ASSERT_TRUE(strstr(profile.get(), COUNTER_NAME2));
  ASSERT_TRUE(strstr(profile.get(), COUNTER_DESCRIPTION2));

  profiler_stop();
}

TEST(GeckoProfiler, Time)
{
  uint32_t features = ProfilerFeature::StackWalk;
  const char* filters[] = {"GeckoMain"};

  double t1 = profiler_time();
  double t2 = profiler_time();
  ASSERT_TRUE(t1 <= t2);

  // profiler_start() restarts the timer used by profiler_time().
  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL, features,
                 filters, MOZ_ARRAY_LENGTH(filters), 0);

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
  const char* filters[] = {"GeckoMain"};

  ASSERT_TRUE(!profiler_get_profile());

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL, features,
                 filters, MOZ_ARRAY_LENGTH(filters), 0);

  mozilla::Maybe<uint32_t> activeFeatures = profiler_features_if_active();
  ASSERT_TRUE(activeFeatures.isSome());
  // Not all platforms support stack-walking.
  const bool hasStackWalk = ProfilerFeature::HasStackWalk(*activeFeatures);
  // "threads" may automatically be added when `filters` is not empty.
  const bool hasThreads = ProfilerFeature::HasThreads(*activeFeatures);

  UniquePtr<char[]> profile = profiler_get_profile();
  JSONOutputCheck(profile.get(), [&](const Json::Value& aRoot) {
    GET_JSON(meta, aRoot["meta"], Object);
    {
      GET_JSON(configuration, meta["configuration"], Object);
      {
        GET_JSON(features, configuration["features"], Array);
        {
          EXPECT_EQ(features.size(),
                    (hasStackWalk ? 1u : 0u) + (hasThreads ? 1u : 0u));
          if (hasStackWalk) {
            EXPECT_JSON_ARRAY_CONTAINS(features, String, "stackwalk");
          }
          if (hasThreads) {
            EXPECT_JSON_ARRAY_CONTAINS(features, String, "threads");
          }
        }
        GET_JSON(threads, configuration["threads"], Array);
        {
          EXPECT_EQ(threads.size(), 1u);
          EXPECT_JSON_ARRAY_CONTAINS(threads, String, "GeckoMain");
        }
      }
    }
  });

  profiler_stop();

  ASSERT_TRUE(!profiler_get_profile());
}

TEST(GeckoProfiler, StreamJSONForThisProcess)
{
  uint32_t features = ProfilerFeature::StackWalk;
  const char* filters[] = {"GeckoMain"};

  SpliceableChunkedJSONWriter w;
  ASSERT_TRUE(!::profiler_stream_json_for_this_process(w));

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL, features,
                 filters, MOZ_ARRAY_LENGTH(filters), 0);

  w.Start();
  ASSERT_TRUE(::profiler_stream_json_for_this_process(w));
  w.End();

  UniquePtr<char[]> profile = w.ChunkedWriteFunc().CopyData();

  JSONOutputCheck(profile.get(), [](const Json::Value&) {});

  profiler_stop();

  ASSERT_TRUE(!::profiler_stream_json_for_this_process(w));
}

TEST(GeckoProfiler, StreamJSONForThisProcessThreaded)
{
  // Same as the previous test, but calling some things on background threads.
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_NewNamedThread("GeckoProfGTest", getter_AddRefs(thread));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  uint32_t features = ProfilerFeature::StackWalk;
  const char* filters[] = {"GeckoMain"};

  SpliceableChunkedJSONWriter w;
  ASSERT_TRUE(!::profiler_stream_json_for_this_process(w));

  // Start the profiler on the main thread.
  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL, features,
                 filters, MOZ_ARRAY_LENGTH(filters), 0);

  // Call profiler_stream_json_for_this_process on a background thread.
  thread->Dispatch(
      NS_NewRunnableFunction(
          "GeckoProfiler_StreamJSONForThisProcessThreaded_Test::TestBody",
          [&]() {
            w.Start();
            ASSERT_TRUE(::profiler_stream_json_for_this_process(w));
            w.End();
          }),
      NS_DISPATCH_SYNC);

  UniquePtr<char[]> profile = w.ChunkedWriteFunc().CopyData();

  JSONOutputCheck(profile.get(), [](const Json::Value&) {});

  // Stop the profiler and call profiler_stream_json_for_this_process on a
  // background thread.
  thread->Dispatch(
      NS_NewRunnableFunction(
          "GeckoProfiler_StreamJSONForThisProcessThreaded_Test::TestBody",
          [&]() {
            profiler_stop();
            ASSERT_TRUE(!::profiler_stream_json_for_this_process(w));
          }),
      NS_DISPATCH_SYNC);
  thread->Shutdown();

  // Call profiler_stream_json_for_this_process on the main thread.
  ASSERT_TRUE(!::profiler_stream_json_for_this_process(w));
}

TEST(GeckoProfiler, ProfilingStack)
{
  uint32_t features = ProfilerFeature::StackWalk;
  const char* filters[] = {"GeckoMain"};

  AUTO_PROFILER_LABEL("A::B", OTHER);

  UniqueFreePtr<char> dynamic(strdup("dynamic"));
  {
    AUTO_PROFILER_LABEL_DYNAMIC_CSTR("A::C", JS, dynamic.get());
    AUTO_PROFILER_LABEL_DYNAMIC_NSCSTRING("A::C2", JS,
                                          nsDependentCString(dynamic.get()));
    AUTO_PROFILER_LABEL_DYNAMIC_LOSSY_NSSTRING(
        "A::C3", JS, NS_ConvertUTF8toUTF16(dynamic.get()));

    profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                   features, filters, MOZ_ARRAY_LENGTH(filters), 0);

    ASSERT_TRUE(profiler_get_backtrace());
  }

  AutoProfilerLabel label1("A", nullptr, JS::ProfilingCategoryPair::DOM);
  AutoProfilerLabel label2("A", dynamic.get(),
                           JS::ProfilingCategoryPair::NETWORK);
  ASSERT_TRUE(profiler_get_backtrace());

  profiler_stop();

  ASSERT_TRUE(!profiler_get_profile());
}

TEST(GeckoProfiler, Bug1355807)
{
  uint32_t features = ProfilerFeature::JS;
  const char* manyThreadsFilter[] = {""};
  const char* fewThreadsFilter[] = {"GeckoMain"};

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL, features,
                 manyThreadsFilter, MOZ_ARRAY_LENGTH(manyThreadsFilter), 0);

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL, features,
                 fewThreadsFilter, MOZ_ARRAY_LENGTH(fewThreadsFilter), 0);

  // In bug 1355807 this caused an assertion failure in StopJSSampling().
  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL, features,
                 fewThreadsFilter, MOZ_ARRAY_LENGTH(fewThreadsFilter), 0);

  profiler_stop();
}

class GTestStackCollector final : public ProfilerStackCollector {
 public:
  GTestStackCollector() : mSetIsMainThread(0), mFrames(0) {}

  virtual void SetIsMainThread() { mSetIsMainThread++; }

  virtual void CollectNativeLeafAddr(void* aAddr) { mFrames++; }
  virtual void CollectJitReturnAddr(void* aAddr) { mFrames++; }
  virtual void CollectWasmFrame(const char* aLabel) { mFrames++; }
  virtual void CollectProfilingStackFrame(
      const js::ProfilingStackFrame& aFrame) {
    mFrames++;
  }

  int mSetIsMainThread;
  int mFrames;
};

void DoSuspendAndSample(ProfilerThreadId aTidToSample,
                        nsIThread* aSamplingThread) {
  aSamplingThread->Dispatch(
      NS_NewRunnableFunction(
          "GeckoProfiler_SuspendAndSample_Test::TestBody",
          [&]() {
            uint32_t features = ProfilerFeature::Leaf;
            GTestStackCollector collector;
            profiler_suspend_and_sample_thread(aTidToSample, features,
                                               collector,
                                               /* sampleNative = */ true);

            ASSERT_TRUE(collector.mSetIsMainThread ==
                        (aTidToSample == profiler_main_thread_id()));
            ASSERT_TRUE(collector.mFrames > 0);
          }),
      NS_DISPATCH_SYNC);
}

TEST(GeckoProfiler, SuspendAndSample)
{
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_NewNamedThread("GeckoProfGTest", getter_AddRefs(thread));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  ProfilerThreadId tid = profiler_current_thread_id();

  ASSERT_TRUE(!profiler_is_active());

  // Suspend and sample while the profiler is inactive.
  DoSuspendAndSample(tid, thread);

  DoSuspendAndSample(ProfilerThreadId{}, thread);

  uint32_t features = ProfilerFeature::JS | ProfilerFeature::Threads;
  const char* filters[] = {"GeckoMain", "Compositor"};

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL, features,
                 filters, MOZ_ARRAY_LENGTH(filters), 0);

  ASSERT_TRUE(profiler_is_active());

  // Suspend and sample while the profiler is active.
  DoSuspendAndSample(tid, thread);

  DoSuspendAndSample(ProfilerThreadId{}, thread);

  profiler_stop();

  ASSERT_TRUE(!profiler_is_active());
}

// Returns `static_cast<SamplingState>(-1)` if callback could not be installed.
static SamplingState WaitForSamplingState() {
  Atomic<int> samplingState{-1};

  if (!profiler_callback_after_sampling([&](SamplingState aSamplingState) {
        samplingState = static_cast<int>(aSamplingState);
      })) {
    return static_cast<SamplingState>(-1);
  }

  while (samplingState == -1) {
  }

  return static_cast<SamplingState>(static_cast<int>(samplingState));
}

TEST(GeckoProfiler, PostSamplingCallback)
{
  const char* filters[] = {"GeckoMain"};

  ASSERT_TRUE(!profiler_is_active());
  ASSERT_TRUE(!profiler_callback_after_sampling(
      [&](SamplingState) { ASSERT_TRUE(false); }));

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                 ProfilerFeature::StackWalk, filters, MOZ_ARRAY_LENGTH(filters),
                 0);
  {
    // Stack sampling -> This label should appear at least once.
    AUTO_PROFILER_LABEL("PostSamplingCallback completed", OTHER);
    ASSERT_EQ(WaitForSamplingState(), SamplingState::SamplingCompleted);
  }
  UniquePtr<char[]> profileCompleted = profiler_get_profile();
  JSONOutputCheck(profileCompleted.get(), [](const Json::Value& aRoot) {
    GET_JSON(threads, aRoot["threads"], Array);
    {
      GET_JSON(thread0, threads[0], Object);
      {
        EXPECT_JSON_ARRAY_CONTAINS(thread0["stringTable"], String,
                                   "PostSamplingCallback completed");
      }
    }
  });

  profiler_pause();
  {
    // Paused -> This label should not appear.
    AUTO_PROFILER_LABEL("PostSamplingCallback paused", OTHER);
    ASSERT_EQ(WaitForSamplingState(), SamplingState::SamplingPaused);
  }
  UniquePtr<char[]> profilePaused = profiler_get_profile();
  JSONOutputCheck(profilePaused.get(), [](const Json::Value& aRoot) {});
  // This string shouldn't appear *anywhere* in the profile.
  ASSERT_FALSE(strstr(profilePaused.get(), "PostSamplingCallback paused"));

  profiler_resume();
  {
    // Stack sampling -> This label should appear at least once.
    AUTO_PROFILER_LABEL("PostSamplingCallback resumed", OTHER);
    ASSERT_EQ(WaitForSamplingState(), SamplingState::SamplingCompleted);
  }
  UniquePtr<char[]> profileResumed = profiler_get_profile();
  JSONOutputCheck(profileResumed.get(), [](const Json::Value& aRoot) {
    GET_JSON(threads, aRoot["threads"], Array);
    {
      GET_JSON(thread0, threads[0], Object);
      {
        EXPECT_JSON_ARRAY_CONTAINS(thread0["stringTable"], String,
                                   "PostSamplingCallback resumed");
      }
    }
  });

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                 ProfilerFeature::StackWalk | ProfilerFeature::NoStackSampling,
                 filters, MOZ_ARRAY_LENGTH(filters), 0);
  {
    // No stack sampling -> This label should not appear.
    AUTO_PROFILER_LABEL("PostSamplingCallback completed (no stacks)", OTHER);
    ASSERT_EQ(WaitForSamplingState(), SamplingState::NoStackSamplingCompleted);
  }
  UniquePtr<char[]> profileNoStacks = profiler_get_profile();
  JSONOutputCheck(profileNoStacks.get(), [](const Json::Value& aRoot) {});
  // This string shouldn't appear *anywhere* in the profile.
  ASSERT_FALSE(strstr(profileNoStacks.get(),
                      "PostSamplingCallback completed (no stacks)"));

  // Note: There is no non-racy way to test for SamplingState::JustStopped, as
  // it would require coordination between `profiler_stop()` and another thread
  // doing `profiler_callback_after_sampling()` at just the right moment.

  profiler_stop();
  ASSERT_TRUE(!profiler_is_active());
  ASSERT_TRUE(!profiler_callback_after_sampling(
      [&](SamplingState) { ASSERT_TRUE(false); }));
}

TEST(GeckoProfiler, ProfilingStateCallback)
{
  const char* filters[] = {"GeckoMain"};

  ASSERT_TRUE(!profiler_is_active());

  struct ProfilingStateAndId {
    ProfilingState mProfilingState;
    int mId;
  };
  DataMutex<Vector<ProfilingStateAndId>> states{"Profiling states"};
  auto CreateCallback = [&states](int id) {
    return [id, &states](ProfilingState aProfilingState) {
      auto lockedStates = states.Lock();
      ASSERT_TRUE(
          lockedStates->append(ProfilingStateAndId{aProfilingState, id}));
    };
  };
  auto CheckStatesIsEmpty = [&states]() {
    auto lockedStates = states.Lock();
    EXPECT_TRUE(lockedStates->empty());
  };
  auto CheckStatesOnlyContains = [&states](ProfilingState aProfilingState,
                                           int aId) {
    auto lockedStates = states.Lock();
    EXPECT_EQ(lockedStates->length(), 1u);
    if (lockedStates->length() >= 1u) {
      EXPECT_EQ((*lockedStates)[0].mProfilingState, aProfilingState);
      EXPECT_EQ((*lockedStates)[0].mId, aId);
    }
    lockedStates->clear();
  };

  profiler_add_state_change_callback(AllProfilingStates(), CreateCallback(1),
                                     1);
  // This is in case of error, and it also exercises the (allowed) removal of
  // unknown callback ids.
  auto cleanup1 = mozilla::MakeScopeExit(
      []() { profiler_remove_state_change_callback(1); });
  CheckStatesIsEmpty();

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                 ProfilerFeature::StackWalk, filters, MOZ_ARRAY_LENGTH(filters),
                 0);

  CheckStatesOnlyContains(ProfilingState::Started, 1);

  profiler_add_state_change_callback(AllProfilingStates(), CreateCallback(2),
                                     2);
  // This is in case of error, and it also exercises the (allowed) removal of
  // unknown callback ids.
  auto cleanup2 = mozilla::MakeScopeExit(
      []() { profiler_remove_state_change_callback(2); });
  CheckStatesOnlyContains(ProfilingState::AlreadyActive, 2);

  profiler_remove_state_change_callback(2);
  CheckStatesOnlyContains(ProfilingState::RemovingCallback, 2);
  // Note: The actual removal is effectively tested below, by not seeing any
  // more invocations of the 2nd callback.

  ASSERT_EQ(WaitForSamplingState(), SamplingState::SamplingCompleted);
  UniquePtr<char[]> profileCompleted = profiler_get_profile();
  CheckStatesOnlyContains(ProfilingState::GeneratingProfile, 1);
  JSONOutputCheck(profileCompleted.get(), [](const Json::Value& aRoot) {});

  profiler_pause();
  CheckStatesOnlyContains(ProfilingState::Pausing, 1);
  UniquePtr<char[]> profilePaused = profiler_get_profile();
  CheckStatesOnlyContains(ProfilingState::GeneratingProfile, 1);
  JSONOutputCheck(profilePaused.get(), [](const Json::Value& aRoot) {});

  profiler_resume();
  CheckStatesOnlyContains(ProfilingState::Resumed, 1);
  ASSERT_EQ(WaitForSamplingState(), SamplingState::SamplingCompleted);
  UniquePtr<char[]> profileResumed = profiler_get_profile();
  CheckStatesOnlyContains(ProfilingState::GeneratingProfile, 1);
  JSONOutputCheck(profileResumed.get(), [](const Json::Value& aRoot) {});

  // This effectively stops the profiler before restarting it, but
  // ProfilingState::Stopping is not notified. See `profiler_start` for details.
  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                 ProfilerFeature::StackWalk | ProfilerFeature::NoStackSampling,
                 filters, MOZ_ARRAY_LENGTH(filters), 0);
  CheckStatesOnlyContains(ProfilingState::Started, 1);
  ASSERT_EQ(WaitForSamplingState(), SamplingState::NoStackSamplingCompleted);
  UniquePtr<char[]> profileNoStacks = profiler_get_profile();
  CheckStatesOnlyContains(ProfilingState::GeneratingProfile, 1);
  JSONOutputCheck(profileNoStacks.get(), [](const Json::Value& aRoot) {});

  profiler_stop();
  CheckStatesOnlyContains(ProfilingState::Stopping, 1);
  ASSERT_TRUE(!profiler_is_active());

  profiler_remove_state_change_callback(1);
  CheckStatesOnlyContains(ProfilingState::RemovingCallback, 1);

  // Note: ProfilingState::ShuttingDown cannot be tested here, and the profiler
  // can only be shut down once per process.
}

TEST(GeckoProfiler, BaseProfilerHandOff)
{
  const char* filters[] = {"GeckoMain"};

  ASSERT_TRUE(!baseprofiler::profiler_is_active());
  ASSERT_TRUE(!profiler_is_active());

  // Start the Base Profiler.
  baseprofiler::profiler_start(
      PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
      ProfilerFeature::StackWalk, filters, MOZ_ARRAY_LENGTH(filters));

  ASSERT_TRUE(baseprofiler::profiler_is_active());
  ASSERT_TRUE(!profiler_is_active());

  // Add at least a marker, which should go straight into the buffer.
  Maybe<baseprofiler::ProfilerBufferInfo> info0 =
      baseprofiler::profiler_get_buffer_info();
  BASE_PROFILER_MARKER_UNTYPED("Marker from base profiler", OTHER, {});
  Maybe<baseprofiler::ProfilerBufferInfo> info1 =
      baseprofiler::profiler_get_buffer_info();
  ASSERT_GT(info1->mRangeEnd, info0->mRangeEnd);

  // Start the Gecko Profiler, which should grab the Base Profiler profile and
  // stop it.
  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                 ProfilerFeature::StackWalk, filters, MOZ_ARRAY_LENGTH(filters),
                 0);

  ASSERT_TRUE(!baseprofiler::profiler_is_active());
  ASSERT_TRUE(profiler_is_active());

  // Write some Gecko Profiler samples.
  ASSERT_EQ(WaitForSamplingState(), SamplingState::SamplingCompleted);

  // Check that the Gecko Profiler profile contains at least the Base Profiler
  // main thread samples.
  UniquePtr<char[]> profile = profiler_get_profile();
  JSONOutputCheck(profile.get(), [](const Json::Value& aRoot) {
    GET_JSON(threads, aRoot["threads"], Array);
    {
      bool found = false;
      for (const Json::Value& thread : threads) {
        ASSERT_TRUE(thread.isObject());
        GET_JSON(name, thread["name"], String);
        if (name.asString() == "GeckoMain (pre-xul)") {
          found = true;
          EXPECT_JSON_ARRAY_CONTAINS(thread["stringTable"], String,
                                     "Marker from base profiler");
          break;
        }
      }
      EXPECT_TRUE(found);
    }
  });

  profiler_stop();
  ASSERT_TRUE(!profiler_is_active());
}

TEST(GeckoProfiler, CPUUsage)
{
  const char* filters[] = {"GeckoMain"};

  // We want to ensure that CPU usage numbers are present whether or not we are
  // collecting stack samples.
  static constexpr bool scTestsWithOrWithoutStackSampling[] = {false, true};
  for (const bool testWithNoStackSampling : scTestsWithOrWithoutStackSampling) {
    ASSERT_TRUE(!profiler_is_active());
    ASSERT_TRUE(!profiler_callback_after_sampling(
        [&](SamplingState) { ASSERT_TRUE(false); }));

    profiler_start(
        PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
        ProfilerFeature::StackWalk | ProfilerFeature::CPUUtilization |
            (testWithNoStackSampling ? ProfilerFeature::NoStackSampling : 0),
        filters, MOZ_ARRAY_LENGTH(filters), 0);
    // Grab a few samples, each with a different label on the stack.
#define SAMPLE_LABEL_PREFIX "CPUUsage sample label "
    static constexpr const char* scSampleLabels[] = {
        SAMPLE_LABEL_PREFIX "0", SAMPLE_LABEL_PREFIX "1",
        SAMPLE_LABEL_PREFIX "2", SAMPLE_LABEL_PREFIX "3",
        SAMPLE_LABEL_PREFIX "4", SAMPLE_LABEL_PREFIX "5",
        SAMPLE_LABEL_PREFIX "6", SAMPLE_LABEL_PREFIX "7",
        SAMPLE_LABEL_PREFIX "8", SAMPLE_LABEL_PREFIX "9"};
    static constexpr size_t scSampleLabelCount =
        (sizeof(scSampleLabels) / sizeof(scSampleLabels[0]));
    // We'll do two samplings for each label.
    static constexpr size_t scMinSamplings = scSampleLabelCount * 2;

    for (const char* sampleLabel : scSampleLabels) {
      AUTO_PROFILER_LABEL(sampleLabel, OTHER);
      ASSERT_EQ(WaitForSamplingState(), SamplingState::SamplingCompleted);
      // Note: There could have been a delay before this label above, where the
      // profiler could have sampled the stack and missed the label. By forcing
      // another sampling now, the label is guaranteed to be present.
      ASSERT_EQ(WaitForSamplingState(), SamplingState::SamplingCompleted);
    }

    UniquePtr<char[]> profile = profiler_get_profile();

    if (testWithNoStackSampling) {
      // If we are testing nostacksampling, we shouldn't find this label prefix
      // in the profile.
      EXPECT_FALSE(strstr(profile.get(), SAMPLE_LABEL_PREFIX));
    } else {
      // In normal sampling mode, we should find all labels.
      for (const char* sampleLabel : scSampleLabels) {
        EXPECT_TRUE(strstr(profile.get(), sampleLabel));
      }
    }

    JSONOutputCheck(profile.get(), [testWithNoStackSampling](
                                       const Json::Value& aRoot) {
      // Check that the "cpu" feature is present.
      GET_JSON(meta, aRoot["meta"], Object);
      {
        GET_JSON(configuration, meta["configuration"], Object);
        {
          GET_JSON(features, configuration["features"], Array);
          { EXPECT_JSON_ARRAY_CONTAINS(features, String, "cpu"); }
        }
      }

      {
        GET_JSON(sampleUnits, meta["sampleUnits"], Object);
        {
          EXPECT_EQ_JSON(sampleUnits["time"], String, "ms");
          EXPECT_EQ_JSON(sampleUnits["eventDelay"], String, "ms");
#if defined(GP_OS_windows) || defined(GP_OS_darwin) || defined(GP_OS_linux) || \
    defined(GP_OS_android) || defined(GP_OS_freebsd)
          // Note: The exact string is not important here.
          EXPECT_TRUE(sampleUnits["threadCPUDelta"].isString())
              << "There should be a sampleUnits.threadCPUDelta on this "
                 "platform";
#else
        EXPECT_FALSE(sampleUnits.isMember("threadCPUDelta"))
            << "Unexpected sampleUnits.threadCPUDelta on this platform";;
#endif
        }
      }

      // Check that the sample schema contains "threadCPUDelta".
      GET_JSON(threads, aRoot["threads"], Array);
      {
        GET_JSON(thread0, threads[0], Object);
        {
          GET_JSON(samples, thread0["samples"], Object);
          {
            Json::ArrayIndex stackIndex = 0;
            Json::ArrayIndex threadCPUDeltaIndex = 0;
            GET_JSON(schema, samples["schema"], Object);
            {
              GET_JSON(jsonStackIndex, schema["stack"], UInt);
              stackIndex = jsonStackIndex.asUInt();
              GET_JSON(jsonThreadCPUDeltaIndex, schema["threadCPUDelta"], UInt);
              threadCPUDeltaIndex = jsonThreadCPUDeltaIndex.asUInt();
            }

            std::set<uint64_t> stackLeaves;  // To count distinct leaves.
            unsigned threadCPUDeltaCount = 0;
            GET_JSON(data, samples["data"], Array);
            if (testWithNoStackSampling) {
              // When not sampling stacks, the first sampling loop will have no
              // running times, so it won't output anything.
              EXPECT_GE(data.size(), scMinSamplings - 1);
            } else {
              EXPECT_GE(data.size(), scMinSamplings);
            }
            for (const Json::Value& sample : data) {
              ASSERT_TRUE(sample.isArray());
              if (sample.isValidIndex(stackIndex)) {
                if (!sample[stackIndex].isNull()) {
                  GET_JSON(stack, sample[stackIndex], UInt64);
                  stackLeaves.insert(stack.asUInt64());
                }
              }
              if (sample.isValidIndex(threadCPUDeltaIndex)) {
                if (!sample[threadCPUDeltaIndex].isNull()) {
                  EXPECT_TRUE(sample[threadCPUDeltaIndex].isUInt64());
                  ++threadCPUDeltaCount;
                }
              }
            }

            if (testWithNoStackSampling) {
              // in nostacksampling mode, there should only be one kind of stack
              // leaf (the root).
              EXPECT_EQ(stackLeaves.size(), 1u);
            } else {
              // in normal sampling mode, there should be at least one kind of
              // stack leaf for each distinct label.
              EXPECT_GE(stackLeaves.size(), scSampleLabelCount);
            }

#if defined(GP_OS_windows) || defined(GP_OS_darwin) || defined(GP_OS_linux) || \
    defined(GP_OS_android) || defined(GP_OS_freebsd)
            EXPECT_GE(threadCPUDeltaCount, data.size() - 1u)
                << "There should be 'threadCPUDelta' values in all but 1 "
                   "samples";
#else
          // All "threadCPUDelta" data should be absent or null on unsupported
          // platforms.
          EXPECT_EQ(threadCPUDeltaCount, 0u);
#endif
          }
        }
      }
    });

    // Note: There is no non-racy way to test for SamplingState::JustStopped, as
    // it would require coordination between `profiler_stop()` and another
    // thread doing `profiler_callback_after_sampling()` at just the right
    // moment.

    profiler_stop();
    ASSERT_TRUE(!profiler_is_active());
    ASSERT_TRUE(!profiler_callback_after_sampling(
        [&](SamplingState) { ASSERT_TRUE(false); }));
  }
}
