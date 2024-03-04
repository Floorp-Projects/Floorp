/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <UIKit/UIKit.h>

#include "IMEData.h"

namespace mozilla::widget {

class UIKitUtils final {
 public:
  UIKitUtils() = delete;
  ~UIKitUtils() = delete;

  static UIKeyboardType GetUIKeyboardType(const InputContext& aContext);
  static UIReturnKeyType GetUIReturnKeyType(const InputContext& aContext);
  static UITextAutocapitalizationType GetUITextAutocapitalizationType(
      const InputContext& aContext);
};

}  // namespace mozilla::widget
