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
#include "nsINetSupport.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContextOwner.h"
#include "nsIDocumentLoaderObserver.h"
#include "nsDOMEvent.h"
#include "nsIPresContext.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsXPComCIID.h"
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
#include "nsIRefreshUrl.h"
#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "jsurl.h"
#include "nsIBrowserWindow.h"
#include "nsIContent.h"
#include "prlog.h"
#include "nsCOMPtr.h"
#include "nsIPresShell.h"
#include "nsIStreamObserver.h"

#ifdef XP_PC
#include <windows.h>
#endif

//XXX used for nsIStreamObserver implementation.  This sould be replaced by DocLoader
//    notifications...
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
#if XP_MAC
// This has all the same problems as the above
extern "C" PLEventQueue* GetMacPLEventQueue();
#endif

//----------------------------------------------------------------------

class nsWebShell : public nsIWebShell,
                   public nsIWebShellContainer,
                   public nsILinkHandler,
                   public nsIScriptContextOwner,
                   public nsIDocumentLoaderObserver,
                   public nsIRefreshUrl,
                   public nsINetSupport,
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
  NS_IMETHOD HandleUnknownContentType( nsIURL* aURL,
                                       const char *aContentType,
                                       const char *aCommand );

  // nsIWebShell
  NS_IMETHOD Init(nsNativeWidget aNativeParent,
                  PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h,
                  nsScrollPreference aScrolling = nsScrollPreference_kAuto,
                  PRBool aAllowPlugins = PR_TRUE,
                  PRBool aIsSunkenBorder = PR_FALSE);
  NS_IMETHOD Destroy(void);
  NS_IMETHOD GetBounds(PRInt32 &x, PRInt32 &y, PRInt32 &w, PRInt32 &h);
  NS_IMETHOD SetBounds(PRInt32 x, PRInt32 y, PRInt32 w, PRInt32 h);
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
  NS_IMETHOD SetParent(nsIWebShell* aParent);
  NS_IMETHOD GetParent(nsIWebShell*& aParent);
  NS_IMETHOD GetChildCount(PRInt32& aResult);
  NS_IMETHOD AddChild(nsIWebShell* aChild);
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIWebShell*& aResult);
  NS_IMETHOD GetName(const PRUnichar** aName);
  NS_IMETHOD SetName(const PRUnichar* aName);
  NS_IMETHOD FindChildWithName(const PRUnichar* aName,
                               nsIWebShell*& aResult);
  NS_IMETHOD GetMarginWidth (PRInt32& aWidth);
  NS_IMETHOD SetMarginWidth (PRInt32  aWidth);
  NS_IMETHOD GetMarginHeight(PRInt32& aWidth);
  NS_IMETHOD SetMarginHeight(PRInt32  aHeight);
  NS_IMETHOD GetScrolling(PRInt32& aScrolling);
  NS_IMETHOD SetScrolling(PRInt32 aScrolling);
  NS_IMETHOD GetIsFrame(PRBool& aIsFrame);
  NS_IMETHOD SetIsFrame(PRBool aIsFrame);
  
  // Document load api's
  NS_IMETHOD GetDocumentLoader(nsIDocumentLoader*& aResult);
  NS_IMETHOD LoadURL(const PRUnichar *aURLSpec,
                     nsIPostData* aPostData=nsnull,
                     PRBool aModifyHistory=PR_TRUE,
                     nsURLReloadType aType = nsURLReload,
                     const PRUint32 localIP = 0);
  NS_IMETHOD LoadURL(const PRUnichar *aURLSpec,
                     const char* aCommand,
                     nsIPostData* aPostData=nsnull,
                     PRBool aModifyHistory=PR_TRUE,
                     nsURLReloadType aType = nsURLReload,
                     const PRUint32 localIP = 0);

  NS_IMETHOD Stop(void);
  NS_IMETHOD Reload(nsURLReloadType aType);
   
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
  NS_IMETHOD EndLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aStatus);
  NS_IMETHOD NewWebShell(PRUint32 aChromeMask,
                         PRBool aVisible,
                         nsIWebShell *&aNewWebShell);
  NS_IMETHOD FindWebShellWithName(const PRUnichar* aName, nsIWebShell*& aResult);
  NS_IMETHOD FocusAvailable(nsIWebShell* aFocusedWebShell);

  // nsILinkHandler
  NS_IMETHOD OnLinkClick(nsIContent* aContent, 
                         nsLinkVerb aVerb,
                         const PRUnichar* aURLSpec,
                         const PRUnichar* aTargetSpec,
                         nsIPostData* aPostData = 0);
  NS_IMETHOD OnOverLink(nsIContent* aContent,
                        const PRUnichar* aURLSpec,
                        const PRUnichar* aTargetSpec);
  NS_IMETHOD GetLinkState(const PRUnichar* aURLSpec, nsLinkState& aState);

  // nsIScriptContextOwner
  NS_IMETHOD GetScriptContext(nsIScriptContext **aContext);
  NS_IMETHOD GetScriptGlobalObject(nsIScriptGlobalObject **aGlobal);
  NS_IMETHOD ReleaseScriptContext(nsIScriptContext *aContext);

  // nsIDocumentLoaderObserver
  NS_IMETHOD OnStartDocumentLoad(nsIURL* aURL, const char* aCommand);
  NS_IMETHOD OnEndDocumentLoad(nsIURL* aURL, PRInt32 aStatus);
  NS_IMETHOD OnStartURLLoad(nsIURL* aURL, const char* aContentType, 
                            nsIContentViewer* aViewer);
  NS_IMETHOD OnProgressURLLoad(nsIURL* aURL, PRUint32 aProgress, 
                               PRUint32 aProgressMax);
  NS_IMETHOD OnStatusURLLoad(nsIURL* aURL, nsString& aMsg);
  NS_IMETHOD OnEndURLLoad(nsIURL* aURL, PRInt32 aStatus);
