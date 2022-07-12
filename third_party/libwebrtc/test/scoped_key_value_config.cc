/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "test/scoped_key_value_config.h"

#include "api/webrtc_key_value_config.h"
#include "rtc_base/checks.h"
#include "test/field_trial.h"

namespace {

// This part is copied from system_wrappers/field_trial.cc.
void InsertIntoMap(std::map<std::string, std::string>& key_value_map,
                   const std::string& s) {
  std::string::size_type field_start = 0;
  while (field_start < s.size()) {
    std::string::size_type separator_pos = s.find('/', field_start);
    RTC_CHECK_NE(separator_pos, std::string::npos)
        << "Missing separator '/' after field trial key.";
    RTC_CHECK_GT(separator_pos, field_start)
        << "Field trial key cannot be empty.";
    std::string key = s.substr(field_start, separator_pos - field_start);
    field_start = separator_pos + 1;

    RTC_CHECK_LT(field_start, s.size())
        << "Missing value after field trial key. String ended.";
    separator_pos = s.find('/', field_start);
    RTC_CHECK_NE(separator_pos, std::string::npos)
        << "Missing terminating '/' in field trial string.";
    RTC_CHECK_GT(separator_pos, field_start)
        << "Field trial value cannot be empty.";
    std::string value = s.substr(field_start, separator_pos - field_start);
    field_start = separator_pos + 1;

    key_value_map[key] = value;
  }
  // This check is technically redundant due to earlier checks.
  // We nevertheless keep the check to make it clear that the entire
  // string has been processed, and without indexing past the end.
  RTC_CHECK_EQ(field_start, s.size());
}

}  // namespace

namespace webrtc {
namespace test {

ScopedKeyValueConfig::ScopedKeyValueConfig()
    : ScopedKeyValueConfig(nullptr, "") {}

ScopedKeyValueConfig::ScopedKeyValueConfig(const std::string& s)
    : ScopedKeyValueConfig(nullptr, s) {}

ScopedKeyValueConfig::ScopedKeyValueConfig(ScopedKeyValueConfig& parent,
                                           const std::string& s)
    : ScopedKeyValueConfig(&parent, s) {}

ScopedKeyValueConfig::ScopedKeyValueConfig(ScopedKeyValueConfig* parent,
                                           const std::string& s)
    : parent_(parent), leaf_(nullptr) {
  InsertIntoMap(key_value_map_, s);

  if (!s.empty()) {
    // Also store field trials in global string (until we get rid of it).
    scoped_field_trials_ = std::make_unique<ScopedFieldTrials>(s);
  }

  if (parent == nullptr) {
    // We are root, set leaf_.
    leaf_ = this;
  } else {
    // Link root to new leaf.
    GetRoot(parent)->leaf_ = this;
    RTC_DCHECK(leaf_ == nullptr);
  }
}

ScopedKeyValueConfig::~ScopedKeyValueConfig() {
  if (parent_) {
    GetRoot(parent_)->leaf_ = parent_;
  }
}

ScopedKeyValueConfig* ScopedKeyValueConfig::GetRoot(ScopedKeyValueConfig* n) {
  while (n->parent_ != nullptr) {
    n = n->parent_;
  }
  return n;
}

std::string ScopedKeyValueConfig::Lookup(absl::string_view key) const {
  if (parent_ == nullptr) {
    return leaf_->LookupRecurse(key);
  } else {
    return LookupRecurse(key);
  }
}

std::string ScopedKeyValueConfig::LookupRecurse(absl::string_view key) const {
  auto it = key_value_map_.find(std::string(key));
  if (it != key_value_map_.end())
    return it->second;

  if (parent_) {
    return parent_->LookupRecurse(key);
  }

  return "";
}

}  // namespace test
}  // namespace webrtc
