/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/video_engine/test/auto_test/primitives/choice_helpers.h"
#include "webrtc/video_engine/test/auto_test/primitives/fake_stdin.h"

namespace webrtc {

class ChoiceHelpersTest : public testing::Test {
};

TEST_F(ChoiceHelpersTest, SplitReturnsEmptyChoicesForEmptyInput) {
  EXPECT_TRUE(SplitChoices("").empty());
}

TEST_F(ChoiceHelpersTest, SplitHandlesSingleChoice) {
  Choices choices = SplitChoices("Single Choice");
  EXPECT_EQ(1u, choices.size());
  EXPECT_EQ("Single Choice", choices[0]);
}

TEST_F(ChoiceHelpersTest, SplitHandlesSingleChoiceWithEndingNewline) {
  Choices choices = SplitChoices("Single Choice\n");
  EXPECT_EQ(1u, choices.size());
  EXPECT_EQ("Single Choice", choices[0]);
}

TEST_F(ChoiceHelpersTest, SplitHandlesMultipleChoices) {
  Choices choices = SplitChoices(
      "Choice 1\n"
      "Choice 2\n"
      "Choice 3");
  EXPECT_EQ(3u, choices.size());
  EXPECT_EQ("Choice 1", choices[0]);
  EXPECT_EQ("Choice 2", choices[1]);
  EXPECT_EQ("Choice 3", choices[2]);
}

TEST_F(ChoiceHelpersTest, SplitHandlesMultipleChoicesWithEndingNewline) {
  Choices choices = SplitChoices(
      "Choice 1\n"
      "Choice 2\n"
      "Choice 3\n");
  EXPECT_EQ(3u, choices.size());
  EXPECT_EQ("Choice 1", choices[0]);
  EXPECT_EQ("Choice 2", choices[1]);
  EXPECT_EQ("Choice 3", choices[2]);
}

TEST_F(ChoiceHelpersTest, CanSelectUsingChoiceBuilder) {
  FILE* fake_stdin = FakeStdin("1\n2\n");
  EXPECT_EQ(1, FromChoices("Title",
                           "Choice 1\n"
                           "Choice 2").WithInputSource(fake_stdin).Choose());
  EXPECT_EQ(2, FromChoices("","Choice 1\n"
                           "Choice 2").WithInputSource(fake_stdin).Choose());
  fclose(fake_stdin);
}

TEST_F(ChoiceHelpersTest, RetriesIfGivenInvalidChoice) {
  FILE* fake_stdin = FakeStdin("3\n0\n99\n23409234809\na\nwhatever\n1\n");
  EXPECT_EQ(1, FromChoices("Title",
                           "Choice 1\n"
                           "Choice 2").WithInputSource(fake_stdin).Choose());
  fclose(fake_stdin);
}

TEST_F(ChoiceHelpersTest, RetriesOnEnterIfNoDefaultSet) {
  FILE* fake_stdin = FakeStdin("\n2\n");
  EXPECT_EQ(2, FromChoices("Title",
                           "Choice 1\n"
                           "Choice 2").WithInputSource(fake_stdin).Choose());
  fclose(fake_stdin);
}

TEST_F(ChoiceHelpersTest, PicksDefaultOnEnterIfDefaultSet) {
  FILE* fake_stdin = FakeStdin("\n");
  EXPECT_EQ(2, FromChoices("Title",
                           "Choice 1\n"
                           "Choice 2").WithInputSource(fake_stdin)
                               .WithDefault("Choice 2").Choose());
  fclose(fake_stdin);
}

}  // namespace webrtc
