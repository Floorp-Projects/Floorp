/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCocoaWindow.h"

#include "NativeKeyBindings.h"
#include "ScreenHelperCocoa.h"
#include "TextInputHandler.h"
#include "nsCocoaUtils.h"
#include "nsObjCExceptions.h"
#include "nsCOMPtr.h"
#include "nsWidgetsCID.h"
#include "nsIRollupListener.h"
#include "nsChildView.h"
#include "nsWindowMap.h"
#include "nsAppShell.h"
#include "nsIAppShellService.h"
#include "nsIBaseWindow.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIAppWindow.h"
#include "nsToolkit.h"
#include "nsPIDOMWindow.h"
#include "nsThreadUtils.h"
#include "nsMenuBarX.h"
#include "nsMenuUtilsX.h"
#include "nsStyleConsts.h"
#include "nsNativeThemeColors.h"
#include "nsNativeThemeCocoa.h"
#include "nsChildView.h"
#include "nsCocoaFeatures.h"
#include "nsIScreenManager.h"
#include "nsIWidgetListener.h"
#include "nsXULPopupManager.h"
#include "VibrancyManager.h"
#include "nsPresContext.h"
#include "nsDocShell.h"

#include "gfxPlatform.h"
#include "qcms.h"

#include "mozilla/AutoRestore.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/dom/Document.h"
#include "mozilla/Maybe.h"
#include "mozilla/NativeKeyBindingsType.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include "mozilla/ScopeExit.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_widget.h"
#include "mozilla/WritingModes.h"
#include "mozilla/layers/CompositorBridgeChild.h"
#include "mozilla/widget/Screen.h"
#include <algorithm>

namespace mozilla {
namespace layers {
class LayerManager;
}  // namespace layers
}  // namespace mozilla
using namespace mozilla::layers;
using namespace mozilla::widget;
using namespace mozilla;

BOOL sTouchBarIsInitialized = NO;

// defined in nsMenuBarX.mm
extern NSMenu* sApplicationMenu;  // Application menu shared by all menubars

// defined in nsChildView.mm
extern BOOL gSomeMenuBarPainted;

static uint32_t sModalWindowCount = 0;

extern "C" {
// CGSPrivate.h
typedef NSInteger CGSConnection;
typedef NSUInteger CGSSpaceID;
typedef NSInteger CGSWindow;
typedef enum {
  kCGSSpaceIncludesCurrent = 1 << 0,
  kCGSSpaceIncludesOthers = 1 << 1,
  kCGSSpaceIncludesUser = 1 << 2,

  kCGSAllSpacesMask =
      kCGSSpaceIncludesCurrent | kCGSSpaceIncludesOthers | kCGSSpaceIncludesUser
} CGSSpaceMask;
static NSString* const CGSSpaceIDKey = @"ManagedSpaceID";
static NSString* const CGSSpacesKey = @"Spaces";
extern CGSConnection _CGSDefaultConnection(void);
extern CGError CGSSetWindowTransform(CGSConnection cid, CGSWindow wid,
                                     CGAffineTransform transform);
}

#define NS_APPSHELLSERVICE_CONTRACTID "@mozilla.org/appshell/appShellService;1"

static void RollUpPopups(nsIRollupListener::AllowAnimations aAllowAnimations =
                             nsIRollupListener::AllowAnimations::Yes) {
  if (RefPtr pm = nsXULPopupManager::GetInstance()) {
    pm->RollupTooltips();
  }

  nsIRollupListener* rollupListener = nsBaseWidget::GetActiveRollupListener();
  if (!rollupListener) {
    return;
  }
  if (rollupListener->RollupNativeMenu()) {
    return;
  }
  nsCOMPtr<nsIWidget> rollupWidget = rollupListener->GetRollupWidget();
  if (!rollupWidget) {
    return;
  }
  nsIRollupListener::RollupOptions options{
      0, nsIRollupListener::FlushViews::Yes, nullptr, aAllowAnimations};
  rollupListener->Rollup(options);
}

nsCocoaWindow::nsCocoaWindow()
    : mParent(nullptr),
      mAncestorLink(nullptr),
      mWindow(nil),
      mDelegate(nil),
      mPopupContentView(nil),
      mFullscreenTransitionAnimation(nil),
      mShadowStyle(WindowShadow::None),
      mBackingScaleFactor(0.0),
      mAnimationType(nsIWidget::eGenericWindowAnimation),
      mWindowMadeHere(false),
      mSizeMode(nsSizeMode_Normal),
      mInFullScreenMode(false),
      mInNativeFullScreenMode(false),
      mIgnoreOcclusionCount(0),
      mHasStartedNativeFullscreen(false),
      mWindowAnimationBehavior(NSWindowAnimationBehaviorDefault) {
  // Disable automatic tabbing. We need to do this before we
  // orderFront any of our windows.
  NSWindow.allowsAutomaticWindowTabbing = NO;
}

void nsCocoaWindow::DestroyNativeWindow() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!mWindow) {
    return;
  }

  MOZ_ASSERT(mWindowMadeHere,
             "We shouldn't be trying to destroy a window we didn't create.");

  // Clear our class state that is keyed off of mWindow. It's our last
  // chance! This ensures that other nsCocoaWindow instances are not waiting
  // for us to finish a native transition that will have no listener once
  // we clear our delegate.
  EndOurNativeTransition();

  // We are about to destroy mWindow. Before we do that, make sure that we
  // hide the window using the Show() method, because it has several side
  // effects that our parent and listeners might be expecting. If we don't
  // do this now, then these side effects will never execute, though the
  // window will definitely no longer be shown.
  Show(false);

  [mWindow releaseJSObjects];
  // We want to unhook the delegate here because we don't want events
  // sent to it after this object has been destroyed.
  mWindow.delegate = nil;
  [mWindow close];
  mWindow = nil;
  [mDelegate autorelease];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

nsCocoaWindow::~nsCocoaWindow() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  // Notify the children that we're gone.  Popup windows (e.g. tooltips) can
  // have nsChildView children.  'kid' is an nsChildView object if and only if
  // its 'type' is 'WindowType::Child'.
  // childView->ResetParent() can change our list of children while it's
  // being iterated, so the way we iterate the list must allow for this.
  for (nsIWidget* kid = mLastChild; kid;) {
    WindowType kidType = kid->GetWindowType();
    if (kidType == WindowType::Child) {
      nsChildView* childView = static_cast<nsChildView*>(kid);
      kid = kid->GetPrevSibling();
      childView->ResetParent();
    } else {
      nsCocoaWindow* childWindow = static_cast<nsCocoaWindow*>(kid);
      childWindow->mParent = nullptr;
      childWindow->mAncestorLink = mAncestorLink;
      kid = kid->GetPrevSibling();
    }
  }

  if (mWindow && mWindowMadeHere) {
    CancelAllTransitions();
    DestroyNativeWindow();
  }

  NS_IF_RELEASE(mPopupContentView);
  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// Find the screen that overlaps aRect the most,
// if none are found default to the mainScreen.
static NSScreen* FindTargetScreenForRect(const DesktopIntRect& aRect) {
  NSScreen* targetScreen = [NSScreen mainScreen];
  NSEnumerator* screenEnum = [[NSScreen screens] objectEnumerator];
  int largestIntersectArea = 0;
  while (NSScreen* screen = [screenEnum nextObject]) {
    DesktopIntRect screenRect =
        nsCocoaUtils::CocoaRectToGeckoRect([screen visibleFrame]);
    screenRect = screenRect.Intersect(aRect);
    int area = screenRect.width * screenRect.height;
    if (area > largestIntersectArea) {
      largestIntersectArea = area;
      targetScreen = screen;
    }
  }
  return targetScreen;
}

DesktopToLayoutDeviceScale ParentBackingScaleFactor(nsIWidget* aParent,
                                                    NSView* aParentView) {
  if (aParent) {
    return aParent->GetDesktopToDeviceScale();
  }
  NSWindow* parentWindow = [aParentView window];
  if (parentWindow) {
    return DesktopToLayoutDeviceScale(parentWindow.backingScaleFactor);
  }
  return DesktopToLayoutDeviceScale(1.0);
}

// Returns the screen rectangle for the given widget.
// Child widgets are positioned relative to this rectangle.
// Exactly one of the arguments must be non-null.
static DesktopRect GetWidgetScreenRectForChildren(nsIWidget* aWidget,
                                                  NSView* aView) {
  if (aWidget) {
    mozilla::DesktopToLayoutDeviceScale scale =
        aWidget->GetDesktopToDeviceScale();
    if (aWidget->GetWindowType() == WindowType::Child) {
      return aWidget->GetScreenBounds() / scale;
    }
    return aWidget->GetClientBounds() / scale;
  }

  MOZ_RELEASE_ASSERT(aView);

  // 1. Transform the view rect into window coords.
  // The returned rect is in "origin bottom-left" coordinates.
  NSRect rectInWindowCoordinatesOBL = [aView convertRect:[aView bounds]
                                                  toView:nil];

  // 2. Turn the window-coord rect into screen coords, still origin bottom-left.
  NSRect rectInScreenCoordinatesOBL =
      [[aView window] convertRectToScreen:rectInWindowCoordinatesOBL];

  // 3. Convert the NSRect to a DesktopRect. This will convert to coordinates
  // with the origin in the top left corner of the primary screen.
  return DesktopRect(
      nsCocoaUtils::CocoaRectToGeckoRect(rectInScreenCoordinatesOBL));
}

// aRect here is specified in desktop pixels
//
// For child windows (where either aParent or aNativeParent is non-null),
// aRect.{x,y} are offsets from the origin of the parent window and not an
// absolute position.
nsresult nsCocoaWindow::Create(nsIWidget* aParent, nsNativeWidget aNativeParent,
                               const DesktopIntRect& aRect,
                               widget::InitData* aInitData) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // Because the hidden window is created outside of an event loop,
  // we have to provide an autorelease pool (see bug 559075).
  nsAutoreleasePool localPool;

  // Set defaults which can be overriden from aInitData in BaseCreate
  mWindowType = WindowType::TopLevel;
  mBorderStyle = BorderStyle::Default;

  // Ensure that the toolkit is created.
  nsToolkit::GetToolkit();

  Inherited::BaseCreate(aParent, aInitData);

  mParent = aParent;
  mAncestorLink = aParent;
  mAlwaysOnTop = aInitData->mAlwaysOnTop;
  mIsAlert = aInitData->mIsAlert;

  // If we have a parent widget, the new widget will be offset from the
  // parent widget by aRect.{x,y}. Otherwise, we'll use aRect for the
  // new widget coordinates.
  DesktopIntPoint parentOrigin;

  // Do we have a parent widget?
  if (aParent || aNativeParent) {
    DesktopRect parentDesktopRect =
        GetWidgetScreenRectForChildren(aParent, (NSView*)aNativeParent);
    parentOrigin = gfx::RoundedToInt(parentDesktopRect.TopLeft());
  }

  DesktopIntRect widgetRect = aRect + parentOrigin;

  nsresult rv =
      CreateNativeWindow(nsCocoaUtils::GeckoRectToCocoaRect(widgetRect),
                         mBorderStyle, false, aInitData->mIsPrivate);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mWindowType == WindowType::Popup) {
    // now we can convert widgetRect to device pixels for the window we created,
    // as the child view expects a rect expressed in the dev pix of its parent
    LayoutDeviceIntRect devRect =
        RoundedToInt(aRect * GetDesktopToDeviceScale());
    return CreatePopupContentView(devRect, aInitData);
  }

  mIsAnimationSuppressed = aInitData->mIsAnimationSuppressed;

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult nsCocoaWindow::Create(nsIWidget* aParent, nsNativeWidget aNativeParent,
                               const LayoutDeviceIntRect& aRect,
                               widget::InitData* aInitData) {
  DesktopIntRect desktopRect = RoundedToInt(
      aRect / ParentBackingScaleFactor(aParent, (NSView*)aNativeParent));
  return Create(aParent, aNativeParent, desktopRect, aInitData);
}

static unsigned int WindowMaskForBorderStyle(BorderStyle aBorderStyle) {
  bool allOrDefault = (aBorderStyle == BorderStyle::All ||
                       aBorderStyle == BorderStyle::Default);

  /* Apple's docs on NSWindow styles say that "a window's style mask should
   * include NSWindowStyleMaskTitled if it includes any of the others [besides
   * NSWindowStyleMaskBorderless]".  This implies that a borderless window
   * shouldn't have any other styles than NSWindowStyleMaskBorderless.
   */
  if (!allOrDefault && !(aBorderStyle & BorderStyle::Title)) {
    if (aBorderStyle & BorderStyle::Minimize) {
      /* It appears that at a minimum, borderless windows can be miniaturizable,
       * effectively contradicting some of Apple's documentation referenced
       * above. One such exception is the screen share indicator, see
       * bug 1742877.
       */
      return NSWindowStyleMaskBorderless | NSWindowStyleMaskMiniaturizable;
    }
    return NSWindowStyleMaskBorderless;
  }

  unsigned int mask = NSWindowStyleMaskTitled;
  if (allOrDefault || aBorderStyle & BorderStyle::Close) {
    mask |= NSWindowStyleMaskClosable;
  }
  if (allOrDefault || aBorderStyle & BorderStyle::Minimize) {
    mask |= NSWindowStyleMaskMiniaturizable;
  }
  if (allOrDefault || aBorderStyle & BorderStyle::ResizeH) {
    mask |= NSWindowStyleMaskResizable;
  }

  return mask;
}

// If aRectIsFrameRect, aRect specifies the frame rect of the new window.
// Otherwise, aRect.x/y specify the position of the window's frame relative to
// the bottom of the menubar and aRect.width/height specify the size of the
// content rect.
nsresult nsCocoaWindow::CreateNativeWindow(const NSRect& aRect,
                                           BorderStyle aBorderStyle,
                                           bool aRectIsFrameRect,
                                           bool aIsPrivateBrowsing) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // We default to NSWindowStyleMaskBorderless, add features if needed.
  unsigned int features = NSWindowStyleMaskBorderless;

  // Configure the window we will create based on the window type.
  switch (mWindowType) {
    case WindowType::Invisible:
    case WindowType::Child:
      break;
    case WindowType::Popup:
      if (aBorderStyle != BorderStyle::Default &&
          mBorderStyle & BorderStyle::Title) {
        features |= NSWindowStyleMaskTitled;
        if (aBorderStyle & BorderStyle::Close) {
          features |= NSWindowStyleMaskClosable;
        }
      }
      break;
    case WindowType::TopLevel:
    case WindowType::Dialog:
      features = WindowMaskForBorderStyle(aBorderStyle);
      break;
    default:
      NS_ERROR("Unhandled window type!");
      return NS_ERROR_FAILURE;
  }

  NSRect contentRect;

  if (aRectIsFrameRect) {
    contentRect = [NSWindow contentRectForFrameRect:aRect styleMask:features];
  } else {
    /*
     * We pass a content area rect to initialize the native Cocoa window. The
     * content rect we give is the same size as the size we're given by gecko.
     * The origin we're given for non-popup windows is moved down by the height
     * of the menu bar so that an origin of (0,100) from gecko puts the window
     * 100 pixels below the top of the available desktop area. We also move the
     * origin down by the height of a title bar if it exists. This is so the
     * origin that gecko gives us for the top-left of  the window turns out to
     * be the top-left of the window we create. This is how it was done in
     * Carbon. If it ought to be different we'll probably need to look at all
     * the callers.
     *
     * Note: This means that if you put a secondary screen on top of your main
     * screen and open a window in the top screen, it'll be incorrectly shifted
     * down by the height of the menu bar. Same thing would happen in Carbon.
     *
     * Note: If you pass a rect with 0,0 for an origin, the window ends up in a
     * weird place for some reason. This stops that without breaking popups.
     */
    // Compensate for difference between frame and content area height (e.g.
    // title bar).
    NSRect newWindowFrame = [NSWindow frameRectForContentRect:aRect
                                                    styleMask:features];

    contentRect = aRect;
    contentRect.origin.y -= (newWindowFrame.size.height - aRect.size.height);

    if (mWindowType != WindowType::Popup) {
      contentRect.origin.y -= NSApp.mainMenu.menuBarHeight;
    }
  }

  // NSLog(@"Top-level window being created at Cocoa rect: %f, %f, %f, %f\n",
  //       rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);

  Class windowClass = [BaseWindow class];
  if ((mWindowType == WindowType::TopLevel ||
       mWindowType == WindowType::Dialog) &&
      (features & NSWindowStyleMaskTitled)) {
    // If we have a titlebar on a top-level window, we want to be able to
    // control the titlebar color (for unified windows), so use the special
    // ToolbarWindow class. Note that we need to check the window type because
    // we mark sheets as having titlebars.
    windowClass = [ToolbarWindow class];
  } else if (mWindowType == WindowType::Popup) {
    windowClass = [PopupWindow class];
    // If we're a popup window we need to use the PopupWindow class.
  } else if (features == NSWindowStyleMaskBorderless) {
    // If we're a non-popup borderless window we need to use the
    // BorderlessWindow class.
    windowClass = [BorderlessWindow class];
  }

  // Create the window
  mWindow = [[windowClass alloc] initWithContentRect:contentRect
                                           styleMask:features
                                             backing:NSBackingStoreBuffered
                                               defer:YES];

  // Make sure that window titles don't leak to disk in private browsing mode
  // due to macOS' resume feature.
  mWindow.restorable = !aIsPrivateBrowsing;
  if (aIsPrivateBrowsing) {
    [mWindow disableSnapshotRestoration];
  }

  // setup our notification delegate. Note that setDelegate: does NOT retain.
  mDelegate = [[WindowDelegate alloc] initWithGeckoWindow:this];
  mWindow.delegate = mDelegate;

  // Make sure that the content rect we gave has been honored.
  NSRect wantedFrame = [mWindow frameRectForChildViewRect:contentRect];
  if (!NSEqualRects(mWindow.frame, wantedFrame)) {
    // This can happen when the window is not on the primary screen.
    [mWindow setFrame:wantedFrame display:NO];
  }
  UpdateBounds();

  if (mWindowType == WindowType::Invisible) {
    mWindow.level = kCGDesktopWindowLevelKey;
  }

  if (mWindowType == WindowType::Popup) {
    SetPopupWindowLevel();
    mWindow.backgroundColor = NSColor.clearColor;
    mWindow.opaque = NO;

    // When multiple spaces are in use and the browser is assigned to a
    // particular space, override the "Assign To" space and display popups on
    // the active space. Does not work with multiple displays. See
    // NeedsRecreateToReshow() for multi-display with multi-space workaround.
    mWindow.collectionBehavior = mWindow.collectionBehavior |
                                 NSWindowCollectionBehaviorMoveToActiveSpace;
  } else {
    // Non-popup windows are always opaque.
    mWindow.opaque = YES;
  }

  if (mAlwaysOnTop || mIsAlert) {
    mWindow.level = NSFloatingWindowLevel;
    mWindow.collectionBehavior =
        mWindow.collectionBehavior | NSWindowCollectionBehaviorCanJoinAllSpaces;
  }
  mWindow.contentMinSize = NSMakeSize(60, 60);
  [mWindow disableCursorRects];

  // Make the window use CoreAnimation from the start, so that we don't
  // switch from a non-CA window to a CA-window in the middle.
  mWindow.contentView.wantsLayer = YES;

  // Make sure the window starts out not draggable by the background.
  // We will turn it on as necessary.
  mWindow.movableByWindowBackground = NO;

  [WindowDataMap.sharedWindowDataMap ensureDataForWindow:mWindow];
  mWindowMadeHere = true;

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

