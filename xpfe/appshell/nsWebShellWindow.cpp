/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsWebShellWindow.h"

#include "nsLayoutCID.h"
#include "nsContentCID.h"
#include "nsIWeakReference.h"
#include "nsIContentViewer.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "nsNetCID.h"
#include "nsIStringBundle.h"
#include "nsReadableUtils.h"

#include "nsContentUtils.h"
#include "nsEscape.h"
#include "nsPIDOMWindow.h"
#include "nsIWebNavigation.h"
#include "nsIWindowWatcher.h"

#include "nsWidgetInitData.h"
#include "nsWidgetsCID.h"
#include "nsIWidget.h"
#include "nsIWidgetListener.h"

#include "nsINodeList.h"

#include "nsITimer.h"
#include "nsXULPopupManager.h"

#include "nsFocusManager.h"

#include "nsIWebProgress.h"
#include "nsIWebProgressListener.h"

#include "nsIDocument.h"
#include "nsIDOMNode.h"
#include "nsIDocumentLoaderFactory.h"
#include "nsIObserverService.h"

#include "nsIScreenManager.h"
#include "nsIScreen.h"

#include "nsIContent.h" // for menus
#include "nsIScriptSecurityManager.h"

// For calculating size
#include "nsIPresShell.h"
#include "nsPresContext.h"

#include "nsIBaseWindow.h"
#include "nsIDocShellTreeItem.h"

#include "mozilla/Attributes.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/EventDispatcher.h"
#include "mozilla/MouseEvents.h"

#include "nsPIWindowRoot.h"

#include "gfxPlatform.h"

#ifdef XP_MACOSX
#include "nsINativeMenuService.h"
#define USE_NATIVE_MENUS
#endif

using namespace mozilla;
using namespace mozilla::dom;

/* Define Class IDs */
static NS_DEFINE_CID(kWindowCID,           NS_WINDOW_CID);

#define SIZE_PERSISTENCE_TIMEOUT 500 // msec

nsWebShellWindow::nsWebShellWindow(uint32_t aChromeFlags)
  : nsXULWindow(aChromeFlags)
  , mSPTimerLock("nsWebShellWindow.mSPTimerLock")
  , mWidgetListenerDelegate(this)
{
}

nsWebShellWindow::~nsWebShellWindow()
{
  MutexAutoLock lock(mSPTimerLock);
  if (mSPTimer)
    mSPTimer->Cancel();
}

NS_IMPL_ADDREF_INHERITED(nsWebShellWindow, nsXULWindow)
NS_IMPL_RELEASE_INHERITED(nsWebShellWindow, nsXULWindow)

NS_INTERFACE_MAP_BEGIN(nsWebShellWindow)
  NS_INTERFACE_MAP_ENTRY(nsIWebProgressListener)
NS_INTERFACE_MAP_END_INHERITING(nsXULWindow)

