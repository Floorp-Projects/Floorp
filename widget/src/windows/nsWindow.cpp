/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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
#include "nsStringUtil.h"
#include <windows.h>
//#include <winuser.h>
#include <zmouse.h>
//#include "sysmets.h"
#include "nsGfxCIID.h"
#include "resource.h"
#include <commctrl.h>
#include "prtime.h"
#include "nsIRenderingContextWin.h"

#include "nsIMenu.h"
#include "nsMenu.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"
#include "nsMenuItem.h"
#include <imm.h>

//#define NEW_DRAG_AND_DROP
#ifdef NEW_DRAG_AND_DROP
#include "nsNativeDragTarget.h"
//#include "nsDropTarget.h"
//#include "DragDrop.h"
//#include "DropTar.h"
//#include "DropSrc.h"
#endif

// For clipboard support
#include "nsIServiceManager.h"
#include "nsIClipboard.h"
#include "nsWidgetsCID.h"
static NS_DEFINE_IID(kIClipboardIID,    NS_ICLIPBOARD_IID);
static NS_DEFINE_CID(kCClipboardCID,    NS_CLIPBOARD_CID);


BOOL nsWindow::sIsRegistered = FALSE;

nsWindow* nsWindow::gCurrentWindow = nsnull;

#define IS_VK_DOWN(a) (PRBool)(((GetKeyState(a) & 0x80)) ? (PR_TRUE) : (PR_FALSE))

// Global variable 
//     g_hinst - handle of the application instance 
extern HINSTANCE g_hinst; 

static NS_DEFINE_IID(kIWidgetIID,       NS_IWIDGET_IID);
static NS_DEFINE_IID(kIMenuIID,         NS_IMENU_IID);
static NS_DEFINE_IID(kIMenuItemIID,     NS_IMENUITEM_IID);
//static NS_DEFINE_IID(kIMenuListenerIID, NS_IMENULISTENER_IID);

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
    mBackground         = ::GetSysColor(COLOR_BTNFACE);
    mBrush              = ::CreateSolidBrush(NSRGB_2_COLOREF(mBackground));
    mForeground         = ::GetSysColor(COLOR_WINDOWTEXT);
    mPalette            = NULL;
    mIsShiftDown        = PR_FALSE;
    mIsControlDown      = PR_FALSE;
    mIsAltDown          = PR_FALSE;
    mIsDestroying       = PR_FALSE;
    mOnDestroyCalled    = PR_FALSE;
    mTooltip            = NULL;
    mDeferredPositioner = NULL;
    mLastPoint.x        = 0;
    mLastPoint.y        = 0;
    mPreferredWidth     = 0;
    mPreferredHeight    = 0;
    mFont               = nsnull;
    mIsVisible          = PR_FALSE;
    mHas3DBorder        = PR_FALSE;
    mMenuBar            = nsnull;
    mMenuCmdId          = 0;
   
    mHitMenu            = nsnull;
    mHitSubMenus        = new nsVoidArray();
    mVScrollbar         = nsnull;

	mIMEProperty		= 0;
	mIMEIsComposing		= PR_FALSE;
	mIMECompositionString = NULL;
	mIMECompositionStringSize = 0;
	mIMECompositionStringSize = 0;
	mIMECompositionUniString = NULL;

#ifdef NEW_DRAG_AND_DROP
    mNativeDragTarget = nsnull;
    //mDragSource         = nsnull;
#endif

}


//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
  mIsDestroying = PR_TRUE;
  if (gCurrentWindow == this) {
    gCurrentWindow = nsnull;
  }

  MouseTrailer * mouseTrailer = MouseTrailer::GetMouseTrailer(0);
  if (mouseTrailer->GetMouseTrailerWindow() == this) {
    mouseTrailer->DestroyTimer();
  } 

  // If the widget was released without calling Destroy() then the native
  // window still exists, and we need to destroy it
  if (NULL != mWnd) {
    Destroy();
  }

  NS_IF_RELEASE(mHitMenu); // this should always have already been freed by the deselect

#ifdef NEW_DRAG_AND_DROP
  //NS_IF_RELEASE(mDragTarget); 
  //NS_IF_RELEASE(mDragSource); 
#endif

  //XXX Temporary: Should not be caching the font
  delete mFont;
}


//-------------------------------------------------------------------------
//
// Default for height modification is to do nothing
//
//-------------------------------------------------------------------------

PRInt32 nsWindow::GetHeight(PRInt32 aProposedHeight)
{
  return(aProposedHeight);
}

//-------------------------------------------------------------------------
//
// Deferred Window positioning
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::BeginResizingChildren(void)
{
  if (NULL == mDeferredPositioner)
    mDeferredPositioner = ::BeginDeferWindowPos(1);
  return NS_OK;
}

NS_METHOD nsWindow::EndResizingChildren(void)
{
  if (NULL != mDeferredPositioner) {
    ::EndDeferWindowPos(mDeferredPositioner);
    mDeferredPositioner = NULL;
  }
  return NS_OK;
}

// DoCreateTooltip - creates a tooltip control and adds some tools  
//     to it. 
// Returns the handle of the tooltip control if successful or NULL
//     otherwise. 
// hwndOwner - handle of the owner window 
// 

void nsWindow::AddTooltip(HWND hwndOwner,nsRect* aRect, int aId) 
{ 
    TOOLINFO ti;    // tool information
    memset(&ti, 0, sizeof(TOOLINFO));
  
    // Make sure the common control DLL is loaded
    InitCommonControls(); 
 
    // Create a tooltip control for the window if needed
    if (mTooltip == (HWND) NULL) {
        mTooltip = CreateWindow(TOOLTIPS_CLASS, (LPSTR) NULL, TTS_ALWAYSTIP, 
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
        NULL, (HMENU) NULL, 
        nsToolkit::mDllInstance,
        NULL);
    }
 
    if (mTooltip == (HWND) NULL) 
        return;

    ti.cbSize = sizeof(TOOLINFO); 
    ti.uFlags = TTF_SUBCLASS;
    ti.hwnd = hwndOwner; 
    ti.hinst = nsToolkit::mDllInstance; 
    ti.uId = aId;
    ti.lpszText = (LPSTR)" "; // must set text to 
                              // something for tooltip to give events; 
    ti.rect.left = aRect->x; 
    ti.rect.top = aRect->y; 
    ti.rect.right = aRect->x + aRect->width; 
    ti.rect.bottom = aRect->y + aRect->height; 

    if (!SendMessage(mTooltip, TTM_ADDTOOL, 0, 
            (LPARAM) (LPTOOLINFO) &ti)) 
        return; 
}

NS_METHOD nsWindow::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
  POINT point;
  point.x = aOldRect.x;
  point.y = aOldRect.y;
  ::ClientToScreen((HWND)GetNativeData(NS_NATIVE_WINDOW), &point);
  aNewRect.x = point.x;
  aNewRect.y = point.y;
  aNewRect.width = aOldRect.width;
  aNewRect.height = aOldRect.height;
  return NS_OK;
}

NS_METHOD nsWindow::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
  POINT point;
  point.x = aOldRect.x;
  point.y = aOldRect.y;
  ::ScreenToClient((HWND)GetNativeData(NS_NATIVE_WINDOW), &point);
  aNewRect.x = point.x;
  aNewRect.y = point.y;
  aNewRect.width = aOldRect.width;
  aNewRect.height = aOldRect.height;
  return NS_OK;
} 

