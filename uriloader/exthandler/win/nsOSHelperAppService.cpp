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
#include "nsIMIMEInfo.h"
#include "nsILocalFile.h"

// we need windows.h to read out registry information...
#include <windows.h>

// helper methods: forward declarations...
BYTE * GetValueBytes( HKEY hKey, const char *pValueName);
nsresult GetExtensionFrom4xRegistryInfo(const char * aMimeType, nsCString& aFileExtension);

nsOSHelperAppService::nsOSHelperAppService() : nsExternalHelperAppService()
{}

nsOSHelperAppService::~nsOSHelperAppService()
{}


NS_IMETHODIMP nsOSHelperAppService::CanHandleContent(const char *aMimeContentType, nsIURI * aURI, PRBool * aCanHandleContent)
{
  // once we have user over ride stuff working, we need to first call up to our base class
  // and ask the base class if we can handle the content. This will take care of looking for user specified 
  // apps for content types.

  *aCanHandleContent = PR_FALSE;
  nsresult rv = nsExternalHelperAppService::CanHandleContent(aMimeContentType, aURI,aCanHandleContent);

  if (NS_FAILED(rv) || *aCanHandleContent == PR_FALSE)
  {

  }
  return NS_OK;
}

nsresult nsOSHelperAppService::FindOSMimeInfoForType(const char * aMimeContentType, nsIURI * aURI, char ** aFileExtension, nsIMIMEInfo ** aMIMEInfo)
{
  nsresult rv = NS_OK;
  nsCAutoString fileExtension;
  nsCOMPtr<nsIMIMEInfo> mimeInfo;

  // (1) ask the base class if they have a mime info object for this content type...
  rv = GetMIMEInfoForMimeType(aMimeContentType, getter_AddRefs(mimeInfo));
  if (mimeInfo)
  {
    nsXPIDLCString mimefileExt;
    mimeInfo->FirstExtension(getter_Copies(mimefileExt));
    fileExtension = ".";  
    fileExtension.Append(mimefileExt);
  }

  // we don't have a mozilla override extension for this content type....
  // try looking in the netscape windows registry to see if we got lucky
  // and have pre-populated the registry with mappings
  if (fileExtension.IsEmpty()) 
    rv = GetExtensionFrom4xRegistryInfo(aMimeContentType, fileExtension);

  if (FAILED(rv) || fileExtension.IsEmpty())
  {
    // if we couldn't find one, don't give up yet! Try and see if there is an extension in the 
    // url itself...
    nsCOMPtr<nsIURL> url = do_QueryInterface(aURI);

    if (url)
    {
      nsXPIDLCString extenion;
      url->GetFileExtension(getter_Copies(extenion));
    
     fileExtension = ".";  
     fileExtension.Append(extenion);
    }
  } // if we couldn't get extension information from the registry...

  // look up the content type and get a platform specific handle to the app we want to use for this 
  // download...create a nsExternalAppHandler, bind the application token to it (as a nsIFile??) and return this
  // as the stream listener to use...

  if (!fileExtension.IsEmpty())
  {
     HKEY hKey;
     LONG err = ::RegOpenKeyEx( HKEY_CLASSES_ROOT, fileExtension, 0, KEY_QUERY_VALUE, &hKey);
     if (err == ERROR_SUCCESS)
     {
        LPBYTE pBytes = GetValueBytes( hKey, "Content Type");
        LPBYTE pFileDescription = GetValueBytes(hKey, "");

        // create a new mime info object and initialize it if we don't have one already...
        if (!mimeInfo)
        {
          mimeInfo = do_CreateInstance(NS_MIMEINFO_CONTRACTID);
          if (mimeInfo)
          {
            mimeInfo->SetMIMEType((char *) pBytes);
            // if the file extension includes the '.' then we don't want to include that when we append
            // it to the mime info object.
            const char * ext = fileExtension.GetBuffer();
            if (ext && *ext == '.' && (ext+1))
              mimeInfo->AppendExtension(ext+1);
            else
              mimeInfo->AppendExtension(fileExtension);

            mimeInfo->SetPreferredAction(nsIMIMEInfo::useSystemDefault);

            // the following is just a test for right now...I'll find something useful out of the registry
            // or just stuff the file executable here...
            nsAutoString description;
            description.AssignWithConversion((char *) pFileDescription);

            PRInt32 pos = description.FindChar('.', PR_TRUE);
            if (pos > 0) 
              description.Truncate(pos); 
            // the format of the description usually looks like appname.version.something.
            // for now, let's try to make it pretty and just show you the appname.

            mimeInfo->SetApplicationDescription(description.GetUnicode());
          }
        }

        delete [] pBytes;
        delete [] pFileDescription;

        // close the key
       ::RegCloseKey(hKey);

     } // if we got an entry out of the registry...

     // this is the ONLY code path which leads to success where we should set our return variables...
     *aMIMEInfo = mimeInfo;
     NS_IF_ADDREF(*aMIMEInfo);

     *aFileExtension = fileExtension.ToNewCString();

  } // if we have a file extension

  return rv;
}

