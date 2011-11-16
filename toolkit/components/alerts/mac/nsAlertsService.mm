/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 sts=2 expandtab
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Growl implementation of nsIAlertsService.
 *
 * The Initial Developer of the Original Code is
 *   Shawn Wilsher <me@shawnwilsher.com>.
 * Portions created by the Initial Developer are Copyright (C) 2006-2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsAlertsService.h"
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
#import "GrowlApplicationBridge.h"

////////////////////////////////////////////////////////////////////////////////
//// GrowlDelegateWrapper

struct GrowlDelegateWrapper
{
  mozGrowlDelegate* delegate;

  GrowlDelegateWrapper()
  {
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

    delegate = [[mozGrowlDelegate alloc] init];

    NS_OBJC_END_TRY_ABORT_BLOCK;
  }

  ~GrowlDelegateWrapper()
  {
    NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

    [delegate release];

    NS_OBJC_END_TRY_ABORT_BLOCK;
  }
};

/**
 * Helper function to dispatch a notification to Growl
 */
static nsresult
DispatchNamedNotification(const nsAString &aName,
                          const nsAString &aImage,
                          const nsAString &aTitle,
                          const nsAString &aMessage,
                          const nsAString &aCookie,
                          nsIObserver *aListener)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if ([GrowlApplicationBridge isGrowlRunning] == NO)
    return NS_ERROR_NOT_AVAILABLE;

  mozGrowlDelegate *delegate =
    static_cast<mozGrowlDelegate *>([GrowlApplicationBridge growlDelegate]);
  if (!delegate)
    return NS_ERROR_NOT_AVAILABLE;

  PRUint32 ind = 0;
  if (aListener)
    ind = [delegate addObserver: aListener];

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
  if (!listener)
    return NS_ERROR_OUT_OF_MEMORY;

  nsCOMPtr<nsIStreamLoader> loader;
  return NS_NewStreamLoader(getter_AddRefs(loader), uri, listener);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

////////////////////////////////////////////////////////////////////////////////
//// nsAlertsService

NS_IMPL_THREADSAFE_ADDREF(nsAlertsService)
NS_IMPL_THREADSAFE_RELEASE(nsAlertsService)

NS_INTERFACE_MAP_BEGIN(nsAlertsService)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAlertsService)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIAlertsService)
NS_INTERFACE_MAP_END_THREADSAFE

nsresult
nsAlertsService::Init()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_ASSERTION([GrowlApplicationBridge growlDelegate] == nil,
               "We already registered with Growl!");

  nsresult rv;
  nsCOMPtr<nsIObserverService> os =
    do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<nsNotificationsList> notifications = new nsNotificationsList();

  if (notifications)
    (void)os->NotifyObservers(notifications, "before-growl-registration", nsnull);

  mDelegate = new GrowlDelegateWrapper();

  if (notifications)
    notifications->informController(mDelegate->delegate);

  // registers with Growl
  [GrowlApplicationBridge setGrowlDelegate: mDelegate->delegate];

  (void)os->AddObserver(this, DOM_WINDOW_DESTROYED_TOPIC, false);
  (void)os->AddObserver(this, "profile-before-change", false);

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsAlertsService::nsAlertsService() : mDelegate(nsnull) {}

nsAlertsService::~nsAlertsService()
{
  delete mDelegate;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIAlertsService

NS_IMETHODIMP
nsAlertsService::ShowAlertNotification(const nsAString& aImageUrl,
                                       const nsAString& aAlertTitle,
                                       const nsAString& aAlertText,
                                       bool aAlertClickable,
                                       const nsAString& aAlertCookie,
                                       nsIObserver* aAlertListener,
                                       const nsAString& aAlertName)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_ASSERTION(mDelegate->delegate == [GrowlApplicationBridge growlDelegate],
               "Growl Delegate was not registered properly.");

  if (!aAlertName.IsEmpty()) {
    return DispatchNamedNotification(aAlertName, aImageUrl, aAlertTitle,
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
      if (NS_FAILED(rv))
        name = NS_LITERAL_STRING("General Notification");
    }
  }

  return DispatchNamedNotification(name, aImageUrl, aAlertTitle,
                                   aAlertText, aAlertCookie, aAlertListener);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

////////////////////////////////////////////////////////////////////////////////
//// nsIObserver

NS_IMETHODIMP
nsAlertsService::Observe(nsISupports* aSubject, const char* aTopic,
                         const PRUnichar* aData)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!mDelegate)
    return NS_OK;

  if (strcmp(aTopic, DOM_WINDOW_DESTROYED_TOPIC) == 0) {
    nsCOMPtr<nsIDOMWindow> window(do_QueryInterface(aSubject));
    if (window)
      [mDelegate->delegate forgetObserversForWindow:window];
  }
  else if (strcmp(aTopic, "profile-before-change") == 0) {
    [mDelegate->delegate forgetObservers];
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}
