/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *    Pierre Phaneuf <pp@ludusdesign.com>
 *    Travis Bogard <travis@netscape.com>
 */

// Local Includes
#include "nsBrowserInstance.h"

// Helper Includes

// Interfaces Needed
#include "nsIXULWindow.h"
#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsISHistory.h"
#include "nsIWebNavigation.h"
#include "nsIWebProgress.h"
#include "nsIXULBrowserWindow.h"
#include "nsPIDOMWindow.h"

///  Unsorted Includes

#include "nsIWebShell.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIClipboardCommands.h"
#include "pratom.h"
#include "prprf.h"
#include "nsIComponentManager.h"

#include "nsIFileSpecWithUI.h"

#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"

#include "nsIScriptGlobalObject.h"
#include "nsIWebShell.h"
#include "nsIDocShell.h"
#include "nsIWebShellWindow.h"
#include "nsIWebBrowserChrome.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"

#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIIOService.h"
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
#include "nsIUrlbarHistory.h"

#include "nsIDOMXULDocument.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"


#include "nsIPresContext.h"
#include "nsIPresShell.h"

#include "nsIDocumentLoader.h"
#include "nsIObserverService.h"
#include "nsFileLocations.h"

#include "nsIFileSpec.h"
#include "nsIWalletService.h"

#include "nsCURILoader.h"
#include "nsIContentHandler.h"
#include "nsNetUtil.h"
#include "nsICmdLineHandler.h"

#include "nsIWindowMediator.h"

#include "nsIDocumentCharsetInfo.h"
#include "nsICharsetConverterManager.h"
#include "nsICharsetConverterManager2.h"

#include "nsIDocShellHistory.h"

// Interface for "unknown content type handler" component/service.
#include "nsIUnkContentTypeHandler.h"

// Stuff to implement file download dialog.
#include "nsIDocumentObserver.h"
#include "nsIContent.h"
#include "nsIContentViewerFile.h"
#include "nsINameSpaceManager.h"
#include "nsFileStream.h"
#include "nsIProxyObjectManager.h" 

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
static NS_DEFINE_IID(kIWalletServiceIID, NS_IWALLETSERVICE_IID);
static NS_DEFINE_IID(kWalletServiceCID, NS_WALLETSERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_CID(kDocumentCharsetInfoCID, NS_DOCUMENTCHARSETINFO_CID);
static NS_DEFINE_CID(kCharsetConverterManagerCID, NS_ICHARSETCONVERTERMANAGER_CID);
static NS_DEFINE_IID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);

// Stuff to implement find/findnext
#include "nsIFindComponent.h"
#ifdef DEBUG_warren
#include "prlog.h"
#if defined(DEBUG) || defined(FORCE_PR_LOG)
static PRLogModuleInfo* gTimerLog = nsnull;
#endif /* DEBUG || FORCE_PR_LOG */
#endif

// if DEBUG or MOZ_PERF_METRICS are defined, enable the PageCycler
#ifdef DEBUG
#define ENABLE_PAGE_CYCLER
#endif
#ifdef MOZ_PERF_METRICS
#define ENABLE_PAGE_CYCLER
#endif

#include "nsTimeBomb.h"

/* Define Class IDs */
static NS_DEFINE_IID(kAppShellServiceCID,       NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kCmdLineServiceCID,        NS_COMMANDLINE_SERVICE_CID);
static NS_DEFINE_IID(kCGlobalHistoryCID,        NS_GLOBALHISTORY_CID);
static NS_DEFINE_CID(kCPrefServiceCID,          NS_PREF_CID);
static NS_DEFINE_CID(kTimeBombCID,     NS_TIMEBOMB_CID);
static NS_DEFINE_CID(kWindowMediatorCID, NS_WINDOWMEDIATOR_CID);


#ifdef DEBUG                                                           
static int APP_DEBUG = 0; // Set to 1 in debugger to turn on debugging.
#else                                                                  
#define APP_DEBUG 0                                                    
#endif                                                                 


#define PREF_HOMEPAGE_OVERRIDE_URL "startup.homepage_override_url"
#define PREF_HOMEPAGE_OVERRIDE "browser.startup.homepage_override.1"
#define PREF_BROWSER_STARTUP_PAGE "browser.startup.page"
#define PREF_BROWSER_STARTUP_HOMEPAGE "browser.startup.homepage"

static nsresult
FindNamedXULElement(nsIDocShell * aShell, const char *aId, nsCOMPtr<nsIDOMElement> * aResult );


//*****************************************************************************
//***    PageCycler: Object Management
//*****************************************************************************

#ifdef ENABLE_PAGE_CYCLER
#include "nsIProxyObjectManager.h"
#include "nsITimer.h"

static void TimesUp(nsITimer *aTimer, void *aClosure);
  // Timer callback: called when the timer fires

class PageCycler : public nsIObserver {
public:
  NS_DECL_ISUPPORTS

  PageCycler(nsBrowserInstance* appCore, const char *aTimeoutValue = nsnull)
    : mAppCore(appCore), mBuffer(nsnull), mCursor(nsnull), mTimeoutValue(0) { 
    NS_INIT_REFCNT();
    NS_ADDREF(mAppCore);
    if (aTimeoutValue){
      mTimeoutValue = atol(aTimeoutValue);
    }
  }

  virtual ~PageCycler() { 
    if (mBuffer) delete[] mBuffer;
    NS_RELEASE(mAppCore);
  }

  nsresult Init(const char* nativePath) {
    nsresult rv;
    mFile = nativePath;
    if (!mFile.IsFile())
      return mFile.Error();

    nsCOMPtr<nsISupports> in;
    rv = NS_NewTypicalInputFileStream(getter_AddRefs(in), mFile);
    if (NS_FAILED(rv)) return rv;
    nsCOMPtr<nsIInputStream> inStr = do_QueryInterface(in, &rv);
    if (NS_FAILED(rv)) return rv;
    PRUint32 avail;
    rv = inStr->Available(&avail);
    if (NS_FAILED(rv)) return rv;

    mBuffer = new char[avail + 1];
    if (mBuffer == nsnull)
      return NS_ERROR_OUT_OF_MEMORY;
    PRUint32 amt;
    rv = inStr->Read(mBuffer, avail, &amt);
    if (NS_FAILED(rv)) return rv;
    NS_ASSERTION(amt == avail, "Didn't get the whole file.");
    mBuffer[avail] = '\0';
    mCursor = mBuffer;

    NS_WITH_SERVICE(nsIObserverService, obsServ, NS_OBSERVERSERVICE_PROGID, &rv);
    if (NS_FAILED(rv)) return rv;
    nsString topic; topic.AssignWithConversion("EndDocumentLoad");
    rv = obsServ->AddObserver(this, topic.GetUnicode());
    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to add self to observer service");
    return rv; 
  }

  nsresult GetNextURL(nsString& result) {
    result.AssignWithConversion(mCursor);
    PRInt32 pos = result.Find(NS_LINEBREAK);
    if (pos > 0) {
      result.Truncate(pos);
      mLastRequest.Assign(result);
      mCursor += pos + NS_LINEBREAK_LEN;
      return NS_OK;
    }
    else if ( !result.IsEmpty() ) {
      // no more URLs after this one
      mCursor += result.Length(); // Advance cursor to terminating '\0'
      mLastRequest.Assign(result);
      return NS_OK;
    }
    else {
      // no more URLs, so quit the browser
      nsresult rv;
      // make sure our timer is stopped first
      StopTimer();
      NS_WITH_SERVICE(nsIAppShellService, appShellServ, kAppShellServiceCID, &rv);
      if(NS_FAILED(rv)) return rv;
      NS_WITH_SERVICE(nsIProxyObjectManager, pIProxyObjectManager, kProxyObjectManagerCID, &rv);
      if(NS_FAILED(rv)) return rv;
      nsCOMPtr<nsIAppShellService> appShellProxy;
      rv = pIProxyObjectManager->GetProxyForObject(NS_UI_THREAD_EVENTQ, NS_GET_IID(nsIAppShellService), 
                                                appShellServ, PROXY_ASYNC | PROXY_ALWAYS,
                                                getter_AddRefs(appShellProxy));

      (void)appShellProxy->Quit();
      return NS_ERROR_FAILURE;
    }
  }

  NS_IMETHOD Observe(nsISupports* aSubject, 
                     const PRUnichar* aTopic,
                     const PRUnichar* someData) {
    nsresult rv = NS_OK;
    nsString data(someData);
    if (data.Find(mLastRequest) == 0) {
      char* dataStr = data.ToNewCString();
      printf("########## PageCycler loaded: %s\n", dataStr);
      nsCRT::free(dataStr);

      nsAutoString url;
      rv = GetNextURL(url);
      if (NS_SUCCEEDED(rv)) {
        // stop previous timer
        StopTimer();
        if (aSubject == this){
          // if the aSubject arg is 'this' then we were called by the Timer
          // Stop the current load and move on to the next
          mAppCore->Stop();
        }
        // load the URL
        rv = mAppCore->LoadUrl(url.GetUnicode());
        // start new timer
        StartTimer();
      }
    }
    else {
      char* dataStr = data.ToNewCString();
      printf("########## PageCycler possible failure for: %s\n", dataStr);
      nsCRT::free(dataStr);
    }
    return rv;
  }

  const nsAutoString &GetLastRequest( void )
  { 
    return mLastRequest; 
  }

  // StartTimer: if mTimeoutValue is positive, then create a new timer
  //             that will call the callback fcn 'TimesUp' to keep us cycling
  //             through the URLs
  nsresult StartTimer(void)
  {
    nsresult rv=NS_OK;
    if (mTimeoutValue > 0){
      mShutdownTimer = do_CreateInstance("component://netscape/timer", &rv);
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create timer for PageCycler...");
      if (NS_SUCCEEDED(rv) && mShutdownTimer){
        mShutdownTimer->Init(TimesUp, (void *)this, mTimeoutValue*1000);
      }
    }
    return rv;
  }

  // StopTimer: if there is a timer, cancel it
  nsresult StopTimer(void)
  {
    if (mShutdownTimer){
      mShutdownTimer->Cancel();
    }
    return NS_OK;
  }

protected:
  nsBrowserInstance*     mAppCore;
  nsFileSpec            mFile;
  char*                 mBuffer;
  char*                 mCursor;
  nsAutoString          mLastRequest;
  nsCOMPtr<nsITimer>    mShutdownTimer;
  PRUint32              mTimeoutValue;
};

