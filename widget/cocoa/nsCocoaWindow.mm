/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCocoaWindow.h"

#include "NativeKeyBindings.h"
#include "ScreenHelperCocoa.h"
#include "TextInputHandler.h"
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
#include "nsIXULWindow.h"
#include "nsToolkit.h"
#include "nsIDOMWindow.h"
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
#include "VibrancyManager.h"

#include "gfxPlatform.h"
#include "gfxPrefs.h"
#include "qcms.h"

#include "mozilla/AutoRestore.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/Preferences.h"
#include "mozilla/PresShell.h"
#include <algorithm>

namespace mozilla {
namespace layers {
class LayerManager;
}  // namespace layers
}  // namespace mozilla
using namespace mozilla::layers;
using namespace mozilla::widget;
using namespace mozilla;

int32_t gXULModalLevel = 0;

// In principle there should be only one app-modal window at any given time.
// But sometimes, despite our best efforts, another window appears above the
// current app-modal window.  So we need to keep a linked list of app-modal
// windows.  (A non-sheet window that appears above an app-modal window is
// also made app-modal.)  See nsCocoaWindow::SetModal().
nsCocoaWindowList* gGeckoAppModalWindowList = NULL;

// defined in nsMenuBarX.mm
extern NSMenu* sApplicationMenu;  // Application menu shared by all menubars

// defined in nsChildView.mm
extern BOOL gSomeMenuBarPainted;

#if !defined(MAC_OS_X_VERSION_10_9) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_9

enum NSWindowOcclusionState { NSWindowOcclusionStateVisible = 0x1 << 1 };

@interface NSWindow (OcclusionState)
- (NSWindowOcclusionState)occlusionState;
@end

#endif

#if !defined(MAC_OS_X_VERSION_10_10) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_10

enum NSWindowTitleVisibility { NSWindowTitleVisible = 0, NSWindowTitleHidden = 1 };

@interface NSWindow (TitleVisibility)
- (void)setTitleVisibility:(NSWindowTitleVisibility)visibility;
- (void)setTitlebarAppearsTransparent:(BOOL)isTitlebarTransparent;
@end

#endif

#if !defined(MAC_OS_X_VERSION_10_12) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12

@interface NSWindow (AutomaticWindowTabbing)
+ (void)setAllowsAutomaticWindowTabbing:(BOOL)allow;
@end

#endif

extern "C" {
// CGSPrivate.h
typedef NSInteger CGSConnection;
typedef NSInteger CGSWindow;
typedef NSUInteger CGSWindowFilterRef;
extern CGSConnection _CGSDefaultConnection(void);
extern CGError CGSSetWindowShadowAndRimParameters(const CGSConnection cid, CGSWindow wid,
                                                  float standardDeviation, float density,
                                                  int offsetX, int offsetY, unsigned int flags);
extern CGError CGSSetWindowBackgroundBlurRadius(CGSConnection cid, CGSWindow wid, NSUInteger blur);
extern CGError CGSSetWindowTransform(CGSConnection cid, CGSWindow wid, CGAffineTransform transform);
}

#define NS_APPSHELLSERVICE_CONTRACTID "@mozilla.org/appshell/appShellService;1"

NS_IMPL_ISUPPORTS_INHERITED(nsCocoaWindow, Inherited, nsPIWidgetCocoa)

// A note on testing to see if your object is a sheet...
// |mWindowType == eWindowType_sheet| is true if your gecko nsIWidget is a sheet
// widget - whether or not the sheet is showing. |[mWindow isSheet]| will return
// true *only when the sheet is actually showing*. Choose your test wisely.

static void RollUpPopups() {
  nsIRollupListener* rollupListener = nsBaseWidget::GetActiveRollupListener();
  NS_ENSURE_TRUE_VOID(rollupListener);
  nsCOMPtr<nsIWidget> rollupWidget = rollupListener->GetRollupWidget();
  if (!rollupWidget) return;
  rollupListener->Rollup(0, true, nullptr, nullptr);
}

nsCocoaWindow::nsCocoaWindow()
    : mParent(nullptr),
      mAncestorLink(nullptr),
      mWindow(nil),
      mDelegate(nil),
      mSheetWindowParent(nil),
      mPopupContentView(nil),
      mFullscreenTransitionAnimation(nil),
      mShadowStyle(NS_STYLE_WINDOW_SHADOW_DEFAULT),
      mBackingScaleFactor(0.0),
      mAnimationType(nsIWidget::eGenericWindowAnimation),
      mWindowMadeHere(false),
      mSheetNeedsShow(false),
      mInFullScreenMode(false),
      mInFullScreenTransition(false),
      mModal(false),
      mFakeModal(false),
      mSupportsNativeFullScreen(false),
      mInNativeFullScreenMode(false),
      mIsAnimationSuppressed(false),
      mInReportMoveEvent(false),
      mInResize(false),
      mWindowTransformIsIdentity(true),
      mNumModalDescendents(0),
      mWindowAnimationBehavior(NSWindowAnimationBehaviorDefault) {
  if ([NSWindow respondsToSelector:@selector(setAllowsAutomaticWindowTabbing:)]) {
    // Disable automatic tabbing on 10.12. We need to do this before we
    // orderFront any of our windows.
    [NSWindow setAllowsAutomaticWindowTabbing:NO];
  }
}

