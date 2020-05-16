/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXULAlerts_h__
#define nsXULAlerts_h__

#include "nsCycleCollectionParticipant.h"
#include "nsDataHashtable.h"
#include "nsHashKeys.h"
#include "nsInterfaceHashtable.h"

#include "mozIDOMWindow.h"
#include "nsIObserver.h"

struct PendingAlert {
  void Init(nsIAlertNotification* aAlert, nsIObserver* aListener) {
    mAlert = aAlert;
    mListener = aListener;
  }
  nsCOMPtr<nsIAlertNotification> mAlert;
  nsCOMPtr<nsIObserver> mListener;
};

class nsXULAlerts : public nsIAlertsService,
                    public nsIAlertsDoNotDisturb,
                    public nsIAlertsIconURI {
  friend class nsXULAlertObserver;

 public:
  NS_DECL_NSIALERTSICONURI
  NS_DECL_NSIALERTSDONOTDISTURB
  NS_DECL_NSIALERTSSERVICE
  NS_DECL_ISUPPORTS

  nsXULAlerts() = default;

  static already_AddRefed<nsXULAlerts> GetInstance();

 protected:
  virtual ~nsXULAlerts() = default;
  void PersistentAlertFinished();

  nsInterfaceHashtable<nsStringHashKey, mozIDOMWindowProxy> mNamedWindows;
  uint32_t mPersistentAlertCount = 0;
  nsTArray<PendingAlert> mPendingPersistentAlerts;
  bool mDoNotDisturb = false;

 private:
  bool mSuppressForScreenSharing = false;
};

/**
 * This class wraps observers for alerts and watches
 * for the "alertfinished" event in order to release
 * the reference on the nsIDOMWindow of the XUL alert.
 */
class nsXULAlertObserver : public nsIObserver {
 public:
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_NSIOBSERVER
  NS_DECL_CYCLE_COLLECTION_CLASS(nsXULAlertObserver)

  nsXULAlertObserver(nsXULAlerts* aXULAlerts, const nsAString& aAlertName,
                     nsIObserver* aObserver, bool aIsPersistent)
      : mXULAlerts(aXULAlerts),
        mAlertName(aAlertName),
        mObserver(aObserver),
        mIsPersistent(aIsPersistent) {}

  void SetAlertWindow(mozIDOMWindowProxy* aWindow) { mAlertWindow = aWindow; }

 protected:
  virtual ~nsXULAlertObserver() = default;

  RefPtr<nsXULAlerts> mXULAlerts;
  nsString mAlertName;
  nsCOMPtr<mozIDOMWindowProxy> mAlertWindow;
  nsCOMPtr<nsIObserver> mObserver;
  bool mIsPersistent;
};

#endif /* nsXULAlerts_h__ */
