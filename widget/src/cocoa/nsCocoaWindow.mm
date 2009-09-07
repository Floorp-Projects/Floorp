/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Aas <josh@mozilla.com>
 *   Colin Barrett <cbarrett@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsCocoaWindow.h"

#include "nsObjCExceptions.h"
#include "nsCOMPtr.h"
#include "nsWidgetsCID.h"
#include "nsGUIEvent.h"
#include "nsIRollupListener.h"
#include "nsChildView.h"
#include "nsWindowMap.h"
#include "nsAppShell.h"
#include "nsIAppShell.h"
#include "nsIAppShellService.h"
#include "nsIBaseWindow.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIXULWindow.h"
#include "nsIPrefService.h"
#include "nsIPrefBranch.h"
#include "nsToolkit.h"
#include "nsPrintfCString.h"
#include "nsIServiceManager.h"
#include "nsIDOMWindow.h"
#include "nsPIDOMWindow.h"
#include "nsIDOMElement.h"
#include "nsThreadUtils.h"
#include "nsMenuBarX.h"
#include "nsMenuUtilsX.h"
#include "nsStyleConsts.h"
#include "nsNativeThemeColors.h"
#include "nsChildView.h"

#include "gfxPlatform.h"
#include "qcms.h"

// defined in nsAppShell.mm
extern nsCocoaAppModalWindowList *gCocoaAppModalWindowList;

PRInt32 gXULModalLevel = 0;

// In principle there should be only one app-modal window at any given time.
// But sometimes, despite our best efforts, another window appears above the
// current app-modal window.  So we need to keep a linked list of app-modal
// windows.  (A non-sheet window that appears above an app-modal window is
// also made app-modal.)  See nsCocoaWindow::SetModal().
nsCocoaWindowList *gGeckoAppModalWindowList = NULL;

PRBool gConsumeRollupEvent;

// defined in nsMenuBarX.mm
extern NSMenu* sApplicationMenu; // Application menu shared by all menubars

// defined in nsChildView.mm
extern nsIRollupListener * gRollupListener;
extern nsIWidget         * gRollupWidget;
extern BOOL                gSomeMenuBarPainted;

#define NS_APPSHELLSERVICE_CONTRACTID "@mozilla.org/appshell/appShellService;1"

NS_IMPL_ISUPPORTS_INHERITED1(nsCocoaWindow, Inherited, nsPIWidgetCocoa)

// A note on testing to see if your object is a sheet...
// |mWindowType == eWindowType_sheet| is true if your gecko nsIWidget is a sheet
// widget - whether or not the sheet is showing. |[mWindow isSheet]| will return
// true *only when the sheet is actually showing*. Choose your test wisely.

// roll up any popup windows
static void RollUpPopups()
{
  if (gRollupListener && gRollupWidget)
    gRollupListener->Rollup(nsnull, nsnull);
}

nsCocoaWindow::nsCocoaWindow()
: mParent(nsnull)
, mWindow(nil)
, mDelegate(nil)
, mSheetWindowParent(nil)
, mPopupContentView(nil)
, mIsResizing(PR_FALSE)
, mWindowMadeHere(PR_FALSE)
, mSheetNeedsShow(PR_FALSE)
, mFullScreen(PR_FALSE)
, mModal(PR_FALSE)
, mNumModalDescendents(0)
{

}

// Sometimes NSViews are removed from a window or moved to a new window.
// Since our ChildViews have their own mWindow field instead of always using
// [view window], we need to notify them when this happens.
static void SetNativeWindowOnSubviews(NSView *aNativeView, NSWindow *aWin)
{
  if (!aNativeView)
    return;
  if ([aNativeView respondsToSelector:@selector(setNativeWindow:)])
    [(NSView<mozView>*)aNativeView setNativeWindow:aWin];
  NSArray *immediateSubviews = [aNativeView subviews];
  int count = [immediateSubviews count];
  for (int i = 0; i < count; ++i)
    SetNativeWindowOnSubviews((NSView *)[immediateSubviews objectAtIndex:i], aWin);
}


// Under unusual circumstances, an nsCocoaWindow object can be destroyed
// before the nsChildView objects it contains are destroyed.  But this will
// invalidate the (weak) mWindow variable in these nsChildView objects
// before their own destructors have been called.  So we need to null-out
// this variable in our nsChildView objects as we're destroyed.  This helps
// resolve bmo bug 479749.
static void TellNativeViewsGoodbye(NSView *aNativeView)
{
  SetNativeWindowOnSubviews(aNativeView, nil);
}

