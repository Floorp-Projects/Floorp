/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_WinModifierKeyState_h_
#define mozilla_widget_WinModifierKeyState_h_

#include "mozilla/RefPtr.h"
#include "mozilla/EventForwards.h"
#include "nsStringFwd.h"
#include <windows.h>

namespace mozilla {
namespace widget {

class MOZ_STACK_CLASS ModifierKeyState final {
 public:
  ModifierKeyState();
  explicit ModifierKeyState(Modifiers aModifiers);

  void Update();

  void Unset(Modifiers aRemovingModifiers);
  void Set(Modifiers aAddingModifiers);

  void InitInputEvent(WidgetInputEvent& aInputEvent) const;

  // Do not create IsAltGr() because it's unclear whether:
  // - AltGr key is actually pressed.
  // - Both Ctrl and Alt keys are pressed when a keyboard layout which
  //   has AltGr key.
  // - Both Ctrl and Alt keys are pressed when a keyboard layout which
  //   does not have AltGr key.
  bool IsShift() const;
  bool IsControl() const;
  bool IsAlt() const;
  bool IsWin() const;

  bool MaybeMatchShortcutKey() const;

  bool IsCapsLocked() const;
  bool IsNumLocked() const;
  bool IsScrollLocked() const;

  MOZ_ALWAYS_INLINE Modifiers GetModifiers() const { return mModifiers; }

 private:
  Modifiers mModifiers;

  void InitMouseEvent(WidgetInputEvent& aMouseEvent) const;
};

const nsCString ToString(const ModifierKeyState& aModifierKeyState);

}  // namespace widget
}  // namespace mozilla

#endif  // #ifndef mozilla_widget_WinModifierKeyState_h_
