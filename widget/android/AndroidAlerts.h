/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_widget_AndroidAlerts_h__
#define mozilla_widget_AndroidAlerts_h__

#include "nsTHashMap.h"
#include "nsInterfaceHashtable.h"
#include "nsCOMPtr.h"
#include "nsHashKeys.h"
#include "nsIAlertsService.h"
#include "nsIObserver.h"

#include "mozilla/java/WebNotificationWrappers.h"

#include "mozilla/StaticPtr.h"

namespace mozilla {
namespace widget {

class AndroidAlerts : public nsIAlertsService {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIALERTSSERVICE

  AndroidAlerts() {}

  static void NotifyListener(const nsAString& aName, const char* aTopic,
                             const char16_t* aCookie);

  static nsTHashMap<nsStringHashKey, mozilla::java::WebNotification::GlobalRef>
      mNotificationsMap;

 protected:
  virtual ~AndroidAlerts() { sListenerMap = nullptr; }

  using ListenerMap = nsInterfaceHashtable<nsStringHashKey, nsIObserver>;
  static StaticAutoPtr<ListenerMap> sListenerMap;
};

}  // namespace widget
}  // namespace mozilla

#endif  // nsAndroidAlerts_h__
