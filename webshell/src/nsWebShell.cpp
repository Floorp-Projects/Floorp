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
#include "nsIWebShell.h"
#include "nsIDocumentLoader.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIClipboardCommands.h"
#include "nsIDeviceContext.h"
#include "nsILinkHandler.h"
#include "nsIStreamListener.h"
#include "nsIPrompt.h"
#include "nsNeckoUtil.h"
#include "nsIProtocolHandler.h"
#include "nsIDNSService.h"
#include "nsIRefreshURI.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContextOwner.h"
#include "nsIDocumentLoaderObserver.h"
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
#include "nsIStreamObserver.h"
#include "nsIWebShellServices.h"
#include "nsIGlobalHistory.h"
#include "prmem.h"
#include "nsXPIDLString.h"
#include "nsIDOMHTMLElement.h"
#include "nsIDOMHTMLDocument.h"
#include "nsLayoutCID.h"
#include "nsIDOMRange.h"
#include "nsIFrameReflow.h"

#include "nsILocaleService.h"
static NS_DEFINE_CID(kLocaleServiceCID, NS_LOCALESERVICE_CID);

#ifdef XP_PC
#include <windows.h>
#endif

//XXX used for nsIStreamObserver implementation.  This sould be replaced by DocLoader
//    notifications...
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIURL.h"

//XXX for nsIPostData; this is wrong; we shouldn't see the nsIDocument type
#include "nsIDocument.h"

#ifdef DEBUG
#undef NOISY_LINKS
#else
#undef NOISY_LINKS
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
#define OLD_HISTORY 1

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

static nsAutoString LinkCommand("linkclick");
//----------------------------------------------------------------------

class nsWebShell : public nsIWebShell,
                   public nsIWebShellContainer,
                   public nsIWebShellServices,
                   public nsILinkHandler,
                   public nsIScriptContextOwner,
                   public nsIDocumentLoaderObserver,
                   public nsIPrompt,
                   public nsIRefreshURI,
//                   public nsIStreamObserver,
                   public nsIClipboardCommands
{
public:
  nsWebShell();
  virtual ~nsWebShell();

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIContentViewerContainer
  NS_IMETHOD QueryCapability(const nsIID &aIID, void** aResult);
  NS_IMETHOD Embed(nsIContentViewer* aDocViewer,
                   const char* aCommand,
                   nsISupports* aExtraInfo);
  NS_IMETHOD GetContentViewer(nsIContentViewer** aResult);

  // nsIWebShell
  NS_IMETHOD Init(nsNativeWidget aNativeParent,
                  PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h,
                  nsScrollPreference aScrolling = nsScrollPreference_kAuto,
                  PRBool aAllowPlugins = PR_TRUE,
                  PRBool aIsSunkenBorder = PR_FALSE);
  NS_IMETHOD Destroy(void);
  NS_IMETHOD GetBounds(PRInt32 &x, PRInt32 &y, PRInt32 &w, PRInt32 &h);
  NS_IMETHOD SetBounds(PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h);
  NS_IMETHOD SizeToContent();
  NS_IMETHOD MoveTo(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD Show();
  NS_IMETHOD Hide();
  NS_IMETHOD SetFocus();
  NS_IMETHOD RemoveFocus();
  NS_IMETHOD Repaint(PRBool aForce);
  NS_IMETHOD SetContentViewer(nsIContentViewer* aViewer);
  NS_IMETHOD SetContainer(nsIWebShellContainer* aContainer);
  NS_IMETHOD GetContainer(nsIWebShellContainer*& aResult);
  NS_IMETHOD SetObserver(nsIStreamObserver* anObserver);
  NS_IMETHOD GetObserver(nsIStreamObserver*& aResult);
  NS_IMETHOD SetDocLoaderObserver(nsIDocumentLoaderObserver* anObserver);
  NS_IMETHOD GetDocLoaderObserver(nsIDocumentLoaderObserver*& aResult);
  NS_IMETHOD SetPrefs(nsIPref* aPrefs);
  NS_IMETHOD GetPrefs(nsIPref*& aPrefs);
  NS_IMETHOD GetRootWebShell(nsIWebShell*& aResult);
  NS_IMETHOD GetRootWebShellEvenIfChrome(nsIWebShell*& aResult);
  NS_IMETHOD SetParent(nsIWebShell* aParent);
  NS_IMETHOD GetParent(nsIWebShell*& aParent);
  NS_IMETHOD GetParentEvenIfChrome(nsIWebShell*& aParent);
  NS_IMETHOD GetChildCount(PRInt32& aResult);
  NS_IMETHOD AddChild(nsIWebShell* aChild);
  NS_IMETHOD RemoveChild(nsIWebShell* aChild);
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIWebShell*& aResult);
  NS_IMETHOD GetName(const PRUnichar** aName);
  NS_IMETHOD SetName(const PRUnichar* aName);
  NS_IMETHOD FindChildWithName(const PRUnichar* aName,
                               nsIWebShell*& aResult);

  NS_IMETHOD SetWebShellType(nsWebShellType aWebShellType);
  NS_IMETHOD GetWebShellType(nsWebShellType& aWebShellType);
  NS_IMETHOD GetContainingChromeElement(nsIContent** aResult);
  NS_IMETHOD SetContainingChromeElement(nsIContent* aChromeElement);

  NS_IMETHOD GetMarginWidth (PRInt32& aWidth);
  NS_IMETHOD SetMarginWidth (PRInt32  aWidth);
  NS_IMETHOD GetMarginHeight(PRInt32& aWidth);
  NS_IMETHOD SetMarginHeight(PRInt32  aHeight);
  NS_IMETHOD GetScrolling(PRInt32& aScrolling);
  NS_IMETHOD SetScrolling(PRInt32 aScrolling, PRBool aSetCurrentAndInitial = PR_TRUE);
  NS_IMETHOD GetIsFrame(PRBool& aIsFrame);
  NS_IMETHOD SetIsFrame(PRBool aIsFrame);
  NS_IMETHOD SetZoom(float aZoom);
  NS_IMETHOD GetZoom(float *aZoom);

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

  // Chrome api's
  NS_IMETHOD SetTitle(const PRUnichar* aTitle);
  NS_IMETHOD GetTitle(const PRUnichar** aResult);

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

  // nsIScriptContextOwner
  NS_IMETHOD GetScriptContext(nsIScriptContext **aContext);
  NS_IMETHOD GetScriptGlobalObject(nsIScriptGlobalObject **aGlobal);
  NS_IMETHOD ReleaseScriptContext(nsIScriptContext *aContext);
  NS_IMETHOD ReportScriptError(const char* aErrorString,
                               const char* aFileName,
                               PRInt32     aLineNo,
                               const char* aLineBuf);

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

  // nsIPrompt
  NS_DECL_NSIPROMPT

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
  NS_IMETHOD GetDefaultCharacterSet (const PRUnichar** aDefaultCharacterSet);
  NS_IMETHOD SetDefaultCharacterSet (const PRUnichar*  aDefaultCharacterSet);

  NS_IMETHOD GetForceCharacterSet (const PRUnichar** aForceCharacterSet);
  NS_IMETHOD SetForceCharacterSet (const PRUnichar*  aForceCharacterSet);

  NS_IMETHOD GetCharacterSetHint (const PRUnichar** oHintCharset, nsCharsetSource* oSource);

  NS_IMETHOD SetSessionHistory(nsISessionHistory * aSHist);
  NS_IMETHOD GetSessionHistory(nsISessionHistory *& aResult);
  NS_IMETHOD SetIsInSHist(PRBool aIsFrame);
  NS_IMETHOD GetIsInSHist(PRBool& aIsFrame);
  NS_IMETHOD GetURL(const PRUnichar** aURL);
  NS_IMETHOD SetURL(const PRUnichar* aURL);
  NS_IMETHOD SetUrlDispatcher(nsIUrlDispatcher * anObserver);
  NS_IMETHOD GetUrlDispatcher(nsIUrlDispatcher *& aResult);

protected:
  void InitFrameData(PRBool aCompleteInitScrolling);
  nsresult CheckForTrailingSlash(nsIURI* aURL);
  nsresult StopBeforeRequestingURL(void);
  nsresult StopAfterURLAvailable(void);
  nsresult GetViewManager(nsIViewManager* *viewManager);

  nsIEventQueue* mThreadEventQueue;
  nsIScriptGlobalObject *mScriptGlobal;
  nsIScriptContext* mScriptContext;

  nsIStreamObserver * mObserver;
  nsIWebShellContainer* mContainer;
  nsIContentViewer* mContentViewer;
  nsIDeviceContext* mDeviceContext;
  nsIPref* mPrefs;
  nsIWidget* mWindow;
  nsIDocumentLoader* mDocLoader;
  nsIDocumentLoaderObserver* mDocLoaderObserver;
  nsIUrlDispatcher *  mUrlDispatcher;

  nsIWebShell* mParent;
  nsVoidArray mChildren;
  nsString mName;
  nsString mDefaultCharacterSet;


  nsVoidArray mHistory;
  PRInt32 mHistoryIndex;


  nsIGlobalHistory* mHistoryService;
  nsISessionHistory * mSHist;

  nsString mTitle;
  nsString mURL;
  nsString mReferrer;

  nsString mOverURL;
  nsString mOverTarget;

  PRPackedBool mIsInSHist;
  PRPackedBool mIsFrame;
  PRPackedBool mFailedToLoadHistoryService;

  nsScrollPreference mScrollPref;
  PRInt32 mMarginWidth;
  PRInt32 mMarginHeight;
  PRInt32 mScrolling[2];
  nsVoidArray mRefreshments;

  nsWebShellType mWebShellType;
  nsIContent* mChromeElement; // Weak reference.

  nsISupports* mHistoryState; // Weak reference.  Session history owns this.

  void ReleaseChildren();
  void DestroyChildren();
  nsresult CreateScriptEnvironment();
  nsresult DoLoadURL(nsIURI * aUri, 
                     const char* aCommand,
                     nsIInputStream* aPostDataStream,
                     nsLoadFlags aType,
                     const PRUint32 aLocalIP,
                     const PRUnichar* aReferrer);

  float mZoom;

  static nsIPluginHost    *mPluginHost;
  static nsIPluginManager *mPluginManager;
  static PRUint32          mPluginInitCnt;
  PRBool mProcessedEndDocumentLoad;

  // XXX store mHintCharset and mHintCharsetSource here untill we find out a good cood path
  nsString mHintCharset;
  nsCharsetSource mHintCharsetSource;
  PRBool mViewSource;
  nsString mForceCharacterSet;

  // if there is no mWindow, this will keep track of the bounds  --dwc0001
  nsRect  mBounds;

#ifdef NS_DEBUG
private:
  // We're counting the number of |nsWebShells| to help find leaks
  static unsigned long gNumberOfWebShells;

public:
  static unsigned long TotalWebShellsInExistence() { return gNumberOfWebShells; }
#endif
};

