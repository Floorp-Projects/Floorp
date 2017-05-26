/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsIAppShellService.h"
#include "nsIComponentManager.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIServiceManager.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsIXPConnect.h"
#include "nsIXULRuntime.h"

#include "nsIWindowMediator.h"
#include "nsIWindowWatcher.h"
#include "nsPIWindowWatcher.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsWebShellWindow.h"

#include "nsWidgetInitData.h"
#include "nsWidgetsCID.h"
#include "nsIWidget.h"
#include "nsIRequestObserver.h"
#include "nsIEmbeddingSiteWindow.h"

#include "nsAppShellService.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"
#include "nsISupportsPrimitives.h"
#include "nsILoadContext.h"
#include "nsIWebNavigation.h"
#include "nsIWindowlessBrowser.h"

#include "mozilla/Attributes.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/StartupTimeline.h"
#include "mozilla/intl/LocaleService.h"

#include "nsEmbedCID.h"
#include "nsIWebBrowser.h"
#include "nsIDocShell.h"
#include "gfxPlatform.h"

#ifdef MOZ_INSTRUMENT_EVENT_LOOP
#include "EventTracer.h"
#endif

using namespace mozilla;
using mozilla::intl::LocaleService;

// Default URL for the hidden window, can be overridden by a pref on Mac
#define DEFAULT_HIDDENWINDOW_URL "resource://gre-resources/hiddenWindow.html"

class nsIAppShell;

nsAppShellService::nsAppShellService() :
  mXPCOMWillShutDown(false),
  mXPCOMShuttingDown(false),
  mModalWindowCount(0),
  mApplicationProvidedHiddenWindow(false),
  mScreenId(0)
{
  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();

  if (obs) {
    obs->AddObserver(this, "xpcom-will-shutdown", false);
    obs->AddObserver(this, "xpcom-shutdown", false);
  }
}

nsAppShellService::~nsAppShellService()
{
}


/*
 * Implement the nsISupports methods...
 */
NS_IMPL_ISUPPORTS(nsAppShellService,
                  nsIAppShellService,
                  nsIObserver)

NS_IMETHODIMP
nsAppShellService::CreateHiddenWindow()
{
  return CreateHiddenWindowHelper(false);
}

NS_IMETHODIMP
nsAppShellService::SetScreenId(uint32_t aScreenId)
{
  mScreenId = aScreenId;
  return NS_OK;
}

void
nsAppShellService::EnsurePrivateHiddenWindow()
{
  if (!mHiddenPrivateWindow) {
    CreateHiddenWindowHelper(true);
  }
}