NS_IMETHODIMP nsOSHelperAppService::DoContent(const char *aMimeContentType, nsIURI *aURI, nsISupports *aWindowContext, 
                                                    PRBool *aAbortProcess, nsIStreamListener ** aStreamListener)
{
  nsresult rv = NS_OK;

  // see if we have user specified information for handling this content type by giving the base class
  // first crack at it...

  rv = nsExternalHelperAppService::DoContent(aMimeContentType, aURI, aWindowContext, aAbortProcess, aStreamListener);
  
  // this is important!! if do content for the base class returned any success code, then assume we are done
  // and don't even play around with 
  if (NS_SUCCEEDED(rv)) return NS_OK;

  // okay the base class couldn't do anything so now it's our turn!!!

  // ACK!!! we've done all this work to discover the content type just to find out that windows
  // registery uses the extension to figure out the right helper app....that's a bummer...
  // now we need to try to get the extension for the content type...

  nsCOMPtr<nsIMIMEInfo> mimeInfo;
  nsXPIDLCString fileExtension;
  rv = FindOSMimeInfoForType(aMimeContentType, aURI, getter_Copies(fileExtension), getter_AddRefs(mimeInfo));

  *aStreamListener = nsnull;
  if (NS_SUCCEEDED(rv) && mimeInfo)
  {
    // this code is incomplete and just here to get things started..
    nsExternalAppHandler * handler = CreateNewExternalHandler(mimeInfo, fileExtension, aWindowContext);
    handler->QueryInterface(NS_GET_IID(nsIStreamListener), (void **) aStreamListener);
  }

  return rv;
}

NS_IMETHODIMP nsOSHelperAppService::LaunchAppWithTempFile(nsIMIMEInfo * aMIMEInfo, nsIFile * aTempFile)
{
  nsresult rv = NS_OK;
  if (aMIMEInfo)
  {
    nsCOMPtr<nsIFile> application;
    nsXPIDLCString path;
    aTempFile->GetPath(getter_Copies(path));

    nsMIMEInfoHandleAction action = nsIMIMEInfo::useSystemDefault;
    aMIMEInfo->GetPreferredAction(&action);
    
    aMIMEInfo->GetPreferredApplicationHandler(getter_AddRefs(application));
    if (application && action == nsIMIMEInfo::useHelperApp)
    {
      // if we were given an application to use then use it....otherwise
      // make the registry call to launch the app
      const char * strPath = (const char *) path;
      application->Spawn(&strPath, 1);
    }    
    else // use the system default
    {
      // use the app registry name to launch a shell execute....
      LONG r = (LONG) ::ShellExecute( NULL, "open", (const char *) path, NULL, NULL, SW_SHOWNORMAL);
      if (r < 32) 
        rv = NS_ERROR_FAILURE;
			else
			  rv = NS_OK;
    }
  }

  return rv;
}

