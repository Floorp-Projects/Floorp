/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#pragma once

#if defined(ACCESSIBILITY)

#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsServiceManagerUtils.h"
#include "mozilla/a11y/Accessible.h"

#include "mozwrlbase.h"

namespace mozilla {
namespace widget {
namespace winrt {

// Connects to our accessibility framework to receive
// events and query the dom.
class AccessibilityBridge : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  AccessibilityBridge() {}

  ~AccessibilityBridge() {
    Disconnect();
  }

  bool Init(IUnknown* aBridge, mozilla::a11y::Accessible* aPtr) {
    mAccess = aPtr;
    mBridge = aBridge;
    return Connect();
  }

  bool Connect() {
    nsresult rv;

    nsCOMPtr<nsIObserverService> observerService = 
      do_GetService("@mozilla.org/observer-service;1", &rv);
    if (NS_FAILED(rv)) {
      return false;
    }
    rv = observerService->AddObserver(this, "accessible-event", false);
    if (NS_FAILED(rv)) {
      return false;
    }
    return true;
  }

  bool Connected() {
    return mAccess ? true : false;
  }

  void Disconnect() {
    nsresult rv;
    nsCOMPtr<nsIObserverService> observerService = 
      do_GetService("@mozilla.org/observer-service;1", &rv);
    if (NS_FAILED(rv)) {
      NS_WARNING("failed to get observersvc on shutdown.");
      return;
    }
    observerService->RemoveObserver(this, "accessible-event");
    mAccess = nullptr;
  }

private:
  nsRefPtr<mozilla::a11y::Accessible> mAccess;
  Microsoft::WRL::ComPtr<IUnknown> mBridge;
};

} } }

#endif // ACCESSIBILITY