NS_IMPL_ADDREF(PageCycler)
NS_IMPL_RELEASE(PageCycler)

NS_INTERFACE_MAP_BEGIN(PageCycler)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIObserver)
NS_INTERFACE_MAP_END

// TimesUp: callback for the PageCycler timer: called when we have waited too long
// for the page to finish loading.
// 
// The aClosure argument is actually the PageCycler, 
// so use it to stop the timer and call the Observe fcn to move on to the next URL
// Note that we pass the PageCycler instance as the aSubject argument to the Observe
// function. This is our indication that the Observe method is being called after a
// timeout, allowing the PageCycler to do any necessary processing before moving on.
void TimesUp(nsITimer *aTimer, void *aClosure)
{
  if(aClosure){
    char urlBuf[64];
    PageCycler *pCycler = (PageCycler *)aClosure;
    pCycler->StopTimer();
    pCycler->GetLastRequest().ToCString( urlBuf, sizeof(urlBuf), 0 );
    fprintf(stderr,"########## PageCycler Timeout on URL: %s\n", urlBuf);
    pCycler->Observe( pCycler, nsnull, (pCycler->GetLastRequest()).GetUnicode() );
  }
}

#endif //ENABLE_PAGE_CYCLER

PRUint32 nsBrowserInstance::gRefCnt = 0;
int PR_CALLBACK ButtonShowHideCallback(const char* aPref, void* aClosure);
static const char kShowToolbarElts[] = "browser.toolbars.showbutton";

//*****************************************************************************
//***    nsBrowserInstance: Object Management
//*****************************************************************************

nsBrowserInstance::nsBrowserInstance() : mIsClosed(PR_FALSE)
{
  mContentWindowWeak    = nsnull;
  mContentScriptContext = nsnull;
  mWebShellWin          = nsnull;
  mDocShell             = nsnull;
  mDOMWindow            = nsnull;
  mContentAreaDocShellWeak  = nsnull;
  mContentAreaDocLoaderWeak = nsnull;
  NS_INIT_REFCNT();
  gRefCnt++;
  if (gRefCnt == 1) {
    nsresult rv;
    // Add callback listeners for toolbar buttons showing/hiding.
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_PROGID, &rv);
    if (NS_SUCCEEDED(rv)) {
      prefs->RegisterCallback(kShowToolbarElts, ButtonShowHideCallback, nsnull);
    }    
  }
}

nsBrowserInstance::~nsBrowserInstance()
{
  Close();
  gRefCnt--;
  if (gRefCnt == 0) {
    // Remove callback listeners for toolbar buttons showing/hiding.
    nsresult rv;
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_PROGID, &rv);
    if (NS_SUCCEEDED(rv)) {
      prefs->UnregisterCallback(kShowToolbarElts, ButtonShowHideCallback, nsnull);
    }    
  }
}

void
nsBrowserInstance::ReinitializeContentVariables()
{
  nsCOMPtr<nsIDOMWindow> content;
  mDOMWindow->Get_content(getter_AddRefs(content));
  SetContentWindow(content);
}

nsIDocShell* 
nsBrowserInstance::GetContentAreaDocShell()
{
  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mContentAreaDocShellWeak));
  if (docShell) {
    // the docshell still exists, but has it been destroyed?
    nsCOMPtr<nsIBaseWindow> hack = do_QueryInterface(docShell);
    if (hack) {
      nsCOMPtr<nsIWidget> parent;
      hack->GetParentWidget(getter_AddRefs(parent));
      if (!parent)
        // it's a zombie. a new one is in place. set up to use it.
        docShell = 0;
    }
  }
  if (!docShell)
    ReinitializeContentVariables();
  docShell = do_QueryReferent(mContentAreaDocShellWeak);
  return docShell.get();
}
    
nsIDOMWindow* 
nsBrowserInstance::GetContentWindow()
{
  nsCOMPtr<nsIDOMWindow> domWindow(do_QueryReferent(mContentWindowWeak));
  if (!domWindow)
    ReinitializeContentVariables();
  domWindow = do_QueryReferent(mContentWindowWeak);
  return domWindow.get();
}

nsIDocumentLoader* 
nsBrowserInstance::GetContentAreaDocLoader()
{
  nsCOMPtr<nsIDocumentLoader> docLoader(do_QueryReferent(mContentAreaDocLoaderWeak));
  if (!docLoader)
    ReinitializeContentVariables();
  docLoader = do_QueryReferent(mContentAreaDocLoaderWeak);
  return docLoader.get();
}

//*****************************************************************************
//    nsBrowserInstance: nsISupports
//*****************************************************************************

NS_IMPL_ADDREF(nsBrowserInstance)
NS_IMPL_RELEASE(nsBrowserInstance)

NS_INTERFACE_MAP_BEGIN(nsBrowserInstance)
   NS_INTERFACE_MAP_ENTRY(nsIBrowserInstance)
   NS_INTERFACE_MAP_ENTRY(nsIDocumentLoaderObserver)
   NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIURIContentListener)
NS_INTERFACE_MAP_END

//*****************************************************************************
//    nsBrowserInstance: nsIBrowserInstance
//*****************************************************************************

NS_IMETHODIMP    
nsBrowserInstance::Back()
{
#ifdef SH_IN_FRAMES
	nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(GetContentAreaDocShell()));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

	NS_ENSURE_TRUE(mSessionHistory, NS_ERROR_UNEXPECTED);
  webNav = do_QueryInterface(mSessionHistory);
  webNav->GoBack();
#else
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(GetContentAreaDocShell()));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

  webNav->GoBack();
#endif 
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserInstance::Forward()
{
#ifdef SH_IN_FRAMES
	nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(GetContentAreaDocShell()));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

	NS_ENSURE_TRUE(mSessionHistory, NS_ERROR_UNEXPECTED);
  webNav = do_QueryInterface(mSessionHistory);
  webNav->GoForward();
#else
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(GetContentAreaDocShell()));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

  webNav->GoForward();
#endif 
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserInstance::GetCanGoBack(PRBool* aCan)
{
#ifdef SH_IN_FRAMES
	nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(GetContentAreaDocShell()));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

	NS_ENSURE_TRUE(mSessionHistory, NS_ERROR_UNEXPECTED);
  webNav = do_QueryInterface(mSessionHistory);
  webNav->GetCanGoBack(aCan);
#else
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(GetContentAreaDocShell()));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

  webNav->GetCanGoBack(aCan);
#endif 
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserInstance::GetCanGoForward(PRBool* aCan)
{
#ifdef SH_IN_FRAMES
	nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(GetContentAreaDocShell()));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

	NS_ENSURE_TRUE(mSessionHistory, NS_ERROR_UNEXPECTED);
  webNav = do_QueryInterface(mSessionHistory);
  webNav->GetCanGoForward(aCan);
#else

  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(GetContentAreaDocShell()));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

  webNav->GetCanGoForward(aCan);
#endif 
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserInstance::Reload(nsLoadFlags flags)
{
#ifdef SH_IN_FRAMES
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(GetContentAreaDocShell()));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);

	NS_ENSURE_TRUE(mSessionHistory, NS_ERROR_UNEXPECTED);
  webNav = do_QueryInterface(mSessionHistory);
  webNav->Reload(flags);
#else
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(GetContentAreaDocShell()));
  NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);
  webNav->Reload(flags);
#endif
  return NS_OK;
}   

NS_IMETHODIMP    
nsBrowserInstance::Stop()
{
   nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(GetContentAreaDocShell()));
   if(webNav)
      webNav->Stop();
  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserInstance::LoadUrl(const PRUnichar * urlToLoad)
{
  nsresult rv = NS_OK;

  /* Ask nsWebShell to load the URl */
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(GetContentAreaDocShell()));
    
  // Normal browser.
  rv = webNav->LoadURI( urlToLoad );

  return rv;
}

