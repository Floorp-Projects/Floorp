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
#include "nsIStringBundle.h"
#include "nsIPromptService.h"
#include "nsMemory.h"

#include "nsIInternetConfigService.h"

// chrome URL's
#define HELPERAPPLAUNCHER_BUNDLE_URL "chrome://global/locale/helperAppLauncher.properties"
#define BRAND_BUNDLE_URL "chrome://global/locale/brand.properties"

#define NS_PROMPTSERVICE_CONTRACTID "@mozilla.org/embedcomp/prompt-service;1"

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

    nsMIMEInfoHandleAction action = nsIMIMEInfo::useSystemDefault;
    aMIMEInfo->GetPreferredAction(&action);

    if (action==nsIMIMEInfo::useHelperApp)
    {
      aMIMEInfo->GetPreferredApplicationHandler(getter_AddRefs(application));
    }
    else
    {
      aMIMEInfo->GetDefaultApplicationHandler(getter_AddRefs(application));
    }

    if (!application)
        return NS_ERROR_FAILURE;

    nsCOMPtr <nsILocalFileMac> app = do_QueryInterface(application, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr <nsILocalFile> docToLoad = do_QueryInterface(aTempFile, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = app->LaunchWithDoc(docToLoad, PR_FALSE); 
  }
  return rv;
}

NS_IMETHODIMP nsOSHelperAppService::ExternalProtocolHandlerExists(const char * aProtocolScheme, PRBool * aHandlerExists)
{
  // look up the protocol scheme in Internet Config....if we find a match then we have a handler for it...
  *aHandlerExists = PR_FALSE;
  // ask the internet config service to look it up for us...
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIInternetConfigService> icService (do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID));
  if (icService)
  {
    rv = icService->HasProtocolHandler(aProtocolScheme, aHandlerExists);
    if (rv == NS_ERROR_NOT_AVAILABLE)
    {
      // current app is registered to handle the protocol, put up an alert
      nsCOMPtr<nsIStringBundleService> stringBundleService = do_GetService(NS_STRINGBUNDLE_CONTRACTID);
      if (stringBundleService)
      {
        nsCOMPtr<nsIStringBundle> appLauncherBundle;
        rv = stringBundleService->CreateBundle(HELPERAPPLAUNCHER_BUNDLE_URL, getter_AddRefs(appLauncherBundle));
        if (rv == NS_OK)
        {
          nsCOMPtr<nsIStringBundle> brandBundle;
          rv = stringBundleService->CreateBundle(BRAND_BUNDLE_URL, getter_AddRefs(brandBundle));
          if (rv == NS_OK)
          {
            nsXPIDLString brandName;
            rv = brandBundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(), getter_Copies(brandName));
            if (rv == NS_OK)
            {
              nsXPIDLString errorStr;
              NS_ConvertASCIItoUCS2 proto(aProtocolScheme);
              const PRUnichar *formatStrings[] = { brandName.get(), proto.get() };
              rv = appLauncherBundle->FormatStringFromName(NS_LITERAL_STRING("protocolNotHandled").get(),
                                                           formatStrings,
                                                           2,
                                                           getter_Copies(errorStr));
              if (rv == NS_OK)
              {
                nsCOMPtr<nsIPromptService> prompt (do_GetService(NS_PROMPTSERVICE_CONTRACTID));
                if (prompt)
                  prompt->Alert(nsnull, NS_LITERAL_STRING("Alert").get(), errorStr.get());
              }
            }
          }
        }
      }
    }
  }
  return rv;
}

NS_IMETHODIMP nsOSHelperAppService::LoadUrl(nsIURI * aURL)
{
  nsCAutoString url;
  nsresult rv = NS_ERROR_FAILURE;
  
  if (aURL)
  {
    aURL->GetSpec(url);
    if (!url.IsEmpty())
    {
      nsCOMPtr<nsIInternetConfigService> icService (do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID));
      if (icService)
        rv = icService->LaunchURL(url.get());
    }
  }
  return rv;
}

nsresult nsOSHelperAppService::GetFileTokenForPath(const PRUnichar * platformAppPath, nsIFile ** aFile)
{
  nsCOMPtr<nsILocalFile> localFile (do_CreateInstance(NS_LOCAL_FILE_CONTRACTID));
  if (!localFile)
    return NS_ERROR_FAILURE;
  
  localFile->InitWithPath(nsDependentString(platformAppPath));
  *aFile = localFile;
  NS_IF_ADDREF(*aFile);

  return NS_OK;
}
///////////////////////////
// method overrides --> use internet config information for mime type lookup.
///////////////////////////

