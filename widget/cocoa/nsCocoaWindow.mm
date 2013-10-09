/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCocoaWindow.h"

#include "nsObjCExceptions.h"
#include "nsCOMPtr.h"
#include "nsWidgetsCID.h"
#include "nsIRollupListener.h"
#include "nsChildView.h"
#include "TextInputHandler.h"
#include "nsWindowMap.h"
#include "nsAppShell.h"
#include "nsIAppShellService.h"
#include "nsIBaseWindow.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIXULWindow.h"
#include "nsToolkit.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMElement.h"
#include "nsThreadUtils.h"
#include "nsMenuBarX.h"
#include "nsMenuUtilsX.h"
#include "nsStyleConsts.h"
#include "nsNativeThemeColors.h"
#include "nsChildView.h"
#include "nsCocoaFeatures.h"
#include "nsIScreenManager.h"
#include "nsIWidgetListener.h"
#include "nsIPresShell.h"

#include "gfxPlatform.h"
#include "qcms.h"

#include "mozilla/AutoRestore.h"
#include "mozilla/BasicEvents.h"
#include "mozilla/Preferences.h"
#include <algorithm>

namespace mozilla {
namespace layers {
class LayerManager;
}
}
using namespace mozilla::layers;
using namespace mozilla::widget;
using namespace mozilla;

// defined in nsAppShell.mm
extern nsCocoaAppModalWindowList *gCocoaAppModalWindowList;

int32_t gXULModalLevel = 0;

// In principle there should be only one app-modal window at any given time.
// But sometimes, despite our best efforts, another window appears above the
// current app-modal window.  So we need to keep a linked list of app-modal
// windows.  (A non-sheet window that appears above an app-modal window is
// also made app-modal.)  See nsCocoaWindow::SetModal().
nsCocoaWindowList *gGeckoAppModalWindowList = NULL;

// defined in nsMenuBarX.mm
extern NSMenu* sApplicationMenu; // Application menu shared by all menubars

// defined in nsChildView.mm
extern BOOL                gSomeMenuBarPainted;

extern "C" {
  // CGSPrivate.h
  typedef NSInteger CGSConnection;
  typedef NSInteger CGSWindow;
  typedef NSUInteger CGSWindowFilterRef;
  extern CGSConnection _CGSDefaultConnection(void);
  extern CGError CGSSetWindowShadowAndRimParameters(const CGSConnection cid, CGSWindow wid, float standardDeviation, float density, int offsetX, int offsetY, unsigned int flags);
  extern CGError CGSNewCIFilterByName(CGSConnection cid, CFStringRef filterName, CGSWindowFilterRef *outFilter);
  extern CGError CGSSetCIFilterValuesFromDictionary(CGSConnection cid, CGSWindowFilterRef filter, CFDictionaryRef filterValues);
  extern CGError CGSAddWindowFilter(CGSConnection cid, CGSWindow wid, CGSWindowFilterRef filter, NSInteger flags);
  extern CGError CGSRemoveWindowFilter(CGSConnection cid, CGSWindow wid, CGSWindowFilterRef filter);
  extern CGError CGSReleaseCIFilter(CGSConnection cid, CGSWindowFilterRef filter);
}

#define NS_APPSHELLSERVICE_CONTRACTID "@mozilla.org/appshell/appShellService;1"

NS_IMPL_ISUPPORTS_INHERITED1(nsCocoaWindow, Inherited, nsPIWidgetCocoa)

// A note on testing to see if your object is a sheet...
// |mWindowType == eWindowType_sheet| is true if your gecko nsIWidget is a sheet
// widget - whether or not the sheet is showing. |[mWindow isSheet]| will return
// true *only when the sheet is actually showing*. Choose your test wisely.

static void RollUpPopups()
{
  nsIRollupListener* rollupListener = nsBaseWidget::GetActiveRollupListener();
  NS_ENSURE_TRUE_VOID(rollupListener);
  nsCOMPtr<nsIWidget> rollupWidget = rollupListener->GetRollupWidget();
  if (!rollupWidget)
    return;
  rollupListener->Rollup(0, nullptr);
}

nsCocoaWindow::nsCocoaWindow()
: mParent(nullptr)
, mWindow(nil)
, mDelegate(nil)
, mSheetWindowParent(nil)
, mPopupContentView(nil)
, mShadowStyle(NS_STYLE_WINDOW_SHADOW_DEFAULT)
, mWindowFilter(0)
, mBackingScaleFactor(0.0)
, mAnimationType(nsIWidget::eGenericWindowAnimation)
, mWindowMadeHere(false)
, mSheetNeedsShow(false)
, mFullScreen(false)
, mInFullScreenTransition(false)
, mModal(false)
, mUsesNativeFullScreen(false)
, mIsAnimationSuppressed(false)
, mInReportMoveEvent(false)
, mNumModalDescendents(0)
{

}

