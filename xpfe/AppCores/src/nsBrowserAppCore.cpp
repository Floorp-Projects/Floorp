
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsBrowserAppCore.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"
#include "pratom.h"
#include "nsRepository.h"
#include "nsAppCores.h"
#include "nsAppCoresCIDs.h"
#include "nsAppCoresManager.h"

#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"

#include "nsIScriptGlobalObject.h"
#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"
#include "nsCOMPtr.h"

#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIWidget.h"
#include "plevent.h"

#include "nsIAppShell.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"

/* Define Class IDs */
static NS_DEFINE_IID(kAppShellServiceCID,        NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kBrowserAppCoreCID,         NS_BROWSERAPPCORE_CID);

/* Define Interface IDs */
static NS_DEFINE_IID(kIAppShellServiceIID,       NS_IAPPSHELL_SERVICE_IID);

static NS_DEFINE_IID(kISupportsIID,              NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIBrowserAppCoreIID,        NS_IDOMBROWSERAPPCORE_IID);

static NS_DEFINE_IID(kIDOMDocumentIID,           nsIDOMDocument::IID());
static NS_DEFINE_IID(kIDocumentIID,              nsIDocument::IID());


static NS_DEFINE_IID(kINetSupportIID,            NS_INETSUPPORT_IID);
static NS_DEFINE_IID(kIStreamObserverIID,        NS_ISTREAMOBSERVER_IID);

static NS_DEFINE_IID(kIWebShellWindowIID,        NS_IWEBSHELL_WINDOW_IID);

#define APP_DEBUG 0 

/////////////////////////////////////////////////////////////////////////
// nsBrowserAppCore
/////////////////////////////////////////////////////////////////////////

nsBrowserAppCore::nsBrowserAppCore()
{
  if (APP_DEBUG) printf("Created nsBrowserAppCore\n");

  mScriptObject         = nsnull;
  mToolbarWindow        = nsnull;
  mToolbarScriptContext = nsnull;
  mContentWindow        = nsnull;
  mContentScriptContext = nsnull;
  mWebShellWin          = nsnull;

  IncInstanceCount();
  NS_INIT_REFCNT();
}

nsBrowserAppCore::~nsBrowserAppCore()
{
  NS_IF_RELEASE(mToolbarWindow);
  NS_IF_RELEASE(mToolbarScriptContext);
  NS_IF_RELEASE(mContentWindow);
  NS_IF_RELEASE(mContentScriptContext);
  NS_IF_RELEASE(mWebShellWin);
  DecInstanceCount();  
}


NS_IMPL_ADDREF(nsBrowserAppCore)
NS_IMPL_RELEASE(nsBrowserAppCore)


