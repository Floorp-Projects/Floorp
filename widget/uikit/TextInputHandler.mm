/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TextInputHandler.h"

#import <UIKit/UIKit.h>

#include "mozilla/EventForwards.h"
#include "mozilla/Logging.h"
#include "mozilla/MacStringHelpers.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/TextEvents.h"
#include "mozilla/WidgetUtils.h"
#include "nsIWidget.h"
#include "nsObjCExceptions.h"
#include "nsString.h"
#include "nsWindow.h"

mozilla::LazyLogModule gIMELog("TextInputHandler");

namespace mozilla::widget {

NS_IMPL_ISUPPORTS(TextInputHandler, TextEventDispatcherListener,
                  nsISupportsWeakReference)

TextInputHandler::TextInputHandler(nsWindow* aWidget)
    : mWidget(aWidget), mDispatcher(aWidget->GetTextEventDispatcher()) {}

nsresult TextInputHandler::NotifyIME(TextEventDispatcher* aTextEventDispatcher,
                                     const IMENotification& aNotification) {
  return NS_OK;
}

IMENotificationRequests TextInputHandler::GetIMENotificationRequests() {
  return IMENotificationRequests();
}

void TextInputHandler::OnRemovedFrom(
    TextEventDispatcher* aTextEventDispatcher) {}

void TextInputHandler::WillDispatchKeyboardEvent(
    TextEventDispatcher* aTextEventDispatcher,
    WidgetKeyboardEvent& aKeyboardEvent, uint32_t aIndexOfKeypress,
    void* aData) {}

bool TextInputHandler::InsertText(NSString* aText) {
  nsString str;
  CopyNSStringToXPCOMString(aText, str);

  MOZ_LOG(gIMELog, LogLevel::Info,
          ("%p TextInputHandler::InsertText(aText=%s)", this,
           NS_ConvertUTF16toUTF8(str).get()));

  if (Destroyed()) {
    return false;
  }

  if (str.Length() == 1) {
    char16_t charCode = str[0];
    if (charCode == 0x0a) {
      return EmulateKeyboardEvent(NS_VK_RETURN, KEY_NAME_INDEX_Enter, charCode);
    }
    if (charCode == 0x08) {
      return EmulateKeyboardEvent(NS_VK_BACK, KEY_NAME_INDEX_Backspace,
                                  charCode);
    }
    if (uint32_t keyCode = WidgetUtils::ComputeKeyCodeFromChar(charCode)) {
      return EmulateKeyboardEvent(keyCode, KEY_NAME_INDEX_USE_STRING, charCode);
    }
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  RefPtr<nsWindow> widget(mWidget);
  if (!DispatchKeyDownEvent(NS_VK_PROCESSKEY, KEY_NAME_INDEX_Process, 0,
                            status)) {
    return false;
  }
  if (Destroyed()) {
    return false;
  }

  mDispatcher->CommitComposition(status, &str, nullptr);
  if (widget->Destroyed()) {
    return false;
  }

  DispatchKeyUpEvent(NS_VK_PROCESSKEY, KEY_NAME_INDEX_Process, 0, status);

  return true;
}

bool TextInputHandler::HandleCommand(Command aCommand) {
  MOZ_LOG(gIMELog, LogLevel::Info,
          ("%p   TextInputHandler::HandleCommand, aCommand=%s", this,
           ToChar(aCommand)));

  if (Destroyed()) {
    return false;
  }

  if (aCommand != Command::DeleteCharBackward) {
    return false;
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  if (!DispatchKeyDownEvent(NS_VK_BACK, KEY_NAME_INDEX_Backspace, 0, status)) {
    return true;
  }
  if (Destroyed() || status == nsEventStatus_eConsumeNoDefault) {
    return true;
  }

  // TODO: Focus check

  if (!DispatchKeyPressEvent(NS_VK_BACK, KEY_NAME_INDEX_Backspace, 0, status)) {
    return true;
  }
  if (Destroyed() || status == nsEventStatus_eConsumeNoDefault) {
    return true;
  }

  // TODO: Focus check

  DispatchKeyUpEvent(NS_VK_BACK, KEY_NAME_INDEX_Backspace, 0, status);

  return true;
}

static uint32_t ComputeKeyModifiers(uint32_t aCharCode) {
  if (aCharCode >= 'A' && aCharCode <= 'Z') {
    return MODIFIER_SHIFT;
  }
  return 0;
}

static void InitKeyEvent(WidgetKeyboardEvent& aEvent, uint32_t aKeyCode,
                         KeyNameIndex aKeyNameIndex, char16_t aCharCode) {
  aEvent.mKeyCode = aKeyCode;
  aEvent.mIsRepeat = false;
  aEvent.mKeyNameIndex = aKeyNameIndex;
  // TODO(m_kato):
  // How to get native key? Then, implement NativeKeyToDOM*.h for iOS
  aEvent.mCodeNameIndex = CODE_NAME_INDEX_UNKNOWN;
  if (aEvent.mKeyNameIndex == KEY_NAME_INDEX_USE_STRING) {
    aEvent.mKeyValue = aCharCode;
  }
  aEvent.mModifiers = ComputeKeyModifiers(aCharCode);
  aEvent.mLocation = eKeyLocationStandard;
  aEvent.mTimeStamp = TimeStamp::Now();
}

bool TextInputHandler::DispatchKeyDownEvent(uint32_t aKeyCode,
                                            KeyNameIndex aKeyNameIndex,
                                            char16_t aCharCode,
                                            nsEventStatus& aStatus) {
  MOZ_ASSERT(aKeyCode);
  MOZ_ASSERT(mWidget);

  WidgetKeyboardEvent keydownEvent(true, eKeyDown, mWidget);
  InitKeyEvent(keydownEvent, aKeyCode, aKeyNameIndex, aCharCode);
  nsresult rv = mDispatcher->BeginNativeInputTransaction();
  if (NS_FAILED(rv)) {
    NS_WARNING("BeginNativeInputTransaction is failed");
    return false;
  }
  return mDispatcher->DispatchKeyboardEvent(eKeyDown, keydownEvent, aStatus);
}

bool TextInputHandler::DispatchKeyUpEvent(uint32_t aKeyCode,
                                          KeyNameIndex aKeyNameIndex,
                                          char16_t aCharCode,
                                          nsEventStatus& aStatus) {
  MOZ_ASSERT(aKeyCode);
  MOZ_ASSERT(mWidget);

  WidgetKeyboardEvent keyupEvent(true, eKeyUp, mWidget);
  InitKeyEvent(keyupEvent, aKeyCode, aKeyNameIndex, aCharCode);
  nsresult rv = mDispatcher->BeginNativeInputTransaction();
  if (NS_FAILED(rv)) {
    NS_WARNING("BeginNativeInputTransaction is failed");
    return false;
  }
  return mDispatcher->DispatchKeyboardEvent(eKeyUp, keyupEvent, aStatus);
}

bool TextInputHandler::DispatchKeyPressEvent(uint32_t aKeyCode,
                                             KeyNameIndex aKeyNameIndex,
                                             char16_t aCharCode,
                                             nsEventStatus& aStatus) {
  MOZ_ASSERT(aKeyCode);
  MOZ_ASSERT(mWidget);

  WidgetKeyboardEvent keypressEvent(true, eKeyPress, mWidget);
  InitKeyEvent(keypressEvent, aKeyCode, aKeyNameIndex, aCharCode);
  nsresult rv = mDispatcher->BeginNativeInputTransaction();
  if (NS_FAILED(rv)) {
    NS_WARNING("BeginNativeInputTransaction is failed");
    return false;
  }
  return mDispatcher->MaybeDispatchKeypressEvents(keypressEvent, aStatus);
}

bool TextInputHandler::EmulateKeyboardEvent(uint32_t aKeyCode,
                                            KeyNameIndex aKeyNameIndex,
                                            char16_t aCharCode) {
  MOZ_ASSERT(aCharCode);

  MOZ_LOG(gIMELog, LogLevel::Info,
          ("%p TextInputHandler::EmulateKeyboardEvent(aKeyCode=%x, "
           "aKeyNameIndex=%x, aCharCode=%x)",
           this, aKeyCode, aKeyNameIndex, aCharCode));

  nsEventStatus status = nsEventStatus_eIgnore;
  if (!DispatchKeyDownEvent(aKeyCode, aKeyNameIndex, aCharCode, status)) {
    return true;
  }
  if (Destroyed() || status == nsEventStatus_eConsumeNoDefault) {
    return true;
  }
  // TODO: Focus check

  if (!DispatchKeyPressEvent(aKeyCode, aKeyNameIndex, aCharCode, status)) {
    return true;
  }
  if (Destroyed() || status == nsEventStatus_eConsumeNoDefault) {
    return true;
  }
  // TODO: Focus check

  DispatchKeyUpEvent(aKeyCode, aKeyNameIndex, aCharCode, status);
  return true;
}

void TextInputHandler::OnDestroyed() { mWidget = nullptr; }

}  // namespace mozilla::widget
