/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Pierre Phaneuf <pp@ludusdesign.com>
 *    Travis Bogard <travis@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Local Includes
#include "nsBrowserInstance.h"

// Helper Includes
#include "nsIGenericFactory.h"

// Interfaces Needed
#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIHttpProtocolHandler.h"
#include "nsISHistory.h"
#include "nsIWebNavigation.h"
#include "nsPIDOMWindow.h"

///  Unsorted Includes

#include "nsIObserver.h"
#include "pratom.h"
#include "prprf.h"
#include "nsIComponentManager.h"

#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMWindowInternal.h"

#include "nsIContentViewer.h"
#include "nsIContentViewerEdit.h"
#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWindowWatcher.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"

#include "nsIPref.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIWidget.h"
#include "plevent.h"

#include "nsIAppShell.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"

#include "nsIDocumentViewer.h"
#include "nsIGlobalHistory.h"
#include "nsIBrowserHistory.h"

#include "nsIObserverService.h"

#include "nsILocalFile.h"
#include "nsIFileStreams.h"

#include "nsNetUtil.h"
#include "nsICmdLineService.h"

// Stuff to implement file download dialog.
#include "nsFileStream.h"
#include "nsIProxyObjectManager.h" 

#ifdef MOZ_PHOENIX
#include "nsToolkitCompsCID.h"
#endif

#include "nsBrowserStatusFilter.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBrowserStatusFilter)

// If DEBUG, NS_BUILD_REFCNT_LOGGING, MOZ_PERF_METRICS, or MOZ_JPROF is
// defined, enable the PageCycler.
#if defined(DEBUG) || defined(NS_BUILD_REFCNT_LOGGING) || defined(MOZ_PERF_METRICS) || defined(MOZ_JPROF)
#define ENABLE_PAGE_CYCLER
#endif

#include "nsITimeBomb.h"

