/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#include "nsCOMPtr.h"
#include "nsIServiceManager.h"    // for drag and drop
#include "nsWidgetsCID.h"
#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsIDragSessionMac.h"
#include "nsIScreen.h"
#include "nsIScreenManager.h"
#include "nsGUIEvent.h"
#include "nsMacResources.h"
#include "nsIRollupListener.h"
#include "nsChildView.h"
#include "nsIAppShell.h"
#include "nsIAppShellService.h"
#include "nsIBaseWindow.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIXULWindow.h"

// Define Class IDs -- i hate having to do this
static NS_DEFINE_CID(kCDragServiceCID,  NS_DRAGSERVICE_CID);

// externs defined in nsChildView.mm
extern nsIRollupListener * gRollupListener;
extern nsIWidget         * gRollupWidget;
extern BOOL                gSomeMenuBarPainted;

#define NS_APPSHELLSERVICE_CONTRACTID "@mozilla.org/appshell/appShellService;1"

// call getHiddenWindowNativeMenu, don't use this directly
static nsIMenuBar* gHiddenWindowMenuBar;

NS_IMPL_ISUPPORTS_INHERITED1(nsCocoaWindow, Inherited, nsPIWidgetCocoa)


// get the highest point on any screen
static float HighestPointOnAnyScreen()
{
  float highestScreenPoint = 0.0;
  NSArray* allScreens = [NSScreen screens];
  for (unsigned int i = 0; i < [allScreens count]; i++) {
    NSRect currScreenFrame = [[allScreens objectAtIndex:i] frame];
    float currScreenHighestPoint = currScreenFrame.origin.y + currScreenFrame.size.height;
    if (currScreenHighestPoint > highestScreenPoint)
      highestScreenPoint = currScreenHighestPoint;
  }
  return highestScreenPoint;
}


/*
 * Gecko rects (nsRect) contain an origin (x,y) in a coordinate
 * system with (0,0) in the top-left of the screen. Cocoa rects
 * (NSRect) contain an origin (x,y) in a coordinate system with
 * (0,0) in the bottom-left of the screen. Both nsRect and NSRect
 * contain width/height info, with no difference in their use.
 */
static NSRect geckoRectToCocoaRect(const nsRect &geckoRect)
{
  // We only need to change the Y coordinate by starting with the screen
  // height, subtracting the gecko Y coordinate, and subtracting the
  // height.
  return NSMakeRect(geckoRect.x,
                    HighestPointOnAnyScreen() - geckoRect.y - geckoRect.height,
                    geckoRect.width,
                    geckoRect.height);
}


//
// See explanation for geckoRectToCocoaRect, guess what this does...
//
static nsRect cocoaRectToGeckoRect(const NSRect &cocoaRect)
{
  // We only need to change the Y coordinate by starting with the screen
  // height, subtracting the gecko Y coordinate, and subtracting the
  // height.
  return nsRect((nscoord)cocoaRect.origin.x,
                (nscoord)(HighestPointOnAnyScreen() - (cocoaRect.origin.y + cocoaRect.size.height)),
                (nscoord)cocoaRect.size.width,
                (nscoord)cocoaRect.size.height);
}


//
// nsCocoaWindow constructor
//
nsCocoaWindow::nsCocoaWindow()
: mParent(nsnull)
, mWindow(nil)
, mDelegate(nil)
, mSheetWindowParent(nil)
, mIsResizing(PR_FALSE)
, mWindowMadeHere(PR_FALSE)
, mVisible(PR_FALSE)
, mSheetNeedsShow(PR_FALSE)
{

}


//
// nsCocoaWindow destructor
//
nsCocoaWindow::~nsCocoaWindow()
{
  if (mWindow && mWindowMadeHere) {
    [mWindow autorelease];
    [mDelegate autorelease];
  }
}


