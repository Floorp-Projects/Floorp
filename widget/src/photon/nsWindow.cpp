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
#include <Pt.h>
#include "nsGfxCIID.h"
#include "prtime.h"

#include "nsIMenu.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"


nsWindow* nsWindow::gCurrentWindow = nsnull;

// Global variable 

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
    mWidget             = 0;
    mIsShiftDown        = PR_FALSE;
    mIsControlDown      = PR_FALSE;
    mIsAltDown          = PR_FALSE;
    mIsDestroying       = PR_FALSE;
    mOnDestroyCalled    = PR_FALSE;
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
}


//-------------------------------------------------------------------------
//
// nsWindow destructor
//
//-------------------------------------------------------------------------
nsWindow::~nsWindow()
{
  mIsDestroying = PR_TRUE;
  if( gCurrentWindow == this )
  {
    gCurrentWindow = nsnull;
  }

  // If the widget was released without calling Destroy() then the native
  // window still exists, and we need to destroy it
  if( NULL != mWidget )
  {
    Destroy();
  }

  NS_IF_RELEASE(mHitMenu); // this should always have already been freed by the deselect

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
  return NS_OK;
}

NS_METHOD nsWindow::EndResizingChildren(void)
{
  return NS_OK;
}

// DoCreateTooltip - creates a tooltip control and adds some tools  
//     to it. 
// Returns the handle of the tooltip control if successful or NULL
//     otherwise. 
// hwndOwner - handle of the owner window 
// 


NS_METHOD nsWindow::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
  return NS_OK;
}

NS_METHOD nsWindow::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
  return NS_OK;
} 

//-------------------------------------------------------------------------
//
// Setup initial tooltip rectangles
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::SetTooltips(PRUint32 aNumberOfTips,nsRect* aTooltipAreas[])
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Update all tooltip rectangles
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::UpdateTooltips(nsRect* aNewTips[])
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Remove all tooltip rectangles
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::RemoveTooltips()
{
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
  if((aStatus != nsEventStatus_eIgnore) && (nsnull != mEventListener)) {
    aStatus = mEventListener->ProcessEvent(*event);
  }

  nsWindow * thisPtr = this;

  return NS_OK;
}

//-------------------------------------------------------------------------
PRBool nsWindow::DispatchWindowEvent(nsGUIEvent* event)
{
  return PR_TRUE;
}

//-------------------------------------------------------------------------
//
// Dispatch standard event
//
//-------------------------------------------------------------------------

PRBool nsWindow::DispatchStandardEvent(PRUint32 aMsg)
{
  return PR_TRUE;
}

//-------------------------------------------------------------------------
//
// the nsWindow raw event callback for all nsWindows in this toolkit
//
//-------------------------------------------------------------------------
int nsWindow::RawEventHandler( PtWidget_t *widget, void *data, PtCallbackInfo_t *cbinfo )
{
  // Get the window which caused the event and ask it to process the message
  PtArg_t arg;
  nsWindow *someWindow = (nsWindow*) data;

  if( nsnull != someWindow )
    someWindow->HandleEvent( cbinfo );

  return( Pt_CONTINUE );
}



