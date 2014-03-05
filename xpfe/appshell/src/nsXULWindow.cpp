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
#include "prprf.h"
#include "nsCRT.h"
#include "nsThreadUtils.h"
#include "nsNetCID.h"

//Interfaces needed to be included
#include "nsIAppShell.h"
#include "nsIAppShellService.h"
#include "nsIServiceManager.h"
#include "nsIContentViewer.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMXULDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULElement.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMScreen.h"
#include "nsIEmbeddingSiteWindow.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIIOService.h"
#include "nsIMarkupDocumentViewer.h"
#include "nsIObserverService.h"
#include "nsIWindowMediator.h"
#include "nsIScreenManager.h"
#include "nsIScreen.h"
#include "nsIScrollable.h"
#include "nsIScriptSecurityManager.h"
#include "nsIWindowWatcher.h"
#include "nsIURI.h"
#include "nsIDOMCSSStyleDeclaration.h"
#include "nsAppShellCID.h"
#include "nsReadableUtils.h"
#include "nsStyleConsts.h"
#include "nsPresContext.h"
#include "nsContentUtils.h"
#include "nsWebShellWindow.h" // get rid of this one, too...
#include "nsGlobalWindow.h"

#include "prenv.h"
#include "mozilla/AutoRestore.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/BarProps.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/ScriptSettings.h"