NS_IMETHODIMP 
nsBrowserAppCore::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if ( aIID.Equals(kIBrowserAppCoreIID) ) {
    *aInstancePtr = (void*) ((nsIDOMBrowserAppCore*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kINetSupportIID)) {
    *aInstancePtr = (void*) ((nsINetSupport*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIStreamObserverIID)) {
    *aInstancePtr = (void*) ((nsIStreamObserver*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
 

  return nsBaseAppCore::QueryInterface(aIID, aInstancePtr);
}


NS_IMETHODIMP 
nsBrowserAppCore::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) 
  {
      res = NS_NewScriptBrowserAppCore(aContext, 
                                (nsISupports *)(nsIDOMBrowserAppCore*)this, 
                                nsnull, 
                                &mScriptObject);
  }

  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP    
nsBrowserAppCore::Init(const nsString& aId)
{
   
  nsBaseAppCore::Init(aId);

	nsAppCoresManager* sdm = new nsAppCoresManager();
	sdm->Add((nsIDOMBaseAppCore *)(nsBaseAppCore *)this);
	delete sdm;

	return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::Back()
{
  ExecuteScript(mToolbarScriptContext, mDisableScript);
  ExecuteScript(mContentScriptContext, "window.back();");
  ExecuteScript(mToolbarScriptContext, mEnableScript);
	return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::Forward()
{
  ExecuteScript(mToolbarScriptContext, mDisableScript);
  ExecuteScript(mContentScriptContext, "window.forward();");
  ExecuteScript(mToolbarScriptContext, mEnableScript);
	return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::SetDisableCallback(const nsString& aScript)
{
  mDisableScript = aScript;
	return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::SetEnableCallback(const nsString& aScript)
{
  mEnableScript = aScript;
	return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::LoadUrl(const nsString& aUrl)
{
  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::SetToolbarWindow(nsIDOMWindow* aWin)
{
  mToolbarWindow = aWin;
  NS_ADDREF(aWin);
  mToolbarScriptContext = GetScriptContext(aWin);

	return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::SetContentWindow(nsIDOMWindow* aWin)
{
  mContentWindow = aWin;
  NS_ADDREF(aWin);
  mContentScriptContext = GetScriptContext(aWin);
  nsCOMPtr<nsIScriptGlobalObject> globalObj( mContentWindow );
  if (!globalObj) {
    return NS_ERROR_FAILURE;
  }

  nsIWebShell * webShell;
  globalObj->GetWebShell(&webShell);
  if (nsnull != webShell) {
    webShell->SetObserver(this);
    PRUnichar * name;
    webShell->GetName( &name);
    nsAutoString str(name);

    if (APP_DEBUG) printf("Attaching to Content WebShell [%s]\n", str.ToNewCString()); // this leaks
    NS_RELEASE(webShell);
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::SetWebShellWindow(nsIDOMWindow* aWin)
{
  if (!mContentWindow) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIScriptGlobalObject> globalObj( aWin );
  if (!globalObj) {
    return NS_ERROR_FAILURE;
  }

  nsIWebShell * webShell;
  globalObj->GetWebShell(&webShell);
  if (nsnull != webShell) {
    PRUnichar * name;
    webShell->GetName( &name);
    nsAutoString str(name);

    if (APP_DEBUG) printf("Attaching to WebShellWindow[%s]\n", str.ToNewCString());

    nsIWebShellContainer * webShellContainer;
    webShell->GetContainer(webShellContainer);
    if (nsnull != webShellContainer) {
      if (NS_OK == webShellContainer->QueryInterface(kIWebShellWindowIID, (void**) &mWebShellWin)) {
      }
      NS_RELEASE(webShellContainer);
    }
    NS_RELEASE(webShell);
  }
  return NS_OK;
}


NS_IMETHODIMP    
nsBrowserAppCore::ExecuteScript(nsIScriptContext * aContext, const nsString& aScript)
{
  if (nsnull != aContext) {
    const char* url = "";
    PRBool isUndefined = PR_FALSE;
    nsString rVal;
    if (APP_DEBUG) printf("Executing [%s]\n", aScript.ToNewCString());
    aContext->EvaluateString(aScript, url, 0, rVal, &isUndefined);
  } 
  return NS_OK;
}




NS_IMETHODIMP    
nsBrowserAppCore::DoDialog()
{
  nsresult rv;
  nsString controllerCID;

  char *  urlstr=nsnull;
  char *   progname = nsnull;
  char *   width=nsnull, *height=nsnull;
  char *  iconic_state=nsnull;

  nsIAppShellService* appShell = nsnull;

  urlstr = "resource:/res/samples/Password.html";

  /*
   * Create the Application Shell instance...
   */
  rv = nsServiceManager::GetService(kAppShellServiceCID,
                                    kIAppShellServiceIID,
                                    (nsISupports**)&appShell);
  if (!NS_SUCCEEDED(rv)) {
    goto done;
  }

  /*
   * Initialize the Shell...
   */
  rv = appShell->Initialize();
  if (!NS_SUCCEEDED(rv)) {
    goto done;
  }
 
  /*
   * Post an event to the shell instance to load the AppShell 
   * initialization routines...  
   * 
   * This allows the application to enter its event loop before having to 
   * deal with GUI initialization...
   */
  ///write me...
  nsIURL* url;
  nsIWidget* newWindow;
  
  rv = NS_NewURL(&url, urlstr);
  if (NS_FAILED(rv)) {
    goto done;
  }

  nsIWidget * parent;
  mWebShellWin->GetWidget(parent);

  /*
   * XXX: Currently, the CID for the "controller" is passed in as an argument 
   *      to CreateTopLevelWindow(...).  Once XUL supports "controller" 
   *      components this will be specified in the XUL description...
   */
  controllerCID = "43147b80-8a39-11d2-9938-0080c7cb1081";
  appShell->CreateDialogWindow(parent, url, controllerCID, newWindow, (nsIStreamObserver *)this);
  newWindow->Resize(300, 200, PR_TRUE);
  NS_RELEASE(url);
  NS_RELEASE(parent);
  
   /*
    * Start up the main event loop...
    */
  rv = NS_OK;
  while (rv == NS_OK) {
    void * data;
    PRBool inWin;
    PRBool isMouseEvent;
    rv = appShell->GetNativeEvent(data, newWindow, inWin, isMouseEvent);
    //if (rv == NS_OK) {
    //  if (APP_DEBUG) printf("In win %d   is mouse %d\n", inWin, isMouseEvent);
    //} 
    if (rv == NS_OK && (inWin || (!inWin && !isMouseEvent))) {
      appShell->DispatchNativeEvent(data);
    }
  }

  /*
   * Shut down the Shell instance...  This is done even if the Run(...)
   * method returned an error.
   */
  (void) appShell->Shutdown();

done:

  /* Release the shell... */
  if (nsnull != appShell) {
    nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);
  }
  return NS_OK;
}


NS_IMETHODIMP
nsBrowserAppCore::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
  nsresult rv = NS_OK;
if (APP_DEBUG) printf("OnStartBinding\n");
  return rv;
}


NS_IMETHODIMP
nsBrowserAppCore::OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax)
{
  nsresult rv = NS_OK;
  if (APP_DEBUG) printf("OnProgress\n");
  return rv;
}


NS_IMETHODIMP
nsBrowserAppCore::OnStatus(nsIURL* aURL, const PRUnichar* aMsg)
{
  nsresult rv = NS_OK;
  if (APP_DEBUG) printf("OnStatus\n");
  return rv;
}


NS_IMETHODIMP
nsBrowserAppCore::OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg)
{
  nsresult rv = NS_OK;
  if (APP_DEBUG) printf("OnStopBinding\n");
  return rv;
}

//----------------------------------------------------------------------

NS_IMETHODIMP_(void)
nsBrowserAppCore::Alert(const nsString &aText)
{
  if (APP_DEBUG) printf("Alert\n");
}

NS_IMETHODIMP_(PRBool)
nsBrowserAppCore::Confirm(const nsString &aText)
{
  PRBool bResult = PR_FALSE;
  if (APP_DEBUG) printf("Confirm\n");
  return bResult;
}

NS_IMETHODIMP_(PRBool)
nsBrowserAppCore::Prompt(const nsString &aText,
                   const nsString &aDefault,
                   nsString &aResult)
{
  PRBool bResult = PR_FALSE;
  if (APP_DEBUG) printf("Prompt\n");
  return bResult;
}

NS_IMETHODIMP_(PRBool) 
nsBrowserAppCore::PromptUserAndPassword(const nsString &aText,
                                  nsString &aUser,
                                  nsString &aPassword)
{
  PRBool bResult = PR_FALSE;
  if (APP_DEBUG) printf("PromptUserAndPassword\n");
  DoDialog();
  return bResult;
}

NS_IMETHODIMP_(PRBool) 
nsBrowserAppCore::PromptPassword(const nsString &aText,
                           nsString &aPassword)
{
  PRBool bResult = PR_FALSE;
  if (APP_DEBUG) printf("PromptPassword\n");
  return bResult;
}


