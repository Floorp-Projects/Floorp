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
#include "nsIDOMAppCoresManager.h"

#include "nsIFileWidget.h"
#include "nsWidgetsCID.h"

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
#include "nsICmdLineService.h"
#include "nsIGlobalHistory.h"

#include "nsIRelatedLinksDataSource.h"
#include "nsIDOMXULDocument.h"

#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsFileSpec.h"  // needed for nsAutoCString

// FileDialog
static NS_DEFINE_IID(kCFileWidgetCID, NS_FILEWIDGET_CID);
static NS_DEFINE_IID(kIFileWidgetIID, NS_IFILEWIDGET_IID);

#if defined(ClientWallet) || defined(SingleSignon)
#ifdef ClientWallet
#include "nsIFileLocator.h"
#include "nsFileLocations.h"
static NS_DEFINE_IID(kIFileLocatorIID, NS_IFILELOCATOR_IID);
static NS_DEFINE_CID(kFileLocatorCID, NS_FILELOCATOR_CID);
#endif
#include "nsIWalletService.h"
static NS_DEFINE_IID(kIWalletServiceIID, NS_IWALLETSERVICE_IID);
static NS_DEFINE_IID(kWalletServiceCID, NS_WALLETSERVICE_CID);
#endif

// Interface for "unknown content type handler" component/service.
#include "nsIUnknownContentTypeHandler.h"

// Stuff to implement file download dialog.
#include "nsIXULWindowCallbacks.h"
#include "nsIDocumentObserver.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsFileStream.h"
#include "nsINetService.h"
NS_DEFINE_IID(kINetServiceIID,            NS_INETSERVICE_IID);
NS_DEFINE_IID(kNetServiceCID,             NS_NETSERVICE_CID);

// For related links
#include "nsRDFCID.h"
#include "nsIRDFService.h"
NS_DEFINE_CID(kRDFServiceCID,             NS_RDFSERVICE_CID);

// Stuff to implement find/findnext
#include "nsIFindComponent.h"


static NS_DEFINE_IID(kIDocumentViewerIID, NS_IDOCUMENT_VIEWER_IID);


/* Define Class IDs */
static NS_DEFINE_IID(kAppShellServiceCID,        NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kBrowserAppCoreCID,         NS_BROWSERAPPCORE_CID);
static NS_DEFINE_IID(kCmdLineServiceCID,    NS_COMMANDLINE_SERVICE_CID);
static NS_DEFINE_IID(kCGlobalHistoryCID,       NS_GLOBALHISTORY_CID);

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
static NS_DEFINE_IID(kIGlobalHistoryIID,       NS_IGLOBALHISTORY_IID);

#define APP_DEBUG 0

#define FILE_PROTOCOL "file://"

static nsresult
FindNamedXULElement(nsIWebShell * aShell, const char *aId, nsCOMPtr<nsIDOMElement> * aResult );


static nsresult setAttribute( nsIWebShell *shell,
                              const char *id,
                              const char *name,
                              const nsString &value );
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
  mGHistory             = nsnull;
  mSearchContext        = nsnull;

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
  NS_IF_RELEASE(mSearchContext);

  if (nsnull != mGHistory) {
    nsServiceManager::ReleaseService(kCGlobalHistoryCID, mGHistory);
  }

  DecInstanceCount();  
}


NS_IMPL_ADDREF_INHERITED(nsBrowserAppCore, nsBaseAppCore)
NS_IMPL_RELEASE_INHERITED(nsBrowserAppCore, nsBaseAppCore)


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
nsBrowserAppCore::Init(const nsString& aId)
{
   
  nsBaseAppCore::Init(aId);



  // register object into Service Manager
  static NS_DEFINE_IID(kIDOMAppCoresManagerIID, NS_IDOMAPPCORESMANAGER_IID);
  static NS_DEFINE_IID(kAppCoresManagerCID,  NS_APPCORESMANAGER_CID);

  nsIDOMAppCoresManager * appCoreManager;
  nsresult rv = nsServiceManager::GetService(kAppCoresManagerCID,
                                             kIDOMAppCoresManagerIID,
                                             (nsISupports**)&appCoreManager);
  if (NS_OK == rv) {
	  appCoreManager->Add((nsIDOMBaseAppCore *)(nsBaseAppCore *)this);
    nsServiceManager::ReleaseService(kAppCoresManagerCID, appCoreManager);
  }

  // Get the Global history service  
  nsServiceManager::GetService(kCGlobalHistoryCID, kIGlobalHistoryIID,
					(nsISupports **)&mGHistory);

  return rv;

}

