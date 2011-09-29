/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 *   Christian Biesinger <cbiesinger@web.de>
 *   Dan Mosedale <dmose@mozilla.org>
 *   Stan Shebs <stanshebs@earthlink.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
    if (::LSGetApplicationForItem(&tempFileRef, kLSRolesAll, &appFSRef, nsnull) == noErr)
    {
      app = (do_CreateInstance("@mozilla.org/file/local;1"));
      if (!app) return NS_ERROR_FAILURE;
      app->InitWithFSRef(&appFSRef);
    } else {
      return NS_ERROR_FAILURE;
    }
  }
  nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(aFile);
  return app->LaunchWithDoc(localFile, PR_FALSE);

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
