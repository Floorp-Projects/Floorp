/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MathAlgorithms.h"

// Local includes
#include "nsXULWindow.h"
#include <algorithm>

// Helper classes
#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsWidgetsCID.h"
#include "nsThreadUtils.h"
#include "nsNetCID.h"
#include "nsQueryObject.h"
#include "mozilla/Sprintf.h"

//Interfaces needed to be included
#include "nsGlobalWindowOuter.h"
#include "nsIAppShell.h"
#include "nsIAppShellService.h"
#include "nsIServiceManager.h"
#include "nsIContentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsPIDOMWindow.h"
#include "nsScreen.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIIOService.h"
#include "nsILoadContext.h"
#include "nsIObserverService.h"
#include "nsIWindowMediator.h"
#include "nsIScreenManager.h"
#include "nsIScreen.h"
#include "nsIScrollable.h"
#include "nsIScriptSecurityManager.h"
#include "nsIWindowWatcher.h"
#include "nsIURI.h"
#include "nsAppShellCID.h"
#include "nsReadableUtils.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsContentUtils.h"
#include "nsWebShellWindow.h" // get rid of this one, too...
#include "nsGlobalWindow.h"
#include "XULDocument.h"

#include "prenv.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/Preferences.h"
#include "mozilla/Services.h"
#include "mozilla/dom/BarProps.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/TabParent.h"

using namespace mozilla;
using dom::AutoNoJSAPI;

#define SIZEMODE_NORMAL     NS_LITERAL_STRING("normal")
#define SIZEMODE_MAXIMIZED  NS_LITERAL_STRING("maximized")
#define SIZEMODE_MINIMIZED  NS_LITERAL_STRING("minimized")
#define SIZEMODE_FULLSCREEN NS_LITERAL_STRING("fullscreen")

#define WINDOWTYPE_ATTRIBUTE NS_LITERAL_STRING("windowtype")

#define PERSIST_ATTRIBUTE  NS_LITERAL_STRING("persist")
#define SCREENX_ATTRIBUTE  NS_LITERAL_STRING("screenX")
#define SCREENY_ATTRIBUTE  NS_LITERAL_STRING("screenY")
#define WIDTH_ATTRIBUTE    NS_LITERAL_STRING("width")
#define HEIGHT_ATTRIBUTE   NS_LITERAL_STRING("height")
#define MODE_ATTRIBUTE     NS_LITERAL_STRING("sizemode")
#define ZLEVEL_ATTRIBUTE   NS_LITERAL_STRING("zlevel")


//*****************************************************************************
//***    nsXULWindow: Object Management
//*****************************************************************************

nsXULWindow::nsXULWindow(uint32_t aChromeFlags)
  : mChromeTreeOwner(nullptr),
    mContentTreeOwner(nullptr),
    mPrimaryContentTreeOwner(nullptr),
    mModalStatus(NS_OK),
    mContinueModalLoop(false),
    mDebuting(false),
    mChromeLoaded(false),
    mSizingShellFromXUL(false),
    mShowAfterLoad(false),
    mIntrinsicallySized(false),
    mCenterAfterLoad(false),
    mIsHiddenWindow(false),
    mLockedUntilChromeLoad(false),
    mIgnoreXULSize(false),
    mIgnoreXULPosition(false),
    mChromeFlagsFrozen(false),
    mIgnoreXULSizeMode(false),
    mDestroying(false),
    mRegistered(false),
    mPersistentAttributesDirty(0),
    mPersistentAttributesMask(0),
    mChromeFlags(aChromeFlags),
    mNextTabParentId(0)
{
}

nsXULWindow::~nsXULWindow()
{
  Destroy();
}

//*****************************************************************************
// nsXULWindow::nsISupports
//*****************************************************************************

NS_IMPL_ADDREF(nsXULWindow)
NS_IMPL_RELEASE(nsXULWindow)

NS_INTERFACE_MAP_BEGIN(nsXULWindow)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIXULWindow)
  NS_INTERFACE_MAP_ENTRY(nsIXULWindow)
  NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  if (aIID.Equals(NS_GET_IID(nsXULWindow)))
    foundInterface = reinterpret_cast<nsISupports*>(this);
  else
NS_INTERFACE_MAP_END

//*****************************************************************************
// nsXULWindow::nsIIntefaceRequestor
//*****************************************************************************

NS_IMETHODIMP nsXULWindow::GetInterface(const nsIID& aIID, void** aSink)
{
  nsresult rv;

  NS_ENSURE_ARG_POINTER(aSink);

  if (aIID.Equals(NS_GET_IID(nsIPrompt))) {
    rv = EnsurePrompter();
    if (NS_FAILED(rv)) return rv;
    return mPrompter->QueryInterface(aIID, aSink);
  }
  if (aIID.Equals(NS_GET_IID(nsIAuthPrompt))) {
    rv = EnsureAuthPrompter();
    if (NS_FAILED(rv)) return rv;
    return mAuthPrompter->QueryInterface(aIID, aSink);
  }
  if (aIID.Equals(NS_GET_IID(mozIDOMWindowProxy))) {
    return GetWindowDOMWindow(reinterpret_cast<mozIDOMWindowProxy**>(aSink));
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMWindow))) {
    nsCOMPtr<mozIDOMWindowProxy> window = nullptr;
    rv = GetWindowDOMWindow(getter_AddRefs(window));
    nsCOMPtr<nsIDOMWindow> domWindow = do_QueryInterface(window);
    domWindow.forget(aSink);
    return rv;
  }
  if (aIID.Equals(NS_GET_IID(nsIWebBrowserChrome)) &&
    NS_SUCCEEDED(EnsureContentTreeOwner()) &&
    NS_SUCCEEDED(mContentTreeOwner->QueryInterface(aIID, aSink)))
    return NS_OK;

  if (aIID.Equals(NS_GET_IID(nsIEmbeddingSiteWindow)) &&
    NS_SUCCEEDED(EnsureContentTreeOwner()) &&
    NS_SUCCEEDED(mContentTreeOwner->QueryInterface(aIID, aSink)))
    return NS_OK;

  return QueryInterface(aIID, aSink);
}

//*****************************************************************************
// nsXULWindow::nsIXULWindow
//*****************************************************************************

