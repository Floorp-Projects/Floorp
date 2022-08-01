/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef ToastNotification_h__
#define ToastNotification_h__

#include "mozilla/Maybe.h"
#include "nsIAlertsService.h"
#include "nsIObserver.h"
#include "nsIThread.h"
#include "nsIWindowsAlertsService.h"
#include "nsRefPtrHashtable.h"

namespace mozilla {
namespace widget {

class ToastNotificationHandler;

class ToastNotification final : public nsIWindowsAlertsService,
                                public nsIAlertsDoNotDisturb,
                                public nsIObserver {
 public:
  NS_DECL_NSIALERTSSERVICE
  NS_DECL_NSIWINDOWSALERTSSERVICE
  NS_DECL_NSIALERTSDONOTDISTURB
  NS_DECL_NSIOBSERVER
  NS_DECL_ISUPPORTS

  ToastNotification();

  nsresult Init();

  bool IsActiveHandler(const nsAString& aAlertName,
                       ToastNotificationHandler* aHandler);
  void RemoveHandler(const nsAString& aAlertName,
                     ToastNotificationHandler* aHandler);

  nsresult BackgroundDispatch(nsIRunnable* runnable);

 protected:
  virtual ~ToastNotification();
  bool EnsureAumidRegistered();

  static bool AssignIfMsixAumid(Maybe<nsAutoString>& aAumid);
  static bool AssignIfNsisAumid(nsAutoString& aInstallHash,
                                Maybe<nsAutoString>& aAumid);
  static bool RegisterRuntimeAumid(nsAutoString& aInstallHash,
                                   Maybe<nsAutoString>& aAumid);

  nsRefPtrHashtable<nsStringHashKey, ToastNotificationHandler> mActiveHandlers;
  Maybe<nsAutoString> mAumid;
  bool mSuppressForScreenSharing = false;
};

}  // namespace widget
}  // namespace mozilla

#endif