static nsIMenuBar* GetHiddenWindowMenuBar()
{
  if (gHiddenWindowMenuBar)
    return gHiddenWindowMenuBar;
  
  nsCOMPtr<nsIAppShellService> appShell(do_GetService(NS_APPSHELLSERVICE_CONTRACTID));
  if (!appShell) {
    NS_WARNING("Couldn't get AppShellService in order to get hidden window ref");
    return NULL;
  }
  
  nsCOMPtr<nsIXULWindow> hiddenWindow;
  appShell->GetHiddenWindow(getter_AddRefs(hiddenWindow));
  if (!hiddenWindow) {
    NS_WARNING("Couldn't get hidden window from appshell");
    return NULL;
  }
  
  nsCOMPtr<nsIBaseWindow> baseHiddenWindow;
  baseHiddenWindow = do_GetInterface(hiddenWindow);
  if (!baseHiddenWindow) {
    NS_WARNING("Couldn't get nsIBaseWindow from hidden window (nsIXULWindow)");
    return NULL;
  }
  
  nsCOMPtr<nsIWidget> hiddenWindowWidget;
  if (NS_FAILED(baseHiddenWindow->GetMainWidget(getter_AddRefs(hiddenWindowWidget)))) {
    NS_WARNING("Couldn't get nsIWidget from hidden window (nsIBaseWindow)");
    return NULL;
  }
  
  nsIWidget* hiddenWindowWidgetNoCOMPtr = hiddenWindowWidget;
  nsIMenuBar* geckoMenuBar = NS_STATIC_CAST(nsCocoaWindow*, hiddenWindowWidgetNoCOMPtr)->GetMenuBar();  
  
  if (geckoMenuBar) {
    gHiddenWindowMenuBar = geckoMenuBar;
  }
  
  return gHiddenWindowMenuBar;
}