//  NS_IMETHOD OnConnectionsComplete();

  // nsIRefreshURL interface methods...
  NS_IMETHOD RefreshURL(nsIURL* aURL, PRInt32 millis, PRBool repeat);
  NS_IMETHOD CancelRefreshURLTimers(void);


#if 0
  // nsIStreamObserver
  NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);
  NS_IMETHOD OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax);
  NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMsg);
  NS_IMETHOD OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg);
#endif  /* 0 */


	// nsINetSupport interface methods
  NS_IMETHOD_(void) Alert(const nsString &aText);
  NS_IMETHOD_(PRBool) Confirm(const nsString &aText);
  NS_IMETHOD_(PRBool) Prompt(const nsString &aText,
                             const nsString &aDefault,
                             nsString &aResult);
  NS_IMETHOD_(PRBool) PromptUserAndPassword(const nsString &aText,
                                            nsString &aUser,
                                            nsString &aPassword);
  NS_IMETHOD_(PRBool) PromptPassword(const nsString &aText,
                                     nsString &aPassword);

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
  PLEventQueue* GetEventQueue(void);
  void HandleLinkClickEvent(nsIContent *aContent,
                            nsLinkVerb aVerb,
                            const PRUnichar* aURLSpec,
                            const PRUnichar* aTargetSpec,
                            nsIPostData* aPostDat = 0);

  void ShowHistory();

  nsIWebShell* GetTarget(const PRUnichar* aName);
  nsIBrowserWindow* GetBrowserWindow(void);

  static void RefreshURLCallback(nsITimer* aTimer, void* aClosure);

  static nsEventStatus PR_CALLBACK HandleEvent(nsGUIEvent *aEvent);

  nsresult CreatePluginHost(PRBool aAllowPlugins);
  nsresult DestroyPluginHost(void);

  NS_IMETHOD GetDefaultCharacterSet (const PRUnichar** aDefaultCharacterSet);
  NS_IMETHOD SetDefaultCharacterSet (const PRUnichar*  aDefaultCharacterSet);

protected:
  void InitFrameData();
  nsresult CheckForTrailingSlash(nsIURL* aURL);

  PLEventQueue* mThreadEventQueue;
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
  nsINetSupport* mNetSupport;

  nsIWebShell* mParent;
  nsVoidArray mChildren;
  nsString mName;
  nsString mDefaultCharacterSet;

  nsVoidArray mHistory;
  PRInt32 mHistoryIndex;

  nsString mTitle;

  nsString mOverURL;
  nsString mOverTarget;

  nsScrollPreference mScrollPref;
  PRInt32 mMarginWidth;
  PRInt32 mMarginHeight;
  PRInt32 mScrolling;
  PRBool  mIsFrame;
  nsVoidArray mRefreshments;

  void ReleaseChildren();
  void DestroyChildren();
  nsresult CreateScriptEnvironment();
  nsresult DoLoadURL(const nsString& aUrlSpec,
                     const char* aCommand,
                     nsIPostData* aPostData,
                     nsURLReloadType aType,
                     const PRUint32 aLocalIP);

  static nsIPluginHost    *mPluginHost;
  static nsIPluginManager *mPluginManager;
  static PRUint32          mPluginInitCnt;
};

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
static NS_DEFINE_IID(kINetSupportIID,         NS_INETSUPPORT_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWebShellIID,           NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIWidgetIID,             NS_IWIDGET_IID);
static NS_DEFINE_IID(kIPluginManagerIID,      NS_IPLUGINMANAGER_IID);
static NS_DEFINE_IID(kIPluginHostIID,         NS_IPLUGINHOST_IID);
//static NS_DEFINE_IID(kCPluginHostCID,         NS_PLUGIN_HOST_CID);
static NS_DEFINE_IID(kCPluginManagerCID,      NS_PLUGINMANAGER_CID);
static NS_DEFINE_IID(kIDocumentViewerIID,     NS_IDOCUMENT_VIEWER_IID);
static NS_DEFINE_IID(kRefreshURLIID,          NS_IREFRESHURL_IID);
static NS_DEFINE_IID(kITimerCallbackIID,      NS_ITIMERCALLBACK_IID);
static NS_DEFINE_IID(kIWebShellContainerIID,  NS_IWEB_SHELL_CONTAINER_IID);
static NS_DEFINE_IID(kIBrowserWindowIID,      NS_IBROWSER_WINDOW_IID);
static NS_DEFINE_IID(kIClipboardCommandsIID,  NS_ICLIPBOARDCOMMANDS_IID);
static NS_DEFINE_IID(kIEventQueueServiceIID,  NS_IEVENTQUEUESERVICE_IID);

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
  NS_INIT_REFCNT();
  mHistoryIndex = -1;
  mScrollPref = nsScrollPreference_kAuto;
  mScriptGlobal = nsnull;
  mScriptContext = nsnull;
