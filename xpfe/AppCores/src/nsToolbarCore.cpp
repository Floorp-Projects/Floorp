
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

#include "nsToolbarCore.h"
#include "nsIBrowserWindow.h"
#include "nsRepository.h"
#include "nsAppCores.h"
#include "nsAppCoresCIDs.h"
#include "nsAppCoresManager.h"
#include "nsCOMPtr.h"
#include "nsIWebShell.h"

#include "nsIScriptContext.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMCharacterData.h"
#include "nsIScriptGlobalObject.h"
#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"


// Globals
static NS_DEFINE_IID(kISupportsIID,              NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIToolbarCoreIID,           NS_IDOMTOOLBARCORE_IID);

static NS_DEFINE_IID(kIDOMDocumentIID,           nsIDOMDocument::IID());
static NS_DEFINE_IID(kIDocumentIID,              nsIDocument::IID());
static NS_DEFINE_IID(kIDOMCharacterDataIID,      nsIDOMCharacterData::IID());

static NS_DEFINE_IID(kToolbarCoreCID,            NS_TOOLBARCORE_CID);

static NS_DEFINE_IID(kINetSupportIID,            NS_INETSUPPORT_IID);
static NS_DEFINE_IID(kIStreamObserverIID,        NS_ISTREAMOBSERVER_IID);

static NS_DEFINE_IID(kIWebShellWindowIID,        NS_IWEBSHELL_WINDOW_IID);


/////////////////////////////////////////////////////////////////////////
// nsToolbarCore
/////////////////////////////////////////////////////////////////////////

nsToolbarCore::nsToolbarCore()
{
  printf("Created nsToolbarCore\n");

  mWindow         = nsnull;
  mStatusText     = nsnull;
  mWebShellWin    = nsnull;

  IncInstanceCount();
  NS_INIT_REFCNT();
}

nsToolbarCore::~nsToolbarCore()
{
  NS_IF_RELEASE(mWindow);
  NS_IF_RELEASE(mWebShellWin);
  NS_IF_RELEASE(mStatusText);
  DecInstanceCount();  
}


NS_IMPL_ADDREF(nsToolbarCore)
NS_IMPL_RELEASE(nsToolbarCore)


NS_IMETHODIMP 
nsToolbarCore::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if ( aIID.Equals(kIToolbarCoreIID) ) {
    *aInstancePtr = (void*) ((nsIDOMToolbarCore*)this);
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
nsToolbarCore::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) 
  {
      res = NS_NewScriptToolbarCore(aContext, 
                                (nsISupports *)(nsIDOMToolbarCore*)this, 
                                nsnull, 
                                &mScriptObject);
  }

  *aScriptObject = mScriptObject;
  return res;
}



NS_IMETHODIMP    
nsToolbarCore::Init(const nsString& aId)
{
   
  nsBaseAppCore::Init(aId);

	nsAppCoresManager* sdm = new nsAppCoresManager();
	sdm->Add((nsIDOMBaseAppCore *)(nsBaseAppCore *)this);
	delete sdm;

	return NS_OK;
}




NS_IMETHODIMP    
nsToolbarCore::SetStatus(const nsString& aMsg)
{
/*  if (nsnull == mStatusText) {
    nsIDOMDocument * domDoc;
    mWindow->GetDocument(&domDoc);
    if (!domDoc)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMNode> parent(GetParentNodeFromDOMDoc(domDoc));
    if (!parent)
      return NS_ERROR_FAILURE;

    PRInt32 count = 0;
    nsCOMPtr<nsIDOMNode> statusNode(FindNamedDOMNode(nsAutoString("#text"), parent, count, 7));
    if (!statusNode)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMCharacterData> charData(statusNode);
    if (!charData)
      return NS_ERROR_FAILURE;

    mStatusText = charData;
    mStatusText->SetData(nsAutoString("Ready.....")); // <<====== EVIL HARD-CODED STRING.
    NS_RELEASE(domDoc);
  }

  mStatusText->SetData(aMsg); */
  DoDialog();

  return NS_OK;
}

