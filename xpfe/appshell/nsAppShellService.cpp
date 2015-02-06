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

#include "prprf.h"

#include "nsWidgetInitData.h"
#include "nsWidgetsCID.h"
#include "nsIWidget.h"
#include "nsIRequestObserver.h"

/* For implementing GetHiddenWindowAndJSContext */
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"

#include "nsAppShellService.h"
#include "nsISupportsPrimitives.h"
#include "nsIChromeRegistry.h"
#include "nsILoadContext.h"
#include "nsIWebNavigation.h"

#include "mozilla/Attributes.h"
#include "mozilla/Preferences.h"
#include "mozilla/StartupTimeline.h"

#include "nsEmbedCID.h"
#include "nsIWebBrowser.h"
#include "nsIDocShell.h"

#ifdef MOZ_INSTRUMENT_EVENT_LOOP
#include "EventTracer.h"
#endif

using namespace mozilla;

// Default URL for the hidden window, can be overridden by a pref on Mac
#define DEFAULT_HIDDENWINDOW_URL "resource://gre-resources/hiddenWindow.html"

class nsIAppShell;

nsAppShellService::nsAppShellService() : 
  mXPCOMWillShutDown(false),
  mXPCOMShuttingDown(false),
  mModalWindowCount(0),
  mApplicationProvidedHiddenWindow(false)
{
  nsCOMPtr<nsIObserverService> obs
    (do_GetService("@mozilla.org/observer-service;1"));

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

  nsRefPtr<nsWebShellWindow> newWindow;
  if (!aIsPrivate) {
    rv = JustCreateTopWindow(nullptr, url,
                             chromeMask, initialWidth, initialHeight,
                             true, nullptr, getter_AddRefs(newWindow));
    NS_ENSURE_SUCCESS(rv, rv);

    mHiddenWindow.swap(newWindow);
  } else {
    // Create the hidden private window
    chromeMask |= nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW;

    rv = JustCreateTopWindow(nullptr, url,
                             chromeMask, initialWidth, initialHeight,
                             true, nullptr, getter_AddRefs(newWindow));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIDocShell> docShell;
    newWindow->GetDocShell(getter_AddRefs(docShell));
    if (docShell) {
      docShell->SetAffectPrivateSessionLifetime(false);
    }

    mHiddenPrivateWindow.swap(newWindow);
  }

  // RegisterTopLevelWindow(newWindow); -- Mac only

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
                                        nsIXULWindow **aResult)

{
  nsresult rv;

  StartupTimeline::RecordOnce(StartupTimeline::CREATE_TOP_LEVEL_WINDOW);

  nsWebShellWindow *newWindow = nullptr;
  rv = JustCreateTopWindow(aParent, aUrl,
                           aChromeMask, aInitialWidth, aInitialHeight,
                           false, aOpeningTab, &newWindow);  // addrefs

  *aResult = newWindow; // transfer ref

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
                              public nsIInterfaceRequestor,
                              public nsSupportsWeakReference {
protected:
    virtual ~WebBrowserChrome2Stub() {}
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBBROWSERCHROME
    NS_DECL_NSIWEBBROWSERCHROME2
    NS_DECL_NSIINTERFACEREQUESTOR
};

NS_INTERFACE_MAP_BEGIN(WebBrowserChrome2Stub)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebBrowserChrome)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome)
  NS_INTERFACE_MAP_ENTRY(nsIWebBrowserChrome2)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
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
WebBrowserChrome2Stub::GetInterface(const nsIID & aIID, void **aSink)
{
    return QueryInterface(aIID, aSink);
}

