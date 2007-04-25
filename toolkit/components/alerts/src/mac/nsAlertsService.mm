/* ***** BEGIN LICENSE BLOCK *****
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
#include "nsString.h"
#include "nsAlertsImageLoadListener.h"
#include "nsIURI.h"
#include "nsIStreamLoader.h"
#include "nsNetUtil.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsIServiceManager.h"
#include "nsICategoryManager.h"
#include "nsMemory.h"

#import "mozGrowlDelegate.h"
#import "GrowlApplicationBridge.h"

NS_IMPL_THREADSAFE_ADDREF(nsAlertsService)
NS_IMPL_THREADSAFE_RELEASE(nsAlertsService)

NS_INTERFACE_MAP_BEGIN(nsAlertsService)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAlertsService)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsIAlertsService)
NS_INTERFACE_MAP_END_THREADSAFE

struct GrowlDelegateWrapper
{
  mozGrowlDelegate* delegate;

  GrowlDelegateWrapper()
  {
    delegate = [[mozGrowlDelegate alloc] init];
  }

  ~GrowlDelegateWrapper()
  {
    [delegate release];
  }
};

nsresult
nsAlertsService::Init()
{
  if ([GrowlApplicationBridge isGrowlInstalled] == NO)
    return NS_ERROR_SERVICE_NOT_AVAILABLE;

  mDelegate = new GrowlDelegateWrapper();

  return NS_OK;
}

nsAlertsService::nsAlertsService() : mDelegate(nsnull) {}

nsAlertsService::~nsAlertsService()
{
  if (mDelegate)
    delete mDelegate;
}

NS_IMETHODIMP
nsAlertsService::ShowAlertNotification(const nsAString& aImageUrl,
                                       const nsAString& aAlertTitle,
                                       const nsAString& aAlertText,
                                       PRBool aAlertClickable,
                                       const nsAString& aAlertCookie,
                                       nsIObserver* aAlertListener)
{

  NS_ASSERTION(mDelegate->delegate == [GrowlApplicationBridge growlDelegate],
               "Growl Delegate was not registered properly.");

  PRUint32 ind = 0;
  if (aAlertListener)
    ind = [mDelegate->delegate addObserver: aAlertListener];

  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aImageUrl);
  if (NS_FAILED(rv)) {
    // image uri failed to resolve, so dispatch to growl with no image
    [mozGrowlDelegate notifyWithTitle: aAlertTitle
                          description: aAlertText
                             iconData: [NSData data]
                                  key: ind
                               cookie: aAlertCookie];

    return NS_OK;
  }

  nsCOMPtr<nsAlertsImageLoadListener> listener =
    new nsAlertsImageLoadListener(aAlertTitle, aAlertText, aAlertClickable,
                                  aAlertCookie, ind);

  nsCOMPtr<nsIStreamLoader> loader;
  rv = NS_NewStreamLoader(getter_AddRefs(loader), uri, listener);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsAlertsService::Observe(nsISupports* aSubject, const char* aTopic,
                         const PRUnichar* aData)
{
  if (nsCRT::strcmp(aTopic, "app-startup") == 0) // registers with Growl here
    [GrowlApplicationBridge setGrowlDelegate: mDelegate->delegate];

  return NS_OK;
}

NS_METHOD
nsAlertsServiceRegister(nsIComponentManager* aCompMgr,
                        nsIFile* aPath,
                        const char* registryLocation,
                        const char* componentType,
                        const nsModuleComponentInfo* info)
{
  nsresult rv;

  nsCOMPtr<nsICategoryManager> catman =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  char* prev = nsnull;
  rv = catman->AddCategoryEntry("app-startup", "nsAlertsService",
                                NS_ALERTSERVICE_CONTRACTID, PR_TRUE, PR_TRUE,
                                &prev);
  if (prev)
    nsMemory::Free(prev);

  return rv;
}

NS_METHOD
nsAlertsServiceUnregister(nsIComponentManager* aCompMgr,
                          nsIFile* aPath,
                          const char* registryLocation,
                          const nsModuleComponentInfo* info)
{
  nsresult rv;

  nsCOMPtr<nsICategoryManager> catman =
    do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = catman->DeleteCategoryEntry("app-startup", "nsAlertsService", PR_TRUE);

  return rv;
}
