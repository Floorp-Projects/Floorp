/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "core/TelemetryOrigin.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "mozilla/ContentBlockingLog.h"
#include "mozilla/Services.h"
#include "mozilla/Telemetry.h"
#include "mozilla/Unused.h"
#include "nsIObserverService.h"
#include "TelemetryFixture.h"
#include "TelemetryTestHelpers.h"

using namespace mozilla;
using namespace TelemetryTestHelpers;
using mozilla::Telemetry::OriginMetricID;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::StrEq;

constexpr auto kTelemetryTest1Metric = "telemetry.test_test1"_ns;

constexpr auto kDoubleclickOrigin = "doubleclick.net"_ns;
constexpr auto kDoubleclickOriginHash =
    "uXNT1PzjAVau8b402OMAIGDejKbiXfQX5iXvPASfO/s="_ns;
constexpr auto kFacebookOrigin = "fb.com"_ns;
constexpr auto kUnknownOrigin1 =
    "this origin isn't known to Origin Telemetry"_ns;
constexpr auto kUnknownOrigin2 = "neither is this one"_ns;

// Properly prepare the prio prefs
// (Sourced from PrioEncoder.cpp from when it was being prototyped)
constexpr auto prioKeyA =
    "35AC1C7576C7C6EDD7FED6BCFC337B34D48CB4EE45C86BEEFB40BD8875707733"_ns;
constexpr auto prioKeyB =
    "26E6674E65425B823F1F1D5F96E3BB3EF9E406EC7FBA7DEF8B08A35DD135AF50"_ns;

// Test that we can properly record origin stuff using the C++ API.
TEST_F(TelemetryTestFixture, RecordOrigin) {
  AutoJSContextWithGlobal cx(mCleanGlobal);
  JSContext* aCx = cx.GetJSContext();

  Unused << mTelemetry->ClearOrigins();

  Telemetry::RecordOrigin(OriginMetricID::TelemetryTest_Test1,
                          mozilla::ContentBlockingLog::kDummyOriginHash);

  JS::RootedValue originSnapshot(aCx);
  GetOriginSnapshot(aCx, &originSnapshot);

  ASSERT_FALSE(originSnapshot.isNullOrUndefined())
  << "Origin snapshot must not be null/undefined.";

  JS::RootedValue origins(aCx);
  JS::RootedObject snapshotObj(aCx, &originSnapshot.toObject());
  ASSERT_TRUE(
      JS_GetProperty(aCx, snapshotObj, kTelemetryTest1Metric.get(), &origins))
  << "telemetry.test_test1 must be in the snapshot.";

  JS::RootedObject originsObj(aCx, &origins.toObject());
  JS::RootedValue count(aCx);
  ASSERT_TRUE(JS_GetProperty(
      aCx, originsObj, mozilla::ContentBlockingLog::kDummyOriginHash.get(),
      &count));
  ASSERT_TRUE(count.isInt32() && count.toInt32() == 1)
  << "Must have recorded the origin exactly once.";

  // Now test that the snapshot didn't clear things out.
  GetOriginSnapshot(aCx, &originSnapshot);
  ASSERT_FALSE(originSnapshot.isNullOrUndefined());
  JS::RootedObject unemptySnapshotObj(aCx, &originSnapshot.toObject());
  JS::Rooted<JS::IdVector> ids(aCx, JS::IdVector(aCx));
  ASSERT_TRUE(JS_Enumerate(aCx, unemptySnapshotObj, &ids));
  ASSERT_GE(ids.length(), (unsigned)0) << "Returned object must not be empty.";
}

