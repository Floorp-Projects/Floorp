/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "KeyboardLayout.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/widget/IMEData.h"
#include "nsWindow.h"
#include "WinIMEHandler.h"
#include "WinTextEventDispatcherListener.h"

namespace mozilla {
namespace widget {

StaticRefPtr<WinTextEventDispatcherListener>
  WinTextEventDispatcherListener::sInstance;

// static
WinTextEventDispatcherListener*
WinTextEventDispatcherListener::GetInstance()
{
  if (!sInstance) {
    sInstance = new WinTextEventDispatcherListener();
  }
  return sInstance.get();
}

void
WinTextEventDispatcherListener::Shutdown()
{
  sInstance = nullptr;
}

NS_IMPL_ISUPPORTS(WinTextEventDispatcherListener,
                  TextEventDispatcherListener,
                  nsISupportsWeakReference)

WinTextEventDispatcherListener::WinTextEventDispatcherListener()
{
}

WinTextEventDispatcherListener::~WinTextEventDispatcherListener()
{
}

NS_IMETHODIMP
WinTextEventDispatcherListener::NotifyIME(
                                  TextEventDispatcher* aTextEventDispatcher,
                                  const IMENotification& aNotification)
{
  nsWindow* window = static_cast<nsWindow*>(aTextEventDispatcher->GetWidget());
  if (NS_WARN_IF(!window)) {
    return NS_ERROR_FAILURE;
  }
  return IMEHandler::NotifyIME(window, aNotification);
}

NS_IMETHODIMP_(void)
WinTextEventDispatcherListener::OnRemovedFrom(
                                  TextEventDispatcher* aTextEventDispatcher)
{
  // XXX When input transaction is being stolen by add-on, what should we do?
}

NS_IMETHODIMP_(void)
WinTextEventDispatcherListener::WillDispatchKeyboardEvent(
                                  TextEventDispatcher* aTextEventDispatcher,
                                  WidgetKeyboardEvent& aKeyboardEvent,
                                  uint32_t aIndexOfKeypress,
                                  void* aData)
{
  static_cast<NativeKey*>(aData)->
    WillDispatchKeyboardEvent(aKeyboardEvent, aIndexOfKeypress);
}

} // namespace widget
} // namespace mozilla
