/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsIAppShellService.h"
#include "nsISupportsArray.h"
#include "nsIComponentManager.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIServiceManager.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsIXPConnect.h"
#include "nsIJSContextStack.h"

#include "nsIWindowMediator.h"
#include "nsIWindowWatcher.h"
#include "nsPIWindowWatcher.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsWebShellWindow.h"

#include "nsIEnumerator.h"
#include "nsCRT.h"
#include "prprf.h"    

#include "nsWidgetsCID.h"
#include "nsIRequestObserver.h"

/* For implementing GetHiddenWindowAndJSContext */
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "jsapi.h"

#include "nsAppShellService.h"
#include "nsISupportsPrimitives.h"
#include "nsIPlatformCharset.h"
#include "nsICharsetConverterManager.h"
#include "nsIUnicodeDecoder.h"
#include "nsIChromeRegistry.h"
#include "nsILoadContext.h"
#include "nsIWebNavigation.h"

#include "mozilla/Preferences.h"
#include "mozilla/StartupTimeline.h"

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
NS_IMPL_ISUPPORTS2(nsAppShellService,
                   nsIAppShellService,
                   nsIObserver)

NS_IMETHODIMP
nsAppShellService::CreateHiddenWindow()
{
  nsresult rv;
  int32_t initialHeight = 100, initialWidth = 100;

#ifdef XP_MACOSX
  uint32_t    chromeMask = 0;
  nsAdoptingCString prefVal =
      Preferences::GetCString("browser.hiddenWindowChromeURL");
  const char* hiddenWindowURL = prefVal.get() ? prefVal.get() : DEFAULT_HIDDENWINDOW_URL;
  mApplicationProvidedHiddenWindow = prefVal.get() ? true : false;
#else
  static const char hiddenWindowURL[] = DEFAULT_HIDDENWINDOW_URL;
  uint32_t    chromeMask =  nsIWebBrowserChrome::CHROME_ALL;
#endif

  nsCOMPtr<nsIURI> url;
  rv = NS_NewURI(getter_AddRefs(url), hiddenWindowURL);
  NS_ENSURE_SUCCESS(rv, rv);

  nsRefPtr<nsWebShellWindow> newWindow;
  rv = JustCreateTopWindow(nullptr, url,
                           chromeMask, initialWidth, initialHeight,
                           true, getter_AddRefs(newWindow));
  NS_ENSURE_SUCCESS(rv, rv);

  mHiddenWindow.swap(newWindow);

#ifdef MOZ_PER_WINDOW_PRIVATE_BROWSING
  // Create the hidden private window
  chromeMask |= nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW;

  rv = JustCreateTopWindow(nullptr, url,
                           chromeMask, initialWidth, initialHeight,
                           true, getter_AddRefs(newWindow));
  NS_ENSURE_SUCCESS(rv, rv);

  mHiddenPrivateWindow.swap(newWindow);
#endif

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

#ifdef MOZ_PER_WINDOW_PRIVATE_BROWSING
  if (mHiddenPrivateWindow) {
    mHiddenPrivateWindow->Destroy();

    mHiddenPrivateWindow = nullptr;
  }
#endif

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
                                        nsIXULWindow **aResult)

