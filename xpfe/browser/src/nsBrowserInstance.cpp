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
#include "nsIHTTPChannel.h"

///  Unsorted Includes

#include "nsIMarkupDocumentViewer.h"
#include "pratom.h"
#include "prprf.h"
#include "nsIComponentManager.h"

#include "nsIFileSpecWithUI.h"

#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMWindowInternal.h"

#include "nsIScriptGlobalObject.h"
#include "nsIContentViewer.h"
#include "nsIContentViewerEdit.h"
#include "nsIWebShell.h"
#include "nsIWebNavigation.h"
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

#include "nsCURILoader.h"
#include "nsIContentHandler.h"
#include "nsNetUtil.h"
#include "nsICmdLineHandler.h"

#include "nsIWindowMediator.h"

#include "nsDocumentCharsetInfoCID.h"
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

#include "nsISecureBrowserUI.h"

static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);
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

  PLEvent mEvent;

  PageCycler(nsBrowserInstance* appCore, const char *aTimeoutValue = nsnull, const char *aWaitValue = nsnull)
    : mAppCore(appCore), mBuffer(nsnull), mCursor(nsnull), mTimeoutValue(0), mWaitValue(1 /*sec*/) { 
    NS_INIT_REFCNT();
    NS_ADDREF(mAppCore);
    if (aTimeoutValue){
      mTimeoutValue = atol(aTimeoutValue);
    }
    if (aWaitValue) {
      mWaitValue = atol(aWaitValue);
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

    NS_WITH_SERVICE(nsIObserverService, obsServ, NS_OBSERVERSERVICE_CONTRACTID, &rv);
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
          nsCOMPtr<nsIDocShell> docShell;
          mAppCore->GetContentDocShell(getter_AddRefs(docShell));

          nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(docShell));
          if(webNav)
            webNav->Stop();
        }

        // We need to enqueue an event to load the next page,
        // otherwise we'll run the risk of confusing the docshell
        // (which notifies observers before propogating the
        // DocumentEndLoad up to parent docshells).
        nsCOMPtr<nsIEventQueueService> eqs
          = do_GetService(NS_EVENTQUEUESERVICE_CONTRACTID);

        rv = NS_ERROR_FAILURE;

        if (eqs) {
          nsCOMPtr<nsIEventQueue> eq;
          eqs->ResolveEventQueue(NS_UI_THREAD_EVENTQ, getter_AddRefs(eq));
          if (eq) {
            rv = eq->InitEvent(&mEvent, this, HandleAsyncLoadEvent,
                               DestroyAsyncLoadEvent);

            if (NS_SUCCEEDED(rv)) {
              rv = eq->PostEvent(&mEvent);
            }
          }
        }

        if (NS_FAILED(rv)) {
          printf("######### PageCycler couldn't asynchronously load: %s\n",
                 NS_STATIC_CAST(const char*, NS_ConvertUCS2toUTF8(mLastRequest)));
        }
      }
    }
    else {
      char* dataStr = data.ToNewCString();
      printf("########## PageCycler possible failure for: %s\n", dataStr);
      nsCRT::free(dataStr);
    }
    return rv;
  }

  static void* PR_CALLBACK
  HandleAsyncLoadEvent(PLEvent* aEvent)
  {
    // This is the callback that actually loads the page
    PageCycler* self = NS_STATIC_CAST(PageCycler*, PL_GetEventOwner(aEvent));

    // load the URL
    const PRUnichar* url = self->mLastRequest.GetUnicode();
    printf("########## PageCycler starting: %s\n",
           NS_STATIC_CAST(const char*, NS_ConvertUCS2toUTF8(url)));

    self->mAppCore->LoadUrl(url);

    // start new timer
    self->StartTimer();
    return nsnull;
  }

  static void PR_CALLBACK
  DestroyAsyncLoadEvent(PLEvent* aEvent) { /*no-op*/ }

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
      mShutdownTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
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
  PRUint32              mWaitValue;
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
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
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
    NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv)) {
      prefs->UnregisterCallback(kShowToolbarElts, ButtonShowHideCallback, nsnull);
    }    
  }
}