//
// Utility method for implementing both Create(nsIWidget ...) and
// Create(nsNativeWidget...)
//
nsresult nsCocoaWindow::StandardCreate(nsIWidget *aParent,
                        const nsRect &aRect,
                        EVENT_CALLBACK aHandleEventFunction,
                        nsIDeviceContext *aContext,
                        nsIAppShell *aAppShell,
                        nsIToolkit *aToolkit,
                        nsWidgetInitData *aInitData,
                        nsNativeWidget aNativeWindow)
{
  Inherited::BaseCreate(aParent, aRect, aHandleEventFunction, aContext, aAppShell,
                        aToolkit, aInitData);
  
  mParent = aParent;
  
  // create a window if we aren't given one, always create if this should be a popup
  if (!aNativeWindow || (aInitData && aInitData->mWindowType == eWindowType_popup)) {
    // decide on a window type
    PRBool allOrDefault = PR_FALSE;
    if (aInitData) {
      allOrDefault = aInitData->mBorderStyle == eBorderStyle_all ||
                     aInitData->mBorderStyle == eBorderStyle_default;
      mWindowType = aInitData->mWindowType;
      // if a toplevel window was requested without a titlebar, use a dialog
      if (mWindowType == eWindowType_toplevel &&
          (aInitData->mBorderStyle == eBorderStyle_none ||
           !allOrDefault &&
           !(aInitData->mBorderStyle & eBorderStyle_title)))
        mWindowType = eWindowType_dialog;
    }
    else {
      allOrDefault = PR_TRUE;
      mWindowType = eWindowType_toplevel;
    }
    
#ifdef MOZ_MACBROWSER
    if (mWindowType == eWindowType_popup)
      return NS_OK;
#endif
    
    // we default to NSBorderlessWindowMask, add features if needed
    unsigned int features = NSBorderlessWindowMask;
    
    // Configure the window we will create based on the window type
    switch (mWindowType)
    {
      case eWindowType_child:
        // In Carbon, we made this a window of type kPlainWindowClass.
        // I think that is pretty much equiv to NSBorderlessWindowMask.
        break;
      case eWindowType_dialog:
        if (aInitData) {
          // we never give dialogs a close button
          switch (aInitData->mBorderStyle)
          {
            case eBorderStyle_none:
              break;
            case eBorderStyle_default:
              features |= NSTitledWindowMask;
              break;
            case eBorderStyle_all:
              features |= NSTitledWindowMask;
              features |= NSResizableWindowMask;
              features |= NSMiniaturizableWindowMask;
              break;
            default:
              if (aInitData->mBorderStyle & eBorderStyle_title) {
                features |= NSTitledWindowMask;
                features |= NSMiniaturizableWindowMask;
              }
              if (aInitData->mBorderStyle & eBorderStyle_resizeh) {
                features |= NSResizableWindowMask;
              }
              break;
          }
        }
        else {
          features |= NSTitledWindowMask;
          features |= NSMiniaturizableWindowMask;
        }
        break;
      case eWindowType_sheet:
        if (aInitData) {
          nsWindowType parentType;
          aParent->GetWindowType(parentType);
          if (parentType != eWindowType_invisible &&
              aInitData->mBorderStyle & eBorderStyle_resizeh) {
            features = NSResizableWindowMask;
          }
          else {
            features = NSMiniaturizableWindowMask;
          }
        }
        else {
          features = NSMiniaturizableWindowMask;
        }
        features |= NSTitledWindowMask;
        break;
      case eWindowType_popup:
        features |= NSBorderlessWindowMask;
        break;
      case eWindowType_toplevel:
        features |= NSTitledWindowMask;
        features |= NSMiniaturizableWindowMask;
        if (allOrDefault || aInitData->mBorderStyle & eBorderStyle_close)
          features |= NSClosableWindowMask;
        if (allOrDefault || aInitData->mBorderStyle & eBorderStyle_resizeh)
          features |= NSResizableWindowMask;
        break;
      case eWindowType_invisible:
        break;
      default:
        NS_ERROR("Unhandled window type!");
        return NS_ERROR_FAILURE;
    }
    
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
    NSRect rect = geckoRectToCocoaRect(aRect);
    
    // compensate for difference between frame and content area height (e.g. title bar)
    NSRect newWindowFrame = [NSWindow frameRectForContentRect:rect styleMask:features];

    rect.origin.y -= (newWindowFrame.size.height - rect.size.height);
    
    if (mWindowType != eWindowType_popup)
      rect.origin.y -= ::GetMBarHeight();
    
#ifdef DEBUG
    NSLog(@"Top-level window being created at Cocoa rect: %f, %f, %f, %f\n",
          rect.origin.x, rect.origin.y, rect.size.width, rect.size.height);
#endif
    
    // create the window
    mWindow = [[NSWindow alloc] initWithContentRect:rect styleMask:features 
                                backing:NSBackingStoreBuffered defer:NO];
    
    if (mWindowType == eWindowType_popup) {
      [mWindow setLevel:NSPopUpMenuWindowLevel];
      [mWindow setHasShadow:YES];
    }
    else if (mWindowType == eWindowType_invisible) {
      [mWindow setLevel:kCGDesktopWindowLevelKey];
    }

    [mWindow setContentMinSize:NSMakeSize(60, 60)];

    [mWindow setReleasedWhenClosed:NO];

    // register for mouse-moved events. The default is to ignore them for perf reasons.
    [mWindow setAcceptsMouseMovedEvents:YES];
    
    // setup our notification delegate. Note that setDelegate: does NOT retain.
    mDelegate = [[WindowDelegate alloc] initWithGeckoWindow:this];
    [mWindow setDelegate:mDelegate];
    
    mWindowMadeHere = PR_TRUE;
  }
  else {
    mWindow = (NSWindow*)aNativeWindow;
    mVisible = PR_TRUE;
  }
  
  return NS_OK;
}


//
// Create a nsCocoaWindow using a native window provided by the application
//
NS_IMETHODIMP nsCocoaWindow::Create(nsNativeWidget aNativeWindow,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  return(StandardCreate(nsnull, aRect, aHandleEventFunction,
                          aContext, aAppShell, aToolkit, aInitData,
                            aNativeWindow));
}


NS_IMETHODIMP nsCocoaWindow::Create(nsIWidget* aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  return(StandardCreate(aParent, aRect, aHandleEventFunction,
                        aContext, aAppShell, aToolkit, aInitData, nsnull));
}


void* nsCocoaWindow::GetNativeData(PRUint32 aDataType)
{
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
      
    case NS_NATIVE_GRAPHIC: // quickdraw port of top view (for now)
      retVal = [[mWindow contentView] qdPort];
      break;
  }

  return retVal;
}


NS_IMETHODIMP nsCocoaWindow::IsVisible(PRBool & aState)
{
  aState = mVisible;
  return NS_OK;
}


