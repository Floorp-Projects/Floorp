/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All 
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 03/23/2000       IBM Corp.      Added InvalidateRegion method; keyboard (OnKey) handling; and
 *                                         fixed uninitialized event.isMeta.
 * 04/12/2000       IBM Corp.      DispatchMouseEvent changes.
 * 04/14/2000       IBM Corp.      Ported CaptureRollupEvents changes from Windows.
 * 06/09/2000       IBM Corp.      Added cases for more cursors in SetCursor.
 * 06/14/2000       IBM Corp.      Removed dead menu code to fix build break.
 * 06/15/2000       IBM Corp.      Created NS2PM for rectangles.
 * 06/21/2000       IBM Corp.      Corrected menu parentage; added CaptureMouse.
 * 06/22/2000       IBM Corp.      Corrected menu ownership
 *
 */

#include "nsWindow.h"
#include "nsIAppShell.h"
#include "nsIFontMetrics.h"
#include "nsFont.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsRect.h"
#include "nsTransform2D.h"
#include "nsGfxCIID.h"
#include "prtime.h"
// #include "nsTooltipManager.h"
#include "nsISupportsArray.h"
#include "nsIMenuBar.h"
//#include "nsIMenuItem.h"
#include "nsHashtable.h"
//#include "nsMenu.h"
#include "nsDragService.h"

#include "nsIRollupListener.h"
#include "nsIMenuRollup.h"
#include "nsIRegion.h"

//~~~ windowless plugin support
#include "nsplugindefs.h"

#include "nsITimer.h"

#include <stdlib.h>
#include <ctype.h>

#ifdef DEBUG_sobotka
static int WINDOWCOUNT = 0;
#endif

// HWNDs are mapped to nsWindow objects using a custom presentation parameter,
// which is registered in nsModule -- thanks to Cindy Ross for explaining how
// to do this.
//
// The subclass proc (fnwpNSWindow) calls ProcessMessage() in the object.
// Decisions are taken here about what to do - the purpose of the OnFoo()
// methods is to generate an NS event to the various people who are
// listening, or not.
//
// OS/2 things: remember supplied coords are in the XP space.  There are
// NS2PM methods for conversion of points & rectangles; position is a bit
// different in that it's the *parent* window whose height must be used.
//
// Deferred window positioning is emulated using WinSetMultWindowPos in
// the hopes that there was a good reason for adding it to nsIWidget.
//
// SetColorSpace() is not implemented on purpose.  So there.
//
// John Fairhurst 17-09-98 first version
//        Revised 01-12-98 to inherit from nsBaseWidget.
//        Revised 24-01-99 to use hashtable
//        Revised 15-03-99 for new menu classes
//        Revised 05-06-99 to use pres-params
//        Revised 19-06-99 drag'n'drop, etc.

// XXX don't deliver click-events to obscured parents
BOOL g_bHandlingMouseClick = FALSE;

nsWindow* nsWindow::gCurrentWindow = nsnull;

////////////////////////////////////////////////////
// Rollup Listener - global variable defintions
////////////////////////////////////////////////////
nsIRollupListener * gRollupListener           = nsnull;
nsIWidget         * gRollupWidget             = nsnull;
PRBool              gRollupConsumeRollupEvent = PR_FALSE;
////////////////////////////////////////////////////

PRBool gJustGotActivate = PR_FALSE;
PRBool gJustGotDeactivate = PR_FALSE;

HWND   gHwndBeingDestroyed = NULLHANDLE;

////////////////////////////////////////////////////
// Mouse Clicks - static variable defintions 
// for figuring out 1 - 3 Clicks
////////////////////////////////////////////////////
static POINTL gLastMousePoint;
static LONG   gLastMsgTime    = 0;
static LONG   gLastClickCount = 0;
////////////////////////////////////////////////////

//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow() : nsBaseWidget()
{
    NS_INIT_REFCNT();
    mWnd                = 0;
    mPrevWndProc        = NULL;
    mParent             = 0;
    mNextID             = 1;
    mNextCmdID          = 1;
    mSWPs               = 0;
    mlHave              = 0;
    mlUsed              = 0;
    mPointer            = 0;
    mPS                 = 0;
    mPSRefs             = 0;
    mDragInside         = FALSE;
    mDeadKey            = 0;
    mHaveDeadKey        = FALSE;
    // This is so that frame windows can be destroyed from their destructors.
    mHackDestroyWnd     = 0;

    mPreferredWidth     = 0;
    mPreferredHeight    = 0;
    mWindowState        = nsWindowState_ePrecreate;
    mWindowType         = eWindowType_child;
    mBorderStyle        = eBorderStyle_default;
    mFont               = nsnull;
    mOS2Toolkit         = nsnull;
    mMenuBar            = nsnull;
//    mActiveMenu        = nsnull;
}

//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
  // How destruction works: A call of Destroy() destroys the PM window.  This
  // triggers an OnDestroy(), which frees resources.  If not Destroy'd at
  // delete time, Destroy() gets called anyway.
  
  // NOTE: Calling virtual functions from destructors is bad; they always
  //       bind in the current object (ie. as if they weren't virtual).  It
  //       may even be illegal to call them from here.
  //
  mIsDestroying = PR_TRUE;
  if (gCurrentWindow == this) {
    gCurrentWindow = nsnull;
  }

  // If the widget was released without calling Destroy() then the native
  // window still exists, and we need to destroy it
  if( !(mWindowState & nsWindowState_eDead) )
  {
    mWindowState |= nsWindowState_eDoingDelete;
    mWindowState &= ~(nsWindowState_eLive|nsWindowState_ePrecreate|
                      nsWindowState_eInCreate);
//    if( mWnd)
      Destroy();
  }
}

NS_METHOD nsWindow::CaptureMouse(PRBool aCapture)
{
  if (PR_TRUE == aCapture) { 
    WinSetCapture( HWND_DESKTOP, mWnd);
  } else {
    WinSetCapture( HWND_DESKTOP, NULLHANDLE);
  }
//  mIsInMouseCapture = aCapture;
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Deferred Window positioning
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::BeginResizingChildren(void)
{
   if( !mSWPs)
   {
      mlHave = 10;
      mlUsed = 0;
      mSWPs = (PSWP) malloc( 10 * sizeof( SWP));
   }
   return NS_OK;
}

void nsWindow::DeferPosition( HWND hwnd, HWND hwndInsertBehind,
                              long x, long y, long cx, long cy, ULONG flags)
{
   if( mSWPs)
   {
      if( mlHave == mlUsed) // need more swps
      {
         mlHave += 10;
         mSWPs = (PSWP) realloc( mSWPs, mlHave * sizeof( SWP));
      }
      mSWPs[ mlUsed].hwnd = hwnd;
      mSWPs[ mlUsed].hwndInsertBehind = hwndInsertBehind;
      mSWPs[ mlUsed].x = x;
      mSWPs[ mlUsed].y = y;
      mSWPs[ mlUsed].cx = cx;
      mSWPs[ mlUsed].cy = cy;
      mSWPs[ mlUsed].fl = flags;
      mSWPs[ mlUsed].ulReserved1 = 0;
      mSWPs[ mlUsed].ulReserved2 = 0;
      mlUsed++;
   }
}

NS_METHOD nsWindow::EndResizingChildren(void)
{
   if( nsnull != mSWPs)
   {
      WinSetMultWindowPos( 0/*hab*/, mSWPs, mlUsed);
      free( mSWPs);
      mSWPs = nsnull;
      mlUsed = mlHave = 0;
   }
   return NS_OK;
}

NS_METHOD nsWindow::WidgetToScreen(const nsRect &aOldRect, nsRect &aNewRect)
{
  POINTL point = { aOldRect.x, aOldRect.y };
  NS2PM( point);

  WinMapWindowPoints( mWnd, HWND_DESKTOP, &point, 1);

  aNewRect.x = point.x;
  aNewRect.y = WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN) - point.y - 1;
  aNewRect.width = aOldRect.width;
  aNewRect.height = aOldRect.height;
  return NS_OK;
}

