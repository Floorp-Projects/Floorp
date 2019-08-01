/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "TelemetryFixture.h"
#include "streaming/GeckoViewStreamingTelemetry.h"

using namespace mozilla;
using namespace mozilla::Telemetry;
using namespace TelemetryTestHelpers;
using GeckoViewStreamingTelemetry::StreamingTelemetryDelegate;
using ::testing::_;
using ::testing::Eq;

namespace {

const char* kGeckoViewStreamingPref = "toolkit.telemetry.geckoview.streaming";
const char* kBatchTimeoutPref = "toolkit.telemetry.geckoview.batchDurationMS";

class MockDelegate final : public StreamingTelemetryDelegate {
 public:
  MOCK_METHOD2(ReceiveHistogramSamples,
               void(const nsCString& aHistogramName,
                    const nsTArray<uint32_t>& aSamples));
};  // class MockDelegate

TEST_F(TelemetryTestFixture, StreamingTelemetryStreamsHistogramSamples) {
  NS_NAMED_LITERAL_CSTRING(kTestHgramName, "TELEMETRY_TEST_STREAMING");
  const uint32_t kSampleOne = 401;
  const uint32_t kSampleTwo = 2019;

  nsTArray<uint32_t> samplesArray;
  samplesArray.AppendElement(kSampleOne);
  samplesArray.AppendElement(kSampleTwo);

  auto md = MakeRefPtr<MockDelegate>();
  EXPECT_CALL(*md, ReceiveHistogramSamples(Eq(kTestHgramName),
                                           Eq(std::move(samplesArray))));
  GeckoViewStreamingTelemetry::RegisterDelegate(md);

  Preferences::SetBool(kGeckoViewStreamingPref, true);
  Preferences::SetInt(kBatchTimeoutPref, 5000);

  Telemetry::Accumulate(Telemetry::TELEMETRY_TEST_STREAMING, kSampleOne);
  Preferences::SetInt(kBatchTimeoutPref, 0);
  Telemetry::Accumulate(Telemetry::TELEMETRY_TEST_STREAMING, kSampleTwo);

  // Clear the delegate so when the batch is done being sent it can be deleted.
  GeckoViewStreamingTelemetry::RegisterDelegate(nullptr);
}

}  // namespace