TEST_F(TelemetryTestFixture, RecordOriginTwiceAndClear) {
  AutoJSContextWithGlobal cx(mCleanGlobal);
  JSContext* aCx = cx.GetJSContext();

  Unused << mTelemetry->ClearOrigins();

  Telemetry::RecordOrigin(OriginMetricID::TelemetryTest_Test1,
                          kDoubleclickOrigin);
  Telemetry::RecordOrigin(OriginMetricID::TelemetryTest_Test1,
                          kDoubleclickOrigin);

  JS::RootedValue originSnapshot(aCx);
  GetOriginSnapshot(aCx, &originSnapshot, true /* aClear */);

  ASSERT_FALSE(originSnapshot.isNullOrUndefined())
  << "Origin snapshot must not be null/undefined.";

  JS::RootedValue origins(aCx);
  JS::RootedObject snapshotObj(aCx, &originSnapshot.toObject());
  ASSERT_TRUE(
      JS_GetProperty(aCx, snapshotObj, kTelemetryTest1Metric.get(), &origins))
  << "telemetry.test_test1 must be in the snapshot.";

  JS::RootedObject originsObj(aCx, &origins.toObject());
  JS::RootedValue count(aCx);
  ASSERT_TRUE(
      JS_GetProperty(aCx, originsObj, kDoubleclickOrigin.get(), &count));
  ASSERT_TRUE(count.isInt32() && count.toInt32() == 2)
  << "Must have recorded the origin exactly twice.";

  // Now check that snapshotting with clear actually cleared it.
  GetOriginSnapshot(aCx, &originSnapshot);
  ASSERT_FALSE(originSnapshot.isNullOrUndefined());
  JS::RootedObject emptySnapshotObj(aCx, &originSnapshot.toObject());
  JS::Rooted<JS::IdVector> ids(aCx, JS::IdVector(aCx));
  ASSERT_TRUE(JS_Enumerate(aCx, emptySnapshotObj, &ids));
  ASSERT_EQ(ids.length(), (unsigned)0) << "Returned object must be empty.";
}

TEST_F(TelemetryTestFixture, RecordOriginTwiceMixed) {
  AutoJSContextWithGlobal cx(mCleanGlobal);
  JSContext* aCx = cx.GetJSContext();

  Unused << mTelemetry->ClearOrigins();

  Telemetry::RecordOrigin(OriginMetricID::TelemetryTest_Test1,
                          kDoubleclickOrigin);
  Telemetry::RecordOrigin(OriginMetricID::TelemetryTest_Test1,
                          kDoubleclickOriginHash);

  Preferences::SetCString("prio.publicKeyA", prioKeyA);
  Preferences::SetCString("prio.publicKeyB", prioKeyB);

  nsTArray<Tuple<nsCString, nsCString>> encodedStrings;
  GetEncodedOriginStrings(aCx, kTelemetryTest1Metric + "-%u"_ns,
                          encodedStrings);
  ASSERT_EQ(2 * TelemetryOrigin::SizeOfPrioDatasPerMetric(),
            encodedStrings.Length());

  JS::RootedValue originSnapshot(aCx);
  GetOriginSnapshot(aCx, &originSnapshot, true /* aClear */);

  ASSERT_FALSE(originSnapshot.isNullOrUndefined())
  << "Origin snapshot must not be null/undefined.";

  JS::RootedValue origins(aCx);
  JS::RootedObject snapshotObj(aCx, &originSnapshot.toObject());
  ASSERT_TRUE(
      JS_GetProperty(aCx, snapshotObj, kTelemetryTest1Metric.get(), &origins))
  << "telemetry.test_test1 must be in the snapshot.";

  JS::RootedObject originsObj(aCx, &origins.toObject());
  JS::RootedValue count(aCx);
  ASSERT_TRUE(
      JS_GetProperty(aCx, originsObj, kDoubleclickOrigin.get(), &count));
  ASSERT_TRUE(count.isInt32() && count.toInt32() == 2)
  << "Must have recorded the origin exactly twice.";
}

TEST_F(TelemetryTestFixture, RecordUnknownOrigin) {
  AutoJSContextWithGlobal cx(mCleanGlobal);
  JSContext* aCx = cx.GetJSContext();

  Unused << mTelemetry->ClearOrigins();

  Telemetry::RecordOrigin(OriginMetricID::TelemetryTest_Test1, kUnknownOrigin1);

  JS::RootedValue originSnapshot(aCx);
  GetOriginSnapshot(aCx, &originSnapshot);

  ASSERT_FALSE(originSnapshot.isNullOrUndefined())
  << "Origin snapshot must not be null/undefined.";

  JS::RootedValue origins(aCx);
  JS::RootedObject snapshotObj(aCx, &originSnapshot.toObject());
  ASSERT_TRUE(
      JS_GetProperty(aCx, snapshotObj, kTelemetryTest1Metric.get(), &origins))
  << "telemetry.test_test1 must be in the snapshot.";

  JS::RootedObject originsObj(aCx, &origins.toObject());
  JS::RootedValue count(aCx);
  ASSERT_TRUE(JS_GetProperty(aCx, originsObj, "__UNKNOWN__", &count));
  ASSERT_TRUE(count.isInt32() && count.toInt32() == 1)
  << "Must have recorded the unknown origin exactly once.";

  // Record a second, different unknown origin and ensure only one is stored.
  Telemetry::RecordOrigin(OriginMetricID::TelemetryTest_Test1, kUnknownOrigin2);

  GetOriginSnapshot(aCx, &originSnapshot);

  ASSERT_FALSE(originSnapshot.isNullOrUndefined())
  << "Origin snapshot must not be null/undefined.";

  JS::RootedObject snapshotObj2(aCx, &originSnapshot.toObject());
  ASSERT_TRUE(
      JS_GetProperty(aCx, snapshotObj2, kTelemetryTest1Metric.get(), &origins))
  << "telemetry.test_test1 must be in the snapshot.";

  JS::RootedObject originsObj2(aCx, &origins.toObject());
  JS::RootedValue count2(aCx);
  ASSERT_TRUE(JS_GetProperty(aCx, originsObj2, "__UNKNOWN__", &count2));
  ASSERT_TRUE(count2.isInt32() && count2.toInt32() == 1)
  << "Must have recorded the unknown origin exactly once.";
}