nsresult nsCocoaWindow::CreatePopupContentView(const LayoutDeviceIntRect& aRect,
                                               widget::InitData* aInitData) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // We need to make our content view a ChildView.
  mPopupContentView = new nsChildView();
  if (!mPopupContentView) return NS_ERROR_FAILURE;

  NS_ADDREF(mPopupContentView);

  nsIWidget* thisAsWidget = static_cast<nsIWidget*>(this);
  nsresult rv =
      mPopupContentView->Create(thisAsWidget, nullptr, aRect, aInitData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  NSView* contentView = mWindow.contentView;
  auto* childView = static_cast<ChildView*>(
      mPopupContentView->GetNativeData(NS_NATIVE_WIDGET));
  childView.frame = contentView.bounds;
  childView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
  [contentView addSubview:childView];

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

void nsCocoaWindow::Destroy() {
  if (mOnDestroyCalled) {
    return;
  }
  mOnDestroyCalled = true;

  // Deal with the possiblity that we're being destroyed while running modal.
  if (mModal) {
    SetModal(false);
  }

  // If we don't hide here we run into problems with panels, this is not ideal.
  // (Bug 891424)
  Show(false);

  if (mPopupContentView) mPopupContentView->Destroy();

  if (mFullscreenTransitionAnimation) {
    [mFullscreenTransitionAnimation stopAnimation];
    ReleaseFullscreenTransitionAnimation();
  }

  nsBaseWidget::Destroy();
  // nsBaseWidget::Destroy() calls GetParent()->RemoveChild(this). But we
  // don't implement GetParent(), so we need to do the equivalent here.
  if (mParent) {
    mParent->RemoveChild(this);
  }
  nsBaseWidget::OnDestroy();

  if (mInFullScreenMode && !mInNativeFullScreenMode) {
    // Keep these calls balanced for emulated fullscreen.
    nsCocoaUtils::HideOSChromeOnScreen(false);
  }

  // Destroy the native window here (and not wait for that to happen in our
  // destructor). Otherwise this might not happen for several seconds because
  // at least one object holding a reference to ourselves is usually waiting
  // to be garbage-collected.
  if (mWindow && mWindowMadeHere) {
    CancelAllTransitions();
    DestroyNativeWindow();
  }
}

void* nsCocoaWindow::GetNativeData(uint32_t aDataType) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  void* retVal = nullptr;

  switch (aDataType) {
    // to emulate how windows works, we always have to return a NSView
    // for NS_NATIVE_WIDGET
    case NS_NATIVE_WIDGET:
      retVal = mWindow.contentView;
      break;

    case NS_NATIVE_WINDOW:
      retVal = mWindow;
      break;

    case NS_NATIVE_GRAPHIC:
      // There isn't anything that makes sense to return here,
      // and it doesn't matter so just return nullptr.
      NS_ERROR("Requesting NS_NATIVE_GRAPHIC on a top-level window!");
      break;
    case NS_RAW_NATIVE_IME_CONTEXT: {
      retVal = GetPseudoIMEContext();
      if (retVal) {
        break;
      }
      NSView* view = mWindow ? mWindow.contentView : nil;
      if (view) {
        retVal = view.inputContext;
      }
      // If inputContext isn't available on this window, return this window's
      // pointer instead of nullptr since if this returns nullptr,
      // IMEStateManager cannot manage composition with TextComposition
      // instance.  Although, this case shouldn't occur.
      if (NS_WARN_IF(!retVal)) {
        retVal = this;
      }
      break;
    }
  }

  return retVal;

  NS_OBJC_END_TRY_BLOCK_RETURN(nullptr);
}

bool nsCocoaWindow::IsVisible() const {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  return mWindow && mWindow.isVisibleOrBeingShown;

  NS_OBJC_END_TRY_BLOCK_RETURN(false);
}

void nsCocoaWindow::SetModal(bool aModal) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (mModal == aModal) {
    return;
  }

  // Unlike many functions here, we explicitly *do not check* for the
  // existence of mWindow. This is to ensure that calls to SetModal have
  // no early exits and always update state. That way, if the calls are
  // balanced, we get expected behavior even if the native window has
  // been destroyed during the modal period. Within this function, all
  // the calls to mWindow will resolve even if mWindow is nil (as is
  // guaranteed by Objective-C). And since those calls are only concerned
  // with changing mWindow appearance/level, it's fine for them to be
  // no-ops if mWindow has already been destroyed.

  // This is used during startup (outside the event loop) when creating
  // the add-ons compatibility checking dialog and the profile manager UI;
  // therefore, it needs to provide an autorelease pool to avoid cocoa
  // objects leaking.
  nsAutoreleasePool localPool;

  mModal = aModal;

  if (aModal) {
    sModalWindowCount++;
  } else {
    MOZ_ASSERT(sModalWindowCount);
    sModalWindowCount--;
  }

  // When a window gets "set modal", make the window(s) that it appears over
  // behave as they should.  We can't rely on native methods to do this, for the
  // following reason:  The OS runs modal non-sheet windows in an event loop
  // (using [NSApplication runModalForWindow:] or similar methods) that's
  // incompatible with the modal event loop in AppWindow::ShowModal() (each of
  // these event loops is "exclusive", and can't run at the same time as other
  // (similar) event loops).
  for (auto* ancestor = static_cast<nsCocoaWindow*>(mAncestorLink); ancestor;
       ancestor = static_cast<nsCocoaWindow*>(ancestor->mParent)) {
    const bool changed = aModal ? ancestor->mNumModalDescendants++ == 0
                                : --ancestor->mNumModalDescendants == 0;
    NS_ASSERTION(ancestor->mNumModalDescendants >= 0,
                 "Widget hierarchy changed while modal!");
    if (!changed || ancestor->mWindowType == WindowType::Invisible) {
      continue;
    }
    NSWindow* win = ancestor->GetCocoaWindow();
    [[win standardWindowButton:NSWindowCloseButton] setEnabled:!aModal];
    [[win standardWindowButton:NSWindowMiniaturizeButton] setEnabled:!aModal];
    [[win standardWindowButton:NSWindowZoomButton] setEnabled:!aModal];
  }
  if (aModal) {
    mWindow.level = NSModalPanelWindowLevel;
  } else if (mWindowType == WindowType::Popup) {
    SetPopupWindowLevel();
  } else {
    mWindow.level = NSNormalWindowLevel;
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

bool nsCocoaWindow::IsRunningAppModal() { return [NSApp _isRunningAppModal]; }

// Hide or show this window
void nsCocoaWindow::Show(bool aState) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!mWindow) {
    return;
  }

  // Early exit if our current visibility state is already the requested
  // state.
  if (aState == mWindow.isVisibleOrBeingShown) {
    return;
  }

  [mWindow setBeingShown:aState];
  if (aState && !mWasShown) {
    mWasShown = true;
  }

  NSWindow* nativeParentWindow =
      mParent ? (NSWindow*)mParent->GetNativeData(NS_NATIVE_WINDOW) : nil;

  if (aState && !mBounds.IsEmpty()) {
    // If we had set the activationPolicy to accessory, then right now we won't
    // have a dock icon. Make sure that we undo that and show a dock icon now
    // that we're going to show a window.
    if (NSApp.activationPolicy != NSApplicationActivationPolicyRegular) {
      NSApp.activationPolicy = NSApplicationActivationPolicyRegular;
      PR_SetEnv("MOZ_APP_NO_DOCK=");
    }

    // Don't try to show a popup when the parent isn't visible or is minimized.
    if (mWindowType == WindowType::Popup && nativeParentWindow) {
      if (!nativeParentWindow.isVisible || nativeParentWindow.isMiniaturized) {
        return;
      }
    }

    if (mPopupContentView) {
      // Ensure our content view is visible. We never need to hide it.
      mPopupContentView->Show(true);
    }

    // We're about to show a window. If we are opening the new window while the
    // user is in a fullscreen space, for example because the new window is
    // opened from an existing fullscreen window, then macOS will open the new
    // window in fullscreen, too. For some windows, this is not desirable. We
    // want to prevent it for any popup, alert, or alwaysOnTop windows that
    // aren't already in fullscreen. If the user already got the window into
    // fullscreen somehow, that's fine, but we don't want the initial display to
    // be in fullscreen.
    bool savedValueForSupportsNativeFullscreen = GetSupportsNativeFullscreen();
    if (!mInFullScreenMode &&
        ((mWindowType == WindowType::Popup) || mAlwaysOnTop || mIsAlert)) {
      SetSupportsNativeFullscreen(false);
    }

    if (mWindowType == WindowType::Popup) {
      // For reasons that aren't yet clear, calls to [NSWindow orderFront:] or
      // [NSWindow makeKeyAndOrderFront:] can sometimes trigger "Error (1000)
      // creating CGSWindow", which in turn triggers an internal inconsistency
      // NSException.  These errors shouldn't be fatal.  So we need to wrap
      // calls to ...orderFront: in TRY blocks.  See bmo bug 470864.
      NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;
      [[mWindow contentView] setNeedsDisplay:YES];
      if (!nativeParentWindow || mPopupLevel != PopupLevel::Parent) {
        [mWindow orderFront:nil];
      }
      NS_OBJC_END_TRY_IGNORE_BLOCK;
      SendSetZLevelEvent();
      // If our popup window is a non-native context menu, tell the OS (and
      // other programs) that a menu has opened.  This is how the OS knows to
      // close other programs' context menus when ours open.
      if ([mWindow isKindOfClass:[PopupWindow class]] &&
          [(PopupWindow*)mWindow isContextMenu]) {
        [NSDistributedNotificationCenter.defaultCenter
            postNotificationName:
                @"com.apple.HIToolbox.beginMenuTrackingNotification"
                          object:@"org.mozilla.gecko.PopupWindow"];
      }

      // If a parent window was supplied and this is a popup at the parent
      // level, set its child window. This will cause the child window to
      // appear above the parent and move when the parent does.
      if (nativeParentWindow && mPopupLevel == PopupLevel::Parent) {
        [nativeParentWindow addChildWindow:mWindow ordered:NSWindowAbove];
      }
    } else {
      NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;
      if (mWindowType == WindowType::TopLevel &&
          [mWindow respondsToSelector:@selector(setAnimationBehavior:)]) {
        NSWindowAnimationBehavior behavior;
        if (mIsAnimationSuppressed) {
          behavior = NSWindowAnimationBehaviorNone;
        } else {
          switch (mAnimationType) {
            case nsIWidget::eDocumentWindowAnimation:
              behavior = NSWindowAnimationBehaviorDocumentWindow;
              break;
            default:
              MOZ_FALLTHROUGH_ASSERT("unexpected mAnimationType value");
            case nsIWidget::eGenericWindowAnimation:
              behavior = NSWindowAnimationBehaviorDefault;
              break;
          }
        }
        [mWindow setAnimationBehavior:behavior];
        mWindowAnimationBehavior = behavior;
      }

      // We don't want alwaysontop / alert windows to pull focus when they're
      // opened, as these tend to be for peripheral indicators and displays.
      if (mAlwaysOnTop || mIsAlert) {
        [mWindow orderFront:nil];
      } else {
        [mWindow makeKeyAndOrderFront:nil];
      }
      NS_OBJC_END_TRY_IGNORE_BLOCK;
      SendSetZLevelEvent();
    }
    SetSupportsNativeFullscreen(savedValueForSupportsNativeFullscreen);
  } else {
    // roll up any popups if a top-level window is going away
    if (mWindowType == WindowType::TopLevel ||
        mWindowType == WindowType::Dialog) {
      RollUpPopups();
    }

    // If the window is a popup window with a parent window we need to
    // unhook it here before ordering it out. When you order out the child
    // of a window it hides the parent window.
    if (mWindowType == WindowType::Popup && nativeParentWindow) {
      [nativeParentWindow removeChildWindow:mWindow];
    }

    [mWindow orderOut:nil];
    // If our popup window is a non-native context menu, tell the OS (and
    // other programs) that a menu has closed.
    if ([mWindow isKindOfClass:[PopupWindow class]] &&
        [(PopupWindow*)mWindow isContextMenu]) {
      [NSDistributedNotificationCenter.defaultCenter
          postNotificationName:
              @"com.apple.HIToolbox.endMenuTrackingNotification"
                        object:@"org.mozilla.gecko.PopupWindow"];
    }
  }

  [mWindow setBeingShown:NO];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// Work around a problem where with multiple displays and multiple spaces
// enabled, where the browser is assigned to a single display or space, popup
// windows that are reshown after being hidden with [NSWindow orderOut] show on
// the assigned space even when opened from another display. Apply the
// workaround whenever more than one display is enabled.
bool nsCocoaWindow::NeedsRecreateToReshow() {
  // Limit the workaround to popup windows because only they need to override
  // the "Assign To" setting. i.e., to display where the parent window is.
  return mWindowType == WindowType::Popup && mWasShown &&
         NSScreen.screens.count > 1;
}

WindowRenderer* nsCocoaWindow::GetWindowRenderer() {
  if (mPopupContentView) {
    return mPopupContentView->GetWindowRenderer();
  }
  return nullptr;
}

TransparencyMode nsCocoaWindow::GetTransparencyMode() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  return !mWindow || mWindow.isOpaque ? TransparencyMode::Opaque
                                      : TransparencyMode::Transparent;

  NS_OBJC_END_TRY_BLOCK_RETURN(TransparencyMode::Opaque);
}

// This is called from nsMenuPopupFrame when making a popup transparent.
void nsCocoaWindow::SetTransparencyMode(TransparencyMode aMode) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!mWindow) {
    return;
  }

  BOOL isTransparent = aMode == TransparencyMode::Transparent;
  BOOL currentTransparency = !mWindow.isOpaque;
  if (isTransparent == currentTransparency) {
    return;
  }
  mWindow.opaque = !isTransparent;
  mWindow.backgroundColor =
      isTransparent ? NSColor.clearColor : NSColor.whiteColor;

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsCocoaWindow::Enable(bool aState) {}

bool nsCocoaWindow::IsEnabled() const { return true; }

