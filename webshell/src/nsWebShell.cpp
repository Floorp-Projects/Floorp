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
 */
#include "nsDocShell.h"
#include "nsIWebShell.h"
#include "nsIInterfaceRequestor.h"
#include "nsIDocumentLoader.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIClipboardCommands.h"
#include "nsIDeviceContext.h"
#include "nsILinkHandler.h"
#include "nsIStreamListener.h"
#include "nsNetUtil.h"
#include "nsIProtocolHandler.h"
#include "nsIDNSService.h"
#include "nsIRefreshURI.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsIProgressEventSink.h"
#include "nsDOMEvent.h"
#include "nsIPresContext.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsCRT.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"
#include "plevent.h"
#include "prprf.h"
#include "nsIPluginHost.h"
#include "nsplugin.h"
#include "nsIFrame.h"
//#include "nsPluginsCID.h"
#include "nsIPluginManager.h"
#include "nsIPref.h"
#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "nsIBrowserWindow.h"
#include "nsIContent.h"
#include "prlog.h"
#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsIWebShellServices.h"
#include "nsIGlobalHistory.h"
#include "prmem.h"
#include "nsXPIDLString.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLDocument.h"
#include "nsLayoutCID.h"
#include "nsIDOMRange.h"
#include "nsIURIContentListener.h"
#include "nsIDOMDocument.h"
#include "nsTimer.h"
#include "nsIBaseWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeNode.h"
#include "nsIDocShellTreeOwner.h"
#include "nsCURILoader.h"

#include "nsILocaleService.h"
static NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID);
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

#ifdef XP_PC
#include <windows.h>
#endif

#include "nsIIOService.h"
#include "nsIURL.h"

//XXX for nsIPostData; this is wrong; we shouldn't see the nsIDocument type
#include "nsIDocument.h"

#ifdef DEBUG
#undef NOISY_LINKS
#undef NOISY_WEBSHELL_LEAKS
#else
#undef NOISY_LINKS
#undef NOISY_WEBSHELL_LEAKS
#endif

#define NOISY_WEBSHELL_LEAKS
#ifdef NOISY_WEBSHELL_LEAKS
#undef DETECT_WEBSHELL_LEAKS
#define DETECT_WEBSHELL_LEAKS
#endif

#ifdef NS_DEBUG
/**
 * Note: the log module is created during initialization which
 * means that you cannot perform logging before then.
 */
static PRLogModuleInfo* gLogModule = PR_NewLogModule("webshell");
#endif

#define WEB_TRACE_CALLS        0x1
#define WEB_TRACE_HISTORY      0x2

#define WEB_LOG_TEST(_lm,_bit) (PRIntn((_lm)->level) & (_bit))

#ifdef NS_DEBUG
#define WEB_TRACE(_bit,_args)            \
  PR_BEGIN_MACRO                         \
    if (WEB_LOG_TEST(gLogModule,_bit)) { \
      PR_LogPrint _args;                 \
    }                                    \
  PR_END_MACRO
#else
#define WEB_TRACE(_bit,_args)
#endif


#if OLD_EVENT_QUEUE
  /* The following is not used for the GTK version of the browser.
   * It is still lurking around  for Motif
   */
PLEventQueue* gWebShell_UnixEventQueue;

void nsWebShell_SetUnixEventQueue(PLEventQueue* aEventQueue)
{
  gWebShell_UnixEventQueue = aEventQueue;
}
#endif  /* OLD_EVENT_QUEUE  */


static NS_DEFINE_CID(kGlobalHistoryCID, NS_GLOBALHISTORY_CID);
static NS_DEFINE_CID(kIOServiceCID, NS_IOSERVICE_CID);

//----------------------------------------------------------------------

typedef enum {
   eCharsetReloadInit,
   eCharsetReloadRequested,
   eCharsetReloadStopOrigional
} eCharsetReloadState;

class nsWebShell : public nsDocShell,
                   public nsIWebShell,
                   public nsIWebShellContainer,
                   public nsIWebShellServices,
                   public nsILinkHandler,
                   public nsIDocumentLoaderObserver,
                   public nsIProgressEventSink, // should go away (nsIDocLoaderObs)
                   public nsIRefreshURI,
                   public nsIURIContentListener,
                   public nsIClipboardCommands
{
public:
  nsWebShell();
  virtual ~nsWebShell();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports
  NS_DECL_ISUPPORTS

  NS_DECL_NSIURICONTENTLISTENER

  // nsIInterfaceRequestor
  NS_DECL_NSIINTERFACEREQUESTOR

  // nsIContentViewerContainer
  NS_IMETHOD Embed(nsIContentViewer* aDocViewer,
                   const char* aCommand,
                   nsISupports* aExtraInfo);

  // nsIWebShell
  NS_IMETHOD Init(nsNativeWidget aNativeParent,
                  PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h,
                  nsScrollPreference aScrolling = nsScrollPreference_kAuto,
                  PRBool aAllowPlugins = PR_TRUE,
                  PRBool aIsSunkenBorder = PR_FALSE);
  NS_IMETHOD SizeToContent();
  NS_IMETHOD RemoveFocus();
  NS_IMETHOD SetContentViewer(nsIContentViewer* aViewer);
  NS_IMETHOD SetContainer(nsIWebShellContainer* aContainer);
  NS_IMETHOD GetContainer(nsIWebShellContainer*& aResult);
  NS_IMETHOD GetTopLevelWindow(nsIWebShellContainer** aWebShellWindow);
  NS_IMETHOD GetPrefs(nsIPref*& aPrefs);
  NS_IMETHOD GetRootWebShell(nsIWebShell*& aResult);
  NS_IMETHOD SetParent(nsIWebShell* aParent);
  NS_IMETHOD GetParent(nsIWebShell*& aParent);
  NS_IMETHOD GetReferrer(nsIURI **aReferrer);
  NS_IMETHOD GetChildCount(PRInt32& aResult);
  NS_IMETHOD AddChild(nsIWebShell* aChild);
  NS_IMETHOD RemoveChild(nsIWebShell* aChild);
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIWebShell*& aResult);
  NS_IMETHOD GetName(const PRUnichar** aName);
  NS_IMETHOD SetName(const PRUnichar* aName);
  NS_IMETHOD FindChildWithName(const PRUnichar* aName,
                               nsIWebShell*& aResult);

  NS_IMETHOD SetWebShellType(nsWebShellType aWebShellType);

  NS_IMETHOD GetScrolling(PRInt32& aScrolling);
  NS_IMETHOD SetScrolling(PRInt32 aScrolling, PRBool aSetCurrentAndInitial = PR_TRUE);

  // Document load api's
  NS_IMETHOD GetDocumentLoader(nsIDocumentLoader*& aResult);
  NS_IMETHOD LoadURL(const PRUnichar *aURLSpec,
                     nsIInputStream* aPostDataStream=nsnull,
                     PRBool aModifyHistory=PR_TRUE,
                     nsLoadFlags aType = nsIChannel::LOAD_NORMAL,
                     const PRUint32 localIP = 0,
                     nsISupports * aHistoryState = nsnull,
                     const PRUnichar* aReferrer=nsnull);
  NS_IMETHOD LoadURL(const PRUnichar *aURLSpec,
                     const char* aCommand,
                     nsIInputStream* aPostDataStream=nsnull,
                     PRBool aModifyHistory=PR_TRUE,
                     nsLoadFlags aType = nsIChannel::LOAD_NORMAL,
                     const PRUint32 localIP = 0,
                     nsISupports * aHistoryState=nsnull,
                     const PRUnichar* aReferrer=nsnull);

  NS_IMETHOD LoadURI(nsIURI * aUri,
                     const char * aCommand,
                     nsIInputStream* aPostDataStream=nsnull,
                     PRBool aModifyHistory=PR_TRUE,
                     nsLoadFlags aType = nsIChannel::LOAD_NORMAL,
                     const PRUint32 aLocalIP=0,
					           nsISupports * aHistoryState=nsnull,
                     const PRUnichar* aReferrer=nsnull);

  NS_IMETHOD Stop(void);
  NS_IMETHOD StopBeforeRequestingURL();
  NS_IMETHOD StopAfterURLAvailable();

  NS_IMETHOD Reload(nsLoadFlags aType);

  // History api's
  NS_IMETHOD Back(void);
  NS_IMETHOD CanBack(void);
  NS_IMETHOD Forward(void);
  NS_IMETHOD CanForward(void);
  NS_IMETHOD GoTo(PRInt32 aHistoryIndex);
  NS_IMETHOD GetHistoryLength(PRInt32& aResult);
  NS_IMETHOD GetHistoryIndex(PRInt32& aResult);
  NS_IMETHOD GetURL(PRInt32 aHistoryIndex, const PRUnichar** aURLResult);

  // nsIWebShellContainer
  NS_IMETHOD WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsLoadType aReason);
  NS_IMETHOD BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL);
  NS_IMETHOD ProgressLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aProgress, PRInt32 aProgressMax);
  NS_IMETHOD EndLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsresult aStatus);
  NS_IMETHOD NewWebShell(PRUint32 aChromeMask,
                         PRBool aVisible,
                         nsIWebShell *&aNewWebShell);
  NS_IMETHOD ContentShellAdded(nsIWebShell* aChildShell, nsIContent* frameNode);
  NS_IMETHOD CreatePopup(nsIDOMElement* aElement, nsIDOMElement* aPopupContent,
                         PRInt32 aXPos, PRInt32 aYPos,
                         const nsString& aPopupType, const nsString& anAnchorAlignment,
                         const nsString& aPopupAlignment,
                         nsIDOMWindow* aWindow, nsIDOMWindow** outPopup);
  NS_IMETHOD FindWebShellWithName(const PRUnichar* aName, nsIWebShell*& aResult);
  NS_IMETHOD FocusAvailable(nsIWebShell* aFocusedWebShell, PRBool& aFocusTaken);
  NS_IMETHOD GetHistoryState(nsISupports** aLayoutHistoryState);
  NS_IMETHOD SetHistoryState(nsISupports* aLayoutHistoryState);

  // nsIWebShellServices
  NS_IMETHOD LoadDocument(const char* aURL,
                          const char* aCharset= nsnull ,
                          nsCharsetSource aSource = kCharsetUninitialized);
  NS_IMETHOD ReloadDocument(const char* aCharset= nsnull ,
                            nsCharsetSource aSource = kCharsetUninitialized,
                            const char* aCmd=nsnull);
  NS_IMETHOD StopDocumentLoad(void);
  NS_IMETHOD SetRendering(PRBool aRender);

  // nsILinkHandler
  NS_IMETHOD OnLinkClick(nsIContent* aContent,
                         nsLinkVerb aVerb,
                         const PRUnichar* aURLSpec,
                         const PRUnichar* aTargetSpec,
                         nsIInputStream* aPostDataStream = 0);
  NS_IMETHOD OnOverLink(nsIContent* aContent,
                        const PRUnichar* aURLSpec,
                        const PRUnichar* aTargetSpec);
  NS_IMETHOD GetLinkState(const PRUnichar* aURLSpec, nsLinkState& aState);

  // nsIScriptGlobalObjectOwner
  NS_DECL_NSISCRIPTGLOBALOBJECTOWNER

  // nsIDocumentLoaderObserver
  NS_IMETHOD OnStartDocumentLoad(nsIDocumentLoader* loader,
                                 nsIURI* aURL,
                                 const char* aCommand);
  NS_IMETHOD OnEndDocumentLoad(nsIDocumentLoader* loader,
                               nsIChannel* channel,
                               nsresult aStatus,
                               nsIDocumentLoaderObserver * );
  NS_IMETHOD OnStartURLLoad(nsIDocumentLoader* loader,
                            nsIChannel* channel,
                            nsIContentViewer* aViewer);
  NS_IMETHOD OnProgressURLLoad(nsIDocumentLoader* loader,
                               nsIChannel* channel, PRUint32 aProgress,
                               PRUint32 aProgressMax);
  NS_IMETHOD OnStatusURLLoad(nsIDocumentLoader* loader,
                             nsIChannel* channel, nsString& aMsg);
  NS_IMETHOD OnEndURLLoad(nsIDocumentLoader* loader,
                          nsIChannel* channel, nsresult aStatus);
  NS_IMETHOD HandleUnknownContentType(nsIDocumentLoader* loader,
                                      nsIChannel* channel,
                                      const char *aContentType,
                                      const char *aCommand );


  NS_IMETHOD RefreshURL(const char* aURL, PRInt32 millis, PRBool repeat);

  // nsIRefreshURL interface methods...
  NS_IMETHOD RefreshURI(nsIURI* aURI, PRInt32 aMillis, PRBool aRepeat);
  NS_IMETHOD CancelRefreshURITimers(void);

  // nsIProgressEventSink
  NS_DECL_NSIPROGRESSEVENTSINK

  // nsIClipboardCommands
  NS_IMETHOD CanCutSelection  (PRBool* aResult);
  NS_IMETHOD CanCopySelection (PRBool* aResult);
  NS_IMETHOD CanPasteSelection(PRBool* aResult);

  NS_IMETHOD CutSelection  (void);
  NS_IMETHOD CopySelection (void);
  NS_IMETHOD PasteSelection(void);

  NS_IMETHOD SelectAll(void);
  NS_IMETHOD SelectNone(void);

  NS_IMETHOD FindNext(const PRUnichar * aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound);

  // nsIBaseWindow
  NS_DECL_NSIBASEWINDOW

  // nsIDocShell
  NS_DECL_NSIDOCSHELL

  // nsWebShell
  nsIEventQueue* GetEventQueue(void);
  void HandleLinkClickEvent(nsIContent *aContent,
                            nsLinkVerb aVerb,
                            const PRUnichar* aURLSpec,
                            const PRUnichar* aTargetSpec,
                            nsIInputStream* aPostDataStream = 0);

  void ShowHistory();

  nsIWebShell* GetTarget(const PRUnichar* aName);
  nsIBrowserWindow* GetBrowserWindow(void);

  static void RefreshURLCallback(nsITimer* aTimer, void* aClosure);

  static nsEventStatus PR_CALLBACK HandleEvent(nsGUIEvent *aEvent);

  nsresult CreatePluginHost(PRBool aAllowPlugins);
  nsresult DestroyPluginHost(void);

  NS_IMETHOD IsBusy(PRBool& aResult);

  NS_IMETHOD SetSessionHistory(nsISessionHistory * aSHist);
  NS_IMETHOD GetSessionHistory(nsISessionHistory *& aResult);
  NS_IMETHOD SetIsInSHist(PRBool aIsFrame);
  NS_IMETHOD GetIsInSHist(PRBool& aIsFrame);
  NS_IMETHOD GetURL(const PRUnichar** aURL);
  NS_IMETHOD SetURL(const PRUnichar* aURL);

protected:
  void GetRootWebShellEvenIfChrome(nsIWebShell** aResult);
  void InitFrameData(PRBool aCompleteInitScrolling);
  nsresult CheckForTrailingSlash(nsIURI* aURL);
  nsresult GetViewManager(nsIViewManager* *viewManager);

  nsIEventQueue* mThreadEventQueue;
  nsIScriptGlobalObject *mScriptGlobal;
  nsIScriptContext* mScriptContext;

  nsIWebShellContainer* mContainer;
  nsIDeviceContext* mDeviceContext;
  nsIPref* mPrefs;
  nsIWidget* mWindow;
  nsIDocumentLoader* mDocLoader;

  nsString mDefaultCharacterSet;


  nsVoidArray mHistory;
  PRInt32 mHistoryIndex;


  nsIGlobalHistory* mHistoryService;
  nsISessionHistory * mSHist;

  nsString mURL;
  nsString mReferrer;

  nsString mOverURL;
  nsString mOverTarget;

  PRPackedBool mIsInSHist;
  PRPackedBool mFailedToLoadHistoryService;

  nsScrollPreference mScrollPref;

  PRInt32 mScrolling[2];
  nsVoidArray mRefreshments;

  eCharsetReloadState mCharsetReloadState;

  nsISupports* mHistoryState; // Weak reference.  Session history owns this.

  void ReleaseChildren();
  NS_IMETHOD DestroyChildren();
  nsresult CreateScriptEnvironment();
  nsresult DoLoadURL(nsIURI * aUri, 
                     const char* aCommand,
                     nsIInputStream* aPostDataStream,
                     nsLoadFlags aType,
                     const PRUint32 aLocalIP,
                     const PRUnichar* aReferrer,
                     PRBool aKickOffLoad = PR_TRUE);

  nsresult PrepareToLoadURI(nsIURI * aUri, 
                            const char * aCommand,
                            nsIInputStream * aPostStream,
                            PRBool aModifyHistory,
                            nsLoadFlags aType,
                            const PRUint32 aLocalIP,
                            nsISupports * aHistoryState,
                            const PRUnichar * aReferrer);
  float mZoom;

  static nsIPluginHost    *mPluginHost;
  static nsIPluginManager *mPluginManager;
  static PRUint32          mPluginInitCnt;
  PRBool mProcessedEndDocumentLoad;

  PRBool mViewSource;
  
  // if there is no mWindow, this will keep track of the bounds  --dwc0001
  nsRect  mBounds;

  MOZ_TIMER_DECLARE(mTotalTime)

#ifdef DETECT_WEBSHELL_LEAKS
private:
  // We're counting the number of |nsWebShells| to help find leaks
  static unsigned long gNumberOfWebShells;

