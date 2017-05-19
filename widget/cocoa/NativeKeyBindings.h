/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_NativeKeyBindings_h_
#define mozilla_widget_NativeKeyBindings_h_

#import <Cocoa/Cocoa.h>
#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "nsDataHashtable.h"
#include "nsIWidget.h"

namespace mozilla {
namespace widget {

typedef nsDataHashtable<nsPtrHashKey<struct objc_selector>, CommandInt>
  SelectorCommandHashtable;

class NativeKeyBindings final
{
  typedef nsIWidget::NativeKeyBindingsType NativeKeyBindingsType;

public:
  static NativeKeyBindings* GetInstance(NativeKeyBindingsType aType);
  static void Shutdown();

  void Init(NativeKeyBindingsType aType);

  void GetEditCommands(const WidgetKeyboardEvent& aEvent,
                       nsTArray<CommandInt>& aCommands);

private:
  NativeKeyBindings();

  SelectorCommandHashtable mSelectorToCommand;

  static NativeKeyBindings* sInstanceForSingleLineEditor;
  static NativeKeyBindings* sInstanceForMultiLineEditor;
}; // NativeKeyBindings

} // namespace widget
} // namespace mozilla

#endif // mozilla_widget_NativeKeyBindings_h_