NS_IMETHODIMP    
nsToolbarCore::SetWebShellWindow(nsIDOMWindow* aWin)
{
  mWindow = aWin;
  NS_ADDREF(aWin);

  nsCOMPtr<nsIScriptGlobalObject> globalObj( mWindow );
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

    printf("attaching to [%s]\n", str.ToNewCString());

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
nsToolbarCore::SetWindow(nsIDOMWindow* aWin)
{
  mWindow = aWin;
  NS_ADDREF(aWin);


  nsCOMPtr<nsIScriptGlobalObject> globalObj( mWindow );
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

    printf("attaching to [%s]\n", str.ToNewCString());
    NS_RELEASE(webShell);
  }


/*      nsCOMPtr<nsIDOMNode> oldNode( node );
      oldNode->GetNextSibling(getter_AddRefs(node));
  nsCOMPtr<nsIDOMDocument> domDoc; // result == nsnull;

  // first get the toolbar child WebShell
  nsCOMPtr<nsIWebShell> childWebShell;
  mWebShell->FindChildWithName(aWebShellName, *getter_AddRefs(childWebShell));
  if (!childWebShell)
    return domDoc;
  
  nsCOMPtr<nsIContentViewer> cv;
  childWebShell->GetContentViewer(getter_AddRefs(cv));
  if (!cv)
    return domDoc;
   
  nsCOMPtr<nsIDocumentViewer> docv(cv);
  if (!docv)
    return domDoc;

  nsCOMPtr<nsIDocument> doc;
  docv->GetDocument(*getter_AddRefs(doc));
  if (doc)
    return nsCOMPtr<nsIDOMDocument>(doc);

  return domDoc;*/
	return NS_OK;
}

#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIWidget.h"
#include "plevent.h"

#include "nsIAppShell.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"

/* Define Class IDs */
static NS_DEFINE_IID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);

/* Define Interface IDs */
static NS_DEFINE_IID(kIAppShellServiceIID, NS_IAPPSHELL_SERVICE_IID);

NS_IMETHODIMP    
nsToolbarCore::DoDialog()
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

  NS_RELEASE(url);
  NS_RELEASE(parent);
  
   /*
    * Start up the main event loop...
    */
  //rv = appShell->Run();
  rv = NS_OK;
  while (rv == NS_OK) {
    void * data;
    PRBool inWin;
    PRBool isMouseEvent;
    rv = appShell->GetNativeEvent(data, newWindow, inWin, isMouseEvent);
    if (rv == NS_OK) {
      printf("In win %d   is mouse %d\n", inWin, isMouseEvent);
    } else {
      rv = NS_OK;
    }
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
nsToolbarCore::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
  nsresult rv = NS_OK;
printf("OnStartBinding\n");
  return rv;
}


NS_IMETHODIMP
nsToolbarCore::OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax)
{
  nsresult rv = NS_OK;
printf("OnStartBinding\n");
  return rv;
}


NS_IMETHODIMP
nsToolbarCore::OnStatus(nsIURL* aURL, const PRUnichar* aMsg)
{
  nsresult rv = NS_OK;
printf("OnStartBinding\n");
  return rv;
}


NS_IMETHODIMP
nsToolbarCore::OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg)
{
  nsresult rv = NS_OK;
printf("OnStartBinding\n");
  return rv;
}

//----------------------------------------------------------------------

NS_IMETHODIMP_(void)
nsToolbarCore::Alert(const nsString &aText)
{
printf("Alert\n");
}

NS_IMETHODIMP_(PRBool)
nsToolbarCore::Confirm(const nsString &aText)
{
  PRBool bResult = PR_FALSE;
printf("Confirm\n");
  return bResult;
}

NS_IMETHODIMP_(PRBool)
nsToolbarCore::Prompt(const nsString &aText,
                   const nsString &aDefault,
                   nsString &aResult)
{
  PRBool bResult = PR_FALSE;
printf("Prompt\n");
  return bResult;
}

NS_IMETHODIMP_(PRBool) 
nsToolbarCore::PromptUserAndPassword(const nsString &aText,
                                  nsString &aUser,
                                  nsString &aPassword)
{
  PRBool bResult = PR_FALSE;
printf("PromptUserAndPassword\n");
DoDialog();
  return bResult;
}

NS_IMETHODIMP_(PRBool) 
nsToolbarCore::PromptPassword(const nsString &aText,
                           nsString &aPassword)
{
  PRBool bResult = PR_FALSE;
printf("PromptPassword\n");
  return bResult;
}