void
nsBrowserInstance::ReinitializeContentVariables()
{
  nsCOMPtr<nsIDOMWindowInternal> content;
  mDOMWindow->Get_content(getter_AddRefs(content));
  SetContentWindow(content);

  /* reinitialize the security module */
  nsresult rv;
  nsCOMPtr<nsSecureBrowserUI> security =
     do_CreateInstance(NS_SECURE_BROWSER_UI_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv) && security) {
    nsCOMPtr<nsIDOMDocument> doc;
    rv = mDOMWindow->GetDocument(getter_AddRefs(doc));
    if (NS_SUCCEEDED(rv) && doc) {
      nsCOMPtr<nsIDOMElement> button;
      rv = doc->GetElementById(NS_LITERAL_STRING("security-button"),
                               getter_AddRefs(button));
      if (NS_SUCCEEDED(rv)) {
        security->Init(content,button);
      }
    }
  }
}

nsresult nsBrowserInstance::GetContentAreaDocShell(nsIDocShell** outDocShell)
{
  nsCOMPtr<nsIDocShell> docShell(do_QueryReferent(mContentAreaDocShellWeak));
  if (!mIsClosed && docShell) {
    // we're still alive and the docshell still exists. but has it been destroyed?
    nsCOMPtr<nsIBaseWindow> hack = do_QueryInterface(docShell);
    if (hack) {
      nsCOMPtr<nsIWidget> parent;
      hack->GetParentWidget(getter_AddRefs(parent));
      if (!parent)
        // it's a zombie. a new one is in place. set up to use it.
        docShell = 0;
    }
  }
  if (!mIsClosed && !docShell)
    ReinitializeContentVariables();

  docShell = do_QueryReferent(mContentAreaDocShellWeak);
  NS_IF_ADDREF(*outDocShell = docShell);
  return NS_OK;
}
    

nsresult nsBrowserInstance::GetContentWindow(nsIDOMWindowInternal** outDOMWindow)
{
  nsCOMPtr<nsIDOMWindowInternal> domWindow(do_QueryReferent(mContentWindowWeak));
  if (!domWindow)
    ReinitializeContentVariables();
  domWindow = do_QueryReferent(mContentWindowWeak);
  NS_IF_ADDREF(*outDOMWindow = domWindow);
  return NS_OK;
}

nsresult nsBrowserInstance::GetFocussedContentWindow(nsIDOMWindowInternal** outFocussedWindow)
{
  nsCOMPtr<nsIDOMWindowInternal> focussedWindow;
  
  // get the window that has focus (e.g. framesets)
  if (mDocShell)
  {    
    nsCOMPtr<nsIContentViewer> cv;
    mDocShell->GetContentViewer(getter_AddRefs(cv));    
    nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
    if (docv)
    {
      // Get the document from the doc viewer.
      nsCOMPtr<nsIDocument> doc;
      docv->GetDocument(*getter_AddRefs(doc));
      nsCOMPtr<nsIDOMXULDocument> xulDoc(do_QueryInterface(doc));
      if (xulDoc)
      {
        nsCOMPtr<nsIDOMXULCommandDispatcher>  commandDispatcher;
        xulDoc->GetCommandDispatcher(getter_AddRefs(commandDispatcher));
        
        if (commandDispatcher)
          commandDispatcher->GetFocusedWindow(getter_AddRefs(focussedWindow));
      }
    }
  }
  
  if (!focussedWindow)
    GetContentWindow(getter_AddRefs(focussedWindow));   // default to content window

  NS_IF_ADDREF(*outFocussedWindow = focussedWindow);
  return NS_OK;
}