// This is the "stub" we return from CreateWindowlessBrowser - it exists
// purely to keep a strong reference to the browser and the container to
// prevent the container being collected while the stub remains alive.
class WindowlessBrowserStub MOZ_FINAL : public nsIWebNavigation,
                                        public nsIInterfaceRequestor {
public:
  WindowlessBrowserStub(nsIWebBrowser *aBrowser, nsISupports *aContainer) {
    mBrowser = aBrowser;
    mWebNavigation = do_QueryInterface(aBrowser);
    mInterfaceRequestor = do_QueryInterface(aBrowser);
    mContainer = aContainer;
  }
  NS_DECL_ISUPPORTS
  NS_FORWARD_NSIWEBNAVIGATION(mWebNavigation->)
  NS_FORWARD_NSIINTERFACEREQUESTOR(mInterfaceRequestor->)
private:
  ~WindowlessBrowserStub() {}
  nsCOMPtr<nsIWebBrowser> mBrowser;
  nsCOMPtr<nsIWebNavigation> mWebNavigation;
  nsCOMPtr<nsIInterfaceRequestor> mInterfaceRequestor;
  // we don't use the container but just hold a reference to it.
  nsCOMPtr<nsISupports> mContainer;
};

NS_INTERFACE_MAP_BEGIN(WindowlessBrowserStub)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIWebNavigation)
  NS_INTERFACE_MAP_ENTRY(nsIWebNavigation)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(WindowlessBrowserStub)
NS_IMPL_RELEASE(WindowlessBrowserStub)


