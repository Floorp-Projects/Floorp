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
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dan Mosedale <dmose@mozilla.org>
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

#import <CoreFoundation/CoreFoundation.h>
#import <ApplicationServices/ApplicationServices.h>

#include "nsObjCExceptions.h"
#include "nsLocalHandlerAppMac.h"
#include "nsILocalFileMac.h"
#include "nsIURI.h"

// We override this to make sure app bundles display their pretty name (without .app suffix)
NS_IMETHODIMP nsLocalHandlerAppMac::GetName(nsAString& aName)
{
  if (mExecutable) {
    nsCOMPtr<nsILocalFileMac> macFile = do_QueryInterface(mExecutable);
    if (macFile) {
      bool isPackage;
      (void)macFile->IsPackage(&isPackage);
      if (isPackage)
        return macFile->GetBundleDisplayName(aName);
    }
  }

  return nsLocalHandlerApp::GetName(aName);
}

/** 
 * mostly copy/pasted from nsMacShellService.cpp (which is in browser/,
 * so we can't depend on it here).  This code probably really wants to live
 * somewhere more central (see bug 389922).
 */
NS_IMETHODIMP
nsLocalHandlerAppMac::LaunchWithURI(nsIURI *aURI,
                                    nsIInterfaceRequestor *aWindowContext)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsresult rv;
  nsCOMPtr<nsILocalFileMac> lfm(do_QueryInterface(mExecutable, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  
  CFURLRef appURL;
  rv = lfm->GetCFURL(&appURL);
  if (NS_FAILED(rv))
    return rv;
  
  nsCAutoString uriSpec;
  aURI->GetAsciiSpec(uriSpec);

  const UInt8* uriString = reinterpret_cast<const UInt8*>(uriSpec.get());
  CFURLRef uri = ::CFURLCreateWithBytes(NULL, uriString, uriSpec.Length(),
                                        kCFStringEncodingUTF8, NULL);
  if (!uri) {
    ::CFRelease(appURL);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  CFArrayRef uris = ::CFArrayCreate(NULL, reinterpret_cast<const void**>(&uri),
                                    1, NULL);
  if (!uris) {
    ::CFRelease(uri);
    ::CFRelease(appURL);
    return NS_ERROR_OUT_OF_MEMORY;
  }
  
  LSLaunchURLSpec launchSpec;
  launchSpec.appURL = appURL;
  launchSpec.itemURLs = uris;
  launchSpec.passThruParams = NULL;
  launchSpec.launchFlags = kLSLaunchDefaults;
  launchSpec.asyncRefCon = NULL;
  
  OSErr err = ::LSOpenFromURLSpec(&launchSpec, NULL);
  
  ::CFRelease(uris);
  ::CFRelease(uri);
  ::CFRelease(appURL);
  
  return err != noErr ? NS_ERROR_FAILURE : NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}
