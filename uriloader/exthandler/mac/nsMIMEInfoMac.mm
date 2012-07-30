/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#import <ApplicationServices/ApplicationServices.h>

#include "nsObjCExceptions.h"
#include "nsMIMEInfoMac.h"
#include "nsILocalFileMac.h"
#include "nsIFileURL.h"

// We override this to make sure app bundles display their pretty name (without .app suffix)
NS_IMETHODIMP nsMIMEInfoMac::GetDefaultDescription(nsAString& aDefaultDescription)
{
  if (mDefaultApplication) {
    nsCOMPtr<nsILocalFileMac> macFile = do_QueryInterface(mDefaultApplication);
    if (macFile) {
      bool isPackage;
      (void)macFile->IsPackage(&isPackage);
      if (isPackage)
        return macFile->GetBundleDisplayName(aDefaultDescription);
    }
  }

  return nsMIMEInfoImpl::GetDefaultDescription(aDefaultDescription);
}

NS_IMETHODIMP
nsMIMEInfoMac::LaunchWithFile(nsIFile *aFile)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsCOMPtr<nsIFile> application;
  nsresult rv;

  NS_ASSERTION(mClass == eMIMEInfo, "only MIME infos are currently allowed"
               "to pass content by value");
  
  if (mPreferredAction == useHelperApp) {

    // we don't yet support passing content by value (rather than reference)
    // to web apps.  at some point, we will probably want to.  
    nsCOMPtr<nsILocalHandlerApp> localHandlerApp =
        do_QueryInterface(mPreferredApplication, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = localHandlerApp->GetExecutable(getter_AddRefs(application));
    NS_ENSURE_SUCCESS(rv, rv);
    
  } else if (mPreferredAction == useSystemDefault) {
    application = mDefaultApplication;
  }
  else
    return NS_ERROR_INVALID_ARG;

  // if we've already got an app, just QI so we have the launchWithDoc method
  nsCOMPtr<nsILocalFileMac> app;
  if (application) {
    app = do_QueryInterface(application, &rv);
    if (NS_FAILED(rv)) return rv;
  } else {
    // otherwise ask LaunchServices for an app directly
    nsCOMPtr<nsILocalFileMac> tempFile = do_QueryInterface(aFile, &rv);
    if (NS_FAILED(rv)) return rv;

    FSRef tempFileRef;
    tempFile->GetFSRef(&tempFileRef);

    FSRef appFSRef;
    if (::LSGetApplicationForItem(&tempFileRef, kLSRolesAll, &appFSRef, nullptr) == noErr)
    {
      app = (do_CreateInstance("@mozilla.org/file/local;1"));
      if (!app) return NS_ERROR_FAILURE;
      app->InitWithFSRef(&appFSRef);
    } else {
      return NS_ERROR_FAILURE;
    }
  }
  return app->LaunchWithDoc(aFile, false);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult 
nsMIMEInfoMac::LoadUriInternal(nsIURI *aURI)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_ENSURE_ARG_POINTER(aURI);

  nsresult rv = NS_ERROR_FAILURE;

  nsCAutoString uri;
  aURI->GetSpec(uri);
  if (!uri.IsEmpty()) {
    CFURLRef myURLRef = ::CFURLCreateWithBytes(kCFAllocatorDefault,
                                               (const UInt8*)uri.get(),
                                               strlen(uri.get()),
                                               kCFStringEncodingUTF8,
                                               NULL);
    if (myURLRef) {
      OSStatus status = ::LSOpenCFURLRef(myURLRef, NULL);
      if (status == noErr)
        rv = NS_OK;
      ::CFRelease(myURLRef);
    }
  }

  return rv;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}
