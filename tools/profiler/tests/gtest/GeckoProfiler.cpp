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
#include "platform.h"
#include "ProfileBuffer.h"
#include "ProfilerMarkerPayload.h"

#include "js/Initialization.h"
#include "js/Printf.h"
#include "jsapi.h"
#include "json/json.h"
#include "mozilla/Atomics.h"
#include "mozilla/BlocksRingBuffer.h"
#include "mozilla/ProfileBufferEntrySerializationGeckoExtensions.h"
#include "mozilla/ProfileJSONWriter.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/net/HttpBaseChannel.h"
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

typedef Vector<const char*> StrVec;

static void InactiveFeaturesAndParamsCheck() {
  int entries;
  Maybe<double> duration;
  double interval;
  uint32_t features;
  StrVec filters;
  uint64_t activeBrowsingContextID;

  ASSERT_TRUE(!profiler_is_active());
  ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::MainThreadIO));
  ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::NativeAllocations));

  profiler_get_start_params(&entries, &duration, &interval, &features, &filters,
                            &activeBrowsingContextID);

  ASSERT_TRUE(entries == 0);
  ASSERT_TRUE(duration == Nothing());
  ASSERT_TRUE(interval == 0);
  ASSERT_TRUE(features == 0);
  ASSERT_TRUE(filters.empty());
  ASSERT_TRUE(activeBrowsingContextID == 0);
}

static void ActiveParamsCheck(int aEntries, double aInterval,
                              uint32_t aFeatures, const char** aFilters,
                              size_t aFiltersLen,
                              uint64_t aActiveBrowsingContextID,
                              const Maybe<double>& aDuration = Nothing()) {
  int entries;
  Maybe<double> duration;
  double interval;
  uint32_t features;
  StrVec filters;
  uint64_t activeBrowsingContextID;

  profiler_get_start_params(&entries, &duration, &interval, &features, &filters,
                            &activeBrowsingContextID);

  ASSERT_TRUE(entries == aEntries);
  ASSERT_TRUE(duration == aDuration);
  ASSERT_TRUE(interval == aInterval);
  ASSERT_TRUE(features == aFeatures);
  ASSERT_TRUE(filters.length() == aFiltersLen);
  ASSERT_TRUE(activeBrowsingContextID == aActiveBrowsingContextID);
  for (size_t i = 0; i < aFiltersLen; i++) {
    ASSERT_TRUE(strcmp(filters[i], aFilters[i]) == 0);
  }
}

