// /* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAlertsService_h__
#define nsAlertsService_h__

#include "nsIAlertsService.h"
#include "nsCOMPtr.h"
#include "nsXULAlerts.h"

#ifdef XP_WIN
typedef enum tagMOZ_QUERY_USER_NOTIFICATION_STATE {
    QUNS_NOT_PRESENT = 1,
    QUNS_BUSY = 2,
    QUNS_RUNNING_D3D_FULL_SCREEN = 3,
    QUNS_PRESENTATION_MODE = 4,
    QUNS_ACCEPTS_NOTIFICATIONS = 5,
    QUNS_QUIET_TIME = 6,
    QUNS_IMMERSIVE = 7
} MOZ_QUERY_USER_NOTIFICATION_STATE;

extern "C" {
// This function is Windows Vista or later
typedef HRESULT (__stdcall *SHQueryUserNotificationStatePtr)(MOZ_QUERY_USER_NOTIFICATION_STATE *pquns);
}
#endif // defined(XP_WIN)

class nsAlertsService : public nsIAlertsService,
                        public nsIAlertsDoNotDisturb
{
public:
  NS_DECL_NSIALERTSDONOTDISTURB
  NS_DECL_NSIALERTSSERVICE
  NS_DECL_ISUPPORTS

  nsAlertsService();

protected:
  virtual ~nsAlertsService();

  bool ShouldShowAlert();
  already_AddRefed<nsIAlertsDoNotDisturb> GetDNDBackend();
  nsCOMPtr<nsIAlertsService> mBackend;
};

#endif /* nsAlertsService_h__ */