void nsCocoaWindow::DestroyNativeWindow()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mWindow)
    return;

  CleanUpWindowFilter();
  // We want to unhook the delegate here because we don't want events
  // sent to it after this object has been destroyed.
  [mWindow setDelegate:nil];
  [mWindow close];
  mWindow = nil;
  [mDelegate autorelease];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsCocoaWindow::~nsCocoaWindow()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Notify the children that we're gone.  Popup windows (e.g. tooltips) can
  // have nsChildView children.  'kid' is an nsChildView object if and only if
  // its 'type' is 'eWindowType_child' or 'eWindowType_plugin'.
  // childView->ResetParent() can change our list of children while it's
  // being iterated, so the way we iterate the list must allow for this.
  for (nsIWidget* kid = mLastChild; kid;) {
    nsWindowType kidType;
    kid->GetWindowType(kidType);
    if (kidType == eWindowType_child || kidType == eWindowType_plugin) {
      nsChildView* childView = static_cast<nsChildView*>(kid);
      kid = kid->GetPrevSibling();
      childView->ResetParent();
    } else {
      nsCocoaWindow* childWindow = static_cast<nsCocoaWindow*>(kid);
      childWindow->mParent = nullptr;
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
    NS_ASSERTION(gXULModalLevel >= 0, "Wierdness setting modality!");
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Find the screen that overlaps aRect the most,
// if none are found default to the mainScreen.
static NSScreen *FindTargetScreenForRect(const nsIntRect& aRect)
{
  NSScreen *targetScreen = [NSScreen mainScreen];
  NSEnumerator *screenEnum = [[NSScreen screens] objectEnumerator];
  int largestIntersectArea = 0;
  while (NSScreen *screen = [screenEnum nextObject]) {
    nsIntRect screenRect(nsCocoaUtils::CocoaRectToGeckoRect([screen visibleFrame]));
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
// NB: this operates with aRect in global display pixels
static void FitRectToVisibleAreaForScreen(nsIntRect &aRect, NSScreen *aScreen,
                                          bool aUsesNativeFullScreen)
{
  if (!aScreen) {
    aScreen = FindTargetScreenForRect(aRect);
  }

  nsIntRect screenBounds(nsCocoaUtils::CocoaRectToGeckoRect([aScreen visibleFrame]));

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

  // If aRect is filling the screen and the window supports native (Lion-style)
  // fullscreen mode, reduce aRect's height and shift it down by 22 pixels
  // (nominally the height of the menu bar or of a window's title bar).  For
  // some reason this works around bug 740923.  Yes, it's a bodacious hack.
  // But until we know more it will have to do.
  if (aUsesNativeFullScreen && aRect.y == 0 && aRect.height == screenBounds.height) {
    aRect.y = 22;
    aRect.height -= 22;
  }
}

// Some applications like Camino use native popup windows
// (native context menus, native tooltips)
static bool UseNativePopupWindows()
{
#ifdef MOZ_USE_NATIVE_POPUP_WINDOWS
  return true;
#else
  return false;
#endif /* MOZ_USE_NATIVE_POPUP_WINDOWS */
}

// aRect here is specified in global display pixels
nsresult nsCocoaWindow::Create(nsIWidget *aParent,
                               nsNativeWidget aNativeParent,
                               const nsIntRect &aRect,
                               nsDeviceContext *aContext,
                               nsWidgetInitData *aInitData)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // Because the hidden window is created outside of an event loop,
  // we have to provide an autorelease pool (see bug 559075).
  nsAutoreleasePool localPool;

  nsIntRect newBounds = aRect;
  FitRectToVisibleAreaForScreen(newBounds, nullptr, mUsesNativeFullScreen);

  // Set defaults which can be overriden from aInitData in BaseCreate
  mWindowType = eWindowType_toplevel;
  mBorderStyle = eBorderStyle_default;

  // Ensure that the toolkit is created.
  nsToolkit::GetToolkit();

  // newBounds is still display (global screen) pixels at this point;
  // fortunately, BaseCreate doesn't actually use it so we don't
  // need to worry about trying to convert it to device pixels
  // when we don't have a window (or dev context, perhaps) yet
  Inherited::BaseCreate(aParent, newBounds, aContext, aInitData);

  mParent = aParent;

  // Applications that use native popups don't want us to create popup windows.
  if ((mWindowType == eWindowType_popup) && UseNativePopupWindows())
    return NS_OK;

  nsresult rv =
    CreateNativeWindow(nsCocoaUtils::GeckoRectToCocoaRect(newBounds),
                       mBorderStyle, false);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mWindowType == eWindowType_popup) {
    if (aInitData->mIsDragPopup) {
      [mWindow setIgnoresMouseEvents:YES];
    }
    // now we can convert newBounds to device pixels for the window we created,
    // as the child view expects a rect expressed in the dev pix of its parent
    double scale = BackingScaleFactor();
    newBounds.x *= scale;
    newBounds.y *= scale;
    newBounds.width *= scale;
    newBounds.height *= scale;
    return CreatePopupContentView(newBounds, aContext);
  }

  mIsAnimationSuppressed = aInitData->mIsAnimationSuppressed;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

static unsigned int WindowMaskForBorderStyle(nsBorderStyle aBorderStyle)
{
  bool allOrDefault = (aBorderStyle == eBorderStyle_all ||
                         aBorderStyle == eBorderStyle_default);

  /* Apple's docs on NSWindow styles say that "a window's style mask should
   * include NSTitledWindowMask if it includes any of the others [besides
   * NSBorderlessWindowMask]".  This implies that a borderless window
   * shouldn't have any other styles than NSBorderlessWindowMask.
   */
  if (!allOrDefault && !(aBorderStyle & eBorderStyle_title))
    return NSBorderlessWindowMask;

  unsigned int mask = NSTitledWindowMask;
  if (allOrDefault || aBorderStyle & eBorderStyle_close)
    mask |= NSClosableWindowMask;
  if (allOrDefault || aBorderStyle & eBorderStyle_minimize)
    mask |= NSMiniaturizableWindowMask;
  if (allOrDefault || aBorderStyle & eBorderStyle_resizeh)
    mask |= NSResizableWindowMask;

  return mask;
}

NS_IMETHODIMP nsCocoaWindow::ReparentNativeWidget(nsIWidget* aNewParent)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// If aRectIsFrameRect, aRect specifies the frame rect of the new window.
// Otherwise, aRect.x/y specify the position of the window's frame relative to
// the bottom of the menubar and aRect.width/height specify the size of the
// content rect.
nsresult nsCocoaWindow::CreateNativeWindow(const NSRect &aRect,
                                           nsBorderStyle aBorderStyle,
                                           bool aRectIsFrameRect)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // We default to NSBorderlessWindowMask, add features if needed.
  unsigned int features = NSBorderlessWindowMask;

  // Configure the window we will create based on the window type.
  switch (mWindowType)
  {
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
      nsWindowType parentType;
      mParent->GetWindowType(parentType);
      if (parentType != eWindowType_invisible &&
          aBorderStyle & eBorderStyle_resizeh) {
        features = NSResizableWindowMask;
      }
      else {
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

    if (mWindowType != eWindowType_popup)
      contentRect.origin.y -= [[NSApp mainMenu] menuBarHeight];
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
  mWindow = [[windowClass alloc] initWithContentRect:contentRect styleMask:features 
                                 backing:NSBackingStoreBuffered defer:YES];

  // setup our notification delegate. Note that setDelegate: does NOT retain.
  mDelegate = [[WindowDelegate alloc] initWithGeckoWindow:this];
  [mWindow setDelegate:mDelegate];

  // Make sure that the content rect we gave has been honored.
  NSRect wantedFrame = [mWindow frameRectForContentRect:contentRect];
  if (!NSEqualRects([mWindow frame], wantedFrame)) {
    // This can happen when the window is not on the primary screen.
    [mWindow setFrame:wantedFrame display:NO];
  }
  UpdateBounds();

  if (mWindowType == eWindowType_invisible) {
    [mWindow setLevel:kCGDesktopWindowLevelKey];
  } else if (mWindowType == eWindowType_popup) {
    SetPopupWindowLevel();
    [mWindow setHasShadow:YES];
  }

  [mWindow setBackgroundColor:[NSColor clearColor]];
  [mWindow setOpaque:NO];
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

NS_IMETHODIMP nsCocoaWindow::CreatePopupContentView(const nsIntRect &aRect,
                             nsDeviceContext *aContext)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // We need to make our content view a ChildView.
  mPopupContentView = new nsChildView();
  if (!mPopupContentView)
    return NS_ERROR_FAILURE;

  NS_ADDREF(mPopupContentView);

  nsIWidget* thisAsWidget = static_cast<nsIWidget*>(this);
  mPopupContentView->Create(thisAsWidget, nullptr, aRect, aContext, nullptr);

  ChildView* newContentView = (ChildView*)mPopupContentView->GetNativeData(NS_NATIVE_WIDGET);
  [mWindow setContentView:newContentView];

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsCocoaWindow::Destroy()
{
  // If we don't hide here we run into problems with panels, this is not ideal.
  // (Bug 891424)
  Show(false);

  if (mPopupContentView)
    mPopupContentView->Destroy();

  nsBaseWidget::Destroy();
  // nsBaseWidget::Destroy() calls GetParent()->RemoveChild(this). But we
  // don't implement GetParent(), so we need to do the equivalent here.
  if (mParent) {
    mParent->RemoveChild(this);
  }
  nsBaseWidget::OnDestroy();

  if (mFullScreen) {
    // On Lion we don't have to mess with the OS chrome when in Full Screen
    // mode.  But we do have to destroy the native window here (and not wait
    // for that to happen in our destructor).  We don't switch away from the
    // native window's space until the window is destroyed, and otherwise this
    // might not happen for several seconds (because at least one object
    // holding a reference to ourselves is usually waiting to be garbage-
    // collected).  See bug 757618.
    if (mUsesNativeFullScreen) {
      DestroyNativeWindow();
    } else if (mWindow) {
      nsCocoaUtils::HideOSChromeOnScreen(false, [mWindow screen]);
    }
  }

  return NS_OK;
}

nsIWidget* nsCocoaWindow::GetSheetWindowParent(void)
{
  if (mWindowType != eWindowType_sheet)
    return nullptr;
  nsCocoaWindow *parent = static_cast<nsCocoaWindow*>(mParent);
  while (parent && (parent->mWindowType == eWindowType_sheet))
    parent = static_cast<nsCocoaWindow*>(parent->mParent);
  return parent;
}

void* nsCocoaWindow::GetNativeData(uint32_t aDataType)
{
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
  }

  return retVal;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSNULL;
}

bool nsCocoaWindow::IsVisible() const
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  return (mWindow && ([mWindow isVisible] || mSheetNeedsShow));

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(false);
}

NS_IMETHODIMP nsCocoaWindow::SetModal(bool aState)
{
  if (!mWindow)
    return NS_OK;

  // This is used during startup (outside the event loop) when creating
  // the add-ons compatibility checking dialog and the profile manager UI;
  // therefore, it needs to provide an autorelease pool to avoid cocoa
  // objects leaking.
  nsAutoreleasePool localPool;

  mModal = aState;
  nsCocoaWindow *aParent = static_cast<nsCocoaWindow*>(mParent);
  if (aState) {
    ++gXULModalLevel;
    if (gCocoaAppModalWindowList)
      gCocoaAppModalWindowList->PushGecko(mWindow, this);
    // When a non-sheet window gets "set modal", make the window(s) that it
    // appears over behave as they should.  We can't rely on native methods to
    // do this, for the following reason:  The OS runs modal non-sheet windows
    // in an event loop (using [NSApplication runModalForWindow:] or similar
    // methods) that's incompatible with the modal event loop in nsXULWindow::
    // ShowModal() (each of these event loops is "exclusive", and can't run at
    // the same time as other (similar) event loops).
    if (mWindowType != eWindowType_sheet) {
      while (aParent) {
        if (aParent->mNumModalDescendents++ == 0) {
          NSWindow *aWindow = aParent->GetCocoaWindow();
          if (aParent->mWindowType != eWindowType_invisible) {
            [[aWindow standardWindowButton:NSWindowCloseButton] setEnabled:NO];
            [[aWindow standardWindowButton:NSWindowMiniaturizeButton] setEnabled:NO];
            [[aWindow standardWindowButton:NSWindowZoomButton] setEnabled:NO];
          }
        }
        aParent = static_cast<nsCocoaWindow*>(aParent->mParent);
      }
      [mWindow setLevel:NSModalPanelWindowLevel];
      nsCocoaWindowList *windowList = new nsCocoaWindowList;
      if (windowList) {
        windowList->window = this; // Don't ADDREF
        windowList->prev = gGeckoAppModalWindowList;
        gGeckoAppModalWindowList = windowList;
      }
    }
  }
  else {
    --gXULModalLevel;
    NS_ASSERTION(gXULModalLevel >= 0, "Mismatched call to nsCocoaWindow::SetModal(false)!");
    if (gCocoaAppModalWindowList)
      gCocoaAppModalWindowList->PopGecko(mWindow, this);
    if (mWindowType != eWindowType_sheet) {
      while (aParent) {
        if (--aParent->mNumModalDescendents == 0) {
          NSWindow *aWindow = aParent->GetCocoaWindow();
          if (aParent->mWindowType != eWindowType_invisible) {
            [[aWindow standardWindowButton:NSWindowCloseButton] setEnabled:YES];
            [[aWindow standardWindowButton:NSWindowMiniaturizeButton] setEnabled:YES];
            [[aWindow standardWindowButton:NSWindowZoomButton] setEnabled:YES];
          }
        }
        NS_ASSERTION(aParent->mNumModalDescendents >= 0, "Widget hierarchy changed while modal!");
        aParent = static_cast<nsCocoaWindow*>(aParent->mParent);
      }
      if (gGeckoAppModalWindowList) {
        NS_ASSERTION(gGeckoAppModalWindowList->window == this, "Widget hierarchy changed while modal!");
        nsCocoaWindowList *saved = gGeckoAppModalWindowList;
        gGeckoAppModalWindowList = gGeckoAppModalWindowList->prev;
        delete saved; // "window" not ADDREFed
      }
      if (mWindowType == eWindowType_popup)
        SetPopupWindowLevel();
      else
        [mWindow setLevel:NSNormalWindowLevel];
    }
  }
  return NS_OK;
}

// Hide or show this window
NS_IMETHODIMP nsCocoaWindow::Show(bool bState)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!mWindow)
    return NS_OK;

  // We need to re-execute sometimes in order to bring already-visible
  // windows forward.
  if (!mSheetNeedsShow && !bState && ![mWindow isVisible])
    return NS_OK;

  nsIWidget* parentWidget = mParent;
  nsCOMPtr<nsPIWidgetCocoa> piParentWidget(do_QueryInterface(parentWidget));
  NSWindow* nativeParentWindow = (parentWidget) ?
    (NSWindow*)parentWidget->GetNativeData(NS_NATIVE_WINDOW) : nil;

  if (bState && !mBounds.IsEmpty()) {
    if (mWindowType == eWindowType_sheet) {
      // bail if no parent window (its basically what we do in Carbon)
      if (!nativeParentWindow || !piParentWidget)
        return NS_ERROR_FAILURE;

      NSWindow* topNonSheetWindow = nativeParentWindow;
      
      // If this sheet is the child of another sheet, hide the parent so that
      // this sheet can be displayed. Leave the parent mSheetNeedsShow alone,
      // that is only used to handle sibling sheet contention. The parent will
      // return once there are no more child sheets.
      bool parentIsSheet = false;
      if (NS_SUCCEEDED(piParentWidget->GetIsSheet(&parentIsSheet)) &&
          parentIsSheet) {
        piParentWidget->GetSheetWindowParent(&topNonSheetWindow);
        [NSApp endSheet:nativeParentWindow];
      }

      nsCocoaWindow* sheetShown = nullptr;
      if (NS_SUCCEEDED(piParentWidget->GetChildSheet(true, &sheetShown)) &&
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
      }
      else {
        // A sibling of this sheet is active, don't show this sheet yet.
        // When the active sheet hides, its brothers and sisters that have
        // mSheetNeedsShow set will have their opportunities to display.
        mSheetNeedsShow = true;
      }
    }
    else if (mWindowType == eWindowType_popup) {
      // If a popup window is shown after being hidden, it needs to be "reset"
      // for it to receive any mouse events aside from mouse-moved events
      // (because it was removed from the "window cache" when it was hidden
      // -- see below).  Setting the window number to -1 and then back to its
      // original value seems to accomplish this.  The idea was "borrowed"
      // from the Java Embedding Plugin.
      NSInteger windowNumber = [mWindow windowNumber];
      [mWindow _setWindowNumber:-1];
      [mWindow _setWindowNumber:windowNumber];
      // For reasons that aren't yet clear, calls to [NSWindow orderFront:] or
      // [NSWindow makeKeyAndOrderFront:] can sometimes trigger "Error (1000)
      // creating CGSWindow", which in turn triggers an internal inconsistency
      // NSException.  These errors shouldn't be fatal.  So we need to wrap
      // calls to ...orderFront: in LOGONLY blocks.  See bmo bug 470864.
      NS_OBJC_BEGIN_TRY_LOGONLY_BLOCK;
      [mWindow orderFront:nil];
      NS_OBJC_END_TRY_LOGONLY_BLOCK;
      SendSetZLevelEvent();
      AdjustWindowShadow();
      SetUpWindowFilter();
      // If our popup window is a non-native context menu, tell the OS (and
      // other programs) that a menu has opened.  This is how the OS knows to
      // close other programs' context menus when ours open.
      if ([mWindow isKindOfClass:[PopupWindow class]] &&
          [(PopupWindow*) mWindow isContextMenu]) {
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
        [nativeParentWindow addChildWindow:mWindow
                            ordered:NSWindowAbove];
    }
    else {
      NS_OBJC_BEGIN_TRY_LOGONLY_BLOCK;
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
              NS_NOTREACHED("unexpected mAnimationType value");
              // fall through
            case nsIWidget::eGenericWindowAnimation:
              behavior = NSWindowAnimationBehaviorDefault;
              break;
          }
        }
        [mWindow setAnimationBehavior:behavior];
      }
      [mWindow makeKeyAndOrderFront:nil];
      NS_OBJC_END_TRY_LOGONLY_BLOCK;
      SendSetZLevelEvent();
    }
  }
  else {
    // roll up any popups if a top-level window is going away
    if (mWindowType == eWindowType_toplevel || mWindowType == eWindowType_dialog)
      RollUpPopups();

    // now get rid of the window/sheet
    if (mWindowType == eWindowType_sheet) {
      if (mSheetNeedsShow) {
        // This is an attempt to hide a sheet that never had a chance to
        // be shown. There's nothing to do other than make sure that it
        // won't show.
        mSheetNeedsShow = false;
      }
      else {
        // get sheet's parent *before* hiding the sheet (which breaks the linkage)
        NSWindow* sheetParent = mSheetWindowParent;
        
        // hide the sheet
        [NSApp endSheet:mWindow];
        
        [TopLevelWindowData deactivateInWindow:mWindow];

        nsCocoaWindow* siblingSheetToShow = nullptr;
        bool parentIsSheet = false;

        if (nativeParentWindow && piParentWidget &&
            NS_SUCCEEDED(piParentWidget->GetChildSheet(false, &siblingSheetToShow)) &&
            siblingSheetToShow) {
          // First, give sibling sheets an opportunity to show.
          siblingSheetToShow->Show(true);
        }
        else if (nativeParentWindow && piParentWidget &&
                 NS_SUCCEEDED(piParentWidget->GetIsSheet(&parentIsSheet)) &&
                 parentIsSheet) {
          // Only set contextInfo if the parent of the parent sheet we're about
          // to restore isn't itself a sheet.
          NSWindow* contextInfo = sheetParent;
          nsIWidget* grandparentWidget = nil;
          if (NS_SUCCEEDED(piParentWidget->GetRealParent(&grandparentWidget)) && grandparentWidget) {
            nsCOMPtr<nsPIWidgetCocoa> piGrandparentWidget(do_QueryInterface(grandparentWidget));
            bool grandparentIsSheet = false;
            if (piGrandparentWidget && NS_SUCCEEDED(piGrandparentWidget->GetIsSheet(&grandparentIsSheet)) &&
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
        }
        else {
          // Sheet, that was hard.  No more siblings or parents, going back
          // to a real window.
          NS_OBJC_BEGIN_TRY_LOGONLY_BLOCK;
          [sheetParent makeKeyAndOrderFront:nil];
          NS_OBJC_END_TRY_LOGONLY_BLOCK;
        }
        SendSetZLevelEvent();
      }
    }
    else {
      // If the window is a popup window with a parent window we need to
      // unhook it here before ordering it out. When you order out the child
      // of a window it hides the parent window.
      if (mWindowType == eWindowType_popup && nativeParentWindow)
        [nativeParentWindow removeChildWindow:mWindow];

      CleanUpWindowFilter();
      [mWindow orderOut:nil];
      // Unless it's explicitly removed from NSApp's "window cache", a popup
      // window will keep receiving mouse-moved events even after it's been
      // "ordered out" (instead of the browser window that was underneath it,
      // until you click on that window).  This is bmo bug 378645, but it's
      // surely an Apple bug.  The "window cache" is an undocumented subsystem,
      // all of whose methods are included in the NSWindowCache category of
      // the NSApplication class (in header files generated using class-dump).
      // This workaround was "borrowed" from the Java Embedding Plugin (which
      // uses it for a different purpose).
      if (mWindowType == eWindowType_popup)
        [NSApp _removeWindowFromCache:mWindow];

      // If our popup window is a non-native context menu, tell the OS (and
      // other programs) that a menu has closed.
      if ([mWindow isKindOfClass:[PopupWindow class]] &&
          [(PopupWindow*) mWindow isContextMenu]) {
        [[NSDistributedNotificationCenter defaultCenter]
          postNotificationName:@"com.apple.HIToolbox.endMenuTrackingNotification"
                        object:@"org.mozilla.gecko.PopupWindow"];
      }
    }
  }
  
  if (mPopupContentView)
      mPopupContentView->Show(bState);

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
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
static const ShadowParams kWindowShadowParameters[] = {
  { 0.0f, 0.0f, 0, 0, 0 },        // none
  { 8.0f, 0.5f, 0, 6, 1 },        // default
  { 10.0f, 0.44f, 0, 10, 512 },   // menu
  { 8.0f, 0.5f, 0, 6, 1 },        // tooltip
  { 4.0f, 0.6f, 0, 4, 512 }       // sheet
};

// This method will adjust the window shadow style for popup windows after
// they have been made visible. Before they're visible, their window number
// might be -1, which is not useful.
// We won't attempt to change the shadow for windows that can acquire key state
// since OS X will reset the shadow whenever that happens.
void
nsCocoaWindow::AdjustWindowShadow()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mWindow || ![mWindow isVisible] || ![mWindow hasShadow] ||
      [mWindow canBecomeKeyWindow] || [mWindow windowNumber] == -1)
    return;

  const ShadowParams& params = kWindowShadowParameters[mShadowStyle];
  CGSConnection cid = _CGSDefaultConnection();
  CGSSetWindowShadowAndRimParameters(cid, [mWindow windowNumber],
                                     params.standardDeviation, params.density,
                                     params.offsetX, params.offsetY,
                                     params.flags);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsCocoaWindow::SetUpWindowFilter()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mWindow || ![mWindow isVisible] || [mWindow windowNumber] == -1)
    return;

  CleanUpWindowFilter();

  // Only blur the background of menus and fake sheets, but not on PPC
  // because it results in blank windows (bug 547723).
