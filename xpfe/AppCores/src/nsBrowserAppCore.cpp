
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
#include "prprf.h"
#include "nsIComponentManager.h"
#include "nsAppCores.h"
#include "nsAppCoresCIDs.h"
#include "nsAppCoresManager.h"

#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXULDocument.h"
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

#include "nsIDocumentViewer.h"
#include "nsIDOMHTMLImageElement.h"

#ifdef ClientWallet
#include "nsIWalletService.h"
static NS_DEFINE_IID(kIWalletServiceIID, NS_IWALLETSERVICE_IID);
static NS_DEFINE_IID(kWalletServiceCID, NS_WALLETSERVICE_CID);
#endif



#include "nsICmdLineService.h"


/* Define Class IDs */
static NS_DEFINE_IID(kAppShellServiceCID,        NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kBrowserAppCoreCID,         NS_BROWSERAPPCORE_CID);
static NS_DEFINE_IID(kCmdLineServiceCID,    NS_COMMANDLINE_SERVICE_CID);
/* Define Interface IDs */
static NS_DEFINE_IID(kICmdLineServiceIID,   NS_ICOMMANDLINE_SERVICE_IID);
static NS_DEFINE_IID(kIAppShellServiceIID,       NS_IAPPSHELL_SERVICE_IID);
static NS_DEFINE_IID(kISupportsIID,              NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIBrowserAppCoreIID,        NS_IDOMBROWSERAPPCORE_IID);
static NS_DEFINE_IID(kIDOMDocumentIID,           nsIDOMDocument::GetIID());
static NS_DEFINE_IID(kIDocumentIID,              nsIDocument::GetIID());
static NS_DEFINE_IID(kINetSupportIID,            NS_INETSUPPORT_IID);
static NS_DEFINE_IID(kIStreamObserverIID,        NS_ISTREAMOBSERVER_IID);
static NS_DEFINE_IID(kIWebShellWindowIID,        NS_IWEBSHELL_WINDOW_IID);
static NS_DEFINE_IID(kIURLListenerIID,           NS_IURL_LISTENER_IID);

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
  mWebShell             = nsnull;
  mContentAreaWebShell  = nsnull;

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
  NS_IF_RELEASE(mWebShell);
  NS_IF_RELEASE(mContentAreaWebShell);
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
  if (aIID.Equals(kIURLListenerIID)) {
    *aInstancePtr = (void*) ((nsIURLListener*)this);
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

  // XXX This is lame and needs to be changed
	nsAppCoresManager* sdm = new nsAppCoresManager();
	sdm->Add((nsIDOMBaseAppCore *)(nsBaseAppCore *)this);
	delete sdm;

	return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::SetDocumentCharset(const nsString& aCharset)
{
  nsresult res = NS_OK;
  if (nsnull != mContentWindow) {
    nsCOMPtr<nsIDOMDocument> domDoc;
    if (NS_SUCCEEDED(res = mContentWindow->GetDocument(getter_AddRefs(domDoc)))) {
      nsCOMPtr<nsIDocument> doc(do_QueryInterface(domDoc, &res));
      if (NS_SUCCEEDED(res)) {
        nsString *aNewCharset = new nsString(aCharset);
        if (nsnull != aNewCharset) {
          doc->SetDocumentCharacterSet(aNewCharset);
        }
      }
    }
  }
  return res;
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

#ifdef ClientWallet
//#define WALLET_EDITOR_URL "resource:/res/samples/walleted.html"
//#define WALLET_EDITOR_URL "http://peoplestage/morse/wallet/walleted.html"
#define WALLET_EDITOR_URL "http://people.netscape.com/morse/wallet/walleted.html"

#define WALLET_SAMPLES_URL "http://people.netscape.com/morse/wallet/samples/"
//#define WALLET_SAMPLES_URL "http://peoplestage/morse/wallet/samples/"

PRInt32
newWindow(char* urlName) {
  nsresult rv;
  nsString controllerCID;

  char *  urlstr=nsnull;
  char *   progname = nsnull;
  char *   width=nsnull, *height=nsnull;
  char *  iconic_state=nsnull;

  nsIAppShellService* appShell = nsnull;
  urlstr = urlName;

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

  /*
   * XXX: Currently, the CID for the "controller" is passed in as an argument 
   *      to CreateTopLevelWindow(...).  Once XUL supports "controller" 
   *      components this will be specified in the XUL description...
   */
  controllerCID = "43147b80-8a39-11d2-9938-0080c7cb1081";
  appShell->CreateTopLevelWindow(nsnull, url, controllerCID, newWindow,
              nsnull, nsnull, 615, 650);

  NS_RELEASE(url);
  
done:
  /* Release the shell... */
  if (nsnull != appShell) {
    nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);
  }

  return NS_OK;
}

static NS_DEFINE_IID(kIDocumentViewerIID, NS_IDOCUMENT_VIEWER_IID);
#include "nsIPresContext.h"
static
nsIPresShell*
GetPresShellFor(nsIWebShell* aWebShell)
{
  nsIPresShell* shell = nsnull;
  if (nsnull != aWebShell) {
    nsIContentViewer* cv = nsnull;
    aWebShell->GetContentViewer(&cv);
    if (nsnull != cv) {
      nsIDocumentViewer* docv = nsnull;
      cv->QueryInterface(kIDocumentViewerIID, (void**) &docv);
      if (nsnull != docv) {
        nsIPresContext* cx;
        docv->GetPresContext(cx);
	      if (nsnull != cx) {
	        cx->GetShell(&shell);
	        NS_RELEASE(cx);
	      }
        NS_RELEASE(docv);
      }
      NS_RELEASE(cv);
    }
  }
  return shell;
}


NS_IMETHODIMP    
nsBrowserAppCore::WalletEditor()
{
  /* set a cookie for the wallet editor */
  nsIWalletService *walletservice;
  nsresult res;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    nsIURL * url;
    if (!NS_FAILED(NS_NewURL(&url, WALLET_EDITOR_URL))) {
      res = walletservice->WALLET_PreEdit(url);
      NS_RELEASE(walletservice);
    }
  }

  /* bring up the wallet editor in a new window */
  return newWindow(WALLET_EDITOR_URL);
}

