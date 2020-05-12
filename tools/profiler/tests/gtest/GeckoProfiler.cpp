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
#include "platform.h"
#include "ProfileBuffer.h"
#include "ProfileJSONWriter.h"
#include "ProfilerMarkerPayload.h"

#include "js/Initialization.h"
#include "js/Printf.h"
#include "jsapi.h"
#include "json/json.h"
#include "mozilla/Atomics.h"
#include "mozilla/BlocksRingBuffer.h"
#include "mozilla/ProfileBufferEntrySerializationGeckoExtensions.h"
#include "mozilla/UniquePtrExtensions.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "nsIThread.h"
#include "nsThreadUtils.h"

#include "gtest/gtest.h"

#include <cstring>

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
    nsCString cs(NS_LITERAL_CSTRING("nsCString"));
    nsString s(NS_LITERAL_STRING("nsString"));
    nsAutoCString acs(NS_LITERAL_CSTRING("nsAutoCString"));
    nsAutoString as(NS_LITERAL_STRING("nsAutoString"));
    nsAutoCStringN<8> acs8(NS_LITERAL_CSTRING("nsAutoCStringN"));
    nsAutoStringN<8> as8(NS_LITERAL_STRING("nsAutoStringN"));
    JS::UniqueChars jsuc = JS_smprintf("%s", "JS::UniqueChars");

    rb.PutObjects(cs, s, acs, as, acs8, as8, jsuc);
  }

  rb.ReadEach([](ProfileBufferEntryReader& aER) {
    ASSERT_EQ(aER.ReadObject<nsCString>(), NS_LITERAL_CSTRING("nsCString"));
    ASSERT_EQ(aER.ReadObject<nsString>(), NS_LITERAL_STRING("nsString"));
    ASSERT_EQ(aER.ReadObject<nsAutoCString>(),
              NS_LITERAL_CSTRING("nsAutoCString"));
    ASSERT_EQ(aER.ReadObject<nsAutoString>(),
              NS_LITERAL_STRING("nsAutoString"));
    ASSERT_EQ(aER.ReadObject<nsAutoCStringN<8>>(),
              NS_LITERAL_CSTRING("nsAutoCStringN"));
    ASSERT_EQ(aER.ReadObject<nsAutoStringN<8>>(),
              NS_LITERAL_STRING("nsAutoStringN"));
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
  ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::Privacy));

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
    ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::Privacy));

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
        ProfilerFeature::MainThreadIO | ProfilerFeature::Privacy;
    const char* filters[] = {"GeckoMain", "Foo", "Bar"};

    // Testing with some arbitrary buffer size (as could be provided by
    // external code), which we convert to the appropriate power of 2.
    profiler_start(PowerOfTwo32(999999), 3, features, filters,
                   MOZ_ARRAY_LENGTH(filters), 123, Some(25.0));

    ASSERT_TRUE(profiler_is_active());
    ASSERT_TRUE(profiler_feature_active(ProfilerFeature::MainThreadIO));
    ASSERT_TRUE(profiler_feature_active(ProfilerFeature::Privacy));

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
        ProfilerFeature::MainThreadIO | ProfilerFeature::Privacy;
    const char* filters[] = {"GeckoMain", "Foo", "Bar"};

    profiler_start(PowerOfTwo32(999999), 3, features, filters,
                   MOZ_ARRAY_LENGTH(filters), 0, Nothing());

    ASSERT_TRUE(profiler_is_active());
    ASSERT_TRUE(profiler_feature_active(ProfilerFeature::MainThreadIO));
    ASSERT_TRUE(profiler_feature_active(ProfilerFeature::Privacy));

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
    ASSERT_TRUE(profiler_feature_active(ProfilerFeature::Privacy));

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
    ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::Privacy));

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
    ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::Privacy));

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
              ASSERT_TRUE(!profiler_feature_active(ProfilerFeature::Privacy));

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

  {
    uint32_t features = ProfilerFeature::Privacy;
    const char* filters[] = {"GeckoMain"};

    profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL,
                   features, filters, MOZ_ARRAY_LENGTH(filters), 0);

    // No backtraces obtained when ProfilerFeature::Privacy is set.
    ASSERT_TRUE(!profiler_get_backtrace());

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
  PROFILER_ADD_MARKER("Not paused", OTHER);
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
  PROFILER_ADD_MARKER("Paused", OTHER);
  info2 = profiler_get_buffer_info();
  ASSERT_TRUE(info1->mRangeEnd == info2->mRangeEnd);

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
  SprintfLiteral(buf, "gtest-%d", mN);
  aWriter.IntProperty(buf, mN);
  ++sNumStreamed;
}