TEST(GeckoProfiler, Utilities)
{
  // We'll assume that this test runs in the main thread (which should be true
  // when called from the `main` function).
  const int mainThreadId = profiler_current_thread_id();

  MOZ_RELEASE_ASSERT(profiler_main_thread_id() == mainThreadId);
  MOZ_RELEASE_ASSERT(profiler_is_main_thread());

  std::thread testThread([&]() {
    const int testThreadId = profiler_current_thread_id();
    MOZ_RELEASE_ASSERT(testThreadId != mainThreadId);

    MOZ_RELEASE_ASSERT(profiler_main_thread_id() != testThreadId);
    MOZ_RELEASE_ASSERT(!profiler_is_main_thread());
  });
  testThread.join();
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

// A class that keeps track of how many instances have been created, streamed,
// and destroyed.
class GTestMarkerPayload : public ProfilerMarkerPayload {
 public:
  explicit GTestMarkerPayload(int aN) : mN(aN) { ++sNumCreated; }

  virtual ~GTestMarkerPayload() { ++sNumDestroyed; }

  DECL_STREAM_PAYLOAD

 private:
  GTestMarkerPayload(CommonProps&& aCommonProps, int aN)
      : ProfilerMarkerPayload(std::move(aCommonProps)), mN(aN) {
    ++sNumDeserialized;
  }

  int mN;

 public:
  // The number of GTestMarkerPayload instances that have been created,
  // streamed, and destroyed.
  static int sNumCreated;
  static int sNumSerialized;
  static int sNumDeserialized;
  static int sNumStreamed;
  static int sNumDestroyed;
};

int GTestMarkerPayload::sNumCreated = 0;
int GTestMarkerPayload::sNumSerialized = 0;
int GTestMarkerPayload::sNumDeserialized = 0;
int GTestMarkerPayload::sNumStreamed = 0;
int GTestMarkerPayload::sNumDestroyed = 0;

ProfileBufferEntryWriter::Length GTestMarkerPayload::TagAndSerializationBytes()
    const {
  return CommonPropsTagAndSerializationBytes() +
         ProfileBufferEntryWriter::SumBytes(mN);
}

void GTestMarkerPayload::SerializeTagAndPayload(
    ProfileBufferEntryWriter& aEntryWriter) const {
  static const DeserializerTag tag = TagForDeserializer(Deserialize);
  SerializeTagAndCommonProps(tag, aEntryWriter);
  aEntryWriter.WriteObject(mN);
  ++sNumSerialized;
}

// static
UniquePtr<ProfilerMarkerPayload> GTestMarkerPayload::Deserialize(
    ProfileBufferEntryReader& aEntryReader) {
  ProfilerMarkerPayload::CommonProps props =
      DeserializeCommonProps(aEntryReader);
  auto n = aEntryReader.ReadObject<int>();
  return UniquePtr<ProfilerMarkerPayload>(
      new GTestMarkerPayload(std::move(props), n));
}

void GTestMarkerPayload::StreamPayload(SpliceableJSONWriter& aWriter,
                                       const mozilla::TimeStamp& aStartTime,
                                       UniqueStacks& aUniqueStacks) const {
  StreamCommonProps("gtest", aWriter, aStartTime, aUniqueStacks);
  char buf[64];
  int written = SprintfLiteral(buf, "gtest-%d", mN);
  ASSERT_GT(written, 0);
  aWriter.IntProperty(mozilla::Span<const char>(buf, size_t(written)), mN);
  ++sNumStreamed;
}

TEST(GeckoProfiler, Markers)
{
  uint32_t features = ProfilerFeature::StackWalk;
  const char* filters[] = {"GeckoMain"};

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL, features,
                 filters, MOZ_ARRAY_LENGTH(filters), 0);

  // Used in markers below.
  TimeStamp ts0 = TimeStamp::NowUnfuzzed();

  profiler_tracing_marker("A", "tracing event",
                          JS::ProfilingCategoryPair::OTHER, TRACING_EVENT);
  PROFILER_TRACING_MARKER("A", "tracing start", OTHER, TRACING_INTERVAL_START);
  PROFILER_TRACING_MARKER("A", "tracing end", OTHER, TRACING_INTERVAL_END);

  UniqueProfilerBacktrace bt = profiler_get_backtrace();
  profiler_tracing_marker("B", "tracing event with stack",
                          JS::ProfilingCategoryPair::OTHER, TRACING_EVENT,
                          std::move(bt));

  { AUTO_PROFILER_TRACING_MARKER("C", "auto tracing", OTHER); }

  PROFILER_MARKER_UNTYPED("M1", OTHER, {});
  PROFILER_ADD_MARKER_WITH_PAYLOAD("M2", OTHER, TracingMarkerPayload,
                                   ("C", TRACING_EVENT, ts0));
  PROFILER_MARKER_UNTYPED("M3", OTHER, {});
  PROFILER_ADD_MARKER_WITH_PAYLOAD(
      "M4", OTHER, TracingMarkerPayload,
      ("C", TRACING_EVENT, ts0, mozilla::Nothing(), profiler_get_backtrace()));

  for (int i = 0; i < 10; i++) {
    PROFILER_ADD_MARKER_WITH_PAYLOAD("M5", OTHER, GTestMarkerPayload, (i));
  }
  // The GTestMarkerPayloads should have been created, serialized, and
  // destroyed.
  EXPECT_EQ(GTestMarkerPayload::sNumCreated, 10);
  EXPECT_EQ(GTestMarkerPayload::sNumSerialized, 10);
  EXPECT_EQ(GTestMarkerPayload::sNumDeserialized, 0);
  EXPECT_EQ(GTestMarkerPayload::sNumStreamed, 0);
  EXPECT_EQ(GTestMarkerPayload::sNumDestroyed, 10);

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
  MOZ_RELEASE_ASSERT(
      profiler_add_marker("default-templated markers 2.0 with empty options",
                          geckoprofiler::category::OTHER, {}));

  PROFILER_MARKER_UNTYPED(
      "default-templated markers 2.0 with option", OTHER,
      MarkerStack::TakeBacktrace(profiler_capture_backtrace()));

  PROFILER_MARKER("explicitly-default-templated markers 2.0 with empty options",
                  OTHER, {}, NoPayload);

  MOZ_RELEASE_ASSERT(profiler_add_marker(
      "explicitly-default-templated markers 2.0 with option",
      geckoprofiler::category::OTHER, {},
      ::geckoprofiler::markers::NoPayload{}));

  // Used in markers below.
  TimeStamp ts1 = TimeStamp::NowUnfuzzed();

  // Sleep briefly to ensure a sample is taken and the pending markers are
  // processed.
  PR_Sleep(PR_MillisecondsToInterval(500));

  // Used in markers below.
  TimeStamp ts2 = TimeStamp::NowUnfuzzed();
  // ts1 and ts2 should be different thanks to the sleep.
  EXPECT_NE(ts1, ts2);

  // Test most marker payloads.

  // Keep this one first! (It's used to record `ts1` and `ts2`, to compare
  // to serialized numbers in other markers.)
  MOZ_RELEASE_ASSERT(
      profiler_add_marker("FirstMarker", geckoprofiler::category::OTHER,
                          MarkerTiming::Interval(ts1, ts2),
                          geckoprofiler::markers::Text{}, "FirstMarker"));

  // Other markers in alphabetical order of payload class names.

  {
    const char gcMajorJSON[] = "42";
    const auto len = strlen(gcMajorJSON);
    char* buffer =
        static_cast<char*>(js::SystemAllocPolicy{}.pod_malloc<char>(len + 1));
    strncpy(buffer, gcMajorJSON, len);
    buffer[len] = '\0';
    PROFILER_ADD_MARKER_WITH_PAYLOAD("GCMajorMarkerPayload marker", OTHER,
                                     GCMajorMarkerPayload,
                                     (ts1, ts2, JS::UniqueChars(buffer)));
  }

  {
    const char gcMinorJSON[] = "43";
    const auto len = strlen(gcMinorJSON);
    char* buffer =
        static_cast<char*>(js::SystemAllocPolicy{}.pod_malloc<char>(len + 1));
    strncpy(buffer, gcMinorJSON, len);
    buffer[len] = '\0';
    PROFILER_ADD_MARKER_WITH_PAYLOAD("GCMinorMarkerPayload marker", OTHER,
                                     GCMinorMarkerPayload,
                                     (ts1, ts2, JS::UniqueChars(buffer)));
  }

  {
    const char gcSliceJSON[] = "44";
    const auto len = strlen(gcSliceJSON);
    char* buffer =
        static_cast<char*>(js::SystemAllocPolicy{}.pod_malloc<char>(len + 1));
    strncpy(buffer, gcSliceJSON, len);
    buffer[len] = '\0';
    PROFILER_ADD_MARKER_WITH_PAYLOAD("GCSliceMarkerPayload marker", OTHER,
                                     GCSliceMarkerPayload,
                                     (ts1, ts2, JS::UniqueChars(buffer)));
  }

  PROFILER_ADD_MARKER_WITH_PAYLOAD("NativeAllocationMarkerPayload marker",
                                   OTHER, NativeAllocationMarkerPayload,
                                   (ts1, 9876543210, 1234, 5678, nullptr));

  nsCOMPtr<nsIURI> uri;
  ASSERT_TRUE(
      NS_SUCCEEDED(NS_NewURI(getter_AddRefs(uri), "http://mozilla.org/"_ns)));
  // The marker name will be "Load <aChannelId>: <aURI>".
  profiler_add_network_marker(
      /* nsIURI* aURI */ uri,
      /* const nsACString& aRequestMethod */ "GET"_ns,
      /* int32_t aPriority */ 34,
      /* uint64_t aChannelId */ 1,
      /* NetworkLoadType aType */ NetworkLoadType::LOAD_START,
      /* mozilla::TimeStamp aStart */ ts1,
      /* mozilla::TimeStamp aEnd */ ts2,
      /* int64_t aCount */ 56,
      /* mozilla::net::CacheDisposition aCacheDisposition */
      net::kCacheHit,
      /* uint64_t aInnerWindowID */ 78
      /* const mozilla::net::TimingStruct* aTimings = nullptr */
      /* nsIURI* aRedirectURI = nullptr */
      /* mozilla::UniquePtr<mozilla::ProfileChunkedBuffer> aSource =
         nullptr */
      /* const mozilla::Maybe<nsDependentCString>& aContentType =
         mozilla::Nothing() */);

  profiler_add_network_marker(
      /* nsIURI* aURI */ uri,
      /* const nsACString& aRequestMethod */ "GET"_ns,
      /* int32_t aPriority */ 34,
      /* uint64_t aChannelId */ 12,
      /* NetworkLoadType aType */ NetworkLoadType::LOAD_STOP,
      /* mozilla::TimeStamp aStart */ ts1,
      /* mozilla::TimeStamp aEnd */ ts2,
      /* int64_t aCount */ 56,
      /* mozilla::net::CacheDisposition aCacheDisposition */
      net::kCacheUnresolved,
      /* uint64_t aInnerWindowID */ 78,
      /* const mozilla::net::TimingStruct* aTimings = nullptr */ nullptr,
      /* nsIURI* aRedirectURI = nullptr */ nullptr,
      /* mozilla::UniquePtr<mozilla::ProfileChunkedBuffer> aSource =
         nullptr */
      nullptr,
      /* const mozilla::Maybe<nsDependentCString>& aContentType =
         mozilla::Nothing() */
      Some(nsDependentCString("text/html")));

  nsCOMPtr<nsIURI> redirectURI;
  ASSERT_TRUE(NS_SUCCEEDED(
      NS_NewURI(getter_AddRefs(redirectURI), "http://example.com/"_ns)));
  profiler_add_network_marker(
      /* nsIURI* aURI */ uri,
      /* const nsACString& aRequestMethod */ "GET"_ns,
      /* int32_t aPriority */ 34,
      /* uint64_t aChannelId */ 123,
      /* NetworkLoadType aType */ NetworkLoadType::LOAD_REDIRECT,
      /* mozilla::TimeStamp aStart */ ts1,
      /* mozilla::TimeStamp aEnd */ ts2,
      /* int64_t aCount */ 56,
      /* mozilla::net::CacheDisposition aCacheDisposition */
      net::kCacheUnresolved,
      /* uint64_t aInnerWindowID */ 78,
      /* const mozilla::net::TimingStruct* aTimings = nullptr */ nullptr,
      /* nsIURI* aRedirectURI = nullptr */ redirectURI
      /* mozilla::UniquePtr<mozilla::ProfileChunkedBuffer> aSource =
         nullptr */
      /* const mozilla::Maybe<nsDependentCString>& aContentType =
         mozilla::Nothing() */);

  nsCString screenshotURL = "url"_ns;
  PROFILER_ADD_MARKER_WITH_PAYLOAD(
      "ScreenshotPayload marker", OTHER, ScreenshotPayload,
      (ts1, std::move(screenshotURL), mozilla::gfx::IntSize(12, 34),
       uintptr_t(0x45678u)));

  PROFILER_ADD_MARKER_WITH_PAYLOAD("TextMarkerPayload marker 1", OTHER,
                                   TextMarkerPayload, ("text"_ns, ts1));

  PROFILER_ADD_MARKER_WITH_PAYLOAD("TextMarkerPayload marker 2", OTHER,
                                   TextMarkerPayload, ("text"_ns, ts1, ts2));

  PROFILER_ADD_MARKER_WITH_PAYLOAD("UserTimingMarkerPayload marker mark", OTHER,
                                   UserTimingMarkerPayload,
                                   (u"mark name"_ns, ts1, mozilla::Nothing()));

  PROFILER_ADD_MARKER_WITH_PAYLOAD(
      "UserTimingMarkerPayload marker measure", OTHER, UserTimingMarkerPayload,
      (u"measure name"_ns, Some(u"start mark"_ns), Some(u"end mark"_ns), ts1,
       ts2, mozilla::Nothing()));

  PROFILER_ADD_MARKER_WITH_PAYLOAD(
      "IPCMarkerPayload marker", IPC, IPCMarkerPayload,
      (1111, 1, 3 /* PAPZ::Msg_LayerTransforms */, mozilla::ipc::ParentSide,
       mozilla::ipc::MessageDirection::eSending,
       mozilla::ipc::MessagePhase::Endpoint, false, ts1));

  MOZ_RELEASE_ASSERT(profiler_add_marker(
      "Text in main thread with stack", geckoprofiler::category::OTHER,
      MarkerStack::Capture(), geckoprofiler::markers::Text{}, ""));
  MOZ_RELEASE_ASSERT(profiler_add_marker(
      "Text from main thread with stack", geckoprofiler::category::OTHER,
      MarkerOptions(MarkerThreadId::MainThread(), MarkerStack::Capture()),
      geckoprofiler::markers::Text{}, ""));

  std::thread registeredThread([]() {
    AUTO_PROFILER_REGISTER_THREAD("Marker test sub-thread");
    // Marker in non-profiled thread won't be stored.
    MOZ_RELEASE_ASSERT(profiler_add_marker(
        "Text in registered thread with stack", geckoprofiler::category::OTHER,
        MarkerStack::Capture(), geckoprofiler::markers::Text{}, ""));
    // Marker will be stored in main thread, with stack from registered thread.
    MOZ_RELEASE_ASSERT(profiler_add_marker(
        "Text from registered thread with stack",
        geckoprofiler::category::OTHER,
        MarkerOptions(MarkerThreadId::MainThread(), MarkerStack::Capture()),
        geckoprofiler::markers::Text{}, ""));
  });
  registeredThread.join();

  std::thread unregisteredThread([]() {
    // Marker in unregistered thread won't be stored.
    MOZ_RELEASE_ASSERT(profiler_add_marker(
        "Text in unregistered thread with stack",
        geckoprofiler::category::OTHER, MarkerStack::Capture(),
        geckoprofiler::markers::Text{}, ""));
    // Marker will be stored in main thread, but stack cannot be captured in an
    // unregistered thread.
    MOZ_RELEASE_ASSERT(profiler_add_marker(
        "Text from unregistered thread with stack",
        geckoprofiler::category::OTHER,
        MarkerOptions(MarkerThreadId::MainThread(), MarkerStack::Capture()),
        geckoprofiler::markers::Text{}, ""));
  });
  unregisteredThread.join();

  MOZ_RELEASE_ASSERT(
      profiler_add_marker("Tracing", geckoprofiler::category::OTHER, {},
                          geckoprofiler::markers::Tracing{}, "category"));

  MOZ_RELEASE_ASSERT(profiler_add_marker(
      "UserTimingMark", geckoprofiler::category::OTHER, {},
      geckoprofiler::markers::UserTimingMark{}, "mark name"));

  MOZ_RELEASE_ASSERT(profiler_add_marker(
      "UserTimingMeasure", geckoprofiler::category::OTHER, {},
      geckoprofiler::markers::UserTimingMeasure{}, "measure name",
      Some(mozilla::ProfilerString8View("start")),
      Some(mozilla::ProfilerString8View("end"))));

  MOZ_RELEASE_ASSERT(profiler_add_marker("Text", geckoprofiler::category::OTHER,
                                         {}, geckoprofiler::markers::Text{},
                                         "Text text"));

  MOZ_RELEASE_ASSERT(profiler_add_marker(
      "MediaSample", geckoprofiler::category::OTHER, {},
      geckoprofiler::markers::MediaSampleMarker{}, 123, 456));

  SpliceableChunkedJSONWriter w;
  w.Start();
  EXPECT_TRUE(::profiler_stream_json_for_this_process(w));
  w.End();

  // The GTestMarkerPayloads should have been deserialized, streamed, and
  // destroyed.
  EXPECT_EQ(GTestMarkerPayload::sNumCreated, 10 + 0);
  EXPECT_EQ(GTestMarkerPayload::sNumSerialized, 10 + 0);
  EXPECT_EQ(GTestMarkerPayload::sNumDeserialized, 0 + 10);
  EXPECT_EQ(GTestMarkerPayload::sNumStreamed, 0 + 10);
  EXPECT_EQ(GTestMarkerPayload::sNumDestroyed, 10 + 10);

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
    S_tracing_M2_C,
    S_M3,
    S_tracing_M4_C_stack,
    S_M5_gtest0,
    S_M5_gtest1,
    S_M5_gtest2,
    S_M5_gtest3,
    S_M5_gtest4,
    S_M5_gtest5,
    S_M5_gtest6,
    S_M5_gtest7,
    S_M5_gtest8,
    S_M5_gtest9,
    S_Markers2DefaultEmptyOptions,
    S_Markers2DefaultWithOptions,
    S_Markers2ExplicitDefaultEmptyOptions,
    S_Markers2ExplicitDefaultWithOptions,
    S_FirstMarker,
    S_GCMajorMarkerPayload,
    S_GCMinorMarkerPayload,
    S_GCSliceMarkerPayload,
    S_NativeAllocationMarkerPayload,
    S_NetworkMarkerPayload_start,
    S_NetworkMarkerPayload_stop,
    S_NetworkMarkerPayload_redirect,
    S_ScreenshotPayload,
    S_TextMarkerPayload1,
    S_TextMarkerPayload2,
    S_UserTimingMarkerPayload_mark,
    S_UserTimingMarkerPayload_measure,
    S_IPCMarkerPayload,
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

  // Extract JSON.
  Json::Value parsedRoot;
  Json::CharReaderBuilder builder;
  const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
  ASSERT_TRUE(reader->parse(profile.get(), strchr(profile.get(), '\0'),
                            &parsedRoot, nullptr));

  // Use const root, we only want to test what's in the profile, not change it.
  const Json::Value& root = parsedRoot;
  ASSERT_TRUE(root.isObject());

  // We have a root object.

  // Most common test: Checks that the given value is present, is of the
  // expected type, and has the expected value.
#define EXPECT_EQ_JSON(GETTER, TYPE, VALUE)  \
  if ((GETTER).isNull()) {                   \
    EXPECT_FALSE((GETTER).isNull());         \
  } else if (!(GETTER).is##TYPE()) {         \
    EXPECT_TRUE((GETTER).is##TYPE());        \
  } else {                                   \
    EXPECT_EQ((GETTER).as##TYPE(), (VALUE)); \
  }

  // Checks that the given value is present, and is a valid index into the
  // stringTable, pointing at the expected string.
#define EXPECT_EQ_STRINGTABLE(GETTER, STRING)                         \
  if ((GETTER).isNull()) {                                            \
    EXPECT_FALSE((GETTER).isNull());                                  \
  } else if (!(GETTER).isUInt()) {                                    \
    EXPECT_TRUE((GETTER).isUInt());                                   \
  } else {                                                            \
    EXPECT_LT((GETTER).asUInt(), stringTable.size());                 \
    EXPECT_EQ_JSON(stringTable[(GETTER).asUInt()], String, (STRING)); \
  }

  {
    const Json::Value& threads = root["threads"];
    ASSERT_TRUE(!threads.isNull());
    ASSERT_TRUE(threads.isArray());
    ASSERT_EQ(threads.size(), 1u);

    // root.threads is a 1-element array.

    {
      const Json::Value& thread0 = threads[0];
      ASSERT_TRUE(thread0.isObject());

      // root.threads[0] is an object.

      // Keep a reference to the string table in this block, it will be used
      // below.
      const Json::Value& stringTable = thread0["stringTable"];
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
        const Json::Value& markers = thread0["markers"];
        ASSERT_TRUE(markers.isObject());

        // root.threads[0].markers is an object.

        {
          const Json::Value& data = markers["data"];
          ASSERT_TRUE(data.isArray());

          // root.threads[0].markers.data is an array.

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

            // root.threads[0].markers.data[i] is an array with 5 or 6 elements.

            ASSERT_TRUE(marker[NAME].isUInt());  // name id
            const Json::Value& name = stringTable[marker[NAME].asUInt()];
            ASSERT_TRUE(name.isString());
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
                         "explicitly-default-templated markers 2.0 with empty "
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
              const Json::Value& payload = marker[PAYLOAD];
              ASSERT_TRUE(payload.isObject());

              // root.threads[0].markers.data[i][PAYLOAD] is an object
              // (payload).

              // It should at least have a "type" string.
              const Json::Value& type = payload["type"];
              ASSERT_TRUE(type.isString());
              std::string typeString = type.asString();

              if (nameString == "tracing event") {
                EXPECT_EQ(state, S_tracing_event);
                state = State(S_tracing_event + 1);
                EXPECT_EQ(typeString, "tracing");
                EXPECT_TIMING_INSTANT;
                EXPECT_EQ_JSON(payload["category"], String, "A");
                EXPECT_TRUE(payload["interval"].isNull());
                EXPECT_TRUE(payload["stack"].isNull());

              } else if (nameString == "tracing start") {
                EXPECT_EQ(state, S_tracing_start);
                state = State(S_tracing_start + 1);
                EXPECT_EQ(typeString, "tracing");
                EXPECT_TIMING_START;
                EXPECT_EQ_JSON(payload["category"], String, "A");
                EXPECT_EQ_JSON(payload["interval"], String, "start");
                EXPECT_TRUE(payload["stack"].isNull());

              } else if (nameString == "tracing end") {
                EXPECT_EQ(state, S_tracing_end);
                state = State(S_tracing_end + 1);
                EXPECT_EQ(typeString, "tracing");
                EXPECT_TIMING_END;
                EXPECT_EQ_JSON(payload["category"], String, "A");
                EXPECT_EQ_JSON(payload["interval"], String, "end");
                EXPECT_TRUE(payload["stack"].isNull());

              } else if (nameString == "tracing event with stack") {
                EXPECT_EQ(state, S_tracing_event_with_stack);
                state = State(S_tracing_event_with_stack + 1);
                EXPECT_EQ(typeString, "tracing");
                EXPECT_TIMING_INSTANT;
                EXPECT_EQ_JSON(payload["category"], String, "B");
                EXPECT_TRUE(payload["interval"].isNull());
                EXPECT_TRUE(payload["stack"].isObject());

              } else if (nameString == "auto tracing") {
                switch (state) {
                  case S_tracing_auto_tracing_start:
                    state = State(S_tracing_auto_tracing_start + 1);
                    EXPECT_EQ(typeString, "tracing");
                    EXPECT_TIMING_START;
                    EXPECT_EQ_JSON(payload["category"], String, "C");
                    EXPECT_EQ_JSON(payload["interval"], String, "start");
                    EXPECT_TRUE(payload["stack"].isNull());
                    break;
                  case S_tracing_auto_tracing_end:
                    state = State(S_tracing_auto_tracing_end + 1);
                    EXPECT_EQ(typeString, "tracing");
                    EXPECT_TIMING_END;
                    EXPECT_EQ_JSON(payload["category"], String, "C");
                    EXPECT_EQ_JSON(payload["interval"], String, "end");
                    ASSERT_TRUE(payload["stack"].isNull());
                    break;
                  default:
                    EXPECT_TRUE(state == S_tracing_auto_tracing_start ||
                                state == S_tracing_auto_tracing_end);
                    break;
                }

              } else if (nameString == "M2") {
                EXPECT_EQ(state, S_tracing_M2_C);
                state = State(S_tracing_M2_C + 1);
                EXPECT_EQ(typeString, "tracing");
                EXPECT_TIMING_INSTANT;
                EXPECT_EQ_JSON(payload["category"], String, "C");
                EXPECT_TRUE(payload["interval"].isNull());
                EXPECT_TRUE(payload["stack"].isNull());

              } else if (nameString == "M4") {
                EXPECT_EQ(state, S_tracing_M4_C_stack);
                state = State(S_tracing_M4_C_stack + 1);
                EXPECT_EQ(typeString, "tracing");
                EXPECT_TIMING_INSTANT;
                EXPECT_EQ_JSON(payload["category"], String, "C");
                EXPECT_TRUE(payload["interval"].isNull());
                EXPECT_TRUE(payload["stack"].isObject());

              } else if (nameString == "M5") {
                EXPECT_EQ(typeString, "gtest");
                // It should only have one more element (apart from "type").
                ASSERT_EQ(payload.size(), 2u);
                const auto itEnd = payload.end();
                for (auto it = payload.begin(); it != itEnd; ++it) {
                  std::string key = it.name();
                  if (key != "type") {
                    const Json::Value& value = *it;
                    ASSERT_TRUE(value.isInt());
                    int valueInt = value.asInt();
                    // We expect `"gtest-<i>" : <i>`.
                    EXPECT_EQ(state, State(S_M5_gtest0 + valueInt));
                    state = State(state + 1);
                    EXPECT_EQ(key,
                              std::string("gtest-") + std::to_string(valueInt));
                  }
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
              } else if (nameString == "GCMajorMarkerPayload marker") {
                EXPECT_EQ(state, S_GCMajorMarkerPayload);
                state = State(S_GCMajorMarkerPayload + 1);
                EXPECT_EQ(typeString, "GCMajor");
                EXPECT_TIMING_INTERVAL_AT(ts1Double, ts2Double);
                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["timings"], Int, 42);

              } else if (nameString == "GCMinorMarkerPayload marker") {
                EXPECT_EQ(state, S_GCMinorMarkerPayload);
                state = State(S_GCMinorMarkerPayload + 1);
                EXPECT_EQ(typeString, "GCMinor");
                EXPECT_TIMING_INTERVAL_AT(ts1Double, ts2Double);
                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["nursery"], Int, 43);

              } else if (nameString == "GCSliceMarkerPayload marker") {
                EXPECT_EQ(state, S_GCSliceMarkerPayload);
                state = State(S_GCSliceMarkerPayload + 1);
                EXPECT_EQ(typeString, "GCSlice");
                EXPECT_TIMING_INTERVAL_AT(ts1Double, ts2Double);
                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["timings"], Int, 44);

              } else if (nameString == "NativeAllocationMarkerPayload marker") {
                EXPECT_EQ(state, S_NativeAllocationMarkerPayload);
                state = State(S_NativeAllocationMarkerPayload + 1);
                EXPECT_EQ(typeString, "Native allocation");
                EXPECT_TIMING_INSTANT_AT(ts1Double);
                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["size"], Int64, 9876543210);
                EXPECT_EQ_JSON(payload["memoryAddress"], Int64, 1234);
                EXPECT_EQ_JSON(payload["threadId"], Int64, 5678);

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
                EXPECT_TRUE(payload["contentType"].isNull());

              } else if (nameString == "Load 12: http://mozilla.org/") {
                EXPECT_EQ(state, S_NetworkMarkerPayload_stop);
                state = State(S_NetworkMarkerPayload_stop + 1);
                EXPECT_EQ(typeString, "Network");
                EXPECT_EQ_JSON(payload["startTime"], Double, ts1Double);
                EXPECT_EQ_JSON(payload["endTime"], Double, ts2Double);
                EXPECT_EQ_JSON(payload["id"], Int64, 12);
                EXPECT_EQ_JSON(payload["URI"], String, "http://mozilla.org/");
                EXPECT_EQ_JSON(payload["requestMethod"], String, "GET");
                EXPECT_EQ_JSON(payload["pri"], Int64, 34);
                EXPECT_EQ_JSON(payload["count"], Int64, 56);
                EXPECT_EQ_JSON(payload["cache"], String, "Unresolved");
                EXPECT_TRUE(payload["RedirectURI"].isNull());
                EXPECT_EQ_JSON(payload["contentType"], String, "text/html");

              } else if (nameString == "Load 123: http://mozilla.org/") {
                EXPECT_EQ(state, S_NetworkMarkerPayload_redirect);
                state = State(S_NetworkMarkerPayload_redirect + 1);
                EXPECT_EQ(typeString, "Network");
                EXPECT_EQ_JSON(payload["startTime"], Double, ts1Double);
                EXPECT_EQ_JSON(payload["endTime"], Double, ts2Double);
                EXPECT_EQ_JSON(payload["id"], Int64, 123);
                EXPECT_EQ_JSON(payload["URI"], String, "http://mozilla.org/");
                EXPECT_EQ_JSON(payload["requestMethod"], String, "GET");
                EXPECT_EQ_JSON(payload["pri"], Int64, 34);
                EXPECT_EQ_JSON(payload["count"], Int64, 56);
                EXPECT_EQ_JSON(payload["cache"], String, "Unresolved");
                EXPECT_EQ_JSON(payload["RedirectURI"], String,
                               "http://example.com/");
                EXPECT_TRUE(payload["contentType"].isNull());

              } else if (nameString == "ScreenshotPayload marker") {
                EXPECT_EQ(state, S_ScreenshotPayload);
                state = State(S_ScreenshotPayload + 1);
                EXPECT_EQ(typeString, "CompositorScreenshot");
                EXPECT_EQ_STRINGTABLE(payload["url"], "url");
                EXPECT_EQ_JSON(payload["windowID"], String, "0x45678");
                EXPECT_EQ_JSON(payload["windowWidth"], Int, 12);
                EXPECT_EQ_JSON(payload["windowHeight"], Int, 34);

              } else if (nameString == "TextMarkerPayload marker 1") {
                EXPECT_EQ(state, S_TextMarkerPayload1);
                state = State(S_TextMarkerPayload1 + 1);
                EXPECT_EQ(typeString, "Text");
                EXPECT_TIMING_INSTANT_AT(ts1Double);
                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["name"], String, "text");

              } else if (nameString == "TextMarkerPayload marker 2") {
                EXPECT_EQ(state, S_TextMarkerPayload2);
                state = State(S_TextMarkerPayload2 + 1);
                EXPECT_EQ(typeString, "Text");
                EXPECT_TIMING_INTERVAL_AT(ts1Double, ts2Double);
                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["name"], String, "text");

              } else if (nameString == "UserTimingMarkerPayload marker mark") {
                EXPECT_EQ(state, S_UserTimingMarkerPayload_mark);
                state = State(S_UserTimingMarkerPayload_mark + 1);
                EXPECT_EQ(typeString, "UserTiming");
                EXPECT_TIMING_INSTANT_AT(ts1Double);
                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["name"], String, "mark name");
                EXPECT_EQ_JSON(payload["entryType"], String, "mark");

              } else if (nameString ==
                         "UserTimingMarkerPayload marker measure") {
                EXPECT_EQ(state, S_UserTimingMarkerPayload_measure);
                state = State(S_UserTimingMarkerPayload_measure + 1);
                EXPECT_EQ(typeString, "UserTiming");
                EXPECT_TIMING_INTERVAL_AT(ts1Double, ts2Double);
                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["name"], String, "measure name");
                EXPECT_EQ_JSON(payload["entryType"], String, "measure");
                EXPECT_EQ_JSON(payload["startMark"], String, "start mark");
                EXPECT_EQ_JSON(payload["endMark"], String, "end mark");

              } else if (nameString == "IPCMarkerPayload marker") {
                EXPECT_EQ(state, S_IPCMarkerPayload);
                state = State(S_IPCMarkerPayload + 1);
                EXPECT_EQ(typeString, "IPC");
                EXPECT_TIMING_INSTANT_AT(ts1Double);

                // The startTime and endTime are currently duplicated in the
                // payload.
                EXPECT_EQ_JSON(payload["startTime"], Double, ts1Double);
                EXPECT_EQ_JSON(payload["endTime"], Double, ts1Double);

                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["otherPid"], Int, 1111);
                EXPECT_EQ_JSON(payload["messageSeqno"], Int, 1);
                EXPECT_EQ_JSON(payload["messageType"], String,
                               "PAPZ::Msg_LayerTransforms");
                EXPECT_EQ_JSON(payload["side"], String, "parent");
                EXPECT_EQ_JSON(payload["direction"], String, "sending");
                EXPECT_EQ_JSON(payload["phase"], String, "endpoint");
                EXPECT_EQ_JSON(payload["sync"], Bool, false);

              } else if (nameString == "Text in main thread with stack") {
                EXPECT_EQ(state, S_TextWithStack);
                state = State(S_TextWithStack + 1);
                EXPECT_EQ(typeString, "Text");
                EXPECT_FALSE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["name"], String, "");

              } else if (nameString == "Text from main thread with stack") {
                EXPECT_EQ(state, S_TextToMTWithStack);
                state = State(S_TextToMTWithStack + 1);
                EXPECT_EQ(typeString, "Text");
                EXPECT_FALSE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["name"], String, "");

              } else if (nameString == "Text in registered thread with stack") {
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
          }    // for (marker:data)
        }      // markers.data
      }        // markers
    }          // thread0
  }            // threads

  // We should have read all expected markers.
  EXPECT_EQ(state, S_LAST);

  {
    const Json::Value& meta = root["meta"];
    ASSERT_TRUE(!meta.isNull());
    ASSERT_TRUE(meta.isObject());

    // root.meta is an object.
    {
      const Json::Value& markerSchema = meta["markerSchema"];
      ASSERT_TRUE(!markerSchema.isNull());
      ASSERT_TRUE(markerSchema.isArray());

      // root.meta.markerSchema is an array.

      std::set<std::string> testedSchemaNames;

      for (const Json::Value& schema : markerSchema) {
        const Json::Value& name = schema["name"];
        ASSERT_TRUE(name.isString());
        const std::string nameString = name.asString();

        const Json::Value& display = schema["display"];
        ASSERT_TRUE(display.isArray());

        const Json::Value& data = schema["data"];
        ASSERT_TRUE(data.isArray());

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

        } else if (nameString == "UserTimingMark") {
          EXPECT_EQ(display.size(), 2u);
          EXPECT_EQ(display[0u].asString(), "marker-chart");
          EXPECT_EQ(display[1u].asString(), "marker-table");

          ASSERT_EQ(data.size(), 4u);

          ASSERT_TRUE(data[0u].isObject());
          EXPECT_EQ_JSON(data[0u]["label"], String, "Marker");
          EXPECT_EQ_JSON(data[0u]["value"], String, "UserTiming");

          ASSERT_TRUE(data[1u].isObject());
          EXPECT_EQ_JSON(data[1u]["key"], String, "entryType");
          EXPECT_EQ_JSON(data[1u]["label"], String, "Entry Type");
          EXPECT_EQ_JSON(data[1u]["format"], String, "string");

          ASSERT_TRUE(data[2u].isObject());
          EXPECT_EQ_JSON(data[2u]["key"], String, "name");
          EXPECT_EQ_JSON(data[2u]["label"], String, "Name");
          EXPECT_EQ_JSON(data[2u]["format"], String, "string");

          ASSERT_TRUE(data[3u].isObject());
          EXPECT_EQ_JSON(data[3u]["label"], String, "Description");
          EXPECT_EQ_JSON(data[3u]["value"], String,
                         "UserTimingMark is created using the DOM API "
                         "performance.mark().");

        } else if (nameString == "UserTimingMeasure") {
          EXPECT_EQ(display.size(), 2u);
          EXPECT_EQ(display[0u].asString(), "marker-chart");
          EXPECT_EQ(display[1u].asString(), "marker-table");

          ASSERT_EQ(data.size(), 6u);

          ASSERT_TRUE(data[0u].isObject());
          EXPECT_EQ_JSON(data[0u]["label"], String, "Marker");
          EXPECT_EQ_JSON(data[0u]["value"], String, "UserTiming");

          ASSERT_TRUE(data[1u].isObject());
          EXPECT_EQ_JSON(data[1u]["key"], String, "entryType");
          EXPECT_EQ_JSON(data[1u]["label"], String, "Entry Type");
          EXPECT_EQ_JSON(data[1u]["format"], String, "string");

          ASSERT_TRUE(data[2u].isObject());
          EXPECT_EQ_JSON(data[2u]["key"], String, "name");
          EXPECT_EQ_JSON(data[2u]["label"], String, "Name");
          EXPECT_EQ_JSON(data[2u]["format"], String, "string");

          ASSERT_TRUE(data[3u].isObject());
          EXPECT_EQ_JSON(data[3u]["key"], String, "startMark");
          EXPECT_EQ_JSON(data[3u]["label"], String, "Start Mark");
          EXPECT_EQ_JSON(data[3u]["format"], String, "string");

          ASSERT_TRUE(data[4u].isObject());
          EXPECT_EQ_JSON(data[4u]["key"], String, "endMark");
          EXPECT_EQ_JSON(data[4u]["label"], String, "End Mark");
          EXPECT_EQ_JSON(data[4u]["format"], String, "string");

          ASSERT_TRUE(data[5u].isObject());
          EXPECT_EQ_JSON(data[5u]["label"], String, "Description");
          EXPECT_EQ_JSON(data[5u]["value"], String,
                         "UserTimingMeasure is created using the DOM API "
                         "performance.measure().");

        } else if (nameString == "BHR-detected hang") {
          EXPECT_EQ(display.size(), 3u);
          EXPECT_EQ(display[0u].asString(), "marker-chart");
          EXPECT_EQ(display[1u].asString(), "marker-table");
          EXPECT_EQ(display[2u].asString(), "timeline-overview");

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

        } else {
          ADD_FAILURE() << "Unknown marker schema '" << nameString.c_str()
                        << "'";
        }
      }

      // Check that we've got all expected schema.
      EXPECT_TRUE(testedSchemaNames.find("Text") != testedSchemaNames.end());
      EXPECT_TRUE(testedSchemaNames.find("tracing") != testedSchemaNames.end());
      EXPECT_TRUE(testedSchemaNames.find("UserTimingMark") !=
                  testedSchemaNames.end());
      EXPECT_TRUE(testedSchemaNames.find("UserTimingMeasure") !=
                  testedSchemaNames.end());
      EXPECT_TRUE(testedSchemaNames.find("MediaSample") !=
                  testedSchemaNames.end());
    }  // markerSchema
  }    // meta

  Maybe<ProfilerBufferInfo> info = profiler_get_buffer_info();
  MOZ_RELEASE_ASSERT(info.isSome());
  printf("Profiler buffer range: %llu .. %llu (%llu bytes)\n",
         static_cast<unsigned long long>(info->mRangeStart),
         static_cast<unsigned long long>(info->mRangeEnd),
         // sizeof(ProfileBufferEntry) == 9
         (static_cast<unsigned long long>(info->mRangeEnd) -
          static_cast<unsigned long long>(info->mRangeStart)) *
             9);
  printf("Stats:         min(ns) .. mean(ns) .. max(ns)  [count]\n");
  printf("- Intervals:   %7.1f .. %7.1f  .. %7.1f  [%u]\n",
         info->mIntervalsNs.min, info->mIntervalsNs.sum / info->mIntervalsNs.n,
         info->mIntervalsNs.max, info->mIntervalsNs.n);
  printf("- Overheads:   %7.1f .. %7.1f  .. %7.1f  [%u]\n",
         info->mOverheadsNs.min, info->mOverheadsNs.sum / info->mOverheadsNs.n,
         info->mOverheadsNs.max, info->mOverheadsNs.n);
  printf("  - Locking:   %7.1f .. %7.1f  .. %7.1f  [%u]\n",
         info->mLockingsNs.min, info->mLockingsNs.sum / info->mLockingsNs.n,
         info->mLockingsNs.max, info->mLockingsNs.n);
  printf("  - Clearning: %7.1f .. %7.1f  .. %7.1f  [%u]\n",
         info->mCleaningsNs.min, info->mCleaningsNs.sum / info->mCleaningsNs.n,
         info->mCleaningsNs.max, info->mCleaningsNs.n);
  printf("  - Counters:  %7.1f .. %7.1f  .. %7.1f  [%u]\n",
         info->mCountersNs.min, info->mCountersNs.sum / info->mCountersNs.n,
         info->mCountersNs.max, info->mCountersNs.n);
  printf("  - Threads:   %7.1f .. %7.1f  .. %7.1f  [%u]\n",
         info->mThreadsNs.min, info->mThreadsNs.sum / info->mThreadsNs.n,
         info->mThreadsNs.max, info->mThreadsNs.n);

  profiler_stop();

  // Nothing more should have happened to the GTestMarkerPayloads.
  EXPECT_EQ(GTestMarkerPayload::sNumCreated, 10 + 0 + 0);
  EXPECT_EQ(GTestMarkerPayload::sNumSerialized, 10 + 0 + 0);
  EXPECT_EQ(GTestMarkerPayload::sNumDeserialized, 0 + 10 + 0);
  EXPECT_EQ(GTestMarkerPayload::sNumStreamed, 0 + 10 + 0);
  EXPECT_EQ(GTestMarkerPayload::sNumDestroyed, 10 + 10 + 0);

  // Try to add markers while the profiler is stopped.
  for (int i = 0; i < 10; i++) {
    PROFILER_ADD_MARKER_WITH_PAYLOAD("M5", OTHER, GTestMarkerPayload, (i));
  }

  // Warning: this could be racy
  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL, features,
                 filters, MOZ_ARRAY_LENGTH(filters), 0);

  EXPECT_TRUE(::profiler_stream_json_for_this_process(w));

  profiler_stop();

  // The second set of GTestMarkerPayloads should not have been serialized or
  // streamed.
  EXPECT_EQ(GTestMarkerPayload::sNumCreated, 10 + 0 + 0 + 10);
  EXPECT_EQ(GTestMarkerPayload::sNumSerialized, 10 + 0 + 0 + 0);
  EXPECT_EQ(GTestMarkerPayload::sNumDeserialized, 0 + 10 + 0 + 0);
  EXPECT_EQ(GTestMarkerPayload::sNumStreamed, 0 + 10 + 0 + 0);
  EXPECT_EQ(GTestMarkerPayload::sNumDestroyed, 10 + 10 + 0 + 10);
}