NS_IMETHODIMP    
nsBrowserAppCore::SetDocumentCharset(const nsString& aCharset)
{
  nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(mContentWindow) );
  if (!globalObj) {
    return NS_ERROR_FAILURE;
  }

  nsIWebShell * webShell;
  globalObj->GetWebShell(&webShell);
  if (nsnull != webShell) {
    webShell->SetDefaultCharacterSet( aCharset.GetUnicode());

    NS_RELEASE(webShell);
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::Back()
{
  mContentAreaWebShell->Back();
	return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::Forward()
{
  mContentAreaWebShell->Forward();
	return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::Stop()
{
  mContentAreaWebShell->Stop();
  setAttribute( mWebShell, "Browser:Throbber", "busy", "false" );
	return NS_OK;
}

#ifdef ClientWallet
#define WALLET_EDITOR_NAME "walleted.html"

#define WALLET_SAMPLES_URL "http://people.netscape.com/morse/wallet/samples/"
//#define WALLET_SAMPLES_URL "http://peoplestage/morse/wallet/samples/"

nsresult ProfileDirectory(nsFileSpec& dirSpec) {
  nsresult rv;
  nsIFileLocator* locator = nsnull;
  rv = nsServiceManager::GetService
    (kFileLocatorCID, kIFileLocatorIID, (nsISupports**)&locator);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!locator) {
    return NS_ERROR_FAILURE;
  }
  rv = locator->GetFileLocation
     (nsSpecialFileSpec::App_UserProfileDirectory50, &dirSpec);
  nsServiceManager::ReleaseService(kFileLocatorCID, locator);
  return rv;
}

PRInt32
newWind(char* urlName) {
  nsresult rv;
  
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
  nsIWebShellWindow* newWindow;
  
  rv = NS_NewURL(&url, urlstr);
  if (NS_FAILED(rv)) {
    goto done;
  }

  appShell->CreateTopLevelWindow(nsnull, url, PR_TRUE, newWindow,
              nsnull, nsnull, 615, 480);

  NS_RELEASE(url);
  
done:
  /* Release the shell... */
  if (nsnull != appShell) {
    nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);
  }

  return NS_OK;
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
  nsFileSpec dirSpec;
  nsresult rv = ProfileDirectory(dirSpec);
  if (NS_FAILED(rv)) {
    return rv;
  }
  nsFileURL u = nsFileURL(dirSpec + WALLET_EDITOR_NAME);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    nsIURL * url;
    if (!NS_FAILED(NS_NewURL(&url, (char *)(u.GetURLString())))) {
      res = walletservice->WALLET_PreEdit(url);
      nsServiceManager::ReleaseService(kWalletServiceCID, walletservice);
    }
  }

  /* bring up the wallet editor in a new window */
  return newWind((char *)(u.GetURLString()));
}

NS_IMETHODIMP    
nsBrowserAppCore::WalletChangePassword()
{
  nsIWalletService *walletservice;
  nsresult res;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->WALLET_ChangePassword();
    nsServiceManager::ReleaseService(kWalletServiceCID, walletservice);
  }
  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::WalletSafeFillin(nsIDOMWindow* aWin)
{
  NS_PRECONDITION(aWin != nsnull, "null ptr");
  if (! aWin)
    return NS_ERROR_NULL_POINTER;

  nsIPresShell* shell;
  shell = nsnull;
  nsCOMPtr<nsIWebShell> webcontent; 

  nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject; 
  scriptGlobalObject = do_QueryInterface(aWin); 
  scriptGlobalObject->GetWebShell(getter_AddRefs(webcontent)); 

  nsresult res;
  nsString urlString = nsString("");
  if ( mContentAreaWebShell ) {
    const PRUnichar *url = 0;
    PRInt32 history;
    res = mContentAreaWebShell->GetHistoryIndex(history);
    if (NS_SUCCEEDED(res)) {
      res = mContentAreaWebShell->GetURL( history, &url );
      if (NS_SUCCEEDED(res)) {
        urlString = nsString(url);
      }
    }
  }

  shell = GetPresShellFor(webcontent);
  nsIWalletService *walletservice;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if (NS_SUCCEEDED(res) && (nsnull != walletservice)) {
    res = walletservice->WALLET_Prefill(shell, urlString, PR_FALSE);
    nsServiceManager::ReleaseService(kWalletServiceCID, walletservice);
  }

#ifndef HTMLDialogs 
  if (NS_SUCCEEDED(res)) {
    return newWind("file:///htmldlgs.htm");
  }
#endif

  return NS_OK;
}

