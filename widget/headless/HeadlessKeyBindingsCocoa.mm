/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "HeadlessKeyBindings.h"
#import <Cocoa/Cocoa.h>
#include "nsCocoaUtils.h"
#include "NativeKeyBindings.h"
#include "mozilla/ClearOnShutdown.h"

namespace mozilla {
namespace widget {

HeadlessKeyBindings& HeadlessKeyBindings::GetInstance() {
  static UniquePtr<HeadlessKeyBindings> sInstance;
  if (!sInstance) {
    sInstance.reset(new HeadlessKeyBindings());
    ClearOnShutdown(&sInstance);
  }
  return *sInstance;
}

nsresult HeadlessKeyBindings::AttachNativeKeyEvent(WidgetKeyboardEvent& aEvent) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  aEvent.mNativeKeyEvent = nsCocoaUtils::MakeNewCococaEventFromWidgetEvent(aEvent, 0, nil);

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void HeadlessKeyBindings::GetEditCommands(nsIWidget::NativeKeyBindingsType aType,
                                          const WidgetKeyboardEvent& aEvent,
                                          nsTArray<CommandInt>& aCommands) {
  // Convert the widget keyboard into a cocoa event so it can be translated
  // into commands in the NativeKeyBindings.
  WidgetKeyboardEvent modifiedEvent(aEvent);
  modifiedEvent.mNativeKeyEvent = nsCocoaUtils::MakeNewCococaEventFromWidgetEvent(aEvent, 0, nil);

  NativeKeyBindings* keyBindings = NativeKeyBindings::GetInstance(aType);
  keyBindings->GetEditCommands(modifiedEvent, aCommands);
}

}  // namespace widget
}  // namespace mozilla