// The duration limit will be removed from Firefox, see bug 1632365.
#if 0
TEST(GeckoProfiler, DurationLimit)
{
  uint32_t features = ProfilerFeature::StackWalk;
  const char* filters[] = {"GeckoMain"};

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL, features,
                 filters, MOZ_ARRAY_LENGTH(filters), 0, Some(1.5));

  // Clear up the counters after the last test.
  GTestMarkerPayload::sNumCreated = 0;
  GTestMarkerPayload::sNumSerialized = 0;
  GTestMarkerPayload::sNumDeserialized = 0;
  GTestMarkerPayload::sNumStreamed = 0;
  GTestMarkerPayload::sNumDestroyed = 0;

  PROFILER_ADD_MARKER_WITH_PAYLOAD("M1", OTHER, GTestMarkerPayload, (1));
  PR_Sleep(PR_MillisecondsToInterval(1100));
  PROFILER_ADD_MARKER_WITH_PAYLOAD("M2", OTHER, GTestMarkerPayload, (2));
  PR_Sleep(PR_MillisecondsToInterval(500));

  SpliceableChunkedJSONWriter w;
  ASSERT_TRUE(profiler_stream_json_for_this_process(w));

  // Both markers created, serialized, destroyed; Only the first marker should
  // have been deserialized, streamed, and destroyed again.
  EXPECT_EQ(GTestMarkerPayload::sNumCreated, 2);
  EXPECT_EQ(GTestMarkerPayload::sNumSerialized, 2);
  EXPECT_EQ(GTestMarkerPayload::sNumDeserialized, 1);
  EXPECT_EQ(GTestMarkerPayload::sNumStreamed, 1);
  EXPECT_EQ(GTestMarkerPayload::sNumDestroyed, 3);
}
#endif

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

  UniquePtr<char[]> profile = profiler_get_profile();
  ASSERT_TRUE(profile && profile[0] == '{');

  profiler_stop();

  ASSERT_TRUE(!profiler_get_profile());
}