void nsCocoaWindow::DestroyNativeWindow()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // We want to unhook the delegate here because we don't want events
  // sent to it after this object has been destroyed.
  [mWindow setDelegate:nil];
  [mWindow close];
  [mDelegate autorelease];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsCocoaWindow::~nsCocoaWindow()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (mFullScreen) {
    nsCocoaUtils::HideOSChromeOnScreen(PR_FALSE, [mWindow screen]);
  }

  // Notify the children that we're gone.  Popup windows (e.g. tooltips) can
  // have nsChildView children.  'kid' is an nsChildView object if and only if
  // its 'type' is 'eWindowType_child'.  childView->ResetParent() can change
  // our list of children while it's being iterated, so the way we iterate the
  // list must allow for this.
  for (nsIWidget* kid = mLastChild; kid;) {
    nsWindowType kidType;
    kid->GetWindowType(kidType);
    if (kidType == eWindowType_child) {
      nsChildView* childView = static_cast<nsChildView*>(kid);
      kid = kid->GetPrevSibling();
      childView->ResetParent();
    } else {
      nsCocoaWindow* childWindow = static_cast<nsCocoaWindow*>(kid);
      childWindow->mParent = nsnull;
      kid = kid->GetPrevSibling();
    }
  }

  if (mWindow) {
    TellNativeViewsGoodbye([mWindow contentView]);
    if (mWindowMadeHere) {
      DestroyNativeWindow();
    }
  }

  NS_IF_RELEASE(mPopupContentView);

  // Deal with the possiblity that we're being destroyed while running modal.
  NS_ASSERTION(!mModal, "Widget destroyed while running modal!");
  if (mModal) {
    --gXULModalLevel;
    NS_ASSERTION(gXULModalLevel >= 0, "Wierdness setting modality!");
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Very large windows work in Cocoa, but can take a long time to
// process (multiple minutes), during which time the system is
// unresponsive and seems hung. Although it's likely that windows
// much larger than screen size are bugs, be conservative and only
// intervene if the values are so large as to hog the cpu.
#define SIZE_LIMIT 100000
static bool WindowSizeAllowed(PRInt32 aWidth, PRInt32 aHeight)
{
  if (aWidth > SIZE_LIMIT) {
    NS_ERROR(nsPrintfCString(256, "Requested Cocoa window width of %d is too much, max allowed is %d\n",
                             aWidth, SIZE_LIMIT).get());
    return false;
  }
  if (aHeight > SIZE_LIMIT) {
    NS_ERROR(nsPrintfCString(256, "Requested Cocoa window height of %d is too much, max allowed is %d\n",
                             aHeight, SIZE_LIMIT).get());
    return false;
  }
  return true;
}

// Some applications like Camino use native popup windows
// (native context menus, native tooltips)
static PRBool UseNativePopupWindows()
{
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (!prefs)
    return PR_FALSE;

  PRBool useNativePopupWindows;
  nsresult rv = prefs->GetBoolPref("ui.use_native_popup_windows", &useNativePopupWindows);
  return (NS_SUCCEEDED(rv) && useNativePopupWindows);
}

// Utility method for implementing both Create(nsIWidget ...) and
// Create(nsNativeWidget...)
nsresult nsCocoaWindow::StandardCreate(nsIWidget *aParent,
                        const nsIntRect &aRect,
                        EVENT_CALLBACK aHandleEventFunction,
                        nsIDeviceContext *aContext,
                        nsIAppShell *aAppShell,
                        nsIToolkit *aToolkit,
                        nsWidgetInitData *aInitData,
                        nsNativeWidget aNativeWindow)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!WindowSizeAllowed(aRect.width, aRect.height))
    return NS_ERROR_FAILURE;

  Inherited::BaseCreate(aParent, aRect, aHandleEventFunction, aContext, aAppShell,
                        aToolkit, aInitData);

  mParent = aParent;
  SetWindowType(aInitData ? aInitData->mWindowType : eWindowType_toplevel);
  SetBorderStyle(aInitData ? aInitData->mBorderStyle : eBorderStyle_default);

  // Create a window if we aren't given one, or if this should be a non-native popup.
  if ((mWindowType == eWindowType_popup) ? !UseNativePopupWindows() : !aNativeWindow) {
    nsresult rv = CreateNativeWindow(nsCocoaUtils::GeckoRectToCocoaRect(aRect),
                                     mBorderStyle, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);

    if (mWindowType == eWindowType_popup) {
      rv = CreatePopupContentView(aRect, aHandleEventFunction, aContext, aAppShell, aToolkit);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  } else {
    mWindow = (NSWindow*)aNativeWindow;
    [[WindowDataMap sharedWindowDataMap] ensureDataForWindow:mWindow];
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

static unsigned int WindowMaskForBorderStyle(nsBorderStyle aBorderStyle)
{
  PRBool allOrDefault = (aBorderStyle == eBorderStyle_all ||
                         aBorderStyle == eBorderStyle_default);

  /* Apple's docs on NSWindow styles say that "a window's style mask should
   * include NSTitledWindowMask if it includes any of the others [besides
   * NSBorderlessWindowMask]".  This implies that a borderless window
   * shouldn't have any other styles than NSBorderlessWindowMask.
   */
  if (!allOrDefault && !(aBorderStyle & eBorderStyle_title))
    return NSBorderlessWindowMask;

  unsigned int mask = NSTitledWindowMask | NSMiniaturizableWindowMask;
  if (allOrDefault || aBorderStyle & eBorderStyle_close)
    mask |= NSClosableWindowMask;
  if (allOrDefault || aBorderStyle & eBorderStyle_resizeh)
    mask |= NSResizableWindowMask;

  return mask;
}

// If aRectIsFrameRect, aRect specifies the frame rect of the new window.
// Otherwise, aRect.x/y specify the position of the window's frame relative to
// the bottom of the menubar and aRect.width/height specify the size of the
// content rect.
nsresult nsCocoaWindow::CreateNativeWindow(const NSRect &aRect,
                                           nsBorderStyle aBorderStyle,
                                           PRBool aRectIsFrameRect)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // We default to NSBorderlessWindowMask, add features if needed.
  unsigned int features = NSBorderlessWindowMask;

  // Configure the window we will create based on the window type.
  switch (mWindowType)
  {
    case eWindowType_invisible:
    case eWindowType_child:
    case eWindowType_popup:
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

  Class windowClass = [NSWindow class];
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

  // Make sure that the content rect we gave has been honored.
  NSRect wantedFrame = [mWindow frameRectForContentRect:contentRect];
  if (!NSEqualRects([mWindow frame], wantedFrame)) {
    // This can happen when the window is not on the primary screen.
    [mWindow setFrame:wantedFrame display:NO];
  }

  if (mWindowType == eWindowType_invisible) {
    [mWindow setLevel:kCGDesktopWindowLevelKey];
  } else if (mWindowType == eWindowType_popup) {
    [mWindow setLevel:NSPopUpMenuWindowLevel];
    [mWindow setHasShadow:YES];
  }

  [mWindow setBackgroundColor:[NSColor whiteColor]];
  [mWindow setContentMinSize:NSMakeSize(60, 60)];
  [mWindow disableCursorRects];

  // setup our notification delegate. Note that setDelegate: does NOT retain.
  mDelegate = [[WindowDelegate alloc] initWithGeckoWindow:this];
  [mWindow setDelegate:mDelegate];

  [[WindowDataMap sharedWindowDataMap] ensureDataForWindow:mWindow];
  mWindowMadeHere = PR_TRUE;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsCocoaWindow::CreatePopupContentView(const nsIntRect &aRect,
                             EVENT_CALLBACK aHandleEventFunction,
                             nsIDeviceContext *aContext,
                             nsIAppShell *aAppShell,
                             nsIToolkit *aToolkit)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // We need to make our content view a ChildView.
  mPopupContentView = new nsChildView();
  if (!mPopupContentView)
    return NS_ERROR_FAILURE;

  NS_ADDREF(mPopupContentView);

  nsIWidget* thisAsWidget = static_cast<nsIWidget*>(this);
  mPopupContentView->StandardCreate(thisAsWidget, aRect, aHandleEventFunction,
                                    aContext, aAppShell, aToolkit, nsnull, nsnull);

  ChildView* newContentView = (ChildView*)mPopupContentView->GetNativeData(NS_NATIVE_WIDGET);
  [mWindow setContentView:newContentView];

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// Create a nsCocoaWindow using a native window provided by the application
NS_IMETHODIMP nsCocoaWindow::Create(nsNativeWidget aNativeWindow,
                      const nsIntRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  return(StandardCreate(nsnull, aRect, aHandleEventFunction, aContext,
                        aAppShell, aToolkit, aInitData, aNativeWindow));
}

NS_IMETHODIMP nsCocoaWindow::Create(nsIWidget* aParent,
                      const nsIntRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  return(StandardCreate(aParent, aRect, aHandleEventFunction, aContext,
                        aAppShell, aToolkit, aInitData, nsnull));
}

NS_IMETHODIMP nsCocoaWindow::Destroy()
{
  if (mPopupContentView)
    mPopupContentView->Destroy();

  nsBaseWidget::Destroy();
  nsBaseWidget::OnDestroy();

  return NS_OK;
}

nsIWidget* nsCocoaWindow::GetSheetWindowParent(void)
{
  if (mWindowType != eWindowType_sheet)
    return nsnull;
  nsCocoaWindow *parent = static_cast<nsCocoaWindow*>(mParent);
  while (parent && (parent->mWindowType == eWindowType_sheet))
    parent = static_cast<nsCocoaWindow*>(parent->mParent);
  return parent;
}

void* nsCocoaWindow::GetNativeData(PRUint32 aDataType)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSNULL;

  void* retVal = nsnull;
  
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
      // and it doesn't matter so just return nsnull.
      NS_ERROR("Requesting NS_NATIVE_GRAPHIC on a top-level window!");
      break;
  }

  return retVal;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSNULL;
}

NS_IMETHODIMP nsCocoaWindow::IsVisible(PRBool & aState)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  aState = ([mWindow isVisible] || mSheetNeedsShow);
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsCocoaWindow::SetModal(PRBool aState)
{
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
    NS_ASSERTION(gXULModalLevel >= 0, "Mismatched call to nsCocoaWindow::SetModal(PR_FALSE)!");
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
        [mWindow setLevel:NSPopUpMenuWindowLevel];
      else
        [mWindow setLevel:NSNormalWindowLevel];
    }
  }
  return NS_OK;
}