NS_METHOD nsWindow::ScreenToWidget( const nsRect &aOldRect, nsRect &aNewRect)
{
  POINTL point = { aOldRect.x,
                   WinQuerySysValue( HWND_DESKTOP, SV_CYSCREEN) - aOldRect.y - 1 };
  WinMapWindowPoints( HWND_DESKTOP, mWnd, &point, 1);

  PM2NS( point);

  aNewRect.x = point.x;
  aNewRect.y = point.y;
  aNewRect.width = aOldRect.width;
  aNewRect.height = aOldRect.height;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Convert nsEventStatus value to a windows boolean
//
//-------------------------------------------------------------------------

PRBool nsWindow::ConvertStatus(nsEventStatus aStatus)
{
  switch(aStatus) {
  case nsEventStatus_eIgnore:
    return PR_FALSE;
  case nsEventStatus_eConsumeNoDefault:
    return PR_TRUE;
  case nsEventStatus_eConsumeDoDefault:
    return PR_FALSE;
  default:
    NS_ASSERTION(0, "Illegal nsEventStatus enumeration value");
    break;
  }
  return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// Initialize an event to dispatch
//
//-------------------------------------------------------------------------
void nsWindow::InitEvent(nsGUIEvent& event, PRUint32 aEventType, nsPoint* aPoint)
{
    event.widget = this;
    NS_ADDREF(event.widget);
    event.nativeMsg = nsnull;

    if (nsnull == aPoint) {     // use the point from the event
      // get the message position in client coordinates and in twips
      POINTL ptl;
      WinQueryMsgPos( 0/*hab*/, &ptl);
      WinMapWindowPoints( HWND_DESKTOP, mWnd, &ptl, 1);

#if 0
      printf("++++++++++nsWindow::InitEvent (!pt) mapped point = %ld, %ld\n", ptl.x, ptl.y);
#endif

      PM2NS( ptl);

      event.point.x = ptl.x;
      event.point.y = ptl.y;

#if 0
      printf("++++++++++nsWindow::InitEvent (!pt) converted point = %ld, %ld\n", ptl.x, ptl.y);
#endif
   }
   else
   {                     // use the point override if provided
      event.point.x = aPoint->x;
      event.point.y = aPoint->y;

#if 0
      printf("++++++++++nsWindow::InitEvent point = %ld, %ld\n", aPoint->x, aPoint->y);
#endif
   }

   event.time = WinQueryMsgTime( 0/*hab*/);
   event.message = aEventType;

   /* OS2TODO
   mLastPoint.x = event.point.x;
   mLastPoint.y = event.point.y;
   */
}

//-------------------------------------------------------------------------
//
// Invokes callback and  ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWindow::DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus)
{
#if defined(TRACE_EVENTS) && defined(DEBUG_sobotka)
  DebugPrintEvent(*event, mWnd);
#endif

  aStatus = nsEventStatus_eIgnore;

  // Filters: if state is eInCreate, only send out NS_CREATE
  //          if state is eDoingDelete, don't send out anything because,
  //                                    well, the object's being deleted...
  if( ((mWindowState == nsWindowState_eInCreate) && event->message == NS_CREATE)
      || (mWindowState & nsWindowState_eLive) )
  {
    if (nsnull != mEventCallback) {
      aStatus = (*mEventCallback)( event);
    }
   
    // Dispatch to event listener if event was not consumed
    if ((aStatus != nsEventStatus_eIgnore) && (nsnull != mEventListener)) {
      aStatus = mEventListener->ProcessEvent(*event);
    }
  }

  return NS_OK;
}

//-------------------------------------------------------------------------
PRBool nsWindow::DispatchWindowEvent(nsGUIEvent* event)
{
  nsEventStatus status;
  DispatchEvent(event, status);
  return ConvertStatus(status);
}

PRBool nsWindow::DispatchWindowEvent(nsGUIEvent*event, nsEventStatus &aStatus) {
  DispatchEvent(event, aStatus);
  return ConvertStatus(aStatus);
}

//-------------------------------------------------------------------------
//
// Dispatch standard event
//
//-------------------------------------------------------------------------

PRBool nsWindow::DispatchStandardEvent(PRUint32 aMsg, PRUint8 aEST)
{
  nsGUIEvent event;
  event.eventStructType = aEST;
  InitEvent(event, aMsg);

  PRBool result = DispatchWindowEvent(&event);
  NS_RELEASE(event.widget);
  return result;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::CaptureRollupEvents(nsIRollupListener * aListener, 
                                            PRBool aDoCapture, 
                                            PRBool aConsumeRollupEvent)
{
  if (aDoCapture) {
    /* we haven't bothered carrying a weak reference to gRollupWidget because
       we believe lifespan is properly scoped. this next assertion helps
       assure that remains true. */
    NS_ASSERTION(!gRollupWidget, "rollup widget reassigned before release");
    gRollupConsumeRollupEvent = aConsumeRollupEvent;
    NS_IF_RELEASE(gRollupListener);
    NS_IF_RELEASE(gRollupWidget);
    gRollupListener = aListener;
    NS_ADDREF(aListener);
    gRollupWidget = this;
    NS_ADDREF(this);
  } else {
    NS_IF_RELEASE(gRollupListener);
    //gRollupListener = nsnull;
    NS_IF_RELEASE(gRollupWidget);
  }

  return NS_OK;
}

PRBool 
nsWindow::EventIsInsideWindow(nsWindow* aWindow) 
{
  RECTL r;
  POINTL mp;
  if (WinQueryMsgPos( 0/*hab*/, &mp)) {
    WinMapWindowPoints( HWND_DESKTOP, aWindow->mWnd, &mp, 1);
    WinQueryWindowRect( aWindow->mWnd, &r);
    // now make sure that it wasn't one of our children
    if (mp.x < r.xLeft || mp.x > r.xRight ||
       mp.y > r.yTop || mp.y < r.yBottom) {
      return PR_FALSE;
    }
  } 

  return PR_TRUE;
}

extern "C" {
PVOID APIENTRY WinQueryProperty(HWND hwnd, PCSZ  pszNameOrAtom);

PVOID APIENTRY WinRemoveProperty(HWND hwnd, PCSZ  pszNameOrAtom);

BOOL  APIENTRY WinSetProperty(HWND hwnd, PCSZ  pszNameOrAtom,
                              PVOID pvData, ULONG ulFlags);
}

static PCSZ GetNSWindowPropName() {
  static ATOM atom = 0;

  // this is threadsafe, even without locking;
  // even if there's a race, GlobalAddAtom("nsWindowPtr")
  // will just return the same value
  if (!atom) {
    atom = WinAddAtom(WinQuerySystemAtomTable(), "nsWindowPtr");
  }
  return (PCSZ)atom;
}

nsWindow * nsWindow::GetNSWindowPtr(HWND aWnd) {
  return (nsWindow *) ::WinQueryProperty(aWnd, GetNSWindowPropName());
}

BOOL nsWindow::SetNSWindowPtr(HWND aWnd, nsWindow * ptr) {
  if (ptr == NULL) {
    ::WinRemoveProperty(aWnd, GetNSWindowPropName());
    return TRUE;
  } else {
    return ::WinSetProperty(aWnd, GetNSWindowPropName(), (PVOID)ptr, 0);
  }
}

//
// DealWithPopups
//
// Handle events that may cause a popup (combobox, XPMenu, etc) to need to rollup.
//
BOOL
nsWindow :: DealWithPopups ( ULONG inMsg, MRESULT* outResult )
{
  if ( gRollupListener && gRollupWidget) {
#ifdef XP_OS2
    if( inMsg == WM_ACTIVATE || inMsg == WM_BUTTON1DOWN ||
        inMsg == WM_BUTTON2DOWN || inMsg == WM_BUTTON3DOWN) {
#else
    if (inMsg == WM_ACTIVATE || inMsg == WM_NCLBUTTONDOWN || inMsg == WM_LBUTTONDOWN ||
      inMsg == WM_RBUTTONDOWN || inMsg == WM_MBUTTONDOWN || 
      inMsg == WM_NCMBUTTONDOWN || inMsg == WM_NCRBUTTONDOWN || inMsg == WM_MOUSEACTIVATE) {
#endif
      // Rollup if the event is outside the popup.
      PRBool rollup = !nsWindow::EventIsInsideWindow((nsWindow*)gRollupWidget);
      
      // If we're dealing with menus, we probably have submenus and we don't
      // want to rollup if the click is in a parent menu of the current submenu.
      if (rollup) {
        nsCOMPtr<nsIMenuRollup> menuRollup ( do_QueryInterface(gRollupListener) );
        if ( menuRollup ) {
          nsCOMPtr<nsISupportsArray> widgetChain;
          menuRollup->GetSubmenuWidgetChain ( getter_AddRefs(widgetChain) );
          if ( widgetChain ) {
            PRUint32 count = 0;
            widgetChain->Count(&count);
            for ( PRUint32 i = 0; i < count; ++i ) {
              nsCOMPtr<nsISupports> genericWidget;
              widgetChain->GetElementAt ( i, getter_AddRefs(genericWidget) );
              nsCOMPtr<nsIWidget> widget ( do_QueryInterface(genericWidget) );
              if ( widget ) {
                nsIWidget* temp = widget.get();
                if ( nsWindow::EventIsInsideWindow((nsWindow*)temp) ) {
                  rollup = PR_FALSE;
                  break;
                }
              }
            } // foreach parent menu widget
          }
        } // if rollup listener knows about menus
      }

      // if we've determined that we should still rollup everything, do it.
      if ( rollup ) {
        gRollupListener->Rollup();

        // return TRUE tells Windows that the event is consumed, 
        // false allows the event to be dispatched
        //
        // So if we are NOT supposed to be consuming events, let it go through
        if (gRollupConsumeRollupEvent) {
          *outResult = (MRESULT)TRUE;
          return TRUE;
        } 
      }
    } // if event that might trigger a popup to rollup    
  } // if rollup listeners registered

  return FALSE;

} // DealWithPopups



// Are both windows from this app?
BOOL bothFromSameWindow( HWND hwnd1, HWND hwnd2 )
{
  HWND hwnd1Chain = WinQueryWindow( hwnd1, QW_OWNER );
  if (!hwnd1Chain)
    hwnd1Chain = WinQueryWindow( hwnd1, QW_PARENT );
  HWND hwnd1GChain = WinQueryWindow( hwnd1Chain, QW_OWNER );
  if (!hwnd1GChain)
    hwnd1GChain = WinQueryWindow( hwnd1Chain, QW_PARENT );
  HWND hwnd2Chain = WinQueryWindow( hwnd2, QW_OWNER );
  if (!hwnd2Chain)
    hwnd2Chain = WinQueryWindow( hwnd2, QW_PARENT );
  HWND hwnd2GChain = WinQueryWindow( hwnd2Chain, QW_OWNER );
  if (!hwnd2GChain)
    hwnd2GChain = WinQueryWindow( hwnd2Chain, QW_PARENT );
  while( hwnd1GChain) {
    hwnd1 = hwnd1Chain;
    hwnd1Chain = hwnd1GChain;
    hwnd1GChain = WinQueryWindow( hwnd1Chain, QW_OWNER );
    if (!hwnd1GChain)
      hwnd1GChain = WinQueryWindow( hwnd1Chain, QW_PARENT );
  }
  while( hwnd2GChain) {
    hwnd2 = hwnd2Chain;
    hwnd2Chain = hwnd2GChain;
    hwnd2GChain = WinQueryWindow( hwnd2Chain, QW_OWNER );
    if (!hwnd2GChain)
      hwnd2GChain = WinQueryWindow( hwnd2Chain, QW_PARENT );
  }
  return (hwnd1 == hwnd2);
}

//-------------------------------------------------------------------------
//
// the nsWindow procedure for all nsWindows in this toolkit
//
//-------------------------------------------------------------------------
MRESULT EXPENTRY fnwpNSWindow( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2)
{
   MRESULT popupHandlingResult;
   if( nsWindow::DealWithPopups(msg, &popupHandlingResult) )
      return popupHandlingResult;

   // Get the nsWindow for this hwnd
   nsWindow *wnd = nsWindow::GetNSWindowPtr(hwnd);

   // check to see if we have a rollup listener registered
   if( nsnull != gRollupListener && nsnull != gRollupWidget) {
      if( msg == WM_ACTIVATE || msg == WM_BUTTON1DOWN ||
          msg == WM_BUTTON2DOWN || msg == WM_BUTTON3DOWN) {
      // Rollup if the event is outside the popup.
      PRBool rollup = !nsWindow::EventIsInsideWindow((nsWindow*)gRollupWidget);
      
      // If we're dealing with menus, we probably have submenus and we don't
      // want to rollup if the click is in a parent menu of the current submenu.
      if (rollup) {
        nsCOMPtr<nsIMenuRollup> menuRollup ( do_QueryInterface(gRollupListener) );
        if ( menuRollup ) {
          nsCOMPtr<nsISupportsArray> widgetChain;
          menuRollup->GetSubmenuWidgetChain ( getter_AddRefs(widgetChain) );
          if ( widgetChain ) {
            PRUint32 count = 0;
            widgetChain->Count(&count);
            for ( PRUint32 i = 0; i < count; ++i ) {
              nsCOMPtr<nsISupports> genericWidget;
              widgetChain->GetElementAt ( i, getter_AddRefs(genericWidget) );
              nsCOMPtr<nsIWidget> widget ( do_QueryInterface(genericWidget) );
              if ( widget ) {
                nsIWidget* temp = widget.get();
                if ( nsWindow::EventIsInsideWindow((nsWindow*)temp) ) {
                  rollup = PR_FALSE;
                  break;
                }
              }
            } // foreach parent menu widget
          }
        } // if rollup listener knows about menus
      }
      }
      else if( msg == WM_SETFOCUS) {
         if( !mp2 && 
             !bothFromSameWindow( ((nsWindow*)gRollupWidget)->GetMainWindow(), 
                                  (HWND)mp1) ) {
            gRollupListener->Rollup();
         }
      }
   }

   // Messages which get re-routed if their source was an nsWindow
   // (it's very bad to reroute messages whose source isn't an nsWindow,
   // listboxes with scrollbars for example would break)
   switch( msg)
   {
      case WM_CONTROL:
      case WM_HSCROLL:
      case WM_VSCROLL: // !! potential problems here if canvas children
      {
         // assume parent == owner, true for our creations
         HWND hwndChild = WinWindowFromID( hwnd, SHORT1FROMMP( mp1));
         if( hwndChild)
         {
            nsWindow *w = nsWindow::GetNSWindowPtr(hwndChild);
            if( w)
               wnd = w;
         }
         break;
      }
   }

   MRESULT mRC = 0;

   if( wnd)
   {
      BOOL bPreHandling = g_bHandlingMouseClick;
      if( PR_FALSE == wnd->ProcessMessage( msg, mp1, mp2, mRC) &&
          WinIsWindow( (HAB)0, hwnd) && wnd->GetPrevWP())
      {
         mRC = (wnd->GetPrevWP())( hwnd, msg, mp1, mp2);

      }
      if( !bPreHandling)
         g_bHandlingMouseClick = FALSE;
   }
   else
      /* erm */ mRC = WinDefWindowProc( hwnd, msg, mp1, mp2);

   return mRC;
}

//-------------------------------------------------------------------------
//
// Utility method for implementing both Create(nsIWidget ...) and
// Create(nsNativeWidget...)
//-------------------------------------------------------------------------
void nsWindow::DoCreate( HWND hwndP, nsWindow *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
   mWindowState = nsWindowState_eInCreate;

   if( aInitData != nsnull) {
     SetWindowType(aInitData->mWindowType);
     SetBorderStyle(aInitData->mBorderStyle);
   }

   // Must ensure toolkit before attempting to thread-switch!
   if( !mToolkit)
   {
      if( aToolkit)
      {
         mToolkit = aToolkit;
         NS_ADDREF(mToolkit);
      }
      else if( aParent)
         mToolkit = aParent->GetToolkit();
      else
      {
         // it's some top level window with no toolkit passed in.
         // Create a default toolkit with the current thread
         mToolkit = new nsToolkit;
         NS_ADDREF(mToolkit);
         mToolkit->Init( PR_GetCurrentThread());
      }
      mOS2Toolkit = (nsToolkit*) mToolkit;
   }

   // Switch to the PM thread if necessary...
   if(mOS2Toolkit && !mOS2Toolkit->IsGuiThread())
   {
      ULONG args[7] = { hwndP, (ULONG) aParent, (ULONG) &aRect,
                        (ULONG) aHandleEventFunction,
                        (ULONG) aContext, (ULONG) aAppShell,
                        (ULONG) aInitData };
      MethodInfo info( this, nsWindow::CREATE, 7, args);
      mOS2Toolkit->CallMethod( &info);
   }
   else
      // This is potentially virtual; overridden in nsFrameWindow
      RealDoCreate( hwndP, aParent, aRect, aHandleEventFunction,
                    aContext, aAppShell, aInitData);

   mWindowState = nsWindowState_eLive;
}

void nsWindow::RealDoCreate( HWND              hwndP,
                             nsWindow         *aParent,
                             const nsRect     &aRect,
                             EVENT_CALLBACK    aHandleEventFunction,
                             nsIDeviceContext *aContext,
                             nsIAppShell      *aAppShell,
                             nsWidgetInitData *aInitData,
                             HWND              hwndOwner)
{

  // XXXX TEST HACK to get rid of window bunnies
  //  if (!(aRect.height > 1) && !(aRect.width > 1))
  //    return;

   // Set up parent data - don't addref to avoid circularity
   mParent = aParent;

   // Set up window style: first give subclass a chance to prepare
   if( aInitData)
      PreCreateWidget( aInitData);

   ULONG style = WindowStyle();
   if( aInitData)
   {
      if( aInitData->clipChildren)
         style |= WS_CLIPCHILDREN;
#if 0
      //
      // Windows has a slightly different idea of what the implications are
      // of a window having or not having the CLIPSIBLINGS style.
      // All 'canvas' components we create must have clipsiblings, or
      // strange things happen & performance actually degrades.
      //
      else
        style &= ~WS_CLIPCHILDREN;
#endif

      if( aInitData->clipSiblings)
         style |= WS_CLIPSIBLINGS;
      else
         style &= ~WS_CLIPSIBLINGS;
   }

   if( hwndP != HWND_DESKTOP)
   {
      // For pop-up menus, the parent is the desktop, but use the "parent" as owner
      if( aInitData && aInitData->mWindowType == eWindowType_popup)
      {
         if( !hwndOwner)
         {
            hwndOwner = hwndP;
         }
         hwndP = HWND_DESKTOP;
      }
      // For scrollbars, the parent is the owner, for notification purposes
      else if( !hwndOwner )
      {
         BOOL bHwndIsScrollBar = 
                           (!(strcmp( WindowClass(), WC_SCROLLBAR_STRING )));
         if( bHwndIsScrollBar )
         {
            hwndOwner = hwndP;
         }
      }
   }

   // Create a window: create hidden & then size to avoid swp_noadjust problems
   // owner == parent except for 'borderless top-level' -- see nsCanvas.cpp
   mWnd = WinCreateWindow( hwndP,
                           WindowClass(),
                           0,          // text
                           style,
                           0, 0, 0, 0, // pos/size
                           hwndOwner,
                           HWND_TOP,
                           mParent ? mParent->GetNextID() : 0,
                           0, 0);      // ctldata, presparams

   NS_ASSERTION( mWnd, "Couldn't create window");

#if DEBUG_sobotka
   printf("\n+++++++++++In nsWindow::RealDoCreate created 0x%lx, %d x %d\n",
	  mWnd, aRect.width, aRect.height);
   printf("+++++++++++Location =  %d x %d\n", aRect.x, aRect.y);
   printf("+++++++++++Parent = 0x%lx\n", GetParentHWND());
   printf("+++++++++++WINDOWCOUNT+ = %d\n", ++WINDOWCOUNT);
#endif

   // Make sure we have a device context from somewhere
   if( aContext)
   {
      mContext = aContext;
      NS_ADDREF(mContext);
   }
   else
   {
      nsresult rc = NS_OK;
      static NS_DEFINE_IID(kDeviceContextCID, NS_DEVICE_CONTEXT_CID);

      rc = nsComponentManager::CreateInstance( kDeviceContextCID, nsnull,
                                               NS_GET_IID(nsIDeviceContext),
                                               (void **)&mContext);
      if( NS_SUCCEEDED(rc))
         mContext->Init( (nsNativeWidget) mWnd);
#ifdef DEBUG
      else
         printf( "Couldn't find DC instance for nsWindow\n");
#endif
   }

   Resize( aRect.x, aRect.y, aRect.width, aRect.height, PR_FALSE);

   // Record bounds.  This is XP, the rect of the entire main window in
   // parent space.  Returned by GetBounds().
   // NB: We haven't subclassed yet, so callbacks to change mBounds won't
   //     have happened!
   mBounds = aRect;
   mBounds.height = GetHeight( aRect.height);

   // Record passed in things
   mAppShell = aAppShell;

//   NS_IF_ADDREF( mAppShell);
   GetAppShell();  // Let the base widget class update the refcount for us....
   mEventCallback = aHandleEventFunction;

   if( mParent)
      mParent->AddChild( this);

   // call the event callback to notify about creation

   DispatchStandardEvent( NS_CREATE );
   SubclassWindow(TRUE);
   PostCreateWidget();
}

//-------------------------------------------------------------------------
//
// Create the proper widget
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Create(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
   HWND hwndP = aParent ? (HWND)aParent->GetNativeData( NS_NATIVE_WINDOW)
                        : HWND_DESKTOP;

   DoCreate( hwndP, (nsWindow*) aParent, aRect, aHandleEventFunction,
             aContext, aAppShell, aToolkit, aInitData);

   return NS_OK;
}


//-------------------------------------------------------------------------
//
// create with a native parent
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::Create(nsNativeWidget aParent,
                         const nsRect &aRect,
                         EVENT_CALLBACK aHandleEventFunction,
                         nsIDeviceContext *aContext,
                         nsIAppShell *aAppShell,
                         nsIToolkit *aToolkit,
                         nsWidgetInitData *aInitData)
{
   // We need to find the nsWindow that goes with the native window, or controls
   // all get the ID of 0, and a zillion toolkits get created.
   //
   nsWindow *pParent = nsnull;
   HWND      hwndP = (HWND) aParent;

   if( hwndP && hwndP != HWND_DESKTOP)
      pParent = GetNSWindowPtr(hwndP);

   // XXX WC_MOZILLA will probably need a change here
   //
   if( !hwndP)
     hwndP = HWND_DESKTOP;

   DoCreate( hwndP, pParent, aRect, aHandleEventFunction, aContext,
             aAppShell, aToolkit, aInitData);

   return NS_OK;
}

//-------------------------------------------------------------------------
//
// Close this nsWindow
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Destroy()
{
  // Switch to the "main gui thread" if necessary... This method must
  // be executed on the "gui thread"...
   // Switch to the PM thread if necessary...
   if( mToolkit && !mOS2Toolkit->IsGuiThread())
   {
      MethodInfo info( this, nsWindow::DESTROY);
      mOS2Toolkit->CallMethod( &info);
   }
   else
   {
#if DEBUG_sobotka
     printf("\n++++++++++nsWindow::Destroy trashing 0x%lx\n\n", mHackDestroyWnd ? mHackDestroyWnd : mWnd);
     printf("+++++++++++WINDOWCOUNT- = %d\n\n", --WINDOWCOUNT);
#endif
      // avoid calling into other objects if we're being deleted, 'cos
      // they must have no references to us.
      if( (mWindowState & nsWindowState_eLive) && mParent )
         nsBaseWidget::Destroy();

      // just to be safe. If we're going away and for some reason we're still
      // the rollup widget, rollup and turn off capture.
      if (this == gRollupWidget) {
         if (gRollupListener)
            gRollupListener->Rollup();
         CaptureRollupEvents(nsnull, PR_FALSE, PR_TRUE);
      }

      if( mWnd)
      {
         gHwndBeingDestroyed = (mHackDestroyWnd ? mHackDestroyWnd : mWnd);
         WinDestroyWindow( gHwndBeingDestroyed);
         gHwndBeingDestroyed = NULLHANDLE;
      }
   }
   return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get this nsWindow parent
//
//-------------------------------------------------------------------------
nsIWidget* nsWindow::GetParent(void)
{
   nsWindow *widget = nsnull;
   if( nsnull != mParent)
   {
      NS_ADDREF(mParent);
      widget = mParent;
   }

   return widget;
}

// Now, OS/2 methods
HWND nsWindow::GetParentHWND() const
{
   HWND hwnd = 0;
   if( nsnull != mParent)
      hwnd = mParent->mWnd;
   else
      hwnd = WinQueryWindow( GetMainWindow(), QW_PARENT);
   return hwnd;
}

// ptl is in parent's space
void nsWindow::NS2PM_PARENT( POINTL &ptl)
{
   if( mParent)
      mParent->NS2PM( ptl);
   else
   {
      HWND hwndp = WinQueryWindow( GetMainWindow(), QW_PARENT);
      SWP  swp = { 0 };
      WinQueryWindowPos( hwndp, &swp);
      ptl.y = swp.cy - ptl.y - 1;
   }
}

// ptl is in this window's space
void nsWindow::NS2PM( POINTL &ptl)
{
   ptl.y = GetClientHeight() - ptl.y - 1;
#if 0
   printf("+++++++++In NS2PM client height = %ld\n", GetClientHeight());
#endif
}

// rcl is in this window's space
void nsWindow::NS2PM( RECTL &rcl)
{
   LONG height = rcl.yTop - rcl.yBottom;
   rcl.yTop = GetClientHeight() - rcl.yBottom;
   rcl.yBottom = rcl.yTop - height;
}

//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Show(PRBool bState)
{
   // doesn't seem to require a message queue.
   if( mWnd)
   {
      HWND hwnd = GetMainWindow();
      if( bState == PR_TRUE)
      {
         if( WinQueryWindow( hwnd, QW_PARENT) ==
             WinQueryObjectWindow(HWND_DESKTOP) )
            WinSetParent( hwnd, HWND_DESKTOP, FALSE);
         WinShowWindow( hwnd, TRUE);
      }
      else
      {
         WinShowWindow( hwnd, FALSE);
         if( WinQueryWindow( hwnd, QW_PARENT) ==
             WinQueryDesktopWindow(HWND_DESKTOP, NULLHANDLE) )
            WinSetParent( hwnd, HWND_OBJECT, FALSE);
      }
   }

   return NS_OK;
}

//-------------------------------------------------------------------------
//
// Return PR_TRUE if the whether the component is visible, PR_FALSE otherwise
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::IsVisible(PRBool & bState)
{
   // I guess this means visible & not showing...
   bState = WinIsWindowVisible( mWnd) ? PR_TRUE : PR_FALSE;
   return NS_OK;
}

//-------------------------------------------------------------------------
//
// Position the window behind the given window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::PlaceBehind(nsIWidget *aWidget, PRBool aActivate)
{
  HWND behind = aWidget ? (HWND)aWidget->GetNativeData(NS_NATIVE_WINDOW) : HWND_TOP;
  UINT flags = SWP_ZORDER;
  if (aActivate)
    flags |= SWP_ACTIVATE;

  WinSetWindowPos(mWnd, behind, 0, 0, 0, 0, flags);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Maximize, minimize or restore the window.
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWindow::SetSizeMode(PRInt32 aMode) {

  nsresult rv;
  PRBool visible;

  // save the requested state
  rv = nsBaseWidget::SetSizeMode(aMode);

  IsVisible( visible);
  if (NS_SUCCEEDED(rv) && visible) {
    ULONG mode;
    switch (aMode) {
      case nsSizeMode_Maximized :
        mode = SWP_MAXIMIZE;
        break;
      case nsSizeMode_Minimized :
        mode = SWP_MINIMIZE;
        break;
      default :
        mode = SWP_RESTORE;
    }
    ::WinSetWindowPos(mWnd, NULLHANDLE, 0L, 0L, 0L, 0L, mode);
  }
  return rv;
}

//-------------------------------------------------------------------------
// Return PR_TRUE in aForWindow if the given event should be processed
// assuming this is a modal window.
//-------------------------------------------------------------------------
NS_METHOD nsWindow::ModalEventFilter(PRBool aRealEvent, void *aEvent,
                                     PRBool *aForWindow)
{
  if( PR_FALSE == aRealEvent) {
    *aForWindow = PR_FALSE;
    return NS_OK;
  }
#if 0
  // Set aForWindow if either:
  //   * the message is for a descendent of the given window
  //   * the message is for another window, but is a message which
  //     should be allowed for a disabled window.
 
  PRBool isMouseEvent = PR_FALSE;
  PRBool isInWindow = PR_FALSE;
 
  // Examine the target window & find the frame
  // XXX should GetNativeData() use GetMainWindow() ?
  HWND hwnd = (HWND)GetNativeData(NS_NATIVE_WINDOW);
  hwnd = WinQueryWindow(hwnd, QW_PARENT);
 
  if( hwnd == mQmsg.hwnd || WinIsChild( mQmsg.hwnd, hwnd))
     isInWindow = PR_TRUE;
  else if (!isInWindow && gRollupWidget &&
           EventIsInsideWindow((nsWindow*)gRollupWidget))
     // include for consideration any popup menu which may be active at the moment
     isInWindow = PR_TRUE;
 
  // XXX really ought to do something about focus here
 
  if( !isInWindow)
  {
     // Block mouse messages for non-modal windows
     if( mQmsg.msg >= WM_MOUSEFIRST && mQmsg.msg <= WM_MOUSELAST)
        isMouseEvent = PR_TRUE;
     else if( mQmsg.msg >= WM_MOUSETRANSLATEFIRST &&
              mQmsg.msg <= WM_MOUSETRANSLATELAST)
        isMouseEvent = PR_TRUE;
     else if( mQmsg.msg == WM_MOUSEENTER || mQmsg.msg == WM_MOUSELEAVE)
        isMouseEvent = PR_TRUE;
  }
 
  // set dispatch indicator
  *aForWindow = isInWindow || !isMouseEvent;
#else
  *aForWindow = PR_TRUE;
#endif

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Constrain a potential move to fit onscreen
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::ConstrainPosition(PRInt32 *aX, PRInt32 *aY)
{
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Move(PRInt32 aX, PRInt32 aY)
{
   Resize( aX, aY, mBounds.width, mBounds.height, PR_FALSE);
   return NS_OK;
}

//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
   Resize( mBounds.x, mBounds.y, aWidth, aHeight, aRepaint);
   return NS_OK;
}

//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Resize(PRInt32 aX,
                      PRInt32 aY,
                      PRInt32 w,
                      PRInt32 h,
                      PRBool   aRepaint)
{
  //   NS_ASSERTION((w >=0 ), "Negative width passed to nsWindow::Resize");
  //   NS_ASSERTION((h >=0 ), "Negative height passed to nsWindow::Resize");

   // WinSetWindowPos() appears not to require a msgq
   if( mWnd)
   {
      // need to keep top-left corner in the same place
      // work out real coords of top left
      POINTL ptl= { aX, aY };
      NS2PM_PARENT( ptl);
      // work out real coords of bottom left
      ptl.y -= GetHeight( h) - 1;
      if( mParent)
      {
         WinMapWindowPoints( mParent->mWnd, WinQueryWindow(mWnd, QW_PARENT), &ptl, 1);
      }

      if( !SetWindowPos( 0, ptl.x, ptl.y, w, GetHeight(h), SWP_MOVE | SWP_SIZE))
         if( aRepaint)
            Invalidate(PR_FALSE);

#if DEBUG_sobotka
   printf("+++++++++++Resized 0x%lx at %ld, %ld to %ld x %ld\n\n", mWnd, ptl.x, ptl.y, w, GetHeight(h));
#endif

   }
   else
   {
      mBounds.x = aX;
      mBounds.y = aY;
      mBounds.width = w;
      mBounds.height = h;
   }
   return NS_OK;
}

//-------------------------------------------------------------------------
//
// Enable/disable this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Enable(PRBool bState)
{
   if (mWnd) {
      WinEnableWindow( GetMainWindow(), !!bState);
   }
   return NS_OK;
}


//-------------------------------------------------------------------------
//
// Give the focus to this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetFocus(PRBool aRaise)
{
    //
    // Switch to the "main gui thread" if necessary... This method must
    // be executed on the "gui thread"...
    //
    // Switch to the PM thread if necessary...
    if( !mOS2Toolkit->IsGuiThread())
    {
        MethodInfo info(this, nsWindow::SET_FOCUS);
        mOS2Toolkit->CallMethod(&info);
    }
    else
    if (mWnd && 
        (!gHwndBeingDestroyed || !WinIsChild(mWnd, gHwndBeingDestroyed))) {
        WinSetFocus( HWND_DESKTOP, mWnd);
    }
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get this component dimension
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::GetClientBounds(nsRect &aRect)
{

   // nsFrameWindow overrides this...
   aRect.x = 0;
   aRect.y = 0;
   aRect.width = mBounds.width;
   aRect.height = mBounds.height;
   return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set the background color
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetBackgroundColor(const nscolor &aColor)
{
   if( mWnd)
      SetPresParam( PP_BACKGROUNDCOLOR, aColor);
   mBackground = aColor;
   return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get this component font
//
//-------------------------------------------------------------------------
nsIFontMetrics *nsWindow::GetFont(void)
{
   nsIFontMetrics *metrics = nsnull;

   if( mToolkit)
   {
      char buf[2][128];
      int  ptSize;
   
      WinQueryPresParam( mWnd, PP_FONTNAMESIZE, 0, 0, 128, buf[0], 0);
   
      if( 2 == sscanf( buf[0], "%d.%s", &ptSize, buf[1])) // mmm, scanf()...
      {
         float twip2dev, twip2app;
         mContext->GetTwipsToDevUnits( twip2dev);
         mContext->GetDevUnitsToAppUnits( twip2app);
         twip2app *= twip2dev;
   
         nscoord appSize = (nscoord) (twip2app * ptSize * 20);
   
         nsFont font( buf[1], NS_FONT_STYLE_NORMAL, NS_FONT_VARIANT_NORMAL,
                      NS_FONT_WEIGHT_NORMAL, 0 /*decoration*/, appSize);
   
         mContext->GetMetricsFor( font, metrics);
      }
   }

   return metrics;
}

//-------------------------------------------------------------------------
//
// Set this component font
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetFont(const nsFont &aFont)
{
   if( mToolkit) // called from print-routine (XXX check)
   {
      const char *fontname = gWidgetModuleData->ConvertFromUcs( aFont.name);
   
      // jump through hoops to convert the size in the font (in app units)
      // into points. 
      float dev2twip, app2twip;
      mContext->GetDevUnitsToTwips( dev2twip);
      mContext->GetAppUnitsToDevUnits( app2twip);
      app2twip *= dev2twip;
   
      int points = NSTwipsToFloorIntPoints( nscoord( aFont.size * app2twip));
   
      char *buffer = new char [ strlen( fontname) + 6];
      sprintf( buffer, "%d.%s", points, fontname);

      BOOL rc = WinSetPresParam( mWnd, PP_FONTNAMESIZE,
                                 strlen( buffer) + 1, buffer);
#ifdef DEBUG
      if( !rc)
         printf( "WinSetPresParam PP_FONTNAMESIZE %s failed\n", buffer);
#endif
   
      delete [] buffer;
   }

   if( !mFont)
      mFont = new nsFont( aFont);
   else
      *mFont = aFont;

   return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set this component cursor
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::SetCursor(nsCursor aCursor)
{
  // Only change cursor if it's changing

  //XXX mCursor isn't always right.  Scrollbars and others change it, too.
  //XXX If we want this optimization we need a better way to do it.
  //if (aCursor != mCursor) {
    ULONG sptr = 0;
    switch(aCursor) {
       // builtins
       case eCursor_standard: sptr = SPTR_ARROW;    break;
       case eCursor_wait:     sptr = SPTR_WAIT;     break;
       case eCursor_select:   sptr = SPTR_TEXT;     break;
       case eCursor_sizeWE:   sptr = SPTR_SIZEWE;   break;
       case eCursor_sizeNS:   sptr = SPTR_SIZENS;   break;
       case eCursor_sizeNW:   sptr = SPTR_SIZENWSE; break;
       case eCursor_sizeSE:   sptr = SPTR_SIZENWSE; break;
       case eCursor_sizeNE:   sptr = SPTR_SIZENESW; break;
       case eCursor_sizeSW:   sptr = SPTR_SIZENESW; break;
       case eCursor_move:     sptr = SPTR_MOVE;     break;
       // custom
       case eCursor_hyperlink:
       case eCursor_arrow_north:
       case eCursor_arrow_north_plus:
       case eCursor_arrow_south:
       case eCursor_arrow_south_plus:
       case eCursor_arrow_west:
       case eCursor_arrow_west_plus:
       case eCursor_arrow_east:
       case eCursor_arrow_east_plus:
       case eCursor_crosshair:
       case eCursor_help:
       case eCursor_copy:
       case eCursor_alias:
       case eCursor_context_menu:
       case eCursor_cell:
       case eCursor_grab:
       case eCursor_grabbing:
       case eCursor_spinning:
       case eCursor_count_up:
       case eCursor_count_down:
       case eCursor_count_up_down:
          break;
 
       default:
          NS_ASSERTION(0, "Invalid cursor type");
          break;
    }
 
    if( sptr)
       mPointer = WinQuerySysPointer( HWND_DESKTOP, sptr, FALSE);
    else
       mPointer = gWidgetModuleData->GetPointer( aCursor);
 
    WinSetPointer( HWND_DESKTOP, mPointer);
    mCursor = aCursor;
  //}
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Invalidate(PRBool aIsSynchronous)
{
    if (mWnd)
    {
      WinInvalidateRect( mWnd, 0, FALSE);
#if 0
      if( PR_TRUE == aIsSynchronous) {
         Update();
      }
#endif
    }

    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Invalidate(const nsRect &aRect, PRBool aIsSynchronous)
{
  if (mWnd)
  {
    RECTL rcl = { aRect.x, aRect.y, aRect.x + aRect.width, aRect.y + aRect.height };
    NS2PM( rcl);

    WinInvalidateRect( mWnd, &rcl, FALSE);
#if 0
    if( PR_TRUE == aIsSynchronous) {
      Update();
    }
#endif
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsWindow::InvalidateRegion(const nsIRegion *aRegion, PRBool aIsSynchronous)

{
  nsresult rv = NS_OK;
  if (mWnd) {
    PRInt32 aX, aY, aWidth, aHeight;
    ((nsIRegion*)aRegion)->GetBoundingBox (&aX, &aY, &aWidth, &aHeight);

    RECTL rcl = { aX, aY, aX + aWidth, aY + aHeight };
    NS2PM (rcl);
    WinInvalidateRect (mWnd, &rcl, FALSE);

#if 0
        if( PR_TRUE == aIsSynchronous) {
          Update();
        }
#endif

  }
  return rv;  
}

//-------------------------------------------------------------------------
//
// Force a synchronous repaint of the window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Update()
{
  // Switch to the PM thread if necessary...
  if( !mOS2Toolkit->IsGuiThread())
  {
    MethodInfo info(this, nsWindow::UPDATE_WINDOW);
    mOS2Toolkit->CallMethod(&info);
  }
  else 
  if (mWnd)
    WinUpdateWindow( mWnd);
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void* nsWindow::GetNativeData(PRUint32 aDataType)
{
  void *rc = NULL;

    switch(aDataType) {
        case NS_NATIVE_WIDGET:
        case NS_NATIVE_WINDOW:
        case NS_NATIVE_PLUGIN_PORT:
            rc = (void*) mWnd;
            break;
        case NS_NATIVE_GRAPHIC:
            if( !mPS)
            {
                if( mDragInside) mPS = DrgGetPS( mWnd);
                else mPS = WinGetPS( mWnd);
            }
            mPSRefs++;
            rc = (void*) mPS;
            break;
        case NS_NATIVE_COLORMAP:
        case NS_NATIVE_DISPLAY:
        case NS_NATIVE_REGION:
        case NS_NATIVE_OFFSETX:
        case NS_NATIVE_OFFSETY: // could do this, I suppose; but why?
                                // OTOH, this might make plugins work!
            break;
        default: 
#ifdef DEBUG
            printf( "*** Someone's added a new NS_NATIVE value...\n");
#endif
            break;
    }

    return rc;
}

//~~~
void nsWindow::FreeNativeData(void * data, PRUint32 aDataType)
{
  switch(aDataType)
  {
    case NS_NATIVE_GRAPHIC:
      mPSRefs--;
      if( !mPSRefs)
      {
        BOOL rc;
        if( mDragInside) rc = DrgReleasePS( mPS);
        else rc = WinReleasePS( mPS);
#ifdef DEBUG
        if( !rc)
          printf( "Error from {Win/Drg}ReleasePS()\n");
#endif
        mPS = 0;
      }
      break;
    case NS_NATIVE_DISPLAY:
    case NS_NATIVE_REGION:
    case NS_NATIVE_OFFSETX:
    case NS_NATIVE_OFFSETY:
    case NS_NATIVE_WIDGET:
    case NS_NATIVE_WINDOW:
    case NS_NATIVE_PLUGIN_PORT:
    case NS_NATIVE_COLORMAP:
      break;
    default: 
#ifdef DEBUG
      printf( "*** Someone's added a new NS_NATIVE value...\n");
#endif
      break;
  }
}

//-------------------------------------------------------------------------
//
// Set the colormap of the window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetColorMap(nsColorMap *aColorMap)
{
   // SetColorMap - not implemented.
   // Any palette lives in the device context & should be altered there.
   // And shouldn't be altered at all, once libimg has decided what should
   // be in it.  So my opinion is this method shouldn't be called.
   return NS_ERROR_NOT_IMPLEMENTED;
}

//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
//XXX Scroll is obsolete and should go away soon
NS_METHOD nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
  RECTL rcl;

  if (nsnull != aClipRect)
  {
    rcl.xLeft = aClipRect->x;
    rcl.yBottom = aClipRect->y + aClipRect->height;
    rcl.xRight = rcl.xLeft + aClipRect->width;
    rcl.yTop = rcl.yBottom + aClipRect->height;
    NS2PM( rcl);
    // this rect is inex
  }

  WinScrollWindow( mWnd, aDx, -aDy, aClipRect ? &rcl : 0, 0, 0,
                   0, SW_SCROLLCHILDREN | SW_INVALIDATERGN);
  Update();
  return NS_OK;
}

NS_IMETHODIMP nsWindow::ScrollWidgets(PRInt32 aDx, PRInt32 aDy)
{
    // Scroll the entire contents of the window + change the offset of any child windows
  WinScrollWindow( mWnd, aDx, -aDy, NULL, NULL, NULL, 
                   NULL, SW_INVALIDATERGN | SW_SCROLLCHILDREN);
  Update(); // Force synchronous generation of NS_PAINT
  return NS_OK;
}

NS_IMETHODIMP nsWindow::ScrollRect(nsRect &aRect, PRInt32 aDx, PRInt32 aDy)
{
  RECTL rcl;

  rcl.xLeft = aRect.x;
  rcl.yBottom = aRect.y + aRect.height;
  rcl.xRight = rcl.xLeft + aRect.width;
  rcl.yTop = rcl.yBottom + aRect.height;
  NS2PM( rcl);

    // Scroll the bits in the window defined by trect. 
    // Child windows are not scrolled.
  WinScrollWindow(mWnd, aDx, -aDy, &rcl, NULL, NULL, 
                  NULL, SW_INVALIDATERGN);
  Update(); // Force synchronous generation of NS_PAINT
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Every function that needs a thread switch goes through this function
// by calling SendMessage (..WM_CALLMETHOD..) in nsToolkit::CallMethod.
//
//-------------------------------------------------------------------------
BOOL nsWindow::CallMethod(MethodInfo *info)
{
    BOOL bRet = TRUE;

    switch (info->methodId) {
        case nsWindow::CREATE:
            NS_ASSERTION(info->nArgs == 7, "Wrong number of arguments to CallMethod Create");
            DoCreate( (HWND)               info->args[0],
                      (nsWindow*)          info->args[1],
                      (const nsRect&)*(nsRect*) (info->args[2]),
                      (EVENT_CALLBACK)    (info->args[3]), 
                      (nsIDeviceContext*) (info->args[4]),
                      (nsIAppShell*)      (info->args[5]),
                      nsnull, /* toolkit */
                      (nsWidgetInitData*) (info->args[6]));
            break;

        case nsWindow::DESTROY:
            NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod Destroy");
            Destroy();
            break;

        case nsWindow::SET_FOCUS:
            NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod SetFocus");
            SetFocus(PR_FALSE);
            break;

        case nsWindow::UPDATE_WINDOW:
            NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod UpdateWindow");
            Update();
            break;

        case nsWindow::SET_TITLE:
            NS_ASSERTION(info->nArgs == 1, "Wrong number of arguments to CallMethod SetTitle");
            SetTitle( (const nsString &) info->args[0]);
            break;

        case nsWindow::GET_TITLE:
            NS_ASSERTION(info->nArgs == 2, "Wrong number of arguments to CallMethod GetTitle");
            GetWindowText( *((nsString*) info->args[0]),
                           (PRUint32*)info->args[1]);
            break;

        default:
            bRet = FALSE;
            break;
    }

    return bRet;
}

//-------------------------------------------------------------------------
//
// OnKey
//
//-------------------------------------------------------------------------
// Key handler.  Specs for the various text messages are really confused;
// see other platforms for best results of how things are supposed to work.
//
// Perhaps more importantly, the main man listening to these events (besides
// random bits of javascript) is ender -- see 
// mozilla/editor/base/nsEditorEventListeners.cpp.
//
PRBool nsWindow::OnKey( MPARAM mp1, MPARAM mp2)
{
   nsKeyEvent event;
   USHORT     fsFlags = SHORT1FROMMP(mp1);
   USHORT     usVKey = SHORT2FROMMP(mp2);
   USHORT     usChar = SHORT1FROMMP(mp2);
   UCHAR      uchScan = CHAR4FROMMP(mp1);
   int        unirc = ULS_SUCCESS;

   // It appears we're not supposed to transmit shift, control & alt events
   // to gecko.  Shrug.
   //
   // XXX this may be wrong, but is what gtk is doing...
   if( fsFlags & KC_VIRTUALKEY &&
      (usVKey == VK_SHIFT || usVKey == VK_CTRL ||
       usVKey == VK_ALT || usVKey == VK_ALTGRAF)) return PR_FALSE;

   // Now check if it's a dead-key
   if( fsFlags & KC_DEADKEY)
   {
      // XXX CUA says we're supposed to give some kind of feedback `display the
      //     dead key glyph'.  I'm not sure if we can use the COMPOSE messages
      //     to do this -- it should really be done by someone who can test it
      //     & has some idea what `ought' to happen...

      return PR_TRUE;
   }

   // Now dispatch a keyup/keydown event.  This one is *not* meant to
   // have the unicode charcode in.

   nsPoint point(0,0);
   InitEvent( event, (fsFlags & KC_KEYUP) ? NS_KEY_UP : NS_KEY_DOWN, &point);
   event.keyCode   = WMChar2KeyCode( mp1, mp2);
   event.isShift   = (fsFlags & KC_SHIFT) ? PR_TRUE : PR_FALSE;
   event.isControl = (fsFlags & KC_CTRL) ? PR_TRUE : PR_FALSE;
   event.isAlt     = (fsFlags & KC_ALT) ? PR_TRUE : PR_FALSE;
   event.isMeta    = PR_FALSE;
   event.eventStructType = NS_KEY_EVENT;
   event.charCode = 0;

   PRBool rc = DispatchWindowEvent( &event);

   // Break off now if this was a key-up.
   if( fsFlags & KC_KEYUP)
   {
      NS_RELEASE( event.widget);
      return rc;
   }

   // Break off if we've got an "invalid composition" -- that is, the user
   // typed a deadkey last time, but has now typed something that doesn't
   // make sense in that context.
   if( fsFlags & KC_INVALIDCOMP)
   {
      mHaveDeadKey = FALSE;
      // XXX actually, not sure whether we're supposed to abort the keypress
      //     or process it as though the dead key has been pressed.
      NS_RELEASE( event.widget);
      return rc;
   }

   // Now we need to dispatch a keypress event which has the unicode char.

   event.message = NS_KEY_PRESS;

   if( usChar)
   {
      USHORT inbuf[2];
      UniChar outbuf[4];
      inbuf[0] = usChar;
      inbuf[1] = '\0';
      outbuf[0] = (UniChar)0;

      gWidgetModuleData->ConvertToUcs( (char *)inbuf, (PRUnichar *)outbuf, 4);

      event.charCode = outbuf[0];

      if( event.isControl && !event.isShift && event.charCode >= 'A' && event.charCode <= 'Z' )
      {
         event.charCode = tolower(event.charCode);
      }
      else if( !event.isControl && !event.isAlt && event.charCode != 0)
      {
         if ( !(fsFlags & KC_VIRTUALKEY) || 
              ((fsFlags & KC_CHAR) && (event.keyCode == 0)) )
         {
            event.isShift = PR_FALSE;
            event.keyCode = 0;
         }
         else if (usVKey == VK_SPACE)
         {
            event.isShift = PR_FALSE;
         }
         else  // Real virtual key 
         {
            event.charCode = 0;
         }
      }
   }

   rc = DispatchWindowEvent( &event);
   NS_RELEASE( event.widget);
   return rc;
}

// 'Window procedure'
PRBool nsWindow::ProcessMessage( ULONG msg, MPARAM mp1, MPARAM mp2, MRESULT &rc)
{
    PRBool result = PR_FALSE; // call the default window procedure

    switch (msg) {
#if 0
        case WM_COMMAND: // fire off menu selections
        {
          USHORT usSrc = SHORT1FROMMP( mp2);
          if( usSrc == CMDSRC_MENU || usSrc == CMDSRC_ACCELERATOR)
            result = OnMenuClick( SHORT1FROMMP(mp1));
          break;
        }

        case WM_INITMENU:
          result = OnActivateMenu( HWNDFROMMP(mp2), TRUE);
          break;

        case WM_MENUEND:
          result = OnActivateMenu( HWNDFROMMP(mp2), FALSE);
          break;
#endif

#if 0  // Tooltips appear to be gone
        case WMU_SHOW_TOOLTIP:
        {
          nsTooltipEvent event;
          InitEvent( event, NS_SHOW_TOOLTIP);
          event.tipIndex = LONGFROMMP(mp1);
          event.eventStructType = NS_TOOLTIP_EVENT;
          result = DispatchWindowEvent(&event);
          NS_RELEASE(event.widget);
          break;
        }

        case WMU_HIDE_TOOLTIP:
          result = DispatchStandardEvent( NS_HIDE_TOOLTIP );
          break;
#endif
        case WM_CONTROL: // remember this is resent to the orginator...
          result = OnControl( mp1, mp2);
          break;

        case WM_CLOSE:  // close request
          mWindowState |= nsWindowState_eClosing;
          DispatchStandardEvent( NS_XUL_CLOSE );
          result = PR_TRUE; // abort window closure
          break;

        case WM_DESTROY:
            // clean up.
            OnDestroy();
            result = PR_TRUE;
            break;

        case WM_PAINT:
            result = OnPaint();
            break;

        case WM_TRANSLATEACCEL:
            {
              PQMSG pQmsg = (PQMSG)mp1;
              if (pQmsg->msg == WM_CHAR) 
              {
                LONG mp1 = (LONG)pQmsg->mp1;
                LONG mp2 = (LONG)pQmsg->mp2;
       
                // If we have a shift+enter combination, return false
                // immediately.  OS/2 considers the shift+enter
                // combination an accelerator and will translate it into
                // a WM_OPEN message.  When this happens we do not get a
                // WM_CHAR for the down transition of the enter key when
                // shift is also pressed and OnKeyDown will not be called.
                if ( (SHORT1FROMMP(mp1) & (KC_VIRTUALKEY | KC_SHIFT)) &&
                     (SHORT2FROMMP(mp2) == VK_ENTER)
                   )
                {
                  return(PR_TRUE);
                }
              }
            }
            break;

        case WM_CHAR:
            result = OnKey( mp1, mp2);
            break;

        case WM_QUERYCONVERTPOS:
          {
            PRECTL pCursorRect = (PRECTL)mp1;
            nsCompositionEvent event;
            nsPoint point;
            point.x = 0;
            point.y = 0;
            InitEvent(event,NS_COMPOSITION_START,&point);
            event.eventStructType = NS_COMPOSITION_START;
            event.compositionMessage = NS_COMPOSITION_START;
            DispatchWindowEvent(&event);
            NS_RELEASE(event.widget);
            if(event.theReply.mCursorPosition.height)
            {
              pCursorRect->xLeft = event.theReply.mCursorPosition.x + 1;
              pCursorRect->xRight = pCursorRect->xLeft + event.theReply.mCursorPosition.width - 1;
              pCursorRect->yTop = GetClientHeight() - event.theReply.mCursorPosition.y;
              pCursorRect->yBottom = pCursorRect->yTop - event.theReply.mCursorPosition.height + 1;

              point.x = 0;
              point.y = 0;
              InitEvent(event,NS_COMPOSITION_END,&point);
              event.eventStructType = NS_COMPOSITION_END;
              event.compositionMessage = NS_COMPOSITION_END;
              DispatchWindowEvent(&event);
              NS_RELEASE(event.widget);

              rc = (MRESULT)QCP_CONVERT;
            }
            else
              rc = (MRESULT)QCP_NOCONVERT;

            result = PR_TRUE;
            break;
          }

        // Mouseclicks: we don't dispatch CLICK events because they just cause
        // trouble: gecko seems to expect EITHER buttondown/up OR click events
        // and so that's what we give it.
        //
        // Plus we make WM_CHORD do a button3down in order to get warp-4 paste
        // behaviour (see nsEditorEventListeners.cpp)
    
        case WM_BUTTON1DOWN:
          result = DispatchMouseEvent( NS_MOUSE_LEFT_BUTTON_DOWN, mp1, mp2);
          break;
        case WM_BUTTON1UP:
          result = DispatchMouseEvent( NS_MOUSE_LEFT_BUTTON_UP, mp1, mp2);
          break;
        case WM_BUTTON1DBLCLK:
          result = DispatchMouseEvent( NS_MOUSE_LEFT_DOUBLECLICK, mp1, mp2);
          break;
    
        case WM_BUTTON2DOWN:
          result = DispatchMouseEvent( NS_MOUSE_RIGHT_BUTTON_DOWN, mp1, mp2);
          break;
        case WM_BUTTON2UP:
          result = DispatchMouseEvent( NS_MOUSE_RIGHT_BUTTON_UP, mp1, mp2);
          break;
        case WM_BUTTON2DBLCLK:
          result = DispatchMouseEvent( NS_MOUSE_RIGHT_DOUBLECLICK, mp1, mp2);
          break;
        case WM_CONTEXTMENU:
          result = DispatchMouseEvent( NS_CONTEXTMENU, mp1, mp2);
          break;
    
        case WM_CHORD:

          {
             POINTL ptl;
             WinQueryMsgPos( 0/*hab*/, &ptl);
             WinMapWindowPoints( HWND_DESKTOP, mWnd, &ptl, 1);
             USHORT usFlags = 0;
             if (WinIsKeyDown( VK_SHIFT))
                usFlags |= KC_SHIFT;
             if (WinIsKeyDown( VK_CTRL))
                usFlags |= KC_CTRL;
             if (WinIsKeyDown( VK_ALT) || WinIsKeyDown( VK_ALTGRAF))
                usFlags |= KC_ALT;
             result = DispatchMouseEvent( NS_MOUSE_MIDDLE_CLICK, MPFROM2SHORT(ptl.x, ptl.y), MPFROM2SHORT(0,usFlags));
          }
          break;
        case WM_BUTTON3DOWN:
          result = DispatchMouseEvent( NS_MOUSE_MIDDLE_BUTTON_DOWN, mp1, mp2);
          break;
        case WM_BUTTON3UP:
          result = DispatchMouseEvent( NS_MOUSE_MIDDLE_BUTTON_UP, mp1, mp2);
          break;
        case WM_BUTTON3DBLCLK:
          result = DispatchMouseEvent( NS_MOUSE_MIDDLE_DOUBLECLICK, mp1, mp2);
          break;
    
        case WM_MOUSEMOVE:
          {
            static POINTL ptlLastPos = { -1, -1 };
            // See if mouse has actually moved.
            if ( ptlLastPos.x == (SHORT)SHORT1FROMMP(mp1) &&
                 ptlLastPos.y == (SHORT)SHORT2FROMMP(mp1)) {
                return PR_TRUE;
            } else {
                // Yes, remember new position.
                ptlLastPos.x = (SHORT)SHORT1FROMMP(mp1);
                ptlLastPos.y = (SHORT)SHORT2FROMMP(mp1);
            }
          }
          result = DispatchMouseEvent( NS_MOUSE_MOVE, mp1, mp2);
          break;
        case WM_MOUSEENTER:
          result = DispatchMouseEvent( NS_MOUSE_ENTER, mp1, mp2);
          break;
        case WM_MOUSELEAVE:
          result = DispatchMouseEvent( NS_MOUSE_EXIT, mp1, mp2);
          break;
    
        case WM_HSCROLL:
        case WM_VSCROLL:
          result = OnScroll( msg, mp1, mp2);
          break;

        case WM_FOCUSCHANGED:
          /* Make a test to see if we are in the process of closing, deleting
           * or destroying this nsWindow.  If we are, then we've already
           * gotten rid of our nsDocShell->mScriptGlobal.  To handle focus
           * events when there is no mScriptGlobal is apparently a bad thing.
           */
          if(!(mWindowState & nsWindowState_eClosing))
          {
             PRBool isMozWindowTakingFocus = PR_TRUE;
             if( SHORT1FROMMP( mp2 ) || mWnd == WinQueryFocus(HWND_DESKTOP) )
             {
               result = DispatchFocus( NS_GOTFOCUS, isMozWindowTakingFocus );
               // Only sending an Activate event when we get a WM_ACTIVATE message
               // isn't good enough; need to do this every time we gain focus.
               if( !gJustGotActivate )
               {
                 gJustGotActivate = PR_TRUE;
                 if ( WinIsChild( mWnd, HWNDFROMMP(mp1)) && mNextID == mNextCmdID == 1)
                    result = DispatchFocus( NS_PLUGIN_ACTIVATE, isMozWindowTakingFocus );
                 else
                    result = DispatchFocus( NS_ACTIVATE, isMozWindowTakingFocus );
                 gJustGotActivate = PR_FALSE;
               }
             }
             else
             {
               char className[19];
               ::WinQueryClassName((HWND)mp1, 19, className);
               if (strcmp(className, WindowClass()))
                  isMozWindowTakingFocus = PR_FALSE;
   
               if( gJustGotDeactivate )
               {
                 gJustGotDeactivate = PR_FALSE;
                 result = DispatchFocus( NS_DEACTIVATE, isMozWindowTakingFocus );
               }
               result = DispatchFocus( NS_LOSTFOCUS, isMozWindowTakingFocus );
             }
          }
          break;
    
        case WM_WINDOWPOSCHANGED: 
          result = OnReposition( (PSWP) mp1);
          break;
    
    
        case WM_REALIZEPALETTE:          // hopefully only nsCanvas & nsFrame
          result = OnRealizePalette();   // will need this
          break;
    
        case WM_PRESPARAMCHANGED:
          // This is really for font-change notifies.  Do that first.
          rc = GetPrevWP()( mWnd, msg, mp1, mp2);
          OnPresParamChanged( mp1, mp2);
          result = PR_TRUE;
          break;
    
        case DM_DRAGOVER:
          result = OnDragOver( mp1, mp2, rc);
          break;
    
        case DM_DRAGLEAVE:
          result = OnDragLeave( mp1, mp2);
          break;
    
        case DM_DROP:
          result = OnDrop( mp1, mp2);
          break;
    
        // Need to handle this method in order to keep track of whether there
        // is a drag inside the window; we need to do *this* so that we can
        // generate DRAGENTER messages [which os/2 doesn't provide].
        case DM_DROPHELP:
          mDragInside = FALSE;
          break;
    }
    
    return result;
}


// -----------------------------------------------------------------------
//
// Subclass (or remove the subclass from) this component's nsWindow
//
// -----------------------------------------------------------------------
void nsWindow::SubclassWindow(BOOL bState)
{
  if (NULL != mWnd) {
    NS_PRECONDITION(::WinIsWindow(0, mWnd), "Invalid window handle");
    
    if (bState) {
        // change the nsWindow proc
        mPrevWndProc = WinSubclassWindow(mWnd, fnwpNSWindow);
        NS_ASSERTION(mPrevWndProc, "Null standard window procedure");
        // connect the this pointer to the nsWindow handle
        SetNSWindowPtr(mWnd, this);
    } 
    else {
        WinSubclassWindow(mWnd, mPrevWndProc);
        SetNSWindowPtr(mWnd, NULL);
        mPrevWndProc = NULL;
    }
  }
}

// Overridable OnMessage() methods ------------------------------------------

PRBool nsWindow::OnReposition( PSWP pSwp)
{
   PRBool result = PR_FALSE;
 
   if( pSwp->fl & SWP_MOVE && !(pSwp->fl & SWP_MINIMIZE))
   {
      // need screen coords.
      POINTL ptl = { pSwp->x, pSwp->y + pSwp->cy - 1 };
      WinMapWindowPoints( WinQueryWindow( mWnd, QW_PARENT), GetParentHWND(), &ptl, 1);
      PM2NS_PARENT( ptl);
      mBounds.x = ptl.x;
      mBounds.y = ptl.y;
 
      WinMapWindowPoints( GetParentHWND(), HWND_DESKTOP, &ptl, 1);
      result = OnMove( ptl.x, ptl.y);
   }
   if( pSwp->fl & SWP_SIZE && !(pSwp->fl & SWP_MINIMIZE))
      result = OnResize( pSwp->cx, pSwp->cy);

   return result;
}

PRBool nsWindow::OnRealizePalette()
{
   return PR_FALSE;
}

PRBool nsWindow::OnPresParamChanged( MPARAM mp1, MPARAM mp2)
{
   return PR_FALSE;
}

// This is necessary because notification codes are defined from 1 for each
// control: thus we cannot tell the difference between an EN_SELECT and
// a TABN_SELECT, etc.
// So delegate to those classes who actually want to handle the thing.
PRBool nsWindow::OnControl( MPARAM mp1, MPARAM mp2)
{
   return PR_TRUE; // default to speed things up a bit...
}


//-------------------------------------------------------------------------
//
// WM_DESTROY has been called
//
//-------------------------------------------------------------------------
void nsWindow::OnDestroy()
{
   SubclassWindow( PR_FALSE);
   mWnd = 0;

   // if we were in the middle of deferred window positioning then free up
   if( mSWPs) free( mSWPs);
   mSWPs = 0;
   mlHave = mlUsed = 0;

   // release any ps (erm, probably an error if this is necessary)
   if( mPS)
      WinReleasePS( mPS);
   mPS = 0;

   // release references to context, toolkit, appshell, children
   nsBaseWidget::OnDestroy();

   // kill font
   delete mFont;

   // release menubar
//   NS_IF_RELEASE(mMenuBar);

   // dispatching of the event may cause the reference count to drop to 0
   // and result in this object being deleted. To avoid that, add a
   // reference and then release it after dispatching the event.
   //
   // It's important *not* to do this if we're being called from the
   // destructor -- this would result in our destructor being called *again*
   // from the Release() below.  This is very bad...
   if( !(nsWindowState_eDoingDelete & mWindowState) )
   {
      AddRef();
      DispatchStandardEvent( NS_DESTROY );
      Release();
   }

   // dead widget
   mWindowState |= nsWindowState_eDead;
   mWindowState &= ~(nsWindowState_eLive|nsWindowState_ePrecreate|
                     nsWindowState_eInCreate);
}

//-------------------------------------------------------------------------
//
// Move
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnMove(PRInt32 aX, PRInt32 aY)
{            
  // Params here are in XP-space for the desktop
  nsGUIEvent event;
  InitEvent( event, NS_MOVE);
  event.point.x = aX;
  event.point.y = aY;
  event.eventStructType = NS_GUI_EVENT;

  PRBool result = DispatchWindowEvent( &event);
  NS_RELEASE(event.widget);
  return result;
}

//-------------------------------------------------------------------------
//
// Paint
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnPaint()
{
   return PR_FALSE;
}


//-------------------------------------------------------------------------
//
// Send a resize message to the listener
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnResize(PRInt32 aX, PRInt32 aY)
{
   mBounds.width = aX;
   mBounds.height = aY;
   return DispatchResizeEvent( aX, aY);
}

PRBool nsWindow::DispatchResizeEvent( PRInt32 aX, PRInt32 aY)
{
   PRBool result;
   // call the event callback 
   nsSizeEvent event;
   nsRect      rect( 0, 0, aX, aY);

   InitEvent( event, NS_SIZE);
   event.eventStructType = NS_SIZE_EVENT;
   event.windowSize = &rect;             // this is the *client* rectangle
   event.mWinWidth = mBounds.width;
   event.mWinHeight = mBounds.height;

   result = DispatchWindowEvent( &event);
   NS_RELEASE(event.widget);
   return result;
}                                           

//-------------------------------------------------------------------------
//
// Deal with all sort of mouse event
//
//-------------------------------------------------------------------------
PRBool nsWindow::DispatchMouseEvent( PRUint32 aEventType, MPARAM mp1, MPARAM mp2)
{
  PRBool result = PR_FALSE;

  if (nsnull == mEventCallback && nsnull == mMouseListener) {
    return result;
  }

  nsMouseEvent event;

  // Stop multiple messages for the same PM action
  if( g_bHandlingMouseClick)
    return result;

  // Mouse leave & enter messages don't seem to have position built in.
  if( aEventType && aEventType != NS_MOUSE_ENTER && aEventType != NS_MOUSE_EXIT)
  {
    POINTL ptl = { (SHORT)SHORT1FROMMP( mp1), (SHORT)SHORT2FROMMP( mp1) };
    PM2NS( ptl);
    nsPoint pt( ptl.x, ptl.y);
    InitEvent( event, aEventType, &pt);

    USHORT usFlags = SHORT2FROMMP( mp2);
    event.isShift = (usFlags & KC_SHIFT) ? PR_TRUE : PR_FALSE;
    event.isControl = (usFlags & KC_CTRL) ? PR_TRUE : PR_FALSE;
    event.isAlt = (usFlags & KC_ALT) ? PR_TRUE : PR_FALSE;
  }
  else
  {
    InitEvent( event, aEventType, nsnull);
    event.isShift = WinIsKeyDown( VK_SHIFT);
    event.isControl = WinIsKeyDown( VK_CTRL);
    event.isAlt = WinIsKeyDown( VK_ALT) || WinIsKeyDown( VK_ALTGRAF);
  }

  event.isMeta    = PR_FALSE;
  event.eventStructType = NS_MOUSE_EVENT;

  //Dblclicks are used to set the click count, then changed to mousedowns
  if (aEventType == NS_MOUSE_LEFT_DOUBLECLICK ||
      aEventType == NS_MOUSE_RIGHT_DOUBLECLICK) {
    event.message = (aEventType == NS_MOUSE_LEFT_DOUBLECLICK) ? 
                    NS_MOUSE_LEFT_BUTTON_DOWN : NS_MOUSE_RIGHT_BUTTON_DOWN;
    event.clickCount = 2;
  }
  else {
    event.clickCount = 1;
  }

  nsPluginEvent pluginEvent;

  switch (aEventType)//~~~
  {
    case NS_MOUSE_LEFT_BUTTON_DOWN:
      pluginEvent.event = WM_BUTTON1DOWN;
      break;
    case NS_MOUSE_LEFT_BUTTON_UP:
      pluginEvent.event = WM_BUTTON1UP;
      break;
    case NS_MOUSE_LEFT_DOUBLECLICK:
      pluginEvent.event = WM_BUTTON1DBLCLK;
      break;
    case NS_MOUSE_RIGHT_BUTTON_DOWN:
      pluginEvent.event = WM_BUTTON2DOWN;
      break;
    case NS_MOUSE_RIGHT_BUTTON_UP:
      pluginEvent.event = WM_BUTTON2UP;
      break;
    case NS_MOUSE_RIGHT_DOUBLECLICK:
      pluginEvent.event = WM_BUTTON2DBLCLK;
      break;
    case NS_MOUSE_MIDDLE_BUTTON_DOWN:
      pluginEvent.event = WM_BUTTON3DOWN;
      break;
    case NS_MOUSE_MIDDLE_BUTTON_UP:
      pluginEvent.event = WM_BUTTON3UP;
      break;
    case NS_MOUSE_MIDDLE_DOUBLECLICK:
      pluginEvent.event = WM_BUTTON3DBLCLK;
      break;
    case NS_MOUSE_MOVE:
      pluginEvent.event = WM_MOUSEMOVE;
      break;
    default:
      break;
  }

  pluginEvent.wParam = 0;
/* OS2TODO
  pluginEvent.wParam |= (event.isShift) ? MK_SHIFT : 0;
  pluginEvent.wParam |= (event.isControl) ? MK_CONTROL : 0;
*/
  pluginEvent.lParam = MAKELONG(event.point.x, event.point.y);

  event.nativeMsg = (void *)&pluginEvent;

  // call the event callback 
  if (nsnull != mEventCallback) {

    result = DispatchWindowEvent(&event);

#if 0  // OS2TODO
    if (aEventType == NS_MOUSE_MOVE) {

      // if we are not in mouse capture mode (mouse down and hold)
      // then use "this" window
      // if we are in mouse capture, then all events are being directed
      // back to the nsWindow doing the capture. So therefore, the detection
      // of whether we are in a new nsWindow is wrong. Meaning this MOUSE_MOVE
      // event hold the captured windows pointer not the one the mouse is over.
      //
      // So we use "WindowFromPoint" to find what window we are over and 
      // set that window into the mouse trailer timer.
      if (!mIsInMouseCapture) {
        MouseTrailer * mouseTrailer = MouseTrailer::GetMouseTrailer(0);
        MouseTrailer::SetMouseTrailerWindow(this);
        mouseTrailer->CreateTimer();
      } else {
        POINT mp;
        DWORD pos = ::GetMessagePos();
        mp.x      = LOWORD(pos);
        mp.y      = HIWORD(pos);

        // OK, now find out if we are still inside
        // the captured native window
        POINT cpos;
        cpos.x = LOWORD(pos);
        cpos.y = HIWORD(pos);

        nsWindow * someWindow = NULL;
        HWND hWnd = ::WindowFromPoint(mp);
        if (hWnd != NULL) {
          ::ScreenToClient(hWnd, &cpos);
          RECT r;
          VERIFY(::GetWindowRect(hWnd, &r));
          if (cpos.x >= r.left && cpos.x <= r.right &&
              cpos.y >= r.top && cpos.y <= r.bottom) {
            // yes we are so we should be able to get a valid window
            // although, strangley enough when we are on the frame part of the
            // window we get right here when in capture mode
            // but this window won't match the capture mode window so
            // we are ok
            someWindow = (nsWindow*)::GetWindowLong(hWnd, GWL_USERDATA);
          } 
        }
        // only set the window into the mouse trailer if we have a good window
        if (nsnull != someWindow)  {
          MouseTrailer * mouseTrailer = MouseTrailer::GetMouseTrailer(0);
          MouseTrailer::SetMouseTrailerWindow(someWindow);
          mouseTrailer->CreateTimer();
        }
      }

      nsRect rect;
      GetBounds(rect);
      rect.x = 0;
      rect.y = 0;

      if (rect.Contains(event.point.x, event.point.y)) {
        if (gCurrentWindow == NULL || gCurrentWindow != this) {
          if ((nsnull != gCurrentWindow) && (!gCurrentWindow->mIsDestroying)) {
            MouseTrailer::IgnoreNextCycle();
            gCurrentWindow->DispatchMouseEvent(NS_MOUSE_EXIT, gCurrentWindow->GetLastPoint());
          }
          gCurrentWindow = this;
          if (!mIsDestroying) {
            gCurrentWindow->DispatchMouseEvent(NS_MOUSE_ENTER);
          }
        }
      } 
    } else if (aEventType == NS_MOUSE_EXIT) {
      if (gCurrentWindow == this) {
        gCurrentWindow = nsnull;
      }
    }
#endif //OS2TODO
    g_bHandlingMouseClick = TRUE;
    NS_RELEASE(event.widget);
    return result;
  }

  if (nsnull != mMouseListener) {
    switch (aEventType) {
      case NS_MOUSE_MOVE: {
        result = ConvertStatus(mMouseListener->MouseMoved(event));
        nsRect rect;
        GetBounds(rect);
        if (rect.Contains(event.point.x, event.point.y)) {
          if (gCurrentWindow == NULL || gCurrentWindow != this) {
            gCurrentWindow = this;
          }
        } else {
          //printf("Mouse exit");
        }

      } break;

      case NS_MOUSE_LEFT_BUTTON_DOWN:
      case NS_MOUSE_MIDDLE_BUTTON_DOWN:
      case NS_MOUSE_RIGHT_BUTTON_DOWN:
        result = ConvertStatus(mMouseListener->MousePressed(event));
        break;

      case NS_MOUSE_LEFT_BUTTON_UP:
      case NS_MOUSE_MIDDLE_BUTTON_UP:
      case NS_MOUSE_RIGHT_BUTTON_UP:
        result = ConvertStatus(mMouseListener->MouseReleased(event));
//        result = ConvertStatus(mMouseListener->MouseClicked(event));
        break;

      case NS_MOUSE_LEFT_CLICK:
      case NS_MOUSE_MIDDLE_CLICK:
      case NS_MOUSE_RIGHT_CLICK:
        result = ConvertStatus(mMouseListener->MouseClicked(event));
        break;
    } // switch
  } 

  g_bHandlingMouseClick = TRUE;
  NS_RELEASE(event.widget);
  return result;
}


//-------------------------------------------------------------------------
//
// Deal with focus messages
//
//-------------------------------------------------------------------------
PRBool nsWindow::DispatchFocus(PRUint32 aEventType, PRBool isMozWindowTakingFocus)
{
  // call the event callback 
  if (mEventCallback) {
    nsFocusEvent event;
    event.eventStructType = NS_FOCUS_EVENT;
    InitEvent(event, aEventType);

    //focus and blur event should go to their base widget loc, not current mouse pos
    event.point.x = 0;
    event.point.y = 0;

    event.isMozWindowTakingFocus = isMozWindowTakingFocus;

    nsPluginEvent pluginEvent;

    switch (aEventType)//~~~
    {
      case NS_GOTFOCUS:
        pluginEvent.event = WM_SETFOCUS;
        break;
      case NS_LOSTFOCUS:
        pluginEvent.event = WM_FOCUSCHANGED;
        break;
      case NS_PLUGIN_ACTIVATE:
        pluginEvent.event = WM_FOCUSCHANGED;
        break;
      default:
        break;
    }

    event.nativeMsg = (void *)&pluginEvent;

    PRBool result = DispatchWindowEvent(&event);
    NS_RELEASE(event.widget);
    return result;
  }
  return PR_FALSE;
}


//-------------------------------------------------------------------------
//
// Deal with scrollbar messages
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnScroll( ULONG msgid, MPARAM mp1, MPARAM mp2)
{
   return( (msgid == WM_HSCROLL) ? OnHScroll(mp1, mp2) : OnVScroll(mp1, mp2) );
}

PRBool nsWindow::OnVScroll( MPARAM mp1, MPARAM mp2)
{
    if (nsnull != mEventCallback) {
        nsMouseScrollEvent scrollEvent;
        scrollEvent.eventStructType = NS_MOUSE_SCROLL_EVENT;
        InitEvent(scrollEvent, NS_MOUSE_SCROLL);
        scrollEvent.isShift = WinIsKeyDown( VK_SHIFT);
        scrollEvent.isControl = WinIsKeyDown( VK_CTRL);
        scrollEvent.isAlt = WinIsKeyDown( VK_ALT) || WinIsKeyDown( VK_ALTGRAF);
        scrollEvent.isMeta = PR_FALSE;
        scrollEvent.scrollFlags = nsMouseScrollEvent::kIsVertical;
        switch (SHORT2FROMMP(mp2)) {
          case SB_LINEUP:
            scrollEvent.delta = -1;
            break;
          case SB_LINEDOWN:
            scrollEvent.delta = 1;
            break;
          case SB_PAGEUP:
            scrollEvent.scrollFlags |= nsMouseScrollEvent::kIsFullPage;
            scrollEvent.delta = -1;
            break;
          case SB_PAGEDOWN:
            scrollEvent.scrollFlags |= nsMouseScrollEvent::kIsFullPage;
            scrollEvent.delta = 1;
            break;
          default:
            scrollEvent.delta = 0;
            break;
        }
        DispatchWindowEvent(&scrollEvent);
        NS_RELEASE(scrollEvent.widget);
    }
    return PR_FALSE;
}

PRBool nsWindow::OnHScroll( MPARAM mp1, MPARAM mp2)
{
#if 0  /* OS2TODO */
    if (nsnull != mEventCallback) {
        nsMouseScrollEvent scrollEvent;
        scrollEvent.eventStructType = NS_MOUSE_SCROLL_EVENT;
        InitEvent(scrollEvent, NS_MOUSE_SCROLL);
        scrollEvent.isShift = WinIsKeyDown( VK_SHIFT);
        scrollEvent.isControl = WinIsKeyDown( VK_CTRL);
        scrollEvent.isAlt = WinIsKeyDown( VK_ALT) || WinIsKeyDown( VK_ALTGRAF);
        scrollEvent.isMeta = PR_FALSE;
        scrollEvent.scrollFlags = nsMouseScrollEvent::kIsHorizontal;
        switch (SHORT2FROMMP(mp2)) {
          case SB_LINELEFT:
            scrollEvent.delta = -1;
            break;
          case SB_LINERIGHT:
            scrollEvent.delta = 1;
            break;
          case SB_PAGELEFT:
            scrollEvent.scrollFlags |= nsMouseScrollEvent::kIsFullPage;
            scrollEvent.delta = -1;
            break;
          case SB_PAGERIGHT:
            scrollEvent.scrollFlags |= nsMouseScrollEvent::kIsFullPage;
            scrollEvent.delta = 1;
            break;
          default:
            scrollEvent.delta = 0;
            break;
        }
        DispatchWindowEvent(&scrollEvent);
        NS_RELEASE(scrollEvent.widget);
    }
#endif
    return PR_FALSE;
}


NS_METHOD nsWindow::SetTitle(const nsString& aTitle) 
{
   // Switch to the PM thread if necessary...
   if( mOS2Toolkit && !mOS2Toolkit->IsGuiThread())
   {
      ULONG ulong = (ULONG) &aTitle;
      MethodInfo info( this, nsWindow::SET_TITLE, 1, &ulong);
      mOS2Toolkit->CallMethod( &info);
   }
   else if( mWnd)
   {
      WinSetWindowText( GetMainWindow(),
                        gWidgetModuleData->ConvertFromUcs( aTitle));
   }
   return NS_OK;
} 

NS_METHOD nsWindow::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
  aWidth = mPreferredWidth;
  aHeight = mPreferredHeight;
  return NS_OK;
}

NS_METHOD nsWindow::SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
{
  mPreferredWidth = aWidth;
  mPreferredHeight = aHeight;
  return NS_OK;
}

// We don' wan' no steekin' borda!
NS_METHOD nsWindow::GetBorderSize(PRInt32 &aWidth, PRInt32 &aHeight)
{
  aWidth = 0;
  aHeight = 0;
  return NS_OK;
}


// --------------------------------------------------------------------------
// OS2-specific routines to emulate Windows behaviors
// --------------------------------------------------------------------------

BOOL nsWindow::SetWindowPos( HWND ib, long x, long y, long cx, long cy, ULONG flags)
{
   BOOL bDeferred = FALSE;

   if( mParent && mParent->mSWPs) // XXX bit implicit...
   {
      mParent->DeferPosition( GetMainWindow(), ib, x, y, cx, cy, flags);
      bDeferred = TRUE;
   }
   else // WinSetWindowPos appears not to need msgq (hmm)
      WinSetWindowPos( GetMainWindow(), ib, x, y, cx, cy, GetSWPFlags(flags));

   // When the window is actually sized, mBounds will be updated in the fnwp.

   return bDeferred;
}

// --------------------------------------------------------------------------
// Colours, fonts, painting -------------------------------------------------

nscolor nsWindow::QueryPresParam( ULONG ppID)
{
   nscolor col = 0;
   RGB2    rgb;
   ULONG   found = 0;

   WinQueryPresParam( mWnd, ppID, 0, &found,
                      sizeof( RGB2), &rgb, QPF_PURERGBCOLOR);

   if( found == ppID)
      col = NS_RGB( rgb.bRed, rgb.bGreen, rgb.bBlue);
   return col;
}

void nsWindow::SetPresParam( ULONG ppID, const nscolor &c)
{
   RGB2 rgb = { NS_GET_B( c), NS_GET_G( c), NS_GET_R( c), 0 };
   WinSetPresParam( mWnd, ppID, sizeof( RGB2), &rgb);
}

nscolor nsWindow::GetForegroundColor()
{
   return (mWnd ? QueryPresParam( PP_FOREGROUNDCOLOR) : mForeground);
}

nsresult nsWindow::SetForegroundColor( const nscolor &aColor)
{
   if( mWnd)
      SetPresParam( PP_FOREGROUNDCOLOR, aColor);
   mForeground = aColor;

   return NS_OK;
}

nscolor nsWindow::GetBackgroundColor()
{
   return (mWnd ? QueryPresParam( PP_BACKGROUNDCOLOR) : mBackground);
}

nsresult nsWindow::GetWindowText( nsString &aStr, PRUint32 *rc)
{
   // Switch to the PM thread if necessary...
   if( !mOS2Toolkit->IsGuiThread())
   {
      ULONG args[] = { (ULONG) &aStr, (ULONG) rc };
      MethodInfo info( this, nsWindow::GET_TITLE, 2, args);
      mOS2Toolkit->CallMethod( &info);
   }
   else if( mWnd)
   {
      // XXX there must be some way to query the text straight into the string!
      int length = WinQueryWindowTextLength( mWnd);
      char *tmp = new char [ length + 1 ];
      WinQueryWindowText( mWnd, length + 1, tmp);
      aStr.AssignWithConversion( tmp);
      delete [] tmp;
   }
   return NS_OK;
}

// --------------------------------------------------------------------------
// Misc helpers -------------------------------------------------------------

// May need to thread-switch these...
void nsWindow::AddToStyle( ULONG style)
{
   if( mWnd)
      WinSetWindowBits( mWnd, QWL_STYLE, style, style);
}

void nsWindow::RemoveFromStyle( ULONG style)
{
   if( mWnd)
   {
      ULONG oldStyle = WinQueryWindowULong( mWnd, QWL_STYLE);
      oldStyle &= ~style;
      WinSetWindowULong( mWnd, QWL_STYLE, oldStyle);
   }
}

void nsWindow::GetStyle( ULONG &out)
{
   if( mWnd)
      out = WinQueryWindowULong( mWnd, QWL_STYLE);
}


#if 0 // Handled by XP code now
// --------------------------------------------------------------------------
// Tooltips -----------------------------------------------------------------

nsresult nsWindow::SetTooltips( PRUint32 cTips, nsRect *areas[])
{
   nsTooltipManager::GetManager()->SetTooltips( this, cTips, areas);
   return NS_OK;
}

nsresult nsWindow::UpdateTooltips( nsRect *aNewTips[])
{
   nsTooltipManager::GetManager()->UpdateTooltips( this, aNewTips);
   return NS_OK;
}

nsresult nsWindow::RemoveTooltips()
{
   nsTooltipManager::GetManager()->RemoveTooltips( this);
   return NS_OK;
}

#endif

// --------------------------------------------------------------------------
// Drag'n'drop --------------------------------------------------------------

#define DispatchDragDropEvent(msg) DispatchStandardEvent(msg,NS_DRAGDROP_EVENT)

// XXXX KNOCKED OUT UNTIL nsDragService.cpp is fixed

PRBool nsWindow::OnDragOver( MPARAM mp1, MPARAM mp2, MRESULT &mr)
{
   // Drawing drop feedback should be fun, have to get DrgGetPS() involved
   // somehow.

   // Tell drag service about the drag
  //   gWidgetModuleData->dragService->InitDragOver( (PDRAGINFO) mp1);

   // Invoke gecko for enter if appropriate
  //   if( !mDragInside)
  //   {
  //      DispatchDragDropEvent( NS_DRAGDROP_ENTER);
  //      mDragInside = TRUE;
  //   }

   // Invoke for 'over' to set candrop flag
  //   DispatchDragDropEvent( NS_DRAGDROP_OVER);

   // Get action back from drag service
  //   mr = gWidgetModuleData->dragService->TermDragOver();

   return PR_TRUE;
}

PRBool nsWindow::OnDragLeave( MPARAM mp1, MPARAM mp2)
{
  //   gWidgetModuleData->dragService->InitDragExit( (PDRAGINFO) mp1);
  //   DispatchDragDropEvent( NS_DRAGDROP_EXIT);
  //   gWidgetModuleData->dragService->TermDragExit();

  //   mDragInside = FALSE;

   return PR_TRUE;
}

PRBool nsWindow::OnDrop( MPARAM mp1, MPARAM mp2)
{
  //   gWidgetModuleData->dragService->InitDrop( (PDRAGINFO) mp1);
  //   DispatchDragDropEvent( NS_DRAGDROP_DROP);
  //   gWidgetModuleData->dragService->TermDrop();

   mDragInside = FALSE;

   return PR_TRUE;
}

// --------------------------------------------------------------------------
// Raptor object access

// 'Native data'
// function to translate from a WM_CHAR to an NS VK_ constant ---------------
PRUint32 WMChar2KeyCode( MPARAM mp1, MPARAM mp2)
{
   PRUint32 rc = 0;
   USHORT   flags = SHORT1FROMMP( mp1);

   // First check for characters.
   // This is complicated by keystrokes such as Ctrl+K not having the KC_CHAR
   // bit set, but thankfully they do have the character actually there.
   //
   // So go on the assumption that `if not vkey or deadkey then char'
   //
   if( !(flags & (KC_VIRTUALKEY | KC_DEADKEY)))
   {
      rc = SHORT1FROMMP(mp2);

      if( rc < 0xFF)
      {
         switch( rc)
         {
            case ';':  rc = NS_VK_SEMICOLON;     break;
            case '=':  rc = NS_VK_EQUALS;        break;
            case '*':  rc = NS_VK_MULTIPLY;      break;
            case '+':  rc = NS_VK_ADD;           break;
            case '-':  rc = NS_VK_SUBTRACT;      break;
            case '.':  rc = NS_VK_PERIOD;        break; // NS_VK_DECIMAL ?
            case '|':  rc = NS_VK_SEPARATOR;     break;
            case ',':  rc = NS_VK_COMMA;         break;
            case '/':  rc = NS_VK_SLASH;         break; // NS_VK_DIVIDE ?
            case '`':  rc = NS_VK_BACK_QUOTE;    break;
            case '(':  rc = NS_VK_OPEN_BRACKET;  break;
            case '\\': rc = NS_VK_BACK_SLASH;    break;
            case ')':  rc = NS_VK_CLOSE_BRACKET; break;
            case '\'': rc = NS_VK_QUOTE;         break;
         }
      }
   }
   else if( flags & KC_VIRTUALKEY)
   {
      USHORT vk = SHORT2FROMMP( mp2);
      if( isNumPadScanCode(CHAR4FROMMP(mp1)) && 
          ( ((flags & KC_ALT) && (CHAR4FROMMP(mp1) != PMSCAN_PADPERIOD)) || 
            ((flags & (KC_CHAR | KC_SHIFT)) == KC_CHAR) ) )
      {
         // No virtual key for Alt+NumPad or NumLock+NumPad
         rc = 0;
      }
      else if( !(flags & KC_CHAR) || isNumPadScanCode(CHAR4FROMMP(mp1)) ||
          (vk == VK_BACKSPACE) || (vk == VK_TAB) || (vk == VK_BACKTAB) ||
          (vk == VK_ENTER) || (vk == VK_NEWLINE) || (vk == VK_SPACE) )
      {
         if( vk >= VK_F1 && vk <= VK_F24)
            rc = NS_VK_F1 + (vk - VK_F1);
         else switch( vk)
         {
            case VK_NUMLOCK:   rc = NS_VK_NUM_LOCK; break;
            case VK_SCRLLOCK:  rc = NS_VK_SCROLL_LOCK; break;
            case VK_ESC:       rc = NS_VK_ESCAPE; break; // NS_VK_CANCEL
            case VK_BACKSPACE: rc = NS_VK_BACK; break;
            case VK_TAB:       rc = NS_VK_TAB; break;
            case VK_BACKTAB:   rc = NS_VK_TAB; break; // layout tests for isShift
            case VK_CLEAR:     rc = NS_VK_CLEAR; break;
            case VK_NEWLINE:   rc = NS_VK_RETURN; break;
            case VK_ENTER:     rc = NS_VK_RETURN; break;
            case VK_SHIFT:     rc = NS_VK_SHIFT; break;
            case VK_CTRL:      rc = NS_VK_CONTROL; break;
            case VK_ALT:       rc = NS_VK_ALT; break;
            case VK_PAUSE:     rc = NS_VK_PAUSE; break;
            case VK_CAPSLOCK:  rc = NS_VK_CAPS_LOCK; break;
            case VK_SPACE:     rc = NS_VK_SPACE; break;
            case VK_PAGEUP:    rc = NS_VK_PAGE_UP; break;
            case VK_PAGEDOWN:  rc = NS_VK_PAGE_DOWN; break;
            case VK_END:       rc = NS_VK_END; break;
            case VK_HOME:      rc = NS_VK_HOME; break;
            case VK_LEFT:      rc = NS_VK_LEFT; break;
            case VK_UP:        rc = NS_VK_UP; break;
            case VK_RIGHT:     rc = NS_VK_RIGHT; break;
            case VK_DOWN:      rc = NS_VK_DOWN; break;
            case VK_PRINTSCRN: rc = NS_VK_PRINTSCREEN; break;
            case VK_INSERT:    rc = NS_VK_INSERT; break;
            case VK_DELETE:    rc = NS_VK_DELETE; break;
         }
      }
   }

   return rc;
}