/* Define Class IDs */
static NS_DEFINE_CID(kPrefServiceCID,           NS_PREF_CID);
static NS_DEFINE_CID(kProxyObjectManagerCID,    NS_PROXYEVENT_MANAGER_CID);
static NS_DEFINE_CID(kAppShellServiceCID,       NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_CID(kCmdLineServiceCID,        NS_COMMANDLINE_SERVICE_CID);
static NS_DEFINE_CID(kCGlobalHistoryCID,        NS_GLOBALHISTORY_CID);

#ifdef DEBUG                                                           
static int APP_DEBUG = 0; // Set to 1 in debugger to turn on debugging.
#else                                                                  
#define APP_DEBUG 0                                                    
#endif                                                                 


#define PREF_HOMEPAGE_OVERRIDE_URL "startup.homepage_override_url"
#define PREF_HOMEPAGE_OVERRIDE_MSTONE "browser.startup.homepage_override.mstone"
#define PREF_BROWSER_STARTUP_PAGE "browser.startup.page"
#define PREF_BROWSER_STARTUP_HOMEPAGE "browser.startup.homepage"

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
    NS_INIT_ISUPPORTS();
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
    if (!mFile) {
      rv = NS_NewNativeLocalFile(nsDependentCString(nativePath),
                                 PR_TRUE, getter_AddRefs(mFile));
      if (NS_FAILED(rv)) return rv;
    }

    nsCOMPtr<nsIInputStream> inStr;
    rv = NS_NewLocalFileInputStream(getter_AddRefs(inStr), mFile);
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

    nsCOMPtr<nsIObserverService> obsServ = 
             do_GetService("@mozilla.org/observer-service;1", &rv);
    if (NS_FAILED(rv)) return rv;
    rv = obsServ->AddObserver(this, "EndDocumentLoad", PR_FALSE );
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
      nsCOMPtr<nsIAppShellService> appShellServ = 
               do_GetService(kAppShellServiceCID, &rv);
      if(NS_FAILED(rv)) return rv;
      nsCOMPtr<nsIProxyObjectManager> pIProxyObjectManager = 
               do_GetService(kProxyObjectManagerCID, &rv);
      if(NS_FAILED(rv)) return rv;
      nsCOMPtr<nsIAppShellService> appShellProxy;
      rv = pIProxyObjectManager->GetProxyForObject(NS_UI_THREAD_EVENTQ, NS_GET_IID(nsIAppShellService), 
                                                appShellServ, PROXY_ASYNC | PROXY_ALWAYS,
                                                getter_AddRefs(appShellProxy));

      (void)appShellProxy->Quit(nsIAppShellService::eAttemptQuit);
      return NS_ERROR_FAILURE;
    }
  }

  NS_IMETHOD Observe(nsISupports* aSubject, 
                     const char* aTopic,
                     const PRUnichar* someData) {
    nsresult rv = NS_OK;
    nsString data(someData);
    if (data.Find(mLastRequest) == 0) {
      char* dataStr = ToNewCString(data);
      printf("########## PageCycler loaded (%d ms): %s\n", 
             PR_IntervalToMilliseconds(PR_IntervalNow() - mIntervalTime), 
             dataStr);
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
          mAppCore->GetContentAreaDocShell(getter_AddRefs(docShell));

          nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(docShell));
          if(webNav)
            webNav->Stop(nsIWebNavigation::STOP_ALL);
        }

        // We need to enqueue an event to load the next page,
        // otherwise we'll run the risk of confusing the docshell
        // (which notifies observers before propagating the
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
          printf("######### PageCycler couldn't asynchronously load: %s\n", NS_ConvertUCS2toUTF8(mLastRequest).get());
        }
      }
    }
    else {
      char* dataStr = ToNewCString(data);
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
    const PRUnichar* url = self->mLastRequest.get();
    printf("########## PageCycler starting: %s\n", NS_ConvertUCS2toUTF8(url).get());

    self->mIntervalTime = PR_IntervalNow();
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
        mShutdownTimer->InitWithFuncCallback(TimesUp, (void *)this, mTimeoutValue*1000,
                                             nsITimer::TYPE_ONE_SHOT);
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
  nsBrowserInstance*    mAppCore;
  nsCOMPtr<nsILocalFile> mFile;
  char*                 mBuffer;
  char*                 mCursor;
  nsAutoString          mLastRequest;
  nsCOMPtr<nsITimer>    mShutdownTimer;
  PRUint32              mTimeoutValue;
  PRUint32              mWaitValue;
  PRIntervalTime        mIntervalTime;
};

NS_IMPL_ISUPPORTS1(PageCycler, nsIObserver)

// TimesUp: callback for the PageCycler timer: called when we have
// waited too long for the page to finish loading.
// 
// The aClosure argument is actually the PageCycler, so use it to stop
// the timer and call the Observe fcn to move on to the next URL.  Note
// that we pass the PageCycler instance as the aSubject argument to the
// Observe function. This is our indication that the Observe method is
// being called after a timeout, allowing the PageCycler to do any
// necessary processing before moving on.

void TimesUp(nsITimer *aTimer, void *aClosure)
{
  if(aClosure){
    char urlBuf[64];
    PageCycler *pCycler = (PageCycler *)aClosure;
    pCycler->StopTimer();
    pCycler->GetLastRequest().ToCString( urlBuf, sizeof(urlBuf), 0 );
    fprintf(stderr,"########## PageCycler Timeout on URL: %s\n", urlBuf);
    pCycler->Observe( pCycler, nsnull, (pCycler->GetLastRequest()).get() );
  }
}

#endif //ENABLE_PAGE_CYCLER

PRBool nsBrowserInstance::sCmdLineURLUsed = PR_FALSE;

//*****************************************************************************
//***    nsBrowserInstance: Object Management
//*****************************************************************************

nsBrowserInstance::nsBrowserInstance() : mIsClosed(PR_FALSE)
{
  mDOMWindow            = nsnull;
  mContentAreaDocShellWeak  = nsnull;
  NS_INIT_ISUPPORTS();
}