{
  nsresult rv;

  StartupTimeline::RecordOnce(StartupTimeline::CREATE_TOP_LEVEL_WINDOW);

  nsWebShellWindow *newWindow = nullptr;
  rv = JustCreateTopWindow(aParent, aUrl,
                           aChromeMask, aInitialWidth, aInitialHeight,
                           false, &newWindow);  // addrefs

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
      int32_t sizeMode;
      nsCOMPtr<nsIWidget> widget;
      baseWin->GetMainWidget(getter_AddRefs(widget));
      if (widget && NS_SUCCEEDED(widget->GetSizeMode(&sizeMode)) && 
          sizeMode == nsSizeMode_Fullscreen) {
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
  if (parent && ((aChromeMask & sheetMask) == sheetMask))
    widgetInitData.mWindowType = eWindowType_sheet;
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
                                   aIsHiddenWindow, widgetInitData);

  NS_ENSURE_SUCCESS(rv, rv);

  // Enforce the Private Browsing autoStart pref first.
  bool isPrivateBrowsingWindow =
    Preferences::GetBool("browser.privatebrowsing.autostart");
#ifdef MOZ_PER_WINDOW_PRIVATE_BROWSING
  if (aChromeMask & nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW) {
    // Caller requested a private window
    isPrivateBrowsingWindow = true;
  }
#endif
  if (!isPrivateBrowsingWindow) {
    // Ensure that we propagate any existing private browsing status
    // from the parent, even if it will not actually be used
    // as a parent value.
    nsCOMPtr<nsIDOMWindow> domWin = do_GetInterface(aParent);
    nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(domWin);
    nsCOMPtr<nsILoadContext> parentContext = do_QueryInterface(webNav);
    if (parentContext) {
      isPrivateBrowsingWindow = parentContext->UsePrivateBrowsing();
    }
  }
  nsCOMPtr<nsIDOMWindow> newDomWin =
      do_GetInterface(NS_ISUPPORTS_CAST(nsIBaseWindow*, window));
  nsCOMPtr<nsIWebNavigation> newWebNav = do_GetInterface(newDomWin);
  nsCOMPtr<nsILoadContext> thisContext = do_GetInterface(newWebNav);
  if (thisContext) {
    thisContext->SetPrivateBrowsing(isPrivateBrowsingWindow);
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

  nsCOMPtr<nsIDOMWindow> hiddenDOMWindow(do_GetInterface(docShell, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  *aWindow = hiddenDOMWindow;
  NS_IF_ADDREF(*aWindow);
  return NS_OK;
}

NS_IMETHODIMP
nsAppShellService::GetHiddenPrivateWindow(nsIXULWindow **aWindow)
{
#ifdef MOZ_PER_WINDOW_PRIVATE_BROWSING
  NS_ENSURE_ARG_POINTER(aWindow);

  *aWindow = mHiddenPrivateWindow;
  NS_IF_ADDREF(*aWindow);
  return *aWindow ? NS_OK : NS_ERROR_FAILURE;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_IMETHODIMP
nsAppShellService::GetHiddenPrivateDOMWindow(nsIDOMWindow **aWindow)
{
#ifdef MOZ_PER_WINDOW_PRIVATE_BROWSING
  nsresult rv;
  nsCOMPtr<nsIDocShell> docShell;
  NS_ENSURE_TRUE(mHiddenPrivateWindow, NS_ERROR_FAILURE);

  rv = mHiddenPrivateWindow->GetDocShell(getter_AddRefs(docShell));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIDOMWindow> hiddenPrivateDOMWindow(do_GetInterface(docShell, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  *aWindow = hiddenPrivateDOMWindow;
  NS_IF_ADDREF(*aWindow);
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
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

                // 2. Convert that to an nsIDOMWindow.
                nsCOMPtr<nsIDOMWindow> hiddenDOMWindow(do_GetInterface(docShell));
                if(!hiddenDOMWindow) break;

                // 3. Get script global object for the window.
                nsCOMPtr<nsIScriptGlobalObject> sgo;
                sgo = do_QueryInterface( hiddenDOMWindow );
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
  nsCOMPtr<nsPIDOMWindow> domWindow(do_GetInterface(docShell));
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
#ifdef MOZ_PER_WINDOW_PRIVATE_BROWSING
  if (aWindow == mHiddenPrivateWindow) {
    // CreateHiddenWindow() does not register the window, so we're done.
    return NS_OK;
  }
#endif

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
      nsCOMPtr<nsIDOMWindow> domWindow(do_GetInterface(docShell));
      if (domWindow)
        wwatcher->RemoveWindow(domWindow);
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsAppShellService::Observe(nsISupports* aSubject, const char *aTopic,
                           const PRUnichar *aData)
{
  if (!strcmp(aTopic, "xpcom-will-shutdown")) {
    mXPCOMWillShutDown = true;
  } else if (!strcmp(aTopic, "xpcom-shutdown")) {
    mXPCOMShuttingDown = true;
    if (mHiddenWindow) {
      mHiddenWindow->Destroy();
    }
#ifdef MOZ_PER_WINDOW_PRIVATE_BROWSING
    if (mHiddenPrivateWindow) {
      mHiddenPrivateWindow->Destroy();
    }
#endif
  } else {
    NS_ERROR("Unexpected observer topic!");
  }

  return NS_OK;
}