void nsCocoaWindow::DestroyNativeWindow() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mWindow) return;

  [mWindow releaseJSObjects];
  // We want to unhook the delegate here because we don't want events
  // sent to it after this object has been destroyed.
  [mWindow setDelegate:nil];
  [mWindow close];
  mWindow = nil;
  [mDelegate autorelease];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsCocoaWindow::~nsCocoaWindow() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Notify the children that we're gone.  Popup windows (e.g. tooltips) can
  // have nsChildView children.  'kid' is an nsChildView object if and only if
  // its 'type' is 'eWindowType_child'.
  // childView->ResetParent() can change our list of children while it's
  // being iterated, so the way we iterate the list must allow for this.
  for (nsIWidget* kid = mLastChild; kid;) {
    nsWindowType kidType = kid->WindowType();
    if (kidType == eWindowType_child) {
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
    DestroyNativeWindow();
  }

  NS_IF_RELEASE(mPopupContentView);

  // Deal with the possiblity that we're being destroyed while running modal.
  if (mModal) {
    NS_WARNING("Widget destroyed while running modal!");
    --gXULModalLevel;
    NS_ASSERTION(gXULModalLevel >= 0, "Weirdness setting modality!");
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Find the screen that overlaps aRect the most,
// if none are found default to the mainScreen.
static NSScreen* FindTargetScreenForRect(const DesktopIntRect& aRect) {
  NSScreen* targetScreen = [NSScreen mainScreen];
  NSEnumerator* screenEnum = [[NSScreen screens] objectEnumerator];
  int largestIntersectArea = 0;
  while (NSScreen* screen = [screenEnum nextObject]) {
    DesktopIntRect screenRect = nsCocoaUtils::CocoaRectToGeckoRect([screen visibleFrame]);
    screenRect = screenRect.Intersect(aRect);
    int area = screenRect.width * screenRect.height;
    if (area > largestIntersectArea) {
      largestIntersectArea = area;
      targetScreen = screen;
    }
  }
  return targetScreen;
}

// fits the rect to the screen that contains the largest area of it,
// or to aScreen if a screen is passed in
// NB: this operates with aRect in desktop pixels
static void FitRectToVisibleAreaForScreen(DesktopIntRect& aRect, NSScreen* aScreen) {
  if (!aScreen) {
    aScreen = FindTargetScreenForRect(aRect);
  }

  DesktopIntRect screenBounds = nsCocoaUtils::CocoaRectToGeckoRect([aScreen visibleFrame]);

  if (aRect.width > screenBounds.width) {
    aRect.width = screenBounds.width;
  }
  if (aRect.height > screenBounds.height) {
    aRect.height = screenBounds.height;
  }

  if (aRect.x - screenBounds.x + aRect.width > screenBounds.width) {
    aRect.x += screenBounds.width - (aRect.x - screenBounds.x + aRect.width);
  }
  if (aRect.y - screenBounds.y + aRect.height > screenBounds.height) {
    aRect.y += screenBounds.height - (aRect.y - screenBounds.y + aRect.height);
  }

  // If the left/top edge of the window is off the screen in either direction,
  // then set the window to start at the left/top edge of the screen.
  if (aRect.x < screenBounds.x || aRect.x > (screenBounds.x + screenBounds.width)) {
    aRect.x = screenBounds.x;
  }
  if (aRect.y < screenBounds.y || aRect.y > (screenBounds.y + screenBounds.height)) {
    aRect.y = screenBounds.y;
  }
}

// Some applications use native popup windows
// (native context menus, native tooltips)
static bool UseNativePopupWindows() {
#ifdef MOZ_USE_NATIVE_POPUP_WINDOWS
  return true;
#else
  return false;
#endif /* MOZ_USE_NATIVE_POPUP_WINDOWS */
}

// aRect here is specified in desktop pixels
nsresult nsCocoaWindow::Create(nsIWidget* aParent, nsNativeWidget aNativeParent,
                               const DesktopIntRect& aRect, nsWidgetInitData* aInitData) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Because the hidden window is created outside of an event loop,
  // we have to provide an autorelease pool (see bug 559075).
  nsAutoreleasePool localPool;

  DesktopIntRect newBounds = aRect;
  FitRectToVisibleAreaForScreen(newBounds, nullptr);

  // Set defaults which can be overriden from aInitData in BaseCreate
  mWindowType = eWindowType_toplevel;
  mBorderStyle = eBorderStyle_default;

  // Ensure that the toolkit is created.
  nsToolkit::GetToolkit();

  Inherited::BaseCreate(aParent, aInitData);

  mParent = aParent;
  mAncestorLink = aParent;

  // Applications that use native popups don't want us to create popup windows.
  if ((mWindowType == eWindowType_popup) && UseNativePopupWindows()) return NS_OK;

  nsresult rv =
      CreateNativeWindow(nsCocoaUtils::GeckoRectToCocoaRect(newBounds), mBorderStyle, false);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mWindowType == eWindowType_popup) {
    if (aInitData->mMouseTransparent) {
      [mWindow setIgnoresMouseEvents:YES];
    }
    // now we can convert newBounds to device pixels for the window we created,
    // as the child view expects a rect expressed in the dev pix of its parent
    LayoutDeviceIntRect devRect = RoundedToInt(newBounds * GetDesktopToDeviceScale());
    return CreatePopupContentView(devRect, aInitData);
  }

  mIsAnimationSuppressed = aInitData->mIsAnimationSuppressed;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult nsCocoaWindow::Create(nsIWidget* aParent, nsNativeWidget aNativeParent,
                               const LayoutDeviceIntRect& aRect, nsWidgetInitData* aInitData) {
  DesktopIntRect desktopRect = RoundedToInt(aRect / GetDesktopToDeviceScale());
  return Create(aParent, aNativeParent, desktopRect, aInitData);
}

static unsigned int WindowMaskForBorderStyle(nsBorderStyle aBorderStyle) {
  bool allOrDefault = (aBorderStyle == eBorderStyle_all || aBorderStyle == eBorderStyle_default);

  /* Apple's docs on NSWindow styles say that "a window's style mask should
   * include NSTitledWindowMask if it includes any of the others [besides
   * NSBorderlessWindowMask]".  This implies that a borderless window
   * shouldn't have any other styles than NSBorderlessWindowMask.
   */
  if (!allOrDefault && !(aBorderStyle & eBorderStyle_title)) return NSBorderlessWindowMask;

  unsigned int mask = NSTitledWindowMask;
  if (allOrDefault || aBorderStyle & eBorderStyle_close) mask |= NSClosableWindowMask;
  if (allOrDefault || aBorderStyle & eBorderStyle_minimize) mask |= NSMiniaturizableWindowMask;
  if (allOrDefault || aBorderStyle & eBorderStyle_resizeh) mask |= NSResizableWindowMask;

  return mask;
}

// If aRectIsFrameRect, aRect specifies the frame rect of the new window.
// Otherwise, aRect.x/y specify the position of the window's frame relative to
// the bottom of the menubar and aRect.width/height specify the size of the
// content rect.
nsresult nsCocoaWindow::CreateNativeWindow(const NSRect& aRect, nsBorderStyle aBorderStyle,
                                           bool aRectIsFrameRect) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // We default to NSBorderlessWindowMask, add features if needed.
  unsigned int features = NSBorderlessWindowMask;

  // Configure the window we will create based on the window type.
  switch (mWindowType) {
    case eWindowType_invisible:
    case eWindowType_child:
    case eWindowType_plugin:
      break;
    case eWindowType_popup:
      if (aBorderStyle != eBorderStyle_default && mBorderStyle & eBorderStyle_title) {
        features |= NSTitledWindowMask;
        if (aBorderStyle & eBorderStyle_close) {
          features |= NSClosableWindowMask;
        }
      }
      break;
    case eWindowType_toplevel:
    case eWindowType_dialog:
      features = WindowMaskForBorderStyle(aBorderStyle);
      break;
    case eWindowType_sheet:
      if (mParent->WindowType() != eWindowType_invisible && aBorderStyle & eBorderStyle_resizeh) {
        features = NSResizableWindowMask;
      } else {
        features = NSMiniaturizableWindowMask;
      }
      features |= NSTitledWindowMask;
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
    // Compensate for difference between frame and content area height (e.g. title bar).
    NSRect newWindowFrame = [NSWindow frameRectForContentRect:aRect styleMask:features];

    contentRect = aRect;
    contentRect.origin.y -= (newWindowFrame.size.height - aRect.size.height);

    if (mWindowType != eWindowType_popup) contentRect.origin.y -= [[NSApp mainMenu] menuBarHeight];
  }

  // NSLog(@"Top-level window being created at Cocoa rect: %f, %f, %f, %f\n",
  //       rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);

  Class windowClass = [BaseWindow class];
  // If we have a titlebar on a top-level window, we want to be able to control the
  // titlebar color (for unified windows), so use the special ToolbarWindow class.
  // Note that we need to check the window type because we mark sheets as
  // having titlebars.
  if ((mWindowType == eWindowType_toplevel || mWindowType == eWindowType_dialog) &&
      (features & NSTitledWindowMask))
    windowClass = [ToolbarWindow class];
  // If we're a popup window we need to use the PopupWindow class.
  else if (mWindowType == eWindowType_popup)
    windowClass = [PopupWindow class];
  // If we're a non-popup borderless window we need to use the
  // BorderlessWindow class.
  else if (features == NSBorderlessWindowMask)
    windowClass = [BorderlessWindow class];

  // Create the window
  mWindow = [[windowClass alloc] initWithContentRect:contentRect
                                           styleMask:features
                                             backing:NSBackingStoreBuffered
                                               defer:YES];

  // Make sure that window titles don't leak to disk in private browsing mode
  // due to macOS' resume feature.
  [mWindow setRestorable:NO];
  [mWindow disableSnapshotRestoration];

  // setup our notification delegate. Note that setDelegate: does NOT retain.
  mDelegate = [[WindowDelegate alloc] initWithGeckoWindow:this];
  [mWindow setDelegate:mDelegate];

  // Make sure that the content rect we gave has been honored.
  NSRect wantedFrame = [mWindow frameRectForChildViewRect:contentRect];
  if (!NSEqualRects([mWindow frame], wantedFrame)) {
    // This can happen when the window is not on the primary screen.
    [mWindow setFrame:wantedFrame display:NO];
  }
  UpdateBounds();

  if (mWindowType == eWindowType_invisible) {
    [mWindow setLevel:kCGDesktopWindowLevelKey];
  }

  if (mWindowType == eWindowType_popup) {
    SetPopupWindowLevel();
    [mWindow setBackgroundColor:[NSColor clearColor]];
    [mWindow setOpaque:NO];
  } else {
    // Make sure that regular windows are opaque from the start, so that
    // nsChildView::WidgetTypeSupportsAcceleration returns true for them.
    [mWindow setOpaque:YES];
  }

  [mWindow setContentMinSize:NSMakeSize(60, 60)];
  [mWindow disableCursorRects];

  // Make sure the window starts out not draggable by the background.
  // We will turn it on as necessary.
  [mWindow setMovableByWindowBackground:NO];

  [[WindowDataMap sharedWindowDataMap] ensureDataForWindow:mWindow];
  mWindowMadeHere = true;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

nsresult nsCocoaWindow::CreatePopupContentView(const LayoutDeviceIntRect& aRect,
                                               nsWidgetInitData* aInitData) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // We need to make our content view a ChildView.
  mPopupContentView = new nsChildView();
  if (!mPopupContentView) return NS_ERROR_FAILURE;

  NS_ADDREF(mPopupContentView);

  nsIWidget* thisAsWidget = static_cast<nsIWidget*>(this);
  nsresult rv = mPopupContentView->Create(thisAsWidget, nullptr, aRect, aInitData);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  NSView* contentView = [mWindow contentView];
  ChildView* childView = (ChildView*)mPopupContentView->GetNativeData(NS_NATIVE_WIDGET);
  [childView setFrame:[contentView bounds]];
  [childView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [contentView addSubview:childView];

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void nsCocoaWindow::Destroy() {
  if (mOnDestroyCalled) return;
  mOnDestroyCalled = true;

  // SetFakeModal(true) is called for non-modal window opened by modal window.
  // On Cocoa, it needs corresponding SetFakeModal(false) on destroy to restore
  // ancestor windows' state.
  if (mFakeModal) {
    SetFakeModal(false);
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

  if (mInFullScreenMode) {
    // On Lion we don't have to mess with the OS chrome when in Full Screen
    // mode.  But we do have to destroy the native window here (and not wait
    // for that to happen in our destructor).  We don't switch away from the
    // native window's space until the window is destroyed, and otherwise this
    // might not happen for several seconds (because at least one object
    // holding a reference to ourselves is usually waiting to be garbage-
    // collected).  See bug 757618.
    if (mInNativeFullScreenMode) {
      DestroyNativeWindow();
    } else if (mWindow) {
      nsCocoaUtils::HideOSChromeOnScreen(false);
    }
  }
}

nsIWidget* nsCocoaWindow::GetSheetWindowParent(void) {
  if (mWindowType != eWindowType_sheet) return nullptr;
  nsCocoaWindow* parent = static_cast<nsCocoaWindow*>(mParent);
  while (parent && (parent->mWindowType == eWindowType_sheet))
    parent = static_cast<nsCocoaWindow*>(parent->mParent);
  return parent;
}

void* nsCocoaWindow::GetNativeData(uint32_t aDataType) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSNULL;

  void* retVal = nullptr;

  switch (aDataType) {
    // to emulate how windows works, we always have to return a NSView
    // for NS_NATIVE_WIDGET
    case NS_NATIVE_WIDGET:
    case NS_NATIVE_DISPLAY:
      retVal = [mWindow contentView];
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
      NSView* view = mWindow ? [mWindow contentView] : nil;
      if (view) {
        retVal = [view inputContext];
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

  NS_OBJC_END_TRY_ABORT_BLOCK_NSNULL;
}

bool nsCocoaWindow::IsVisible() const {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  return (mWindow && ([mWindow isVisibleOrBeingShown] || mSheetNeedsShow));

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(false);
}

void nsCocoaWindow::SetModal(bool aState) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mWindow) return;

  // This is used during startup (outside the event loop) when creating
  // the add-ons compatibility checking dialog and the profile manager UI;
  // therefore, it needs to provide an autorelease pool to avoid cocoa
  // objects leaking.
  nsAutoreleasePool localPool;

  mModal = aState;
  nsCocoaWindow* ancestor = static_cast<nsCocoaWindow*>(mAncestorLink);
  if (aState) {
    ++gXULModalLevel;
    // When a non-sheet window gets "set modal", make the window(s) that it
    // appears over behave as they should.  We can't rely on native methods to
    // do this, for the following reason:  The OS runs modal non-sheet windows
    // in an event loop (using [NSApplication runModalForWindow:] or similar
    // methods) that's incompatible with the modal event loop in nsXULWindow::
    // ShowModal() (each of these event loops is "exclusive", and can't run at
    // the same time as other (similar) event loops).
    if (mWindowType != eWindowType_sheet) {
      while (ancestor) {
        if (ancestor->mNumModalDescendents++ == 0) {
          NSWindow* aWindow = ancestor->GetCocoaWindow();
          if (ancestor->mWindowType != eWindowType_invisible) {
            [[aWindow standardWindowButton:NSWindowCloseButton] setEnabled:NO];
            [[aWindow standardWindowButton:NSWindowMiniaturizeButton] setEnabled:NO];
            [[aWindow standardWindowButton:NSWindowZoomButton] setEnabled:NO];
          }
        }
        ancestor = static_cast<nsCocoaWindow*>(ancestor->mParent);
      }
      [mWindow setLevel:NSModalPanelWindowLevel];
      nsCocoaWindowList* windowList = new nsCocoaWindowList;
      if (windowList) {
        windowList->window = this;  // Don't ADDREF
        windowList->prev = gGeckoAppModalWindowList;
        gGeckoAppModalWindowList = windowList;
      }
    }
  } else {
    --gXULModalLevel;
    NS_ASSERTION(gXULModalLevel >= 0, "Mismatched call to nsCocoaWindow::SetModal(false)!");
    if (mWindowType != eWindowType_sheet) {
      while (ancestor) {
        if (--ancestor->mNumModalDescendents == 0) {
          NSWindow* aWindow = ancestor->GetCocoaWindow();
          if (ancestor->mWindowType != eWindowType_invisible) {
            [[aWindow standardWindowButton:NSWindowCloseButton] setEnabled:YES];
            [[aWindow standardWindowButton:NSWindowMiniaturizeButton] setEnabled:YES];
            [[aWindow standardWindowButton:NSWindowZoomButton] setEnabled:YES];
          }
        }
        NS_ASSERTION(ancestor->mNumModalDescendents >= 0, "Widget hierarchy changed while modal!");
        ancestor = static_cast<nsCocoaWindow*>(ancestor->mParent);
      }
      if (gGeckoAppModalWindowList) {
        NS_ASSERTION(gGeckoAppModalWindowList->window == this,
                     "Widget hierarchy changed while modal!");
        nsCocoaWindowList* saved = gGeckoAppModalWindowList;
        gGeckoAppModalWindowList = gGeckoAppModalWindowList->prev;
        delete saved;  // "window" not ADDREFed
      }
      if (mWindowType == eWindowType_popup)
        SetPopupWindowLevel();
      else
        [mWindow setLevel:NSNormalWindowLevel];
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsCocoaWindow::SetFakeModal(bool aState) {
  mFakeModal = aState;
  SetModal(aState);
}

bool nsCocoaWindow::IsRunningAppModal() { return [NSApp _isRunningAppModal]; }

// Hide or show this window
void nsCocoaWindow::Show(bool bState) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mWindow) return;

  // We need to re-execute sometimes in order to bring already-visible
  // windows forward.
  if (!mSheetNeedsShow && !bState && ![mWindow isVisible]) return;

  // Protect against re-entering.
  if (bState && [mWindow isBeingShown]) return;

  [mWindow setBeingShown:bState];

  nsIWidget* parentWidget = mParent;
  nsCOMPtr<nsPIWidgetCocoa> piParentWidget(do_QueryInterface(parentWidget));
  NSWindow* nativeParentWindow =
      (parentWidget) ? (NSWindow*)parentWidget->GetNativeData(NS_NATIVE_WINDOW) : nil;

  if (bState && !mBounds.IsEmpty()) {
    // Don't try to show a popup when the parent isn't visible or is minimized.
    if (mWindowType == eWindowType_popup && nativeParentWindow) {
      if (![nativeParentWindow isVisible] || [nativeParentWindow isMiniaturized]) {
        return;
      }
    }

    if (mPopupContentView) {
      // Ensure our content view is visible. We never need to hide it.
      mPopupContentView->Show(true);
    }

    if (mWindowType == eWindowType_sheet) {
      // bail if no parent window (its basically what we do in Carbon)
      if (!nativeParentWindow || !piParentWidget) return;

      NSWindow* topNonSheetWindow = nativeParentWindow;

      // If this sheet is the child of another sheet, hide the parent so that
      // this sheet can be displayed. Leave the parent mSheetNeedsShow alone,
      // that is only used to handle sibling sheet contention. The parent will
      // return once there are no more child sheets.
      bool parentIsSheet = false;
      if (NS_SUCCEEDED(piParentWidget->GetIsSheet(&parentIsSheet)) && parentIsSheet) {
        piParentWidget->GetSheetWindowParent(&topNonSheetWindow);
        [NSApp endSheet:nativeParentWindow];
      }

      nsCOMPtr<nsIWidget> sheetShown;
      if (NS_SUCCEEDED(piParentWidget->GetChildSheet(true, getter_AddRefs(sheetShown))) &&
          (!sheetShown || sheetShown == this)) {
        // If this sheet is already the sheet actually being shown, don't
        // tell it to show again. Otherwise the number of calls to
        // [NSApp beginSheet...] won't match up with [NSApp endSheet...].
        if (![mWindow isVisible]) {
          mSheetNeedsShow = false;
          mSheetWindowParent = topNonSheetWindow;
          // Only set contextInfo if our parent isn't a sheet.
          NSWindow* contextInfo = parentIsSheet ? nil : mSheetWindowParent;
          [TopLevelWindowData deactivateInWindow:mSheetWindowParent];
          [NSApp beginSheet:mWindow
              modalForWindow:mSheetWindowParent
               modalDelegate:mDelegate
              didEndSelector:@selector(didEndSheet:returnCode:contextInfo:)
                 contextInfo:contextInfo];
          [TopLevelWindowData activateInWindow:mWindow];
          SendSetZLevelEvent();
        }
      } else {
        // A sibling of this sheet is active, don't show this sheet yet.
        // When the active sheet hides, its brothers and sisters that have
        // mSheetNeedsShow set will have their opportunities to display.
        mSheetNeedsShow = true;
      }
    } else if (mWindowType == eWindowType_popup) {
      // For reasons that aren't yet clear, calls to [NSWindow orderFront:] or
      // [NSWindow makeKeyAndOrderFront:] can sometimes trigger "Error (1000)
      // creating CGSWindow", which in turn triggers an internal inconsistency
      // NSException.  These errors shouldn't be fatal.  So we need to wrap
      // calls to ...orderFront: in TRY blocks.  See bmo bug 470864.
      NS_OBJC_BEGIN_TRY_ABORT_BLOCK;
      [[mWindow contentView] setNeedsDisplay:YES];
      [mWindow orderFront:nil];
      NS_OBJC_END_TRY_ABORT_BLOCK;
      SendSetZLevelEvent();
      AdjustWindowShadow();
      SetWindowBackgroundBlur();
      // If our popup window is a non-native context menu, tell the OS (and
      // other programs) that a menu has opened.  This is how the OS knows to
      // close other programs' context menus when ours open.
      if ([mWindow isKindOfClass:[PopupWindow class]] && [(PopupWindow*)mWindow isContextMenu]) {
        [[NSDistributedNotificationCenter defaultCenter]
            postNotificationName:@"com.apple.HIToolbox.beginMenuTrackingNotification"
                          object:@"org.mozilla.gecko.PopupWindow"];
      }

      // If a parent window was supplied and this is a popup at the parent
      // level, set its child window. This will cause the child window to
      // appear above the parent and move when the parent does. Setting this
      // needs to happen after the _setWindowNumber calls above, otherwise the
      // window doesn't focus properly.
      if (nativeParentWindow && mPopupLevel == ePopupLevelParent)
        [nativeParentWindow addChildWindow:mWindow ordered:NSWindowAbove];
    } else {
      NS_OBJC_BEGIN_TRY_ABORT_BLOCK;
      if (mWindowType == eWindowType_toplevel &&
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
              MOZ_ASSERT_UNREACHABLE("unexpected mAnimationType value");
              // fall through
            case nsIWidget::eGenericWindowAnimation:
              behavior = NSWindowAnimationBehaviorDefault;
              break;
          }
        }
        [mWindow setAnimationBehavior:behavior];
        mWindowAnimationBehavior = behavior;
      }
      [mWindow makeKeyAndOrderFront:nil];
      NS_OBJC_END_TRY_ABORT_BLOCK;
      SendSetZLevelEvent();
    }
  } else {
    // roll up any popups if a top-level window is going away
    if (mWindowType == eWindowType_toplevel || mWindowType == eWindowType_dialog) RollUpPopups();

    // now get rid of the window/sheet
    if (mWindowType == eWindowType_sheet) {
      if (mSheetNeedsShow) {
        // This is an attempt to hide a sheet that never had a chance to
        // be shown. There's nothing to do other than make sure that it
        // won't show.
        mSheetNeedsShow = false;
      } else {
        // get sheet's parent *before* hiding the sheet (which breaks the linkage)
        NSWindow* sheetParent = mSheetWindowParent;

        // hide the sheet
        [NSApp endSheet:mWindow];

        [TopLevelWindowData deactivateInWindow:mWindow];

        nsCOMPtr<nsIWidget> siblingSheetToShow;
        bool parentIsSheet = false;

        if (nativeParentWindow && piParentWidget &&
            NS_SUCCEEDED(
                piParentWidget->GetChildSheet(false, getter_AddRefs(siblingSheetToShow))) &&
            siblingSheetToShow) {
          // First, give sibling sheets an opportunity to show.
          siblingSheetToShow->Show(true);
        } else if (nativeParentWindow && piParentWidget &&
                   NS_SUCCEEDED(piParentWidget->GetIsSheet(&parentIsSheet)) && parentIsSheet) {
          // Only set contextInfo if the parent of the parent sheet we're about
          // to restore isn't itself a sheet.
          NSWindow* contextInfo = sheetParent;
          nsIWidget* grandparentWidget = nil;
          if (NS_SUCCEEDED(piParentWidget->GetRealParent(&grandparentWidget)) &&
              grandparentWidget) {
            nsCOMPtr<nsPIWidgetCocoa> piGrandparentWidget(do_QueryInterface(grandparentWidget));
            bool grandparentIsSheet = false;
            if (piGrandparentWidget &&
                NS_SUCCEEDED(piGrandparentWidget->GetIsSheet(&grandparentIsSheet)) &&
                grandparentIsSheet) {
              contextInfo = nil;
            }
          }
          // If there are no sibling sheets, but the parent is a sheet, restore
          // it.  It wasn't sent any deactivate events when it was hidden, so
          // don't call through Show, just let the OS put it back up.
          [NSApp beginSheet:nativeParentWindow
              modalForWindow:sheetParent
               modalDelegate:[nativeParentWindow delegate]
              didEndSelector:@selector(didEndSheet:returnCode:contextInfo:)
                 contextInfo:contextInfo];
        } else {
          // Sheet, that was hard.  No more siblings or parents, going back
          // to a real window.
          NS_OBJC_BEGIN_TRY_ABORT_BLOCK;
          [sheetParent makeKeyAndOrderFront:nil];
          NS_OBJC_END_TRY_ABORT_BLOCK;
        }
        SendSetZLevelEvent();
      }
    } else {
      // If the window is a popup window with a parent window we need to
      // unhook it here before ordering it out. When you order out the child
      // of a window it hides the parent window.
      if (mWindowType == eWindowType_popup && nativeParentWindow)
        [nativeParentWindow removeChildWindow:mWindow];

      [mWindow orderOut:nil];

      // If our popup window is a non-native context menu, tell the OS (and
      // other programs) that a menu has closed.
      if ([mWindow isKindOfClass:[PopupWindow class]] && [(PopupWindow*)mWindow isContextMenu]) {
        [[NSDistributedNotificationCenter defaultCenter]
            postNotificationName:@"com.apple.HIToolbox.endMenuTrackingNotification"
                          object:@"org.mozilla.gecko.PopupWindow"];
      }
    }
  }

  [mWindow setBeingShown:NO];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

struct ShadowParams {
  float standardDeviation;
  float density;
  int offsetX;
  int offsetY;
  unsigned int flags;
};

// These numbers have been determined by looking at the results of
// CGSGetWindowShadowAndRimParameters for native window types.
static const ShadowParams kWindowShadowParametersPreYosemite[] = {
    {0.0f, 0.0f, 0, 0, 0},       // none
    {8.0f, 0.5f, 0, 6, 1},       // default
    {10.0f, 0.44f, 0, 10, 512},  // menu
    {8.0f, 0.5f, 0, 6, 1},       // tooltip
    {4.0f, 0.6f, 0, 4, 512}      // sheet
};

static const ShadowParams kWindowShadowParametersPostYosemite[] = {
    {0.0f, 0.0f, 0, 0, 0},       // none
    {8.0f, 0.5f, 0, 6, 1},       // default
    {9.882353f, 0.3f, 0, 4, 0},  // menu
    {3.294118f, 0.2f, 0, 1, 0},  // tooltip
    {9.882353f, 0.3f, 0, 4, 0}   // sheet
};

// This method will adjust the window shadow style for popup windows after
// they have been made visible. Before they're visible, their window number
// might be -1, which is not useful.
// We won't attempt to change the shadow for windows that can acquire key state
// since OS X will reset the shadow whenever that happens.
void nsCocoaWindow::AdjustWindowShadow() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mWindow || ![mWindow isVisible] || ![mWindow hasShadow] || [mWindow canBecomeKeyWindow] ||
      [mWindow windowNumber] == -1)
    return;

  const ShadowParams& params = nsCocoaFeatures::OnYosemiteOrLater()
                                   ? kWindowShadowParametersPostYosemite[mShadowStyle]
                                   : kWindowShadowParametersPreYosemite[mShadowStyle];
  CGSConnection cid = _CGSDefaultConnection();
  CGSSetWindowShadowAndRimParameters(cid, [mWindow windowNumber], params.standardDeviation,
                                     params.density, params.offsetX, params.offsetY, params.flags);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

static const NSUInteger kWindowBackgroundBlurRadius = 4;

void nsCocoaWindow::SetWindowBackgroundBlur() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mWindow || ![mWindow isVisible] || [mWindow windowNumber] == -1) return;

  // Only blur the background of menus and fake sheets.
  if (mShadowStyle != NS_STYLE_WINDOW_SHADOW_MENU && mShadowStyle != NS_STYLE_WINDOW_SHADOW_SHEET)
    return;

  CGSConnection cid = _CGSDefaultConnection();
  CGSSetWindowBackgroundBlurRadius(cid, [mWindow windowNumber], kWindowBackgroundBlurRadius);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsresult nsCocoaWindow::ConfigureChildren(const nsTArray<Configuration>& aConfigurations) {
  if (mPopupContentView) {
    mPopupContentView->ConfigureChildren(aConfigurations);
  }
  return NS_OK;
}