NS_IMETHODIMP    
nsBrowserInstance::LoadInitialPage(void)
{
  static PRBool cmdLineURLUsed = PR_FALSE;
  char * urlstr = nsnull;
  nsresult rv;

  if (!cmdLineURLUsed) {
    NS_WITH_SERVICE(nsICmdLineService, cmdLineArgs, kCmdLineServiceCID, &rv);
    if (NS_FAILED(rv)) {
      if (APP_DEBUG) fprintf(stderr, "Could not obtain CmdLine processing service\n");
      return NS_ERROR_FAILURE;
    }

#ifdef ENABLE_PAGE_CYCLER
    // First, check if there's a URL file to load (for testing), and if there 
    // is, process it instead of anything else.
    nsXPIDLCString file;
    rv = cmdLineArgs->GetCmdLineValue("-f", getter_Copies(file));
    if (NS_SUCCEEDED(rv) && (const char*)file) {
      // see if we have a timeout value corresponding to the url-file
      nsXPIDLCString timeoutVal;
      rv = cmdLineArgs->GetCmdLineValue("-ftimeout", getter_Copies(timeoutVal));
      // cereate the cool PageCycler instance
      PageCycler* bb = new PageCycler(this, timeoutVal);
      if (bb == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

      NS_ADDREF(bb);
      rv = bb->Init(file);
      if (NS_FAILED(rv)) return rv;

      nsAutoString str;
      rv = bb->GetNextURL(str);
      if (NS_SUCCEEDED(rv)) {
        urlstr = str.ToNewCString();
      }
      NS_RELEASE(bb);
    }
    else 
#endif //ENABLE_PAGE_CYCLER
    {
      // Examine content URL.
      if ( GetContentAreaDocShell() ) {
        nsCOMPtr<nsIURI> uri;
        rv = GetContentAreaDocShell()->GetCurrentURI(getter_AddRefs(uri));
        nsXPIDLCString spec;
        if (uri)
          rv = uri->GetSpec(getter_Copies(spec));
        /* Check whether url is valid. Otherwise we compare with 
           * "about:blank" and there by return from here with out 
           * loading the command line url or default home page.
           */
        if (NS_SUCCEEDED(rv) && nsCRT::strcasecmp(spec, "about:blank") != 0) {
            // Something has already been loaded (probably via window.open),
            // leave it be.
            return NS_OK;
        }
      }

      // Get the URL to load
      rv = cmdLineArgs->GetURLToLoad(&urlstr);
    }

    if (urlstr != nsnull) {
      // A url was provided. Load it
      if (APP_DEBUG) printf("Got Command line URL to load %s\n", urlstr);
      nsString url; url.AssignWithConversion( urlstr );
      nsMemory::Free(urlstr); urlstr = nsnull;
      rv = LoadUrl( url.GetUnicode() );
      cmdLineURLUsed = PR_TRUE;
      return rv;
    }
  }

  // No URL was provided in the command line. Load the default provided
  // in the navigator.xul;

  // but first, abort if the window doesn't want a default page loaded
  if (mWebShellWin) {
    PRBool loadDefault;
    mWebShellWin->ShouldLoadDefaultPage(&loadDefault);
    if (!loadDefault)
      return NS_OK;
  }

  nsCOMPtr<nsIDOMElement>    argsElement;

  rv = FindNamedXULElement(mDocShell, "args", &argsElement);
  if (!argsElement) {
    // Couldn't get the "args" element from the xul file. Load a blank page
    if (APP_DEBUG) printf("Couldn't find args element\n");
    nsAutoString url; url.AssignWithConversion("about:blank");
    rv = LoadUrl( url.GetUnicode() );
    return rv;
  }

  // Load the default page mentioned in the xul file.
  nsString value;
  argsElement->GetAttribute(NS_ConvertASCIItoUCS2("value"), value);
  if (value.Length()) {
    rv = LoadUrl(value.GetUnicode());
    return rv;
  }

  if (APP_DEBUG) printf("Quitting LoadInitialPage\n");
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserInstance::BackButtonPopup(nsIDOMNode * aParent)
{

  if (!GetContentAreaDocShell())  {
    printf("nsBrowserInstance::BackButtonPopup Couldn't get a handle to SessionHistory\n");
    return NS_ERROR_FAILURE;
  }
  
  PRBool hasChildren=PR_FALSE;
  nsresult rv;

  // Check if menu has children. If so, remove them.
  rv = aParent->HasChildNodes(&hasChildren);
  if (NS_SUCCEEDED(rv) && hasChildren) {
    rv = ClearHistoryMenus(aParent);
    if (!NS_SUCCEEDED(rv))
      printf("nsBrowserInstance::BackButtonPopup ERROR While removing old history menu items\n");
  }    // hasChildren
  else {
   if (APP_DEBUG) printf("nsBrowserInstance::BackButtonPopup Menu has no children\n");
  }             

  nsCOMPtr<nsISHistory> mSHistory;
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(GetContentAreaDocShell()));
  webNav->GetSessionHistory(getter_AddRefs(mSHistory));
  NS_ENSURE_TRUE(mSHistory, NS_ERROR_FAILURE);

  PRInt32 length=0,i=0, indix = 0;
  //Get total length of the  Session History
  mSHistory->GetCount(&length);
  mSHistory->GetIndex(&indix);

  //Decide on the # of items in the popup list 
  if (indix > SHISTORY_POPUP_LIST)
     i  = indix-SHISTORY_POPUP_LIST;

  for (PRInt32 j=indix-1;j>=i;j--) {
      PRUnichar  *title=nsnull;
      nsCOMPtr<nsIURI> uri;
      nsXPIDLCString   uriSpec;

	  nsCOMPtr<nsISHEntry> shEntry;
	  mSHistory->GetEntryAtIndex(j, PR_FALSE, getter_AddRefs(shEntry));
      NS_ENSURE_TRUE(shEntry, NS_ERROR_FAILURE);

      shEntry->GetURI(getter_AddRefs(uri));
	  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);
	  uri->GetSpec(getter_Copies(uriSpec));

      shEntry->GetTitle(&title);	 
      nsAutoString  histTitle(title);
      if (APP_DEBUG)printf("nsBrowserInstance::BackButtonPopup URL = %s, TITLE = %s\n", (const char *) uriSpec, histTitle.ToNewCString());
      rv = CreateMenuItem(aParent, j, title);
      if (!NS_SUCCEEDED(rv)) {
        printf("nsBrowserInstance::BackButtonPopup Error while creating history mene item\n");
	  }  
   Recycle(title);
  }  //for
  return NS_OK;

}   //BackButtonpopup


NS_IMETHODIMP
nsBrowserInstance::ForwardButtonPopup(nsIDOMNode * aParent)
{

  if (!GetContentAreaDocShell())  {
    printf("nsBrowserInstance::ForwardButtonPopup Couldn't get a handle to SessionHistory\n");
    return NS_ERROR_FAILURE;
  }

  PRBool hasChildren=PR_FALSE;
  nsresult rv;

  // Check if menu has children. If so, remove them.
  aParent->HasChildNodes(&hasChildren);
  if (hasChildren) {
    // Remove all old entries 
    rv = ClearHistoryMenus(aParent);
    if (!NS_SUCCEEDED(rv)) {
      printf("nsBrowserInstance::ForwardMenuPopup Error while clearing old history entries\n");
    }
  }    // hasChildren
  else {
    if (APP_DEBUG) printf("nsBrowserInstance::ForwardButtonPopup Menu has no children\n");
  }  

  nsCOMPtr<nsISHistory> mSHistory;
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(GetContentAreaDocShell()));
  webNav->GetSessionHistory(getter_AddRefs(mSHistory));
  NS_ENSURE_TRUE(mSHistory, NS_ERROR_FAILURE);

  PRInt32 length=0,i=0, indix = 0;
  //Get total length of the  Session History
  mSHistory->GetCount(&length);
  mSHistory->GetIndex(&indix);

  //Decide on the # of items in the popup list 
  if ((length-indix) > SHISTORY_POPUP_LIST)
     i  = indix+SHISTORY_POPUP_LIST;
  else
   i = length;

  for (PRInt32 j=indix+1;j<i;j++) {
      PRUnichar  *title=nsnull;
      nsCOMPtr<nsIURI> uri;
      nsXPIDLCString   uriSpec;

	  nsCOMPtr<nsISHEntry> shEntry;
	  mSHistory->GetEntryAtIndex(j, PR_FALSE, getter_AddRefs(shEntry));
      NS_ENSURE_TRUE(shEntry, NS_ERROR_FAILURE);

      shEntry->GetURI(getter_AddRefs(uri));
	  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);
	  uri->GetSpec(getter_Copies(uriSpec));

      shEntry->GetTitle(&title);	 
      nsAutoString  histTitle(title);
      if (APP_DEBUG)printf("nsBrowserInstance::ForwardButtonPopup URL = %s, TITLE = %s\n", (const char *) uriSpec, histTitle.ToNewCString());
      rv = CreateMenuItem(aParent, j, title);
      if (!NS_SUCCEEDED(rv)) {
        printf("nsBrowserInstance::ForwardButtonPopup Error while creating history mene item\n");
	  }  
   Recycle(title);
  }

   return NS_OK;

}

NS_IMETHODIMP
nsBrowserInstance::GotoHistoryIndex(PRInt32 aIndex)
{
#ifdef SH_IN_FRAMES
	  NS_ENSURE_TRUE(mSessionHistory, NS_ERROR_UNEXPECTED);
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mSessionHistory));
  webNav->GotoIndex(aIndex);
#else

   nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(GetContentAreaDocShell()));
   webShell->GoTo(aIndex);
#endif
  return NS_OK;

}

NS_IMETHODIMP    
nsBrowserInstance::WalletPreview(nsIDOMWindow* aWin, nsIDOMWindow* aForm, PRBool* status)
{
  NS_PRECONDITION(aForm != nsnull, "null ptr");
  if (! aForm)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject;
  scriptGlobalObject = do_QueryInterface(aForm); 
  nsCOMPtr<nsIDocShell> docShell; 
  scriptGlobalObject->GetDocShell(getter_AddRefs(docShell)); 

  nsCOMPtr<nsIPresShell> presShell;
  if(docShell)
   docShell->GetPresShell(getter_AddRefs(presShell));
  nsIWalletService *walletservice;
  nsresult res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if (NS_SUCCEEDED(res) && (nsnull != walletservice)) {
    res = walletservice->WALLET_Prefill(presShell, PR_FALSE, aForm, status);
    nsServiceManager::ReleaseService(kWalletServiceCID, walletservice);
    if (NS_FAILED(res)) { /* this just means that there was nothing to prefill */
      return NS_OK;
    }
  } else {
    return res;
  }

   nsCOMPtr<nsIScriptGlobalObject> sgo(do_QueryInterface(aWin));
   NS_ENSURE_TRUE(sgo, NS_ERROR_FAILURE);

   nsCOMPtr<nsIScriptContext> scriptContext;
   sgo->GetContext(getter_AddRefs(scriptContext));
   NS_ENSURE_TRUE(scriptContext, NS_ERROR_FAILURE);

   JSContext* jsContext = (JSContext*)scriptContext->GetNativeContext();
   NS_ENSURE_TRUE(jsContext, NS_ERROR_FAILURE);

   void* mark;
   jsval* argv;

   argv = JS_PushArguments(jsContext, &mark, "sss", "chrome://communicator/content/wallet/WalletPreview.xul", "_blank", 
                  //"chrome,dialog=yes,modal=yes,all");
                  "chrome,modal=yes,dialog=yes,all,width=504,height=436");
   NS_ENSURE_TRUE(argv, NS_ERROR_FAILURE);

   nsCOMPtr<nsIDOMWindow> newWindow;
   aWin->OpenDialog(jsContext, argv, 3, getter_AddRefs(newWindow));
   JS_PopArguments(jsContext, mark);

   return NS_OK;
}

NS_IMETHODIMP    
nsBrowserInstance::WalletChangePassword(PRBool* status)
{
  nsIWalletService *walletservice;
  nsresult res;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->WALLET_ChangePassword(status);
    nsServiceManager::ReleaseService(kWalletServiceCID, walletservice);
  }
  return NS_OK;
}

