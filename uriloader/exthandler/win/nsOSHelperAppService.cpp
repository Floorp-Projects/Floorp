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
#include "nsMimeTypes.h"
#include "nsILocalFile.h"

// we need windows.h to read out registry information...
#include <windows.h>

// helper methods: forward declarations...
BYTE * GetValueBytes( HKEY hKey, const char *pValueName, DWORD *pLen=0);
nsresult GetExtensionFrom4xRegistryInfo(const char * aMimeType, nsCString& aFileExtension);
nsresult GetExtensionFromWindowsMimeDatabase(const char * aMimeType, nsCString& aFileExtension);

nsOSHelperAppService::nsOSHelperAppService() : nsExternalHelperAppService()
{}

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

// The windows registry provides a mime database key which lists a set of mime types and corresponding "Extension" values. 
// we can use this to look up our mime type to see if there is a preferred extension for the mime type.
nsresult GetExtensionFromWindowsMimeDatabase(const char * aMimeType, nsCString& aFileExtension)
{
   nsresult rv = NS_OK;
   HKEY hKey;
   nsCAutoString mimeDatabaseKey ("MIME\\Database\\Content Type\\");

   mimeDatabaseKey += aMimeType;
   
   LONG err = ::RegOpenKeyEx( HKEY_CLASSES_ROOT, mimeDatabaseKey.get(), 0, KEY_QUERY_VALUE, &hKey);
   if (err == ERROR_SUCCESS)
   {
      LPBYTE pBytes = GetValueBytes( hKey, "Extension");
      if (pBytes)
      { 
        aFileExtension = (char * )pBytes;
      }

      delete pBytes;

      ::RegCloseKey(hKey);
   }

   return NS_OK;

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
   LONG err = ::RegOpenKeyEx( HKEY_CURRENT_USER, command.get(), 0, KEY_QUERY_VALUE, &hKey);
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

static BYTE * GetValueBytes( HKEY hKey, const char *pValueName, DWORD *pLen)
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
        } else {
            // Return length if caller wanted it.
            if ( pLen ) {
                *pLen = bufSz;
            }
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

// GetMIMEInfoFromRegistry: This function obtains the values of some of the nsIMIMEInfo
// attributes for the mimeType/extension associated with the input registry key.  The default
// entry for that key is the name of a registry key under HKEY_CLASSES_ROOT.  The default
// value for *that* key is the descriptive name of the type.  The EditFlags value is a binary
// value; the low order bit of the third byte of which indicates that the user does not need
// to be prompted.
//
// This function sets only the Description attribute of the input nsIMIMEInfo.
static nsresult GetMIMEInfoFromRegistry( LPBYTE fileType, nsIMIMEInfo *pInfo )
{
    nsresult rv = NS_OK;

    NS_ENSURE_ARG( fileType );
    NS_ENSURE_ARG( pInfo );

    // Get registry key for the pointed-to "file type."
    HKEY fileTypeKey = 0;
    LONG rc = ::RegOpenKeyEx( HKEY_CLASSES_ROOT, (const char *)fileType, 0, KEY_QUERY_VALUE, &fileTypeKey );
    if ( rc == ERROR_SUCCESS )
    {
        // OK, the default value here is the description of the type.
        DWORD lenDesc = 0;
        LPBYTE pDesc = GetValueBytes( fileTypeKey, "", &lenDesc );
        if ( pDesc )
        {
            PRUnichar *pUniDesc = new PRUnichar[lenDesc+1];
            if (pUniDesc) {
              int len = MultiByteToWideChar(CP_ACP, 0, (const char *)pDesc, -1, pUniDesc, lenDesc); 
              pUniDesc[len] = 0;
              pInfo->SetDescription( pUniDesc );
              delete [] pUniDesc;
            }
            delete [] pDesc;
        }
    }
    else
    {
        rv = NS_ERROR_FAILURE;
    }

    return rv;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
// nsIMIMEService method over-rides used to gather information from the windows registry for
// various mime types. 
////////////////////////////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsOSHelperAppService::GetFromExtension(const char *aFileExt, nsIMIMEInfo **_retval) 
{
  // first, see if the base class already has an entry....
  nsresult rv = nsExternalHelperAppService::GetFromExtension(aFileExt, _retval);
  if (NS_SUCCEEDED(rv) && *_retval) return NS_OK; // okay we got an entry so we are done.
  if (!aFileExt || *aFileExt == '\0') return NS_ERROR_FAILURE;

  rv = NS_OK;

  // windows registry assumes your file extension is going to include the '.'.
  // so make sure it's there...
  nsCAutoString fileExtToUse;
  if (aFileExt && aFileExt[0] != '.')
    fileExtToUse = '.';

  fileExtToUse.Append(aFileExt);

  // o.t. try to get an entry from the windows registry.
   HKEY hKey;
   LONG err = ::RegOpenKeyEx( HKEY_CLASSES_ROOT, fileExtToUse.get(), 0, KEY_QUERY_VALUE, &hKey);
   if (err == ERROR_SUCCESS)
   {
      LPBYTE pBytes = GetValueBytes( hKey, "Content Type");
      LPBYTE pFileDescription = GetValueBytes(hKey, "");

      nsCOMPtr<nsIMIMEInfo> mimeInfo (do_CreateInstance(NS_MIMEINFO_CONTRACTID));
      if (mimeInfo && pBytes)
      {
        mimeInfo->SetMIMEType((char *) pBytes);
        // if the file extension includes the '.' then we don't want to include that when we append
        // it to the mime info object.
        if (aFileExt && *aFileExt == '.' && (aFileExt+1))
          mimeInfo->AppendExtension(aFileExt+1);
        else
          mimeInfo->AppendExtension(aFileExt);

        mimeInfo->SetPreferredAction(nsIMIMEInfo::useSystemDefault);

        nsAutoString description;
        description.AssignWithConversion((char *) pFileDescription);

        PRInt32 pos = description.FindChar('.', PR_TRUE);
        if (pos > 0) 
          description.Truncate(pos); 
        // the format of the description usually looks like appname.version.something.
        // for now, let's try to make it pretty and just show you the appname.

        mimeInfo->SetApplicationDescription(description.get());

        // Get other nsIMIMEInfo fields from registry, if possible.
        if ( pFileDescription )
        {
            GetMIMEInfoFromRegistry( pFileDescription, mimeInfo );
        }

        // if we got here, then set our return variable
        *_retval = mimeInfo;
        NS_ADDREF(*_retval);
      }
      else
        rv = NS_ERROR_FAILURE; // we failed to really find an entry in the registry

      delete [] pBytes;
      delete [] pFileDescription;

      // close the key
     ::RegCloseKey(hKey);
   }

   // we failed to find a mime type.
   if (!*_retval) rv = NS_ERROR_FAILURE;
    
   return rv;
}

NS_IMETHODIMP nsOSHelperAppService::GetFromMIMEType(const char *aMIMEType, nsIMIMEInfo ** _retval) 
{
  // first, see if the base class already has an entry....
  nsresult rv = nsExternalHelperAppService::GetFromMIMEType(aMIMEType, _retval);
  if (NS_SUCCEEDED(rv) && *_retval) return NS_OK; // okay we got an entry so we are done.

  // (1) try to use the windows mime database to see if there is a mapping to a file extension
  // (2) try to see if we have some left over 4.x registry info we can peek at...
  nsCAutoString fileExtension;
  GetExtensionFromWindowsMimeDatabase(aMIMEType, fileExtension);
  if (fileExtension.IsEmpty())
    GetExtensionFrom4xRegistryInfo(aMIMEType, fileExtension);

  // now look up based on the file extension.
  if (!fileExtension.IsEmpty())
    return GetFromExtension(fileExtension.get(), _retval);
  else
   rv = NS_ERROR_FAILURE;

  return rv;
}
