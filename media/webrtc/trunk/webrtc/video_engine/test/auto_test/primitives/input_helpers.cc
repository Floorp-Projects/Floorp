/*
 *  Copyright (c) 2012 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/video_engine/test/auto_test/primitives/input_helpers.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include <algorithm>

#include "gflags/gflags.h"

namespace webrtc {

DEFINE_string(override, "",
    "Makes it possible to override choices or inputs. All choices and "
    "inputs will use their default values unless you override them in this "
    "flag's argument. There can be several comma-separated overrides specified:"
    " Overrides are specified as \"title=option text\" for choices and "
    "\"title=value\" for regular inputs. Note that the program will stop if "
    "you provide input not accepted by the input's validator through this flag."
    "\n\nExample: --override \"Enter destination IP=192.168.0.1, "
    "Select a codec=VP8\"");

class AcceptAllNonEmptyValidator : public InputValidator {
 public:
  bool InputOk(const std::string& value) const {
    return value.length() > 0;
  }
};

InputBuilder::InputBuilder(const std::string& title,
                           const InputValidator* input_validator,
                           const OverrideRegistry& override_registry)
    : input_source_(stdin), input_validator_(input_validator),
      override_registry_(override_registry), default_value_(""), title_(title) {
}

InputBuilder::~InputBuilder() {
  delete input_validator_;
}

std::string InputBuilder::AskForInput() const {
  if (override_registry_.HasOverrideFor(title_))
    return GetOverride();
  if (!FLAGS_override.empty() && !default_value_.empty())
    return default_value_;

  // We don't know the answer already, so ask the user.
  return ActuallyAskUser();
}

std::string InputBuilder::ActuallyAskUser() const {
  printf("\n%s%s\n", title_.c_str(), additional_info_.c_str());

  if (!default_value_.empty())
    printf("Hit enter for default (%s):\n", default_value_.c_str());

  printf("# ");
  char raw_input[128];
  if (!fgets(raw_input, 128, input_source_)) {
    // If we get here the user probably hit CTRL+D.
    exit(1);
  }

  std::string input = raw_input;
  input = input.substr(0, input.size() - 1);  // Strip last \n.

  if (input.empty() && !default_value_.empty())
    return default_value_;

  if (!input_validator_->InputOk(input)) {
    printf("Invalid input. Please try again.\n");
    return ActuallyAskUser();
  }
  return input;
}

InputBuilder& InputBuilder::WithInputSource(FILE* input_source) {
  input_source_ = input_source;
  return *this;
}

InputBuilder& InputBuilder::WithInputValidator(
    const InputValidator* input_validator) {
  // If there's a default value, it must be accepted by the input validator.
  assert(default_value_.empty() || input_validator->InputOk(default_value_));
  delete input_validator_;
  input_validator_ = input_validator;
  return *this;
}

InputBuilder& InputBuilder::WithDefault(const std::string& default_value) {
  assert(input_validator_->InputOk(default_value));
  default_value_ = default_value;
  return *this;
}

InputBuilder& InputBuilder::WithAdditionalInfo(const std::string& info) {
  additional_info_ = info;
  return *this;
}

const std::string& InputBuilder::GetOverride() const {
  const std::string& override = override_registry_.GetOverrideFor(title_);
  if (!input_validator_->InputOk(override)) {
    printf("Fatal: Input validator for \"%s\" does not accept override %s.\n",
        title_.c_str(), override.c_str());
    exit(1);
  }
  return override;
}

OverrideRegistry::OverrideRegistry(const std::string& overrides) {
  std::vector<std::string> all_overrides = Split(overrides, ",");
  std::vector<std::string>::const_iterator override = all_overrides.begin();
  for (; override != all_overrides.end(); ++override) {
    std::vector<std::string> key_value = Split(*override, "=");
    if (key_value.size() != 2) {
      printf("Fatal: Override %s is malformed.", (*override).c_str());
      exit(1);
    }
    std::string key = key_value[0];
    std::string value = key_value[1];
    overrides_[key] = value;
  }
}

bool OverrideRegistry::HasOverrideFor(const std::string& title) const {
  return overrides_.find(title) != overrides_.end();
}

const std::string& OverrideRegistry::GetOverrideFor(
    const std::string& title) const {
  assert(HasOverrideFor(title));
  return (*overrides_.find(title)).second;
}

InputBuilder TypedInput(const std::string& title) {
  static OverrideRegistry override_registry_(FLAGS_override);
  return InputBuilder(
      title, new AcceptAllNonEmptyValidator(), override_registry_);
}

std::vector<std::string> Split(const std::string& to_split,
                               const std::string& delimiter) {
  std::vector<std::string> result;
  size_t current_pos = 0;
  size_t next_delimiter = 0;
  while ((next_delimiter = to_split.find(delimiter, current_pos)) !=
      std::string::npos) {
    std::string part = to_split.substr(
        current_pos, next_delimiter - current_pos);
    result.push_back(part);
    current_pos = next_delimiter + 1;
  }
  std::string last_part = to_split.substr(current_pos);
  if (!last_part.empty())
    result.push_back(last_part);

  return result;
}

}  // namespace webrtc