void nsCocoaWindow::ConstrainPosition(DesktopIntPoint& aPoint) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!mWindow || ![mWindow screen]) {
    return;
  }

  nsIntRect screenBounds;

  int32_t width, height;

  NSRect frame = mWindow.frame;

  // zero size rects confuse the screen manager
  width = std::max<int32_t>(frame.size.width, 1);
  height = std::max<int32_t>(frame.size.height, 1);

  nsCOMPtr<nsIScreenManager> screenMgr =
      do_GetService("@mozilla.org/gfx/screenmanager;1");
  if (screenMgr) {
    nsCOMPtr<nsIScreen> screen;
    screenMgr->ScreenForRect(aPoint.x, aPoint.y, width, height,
                             getter_AddRefs(screen));

    if (screen) {
      screen->GetRectDisplayPix(&(screenBounds.x), &(screenBounds.y),
                                &(screenBounds.width), &(screenBounds.height));
    }
  }

  if (aPoint.x < screenBounds.x) {
    aPoint.x = screenBounds.x;
  } else if (aPoint.x >= screenBounds.x + screenBounds.width - width) {
    aPoint.x = screenBounds.x + screenBounds.width - width;
  }

  if (aPoint.y < screenBounds.y) {
    aPoint.y = screenBounds.y;
  } else if (aPoint.y >= screenBounds.y + screenBounds.height - height) {
    aPoint.y = screenBounds.y + screenBounds.height - height;
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsCocoaWindow::SetSizeConstraints(const SizeConstraints& aConstraints) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  // Popups can be smaller than (32, 32)
  NSRect rect = (mWindowType == WindowType::Popup)
                    ? NSZeroRect
                    : NSMakeRect(0.0, 0.0, 32, 32);
  rect = [mWindow frameRectForChildViewRect:rect];

  SizeConstraints c = aConstraints;

  if (c.mScale.scale == MOZ_WIDGET_INVALID_SCALE) {
    c.mScale.scale = BackingScaleFactor();
  }

  c.mMinSize.width = std::max(
      nsCocoaUtils::CocoaPointsToDevPixels(rect.size.width, c.mScale.scale),
      c.mMinSize.width);
  c.mMinSize.height = std::max(
      nsCocoaUtils::CocoaPointsToDevPixels(rect.size.height, c.mScale.scale),
      c.mMinSize.height);

  NSSize minSize = {
      nsCocoaUtils::DevPixelsToCocoaPoints(c.mMinSize.width, c.mScale.scale),
      nsCocoaUtils::DevPixelsToCocoaPoints(c.mMinSize.height, c.mScale.scale)};
  mWindow.minSize = minSize;

  c.mMaxSize.width = std::max(
      nsCocoaUtils::CocoaPointsToDevPixels(c.mMaxSize.width, c.mScale.scale),
      c.mMaxSize.width);
  c.mMaxSize.height = std::max(
      nsCocoaUtils::CocoaPointsToDevPixels(c.mMaxSize.height, c.mScale.scale),
      c.mMaxSize.height);

  NSSize maxSize = {
      c.mMaxSize.width == NS_MAXSIZE ? FLT_MAX
                                     : nsCocoaUtils::DevPixelsToCocoaPoints(
                                           c.mMaxSize.width, c.mScale.scale),
      c.mMaxSize.height == NS_MAXSIZE ? FLT_MAX
                                      : nsCocoaUtils::DevPixelsToCocoaPoints(
                                            c.mMaxSize.height, c.mScale.scale)};
  mWindow.maxSize = maxSize;
  nsBaseWidget::SetSizeConstraints(c);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// Coordinates are desktop pixels
void nsCocoaWindow::Move(double aX, double aY) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!mWindow) {
    return;
  }

  // The point we have is in Gecko coordinates (origin top-left). Convert
  // it to Cocoa ones (origin bottom-left).
  NSPoint coord = {
      static_cast<float>(aX),
      static_cast<float>(nsCocoaUtils::FlippedScreenY(NSToIntRound(aY)))};

  NSRect frame = mWindow.frame;
  if (frame.origin.x != coord.x ||
      frame.origin.y + frame.size.height != coord.y) {
    [mWindow setFrameTopLeftPoint:coord];
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsCocoaWindow::SetSizeMode(nsSizeMode aMode) {
  if (aMode == nsSizeMode_Normal) {
    QueueTransition(TransitionType::Windowed);
  } else if (aMode == nsSizeMode_Minimized) {
    QueueTransition(TransitionType::Miniaturize);
  } else if (aMode == nsSizeMode_Maximized) {
    QueueTransition(TransitionType::Zoom);
  } else if (aMode == nsSizeMode_Fullscreen) {
    MakeFullScreen(true);
  }
}

// The (work)space switching implementation below was inspired by Phoenix:
// https://github.com/kasper/phoenix/tree/d6c877f62b30a060dff119d8416b0934f76af534
// License: MIT.

// Runtime `CGSGetActiveSpace` library function feature detection.
typedef CGSSpaceID (*CGSGetActiveSpaceFunc)(CGSConnection cid);
static CGSGetActiveSpaceFunc GetCGSGetActiveSpaceFunc() {
  static CGSGetActiveSpaceFunc func = nullptr;
  static bool lookedUpFunc = false;
  if (!lookedUpFunc) {
    func = (CGSGetActiveSpaceFunc)dlsym(RTLD_DEFAULT, "CGSGetActiveSpace");
    lookedUpFunc = true;
  }
  return func;
}
// Runtime `CGSCopyManagedDisplaySpaces` library function feature detection.
typedef CFArrayRef (*CGSCopyManagedDisplaySpacesFunc)(CGSConnection cid);
static CGSCopyManagedDisplaySpacesFunc GetCGSCopyManagedDisplaySpacesFunc() {
  static CGSCopyManagedDisplaySpacesFunc func = nullptr;
  static bool lookedUpFunc = false;
  if (!lookedUpFunc) {
    func = (CGSCopyManagedDisplaySpacesFunc)dlsym(
        RTLD_DEFAULT, "CGSCopyManagedDisplaySpaces");
    lookedUpFunc = true;
  }
  return func;
}
// Runtime `CGSCopySpacesForWindows` library function feature detection.
typedef CFArrayRef (*CGSCopySpacesForWindowsFunc)(CGSConnection cid,
                                                  CGSSpaceMask mask,
                                                  CFArrayRef windowIDs);
static CGSCopySpacesForWindowsFunc GetCGSCopySpacesForWindowsFunc() {
  static CGSCopySpacesForWindowsFunc func = nullptr;
  static bool lookedUpFunc = false;
  if (!lookedUpFunc) {
    func = (CGSCopySpacesForWindowsFunc)dlsym(RTLD_DEFAULT,
                                              "CGSCopySpacesForWindows");
    lookedUpFunc = true;
  }
  return func;
}
// Runtime `CGSAddWindowsToSpaces` library function feature detection.
typedef void (*CGSAddWindowsToSpacesFunc)(CGSConnection cid,
                                          CFArrayRef windowIDs,
                                          CFArrayRef spaceIDs);
static CGSAddWindowsToSpacesFunc GetCGSAddWindowsToSpacesFunc() {
  static CGSAddWindowsToSpacesFunc func = nullptr;
  static bool lookedUpFunc = false;
  if (!lookedUpFunc) {
    func =
        (CGSAddWindowsToSpacesFunc)dlsym(RTLD_DEFAULT, "CGSAddWindowsToSpaces");
    lookedUpFunc = true;
  }
  return func;
}
// Runtime `CGSRemoveWindowsFromSpaces` library function feature detection.
typedef void (*CGSRemoveWindowsFromSpacesFunc)(CGSConnection cid,
                                               CFArrayRef windowIDs,
                                               CFArrayRef spaceIDs);
static CGSRemoveWindowsFromSpacesFunc GetCGSRemoveWindowsFromSpacesFunc() {
  static CGSRemoveWindowsFromSpacesFunc func = nullptr;
  static bool lookedUpFunc = false;
  if (!lookedUpFunc) {
    func = (CGSRemoveWindowsFromSpacesFunc)dlsym(RTLD_DEFAULT,
                                                 "CGSRemoveWindowsFromSpaces");
    lookedUpFunc = true;
  }
  return func;
}

void nsCocoaWindow::GetWorkspaceID(nsAString& workspaceID) {
  workspaceID.Truncate();
  int32_t sid = GetWorkspaceID();
  if (sid != 0) {
    workspaceID.AppendInt(sid);
  }
}

int32_t nsCocoaWindow::GetWorkspaceID() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  // Mac OSX space IDs start at '1' (default space), so '0' means 'unknown',
  // effectively.
  CGSSpaceID sid = 0;

  CGSCopySpacesForWindowsFunc CopySpacesForWindows =
      GetCGSCopySpacesForWindowsFunc();
  if (!CopySpacesForWindows) {
    return sid;
  }

  CGSConnection cid = _CGSDefaultConnection();
  // Fetch all spaces that this window belongs to (in order).
  NSArray<NSNumber*>* spaceIDs = CFBridgingRelease(CopySpacesForWindows(
      cid, kCGSAllSpacesMask,
      (__bridge CFArrayRef) @[ @([mWindow windowNumber]) ]));
  if ([spaceIDs count]) {
    // When spaces are found, return the first one.
    // We don't support a single window painted across multiple places for now.
    sid = [spaceIDs[0] integerValue];
  } else {
    // Fall back to the workspace that's currently active, which is '1' in the
    // common case.
    CGSGetActiveSpaceFunc GetActiveSpace = GetCGSGetActiveSpaceFunc();
    if (GetActiveSpace) {
      sid = GetActiveSpace(cid);
    }
  }

  return sid;

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsCocoaWindow::MoveToWorkspace(const nsAString& workspaceIDStr) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if ([NSScreen screensHaveSeparateSpaces] && [[NSScreen screens] count] > 1) {
    // We don't support moving to a workspace when the user has this option
    // enabled in Mission Control.
    return;
  }

  nsresult rv = NS_OK;
  int32_t workspaceID = workspaceIDStr.ToInteger(&rv);
  if (NS_FAILED(rv)) {
    return;
  }

  CGSConnection cid = _CGSDefaultConnection();
  int32_t currentSpace = GetWorkspaceID();
  // If an empty workspace ID is passed in (not valid on OSX), or when the
  // window is already on this workspace, we don't need to do anything.
  if (!workspaceID || workspaceID == currentSpace) {
    return;
  }

  CGSCopyManagedDisplaySpacesFunc CopyManagedDisplaySpaces =
      GetCGSCopyManagedDisplaySpacesFunc();
  CGSAddWindowsToSpacesFunc AddWindowsToSpaces = GetCGSAddWindowsToSpacesFunc();
  CGSRemoveWindowsFromSpacesFunc RemoveWindowsFromSpaces =
      GetCGSRemoveWindowsFromSpacesFunc();
  if (!CopyManagedDisplaySpaces || !AddWindowsToSpaces ||
      !RemoveWindowsFromSpaces) {
    return;
  }

  // Fetch an ordered list of all known spaces.
  NSArray* displaySpacesInfo = CFBridgingRelease(CopyManagedDisplaySpaces(cid));
  // When we found the space we're looking for, we can bail out of the loop
  // early, which this local variable is used for.
  BOOL found = false;
  for (NSDictionary<NSString*, id>* spacesInfo in displaySpacesInfo) {
    NSArray<NSNumber*>* sids =
        [spacesInfo[CGSSpacesKey] valueForKey:CGSSpaceIDKey];
    for (NSNumber* sid in sids) {
      // If we found our space in the list, we're good to go and can jump out of
      // this loop.
      if ((int)[sid integerValue] == workspaceID) {
        found = true;
        break;
      }
    }
    if (found) {
      break;
    }
  }

  // We were unable to find the space to correspond with the workspaceID as
  // requested, so let's bail out.
  if (!found) {
    return;
  }

  // First we add the window to the appropriate space.
  AddWindowsToSpaces(cid, (__bridge CFArrayRef) @[ @([mWindow windowNumber]) ],
                     (__bridge CFArrayRef) @[ @(workspaceID) ]);
  // Then we remove the window from the active space.
  RemoveWindowsFromSpaces(cid,
                          (__bridge CFArrayRef) @[ @([mWindow windowNumber]) ],
                          (__bridge CFArrayRef) @[ @(currentSpace) ]);

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsCocoaWindow::SuppressAnimation(bool aSuppress) {
  if ([mWindow respondsToSelector:@selector(setAnimationBehavior:)]) {
    mWindow.isAnimationSuppressed = aSuppress;
    mWindow.animationBehavior =
        aSuppress ? NSWindowAnimationBehaviorNone : mWindowAnimationBehavior;
  }
}

// This has to preserve the window's frame bounds.
// This method requires (as does the Windows impl.) that you call Resize shortly
// after calling HideWindowChrome. See bug 498835 for fixing this.
void nsCocoaWindow::HideWindowChrome(bool aShouldHide) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!mWindow || !mWindowMadeHere ||
      (mWindowType != WindowType::TopLevel &&
       mWindowType != WindowType::Dialog)) {
    return;
  }

  const BOOL isVisible = mWindow.isVisible;

  // Remove child windows.
  NSArray* childWindows = [mWindow childWindows];
  NSEnumerator* enumerator = [childWindows objectEnumerator];
  NSWindow* child = nil;
  while ((child = [enumerator nextObject])) {
    [mWindow removeChildWindow:child];
  }

  // Remove the views in the old window's content view.
  // The NSArray is autoreleased and retains its NSViews.
  NSArray<NSView*>* contentViewContents = [mWindow contentViewContents];
  for (NSView* view in contentViewContents) {
    [view removeFromSuperviewWithoutNeedingDisplay];
  }

  // Save state (like window title).
  NSMutableDictionary* state = [mWindow exportState];

  // Recreate the window with the right border style.
  NSRect frameRect = mWindow.frame;
  DestroyNativeWindow();
  nsresult rv = CreateNativeWindow(
      frameRect, aShouldHide ? BorderStyle::None : mBorderStyle, true,
      mWindow.restorable);
  NS_ENSURE_SUCCESS_VOID(rv);

  // Re-import state.
  [mWindow importState:state];

  // Add the old content view subviews to the new window's content view.
  for (NSView* view in contentViewContents) {
    [[mWindow contentView] addSubview:view];
  }

  // Reparent child windows.
  enumerator = [childWindows objectEnumerator];
  while ((child = [enumerator nextObject])) {
    [mWindow addChildWindow:child ordered:NSWindowAbove];
  }

  // Show the new window.
  if (isVisible) {
    bool wasAnimationSuppressed = mIsAnimationSuppressed;
    mIsAnimationSuppressed = true;
    Show(true);
    mIsAnimationSuppressed = wasAnimationSuppressed;
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

class FullscreenTransitionData : public nsISupports {
 public:
  NS_DECL_ISUPPORTS

  explicit FullscreenTransitionData(NSWindow* aWindow)
      : mTransitionWindow(aWindow) {}

  NSWindow* mTransitionWindow;

 private:
  virtual ~FullscreenTransitionData() { [mTransitionWindow close]; }
};

NS_IMPL_ISUPPORTS0(FullscreenTransitionData)

@interface FullscreenTransitionDelegate : NSObject <NSAnimationDelegate> {
 @public
  nsCocoaWindow* mWindow;
  nsIRunnable* mCallback;
}
@end

@implementation FullscreenTransitionDelegate
- (void)cleanupAndDispatch:(NSAnimation*)animation {
  [animation setDelegate:nil];
  [self autorelease];
  // The caller should have added ref for us.
  NS_DispatchToMainThread(already_AddRefed<nsIRunnable>(mCallback));
}

- (void)animationDidEnd:(NSAnimation*)animation {
  MOZ_ASSERT(animation == mWindow->FullscreenTransitionAnimation(),
             "Should be handling the only animation on the window");
  mWindow->ReleaseFullscreenTransitionAnimation();
  [self cleanupAndDispatch:animation];
}

- (void)animationDidStop:(NSAnimation*)animation {
  [self cleanupAndDispatch:animation];
}
@end

static bool AlwaysUsesNativeFullScreen() {
  return Preferences::GetBool("full-screen-api.macos-native-full-screen",
                              false);
}

/* virtual */ bool nsCocoaWindow::PrepareForFullscreenTransition(
    nsISupports** aData) {
  if (AlwaysUsesNativeFullScreen()) {
    return false;
  }

  // Our fullscreen transition creates a new window occluding this window.
  // That triggers an occlusion event which can cause DOM fullscreen requests
  // to fail due to the context not being focused at the time the focus check
  // is performed in the child process. Until the transition is cleaned up in
  // CleanupFullscreenTransition(), ignore occlusion events for this window.
  // If this method is changed to return false, the transition will not be
  // performed and mIgnoreOcclusionCount should not be incremented.
  MOZ_ASSERT(mIgnoreOcclusionCount >= 0);
  mIgnoreOcclusionCount++;

  nsCOMPtr<nsIScreen> widgetScreen = GetWidgetScreen();
  NSScreen* cocoaScreen = ScreenHelperCocoa::CocoaScreenForScreen(widgetScreen);

  NSWindow* win =
      [[NSWindow alloc] initWithContentRect:cocoaScreen.frame
                                  styleMask:NSWindowStyleMaskBorderless
                                    backing:NSBackingStoreBuffered
                                      defer:YES];
  [win setBackgroundColor:[NSColor blackColor]];
  [win setAlphaValue:0];
  [win setIgnoresMouseEvents:YES];
  [win setLevel:NSScreenSaverWindowLevel];
  [win makeKeyAndOrderFront:nil];

  auto data = new FullscreenTransitionData(win);
  *aData = data;
  NS_ADDREF(data);
  return true;
}

/* virtual */ void nsCocoaWindow::CleanupFullscreenTransition() {
  MOZ_ASSERT(mIgnoreOcclusionCount > 0);
  mIgnoreOcclusionCount--;
}

/* virtual */ void nsCocoaWindow::PerformFullscreenTransition(
    FullscreenTransitionStage aStage, uint16_t aDuration, nsISupports* aData,
    nsIRunnable* aCallback) {
  auto data = static_cast<FullscreenTransitionData*>(aData);
  FullscreenTransitionDelegate* delegate =
      [[FullscreenTransitionDelegate alloc] init];
  delegate->mWindow = this;
  // Storing already_AddRefed directly could cause static checking fail.
  delegate->mCallback = nsCOMPtr<nsIRunnable>(aCallback).forget().take();

  if (mFullscreenTransitionAnimation) {
    [mFullscreenTransitionAnimation stopAnimation];
    ReleaseFullscreenTransitionAnimation();
  }

  NSDictionary* dict = @{
    NSViewAnimationTargetKey : data->mTransitionWindow,
    NSViewAnimationEffectKey : aStage == eBeforeFullscreenToggle
        ? NSViewAnimationFadeInEffect
        : NSViewAnimationFadeOutEffect
  };
  mFullscreenTransitionAnimation =
      [[NSViewAnimation alloc] initWithViewAnimations:@[ dict ]];
  [mFullscreenTransitionAnimation setDelegate:delegate];
  [mFullscreenTransitionAnimation setDuration:aDuration / 1000.0];
  [mFullscreenTransitionAnimation startAnimation];
}

void nsCocoaWindow::CocoaWindowWillEnterFullscreen(bool aFullscreen) {
  MOZ_ASSERT(mUpdateFullscreenOnResize.isNothing());

  mHasStartedNativeFullscreen = true;

  // Ensure that we update our fullscreen state as early as possible, when the
  // resize happens.
  mUpdateFullscreenOnResize =
      Some(aFullscreen ? TransitionType::Fullscreen : TransitionType::Windowed);
}

void nsCocoaWindow::CocoaWindowDidEnterFullscreen(bool aFullscreen) {
  EndOurNativeTransition();
  mHasStartedNativeFullscreen = false;
  DispatchOcclusionEvent();

  // Check if aFullscreen matches our expected fullscreen state. It might not if
  // there was a failure somewhere along the way, in which case we'll recover
  // from that.
  bool receivedExpectedFullscreen = false;
  if (mUpdateFullscreenOnResize.isSome()) {
    bool expectingFullscreen =
        (*mUpdateFullscreenOnResize == TransitionType::Fullscreen);
    receivedExpectedFullscreen = (expectingFullscreen == aFullscreen);
  } else {
    receivedExpectedFullscreen = (mInFullScreenMode == aFullscreen);
  }

  TransitionType transition =
      aFullscreen ? TransitionType::Fullscreen : TransitionType::Windowed;
  if (receivedExpectedFullscreen) {
    // Everything is as expected. Update our state if needed.
    HandleUpdateFullscreenOnResize();
  } else {
    // We weren't expecting this fullscreen state. Update our fullscreen state
    // to the new reality.
    UpdateFullscreenState(aFullscreen, true);

    // If we have a current transition, switch it to match what we just did.
    if (mTransitionCurrent.isSome()) {
      mTransitionCurrent = Some(transition);
    }
  }

  // Whether we expected this transition or not, we're ready to finish it.
  FinishCurrentTransitionIfMatching(transition);
}

void nsCocoaWindow::UpdateFullscreenState(bool aFullScreen, bool aNativeMode) {
  bool wasInFullscreen = mInFullScreenMode;
  mInFullScreenMode = aFullScreen;
  if (aNativeMode || mInNativeFullScreenMode) {
    mInNativeFullScreenMode = aFullScreen;
  }

  if (aFullScreen == wasInFullscreen) {
    return;
  }

  DispatchSizeModeEvent();

  // Notify the mainChildView with our new fullscreen state.
  nsChildView* mainChildView =
      static_cast<nsChildView*>([[mWindow mainChildView] widget]);
  if (mainChildView) {
    mainChildView->UpdateFullscreen(aFullScreen);
  }
}

nsresult nsCocoaWindow::MakeFullScreen(bool aFullScreen) {
  return DoMakeFullScreen(aFullScreen, AlwaysUsesNativeFullScreen());
}

nsresult nsCocoaWindow::MakeFullScreenWithNativeTransition(bool aFullScreen) {
  return DoMakeFullScreen(aFullScreen, true);
}

nsresult nsCocoaWindow::DoMakeFullScreen(bool aFullScreen,
                                         bool aUseSystemTransition) {
  if (!mWindow) {
    return NS_OK;
  }

  // Figure out what type of transition is being requested.
  TransitionType transition = TransitionType::Windowed;
  if (aFullScreen) {
    // Decide whether to use fullscreen or emulated fullscreen.
    transition =
        (aUseSystemTransition && (mWindow.collectionBehavior &
                                  NSWindowCollectionBehaviorFullScreenPrimary))
            ? TransitionType::Fullscreen
            : TransitionType::EmulatedFullscreen;
  }

  QueueTransition(transition);
  return NS_OK;
}

void nsCocoaWindow::QueueTransition(const TransitionType& aTransition) {
  mTransitionsPending.push(aTransition);
  ProcessTransitions();
}

void nsCocoaWindow::ProcessTransitions() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK

  if (mInProcessTransitions) {
    return;
  }

  mInProcessTransitions = true;

  // Start a loop that will continue as long as we have transitions to process
  // and we aren't waiting on an asynchronous transition to complete. Any
  // transition that starts something async will `continue` this loop to exit.
  while (!mTransitionsPending.empty() && !IsInTransition()) {
    TransitionType nextTransition = mTransitionsPending.front();

    // We have to check for some incompatible transition states, and if we find
    // one, instead perform an alternative transition and leave the queue
    // untouched. If we add one of these transitions, we set
    // mIsTransitionCurrentAdded because we don't want to confuse listeners who
    // are expecting to receive exactly one event when the requested transition
    // has completed.
    switch (nextTransition) {
      case TransitionType::Fullscreen:
      case TransitionType::EmulatedFullscreen:
      case TransitionType::Windowed:
      case TransitionType::Zoom:
        // These can't handle miniaturized windows, so deminiaturize first.
        if (mWindow.miniaturized) {
          mTransitionCurrent = Some(TransitionType::Deminiaturize);
          mIsTransitionCurrentAdded = true;
        }
        break;
      case TransitionType::Miniaturize:
        // This can't handle fullscreen, so go to windowed first.
        if (mInFullScreenMode) {
          mTransitionCurrent = Some(TransitionType::Windowed);
          mIsTransitionCurrentAdded = true;
        }
        break;
      default:
        break;
    }

    // If mTransitionCurrent is still empty, then we use the nextTransition and
    // pop the queue.
    if (mTransitionCurrent.isNothing()) {
      mTransitionCurrent = Some(nextTransition);
      mTransitionsPending.pop();
    }

    switch (*mTransitionCurrent) {
      case TransitionType::Fullscreen: {
        if (!mInFullScreenMode) {
          // Run a local run loop until it is safe to start a native fullscreen
          // transition.
          NSRunLoop* localRunLoop = [NSRunLoop currentRunLoop];
          while (mWindow && !CanStartNativeTransition() &&
                 [localRunLoop runMode:NSDefaultRunLoopMode
                            beforeDate:[NSDate distantFuture]]) {
            // This loop continues to process events until
            // CanStartNativeTransition() returns true or our native
            // window has been destroyed.
          }

          // This triggers an async animation, so continue.
          [mWindow toggleFullScreen:nil];
          continue;
        }
        break;
      }

      case TransitionType::EmulatedFullscreen: {
        if (!mInFullScreenMode) {
          NSDisableScreenUpdates();
          mSuppressSizeModeEvents = true;
          // The order here matters. When we exit full screen mode, we need to
          // show the Dock first, otherwise the newly-created window won't have
          // its minimize button enabled. See bug 526282.
          nsCocoaUtils::HideOSChromeOnScreen(true);
          nsBaseWidget::InfallibleMakeFullScreen(true);
          mSuppressSizeModeEvents = false;
          NSEnableScreenUpdates();
          UpdateFullscreenState(true, false);
        }
        break;
      }

      case TransitionType::Windowed: {
        if (mInFullScreenMode) {
          if (mInNativeFullScreenMode) {
            // Run a local run loop until it is safe to start a native
            // fullscreen transition.
            NSRunLoop* localRunLoop = [NSRunLoop currentRunLoop];
            while (mWindow && !CanStartNativeTransition() &&
                   [localRunLoop runMode:NSDefaultRunLoopMode
                              beforeDate:[NSDate distantFuture]]) {
              // This loop continues to process events until
              // CanStartNativeTransition() returns true or our native
              // window has been destroyed.
            }

            // This triggers an async animation, so continue.
            [mWindow toggleFullScreen:nil];
            continue;
          } else {
            NSDisableScreenUpdates();
            mSuppressSizeModeEvents = true;
            // The order here matters. When we exit full screen mode, we need to
            // show the Dock first, otherwise the newly-created window won't
            // have its minimize button enabled. See bug 526282.
            nsCocoaUtils::HideOSChromeOnScreen(false);
            nsBaseWidget::InfallibleMakeFullScreen(false);
            mSuppressSizeModeEvents = false;
            NSEnableScreenUpdates();
            UpdateFullscreenState(false, false);
          }
        } else if (mWindow.zoomed) {
          [mWindow zoom:nil];

          // Check if we're still zoomed. If we are, we need to do *something*
          // to make the window smaller than the zoom size so Cocoa will treat
          // us as being out of the zoomed state. Otherwise, we could stay
          // zoomed and never be able to be "normal" from calls to SetSizeMode.
          if (mWindow.zoomed) {
            NSRect maximumFrame = mWindow.frame;
            const CGFloat INSET_OUT_OF_ZOOM = 20.0f;
            [mWindow setFrame:NSInsetRect(maximumFrame, INSET_OUT_OF_ZOOM,
                                          INSET_OUT_OF_ZOOM)
                      display:YES];
            MOZ_ASSERT(
                !mWindow.zoomed,
                "We should be able to unzoom by shrinking the frame a bit.");
          }
        }
        break;
      }

      case TransitionType::Miniaturize:
        if (!mWindow.miniaturized) {
          // This triggers an async animation, so continue.
          [mWindow miniaturize:nil];
          continue;
        }
        break;

      case TransitionType::Deminiaturize:
        if (mWindow.miniaturized) {
          // This triggers an async animation, so continue.
          [mWindow deminiaturize:nil];
          continue;
        }
        break;

      case TransitionType::Zoom:
        if (!mWindow.zoomed) {
          [mWindow zoom:nil];
        }
        break;

      default:
        break;
    }

    mTransitionCurrent.reset();
    mIsTransitionCurrentAdded = false;
  }

  mInProcessTransitions = false;

  // When we finish processing transitions, dispatch a size mode event to cover
  // the cases where an inserted transition suppressed one, and the original
  // transition never sent one because it detected it was at the desired state
  // when it ran. If we've already sent a size mode event, then this will be a
  // no-op.
  if (!IsInTransition()) {
    DispatchSizeModeEvent();
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsCocoaWindow::CancelAllTransitions() {
  // Clear our current and pending transitions. This simplifies our
  // reasoning about what happens next, and ensures that whatever is
  // currently happening won't trigger another call to
  // ProcessTransitions().
  mTransitionCurrent.reset();
  mIsTransitionCurrentAdded = false;
  std::queue<TransitionType>().swap(mTransitionsPending);
}

void nsCocoaWindow::FinishCurrentTransitionIfMatching(
    const TransitionType& aTransition) {
  // We've just finished some transition activity, and we're not sure whether it
  // was triggered programmatically, or by the user. If it matches our current
  // transition, then assume it was triggered programmatically and we can clean
  // up that transition and start processing transitions again.

  // Whether programmatic or user-initiated, we send out a size mode event.
  DispatchSizeModeEvent();

  if (mTransitionCurrent.isSome() && (*mTransitionCurrent == aTransition)) {
    // This matches our current transition, so do the safe parts of transition
    // cleanup.
    mTransitionCurrent.reset();
    mIsTransitionCurrentAdded = false;

    // Since this function is called from nsWindowDelegate transition callbacks,
    // we want to make sure those callbacks are all the way done before we
    // continue processing more transitions. To accomplish this, we dispatch
    // ProcessTransitions on the next event loop. Doing this will ensure that
    // any async native transition methods we call (like toggleFullScreen) will
    // succeed.
    if (!mTransitionsPending.empty()) {
      NS_DispatchToCurrentThread(NewRunnableMethod(
          "FinishCurrentTransition", this, &nsCocoaWindow::ProcessTransitions));
    }
  }
}

bool nsCocoaWindow::HandleUpdateFullscreenOnResize() {
  if (mUpdateFullscreenOnResize.isNothing()) {
    return false;
  }

  bool toFullscreen =
      (*mUpdateFullscreenOnResize == TransitionType::Fullscreen);
  mUpdateFullscreenOnResize.reset();
  UpdateFullscreenState(toFullscreen, true);

  return true;
}

/* static */ nsCocoaWindow* nsCocoaWindow::sWindowInNativeTransition(nullptr);

bool nsCocoaWindow::CanStartNativeTransition() {
  if (sWindowInNativeTransition == nullptr) {
    // Claim it and return true, indicating that the caller has permission to
    // start the native fullscreen transition.
    sWindowInNativeTransition = this;
    return true;
  }
  return false;
}

void nsCocoaWindow::EndOurNativeTransition() {
  if (sWindowInNativeTransition == this) {
    sWindowInNativeTransition = nullptr;
  }
}

// Coordinates are desktop pixels
void nsCocoaWindow::DoResize(double aX, double aY, double aWidth,
                             double aHeight, bool aRepaint,
                             bool aConstrainToCurrentScreen) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!mWindow || mInResize) {
    return;
  }

  // We are able to resize a window outside of any aspect ratio contraints
  // applied to it, but in order to "update" the aspect ratio contraint to the
  // new window dimensions, we must re-lock the aspect ratio.
  auto relockAspectRatio = MakeScopeExit([&]() {
    if (mAspectRatioLocked) {
      LockAspectRatio(true);
    }
  });

  AutoRestore<bool> reentrantResizeGuard(mInResize);
  mInResize = true;

  CGFloat scale = mSizeConstraints.mScale.scale;
  if (scale == MOZ_WIDGET_INVALID_SCALE) {
    scale = BackingScaleFactor();
  }

  // mSizeConstraints is in device pixels.
  int32_t width = NSToIntRound(aWidth * scale);
  int32_t height = NSToIntRound(aHeight * scale);

  width = std::max(mSizeConstraints.mMinSize.width,
                   std::min(mSizeConstraints.mMaxSize.width, width));
  height = std::max(mSizeConstraints.mMinSize.height,
                    std::min(mSizeConstraints.mMaxSize.height, height));

  DesktopIntRect newBounds(NSToIntRound(aX), NSToIntRound(aY),
                           NSToIntRound(width / scale),
                           NSToIntRound(height / scale));

  // convert requested bounds into Cocoa coordinate system
  NSRect newFrame = nsCocoaUtils::GeckoRectToCocoaRect(newBounds);

  NSRect frame = mWindow.frame;
  BOOL isMoving = newFrame.origin.x != frame.origin.x ||
                  newFrame.origin.y != frame.origin.y;
  BOOL isResizing = newFrame.size.width != frame.size.width ||
                    newFrame.size.height != frame.size.height;

  if (!isMoving && !isResizing) {
    return;
  }

  // We ignore aRepaint -- we have to call display:YES, otherwise the
  // title bar doesn't immediately get repainted and is displayed in
  // the wrong place, leading to a visual jump.
  [mWindow setFrame:newFrame display:YES];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// Coordinates are desktop pixels
void nsCocoaWindow::Resize(double aX, double aY, double aWidth, double aHeight,
                           bool aRepaint) {
  DoResize(aX, aY, aWidth, aHeight, aRepaint, false);
}

// Coordinates are desktop pixels
void nsCocoaWindow::Resize(double aWidth, double aHeight, bool aRepaint) {
  double invScale = 1.0 / BackingScaleFactor();
  DoResize(mBounds.x * invScale, mBounds.y * invScale, aWidth, aHeight,
           aRepaint, true);
}

// Return the area that the Gecko ChildView in our window should cover, as an
// NSRect in screen coordinates (with 0,0 being the bottom left corner of the
// primary screen).
NSRect nsCocoaWindow::GetClientCocoaRect() {
  if (!mWindow) {
    return NSZeroRect;
  }

  return [mWindow childViewRectForFrameRect:mWindow.frame];
}

LayoutDeviceIntRect nsCocoaWindow::GetClientBounds() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  CGFloat scaleFactor = BackingScaleFactor();
  return nsCocoaUtils::CocoaRectToGeckoRectDevPix(GetClientCocoaRect(),
                                                  scaleFactor);

  NS_OBJC_END_TRY_BLOCK_RETURN(LayoutDeviceIntRect(0, 0, 0, 0));
}