//
// Hide or show this window
//
NS_IMETHODIMP nsCocoaWindow::Show(PRBool bState)
{
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
      }

      nsCocoaWindow* sheetShown = nsnull;
      if (NS_SUCCEEDED(piParentWidget->GetChildSheet(PR_TRUE, &sheetShown)) &&
          (!sheetShown || sheetShown == this)) {
        // If this sheet is already the sheet actually being shown, don't
        // tell it to show again. Otherwise the number of calls to
        // [NSApp beginSheet...] won't match up with [NSApp endSheet...].
        if (![mWindow isSheet]) {
          mVisible = PR_TRUE;
          mSheetNeedsShow = PR_FALSE;
          mSheetWindowParent = topNonSheetWindow;
          [[mSheetWindowParent delegate] sendLostFocusAndDeactivate];
          [NSApp beginSheet:mWindow
             modalForWindow:mSheetWindowParent
              modalDelegate:mDelegate
             didEndSelector:@selector(didEndSheet:returnCode:contextInfo:)
                contextInfo:mSheetWindowParent];
          [[mWindow delegate] sendGotFocusAndActivate];
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
      mVisible = PR_TRUE;
      [mWindow orderFront:nil];
    }
    else {
      mVisible = PR_TRUE;
      [mWindow makeKeyAndOrderFront:nil];
    }
  }
  else {
    // roll up any popups if a top-level window is going away
    if (mWindowType == eWindowType_toplevel) {
      if (gRollupListener != nsnull && gRollupWidget != nsnull)
        gRollupListener->Rollup();
      NS_IF_RELEASE(gRollupListener);
      NS_IF_RELEASE(gRollupWidget);
    }

    // now get rid of the window/sheet
    if (mWindowType == eWindowType_sheet) {
      if (mVisible) {
        mVisible = PR_FALSE;

        // get sheet's parent *before* hiding the sheet (which breaks the linkage)
        NSWindow* sheetParent = mSheetWindowParent;
        
        // hide the sheet
        [NSApp endSheet:mWindow];
        
        [[mWindow delegate] sendLostFocusAndDeactivate];

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
          // If there are no sibling sheets, but the parent is a sheet, restore
          // it.  It wasn't sent any deactivate events when it was hidden, so
          // don't call through Show, just let the OS put it back up.
          [NSApp beginSheet:nativeParentWindow
             modalForWindow:sheetParent
              modalDelegate:[nativeParentWindow delegate]
             didEndSelector:@selector(didEndSheet:returnCode:contextInfo:)
                contextInfo:sheetParent];
        }
        else {
          // Sheet, that was hard.  No more siblings or parents, going back
          // to a real window.
          [sheetParent makeKeyAndOrderFront:nil];
        }
      }
      else if (mSheetNeedsShow) {
        // This is an attempt to hide a sheet that never had a chance to
        // be shown. There's nothing to do other than make sure that it
        // won't show.
        mSheetNeedsShow = PR_FALSE;
      }
    }
    else {
      [mWindow orderOut:nil];
      mVisible = PR_FALSE;
    }
  }
  
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
  if (mWindow) {  
    // if we're a popup, we have to convert from our parent widget's coord
    // system to the global coord system first because the (x,y) we're given
    // is in its coordinate system.
    if (mWindowType == eWindowType_popup) {
      nsRect localRect, globalRect; 
      localRect.x = aX;
      localRect.y = aY;  
      if (mParent) {
        mParent->WidgetToScreen(localRect,globalRect);
        aX=globalRect.x;
        aY=globalRect.y;
      }
    }
    
    NSPoint coord = {aX, aY};
 
    // the point we have assumes that the screen origin is the top-left. Well,
    // it's not. Use the screen object to convert.
    //FIXME -- doesn't work on monitors other than primary
    NSRect screenRect = [[NSScreen mainScreen] frame];
    coord.y = (screenRect.origin.y + screenRect.size.height) - coord.y;
    //printf("final coords %f %f\n", coord.x, coord.y);
    //printf("- window coords before %f %f\n", [mWindow frame].origin.x, [mWindow frame].origin.y);
    [mWindow setFrameTopLeftPoint:coord];
    //printf("- window coords after %f %f\n", [mWindow frame].origin.x, [mWindow frame].origin.y);
  }
  
  return NS_OK;
}


