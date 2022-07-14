/* vim:set ts=2 sw=2 sts=0 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"
#include "js/Conversions.h"
#include "mozilla/Telemetry.h"
#include "TelemetryFixture.h"
#include "TelemetryTestHelpers.h"

using namespace mozilla;
using namespace TelemetryTestHelpers;

TEST_F(TelemetryTestFixture, AutoCounter) {
  const uint32_t kExpectedValue = 100;
  AutoJSContextWithGlobal cx(mCleanGlobal);

  const char* telemetryTestCountName =
      Telemetry::GetHistogramName(Telemetry::TELEMETRY_TEST_COUNT);

  GetAndClearHistogram(cx.GetJSContext(), mTelemetry, "TELEMETRY_TEST_COUNT"_ns,
                       false);

  // Accumulate in the histogram
  {
    Telemetry::AutoCounter<Telemetry::TELEMETRY_TEST_COUNT> autoCounter;
    autoCounter += kExpectedValue / 2;
  }
  // This counter should not accumulate since it does not go out of scope
  Telemetry::AutoCounter<Telemetry::TELEMETRY_TEST_COUNT> autoCounter;
  autoCounter += kExpectedValue;
  // Accumulate a second time in the histogram
  {
    Telemetry::AutoCounter<Telemetry::TELEMETRY_TEST_COUNT> autoCounter;
    autoCounter += kExpectedValue / 2;
  }

  // Get a snapshot for all the histograms
  JS::Rooted<JS::Value> snapshot(cx.GetJSContext());
  GetSnapshots(cx.GetJSContext(), mTelemetry, telemetryTestCountName, &snapshot,
               false);

  // Get the histogram from the snapshot
  JS::Rooted<JS::Value> histogram(cx.GetJSContext());
  GetProperty(cx.GetJSContext(), telemetryTestCountName, snapshot, &histogram);

  // Get "sum" property from histogram
  JS::Rooted<JS::Value> sum(cx.GetJSContext());
  GetProperty(cx.GetJSContext(), "sum", histogram, &sum);

  // Check that the "sum" stored in the histogram matches with |kExpectedValue|
  uint32_t uSum = 0;
  JS::ToUint32(cx.GetJSContext(), sum, &uSum);
  ASSERT_EQ(uSum, kExpectedValue)
      << "The histogram is not returning expected value";
}

TEST_F(TelemetryTestFixture, AutoCounterUnderflow) {
  const uint32_t kExpectedValue = 0;
  AutoJSContextWithGlobal cx(mCleanGlobal);

  const char* telemetryTestCountName =
      Telemetry::GetHistogramName(Telemetry::TELEMETRY_TEST_COUNT);

  GetAndClearHistogram(cx.GetJSContext(), mTelemetry, "TELEMETRY_TEST_COUNT"_ns,
                       false);

  // Accumulate in the histogram
  {
    Telemetry::AutoCounter<Telemetry::TELEMETRY_TEST_COUNT> autoCounter;
    autoCounter += -1;
  }

  // Get a snapshot for all the histograms
  JS::Rooted<JS::Value> snapshot(cx.GetJSContext());
  GetSnapshots(cx.GetJSContext(), mTelemetry, telemetryTestCountName, &snapshot,
               false);

  // Get the histogram from the snapshot
  JS::Rooted<JS::Value> histogram(cx.GetJSContext());
  GetProperty(cx.GetJSContext(), telemetryTestCountName, snapshot, &histogram);

  // Get "sum" property from histogram
  JS::Rooted<JS::Value> sum(cx.GetJSContext());
  GetProperty(cx.GetJSContext(), "sum", histogram, &sum);

  // Check that the "sum" stored in the histogram matches with |kExpectedValue|
  uint32_t uSum = 42;
  JS::ToUint32(cx.GetJSContext(), sum, &uSum);
  ASSERT_EQ(uSum, kExpectedValue)
      << "The histogram is supposed to return 0 when an underflow occurs.";
}

TEST_F(TelemetryTestFixture, RuntimeAutoCounter) {
  const uint32_t kExpectedValue = 100;
  AutoJSContextWithGlobal cx(mCleanGlobal);

  const char* telemetryTestCountName =
      Telemetry::GetHistogramName(Telemetry::TELEMETRY_TEST_COUNT);

  GetAndClearHistogram(cx.GetJSContext(), mTelemetry, "TELEMETRY_TEST_COUNT"_ns,
                       false);

  // Accumulate in the histogram
  {
    Telemetry::RuntimeAutoCounter autoCounter(Telemetry::TELEMETRY_TEST_COUNT);
    autoCounter += kExpectedValue / 2;
  }
  // This counter should not accumulate since it does not go out of scope
  Telemetry::RuntimeAutoCounter autoCounter(Telemetry::TELEMETRY_TEST_COUNT);
  autoCounter += kExpectedValue;
  // Accumulate a second time in the histogram
  {
    Telemetry::RuntimeAutoCounter autoCounter(Telemetry::TELEMETRY_TEST_COUNT);
    autoCounter += kExpectedValue / 2;
  }
  // Get a snapshot for all the histograms
  JS::Rooted<JS::Value> snapshot(cx.GetJSContext());
  GetSnapshots(cx.GetJSContext(), mTelemetry, telemetryTestCountName, &snapshot,
               false);

  // Get the histogram from the snapshot
  JS::Rooted<JS::Value> histogram(cx.GetJSContext());
  GetProperty(cx.GetJSContext(), telemetryTestCountName, snapshot, &histogram);

  // Get "sum" property from histogram
  JS::Rooted<JS::Value> sum(cx.GetJSContext());
  GetProperty(cx.GetJSContext(), "sum", histogram, &sum);

  // Check that the "sum" stored in the histogram matches with |kExpectedValue|
  uint32_t uSum = 0;
  JS::ToUint32(cx.GetJSContext(), sum, &uSum);
  ASSERT_EQ(uSum, kExpectedValue)
      << "The histogram is not returning expected value";
}

TEST_F(TelemetryTestFixture, RuntimeAutoCounterUnderflow) {
  const uint32_t kExpectedValue = 0;
  AutoJSContextWithGlobal cx(mCleanGlobal);

  const char* telemetryTestCountName =
      Telemetry::GetHistogramName(Telemetry::TELEMETRY_TEST_COUNT);

  GetAndClearHistogram(cx.GetJSContext(), mTelemetry, "TELEMETRY_TEST_COUNT"_ns,
                       false);

  // Accumulate in the histogram
  {
    Telemetry::RuntimeAutoCounter autoCounter(Telemetry::TELEMETRY_TEST_COUNT,
                                              kExpectedValue);
    autoCounter += -1;
  }

  // Get a snapshot for all the histograms
  JS::Rooted<JS::Value> snapshot(cx.GetJSContext());
  GetSnapshots(cx.GetJSContext(), mTelemetry, telemetryTestCountName, &snapshot,
               false);

  // Get the histogram from the snapshot
  JS::Rooted<JS::Value> histogram(cx.GetJSContext());
  GetProperty(cx.GetJSContext(), telemetryTestCountName, snapshot, &histogram);

  // Get "sum" property from histogram
  JS::Rooted<JS::Value> sum(cx.GetJSContext());
  GetProperty(cx.GetJSContext(), "sum", histogram, &sum);

  // Check that the "sum" stored in the histogram matches with |kExpectedValue|
  uint32_t uSum = 42;
  JS::ToUint32(cx.GetJSContext(), sum, &uSum);
  ASSERT_EQ(uSum, kExpectedValue)
      << "The histogram is supposed to return 0 when an underflow occurs.";
}