nsresult
nsAppShellService::CreateHiddenWindowHelper(bool aIsPrivate)
{
  nsresult rv;
  int32_t initialHeight = 100, initialWidth = 100;

#ifdef XP_MACOSX
  uint32_t    chromeMask = 0;
  nsAdoptingCString prefVal =
      Preferences::GetCString("browser.hiddenWindowChromeURL");
  const char* hiddenWindowURL = prefVal.get() ? prefVal.get() : DEFAULT_HIDDENWINDOW_URL;
  if (aIsPrivate) {
    hiddenWindowURL = DEFAULT_HIDDENWINDOW_URL;
  } else {
    mApplicationProvidedHiddenWindow = prefVal.get() ? true : false;
  }
#else
  static const char hiddenWindowURL[] = DEFAULT_HIDDENWINDOW_URL;
  uint32_t    chromeMask =  nsIWebBrowserChrome::CHROME_ALL;
#endif

  nsCOMPtr<nsIURI> url;
  rv = NS_NewURI(getter_AddRefs(url), hiddenWindowURL);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aIsPrivate) {
    chromeMask |= nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW;
  }

  RefPtr<nsWebShellWindow> newWindow;
  rv = JustCreateTopWindow(nullptr, url,
                           chromeMask, initialWidth, initialHeight,
                           true, nullptr, nullptr, getter_AddRefs(newWindow));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDocShell> docShell;
  newWindow->GetDocShell(getter_AddRefs(docShell));
  if (docShell) {
    if (aIsPrivate) {
      docShell->SetAffectPrivateSessionLifetime(false);
    }
  }

  if (aIsPrivate) {
    mHiddenPrivateWindow.swap(newWindow);
  } else {
    mHiddenWindow.swap(newWindow);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAppShellService::DestroyHiddenWindow()
{
  if (mHiddenWindow) {
    mHiddenWindow->Destroy();

    mHiddenWindow = nullptr;
  }

  if (mHiddenPrivateWindow) {
    mHiddenPrivateWindow->Destroy();

    mHiddenPrivateWindow = nullptr;
  }

  return NS_OK;
}

/*
 * Create a new top level window and display the given URL within it...
 */
NS_IMETHODIMP
nsAppShellService::CreateTopLevelWindow(nsIXULWindow *aParent,
                                        nsIURI *aUrl,
                                        uint32_t aChromeMask,
                                        int32_t aInitialWidth,
                                        int32_t aInitialHeight,
                                        nsITabParent *aOpeningTab,
                                        mozIDOMWindowProxy *aOpenerWindow,
                                        nsIXULWindow **aResult)

{
  nsresult rv;

  StartupTimeline::RecordOnce(StartupTimeline::CREATE_TOP_LEVEL_WINDOW);

  RefPtr<nsWebShellWindow> newWindow;
  rv = JustCreateTopWindow(aParent, aUrl,
                           aChromeMask, aInitialWidth, aInitialHeight,
                           false, aOpeningTab, aOpenerWindow,
                           getter_AddRefs(newWindow));
  newWindow.forget(aResult);

  if (NS_SUCCEEDED(rv)) {
    // the addref resulting from this is the owning addref for this window
    RegisterTopLevelWindow(*aResult);
    nsCOMPtr<nsIXULWindow> parent;
    if (aChromeMask & nsIWebBrowserChrome::CHROME_DEPENDENT)
      parent = aParent;
    (*aResult)->SetZLevel(CalculateWindowZLevel(parent, aChromeMask));
  }

  return rv;
}

/*
 * This class provides a stub implementation of nsIWebBrowserChrome2, as needed
 * by nsAppShellService::CreateWindowlessBrowser
 */
class WebBrowserChrome2Stub : public nsIWebBrowserChrome2,
                              public nsIEmbeddingSiteWindow,
                              public nsIInterfaceRequestor,
                              public nsSupportsWeakReference {
protected:
    nsCOMPtr<nsIWebBrowser> mBrowser;
    virtual ~WebBrowserChrome2Stub() {}
public:
    explicit WebBrowserChrome2Stub(nsIWebBrowser *aBrowser) : mBrowser(aBrowser) {}
    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBBROWSERCHROME
    NS_DECL_NSIWEBBROWSERCHROME2
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIEMBEDDINGSITEWINDOW
};

NS_INTERFACE_MAP_BEGIN(WebBrowserChrome2Stub)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserChrome)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome2)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIEmbeddingSiteWindow)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WebBrowserChrome2Stub)
NS_IMPL_RELEASE(WebBrowserChrome2Stub)

