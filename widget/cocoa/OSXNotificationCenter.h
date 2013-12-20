/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OSXNotificationCenter_h
#define OSXNotificationCenter_h

#import <Foundation/Foundation.h>
#include "nsIAlertsService.h"
#include "imgINotificationObserver.h"
#include "nsITimer.h"
#include "nsTArray.h"
#include "mozilla/RefPtr.h"

@class mozNotificationCenterDelegate;

namespace mozilla {

class OSXNotificationInfo;

class OSXNotificationCenter : public nsIAlertsService,
                              public imgINotificationObserver,
                              public nsITimerCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIALERTSSERVICE
  NS_DECL_IMGINOTIFICATIONOBSERVER
  NS_DECL_NSITIMERCALLBACK

  OSXNotificationCenter();
  virtual ~OSXNotificationCenter();

  nsresult Init();
  void CloseAlertCocoaString(NSString *aAlertName);
  void OnClick(NSString *aAlertName);
  void ShowPendingNotification(OSXNotificationInfo *osxni);

private:
  mozNotificationCenterDelegate *mDelegate;
  nsTArray<nsRefPtr<OSXNotificationInfo> > mActiveAlerts;
  nsTArray<nsRefPtr<OSXNotificationInfo> > mPendingAlerts;
};

} // namespace mozilla

#endif // OSXNotificationCenter_h