#include "nsIDOMHTMLDocument.h"
static NS_DEFINE_IID(kIDOMHTMLDocumentIID, NS_IDOMHTMLDOCUMENT_IID);
NS_IMETHODIMP    
nsBrowserInstance::WalletQuickFillin(nsIDOMWindow* aWin, PRBool* status)
{
  NS_PRECONDITION(aWin != nsnull, "null ptr");
  if (! aWin)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject; 
  scriptGlobalObject = do_QueryInterface(aWin);
  nsCOMPtr<nsIDocShell> docShell; 
  scriptGlobalObject->GetDocShell(getter_AddRefs(docShell)); 

  nsCOMPtr<nsIPresShell> presShell;
  if(docShell)
   docShell->GetPresShell(getter_AddRefs(presShell));
  nsIWalletService *walletservice;
  nsresult res;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->WALLET_Prefill(presShell, PR_TRUE, aWin, status);
    nsServiceManager::ReleaseService(kWalletServiceCID, walletservice);
    return NS_OK;
  } else {
    return res;
  }
}

NS_IMETHODIMP    
nsBrowserInstance::WalletRequestToCapture(nsIDOMWindow* aWin, PRUint32* status)
{
  NS_PRECONDITION(aWin != nsnull, "null ptr");
  if (! aWin)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject; 
  scriptGlobalObject = do_QueryInterface(aWin);
  nsCOMPtr<nsIDocShell> docShell; 
  scriptGlobalObject->GetDocShell(getter_AddRefs(docShell)); 

  nsCOMPtr<nsIPresShell> presShell;
  if(docShell) 
   docShell->GetPresShell(getter_AddRefs(presShell));
  nsIWalletService *walletservice;
  nsresult res;
  res = nsServiceManager::GetService(kWalletServiceCID,
                                     kIWalletServiceIID,
                                     (nsISupports **)&walletservice);
  if ((NS_OK == res) && (nsnull != walletservice)) {
    res = walletservice->WALLET_RequestToCapture(presShell, aWin, status);
    nsServiceManager::ReleaseService(kWalletServiceCID, walletservice);
    return NS_OK;
  } else {
    return res;
  }
}

NS_IMETHODIMP    
nsBrowserInstance::Init()
{
  nsresult rv = NS_OK;

  // register ourselves as a content listener with the uri dispatcher service
  rv = NS_OK;
  NS_WITH_SERVICE(nsIURILoader, dispatcher, NS_URI_LOADER_PROGID, &rv);
  if (NS_SUCCEEDED(rv)) 
    rv = dispatcher->RegisterContentListener(this);


  return rv;
}

NS_IMETHODIMP    
nsBrowserInstance::SetContentWindow(nsIDOMWindow* aWin)
{
  NS_PRECONDITION(aWin != nsnull, "null ptr");
  if (! aWin)
    return NS_ERROR_NULL_POINTER;

  mContentWindowWeak = getter_AddRefs(NS_GetWeakReference(aWin));

  // NS_ADDREF(aWin); WE DO NOT OWN THIS
  nsresult rv;

  // we do not own the script context, so don't addref it
  nsCOMPtr<nsIScriptGlobalObject> globalObj(do_QueryInterface(aWin));
  if(!globalObj)
   return NS_ERROR_FAILURE;

  nsCOMPtr<nsIScriptContext>  scriptContext;
  globalObj->GetContext(getter_AddRefs(scriptContext));
  mContentScriptContext = scriptContext;

  nsCOMPtr<nsIDocShell> docShell;
  globalObj->GetDocShell(getter_AddRefs(docShell));
  nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(docShell));
  if (webShell) {
    mContentAreaDocShellWeak = getter_AddRefs(NS_GetWeakReference(docShell)); // Weak reference
    docShell->SetDocLoaderObserver((nsIDocumentLoaderObserver *)this);

  nsCOMPtr<nsIWebProgress> webProgress(do_GetInterface(docShell));
  webProgress->AddProgressListener(NS_STATIC_CAST(nsIWebProgressListener*, this));
  nsCOMPtr<nsISHistory> sessionHistory(do_CreateInstance(NS_SHISTORY_PROGID));

  mSessionHistory = sessionHistory;
  if (!mSessionHistory) {
	  printf("#### Error initialising Session History ####\n");
	  return NS_ERROR_FAILURE;
  }
  mSessionHistory->SetRootDocShell(docShell);

  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(docShell));
  if (webNav)
     webNav->SetSessionHistory(sessionHistory);

  nsCOMPtr<nsIDocShellHistory> dsHistory(do_QueryInterface(GetContentAreaDocShell()));
  nsCOMPtr<nsIGlobalHistory> history(do_GetService(kCGlobalHistoryCID));
  if (dsHistory)
 	  dsHistory->SetGlobalHistory(history);

    // Cache the Document Loader for the content area webshell.  This is a 
    // weak reference that is *not* reference counted...
    nsCOMPtr<nsIDocumentLoader> docLoader;

    webShell->GetDocumentLoader(*getter_AddRefs(docLoader));
    mContentAreaDocLoaderWeak = getter_AddRefs(NS_GetWeakReference(docLoader.get()));

    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(docShell));
    nsXPIDLString name;
    docShellAsItem->GetName(getter_Copies(name));
    nsCAutoString str;
    str.AssignWithConversion(name);

    if (APP_DEBUG) {
      printf("Attaching to Content WebShell [%s]\n", (const char *)str);
    }
    nsCOMPtr<nsIUrlbarHistory>  ubHistory = do_GetService(NS_URLBARHISTORY_PROGID, &rv);
	
	NS_ENSURE_TRUE(ubHistory, NS_ERROR_FAILURE);
	mUrlbarHistory = ubHistory;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBrowserInstance::GetContentDocShell(nsIDocShell** aDocShell)
{
  NS_ENSURE_ARG_POINTER(aDocShell);

  *aDocShell = GetContentAreaDocShell();
  NS_IF_ADDREF(*aDocShell); // (GetContentAreaDocShell doesn't)
  return NS_OK;
}


NS_IMETHODIMP
nsBrowserInstance::SetUrlbarHistory(nsIUrlbarHistory* aUBHistory)
{
   mUrlbarHistory = aUBHistory;
   return NS_OK;
}
	

NS_IMETHODIMP
nsBrowserInstance::GetUrlbarHistory(nsIUrlbarHistory** aUrlbarHistory)
{
   NS_ENSURE_ARG_POINTER(aUrlbarHistory);

   *aUrlbarHistory = mUrlbarHistory;
   NS_IF_ADDREF(*aUrlbarHistory);
   return NS_OK;
}

