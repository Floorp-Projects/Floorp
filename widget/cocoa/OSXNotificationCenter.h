/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef OSXNotificationCenter_h
#define OSXNotificationCenter_h

#import <Foundation/Foundation.h>
#include "nsIAlertsService.h"
#include "nsTArray.h"
#include "mozilla/RefPtr.h"

// mozNotificationCenterDelegate is used to access the macOS notification
// center. It is not related to the DesktopNotificationCenter object, which was
// removed in bug 952453. While there are no direct references to this class
// elsewhere, removing this will cause push notifications on macOS to stop
// working.
@class mozNotificationCenterDelegate;

namespace mozilla {

class OSXNotificationInfo;

class OSXNotificationCenter : public nsIAlertsService,
                              public nsIAlertsIconData,
                              public nsIAlertsDoNotDisturb,
                              public nsIAlertNotificationImageListener {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIALERTSSERVICE
  NS_DECL_NSIALERTSICONDATA
  NS_DECL_NSIALERTSDONOTDISTURB
  NS_DECL_NSIALERTNOTIFICATIONIMAGELISTENER

  OSXNotificationCenter();

  nsresult Init();
  void CloseAlertCocoaString(NSString* aAlertName);
  void OnActivate(NSString* aAlertName, NSUserNotificationActivationType aActivationType,
                  unsigned long long aAdditionalActionIndex);
  void ShowPendingNotification(OSXNotificationInfo* osxni);

 protected:
  virtual ~OSXNotificationCenter();

 private:
  mozNotificationCenterDelegate* mDelegate;
  nsTArray<RefPtr<OSXNotificationInfo> > mActiveAlerts;
  nsTArray<RefPtr<OSXNotificationInfo> > mPendingAlerts;
  bool mSuppressForScreenSharing;
};

}  // namespace mozilla

#endif  // OSXNotificationCenter_h