// We have a serious problem!! I have this content type and the windows registry only gives me
// helper apps based on extension. Right now, we really don't have a good place to go for 
// trying to figure out the extension for a particular mime type....One short term hack is to look
// this information in 4.x (it's stored in the windows regsitry). 
nsresult GetExtensionFrom4xRegistryInfo(const char * aMimeType, nsCString& aFileExtension)
{
   nsCAutoString command ("Software\\Netscape\\Netscape Navigator\\Suffixes");
   nsresult rv = NS_OK;
   HKEY hKey;
   LONG err = ::RegOpenKeyEx( HKEY_CURRENT_USER, command, 0, KEY_QUERY_VALUE, &hKey);
   if (err == ERROR_SUCCESS)
   {
      LPBYTE pBytes = GetValueBytes( hKey, aMimeType);
      if (pBytes) // only try to get the extension if we have a value!
      {
        aFileExtension = ".";
        aFileExtension.Append( (char *) pBytes);
      
        // this may be a comma separate list of extensions...just take the first one
        // for now...

        PRInt32 pos = aFileExtension.FindChar(',', PR_TRUE);
        if (pos > 0) // we have a comma separated list of languages...
          aFileExtension.Truncate(pos); // truncate everything after the first comma (including the comma)
      }
   
      delete [] pBytes;
      // close the key
      ::RegCloseKey(hKey);
   }
   else
     rv = NS_ERROR_FAILURE; // no 4.x extension mapping found!

   return rv;
}

BYTE * GetValueBytes( HKEY hKey, const char *pValueName)
{
	LONG	err;
	DWORD	bufSz;
	LPBYTE	pBytes = NULL;

	err = ::RegQueryValueEx( hKey, pValueName, NULL, NULL, NULL, &bufSz); 
	if (err == ERROR_SUCCESS) {
		pBytes = new BYTE[bufSz];
		err = ::RegQueryValueEx( hKey, pValueName, NULL, NULL, pBytes, &bufSz);
		if (err != ERROR_SUCCESS) {
			delete [] pBytes;
			pBytes = NULL;
		}
	}

	return( pBytes);
}

NS_IMETHODIMP nsOSHelperAppService::ExternalProtocolHandlerExists(const char * aProtocolScheme, PRBool * aHandlerExists)
{
  // look up the protocol scheme in the windows registry....if we find a match then we have a handler for it...
  *aHandlerExists = PR_FALSE;
  if (aProtocolScheme && *aProtocolScheme)
  {
     HKEY hKey;
     LONG err = ::RegOpenKeyEx( HKEY_CLASSES_ROOT, aProtocolScheme, 0, KEY_QUERY_VALUE, &hKey);
     if (err == ERROR_SUCCESS)
     {
       *aHandlerExists = PR_TRUE;
       // close the key
       ::RegCloseKey(hKey);
     }
  }

  return NS_OK;
}

// this implementation was pretty much copied verbatime from Tony Robinson's code in nsExternalProtocolWin.cpp

NS_IMETHODIMP nsOSHelperAppService::LoadUrl(nsIURI * aURL)
{
	nsresult rv = NS_OK;

	// 1. Find the default app for this protocol
	// 2. Set up the command line
	// 3. Launch the app.

	// For now, we'll just cheat essentially, check for the command line
	// then just call ShellExecute()!

  if (aURL)
  {
    // extract the url spec from the url
    nsXPIDLCString urlSpec;
    aURL->GetSpec(getter_Copies(urlSpec));

		LONG r = (LONG) ::ShellExecute( NULL, "open", (const char *) urlSpec, NULL, NULL, SW_SHOWNORMAL);
		if (r < 32) 
			rv = NS_ERROR_FAILURE;
  }

  return rv;
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