nsresult nsBrowserInstance::GetContentAreaDocLoader(nsIDocumentLoader** outDocLoader)
{
  nsCOMPtr<nsIDocumentLoader> docLoader(do_QueryReferent(mContentAreaDocLoaderWeak));
  if (!docLoader)
    ReinitializeContentVariables();
  docLoader = do_QueryReferent(mContentAreaDocLoaderWeak);
  NS_IF_ADDREF(*outDocLoader = docLoader);
  return NS_OK;
}

//*****************************************************************************
//    nsBrowserInstance: nsISupports
//*****************************************************************************

NS_IMPL_ADDREF(nsBrowserInstance)
NS_IMPL_RELEASE(nsBrowserInstance)

NS_INTERFACE_MAP_BEGIN(nsBrowserInstance)
   NS_INTERFACE_MAP_ENTRY(nsIBrowserInstance)
   NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
   NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIURIContentListener)
NS_INTERFACE_MAP_END

//*****************************************************************************
//    nsBrowserInstance: nsIBrowserInstance
//*****************************************************************************

NS_IMETHODIMP    
nsBrowserInstance::LoadUrl(const PRUnichar * urlToLoad)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDocShell> docShell;
  GetContentAreaDocShell(getter_AddRefs(docShell));

  /* Ask nsWebShell to load the URl */
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(docShell));
    
  // Normal browser.
  rv = webNav->LoadURI( urlToLoad, nsIWebNavigation::LOAD_FLAGS_NONE );

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
      // see if we have a wait value corresponding to the url-file
      nsXPIDLCString waitVal;
      rv = cmdLineArgs->GetCmdLineValue("-fwait", getter_Copies(waitVal));
      // cereate the cool PageCycler instance
      PageCycler* bb = new PageCycler(this, timeoutVal, waitVal);
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
      nsCOMPtr<nsIDocShell> docShell;
      GetContentAreaDocShell(getter_AddRefs(docShell));

      // Examine content URL.
      if (docShell) {
        nsCOMPtr<nsIURI> uri;
        nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(docShell);
        rv = webNav->GetCurrentURI(getter_AddRefs(uri));
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

  rv = FindNamedXULElement(mDocShell, "args", address_of(argsElement));
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
nsBrowserInstance::Init()
{
  nsresult rv = NS_OK;

  // register ourselves as a content listener with the uri dispatcher service
  rv = NS_OK;
  NS_WITH_SERVICE(nsIURILoader, dispatcher, NS_URI_LOADER_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) 
    rv = dispatcher->RegisterContentListener(this);


  return rv;
}

NS_IMETHODIMP    
nsBrowserInstance::SetContentWindow(nsIDOMWindowInternal* aWin)
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

    // Add a WebProgressListener to receive start/stop document notifications
    nsCOMPtr<nsIWebProgress> webProgress(do_GetInterface(docShell));
    webProgress->AddProgressListener(NS_STATIC_CAST(nsIWebProgressListener*, this));

    nsCOMPtr<nsISHistory> sessionHistory;
    if (mSessionHistory) {
  	  /* There is already a Session History for this browser 
  	   * component. Maybe the theme changed and we got called
  	   * to reinitialize the window, docloader, docshell etc...
  	   * Use the existing session History
  	   */
  	  sessionHistory = mSessionHistory;
    }
    else {
      sessionHistory = (do_CreateInstance(NS_SHISTORY_CONTRACTID, &rv));
      mSessionHistory = sessionHistory;
      if (!mSessionHistory) {
  	    if (APP_DEBUG) printf("#### Error initialising Session History ####\n");
        return NS_FAILED(rv) ? rv : NS_ERROR_OUT_OF_MEMORY;
      }
    }

    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(docShell));
    if (webNav)
       webNav->SetSessionHistory(sessionHistory);

    nsCOMPtr<nsIDocShell> docShell;
    GetContentAreaDocShell(getter_AddRefs(docShell));

    nsCOMPtr<nsIDocShellHistory> dsHistory(do_QueryInterface(docShell));
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
    nsCOMPtr<nsIUrlbarHistory>  ubHistory = do_GetService(NS_URLBARHISTORY_CONTRACTID, &rv);
	
	  NS_ENSURE_TRUE(ubHistory, NS_ERROR_FAILURE);
	  mUrlbarHistory = ubHistory;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsBrowserInstance::GetContentDocShell(nsIDocShell** aDocShell)
{
  NS_ENSURE_ARG_POINTER(aDocShell);

  return GetContentAreaDocShell(aDocShell);
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
nsBrowserInstance::SetWebShellWindow(nsIDOMWindowInternal* aWin)
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
nsBrowserInstance::GetDocumentCharset(PRUnichar **aCharset)
{
  nsCOMPtr<nsIDOMWindowInternal> contentWindow;
  GetContentWindow(getter_AddRefs(contentWindow));

  nsCOMPtr<nsIScriptGlobalObject> globalObj(do_QueryInterface(contentWindow));

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
  nsCOMPtr<nsIDOMWindowInternal> contentWindow;
  GetContentWindow(getter_AddRefs(contentWindow));

  nsCOMPtr<nsIScriptGlobalObject> globalObj(do_QueryInterface(contentWindow));
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
  nsCOMPtr<nsIDOMWindowInternal> contentWindow;
  GetContentWindow(getter_AddRefs(contentWindow));

  nsCOMPtr<nsIScriptGlobalObject> globalObj(do_QueryInterface(contentWindow));
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
  nsCOMPtr<nsIDOMWindowInternal> contentWindow;
  GetContentWindow(getter_AddRefs(contentWindow));

  nsCOMPtr<nsIScriptGlobalObject> globalObj(do_QueryInterface(contentWindow));
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
nsBrowserInstance::Close()
{ 
  // if we have already been closed....then just return
  if (mIsClosed) 
    return NS_OK;
  else
    mIsClosed = PR_TRUE;

  // Undo other stuff we did in SetContentWindow.
  // Remove listeners we may have registered
  nsCOMPtr<nsIDocShell> docShell;
  GetContentAreaDocShell(getter_AddRefs(docShell));
  if (docShell)
  {
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
  NS_WITH_SERVICE(nsIURILoader, dispatcher, NS_URI_LOADER_CONTRACTID, &rv);
  if (NS_SUCCEEDED(rv)) 
    rv = dispatcher->UnRegisterContentListener(this);

  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserInstance::Copy()
{
  nsCOMPtr<nsIDocShell> docShell;
  GetContentAreaDocShell(getter_AddRefs(docShell));
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);
  
  nsCOMPtr<nsIContentViewer> viewer;
  docShell->GetContentViewer(getter_AddRefs(viewer));
  nsCOMPtr<nsIContentViewerEdit> edit(do_QueryInterface(viewer));
  if (edit) {
      edit->CopySelection();
  }
  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserInstance::Find()
{
    nsresult rv = NS_OK;
    PRBool   found = PR_FALSE;
    
    // Get find component.
    nsCOMPtr <nsIFindComponent> finder = do_GetService(NS_IFINDCOMPONENT_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    if (!finder) return NS_ERROR_FAILURE;

    // get the window to search
    nsCOMPtr<nsIDOMWindowInternal>  windowToSearch;  
    GetFocussedContentWindow(getter_AddRefs(windowToSearch));
    
    // Make sure we've initialized searching for this document.
    rv = InitializeSearch(windowToSearch, finder);
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
    nsCOMPtr <nsIFindComponent> finder = do_GetService(NS_IFINDCOMPONENT_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;
    if (!finder) return NS_ERROR_FAILURE;

    // get the window to search
    nsCOMPtr<nsIDOMWindowInternal>  windowToSearch;  
    GetFocussedContentWindow(getter_AddRefs(windowToSearch));

    // Make sure we've initialized searching for this document.
    rv = InitializeSearch(windowToSearch, finder);
    if (NS_FAILED(rv)) return rv;

    // Perform find via find component.
    if (mSearchContext) {
        rv = finder->FindNext(mSearchContext, &found );
    }

    return rv;
}

//*****************************************************************************
//  Helper functions for notifying observers when documents start and finish
//  loading.
//*****************************************************************************

nsresult nsBrowserInstance::StartDocumentLoad(nsIDOMWindow *aDOMWindow,
                                              nsIChannel *aChannel)
{
  nsresult rv;

  nsCOMPtr<nsIURI> uri;
  nsXPIDLCString uriCString;
  nsAutoString urlStr;

  // Get the URI strign and convert it to unicode...
  rv = aChannel->GetURI(getter_AddRefs(uri));
  if (NS_FAILED(rv)) return rv;

  rv = uri->GetSpec(getter_Copies(uriCString));
  if (NS_FAILED(rv)) return rv;

  urlStr.AssignWithConversion(uriCString);

#ifdef DEBUG_warren
  if (gTimerLog == nsnull)
    gTimerLog = PR_NewLogModule("Timer");
  mLoadStartTime = PR_IntervalNow();
  PR_LOG(gTimerLog, PR_LOG_DEBUG, 
         (">>>>> Starting timer for %s\n", (const char *)uriCString));
  printf(">>>>> Starting timer for %s\n", (const char *)uriCString);
#endif

  //
  // If this document notification is for a frame then ignore it...
  //
  nsCOMPtr<nsIDOMWindowInternal> contentWindow;
  nsCOMPtr<nsIDOMWindow> thisDOMWindow;

  rv = GetContentWindow(getter_AddRefs(contentWindow));
  if (NS_FAILED(rv)) return rv;

  thisDOMWindow = do_QueryInterface(contentWindow);
  if (aDOMWindow != thisDOMWindow.get()) {
    // Since the notification is from a different DOM window, just ignore it.
    return NS_OK;
  }

  // Notify observers that a document load has started in the
  // content window.
  NS_WITH_SERVICE(nsIObserverService, observer, NS_OBSERVERSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsAutoString kStartDocumentLoad;
  kStartDocumentLoad.AssignWithConversion("StartDocumentLoad");

  rv = observer->Notify(contentWindow,
                        kStartDocumentLoad.GetUnicode(),
                        urlStr.GetUnicode());

    // XXX Ignore rv for now. They are using nsIEnumerator instead of
    // nsISimpleEnumerator.

  return NS_OK;
}

nsresult nsBrowserInstance::EndDocumentLoad(nsIDOMWindow *aDOMWindow,
                                            nsIChannel *aChannel,
                                            nsresult aStatus)
{
  nsresult rv;

  nsCOMPtr<nsIURI> uri;
  rv = aChannel->GetOriginalURI(getter_AddRefs(uri));
  if (NS_FAILED(rv)) return rv;

  nsXPIDLCString urlCString;
  rv = uri->GetSpec(getter_Copies(urlCString));
  if (NS_FAILED(rv)) return rv;

#ifdef DEBUG_warren
  if (gTimerLog == nsnull)
    gTimerLog = PR_NewLogModule("Timer");
  PRIntervalTime end = PR_IntervalNow();
  PRIntervalTime diff = end - mLoadStartTime;
  PR_LOG(gTimerLog, PR_LOG_DEBUG, 
         (">>>>> Stopping timer for %s. Elapsed: %.3f\n", 
          (const char *)urlCString, PR_IntervalToMilliseconds(diff) / 1000.0));
  printf(">>>>> Stopping timer for %s. Elapsed: %.3f\n", 
         (const char *)urlCString, PR_IntervalToMilliseconds(diff) / 1000.0);
#endif


  //
  // If this document notification is for a frame then ignore it...
  //
  if (aDOMWindow) {
    nsCOMPtr<nsIDOMWindow> topDOMWindow;

    aDOMWindow->GetTop(getter_AddRefs(topDOMWindow));

    if (aDOMWindow != topDOMWindow.get()) {
      // Since the notification is from a child DOM window, just ignore it.
      return NS_OK;
    }
  }

  //
  // XXX: The DocLoader should never be busy at this point!!  
  //
#if 1
  nsCOMPtr<nsIDocumentLoader> docLoader;
  GetContentAreaDocLoader(getter_AddRefs(docLoader));
 
  if (docLoader) {
    PRBool isBusy = PR_FALSE;

    docLoader->IsBusy(&isBusy);
    if (isBusy) {
      NS_ASSERTION(0, "The DocLoader is still busy... There is a bug in End Document notifications\n");
      return NS_OK;
    }
  }
#endif 

  /* If this is a frame, don't do any of the Global History
   * & observer thingy 
   */
  nsAutoString urlStr;
  nsAutoString notifyString;
  nsCOMPtr<nsIDOMWindowInternal> contentWindow;

  urlStr.AssignWithConversion(urlCString);
  notifyString.AssignWithConversion( NS_SUCCEEDED(aStatus) ? "EndDocumentLoad"
                                                           : "FailDocumentLoad" );
  // Notify observers that a document load has started in the
  // content window.
  rv = GetContentWindow(getter_AddRefs(contentWindow));
  if (NS_FAILED(rv)) return rv;

  NS_WITH_SERVICE(nsIObserverService, observer, NS_OBSERVERSERVICE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = observer->Notify(contentWindow,
                        notifyString.GetUnicode(),
                        urlStr.GetUnicode());

  // XXX Ignore rv for now. They are using nsIEnumerator instead of
  // nsISimpleEnumerator.

  /* To satisfy a request from the QA group */
  nsCOMPtr<nsIInputStream> postData;
  if (NS_SUCCEEDED(aStatus)) {
    // Remember post data for http channels.
    nsCOMPtr<nsIHTTPChannel> httpChannel(do_QueryInterface(aChannel));
    if (httpChannel) {
      httpChannel->GetUploadStream(getter_AddRefs(postData));
    }
    fprintf(stdout, "Document %s loaded successfully\n",
            (const char*)urlCString);
    fflush(stdout);
  } else {
    fprintf(stdout, "Error loading URL %s: %0x \n", 
            (const char*)urlCString, aStatus);
    fflush(stdout);
  }
  SetPostData(postData);

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
  nsCOMPtr<nsIDocShell> docShell;
  GetContentAreaDocShell(getter_AddRefs(docShell));

  // forward the DoContent call to our content area webshell
  nsCOMPtr<nsIURIContentListener> ctnListener (do_GetInterface(docShell));
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
  nsCOMPtr<nsIDocShell> docShell;
  GetContentAreaDocShell(getter_AddRefs(docShell));

  // can handle is really determined by what our docshell can 
  // load...so ask it....
  nsCOMPtr<nsIURIContentListener> ctnListener(do_GetInterface(docShell)); 
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
                                 PRInt32 aStateFlags,
                                 nsresult aStatus)
{
  nsresult rv;
  nsCOMPtr<nsIChannel> channel(do_QueryInterface(aRequest));

  EnsureXULBrowserWindow();

  // Ignore this notification if it did not originate from a channel...
  if (!channel) {
    return NS_OK;
  }

  if (aStateFlags & nsIWebProgressListener::STATE_IS_NETWORK) {
    nsCOMPtr<nsIDOMWindow> domWindow;

    rv = aWebProgress->GetDOMWindow(getter_AddRefs(domWindow));
    if (NS_SUCCEEDED(rv)) {
      if (aStateFlags & nsIWebProgressListener::STATE_START) {
        rv = StartDocumentLoad(domWindow, channel);
      }
      else if (aStateFlags & nsIWebProgressListener::STATE_STOP) {
        rv = EndDocumentLoad(domWindow, channel, aStatus);
      }
    }
  }

  if(mXULBrowserWindow) {
    mXULBrowserWindow->OnStateChange(channel, aStateFlags);
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


NS_IMETHODIMP 
nsBrowserInstance::OnSecurityChange(nsIWebProgress *aWebProgress, 
                                    nsIRequest *aRequest, 
                                    PRInt32 state)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


//*****************************************************************************
// nsBrowserInstance: Helpers
//*****************************************************************************

nsresult
nsBrowserInstance::InitializeSearch(nsIDOMWindowInternal* windowToSearch, nsIFindComponent *finder)
{
    nsresult rv = NS_OK;

    if (!finder) return NS_ERROR_NULL_POINTER;
    if (!windowToSearch) return NS_ERROR_NULL_POINTER;
    
    if (!mSearchContext )
        rv = finder->CreateContext(windowToSearch, nsnull, getter_AddRefs(mSearchContext));
    else
        rv = finder->ResetContext(mSearchContext, windowToSearch, nsnull);
    
    return rv;
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

class nsBrowserContentHandler : public nsIContentHandler, public nsICmdLineHandler
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

CMDLINEHANDLER_OTHERS_IMPL(nsBrowserContentHandler,"-chrome","general.startup.browser","Load the specified chrome.",NS_BROWSERSTARTUPHANDLER_CONTRACTID,"Browser Startup Handler", PR_TRUE, PR_FALSE)

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
                        rv = history->GetLastPageVisited(getter_Copies(curl));
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
  nsCOMPtr<nsIDOMWindowInternal> parentWindow;
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

  nsCOMPtr<nsIDOMWindowInternal> newWindow;
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
    NS_BROWSERINSTANCE_CONTRACTID, 
    nsBrowserInstanceConstructor
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_CONTRACTID_PREFIX"text/html", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_CONTRACTID_PREFIX"text/xul", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_CONTRACTID_PREFIX"text/rdf", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_CONTRACTID_PREFIX"text/xml", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_CONTRACTID_PREFIX"text/css", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_CONTRACTID_PREFIX"text/plain", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_CONTRACTID_PREFIX"image/gif", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_CONTRACTID_PREFIX"image/jpeg", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_CONTRACTID_PREFIX"image/png", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_CONTRACTID_PREFIX"image/tiff", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Content Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_CONTENT_HANDLER_CONTRACTID_PREFIX"application/http-index-format", 
    nsBrowserContentHandlerConstructor 
  },
  { "Browser Startup Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    NS_BROWSERSTARTUPHANDLER_CONTRACTID, 
    nsBrowserContentHandlerConstructor,
    nsBrowserContentHandler::RegisterProc,
    nsBrowserContentHandler::UnregisterProc,
  },
  { "Chrome Startup Handler",
    NS_BROWSERCONTENTHANDLER_CID,
    "@mozilla.org/commandlinehandler/general-startup;1?type=chrome",
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
  NS_WITH_SERVICE(nsIPref, prefs, NS_PREF_CONTRACTID, &rv);
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

    if (NS_SUCCEEDED(windowMediator->GetEnumerator(NS_LITERAL_STRING("navigator:browser").get(), 
                                                   getter_AddRefs(windowEnumerator)))) {
      // Get each dom window
      PRBool more;
      windowEnumerator->HasMoreElements(&more);
      while (more) {
        nsCOMPtr<nsISupports> protoWindow;
        windowEnumerator->GetNext(getter_AddRefs(protoWindow));
        if (protoWindow) {
          nsCOMPtr<nsIDOMWindowInternal> domWindow = do_QueryInterface(protoWindow);
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

NS_IMETHODIMP
nsBrowserInstance::GetPostData( nsIInputStream **aResult ) {
    NS_ENSURE_ARG_POINTER( aResult );
    *aResult = mPostData;
    NS_IF_ADDREF( *aResult );
    return NS_OK;
}

NS_IMETHODIMP
nsBrowserInstance::SetPostData( nsIInputStream *aInputStream ) {
    mPostData = aInputStream;
    return NS_OK;
}