#ifndef __ppc__
  if (mShadowStyle != NS_STYLE_WINDOW_SHADOW_MENU &&
      mShadowStyle != NS_STYLE_WINDOW_SHADOW_SHEET)
#endif
    return;

  // Create a CoreImage filter and set it up
  CGSConnection cid = _CGSDefaultConnection();
  CGSNewCIFilterByName(cid, (CFStringRef)@"CIGaussianBlur", &mWindowFilter);
  NSDictionary *options = [NSDictionary dictionaryWithObject:[NSNumber numberWithFloat:2.0] forKey:@"inputRadius"];
  CGSSetCIFilterValuesFromDictionary(cid, mWindowFilter, (CFDictionaryRef)options);

  // Now apply the filter to the window
  NSInteger compositingType = 1 << 0; // Under the window
  CGSAddWindowFilter(cid, [mWindow windowNumber], mWindowFilter, compositingType);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsCocoaWindow::CleanUpWindowFilter()
{
  if (!mWindow || !mWindowFilter || [mWindow windowNumber] == -1)
    return;

  CGSConnection cid = _CGSDefaultConnection();
  CGSRemoveWindowFilter(cid, [mWindow windowNumber], mWindowFilter);
  CGSReleaseCIFilter(cid, mWindowFilter);
  mWindowFilter = 0;
}

nsresult
nsCocoaWindow::ConfigureChildren(const nsTArray<Configuration>& aConfigurations)
{
  if (mPopupContentView) {
    mPopupContentView->ConfigureChildren(aConfigurations);
  }
  return NS_OK;
}

LayerManager*
nsCocoaWindow::GetLayerManager(PLayerTransactionChild* aShadowManager,
                               LayersBackend aBackendHint,
                               LayerManagerPersistence aPersistence,
                               bool* aAllowRetaining)
{
  if (mPopupContentView) {
    return mPopupContentView->GetLayerManager(aShadowManager,
                                              aBackendHint,
                                              aPersistence,
                                              aAllowRetaining);
  }
  return nullptr;
}

nsTransparencyMode nsCocoaWindow::GetTransparencyMode()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  return (!mWindow || [mWindow isOpaque]) ? eTransparencyOpaque : eTransparencyTransparent;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(eTransparencyOpaque);
}

