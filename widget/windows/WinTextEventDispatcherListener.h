/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef WinTextEventDispatcherListener_h_
#define WinTextEventDispatcherListener_h_

#include "mozilla/Attributes.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TextEventDispatcherListener.h"

namespace mozilla {
namespace widget {

/**
 * On Windows, it's enough TextEventDispatcherListener to be a singleton
 * because we have only one input context per process (IMM can create
 * multiple IM context but we don't support such behavior).
 */

class WinTextEventDispatcherListener final : public TextEventDispatcherListener
{
public:
  static WinTextEventDispatcherListener* GetInstance();
  static void Shutdown();

  NS_DECL_ISUPPORTS

  NS_IMETHOD NotifyIME(TextEventDispatcher* aTextEventDispatcher,
                       const IMENotification& aNotification) override;
  NS_IMETHOD_(void) OnRemovedFrom(
                      TextEventDispatcher* aTextEventDispatcher) override;
  NS_IMETHOD_(void) WillDispatchKeyboardEvent(
                      TextEventDispatcher* aTextEventDispatcher,
                      WidgetKeyboardEvent& aKeyboardEvent,
                      uint32_t aIndexOfKeypress,
                      void* aData) override;

private:
  WinTextEventDispatcherListener();
  virtual ~WinTextEventDispatcherListener();

  static StaticRefPtr<WinTextEventDispatcherListener> sInstance;
};

} // namespace widget
} // namespace mozilla

#endif // #ifndef WinTextEventDispatcherListener_h_
