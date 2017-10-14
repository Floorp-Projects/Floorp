/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HeadlessKeyBindings.h"
#include "mozilla/ClearOnShutdown.h"

namespace mozilla {
namespace widget {

HeadlessKeyBindings&
HeadlessKeyBindings::GetInstance()
{
  static UniquePtr<HeadlessKeyBindings> sInstance;
  if (!sInstance) {
    sInstance.reset(new HeadlessKeyBindings());
    ClearOnShutdown(&sInstance);
  }
  return *sInstance;
}

nsresult
HeadlessKeyBindings::AttachNativeKeyEvent(WidgetKeyboardEvent& aEvent)
{
  // Stub for non-mac platforms.
  return NS_OK;
}

void
HeadlessKeyBindings::GetEditCommands(nsIWidget::NativeKeyBindingsType aType,
                                     const WidgetKeyboardEvent& aEvent,
                                     nsTArray<CommandInt>& aCommands)
{
  // Stub for non-mac platforms.
}

} // namespace widget
} // namespace mozilla
