/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 ci et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/MathAlgorithms.h"

// Local includes
#include "AppWindow.h"
#include <algorithm>

// Helper classes
#include "nsPrintfCString.h"
#include "nsString.h"
#include "nsWidgetsCID.h"
#include "nsThreadUtils.h"
#include "nsNetCID.h"
#include "nsQueryObject.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/Sprintf.h"

// Interfaces needed to be included
#include "nsGlobalWindowOuter.h"
#include "nsIAppShell.h"
#include "nsIAppShellService.h"
#include "nsIContentViewer.h"
#include "mozilla/dom/Document.h"
#include "nsPIDOMWindow.h"
#include "nsScreen.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIIOService.h"
#include "nsIObserverService.h"
#include "nsIOpenWindowInfo.h"
#include "nsIWindowMediator.h"
#include "nsIScreenManager.h"
#include "nsIScreen.h"
#include "nsIWindowWatcher.h"
#include "nsIURI.h"
#include "nsAppShellCID.h"
#include "nsReadableUtils.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsContentUtils.h"
#include "nsGlobalWindow.h"
#include "nsXULTooltipListener.h"
#include "nsXULPopupManager.h"
#include "nsFocusManager.h"
#include "nsContentList.h"
#include "nsIDOMWindowUtils.h"
#include "nsServiceManagerUtils.h"

#include "prenv.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/Services.h"
#include "mozilla/SpinEventLoopUntil.h"
#include "mozilla/dom/BarProps.h"
#include "mozilla/dom/DOMRect.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/ScriptSettings.h"
#include "mozilla/dom/BrowserHost.h"
#include "mozilla/dom/BrowserParent.h"
#include "mozilla/dom/LoadURIOptionsBinding.h"
#include "mozilla/intl/LocaleService.h"
#include "mozilla/EventDispatcher.h"

#ifdef XP_WIN
#  include "mozilla/PreXULSkeletonUI.h"
#  include "nsIWindowsUIUtils.h"
#endif

#ifdef MOZ_NEW_XULSTORE
#  include "mozilla/XULStore.h"
#endif

#include "mozilla/dom/DocumentL10n.h"

#ifdef XP_MACOSX
#  include "mozilla/widget/NativeMenuSupport.h"
#  define USE_NATIVE_MENUS
#endif

#define SIZEMODE_NORMAL u"normal"_ns
#define SIZEMODE_MAXIMIZED u"maximized"_ns
#define SIZEMODE_MINIMIZED u"minimized"_ns
#define SIZEMODE_FULLSCREEN u"fullscreen"_ns

#define WINDOWTYPE_ATTRIBUTE u"windowtype"_ns

#define PERSIST_ATTRIBUTE u"persist"_ns
#define SCREENX_ATTRIBUTE u"screenX"_ns
#define SCREENY_ATTRIBUTE u"screenY"_ns
#define WIDTH_ATTRIBUTE u"width"_ns
#define HEIGHT_ATTRIBUTE u"height"_ns
#define MODE_ATTRIBUTE u"sizemode"_ns
#define TILED_ATTRIBUTE u"gtktiledwindow"_ns
#define ZLEVEL_ATTRIBUTE u"zlevel"_ns

#define SIZE_PERSISTENCE_TIMEOUT 500  // msec

//*****************************************************************************
//***    AppWindow: Object Management
//*****************************************************************************

namespace mozilla {

using dom::AutoNoJSAPI;
using dom::BrowserHost;
using dom::BrowsingContext;
using dom::Document;
using dom::Element;
using dom::EventTarget;
using dom::LoadURIOptions;

AppWindow::AppWindow(uint32_t aChromeFlags)
    : mChromeTreeOwner(nullptr),
      mContentTreeOwner(nullptr),
      mPrimaryContentTreeOwner(nullptr),
      mModalStatus(NS_OK),
      mFullscreenChangeState(FullscreenChangeState::NotChanging),
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
      mSPTimerLock("AppWindow.mSPTimerLock"),
      mWidgetListenerDelegate(this) {}

AppWindow::~AppWindow() {
  {
    MutexAutoLock lock(mSPTimerLock);
    if (mSPTimer) mSPTimer->Cancel();
  }
  Destroy();
}

//*****************************************************************************
// AppWindow::nsISupports
//*****************************************************************************

NS_IMPL_ADDREF(AppWindow)
NS_IMPL_RELEASE(AppWindow)

NS_INTERFACE_MAP_BEGIN(AppWindow)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAppWindow)
  NS_INTERFACE_MAP_ENTRY(nsIAppWindow)
  NS_INTERFACE_MAP_ENTRY(nsIBaseWindow)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(AppWindow)
NS_INTERFACE_MAP_END

nsresult AppWindow::Initialize(nsIAppWindow* aParent, nsIAppWindow* aOpener,
                               int32_t aInitialWidth, int32_t aInitialHeight,
                               bool aIsHiddenWindow,
                               nsWidgetInitData& widgetInitData) {
  nsresult rv;
  nsCOMPtr<nsIWidget> parentWidget;

  mIsHiddenWindow = aIsHiddenWindow;

  int32_t initialX = 0, initialY = 0;
  nsCOMPtr<nsIBaseWindow> base(do_QueryInterface(aOpener));
  if (base) {
    int32_t x, y, width, height;
    rv = base->GetPositionAndSize(&x, &y, &width, &height);
    if (NS_FAILED(rv)) {
      mOpenerScreenRect.SetEmpty();
    } else {
      double scale;
      if (NS_SUCCEEDED(base->GetUnscaledDevicePixelsPerCSSPixel(&scale))) {
        mOpenerScreenRect.SetRect(
            NSToIntRound(x / scale), NSToIntRound(y / scale),
            NSToIntRound(width / scale), NSToIntRound(height / scale));
      } else {
        mOpenerScreenRect.SetRect(x, y, width, height);
      }
      initialX = mOpenerScreenRect.X();
      initialY = mOpenerScreenRect.Y();
      ConstrainToOpenerScreen(&initialX, &initialY);
    }
  }

  // XXX: need to get the default window size from prefs...
  // Doesn't come from prefs... will come from CSS/XUL/RDF
  DesktopIntRect deskRect(initialX, initialY, aInitialWidth, aInitialHeight);

  // Create top level window
  if (gfxPlatform::IsHeadless()) {
    mWindow = nsIWidget::CreateHeadlessWidget();
  } else {
    mWindow = nsIWidget::CreateTopLevelWindow();
  }
  if (!mWindow) {
    return NS_ERROR_FAILURE;
  }

  /* This next bit is troublesome. We carry two different versions of a pointer
     to our parent window. One is the parent window's widget, which is passed
     to our own widget. The other is a weak reference we keep here to our
     parent AppWindow. The former is useful to the widget, and we can't
     trust its treatment of the parent reference because they're platform-
     specific. The latter is useful to this class.
       A better implementation would be one in which the parent keeps strong
     references to its children and closes them before it allows itself
     to be closed. This would mimic the behaviour of OSes that support
     top-level child windows in OSes that do not. Later.
  */
  nsCOMPtr<nsIBaseWindow> parentAsWin(do_QueryInterface(aParent));
  if (parentAsWin) {
    parentAsWin->GetMainWidget(getter_AddRefs(parentWidget));
    mParentWindow = do_GetWeakReference(aParent);
  }

  mWindow->SetWidgetListener(&mWidgetListenerDelegate);
  rv = mWindow->Create((nsIWidget*)parentWidget,  // Parent nsIWidget
                       nullptr,                   // Native parent widget
                       deskRect,                  // Widget dimensions
                       &widgetInitData);          // Widget initialization data
  NS_ENSURE_SUCCESS(rv, rv);

  LayoutDeviceIntRect r = mWindow->GetClientBounds();
  // Match the default background color of content. Important on windows
  // since we no longer use content child widgets.
  mWindow->SetBackgroundColor(NS_RGB(255, 255, 255));

  // All Chrome BCs exist within the same BrowsingContextGroup, so we don't need
  // to pass in the opener window here. The opener is set later, if needed, by
  // nsWindowWatcher.
  RefPtr<BrowsingContext> browsingContext =
      BrowsingContext::CreateIndependent(BrowsingContext::Type::Chrome);

  // Create web shell
  mDocShell = nsDocShell::Create(browsingContext);
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  // Make sure to set the item type on the docshell _before_ calling
  // InitWindow() so it knows what type it is.
  NS_ENSURE_SUCCESS(EnsureChromeTreeOwner(), NS_ERROR_FAILURE);

  mDocShell->SetTreeOwner(mChromeTreeOwner);

  r.MoveTo(0, 0);
  NS_ENSURE_SUCCESS(mDocShell->InitWindow(nullptr, mWindow, r.X(), r.Y(),
                                          r.Width(), r.Height()),
                    NS_ERROR_FAILURE);

  // Attach a WebProgress listener.during initialization...
  mDocShell->AddProgressListener(this, nsIWebProgress::NOTIFY_STATE_NETWORK);

  mWindow->MaybeDispatchInitialFocusEvent();

  return rv;
}

//*****************************************************************************
// AppWindow::nsIIntefaceRequestor
//*****************************************************************************