LayerManager* nsCocoaWindow::GetLayerManager(PLayerTransactionChild* aShadowManager,
                                             LayersBackend aBackendHint,
                                             LayerManagerPersistence aPersistence) {
  if (mPopupContentView) {
    return mPopupContentView->GetLayerManager(aShadowManager, aBackendHint, aPersistence);
  }
  return nullptr;
}

nsTransparencyMode nsCocoaWindow::GetTransparencyMode() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  return (!mWindow || [mWindow isOpaque]) ? eTransparencyOpaque : eTransparencyTransparent;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(eTransparencyOpaque);
}

// This is called from nsMenuPopupFrame when making a popup transparent, or
// from nsChildView::SetTransparencyMode for other window types.
void nsCocoaWindow::SetTransparencyMode(nsTransparencyMode aMode) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mWindow) return;

  // Transparent windows are only supported on popups.
  BOOL isTransparent = aMode == eTransparencyTransparent && mWindowType == eWindowType_popup;
  BOOL currentTransparency = ![mWindow isOpaque];
  if (isTransparent != currentTransparency) {
    [mWindow setOpaque:!isTransparent];
    [mWindow setBackgroundColor:(isTransparent ? [NSColor clearColor] : [NSColor whiteColor])];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsCocoaWindow::Enable(bool aState) {}

bool nsCocoaWindow::IsEnabled() const { return true; }

#define kWindowPositionSlop 20

void nsCocoaWindow::ConstrainPosition(bool aAllowSlop, int32_t* aX, int32_t* aY) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mWindow || ![mWindow screen]) {
    return;
  }

  nsIntRect screenBounds;

  int32_t width, height;

  NSRect frame = [mWindow frame];

  // zero size rects confuse the screen manager
  width = std::max<int32_t>(frame.size.width, 1);
  height = std::max<int32_t>(frame.size.height, 1);

  nsCOMPtr<nsIScreenManager> screenMgr = do_GetService("@mozilla.org/gfx/screenmanager;1");
  if (screenMgr) {
    nsCOMPtr<nsIScreen> screen;
    screenMgr->ScreenForRect(*aX, *aY, width, height, getter_AddRefs(screen));

    if (screen) {
      screen->GetRectDisplayPix(&(screenBounds.x), &(screenBounds.y), &(screenBounds.width),
                                &(screenBounds.height));
    }
  }

  if (aAllowSlop) {
    if (*aX < screenBounds.x - width + kWindowPositionSlop) {
      *aX = screenBounds.x - width + kWindowPositionSlop;
    } else if (*aX >= screenBounds.x + screenBounds.width - kWindowPositionSlop) {
      *aX = screenBounds.x + screenBounds.width - kWindowPositionSlop;
    }

    if (*aY < screenBounds.y - height + kWindowPositionSlop) {
      *aY = screenBounds.y - height + kWindowPositionSlop;
    } else if (*aY >= screenBounds.y + screenBounds.height - kWindowPositionSlop) {
      *aY = screenBounds.y + screenBounds.height - kWindowPositionSlop;
    }
  } else {
    if (*aX < screenBounds.x) {
      *aX = screenBounds.x;
    } else if (*aX >= screenBounds.x + screenBounds.width - width) {
      *aX = screenBounds.x + screenBounds.width - width;
    }

    if (*aY < screenBounds.y) {
      *aY = screenBounds.y;
    } else if (*aY >= screenBounds.y + screenBounds.height - height) {
      *aY = screenBounds.y + screenBounds.height - height;
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsCocoaWindow::SetSizeConstraints(const SizeConstraints& aConstraints) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Popups can be smaller than (60, 60)
  NSRect rect = (mWindowType == eWindowType_popup) ? NSZeroRect : NSMakeRect(0.0, 0.0, 60, 60);
  rect = [mWindow frameRectForChildViewRect:rect];

  CGFloat scaleFactor = BackingScaleFactor();

  SizeConstraints c = aConstraints;
  c.mMinSize.width = std::max(nsCocoaUtils::CocoaPointsToDevPixels(rect.size.width, scaleFactor),
                              c.mMinSize.width);
  c.mMinSize.height = std::max(nsCocoaUtils::CocoaPointsToDevPixels(rect.size.height, scaleFactor),
                               c.mMinSize.height);

  NSSize minSize = {nsCocoaUtils::DevPixelsToCocoaPoints(c.mMinSize.width, scaleFactor),
                    nsCocoaUtils::DevPixelsToCocoaPoints(c.mMinSize.height, scaleFactor)};
  [mWindow setMinSize:minSize];

  NSSize maxSize = {c.mMaxSize.width == NS_MAXSIZE
                        ? FLT_MAX
                        : nsCocoaUtils::DevPixelsToCocoaPoints(c.mMaxSize.width, scaleFactor),
                    c.mMaxSize.height == NS_MAXSIZE
                        ? FLT_MAX
                        : nsCocoaUtils::DevPixelsToCocoaPoints(c.mMaxSize.height, scaleFactor)};
  [mWindow setMaxSize:maxSize];

  nsBaseWidget::SetSizeConstraints(c);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Coordinates are desktop pixels
void nsCocoaWindow::Move(double aX, double aY) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mWindow) {
    return;
  }

  // The point we have is in Gecko coordinates (origin top-left). Convert
  // it to Cocoa ones (origin bottom-left).
  NSPoint coord = {static_cast<float>(aX),
                   static_cast<float>(nsCocoaUtils::FlippedScreenY(NSToIntRound(aY)))};

  NSRect frame = [mWindow frame];
  if (frame.origin.x != coord.x || frame.origin.y + frame.size.height != coord.y) {
    [mWindow setFrameTopLeftPoint:coord];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsCocoaWindow::SetSizeMode(nsSizeMode aMode) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mWindow) return;

  // mSizeMode will be updated in DispatchSizeModeEvent, which will be called
  // from a delegate method that handles the state change during one of the
  // calls below.
  nsSizeMode previousMode = mSizeMode;

  if (aMode == nsSizeMode_Normal) {
    if ([mWindow isMiniaturized])
      [mWindow deminiaturize:nil];
    else if (previousMode == nsSizeMode_Maximized && [mWindow isZoomed])
      [mWindow zoom:nil];
  } else if (aMode == nsSizeMode_Minimized) {
    if (![mWindow isMiniaturized]) [mWindow miniaturize:nil];
  } else if (aMode == nsSizeMode_Maximized) {
    if ([mWindow isMiniaturized]) [mWindow deminiaturize:nil];
    if (![mWindow isZoomed]) [mWindow zoom:nil];
  } else if (aMode == nsSizeMode_Fullscreen) {
    if (!mInFullScreenMode) MakeFullScreen(true);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsCocoaWindow::SuppressAnimation(bool aSuppress) {
  if ([mWindow respondsToSelector:@selector(setAnimationBehavior:)]) {
    if (aSuppress) {
      [mWindow setAnimationBehavior:NSWindowAnimationBehaviorNone];
    } else {
      [mWindow setAnimationBehavior:mWindowAnimationBehavior];
    }
  }
}

// This has to preserve the window's frame bounds.
// This method requires (as does the Windows impl.) that you call Resize shortly
// after calling HideWindowChrome. See bug 498835 for fixing this.
void nsCocoaWindow::HideWindowChrome(bool aShouldHide) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mWindow || !mWindowMadeHere ||
      (mWindowType != eWindowType_toplevel && mWindowType != eWindowType_dialog))
    return;

  BOOL isVisible = [mWindow isVisible];

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
  NSRect frameRect = [mWindow frame];
  DestroyNativeWindow();
  nsresult rv = CreateNativeWindow(frameRect, aShouldHide ? eBorderStyle_none : mBorderStyle, true);
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

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

class FullscreenTransitionData : public nsISupports {
 public:
  NS_DECL_ISUPPORTS

  explicit FullscreenTransitionData(NSWindow* aWindow) : mTransitionWindow(aWindow) {}

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

/* virtual */ bool nsCocoaWindow::PrepareForFullscreenTransition(nsISupports** aData) {
  nsCOMPtr<nsIScreen> widgetScreen = GetWidgetScreen();
  NSScreen* cocoaScreen = ScreenHelperCocoa::CocoaScreenForScreen(widgetScreen);

  NSWindow* win = [[NSWindow alloc] initWithContentRect:[cocoaScreen frame]
                                              styleMask:NSBorderlessWindowMask
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

/* virtual */ void nsCocoaWindow::PerformFullscreenTransition(FullscreenTransitionStage aStage,
                                                              uint16_t aDuration,
                                                              nsISupports* aData,
                                                              nsIRunnable* aCallback) {
  auto data = static_cast<FullscreenTransitionData*>(aData);
  FullscreenTransitionDelegate* delegate = [[FullscreenTransitionDelegate alloc] init];
  delegate->mWindow = this;
  // Storing already_AddRefed directly could cause static checking fail.
  delegate->mCallback = nsCOMPtr<nsIRunnable>(aCallback).forget().take();

  if (mFullscreenTransitionAnimation) {
    [mFullscreenTransitionAnimation stopAnimation];
    ReleaseFullscreenTransitionAnimation();
  }

  NSDictionary* dict = @{
    NSViewAnimationTargetKey : data->mTransitionWindow,
    NSViewAnimationEffectKey : aStage == eBeforeFullscreenToggle ? NSViewAnimationFadeInEffect
                                                                 : NSViewAnimationFadeOutEffect
  };
  mFullscreenTransitionAnimation = [[NSViewAnimation alloc] initWithViewAnimations:@[ dict ]];
  [mFullscreenTransitionAnimation setDelegate:delegate];
  [mFullscreenTransitionAnimation setDuration:aDuration / 1000.0];
  [mFullscreenTransitionAnimation startAnimation];
}

void nsCocoaWindow::WillEnterFullScreen(bool aFullScreen) {
  if (mWidgetListener) {
    mWidgetListener->FullscreenWillChange(aFullScreen);
  }
}

void nsCocoaWindow::EnteredFullScreen(bool aFullScreen, bool aNativeMode) {
  mInFullScreenTransition = false;
  bool wasInFullscreen = mInFullScreenMode;
  mInFullScreenMode = aFullScreen;
  if (aNativeMode || mInNativeFullScreenMode) {
    mInNativeFullScreenMode = aFullScreen;
  }
  DispatchSizeModeEvent();
  if (mWidgetListener && wasInFullscreen != aFullScreen) {
    mWidgetListener->FullscreenChanged(aFullScreen);
  }
}

inline bool nsCocoaWindow::ShouldToggleNativeFullscreen(bool aFullScreen,
                                                        bool aUseSystemTransition) {
  if (!mSupportsNativeFullScreen) {
    // If we cannot use native fullscreen, don't touch it.
    return false;
  }
  if (mInNativeFullScreenMode) {
    // If we are using native fullscreen, go ahead to exit it.
    return true;
  }
  if (!aUseSystemTransition) {
    // If we do not want the system fullscreen transition,
    // don't use the native fullscreen.
    return false;
  }
  // If we are using native fullscreen, we should have returned earlier.
  return aFullScreen;
}

nsresult nsCocoaWindow::MakeFullScreen(bool aFullScreen, nsIScreen* aTargetScreen) {
  return DoMakeFullScreen(aFullScreen, false);
}

nsresult nsCocoaWindow::MakeFullScreenWithNativeTransition(bool aFullScreen,
                                                           nsIScreen* aTargetScreen) {
  return DoMakeFullScreen(aFullScreen, true);
}

nsresult nsCocoaWindow::DoMakeFullScreen(bool aFullScreen, bool aUseSystemTransition) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!mWindow) {
    return NS_OK;
  }

  // We will call into MakeFullScreen redundantly when entering/exiting
  // fullscreen mode via OS X controls. When that happens we should just handle
  // it gracefully - no need to ASSERT.
  if (mInFullScreenMode == aFullScreen) {
    return NS_OK;
  }

  mInFullScreenTransition = true;

  if (ShouldToggleNativeFullscreen(aFullScreen, aUseSystemTransition)) {
    // If we're using native fullscreen mode and our native window is invisible,
    // our attempt to go into fullscreen mode will fail with an assertion in
    // system code, without [WindowDelegate windowDidFailToEnterFullScreen:]
    // ever getting called.  To pre-empt this we bail here.  See bug 752294.
    if (aFullScreen && ![mWindow isVisible]) {
      EnteredFullScreen(false);
      return NS_OK;
    }
    MOZ_ASSERT(mInNativeFullScreenMode != aFullScreen,
               "We shouldn't have been in native fullscreen.");
    // Calling toggleFullScreen will result in windowDid(FailTo)?(Enter|Exit)FullScreen
    // to be called from the OS. We will call EnteredFullScreen from those methods,
    // where mInFullScreenMode will be set and a sizemode event will be dispatched.
    [mWindow toggleFullScreen:nil];
  } else {
    NSDisableScreenUpdates();
    // The order here matters. When we exit full screen mode, we need to show the
    // Dock first, otherwise the newly-created window won't have its minimize
    // button enabled. See bug 526282.
    nsCocoaUtils::HideOSChromeOnScreen(aFullScreen);
    nsBaseWidget::InfallibleMakeFullScreen(aFullScreen);
    NSEnableScreenUpdates();
    EnteredFullScreen(aFullScreen, /* aNativeMode */ false);
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// Coordinates are desktop pixels
void nsCocoaWindow::DoResize(double aX, double aY, double aWidth, double aHeight, bool aRepaint,
                             bool aConstrainToCurrentScreen) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mWindow || mInResize) {
    return;
  }

  AutoRestore<bool> reentrantResizeGuard(mInResize);
  mInResize = true;

  // ConstrainSize operates in device pixels, so we need to convert using
  // the backing scale factor here
  CGFloat scale = BackingScaleFactor();
  int32_t width = NSToIntRound(aWidth * scale);
  int32_t height = NSToIntRound(aHeight * scale);
  ConstrainSize(&width, &height);

  DesktopIntRect newBounds(NSToIntRound(aX), NSToIntRound(aY), NSToIntRound(width / scale),
                           NSToIntRound(height / scale));

  // constrain to the screen that contains the largest area of the new rect
  FitRectToVisibleAreaForScreen(newBounds, aConstrainToCurrentScreen ? [mWindow screen] : nullptr);

  // convert requested bounds into Cocoa coordinate system
  NSRect newFrame = nsCocoaUtils::GeckoRectToCocoaRect(newBounds);

  NSRect frame = [mWindow frame];
  BOOL isMoving = newFrame.origin.x != frame.origin.x || newFrame.origin.y != frame.origin.y;
  BOOL isResizing =
      newFrame.size.width != frame.size.width || newFrame.size.height != frame.size.height;

  if (!isMoving && !isResizing) {
    return;
  }

  // We ignore aRepaint -- we have to call display:YES, otherwise the
  // title bar doesn't immediately get repainted and is displayed in
  // the wrong place, leading to a visual jump.
  [mWindow setFrame:newFrame display:YES];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Coordinates are desktop pixels
void nsCocoaWindow::Resize(double aX, double aY, double aWidth, double aHeight, bool aRepaint) {
  DoResize(aX, aY, aWidth, aHeight, aRepaint, false);
}

// Coordinates are desktop pixels
void nsCocoaWindow::Resize(double aWidth, double aHeight, bool aRepaint) {
  double invScale = 1.0 / BackingScaleFactor();
  DoResize(mBounds.x * invScale, mBounds.y * invScale, aWidth, aHeight, aRepaint, true);
}

// Return the area that the Gecko ChildView in our window should cover, as an
// NSRect in screen coordinates (with 0,0 being the bottom left corner of the
// primary screen).
NSRect nsCocoaWindow::GetClientCocoaRect() {
  if (!mWindow) {
    return NSZeroRect;
  }

  return [mWindow childViewRectForFrameRect:[mWindow frame]];
}

LayoutDeviceIntRect nsCocoaWindow::GetClientBounds() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  CGFloat scaleFactor = BackingScaleFactor();
  return nsCocoaUtils::CocoaRectToGeckoRectDevPix(GetClientCocoaRect(), scaleFactor);

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(LayoutDeviceIntRect(0, 0, 0, 0));
}

void nsCocoaWindow::UpdateBounds() {
  NSRect frame = NSZeroRect;
  if (mWindow) {
    frame = [mWindow frame];
  }
  mBounds = nsCocoaUtils::CocoaRectToGeckoRectDevPix(frame, BackingScaleFactor());

  if (mPopupContentView) {
    mPopupContentView->UpdateBoundsFromView();
  }
}

LayoutDeviceIntRect nsCocoaWindow::GetScreenBounds() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

#ifdef DEBUG
  LayoutDeviceIntRect r =
      nsCocoaUtils::CocoaRectToGeckoRectDevPix([mWindow frame], BackingScaleFactor());
  NS_ASSERTION(mWindow && mBounds == r, "mBounds out of sync!");
#endif

  return mBounds;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(LayoutDeviceIntRect(0, 0, 0, 0));
}

double nsCocoaWindow::GetDefaultScaleInternal() { return BackingScaleFactor(); }

static CGFloat GetBackingScaleFactor(NSWindow* aWindow) {
  NSRect frame = [aWindow frame];
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
  NSScreen* screen = FindTargetScreenForRect(nsCocoaUtils::CocoaRectToGeckoRect(frame));
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

  if (mBackingScaleFactor > 0.0) {
    // convert size constraints to the new device pixel coordinate space
    double scaleFactor = newScale / mBackingScaleFactor;
    mSizeConstraints.mMinSize.width = NSToIntRound(mSizeConstraints.mMinSize.width * scaleFactor);
    mSizeConstraints.mMinSize.height = NSToIntRound(mSizeConstraints.mMinSize.height * scaleFactor);
    if (mSizeConstraints.mMaxSize.width < NS_MAXSIZE) {
      mSizeConstraints.mMaxSize.width =
          std::min(NS_MAXSIZE, NSToIntRound(mSizeConstraints.mMaxSize.width * scaleFactor));
    }
    if (mSizeConstraints.mMaxSize.height < NS_MAXSIZE) {
      mSizeConstraints.mMaxSize.height =
          std::min(NS_MAXSIZE, NSToIntRound(mSizeConstraints.mMaxSize.height * scaleFactor));
    }
  }

  mBackingScaleFactor = newScale;

  if (!mWidgetListener || mWidgetListener->GetXULWindow()) {
    return;
  }

  if (PresShell* presShell = mWidgetListener->GetPresShell()) {
    presShell->BackingScaleFactorChanged();
  }
  mWidgetListener->UIResolutionChanged();
}

int32_t nsCocoaWindow::RoundsWidgetCoordinatesTo() {
  if (BackingScaleFactor() == 2.0) {
    return 2;
  }
  return 1;
}

void nsCocoaWindow::SetCursor(nsCursor aDefaultCursor, imgIContainer* aCursorImage,
                              uint32_t aHotspotX, uint32_t aHotspotY) {
  if (mPopupContentView)
    mPopupContentView->SetCursor(aDefaultCursor, aCursorImage, aHotspotX, aHotspotY);
}

nsresult nsCocoaWindow::SetTitle(const nsAString& aTitle) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!mWindow) {
    return NS_OK;
  }

  const nsString& strTitle = PromiseFlatString(aTitle);
  const unichar* uniTitle = reinterpret_cast<const unichar*>(strTitle.get());
  NSString* title = [NSString stringWithCharacters:uniTitle length:strTitle.Length()];
  if ([mWindow drawsContentsIntoWindowFrame] && ![mWindow wantsTitleDrawn]) {
    // Don't cause invalidations when the title isn't displayed.
    [mWindow disableSetNeedsDisplay];
    [mWindow setTitle:title];
    [mWindow enableSetNeedsDisplay];
  } else {
    [mWindow setTitle:title];
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
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
bool nsCocoaWindow::DragEvent(unsigned int aMessage, mozilla::gfx::Point aMouseGlobal,
                              UInt16 aKeyModifiers) {
  return false;
}

NS_IMETHODIMP nsCocoaWindow::SendSetZLevelEvent() {
  nsWindowZ placement = nsWindowZTop;
  nsCOMPtr<nsIWidget> actualBelow;
  if (mWidgetListener)
    mWidgetListener->ZLevelChanged(true, &placement, nullptr, getter_AddRefs(actualBelow));
  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::GetChildSheet(bool aShown, nsIWidget** _retval) {
  nsIWidget* child = GetFirstChild();

  while (child) {
    if (child->WindowType() == eWindowType_sheet) {
      // if it's a sheet, it must be an nsCocoaWindow
      nsCocoaWindow* cocoaWindow = static_cast<nsCocoaWindow*>(child);
      if (cocoaWindow->mWindow && ((aShown && [cocoaWindow->mWindow isVisible]) ||
                                   (!aShown && cocoaWindow->mSheetNeedsShow))) {
        nsCOMPtr<nsIWidget> widget = cocoaWindow;
        widget.forget(_retval);
        return NS_OK;
      }
    }
    child = child->GetNextSibling();
  }

  *_retval = nullptr;

  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::GetRealParent(nsIWidget** parent) {
  *parent = mParent;
  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::GetIsSheet(bool* isSheet) {
  mWindowType == eWindowType_sheet ? * isSheet = true : * isSheet = false;
  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::GetSheetWindowParent(NSWindow** sheetWindowParent) {
  *sheetWindowParent = mSheetWindowParent;
  return NS_OK;
}

// Invokes callback and ProcessEvent methods on Event Listener object
nsresult nsCocoaWindow::DispatchEvent(WidgetGUIEvent* event, nsEventStatus& aStatus) {
  aStatus = nsEventStatus_eIgnore;

  nsCOMPtr<nsIWidget> kungFuDeathGrip(event->mWidget);
  mozilla::Unused << kungFuDeathGrip;  // Not used within this function

  if (mWidgetListener) aStatus = mWidgetListener->HandleEvent(event, mUseAttachedEvents);

  return NS_OK;
}

// aFullScreen should be the window's mInFullScreenMode. We don't have access to that
// from here, so we need to pass it in. mInFullScreenMode should be the canonical
// indicator that a window is currently full screen and it makes sense to keep
// all sizemode logic here.
static nsSizeMode GetWindowSizeMode(NSWindow* aWindow, bool aFullScreen) {
  if (aFullScreen) return nsSizeMode_Fullscreen;
  if ([aWindow isMiniaturized]) return nsSizeMode_Minimized;
  if (([aWindow styleMask] & NSResizableWindowMask) && [aWindow isZoomed])
    return nsSizeMode_Maximized;
  return nsSizeMode_Normal;
}

void nsCocoaWindow::ReportMoveEvent() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Prevent recursion, which can become infinite (see bug 708278).  This
  // can happen when the call to [NSWindow setFrameTopLeftPoint:] in
  // nsCocoaWindow::Move() triggers an immediate NSWindowDidMove notification
  // (and a call to [WindowDelegate windowDidMove:]).
  if (mInReportMoveEvent) {
    return;
  }
  mInReportMoveEvent = true;

  UpdateBounds();

  // Dispatch the move event to Gecko
  NotifyWindowMoved(mBounds.x, mBounds.y);

  mInReportMoveEvent = false;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsCocoaWindow::DispatchSizeModeEvent() {
  if (!mWindow) {
    return;
  }

  nsSizeMode newMode = GetWindowSizeMode(mWindow, mInFullScreenMode);

  // Don't dispatch a sizemode event if:
  // 1. the window is transitioning to fullscreen
  // 2. the new sizemode is the same as the current sizemode
  if (mInFullScreenTransition || mSizeMode == newMode) {
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

  bool newOcclusionState = !([mWindow occlusionState] & NSWindowOcclusionStateVisible);

  // Don't dispatch if the new occlustion state is the same as the current state.
  if (mIsFullyOccluded == newOcclusionState) {
    return;
  }

  mIsFullyOccluded = newOcclusionState;
  if (mWidgetListener) {
    mWidgetListener->OcclusionStateChanged(mIsFullyOccluded);
  }
}

void nsCocoaWindow::ReportSizeEvent() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  UpdateBounds();

  if (mWidgetListener) {
    LayoutDeviceIntRect innerBounds = GetClientBounds();
    mWidgetListener->WindowResized(this, innerBounds.width, innerBounds.height);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsCocoaWindow::SetMenuBar(nsMenuBarX* aMenuBar) {
  if (mMenuBar) mMenuBar->SetParent(nullptr);
  if (!mWindow) {
    mMenuBar = nullptr;
    return;
  }
  mMenuBar = aMenuBar;

  // Only paint for active windows, or paint the hidden window menu bar if no
  // other menu bar has been painted yet so that some reasonable menu bar is
  // displayed when the app starts up.
  id windowDelegate = [mWindow delegate];
  if (mMenuBar && ((!gSomeMenuBarPainted && nsMenuUtilsX::GetHiddenWindowMenuBar() == mMenuBar) ||
                   (windowDelegate && [windowDelegate toplevelActiveState])))
    mMenuBar->Paint();
}

nsresult nsCocoaWindow::SetFocus(bool aState) {
  if (!mWindow) return NS_OK;

  if (mPopupContentView) {
    mPopupContentView->SetFocus(aState);
  } else if (aState && ([mWindow isVisible] || [mWindow isMiniaturized])) {
    if ([mWindow isMiniaturized]) {
      [mWindow deminiaturize:nil];
    }

    [mWindow makeKeyAndOrderFront:nil];
    SendSetZLevelEvent();
  }

  return NS_OK;
}

LayoutDeviceIntPoint nsCocoaWindow::WidgetToScreenOffset() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  return nsCocoaUtils::CocoaRectToGeckoRectDevPix(GetClientCocoaRect(), BackingScaleFactor())
      .TopLeft();

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(LayoutDeviceIntPoint(0, 0));
}

LayoutDeviceIntPoint nsCocoaWindow::GetClientOffset() {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  LayoutDeviceIntRect clientRect = GetClientBounds();

  return clientRect.TopLeft() - mBounds.TopLeft();

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(LayoutDeviceIntPoint(0, 0));
}

LayoutDeviceIntSize nsCocoaWindow::ClientToWindowSize(const LayoutDeviceIntSize& aClientSize) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (!mWindow) return LayoutDeviceIntSize(0, 0);

  CGFloat backingScale = BackingScaleFactor();
  LayoutDeviceIntRect r(0, 0, aClientSize.width, aClientSize.height);
  NSRect rect = nsCocoaUtils::DevPixelsToCocoaPoints(r, backingScale);

  // Our caller expects the inflated rect for windows *with separate titlebars*,
  // i.e. for windows where [mWindow drawsContentsIntoWindowFrame] is NO.
  //
  // So we call frameRectForContentRect on NSWindow here, instead of mWindow, so
  // that we don't run into our override if this window is a window that draws
  // its content into the titlebar.
  //
  // This is the same thing the windows widget does, but we probably should fix
  // that, see bug 1445738.
  NSUInteger styleMask = [mWindow styleMask];
  NSRect inflatedRect = [NSWindow frameRectForContentRect:rect styleMask:styleMask];
  r = nsCocoaUtils::CocoaRectToGeckoRectDevPix(inflatedRect, backingScale);
  return r.Size();

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(LayoutDeviceIntSize(0, 0));
}

nsMenuBarX* nsCocoaWindow::GetMenuBar() { return mMenuBar; }

void nsCocoaWindow::CaptureRollupEvents(nsIRollupListener* aListener, bool aDoCapture) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  gRollupListener = nullptr;

  if (aDoCapture) {
    if (![NSApp isActive]) {
      // We need to capture mouse event if we aren't
      // the active application. We only set this up when needed
      // because they cause spurious mouse event after crash
      // and gdb sessions. See bug 699538.
      nsToolkit::GetToolkit()->RegisterForAllProcessMouseEvents();
    }
    gRollupListener = aListener;

    // Sometimes more than one popup window can be visible at the same time
    // (e.g. nested non-native context menus, or the test case (attachment
    // 276885) for bmo bug 392389, which displays a non-native combo-box in a
    // non-native popup window).  In these cases the "active" popup window should
    // be the topmost -- the (nested) context menu the mouse is currently over,
    // or the combo-box's drop-down list (when it's displayed).  But (among
    // windows that have the same "level") OS X makes topmost the window that
    // last received a mouse-down event, which may be incorrect (in the combo-
    // box case, it makes topmost the window containing the combo-box).  So
    // here we fiddle with a non-native popup window's level to make sure the
    // "active" one is always above any other non-native popup windows that
    // may be visible.
    if (mWindow && (mWindowType == eWindowType_popup)) SetPopupWindowLevel();
  } else {
    nsToolkit::GetToolkit()->UnregisterAllProcessMouseEventHandlers();

    // XXXndeakin this doesn't make sense.
    // Why is the new window assumed to be a modal panel?
    if (mWindow && (mWindowType == eWindowType_popup)) [mWindow setLevel:NSModalPanelWindowLevel];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsresult nsCocoaWindow::GetAttention(int32_t aCycleCount) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  [NSApp requestUserAttention:NSInformationalRequest];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

bool nsCocoaWindow::HasPendingInputEvent() { return nsChildView::DoHasPendingInputEvent(); }

void nsCocoaWindow::SetWindowShadowStyle(int32_t aStyle) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mWindow) return;

  mShadowStyle = aStyle;

  // Shadowless windows are only supported on popups.
  if (mWindowType == eWindowType_popup) {
    [mWindow setHasShadow:aStyle != NS_STYLE_WINDOW_SHADOW_NONE];
  }

  [mWindow setUseMenuStyle:(aStyle == NS_STYLE_WINDOW_SHADOW_MENU)];
  AdjustWindowShadow();
  SetWindowBackgroundBlur();

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsCocoaWindow::SetWindowOpacity(float aOpacity) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mWindow) {
    return;
  }

  [mWindow setAlphaValue:(CGFloat)aOpacity];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

static inline CGAffineTransform GfxMatrixToCGAffineTransform(const gfx::Matrix& m) {
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mWindow) {
    return;
  }

  // Calling CGSSetWindowTransform when the window is not visible results in
  // misplacing the window into doubled x,y coordinates (see bug 1448132).
  if (![mWindow isVisible] || NSIsEmptyRect([mWindow frame])) {
    return;
  }

  if (gfxPrefs::WindowTransformsDisabled()) {
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
    transform = gfx::Matrix::Translation(snappedTranslation.x, snappedTranslation.y);
  }

  // We also need to account for the backing scale factor: aTransform is given
  // in device pixels, but CGSSetWindowTransform works with logical display
  // pixels.
  CGFloat backingScale = BackingScaleFactor();
  transform.PreScale(backingScale, backingScale);
  transform.PostScale(1 / backingScale, 1 / backingScale);

  CGSConnection cid = _CGSDefaultConnection();
  CGSSetWindowTransform(cid, [mWindow windowNumber], GfxMatrixToCGAffineTransform(transform));

  mWindowTransformIsIdentity = isIdentity;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsCocoaWindow::SetShowsToolbarButton(bool aShow) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mWindow) [mWindow setShowsToolbarButton:aShow];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsCocoaWindow::SetShowsFullScreenButton(bool aShow) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mWindow || ![mWindow respondsToSelector:@selector(toggleFullScreen:)] ||
      mSupportsNativeFullScreen == aShow) {
    return;
  }

  // If the window is currently in fullscreen mode, then we're going to
  // transition out first, then set the collection behavior & toggle
  // mSupportsNativeFullScreen, then transtion back into fullscreen mode. This
  // prevents us from getting into a conflicting state with MakeFullScreen
  // where mSupportsNativeFullScreen would lead us down the wrong path.
  bool wasFullScreen = mInFullScreenMode;

  if (wasFullScreen) {
    MakeFullScreen(false);
  }

  NSWindowCollectionBehavior newBehavior = [mWindow collectionBehavior];
  if (aShow) {
    newBehavior |= NSWindowCollectionBehaviorFullScreenPrimary;
  } else {
    newBehavior &= ~NSWindowCollectionBehaviorFullScreenPrimary;
  }
  [mWindow setCollectionBehavior:newBehavior];
  mSupportsNativeFullScreen = aShow;

  if (wasFullScreen) {
    MakeFullScreen(true);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsCocoaWindow::SetWindowAnimationType(nsIWidget::WindowAnimationType aType) {
  mAnimationType = aType;
}

void nsCocoaWindow::SetDrawsTitle(bool aDrawTitle) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (![mWindow drawsContentsIntoWindowFrame]) {
    // If we don't draw into the window frame, we always want to display window
    // titles.
    [mWindow setWantsTitleDrawn:YES];
  } else {
    [mWindow setWantsTitleDrawn:aDrawTitle];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsCocoaWindow::SetUseBrightTitlebarForeground(bool aBrightForeground) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mWindow setUseBrightTitlebarForeground:aBrightForeground];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsresult nsCocoaWindow::SetNonClientMargins(LayoutDeviceIntMargin& margins) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  SetDrawsInTitlebar(margins.top == 0);

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void nsCocoaWindow::SetDrawsInTitlebar(bool aState) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mWindow) [mWindow setDrawsContentsIntoWindowFrame:aState];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

NS_IMETHODIMP nsCocoaWindow::SynthesizeNativeMouseEvent(LayoutDeviceIntPoint aPoint,
                                                        uint32_t aNativeMessage,
                                                        uint32_t aModifierFlags,
                                                        nsIObserver* aObserver) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  AutoObserverNotifier notifier(aObserver, "mouseevent");
  if (mPopupContentView)
    return mPopupContentView->SynthesizeNativeMouseEvent(aPoint, aNativeMessage, aModifierFlags,
                                                         nullptr);

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void nsCocoaWindow::UpdateThemeGeometries(const nsTArray<ThemeGeometry>& aThemeGeometries) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mPopupContentView) {
    return mPopupContentView->UpdateThemeGeometries(aThemeGeometries);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsCocoaWindow::SetPopupWindowLevel() {
  if (!mWindow) return;

  // Floating popups are at the floating level and hide when the window is
  // deactivated.
  if (mPopupLevel == ePopupLevelFloating) {
    [mWindow setLevel:NSFloatingWindowLevel];
    [mWindow setHidesOnDeactivate:YES];
  } else {
    // Otherwise, this is a top-level or parent popup. Parent popups always
    // appear just above their parent and essentially ignore the level.
    [mWindow setLevel:NSPopUpMenuWindowLevel];
    [mWindow setHidesOnDeactivate:NO];
  }
}

void nsCocoaWindow::SetInputContext(const InputContext& aContext,
                                    const InputContextAction& aAction) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  mInputContext = aContext;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsCocoaWindow::GetEditCommands(NativeKeyBindingsType aType, const WidgetKeyboardEvent& aEvent,
                                    nsTArray<CommandInt>& aCommands) {
  // Validate the arguments.
  nsIWidget::GetEditCommands(aType, aEvent, aCommands);

  NativeKeyBindings* keyBindings = NativeKeyBindings::GetInstance(aType);
  keyBindings->GetEditCommands(aEvent, aCommands);
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // make sure we only act on windows that have this kind of
  // object as a delegate
  id windowDelegate = [aWindow delegate];
  if ([windowDelegate class] != [self class]) return;

  nsCocoaWindow* geckoWidget = [windowDelegate geckoWidget];
  NS_ASSERTION(geckoWidget, "Window delegate not returning a gecko widget!");

  nsMenuBarX* geckoMenuBar = geckoWidget->GetMenuBar();
  if (geckoMenuBar) {
    geckoMenuBar->Paint();
  } else {
    // sometimes we don't have a native application menu early in launching
    if (!sApplicationMenu) return;

    NSMenu* mainMenu = [NSApp mainMenu];
    NS_ASSERTION([mainMenu numberOfItems] > 0,
                 "Main menu does not have any items, something is terribly wrong!");

    // Create a new menu bar.
    // We create a GeckoNSMenu because all menu bar NSMenu objects should use that subclass for
    // key handling reasons.
    GeckoNSMenu* newMenuBar = [[GeckoNSMenu alloc] initWithTitle:@"MainMenuBar"];

    // move the application menu from the existing menu bar to the new one
    NSMenuItem* firstMenuItem = [[mainMenu itemAtIndex:0] retain];
    [mainMenu removeItemAtIndex:0];
    [newMenuBar insertItem:firstMenuItem atIndex:0];
    [firstMenuItem release];

    // set our new menu bar as the main menu
    [NSApp setMainMenu:newMenuBar];
    [newMenuBar release];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (id)initWithGeckoWindow:(nsCocoaWindow*)geckoWind {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  [super init];
  mGeckoWindow = geckoWind;
  mToplevelActiveState = false;
  mHasEverBeenZoomed = false;
  return self;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSSize)windowWillResize:(NSWindow*)sender toSize:(NSSize)proposedFrameSize {
  RollUpPopups();

  return proposedFrameSize;
}

- (void)windowDidResize:(NSNotification*)aNotification {
  BaseWindow* window = [aNotification object];
  [window updateTrackingArea];

  if (!mGeckoWindow) return;

  // Resizing might have changed our zoom state.
  mGeckoWindow->DispatchSizeModeEvent();
  mGeckoWindow->ReportSizeEvent();
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

  mGeckoWindow->WillEnterFullScreen(true);
}

// Lion's full screen mode will bypass our internal fullscreen tracking, so
// we need to catch it when we transition and call our own methods, which in
// turn will fire "fullscreen" events.
- (void)windowDidEnterFullScreen:(NSNotification*)notification {
  if (!mGeckoWindow) {
    return;
  }

  mGeckoWindow->EnteredFullScreen(true);

  // On Yosemite, the NSThemeFrame class has two new properties --
  // titlebarView (an NSTitlebarView object) and titlebarContainerView (an
  // NSTitlebarContainerView object).  These are used to display the titlebar
  // in fullscreen mode.  In Safari they're not transparent.  But in Firefox
  // for some reason they are, which causes bug 1069658.  The following code
  // works around this Apple bug or design flaw.
  NSWindow* window = (NSWindow*)[notification object];
  NSView* frameView = [[window contentView] superview];
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
}

- (void)windowWillExitFullScreen:(NSNotification*)notification {
  if (!mGeckoWindow) {
    return;
  }

  mGeckoWindow->WillEnterFullScreen(false);
}

- (void)windowDidExitFullScreen:(NSNotification*)notification {
  if (!mGeckoWindow) {
    return;
  }

  mGeckoWindow->EnteredFullScreen(false);
}

- (void)windowDidFailToEnterFullScreen:(NSWindow*)window {
  if (!mGeckoWindow) {
    return;
  }

  mGeckoWindow->EnteredFullScreen(false);
}

- (void)windowDidFailToExitFullScreen:(NSWindow*)window {
  if (!mGeckoWindow) {
    return;
  }

  mGeckoWindow->EnteredFullScreen(true);
}

- (void)windowDidBecomeMain:(NSNotification*)aNotification {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  RollUpPopups();
  ChildViewMouseTracker::ReEvaluateMouseEnterState();

  // [NSApp _isRunningAppModal] will return true if we're running an OS dialog
  // app modally. If one of those is up then we want it to retain its menu bar.
  if ([NSApp _isRunningAppModal]) return;
  NSWindow* window = [aNotification object];
  if (window) [WindowDelegate paintMenubarForWindow:window];

  if ([window isKindOfClass:[ToolbarWindow class]]) {
    [(ToolbarWindow*)window windowMainStateChanged];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)windowDidResignMain:(NSNotification*)aNotification {
  RollUpPopups();
  ChildViewMouseTracker::ReEvaluateMouseEnterState();

  // [NSApp _isRunningAppModal] will return true if we're running an OS dialog
  // app modally. If one of those is up then we want it to retain its menu bar.
  if ([NSApp _isRunningAppModal]) return;
  RefPtr<nsMenuBarX> hiddenWindowMenuBar = nsMenuUtilsX::GetHiddenWindowMenuBar();
  if (hiddenWindowMenuBar) {
    // printf("painting hidden window menu bar due to window losing main status\n");
    hiddenWindowMenuBar->Paint();
  }

  NSWindow* window = [aNotification object];
  if ([window isKindOfClass:[ToolbarWindow class]]) {
    [(ToolbarWindow*)window windowMainStateChanged];
  }
}

- (void)windowDidBecomeKey:(NSNotification*)aNotification {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  RollUpPopups();
  ChildViewMouseTracker::ReEvaluateMouseEnterState();

  NSWindow* window = [aNotification object];
  if ([window isSheet]) [WindowDelegate paintMenubarForWindow:window];

  nsChildView* mainChildView =
      static_cast<nsChildView*>([[(BaseWindow*)window mainChildView] widget]);
  if (mainChildView) {
    if (mainChildView->GetInputContext().IsPasswordEditor()) {
      TextInputHandler::EnableSecureEventInput();
    } else {
      TextInputHandler::EnsureSecureEventInputDisabled();
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)windowDidResignKey:(NSNotification*)aNotification {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  RollUpPopups();
  ChildViewMouseTracker::ReEvaluateMouseEnterState();

  // If a sheet just resigned key then we should paint the menu bar
  // for whatever window is now main.
  NSWindow* window = [aNotification object];
  if ([window isSheet]) [WindowDelegate paintMenubarForWindow:[NSApp mainWindow]];

  TextInputHandler::EnsureSecureEventInputDisabled();

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)windowWillMove:(NSNotification*)aNotification {
  RollUpPopups();
}

- (void)windowDidMove:(NSNotification*)aNotification {
  if (mGeckoWindow) mGeckoWindow->ReportMoveEvent();
}

- (BOOL)windowShouldClose:(id)sender {
  nsIWidgetListener* listener = mGeckoWindow ? mGeckoWindow->GetWidgetListener() : nullptr;
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
  if (mGeckoWindow) mGeckoWindow->DispatchSizeModeEvent();
}

- (void)windowDidDeminiaturize:(NSNotification*)aNotification {
  if (mGeckoWindow) mGeckoWindow->DispatchSizeModeEvent();
}

- (BOOL)windowShouldZoom:(NSWindow*)window toFrame:(NSRect)proposedFrame {
  if (!mHasEverBeenZoomed && [window isZoomed]) return NO;  // See bug 429954.

  mHasEverBeenZoomed = YES;
  return YES;
}

- (NSRect)window:(NSWindow*)window willPositionSheet:(NSWindow*)sheet usingRect:(NSRect)rect {
  if ([window isKindOfClass:[ToolbarWindow class]]) {
    rect.origin.y = [(ToolbarWindow*)window sheetAttachmentPosition];
  }
  return rect;
}

- (void)didEndSheet:(NSWindow*)sheet returnCode:(int)returnCode contextInfo:(void*)contextInfo {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Note: 'contextInfo' (if it is set) is the window that is the parent of
  // the sheet.  The value of contextInfo is determined in
  // nsCocoaWindow::Show().  If it's set, 'contextInfo' is always the top-
  // level window, not another sheet itself.  But 'contextInfo' is nil if
  // our parent window is also a sheet -- in that case we shouldn't send
  // the top-level window any activate events (because it's our parent
  // window that needs to get these events, not the top-level window).
  [TopLevelWindowData deactivateInWindow:sheet];
  [sheet orderOut:self];
  if (contextInfo) [TopLevelWindowData activateInWindow:(NSWindow*)contextInfo];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)windowDidChangeBackingProperties:(NSNotification*)aNotification {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSWindow* window = (NSWindow*)[aNotification object];

  if ([window respondsToSelector:@selector(backingScaleFactor)]) {
    CGFloat oldFactor =
        [[[aNotification userInfo] objectForKey:@"NSBackingPropertyOldScaleFactorKey"] doubleValue];
    if ([window backingScaleFactor] != oldFactor) {
      mGeckoWindow->BackingScaleFactorChanged();
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
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
    nsIWidgetListener* listener = mGeckoWindow->GetWidgetListener();
    if (listener) {
      listener->WindowActivated();
    }
    mToplevelActiveState = true;
  }
}

- (void)sendToplevelDeactivateEvents {
  if (mToplevelActiveState && mGeckoWindow) {
    nsIWidgetListener* listener = mGeckoWindow->GetWidgetListener();
    if (listener) {
      listener->WindowDeactivated();
    }
    mToplevelActiveState = false;
  }
}

@end

@interface NSView (FrameViewMethodSwizzling)
- (NSPoint)FrameView__closeButtonOrigin;
- (NSPoint)FrameView__fullScreenButtonOrigin;
- (BOOL)FrameView__wantsFloatingTitlebar;
@end

@implementation NSView (FrameViewMethodSwizzling)

- (NSPoint)FrameView__closeButtonOrigin {
  NSPoint defaultPosition = [self FrameView__closeButtonOrigin];
  if ([[self window] isKindOfClass:[ToolbarWindow class]]) {
    return [(ToolbarWindow*)[self window] windowButtonsPositionWithDefaultPosition:defaultPosition];
  }
  return defaultPosition;
}

- (NSPoint)FrameView__fullScreenButtonOrigin {
  NSPoint defaultPosition = [self FrameView__fullScreenButtonOrigin];
  if ([[self window] isKindOfClass:[ToolbarWindow class]]) {
    return
        [(ToolbarWindow*)[self window] fullScreenButtonPositionWithDefaultPosition:defaultPosition];
  }
  return defaultPosition;
}

- (BOOL)FrameView__wantsFloatingTitlebar {
  return NO;
}

@end

static NSMutableSet* gSwizzledFrameViewClasses = nil;

@interface NSWindow (PrivateSetNeedsDisplayInRectMethod)
- (void)_setNeedsDisplayInRect:(NSRect)aRect;
@end

// This method is on NSThemeFrame starting with 10.10, but since NSThemeFrame
// is not a public class, we declare the method on NSView instead. We only have
// this declaration in order to avoid compiler warnings.
@interface NSView (PrivateAddKnownSubviewMethod)
- (void)_addKnownSubview:(NSView*)aView
              positioned:(NSWindowOrderingMode)place
              relativeTo:(NSView*)otherView;
@end

#if !defined(MAC_OS_X_VERSION_10_10) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_10

@interface NSImage (CapInsets)
- (void)setCapInsets:(NSEdgeInsets)capInsets;
@end

#endif

#if !defined(MAC_OS_X_VERSION_10_8) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_8

@interface NSImage (ImageCreationWithDrawingHandler)
+ (NSImage*)imageWithSize:(NSSize)size
                  flipped:(BOOL)drawingHandlerShouldBeCalledWithFlippedContext
           drawingHandler:(BOOL (^)(NSRect dstRect))drawingHandler;
@end

#endif

#if !defined(MAC_OS_X_VERSION_10_12_2) || MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_12_2
@interface NSView (NSTouchBarProvider)
- (NSTouchBar*)makeTouchBar;
@end
#endif

@interface NSView (NSVisualEffectViewSetMaskImage)
- (void)setMaskImage:(NSImage*)image;
@end

@interface BaseWindow (Private)
- (void)removeTrackingArea;
- (void)cursorUpdated:(NSEvent*)aEvent;
- (void)reflowTitlebarElements;
@end

@implementation BaseWindow

// The frame of a window is implemented using undocumented NSView subclasses.
// We offset the window buttons by overriding the methods _closeButtonOrigin
// and _fullScreenButtonOrigin on these frame view classes. The class which is
// used for a window is determined in the window's frameViewClassForStyleMask:
// method, so this is where we make sure that we have swizzled the method on
// all encountered classes.
// We also override the _wantsFloatingTitlebar method to return NO in order to
// avoid some glitches in the titlebar that are caused by the way we mess with
// the window.
+ (Class)frameViewClassForStyleMask:(NSUInteger)styleMask {
  Class frameViewClass = [super frameViewClassForStyleMask:styleMask];

  if (!gSwizzledFrameViewClasses) {
    gSwizzledFrameViewClasses = [[NSMutableSet setWithCapacity:3] retain];
    if (!gSwizzledFrameViewClasses) {
      return frameViewClass;
    }
  }

  static IMP our_closeButtonOrigin =
      class_getMethodImplementation([NSView class], @selector(FrameView__closeButtonOrigin));
  static IMP our_fullScreenButtonOrigin =
      class_getMethodImplementation([NSView class], @selector(FrameView__fullScreenButtonOrigin));
  static IMP our_wantsFloatingTitlebar =
      class_getMethodImplementation([NSView class], @selector(FrameView__wantsFloatingTitlebar));

  if (![gSwizzledFrameViewClasses containsObject:frameViewClass]) {
    // Either of these methods might be implemented in both a subclass of
    // NSFrameView and one of its own subclasses.  Which means that if we
    // aren't careful we might end up swizzling the same method twice.
    // Since method swizzling involves swapping pointers, this would break
    // things.
    IMP _closeButtonOrigin =
        class_getMethodImplementation(frameViewClass, @selector(_closeButtonOrigin));
    if (_closeButtonOrigin && _closeButtonOrigin != our_closeButtonOrigin) {
      nsToolkit::SwizzleMethods(frameViewClass, @selector(_closeButtonOrigin),
                                @selector(FrameView__closeButtonOrigin));
    }
    IMP _fullScreenButtonOrigin =
        class_getMethodImplementation(frameViewClass, @selector(_fullScreenButtonOrigin));
    if (_fullScreenButtonOrigin && _fullScreenButtonOrigin != our_fullScreenButtonOrigin) {
      nsToolkit::SwizzleMethods(frameViewClass, @selector(_fullScreenButtonOrigin),
                                @selector(FrameView__fullScreenButtonOrigin));
    }
    IMP _wantsFloatingTitlebar =
        class_getMethodImplementation(frameViewClass, @selector(_wantsFloatingTitlebar));
    if (_wantsFloatingTitlebar && _wantsFloatingTitlebar != our_wantsFloatingTitlebar) {
      nsToolkit::SwizzleMethods(frameViewClass, @selector(_wantsFloatingTitlebar),
                                @selector(FrameView__wantsFloatingTitlebar));
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
  [super initWithContentRect:aContentRect styleMask:aStyle backing:aBufferingType defer:aFlag];
  mState = nil;
  mDisabledNeedsDisplay = NO;
  mTrackingArea = nil;
  mDirtyRect = NSZeroRect;
  mBeingShown = NO;
  mDrawTitle = NO;
  mBrightTitlebarForeground = NO;
  mUseMenuStyle = NO;
  mTouchBar = nil;
  [self updateTrackingArea];

  return self;
}

// Returns an autoreleased NSImage.
static NSImage* GetMenuMaskImage() {
  CGFloat radius = 4.0f;
  NSEdgeInsets insets = {5, 5, 5, 5};
  NSSize maskSize = {12, 12};
  NSImage* maskImage = [NSImage imageWithSize:maskSize
                                      flipped:YES
                               drawingHandler:^BOOL(NSRect dstRect) {
                                 NSBezierPath* path =
                                     [NSBezierPath bezierPathWithRoundedRect:dstRect
                                                                     xRadius:radius
                                                                     yRadius:radius];
                                 [[NSColor colorWithDeviceWhite:1.0 alpha:1.0] set];
                                 [path fill];
                                 return YES;
                               }];
  [maskImage setCapInsets:insets];
  return maskImage;
}

- (void)swapOutChildViewWrapper:(NSView*)aNewWrapper {
  [aNewWrapper setFrame:[[self contentView] frame]];
  NSView* childView = [[self mainChildView] retain];
  [childView removeFromSuperview];
  [aNewWrapper addSubview:childView];
  [childView release];
  [super setContentView:aNewWrapper];
}

- (void)setUseMenuStyle:(BOOL)aValue {
  if (!VibrancyManager::SystemSupportsVibrancy()) {
    return;
  }

  if (aValue && !mUseMenuStyle) {
    // Turn on rounded corner masking.
    NSView* effectView = VibrancyManager::CreateEffectView(VibrancyType::MENU, YES);
    if ([effectView respondsToSelector:@selector(setMaskImage:)]) {
      [effectView setMaskImage:GetMenuMaskImage()];
    }
    [self swapOutChildViewWrapper:effectView];
    [effectView release];
  } else if (mUseMenuStyle && !aValue) {
    // Turn off rounded corner masking.
    NSView* wrapper = [[NSView alloc] initWithFrame:NSZeroRect];
    [self swapOutChildViewWrapper:wrapper];
    [wrapper release];
  }
  mUseMenuStyle = aValue;
}

- (NSTouchBar*)makeTouchBar {
  mTouchBar = [[nsTouchBar alloc] init];
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
static const NSString* kStateDrawsContentsIntoWindowFrameKey = @"drawsContentsIntoWindowFrame";
static const NSString* kStateShowsToolbarButton = @"showsToolbarButton";
static const NSString* kStateCollectionBehavior = @"collectionBehavior";

- (void)importState:(NSDictionary*)aState {
  if (NSString* title = [aState objectForKey:kStateTitleKey]) {
    [self setTitle:title];
  }
  [self setDrawsContentsIntoWindowFrame:[[aState objectForKey:kStateDrawsContentsIntoWindowFrameKey]
                                            boolValue]];
  [self setShowsToolbarButton:[[aState objectForKey:kStateShowsToolbarButton] boolValue]];
  [self setCollectionBehavior:[[aState objectForKey:kStateCollectionBehavior] unsignedIntValue]];
}

- (NSMutableDictionary*)exportState {
  NSMutableDictionary* state = [NSMutableDictionary dictionaryWithCapacity:10];
  if (NSString* title = [self title]) {
    [state setObject:title forKey:kStateTitleKey];
  }
  [state setObject:[NSNumber numberWithBool:[self drawsContentsIntoWindowFrame]]
            forKey:kStateDrawsContentsIntoWindowFrameKey];
  [state setObject:[NSNumber numberWithBool:[self showsToolbarButton]]
            forKey:kStateShowsToolbarButton];
  [state setObject:[NSNumber numberWithUnsignedInt:[self collectionBehavior]]
            forKey:kStateCollectionBehavior];
  return state;
}

- (void)setDrawsContentsIntoWindowFrame:(BOOL)aState {
  bool changed = (aState != mDrawsIntoWindowFrame);
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
  return [NSWindow contentRectForFrameRect:aFrameRect styleMask:styleMask];
}

- (NSRect)frameRectForChildViewRect:(NSRect)aChildViewRect {
  if (mDrawsIntoWindowFrame) {
    return aChildViewRect;
  }
  NSUInteger styleMask = [self styleMask];
  return [NSWindow frameRectForContentRect:aChildViewRect styleMask:styleMask];
}

- (void)setWantsTitleDrawn:(BOOL)aDrawTitle {
  mDrawTitle = aDrawTitle;
  if ([self respondsToSelector:@selector(setTitleVisibility:)]) {
    [self setTitleVisibility:mDrawTitle ? NSWindowTitleVisible : NSWindowTitleHidden];
  }
}

- (BOOL)wantsTitleDrawn {
  return mDrawTitle;
}

- (void)setUseBrightTitlebarForeground:(BOOL)aBrightForeground {
  mBrightTitlebarForeground = aBrightForeground;
  [[self standardWindowButton:NSWindowFullScreenButton] setNeedsDisplay:YES];
}

- (BOOL)useBrightTitlebarForeground {
  return mBrightTitlebarForeground;
}

- (NSView*)trackingAreaView {
  NSView* contentView = [self contentView];
  return [contentView superview] ? [contentView superview] : contentView;
}

- (NSArray<NSView*>*)contentViewContents {
  return [[[[self contentView] subviews] copy] autorelease];
}

- (ChildView*)mainChildView {
  NSView* contentView = [self contentView];
  NSView* lastView = [[contentView subviews] lastObject];
  if ([lastView isKindOfClass:[ChildView class]]) {
    return (ChildView*)lastView;
  }
  return nil;
}

- (void)removeTrackingArea {
  if (mTrackingArea) {
    [[self trackingAreaView] removeTrackingArea:mTrackingArea];
    [mTrackingArea release];
    mTrackingArea = nil;
  }
}

- (void)updateTrackingArea {
  [self removeTrackingArea];

  NSView* view = [self trackingAreaView];
  const NSTrackingAreaOptions options =
      NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveAlways;
  mTrackingArea = [[NSTrackingArea alloc] initWithRect:[view bounds]
                                               options:options
                                                 owner:self
                                              userInfo:nil];
  [view addTrackingArea:mTrackingArea];
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
  NSView* frameView = [[self contentView] superview];
  if ([frameView respondsToSelector:@selector(_tileTitlebarAndRedisplay:)]) {
    [frameView _tileTitlebarAndRedisplay:NO];
  }
}

// Override methods that translate between content rect and frame rect.
- (NSRect)contentRectForFrameRect:(NSRect)aRect {
  return aRect;
}

- (NSRect)contentRectForFrameRect:(NSRect)aRect styleMask:(NSUInteger)aMask {
  return aRect;
}

- (NSRect)frameRectForContentRect:(NSRect)aRect {
  return aRect;
}

- (NSRect)frameRectForContentRect:(NSRect)aRect styleMask:(NSUInteger)aMask {
  return aRect;
}

- (void)setContentView:(NSView*)aView {
  [super setContentView:aView];

  // Now move the contentView to the bottommost layer so that it's guaranteed
  // to be under the window buttons.
  NSView* frameView = [aView superview];
  [aView removeFromSuperview];
  if ([frameView respondsToSelector:@selector(_addKnownSubview:positioned:relativeTo:)]) {
    // 10.10 prints a warning when we call addSubview on the frame view, so we
    // silence the warning by calling a private method instead.
    [frameView _addKnownSubview:aView positioned:NSWindowBelow relativeTo:nil];
  } else {
    [frameView addSubview:aView positioned:NSWindowBelow relativeTo:nil];
  }
}

- (NSArray*)titlebarControls {
  // Return all subviews of the frameView which are not the content view.
  NSView* frameView = [[self contentView] superview];
  NSMutableArray* array = [[[frameView subviews] mutableCopy] autorelease];
  [array removeObject:[self contentView]];
  return array;
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
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

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
  if ([retval isKindOfClass:[NSArray class]] && [attribute isEqualToString:@"AXChildren"]) {
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

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)releaseJSObjects {
  [mTouchBar releaseJSObjects];
}

@end

@interface NSView (NSThemeFrame)
- (void)_drawTitleStringInClip:(NSRect)aRect;
- (void)_maskCorners:(NSUInteger)aFlags clipRect:(NSRect)aRect;
@end

@implementation TitlebarGradientView

- (void)drawRect:(NSRect)aRect {
  CGContextRef ctx = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
  ToolbarWindow* window = (ToolbarWindow*)[self window];
  nsNativeThemeCocoa::DrawNativeTitlebar(ctx, NSRectToCGRect([self bounds]),
                                         [window unifiedToolbarHeight], [window isMainWindow], NO);

  // The following is only necessary because we're not using
  // NSFullSizeContentViewWindowMask yet: We need to mask our drawing to the
  // rounded top corners of the window, and we need to draw the title string
  // on top. That's because the title string is drawn as part of the frame view
  // and this view covers that drawing up.
  // Once we use NSFullSizeContentViewWindowMask and remove our override of
  // _wantsFloatingTitlebar, Cocoa will draw the title string as part of a
  // separate view which sits on top of the window's content view, and we'll be
  // able to remove the code below.

  NSView* frameView = [[[self window] contentView] superview];
  if (!frameView || ![frameView respondsToSelector:@selector(_maskCorners:clipRect:)] ||
      ![frameView respondsToSelector:@selector(_drawTitleStringInClip:)]) {
    return;
  }

  NSPoint offsetToFrameView = [self convertPoint:NSZeroPoint toView:frameView];
  NSRect clipRect = {offsetToFrameView, [self bounds].size};

  // Both this view and frameView return NO from isFlipped. Switch into
  // frameView's coordinate system using a translation by the offset.
  CGContextSaveGState(ctx);
  CGContextTranslateCTM(ctx, -offsetToFrameView.x, -offsetToFrameView.y);

  [frameView _maskCorners:2 clipRect:clipRect];
  [frameView _drawTitleStringInClip:clipRect];

  CGContextRestoreGState(ctx);
}

- (BOOL)isOpaque {
  return YES;
}

- (BOOL)mouseDownCanMoveWindow {
  return YES;
}

- (NSView*)hitTest:(NSPoint)aPoint {
  return nil;
}

@end

// This class allows us to exercise control over the window's title bar. It is
// used for all windows with titlebars.
//
// ToolbarWindow supports two modes:
//  - drawsContentsIntoWindowFrame mode: In this mode, the Gecko ChildView is
//    sized to cover the entire window frame and manages titlebar drawing.
//  - separate titlebar mode, with support for unified toolbars: In this mode,
//    the Gecko ChildView does not extend into the titlebar. However, this
//    window's content view (which is the ChildView's superview) *does* extend
//    into the titlebar. Moreover, in this mode, we place a TitlebarGradientView
//    in the content view, as a sibling of the ChildView.
//
// The "separate titlebar mode" supports the "unified toolbar" look:
// If there's a toolbar right below the titlebar, the two can "connect" and
// form a single gradient without a separator line in between.
//
// The following mechanism communicates the height of the unified toolbar to
// the ToolbarWindow:
//
// 1) In the style sheet we set the toolbar's -moz-appearance to toolbar.
// 2) When the toolbar is visible and we paint the application chrome
//    window, the array that Gecko passes nsChildView::UpdateThemeGeometries
//    will contain an entry for the widget type StyleAppearance::Toolbar.
// 3) nsChildView::UpdateThemeGeometries passes the toolbar's height, plus the
//    titlebar height, to -[ToolbarWindow setUnifiedToolbarHeight:].
//
// The actual drawing of the gradient happens in two parts: The titlebar part
// (i.e. the top 22 pixels of the gradient) is drawn by the TitlebarGradientView,
// which is a subview of the window's content view and a sibling of the ChildView.
// The rest of the gradient is drawn by Gecko into the ChildView, as part of the
// -moz-appearance rendering of the toolbar.
@implementation ToolbarWindow

- (id)initWithContentRect:(NSRect)aChildViewRect
                styleMask:(NSUInteger)aStyle
                  backing:(NSBackingStoreType)aBufferingType
                    defer:(BOOL)aFlag {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  // We treat aChildViewRect as the rectangle that the window's main ChildView
  // should be sized to. Get the right frameRect for the requested child view
  // rect.
  NSRect frameRect = [NSWindow frameRectForContentRect:aChildViewRect styleMask:aStyle];

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
    mTitlebarGradientView = nil;
    mUnifiedToolbarHeight = 22.0f;
    mSheetAttachmentPosition = aChildViewRect.size.height;
    mWindowButtonsRect = NSZeroRect;
    mFullScreenButtonRect = NSZeroRect;

    if ([self respondsToSelector:@selector(setTitlebarAppearsTransparent:)]) {
      [self setTitlebarAppearsTransparent:YES];
    }

    [self updateTitlebarGradientViewPresence];
  }
  return self;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)dealloc {
  [mTitlebarGradientView release];
  [super dealloc];
}

- (NSArray<NSView*>*)contentViewContents {
  NSMutableArray<NSView*>* contents = [[[self contentView] subviews] mutableCopy];
  if (mTitlebarGradientView) {
    // Do not include the titlebar gradient view in the returned array.
    [contents removeObject:mTitlebarGradientView];
  }
  return [contents autorelease];
}

- (void)updateTitlebarGradientViewPresence {
  BOOL needTitlebarView = ![self drawsContentsIntoWindowFrame];
  if (needTitlebarView && !mTitlebarGradientView) {
    mTitlebarGradientView = [[TitlebarGradientView alloc] initWithFrame:[self titlebarRect]];
    mTitlebarGradientView.autoresizingMask = NSViewWidthSizable | NSViewMinYMargin;
    [self.contentView addSubview:mTitlebarGradientView];
  } else if (!needTitlebarView && mTitlebarGradientView) {
    [mTitlebarGradientView removeFromSuperview];
    [mTitlebarGradientView release];
    mTitlebarGradientView = nil;
  }
}

- (void)windowMainStateChanged {
  [self setTitlebarNeedsDisplay];
}

- (void)setTitlebarNeedsDisplay {
  [mTitlebarGradientView setNeedsDisplay:YES];
}

- (NSRect)titlebarRect {
  CGFloat titlebarHeight = [self titlebarHeight];
  return NSMakeRect(0, [self frame].size.height - titlebarHeight, [self frame].size.width,
                    titlebarHeight);
}

// Returns the unified height of titlebar + toolbar.
- (CGFloat)unifiedToolbarHeight {
  return mUnifiedToolbarHeight;
}

- (CGFloat)titlebarHeight {
  // We use the original content rect here, not what we return from
  // [self contentRectForFrameRect:], because that would give us a
  // titlebarHeight of zero.
  NSRect frameRect = [self frame];
  NSUInteger styleMask = [self styleMask];
  NSRect originalContentRect = [NSWindow contentRectForFrameRect:frameRect styleMask:styleMask];
  return NSMaxY(frameRect) - NSMaxY(originalContentRect);
}

// Stores the complete height of titlebar + toolbar.
- (void)setUnifiedToolbarHeight:(CGFloat)aHeight {
  if (aHeight == mUnifiedToolbarHeight) return;

  mUnifiedToolbarHeight = aHeight;

  if (![self drawsContentsIntoWindowFrame]) {
    [self setTitlebarNeedsDisplay];
  }
}

// Extending the content area into the title bar works by resizing the
// mainChildView so that it covers the titlebar.
- (void)setDrawsContentsIntoWindowFrame:(BOOL)aState {
  BOOL stateChanged = ([self drawsContentsIntoWindowFrame] != aState);
  [super setDrawsContentsIntoWindowFrame:aState];
  if (stateChanged && [[self delegate] isKindOfClass:[WindowDelegate class]]) {
    // Here we extend / shrink our mainChildView. We do that by firing a resize
    // event which will cause the ChildView to be resized to the rect returned
    // by nsCocoaWindow::GetClientBounds. GetClientBounds bases its return
    // value on what we return from drawsContentsIntoWindowFrame.
    WindowDelegate* windowDelegate = (WindowDelegate*)[self delegate];
    nsCocoaWindow* geckoWindow = [windowDelegate geckoWidget];
    if (geckoWindow) {
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

  [self updateTitlebarGradientViewPresence];
}

- (void)setWantsTitleDrawn:(BOOL)aDrawTitle {
  [super setWantsTitleDrawn:aDrawTitle];
  [self setTitlebarNeedsDisplay];
}

- (void)setSheetAttachmentPosition:(CGFloat)aY {
  mSheetAttachmentPosition = aY;
}

- (CGFloat)sheetAttachmentPosition {
  return mSheetAttachmentPosition;
}

- (void)placeWindowButtons:(NSRect)aRect {
  if (!NSEqualRects(mWindowButtonsRect, aRect)) {
    mWindowButtonsRect = aRect;
    [self reflowTitlebarElements];
  }
}

- (NSPoint)windowButtonsPositionWithDefaultPosition:(NSPoint)aDefaultPosition {
  NSInteger styleMask = [self styleMask];
  if ([self drawsContentsIntoWindowFrame] && !(styleMask & NSFullScreenWindowMask) &&
      (styleMask & NSTitledWindowMask)) {
    if (NSIsEmptyRect(mWindowButtonsRect)) {
      // Empty rect. Let's hide the buttons.
      // Position is in non-flipped window coordinates. Using frame's height
      // for the vertical coordinate will move the buttons above the window,
      // making them invisible.
      return NSMakePoint(0, [self frame].size.height);
    }
    return NSMakePoint(mWindowButtonsRect.origin.x, mWindowButtonsRect.origin.y);
  }
  return aDefaultPosition;
}

- (void)placeFullScreenButton:(NSRect)aRect {
  if (!NSEqualRects(mFullScreenButtonRect, aRect)) {
    mFullScreenButtonRect = aRect;
    [self reflowTitlebarElements];
  }
}

- (NSPoint)fullScreenButtonPositionWithDefaultPosition:(NSPoint)aDefaultPosition {
  if ([self drawsContentsIntoWindowFrame] && !NSIsEmptyRect(mFullScreenButtonRect)) {
    return NSMakePoint(std::min(mFullScreenButtonRect.origin.x, aDefaultPosition.x),
                       std::min(mFullScreenButtonRect.origin.y, aDefaultPosition.y));
  }
  return aDefaultPosition;
}

// Returning YES here makes the setShowsToolbarButton method work even though
// the window doesn't contain an NSToolbar.
- (BOOL)_hasToolbar {
  return YES;
}

// Dispatch a toolbar pill button clicked message to Gecko.
- (void)_toolbarPillButtonClicked:(id)sender {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  RollUpPopups();

  if ([[self delegate] isKindOfClass:[WindowDelegate class]]) {
    WindowDelegate* windowDelegate = (WindowDelegate*)[self delegate];
    nsCocoaWindow* geckoWindow = [windowDelegate geckoWidget];
    if (!geckoWindow) return;

    nsIWidgetListener* listener = geckoWindow->GetWidgetListener();
    if (listener) listener->OSToolbarButtonPressed();
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Retain and release "self" to avoid crashes when our widget (and its native
// window) is closed as a result of processing a key equivalent (e.g.
// Command+w or Command+q).  This workaround is only needed for a window
// that can become key.
- (BOOL)performKeyEquivalent:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NSWindow* nativeWindow = [self retain];
  BOOL retval = [super performKeyEquivalent:theEvent];
  [nativeWindow release];
  return retval;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (void)sendEvent:(NSEvent*)anEvent {
  NSEventType type = [anEvent type];

  switch (type) {
    case NSScrollWheel:
    case NSLeftMouseDown:
    case NSLeftMouseUp:
    case NSRightMouseDown:
    case NSRightMouseUp:
    case NSOtherMouseDown:
    case NSOtherMouseUp:
    case NSMouseMoved:
    case NSLeftMouseDragged:
    case NSRightMouseDragged:
    case NSOtherMouseDragged: {
      // Drop all mouse events if a modal window has appeared above us.
      // This helps make us behave as if the OS were running a "real" modal
      // event loop.
      id delegate = [self delegate];
      if (delegate && [delegate isKindOfClass:[WindowDelegate class]]) {
        nsCocoaWindow* widget = [(WindowDelegate*)delegate geckoWidget];
        if (widget) {
          if (gGeckoAppModalWindowList && (widget != gGeckoAppModalWindowList->window)) return;
          if (widget->HasModalDescendents()) return;
        }
      }
      break;
    }
    default:
      break;
  }

  [super sendEvent:anEvent];
}

@end

@implementation PopupWindow

- (id)initWithContentRect:(NSRect)contentRect
                styleMask:(NSUInteger)styleMask
                  backing:(NSBackingStoreType)bufferingType
                    defer:(BOOL)deferCreation {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  mIsContextMenu = false;
  return [super initWithContentRect:contentRect
                          styleMask:styleMask
                            backing:bufferingType
                              defer:deferCreation];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

// Override the private API _backdropBleedAmount. This determines how much the
// desktop wallpaper contributes to the vibrancy backdrop.
// Return 0 in order to match what the system does for sheet windows and
// _NSPopoverWindows.
- (CGFloat)_backdropBleedAmount {
  return 0.0;
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
  NSEventType type = [anEvent type];

  switch (type) {
    case NSScrollWheel:
    case NSLeftMouseDown:
    case NSLeftMouseUp:
    case NSRightMouseDown:
    case NSRightMouseUp:
    case NSOtherMouseDown:
    case NSOtherMouseUp:
    case NSMouseMoved:
    case NSLeftMouseDragged:
    case NSRightMouseDragged:
    case NSOtherMouseDragged: {
      // Drop all mouse events if a modal window has appeared above us.
      // This helps make us behave as if the OS were running a "real" modal
      // event loop.
      id delegate = [self delegate];
      if (delegate && [delegate isKindOfClass:[WindowDelegate class]]) {
        nsCocoaWindow* widget = [(WindowDelegate*)delegate geckoWidget];
        if (widget) {
          if (gGeckoAppModalWindowList && (widget != gGeckoAppModalWindowList->window)) return;
          if (widget->HasModalDescendents()) return;
        }
      }
      break;
    }
    default:
      break;
  }

  [super sendEvent:anEvent];
}

// Apple's doc on this method says that the NSWindow class's default is not to
// become main if the window isn't "visible" -- so we should replicate that
// behavior here.  As best I can tell, the [NSWindow isVisible] method is an
// accurate test of what Apple means by "visibility".
- (BOOL)canBecomeMainWindow {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (![self isVisible]) return NO;
  return YES;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

// Retain and release "self" to avoid crashes when our widget (and its native
// window) is closed as a result of processing a key equivalent (e.g.
// Command+w or Command+q).  This workaround is only needed for a window
// that can become key.
- (BOOL)performKeyEquivalent:(NSEvent*)theEvent {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NSWindow* nativeWindow = [self retain];
  BOOL retval = [super performKeyEquivalent:theEvent];
  [nativeWindow release];
  return retval;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

@end