// This is called from nsMenuPopupFrame when making a popup transparent.
// For other window types, nsChildView::SetTransparencyMode is used.
void nsCocoaWindow::SetTransparencyMode(nsTransparencyMode aMode)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mWindow)
    return;

  BOOL isTransparent = aMode == eTransparencyTransparent;
  BOOL currentTransparency = ![mWindow isOpaque];
  if (isTransparent != currentTransparency) {
    [mWindow setOpaque:!isTransparent];
    [mWindow setBackgroundColor:(isTransparent ? [NSColor clearColor] : [NSColor whiteColor])];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

NS_IMETHODIMP nsCocoaWindow::Enable(bool aState)
{
  return NS_OK;
}

bool nsCocoaWindow::IsEnabled() const
{
  return true;
}

#define kWindowPositionSlop 20

NS_IMETHODIMP nsCocoaWindow::ConstrainPosition(bool aAllowSlop,
                                               int32_t *aX, int32_t *aY)
{
  if (!mWindow || ![mWindow screen]) {
    return NS_OK;
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
      screen->GetRectDisplayPix(&(screenBounds.x), &(screenBounds.y),
                                &(screenBounds.width), &(screenBounds.height));
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

  return NS_OK;
}

void nsCocoaWindow::SetSizeConstraints(const SizeConstraints& aConstraints)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Popups can be smaller than (60, 60)
  NSRect rect =
    (mWindowType == eWindowType_popup) ? NSZeroRect : NSMakeRect(0.0, 0.0, 60, 60);
  rect = [mWindow frameRectForContentRect:rect];

  CGFloat scaleFactor = BackingScaleFactor();

  SizeConstraints c = aConstraints;
  c.mMinSize.width =
    std::max(nsCocoaUtils::CocoaPointsToDevPixels(rect.size.width, scaleFactor),
           c.mMinSize.width);
  c.mMinSize.height =
    std::max(nsCocoaUtils::CocoaPointsToDevPixels(rect.size.height, scaleFactor),
           c.mMinSize.height);

  NSSize minSize = {
    nsCocoaUtils::DevPixelsToCocoaPoints(c.mMinSize.width, scaleFactor),
    nsCocoaUtils::DevPixelsToCocoaPoints(c.mMinSize.height, scaleFactor)
  };
  [mWindow setMinSize:minSize];

  NSSize maxSize = {
    c.mMaxSize.width == NS_MAXSIZE ?
      FLT_MAX : nsCocoaUtils::DevPixelsToCocoaPoints(c.mMaxSize.width, scaleFactor),
    c.mMaxSize.height == NS_MAXSIZE ?
      FLT_MAX : nsCocoaUtils::DevPixelsToCocoaPoints(c.mMaxSize.height, scaleFactor)
  };
  [mWindow setMaxSize:maxSize];

  nsBaseWidget::SetSizeConstraints(c);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Coordinates are global display pixels
NS_IMETHODIMP nsCocoaWindow::Move(double aX, double aY)
{
  if (!mWindow) {
    return NS_OK;
  }

  // The point we have is in Gecko coordinates (origin top-left). Convert
  // it to Cocoa ones (origin bottom-left).
  NSPoint coord = {
    static_cast<float>(aX),
    static_cast<float>(nsCocoaUtils::FlippedScreenY(NSToIntRound(aY)))
  };

  NSRect frame = [mWindow frame];
  if (frame.origin.x != coord.x ||
      frame.origin.y + frame.size.height != coord.y) {
    [mWindow setFrameTopLeftPoint:coord];
  }

  return NS_OK;
}

// Position the window behind the given window
NS_METHOD nsCocoaWindow::PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                     nsIWidget *aWidget, bool aActivate)
{
  return NS_OK;
}

NS_METHOD nsCocoaWindow::SetSizeMode(int32_t aMode)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!mWindow)
    return NS_OK;

  // mSizeMode will be updated in DispatchSizeModeEvent, which will be called
  // from a delegate method that handles the state change during one of the
  // calls below.
  nsSizeMode previousMode = mSizeMode;

  if (aMode == nsSizeMode_Normal) {
    if ([mWindow isMiniaturized])
      [mWindow deminiaturize:nil];
    else if (previousMode == nsSizeMode_Maximized && [mWindow isZoomed])
      [mWindow zoom:nil];
  }
  else if (aMode == nsSizeMode_Minimized) {
    if (![mWindow isMiniaturized])
      [mWindow miniaturize:nil];
  }
  else if (aMode == nsSizeMode_Maximized) {
    if ([mWindow isMiniaturized])
      [mWindow deminiaturize:nil];
    if (![mWindow isZoomed])
      [mWindow zoom:nil];
  }
  else if (aMode == nsSizeMode_Fullscreen) {
    if (!mFullScreen)
      MakeFullScreen(true);
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// This has to preserve the window's frame bounds.
// This method requires (as does the Windows impl.) that you call Resize shortly
// after calling HideWindowChrome. See bug 498835 for fixing this.
NS_IMETHODIMP nsCocoaWindow::HideWindowChrome(bool aShouldHide)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!mWindow || !mWindowMadeHere ||
      (mWindowType != eWindowType_toplevel && mWindowType != eWindowType_dialog))
    return NS_ERROR_FAILURE;

  BOOL isVisible = [mWindow isVisible];

  // Remove child windows.
  NSArray* childWindows = [mWindow childWindows];
  NSEnumerator* enumerator = [childWindows objectEnumerator];
  NSWindow* child = nil;
  while ((child = [enumerator nextObject])) {
    [mWindow removeChildWindow:child];
  }

  // Remove the content view.
  NSView* contentView = [mWindow contentView];
  [contentView retain];
  [contentView removeFromSuperviewWithoutNeedingDisplay];

  // Save state (like window title).
  NSMutableDictionary* state = [mWindow exportState];

  // Recreate the window with the right border style.
  NSRect frameRect = [mWindow frame];
  DestroyNativeWindow();
  nsresult rv = CreateNativeWindow(frameRect, aShouldHide ? eBorderStyle_none : mBorderStyle, true);
  NS_ENSURE_SUCCESS(rv, rv);

  // Re-import state.
  [mWindow importState:state];

  // Reparent the content view.
  [mWindow setContentView:contentView];
  [contentView release];

  // Reparent child windows.
  enumerator = [childWindows objectEnumerator];
  while ((child = [enumerator nextObject])) {
    [mWindow addChildWindow:child ordered:NSWindowAbove];
  }

  // Show the new window.
  if (isVisible) {
    rv = Show(true);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void nsCocoaWindow::EnteredFullScreen(bool aFullScreen)
{
  mInFullScreenTransition = false;
  mFullScreen = aFullScreen;
  DispatchSizeModeEvent();
}

NS_METHOD nsCocoaWindow::MakeFullScreen(bool aFullScreen)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!mWindow) {
    return NS_OK;
  }

  // We will call into MakeFullScreen redundantly when entering/exiting
  // fullscreen mode via OS X controls. When that happens we should just handle
  // it gracefully - no need to ASSERT.
  if (mFullScreen == aFullScreen) {
    return NS_OK;
  }

  // If we're using native fullscreen mode and our native window is invisible,
  // our attempt to go into fullscreen mode will fail with an assertion in
  // system code, without [WindowDelegate windowDidFailToEnterFullScreen:]
  // ever getting called.  To pre-empt this we bail here.  See bug 752294.
  if (mUsesNativeFullScreen && aFullScreen && ![mWindow isVisible]) {
    EnteredFullScreen(false);
    return NS_OK;
  }

  mInFullScreenTransition = true;

  if (mUsesNativeFullScreen) {
    // Calling toggleFullScreen will result in windowDid(FailTo)?(Enter|Exit)FullScreen
    // to be called from the OS. We will call EnteredFullScreen from those methods,
    // where mFullScreen will be set and a sizemode event will be dispatched.
    [mWindow toggleFullScreen:nil];
  } else {
    NSDisableScreenUpdates();
    // The order here matters. When we exit full screen mode, we need to show the
    // Dock first, otherwise the newly-created window won't have its minimize
    // button enabled. See bug 526282.
    nsCocoaUtils::HideOSChromeOnScreen(aFullScreen, [mWindow screen]);
    nsresult rv = nsBaseWidget::MakeFullScreen(aFullScreen);
    NSEnableScreenUpdates();
    NS_ENSURE_SUCCESS(rv, rv);

    EnteredFullScreen(aFullScreen);
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// Coordinates are global display pixels
nsresult nsCocoaWindow::DoResize(double aX, double aY,
                                 double aWidth, double aHeight,
                                 bool aRepaint,
                                 bool aConstrainToCurrentScreen)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!mWindow) {
    return NS_OK;
  }

  // ConstrainSize operates in device pixels, so we need to convert using
  // the backing scale factor here
  CGFloat scale = BackingScaleFactor();
  int32_t width = NSToIntRound(aWidth * scale);
  int32_t height = NSToIntRound(aHeight * scale);
  ConstrainSize(&width, &height);

  nsIntRect newBounds(NSToIntRound(aX), NSToIntRound(aY),
                      NSToIntRound(width / scale),
                      NSToIntRound(height / scale));

  // constrain to the screen that contains the largest area of the new rect
  FitRectToVisibleAreaForScreen(newBounds,
                                aConstrainToCurrentScreen ?
                                  [mWindow screen] : nullptr,
                                mUsesNativeFullScreen);

  // convert requested bounds into Cocoa coordinate system
  NSRect newFrame = nsCocoaUtils::GeckoRectToCocoaRect(newBounds);

  NSRect frame = [mWindow frame];
  BOOL isMoving = newFrame.origin.x != frame.origin.x ||
                  newFrame.origin.y != frame.origin.y;
  BOOL isResizing = newFrame.size.width != frame.size.width ||
                    newFrame.size.height != frame.size.height;

  if (!isMoving && !isResizing) {
    return NS_OK;
  }

  // We ignore aRepaint -- we have to call display:YES, otherwise the
  // title bar doesn't immediately get repainted and is displayed in
  // the wrong place, leading to a visual jump.
  [mWindow setFrame:newFrame display:YES];

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// Coordinates are global display pixels
NS_IMETHODIMP nsCocoaWindow::Resize(double aX, double aY,
                                    double aWidth, double aHeight,
                                    bool aRepaint)
{
  return DoResize(aX, aY, aWidth, aHeight, aRepaint, false);
}

// Coordinates are global display pixels
NS_IMETHODIMP nsCocoaWindow::Resize(double aWidth, double aHeight, bool aRepaint)
{
  double invScale = 1.0 / GetDefaultScale().scale;
  return DoResize(mBounds.x * invScale, mBounds.y * invScale,
                  aWidth, aHeight, aRepaint, true);
}

NS_IMETHODIMP nsCocoaWindow::GetClientBounds(nsIntRect &aRect)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  CGFloat scaleFactor = BackingScaleFactor();
  if (!mWindow) {
    aRect = nsCocoaUtils::CocoaRectToGeckoRectDevPix(NSZeroRect, scaleFactor);
    return NS_OK;
  }

  NSRect r;
  if ([mWindow isKindOfClass:[ToolbarWindow class]] &&
      [(ToolbarWindow*)mWindow drawsContentsIntoWindowFrame]) {
    r = [mWindow frame];
  } else {
    r = [mWindow contentRectForFrameRect:[mWindow frame]];
  }

  aRect = nsCocoaUtils::CocoaRectToGeckoRectDevPix(r, scaleFactor);

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void
nsCocoaWindow::UpdateBounds()
{
  NSRect frame = NSZeroRect;
  if (mWindow) {
    frame = [mWindow frame];
  }
  mBounds = nsCocoaUtils::CocoaRectToGeckoRectDevPix(frame, BackingScaleFactor());
}

NS_IMETHODIMP nsCocoaWindow::GetScreenBounds(nsIntRect &aRect)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

#ifdef DEBUG
  nsIntRect r = nsCocoaUtils::CocoaRectToGeckoRectDevPix([mWindow frame], BackingScaleFactor());
  NS_ASSERTION(mWindow && mBounds == r, "mBounds out of sync!");
#endif

  aRect = mBounds;
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

double
nsCocoaWindow::GetDefaultScaleInternal()
{
  return BackingScaleFactor();
}

static CGFloat
GetBackingScaleFactor(NSWindow* aWindow)
{
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
  NSScreen *screen =
    FindTargetScreenForRect(nsCocoaUtils::CocoaRectToGeckoRect(frame));
  return nsCocoaUtils::GetBackingScaleFactor(screen);
}

CGFloat
nsCocoaWindow::BackingScaleFactor()
{
  if (mBackingScaleFactor > 0.0) {
    return mBackingScaleFactor;
  }
  if (!mWindow) {
    return 1.0;
  }
  mBackingScaleFactor = GetBackingScaleFactor(mWindow);
  return mBackingScaleFactor;
}

void
nsCocoaWindow::BackingScaleFactorChanged()
{
  CGFloat newScale = GetBackingScaleFactor(mWindow);

  // ignore notification if it hasn't really changed (or maybe we have
  // disabled HiDPI mode via prefs)
  if (mBackingScaleFactor == newScale) {
    return;
  }

  if (mBackingScaleFactor > 0.0) {
    // convert size constraints to the new device pixel coordinate space
    double scaleFactor = newScale / mBackingScaleFactor;
    mSizeConstraints.mMinSize.width =
      NSToIntRound(mSizeConstraints.mMinSize.width * scaleFactor);
    mSizeConstraints.mMinSize.height =
      NSToIntRound(mSizeConstraints.mMinSize.height * scaleFactor);
    if (mSizeConstraints.mMaxSize.width < NS_MAXSIZE) {
      mSizeConstraints.mMaxSize.width =
        std::min(NS_MAXSIZE,
               NSToIntRound(mSizeConstraints.mMaxSize.width * scaleFactor));
    }
    if (mSizeConstraints.mMaxSize.height < NS_MAXSIZE) {
      mSizeConstraints.mMaxSize.height =
        std::min(NS_MAXSIZE,
               NSToIntRound(mSizeConstraints.mMaxSize.height * scaleFactor));
    }
  }

  mBackingScaleFactor = newScale;

  if (!mWidgetListener || mWidgetListener->GetXULWindow()) {
    return;
  }

  nsIPresShell* presShell = mWidgetListener->GetPresShell();
  if (presShell) {
    presShell->BackingScaleFactorChanged();
  }
}

int32_t
nsCocoaWindow::RoundsWidgetCoordinatesTo()
{
  if (BackingScaleFactor() == 2.0) {
    return 2;
  }
  return 1;
}

