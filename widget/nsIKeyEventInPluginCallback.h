/* -*- Mode: C++; tab-width: 40; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIKeyEventInPluginCallback_h_
#define nsIKeyEventInPluginCallback_h_

#include "mozilla/EventForwards.h"

#include "nsISupports.h"

#define NS_IKEYEVENTINPLUGINCALLBACK_IID \
{ 0x543c5a8a, 0xc50e, 0x4cf9, \
  { 0xa6, 0xba, 0x29, 0xa1, 0xc5, 0xa5, 0x47, 0x07 } }


class nsIKeyEventInPluginCallback : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IKEYEVENTINPLUGINCALLBACK_IID)

  /**
   * HandledWindowedPluginKeyEvent() is a callback method of
   * nsIWidget::OnWindowedPluginKeyEvent().  When it returns
   * NS_SUCCESS_EVENT_HANDLED_ASYNCHRONOUSLY, it should call this method
   * when the key event is handled.
   *
   * @param aKeyEventData      The key event which was posted to the parent
   *                           process from a plugin process.
   * @param aIsConsumed        true if aKeyEventData is consumed in the
   *                           parent process.  Otherwise, false.
   */
  virtual void HandledWindowedPluginKeyEvent(
                 const mozilla::NativeEventData& aKeyEventData,
                 bool aIsConsumed) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIKeyEventInPluginCallback,
                              NS_IKEYEVENTINPLUGINCALLBACK_IID)

#endif // #ifndef nsIKeyEventInPluginCallback_h_
