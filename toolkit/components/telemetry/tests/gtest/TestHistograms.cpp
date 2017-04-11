/* vim:set ts=2 sw=2 sts=0 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gtest/gtest.h"
#include "js/Conversions.h"
#include "nsITelemetry.h"
#include "Telemetry.h"
#include "TelemetryFixture.h"

TEST_F(TelemetryTestFixture, AccumulateCountHistogram)
{
  const uint32_t kExpectedValue = 100;
  AutoJSContextWithGlobal cx(mCleanGlobal);

  JS::RootedValue testHistogram(cx.GetJSContext());
  JS::RootedValue rval(cx.GetJSContext());

  // Get the histogram
  nsresult rv = mTelemetry->GetHistogramById(
      NS_LITERAL_CSTRING("TELEMETRY_TEST_COUNT"),
      cx.GetJSContext(),
      &testHistogram);
  ASSERT_EQ(rv, NS_OK) << "Cannot fetch histogram";

  // Clear the stored value
  JS::RootedObject testHistogramObj(cx.GetJSContext(), &testHistogram.toObject());
  ASSERT_TRUE(JS_CallFunctionName(cx.GetJSContext(), testHistogramObj, "clear",
                  JS::HandleValueArray::empty(), &rval)) << "Cannot clear histogram";

  // Accumulate in the histogram
  Telemetry::Accumulate(Telemetry::TELEMETRY_TEST_COUNT, kExpectedValue);

  // Get a snapshot for all the histograms
  JS::RootedValue snapshot(cx.GetJSContext());
  rv = mTelemetry->GetHistogramSnapshots(cx.GetJSContext(), &snapshot);
  ASSERT_EQ(rv, NS_OK) << "Cannot call histogram snapshots";

  // Get the histogram from the snapshot
  JS::RootedValue histogram(cx.GetJSContext());
  JS::RootedObject snapshotObj(cx.GetJSContext(), &snapshot.toObject());
  JS_GetProperty(cx.GetJSContext(), snapshotObj, "TELEMETRY_TEST_COUNT", &histogram);

  // Get the snapshot for the test histogram
  JS::RootedValue sum(cx.GetJSContext());
  JS::RootedObject histogramObj(cx.GetJSContext(), &histogram.toObject());
  JS_GetProperty(cx.GetJSContext(), histogramObj, "sum", &sum);

  // Check that the value stored in the histogram matches with |kExpectedValue|
  uint32_t uSum = 0;
  JS::ToUint32(cx.GetJSContext(), sum, &uSum);
  ASSERT_EQ(uSum, kExpectedValue) << "The histogram is not returning expected value";
}

TEST_F(TelemetryTestFixture, AccumulateKeyedCountHistogram)
{
  const uint32_t kExpectedValue = 100;
  AutoJSContextWithGlobal cx(mCleanGlobal);

  JS::RootedValue testHistogram(cx.GetJSContext());
  JS::RootedValue rval(cx.GetJSContext());

  // Get the histogram
  nsresult rv = mTelemetry->GetKeyedHistogramById(
      NS_LITERAL_CSTRING("TELEMETRY_TEST_KEYED_COUNT"),
      cx.GetJSContext(),
      &testHistogram);
  ASSERT_EQ(rv, NS_OK) << "Cannot fetch histogram";

  // Clear the stored value
  JS::RootedObject testHistogramObj(cx.GetJSContext(), &testHistogram.toObject());
  ASSERT_TRUE(JS_CallFunctionName(cx.GetJSContext(), testHistogramObj, "clear",
    JS::HandleValueArray::empty(), &rval)) << "Cannot clear histogram";

  // Accumulate data in the provided key within the histogram
  Telemetry::Accumulate(Telemetry::TELEMETRY_TEST_KEYED_COUNT, NS_LITERAL_CSTRING("sample"),
                        kExpectedValue);

  // Get a snapshot for all the histograms
  JS::RootedValue snapshot(cx.GetJSContext());
  rv = mTelemetry->GetKeyedHistogramSnapshots(cx.GetJSContext(), &snapshot);
  ASSERT_EQ(rv, NS_OK) << "Cannot call histogram snapshots";

  // Get the histogram from the snapshot
  JS::RootedValue histogram(cx.GetJSContext());
  JS::RootedObject snapshotObj(cx.GetJSContext(), &snapshot.toObject());
  ASSERT_TRUE(JS_GetProperty(cx.GetJSContext(), snapshotObj, "TELEMETRY_TEST_KEYED_COUNT",
                             &histogram)) << "Cannot find the expected histogram";

  // Get histogram from keyed data
  JS::RootedObject histogramObj(cx.GetJSContext(), &histogram.toObject());
  JS::RootedValue expectedKeyData(cx.GetJSContext());
  ASSERT_TRUE(JS_GetProperty(cx.GetJSContext(), histogramObj, "sample", &expectedKeyData))
    << "Cannot find the expected key in the histogram data";

  // Get sum from keyed data
  JS::RootedObject expectedKeyDataObj(cx.GetJSContext(), &expectedKeyData.toObject());
  JS::RootedValue sum(cx.GetJSContext());
  ASSERT_TRUE(JS_GetProperty(cx.GetJSContext(), expectedKeyDataObj, "sum", &sum))
    << "Cannot find the 'sum' property in the data for this key";

  // Check that the sum stored in the histogram matches with |kExpectedValue|
  uint32_t uSum = 0;
  JS::ToUint32(cx.GetJSContext(), sum, &uSum);
  ASSERT_EQ(uSum, kExpectedValue) << "The histogram is not returning expected sum";
}