NS_IMETHODIMP nsCocoaWindow::SetCursor(nsCursor aCursor)
{
  if (mPopupContentView)
    return mPopupContentView->SetCursor(aCursor);

  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::SetCursor(imgIContainer* aCursor,
                                       uint32_t aHotspotX, uint32_t aHotspotY)
{
  if (mPopupContentView)
    return mPopupContentView->SetCursor(aCursor, aHotspotX, aHotspotY);

  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::SetTitle(const nsAString& aTitle)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!mWindow)
    return NS_OK;

  const nsString& strTitle = PromiseFlatString(aTitle);
  NSString* title = [NSString stringWithCharacters:strTitle.get() length:strTitle.Length()];
  [mWindow setTitle:title];

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsCocoaWindow::Invalidate(const nsIntRect & aRect)
{
  if (mPopupContentView) {
    return mPopupContentView->Invalidate(aRect);
  }

  return NS_OK;
}

// Pass notification of some drag event to Gecko
//
// The drag manager has let us know that something related to a drag has
// occurred in this window. It could be any number of things, ranging from 
// a drop, to a drag enter/leave, or a drag over event. The actual event
// is passed in |aMessage| and is passed along to our event hanlder so Gecko
// knows about it.
bool nsCocoaWindow::DragEvent(unsigned int aMessage, Point aMouseGlobal, UInt16 aKeyModifiers)
{
  return false;
}

NS_IMETHODIMP nsCocoaWindow::SendSetZLevelEvent()
{
  nsWindowZ placement = nsWindowZTop;
  nsIWidget* actualBelow;
  if (mWidgetListener)
    mWidgetListener->ZLevelChanged(true, &placement, nullptr, &actualBelow);
  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::GetChildSheet(bool aShown, nsCocoaWindow** _retval)
{
  nsIWidget* child = GetFirstChild();

  while (child) {
    nsWindowType type;
    if (NS_SUCCEEDED(child->GetWindowType(type)) && type == eWindowType_sheet) {
      // if it's a sheet, it must be an nsCocoaWindow
      nsCocoaWindow* cocoaWindow = static_cast<nsCocoaWindow*>(child);
      if (cocoaWindow->mWindow &&
          ((aShown && [cocoaWindow->mWindow isVisible]) ||
          (!aShown && cocoaWindow->mSheetNeedsShow))) {
        *_retval = cocoaWindow;
        return NS_OK;
      }
    }
    child = child->GetNextSibling();
  }

  *_retval = nullptr;

  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::GetRealParent(nsIWidget** parent)
{
  *parent = mParent;
  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::GetIsSheet(bool* isSheet)
{
  mWindowType == eWindowType_sheet ? *isSheet = true : *isSheet = false;
  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::GetSheetWindowParent(NSWindow** sheetWindowParent)
{
  *sheetWindowParent = mSheetWindowParent;
  return NS_OK;
}

// Invokes callback and ProcessEvent methods on Event Listener object
NS_IMETHODIMP 
nsCocoaWindow::DispatchEvent(WidgetGUIEvent* event, nsEventStatus& aStatus)
{
  aStatus = nsEventStatus_eIgnore;

  nsIWidget* aWidget = event->widget;
  NS_IF_ADDREF(aWidget);

  if (mWidgetListener)
    aStatus = mWidgetListener->HandleEvent(event, mUseAttachedEvents);

  NS_IF_RELEASE(aWidget);

  return NS_OK;
}

// aFullScreen should be the window's mFullScreen. We don't have access to that
// from here, so we need to pass it in. mFullScreen should be the canonical
// indicator that a window is currently full screen and it makes sense to keep
// all sizemode logic here.
static nsSizeMode
GetWindowSizeMode(NSWindow* aWindow, bool aFullScreen) {
  if (aFullScreen)
    return nsSizeMode_Fullscreen;
  if ([aWindow isMiniaturized])
    return nsSizeMode_Minimized;
  if (([aWindow styleMask] & NSResizableWindowMask) && [aWindow isZoomed])
    return nsSizeMode_Maximized;
  return nsSizeMode_Normal;
}

void
nsCocoaWindow::ReportMoveEvent()
{
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
  if (mWidgetListener)
    mWidgetListener->WindowMoved(this, mBounds.x, mBounds.y);

  mInReportMoveEvent = false;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void
nsCocoaWindow::DispatchSizeModeEvent()
{
  if (!mWindow) {
    return;
  }

  nsSizeMode newMode = GetWindowSizeMode(mWindow, mFullScreen);

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

void
nsCocoaWindow::ReportSizeEvent()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  UpdateBounds();

  if (mWidgetListener) {
    nsIntRect innerBounds;
    GetClientBounds(innerBounds);
    mWidgetListener->WindowResized(this, innerBounds.width, innerBounds.height);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsCocoaWindow::SetMenuBar(nsMenuBarX *aMenuBar)
{
  if (mMenuBar)
    mMenuBar->SetParent(nullptr);
  if (!mWindow) {
    mMenuBar = nullptr;
    return;
  }
  mMenuBar = aMenuBar;

  // Only paint for active windows, or paint the hidden window menu bar if no
  // other menu bar has been painted yet so that some reasonable menu bar is
  // displayed when the app starts up.
  id windowDelegate = [mWindow delegate];
  if (mMenuBar &&
      ((!gSomeMenuBarPainted && nsMenuUtilsX::GetHiddenWindowMenuBar() == mMenuBar) ||
       (windowDelegate && [windowDelegate toplevelActiveState])))
    mMenuBar->Paint();
}

NS_IMETHODIMP nsCocoaWindow::SetFocus(bool aState)
{
  if (!mWindow)
    return NS_OK;

  if (mPopupContentView) {
    mPopupContentView->SetFocus(aState);
  }
  else if (aState && ([mWindow isVisible] || [mWindow isMiniaturized])) {
    [mWindow makeKeyAndOrderFront:nil];
    SendSetZLevelEvent();
  }

  return NS_OK;
}

nsIntPoint nsCocoaWindow::WidgetToScreenOffset()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NSRect rect = NSZeroRect;
  nsIntRect r;
  if (mWindow) {
    rect = [mWindow contentRectForFrameRect:[mWindow frame]];
  }
  r = nsCocoaUtils::CocoaRectToGeckoRectDevPix(rect, BackingScaleFactor());

  return r.TopLeft();

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(nsIntPoint(0,0));
}

nsIntPoint nsCocoaWindow::GetClientOffset()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  nsIntRect clientRect;
  GetClientBounds(clientRect);

  return clientRect.TopLeft() - mBounds.TopLeft();

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(nsIntPoint(0, 0));
}

nsIntSize nsCocoaWindow::ClientToWindowSize(const nsIntSize& aClientSize)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (!mWindow)
    return nsIntSize(0, 0);

  CGFloat backingScale = BackingScaleFactor();
  nsIntRect r(0, 0, aClientSize.width, aClientSize.height);
  NSRect rect = nsCocoaUtils::DevPixelsToCocoaPoints(r, backingScale);

  NSRect inflatedRect = [mWindow frameRectForContentRect:rect];
  return nsCocoaUtils::CocoaRectToGeckoRectDevPix(inflatedRect, backingScale).Size();

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(nsIntSize(0,0));
}

nsMenuBarX* nsCocoaWindow::GetMenuBar()
{
  return mMenuBar;
}

NS_IMETHODIMP nsCocoaWindow::CaptureRollupEvents(nsIRollupListener* aListener, bool aDoCapture)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  gRollupListener = nullptr;
  
  if (aDoCapture) {
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
    if (mWindow && (mWindowType == eWindowType_popup))
      SetPopupWindowLevel();
  } else {
    // XXXndeakin this doesn't make sense.
    // Why is the new window assumed to be a modal panel?
    if (mWindow && (mWindowType == eWindowType_popup))
      [mWindow setLevel:NSModalPanelWindowLevel];
  }
  
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsCocoaWindow::GetAttention(int32_t aCycleCount)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  [NSApp requestUserAttention:NSInformationalRequest];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

bool
nsCocoaWindow::HasPendingInputEvent()
{
  return nsChildView::DoHasPendingInputEvent();
}

NS_IMETHODIMP nsCocoaWindow::SetWindowShadowStyle(int32_t aStyle)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!mWindow)
    return NS_OK;

  mShadowStyle = aStyle;
  [mWindow setHasShadow:(aStyle != NS_STYLE_WINDOW_SHADOW_NONE)];
  AdjustWindowShadow();
  SetUpWindowFilter();

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void nsCocoaWindow::SetShowsToolbarButton(bool aShow)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mWindow)
    [mWindow setShowsToolbarButton:aShow];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsCocoaWindow::SetShowsFullScreenButton(bool aShow)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (!mWindow || ![mWindow respondsToSelector:@selector(toggleFullScreen:)] ||
      mUsesNativeFullScreen == aShow) {
    return;
  }

  // If the window is currently in fullscreen mode, then we're going to
  // transition out first, then set the collection behavior & toggle
  // mUsesNativeFullScreen, then transtion back into fullscreen mode. This
  // prevents us from getting into a conflicting state with MakeFullScreen
  // where mUsesNativeFullScreen would lead us down the wrong path.
  bool wasFullScreen = mFullScreen;

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
  mUsesNativeFullScreen = aShow;

  if (wasFullScreen) {
    MakeFullScreen(true);
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsCocoaWindow::SetWindowAnimationType(nsIWidget::WindowAnimationType aType)
{
  mAnimationType = aType;
}

NS_IMETHODIMP nsCocoaWindow::SetWindowTitlebarColor(nscolor aColor, bool aActive)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!mWindow)
    return NS_OK;

  // If they pass a color with a complete transparent alpha component, use the
  // native titlebar appearance.
  if (NS_GET_A(aColor) == 0) {
    [mWindow setTitlebarColor:nil forActiveWindow:(BOOL)aActive]; 
  } else {
    // Transform from sRGBA to monitor RGBA. This seems like it would make trying
    // to match the system appearance lame, so probably we just shouldn't color 
    // correct chrome.
    if (gfxPlatform::GetCMSMode() == eCMSMode_All) {
      qcms_transform *transform = gfxPlatform::GetCMSRGBATransform();
      if (transform) {
        uint8_t color[3];
        color[0] = NS_GET_R(aColor);
        color[1] = NS_GET_G(aColor);
        color[2] = NS_GET_B(aColor);
        qcms_transform_data(transform, color, color, 1);
        aColor = NS_RGB(color[0], color[1], color[2]);
      }
    }

    [mWindow setTitlebarColor:[NSColor colorWithDeviceRed:NS_GET_R(aColor)/255.0
                                                    green:NS_GET_G(aColor)/255.0
                                                     blue:NS_GET_B(aColor)/255.0
                                                    alpha:NS_GET_A(aColor)/255.0]
              forActiveWindow:(BOOL)aActive];
  }
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void nsCocoaWindow::SetDrawsInTitlebar(bool aState)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mWindow)
    [mWindow setDrawsContentsIntoWindowFrame:aState];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

NS_IMETHODIMP nsCocoaWindow::SynthesizeNativeMouseEvent(nsIntPoint aPoint,
                                                        uint32_t aNativeMessage,
                                                        uint32_t aModifierFlags)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (mPopupContentView)
    return mPopupContentView->SynthesizeNativeMouseEvent(aPoint, aNativeMessage,
                                                         aModifierFlags);

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

gfxASurface* nsCocoaWindow::GetThebesSurface()
{
  if (mPopupContentView)
    return mPopupContentView->GetThebesSurface();
  return nullptr;
}

void nsCocoaWindow::SetPopupWindowLevel()
{
  if (!mWindow)
    return;

  // Floating popups are at the floating level and hide when the window is
  // deactivated.
  if (mPopupLevel == ePopupLevelFloating) {
    [mWindow setLevel:NSFloatingWindowLevel];
    [mWindow setHidesOnDeactivate:YES];
  }
  else {
    // Otherwise, this is a top-level or parent popup. Parent popups always
    // appear just above their parent and essentially ignore the level.
    [mWindow setLevel:NSPopUpMenuWindowLevel];
    [mWindow setHidesOnDeactivate:NO];
  }
}

bool nsCocoaWindow::IsChildInFailingLeftClickThrough(NSView *aChild)
{
  if ([aChild isKindOfClass:[ChildView class]]) {
    ChildView* childView = (ChildView*) aChild;
    if ([childView isInFailingLeftClickThrough])
      return true;
  }
  NSArray* subviews = [aChild subviews];
  if (subviews) {
    NSUInteger count = [subviews count];
    for (NSUInteger i = 0; i < count; ++i) {
      NSView* aView = (NSView*) [subviews objectAtIndex:i];
      if (IsChildInFailingLeftClickThrough(aView))
        return true;
    }
  }
  return false;
}

// Don't focus a plugin if we're in a left click-through that will
// fail (see [ChildView isInFailingLeftClickThrough]).  Called from
// [ChildView shouldFocusPlugin].
bool nsCocoaWindow::ShouldFocusPlugin()
{
  if (!mWindow || IsChildInFailingLeftClickThrough([mWindow contentView]))
    return false;

  return true;
}

NS_IMETHODIMP
nsCocoaWindow::NotifyIME(NotificationToIME aNotification)
{
  switch (aNotification) {
    case NOTIFY_IME_OF_FOCUS:
      if (mInputContext.IsPasswordEditor()) {
        TextInputHandler::EnableSecureEventInput();
      }
      return NS_OK;
    case NOTIFY_IME_OF_BLUR:
      // When we're going to be deactive, we must disable the secure event input
      // mode, see the Carbon Event Manager Reference.
      TextInputHandler::EnsureSecureEventInputDisabled();
      return NS_OK;
    default:
      return NS_ERROR_NOT_IMPLEMENTED;
  }
}

NS_IMETHODIMP_(void)
nsCocoaWindow::SetInputContext(const InputContext& aContext,
                               const InputContextAction& aAction)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mWindow &&
      [mWindow firstResponder] == mWindow &&
      [mWindow isKeyWindow] &&
      [[NSApplication sharedApplication] isActive]) {
    if (aContext.IsPasswordEditor()) {
      TextInputHandler::EnableSecureEventInput();
    } else {
      TextInputHandler::EnsureSecureEventInputDisabled();
    }
  }
  mInputContext = aContext;

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

@implementation WindowDelegate

// We try to find a gecko menu bar to paint. If one does not exist, just paint
// the application menu by itself so that a window doesn't have some other
// window's menu bar.
+ (void)paintMenubarForWindow:(NSWindow*)aWindow
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // make sure we only act on windows that have this kind of
  // object as a delegate
  id windowDelegate = [aWindow delegate];
  if ([windowDelegate class] != [self class])
    return;

  nsCocoaWindow* geckoWidget = [windowDelegate geckoWidget];
  NS_ASSERTION(geckoWidget, "Window delegate not returning a gecko widget!");
  
  nsMenuBarX* geckoMenuBar = geckoWidget->GetMenuBar();
  if (geckoMenuBar) {
    geckoMenuBar->Paint();
  }
  else {
    // sometimes we don't have a native application menu early in launching
    if (!sApplicationMenu)
      return;

    NSMenu* mainMenu = [NSApp mainMenu];
    NS_ASSERTION([mainMenu numberOfItems] > 0, "Main menu does not have any items, something is terribly wrong!");

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

- (id)initWithGeckoWindow:(nsCocoaWindow*)geckoWind
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  [super init];
  mGeckoWindow = geckoWind;
  mToplevelActiveState = false;
  mHasEverBeenZoomed = false;
  return self;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)proposedFrameSize
{
  RollUpPopups();
  
  return proposedFrameSize;
}

- (void)windowDidResize:(NSNotification *)aNotification
{
  BaseWindow* window = [aNotification object];
  [window updateTrackingArea];

  if (!mGeckoWindow)
    return;

  // Resizing might have changed our zoom state.
  mGeckoWindow->DispatchSizeModeEvent();
  mGeckoWindow->ReportSizeEvent();
}

- (void)windowDidChangeScreen:(NSNotification *)aNotification
{
  if (!mGeckoWindow)
    return;

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
  NSWindow *window = (NSWindow *)[aNotification object];
  if ([window respondsToSelector:@selector(backingScaleFactor)]) {
    if (GetBackingScaleFactor(window) != mGeckoWindow->BackingScaleFactor()) {
      mGeckoWindow->BackingScaleFactorChanged();
    }
  }

  mGeckoWindow->ReportMoveEvent();
}

// Lion's full screen mode will bypass our internal fullscreen tracking, so
// we need to catch it when we transition and call our own methods, which in
// turn will fire "fullscreen" events.
- (void)windowDidEnterFullScreen:(NSNotification *)notification
{
  if (!mGeckoWindow) {
    return;
  }

  mGeckoWindow->EnteredFullScreen(true);
}

- (void)windowDidExitFullScreen:(NSNotification *)notification
{
  if (!mGeckoWindow) {
    return;
  }

  mGeckoWindow->EnteredFullScreen(false);
}

- (void)windowDidFailToEnterFullScreen:(NSWindow *)window
{
  if (!mGeckoWindow) {
    return;
  }

  mGeckoWindow->EnteredFullScreen(false);
}

- (void)windowDidFailToExitFullScreen:(NSWindow *)window
{
  if (!mGeckoWindow) {
    return;
  }

  mGeckoWindow->EnteredFullScreen(true);
}

- (void)windowDidBecomeMain:(NSNotification *)aNotification
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  RollUpPopups();
  ChildViewMouseTracker::ReEvaluateMouseEnterState();

  // [NSApp _isRunningAppModal] will return true if we're running an OS dialog
  // app modally. If one of those is up then we want it to retain its menu bar.
  if ([NSApp _isRunningAppModal])
    return;
  NSWindow* window = [aNotification object];
  if (window)
    [WindowDelegate paintMenubarForWindow:window];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)windowDidResignMain:(NSNotification *)aNotification
{
  RollUpPopups();
  ChildViewMouseTracker::ReEvaluateMouseEnterState();

  // [NSApp _isRunningAppModal] will return true if we're running an OS dialog
  // app modally. If one of those is up then we want it to retain its menu bar.
  if ([NSApp _isRunningAppModal])
    return;
  nsRefPtr<nsMenuBarX> hiddenWindowMenuBar = nsMenuUtilsX::GetHiddenWindowMenuBar();
  if (hiddenWindowMenuBar) {
    // printf("painting hidden window menu bar due to window losing main status\n");
    hiddenWindowMenuBar->Paint();
  }
}

