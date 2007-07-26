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

#include <LaunchServices.h>

#include "nsMIMEInfoMac.h"
#include "nsILocalFileMac.h"
#include "nsIFileURL.h"
#include "nsIInternetConfigService.h"

NS_IMETHODIMP
nsMIMEInfoMac::LaunchWithURI(nsIURI* aURI)
{
  nsCOMPtr<nsIFile> application;
  nsresult rv;
  
  if (mPreferredAction == useHelperApp) {

    // check for and launch with web handler app
    nsCOMPtr<nsIWebHandlerApp> webHandlerApp =
      do_QueryInterface(mPreferredApplication, &rv);
    if (NS_SUCCEEDED(rv)) {
      return LaunchWithWebHandler(webHandlerApp, aURI);         
    }

    // otherwise, get the application executable from the handler
    nsCOMPtr<nsILocalHandlerApp> localHandlerApp =
        do_QueryInterface(mPreferredApplication, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    
    rv = localHandlerApp->GetExecutable(getter_AddRefs(application));
    NS_ENSURE_SUCCESS(rv, rv);
    
  } else if (mPreferredAction == useSystemDefault)
    application = mDefaultApplication;
  else
    return NS_ERROR_INVALID_ARG;

  if (mClass == eProtocolInfo)
    return LoadUriInternal(aURI);

  // get the nsILocalFile version of the doc to launch with
  nsCOMPtr<nsILocalFile> docToLoad;
  rv = GetLocalFileFromURI(aURI, getter_AddRefs(docToLoad));
  NS_ENSURE_SUCCESS(rv, rv);

  // if we've already got an app, just QI so we have the launchWithDoc method
  nsCOMPtr<nsILocalFileMac> app;
  if (application) {
    app = do_QueryInterface(application, &rv);
    if (NS_FAILED(rv)) return rv;
  } else {
    // otherwise ask LaunchServices for an app directly
    nsCOMPtr<nsILocalFileMac> tempFile = do_QueryInterface(docToLoad, &rv);
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
  
  return app->LaunchWithDoc(docToLoad, PR_FALSE); 
}

nsresult 
nsMIMEInfoMac::LoadUriInternal(nsIURI *aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
  nsresult rv = NS_ERROR_FAILURE;
  
  nsCAutoString uri;
  aURI->GetSpec(uri);
  if (!uri.IsEmpty()) {
    nsCOMPtr<nsIInternetConfigService> icService = 
      do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID);
    if (icService)
      rv = icService->LaunchURL(uri.get());
  }
  return rv;
}

NS_IMETHODIMP
nsMIMEInfoMac::GetHasDefaultHandler(PRBool *_retval)
{
  // We have a default application if we have a description
  *_retval = !mDefaultAppDescription.IsEmpty();
  return NS_OK;
}