TEST_F(TelemetryTestFixture, EncodedSnapshot) {
  AutoJSContextWithGlobal cx(mCleanGlobal);
  JSContext* aCx = cx.GetJSContext();

  Unused << mTelemetry->ClearOrigins();

  Telemetry::RecordOrigin(OriginMetricID::TelemetryTest_Test1,
                          kDoubleclickOrigin);
  Telemetry::RecordOrigin(OriginMetricID::TelemetryTest_Test1, kUnknownOrigin1);

  Preferences::SetCString("prio.publicKeyA", prioKeyA);
  Preferences::SetCString("prio.publicKeyB", prioKeyB);

  nsTArray<Tuple<nsCString, nsCString>> firstStrings;
  GetEncodedOriginStrings(aCx, kTelemetryTest1Metric + "-%u"_ns, firstStrings);

  // Now snapshot a second time and ensure the encoded payloads change.
  nsTArray<Tuple<nsCString, nsCString>> secondStrings;
  GetEncodedOriginStrings(aCx, kTelemetryTest1Metric + "-%u"_ns, secondStrings);

  const auto sizeOfPrioDatasPerMetric =
      TelemetryOrigin::SizeOfPrioDatasPerMetric();
  ASSERT_EQ(sizeOfPrioDatasPerMetric, firstStrings.Length());
  ASSERT_EQ(sizeOfPrioDatasPerMetric, secondStrings.Length());

  for (size_t i = 0; i < sizeOfPrioDatasPerMetric; ++i) {
    auto& aStr = Get<0>(firstStrings[i]);
    auto& bStr = Get<1>(firstStrings[i]);
    auto& secondAStr = Get<0>(secondStrings[i]);
    auto& secondBStr = Get<1>(secondStrings[i]);

    ASSERT_TRUE(aStr != secondAStr)
    << "aStr (" << aStr.get() << ") must not equal secondAStr ("
    << secondAStr.get() << ")";
    ASSERT_TRUE(bStr != secondBStr)
    << "bStr (" << bStr.get() << ") must not equal secondBStr ("
    << secondBStr.get() << ")";
  }
}

class MockObserver final : public nsIObserver {
 public:
  NS_DECL_ISUPPORTS

  MOCK_METHOD1(Mobserve, void(const char* aTopic));
  NS_IMETHOD Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) override {
    Mobserve(aTopic);
    return NS_OK;
  };

  MockObserver() = default;

 private:
  ~MockObserver() = default;
};

NS_IMPL_ISUPPORTS(MockObserver, nsIObserver);

TEST_F(TelemetryTestFixture, OriginTelemetryNotifiesTopic) {
  Unused << mTelemetry->ClearOrigins();

  const char* kTopic = "origin-telemetry-storage-limit-reached";

  MockObserver* mo = new MockObserver();
  nsCOMPtr<nsIObserver> nsMo(mo);
  EXPECT_CALL(*mo, Mobserve(StrEq(kTopic))).Times(1);

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  ASSERT_TRUE(os);
  os->AddObserver(nsMo, kTopic, false);

  const size_t size = ceil(10.0 / TelemetryOrigin::SizeOfPrioDatasPerMetric());
  for (size_t i = 0; i < size; ++i) {
    if (i < size - 1) {
      // Let's ensure we only notify the once.
      Telemetry::RecordOrigin(OriginMetricID::TelemetryTest_Test1,
                              kFacebookOrigin);
    }
    Telemetry::RecordOrigin(OriginMetricID::TelemetryTest_Test1,
                            kDoubleclickOrigin);
  }

  os->RemoveObserver(nsMo, kTopic);
}