#include "nsIDOMHTMLDocument.h"
static NS_DEFINE_IID(kIDOMHTMLDocumentIID, NS_IDOMHTMLDOCUMENT_IID);
NS_IMETHODIMP    
nsBrowserAppCore::WalletQuickFillin(nsIDOMWindow* aWin)
{
  NS_PRECONDITION(aWin != nsnull, "null ptr");
  if (! aWin)
    return NS_ERROR_NULL_POINTER;

  nsIPresShell* shell;
  shell = nsnull;
  nsCOMPtr<nsIWebShell> webcontent; 

  nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject; 
  scriptGlobalObject = do_QueryInterface(aWin); 
  scriptGlobalObject->GetWebShell(getter_AddRefs(webcontent)); 

  shell = GetPresShellFor(webcontent);
  nsIWalletService *walletservice;
  nsresult res;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    nsString urlString = nsString("");
    res = walletservice->WALLET_Prefill(shell, urlString, PR_TRUE);
    nsServiceManager::ReleaseService(kWalletServiceCID, walletservice);
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::WalletSamples()
{
  /* bring up the samples in a new window */
  mContentAreaWebShell->LoadURL(nsString(WALLET_SAMPLES_URL).GetUnicode(), nsnull, nsnull);
  return NS_OK;
}

#else
NS_IMETHODIMP
nsBrowserAppCore::WalletEditor() {
  return NS_OK;
}
NS_IMETHODIMP    
nsBrowserAppCore::WalletSamples() {
  return NS_OK;
}
NS_IMETHODIMP
nsBrowserAppCore::WalletChangePassword() {
  return NS_OK;
}
NS_IMETHODIMP
nsBrowserAppCore::WalletQuickFillin(nsIDOMWindow*) {
  return NS_OK;
}
NS_IMETHODIMP
nsBrowserAppCore::WalletSafeFillin(nsIDOMWindow*) {
  return NS_OK;
}
#endif

#ifdef SingleSignon
NS_IMETHODIMP    
nsBrowserAppCore::SignonViewer()
{
  nsIWalletService *walletservice;
  nsresult res;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->SI_DisplaySignonInfoAsHTML();
    nsServiceManager::ReleaseService(kWalletServiceCID, walletservice);
  }
#ifndef HTMLDialogs 
  return newWind("file:///htmldlgs.htm");
#endif
  return NS_OK;
}
#else
NS_IMETHODIMP    
nsBrowserAppCore::SignonViewer()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

#ifdef CookieManagement
NS_IMETHODIMP    
nsBrowserAppCore::CookieViewer()
{
  nsINetService *netservice;
  nsresult res;
  res = nsServiceManager::GetService(kNetServiceCID,
                                     kINetServiceIID,
                                     (nsISupports **)&netservice);
  if ((NS_OK == res) && (nsnull != netservice)) {
    res = netservice->NET_DisplayCookieInfoAsHTML();
    nsServiceManager::ReleaseService(kNetServiceCID, netservice);
  }
#ifndef HTMLDialogs 
  return newWind("file:///htmldlgs.htm");
#endif
  return NS_OK;
}
#else
NS_IMETHODIMP    
nsBrowserAppCore::CookieViewer()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
#endif

