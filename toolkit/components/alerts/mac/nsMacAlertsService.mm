/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: sw=2 ts=2 sts=2 expandtab */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <CoreServices/CoreServices.h>

#include "nsMacAlertsService.h"
#include "nsStringAPI.h"
#include "nsIURI.h"
#include "nsIStreamLoader.h"
#include "nsNetUtil.h"
#include "nsCOMPtr.h"
#include "nsIObserverService.h"
#include "nsAutoPtr.h"
#include "nsObjCExceptions.h"
#include "nsPIDOMWindow.h"

#import "mozNotificationCenterDelegate.h"
#import "nsNotificationCenterCompat.h"

////////////////////////////////////////////////////////////////////////////////
//// NotificationCenterDelegateWrapper

struct NotificationCenterDelegateWrapper
{
  mozNotificationCenterDelegate* mDelegate;

  NotificationCenterDelegateWrapper()
  {
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

    mDelegate = [[mozNotificationCenterDelegate alloc] init];
    
    Class c = NSClassFromString(@"NSUserNotificationCenter");
    id<FakeNSUserNotificationCenter> nc = [c performSelector:@selector(defaultUserNotificationCenter)];
    nc.delegate = mDelegate;

    NS_OBJC_END_TRY_ABORT_BLOCK;
  }

  ~NotificationCenterDelegateWrapper()
  {
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

    Class c = NSClassFromString(@"NSUserNotificationCenter");
    id<FakeNSUserNotificationCenter> nc = [c performSelector:@selector(defaultUserNotificationCenter)];
    nc.delegate = nil;

    [mDelegate release];

    NS_OBJC_END_TRY_ABORT_BLOCK;
  }
};

static nsresult
DispatchNCNotification(NotificationCenterDelegateWrapper* aDelegateWrapper,
                       const nsAString &aTitle,
                       const nsAString &aMessage,
                       const nsAString &aCookie,
                       nsIObserver *aListener)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  mozNotificationCenterDelegate* delegate = aDelegateWrapper->mDelegate;

  if (!delegate) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  uint32_t ind = 0;
  if (aListener) {
    ind = [delegate addObserver:aListener];
  }

  // Notification Center doesn't allow images so we'll just drop that
  [delegate notifyWithTitle:aTitle
                description:aMessage
                        key:ind
                     cookie:aCookie];

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

////////////////////////////////////////////////////////////////////////////////
//// nsMacAlertsService

NS_IMPL_THREADSAFE_ADDREF(nsMacAlertsService)
NS_IMPL_THREADSAFE_RELEASE(nsMacAlertsService)

NS_INTERFACE_MAP_BEGIN(nsMacAlertsService)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAlertsService)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIAlertsService)
NS_INTERFACE_MAP_END_THREADSAFE

nsresult
nsMacAlertsService::Init()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsresult rv;
  nsCOMPtr<nsIObserverService> os =
    do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Only use Notification Center with OS version >= 10.8.
  SInt32 osVersion = 0;
  OSErr err = ::Gestalt (gestaltSystemVersion, &osVersion);
  osVersion &= 0xFFFF; // The system version is in the low order word
  if (err != noErr || (osVersion < 0x00001080)) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  rv = InitNotificationCenter();
  if (NS_FAILED(rv)) {
    return rv;
  }

  (void)os->AddObserver(this, DOM_WINDOW_DESTROYED_TOPIC, false);
  (void)os->AddObserver(this, "profile-before-change", false);

  return rv;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult
nsMacAlertsService::InitNotificationCenter() {
  mNCDelegate = new NotificationCenterDelegateWrapper();
  return NS_OK;
}

nsMacAlertsService::nsMacAlertsService()
: mNCDelegate(nullptr)
{}

nsMacAlertsService::~nsMacAlertsService()
{
  delete mNCDelegate;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIAlertsService

NS_IMETHODIMP
nsMacAlertsService::ShowAlertNotification(const nsAString& aImageUrl,
                                            const nsAString& aAlertTitle,
                                            const nsAString& aAlertText,
                                            bool aAlertClickable,
                                            const nsAString& aAlertCookie,
                                            nsIObserver* aAlertListener,
                                            const nsAString& aAlertName)
{
  return DispatchNCNotification(mNCDelegate, aAlertTitle, aAlertText,
                                aAlertCookie, aAlertListener);
}

////////////////////////////////////////////////////////////////////////////////
//// nsIObserver

NS_IMETHODIMP
nsMacAlertsService::Observe(nsISupports* aSubject, const char* aTopic,
                              const PRUnichar* aData)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!mNCDelegate) {
    return NS_OK;
  }

  if (strcmp(aTopic, DOM_WINDOW_DESTROYED_TOPIC) == 0) {
    nsCOMPtr<nsIDOMWindow> window(do_QueryInterface(aSubject));
    if (window) {
      [mNCDelegate->mDelegate forgetObserversForWindow:window];
    }
  }
  else if (strcmp(aTopic, "profile-before-change") == 0) {
    [mNCDelegate->mDelegate forgetObservers];
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}
