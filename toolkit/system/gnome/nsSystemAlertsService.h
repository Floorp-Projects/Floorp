/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSystemAlertsService_h__
#define nsSystemAlertsService_h__

#include "nsIAlertsService.h"
#include "nsDataHashtable.h"
#include "nsCOMPtr.h"

class nsAlertsIconListener;

class nsSystemAlertsService : public nsIAlertsService
{
public:
  NS_DECL_NSIALERTSSERVICE
  NS_DECL_ISUPPORTS

  nsSystemAlertsService();

  nsresult Init();

  bool IsActiveListener(const nsAString& aAlertName,
                        nsAlertsIconListener* aListener);
  void RemoveListener(const nsAString& aAlertName,
                      nsAlertsIconListener* aListener);

protected:
  virtual ~nsSystemAlertsService();

  void AddListener(const nsAString& aAlertName,
                   nsAlertsIconListener* aListener);

  nsDataHashtable<nsStringHashKey, nsAlertsIconListener*> mActiveListeners;
};

#endif /* nsSystemAlertsService_h__ */