NS_IMETHODIMP nsXULWindow::GetDocShell(nsIDocShell** aDocShell)
{
  NS_ENSURE_ARG_POINTER(aDocShell);

  *aDocShell = mDocShell;
  NS_IF_ADDREF(*aDocShell);
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetZLevel(uint32_t *outLevel)
{
  nsCOMPtr<nsIWindowMediator> mediator(do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (mediator)
    mediator->GetZLevel(this, outLevel);
  else
    *outLevel = normalZ;
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetZLevel(uint32_t aLevel)
{
  nsCOMPtr<nsIWindowMediator> mediator(do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (!mediator)
    return NS_ERROR_FAILURE;

  uint32_t zLevel;
  mediator->GetZLevel(this, &zLevel);
  if (zLevel == aLevel)
    return NS_OK;

  /* refuse to raise a maximized window above the normal browser level,
     for fear it could hide newly opened browser windows */
  if (aLevel > nsIXULWindow::normalZ && mWindow) {
    nsSizeMode sizeMode = mWindow->SizeMode();
    if (sizeMode == nsSizeMode_Maximized || sizeMode == nsSizeMode_Fullscreen) {
      return NS_ERROR_FAILURE;
    }
  }

  // do it
  mediator->SetZLevel(this, aLevel);
  PersistentAttributesDirty(PAD_MISC);
  SavePersistentAttributes();

  nsCOMPtr<nsIContentViewer> cv;
  mDocShell->GetContentViewer(getter_AddRefs(cv));
  if (cv) {
    nsCOMPtr<nsIDocument> doc = cv->GetDocument();
    if (doc) {
      ErrorResult rv;
      RefPtr<dom::Event> event =
        doc->CreateEvent(NS_LITERAL_STRING("Events"), dom::CallerType::System,
                         rv);
      if (event) {
        event->InitEvent(NS_LITERAL_STRING("windowZLevel"), true, false);

        event->SetTrusted(true);

        doc->DispatchEvent(*event);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetChromeFlags(uint32_t *aChromeFlags)
{
  NS_ENSURE_ARG_POINTER(aChromeFlags);
  *aChromeFlags = mChromeFlags;
  /* mChromeFlags is kept up to date, except for scrollbar visibility.
     That can be changed directly by the content DOM window, which
     doesn't know to update the chrome window. So that we must check
     separately. */

  // however, it's pointless to ask if the window isn't set up yet
  if (!mChromeLoaded)
    return NS_OK;

  if (GetContentScrollbarVisibility())
    *aChromeFlags |= nsIWebBrowserChrome::CHROME_SCROLLBARS;
  else
    *aChromeFlags &= ~nsIWebBrowserChrome::CHROME_SCROLLBARS;

  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetChromeFlags(uint32_t aChromeFlags)
{
  NS_ASSERTION(!mChromeFlagsFrozen,
               "SetChromeFlags() after AssumeChromeFlagsAreFrozen()!");

  mChromeFlags = aChromeFlags;
  if (mChromeLoaded) {
    ApplyChromeFlags();
  }
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::AssumeChromeFlagsAreFrozen()
{
  mChromeFlagsFrozen = true;
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetIntrinsicallySized(bool aIntrinsicallySized)
{
  mIntrinsicallySized = aIntrinsicallySized;
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetIntrinsicallySized(bool* aIntrinsicallySized)
{
  NS_ENSURE_ARG_POINTER(aIntrinsicallySized);

  *aIntrinsicallySized = mIntrinsicallySized;
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetPrimaryContentShell(nsIDocShellTreeItem**
   aDocShellTreeItem)
{
  NS_ENSURE_ARG_POINTER(aDocShellTreeItem);
  NS_IF_ADDREF(*aDocShellTreeItem = mPrimaryContentShell);
  return NS_OK;
}

NS_IMETHODIMP
nsXULWindow::TabParentAdded(nsITabParent* aTab, bool aPrimary)
{
  if (aPrimary) {
    mPrimaryTabParent = aTab;
    mPrimaryContentShell = nullptr;
  } else if (mPrimaryTabParent == aTab) {
    mPrimaryTabParent = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULWindow::TabParentRemoved(nsITabParent* aTab)
{
  if (aTab == mPrimaryTabParent) {
    mPrimaryTabParent = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULWindow::GetPrimaryTabParent(nsITabParent** aTab)
{
  nsCOMPtr<nsITabParent> tab = mPrimaryTabParent;
  tab.forget(aTab);
  return NS_OK;
}

static LayoutDeviceIntSize
GetOuterToInnerSizeDifference(nsIWidget* aWindow)
{
  if (!aWindow) {
    return LayoutDeviceIntSize();
  }
  LayoutDeviceIntSize baseSize(200, 200);
  LayoutDeviceIntSize windowSize = aWindow->ClientToWindowSize(baseSize);
  return windowSize - baseSize;
}

static CSSIntSize
GetOuterToInnerSizeDifferenceInCSSPixels(nsIWidget* aWindow)
{
  if (!aWindow) {
    return { };
  }
  LayoutDeviceIntSize devPixelSize = GetOuterToInnerSizeDifference(aWindow);
  return RoundedToInt(devPixelSize / aWindow->GetDefaultScale());
}

NS_IMETHODIMP
nsXULWindow::GetOuterToInnerHeightDifferenceInCSSPixels(uint32_t* aResult)
{
  *aResult = GetOuterToInnerSizeDifferenceInCSSPixels(mWindow).height;
  return NS_OK;
}

NS_IMETHODIMP
nsXULWindow::GetOuterToInnerWidthDifferenceInCSSPixels(uint32_t* aResult)
{
  *aResult = GetOuterToInnerSizeDifferenceInCSSPixels(mWindow).width;
  return NS_OK;
}

nsTArray<RefPtr<mozilla::LiveResizeListener>>
nsXULWindow::GetLiveResizeListeners()
{
  nsTArray<RefPtr<mozilla::LiveResizeListener>> listeners;
  if (mPrimaryTabParent) {
    TabParent* parent = static_cast<TabParent*>(mPrimaryTabParent.get());
    listeners.AppendElement(parent);
  }
  return listeners;
}

NS_IMETHODIMP nsXULWindow::AddChildWindow(nsIXULWindow *aChild)
{
  // we're not really keeping track of this right now
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::RemoveChildWindow(nsIXULWindow *aChild)
{
  // we're not really keeping track of this right now
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::ShowModal()
{
  AUTO_PROFILER_LABEL("nsXULWindow::ShowModal", OTHER);

  // Store locally so it doesn't die on us
  nsCOMPtr<nsIWidget> window = mWindow;
  nsCOMPtr<nsIXULWindow> tempRef = this;

  window->SetModal(true);
  mContinueModalLoop = true;
  EnableParent(false);

  {
    AutoNoJSAPI nojsapi;
    SpinEventLoopUntil([&]() { return !mContinueModalLoop; });
  }

  mContinueModalLoop = false;
  window->SetModal(false);
  /*   Note there's no EnableParent(true) here to match the false one
     above. That's done in ExitModalLoop. It's important that the parent
     be re-enabled before this window is made invisible; to do otherwise
     causes bizarre z-ordering problems. At this point, the window is
     already invisible.
       No known current implementation of Enable would have a problem with
     re-enabling the parent twice, so we could do it again here without
     breaking any current implementation. But that's unnecessary if the
     modal loop is always exited using ExitModalLoop (the other way would be
     to change the protected member variable directly.)
  */

  return mModalStatus;
}

//*****************************************************************************
// nsXULWindow::nsIBaseWindow
//*****************************************************************************

NS_IMETHODIMP nsXULWindow::InitWindow(nativeWindow aParentNativeWindow,
   nsIWidget* parentWidget, int32_t x, int32_t y, int32_t cx, int32_t cy)
{
  //XXX First Check In
  NS_ASSERTION(false, "Not Yet Implemented");
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::Create()
{
  //XXX First Check In
  NS_ASSERTION(false, "Not Yet Implemented");
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::Destroy()
{
  if (!mWindow)
     return NS_OK;

  // Ensure we don't reenter this code
  if (mDestroying)
    return NS_OK;

  mozilla::AutoRestore<bool> guard(mDestroying);
  mDestroying = true;

  nsCOMPtr<nsIAppShellService> appShell(do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
  NS_ASSERTION(appShell, "Couldn't get appShell... xpcom shutdown?");
  if (appShell)
    appShell->UnregisterTopLevelWindow(static_cast<nsIXULWindow*>(this));

  nsCOMPtr<nsIXULWindow> parentWindow(do_QueryReferent(mParentWindow));
  if (parentWindow)
    parentWindow->RemoveChildWindow(this);

  // let's make sure the window doesn't get deleted out from under us
  // while we are trying to close....this can happen if the docshell
  // we close ends up being the last owning reference to this xulwindow

  // XXXTAB This shouldn't be an issue anymore because the ownership model
  // only goes in one direction.  When webshell container is fully removed
  // try removing this...

  nsCOMPtr<nsIXULWindow> placeHolder = this;

  // Remove modality (if any) and hide while destroying. More than
  // a convenience, the hide prevents user interaction with the partially
  // destroyed window. This is especially necessary when the eldest window
  // in a stack of modal windows is destroyed first. It happens.
  ExitModalLoop(NS_OK);
  // XXX: Skip unmapping the window on Linux due to GLX hangs on the compositor
  // thread with NVIDIA driver 310.32. We don't need to worry about user
  // interactions with destroyed windows on X11 either.
#ifndef MOZ_WIDGET_GTK
  if (mWindow)
    mWindow->Show(false);
#endif

#if defined(XP_WIN)
  // We need to explicitly set the focus on Windows, but
  // only if the parent is visible.
  nsCOMPtr<nsIBaseWindow> parent(do_QueryReferent(mParentWindow));
  if (parent) {
    nsCOMPtr<nsIWidget> parentWidget;
    parent->GetMainWidget(getter_AddRefs(parentWidget));
    if (!parentWidget || parentWidget->IsVisible()) {
      nsCOMPtr<nsIBaseWindow> baseHiddenWindow;
      if (appShell) {
        nsCOMPtr<nsIXULWindow> hiddenWindow;
        appShell->GetHiddenWindow(getter_AddRefs(hiddenWindow));
        if (hiddenWindow)
          baseHiddenWindow = do_GetInterface(hiddenWindow);
      }
      // somebody screwed up somewhere. hiddenwindow shouldn't be anybody's
      // parent. still, when it happens, skip activating it.
      if (baseHiddenWindow != parent) {
        nsCOMPtr<nsIWidget> parentWidget;
        parent->GetMainWidget(getter_AddRefs(parentWidget));
        if (parentWidget)
          parentWidget->PlaceBehind(eZPlacementTop, 0, true);
      }
    }
  }
#endif

  mDOMWindow = nullptr;
  if (mDocShell) {
    nsCOMPtr<nsIBaseWindow> shellAsWin(do_QueryInterface(mDocShell));
    shellAsWin->Destroy();
    mDocShell = nullptr; // this can cause reentrancy of this function
  }

  mPrimaryContentShell = nullptr;

  if (mContentTreeOwner) {
    mContentTreeOwner->XULWindow(nullptr);
    NS_RELEASE(mContentTreeOwner);
  }
  if (mPrimaryContentTreeOwner) {
    mPrimaryContentTreeOwner->XULWindow(nullptr);
    NS_RELEASE(mPrimaryContentTreeOwner);
  }
  if (mChromeTreeOwner) {
    mChromeTreeOwner->XULWindow(nullptr);
    NS_RELEASE(mChromeTreeOwner);
  }
  if (mWindow) {
    mWindow->SetWidgetListener(nullptr); // nsWebShellWindow hackery
    mWindow->Destroy();
    mWindow = nullptr;
  }

  if (!mIsHiddenWindow && mRegistered) {
    /* Inform appstartup we've destroyed this window and it could
       quit now if it wanted. This must happen at least after mDocShell
       is destroyed, because onunload handlers fire then, and those being
       script, anything could happen. A new window could open, even.
       See bug 130719. */
    nsCOMPtr<nsIObserverService> obssvc = services::GetObserverService();
    NS_ASSERTION(obssvc, "Couldn't get observer service?");

    if (obssvc)
      obssvc->NotifyObservers(nullptr, "xul-window-destroyed", nullptr);
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetDevicePixelsPerDesktopPixel(double *aScale)
{
  *aScale = mWindow ? mWindow->GetDesktopToDeviceScale().scale : 1.0;
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetUnscaledDevicePixelsPerCSSPixel(double *aScale)
{
  *aScale = mWindow ? mWindow->GetDefaultScale().scale : 1.0;
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetPositionDesktopPix(int32_t aX, int32_t aY)
{
  mWindow->Move(aX, aY);
  if (mSizingShellFromXUL) {
    // If we're invoked for sizing from XUL, we want to neither ignore anything
    // nor persist anything, since it's already the value in XUL.
    return NS_OK;
  }
  if (!mChromeLoaded) {
    // If we're called before the chrome is loaded someone obviously wants this
    // window at this position. We don't persist this one-time position.
    mIgnoreXULPosition = true;
    return NS_OK;
  }
  PersistentAttributesDirty(PAD_POSITION);
  SavePersistentAttributes();
  return NS_OK;
}

// The parameters here are device pixels; do the best we can to convert to
// desktop px, using the window's current scale factor (if available).
NS_IMETHODIMP nsXULWindow::SetPosition(int32_t aX, int32_t aY)
{
  // Don't reset the window's size mode here - platforms that don't want to move
  // maximized windows should reset it in their respective Move implementation.
  DesktopToLayoutDeviceScale currScale = mWindow->GetDesktopToDeviceScale();
  DesktopPoint pos = LayoutDeviceIntPoint(aX, aY) / currScale;
  return SetPositionDesktopPix(pos.x, pos.y);
}

NS_IMETHODIMP nsXULWindow::GetPosition(int32_t* aX, int32_t* aY)
{
  return GetPositionAndSize(aX, aY, nullptr, nullptr);
}

NS_IMETHODIMP nsXULWindow::SetSize(int32_t aCX, int32_t aCY, bool aRepaint)
{
  /* any attempt to set the window's size or position overrides the window's
     zoom state. this is important when these two states are competing while
     the window is being opened. but it should probably just always be so. */
  mWindow->SetSizeMode(nsSizeMode_Normal);

  mIntrinsicallySized = false;

  DesktopToLayoutDeviceScale scale = mWindow->GetDesktopToDeviceScale();
  DesktopSize size = LayoutDeviceIntSize(aCX, aCY) / scale;
  mWindow->Resize(size.width, size.height, aRepaint);
  if (mSizingShellFromXUL) {
    // If we're invoked for sizing from XUL, we want to neither ignore anything
    // nor persist anything, since it's already the value in XUL.
    return NS_OK;
  }
  if (!mChromeLoaded) {
    // If we're called before the chrome is loaded someone obviously wants this
    // window at this size & in the normal size mode (since it is the only mode
    // in which setting dimensions makes sense). We don't persist this one-time
    // size.
    mIgnoreXULSize = true;
    mIgnoreXULSizeMode = true;
    return NS_OK;
  }
  PersistentAttributesDirty(PAD_SIZE);
  SavePersistentAttributes();
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetSize(int32_t* aCX, int32_t* aCY)
{
  return GetPositionAndSize(nullptr, nullptr, aCX, aCY);
}

NS_IMETHODIMP nsXULWindow::SetPositionAndSize(int32_t aX, int32_t aY,
   int32_t aCX, int32_t aCY, uint32_t aFlags)
{
  /* any attempt to set the window's size or position overrides the window's
     zoom state. this is important when these two states are competing while
     the window is being opened. but it should probably just always be so. */
  mWindow->SetSizeMode(nsSizeMode_Normal);

  mIntrinsicallySized = false;

  DesktopToLayoutDeviceScale scale = mWindow->GetDesktopToDeviceScale();
  DesktopRect rect = LayoutDeviceIntRect(aX, aY, aCX, aCY) / scale;
  mWindow->Resize(rect.X(), rect.Y(), rect.Width(), rect.Height(),
                  !!(aFlags & nsIBaseWindow::eRepaint));
  if (mSizingShellFromXUL) {
    // If we're invoked for sizing from XUL, we want to neither ignore anything
    // nor persist anything, since it's already the value in XUL.
    return NS_OK;
  }
  if (!mChromeLoaded) {
    // If we're called before the chrome is loaded someone obviously wants this
    // window at this size and position. We don't persist this one-time setting.
    mIgnoreXULPosition = true;
    mIgnoreXULSize = true;
    mIgnoreXULSizeMode = true;
    return NS_OK;
  }
  PersistentAttributesDirty(PAD_POSITION | PAD_SIZE);
  SavePersistentAttributes();
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetPositionAndSize(int32_t* x, int32_t* y, int32_t* cx,
   int32_t* cy)
{

  if (!mWindow)
    return NS_ERROR_FAILURE;

  LayoutDeviceIntRect rect = mWindow->GetScreenBounds();

  if (x)
    *x = rect.X();
  if (y)
    *y = rect.Y();
  if (cx)
    *cx = rect.Width();
  if (cy)
    *cy = rect.Height();

  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::Center(nsIXULWindow *aRelative, bool aScreen, bool aAlert)
{
  int32_t  left, top, width, height,
           ourWidth, ourHeight;
  bool     screenCoordinates =  false,
           windowCoordinates =  false;
  nsresult result;

  if (!mChromeLoaded) {
    // note we lose the parameters. at time of writing, this isn't a problem.
    mCenterAfterLoad = true;
    return NS_OK;
  }

  if (!aScreen && !aRelative)
    return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIScreenManager> screenmgr = do_GetService("@mozilla.org/gfx/screenmanager;1", &result);
  if (NS_FAILED(result))
    return result;

  nsCOMPtr<nsIScreen> screen;

  if (aRelative) {
    nsCOMPtr<nsIBaseWindow> base(do_QueryInterface(aRelative, &result));
    if (base) {
      // get window rect
      result = base->GetPositionAndSize(&left, &top, &width, &height);
      if (NS_SUCCEEDED(result)) {
        double scale;
        if (NS_SUCCEEDED(base->GetDevicePixelsPerDesktopPixel(&scale))) {
          left = NSToIntRound(left / scale);
          top = NSToIntRound(top / scale);
          width = NSToIntRound(width / scale);
          height = NSToIntRound(height / scale);
        }
        // if centering on screen, convert that to the corresponding screen
        if (aScreen)
          screenmgr->ScreenForRect(left, top, width, height, getter_AddRefs(screen));
        else
          windowCoordinates = true;
      } else {
        // something's wrong with the reference window.
        // fall back to the primary screen
        aRelative = 0;
        aScreen = true;
      }
    }
  }
  if (!aRelative) {
    if (!mOpenerScreenRect.IsEmpty()) {
      // FIXME - check if these are device or display pixels
      screenmgr->ScreenForRect(mOpenerScreenRect.X(), mOpenerScreenRect.Y(),
                               mOpenerScreenRect.Width(), mOpenerScreenRect.Height(),
                               getter_AddRefs(screen));
    } else {
      screenmgr->GetPrimaryScreen(getter_AddRefs(screen));
    }
  }

  if (aScreen && screen) {
    screen->GetAvailRectDisplayPix(&left, &top, &width, &height);
    screenCoordinates = true;
  }

  if (screenCoordinates || windowCoordinates) {
    NS_ASSERTION(mWindow, "what, no window?");
    double scale = mWindow->GetDesktopToDeviceScale().scale;
    GetSize(&ourWidth, &ourHeight);
    int32_t scaledWidth, scaledHeight;
    scaledWidth = NSToIntRound(ourWidth / scale);
    scaledHeight = NSToIntRound(ourHeight / scale);
    left += (width - scaledWidth) / 2;
    top += (height - scaledHeight) / (aAlert ? 3 : 2);
    if (windowCoordinates) {
      mWindow->ConstrainPosition(false, &left, &top);
    }
    SetPosition(left * scale, top * scale);

    // If moving the window caused it to change size,
    // re-do the centering.
    int32_t newWidth, newHeight;
    GetSize(&newWidth, &newHeight);
    if (newWidth != ourWidth || newHeight != ourHeight) {
      return Center(aRelative, aScreen, aAlert);
    }
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsXULWindow::Repaint(bool aForce)
{
  //XXX First Check In
  NS_ASSERTION(false, "Not Yet Implemented");
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetParentWidget(nsIWidget** aParentWidget)
{
  NS_ENSURE_ARG_POINTER(aParentWidget);
  NS_ENSURE_STATE(mWindow);

  NS_IF_ADDREF(*aParentWidget = mWindow->GetParent());
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetParentWidget(nsIWidget* aParentWidget)
{
  //XXX First Check In
  NS_ASSERTION(false, "Not Yet Implemented");
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetParentNativeWindow(nativeWindow* aParentNativeWindow)
{
  NS_ENSURE_ARG_POINTER(aParentNativeWindow);

  nsCOMPtr<nsIWidget> parentWidget;
  NS_ENSURE_SUCCESS(GetParentWidget(getter_AddRefs(parentWidget)), NS_ERROR_FAILURE);

  if (parentWidget) {
    *aParentNativeWindow = parentWidget->GetNativeData(NS_NATIVE_WIDGET);
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetParentNativeWindow(nativeWindow aParentNativeWindow)
{
  //XXX First Check In
  NS_ASSERTION(false, "Not Yet Implemented");
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetNativeHandle(nsAString& aNativeHandle)
{
  nsCOMPtr<nsIWidget> mainWidget;
  NS_ENSURE_SUCCESS(GetMainWidget(getter_AddRefs(mainWidget)), NS_ERROR_FAILURE);

  if (mainWidget) {
    nativeWindow nativeWindowPtr = mainWidget->GetNativeData(NS_NATIVE_WINDOW);
    /* the nativeWindow pointer is converted to and exposed as a string. This
       is a more reliable way not to lose information (as opposed to JS
       |Number| for instance) */
    aNativeHandle = NS_ConvertASCIItoUTF16(nsPrintfCString("0x%p", nativeWindowPtr));
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetVisibility(bool* aVisibility)
{
  NS_ENSURE_ARG_POINTER(aVisibility);

  // Always claim to be visible for now. See bug
  // https://bugzilla.mozilla.org/show_bug.cgi?id=306245.

  *aVisibility = true;

  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetVisibility(bool aVisibility)
{
  if (!mChromeLoaded) {
    mShowAfterLoad = aVisibility;
    return NS_OK;
  }

  if (mDebuting) {
    return NS_OK;
  }
  mDebuting = true;  // (Show / Focus is recursive)

  //XXXTAB Do we really need to show docshell and the window?  Isn't
  // the window good enough?
  nsCOMPtr<nsIBaseWindow> shellAsWin(do_QueryInterface(mDocShell));
  shellAsWin->SetVisibility(aVisibility);
  // Store locally so it doesn't die on us. 'Show' can result in the window
  // being closed with nsXULWindow::Destroy being called. That would set
  // mWindow to null and posibly destroy the nsIWidget while its Show method
  // is on the stack. We need to keep it alive until Show finishes.
  nsCOMPtr<nsIWidget> window = mWindow;
  window->Show(aVisibility);

  nsCOMPtr<nsIWindowMediator> windowMediator(do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (windowMediator)
     windowMediator->UpdateWindowTimeStamp(static_cast<nsIXULWindow*>(this));

  // notify observers so that we can hide the splash screen if possible
  nsCOMPtr<nsIObserverService> obssvc = services::GetObserverService();
  NS_ASSERTION(obssvc, "Couldn't get observer service.");
  if (obssvc) {
    obssvc->NotifyObservers(nullptr, "xul-window-visible", nullptr);
  }

  mDebuting = false;
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetEnabled(bool *aEnabled)
{
  NS_ENSURE_ARG_POINTER(aEnabled);

  if (mWindow) {
    *aEnabled = mWindow->IsEnabled();
    return NS_OK;
  }

  *aEnabled = true; // better guess than most
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsXULWindow::SetEnabled(bool aEnable)
{
  if (mWindow) {
    mWindow->Enable(aEnable);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsXULWindow::GetMainWidget(nsIWidget** aMainWidget)
{
  NS_ENSURE_ARG_POINTER(aMainWidget);

  *aMainWidget = mWindow;
  NS_IF_ADDREF(*aMainWidget);
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetFocus()
{
  //XXX First Check In
  NS_ASSERTION(false, "Not Yet Implemented");
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetTitle(nsAString& aTitle)
{
  aTitle = mTitle;
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetTitle(const nsAString& aTitle)
{
  NS_ENSURE_STATE(mWindow);
  mTitle.Assign(aTitle);
  mTitle.StripCRLF();
  NS_ENSURE_SUCCESS(mWindow->SetTitle(mTitle), NS_ERROR_FAILURE);
  return NS_OK;
}


//*****************************************************************************
// nsXULWindow: Helpers
//*****************************************************************************

NS_IMETHODIMP nsXULWindow::EnsureChromeTreeOwner()
{
  if (mChromeTreeOwner)
    return NS_OK;

  mChromeTreeOwner = new nsChromeTreeOwner();
  NS_ADDREF(mChromeTreeOwner);
  mChromeTreeOwner->XULWindow(this);

  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::EnsureContentTreeOwner()
{
  if (mContentTreeOwner)
    return NS_OK;

  mContentTreeOwner = new nsContentTreeOwner(false);
  NS_ADDREF(mContentTreeOwner);
  mContentTreeOwner->XULWindow(this);

  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::EnsurePrimaryContentTreeOwner()
{
  if (mPrimaryContentTreeOwner)
    return NS_OK;

  mPrimaryContentTreeOwner = new nsContentTreeOwner(true);
  NS_ADDREF(mPrimaryContentTreeOwner);
  mPrimaryContentTreeOwner->XULWindow(this);

  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::EnsurePrompter()
{
  if (mPrompter)
    return NS_OK;

  nsCOMPtr<mozIDOMWindowProxy> ourWindow;
  nsresult rv = GetWindowDOMWindow(getter_AddRefs(ourWindow));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIWindowWatcher> wwatch =
        do_GetService(NS_WINDOWWATCHER_CONTRACTID);
    if (wwatch)
      wwatch->GetNewPrompter(ourWindow, getter_AddRefs(mPrompter));
  }
  return mPrompter ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsXULWindow::EnsureAuthPrompter()
{
  if (mAuthPrompter)
    return NS_OK;

  nsCOMPtr<mozIDOMWindowProxy> ourWindow;
  nsresult rv = GetWindowDOMWindow(getter_AddRefs(ourWindow));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
    if (wwatch)
      wwatch->GetNewAuthPrompter(ourWindow, getter_AddRefs(mAuthPrompter));
  }
  return mAuthPrompter ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsXULWindow::GetAvailScreenSize(int32_t* aAvailWidth, int32_t* aAvailHeight)
{
  nsCOMPtr<mozIDOMWindowProxy> domWindow;
  GetWindowDOMWindow(getter_AddRefs(domWindow));
  NS_ENSURE_STATE(domWindow);

  auto* window = nsGlobalWindowOuter::Cast(domWindow);

  RefPtr<nsScreen> screen = window->GetScreen();
  NS_ENSURE_STATE(screen);

  ErrorResult rv;
  *aAvailWidth = screen->GetAvailWidth(rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  *aAvailHeight = screen->GetAvailHeight(rv);
  if (NS_WARN_IF(rv.Failed())) {
    return rv.StealNSResult();
  }

  return NS_OK;
}

// Rounds window size to 1000x1000, or, if there isn't enough available
// screen space, to a multiple of 200x100.
NS_IMETHODIMP nsXULWindow::ForceRoundedDimensions()
{
  if (mIsHiddenWindow) {
    return NS_OK;
  }

  int32_t availWidthCSS    = 0;
  int32_t availHeightCSS   = 0;
  int32_t contentWidthCSS  = 0;
  int32_t contentHeightCSS = 0;
  int32_t windowWidthCSS   = 0;
  int32_t windowHeightCSS  = 0;
  double devicePerCSSPixels = 1.0;

  GetUnscaledDevicePixelsPerCSSPixel(&devicePerCSSPixels);

  GetAvailScreenSize(&availWidthCSS, &availHeightCSS);

  // To get correct chrome size, we have to resize the window to a proper
  // size first. So, here, we size it to its available size.
  SetSpecifiedSize(availWidthCSS, availHeightCSS);

  // Get the current window size for calculating chrome UI size.
  GetSize(&windowWidthCSS, &windowHeightCSS); // device pixels
  windowWidthCSS = NSToIntRound(windowWidthCSS / devicePerCSSPixels);
  windowHeightCSS = NSToIntRound(windowHeightCSS / devicePerCSSPixels);

  // Get the content size for calculating chrome UI size.
  GetPrimaryContentSize(&contentWidthCSS, &contentHeightCSS);

  // Calculate the chrome UI size.
  int32_t chromeWidth = 0, chromeHeight = 0;
  chromeWidth = windowWidthCSS - contentWidthCSS;
  chromeHeight = windowHeightCSS - contentHeightCSS;

  int32_t targetContentWidth = 0, targetContentHeight = 0;

  // Here, we use the available screen dimensions as the input dimensions to
  // force the window to be rounded as the maximum available content size.
  nsContentUtils::CalcRoundedWindowSizeForResistingFingerprinting(
    chromeWidth,
    chromeHeight,
    availWidthCSS,
    availHeightCSS,
    availWidthCSS,
    availHeightCSS,
    false, // aSetOuterWidth
    false, // aSetOuterHeight
    &targetContentWidth,
    &targetContentHeight
  );

  targetContentWidth = NSToIntRound(targetContentWidth * devicePerCSSPixels);
  targetContentHeight = NSToIntRound(targetContentHeight * devicePerCSSPixels);

  SetPrimaryContentSize(targetContentWidth, targetContentHeight);

  mIgnoreXULSize = true;
  mIgnoreXULSizeMode = true;

  return NS_OK;
}

void nsXULWindow::OnChromeLoaded()
{
  nsresult rv = EnsureContentTreeOwner();

  if (NS_SUCCEEDED(rv)) {
    mChromeLoaded = true;
    ApplyChromeFlags();
    SyncAttributesToWidget();
    SizeShell();

    if (mShowAfterLoad) {
      SetVisibility(true);
      // At this point the window may have been closed during Show(), so
      // nsXULWindow::Destroy may already have been called. Take care!
    }
  }
  mPersistentAttributesMask |= PAD_POSITION | PAD_SIZE | PAD_MISC;
}

// If aSpecWidth and/or aSpecHeight are > 0, we will use these CSS px sizes
// to fit to the screen when staggering windows; if they're negative,
// we use the window's current size instead.
bool nsXULWindow::LoadPositionFromXUL(int32_t aSpecWidth, int32_t aSpecHeight)
{
  bool     gotPosition = false;

  // if we're the hidden window, don't try to validate our size/position. We're
  // special.
  if (mIsHiddenWindow)
    return false;

  nsCOMPtr<dom::Element> windowElement = GetWindowDOMElement();
  NS_ENSURE_TRUE(windowElement, false);

  int32_t currX = 0;
  int32_t currY = 0;
  int32_t currWidth = 0;
  int32_t currHeight = 0;
  nsresult errorCode;
  int32_t temp;

  GetPositionAndSize(&currX, &currY, &currWidth, &currHeight);

  // Convert to global display pixels for consistent window management across
  // screens with diverse resolutions
  double devToDesktopScale = 1.0 / mWindow->GetDesktopToDeviceScale().scale;
  currX = NSToIntRound(currX * devToDesktopScale);
  currY = NSToIntRound(currY * devToDesktopScale);

  // For size, use specified value if > 0, else current value
  double devToCSSScale = 1.0 / mWindow->GetDefaultScale().scale;
  int32_t cssWidth =
    aSpecWidth > 0 ? aSpecWidth : NSToIntRound(currWidth * devToCSSScale);
  int32_t cssHeight =
    aSpecHeight > 0 ? aSpecHeight : NSToIntRound(currHeight * devToCSSScale);

  // Obtain the position information from the <xul:window> element.
  int32_t specX = currX;
  int32_t specY = currY;
  nsAutoString posString;

  windowElement->GetAttribute(SCREENX_ATTRIBUTE, posString);
  temp = posString.ToInteger(&errorCode);
  if (NS_SUCCEEDED(errorCode)) {
    specX = temp;
    gotPosition = true;
  }
  windowElement->GetAttribute(SCREENY_ATTRIBUTE, posString);
  temp = posString.ToInteger(&errorCode);
  if (NS_SUCCEEDED(errorCode)) {
    specY = temp;
    gotPosition = true;
  }

  if (gotPosition) {
    // our position will be relative to our parent, if any
    nsCOMPtr<nsIBaseWindow> parent(do_QueryReferent(mParentWindow));
    if (parent) {
      int32_t parentX, parentY;
      if (NS_SUCCEEDED(parent->GetPosition(&parentX, &parentY))) {
        double scale;
        if (NS_SUCCEEDED(parent->GetDevicePixelsPerDesktopPixel(&scale))) {
          parentX = NSToIntRound(parentX / scale);
          parentY = NSToIntRound(parentY / scale);
        }
        specX += parentX;
        specY += parentY;
      }
    }
    else {
      StaggerPosition(specX, specY, cssWidth, cssHeight);
    }
  }
  mWindow->ConstrainPosition(false, &specX, &specY);
  if (specX != currX || specY != currY) {
    SetPositionDesktopPix(specX, specY);
  }

  return gotPosition;
}

static Maybe<int32_t>
ReadIntAttribute(const Element& aElement, nsAtom* aAtom)
{
  nsAutoString attrString;
  if (!aElement.GetAttr(kNameSpaceID_None, aAtom, attrString)) {
    return Nothing();
  }

  nsresult res = NS_OK;
  int32_t ret = attrString.ToInteger(&res);
  return NS_SUCCEEDED(res) ? Some(ret) : Nothing();
}

static Maybe<int32_t>
ReadSize(const Element& aElement,
         nsAtom* aAttr,
         nsAtom* aMinAttr,
         nsAtom* aMaxAttr)
{
  Maybe<int32_t> attr = ReadIntAttribute(aElement, aAttr);
  if (!attr) {
    return Nothing();
  }

  int32_t min =
    std::max(100, ReadIntAttribute(aElement, aMinAttr).valueOr(100));
  int32_t max = ReadIntAttribute(aElement, aMaxAttr)
    .valueOr(std::numeric_limits<int32_t>::max());

  return Some(std::min(max, std::max(*attr, min)));
}

bool
nsXULWindow::LoadSizeFromXUL(int32_t& aSpecWidth, int32_t& aSpecHeight)
{
  bool     gotSize = false;

  // if we're the hidden window, don't try to validate our size/position. We're
  // special.
  if (mIsHiddenWindow) {
    return false;
  }

  nsCOMPtr<dom::Element> windowElement = GetWindowDOMElement();
  NS_ENSURE_TRUE(windowElement, false);

  // Obtain the sizing information from the <xul:window> element.
  aSpecWidth = 100;
  aSpecHeight = 100;

  if (auto width = ReadSize(*windowElement,
                            nsGkAtoms::width,
                            nsGkAtoms::minwidth,
                            nsGkAtoms::maxwidth)) {
    aSpecWidth = *width;
    gotSize = true;
  }

  if (auto height = ReadSize(*windowElement,
                             nsGkAtoms::height,
                             nsGkAtoms::minheight,
                             nsGkAtoms::maxheight)) {
    aSpecHeight = *height;
    gotSize = true;
  }

  return gotSize;
}

void
nsXULWindow::SetSpecifiedSize(int32_t aSpecWidth, int32_t aSpecHeight)
{
  // constrain to screen size
  int32_t screenWidth;
  int32_t screenHeight;

  if (NS_SUCCEEDED(GetAvailScreenSize(&screenWidth, &screenHeight))) {
    if (aSpecWidth > screenWidth) {
      aSpecWidth = screenWidth;
    }
    if (aSpecHeight > screenHeight) {
      aSpecHeight = screenHeight;
    }
  }

  NS_ASSERTION(mWindow, "we expected to have a window already");

  int32_t currWidth = 0;
  int32_t currHeight = 0;
  GetSize(&currWidth, &currHeight); // returns device pixels

  // convert specified values to device pixels, and resize if needed
  double cssToDevPx = mWindow ? mWindow->GetDefaultScale().scale : 1.0;
  aSpecWidth = NSToIntRound(aSpecWidth * cssToDevPx);
  aSpecHeight = NSToIntRound(aSpecHeight * cssToDevPx);
  mIntrinsicallySized = false;
  if (aSpecWidth != currWidth || aSpecHeight != currHeight) {
    SetSize(aSpecWidth, aSpecHeight, false);
  }
}

/* Miscellaneous persistent attributes are attributes named in the
   |persist| attribute, other than size and position. Those are special
   because it's important to load those before one of the misc
   attributes (sizemode) and they require extra processing. */
bool nsXULWindow::LoadMiscPersistentAttributesFromXUL()
{
  bool     gotState = false;

  /* There are no misc attributes of interest to the hidden window.
     It's especially important not to try to validate that window's
     size or position, because some platforms (Mac OS X) need to
     make it visible and offscreen. */
  if (mIsHiddenWindow)
    return false;

  nsCOMPtr<dom::Element> windowElement = GetWindowDOMElement();
  NS_ENSURE_TRUE(windowElement, false);

  nsAutoString stateString;

  // sizemode
  windowElement->GetAttribute(MODE_ATTRIBUTE, stateString);
  nsSizeMode sizeMode = nsSizeMode_Normal;
  /* ignore request to minimize, to not confuse novices
  if (stateString.Equals(SIZEMODE_MINIMIZED))
    sizeMode = nsSizeMode_Minimized;
  */
  if (!mIgnoreXULSizeMode &&
      (stateString.Equals(SIZEMODE_MAXIMIZED) || stateString.Equals(SIZEMODE_FULLSCREEN))) {
    /* Honor request to maximize only if the window is sizable.
       An unsizable, unmaximizable, yet maximized window confuses
       Windows OS and is something of a travesty, anyway. */
    if (mChromeFlags & nsIWebBrowserChrome::CHROME_WINDOW_RESIZE) {
      mIntrinsicallySized = false;

      if (stateString.Equals(SIZEMODE_MAXIMIZED))
        sizeMode = nsSizeMode_Maximized;
      else
        sizeMode = nsSizeMode_Fullscreen;
    }
  }

  // If we are told to ignore the size mode attribute update the
  // document so the attribute and window are in sync.
  if (mIgnoreXULSizeMode) {
    nsAutoString sizeString;
    if (sizeMode == nsSizeMode_Maximized)
      sizeString.Assign(SIZEMODE_MAXIMIZED);
    else if (sizeMode == nsSizeMode_Fullscreen)
      sizeString.Assign(SIZEMODE_FULLSCREEN);
    else if (sizeMode == nsSizeMode_Normal)
      sizeString.Assign(SIZEMODE_NORMAL);
    if (!sizeString.IsEmpty()) {
      ErrorResult rv;
      windowElement->SetAttribute(MODE_ATTRIBUTE, sizeString, rv);
    }
  }

  if (sizeMode == nsSizeMode_Fullscreen) {
    nsCOMPtr<mozIDOMWindowProxy> ourWindow;
    GetWindowDOMWindow(getter_AddRefs(ourWindow));
    auto* piWindow = nsPIDOMWindowOuter::From(ourWindow);
    piWindow->SetFullScreen(true);
  } else {
    // For maximized windows, ignore the XUL size and position attributes,
    // as setting them would set the window back to normal sizemode.
    if (sizeMode == nsSizeMode_Maximized) {
      mIgnoreXULSize = true;
      mIgnoreXULPosition = true;
    }
    mWindow->SetSizeMode(sizeMode);
  }
  gotState = true;

  // zlevel
  windowElement->GetAttribute(ZLEVEL_ATTRIBUTE, stateString);
  if (!stateString.IsEmpty()) {
    nsresult errorCode;
    int32_t zLevel = stateString.ToInteger(&errorCode);
    if (NS_SUCCEEDED(errorCode) && zLevel >= lowestZ && zLevel <= highestZ)
      SetZLevel(zLevel);
  }

  return gotState;
}

/* Stagger windows of the same type so they don't appear on top of each other.
   This code does have a scary double loop -- it'll keep passing through
   the entire list of open windows until it finds a non-collision. Doesn't
   seem to be a problem, but it deserves watching.
   The aRequested{X,Y} parameters here are in desktop pixels;
   the aSpec{Width,Height} parameters are CSS pixel dimensions.
*/
void nsXULWindow::StaggerPosition(int32_t &aRequestedX, int32_t &aRequestedY,
                                  int32_t aSpecWidth, int32_t aSpecHeight)
{
  // These "constants" will be converted from CSS to desktop pixels
  // for the appropriate screen, assuming we find a screen to use...
  // hence they're not actually declared const here.
  int32_t kOffset = 22;
  uint32_t kSlop  = 4;

  bool     keepTrying;
  int      bouncedX = 0, // bounced off vertical edge of screen
           bouncedY = 0; // bounced off horizontal edge

  // look for any other windows of this type
  nsCOMPtr<nsIWindowMediator> wm(do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (!wm)
    return;

  nsCOMPtr<dom::Element> windowElement = GetWindowDOMElement();
  if (!windowElement)
    return;

  nsCOMPtr<nsIXULWindow> ourXULWindow(this);

  nsAutoString windowType;
  windowElement->GetAttribute(WINDOWTYPE_ATTRIBUTE, windowType);

  int32_t screenTop = 0,    // it's pointless to initialize these ...
          screenRight = 0,  // ... but to prevent oversalubrious and ...
          screenBottom = 0, // ... underbright compilers from ...
          screenLeft = 0;   // ... issuing warnings.
  bool    gotScreen = false;

  { // fetch screen coordinates
    nsCOMPtr<nsIScreenManager> screenMgr(do_GetService(
                                         "@mozilla.org/gfx/screenmanager;1"));
    if (screenMgr) {
      nsCOMPtr<nsIScreen> ourScreen;
      // the coordinates here are already display pixels
      screenMgr->ScreenForRect(aRequestedX, aRequestedY,
                               aSpecWidth, aSpecHeight,
                               getter_AddRefs(ourScreen));
      if (ourScreen) {
        int32_t screenWidth, screenHeight;
        ourScreen->GetAvailRectDisplayPix(&screenLeft, &screenTop,
                                          &screenWidth, &screenHeight);
        screenBottom = screenTop + screenHeight;
        screenRight = screenLeft + screenWidth;
        // Get the screen's scaling factors and convert staggering constants
        // from CSS px to desktop pixel units
        double desktopToDeviceScale = 1.0, cssToDeviceScale = 1.0;
        ourScreen->GetContentsScaleFactor(&desktopToDeviceScale);
        ourScreen->GetDefaultCSSScaleFactor(&cssToDeviceScale);
        double cssToDesktopFactor = cssToDeviceScale / desktopToDeviceScale;
        kOffset = NSToIntRound(kOffset * cssToDesktopFactor);
        kSlop = NSToIntRound(kSlop * cssToDesktopFactor);
        // Convert dimensions from CSS to desktop pixels
        aSpecWidth = NSToIntRound(aSpecWidth * cssToDesktopFactor);
        aSpecHeight = NSToIntRound(aSpecHeight * cssToDesktopFactor);
        gotScreen = true;
      }
    }
  }

  // One full pass through all windows of this type, repeat until no collisions.
  do {
    keepTrying = false;
    nsCOMPtr<nsISimpleEnumerator> windowList;
    wm->GetXULWindowEnumerator(windowType.get(), getter_AddRefs(windowList));

    if (!windowList)
      break;

    // One full pass through all windows of this type, offset and stop on collision.
    do {
      bool more;
      windowList->HasMoreElements(&more);
      if (!more)
        break;

      nsCOMPtr<nsISupports> supportsWindow;
      windowList->GetNext(getter_AddRefs(supportsWindow));

      nsCOMPtr<nsIXULWindow> listXULWindow(do_QueryInterface(supportsWindow));
      if (listXULWindow != ourXULWindow) {
        int32_t listX, listY;
        nsCOMPtr<nsIBaseWindow> listBaseWindow(do_QueryInterface(supportsWindow));
        listBaseWindow->GetPosition(&listX, &listY);
        double scale;
        if (NS_SUCCEEDED(listBaseWindow->GetDevicePixelsPerDesktopPixel(&scale))) {
          listX = NSToIntRound(listX / scale);
          listY = NSToIntRound(listY / scale);
        }

        if (Abs(listX - aRequestedX) <= kSlop && Abs(listY - aRequestedY) <= kSlop) {
          // collision! offset and start over
          if (bouncedX & 0x1)
            aRequestedX -= kOffset;
          else
            aRequestedX += kOffset;
          aRequestedY += kOffset;

          if (gotScreen) {
            // if we're moving to the right and we need to bounce...
            if (!(bouncedX & 0x1) && ((aRequestedX + aSpecWidth) > screenRight)) {
              aRequestedX = screenRight - aSpecWidth;
              ++bouncedX;
            }

            // if we're moving to the left and we need to bounce...
            if ((bouncedX & 0x1) && aRequestedX < screenLeft) {
              aRequestedX = screenLeft;
              ++bouncedX;
            }

            // if we hit the bottom then bounce to the top
            if (aRequestedY + aSpecHeight > screenBottom) {
              aRequestedY = screenTop;
              ++bouncedY;
            }
          }

          /* loop around again,
             but it's time to give up once we've covered the screen.
             there's a potential infinite loop with lots of windows. */
          keepTrying = bouncedX < 2 || bouncedY == 0;
          break;
        }
      }
    } while(1);
  } while (keepTrying);
}

void nsXULWindow::SyncAttributesToWidget()
{
  nsCOMPtr<dom::Element> windowElement = GetWindowDOMElement();
  if (!windowElement)
    return;

  nsAutoString attr;

  // "hidechrome" attribute
  if (windowElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidechrome,
                                 nsGkAtoms::_true, eCaseMatters)) {
    mWindow->HideWindowChrome(true);
  }

  // "chromemargin" attribute
  nsIntMargin margins;
  windowElement->GetAttribute(NS_LITERAL_STRING("chromemargin"), attr);
  if (nsContentUtils::ParseIntMarginValue(attr, margins)) {
    LayoutDeviceIntMargin tmp = LayoutDeviceIntMargin::FromUnknownMargin(margins);
    mWindow->SetNonClientMargins(tmp);
  }

  // "windowtype" attribute
  windowElement->GetAttribute(WINDOWTYPE_ATTRIBUTE, attr);
  if (!attr.IsEmpty()) {
    mWindow->SetWindowClass(attr);
  }

  // "id" attribute for icon
  windowElement->GetAttribute(NS_LITERAL_STRING("id"), attr);
  if (attr.IsEmpty()) {
    attr.AssignLiteral("default");
  }
  mWindow->SetIcon(attr);

  // "drawtitle" attribute
  windowElement->GetAttribute(NS_LITERAL_STRING("drawtitle"), attr);
  mWindow->SetDrawsTitle(attr.LowerCaseEqualsLiteral("true"));

  // "toggletoolbar" attribute
  windowElement->GetAttribute(NS_LITERAL_STRING("toggletoolbar"), attr);
  mWindow->SetShowsToolbarButton(attr.LowerCaseEqualsLiteral("true"));

  // "fullscreenbutton" attribute
  windowElement->GetAttribute(NS_LITERAL_STRING("fullscreenbutton"), attr);
  mWindow->SetShowsFullScreenButton(attr.LowerCaseEqualsLiteral("true"));

  // "macanimationtype" attribute
  windowElement->GetAttribute(NS_LITERAL_STRING("macanimationtype"), attr);
  if (attr.EqualsLiteral("document")) {
    mWindow->SetWindowAnimationType(nsIWidget::eDocumentWindowAnimation);
  }
}

NS_IMETHODIMP nsXULWindow::SavePersistentAttributes()
{
  // can happen when the persistence timer fires at an inopportune time
  // during window shutdown
  if (!mDocShell)
    return NS_ERROR_FAILURE;

  nsCOMPtr<dom::Element> docShellElement = GetWindowDOMElement();
  if (!docShellElement)
    return NS_ERROR_FAILURE;

  nsAutoString   persistString;
  docShellElement->GetAttribute(PERSIST_ATTRIBUTE, persistString);
  if (persistString.IsEmpty()) { // quick check which sometimes helps
    mPersistentAttributesDirty = 0;
    return NS_OK;
  }

  bool isFullscreen = false;
  if (nsPIDOMWindowOuter* domWindow = mDocShell->GetWindow()) {
    isFullscreen = domWindow->GetFullScreen();
  }

  // get our size, position and mode to persist
  LayoutDeviceIntRect rect;
  bool gotRestoredBounds = NS_SUCCEEDED(mWindow->GetRestoredBounds(rect));

  // we use CSS pixels for size, but desktop pixels for position
  CSSToLayoutDeviceScale sizeScale = mWindow->GetDefaultScale();
  DesktopToLayoutDeviceScale posScale = mWindow->GetDesktopToDeviceScale();

  // make our position relative to our parent, if any
  nsCOMPtr<nsIBaseWindow> parent(do_QueryReferent(mParentWindow));
  if (parent && gotRestoredBounds) {
    int32_t parentX, parentY;
    if (NS_SUCCEEDED(parent->GetPosition(&parentX, &parentY))) {
      rect.MoveBy(-parentX, -parentY);
    }
  }

  nsAutoString                sizeString;
  nsAutoString                windowElementId;

  // fetch docShellElement's ID and XUL owner document
  RefPtr<dom::XULDocument> ownerXULDoc =
    docShellElement->OwnerDoc()->IsXULDocument()
      ? docShellElement->OwnerDoc()->AsXULDocument() : nullptr;
  if (docShellElement->IsXULElement()) {
    docShellElement->GetId(windowElementId);
  }

  bool shouldPersist = !isFullscreen && ownerXULDoc;
  ErrorResult rv;
  // (only for size elements which are persisted)
  if ((mPersistentAttributesDirty & PAD_POSITION) && gotRestoredBounds) {
    if (persistString.Find("screenX") >= 0) {
      sizeString.Truncate();
      sizeString.AppendInt(NSToIntRound(rect.X() / posScale.scale));
      docShellElement->SetAttribute(SCREENX_ATTRIBUTE, sizeString, rv);
      if (shouldPersist) {
        IgnoredErrorResult err;
        ownerXULDoc->Persist(windowElementId, SCREENX_ATTRIBUTE, err);
      }
    }
    if (persistString.Find("screenY") >= 0) {
      sizeString.Truncate();
      sizeString.AppendInt(NSToIntRound(rect.Y() / posScale.scale));
      docShellElement->SetAttribute(SCREENY_ATTRIBUTE, sizeString, rv);
      if (shouldPersist) {
        IgnoredErrorResult err;
        ownerXULDoc->Persist(windowElementId, SCREENY_ATTRIBUTE, err);
      }
    }
  }

  if ((mPersistentAttributesDirty & PAD_SIZE) && gotRestoredBounds) {
    LayoutDeviceIntRect innerRect = rect - GetOuterToInnerSizeDifference(mWindow);
    if (persistString.Find("width") >= 0) {
      sizeString.Truncate();
      sizeString.AppendInt(NSToIntRound(innerRect.Width() / sizeScale.scale));
      docShellElement->SetAttribute(WIDTH_ATTRIBUTE, sizeString, rv);
      if (shouldPersist) {
        IgnoredErrorResult err;
        ownerXULDoc->Persist(windowElementId, WIDTH_ATTRIBUTE, err);
      }
    }
    if (persistString.Find("height") >= 0) {
      sizeString.Truncate();
      sizeString.AppendInt(NSToIntRound(innerRect.Height() / sizeScale.scale));
      docShellElement->SetAttribute(HEIGHT_ATTRIBUTE, sizeString, rv);
      if (shouldPersist) {
        IgnoredErrorResult err;
        ownerXULDoc->Persist(windowElementId, HEIGHT_ATTRIBUTE, err);
      }
    }
  }

  if (mPersistentAttributesDirty & PAD_MISC) {
    nsSizeMode sizeMode = mWindow->SizeMode();

    if (sizeMode != nsSizeMode_Minimized) {
      if (sizeMode == nsSizeMode_Maximized)
        sizeString.Assign(SIZEMODE_MAXIMIZED);
      else if (sizeMode == nsSizeMode_Fullscreen)
        sizeString.Assign(SIZEMODE_FULLSCREEN);
      else
        sizeString.Assign(SIZEMODE_NORMAL);
      docShellElement->SetAttribute(MODE_ATTRIBUTE, sizeString, rv);
      if (shouldPersist && persistString.Find("sizemode") >= 0) {
        IgnoredErrorResult err;
        ownerXULDoc->Persist(windowElementId, MODE_ATTRIBUTE, err);
      }
    }
    if (persistString.Find("zlevel") >= 0) {
      uint32_t zLevel;
      nsCOMPtr<nsIWindowMediator> mediator(do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
      if (mediator) {
        mediator->GetZLevel(this, &zLevel);
        sizeString.Truncate();
        sizeString.AppendInt(zLevel);
        docShellElement->SetAttribute(ZLEVEL_ATTRIBUTE, sizeString, rv);
        if (shouldPersist) {
          IgnoredErrorResult err;
          ownerXULDoc->Persist(windowElementId, ZLEVEL_ATTRIBUTE, err);
        }
      }
    }
  }

  mPersistentAttributesDirty = 0;
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetWindowDOMWindow(mozIDOMWindowProxy** aDOMWindow)
{
  NS_ENSURE_STATE(mDocShell);

  if (!mDOMWindow)
    mDOMWindow = mDocShell->GetWindow();
  NS_ENSURE_TRUE(mDOMWindow, NS_ERROR_FAILURE);

  *aDOMWindow = mDOMWindow;
  NS_ADDREF(*aDOMWindow);
  return NS_OK;
}

dom::Element*
nsXULWindow::GetWindowDOMElement() const
{
  NS_ENSURE_TRUE(mDocShell, nullptr);

  nsCOMPtr<nsIContentViewer> cv;
  mDocShell->GetContentViewer(getter_AddRefs(cv));
  NS_ENSURE_TRUE(cv, nullptr);

  const nsIDocument* document = cv->GetDocument();
  NS_ENSURE_TRUE(document, nullptr);

  return document->GetRootElement();
}

nsresult nsXULWindow::ContentShellAdded(nsIDocShellTreeItem* aContentShell,
   bool aPrimary)
{
  // Set the default content tree owner
  if (aPrimary) {
    NS_ENSURE_SUCCESS(EnsurePrimaryContentTreeOwner(), NS_ERROR_FAILURE);
    aContentShell->SetTreeOwner(mPrimaryContentTreeOwner);
    mPrimaryContentShell = aContentShell;
    mPrimaryTabParent = nullptr;
  }
  else {
    NS_ENSURE_SUCCESS(EnsureContentTreeOwner(), NS_ERROR_FAILURE);
    aContentShell->SetTreeOwner(mContentTreeOwner);
    if (mPrimaryContentShell == aContentShell)
      mPrimaryContentShell = nullptr;
  }

  return NS_OK;
}

nsresult nsXULWindow::ContentShellRemoved(nsIDocShellTreeItem* aContentShell)
{
  if (mPrimaryContentShell == aContentShell) {
    mPrimaryContentShell = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXULWindow::GetPrimaryContentSize(int32_t* aWidth,
                                   int32_t* aHeight)
{
  if (mPrimaryTabParent) {
    return GetPrimaryTabParentSize(aWidth, aHeight);
  } else if (mPrimaryContentShell) {
    return GetPrimaryContentShellSize(aWidth, aHeight);
  }
  return NS_ERROR_UNEXPECTED;
}

nsresult
nsXULWindow::GetPrimaryTabParentSize(int32_t* aWidth,
                                     int32_t* aHeight)
{
  TabParent* tabParent = TabParent::GetFrom(mPrimaryTabParent);
  // Need strong ref, since Client* can run script.
  nsCOMPtr<Element> element = tabParent->GetOwnerElement();
  NS_ENSURE_STATE(element);

  *aWidth = element->ClientWidth();
  *aHeight = element->ClientHeight();
  return NS_OK;
}

nsresult
nsXULWindow::GetPrimaryContentShellSize(int32_t* aWidth,
                                        int32_t* aHeight)
{
  NS_ENSURE_STATE(mPrimaryContentShell);

  nsCOMPtr<nsIBaseWindow> shellWindow(do_QueryInterface(mPrimaryContentShell));
  NS_ENSURE_STATE(shellWindow);

  int32_t devicePixelWidth, devicePixelHeight;
  double shellScale = 1.0;
  // We want to return CSS pixels. First, we get device pixels
  // from the content area...
  shellWindow->GetSize(&devicePixelWidth, &devicePixelHeight);
  // And then get the device pixel scaling factor. Dividing device
  // pixels by this scaling factor gives us CSS pixels.
  shellWindow->GetUnscaledDevicePixelsPerCSSPixel(&shellScale);
  *aWidth = NSToIntRound(devicePixelWidth / shellScale);
  *aHeight = NSToIntRound(devicePixelHeight / shellScale);
  return NS_OK;
}

NS_IMETHODIMP
nsXULWindow::SetPrimaryContentSize(int32_t aWidth,
                                   int32_t aHeight)
{
  if (mPrimaryTabParent) {
    return SetPrimaryTabParentSize(aWidth, aHeight);
  } else if (mPrimaryContentShell) {
    return SizeShellTo(mPrimaryContentShell, aWidth, aHeight);
  }
  return NS_ERROR_UNEXPECTED;
}

nsresult
nsXULWindow::SetPrimaryTabParentSize(int32_t aWidth,
                                     int32_t aHeight)
{
  int32_t shellWidth, shellHeight;
  GetPrimaryTabParentSize(&shellWidth, &shellHeight);

  double scale = 1.0;
  GetUnscaledDevicePixelsPerCSSPixel(&scale);

  SizeShellToWithLimit(aWidth, aHeight,
                       shellWidth * scale, shellHeight * scale);
  return NS_OK;
}

nsresult
nsXULWindow::GetRootShellSize(int32_t* aWidth,
                              int32_t* aHeight)
{
  nsCOMPtr<nsIBaseWindow> shellAsWin = do_QueryInterface(mDocShell);
  NS_ENSURE_TRUE(shellAsWin, NS_ERROR_FAILURE);
  return shellAsWin->GetSize(aWidth, aHeight);
}

nsresult
nsXULWindow::SetRootShellSize(int32_t aWidth,
                              int32_t aHeight)
{
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem = do_QueryInterface(mDocShell);
  return SizeShellTo(docShellAsItem, aWidth, aHeight);
}

NS_IMETHODIMP nsXULWindow::SizeShellTo(nsIDocShellTreeItem* aShellItem,
   int32_t aCX, int32_t aCY)
{
  // XXXTAB This is wrong, we should actually reflow based on the passed in
  // shell.  For now we are hacking and doing delta sizing.  This is bad
  // because it assumes all size we add will go to the shell which probably
  // won't happen.

  nsCOMPtr<nsIBaseWindow> shellAsWin(do_QueryInterface(aShellItem));
  NS_ENSURE_TRUE(shellAsWin, NS_ERROR_FAILURE);

  int32_t width = 0;
  int32_t height = 0;
  shellAsWin->GetSize(&width, &height);

  SizeShellToWithLimit(aCX, aCY, width, height);

  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::ExitModalLoop(nsresult aStatus)
{
  if (mContinueModalLoop)
    EnableParent(true);
  mContinueModalLoop = false;
  mModalStatus = aStatus;
  return NS_OK;
}

// top-level function to create a new window
NS_IMETHODIMP nsXULWindow::CreateNewWindow(int32_t aChromeFlags,
                                           nsITabParent *aOpeningTab,
                                           mozIDOMWindowProxy *aOpener,
                                           uint64_t aNextTabParentId,
                                           nsIXULWindow **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  if (aChromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME) {
    MOZ_RELEASE_ASSERT(aNextTabParentId == 0,
                       "Unexpected next tab parent ID, should never have a non-zero nextTabParentId when creating a new chrome window");
    return CreateNewChromeWindow(aChromeFlags, aOpeningTab, aOpener, _retval);
  }
  return CreateNewContentWindow(aChromeFlags, aOpeningTab, aOpener, aNextTabParentId, _retval);
}

NS_IMETHODIMP nsXULWindow::CreateNewChromeWindow(int32_t aChromeFlags,
                                                 nsITabParent *aOpeningTab,
                                                 mozIDOMWindowProxy *aOpener,
                                                 nsIXULWindow **_retval)
{
  nsCOMPtr<nsIAppShellService> appShell(do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
  NS_ENSURE_TRUE(appShell, NS_ERROR_FAILURE);

  // Just do a normal create of a window and return.
  nsCOMPtr<nsIXULWindow> newWindow;
  appShell->CreateTopLevelWindow(this, nullptr, aChromeFlags,
                                 nsIAppShellService::SIZE_TO_CONTENT,
                                 nsIAppShellService::SIZE_TO_CONTENT,
                                 aOpeningTab, aOpener,
                                 getter_AddRefs(newWindow));

  NS_ENSURE_TRUE(newWindow, NS_ERROR_FAILURE);

  *_retval = newWindow;
  NS_ADDREF(*_retval);

  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::CreateNewContentWindow(int32_t aChromeFlags,
                                                  nsITabParent *aOpeningTab,
                                                  mozIDOMWindowProxy *aOpener,
                                                  uint64_t aNextTabParentId,
                                                  nsIXULWindow **_retval)
{
  nsCOMPtr<nsIAppShellService> appShell(do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
  NS_ENSURE_TRUE(appShell, NS_ERROR_FAILURE);

  // We need to create a new top level window and then enter a nested
  // loop. Eventually the new window will be told that it has loaded,
  // at which time we know it is safe to spin out of the nested loop
  // and allow the opening code to proceed.

  nsCOMPtr<nsIURI> uri;

  nsAutoCString urlStr;
  Preferences::GetCString("browser.chromeURL", urlStr);
  if (urlStr.IsEmpty()) {
    urlStr.AssignLiteral("chrome://navigator/content/navigator.xul");
  }

  nsCOMPtr<nsIIOService> service(do_GetService(NS_IOSERVICE_CONTRACTID));
  if (service) {
    service->NewURI(urlStr, nullptr, nullptr, getter_AddRefs(uri));
  }
  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);

  // We need to create a chrome window to contain the content window we're about
  // to pass back. The subject principal needs to be system while we're creating
  // it to make things work right, so force a system caller. See bug 799348
  // comment 13 for a description of what happens when we don't.
  nsCOMPtr<nsIXULWindow> newWindow;
  {
    AutoNoJSAPI nojsapi;
    // We actually want this toplevel window which we are creating to have a
    // null opener, as we will be creating the content xul:browser window inside
    // of it, so we pass nullptr as our aOpener.
    appShell->CreateTopLevelWindow(this, uri,
                                   aChromeFlags, 615, 480,
                                   aOpeningTab, nullptr,
                                   getter_AddRefs(newWindow));
    NS_ENSURE_TRUE(newWindow, NS_ERROR_FAILURE);
  }

  // Specify that we want the window to remain locked until the chrome has loaded.
  nsXULWindow *xulWin = static_cast<nsXULWindow*>
                                   (static_cast<nsIXULWindow*>
                                               (newWindow));

  if (aNextTabParentId) {
    xulWin->mNextTabParentId = aNextTabParentId;
  }

  if (aOpener) {
    nsCOMPtr<nsIDocShell> docShell;
    xulWin->GetDocShell(getter_AddRefs(docShell));
    MOZ_ASSERT(docShell);
    nsCOMPtr<nsPIDOMWindowOuter> window = docShell->GetWindow();
    MOZ_ASSERT(window);
    window->SetOpenerForInitialContentBrowser(nsPIDOMWindowOuter::From(aOpener));
  }

  xulWin->LockUntilChromeLoad();

  {
    AutoNoJSAPI nojsapi;
    SpinEventLoopUntil([&]() { return !xulWin->IsLocked(); });
  }

  NS_ENSURE_STATE(xulWin->mPrimaryContentShell || xulWin->mPrimaryTabParent);
  MOZ_ASSERT_IF(xulWin->mPrimaryContentShell, aNextTabParentId == 0);

  *_retval = newWindow;
  NS_ADDREF(*_retval);

  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetHasPrimaryContent(bool* aResult)
{
  *aResult = mPrimaryTabParent || mPrimaryContentShell;
  return NS_OK;
}

void nsXULWindow::EnableParent(bool aEnable)
{
  nsCOMPtr<nsIBaseWindow> parentWindow;
  nsCOMPtr<nsIWidget>     parentWidget;

  parentWindow = do_QueryReferent(mParentWindow);
  if (parentWindow)
    parentWindow->GetMainWidget(getter_AddRefs(parentWidget));
  if (parentWidget)
    parentWidget->Enable(aEnable);
}

// Constrain the window to its proper z-level
bool nsXULWindow::ConstrainToZLevel(bool        aImmediate,
                                      nsWindowZ  *aPlacement,
                                      nsIWidget  *aReqBelow,
                                      nsIWidget **aActualBelow)
{
#if 0
  /* Do we have a parent window? This means our z-order is already constrained,
     since we're a dependent window. Our window list isn't hierarchical,
     so we can't properly calculate placement for such a window.
     Should we just abort? */
  nsCOMPtr<nsIBaseWindow> parentWindow = do_QueryReferent(mParentWindow);
  if (parentWindow)
    return false;
#endif

  nsCOMPtr<nsIWindowMediator> mediator(do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (!mediator)
    return false;

  bool           altered;
  uint32_t       position,
                 newPosition,
                 zLevel;
  nsIXULWindow  *us = this;

  altered = false;
  mediator->GetZLevel(this, &zLevel);

  // translate from WidgetGUIEvent to nsIWindowMediator constants
  position = nsIWindowMediator::zLevelTop;
  if (*aPlacement == nsWindowZBottom || zLevel == nsIXULWindow::lowestZ)
    position = nsIWindowMediator::zLevelBottom;
  else if (*aPlacement == nsWindowZRelative)
    position = nsIWindowMediator::zLevelBelow;

  if (NS_SUCCEEDED(mediator->CalculateZPosition(us, position, aReqBelow,
                               &newPosition, aActualBelow, &altered))) {
    /* If we were asked to move to the top but constrained to remain
       below one of our other windows, first move all windows in that
       window's layer and above to the top. This allows the user to
       click a window which can't be topmost and still bring mozilla
       to the foreground. */
    if (altered &&
        (position == nsIWindowMediator::zLevelTop ||
         (position == nsIWindowMediator::zLevelBelow && aReqBelow == 0)))
      PlaceWindowLayersBehind(zLevel + 1, nsIXULWindow::highestZ, 0);

    if (*aPlacement != nsWindowZBottom &&
        position == nsIWindowMediator::zLevelBottom)
      altered = true;
    if (altered || aImmediate) {
      if (newPosition == nsIWindowMediator::zLevelTop)
        *aPlacement = nsWindowZTop;
      else if (newPosition == nsIWindowMediator::zLevelBottom)
        *aPlacement = nsWindowZBottom;
      else
        *aPlacement = nsWindowZRelative;

      if (aImmediate) {
        nsCOMPtr<nsIBaseWindow> ourBase = do_QueryObject(this);
        if (ourBase) {
          nsCOMPtr<nsIWidget> ourWidget;
          ourBase->GetMainWidget(getter_AddRefs(ourWidget));
          ourWidget->PlaceBehind(*aPlacement == nsWindowZBottom ?
                                   eZPlacementBottom : eZPlacementBelow,
                                 *aActualBelow, false);
        }
      }
    }

    /* CalculateZPosition can tell us to be below nothing, because it tries
       not to change something it doesn't recognize. A request to verify
       being below an unrecognized window, then, is treated as a request
       to come to the top (below null) */
    nsCOMPtr<nsIXULWindow> windowAbove;
    if (newPosition == nsIWindowMediator::zLevelBelow && *aActualBelow) {
      windowAbove = (*aActualBelow)->GetWidgetListener()->GetXULWindow();
    }

    mediator->SetZPosition(us, newPosition, windowAbove);
  }

  return altered;
}

/* Re-z-position all windows in the layers from aLowLevel to aHighLevel,
   inclusive, to be behind aBehind. aBehind of null means on top.
   Note this method actually does nothing to our relative window positions.
   (And therefore there's no need to inform WindowMediator we're moving
   things, because we aren't.) This method is useful for, say, moving
   a range of layers of our own windows relative to windows belonging to
   external applications.
*/
void nsXULWindow::PlaceWindowLayersBehind(uint32_t aLowLevel,
                                          uint32_t aHighLevel,
                                          nsIXULWindow *aBehind)
{
  // step through windows in z-order from top to bottommost window

  nsCOMPtr<nsIWindowMediator> mediator(do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (!mediator)
    return;

  nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
  mediator->GetZOrderXULWindowEnumerator(0, true,
              getter_AddRefs(windowEnumerator));
  if (!windowEnumerator)
    return;

  // each window will be moved behind previousHighWidget, itself
  // a moving target. initialize it.
  nsCOMPtr<nsIWidget> previousHighWidget;
  if (aBehind) {
    nsCOMPtr<nsIBaseWindow> highBase(do_QueryInterface(aBehind));
    if (highBase)
      highBase->GetMainWidget(getter_AddRefs(previousHighWidget));
  }

  // get next lower window
  bool more;
  while (NS_SUCCEEDED(windowEnumerator->HasMoreElements(&more)) && more) {
    uint32_t nextZ; // z-level of nextWindow
    nsCOMPtr<nsISupports> nextWindow;
    windowEnumerator->GetNext(getter_AddRefs(nextWindow));
    nsCOMPtr<nsIXULWindow> nextXULWindow(do_QueryInterface(nextWindow));
    nextXULWindow->GetZLevel(&nextZ);
    if (nextZ < aLowLevel)
      break; // we've processed all windows through aLowLevel

    // move it just below its next higher window
    nsCOMPtr<nsIBaseWindow> nextBase(do_QueryInterface(nextXULWindow));
    if (nextBase) {
      nsCOMPtr<nsIWidget> nextWidget;
      nextBase->GetMainWidget(getter_AddRefs(nextWidget));
      if (nextZ <= aHighLevel)
        nextWidget->PlaceBehind(eZPlacementBelow, previousHighWidget, false);
      previousHighWidget = nextWidget;
    }
  }
}

void nsXULWindow::SetContentScrollbarVisibility(bool aVisible)
{
  nsCOMPtr<nsPIDOMWindowOuter> contentWin(do_GetInterface(mPrimaryContentShell));
  if (!contentWin) {
    return;
  }

  nsContentUtils::SetScrollbarsVisibility(contentWin->GetDocShell(), aVisible);
}

bool nsXULWindow::GetContentScrollbarVisibility()
{
  // This code already exists in dom/src/base/nsBarProp.cpp, but we
  // can't safely get to that from here as this function is called
  // while the DOM window is being set up, and we need the DOM window
  // to get to that code.
  nsCOMPtr<nsIScrollable> scroller(do_QueryInterface(mPrimaryContentShell));

  if (scroller) {
    int32_t prefValue;
    scroller->GetDefaultScrollbarPreferences(
                  nsIScrollable::ScrollOrientation_Y, &prefValue);
    if (prefValue == nsIScrollable::Scrollbar_Never) // try the other way
      scroller->GetDefaultScrollbarPreferences(
                  nsIScrollable::ScrollOrientation_X, &prefValue);

    if (prefValue == nsIScrollable::Scrollbar_Never)
      return false;
  }

  return true;
}

// during spinup, attributes that haven't been loaded yet can't be dirty
void nsXULWindow::PersistentAttributesDirty(uint32_t aDirtyFlags)
{
  mPersistentAttributesDirty |= aDirtyFlags & mPersistentAttributesMask;
}

void
nsXULWindow::ApplyChromeFlags()
{
  nsCOMPtr<dom::Element> window = GetWindowDOMElement();
  if (!window) {
    return;
  }

  if (mChromeLoaded) {
    // The two calls in this block don't need to happen early because they
    // don't cause a global restyle on the document.  Not only that, but the
    // scrollbar stuff needs a content area to toggle the scrollbars on anyway.
    // So just don't do these until mChromeLoaded is true.

    // Scrollbars have their own special treatment.
    SetContentScrollbarVisibility(mChromeFlags &
                                  nsIWebBrowserChrome::CHROME_SCROLLBARS ?
                                    true : false);
  }

  /* the other flags are handled together. we have style rules
     in navigator.css that trigger visibility based on
     the 'chromehidden' attribute of the <window> tag. */
  nsAutoString newvalue;

  if (! (mChromeFlags & nsIWebBrowserChrome::CHROME_MENUBAR))
    newvalue.AppendLiteral("menubar ");

  if (! (mChromeFlags & nsIWebBrowserChrome::CHROME_TOOLBAR))
    newvalue.AppendLiteral("toolbar ");

  if (! (mChromeFlags & nsIWebBrowserChrome::CHROME_LOCATIONBAR))
    newvalue.AppendLiteral("location ");

  if (! (mChromeFlags & nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR))
    newvalue.AppendLiteral("directories ");

  if (! (mChromeFlags & nsIWebBrowserChrome::CHROME_STATUSBAR))
    newvalue.AppendLiteral("status ");

  if (! (mChromeFlags & nsIWebBrowserChrome::CHROME_EXTRA))
    newvalue.AppendLiteral("extrachrome ");

  // Note that if we're not actually changing the value this will be a no-op,
  // so no need to compare to the old value.
  IgnoredErrorResult rv;
  window->SetAttribute(NS_LITERAL_STRING("chromehidden"), newvalue, rv);
}

NS_IMETHODIMP
nsXULWindow::BeforeStartLayout()
{
  ApplyChromeFlags();
  SyncAttributesToWidget();
  SizeShell();
  return NS_OK;
}

void
nsXULWindow::SizeShell()
{
  AutoRestore<bool> sizingShellFromXUL(mSizingShellFromXUL);
  mSizingShellFromXUL = true;

  int32_t specWidth = -1, specHeight = -1;
  bool gotSize = false;
  bool isContent = false;

  GetHasPrimaryContent(&isContent);

  CSSIntSize windowDiff = GetOuterToInnerSizeDifferenceInCSSPixels(mWindow);

  // If this window has a primary content and fingerprinting resistance is
  // enabled, we enforce this window to rounded dimensions.
  if (isContent && nsContentUtils::ShouldResistFingerprinting()) {
    ForceRoundedDimensions();
  } else if (!mIgnoreXULSize) {
    gotSize = LoadSizeFromXUL(specWidth, specHeight);
    specWidth += windowDiff.width;
    specHeight += windowDiff.height;
  }

  bool positionSet = !mIgnoreXULPosition;
  nsCOMPtr<nsIXULWindow> parentWindow(do_QueryReferent(mParentWindow));
#if defined(XP_UNIX) && !defined(XP_MACOSX)
  // don't override WM placement on unix for independent, top-level windows
  // (however, we think the benefits of intelligent dependent window placement
  // trump that override.)
  if (!parentWindow)
    positionSet = false;
#endif
  if (positionSet) {
    // We have to do this before sizing the window, because sizing depends
    // on the resolution of the screen we're on. But positioning needs to
    // know the size so that it can constrain to screen bounds.... as an
    // initial guess here, we'll use the specified size (if any).
    positionSet = LoadPositionFromXUL(specWidth, specHeight);
  }

  if (gotSize) {
    SetSpecifiedSize(specWidth, specHeight);
  }

  if (mIntrinsicallySized) {
    // (if LoadSizeFromXUL set the size, mIntrinsicallySized will be false)
    nsCOMPtr<nsIContentViewer> cv;
    mDocShell->GetContentViewer(getter_AddRefs(cv));
    if (cv) {
      nsCOMPtr<nsIDocShellTreeItem> docShellAsItem = do_QueryInterface(mDocShell);
      nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
      docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
      if (treeOwner) {
        // GetContentSize can fail, so initialise |width| and |height| to be
        // on the safe side.
        int32_t width = 0, height = 0;
        if (NS_SUCCEEDED(cv->GetContentSize(&width, &height))) {
          treeOwner->SizeShellTo(docShellAsItem, width, height);
          // Update specified size for the final LoadPositionFromXUL call.
          specWidth = width + windowDiff.width;
          specHeight = height + windowDiff.height;
        }
      }
    }
  }

  // Now that we have set the window's final size, we can re-do its
  // positioning so that it is properly constrained to the screen.
  if (positionSet) {
    LoadPositionFromXUL(specWidth, specHeight);
  }

  LoadMiscPersistentAttributesFromXUL();

  if (mChromeLoaded && mCenterAfterLoad && !positionSet &&
      mWindow->SizeMode() == nsSizeMode_Normal) {
    Center(parentWindow, parentWindow ? false : true, false);
  }
}

NS_IMETHODIMP nsXULWindow::GetXULBrowserWindow(nsIXULBrowserWindow * *aXULBrowserWindow)
{
  NS_IF_ADDREF(*aXULBrowserWindow = mXULBrowserWindow);
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetXULBrowserWindow(nsIXULBrowserWindow * aXULBrowserWindow)
{
  mXULBrowserWindow = aXULBrowserWindow;
  return NS_OK;
}

void nsXULWindow::SizeShellToWithLimit(int32_t aDesiredWidth,
                                       int32_t aDesiredHeight,
                                       int32_t shellItemWidth,
                                       int32_t shellItemHeight)
{
  int32_t widthDelta = aDesiredWidth - shellItemWidth;
  int32_t heightDelta = aDesiredHeight - shellItemHeight;

  if (widthDelta || heightDelta) {
    int32_t winWidth = 0;
    int32_t winHeight = 0;

    GetSize(&winWidth, &winHeight);
    // There's no point in trying to make the window smaller than the
    // desired content area size --- that's not likely to work. This whole
    // function assumes that the outer docshell is adding some constant
    // "border" chrome to the content area.
    winWidth = std::max(winWidth + widthDelta, aDesiredWidth);
    winHeight = std::max(winHeight + heightDelta, aDesiredHeight);
    SetSize(winWidth, winHeight, true);
  }
}

nsresult
nsXULWindow::GetTabCount(uint32_t* aResult)
{
  if (mXULBrowserWindow) {
    return mXULBrowserWindow->GetTabCount(aResult);
  }

  *aResult = 0;
  return NS_OK;
}

nsresult
nsXULWindow::GetNextTabParentId(uint64_t* aNextTabParentId)
{
  NS_ENSURE_ARG_POINTER(aNextTabParentId);
  *aNextTabParentId = mNextTabParentId;
  return NS_OK;
}
