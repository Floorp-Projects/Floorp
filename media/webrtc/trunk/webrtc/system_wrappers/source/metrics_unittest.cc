/*
 *  Copyright (c) 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"

#include "webrtc/system_wrappers/include/metrics.h"
#include "webrtc/test/histogram.h"

namespace webrtc {
namespace {
const int kSample = 22;
const std::string kName = "Name";

void AddSparseSample(const std::string& name, int sample) {
  RTC_HISTOGRAM_COUNTS_SPARSE_100(name, sample);
}
#if GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
void AddSample(const std::string& name, int sample) {
  RTC_HISTOGRAM_COUNTS_100(name, sample);
}
#endif  // GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
}  // namespace

TEST(MetricsTest, InitiallyNoSamples) {
  test::ClearHistograms();
  EXPECT_EQ(0, test::NumHistogramSamples(kName));
  EXPECT_EQ(-1, test::LastHistogramSample(kName));
}

TEST(MetricsTest, RtcHistogramPercent_AddSample) {
  test::ClearHistograms();
  RTC_HISTOGRAM_PERCENTAGE(kName, kSample);
  EXPECT_EQ(1, test::NumHistogramSamples(kName));
  EXPECT_EQ(kSample, test::LastHistogramSample(kName));
}

TEST(MetricsTest, RtcHistogramEnumeration_AddSample) {
  test::ClearHistograms();
  RTC_HISTOGRAM_ENUMERATION(kName, kSample, kSample + 1);
  EXPECT_EQ(1, test::NumHistogramSamples(kName));
  EXPECT_EQ(kSample, test::LastHistogramSample(kName));
}

TEST(MetricsTest, RtcHistogramCountsSparse_AddSample) {
  test::ClearHistograms();
  RTC_HISTOGRAM_COUNTS_SPARSE_100(kName, kSample);
  EXPECT_EQ(1, test::NumHistogramSamples(kName));
  EXPECT_EQ(kSample, test::LastHistogramSample(kName));
}

TEST(MetricsTest, RtcHistogramCounts_AddSample) {
  test::ClearHistograms();
  RTC_HISTOGRAM_COUNTS_100(kName, kSample);
  EXPECT_EQ(1, test::NumHistogramSamples(kName));
  EXPECT_EQ(kSample, test::LastHistogramSample(kName));
}

TEST(MetricsTest, RtcHistogramCounts_AddMultipleSamples) {
  test::ClearHistograms();
  const int kNumSamples = 10;
  for (int i = 0; i < kNumSamples; ++i) {
    RTC_HISTOGRAM_COUNTS_100(kName, i);
  }
  EXPECT_EQ(kNumSamples, test::NumHistogramSamples(kName));
  EXPECT_EQ(kNumSamples - 1, test::LastHistogramSample(kName));
}

TEST(MetricsTest, RtcHistogramSparse_NonConstantNameWorks) {
  test::ClearHistograms();
  AddSparseSample("Name1", kSample);
  AddSparseSample("Name2", kSample);
  EXPECT_EQ(1, test::NumHistogramSamples("Name1"));
  EXPECT_EQ(1, test::NumHistogramSamples("Name2"));
}

#if GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(MetricsTest, RtcHistogram_FailsForNonConstantName) {
  test::ClearHistograms();
  AddSample("Name1", kSample);
  EXPECT_DEATH(AddSample("Name2", kSample), "");
}
#endif  // GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

}  // namespace webrtc