nsresult nsWebShellWindow::Initialize(nsIXULWindow* aParent,
                                      nsIXULWindow* aOpener,
                                      nsIURI* aUrl,
                                      int32_t aInitialWidth,
                                      int32_t aInitialHeight,
                                      bool aIsHiddenWindow,
                                      nsITabParent *aOpeningTab,
                                      mozIDOMWindowProxy *aOpenerWindow,
                                      nsWidgetInitData& widgetInitData)
{
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
        mOpenerScreenRect.SetRect(NSToIntRound(x / scale),
                                  NSToIntRound(y / scale),
                                  NSToIntRound(width / scale),
                                  NSToIntRound(height / scale));
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
    if (!mWindow) {
      return NS_ERROR_FAILURE;
    }
  } else {
    mWindow = do_CreateInstance(kWindowCID, &rv);
    if (NS_OK != rv) {
      return rv;
    }
  }

  /* This next bit is troublesome. We carry two different versions of a pointer
     to our parent window. One is the parent window's widget, which is passed
     to our own widget. The other is a weak reference we keep here to our
     parent WebShellWindow. The former is useful to the widget, and we can't
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
  rv = mWindow->Create((nsIWidget *)parentWidget, // Parent nsIWidget
                       nullptr,                   // Native parent widget
                       deskRect,                  // Widget dimensions
                       &widgetInitData);          // Widget initialization data
  NS_ENSURE_SUCCESS(rv, rv);

  LayoutDeviceIntRect r = mWindow->GetClientBounds();
  // Match the default background color of content. Important on windows
  // since we no longer use content child widgets.
  mWindow->SetBackgroundColor(NS_RGB(255,255,255));

  // Create web shell
  mDocShell = do_CreateInstance("@mozilla.org/docshell;1");
  NS_ENSURE_TRUE(mDocShell, NS_ERROR_FAILURE);

  mDocShell->SetOpener(aOpeningTab);

  // Make sure to set the item type on the docshell _before_ calling
  // Create() so it knows what type it is.
  nsCOMPtr<nsIDocShellTreeItem> docShellAsItem(do_QueryInterface(mDocShell));
  NS_ENSURE_TRUE(docShellAsItem, NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(EnsureChromeTreeOwner(), NS_ERROR_FAILURE);

  docShellAsItem->SetTreeOwner(mChromeTreeOwner);
  docShellAsItem->SetItemType(nsIDocShellTreeItem::typeChrome);

  r.MoveTo(0, 0);
  nsCOMPtr<nsIBaseWindow> docShellAsWin(do_QueryInterface(mDocShell));
  NS_ENSURE_SUCCESS(docShellAsWin->InitWindow(nullptr, mWindow,
   r.X(), r.Y(), r.Width(), r.Height()), NS_ERROR_FAILURE);
  NS_ENSURE_SUCCESS(docShellAsWin->Create(), NS_ERROR_FAILURE);

  // Attach a WebProgress listener.during initialization...
  nsCOMPtr<nsIWebProgress> webProgress(do_GetInterface(mDocShell, &rv));
  if (webProgress) {
    webProgress->AddProgressListener(this, nsIWebProgress::NOTIFY_STATE_NETWORK);
  }

  if (aOpenerWindow) {
    nsPIDOMWindowOuter* window = mDocShell->GetWindow();
    MOZ_ASSERT(window);
    window->SetOpenerWindow(nsPIDOMWindowOuter::From(aOpenerWindow), true);
  }

  // Eagerly create an about:blank content viewer with the right principal here,
  // rather than letting it happening in the upcoming call to
  // SetInitialPrincipalToSubject. This avoids creating the about:blank document
  // and then blowing it away with a second one, which can cause problems for the
  // top-level chrome window case. See bug 789773.
  // Note that we don't accept expanded principals here, similar to
  // SetInitialPrincipalToSubject.
  if (nsContentUtils::IsInitialized()) { // Sometimes this happens really early  See bug 793370.
    MOZ_ASSERT(mDocShell->ItemType() == nsIDocShellTreeItem::typeChrome);
    nsCOMPtr<nsIPrincipal> principal = nsContentUtils::SubjectPrincipalOrSystemIfNativeCaller();
    if (nsContentUtils::IsExpandedPrincipal(principal)) {
      principal = nullptr;
    }
    rv = mDocShell->CreateAboutBlankContentViewer(principal);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIDocument> doc = mDocShell->GetDocument();
    NS_ENSURE_TRUE(!!doc, NS_ERROR_FAILURE);
    doc->SetIsInitialDocument(true);
  }

  if (nullptr != aUrl)  {
    nsCString tmpStr;

    rv = aUrl->GetSpec(tmpStr);
    if (NS_FAILED(rv)) return rv;

    NS_ConvertUTF8toUTF16 urlString(tmpStr);
    nsCOMPtr<nsIWebNavigation> webNav(do_QueryInterface(mDocShell));
    NS_ENSURE_TRUE(webNav, NS_ERROR_FAILURE);
    rv = webNav->LoadURI(urlString.get(),
                         nsIWebNavigation::LOAD_FLAGS_NONE,
                         nullptr,
                         nullptr,
                         nullptr,
                         nsContentUtils::GetSystemPrincipal());
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return rv;
}

nsIPresShell*
nsWebShellWindow::GetPresShell()
{
  if (!mDocShell)
    return nullptr;

  return mDocShell->GetPresShell();
}

bool
nsWebShellWindow::WindowMoved(nsIWidget* aWidget, int32_t x, int32_t y)
{
  nsXULPopupManager* pm = nsXULPopupManager::GetInstance();
  if (pm) {
    nsCOMPtr<nsPIDOMWindowOuter> window =
      mDocShell ? mDocShell->GetWindow() : nullptr;
    pm->AdjustPopupsOnWindowChange(window);
  }

  // Notify all tabs that the widget moved.
  if (mDocShell && mDocShell->GetWindow()) {
    nsCOMPtr<EventTarget> eventTarget = mDocShell->GetWindow()->GetTopWindowRoot();
    nsContentUtils::DispatchChromeEvent(mDocShell->GetDocument(),
                                        eventTarget,
                                        NS_LITERAL_STRING("MozUpdateWindowPos"),
                                        false, false, nullptr);
  }

  // Persist position, but not immediately, in case this OS is firing
  // repeated move events as the user drags the window
  SetPersistenceTimer(PAD_POSITION);
  return false;
}

bool
nsWebShellWindow::WindowResized(nsIWidget* aWidget, int32_t aWidth, int32_t aHeight)
{
  nsCOMPtr<nsIBaseWindow> shellAsWin(do_QueryInterface(mDocShell));
  if (shellAsWin) {
    shellAsWin->SetPositionAndSize(0, 0, aWidth, aHeight, 0);
  }
  // Persist size, but not immediately, in case this OS is firing
  // repeated size events as the user drags the sizing handle
  if (!IsLocked())
    SetPersistenceTimer(PAD_POSITION | PAD_SIZE | PAD_MISC);
  return true;
}

bool
nsWebShellWindow::RequestWindowClose(nsIWidget* aWidget)
{
  // Maintain a reference to this as it is about to get destroyed.
  nsCOMPtr<nsIXULWindow> xulWindow(this);

  nsCOMPtr<nsPIDOMWindowOuter> window(mDocShell ? mDocShell->GetWindow() : nullptr);
  nsCOMPtr<EventTarget> eventTarget = do_QueryInterface(window);

  nsCOMPtr<nsIPresShell> presShell = mDocShell->GetPresShell();
  if (!presShell) {
    mozilla::DebugOnly<bool> dying;
    MOZ_ASSERT(NS_SUCCEEDED(mDocShell->IsBeingDestroyed(&dying)) && dying,
               "No presShell, but window is not being destroyed");
  } else if (eventTarget) {
    RefPtr<nsPresContext> presContext = presShell->GetPresContext();

    nsEventStatus status = nsEventStatus_eIgnore;
    WidgetMouseEvent event(true, eClose, nullptr,
                           WidgetMouseEvent::eReal);
    if (NS_SUCCEEDED(EventDispatcher::Dispatch(eventTarget, presContext,
                                               &event, nullptr, &status)) &&
        status == nsEventStatus_eConsumeNoDefault)
      return false;
  }

  Destroy();
  return false;
}

void
nsWebShellWindow::SizeModeChanged(nsSizeMode sizeMode)
{
  // An alwaysRaised (or higher) window will hide any newly opened normal
  // browser windows, so here we just drop a raised window to the normal
  // zlevel if it's maximized. We make no provision for automatically
  // re-raising it when restored.
  if (sizeMode == nsSizeMode_Maximized || sizeMode == nsSizeMode_Fullscreen) {
    uint32_t zLevel;
    GetZLevel(&zLevel);
    if (zLevel > nsIXULWindow::normalZ)
      SetZLevel(nsIXULWindow::normalZ);
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
    }
    else if (sizeMode != nsSizeMode_Minimized) {
      if (ourWindow->GetFullScreen()) {
        // The first SetFullscreenInternal call below ensures that we do
        // not trigger any fullscreen transition even if the window was
        // put in fullscreen only for the Fullscreen API. The second
        // SetFullScreen call ensures that the window really exit from
        // fullscreen even if it entered fullscreen for both Fullscreen
        // Mode and Fullscreen API.
        ourWindow->SetFullscreenInternal(FullscreenReason::ForForceExitFullscreen, false);
        ourWindow->SetFullScreen(false);
      }
    }

    // And always fire a user-defined sizemodechange event on the window
    ourWindow->DispatchCustomEvent(NS_LITERAL_STRING("sizemodechange"));
  }

  nsIPresShell* presShell;
  if ((presShell = GetPresShell())) {
    presShell->GetPresContext()->SizeModeChanged(sizeMode);
  }

  // Note the current implementation of SetSizeMode just stores
  // the new state; it doesn't actually resize. So here we store
  // the state and pass the event on to the OS. The day is coming
  // when we'll handle the event here, and the return result will
  // then need to be different.
}

void
nsWebShellWindow::UIResolutionChanged()
{
  nsCOMPtr<nsPIDOMWindowOuter> ourWindow =
    mDocShell ? mDocShell->GetWindow() : nullptr;
  if (ourWindow) {
    ourWindow->DispatchCustomEvent(NS_LITERAL_STRING("resolutionchange"));
  }
}

void
nsWebShellWindow::FullscreenWillChange(bool aInFullscreen)
{
  if (mDocShell) {
    if (nsCOMPtr<nsPIDOMWindowOuter> ourWindow = mDocShell->GetWindow()) {
      ourWindow->FullscreenWillChange(aInFullscreen);
    }
  }
}

void
nsWebShellWindow::FullscreenChanged(bool aInFullscreen)
{
  if (mDocShell) {
    if (nsCOMPtr<nsPIDOMWindowOuter> ourWindow = mDocShell->GetWindow()) {
      ourWindow->FinishFullscreenChange(aInFullscreen);
    }
  }
}

void
nsWebShellWindow::OcclusionStateChanged(bool aIsFullyOccluded)
{
  nsCOMPtr<nsPIDOMWindowOuter> ourWindow =
    mDocShell ? mDocShell->GetWindow() : nullptr;
  if (ourWindow) {
    // And always fire a user-defined occlusionstatechange event on the window
    ourWindow->DispatchCustomEvent(NS_LITERAL_STRING("occlusionstatechange"));
  }
}

void
nsWebShellWindow::OSToolbarButtonPressed()
{
  // Keep a reference as setting the chrome flags can fire events.
  nsCOMPtr<nsIXULWindow> xulWindow(this);

  // rjc: don't use "nsIWebBrowserChrome::CHROME_EXTRA"
  //      due to components with multiple sidebar components
  //      (such as Mail/News, Addressbook, etc)... and frankly,
  //      Mac IE, OmniWeb, and other Mac OS X apps all work this way
  uint32_t    chromeMask = (nsIWebBrowserChrome::CHROME_TOOLBAR |
                            nsIWebBrowserChrome::CHROME_LOCATIONBAR |
                            nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR);

  nsCOMPtr<nsIWebBrowserChrome> wbc(do_GetInterface(xulWindow));
  if (!wbc)
    return;

  uint32_t    chromeFlags, newChromeFlags = 0;
  wbc->GetChromeFlags(&chromeFlags);
  newChromeFlags = chromeFlags & chromeMask;
  if (!newChromeFlags)    chromeFlags |= chromeMask;
  else                    chromeFlags &= (~newChromeFlags);
  wbc->SetChromeFlags(chromeFlags);
}

bool
nsWebShellWindow::ZLevelChanged(bool aImmediate, nsWindowZ *aPlacement,
                                nsIWidget* aRequestBelow, nsIWidget** aActualBelow)
{
  if (aActualBelow)
    *aActualBelow = nullptr;

  return ConstrainToZLevel(aImmediate, aPlacement, aRequestBelow, aActualBelow);
}

void
nsWebShellWindow::WindowActivated()
{
  nsCOMPtr<nsIXULWindow> xulWindow(this);

  // focusing the window could cause it to close, so keep a reference to it
  nsCOMPtr<nsPIDOMWindowOuter> window = mDocShell ? mDocShell->GetWindow() : nullptr;
  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  if (fm && window)
    fm->WindowRaised(window);

  if (mChromeLoaded) {
    PersistentAttributesDirty(PAD_POSITION | PAD_SIZE | PAD_MISC);
    SavePersistentAttributes();
   }
}

void
nsWebShellWindow::WindowDeactivated()
{
  nsCOMPtr<nsIXULWindow> xulWindow(this);

  nsCOMPtr<nsPIDOMWindowOuter> window =
    mDocShell ? mDocShell->GetWindow() : nullptr;
  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  if (fm && window)
    fm->WindowLowered(window);
}

#ifdef USE_NATIVE_MENUS
static void LoadNativeMenus(nsIDocument *aDoc, nsIWidget *aParentWindow)
{
  if (gfxPlatform::IsHeadless()) {
    return;
  }
  nsCOMPtr<nsINativeMenuService> nms = do_GetService("@mozilla.org/widget/nativemenuservice;1");
  if (!nms) {
    return;
  }

  // Find the menubar tag (if there is more than one, we ignore all but
  // the first).
  nsCOMPtr<nsINodeList> menubarElements =
    aDoc->GetElementsByTagNameNS(NS_LITERAL_STRING("http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"),
                                 NS_LITERAL_STRING("menubar"));

  nsCOMPtr<nsINode> menubarNode;
  if (menubarElements) {
    menubarNode = menubarElements->Item(0);
  }

  if (menubarNode) {
    nsCOMPtr<Element> menubarContent(do_QueryInterface(menubarNode));
    nms->CreateNativeMenuBar(aParentWindow, menubarContent);
  } else {
    nms->CreateNativeMenuBar(aParentWindow, nullptr);
  }
}
#endif

namespace mozilla {

class WebShellWindowTimerCallback final : public nsITimerCallback
                                        , public nsINamed
{
public:
  explicit WebShellWindowTimerCallback(nsWebShellWindow* aWindow)
    : mWindow(aWindow)
  {}

  NS_DECL_THREADSAFE_ISUPPORTS

  NS_IMETHOD Notify(nsITimer* aTimer) override
  {
    // Although this object participates in a refcount cycle (this -> mWindow
    // -> mSPTimer -> this), mSPTimer is a one-shot timer and releases this
    // after it fires.  So we don't need to release mWindow here.

    mWindow->FirePersistenceTimer();
    return NS_OK;
  }

  NS_IMETHOD GetName(nsACString& aName) override
  {
    aName.AssignLiteral("WebShellWindowTimerCallback");
    return NS_OK;
  }

private:
  ~WebShellWindowTimerCallback() {}

  RefPtr<nsWebShellWindow> mWindow;
};

NS_IMPL_ISUPPORTS(WebShellWindowTimerCallback, nsITimerCallback, nsINamed)

} // namespace mozilla

void
nsWebShellWindow::SetPersistenceTimer(uint32_t aDirtyFlags)
{
  MutexAutoLock lock(mSPTimerLock);
  if (!mSPTimer) {
    mSPTimer = NS_NewTimer();
    if (!mSPTimer) {
      NS_WARNING("Couldn't create @mozilla.org/timer;1 instance?");
      return;
    }
  }

  RefPtr<WebShellWindowTimerCallback> callback =
    new WebShellWindowTimerCallback(this);
  mSPTimer->InitWithCallback(callback, SIZE_PERSISTENCE_TIMEOUT,
                             nsITimer::TYPE_ONE_SHOT);

  PersistentAttributesDirty(aDirtyFlags);
}

void
nsWebShellWindow::FirePersistenceTimer()
{
  MutexAutoLock lock(mSPTimerLock);
  SavePersistentAttributes();
}


//----------------------------------------
// nsIWebProgessListener implementation
//----------------------------------------
NS_IMETHODIMP
nsWebShellWindow::OnProgressChange(nsIWebProgress *aProgress,
                                   nsIRequest *aRequest,
                                   int32_t aCurSelfProgress,
                                   int32_t aMaxSelfProgress,
                                   int32_t aCurTotalProgress,
                                   int32_t aMaxTotalProgress)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::OnStateChange(nsIWebProgress *aProgress,
                                nsIRequest *aRequest,
                                uint32_t aStateFlags,
                                nsresult aStatus)
{
  // If the notification is not about a document finishing, then just
  // ignore it...
  if (!(aStateFlags & nsIWebProgressListener::STATE_STOP) ||
      !(aStateFlags & nsIWebProgressListener::STATE_IS_NETWORK)) {
    return NS_OK;
  }

  if (mChromeLoaded)
    return NS_OK;

  // If this document notification is for a frame then ignore it...
  nsCOMPtr<mozIDOMWindowProxy> eventWin;
  aProgress->GetDOMWindow(getter_AddRefs(eventWin));
  auto* eventPWin = nsPIDOMWindowOuter::From(eventWin);
  if (eventPWin) {
    nsPIDOMWindowOuter *rootPWin = eventPWin->GetPrivateRoot();
    if (eventPWin != rootPWin)
      return NS_OK;
  }

  mChromeLoaded = true;
  mLockedUntilChromeLoad = false;

#ifdef USE_NATIVE_MENUS
  ///////////////////////////////
  // Find the Menubar DOM  and Load the menus, hooking them up to the loaded commands
  ///////////////////////////////
  nsCOMPtr<nsIContentViewer> cv;
  mDocShell->GetContentViewer(getter_AddRefs(cv));
  if (cv) {
    nsCOMPtr<nsIDocument> menubarDoc = cv->GetDocument();
    if (menubarDoc)
      LoadNativeMenus(menubarDoc, mWindow);
  }
#endif // USE_NATIVE_MENUS

  OnChromeLoaded();

  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::OnLocationChange(nsIWebProgress *aProgress,
                                   nsIRequest *aRequest,
                                   nsIURI *aURI,
                                   uint32_t aFlags)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::OnStatusChange(nsIWebProgress* aWebProgress,
                                 nsIRequest* aRequest,
                                 nsresult aStatus,
                                 const char16_t* aMessage)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}

NS_IMETHODIMP
nsWebShellWindow::OnSecurityChange(nsIWebProgress *aWebProgress,
                                   nsIRequest *aRequest,
                                   uint32_t state)
{
  NS_NOTREACHED("notification excluded in AddProgressListener(...)");
  return NS_OK;
}


/**
 * ExecuteCloseHandler - Run the close handler, if any.
 * @return true iff we found a close handler to run.
 */