//-------------------------------------------------------------------------
//
// Setup initial tooltip rectangles
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[])
{
  RemoveTooltips();
  for (int i = 0; i < (int)aNumberOfTips; i++) {
    AddTooltip(mWnd, aTooltipAreas[i], i);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Update all tooltip rectangles
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::UpdateTooltips(nsRect* aNewTips[])
{
  TOOLINFO ti;
  memset(&ti, 0, sizeof(TOOLINFO));
  ti.cbSize = sizeof(TOOLINFO);
  ti.hwnd = mWnd;
  // Get the number of tooltips
  UINT count = ::SendMessage(mTooltip, TTM_GETTOOLCOUNT, 0, 0); 
  NS_ASSERTION(count > 0, "Called UpdateTooltips before calling SetTooltips");

  for (UINT i = 0; i < count; i++) {
    ti.uId = i;
    int result =::SendMessage(mTooltip, TTM_ENUMTOOLS, i, (LPARAM) (LPTOOLINFO)&ti);

    nsRect* newTip = aNewTips[i];
    ti.rect.left    = newTip->x; 
    ti.rect.top     = newTip->y; 
    ti.rect.right   = newTip->x + newTip->width; 
    ti.rect.bottom  = newTip->y + newTip->height; 
    ::SendMessage(mTooltip, TTM_NEWTOOLRECT, 0, (LPARAM) (LPTOOLINFO)&ti);

  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Remove all tooltip rectangles
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::RemoveTooltips()
{
  TOOLINFO ti;
  memset(&ti, 0, sizeof(TOOLINFO));
  ti.cbSize = sizeof(TOOLINFO);
  long val;

  if (mTooltip == NULL)
    return NS_ERROR_FAILURE;

  // Get the number of tooltips
  UINT count = ::SendMessage(mTooltip, TTM_GETTOOLCOUNT, 0, (LPARAM)&val); 
  for (UINT i = 0; i < count; i++) {
    ti.uId = i;
    ti.hwnd = mWnd;
    ::SendMessage(mTooltip, TTM_DELTOOL, 0, (LPARAM) (LPTOOLINFO)&ti);
  }
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

    if (nsnull == aPoint) {     // use the point from the event
      // get the message position in client coordinates and in twips
      DWORD pos = ::GetMessagePos();
      POINT cpos;

      cpos.x = LOWORD(pos);
      cpos.y = HIWORD(pos);

      if (mWnd != NULL) {
        ::ScreenToClient(mWnd, &cpos);
        event.point.x = cpos.x;
        event.point.y = cpos.y;
      } else {
        event.point.x = 0;
        event.point.y = 0;
      }
    }
    else {                      // use the point override if provided
      event.point.x = aPoint->x;
      event.point.y = aPoint->y;
    }

    event.time = ::GetMessageTime();
    event.message = aEventType;

    mLastPoint.x = event.point.x;
    mLastPoint.y = event.point.y;
}

//-------------------------------------------------------------------------
//
// Invokes callback and  ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------

NS_IMETHODIMP nsWindow::DispatchEvent(nsGUIEvent* event, nsEventStatus & aStatus)
{
  aStatus = nsEventStatus_eIgnore;
  
  //if (nsnull != mMenuListener)
  //	aStatus = mMenuListener->MenuSelected(*event);
  if (nsnull != mEventCallback) {
    aStatus = (*mEventCallback)(event);
  }

    // Dispatch to event listener if event was not consumed
  if ((aStatus != nsEventStatus_eIgnore) && (nsnull != mEventListener)) {
    aStatus = mEventListener->ProcessEvent(*event);
  }

  // the window can be destroyed during processing of seemingly innocuous events like, say,
  // mousedowns due to the magic of scripting. mousedowns will return nsEventStatus_eIgnore,
  // which causes problems with the deleted window. therefore:
  if (mOnDestroyCalled)
    aStatus = nsEventStatus_eConsumeNoDefault;
  return NS_OK;
}

//-------------------------------------------------------------------------
PRBool nsWindow::DispatchWindowEvent(nsGUIEvent* event)
{
  nsEventStatus status;
  DispatchEvent(event, status);
  return ConvertStatus(status);
}

//-------------------------------------------------------------------------
//
// Dispatch standard event
//
//-------------------------------------------------------------------------

PRBool nsWindow::DispatchStandardEvent(PRUint32 aMsg)
{
  nsGUIEvent event;
  event.eventStructType = NS_GUI_EVENT;
  InitEvent(event, aMsg);

  PRBool result = DispatchWindowEvent(&event);
  NS_RELEASE(event.widget);
  return result;
}

//-------------------------------------------------------------------------
//
// the nsWindow procedure for all nsWindows in this toolkit
//
//-------------------------------------------------------------------------
LRESULT CALLBACK nsWindow::WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{

    // Get the window which caused the event and ask it to process the message
    nsWindow *someWindow = (nsWindow*)::GetWindowLong(hWnd, GWL_USERDATA);

    // Re-direct a tab change message destined for its parent window to the
    // the actual window which generated the event.
    if (msg == WM_NOTIFY) {
      LPNMHDR pnmh = (LPNMHDR) lParam;
      if (pnmh->code == TCN_SELCHANGE) {             
        someWindow = (nsWindow*)::GetWindowLong(pnmh->hwndFrom, GWL_USERDATA); 
      }
    }


    if (nsnull != someWindow) {
        LRESULT retValue;
        if (PR_TRUE == someWindow->ProcessMessage(msg, wParam, lParam, &retValue)) {
            return retValue;
        }
    }

#if defined(STRICT)
    return ::CallWindowProc((WNDPROC)someWindow->GetPrevWindowProc(), hWnd, 
                            msg, wParam, lParam);
#else
    return ::CallWindowProc((FARPROC)someWindow->GetPrevWindowProc(), hWnd, 
                            msg, wParam, lParam);
#endif
}


//WINOLEAPI oleStatus;
//-------------------------------------------------------------------------
//
// Utility method for implementing both Create(nsIWidget ...) and
// Create(nsNativeWidget...)
//-------------------------------------------------------------------------
// This is needed for drag & drop & Clipboard support
BOOL gOLEInited = FALSE;

nsresult nsWindow::StandardWindowCreate(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData,
                      nsNativeWidget aNativeParent)
{
    
    BaseCreate(aParent, aRect, aHandleEventFunction, aContext, 
       aAppShell, aToolkit, aInitData);

      // See if the caller wants to explictly set clip children and clip siblings
    DWORD style = WindowStyle();
    if (nsnull != aInitData) {
      if (aInitData->clipChildren) {
        style |= WS_CLIPCHILDREN;
      } else {
        style &= ~WS_CLIPCHILDREN;
      }
      if (aInitData->clipSiblings) {
        style |= WS_CLIPSIBLINGS;
      }
    }

    // Switch to the "main gui thread" if necessary... This method must
    // be executed on the "gui thread"...
    //
  
    nsToolkit* toolkit = (nsToolkit *)mToolkit;
    if (toolkit) {
    if (!toolkit->IsGuiThread()) {
        DWORD args[7];
        args[0] = (DWORD)aParent;
        args[1] = (DWORD)&aRect;
        args[2] = (DWORD)aHandleEventFunction;
        args[3] = (DWORD)aContext;
        args[4] = (DWORD)aAppShell;
        args[5] = (DWORD)aToolkit;
        args[6] = (DWORD)aInitData;

        if (nsnull != aParent) {
           // nsIWidget parent dispatch
          MethodInfo info(this, nsWindow::CREATE, 7, args);
          toolkit->CallMethod(&info);
           return NS_OK;
        }
        else {
            // Native parent dispatch
          MethodInfo info(this, nsWindow::CREATE_NATIVE, 5, args);
          toolkit->CallMethod(&info);
          return NS_OK;
        }
    }
    }
 
    HWND parent;
    if (nsnull != aParent) { // has a nsIWidget parent
      parent = ((aParent) ? (HWND)aParent->GetNativeData(NS_NATIVE_WINDOW) : nsnull);
    } else { // has a nsNative parent
       parent = (HWND)aNativeParent;
    }

    DWORD extendedStyle = WindowExStyle();
    if (nsnull != aInitData) {
      if (aInitData->mBorderStyle == eBorderStyle_dialog ||
          aInitData->mBorderStyle == eBorderStyle_none) {
        extendedStyle &= ~WS_EX_CLIENTEDGE;
      } else if (aInitData->mBorderStyle == eBorderStyle_3DChildWindow) {
        extendedStyle |= WS_EX_CLIENTEDGE;
      }
    }

    mHas3DBorder = (extendedStyle & WS_EX_CLIENTEDGE) > 0;

    mWnd = ::CreateWindowEx(extendedStyle,
                            WindowClass(),
                            "",
                            style,
                            aRect.x,
                            aRect.y,
                            aRect.width,
                            GetHeight(aRect.height),
                            parent,
                            NULL,
                            nsToolkit::mDllInstance,
                            NULL);
   
    VERIFY(mWnd);

    // Initial Drag & Drop Work
   if (!gOLEInited) {
     DWORD dwVer = ::OleBuildVersion();

     if (FAILED(::OleInitialize(NULL))){
       printf("***** OLE has not been initialized!\n");
     } else {
       //if defined(DEBUG) printf("***** OLE has been initialized!\n");
     }
     gOLEInited = TRUE;
   }
#ifdef NEW_DRAG_AND_DROP
   mNativeDragTarget = new nsNativeDragTarget(this);
   if (NULL != mNativeDragTarget) {
     mNativeDragTarget->AddRef();
     if (S_OK == ::CoLockObjectExternal((LPUNKNOWN)mNativeDragTarget,TRUE,FALSE)) {
       if (S_OK == ::RegisterDragDrop(mWnd, (LPDROPTARGET)mNativeDragTarget)) {
       }
     }
   }
   //mDragDrop = new CfDragDrop();
   //mDragDrop->AddRef();
   //mDragDrop->Initialize(this);

   /*mDragTarget = new CfDropTarget(*mDragDrop);
   mDragTarget->AddRef();

   mDragSource = new CfDropSource(*mDragDrop);
   mDragSource->AddRef();*/

#endif

    // call the event callback to notify about creation

    DispatchStandardEvent(NS_CREATE);
    SubclassWindow(TRUE);
    return(NS_OK);
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
    return(StandardWindowCreate(aParent, aRect, aHandleEventFunction,
                         aContext, aAppShell, aToolkit, aInitData,
                         nsnull));
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
    return(StandardWindowCreate(nsnull, aRect, aHandleEventFunction,
                         aContext, aAppShell, aToolkit, aInitData,
                         aParent));
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
  nsToolkit* toolkit = (nsToolkit *)mToolkit;
  if (toolkit != nsnull && !toolkit->IsGuiThread()) {
    MethodInfo info(this, nsWindow::DESTROY);
    toolkit->CallMethod(&info);
    return NS_ERROR_FAILURE;
  }

  // disconnect from the parent
  if (!mIsDestroying) {
    nsBaseWidget::Destroy();
  }

#ifdef NEW_DRAG_AND_DROP
  if (NULL != mNativeDragTarget) {
    if (S_OK == ::CoLockObjectExternal((LPUNKNOWN)mNativeDragTarget, FALSE, TRUE)) {
    }
    NS_RELEASE(mNativeDragTarget);
  }
#endif

  // destroy the HWND
  if (mWnd) {
    // prevent the widget from causing additional events
    mEventCallback = nsnull;
    VERIFY(::DestroyWindow(mWnd));
    mWnd = NULL;
    //our windows can be subclassed by
    //others and these namless, faceless others
    //may not let us know about WM_DESTROY. so,
    //if OnDestroy() didn't get called, just call
    //it now. MMP
    if (PR_FALSE == mOnDestroyCalled)
      OnDestroy();
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
    nsWindow* widget = NULL;
    if (mWnd) {
        HWND parent = ::GetParent(mWnd);
        if (parent) {
            widget = (nsWindow *)::GetWindowLong(parent, GWL_USERDATA);
            if (widget) {
              // If the widget is in the process of being destroyed then
              // do NOT return it
              if (widget->mIsDestroying) {
                widget = NULL;
              } else {
                NS_ADDREF(widget);
              }
            }
        }
    }

    return (nsIWidget*)widget;
}


//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Show(PRBool bState)
{
    if (mWnd) {
        if (bState) {
            ::SetWindowPos(mWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_SHOWWINDOW);
        }
        else
            ::ShowWindow(mWnd, SW_HIDE);
    }
    mIsVisible = bState;
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Return PR_TRUE if the whether the component is visible, PR_FALSE otherwise
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::IsVisible(PRBool & bState)
{
  bState = mIsVisible;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Move(PRUint32 aX, PRUint32 aY)
{
  mBounds.x = aX;
  mBounds.y = aY;

    if (mWnd) {
        nsIWidget *par = GetParent();
        HDWP      deferrer = NULL;

        if (nsnull != par) {
          deferrer = ((nsWindow *)par)->mDeferredPositioner;
        }

        if (NULL != deferrer) {
            VERIFY(((nsWindow *)par)->mDeferredPositioner = ::DeferWindowPos(deferrer,
                                  mWnd, NULL, aX, aY, 0, 0,
                                  SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE));
        }
        else {
            VERIFY(::SetWindowPos(mWnd, NULL, aX, aY, 0, 0, 
                                  SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOSIZE));
        }

        NS_IF_RELEASE(par);
    }
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Resize(PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
  // Set cached value for lightweight and printing
  mBounds.width  = aWidth;
  mBounds.height = aHeight;

    if (mWnd) {
        nsIWidget *par = GetParent();
        HDWP      deferrer = NULL;

        if (nsnull != par) {
          deferrer = ((nsWindow *)par)->mDeferredPositioner;
        }

        UINT  flags = SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE;
        if (!aRepaint) {
          flags |= SWP_NOREDRAW;
        }

        if (NULL != deferrer) {
            VERIFY(((nsWindow *)par)->mDeferredPositioner = ::DeferWindowPos(deferrer,
                                    mWnd, NULL, 0, 0, aWidth, GetHeight(aHeight), flags));
        }
        else {
            VERIFY(::SetWindowPos(mWnd, NULL, 0, 0, aWidth, GetHeight(aHeight), 
                                  flags));
        }

        NS_IF_RELEASE(par);
    }
    return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Resize(PRUint32 aX,
                      PRUint32 aY,
                      PRUint32 aWidth,
                      PRUint32 aHeight,
                      PRBool   aRepaint)
{
  // Set cached value for lightweight and printing
  mBounds.x      = aX;
  mBounds.y      = aY;
  mBounds.width  = aWidth;
  mBounds.height = aHeight;

    if (mWnd) {
        nsIWidget *par = GetParent();
        HDWP      deferrer = NULL;

        if (nsnull != par) {
          deferrer = ((nsWindow *)par)->mDeferredPositioner;
        }

        UINT  flags = SWP_NOZORDER | SWP_NOACTIVATE;
        if (!aRepaint) {
          flags |= SWP_NOREDRAW;
        }

        if (NULL != deferrer) {
            VERIFY(((nsWindow *)par)->mDeferredPositioner = ::DeferWindowPos(deferrer,
                                    mWnd, NULL, aX, aY, aWidth, GetHeight(aHeight), 
                                    flags));
        }
        else {
            VERIFY(::SetWindowPos(mWnd, NULL, aX, aY, aWidth, GetHeight(aHeight), 
                                  flags));
        }

        NS_IF_RELEASE(par);
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
        ::EnableWindow(mWnd, bState);
    }
    return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Give the focus to this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetFocus(void)
{
    //
    // Switch to the "main gui thread" if necessary... This method must
    // be executed on the "gui thread"...
    //
    nsToolkit* toolkit = (nsToolkit *)mToolkit;
    if (!toolkit->IsGuiThread()) {
        MethodInfo info(this, nsWindow::SET_FOCUS);
        toolkit->CallMethod(&info);
        return NS_ERROR_FAILURE;
    }

    if (mWnd) {
        ::SetFocus(mWnd);
    }
    return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Get this component dimension
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::GetBounds(nsRect &aRect)
{
  if (mWnd) {
    RECT r;
    VERIFY(::GetWindowRect(mWnd, &r));

    // assign size
    aRect.width  = r.right - r.left;
    aRect.height = r.bottom - r.top;

    // convert coordinates if parent exists
    HWND parent = ::GetParent(mWnd);
    if (parent) {
      RECT pr;
      VERIFY(::GetWindowRect(parent, &pr));
      r.left -= pr.left;
      r.top  -= pr.top;
    }
    aRect.x = r.left;
    aRect.y = r.top;
  } else {
    aRect = mBounds;
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

  if (mWnd) {
    RECT r;
    VERIFY(::GetClientRect(mWnd, &r));

    // assign size
    aRect.x = 0;
    aRect.y = 0;
    aRect.width  = r.right - r.left;
    aRect.height = r.bottom - r.top;

  } else {
    aRect.SetRect(0,0,0,0);
  }
  return NS_OK;
}

//get the bounds, but don't take into account the client size

void nsWindow::GetNonClientBounds(nsRect &aRect)
{
  if (mWnd) {
      RECT r;
      VERIFY(::GetWindowRect(mWnd, &r));

      // assign size
      aRect.width = r.right - r.left;
      aRect.height = r.bottom - r.top;

      // convert coordinates if parent exists
      HWND parent = ::GetParent(mWnd);
      if (parent) {
        RECT pr;
        VERIFY(::GetWindowRect(parent, &pr));
        r.left -= pr.left;
        r.top -= pr.top;
      }
      aRect.x = r.left;
      aRect.y = r.top;
  } else {
      aRect.SetRect(0,0,0,0);
  }
}

           
//-------------------------------------------------------------------------
//
// Set the background color
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetBackgroundColor(const nscolor &aColor)
{
    nsBaseWidget::SetBackgroundColor(aColor);
  
    if (mBrush)
      ::DeleteObject(mBrush);

    mBrush = ::CreateSolidBrush(NSRGB_2_COLOREF(mBackground));
    if (mWnd != NULL) {
      SetClassLong(mWnd, GCL_HBRBACKGROUND, (LONG)mBrush);
    }
    return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Get this component font
//
//-------------------------------------------------------------------------
nsIFontMetrics* nsWindow::GetFont(void)
{
    NS_NOTYETIMPLEMENTED("GetFont not yet implemented"); // to be implemented
    return NULL;
}

    
//-------------------------------------------------------------------------
//
// Set this component font
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetFont(const nsFont &aFont)
{
  // Cache Font for owner draw
  if (mFont == nsnull) {
    mFont = new nsFont(aFont);
  } else {
    *mFont  = aFont;
  }
  
  // Bail out if there is no context
  if (nsnull == mContext) {
    return NS_ERROR_FAILURE;
  }

  nsIFontMetrics* metrics;
  mContext->GetMetricsFor(aFont, metrics);
  nsFontHandle  fontHandle;
  metrics->GetFontHandle(fontHandle);
  HFONT hfont = (HFONT)fontHandle;

    // Draw in the new font
  ::SendMessage(mWnd, WM_SETFONT, (WPARAM)hfont, (LPARAM)0); 
  NS_RELEASE(metrics);

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
    HCURSOR newCursor = NULL;

    switch(aCursor) {
    case eCursor_select:
      newCursor = ::LoadCursor(NULL, IDC_IBEAM);
      break;
      
    case eCursor_wait:
      newCursor = ::LoadCursor(NULL, IDC_WAIT);
      break;

    case eCursor_hyperlink: {
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_SELECTANCHOR));
      break;
    }

    case eCursor_standard:
      newCursor = ::LoadCursor(NULL, IDC_ARROW);
      break;

    case eCursor_sizeWE:
      newCursor = ::LoadCursor(NULL, IDC_SIZEWE);
      break;

    case eCursor_sizeNS:
      newCursor = ::LoadCursor(NULL, IDC_SIZENS);
      break;

    case eCursor_arrow_north:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWNORTH));
      break;

    case eCursor_arrow_north_plus:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWNORTHPLUS));
      break;

    case eCursor_arrow_south:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWSOUTH));
      break;

    case eCursor_arrow_south_plus:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWSOUTHPLUS));
      break;

    case eCursor_arrow_east:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWEAST));
      break;

    case eCursor_arrow_east_plus:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWEASTPLUS));
      break;

    case eCursor_arrow_west:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWWEST));
      break;

    case eCursor_arrow_west_plus:
      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWWESTPLUS));
      break;

    default:
      NS_ASSERTION(0, "Invalid cursor type");
      break;
    }

    if (NULL != newCursor) {
      mCursor = aCursor;
      HCURSOR oldCursor = ::SetCursor(newCursor);
    }
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
    if (mWnd) {
        VERIFY(::InvalidateRect(mWnd, NULL, TRUE));
        if (aIsSynchronous) {
          VERIFY(::UpdateWindow(mWnd));
        }
    }
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
  RECT rect;

  if (mWnd) {
    rect.left   = aRect.x;
    rect.top    = aRect.y;
    rect.right  = aRect.x + aRect.width;
    rect.bottom = aRect.y  + aRect.height;
    VERIFY(::InvalidateRect(mWnd, &rect, TRUE));
    if (aIsSynchronous) {
      VERIFY(::UpdateWindow(mWnd));
    }
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Force a synchronous repaint of the window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Update()
{
  // updates can come through for windows no longer holding an mWnd during
  // deletes triggered by JavaScript in buttons with mouse feedback
  if (mWnd)
    VERIFY(::UpdateWindow(mWnd));
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void* nsWindow::GetNativeData(PRUint32 aDataType)
{
    switch(aDataType) {
        case NS_NATIVE_WIDGET:
        case NS_NATIVE_WINDOW:
        case NS_NATIVE_PLUGIN_PORT:
            return (void*)mWnd;
        case NS_NATIVE_GRAPHIC:
            // XXX:  This is sleezy!!  Remember to Release the DC after using it!
            return (void*)::GetDC(mWnd);
        case NS_NATIVE_COLORMAP:
        default:
            break;
    }

    return NULL;
}

//-------------------------------------------------------------------------
//
// Set the colormap of the window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetColorMap(nsColorMap *aColorMap)
{
    if (mPalette != NULL) {
        ::DeleteObject(mPalette);
    }

    PRUint8 *map = aColorMap->Index;
    LPLOGPALETTE pLogPal = (LPLOGPALETTE) new char[2 * sizeof(WORD) +
                                              aColorMap->NumColors * sizeof(PALETTEENTRY)];
	pLogPal->palVersion = 0x300;
	pLogPal->palNumEntries = aColorMap->NumColors;
	for(int i = 0; i < aColorMap->NumColors; i++) 
    {
		pLogPal->palPalEntry[i].peRed = *map++;
		pLogPal->palPalEntry[i].peGreen = *map++;
		pLogPal->palPalEntry[i].peBlue = *map++;
		pLogPal->palPalEntry[i].peFlags = 0;
	}
	mPalette = ::CreatePalette(pLogPal);
	delete pLogPal;

    NS_ASSERTION(mPalette != NULL, "Null palette");
    if (mPalette != NULL) {
        HDC hDC = ::GetDC(mWnd);
        HPALETTE hOldPalette = ::SelectPalette(hDC, mPalette, TRUE);
        ::RealizePalette(hDC);
        ::SelectPalette(hDC, hOldPalette, TRUE);
        ::ReleaseDC(mWnd, hDC);
    }
    return NS_OK;
}


//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
  RECT  trect;

  if (nsnull != aClipRect)
  {
    trect.left = aClipRect->x;
    trect.top = aClipRect->y;
    trect.right = aClipRect->XMost();
    trect.bottom = aClipRect->YMost();
  }

  ::ScrollWindowEx(mWnd, aDx, aDy, (nsnull != aClipRect) ? &trect : NULL, NULL,
                   NULL, NULL, SW_INVALIDATE | SW_SCROLLCHILDREN);
  ::UpdateWindow(mWnd);
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
            NS_ASSERTION(info->nArgs == 7, "Wrong number of arguments to CallMethod");
            Create((nsIWidget*)(info->args[0]), 
                        (nsRect&)*(nsRect*)(info->args[1]), 
                        (EVENT_CALLBACK)(info->args[2]), 
                        (nsIDeviceContext*)(info->args[3]),
                        (nsIAppShell *)(info->args[4]),
                        (nsIToolkit*)(info->args[5]),
                        (nsWidgetInitData*)(info->args[6]));
            break;

        case nsWindow::CREATE_NATIVE:
            NS_ASSERTION(info->nArgs == 7, "Wrong number of arguments to CallMethod");
            Create((nsNativeWidget)(info->args[0]), 
                        (nsRect&)*(nsRect*)(info->args[1]), 
                        (EVENT_CALLBACK)(info->args[2]), 
                        (nsIDeviceContext*)(info->args[3]),
                        (nsIAppShell *)(info->args[4]),
                        (nsIToolkit*)(info->args[5]),
                        (nsWidgetInitData*)(info->args[6]));
            return TRUE;

        case nsWindow::DESTROY:
            NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
            Destroy();
            break;

        case nsWindow::SET_FOCUS:
            NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
            SetFocus();
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

PRBool nsWindow::OnKey(PRUint32 aEventType, PRBool aCalcCharCode, TCHAR aCharCode, UINT nChar, UINT nRepCnt, UINT nFlags)
{
  if (nChar == NS_VK_CAPS_LOCK ||
      nChar == NS_VK_ALT ||
      nChar == NS_VK_SHIFT ||
      nChar == NS_VK_CONTROL) {
    return FALSE;
  }

  nsKeyEvent event;
  nsPoint point;

  point.x = 0;
  point.y = 0;

  InitEvent(event, aEventType, &point);

  if (aCalcCharCode) {
    // Now let windows do the conversion to the ascii code
    WORD asciiChar = 0;
    BYTE kbstate[256];
    ::GetKeyboardState(kbstate);
    ToAscii(nChar, nFlags & 0xff, kbstate, &asciiChar, 0);
    event.charCode = (char)asciiChar;
  } else {
    event.charCode = aCharCode;
  }

  event.keyCode = nChar;
  //printf("charCode %d  keyCode %d\n",  event.charCode, event.keyCode);

  event.isShift   = mIsShiftDown;
  event.isControl = mIsControlDown;
  event.isAlt     = mIsAltDown;
  event.eventStructType = NS_KEY_EVENT;

  PRBool result = DispatchWindowEvent(&event);
  NS_RELEASE(event.widget);
  return result;
}

//-------------------------------------------------------------------------
void nsWindow::SetUpForPaint(HDC aHDC) 
{
  ::SetBkColor (aHDC, NSRGB_2_COLOREF(mBackground));
  ::SetTextColor(aHDC, NSRGB_2_COLOREF(mForeground));
  ::SetBkMode (aHDC, TRANSPARENT);
}

//-------------------------------------------------------------------------
nsIMenuItem * nsWindow::FindMenuItem(nsIMenu * aMenu, PRUint32 aId)
{
  PRUint32 i, count;
  aMenu->GetItemCount(count);
  for (i=0;i<count;i++) {
    nsISupports * item;
    nsIMenuItem * menuItem;
    nsIMenu     * menu;

    aMenu->GetItemAt(i, item);
    if (NS_OK == item->QueryInterface(kIMenuItemIID, (void **)&menuItem)) {
      if (((nsMenuItem *)menuItem)->GetCmdId() == (PRInt32)aId) {
        NS_RELEASE(item);
        return menuItem;
      }
    } else if (NS_OK == item->QueryInterface(kIMenuIID, (void **)&menu)) {
      nsIMenuItem * fndItem = FindMenuItem(menu, aId);
      NS_RELEASE(menu);
      if (nsnull != fndItem) {
        NS_RELEASE(item);
        return fndItem;
      }
    }
    NS_RELEASE(item);
  }
  return nsnull;
}

//-------------------------------------------------------------------------
static nsIMenuItem * FindMenuChild(nsIMenu * aMenu, PRInt32 aId)
{
  PRUint32 i, count;
  aMenu->GetItemCount(count);
  for (i=0;i<count;i++) {
    nsISupports * item;
    aMenu->GetItemAt(i, item);
    nsIMenuItem * menuItem;
    if (NS_OK == item->QueryInterface(kIMenuItemIID, (void **)&menuItem)) {
      if (((nsMenuItem *)menuItem)->GetCmdId() == (PRInt32)aId) {
        NS_RELEASE(item);
        return menuItem;
      }
    }
    NS_RELEASE(item);
  }
  return nsnull;
}


//-------------------------------------------------------------------------
nsIMenu * nsWindow::FindMenu(nsIMenu * aMenu, HMENU aNativeMenu, PRInt32 &aDepth)
{
  if (aNativeMenu == ((nsMenu *)aMenu)->GetNativeMenu()) {
    NS_ADDREF(aMenu);
    return aMenu;
  }

  aDepth++;
  PRUint32 i, count;
  aMenu->GetItemCount(count);
  for (i=0;i<count;i++) {
    nsISupports * item;
    aMenu->GetItemAt(i, item);
    nsIMenu * menu;
    if (NS_OK == item->QueryInterface(kIMenuIID, (void **)&menu)) {
      HMENU nativeMenu = ((nsMenu *)menu)->GetNativeMenu();
      if (nativeMenu == aNativeMenu) {
        return menu;
      } else {
        nsIMenu * fndMenu = FindMenu(menu, aNativeMenu, aDepth);
        if (fndMenu) {
          NS_RELEASE(item);
          NS_RELEASE(menu);
          return fndMenu;
        }
      }
      NS_RELEASE(menu);
    }
    NS_RELEASE(item);
  }
  return nsnull;
}

//-------------------------------------------------------------------------
static void AdjustMenus(nsIMenu * aCurrentMenu, nsIMenu * aNewMenu, nsMenuEvent & aEvent) 
{
  if (nsnull != aCurrentMenu) {
    nsIMenuListener * listener;
    if (NS_OK == aCurrentMenu->QueryInterface(kIMenuListenerIID, (void **)&listener)) {
      //listener->MenuDeselected(aEvent);
	  //listener->MenuDestruct(aEvent);

      NS_RELEASE(listener);

    }

  }



  if (nsnull != aNewMenu)  {

    nsIMenuListener * listener;

    if (NS_OK == aNewMenu->QueryInterface(kIMenuListenerIID, (void **)&listener)) {

		NS_ASSERTION(false, "get debugger");

      //listener->MenuSelected(aEvent);

	  //listener->MenuConstruct(aEvent, this, null);

      NS_RELEASE(listener);

    }

  }

}



//-------------------------------------------------------------------------

nsresult nsWindow::MenuHasBeenSelected(
  HMENU aNativeMenu, 
  UINT  aItemNum, 
  UINT  aFlags, 
  UINT  aCommand)
{

  // Build nsMenuEvent
  nsMenuEvent event;
  event.mCommand = aCommand;
  event.eventStructType = NS_MENU_EVENT;
  InitEvent(event, NS_MENU_SELECTED);

  // The MF_POPUP flag tells us if we are a menu item or a menu
  // the aItemNum is either the command ID of the menu item or 
  // the position of the menu as a child of its parent

  PRBool isMenuItem = !(aFlags & MF_POPUP);

  // uItem is the position of the item that was clicked
  // aNativeMenu is a handle to the menu that was clicked

  // if aNativeMenu is NULL then the menu is being deselected
  if (!aNativeMenu) {
    printf("///////////// Menu is NULL!\n");
    // check to make sure something had been selected
    //AdjustMenus(mHitMenu, nsnull, event);

	nsIMenu * aNewMenu = nsnull;

	nsMenuEvent aEvent = event;

//static void AdjustMenus(nsIMenu * aCurrentMenu, nsIMenu * aNewMenu, nsMenuEvent & aEvent) 

{

  if (nsnull != mHitMenu) {

    nsIMenuListener * listener;

    if (NS_OK == mHitMenu->QueryInterface(kIMenuListenerIID, (void **)&listener)) {

		//NS_ASSERTION(false, "get debugger");

      //listener->MenuDeselected(aEvent);

	  //listener->MenuDestruct(aEvent);

      NS_RELEASE(listener);

    }

  }



  if (nsnull != aNewMenu)  {

    nsIMenuListener * listener;

    if (NS_OK == aNewMenu->QueryInterface(kIMenuListenerIID, (void **)&listener)) {

      //listener->MenuSelected(aEvent);

	  //listener->MenuConstruct(aEvent, this, null);

      NS_RELEASE(listener);

    }

  }

}

    NS_IF_RELEASE(mHitMenu);
    // Clear All SubMenu items
    while (mHitSubMenus->Count() > 0) {
      PRUint32 inx = mHitSubMenus->Count()-1;
      nsIMenu * menu = (nsIMenu *)mHitSubMenus->ElementAt(inx);
      //AdjustMenus(menu, nsnull, event);

	  nsIMenu * aCurrentMenu = menu;

	  nsIMenu * aNewMenu = nsnull;

//static void AdjustMenus(nsIMenu * aCurrentMenu, nsIMenu * aNewMenu, nsMenuEvent & aEvent) 

{

  if (nsnull != aCurrentMenu) {

    nsIMenuListener * listener;

    if (NS_OK == aCurrentMenu->QueryInterface(kIMenuListenerIID, (void **)&listener)) {

		//NS_ASSERTION(false, "get debugger");

      //listener->MenuDeselected(event);

	  //listener->MenuDestruct(event);

      NS_RELEASE(listener);

    }

  }



  if (nsnull != aNewMenu)  {

    nsIMenuListener * listener;

    if (NS_OK == aNewMenu->QueryInterface(kIMenuListenerIID, (void **)&listener)) {

      //listener->MenuSelected(event);

	  //listener->MenuConstruct(event, this, null);

      NS_RELEASE(listener);

    }

  }

}

      NS_RELEASE(menu);
      mHitSubMenus->RemoveElementAt(inx);
    }
    return NS_OK;
  } else { // The menu is being selected
    void * voidData;
    mMenuBar->GetNativeData(voidData);
    HMENU nativeMenuBar = (HMENU)voidData;

    // first check to see if it is a member of the menubar
    nsIMenu * hitMenu = nsnull;
    if (aNativeMenu == nativeMenuBar) {
      mMenuBar->GetMenuAt(aItemNum, hitMenu);
      if (mHitMenu != hitMenu) {
		  //mHitMenu, hitMenu, event

//AdjustMenus(nsIMenu * aCurrentMenu, nsIMenu * aNewMenu, nsMenuEvent & aEvent) 
{

  if (nsnull != mHitMenu) {

    nsIMenuListener * listener;

    if (NS_OK == mHitMenu->QueryInterface(kIMenuListenerIID, (void **)&listener)) {

		//NS_ASSERTION(false, "get debugger");

      //listener->MenuDeselected(aEvent);

	  listener->MenuDestruct(event);
      NS_RELEASE(listener);

    }

  }

  if (nsnull != hitMenu)  {
    nsIMenuListener * listener;

    if (NS_OK == hitMenu->QueryInterface(kIMenuListenerIID, (void **)&listener)) {
      //listener->MenuSelected(aEvent);

	  listener->MenuConstruct(event, this, NULL, NULL);
      NS_RELEASE(listener);

    }

  }

}

        NS_IF_RELEASE(mHitMenu);
        mHitMenu = hitMenu;
      } else {
        NS_IF_RELEASE(hitMenu);
      }
    } else {
      // At this point we know we are inside a menu

      // Find the menu we are in (the parent menu)
      nsIMenu * parentMenu = nsnull;
      PRInt32 fndDepth = 0;
      PRUint32 i, count;
      mMenuBar->GetMenuCount(count);
      for (i=0;i<count;i++) {
        nsIMenu * menu;
        mMenuBar->GetMenuAt(i, menu);
        PRInt32 depth = 0;
        parentMenu = FindMenu(menu, aNativeMenu, depth);
        if (parentMenu) {
          fndDepth = depth;
          break;
        }
        NS_RELEASE(menu);
      }

      if (nsnull != parentMenu) {

        // Sometimes an event comes through for a menu that is being popup down
        // So it its depth is great then the current hit list count it already gone.
        if (fndDepth > mHitSubMenus->Count()) {
          NS_RELEASE(parentMenu);
          return NS_OK;
        }

        nsIMenu * newMenu  = nsnull;

        // Skip if it is a menu item, otherwise, we get the menu by position
        if (!isMenuItem) {
          printf("Getting submenu by position %d from parentMenu\n", aItemNum);
          nsISupports * item;
          parentMenu->GetItemAt((PRUint32)aItemNum, item);
          if (NS_OK != item->QueryInterface(kIMenuIID, (void **)&newMenu)) {
            printf("Item was not a menu! What are we doing here? Return early....\n");
            return NS_ERROR_FAILURE;
          }
        }

        // Figure out if this new menu is in the list of popup'ed menus
        PRBool newFound = PR_FALSE;
        PRInt32 newLevel = 0;
        for (newLevel=0;newLevel<mHitSubMenus->Count();newLevel++) {
          if (newMenu == (nsIMenu *)mHitSubMenus->ElementAt(newLevel)) {
            newFound = PR_TRUE;
            break;
          }
        }

        // Figure out if the parent menu is in the list of popup'ed menus
        PRBool found = PR_FALSE;
        PRInt32 level = 0;
        for (level=0;level<mHitSubMenus->Count();level++) {
          if (parentMenu == (nsIMenu *)mHitSubMenus->ElementAt(level)) {
            found = PR_TRUE;
            break;
          }
        }

        // So now figure out were we are compared to the hit list depth
        // we figure out how many items are open below
        //
        // If the parent was found then we use it
        // if the parent was NOT found this means we are at the very first level (menu from the menubar)
        // Windows will send an event for a parent AND child that is already in the hit list
        // and we think we should be popping it down. So we check to see if the 
        // new menu is already in the tree so it doesn't get removed and then added.
        PRInt32 numToRemove = 0;
        if (found) {
          numToRemove = mHitSubMenus->Count() - level - 1;
        } else {
          // This means we got a menu event for a menubar menu
          if (newFound) { // newFound checks to see if the new menu to be added is already in the hit list
            numToRemove = mHitSubMenus->Count() - newLevel - 1;
          } else {
            numToRemove = mHitSubMenus->Count();
          }
        }

        // If we are to remove 1 item && the new menu to be added is the 
        // same as the one we would be removing, then don't remove it.
        if (numToRemove == 1 && newMenu == (nsIMenu *)mHitSubMenus->ElementAt(mHitSubMenus->Count()-1)) {
          numToRemove = 0;
        }

        // Now loop thru and removing the menu from thre list
        PRInt32 ii;
        for (ii=0;ii<numToRemove;ii++) {
          nsIMenu * m = (nsIMenu *)mHitSubMenus->ElementAt(mHitSubMenus->Count()-1 );
          //AdjustMenus(m, nsnull, event);

		  nsIMenu * aCurrentMenu = m;

		  nsIMenu * aNewMenu = nsnull;

		  nsMenuEvent aEvent = event;

//static void AdjustMenus(nsIMenu * aCurrentMenu, nsIMenu * aNewMenu, nsMenuEvent & aEvent) 

{

  if (nsnull != aCurrentMenu) {

    nsIMenuListener * listener;

    if (NS_OK == aCurrentMenu->QueryInterface(kIMenuListenerIID, (void **)&listener)) {

      //listener->MenuDeselected(aEvent);

	  //listener->MenuDestruct(aEvent);

      NS_RELEASE(listener);

    }

  }



  if (nsnull != aNewMenu)  {

    nsIMenuListener * listener;

    if (NS_OK == aNewMenu->QueryInterface(kIMenuListenerIID, (void **)&listener)) {

		NS_ASSERTION(false, "get debugger");

      //listener->MenuSelected(aEvent);

	  //listener->MenuConstruct(aEvent, this, null);

      NS_RELEASE(listener);

    }

  }

}



          nsString name;
          m->GetLabel(name);
          NS_RELEASE(m);
          mHitSubMenus->RemoveElementAt(mHitSubMenus->Count()-1);
        }
 
        // At this point we bail if we are a menu item
        if (isMenuItem) {
          return NS_OK;
        }

        // Here we know we have a menu, check one last time to see 
        // if the new one is the last one in the list
        // Add it if it isn't or skip adding it
        nsString name;
        newMenu->GetLabel(name);
        if (newMenu != (nsIMenu *)mHitSubMenus->ElementAt(mHitSubMenus->Count()-1)) {
          mHitSubMenus->AppendElement(newMenu);
          NS_ADDREF(newMenu);
          //AdjustMenus(nsnull, newMenu, event);

		  nsIMenu * aCurrentMenu = nsnull;

		  nsIMenu * aNewMenu = newMenu;

		  nsMenuEvent aEvent = event;

//static void AdjustMenus(nsIMenu * aCurrentMenu, nsIMenu * aNewMenu, nsMenuEvent & aEvent) 

{

  if (nsnull != aCurrentMenu) {

    nsIMenuListener * listener;

    if (NS_OK == aCurrentMenu->QueryInterface(kIMenuListenerIID, (void **)&listener)) {

      //listener->MenuDeselected(aEvent);

	  //listener->MenuDestruct(aEvent);

      NS_RELEASE(listener);

    }

  }



  if (nsnull != aNewMenu)  {

    nsIMenuListener * listener;

    if (NS_OK == aNewMenu->QueryInterface(kIMenuListenerIID, (void **)&listener)) {

	  //NS_ASSERTION(false, "get debugger");

      //listener->MenuSelected(aEvent);

	  //nsIDOMNode * node;

      //mMenuBar->GetDOMNode(&node);

	  listener->MenuConstruct(aEvent, this, nsnull, nsnull);

      NS_RELEASE(listener);

    }

  }

}

        }

        NS_RELEASE(parentMenu);
      } else {
        printf("no menu was found. This is bad.\n");
        // XXX need to assert here!
      }
    }
  }  
  return NS_OK;
}
//---------------------------------------------------------
NS_METHOD nsWindow::EnableFileDrop(PRBool aEnable)
{
  //::DragAcceptFiles(mWnd, (aEnable?TRUE:FALSE));
#ifdef NEW_DRAG_AND_DROP
   mNativeDragTarget = new nsNativeDragTarget(this);
   if (NULL != mNativeDragTarget) {
     mNativeDragTarget->AddRef();
     if (S_OK == ::CoLockObjectExternal((LPUNKNOWN)mNativeDragTarget,TRUE,FALSE)) {
       if (S_OK == ::RegisterDragDrop(mWnd, (LPDROPTARGET)mNativeDragTarget)) {
       }
     }
   }
#endif
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Process all nsWindows messages
//
//-------------------------------------------------------------------------
PRBool nsWindow::ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *aRetValue)
{
    static UINT vkKeyCached = 0;                // caches VK code fon WM_KEYDOWN
    static BOOL firstTime = TRUE;                // for mouse wheel logic
    static int  iDeltaPerLine, iAccumDelta ;     // for mouse wheel logic
    ULONG       ulScrollLines ;                  // for mouse wheel logic

    PRBool        result = PR_FALSE; // call the default nsWindow proc
    nsPaletteInfo palInfo;
    *aRetValue = 0;

    switch (msg) {

        case WM_COMMAND: {
          WORD wNotifyCode = HIWORD(wParam); // notification code 
          if (wNotifyCode == 0) { // Menu selection
            nsMenuEvent event;
            event.mCommand = LOWORD(wParam);
            event.eventStructType = NS_MENU_EVENT;
            InitEvent(event, NS_MENU_SELECTED);
            result = DispatchWindowEvent(&event);

			printf("WM_COMMAND \n");

            if (mMenuBar) {
              PRUint32 i, count;
              mMenuBar->GetMenuCount(count);
              for (i=0;i<count;i++) {
                nsIMenu * menu;
                mMenuBar->GetMenuAt(i, menu);
                nsIMenuItem * menuItem = FindMenuItem(menu, event.mCommand);
                if (menuItem) {
                  nsIMenuListener * listener;
                  if (NS_OK == menuItem->QueryInterface(kIMenuListenerIID, (void **)&listener)) {
                    listener->MenuSelected(event);
                    NS_RELEASE(listener);

					menu->QueryInterface(kIMenuListenerIID, (void **)&listener);
					if(listener){
					  listener->MenuDestruct(event);
					  NS_RELEASE(listener);
					}
                  }
                  NS_RELEASE(menuItem);
                }
                NS_RELEASE(menu);
              }
            }
            NS_RELEASE(event.widget);
          }
        }
        break;

        
        case WM_NOTIFY:
            // TAB change
          {
            LPNMHDR pnmh = (LPNMHDR) lParam;

            switch (pnmh->code) {
              case TCN_SELCHANGE: {
                DispatchStandardEvent(NS_TABCHANGE);
                result = PR_TRUE;
              }
              break;

              case TTN_SHOW: {
                  nsTooltipEvent event;
                  InitEvent(event, NS_SHOW_TOOLTIP);
                  event.tipIndex = (PRUint32)wParam;
                  event.eventStructType = NS_TOOLTIP_EVENT;
                  result = DispatchWindowEvent(&event);
                  NS_RELEASE(event.widget);
              }
              break;

              case TTN_POP:
                result = DispatchStandardEvent(NS_HIDE_TOOLTIP);
                break;
            }
          }
          break;

        case WM_MOVE: // Window moved 
          {
            PRInt32 x = (PRInt32)LOWORD(lParam); // horizontal position in screen coordinates 
            PRInt32 y = (PRInt32)HIWORD(lParam); // vertical position in screen coordinates 
            result = OnMove(x, y); 
          }
          break;

        case WM_DESTROY:
            // clean up.
            OnDestroy();
            result = PR_TRUE;
            break;

        case WM_PAINT:
            result = OnPaint();
            break;

        case WM_CHAR:
          printf("Doing WM_CHAR\n");
            mIsShiftDown   = IS_VK_DOWN(NS_VK_SHIFT);
            mIsControlDown = IS_VK_DOWN(NS_VK_CONTROL);
            mIsAltDown     = IS_VK_DOWN(NS_VK_ALT);
            
			      if (!mIMEIsComposing)
				      result = OnKey(NS_KEY_DOWN, FALSE, wParam, vkKeyCached, LOWORD(lParam), HIWORD(lParam));
			      else
				      result = PR_FALSE;
          break;

        case WM_KEYUP: 
            mIsShiftDown   = IS_VK_DOWN(NS_VK_SHIFT);
            mIsControlDown = IS_VK_DOWN(NS_VK_CONTROL);
            mIsAltDown     = IS_VK_DOWN(NS_VK_ALT);
            
			      if (!mIMEIsComposing)
				      result = OnKey(NS_KEY_UP, TRUE, 0, wParam, LOWORD(lParam), HIWORD(lParam));
			      else
				      result = PR_FALSE;
            break;

        case WM_KEYDOWN: 
            {
            // Cache the key code when it is regular chars
            vkKeyCached = (UINT)wParam;

            // Tab and return keys do not generate WM_CHAR events
            if (vkKeyCached == 9 || 
                vkKeyCached == 13) {
              mIsShiftDown   = IS_VK_DOWN(NS_VK_SHIFT);
              mIsControlDown = IS_VK_DOWN(NS_VK_CONTROL);
              mIsAltDown     = IS_VK_DOWN(NS_VK_ALT);
			        if (!mIMEIsComposing)
				        result = OnKey(NS_KEY_DOWN, TRUE, 0, wParam, LOWORD(lParam), HIWORD(lParam));
			        else
				        result = PR_FALSE;
            } else {
               //these are handled by WM_CHAR
  				    result = PR_FALSE;
            }
            }
            break;

        // say we've dealt with erase background if widget does
        // not need auto-erasing
        case WM_ERASEBKGND: 
            if (! AutoErase()) {
              *aRetValue = 1;
              result = PR_TRUE;
            } 
            break;

        case WM_MOUSEMOVE:
            //RelayMouseEvent(msg,wParam, lParam); 
            result = DispatchMouseEvent(NS_MOUSE_MOVE);
            break;

        case WM_LBUTTONDOWN:
            SetFocus();
            //RelayMouseEvent(msg,wParam, lParam); 
            result = DispatchMouseEvent(NS_MOUSE_LEFT_BUTTON_DOWN);
            break;

        case WM_LBUTTONUP:
            //RelayMouseEvent(msg,wParam, lParam); 
            result = DispatchMouseEvent(NS_MOUSE_LEFT_BUTTON_UP);
            break;

        case WM_LBUTTONDBLCLK:
            result = DispatchMouseEvent(NS_MOUSE_LEFT_BUTTON_DOWN);
            if (result == PR_FALSE)
              result = DispatchMouseEvent(NS_MOUSE_LEFT_DOUBLECLICK);
            break;

        case WM_MBUTTONDOWN:
            result = DispatchMouseEvent(NS_MOUSE_MIDDLE_BUTTON_DOWN);
            break;

        case WM_MBUTTONUP:
            result = DispatchMouseEvent(NS_MOUSE_MIDDLE_BUTTON_UP);
            break;

        case WM_MBUTTONDBLCLK:
            result = DispatchMouseEvent(NS_MOUSE_MIDDLE_BUTTON_DOWN);           
            break;

        case WM_RBUTTONDOWN:
            result = DispatchMouseEvent(NS_MOUSE_RIGHT_BUTTON_DOWN);            
            break;

        case WM_RBUTTONUP:
            result = DispatchMouseEvent(NS_MOUSE_RIGHT_BUTTON_UP);
            break;

        case WM_RBUTTONDBLCLK:
            result = DispatchMouseEvent(NS_MOUSE_RIGHT_BUTTON_DOWN);
            if (result == PR_FALSE)
              result = DispatchMouseEvent(NS_MOUSE_RIGHT_DOUBLECLICK);                      
            break;

        case WM_HSCROLL:
        case WM_VSCROLL: 
	          // check for the incoming nsWindow handle to be null in which case
	          // we assume the message is coming from a horizontal scrollbar inside
	          // a listbox and we don't bother processing it (well, we don't have to)
	          if (lParam) {
                nsWindow* scrollbar = (nsWindow*)::GetWindowLong((HWND)lParam, GWL_USERDATA);

		            if (scrollbar) {
		                result = scrollbar->OnScroll(LOWORD(wParam), (short)HIWORD(wParam));
		            }
	          }
            break;

        case WM_CTLCOLORLISTBOX:
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORBTN:
        //case WM_CTLCOLORSCROLLBAR: //XXX causes a the scrollbar to be drawn incorrectly
        case WM_CTLCOLORSTATIC:
	          if (lParam) {
              nsWindow* control = (nsWindow*)::GetWindowLong((HWND)lParam, GWL_USERDATA);
		          if (control) {
                control->SetUpForPaint((HDC)wParam);
		            *aRetValue = (LPARAM)control->OnControlColor();
              }
	          }
    
            result = PR_TRUE;
            break;

        case WM_SETFOCUS:
            result = DispatchFocus(NS_GOTFOCUS);
            break;

        case WM_KILLFOCUS:
            result = DispatchFocus(NS_LOSTFOCUS);
            break;

        case WM_WINDOWPOSCHANGED: 
        {
            WINDOWPOS *wp = (LPWINDOWPOS)lParam;

            // We only care about a resize, so filter out things like z-order
            // changes. Note: there's a WM_MOVE handler above which is why we're
            // not handling them here...
            if (0 == (wp->flags & SWP_NOSIZE)) {
              // XXX Why are we using the client size area? If the size notification
              // is for the client area then the origin should be (0,0) and not
              // the window origin in screen coordinates...
              RECT r;
              ::GetWindowRect(mWnd, &r);
              PRInt32 newWidth, newHeight;
              newWidth = PRInt32(r.right - r.left);
              newHeight = PRInt32(r.bottom - r.top);
              nsRect rect(wp->x, wp->y, newWidth, newHeight);
              //if (newWidth != mBounds.width)
              {
                RECT drect;

                //getting wider

                drect.left = wp->x + mBounds.width;
                drect.top = wp->y;
                drect.right = drect.left + (newWidth - mBounds.width);
                drect.bottom = drect.top + newHeight;

//                ::InvalidateRect(mWnd, NULL, FALSE);
//                ::InvalidateRect(mWnd, &drect, FALSE);
                ::RedrawWindow(mWnd, &drect, NULL,
                               RDW_INVALIDATE | RDW_NOERASE | RDW_NOINTERNALPAINT | RDW_ERASENOW | RDW_ALLCHILDREN);
              }
              //if (newHeight != mBounds.height)
              {
                RECT drect;

                //getting taller

                drect.left = wp->x;
                drect.top = wp->y + mBounds.height;
                drect.right = drect.left + newWidth;
                drect.bottom = drect.top + (newHeight - mBounds.height);

//                ::InvalidateRect(mWnd, NULL, FALSE);
//                ::InvalidateRect(mWnd, &drect, FALSE);
                ::RedrawWindow(mWnd, &drect, NULL,
                               RDW_INVALIDATE | RDW_NOERASE | RDW_NOINTERNALPAINT | RDW_ERASENOW | RDW_ALLCHILDREN);
              }
              mBounds.width  = newWidth;
              mBounds.height = newHeight;
              ///nsRect rect(wp->x, wp->y, wp->cx, wp->cy);

              // recalculate the width and height
              // this time based on the client area
              if (::GetClientRect(mWnd, &r)) {
                rect.width  = PRInt32(r.right - r.left);
                rect.height = PRInt32(r.bottom - r.top);
              }
              result = OnResize(rect);
            }
            break;
        }
//#if 0 // these are needed for now

        case WM_INITMENU: {
          printf("WM_INITMENU\n");
          } break;

        case WM_INITMENUPOPUP: {
          printf("WM_INITMENUPOPUP\n");
          } break;
//#endif


        case WM_MENUSELECT: 
			printf("WM_MENUSELECT\n");
          if (mMenuBar) {
            MenuHasBeenSelected((HMENU)lParam, (UINT)LOWORD(wParam), (UINT)HIWORD(wParam), (UINT) LOWORD(wParam));
          }
          break;

        case WM_SETTINGCHANGE:
          firstTime = TRUE;
          // Fall through
        case WM_MOUSEWHEEL: {
         if (firstTime) {
           firstTime = FALSE;
            //printf("^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^ WM_SETTINGCHANGE\n");
            SystemParametersInfo (104, 0, &ulScrollLines, 0) ;
            //SystemParametersInfo (SPI_GETWHEELSCROLLLINES, 0, &ulScrollLines, 0) ;
          
            // ulScrollLines usually equals 3 or 0 (for no scrolling)
            // WHEEL_DELTA equals 120, so iDeltaPerLine will be 40

            if (ulScrollLines)
              iDeltaPerLine = WHEEL_DELTA / ulScrollLines ;
            else
              iDeltaPerLine = 0 ;
            //printf("ulScrollLines %d  iDeltaPerLine %d\n", ulScrollLines, iDeltaPerLine);

            if (msg == WM_SETTINGCHANGE) {
              return 0;
            }
         }
         HWND scrollbar = NULL;
         if (nsnull != mVScrollbar) {
           scrollbar = (HWND)mVScrollbar->GetNativeData(NS_NATIVE_WINDOW);
         }
         if (scrollbar) {
            if (iDeltaPerLine == 0)
              break ;

            iAccumDelta += (short) HIWORD (wParam) ;     // 120 or -120

            while (iAccumDelta >= iDeltaPerLine) {    
              //printf("iAccumDelta %d\n", iAccumDelta);
              SendMessage (mWnd, WM_VSCROLL, SB_LINEUP, (LONG)scrollbar) ;
              iAccumDelta -= iDeltaPerLine ;
            }

            while (iAccumDelta <= -iDeltaPerLine) {
              //printf("iAccumDelta %d\n", iAccumDelta);
              SendMessage (mWnd, WM_VSCROLL, SB_LINEDOWN, (LONG)scrollbar) ;
              iAccumDelta += iDeltaPerLine ;
            }
         }
            return 0 ;
      } break;


        case WM_PALETTECHANGED:
            if ((HWND)wParam == mWnd) {
                // We caused the WM_PALETTECHANGED message so avoid realizing
                // another foreground palette
                result = PR_TRUE;
                break;
            }
            // fall thru...

        case WM_QUERYNEWPALETTE:
            mContext->GetPaletteInfo(palInfo);
            if (palInfo.isPaletteDevice && palInfo.palette) {
                HDC hDC = ::GetDC(mWnd);
                HPALETTE hOldPal = ::SelectPalette(hDC, (HPALETTE)palInfo.palette, FALSE);
                
                // Realize the drawing palette
                int i = ::RealizePalette(hDC);

                // Did any of our colors change?
                if (i > 0) {
                  // Yes, so repaint
                  ::InvalidateRect(mWnd, (LPRECT)NULL, TRUE);
                }

                ::SelectPalette(hDC, hOldPal, TRUE);
                ::RealizePalette(hDC);
                ::ReleaseDC(mWnd, hDC);
                *aRetValue = TRUE;
            }
            result = PR_TRUE;
            break;

		case WM_IME_STARTCOMPOSITION: {
			COMPOSITIONFORM compForm;
			HIMC hIMEContext;

			mIMEIsComposing = PR_TRUE;
#ifdef		DEBUG_TAGUE
			printf("IME: Recieved WM_IME_STARTCOMPOSITION\n");
#endif
			if ((mIMEProperty & IME_PROP_SPECIAL_UI) || (mIMEProperty & IME_PROP_AT_CARET)) {
				return PR_FALSE;
			}

			hIMEContext = ::ImmGetContext(mWnd);
			if (hIMEContext==NULL) {
				return PR_TRUE;
			}

			compForm.dwStyle = CFS_POINT;
			compForm.ptCurrentPos.x = 100;
			compForm.ptCurrentPos.y = 100;

			::ImmSetCompositionWindow(hIMEContext,&compForm);
			::ImmReleaseContext(mWnd,hIMEContext);

			result = PR_TRUE;
			}
			break;

		case WM_IME_COMPOSITION: {
			//COMPOSITIONFORM compForm;
			HIMC hIMEContext;

			result = PR_FALSE;					// will change this if an IME message we handle

			hIMEContext = ::ImmGetContext(mWnd);
			if (hIMEContext==NULL) {
				return PR_TRUE;
			}

			if (lParam & GCS_COMPSTR) {
				long compStrLen = ::ImmGetCompositionString(hIMEContext,GCS_COMPSTR,NULL,0);
				if (compStrLen+1>mIMECompositionStringSize) {
					if (mIMECompositionString!=NULL) delete [] mIMECompositionString;
					mIMECompositionString = new char[compStrLen+32];
					mIMECompositionStringSize = compStrLen+32;
				}
				
				::ImmGetCompositionString(hIMEContext,GCS_COMPSTR,mIMECompositionString,mIMECompositionStringSize);
				mIMECompositionStringLength = compStrLen;
				mIMECompositionString[compStrLen]='\0';
				HandleTextEvent(PR_FALSE);
				result = PR_TRUE;
			}

			if (lParam & GCS_RESULTSTR) {
				long compStrLen = ::ImmGetCompositionString(hIMEContext,GCS_RESULTSTR,NULL,0);
				if (compStrLen+1>mIMECompositionStringSize) {
					delete [] mIMECompositionString;
					mIMECompositionString = new char[compStrLen+32];
					mIMECompositionStringSize = compStrLen+32;
				}
				
				::ImmGetCompositionString(hIMEContext,GCS_RESULTSTR,mIMECompositionString,mIMECompositionStringSize);
				mIMECompositionStringLength = compStrLen;
				mIMECompositionString[compStrLen]='\0';
				result = PR_TRUE;
				HandleTextEvent(PR_TRUE);
			}
#ifdef DEBUG_TAGUE
			if (lParam & GCS_COMPSTR)
				printf("IME: Composition String = %s\n",mIMECompositionString);
			if (lParam & GCS_RESULTSTR)
				printf("IME: Result String = %s\n",mIMECompositionString);
#endif			
			
			::ImmReleaseContext(mWnd,hIMEContext);

			}
			break;

		case WM_IME_ENDCOMPOSITION:

			mIMEIsComposing = PR_FALSE;
#ifdef DEBUG_TAGUE
			printf("IME: Received WM_IME_ENDCOMPOSITION\n");
#endif
			result = PR_TRUE;
			break;

        case WM_DROPFILES: {
          /*HDROP hDropInfo = (HDROP) wParam;
	        UINT nFiles = ::DragQueryFile(hDropInfo, (UINT)-1, NULL, 0);

	        for (UINT iFile = 0; iFile < nFiles; iFile++) {
		        TCHAR szFileName[_MAX_PATH];
		        ::DragQueryFile(hDropInfo, iFile, szFileName, _MAX_PATH);
            printf("szFileName [%s]\n", szFileName);
            nsAutoString fileStr(szFileName);
            nsEventStatus status;
            nsDragDropEvent event;
            InitEvent(event, NS_DRAGDROP_EVENT);
            event.mType      = nsDragDropEventStatus_eDrop;
            event.mIsFileURL = PR_FALSE;
            event.mURL       = (PRUnichar *)fileStr.GetUnicode();
            DispatchEvent(&event, status);
	        }*/
        } break;

      case WM_DESTROYCLIPBOARD: {
        nsIClipboard* clipboard;
        nsresult rv = nsServiceManager::GetService(kCClipboardCID,
                                                   kIClipboardIID,
                                                   (nsISupports **)&clipboard);
        clipboard->EmptyClipboard();
        NS_RELEASE(clipboard);
      } break;

    }

    return result;
}



//-------------------------------------------------------------------------
//
// return the window class name and initialize the class if needed
//
//-------------------------------------------------------------------------
LPCTSTR nsWindow::WindowClass()
{
    const LPCTSTR className = "NetscapeWindowClass";

    if (!nsWindow::sIsRegistered) {
        WNDCLASS wc;

//        wc.style            = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.style            = CS_DBLCLKS;
        wc.lpfnWndProc      = ::DefWindowProc;
        wc.cbClsExtra       = 0;
        wc.cbWndExtra       = 0;
        wc.hInstance        = nsToolkit::mDllInstance;
        wc.hIcon            = ::LoadIcon(NULL, IDI_APPLICATION);
        wc.hCursor          = NULL;
        wc.hbrBackground    = mBrush;
        wc.lpszMenuName     = NULL;
        wc.lpszClassName    = className;
    
        nsWindow::sIsRegistered = ::RegisterClass(&wc);
    }

    return className;
}


//-------------------------------------------------------------------------
//
// return nsWindow styles
//
//-------------------------------------------------------------------------
DWORD nsWindow::WindowStyle()
{
    return WS_OVERLAPPEDWINDOW;
}


//-------------------------------------------------------------------------
//
// return nsWindow extended styles
//
//-------------------------------------------------------------------------
DWORD nsWindow::WindowExStyle()
{
    return WS_EX_CLIENTEDGE;
}


// -----------------------------------------------------------------------
//
// Subclass (or remove the subclass from) this component's nsWindow
//
// -----------------------------------------------------------------------
void nsWindow::SubclassWindow(BOOL bState)
{
  if (NULL != mWnd) {
    NS_PRECONDITION(::IsWindow(mWnd), "Invalid window handle");
    
    if (bState) {
        // change the nsWindow proc
        mPrevWndProc = (WNDPROC)::SetWindowLong(mWnd, GWL_WNDPROC, 
                                                 (LONG)nsWindow::WindowProc);
        NS_ASSERTION(mPrevWndProc, "Null standard window procedure");
        // connect the this pointer to the nsWindow handle
        ::SetWindowLong(mWnd, GWL_USERDATA, (LONG)this);
    } 
    else {
        ::SetWindowLong(mWnd, GWL_WNDPROC, (LONG)mPrevWndProc);
        ::SetWindowLong(mWnd, GWL_USERDATA, (LONG)NULL);
        mPrevWndProc = NULL;
    }
  }
}


//-------------------------------------------------------------------------
//
// WM_DESTROY has been called
//
//-------------------------------------------------------------------------
void nsWindow::OnDestroy()
{
    mOnDestroyCalled = PR_TRUE;

    SubclassWindow(FALSE);
    mWnd = NULL;

    // free GDI objects
    if (mBrush) {
      VERIFY(::DeleteObject(mBrush));
      mBrush = NULL;
    }
    if (mPalette) {
      VERIFY(::DeleteObject(mPalette));
      mPalette = NULL;
    }

    // free tooltip window
    if (mTooltip) {
      VERIFY(::DestroyWindow(mTooltip));
      mTooltip = NULL;
    }

    // if we were in the middle of deferred window positioning then
    // free the memory for the multiple-window position structure
    if (mDeferredPositioner) {
      VERIFY(::EndDeferWindowPos(mDeferredPositioner));
      mDeferredPositioner = NULL;
    }

    // release references to children, device context, toolkit, and app shell
    nsBaseWidget::OnDestroy();
 
    // dispatch the event
    if (!mIsDestroying) {
      // dispatching of the event may cause the reference count to drop to 0
      // and result in this object being destroyed. To avoid that, add a reference
      // and then release it after dispatching the event
      AddRef();
      DispatchStandardEvent(NS_DESTROY);
      Release();
    }
}

//-------------------------------------------------------------------------
//
// Move
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnMove(PRInt32 aX, PRInt32 aY)
{            
  nsGUIEvent event;
  InitEvent(event, NS_MOVE);
  event.point.x = aX;
  event.point.y = aY;
  event.eventStructType = NS_GUI_EVENT;

  PRBool result = DispatchWindowEvent(&event);
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
    nsRect    bounds;
    PRBool result = PR_TRUE;
    PAINTSTRUCT ps;

#ifdef PAINT_DEBUG
HRGN rgn = ::CreateRectRgn(0, 0, 0, 0);
::GetUpdateRgn(mWnd, rgn, TRUE);
HDC dc = ::GetDC(mWnd);
HBRUSH brsh = ::CreateSolidBrush(RGB(255, 0, 0));
::FillRgn(dc, rgn, brsh);
::ReleaseDC(mWnd, dc);
::DeleteObject(rgn);

int x;

for (x = 0; x < 10000000; x++);
#endif

    HDC hDC = ::BeginPaint(mWnd, &ps);

    // XXX What is this check doing? If it's trying to check for an empty
    // paint rect then use the IsRectEmpty() function...
    if (ps.rcPaint.left || ps.rcPaint.right || ps.rcPaint.top || ps.rcPaint.bottom) {
        // call the event callback 
        if (mEventCallback) {
            nsPaintEvent event;

            InitEvent(event, NS_PAINT);

            nsRect rect(ps.rcPaint.left, 
                        ps.rcPaint.top, 
                        ps.rcPaint.right - ps.rcPaint.left, 
                        ps.rcPaint.bottom - ps.rcPaint.top);
            event.rect = &rect;
            event.eventStructType = NS_PAINT_EVENT;

            static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
            static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);
            static NS_DEFINE_IID(kRenderingContextWinIID, NS_IRENDERING_CONTEXT_WIN_IID);

            if (NS_OK == nsComponentManager::CreateInstance(kRenderingContextCID, nsnull, kRenderingContextIID, (void **)&event.renderingContext))
            {
              nsIRenderingContextWin *winrc;

              if (NS_OK == event.renderingContext->QueryInterface(kRenderingContextWinIID, (void **)&winrc))
              {
                nsDrawingSurface surf;

                //i know all of this seems a little backwards. i'll fix it, i swear. MMP

                if (NS_OK == winrc->CreateDrawingSurface(hDC, surf))
                {
                  event.renderingContext->Init(mContext, surf);
                  result = DispatchWindowEvent(&event);
                  event.renderingContext->DestroyDrawingSurface(surf);
                }

                NS_RELEASE(winrc);
              }

              NS_RELEASE(event.renderingContext);
            }
            else
              result = PR_FALSE;

            NS_RELEASE(event.widget);
        }
    }

    ::EndPaint(mWnd, &ps);

    return result;
}


//-------------------------------------------------------------------------
//
// Send a resize message to the listener
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnResize(nsRect &aWindowRect)
{
  // call the event callback 
  if (mEventCallback) {
    nsSizeEvent event;
    InitEvent(event, NS_SIZE);
    event.windowSize = &aWindowRect;
    event.eventStructType = NS_SIZE_EVENT;
    RECT r;
    if (::GetWindowRect(mWnd, &r)) {
      event.mWinWidth  = PRInt32(r.right - r.left);
      event.mWinHeight = PRInt32(r.bottom - r.top);
    } else {
      event.mWinWidth  = 0;
      event.mWinHeight = 0;
    }
    PRBool result = DispatchWindowEvent(&event);
    NS_RELEASE(event.widget);
    return result;
  }

  return PR_FALSE;
}



//-------------------------------------------------------------------------
//
// Deal with all sort of mouse event
//
//-------------------------------------------------------------------------
PRBool nsWindow::DispatchMouseEvent(PRUint32 aEventType, nsPoint* aPoint)
{
  PRBool result = PR_FALSE;

  if (nsnull == mEventCallback && nsnull == mMouseListener) {
    return result;
  }

  nsMouseEvent event;
  InitEvent(event, aEventType, aPoint);

  event.isShift   = IS_VK_DOWN(NS_VK_SHIFT);
  event.isControl = IS_VK_DOWN(NS_VK_CONTROL);
  event.isAlt     = IS_VK_DOWN(NS_VK_ALT);
  event.clickCount = (aEventType == NS_MOUSE_LEFT_DOUBLECLICK ||
                      aEventType == NS_MOUSE_LEFT_DOUBLECLICK)? 2:1;
  event.eventStructType = NS_MOUSE_EVENT;

  // call the event callback 
  if (nsnull != mEventCallback) {

    result = DispatchWindowEvent(&event);

    if (aEventType == NS_MOUSE_MOVE) {

      MouseTrailer * mouseTrailer = MouseTrailer::GetMouseTrailer(0);
      MouseTrailer::SetMouseTrailerWindow(this);
      mouseTrailer->CreateTimer();

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
        result = ConvertStatus(mMouseListener->MouseClicked(event));
        break;
    } // switch
  } 
  NS_RELEASE(event.widget);
  return result;
}

//-------------------------------------------------------------------------
//
// Deal with focus messages
//
//-------------------------------------------------------------------------
PRBool nsWindow::DispatchFocus(PRUint32 aEventType)
{
    // call the event callback 
    if (mEventCallback) {
      if ((nsnull != gCurrentWindow) && (!gCurrentWindow->mIsDestroying)) {
        return(DispatchStandardEvent(aEventType));
      }
    }

    return PR_FALSE;
}


//-------------------------------------------------------------------------
//
// Deal with scrollbar messages (actually implemented only in nsScrollbar)
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnScroll(UINT scrollCode, int cPos)
{
    return PR_FALSE;
}


//-------------------------------------------------------------------------
//
// Return the brush used to paint the background of this control 
//
//-------------------------------------------------------------------------
HBRUSH nsWindow::OnControlColor()
{
    return mBrush;
}

//-------------------------------------------------------------------------
//
// return the style for a child nsWindow
//
//-------------------------------------------------------------------------
DWORD ChildWindow::WindowStyle()
{
    return WS_CHILD | WS_CLIPCHILDREN | GetBorderStyle(mBorderStyle);
}

//-------------------------------------------------------------------------
//
// Deal with all sort of mouse event
//
//-------------------------------------------------------------------------
PRBool ChildWindow::DispatchMouseEvent(PRUint32 aEventType, nsPoint* aPoint)
{
  PRBool result = PR_FALSE;

  if (nsnull == mEventCallback && nsnull == mMouseListener) {
    return result;
  }

  switch (aEventType) {
    case NS_MOUSE_LEFT_BUTTON_DOWN:
    case NS_MOUSE_MIDDLE_BUTTON_DOWN:
    case NS_MOUSE_RIGHT_BUTTON_DOWN:
      SetCapture(mWnd);
      break;

    case NS_MOUSE_LEFT_BUTTON_UP:
    case NS_MOUSE_MIDDLE_BUTTON_UP:
    case NS_MOUSE_RIGHT_BUTTON_UP:
      ReleaseCapture();
      break;

    default:
      break;

  } // switch

  return nsWindow::DispatchMouseEvent(aEventType, aPoint);
}

DWORD nsWindow::GetBorderStyle(nsBorderStyle aBorderStyle)
{
  switch(aBorderStyle)
  {
    case eBorderStyle_none:
      return(0);
    break;

    case eBorderStyle_dialog:
     return(WS_DLGFRAME | DS_3DLOOK);
    break;

    default:
      NS_ASSERTION(0, "unknown border style");
      return(WS_OVERLAPPEDWINDOW);
  }
}

NS_METHOD nsWindow::SetTitle(const nsString& aTitle) 
{
  NS_ALLOC_STR_BUF(buf, aTitle, 256);
  ::SendMessage(mWnd, WM_SETTEXT, (WPARAM)0, (LPARAM)(LPCTSTR)buf);
  NS_FREE_STR_BUF(buf);
  return NS_OK;
} 


PRBool nsWindow::AutoErase()
{
  return(PR_FALSE);
}

NS_METHOD nsWindow::SetMenuBar(nsIMenuBar * aMenuBar) 
{
  mMenuBar = aMenuBar;
  NS_ADDREF(mMenuBar);

  HMENU nativeMenuHandle;
  void * voidData;
  aMenuBar->GetNativeData(voidData);
  nativeMenuHandle = (HMENU)voidData;

  if (nsnull != nativeMenuHandle) {
    ::SetMenu(mWnd, nativeMenuHandle);
    return NS_OK;
  } else {
    return NS_ERROR_FAILURE;
  }
} 

NS_METHOD nsWindow::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
  aWidth  = mPreferredWidth;
  aHeight = mPreferredHeight;
  return NS_ERROR_FAILURE;
}

