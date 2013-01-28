/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: sw=2 ts=2 sts=2 expandtab */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <CoreServices/CoreServices.h>

#include "nsMacAlertsService.h"
#include "nsStringAPI.h"
#include "nsAlertsImageLoadListener.h"
#include "nsIURI.h"
#include "nsIStreamLoader.h"
#include "nsNetUtil.h"
#include "nsCOMPtr.h"
#include "nsIStringBundle.h"
#include "nsIObserverService.h"
#include "nsAutoPtr.h"
#include "nsNotificationsList.h"
#include "nsObjCExceptions.h"
#include "nsPIDOMWindow.h"

#import "mozGrowlDelegate.h"
#import "mozNotificationCenterDelegate.h"
#import "GrowlApplicationBridge.h"
#import "nsNotificationCenterCompat.h"

////////////////////////////////////////////////////////////////////////////////
//// GrowlDelegateWrapper

struct GrowlDelegateWrapper
{
  mozGrowlDelegate* mDelegate;
  
  GrowlDelegateWrapper()
  {
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK;
    
    mDelegate = [[mozGrowlDelegate alloc] init];
    
    NS_OBJC_END_TRY_ABORT_BLOCK;
  }
  
  ~GrowlDelegateWrapper()
  {
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK;
    
    [mDelegate release];
    
    NS_OBJC_END_TRY_ABORT_BLOCK;
  }
};

/**
 * Helper function to dispatch a notification to Growl
 */
static nsresult
DispatchGrowlNotification(const nsAString &aName,
                          const nsAString &aImage,
                          const nsAString &aTitle,
                          const nsAString &aMessage,
                          const nsAString &aCookie,
                          nsIObserver *aListener)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;
  
  if ([GrowlApplicationBridge isGrowlRunning] == NO) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  
  mozGrowlDelegate *delegate =
  static_cast<mozGrowlDelegate *>([GrowlApplicationBridge growlDelegate]);
  if (!delegate) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  
  uint32_t ind = 0;
  if (aListener) {
    ind = [delegate addObserver: aListener];
  }
  
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aImage);
  if (NS_FAILED(rv)) {
    // image uri failed to resolve, so dispatch to growl with no image
    [mozGrowlDelegate notifyWithName: aName
                               title: aTitle
                         description: aMessage
                            iconData: [NSData data]
                                 key: ind
                              cookie: aCookie];
    return NS_OK;
  }
  
  nsCOMPtr<nsAlertsImageLoadListener> listener =
  new nsAlertsImageLoadListener(aName, aTitle, aMessage, aCookie, ind);
  if (!listener) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  nsCOMPtr<nsIStreamLoader> loader;
  return NS_NewStreamLoader(getter_AddRefs(loader), uri, listener);
  
  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

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

// Notification Center support isn't ready, don't use it.
/*
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
*/

  InitGrowl();

  (void)os->AddObserver(this, DOM_WINDOW_DESTROYED_TOPIC, false);
  (void)os->AddObserver(this, "profile-before-change", false);

  return rv;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult
nsMacAlertsService::InitGrowl() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_ASSERTION([GrowlApplicationBridge growlDelegate] == nil,
               "We already registered with Growl!");

  nsresult rv;
  nsCOMPtr<nsIObserverService> os =
  do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<nsNotificationsList> notifications = new nsNotificationsList();

  if (notifications) {
    (void)os->NotifyObservers(notifications, "before-growl-registration", nullptr);
  }

  mGrowlDelegate = new GrowlDelegateWrapper();

  if (notifications) {
    notifications->informController(mGrowlDelegate->mDelegate);
  }

  [GrowlApplicationBridge setGrowlDelegate: mGrowlDelegate->mDelegate];

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult
nsMacAlertsService::InitNotificationCenter() {
  mNCDelegate = new NotificationCenterDelegateWrapper();
  return NS_OK;
}

nsMacAlertsService::nsMacAlertsService()
: mGrowlDelegate(nullptr)
, mNCDelegate(nullptr)
{}

nsMacAlertsService::~nsMacAlertsService()
{
  delete mGrowlDelegate;
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Growl requires a few extra steps, like checking that it's registered and
  // the alert has a name.
  if (mGrowlDelegate) {
    NS_ASSERTION(mGrowlDelegate->mDelegate == [GrowlApplicationBridge growlDelegate],
                 "Growl Delegate was not registered properly.");

    if (!aAlertName.IsEmpty()) {
      return DispatchGrowlNotification(aAlertName, aImageUrl, aAlertTitle,
                                       aAlertText, aAlertCookie, aAlertListener);
    }

    nsresult rv;
    nsCOMPtr<nsIStringBundleService> bundleService =
    do_GetService("@mozilla.org/intl/stringbundle;1", &rv);

    // We don't want to fail just yet if we can't get the alert name
    nsString name = NS_LITERAL_STRING("General Notification");
    if (NS_SUCCEEDED(rv)) {
      nsCOMPtr<nsIStringBundle> bundle;
      rv = bundleService->CreateBundle(GROWL_STRING_BUNDLE_LOCATION,
                                       getter_AddRefs(bundle));
      if (NS_SUCCEEDED(rv)) {
        rv = bundle->GetStringFromName(NS_LITERAL_STRING("general").get(),
                                       getter_Copies(name));
        if (NS_FAILED(rv)) {
          name = NS_LITERAL_STRING("General Notification");
        }
      }
    }
    
    return DispatchGrowlNotification(name, aImageUrl, aAlertTitle,
                                     aAlertText, aAlertCookie, aAlertListener);
  } else {
    // Notification Center is easy: no image, no name.
    return DispatchNCNotification(mNCDelegate, aAlertTitle, aAlertText,
                                  aAlertCookie, aAlertListener);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIObserver

NS_IMETHODIMP
nsMacAlertsService::Observe(nsISupports* aSubject, const char* aTopic,
                              const PRUnichar* aData)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!mGrowlDelegate && !mNCDelegate) {
    return NS_OK;
  }

  if (strcmp(aTopic, DOM_WINDOW_DESTROYED_TOPIC) == 0) {
    nsCOMPtr<nsIDOMWindow> window(do_QueryInterface(aSubject));
    if (window) {
      if (mGrowlDelegate) {
        [mGrowlDelegate->mDelegate forgetObserversForWindow:window];
      }
      else if (mNCDelegate) {
        [mNCDelegate->mDelegate forgetObserversForWindow:window];
      }
    }
  }
  else if (strcmp(aTopic, "profile-before-change") == 0) {
    if (mGrowlDelegate) {
      [mGrowlDelegate->mDelegate forgetObservers];
    }
    else if (mNCDelegate) {
      [mNCDelegate->mDelegate forgetObservers];
    }
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}
