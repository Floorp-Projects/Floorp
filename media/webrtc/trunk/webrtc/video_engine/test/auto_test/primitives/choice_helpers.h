/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VIDEO_ENGINE_TEST_AUTO_TEST_PRIMITIVES_CHOICE_HELPERS_H_
#define WEBRTC_VIDEO_ENGINE_TEST_AUTO_TEST_PRIMITIVES_CHOICE_HELPERS_H_

#include <string>
#include <vector>

#include "video_engine/test/auto_test/primitives/input_helpers.h"

namespace webrtc {

typedef std::vector<std::string> Choices;

/**
 * Used to ask the user to make a choice. This class will allow you to
 * configure how to ask the question, and then ask it. For instance,
 *
 * int choice = FromChoices("Choice 1\n"
 *                          "Choice 2\n").WithDefault("Choice 1").Choose();
 *
 * will print a menu presenting the two choices and ask for input. The user,
 * can input 1, 2 or just hit enter since we specified a default in this case.
 * The Choose call will block until the user gives valid input one way or the
 * other. The choice variable is guaranteed to contain either 1 or 2 after
 * this particular call.
 *
 * The class uses stdout and stdin by default, but stdin can be replaced using
 * WithInputSource for unit tests.
 */
class ChoiceBuilder {
 public:
  explicit ChoiceBuilder(const std::string& title, const Choices& choices);

  // Specifies the choice as the default. The choice must be one of the choices
  // passed in the constructor. If this method is not called, the user has to
  // choose an option explicitly.
  ChoiceBuilder& WithDefault(const std::string& default_choice);

  // Replaces the input source where we ask for input. Default is stdin.
  ChoiceBuilder& WithInputSource(FILE* input_source);

  // Prints the choice list and requests input from the input source. Returns
  // the choice number (choices start at 1).
  int Choose();
 private:
  std::string MakeHumanReadableOptions();

  Choices choices_;
  InputBuilder input_helper_;
};

// Convenience function that creates a choice builder given a string where
// choices are separated by \n.
ChoiceBuilder FromChoices(const std::string& title,
                          const std::string& raw_choices);

// Creates choices from a string where choices are separated by \n.
Choices SplitChoices(const std::string& raw_choices);

}  // namespace webrtc

#endif  // CHOICE_HELPERS_H_