NS_METHOD nsWindow::SetPreferredSize(PRInt32 aWidth, PRInt32 aHeight)
{
  mPreferredWidth  = aWidth;
  mPreferredHeight = aHeight;
  return NS_OK;
}

void
nsWindow::HandleTextEvent(PRBool commit)
{
  nsTextEvent event;
  nsPoint point;
  size_t	unicharSize;

  point.x = 0;
  point.y = 0;

  InitEvent(event, NS_TEXT_EVENT, &point);
  if (mIMECompositionUniString!=NULL)
	  delete [] mIMECompositionUniString;
  
  unicharSize = ::MultiByteToWideChar(CP_ACP,MB_PRECOMPOSED,mIMECompositionString,mIMECompositionStringLength,
	  mIMECompositionUniString,0);
  mIMECompositionUniString = new PRUnichar[unicharSize+1];
  ::MultiByteToWideChar(CP_ACP,MB_PRECOMPOSED,mIMECompositionString,mIMECompositionStringLength,
	  mIMECompositionUniString,unicharSize);
  mIMECompositionUniString[unicharSize] = (PRUnichar)0;

  event.theText      = mIMECompositionUniString;
  event.commitText	 = commit;
  event.isShift   = mIsShiftDown;
  event.isControl = mIsControlDown;
  event.isAlt     = mIsAltDown;
  event.eventStructType = NS_TEXT_EVENT;

  (void)DispatchWindowEvent(&event);
  NS_RELEASE(event.widget);

}