nsBrowserInstance::~nsBrowserInstance()
{
  Close();
}

void
nsBrowserInstance::ReinitializeContentVariables()
{
  NS_ASSERTION(mDOMWindow,"Reinitializing Content Variables without a window will cause a crash. see Bugzilla Bug 46454");
  if (!mDOMWindow)
    return;

  nsCOMPtr<nsIDOMWindow> contentWindow;
  mDOMWindow->GetContent(getter_AddRefs(contentWindow));

  nsCOMPtr<nsIScriptGlobalObject> globalObj(do_QueryInterface(contentWindow));

  if (globalObj) {
    nsCOMPtr<nsIDocShell> docShell;
    globalObj->GetDocShell(getter_AddRefs(docShell));

    mContentAreaDocShellWeak = dont_AddRef(NS_GetWeakReference(docShell)); // Weak reference

    if (APP_DEBUG) {
      nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(docShell));
      if (docShellAsItem) {
        nsXPIDLString name;
        docShellAsItem->GetName(getter_Copies(name));
        printf("Attaching to Content WebShell [%s]\n", NS_LossyConvertUCS2toASCII(name).get());
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
  *outDocShell = docShell;
  NS_IF_ADDREF(*outDocShell);
  return NS_OK;
}
    
//*****************************************************************************
//    nsBrowserInstance: nsISupports
//*****************************************************************************

NS_IMPL_ADDREF(nsBrowserInstance)
NS_IMPL_RELEASE(nsBrowserInstance)

NS_INTERFACE_MAP_BEGIN(nsBrowserInstance)
   NS_INTERFACE_MAP_ENTRY(nsIBrowserInstance)
   NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIBrowserInstance)
NS_INTERFACE_MAP_END

//*****************************************************************************
//    nsBrowserInstance: nsIBrowserInstance
//*****************************************************************************

nsresult
nsBrowserInstance::LoadUrl(const PRUnichar * urlToLoad)
{
  nsresult rv = NS_OK;

  nsCOMPtr<nsIDocShell> docShell;
  GetContentAreaDocShell(getter_AddRefs(docShell));

  /* Ask nsWebShell to load the URl */
  nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(docShell));
    
  // Normal browser.
  rv = webNav->LoadURI( urlToLoad,                          // URI string
                        nsIWebNavigation::LOAD_FLAGS_NONE,  // Load flags
                        nsnull,                             // Refering URI
                        nsnull,                             // Post data
                        nsnull );                           // Extra headers

  return rv;
}

NS_IMETHODIMP    
nsBrowserInstance::SetCmdLineURLUsed(PRBool aCmdLineURLUsed)
{
  sCmdLineURLUsed = aCmdLineURLUsed;
  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserInstance::GetCmdLineURLUsed(PRBool* aCmdLineURLUsed)
{
  NS_ASSERTION(aCmdLineURLUsed, "aCmdLineURLUsed can't be null");
  if (!aCmdLineURLUsed)
    return NS_ERROR_NULL_POINTER;

  *aCmdLineURLUsed = sCmdLineURLUsed;
  return NS_OK;
}

NS_IMETHODIMP    
nsBrowserInstance::StartPageCycler(PRBool* aIsPageCycling)
{
  nsresult rv;

  *aIsPageCycling = PR_FALSE;
  if (!sCmdLineURLUsed) {
    nsCOMPtr<nsICmdLineService> cmdLineArgs = 
             do_GetService(kCmdLineServiceCID, &rv);
    if (NS_FAILED(rv)) {
      if (APP_DEBUG) fprintf(stderr, "Could not obtain CmdLine processing service\n");
      return NS_ERROR_FAILURE;
    }

#ifdef ENABLE_PAGE_CYCLER
    // First, check if there's a URL file to load (for testing), and if there 
    // is, process it instead of anything else.
    nsAutoString urlstr;
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

      rv = bb->GetNextURL(urlstr);
      NS_RELEASE(bb);

      *aIsPageCycling = PR_TRUE;
    }

    if (!urlstr.IsEmpty()) {
      // A url was provided. Load it
      if (APP_DEBUG) printf("Got Command line URL to load %s\n", NS_ConvertUCS2toUTF8(urlstr).get());
      rv = LoadUrl( urlstr.get() );
      sCmdLineURLUsed = PR_TRUE;
      return rv;
    }
#endif //ENABLE_PAGE_CYCLER
  }
  return NS_OK;
}