static void JSONOutputCheck(const char* aOutput) {
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
  const char* filters[] = {"GeckoMain"};

  SpliceableChunkedJSONWriter w;
  ASSERT_TRUE(!::profiler_stream_json_for_this_process(w));

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL, features,
                 filters, MOZ_ARRAY_LENGTH(filters), 0);

  w.Start();
  ASSERT_TRUE(::profiler_stream_json_for_this_process(w));
  w.End();

  UniquePtr<char[]> profile = w.ChunkedWriteFunc().CopyData();

  JSONOutputCheck(profile.get());

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

  JSONOutputCheck(profile.get());

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

void DoSuspendAndSample(int aTid, nsIThread* aThread) {
  aThread->Dispatch(
      NS_NewRunnableFunction("GeckoProfiler_SuspendAndSample_Test::TestBody",
                             [&]() {
                               uint32_t features = ProfilerFeature::Leaf;
                               GTestStackCollector collector;
                               profiler_suspend_and_sample_thread(
                                   aTid, features, collector,
                                   /* sampleNative = */ true);

                               ASSERT_TRUE(collector.mSetIsMainThread == 1);
                               ASSERT_TRUE(collector.mFrames > 0);
                             }),
      NS_DISPATCH_SYNC);
}

TEST(GeckoProfiler, SuspendAndSample)
{
  nsCOMPtr<nsIThread> thread;
  nsresult rv = NS_NewNamedThread("GeckoProfGTest", getter_AddRefs(thread));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  int tid = profiler_current_thread_id();

  ASSERT_TRUE(!profiler_is_active());

  // Suspend and sample while the profiler is inactive.
  DoSuspendAndSample(tid, thread);

  uint32_t features = ProfilerFeature::JS | ProfilerFeature::Threads;
  const char* filters[] = {"GeckoMain", "Compositor"};

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL, features,
                 filters, MOZ_ARRAY_LENGTH(filters), 0);

  ASSERT_TRUE(profiler_is_active());

  // Suspend and sample while the profiler is active.
  DoSuspendAndSample(tid, thread);

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
  ASSERT_TRUE(profileCompleted);
  ASSERT_TRUE(profileCompleted[0] == '{');
  ASSERT_TRUE(strstr(profileCompleted.get(), "PostSamplingCallback completed"));

  profiler_pause();
  {
    // Paused -> This label should not appear.
    AUTO_PROFILER_LABEL("PostSamplingCallback paused", OTHER);
    ASSERT_EQ(WaitForSamplingState(), SamplingState::SamplingPaused);
  }
  UniquePtr<char[]> profilePaused = profiler_get_profile();
  ASSERT_TRUE(profilePaused);
  ASSERT_TRUE(profilePaused[0] == '{');
  ASSERT_FALSE(strstr(profilePaused.get(), "PostSamplingCallback paused"));

  profiler_resume();
  {
    // Stack sampling -> This label should appear at least once.
    AUTO_PROFILER_LABEL("PostSamplingCallback resumed", OTHER);
    ASSERT_EQ(WaitForSamplingState(), SamplingState::SamplingCompleted);
  }
  UniquePtr<char[]> profileResumed = profiler_get_profile();
  ASSERT_TRUE(profileResumed);
  ASSERT_TRUE(profileResumed[0] == '{');
  ASSERT_TRUE(strstr(profileResumed.get(), "PostSamplingCallback resumed"));

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                 ProfilerFeature::StackWalk | ProfilerFeature::NoStackSampling,
                 filters, MOZ_ARRAY_LENGTH(filters), 0);
  {
    // No stack sampling -> This label should not appear.
    AUTO_PROFILER_LABEL("PostSamplingCallback completed (no stacks)", OTHER);
    ASSERT_EQ(WaitForSamplingState(), SamplingState::NoStackSamplingCompleted);
  }
  UniquePtr<char[]> profileNoStacks = profiler_get_profile();
  ASSERT_TRUE(profileNoStacks);
  ASSERT_TRUE(profileNoStacks[0] == '{');
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
  ASSERT_TRUE(profile);
  ASSERT_TRUE(profile[0] == '{');
  ASSERT_TRUE(strstr(profile.get(), "GeckoMain (pre-xul)"));
  ASSERT_TRUE(strstr(profile.get(), "Marker from base profiler"));

  profiler_stop();
  ASSERT_TRUE(!profiler_is_active());
}
