/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is a cut down version of base/feature_list.h.
// This just returns the default state for a feature.

#ifndef BASE_FEATURE_LIST_H_
#define BASE_FEATURE_LIST_H_

#include "base/macros.h"

namespace base {

// Specifies whether a given feature is enabled or disabled by default.
enum FeatureState {
  FEATURE_DISABLED_BY_DEFAULT,
  FEATURE_ENABLED_BY_DEFAULT,
};

// The Feature struct is used to define the default state for a feature. See
// comment below for more details. There must only ever be one struct instance
// for a given feature name - generally defined as a constant global variable or
// file static. It should never be used as a constexpr as it breaks
// pointer-based identity lookup.
struct BASE_EXPORT Feature {
  // The name of the feature. This should be unique to each feature and is used
  // for enabling/disabling features via command line flags and experiments.
  // It is strongly recommended to use CamelCase style for feature names, e.g.
  // "MyGreatFeature".
  const char* const name;

  // The default state (i.e. enabled or disabled) for this feature.
  const FeatureState default_state;
};

class BASE_EXPORT FeatureList {
 public:
  static bool IsEnabled(const Feature& feature) {
    return feature.default_state == FEATURE_ENABLED_BY_DEFAULT;
  }

  static FeatureList* GetInstance() { return nullptr; }

  DISALLOW_COPY_AND_ASSIGN(FeatureList);
};

}  // namespace base

#endif  // BASE_FEATURE_LIST_H_