NS_IMETHODIMP
nsBrowserInstance::Init()
{
  nsresult rv = NS_OK;

  return rv;
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

  if (APP_DEBUG) {
    nsCOMPtr<nsIDocShell> docShell;
    globalObj->GetDocShell(getter_AddRefs(docShell));
    nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(docShell));
    if (docShellAsItem) {
      // inform our top level webshell that we are its parent URI content listener...
      nsXPIDLString name;
      docShellAsItem->GetName(getter_Copies(name));
      printf("Attaching to WebShellWindow[%s]\n", NS_LossyConvertUCS2toASCII(name).get());
    }
  }

  ReinitializeContentVariables();

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

  return NS_OK;
}

//*****************************************************************************
// nsBrowserInstance: Helpers
//*****************************************************************************


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

CMDLINEHANDLER_OTHERS_IMPL(nsBrowserContentHandler,"-chrome","general.startup.browser","Load the specified chrome.", PR_TRUE, PR_FALSE)
CMDLINEHANDLER_REGISTERPROC_IMPL(nsBrowserContentHandler, "Browser Startup Handler", NS_BROWSERSTARTUPHANDLER_CONTRACTID);
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

PRBool nsBrowserContentHandler::NeedHomepageOverride(nsIPref *aPrefService)
{
  NS_ASSERTION(aPrefService, "Null pointer to prefs service!");

  // get browser's current milestone
  nsresult rv;
  nsCOMPtr<nsIHttpProtocolHandler> httpHandler(
      do_GetService("@mozilla.org/network/protocol;1?name=http", &rv));
  if (NS_FAILED(rv))
    return PR_TRUE;
  nsCAutoString currMilestone;
  httpHandler->GetMisc(currMilestone);

  // get saved milestone from user's prefs
  nsXPIDLCString savedMilestone;
  rv = aPrefService->GetCharPref(PREF_HOMEPAGE_OVERRIDE_MSTONE, 
                                 getter_Copies(savedMilestone));

  // failed to get pref -or- saved milestone older than current milestone, 
  // write out known current milestone and show URL this time
  if (NS_FAILED(rv) || 
      !(currMilestone.Equals(savedMilestone)))
  {
    // update milestone in "homepage override" pref
    aPrefService->SetCharPref(PREF_HOMEPAGE_OVERRIDE_MSTONE, 
                              currMilestone.get());
    return PR_TRUE;
  }
  
  // don't override if saved and current are same
  return PR_FALSE;
}

nsresult GetHomePageGroup(nsIPref* aPref, PRUnichar** aResult)
{
  nsresult rv;

  nsXPIDLString uri;
  rv = aPref->GetLocalizedUnicharPref(PREF_BROWSER_STARTUP_HOMEPAGE, getter_Copies(uri));
  if (NS_FAILED(rv))
    return rv;

  PRInt32 count = 0;
  rv = aPref->GetIntPref("browser.startup.homepage.count", &count);

  // if we couldn't get the pref (unlikely) or only have one homepage
  if (NS_FAILED(rv) || count <= 1) {
    *aResult = ToNewUnicode(uri);
    return NS_OK;
  }

  // The "homepage" is a group of pages, put them in uriList separated by '\n'
  nsAutoString uriList(uri);

  for (PRInt32 i = 1; i < count; ++i) {
    nsCAutoString pref(NS_LITERAL_CSTRING("browser.startup.homepage."));
    pref.AppendInt(i);

    rv = aPref->GetLocalizedUnicharPref(pref.get(), getter_Copies(uri));
    if (NS_FAILED(rv))
      return rv;

    uriList.Append(PRUnichar('\n'));
    uriList.Append(uri);
  }

  *aResult = ToNewUnicode(uriList);
  return NS_OK;
}

