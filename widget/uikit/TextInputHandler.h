/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TextInputHandler_h_
#define TextInputHandler_h_

#import <UIKit/UITextInput.h>

#include "mozilla/EventForwards.h"
#include "mozilla/TextEventDispatcherListener.h"
#include "mozilla/widget/IMEData.h"
#include "nsCOMPtr.h"

class nsWindow;

namespace mozilla::widget {
class TextEventDispatcher;

// This is the temporary input class. When implementing UITextInpt protocol, we
// should share this class with Cocoa's version.
class TextInputHandler final : public TextEventDispatcherListener {
 public:
  explicit TextInputHandler(nsWindow* aWidget);
  TextInputHandler() = delete;

  NS_DECL_ISUPPORTS

  NS_IMETHOD NotifyIME(TextEventDispatcher* aTextEventDispatcher,
                       const IMENotification& aNotification) override;
  NS_IMETHOD_(IMENotificationRequests) GetIMENotificationRequests() override;
  NS_IMETHOD_(void)
  OnRemovedFrom(TextEventDispatcher* aTextEventDispatcher) override;
  NS_IMETHOD_(void)
  WillDispatchKeyboardEvent(TextEventDispatcher* aTextEventDispatcher,
                            WidgetKeyboardEvent& aKeyboardEvent,
                            uint32_t aIndexOfKeypress, void* aData) override;

  // UIKeyInput delegation
  bool InsertText(NSString* aText);
  bool HandleCommand(Command aCommand);

  void OnDestroyed();

 private:
  virtual ~TextInputHandler() = default;

  bool DispatchKeyDownEvent(uint32_t aKeyCode, KeyNameIndex aKeyNameIndex,
                            char16_t aCharCode, nsEventStatus& aStatus);
  bool DispatchKeyUpEvent(uint32_t aKeyCode, KeyNameIndex aKeyNameIndex,
                          char16_t aCharCode, nsEventStatus& aStatus);
  bool DispatchKeyPressEvent(uint32_t aKeyCode, KeyNameIndex aKeyNameIndex,
                             char16_t aCharCode, nsEventStatus& aStatus);

  bool EmulateKeyboardEvent(uint32_t aKeyCode, KeyNameIndex aKeyNameIndex,
                            char16_t charCode);

  bool Destroyed() { return !mWidget; }

  nsWindow* mWidget;  // weak ref
  RefPtr<TextEventDispatcher> mDispatcher;
};

}  // namespace mozilla::widget
#endif
