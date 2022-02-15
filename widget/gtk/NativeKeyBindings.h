/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NativeKeyBindings_h
#define NativeKeyBindings_h

#include "mozilla/Attributes.h"
#include "mozilla/EventForwards.h"
#include "nsIWidget.h"

#include <glib.h>  // for guint

using GtkWidget = struct _GtkWidget;

namespace mozilla {
enum class NativeKeyBindingsType : uint8_t;

class WritingMode;
template <typename T>
class Maybe;

namespace widget {

class NativeKeyBindings final {
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
  ~NativeKeyBindings();

  bool GetEditCommandsInternal(const WidgetKeyboardEvent& aEvent,
                               nsTArray<CommandInt>& aCommands, guint aKeyval);

  GtkWidget* mNativeTarget;

  static NativeKeyBindings* sInstanceForSingleLineEditor;
  static NativeKeyBindings* sInstanceForMultiLineEditor;
};

}  // namespace widget
}  // namespace mozilla

#endif  // NativeKeyBindings_h