NS_IMETHODIMP nsBrowserContentHandler::GetDefaultArgs(PRUnichar **aDefaultArgs)
{
  if (!aDefaultArgs)
    return NS_ERROR_NULL_POINTER;

  nsresult rv;
  static PRBool timebombChecked = PR_FALSE;
  nsAutoString args;

  if (!timebombChecked) {
    // timebomb check
    timebombChecked = PR_TRUE;

    PRBool expired;
    nsCOMPtr<nsITimeBomb> timeBomb(do_GetService(NS_TIMEBOMB_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;

    rv = timeBomb->Init();
    if (NS_FAILED(rv)) return rv;

    rv = timeBomb->CheckWithUI(&expired);
    if (NS_FAILED(rv)) return rv;

    if (expired) {
      nsXPIDLCString urlString;
      rv = timeBomb->GetTimebombURL(getter_Copies(urlString));
      if (NS_FAILED(rv)) return rv;

      args.AssignWithConversion(urlString);
    }
  }

  if (args.IsEmpty()) {
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID));
    if (!prefs) return NS_ERROR_FAILURE;

    if (NeedHomepageOverride(prefs)) {
      nsXPIDLString url;
      rv = prefs->GetLocalizedUnicharPref(PREF_HOMEPAGE_OVERRIDE_URL, getter_Copies(url));
      if (NS_SUCCEEDED(rv) && (const PRUnichar *)url) {
        args = url;
      }
    }

    if (args.IsEmpty()) {
      PRInt32 choice = 0;
      rv = prefs->GetIntPref(PREF_BROWSER_STARTUP_PAGE, &choice);
      if (NS_SUCCEEDED(rv)) {
        switch (choice) {
          case 1: {
            // skip the code below
            return GetHomePageGroup(prefs, aDefaultArgs);
          }
          case 2: {
            nsCOMPtr<nsIBrowserHistory> history(do_GetService(kCGlobalHistoryCID));
            if (history) {
              nsXPIDLCString curl;
              rv = history->GetLastPageVisited(getter_Copies(curl));
              args.AssignWithConversion(curl);
            }
            break;
          }
          case 0:
          default:
            // fall through to about:blank below
            break;
        }
      }

      // the default, in case we fail somewhere
      if (args.IsEmpty())
        args.Assign(NS_LITERAL_STRING("about:blank"));
    }
  }

  *aDefaultArgs = ToNewUnicode(args);
  return NS_OK;
}

NS_IMETHODIMP nsBrowserContentHandler::HandleContent(const char * aContentType,
                                                     const char * aCommand,
                                                     nsISupports * aWindowContext,
                                                     nsIRequest * aRequest)
{
  // create a new browser window to handle the content
  NS_ENSURE_ARG(aRequest);
  nsCOMPtr<nsIDOMWindow> parentWindow;

  if (aWindowContext)
    parentWindow = do_GetInterface(aWindowContext);

  nsCOMPtr<nsIChannel> aChannel = do_QueryInterface(aRequest);
  if (!aChannel) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIURI> uri;
  aChannel->GetURI(getter_AddRefs(uri));
  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);
  nsCAutoString spec;
  uri->GetSpec(spec);

  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  if (wwatch) {
    nsCOMPtr<nsIDOMWindow> newWindow;
    wwatch->OpenWindow(parentWindow, spec.get(), "", 0, 0,
              getter_AddRefs(newWindow));
  }

  // now abort the current channel load...
  aRequest->Cancel(NS_BINDING_ABORTED);

  return NS_OK;
}
