/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAlertsService_h__
#define nsAlertsService_h__

#include "nsIAlertsService.h"
#include "nsCOMPtr.h"
#include "nsXULAlerts.h"

class nsAlertsService : public nsIAlertsService, public nsIAlertsDoNotDisturb {
 public:
  NS_DECL_NSIALERTSDONOTDISTURB
  NS_DECL_NSIALERTSSERVICE
  NS_DECL_ISUPPORTS

  nsAlertsService();

 protected:
  virtual ~nsAlertsService();

  bool ShouldShowAlert();
  bool ShouldUseSystemBackend();
  already_AddRefed<nsIAlertsDoNotDisturb> GetDNDBackend();
  nsCOMPtr<nsIAlertsService> mBackend;
};

#endif /* nsAlertsService_h__ */