NS_IMETHODIMP    
nsBrowserAppCore::LoadUrl(const nsString& aUrl)
{
  char * urlstr = nsnull;
  urlstr = aUrl.ToNewCString();

  if (!urlstr)
    return NS_OK;

  /* Ask nsWebShell to load the URl */
  nsString id;
  GetId(id);
  if ( id.Find("ViewSource") == 0 ) {
    // Viewing source, load with "view-source" command.
    mContentAreaWebShell->LoadURL(nsString(urlstr).GetUnicode(), "view-source", nsnull, PR_FALSE );
  } else {
    // Normal browser.
    mContentAreaWebShell->LoadURL(nsString(urlstr).GetUnicode());
  }

  delete[] urlstr;

  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::LoadInitialPage(void)
{
  static PRBool cmdLineURLUsed = PR_FALSE;
  char * urlstr = nsnull;
  nsresult rv;
  nsICmdLineService * cmdLineArgs;

  // Examine content URL.
  if ( mContentAreaWebShell ) {
      const PRUnichar *url = 0;
      nsresult rv = mContentAreaWebShell->GetURL( 0, &url );
      if ( NS_SUCCEEDED( rv ) ) {
          if ( nsString(url) != "about:blank" ) {
              // Something has already been loaded (probably via window.open),
              // leave it be.
              return NS_OK;
          }
      }
  }

  rv = nsServiceManager::GetService(kCmdLineServiceCID,
                                    kICmdLineServiceIID,
                                    (nsISupports **)&cmdLineArgs);
  if (NS_FAILED(rv)) {
    if (APP_DEBUG) fprintf(stderr, "Could not obtain CmdLine processing service\n");
    return NS_ERROR_FAILURE;
  }

  // Get the URL to load
  rv = cmdLineArgs->GetURLToLoad(&urlstr);
  nsServiceManager::ReleaseService(kCmdLineServiceCID, cmdLineArgs);

  if ( !cmdLineURLUsed && urlstr != nsnull) {
  // A url was provided. Load it
     if (APP_DEBUG) printf("Got Command line URL to load %s\n", urlstr);
     rv = LoadUrl(nsString(urlstr));
     cmdLineURLUsed = PR_TRUE;
     return rv;
  }

  // No URL was provided in the command line. Load the default provided
  // in the navigator.xul;

  nsCOMPtr<nsIDOMElement>    argsElement;

  rv = FindNamedXULElement(mWebShell, "args", &argsElement);
  if (!argsElement) {
  // Couldn't get the "args" element from the xul file. Load a blank page
     if (APP_DEBUG) printf("Couldn't find args element\n");
     nsString * url = new nsString("about:blank"); 
     rv = LoadUrl(nsString(urlstr));
     return rv;
  }

  // Load the default page mentioned in the xul file.
    nsString value;
    argsElement->GetAttribute(nsString("value"), value);
    if ((value.ToNewCString()) != "") {
        if (APP_DEBUG) printf("Got args value from xul file to be %s\n", value.ToNewCString());
    rv = LoadUrl(value);
    return rv;
    }

    if (APP_DEBUG) printf("Quitting LoadInitialPage\n");
    return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::SetToolbarWindow(nsIDOMWindow* aWin)
{
  NS_PRECONDITION(aWin != nsnull, "null ptr");
  if (! aWin)
    return NS_ERROR_NULL_POINTER;

  mToolbarWindow = aWin;
  NS_ADDREF(aWin);
  mToolbarScriptContext = GetScriptContext(aWin);

	return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::SetContentWindow(nsIDOMWindow* aWin)
{
  NS_PRECONDITION(aWin != nsnull, "null ptr");
  if (! aWin)
    return NS_ERROR_NULL_POINTER;

  mContentWindow = aWin;

  NS_ADDREF(aWin);
  mContentScriptContext = GetScriptContext(aWin);
  nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(mContentWindow) );
  if (!globalObj) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIWebShell> webShell;
  globalObj->GetWebShell(getter_AddRefs(webShell));
  if (webShell) {
    mContentAreaWebShell = webShell;
    NS_ADDREF(mContentAreaWebShell);
    webShell->SetDocLoaderObserver((nsIDocumentLoaderObserver *)this);

    const PRUnichar * name;
    webShell->GetName( &name);
    nsAutoString str(name);

    if (APP_DEBUG) {
      printf("Attaching to Content WebShell [%s]\n", nsAutoCString(str));
    }
  }

  return NS_OK;

}



NS_IMETHODIMP    
nsBrowserAppCore::SetWebShellWindow(nsIDOMWindow* aWin)
{
  NS_PRECONDITION(aWin != nsnull, "null ptr");
  if (! aWin)
    return NS_ERROR_NULL_POINTER;

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

    if (APP_DEBUG) {
      printf("Attaching to WebShellWindow[%s]\n", nsAutoCString(str));
    }

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



// Utility to extract document from a webshell object.
static nsCOMPtr<nsIDocument> getDocument( nsIWebShell *aWebShell ) {
    nsCOMPtr<nsIDocument> result;

    // Get content viewer from the web shell.
    nsCOMPtr<nsIContentViewer> contentViewer;
    nsresult rv = aWebShell ? aWebShell->GetContentViewer(getter_AddRefs(contentViewer))
                            : NS_ERROR_NULL_POINTER;

    if ( contentViewer ) {
        // Up-cast to a document viewer.
        nsCOMPtr<nsIDocumentViewer> docViewer(do_QueryInterface(contentViewer));
        if ( docViewer ) {
            // Get the document from the doc viewer.
            docViewer->GetDocument(*getter_AddRefs(result));
        } else {
            if (APP_DEBUG) printf( "%s %d: Upcast to nsIDocumentViewer failed\n",
                                   __FILE__, (int)__LINE__ );
        }
    } else {
        if (APP_DEBUG) printf( "%s %d: GetContentViewer failed, rv=0x%X\n",
                               __FILE__, (int)__LINE__, (int)rv );
    }
    return result;
}

// Utility to set element attribute.
static nsresult setAttribute( nsIWebShell *shell,
                              const char *id,
                              const char *name,
                              const nsString &value ) {
    nsresult rv = NS_OK;

    nsCOMPtr<nsIDocument> doc = getDocument( shell );

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
          if (APP_DEBUG)   printf("Upcast to nsIDOMXULDocument failed\n");
        }
    } else {
        if (APP_DEBUG) printf("getDocument failed, rv=0x%X\n",(int)rv);
    }
    return rv;
}

// nsIDocumentLoaderObserver methods

NS_IMETHODIMP
nsBrowserAppCore::OnStartDocumentLoad(nsIDocumentLoader* loader, nsIURL* aURL, const char* aCommand)
{
  // Kick start the throbber
   setAttribute( mWebShell, "Browser:Throbber", "busy", "true" );

  // Enable the Stop buton
   setAttribute( mWebShell, "canStop", "disabled", "false" );

   return NS_OK;
}


NS_IMETHODIMP
nsBrowserAppCore::OnEndDocumentLoad(nsIDocumentLoader* loader, nsIURL *aUrl, PRInt32 aStatus)
{

    const char* spec =nsnull;

	aUrl->GetSpec(&spec);

    if (aStatus != NS_OK) {
		goto done;
	}
    
    // Update global history.
    //NS_ASSERTION(mGHistory != nsnull, "history not initialized");
    if (mGHistory && mWebShell) {
        nsresult rv;

        nsAutoString url(spec);
        char* urlSpec = url.ToNewCString();
        do {
            if (NS_FAILED(rv = mGHistory->AddPage(urlSpec, /* XXX referrer? */ nsnull, PR_Now()))) {
                NS_ERROR("unable to add page to history");
                break;
            }

            const PRUnichar* title;
            if (NS_FAILED(rv = mWebShell->GetTitle(&title))) {
                NS_ERROR("unable to get doc title");
                break;
            }

            if (NS_FAILED(rv = mGHistory->SetPageTitle(urlSpec, title))) {
                NS_ERROR("unable to set doc title");
                break;
            }
        } while (0);
        delete[] urlSpec;
    }

    // Update Related Links
    if (mWebShell)
    {
    	nsCOMPtr<nsIScriptContextOwner>	newContextOwner;
    	newContextOwner = do_QueryInterface(mWebShell);
    	if (newContextOwner)
    	{
    		nsCOMPtr<nsIScriptGlobalObject>	newGlobalObject;
    		nsresult			rv;
    		if (NS_SUCCEEDED(rv = newContextOwner->GetScriptGlobalObject(getter_AddRefs(newGlobalObject))))
    		{
    			if (newGlobalObject)
			{
				nsCOMPtr<nsIDOMWindow>	aDOMWindow;
				aDOMWindow = do_QueryInterface(newGlobalObject);
				if (aDOMWindow)
				{
					nsCOMPtr<nsIDOMDocument>	aDOMDocument;
					if (NS_SUCCEEDED(rv = aDOMWindow->GetDocument(getter_AddRefs(aDOMDocument))))
					{
						nsCOMPtr<nsIDOMXULDocument>	xulDoc( do_QueryInterface(aDOMDocument) );
						if (xulDoc)
						{
							NS_WITH_SERVICE(nsIRDFService, rdfService, kRDFServiceCID, &rv);
							if (NS_SUCCEEDED(rv))
							{
								nsCOMPtr<nsIRDFDataSource>	relatedLinksDS;
								if (NS_SUCCEEDED(rv = rdfService->GetDataSource("rdf:relatedlinks", getter_AddRefs(relatedLinksDS))))
								{
									nsCOMPtr<nsIRDFRelatedLinksDataSource>	rl(do_QueryInterface(relatedLinksDS));
									if (rl)
									{
										rl->SetRelatedLinksURL(spec);
									}
								}
							}
						}
					}
				}
    			}
    		}
    	}
    }
    
done:
     // Stop the throbber and set the urlbar string
    setAttribute( mWebShell, "urlbar", "value", spec );
    setAttribute( mWebShell, "Browser:Throbber", "busy", "false" );

     // Check with the content area webshell if back and forward
     // buttons can be enabled
    nsresult rv = mContentAreaWebShell->CanForward();
    setAttribute(mWebShell, "canGoForward", "disabled", (rv == NS_OK) ? "" : "true");

    rv = mContentAreaWebShell->CanBack();
    setAttribute(mWebShell, "canGoBack", "disabled", (rv == NS_OK) ? "" : "true");

	//Disable the Stop button
	setAttribute( mWebShell, "canStop", "disabled", "true" );

    /* To satisfy a request from the QA group */
	if (aStatus == NS_OK) {
      fprintf(stdout, "Document %s loaded successfully\n", spec);
      fflush(stdout);
	}
	else {
      fprintf(stdout, "Error loading URL %s \n", spec);
      fflush(stdout);
	}

   return NS_OK;
}

NS_IMETHODIMP
nsBrowserAppCore::HandleUnknownContentType(nsIDocumentLoader* loader, 
                                           nsIURL *aURL,
                                           const char *aContentType,
                                           const char *aCommand ) {
    nsresult rv = NS_OK;

    // Turn off the indicators in the chrome.
    setAttribute( mWebShell, "Browser:Throbber", "busy", "false" );

    // Get "unknown content type handler" and have it handle this.
    nsIUnknownContentTypeHandler *handler;
    rv = nsServiceManager::GetService( NS_IUNKNOWNCONTENTTYPEHANDLER_PROGID,
                                       nsIUnknownContentTypeHandler::GetIID(),
                                       (nsISupports**)&handler );

    if ( NS_SUCCEEDED( rv ) ) {
        /* Have handler take care of this. */
        rv = handler->HandleUnknownContentType( aURL, aContentType, loader );

        // Release the unknown content type handler service object.
        nsServiceManager::ReleaseService( NS_IUNKNOWNCONTENTTYPEHANDLER_PROGID, handler );
    } else {
        #ifdef NS_DEBUG
        printf( "%s %d: GetService failed for unknown content type handler, rv=0x%08X\n",
                __FILE__, (int)__LINE__, (int)rv );
        #endif
    }

    return rv;
}

NS_IMETHODIMP
nsBrowserAppCore::OnStartURLLoad(nsIDocumentLoader* loader, 
                                 nsIURL* aURL, const char* aContentType,
                                 nsIContentViewer* aViewer)
{

   return NS_OK;
}

NS_IMETHODIMP
nsBrowserAppCore::OnProgressURLLoad(nsIDocumentLoader* loader, 
                                    nsIURL* aURL, PRUint32 aProgress, 
                                    PRUint32 aProgressMax)
{
  nsresult rv = NS_OK;
  PRUint32 progress = aProgressMax ? ( aProgress * 100 ) / aProgressMax : 0;
  const char *urlString = 0;
  aURL->GetSpec( &urlString );
  return rv;
   return NS_OK;
}


NS_IMETHODIMP
nsBrowserAppCore::OnStatusURLLoad(nsIDocumentLoader* loader, 
                                  nsIURL* aURL, nsString& aMsg)
{
  nsresult rv = setAttribute( mWebShell, "Browser:Status", "text", aMsg );
   return rv;
}


NS_IMETHODIMP
nsBrowserAppCore::OnEndURLLoad(nsIDocumentLoader* loader, 
                               nsIURL* aURL, PRInt32 aStatus)
{

   return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::NewWindow()
{  
  nsresult rv;
  
  char * urlstr = nsnull;

  nsIAppShellService* appShell = nsnull;

  // Default URL if one was not provided in the cmdline
  if (nsnull == urlstr)
      urlstr = "chrome://navigator/content/";
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
  nsIWebShellWindow* newWindow;
  
  rv = NS_NewURL(&url, urlstr);
  if (NS_FAILED(rv)) {
    goto done;
  }

  appShell->CreateTopLevelWindow(nsnull, url, PR_TRUE, newWindow,
              nsnull, nsnull, 615, 480);
  NS_RELEASE(url);
  
done:
  /* Release the shell... */
  if (nsnull != appShell) {
    nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);
  }
    return NS_OK;
}


//----------------------------------------------------------
static void BuildFileURL(const char * aFileName, nsString & aFileURL) 
{
  nsAutoString fileName(aFileName);
  char * str = fileName.ToNewCString();

  PRInt32 len = strlen(str);
  PRInt32 sum = len + sizeof(FILE_PROTOCOL);
  char* lpszFileURL = new char[sum];

  // Translate '\' to '/'
  for (PRInt32 i = 0; i < len; i++) {
    if (str[i] == '\\') {
	    str[i] = '/';
    }
  }

  // Build the file URL
  PR_snprintf(lpszFileURL, sum, "%s%s", FILE_PROTOCOL, str);

  // create string
  aFileURL = lpszFileURL;
  delete[] lpszFileURL;
  delete[] str;
}

NS_IMETHODIMP    
nsBrowserAppCore::OpenWindow()
{  
  nsIFileWidget *fileWidget;

  nsString title("Open File");
  if (NS_OK == nsComponentManager::CreateInstance(kCFileWidgetCID, nsnull, kIFileWidgetIID, (void**)&fileWidget)) {
    nsString titles[] = {"All Readable Files", "HTML Files",
                         "XML Files", "Image Files", "All Files"};
    nsString filters[] = {"*.htm; *.html; *.xml; *.gif; *.jpg; *.jpeg; *.png",
                          "*.htm; *.html",
                          "*.xml",
                          "*.gif; *.jpg; *.jpeg; *.png",
                          "*.*"};
    fileWidget->SetFilterList(5, titles, filters);

    nsFileSpec fileSpec;
    if (fileWidget->GetFile(nsnull, title, fileSpec) == nsFileDlgResults_OK) {
      nsFileURL fileURL(fileSpec);
      char buffer[1024];
      const nsAutoCString cstr(fileURL.GetAsString());
      PR_snprintf( buffer, sizeof buffer, "OpenFile(\"%s\")", (const char*)cstr);
      ExecuteScript( mToolbarScriptContext, buffer );
    }
    NS_RELEASE(fileWidget);
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
    imgElement->SetSrc(aResName);
  }

}


NS_IMETHODIMP    
nsBrowserAppCore::PrintPreview()
{ 

  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::Copy()
{ 
  nsIPresShell * presShell = GetPresShellFor(mContentAreaWebShell);
  if (nsnull != presShell) {
    presShell->DoCopy();
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserAppCore::Print()
{  
  if (nsnull != mContentAreaWebShell) {
    nsIContentViewer *viewer = nsnull;

    mContentAreaWebShell->GetContentViewer(&viewer);

    if (nsnull != viewer) {
      viewer->Print();
      NS_RELEASE(viewer);
    }
  }
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

void
nsBrowserAppCore::InitializeSearch( nsIFindComponent *finder ) {
    nsresult rv = NS_OK;

    if ( finder && !mSearchContext ) {
        // Create the search context for this browser window.
        rv = finder->CreateContext( mContentAreaWebShell, &mSearchContext );
        if ( NS_FAILED( rv ) ) {
            #ifdef NS_DEBUG
            printf( "%s %d CreateContext failed, rv=0x%X\n",
                    __FILE__, (int)__LINE__, (int)rv );
            #endif
        }
    }
}

NS_IMETHODIMP    
nsBrowserAppCore::Find() {
    nsresult rv = NS_OK;

    // Get find component.
    nsIFindComponent *finder;
    rv = nsServiceManager::GetService( NS_IFINDCOMPONENT_PROGID,
                                       nsIFindComponent::GetIID(),
                                       (nsISupports**)&finder );
    if ( NS_SUCCEEDED( rv ) ) {
        // Make sure we've initialized searching for this document.
        InitializeSearch( finder );

        // Perform find via find component.
        if ( finder && mSearchContext ) {
            rv = finder->Find( mSearchContext );
        }

        // Release the service.
        nsServiceManager::ReleaseService( NS_IFINDCOMPONENT_PROGID, finder );
    } else {
        #ifdef NS_DEBUG
            printf( "%s %d: GetService failed for find component, rv=0x08%X\n",
                    __FILE__, (int)__LINE__, (int)rv );
        #endif
    }

    return rv;
}

NS_IMETHODIMP    
nsBrowserAppCore::FindNext() {
    nsresult rv = NS_OK;

    // Get find component.
    nsIFindComponent *finder;
    rv = nsServiceManager::GetService( NS_IFINDCOMPONENT_PROGID,
                                       nsIFindComponent::GetIID(),
                                       (nsISupports**)&finder );
    if ( NS_SUCCEEDED( rv ) ) {
        // Make sure we've initialized searching for this document.
        InitializeSearch( finder );

        // Perform find via find component.
        if ( finder && mSearchContext ) {
            rv = finder->FindNext( mSearchContext );
        }

        // Release the service.
        nsServiceManager::ReleaseService( NS_IFINDCOMPONENT_PROGID, finder );
    } else {
        #ifdef NS_DEBUG
            printf( "%s %d: GetService failed for find component, rv=0x08%X\n",
                    __FILE__, (int)__LINE__, (int)rv );
        #endif
    }

    return rv;
}

NS_IMETHODIMP    
nsBrowserAppCore::ExecuteScript(nsIScriptContext * aContext, const nsString& aScript)
{
  if (nsnull != aContext) {
    const char* url = "";
    PRBool isUndefined = PR_FALSE;
    nsString rVal;
    if (APP_DEBUG) {
      printf("Executing [%s]\n", nsAutoCString(aScript));
    }
    aContext->EvaluateString(aScript, url, 0, rVal, &isUndefined);
  } 
  return NS_OK;
}



NS_IMETHODIMP    
nsBrowserAppCore::DoDialog()
{
  // (adapted from nsToolkitCore)
  nsresult           rv;
  nsIWebShellWindow  *window;

  window = nsnull;

  nsCOMPtr<nsIURL> urlObj;
  rv = NS_NewURL(getter_AddRefs(urlObj), "resource://res/samples/Password.html");
  if (NS_FAILED(rv))
    return rv;

  NS_WITH_SERVICE(nsIAppShellService, appShell, kAppShellServiceCID, &rv);
  if (NS_FAILED(rv))
    return rv;

  rv = appShell->RunModalDialog(mWebShellWin, urlObj, window, nsnull, nsnull, 300, 200);
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


static nsresult
FindNamedXULElement(nsIWebShell * aShell,
                              const char *aId,
                              nsCOMPtr<nsIDOMElement> * aResult ) {
    nsresult rv = NS_OK;

    nsCOMPtr<nsIContentViewer> cv;
    rv = aShell ? aShell->GetContentViewer(getter_AddRefs(cv))
               : NS_ERROR_NULL_POINTER;
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

                    rv = xulDoc->GetElementById( aId, getter_AddRefs(elem) );
                    if ( elem ) {
			*aResult =  elem;
                    } else {
                       if (APP_DEBUG) printf("GetElementByID failed, rv=0x%X\n",(int)rv);
                    }
                } else {
                  if (APP_DEBUG)   printf("Upcast to nsIDOMXULDocument failed\n");
                }

            } else {
               if (APP_DEBUG)  printf("GetDocument failed, rv=0x%X\n",(int)rv);
            }
        } else {
             if (APP_DEBUG)  printf("Upcast to nsIDocumentViewer failed\n");
        }
    } else {
       if (APP_DEBUG) printf("GetContentViewer failed, rv=0x%X\n",(int)rv);
    }
    return rv;
}