//  mURLListener = nsnull;
  InitFrameData();
  mIsFrame = PR_FALSE;
}

nsWebShell::~nsWebShell()
{
  // Stop any pending document loads and destroy the loader...
  if (nsnull != mDocLoader) {
    mDocLoader->Stop();
    mDocLoader->RemoveObserver((nsIDocumentLoaderObserver*)this);
    mDocLoader->SetContainer(nsnull);
    NS_RELEASE(mDocLoader);
  }
  // Cancel any timers that were set for this loader.
  CancelRefreshURLTimers();

  NS_IF_RELEASE(mWindow);

  NS_IF_RELEASE(mContentViewer);
  NS_IF_RELEASE(mDeviceContext);
  NS_IF_RELEASE(mPrefs);
//  NS_IF_RELEASE(mURLListener);
  NS_IF_RELEASE(mContainer);
  NS_IF_RELEASE(mObserver);
  NS_IF_RELEASE(mNetSupport);

  if (nsnull != mScriptGlobal) {
    mScriptGlobal->SetWebShell(nsnull);
    NS_RELEASE(mScriptGlobal);
  }
  NS_IF_RELEASE(mScriptContext);

  InitFrameData();
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
}

void nsWebShell::InitFrameData()
{
  mMarginWidth  = -1;  
  mMarginHeight = -1;
  mScrolling    = -1;
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

NS_IMPL_THREADSAFE_ADDREF(nsWebShell)
NS_IMPL_THREADSAFE_RELEASE(nsWebShell)

nsresult
nsWebShell::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  nsresult rv = NS_NOINTERFACE;

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
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
  if (aIID.Equals(kRefreshURLIID)) {
    *aInstancePtr = (void*)(nsIRefreshUrl*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kINetSupportIID)) {
    *aInstancePtr = (void*) ((nsINetSupport*)this);
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

  mWindow->GetClientBounds(bounds);
  bounds.x = bounds.y = 0;
  rv = mContentViewer->Init(mWindow->GetNativeData(NS_NATIVE_WIDGET), 
                            mDeviceContext, 
                            mPrefs,
                            bounds,
                            mScrollPref);
  if (NS_SUCCEEDED(rv)) {
    mContentViewer->Show();
  }

  // Now that we have switched documents, forget all of our children
  ReleaseChildren();

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
nsWebShell::HandleUnknownContentType( nsIURL* aURL,
                                      const char *aContentType,
                                      const char *aCommand ) {
    // If we have a doc loader observer, let it respond to this.
    return mDocLoaderObserver ? mDocLoaderObserver->HandleUnknownContentType( aURL, aContentType, aCommand )
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
  nsIEventQueueService* eventService;
  rv = nsServiceManager::GetService(kEventQueueServiceCID,
                                    kIEventQueueServiceIID,
                                    (nsISupports **)&eventService);
  if (NS_SUCCEEDED(rv)) {
    // XXX: What if this fails?
    rv = eventService->GetThreadEventQueue(PR_GetCurrentThread(), 
                                           &mThreadEventQueue);
    nsServiceManager::ReleaseService(kEventQueueServiceCID, eventService);
  }


  //XXX make sure plugins have started up. this really needs to
  //be associated with the nsIContentViewerContainer interfaces,
  //not the nsIWebShell interfaces. this is a hack. MMP
  nsRect aBounds(x,y,w,h);
  nsWidgetInitData  widgetInit;

  CreatePluginHost(aAllowPlugins);

  //mScrollPref = aScrolling;

  WEB_TRACE(WEB_TRACE_CALLS,
            ("nsWebShell::Init: this=%p", this));

  // Initial error checking...
  NS_PRECONDITION(nsnull != aNativeParent, "null Parent Window");
  if (nsnull == aNativeParent) {
    rv = NS_ERROR_NULL_POINTER;
    goto done;
  }

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
    nsIDocumentLoader* docLoaderService;

    // Get the global document loader service...  
    rv = nsServiceManager::GetService(kDocLoaderServiceCID,
                                      kIDocumentLoaderIID,
                                      (nsISupports **)&docLoaderService);
    if (NS_SUCCEEDED(rv)) {
      rv = docLoaderService->CreateDocumentLoader(&mDocLoader);
      nsServiceManager::ReleaseService(kDocLoaderServiceCID, docLoaderService);
    }
  }
  if (NS_FAILED(rv)) {
    goto done;
  }

  // Set the webshell as the default IContentViewerContainer for the loader...
  mDocLoader->SetContainer(this);

  //Register ourselves as an observer for the new doc loader
  mDocLoader->AddObserver((nsIDocumentLoaderObserver*)this);

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
  if (NS_FAILED(rv)) {
    goto done;
  }

  widgetInit.clipChildren = PR_FALSE;
  //widgetInit.mBorderStyle = aIsSunkenBorder ? eBorderStyle_3DChildWindow : eBorderStyle_none;
  mWindow->Create(aNativeParent, aBounds, nsWebShell::HandleEvent,
                  mDeviceContext, nsnull, nsnull, &widgetInit);

done:
  return rv;
}

NS_IMETHODIMP
nsWebShell::Destroy()
{
  nsresult rv = NS_OK;

  // Stop any URLs that are currently being loaded...
  Stop();

//  SetURLListener(nsnull);
  SetContainer(nsnull);
  SetObserver(nsnull);
  SetDocLoaderObserver(nsnull);

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
  NS_PRECONDITION(nsnull != mWindow, "null window");
  aResult.SetRect(0, 0, 0, 0);
  if (nsnull != mWindow) {
    mWindow->GetClientBounds(aResult);
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
  NS_PRECONDITION(nsnull != mWindow, "null window");
  
  nsRect rectClient(0,0,w,h);
  PRInt32 borderWidth  = 0;
  PRInt32 borderHeight = 0;

  if (nsnull != mWindow) {
    mWindow->GetBorderSize(borderWidth, borderHeight);
    // Don't have the widget repaint. Layout will generate repaint requests
    // during reflow
    mWindow->Resize(x, y, w, h,
                    PR_FALSE);
  }

  // Set the size of the content area, which is the size of the window minus the borders
  if (nsnull != mContentViewer) {
    nsRect rr(0, 0, w-(borderWidth*2), h-(borderHeight*2));
    mContentViewer->SetBounds(rr);
  }

  return NS_OK;
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
  NS_PRECONDITION(nsnull != mWindow, "null window");

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
  NS_PRECONDITION(nsnull != mWindow, "null window");

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
  NS_PRECONDITION(nsnull != mWindow, "null window");

  if (nsnull != mWindow) {
    mWindow->SetFocus();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::RemoveFocus()
{
  NS_PRECONDITION(nsnull != mWindow, "null window");

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
  NS_PRECONDITION(nsnull != mWindow, "null window");

  if (nsnull != mWindow) {
    mWindow->Invalidate(aForce);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetContentViewer(nsIContentViewer* aViewer)
{
  NS_IF_RELEASE(mContentViewer);
  mContentViewer = aViewer;
  NS_IF_ADDREF(aViewer);
  return NS_OK;
}

#if 0
NS_IMETHODIMP
nsWebShell::SetURLListener(nsIURLListener* aURLListener)
{
  NS_IF_RELEASE(mURLListener);
  mURLListener = aURLListener;
  NS_IF_ADDREF(aURLListener);
  return NS_OK;
}

#endif  /* 0 */

NS_IMETHODIMP
nsWebShell::SetContainer(nsIWebShellContainer* aContainer)
{
  NS_IF_RELEASE(mContainer);
  mContainer = aContainer;
  NS_IF_ADDREF(aContainer);
  return NS_OK;
}

#if 0
NS_IMETHODIMP
nsWebShell::GetURLListener(nsIURLListener *& aResult)
{
  aResult = mURLListener;
  NS_IF_ADDREF(mURLListener);
  return NS_OK;
}
#endif  /* 0 */


NS_IMETHODIMP
nsWebShell::GetContainer(nsIWebShellContainer*& aResult)
{
  aResult = mContainer;
  NS_IF_ADDREF(mContainer);
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
  NS_IF_RELEASE(mNetSupport);

  mObserver = anObserver;
  if (nsnull != mObserver) {
    mObserver->QueryInterface(kINetSupportIID, (void **) &mNetSupport);
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
  NS_IF_RELEASE(mDocLoaderObserver);

  mDocLoaderObserver = anObserver;
  NS_IF_ADDREF(mDocLoaderObserver);
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
  NS_ADDREF(aChild);
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
  *aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetName(const PRUnichar* aName)
{
  mName = aName;
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
      nsresult rv = child->FindChildWithName(aName, aResult);
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
  aScrolling = mScrolling;
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::SetScrolling(PRInt32 aScrolling)
{
  mScrolling = aScrolling;
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
                    nsIPostData* aPostData,
                    PRBool aModifyHistory,
                    nsURLReloadType aType,
                    const PRUint32 aLocalIP)
{
  // if this is the top level web shell, initialize some things, in case the 
  // web shell is being recycled
  if (!mIsFrame) {
    InitFrameData();
  }

  return LoadURL(aURLSpec,"view",aPostData,aModifyHistory,aType, aLocalIP);
}

nsresult
nsWebShell::DoLoadURL(const nsString& aUrlSpec,
                      const char* aCommand,
                      nsIPostData* aPostData,
                      nsURLReloadType aType,
                      const PRUint32 aLocalIP)


{

  // If it's a normal reload that uses the cache, look at the destination anchor
  // and see if it's an element within the current document
  if ((aType == nsURLReload) && (nsnull != mContentViewer)) {
    nsCOMPtr<nsIDocumentViewer> docViewer;
    if (NS_SUCCEEDED(mContentViewer->QueryInterface(kIDocumentViewerIID,
                                                    getter_AddRefs(docViewer)))) {
      // Get the document object
      nsCOMPtr<nsIDocument> doc;
      docViewer->GetDocument(*getter_AddRefs(doc));

      // Get the URL for the document
      nsCOMPtr<nsIURL>  docURL = nsDontAddRef<nsIURL>(doc->GetDocumentURL());

      // See if they're the same
      nsCOMPtr<nsIURL>  url;
      NS_NewURL(getter_AddRefs(url), aUrlSpec);

      if ((PRBool)docURL->Equals(url)) {
        // See if there's a destination anchor
        const char* ref;
        url->GetRef(&ref);

        if (nsnull != ref) {
          // Get the pres shell object
          nsCOMPtr<nsIPresShell> presShell;
          docViewer->GetPresShell(*getter_AddRefs(presShell));

          presShell->GoToAnchor(nsAutoString(ref));
          return NS_OK;
        }
      }
    }
  }

  // Stop loading the current document (if any...).  This call may result in
  // firing an EndLoadURL notification for the old document...
  Stop();


  // Tell web-shell-container we are loading a new url
  if (nsnull != mContainer) {
    nsresult rv = mContainer->BeginLoadURL(this, aUrlSpec);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

#if 0
  // Tell URL listener we are loading a new url.
  if (nsnull != mURLListener) {
      nsresult rv = mURLListener->BeginLoadURL(this, aUrlSpec);
      if (NS_FAILED(rv)) {
          return rv;
      }
  }
#endif  /* 0 */

 /* WebShell was primarily passing the buck when it came to streamObserver.
  * So, pass on the observer which is already a streamObserver to DocLoder.
  *  - Radha
  */
  
  return mDocLoader->LoadDocument(aUrlSpec,       // URL string
                                  aCommand,       // Command
                                  this,           // Container
                                  aPostData,      // Post Data
                                  nsnull,         // Extra Info...
                                  mObserver,      // Observer
                                  aType,          // reload type
                                  aLocalIP);      // load attributes.
}

NS_IMETHODIMP
nsWebShell::LoadURL(const PRUnichar *aURLSpec,
                    const char* aCommand,
                    nsIPostData* aPostData,
                    PRBool aModifyHistory,
                    nsURLReloadType aType,
                    const PRUint32 aLocalIP)
{

  nsresult rv;
  PRInt32 colon, fSlash;
  PRUnichar port;
  nsAutoString urlSpec;
  convertFileToURL(nsString(aURLSpec), urlSpec);

  fSlash=urlSpec.Find('/');

  // if no scheme (protocol) is found, assume http.
  if ( ((colon=urlSpec.Find(':')) == -1) // no colon at all
       || ( (fSlash > -1) && (colon > fSlash) ) // the only colon comes after the first slash
       || ( (colon < urlSpec.Length()-1) // the first char after the first colon is a digit (i.e. a port)
            && ((port=urlSpec.CharAt(colon+1)) <= '9')
            && (port > '0') )) {
    // find host name
    int hostPos = urlSpec.FindCharInSet("./:");
    if (hostPos == -1) {
      hostPos = urlSpec.Length();
    }

    // extract host name
    nsAutoString hostSpec;
    urlSpec.Left(hostSpec, hostPos);

    // insert url spec corresponding to host name
    if (hostSpec.EqualsIgnoreCase("www")) {
      nsString ftpDef("http://");
      urlSpec.Insert(ftpDef, 0, 7);
    } else if (hostSpec.EqualsIgnoreCase("ftp")) {
      nsString ftpDef("ftp://");
      urlSpec.Insert(ftpDef, 0, 6);
    } else {
      nsString httpDef("http://");
      urlSpec.Insert(httpDef, 0, 7);
    }
  }

  // Give web-shell-container right of refusal
  if (nsnull != mContainer) {
    rv = mContainer->WillLoadURL(this, urlSpec, nsLoadURL);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

#if 0
  // Give URL listener right of refusal.
  if (nsnull != mURLListener) {
      rv = mURLListener->WillLoadURL(this, urlSpec, nsLoadURL);
      if (NS_FAILED(rv)) {
          return rv;
      }
  }
#endif  /* 0 */

  nsString* url = new nsString(urlSpec);
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

  return DoLoadURL(urlSpec, aCommand, aPostData, aType, aLocalIP);
}

NS_IMETHODIMP nsWebShell::Stop(void)
{
  if (nsnull != mContentViewer) {
    mContentViewer->Stop();
  }

  // Cancel any timers that were set for this loader.
  CancelRefreshURLTimers();

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

NS_IMETHODIMP nsWebShell::Reload(nsURLReloadType aType)
{
  nsString* s = (nsString*) mHistory.ElementAt(mHistoryIndex);
  if (nsnull != s) {
    // XXX What about the post data?
    return LoadURL(*s, nsnull, PR_FALSE, aType);
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
  return (mHistoryIndex > 0 ? NS_OK : NS_COMFALSE);
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
      rv = mContainer->WillLoadURL(this, urlSpec, nsLoadHistory);
      if (NS_FAILED(rv)) {
        return rv;
      }
    }

#if 0
    // Give URL listener right of refusal
    if (nsnull != mURLListener) {
        rv = mURLListener->WillLoadURL(this, urlSpec, nsLoadHistory);
        if (NS_FAILED(rv)) {
            return rv;
        }
    }
#endif  /* 0 */

    printf("Goto %d\n", aHistoryIndex);
    mHistoryIndex = aHistoryIndex;
    ShowHistory();

    rv = DoLoadURL(urlSpec,       // URL string
                   "view",        // Command
                   nsnull,        // Post Data
                   nsURLReload,   // the reload type
                   0);            // load attributes
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
  if ((aHistoryIndex >= 0) &&
      (aHistoryIndex <= mHistory.Count() - 1)) {
    nsString* s = (nsString*) mHistory.ElementAt(aHistoryIndex);
    if (nsnull != s) {
      *aURLResult = *s;
    }
    rv = NS_OK;
  }
  return rv;
}

void
nsWebShell::ShowHistory()
{
#ifdef NS_DEBUG
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
    }
  } else {
    NS_RELEASE(parent);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::GetTitle(const PRUnichar** aResult)
{
  *aResult = mTitle;
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
nsWebShell::EndLoadURL(nsIWebShell* aShell, const PRUnichar* aURL, PRInt32 aStatus)
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
nsWebShell::FindWebShellWithName(const PRUnichar* aName, nsIWebShell*& aResult)
{
  if (nsnull != mContainer) {
    return mContainer->FindWebShellWithName(aName, aResult);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::FocusAvailable(nsIWebShell* aFocusedWebShell)
{
  //If the WebShell with focus is us, pass this up to container
  if (this == aFocusedWebShell && nsnull != mContainer) {
    mContainer->FocusAvailable(this);
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
        mContainer->FocusAvailable(this);
        break;
      }
    }
  }
 
  return NS_OK;
}
//----------------------------------------------------------------------

// WebShell link handling

struct OnLinkClickEvent : public PLEvent {
  OnLinkClickEvent(nsWebShell* aHandler, nsIContent* aContent,
                   nsLinkVerb aVerb, const PRUnichar* aURLSpec,
                   const PRUnichar* aTargetSpec, nsIPostData* aPostData = 0);
  ~OnLinkClickEvent();

  void HandleEvent() {
    mHandler->HandleLinkClickEvent(mContent, mVerb, *mURLSpec, *
                                   mTargetSpec, mPostData);
  }

  nsWebShell*  mHandler;
  nsString*    mURLSpec;
  nsString*    mTargetSpec;
  nsIPostData* mPostData;
  nsIContent*  mContent;
  nsLinkVerb   mVerb;
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
                                   nsIPostData* aPostData)
{
  PLEventQueue* eventQueue;

  mHandler = aHandler;
  NS_ADDREF(aHandler);
  mURLSpec = new nsString(aURLSpec);
  mTargetSpec = new nsString(aTargetSpec);
  mPostData = aPostData;
  NS_IF_ADDREF(mPostData);
  mContent = aContent;
  NS_IF_ADDREF(mContent);
  mVerb = aVerb;
  
  PL_InitEvent(this, nsnull,
               (PLHandleEventProc) ::HandlePLEvent,
               (PLDestroyEventProc) ::DestroyPLEvent);

// XXX: The MAC ifdef should be replaced by the one in #else when
// it uses EventQueueService...

#ifdef XP_MAC 
	eventQueue = GetMacPLEventQueue();
#else
  eventQueue = aHandler->GetEventQueue();
#endif

	PL_PostEvent(eventQueue, this);
}

OnLinkClickEvent::~OnLinkClickEvent()
{
  NS_IF_RELEASE(mContent);
  NS_IF_RELEASE(mHandler);
  NS_IF_RELEASE(mPostData);
  if (nsnull != mURLSpec) delete mURLSpec;
  if (nsnull != mTargetSpec) delete mTargetSpec;
  
}

//----------------------------------------

NS_IMETHODIMP
nsWebShell::OnLinkClick(nsIContent* aContent, 
                        nsLinkVerb aVerb,
                        const PRUnichar* aURLSpec,
                        const PRUnichar* aTargetSpec,
                        nsIPostData* aPostData)
{
  OnLinkClickEvent* ev;
  nsresult rv = NS_OK;

  ev = new OnLinkClickEvent(this, aContent, aVerb, aURLSpec, 
                            aTargetSpec, aPostData);
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
    if (nsnull == mParent) {
      target = this;
    }
    else {
      target = mParent;
    }
    NS_ADDREF(target);
  }
  else if (name.EqualsIgnoreCase("_top")) {
    GetRootWebShell(target);
  }
  else {
    // Look from the top of the tree downward
    if (nsnull != mContainer) {
      mContainer->FindWebShellWithName(aName, target);
      if (nsnull == target) {
        mContainer->NewWebShell(PRUint32(~0), PR_TRUE, target);
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

PLEventQueue* nsWebShell::GetEventQueue(void)
{
  NS_PRECONDITION(nsnull != mThreadEventQueue, "PLEventQueue for thread is null");
  return mThreadEventQueue;
}

void
nsWebShell::HandleLinkClickEvent(nsIContent *aContent,
                                 nsLinkVerb aVerb,
                                 const PRUnichar* aURLSpec,
                                 const PRUnichar* aTargetSpec,
                                 nsIPostData* aPostData)
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
          shell->LoadURL(aURLSpec, aPostData);
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
nsWebShell:: GetLinkState(const PRUnichar* aURLSpec, nsLinkState& aState)
{
  nsString URLSpec(aURLSpec);
  aState = eLinkState_Unvisited;
#ifdef NS_DEBUG
  if (URLSpec.Equals("http://visited/")) {
    aState = eLinkState_Visited;
  }
  else if (URLSpec.Equals("http://out-of-date/")) {
    aState = eLinkState_OutOfDate;
  }
#endif
  return NS_OK;
}

//----------------------------------------------------------------------
nsIBrowserWindow* nsWebShell::GetBrowserWindow()
{
  nsIBrowserWindow *browserWindow = nsnull;
  nsIWebShell *rootWebShell;

  GetRootWebShell(rootWebShell);

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
    res = NS_CreateContext(mScriptGlobal, &mScriptContext);
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
nsWebShell::OnStartDocumentLoad(nsIURL* aURL, const char* aCommand)
{
  nsIDocumentViewer* docViewer;
  nsresult rv = NS_ERROR_FAILURE;
  
  if (nsnull != mScriptGlobal) {
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

  /*
   *Fire the OnStartDocumentLoad of the webshell observer
   */
  if ((nsnull != mContainer) && (nsnull != mDocLoaderObserver))
  {
     mDocLoaderObserver->OnStartDocumentLoad(aURL, aCommand);
  }

  
  return rv;
}



NS_IMETHODIMP
nsWebShell::OnEndDocumentLoad(nsIURL* aURL, PRInt32 aStatus)
{
  nsIDocumentViewer* docViewer;
  nsresult rv = NS_ERROR_FAILURE;

  if (nsnull != mScriptGlobal) {
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

  /* Fire the EndLoadURL of the container. This is primarily for the viewer */
  if (nsnull != aURL) {
     nsAutoString urlString;
     const char* spec;
     rv = aURL->GetSpec(&spec);

     /* XXX: The load status needs to be passed in... */
     if (NS_SUCCEEDED(rv)) {
       urlString = spec;
       if (nsnull != mContainer) {
          rv = mContainer->EndLoadURL(this, urlString, /* XXX */ 0 );
       }  
     }
  }

  /*
   *Fire the OnEndDocumentLoad of the DocLoaderobserver
   */
  if ((nsnull != mContainer) && (nsnull != mDocLoaderObserver) && (nsnull != aURL)){
     mDocLoaderObserver->OnEndDocumentLoad(aURL, aStatus);
  }
  
  return rv;

}

NS_IMETHODIMP
nsWebShell::OnStartURLLoad(nsIURL* aURL, const char* aContentType, 
                           nsIContentViewer* aViewer)
{

  // XXX This is a temporary hack for meeting the M4 milestone
  // for seamonkey.  I think Netlib should send a message to all stream listeners
  // when it changes the URL like this.  That would mean adding a new method
  // to nsIStreamListener.  Need to talk to Rick, Kipp, Gagan about this.
  CheckForTrailingSlash(aURL);

  /*
   *Fire the OnStartDocumentLoad of the webshell observer
   */
  if ((nsnull != mContainer) && (nsnull != mDocLoaderObserver))
  {
    mDocLoaderObserver->OnStartURLLoad(aURL, aContentType, aViewer);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::OnProgressURLLoad(nsIURL* aURL, PRUint32 aProgress, 
                              PRUint32 aProgressMax)
{
  /*
   *Fire the OnStartDocumentLoad of the webshell observer and container...
   */
  if ((nsnull != mContainer) && (nsnull != mDocLoaderObserver))
  {
     mDocLoaderObserver->OnProgressURLLoad(aURL, aProgress, aProgressMax);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::OnStatusURLLoad(nsIURL* aURL, nsString& aMsg)
{
  /*
   *Fire the OnStartDocumentLoad of the webshell observer and container...
   */
  if ((nsnull != mContainer) && (nsnull != mDocLoaderObserver))
  {
     mDocLoaderObserver->OnStatusURLLoad(aURL, aMsg);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWebShell::OnEndURLLoad(nsIURL* aURL, PRInt32 aStatus)
{
  /*
   *Fire the OnStartDocumentLoad of the webshell observer
   */
  if ((nsnull != mContainer) && (nsnull != mDocLoaderObserver))
  {
      mDocLoaderObserver->OnEndURLLoad(aURL, aStatus);
  }

  return NS_OK;
}


#if 0
NS_IMETHODIMP
nsWebShell::OnConnectionsComplete()
{
  nsIDocumentViewer* docViewer;
  nsresult rv = NS_ERROR_FAILURE;
  
  if (nsnull != mScriptGlobal) {
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
  
  /*
   *Fire the EndLoadURL(...) notification...
   */
  if (((nsnull != mURLListener) || (nsnull != mContainer)) && (nsnull != mContentViewer)) {
    nsIDocument* document;

    rv = mContentViewer->QueryInterface(kIDocumentViewerIID, (void**)&docViewer);
    if (NS_SUCCEEDED(rv)) {
      rv = docViewer->GetDocument(document);
      if (NS_SUCCEEDED(rv)) {
        nsAutoString urlString;
        nsIURL* url;

        url = document->GetDocumentURL();
        if (nsnull != url) {
          const char* spec;
          rv = url->GetSpec(&spec);

          /* XXX: The load status needs to be passed in... */
          if (NS_SUCCEEDED(rv)) {
            urlString = spec;
            if (nsnull != mContainer) {
                rv = mContainer->EndLoadURL(this, urlString, /* XXX */ 0 );
            }
            if (NS_SUCCEEDED(rv) && nsnull != mURLListener) {
                rv = mURLListener->EndLoadURL(this, urlString, /* XXX */ 0 );
            }
          }
          NS_RELEASE(url);
        }
        NS_RELEASE(document);
      }
      NS_RELEASE(docViewer);
    }
  }
  
  return rv;

}

#endif  /* 0 */


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
    mShell->LoadURL(mUrlSpec, nsnull, PR_TRUE, nsURLReload);
  }
  /* 
   * LoadURL(...) will cancel all refresh timers... This causes the Timer and
   * its refreshData instance to be released...
   */
}


NS_IMETHODIMP
nsWebShell::RefreshURL(nsIURL* aURL, PRInt32 millis, PRBool repeat)
{
  nsresult rv = NS_OK;
  nsITimer *timer=nsnull;
  refreshData *data;

  if (nsnull == aURL) {
    NS_PRECONDITION((aURL != nsnull), "Null pointer");
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

  const char* spec;
  rv = aURL->GetSpec(&spec);

  data->mUrlSpec  = spec;
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
nsWebShell::CancelRefreshURLTimers(void) {
  PRInt32 index;
  nsITimer* timer;

  /* Right now all we can do is cancel all the timers for this webshell. */
  NS_LOCK_INSTANCE();

  /* Walk the list backwards to avoid copying the array as it shrinks.. */
  for (index = mRefreshments.Count()-1; (0 <= index); index--) {
    timer=(nsITimer*)mRefreshments.ElementAt(index);
    /* Remove the entry from the list before releasing the timer.*/
    mRefreshments.RemoveElementAt(index);

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
nsresult nsWebShell::CheckForTrailingSlash(nsIURL* aURL)
{
  nsString* historyURL = (nsString*) mHistory.ElementAt(mHistoryIndex);
  const char* spec;
  aURL->GetSpec(&spec);
  nsString* newURL = (nsString*) new nsString(spec);

  if (newURL && newURL->Last() == '/' && !historyURL->Equals(*newURL)) {
    // Replace the top most history entry with the new url
    if (nsnull != historyURL) {
      delete historyURL;
    }
    mHistory.ReplaceElementAt(newURL, mHistoryIndex);
  } else
    delete newURL;

  return NS_OK;
}

#if 0
NS_IMETHODIMP
nsWebShell::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
  nsresult rv = NS_OK;

  if (nsnull != mObserver) {
    rv = mObserver->OnStartBinding(aURL, aContentType);
  }
  return rv;
}


NS_IMETHODIMP
nsWebShell::OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax)
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
nsWebShell::OnStatus(nsIURL* aURL, const PRUnichar* aMsg)
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
nsWebShell::OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg)
{
  nsresult rv = NS_OK;

  if (nsnull != mObserver) {
    rv = mObserver->OnStopBinding(aURL, aStatus, aMsg);
  }
  return rv;
}
#endif  /* 0 */


//----------------------------------------------------------------------

NS_IMETHODIMP_(void)
nsWebShell::Alert(const nsString &aText)
{
  if (nsnull != mNetSupport) {
    mNetSupport->Alert(aText);
  }
}

NS_IMETHODIMP_(PRBool)
nsWebShell::Confirm(const nsString &aText)
{
  PRBool bResult = PR_FALSE;

  if (nsnull != mNetSupport) {
    bResult = mNetSupport->Confirm(aText);
  }
  return bResult;
}

NS_IMETHODIMP_(PRBool)
nsWebShell::Prompt(const nsString &aText,
                   const nsString &aDefault,
                   nsString &aResult)
{
  PRBool bResult = PR_FALSE;

  if (nsnull != mNetSupport) {
    bResult = mNetSupport->Prompt(aText, aDefault, aResult);
  }
  return bResult;
}

NS_IMETHODIMP_(PRBool) 
nsWebShell::PromptUserAndPassword(const nsString &aText,
                                  nsString &aUser,
                                  nsString &aPassword)
{
  PRBool bResult = PR_FALSE;

  if (nsnull != mNetSupport) {
    bResult = mNetSupport->PromptUserAndPassword(aText, aUser, aPassword);
  }
  return bResult;
}

NS_IMETHODIMP_(PRBool) 
nsWebShell::PromptPassword(const nsString &aText,
                           nsString &aPassword)
{
  PRBool bResult = PR_FALSE;

  if (nsnull != mNetSupport) {
    bResult = mNetSupport->PromptPassword(aText, aPassword);
  }
  return bResult;
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
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsWebShell::SelectNone(void)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP 
nsWebShell::GetDefaultCharacterSet (const PRUnichar** aDefaultCharacterSet)
{
  *aDefaultCharacterSet = mDefaultCharacterSet;
  return NS_OK;
}
NS_IMETHODIMP 
nsWebShell::SetDefaultCharacterSet (const PRUnichar*  aDefaultCharacterSet)  
{
  mDefaultCharacterSet = aDefaultCharacterSet;
  return NS_OK;
}

//----------------------------------------------------
NS_IMETHODIMP
nsWebShell::FindNext(const PRUnichar * aSearchStr, PRBool aMatchCase, PRBool aSearchDown, PRBool &aIsFound)
{
  return NS_ERROR_FAILURE;
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
  // XXX TEMPORARY Till we have real pluggable protocol handlers
  NET_InitJavaScriptProtocol();
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