NS_IMETHODIMP
WebBrowserChrome2Stub::SetStatus(uint32_t aStatusType, const char16_t* aStatus)
{
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserChrome2Stub::GetWebBrowser(nsIWebBrowser** aWebBrowser)
{
  NS_NOTREACHED("WebBrowserChrome2Stub::GetWebBrowser is not supported");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebBrowserChrome2Stub::SetWebBrowser(nsIWebBrowser* aWebBrowser)
{
  NS_NOTREACHED("WebBrowserChrome2Stub::SetWebBrowser is not supported");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebBrowserChrome2Stub::GetChromeFlags(uint32_t* aChromeFlags)
{
  *aChromeFlags = 0;
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserChrome2Stub::SetChromeFlags(uint32_t aChromeFlags)
{
  NS_NOTREACHED("WebBrowserChrome2Stub::SetChromeFlags is not supported");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebBrowserChrome2Stub::DestroyBrowserWindow()
{
  NS_NOTREACHED("WebBrowserChrome2Stub::DestroyBrowserWindow is not supported");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebBrowserChrome2Stub::SizeBrowserTo(int32_t aCX, int32_t aCY)
{
  NS_NOTREACHED("WebBrowserChrome2Stub::SizeBrowserTo is not supported");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebBrowserChrome2Stub::ShowAsModal()
{
  NS_NOTREACHED("WebBrowserChrome2Stub::ShowAsModal is not supported");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebBrowserChrome2Stub::IsWindowModal(bool* aResult)
{
  *aResult = false;
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserChrome2Stub::ExitModalEventLoop(nsresult aStatus)
{
  NS_NOTREACHED("WebBrowserChrome2Stub::ExitModalEventLoop is not supported");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebBrowserChrome2Stub::SetStatusWithContext(uint32_t aStatusType,
                                            const nsAString& aStatusText,
                                            nsISupports* aStatusContext)
{
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserChrome2Stub::GetInterface(const nsIID& aIID, void** aSink)
{
    return QueryInterface(aIID, aSink);
}

// nsIEmbeddingSiteWindow impl
NS_IMETHODIMP
WebBrowserChrome2Stub::GetDimensions(uint32_t flags, int32_t* x, int32_t* y, int32_t* cx, int32_t* cy)
{
  if (x) {
    *x = 0;
  }

  if (y) {
    *y = 0;
  }

  if (cx) {
    *cx = 0;
  }

  if (cy) {
    *cy = 0;
  }

  return NS_OK;
}

NS_IMETHODIMP
WebBrowserChrome2Stub::SetDimensions(uint32_t flags, int32_t x, int32_t y, int32_t cx, int32_t cy)
{
  nsCOMPtr<nsIBaseWindow> window = do_QueryInterface(mBrowser);
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);
  window->SetSize(cx, cy, true);
  return NS_OK;
}

NS_IMETHODIMP
WebBrowserChrome2Stub::SetFocus()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebBrowserChrome2Stub::GetVisibility(bool* aVisibility)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
WebBrowserChrome2Stub::SetVisibility(bool aVisibility)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebBrowserChrome2Stub::GetTitle(char16_t** aTitle)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
NS_IMETHODIMP
WebBrowserChrome2Stub::SetTitle(const char16_t* aTitle)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebBrowserChrome2Stub::GetSiteWindow(void** aSiteWindow)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
WebBrowserChrome2Stub::Blur()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

class BrowserDestroyer final : public Runnable
{
public:
  BrowserDestroyer(nsIWebBrowser *aBrowser, nsISupports *aContainer) :
    mBrowser(aBrowser),
    mContainer(aContainer)
  {
  }

  NS_IMETHOD
  Run() override
  {
    // Explicitly destroy the browser, in case this isn't the last reference.
    nsCOMPtr<nsIBaseWindow> window = do_QueryInterface(mBrowser);
    return window->Destroy();
  }

protected:
  virtual ~BrowserDestroyer() {}

private:
  nsCOMPtr<nsIWebBrowser> mBrowser;
  nsCOMPtr<nsISupports> mContainer;
};

// This is the "stub" we return from CreateWindowlessBrowser - it exists
// to manage the lifetimes of the nsIWebBrowser and container window.
// In particular, it keeps a strong reference to both, to prevent them from
// being collected while this object remains alive, and ensures that they
// aren't destroyed when it's not safe to run scripts.
class WindowlessBrowser final : public nsIWindowlessBrowser,
                                public nsIInterfaceRequestor
{
public:
  WindowlessBrowser(nsIWebBrowser *aBrowser, nsISupports *aContainer) :
    mBrowser(aBrowser),
    mContainer(aContainer),
    mClosed(false)
  {
    mWebNavigation = do_QueryInterface(aBrowser);
    mInterfaceRequestor = do_QueryInterface(aBrowser);
  }
  NS_DECL_ISUPPORTS
  NS_FORWARD_SAFE_NSIWEBNAVIGATION(mWebNavigation)
  NS_FORWARD_SAFE_NSIINTERFACEREQUESTOR(mInterfaceRequestor)

  NS_IMETHOD
  Close() override
  {
    NS_ENSURE_TRUE(!mClosed, NS_ERROR_UNEXPECTED);
    NS_ASSERTION(nsContentUtils::IsSafeToRunScript(),
                 "WindowlessBrowser::Close called when not safe to run scripts");

    mClosed = true;

    mWebNavigation = nullptr;
    mInterfaceRequestor = nullptr;

    nsCOMPtr<nsIBaseWindow> window = do_QueryInterface(mBrowser);
    return window->Destroy();
  }

protected:
  virtual ~WindowlessBrowser()
  {
    if (mClosed) {
      return;
    }

    NS_WARNING("Windowless browser was not closed prior to destruction");

    // The docshell destructor needs to dispatch events, and can only run
    // when it's safe to run scripts. If this was triggered by GC, it may
    // not always be safe to run scripts, in which cases we need to delay
    // destruction until it is.
    nsCOMPtr<nsIRunnable> runnable = new BrowserDestroyer(mBrowser, mContainer);
    nsContentUtils::AddScriptRunner(runnable);
  }

private:
  nsCOMPtr<nsIWebBrowser> mBrowser;
  nsCOMPtr<nsIWebNavigation> mWebNavigation;
  nsCOMPtr<nsIInterfaceRequestor> mInterfaceRequestor;
  // we don't use the container but just hold a reference to it.
  nsCOMPtr<nsISupports> mContainer;

  bool mClosed;
};

NS_IMPL_ISUPPORTS(WindowlessBrowser, nsIWindowlessBrowser, nsIWebNavigation, nsIInterfaceRequestor)


NS_IMETHODIMP
nsAppShellService::CreateWindowlessBrowser(bool aIsChrome, nsIWindowlessBrowser **aResult)
{
  /* First, we create an instance of nsWebBrowser. Instances of this class have
   * an associated doc shell, which is what we're interested in.
   */
  nsCOMPtr<nsIWebBrowser> browser = do_CreateInstance(NS_WEBBROWSER_CONTRACTID);
  if (!browser) {
    NS_ERROR("Couldn't create instance of nsWebBrowser!");
    return NS_ERROR_FAILURE;
  }

  /* Next, we set the container window for our instance of nsWebBrowser. Since
   * we don't actually have a window, we instead set the container window to be
   * an instance of WebBrowserChrome2Stub, which provides a stub implementation
   * of nsIWebBrowserChrome2.
   */
  RefPtr<WebBrowserChrome2Stub> stub = new WebBrowserChrome2Stub(browser);
  browser->SetContainerWindow(stub);

  nsCOMPtr<nsIWebNavigation> navigation = do_QueryInterface(browser);

  nsCOMPtr<nsIDocShellTreeItem> item = do_QueryInterface(navigation);
  item->SetItemType(aIsChrome ? nsIDocShellTreeItem::typeChromeWrapper
                              : nsIDocShellTreeItem::typeContentWrapper);

  /* A windowless web browser doesn't have an associated OS level window. To
   * accomplish this, we initialize the window associated with our instance of
   * nsWebBrowser with an instance of HeadlessWidget/PuppetWidget, which provide
   * a stub implementation of nsIWidget.
   */
  nsCOMPtr<nsIWidget> widget;
  if (gfxPlatform::IsHeadless()) {
    widget = nsIWidget::CreateHeadlessWidget();
  } else {
    widget = nsIWidget::CreatePuppetWidget(nullptr);
  }
  if (!widget) {
    NS_ERROR("Couldn't create instance of stub widget");
    return NS_ERROR_FAILURE;
  }
  nsresult rv =
    widget->Create(nullptr, 0, LayoutDeviceIntRect(0, 0, 0, 0), nullptr);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsIBaseWindow> window = do_QueryInterface(navigation);
  window->InitWindow(0, widget, 0, 0, 0, 0);
  window->Create();

  nsISupports *isstub = NS_ISUPPORTS_CAST(nsIWebBrowserChrome2*, stub);
  RefPtr<nsIWindowlessBrowser> result = new WindowlessBrowser(browser, isstub);
  nsCOMPtr<nsIDocShell> docshell = do_GetInterface(result);
  docshell->SetInvisible(true);

  result.forget(aResult);
  return NS_OK;
}

uint32_t
nsAppShellService::CalculateWindowZLevel(nsIXULWindow *aParent,
                                         uint32_t      aChromeMask)
{
  uint32_t zLevel;

  zLevel = nsIXULWindow::normalZ;
  if (aChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_RAISED)
    zLevel = nsIXULWindow::raisedZ;
  else if (aChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_LOWERED)
    zLevel = nsIXULWindow::loweredZ;

#ifdef XP_MACOSX
  /* Platforms on which modal windows are always application-modal, not
     window-modal (that's just the Mac, right?) want modal windows to
     be stacked on top of everyone else.

     On Mac OS X, bind modality to parent window instead of app (ala Mac OS 9)
  */
  uint32_t modalDepMask = nsIWebBrowserChrome::CHROME_MODAL |
                          nsIWebBrowserChrome::CHROME_DEPENDENT;
  if (aParent && (aChromeMask & modalDepMask)) {
    aParent->GetZLevel(&zLevel);
  }
#else
  /* Platforms with native support for dependent windows (that's everyone
      but pre-Mac OS X, right?) know how to stack dependent windows. On these
      platforms, give the dependent window the same level as its parent,
      so we won't try to override the normal platform behaviour. */
  if ((aChromeMask & nsIWebBrowserChrome::CHROME_DEPENDENT) && aParent)
    aParent->GetZLevel(&zLevel);
#endif

  return zLevel;
}

#ifdef XP_WIN
/*
 * Checks to see if any existing window is currently in fullscreen mode.
 */
static bool
CheckForFullscreenWindow()
{
  nsCOMPtr<nsIWindowMediator> wm(do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (!wm)
    return false;

  nsCOMPtr<nsISimpleEnumerator> windowList;
  wm->GetXULWindowEnumerator(nullptr, getter_AddRefs(windowList));
  if (!windowList)
    return false;

  for (;;) {
    bool more = false;
    windowList->HasMoreElements(&more);
    if (!more)
      return false;

    nsCOMPtr<nsISupports> supportsWindow;
    windowList->GetNext(getter_AddRefs(supportsWindow));
    nsCOMPtr<nsIBaseWindow> baseWin(do_QueryInterface(supportsWindow));
    if (baseWin) {
      nsCOMPtr<nsIWidget> widget;
      baseWin->GetMainWidget(getter_AddRefs(widget));
      if (widget && widget->SizeMode() == nsSizeMode_Fullscreen) {
        return true;
      }
    }
  }
  return false;
}
#endif

/*
 * Just do the window-making part of CreateTopLevelWindow
 */
nsresult
nsAppShellService::JustCreateTopWindow(nsIXULWindow *aParent,
                                       nsIURI *aUrl,
                                       uint32_t aChromeMask,
                                       int32_t aInitialWidth,
                                       int32_t aInitialHeight,
                                       bool aIsHiddenWindow,
                                       nsITabParent *aOpeningTab,
                                       mozIDOMWindowProxy *aOpenerWindow,
                                       nsWebShellWindow **aResult)
{
  *aResult = nullptr;
  NS_ENSURE_STATE(!mXPCOMWillShutDown);

  nsCOMPtr<nsIXULWindow> parent;
  if (aChromeMask & nsIWebBrowserChrome::CHROME_DEPENDENT)
    parent = aParent;

  RefPtr<nsWebShellWindow> window = new nsWebShellWindow(aChromeMask);

#ifdef XP_WIN
  // If the parent is currently fullscreen, tell the child to ignore persisted
  // full screen states. This way new browser windows open on top of fullscreen
  // windows normally.
  if (window && CheckForFullscreenWindow())
    window->IgnoreXULSizeMode(true);
#endif

  nsWidgetInitData widgetInitData;

  if (aIsHiddenWindow)
    widgetInitData.mWindowType = eWindowType_invisible;
  else
    widgetInitData.mWindowType = aChromeMask & nsIWebBrowserChrome::CHROME_OPENAS_DIALOG ?
      eWindowType_dialog : eWindowType_toplevel;

  if (aChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_POPUP)
    widgetInitData.mWindowType = eWindowType_popup;

  if (aChromeMask & nsIWebBrowserChrome::CHROME_SUPPRESS_ANIMATION)
    widgetInitData.mIsAnimationSuppressed = true;

#ifdef XP_MACOSX
  // Mac OS X sheet support
  // Adding CHROME_OPENAS_CHROME to sheetMask makes modal windows opened from
  // nsGlobalWindow::ShowModalDialog() be dialogs (not sheets), while modal
  // windows opened from nsPromptService::DoDialog() still are sheets.  This
  // fixes bmo bug 395465 (see nsCocoaWindow::StandardCreate() and
  // nsCocoaWindow::SetModal()).
  uint32_t sheetMask = nsIWebBrowserChrome::CHROME_OPENAS_DIALOG |
                       nsIWebBrowserChrome::CHROME_MODAL |
                       nsIWebBrowserChrome::CHROME_OPENAS_CHROME;
  if (parent &&
      (parent != mHiddenWindow && parent != mHiddenPrivateWindow) &&
      ((aChromeMask & sheetMask) == sheetMask)) {
    widgetInitData.mWindowType = eWindowType_sheet;
  }
#endif

#if defined(XP_WIN)
  if (widgetInitData.mWindowType == eWindowType_toplevel ||
      widgetInitData.mWindowType == eWindowType_dialog)
    widgetInitData.clipChildren = true;
#endif

  // note default chrome overrides other OS chrome settings, but
  // not internal chrome
  if (aChromeMask & nsIWebBrowserChrome::CHROME_DEFAULT)
    widgetInitData.mBorderStyle = eBorderStyle_default;
  else if ((aChromeMask & nsIWebBrowserChrome::CHROME_ALL) == nsIWebBrowserChrome::CHROME_ALL)
    widgetInitData.mBorderStyle = eBorderStyle_all;
  else {
    widgetInitData.mBorderStyle = eBorderStyle_none; // assumes none == 0x00
    if (aChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_BORDERS)
      widgetInitData.mBorderStyle = static_cast<enum nsBorderStyle>(widgetInitData.mBorderStyle | eBorderStyle_border);
    if (aChromeMask & nsIWebBrowserChrome::CHROME_TITLEBAR)
      widgetInitData.mBorderStyle = static_cast<enum nsBorderStyle>(widgetInitData.mBorderStyle | eBorderStyle_title);
    if (aChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_CLOSE)
      widgetInitData.mBorderStyle = static_cast<enum nsBorderStyle>(widgetInitData.mBorderStyle | eBorderStyle_close);
    if (aChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_RESIZE) {
      widgetInitData.mBorderStyle = static_cast<enum nsBorderStyle>(widgetInitData.mBorderStyle | eBorderStyle_resizeh);
      // only resizable windows get the maximize button (but not dialogs)
      if (!(aChromeMask & nsIWebBrowserChrome::CHROME_OPENAS_DIALOG))
        widgetInitData.mBorderStyle = static_cast<enum nsBorderStyle>(widgetInitData.mBorderStyle | eBorderStyle_maximize);
    }
    // all windows (except dialogs) get minimize buttons and the system menu
    if (!(aChromeMask & nsIWebBrowserChrome::CHROME_OPENAS_DIALOG))
      widgetInitData.mBorderStyle = static_cast<enum nsBorderStyle>(widgetInitData.mBorderStyle | eBorderStyle_minimize | eBorderStyle_menu);
    // but anyone can explicitly ask for a minimize button
    if (aChromeMask & nsIWebBrowserChrome::CHROME_WINDOW_MIN) {
      widgetInitData.mBorderStyle = static_cast<enum nsBorderStyle>(widgetInitData.mBorderStyle | eBorderStyle_minimize);
    }
  }

  if (aInitialWidth == nsIAppShellService::SIZE_TO_CONTENT ||
      aInitialHeight == nsIAppShellService::SIZE_TO_CONTENT) {
    aInitialWidth = 1;
    aInitialHeight = 1;
    window->SetIntrinsicallySized(true);
  }

  bool center = aChromeMask & nsIWebBrowserChrome::CHROME_CENTER_SCREEN;

  widgetInitData.mRTL = LocaleService::GetInstance()->IsAppLocaleRTL();

#ifdef MOZ_WIDGET_GONK
  // B2G multi-screen support. Screen ID is for differentiating screens of
  // windows, and due to the hardware limitation, it is platform-specific for
  // now, which align with the value of display type defined in HWC.
  widgetInitData.mScreenId = mScreenId;
#endif

  nsresult rv = window->Initialize(parent, center ? aParent : nullptr,
                                   aUrl, aInitialWidth, aInitialHeight,
                                   aIsHiddenWindow, aOpeningTab,
                                   aOpenerWindow, widgetInitData);

  NS_ENSURE_SUCCESS(rv, rv);

  // Enforce the Private Browsing autoStart pref first.
  bool isPrivateBrowsingWindow =
    Preferences::GetBool("browser.privatebrowsing.autostart");
  bool isUsingRemoteTabs = mozilla::BrowserTabsRemoteAutostart();

  if (aChromeMask & nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW) {
    // Caller requested a private window
    isPrivateBrowsingWindow = true;
  }
  if (aChromeMask & nsIWebBrowserChrome::CHROME_REMOTE_WINDOW) {
    isUsingRemoteTabs = true;
  }

  nsCOMPtr<mozIDOMWindowProxy> domWin = do_GetInterface(aParent);
  nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(domWin);
  nsCOMPtr<nsILoadContext> parentContext = do_QueryInterface(webNav);

  if (!isPrivateBrowsingWindow && parentContext) {
    // Ensure that we propagate any existing private browsing status
    // from the parent, even if it will not actually be used
    // as a parent value.
    isPrivateBrowsingWindow = parentContext->UsePrivateBrowsing();
  }

  if (parentContext) {
    isUsingRemoteTabs = parentContext->UseRemoteTabs();
  }

  nsCOMPtr<mozIDOMWindowProxy> newDomWin =
      do_GetInterface(NS_ISUPPORTS_CAST(nsIBaseWindow*, window));
  nsCOMPtr<nsIWebNavigation> newWebNav = do_GetInterface(newDomWin);
  nsCOMPtr<nsILoadContext> thisContext = do_GetInterface(newWebNav);
  if (thisContext) {
    thisContext->SetPrivateBrowsing(isPrivateBrowsingWindow);
    thisContext->SetRemoteTabs(isUsingRemoteTabs);
  }

  window.forget(aResult);
  if (parent)
    parent->AddChildWindow(*aResult);

  if (center)
    rv = (*aResult)->Center(parent, parent ? false : true, false);

  return rv;
}

NS_IMETHODIMP
nsAppShellService::GetHiddenWindow(nsIXULWindow **aWindow)
{
  NS_ENSURE_ARG_POINTER(aWindow);

  *aWindow = mHiddenWindow;
  NS_IF_ADDREF(*aWindow);
  return *aWindow ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsAppShellService::GetHiddenDOMWindow(mozIDOMWindowProxy **aWindow)
{
  nsresult rv;
  nsCOMPtr<nsIDocShell> docShell;
  NS_ENSURE_TRUE(mHiddenWindow, NS_ERROR_FAILURE);

  rv = mHiddenWindow->GetDocShell(getter_AddRefs(docShell));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsPIDOMWindowOuter> hiddenDOMWindow(docShell->GetWindow());
  hiddenDOMWindow.forget(aWindow);
  return *aWindow ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsAppShellService::GetHiddenPrivateWindow(nsIXULWindow **aWindow)
{
  NS_ENSURE_ARG_POINTER(aWindow);

  EnsurePrivateHiddenWindow();

  *aWindow = mHiddenPrivateWindow;
  NS_IF_ADDREF(*aWindow);
  return *aWindow ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsAppShellService::GetHiddenPrivateDOMWindow(mozIDOMWindowProxy **aWindow)
{
  EnsurePrivateHiddenWindow();

  nsresult rv;
  nsCOMPtr<nsIDocShell> docShell;
  NS_ENSURE_TRUE(mHiddenPrivateWindow, NS_ERROR_FAILURE);

  rv = mHiddenPrivateWindow->GetDocShell(getter_AddRefs(docShell));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsPIDOMWindowOuter> hiddenPrivateDOMWindow(docShell->GetWindow());
  hiddenPrivateDOMWindow.forget(aWindow);
  return *aWindow ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsAppShellService::GetHasHiddenPrivateWindow(bool* aHasPrivateWindow)
{
  NS_ENSURE_ARG_POINTER(aHasPrivateWindow);

  *aHasPrivateWindow = !!mHiddenPrivateWindow;
  return NS_OK;
}

NS_IMETHODIMP
nsAppShellService::GetApplicationProvidedHiddenWindow(bool* aAPHW)
{
    *aAPHW = mApplicationProvidedHiddenWindow;
    return NS_OK;
}

/*
 * Register a new top level window (created elsewhere)
 */
NS_IMETHODIMP
nsAppShellService::RegisterTopLevelWindow(nsIXULWindow* aWindow)
{
  NS_ENSURE_ARG_POINTER(aWindow);

  nsCOMPtr<nsIDocShell> docShell;
  aWindow->GetDocShell(getter_AddRefs(docShell));
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsPIDOMWindowOuter> domWindow(docShell->GetWindow());
  NS_ENSURE_TRUE(domWindow, NS_ERROR_FAILURE);
  domWindow->SetInitialPrincipalToSubject();

  // tell the window mediator about the new window
  nsCOMPtr<nsIWindowMediator> mediator
    ( do_GetService(NS_WINDOWMEDIATOR_CONTRACTID) );
  NS_ASSERTION(mediator, "Couldn't get window mediator.");

  if (mediator)
    mediator->RegisterWindow(aWindow);

  // tell the window watcher about the new window
  nsCOMPtr<nsPIWindowWatcher> wwatcher ( do_GetService(NS_WINDOWWATCHER_CONTRACTID) );
  NS_ASSERTION(wwatcher, "No windowwatcher?");
  if (wwatcher && domWindow) {
    wwatcher->AddWindow(domWindow, 0);
  }

  // an ongoing attempt to quit is stopped by a newly opened window
  nsCOMPtr<nsIObserverService> obssvc = services::GetObserverService();
  NS_ASSERTION(obssvc, "Couldn't get observer service.");

  if (obssvc) {
    obssvc->NotifyObservers(aWindow, "xul-window-registered", nullptr);
    nsXULWindow* xulWindow = static_cast<nsXULWindow*>(aWindow);
    xulWindow->WasRegistered();
  }

  return NS_OK;
}


NS_IMETHODIMP
nsAppShellService::UnregisterTopLevelWindow(nsIXULWindow* aWindow)
{
  if (mXPCOMShuttingDown) {
    /* return an error code in order to:
       - avoid doing anything with other member variables while we are in
         the destructor
       - notify the caller not to release the AppShellService after
         unregistering the window
         (we don't want to be deleted twice consecutively to
         mHiddenWindow->Destroy() in our destructor)
    */
    return NS_ERROR_FAILURE;
  }

  NS_ENSURE_ARG_POINTER(aWindow);

  if (aWindow == mHiddenWindow) {
    // CreateHiddenWindow() does not register the window, so we're done.
    return NS_OK;
  }
  if (aWindow == mHiddenPrivateWindow) {
    // CreateHiddenWindow() does not register the window, so we're done.
    return NS_OK;
  }

  // tell the window mediator
  nsCOMPtr<nsIWindowMediator> mediator
    ( do_GetService(NS_WINDOWMEDIATOR_CONTRACTID) );
  NS_ASSERTION(mediator, "Couldn't get window mediator. Doing xpcom shutdown?");

  if (mediator)
    mediator->UnregisterWindow(aWindow);

  // tell the window watcher
  nsCOMPtr<nsPIWindowWatcher> wwatcher ( do_GetService(NS_WINDOWWATCHER_CONTRACTID) );
  NS_ASSERTION(wwatcher, "Couldn't get windowwatcher, doing xpcom shutdown?");
  if (wwatcher) {
    nsCOMPtr<nsIDocShell> docShell;
    aWindow->GetDocShell(getter_AddRefs(docShell));
    if (docShell) {
      nsCOMPtr<nsPIDOMWindowOuter> domWindow(docShell->GetWindow());
      if (domWindow)
        wwatcher->RemoveWindow(domWindow);
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsAppShellService::Observe(nsISupports* aSubject, const char *aTopic,
                           const char16_t *aData)
{
  if (!strcmp(aTopic, "xpcom-will-shutdown")) {
    mXPCOMWillShutDown = true;
  } else if (!strcmp(aTopic, "xpcom-shutdown")) {
    mXPCOMShuttingDown = true;
    if (mHiddenWindow) {
      mHiddenWindow->Destroy();
    }
    if (mHiddenPrivateWindow) {
      mHiddenPrivateWindow->Destroy();
    }
  } else {
    NS_ERROR("Unexpected observer topic!");
  }

  return NS_OK;
}

NS_IMETHODIMP
nsAppShellService::StartEventLoopLagTracking(bool* aResult)
{
#ifdef MOZ_INSTRUMENT_EVENT_LOOP
    *aResult = mozilla::InitEventTracing(true);
#endif
    return NS_OK;
}

NS_IMETHODIMP
nsAppShellService::StopEventLoopLagTracking()
{
#ifdef MOZ_INSTRUMENT_EVENT_LOOP
    mozilla::ShutdownEventTracing();
#endif
    return NS_OK;
}