bool nsWebShellWindow::ExecuteCloseHandler()
{
  /* If the event handler closes this window -- a likely scenario --
     things get deleted out of order without this death grip.
     (The problem may be the death grip in nsWindow::windowProc,
     which forces this window's widget to remain alive longer
     than it otherwise would.) */
  nsCOMPtr<nsIXULWindow> kungFuDeathGrip(this);

  nsCOMPtr<EventTarget> eventTarget;
  if (mDocShell) {
    eventTarget = do_QueryInterface(mDocShell->GetWindow());
  }

  if (eventTarget) {
    nsCOMPtr<nsIContentViewer> contentViewer;
    mDocShell->GetContentViewer(getter_AddRefs(contentViewer));
    if (contentViewer) {
      RefPtr<nsPresContext> presContext;
      contentViewer->GetPresContext(getter_AddRefs(presContext));

      nsEventStatus status = nsEventStatus_eIgnore;
      WidgetMouseEvent event(true, eClose, nullptr,
                             WidgetMouseEvent::eReal);

      nsresult rv = EventDispatcher::Dispatch(eventTarget, presContext,
                                              &event, nullptr, &status);
      if (NS_SUCCEEDED(rv) && status == nsEventStatus_eConsumeNoDefault)
        return true;
      // else fall through and return false
    }
  }

  return false;
} // ExecuteCloseHandler

