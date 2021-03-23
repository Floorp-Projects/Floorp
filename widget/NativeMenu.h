/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_NativeMenu_h
#define mozilla_widget_NativeMenu_h

#include "nsISupportsImpl.h"

namespace mozilla::widget {

class NativeMenu {
 public:
  NS_INLINE_DECL_REFCOUNTING(NativeMenu)

 protected:
  virtual ~NativeMenu() = default;
};

}  // namespace mozilla::widget

#endif  // mozilla_widget_NativeMenu_h