void nsCocoaWindow::UpdateBounds() {
  NSRect frame = NSZeroRect;
  if (mWindow) {
    frame = mWindow.frame;
  }
  mBounds =
      nsCocoaUtils::CocoaRectToGeckoRectDevPix(frame, BackingScaleFactor());

  if (mPopupContentView) {
    mPopupContentView->UpdateBoundsFromView();
  }
}

LayoutDeviceIntRect nsCocoaWindow::GetScreenBounds() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

#ifdef DEBUG
  LayoutDeviceIntRect r = nsCocoaUtils::CocoaRectToGeckoRectDevPix(
      mWindow.frame, BackingScaleFactor());
  NS_ASSERTION(mWindow && mBounds == r, "mBounds out of sync!");
#endif

  return mBounds;

  NS_OBJC_END_TRY_BLOCK_RETURN(LayoutDeviceIntRect(0, 0, 0, 0));
}

double nsCocoaWindow::GetDefaultScaleInternal() { return BackingScaleFactor(); }

static CGFloat GetBackingScaleFactor(NSWindow* aWindow) {
  NSRect frame = aWindow.frame;
  if (frame.size.width > 0 && frame.size.height > 0) {
    return nsCocoaUtils::GetBackingScaleFactor(aWindow);
  }

  // For windows with zero width or height, the backingScaleFactor method
  // is broken - it will always return 2 on a retina macbook, even when
  // the window position implies it's on a non-hidpi external display
  // (to the extent that a zero-area window can be said to be "on" a
  // display at all!)
  // And to make matters worse, Cocoa even fires a
  // windowDidChangeBackingProperties notification with the
  // NSBackingPropertyOldScaleFactorKey key when a window on an
  // external display is resized to/from zero height, even though it hasn't
  // really changed screens.

  // This causes us to handle popup window sizing incorrectly when the
  // popup is resized to zero height (bug 820327) - nsXULPopupManager
  // becomes (incorrectly) convinced the popup has been explicitly forced
  // to a non-default size and needs to have size attributes attached.

  // Workaround: instead of asking the window, we'll find the screen it is on
  // and ask that for *its* backing scale factor.

  // (See bug 853252 and additional comments in windowDidChangeScreen: below
  // for further complications this causes.)

  // First, expand the rect so that it actually has a measurable area,
  // for FindTargetScreenForRect to use.
  if (frame.size.width == 0) {
    frame.size.width = 1;
  }
  if (frame.size.height == 0) {
    frame.size.height = 1;
  }

  // Then identify the screen it belongs to, and return its scale factor.
  NSScreen* screen =
      FindTargetScreenForRect(nsCocoaUtils::CocoaRectToGeckoRect(frame));
  return nsCocoaUtils::GetBackingScaleFactor(screen);
}

