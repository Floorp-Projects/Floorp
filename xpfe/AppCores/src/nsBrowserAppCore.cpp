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
#include "nsIWalletService.h"
static NS_DEFINE_IID(kIWalletServiceIID, NS_IWALLETSERVICE_IID);
static NS_DEFINE_IID(kWalletServiceCID, NS_WALLETSERVICE_CID);
#endif

// Stuff to implement file download dialog.
#include "nsIXULWindowCallbacks.h"
#include "nsIDocumentObserver.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"
#include "nsFileStream.h"
#include "nsINetService.h"
NS_DEFINE_IID(kINetServiceIID,            NS_INETSERVICE_IID);
NS_DEFINE_IID(kNetServiceCID,             NS_NETSERVICE_CID);

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
static NS_DEFINE_IID(kIURLListenerIID,           NS_IURL_LISTENER_IID);
static NS_DEFINE_IID(kIGlobalHistoryIID,       NS_IGLOBALHISTORY_IID);

#define APP_DEBUG 0

#define FILE_PROTOCOL "file://"

static nsresult
FindNamedXULElement(nsIWebShell * aShell, const char *aId, nsCOMPtr<nsIDOMElement> * aResult );

/////////////////////////////////////////////////////////////////////////
// nsBrowserAppCore
/////////////////////////////////////////////////////////////////////////

nsIFindComponent *nsBrowserAppCore::mFindComponent = 0;

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
  NS_IF_RELEASE(mGHistory);
  NS_IF_RELEASE(mSearchContext);
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

#ifdef ClientWallet
//#define WALLET_EDITOR_URL "resource:/res/samples/walleted.html"
//#define WALLET_EDITOR_URL "http://peoplestage/morse/wallet/walleted.html"
#define WALLET_EDITOR_URL "http://people.netscape.com/morse/wallet/walleted.html"

#define WALLET_SAMPLES_URL "http://people.netscape.com/morse/wallet/samples/"
//#define WALLET_SAMPLES_URL "http://peoplestage/morse/wallet/samples/"

PRInt32
newWind(char* urlName) {
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
  nsIWebShellWindow* newWindow;
  
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
  if ((NS_OK == res) && (nsnull != walletservice)) {
    nsIURL * url;
    if (!NS_FAILED(NS_NewURL(&url, WALLET_EDITOR_URL))) {
      res = walletservice->WALLET_PreEdit(url);
      NS_RELEASE(walletservice);
    }
  }

  /* bring up the wallet editor in a new window */
  return newWind(WALLET_EDITOR_URL);
}

NS_IMETHODIMP    
nsBrowserAppCore::WalletSafeFillin()
{
  nsIPresShell* shell;
  shell = nsnull;
  nsCOMPtr<nsIWebShell> webcontent; 
  mWebShell->FindChildWithName(nsAutoString("content").GetUnicode(), *getter_AddRefs(webcontent));
  nsCOMPtr<nsIWebShell> webcontent2; 
  webcontent->ChildAt(1, (nsIWebShell*&)webcontent2); 
  shell = GetPresShellFor(webcontent2);

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
  return newWind("file:///y|/htmldlgs.htm");
#endif

  return NS_OK;
}

#include "nsIDOMHTMLDocument.h"
static NS_DEFINE_IID(kIDOMHTMLDocumentIID, NS_IDOMHTMLDOCUMENT_IID);
NS_IMETHODIMP    
nsBrowserAppCore::WalletQuickFillin()
{
  nsIPresShell* shell;
  shell = nsnull;
  nsCOMPtr<nsIWebShell> webcontent; 
  mWebShell->FindChildWithName(nsAutoString("content").GetUnicode(), *getter_AddRefs(webcontent));
  nsCOMPtr<nsIWebShell> webcontent2; 
  webcontent->ChildAt(1, (nsIWebShell*&)webcontent2); 
  shell = GetPresShellFor(webcontent2);

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
nsBrowserAppCore::WalletQuickFillin() {
  return NS_OK;
}
NS_IMETHODIMP
nsBrowserAppCore::WalletSafeFillin() {
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
    NS_RELEASE(walletservice);
  }
#ifndef HTMLDialogs 
  return newWind("file:///y|/htmldlgs.htm");
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
    NS_RELEASE(netservice);
  }
