/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NativeKeyBindings_h
#define NativeKeyBindings_h

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "nsTHashMap.h"
#include "nsIWidget.h"

struct objc_selector;

namespace mozilla {

class WritingMode;
template <typename T>
class Maybe;

namespace widget {

typedef nsTHashMap<nsPtrHashKey<objc_selector>, Command>
    SelectorCommandHashtable;

class NativeKeyBindings final {
  typedef nsIWidget::NativeKeyBindingsType NativeKeyBindingsType;

 public:
  static NativeKeyBindings* GetInstance(NativeKeyBindingsType aType);
  static void Shutdown();

  /**
   * GetEditCommandsForTests() returns commands performed in native widget
   * in typical environment.  I.e., this does NOT refer customized shortcut
   * key mappings of the environment.
   */
  static void GetEditCommandsForTests(NativeKeyBindingsType aType,
                                      const WidgetKeyboardEvent& aEvent,
                                      const Maybe<WritingMode>& aWritingMode,
                                      nsTArray<CommandInt>& aCommands);

  void Init(NativeKeyBindingsType aType);

  void GetEditCommands(const WidgetKeyboardEvent& aEvent,
                       const Maybe<WritingMode>& aWritingMode,
                       nsTArray<CommandInt>& aCommands);

 private:
  NativeKeyBindings();

  void AppendEditCommandsForSelector(objc_selector* aSelector,
                                     nsTArray<CommandInt>& aCommands) const;

  void LogEditCommands(const nsTArray<CommandInt>& aCommands,
                       const char* aDescription) const;

  SelectorCommandHashtable mSelectorToCommand;

  static NativeKeyBindings* sInstanceForSingleLineEditor;
  static NativeKeyBindings* sInstanceForMultiLineEditor;
};  // NativeKeyBindings

}  // namespace widget
}  // namespace mozilla

#endif  // NativeKeyBindings_h
