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

  void SetLocalFile(nsIFile * aApplicationToUse);

  // used to launch the application passing in the location of the temp file
  // to be associated with this app.
  nsresult LaunchApplication(nsIFile * aTempFile);

protected:
  nsCString mAppRegistryName;
  nsCOMPtr<nsIFile> mApplicationToUse;
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

void nsExternalApplication::SetLocalFile(nsIFile * aApplicationToUse)
{
  mApplicationToUse = aApplicationToUse;
}

nsresult nsExternalApplication::LaunchApplication(nsIFile * aTempFile)
{
  nsresult rv = NS_OK;
  nsXPIDLCString path;
  aTempFile->GetPath(getter_Copies(path));

  // if we were given an application to use then use it....otherwise
  if (mApplicationToUse)
  {
    const char * strPath = (const char *) path;
    mApplication->Spawn(&strPath, 1);
  }
  else if (!mAppRegistryName.IsEmpty() && aTempFile)
  {
	  nsCAutoString command;
 
	// build the command to run   
	command =  (const char *)mAppRegistryName;
	command += " ";
	command += (const char *)path;

#ifdef DEBUG_seth
	printf("spawn this: %s\n",(const char *)command);
#endif
	int err = system((const char *)command);

	if (err == 0) {
		rv = NS_OK;
	}
	else {
		rv = NS_ERROR_FAILURE;
	}
  }

  return rv;
}

nsOSHelperAppService::nsOSHelperAppService() : nsExternalHelperAppService()
{
 	//nsExternalHelperAppService::Init();
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


  printf("fix this hardcoding\n");

  nsCAutoString fileExtension;
  fileExtension = ".mp3";

  // create an application that represents this app name...
  nsExternalApplication * application = nsnull;
  NS_NEWXPCOM(application, nsExternalApplication);

  if (application)
  	application->SetAppRegistryName("xmms");

  nsCOMPtr<nsISupports> appSupports = do_QueryInterface(application);

  // this code is incomplete and just here to get things started..
  nsExternalAppHandler * handler = CreateNewExternalHandler(appSupports, fileExtension);
  handler->QueryInterface(NS_GET_IID(nsIStreamListener), (void **) aStreamListener);

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

nsresult nsOSHelperAppService::CreateStreamListenerWithApp(nsIFile * aApplicationToUse, const char * aFileExtension, nsIStreamListener ** aStreamListener)
{
  nsresult rv = NS_OK;
  
  // create an application that represents this app name...
  nsExternalApplication * application = nsnull;
  NS_NEWXPCOM(application, nsExternalApplication);
  NS_IF_ADDREF(application);

  if (application)
  {
    application->SetLocalFile(aApplicationToUse);
    nsCOMPtr<nsISupports> appSupports = do_QueryInterface(application);    
    // this code is incomplete and just here to get things started..
    nsExternalAppHandler * handler = CreateNewExternalHandler(appSupports, aFileExtension);
    handler->QueryInterface(NS_GET_IID(nsIStreamListener), (void **) aStreamListener);
  }

  NS_IF_RELEASE(application);
  
  return rv;
}