#ifdef NS_DEBUG
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
static NS_DEFINE_IID(kIDeviceContextIID,      NS_IDEVICE_CONTEXT_IID);
static NS_DEFINE_IID(kIDocumentLoaderIID,     NS_IDOCUMENTLOADER_IID);
static NS_DEFINE_IID(kIFactoryIID,            NS_IFACTORY_IID);
static NS_DEFINE_IID(kIScriptContextOwnerIID, NS_ISCRIPTCONTEXTOWNER_IID);
static NS_DEFINE_IID(kIStreamObserverIID,     NS_ISTREAMOBSERVER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kRefreshURIIID,          NS_IREFRESHURI_IID);

static NS_DEFINE_IID(kIWebShellIID,           NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIWebShellServicesIID,   NS_IWEB_SHELL_SERVICES_IID);
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
static NS_DEFINE_IID(kISessionHistoryIID,  NS_ISESSION_HISTORY_IID);
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
nsWebShell::nsWebShell()
{
#ifdef NS_DEBUG
  // We're counting the number of |nsWebShells| to help find leaks
  ++gNumberOfWebShells;
#endif

  NS_INIT_REFCNT();
  mHistoryIndex = -1;
  mScrollPref = nsScrollPreference_kAuto;
  mScriptGlobal = nsnull;
  mScriptContext = nsnull;
  mThreadEventQueue = nsnull;
  InitFrameData(PR_TRUE);
  mIsFrame = PR_FALSE;
  mWebShellType = nsWebShellContent;
  mChromeElement = nsnull;
  mSHist = nsnull;
  mIsInSHist = PR_FALSE;
  mFailedToLoadHistoryService = PR_FALSE;
  mDefaultCharacterSet = "";
  mProcessedEndDocumentLoad = PR_FALSE;
  mHintCharset = "";
  mHintCharsetSource = kCharsetUninitialized;
  mViewSource=PR_FALSE;
  mForceCharacterSet = "";
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

  NS_IF_RELEASE(mWindow);
  NS_IF_RELEASE(mThreadEventQueue);
  NS_IF_RELEASE(mContentViewer);
  NS_IF_RELEASE(mDeviceContext);
  NS_IF_RELEASE(mPrefs);
  NS_IF_RELEASE(mContainer);
  NS_IF_RELEASE(mObserver);

  if (nsnull != mScriptGlobal) {
    mScriptGlobal->SetWebShell(nsnull);
    NS_RELEASE(mScriptGlobal);
  }
  if (nsnull != mScriptContext) {
    mScriptContext->SetOwner(nsnull);
    NS_RELEASE(mScriptContext);
  }

  InitFrameData(PR_TRUE);
  mIsFrame = PR_FALSE;

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

#ifdef NS_DEBUG
  // We're counting the number of |nsWebShells| to help find leaks
  --gNumberOfWebShells;
#endif
}

void nsWebShell::InitFrameData(PRBool aCompleteInitScrolling)
{
  if (aCompleteInitScrolling) {
    mScrolling[0] = -1;
    mScrolling[1] = -1;
    mMarginWidth  = -1;
    mMarginHeight = -1;
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
    nsIWebShell* shell = (nsIWebShell*) mChildren.ElementAt(i);
    shell->SetParent(nsnull);

    //Break circular reference of webshell to contentviewer
    shell->SetContentViewer(nsnull);
    NS_RELEASE(shell);
  }
  mChildren.Clear();
}

void
nsWebShell::DestroyChildren()
{
  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) {
    nsIWebShell* shell = (nsIWebShell*) mChildren.ElementAt(i);
    shell->SetParent(nsnull);
    shell->Destroy();
    NS_RELEASE(shell);
  }
  mChildren.Clear();
}


NS_IMPL_ADDREF(nsWebShell)
NS_IMPL_RELEASE(nsWebShell)

