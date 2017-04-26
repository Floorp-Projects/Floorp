/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoWindowStateHelper.h"

#include "mozilla/dom/Event.h"
#include "nsIDocument.h"
#include "nsIDOMEvent.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsString.h"

using namespace mozilla;
using namespace mozilla::dom;

/****************************************************************
 ****************** nsAutoWindowStateHelper *********************
 ****************************************************************/

nsAutoWindowStateHelper::nsAutoWindowStateHelper(nsPIDOMWindowOuter* aWindow)
  : mWindow(aWindow)
  , mDefaultEnabled(DispatchEventToChrome("DOMWillOpenModalDialog"))
{
  if (mWindow) {
    mWindow->EnterModalState();
  }
}

nsAutoWindowStateHelper::~nsAutoWindowStateHelper()
{
  if (mWindow) {
    mWindow->LeaveModalState();
  }

  if (mDefaultEnabled) {
    DispatchEventToChrome("DOMModalDialogClosed");
  }
}

bool
nsAutoWindowStateHelper::DispatchEventToChrome(const char* aEventName)
{
  // XXXbz should we skip dispatching the event if the inner changed?
  // That is, should we store both the inner and the outer?
  if (!mWindow) {
    return true;
  }

  // The functions of nsContentUtils do not provide the required behavior,
  // so the following is inlined.
  nsIDocument* doc = mWindow->GetExtantDoc();
  if (!doc) {
    return true;
  }

  ErrorResult rv;
  RefPtr<Event> event = doc->CreateEvent(NS_LITERAL_STRING("Events"),
                                         CallerType::System, rv);
  if (rv.Failed()) {
    rv.SuppressException();
    return false;
  }
  event->InitEvent(NS_ConvertASCIItoUTF16(aEventName), true, true);
  event->SetTrusted(true);
  event->WidgetEventPtr()->mFlags.mOnlyChromeDispatch = true;

  nsCOMPtr<EventTarget> target = do_QueryInterface(mWindow);
  bool defaultActionEnabled;
  target->DispatchEvent(event, &defaultActionEnabled);
  return defaultActionEnabled;
}