NS_IMETHODIMP nsOSHelperAppService::GetFromExtension(const char * aFileExt, nsIMIMEInfo ** aMIMEInfo)
{
  // first, ask our base class....
  nsresult rv = nsExternalHelperAppService::GetFromExtension(aFileExt, aMIMEInfo);
  if (NS_SUCCEEDED(rv) && *aMIMEInfo) 
  {
    UpdateCreatorInfo(*aMIMEInfo);
  }
  return rv;
}
  
nsresult nsOSHelperAppService::GetMIMEInfoForExtensionFromOS(const char * aFileExt, nsIMIMEInfo ** aMIMEInfo)
{
  nsresult rv;
  
  // ask the internet config service to look it up for us...
  nsCOMPtr<nsIInternetConfigService> icService (do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID));
  if (icService)
  {
    rv = icService->GetMIMEInfoFromExtension(aFileExt, aMIMEInfo);
    // if we got an entry, don't waste time hitting IC for this information next time, store it in our
    // hash table....
    // XXX Once cache can be invalidated, add the mime info to it.  See bug 121644
    // if (NS_SUCCEEDED(rv) && *aMIMEInfo)
    //  	AddMimeInfoToCache(*aMIMEInfo);    
  }
  
  if (!*aMIMEInfo) rv = NS_ERROR_FAILURE;
  return rv;
}

NS_IMETHODIMP nsOSHelperAppService::GetFromMIMEType(const char * aMIMEType, nsIMIMEInfo ** aMIMEInfo)
{
  // first, ask our base class....
  nsresult rv = nsExternalHelperAppService::GetFromMIMEType(aMIMEType, aMIMEInfo);
  if (NS_SUCCEEDED(rv) && *aMIMEInfo) 
  {
    UpdateCreatorInfo(*aMIMEInfo);
  }
  return rv;
}
  
nsresult nsOSHelperAppService::GetMIMEInfoForMimeTypeFromOS(const char * aMIMEType, nsIMIMEInfo ** aMIMEInfo)
{
  nsresult rv;

  // ask the internet config service to look it up for us...
  nsCOMPtr<nsIInternetConfigService> icService (do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID));
  if (icService)
  {
    rv = icService->FillInMIMEInfo(aMIMEType, nsnull, aMIMEInfo);
    // if we got an entry, don't waste time hitting IC for this information next time, store it in our
    // hash table....
    // XXX Once cache can be invalidated, add the mime info to it.  See bug 121644
    // if (NS_SUCCEEDED(rv) && *aMIMEInfo)
    //   AddMimeInfoToCache(*aMIMEInfo);    
  }
  
  if (!*aMIMEInfo) rv = NS_ERROR_FAILURE;
  return rv;
}

// we never want to use a hard coded value for the creator and file type for the mac. always look these values up
// from internet config.
void nsOSHelperAppService::UpdateCreatorInfo(nsIMIMEInfo * aMIMEInfo)
{
  PRUint32 macCreatorType;
  PRUint32 macFileType;
  aMIMEInfo->GetMacType(&macFileType);
  aMIMEInfo->GetMacCreator(&macCreatorType);
  
  if (macFileType == 0 || macCreatorType == 0)
  {
    // okay these values haven't been initialized yet so fetch a mime object from internet config.
    nsXPIDLCString mimeType;
    aMIMEInfo->GetMIMEType(getter_Copies(mimeType));
    nsCOMPtr<nsIInternetConfigService> icService (do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID));
    if (icService)
    {
      nsCOMPtr<nsIMIMEInfo> osMimeObject;
      icService->FillInMIMEInfo(mimeType, nsnull, getter_AddRefs(osMimeObject));
      if (osMimeObject)
      {
        osMimeObject->GetMacType(&macFileType);
        osMimeObject->GetMacCreator(&macCreatorType);
        aMIMEInfo->SetMacCreator(macCreatorType);
        aMIMEInfo->SetMacType(macFileType);
      } // if we got an os object
    } // if we got the ic service
  } // if the creator or file type hasn't been initialized yet
} 