public:
  static unsigned long TotalWebShellsInExistence() { return gNumberOfWebShells; }
#endif
};

#ifdef DETECT_WEBSHELL_LEAKS
unsigned long nsWebShell::gNumberOfWebShells = 0;

extern "C" NS_WEB
unsigned long
NS_TotalWebShellsInExistence()
{
  return nsWebShell::TotalWebShellsInExistence();
}
#endif

//----------------------------------------------------------------------

// Class IID's
static NS_DEFINE_IID(kEventQueueServiceCID,   NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kChildCID,               NS_CHILD_CID);
static NS_DEFINE_IID(kDeviceContextCID,       NS_DEVICE_CONTEXT_CID);
static NS_DEFINE_IID(kDocLoaderServiceCID,    NS_DOCUMENTLOADER_SERVICE_CID);
static NS_DEFINE_IID(kWebShellCID,            NS_WEB_SHELL_CID);

// IID's
static NS_DEFINE_IID(kIContentViewerContainerIID,
                     NS_ICONTENT_VIEWER_CONTAINER_IID);
static NS_DEFINE_IID(kIDocumentLoaderObserverIID,
                     NS_IDOCUMENT_LOADER_OBSERVER_IID);
static NS_DEFINE_IID(kIProgressEventSinkIID,  NS_IPROGRESSEVENTSINK_IID);
static NS_DEFINE_IID(kIDeviceContextIID,      NS_IDEVICE_CONTEXT_IID);
static NS_DEFINE_IID(kIDocumentLoaderIID,     NS_IDOCUMENTLOADER_IID);
static NS_DEFINE_IID(kIFactoryIID,            NS_IFACTORY_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kRefreshURIIID,          NS_IREFRESHURI_IID);

static NS_DEFINE_IID(kIWidgetIID,             NS_IWIDGET_IID);
static NS_DEFINE_IID(kIPluginManagerIID,      NS_IPLUGINMANAGER_IID);
static NS_DEFINE_IID(kIPluginHostIID,         NS_IPLUGINHOST_IID);
static NS_DEFINE_IID(kCPluginManagerCID,      NS_PLUGINMANAGER_CID);
static NS_DEFINE_IID(kIDocumentViewerIID,     NS_IDOCUMENT_VIEWER_IID);
static NS_DEFINE_IID(kITimerCallbackIID,      NS_ITIMERCALLBACK_IID);
static NS_DEFINE_IID(kIWebShellContainerIID,  NS_IWEB_SHELL_CONTAINER_IID);
static NS_DEFINE_IID(kIBrowserWindowIID,      NS_IBROWSER_WINDOW_IID);
static NS_DEFINE_IID(kIClipboardCommandsIID,  NS_ICLIPBOARDCOMMANDS_IID);
static NS_DEFINE_IID(kIEventQueueServiceIID,  NS_IEVENTQUEUESERVICE_IID);
static NS_DEFINE_IID(kISessionHistoryIID,     NS_ISESSIONHISTORY_IID);
static NS_DEFINE_IID(kIDOMHTMLDocumentIID,    NS_IDOMHTMLDOCUMENT_IID);
static NS_DEFINE_CID(kCDOMRangeCID,           NS_RANGE_CID);
// XXX not sure
static NS_DEFINE_IID(kILinkHandlerIID,        NS_ILINKHANDLER_IID);

nsIPluginHost *nsWebShell::mPluginHost = nsnull;
nsIPluginManager *nsWebShell::mPluginManager = nsnull;
PRUint32 nsWebShell::mPluginInitCnt = 0;

nsresult nsWebShell::CreatePluginHost(PRBool aAllowPlugins)
{
  nsresult rv = NS_OK;

  if ((PR_TRUE == aAllowPlugins) && (0 == mPluginInitCnt))
  {
    if (nsnull == mPluginManager)
    {
      // use the service manager to obtain the plugin manager.
      rv = nsServiceManager::GetService(kCPluginManagerCID, kIPluginManagerIID,
                                        (nsISupports**)&mPluginManager);
      if (NS_OK == rv)
      {
        if (NS_OK == mPluginManager->QueryInterface(kIPluginHostIID,
                                                    (void **)&mPluginHost))
        {
          mPluginHost->Init();
        }
      }
    }
  }

  mPluginInitCnt++;

  return rv;
}

nsresult nsWebShell::DestroyPluginHost(void)
{
  mPluginInitCnt--;

  NS_ASSERTION(!(mPluginInitCnt < 0), "underflow in plugin host destruction");

  if (0 == mPluginInitCnt)
  {
    if (nsnull != mPluginHost) {
      mPluginHost->Destroy();
      mPluginHost->Release();
      mPluginHost = NULL;
    }

    // use the service manager to release the plugin manager.
    if (nsnull != mPluginManager) {
      nsServiceManager::ReleaseService(kCPluginManagerCID, mPluginManager);
      mPluginManager = NULL;
    }
  }

  return NS_OK;
}

//----------------------------------------------------------------------

// Note: operator new zeros our memory
nsWebShell::nsWebShell() : nsDocShell()
{
#ifdef DETECT_WEBSHELL_LEAKS
  // We're counting the number of |nsWebShells| to help find leaks
  ++gNumberOfWebShells;
#endif
#ifdef NOISY_WEBSHELL_LEAKS
  printf("WEBSHELL+ = %d\n", gNumberOfWebShells);
#endif

  NS_INIT_REFCNT();
  mHistoryIndex = -1;
  mScrollPref = nsScrollPreference_kAuto;
  mScriptGlobal = nsnull;
  mScriptContext = nsnull;
  mThreadEventQueue = nsnull;
  InitFrameData(PR_TRUE);
  mItemType = typeContent;
  mSHist = nsnull;
  mIsInSHist = PR_FALSE;
  mFailedToLoadHistoryService = PR_FALSE;
  mDefaultCharacterSet = "";
  mProcessedEndDocumentLoad = PR_FALSE;
  mCharsetReloadState = eCharsetReloadInit;
  mViewSource=PR_FALSE;
  mHistoryService = nsnull;
  mHistoryState = nsnull;
}

nsWebShell::~nsWebShell()
{
  if (nsnull != mHistoryService) {
    nsServiceManager::ReleaseService(kGlobalHistoryCID, mHistoryService);
    mHistoryService = nsnull;
  }

  // Stop any pending document loads and destroy the loader...
  if (nsnull != mDocLoader) {
    mDocLoader->Stop();
    mDocLoader->RemoveObserver((nsIDocumentLoaderObserver*)this);
    mDocLoader->SetContainer(nsnull);
    NS_RELEASE(mDocLoader);
  }
  // Cancel any timers that were set for this loader.
  CancelRefreshURITimers();

  ++mRefCnt; // following releases can cause this destructor to be called
             // recursively if the refcount is allowed to remain 0

  NS_IF_RELEASE(mSHist);
  NS_IF_RELEASE(mWindow);
  NS_IF_RELEASE(mThreadEventQueue);
  mContentViewer=nsnull;
  NS_IF_RELEASE(mDeviceContext);
  NS_IF_RELEASE(mPrefs);
  NS_IF_RELEASE(mContainer);

  if (nsnull != mScriptGlobal) {
    mScriptGlobal->SetWebShell(nsnull);
    NS_RELEASE(mScriptGlobal);
  }
  if (nsnull != mScriptContext) {
    mScriptContext->SetOwner(nsnull);
    NS_RELEASE(mScriptContext);
  }

  InitFrameData(PR_TRUE);

  // XXX Because we hold references to the children and they hold references
  // to us we never get destroyed. See Destroy() instead...
#if 0
  // Release references on our children
  ReleaseChildren();
#endif


  // Free up history memory
  PRInt32 i, n = mHistory.Count();
  for (i = 0; i < n; i++) {
    nsString* s = (nsString*) mHistory.ElementAt(i);
    delete s;
  }


  DestroyPluginHost();

#ifdef DETECT_WEBSHELL_LEAKS
  // We're counting the number of |nsWebShells| to help find leaks
  --gNumberOfWebShells;
#endif
#ifdef NOISY_WEBSHELL_LEAKS
  printf("WEBSHELL- = %d\n", gNumberOfWebShells);
#endif
}

void nsWebShell::InitFrameData(PRBool aCompleteInitScrolling)
{
  if (aCompleteInitScrolling) {
    mScrolling[0] = -1;
    mScrolling[1] = -1;
    SetMarginWidth(-1);    
    SetMarginHeight(-1);
  }
  else {
    mScrolling[1] = mScrolling[0];
  }
}

void
nsWebShell::ReleaseChildren()
{
  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) {
    nsCOMPtr<nsIDocShellTreeItem> shell = dont_AddRef((nsIDocShellTreeItem*)mChildren.ElementAt(i));
    shell->SetParent(nsnull);

    //Break circular reference of webshell to contentviewer
    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(shell));
    webShell->SetContentViewer(nsnull);
  }
  mChildren.Clear();
}

NS_IMETHODIMP
nsWebShell::DestroyChildren()
{
  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) {
    nsIDocShellTreeItem * shell = (nsIDocShellTreeItem*) mChildren.ElementAt(i);
    shell->SetParent(nsnull);
    nsCOMPtr<nsIBaseWindow> shellWin(do_QueryInterface(shell));
    shellWin->Destroy();
    NS_RELEASE(shell);

  }
  mChildren.Clear();
  return NS_OK;
}


NS_IMPL_ADDREF(nsWebShell)
NS_IMPL_RELEASE(nsWebShell)

NS_INTERFACE_MAP_BEGIN(nsWebShell)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebShell)
   NS_INTERFACE_MAP_ENTRY(nsIWebShell)
   NS_INTERFACE_MAP_ENTRY(nsIWebShellServices)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIContentViewerContainer, nsIWebShell)
   NS_INTERFACE_MAP_ENTRY(nsIScriptGlobalObjectOwner)
   NS_INTERFACE_MAP_ENTRY(nsIDocumentLoaderObserver)
   NS_INTERFACE_MAP_ENTRY(nsIProgressEventSink)
   NS_INTERFACE_MAP_ENTRY(nsIWebShellContainer)
   NS_INTERFACE_MAP_ENTRY(nsILinkHandler)
   NS_INTERFACE_MAP_ENTRY(nsIRefreshURI)
   NS_INTERFACE_MAP_ENTRY(nsIClipboardCommands)
   NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
   NS_INTERFACE_MAP_ENTRY(nsIURIContentListener)
   NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
   NS_INTERFACE_MAP_ENTRY(nsIDocShell)
   NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeItem)
   NS_INTERFACE_MAP_ENTRY(nsIDocShellTreeNode)
NS_INTERFACE_MAP_END