CGFloat nsCocoaWindow::BackingScaleFactor() {
  if (mBackingScaleFactor > 0.0) {
    return mBackingScaleFactor;
  }
  if (!mWindow) {
    return 1.0;
  }
  mBackingScaleFactor = GetBackingScaleFactor(mWindow);
  return mBackingScaleFactor;
}

void nsCocoaWindow::BackingScaleFactorChanged() {
  CGFloat newScale = GetBackingScaleFactor(mWindow);

  // ignore notification if it hasn't really changed (or maybe we have
  // disabled HiDPI mode via prefs)
  if (mBackingScaleFactor == newScale) {
    return;
  }

  mBackingScaleFactor = newScale;
  NotifyAPZOfDPIChange();

  if (!mWidgetListener || mWidgetListener->GetAppWindow()) {
    return;
  }

  if (PresShell* presShell = mWidgetListener->GetPresShell()) {
    presShell->BackingScaleFactorChanged();
  }
}

int32_t nsCocoaWindow::RoundsWidgetCoordinatesTo() {
  if (BackingScaleFactor() == 2.0) {
    return 2;
  }
  return 1;
}

void nsCocoaWindow::SetCursor(const Cursor& aCursor) {
  if (mPopupContentView) {
    mPopupContentView->SetCursor(aCursor);
  }
}

nsresult nsCocoaWindow::SetTitle(const nsAString& aTitle) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (!mWindow) {
    return NS_OK;
  }

  const nsString& strTitle = PromiseFlatString(aTitle);
  const unichar* uniTitle = reinterpret_cast<const unichar*>(strTitle.get());
  NSString* title = [NSString stringWithCharacters:uniTitle
                                            length:strTitle.Length()];
  if (mWindow.drawsContentsIntoWindowFrame && !mWindow.wantsTitleDrawn) {
    // Don't cause invalidations when the title isn't displayed.
    [mWindow disableSetNeedsDisplay];
    [mWindow setTitle:title];
    [mWindow enableSetNeedsDisplay];
  } else {
    [mWindow setTitle:title];
  }

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

void nsCocoaWindow::Invalidate(const LayoutDeviceIntRect& aRect) {
  if (mPopupContentView) {
    mPopupContentView->Invalidate(aRect);
  }
}

// Pass notification of some drag event to Gecko
//
// The drag manager has let us know that something related to a drag has
// occurred in this window. It could be any number of things, ranging from
// a drop, to a drag enter/leave, or a drag over event. The actual event
// is passed in |aMessage| and is passed along to our event hanlder so Gecko
// knows about it.
bool nsCocoaWindow::DragEvent(unsigned int aMessage,
                              mozilla::gfx::Point aMouseGlobal,
                              UInt16 aKeyModifiers) {
  return false;
}

void nsCocoaWindow::SendSetZLevelEvent() {
  if (mWidgetListener) {
    nsWindowZ placement = nsWindowZTop;
    nsCOMPtr<nsIWidget> actualBelow;
    mWidgetListener->ZLevelChanged(true, &placement, nullptr,
                                   getter_AddRefs(actualBelow));
  }
}

// Invokes callback and ProcessEvent methods on Event Listener object
nsresult nsCocoaWindow::DispatchEvent(WidgetGUIEvent* event,
                                      nsEventStatus& aStatus) {
  aStatus = nsEventStatus_eIgnore;

  nsCOMPtr<nsIWidget> kungFuDeathGrip(event->mWidget);
  mozilla::Unused << kungFuDeathGrip;  // Not used within this function

  if (mWidgetListener) {
    aStatus = mWidgetListener->HandleEvent(event, mUseAttachedEvents);
  }

  return NS_OK;
}

// aFullScreen should be the window's mInFullScreenMode. We don't have access to
// that from here, so we need to pass it in. mInFullScreenMode should be the
// canonical indicator that a window is currently full screen and it makes sense
// to keep all sizemode logic here.
static nsSizeMode GetWindowSizeMode(NSWindow* aWindow, bool aFullScreen) {
  if (aFullScreen) {
    return nsSizeMode_Fullscreen;
  }
  if (aWindow.isMiniaturized) {
    return nsSizeMode_Minimized;
  }
  if ((aWindow.styleMask & NSWindowStyleMaskResizable) && aWindow.isZoomed) {
    return nsSizeMode_Maximized;
  }
  return nsSizeMode_Normal;
}

void nsCocoaWindow::ReportMoveEvent() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  // Prevent recursion, which can become infinite (see bug 708278).  This
  // can happen when the call to [NSWindow setFrameTopLeftPoint:] in
  // nsCocoaWindow::Move() triggers an immediate NSWindowDidMove notification
  // (and a call to [WindowDelegate windowDidMove:]).
  if (mInReportMoveEvent) {
    return;
  }
  mInReportMoveEvent = true;

  UpdateBounds();

  // The zoomed state can change when we're moving, in which case we need to
  // update our internal mSizeMode. This can happen either if we're maximized
  // and then moved, or if we're not maximized and moved back to zoomed state.
  if (mWindow && (mSizeMode == nsSizeMode_Maximized) ^ mWindow.isZoomed) {
    DispatchSizeModeEvent();
  }

  // Dispatch the move event to Gecko
  NotifyWindowMoved(mBounds.x, mBounds.y);

  mInReportMoveEvent = false;

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsCocoaWindow::DispatchSizeModeEvent() {
  if (!mWindow) {
    return;
  }

  if (mSuppressSizeModeEvents || mIsTransitionCurrentAdded) {
    return;
  }

  nsSizeMode newMode = GetWindowSizeMode(mWindow, mInFullScreenMode);
  if (mSizeMode == newMode) {
    return;
  }

  mSizeMode = newMode;
  if (mWidgetListener) {
    mWidgetListener->SizeModeChanged(newMode);
  }
}

void nsCocoaWindow::DispatchOcclusionEvent() {
  if (!mWindow) {
    return;
  }

  // Our new occlusion state is true if the window is not visible.
  bool newOcclusionState =
      !(mHasStartedNativeFullscreen ||
        ([mWindow occlusionState] & NSWindowOcclusionStateVisible));

  // Don't dispatch if the new occlustion state is the same as the current
  // state.
  if (mIsFullyOccluded == newOcclusionState) {
    return;
  }

  MOZ_ASSERT(mIgnoreOcclusionCount >= 0);
  if (newOcclusionState && mIgnoreOcclusionCount > 0) {
    return;
  }

  mIsFullyOccluded = newOcclusionState;
  if (mWidgetListener) {
    mWidgetListener->OcclusionStateChanged(mIsFullyOccluded);
  }
}

void nsCocoaWindow::ReportSizeEvent() {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  UpdateBounds();
  if (mWidgetListener) {
    LayoutDeviceIntRect innerBounds = GetClientBounds();
    mWidgetListener->WindowResized(this, innerBounds.width, innerBounds.height);
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsCocoaWindow::SetMenuBar(RefPtr<nsMenuBarX>&& aMenuBar) {
  if (!mWindow) {
    mMenuBar = nullptr;
    return;
  }
  mMenuBar = std::move(aMenuBar);

  // Only paint for active windows, or paint the hidden window menu bar if no
  // other menu bar has been painted yet so that some reasonable menu bar is
  // displayed when the app starts up.
  if (mMenuBar && ((!gSomeMenuBarPainted &&
                    nsMenuUtilsX::GetHiddenWindowMenuBar() == mMenuBar) ||
                   mWindow.isMainWindow)) {
    mMenuBar->Paint();
  }
}

void nsCocoaWindow::SetFocus(Raise aRaise,
                             mozilla::dom::CallerType aCallerType) {
  if (!mWindow) return;

  if (mPopupContentView) {
    return mPopupContentView->SetFocus(aRaise, aCallerType);
  }

  if (aRaise == Raise::Yes && (mWindow.isVisible || mWindow.isMiniaturized)) {
    if (mWindow.isMiniaturized) {
      [mWindow deminiaturize:nil];
    }
    [mWindow makeKeyAndOrderFront:nil];
    SendSetZLevelEvent();
  }
}

LayoutDeviceIntPoint nsCocoaWindow::WidgetToScreenOffset() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  return nsCocoaUtils::CocoaRectToGeckoRectDevPix(GetClientCocoaRect(),
                                                  BackingScaleFactor())
      .TopLeft();

  NS_OBJC_END_TRY_BLOCK_RETURN(LayoutDeviceIntPoint(0, 0));
}

LayoutDeviceIntPoint nsCocoaWindow::GetClientOffset() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  LayoutDeviceIntRect clientRect = GetClientBounds();

  return clientRect.TopLeft() - mBounds.TopLeft();

  NS_OBJC_END_TRY_BLOCK_RETURN(LayoutDeviceIntPoint(0, 0));
}

LayoutDeviceIntMargin nsCocoaWindow::ClientToWindowMargin() {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  if (!mWindow || mWindow.drawsContentsIntoWindowFrame ||
      mWindowType == WindowType::Popup) {
    return {};
  }

  NSRect clientNSRect = mWindow.contentLayoutRect;
  NSRect frameNSRect = [mWindow frameRectForChildViewRect:clientNSRect];

  CGFloat backingScale = BackingScaleFactor();
  const auto clientRect =
      nsCocoaUtils::CocoaRectToGeckoRectDevPix(clientNSRect, backingScale);
  const auto frameRect =
      nsCocoaUtils::CocoaRectToGeckoRectDevPix(frameNSRect, backingScale);

  return frameRect - clientRect;

  NS_OBJC_END_TRY_BLOCK_RETURN({});
}

nsMenuBarX* nsCocoaWindow::GetMenuBar() { return mMenuBar; }

void nsCocoaWindow::CaptureRollupEvents(bool aDoCapture) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (aDoCapture) {
    if (!NSApp.isActive) {
      // We need to capture mouse event if we aren't
      // the active application. We only set this up when needed
      // because they cause spurious mouse event after crash
      // and gdb sessions. See bug 699538.
      nsToolkit::GetToolkit()->MonitorAllProcessMouseEvents();
    }

    // Sometimes more than one popup window can be visible at the same time
    // (e.g. nested non-native context menus, or the test case (attachment
    // 276885) for bmo bug 392389, which displays a non-native combo-box in a
    // non-native popup window).  In these cases the "active" popup window
    // should be the topmost -- the (nested) context menu the mouse is currently
    // over, or the combo-box's drop-down list (when it's displayed).  But
    // (among windows that have the same "level") OS X makes topmost the window
    // that last received a mouse-down event, which may be incorrect (in the
    // combo-box case, it makes topmost the window containing the combo-box).
    // So here we fiddle with a non-native popup window's level to make sure the
    // "active" one is always above any other non-native popup windows that
    // may be visible.
    if (mWindowType == WindowType::Popup) {
      SetPopupWindowLevel();
    }
  } else {
    nsToolkit::GetToolkit()->StopMonitoringAllProcessMouseEvents();

    // XXXndeakin this doesn't make sense.
    // Why is the new window assumed to be a modal panel?
    if (mWindow && mWindowType == WindowType::Popup) {
      mWindow.level = NSModalPanelWindowLevel;
    }
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

nsresult nsCocoaWindow::GetAttention(int32_t aCycleCount) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  [NSApp requestUserAttention:NSInformationalRequest];
  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

bool nsCocoaWindow::HasPendingInputEvent() {
  return nsChildView::DoHasPendingInputEvent();
}

void nsCocoaWindow::SetWindowShadowStyle(WindowShadow aStyle) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (mShadowStyle == aStyle) {
    return;
  }

  mShadowStyle = aStyle;

  if (!mWindow || mWindowType != WindowType::Popup) {
    return;
  }

  mWindow.shadowStyle = mShadowStyle;
  [mWindow setEffectViewWrapperForStyle:mShadowStyle];
  [mWindow setHasShadow:aStyle != WindowShadow::None];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsCocoaWindow::SetWindowOpacity(float aOpacity) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!mWindow) {
    return;
  }

  [mWindow setAlphaValue:(CGFloat)aOpacity];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsCocoaWindow::SetColorScheme(const Maybe<ColorScheme>& aScheme) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!mWindow) {
    return;
  }
  NSAppearance* appearance =
      aScheme ? NSAppearanceForColorScheme(*aScheme) : nil;
  if (mWindow.appearance != appearance) {
    mWindow.appearance = appearance;
  }
  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

static inline CGAffineTransform GfxMatrixToCGAffineTransform(
    const gfx::Matrix& m) {
  CGAffineTransform t;
  t.a = m._11;
  t.b = m._12;
  t.c = m._21;
  t.d = m._22;
  t.tx = m._31;
  t.ty = m._32;
  return t;
}

void nsCocoaWindow::SetWindowTransform(const gfx::Matrix& aTransform) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (!mWindow) {
    return;
  }

  // Calling CGSSetWindowTransform when the window is not visible results in
  // misplacing the window into doubled x,y coordinates (see bug 1448132).
  if (!mWindow.isVisible || NSIsEmptyRect(mWindow.frame)) {
    return;
  }

  if (StaticPrefs::widget_window_transforms_disabled()) {
    // CGSSetWindowTransform is a private API. In case calling it causes
    // problems either now or in the future, we'll want to have an easy kill
    // switch. So we allow disabling it with a pref.
    return;
  }

  gfx::Matrix transform = aTransform;

  // aTransform is a transform that should be applied to the window relative
  // to its regular position: If aTransform._31 is 100, then we want the
  // window to be displayed 100 pixels to the right of its regular position.
  // The transform that CGSSetWindowTransform accepts has a different meaning:
  // It's used to answer the question "For the screen pixel at x,y (with the
  // origin at the top left), what pixel in the window's buffer (again with
  // origin top left) should be displayed at that position?"
  // In the example above, this means that we need to call
  // CGSSetWindowTransform with a horizontal translation of -windowPos.x - 100.
  // So we need to invert the transform and adjust it by the window's position.
  if (!transform.Invert()) {
    // Treat non-invertible transforms as the identity transform.
    transform = gfx::Matrix();
  }

  bool isIdentity = transform.IsIdentity();
  if (isIdentity && mWindowTransformIsIdentity) {
    return;
  }

  transform.PreTranslate(-mBounds.x, -mBounds.y);

  // Snap translations to device pixels, to match what we do for CSS transforms
  // and because the window server rounds down instead of to nearest.
  if (!transform.HasNonTranslation() && transform.HasNonIntegerTranslation()) {
    auto snappedTranslation = gfx::IntPoint::Round(transform.GetTranslation());
    transform =
        gfx::Matrix::Translation(snappedTranslation.x, snappedTranslation.y);
  }

  // We also need to account for the backing scale factor: aTransform is given
  // in device pixels, but CGSSetWindowTransform works with logical display
  // pixels.
  CGFloat backingScale = BackingScaleFactor();
  transform.PreScale(backingScale, backingScale);
  transform.PostScale(1 / backingScale, 1 / backingScale);

  CGSConnection cid = _CGSDefaultConnection();
  CGSSetWindowTransform(cid, [mWindow windowNumber],
                        GfxMatrixToCGAffineTransform(transform));

  mWindowTransformIsIdentity = isIdentity;

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsCocoaWindow::SetInputRegion(const InputRegion& aInputRegion) {
  MOZ_ASSERT(mWindowType == WindowType::Popup,
             "This should only be called on popup windows.");
  // TODO: Somehow support aInputRegion.mMargin? Though maybe not.
  if (aInputRegion.mFullyTransparent) {
    [mWindow setIgnoresMouseEvents:YES];
  } else {
    [mWindow setIgnoresMouseEvents:NO];
  }
}

void nsCocoaWindow::SetShowsToolbarButton(bool aShow) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (mWindow) [mWindow setShowsToolbarButton:aShow];

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

bool nsCocoaWindow::GetSupportsNativeFullscreen() {
  return mWindow.collectionBehavior &
         NSWindowCollectionBehaviorFullScreenPrimary;
}

