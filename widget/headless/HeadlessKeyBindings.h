/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_HeadlessKeyBindings_h
#define mozilla_widget_HeadlessKeyBindings_h

#include "mozilla/TextEvents.h"
#include "nsIWidget.h"
#include "nsTArray.h"

namespace mozilla {
namespace widget {

/**
 * Helper to emulate native key bindings. Currently only MacOS is supported.
 */

class HeadlessKeyBindings final {
 public:
  HeadlessKeyBindings() = default;

  static HeadlessKeyBindings& GetInstance();

  void GetEditCommands(nsIWidget::NativeKeyBindingsType aType,
                       const WidgetKeyboardEvent& aEvent,
                       nsTArray<CommandInt>& aCommands);
  [[nodiscard]] nsresult AttachNativeKeyEvent(WidgetKeyboardEvent& aEvent);
};

}  // namespace widget
}  // namespace mozilla

#endif  // mozilla_widget_HeadlessKeyBindings_h
