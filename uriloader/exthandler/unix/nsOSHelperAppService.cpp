/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 */

#include "nsOSHelperAppService.h"
#include "nsISupports.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIURL.h"
#include "nsILocalFile.h"
#include <stdlib.h>		// for system()


nsOSHelperAppService::nsOSHelperAppService() : nsExternalHelperAppService()
{
}

nsOSHelperAppService::~nsOSHelperAppService()
{}

NS_IMETHODIMP nsOSHelperAppService::LaunchAppWithTempFile(nsIMIMEInfo * aMIMEInfo, nsIFile * aTempFile)
{
  nsresult rv = NS_OK;
  if (aMIMEInfo)
  {
    nsCOMPtr<nsIFile> application;
    nsXPIDLCString path;
    aTempFile->GetPath(getter_Copies(path));
    
    aMIMEInfo->GetPreferredApplicationHandler(getter_AddRefs(application));
    if (application)
    {
      // if we were given an application to use then use it....otherwise
      // make the registry call to launch the app
      const char * strPath = (const char *) path;
      application->Spawn(&strPath, 1);
    }    
    else
    {
      // if we had hooks into the OS we can use them here to launch the app
    }
  }

  return rv;
}

NS_IMETHODIMP nsOSHelperAppService::ExternalProtocolHandlerExists(const char * aProtocolScheme, PRBool * aHandlerExists)
{
  // look up the protocol scheme in the windows registry....if we find a match then we have a handler for it...
  *aHandlerExists = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsOSHelperAppService::LoadUrl(nsIURI * aURL)
{
  return NS_ERROR_FAILURE;
}

nsresult nsOSHelperAppService::GetFileTokenForPath(const PRUnichar * platformAppPath, nsIFile ** aFile)
{
  nsCOMPtr<nsILocalFile> localFile (do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
  nsresult rv = NS_OK;

  if (localFile)
  {
    if (localFile)
      localFile->InitWithUnicodePath(platformAppPath);
    *aFile = localFile;
    NS_IF_ADDREF(*aFile);
  }
  else
    rv = NS_ERROR_FAILURE;

  return rv;
}