nsresult nsWindow::StandardWindowCreate(nsIWidget *aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData,
                      nsNativeWidget aNativeParent)
{
  PtArg_t arg[5];
  PhPoint_t pos;
  PhDim_t dim;
  unsigned long render_flags;
  nsresult result = NS_ERROR_FAILURE;

  BaseCreate(aParent, aRect, aHandleEventFunction, aContext, 
     aAppShell, aToolkit, aInitData);


  // Switch to the "main gui thread" if necessary... This method must
  // be executed on the "gui thread"...

  // REVISIT
  printf( "Must check thread here...\n" );  

  PtWidget_t *parent;
  if( nsnull != aParent ) // has a nsIWidget parent
    parent = (PtWidget_t*) aParent->GetNativeData(NS_NATIVE_WINDOW);
  else  // has a nsNative parent
   parent = (PtWidget_t*)aNativeParent;

  render_flags = Ph_WM_RENDER_TITLE | Ph_WM_RENDER_CLOSE;
  if( nsnull != aInitData )
  {
    switch( aInitData->mBorderStyle )
    {
    case eBorderStyle_dialog:
      render_flags |= Ph_WM_RENDER_BORDER;
      break;

    case eBorderStyle_window:
      render_flags |= Ph_WM_RENDER_BORDER | Ph_WM_RENDER_RESIZE |
                      Ph_WM_RENDER_MAX | Ph_WM_RENDER_MIN | Ph_WM_RENDER_MENU;

    case eBorderStyle_3DChildWindow:
      render_flags |= Ph_WM_RENDER_BORDER | Ph_WM_RENDER_RESIZE;
      break;

    case eBorderStyle_none:
    default:
      break;
    }
  }

  mHas3DBorder = PR_FALSE;

  PtSetArg( &arg[0], Pt_ARG_POS, &pos, 0 );
  PtSetArg( &arg[1], Pt_ARG_DIM, &dim, 0 );
  PtSetArg( &arg[2], Pt_ARG_WINDOW_RENDER_FLAGS, render_flags, 0 );
  mWidget = PtCreateWidget( PtWindow, parent, 3, arg );

  if( mWidget )
  {
    // Attach event handler
    PtAddEventHandler( mWidget, 0xFFFFFFFF, RawEventHandler, this );
    
    // call the event callback to notify about creation
    DispatchStandardEvent(NS_CREATE);
  }

  return result;
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
  // REVISIT
  printf( "Must check thread here...\n" );  

  // disconnect from the parent
  if (!mIsDestroying) {
    nsBaseWidget::Destroy();
  }

  // destroy the HWND
  if( mWidget )
  {
    // prevent the widget from causing additional events
    mEventCallback = nsnull;
    PtDestroyWidget( mWidget );
    mWidget = NULL;
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

  if( mWidget )
  {
    PtWidget_t* parent = PtWidgetParent( mWidget );
    if( parent )
    {
      PtArg_t arg;
      
      PtSetArg( &arg, Pt_ARG_USER_DATA, &widget, 0 );
      if(( PtGetResources( mWidget, 1, &arg ) == 0 ) && widget )
      {
        // If the widget is in the process of being destroyed then
        // do NOT return it
        if( widget->mIsDestroying )
          widget = NULL;
        else
          NS_ADDREF( widget );
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
NS_METHOD nsWindow::Show( PRBool bState )
{
  if( mWidget )
  {
    if( bState )
      PtRealizeWidget( mWidget );
    else
      PtUnrealizeWidget( mWidget );
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
  if( mWidget && PtWidgetIsRealized( mWidget ))
    bState = PR_TRUE;
  else
    bState = PR_FALSE;

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

  if( mWidget )
  {
    PtArg_t arg;
    PhPoint_t pos = { aX, aY };

    PtSetArg( &arg, Pt_ARG_POS, &pos, 0 );
    PtSetResources( mWidget, 1, &arg );
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

  if( mWidget )
  {
    PtArg_t arg;
    PhDim_t dim = { aWidth, aHeight };

    PtSetArg( &arg, Pt_ARG_DIM, &dim, 0 );
    PtSetResources( mWidget, 1, &arg );
  }
  return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Resize(PRUint32 aX, PRUint32 aY, PRUint32 aWidth, PRUint32 aHeight, PRBool aRepaint)
{
  // Set cached value for lightweight and printing
  mBounds.x      = aX;
  mBounds.y      = aY;
  mBounds.width  = aWidth;
  mBounds.height = aHeight;

  if( mWidget )
  {
    PtArg_t arg[2];
    PhPoint_t pos = { aX, aY };
    PhDim_t dim = { aWidth, aHeight };

    PtSetArg( &arg[0], Pt_ARG_POS, &pos, 0 );
    PtSetArg( &arg[1], Pt_ARG_DIM, &dim, 0 );
    PtSetResources( mWidget, 2, arg );
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
  if( mWidget )
  {
    PtArg_t arg;
      
    if( PR_TRUE == bState )
      PtSetArg( &arg, Pt_ARG_FLAGS, 0, Pt_BLOCKED );
    else
      PtSetArg( &arg, Pt_ARG_FLAGS, Pt_BLOCKED, Pt_BLOCKED );

    PtSetResources( mWidget, 1, &arg );      
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

    // REVISIT
    printf( "Must check thread here...\n" );  

    if( mWidget )
    {
      PtContainerGiveFocus( mWidget, NULL );
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
  nsresult res = NS_ERROR_FAILURE;

  if( mWidget )
  {
    PhRect_t extent;

    PtWidgetExtent( mWidget, &extent );

    aRect.x = 0;
    aRect.y = 0;
    aRect.width = extent.lr.x - extent.ul.x;
    aRect.height = extent.lr.y - extent.ul.y;

    res = NS_OK;
  }
  else
    aRect.SetRect(0,0,0,0);

  return res;
}

//-------------------------------------------------------------------------
//
// Get this component dimension
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::GetClientBounds( nsRect &aRect )
{
  nsresult res = NS_ERROR_FAILURE;

  if( mWidget )
  {
    PhRect_t extent;

    PtBasicWidgetCanvas( mWidget, &extent );

    aRect.x = 0;
    aRect.y = 0;
    aRect.width = extent.lr.x - extent.ul.x;
    aRect.height = extent.lr.y - extent.ul.y;

    res = NS_OK;
  }
  else
    aRect.SetRect(0,0,0,0);

  return res;
}

//get the bounds, but don't take into account the client size

void nsWindow::GetNonClientBounds(nsRect &aRect)
{
  if( mWidget )
  {
    PhRect_t extent;
    PtArg_t arg[2];
    int lf,rf,tf,bf;
    unsigned long *render;
    int *border;

    PtWidgetExtent( mWidget, &extent );

    PtSetArg( &arg[0], Pt_ARG_WINDOW_RENDER_FLAGS, &render, 0 );
    PtSetArg( &arg[1], Pt_ARG_BORDER_WIDTH, &border, 0 );
    if( PtGetResources( mWidget, 2, arg ) == 0 )
    {
      PtFrameSize( *render, *border, &lf, &tf, &rf, &bf );
    }
    else
    {
      lf = rf = tf = bf = 0;
    }

    aRect.x = extent.ul.x - lf;
    aRect.y = extent.ul.y - tf;
    aRect.width = extent.lr.x - extent.ul.x + lf + rf;
    aRect.height = extent.lr.y - extent.ul.y + tf + bf;

    // REVISIT - What are these coords supposed to be
    // relative to? Screen, Parent, Top-Level Window?????
  }
  else
    aRect.SetRect(0,0,0,0);

}

           
//-------------------------------------------------------------------------
//
// Set the background color
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetBackgroundColor(const nscolor &aColor)
{
  return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Get this component font
//
//-------------------------------------------------------------------------
nsIFontMetrics* nsWindow::GetFont(void)
{
  return NULL;
}

    
//-------------------------------------------------------------------------
//
// Set this component font
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetFont(const nsFont &aFont)
{
  return NS_OK;
}

        
//-------------------------------------------------------------------------
//
// Set this component cursor
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::SetCursor(nsCursor aCursor)
{
  unsigned short curs;

  switch( aCursor )
  {
  case eCursor_select:
    curs = Ph_CURSOR_INSERT;
    break;
      
  case eCursor_wait:
    curs = Ph_CURSOR_LONG_WAIT;
    break;

  case eCursor_hyperlink:
    curs = Ph_CURSOR_FINGER;
    break;

  case eCursor_standard:
    curs = Ph_CURSOR_POINTER;
    break;

  case eCursor_sizeWE:
    curs = Ph_CURSOR_DRAG_HORIZONTAL;
    break;

  case eCursor_sizeNS:
    curs = Ph_CURSOR_DRAG_VERTICAL;
    break;

  // REVISIT - Photon does not have the following cursor types...

  case eCursor_arrow_north:
    curs = Ph_CURSOR_POINTER;
    break;

  case eCursor_arrow_north_plus:
    curs = Ph_CURSOR_POINTER;
    break;

  case eCursor_arrow_south:
    curs = Ph_CURSOR_POINTER;
    break;

  case eCursor_arrow_south_plus:
    curs = Ph_CURSOR_POINTER;
    break;

  case eCursor_arrow_east:
    curs = Ph_CURSOR_POINTER;
    break;

  case eCursor_arrow_east_plus:
    curs = Ph_CURSOR_POINTER;
    break;

  case eCursor_arrow_west:
    curs = Ph_CURSOR_POINTER;
    break;

  case eCursor_arrow_west_plus:
    curs = Ph_CURSOR_POINTER;
    break;

  default:
    NS_ASSERTION(0, "Invalid cursor type");
    break;
  }

  PtArg_t arg;

  PtSetArg( &arg, Pt_ARG_CURSOR_TYPE, curs, 0 );
  PtSetResources( mWidget, 1, &arg );
  mCursor = aCursor;

  return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Invalidate(PRBool aIsSynchronous)
{
  if( mWidget )
  {
    PtDamageWidget( mWidget );

    // REVISIT - PtFlush may be unstable & cause crashes...
    if( aIsSynchronous )
      PtFlush();
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
  if( mWidget )
  {
    PhRect_t extent;
 
    extent.ul.x = aRect.x;
    extent.ul.y = aRect.y;
    extent.lr.x = aRect.x + aRect.width;
    extent.lr.y = aRect.y + aRect.height;
 
    PtDamageExtent( mWidget, &extent );

    // REVISIT - PtFlush may be unstable & cause crashes...
    if( aIsSynchronous )
      PtFlush();
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
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void* nsWindow::GetNativeData( PRUint32 aDataType )
{
  switch( aDataType )
  {
    case NS_NATIVE_WIDGET:
    case NS_NATIVE_WINDOW:
      return (void*)mWidget;
    case NS_NATIVE_GRAPHIC:
//    return (void*)::GetDC(mWnd);
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
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
  return NS_OK;
}



//-------------------------------------------------------------------------
//
// OnKey
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnKey( PRUint32 aEventType, PRUint32 aKeyCode )
{
  return PR_FALSE;
}


//-------------------------------------------------------------------------
static void AdjustMenus(nsIMenu * aCurrentMenu, nsIMenu * aNewMenu, nsMenuEvent & aEvent) 
{
  if (nsnull != aCurrentMenu) {
    nsIMenuListener * listener;
    if (NS_OK == aCurrentMenu->QueryInterface(kIMenuListenerIID, (void **)&listener)) {
      listener->MenuDeselected(aEvent);
      NS_RELEASE(listener);
    }
  }

  if (nsnull != aNewMenu)  {
    nsIMenuListener * listener;
    if (NS_OK == aNewMenu->QueryInterface(kIMenuListenerIID, (void **)&listener)) {
      listener->MenuSelected(aEvent);
      NS_RELEASE(listener);
    }
  }
}


//---------------------------------------------------------
NS_METHOD nsWindow::EnableFileDrop(PRBool aEnable)
{
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Process all nsWindows messages
//
//-------------------------------------------------------------------------
PRBool nsWindow::HandleEvent( PtCallbackInfo_t* aCbInfo )
{
  PRBool  result = PR_FALSE; // call the default nsWindow proc

  // When we get menu selections, dispatch the menu command (event) AND IF there is
  // a menu listener, call "listener->MenuSelected(event)"

  if( aCbInfo->reason == Pt_CB_RAW )
  {
    PhEvent_t* event = aCbInfo->event;

    switch( event->type )
    {
    case Ph_EV_KEY:
      {
      PhKeyEvent_t* keyev = (PhKeyEvent_t*) PhGetData( event );
    
      mIsShiftDown   = ( keyev->key_mods & Pk_KM_Shift ) ? PR_TRUE : PR_FALSE;
      mIsControlDown = ( keyev->key_mods & Pk_KM_Ctrl ) ? PR_TRUE : PR_FALSE;
      mIsAltDown     = ( keyev->key_mods & Pk_KM_Alt ) ? PR_TRUE : PR_FALSE;
      if( keyev->key_flags & Pk_KF_Key_Down )
        result = OnKey( NS_KEY_DOWN, keyev->key_sym );
      else
        result = OnKey( NS_KEY_UP, keyev->key_sym );
      }
      break;

    case Ph_EV_PTR_MOTION_BUTTON:
    case Ph_EV_PTR_MOTION_NOBUTTON:
      result = DispatchMouseEvent( NS_MOUSE_MOVE );
      break;

    case Ph_EV_BUT_PRESS:
      {
      PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( event );

      if( ptrev )
      {
        if( ptrev->buttons & Ph_BUTTON_SELECT ) // Normally the left mouse button
        {
          if( ptrev->click_count == 2 )
            result = DispatchMouseEvent( NS_MOUSE_LEFT_DOUBLECLICK );
          else
            result = DispatchMouseEvent( NS_MOUSE_LEFT_BUTTON_DOWN );
        }
        else if( ptrev->buttons & Ph_BUTTON_MENU ) // the right button
        {
          if( ptrev->click_count == 2 )
            result = DispatchMouseEvent( NS_MOUSE_RIGHT_DOUBLECLICK );
          else
            result = DispatchMouseEvent( NS_MOUSE_RIGHT_BUTTON_DOWN );
        }
        else // middle button
        {
          if( ptrev->click_count == 2 )
            result = DispatchMouseEvent( NS_MOUSE_MIDDLE_DOUBLECLICK );
          else
            result = DispatchMouseEvent( NS_MOUSE_MIDDLE_BUTTON_DOWN );
        }
      }
      }
      break;

    case Ph_EV_BUT_RELEASE:
      {
      PhPointerEvent_t* ptrev = (PhPointerEvent_t*) PhGetData( event );

      if( ptrev )
      {
        if( ptrev->buttons & Ph_BUTTON_SELECT ) // Normally the left mouse button
          result = DispatchMouseEvent( NS_MOUSE_LEFT_BUTTON_UP );
        else if( ptrev->buttons & Ph_BUTTON_MENU ) // the right button
          result = DispatchMouseEvent( NS_MOUSE_RIGHT_BUTTON_UP );
        else // middle button
          result = DispatchMouseEvent( NS_MOUSE_MIDDLE_BUTTON_UP );
      }
      }
      break;

    case Ph_EV_WM:
      {
      PhWindowEvent_t* wmev = (PhWindowEvent_t*) PhGetData( event );
      switch( wmev->event_f )
      {
      case Ph_WM_FOCUS:
        if( wmev->event_state == Ph_WM_EVSTATE_FOCUS )
          result = DispatchFocus(NS_GOTFOCUS);
        else
          result = DispatchFocus(NS_LOSTFOCUS);
        break;
      }
      }
      break;
    }
  }

  return result;
}



//-------------------------------------------------------------------------
//
// WM_DESTROY has been called
//
//-------------------------------------------------------------------------
void nsWindow::OnDestroy()
{
  mOnDestroyCalled = PR_TRUE;

  mWidget = NULL;

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
  return PR_TRUE;
}


//-------------------------------------------------------------------------
//
// Paint
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnPaint()
{
  return PR_TRUE;
}


//-------------------------------------------------------------------------
//
// Send a resize message to the listener
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnResize(nsRect &aWindowRect)
{
  // call the event callback 
  if( mEventCallback )
  {
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
  return PR_TRUE;
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
//      SetCapture(mWnd);
      // Photon doesn't allow mouse events to be captures outside of the
      // apps regions...
      break;

    case NS_MOUSE_LEFT_BUTTON_UP:
    case NS_MOUSE_MIDDLE_BUTTON_UP:
    case NS_MOUSE_RIGHT_BUTTON_UP:
//      ReleaseCapture();
      break;

    default:
      break;

  } // switch

  return nsWindow::DispatchMouseEvent(aEventType, aPoint);
}



NS_METHOD nsWindow::SetTitle(const nsString& aTitle) 
{
  if( mWidget )
  {
    PtArg_t arg;
  
    NS_ALLOC_STR_BUF(buf, aTitle, 256);
    PtSetArg( &arg, Pt_ARG_WINDOW_TITLE, buf, 0 );
    PtSetResources( mWidget, 1, &arg );
    NS_FREE_STR_BUF(buf);
  }
  return NS_OK;
} 

PRBool nsWindow::AutoErase()
{
  return(PR_FALSE);
}

NS_METHOD nsWindow::SetMenuBar(nsIMenuBar * aMenuBar) 
{
  return NS_OK;
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