NS_IMETHODIMP
nsAppShellService::CreateWindowlessBrowser(bool aIsChrome, nsIWebNavigation **aResult)
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
  nsRefPtr<WebBrowserChrome2Stub> stub = new WebBrowserChrome2Stub();
  if (!stub) {
    NS_ERROR("Couldn't create instance of WebBrowserChrome2Stub!");
    return NS_ERROR_FAILURE;
  }
  browser->SetContainerWindow(stub);

  nsCOMPtr<nsIWebNavigation> navigation = do_QueryInterface(browser);

  nsCOMPtr<nsIDocShellTreeItem> item = do_QueryInterface(navigation);
  item->SetItemType(aIsChrome ? nsIDocShellTreeItem::typeChromeWrapper
                              : nsIDocShellTreeItem::typeContentWrapper);

  /* A windowless web browser doesn't have an associated OS level window. To
   * accomplish this, we initialize the window associated with our instance of
   * nsWebBrowser with an instance of PuppetWidget, which provides a stub
   * implementation of nsIWidget.
   */
  nsCOMPtr<nsIWidget> widget = nsIWidget::CreatePuppetWidget(nullptr);
  if (!widget) {
    NS_ERROR("Couldn't create instance of PuppetWidget");
    return NS_ERROR_FAILURE;
  }
  widget->Create(nullptr, 0, nsIntRect(nsIntPoint(0, 0), nsIntSize(0, 0)),
                 nullptr);
  nsCOMPtr<nsIBaseWindow> window = do_QueryInterface(navigation);
  window->InitWindow(0, widget, 0, 0, 0, 0);
  window->Create();

  nsISupports *isstub = NS_ISUPPORTS_CAST(nsIWebBrowserChrome2*, stub);
  nsRefPtr<nsIWebNavigation> result = new WindowlessBrowserStub(browser, isstub);
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
                                       nsWebShellWindow **aResult)
{
  *aResult = nullptr;
  NS_ENSURE_STATE(!mXPCOMWillShutDown);

  nsCOMPtr<nsIXULWindow> parent;
  if (aChromeMask & nsIWebBrowserChrome::CHROME_DEPENDENT)
    parent = aParent;

  nsRefPtr<nsWebShellWindow> window = new nsWebShellWindow(aChromeMask);
  NS_ENSURE_TRUE(window, NS_ERROR_OUT_OF_MEMORY);

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

  if (aChromeMask & nsIWebBrowserChrome::CHROME_MAC_SUPPRESS_ANIMATION)
    widgetInitData.mIsAnimationSuppressed = true;

  if (aChromeMask & nsIWebBrowserChrome::CHROME_REMOTE_WINDOW)
    widgetInitData.mRequireOffMainThreadCompositing = true;

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

  nsCOMPtr<nsIXULChromeRegistry> reg =
    mozilla::services::GetXULChromeRegistryService();
  if (reg) {
    nsAutoCString package;
    package.AssignLiteral("global");
    bool isRTL = false;
    reg->IsLocaleRTL(package, &isRTL);
    widgetInitData.mRTL = isRTL;
  }

  nsresult rv = window->Initialize(parent, center ? aParent : nullptr,
                                   aUrl, aInitialWidth, aInitialHeight,
                                   aIsHiddenWindow, aOpeningTab, widgetInitData);

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

  nsCOMPtr<nsIDOMWindow> domWin = do_GetInterface(aParent);
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

  nsCOMPtr<nsIDOMWindow> newDomWin =
      do_GetInterface(NS_ISUPPORTS_CAST(nsIBaseWindow*, window));
  nsCOMPtr<nsIWebNavigation> newWebNav = do_GetInterface(newDomWin);
  nsCOMPtr<nsILoadContext> thisContext = do_GetInterface(newWebNav);
  if (thisContext) {
    thisContext->SetPrivateBrowsing(isPrivateBrowsingWindow);
    thisContext->SetRemoteTabs(isUsingRemoteTabs);
  }

  window.swap(*aResult); // transfer reference
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
nsAppShellService::GetHiddenDOMWindow(nsIDOMWindow **aWindow)
{
  nsresult rv;
  nsCOMPtr<nsIDocShell> docShell;
  NS_ENSURE_TRUE(mHiddenWindow, NS_ERROR_FAILURE);

  rv = mHiddenWindow->GetDocShell(getter_AddRefs(docShell));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMWindow> hiddenDOMWindow(docShell->GetWindow());
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
nsAppShellService::GetHiddenPrivateDOMWindow(nsIDOMWindow **aWindow)
{
  EnsurePrivateHiddenWindow();

  nsresult rv;
  nsCOMPtr<nsIDocShell> docShell;
  NS_ENSURE_TRUE(mHiddenPrivateWindow, NS_ERROR_FAILURE);

  rv = mHiddenPrivateWindow->GetDocShell(getter_AddRefs(docShell));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(docShell, NS_ERROR_FAILURE);

  nsCOMPtr<nsIDOMWindow> hiddenPrivateDOMWindow(docShell->GetWindow());
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
nsAppShellService::GetHiddenWindowAndJSContext(nsIDOMWindow **aWindow,
                                               JSContext    **aJSContext)
{
    nsresult rv = NS_OK;
    if ( aWindow && aJSContext ) {
        *aWindow    = nullptr;
        *aJSContext = nullptr;

        if ( mHiddenWindow ) {
            // Convert hidden window to nsIDOMWindow and extract its JSContext.
            do {
                // 1. Get doc for hidden window.
                nsCOMPtr<nsIDocShell> docShell;
                rv = mHiddenWindow->GetDocShell(getter_AddRefs(docShell));
                if (NS_FAILED(rv)) break;
                if (!docShell) {
                  break;
                }

                // 2. Convert that to an nsIDOMWindow.
                nsCOMPtr<nsIDOMWindow> hiddenDOMWindow(docShell->GetWindow());
                if(!hiddenDOMWindow) break;

                // 3. Get script global object for the window.
                nsCOMPtr<nsIScriptGlobalObject> sgo = docShell->GetScriptGlobalObject();
                if (!sgo) { rv = NS_ERROR_FAILURE; break; }

                // 4. Get script context from that.
                nsIScriptContext *scriptContext = sgo->GetContext();
                if (!scriptContext) { rv = NS_ERROR_FAILURE; break; }

                // 5. Get JSContext from the script context.
                JSContext *jsContext = scriptContext->GetNativeContext();
                if (!jsContext) { rv = NS_ERROR_FAILURE; break; }

                // Now, give results to caller.
                *aWindow    = hiddenDOMWindow.get();
                NS_IF_ADDREF( *aWindow );
                *aJSContext = jsContext;
            } while (0);
        } else {
            rv = NS_ERROR_FAILURE;
        }
    } else {
        rv = NS_ERROR_NULL_POINTER;
    }
    return rv;
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

  nsCOMPtr<nsPIDOMWindow> domWindow(docShell->GetWindow());
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
  nsCOMPtr<nsIObserverService> obssvc =
    do_GetService("@mozilla.org/observer-service;1");
  NS_ASSERTION(obssvc, "Couldn't get observer service.");

  if (obssvc)
    obssvc->NotifyObservers(aWindow, "xul-window-registered", nullptr);

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
      nsCOMPtr<nsIDOMWindow> domWindow(docShell->GetWindow());
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
