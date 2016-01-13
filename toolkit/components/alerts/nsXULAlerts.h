/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsXULAlerts_h__
#define nsXULAlerts_h__

#include "nsHashKeys.h"
#include "nsInterfaceHashtable.h"

#include "nsIDOMWindow.h"
#include "nsIObserver.h"

class nsXULAlerts : public nsIAlertsService,
                    public nsIAlertsDoNotDisturb
{
  friend class nsXULAlertObserver;
public:
  NS_DECL_NSIALERTSDONOTDISTURB
  NS_DECL_NSIALERTSSERVICE
  NS_DECL_ISUPPORTS

  nsXULAlerts()
  {
  }

  static already_AddRefed<nsXULAlerts> GetInstance();

protected:
  virtual ~nsXULAlerts() {}

  nsInterfaceHashtable<nsStringHashKey, nsIDOMWindow> mNamedWindows;
  bool mDoNotDisturb = false;
};

/**
 * This class wraps observers for alerts and watches
 * for the "alertfinished" event in order to release
 * the reference on the nsIDOMWindow of the XUL alert.
 */
class nsXULAlertObserver : public nsIObserver {
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsXULAlertObserver(nsXULAlerts* aXULAlerts, const nsAString& aAlertName,
                     nsIObserver* aObserver)
    : mXULAlerts(aXULAlerts), mAlertName(aAlertName),
      mObserver(aObserver) {}

  void SetAlertWindow(nsIDOMWindow* aWindow) { mAlertWindow = aWindow; }

protected:
  virtual ~nsXULAlertObserver() {}

  RefPtr<nsXULAlerts> mXULAlerts;
  nsString mAlertName;
  nsCOMPtr<nsIDOMWindow> mAlertWindow;
  nsCOMPtr<nsIObserver> mObserver;
};

#endif /* nsXULAlerts_h__ */

