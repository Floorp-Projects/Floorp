/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/video_engine/test/auto_test/primitives/fake_stdin.h"
#include "webrtc/video_engine/test/auto_test/primitives/input_helpers.h"

namespace webrtc {

class InputHelpersTest: public testing::Test {
};

TEST_F(InputHelpersTest, AcceptsAnyInputExceptEmptyByDefault) {
  FILE* fake_stdin = FakeStdin("\n\nWhatever\n");
  std::string result = TypedInput("Title")
      .WithInputSource(fake_stdin).AskForInput();
  EXPECT_EQ("Whatever", result);
  fclose(fake_stdin);
}

TEST_F(InputHelpersTest, ReturnsDefaultOnEmptyInputIfDefaultSet) {
  FILE* fake_stdin = FakeStdin("\n\nWhatever\n");
  std::string result = TypedInput("Title")
      .WithInputSource(fake_stdin)
      .WithDefault("MyDefault")
      .AskForInput();
  EXPECT_EQ("MyDefault", result);
  fclose(fake_stdin);
}

TEST_F(InputHelpersTest, ObeysInputValidator) {
  class ValidatorWhichOnlyAcceptsFooBar : public InputValidator {
   public:
    bool InputOk(const std::string& input) const {
      return input == "FooBar";
    }
  };
  FILE* fake_stdin = FakeStdin("\nFoo\nBar\nFoo Bar\nFooBar\n");
  std::string result = TypedInput("Title")
      .WithInputSource(fake_stdin)
      .WithInputValidator(new ValidatorWhichOnlyAcceptsFooBar())
      .AskForInput();
  EXPECT_EQ("FooBar", result);
  fclose(fake_stdin);
}

TEST_F(InputHelpersTest, OverrideRegistryParsesOverridesCorrectly) {
  // TODO(phoglund): Ignore spaces where appropriate
  OverrideRegistry override_registry("My Title=Value,My Choice=1");
  EXPECT_TRUE(override_registry.HasOverrideFor("My Title"));
  EXPECT_EQ("Value", override_registry.GetOverrideFor("My Title"));
  EXPECT_TRUE(override_registry.HasOverrideFor("My Choice"));
  EXPECT_EQ("1", override_registry.GetOverrideFor("My Choice"));
  EXPECT_FALSE(override_registry.HasOverrideFor("Not Overridden"));
}

TEST_F(InputHelpersTest, ObeysOverridesBeforeAnythingElse) {
  class CarelessValidator : public InputValidator {
  public:
    bool InputOk(const std::string& input) const {
      return true;
    }
  };
  FILE* fake_stdin = FakeStdin("\nFoo\nBar\nFoo Bar\nFooBar\n");
  OverrideRegistry override_registry("My Title=Value,My Choice=1");
  EXPECT_EQ("Value", InputBuilder("My Title",
      new CarelessValidator(), override_registry)
          .WithDefault("Whatever")
          .WithInputSource(fake_stdin).AskForInput());
  fclose(fake_stdin);
}

};
