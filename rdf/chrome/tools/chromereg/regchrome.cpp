/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 * 
 */

#include "nsIServiceManager.h"
#include "nsCOMPtr.h"
#include "nsIChromeRegistry.h"
#include "nsAppFileLocationProvider.h"

int main(int argc, char **argv)
{
  NS_InitXPCOM(nsnull, nsnull);

  nsCOMPtr <nsIChromeRegistry> chromeReg = 
    do_GetService("@mozilla.org/chrome/chrome-registry;1");
  if (!chromeReg) {
    NS_WARNING("chrome check couldn't get the chrome registry");
    return NS_ERROR_FAILURE;
  }
  // initialize the directory service so that the chrome directory can
  // be found when registering new chrome.
  nsIDirectoryServiceProvider *appFileLocProvider;
  appFileLocProvider = new nsAppFileLocationProvider;
  if (!appFileLocProvider) {
    NS_WARNING("failed to create directory service provider\n");
    return NS_ERROR_FAILURE;
  }
  // add a reference
  NS_ADDREF(appFileLocProvider);
  nsresult rv;
  NS_WITH_SERVICE(nsIDirectoryService, directoryService,
		  NS_DIRECTORY_SERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    NS_WARNING("failed to get directory service");
    return rv;
  }
  rv = directoryService->RegisterProvider(appFileLocProvider);
  if (NS_FAILED(rv)) {
    NS_WARNING("failed to register provider");
    return rv;
  }
  NS_RELEASE(appFileLocProvider); // RegisterProvider did AddRef - It owns it now
  chromeReg->CheckForNewChrome();
  // release the directory service before we shutdown XPCOM
  directoryService = 0;
  // release the chrome registry before we shutdown XPCOM
  chromeReg = 0;
  NS_ShutdownXPCOM(nsnull);
}