void nsWebShellWindow::ConstrainToOpenerScreen(int32_t* aX, int32_t* aY)
{
  if (mOpenerScreenRect.IsEmpty()) {
    *aX = *aY = 0;
    return;
  }

  int32_t left, top, width, height;
  // Constrain initial positions to the same screen as opener
  nsCOMPtr<nsIScreenManager> screenmgr = do_GetService("@mozilla.org/gfx/screenmanager;1");
  if (screenmgr) {
    nsCOMPtr<nsIScreen> screen;
    screenmgr->ScreenForRect(mOpenerScreenRect.X(), mOpenerScreenRect.Y(),
                             mOpenerScreenRect.Width(), mOpenerScreenRect.Height(),
                             getter_AddRefs(screen));
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

// nsIBaseWindow
NS_IMETHODIMP nsWebShellWindow::Destroy()
{
  nsresult rv;
  nsCOMPtr<nsIWebProgress> webProgress(do_GetInterface(mDocShell, &rv));
  if (webProgress) {
    webProgress->RemoveProgressListener(this);
  }

  nsCOMPtr<nsIXULWindow> kungFuDeathGrip(this);
  {
    MutexAutoLock lock(mSPTimerLock);
    if (mSPTimer) {
      mSPTimer->Cancel();
      SavePersistentAttributes();
      mSPTimer = nullptr;
    }
  }
  return nsXULWindow::Destroy();
}

nsIXULWindow*
nsWebShellWindow::WidgetListenerDelegate::GetXULWindow()
{
  return mWebShellWindow->GetXULWindow();
}

nsIPresShell*
nsWebShellWindow::WidgetListenerDelegate::GetPresShell()
{
  return mWebShellWindow->GetPresShell();
}

bool
nsWebShellWindow::WidgetListenerDelegate::WindowMoved(
  nsIWidget* aWidget, int32_t aX, int32_t aY)
{
  RefPtr<nsWebShellWindow> holder = mWebShellWindow;
  return holder->WindowMoved(aWidget, aX, aY);
}

bool
nsWebShellWindow::WidgetListenerDelegate::WindowResized(
  nsIWidget* aWidget, int32_t aWidth, int32_t aHeight)
{
  RefPtr<nsWebShellWindow> holder = mWebShellWindow;
  return holder->WindowResized(aWidget, aWidth, aHeight);
}

bool
nsWebShellWindow::WidgetListenerDelegate::RequestWindowClose(nsIWidget* aWidget)
{
  RefPtr<nsWebShellWindow> holder = mWebShellWindow;
  return holder->RequestWindowClose(aWidget);
}

void
nsWebShellWindow::WidgetListenerDelegate::SizeModeChanged(nsSizeMode aSizeMode)
{
  RefPtr<nsWebShellWindow> holder = mWebShellWindow;
  holder->SizeModeChanged(aSizeMode);
}

void
nsWebShellWindow::WidgetListenerDelegate::UIResolutionChanged()
{
  RefPtr<nsWebShellWindow> holder = mWebShellWindow;
  holder->UIResolutionChanged();
}

void
nsWebShellWindow::WidgetListenerDelegate::FullscreenWillChange(
  bool aInFullscreen)
{
  RefPtr<nsWebShellWindow> holder = mWebShellWindow;
  holder->FullscreenWillChange(aInFullscreen);
}

void
nsWebShellWindow::WidgetListenerDelegate::FullscreenChanged(bool aInFullscreen)
{
  RefPtr<nsWebShellWindow> holder = mWebShellWindow;
  holder->FullscreenChanged(aInFullscreen);
}

void
nsWebShellWindow::WidgetListenerDelegate::OcclusionStateChanged(
  bool aIsFullyOccluded)
{
  RefPtr<nsWebShellWindow> holder = mWebShellWindow;
  holder->OcclusionStateChanged(aIsFullyOccluded);
}

void
nsWebShellWindow::WidgetListenerDelegate::OSToolbarButtonPressed()
{
  RefPtr<nsWebShellWindow> holder = mWebShellWindow;
  holder->OSToolbarButtonPressed();
}

bool
nsWebShellWindow::WidgetListenerDelegate::ZLevelChanged(
  bool aImmediate, nsWindowZ *aPlacement, nsIWidget* aRequestBelow,
  nsIWidget** aActualBelow)
{
  RefPtr<nsWebShellWindow> holder = mWebShellWindow;
  return holder->ZLevelChanged(aImmediate,
                               aPlacement,
                               aRequestBelow,
                               aActualBelow);
}

void
nsWebShellWindow::WidgetListenerDelegate::WindowActivated()
{
  RefPtr<nsWebShellWindow> holder = mWebShellWindow;
  holder->WindowActivated();
}

void
nsWebShellWindow::WidgetListenerDelegate::WindowDeactivated()
{
  RefPtr<nsWebShellWindow> holder = mWebShellWindow;
  holder->WindowDeactivated();
}