#ifndef HTMLDialogs 
  return newWind("file:///y|/htmldlgs.htm");
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
  char * urlstr = nsnull;
  nsresult rv;
  nsICmdLineService * cmdLineArgs;

  rv = nsServiceManager::GetService(kCmdLineServiceCID,
                                    kICmdLineServiceIID,
                                    (nsISupports **)&cmdLineArgs);
  if (NS_FAILED(rv)) {
    if (APP_DEBUG) fprintf(stderr, "Could not obtain CmdLine processing service\n");
    return NS_ERROR_FAILURE;
  }

  // Get the URL to load
  rv = cmdLineArgs->GetURLToLoad(&urlstr);
  if (urlstr != nsnull) {
  // A url was provided. Load it
     if (APP_DEBUG) printf("Got Command line URL to load %s\n", urlstr);
     rv = LoadUrl(nsString(urlstr));
     return rv;
  }
  // No URL was provided in the command line. Load the default provided
  // in the navigator.xul;

  nsCOMPtr<nsIDOMElement>    argsElement;

  rv = FindNamedXULElement(mWebShell, "args", &argsElement);
  if (rv != NS_OK) {
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

  nsIWebShell * webShell;
  globalObj->GetWebShell(&webShell);
  if (nsnull != webShell) {
    mContentAreaWebShell = webShell;
    NS_ADDREF(webShell);
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

#if 0
  if (!mContentWindow) {
    return NS_ERROR_FAILURE;
  }
#endif  /* 0 */
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
nsBrowserAppCore::OnStartDocumentLoad(nsIURL* aURL, const char* aCommand)
{
  // Kick start the throbber
   setAttribute( mWebShell, "Browser:Throbber", "busy", "true" );
   return NS_OK;
}


NS_IMETHODIMP
nsBrowserAppCore::OnEndDocumentLoad(nsIURL *aUrl, PRInt32 aStatus)
{

    const char* spec =nsnull;
    
    aUrl->GetSpec(&spec);

    // Update global history.
    NS_ASSERTION(mGHistory != nsnull, "history not initialized");
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
							nsCOMPtr<nsIRDFService>		rdfService;
							if (NS_SUCCEEDED(rv = xulDoc->GetRdf(getter_AddRefs(rdfService))))
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
    
     // Stop the throbber and set the urlbar string
    setAttribute( mWebShell, "urlbar", "value", spec );
    setAttribute( mWebShell, "Browser:Throbber", "busy", "false" );

     // Check with the content area webshell if back and forward
     // buttons can be enabled
    nsresult rv = mContentAreaWebShell->CanForward();
    setAttribute(mWebShell, "canGoForward", "disabled", (rv == NS_OK) ? "" : "true");

    rv = mContentAreaWebShell->CanBack();
    setAttribute(mWebShell, "canGoBack", "disabled", (rv == NS_OK) ? "" : "true");

    /* To satisfy a request from the QA group */
    fprintf(stdout, "Document %s loaded successfully\n", spec);
    fflush(stdout);
   return NS_OK;
}

// Notice: This is only a temporary home for nsFileDownloadDialog!
// It will be moving to it's own component .h/.cpp file soon.
struct nsFileDownloadDialog : public nsIXULWindowCallbacks,
                                     nsIStreamListener,
                                     nsIDocumentObserver {
    // Declare implementation of ISupports stuff.
    NS_DECL_ISUPPORTS

    // Declare implementations of nsIXULWindowCallbacks interface functions.
    NS_IMETHOD ConstructBeforeJavaScript(nsIWebShell *aWebShell);
    NS_IMETHOD ConstructAfterJavaScript(nsIWebShell *aWebShell) { return NS_OK; }

    // Declare implementations of nsIStreamListener/nsIStreamObserver functions.
    NS_IMETHOD GetBindInfo(nsIURL* aURL, nsStreamBindingInfo* aInfo) { return NS_ERROR_NOT_IMPLEMENTED; }
    NS_IMETHOD OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, PRUint32 aLength);
    NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);
    NS_IMETHOD OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax);
    NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMsg);
    NS_IMETHOD OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg);

    // Declare implementations of nsIDocumentObserver functions.
    NS_IMETHOD BeginUpdate(nsIDocument *aDocument) { return NS_OK; }
    NS_IMETHOD EndUpdate(nsIDocument *aDocument) { return NS_OK; }
    NS_IMETHOD BeginLoad(nsIDocument *aDocument) { return NS_OK; }
    NS_IMETHOD EndLoad(nsIDocument *aDocument) { return NS_OK; }
    NS_IMETHOD BeginReflow(nsIDocument *aDocument, nsIPresShell* aShell) { return NS_OK; }
    NS_IMETHOD EndReflow(nsIDocument *aDocument, nsIPresShell* aShell) { return NS_OK; }
    NS_IMETHOD ContentChanged(nsIDocument *aDocument,
                              nsIContent* aContent,
                              nsISupports* aSubContent) { return NS_OK; }
    NS_IMETHOD ContentStatesChanged(nsIDocument* aDocument,
                                    nsIContent* aContent1,
                                    nsIContent* aContent2) { return NS_OK; }
    // This one we care about; see implementation below.
    NS_IMETHOD AttributeChanged(nsIDocument *aDocument,
                                nsIContent*  aContent,
                                nsIAtom*     aAttribute,
                                PRInt32      aHint);
    NS_IMETHOD ContentAppended(nsIDocument *aDocument,
                               nsIContent* aContainer,
                               PRInt32     aNewIndexInContainer) { return NS_OK; }
    NS_IMETHOD ContentInserted(nsIDocument *aDocument,
                               nsIContent* aContainer,
                               nsIContent* aChild,
                               PRInt32 aIndexInContainer) { return NS_OK; }
    NS_IMETHOD ContentReplaced(nsIDocument *aDocument,
                               nsIContent* aContainer,
                               nsIContent* aOldChild,
                               nsIContent* aNewChild,
                               PRInt32 aIndexInContainer) { return NS_OK; }
    NS_IMETHOD ContentRemoved(nsIDocument *aDocument,
                              nsIContent* aContainer,
                              nsIContent* aChild,
                              PRInt32 aIndexInContainer) { return NS_OK; }
    NS_IMETHOD StyleSheetAdded(nsIDocument *aDocument,
                               nsIStyleSheet* aStyleSheet) { return NS_OK; }
    NS_IMETHOD StyleSheetRemoved(nsIDocument *aDocument,
                                 nsIStyleSheet* aStyleSheet) { return NS_OK; }
    NS_IMETHOD StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                              nsIStyleSheet* aStyleSheet,
                                              PRBool aDisabled) { return NS_OK; }
    NS_IMETHOD StyleRuleChanged(nsIDocument *aDocument,
                                nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule,
                                PRInt32 aHint) { return NS_OK; }
    NS_IMETHOD StyleRuleAdded(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule) { return NS_OK; }
    NS_IMETHOD StyleRuleRemoved(nsIDocument *aDocument,
                                nsIStyleSheet* aStyleSheet,
                                nsIStyleRule* aStyleRule) { return NS_OK; }
    NS_IMETHOD DocumentWillBeDestroyed(nsIDocument *aDocument) { return NS_OK; }

    // nsFileDownloadDialog stuff
    nsFileDownloadDialog( nsIURL *aURL, const char *aContentType );
    ~nsFileDownloadDialog() { delete mOutput; delete [] mBuffer; }
    void OnOK( nsIContent *aContent );
    void OnClose();
    void OnStart();
    void OnStop();
    void SetWindow( nsIWebShellWindow *aWindow );