- (void)windowDidBecomeKey:(NSNotification *)aNotification
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  RollUpPopups();
  ChildViewMouseTracker::ReEvaluateMouseEnterState();

  NSWindow* window = [aNotification object];
  if ([window isSheet])
    [WindowDelegate paintMenubarForWindow:window];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)windowDidResignKey:(NSNotification *)aNotification
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  RollUpPopups();
  ChildViewMouseTracker::ReEvaluateMouseEnterState();

  // If a sheet just resigned key then we should paint the menu bar
  // for whatever window is now main.
  NSWindow* window = [aNotification object];
  if ([window isSheet])
    [WindowDelegate paintMenubarForWindow:[NSApp mainWindow]];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)windowWillMove:(NSNotification *)aNotification
{
  RollUpPopups();
}

- (void)windowDidMove:(NSNotification *)aNotification
{
  if (mGeckoWindow)
    mGeckoWindow->ReportMoveEvent();
}

- (BOOL)windowShouldClose:(id)sender
{
  nsIWidgetListener* listener = mGeckoWindow ? mGeckoWindow->GetWidgetListener() : nullptr;
  if (listener)
    listener->RequestWindowClose(mGeckoWindow);
  return NO; // gecko will do it
}

- (void)windowWillClose:(NSNotification *)aNotification
{
  RollUpPopups();
}

- (void)windowWillMiniaturize:(NSNotification *)aNotification
{
  RollUpPopups();
}

- (void)windowDidMiniaturize:(NSNotification *)aNotification
{
  if (mGeckoWindow)
    mGeckoWindow->DispatchSizeModeEvent();
}

- (void)windowDidDeminiaturize:(NSNotification *)aNotification
{
  if (mGeckoWindow)
    mGeckoWindow->DispatchSizeModeEvent();
}

- (BOOL)windowShouldZoom:(NSWindow *)window toFrame:(NSRect)proposedFrame
{
  if (!mHasEverBeenZoomed && [window isZoomed])
    return NO; // See bug 429954.

  mHasEverBeenZoomed = YES;
  return YES;
}