// Hide or show this window
NS_IMETHODIMP nsCocoaWindow::Show(PRBool bState)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

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
      PRBool parentIsSheet = PR_FALSE;
      if (NS_SUCCEEDED(piParentWidget->GetIsSheet(&parentIsSheet)) &&
          parentIsSheet) {
        piParentWidget->GetSheetWindowParent(&topNonSheetWindow);
        [NSApp endSheet:nativeParentWindow];
        [nativeParentWindow setAcceptsMouseMovedEvents:NO];
      }

      nsCocoaWindow* sheetShown = nsnull;
      if (NS_SUCCEEDED(piParentWidget->GetChildSheet(PR_TRUE, &sheetShown)) &&
          (!sheetShown || sheetShown == this)) {
        // If this sheet is already the sheet actually being shown, don't
        // tell it to show again. Otherwise the number of calls to
        // [NSApp beginSheet...] won't match up with [NSApp endSheet...].
        if (![mWindow isVisible]) {
          mSheetNeedsShow = PR_FALSE;
          mSheetWindowParent = topNonSheetWindow;
          // Only set contextInfo if our parent isn't a sheet.
          NSWindow* contextInfo = parentIsSheet ? nil : mSheetWindowParent;
          [TopLevelWindowData deactivateInWindow:mSheetWindowParent];
          [mWindow setAcceptsMouseMovedEvents:YES];
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
        mSheetNeedsShow = PR_TRUE;
      }
    }
    else if (mWindowType == eWindowType_popup) {
      // If a popup window is shown after being hidden, it needs to be "reset"
      // for it to receive any mouse events aside from mouse-moved events
      // (because it was removed from the "window cache" when it was hidden
      // -- see below).  Setting the window number to -1 and then back to its
      // original value seems to accomplish this.  The idea was "borrowed"
      // from the Java Embedding Plugin.
      int windowNumber = [mWindow windowNumber];
      [mWindow _setWindowNumber:-1];
      [mWindow _setWindowNumber:windowNumber];
      [mWindow setAcceptsMouseMovedEvents:YES];
      // For reasons that aren't yet clear, calls to [NSWindow orderFront:] or
      // [NSWindow makeKeyAndOrderFront:] can sometimes trigger "Error (1000)
      // creating CGSWindow", which in turn triggers an internal inconsistency
      // NSException.  These errors shouldn't be fatal.  So we need to wrap
      // calls to ...orderFront: in LOGONLY blocks.  See bmo bug 470864.
      NS_OBJC_BEGIN_TRY_LOGONLY_BLOCK;
      [mWindow orderFront:nil];
      NS_OBJC_END_TRY_LOGONLY_BLOCK;
      SendSetZLevelEvent();
      // If our popup window is a non-native context menu, tell the OS (and
      // other programs) that a menu has opened.  This is how the OS knows to
      // close other programs' context menus when ours open.
      if ([mWindow isKindOfClass:[PopupWindow class]] &&
          [(PopupWindow*) mWindow isContextMenu]) {
        [[NSDistributedNotificationCenter defaultCenter]
          postNotificationName:@"com.apple.HIToolbox.beginMenuTrackingNotification"
                        object:@"org.mozilla.gecko.PopupWindow"];
      }

      // if a parent was supplied, set its child window. This will cause the
      // child window to appear above the parent and move when the parent
      // does. Setting this needs to happen after the _setWindowNumber calls
      // above, otherwise the window doesn't focus properly.
      if (nativeParentWindow)
        [nativeParentWindow addChildWindow:mWindow
                            ordered:NSWindowAbove];
    }
    else {
      [mWindow setAcceptsMouseMovedEvents:YES];
      NS_OBJC_BEGIN_TRY_LOGONLY_BLOCK;
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
        mSheetNeedsShow = PR_FALSE;
      }
      else {
        // get sheet's parent *before* hiding the sheet (which breaks the linkage)
        NSWindow* sheetParent = mSheetWindowParent;
        
        // hide the sheet
        [NSApp endSheet:mWindow];
        
        [mWindow setAcceptsMouseMovedEvents:NO];

        [TopLevelWindowData deactivateInWindow:mWindow];

        nsCocoaWindow* siblingSheetToShow = nsnull;
        PRBool parentIsSheet = PR_FALSE;

        if (nativeParentWindow && piParentWidget &&
            NS_SUCCEEDED(piParentWidget->GetChildSheet(PR_FALSE, &siblingSheetToShow)) &&
            siblingSheetToShow) {
          // First, give sibling sheets an opportunity to show.
          siblingSheetToShow->Show(PR_TRUE);
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
            PRBool grandparentIsSheet = PR_FALSE;
            if (piGrandparentWidget && NS_SUCCEEDED(piGrandparentWidget->GetIsSheet(&grandparentIsSheet)) &&
                grandparentIsSheet) {
                contextInfo = nil;
            }
          }
          // If there are no sibling sheets, but the parent is a sheet, restore
          // it.  It wasn't sent any deactivate events when it was hidden, so
          // don't call through Show, just let the OS put it back up.
          [nativeParentWindow setAcceptsMouseMovedEvents:YES];
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
          [sheetParent setAcceptsMouseMovedEvents:YES];
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

      // it's very important to turn off mouse moved events when hiding a window, otherwise
      // the windows' tracking rects will interfere with each other. (bug 356528)
      [mWindow setAcceptsMouseMovedEvents:NO];

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

nsresult
nsCocoaWindow::ConfigureChildren(const nsTArray<Configuration>& aConfigurations)
{
  if (mPopupContentView) {
    mPopupContentView->ConfigureChildren(aConfigurations);
  }
  return NS_OK;
}

void
nsCocoaWindow::Scroll(const nsIntPoint& aDelta,
                      const nsTArray<nsIntRect>& aDestRects,
                      const nsTArray<Configuration>& aConfigurations)
{
  if (mPopupContentView) {
    mPopupContentView->Scroll(aDelta, aDestRects, aConfigurations);
  }
}

void nsCocoaWindow::MakeBackgroundTransparent(PRBool aTransparent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  BOOL currentTransparency = ![mWindow isOpaque];
  if (aTransparent != currentTransparency) {
    [mWindow setOpaque:!aTransparent];
    [mWindow setBackgroundColor:(aTransparent ? [NSColor clearColor] : [NSColor whiteColor])];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

nsTransparencyMode nsCocoaWindow::GetTransparencyMode()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  return [mWindow isOpaque] ? eTransparencyOpaque : eTransparencyTransparent;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(eTransparencyOpaque);
}

// This is called from nsMenuPopupFrame when making a popup transparent.
// For other window types, nsChildView::SetTransparencyMode is used.
void nsCocoaWindow::SetTransparencyMode(nsTransparencyMode aMode)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  BOOL isTransparent = aMode == eTransparencyTransparent;

  BOOL currentTransparency = ![mWindow isOpaque];
  if (isTransparent != currentTransparency) {
    // Take care of window transparency
    MakeBackgroundTransparent(isTransparent);
    // Make sure our content view is also transparent
    if (mPopupContentView) {
      ChildView *childView = (ChildView*)mPopupContentView->GetNativeData(NS_NATIVE_WIDGET);
      if (childView) {
        [childView setTransparent:isTransparent];
      }
    }
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

NS_METHOD nsCocoaWindow::AddEventListener(nsIEventListener * aListener)
{
  nsBaseWidget::AddEventListener(aListener);

  if (mPopupContentView)
    mPopupContentView->AddEventListener(aListener);

  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::Enable(PRBool aState)
{
  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::IsEnabled(PRBool *aState)
{
  if (aState)
    *aState = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::ConstrainPosition(PRBool aAllowSlop,
                                               PRInt32 *aX, PRInt32 *aY)
{
  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::Move(PRInt32 aX, PRInt32 aY)
{
  if (!mWindow || (mBounds.x == aX && mBounds.y == aY))
    return NS_OK;

  // The point we have is in Gecko coordinates (origin top-left). Convert
  // it to Cocoa ones (origin bottom-left).
  NSPoint coord = {aX, nsCocoaUtils::FlippedScreenY(aY)};
  [mWindow setFrameTopLeftPoint:coord];

  return NS_OK;
}

// Position the window behind the given window
NS_METHOD nsCocoaWindow::PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                     nsIWidget *aWidget, PRBool aActivate)
{
  return NS_OK;
}

// Note bug 278777, we need to update state when the window is unminimized
// from the dock by users.
NS_METHOD nsCocoaWindow::SetSizeMode(PRInt32 aMode)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  PRInt32 previousMode;
  nsBaseWidget::GetSizeMode(&previousMode);

  nsresult rv = nsBaseWidget::SetSizeMode(aMode);
  NS_ENSURE_SUCCESS(rv, rv);

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

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// This has to preserve the window's frame bounds.
// This method requires (as does the Windows impl.) that you call Resize shortly
// after calling HideWindowChrome. See bug 498835 for fixing this.
NS_IMETHODIMP nsCocoaWindow::HideWindowChrome(PRBool aShouldHide)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!mWindowMadeHere ||
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

  // Recreate the window with the right border style.
  NSRect frameRect = [mWindow frame];
  DestroyNativeWindow();
  nsresult rv = CreateNativeWindow(frameRect, aShouldHide ? eBorderStyle_none : mBorderStyle, PR_TRUE);
  NS_ENSURE_SUCCESS(rv, rv);

  // Reparent the content view.
  [mWindow setContentView:contentView];
  [contentView release];
  SetNativeWindowOnSubviews(contentView, mWindow);

  // Reparent child windows.
  enumerator = [childWindows objectEnumerator];
  while ((child = [enumerator nextObject])) {
    [mWindow addChildWindow:child ordered:NSWindowAbove];
  }

  // Show the new window.
  if (isVisible) {
    rv = Show(PR_TRUE);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}


NS_METHOD nsCocoaWindow::MakeFullScreen(PRBool aFullScreen)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_ASSERTION(mFullScreen != aFullScreen, "Unnecessary MakeFullScreen call");

  NSDisableScreenUpdates();
  nsresult rv = nsBaseWidget::MakeFullScreen(aFullScreen);
  NSEnableScreenUpdates();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCocoaUtils::HideOSChromeOnScreen(aFullScreen, [mWindow screen]);

  mFullScreen = aFullScreen;

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}


NS_IMETHODIMP nsCocoaWindow::Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!WindowSizeAllowed(aWidth, aHeight))
    return NS_ERROR_FAILURE;

  nsIntRect windowBounds(nsCocoaUtils::CocoaRectToGeckoRect([mWindow frame]));
  BOOL isMoving = (windowBounds.x != aX || windowBounds.y != aY);
  BOOL isResizing = (windowBounds.width != aWidth || windowBounds.height != aHeight);

  if (IsResizing() || !mWindow || (!isMoving && !isResizing))
    return NS_OK;

  nsIntRect geckoRect(aX, aY, aWidth, aHeight);
  NSRect newFrame = nsCocoaUtils::GeckoRectToCocoaRect(geckoRect);

  // We have to report the size event -first-, to make sure that content
  // repositions itself.  Cocoa views are anchored at the bottom left,
  // so if we don't do this our child view will end up being stuck in the
  // wrong place during a resize.
  if (isResizing)
    ReportSizeEvent(&newFrame);

  StartResizing();
  // We ignore aRepaint -- we have to call display:YES, otherwise the
  // title bar doesn't immediately get repainted and is displayed in
  // the wrong place, leading to a visual jump.
  [mWindow setFrame:newFrame display:YES];
  StopResizing();

  // now, check whether we got the frame that we wanted
  NSRect actualFrame = [mWindow frame];
  if (newFrame.size.width != actualFrame.size.width || newFrame.size.height != actualFrame.size.height) {
    // We didn't; the window must have been too big or otherwise invalid.
    // Report -another- resize in this case, to make sure things are in
    // the right place.  This will cause some visual jitter, but
    // shouldn't happen often.
    ReportSizeEvent();
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsCocoaWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if (!WindowSizeAllowed(aWidth, aHeight))
    return NS_ERROR_FAILURE;

  nsIntRect windowBounds(nsCocoaUtils::CocoaRectToGeckoRect([mWindow frame]));
  return Resize(windowBounds.x, windowBounds.y, aWidth, aHeight, aRepaint);

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsCocoaWindow::GetScreenBounds(nsIntRect &aRect)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  aRect = nsCocoaUtils::CocoaRectToGeckoRect([mWindow frame]);
  // printf("GetScreenBounds: output: %d,%d,%d,%d\n", aRect.x, aRect.y, aRect.width, aRect.height);
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsCocoaWindow::SetTitle(const nsAString& aTitle)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  const nsString& strTitle = PromiseFlatString(aTitle);
  NSString* title = [NSString stringWithCharacters:strTitle.get() length:strTitle.Length()];
  [mWindow setTitle:title];

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsCocoaWindow::Invalidate(const nsIntRect & aRect, PRBool aIsSynchronous)
{
  if (mPopupContentView)
    return mPopupContentView->Invalidate(aRect, aIsSynchronous);

  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::Invalidate(PRBool aIsSynchronous)
{
  if (mPopupContentView)
    return mPopupContentView->Invalidate(aIsSynchronous);

  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::Update()
{
  if (mPopupContentView)
    return mPopupContentView->Update();

  return NS_OK;
}

// Pass notification of some drag event to Gecko
//
// The drag manager has let us know that something related to a drag has
// occurred in this window. It could be any number of things, ranging from 
// a drop, to a drag enter/leave, or a drag over event. The actual event
// is passed in |aMessage| and is passed along to our event hanlder so Gecko
// knows about it.
PRBool nsCocoaWindow::DragEvent(unsigned int aMessage, Point aMouseGlobal, UInt16 aKeyModifiers)
{
  return PR_FALSE;
}

NS_IMETHODIMP nsCocoaWindow::SendSetZLevelEvent()
{
  nsZLevelEvent event(PR_TRUE, NS_SETZLEVEL, this);

  event.refPoint.x = mBounds.x;
  event.refPoint.y = mBounds.y;
  event.time = PR_IntervalNow();

  event.mImmediate = PR_TRUE;

  nsEventStatus status = nsEventStatus_eIgnore;
  DispatchEvent(&event, status);

  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::GetChildSheet(PRBool aShown, nsCocoaWindow** _retval)
{
  nsIWidget* child = GetFirstChild();

  while (child) {
    nsWindowType type;
    if (NS_SUCCEEDED(child->GetWindowType(type)) && type == eWindowType_sheet) {
      // if it's a sheet, it must be an nsCocoaWindow
      nsCocoaWindow* cocoaWindow = static_cast<nsCocoaWindow*>(child);
      if ((aShown && [cocoaWindow->mWindow isVisible]) ||
          (!aShown && cocoaWindow->mSheetNeedsShow)) {
        *_retval = cocoaWindow;
        return NS_OK;
      }
    }
    child = child->GetNextSibling();
  }

  *_retval = nsnull;

  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::GetRealParent(nsIWidget** parent)
{
  *parent = mParent;
  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::GetIsSheet(PRBool* isSheet)
{
  mWindowType == eWindowType_sheet ? *isSheet = PR_TRUE : *isSheet = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::GetSheetWindowParent(NSWindow** sheetWindowParent)
{
  *sheetWindowParent = mSheetWindowParent;
  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::ResetInputState()
{
  return NS_OK;
}

// Invokes callback and ProcessEvent methods on Event Listener object
NS_IMETHODIMP 
nsCocoaWindow::DispatchEvent(nsGUIEvent* event, nsEventStatus& aStatus)
{
  aStatus = nsEventStatus_eIgnore;

  nsIWidget* aWidget = event->widget;
  NS_IF_ADDREF(aWidget);

  if (mEventCallback)
    aStatus = (*mEventCallback)(event);

  // Dispatch to event listener if event was not consumed
  if (mEventListener && aStatus != nsEventStatus_eConsumeNoDefault)
    aStatus = mEventListener->ProcessEvent(*event);

  NS_IF_RELEASE(aWidget);

  return NS_OK;
}

static nsSizeMode
GetWindowSizeMode(NSWindow* aWindow) {
  if ([aWindow isMiniaturized])
    return nsSizeMode_Minimized;
  if (([aWindow styleMask] & NSResizableWindowMask) && [aWindow isZoomed])
    return nsSizeMode_Maximized;
  return nsSizeMode_Normal;
}

void
nsCocoaWindow::DispatchSizeModeEvent()
{
  nsSizeModeEvent event(PR_TRUE, NS_SIZEMODE, this);
  event.mSizeMode = GetWindowSizeMode(mWindow);
  event.time = PR_IntervalNow();

  nsEventStatus status = nsEventStatus_eIgnore;
  DispatchEvent(&event, status);
}

void
nsCocoaWindow::ReportSizeEvent(NSRect *r)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSRect windowFrame;
  if (r)
    windowFrame = [mWindow contentRectForFrameRect:(*r)];
  else
    windowFrame = [mWindow contentRectForFrameRect:[mWindow frame]];
  mBounds.width  = nscoord(windowFrame.size.width);
  mBounds.height = nscoord(windowFrame.size.height);

  nsSizeEvent sizeEvent(PR_TRUE, NS_SIZE, this);
  sizeEvent.time = PR_IntervalNow();

  sizeEvent.windowSize = &mBounds;
  sizeEvent.mWinWidth  = mBounds.width;
  sizeEvent.mWinHeight = mBounds.height;

  nsEventStatus status = nsEventStatus_eIgnore;
  DispatchEvent(&sizeEvent, status);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

void nsCocoaWindow::SetMenuBar(nsMenuBarX *aMenuBar)
{
  if (mMenuBar)
    mMenuBar->SetParent(nsnull);
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

NS_IMETHODIMP nsCocoaWindow::SetFocus(PRBool aState)
{
  if (mPopupContentView) {
    mPopupContentView->SetFocus(aState);
  }
  else if (aState && [mWindow isVisible]) {
    // if the window is shown, move it to the front
    [mWindow setAcceptsMouseMovedEvents:YES];
    [mWindow makeKeyAndOrderFront:nil];
    SendSetZLevelEvent();
  }

  return NS_OK;
}

nsIntPoint nsCocoaWindow::WidgetToScreenOffset()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_RETURN;

  nsIntRect r = nsCocoaUtils::CocoaRectToGeckoRect([mWindow contentRectForFrameRect:[mWindow frame]]);

  return r.TopLeft();

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(nsIntPoint(0,0));
}

nsMenuBarX* nsCocoaWindow::GetMenuBar()
{
  return mMenuBar;
}

NS_IMETHODIMP nsCocoaWindow::CaptureRollupEvents(nsIRollupListener * aListener, 
                                                 PRBool aDoCapture, 
                                                 PRBool aConsumeRollupEvent)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  NS_IF_RELEASE(gRollupListener);
  NS_IF_RELEASE(gRollupWidget);
  
  if (aDoCapture) {
    gRollupListener = aListener;
    NS_ADDREF(aListener);
    gRollupWidget = this;
    NS_ADDREF(this);

    gConsumeRollupEvent = aConsumeRollupEvent;

    // Sometimes more than one popup window can be visible at the same time
    // (e.g. nested non-native context menus, or the test case (attachment
    // 276885) for bmo bug 392389, which displays a non-native combo-box in
    // a non-native popup window).  In these cases the "active" popup window
    // (the one that corresponds to the current gRollupWidget) should be the
    // topmost -- the (nested) context menu the mouse is currently over, or
    // the combo-box's drop-down list (when it's displayed).  But (among
    // windows that have the same "level") OS X makes topmost the window that
    // last received a mouse-down event, which may be incorrect (in the combo-
    // box case, it makes topmost the window containing the combo-box).  So
    // here we fiddle with a non-native popup window's level to make sure the
    // "active" one is always above any other non-native popup windows that
    // may be visible.
    if (mWindow && (mWindowType == eWindowType_popup))
      [mWindow setLevel:NSPopUpMenuWindowLevel];
  } else {
    if (mWindow && (mWindowType == eWindowType_popup))
      [mWindow setLevel:NSModalPanelWindowLevel];
  }
  
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsCocoaWindow::GetAttention(PRInt32 aCycleCount)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  [NSApp requestUserAttention:NSInformationalRequest];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

PRBool
nsCocoaWindow::HasPendingInputEvent()
{
  return nsChildView::DoHasPendingInputEvent();
}

NS_IMETHODIMP nsCocoaWindow::SetWindowShadowStyle(PRInt32 aStyle)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  if ([mWindow hasShadow] != (aStyle != NS_STYLE_WINDOW_SHADOW_NONE))
    [mWindow setHasShadow:(aStyle != NS_STYLE_WINDOW_SHADOW_NONE)];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

void nsCocoaWindow::SetShowsToolbarButton(PRBool aShow)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mWindow setShowsToolbarButton:aShow];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

NS_IMETHODIMP nsCocoaWindow::SetWindowTitlebarColor(nscolor aColor, PRBool aActive)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  // If our cocoa window isn't a ToolbarWindow, something is wrong.
  if (![mWindow isKindOfClass:[ToolbarWindow class]]) {
    // Don't output a warning for the hidden window.
    NS_WARN_IF_FALSE(SameCOMIdentity(nsCocoaUtils::GetHiddenWindowWidget(), (nsIWidget*)this),
                     "Calling SetWindowTitlebarColor on window that isn't of the ToolbarWindow class.");
    return NS_ERROR_FAILURE;
  }

  // If they pass a color with a complete transparent alpha component, use the
  // native titlebar appearance.
  if (NS_GET_A(aColor) == 0) {
    [(ToolbarWindow*)mWindow setTitlebarColor:nil forActiveWindow:(BOOL)aActive]; 
  } else {
    // Transform from sRGBA to monitor RGBA. This seems like it would make trying
    // to match the system appearance lame, so probably we just shouldn't color 
    // correct chrome.
    if (gfxPlatform::GetCMSMode() == eCMSMode_All) {
      qcms_transform *transform = gfxPlatform::GetCMSRGBATransform();
      if (transform) {
        PRUint8 color[3];
        color[0] = NS_GET_R(aColor);
        color[1] = NS_GET_G(aColor);
        color[2] = NS_GET_B(aColor);
        qcms_transform_data(transform, color, color, 1);
        aColor = NS_RGB(color[0], color[1], color[2]);
      }
    }

    [(ToolbarWindow*)mWindow setTitlebarColor:[NSColor colorWithDeviceRed:NS_GET_R(aColor)/255.0
                                                                    green:NS_GET_G(aColor)/255.0
                                                                     blue:NS_GET_B(aColor)/255.0
                                                                    alpha:NS_GET_A(aColor)/255.0]
                              forActiveWindow:(BOOL)aActive];
  }
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

gfxASurface* nsCocoaWindow::GetThebesSurface()
{
  if (mPopupContentView)
    return mPopupContentView->GetThebesSurface();
  return nsnull;
}

NS_IMETHODIMP nsCocoaWindow::BeginSecureKeyboardInput()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsresult rv = nsBaseWidget::BeginSecureKeyboardInput();
  if (NS_SUCCEEDED(rv))
    ::EnableSecureEventInput();
  return rv;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP nsCocoaWindow::EndSecureKeyboardInput()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsresult rv = nsBaseWidget::EndSecureKeyboardInput();
  if (NS_SUCCEEDED(rv))
    ::DisableSecureEventInput();
  return rv;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

// Callback used by the default titlebar and toolbar shading.
// *aIn == 0 at the top of the titlebar/toolbar, *aIn == 1 at the bottom
/* static */ void
nsCocoaWindow::UnifiedShading(void* aInfo, const CGFloat* aIn, CGFloat* aOut)
{
  UnifiedGradientInfo* info = (UnifiedGradientInfo*)aInfo;
  // The gradient percentage at the bottom of the titlebar / top of the toolbar
  float start = info->titlebarHeight / (info->titlebarHeight + info->toolbarHeight - 1);
  const float startGrey = NativeGreyColorAsFloat(headerStartGrey, info->windowIsMain);
  const float endGrey = NativeGreyColorAsFloat(headerEndGrey, info->windowIsMain);
  // *aIn is the gradient percentage of the titlebar or toolbar gradient,
  // a is the gradient percentage of the whole unified gradient.
  float a = info->drawTitlebar ? *aIn * start : start + *aIn * (1 - start);
  float result = (1.0f - a) * startGrey + a * endGrey;
  aOut[0] = result;
  aOut[1] = result;
  aOut[2] = result;
  aOut[3] = 1.0f;
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
  mToplevelActiveState = PR_FALSE;
  mHasEverBeenZoomed = PR_FALSE;
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
  if (!mGeckoWindow || mGeckoWindow->IsResizing())
    return;

  // Resizing might have changed our zoom state.
  mGeckoWindow->DispatchSizeModeEvent();
  mGeckoWindow->ReportSizeEvent();
}

- (void)windowDidBecomeMain:(NSNotification *)aNotification
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  RollUpPopups();

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

  NSWindow* window = [aNotification object];
  if ([window isSheet])
    [WindowDelegate paintMenubarForWindow:window];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)windowDidResignKey:(NSNotification *)aNotification
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

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
  // Dispatch the move event to Gecko
  nsGUIEvent guiEvent(PR_TRUE, NS_MOVE, mGeckoWindow);
  nsIntRect rect;
  mGeckoWindow->GetScreenBounds(rect);
  guiEvent.refPoint.x = rect.x;
  guiEvent.refPoint.y = rect.y;
  guiEvent.time = PR_IntervalNow();
  nsEventStatus status = nsEventStatus_eIgnore;
  mGeckoWindow->DispatchEvent(&guiEvent, status);
}

- (BOOL)windowShouldClose:(id)sender
{
  // We only want to send NS_XUL_CLOSE and let gecko close the window
  nsGUIEvent guiEvent(PR_TRUE, NS_XUL_CLOSE, mGeckoWindow);
  guiEvent.time = PR_IntervalNow();
  nsEventStatus status = nsEventStatus_eIgnore;
  mGeckoWindow->DispatchEvent(&guiEvent, status);
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

- (void)sendFocusEvent:(PRUint32)eventType
{
  if (!mGeckoWindow)
    return;

  nsEventStatus status = nsEventStatus_eIgnore;
  nsGUIEvent focusGuiEvent(PR_TRUE, eventType, mGeckoWindow);
  focusGuiEvent.time = PR_IntervalNow();
  mGeckoWindow->DispatchEvent(&focusGuiEvent, status);
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

- (nsCocoaWindow*)geckoWidget
{
  return mGeckoWindow;
}

- (PRBool)toplevelActiveState
{
  return mToplevelActiveState;
}

- (void)sendToplevelActivateEvents
{
  if (!mToplevelActiveState) {
    [self sendFocusEvent:NS_ACTIVATE];
    mToplevelActiveState = PR_TRUE;
  }
}

- (void)sendToplevelDeactivateEvents
{
  if (mToplevelActiveState) {
    [self sendFocusEvent:NS_DEACTIVATE];
    mToplevelActiveState = PR_FALSE;
  }
}

@end

// Category on NSWindow so callers can use the same method on both ToolbarWindows
// and NSWindows for accessing the background color.
@implementation NSWindow(ToolbarWindowCompat)

- (NSColor*)windowBackgroundColor
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [self backgroundColor];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

@end

@interface ToolbarWindow(Private)

- (void)redrawTitlebar;

@end

// This class allows us to have a "unified toolbar" style window. It works like this:
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
// This class also provides us with a pill button to show/hide the toolbar.
//
// Drawing the unified gradient in the titlebar and the toolbar works like this:
// 1) In the style sheet we set the toolbar's -moz-appearance to -moz-mac-unified-toolbar.
// 2) When the toolbar is drawn, Gecko calls nsNativeThemeCocoa::DrawWidgetBackground
//    for the widget type NS_THEME_MOZ_MAC_UNIFIED_TOOLBAR.
// 3) This calls DrawUnifiedToolbar which finds the toolbar frame's ToolbarWindow
//    and passes the toolbar frame's height to setUnifiedToolbarHeight.
// 4) If the toolbar height has changed, a titlebar redraw is triggered by
//    [self display] and the upper part of the unified gradient is drawn in the
//    titlebar.
// 5) DrawUnifiedToolbar draws the lower part of the unified gradient in the toolbar.
//
// Whenever the unified gradient is drawn in the titlebar or the toolbar, both
// titlebar height and toolbar height must be known in order to construct the
// correct gradient (which is a linear gradient with the length
// titlebarHeight + toolbarHeight - 1). But you can only get from the toolbar frame
// to the containing window - the other direction doesn't work. That's why the
// toolbar height is cached in the ToolbarWindow but nsNativeThemeCocoa can simply
// query the window for its titlebar height when drawing the toolbar.
@implementation ToolbarWindow

- (id)initWithContentRect:(NSRect)aContentRect styleMask:(NSUInteger)aStyle backing:(NSBackingStoreType)aBufferingType defer:(BOOL)aFlag
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  aStyle = aStyle | NSTexturedBackgroundWindowMask;
  if ((self = [super initWithContentRect:aContentRect styleMask:aStyle backing:aBufferingType defer:aFlag])) {
    mColor = [[TitlebarAndBackgroundColor alloc] initWithActiveTitlebarColor:nil
                                                       inactiveTitlebarColor:nil
                                                             backgroundColor:[NSColor whiteColor]
                                                                   forWindow:self];
    // Call the superclass's implementation, to avoid our guard method below.
    [super setBackgroundColor:mColor];

    mUnifiedToolbarHeight = 0.0f;

    // setBottomCornerRounded: is a private API call, so we check to make sure
    // we respond to it just in case.
    if ([self respondsToSelector:@selector(setBottomCornerRounded:)])
      [self setBottomCornerRounded:NO];
  }
  return self;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)dealloc
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mColor release];
  [super dealloc];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// We don't provide our own implementation of -backgroundColor because NSWindow
// looks at it, apparently. This is here to keep someone from messing with our
// custom NSColor subclass.
- (void)setBackgroundColor:(NSColor*)aColor
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mColor setBackgroundColor:aColor];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// If you need to get at the background color of the window (in the traditional
// sense) use this method instead.
- (NSColor*)windowBackgroundColor
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  return [mColor backgroundColor];

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

// Pass nil here to get the default appearance.
- (void)setTitlebarColor:(NSColor*)aColor forActiveWindow:(BOOL)aActive
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mColor setTitlebarColor:aColor forActiveWindow:aActive];
  [self redrawTitlebar];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// This is called by nsNativeThemeCocoa.mm's DrawUnifiedToolbar.
// We need to know the toolbar's height in order to draw the correct
// unified gradient in the titlebar.
- (void)setUnifiedToolbarHeight:(float)aToolbarHeight
{
  if (mUnifiedToolbarHeight == aToolbarHeight)
    return;
  mUnifiedToolbarHeight = aToolbarHeight;
  [self redrawTitlebar];
}

- (float)unifiedToolbarHeight
{
  return mUnifiedToolbarHeight;
}

- (float)titlebarHeight
{
  NSRect frameRect = [self frame];
  return frameRect.size.height - [self contentRectForFrameRect:frameRect].size.height;
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

  nsCocoaWindow *geckoWindow = [[self delegate] geckoWidget];
  if (!geckoWindow)
    return;
  nsEventStatus status = nsEventStatus_eIgnore;
  nsGUIEvent guiEvent(PR_TRUE, NS_OS_TOOLBAR, geckoWindow);
  guiEvent.time = PR_IntervalNow();
  geckoWindow->DispatchEvent(&guiEvent, status);

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

@implementation ToolbarWindow(Private)

- (void)redrawTitlebar
{
  NSView* borderView = [[self contentView] superview];
  if (!borderView)
    return;

  NSRect rect = NSMakeRect(0, [[self contentView] bounds].size.height,
                           [borderView bounds].size.width, [self titlebarHeight]);
  // setNeedsDisplayInRect doesn't have any effect here, but displayRect does.
  [borderView displayRect:rect];
}

@end

// Custom NSColor subclass where most of the work takes place for drawing in
// the titlebar area.
@implementation TitlebarAndBackgroundColor

- (id)initWithActiveTitlebarColor:(NSColor*)aActiveTitlebarColor
            inactiveTitlebarColor:(NSColor*)aInactiveTitlebarColor
                  backgroundColor:(NSColor*)aBackgroundColor
                        forWindow:(NSWindow*)aWindow
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NIL;

  if ((self = [super init])) {
    mActiveTitlebarColor = [aActiveTitlebarColor retain];
    mInactiveTitlebarColor = [aInactiveTitlebarColor retain];
    mBackgroundColor = [aBackgroundColor retain];
    mWindow = aWindow; // weak ref to avoid a cycle
  }
  return self;

  NS_OBJC_END_TRY_ABORT_BLOCK_NIL;
}

- (void)dealloc
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mActiveTitlebarColor release];
  [mInactiveTitlebarColor release];
  [mBackgroundColor release];
  [super dealloc];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Our pattern width is 1 pixel. CoreGraphics can cache and tile for us.
static const float sPatternWidth = 1.0f;

// Callback where all of the drawing for this color takes place.
void patternDraw(void* aInfo, CGContextRef aContext)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  TitlebarAndBackgroundColor *color = (TitlebarAndBackgroundColor*)aInfo;
  NSColor *backgroundColor = [color backgroundColor];
  ToolbarWindow *window = (ToolbarWindow*)[color window];
  BOOL isMain = [window isMainWindow];
  NSColor *titlebarColor = isMain ? [color activeTitlebarColor] : [color inactiveTitlebarColor];

  // Remember: this context is NOT flipped, so the origin is in the bottom left.
  float titlebarHeight = [window titlebarHeight];
  float titlebarOrigin = [window frame].size.height - titlebarHeight;

  UnifiedGradientInfo info = { titlebarHeight, [window unifiedToolbarHeight], isMain, YES };

  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithGraphicsPort:aContext flipped:NO]];

  // If the titlebar color is nil, draw the default titlebar shading.
  if (!titlebarColor) {
    // Create and draw a CGShading that uses nsCocoaWindow::UnifiedShading() as its callback.
    CGFunctionCallbacks callbacks = {0, nsCocoaWindow::UnifiedShading, NULL};
    CGFunctionRef function = CGFunctionCreate(&info, 1, NULL, 4, NULL, &callbacks);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGShadingRef shading = CGShadingCreateAxial(colorSpace,
                                                CGPointMake(0.0f, titlebarOrigin + titlebarHeight),
                                                CGPointMake(0.0f, titlebarOrigin),
                                                function, NO, NO);
    CGColorSpaceRelease(colorSpace);
    CGFunctionRelease(function);
    CGContextDrawShading(aContext, shading);
    CGShadingRelease(shading);

    // Draw the one pixel border at the bottom of the titlebar.
    if ([window unifiedToolbarHeight] == 0) {
      CGRect borderRect = CGRectMake(0.0f, titlebarOrigin, sPatternWidth, 1.0f);
      DrawNativeGreyColorInRect(aContext, headerBorderGrey, borderRect, isMain);
    }
  } else {
    // if the titlebar color is not nil, just set and draw it normally.
    [titlebarColor set];
    NSRectFill(NSMakeRect(0.0f, titlebarOrigin, sPatternWidth, titlebarHeight));
  }

  // Draw the background color of the window everywhere but where the titlebar is.
  [backgroundColor set];
  NSRectFill(NSMakeRect(0.0f, 0.0f, 1.0f, titlebarOrigin));

  [NSGraphicsContext restoreGraphicsState];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (void)setFill
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  CGContextRef context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];

  // Set up the pattern to be as tall as our window, and one pixel wide.
  // CoreGraphics can cache and tile us quickly.
  CGPatternCallbacks callbacks = {0, &patternDraw, NULL};
  CGPatternRef pattern = CGPatternCreate(self, CGRectMake(0.0f, 0.0f, sPatternWidth, [mWindow frame].size.height), 
                                         CGAffineTransformIdentity, 1, [mWindow frame].size.height,
                                         kCGPatternTilingConstantSpacing, true, &callbacks);

  // Set the pattern as the fill, which is what we were asked to do. All our
  // drawing will take place in the patternDraw callback.
  CGColorSpaceRef patternSpace = CGColorSpaceCreatePattern(NULL);
  CGContextSetFillColorSpace(context, patternSpace);
  CGColorSpaceRelease(patternSpace);
  CGFloat component = 1.0f;
  CGContextSetFillPattern(context, pattern, &component);
  CGPatternRelease(pattern);

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

// Pass nil here to get the default appearance.
- (void)setTitlebarColor:(NSColor*)aColor forActiveWindow:(BOOL)aActive
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  if (aActive) {
    [mActiveTitlebarColor autorelease];
    mActiveTitlebarColor = [aColor retain];
  } else {
    [mInactiveTitlebarColor autorelease];
    mInactiveTitlebarColor = [aColor retain];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (NSColor*)activeTitlebarColor
{
  return mActiveTitlebarColor;
}

- (NSColor*)inactiveTitlebarColor
{
  return mInactiveTitlebarColor;
}

- (void)setBackgroundColor:(NSColor*)aColor
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [mBackgroundColor autorelease];
  mBackgroundColor = [aColor retain];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

- (NSColor*)backgroundColor
{
  return mBackgroundColor;
}

- (NSWindow*)window
{
  return mWindow;
}

- (NSString*)colorSpaceName
{
  return NSDeviceRGBColorSpace;
}

- (void)set
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  [self setFill];

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

@end

@implementation PopupWindow

// The OS treats our custom popup windows very strangely -- many mouse events
// sent to them never reach their target NSView objects.  (That these windows
// are borderless and of level NSPopUpMenuWindowLevel may have something to do
// with it.)  The best solution is to pre-empt the OS, as follows.  (All
// events for a given NSWindow object go through its sendEvent: method.)
- (void)sendEvent:(NSEvent *)anEvent
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  NSView *target = nil;
  NSView *contentView = nil;
  NSEventType type = [anEvent type];
  NSPoint windowLocation = NSZeroPoint;
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
      if ((contentView = [self contentView])) {
        // Since [anEvent window] might not be us, we can't use [anEvent locationInWindow].
        windowLocation = nsCocoaUtils::EventLocationForWindow(anEvent, self);
        target = [contentView hitTest:[contentView convertPoint:windowLocation fromView:nil]];
        // If the hit test failed, the event is targeted here but is not over the window.
        // Target it at the first responder.
        if (!target)
          target = (NSView*)[self firstResponder];
      }
      break;
    default:
      break;
  }
  if (target) {
    switch (type) {
      case NSScrollWheel:
        [target scrollWheel:anEvent];
        break;
      case NSLeftMouseDown:
        if ([NSApp isActive]) {
          [target mouseDown:anEvent];
        } else if (mIsContextMenu) {
          [target mouseDown:anEvent];
          // If we're in a context menu and our NSApp isn't active (i.e. if
          // we're in a context menu raised by a right mouse-down event), we
          // don't want the OS to send the coming NSLeftMouseUp event to NSApp
          // via the window server, but we do want our ChildView to receive an
          // NSLeftMouseUp event (and to send a Gecko NS_MOUSE_BUTTON_UP event
          // to the corresponding nsChildView object).  If our NSApp isn't
          // active when it receives the coming NSLeftMouseUp via the window
          // server, our app will (in effect) become partially activated,
          // which has strange side effects:  For example, if another app's
          // window had the focus, that window will lose the focus and the
          // other app's main menu will be completely disabled (though it will
          // continue to be displayed).
          // A side effect of not allowing the coming NSLeftMouseUp event to be
          // sent to NSApp via the window server is that our custom context
          // menus will roll up whenever the user left-clicks on them, whether
          // or not the left-click hit an active menu item.  This is how native
          // context menus behave, but wasn't how our custom context menus
          // behaved previously (on the trunk or e.g. in Firefox 2.0.0.4).
          // If our ChildView's corresponding nsChildView object doesn't
          // dispatch an NS_MOUSE_BUTTON_UP event, none of our active menu items
          // will "work" on an NSLeftMouseUp.
          NSEvent *newEvent = [NSEvent mouseEventWithType:NSLeftMouseUp
                                                 location:windowLocation
                                            modifierFlags:nsCocoaUtils::GetCocoaEventModifierFlags(anEvent)
                                                timestamp:GetCurrentEventTime()
                                             windowNumber:[self windowNumber]
                                                  context:nil
                                              eventNumber:0
                                               clickCount:1
                                                 pressure:0.0];
          [target mouseUp:newEvent];
          RollUpPopups();
        } else {
          // If our NSApp isn't active and we're not a context menu (i.e. if
          // we're an ordinary popup window), activate us before sending the
          // event to its target.  This prevents us from being used in the
          // background, and resolves bmo bug 434097 (another app focus
          // wierdness bug).
          [NSApp activateIgnoringOtherApps:YES];
          [target mouseDown:anEvent];
        }
        break;
      case NSLeftMouseUp:
        [target mouseUp:anEvent];
        break;
      case NSRightMouseDown:
        [target rightMouseDown:anEvent];
        break;
      case NSRightMouseUp:
        [target rightMouseUp:anEvent];
        break;
      case NSOtherMouseDown:
        [target otherMouseDown:anEvent];
        break;
      case NSOtherMouseUp:
        [target otherMouseUp:anEvent];
        break;
      case NSMouseMoved:
        [target mouseMoved:anEvent];
        break;
      case NSLeftMouseDragged:
        [target mouseDragged:anEvent];
        break;
      case NSRightMouseDragged:
        [target rightMouseDragged:anEvent];
        break;
      case NSOtherMouseDragged:
        [target otherMouseDragged:anEvent];
        break;
      default:
        [super sendEvent:anEvent];
        break;
    }
  } else {
    [super sendEvent:anEvent];
  }

  NS_OBJC_END_TRY_ABORT_BLOCK;
}

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