NS_IMETHODIMP
nsWebShell::GetInterface(const nsIID &aIID, void** aInstancePtr)
{
   NS_ENSURE_ARG_POINTER(aInstancePtr);

   if(aIID.Equals(NS_GET_IID(nsILinkHandler)))
      {
      *aInstancePtr = NS_STATIC_CAST(nsILinkHandler*, this);
      NS_ADDREF((nsISupports*)*aInstancePtr);
      return NS_OK;
      }
   else if(aIID.Equals(NS_GET_IID(nsIScriptGlobalObjectOwner)))
      {
      *aInstancePtr = NS_STATIC_CAST(nsIScriptGlobalObjectOwner*, this);
      NS_ADDREF((nsISupports*)*aInstancePtr);
      return NS_OK;
      }
   else if (aIID.Equals(NS_GET_IID(nsIURIContentListener)))
   {
      *aInstancePtr = NS_STATIC_CAST(nsIURIContentListener*, this);
      NS_ADDREF((nsISupports*)*aInstancePtr);
      return NS_OK;
   }
   else if(aIID.Equals(NS_GET_IID(nsIScriptGlobalObject)))
      {
      NS_ENSURE_SUCCESS(CreateScriptEnvironment(), NS_ERROR_FAILURE);
      *aInstancePtr = mScriptGlobal;
      NS_ADDREF((nsISupports*)*aInstancePtr);
      return NS_OK;
      }
   else if(mPluginManager) //XXX this seems a little wrong. MMP
      return mPluginManager->QueryInterface(aIID, aInstancePtr);

  return QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsWebShell::Embed(nsIContentViewer* aContentViewer,
                  const char* aCommand,
                  nsISupports* aExtraInfo)
{
  nsresult rv;
  nsRect bounds;

  WEB_TRACE(WEB_TRACE_CALLS,
      ("nsWebShell::Embed: this=%p aDocViewer=%p aCommand=%s aExtraInfo=%p",
       this, aContentViewer, aCommand ? aCommand : "", aExtraInfo));

  if (mContentViewer) // && (eCharsetReloadInit!=mCharsetReloadState))
  { // get any interesting state from the old content viewer
    // XXX: it would be far better to just reuse the document viewer ,
    //      since we know we're just displaying the same document as before
    PRUnichar *defaultCharset=nsnull;
    PRUnichar *forceCharset=nsnull;
    PRUnichar *hintCharset=nsnull;
    PRInt32 hintCharsetSource;

    nsCOMPtr<nsIMarkupDocumentViewer> oldMUDV = do_QueryInterface(mContentViewer);
    nsCOMPtr<nsIMarkupDocumentViewer> newMUDV = do_QueryInterface(aContentViewer);
    if (oldMUDV && newMUDV)
    {
      NS_ENSURE_SUCCESS(oldMUDV->GetDefaultCharacterSet (&defaultCharset), NS_ERROR_FAILURE);
      NS_ENSURE_SUCCESS(oldMUDV->GetForceCharacterSet (&forceCharset), NS_ERROR_FAILURE);
      NS_ENSURE_SUCCESS(oldMUDV->GetHintCharacterSet (&hintCharset), NS_ERROR_FAILURE);
      NS_ENSURE_SUCCESS(oldMUDV->GetHintCharacterSetSource (&hintCharsetSource), NS_ERROR_FAILURE);

      // set the old state onto the new content viewer
      NS_ENSURE_SUCCESS(newMUDV->SetDefaultCharacterSet (defaultCharset), NS_ERROR_FAILURE);
      NS_ENSURE_SUCCESS(newMUDV->SetForceCharacterSet (forceCharset), NS_ERROR_FAILURE);
      NS_ENSURE_SUCCESS(newMUDV->SetHintCharacterSet (hintCharset), NS_ERROR_FAILURE);
      NS_ENSURE_SUCCESS(newMUDV->SetHintCharacterSetSource (hintCharsetSource), NS_ERROR_FAILURE);

      if (defaultCharset) {
        Recycle(defaultCharset);
      }
      if (forceCharset) {
        Recycle(forceCharset);
      }
      if (hintCharset) {
        Recycle(hintCharset);
      }
    }
  }
  mContentViewer = nsnull;
  if (nsnull != mScriptContext) {
    mScriptContext->GC();
  }
  mContentViewer = aContentViewer;

  // check to see if we have a window to embed into --dwc0001
  /* Note we also need to check for the presence of a native widget. If the
     webshell is hidden before it's embedded, which can happen in an onload
     handler, the native widget is destroyed before this code is run.  This
     appears to be mostly harmless except on Windows, where the subsequent
     attempt to create a child window without a parent is met with disdain
     by the OS. It's handy, then, that GetNativeData on Windows returns
     null in this case. */
  if(mWindow && mWindow->GetNativeData(NS_NATIVE_WIDGET)) {
    mWindow->GetClientBounds(bounds);
    bounds.x = bounds.y = 0;
    rv = mContentViewer->Init(mWindow->GetNativeData(NS_NATIVE_WIDGET),
                              mDeviceContext,
                              mPrefs,
                              bounds,
                              mScrollPref);

    // If the history state has been set by session history,
    // set it on the pres shell now that we have a content
    // viewer.
    if (mContentViewer && mHistoryState) {
      nsCOMPtr<nsIDocumentViewer> docv = do_QueryInterface(mContentViewer);
      if (nsnull != docv) {
        nsCOMPtr<nsIPresShell> shell;
        rv = docv->GetPresShell(*getter_AddRefs(shell));
        if (NS_SUCCEEDED(rv)) {
          rv = shell->SetHistoryState((nsILayoutHistoryState*) mHistoryState);
        }
      }
    }

    if (NS_SUCCEEDED(rv)) {
      mContentViewer->Show();
    }
  } else {
    mContentViewer = nsnull;
  }

  // Now that we have switched documents, forget all of our children
  DestroyChildren();

  return rv;
}

NS_IMETHODIMP
nsWebShell::HandleUnknownContentType(nsIDocumentLoader* loader,
                                     nsIChannel* channel,
                                     const char *aContentType,
                                     const char *aCommand ) {
    // If we have a doc loader observer, let it respond to this.
    return mDocLoaderObserver ? mDocLoaderObserver->HandleUnknownContentType( mDocLoader, channel, aContentType, aCommand )
                              : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWebShell::Init(nsNativeWidget aNativeParent,
                 PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h,
                 nsScrollPreference aScrolling,
                 PRBool aAllowPlugins,
                 PRBool aIsSunkenBorder)
{
  nsresult rv = NS_OK;

  // Cache the PL_EventQueue of the current UI thread...
  //
  // Since this call must be made on the UI thread, we know the Event Queue
  // will be associated with the current thread...
  //
  NS_WITH_SERVICE(nsIEventQueueService, eventService, kEventQueueServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = eventService->GetThreadEventQueue(NS_CURRENT_THREAD, &mThreadEventQueue);
  if (NS_FAILED(rv)) return rv;

  //XXX make sure plugins have started up. this really needs to
  //be associated with the nsIContentViewerContainer interfaces,
  //not the nsIWebShell interfaces. this is a hack. MMP
  nsRect aBounds(x,y,w,h);
  mBounds.SetRect(x,y,w,h);     // initialize the webshells bounds --dwc0001
  nsWidgetInitData  widgetInit;

  CreatePluginHost(aAllowPlugins);

  //mScrollPref = aScrolling;

  WEB_TRACE(WEB_TRACE_CALLS,
            ("nsWebShell::Init: this=%p", this));

/*  it is ok to have a webshell without an aNativeParent (used later to create the mWindow --dwc0001
  // Initial error checking...
  NS_PRECONDITION(nsnull != aNativeParent, "null Parent Window");
  if (nsnull == aNativeParent) {
    rv = NS_ERROR_NULL_POINTER;
    goto done;
  }
*/
  // Create a document loader...
  nsCOMPtr<nsIWebShell> webShellParent(do_QueryInterface(mParent));
  if (webShellParent) {
    nsIDocumentLoader* parentLoader;

    // Create a child document loader...
    rv = webShellParent->GetDocumentLoader(parentLoader);
    if (NS_SUCCEEDED(rv)) {
      rv = parentLoader->CreateDocumentLoader(&mDocLoader);
      NS_RELEASE(parentLoader);
    }
  } else {
    NS_WITH_SERVICE(nsIDocumentLoader, docLoaderService, kDocLoaderServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;


    rv = docLoaderService->CreateDocumentLoader(&mDocLoader);
    if (NS_FAILED(rv)) return rv;
  }

  // Set the webshell as the default IContentViewerContainer for the loader...
  mDocLoader->SetContainer(NS_STATIC_CAST(nsIContentViewerContainer*, (nsIWebShell*)this));

  //Register ourselves as an observer for the new doc loader
  mDocLoader->AddObserver((nsIDocumentLoaderObserver*)this);


  /* this is the old code, commented out for the new code below while I figure this out -- dwc0001
  // Create device context
  rv = nsComponentManager::CreateInstance(kDeviceContextCID, nsnull,
                                    kIDeviceContextIID,
                                    (void **)&mDeviceContext);
  if (NS_FAILED(rv)) {
    goto done;
  }
  mDeviceContext->Init(aNativeParent);
  float dev2twip;
  mDeviceContext->GetDevUnitsToTwips(dev2twip);
  mDeviceContext->SetDevUnitsToAppUnits(dev2twip);
  float twip2dev;
  mDeviceContext->GetTwipsToDevUnits(twip2dev);
  mDeviceContext->SetAppUnitsToDevUnits(twip2dev);
//  mDeviceContext->SetGamma(1.7f);
  mDeviceContext->SetGamma(1.0f);

  // Create a Native window for the shell container...
  rv = nsComponentManager::CreateInstance(kChildCID, nsnull, kIWidgetIID, (void**)&mWindow);
  if (NS_FAILED(rv)) return rv;

  widgetInit.clipChildren = PR_FALSE;
  widgetInit.mWindowType = eWindowType_child;
  //widgetInit.mBorderStyle = aIsSunkenBorder ? eBorderStyle_3DChildWindow : eBorderStyle_none;
  mWindow->Create(aNativeParent, aBounds, nsWebShell::HandleEvent,
                  mDeviceContext, nsnull, nsnull, &widgetInit);
*/

  // Create device context
  if (nsnull != aNativeParent) {
    rv = nsComponentManager::CreateInstance(kDeviceContextCID, nsnull,
                                            kIDeviceContextIID,
                                            (void **)&mDeviceContext);
    if (NS_FAILED(rv)) return rv;
    mDeviceContext->Init(aNativeParent);
    float dev2twip;
    mDeviceContext->GetDevUnitsToTwips(dev2twip);
    mDeviceContext->SetDevUnitsToAppUnits(dev2twip);
    float twip2dev;
    mDeviceContext->GetTwipsToDevUnits(twip2dev);
    mDeviceContext->SetAppUnitsToDevUnits(twip2dev);
    //  mDeviceContext->SetGamma(1.7f);
    mDeviceContext->SetGamma(1.0f);

    // Create a Native window for the shell container...
    rv = nsComponentManager::CreateInstance(kChildCID, nsnull, kIWidgetIID, (void**)&mWindow);
    if (NS_FAILED(rv)) return rv;

    widgetInit.clipChildren = PR_FALSE;
    widgetInit.mWindowType = eWindowType_child;
    //widgetInit.mBorderStyle = aIsSunkenBorder ? eBorderStyle_3DChildWindow : eBorderStyle_none;
    mWindow->Create(aNativeParent, aBounds, nsWebShell::HandleEvent,
                    mDeviceContext, nsnull, nsnull, &widgetInit);
  } else {
    // we need a deviceContext

    rv = nsComponentManager::CreateInstance(kDeviceContextCID, nsnull,kIDeviceContextIID,(void **)&mDeviceContext);
    if (NS_FAILED(rv)) return rv;
    mDeviceContext->Init(aNativeParent);
    float dev2twip;
    mDeviceContext->GetDevUnitsToTwips(dev2twip);
    mDeviceContext->SetDevUnitsToAppUnits(dev2twip);
    float twip2dev;
    mDeviceContext->GetTwipsToDevUnits(twip2dev);
    mDeviceContext->SetAppUnitsToDevUnits(twip2dev);
    mDeviceContext->SetGamma(1.0f);

    widgetInit.clipChildren = PR_FALSE;
    widgetInit.mWindowType = eWindowType_child;
  }

  // Set the language portion of the user agent. :)
  NS_WITH_SERVICE(nsILocaleService, localeServ, kLocaleServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  PRUnichar *UALang;
  rv = localeServ->GetLocaleComponentForUserAgent(&UALang);
  if (NS_FAILED(rv)) return rv;

  NS_WITH_SERVICE(nsIIOService, ioServ, kIOServiceCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = ioServ->SetLanguage(UALang);
  nsAllocator::Free(UALang);
  return rv;
}


NS_IMETHODIMP
nsWebShell::IsBusy(PRBool& aResult)
{

  if (mDocLoader!=nsnull) {
    mDocLoader->IsBusy(aResult);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SizeToContent()
{
  nsresult rv;

  // get the presentation shell
  nsCOMPtr<nsIContentViewer> cv;
  GetContentViewer(getter_AddRefs(cv));
  if (cv) {
    nsCOMPtr<nsIDocumentViewer> dv = do_QueryInterface(cv);
    if (dv) {
      nsCOMPtr<nsIPresContext> pcx;
      dv->GetPresContext(*getter_AddRefs(pcx));
      if (pcx) {
        nsCOMPtr<nsIPresShell> pshell;
        pcx->GetShell(getter_AddRefs(pshell));

        // whew! so resize the presentation shell
        if (pshell) {
          nsRect  shellArea;
          PRInt32 width, height;
          float   pixelScale;

          rv = pshell->ResizeReflow(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);

          // so how big is it?
          pcx->GetVisibleArea(shellArea);
          pcx->GetTwipsToPixels(&pixelScale);
          width = PRInt32((float)shellArea.width*pixelScale);
          height = PRInt32((float)shellArea.height*pixelScale);

          // if we're the outermost webshell for this window, size the window
          if (mContainer) {
            nsCOMPtr<nsIBrowserWindow> browser = do_QueryInterface(mContainer);
            if (browser) {
              nsCOMPtr<nsIWebShell> browserWebShell;
              PRInt32 oldX, oldY, oldWidth, oldHeight,
                      widthDelta, heightDelta;
              nsRect  windowBounds;

              GetPositionAndSize(&oldX, &oldY, &oldWidth, &oldHeight);
              widthDelta = width - oldWidth;
              heightDelta = height - oldHeight;
              browser->GetWindowBounds(windowBounds);
              browser->SizeWindowTo(windowBounds.width + widthDelta,
                                    windowBounds.height + heightDelta);
            }
          }
        }
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
nsWebShell::RemoveFocus()
{
  /*
  --dwc0001
  NS_PRECONDITION(nsnull != mWindow, "null window");
  */

  if (nsnull != mWindow) {
    nsIWidget *parentWidget = mWindow->GetParent();
    if (nsnull != parentWidget) {
      parentWidget->SetFocus();
      NS_RELEASE(parentWidget);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetContentViewer(nsIContentViewer* aViewer)
{
  mContentViewer = aViewer;
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetContainer(nsIWebShellContainer* aContainer)
{
  NS_IF_RELEASE(mContainer);
  mContainer = aContainer;
  NS_IF_ADDREF(mContainer);

  // uri dispatching change.....if you set a container for a webshell
  // and that container is a content listener itself....then use
  // it as our parent container. 
  nsresult rv = NS_OK;
  nsCOMPtr<nsIURIContentListener> contentListener = do_QueryInterface(mContainer, &rv);
  if (NS_SUCCEEDED(rv) && contentListener)
    SetParentURIContentListener(contentListener);

  // if the container is getting set to null, then our parent must be going away
  // so clear out our knowledge of the content listener represented by the container
  if (!aContainer)
    SetParentURIContentListener(nsnull);

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetContainer(nsIWebShellContainer*& aResult)
{
  aResult = mContainer;
  NS_IF_ADDREF(mContainer);
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetTopLevelWindow(nsIWebShellContainer** aTopLevelWindow)
{
   NS_ENSURE_ARG_POINTER(aTopLevelWindow);
   *aTopLevelWindow = nsnull;

   nsCOMPtr<nsIWebShell> rootWebShell;

   GetRootWebShellEvenIfChrome(getter_AddRefs(rootWebShell));
   if(!rootWebShell)
      return NS_OK;
   
   nsCOMPtr<nsIWebShellContainer> rootContainer;
   rootWebShell->GetContainer(*aTopLevelWindow);

   return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetSessionHistory(nsISessionHistory* aSHist)
{
  NS_IF_RELEASE(mSHist);
  mSHist = aSHist;
  NS_IF_ADDREF(aSHist);
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetSessionHistory(nsISessionHistory *& aResult)
{
  aResult = mSHist;
  NS_IF_ADDREF(mSHist);
  return NS_OK;
}

nsEventStatus PR_CALLBACK
nsWebShell::HandleEvent(nsGUIEvent *aEvent)
{
  return nsEventStatus_eIgnore;
}

NS_IMETHODIMP
nsWebShell::GetPrefs(nsIPref*& aPrefs)
{
  aPrefs = mPrefs;
  NS_IF_ADDREF(aPrefs);
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetRootWebShell(nsIWebShell*& aResult)
{
   nsCOMPtr<nsIDocShellTreeItem> top;
   GetSameTypeRootTreeItem(getter_AddRefs(top));
   nsCOMPtr<nsIWebShell> topAsWebShell(do_QueryInterface(top));
   aResult = topAsWebShell;
   NS_IF_ADDREF(aResult);
   return NS_OK;
}

void
nsWebShell::GetRootWebShellEvenIfChrome(nsIWebShell** aResult)
{
   nsCOMPtr<nsIDocShellTreeItem> top;
   GetRootTreeItem(getter_AddRefs(top));
   nsCOMPtr<nsIWebShell> topAsWebShell(do_QueryInterface(top));
   *aResult = topAsWebShell;
   NS_IF_ADDREF(*aResult);
}

NS_IMETHODIMP
nsWebShell::SetParent(nsIWebShell* aParent)
{
   nsCOMPtr<nsIDocShellTreeItem> parentAsTreeItem(do_QueryInterface(aParent));

   mParent = parentAsTreeItem.get();
   return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetParent(nsIWebShell*& aParent)
{
   nsCOMPtr<nsIDocShellTreeItem> parent;
   NS_ENSURE_SUCCESS(GetSameTypeParent(getter_AddRefs(parent)), NS_ERROR_FAILURE);

   if(parent)
      parent->QueryInterface(NS_GET_IID(nsIWebShell), (void**)&aParent);
   else
      aParent = nsnull;
   return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetReferrer(nsIURI **aReferrer)
{
  return NS_NewURI(aReferrer, mReferrer, nsnull);
}

NS_IMETHODIMP
nsWebShell::GetChildCount(PRInt32& aResult)
{
   return nsDocShell::GetChildCount(&aResult);
}

NS_IMETHODIMP
nsWebShell::AddChild(nsIWebShell* aChild)
{
   NS_ENSURE_ARG(aChild);

   nsCOMPtr<nsIDocShellTreeItem> treeItemChild(do_QueryInterface(aChild));
   return nsDocShell::AddChild(treeItemChild);
}

NS_IMETHODIMP
nsWebShell::RemoveChild(nsIWebShell* aChild)
{
   NS_ENSURE_ARG(aChild);
   nsCOMPtr<nsIDocShellTreeItem> treeItemChild(do_QueryInterface(aChild));
   return nsDocShell::RemoveChild(treeItemChild);
}

NS_IMETHODIMP
nsWebShell::ChildAt(PRInt32 aIndex, nsIWebShell*& aResult)
{
   nsCOMPtr<nsIDocShellTreeItem> child;

   NS_ENSURE_SUCCESS(GetChildAt(aIndex, getter_AddRefs(child)),
      NS_ERROR_FAILURE);

   NS_ENSURE_SUCCESS(CallQueryInterface(child.get(), &aResult), 
      NS_ERROR_FAILURE);
   return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetName(const PRUnichar** aName)
{
   *aName = mName.GetUnicode();
   return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetName(const PRUnichar* aName)
{
   return nsDocShell::SetName(aName);
}

NS_IMETHODIMP
nsWebShell::GetURL(const PRUnichar** aURL)
{
 // XXX This is wrong unless the parameter is marked "shared". 
 // It should otherwise be copied and freed by the caller. 
  *aURL = mURL.GetUnicode();
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetURL(const PRUnichar* aURL)
{
  mURL = aURL;
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetIsInSHist(PRBool& aResult)
{
  aResult = mIsInSHist;
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetIsInSHist(PRBool aIsInSHist)
{
  mIsInSHist = aIsInSHist;
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::FindChildWithName(const PRUnichar* aName1,
                              nsIWebShell*& aResult)
{
   nsCOMPtr<nsIDocShellTreeItem> treeItem;

   NS_ENSURE_SUCCESS(nsDocShell::FindChildWithName(aName1, PR_FALSE, nsnull, 
      getter_AddRefs(treeItem)), NS_ERROR_FAILURE);

   if(treeItem)
      CallQueryInterface(treeItem.get(), &aResult);

   return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetWebShellType(nsWebShellType aWebShellType)
{
   PRInt32 treeItemType;

   switch(aWebShellType)
      {
      case nsWebShellChrome:
         treeItemType = typeChrome;
         break;

      case nsWebShellContent:
         treeItemType = typeContent;
         break;

      default:
         NS_ERROR("Attempt to set bogus webshell type: values should be content or chrome.");
         return NS_ERROR_FAILURE;
      }
   return SetItemType(treeItemType);
}

NS_IMETHODIMP
nsWebShell::GetScrolling(PRInt32& aScrolling)
{
  aScrolling = mScrolling[1];
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetScrolling(PRInt32 aScrolling, PRBool aSetCurrentAndInitial)
{
  mScrolling[1] = aScrolling;
  if (aSetCurrentAndInitial) {
    mScrolling[0] = aScrolling;
  }
  return NS_OK;
}


/**
 * Document Load methods
 */
NS_IMETHODIMP
nsWebShell::GetDocumentLoader(nsIDocumentLoader*& aResult)
{
  aResult = mDocLoader;
  NS_IF_ADDREF(mDocLoader);
  return (nsnull != mDocLoader) ? NS_OK : NS_ERROR_FAILURE;
}

#define FILE_PROTOCOL "file:///"

static void convertFileToURL(const nsString &aIn, nsString &aOut)
{
#ifdef XP_PC
  char szFile[1000];
  aIn.ToCString(szFile, sizeof(szFile));
  if (PL_strchr(szFile, '\\')) {
    PRInt32 len = strlen(szFile);
    PRInt32 sum = len + sizeof(FILE_PROTOCOL);
    char* lpszFileURL = (char *)PR_Malloc(sum + 1);

    // Translate '\' to '/'
    for (PRInt32 i = 0; i < len; i++) {
      if (szFile[i] == '\\') {
        szFile[i] = '/';
      }
      if (szFile[i] == ':') {
        szFile[i] = '|';
      }
    }

    // Build the file URL
    PR_snprintf(lpszFileURL, sum, "%s%s", FILE_PROTOCOL, szFile);
    aOut = lpszFileURL;
    PR_Free((void *)lpszFileURL);
  }
  else
#endif
  {
    aOut = aIn;
  }
}

NS_IMETHODIMP
nsWebShell::LoadURL(const PRUnichar *aURLSpec,
                    nsIInputStream* aPostDataStream,
                    PRBool aModifyHistory,
                    nsLoadFlags aType,
                    const PRUint32 aLocalIP,
                    nsISupports * aHistoryState,
                    const PRUnichar* aReferrer)
{
  // Initialize margnwidth, marginheight. Put scrolling back the way it was
  // before the last document was loaded.

	InitFrameData(PR_FALSE);

  const char *cmd = mViewSource ? "view-source" : "view" ;
  mViewSource = PR_FALSE; // reset it
  return LoadURL(aURLSpec, cmd, aPostDataStream,
                 aModifyHistory,aType, aLocalIP, aHistoryState,
                 aReferrer);
}


static PRBool EqualBaseURLs(nsIURI* url1, nsIURI* url2)
{
   nsXPIDLCString spec1;
   nsXPIDLCString spec2;
   char *  anchor1 = nsnull, * anchor2=nsnull;
   PRBool rv = PR_FALSE;
  
   if (url1 && url2) {
     // XXX We need to make these strcmps case insensitive.
     url1->GetSpec(getter_Copies(spec1));
     url2->GetSpec(getter_Copies(spec2));
 
     /* Don't look at the ref-part */
     anchor1 = PL_strrchr(spec1, '#');
     anchor2 = PL_strrchr(spec2, '#');
 
     if (anchor1)
         *anchor1 = '\0';
     if (anchor2)
         *anchor2 = '\0';
 
     if (0 == PL_strcmp(spec1,spec2)) {
       rv = PR_TRUE;
     }
   }   // url1 && url2
  return rv;
}

nsresult
nsWebShell::DoLoadURL(nsIURI * aUri,
                      const char* aCommand,
                      nsIInputStream* aPostDataStream,
                      nsLoadFlags aType,
                      const PRUint32 aLocalIP,
                      const PRUnichar* aReferrer,
                      PRBool aKickOffLoad)
{
  if (!aUri)
    return NS_ERROR_NULL_POINTER;

  // This should probably get saved in mHistoryService or something... 
  // Ugh. It sucks that we have to hack webshell like this. Forgive me, Father.
  do {
    nsresult rv;
    NS_WITH_SERVICE(nsIGlobalHistory, history, "component://netscape/browser/global-history", &rv);
    if (NS_FAILED(rv)) break;

    rv = history->AddPage(nsCAutoString(mURL), nsnull /* referrer */, PR_Now());
    if (NS_FAILED(rv)) break;
  } while (0);

  nsXPIDLCString urlSpec;
  nsresult rv = NS_OK;
  rv = aUri->GetSpec(getter_Copies(urlSpec));
  if (NS_FAILED(rv)) return rv;

  // If it's a normal reload that uses the cache, look at the destination anchor
  // and see if it's an element within the current document
  // We don't have a reload loadtype yet in necko. So, check for just history
  // loadtype
  if ((aType == nsISessionHistory::LOAD_HISTORY || aType == nsIChannel::LOAD_NORMAL) && (nsnull != mContentViewer) &&
      (nsnull == aPostDataStream))
  {
    nsCOMPtr<nsIDocumentViewer> docViewer;
    if (NS_SUCCEEDED(mContentViewer->QueryInterface(kIDocumentViewerIID,
                                                    getter_AddRefs(docViewer)))) {
      // Get the document object
      nsCOMPtr<nsIDocument> doc;
      docViewer->GetDocument(*getter_AddRefs(doc));

      // Get the URL for the document
      nsCOMPtr<nsIURI>  docURL = nsDontAddRef<nsIURI>(doc->GetDocumentURL());

      if (aUri && docURL && EqualBaseURLs(docURL, aUri)) {
        // See if there's a destination anchor
        nsXPIDLCString ref;
        nsCOMPtr<nsIURL> aUrl = do_QueryInterface(aUri);
        if (aUrl)
          rv = aUrl->GetRef(getter_Copies(ref));

        nsCOMPtr<nsIPresShell> presShell;
        rv = docViewer->GetPresShell(*getter_AddRefs(presShell));

        if (NS_SUCCEEDED(rv) && presShell) {
           /* Pass OnStartDocument notifications to the docloaderobserver
            * so that urlbar, forward/back buttons will
            * behave properly when going to named anchors
            */
           nsCOMPtr<nsIChannel> dummyChannel;
           rv = NS_OpenURI(getter_AddRefs(dummyChannel), aUri, nsnull);
           if (NS_FAILED(rv)) return rv;
           if (nsnull != (const char *) ref) {
              rv = OnStartDocumentLoad(mDocLoader, aUri, "load");
              // Go to the anchor in the current document
              rv = presShell->GoToAnchor(nsAutoString(ref));            
		          mProcessedEndDocumentLoad = PR_FALSE;
              // Set the URL & referrer if the anchor was successfully visited
              if (NS_SUCCEEDED(rv)) {
                  mURL = urlSpec;
                  mReferrer = aReferrer;
              }
		          // Pass on status of scrolling/anchor visit to docloaderobserver
              rv = OnEndDocumentLoad(mDocLoader, dummyChannel, rv, this);
		          return rv;		   
           }
           else if (aType == nsISessionHistory::LOAD_HISTORY)
           {
              rv = OnStartDocumentLoad(mDocLoader, aUri, "load");
              // Go to the top of the current document
              nsCOMPtr<nsIViewManager> viewMgr;
              rv = presShell->GetViewManager(getter_AddRefs(viewMgr));
              if (NS_SUCCEEDED(rv) && viewMgr) {
                nsIScrollableView* view;
                rv = viewMgr->GetRootScrollableView(&view);
				        if (NS_SUCCEEDED(rv) && view)
                rv = view->ScrollTo(0, 0, NS_VMREFRESH_IMMEDIATE);
                if (NS_SUCCEEDED(rv)) {
                     mURL = urlSpec;
                     mReferrer = aReferrer;
                }
                mProcessedEndDocumentLoad = PR_FALSE;
		            // Pass on status of scrolling/anchor visit to docloaderobserver
                rv = OnEndDocumentLoad(mDocLoader, dummyChannel, rv, this);
		            return rv;		   
              }
           }
#if 0        		   
		   mProcessedEndDocumentLoad = PR_FALSE;
		   // Pass on status of scrolling/anchor visit to docloaderobserver
           rv = OnEndDocumentLoad(mDocLoader, dummyChannel, rv, this);
		   return rv;		   
#endif /* 0 */
        }  // NS_SUCCEEDED(rv) && presShell
      } // EqualBaseURLs(docURL, url)
    }
  }

  // Stop loading the current document (if any...).  This call may result in
  // firing an EndLoadURL notification for the old document...
  if (aKickOffLoad)
    StopBeforeRequestingURL();
  

  // Tell web-shell-container we are loading a new url
  if (nsnull != mContainer) {
    nsAutoString uniSpec (urlSpec);
    rv = mContainer->BeginLoadURL(this, uniSpec.GetUnicode());
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  mProcessedEndDocumentLoad = PR_FALSE;

#ifdef MOZ_PERF_METRICS
  {
     char* url;
     nsresult rv = NS_OK;
     rv = aUri->GetSpec(&url);
     if (NS_SUCCEEDED(rv)) {
       MOZ_TIMER_LOG(("*** Timing layout processes on url: '%s', webshell: %p\n", url, this));
       delete [] url;
     }
  }
  
  MOZ_TIMER_DEBUGLOG(("Reset and start: nsWebShell::DoLoadURL(), this=%p\n", this));
  MOZ_TIMER_RESET(mTotalTime);
  MOZ_TIMER_START(mTotalTime);
#endif

 /* WebShell was primarily passing the buck when it came to streamObserver.
  * So, pass on the observer which is already a streamObserver to DocLoder.
  */
  if (aKickOffLoad)
    rv = mDocLoader->LoadDocument(aUri,        // URL string
                                  aCommand,        // Command
                                  NS_STATIC_CAST(nsIContentViewerContainer*, (nsIWebShell*)this),// Container
                                  aPostDataStream, // Post Data
                                  nsnull,          // Extra Info...
                                  aType,           // reload type
                                  aLocalIP,        // load attributes.
                                  aReferrer);      // referrer

    // Fix for bug 1646.  Change the notion of current url and referrer only after
  // the document load succeeds.
  if (NS_SUCCEEDED(rv)) {
    mURL = urlSpec;
    mReferrer = aReferrer;
  }

  return rv;
}

// nsIURIContentListener support
NS_IMETHODIMP
nsWebShell::GetProtocolHandler(nsIURI *aURI, nsIProtocolHandler **aProtocolHandler)
{
   // Farm this off to our content listener
   NS_ENSURE_SUCCESS(EnsureContentListener(), NS_ERROR_FAILURE);
   return mContentListener->GetProtocolHandler(aURI, aProtocolHandler);
}

NS_IMETHODIMP nsWebShell::CanHandleContent(const char * aContentType,
                                           nsURILoadCommand aCommand,
                                           const char * aWindowTarget,
                                           char ** aDesiredContentType,
                                           PRBool * aCanHandleContent)

{
  // the webshell knows nothing about content policy....pass this
  // up to our parent content handler...
  nsCOMPtr<nsIURIContentListener> parentListener;
  nsresult rv = GetParentURIContentListener(getter_AddRefs(parentListener));
  if (parentListener)
    rv = parentListener->CanHandleContent(aContentType, aCommand, aWindowTarget, aDesiredContentType, 
                                          aCanHandleContent);
  else
    // i'm going to try something interesting...if we don't have a parent to dictate whether we
    // can handle the content, let's say that we CAN handle the content instead of saying that we 
    // can't. I was running into the scenario where we were running a chrome url to bring up the first window
    // in the top level webshell. We didn't have a parent content listener yet because it was too early in
    // the start up process...so we'd fail to load the url....
    *aCanHandleContent = PR_TRUE;
  return NS_OK;
} 

NS_IMETHODIMP 
nsWebShell::DoContent(const char * aContentType,
                      nsURILoadCommand aCommand,
                      const char * aWindowTarget,
                      nsIChannel * aOpenedChannel,
                      nsIStreamListener ** aContentHandler,
                      PRBool * aAbortProcess)
{
  nsresult rv = NS_OK;
  if (aAbortProcess)
    *aAbortProcess = PR_FALSE;

  nsXPIDLCString strCommand;
  // go to the uri loader and ask it to convert the uri load command into a old
  // world style string
  NS_WITH_SERVICE(nsIURILoader, pURILoader, NS_URI_LOADER_PROGID, &rv);
  if (NS_SUCCEEDED(rv))
    pURILoader->GetStringForCommand(aCommand, getter_Copies(strCommand));


  // first, run any uri preparation stuff that we would have run normally
  // had we gone through OpenURI
  nsCOMPtr<nsIURI> aUri;
  aOpenedChannel->GetURI(getter_AddRefs(aUri));
  PrepareToLoadURI(aUri, strCommand, nsnull, PR_TRUE, nsIChannel::LOAD_NORMAL, 0, nsnull, nsnull);
  // mscott: when I called DoLoadURL I found that we ran into problems because
  // we currently don't have channel retargeting yet. Basically, what happens is that
  // DoLoadURL calls StopBeforeRequestingURL and this cancels the current load group
  // however since we can't retarget yet, we were basically canceling our very
  // own load group!!! So the request would get canceled out from under us...
  // after retargeting we may be able to safely call DoLoadURL. 
  DoLoadURL(aUri, strCommand, nsnull, nsIChannel::LOAD_NORMAL, 0, nsnull, PR_FALSE);
  return mDocLoader->LoadOpenedDocument(aOpenedChannel, 
                                        strCommand,
                                        (nsIWebShell*)this,
                                        nsnull,
                                        nsnull,
                                        aContentHandler);
}

nsresult nsWebShell::PrepareToLoadURI(nsIURI * aUri, 
                                      const char * aCommand,
                                      nsIInputStream * aPostStream,
                                      PRBool aModifyHistory,
                                      nsLoadFlags aType,
                                      const PRUint32 aLocalIP,
                                      nsISupports * aHistoryState,
                                      const PRUnichar * aReferrer)
{
  nsresult rv;
  CancelRefreshURITimers();
  nsXPIDLCString scheme, CUriSpec;

  if (!aUri) return NS_ERROR_NULL_POINTER;

  rv = aUri->GetScheme(getter_Copies(scheme));
  if (NS_FAILED(rv)) return rv;
  rv = aUri->GetSpec(getter_Copies(CUriSpec));
  if (NS_FAILED(rv)) return rv;

  nsAutoString uriSpec(CUriSpec);

  nsXPIDLCString spec;
  rv = aUri->GetSpec(getter_Copies(spec));
  if (NS_FAILED(rv)) return rv;

  nsString* url = new nsString(uriSpec);
  if (aModifyHistory) {
    // Discard part of history that is no longer reachable
    PRInt32 i, n = mHistory.Count();
    i = mHistoryIndex + 1;
    while (--n >= i) {
      nsString* u = (nsString*) mHistory.ElementAt(n);
      delete u;
      mHistory.RemoveElementAt(n);
    }

    // Tack on new url
    mHistory.AppendElement(url);
    mHistoryIndex++;
  }
  else {
	  
    // Replace the current history index with this URL
    nsString* u = (nsString*) mHistory.ElementAt(mHistoryIndex);
    if (nsnull != u) {
      delete u;
    }
    mHistory.ReplaceElementAt(url, mHistoryIndex);
  }
  ShowHistory();

  // Give web-shell-container right of refusal
  if (nsnull != mContainer) {
    nsAutoString str(spec);
    rv = mContainer->WillLoadURL(this, str.GetUnicode(), nsLoadURL);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  return rv;
}


NS_IMETHODIMP 
nsWebShell::LoadURI(nsIURI * aUri,
                    const char * aCommand,
                    nsIInputStream* aPostDataStream,
                    PRBool aModifyHistory,
                    nsLoadFlags aType,
                    const PRUint32 aLocalIP,
					          nsISupports * aHistoryState,
                    const PRUnichar* aReferrer)
{
  nsresult rv = PrepareToLoadURI(aUri, aCommand, aPostDataStream,
                                 aModifyHistory, aType, aLocalIP,
                                 aHistoryState, aReferrer);
  if (NS_SUCCEEDED(rv))
    rv =  DoLoadURL(aUri, aCommand, aPostDataStream, aType, aLocalIP, aReferrer);
  return rv;
}

NS_IMETHODIMP
nsWebShell::LoadURL(const PRUnichar *aURLSpec,
                    const char* aCommand,
                    nsIInputStream* aPostDataStream,
                    PRBool aModifyHistory,
                    nsLoadFlags aType,
                    const PRUint32 aLocalIP,
                    nsISupports * aHistoryState,
                    const PRUnichar* aReferrer)
{
  nsresult rv;

  /* 
     TODO This doesnt belong here... The app should be doing all this
     URL play. The webshell should not have a clue about whats "mailto" 
     If you insist that this should be here, then put in URL parsing 
     optimizations here. -Gagan
  */
  nsAutoString urlStr(aURLSpec);
  // first things first. try to create a uri out of the string.
  nsCOMPtr<nsIURI> uri;
  nsXPIDLCString  spec;
  rv = NS_NewURI(getter_AddRefs(uri), urlStr, nsnull);
  if (NS_FAILED(rv)) {
    // no dice.
    nsAutoString urlSpec;
    urlStr.Trim(" ", PR_TRUE, PR_TRUE);

    // see if we've got a file url.
    convertFileToURL(urlStr, urlSpec);
    rv = NS_NewURI(getter_AddRefs(uri), urlSpec, nsnull);
    if (NS_FAILED(rv)) {
      // KEYWORDS
      // keyword failover
      // we kick the following cases to the keyword server:
      //   * starts with a '?'
      //   * contains a space
      //   * (windows only) is a single word (contains no dots or scheme) XXX: this breaks
      //     intranet host lookups. This latter case needs to be handled by
      //     dns notifications rather than string interrogation.
      if (
#ifdef XP_PC
          // This is windows only because windows is the only platform that utilizes
          // WINS resolution (a DNS fallback) which can take several minutes to
          // fail a resolve call if the host does not exist. Thus, this forces us
          // to bypass DNS/WINS altogether on windows.
          (urlStr.FindChar('.') == -1) || 
#endif // XP_PC
          (urlStr.First() == '?') || (urlStr.FindChar(' ') > -1) ) {
          nsAutoString keywordSpec("keyword:");
          keywordSpec.Append(aURLSpec);
          rv = NS_NewURI(getter_AddRefs(uri), keywordSpec, nsnull);
      }

      if (NS_FAILED(rv)) {
          PRInt32 colon, fSlash = urlSpec.FindChar('/');
          PRUnichar port;
          // if no scheme (protocol) is found, assume http.
          if ( ((colon=urlSpec.FindChar(':')) == -1) // no colon at all
               || ( (fSlash > -1) && (colon > fSlash) ) // the only colon comes after the first slash
               || ( (colon < urlSpec.Length()-1) // the first char after the first colon is a digit (i.e. a port)
                    && ((port=urlSpec.CharAt(colon+1)) <= '9')
                    && (port > '0') )) {
            // find host name
            PRInt32 hostPos = urlSpec.FindCharInSet("./:");
            if (hostPos == -1) {
              hostPos = urlSpec.Length();
            }

            // extract host name
            nsAutoString hostSpec;
            urlSpec.Left(hostSpec, hostPos);

            // insert url spec corresponding to host name
            if (hostSpec.EqualsIgnoreCase("ftp")) {
              urlSpec.Insert("ftp://", 0, 6);
            } else {
              urlSpec.Insert("http://", 0, 7);
            }
          } // end if colon
          rv = NS_NewURI(getter_AddRefs(uri), urlSpec, nsnull);

	      nsAutoString  url(aURLSpec);
	      if (((url.Find("mailto:", PR_TRUE))<0) && (NS_FAILED(rv))) {
            // no dice, even more tricks?
            return rv;
          }
      }
    }

  }

  if (!uri) // we were unable to create a uri
      return rv;
  
  //Take care of mailto: url
  PRBool  isMail= PR_FALSE;
  rv = uri->GetSpec(getter_Copies(spec));
  if (NS_FAILED(rv))
	   return rv;

  nsAutoString urlAStr(spec);
  if ((urlAStr.Find("mailto", PR_TRUE)) >= 0) {
     isMail = PR_TRUE;
  }

  // Get hold of Root webshell
  nsCOMPtr<nsIWebShell>  root;
  nsCOMPtr<nsISessionHistory> shist;
  PRBool  isLoadingHistory=PR_FALSE; // Is SH currently loading an entry from history?
  rv = GetRootWebShell(*getter_AddRefs(root));
  // Get hold of session History
  if (NS_SUCCEEDED(rv) && root) {    
    root->GetSessionHistory(*getter_AddRefs(shist));
  }
  if (shist)
	  shist->GetLoadingFlag(&isLoadingHistory);


  /* 
   * Save the history state for the current index iff this loadurl() request
   * is not from SH. When the request comes from SH, aModifyHistory will
   * be false and nsSessionHistory.cpp takes of this.
   */
  if (shist) {
	 PRInt32  indix;
     shist->GetCurrentIndex(&indix);
     if (indix >= 0 && (aModifyHistory)) {
       nsCOMPtr<nsISupports>  historyState;
       rv = GetHistoryState(getter_AddRefs(historyState));
	   if (NS_SUCCEEDED(rv) && historyState)
          shist->SetHistoryObjectForIndex(indix, historyState);
	 }
  }
  /* Set the History state object for the current page in the
   * presentation shell. If it is a new page being visited,
   * aHistoryState is null. If the load is coming from
   * session History, it will be set to the cached history object by
   * session History.
   */
   SetHistoryState(aHistoryState);

  /* Add the page to session history */
  if (aModifyHistory && shist)  {
        PRInt32  ret;
        ret = shist->Add(spec, this);
  }

  nsCOMPtr<nsIWebShell> parent;
  nsresult res = GetParent(*getter_AddRefs(parent));
  nsAutoString urlstr;

  if ((isLoadingHistory)) {
	/* if LoadURL() got called from SH, AND If we are going "Back/Forward" 
	 * to a frame page,SH  will change the mURL to the right value
     * for smoother redraw. So, create a new nsIURI based on mURL,
     * so that it will work right in such situations.
	 */
     urlstr = mURL;
  }
  else{
	/* If the call is not from SH, use the url passed by the caller
	 * so that things like JS will work right. This is for bug # 1646.
	 * May regress in other situations.
	 * What a hack
	 */
     urlstr=spec;
  }
  
  nsCOMPtr<nsIURI>   newURI;
  res = NS_NewURI(getter_AddRefs(newURI), urlstr, nsnull);

  if (NS_SUCCEEDED(res)) {
    // now that we have a uri, call the REAL LoadURI method which requires a nsIURI.
    return LoadURI(newURI, aCommand, aPostDataStream, aModifyHistory, aType, aLocalIP, aHistoryState, aReferrer);
  }
  return rv;

}


NS_IMETHODIMP nsWebShell::Stop(void)
{
  if (nsnull != mContentViewer) {
    mContentViewer->Stop();
  }

  // Cancel any timers that were set for this loader.
  CancelRefreshURITimers();

  if (mDocLoader) {
    // Stop any documents that are currently being loaded...
    mDocLoader->Stop();
  }

  // Stop the documents being loaded by children too...
  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) {
    nsIDocShell* shell = (nsIDocShell*) mChildren.ElementAt(i);
    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(shell));
    webShell->Stop();
  }

  return NS_OK;
}

// This "stops" the current document load enough so that the document loader
// can be used to load a new URL.
NS_IMETHODIMP
nsWebShell::StopBeforeRequestingURL()
{
  if (mDocLoader) {
    // Stop any documents that are currently being loaded...
    mDocLoader->Stop();
  }

  // Recurse down the webshell hierarchy.
  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) {
    nsIDocShell* shell = (nsIDocShell*) mChildren.ElementAt(i);
    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(shell));
    webShell->StopBeforeRequestingURL();
  }

  return NS_OK;
}

// This "stops" the current document load completely and is called once
// it has been determined that the new URL is valid and ready to be thrown
// at us from netlib.
NS_IMETHODIMP
nsWebShell::StopAfterURLAvailable()
{
  if (nsnull != mContentViewer) {
    mContentViewer->Stop();
  }

  // Cancel any timers that were set for this loader.
  CancelRefreshURITimers();

  // Recurse down the webshell hierarchy.
  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) {
    nsIDocShell* shell = (nsIDocShell*) mChildren.ElementAt(i);
    nsCOMPtr<nsIWebShell> webShell(do_QueryInterface(shell));
    webShell->StopAfterURLAvailable();
  }

  return NS_OK;
}

NS_IMETHODIMP nsWebShell::Reload(nsLoadFlags aType)
{
  nsString* s = (nsString*) mHistory.ElementAt(mHistoryIndex);
  if (nsnull != s) {
    // XXX What about the post data?

    // Allocate since mReferrer will change beneath us
    PRUnichar* str = mReferrer.ToNewUnicode();
    return LoadURL(s->GetUnicode(), nsnull, PR_FALSE, 
                   aType, 0, nsnull, str);
    Recycle(str);
  }

  return NS_ERROR_FAILURE;

}

//----------------------------------------

// History methods

NS_IMETHODIMP
nsWebShell::Back(void)
{
  return GoTo(mHistoryIndex - 1);
}

NS_IMETHODIMP
nsWebShell::CanBack(void)
{
  return ((mHistoryIndex-1)  > - 1 ? NS_OK : NS_COMFALSE);
}

NS_IMETHODIMP
nsWebShell::Forward(void)
{
  return GoTo(mHistoryIndex + 1);
}

NS_IMETHODIMP
nsWebShell::CanForward(void)
{
  return (mHistoryIndex  < mHistory.Count() - 1 ? NS_OK : NS_COMFALSE);
}

NS_IMETHODIMP
nsWebShell::GoTo(PRInt32 aHistoryIndex)
{
  nsresult rv = NS_ERROR_ILLEGAL_VALUE;
  if ((aHistoryIndex >= 0) &&
      (aHistoryIndex < mHistory.Count())) {
    nsString* s = (nsString*) mHistory.ElementAt(aHistoryIndex);

    // Give web-shell-container right of refusal
    nsAutoString urlSpec(*s);
    if (nsnull != mContainer) {
      rv = mContainer->WillLoadURL(this, urlSpec.GetUnicode(), nsLoadHistory);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }

    printf("Goto %d\n", aHistoryIndex);
    mHistoryIndex = aHistoryIndex;
    ShowHistory();

    // convert the uri spec into a url and then pass it to DoLoadURL
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), urlSpec, nsnull);
    if (NS_FAILED(rv)) return rv;

    rv = DoLoadURL(uri,       // URL string
                   "view",        // Command
                   nsnull,        // Post Data
				   nsISessionHistory::LOAD_HISTORY,  // the reload type
                   0,            // load attributes
                   nsnull);      // referrer
  }
  return rv;

}

NS_IMETHODIMP
nsWebShell::GetHistoryLength(PRInt32& aResult)
{
  aResult = mHistory.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetHistoryIndex(PRInt32& aResult)
{
  aResult = mHistoryIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetURL(PRInt32 aHistoryIndex, const PRUnichar** aURLResult)
{
  nsresult rv = NS_ERROR_ILLEGAL_VALUE;

  // XXX Ownership rules for the string passed back from this
  // method are not XPCOM compliant. If they were correct, 
  // the caller would deallocate the string.
  if ((aHistoryIndex >= 0) &&
      (aHistoryIndex <= mHistory.Count() - 1)) {
    nsString* s = (nsString*) mHistory.ElementAt(aHistoryIndex);
    if (nsnull != s) {
      *aURLResult = s->GetUnicode();
    }
    rv = NS_OK;
  }
  return rv;
}

void
nsWebShell::ShowHistory()
{
#if defined(NS_DEBUG)
  if (WEB_LOG_TEST(gLogModule, WEB_TRACE_HISTORY)) {
    PRInt32 i, n = mHistory.Count();
    for (i = 0; i < n; i++) {
      if (i == mHistoryIndex) {
        printf("**");
      }
      else {
        printf("  ");
      }
      nsString* u = (nsString*) mHistory.ElementAt(i);
      fputs(*u, stdout);
      printf("\n");
    }
  }
#endif
}


//----------------------------------------

//----------------------------------------------------------------------

// WebShell container implementation

NS_IMETHODIMP
nsWebShell::WillLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsLoadType aReason)
{
  nsresult rv = NS_OK;
  if (nsnull != mContainer) {
    rv = mContainer->WillLoadURL(aShell, aURL, aReason);
  }
  return rv;
}

NS_IMETHODIMP
nsWebShell::BeginLoadURL(nsIWebShell* aShell, const PRUnichar* aURL)
{
  if (nsnull != mContainer) {
    // XXX: do not propagate this notification up from any frames...
    return mContainer->BeginLoadURL(aShell, aURL);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::ProgressLoadURL(nsIWebShell* aShell,
                            const PRUnichar* aURL,
                            PRInt32 aProgress,
                            PRInt32 aProgressMax)
{
  nsresult rv = NS_OK;
  if (nsnull != mContainer) {
    rv = mContainer->ProgressLoadURL(aShell, aURL, aProgress, aProgressMax);
  }
  return rv;
}

NS_IMETHODIMP
nsWebShell::EndLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, nsresult aStatus)
{
  nsresult rv = NS_OK;
  if (nsnull != mContainer) {
    // XXX: do not propagate this notification up from any frames...
    return mContainer->EndLoadURL(aShell, aURL, aStatus);
  }
  return rv;
}


NS_IMETHODIMP
nsWebShell::NewWebShell(PRUint32 aChromeMask,
                        PRBool aVisible,
                        nsIWebShell *&aNewWebShell)
{
  if (nsnull != mContainer) {
    return mContainer->NewWebShell(aChromeMask, aVisible, aNewWebShell);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::ContentShellAdded(nsIWebShell* aChildShell, nsIContent* frameNode)
{
  if (nsnull != mContainer) {
    return mContainer->ContentShellAdded(aChildShell, frameNode);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::CreatePopup(nsIDOMElement* aElement, nsIDOMElement* aPopupContent,
                         PRInt32 aXPos, PRInt32 aYPos,
                         const nsString& aPopupType, const nsString& anAnchorAlignment,
                         const nsString& aPopupAlignment,
                         nsIDOMWindow* aWindow, nsIDOMWindow** outPopup)
{
  if (nsnull != mContainer) {
    return mContainer->CreatePopup(aElement, aPopupContent, aXPos, aYPos, aPopupType,
                                   anAnchorAlignment, aPopupAlignment,
                                   aWindow, outPopup);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::FindWebShellWithName(const PRUnichar* aName, nsIWebShell*& aResult)
{
   // First we try the new system
   nsCOMPtr<nsIDocShellTreeItem> treeItem;

   NS_ENSURE_SUCCESS(FindItemWithName(aName, nsnull, 
      getter_AddRefs(treeItem)), NS_ERROR_FAILURE);

   if(treeItem)
      CallQueryInterface(treeItem.get(), &aResult);
   else if(mContainer) // Then fall back to the old.
      return mContainer->FindWebShellWithName(aName, aResult);
  
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::FocusAvailable(nsIWebShell* aFocusedWebShell, PRBool& aFocusTaken)
{
   nsCOMPtr<nsIBaseWindow> webShellAsWin(do_QueryInterface(aFocusedWebShell));
   return FocusAvailable(webShellAsWin, &aFocusTaken);
}

NS_IMETHODIMP
nsWebShell::GetHistoryState(nsISupports** aLayoutHistoryState)
{
  nsresult rv = NS_OK;
  // XXX Need to think about what to do for framesets.
  // For now, return an error if this webshell
  // contains a frame or a frameset document.
  // The main content area will always have a parent. It is
  // enough to check the children count to verify frames.
  if (mChildren.Count() > 0) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  // Ask the pres shell for the history state
  if (mContentViewer) {
    nsCOMPtr<nsIDocumentViewer> docv = do_QueryInterface(mContentViewer);
    if (nsnull != docv) {
      nsCOMPtr<nsIPresShell> shell;
      rv = docv->GetPresShell(*getter_AddRefs(shell));
      if (NS_SUCCEEDED(rv)) {
        rv = shell->GetHistoryState((nsILayoutHistoryState**) aLayoutHistoryState);
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
nsWebShell::SetHistoryState(nsISupports* aLayoutHistoryState)
{
  mHistoryState = aLayoutHistoryState;
  return NS_OK;
}

//----------------------------------------------------------------------
// Web Shell Services API

NS_IMETHODIMP
nsWebShell::LoadDocument(const char* aURL,
                         const char* aCharset,
                         nsCharsetSource aSource)
{
  // XXX hack. kee the aCharset and aSource wait to pick it up
  nsCOMPtr<nsIContentViewer> cv;
  NS_ENSURE_SUCCESS(GetContentViewer(getter_AddRefs(cv)), NS_ERROR_FAILURE);
  if (cv)
  {
    nsCOMPtr<nsIMarkupDocumentViewer> muDV = do_QueryInterface(cv);  
    if (muDV)
    {
      nsCharsetSource hint;
      muDV->GetHintCharacterSetSource((PRInt32 *)(&hint));
      if( aSource > hint ) 
      {
        nsAutoString inputCharSet(aCharset);
        muDV->SetHintCharacterSet(inputCharSet.GetUnicode());
        muDV->SetHintCharacterSetSource((PRInt32)aSource);
        if(eCharsetReloadRequested != mCharsetReloadState) 
        {
          mCharsetReloadState = eCharsetReloadRequested;
          nsAutoString url(aURL);
          LoadURL(url.GetUnicode());
        }
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::ReloadDocument(const char* aCharset,
                           nsCharsetSource aSource,
                           const char* aCmd)
{

  // XXX hack. kee the aCharset and aSource wait to pick it up
  nsCOMPtr<nsIContentViewer> cv;
  NS_ENSURE_SUCCESS(GetContentViewer(getter_AddRefs(cv)), NS_ERROR_FAILURE);
  if (cv)
  {
    nsCOMPtr<nsIMarkupDocumentViewer> muDV = do_QueryInterface(cv);  
    if (muDV)
    {
      nsCharsetSource hint;
      muDV->GetHintCharacterSetSource((PRInt32 *)(&hint));
      if( aSource > hint ) 
      {
        nsAutoString inputCharSet(aCharset);
         muDV->SetHintCharacterSet(inputCharSet.GetUnicode());
         muDV->SetHintCharacterSetSource((PRInt32)aSource);
         mViewSource = (0==PL_strcmp("view-source", aCmd));
         if(eCharsetReloadRequested != mCharsetReloadState) 
         {
            mCharsetReloadState = eCharsetReloadRequested;
            return Reload(nsIChannel::LOAD_NORMAL);
         }
      }
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
nsWebShell::StopDocumentLoad(void)
{
  if(eCharsetReloadRequested != mCharsetReloadState) 
  {
     Stop();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetRendering(PRBool aRender)
{
  if(eCharsetReloadRequested != mCharsetReloadState) 
  {
    if (mContentViewer) {
       mContentViewer->SetEnableRendering(aRender);
    }
  }
  return NS_OK;
}

//----------------------------------------------------------------------

// WebShell link handling

struct OnLinkClickEvent : public PLEvent {
  OnLinkClickEvent(nsWebShell* aHandler, nsIContent* aContent,
                   nsLinkVerb aVerb, const PRUnichar* aURLSpec,
                   const PRUnichar* aTargetSpec, nsIInputStream* aPostDataStream = 0);
  ~OnLinkClickEvent();

  void HandleEvent() {
    mHandler->HandleLinkClickEvent(mContent, mVerb, mURLSpec->GetUnicode(),
                                   mTargetSpec->GetUnicode(), mPostDataStream);
  }

  nsWebShell*  mHandler;
  nsString*    mURLSpec;
  nsString*    mTargetSpec;
  nsIInputStream* mPostDataStream;
  nsIContent*     mContent;
  nsLinkVerb      mVerb;
};

static void PR_CALLBACK HandlePLEvent(OnLinkClickEvent* aEvent)
{
  aEvent->HandleEvent();
}

static void PR_CALLBACK DestroyPLEvent(OnLinkClickEvent* aEvent)
{
  delete aEvent;
}

OnLinkClickEvent::OnLinkClickEvent(nsWebShell* aHandler,
                                   nsIContent *aContent,
                                   nsLinkVerb aVerb,
                                   const PRUnichar* aURLSpec,
                                   const PRUnichar* aTargetSpec,
                                   nsIInputStream* aPostDataStream)
{
  nsIEventQueue* eventQueue;

  mHandler = aHandler;
  NS_ADDREF(aHandler);
  mURLSpec = new nsString(aURLSpec);
  mTargetSpec = new nsString(aTargetSpec);
  mPostDataStream = aPostDataStream;
  NS_IF_ADDREF(mPostDataStream);
  mContent = aContent;
  NS_IF_ADDREF(mContent);
  mVerb = aVerb;

  PL_InitEvent(this, nsnull,
               (PLHandleEventProc) ::HandlePLEvent,
               (PLDestroyEventProc) ::DestroyPLEvent);

  eventQueue = aHandler->GetEventQueue();
  eventQueue->PostEvent(this);
  NS_RELEASE(eventQueue);
}

OnLinkClickEvent::~OnLinkClickEvent()
{
  NS_IF_RELEASE(mContent);
  NS_IF_RELEASE(mHandler);
  NS_IF_RELEASE(mPostDataStream);
  if (nsnull != mURLSpec) delete mURLSpec;
  if (nsnull != mTargetSpec) delete mTargetSpec;

}

//----------------------------------------

NS_IMETHODIMP
nsWebShell::OnLinkClick(nsIContent* aContent,
                        nsLinkVerb aVerb,
                        const PRUnichar* aURLSpec,
                        const PRUnichar* aTargetSpec,
                        nsIInputStream* aPostDataStream)
{
  OnLinkClickEvent* ev;
  nsresult rv = NS_OK;

  ev = new OnLinkClickEvent(this, aContent, aVerb, aURLSpec,
                            aTargetSpec, aPostDataStream);
  if (nsnull == ev) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  return rv;
}

// Find the web shell in the entire tree that we can reach that the
// link click should go to.

// XXX This doesn't yet know how to target other windows with their
// own tree
nsIWebShell*
nsWebShell::GetTarget(const PRUnichar* aName)
{
  nsAutoString name(aName);
  nsIWebShell* target = nsnull;

  if (0 == name.Length()) {
    NS_ADDREF_THIS();
    return this;
  }

  if (name.EqualsIgnoreCase("_blank")) {
    nsIWebShell *shell;
    if (NS_OK == NewWebShell(NS_CHROME_ALL_CHROME, PR_TRUE, shell))
      target = shell;
    else
    {
      //don't know what to do here? MMP
      NS_ASSERTION(PR_FALSE, "unable to get new webshell");
    }
  }
  else if (name.EqualsIgnoreCase("_self")) {
    target = this;
    NS_ADDREF(target);
  }
  else if (name.EqualsIgnoreCase("_parent")) {
    GetParent(target);
    if (target == nsnull) {
      target = this;
      NS_ADDREF(target);
    }
  }
  else if (name.EqualsIgnoreCase("_top")) {
    GetRootWebShell(target);            // this addrefs, which is OK
  }
  else if (name.EqualsIgnoreCase("_content")) {
    // a kind of special case: only the window can answer this question
    NS_ASSERTION(mContainer, "null container in WebShell::GetTarget");
    if (nsnull != mContainer)
      mContainer->FindWebShellWithName(aName, target);
      // (and don't SetName())
    // else target remains nsnull, which would be bad
  }
  else {
    // Look from the top of the tree downward
    NS_ASSERTION(mContainer, "null container in WebShell::GetTarget");
    if (nsnull != mContainer) {
      mContainer->FindWebShellWithName(aName, target);
      if (nsnull == target) {
        mContainer->NewWebShell(NS_CHROME_ALL_CHROME, PR_TRUE, target);
      }
      if (nsnull != target) {
        target->SetName(aName);
      }
      else {
        target = this;
        NS_ADDREF(target);
      }
    }
  }

  return target;
}

nsIEventQueue* nsWebShell::GetEventQueue(void)
{
  NS_PRECONDITION(nsnull != mThreadEventQueue, "EventQueue for thread is null");
  NS_ADDREF(mThreadEventQueue);
  return mThreadEventQueue;
}

void
nsWebShell::HandleLinkClickEvent(nsIContent *aContent,
                                 nsLinkVerb aVerb,
                                 const PRUnichar* aURLSpec,
                                 const PRUnichar* aTargetSpec,
                                 nsIInputStream* aPostDataStream)
{
  nsAutoString target(aTargetSpec);


  switch(aVerb) {
    case eLinkVerb_New:
      target.SetString("_blank");
      // Fall into replace case
    case eLinkVerb_Replace:
      {
        nsIWebShell* shell = GetTarget(target.GetUnicode());
        if (nsnull != shell) {
          // Allocate since mURL may change beneath us
          PRUnichar* str = mURL.ToNewUnicode();
          nsresult rv = NS_OK;

          NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &rv); 
          PRBool useURILoader = PR_FALSE;
          if (NS_SUCCEEDED(rv))
            prefs->GetBoolPref("browser.uriloader", &useURILoader);
          if (useURILoader)
          {
              // for now, just hack the verb to be view-link-clicked
              // and down in the load document code we'll detect this and
              // set the correct uri loader command
            (void)shell->LoadURL(aURLSpec, "view-link-click", aPostDataStream,
                                PR_TRUE, nsIChannel::LOAD_NORMAL, 
                                0, nsnull, str);
          }
          else
          {
            (void)shell->LoadURL(aURLSpec, aPostDataStream,
                                 PR_TRUE, nsIChannel::LOAD_NORMAL,
                                 0, nsnull, str);
          }
          Recycle(str);
          NS_RELEASE(shell);
        }
      }
      break;
    case eLinkVerb_Embed:
    default:
      ;
      // XXX Need to do this
  }
}

NS_IMETHODIMP
nsWebShell::OnOverLink(nsIContent* aContent,
                       const PRUnichar* aURLSpec,
                       const PRUnichar* aTargetSpec)
{
  if (!mOverURL.Equals(aURLSpec) || !mOverTarget.Equals(aTargetSpec)) {
#ifdef NOISY_LINKS
    fputs("Was '", stdout);
    fputs(mOverURL, stdout);
    fputs("' '", stdout);
    fputs(mOverTarget, stdout);
    fputs("'\n", stdout);
    fputs("Over link '", stdout);
    fputs(aURLSpec, stdout);
    fputs("' '", stdout);
    fputs(aTargetSpec, stdout);
    fputs("'\n", stdout);
#endif /* NS_DEBUG */

    mOverURL = aURLSpec;
    mOverTarget = aTargetSpec;

    // Get the browser window and setStatus
    nsIBrowserWindow *browserWindow;

    browserWindow = GetBrowserWindow();
    if (nsnull != browserWindow) {
      browserWindow->SetStatus(aURLSpec);
      NS_RELEASE(browserWindow);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetLinkState(const PRUnichar* aURLSpec, nsLinkState& aState)
{
  aState = eLinkState_Unvisited;

  nsresult rv;

  // XXX: GlobalHistory is going to be moved out of the webshell into a more appropriate place.
  if ((nsnull == mHistoryService) && !mFailedToLoadHistoryService) {
    rv = nsServiceManager::GetService(kGlobalHistoryCID,
                                      nsIGlobalHistory::GetIID(),
                                      (nsISupports**) &mHistoryService);

    if (NS_FAILED(rv)) {
      mFailedToLoadHistoryService = PR_TRUE;
    }
  }

  if (mHistoryService) {
    // XXX aURLSpec should really be a char*, not a PRUnichar*.
    nsAutoString urlStr(aURLSpec);

    char buf[256];
    char* url = buf;

    if (urlStr.Length() >= PRInt32(sizeof buf)) {
      url = new char[urlStr.Length() + 1];
    }

    PRInt64 lastVisitDate;

    if (url) {
      urlStr.ToCString(url, urlStr.Length() + 1);

      rv = mHistoryService->GetLastVisitDate(url, &lastVisitDate);

      if (url != buf)
        delete[] url;
    }
    else {
      rv = NS_ERROR_OUT_OF_MEMORY;
    }

  //XXX: Moved to destructor  nsServiceManager::ReleaseService(kGlobalHistoryCID, mHistoryService);

    if (NS_FAILED(rv)) return rv;

    // a last-visit-date of zero means we've never seen it before; so
    // if it's not zero, we must've seen it.
    if (! LL_IS_ZERO(lastVisitDate))
      aState = eLinkState_Visited;

    // XXX how to tell if eLinkState_OutOfDate?
  }

  return NS_OK;
}

//----------------------------------------------------------------------
nsIBrowserWindow* nsWebShell::GetBrowserWindow()
{
   nsCOMPtr<nsIWebShell> rootWebShell;
  nsIBrowserWindow *browserWindow = nsnull;

  GetRootWebShellEvenIfChrome(getter_AddRefs(rootWebShell));

  if (rootWebShell) {
    nsIWebShellContainer *rootContainer;
    rootWebShell->GetContainer(rootContainer);
    if (nsnull != rootContainer) {
      rootContainer->QueryInterface(kIBrowserWindowIID, (void**)&browserWindow);
      NS_RELEASE(rootContainer);
    }
  }

  return browserWindow;
}

nsresult
nsWebShell::CreateScriptEnvironment()
{
  nsresult res = NS_OK;

  if (nsnull == mScriptGlobal) {
    res = NS_NewScriptGlobalObject(&mScriptGlobal);
    if (NS_FAILED(res)) {
      return res;
    }
    mScriptGlobal->SetWebShell(this);
    mScriptGlobal->SetGlobalObjectOwner(
      NS_STATIC_CAST(nsIScriptGlobalObjectOwner*, this));
  }

  if (nsnull == mScriptContext) {
    res = NS_CreateScriptContext(mScriptGlobal, &mScriptContext);
  }

  return res;
}

NS_IMETHODIMP
nsWebShell::OnStartDocumentLoad(nsIDocumentLoader* loader,
                                nsIURI* aURL,
                                const char* aCommand)
{
  nsIDocumentViewer* docViewer;
  nsresult rv = NS_ERROR_FAILURE;

  if ((nsnull != mScriptGlobal) &&
      (loader == mDocLoader)) {
    if (nsnull != mContentViewer &&
        NS_OK == mContentViewer->QueryInterface(kIDocumentViewerIID, (void**)&docViewer)) {
      nsIPresContext *presContext;
      if (NS_OK == docViewer->GetPresContext(presContext)) {
        nsEventStatus status = nsEventStatus_eIgnore;
        nsMouseEvent event;
        event.eventStructType = NS_EVENT;
        event.message = NS_PAGE_UNLOAD;
        rv = mScriptGlobal->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);

        NS_RELEASE(presContext);
      }
      NS_RELEASE(docViewer);
    }
  }

  if (loader == mDocLoader) {
    nsCOMPtr<nsIDocumentLoaderObserver> dlObserver;

    if (!mDocLoaderObserver && mParent) {
      /* If this is a frame (in which case it would have a parent && doesn't
       * have a documentloaderObserver, get it from the rootWebShell
       */
      nsCOMPtr<nsIWebShell> root;
      nsresult res = GetRootWebShell(*getter_AddRefs(root));

      if (NS_SUCCEEDED(res) && root)
        root->GetDocLoaderObserver(getter_AddRefs(dlObserver));
    }
    else
    {
      dlObserver = do_QueryInterface(mDocLoaderObserver);  // we need this to addref
    }
    /*
     * Fire the OnStartDocumentLoad of the webshell observer
     */
    if ((nsnull != mContainer) && (nsnull != dlObserver))
    {
       dlObserver->OnStartDocumentLoad(mDocLoader, aURL, aCommand);
    }
  }

  return rv;
}



NS_IMETHODIMP
nsWebShell::OnEndDocumentLoad(nsIDocumentLoader* loader,
                              nsIChannel* channel,
                              nsresult aStatus,
                              nsIDocumentLoaderObserver * aWebShell)
{
#ifdef MOZ_PERF_METRICS
  MOZ_TIMER_DEBUGLOG(("Stop: nsWebShell::OnEndDocumentLoad(), this=%p\n", this));
  MOZ_TIMER_STOP(mTotalTime);
  MOZ_TIMER_LOG(("Total (Layout + Page Load) Time (webshell=%p): ", this));
  MOZ_TIMER_PRINT(mTotalTime);
#endif

  nsresult rv = NS_ERROR_FAILURE;
  if (!channel) {
    return NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<nsIURI> aURL;
  rv = channel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) return rv;
  
  // clean up reload state for meta charset
  if(eCharsetReloadRequested == mCharsetReloadState)
      mCharsetReloadState = eCharsetReloadStopOrigional;
  else 
      mCharsetReloadState = eCharsetReloadInit;

  /* one of many safeguards that prevent death and destruction if
     someone is so very very rude as to bring this window down
     during this load handler. */
  nsCOMPtr<nsIWebShell> kungFuDeathGrip(this);

//if (!mProcessedEndDocumentLoad) {
  if (loader == mDocLoader) {
    mProcessedEndDocumentLoad = PR_TRUE;

    if (nsnull != mScriptGlobal) {
      nsIDocumentViewer* docViewer;
      if (nsnull != mContentViewer &&
          NS_OK == mContentViewer->QueryInterface(kIDocumentViewerIID, (void**)&docViewer)) {
        nsIPresContext *presContext;
        if (NS_OK == docViewer->GetPresContext(presContext)) {
          nsEventStatus status = nsEventStatus_eIgnore;
          nsMouseEvent event;
          event.eventStructType = NS_EVENT;
          event.message = NS_PAGE_LOAD;
          rv = mScriptGlobal->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);

          NS_RELEASE(presContext);
        }
        NS_RELEASE(docViewer);
      }
    }

    // Fire the EndLoadURL of the web shell container
    if (nsnull != aURL) {
       nsAutoString urlString;
       char* spec;
       rv = aURL->GetSpec(&spec);
       if (NS_SUCCEEDED(rv)) {
         urlString = spec;
         if (nsnull != mContainer) {
            rv = mContainer->EndLoadURL(this, urlString.GetUnicode(), 0);
         }
         nsCRT::free(spec);
       }
    }


    nsCOMPtr<nsIDocumentLoaderObserver> dlObserver;

    if (!mDocLoaderObserver && mParent) {
      /* If this is a frame (in which case it would have a parent && doesn't
       * have a documentloaderObserver, get it from the rootWebShell
       */
      nsCOMPtr<nsIWebShell> root;
      nsresult res = GetRootWebShell(*getter_AddRefs(root));

      if (NS_SUCCEEDED(res) && root)
        root->GetDocLoaderObserver(getter_AddRefs(dlObserver));
    }
    else
    {
      /* Take care of the Trailing slash situation */
      if (mSHist)
        CheckForTrailingSlash(aURL);
      dlObserver = do_QueryInterface(mDocLoaderObserver);  // we need this to addref
    }

    /*
     * Fire the OnEndDocumentLoad of the DocLoaderobserver
     */
    if (dlObserver && (nsnull != aURL)) {
       dlObserver->OnEndDocumentLoad(mDocLoader, channel, aStatus, aWebShell);
    }

    if ( (mDocLoader == loader) && (aStatus == NS_ERROR_UNKNOWN_HOST) ) {
        // KEYWORDS
        // if a lookup failed, check to see if it was non-qualified.
        // if it was, kick it to the keyword server. the keyword server
        // now handles the www.*.com trick.
        char *host = nsnull;
        rv = aURL->GetHost(&host);
        if (NS_FAILED(rv)) return rv;

        if(!host) return NS_ERROR_OUT_OF_MEMORY;
        
        char *tmp = host;
        while (*tmp && (*tmp != '.')) tmp++;
        if (!*tmp) {
            nsAutoString keywordSpec("keyword:");
            keywordSpec.Append(host);
            rv = LoadURL(keywordSpec.GetUnicode(), "view");
            if (NS_FAILED(rv)) {
                // if we couldn't load the keyword munged URL do
                // our own www.*.com trick.
                nsAllocator::Free(host);
                nsCAutoString hostStr;
                rv = aURL->GetHost(&host);
                if (NS_FAILED(rv)) return rv;
     
                hostStr.SetString(host);
                PRInt32 dotLoc = -1;
                dotLoc = hostStr.FindChar('.');
                PRBool retry = PR_FALSE;
                if (-1 == dotLoc) {
                    hostStr.Insert("www.", 0, 4);
                    hostStr.Append(".com");
                    retry = PR_TRUE;
                } else if ( (hostStr.Length() - dotLoc) == 3) {
                    hostStr.Insert("www.", 0, 4);
                    retry = PR_TRUE;
                }
    
                if (retry) {
                    rv = aURL->SetHost(hostStr.GetBuffer());
                    if (NS_FAILED(rv)) return rv;
                    char *aSpec = nsnull;
                    rv = aURL->GetSpec(&aSpec);
                    if (NS_FAILED(rv)) return rv;
                    nsAutoString newURL(aSpec);
                    nsAllocator::Free(aSpec);
                    // reload the url
                    rv = LoadURL(newURL.GetUnicode(), "view");
                } // retry
            } // end keyword load failure
        } // end !*tmp
        nsAllocator::Free(host);
    } // unknown host
  } //!mProcessedEndDocumentLoad
  else {
    rv = NS_OK;
  }

  return rv;
}

NS_IMETHODIMP
nsWebShell::OnStartURLLoad(nsIDocumentLoader* loader,
                           nsIChannel* channel,
                           nsIContentViewer* aViewer)
{
  nsresult rv;

  nsCOMPtr<nsIURI> aURL;
  rv = channel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) return rv;


  // Stop loading of the earlier document completely when the document url
  // load starts.  Now we know that this url is valid and available.
  nsXPIDLCString url;
  aURL->GetSpec(getter_Copies(url));
  if (0 == PL_strcmp(url, mURL.GetBuffer()))
    StopAfterURLAvailable();

  /*
   *Fire the OnStartDocumentLoad of the webshell observer
   */
  if ((nsnull != mContainer) && (nsnull != mDocLoaderObserver))
  {
    mDocLoaderObserver->OnStartURLLoad(mDocLoader, channel, aViewer);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::OnProgressURLLoad(nsIDocumentLoader* loader,
                              nsIChannel* channel,
                              PRUint32 aProgress,
                              PRUint32 aProgressMax)
{
  /*
   *Fire the OnStartDocumentLoad of the webshell observer and container...
   */
  if ((nsnull != mContainer) && (nsnull != mDocLoaderObserver))
  {
     mDocLoaderObserver->OnProgressURLLoad(mDocLoader, channel, aProgress, aProgressMax);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::OnStatusURLLoad(nsIDocumentLoader* loader,
                            nsIChannel* channel,
                            nsString& aMsg)
{
  /*
   *Fire the OnStartDocumentLoad of the webshell observer and container...
   */
  if ((nsnull != mContainer) && (nsnull != mDocLoaderObserver))
  {
     mDocLoaderObserver->OnStatusURLLoad(mDocLoader, channel, aMsg);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::OnEndURLLoad(nsIDocumentLoader* loader,
                         nsIChannel* channel,
                         nsresult aStatus)
{
#if 0
  const char* spec;
  aURL->GetSpec(&spec);
  printf("nsWebShell::OnEndURLLoad:%p: loader=%p url=%s status=%d\n", this, loader, spec, aStatus);
#endif
  /*
   *Fire the OnEndDocumentLoad of the webshell observer
   */
  if ((nsnull != mContainer) && (nsnull != mDocLoaderObserver))
  {
      mDocLoaderObserver->OnEndURLLoad(mDocLoader, channel, aStatus);
  }
  return NS_OK;
}

/* For use with redirect/refresh url api */
class refreshData : public nsITimerCallback
{
public:
  refreshData();

  NS_DECL_ISUPPORTS

  // nsITimerCallback interface
  virtual void Notify(nsITimer *timer);

  nsIWebShell* mShell;
  nsString     mUrlSpec;
  PRBool       mRepeat;
  PRInt32      mDelay;

protected:
  virtual ~refreshData();
};

refreshData::refreshData()
{
  NS_INIT_REFCNT();

  mShell   = nsnull;
}

refreshData::~refreshData()
{
  NS_IF_RELEASE(mShell);
}


NS_IMPL_ISUPPORTS(refreshData, kITimerCallbackIID);

void refreshData::Notify(nsITimer *aTimer)
{
  NS_PRECONDITION((nsnull != mShell), "Null pointer...");
  if (nsnull != mShell) {
    mShell->LoadURL(mUrlSpec.GetUnicode(), nsnull, PR_TRUE, nsIChannel::LOAD_NORMAL);
  }
  /*
   * LoadURL(...) will cancel all refresh timers... This causes the Timer and
   * its refreshData instance to be released...
   */
}


NS_IMETHODIMP
nsWebShell::RefreshURI(nsIURI* aURI, PRInt32 millis, PRBool repeat)
{

  nsresult rv = NS_OK;
  if (nsnull == aURI) {
    NS_PRECONDITION((aURI != nsnull), "Null pointer");
    rv = NS_ERROR_NULL_POINTER;
    goto done;
  }

  char* spec;
  aURI->GetSpec(&spec);
  rv = RefreshURL(spec, millis, repeat);
  nsCRT::free(spec);

done:
  return rv;
}

NS_IMETHODIMP
nsWebShell::RefreshURL(const char* aURI, PRInt32 millis, PRBool repeat)
{
  nsresult rv = NS_OK;
  nsITimer *timer=nsnull;
  refreshData *data;

  if (nsnull == aURI) {
    NS_PRECONDITION((aURI != nsnull), "Null pointer");
    rv = NS_ERROR_NULL_POINTER;
    goto done;
  }

  NS_NEWXPCOM(data, refreshData);
  if (nsnull == data) {
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto done;
  }

  // Set the reference count to one...
  NS_ADDREF(data);

  data->mShell = this;
  NS_ADDREF(data->mShell);

  data->mUrlSpec  = aURI;
  data->mDelay    = millis;
  data->mRepeat   = repeat;

  /* Create the timer. */
  if (NS_OK == NS_NewTimer(&timer)) {
      /* Add the timer to our array. */
      NS_LOCK_INSTANCE();
      mRefreshments.AppendElement(timer);
      timer->Init(data, millis);
      NS_UNLOCK_INSTANCE();
  }

  NS_RELEASE(data);

done:
  return rv;
}

NS_IMETHODIMP
nsWebShell::CancelRefreshURITimers(void)
{
  PRInt32 i;
  nsITimer* timer;

  /* Right now all we can do is cancel all the timers for this webshell. */
  NS_LOCK_INSTANCE();

  /* Walk the list backwards to avoid copying the array as it shrinks.. */
  for (i = mRefreshments.Count()-1; (0 <= i); i--) {
    timer=(nsITimer*)mRefreshments.ElementAt(i);
    /* Remove the entry from the list before releasing the timer.*/
    mRefreshments.RemoveElementAt(i);

    if (timer) {
      timer->Cancel();
      NS_RELEASE(timer);
    }
  }
  NS_UNLOCK_INSTANCE();

  return NS_OK;
}


//----------------------------------------------------------------------

/*
 * There are cases where netlib does things like add a trailing slash
 * to the url being retrieved.  We need to watch out for such
 * changes and update the currently loading url's entry in the history
 * list. UpdateHistoryEntry() does this.
 *
 * Assumptions:
 *
 *   1) aURL is the URL that was inserted into the history list in LoadURL()
 *   2) The load of aURL is in progress and this function is being called
 *      from one of the functions in nsIStreamListener implemented by nsWebShell.
 */
nsresult nsWebShell::CheckForTrailingSlash(nsIURI* aURL)
{

  PRInt32     curIndex=0;
  nsresult rv;

  /* Get current history index and url for it */
  rv = mSHist->GetCurrentIndex(&curIndex);

  /* Get the url that netlib passed us */
  char* spec;
  aURL->GetSpec(&spec);
 
  //Set it in session history
  if (NS_SUCCEEDED(rv) && !mTitle.IsEmpty()) {
    mSHist->SetTitleForIndex(curIndex, mTitle.GetUnicode());
    // Replace the top most history entry with the new url
    mSHist->SetURLForIndex(curIndex, spec);
  }
  nsCRT::free(spec);


  return NS_OK;
}

//----------------------------------------------------
NS_IMETHODIMP
nsWebShell::CanCutSelection(PRBool* aResult)
{
  nsresult rv = NS_OK;

  if (nsnull == aResult) {
    rv = NS_ERROR_NULL_POINTER;
  } else {
    *aResult = PR_FALSE;
  }

  return rv;
}

NS_IMETHODIMP
nsWebShell::CanCopySelection(PRBool* aResult)
{
  nsresult rv = NS_OK;

  if (nsnull == aResult) {
    rv = NS_ERROR_NULL_POINTER;
  } else {
    *aResult = PR_FALSE;
  }

  return rv;
}

NS_IMETHODIMP
nsWebShell::CanPasteSelection(PRBool* aResult)
{
  nsresult rv = NS_OK;

  if (nsnull == aResult) {
    rv = NS_ERROR_NULL_POINTER;
  } else {
    *aResult = PR_FALSE;
  }

  return rv;
}

NS_IMETHODIMP
nsWebShell::CutSelection(void)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWebShell::CopySelection(void)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWebShell::PasteSelection(void)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWebShell::SelectAll(void)
{
  nsresult rv;

  nsCOMPtr<nsIDocumentViewer> docViewer;
  rv = mContentViewer->QueryInterface(kIDocumentViewerIID,
                                      getter_AddRefs(docViewer));
  if (NS_FAILED(rv) || !docViewer) return rv;

  nsCOMPtr<nsIPresShell> presShell;
  rv = docViewer->GetPresShell(*getter_AddRefs(presShell));
  if (NS_FAILED(rv) || !presShell) return rv;

  nsCOMPtr<nsIDOMSelection> selection;
  rv = presShell->GetSelection(SELECTION_NORMAL, getter_AddRefs(selection));
  if (NS_FAILED(rv) || !selection) return rv;

  // Get the document object
  nsCOMPtr<nsIDocument> doc;
  rv = docViewer->GetDocument(*getter_AddRefs(doc));
  if (NS_FAILED(rv) || !doc) return rv;


  nsCOMPtr<nsIDOMHTMLDocument> htmldoc;
  rv = doc->QueryInterface(kIDOMHTMLDocumentIID, (void**)&htmldoc);
  if (NS_FAILED(rv) || !htmldoc) return rv;

  nsCOMPtr<nsIDOMHTMLElement>bodyElement;
  rv = htmldoc->GetBody(getter_AddRefs(bodyElement));
  if (NS_FAILED(rv) || !bodyElement) return rv;

  nsCOMPtr<nsIDOMNode>bodyNode = do_QueryInterface(bodyElement);
  if (!bodyNode) return NS_ERROR_NO_INTERFACE;

#if 0
  rv = selection->Collapse(bodyNode, 0);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDOMRange>range;
  rv = selection->GetRangeAt(0, getter_AddRefs(range));
  if (NS_FAILED(rv) || !range) return rv;
#endif

  rv = selection->ClearSelection();
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIDOMRange> range;
  rv = nsComponentManager::CreateInstance(kCDOMRangeCID, nsnull,
                                          nsIDOMRange::GetIID(),
                                          getter_AddRefs(range));

  rv = range->SelectNodeContents(bodyNode);
  if (NS_FAILED(rv)) return rv;

  rv = selection->AddRange(range);
  return rv;
}

NS_IMETHODIMP
nsWebShell::SelectNone(void)
{
  return NS_ERROR_FAILURE;
}


//----------------------------------------------------
NS_IMETHODIMP
nsWebShell::FindNext(const PRUnichar * aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound)
{
  return NS_ERROR_FAILURE;
}

// Methods from nsIProgressEventSink
NS_IMETHODIMP
nsWebShell::OnProgress(nsIChannel* channel, nsISupports* ctxt, 
    PRUint32 aProgress, 
    PRUint32 aProgressMax)
{
    if (nsnull != mDocLoaderObserver)
    {
        return mDocLoaderObserver->OnProgressURLLoad(
            mDocLoader,
            channel,
            aProgress,
            aProgressMax);
    }
    return NS_OK;
}

NS_IMETHODIMP
nsWebShell::OnStatus(nsIChannel* channel, nsISupports* ctxt, 
    const PRUnichar* aMsg)
{
    if (nsnull != mDocLoaderObserver)
    {
        nsAutoString temp(aMsg);
#ifdef DEBUG_gagan

#ifdef XP_UNIX
        printf("\033[33m"); // Start yellow
#endif

        char* tmp = temp.ToNewCString();
        printf("%s\n",tmp);
        CRTFREEIF(tmp);
#ifdef XP_UNIX
        printf("\033[0m"); // End colors
#endif

#endif // DEBUG_gagan
        nsresult rv =  mDocLoaderObserver->OnStatusURLLoad(
            mDocLoader,
            channel,
            temp);

#ifndef BUG_16273_FIXED
        //free the message-
        PRUnichar* tempChar = (PRUnichar*) aMsg;
        CRTFREEIF(tempChar);
#endif
        return rv;
    }
    return NS_OK;
}

nsresult nsWebShell::GetViewManager(nsIViewManager* *viewManager)
{
	nsresult rv = NS_ERROR_FAILURE;
	*viewManager = nsnull;
	do {
		if (nsnull == mContentViewer) break;
	
		nsCOMPtr<nsIDocumentViewer> docViewer;
		rv = mContentViewer->QueryInterface(kIDocumentViewerIID,
											getter_AddRefs(docViewer));
		if (NS_FAILED(rv)) break;
		
		nsCOMPtr<nsIPresContext> context;
		rv = docViewer->GetPresContext(*getter_AddRefs(context));
		if (NS_FAILED(rv)) break;
		
		nsCOMPtr<nsIPresShell> shell;
		rv = context->GetShell(getter_AddRefs(shell));
		if (NS_FAILED(rv)) break;
		
		rv = shell->GetViewManager(viewManager);
	} while (0);
	return rv;
}


//*****************************************************************************
// nsWebShell::nsIBaseWindow
//*****************************************************************************   

NS_IMETHODIMP nsWebShell::InitWindow(nativeWindow parentNativeWindow,
   nsIWidget* parentWidget, PRInt32 x, PRInt32 y, PRInt32 cx, PRInt32 cy)   
{
   NS_WARN_IF_FALSE(PR_FALSE, "NOT IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebShell::Create()
{
   NS_WARN_IF_FALSE(PR_FALSE, "NOT IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebShell::Destroy()
{
  nsresult rv = NS_OK;

  //Fire unload event before we blow anything away.
  if (nsnull != mScriptGlobal) {
    nsIDocumentViewer* docViewer;
    if (nsnull != mContentViewer &&
        NS_OK == mContentViewer->QueryInterface(kIDocumentViewerIID, (void**)&docViewer)) {
      nsIPresContext *presContext;
      if (NS_OK == docViewer->GetPresContext(presContext)) {
        nsEventStatus status = nsEventStatus_eIgnore;
        nsMouseEvent event;
        event.eventStructType = NS_EVENT;
        event.message = NS_PAGE_UNLOAD;
        rv = mScriptGlobal->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);

        NS_RELEASE(presContext);
      }
      NS_RELEASE(docViewer);
    }
  }

  // Stop any URLs that are currently being loaded...
  Stop();
  mDocLoader->Destroy();

  SetContainer(nsnull);
  SetDocLoaderObserver(nsnull);

  // Remove this webshell from its parent's child list
  nsCOMPtr<nsIWebShell> webShellParent(do_QueryInterface(mParent));

  if (webShellParent) {
    webShellParent->RemoveChild(this);
  }

  if (nsnull != mDocLoader) {
    mDocLoader->SetContainer(nsnull);
  }

  mContentViewer = nsnull;

  // Destroy our child web shells and release references to them
  DestroyChildren();
  return rv;
}

NS_IMETHODIMP nsWebShell::SetPosition(PRInt32 aX, PRInt32 aY)
{
   NS_PRECONDITION(nsnull != mWindow, "null window");

   if(mWindow)
      mWindow->Move(aX, aY);
   return NS_OK;
}

NS_IMETHODIMP nsWebShell::GetPosition(PRInt32* aX, PRInt32* aY)
{
   PRInt32 dummyHolder;

   return GetPositionAndSize(aX, aY, &dummyHolder, &dummyHolder);
}

NS_IMETHODIMP nsWebShell::SetSize(PRInt32 aCX, PRInt32 aCY, PRBool aRepaint)
{
   PRInt32 x = 0, y = 0;
   GetPosition(&x, &y);
   return SetPositionAndSize(x, y, aCX, aCY, aRepaint);
}

NS_IMETHODIMP nsWebShell::GetSize(PRInt32* aCX, PRInt32* aCY)
{
   PRInt32 dummyHolder;

   return GetPositionAndSize(&dummyHolder, &dummyHolder, aCX, aCY);
}

NS_IMETHODIMP nsWebShell::SetPositionAndSize(PRInt32 x, PRInt32 y, PRInt32 cx,
   PRInt32 cy, PRBool fRepaint)
{
     /*
  --dwc0001
  NS_PRECONDITION(nsnull != mWindow, "null window");
  */

  PRInt32 borderWidth  = 0;
  PRInt32 borderHeight = 0;
  if (nsnull != mWindow) {
    mWindow->GetBorderSize(borderWidth, borderHeight);
    // Don't have the widget repaint. Layout will generate repaint requests
    // during reflow
    mWindow->Resize(x, y, cx, cy, fRepaint);
  }

  mBounds.SetRect(x,y,cx,cy);   // set the webshells bounds --dwc0001

  // Set the size of the content area, which is the size of the window
  // minus the borders
  if (nsnull != mContentViewer) {
    nsRect rr(0, 0, cx-(borderWidth*2), cy-(borderHeight*2));
    mContentViewer->SetBounds(rr);
  }
  return NS_OK;
}

NS_IMETHODIMP nsWebShell::GetPositionAndSize(PRInt32* aX, PRInt32* aY, 
   PRInt32* aCX, PRInt32* aCY)
{
   nsRect result;

   if(mWindow)
      mWindow->GetClientBounds(result);
   else
      result = mBounds;

   *aX = result.x;
   *aY = result.y;
   *aCX = result.width;
   *aCY = result.height;

   return NS_OK;
}

NS_IMETHODIMP nsWebShell::Repaint(PRBool aForce)
{
  /*
  --dwc0001
  NS_PRECONDITION(nsnull != mWindow, "null window");
  */

  if (nsnull != mWindow) {
    mWindow->Invalidate(aForce);
  }

#if 0
	if (nsnull != mWindow) {
		mWindow->Invalidate(aForce);
	}
	return NS_OK;
#else
	nsresult rv;
	nsCOMPtr<nsIViewManager> viewManager;
	rv = GetViewManager(getter_AddRefs(viewManager));
	if (NS_SUCCEEDED(rv) && viewManager) {
		rv = viewManager->UpdateAllViews(0);
	}
	return rv;
#endif
}

NS_IMETHODIMP nsWebShell::GetParentWidget(nsIWidget** parentWidget)
{
   NS_WARN_IF_FALSE(PR_FALSE, "NOT IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebShell::SetParentWidget(nsIWidget* parentWidget)
{
   NS_WARN_IF_FALSE(PR_FALSE, "NOT IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebShell::GetParentNativeWindow(nativeWindow* parentNativeWindow)
{
   NS_WARN_IF_FALSE(PR_FALSE, "NOT IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebShell::SetParentNativeWindow(nativeWindow parentNativeWindow)
{
   NS_WARN_IF_FALSE(PR_FALSE, "NOT IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebShell::GetVisibility(PRBool* aVisibility)
{
   NS_WARN_IF_FALSE(PR_FALSE, "NOT IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebShell::SetVisibility(PRBool aVisibility)
{
   if(mWindow)
      mWindow->Show(aVisibility);

   if(aVisibility)
      {
      if(mContentViewer)
         mContentViewer->Show();
      }
   else
      {
      if(mContentViewer)
         mContentViewer->Hide();
      }
   return NS_OK;
}

NS_IMETHODIMP nsWebShell::GetMainWidget(nsIWidget** mainWidget)
{
   NS_WARN_IF_FALSE(PR_FALSE, "NOT IMPLEMENTED");
   return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsWebShell::SetFocus()
{
  /*
  --dwc0001
  NS_PRECONDITION(nsnull != mWindow, "null window");
  */

  if (nsnull != mWindow) {
    mWindow->SetFocus();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::FocusAvailable(nsIBaseWindow* aCurrentFocus, PRBool* aTookFocus)
{
   NS_ENSURE_ARG_POINTER(aTookFocus);
   // Next person we should call is first the parent otherwise the 
   // docshell tree owner.
   nsCOMPtr<nsIBaseWindow> nextCallWin(do_QueryInterface(mParent));
   if(!nextCallWin)
      {//XXX Enable this when docShellTreeOwner is added
      //nextCallWin = do_QueryInterface(mDocShellTreeOwner);
      }

   //If the current focus is us, offer it to the next owner.
   if(aCurrentFocus == NS_STATIC_CAST(nsIBaseWindow*, this))
      {
      if(nextCallWin)
         return nextCallWin->FocusAvailable(aCurrentFocus, aTookFocus);
      return NS_OK;
      }

   //Otherwise, check the chilren and offer it to the next sibling.
   PRInt32 i;
   PRInt32 n = mChildren.Count();
   for(i = 0; i < n; i++)
      {
      nsCOMPtr<nsIBaseWindow> 
         child(do_QueryInterface((nsISupports*)mChildren.ElementAt(i)));
      if(child.get() == aCurrentFocus)
         {
         while(++i < n)
            {
            child = do_QueryInterface((nsISupports*)mChildren.ElementAt(i));
            if(NS_SUCCEEDED(child->SetFocus()))
               {
               *aTookFocus = PR_TRUE;
               return NS_OK;
               }
            }
         }
      }
   if(nextCallWin)
      return nextCallWin->FocusAvailable(aCurrentFocus, aTookFocus);
   return NS_OK;
}

NS_IMETHODIMP nsWebShell::GetTitle(PRUnichar** aTitle)
{
   return nsDocShell::GetTitle(aTitle);
}

NS_IMETHODIMP nsWebShell::SetTitle(const PRUnichar* aTitle)
{
  // Record local title
  mTitle = aTitle;

  // Title's set on the top level web-shell are passed ont to the container
  nsCOMPtr<nsIDocShellTreeItem> parent;
  GetSameTypeParent(getter_AddRefs(parent));
  if (!parent) {
    nsIBrowserWindow *browserWindow = GetBrowserWindow();
    if (nsnull != browserWindow) {
      browserWindow->SetTitle(aTitle);
      NS_RELEASE(browserWindow);

      // Oh this hack sucks. But there isn't any other way that I can
      // reliably get the title text. Sorry.
      do {
        nsresult rv;
        NS_WITH_SERVICE(nsIGlobalHistory, history, "component://netscape/browser/global-history", &rv);
        if (NS_FAILED(rv)) break;

        rv = history->SetPageTitle(nsCAutoString(mURL), aTitle);
        if (NS_FAILED(rv)) break;
      } while (0);
    }
  }

  return NS_OK;
}

//*****************************************************************************
// nsWebShell::nsIDocShell
//*****************************************************************************   

NS_IMETHODIMP nsWebShell::LoadURI(nsIURI* aUri, 
   nsIPresContext* presContext)
{
   return nsDocShell::LoadURI(aUri, presContext);
}

NS_IMETHODIMP nsWebShell::LoadURIVia(nsIURI* aUri, 
   nsIPresContext* aPresContext, PRUint32 aAdapterBinding)
{
   return nsDocShell::LoadURIVia(aUri, aPresContext, aAdapterBinding);
}

NS_IMETHODIMP nsWebShell::GetDocument(nsIDOMDocument** aDocument)
{
   return nsDocShell::GetDocument(aDocument);
}

NS_IMETHODIMP nsWebShell::GetCurrentURI(nsIURI** aURI)
{
   return nsDocShell::GetCurrentURI(aURI);
}

NS_IMETHODIMP nsWebShell::SetDocument(nsIDOMDocument *aDOMDoc, 
   nsIDOMElement *aRootNode)
{
  // The tricky part is bypassing the normal load process and just putting a document into
  // the webshell.  This is particularly nasty, since webshells don't normally even know
  // about their documents

  // (1) Create a document viewer 
  nsCOMPtr<nsIContentViewer> documentViewer;
  nsCOMPtr<nsIDocumentLoaderFactory> docFactory;
  static NS_DEFINE_CID(kLayoutDocumentLoaderFactoryCID, NS_LAYOUT_DOCUMENT_LOADER_FACTORY_CID);
  NS_ENSURE_SUCCESS(nsComponentManager::CreateInstance(kLayoutDocumentLoaderFactoryCID, nsnull, 
                                                       nsIDocumentLoaderFactory::GetIID(),
                                                       (void**)getter_AddRefs(docFactory)),
                    NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDOMDoc);
  if (!doc) { return NS_ERROR_NULL_POINTER; }

  NS_ENSURE_SUCCESS(docFactory->CreateInstanceForDocument(NS_STATIC_CAST(nsIContentViewerContainer*, (nsIWebShell*)this),
                                                          doc,
                                                          "view",
                                                          getter_AddRefs(documentViewer)),
                    NS_ERROR_FAILURE); 

  // (2) Feed the webshell to the content viewer
  NS_ENSURE_SUCCESS(documentViewer->SetContainer((nsIWebShell*)this), NS_ERROR_FAILURE);

  // (3) Tell the content viewer container to embed the content viewer.
  //     (This step causes everything to be set up for an initial flow.)
  NS_ENSURE_SUCCESS(Embed(documentViewer, "view", nsnull), NS_ERROR_FAILURE);

  // XXX: It would be great to get rid of this dummy channel!
  const nsAutoString uriString = "about:blank";
  nsCOMPtr<nsIURI> uri;
  NS_ENSURE_SUCCESS(NS_NewURI(getter_AddRefs(uri), uriString), NS_ERROR_FAILURE);
  if (!uri) { return NS_ERROR_OUT_OF_MEMORY; }

  nsCOMPtr<nsIChannel> dummyChannel;
  NS_ENSURE_SUCCESS(NS_OpenURI(getter_AddRefs(dummyChannel), uri, nsnull), NS_ERROR_FAILURE);

  // (4) fire start document load notification
  nsCOMPtr<nsIStreamListener> outStreamListener;  // a valid pointer is required for the returned stream listener
    // XXX: warning: magic cookie!  should get string "view delayedContentLoad"
    //      from somewhere, maybe nsIHTMLDocument?
  NS_ENSURE_SUCCESS(doc->StartDocumentLoad("view delayedContentLoad", dummyChannel, nsnull, NS_STATIC_CAST(nsIContentViewerContainer*, (nsIWebShell*)this),
                                           getter_AddRefs(outStreamListener)), 
                    NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(OnStartDocumentLoad(mDocLoader, uri, "load"), NS_ERROR_FAILURE);

  // (5) hook up the document and its content
  nsCOMPtr<nsIContent> rootContent = do_QueryInterface(aRootNode);
  if (!doc) { return NS_ERROR_OUT_OF_MEMORY; }
  NS_ENSURE_SUCCESS(rootContent->SetDocument(doc, PR_FALSE), NS_ERROR_FAILURE);
  doc->SetRootContent(rootContent);
  rootContent->SetDocument(doc, PR_TRUE);

  // (6) reflow the document
  SetScrolling(-1, PR_FALSE);
  PRInt32 i;
  PRInt32 ns = doc->GetNumberOfShells();
  for (i = 0; i < ns; i++) 
  {
    nsCOMPtr<nsIPresShell> shell(dont_AddRef(doc->GetShellAt(i)));
    if (shell) 
    {
      // Make shell an observer for next time
      NS_ENSURE_SUCCESS(shell->BeginObservingDocument(), NS_ERROR_FAILURE);

      // Resize-reflow this time
      nsCOMPtr<nsIDocumentViewer> docViewer = do_QueryInterface(documentViewer);
      if (!docViewer) { return NS_ERROR_OUT_OF_MEMORY; }
      nsCOMPtr<nsIPresContext> presContext;
      NS_ENSURE_SUCCESS(docViewer->GetPresContext(*(getter_AddRefs(presContext))), NS_ERROR_FAILURE);
      if (!presContext) { return NS_ERROR_OUT_OF_MEMORY; }
      float p2t;
      presContext->GetScaledPixelsToTwips(&p2t);

      nsRect r;
      GetPositionAndSize(&r.x, &r.y, &r.width, &r.height);
      NS_ENSURE_SUCCESS(shell->InitialReflow(NSToCoordRound(r.width * p2t), NSToCoordRound(r.height * p2t)), NS_ERROR_FAILURE);

      // Now trigger a refresh
      nsCOMPtr<nsIViewManager> vm;
      NS_ENSURE_SUCCESS(shell->GetViewManager(getter_AddRefs(vm)), NS_ERROR_FAILURE);
      if (vm) 
      {
        PRBool enabled;
        documentViewer->GetEnableRendering(&enabled);
        if (enabled) {
          vm->EnableRefresh();
        }
        NS_ENSURE_SUCCESS(vm->SetWindowDimensions(NSToCoordRound(r.width * p2t), 
                                                  NSToCoordRound(r.height * p2t)), 
                          NS_ERROR_FAILURE);
      }
    }
  }

  // (7) fire end document load notification
	mProcessedEndDocumentLoad = PR_FALSE;
  nsresult rv = NS_OK;
  NS_ENSURE_SUCCESS(OnEndDocumentLoad(mDocLoader, dummyChannel, rv, this), NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);  // test the resulting out-param separately

  return NS_OK;
}

NS_IMETHODIMP nsWebShell::GetPresContext(nsIPresContext** aPresContext)
{
   return nsDocShell::GetPresContext(aPresContext);
}

NS_IMETHODIMP nsWebShell::GetPresShell(nsIPresShell** aPresShell)
{
   return nsDocShell::GetPresShell(aPresShell);
}

NS_IMETHODIMP nsWebShell::GetContentViewer(nsIContentViewer** aContentViewer)
{
   return nsDocShell::GetContentViewer(aContentViewer);
}

NS_IMETHODIMP
nsWebShell::GetChromeEventHandler(nsIChromeEventHandler** aChromeEventHandler)
{
   return nsDocShell::GetChromeEventHandler(aChromeEventHandler);
}

NS_IMETHODIMP
nsWebShell::SetChromeEventHandler(nsIChromeEventHandler* aChromeEventHandler)
{
   return nsDocShell::SetChromeEventHandler(aChromeEventHandler);
}

NS_IMETHODIMP nsWebShell::GetParentURIContentListener(nsIURIContentListener**
   aParent)
{
   return nsDocShell::GetParentURIContentListener(aParent);
}

NS_IMETHODIMP nsWebShell::SetParentURIContentListener(nsIURIContentListener*
   aParent)
{
   return nsDocShell::SetParentURIContentListener(aParent);
}

NS_IMETHODIMP nsWebShell::GetPrefs(nsIPref** aPrefs)
{
   NS_ENSURE_ARG_POINTER(aPrefs);

  *aPrefs = mPrefs;
  NS_IF_ADDREF(*aPrefs);
  return NS_OK;
}

NS_IMETHODIMP nsWebShell::SetPrefs(nsIPref* aPrefs)
{
  NS_IF_RELEASE(mPrefs);
  mPrefs = aPrefs;
  NS_IF_ADDREF(mPrefs);
  return NS_OK;
}

NS_IMETHODIMP nsWebShell::GetZoom(float* aZoom)
{
  *aZoom = mZoom;
  return NS_OK;
}

NS_IMETHODIMP nsWebShell::SetZoom(float aZoom)
{
  mZoom = aZoom;

  if (mDeviceContext)
    mDeviceContext->SetZoom(mZoom);

  if (mContentViewer) {
    nsIDocumentViewer* docv = nsnull;
    mContentViewer->QueryInterface(kIDocumentViewerIID, (void**) &docv);
    if (nsnull != docv) {
      nsIPresContext* cx = nsnull;
      docv->GetPresContext(cx);
      if (nsnull != cx) {
        nsIPresShell  *shell = nsnull;
        cx->GetShell(&shell);
        if (nsnull != shell) {
          nsIViewManager  *vm = nsnull;
          shell->GetViewManager(&vm);
          if (nsnull != vm) {
            nsIView *rootview = nsnull;
            nsIScrollableView *sv = nsnull;
            vm->GetRootScrollableView(&sv);
            if (nsnull != sv)
              sv->ComputeScrollOffsets();
            vm->GetRootView(rootview);
            if (nsnull != rootview)
              vm->UpdateView(rootview, 0);
              NS_RELEASE(vm);
          }
          NS_RELEASE(shell);
        }
        NS_RELEASE(cx);
      }
      NS_RELEASE(docv);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsWebShell::GetDocLoaderObserver(nsIDocumentLoaderObserver * *aDocLoaderObserver)
{
   return nsDocShell::GetDocLoaderObserver(aDocLoaderObserver);
}

NS_IMETHODIMP 
nsWebShell::SetDocLoaderObserver(nsIDocumentLoaderObserver * aDocLoaderObserver)
{
   return nsDocShell::SetDocLoaderObserver(aDocLoaderObserver);
}

NS_IMETHODIMP
nsWebShell::GetMarginWidth(PRInt32* aWidth)
{
   return nsDocShell::GetMarginWidth(aWidth);
}

NS_IMETHODIMP
nsWebShell::SetMarginWidth(PRInt32 aWidth)
{
   return nsDocShell::SetMarginWidth(aWidth);
}

NS_IMETHODIMP
nsWebShell::GetMarginHeight(PRInt32* aHeight)
{
   return nsDocShell::GetMarginHeight(aHeight);
}

NS_IMETHODIMP
nsWebShell::SetMarginHeight(PRInt32 aHeight)
{
   return nsDocShell::SetMarginHeight(aHeight);
}

//*****************************************************************************
// nsWebShell::nsIScriptGlobalObjectOwner
//*****************************************************************************   

NS_IMETHODIMP
nsWebShell::GetScriptGlobalObject(nsIScriptGlobalObject** aGlobal)
{
  NS_PRECONDITION(nsnull != aGlobal, "null arg");
  nsresult res = NS_OK;

  res = CreateScriptEnvironment();

  if (NS_SUCCEEDED(res)) {
    *aGlobal = mScriptGlobal;
    NS_IF_ADDREF(mScriptGlobal);
  }

  return res;
}

NS_IMETHODIMP 
nsWebShell::ReportScriptError(const char* aErrorString,
                              const char* aFileName,
                              PRInt32     aLineNo,
                              const char* aLineBuf)
{
   return nsDocShell::ReportScriptError(aErrorString, aFileName, aLineNo, 
      aLineBuf);
}

//----------------------------------------------------------------------

// Factory code for creating nsWebShell's

class nsWebShellFactory : public nsIFactory
{
public:
  nsWebShellFactory();
  virtual ~nsWebShellFactory();

  NS_DECL_ISUPPORTS

  // nsIFactory methods
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock);
};

nsWebShellFactory::nsWebShellFactory()
{
   NS_INIT_REFCNT();
}

nsWebShellFactory::~nsWebShellFactory()
{
}

NS_IMPL_ADDREF(nsWebShellFactory);
NS_IMPL_RELEASE(nsWebShellFactory);

NS_INTERFACE_MAP_BEGIN(nsWebShellFactory)
   NS_INTERFACE_MAP_ENTRY(nsISupports)
   NS_INTERFACE_MAP_ENTRY(nsIFactory)
NS_INTERFACE_MAP_END

nsresult
nsWebShellFactory::CreateInstance(nsISupports *aOuter,
                                  const nsIID &aIID,
                                  void **aResult)
{
  nsresult rv;
  nsWebShell *inst;

  NS_ENSURE_ARG_POINTER(aResult);
  NS_ENSURE_NO_AGGREGATION(aOuter);
  *aResult = NULL;

  NS_NEWXPCOM(inst, nsWebShell);
  NS_ENSURE_TRUE(inst, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

  return rv;
}

nsresult
nsWebShellFactory::LockFactory(PRBool aLock)
{
  // Not implemented in simplest case.
  return NS_OK;
}

extern "C" NS_WEB nsresult
NS_NewWebShellFactory(nsIFactory** aFactory)
{
  nsresult rv = NS_OK;
  nsIFactory* inst = new nsWebShellFactory();
  if (nsnull == inst) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    NS_ADDREF(inst);
  }
  *aFactory = inst;
  return rv;
}