NS_IMETHODIMP    
nsBrowserInstance::SetWebShellWindow(nsIDOMWindow* aWin)
{
   NS_ENSURE_ARG(aWin);
   mDOMWindow = aWin;

  nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(aWin) );
  if (!globalObj) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShell> docShell;
  globalObj->GetDocShell(getter_AddRefs(docShell));
  if (docShell) {
    mDocShell = docShell.get();
    //NS_ADDREF(mDocShell); WE DO NOT OWN THIS
    // inform our top level webshell that we are its parent URI content listener...
    docShell->SetParentURIContentListener(this);

    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(docShell));
    nsXPIDLString name;
    docShellAsItem->GetName(getter_Copies(name));
    nsCAutoString str;
    str.AssignWithConversion(name);

    if (APP_DEBUG) {
      printf("Attaching to WebShellWindow[%s]\n", (const char *)str);
    }

    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(docShell));
    nsCOMPtr<nsIWebShellContainer> webShellContainer;
    webShell->GetContainer(*getter_AddRefs(webShellContainer));
    if (nsnull != webShellContainer)
    {
      nsCOMPtr<nsIWebShellWindow> webShellWin;
      if (NS_OK == webShellContainer->QueryInterface(NS_GET_IID(nsIWebShellWindow), getter_AddRefs(webShellWin)))
      {
        mWebShellWin = webShellWin;   // WE DO NOT OWN THIS
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserInstance::SetTextZoom(float aTextZoom)
{
  nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(GetContentWindow()) );
  if (!globalObj) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShell> docShell;
  globalObj->GetDocShell(getter_AddRefs(docShell));
  if (docShell) 
  {
    nsCOMPtr<nsIContentViewer> childCV;
    NS_ENSURE_SUCCESS(docShell->GetContentViewer(getter_AddRefs(childCV)), NS_ERROR_FAILURE);
    if (childCV) 
    {
      nsCOMPtr<nsIMarkupDocumentViewer> markupCV = do_QueryInterface(childCV);
      if (markupCV) {
        NS_ENSURE_SUCCESS(markupCV->SetTextZoom(aTextZoom), NS_ERROR_FAILURE);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserInstance::GetDocumentCharset(PRUnichar **aCharset)
{
  nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(GetContentWindow()) );

  if (!globalObj) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShell> docShell;
  globalObj->GetDocShell(getter_AddRefs(docShell));
  if (docShell) 
  {
    nsCOMPtr<nsIContentViewer> childCV;
    NS_ENSURE_SUCCESS(docShell->GetContentViewer(getter_AddRefs(childCV)), NS_ERROR_FAILURE);
    if (childCV) 
    {
      nsCOMPtr<nsIMarkupDocumentViewer> markupCV = do_QueryInterface(childCV);
      if (markupCV) {
        // This allocates a new buffer
        NS_ENSURE_SUCCESS(markupCV->GetDefaultCharacterSet(aCharset), NS_ERROR_FAILURE);
      }
    }
  }
  
  if (!*aCharset) {
    nsAutoString blank;
    *aCharset = blank.ToNewUnicode();
  }
  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserInstance::SetDocumentCharset(const PRUnichar *aCharset)
{
  nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(GetContentWindow()) );
  if (!globalObj) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShell> docShell;
  globalObj->GetDocShell(getter_AddRefs(docShell));
  if (docShell) 
  {
    nsCOMPtr<nsIContentViewer> childCV;
    NS_ENSURE_SUCCESS(docShell->GetContentViewer(getter_AddRefs(childCV)), NS_ERROR_FAILURE);
    if (childCV) 
    {
      nsCOMPtr<nsIMarkupDocumentViewer> markupCV = do_QueryInterface(childCV);
      if (markupCV) {
        NS_ENSURE_SUCCESS(markupCV->SetDefaultCharacterSet(aCharset), NS_ERROR_FAILURE);
      }
    }
  }
  return NS_OK;
}

// XXX isolate the common code in the next two methods into a common method

NS_IMETHODIMP    
nsBrowserInstance::SetForcedCharset(const PRUnichar * aCharset)
{
  nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(GetContentWindow()) );
  if (!globalObj) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShell> docShell;
  globalObj->GetDocShell(getter_AddRefs(docShell));
  if (!docShell) return NS_OK;

  nsresult res = NS_OK;
  nsCOMPtr<nsIDocumentCharsetInfo> dcInfo = NULL;

  res = docShell->GetDocumentCharsetInfo(getter_AddRefs(dcInfo));
  if (dcInfo.get() == NULL) {
    res = nsComponentManager::CreateInstance(kDocumentCharsetInfoCID, NULL,
      NS_GET_IID(nsIDocumentCharsetInfo), getter_AddRefs(dcInfo));
    if (NS_FAILED(res)) return NS_ERROR_FAILURE;
    res = docShell->SetDocumentCharsetInfo(dcInfo);
    if (NS_FAILED(res)) return NS_ERROR_FAILURE;
  }

  NS_WITH_SERVICE(nsICharsetConverterManager2, ccMan, kCharsetConverterManagerCID, &res);
  if (NS_FAILED(res)) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIAtom> csAtom;
  res = ccMan->GetCharsetAtom(aCharset, getter_AddRefs(csAtom));
  if (NS_FAILED(res)) return NS_ERROR_FAILURE;

  res = dcInfo->SetForcedCharset(csAtom);
  if (NS_FAILED(res)) return NS_ERROR_FAILURE;

  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserInstance::SetForcedDetector()
{
  nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(GetContentWindow()) );
  if (!globalObj) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShell> docShell;
  globalObj->GetDocShell(getter_AddRefs(docShell));
  if (!docShell) return NS_OK;

  nsresult res = NS_OK;
  nsCOMPtr<nsIDocumentCharsetInfo> dcInfo = NULL;

  res = docShell->GetDocumentCharsetInfo(getter_AddRefs(dcInfo));
  if (dcInfo.get() == NULL) {
    res = nsComponentManager::CreateInstance(kDocumentCharsetInfoCID, NULL,
      NS_GET_IID(nsIDocumentCharsetInfo), getter_AddRefs(dcInfo));
    if (NS_FAILED(res)) return NS_ERROR_FAILURE;
    res = docShell->SetDocumentCharsetInfo(dcInfo);
    if (NS_FAILED(res)) return NS_ERROR_FAILURE;
  }

  res = dcInfo->SetForcedDetector(PR_TRUE);
  if (NS_FAILED(res)) return NS_ERROR_FAILURE;

  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserInstance::PrintPreview()
{ 

  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserInstance::Print()
{  
  if (GetContentAreaDocShell()) {
    nsCOMPtr<nsIContentViewer> viewer;    
    GetContentAreaDocShell()->GetContentViewer(getter_AddRefs(viewer));    
    if (nsnull != viewer) {
      nsCOMPtr<nsIContentViewerFile> viewerFile = do_QueryInterface(viewer);
      if (viewerFile) {
        NS_ENSURE_SUCCESS(viewerFile->Print(PR_FALSE,nsnull), NS_ERROR_FAILURE);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserInstance::Close()
{ 
  // if we have already been closed....then just return
  if (mIsClosed) 
    return NS_OK;
  else
    mIsClosed = PR_TRUE;

  // Undo other stuff we did in SetContentWindow.
  // Remove listeners we may have registered
  nsIDocShell* docShell = GetContentAreaDocShell();
  if (docShell)
  {
    docShell->SetDocLoaderObserver( 0 );
    nsCOMPtr<nsIWebProgress> webProgress(do_GetInterface(docShell));
    webProgress->RemoveProgressListener(NS_STATIC_CAST(nsIWebProgressListener*, this));
  }

  // Release search context.
  mSearchContext = null_nsCOMPtr();;

  //Release Urlbar History
  mUrlbarHistory = null_nsCOMPtr();

  // unregister ourselves with the uri loader because
  // we can no longer accept new content!
  nsresult rv = NS_OK;
  NS_WITH_SERVICE(nsIURILoader, dispatcher, NS_URI_LOADER_PROGID, &rv);
  if (NS_SUCCEEDED(rv)) 
    rv = dispatcher->UnRegisterContentListener(this);

  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserInstance::Copy()
{ 
   nsCOMPtr<nsIPresShell> presShell;
   GetContentAreaDocShell()->GetPresShell(getter_AddRefs(presShell));
  if (presShell) {
    presShell->DoCopy();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBrowserInstance::SelectAll()
{
  nsresult rv;
  nsCOMPtr<nsIClipboardCommands> clip(do_QueryInterface(GetContentAreaDocShell(),&rv));
  if ( NS_SUCCEEDED(rv) ) {
      rv = clip->SelectAll();
  }

  return rv;
}

NS_IMETHODIMP    
nsBrowserInstance::Find()
{
    nsresult rv = NS_OK;
    PRBool   found = PR_FALSE;
    
    // Get find component.
    nsCOMPtr <nsIFindComponent> finder = do_GetService(NS_IFINDCOMPONENT_PROGID, &rv);
    if (NS_FAILED(rv)) return rv;
    if (!finder) return NS_ERROR_FAILURE;

    // Make sure we've initialized searching for this document.
    rv = InitializeSearch( finder );
    if (NS_FAILED(rv)) return rv;

    // Perform find via find component.
    if (mSearchContext) {
        rv = finder->Find( mSearchContext, &found );
    }

    return rv;
}

NS_IMETHODIMP    
nsBrowserInstance::FindNext()
{
    nsresult rv = NS_OK;
    PRBool   found = PR_FALSE;

    // Get find component.
    nsCOMPtr <nsIFindComponent> finder = do_GetService(NS_IFINDCOMPONENT_PROGID, &rv);
    if (NS_FAILED(rv)) return rv;
    if (!finder) return NS_ERROR_FAILURE;

    // Make sure we've initialized searching for this document.
    rv = InitializeSearch( finder );
    if (NS_FAILED(rv)) return rv;

    // Perform find via find component.
    if (mSearchContext) {
            rv = finder->FindNext( mSearchContext, &found );
    }

    return rv;
}

//*****************************************************************************
//    nsBrowserInstance: nsIDocumentLoaderObserver
//*****************************************************************************

NS_IMETHODIMP
nsBrowserInstance::OnStartDocumentLoad(nsIDocumentLoader* aLoader, nsIURI* aURL, const char* aCommand)
{
   NS_ENSURE_ARG(aLoader);
   NS_ENSURE_ARG(aURL);

  nsresult rv;

  // Notify observers that a document load has started in the
  // content window.
  NS_WITH_SERVICE(nsIObserverService, observer, NS_OBSERVERSERVICE_PROGID, &rv);
  if (NS_FAILED(rv)) return rv;

  char* url;
  rv = aURL->GetSpec(&url);
  if (NS_FAILED(rv)) return rv;

  nsAutoString urlStr; urlStr.AssignWithConversion(url);

#ifdef DEBUG_warren
  char* urls;
  aURL->GetSpec(&urls);
  if (gTimerLog == nsnull)
    gTimerLog = PR_NewLogModule("Timer");
  mLoadStartTime = PR_IntervalNow();
  PR_LOG(gTimerLog, PR_LOG_DEBUG, 
         (">>>>> Starting timer for %s\n", urls));
  printf(">>>>> Starting timer for %s\n", urls);
  nsCRT::free(urls);
#endif

  // Check if this notification is for a frame
  PRBool isFrame=PR_FALSE;
  nsCOMPtr<nsISupports> container;
  aLoader->GetContainer(getter_AddRefs(container));
  if (container) {
     nsCOMPtr<nsIDocShellTreeItem>   docShellAsItem(do_QueryInterface(container));
   if (docShellAsItem) {
       nsCOMPtr<nsIDocShellTreeItem> parent;
     docShellAsItem->GetSameTypeParent(getter_AddRefs(parent));
     if (parent) 
       isFrame = PR_TRUE;
     }
  }

  if (!isFrame) {
    nsAutoString kStartDocumentLoad; kStartDocumentLoad.AssignWithConversion("StartDocumentLoad");
    rv = observer->Notify(GetContentWindow(),
                        kStartDocumentLoad.GetUnicode(),
                        urlStr.GetUnicode());

    // XXX Ignore rv for now. They are using nsIEnumerator instead of
    // nsISimpleEnumerator.
  }

  nsCRT::free(url);

  return NS_OK;
}


NS_IMETHODIMP
nsBrowserInstance::OnEndDocumentLoad(nsIDocumentLoader* aLoader, nsIChannel* channel, nsresult aStatus)
{
   NS_ENSURE_ARG(aLoader);
   NS_ENSURE_ARG(channel);

  nsresult rv;

  nsCOMPtr<nsIURI> aUrl;
  rv = channel->GetOriginalURI(getter_AddRefs(aUrl));
  if (NS_FAILED(rv)) return rv;

  nsXPIDLCString url;
  rv = aUrl->GetSpec(getter_Copies(url));
  if (NS_FAILED(rv)) return rv;

  PRBool  isFrame=PR_FALSE;
  nsCOMPtr<nsISupports> container;
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem, parent;

  aLoader->GetContainer(getter_AddRefs(container));
  // Is this a frame ?
  docShellAsItem = do_QueryInterface(container);
  if (docShellAsItem) {
    docShellAsItem->GetSameTypeParent(getter_AddRefs(parent));
  }
  if (parent)
  isFrame = PR_TRUE;


  if (GetContentAreaDocLoader()) {
    PRBool isBusy = PR_FALSE;

    GetContentAreaDocLoader()->IsBusy(&isBusy);
    if (isBusy) {
      return NS_OK;
    }
  }

  /* If this is a frame, don't do any of the Global History
   * & observer thingy 
   */
  if (!isFrame) {
      nsAutoString urlStr; urlStr.AssignWithConversion(url);
      nsAutoString kEndDocumentLoad; kEndDocumentLoad.AssignWithConversion("EndDocumentLoad");
      nsAutoString kFailDocumentLoad; kFailDocumentLoad.AssignWithConversion("FailDocumentLoad");

      // Notify observers that a document load has started in the
      // content window.
      NS_WITH_SERVICE(nsIObserverService, observer, NS_OBSERVERSERVICE_PROGID, &rv);
      if (NS_FAILED(rv)) return rv;

      rv = observer->Notify(GetContentWindow(),
                            NS_SUCCEEDED(aStatus) ? kEndDocumentLoad.GetUnicode() : kFailDocumentLoad.GetUnicode(),
                            urlStr.GetUnicode());

      // XXX Ignore rv for now. They are using nsIEnumerator instead of
      // nsISimpleEnumerator.

      /* To satisfy a request from the QA group */
      if (aStatus == NS_OK) {
        fprintf(stdout, "Document %s loaded successfully\n", (const char*)url);
        fflush(stdout);
    }
      else {
        fprintf(stdout, "Error loading URL %s \n", (const char*)url);
        fflush(stdout);
    }
  } //if (!isFrame)

#ifdef DEBUG_warren
  char* urls;
  aUrl->GetSpec(&urls);
  if (gTimerLog == nsnull)
    gTimerLog = PR_NewLogModule("Timer");
  PRIntervalTime end = PR_IntervalNow();
  PRIntervalTime diff = end - mLoadStartTime;
  PR_LOG(gTimerLog, PR_LOG_DEBUG, 
         (">>>>> Stopping timer for %s. Elapsed: %.3f\n", 
          urls, PR_IntervalToMilliseconds(diff) / 1000.0));
  printf(">>>>> Stopping timer for %s. Elapsed: %.3f\n", 
         urls, PR_IntervalToMilliseconds(diff) / 1000.0);
  nsCRT::free(urls);
#endif

  return NS_OK;
}

NS_IMETHODIMP
nsBrowserInstance::OnStartURLLoad(nsIDocumentLoader* loader, 
                                 nsIChannel* channel)
{
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserInstance::OnProgressURLLoad(nsIDocumentLoader* loader, 
                                    nsIChannel* channel, PRUint32 aProgress, 
                                    PRUint32 aProgressMax)
{
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserInstance::OnStatusURLLoad(nsIDocumentLoader* loader, 
                                  nsIChannel* channel, nsString& aMsg)
{
   EnsureXULBrowserWindow();
   if(!mXULBrowserWindow)
      return NS_OK;

   mXULBrowserWindow->SetDefaultStatus(aMsg.GetUnicode());

   return NS_OK;
}

NS_IMETHODIMP
nsBrowserInstance::OnEndURLLoad(nsIDocumentLoader* loader, 
                               nsIChannel* channel, nsresult aStatus)
{
  return NS_OK;
}

//*****************************************************************************
//    nsBrowserInstance: nsIURIContentListener
//*****************************************************************************

NS_IMETHODIMP nsBrowserInstance::OnStartURIOpen(nsIURI* aURI, 
   const char* aWindowTarget, PRBool* aAbortOpen)
{
   return NS_OK;
}

NS_IMETHODIMP 
nsBrowserInstance::GetProtocolHandler(nsIURI * /* aURI */, nsIProtocolHandler **aProtocolHandler)
{
   // we don't have any app specific protocol handlers we want to use so 
  // just use the system default by returning null.
  *aProtocolHandler = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsBrowserInstance::DoContent(const char *aContentType, nsURILoadCommand aCommand, const char *aWindowTarget, 
                             nsIChannel *aChannel, nsIStreamListener **aContentHandler, PRBool *aAbortProcess)
{
  // forward the DoContent call to our content area webshell
  nsCOMPtr<nsIURIContentListener> ctnListener (do_GetInterface(GetContentAreaDocShell()));
  if (ctnListener)
    return ctnListener->DoContent(aContentType, aCommand, aWindowTarget, aChannel, aContentHandler, aAbortProcess);
  return NS_OK;
}

NS_IMETHODIMP 
nsBrowserInstance::IsPreferred(const char * aContentType,
                               nsURILoadCommand aCommand,
                               const char * aWindowTarget,
                               char ** aDesiredContentType,
                               PRBool * aCanHandleContent)
{
  // the browser window is the primary content handler for the following types:
  // If we are asked to handle any of these types, we will always say Yes!
  // regardlesss of the uri load command.
  //    incoming Type                     Preferred type
  //      text/html
  //      text/xul
  //      text/rdf
  //      text/xml
  //      text/css
  //      image/gif
  //      image/jpeg
  //      image/png
  //      image/tiff
  //      application/http-index-format

  if (aContentType)
  {
     // (1) list all content types we want to  be the primary handler for....
     // and suggest a desired content type if appropriate...
     if (nsCRT::strcasecmp(aContentType,  "text/html") == 0
       || nsCRT::strcasecmp(aContentType, "text/xul") == 0
       || nsCRT::strcasecmp(aContentType, "text/rdf") == 0 
       || nsCRT::strcasecmp(aContentType, "text/xml") == 0
       || nsCRT::strcasecmp(aContentType, "text/css") == 0
       || nsCRT::strcasecmp(aContentType, "image/gif") == 0
       || nsCRT::strcasecmp(aContentType, "image/jpeg") == 0
       || nsCRT::strcasecmp(aContentType, "image/png") == 0
       || nsCRT::strcasecmp(aContentType, "image/tiff") == 0
       || nsCRT::strcasecmp(aContentType, "text/plain") == 0
       || nsCRT::strcasecmp(aContentType, "application/http-index-format") == 0)
       *aCanHandleContent = PR_TRUE;
  }
  else
    *aCanHandleContent = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP 
nsBrowserInstance::CanHandleContent(const char * aContentType,
                                    nsURILoadCommand aCommand,
                                    const char * aWindowTarget,
                                    char ** aDesiredContentType,
                                    PRBool * aCanHandleContent)

{
   // can handle is really determined by what our docshell can 
   // load...so ask it....
   nsCOMPtr<nsIURIContentListener> ctnListener(do_GetInterface(GetContentAreaDocShell())); 
   if (ctnListener)
     return ctnListener->CanHandleContent(aContentType, aCommand, aWindowTarget, aDesiredContentType, aCanHandleContent);
   else
     *aCanHandleContent = PR_FALSE;
 
   return NS_OK;
}

NS_IMETHODIMP 
nsBrowserInstance::GetParentContentListener(nsIURIContentListener** aParent)
{
  *aParent = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsBrowserInstance::SetParentContentListener(nsIURIContentListener* aParent)
{
  NS_ASSERTION(!aParent, "SetParentContentListener on the application level should never be called");
  return NS_OK;
}

NS_IMETHODIMP 
nsBrowserInstance::GetLoadCookie(nsISupports ** aLoadCookie)
{
  *aLoadCookie = nsnull;
  return NS_OK;
}

NS_IMETHODIMP 
nsBrowserInstance::SetLoadCookie(nsISupports * aLoadCookie)
{
  NS_ASSERTION(!aLoadCookie, "SetLoadCookie on the application level should never be called");
  return NS_OK;
}

//*****************************************************************************
// nsBrowserInstance::nsIWebProgressListener
//*****************************************************************************

NS_IMETHODIMP
nsBrowserInstance::OnProgressChange(nsIWebProgress* aWebProgress,
                                    nsIRequest* aRequest,
                                    PRInt32 aCurSelfProgress,
                                    PRInt32 aMaxSelfProgress, 
                                    PRInt32 aCurTotalProgress,
                                    PRInt32 aMaxTotalProgress)
{
  EnsureXULBrowserWindow();
  if(mXULBrowserWindow) {
    nsresult rv;
    nsCOMPtr<nsIChannel> channel;

    channel = do_QueryInterface(aRequest, &rv);
    if (NS_SUCCEEDED(rv)) {
      mXULBrowserWindow->OnProgress(channel, aCurTotalProgress, aMaxTotalProgress);
    }
  }
  return NS_OK;
}
      
NS_IMETHODIMP
nsBrowserInstance::OnStateChange(nsIWebProgress* aWebProgress,
                                 nsIRequest* aRequest,
                                 PRInt32 aProgressStateFlags,
                                 nsresult aStatus)
{
  EnsureXULBrowserWindow();
  if(mXULBrowserWindow) {
    nsresult rv;
    nsCOMPtr<nsIChannel> channel;

    channel = do_QueryInterface(aRequest, &rv);
    if (NS_SUCCEEDED(rv)) {
      mXULBrowserWindow->OnStateChange(channel, aProgressStateFlags);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsBrowserInstance::OnLocationChange(nsIWebProgress* aWebProgress,
                                                  nsIRequest* aRequest,
                                                  nsIURI* aLocation)
{
   EnsureXULBrowserWindow();
   if(!mXULBrowserWindow)
      return NS_OK;

   nsXPIDLCString spec;
   aLocation->GetSpec(getter_Copies(spec));
   nsAutoString specW; specW.AssignWithConversion(spec);
   mXULBrowserWindow->OnLocationChange(specW.GetUnicode());

   return NS_OK;
}

NS_IMETHODIMP 
nsBrowserInstance::OnStatusChange(nsIWebProgress* aWebProgress,
                                  nsIRequest* aRequest,
                                  nsresult aStatus,
                                  const PRUnichar* aMessage)
{
  EnsureXULBrowserWindow();
  if(mXULBrowserWindow) {
    nsresult rv;
    nsCOMPtr<nsIChannel> channel;

    channel = do_QueryInterface(aRequest, &rv);
    if (NS_SUCCEEDED(rv)) {
      mXULBrowserWindow->OnStatus(channel, aStatus, aMessage);
    }
  }
  return NS_OK;
}

//*****************************************************************************
// nsBrowserInstance: Helpers
//*****************************************************************************

nsresult
nsBrowserInstance::InitializeSearch( nsIFindComponent *finder )
{
    nsresult rv = NS_OK;

    if (!finder) return NS_ERROR_NULL_POINTER;

    if (!mSearchContext ) {
        // Create the search context for this browser window.
        rv = finder->CreateContext(GetContentWindow(), nsnull, getter_AddRefs(mSearchContext));
    }
    return rv;
}

NS_IMETHODIMP nsBrowserInstance::CreateMenuItem(
  nsIDOMNode *    aParentMenu,
  PRInt32      aIndex,
  const PRUnichar *  aName)
{
  if (APP_DEBUG) printf("In CreateMenuItem\n");
  nsresult rv=NS_OK;  
  nsCOMPtr<nsIDOMDocument>  doc;

  rv = aParentMenu->GetOwnerDocument(getter_AddRefs(doc));
  if (!NS_SUCCEEDED(rv)) {
    printf("nsBrowserInstance::CreateMenuItem ERROR Getting handle to the document\n");
    return NS_ERROR_FAILURE;
  }
  nsString menuitemName(aName);
  
  // Create nsMenuItem
  nsCOMPtr<nsIDOMElement>  menuItemElement;
  nsString  tagName; tagName.AssignWithConversion("menuitem");
  rv = doc->CreateElement(tagName, getter_AddRefs(menuItemElement));
  if (!NS_SUCCEEDED(rv)) {
    printf("nsBrowserInstance::CreateMenuItem ERROR creating the menu item element\n");
    return NS_ERROR_FAILURE;
  }

  /*
  // Set the hist attribute to true
  rv = menuItemElement->SetAttribute(nsString("ishist"), nsString("true"));
  if (!NS_SUCCEEDED(rv)) {
    printf("nsBrowserAppCore::CreateMenuItem ERROR setting ishist handler\n");
    return NS_ERROR_FAILURE;
  }
*/

  //Set the label for the menu item
  nsString menuitemlabel(aName);
  if (APP_DEBUG) printf("nsBrowserInstance::CreateMenuItem Setting menu name to %s\n", menuitemlabel.ToNewCString());
  rv = menuItemElement->SetAttribute(NS_ConvertASCIItoUCS2("value"), menuitemlabel);
  if (!NS_SUCCEEDED(rv)) {
    printf("nsBrowserInstance::CreateMenuItem ERROR Setting node value for menu item ****\n");
    return NS_ERROR_FAILURE;
  }


    // Set the index attribute to  item's index
  nsString indexString;
  indexString.AppendInt(aIndex);
  rv = menuItemElement->SetAttribute(NS_ConvertASCIItoUCS2("index"), indexString);

  if (!NS_SUCCEEDED(rv)) {
    printf("nsBrowserAppCore::CreateMenuItem ERROR setting ishist handler\n");
    return NS_ERROR_FAILURE;
  }

    // Make a DOMNode out of it
  nsCOMPtr<nsIDOMNode>  menuItemNode = do_QueryInterface(menuItemElement);
  if (!menuItemNode) {
    printf("nsBrowserInstance::CreateMenuItem ERROR converting DOMElement to DOMNode *****\n");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMNode> resultNode;
  // Make nsMenuItem a child of nsMenu
  rv = aParentMenu->AppendChild(menuItemNode, getter_AddRefs(resultNode));
  
  if (!NS_SUCCEEDED(rv)) 
  {
       printf("nsBrowserInstance::CreateMenuItem ERROR appending menuitem to menu *****\n");
     return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBrowserInstance::UpdateGoMenu(nsIDOMNode * aGoMenu)
{

 if (!GetContentAreaDocShell())  {
    printf("nsBrowserInstance::UpdateGoMenu Couldn't get a handle to Content Area DocShell\n");
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDOMNode> menuPopup = aGoMenu;
  nsresult rv;

  // Clear all history children under Go menu
  rv = ClearHistoryMenus(menuPopup);
  if (!NS_SUCCEEDED(rv)) {
    printf("nsBrowserInstance::UpdateGoMenu Error while clearing old history list\n");
  }

  nsCOMPtr<nsISHistory> mSHistory;
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(GetContentAreaDocShell()));
  webNav->GetSessionHistory(getter_AddRefs(mSHistory));
  NS_ENSURE_TRUE(mSHistory, NS_ERROR_FAILURE);

  PRInt32 length=0,i=0;
  //Get total length of the  Session History
  mSHistory->GetCount(&length);

  //Decide on the # of items in the popup list 
  if (length > SHISTORY_POPUP_LIST)
     i  = length-SHISTORY_POPUP_LIST;

  for (PRInt32 j=length-1;j>=i;j--) {
      PRUnichar  *title=nsnull;
      nsCOMPtr<nsIURI> uri;
      nsXPIDLCString   uriSpec;

	  nsCOMPtr<nsISHEntry> shEntry;
	  mSHistory->GetEntryAtIndex(j, PR_FALSE, getter_AddRefs(shEntry));
      NS_ENSURE_TRUE(shEntry, NS_ERROR_FAILURE);

      shEntry->GetURI(getter_AddRefs(uri));
	  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);
	  uri->GetSpec(getter_Copies(uriSpec));

      shEntry->GetTitle(&title);	 
      nsAutoString  histTitle(title);
      if (APP_DEBUG)printf("nsBrowserInstance::UpdateGoMenu URL = %s, TITLE = %s\n", (const char *) uriSpec, histTitle.ToNewCString());
      rv = CreateMenuItem(menuPopup, j, title);
      if (!NS_SUCCEEDED(rv)) {
        printf("nsBrowserInstance::UpdateGoMenu Error while creating history mene item\n");
	  }
      Recycle(title);
	}

  return NS_OK;

}

NS_IMETHODIMP
nsBrowserInstance::ClearHistoryMenus(nsIDOMNode * aParent)
{

   nsresult rv;
   nsCOMPtr<nsIDOMNode> menu = dont_QueryInterface(aParent);

     nsCOMPtr<nsIDOMNodeList>   childList;

     //Get handle to the children list
     rv = menu->GetChildNodes(getter_AddRefs(childList));
     if (NS_SUCCEEDED(rv) && childList) {
        PRInt32 ccount=0;
        childList->GetLength((unsigned int *)&ccount);

        // Remove the children that has the 'hist' attribute set to true.
        for (PRInt32 i=0; i<ccount; i++) {
            nsCOMPtr<nsIDOMNode> child;
            rv = childList->Item(i, getter_AddRefs(child));
      if (!NS_SUCCEEDED(rv) ||  !child) {
        printf("nsBrowserInstance::ClearHistoryPopup, Could not get child\n");
        return NS_ERROR_FAILURE;
      }
      // Get element out of the node
      nsCOMPtr<nsIDOMElement> childElement(do_QueryInterface(child));
      if (!childElement) {
        printf("nsBrowserInstance::ClearHistorypopup Could n't get DOMElement out of DOMNode for child\n");
        return NS_ERROR_FAILURE;
      }

      nsString  attrname; attrname.AssignWithConversion("index");
      nsString  attrvalue;
      rv = childElement->GetAttribute(attrname, attrvalue);

      if (NS_SUCCEEDED(rv) && (attrvalue.Length() > 0)) {
        // It is a history menu item. Remove it
                nsCOMPtr<nsIDOMNode> ret;         
                rv = menu->RemoveChild(child, getter_AddRefs(ret));
          if (NS_SUCCEEDED(rv)) {
           if (ret) {
              if (APP_DEBUG) printf("nsBrowserInstance::ClearHistoryPopup Child %x removed from the popuplist \n", (unsigned int) child.get());                
           }
           else {
              printf("nsBrowserInstance::ClearHistoryPopup Child %x was not removed from popuplist\n", (unsigned int) child.get());
           }
		  }  // NS_SUCCEEDED(rv)
          else
		  {
           printf("nsBrowserInstance::ClearHistoryPopup Child %x was not removed from popuplist\n", (unsigned int) child.get());
                   return NS_ERROR_FAILURE;
		  }         
      }  // atrrvalue == true      
	  else
	  {
		  if (APP_DEBUG) printf("Couldn't get attribute values \n");
	  }
    } //(for) 
   }   // if (childList)
   return NS_OK;
}

NS_IMETHODIMP
nsBrowserInstance::EnsureXULBrowserWindow()
{
   if(mXULBrowserWindow)
      return NS_OK;
   
   nsCOMPtr<nsPIDOMWindow> piDOMWindow(do_QueryInterface(mDOMWindow));
   NS_ENSURE_TRUE(piDOMWindow, NS_ERROR_FAILURE);

   nsCOMPtr<nsISupports> xpConnectObj;
   nsAutoString xulBrowserWinId; xulBrowserWinId.AssignWithConversion("XULBrowserWindow");
   piDOMWindow->GetObjectProperty(xulBrowserWinId.GetUnicode(), getter_AddRefs(xpConnectObj));
   mXULBrowserWindow = do_QueryInterface(xpConnectObj);

   if(mXULBrowserWindow)
      return NS_OK;

   return NS_ERROR_FAILURE;
}

static nsresult
FindNamedXULElement(nsIDocShell * aShell,
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

                    rv = xulDoc->GetElementById( NS_ConvertASCIItoUCS2(aId), getter_AddRefs(elem) );
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


////////////////////////////////////////////////////////////////////////
// browserCntHandler is a content handler component that registers
// the browse as the preferred content handler for various content
// types like text/html, image/jpeg, etc. When the uri loader
// has a content type that no currently open window wants to handle, 
// it will ask the registry for a registered content handler for this
// type. If the browser is registered to handle that type, we'll end
// up in here where we create a new browser window for the url.
//
// I had intially written this as a JS component and would like to do
// so again. However, JS components can't access xpconnect objects that
// return DOM objects. And we need a dom window to bootstrap the browser
/////////////////////////////////////////////////////////////////////////

class nsBrowserContentHandler : public nsIContentHandler, nsICmdLineHandler
{
public:
  NS_DECL_NSICONTENTHANDLER
  NS_DECL_NSICMDLINEHANDLER
  NS_DECL_ISUPPORTS
  CMDLINEHANDLER_REGISTERPROC_DECLS

  nsBrowserContentHandler();
  virtual ~nsBrowserContentHandler();

protected:

};

NS_IMPL_ADDREF(nsBrowserContentHandler);
NS_IMPL_RELEASE(nsBrowserContentHandler);

NS_INTERFACE_MAP_BEGIN(nsBrowserContentHandler)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContentHandler)
   NS_INTERFACE_MAP_ENTRY(nsIContentHandler)
   NS_INTERFACE_MAP_ENTRY(nsICmdLineHandler)
NS_INTERFACE_MAP_END

nsBrowserContentHandler::nsBrowserContentHandler()
{
  NS_INIT_ISUPPORTS();
}

nsBrowserContentHandler::~nsBrowserContentHandler()
{
}

CMDLINEHANDLER_OTHERS_IMPL(nsBrowserContentHandler,"-chrome","general.startup.browser","Start with browser.",NS_BROWSERSTARTUPHANDLER_PROGID,"Browser Startup Handler", PR_TRUE, PR_FALSE)

NS_IMETHODIMP nsBrowserContentHandler::GetChromeUrlForTask(char **aChromeUrlForTask) {

  if (!aChromeUrlForTask)
    return NS_ERROR_NULL_POINTER;

  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID));
  if (prefs) {
    rv = prefs->CopyCharPref("browser.chromeURL", aChromeUrlForTask);
    if (NS_SUCCEEDED(rv) && (*aChromeUrlForTask)[0] == '\0') {
      PL_strfree(*aChromeUrlForTask);
      rv = NS_ERROR_FAILURE;
    }
  }
  if (NS_FAILED(rv))
    *aChromeUrlForTask = PL_strdup("chrome://navigator/content/navigator.xul");

  return NS_OK;
}

NS_IMETHODIMP nsBrowserContentHandler::GetDefaultArgs(PRUnichar **aDefaultArgs) 
{ 
    if (!aDefaultArgs) return NS_ERROR_FAILURE; 

    nsString args;

    nsresult rv;
    nsXPIDLString url;
    static PRBool timebombChecked = PR_FALSE;

    if (timebombChecked == PR_FALSE)
    {
        // timebomb check
        timebombChecked = PR_TRUE;
        
        PRBool expired;
        NS_WITH_SERVICE(nsITimeBomb, timeBomb, kTimeBombCID, &rv);
        if ( NS_FAILED(rv) ) return rv; 

        rv = timeBomb->Init();
        if ( NS_FAILED(rv) ) return rv; 

        rv = timeBomb->CheckWithUI(&expired);
        if ( NS_FAILED(rv) ) return rv; 

        if ( expired ) 
        {
            char* urlString;
            rv = timeBomb->GetTimebombURL(&urlString);
            if ( NS_FAILED(rv) ) return rv;

            *aDefaultArgs =  nsXPIDLString::Copy(url);
            nsMemory::Free(urlString);
            return rv;
        }
    }

    /* the default, in case we fail somewhere */
    args.AssignWithConversion("about:blank");

    nsCOMPtr<nsIPref> prefs(do_GetService(kCPrefServiceCID));
    if (!prefs) return NS_ERROR_FAILURE;

    nsCOMPtr<nsIGlobalHistory> history(do_GetService(kCGlobalHistoryCID));

    PRBool override = PR_FALSE;
    rv = prefs->GetBoolPref(PREF_HOMEPAGE_OVERRIDE, &override);
    if (NS_SUCCEEDED(rv) && override) {
        rv = prefs->GetLocalizedUnicharPref(PREF_HOMEPAGE_OVERRIDE_URL, getter_Copies(url));
        if (NS_SUCCEEDED(rv) && (const PRUnichar *)url) {
            rv = prefs->SetBoolPref(PREF_HOMEPAGE_OVERRIDE, PR_FALSE);
        }
    }
    
    nsAutoString tmp(url);
    if (!url || !tmp.Length()) {
        PRInt32 choice = 0;
        rv = prefs->GetIntPref(PREF_BROWSER_STARTUP_PAGE, &choice);
        if (NS_SUCCEEDED(rv)) {
            switch (choice) {
                case 1:
                    rv = prefs->GetLocalizedUnicharPref(PREF_BROWSER_STARTUP_HOMEPAGE, getter_Copies(url));
                    tmp = url;
                    break;
                case 2:
                    if (history) {
                        nsXPIDLCString curl;
                        rv = history->GetLastPageVisted(getter_Copies(curl));
                        tmp = NS_ConvertUTF8toUCS2((const char *)curl);
                    }
                    break;
                case 0:
                default:
                    args.AssignWithConversion("about:blank");
                    break;
            }
        }
    }

    if (NS_SUCCEEDED(rv) && tmp.Length()) {              
        args = tmp;
    }

    *aDefaultArgs = args.ToNewUnicode(); 
    return NS_OK;
}

NS_IMETHODIMP nsBrowserContentHandler::HandleContent(const char * aContentType,
                                                     const char * aCommand,
                                                     const char * aWindowTarget,
                                                     nsISupports * aWindowContext,
                                                     nsIChannel * aChannel)
{
  // we need a dom window to create the new browser window...in order
  // to do this, we need to get the window mediator service and ask it for a dom window
  NS_ENSURE_ARG(aChannel);
  nsCOMPtr<nsIDOMWindow> parentWindow;
  JSContext* jsContext = nsnull;

  if (aWindowContext)
  {
    parentWindow = do_GetInterface(aWindowContext);
    if (parentWindow)
    {
      nsCOMPtr<nsIScriptGlobalObject> sgo;     
      sgo = do_QueryInterface( parentWindow );
      if (sgo)
      {
        nsCOMPtr<nsIScriptContext> scriptContext;
        sgo->GetContext( getter_AddRefs( scriptContext ) );
        if (scriptContext)
          jsContext = (JSContext*)scriptContext->GetNativeContext();
      }

    }
  }

  // if we still don't have a parent window, try to use the hidden window...
  if (!parentWindow || !jsContext)
  {
    nsCOMPtr<nsIAppShellService> windowService (do_GetService(kAppShellServiceCID));
    NS_ENSURE_SUCCESS(windowService->GetHiddenWindowAndJSContext(getter_AddRefs(parentWindow), &jsContext),
                      NS_ERROR_FAILURE);
  }

  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));
  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);
  nsXPIDLCString spec;
  uri->GetSpec(getter_Copies(spec));

  void* mark;
  jsval* argv;

  nsAutoString value;
  value.AssignWithConversion(spec);

  // we only want to pass in the window target name if it isn't something like _new or _blank....
  // i.e. only real names like "my window", etc...
  const char * windowTarget = aWindowTarget;
  if (!aWindowTarget || !nsCRT::strcasecmp(aWindowTarget, "_new") ||
                        !nsCRT::strcasecmp(aWindowTarget, "_blank") ||
                        !nsCRT::strcasecmp(aWindowTarget, "_top") ||
                        !nsCRT::strcasecmp(aWindowTarget, "_parent") ||
                        !nsCRT::strcasecmp(aWindowTarget, "_content"))
    windowTarget = "";

  argv = JS_PushArguments(jsContext, &mark, "Ws", value.GetUnicode(), windowTarget);
  NS_ENSURE_TRUE(argv, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMWindow> newWindow;
  parentWindow->Open(jsContext, argv, 2, getter_AddRefs(newWindow));
  JS_PopArguments(jsContext, mark);

  // now abort the current channel load...
  aChannel->Cancel(NS_BINDING_ABORTED);

  return NS_OK;
}

NS_DEFINE_MODULE_INSTANCE_COUNTER()

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsBrowserInstance, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBrowserContentHandler)

static nsModuleComponentInfo components[] = {
  { "nsBrowserInstance",
    NS_BROWSERINSTANCE_CID,
    NS_BROWSERINSTANCE_PROGID, 
    nsBrowserInstanceConstructor
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_PROGID_PREFIX"text/html", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_PROGID_PREFIX"text/xul", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_PROGID_PREFIX"text/rdf", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_PROGID_PREFIX"text/xml", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_PROGID_PREFIX"text/css", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_PROGID_PREFIX"text/plain", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_PROGID_PREFIX"image/gif", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_PROGID_PREFIX"image/jpeg", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_PROGID_PREFIX"image/png", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_PROGID_PREFIX"image/tiff", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_PROGID_PREFIX"application/http-index-format", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Startup Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_BROWSERSTARTUPHANDLER_PROGID, 
    nsBrowserContentHandlerConstructor,
    nsBrowserContentHandler::RegisterProc,
    nsBrowserContentHandler::UnregisterProc,
  },
  { "Chrome Startup Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    "component://netscape/commandlinehandler/general-startup-chrome",
    nsBrowserContentHandlerConstructor,
  } 
  
};

NS_IMPL_NSGETMODULE("nsBrowserModule", components)

// =================================================================================
// Toolbar button pref callback
// =================================================================================

int PR_CALLBACK ButtonShowHideCallback(const char* aPref, void* aClosure)
{
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_PROGID, &rv);
  if (NS_FAILED(rv))
    return 0;

  // There is an intelligent mapping from
  // the name of the pref to the id of the element in the XUL.
  //
  // browser.toolbars.showbutton.XXX maps to
  // an element with an ID of XXX-button.  If the ID is of the
  // correct form, then this code will scale tolerably
  // as new buttons are added, and I won't need to have special
  // "My Netscape" and "Net2Phone" cases in the Netscape tree (which
  // would be non-ideal to say the least).
  //
  // hyatt

  nsAutoString pref; pref.AssignWithConversion(aPref);
  PRInt32 index = pref.RFindChar('.');
  if (index == -1)
    return 0;

  nsAutoString element;
  pref.Right(element, pref.Length()-index-1);
  element += NS_LITERAL_STRING("-button");

  PRBool show = PR_TRUE;
  prefs->GetBoolPref(aPref, &show);

  // Get the window mediator and loop over all navigator windows.
  NS_WITH_SERVICE(nsIWindowMediator, windowMediator, kWindowMediatorCID, &rv);
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsISimpleEnumerator> windowEnumerator;

    if (NS_SUCCEEDED(windowMediator->GetEnumerator(NS_LITERAL_STRING("navigator:browser"), 
                                                   getter_AddRefs(windowEnumerator)))) {
      // Get each dom window
      PRBool more;
      windowEnumerator->HasMoreElements(&more);
      while (more) {
        nsCOMPtr<nsISupports> protoWindow;
        windowEnumerator->GetNext(getter_AddRefs(protoWindow));
        if (protoWindow) {
          nsCOMPtr<nsIDOMWindow> domWindow = do_QueryInterface(protoWindow);
          if (domWindow) {
            nsCOMPtr<nsIDOMDocument> doc;
            domWindow->GetDocument(getter_AddRefs(doc));
            
            nsCOMPtr<nsIDOMElement> elt;
            doc->GetElementById(element, getter_AddRefs(elt));
            if (elt) {
              if (show)
                elt->SetAttribute(NS_LITERAL_STRING("hidden"), NS_LITERAL_STRING("false"));
              else elt->SetAttribute(NS_LITERAL_STRING("hidden"), NS_LITERAL_STRING("true"));

              // Modality issues with the (sigh) modal prefs dialog necessitate a synchronous
              // reflow.  Otherwise the reflow seems to get "lost".
              // Do that here by flushing all pending reflows.
              nsCOMPtr<nsIDocument> document(do_QueryInterface(doc));
              document->FlushPendingNotifications();
            }
          }
        }
        windowEnumerator->HasMoreElements(&more);
      }
    }
  }

  return 0;
}