private:
    nsCOMPtr<nsIURL> mUrl;
    nsCOMPtr<nsIWebShell> mWebShell;
    nsCOMPtr<nsIWebShellWindow> mWindow;
    nsOutputFileStream *mOutput;
    nsString         mContentType;
    nsFileSpec       mFileName;
    PRUint32         mBufLen;
    char *           mBuffer;
    PRBool           mStopped;
    enum { kPrompt, kProgress } mMode;
}; // nsFileDownloadDialog

// Standard implementations of addref/release.
NS_IMPL_ADDREF( nsFileDownloadDialog );
NS_IMPL_RELEASE( nsFileDownloadDialog );

NS_IMETHODIMP 
nsFileDownloadDialog::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if (aIID.Equals(nsIDocumentObserver::GetIID())) {
    *aInstancePtr = (void*) ((nsIDocumentObserver*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIXULWindowCallbacks::GetIID())) {
    *aInstancePtr = (void*) ((nsIXULWindowCallbacks*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_ERROR_NO_INTERFACE;
}

// ctor
nsFileDownloadDialog::nsFileDownloadDialog( nsIURL *aURL, const char *aContentType )
        : mUrl( nsDontQueryInterface<nsIURL>(aURL) ),
          mWebShell(),
          mWindow(),
          mOutput(0),
          mBufLen( 8192 ),
          mBuffer( new char[ mBufLen ] ),
          mStopped( PR_FALSE ),
          mContentType( aContentType ),
          mMode( kPrompt ) {
    // Initialize ref count.
    NS_INIT_REFCNT();
}

// Do startup stuff from C++ side.
NS_IMETHODIMP
nsFileDownloadDialog::ConstructBeforeJavaScript(nsIWebShell *aWebShell) {
    nsresult rv = NS_OK;

    // Save web shell pointer.
    mWebShell = nsDontQueryInterface<nsIWebShell>( aWebShell );

    // Store instance information into dialog's DOM.
    const char *loc = 0;
    mUrl->GetSpec( &loc );
    setAttribute( mWebShell, "data.location", "value", loc );
    setAttribute( mWebShell, "data.contentType", "value", mContentType );

    // If showing download progress, make target file name known.
    if ( mMode == kProgress ) {
        setAttribute( mWebShell, "data.fileName", "value", nsString((const char*)mFileName) );
    }

    // Add as observer of the xul document.
    nsCOMPtr<nsIContentViewer> cv;
    rv = mWebShell->GetContentViewer(getter_AddRefs(cv));
    if ( cv ) {
        // Up-cast.
        nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
        if ( docv ) {
            // Get the document from the doc viewer.
            nsCOMPtr<nsIDocument> doc;
            rv = docv->GetDocument(*getter_AddRefs(doc));
            if ( doc ) {
                doc->AddObserver( this );
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
nsFileDownloadDialog::OnDataAvailable(nsIURL* aURL, nsIInputStream *aIStream, PRUint32 aLength) {
    nsresult rv = NS_OK;

    // Check for download cancelled by user.
    if ( mStopped ) {
        // Close the output file.
        if ( mOutput ) {
            mOutput->close();
        }
        // Close the input stream.
        aIStream->Close();
    } else {
        // Allocate buffer space.
        if ( aLength > mBufLen ) {
            char *oldBuffer = mBuffer;
    
            mBuffer = new char[ aLength ];
    
            if ( mBuffer ) {
                // Use new (bigger) buffer.
                mBufLen = aLength;
                // Delete old (smaller) buffer.
                delete [] oldBuffer;
            } else {
                // Keep the one we've got.
                mBuffer = oldBuffer;
            }
        }
    
        // Read the data.
        PRUint32 bytesRead;
        rv = aIStream->Read( mBuffer, ( mBufLen > aLength ) ? aLength : mBufLen, &bytesRead );
    
        if ( NS_SUCCEEDED(rv) ) {
            // Write the data just read to the output stream.
            if ( mOutput ) {
                mOutput->write( mBuffer, bytesRead );
            }
        } else {
            printf( "Error reading stream, rv=0x%X\n", (int)rv );
        }
    }

    return rv;
}

NS_IMETHODIMP
nsFileDownloadDialog::OnStartBinding(nsIURL* aURL, const char *aContentType) {
    nsresult rv = NS_OK;
    return NS_OK;
}

NS_IMETHODIMP
nsFileDownloadDialog::OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax) {
    nsresult rv = NS_OK;
    char buf[16];
    PR_snprintf( buf, sizeof buf, "%lu", aProgressMax );
    setAttribute( mWebShell, "data.progress", "max", buf );
    PR_snprintf( buf, sizeof buf, "%lu", aProgress );
    setAttribute( mWebShell, "data.progress", "value", buf );
    return NS_OK;
}

NS_IMETHODIMP
nsFileDownloadDialog::OnStatus(nsIURL* aURL, const PRUnichar* aMsg) {
    nsresult rv = NS_OK;
    nsString msg = aMsg;
    setAttribute( mWebShell, "data.status", "value", aMsg );
    return NS_OK;
}

// Utility function to close a window given a root nsIWebShell.
static void closeWindow( nsIWebShellWindow *aWebShellWindow ) {
    if ( aWebShellWindow ) {
        // crashes!
        aWebShellWindow->Close();
    }
}

NS_IMETHODIMP
nsFileDownloadDialog::OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg) {
    nsresult rv = NS_OK;
    // Close the output file.
    if ( mOutput ) {
        mOutput->close();
    }
    // Signal UI that download is complete.
    setAttribute( mWebShell, "data.progress", "completed", "true" );
    return rv;
}

// Handle attribute changing; we only care about the element "data.execute"
// which is used to signal command execution from the UI.
NS_IMETHODIMP
nsFileDownloadDialog::AttributeChanged( nsIDocument *aDocument,
                                        nsIContent*  aContent,
                                        nsIAtom*     aAttribute,
                                        PRInt32      aHint ) {
    nsresult rv = NS_OK;
    // Look for data.execute command changing.
    nsString id;
    nsCOMPtr<nsIAtom> atomId = nsDontQueryInterface<nsIAtom>( NS_NewAtom("id") );
    aContent->GetAttribute( kNameSpaceID_None, atomId, id );
    if ( id == "data.execute" ) {
        nsString cmd;
        nsCOMPtr<nsIAtom> atomCommand = nsDontQueryInterface<nsIAtom>( NS_NewAtom("command") );
        aContent->GetAttribute( kNameSpaceID_None, atomCommand, cmd );
        if ( cmd == "ok" ) {
            OnOK( aContent );
        } else if ( cmd == "start" ) {
            OnStart();
        } else if ( cmd == "stop" ) {
            OnStop();
        } else if ( cmd == "close" ) {
            OnClose();
        } else {
        }
        aContent->SetAttribute( kNameSpaceID_None, atomCommand, "", PR_FALSE );
    }

    return rv;
}

// OnOK
void
nsFileDownloadDialog::OnOK( nsIContent *aContent ) {
    // Show progress.
    if ( mWebShell ) {
        nsString fileName;
nsCOMPtr<nsIAtom> atomFileName = nsDontQueryInterface<nsIAtom>( NS_NewAtom("filename") );
        aContent->GetAttribute( kNameSpaceID_None, atomFileName, fileName );
        mFileName = fileName;
        mMode = kProgress;
        nsString progressXUL = "resource:/res/samples/downloadProgress.xul";
        mWebShell->LoadURL( progressXUL.GetUnicode() );
    }
    // Open output file stream.
    mOutput = new nsOutputFileStream( mFileName );
}

void
nsFileDownloadDialog::OnClose() {
    // Close the window.
    closeWindow( mWindow );
}

void
nsFileDownloadDialog::OnStart() {
    if ( mMode == kProgress ) {
        // Load source stream into file.
        nsINetService *inet = 0;
        nsresult rv = nsServiceManager::GetService( kNetServiceCID,
                                                    kINetServiceIID,
                                                    (nsISupports**)&inet );
        if (NS_OK == rv) {
            rv = inet->OpenStream(mUrl, this);
            nsServiceManager::ReleaseService(kNetServiceCID, inet);
        } else {
            if ( APP_DEBUG ) { printf( "Error getting Net Service, rv=0x%X\n", (int)rv ); }
        }
    }
}

void
nsFileDownloadDialog::OnStop() {
    // Stop the netlib xfer.
    mStopped = PR_TRUE;
}

void
nsFileDownloadDialog::SetWindow( nsIWebShellWindow *aWindow ) {
    mWindow = nsDontQueryInterface<nsIWebShellWindow>(aWindow);
}

NS_IMETHODIMP
nsBrowserAppCore::HandleUnknownContentType( nsIURL *aURL,
                                            const char *aContentType,
                                            const char *aCommand ) {
    nsresult rv = NS_OK;

    // Note: The following code is broken.  It should rightfully be loading
    // some "unknown content type handler" component and giving it control.
    // We will change this as soon as nsFileDownloadDialog is moved to a
    // separate component or components.

    // Get app shell service.
    nsIAppShellService *appShell;
    rv = nsServiceManager::GetService(kAppShellServiceCID,
                                      kIAppShellServiceIID,
                                      (nsISupports**)&appShell);

    if ( NS_SUCCEEDED(rv) ) {
        // Open "Save to disk" dialog.
        nsString controllerCID = "43147b80-8a39-11d2-9938-0080c7cb1081";
        nsIWebShellWindow *newWindow;

        // Make url for dialog xul.
        nsIURL *url;
        rv = NS_NewURL( &url, "resource:/res/samples/saveToDisk.xul" );

        if ( NS_SUCCEEDED(rv) ) {
            // Create "save to disk" nsIXULCallbacks...
            nsFileDownloadDialog *dialog = new nsFileDownloadDialog( aURL, aContentType );

            rv = appShell->CreateTopLevelWindow( nsnull,
                                                 url,
                                                 controllerCID,
                                                 newWindow,
                                                 nsnull,
                                                 dialog,
                                                 425,
                                                 200 );

            // Give find dialog the window pointer (if it worked).
            if ( NS_SUCCEEDED(rv) ) {
                dialog->SetWindow( newWindow );
            }

            NS_RELEASE(url);
        }
    }

    return rv;
}

NS_IMETHODIMP
nsBrowserAppCore::OnStartURLLoad(nsIURL* aURL, const char* aContentType, 
                            nsIContentViewer* aViewer)
{

   return NS_OK;
}

NS_IMETHODIMP
nsBrowserAppCore::OnProgressURLLoad(nsIURL* aURL, PRUint32 aProgress, 
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
nsBrowserAppCore::OnStatusURLLoad(nsIURL* aURL, nsString& aMsg)
{
  nsresult rv = setAttribute( mWebShell, "Browser:Status", "text", aMsg );
   return rv;
}


NS_IMETHODIMP
nsBrowserAppCore::OnEndURLLoad(nsIURL* aURL, PRInt32 aStatus)
{

   return NS_OK;
}


#if 0
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

  return rv;
}

NS_IMETHODIMP 
nsBrowserAppCore::EndLoadURL(nsIWebShell* aWebShell, const PRUnichar* aURL,
                             PRInt32 aStatus)
{
    // Update global history.
    NS_ASSERTION(mGHistory != nsnull, "history not initialized");
    if (mGHistory) {
        nsresult rv;

        nsAutoString url(aURL);
        char* urlSpec = url.ToNewCString();
        do {
            if (NS_FAILED(rv = mGHistory->AddPage(urlSpec, /* XXX referrer? */ nsnull, PR_Now()))) {
                NS_ERROR("unable to add page to history");
                break;
            }

            const PRUnichar* title;
            if (NS_FAILED(rv = aWebShell->GetTitle(&title))) {
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

    setAttribute( mWebShell, "Browser:Throbber", "busy", "false" );
    return NS_OK;
}
#endif  /* 0 */

NS_IMETHODIMP    
nsBrowserAppCore::NewWindow()
{  
  nsresult rv;
  nsString controllerCID;
 
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

  /*
   * XXX: Currently, the CID for the "controller" is passed in as an argument 
   *      to CreateTopLevelWindow(...).  Once XUL supports "controller" 
   *      components this will be specified in the XUL description...
   */
  controllerCID = "43147b80-8a39-11d2-9938-0080c7cb1081";
  appShell->CreateTopLevelWindow(nsnull, url, controllerCID, newWindow,
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
  nsComponentManager::CreateInstance(kCFileWidgetCID, nsnull, kIFileWidgetIID, (void**)&fileWidget);
  
  nsString titles[] = {"All Readable Files", "HTML Files",
                       "XML Files", "Image Files", "All Files"};
  nsString filters[] = {"*.htm; *.html; *.xml; *.gif; *.jpg; *.jpeg; *.png",
                        "*.htm; *.html",
                        "*.xml",
                        "*.gif; *.jpg; *.jpeg; *.png",
                        "*.*"};
  fileWidget->SetFilterList(5, titles, filters);

  fileWidget->Create(nsnull, title, eMode_load, nsnull, nsnull);

#if 0 // Old way
  nsAutoString fileURL;
  PRBool result = fileWidget->Show();
  if (result) {
    nsString fileName;
    nsString dirName;
    fileWidget->GetFile(fileName);

    BuildFileURL(nsAutoCString(fileName), fileURL);
  }
  printf("If I could open a new window with [%s] I would.\n", (const char *)nsAutoCString(fileURL));
#else  // New Way
  nsFileSpec fileSpec;
  if (fileWidget->GetFile(nsnull, title, fileSpec) == nsFileDlgResults_OK) {

  nsFileURL fileURL(fileSpec);
  char buffer[1024];
  const nsAutoCString cstr(fileURL.GetAsString());
  PR_snprintf( buffer, sizeof buffer, "OpenFile(\"%s\")", (const char*)cstr);
  ExecuteScript( mToolbarScriptContext, buffer );
  }
#endif
  NS_RELEASE(fileWidget);

  return NS_OK;
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
nsBrowserAppCore::InitializeSearch() {
    nsresult rv = NS_OK;

    // First, get find component (if we haven't done so already).
    if ( !mFindComponent ) {
        // Ultimately, should be something like appShell->GetComponent().
        rv = nsComponentManager::CreateInstance( NS_IFINDCOMPONENT_PROGID,
                                                 0,
                                                 nsIFindComponent::GetIID(),
                                                 (void**)&mFindComponent );
        if ( NS_SUCCEEDED( rv ) ) {
            // Initialize the find component.
            nsIAppShellService *appShell = 0;
            rv = nsServiceManager::GetService( kAppShellServiceCID,
                                               nsIAppShellService::GetIID(),
                                               (nsISupports**)&appShell );
            if ( NS_SUCCEEDED( rv ) ) {
                // Initialize the find component.
                mFindComponent->Initialize( appShell, 0 );
                // Release the app shell.
                nsServiceManager::ReleaseService( kAppShellServiceCID, appShell );
            } else {
                #ifdef NS_DEBUG
                printf( "nsBrowserAppCore::InitializeSearch failed, GetService rv=0x%X\n", (int)rv );
                #endif
                // Couldn't initialize it, so release it.
                mFindComponent->Release();
            }
        } else {
            #ifdef NS_DEBUG
            printf( "nsBrowserAppCore::InitializeSearch failed, CreateInstace rv=0x%X\n", (int)rv );
            #endif
        }
    }

    if ( NS_SUCCEEDED( rv ) && !mSearchContext ) {
        // Create the search context for this browser window.
        nsCOMPtr<nsIDocument> document = getDocument( mContentAreaWebShell );
        nsresult rv = mFindComponent->CreateContext( document, &mSearchContext );
        if ( NS_FAILED( rv ) ) {
            #ifdef NS_DEBUG
            printf( "%s %d CreateContext failed, rv=0x%X\n",
                    __FILE__, (int)__LINE__, (int)rv );
            #endif
        }
    }
}

void
nsBrowserAppCore::ResetSearchContext() {
    // Test if we've created the search context yet.
    if ( mFindComponent && mSearchContext ) {
        // OK, reset it.
        nsCOMPtr<nsIDocument> document = getDocument( mContentAreaWebShell );
        nsresult rv = mFindComponent->ResetContext( mSearchContext, document );
        if ( NS_FAILED( rv ) ) {
            #ifdef NS_DEBUG
            printf( "%s %d ResetContext failed, rv=0x%X\n",
                    __FILE__, (int)__LINE__, (int)rv );
            #endif
        }
    }
}

NS_IMETHODIMP    
nsBrowserAppCore::Find() {
    nsresult rv = NS_OK;

    // Make sure we've initialized searching for this document.
    InitializeSearch();

    // Perform find via find component.
    if ( mFindComponent && mSearchContext ) {
        rv = mFindComponent->Find( mSearchContext );
    }

    return rv;
}

NS_IMETHODIMP    
nsBrowserAppCore::FindNext() {
    nsresult rv = NS_OK;

    // Make sure we've initialized searching for this document.
    InitializeSearch();

    // Perform find next via find component.
    if ( mFindComponent && mSearchContext ) {
        rv = mFindComponent->FindNext( mSearchContext );
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
  /* (adapted from nsToolkitCore. No one's using this function, are they!?)
  */
  nsresult           rv;
  nsString           controllerCID;
  nsIAppShellService *appShell;
  nsIWebShellWindow  *window;

  window = nsnull;

  nsCOMPtr<nsIURL> urlObj;
  rv = NS_NewURL(getter_AddRefs(urlObj), "resource://res/samples/Password.html");
  if (NS_FAILED(rv))
    return rv;

  rv = nsServiceManager::GetService(kAppShellServiceCID, kIAppShellServiceIID,
                                    (nsISupports**) &appShell);
  if (NS_FAILED(rv))
    return rv;

  // hardwired temporary hack.  See nsAppRunner.cpp at main()
  controllerCID = "43147b80-8a39-11d2-9938-0080c7cb1081";

  appShell->CreateDialogWindow(mWebShellWin, urlObj, controllerCID, window,
                               nsnull, nsnull, 615, 480);
  nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);
//  window->Resize(300, 200, PR_TRUE); (until Resize gets moved into nsIWebShellWindow)

  if (window != nsnull)
    window->ShowModal();

  return rv;
}

#if 0
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
return NS_OK;
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

#endif /* 0 */
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