using namespace mozilla;
using dom::AutoSystemCaller;

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
    mContextFlags(0),
    mPersistentAttributesDirty(0),
    mPersistentAttributesMask(0),
    mChromeFlags(aChromeFlags)
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
  if (aIID.Equals(NS_GET_IID(nsIDOMWindow))) {
    return GetWindowDOMWindow(reinterpret_cast<nsIDOMWindow**>(aSink));
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMWindowInternal))) {
    nsIDOMWindow* domWindow = nullptr;
    rv = GetWindowDOMWindow(&domWindow);
    nsIDOMWindowInternal* domWindowInternal =
      static_cast<nsIDOMWindowInternal*>(domWindow);
    *aSink = domWindowInternal;
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
    int32_t sizeMode = mWindow->SizeMode();
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
      nsRefPtr<dom::Event> event =
        doc->CreateEvent(NS_LITERAL_STRING("Events"),rv);
      if (event) {
        event->InitEvent(NS_LITERAL_STRING("windowZLevel"), true, false);

        event->SetTrusted(true);

        bool defaultActionEnabled;
        doc->DispatchEvent(event, &defaultActionEnabled);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetContextFlags(uint32_t *aContextFlags)
{
  NS_ENSURE_ARG_POINTER(aContextFlags);
  *aContextFlags = mContextFlags;
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetContextFlags(uint32_t aContextFlags)
{
  mContextFlags = aContextFlags;
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
  if (mChromeLoaded)
    NS_ENSURE_SUCCESS(ApplyChromeFlags(), NS_ERROR_FAILURE);
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

NS_IMETHODIMP nsXULWindow::GetContentShellById(const char16_t* aID, 
   nsIDocShellTreeItem** aDocShellTreeItem)
{
  NS_ENSURE_ARG_POINTER(aDocShellTreeItem);
  *aDocShellTreeItem = nullptr;

  uint32_t count = mContentShells.Length();
  for (uint32_t i = 0; i < count; i++) {
    nsContentShellInfo* shellInfo = mContentShells.ElementAt(i);
    if (shellInfo->id.Equals(aID)) {
      *aDocShellTreeItem = nullptr;
      if (shellInfo->child)
        CallQueryReferent(shellInfo->child.get(), aDocShellTreeItem);
      return NS_OK;
    }
  }
  return NS_ERROR_FAILURE;
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
  // Store locally so it doesn't die on us
  nsCOMPtr<nsIWidget> window = mWindow;
  nsCOMPtr<nsIXULWindow> tempRef = this;  

  window->SetModal(true);
  mContinueModalLoop = true;
  EnableParent(false);

  {
    AutoSystemCaller asc;
    nsIThread *thread = NS_GetCurrentThread();
    while (mContinueModalLoop) {
      if (!NS_ProcessNextEvent(thread))
        break;
    }
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
  if (mWindow)
    mWindow->Show(false);

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

  // Remove our ref on the content shells
  uint32_t count = mContentShells.Length();
  for (uint32_t i = 0; i < count; i++) {
    nsContentShellInfo* shellInfo = mContentShells.ElementAt(i);
    delete shellInfo;
  }
  mContentShells.Clear();
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

  if (!mIsHiddenWindow) {
    /* Inform appstartup we've destroyed this window and it could
       quit now if it wanted. This must happen at least after mDocShell
       is destroyed, because onunload handlers fire then, and those being
       script, anything could happen. A new window could open, even.
       See bug 130719. */
    nsCOMPtr<nsIObserverService> obssvc =
        do_GetService("@mozilla.org/observer-service;1");
    NS_ASSERTION(obssvc, "Couldn't get observer service?");

    if (obssvc)
      obssvc->NotifyObservers(nullptr, "xul-window-destroyed", nullptr);
  }

  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetUnscaledDevicePixelsPerCSSPixel(double *aScale)
{
  *aScale = mWindow ? mWindow->GetDefaultScale().scale : 1.0;
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetPosition(int32_t aX, int32_t aY)
{
  // Don't reset the window's size mode here - platforms that don't want to move
  // maximized windows should reset it in their respective Move implementation.
  CSSToLayoutDeviceScale scale = mWindow->GetDefaultScale();
  double invScale = 1.0 / scale.scale;
  nsresult rv = mWindow->Move(aX * invScale, aY * invScale);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
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

  CSSToLayoutDeviceScale scale = mWindow->GetDefaultScale();
  double invScale = 1.0 / scale.scale;
  nsresult rv = mWindow->Resize(aCX * invScale, aCY * invScale, aRepaint);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
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
   int32_t aCX, int32_t aCY, bool aRepaint)
{
  /* any attempt to set the window's size or position overrides the window's
     zoom state. this is important when these two states are competing while
     the window is being opened. but it should probably just always be so. */
  mWindow->SetSizeMode(nsSizeMode_Normal);

  mIntrinsicallySized = false;

  CSSToLayoutDeviceScale scale = mWindow->GetDefaultScale();
  double invScale = 1.0 / scale.scale;
  nsresult rv = mWindow->Resize(aX * invScale, aY * invScale,
                                aCX * invScale, aCY * invScale,
                                aRepaint);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
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
  nsIntRect rect;

  if (!mWindow)
    return NS_ERROR_FAILURE;

  mWindow->GetScreenBounds(rect);

  if (x)
    *x = rect.x;
  if (y)
    *y = rect.y;
  if (cx)
    *cx = rect.width;
  if (cy)
    *cy = rect.height;

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
        if (NS_SUCCEEDED(base->GetUnscaledDevicePixelsPerCSSPixel(&scale))) {
          // convert device-pixel coordinates to global display pixels
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
      screenmgr->ScreenForRect(mOpenerScreenRect.x, mOpenerScreenRect.y,
                               mOpenerScreenRect.width, mOpenerScreenRect.height,
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
    CSSToLayoutDeviceScale scale = mWindow->GetDefaultScale();
    GetSize(&ourWidth, &ourHeight);
    ourWidth = NSToIntRound(ourWidth / scale.scale);
    ourHeight = NSToIntRound(ourHeight / scale.scale);
    left += (width - ourWidth) / 2;
    top += (height - ourHeight) / (aAlert ? 3 : 2);
    if (windowCoordinates) {
      mWindow->ConstrainPosition(false, &left, &top);
    }
    SetPosition(left * scale.scale, top * scale.scale);
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
  nsCOMPtr<nsIObserverService> obssvc
    (do_GetService("@mozilla.org/observer-service;1"));
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

NS_IMETHODIMP nsXULWindow::GetTitle(char16_t** aTitle)
{
  NS_ENSURE_ARG_POINTER(aTitle);

  *aTitle = ToNewUnicode(mTitle);
  if (!*aTitle)
    return NS_ERROR_OUT_OF_MEMORY;
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::SetTitle(const char16_t* aTitle)
{
  NS_ENSURE_STATE(mWindow);
  mTitle.Assign(aTitle);
  mTitle.StripChars("\n\r");
  NS_ENSURE_SUCCESS(mWindow->SetTitle(mTitle), NS_ERROR_FAILURE);

  // Tell the window mediator that a title has changed
  nsCOMPtr<nsIWindowMediator> windowMediator(do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
  if (!windowMediator)
    return NS_OK;

  windowMediator->UpdateWindowTitle(static_cast<nsIXULWindow*>(this), aTitle);

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
  NS_ENSURE_TRUE(mChromeTreeOwner, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(mChromeTreeOwner);
  mChromeTreeOwner->XULWindow(this);

  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::EnsureContentTreeOwner()
{
  if (mContentTreeOwner)
    return NS_OK;

  mContentTreeOwner = new nsContentTreeOwner(false);
  NS_ENSURE_TRUE(mContentTreeOwner, NS_ERROR_FAILURE);

  NS_ADDREF(mContentTreeOwner);
  mContentTreeOwner->XULWindow(this);
   
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::EnsurePrimaryContentTreeOwner()
{
  if (mPrimaryContentTreeOwner)
    return NS_OK;

  mPrimaryContentTreeOwner = new nsContentTreeOwner(true);
  NS_ENSURE_TRUE(mPrimaryContentTreeOwner, NS_ERROR_FAILURE);

  NS_ADDREF(mPrimaryContentTreeOwner);
  mPrimaryContentTreeOwner->XULWindow(this);

  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::EnsurePrompter()
{
  if (mPrompter)
    return NS_OK;
   
  nsCOMPtr<nsIDOMWindow> ourWindow;
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
      
  nsCOMPtr<nsIDOMWindow> ourWindow;
  nsresult rv = GetWindowDOMWindow(getter_AddRefs(ourWindow));
  if (NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
    if (wwatch)
      wwatch->GetNewAuthPrompter(ourWindow, getter_AddRefs(mAuthPrompter));
  }
  return mAuthPrompter ? NS_OK : NS_ERROR_FAILURE;
}
 
void nsXULWindow::OnChromeLoaded()
{
  nsresult rv = EnsureContentTreeOwner();

  if (NS_SUCCEEDED(rv)) {
    mChromeLoaded = true;
    ApplyChromeFlags();
    SyncAttributesToWidget();
    if (!mIgnoreXULSize)
      LoadSizeFromXUL();
    if (mIntrinsicallySized) {
      // (if LoadSizeFromXUL set the size, mIntrinsicallySized will be false)
      nsCOMPtr<nsIContentViewer> cv;
      mDocShell->GetContentViewer(getter_AddRefs(cv));
      nsCOMPtr<nsIMarkupDocumentViewer> markupViewer = do_QueryInterface(cv);
      if (markupViewer) {
        nsCOMPtr<nsIDocShellTreeItem> docShellAsItem = do_QueryInterface(mDocShell);
        nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
        docShellAsItem->GetTreeOwner(getter_AddRefs(treeOwner));
        if (treeOwner) {
          int32_t width, height;
          markupViewer->GetContentSize(&width, &height);
          treeOwner->SizeShellTo(docShellAsItem, width, height);
        }
      }
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
    if (positionSet)
      positionSet = LoadPositionFromXUL();
    LoadMiscPersistentAttributesFromXUL();

    if (mCenterAfterLoad && !positionSet)
      Center(parentWindow, parentWindow ? false : true, false);

    if (mShowAfterLoad) {
      SetVisibility(true);
      // At this point the window may have been closed during Show(), so
      // nsXULWindow::Destroy may already have been called. Take care!
    }
  }
  mPersistentAttributesMask |= PAD_POSITION | PAD_SIZE | PAD_MISC;
}

bool nsXULWindow::LoadPositionFromXUL()
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
  CSSToLayoutDeviceScale scale = mWindow->GetDefaultScale();
  currX = NSToIntRound(currX / scale.scale);
  currY = NSToIntRound(currY / scale.scale);
  currWidth = NSToIntRound(currWidth / scale.scale);
  currHeight = NSToIntRound(currHeight / scale.scale);

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
        if (NS_SUCCEEDED(parent->GetUnscaledDevicePixelsPerCSSPixel(&scale))) {
          parentX = NSToIntRound(parentX / scale);
          parentY = NSToIntRound(parentY / scale);
        }
        specX += parentX;
        specY += parentY;
      }
    }
    else {
      StaggerPosition(specX, specY, currWidth, currHeight);
    }
  }
  mWindow->ConstrainPosition(false, &specX, &specY);
  if (specX != currX || specY != currY) {
    CSSToLayoutDeviceScale scale = mWindow->GetDefaultScale();
    SetPosition(specX * scale.scale, specY * scale.scale);
  }

  return gotPosition;
}

bool nsXULWindow::LoadSizeFromXUL()
{
  bool     gotSize = false;

  // if we're the hidden window, don't try to validate our size/position. We're
  // special.
  if (mIsHiddenWindow)
    return false;

  nsCOMPtr<dom::Element> windowElement = GetWindowDOMElement();
  NS_ENSURE_TRUE(windowElement, false);

  int32_t currWidth = 0;
  int32_t currHeight = 0;
  nsresult errorCode;
  int32_t temp;

  NS_ASSERTION(mWindow, "we expected to have a window already");

  CSSToLayoutDeviceScale scale = mWindow ? mWindow->GetDefaultScale()
                                         : CSSToLayoutDeviceScale(1.0);
  GetSize(&currWidth, &currHeight);
  currWidth = NSToIntRound(currWidth / scale.scale);
  currHeight = NSToIntRound(currHeight / scale.scale);

  // Obtain the position and sizing information from the <xul:window> element.
  int32_t specWidth = currWidth;
  int32_t specHeight = currHeight;
  nsAutoString sizeString;

  windowElement->GetAttribute(WIDTH_ATTRIBUTE, sizeString);
  temp = sizeString.ToInteger(&errorCode);
  if (NS_SUCCEEDED(errorCode) && temp > 0) {
    specWidth = std::max(temp, 100);
    gotSize = true;
  }
  windowElement->GetAttribute(HEIGHT_ATTRIBUTE, sizeString);
  temp = sizeString.ToInteger(&errorCode);
  if (NS_SUCCEEDED(errorCode) && temp > 0) {
    specHeight = std::max(temp, 100);
    gotSize = true;
  }

  if (gotSize) {
    // constrain to screen size
    nsCOMPtr<nsIDOMWindow> domWindow;
    GetWindowDOMWindow(getter_AddRefs(domWindow));
    if (domWindow) {
      nsCOMPtr<nsIDOMScreen> screen;
      domWindow->GetScreen(getter_AddRefs(screen));
      if (screen) {
        int32_t screenWidth;
        int32_t screenHeight;
        screen->GetAvailWidth(&screenWidth);
        screen->GetAvailHeight(&screenHeight);
        if (specWidth > screenWidth)
          specWidth = screenWidth;
        if (specHeight > screenHeight)
          specHeight = screenHeight;
      }
    }

    mIntrinsicallySized = false;
    if (specWidth != currWidth || specHeight != currHeight) {
      CSSToLayoutDeviceScale scale = mWindow->GetDefaultScale();
      SetSize(specWidth * scale.scale, specHeight * scale.scale, false);
    }
  }

  return gotSize;
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
  int32_t sizeMode = nsSizeMode_Normal;
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
    nsCOMPtr<nsIDOMWindow> ourWindow;
    GetWindowDOMWindow(getter_AddRefs(ourWindow));
    ourWindow->SetFullScreen(true);
  } else {
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
*/
void nsXULWindow::StaggerPosition(int32_t &aRequestedX, int32_t &aRequestedY,
                                  int32_t aSpecWidth, int32_t aSpecHeight)
{
  const int32_t kOffset = 22;
  const uint32_t kSlop  = 4;

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
        if (NS_SUCCEEDED(listBaseWindow->GetUnscaledDevicePixelsPerCSSPixel(&scale))) {
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
    mWindow->SetNonClientMargins(margins);
  }

  // "accelerated" attribute
  bool isAccelerated = windowElement->HasAttribute(NS_LITERAL_STRING("accelerated"));
  mWindow->SetLayersAcceleration(isAccelerated);

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

  // get our size, position and mode to persist
  int32_t x, y, cx, cy;
  NS_ENSURE_SUCCESS(GetPositionAndSize(&x, &y, &cx, &cy), NS_ERROR_FAILURE);

  int32_t sizeMode = mWindow->SizeMode();
  CSSToLayoutDeviceScale scale = mWindow->GetDefaultScale();

  // make our position relative to our parent, if any
  nsCOMPtr<nsIBaseWindow> parent(do_QueryReferent(mParentWindow));
  if (parent) {
    int32_t parentX, parentY;
    if (NS_SUCCEEDED(parent->GetPosition(&parentX, &parentY))) {
      x -= parentX;
      y -= parentY;
    }
  }

  char                        sizeBuf[10];
  nsAutoString                sizeString;
  nsAutoString                windowElementId;
  nsCOMPtr<nsIDOMXULDocument> ownerXULDoc;

  // fetch docShellElement's ID and XUL owner document
  ownerXULDoc = do_QueryInterface(docShellElement->OwnerDoc());
  if (docShellElement->IsXUL()) {
    docShellElement->GetId(windowElementId);
  }

  ErrorResult rv;
  // (only for size elements which are persisted)
  if ((mPersistentAttributesDirty & PAD_POSITION) &&
      sizeMode == nsSizeMode_Normal) {
    if (persistString.Find("screenX") >= 0) {
      PR_snprintf(sizeBuf, sizeof(sizeBuf), "%d", NSToIntRound(x / scale.scale));
      sizeString.AssignWithConversion(sizeBuf);
      docShellElement->SetAttribute(SCREENX_ATTRIBUTE, sizeString, rv);
      if (ownerXULDoc) // force persistence in case the value didn't change
        ownerXULDoc->Persist(windowElementId, SCREENX_ATTRIBUTE);
    }
    if (persistString.Find("screenY") >= 0) {
      PR_snprintf(sizeBuf, sizeof(sizeBuf), "%d", NSToIntRound(y / scale.scale));
      sizeString.AssignWithConversion(sizeBuf);
      docShellElement->SetAttribute(SCREENY_ATTRIBUTE, sizeString, rv);
      if (ownerXULDoc)
        ownerXULDoc->Persist(windowElementId, SCREENY_ATTRIBUTE);
    }
  }

  if ((mPersistentAttributesDirty & PAD_SIZE) &&
      sizeMode == nsSizeMode_Normal) {
    if (persistString.Find("width") >= 0) {
      PR_snprintf(sizeBuf, sizeof(sizeBuf), "%d", NSToIntRound(cx / scale.scale));
      sizeString.AssignWithConversion(sizeBuf);
      docShellElement->SetAttribute(WIDTH_ATTRIBUTE, sizeString, rv);
      if (ownerXULDoc)
        ownerXULDoc->Persist(windowElementId, WIDTH_ATTRIBUTE);
    }
    if (persistString.Find("height") >= 0) {
      PR_snprintf(sizeBuf, sizeof(sizeBuf), "%d", NSToIntRound(cy / scale.scale));
      sizeString.AssignWithConversion(sizeBuf);
      docShellElement->SetAttribute(HEIGHT_ATTRIBUTE, sizeString, rv);
      if (ownerXULDoc)
        ownerXULDoc->Persist(windowElementId, HEIGHT_ATTRIBUTE);
    }
  }

  if (mPersistentAttributesDirty & PAD_MISC) {
    if (sizeMode != nsSizeMode_Minimized) {
      if (sizeMode == nsSizeMode_Maximized)
        sizeString.Assign(SIZEMODE_MAXIMIZED);
      else if (sizeMode == nsSizeMode_Fullscreen)
        sizeString.Assign(SIZEMODE_FULLSCREEN);
      else
        sizeString.Assign(SIZEMODE_NORMAL);
      docShellElement->SetAttribute(MODE_ATTRIBUTE, sizeString, rv);
      if (ownerXULDoc && persistString.Find("sizemode") >= 0)
        ownerXULDoc->Persist(windowElementId, MODE_ATTRIBUTE);
    }
    if (persistString.Find("zlevel") >= 0) {
      uint32_t zLevel;
      nsCOMPtr<nsIWindowMediator> mediator(do_GetService(NS_WINDOWMEDIATOR_CONTRACTID));
      if (mediator) {
        mediator->GetZLevel(this, &zLevel);
        PR_snprintf(sizeBuf, sizeof(sizeBuf), "%lu", (unsigned long)zLevel);
        sizeString.AssignWithConversion(sizeBuf);
        docShellElement->SetAttribute(ZLEVEL_ATTRIBUTE, sizeString, rv);
        ownerXULDoc->Persist(windowElementId, ZLEVEL_ATTRIBUTE);
      }
    }
  }

  mPersistentAttributesDirty = 0;
  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::GetWindowDOMWindow(nsIDOMWindow** aDOMWindow)
{
  NS_ENSURE_STATE(mDocShell);

  if (!mDOMWindow)
    mDOMWindow = do_GetInterface(mDocShell);
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
   bool aPrimary, bool aTargetable, const nsAString& aID)
{
  nsContentShellInfo* shellInfo = nullptr;

  uint32_t i, count = mContentShells.Length();
  nsWeakPtr contentShellWeak = do_GetWeakReference(aContentShell);
  for (i = 0; i < count; i++) {
    nsContentShellInfo* info = mContentShells.ElementAt(i);
    if (info->id.Equals(aID)) {
      // We already exist. Do a replace.
      info->child = contentShellWeak;
      shellInfo = info;
    }
    else if (info->child == contentShellWeak)
      info->child = nullptr;
  }

  if (!shellInfo) {
    shellInfo = new nsContentShellInfo(aID, contentShellWeak);
    mContentShells.AppendElement(shellInfo);
  }
    
  // Set the default content tree owner
  if (aPrimary) {
    NS_ENSURE_SUCCESS(EnsurePrimaryContentTreeOwner(), NS_ERROR_FAILURE);
    aContentShell->SetTreeOwner(mPrimaryContentTreeOwner);
    mPrimaryContentShell = aContentShell;
  }
  else {
    NS_ENSURE_SUCCESS(EnsureContentTreeOwner(), NS_ERROR_FAILURE);
    aContentShell->SetTreeOwner(mContentTreeOwner);
    if (mPrimaryContentShell == aContentShell)
      mPrimaryContentShell = nullptr;
  }

  if (aTargetable) {
#ifdef DEBUG
    int32_t debugCount = mTargetableShells.Count();
    int32_t debugCounter;
    for (debugCounter = debugCount - 1; debugCounter >= 0; --debugCounter) {
      nsCOMPtr<nsIDocShellTreeItem> curItem =
        do_QueryReferent(mTargetableShells[debugCounter]);
      NS_ASSERTION(!SameCOMIdentity(curItem, aContentShell),
                   "Adding already existing item to mTargetableShells");
    }
#endif
    
    // put the new shell at the start of the targetable shells list if either
    // it's the new primary shell or there is no existing primary shell (which
    // means that chances are this one just stopped being primary).  If we
    // really cared, we could keep track of the "last no longer primary shell"
    // explicitly, but it probably doesn't matter enough: the difference would
    // only be felt in a situation where all shells were non-primary, which
    // doesn't happen much.  In a situation where there is one and only one
    // primary shell, and in which shells get unmarked as primary before some
    // other shell gets marked as primary, this effectively stores the list of
    // targetable shells in "most recently primary first" order.
    bool inserted;
    if (aPrimary || !mPrimaryContentShell) {
      inserted = mTargetableShells.InsertObjectAt(contentShellWeak, 0);
    } else {
      inserted = mTargetableShells.AppendObject(contentShellWeak);
    }
    NS_ENSURE_TRUE(inserted, NS_ERROR_OUT_OF_MEMORY);
  }

  return NS_OK;
}

nsresult nsXULWindow::ContentShellRemoved(nsIDocShellTreeItem* aContentShell)
{
  if (mPrimaryContentShell == aContentShell) {
    mPrimaryContentShell = nullptr;
  }

  int32_t i, count = mContentShells.Length();
  for (i = count - 1; i >= 0; --i) {
    nsContentShellInfo* info = mContentShells.ElementAt(i);
    nsCOMPtr<nsIDocShellTreeItem> curItem = do_QueryReferent(info->child);
    if (!curItem || SameCOMIdentity(curItem, aContentShell)) {
      mContentShells.RemoveElementAt(i);
      delete info;
    }
  }

  count = mTargetableShells.Count();
  for (i = count - 1; i >= 0; --i) {
    nsCOMPtr<nsIDocShellTreeItem> curItem =
      do_QueryReferent(mTargetableShells[i]);
    if (!curItem || SameCOMIdentity(curItem, aContentShell)) {
      mTargetableShells.RemoveObjectAt(i);
    }
  }
  
  return NS_OK;
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

  int32_t widthDelta = aCX - width;
  int32_t heightDelta = aCY - height;

  if (widthDelta || heightDelta) {
    int32_t winCX = 0;
    int32_t winCY = 0;

    GetSize(&winCX, &winCY);
    // There's no point in trying to make the window smaller than the
    // desired docshell size --- that's not likely to work. This whole
    // function assumes that the outer docshell is adding some constant
    // "border" chrome to aShellItem.
    winCX = std::max(winCX + widthDelta, aCX);
    winCY = std::max(winCY + heightDelta, aCY);
    SetSize(winCX, winCY, true);
  }

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
                             nsIXULWindow **_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  if (aChromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME)
    return CreateNewChromeWindow(aChromeFlags, _retval);
  return CreateNewContentWindow(aChromeFlags, _retval);
}

NS_IMETHODIMP nsXULWindow::CreateNewChromeWindow(int32_t aChromeFlags,
                                                 nsIXULWindow **_retval)
{
  nsCOMPtr<nsIAppShellService> appShell(do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
  NS_ENSURE_TRUE(appShell, NS_ERROR_FAILURE);

  // Just do a normal create of a window and return.

  nsCOMPtr<nsIXULWindow> newWindow;
  appShell->CreateTopLevelWindow(this, nullptr, aChromeFlags,
                                 nsIAppShellService::SIZE_TO_CONTENT,
                                 nsIAppShellService::SIZE_TO_CONTENT,
                                 getter_AddRefs(newWindow));

  NS_ENSURE_TRUE(newWindow, NS_ERROR_FAILURE);

  *_retval = newWindow;
  NS_ADDREF(*_retval);

  return NS_OK;
}

NS_IMETHODIMP nsXULWindow::CreateNewContentWindow(int32_t aChromeFlags,
                                                  nsIXULWindow **_retval)
{
  nsCOMPtr<nsIAppShellService> appShell(do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
  NS_ENSURE_TRUE(appShell, NS_ERROR_FAILURE);

  // We need to create a new top level window and then enter a nested
  // loop. Eventually the new window will be told that it has loaded,
  // at which time we know it is safe to spin out of the nested loop
  // and allow the opening code to proceed.

  nsCOMPtr<nsIURI> uri;

  nsAdoptingCString urlStr = Preferences::GetCString("browser.chromeURL");
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
    AutoSystemCaller asc;
    appShell->CreateTopLevelWindow(this, uri,
                                   aChromeFlags, 615, 480,
                                   getter_AddRefs(newWindow));
    NS_ENSURE_TRUE(newWindow, NS_ERROR_FAILURE);
  }

  // Specify that we want the window to remain locked until the chrome has loaded.
  nsXULWindow *xulWin = static_cast<nsXULWindow*>
                                   (static_cast<nsIXULWindow*>
                                               (newWindow));

  xulWin->LockUntilChromeLoad();

  {
    AutoSystemCaller asc;
    nsIThread *thread = NS_GetCurrentThread();
    while (xulWin->IsLocked()) {
      if (!NS_ProcessNextEvent(thread))
        break;
    }
 }

  NS_ENSURE_STATE(xulWin->mPrimaryContentShell);

  *_retval = newWindow;
  NS_ADDREF(*_retval);

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
  while (windowEnumerator->HasMoreElements(&more), more) {
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
  nsCOMPtr<nsPIDOMWindow> contentWin(do_GetInterface(mPrimaryContentShell));
  if (contentWin) {
    mozilla::ErrorResult rv;
    nsRefPtr<nsGlobalWindow> window = static_cast<nsGlobalWindow*>(contentWin.get());
    nsRefPtr<mozilla::dom::BarProp> scrollbars = window->GetScrollbars(rv);
    if (scrollbars) {
      scrollbars->SetVisible(aVisible, rv);
    }
  }
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

NS_IMETHODIMP nsXULWindow::ApplyChromeFlags()
{
  nsCOMPtr<dom::Element> window = GetWindowDOMElement();
  NS_ENSURE_TRUE(window, NS_ERROR_FAILURE);

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
  ErrorResult rv;
  window->SetAttribute(NS_LITERAL_STRING("chromehidden"), newvalue, rv);

  return NS_OK;
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

//*****************************************************************************
//*** nsContentShellInfo: Object Management
//*****************************************************************************   

nsContentShellInfo::nsContentShellInfo(const nsAString& aID,
                                       nsIWeakReference* aContentShell)
  : id(aID),
    child(aContentShell)
{
  MOZ_COUNT_CTOR(nsContentShellInfo);
}

nsContentShellInfo::~nsContentShellInfo()
{
  MOZ_COUNT_DTOR(nsContentShellInfo);
   //XXX Set Tree Owner to null if the tree owner is nsXULWindow->mContentTreeOwner
} 