void nsCocoaWindow::SetSupportsNativeFullscreen(
    bool aSupportsNativeFullscreen) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (mWindow) {
    // This determines whether we tell cocoa that the window supports native
    // full screen. If we do so, and another window is in native full screen,
    // this window will also appear in native full screen. We generally only
    // want to do this for primary application windows. We'll set the
    // relevant macnativefullscreen attribute on those, which will lead to us
    // being called with aSupportsNativeFullscreen set to `true` here.
    NSWindowCollectionBehavior newBehavior = [mWindow collectionBehavior];
    if (aSupportsNativeFullscreen) {
      newBehavior |= NSWindowCollectionBehaviorFullScreenPrimary;
    } else {
      newBehavior &= ~NSWindowCollectionBehaviorFullScreenPrimary;
    }
    [mWindow setCollectionBehavior:newBehavior];
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsCocoaWindow::SetWindowAnimationType(
    nsIWidget::WindowAnimationType aType) {
  mAnimationType = aType;
}

void nsCocoaWindow::SetDrawsTitle(bool aDrawTitle) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  // If we don't draw into the window frame, we always want to display window
  // titles.
  mWindow.wantsTitleDrawn = aDrawTitle || !mWindow.drawsContentsIntoWindowFrame;

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

nsresult nsCocoaWindow::SetNonClientMargins(
    const LayoutDeviceIntMargin& margins) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  SetDrawsInTitlebar(margins.top == 0);

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

void nsCocoaWindow::SetDrawsInTitlebar(bool aState) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (mWindow) {
    [mWindow setDrawsContentsIntoWindowFrame:aState];
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

NS_IMETHODIMP nsCocoaWindow::SynthesizeNativeMouseEvent(
    LayoutDeviceIntPoint aPoint, NativeMouseMessage aNativeMessage,
    MouseButton aButton, nsIWidget::Modifiers aModifierFlags,
    nsIObserver* aObserver) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  AutoObserverNotifier notifier(aObserver, "mouseevent");
  if (mPopupContentView) {
    return mPopupContentView->SynthesizeNativeMouseEvent(
        aPoint, aNativeMessage, aButton, aModifierFlags, nullptr);
  }

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

NS_IMETHODIMP nsCocoaWindow::SynthesizeNativeMouseScrollEvent(
    LayoutDeviceIntPoint aPoint, uint32_t aNativeMessage, double aDeltaX,
    double aDeltaY, double aDeltaZ, uint32_t aModifierFlags,
    uint32_t aAdditionalFlags, nsIObserver* aObserver) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  AutoObserverNotifier notifier(aObserver, "mousescrollevent");
  if (mPopupContentView) {
    // Pass nullptr as the observer so that the AutoObserverNotification in
    // nsChildView::SynthesizeNativeMouseScrollEvent will be ignored.
    return mPopupContentView->SynthesizeNativeMouseScrollEvent(
        aPoint, aNativeMessage, aDeltaX, aDeltaY, aDeltaZ, aModifierFlags,
        aAdditionalFlags, nullptr);
  }

  return NS_OK;

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

void nsCocoaWindow::LockAspectRatio(bool aShouldLock) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (aShouldLock) {
    [mWindow setContentAspectRatio:mWindow.frame.size];
    mAspectRatioLocked = true;
  } else {
    // According to
    // https://developer.apple.com/documentation/appkit/nswindow/1419507-aspectratio,
    // aspect ratios and resize increments are mutually exclusive, and the
    // accepted way of cancelling an established aspect ratio is to set the
    // resize increments to 1.0, 1.0
    [mWindow setResizeIncrements:NSMakeSize(1.0, 1.0)];
    mAspectRatioLocked = false;
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsCocoaWindow::UpdateThemeGeometries(
    const nsTArray<ThemeGeometry>& aThemeGeometries) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  if (mPopupContentView) {
    return mPopupContentView->UpdateThemeGeometries(aThemeGeometries);
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

void nsCocoaWindow::SetPopupWindowLevel() {
  if (!mWindow) {
    return;
  }
  // Otherwise, this is a top-level or parent popup. Parent popups always
  // appear just above their parent and essentially ignore the level.
  mWindow.level = NSPopUpMenuWindowLevel;
  mWindow.hidesOnDeactivate = NO;
}

void nsCocoaWindow::SetInputContext(const InputContext& aContext,
                                    const InputContextAction& aAction) {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  mInputContext = aContext;

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

bool nsCocoaWindow::GetEditCommands(NativeKeyBindingsType aType,
                                    const WidgetKeyboardEvent& aEvent,
                                    nsTArray<CommandInt>& aCommands) {
  // Validate the arguments.
  if (NS_WARN_IF(!nsIWidget::GetEditCommands(aType, aEvent, aCommands))) {
    return false;
  }

  NativeKeyBindings* keyBindings = NativeKeyBindings::GetInstance(aType);
  // When the keyboard event is fired from this widget, it must mean that no web
  // content has focus because any web contents should be on `nsChildView`.  And
  // in any locales, the system UI is always horizontal layout.  So, let's pass
  // `Nothing()` for the writing mode here, it won't be treated as in a vertical
  // content.
  keyBindings->GetEditCommands(aEvent, Nothing(), aCommands);
  return true;
}

void nsCocoaWindow::PauseOrResumeCompositor(bool aPause) {
  if (auto* mainChildView =
          static_cast<nsIWidget*>(mWindow.mainChildView.widget)) {
    mainChildView->PauseOrResumeCompositor(aPause);
  }
}

bool nsCocoaWindow::AsyncPanZoomEnabled() const {
  if (mPopupContentView) {
    return mPopupContentView->AsyncPanZoomEnabled();
  }
  return nsBaseWidget::AsyncPanZoomEnabled();
}

bool nsCocoaWindow::StartAsyncAutoscroll(const ScreenPoint& aAnchorLocation,
                                         const ScrollableLayerGuid& aGuid) {
  if (mPopupContentView) {
    return mPopupContentView->StartAsyncAutoscroll(aAnchorLocation, aGuid);
  }
  return nsBaseWidget::StartAsyncAutoscroll(aAnchorLocation, aGuid);
}

void nsCocoaWindow::StopAsyncAutoscroll(const ScrollableLayerGuid& aGuid) {
  if (mPopupContentView) {
    mPopupContentView->StopAsyncAutoscroll(aGuid);
    return;
  }
  nsBaseWidget::StopAsyncAutoscroll(aGuid);
}

already_AddRefed<nsIWidget> nsIWidget::CreateTopLevelWindow() {
  nsCOMPtr<nsIWidget> window = new nsCocoaWindow();
  return window.forget();
}

already_AddRefed<nsIWidget> nsIWidget::CreateChildWindow() {
  nsCOMPtr<nsIWidget> window = new nsChildView();
  return window.forget();
}

@implementation WindowDelegate

// We try to find a gecko menu bar to paint. If one does not exist, just paint
// the application menu by itself so that a window doesn't have some other
// window's menu bar.
+ (void)paintMenubarForWindow:(NSWindow*)aWindow {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  // make sure we only act on windows that have this kind of
  // object as a delegate
  id windowDelegate = [aWindow delegate];
  if ([windowDelegate class] != [self class]) return;

  nsCocoaWindow* geckoWidget = [windowDelegate geckoWidget];
  NS_ASSERTION(geckoWidget, "Window delegate not returning a gecko widget!");

  if (nsMenuBarX* geckoMenuBar = geckoWidget->GetMenuBar()) {
    geckoMenuBar->Paint();
  } else {
    // sometimes we don't have a native application menu early in launching
    if (!sApplicationMenu) {
      return;
    }

    NSMenu* mainMenu = NSApp.mainMenu;
    NS_ASSERTION(
        mainMenu.numberOfItems > 0,
        "Main menu does not have any items, something is terribly wrong!");

    // Create a new menu bar.
    // We create a GeckoNSMenu because all menu bar NSMenu objects should use
    // that subclass for key handling reasons.
    GeckoNSMenu* newMenuBar =
        [[GeckoNSMenu alloc] initWithTitle:@"MainMenuBar"];

    // move the application menu from the existing menu bar to the new one
    NSMenuItem* firstMenuItem = [[mainMenu itemAtIndex:0] retain];
    [mainMenu removeItemAtIndex:0];
    [newMenuBar insertItem:firstMenuItem atIndex:0];
    [firstMenuItem release];

    // set our new menu bar as the main menu
    NSApp.mainMenu = newMenuBar;
    [newMenuBar release];
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (id)initWithGeckoWindow:(nsCocoaWindow*)geckoWind {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  [super init];
  mGeckoWindow = geckoWind;
  mToplevelActiveState = false;
  mHasEverBeenZoomed = false;
  return self;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (NSSize)windowWillResize:(NSWindow*)sender toSize:(NSSize)proposedFrameSize {
  RollUpPopups();
  return proposedFrameSize;
}

- (NSRect)windowWillUseStandardFrame:(NSWindow*)window
                        defaultFrame:(NSRect)newFrame {
  // This function needs to return a rect representing the frame a window would
  // have if it is in its "maximized" size mode. The parameter newFrame is
  // supposed to be a frame representing the maximum window size on the screen
  // where the window currently appears. However, in practice, newFrame can be a
  // much smaller size. So, we ignore newframe and instead return the frame of
  // the entire screen associated with the window. That frame is bigger than the
  // window could actually be, due to the presence of the menubar and possibly
  // the dock, but we never call this function directly, and Cocoa callers will
  // shrink it to its true maximum size.
  return window.screen.frame;
}

void nsCocoaWindow::CocoaSendToplevelActivateEvents() {
  if (mWidgetListener) {
    mWidgetListener->WindowActivated();
  }
}

void nsCocoaWindow::CocoaSendToplevelDeactivateEvents() {
  if (mWidgetListener) {
    mWidgetListener->WindowDeactivated();
  }
}

void nsCocoaWindow::CocoaWindowDidResize() {
  // It's important to update our bounds before we trigger any listeners. This
  // ensures that our bounds are correct when GetScreenBounds is called.
  UpdateBounds();

  if (HandleUpdateFullscreenOnResize()) {
    ReportSizeEvent();
    return;
  }

  // Resizing might have changed our zoom state.
  DispatchSizeModeEvent();
  ReportSizeEvent();
}

- (void)windowDidResize:(NSNotification*)aNotification {
  BaseWindow* window = [aNotification object];
  [window updateTrackingArea];

  if (!mGeckoWindow) return;

  mGeckoWindow->CocoaWindowDidResize();
}

- (void)windowDidChangeScreen:(NSNotification*)aNotification {
  if (!mGeckoWindow) return;

  // Because of Cocoa's peculiar treatment of zero-size windows (see comments
  // at GetBackingScaleFactor() above), we sometimes have a situation where
  // our concept of backing scale (based on the screen where the zero-sized
  // window is positioned) differs from Cocoa's idea (always based on the
  // Retina screen, AFAICT, even when an external non-Retina screen is the
  // primary display).
  //
  // As a result, if the window was created with zero size on an external
  // display, but then made visible on the (secondary) Retina screen, we
  // will *not* get a windowDidChangeBackingProperties notification for it.
  // This leads to an incorrect GetDefaultScale(), and widget coordinate
  // confusion, as per bug 853252.
  //
  // To work around this, we check for a backing scale mismatch when we
  // receive a windowDidChangeScreen notification, as we will receive this
  // even if Cocoa was already treating the zero-size window as having
  // Retina backing scale.
  NSWindow* window = (NSWindow*)[aNotification object];
  if ([window respondsToSelector:@selector(backingScaleFactor)]) {
    if (GetBackingScaleFactor(window) != mGeckoWindow->BackingScaleFactor()) {
      mGeckoWindow->BackingScaleFactorChanged();
    }
  }

  mGeckoWindow->ReportMoveEvent();
}

- (void)windowWillEnterFullScreen:(NSNotification*)notification {
  if (!mGeckoWindow) {
    return;
  }
  mGeckoWindow->CocoaWindowWillEnterFullscreen(true);
}

// Lion's full screen mode will bypass our internal fullscreen tracking, so
// we need to catch it when we transition and call our own methods, which in
// turn will fire "fullscreen" events.
- (void)windowDidEnterFullScreen:(NSNotification*)notification {
  // On Yosemite, the NSThemeFrame class has two new properties --
  // titlebarView (an NSTitlebarView object) and titlebarContainerView (an
  // NSTitlebarContainerView object).  These are used to display the titlebar
  // in fullscreen mode.  In Safari they're not transparent.  But in Firefox
  // for some reason they are, which causes bug 1069658.  The following code
  // works around this Apple bug or design flaw.
  NSWindow* window = notification.object;
  NSView* frameView = window.contentView.superview;
  NSView* titlebarView = nil;
  NSView* titlebarContainerView = nil;
  if ([frameView respondsToSelector:@selector(titlebarView)]) {
    titlebarView = [frameView titlebarView];
  }
  if ([frameView respondsToSelector:@selector(titlebarContainerView)]) {
    titlebarContainerView = [frameView titlebarContainerView];
  }
  if ([titlebarView respondsToSelector:@selector(setTransparent:)]) {
    [titlebarView setTransparent:NO];
  }
  if ([titlebarContainerView respondsToSelector:@selector(setTransparent:)]) {
    [titlebarContainerView setTransparent:NO];
  }

  if (@available(macOS 11.0, *)) {
    if ([window isKindOfClass:[ToolbarWindow class]]) {
      // In order to work around a drawing bug with windows in full screen
      // mode, disable titlebar separators for full screen windows of the
      // ToolbarWindow class. The drawing bug was filed as FB9056136. See bug
      // 1700211 and bug 1912338 for more details.
      window.titlebarSeparatorStyle = NSTitlebarSeparatorStyleNone;
    }
  }

  if (!mGeckoWindow) {
    return;
  }
  mGeckoWindow->CocoaWindowDidEnterFullscreen(true);
}

- (void)windowWillExitFullScreen:(NSNotification*)notification {
  if (!mGeckoWindow) {
    return;
  }
  mGeckoWindow->CocoaWindowWillEnterFullscreen(false);
}

- (void)windowDidExitFullScreen:(NSNotification*)notification {
  if (!mGeckoWindow) {
    return;
  }
  mGeckoWindow->CocoaWindowDidEnterFullscreen(false);
}

- (void)windowDidFailToEnterFullScreen:(NSNotification*)notification {
  if (!mGeckoWindow) {
    return;
  }

  MOZ_ASSERT((mGeckoWindow->GetCocoaWindow().styleMask &
              NSWindowStyleMaskFullScreen) == 0);
  MOZ_ASSERT(mGeckoWindow->SizeMode() == nsSizeMode_Fullscreen);

  // We're in a strange situation. We've told DOM that we are going to
  // fullscreen by changing our size mode, and therefore the window
  // content is what we would show if we were properly in fullscreen.
  // But the window is actually in a windowed style. We have to do
  // several things:
  // 1) Clear sWindowInNativeTransition and mTransitionCurrent, both set
  //    when we started the fullscreen transition.
  // 2) Change our size mode to windowed.
  // Conveniently, we can do these things by pretending we just arrived
  // at windowed mode, and all will be sorted out.
  mGeckoWindow->CocoaWindowDidEnterFullscreen(false);
}

- (void)windowDidFailToExitFullScreen:(NSNotification*)notification {
  if (!mGeckoWindow) {
    return;
  }
  // Similarly to windowDidFailToEnterFullScreen, we can get the right
  // result by pretending we just entered fullscreen.
  mGeckoWindow->CocoaWindowDidEnterFullscreen(true);
}

- (void)windowDidBecomeMain:(NSNotification*)aNotification {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  RollUpPopups();
  ChildViewMouseTracker::ReEvaluateMouseEnterState();

  // [NSApp _isRunningAppModal] will return true if we're running an OS dialog
  // app modally. If one of those is up then we want it to retain its menu bar.
  if (NSApp._isRunningAppModal) {
    return;
  }
  NSWindow* window = aNotification.object;
  if (window) {
    [WindowDelegate paintMenubarForWindow:window];
  }

  if ([window isKindOfClass:[ToolbarWindow class]]) {
    [(ToolbarWindow*)window windowMainStateChanged];
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)windowDidResignMain:(NSNotification*)aNotification {
  RollUpPopups();
  ChildViewMouseTracker::ReEvaluateMouseEnterState();

  // [NSApp _isRunningAppModal] will return true if we're running an OS dialog
  // app modally. If one of those is up then we want it to retain its menu bar.
  if ([NSApp _isRunningAppModal]) return;
  RefPtr<nsMenuBarX> hiddenWindowMenuBar =
      nsMenuUtilsX::GetHiddenWindowMenuBar();
  if (hiddenWindowMenuBar) {
    // printf("painting hidden window menu bar due to window losing main
    // status\n");
    hiddenWindowMenuBar->Paint();
  }

  NSWindow* window = [aNotification object];
  if ([window isKindOfClass:[ToolbarWindow class]]) {
    [(ToolbarWindow*)window windowMainStateChanged];
  }
}

- (void)windowDidBecomeKey:(NSNotification*)aNotification {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  RollUpPopups();
  ChildViewMouseTracker::ReEvaluateMouseEnterState();

  NSWindow* window = [aNotification object];
  auto* mainChildView =
      static_cast<nsChildView*>([[(BaseWindow*)window mainChildView] widget]);
  if (mainChildView) {
    if (mainChildView->GetInputContext().IsPasswordEditor()) {
      TextInputHandler::EnableSecureEventInput();
    } else {
      TextInputHandler::EnsureSecureEventInputDisabled();
    }
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)windowDidResignKey:(NSNotification*)aNotification {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  RollUpPopups(nsIRollupListener::AllowAnimations::No);

  ChildViewMouseTracker::ReEvaluateMouseEnterState();
  TextInputHandler::EnsureSecureEventInputDisabled();

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

- (void)windowWillMove:(NSNotification*)aNotification {
  RollUpPopups();
}

- (void)windowDidMove:(NSNotification*)aNotification {
  if (mGeckoWindow) mGeckoWindow->ReportMoveEvent();
}

- (BOOL)windowShouldClose:(id)sender {
  nsIWidgetListener* listener =
      mGeckoWindow ? mGeckoWindow->GetWidgetListener() : nullptr;
  if (listener) listener->RequestWindowClose(mGeckoWindow);
  return NO;  // gecko will do it
}

- (void)windowWillClose:(NSNotification*)aNotification {
  RollUpPopups();
}

- (void)windowWillMiniaturize:(NSNotification*)aNotification {
  RollUpPopups();
}

- (void)windowDidMiniaturize:(NSNotification*)aNotification {
  if (!mGeckoWindow) {
    return;
  }
  mGeckoWindow->FinishCurrentTransitionIfMatching(
      nsCocoaWindow::TransitionType::Miniaturize);
}

- (void)windowDidDeminiaturize:(NSNotification*)aNotification {
  if (!mGeckoWindow) {
    return;
  }
  mGeckoWindow->FinishCurrentTransitionIfMatching(
      nsCocoaWindow::TransitionType::Deminiaturize);
}

- (BOOL)windowShouldZoom:(NSWindow*)window toFrame:(NSRect)proposedFrame {
  if (!mHasEverBeenZoomed && window.isZoomed) {
    return NO;  // See bug 429954.
  }
  mHasEverBeenZoomed = YES;
  return YES;
}

- (void)windowDidChangeBackingProperties:(NSNotification*)aNotification {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  NSWindow* window = (NSWindow*)[aNotification object];

  if ([window respondsToSelector:@selector(backingScaleFactor)]) {
    CGFloat oldFactor = [[[aNotification userInfo]
        objectForKey:@"NSBackingPropertyOldScaleFactorKey"] doubleValue];
    if (window.backingScaleFactor != oldFactor) {
      mGeckoWindow->BackingScaleFactorChanged();
    }
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// This method is on NSWindowDelegate starting with 10.9
- (void)windowDidChangeOcclusionState:(NSNotification*)aNotification {
  if (mGeckoWindow) {
    mGeckoWindow->DispatchOcclusionEvent();
  }
}

- (nsCocoaWindow*)geckoWidget {
  return mGeckoWindow;
}

- (bool)toplevelActiveState {
  return mToplevelActiveState;
}

- (void)sendToplevelActivateEvents {
  if (!mToplevelActiveState && mGeckoWindow) {
    mGeckoWindow->CocoaSendToplevelActivateEvents();

    mToplevelActiveState = true;
  }
}

- (void)sendToplevelDeactivateEvents {
  if (mToplevelActiveState && mGeckoWindow) {
    mGeckoWindow->CocoaSendToplevelDeactivateEvents();
    mToplevelActiveState = false;
  }
}

@end

@interface NSView (FrameViewMethodSwizzling)
- (NSPoint)FrameView__closeButtonOrigin;
- (CGFloat)FrameView__titlebarHeight;
@end

@implementation NSView (FrameViewMethodSwizzling)

- (NSPoint)FrameView__closeButtonOrigin {
  if (![self.window isKindOfClass:[ToolbarWindow class]]) {
    return self.FrameView__closeButtonOrigin;
  }
  auto* win = static_cast<ToolbarWindow*>(self.window);
  if (win.drawsContentsIntoWindowFrame && !win.wantsTitleDrawn &&
      !(win.styleMask & NSWindowStyleMaskFullScreen) &&
      (win.styleMask & NSWindowStyleMaskTitled)) {
    const NSRect buttonsRect = win.windowButtonsRect;
    if (NSIsEmptyRect(buttonsRect)) {
      // Empty rect. Let's hide the buttons.
      // Position is in non-flipped window coordinates. Using frame's height
      // for the vertical coordinate will move the buttons above the window,
      // making them invisible.
      return NSMakePoint(buttonsRect.origin.x, win.frame.size.height);
    }
    if (win.windowTitlebarLayoutDirection ==
        NSUserInterfaceLayoutDirectionRightToLeft) {
      // We're in RTL mode, which means that the close button is the rightmost
      // button of the three window buttons. and buttonsRect.origin is the
      // bottom left corner of the green (zoom) button. The close button is 40px
      // to the right of the zoom button. This is confirmed to be the same on
      // all macOS versions between 10.12 - 12.0.
      return NSMakePoint(buttonsRect.origin.x + 40.0f, buttonsRect.origin.y);
    }
    return buttonsRect.origin;
  }
  return self.FrameView__closeButtonOrigin;
}

- (CGFloat)FrameView__titlebarHeight {
  // XXX: Shouldn't this be [super FrameView__titlebarHeight]?
  CGFloat height = [self FrameView__titlebarHeight];
  if ([self.window isKindOfClass:[ToolbarWindow class]]) {
    // Make sure that the titlebar height includes our shifted buttons.
    // The following coordinates are in window space, with the origin being at
    // the bottom left corner of the window.
    auto* win = static_cast<ToolbarWindow*>(self.window);
    CGFloat frameHeight = self.frame.size.height;
    CGFloat windowButtonY = frameHeight;
    if (!NSIsEmptyRect(win.windowButtonsRect) &&
        win.drawsContentsIntoWindowFrame &&
        !(win.styleMask & NSWindowStyleMaskFullScreen) &&
        (win.styleMask & NSWindowStyleMaskTitled)) {
      windowButtonY = win.windowButtonsRect.origin.y;
    }
    height = std::max(height, frameHeight - windowButtonY);
  }
  return height;
}

@end

static NSMutableSet* gSwizzledFrameViewClasses = nil;

@interface NSWindow (PrivateSetNeedsDisplayInRectMethod)
- (void)_setNeedsDisplayInRect:(NSRect)aRect;
@end

@interface BaseWindow (Private)
- (void)removeTrackingArea;
- (void)cursorUpdated:(NSEvent*)aEvent;
- (void)reflowTitlebarElements;
@end

@implementation BaseWindow

// The frame of a window is implemented using undocumented NSView subclasses.
// We offset the window buttons by overriding the method _closeButtonOrigin on
// these frame view classes. The class which is
// used for a window is determined in the window's frameViewClassForStyleMask:
// method, so this is where we make sure that we have swizzled the method on
// all encountered classes.
+ (Class)frameViewClassForStyleMask:(NSUInteger)styleMask {
  Class frameViewClass = [super frameViewClassForStyleMask:styleMask];

  if (!gSwizzledFrameViewClasses) {
    gSwizzledFrameViewClasses = [[NSMutableSet setWithCapacity:3] retain];
    if (!gSwizzledFrameViewClasses) {
      return frameViewClass;
    }
  }

  static IMP our_closeButtonOrigin = class_getMethodImplementation(
      [NSView class], @selector(FrameView__closeButtonOrigin));
  static IMP our_titlebarHeight = class_getMethodImplementation(
      [NSView class], @selector(FrameView__titlebarHeight));

  if (![gSwizzledFrameViewClasses containsObject:frameViewClass]) {
    // Either of these methods might be implemented in both a subclass of
    // NSFrameView and one of its own subclasses.  Which means that if we
    // aren't careful we might end up swizzling the same method twice.
    // Since method swizzling involves swapping pointers, this would break
    // things.
    IMP _closeButtonOrigin = class_getMethodImplementation(
        frameViewClass, @selector(_closeButtonOrigin));
    if (_closeButtonOrigin && _closeButtonOrigin != our_closeButtonOrigin) {
      nsToolkit::SwizzleMethods(frameViewClass, @selector(_closeButtonOrigin),
                                @selector(FrameView__closeButtonOrigin));
    }

    // Override _titlebarHeight so that the floating titlebar doesn't clip the
    // bottom of the window buttons which we move down with our override of
    // _closeButtonOrigin.
    IMP _titlebarHeight = class_getMethodImplementation(
        frameViewClass, @selector(_titlebarHeight));
    if (_titlebarHeight && _titlebarHeight != our_titlebarHeight) {
      nsToolkit::SwizzleMethods(frameViewClass, @selector(_titlebarHeight),
                                @selector(FrameView__titlebarHeight));
    }

    [gSwizzledFrameViewClasses addObject:frameViewClass];
  }

  return frameViewClass;
}

- (id)initWithContentRect:(NSRect)aContentRect
                styleMask:(NSUInteger)aStyle
                  backing:(NSBackingStoreType)aBufferingType
                    defer:(BOOL)aFlag {
  mDrawsIntoWindowFrame = NO;
  [super initWithContentRect:aContentRect
                   styleMask:aStyle
                     backing:aBufferingType
                       defer:aFlag];
  mState = nil;
  mDisabledNeedsDisplay = NO;
  mTrackingArea = nil;
  mViewWithTrackingArea = nil;
  mDirtyRect = NSZeroRect;
  mBeingShown = NO;
  mDrawTitle = NO;
  mTouchBar = nil;
  mIsAnimationSuppressed = NO;
  [self updateTrackingArea];

  return self;
}

// Returns an autoreleased NSImage.
static NSImage* GetMenuMaskImage() {
  const CGFloat radius = 6.0f;
  const NSSize maskSize = {radius * 3.0f, radius * 3.0f};
  NSImage* maskImage = [NSImage imageWithSize:maskSize
                                      flipped:FALSE
                               drawingHandler:^BOOL(NSRect dstRect) {
                                 NSBezierPath* path = [NSBezierPath
                                     bezierPathWithRoundedRect:dstRect
                                                       xRadius:radius
                                                       yRadius:radius];
                                 [NSColor.blackColor set];
                                 [path fill];
                                 return YES;
                               }];
  maskImage.capInsets = NSEdgeInsetsMake(radius, radius, radius, radius);
  return maskImage;
}

// Add an effect view wrapper if needed so that the OS draws the appropriate
// vibrancy effect and window border.
- (void)setEffectViewWrapperForStyle:(WindowShadow)aStyle {
  NSView* wrapper = [&]() -> NSView* {
    if (aStyle == WindowShadow::Menu || aStyle == WindowShadow::Tooltip) {
      const bool isMenu = aStyle == WindowShadow::Menu;
      auto* effectView =
          [[NSVisualEffectView alloc] initWithFrame:self.contentView.frame];
      effectView.material =
          isMenu ? NSVisualEffectMaterialMenu : NSVisualEffectMaterialToolTip;
      // Tooltip and menu windows are never "key", so we need to tell the
      // vibrancy effect to look active regardless of window state.
      effectView.state = NSVisualEffectStateActive;
      effectView.blendingMode = NSVisualEffectBlendingModeBehindWindow;
      if (isMenu) {
        // Turn on rounded corner masking.
        effectView.maskImage = GetMenuMaskImage();
      }
      return effectView;
    }
    return [[NSView alloc] initWithFrame:self.contentView.frame];
  }();

  wrapper.wantsLayer = YES;
  // Swap out our content view by the new view. Setting .contentView releases
  // the old view.
  NSView* childView = [self.mainChildView retain];
  [childView removeFromSuperview];
  [wrapper addSubview:childView];
  [childView release];
  super.contentView = wrapper;
  [wrapper release];
}

- (NSTouchBar*)makeTouchBar {
  mTouchBar = [[nsTouchBar alloc] init];
  if (mTouchBar) {
    sTouchBarIsInitialized = YES;
  }
  return mTouchBar;
}

- (void)setBeingShown:(BOOL)aValue {
  mBeingShown = aValue;
}

- (BOOL)isBeingShown {
  return mBeingShown;
}

- (BOOL)isVisibleOrBeingShown {
  return [super isVisible] || mBeingShown;
}

- (void)setIsAnimationSuppressed:(BOOL)aValue {
  mIsAnimationSuppressed = aValue;
}

- (BOOL)isAnimationSuppressed {
  return mIsAnimationSuppressed;
}

- (void)disableSetNeedsDisplay {
  mDisabledNeedsDisplay = YES;
}

- (void)enableSetNeedsDisplay {
  mDisabledNeedsDisplay = NO;
}

- (void)dealloc {
  [mTouchBar release];
  [self removeTrackingArea];
  ChildViewMouseTracker::OnDestroyWindow(self);
  [super dealloc];
}

static const NSString* kStateTitleKey = @"title";
static const NSString* kStateDrawsContentsIntoWindowFrameKey =
    @"drawsContentsIntoWindowFrame";
static const NSString* kStateShowsToolbarButton = @"showsToolbarButton";
static const NSString* kStateCollectionBehavior = @"collectionBehavior";
static const NSString* kStateWantsTitleDrawn = @"wantsTitleDrawn";

- (void)importState:(NSDictionary*)aState {
  if (NSString* title = [aState objectForKey:kStateTitleKey]) {
    [self setTitle:title];
  }
  [self setDrawsContentsIntoWindowFrame:
            [[aState objectForKey:kStateDrawsContentsIntoWindowFrameKey]
                boolValue]];
  [self setShowsToolbarButton:[[aState objectForKey:kStateShowsToolbarButton]
                                  boolValue]];
  [self setCollectionBehavior:[[aState objectForKey:kStateCollectionBehavior]
                                  unsignedIntValue]];
  [self setWantsTitleDrawn:[[aState objectForKey:kStateWantsTitleDrawn]
                               boolValue]];
}

- (NSMutableDictionary*)exportState {
  NSMutableDictionary* state = [NSMutableDictionary dictionaryWithCapacity:10];
  if (NSString* title = self.title) {
    [state setObject:title forKey:kStateTitleKey];
  }
  [state setObject:[NSNumber numberWithBool:self.drawsContentsIntoWindowFrame]
            forKey:kStateDrawsContentsIntoWindowFrameKey];
  [state setObject:[NSNumber numberWithBool:self.showsToolbarButton]
            forKey:kStateShowsToolbarButton];
  [state setObject:[NSNumber numberWithUnsignedInt:self.collectionBehavior]
            forKey:kStateCollectionBehavior];
  [state setObject:[NSNumber numberWithBool:self.wantsTitleDrawn]
            forKey:kStateWantsTitleDrawn];
  return state;
}

- (void)setDrawsContentsIntoWindowFrame:(BOOL)aState {
  bool changed = aState != mDrawsIntoWindowFrame;
  mDrawsIntoWindowFrame = aState;
  if (changed) {
    [self reflowTitlebarElements];
  }
}

- (BOOL)drawsContentsIntoWindowFrame {
  return mDrawsIntoWindowFrame;
}

- (NSRect)childViewRectForFrameRect:(NSRect)aFrameRect {
  if (mDrawsIntoWindowFrame) {
    return aFrameRect;
  }
  NSUInteger styleMask = [self styleMask];
  styleMask &= ~NSWindowStyleMaskFullSizeContentView;
  return [NSWindow contentRectForFrameRect:aFrameRect styleMask:styleMask];
}

- (NSRect)frameRectForChildViewRect:(NSRect)aChildViewRect {
  if (mDrawsIntoWindowFrame) {
    return aChildViewRect;
  }
  NSUInteger styleMask = [self styleMask];
  styleMask &= ~NSWindowStyleMaskFullSizeContentView;
  return [NSWindow frameRectForContentRect:aChildViewRect styleMask:styleMask];
}

- (NSTimeInterval)animationResizeTime:(NSRect)newFrame {
  if (mIsAnimationSuppressed) {
    // Should not animate the initial session-restore size change
    return 0.0;
  }

  return [super animationResizeTime:newFrame];
}

- (void)setWantsTitleDrawn:(BOOL)aDrawTitle {
  mDrawTitle = aDrawTitle;
  [self setTitleVisibility:mDrawTitle ? NSWindowTitleVisible
                                      : NSWindowTitleHidden];
}

- (BOOL)wantsTitleDrawn {
  return mDrawTitle;
}

- (NSView*)trackingAreaView {
  NSView* contentView = self.contentView;
  return contentView.superview ? contentView.superview : contentView;
}

- (NSArray<NSView*>*)contentViewContents {
  return [[self.contentView.subviews copy] autorelease];
}

- (ChildView*)mainChildView {
  NSView* contentView = self.contentView;
  NSView* lastView = contentView.subviews.lastObject;
  if ([lastView isKindOfClass:[ChildView class]]) {
    return (ChildView*)lastView;
  }
  return nil;
}

- (void)removeTrackingArea {
  [mViewWithTrackingArea removeTrackingArea:mTrackingArea];

  [mTrackingArea release];
  mTrackingArea = nil;

  [mViewWithTrackingArea release];
  mViewWithTrackingArea = nil;
}

- (void)updateTrackingArea {
  [self removeTrackingArea];

  mViewWithTrackingArea = [self.trackingAreaView retain];
  const NSTrackingAreaOptions options = NSTrackingMouseEnteredAndExited |
                                        NSTrackingMouseMoved |
                                        NSTrackingActiveAlways;
  mTrackingArea =
      [[NSTrackingArea alloc] initWithRect:[mViewWithTrackingArea bounds]
                                   options:options
                                     owner:self
                                  userInfo:nil];
  [mViewWithTrackingArea addTrackingArea:mTrackingArea];
}

- (void)mouseEntered:(NSEvent*)aEvent {
  ChildViewMouseTracker::MouseEnteredWindow(aEvent);
}

- (void)mouseExited:(NSEvent*)aEvent {
  ChildViewMouseTracker::MouseExitedWindow(aEvent);
}

- (void)mouseMoved:(NSEvent*)aEvent {
  ChildViewMouseTracker::MouseMoved(aEvent);
}

- (void)cursorUpdated:(NSEvent*)aEvent {
  // Nothing to do here, but NSTrackingArea wants us to implement this method.
}

- (void)_setNeedsDisplayInRect:(NSRect)aRect {
  // Prevent unnecessary invalidations due to moving NSViews (e.g. for plugins)
  if (!mDisabledNeedsDisplay) {
    // This method is only called by Cocoa, so when we're here, we know that
    // it's available and don't need to check whether our superclass responds
    // to the selector.
    [super _setNeedsDisplayInRect:aRect];
    mDirtyRect = NSUnionRect(mDirtyRect, aRect);
  }
}

- (NSRect)getAndResetNativeDirtyRect {
  NSRect dirtyRect = mDirtyRect;
  mDirtyRect = NSZeroRect;
  return dirtyRect;
}

// Possibly move the titlebar buttons.
- (void)reflowTitlebarElements {
  NSView* frameView = self.contentView.superview;
  if ([frameView respondsToSelector:@selector(_tileTitlebarAndRedisplay:)]) {
    [frameView _tileTitlebarAndRedisplay:NO];
  }
}

- (BOOL)respondsToSelector:(SEL)aSelector {
  // Claim the window doesn't respond to this so that the system
  // doesn't steal keyboard equivalents for it. Bug 613710.
  if (aSelector == @selector(cancelOperation:)) {
    return NO;
  }

  return [super respondsToSelector:aSelector];
}

- (void)doCommandBySelector:(SEL)aSelector {
  // We override this so that it won't beep if it can't act.
  // We want to control the beeping for missing or disabled
  // commands ourselves.
  [self tryToPerform:aSelector with:nil];
}

- (id)accessibilityAttributeValue:(NSString*)attribute {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  id retval = [super accessibilityAttributeValue:attribute];

  // The following works around a problem with Text-to-Speech on OS X 10.7.
  // See bug 674612 for more info.
  //
  // When accessibility is off, AXUIElementCopyAttributeValue(), when called
  // on an AXApplication object to get its AXFocusedUIElement attribute,
  // always returns an AXWindow object (the actual browser window -- never a
  // mozAccessible object).  This also happens with accessibility turned on,
  // if no other object in the browser window has yet been focused.  But if
  // the browser window has a title bar (as it currently always does), the
  // AXWindow object will always have four "accessible" children, one of which
  // is an AXStaticText object (the title bar's "title"; the other three are
  // the close, minimize and zoom buttons).  This means that (for complicated
  // reasons, for which see bug 674612) Text-to-Speech on OS X 10.7 will often
  // "speak" the window title, no matter what text is selected, or even if no
  // text at all is selected.  (This always happens when accessibility is off.
  // It doesn't happen in Firefox releases because Apple has (on OS X 10.7)
  // special-cased the handling of apps whose CFBundleIdentifier is
  // org.mozilla.firefox.)
  //
  // We work around this problem by only returning AXChildren that are
  // mozAccessible object or are one of the titlebar's buttons (which
  // instantiate subclasses of NSButtonCell).
  if ([retval isKindOfClass:[NSArray class]] &&
      [attribute isEqualToString:@"AXChildren"]) {
    NSMutableArray* holder = [NSMutableArray arrayWithCapacity:10];
    [holder addObjectsFromArray:(NSArray*)retval];
    NSUInteger count = [holder count];
    for (NSInteger i = count - 1; i >= 0; --i) {
      id item = [holder objectAtIndex:i];
      // Remove anything from holder that isn't one of the titlebar's buttons
      // (which instantiate subclasses of NSButtonCell) or a mozAccessible
      // object (or one of its subclasses).
      if (![item isKindOfClass:[NSButtonCell class]] &&
          ![item respondsToSelector:@selector(hasRepresentedView)]) {
        [holder removeObjectAtIndex:i];
      }
    }
    retval = [NSArray arrayWithArray:holder];
  }

  return retval;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (void)releaseJSObjects {
  [mTouchBar releaseJSObjects];
}

@end

@interface MOZTitlebarAccessoryView : NSView
@end

@implementation MOZTitlebarAccessoryView : NSView
- (void)viewWillMoveToWindow:(NSWindow*)aWindow {
  if (aWindow) {
    // When entering full screen mode, titlebar accessory views are inserted
    // into a floating NSWindow which houses the window titlebar and toolbars.
    // In order to work around a drawing bug with windows in full screen mode,
    // disable titlebar separators for all NSWindows that this view is used in
    // that are not of the ToolbarWindow class, such as the floating full
    // screen toolbar window. The drawing bug was filed as FB9056136. See bug
    // 1700211 and bug 1912338 for more details.
    if (@available(macOS 11.0, *)) {
      aWindow.titlebarSeparatorStyle =
          [aWindow isKindOfClass:[ToolbarWindow class]]
              ? NSTitlebarSeparatorStyleAutomatic
              : NSTitlebarSeparatorStyleNone;
    }
  }
}
@end

@implementation FullscreenTitlebarTracker
- (FullscreenTitlebarTracker*)init {
  [super init];
  self.hidden = YES;
  return self;
}
- (void)loadView {
  self.view =
      [[[MOZTitlebarAccessoryView alloc] initWithFrame:NSZeroRect] autorelease];
}
@end

// Drop all mouse events if a modal window has appeared above us.
// This helps make us behave as if the OS were running a "real" modal event
// loop.
static bool MaybeDropEventForModalWindow(NSEvent* aEvent, id aDelegate) {
  if (!sModalWindowCount) {
    return false;
  }

  NSEventType type = [aEvent type];
  switch (type) {
    case NSEventTypeScrollWheel:
    case NSEventTypeLeftMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeRightMouseDown:
    case NSEventTypeRightMouseUp:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeOtherMouseUp:
    case NSEventTypeMouseMoved:
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDragged:
      break;
    default:
      return false;
  }

  if (aDelegate && [aDelegate isKindOfClass:[WindowDelegate class]]) {
    if (nsCocoaWindow* widget = [(WindowDelegate*)aDelegate geckoWidget]) {
      if (!widget->IsModal() || widget->HasModalDescendants()) {
        return true;
      }
    }
  }
  return false;
}

@implementation ToolbarWindow

- (id)initWithContentRect:(NSRect)aChildViewRect
                styleMask:(NSUInteger)aStyle
                  backing:(NSBackingStoreType)aBufferingType
                    defer:(BOOL)aFlag {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // We treat aChildViewRect as the rectangle that the window's main ChildView
  // should be sized to. Get the right frameRect for the requested child view
  // rect.
  NSRect frameRect = [NSWindow frameRectForContentRect:aChildViewRect
                                             styleMask:aStyle];

  // Always size the content view to the full frame size of the window.
  // We do this even if we want this window to have a titlebar; in that case,
  // the window's content view covers the entire window but the ChildView inside
  // it will only cover the content area. We do this so that we can render the
  // titlebar gradient manually, with a subview of our content view that's
  // positioned in the titlebar area. This lets us have a smooth connection
  // between titlebar and toolbar gradient in case the window has a "unified
  // toolbar + titlebar" look. Moreover, always using a full size content view
  // lets us toggle the titlebar on and off without changing the window's style
  // mask (which would have other subtle effects, for example on keyboard
  // focus).
  aStyle |= NSWindowStyleMaskFullSizeContentView;

  // -[NSWindow initWithContentRect:styleMask:backing:defer:] calls
  // [self frameRectForContentRect:styleMask:] to convert the supplied content
  // rect to the window's frame rect. We've overridden that method to be a
  // pass-through function. So, in order to get the intended frameRect, we need
  // to supply frameRect itself as the "content rect".
  NSRect contentRect = frameRect;

  if ((self = [super initWithContentRect:contentRect
                               styleMask:aStyle
                                 backing:aBufferingType
                                   defer:aFlag])) {
    mWindowButtonsRect = NSZeroRect;

    mFullscreenTitlebarTracker = [[FullscreenTitlebarTracker alloc] init];
    // revealAmount is an undocumented property of
    // NSTitlebarAccessoryViewController that updates whenever the menubar
    // slides down in fullscreen mode.
    [mFullscreenTitlebarTracker addObserver:self
                                 forKeyPath:@"revealAmount"
                                    options:NSKeyValueObservingOptionNew
                                    context:nil];
    // Adding this accessory view controller allows us to shift the toolbar down
    // when the user mouses to the top of the screen in fullscreen.
    [(NSWindow*)self
        addTitlebarAccessoryViewController:mFullscreenTitlebarTracker];
  }
  return self;

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary<NSKeyValueChangeKey, id>*)change
                       context:(void*)context {
  if ([keyPath isEqualToString:@"revealAmount"]) {
    [[self mainChildView] ensureNextCompositeIsAtomicWithMainThreadPaint];
    NSNumber* revealAmount = (change[NSKeyValueChangeNewKey]);
    [self updateTitlebarShownAmount:[revealAmount doubleValue]];
  } else {
    [super observeValueForKeyPath:keyPath
                         ofObject:object
                           change:change
                          context:context];
  }
}

static bool ScreenHasNotch(nsCocoaWindow* aGeckoWindow) {
  if (@available(macOS 12.0, *)) {
    nsCOMPtr<nsIScreen> widgetScreen = aGeckoWindow->GetWidgetScreen();
    NSScreen* cocoaScreen =
        ScreenHelperCocoa::CocoaScreenForScreen(widgetScreen);
    return cocoaScreen.safeAreaInsets.top != 0.0f;
  }
  return false;
}

static bool ShouldShiftByMenubarHeightInFullscreen(nsCocoaWindow* aWindow) {
  switch (StaticPrefs::widget_macos_shift_by_menubar_on_fullscreen()) {
    case 0:
      return false;
    case 1:
      return true;
    default:
      break;
  }
  // TODO: On notch-less macbooks, this creates extra space when the
  // "automatically show and hide the menubar on fullscreen" option is unchecked
  // (default checked). We tried to detect that in bug 1737831 but it wasn't
  // reliable enough, see the regressions from that bug. For now, stick to the
  // good behavior for default configurations (that is, shift by menubar height
  // on notch-less macbooks, and don't for devices that have a notch). This will
  // need refinement in the future.
  return !ScreenHasNotch(aWindow);
}

- (void)updateTitlebarShownAmount:(CGFloat)aShownAmount {
  if (!(self.styleMask & NSWindowStyleMaskFullScreen)) {
    // We are not interested in the size of the titlebar unless we are in
    // fullscreen.
    return;
  }

  // [NSApp mainMenu] menuBarHeight] returns one of two values: the full height
  // if the menubar is shown or is in the process of being shown, and 0
  // otherwise. Since we are multiplying the menubar height by aShownAmount, we
  // always want the full height.
  CGFloat menuBarHeight = NSApp.mainMenu.menuBarHeight;
  if (menuBarHeight > 0.0f) {
    mMenuBarHeight = menuBarHeight;
  }
  if ([[self delegate] isKindOfClass:[WindowDelegate class]]) {
    WindowDelegate* windowDelegate = (WindowDelegate*)[self delegate];
    nsCocoaWindow* geckoWindow = [windowDelegate geckoWidget];
    if (!geckoWindow) {
      return;
    }

    if (nsIWidgetListener* listener = geckoWindow->GetWidgetListener()) {
      // titlebarHeight returns 0 when we're in fullscreen, return the default
      // titlebar height.
      CGFloat shiftByPixels =
          LookAndFeel::GetInt(LookAndFeel::IntID::MacTitlebarHeight) *
          aShownAmount;
      if (ShouldShiftByMenubarHeightInFullscreen(geckoWindow)) {
        shiftByPixels += mMenuBarHeight * aShownAmount;
      }
      // Use desktop pixels rather than the DesktopToLayoutDeviceScale in
      // nsCocoaWindow. The latter accounts for screen DPI. We don't want that
      // because the revealAmount property already accounts for it, so we'd be
      // compounding DPI scales > 1.
      listener->MacFullscreenMenubarOverlapChanged(DesktopCoord(shiftByPixels));
    }
  }
}

- (void)dealloc {
  [mFullscreenTitlebarTracker removeObserver:self forKeyPath:@"revealAmount"];
  [mFullscreenTitlebarTracker removeFromParentViewController];
  [mFullscreenTitlebarTracker release];

  [super dealloc];
}

- (NSArray<NSView*>*)contentViewContents {
  return [[self.contentView.subviews copy] autorelease];
}

- (void)windowMainStateChanged {
  [[self mainChildView] ensureNextCompositeIsAtomicWithMainThreadPaint];
}

// Extending the content area into the title bar works by resizing the
// mainChildView so that it covers the titlebar.
- (void)setDrawsContentsIntoWindowFrame:(BOOL)aState {
  BOOL stateChanged = self.drawsContentsIntoWindowFrame != aState;
  [super setDrawsContentsIntoWindowFrame:aState];
  if (stateChanged && [self.delegate isKindOfClass:[WindowDelegate class]]) {
    // Hide the titlebar if we are drawing into it
    self.titlebarAppearsTransparent = self.drawsContentsIntoWindowFrame;

    // Here we extend / shrink our mainChildView. We do that by firing a resize
    // event which will cause the ChildView to be resized to the rect returned
    // by nsCocoaWindow::GetClientBounds. GetClientBounds bases its return
    // value on what we return from drawsContentsIntoWindowFrame.
    auto* windowDelegate = static_cast<WindowDelegate*>(self.delegate);
    if (nsCocoaWindow* geckoWindow = windowDelegate.geckoWidget) {
      // Re-layout our contents.
      geckoWindow->ReportSizeEvent();
    }

    // Resizing the content area causes a reflow which would send a synthesized
    // mousemove event to the old mouse position relative to the top left
    // corner of the content area. But the mouse has shifted relative to the
    // content area, so that event would have wrong position information. So
    // we'll send a mouse move event with the correct new position.
    ChildViewMouseTracker::ResendLastMouseMoveEvent();
  }
}

- (void)placeWindowButtons:(NSRect)aRect {
  if (!NSEqualRects(mWindowButtonsRect, aRect)) {
    mWindowButtonsRect = aRect;
    [self reflowTitlebarElements];
  }
}

- (NSRect)windowButtonsRect {
  return mWindowButtonsRect;
}

// Returning YES here makes the setShowsToolbarButton method work even though
// the window doesn't contain an NSToolbar.
- (BOOL)_hasToolbar {
  return YES;
}

// Dispatch a toolbar pill button clicked message to Gecko.
- (void)_toolbarPillButtonClicked:(id)sender {
  NS_OBJC_BEGIN_TRY_IGNORE_BLOCK;

  RollUpPopups();

  if ([self.delegate isKindOfClass:[WindowDelegate class]]) {
    auto* windowDelegate = static_cast<WindowDelegate*>(self.delegate);
    nsCocoaWindow* geckoWindow = windowDelegate.geckoWidget;
    if (!geckoWindow) {
      return;
    }

    if (nsIWidgetListener* listener = geckoWindow->GetWidgetListener()) {
      listener->OSToolbarButtonPressed();
    }
  }

  NS_OBJC_END_TRY_IGNORE_BLOCK;
}

// Retain and release "self" to avoid crashes when our widget (and its native
// window) is closed as a result of processing a key equivalent (e.g.
// Command+w or Command+q).  This workaround is only needed for a window
// that can become key.
- (BOOL)performKeyEquivalent:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NSWindow* nativeWindow = [self retain];
  BOOL retval = [super performKeyEquivalent:theEvent];
  [nativeWindow release];
  return retval;

  NS_OBJC_END_TRY_BLOCK_RETURN(NO);
}

- (void)sendEvent:(NSEvent*)anEvent {
  if (MaybeDropEventForModalWindow(anEvent, self.delegate)) {
    return;
  }
  [super sendEvent:anEvent];
}

@end

@implementation PopupWindow

- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSUInteger)styleMask
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)deferCreation {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  mIsContextMenu = false;
  return [super initWithContentRect:contentRect
                          styleMask:styleMask
                            backing:bufferingType
                              defer:deferCreation];

  NS_OBJC_END_TRY_BLOCK_RETURN(nil);
}

// Override the private API _backdropBleedAmount. This determines how much the
// desktop wallpaper contributes to the vibrancy backdrop.
// Return 0 in order to match what the system does for sheet windows and
// _NSPopoverWindows.
- (CGFloat)_backdropBleedAmount {
  return 0.0;
}

// Override the private API shadowOptions.
// The constants below were found in AppKit's implementations of the
// shadowOptions method on the various window types.
static const NSUInteger kWindowShadowOptionsNoShadow = 0;
static const NSUInteger kWindowShadowOptionsMenu = 2;
static const NSUInteger kWindowShadowOptionsTooltip = 4;

- (NSDictionary*)shadowParameters {
  NSDictionary* parent = [super shadowParameters];
  // NSLog(@"%@", parent);
  if (self.shadowStyle != WindowShadow::Panel) {
    return parent;
  }
  NSMutableDictionary* copy = [parent mutableCopy];
  for (auto* key : {@"com.apple.WindowShadowRimDensityActive",
                    @"com.apple.WindowShadowRimDensityInactive"}) {
    if ([parent objectForKey:key] != nil) {
      [copy setValue:@(0) forKey:key];
    }
  }
  return copy;
}

- (NSUInteger)shadowOptions {
  if (!self.hasShadow) {
    return kWindowShadowOptionsNoShadow;
  }

  switch (self.shadowStyle) {
    case WindowShadow::None:
      return kWindowShadowOptionsNoShadow;

    case WindowShadow::Menu:
    case WindowShadow::Panel:
      return kWindowShadowOptionsMenu;

    case WindowShadow::Tooltip:
      return kWindowShadowOptionsTooltip;
  }
}

- (BOOL)isContextMenu {
  return mIsContextMenu;
}

- (void)setIsContextMenu:(BOOL)flag {
  mIsContextMenu = flag;
}

- (BOOL)canBecomeMainWindow {
  // This is overriden because the default is 'yes' when a titlebar is present.
  return NO;
}

@end

// According to Apple's docs on [NSWindow canBecomeKeyWindow] and [NSWindow
// canBecomeMainWindow], windows without a title bar or resize bar can't (by
// default) become key or main.  But if a window can't become key, it can't
// accept keyboard input (bmo bug 393250).  And it should also be possible for
// an otherwise "ordinary" window to become main.  We need to override these
// two methods to make this happen.
@implementation BorderlessWindow

- (BOOL)canBecomeKeyWindow {
  return YES;
}

- (void)sendEvent:(NSEvent*)anEvent {
  if (MaybeDropEventForModalWindow(anEvent, self.delegate)) {
    return;
  }

  [super sendEvent:anEvent];
}

// Apple's doc on this method says that the NSWindow class's default is not to
// become main if the window isn't "visible" -- so we should replicate that
// behavior here.  As best I can tell, the [NSWindow isVisible] method is an
// accurate test of what Apple means by "visibility".
- (BOOL)canBecomeMainWindow {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  return self.isVisible;

  NS_OBJC_END_TRY_BLOCK_RETURN(NO);
}

// Retain and release "self" to avoid crashes when our widget (and its native
// window) is closed as a result of processing a key equivalent (e.g.
// Command+w or Command+q).  This workaround is only needed for a window
// that can become key.
- (BOOL)performKeyEquivalent:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  NSWindow* nativeWindow = [self retain];
  BOOL retval = [super performKeyEquivalent:theEvent];
  [nativeWindow release];
  return retval;

  NS_OBJC_END_TRY_BLOCK_RETURN(NO);
}

@end
