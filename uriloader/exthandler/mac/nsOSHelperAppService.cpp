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
#include "nsILocalFileMac.h"
#include "nsMimeTypes.h"

#include "nsIInternetConfigService.h"

nsOSHelperAppService::nsOSHelperAppService() : nsExternalHelperAppService()
{
 	nsExternalHelperAppService::Init();
}

nsOSHelperAppService::~nsOSHelperAppService()
{}


NS_IMETHODIMP nsOSHelperAppService::CanHandleContent(const char *aMimeContentType, nsIURI * aURI, PRBool * aCanHandleContent)
{
  // once we have user over ride stuff working, we need to first call up to our base class
  // and ask the base class if we can handle the content. This will take care of looking for user specified 
  // apps for content types.
  
  // for now we only have defaults to worry about...
  // go look up in the windows registry to see if there is a handler for this mime type...if there is return TRUE...
  *aCanHandleContent = PR_FALSE;
  nsresult rv = nsExternalHelperAppService::CanHandleContent(aMimeContentType, aURI,aCanHandleContent);

  if (NS_FAILED(rv) || *aCanHandleContent == PR_FALSE)
  {
    nsCOMPtr<nsIInternetConfigService> icService (do_GetService(NS_INTERNETCONFIGSERVICE_PROGID));
    if (icService)
    {
      rv = icService->HasMappingForMIMEType(aMimeContentType, aCanHandleContent);
    }
  }
  return rv;
}

NS_IMETHODIMP nsOSHelperAppService::DoContent(const char *aMimeContentType, nsIURI *aURI, nsISupports *aWindowContext, 
                                                    PRBool *aAbortProcess, nsIStreamListener ** aStreamListener)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIURL> url = do_QueryInterface(aURI);
  
  // see if we have user specified information for handling this content type by giving the base class
  // first crack at it...
  
  rv = nsExternalHelperAppService::DoContent(aMimeContentType, aURI, aWindowContext, aAbortProcess, aStreamListener);
  
  // this is important!! if do content for the base class returned any success code, then assume we are done
  // and don't even play around with 
  if (NS_SUCCEEDED(rv))
  {
    if (((strcmp(aMimeContentType, UNKNOWN_CONTENT_TYPE) == 0) ||
         (strcmp(aMimeContentType, APPLICATION_OCTET_STREAM) == 0)) &&
         url)
    {
      // trap for unknown or octet-stream to check for a ".bin" file
      char str[16];
      char *strptr = str;
      url->GetFileExtension(&strptr);
      if (*strptr)
      {
        if (strcmp(strptr, "bin") != 0)
        {
          return NS_OK;
        }
        else
        {
          // it's a ".bin" file, on Mac, it's probably a MacBinary file
          // do lookup in Internet Config
          rv = NS_ERROR_FAILURE;
        }
      }
    }
    else
    {
      return NS_OK;
    }
  }
  
  *aStreamListener = nsnull;
  // use InternetConfig to fill in MIMEInfo
  nsCOMPtr<nsIMIMEInfo> mimeInfo;
  nsCOMPtr<nsIInternetConfigService> icService (do_GetService(NS_INTERNETCONFIGSERVICE_PROGID));
  if (icService)
  {
    if (url)
      rv = icService->FillInMIMEInfo(aMimeContentType, url, getter_AddRefs(mimeInfo));
    else
      rv = icService->FillInMIMEInfo(aMimeContentType, nsnull, getter_AddRefs(mimeInfo));
    if (NS_SUCCEEDED(rv) && mimeInfo)
    {
      // this code is incomplete and just here to get things started..
      nsXPIDLCString fileExtension;
      mimeInfo->FirstExtension(getter_Copies(fileExtension));
      nsExternalAppHandler * handler = CreateNewExternalHandler(mimeInfo, fileExtension, aWindowContext);
      handler->QueryInterface(NS_GET_IID(nsIStreamListener), (void **) aStreamListener);
      rv = NS_OK;
    }
  }
  return rv;
}

NS_IMETHODIMP nsOSHelperAppService::LaunchAppWithTempFile(nsIMIMEInfo * aMIMEInfo, nsIFile * aTempFile)
{
  nsresult rv = NS_OK;
  if (aMIMEInfo)
  {
    nsCOMPtr<nsIFile> application;   
    aMIMEInfo->GetPreferredApplicationHandler(getter_AddRefs(application));
    if (application)
    {
  	  nsCOMPtr <nsILocalFileMac> app = do_QueryInterface(application, &rv);
  	  if (NS_FAILED(rv)) return rv;
  	
  	  nsCOMPtr <nsILocalFile> docToLoad = do_QueryInterface(aTempFile, &rv);
  	  if (NS_FAILED(rv)) return rv;
  	
  	  rv = app->LaunchAppWithDoc(docToLoad, PR_FALSE); 
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
  nsCOMPtr<nsILocalFile> localFile (do_CreateInstance(NS_LOCAL_FILE_PROGID));
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