NS_IMETHODIMP    
nsBrowserAppCore::WalletSafeFillin()
{
  nsIPresShell* shell;
  shell = nsnull;
  nsCOMPtr<nsIWebShell> webcontent; 
  mWebShell->FindChildWithName(nsAutoString("content"), *getter_AddRefs(webcontent));
  shell = GetPresShellFor(webcontent);

  nsIWalletService *walletservice;
  nsresult res;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->WALLET_Prefill(shell, PR_FALSE);
    NS_RELEASE(walletservice);
  }

#ifndef HTMLDialogs 
  return newWindow("file:///y|/htmldlgs.htm");
#endif

  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::WalletQuickFillin()
{
  nsIPresShell* shell;
  shell = nsnull;
  nsCOMPtr<nsIWebShell> webcontent; 
  mWebShell->FindChildWithName(nsAutoString("content"), *getter_AddRefs(webcontent));
  shell = GetPresShellFor(webcontent);

  nsIWalletService *walletservice;
  nsresult res;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->WALLET_Prefill(shell, PR_TRUE);
    NS_RELEASE(walletservice);
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::WalletSamples()
{
  /* bring up the samples in a new window */
  mContentAreaWebShell->LoadURL(nsString(WALLET_SAMPLES_URL), nsnull, nsnull);
  return NS_OK;
}

#endif

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
  char * urlstr = nsnull;
  urlstr = aUrl.ToNewCString();

  if (!urlstr)
    return NS_OK;

  printf("URL to load in nsBrowserAppCore is %s\n", urlstr);

  /* Ask nsWebShell to load the URl */
  mContentAreaWebShell->LoadURL(nsString(urlstr), nsnull, nsnull);

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
  nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(mContentWindow) );
  if (!globalObj) {
    return NS_ERROR_FAILURE;
  }

  nsIWebShell * webShell;
  globalObj->GetWebShell(&webShell);
  if (nsnull != webShell) {
    mContentAreaWebShell = webShell;
    NS_ADDREF(webShell);
    webShell->SetObserver(this);
    const PRUnichar * name;
    webShell->GetName( &name);
    nsAutoString str(name);

    if (APP_DEBUG) {
        char *name = str.ToNewCString();
        printf("Attaching to Content WebShell [%s]\n", name);
        delete [] name;
    }
    mContentAreaWebShell->SetURLListener(this);
    // Break link to chrome.
    mContentAreaWebShell->SetParent(0);
  }

  return NS_OK;

}