nsresult
nsWebShell::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  nsresult rv = NS_NOINTERFACE;

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIWebShellServicesIID)) {
    *aInstancePtr = (void*)(nsIWebShellServices*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIWebShellIID)) {
    *aInstancePtr = (void*)(nsIWebShell*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIContentViewerContainerIID)) {
    *aInstancePtr = (void*)(nsIContentViewerContainer*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptContextOwnerIID)) {
    *aInstancePtr = (void*)(nsIScriptContextOwner*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDocumentLoaderObserverIID)) {
    *aInstancePtr = (void*)(nsIDocumentLoaderObserver*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIWebShellContainerIID)) {
    *aInstancePtr = (void*)(nsIWebShellContainer*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kILinkHandlerIID)) {
    //I added this for plugin support of jumping
    //through links. maybe there is a better way... MMP
    *aInstancePtr = (void*)(nsILinkHandler*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIPrompt::GetIID())) {
    *aInstancePtr = (void*) ((nsIPrompt*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kRefreshURIIID)) {
    *aInstancePtr = (void*) ((nsIRefreshURI*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIClipboardCommandsIID)) {
    *aInstancePtr = (void*) ((nsIClipboardCommands*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsIWebShell*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }

#if defined(NS_DEBUG)
  /*
   * Check for the debug-only interface indicating thread-safety
   */
  static NS_DEFINE_IID(kIsThreadsafeIID, NS_ISTHREADSAFE_IID);
  if (aIID.Equals(kIsThreadsafeIID)) {
    return NS_OK;
  }
#endif /* NS_DEBUG */

  return rv;
}

NS_IMETHODIMP
nsWebShell::QueryCapability(const nsIID &aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(kILinkHandlerIID)) {
    *aInstancePtr = (void*) ((nsILinkHandler*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIScriptContextOwnerIID)) {
    *aInstancePtr = (void*) ((nsIScriptContextOwner*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  //XXX this seems a little wrong. MMP
  if (nsnull != mPluginManager)
    return mPluginManager->QueryInterface(aIID, aInstancePtr);

  return NS_NOINTERFACE;
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

  NS_IF_RELEASE(mContentViewer);
  if (nsnull != mScriptContext) {
    mScriptContext->GC();
  }
  mContentViewer = aContentViewer;
  NS_ADDREF(aContentViewer);

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
nsWebShell::GetContentViewer(nsIContentViewer** aResult)
{
  nsresult rv = NS_OK;

  if (nsnull == aResult) {
    rv = NS_ERROR_NULL_POINTER;
  } else {
    *aResult = mContentViewer;
    NS_IF_ADDREF(mContentViewer);
  }
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

  rv = eventService->GetThreadEventQueue(PR_GetCurrentThread(), &mThreadEventQueue);
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
  if (nsnull != mParent) {
    nsIDocumentLoader* parentLoader;

    // Create a child document loader...
    rv = mParent->GetDocumentLoader(parentLoader);
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
  mDocLoader->SetContainer(this);

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
nsWebShell::Destroy()
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
        rv = mScriptGlobal->HandleDOMEvent(*presContext, &event, nsnull, NS_EVENT_FLAG_INIT, status);

        NS_RELEASE(presContext);
      }
      NS_RELEASE(docViewer);
    }
  }

  // Stop any URLs that are currently being loaded...
  Stop();
  mDocLoader->Destroy();

  SetContainer(nsnull);
  SetObserver(nsnull);
  SetDocLoaderObserver(nsnull);
  SetUrlDispatcher(nsnull);

  // Remove this webshell from its parent's child list
  if (nsnull != mParent) {
    mParent->RemoveChild(this);
  }

  if (nsnull != mDocLoader) {
    mDocLoader->SetContainer(nsnull);
  }

  NS_IF_RELEASE(mContentViewer);

  // Destroy our child web shells and release references to them
  DestroyChildren();
  return rv;
}


NS_IMETHODIMP
nsWebShell::GetBounds(PRInt32 &x, PRInt32 &y, PRInt32 &w, PRInt32 &h)
{
nsRect aResult;

  /* old code path --dwc001
  NS_PRECONDITION(nsnull != mWindow, "null window");
  aResult.SetRect(0, 0, 0, 0);
  if (nsnull != mWindow) {
    mWindow->GetClientBounds(aResult);
  }
  */

  if (nsnull != mWindow) {
    mWindow->GetClientBounds(aResult);
  } else {
    aResult = mBounds;
  }

  x = aResult.x;
  y = aResult.y;
  w = aResult.width;
  h = aResult.height;

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetBounds(PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h)
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
    mWindow->Resize(x, y, w, h, PR_FALSE);
  }

  mBounds.SetRect(x,y,w,h);   // set the webshells bounds --dwc0001

  // Set the size of the content area, which is the size of the window
  // minus the borders
  if (nsnull != mContentViewer) {
    nsRect rr(0, 0, w-(borderWidth*2), h-(borderHeight*2));
    mContentViewer->SetBounds(rr);
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

              GetBounds(oldX, oldY, oldWidth, oldHeight);
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
nsWebShell::MoveTo(PRInt32 aX, PRInt32 aY)
{
  NS_PRECONDITION(nsnull != mWindow, "null window");

  if (nsnull != mWindow) {
    mWindow->Move(aX, aY);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::Show()
{
  /*
  --dwc001
  NS_PRECONDITION(nsnull != mWindow, "null window");
  */

  if (nsnull != mWindow) {
    mWindow->Show(PR_TRUE);
  }
  if (nsnull != mContentViewer) {
    mContentViewer->Show();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::Hide()
{
  /*
  --dwc0001
  NS_PRECONDITION(nsnull != mWindow, "null window");
  */

  if (nsnull != mWindow) {
    mWindow->Show(PR_FALSE);
  }
  if (nsnull != mContentViewer) {
    mContentViewer->Hide();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetFocus()
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
nsWebShell::Repaint(PRBool aForce)
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

NS_IMETHODIMP
nsWebShell::SetContentViewer(nsIContentViewer* aViewer)
{
  NS_IF_RELEASE(mContentViewer);
  mContentViewer = aViewer;
  NS_IF_ADDREF(aViewer);
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetContainer(nsIWebShellContainer* aContainer)
{
  NS_IF_RELEASE(mContainer);
  mContainer = aContainer;
  NS_IF_ADDREF(aContainer);
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
nsWebShell::SetObserver(nsIStreamObserver* anObserver)
{
  NS_IF_RELEASE(mObserver);

  mObserver = anObserver;
  if (nsnull != mObserver) {
    NS_ADDREF(mObserver);
  }
  return NS_OK;
}


NS_IMETHODIMP
nsWebShell::GetObserver(nsIStreamObserver*& aResult)
{
  aResult = mObserver;
  NS_IF_ADDREF(mObserver);
  return NS_OK;
}



NS_IMETHODIMP
nsWebShell::SetDocLoaderObserver(nsIDocumentLoaderObserver* anObserver)
{
//  if (anObserver != nsnull) {
  NS_IF_RELEASE(mDocLoaderObserver);

  mDocLoaderObserver = anObserver;
  NS_IF_ADDREF(mDocLoaderObserver);
//  }
  return NS_OK;
}


NS_IMETHODIMP
nsWebShell::GetDocLoaderObserver(nsIDocumentLoaderObserver*& aResult)
{
  aResult = mDocLoaderObserver;
  NS_IF_ADDREF(mDocLoaderObserver);
  return NS_OK;
}


NS_IMETHODIMP
nsWebShell::SetUrlDispatcher(nsIUrlDispatcher* aDispatcher)
{
  NS_IF_RELEASE(mUrlDispatcher);

  mUrlDispatcher = aDispatcher;
  NS_IF_ADDREF(mUrlDispatcher);
  return NS_OK;
}


NS_IMETHODIMP
nsWebShell::GetUrlDispatcher(nsIUrlDispatcher*& aResult)
{
  aResult = mUrlDispatcher;
  NS_IF_ADDREF(mUrlDispatcher);
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetPrefs(nsIPref* aPrefs)
{
  NS_IF_RELEASE(mPrefs);
  mPrefs = aPrefs;
  NS_IF_ADDREF(mPrefs);
  return NS_OK;
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
  nsIWebShell* top = this;
  NS_ADDREF(this);
  for (;;) {
    nsIWebShell* parent;
    top->GetParent(parent);
    if (nsnull == parent) {
      break;
    }
    NS_RELEASE(top);
    top = parent;
  }
  aResult = top;
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetRootWebShellEvenIfChrome(nsIWebShell*& aResult)
{
  nsIWebShell* top = this;
  NS_ADDREF(this);
  for (;;) {
    nsIWebShell* parent;
    top->GetParentEvenIfChrome(parent);
    if (nsnull == parent) {
      break;
    }
    NS_RELEASE(top);
    top = parent;
  }
  aResult = top;
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetParent(nsIWebShell* aParent)
{
  NS_IF_RELEASE(mParent);
  mParent = aParent;
  NS_IF_ADDREF(aParent);
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetParent(nsIWebShell*& aParent)
{
  if (mWebShellType == nsWebShellContent)
  {
    // We cannot return our parent if it is a chrome webshell.
    nsWebShellType parentType;
    if (mParent)
    {
      mParent->GetWebShellType(parentType);
      if (parentType == nsWebShellChrome)
      {
        aParent = nsnull; // Just return null.
        return NS_OK;
      }
    }
  }

  aParent = mParent;
  NS_IF_ADDREF(mParent);
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetParentEvenIfChrome(nsIWebShell*& aParent)
{
  aParent = mParent;
  NS_IF_ADDREF(mParent);
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetChildCount(PRInt32& aResult)
{
  aResult = mChildren.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::AddChild(nsIWebShell* aChild)
{
  NS_PRECONDITION(nsnull != aChild, "null ptr");
  if (nsnull == aChild) {
    return NS_ERROR_NULL_POINTER;
  }
  mChildren.AppendElement(aChild);
  aChild->SetParent(this);
  const PRUnichar *defaultCharset=nsnull;
  if(NS_SUCCEEDED(this->GetDefaultCharacterSet (&defaultCharset)))
       aChild->SetDefaultCharacterSet(defaultCharset);
  aChild->SetForceCharacterSet(mForceCharacterSet.GetUnicode());
  NS_ADDREF(aChild);

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::RemoveChild(nsIWebShell* aChild)
{
  NS_PRECONDITION(nsnull != aChild, "nsWebShell::RemoveChild(): null ptr");
  if (nsnull == aChild) {
    return NS_ERROR_NULL_POINTER;
  }
  mChildren.RemoveElement(aChild);
  aChild->SetParent(nsnull);
  NS_RELEASE(aChild);

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::ChildAt(PRInt32 aIndex, nsIWebShell*& aResult)
{
  if (PRUint32(aIndex) >= PRUint32(mChildren.Count())) {
    aResult = nsnull;
  }
  else {
    aResult = (nsIWebShell*) mChildren.ElementAt(aIndex);
    NS_IF_ADDREF(aResult);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetName(const PRUnichar** aName)
{
  // XXX This is wrong unless the parameter is marked "shared".
  // It should otherwise be copied and freed by the caller.
  *aName = mName.GetUnicode();
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetName(const PRUnichar* aName)
{
  mName = aName;
  return NS_OK;
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
  aResult = nsnull;
  nsString aName(aName1);

  const PRUnichar *childName;
  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) {
    nsIWebShell* child = (nsIWebShell*) mChildren.ElementAt(i);
    if (nsnull != child) {
      child->GetName(&childName);
      if (aName.Equals(childName)) {
        aResult = child;
        NS_ADDREF(child);
        break;
      }

      // See if child contains the shell with the given name
      nsresult rv = child->FindChildWithName(aName.GetUnicode(), aResult);
      if (NS_FAILED(rv)) {
        return rv;
      }
      if (nsnull != aResult) {
        break;
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetWebShellType(nsWebShellType& aWebShellType)
{
  aWebShellType = mWebShellType;
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetWebShellType(nsWebShellType aWebShellType)
{
  if (aWebShellType != nsWebShellChrome && aWebShellType != nsWebShellContent)
  {
    NS_ERROR("Attempt to set bogus webshell type: values should be content or chrome.");
    return NS_ERROR_FAILURE;
  }
  mWebShellType = aWebShellType;
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetContainingChromeElement(nsIContent** aResult)
{
  NS_IF_ADDREF(mChromeElement);
  *aResult = mChromeElement;
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetContainingChromeElement(nsIContent* aChromeElement)
{
  // Weak reference. Don't addref.
  mChromeElement = aChromeElement;
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetMarginWidth(PRInt32& aWidth)
{
  aWidth = mMarginWidth;
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetMarginWidth(PRInt32 aWidth)
{
  mMarginWidth = aWidth;
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetMarginHeight(PRInt32& aHeight)
{
  aHeight = mMarginHeight;
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetMarginHeight(PRInt32 aHeight)
{
  mMarginHeight = aHeight;
  return NS_OK;
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

NS_IMETHODIMP
nsWebShell::GetIsFrame(PRBool& aIsFrame)
{
  aIsFrame = mIsFrame;
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetIsFrame(PRBool aIsFrame)
{
  mIsFrame = aIsFrame;
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetZoom(float aZoom)
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
              vm->UpdateView(rootview, nsnull, 0);
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
nsWebShell::GetZoom(float *aZoom)
{
  *aZoom = mZoom;
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

// Nisheeth: returns true if the host and the file parts of
// the two nsIURI's match.
static PRBool EqualBaseURLs(nsIURI* url1, nsIURI* url2)
{
  nsXPIDLCString host1;
  nsXPIDLCString host2;
  nsXPIDLCString file1;
  nsXPIDLCString file2;
  char *  anchor1 = nsnull, * anchor2=nsnull;
  PRBool rv = PR_FALSE;

  if (url1 && url2) {
    // XXX We need to make these strcmps case insensitive.
    url1->GetHost(getter_Copies(host1));
    url2->GetHost(getter_Copies(host2));
    if (0 == PL_strcmp(host1, host2)) {
      url1->GetPath(getter_Copies(file1));
      url2->GetPath(getter_Copies(file2));

      anchor1 = PL_strrchr(file1, '#');
      if (anchor1) {
        char * tmp = PL_strstr(file1, file2);
        if (tmp && (const char *)tmp == file1) {
          return PR_TRUE;
        }
      }
      anchor2 = PL_strrchr(file2, '#');
      if (anchor2) {
        char * tmp = PL_strstr(file2, file1);
        if (tmp && (const char *)tmp == file2) {
          return PR_TRUE;
        }
      }

      if (0 == PL_strcmp(file1, file2)) {
        rv = PR_TRUE;
      }
    }  // strcmp(host1, host2)
  }   // url1 && url2
  return rv;
}

nsresult
nsWebShell::DoLoadURL(nsIURI * aUri,
                      const char* aCommand,
                      nsIInputStream* aPostDataStream,
                      nsLoadFlags aType,
                      const PRUint32 aLocalIP,
                      const PRUnichar* aReferrer)
{
  if (!aUri)
    return NS_ERROR_NULL_POINTER;

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
  mURL = urlSpec;

  mReferrer = aReferrer;

  // If it's a normal reload that uses the cache, look at the destination anchor
  // and see if it's an element within the current document
  // We don't have a reload loadtype yet in necko. So, check for just history
  // loadtype
  if ((aType == LOAD_HISTORY || aType == nsIChannel::LOAD_NORMAL) && (nsnull != mContentViewer) &&
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
          if (nsnull != (const char *) ref) {
            // Go to the anchor in the current document
            rv = presShell->GoToAnchor(nsAutoString(ref));

            // Pass notifications to BrowserAppCore just to be consistent with
            // regular page loads thro' necko
            nsCOMPtr<nsIChannel> dummyChannel;
            rv = NS_OpenURI(getter_AddRefs(dummyChannel), aUri, nsnull);
            if (NS_FAILED(rv)) return rv;

            mProcessedEndDocumentLoad = PR_FALSE;
            rv = OnEndDocumentLoad(mDocLoader, dummyChannel, 0, this);

            return rv;
          }
          else if (aType == LOAD_HISTORY)
          {
            // Go to the top of the current document
            nsCOMPtr<nsIViewManager> viewMgr;
            rv = presShell->GetViewManager(getter_AddRefs(viewMgr));
            if (NS_SUCCEEDED(rv) && viewMgr) {


              nsIScrollableView* view;
              rv = viewMgr->GetRootScrollableView(&view);
              if (NS_SUCCEEDED(rv) && view)
                rv = view->ScrollTo(0, 0, NS_VMREFRESH_IMMEDIATE);

              // Pass notifications to BrowserAppCore just to be consistent with
              // regular necko loads.
              nsCOMPtr<nsIChannel> dummyChannel;
              rv = NS_OpenURI(getter_AddRefs(dummyChannel), aUri, nsnull);
              if (NS_FAILED(rv)) return rv;
              mProcessedEndDocumentLoad = PR_FALSE;

              rv = OnEndDocumentLoad(mDocLoader, dummyChannel, 0, this);



            }
            return rv;
          }
        }
      } // EqualBaseURLs(docURL, url)
    }
  }

  // Stop loading the current document (if any...).  This call may result in
  // firing an EndLoadURL notification for the old document...
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

 /* WebShell was primarily passing the buck when it came to streamObserver.
  * So, pass on the observer which is already a streamObserver to DocLoder.
  *  - Radha
  */

  return mDocLoader->LoadDocument(aUri,        // URL string
                                  aCommand,        // Command
                                  this,            // Container
                                  aPostDataStream, // Post Data
                                  nsnull,          // Extra Info...
                                  mObserver,       // Observer
                                  aType,           // reload type
                                  aLocalIP,        // load attributes.
                                  aReferrer);      // referrer
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
  nsresult rv;
  CancelRefreshURITimers();
  nsXPIDLCString scheme, CUriSpec;

  rv = aUri->GetScheme(getter_Copies(scheme));
  if (NS_FAILED(rv)) return rv;
  rv = aUri->GetSpec(getter_Copies(CUriSpec));
  if (NS_FAILED(rv)) return rv;

  nsAutoString uriSpec(CUriSpec);

  nsXPIDLCString spec;
  rv = aUri->GetSpec(getter_Copies(spec));
  if (NS_FAILED(rv)) return rv;

  /*
   * Before the new page is added to the session history,
   * save the history information of the previous page in
   * session history
   */

   nsISupports * historyState=nsnull;

   // Get the history object for the previous page.
   rv = GetHistoryState(&historyState);
   nsCOMPtr<nsIWebShell> rootWebShell;
   rv = GetRootWebShell(*getter_AddRefs(rootWebShell));
   if (NS_SUCCEEDED(rv) && rootWebShell) {
      nsCOMPtr<nsISessionHistory> shist;
      rv = rootWebShell->GetSessionHistory(*getter_AddRefs(shist));
      if (NS_SUCCEEDED(rv) && shist) {
        PRInt32 indix=0;
        shist->getCurrentIndex(indix);
        // Save it in session history
        shist->SetHistoryObjectForIndex(indix, historyState);
      }
   }



  /* If this is one of the frames, get it from the top level shell */

  if (aModifyHistory) {
    if (rootWebShell) {
      nsCOMPtr<nsISessionHistory> shist;
      rootWebShell->GetSessionHistory(*getter_AddRefs(shist));
      /* Add yourself to the Session History */
      if (shist) {
        PRInt32  ret=0;
        ret = shist->add(this);
      }
    }
  }

  /* Set the History state object for the current page in the
   * presentation shell. If it is a new page being visited,
   * aHistoryState is null. If the load is coming from
   * session History, it will be set to the cached history object by
   * session History.
   */
  SetHistoryState(aHistoryState);

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


  /* The session History may have changed the URL. So pass on the
   * right one for loading
   */
  // Give web-shell-container right of refusal
  if (nsnull != mContainer) {
    nsAutoString str(spec);
    rv = mContainer->WillLoadURL(this, str.GetUnicode(), nsLoadURL);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }


  return DoLoadURL(aUri, aCommand, aPostDataStream, aType, aLocalIP, aReferrer);
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

  nsAutoString urlStr(aURLSpec);
  // first things first. try to create a uri out of the string.
  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), urlStr, nsnull);
  if (NS_FAILED(rv)) {
    // no dice.
    nsAutoString urlSpec;
    urlStr.Trim(" ", PR_TRUE, PR_TRUE);

    // see if we've got a file url.
    convertFileToURL(urlStr, urlSpec);
    rv = NS_NewURI(getter_AddRefs(uri), urlSpec, nsnull);
    if (NS_FAILED(rv)) {
      // no dice, try more tricks

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

    //Take care of mailto: url
  PRBool  isMail= PR_FALSE;

  nsAutoString urlAStr(aURLSpec);
  if ((urlAStr.Find("mailto", PR_TRUE)) >= 0) {
     isMail = PR_TRUE;
  }

  nsCOMPtr<nsIWebShell>  root;
  rv = GetRootWebShell(*getter_AddRefs(root));

  /* Ask the URL dispatcher to take care of this URL only if it is a
   * mailto: link clicked inside a browser or any link clicked
   * inside a *non-browser* window. Note this mechanism s'd go away once
   * we have the protocol registry and window manager available
   */
  if (NS_SUCCEEDED(rv) && root && isMail) {

      //Ask the url Dispatcher to load the appropriate component for the URL.
      nsCOMPtr<nsIUrlDispatcher>  urlDispatcher;
      rv = root->GetUrlDispatcher(*getter_AddRefs(urlDispatcher));
      if (NS_SUCCEEDED(rv) && urlDispatcher) {
        printf("calling HandleUrl\n");
        urlDispatcher->HandleUrl(LinkCommand.GetUnicode(),
                                 urlAStr.GetUnicode(), aPostDataStream);
        return NS_OK;
      }
  }



  // now that we have a uri, call the REAL LoadURI method which requires a nsIURI.
  return LoadURI(uri, aCommand, aPostDataStream, aModifyHistory, aType, aLocalIP, aHistoryState, aReferrer);
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
    nsIWebShell* shell = (nsIWebShell*) mChildren.ElementAt(i);
    shell->Stop();
  }

  return NS_OK;
}

// This "stops" the current document load enough so that the document loader
// can be used to load a new URL.
nsresult
nsWebShell::StopBeforeRequestingURL()
{
  if (mDocLoader) {
    // Stop any documents that are currently being loaded...
    mDocLoader->Stop();
  }

  // Recurse down the webshell hierarchy.
  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) {
    nsWebShell* shell = (nsWebShell*) mChildren.ElementAt(i);
    shell->StopBeforeRequestingURL();
  }

  return NS_OK;
}

// This "stops" the current document load completely and is called once
// it has been determined that the new URL is valid and ready to be thrown
// at us from netlib.
nsresult
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
    nsWebShell* shell = (nsWebShell*) mChildren.ElementAt(i);
    shell->StopAfterURLAvailable();
  }

  return NS_OK;
}

/* The generic session History code here is now obsolete.
 * Use nsISessionHistory instead
 */
NS_IMETHODIMP nsWebShell::Reload(nsLoadFlags aType)
{
#ifdef OLD_HISTORY
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
#else
  if (mSHist)
     return mSHist->Reload(this, aType);
  return NS_OK;
#endif
}

//----------------------------------------

// History methods

NS_IMETHODIMP
nsWebShell::Back(void)
{
#ifdef OLD_HISTORY
  return GoTo(mHistoryIndex - 1);
#else
  if (mSHist)
    return mSHist->GoBack(this);
  return NS_OK;
#endif
}

NS_IMETHODIMP
nsWebShell::CanBack(void)
{
#ifdef OLD_HISTORY
  return (mHistoryIndex  > mHistory.Count() - 1 ? NS_OK : NS_COMFALSE);
#else
  if (mSHist) {
    PRBool result=PR_TRUE;
    mSHist->canBack(result);
    return  (result ? NS_OK : NS_COMFALSE);
  }
  return NS_OK;
#endif

}

NS_IMETHODIMP
nsWebShell::Forward(void)
{
#ifdef OLD_HISTORY
  return GoTo(mHistoryIndex + 1);
#else
  if (mSHist)
    return mSHist->GoForward(this);
  return NS_OK;
#endif
}

NS_IMETHODIMP
nsWebShell::CanForward(void)
{
#ifdef OLD_HISTORY
  return (mHistoryIndex  < mHistory.Count() - 1 ? NS_OK : NS_COMFALSE);
#else
  if (mSHist) {
    PRBool result=PR_TRUE;
    mSHist->canForward(result);
    return  (result ? NS_OK : NS_COMFALSE);
  }
  return NS_OK;
#endif
}

NS_IMETHODIMP
nsWebShell::GoTo(PRInt32 aHistoryIndex)
{
#ifdef OLD_HISTORY
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
                   nsIChannel::LOAD_NORMAL,   // the reload type
                   0,            // load attributes
                   nsnull);      // referrer
  }
  return rv;
#else
   if (mSHist)
     return mSHist->Goto(aHistoryIndex, this, PR_FALSE);
   return NS_OK;


#endif

}

NS_IMETHODIMP
nsWebShell::GetHistoryLength(PRInt32& aResult)
{
#ifdef OLD_HISTORY
  aResult = mHistory.Count();
#else
  if (mSHist)
    return mSHist->getHistoryLength(aResult);
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetHistoryIndex(PRInt32& aResult)
{
#ifdef OLD_HISTORY
  aResult = mHistoryIndex;
#else
  if (mSHist)
    return mSHist->getCurrentIndex(aResult);
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetURL(PRInt32 aHistoryIndex, const PRUnichar** aURLResult)
{
  nsresult rv = NS_ERROR_ILLEGAL_VALUE;
#ifdef OLD_HISTORY
  if ((aHistoryIndex >= 0) &&
      (aHistoryIndex <= mHistory.Count() - 1)) {
    nsString* s = (nsString*) mHistory.ElementAt(aHistoryIndex);
    if (nsnull != s) {
      *aURLResult = s->GetUnicode();
    }
    rv = NS_OK;
  }
#else
  if (mSHist)
     return mSHist->GetURLForIndex(aHistoryIndex, aURLResult);

#endif
  return rv;
}

void
nsWebShell::ShowHistory()
{
#if defined(OLD_HISTORY) && defined(NS_DEBUG)
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

// Chrome API's

NS_IMETHODIMP
nsWebShell::SetTitle(const PRUnichar* aTitle)
{
  // Record local title
  mTitle = aTitle;

  // Title's set on the top level web-shell are passed ont to the container
  nsIWebShell* parent;
  GetParent(parent);
  if (nsnull == parent) {
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
  } else {
    parent->SetTitle(aTitle);
    NS_RELEASE(parent);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetTitle(const PRUnichar** aResult)
{
  *aResult = mTitle.GetUnicode();
  return NS_OK;
}

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
  if (nsnull != mContainer) {
    return mContainer->FindWebShellWithName(aName, aResult);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::FocusAvailable(nsIWebShell* aFocusedWebShell, PRBool& aFocusTaken)
{
  //If the WebShell with focus is us, pass this up to container
  if (this == aFocusedWebShell && nsnull != mContainer) {
    mContainer->FocusAvailable(this, aFocusTaken);
  }

  nsIWebShell* shell = nsnull;

  //Other wise, check children and move focus to next one
  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) {
    shell = (nsIWebShell*)mChildren.ElementAt(i);
    if (shell == aFocusedWebShell) {
      if (++i < n) {
        shell = (nsIWebShell*)mChildren.ElementAt(i);
        shell->SetFocus();
        break;
      }
      else if (nsnull != mContainer) {
        mContainer->FocusAvailable(this, aFocusTaken);
        break;
      }
    }
  }

  return NS_OK;
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
  mHintCharset = aCharset;
  mHintCharsetSource = aSource;

  nsAutoString url(aURL);
  LoadURL(url.GetUnicode());
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::ReloadDocument(const char* aCharset,
                           nsCharsetSource aSource,
                           const char* aCmd)
{

  // XXX hack. kee the aCharset and aSource wait to pick it up
  mHintCharset = aCharset;
  mHintCharsetSource= aSource;
  mViewSource = (0==PL_strcmp("view-source", aCmd));
  return Reload(nsIChannel::LOAD_NORMAL);
}


NS_IMETHODIMP
nsWebShell::StopDocumentLoad(void)
{
  Stop();
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetRendering(PRBool aRender)
{
  if (mContentViewer) {
    mContentViewer->SetEnableRendering(aRender);
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
    if (NS_OK == NewWebShell(PRUint32(~0), PR_TRUE, shell))
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
          (void)shell->LoadURL(aURLSpec, aPostDataStream,
                               PR_TRUE, nsIChannel::LOAD_NORMAL,
                               0, nsnull, str);
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
  nsIBrowserWindow *browserWindow = nsnull;
  nsIWebShell *rootWebShell;

  GetRootWebShellEvenIfChrome(rootWebShell);

  if (nsnull != rootWebShell) {
    nsIWebShellContainer *rootContainer;
    rootWebShell->GetContainer(rootContainer);
    if (nsnull != rootContainer) {
      rootContainer->QueryInterface(kIBrowserWindowIID, (void**)&browserWindow);
      NS_RELEASE(rootContainer);
    }
    NS_RELEASE(rootWebShell);
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
  }

  if (nsnull == mScriptContext) {
    res = NS_CreateScriptContext(mScriptGlobal, &mScriptContext);
    if (NS_SUCCEEDED(res)) {
      mScriptContext->SetOwner(this);
    }
  }

  return res;
}

nsresult
nsWebShell::GetScriptContext(nsIScriptContext** aContext)
{
  NS_PRECONDITION(nsnull != aContext, "null arg");
  nsresult res = NS_OK;

  res = CreateScriptEnvironment();

  if (NS_SUCCEEDED(res)) {
    *aContext = mScriptContext;
    NS_ADDREF(mScriptContext);
  }

  return res;
}

nsresult
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

nsresult
nsWebShell::ReleaseScriptContext(nsIScriptContext *aContext)
{
  // XXX Is this right? Why are we passing in a context?
  NS_IF_RELEASE(aContext);
  return NS_OK;
}

NS_IMETHODIMP 
nsWebShell::ReportScriptError(const char* aErrorString,
                              const char* aFileName,
                              PRInt32     aLineNo,
                              const char* aLineBuf)
{
  // XXX To be implemented by scc. The following implementation
  // is temporary.

  nsAutoString error;
  error.SetString("JavaScript Error: ");
  error.Append(aErrorString);
  error += "\n";

  if (aFileName) {
    error += "URL: ";
    error += aFileName;
    error += "\n";
  }

  if(aLineNo) {
    error += "LineNo: ";
    error.Append(aLineNo, 10);
    error += "\n";
  }

  if(aLineBuf) {
    error += "Line text: '";
    error += aLineBuf;
    error += "'\n";
  }

  // XXX Ughhh...allocation
  char* errorStr = error.ToNewCString();
  if (errorStr) {
    printf("%s\n", errorStr);
    Recycle(errorStr);
  }
  
  // XXX Turn it off for now...there should be an Error method too
  //Alert(error.GetUnicode());

  return NS_OK;
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
        rv = mScriptGlobal->HandleDOMEvent(*presContext, &event, nsnull, NS_EVENT_FLAG_INIT, status);

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
        root->GetDocLoaderObserver(*getter_AddRefs(dlObserver));
    }
    else
    {
      dlObserver = do_QueryInterface(mDocLoaderObserver);  // we need this to addref
    }
    /*
     *Fire the OnStartDocumentLoad of the webshell observer
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
  nsresult rv = NS_ERROR_FAILURE;
  if (!channel) {
    return NS_ERROR_NULL_POINTER;
  }

  nsCOMPtr<nsIURI> aURL;
  rv = channel->GetURI(getter_AddRefs(aURL));
  if (NS_FAILED(rv)) return rv;

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
          rv = mScriptGlobal->HandleDOMEvent(*presContext, &event, nsnull, NS_EVENT_FLAG_INIT, status);

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
        root->GetDocLoaderObserver(*getter_AddRefs(dlObserver));
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
        // We need to check for a dns failure in aStatus, but dns failure codes
        // aren't proliferated yet. This checks for failure for a host lacking
        // "www." and/or ".com" and munges the url acordingly, then fires off
        // a new request.
        //
        // XXX This code may or may not have mem leaks depending on the version
        // XXX stdurl that is in the tree at a given point in time. This needs to
        // XXX be fixed once we have a solid version of std url in.
        char *host = nsnull;
        nsString2 hostStr;
        rv = aURL->GetHost(&host);
        if (NS_FAILED(rv)) return rv;

        hostStr.SetString(host);
        nsAllocator::Free(host);
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
            char *modHost = hostStr.ToNewCString();
            if (!modHost)
                return NS_ERROR_OUT_OF_MEMORY;
            rv = aURL->SetHost(modHost);
            nsAllocator::Free(modHost);
            modHost = nsnull;
            if (NS_FAILED(rv)) return rv;
            char *aSpec = nsnull;
            rv = aURL->GetSpec(&aSpec);
            if (NS_FAILED(rv)) return rv;
            nsString2 newURL(aSpec);
            // reload the url
            const PRUnichar *spec = newURL.GetUnicode();
            if (spec) {
                rv = LoadURL(spec, "view");
            }
        } // retry
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

  // XXX This is a temporary hack for meeting the M4 milestone
  // for seamonkey.  I think Netlib should send a message to all stream listeners
  // when it changes the URL like this.  That would mean adding a new method
  // to nsIStreamListener.  Need to talk to Rick, Kipp, Gagan about this.

  /* Overriding comments: History mechanism has changed. So is Necko changing.
   * Need to check in the new world if this is still valid. If so, new methods
   * need to be added to nsISessionHistory. Until then. We don't need this.
   * This is being done so that old History code can be removed.
   */
//  CheckForTrailingSlash(aURL);

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
   *Fire the OnStartDocumentLoad of the webshell observer
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

  const PRUnichar * url=nsnull, * title=nsnull;
  PRInt32     curIndex=0;
  nsresult rv;
  nsString * historyURL=nsnull, * newURL=nsnull;

  /* Get current history index and url for it */
  rv = mSHist->getCurrentIndex(curIndex);
  if (NS_SUCCEEDED(rv) && curIndex >= 0) {
    mSHist->GetURLForIndex(curIndex, &url);
    historyURL = (nsString *)  new nsString(url);
  }

  /* Get the url that netlib passed us */
  char* spec;
  aURL->GetSpec(&spec);
  newURL = (nsString*) new nsString(spec);
  nsCRT::free(spec);

  if (newURL && historyURL && newURL->Last() == '/' && !historyURL->Equals(*newURL)) {
  }

  rv = GetTitle(&title);

  if (NS_SUCCEEDED(rv) && title) {
    nsString titleStr(title);
    mSHist->SetTitleForIndex(curIndex, title);
    // Replace the top most history entry with the new url
    mSHist->SetURLForIndex(curIndex, newURL->GetUnicode());
  }

  if (newURL) delete newURL;
  if (historyURL) delete historyURL;
  
  return NS_OK;
}

#if 0
NS_IMETHODIMP
nsWebShell::OnStartRequest(nsIURI* aURL, const char *aContentType)
{
  nsresult rv = NS_OK;

  if (nsnull != mObserver) {
    rv = mObserver->OnStartRequest(aURL, aContentType);
  }
  return rv;
}


NS_IMETHODIMP
nsWebShell::OnProgress(nsIURI* aURL, PRUint32 aProgress, PRUint32 aProgressMax)
{
  nsresult rv = NS_OK;

  if (nsnull != mObserver) {
    rv = mObserver->OnProgress(aURL, aProgress, aProgressMax);
  }

  // Pass status messages out to the nsIBrowserWindow...
  nsIBrowserWindow *browserWindow;

  browserWindow = GetBrowserWindow();
  if (nsnull != browserWindow) {
    browserWindow->SetProgress(aProgress, aProgressMax);
    NS_RELEASE(browserWindow);
  }

  return rv;
}


NS_IMETHODIMP
nsWebShell::OnStatus(nsIURI* aURL, const PRUnichar* aMsg)
{
  nsresult rv = NS_OK;

  if (nsnull != mObserver) {
    rv = mObserver->OnStatus(aURL, aMsg);
  }

  // Pass status messages out to the nsIBrowserWindow...
  nsIBrowserWindow *browserWindow;

  browserWindow = GetBrowserWindow();
  if (nsnull != browserWindow) {
    browserWindow->SetStatus(aMsg);
    NS_RELEASE(browserWindow);
  }

  return rv;
}


NS_IMETHODIMP
nsWebShell::OnStopRequest(nsIURI* aURL, nsresult aStatus, const PRUnichar* aMsg)
{
  nsresult rv = NS_OK;

  if (nsnull != mObserver) {
    rv = mObserver->OnStopRequest(aURL, aStatus, aMsg);
  }
  return rv;
}
#endif  /* 0 */


//----------------------------------------------------------------------

NS_IMETHODIMP
nsWebShell::Alert(const PRUnichar *text)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIPrompt> prompter;

  if (mContainer) {
    prompter = do_QueryInterface(mContainer);
    if (prompter) {
      rv = prompter->Alert(text);
    }
  }
  
  return rv;
}

NS_IMETHODIMP
nsWebShell::Confirm(const PRUnichar *text,
                    PRBool *result)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIPrompt> prompter;

  if (mContainer) {
    prompter = do_QueryInterface(mContainer);
    if (prompter) {
      rv = prompter->Confirm(text, result);
    }
  }
  
  return rv;
}

NS_IMETHODIMP
nsWebShell::ConfirmYN(const PRUnichar *text,
                    PRBool *result)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIPrompt> prompter;

  if (mContainer) {
    prompter = do_QueryInterface(mContainer);
    if (prompter) {
      rv = prompter->ConfirmYN(text, result);
    }
  }
  
  return rv;
}

NS_IMETHODIMP
nsWebShell::ConfirmCheck(const PRUnichar *text,
                         const PRUnichar *checkMsg,
                         PRBool *checkValue,
                         PRBool *result)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIPrompt> prompter;

  if (mContainer) {
    prompter = do_QueryInterface(mContainer);
    if (prompter) {
      rv = prompter->ConfirmCheck(text, checkMsg, checkValue, result);
    }
  }
  
  return rv;
}

NS_IMETHODIMP
nsWebShell::ConfirmCheckYN(const PRUnichar *text,
                         const PRUnichar *checkMsg,
                         PRBool *checkValue,
                         PRBool *result)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIPrompt> prompter;

  if (mContainer) {
    prompter = do_QueryInterface(mContainer);
    if (prompter) {
      rv = prompter->ConfirmCheckYN(text, checkMsg, checkValue, result);
    }
  }
  
  return rv;
}

NS_IMETHODIMP
nsWebShell::Prompt(const PRUnichar *text,
                   const PRUnichar *defaultText,
                   PRUnichar **result,
                   PRBool *_retval)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIPrompt> prompter;

  if (mContainer) {
    prompter = do_QueryInterface(mContainer);
    if (prompter) {
      rv = prompter->Prompt(text, defaultText, result, _retval);
    }
  }
  
  return rv;
}

NS_IMETHODIMP
nsWebShell::PromptUsernameAndPassword(const PRUnichar *text,
                                      PRUnichar **user,
                                      PRUnichar **pwd,
                                      PRBool *_retval)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIPrompt> prompter;

  if (mContainer) {
    prompter = do_QueryInterface(mContainer);
    if (prompter) {
      rv = prompter->PromptUsernameAndPassword(text, user, pwd, _retval);
    }
  }
  
  return rv;
}

NS_IMETHODIMP
nsWebShell::PromptPassword(const PRUnichar *text,
                           PRUnichar **pwd,
                           PRBool *_retval)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsIPrompt> prompter;

  if (mContainer) {
    prompter = do_QueryInterface(mContainer);
    if (prompter) {
      rv = prompter->PromptPassword(text, pwd, _retval);
    }
  }
  
  return rv;
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

static char *gDefCharset = nsnull;

NS_IMETHODIMP
nsWebShell::GetDefaultCharacterSet (const PRUnichar** aDefaultCharacterSet)
{
  if (0 == mDefaultCharacterSet.Length()) {
      if ((nsnull == gDefCharset) || (nsnull == *gDefCharset)) {
         if(mPrefs)
            mPrefs->CopyCharPref("intl.charset.default", &gDefCharset);
      }
      if ((nsnull == gDefCharset) || (nsnull == *gDefCharset))
        mDefaultCharacterSet = "ISO-8859-1";
      else
        mDefaultCharacterSet = gDefCharset;
  }
  *aDefaultCharacterSet = mDefaultCharacterSet.GetUnicode();
  return NS_OK;
}
NS_IMETHODIMP
nsWebShell::SetDefaultCharacterSet (const PRUnichar*  aDefaultCharacterSet)
{
  mDefaultCharacterSet = aDefaultCharacterSet;
  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) {
    nsIWebShell* child = (nsIWebShell*) mChildren.ElementAt(i);
    if (nsnull != child) {
      child->SetDefaultCharacterSet(aDefaultCharacterSet);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetForceCharacterSet (const PRUnichar** aForceCharacterSet)
{
  nsAutoString emptyStr;
  if (mForceCharacterSet.Equals(emptyStr)) {
    *aForceCharacterSet = nsnull;
  }
  else {
    *aForceCharacterSet = mForceCharacterSet.GetUnicode();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetForceCharacterSet (const PRUnichar*  aForceCharacterSet)
{
  mForceCharacterSet = aForceCharacterSet;
  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) {
    nsIWebShell* child = (nsIWebShell*) mChildren.ElementAt(i);
    if (nsnull != child) {
      child->SetForceCharacterSet(aForceCharacterSet);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsWebShell::GetCharacterSetHint (const PRUnichar** oHintCharset, nsCharsetSource* oSource)
{
  *oSource = mHintCharsetSource;
  if(kCharsetUninitialized == mHintCharsetSource) {
    *oHintCharset = nsnull;
  } else {
    *oHintCharset = mHintCharset.GetUnicode();
    // clean up after we access it.
    mHintCharsetSource = kCharsetUninitialized;
  }
  return NS_OK;
}


//----------------------------------------------------
NS_IMETHODIMP
nsWebShell::FindNext(const PRUnichar * aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound)
{
  return NS_ERROR_FAILURE;
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

//----------------------------------------------------------------------

// Factory code for creating nsWebShell's

class nsWebShellFactory : public nsIFactory
{
public:
  nsWebShellFactory();
  virtual ~nsWebShellFactory();

  // nsISupports methods
  NS_IMETHOD QueryInterface(const nsIID &aIID, void **aResult);
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  // nsIFactory methods
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock);

private:
  // XXX TEMPORARY placeholder for starting up some
  // services in lieu of a service manager.
  static void StartServices();

  static PRBool mStartedServices;
  nsrefcnt  mRefCnt;
};

PRBool nsWebShellFactory::mStartedServices = PR_FALSE;

void
nsWebShellFactory::StartServices()
{
  mStartedServices = PR_TRUE;
}

nsWebShellFactory::nsWebShellFactory()
{
  if (!mStartedServices) {
    StartServices();
  }
  mRefCnt = 0;
}

nsWebShellFactory::~nsWebShellFactory()
{
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

nsresult
nsWebShellFactory::QueryInterface(const nsIID &aIID, void **aResult)
{
  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aResult = NULL;

  if (aIID.Equals(kISupportsIID)) {
    *aResult = (void *)(nsISupports*)this;
  } else if (aIID.Equals(kIFactoryIID)) {
    *aResult = (void *)(nsIFactory*)this;
  }

  if (*aResult == NULL) {
    return NS_NOINTERFACE;
  }

  NS_ADDREF_THIS();  // Increase reference count for caller
  return NS_OK;
}

nsrefcnt
nsWebShellFactory::AddRef()
{
  return ++mRefCnt;
}

nsrefcnt
nsWebShellFactory::Release()
{
  if (--mRefCnt == 0) {
    delete this;
    return 0; // Don't access mRefCnt after deleting!
  }
  return mRefCnt;
}

nsresult
nsWebShellFactory::CreateInstance(nsISupports *aOuter,
                                  const nsIID &aIID,
                                  void **aResult)
{
  nsresult rv;
  nsWebShell *inst;

  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = NULL;
  if (nsnull != aOuter) {
    rv = NS_ERROR_NO_AGGREGATION;
    goto done;
  }

  NS_NEWXPCOM(inst, nsWebShell);
  if (inst == NULL) {
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto done;
  }

  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

done:
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
