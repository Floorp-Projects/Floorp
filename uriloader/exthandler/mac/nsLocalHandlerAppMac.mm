/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