TEST(GeckoProfiler, Markers)
{
  uint32_t features = ProfilerFeature::StackWalk;
  const char* filters[] = {"GeckoMain"};

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL, features,
                 filters, MOZ_ARRAY_LENGTH(filters), 0);

  profiler_tracing_marker("A", "tracing event",
                          JS::ProfilingCategoryPair::OTHER, TRACING_EVENT);
  PROFILER_TRACING_MARKER("A", "tracing start", OTHER, TRACING_INTERVAL_START);
  PROFILER_TRACING_MARKER("A", "tracing end", OTHER, TRACING_INTERVAL_END);

  UniqueProfilerBacktrace bt = profiler_get_backtrace();
  profiler_tracing_marker("B", "tracing event with stack",
                          JS::ProfilingCategoryPair::OTHER, TRACING_EVENT,
                          std::move(bt));

  { AUTO_PROFILER_TRACING_MARKER("C", "auto tracing", OTHER); }

  PROFILER_ADD_MARKER("M1", OTHER);
  PROFILER_ADD_MARKER_WITH_PAYLOAD("M2", OTHER, TracingMarkerPayload,
                                   ("C", TRACING_EVENT));
  PROFILER_ADD_MARKER("M3", OTHER);
  PROFILER_ADD_MARKER_WITH_PAYLOAD(
      "M4", OTHER, TracingMarkerPayload,
      ("C", TRACING_EVENT, mozilla::Nothing(), profiler_get_backtrace()));

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
  PROFILER_ADD_MARKER_WITH_PAYLOAD(
      "FileIOMarkerPayload marker", OTHER, FileIOMarkerPayload,
      ("operation", "source", "filename", ts1, ts2, nullptr));

  // Other markers in alphabetical order of payload class names.

  PROFILER_ADD_MARKER_WITH_PAYLOAD(
      "DOMEventMarkerPayload marker", OTHER, DOMEventMarkerPayload,
      (NS_LITERAL_STRING("dom event"), ts1, "category", TRACING_EVENT,
       mozilla::Nothing()));

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

  PROFILER_ADD_MARKER_WITH_PAYLOAD("HangMarkerPayload marker", OTHER,
                                   HangMarkerPayload, (ts1, ts2));

  PROFILER_ADD_MARKER_WITH_PAYLOAD("LogMarkerPayload marker", OTHER,
                                   LogMarkerPayload, ("module", "text", ts1));

  PROFILER_ADD_MARKER_WITH_PAYLOAD("LongTaskMarkerPayload marker", OTHER,
                                   LongTaskMarkerPayload, (ts1, ts2));

  PROFILER_ADD_MARKER_WITH_PAYLOAD("NativeAllocationMarkerPayload marker",
                                   OTHER, NativeAllocationMarkerPayload,
                                   (ts1, 9876543210, 1234, 5678, nullptr));

  PROFILER_ADD_MARKER_WITH_PAYLOAD(
      "NetworkMarkerPayload start marker", OTHER, NetworkMarkerPayload,
      (1, "http://mozilla.org/", NetworkLoadType::LOAD_START, ts1, ts2, 34, 56,
       net::kCacheHit, 78));

  PROFILER_ADD_MARKER_WITH_PAYLOAD(
      "NetworkMarkerPayload stop marker", OTHER, NetworkMarkerPayload,
      (12, "http://mozilla.org/", NetworkLoadType::LOAD_STOP, ts1, ts2, 34, 56,
       net::kCacheUnresolved, 78, nullptr, nullptr, nullptr,
       Some(nsDependentCString(NS_LITERAL_CSTRING("text/html").get()))));

  PROFILER_ADD_MARKER_WITH_PAYLOAD(
      "NetworkMarkerPayload redirect marker", OTHER, NetworkMarkerPayload,
      (123, "http://mozilla.org/", NetworkLoadType::LOAD_REDIRECT, ts1, ts2, 34,
       56, net::kCacheUnresolved, 78, nullptr, "http://example.com/"));

  PROFILER_ADD_MARKER_WITH_PAYLOAD(
      "PrefMarkerPayload marker", OTHER, PrefMarkerPayload,
      ("preference name", mozilla::Nothing(), mozilla::Nothing(),
       NS_LITERAL_CSTRING("preference value"), ts1));

  nsCString screenshotURL = NS_LITERAL_CSTRING("url");
  PROFILER_ADD_MARKER_WITH_PAYLOAD(
      "ScreenshotPayload marker", OTHER, ScreenshotPayload,
      (ts1, std::move(screenshotURL), mozilla::gfx::IntSize(12, 34),
       uintptr_t(0x45678u)));

  PROFILER_ADD_MARKER_WITH_PAYLOAD("TextMarkerPayload marker 1", OTHER,
                                   TextMarkerPayload,
                                   (NS_LITERAL_CSTRING("text"), ts1));

  PROFILER_ADD_MARKER_WITH_PAYLOAD("TextMarkerPayload marker 2", OTHER,
                                   TextMarkerPayload,
                                   (NS_LITERAL_CSTRING("text"), ts1, ts2));

  PROFILER_ADD_MARKER_WITH_PAYLOAD(
      "UserTimingMarkerPayload marker mark", OTHER, UserTimingMarkerPayload,
      (NS_LITERAL_STRING("mark name"), ts1, mozilla::Nothing()));

  PROFILER_ADD_MARKER_WITH_PAYLOAD(
      "UserTimingMarkerPayload marker measure", OTHER, UserTimingMarkerPayload,
      (NS_LITERAL_STRING("measure name"), Some(NS_LITERAL_STRING("start mark")),
       Some(NS_LITERAL_STRING("end mark")), ts1, ts2, mozilla::Nothing()));

  PROFILER_ADD_MARKER_WITH_PAYLOAD("VsyncMarkerPayload marker", OTHER,
                                   VsyncMarkerPayload, (ts1));

  PROFILER_ADD_MARKER_WITH_PAYLOAD(
      "IPCMarkerPayload marker", IPC, IPCMarkerPayload,
      (1111, 1, 3 /* PAPZ::Msg_LayerTransforms */, mozilla::ipc::ParentSide,
       mozilla::ipc::MessageDirection::eSending, false, ts1));

  SpliceableChunkedJSONWriter w;
  w.Start();
  EXPECT_TRUE(profiler_stream_json_for_this_process(w));
  w.End();

  // The GTestMarkerPayloads should have been deserialized, streamed, and
  // destroyed.
  EXPECT_EQ(GTestMarkerPayload::sNumCreated, 10 + 0);
  EXPECT_EQ(GTestMarkerPayload::sNumSerialized, 10 + 0);
  EXPECT_EQ(GTestMarkerPayload::sNumDeserialized, 0 + 10);
  EXPECT_EQ(GTestMarkerPayload::sNumStreamed, 0 + 10);
  EXPECT_EQ(GTestMarkerPayload::sNumDestroyed, 10 + 10);

  UniquePtr<char[]> profile = w.WriteFunc()->CopyData();
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
    S_FileIOMarkerPayload,
    S_DOMEventMarkerPayload,
    S_GCMajorMarkerPayload,
    S_GCMinorMarkerPayload,
    S_GCSliceMarkerPayload,
    S_HangMarkerPayload,
    S_LogMarkerPayload,
    S_LongTaskMarkerPayload,
    S_NativeAllocationMarkerPayload,
    S_NetworkMarkerPayload_start,
    S_NetworkMarkerPayload_stop,
    S_NetworkMarkerPayload_redirect,
    S_PrefMarkerPayload,
    S_ScreenshotPayload,
    S_TextMarkerPayload1,
    S_TextMarkerPayload2,
    S_UserTimingMarkerPayload_mark,
    S_UserTimingMarkerPayload_measure,
    S_VsyncMarkerPayload,
    S_IPCMarkerPayload,

    S_LAST,
  } state = State(0);

  // These will be set when first read from S_FileIOMarkerPayload, then
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
            ASSERT_TRUE(marker.isArray());
            ASSERT_GE(marker.size(), 3u);
            ASSERT_LE(marker.size(), 4u);

            // root.threads[0].markers.data[i] is an array with 3 or 4 elements.

            ASSERT_TRUE(marker[0].isUInt());  // name id
            const Json::Value& name = stringTable[marker[0].asUInt()];
            ASSERT_TRUE(name.isString());
            std::string nameString = name.asString();

            EXPECT_TRUE(marker[1].isNumeric());  // timestamp

            EXPECT_TRUE(marker[2].isUInt());  // category

            if (marker.size() == 3u) {
              // root.threads[0].markers.data[i] is an array with 3 elements,
              // so there is no payload.
              if (nameString == "M1") {
                ASSERT_EQ(state, S_M1);
                state = State(state + 1);
              } else if (nameString == "M3") {
                ASSERT_EQ(state, S_M3);
                state = State(state + 1);
              }
            } else {
              // root.threads[0].markers.data[i] is an array with 4 elements,
              // so there is a payload.
              const Json::Value& payload = marker[3];
              ASSERT_TRUE(payload.isObject());

              // root.threads[0].markers.data[i][3] is an object (payload).

              // It should at least have a "type" string.
              const Json::Value& type = payload["type"];
              ASSERT_TRUE(type.isString());
              std::string typeString = type.asString();

              if (nameString == "tracing event") {
                EXPECT_EQ(state, S_tracing_event);
                state = State(S_tracing_event + 1);
                EXPECT_EQ(typeString, "tracing");
                EXPECT_EQ_JSON(payload["category"], String, "A");
                EXPECT_TRUE(payload["interval"].isNull());
                EXPECT_TRUE(payload["stack"].isNull());

              } else if (nameString == "tracing start") {
                EXPECT_EQ(state, S_tracing_start);
                state = State(S_tracing_start + 1);
                EXPECT_EQ(typeString, "tracing");
                EXPECT_EQ_JSON(payload["category"], String, "A");
                EXPECT_EQ_JSON(payload["interval"], String, "start");
                EXPECT_TRUE(payload["stack"].isNull());

              } else if (nameString == "tracing end") {
                EXPECT_EQ(state, S_tracing_end);
                state = State(S_tracing_end + 1);
                EXPECT_EQ(typeString, "tracing");
                EXPECT_EQ_JSON(payload["category"], String, "A");
                EXPECT_EQ_JSON(payload["interval"], String, "end");
                EXPECT_TRUE(payload["stack"].isNull());

              } else if (nameString == "tracing event with stack") {
                EXPECT_EQ(state, S_tracing_event_with_stack);
                state = State(S_tracing_event_with_stack + 1);
                EXPECT_EQ(typeString, "tracing");
                EXPECT_EQ_JSON(payload["category"], String, "B");
                EXPECT_TRUE(payload["interval"].isNull());
                EXPECT_TRUE(payload["stack"].isObject());

              } else if (nameString == "auto tracing") {
                switch (state) {
                  case S_tracing_auto_tracing_start:
                    state = State(S_tracing_auto_tracing_start + 1);
                    EXPECT_EQ_JSON(payload["category"], String, "C");
                    EXPECT_EQ_JSON(payload["interval"], String, "start");
                    EXPECT_TRUE(payload["stack"].isNull());
                    break;
                  case S_tracing_auto_tracing_end:
                    state = State(S_tracing_auto_tracing_end + 1);
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
                EXPECT_EQ_JSON(payload["category"], String, "C");
                EXPECT_TRUE(payload["interval"].isNull());
                EXPECT_TRUE(payload["stack"].isNull());

              } else if (nameString == "M4") {
                EXPECT_EQ(state, S_tracing_M4_C_stack);
                state = State(S_tracing_M4_C_stack + 1);
                EXPECT_EQ(typeString, "tracing");
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

              } else if (nameString == "FileIOMarkerPayload marker") {
                EXPECT_EQ(state, S_FileIOMarkerPayload);
                state = State(S_FileIOMarkerPayload + 1);
                EXPECT_EQ(typeString, "FileIO");

                // Record start and end times, to compare with timestamps in
                // following markers.
                EXPECT_EQ(ts1Double, 0.0);
                ts1Double = payload["startTime"].asDouble();
                EXPECT_NE(ts1Double, 0.0);
                EXPECT_EQ(ts2Double, 0.0);
                ts2Double = payload["endTime"].asDouble();
                EXPECT_NE(ts2Double, 0.0);

                // Start timestamp is also stored in marker outside of payload.
                EXPECT_EQ_JSON(marker[1], Double, ts1Double);

                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["operation"], String, "operation");
                EXPECT_EQ_JSON(payload["source"], String, "source");
                EXPECT_EQ_JSON(payload["filename"], String, "filename");

              } else if (nameString == "DOMEventMarkerPayload marker") {
                EXPECT_EQ(state, S_DOMEventMarkerPayload);
                state = State(S_DOMEventMarkerPayload + 1);
                EXPECT_EQ(typeString, "tracing");
                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["category"], String, "category");
                EXPECT_TRUE(payload["interval"].isNull());
                EXPECT_EQ_JSON(payload["timeStamp"], Double, ts1Double);
                EXPECT_EQ_JSON(payload["eventType"], String, "dom event");

              } else if (nameString == "GCMajorMarkerPayload marker") {
                EXPECT_EQ(state, S_GCMajorMarkerPayload);
                state = State(S_GCMajorMarkerPayload + 1);
                EXPECT_EQ(typeString, "GCMajor");
                EXPECT_EQ_JSON(payload["startTime"], Double, ts1Double);
                // Start timestamp is also stored in marker outside of payload.
                EXPECT_EQ_JSON(marker[1], Double, ts1Double);
                EXPECT_EQ_JSON(payload["endTime"], Double, ts2Double);
                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["timings"], Int, 42);

              } else if (nameString == "GCMinorMarkerPayload marker") {
                EXPECT_EQ(state, S_GCMinorMarkerPayload);
                state = State(S_GCMinorMarkerPayload + 1);
                EXPECT_EQ(typeString, "GCMinor");
                EXPECT_EQ_JSON(payload["startTime"], Double, ts1Double);
                // Start timestamp is also stored in marker outside of payload.
                EXPECT_EQ_JSON(marker[1], Double, ts1Double);
                EXPECT_EQ_JSON(payload["endTime"], Double, ts2Double);
                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["nursery"], Int, 43);

              } else if (nameString == "GCSliceMarkerPayload marker") {
                EXPECT_EQ(state, S_GCSliceMarkerPayload);
                state = State(S_GCSliceMarkerPayload + 1);
                EXPECT_EQ(typeString, "GCSlice");
                EXPECT_EQ_JSON(payload["startTime"], Double, ts1Double);
                // Start timestamp is also stored in marker outside of payload.
                EXPECT_EQ_JSON(marker[1], Double, ts1Double);
                EXPECT_EQ_JSON(payload["endTime"], Double, ts2Double);
                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["timings"], Int, 44);

              } else if (nameString == "HangMarkerPayload marker") {
                EXPECT_EQ(state, S_HangMarkerPayload);
                state = State(S_HangMarkerPayload + 1);
                EXPECT_EQ(typeString, "BHR-detected hang");
                EXPECT_EQ_JSON(payload["startTime"], Double, ts1Double);
                // Start timestamp is also stored in marker outside of payload.
                EXPECT_EQ_JSON(marker[1], Double, ts1Double);
                EXPECT_EQ_JSON(payload["endTime"], Double, ts2Double);
                EXPECT_TRUE(payload["stack"].isNull());

              } else if (nameString == "LogMarkerPayload marker") {
                EXPECT_EQ(state, S_LogMarkerPayload);
                state = State(S_LogMarkerPayload + 1);
                EXPECT_EQ(typeString, "Log");
                EXPECT_EQ_JSON(payload["startTime"], Double, ts1Double);
                // Start timestamp is also stored in marker outside of payload.
                EXPECT_EQ_JSON(marker[1], Double, ts1Double);
                EXPECT_EQ_JSON(payload["endTime"], Double, ts1Double);
                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["name"], String, "text");
                EXPECT_EQ_JSON(payload["module"], String, "module");

              } else if (nameString == "LongTaskMarkerPayload marker") {
                EXPECT_EQ(state, S_LongTaskMarkerPayload);
                state = State(S_LongTaskMarkerPayload + 1);
                EXPECT_EQ(typeString, "MainThreadLongTask");
                EXPECT_EQ_JSON(payload["startTime"], Double, ts1Double);
                // Start timestamp is also stored in marker outside of payload.
                EXPECT_EQ_JSON(marker[1], Double, ts1Double);
                EXPECT_EQ_JSON(payload["endTime"], Double, ts2Double);
                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["category"], String, "LongTask");

              } else if (nameString == "NativeAllocationMarkerPayload marker") {
                EXPECT_EQ(state, S_NativeAllocationMarkerPayload);
                state = State(S_NativeAllocationMarkerPayload + 1);
                EXPECT_EQ(typeString, "Native allocation");
                EXPECT_EQ_JSON(payload["startTime"], Double, ts1Double);
                // Start timestamp is also stored in marker outside of payload.
                EXPECT_EQ_JSON(marker[1], Double, ts1Double);
                EXPECT_EQ_JSON(payload["endTime"], Double, ts1Double);
                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["size"], Int64, 9876543210);
                EXPECT_EQ_JSON(payload["memoryAddress"], Int64, 1234);
                EXPECT_EQ_JSON(payload["threadId"], Int64, 5678);

              } else if (nameString == "NetworkMarkerPayload start marker") {
                EXPECT_EQ(state, S_NetworkMarkerPayload_start);
                state = State(S_NetworkMarkerPayload_start + 1);
                EXPECT_EQ(typeString, "Network");
                EXPECT_EQ_JSON(payload["id"], Int64, 1);
                EXPECT_EQ_JSON(payload["URI"], String, "http://mozilla.org/");
                EXPECT_EQ_JSON(payload["pri"], Int64, 34);
                EXPECT_EQ_JSON(payload["count"], Int64, 56);
                EXPECT_EQ_JSON(payload["cache"], String, "Hit");
                EXPECT_EQ_JSON(payload["RedirectURI"], String, "");
                EXPECT_TRUE(payload["contentType"].isNull());

              } else if (nameString == "NetworkMarkerPayload stop marker") {
                EXPECT_EQ(state, S_NetworkMarkerPayload_stop);
                state = State(S_NetworkMarkerPayload_stop + 1);
                EXPECT_EQ(typeString, "Network");
                EXPECT_EQ_JSON(payload["id"], Int64, 12);
                EXPECT_EQ_JSON(payload["URI"], String, "http://mozilla.org/");
                EXPECT_EQ_JSON(payload["pri"], Int64, 34);
                EXPECT_EQ_JSON(payload["count"], Int64, 56);
                EXPECT_EQ_JSON(payload["cache"], String, "Unresolved");
                EXPECT_EQ_JSON(payload["RedirectURI"], String, "");
                EXPECT_EQ_JSON(payload["contentType"], String, "text/html");

              } else if (nameString == "NetworkMarkerPayload redirect marker") {
                EXPECT_EQ(state, S_NetworkMarkerPayload_redirect);
                state = State(S_NetworkMarkerPayload_redirect + 1);
                EXPECT_EQ(typeString, "Network");
                EXPECT_EQ_JSON(payload["id"], Int64, 123);
                EXPECT_EQ_JSON(payload["URI"], String, "http://mozilla.org/");
                EXPECT_EQ_JSON(payload["pri"], Int64, 34);
                EXPECT_EQ_JSON(payload["count"], Int64, 56);
                EXPECT_EQ_JSON(payload["cache"], String, "Unresolved");
                EXPECT_EQ_JSON(payload["RedirectURI"], String,
                               "http://example.com/");
                EXPECT_TRUE(payload["contentType"].isNull());

              } else if (nameString == "PrefMarkerPayload marker") {
                EXPECT_EQ(state, S_PrefMarkerPayload);
                state = State(S_PrefMarkerPayload + 1);
                EXPECT_EQ(typeString, "PreferenceRead");
                EXPECT_EQ_JSON(payload["startTime"], Double, ts1Double);
                // Start timestamp is also stored in marker outside of payload.
                EXPECT_EQ_JSON(marker[1], Double, ts1Double);
                EXPECT_EQ_JSON(payload["endTime"], Double, ts1Double);
                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["prefAccessTime"], Double, ts1Double);
                EXPECT_EQ_JSON(payload["prefName"], String, "preference name");
                EXPECT_EQ_JSON(payload["prefKind"], String, "Shared");
                EXPECT_EQ_JSON(payload["prefType"], String,
                               "Preference not found");
                EXPECT_EQ_JSON(payload["prefValue"], String,
                               "preference value");

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
                EXPECT_EQ_JSON(payload["startTime"], Double, ts1Double);
                // Start timestamp is also stored in marker outside of payload.
                EXPECT_EQ_JSON(marker[1], Double, ts1Double);
                EXPECT_EQ_JSON(payload["endTime"], Double, ts1Double);
                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["name"], String, "text");

              } else if (nameString == "TextMarkerPayload marker 2") {
                EXPECT_EQ(state, S_TextMarkerPayload2);
                state = State(S_TextMarkerPayload2 + 1);
                EXPECT_EQ(typeString, "Text");
                EXPECT_EQ_JSON(payload["startTime"], Double, ts1Double);
                // Start timestamp is also stored in marker outside of payload.
                EXPECT_EQ_JSON(marker[1], Double, ts1Double);
                EXPECT_EQ_JSON(payload["endTime"], Double, ts2Double);
                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["name"], String, "text");

              } else if (nameString == "UserTimingMarkerPayload marker mark") {
                EXPECT_EQ(state, S_UserTimingMarkerPayload_mark);
                state = State(S_UserTimingMarkerPayload_mark + 1);
                EXPECT_EQ(typeString, "UserTiming");
                EXPECT_EQ_JSON(payload["startTime"], Double, ts1Double);
                // Start timestamp is also stored in marker outside of payload.
                EXPECT_EQ_JSON(marker[1], Double, ts1Double);
                EXPECT_EQ_JSON(payload["endTime"], Double, ts1Double);
                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["name"], String, "mark name");
                EXPECT_EQ_JSON(payload["entryType"], String, "mark");

              } else if (nameString ==
                         "UserTimingMarkerPayload marker measure") {
                EXPECT_EQ(state, S_UserTimingMarkerPayload_measure);
                state = State(S_UserTimingMarkerPayload_measure + 1);
                EXPECT_EQ(typeString, "UserTiming");
                EXPECT_EQ_JSON(payload["startTime"], Double, ts1Double);
                // Start timestamp is also stored in marker outside of payload.
                EXPECT_EQ_JSON(marker[1], Double, ts1Double);
                EXPECT_EQ_JSON(payload["endTime"], Double, ts2Double);
                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["name"], String, "measure name");
                EXPECT_EQ_JSON(payload["entryType"], String, "measure");
                EXPECT_EQ_JSON(payload["startMark"], String, "start mark");
                EXPECT_EQ_JSON(payload["endMark"], String, "end mark");

              } else if (nameString == "VsyncMarkerPayload marker") {
                EXPECT_EQ(state, S_VsyncMarkerPayload);
                state = State(S_VsyncMarkerPayload + 1);
                EXPECT_EQ(typeString, "VsyncTimestamp");
                // Timestamp is stored in marker outside of payload.
                EXPECT_EQ_JSON(marker[1], Double, ts1Double);
                EXPECT_TRUE(payload["stack"].isNull());

              } else if (nameString == "IPCMarkerPayload marker") {
                EXPECT_EQ(state, S_IPCMarkerPayload);
                state = State(S_IPCMarkerPayload + 1);
                EXPECT_EQ(typeString, "IPC");
                EXPECT_EQ_JSON(payload["startTime"], Double, ts1Double);
                // Start timestamp is also stored in marker outside of payload.
                EXPECT_EQ_JSON(marker[1], Double, ts1Double);
                EXPECT_EQ_JSON(payload["endTime"], Double, ts1Double);
                EXPECT_TRUE(payload["stack"].isNull());
                EXPECT_EQ_JSON(payload["otherPid"], Int, 1111);
                EXPECT_EQ_JSON(payload["messageSeqno"], Int, 1);
                EXPECT_EQ_JSON(payload["messageType"], String,
                               "PAPZ::Msg_LayerTransforms");
                EXPECT_EQ_JSON(payload["side"], String, "parent");
                EXPECT_EQ_JSON(payload["direction"], String, "sending");
                EXPECT_EQ_JSON(payload["sync"], Bool, false);
              }
            }  // marker with payload
          }    // for (marker:data)
        }      // markers.data
      }        // markers
    }          // thread0
  }            // threads

  // We should have read all expected markers.
  EXPECT_EQ(state, S_LAST);

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

  EXPECT_TRUE(profiler_stream_json_for_this_process(w));

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
  ASSERT_TRUE(profiler_stream_json_for_this_process(w));

  UniquePtr<char[]> profile = w.WriteFunc()->CopyData();

  // counter name and description should appear as is.
  ASSERT_TRUE(strstr(profile.get(), COUNTER_NAME));
  ASSERT_TRUE(strstr(profile.get(), COUNTER_DESCRIPTION));
  ASSERT_FALSE(strstr(profile.get(), COUNTER_NAME2));
  ASSERT_FALSE(strstr(profile.get(), COUNTER_DESCRIPTION2));

  AUTO_PROFILER_COUNT_TOTAL(TestCounter2, 10);
  PR_Sleep(PR_MillisecondsToInterval(200));

  ASSERT_TRUE(profiler_stream_json_for_this_process(w));

  profile = w.WriteFunc()->CopyData();
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
  ASSERT_TRUE(!profiler_stream_json_for_this_process(w));

  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL, features,
                 filters, MOZ_ARRAY_LENGTH(filters), 0);

  w.Start();
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
  const char* filters[] = {"GeckoMain"};

  SpliceableChunkedJSONWriter w;
  ASSERT_TRUE(!profiler_stream_json_for_this_process(w));

  // Start the profiler on the main thread.
  profiler_start(PROFILER_DEFAULT_ENTRIES, PROFILER_DEFAULT_INTERVAL, features,
                 filters, MOZ_ARRAY_LENGTH(filters), 0);

  // Call profiler_stream_json_for_this_process on a background thread.
  thread->Dispatch(
      NS_NewRunnableFunction(
          "GeckoProfiler_StreamJSONForThisProcessThreaded_Test::TestBody",
          [&]() {
            w.Start();
            ASSERT_TRUE(profiler_stream_json_for_this_process(w));
            w.End();
          }),
      NS_DISPATCH_SYNC);

  UniquePtr<char[]> profile = w.WriteFunc()->CopyData();

  JSONOutputCheck(profile.get());

  // Stop the profiler and call profiler_stream_json_for_this_process on a
  // background thread.
  thread->Dispatch(
      NS_NewRunnableFunction(
          "GeckoProfiler_StreamJSONForThisProcessThreaded_Test::TestBody",
          [&]() {
            profiler_stop();
            ASSERT_TRUE(!profiler_stream_json_for_this_process(w));
          }),
      NS_DISPATCH_SYNC);
  thread->Shutdown();

  // Call profiler_stream_json_for_this_process on the main thread.
  ASSERT_TRUE(!profiler_stream_json_for_this_process(w));
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
  BASE_PROFILER_ADD_MARKER("Marker from base profiler", OTHER);
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
