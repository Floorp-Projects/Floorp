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

// we need windows.h to read out registry information...
#include <windows.h>

// this is a platform specific class that abstracts an application.
// we treat this object as a cookie when we pass it to an external app handler..
// the handler will present this cookie back to the helper app service along with a
// an argument (the temp file).
class nsExternalApplication : public nsISupports
{
public:
  NS_DECL_ISUPPORTS

  nsExternalApplication();
  virtual ~nsExternalApplication();

  // the app registry name is the key we got from the registry for the
  // application. We should be able to just call ::ShellExecute on this name
  // in order to launch the application.
  void SetAppRegistryName(const char * aAppRegistryName);

  // used to launch the application passing in the location of the temp file
  // to be associated with this app.
  nsresult LaunchApplication(nsIFile * aTempFile);

protected:
  nsCString mAppRegistryName;
};


NS_IMPL_THREADSAFE_ADDREF(nsExternalApplication)
NS_IMPL_THREADSAFE_RELEASE(nsExternalApplication)

NS_INTERFACE_MAP_BEGIN(nsExternalApplication)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISupports)
NS_INTERFACE_MAP_END_THREADSAFE

nsExternalApplication::nsExternalApplication()
{
  NS_INIT_ISUPPORTS();
}

nsExternalApplication::~nsExternalApplication()
{}

void nsExternalApplication::SetAppRegistryName(const char * aAppRegistryName)
{
  mAppRegistryName = aAppRegistryName;
}


nsresult nsExternalApplication::LaunchApplication(nsIFile * aTempFile)
{
  nsresult rv = NS_OK;

  if (!mAppRegistryName.IsEmpty() && aTempFile)
  {
    nsXPIDLCString path;
    aTempFile->GetPath(getter_Copies(path));
    
    // use the app registry name to launch a shell execute....
    LONG r = (LONG) ::ShellExecute( NULL, "open", (const char *) path, NULL, NULL, SW_SHOWNORMAL);
    if (r < 32) 
    {
			rv = NS_ERROR_FAILURE;
		}
		else
			rv = NS_OK;
  }

  return rv;
}

// helper methods: forward declarations...
BYTE * GetValueBytes( HKEY hKey, const char *pValueName);
nsresult GetExtensionFrom4xRegistryInfo(const char * aMimeType, nsCString& aFileExtension);

nsOSHelperAppService::nsOSHelperAppService() : nsExternalHelperAppService()
{
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
  return NS_OK;
}

NS_IMETHODIMP nsOSHelperAppService::DoContent(const char *aMimeContentType, nsIURI *aURI, nsISupports *aWindowContext, 
                                                    PRBool *aAbortProcess, nsIStreamListener ** aStreamListener)
{
  // look up the content type and get a platform specific handle to the app we want to use for this 
  // download...create a nsExternalAppHandler, bind the application token to it (as a nsIFile??) and return this
  // as the stream listener to use...

  // eventually when we start trying to hook up some UI we may need to insert code here to throw up a dialog
  // and ask the user if they wish to use this app to open this content type...

  // now bind the handler to the application we want to launch when we the handler is done
  // receiving all the data...

  // ACK!!! we've done all this work to discover the content type just to find out that windows
  // registery uses the extension to figure out the right helper app....that's a bummer...

  nsCAutoString fileExtension;
  // this is just a hack for now...look up the content type and find a file
  // extension using information 4.x put in the windows registry...
  nsresult rv = GetExtensionFrom4xRegistryInfo(aMimeContentType, fileExtension);
  if (FAILED(rv))
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

  if (!fileExtension.IsEmpty())
  {
     nsCAutoString appName;
     HKEY hKey;
     LONG err = ::RegOpenKeyEx( HKEY_CLASSES_ROOT, fileExtension, 0, KEY_QUERY_VALUE, &hKey);
     if (err == ERROR_SUCCESS)
     {
        LPBYTE pBytes = GetValueBytes( hKey, NULL);
        appName = (char *) pBytes;
        delete [] pBytes;

        // create an application that represents this app name...
        nsExternalApplication * application = nsnull;
        NS_NEWXPCOM(application, nsExternalApplication);

        if (application)
          application->SetAppRegistryName(appName);

        nsCOMPtr<nsISupports> appSupports = do_QueryInterface(application);

        // this code is incomplete and just here to get things started..
        nsExternalAppHandler * handler = CreateNewExternalHandler(appSupports, fileExtension);
        handler->QueryInterface(NS_GET_IID(nsIStreamListener), (void **) aStreamListener);

        // close the key
       ::RegCloseKey(hKey);

     } // if we got an entry out of the registry...
  } // if we have a file extension

  return NS_OK;
}

NS_IMETHODIMP nsOSHelperAppService::LaunchAppWithTempFile(nsIFile * aTempFile, nsISupports * aAppCookie)
{
  if (aAppCookie)
  { 
     nsExternalApplication * application = NS_STATIC_CAST(nsExternalApplication *, aAppCookie);
     return application->LaunchApplication(aTempFile);
  }
  else
    return NS_ERROR_FAILURE;
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
      aFileExtension = ".";
      aFileExtension.Append( (char *) pBytes);
      
      // this may be a comma separate list of extensions...just take the first one
      // for now...

      PRInt32 pos = aFileExtension.FindChar(',', PR_TRUE);
      if (pos > 0) // we have a comma separated list of languages...
        aFileExtension.Truncate(pos); // truncate everything after the first comma (including the comma)
   
      delete [] pBytes;
      // close the key
      ::RegCloseKey(hKey);
   }
   else
     rv = NS_ERROR_FAILURE; // not 4.x extension mapping found!

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
