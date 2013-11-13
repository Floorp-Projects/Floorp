/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/test/auto_test/primitives/choice_helpers.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>
#include <sstream>

namespace webrtc {

ChoiceBuilder::ChoiceBuilder(const std::string& title, const Choices& choices)
    : choices_(choices),
      input_helper_(TypedInput(title)) {
  input_helper_.WithInputValidator(
      new IntegerWithinRangeValidator(1, choices.size()));
  input_helper_.WithAdditionalInfo(MakeHumanReadableOptions());
}

int ChoiceBuilder::Choose() {
  std::string input = input_helper_.AskForInput();
  return atoi(input.c_str());
}

ChoiceBuilder& ChoiceBuilder::WithDefault(const std::string& default_choice) {
  Choices::const_iterator iterator = std::find(
      choices_.begin(), choices_.end(), default_choice);
  assert(iterator != choices_.end() && "No such choice.");

  // Store the value as the choice number, e.g. its index + 1.
  int choice_index = (iterator - choices_.begin()) + 1;
  char number[16];
  sprintf(number, "%d", choice_index);

  input_helper_.WithDefault(number);
  return *this;
}

ChoiceBuilder& ChoiceBuilder::WithInputSource(FILE* input_source) {
  input_helper_.WithInputSource(input_source);
  return *this;
}

std::string ChoiceBuilder::MakeHumanReadableOptions() {
  std::string result = "";
  Choices::const_iterator iterator = choices_.begin();
  for (int number = 1; iterator != choices_.end(); ++iterator, ++number) {
    std::ostringstream os;
    os << "\n  " << number << ". " << (*iterator).c_str();
    result += os.str();
  }
  return result;
}

Choices SplitChoices(const std::string& raw_choices) {
  return Split(raw_choices, "\n");
}

ChoiceBuilder FromChoices(
    const std::string& title, const std::string& raw_choices) {
  return ChoiceBuilder(title, SplitChoices(raw_choices));
}

}  // namespace webrtc