NS_IMETHODIMP    
nsBrowserAppCore::SetWebShellWindow(nsIDOMWindow* aWin)
{
  if (!mContentWindow) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(aWin) );
  if (!globalObj) {
    return NS_ERROR_FAILURE;
  }

  nsIWebShell * webShell;
  globalObj->GetWebShell(&webShell);
  if (nsnull != webShell) {
    mWebShell = webShell;
    NS_ADDREF(mWebShell);
    const PRUnichar * name;
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

// nsIURLListener methods

NS_IMETHODIMP 
nsBrowserAppCore::WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL,
                              nsLoadType aReason)
{

  // Notify the AppCore
    return NS_OK;
}

static nsresult setAttribute( nsIWebShell *shell,
                              const char *id,
                              const char *name,
                              const nsString &value ) {
    nsresult rv = NS_OK;
  
    nsCOMPtr<nsIContentViewer> cv;
    rv = shell->GetContentViewer(getter_AddRefs(cv));
    if ( cv ) {
        // Up-cast.
        nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
        if ( docv ) {
            // Get the document from the doc viewer.
            nsCOMPtr<nsIDocument> doc;
            rv = docv->GetDocument(*getter_AddRefs(doc));
            if ( doc ) {
                // Up-cast.
                nsCOMPtr<nsIDOMXULDocument> xulDoc( do_QueryInterface(doc) );
                if ( xulDoc ) {
                    // Find specified element.
                    nsCOMPtr<nsIDOMElement> elem;
                    rv = xulDoc->GetElementById( id, getter_AddRefs(elem) );
                    if ( elem ) {
                        // Set the text attribute.
                        rv = elem->SetAttribute( name, value );
                        if ( APP_DEBUG ) {
                            char *p = value.ToNewCString();
                            delete [] p;
                        }
                        if ( rv != NS_OK ) {
                            if (APP_DEBUG) printf("SetAttribute failed, rv=0x%X\n",(int)rv);
                        }
                    } else {
                        if (APP_DEBUG) printf("GetElementByID failed, rv=0x%X\n",(int)rv);
                    }
                } else {
                    if (APP_DEBUG) printf("Upcast to nsIDOMXULDocument failed\n");
                }
            } else {
                if (APP_DEBUG) printf("GetDocument failed, rv=0x%X\n",(int)rv);
            }
        } else {
            if (APP_DEBUG) printf("Upcast to nsIDocumentViewer failed\n");
        }
    } else {
        if (APP_DEBUG) printf("GetContentViewer failed, rv=0x%X\n",(int)rv);
    }
    return rv;
}

NS_IMETHODIMP 
nsBrowserAppCore::BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL)
{
  setAttribute( mWebShell, "Browser:Throbber", "busy", "true" );
  return NS_OK;
}