//
// Position the window behind the given window
//
NS_METHOD nsCocoaWindow::PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                     nsIWidget *aWidget, PRBool aActivate)
{
  return NS_OK;
}


//
// zoom/restore
//
NS_METHOD nsCocoaWindow::SetSizeMode(PRInt32 aMode)
{
  return NS_OK;
}


NS_IMETHODIMP nsCocoaWindow::Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  Move(aX, aY);
  Resize(aWidth, aHeight, aRepaint);
  return NS_OK;
}


//
// Resize this window
//
NS_IMETHODIMP nsCocoaWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  if (mWindow) {
    NSRect newBounds = [mWindow frame];
    newBounds.size.width = aWidth;
    // Note that [mWindow isSheet] is not the same as checking for
    // |mWindowType == eWindowType_sheet|. If this is a sheet object, the latter
    // will always be true. The former is true only when the sheet is being shown.
    // Here we need to know if the sheet is actually being shown because if it is,
    // the content view and the window's frame are equal, despite the fact that
    // the native window object has the title bar flag set. If the window is not
    // being shown as a sheet the content area and window frame differ.
    if (mWindowType == eWindowType_popup || [mWindow isSheet])
      newBounds.size.height = aHeight;
    else
      newBounds.size.height = aHeight + kTitleBarHeight; // add height of title bar
    StartResizing();
    [mWindow setFrame:newBounds display:NO];
    StopResizing();
  }

  mBounds.width  = aWidth;
  mBounds.height = aHeight;
  
  // tell gecko to update all the child widgets
  ReportSizeEvent();
  
  return NS_OK;
}


//
// We return the origin for the entire window (title bar and all) but
// the size of the content area. I have no idea why it was originally done
// this way, but it matches Carbon and makes things work nicely.
//
NS_IMETHODIMP nsCocoaWindow::GetScreenBounds(nsRect &aRect)
{
  nsRect windowFrame = cocoaRectToGeckoRect([mWindow frame]);
  aRect.x = windowFrame.x;
  aRect.y = windowFrame.y;
  aRect.width = mBounds.width;
  aRect.height = mBounds.height;
  // printf("GetScreenBounds: output: %d,%d,%d,%d\n", aRect.x, aRect.y, aRect.width, aRect.height);
  return NS_OK;
}


PRBool nsCocoaWindow::OnPaint(nsPaintEvent &event)
{
  return PR_TRUE; // don't dispatch the update event
}


NS_IMETHODIMP nsCocoaWindow::SetTitle(const nsAString& aTitle)
{
  const nsString& strTitle = PromiseFlatString(aTitle);
  NSString* title = [NSString stringWithCharacters:strTitle.get() length:strTitle.Length()];
  [mWindow setTitle:title];

  return NS_OK;
}


//
// Pass notification of some drag event to Gecko
//
// The drag manager has let us know that something related to a drag has
// occurred in this window. It could be any number of things, ranging from 
// a drop, to a drag enter/leave, or a drag over event. The actual event
// is passed in |aMessage| and is passed along to our event hanlder so Gecko
// knows about it.
//
PRBool nsCocoaWindow::DragEvent ( unsigned int aMessage, Point aMouseGlobal, UInt16 aKeyModifiers )
{
  return PR_FALSE;
}


//
// Like ::BringToFront, but constrains the window to its z-level
//
NS_IMETHODIMP nsCocoaWindow::ComeToFront()
{
/*
  nsZLevelEvent event(PR_TRUE, NS_SETZLEVEL, this);
  
  event.refPoint.x = mBounds.x;
  event.refPoint.y = mBounds.y;
  event.time = PR_IntervalNow();
  
  event.mImmediate = PR_TRUE;
  
  nsEventStatus status = nsEventStatus_eIgnore;
  DispatchEvent(&event, status);
*/
  
  return NS_OK;
}


