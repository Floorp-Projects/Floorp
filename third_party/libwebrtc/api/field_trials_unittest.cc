/*
 *  Copyright 2022 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "api/field_trials.h"

#include "api/transport/field_trial_based_config.h"
#include "system_wrappers/include/field_trial.h"
#include "test/gtest.h"

#if GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
#include "test/testsupport/rtc_expect_death.h"
#endif  // GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

namespace webrtc {

TEST(FieldTrials, EmptyString) {
  FieldTrials f("");
  EXPECT_FALSE(f.IsEnabled("MyCoolTrial"));
  EXPECT_FALSE(f.IsDisabled("MyCoolTrial"));
}

TEST(FieldTrials, EnableDisable) {
  FieldTrials f("MyCoolTrial/Enabled/MyUncoolTrial/Disabled/");
  EXPECT_TRUE(f.IsEnabled("MyCoolTrial"));
  EXPECT_TRUE(f.IsDisabled("MyUncoolTrial"));
}

TEST(FieldTrials, SetGlobalStringAndReadFromFieldTrial) {
  const char* s = "MyCoolTrial/Enabled/MyUncoolTrial/Disabled/";
  webrtc::field_trial::InitFieldTrialsFromString(s);
  FieldTrialBasedConfig f;
  EXPECT_TRUE(f.IsEnabled("MyCoolTrial"));
  EXPECT_TRUE(f.IsDisabled("MyUncoolTrial"));
}

TEST(FieldTrials, SetFieldTrialAndReadFromGlobalString) {
  FieldTrials f("MyCoolTrial/Enabled/MyUncoolTrial/Disabled/");
  EXPECT_TRUE(webrtc::field_trial::IsEnabled("MyCoolTrial"));
  EXPECT_TRUE(webrtc::field_trial::IsDisabled("MyUncoolTrial"));
}

TEST(FieldTrials, RestoresGlobalString) {
  const char* s = "SomeString/Enabled/";
  webrtc::field_trial::InitFieldTrialsFromString(s);
  {
    FieldTrials f("SomeOtherString/Enabled/");
    EXPECT_EQ(std::string("SomeOtherString/Enabled/"),
              webrtc::field_trial::GetFieldTrialString());
  }
  EXPECT_EQ(s, webrtc::field_trial::GetFieldTrialString());
}

#if GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)
TEST(FieldTrials, OnlyOneInstance) {
  FieldTrials f("SomeString/Enabled/");
  RTC_EXPECT_DEATH(FieldTrials("SomeOtherString/Enabled/").Lookup("Whatever"),
                   "Only one instance");
}
#endif  // GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

TEST(FieldTrials, SequentialInstances) {
  { FieldTrials f("SomeString/Enabled/"); }
  { FieldTrials f("SomeOtherString/Enabled/"); }
}

}  // namespace webrtc