NS_IMETHODIMP AppWindow::GetInterface(const nsIID& aIID, void** aSink) {
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
// AppWindow::nsIAppWindow
//*****************************************************************************

NS_IMETHODIMP AppWindow::GetDocShell(nsIDocShell** aDocShell) {
  NS_ENSURE_ARG_POINTER(aDocShell);

  *aDocShell = mDocShell;
  NS_IF_ADDREF(*aDocShell);
  return NS_OK;
}

NS_IMETHODIMP AppWindow::GetZLevel(uint32_t* outLevel) {
  nsCOMPtr<nsIWindowMediator> mediator(
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (mediator)
    mediator->GetZLevel(this, outLevel);
  else
    *outLevel = normalZ;
  return NS_OK;
}

NS_IMETHODIMP AppWindow::SetZLevel(uint32_t aLevel) {
  nsCOMPtr<nsIWindowMediator> mediator(
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (!mediator) return NS_ERROR_FAILURE;

  uint32_t zLevel;
  mediator->GetZLevel(this, &zLevel);
  if (zLevel == aLevel) return NS_OK;

  /* refuse to raise a maximized window above the normal browser level,
     for fear it could hide newly opened browser windows */
  if (aLevel > nsIAppWindow::normalZ && mWindow) {
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
    RefPtr<dom::Document> doc = cv->GetDocument();
    if (doc) {
      ErrorResult rv;
      RefPtr<dom::Event> event =
          doc->CreateEvent(u"Events"_ns, dom::CallerType::System, rv);
      if (event) {
        event->InitEvent(u"windowZLevel"_ns, true, false);

        event->SetTrusted(true);

        doc->DispatchEvent(*event);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP AppWindow::GetChromeFlags(uint32_t* aChromeFlags) {
  NS_ENSURE_ARG_POINTER(aChromeFlags);
  *aChromeFlags = mChromeFlags;
  return NS_OK;
}

NS_IMETHODIMP AppWindow::SetChromeFlags(uint32_t aChromeFlags) {
  NS_ASSERTION(!mChromeFlagsFrozen,
               "SetChromeFlags() after AssumeChromeFlagsAreFrozen()!");

  mChromeFlags = aChromeFlags;
  if (mChromeLoaded) {
    ApplyChromeFlags();
  }
  return NS_OK;
}

NS_IMETHODIMP AppWindow::AssumeChromeFlagsAreFrozen() {
  mChromeFlagsFrozen = true;
  return NS_OK;
}

NS_IMETHODIMP AppWindow::SetIntrinsicallySized(bool aIntrinsicallySized) {
  mIntrinsicallySized = aIntrinsicallySized;
  return NS_OK;
}

NS_IMETHODIMP AppWindow::GetIntrinsicallySized(bool* aIntrinsicallySized) {
  NS_ENSURE_ARG_POINTER(aIntrinsicallySized);

  *aIntrinsicallySized = mIntrinsicallySized;
  return NS_OK;
}

NS_IMETHODIMP AppWindow::GetPrimaryContentShell(
    nsIDocShellTreeItem** aDocShellTreeItem) {
  NS_ENSURE_ARG_POINTER(aDocShellTreeItem);
  NS_IF_ADDREF(*aDocShellTreeItem = mPrimaryContentShell);
  return NS_OK;
}

NS_IMETHODIMP
AppWindow::RemoteTabAdded(nsIRemoteTab* aTab, bool aPrimary) {
  if (aPrimary) {
    mPrimaryBrowserParent = aTab;
    mPrimaryContentShell = nullptr;
  } else if (mPrimaryBrowserParent == aTab) {
    mPrimaryBrowserParent = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
AppWindow::RemoteTabRemoved(nsIRemoteTab* aTab) {
  if (aTab == mPrimaryBrowserParent) {
    mPrimaryBrowserParent = nullptr;
  }

  return NS_OK;
}

NS_IMETHODIMP
AppWindow::GetPrimaryRemoteTab(nsIRemoteTab** aTab) {
  nsCOMPtr<nsIRemoteTab> tab = mPrimaryBrowserParent;
  tab.forget(aTab);
  return NS_OK;
}

static LayoutDeviceIntSize GetOuterToInnerSizeDifference(nsIWidget* aWindow) {
  if (!aWindow) {
    return LayoutDeviceIntSize();
  }
  LayoutDeviceIntSize baseSize(200, 200);
  LayoutDeviceIntSize windowSize = aWindow->ClientToWindowSize(baseSize);
  return windowSize - baseSize;
}

static CSSIntSize GetOuterToInnerSizeDifferenceInCSSPixels(nsIWidget* aWindow) {
  if (!aWindow) {
    return {};
  }
  LayoutDeviceIntSize devPixelSize = GetOuterToInnerSizeDifference(aWindow);
  return RoundedToInt(devPixelSize / aWindow->GetDefaultScale());
}

NS_IMETHODIMP
AppWindow::GetOuterToInnerHeightDifferenceInCSSPixels(uint32_t* aResult) {
  *aResult = GetOuterToInnerSizeDifferenceInCSSPixels(mWindow).height;
  return NS_OK;
}

NS_IMETHODIMP
AppWindow::GetOuterToInnerWidthDifferenceInCSSPixels(uint32_t* aResult) {
  *aResult = GetOuterToInnerSizeDifferenceInCSSPixels(mWindow).width;
  return NS_OK;
}

nsTArray<RefPtr<mozilla::LiveResizeListener>>
AppWindow::GetLiveResizeListeners() {
  nsTArray<RefPtr<mozilla::LiveResizeListener>> listeners;
  if (mPrimaryBrowserParent) {
    BrowserHost* host = BrowserHost::GetFrom(mPrimaryBrowserParent.get());
    listeners.AppendElement(host->GetActor());
  }
  return listeners;
}

NS_IMETHODIMP AppWindow::AddChildWindow(nsIAppWindow* aChild) {
  // we're not really keeping track of this right now
  return NS_OK;
}

NS_IMETHODIMP AppWindow::RemoveChildWindow(nsIAppWindow* aChild) {
  // we're not really keeping track of this right now
  return NS_OK;
}

NS_IMETHODIMP AppWindow::ShowModal() {
  AUTO_PROFILER_LABEL("AppWindow::ShowModal", OTHER);

  // Store locally so it doesn't die on us
  nsCOMPtr<nsIWidget> window = mWindow;
  nsCOMPtr<nsIAppWindow> tempRef = this;

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
// AppWindow::nsIBaseWindow
//*****************************************************************************

NS_IMETHODIMP AppWindow::InitWindow(nativeWindow aParentNativeWindow,
                                    nsIWidget* parentWidget, int32_t x,
                                    int32_t y, int32_t cx, int32_t cy) {
  // XXX First Check In
  NS_ASSERTION(false, "Not Yet Implemented");
  return NS_OK;
}

NS_IMETHODIMP AppWindow::Destroy() {
  nsCOMPtr<nsIAppWindow> kungFuDeathGrip(this);

  if (mDocShell) {
    mDocShell->RemoveProgressListener(this);
  }

  {
    MutexAutoLock lock(mSPTimerLock);
    if (mSPTimer) {
      mSPTimer->Cancel();
      SavePersistentAttributes();
      mSPTimer = nullptr;
    }
  }

  if (!mWindow) return NS_OK;

  // Ensure we don't reenter this code
  if (mDestroying) return NS_OK;

  mozilla::AutoRestore<bool> guard(mDestroying);
  mDestroying = true;

  nsCOMPtr<nsIAppShellService> appShell(
      do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
  NS_ASSERTION(appShell, "Couldn't get appShell... xpcom shutdown?");
  if (appShell)
    appShell->UnregisterTopLevelWindow(static_cast<nsIAppWindow*>(this));

  nsCOMPtr<nsIAppWindow> parentWindow(do_QueryReferent(mParentWindow));
  if (parentWindow) parentWindow->RemoveChildWindow(this);

  // Remove modality (if any) and hide while destroying. More than
  // a convenience, the hide prevents user interaction with the partially
  // destroyed window. This is especially necessary when the eldest window
  // in a stack of modal windows is destroyed first. It happens.
  ExitModalLoop(NS_OK);
  // XXX: Skip unmapping the window on Linux due to GLX hangs on the compositor
  // thread with NVIDIA driver 310.32. We don't need to worry about user
  // interactions with destroyed windows on X11 either.
#ifndef MOZ_WIDGET_GTK
  if (mWindow) mWindow->Show(false);
#endif

#if defined(XP_WIN)
  // We need to explicitly set the focus on Windows, but
  // only if the parent is visible.
  nsCOMPtr<nsIBaseWindow> parent(do_QueryReferent(mParentWindow));
  if (parent) {
    nsCOMPtr<nsIWidget> parentWidget;
    parent->GetMainWidget(getter_AddRefs(parentWidget));

    if (parentWidget && parentWidget->IsVisible()) {
      bool isParentHiddenWindow = false;

      if (appShell) {
        bool hasHiddenWindow = false;
        appShell->GetHasHiddenWindow(&hasHiddenWindow);
        if (hasHiddenWindow) {
          nsCOMPtr<nsIBaseWindow> baseHiddenWindow;
          nsCOMPtr<nsIAppWindow> hiddenWindow;
          appShell->GetHiddenWindow(getter_AddRefs(hiddenWindow));
          if (hiddenWindow) {
            baseHiddenWindow = do_GetInterface(hiddenWindow);
            isParentHiddenWindow = (baseHiddenWindow == parent);
          }
        }
      }

      // somebody screwed up somewhere. hiddenwindow shouldn't be anybody's
      // parent. still, when it happens, skip activating it.
      if (!isParentHiddenWindow) {
        parentWidget->PlaceBehind(eZPlacementTop, 0, true);
      }
    }
  }
#endif

  RemoveTooltipSupport();

  mDOMWindow = nullptr;
  if (mDocShell) {
    RefPtr<BrowsingContext> bc(mDocShell->GetBrowsingContext());
    mDocShell->Destroy();
    bc->Detach();
    mDocShell = nullptr;  // this can cause reentrancy of this function
  }

  mPrimaryContentShell = nullptr;

  if (mContentTreeOwner) {
    mContentTreeOwner->AppWindow(nullptr);
    NS_RELEASE(mContentTreeOwner);
  }
  if (mPrimaryContentTreeOwner) {
    mPrimaryContentTreeOwner->AppWindow(nullptr);
    NS_RELEASE(mPrimaryContentTreeOwner);
  }
  if (mChromeTreeOwner) {
    mChromeTreeOwner->AppWindow(nullptr);
    NS_RELEASE(mChromeTreeOwner);
  }
  if (mWindow) {
    mWindow->SetWidgetListener(nullptr);  // nsWebShellWindow hackery
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

NS_IMETHODIMP AppWindow::GetDevicePixelsPerDesktopPixel(double* aScale) {
  *aScale = mWindow ? mWindow->GetDesktopToDeviceScale().scale : 1.0;
  return NS_OK;
}

NS_IMETHODIMP AppWindow::GetUnscaledDevicePixelsPerCSSPixel(double* aScale) {
  *aScale = mWindow ? mWindow->GetDefaultScale().scale : 1.0;
  return NS_OK;
}

NS_IMETHODIMP AppWindow::SetPositionDesktopPix(int32_t aX, int32_t aY) {
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
NS_IMETHODIMP AppWindow::SetPosition(int32_t aX, int32_t aY) {
  // Don't reset the window's size mode here - platforms that don't want to move
  // maximized windows should reset it in their respective Move implementation.
  DesktopToLayoutDeviceScale currScale = mWindow->GetDesktopToDeviceScale();
  DesktopPoint pos = LayoutDeviceIntPoint(aX, aY) / currScale;
  return SetPositionDesktopPix(pos.x, pos.y);
}

NS_IMETHODIMP AppWindow::GetPosition(int32_t* aX, int32_t* aY) {
  return GetPositionAndSize(aX, aY, nullptr, nullptr);
}

NS_IMETHODIMP AppWindow::SetSize(int32_t aCX, int32_t aCY, bool aRepaint) {
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

NS_IMETHODIMP AppWindow::GetSize(int32_t* aCX, int32_t* aCY) {
  return GetPositionAndSize(nullptr, nullptr, aCX, aCY);
}

NS_IMETHODIMP AppWindow::SetPositionAndSize(int32_t aX, int32_t aY, int32_t aCX,
                                            int32_t aCY, uint32_t aFlags) {
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

NS_IMETHODIMP AppWindow::GetPositionAndSize(int32_t* x, int32_t* y, int32_t* cx,
                                            int32_t* cy) {
  if (!mWindow) return NS_ERROR_FAILURE;

  LayoutDeviceIntRect rect = mWindow->GetScreenBounds();

  if (x) *x = rect.X();
  if (y) *y = rect.Y();
  if (cx) *cx = rect.Width();
  if (cy) *cy = rect.Height();

  return NS_OK;
}

NS_IMETHODIMP AppWindow::Center(nsIAppWindow* aRelative, bool aScreen,
                                bool aAlert) {
  int32_t left, top, width, height, ourWidth, ourHeight;
  bool screenCoordinates = false, windowCoordinates = false;
  nsresult result;

  if (!mChromeLoaded) {
    // note we lose the parameters. at time of writing, this isn't a problem.
    mCenterAfterLoad = true;
    return NS_OK;
  }

  if (!aScreen && !aRelative) return NS_ERROR_INVALID_ARG;

  nsCOMPtr<nsIScreenManager> screenmgr =
      do_GetService("@mozilla.org/gfx/screenmanager;1", &result);
  if (NS_FAILED(result)) return result;

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
          screenmgr->ScreenForRect(left, top, width, height,
                                   getter_AddRefs(screen));
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
                               mOpenerScreenRect.Width(),
                               mOpenerScreenRect.Height(),
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

NS_IMETHODIMP AppWindow::Repaint(bool aForce) {
  // XXX First Check In
  NS_ASSERTION(false, "Not Yet Implemented");
  return NS_OK;
}

NS_IMETHODIMP AppWindow::GetParentWidget(nsIWidget** aParentWidget) {
  NS_ENSURE_ARG_POINTER(aParentWidget);
  NS_ENSURE_STATE(mWindow);

  NS_IF_ADDREF(*aParentWidget = mWindow->GetParent());
  return NS_OK;
}

NS_IMETHODIMP AppWindow::SetParentWidget(nsIWidget* aParentWidget) {
  // XXX First Check In
  NS_ASSERTION(false, "Not Yet Implemented");
  return NS_OK;
}

NS_IMETHODIMP AppWindow::GetParentNativeWindow(
    nativeWindow* aParentNativeWindow) {
  NS_ENSURE_ARG_POINTER(aParentNativeWindow);

  nsCOMPtr<nsIWidget> parentWidget;
  NS_ENSURE_SUCCESS(GetParentWidget(getter_AddRefs(parentWidget)),
                    NS_ERROR_FAILURE);

  if (parentWidget) {
    *aParentNativeWindow = parentWidget->GetNativeData(NS_NATIVE_WIDGET);
  }

  return NS_OK;
}

NS_IMETHODIMP AppWindow::SetParentNativeWindow(
    nativeWindow aParentNativeWindow) {
  // XXX First Check In
  NS_ASSERTION(false, "Not Yet Implemented");
  return NS_OK;
}

NS_IMETHODIMP AppWindow::GetNativeHandle(nsAString& aNativeHandle) {
  nsCOMPtr<nsIWidget> mainWidget;
  NS_ENSURE_SUCCESS(GetMainWidget(getter_AddRefs(mainWidget)),
                    NS_ERROR_FAILURE);

  if (mainWidget) {
    nativeWindow nativeWindowPtr = mainWidget->GetNativeData(NS_NATIVE_WINDOW);
    /* the nativeWindow pointer is converted to and exposed as a string. This
       is a more reliable way not to lose information (as opposed to JS
       |Number| for instance) */
    aNativeHandle =
        NS_ConvertASCIItoUTF16(nsPrintfCString("0x%p", nativeWindowPtr));
  }

  return NS_OK;
}

NS_IMETHODIMP AppWindow::GetVisibility(bool* aVisibility) {
  NS_ENSURE_ARG_POINTER(aVisibility);

  // Always claim to be visible for now. See bug
  // https://bugzilla.mozilla.org/show_bug.cgi?id=306245.

  *aVisibility = true;

  return NS_OK;
}

NS_IMETHODIMP AppWindow::SetVisibility(bool aVisibility) {
  if (!mChromeLoaded) {
    mShowAfterLoad = aVisibility;
    return NS_OK;
  }

  if (mDebuting) {
    return NS_OK;
  }
  mDebuting = true;  // (Show / Focus is recursive)

  // XXXTAB Do we really need to show docshell and the window?  Isn't
  // the window good enough?
  mDocShell->SetVisibility(aVisibility);
  // Store locally so it doesn't die on us. 'Show' can result in the window
  // being closed with AppWindow::Destroy being called. That would set
  // mWindow to null and posibly destroy the nsIWidget while its Show method
  // is on the stack. We need to keep it alive until Show finishes.
  nsCOMPtr<nsIWidget> window = mWindow;
  window->Show(aVisibility);

  nsCOMPtr<nsIWindowMediator> windowMediator(
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (windowMediator)
    windowMediator->UpdateWindowTimeStamp(static_cast<nsIAppWindow*>(this));

  // notify observers so that we can hide the splash screen if possible
  nsCOMPtr<nsIObserverService> obssvc = services::GetObserverService();
  NS_ASSERTION(obssvc, "Couldn't get observer service.");
  if (obssvc) {
    obssvc->NotifyObservers(static_cast<nsIAppWindow*>(this),
                            "xul-window-visible", nullptr);
  }

  mDebuting = false;
  return NS_OK;
}

NS_IMETHODIMP AppWindow::GetEnabled(bool* aEnabled) {
  NS_ENSURE_ARG_POINTER(aEnabled);

  if (mWindow) {
    *aEnabled = mWindow->IsEnabled();
    return NS_OK;
  }

  *aEnabled = true;  // better guess than most
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP AppWindow::SetEnabled(bool aEnable) {
  if (mWindow) {
    mWindow->Enable(aEnable);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP AppWindow::GetMainWidget(nsIWidget** aMainWidget) {
  NS_ENSURE_ARG_POINTER(aMainWidget);

  *aMainWidget = mWindow;
  NS_IF_ADDREF(*aMainWidget);
  return NS_OK;
}

NS_IMETHODIMP AppWindow::SetFocus() {
  // XXX First Check In
  NS_ASSERTION(false, "Not Yet Implemented");
  return NS_OK;
}

NS_IMETHODIMP AppWindow::GetTitle(nsAString& aTitle) {
  aTitle = mTitle;
  return NS_OK;
}

NS_IMETHODIMP AppWindow::SetTitle(const nsAString& aTitle) {
  NS_ENSURE_STATE(mWindow);
  mTitle.Assign(aTitle);
  mTitle.StripCRLF();
  NS_ENSURE_SUCCESS(mWindow->SetTitle(mTitle), NS_ERROR_FAILURE);
  return NS_OK;
}

//*****************************************************************************
// AppWindow: Helpers
//*****************************************************************************

NS_IMETHODIMP AppWindow::EnsureChromeTreeOwner() {
  if (mChromeTreeOwner) return NS_OK;

  mChromeTreeOwner = new nsChromeTreeOwner();
  NS_ADDREF(mChromeTreeOwner);
  mChromeTreeOwner->AppWindow(this);

  return NS_OK;
}

NS_IMETHODIMP AppWindow::EnsureContentTreeOwner() {
  if (mContentTreeOwner) return NS_OK;

  mContentTreeOwner = new nsContentTreeOwner(false);
  NS_ADDREF(mContentTreeOwner);
  mContentTreeOwner->AppWindow(this);

  return NS_OK;
}

NS_IMETHODIMP AppWindow::EnsurePrimaryContentTreeOwner() {
  if (mPrimaryContentTreeOwner) return NS_OK;

  mPrimaryContentTreeOwner = new nsContentTreeOwner(true);
  NS_ADDREF(mPrimaryContentTreeOwner);
  mPrimaryContentTreeOwner->AppWindow(this);

  return NS_OK;
}

NS_IMETHODIMP AppWindow::EnsurePrompter() {
  if (mPrompter) return NS_OK;

  nsCOMPtr<mozIDOMWindowProxy> ourWindow;
  nsresult rv = GetWindowDOMWindow(getter_AddRefs(ourWindow));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIWindowWatcher> wwatch =
        do_GetService(NS_WINDOWWATCHER_CONTRACTID);
    if (wwatch) wwatch->GetNewPrompter(ourWindow, getter_AddRefs(mPrompter));
  }
  return mPrompter ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP AppWindow::EnsureAuthPrompter() {
  if (mAuthPrompter) return NS_OK;

  nsCOMPtr<mozIDOMWindowProxy> ourWindow;
  nsresult rv = GetWindowDOMWindow(getter_AddRefs(ourWindow));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIWindowWatcher> wwatch(
        do_GetService(NS_WINDOWWATCHER_CONTRACTID));
    if (wwatch)
      wwatch->GetNewAuthPrompter(ourWindow, getter_AddRefs(mAuthPrompter));
  }
  return mAuthPrompter ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP AppWindow::GetAvailScreenSize(int32_t* aAvailWidth,
                                            int32_t* aAvailHeight) {
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
NS_IMETHODIMP AppWindow::ForceRoundedDimensions() {
  if (mIsHiddenWindow) {
    return NS_OK;
  }

  int32_t availWidthCSS = 0;
  int32_t availHeightCSS = 0;
  int32_t contentWidthCSS = 0;
  int32_t contentHeightCSS = 0;
  int32_t windowWidthCSS = 0;
  int32_t windowHeightCSS = 0;
  double devicePerCSSPixels = 1.0;

  GetUnscaledDevicePixelsPerCSSPixel(&devicePerCSSPixels);

  GetAvailScreenSize(&availWidthCSS, &availHeightCSS);

  // To get correct chrome size, we have to resize the window to a proper
  // size first. So, here, we size it to its available size.
  SetSpecifiedSize(availWidthCSS, availHeightCSS);

  // Get the current window size for calculating chrome UI size.
  GetSize(&windowWidthCSS, &windowHeightCSS);  // device pixels
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
      chromeWidth, chromeHeight, availWidthCSS, availHeightCSS, availWidthCSS,
      availHeightCSS,
      false,  // aSetOuterWidth
      false,  // aSetOuterHeight
      &targetContentWidth, &targetContentHeight);

  targetContentWidth = NSToIntRound(targetContentWidth * devicePerCSSPixels);
  targetContentHeight = NSToIntRound(targetContentHeight * devicePerCSSPixels);

  SetPrimaryContentSize(targetContentWidth, targetContentHeight);

  return NS_OK;
}

void AppWindow::OnChromeLoaded() {
  nsresult rv = EnsureContentTreeOwner();

  if (NS_SUCCEEDED(rv)) {
    mChromeLoaded = true;
    ApplyChromeFlags();
    SyncAttributesToWidget();
    if (mWindow) {
      SizeShell();
      if (mShowAfterLoad) {
        SetVisibility(true);
      }
      AddTooltipSupport();
    }
    // At this point the window may have been closed already during Show() or
    // SyncAttributesToWidget(), so AppWindow::Destroy may already have been
    // called. Take care!
  }
  mPersistentAttributesMask |= PAD_POSITION | PAD_SIZE | PAD_MISC;
}

bool AppWindow::NeedsTooltipListener() {
  nsCOMPtr<dom::Element> docShellElement = GetWindowDOMElement();
  if (!docShellElement || docShellElement->IsXULElement()) {
    // Tooltips in XUL are handled by each element.
    return false;
  }
  // All other non-XUL document types need a tooltip listener.
  return true;
}

void AppWindow::AddTooltipSupport() {
  if (!NeedsTooltipListener()) {
    return;
  }
  nsXULTooltipListener* listener = nsXULTooltipListener::GetInstance();
  if (!listener) {
    return;
  }

  nsCOMPtr<dom::Element> docShellElement = GetWindowDOMElement();
  MOZ_ASSERT(docShellElement);
  listener->AddTooltipSupport(docShellElement);
}

void AppWindow::RemoveTooltipSupport() {
  if (!NeedsTooltipListener()) {
    return;
  }
  nsXULTooltipListener* listener = nsXULTooltipListener::GetInstance();
  if (!listener) {
    return;
  }

  nsCOMPtr<dom::Element> docShellElement = GetWindowDOMElement();
  MOZ_ASSERT(docShellElement);
  listener->RemoveTooltipSupport(docShellElement);
}

// If aSpecWidth and/or aSpecHeight are > 0, we will use these CSS px sizes
// to fit to the screen when staggering windows; if they're negative,
// we use the window's current size instead.
bool AppWindow::LoadPositionFromXUL(int32_t aSpecWidth, int32_t aSpecHeight) {
  bool gotPosition = false;

  // if we're the hidden window, don't try to validate our size/position. We're
  // special.
  if (mIsHiddenWindow) return false;

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

  bool allowSlop = false;
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
    } else {
      StaggerPosition(specX, specY, cssWidth, cssHeight);
      allowSlop = true;
    }
  }
  mWindow->ConstrainPosition(allowSlop, &specX, &specY);
  if (specX != currX || specY != currY) {
    SetPositionDesktopPix(specX, specY);
  }

  return gotPosition;
}

static Maybe<int32_t> ReadIntAttribute(const Element& aElement, nsAtom* aAtom) {
  nsAutoString attrString;
  if (!aElement.GetAttr(kNameSpaceID_None, aAtom, attrString)) {
    return Nothing();
  }

  nsresult res = NS_OK;
  int32_t ret = attrString.ToInteger(&res);
  return NS_SUCCEEDED(res) ? Some(ret) : Nothing();
}

static Maybe<int32_t> ReadSize(const Element& aElement, nsAtom* aAttr,
                               nsAtom* aMinAttr, nsAtom* aMaxAttr) {
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

bool AppWindow::LoadSizeFromXUL(int32_t& aSpecWidth, int32_t& aSpecHeight) {
  bool gotSize = false;

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

  if (auto width = ReadSize(*windowElement, nsGkAtoms::width,
                            nsGkAtoms::minwidth, nsGkAtoms::maxwidth)) {
    aSpecWidth = *width;
    gotSize = true;
  }

  if (auto height = ReadSize(*windowElement, nsGkAtoms::height,
                             nsGkAtoms::minheight, nsGkAtoms::maxheight)) {
    aSpecHeight = *height;
    gotSize = true;
  }

  return gotSize;
}

void AppWindow::SetSpecifiedSize(int32_t aSpecWidth, int32_t aSpecHeight) {
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
  GetSize(&currWidth, &currHeight);  // returns device pixels

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
bool AppWindow::UpdateWindowStateFromMiscXULAttributes() {
  bool gotState = false;

  /* There are no misc attributes of interest to the hidden window.
     It's especially important not to try to validate that window's
     size or position, because some platforms (Mac OS X) need to
     make it visible and offscreen. */
  if (mIsHiddenWindow) return false;

  nsCOMPtr<dom::Element> windowElement = GetWindowDOMElement();
  NS_ENSURE_TRUE(windowElement, false);

  nsAutoString stateString;
  nsSizeMode sizeMode = nsSizeMode_Normal;

  // If we are told to ignore the size mode attribute, force
  // normal sizemode.
  if (mIgnoreXULSizeMode) {
    windowElement->SetAttribute(MODE_ATTRIBUTE, SIZEMODE_NORMAL,
                                IgnoreErrors());
  } else {
    // Otherwise, read sizemode from DOM and, if the window is resizable,
    // set it later.
    windowElement->GetAttribute(MODE_ATTRIBUTE, stateString);
    if ((stateString.Equals(SIZEMODE_MAXIMIZED) ||
         stateString.Equals(SIZEMODE_FULLSCREEN))) {
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
void AppWindow::StaggerPosition(int32_t& aRequestedX, int32_t& aRequestedY,
                                int32_t aSpecWidth, int32_t aSpecHeight) {
  // These "constants" will be converted from CSS to desktop pixels
  // for the appropriate screen, assuming we find a screen to use...
  // hence they're not actually declared const here.
  int32_t kOffset = 22;
  uint32_t kSlop = 4;

  bool keepTrying;
  int bouncedX = 0,  // bounced off vertical edge of screen
      bouncedY = 0;  // bounced off horizontal edge

  // look for any other windows of this type
  nsCOMPtr<nsIWindowMediator> wm(do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (!wm) return;

  nsCOMPtr<dom::Element> windowElement = GetWindowDOMElement();
  if (!windowElement) return;

  nsCOMPtr<nsIAppWindow> ourAppWindow(this);

  nsAutoString windowType;
  windowElement->GetAttribute(WINDOWTYPE_ATTRIBUTE, windowType);

  int32_t screenTop = 0,  // it's pointless to initialize these ...
      screenRight = 0,    // ... but to prevent oversalubrious and ...
      screenBottom = 0,   // ... underbright compilers from ...
      screenLeft = 0;     // ... issuing warnings.
  bool gotScreen = false;

  {  // fetch screen coordinates
    nsCOMPtr<nsIScreenManager> screenMgr(
        do_GetService("@mozilla.org/gfx/screenmanager;1"));
    if (screenMgr) {
      nsCOMPtr<nsIScreen> ourScreen;
      // the coordinates here are already display pixels
      screenMgr->ScreenForRect(aRequestedX, aRequestedY, aSpecWidth,
                               aSpecHeight, getter_AddRefs(ourScreen));
      if (ourScreen) {
        int32_t screenWidth, screenHeight;
        ourScreen->GetAvailRectDisplayPix(&screenLeft, &screenTop, &screenWidth,
                                          &screenHeight);
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
    wm->GetAppWindowEnumerator(windowType.get(), getter_AddRefs(windowList));

    if (!windowList) break;

    // One full pass through all windows of this type, offset and stop on
    // collision.
    do {
      bool more;
      windowList->HasMoreElements(&more);
      if (!more) break;

      nsCOMPtr<nsISupports> supportsWindow;
      windowList->GetNext(getter_AddRefs(supportsWindow));

      nsCOMPtr<nsIAppWindow> listAppWindow(do_QueryInterface(supportsWindow));
      if (listAppWindow != ourAppWindow) {
        int32_t listX, listY;
        nsCOMPtr<nsIBaseWindow> listBaseWindow(
            do_QueryInterface(supportsWindow));
        listBaseWindow->GetPosition(&listX, &listY);
        double scale;
        if (NS_SUCCEEDED(
                listBaseWindow->GetDevicePixelsPerDesktopPixel(&scale))) {
          listX = NSToIntRound(listX / scale);
          listY = NSToIntRound(listY / scale);
        }

        if (Abs(listX - aRequestedX) <= kSlop &&
            Abs(listY - aRequestedY) <= kSlop) {
          // collision! offset and start over
          if (bouncedX & 0x1)
            aRequestedX -= kOffset;
          else
            aRequestedX += kOffset;
          aRequestedY += kOffset;

          if (gotScreen) {
            // if we're moving to the right and we need to bounce...
            if (!(bouncedX & 0x1) &&
                ((aRequestedX + aSpecWidth) > screenRight)) {
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
    } while (true);
  } while (keepTrying);
}

void AppWindow::SyncAttributesToWidget() {
  nsCOMPtr<dom::Element> windowElement = GetWindowDOMElement();
  if (!windowElement) return;

  MOZ_DIAGNOSTIC_ASSERT(mWindow, "No widget on SyncAttributesToWidget?");

  nsAutoString attr;

  // "hidechrome" attribute
  if (windowElement->AttrValueIs(kNameSpaceID_None, nsGkAtoms::hidechrome,
                                 nsGkAtoms::_true, eCaseMatters)) {
    mWindow->HideWindowChrome(true);
  }

  NS_ENSURE_TRUE_VOID(mWindow);

  // "chromemargin" attribute
  nsIntMargin margins;
  windowElement->GetAttribute(u"chromemargin"_ns, attr);
  if (nsContentUtils::ParseIntMarginValue(attr, margins)) {
    LayoutDeviceIntMargin tmp =
        LayoutDeviceIntMargin::FromUnknownMargin(margins);
    mWindow->SetNonClientMargins(tmp);
  }

  NS_ENSURE_TRUE_VOID(mWindow);

  // "windowtype" attribute
  windowElement->GetAttribute(WINDOWTYPE_ATTRIBUTE, attr);
  if (!attr.IsEmpty()) {
    mWindow->SetWindowClass(attr);
  }

  NS_ENSURE_TRUE_VOID(mWindow);

  // "icon" attribute
  windowElement->GetAttribute(u"icon"_ns, attr);
  if (!attr.IsEmpty()) {
    mWindow->SetIcon(attr);

    NS_ENSURE_TRUE_VOID(mWindow);
  }

  // "drawtitle" attribute
  windowElement->GetAttribute(u"drawtitle"_ns, attr);
  mWindow->SetDrawsTitle(attr.LowerCaseEqualsLiteral("true"));

  NS_ENSURE_TRUE_VOID(mWindow);

  // "toggletoolbar" attribute
  windowElement->GetAttribute(u"toggletoolbar"_ns, attr);
  mWindow->SetShowsToolbarButton(attr.LowerCaseEqualsLiteral("true"));

  NS_ENSURE_TRUE_VOID(mWindow);

  // "macnativefullscreen" attribute
  windowElement->GetAttribute(u"macnativefullscreen"_ns, attr);
  mWindow->SetSupportsNativeFullscreen(attr.LowerCaseEqualsLiteral("true"));

  NS_ENSURE_TRUE_VOID(mWindow);

  // "macanimationtype" attribute
  windowElement->GetAttribute(u"macanimationtype"_ns, attr);
  if (attr.EqualsLiteral("document")) {
    mWindow->SetWindowAnimationType(nsIWidget::eDocumentWindowAnimation);
  }
}

enum class ConversionDirection {
  InnerToOuter,
  OuterToInner,
};

static void ConvertWindowSize(nsIAppWindow* aWin, const nsAtom* aAttr,
                              ConversionDirection aDirection,
                              nsAString& aInOutString) {
  MOZ_ASSERT(aWin);
  MOZ_ASSERT(aAttr == nsGkAtoms::width || aAttr == nsGkAtoms::height);

  nsresult rv;
  int32_t size = aInOutString.ToInteger(&rv);
  if (NS_FAILED(rv)) {
    return;
  }

  int32_t sizeDiff = aAttr == nsGkAtoms::width
                         ? aWin->GetOuterToInnerWidthDifferenceInCSSPixels()
                         : aWin->GetOuterToInnerHeightDifferenceInCSSPixels();

  if (!sizeDiff) {
    return;
  }

  int32_t multiplier = aDirection == ConversionDirection::InnerToOuter ? 1 : -1;

  CopyASCIItoUTF16(nsPrintfCString("%d", size + multiplier * sizeDiff),
                   aInOutString);
}

nsresult AppWindow::GetPersistentValue(const nsAtom* aAttr, nsAString& aValue) {
  nsCOMPtr<dom::Element> docShellElement = GetWindowDOMElement();
  if (!docShellElement) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString windowElementId;
  docShellElement->GetId(windowElementId);
  // Elements must have an ID to be persisted.
  if (windowElementId.IsEmpty()) {
    return NS_OK;
  }

  RefPtr<dom::Document> ownerDoc = docShellElement->OwnerDoc();
  nsIURI* docURI = ownerDoc->GetDocumentURI();
  if (!docURI) {
    return NS_ERROR_FAILURE;
  }
  nsAutoCString utf8uri;
  nsresult rv = docURI->GetSpec(utf8uri);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ConvertUTF8toUTF16 uri(utf8uri);

#ifdef MOZ_NEW_XULSTORE
  nsDependentAtomString attrString(aAttr);
  rv = XULStore::GetValue(uri, windowElementId, attrString, aValue);
#else
  if (!mLocalStore) {
    mLocalStore = do_GetService("@mozilla.org/xul/xulstore;1");
    if (NS_WARN_IF(!mLocalStore)) {
      return NS_ERROR_NOT_INITIALIZED;
    }
  }

  rv = mLocalStore->GetValue(uri, windowElementId, nsDependentAtomString(aAttr),
                             aValue);
#endif
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (aAttr == nsGkAtoms::width || aAttr == nsGkAtoms::height) {
    // Convert attributes from outer size to inner size for top-level
    // windows, see bug 1444525 & co.
    ConvertWindowSize(this, aAttr, ConversionDirection::OuterToInner, aValue);
  }

  return NS_OK;
}

nsresult AppWindow::GetDocXulStoreKeys(nsString& aUriSpec,
                                       nsString& aWindowElementId) {
  nsCOMPtr<dom::Element> docShellElement = GetWindowDOMElement();
  if (!docShellElement) {
    return NS_ERROR_FAILURE;
  }

  docShellElement->GetId(aWindowElementId);
  // Match the behavior of XULPersist and only persist values if the element
  // has an ID.
  if (aWindowElementId.IsEmpty()) {
    return NS_OK;
  }

  RefPtr<dom::Document> ownerDoc = docShellElement->OwnerDoc();
  nsIURI* docURI = ownerDoc->GetDocumentURI();
  if (!docURI) {
    return NS_ERROR_FAILURE;
  }

  nsAutoCString utf8uri;
  nsresult rv = docURI->GetSpec(utf8uri);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  aUriSpec = NS_ConvertUTF8toUTF16(utf8uri);

  return NS_OK;
}

nsresult AppWindow::MaybeSaveEarlyWindowPersistentValues(
    const LayoutDeviceIntRect& aRect) {
#ifdef XP_WIN
  nsAutoString uri;
  nsAutoString windowElementId;
  nsresult rv = GetDocXulStoreKeys(uri, windowElementId);

  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  if (!windowElementId.EqualsLiteral("main-window") ||
      !uri.EqualsLiteral("chrome://browser/content/browser.xhtml")) {
    return NS_OK;
  }

  SkeletonUISettings settings;

  settings.screenX = aRect.X();
  settings.screenY = aRect.Y();
  settings.width = aRect.Width();
  settings.height = aRect.Height();

  settings.maximized = mWindow->SizeMode() == nsSizeMode_Maximized;
  settings.cssToDevPixelScaling = mWindow->GetDefaultScale().scale;

  nsCOMPtr<dom::Element> windowElement = GetWindowDOMElement();
  Document* doc = windowElement->GetComposedDoc();
  Element* urlbarEl = doc->GetElementById(u"urlbar"_ns);

  nsCOMPtr<nsPIDOMWindowOuter> window = mDocShell->GetWindow();
  nsCOMPtr<nsIDOMWindowUtils> utils =
      nsGlobalWindowOuter::Cast(window)->WindowUtils();
  RefPtr<dom::DOMRect> urlbarRect;
  rv = utils->GetBoundsWithoutFlushing(urlbarEl, getter_AddRefs(urlbarRect));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  double urlbarX = urlbarRect->X();
  double urlbarWidth = urlbarRect->Width();

  // Hard-coding the following values and this behavior in general is rather
  // fragile, and can easily get out of sync with the actual front-end values.
  // This is not intended as a long-term solution, but only as the relatively
  // straightforward implementation of an experimental feature. If we want to
  // ship the skeleton UI to all users, we should strongly consider a more
  // robust solution than this. The vertical position of the urlbar will be
  // fixed.
  nsAutoString attributeValue;
  urlbarEl->GetAttribute(u"breakout-extend"_ns, attributeValue);
  // Scale down the urlbar if it is focused
  if (attributeValue.EqualsLiteral("true")) {
    // defined in browser.inc.css as 2px
    int urlbarBreakoutExtend = 2;
    // defined in urlbar-searchbar.inc.css as 5px
    int urlbarMarginInline = 5;

    // breakout-extend measurements are defined in urlbar-searchbar.inc.css
    urlbarX += (double)(urlbarBreakoutExtend + urlbarMarginInline);
    urlbarWidth -= (double)(2 * (urlbarBreakoutExtend + urlbarMarginInline));
  }
  CSSPixelSpan urlbar;
  urlbar.start = urlbarX;
  urlbar.end = urlbar.start + urlbarWidth;
  settings.urlbarSpan = urlbar;

  Element* navbar = doc->GetElementById(u"nav-bar"_ns);

  Element* searchbarEl = doc->GetElementById(u"searchbar"_ns);
  CSSPixelSpan searchbar;
  if (navbar->Contains(searchbarEl)) {
    RefPtr<dom::DOMRect> searchbarRect;
    rv = utils->GetBoundsWithoutFlushing(searchbarEl,
                                         getter_AddRefs(searchbarRect));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    searchbar.start = searchbarRect->X();
    searchbar.end = searchbar.start + searchbarRect->Width();
  } else {
    // There is no searchbar in the UI
    searchbar.start = 0;
    searchbar.end = 0;
  }
  settings.searchbarSpan = searchbar;

  nsAutoString bookmarksVisibility;
  Preferences::GetString("browser.toolbars.bookmarks.visibility",
                         bookmarksVisibility);
  settings.bookmarksToolbarShown =
      bookmarksVisibility.EqualsLiteral("always") ||
      (Preferences::GetBool("browser.toolbars.bookmarks.2h2020", false) &&
       bookmarksVisibility.EqualsLiteral("newtab"));

  Element* menubar = doc->GetElementById(u"toolbar-menubar"_ns);
  menubar->GetAttribute(u"autohide"_ns, attributeValue);
  settings.menubarShown = attributeValue.EqualsLiteral("false");

  ErrorResult err;
  nsCOMPtr<nsIHTMLCollection> toolbarSprings = navbar->GetElementsByTagNameNS(
      u"http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"_ns,
      u"toolbarspring"_ns, err);
  if (err.Failed()) {
    return NS_ERROR_FAILURE;
  }
  mozilla::Vector<CSSPixelSpan> springs;
  for (int i = 0; i < toolbarSprings->Length(); i++) {
    RefPtr<Element> springEl = toolbarSprings->Item(i);
    RefPtr<dom::DOMRect> springRect;
    rv = utils->GetBoundsWithoutFlushing(springEl, getter_AddRefs(springRect));
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }
    CSSPixelSpan spring;
    spring.start = springRect->X();
    spring.end = spring.start + springRect->Width();
    if (!settings.springs.append(spring)) {
      return NS_ERROR_FAILURE;
    }
  }

  settings.rtlEnabled = intl::LocaleService::GetInstance()->IsAppLocaleRTL();

  bool isInTabletMode = false;
  bool autoTouchModePref =
      Preferences::GetBool("browser.touchmode.auto", false);
  if (autoTouchModePref) {
    nsCOMPtr<nsIWindowsUIUtils> uiUtils(
        do_GetService("@mozilla.org/windows-ui-utils;1"));
    if (!NS_WARN_IF(!uiUtils)) {
      uiUtils->GetInTabletMode(&isInTabletMode);
    }
  }

  if (isInTabletMode) {
    settings.uiDensity = SkeletonUIDensity::Touch;
  } else {
    int uiDensityPref = Preferences::GetInt("browser.uidensity", 0);
    switch (uiDensityPref) {
      case 0: {
        settings.uiDensity = SkeletonUIDensity::Default;
        break;
      }
      case 1: {
        settings.uiDensity = SkeletonUIDensity::Compact;
        break;
      }
      case 2: {
        settings.uiDensity = SkeletonUIDensity::Touch;
        break;
      }
    }
  }

  Unused << PersistPreXULSkeletonUIValues(settings);
#endif

  return NS_OK;
}

nsresult AppWindow::SetPersistentValue(const nsAtom* aAttr,
                                       const nsAString& aValue) {
  nsAutoString uri;
  nsAutoString windowElementId;
  nsresult rv = GetDocXulStoreKeys(uri, windowElementId);

  if (NS_FAILED(rv) || windowElementId.IsEmpty()) {
    return rv;
  }

  nsAutoString maybeConvertedValue(aValue);
  if (aAttr == nsGkAtoms::width || aAttr == nsGkAtoms::height) {
    // Make sure we store the <window> attributes as outer window size, see
    // bug 1444525 & co.
    ConvertWindowSize(this, aAttr, ConversionDirection::InnerToOuter,
                      maybeConvertedValue);
  }

#ifdef MOZ_NEW_XULSTORE
  nsDependentAtomString attrString(aAttr);
  return XULStore::SetValue(uri, windowElementId, attrString,
                            maybeConvertedValue);
#else
  if (!mLocalStore) {
    mLocalStore = do_GetService("@mozilla.org/xul/xulstore;1");
    if (NS_WARN_IF(!mLocalStore)) {
      return NS_ERROR_NOT_INITIALIZED;
    }
  }

  return mLocalStore->SetValue(
      uri, windowElementId, nsDependentAtomString(aAttr), maybeConvertedValue);
#endif
}

NS_IMETHODIMP AppWindow::SavePersistentAttributes() {
  // can happen when the persistence timer fires at an inopportune time
  // during window shutdown
  if (!mDocShell) return NS_ERROR_FAILURE;

  nsCOMPtr<dom::Element> docShellElement = GetWindowDOMElement();
  if (!docShellElement) return NS_ERROR_FAILURE;

  nsAutoString persistString;
  docShellElement->GetAttribute(PERSIST_ATTRIBUTE, persistString);
  if (persistString.IsEmpty()) {  // quick check which sometimes helps
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

  nsAutoString sizeString;
  bool shouldPersist = !isFullscreen;
  ErrorResult rv;
  // (only for size elements which are persisted)
  if ((mPersistentAttributesDirty & PAD_POSITION) && gotRestoredBounds) {
    if (persistString.Find("screenX") >= 0) {
      sizeString.Truncate();
      sizeString.AppendInt(NSToIntRound(rect.X() / posScale.scale));
      docShellElement->SetAttribute(SCREENX_ATTRIBUTE, sizeString, rv);
      if (shouldPersist) {
        Unused << SetPersistentValue(nsGkAtoms::screenX, sizeString);
      }
    }
    if (persistString.Find("screenY") >= 0) {
      sizeString.Truncate();
      sizeString.AppendInt(NSToIntRound(rect.Y() / posScale.scale));
      docShellElement->SetAttribute(SCREENY_ATTRIBUTE, sizeString, rv);
      if (shouldPersist) {
        Unused << SetPersistentValue(nsGkAtoms::screenY, sizeString);
      }
    }
  }

  if ((mPersistentAttributesDirty & PAD_SIZE) && gotRestoredBounds) {
    LayoutDeviceIntRect innerRect =
        rect - GetOuterToInnerSizeDifference(mWindow);
    if (persistString.Find("width") >= 0) {
      sizeString.Truncate();
      sizeString.AppendInt(NSToIntRound(innerRect.Width() / sizeScale.scale));
      docShellElement->SetAttribute(WIDTH_ATTRIBUTE, sizeString, rv);
      if (shouldPersist) {
        Unused << SetPersistentValue(nsGkAtoms::width, sizeString);
      }
    }
    if (persistString.Find("height") >= 0) {
      sizeString.Truncate();
      sizeString.AppendInt(NSToIntRound(innerRect.Height() / sizeScale.scale));
      docShellElement->SetAttribute(HEIGHT_ATTRIBUTE, sizeString, rv);
      if (shouldPersist) {
        Unused << SetPersistentValue(nsGkAtoms::height, sizeString);
      }
    }
  }

  Unused << MaybeSaveEarlyWindowPersistentValues(rect);

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
        Unused << SetPersistentValue(nsGkAtoms::sizemode, sizeString);
      }
    }
    bool tiled = mWindow->IsTiled();
    if (tiled) {
      sizeString.Assign(u"true"_ns);
    } else {
      sizeString.Assign(u"false"_ns);
    }
    docShellElement->SetAttribute(TILED_ATTRIBUTE, sizeString, rv);
    if (persistString.Find("zlevel") >= 0) {
      uint32_t zLevel;
      nsCOMPtr<nsIWindowMediator> mediator(
          do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
      if (mediator) {
        mediator->GetZLevel(this, &zLevel);
        sizeString.Truncate();
        sizeString.AppendInt(zLevel);
        docShellElement->SetAttribute(ZLEVEL_ATTRIBUTE, sizeString, rv);
        if (shouldPersist) {
          Unused << SetPersistentValue(nsGkAtoms::zlevel, sizeString);
        }
      }
    }
  }

  mPersistentAttributesDirty = 0;
  return NS_OK;
}

NS_IMETHODIMP AppWindow::GetWindowDOMWindow(mozIDOMWindowProxy** aDOMWindow) {
  NS_ENSURE_STATE(mDocShell);

  if (!mDOMWindow) mDOMWindow = mDocShell->GetWindow();
  NS_ENSURE_TRUE(mDOMWindow, NS_ERROR_FAILURE);

  *aDOMWindow = mDOMWindow;
  NS_ADDREF(*aDOMWindow);
  return NS_OK;
}

dom::Element* AppWindow::GetWindowDOMElement() const {
  NS_ENSURE_TRUE(mDocShell, nullptr);

  nsCOMPtr<nsIContentViewer> cv;
  mDocShell->GetContentViewer(getter_AddRefs(cv));
  NS_ENSURE_TRUE(cv, nullptr);

  const dom::Document* document = cv->GetDocument();
  NS_ENSURE_TRUE(document, nullptr);

  return document->GetRootElement();
}

nsresult AppWindow::ContentShellAdded(nsIDocShellTreeItem* aContentShell,
                                      bool aPrimary) {
  // Set the default content tree owner
  if (aPrimary) {
    NS_ENSURE_SUCCESS(EnsurePrimaryContentTreeOwner(), NS_ERROR_FAILURE);
    aContentShell->SetTreeOwner(mPrimaryContentTreeOwner);
    mPrimaryContentShell = aContentShell;
    mPrimaryBrowserParent = nullptr;
  } else {
    NS_ENSURE_SUCCESS(EnsureContentTreeOwner(), NS_ERROR_FAILURE);
    aContentShell->SetTreeOwner(mContentTreeOwner);
    if (mPrimaryContentShell == aContentShell) mPrimaryContentShell = nullptr;
  }

  return NS_OK;
}

nsresult AppWindow::ContentShellRemoved(nsIDocShellTreeItem* aContentShell) {
  if (mPrimaryContentShell == aContentShell) {
    mPrimaryContentShell = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
AppWindow::GetPrimaryContentSize(int32_t* aWidth, int32_t* aHeight) {
  if (mPrimaryBrowserParent) {
    return GetPrimaryRemoteTabSize(aWidth, aHeight);
  } else if (mPrimaryContentShell) {
    return GetPrimaryContentShellSize(aWidth, aHeight);
  }
  return NS_ERROR_UNEXPECTED;
}

nsresult AppWindow::GetPrimaryRemoteTabSize(int32_t* aWidth, int32_t* aHeight) {
  BrowserHost* host = BrowserHost::GetFrom(mPrimaryBrowserParent.get());
  // Need strong ref, since Client* can run script.
  RefPtr<dom::Element> element = host->GetOwnerElement();
  NS_ENSURE_STATE(element);

  *aWidth = element->ClientWidth();
  *aHeight = element->ClientHeight();
  return NS_OK;
}

nsresult AppWindow::GetPrimaryContentShellSize(int32_t* aWidth,
                                               int32_t* aHeight) {
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
AppWindow::SetPrimaryContentSize(int32_t aWidth, int32_t aHeight) {
  if (mPrimaryBrowserParent) {
    return SetPrimaryRemoteTabSize(aWidth, aHeight);
  } else if (mPrimaryContentShell) {
    return SizeShellTo(mPrimaryContentShell, aWidth, aHeight);
  }
  return NS_ERROR_UNEXPECTED;
}

nsresult AppWindow::SetPrimaryRemoteTabSize(int32_t aWidth, int32_t aHeight) {
  int32_t shellWidth, shellHeight;
  GetPrimaryRemoteTabSize(&shellWidth, &shellHeight);

  double scale = 1.0;
  GetUnscaledDevicePixelsPerCSSPixel(&scale);

  SizeShellToWithLimit(aWidth, aHeight, shellWidth * scale,
                       shellHeight * scale);
  return NS_OK;
}

nsresult AppWindow::GetRootShellSize(int32_t* aWidth, int32_t* aHeight) {
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);
  return mDocShell->GetSize(aWidth, aHeight);
}

nsresult AppWindow::SetRootShellSize(int32_t aWidth, int32_t aHeight) {
  return SizeShellTo(mDocShell, aWidth, aHeight);
}

NS_IMETHODIMP AppWindow::SizeShellTo(nsIDocShellTreeItem* aShellItem,
                                     int32_t aCX, int32_t aCY) {
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

NS_IMETHODIMP AppWindow::ExitModalLoop(nsresult aStatus) {
  if (mContinueModalLoop) EnableParent(true);
  mContinueModalLoop = false;
  mModalStatus = aStatus;
  return NS_OK;
}

// top-level function to create a new window
NS_IMETHODIMP AppWindow::CreateNewWindow(int32_t aChromeFlags,
                                         nsIOpenWindowInfo* aOpenWindowInfo,
                                         nsIAppWindow** _retval) {
  NS_ENSURE_ARG_POINTER(_retval);

  if (aChromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME) {
    MOZ_RELEASE_ASSERT(
        !aOpenWindowInfo,
        "Unexpected nsOpenWindowInfo when creating a new chrome window");
    return CreateNewChromeWindow(aChromeFlags, _retval);
  }

  return CreateNewContentWindow(aChromeFlags, aOpenWindowInfo, _retval);
}

NS_IMETHODIMP AppWindow::CreateNewChromeWindow(int32_t aChromeFlags,
                                               nsIAppWindow** _retval) {
  nsCOMPtr<nsIAppShellService> appShell(
      do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
  NS_ENSURE_TRUE(appShell, NS_ERROR_FAILURE);

  // Just do a normal create of a window and return.
  nsCOMPtr<nsIAppWindow> newWindow;
  appShell->CreateTopLevelWindow(
      this, nullptr, aChromeFlags, nsIAppShellService::SIZE_TO_CONTENT,
      nsIAppShellService::SIZE_TO_CONTENT, getter_AddRefs(newWindow));

  NS_ENSURE_TRUE(newWindow, NS_ERROR_FAILURE);

  newWindow.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP AppWindow::CreateNewContentWindow(
    int32_t aChromeFlags, nsIOpenWindowInfo* aOpenWindowInfo,
    nsIAppWindow** _retval) {
  nsCOMPtr<nsIAppShellService> appShell(
      do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
  NS_ENSURE_TRUE(appShell, NS_ERROR_FAILURE);

  // We need to create a new top level window and then enter a nested
  // loop. Eventually the new window will be told that it has loaded,
  // at which time we know it is safe to spin out of the nested loop
  // and allow the opening code to proceed.

  nsCOMPtr<nsIURI> uri;
  nsAutoCString urlStr;
  urlStr.AssignLiteral(BROWSER_CHROME_URL_QUOTED);

  nsCOMPtr<nsIIOService> service(do_GetService(NS_IOSERVICE_CONTRACTID));
  if (service) {
    service->NewURI(urlStr, nullptr, nullptr, getter_AddRefs(uri));
  }
  NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE);

  // We need to create a chrome window to contain the content window we're about
  // to pass back. The subject principal needs to be system while we're creating
  // it to make things work right, so force a system caller. See bug 799348
  // comment 13 for a description of what happens when we don't.
  nsCOMPtr<nsIAppWindow> newWindow;
  {
    AutoNoJSAPI nojsapi;
    appShell->CreateTopLevelWindow(this, uri, aChromeFlags, 615, 480,
                                   getter_AddRefs(newWindow));
    NS_ENSURE_TRUE(newWindow, NS_ERROR_FAILURE);
  }

  AppWindow* appWin =
      static_cast<AppWindow*>(static_cast<nsIAppWindow*>(newWindow));

  // Specify which flags should be used by browser.xhtml to create the initial
  // content browser window.
  appWin->mInitialOpenWindowInfo = aOpenWindowInfo;

  // Specify that we want the window to remain locked until the chrome has
  // loaded.
  appWin->LockUntilChromeLoad();

  {
    AutoNoJSAPI nojsapi;
    SpinEventLoopUntil([&]() { return !appWin->IsLocked(); });
  }

  NS_ENSURE_STATE(appWin->mPrimaryContentShell ||
                  appWin->mPrimaryBrowserParent);
  MOZ_ASSERT_IF(appWin->mPrimaryContentShell,
                !aOpenWindowInfo->GetNextRemoteBrowser());

  newWindow.forget(_retval);

  return NS_OK;
}

NS_IMETHODIMP AppWindow::GetHasPrimaryContent(bool* aResult) {
  *aResult = mPrimaryBrowserParent || mPrimaryContentShell;
  return NS_OK;
}

void AppWindow::EnableParent(bool aEnable) {
  nsCOMPtr<nsIBaseWindow> parentWindow;
  nsCOMPtr<nsIWidget> parentWidget;

  parentWindow = do_QueryReferent(mParentWindow);
  if (parentWindow) parentWindow->GetMainWidget(getter_AddRefs(parentWidget));
  if (parentWidget) parentWidget->Enable(aEnable);
}

// Constrain the window to its proper z-level
bool AppWindow::ConstrainToZLevel(bool aImmediate, nsWindowZ* aPlacement,
                                  nsIWidget* aReqBelow,
                                  nsIWidget** aActualBelow) {
#if 0
  /* Do we have a parent window? This means our z-order is already constrained,
     since we're a dependent window. Our window list isn't hierarchical,
     so we can't properly calculate placement for such a window.
     Should we just abort? */
  nsCOMPtr<nsIBaseWindow> parentWindow = do_QueryReferent(mParentWindow);
  if (parentWindow)
    return false;
#endif

  nsCOMPtr<nsIWindowMediator> mediator(
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (!mediator) return false;

  bool altered;
  uint32_t position, newPosition, zLevel;
  nsIAppWindow* us = this;

  altered = false;
  mediator->GetZLevel(this, &zLevel);

  // translate from WidgetGUIEvent to nsIWindowMediator constants
  position = nsIWindowMediator::zLevelTop;
  if (*aPlacement == nsWindowZBottom || zLevel == nsIAppWindow::lowestZ)
    position = nsIWindowMediator::zLevelBottom;
  else if (*aPlacement == nsWindowZRelative)
    position = nsIWindowMediator::zLevelBelow;

  if (NS_SUCCEEDED(mediator->CalculateZPosition(
          us, position, aReqBelow, &newPosition, aActualBelow, &altered))) {
    /* If we were asked to move to the top but constrained to remain
       below one of our other windows, first move all windows in that
       window's layer and above to the top. This allows the user to
       click a window which can't be topmost and still bring mozilla
       to the foreground. */
    if (altered &&
        (position == nsIWindowMediator::zLevelTop ||
         (position == nsIWindowMediator::zLevelBelow && aReqBelow == 0)))
      PlaceWindowLayersBehind(zLevel + 1, nsIAppWindow::highestZ, 0);

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
          ourWidget->PlaceBehind(*aPlacement == nsWindowZBottom
                                     ? eZPlacementBottom
                                     : eZPlacementBelow,
                                 *aActualBelow, false);
        }
      }
    }

    /* CalculateZPosition can tell us to be below nothing, because it tries
       not to change something it doesn't recognize. A request to verify
       being below an unrecognized window, then, is treated as a request
       to come to the top (below null) */
    nsCOMPtr<nsIAppWindow> windowAbove;
    if (newPosition == nsIWindowMediator::zLevelBelow && *aActualBelow) {
      windowAbove = (*aActualBelow)->GetWidgetListener()->GetAppWindow();
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
void AppWindow::PlaceWindowLayersBehind(uint32_t aLowLevel, uint32_t aHighLevel,
                                        nsIAppWindow* aBehind) {
  // step through windows in z-order from top to bottommost window

  nsCOMPtr<nsIWindowMediator> mediator(
      do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (!mediator) return;

  nsCOMPtr<nsISimpleEnumerator> windowEnumerator;
  mediator->GetZOrderAppWindowEnumerator(0, true,
                                         getter_AddRefs(windowEnumerator));
  if (!windowEnumerator) return;

  // each window will be moved behind previousHighWidget, itself
  // a moving target. initialize it.
  nsCOMPtr<nsIWidget> previousHighWidget;
  if (aBehind) {
    nsCOMPtr<nsIBaseWindow> highBase(do_QueryInterface(aBehind));
    if (highBase) highBase->GetMainWidget(getter_AddRefs(previousHighWidget));
  }

  // get next lower window
  bool more;
  while (NS_SUCCEEDED(windowEnumerator->HasMoreElements(&more)) && more) {
    uint32_t nextZ;  // z-level of nextWindow
    nsCOMPtr<nsISupports> nextWindow;
    windowEnumerator->GetNext(getter_AddRefs(nextWindow));
    nsCOMPtr<nsIAppWindow> nextAppWindow(do_QueryInterface(nextWindow));
    nextAppWindow->GetZLevel(&nextZ);
    if (nextZ < aLowLevel)
      break;  // we've processed all windows through aLowLevel

    // move it just below its next higher window
    nsCOMPtr<nsIBaseWindow> nextBase(do_QueryInterface(nextAppWindow));
    if (nextBase) {
      nsCOMPtr<nsIWidget> nextWidget;
      nextBase->GetMainWidget(getter_AddRefs(nextWidget));
      if (nextZ <= aHighLevel)
        nextWidget->PlaceBehind(eZPlacementBelow, previousHighWidget, false);
      previousHighWidget = nextWidget;
    }
  }
}

void AppWindow::SetContentScrollbarVisibility(bool aVisible) {
  nsCOMPtr<nsPIDOMWindowOuter> contentWin(
      do_GetInterface(mPrimaryContentShell));
  if (!contentWin) {
    return;
  }

  nsContentUtils::SetScrollbarsVisibility(contentWin->GetDocShell(), aVisible);
}

// during spinup, attributes that haven't been loaded yet can't be dirty
void AppWindow::PersistentAttributesDirty(uint32_t aDirtyFlags) {
  mPersistentAttributesDirty |= aDirtyFlags & mPersistentAttributesMask;
}

void AppWindow::ApplyChromeFlags() {
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
                                  nsIWebBrowserChrome::CHROME_SCROLLBARS);
  }

  /* the other flags are handled together. we have style rules
     in navigator.css that trigger visibility based on
     the 'chromehidden' attribute of the <window> tag. */
  nsAutoString newvalue;

  if (!(mChromeFlags & nsIWebBrowserChrome::CHROME_MENUBAR))
    newvalue.AppendLiteral("menubar ");

  if (!(mChromeFlags & nsIWebBrowserChrome::CHROME_TOOLBAR))
    newvalue.AppendLiteral("toolbar ");

  if (!(mChromeFlags & nsIWebBrowserChrome::CHROME_LOCATIONBAR))
    newvalue.AppendLiteral("location ");

  if (!(mChromeFlags & nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR))
    newvalue.AppendLiteral("directories ");

  if (!(mChromeFlags & nsIWebBrowserChrome::CHROME_STATUSBAR))
    newvalue.AppendLiteral("status ");

  if (!(mChromeFlags & nsIWebBrowserChrome::CHROME_EXTRA))
    newvalue.AppendLiteral("extrachrome ");

  // Note that if we're not actually changing the value this will be a no-op,
  // so no need to compare to the old value.
  IgnoredErrorResult rv;
  window->SetAttribute(u"chromehidden"_ns, newvalue, rv);
}

NS_IMETHODIMP
AppWindow::BeforeStartLayout() {
  ApplyChromeFlags();
  LoadPersistentWindowState();
  SyncAttributesToWidget();
  if (mWindow) {
    SizeShell();
  }
  return NS_OK;
}

NS_IMETHODIMP
AppWindow::LockAspectRatio(bool aShouldLock) {
  mWindow->LockAspectRatio(aShouldLock);
  return NS_OK;
}

void AppWindow::LoadPersistentWindowState() {
  nsCOMPtr<dom::Element> docShellElement = GetWindowDOMElement();
  if (!docShellElement) {
    return;
  }

  // Check if the window wants to persist anything.
  nsAutoString persist;
  docShellElement->GetAttr(kNameSpaceID_None, nsGkAtoms::persist, persist);
  if (persist.IsEmpty()) {
    return;
  }

  auto loadValue = [&](const nsAtom* aAttr) {
    nsDependentAtomString attrString(aAttr);
    if (persist.Find(attrString) >= 0) {
      nsAutoString value;
      nsresult rv = GetPersistentValue(aAttr, value);
      NS_WARNING_ASSERTION(NS_SUCCEEDED(rv), "Failed to get persistent state.");
      if (NS_SUCCEEDED(rv) && !value.IsEmpty()) {
        IgnoredErrorResult err;
        docShellElement->SetAttribute(attrString, value, err);
      }
    }
  };

  loadValue(nsGkAtoms::screenX);
  loadValue(nsGkAtoms::screenY);
  loadValue(nsGkAtoms::width);
  loadValue(nsGkAtoms::height);
  loadValue(nsGkAtoms::sizemode);
}

void AppWindow::SizeShell() {
  AutoRestore<bool> sizingShellFromXUL(mSizingShellFromXUL);
  mSizingShellFromXUL = true;

  int32_t specWidth = -1, specHeight = -1;
  bool gotSize = false;

  nsCOMPtr<dom::Element> windowElement = GetWindowDOMElement();
  nsAutoString windowType;
  if (windowElement) {
    windowElement->GetAttribute(WINDOWTYPE_ATTRIBUTE, windowType);
  }

  CSSIntSize windowDiff = GetOuterToInnerSizeDifferenceInCSSPixels(mWindow);

  // If we're using fingerprint resistance, we're going to resize the window
  // once we have primary content.
  if (nsContentUtils::ShouldResistFingerprinting() &&
      windowType.EqualsLiteral("navigator:browser")) {
    // Once we've got primary content, force dimensions.
    if (mPrimaryContentShell || mPrimaryBrowserParent) {
      ForceRoundedDimensions();
    }
    // Always avoid setting size/sizemode on this window.
    mIgnoreXULSize = true;
    mIgnoreXULSizeMode = true;
  } else if (!mIgnoreXULSize) {
    gotSize = LoadSizeFromXUL(specWidth, specHeight);
    specWidth += windowDiff.width;
    specHeight += windowDiff.height;
  }

  bool positionSet = !mIgnoreXULPosition;
  nsCOMPtr<nsIAppWindow> parentWindow(do_QueryReferent(mParentWindow));
#if defined(XP_UNIX) && !defined(XP_MACOSX)
  // don't override WM placement on unix for independent, top-level windows
  // (however, we think the benefits of intelligent dependent window placement
  // trump that override.)
  if (!parentWindow) positionSet = false;
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
      RefPtr<nsDocShell> docShell = mDocShell;
      nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
      docShell->GetTreeOwner(getter_AddRefs(treeOwner));
      if (treeOwner) {
        // GetContentSize can fail, so initialise |width| and |height| to be
        // on the safe side.
        int32_t width = 0, height = 0;
        if (NS_SUCCEEDED(cv->GetContentSize(&width, &height))) {
          treeOwner->SizeShellTo(docShell, width, height);
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

  UpdateWindowStateFromMiscXULAttributes();

  if (mChromeLoaded && mCenterAfterLoad && !positionSet &&
      mWindow->SizeMode() == nsSizeMode_Normal) {
    Center(parentWindow, parentWindow ? false : true, false);
  }
}

NS_IMETHODIMP AppWindow::GetXULBrowserWindow(
    nsIXULBrowserWindow** aXULBrowserWindow) {
  NS_IF_ADDREF(*aXULBrowserWindow = mXULBrowserWindow);
  return NS_OK;
}

NS_IMETHODIMP AppWindow::SetXULBrowserWindow(
    nsIXULBrowserWindow* aXULBrowserWindow) {
  mXULBrowserWindow = aXULBrowserWindow;
  return NS_OK;
}

void AppWindow::SizeShellToWithLimit(int32_t aDesiredWidth,
                                     int32_t aDesiredHeight,
                                     int32_t shellItemWidth,
                                     int32_t shellItemHeight) {
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

nsresult AppWindow::GetTabCount(uint32_t* aResult) {
  if (mXULBrowserWindow) {
    return mXULBrowserWindow->GetTabCount(aResult);
  }

  *aResult = 0;
  return NS_OK;
}

nsresult AppWindow::GetInitialOpenWindowInfo(
    nsIOpenWindowInfo** aOpenWindowInfo) {
  NS_ENSURE_ARG_POINTER(aOpenWindowInfo);
  *aOpenWindowInfo = do_AddRef(mInitialOpenWindowInfo).take();
  return NS_OK;
}

PresShell* AppWindow::GetPresShell() {
  if (!mDocShell) {
    return nullptr;
  }
  return mDocShell->GetPresShell();
}

bool AppWindow::WindowMoved(nsIWidget* aWidget, int32_t x, int32_t y) {
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm) {
    nsCOMPtr<nsPIDOMWindowOuter> window =
        mDocShell ? mDocShell->GetWindow() : nullptr;
    pm->AdjustPopupsOnWindowChange(window);
  }

  // Notify all tabs that the widget moved.
  if (mDocShell && mDocShell->GetWindow()) {
    nsCOMPtr<EventTarget> eventTarget =
        mDocShell->GetWindow()->GetTopWindowRoot();
    nsContentUtils::DispatchChromeEvent(
        mDocShell->GetDocument(), eventTarget, u"MozUpdateWindowPos"_ns,
        CanBubble::eNo, Cancelable::eNo, nullptr);
  }

  // Persist position, but not immediately, in case this OS is firing
  // repeated move events as the user drags the window
  SetPersistenceTimer(PAD_POSITION);
  return false;
}

bool AppWindow::WindowResized(nsIWidget* aWidget, int32_t aWidth,
                              int32_t aHeight) {
  if (mDocShell) {
    mDocShell->SetPositionAndSize(0, 0, aWidth, aHeight, 0);
  }
  // Persist size, but not immediately, in case this OS is firing
  // repeated size events as the user drags the sizing handle
  if (!IsLocked()) SetPersistenceTimer(PAD_POSITION | PAD_SIZE | PAD_MISC);
  // Check if we need to continue a fullscreen change.
  switch (mFullscreenChangeState) {
    case FullscreenChangeState::WillChange:
      mFullscreenChangeState = FullscreenChangeState::WidgetResized;
      break;
    case FullscreenChangeState::WidgetEnteredFullscreen:
      FinishFullscreenChange(true);
      break;
    case FullscreenChangeState::WidgetExitedFullscreen:
      FinishFullscreenChange(false);
      break;
    case FullscreenChangeState::WidgetResized:
    case FullscreenChangeState::NotChanging:
      break;
  }
  return true;
}

bool AppWindow::RequestWindowClose(nsIWidget* aWidget) {
  // Maintain a reference to this as it is about to get destroyed.
  nsCOMPtr<nsIAppWindow> appWindow(this);

  nsCOMPtr<nsPIDOMWindowOuter> window(mDocShell ? mDocShell->GetWindow()
                                                : nullptr);
  nsCOMPtr<EventTarget> eventTarget = do_QueryInterface(window);

  RefPtr<PresShell> presShell = mDocShell->GetPresShell();
  if (!presShell) {
    mozilla::DebugOnly<bool> dying;
    MOZ_ASSERT(NS_SUCCEEDED(mDocShell->IsBeingDestroyed(&dying)) && dying,
               "No presShell, but window is not being destroyed");
  } else if (eventTarget) {
    RefPtr<nsPresContext> presContext = presShell->GetPresContext();

    nsEventStatus status = nsEventStatus_eIgnore;
    WidgetMouseEvent event(true, eClose, nullptr, WidgetMouseEvent::eReal);
    if (NS_SUCCEEDED(EventDispatcher::Dispatch(eventTarget, presContext, &event,
                                               nullptr, &status)) &&
        status == nsEventStatus_eConsumeNoDefault)
      return false;
  }

  Destroy();
  return false;
}

void AppWindow::SizeModeChanged(nsSizeMode sizeMode) {
  // An alwaysRaised (or higher) window will hide any newly opened normal
  // browser windows, so here we just drop a raised window to the normal
  // zlevel if it's maximized. We make no provision for automatically
  // re-raising it when restored.
  if (sizeMode == nsSizeMode_Maximized || sizeMode == nsSizeMode_Fullscreen) {
    uint32_t zLevel;
    GetZLevel(&zLevel);
    if (zLevel > nsIAppWindow::normalZ) SetZLevel(nsIAppWindow::normalZ);
  }
  mWindow->SetSizeMode(sizeMode);

  // Persist mode, but not immediately, because in many (all?)
  // cases this will merge with the similar call in NS_SIZE and
  // write the attribute values only once.
  SetPersistenceTimer(PAD_MISC);
  nsCOMPtr<nsPIDOMWindowOuter> ourWindow =
      mDocShell ? mDocShell->GetWindow() : nullptr;
  if (ourWindow) {
    // Ensure that the fullscreen state is synchronized between
    // the widget and the outer window object.
    if (sizeMode == nsSizeMode_Fullscreen) {
      ourWindow->SetFullScreen(true);
    } else if (sizeMode != nsSizeMode_Minimized) {
      if (ourWindow->GetFullScreen()) {
        // The first SetFullscreenInternal call below ensures that we do
        // not trigger any fullscreen transition even if the window was
        // put in fullscreen only for the Fullscreen API. The second
        // SetFullScreen call ensures that the window really exit from
        // fullscreen even if it entered fullscreen for both Fullscreen
        // Mode and Fullscreen API.
        ourWindow->SetFullscreenInternal(
            FullscreenReason::ForForceExitFullscreen, false);
        ourWindow->SetFullScreen(false);
      }
    }

    // And always fire a user-defined sizemodechange event on the window
    ourWindow->DispatchCustomEvent(u"sizemodechange"_ns);
  }

  if (PresShell* presShell = GetPresShell()) {
    presShell->GetPresContext()->SizeModeChanged(sizeMode);
  }

  // Note the current implementation of SetSizeMode just stores
  // the new state; it doesn't actually resize. So here we store
  // the state and pass the event on to the OS. The day is coming
  // when we'll handle the event here, and the return result will
  // then need to be different.
}

void AppWindow::UIResolutionChanged() {
  nsCOMPtr<nsPIDOMWindowOuter> ourWindow =
      mDocShell ? mDocShell->GetWindow() : nullptr;
  if (ourWindow) {
    ourWindow->DispatchCustomEvent(u"resolutionchange"_ns,
                                   ChromeOnlyDispatch::eYes);
  }
}

void AppWindow::FullscreenWillChange(bool aInFullscreen) {
  if (mDocShell) {
    if (nsCOMPtr<nsPIDOMWindowOuter> ourWindow = mDocShell->GetWindow()) {
      ourWindow->FullscreenWillChange(aInFullscreen);
    }
  }
  MOZ_ASSERT(mFullscreenChangeState == FullscreenChangeState::NotChanging);
  mFullscreenChangeState = FullscreenChangeState::WillChange;
}

void AppWindow::FullscreenChanged(bool aInFullscreen) {
  if (mFullscreenChangeState == FullscreenChangeState::WidgetResized) {
    FinishFullscreenChange(aInFullscreen);
  } else {
    NS_WARNING_ASSERTION(
        mFullscreenChangeState == FullscreenChangeState::WillChange,
        "Unexpected fullscreen change state");
    FullscreenChangeState newState =
        aInFullscreen ? FullscreenChangeState::WidgetEnteredFullscreen
                      : FullscreenChangeState::WidgetExitedFullscreen;
    mFullscreenChangeState = newState;
    nsCOMPtr<nsIAppWindow> kungFuDeathGrip(this);
    // Wait for resize for a small amount of time.
    // 80ms is actually picked arbitrarily. But it shouldn't be too large
    // in case the widget resize is not going to happen at all, which can
    // be the case for some Linux window managers and possibly Android.
    NS_DelayedDispatchToCurrentThread(
        NS_NewRunnableFunction(
            "AppWindow::FullscreenChanged",
            [this, kungFuDeathGrip, newState, aInFullscreen]() {
              if (mFullscreenChangeState == newState) {
                FinishFullscreenChange(aInFullscreen);
              }
            }),
        80);
  }
}

void AppWindow::FinishFullscreenChange(bool aInFullscreen) {
  mFullscreenChangeState = FullscreenChangeState::NotChanging;
  if (mDocShell) {
    if (nsCOMPtr<nsPIDOMWindowOuter> ourWindow = mDocShell->GetWindow()) {
      ourWindow->FinishFullscreenChange(aInFullscreen);
    }
  }
}

void AppWindow::MacFullscreenMenubarOverlapChanged(
    mozilla::DesktopCoord aOverlapAmount) {
  if (mDocShell) {
    if (nsCOMPtr<nsPIDOMWindowOuter> ourWindow = mDocShell->GetWindow()) {
      ourWindow->MacFullscreenMenubarOverlapChanged(aOverlapAmount);
    }
  }
}

void AppWindow::OcclusionStateChanged(bool aIsFullyOccluded) {
  nsCOMPtr<nsPIDOMWindowOuter> ourWindow =
      mDocShell ? mDocShell->GetWindow() : nullptr;
  if (ourWindow) {
    // And always fire a user-defined occlusionstatechange event on the window
    ourWindow->DispatchCustomEvent(u"occlusionstatechange"_ns,
                                   ChromeOnlyDispatch::eYes);
  }
}

void AppWindow::OSToolbarButtonPressed() {
  // Keep a reference as setting the chrome flags can fire events.
  nsCOMPtr<nsIAppWindow> appWindow(this);

  // rjc: don't use "nsIWebBrowserChrome::CHROME_EXTRA"
  //      due to components with multiple sidebar components
  //      (such as Mail/News, Addressbook, etc)... and frankly,
  //      Mac IE, OmniWeb, and other Mac OS X apps all work this way
  uint32_t chromeMask = (nsIWebBrowserChrome::CHROME_TOOLBAR |
                         nsIWebBrowserChrome::CHROME_LOCATIONBAR |
                         nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR);

  nsCOMPtr<nsIWebBrowserChrome> wbc(do_GetInterface(appWindow));
  if (!wbc) return;

  uint32_t chromeFlags, newChromeFlags = 0;
  wbc->GetChromeFlags(&chromeFlags);
  newChromeFlags = chromeFlags & chromeMask;
  if (!newChromeFlags)
    chromeFlags |= chromeMask;
  else
    chromeFlags &= (~newChromeFlags);
  wbc->SetChromeFlags(chromeFlags);
}

bool AppWindow::ZLevelChanged(bool aImmediate, nsWindowZ* aPlacement,
                              nsIWidget* aRequestBelow,
                              nsIWidget** aActualBelow) {
  if (aActualBelow) *aActualBelow = nullptr;

  return ConstrainToZLevel(aImmediate, aPlacement, aRequestBelow, aActualBelow);
}

void AppWindow::WindowActivated() {
  nsCOMPtr<nsIAppWindow> appWindow(this);

  // focusing the window could cause it to close, so keep a reference to it
  nsCOMPtr<nsPIDOMWindowOuter> window =
      mDocShell ? mDocShell->GetWindow() : nullptr;
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm && window) {
    fm->WindowRaised(window, nsFocusManager::GenerateFocusActionId());
  }

  if (mChromeLoaded) {
    PersistentAttributesDirty(PAD_POSITION | PAD_SIZE | PAD_MISC);
    SavePersistentAttributes();
  }
}

void AppWindow::WindowDeactivated() {
  nsCOMPtr<nsIAppWindow> appWindow(this);

  nsCOMPtr<nsPIDOMWindowOuter> window =
      mDocShell ? mDocShell->GetWindow() : nullptr;
  nsFocusManager* fm = nsFocusManager::GetFocusManager();
  if (fm && window && !fm->IsTestMode()) {
    fm->WindowLowered(window, nsFocusManager::GenerateFocusActionId());
  }
}

#ifdef USE_NATIVE_MENUS
static void LoadNativeMenus(Document* aDoc, nsIWidget* aParentWindow) {
  if (gfxPlatform::IsHeadless()) {
    return;
  }

  // Find the menubar tag (if there is more than one, we ignore all but
  // the first).
  nsCOMPtr<nsINodeList> menubarElements = aDoc->GetElementsByTagNameNS(
      nsLiteralString(
          u"http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"),
      u"menubar"_ns);

  nsCOMPtr<nsINode> menubarNode;
  if (menubarElements) {
    menubarNode = menubarElements->Item(0);
  }

  using widget::NativeMenuSupport;
  if (menubarNode) {
    nsCOMPtr<Element> menubarContent(do_QueryInterface(menubarNode));
    NativeMenuSupport::CreateNativeMenuBar(aParentWindow, menubarContent);
  } else {
    NativeMenuSupport::CreateNativeMenuBar(aParentWindow, nullptr);
  }
}

class L10nReadyPromiseHandler final : public dom::PromiseNativeHandler {
 public:
  NS_DECL_ISUPPORTS

  L10nReadyPromiseHandler(Document* aDoc, nsIWidget* aParentWindow)
      : mDocument(aDoc), mWindow(aParentWindow) {}

  void ResolvedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    LoadNativeMenus(mDocument, mWindow);
  }

  void RejectedCallback(JSContext* aCx, JS::Handle<JS::Value> aValue) override {
    // Again, this shouldn't happen, but fallback to loading the menus as is.
    NS_WARNING(
        "L10nReadyPromiseHandler rejected - loading fallback native "
        "menu.");
    LoadNativeMenus(mDocument, mWindow);
  }

 private:
  ~L10nReadyPromiseHandler() = default;

  RefPtr<Document> mDocument;
  nsCOMPtr<nsIWidget> mWindow;
};

NS_IMPL_ISUPPORTS0(L10nReadyPromiseHandler)

#endif

class AppWindowTimerCallback final : public nsITimerCallback, public nsINamed {
 public:
  explicit AppWindowTimerCallback(AppWindow* aWindow) : mWindow(aWindow) {}

  NS_DECL_THREADSAFE_ISUPPORTS

  NS_IMETHOD Notify(nsITimer* aTimer) override {
    // Although this object participates in a refcount cycle (this -> mWindow
    // -> mSPTimer -> this), mSPTimer is a one-shot timer and releases this
    // after it fires.  So we don't need to release mWindow here.

    mWindow->FirePersistenceTimer();
    return NS_OK;
  }

  NS_IMETHOD GetName(nsACString& aName) override {
    aName.AssignLiteral("AppWindowTimerCallback");
    return NS_OK;
  }

 private:
  ~AppWindowTimerCallback() {}

  RefPtr<AppWindow> mWindow;
};

NS_IMPL_ISUPPORTS(AppWindowTimerCallback, nsITimerCallback, nsINamed)

void AppWindow::SetPersistenceTimer(uint32_t aDirtyFlags) {
  MutexAutoLock lock(mSPTimerLock);
  if (!mSPTimer) {
    mSPTimer = NS_NewTimer();
    if (!mSPTimer) {
      NS_WARNING("Couldn't create @mozilla.org/timer;1 instance?");
      return;
    }
  }

  RefPtr<AppWindowTimerCallback> callback = new AppWindowTimerCallback(this);
  mSPTimer->InitWithCallback(callback, SIZE_PERSISTENCE_TIMEOUT,
                             nsITimer::TYPE_ONE_SHOT);

  PersistentAttributesDirty(aDirtyFlags);
}

void AppWindow::FirePersistenceTimer() {
  MutexAutoLock lock(mSPTimerLock);
  SavePersistentAttributes();
}

//----------------------------------------
// nsIWebProgessListener implementation
//----------------------------------------
NS_IMETHODIMP
AppWindow::OnProgressChange(nsIWebProgress* aProgress, nsIRequest* aRequest,
                            int32_t aCurSelfProgress, int32_t aMaxSelfProgress,
                            int32_t aCurTotalProgress,
                            int32_t aMaxTotalProgress) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
AppWindow::OnStateChange(nsIWebProgress* aProgress, nsIRequest* aRequest,
                         uint32_t aStateFlags, nsresult aStatus) {
  // If the notification is not about a document finishing, then just
  // ignore it...
  if (!(aStateFlags & nsIWebProgressListener::STATE_STOP) ||
      !(aStateFlags & nsIWebProgressListener::STATE_IS_NETWORK)) {
    return NS_OK;
  }

  if (mChromeLoaded) return NS_OK;

  // If this document notification is for a frame then ignore it...
  nsCOMPtr<mozIDOMWindowProxy> eventWin;
  aProgress->GetDOMWindow(getter_AddRefs(eventWin));
  auto* eventPWin = nsPIDOMWindowOuter::From(eventWin);
  if (eventPWin) {
    nsPIDOMWindowOuter* rootPWin = eventPWin->GetPrivateRoot();
    if (eventPWin != rootPWin) return NS_OK;
  }

  mChromeLoaded = true;
  mLockedUntilChromeLoad = false;

#ifdef USE_NATIVE_MENUS
  ///////////////////////////////
  // Find the Menubar DOM  and Load the menus, hooking them up to the loaded
  // commands
  ///////////////////////////////
  nsCOMPtr<nsIContentViewer> cv;
  mDocShell->GetContentViewer(getter_AddRefs(cv));
  if (cv) {
    RefPtr<Document> menubarDoc = cv->GetDocument();
    if (menubarDoc) {
      RefPtr<DocumentL10n> l10n = menubarDoc->GetL10n();
      if (l10n) {
        // Wait for l10n to be ready so the menus are localized.
        RefPtr<Promise> promise = l10n->Ready();
        MOZ_ASSERT(promise);
        RefPtr<L10nReadyPromiseHandler> handler =
            new L10nReadyPromiseHandler(menubarDoc, mWindow);
        promise->AppendNativeHandler(handler);
      } else {
        // Something went wrong loading the doc and l10n wasn't created. This
        // shouldn't really happen, but if it does fallback to trying to load
        // the menus as is.
        LoadNativeMenus(menubarDoc, mWindow);
      }
    }
  }
#endif  // USE_NATIVE_MENUS

  OnChromeLoaded();

  return NS_OK;
}

NS_IMETHODIMP
AppWindow::OnLocationChange(nsIWebProgress* aProgress, nsIRequest* aRequest,
                            nsIURI* aURI, uint32_t aFlags) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
AppWindow::OnStatusChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                          nsresult aStatus, const char16_t* aMessage) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
AppWindow::OnSecurityChange(nsIWebProgress* aWebProgress, nsIRequest* aRequest,
                            uint32_t aState) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
AppWindow::OnContentBlockingEvent(nsIWebProgress* aWebProgress,
                                  nsIRequest* aRequest, uint32_t aEvent) {
  MOZ_ASSERT_UNREACHABLE("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

/**
 * ExecuteCloseHandler - Run the close handler, if any.
 * @return true iff we found a close handler to run.
 */
bool AppWindow::ExecuteCloseHandler() {
  /* If the event handler closes this window -- a likely scenario --
     things get deleted out of order without this death grip.
     (The problem may be the death grip in nsWindow::windowProc,
     which forces this window's widget to remain alive longer
     than it otherwise would.) */
  nsCOMPtr<nsIAppWindow> kungFuDeathGrip(this);

  nsCOMPtr<EventTarget> eventTarget;
  if (mDocShell) {
    eventTarget = do_QueryInterface(mDocShell->GetWindow());
  }

  if (eventTarget) {
    nsCOMPtr<nsIContentViewer> contentViewer;
    mDocShell->GetContentViewer(getter_AddRefs(contentViewer));
    if (contentViewer) {
      RefPtr<nsPresContext> presContext = contentViewer->GetPresContext();

      nsEventStatus status = nsEventStatus_eIgnore;
      WidgetMouseEvent event(true, eClose, nullptr, WidgetMouseEvent::eReal);

      nsresult rv = EventDispatcher::Dispatch(eventTarget, presContext, &event,
                                              nullptr, &status);
      if (NS_SUCCEEDED(rv) && status == nsEventStatus_eConsumeNoDefault)
        return true;
      // else fall through and return false
    }
  }

  return false;
}  // ExecuteCloseHandler

void AppWindow::ConstrainToOpenerScreen(int32_t* aX, int32_t* aY) {
  if (mOpenerScreenRect.IsEmpty()) {
    *aX = *aY = 0;
    return;
  }

  int32_t left, top, width, height;
  // Constrain initial positions to the same screen as opener
  nsCOMPtr<nsIScreenManager> screenmgr =
      do_GetService("@mozilla.org/gfx/screenmanager;1");
  if (screenmgr) {
    nsCOMPtr<nsIScreen> screen;
    screenmgr->ScreenForRect(
        mOpenerScreenRect.X(), mOpenerScreenRect.Y(), mOpenerScreenRect.Width(),
        mOpenerScreenRect.Height(), getter_AddRefs(screen));
    if (screen) {
      screen->GetAvailRectDisplayPix(&left, &top, &width, &height);
      if (*aX < left || *aX > left + width) {
        *aX = left;
      }
      if (*aY < top || *aY > top + height) {
        *aY = top;
      }
    }
  }
}

nsIAppWindow* AppWindow::WidgetListenerDelegate::GetAppWindow() {
  return mAppWindow->GetAppWindow();
}

PresShell* AppWindow::WidgetListenerDelegate::GetPresShell() {
  return mAppWindow->GetPresShell();
}

bool AppWindow::WidgetListenerDelegate::WindowMoved(nsIWidget* aWidget,
                                                    int32_t aX, int32_t aY) {
  RefPtr<AppWindow> holder = mAppWindow;
  return holder->WindowMoved(aWidget, aX, aY);
}

bool AppWindow::WidgetListenerDelegate::WindowResized(nsIWidget* aWidget,
                                                      int32_t aWidth,
                                                      int32_t aHeight) {
  RefPtr<AppWindow> holder = mAppWindow;
  return holder->WindowResized(aWidget, aWidth, aHeight);
}

bool AppWindow::WidgetListenerDelegate::RequestWindowClose(nsIWidget* aWidget) {
  RefPtr<AppWindow> holder = mAppWindow;
  return holder->RequestWindowClose(aWidget);
}

void AppWindow::WidgetListenerDelegate::SizeModeChanged(nsSizeMode aSizeMode) {
  RefPtr<AppWindow> holder = mAppWindow;
  holder->SizeModeChanged(aSizeMode);
}

void AppWindow::WidgetListenerDelegate::UIResolutionChanged() {
  RefPtr<AppWindow> holder = mAppWindow;
  holder->UIResolutionChanged();
}

void AppWindow::WidgetListenerDelegate::FullscreenWillChange(
    bool aInFullscreen) {
  RefPtr<AppWindow> holder = mAppWindow;
  holder->FullscreenWillChange(aInFullscreen);
}

void AppWindow::WidgetListenerDelegate::FullscreenChanged(bool aInFullscreen) {
  RefPtr<AppWindow> holder = mAppWindow;
  holder->FullscreenChanged(aInFullscreen);
}

void AppWindow::WidgetListenerDelegate::MacFullscreenMenubarOverlapChanged(
    DesktopCoord aOverlapAmount) {
  RefPtr<AppWindow> holder = mAppWindow;
  return holder->MacFullscreenMenubarOverlapChanged(aOverlapAmount);
}

void AppWindow::WidgetListenerDelegate::OcclusionStateChanged(
    bool aIsFullyOccluded) {
  RefPtr<AppWindow> holder = mAppWindow;
  holder->OcclusionStateChanged(aIsFullyOccluded);
}

void AppWindow::WidgetListenerDelegate::OSToolbarButtonPressed() {
  RefPtr<AppWindow> holder = mAppWindow;
  holder->OSToolbarButtonPressed();
}

bool AppWindow::WidgetListenerDelegate::ZLevelChanged(
    bool aImmediate, nsWindowZ* aPlacement, nsIWidget* aRequestBelow,
    nsIWidget** aActualBelow) {
  RefPtr<AppWindow> holder = mAppWindow;
  return holder->ZLevelChanged(aImmediate, aPlacement, aRequestBelow,
                               aActualBelow);
}

void AppWindow::WidgetListenerDelegate::WindowActivated() {
  RefPtr<AppWindow> holder = mAppWindow;
  holder->WindowActivated();
}

void AppWindow::WidgetListenerDelegate::WindowDeactivated() {
  RefPtr<AppWindow> holder = mAppWindow;
  holder->WindowDeactivated();
}

}  // namespace mozilla