- (void)didEndSheet:(NSWindow*)sheet returnCode:(int)returnCode contextInfo:(void*)contextInfo
{
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
  if (contextInfo)
    [TopLevelWindowData activateInWindow:(NSWindow*)contextInfo];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)windowDidChangeBackingProperties:(NSNotification *)aNotification
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSWindow *window = (NSWindow *)[aNotification object];

  if ([window respondsToSelector:@selector(backingScaleFactor)]) {
    CGFloat oldFactor =
      [[[aNotification userInfo]
         objectForKey:@"NSBackingPropertyOldScaleFactorKey"] doubleValue];
    if ([window backingScaleFactor] != oldFactor) {
      mGeckoWindow->BackingScaleFactorChanged();
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (nsCocoaWindow*)geckoWidget
{
  return mGeckoWindow;
}

- (bool)toplevelActiveState
{
  return mToplevelActiveState;
}

- (void)sendToplevelActivateEvents
{
  if (!mToplevelActiveState && mGeckoWindow) {
    nsIWidgetListener* listener = mGeckoWindow->GetWidgetListener();
    if (listener)
      listener->WindowActivated();
    mToplevelActiveState = true;
  }
}

- (void)sendToplevelDeactivateEvents
{
  if (mToplevelActiveState && mGeckoWindow) {
    nsIWidgetListener* listener = mGeckoWindow->GetWidgetListener();
    if (listener)
      listener->WindowDeactivated();
    mToplevelActiveState = false;
  }
}

@end

static float
GetDPI(NSWindow* aWindow)
{
  NSScreen* screen = [aWindow screen];
  if (!screen)
    return 96.0f;

  CGDirectDisplayID displayID =
    [[[screen deviceDescription] objectForKey:@"NSScreenNumber"] intValue];
  CGFloat heightMM = ::CGDisplayScreenSize(displayID).height;
  size_t heightPx = ::CGDisplayPixelsHigh(displayID);
  CGFloat scaleFactor = [aWindow userSpaceScaleFactor];
  if (scaleFactor < 0.01 || heightMM < 1 || heightPx < 1) {
    // Something extremely bogus is going on
    return 96.0f;
  }

  // Currently we don't do our own scaling to take account
  // of userSpaceScaleFactor, so every "pixel" we draw is actually
  // userSpaceScaleFactor screen pixels. So divide the screen height
  // by userSpaceScaleFactor to get the number of "device pixels"
  // available.
  float dpi = (heightPx / scaleFactor) / (heightMM / MM_PER_INCH_FLOAT);

  // Account for HiDPI mode where Cocoa's "points" do not correspond to real
  // device pixels
  CGFloat backingScale = GetBackingScaleFactor(aWindow);

  return dpi * backingScale;
}

@interface BaseWindow(Private)
- (void)removeTrackingArea;
- (void)cursorUpdated:(NSEvent*)aEvent;
- (void)updateContentViewSize;
@end

@implementation BaseWindow

- (id)initWithContentRect:(NSRect)aContentRect styleMask:(NSUInteger)aStyle backing:(NSBackingStoreType)aBufferingType defer:(BOOL)aFlag
{
  mDrawsIntoWindowFrame = NO;
  [super initWithContentRect:aContentRect styleMask:aStyle backing:aBufferingType defer:aFlag];
  mState = nil;
  mActiveTitlebarColor = nil;
  mInactiveTitlebarColor = nil;
  mScheduledShadowInvalidation = NO;
  mDPI = GetDPI(self);
  mTrackingArea = nil;
  mBeingShown = NO;
  [self updateTrackingArea];

  return self;
}

- (BOOL)isVisibleOrBeingShown
{
  return [super isVisible] || mBeingShown;
}

- (void)orderFront:(id)sender
{
  AutoRestore<BOOL> saveBeingShown(mBeingShown);
  mBeingShown = YES;
  [super orderFront:sender];
}

- (void)makeKeyAndOrderFront:(id)sender
{
  AutoRestore<BOOL> saveBeingShown(mBeingShown);
  mBeingShown = YES;
  [super makeKeyAndOrderFront:sender];
}

- (void)dealloc
{
  [mActiveTitlebarColor release];
  [mInactiveTitlebarColor release];
  [self removeTrackingArea];
  ChildViewMouseTracker::OnDestroyWindow(self);
  [super dealloc];
}

static const NSString* kStateTitleKey = @"title";
static const NSString* kStateDrawsContentsIntoWindowFrameKey = @"drawsContentsIntoWindowFrame";
static const NSString* kStateActiveTitlebarColorKey = @"activeTitlebarColor";
static const NSString* kStateInactiveTitlebarColorKey = @"inactiveTitlebarColor";
static const NSString* kStateShowsToolbarButton = @"showsToolbarButton";

- (void)importState:(NSDictionary*)aState
{
  [self setTitle:[aState objectForKey:kStateTitleKey]];
  [self setDrawsContentsIntoWindowFrame:[[aState objectForKey:kStateDrawsContentsIntoWindowFrameKey] boolValue]];
  [self setTitlebarColor:[aState objectForKey:kStateActiveTitlebarColorKey] forActiveWindow:YES];
  [self setTitlebarColor:[aState objectForKey:kStateInactiveTitlebarColorKey] forActiveWindow:NO];
  [self setShowsToolbarButton:[[aState objectForKey:kStateShowsToolbarButton] boolValue]];
}

- (NSMutableDictionary*)exportState
{
  NSMutableDictionary* state = [NSMutableDictionary dictionaryWithCapacity:10];
  [state setObject:[self title] forKey:kStateTitleKey];
  [state setObject:[NSNumber numberWithBool:[self drawsContentsIntoWindowFrame]]
            forKey:kStateDrawsContentsIntoWindowFrameKey];
  NSColor* activeTitlebarColor = [self titlebarColorForActiveWindow:YES];
  if (activeTitlebarColor) {
    [state setObject:activeTitlebarColor forKey:kStateActiveTitlebarColorKey];
  }
  NSColor* inactiveTitlebarColor = [self titlebarColorForActiveWindow:NO];
  if (inactiveTitlebarColor) {
    [state setObject:inactiveTitlebarColor forKey:kStateInactiveTitlebarColorKey];
  }
  [state setObject:[NSNumber numberWithBool:[self showsToolbarButton]]
            forKey:kStateShowsToolbarButton];
  return state;
}

- (void)setDrawsContentsIntoWindowFrame:(BOOL)aState
{
  mDrawsIntoWindowFrame = aState;
  [self updateContentViewSize];
}

- (BOOL)drawsContentsIntoWindowFrame
{
  return mDrawsIntoWindowFrame;
}

// Pass nil here to get the default appearance.
- (void)setTitlebarColor:(NSColor*)aColor forActiveWindow:(BOOL)aActive
{
  [aColor retain];
  if (aActive) {
    [mActiveTitlebarColor release];
    mActiveTitlebarColor = aColor;
  } else {
    [mInactiveTitlebarColor release];
    mInactiveTitlebarColor = aColor;
  }
}

- (NSColor*)titlebarColorForActiveWindow:(BOOL)aActive
{
  return aActive ? mActiveTitlebarColor : mInactiveTitlebarColor;
}

- (void)deferredInvalidateShadow
{
  if (mScheduledShadowInvalidation || [self isOpaque] || ![self hasShadow])
    return;

  [self performSelector:@selector(invalidateShadow) withObject:nil afterDelay:0];
  mScheduledShadowInvalidation = YES;
}

- (void)invalidateShadow
{
  [super invalidateShadow];
  mScheduledShadowInvalidation = NO;
}

- (float)getDPI
{
  return mDPI;
}

- (NSView*)trackingAreaView
{
  NSView* contentView = [self contentView];
  return [contentView superview] ? [contentView superview] : contentView;
}

- (ChildView*)mainChildView
{
  NSView *contentView = [self contentView];
  // A PopupWindow's contentView is a ChildView object.
  if ([contentView isKindOfClass:[ChildView class]]) {
    return (ChildView*)contentView;
  }
  NSView* lastView = [[contentView subviews] lastObject];
  if ([lastView isKindOfClass:[ChildView class]]) {
    return (ChildView*)lastView;
  }
  return nil;
}

- (void)removeTrackingArea
{
  if (mTrackingArea) {
    [[self trackingAreaView] removeTrackingArea:mTrackingArea];
    [mTrackingArea release];
    mTrackingArea = nil;
  }
}

- (void)updateTrackingArea
{
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

- (void)mouseEntered:(NSEvent*)aEvent
{
  ChildViewMouseTracker::MouseEnteredWindow(aEvent);
}

- (void)mouseExited:(NSEvent*)aEvent
{
  ChildViewMouseTracker::MouseExitedWindow(aEvent);
}

- (void)mouseMoved:(NSEvent*)aEvent
{
  ChildViewMouseTracker::MouseMoved(aEvent);
}

- (void)cursorUpdated:(NSEvent*)aEvent
{
  // Nothing to do here, but NSTrackingArea wants us to implement this method.
}

- (void)updateContentViewSize
{
  NSRect rect = [self contentRectForFrameRect:[self frame]];
  [[self contentView] setFrameSize:rect.size];
}

// Override methods that translate between content rect and frame rect.
- (NSRect)contentRectForFrameRect:(NSRect)aRect
{
  if ([self drawsContentsIntoWindowFrame]) {
    return aRect;
  }
  return [super contentRectForFrameRect:aRect];
}

- (NSRect)contentRectForFrameRect:(NSRect)aRect styleMask:(NSUInteger)aMask
{
  if ([self drawsContentsIntoWindowFrame]) {
    return aRect;
  }
  if ([super respondsToSelector:@selector(contentRectForFrameRect:styleMask:)]) {
    return [super contentRectForFrameRect:aRect styleMask:aMask];
  } else {
    return [NSWindow contentRectForFrameRect:aRect styleMask:aMask];
  }
}

- (NSRect)frameRectForContentRect:(NSRect)aRect
{
  if ([self drawsContentsIntoWindowFrame]) {
    return aRect;
  }
  return [super frameRectForContentRect:aRect];
}

- (NSRect)frameRectForContentRect:(NSRect)aRect styleMask:(NSUInteger)aMask
{
  if ([self drawsContentsIntoWindowFrame]) {
    return aRect;
  }
  if ([super respondsToSelector:@selector(frameRectForContentRect:styleMask:)]) {
    return [super frameRectForContentRect:aRect styleMask:aMask];
  } else {
    return [NSWindow frameRectForContentRect:aRect styleMask:aMask];
  }
}

- (void)setContentView:(NSView*)aView
{
  [super setContentView:aView];

  // Now move the contentView to the bottommost layer so that it's guaranteed
  // to be under the window buttons.
  NSView* frameView = [aView superview];
  [aView removeFromSuperview];
  [frameView addSubview:aView positioned:NSWindowBelow relativeTo:nil];
}

- (NSArray*)titlebarControls
{
  // Return all subviews of the frameView which are not the content view.
  NSView* frameView = [[self contentView] superview];
  NSMutableArray* array = [[[frameView subviews] mutableCopy] autorelease];
  [array removeObject:[self contentView]];
  return array;
}

- (BOOL)respondsToSelector:(SEL)aSelector
{
  // Claim the window doesn't respond to this so that the system
  // doesn't steal keyboard equivalents for it. Bug 613710.
  if (aSelector == @selector(cancelOperation:)) {
    return NO;
  }

  return [super respondsToSelector:aSelector];
}

- (void) doCommandBySelector:(SEL)aSelector
{
  // We override this so that it won't beep if it can't act.
  // We want to control the beeping for missing or disabled
  // commands ourselves.
  [self tryToPerform:aSelector with:nil];
}

- (id)accessibilityAttributeValue:(NSString *)attribute
{
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
  if (nsCocoaFeatures::OnLionOrLater() && [retval isKindOfClass:[NSArray class]] &&
      [attribute isEqualToString:@"AXChildren"]) {
    NSMutableArray *holder = [NSMutableArray arrayWithCapacity:10];
    [holder addObjectsFromArray:(NSArray *)retval];
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
}

// If we were built on OS X 10.6 or with the 10.6 SDK and are running on Lion,
// the OS (specifically -[NSWindow sendEvent:]) won't send NSEventTypeGesture
// events to -[ChildView magnifyWithEvent:] as it should.  The following code
// gets around this.  See bug 863841.
#if !defined( MAC_OS_X_VERSION_10_7 ) || \
    ( MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_X_VERSION_10_7 )
- (void)sendEvent:(NSEvent *)anEvent
{
  if ([ChildView isLionSmartMagnifyEvent: anEvent]) {
    [[self mainChildView] magnifyWithEvent:anEvent];
    return;
  }
  [super sendEvent:anEvent];
}
#endif

@end

// This class allows us to exercise control over the window's title bar. This
// allows for a "unified toolbar" look without having to extend the content
// area into the title bar. It works like this:
// 1) We set the window's style to textured.
// 2) Because of this, the background color applies to the entire window, including
//     the titlebar area. For normal textured windows, the default pattern is a 
//    "brushed metal" image on Tiger and a unified gradient on Leopard.
// 3) We set the background color to a custom NSColor subclass that knows how tall the window is.
//    When -set is called on it, it sets a pattern (with a draw callback) as the fill. In that callback,
//    it paints the the titlebar and background colors in the correct areas of the context it's given,
//    which will fill the entire window (CG will tile it horizontally for us).
// 4) Whenever the window's main state changes and when [window display] is called,
//    Cocoa redraws the titlebar using the patternDraw callback function.
//
// This class also provides us with a pill button to show/hide the toolbar up to 10.6.
//
// Drawing the unified gradient in the titlebar and the toolbar works like this:
// 1) In the style sheet we set the toolbar's -moz-appearance to toolbar or
//    -moz-mac-unified-toolbar.
// 2) When the toolbar is visible and we paint the application chrome
//    window, the array that Gecko passes nsChildView::UpdateThemeGeometries
//    will contain an entry for the widget type NS_THEME_TOOLBAR or
//    NS_THEME_MOZ_MAC_UNIFIED_TOOLBAR.
// 3) nsChildView::UpdateThemeGeometries finds the toolbar frame's ToolbarWindow
//    and passes the toolbar frame's height to setUnifiedToolbarHeight.
// 4) If the toolbar height has changed, a titlebar redraw is triggered and the
//    upper part of the unified gradient is drawn in the titlebar.
// 5) The lower part of the unified gradient in the toolbar is drawn during
//    normal window content painting in nsNativeThemeCocoa::DrawUnifiedToolbar.
//
// Whenever the unified gradient is drawn in the titlebar or the toolbar, both
// titlebar height and toolbar height must be known in order to construct the
// correct gradient. But you can only get from the toolbar frame
// to the containing window - the other direction doesn't work. That's why the
// toolbar height is cached in the ToolbarWindow but nsNativeThemeCocoa can simply
// query the window for its titlebar height when drawing the toolbar.
//
// Note that in drawsContentsIntoWindowFrame mode, titlebar drawing works in a
// completely different way: In that mode, the window's mainChildView will
// cover the titlebar completely and nothing that happens in the window
// background will reach the screen.
@implementation ToolbarWindow

- (id)initWithContentRect:(NSRect)aContentRect styleMask:(NSUInteger)aStyle backing:(NSBackingStoreType)aBufferingType defer:(BOOL)aFlag
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  aStyle = aStyle | NSTexturedBackgroundWindowMask;
  if ((self = [super initWithContentRect:aContentRect styleMask:aStyle backing:aBufferingType defer:aFlag])) {
    mColor = [[TitlebarAndBackgroundColor alloc] initWithWindow:self];
    // Bypass our guard method below.
    [super setBackgroundColor:mColor];
    mBackgroundColor = [[NSColor whiteColor] retain];

    mUnifiedToolbarHeight = 22.0f;

    // setBottomCornerRounded: is a private API call, so we check to make sure
    // we respond to it just in case.
    if ([self respondsToSelector:@selector(setBottomCornerRounded:)])
      [self setBottomCornerRounded:nsCocoaFeatures::OnLionOrLater()];

    [self setAutorecalculatesContentBorderThickness:NO forEdge:NSMaxYEdge];
    [self setContentBorderThickness:0.0f forEdge:NSMaxYEdge];
  }
  return self;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)dealloc
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mColor release];
  [mBackgroundColor release];
  [mTitlebarView release];
  [super dealloc];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)setTitlebarColor:(NSColor*)aColor forActiveWindow:(BOOL)aActive
{
  [super setTitlebarColor:aColor forActiveWindow:aActive];
  [self setTitlebarNeedsDisplayInRect:[self titlebarRect]];
}

- (void)setBackgroundColor:(NSColor*)aColor
{
  [aColor retain];
  [mBackgroundColor release];
  mBackgroundColor = aColor;
}

- (NSColor*)windowBackgroundColor
{
  return mBackgroundColor;
}

- (void)setTitlebarNeedsDisplayInRect:(NSRect)aRect
{
  [self setTitlebarNeedsDisplayInRect:aRect sync:NO];
}

- (void)setTitlebarNeedsDisplayInRect:(NSRect)aRect sync:(BOOL)aSync
{
  NSRect titlebarRect = [self titlebarRect];
  NSRect rect = NSIntersectionRect(titlebarRect, aRect);
  if (NSIsEmptyRect(rect))
    return;

  NSView* borderView = [[self contentView] superview];
  if (!borderView)
    return;

  if (aSync) {
    [borderView displayRect:rect];
  } else {
    [borderView setNeedsDisplayInRect:rect];
  }
}

- (NSRect)titlebarRect
{
  return NSMakeRect(0, [[self contentView] bounds].size.height,
                    [self frame].size.width, [self titlebarHeight]);
}

// Returns the unified height of titlebar + toolbar.
- (CGFloat)unifiedToolbarHeight
{
  return mUnifiedToolbarHeight;
}

- (CGFloat)titlebarHeight
{
  // We use the original content rect here, not what we return from
  // [self contentRectForFrameRect:], because that would give us a
  // titlebarHeight of zero in drawsContentsIntoWindowFrame mode.
  NSRect frameRect = [self frame];
  NSRect originalContentRect = [NSWindow contentRectForFrameRect:frameRect styleMask:[self styleMask]];
  return NSMaxY(frameRect) - NSMaxY(originalContentRect);
}

// Stores the complete height of titlebar + toolbar.
- (void)setUnifiedToolbarHeight:(CGFloat)aHeight
{
  if (aHeight == mUnifiedToolbarHeight)
    return;

  mUnifiedToolbarHeight = aHeight;

  // Update sheet positioning hint
  CGFloat topMargin = mUnifiedToolbarHeight - [self titlebarHeight];
  [self setContentBorderThickness:topMargin forEdge:NSMaxYEdge];

  // Redraw the title bar. If we're inside painting, we'll do it right now,
  // otherwise we'll just invalidate it.
  BOOL needSyncRedraw = ([NSView focusView] != nil);
  [self setTitlebarNeedsDisplayInRect:[self titlebarRect] sync:needSyncRedraw];
}

// Extending the content area into the title bar works by resizing the
// mainChildView so that it covers the titlebar.
- (void)setDrawsContentsIntoWindowFrame:(BOOL)aState
{
  BOOL stateChanged = ([self drawsContentsIntoWindowFrame] != aState);
  [super setDrawsContentsIntoWindowFrame:aState];
  if (stateChanged && [[self delegate] isKindOfClass:[WindowDelegate class]]) {
    // Here we extend / shrink our mainChildView. We do that by firing a resize
    // event which will cause the ChildView to be resized to the rect returned
    // by nsCocoaWindow::GetClientBounds. GetClientBounds bases its return
    // value on what we return from drawsContentsIntoWindowFrame.
    WindowDelegate *windowDelegate = (WindowDelegate *)[self delegate];
    nsCocoaWindow *geckoWindow = [windowDelegate geckoWidget];
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
}

// Returning YES here makes the setShowsToolbarButton method work even though
// the window doesn't contain an NSToolbar.
- (BOOL)_hasToolbar
{
  return YES;
}

// Dispatch a toolbar pill button clicked message to Gecko.
- (void)_toolbarPillButtonClicked:(id)sender
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  RollUpPopups();

  if ([[self delegate] isKindOfClass:[WindowDelegate class]]) {
    WindowDelegate *windowDelegate = (WindowDelegate *)[self delegate];
    nsCocoaWindow *geckoWindow = [windowDelegate geckoWidget];
    if (!geckoWindow)
      return;

    nsIWidgetListener* listener = geckoWindow->GetWidgetListener();
    if (listener)
      listener->OSToolbarButtonPressed();
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Retain and release "self" to avoid crashes when our widget (and its native
// window) is closed as a result of processing a key equivalent (e.g.
// Command+w or Command+q).  This workaround is only needed for a window
// that can become key.
- (BOOL)performKeyEquivalent:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NSWindow *nativeWindow = [self retain];
  BOOL retval = [super performKeyEquivalent:theEvent];
  [nativeWindow release];
  return retval;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

- (void)sendEvent:(NSEvent *)anEvent
{
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
    case NSOtherMouseDragged:
    {
      // Drop all mouse events if a modal window has appeared above us.
      // This helps make us behave as if the OS were running a "real" modal
      // event loop.
      id delegate = [self delegate];
      if (delegate && [delegate isKindOfClass:[WindowDelegate class]]) {
        nsCocoaWindow *widget = [(WindowDelegate *)delegate geckoWidget];
        if (widget) {
          if (type == NSMouseMoved) {
            [[self mainChildView] updateWindowDraggableStateOnMouseMove:anEvent];
          }
          if (gGeckoAppModalWindowList && (widget != gGeckoAppModalWindowList->window))
            return;
          if (widget->HasModalDescendents())
            return;
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

// Custom NSColor subclass where most of the work takes place for drawing in
// the titlebar area. Not used in drawsContentsIntoWindowFrame mode.
@implementation TitlebarAndBackgroundColor

- (id)initWithWindow:(ToolbarWindow*)aWindow
{
  if ((self = [super init])) {
    mWindow = aWindow; // weak ref to avoid a cycle
  }
  return self;
}

static void
DrawNativeTitlebar(CGContextRef aContext, CGRect aTitlebarRect,
                   CGFloat aUnifiedToolbarHeight, BOOL aIsMain)
{
  if (aTitlebarRect.size.width * aTitlebarRect.size.height > CUIDRAW_MAX_AREA) {
    return;
  }

  CUIDraw([NSWindow coreUIRenderer], aTitlebarRect, aContext,
          (CFDictionaryRef)[NSDictionary dictionaryWithObjectsAndKeys:
            @"kCUIWidgetWindowFrame", @"widget",
            @"regularwin", @"windowtype",
            (aIsMain ? @"normal" : @"inactive"), @"state",
            [NSNumber numberWithDouble:aUnifiedToolbarHeight], @"kCUIWindowFrameUnifiedTitleBarHeightKey",
            [NSNumber numberWithBool:YES], @"kCUIWindowFrameDrawTitleSeparatorKey",
            nil],
          nil);

  if (nsCocoaFeatures::OnLionOrLater()) {
    // On Lion the call to CUIDraw doesn't draw the top pixel strip at some
    // window widths. We don't want to have a flickering transparent line, so
    // we overdraw it.
    CGContextSetRGBFillColor(aContext, 0.95, 0.95, 0.95, 1);
    CGContextFillRect(aContext, CGRectMake(0, CGRectGetMaxY(aTitlebarRect) - 1,
                                           aTitlebarRect.size.width, 1));
  }
}

// Pattern draw callback for standard titlebar gradients and solid titlebar colors
static void
TitlebarDrawCallback(void* aInfo, CGContextRef aContext)
{
  ToolbarWindow *window = (ToolbarWindow*)aInfo;
  if (![window drawsContentsIntoWindowFrame]) {
    NSRect titlebarRect = [window titlebarRect];
    BOOL isMain = [window isMainWindow];
    NSColor *titlebarColor = [window titlebarColorForActiveWindow:isMain];
    if (!titlebarColor) {
      // If the titlebar color is nil, draw the default titlebar shading.
      DrawNativeTitlebar(aContext, NSRectToCGRect(titlebarRect),
                         [window unifiedToolbarHeight], isMain);
    } else {
      // If the titlebar color is not nil, just set and draw it normally.
      [NSGraphicsContext saveGraphicsState];
      [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:aContext flipped:NO]];
      [titlebarColor set];
      NSRectFill(titlebarRect);
      [NSGraphicsContext restoreGraphicsState];
    }
  }
}

- (void)setFill
{
  float patternWidth = [mWindow frame].size.width;

  CGPatternCallbacks callbacks = {0, &TitlebarDrawCallback, NULL};
  CGPatternRef pattern = CGPatternCreate(mWindow, CGRectMake(0.0f, 0.0f, patternWidth, [mWindow frame].size.height), 
                                         CGAffineTransformIdentity, patternWidth, [mWindow frame].size.height,
                                         kCGPatternTilingConstantSpacing, true, &callbacks);

  // Set the pattern as the fill, which is what we were asked to do. All our
  // drawing will take place in the patternDraw callback.
  CGColorSpaceRef patternSpace = CGColorSpaceCreatePattern(NULL);
  CGContextRef context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
  CGContextSetFillColorSpace(context, patternSpace);
  CGColorSpaceRelease(patternSpace);
  CGFloat component = 1.0f;
  CGContextSetFillPattern(context, pattern, &component);
  CGPatternRelease(pattern);
}

- (void)set
{
  [self setFill];
}

- (NSString*)colorSpaceName
{
  return NSDeviceRGBColorSpace;
}

@end

@implementation PopupWindow

- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask
      backing:(NSBackingStoreType)bufferingType defer:(BOOL)deferCreation
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  mIsContextMenu = false;
  return [super initWithContentRect:contentRect styleMask:styleMask
          backing:bufferingType defer:deferCreation];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (BOOL)isContextMenu
{
  return mIsContextMenu;
}

- (void)setIsContextMenu:(BOOL)flag
{
  mIsContextMenu = flag;
}

- (BOOL)canBecomeMainWindow
{
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

- (BOOL)canBecomeKeyWindow
{
  return YES;
}

- (void)sendEvent:(NSEvent *)anEvent
{
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
    case NSOtherMouseDragged:
    {
      // Drop all mouse events if a modal window has appeared above us.
      // This helps make us behave as if the OS were running a "real" modal
      // event loop.
      id delegate = [self delegate];
      if (delegate && [delegate isKindOfClass:[WindowDelegate class]]) {
        nsCocoaWindow *widget = [(WindowDelegate *)delegate geckoWidget];
        if (widget) {
          if (gGeckoAppModalWindowList && (widget != gGeckoAppModalWindowList->window))
            return;
          if (widget->HasModalDescendents())
            return;
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
- (BOOL)canBecomeMainWindow
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  if (![self isVisible])
    return NO;
  return YES;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

// Retain and release "self" to avoid crashes when our widget (and its native
// window) is closed as a result of processing a key equivalent (e.g.
// Command+w or Command+q).  This workaround is only needed for a window
// that can become key.
- (BOOL)performKeyEquivalent:(NSEvent*)theEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  NSWindow *nativeWindow = [self retain];
  BOOL retval = [super performKeyEquivalent:theEvent];
  [nativeWindow release];
  return retval;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NO);
}

@end