NS_IMETHODIMP nsCocoaWindow::GetChildSheet(PRBool aShown, nsCocoaWindow** _retval)
{
  nsIWidget* child = GetFirstChild();

  while (child) {
    // find out if this is a top-level window
    nsCOMPtr<nsPIWidgetCocoa> piChildWidget(do_QueryInterface(child));
    if (piChildWidget) {
      // if it implements nsPIWidgetCocoa, it must be an nsCocoaWindow
      nsCocoaWindow* window = NS_STATIC_CAST(nsCocoaWindow*, child);
      nsWindowType type;
      if (NS_SUCCEEDED(window->GetWindowType(type)) &&
          type == eWindowType_sheet) {
        // if it's a sheet, it must be an nsCocoaWindow
        nsCocoaWindow* cocoaWindow = NS_STATIC_CAST(nsCocoaWindow*, window);
        if ((aShown && cocoaWindow->mVisible) ||
            (!aShown && cocoaWindow->mSheetNeedsShow)) {
          *_retval = cocoaWindow;
          return NS_OK;
        }
      }
    }
    child = child->GetNextSibling();
  }

  *_retval = nsnull;

  return NS_OK;
}


NS_IMETHODIMP nsCocoaWindow::GetMenuBar(nsIMenuBar** menuBar)
{
  *menuBar = mMenuBar;
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


//
// Invokes callback and  ProcessEvent method on Event Listener object
//
NS_IMETHODIMP 
nsCocoaWindow::DispatchEvent(nsGUIEvent* event, nsEventStatus& aStatus)
{
  aStatus = nsEventStatus_eIgnore;

  nsIWidget* aWidget = event->widget;
  NS_IF_ADDREF(aWidget);
  
  if (nsnull != mMenuListener){
    if(NS_MENU_EVENT == event->eventStructType)
      aStatus = mMenuListener->MenuSelected(static_cast<nsMenuEvent&>(*event));
  }
  if (mEventCallback)
    aStatus = (*mEventCallback)(event);

  // Dispatch to event listener if event was not consumed
  if ((aStatus != nsEventStatus_eConsumeNoDefault) && (mEventListener != nsnull))
    aStatus = mEventListener->ProcessEvent(*event);

  NS_IF_RELEASE(aWidget);

  return NS_OK;
}


void
nsCocoaWindow::ReportSizeEvent()
{
  // nsEvent
  nsSizeEvent sizeEvent(PR_TRUE, NS_SIZE, this);
  sizeEvent.time = PR_IntervalNow();

  // nsSizeEvent
  sizeEvent.windowSize = &mBounds;
  sizeEvent.mWinWidth  = mBounds.width;
  sizeEvent.mWinHeight = mBounds.height;
  
  // dispatch event
  nsEventStatus status = nsEventStatus_eIgnore;
  DispatchEvent(&sizeEvent, status);
}


NS_IMETHODIMP nsCocoaWindow::SetMenuBar(nsIMenuBar *aMenuBar)
{
  if (mMenuBar)
    mMenuBar->SetParent(nsnull);
  mMenuBar = aMenuBar;
  
  // We paint the hidden window menu bar if no other menu bar has been painted
  // yet so that some reasonable menu bar is displayed when the app starts up.
  if (!gSomeMenuBarPainted && mMenuBar && (GetHiddenWindowMenuBar() == mMenuBar))
    mMenuBar->Paint();
  
  return NS_OK;
}


NS_IMETHODIMP nsCocoaWindow::ShowMenuBar(PRBool aShow)
{
  return NS_ERROR_FAILURE;
}


nsIMenuBar* nsCocoaWindow::GetMenuBar()
{
  return mMenuBar;
}


NS_IMETHODIMP nsCocoaWindow::CaptureRollupEvents(nsIRollupListener * aListener, 
                                                 PRBool aDoCapture, 
                                                 PRBool aConsumeRollupEvent)
{
  if (aDoCapture) {
    NS_IF_RELEASE(gRollupListener);
    NS_IF_RELEASE(gRollupWidget);
    gRollupListener = aListener;
    NS_ADDREF(aListener);
    gRollupWidget = this;
    NS_ADDREF(this);
  } else {
    NS_IF_RELEASE(gRollupListener);
    NS_IF_RELEASE(gRollupWidget);
  }
  
  return NS_OK;
}


@implementation WindowDelegate


- (id)initWithGeckoWindow:(nsCocoaWindow*)geckoWind
{
  [super init];
  mGeckoWindow = geckoWind;
  return self;
}


- (NSSize)windowWillResize:(NSWindow *)sender toSize:(NSSize)proposedFrameSize
{
  // roll up any popups
  if (gRollupListener != nsnull && gRollupWidget != nsnull)
    gRollupListener->Rollup();
  
  return proposedFrameSize;
}


- (void)windowDidResize:(NSNotification *)aNotification
{
  if (mGeckoWindow->IsResizing())
    return;
  
  // Gecko already compensates for the title bar, so we have to strip it out here.
  NSRect frameRect = [[[aNotification object] contentView] frame];
  mGeckoWindow->Resize(NS_STATIC_CAST(PRInt32,frameRect.size.width),
                       NS_STATIC_CAST(PRInt32,frameRect.size.height), PR_TRUE);
}


- (void)windowDidBecomeMain:(NSNotification *)aNotification
{
  if (!mGeckoWindow)
    return;

  nsIMenuBar* myMenuBar = mGeckoWindow->GetMenuBar();
  if (myMenuBar) {
    // printf("painting window menu bar due to window becoming main\n");
    myMenuBar->Paint();
  }
}


- (void)windowDidResignMain:(NSNotification *)aNotification
{
  // roll up any popups
  if (gRollupListener != nsnull && gRollupWidget != nsnull)
    gRollupListener->Rollup();
  
  nsCOMPtr<nsIMenuBar> hiddenWindowMenuBar = GetHiddenWindowMenuBar();
  if (hiddenWindowMenuBar) {
    // printf("painting hidden window menu bar due to window losing main status\n");
    hiddenWindowMenuBar->Paint();
  }
}


- (void)windowWillMove:(NSNotification *)aNotification
{
  // roll up any popups
  if (gRollupListener != nsnull && gRollupWidget != nsnull)
    gRollupListener->Rollup();
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
  // roll up any popups
  if (gRollupListener != nsnull && gRollupWidget != nsnull)
    gRollupListener->Rollup();
}


- (void)windowWillMiniaturize:(NSNotification *)aNotification
{
  // roll up any popups
  if (gRollupListener != nsnull && gRollupWidget != nsnull)
    gRollupListener->Rollup();
}


- (void)sendGotFocusAndActivate
{
  if (!mGeckoWindow)
    return;
  
  nsEventStatus status = nsEventStatus_eIgnore;
  
  nsGUIEvent focusGuiEvent(PR_TRUE, NS_GOTFOCUS, mGeckoWindow);
  focusGuiEvent.time = PR_IntervalNow();
  mGeckoWindow->DispatchEvent(&focusGuiEvent, status);
  
  nsGUIEvent activateGuiEvent(PR_TRUE, NS_ACTIVATE, mGeckoWindow);
  activateGuiEvent.time = PR_IntervalNow();
  mGeckoWindow->DispatchEvent(&activateGuiEvent, status);
}


- (void)sendLostFocusAndDeactivate
{
  if (!mGeckoWindow)
    return;
  
  nsEventStatus status = nsEventStatus_eIgnore;
  
  nsGUIEvent deactivateGuiEvent(PR_TRUE, NS_DEACTIVATE, mGeckoWindow);
  deactivateGuiEvent.time = PR_IntervalNow();
  mGeckoWindow->DispatchEvent(&deactivateGuiEvent, status);
  
  nsGUIEvent lostfocusGuiEvent(PR_TRUE, NS_LOSTFOCUS, mGeckoWindow);
  lostfocusGuiEvent.time = PR_IntervalNow();
  mGeckoWindow->DispatchEvent(&lostfocusGuiEvent, status);
}


- (void)didEndSheet:(NSWindow*)sheet returnCode:(int)returnCode contextInfo:(void*)contextInfo
{
  // Note: 'contextInfo' is the window that is the parent of the sheet,
  // we set that in nsCocoaWindow::Show. 'contextInfo' is always the top-level
  // window, not another sheet itself.
  [[sheet delegate] sendLostFocusAndDeactivate];
  [sheet orderOut:self];
  [[(NSWindow*)contextInfo delegate] sendGotFocusAndActivate];
}


@end