NS_IMETHODIMP 
nsBrowserAppCore::ProgressLoadURL(nsIWebShell* aShell, const PRUnichar* aURL,
                                  PRInt32 aProgress, PRInt32 aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsBrowserAppCore::EndLoadURL(nsIWebShell* aWebShell, const PRUnichar* aURL,
                             PRInt32 aStatus)
{
  setAttribute( mWebShell, "Browser:Throbber", "busy", "false" );
  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::NewWindow()
{  
  nsresult rv;
  nsString controllerCID;
 
  char * urlstr = nsnull;

  nsIAppShellService* appShell = nsnull;

  // Default URL if one was not provided in the cmdline
  if (nsnull == urlstr)
      urlstr = "resource:/res/samples/navigator.xul";
  else
      fprintf(stderr, "URL to load is %s\n", urlstr);

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

  /*
   * XXX: Currently, the CID for the "controller" is passed in as an argument 
   *      to CreateTopLevelWindow(...).  Once XUL supports "controller" 
   *      components this will be specified in the XUL description...
   */
  controllerCID = "43147b80-8a39-11d2-9938-0080c7cb1081";
  appShell->CreateTopLevelWindow(nsnull, url, controllerCID, newWindow,
              nsnull, nsnull, 615, 650);
  NS_RELEASE(url);
  
done:
  /* Release the shell... */
  if (nsnull != appShell) {
    nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);
  }

  return NS_OK;
}

//----------------------------------------
void nsBrowserAppCore::SetButtonImage(nsIDOMNode * aParentNode, PRInt32 aBtnNum, const nsString &aResName)
{
  PRInt32 count = 0;
  nsCOMPtr<nsIDOMNode> button(FindNamedDOMNode(nsAutoString("button"), aParentNode, count, aBtnNum)); 
  count = 0;
  nsCOMPtr<nsIDOMNode> img(FindNamedDOMNode(nsAutoString("img"), button, count, 1)); 
  nsCOMPtr<nsIDOMHTMLImageElement> imgElement(do_QueryInterface(img));
  if (imgElement) {
    char * str = aResName.ToNewCString();
    imgElement->SetSrc(str);
    delete [] str;
  }

}


NS_IMETHODIMP    
nsBrowserAppCore::PrintPreview()
{ 
  if (!mWebShell) 
    return NS_OK;

  //////////////////////////////////////////////////////
  // This is Experimental code showing one approach 
  // to dynamically changing the toolbar icons.
  //////////////////////////////////////////////////////

  // first get the toolbar child WebShell
  nsCOMPtr<nsIContentViewer> cv;
  mWebShell->GetContentViewer(getter_AddRefs(cv));
  if (!cv)
    return NS_OK;
   
  nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
  if (!docv)
    return NS_OK;

  nsCOMPtr<nsIDocument> doc;
  docv->GetDocument(*getter_AddRefs(doc));
  if (!doc)
    return NS_OK;

  nsCOMPtr<nsIDOMDocument> domDoc(do_QueryInterface(doc));
  nsCOMPtr<nsIDOMNode> parentNode(GetParentNodeFromDOMDoc(domDoc)); 

  SetButtonImage(parentNode, 1, "resource:/res/toolbar/TB_NewBack.gif");
  SetButtonImage(parentNode, 2, "resource:/res/toolbar/TB_NewForward.gif");
  SetButtonImage(parentNode, 3, "resource:/res/toolbar/TB_NewReload.gif");
  SetButtonImage(parentNode, 4, "resource:/res/toolbar/TB_NewStop.gif");
  SetButtonImage(parentNode, 5, "resource:/res/toolbar/TB_NewHome.gif"); 
  SetButtonImage(parentNode, 6, "resource:/res/toolbar/TB_NewPrint.gif"); 
  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::Close()
{  
  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::Exit()
{  
  nsIAppShellService* appShell = nsnull;

  /*
   * Create the Application Shell instance...
   */
  nsresult rv = nsServiceManager::GetService(kAppShellServiceCID,
                                             kIAppShellServiceIID,
                                             (nsISupports**)&appShell);
  if (NS_SUCCEEDED(rv)) {
    appShell->Shutdown();
    nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);
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

  char * urlstr = nsnull;

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
  appShell->CreateDialogWindow(parent, url, controllerCID, newWindow,
              (nsIStreamObserver *)this, nsnull, 516, 650);
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
  const char *urlString = 0;
  aURL->GetSpec( &urlString );
  if ( urlString ) { 
    setAttribute( mWebShell, "Browser:OnStartBinding", "content-type", aContentType );
    setAttribute( mWebShell, "Browser:OnStartBinding", "url", urlString );
  }
  return rv;
}


NS_IMETHODIMP
nsBrowserAppCore::OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax)
{
  nsresult rv = NS_OK;
  PRUint32 progress = aProgressMax ? ( aProgress * 100 ) / aProgressMax : 0;
  const char *urlString = 0;
  aURL->GetSpec( &urlString );
  return rv;
}


NS_IMETHODIMP
nsBrowserAppCore::OnStatus(nsIURL* aURL, const PRUnichar* aMsg)
{
  nsresult rv = setAttribute( mWebShell, "Browser:Status", "text", aMsg );
  return rv;
}


NS_IMETHODIMP
nsBrowserAppCore::OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg)
{
  nsresult rv = NS_OK;
  const char *urlString = 0;
  aURL->GetSpec( &urlString );
  if ( urlString ) { 
    char status[256];
    PR_snprintf( status, sizeof status, "0x%08X", (int) aStatus );
    setAttribute( mWebShell, "Browser:OnStopBinding", "status", status );
    setAttribute( mWebShell, "Browser:OnStopBinding", "text", aMsg );
    setAttribute( mWebShell, "Browser:OnStopBinding", "url", urlString );
  }
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


