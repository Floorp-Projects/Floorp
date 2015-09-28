/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#if !defined(MAC_OS_X_VERSION_10_8) || (MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_8)
typedef NSInteger NSUserNotificationActivationType;
#endif

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

  nsresult Init();
  void CloseAlertCocoaString(NSString *aAlertName);
  void OnActivate(NSString *aAlertName, NSUserNotificationActivationType aActivationType,
                  unsigned long long aAdditionalActionIndex);
  void ShowPendingNotification(OSXNotificationInfo *osxni);

protected:
  virtual ~OSXNotificationCenter();

private:
  mozNotificationCenterDelegate *mDelegate;
  nsTArray<nsRefPtr<OSXNotificationInfo> > mActiveAlerts;
  nsTArray<nsRefPtr<OSXNotificationInfo> > mPendingAlerts;
};

} // namespace mozilla

#endif // OSXNotificationCenter_h
