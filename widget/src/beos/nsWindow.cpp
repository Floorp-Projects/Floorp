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
//#include "sysmets.h"
#include "nsGfxCIID.h"
#include "resource.h"
#include "prtime.h"

#include "nsIMenu.h"
#include "nsMenu.h"
#include "nsIMenuItem.h"
#include "nsIMenuListener.h"
#include "nsMenuItem.h"

#include <InterfaceDefs.h>
#include <Region.h>
#include <Debug.h>
#include <MenuBar.h>

#ifdef DRAG_DROP
//#include "nsDropTarget.h"
#include "DragDrop.h"
#include "DropTar.h"
#include "DropSrc.h"
#endif

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
	rgb_color	back = ui_color(B_PANEL_BACKGROUND_COLOR);

    NS_INIT_REFCNT();
    mView               = 0;
    mBackground         = NS_RGB(back.red, back.green, back.blue);
    mForeground         = NS_RGB(0x00,0x00,0x00);
    mIsDestroying       = PR_FALSE;
    mOnDestroyCalled    = PR_FALSE;
//    mTooltip            = NULL;
    mPreferredWidth     = 0;
    mPreferredHeight    = 0;
    mFont               = nsnull;
    mIsVisible          = PR_FALSE;
    mMenuBar            = nsnull;
    mMenuCmdId          = 0;
   
    mHitMenu            = nsnull;
    mHitSubMenus        = new nsVoidArray();
    mVScrollbar         = nsnull;

#ifdef DRAG_DROP
    mDragDrop           = nsnull;
    mDropTarget         = nsnull;
    mDropSource         = nsnull;
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

  // If the widget was released without calling Destroy() then the native
  // window still exists, and we need to destroy it
  if (NULL != mView) {
    Destroy();
  }

  NS_IF_RELEASE(mHitMenu); // this should always have already been freed by the deselect
#ifdef DRAG_DROP
  NS_IF_RELEASE(mDropTarget); 
  NS_IF_RELEASE(mDropSource); 
  if (mDragDrop)
    delete mDragDrop;
  //NS_IF_RELEASE(mDragDrop); 
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

NS_METHOD nsWindow::BeginResizingChildren(void)
{
	if(mView && mView->LockLooper())
	{
		mView->Window()->BeginViewTransaction();
		mView->UnlockLooper();
	}
	return NS_OK;
}

NS_METHOD nsWindow::EndResizingChildren(void)
{
	if(mView && mView->LockLooper())
	{
		mView->Window()->EndViewTransaction();
		mView->UnlockLooper();
	}
	return NS_OK;
}

// DoCreateTooltip - creates a tooltip control and adds some tools  
//     to it. 
// Returns the handle of the tooltip control if successful or NULL
//     otherwise. 
// hwndOwner - handle of the owner window 
// 

void nsWindow::AddTooltip(BView * hwndOwner,nsRect* aRect, int aId) 
{ 
#if 0
    TOOLINFO ti;    // tool information
    memset(&ti, 0, sizeof(TOOLINFO));
  
    // Make sure the common control DLL is loaded
    InitCommonControls(); 
 
    // Create a tooltip control for the window if needed
    if (mTooltip == (BWindow *) NULL) {
        mTooltip = CreateWindow(TOOLTIPS_CLASS, (LPSTR) NULL, TTS_ALWAYSTIP, 
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 
        NULL, (HMENU) NULL, 
        nsToolkit::mDllInstance,
        NULL);
    }
 
    if (mTooltip == (BWindow *) NULL) 
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
#endif
}

NS_METHOD nsWindow::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
	BPoint	point;
	point.x = aOldRect.x;
	point.y = aOldRect.y;
	if(mView && mView->LockLooper())
	{
		mView->ConvertToScreen(&point);
		mView->UnlockLooper();
	}
	aNewRect.x = nscoord(point.x);
	aNewRect.y = nscoord(point.y);
	aNewRect.width = aOldRect.width;
	aNewRect.height = aOldRect.height;
	return NS_OK;
}

NS_METHOD nsWindow::ScreenToWidget(const nsRect& aOldRect, nsRect& aNewRect)
{
	BPoint	point;
	point.x = aOldRect.x;
	point.y = aOldRect.y;
	if(mView && mView->LockLooper())
	{
		mView->ConvertFromScreen(&point);
		mView->UnlockLooper();
	}
	aNewRect.x = nscoord(point.x);
	aNewRect.y = nscoord(point.y);
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
		AddTooltip(mView, aTooltipAreas[i], i);
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
#if 0
  TOOLINFO ti;
  memset(&ti, 0, sizeof(TOOLINFO));
  ti.cbSize = sizeof(TOOLINFO);
  ti.hwnd = mView;
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
#endif
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Remove all tooltip rectangles
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::RemoveTooltips()
{
#if 0
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
    ti.hwnd = mView;
    ::SendMessage(mTooltip, TTM_DELTOOL, 0, (LPARAM) (LPTOOLINFO)&ti);
  }
#endif
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
		event.point.x = 0;
		event.point.y = 0;
#if 0
      DWORD pos = ::GetMessagePos();
      POINT cpos;

      cpos.x = LOWORD(pos);
      cpos.y = HIWORD(pos);

      if (mView != NULL) {
        ::ScreenToClient(mView, &cpos);
        event.point.x = cpos.x;
        event.point.y = cpos.y;
      } else {
        event.point.x = 0;
        event.point.y = 0;
      }
#endif
	}
	else {                      // use the point override if provided
		event.point.x = aPoint->x;
		event.point.y = aPoint->y;
	}
	event.time = PR_IntervalNow();
	event.message = aEventType;
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
// YK990522 was unused
//  nsWindow * thisPtr = this;
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

//WINOLEAPI oleStatus;
//-------------------------------------------------------------------------
//
// Utility method for implementing both Create(nsIWidget ...) and
// Create(nsNativeWidget...)
//-------------------------------------------------------------------------
#ifdef DRAG_DROP
BOOL gOLEInited = FALSE;
#endif

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

    // Switch to the "main gui thread" if necessary... This method must
    // be executed on the "gui thread"...
    //
  
    nsToolkit* toolkit = (nsToolkit *)mToolkit;
    if (toolkit) {
    if (!toolkit->IsGuiThread()) {
        uint32 args[7];
        args[0] = (uint32)aParent;
        args[1] = (uint32)&aRect;
        args[2] = (uint32)aHandleEventFunction;
        args[3] = (uint32)aContext;
        args[4] = (uint32)aAppShell;
        args[5] = (uint32)aToolkit;
        args[6] = (uint32)aInitData;

        if (nsnull != aParent) {
           // nsIWidget parent dispatch
          MethodInfo info(this, this, nsWindow::CREATE, 7, args);
          toolkit->CallMethod(&info);
           return NS_OK;
        }
        else {
            // Native parent dispatch
          MethodInfo info(this, this, nsWindow::CREATE_NATIVE, 5, args);
          toolkit->CallMethod(&info);
          return NS_OK;
        }
    }
    }
 
    BView *parent;
    if (nsnull != aParent) { // has a nsIWidget parent
      parent = ((aParent) ? (BView *)aParent->GetNativeData(NS_NATIVE_WINDOW) : nsnull);
    } else { // has a nsNative parent
       parent = (BView *)aNativeParent;
    }

    mView = CreateBeOSView();
	if(parent)
	{
		bool mustunlock;

		if(parent->LockLooper())
			mustunlock = true;

		parent->AddChild(mView);
		mView->MoveTo(aRect.x, aRect.y);
		mView->ResizeTo(aRect.width, GetHeight(aRect.height));

		if(mustunlock)
			parent->UnlockLooper();
	}
	else
	{
		// create window
		BRect winrect = BRect(aRect.x, aRect.y, aRect.x + aRect.width - 1, aRect.y + aRect.height - 1);
		winrect.OffsetBy( 10, 30 );
		nsWindowBeOS *w = new nsWindowBeOS(this,
				winrect,
				"", B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL, B_ASYNCHRONOUS_CONTROLS);
		w->AddChild(mView);

		// FIXME: we have to use the window size because
		// the window might not like sizes less then 30x30 or something like that
		mView->MoveTo(0, 0);
		mView->ResizeTo(w->Bounds().Width(), w->Bounds().Height());
		mView->SetResizingMode(B_FOLLOW_ALL);
		w->Run();
	}

#if 0
    // Initial Drag & Drop Work
#ifdef DRAG_DROP
   if (!gOLEInited) {
     DWORD dwVer = ::OleBuildVersion();

     if (FAILED(::OleInitialize(NULL))){
       printf("***** OLE has been initialized!\n");
     }
     gOLEInited = TRUE;
   }

   mDragDrop = new CfDragDrop();
   //mDragDrop->AddRef();
   mDragDrop->Initialize(this);

   /*mDropTarget = new CfDropTarget(*mDragDrop);
   mDropTarget->AddRef();

   mDropSource = new CfDropSource(*mDragDrop);
   mDropSource->AddRef();*/

   /*mDropTarget = new nsDropTarget(this);
   mDropTarget->AddRef();
   if (S_OK == ::CoLockObjectExternal((LPUNKNOWN)mDropTarget,TRUE,FALSE)) {
     if (S_OK == ::RegisterDragDrop(mView, (LPDROPTARGET)mDropTarget)) {

     }
   }*/
#endif
#endif

    // call the event callback to notify about creation
    DispatchStandardEvent(NS_CREATE);
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

BView *nsWindow::CreateBeOSView()
{
	return new nsViewBeOS(this, BRect(0, 0, 0, 0), "", 0, B_WILL_DRAW | B_FRAME_EVENTS);
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
    MethodInfo info(this, this, nsWindow::DESTROY);
    toolkit->CallMethod(&info);
    return NS_ERROR_FAILURE;
  }

  // disconnect from the parent
  if (!mIsDestroying) {
    nsBaseWidget::Destroy();
  }

	// destroy the BView
	if (mView)
	{
		// prevent the widget from causing additional events
		mEventCallback = nsnull;

		if(mView->LockLooper())
		{
			// destroy from inside
			BWindow	*w = mView->Window();
			if(mView->Parent())
			{
				mView->Parent()->RemoveChild(mView);
				w->Unlock();
			}
			else
			{
				w->RemoveChild(mView);
				w->Quit();
			}
		}

		// window is already gone
		while(mView->ChildAt(0))
			mView->RemoveChild(mView->ChildAt(0));
		delete mView;
		mView = NULL;

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
	nsIWidget	*widget = 0;
	BView		*parent;
	if(mView && (parent = mView->Parent()) != 0)
	{
		nsIWidgetStore *ws = dynamic_cast<nsIWidgetStore *>(parent);
		NS_ASSERTION(ws != 0, "view must be derived from nsIWidgetStore");
		if((widget = ws->GetMozillaWidget()) != 0)
		{
			// If the widget is in the process of being destroyed then
			// do NOT return it
			if(((nsWindow *)widget)->mIsDestroying)
				widget = 0;
			else
				NS_ADDREF(widget);
		}
	}
	
	return widget;
}


//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Show(PRBool bState)
{
	bool mustunlock = false;
	bool havewindow = false;

	if(mView)
	{
		if(mView->LockLooper())
			mustunlock = true;

		if(mustunlock && mView->Parent() == 0)
			havewindow = true;

		if(PR_FALSE == bState)
		{
			mView->Hide();
			if(havewindow)
				mView->Window()->Hide();
		}
		else
		{
			mView->Show();
			if(havewindow)
				mView->Window()->Show();
		}

		if(mustunlock)
			mView->UnlockLooper();
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



NS_METHOD nsWindow::Minimize(void)
{
  mView->Window()->Minimize(true);
  return NS_OK;
}

NS_METHOD nsWindow::Maximize(void)
{
  // IMPLIMENT ME! :)
  return NS_OK;
}


NS_METHOD nsWindow::Restore(void)
{
  mView->Window()->Minimize(false);
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Move(PRInt32 aX, PRInt32 aY)
{
	bool mustunlock = false;
	bool havewindow = false;

	mBounds.x = aX;
	mBounds.y = aY;

	if(mView)
	{
		if(mView->LockLooper())
			mustunlock = true;

		if(mustunlock && mView->Parent() == 0)
			havewindow = true;

		if(mView->Parent() || ! havewindow)
			mView->MoveTo(aX, aY);
		else
			mView->Window()->MoveTo(aX, aY);

		if(mustunlock)
			mView->UnlockLooper();
	}

	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
	bool mustunlock = false;
	bool havewindow = false;

	if(aWidth < 0 || aHeight < 0)
		return NS_OK;

	// Set cached value for lightweight and printing
	mBounds.width  = aWidth;
	mBounds.height = aHeight;

	if(mView)
	{
		if(mView->LockLooper())
			mustunlock = true;

		if(mustunlock && mView->Parent() == 0)
			havewindow = true;

		if(! aRepaint)
			printf("nsWindow::Resize FIXME: no repaint not implemented\n");

		if(mView->Parent() || ! havewindow)
			mView->ResizeTo(aWidth, GetHeight(aHeight));
		else
			mView->Window()->ResizeTo(aWidth, GetHeight(aHeight));

		if(mustunlock)
			mView->UnlockLooper();
	}

	return NS_OK;
}
    
//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Resize(PRInt32 aX,
                      PRInt32 aY,
                      PRInt32 aWidth,
                      PRInt32 aHeight,
                      PRBool   aRepaint)
{
	bool mustunlock = false;
	bool havewindow = false;

	if(aWidth < 0 || aHeight < 0)
		return NS_OK;

	// Set cached value for lightweight and printing
	mBounds.x = aX;
	mBounds.y = aY;
	mBounds.width  = aWidth;
	mBounds.height = aHeight;

	if(mView)
	{
		if(mView->LockLooper())
			mustunlock = true;

		if(mustunlock && mView->Parent() == 0)
			havewindow = true;

		if(! aRepaint)
			printf("nsWindow::Resize FIXME: no repaint not implemented\n");
		
		if(mView->Parent() || ! havewindow)
		{
			mView->MoveTo(aX, aY);
			mView->ResizeTo(aWidth, GetHeight(aHeight));
		}
		else
		{
			mView->Window()->MoveTo(aX, aY);
			mView->Window()->ResizeTo(aWidth, GetHeight(aHeight));
		}

		if(mustunlock)
			mView->UnlockLooper();
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
printf("nsWindow::Enable - FIXME: not implemented\n");
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
        MethodInfo info(this, this, nsWindow::SET_FOCUS);
        toolkit->CallMethod(&info);
        return NS_ERROR_FAILURE;
    }

	if(mView && mView->LockLooper())
	{
		mView->MakeFocus(true);
		mView->UnlockLooper();
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
	if(mView && mView->LockLooper())
	{
		BRect r = mView->Frame();
		aRect.x = nscoord(r.left);
		aRect.y = nscoord(r.top);
		aRect.width  = r.IntegerWidth();
		aRect.height = r.IntegerHeight();
		mView->UnlockLooper();
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
	if(mView && mView->LockLooper())
	{
		BRect r = mView->Bounds();
		aRect.x = nscoord(r.left);
		aRect.y = nscoord(r.top);
		aRect.width  = r.IntegerWidth();
		aRect.height = r.IntegerHeight();
		mView->UnlockLooper();
	} else {
		aRect.SetRect(0,0,0,0);
	}

	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set the background color
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetBackgroundColor(const nscolor &aColor)
{
    nsBaseWidget::SetBackgroundColor(aColor);

	if(mView && mView->LockLooper())
	{
		mView->SetViewColor(NS_GET_R(aColor), NS_GET_G(aColor), NS_GET_B(aColor), NS_GET_A(aColor));
		mView->UnlockLooper();
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
	BFont *font = (BFont*)fontHandle;
	if(font && mView && mView->LockLooper())
	{
		mView->SetFont(font, B_FONT_ALL);
		mView->UnlockLooper();
	}
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
//    HCURSOR newCursor = NULL;

    switch(aCursor) {
    case eCursor_select:
//      newCursor = ::LoadCursor(NULL, IDC_IBEAM);
      break;
      
    case eCursor_wait:
//      newCursor = ::LoadCursor(NULL, IDC_WAIT);
      break;

    case eCursor_hyperlink: {
//      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_SELECTANCHOR));
      break;
    }

    case eCursor_standard:
//      newCursor = ::LoadCursor(NULL, IDC_ARROW);
      break;

    case eCursor_sizeWE:
//      newCursor = ::LoadCursor(NULL, IDC_SIZEWE);
      break;

    case eCursor_sizeNS:
//      newCursor = ::LoadCursor(NULL, IDC_SIZENS);
      break;

    case eCursor_arrow_north:
//      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWNORTH));
      break;

    case eCursor_arrow_north_plus:
//      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWNORTHPLUS));
      break;

    case eCursor_arrow_south:
//      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWSOUTH));
      break;

    case eCursor_arrow_south_plus:
//      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWSOUTHPLUS));
      break;

    case eCursor_arrow_east:
//      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWEAST));
      break;

    case eCursor_arrow_east_plus:
//      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWEASTPLUS));
      break;

    case eCursor_arrow_west:
//      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWWEST));
      break;

    case eCursor_arrow_west_plus:
//      newCursor = ::LoadCursor(nsToolkit::mDllInstance, MAKEINTRESOURCE(IDC_ARROWWESTPLUS));
      break;

    default:
      NS_ASSERTION(0, "Invalid cursor type");
      break;
    }

#if 0
    if (NULL != newCursor) {
      mCursor = aCursor;
      HCURSOR oldCursor = ::SetCursor(newCursor);
    }
#endif
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
	if(mView && mView->LockLooper())
	{
   		if(PR_TRUE == aIsSynchronous)
			OnPaint(mBounds);
		else 
			mView->Invalidate();
  		mView->UnlockLooper();
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
	if(mView && mView->LockLooper())
	{
		BRect	r(aRect.x, aRect.y, aRect.x + aRect.width - 1, aRect.y + aRect.height - 1);
   		if(PR_TRUE == aIsSynchronous)
			mView->Draw(r);
		else 
			mView->Invalidate(r);
  		mView->UnlockLooper();
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
	if(mView && mView->LockLooper())
	{
		mView->Window()->UpdateIfNeeded();
		mView->UnlockLooper();
	}
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
        case NS_NATIVE_GRAPHIC:
            return (void *)((BView *)mView);
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
printf("nsWindow::SetColorMap - not implemented\n");
    return NS_OK;
}


//-------------------------------------------------------------------------
//
// Scroll the bits of a window
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect)
{
	BRect	src;
	BRect	dest;

	if(mView && mView->LockLooper())
	{
		BRect b = mView->Bounds();
		if(aClipRect)
		{
			src.left = aClipRect->x;
			src.top = aClipRect->y;
			src.right = aClipRect->XMost();
			src.bottom = aClipRect->YMost();
		}
		else
			src = b;

		BRegion	invalid;
		invalid.Include(src);

		// make sure we only reference visible bits
		// so we don't trigger a BView invalidate
		if(src.left + aDx < 0)
			src.left = -aDx;
		if(src.right + aDy > b.right)
			src.right = b.right - aDy;
		if(src.top + aDy < 0)
			src.top = -aDy;
		if(src.bottom + aDy > b.bottom)
			src.bottom = b.bottom - aDy;

		dest = src;
		dest.left += aDx;
		dest.right += aDx;
		dest.top += aDy;
		dest.bottom += aDy;

		mView->ConstrainClippingRegion(0);
		if(src.IsValid() && dest.IsValid())
			mView->CopyBits(src, dest);

		invalid.Exclude(dest);

		for(BView *child = mView->ChildAt(0); child; child = child->NextSibling())
			child->MoveBy(aDx, aDy);

		// scan through rects and paint them directly
		// so we avoid going through the callback stuff
		int32 rects = invalid.CountRects();
		for(int32 i = 0; i < rects; i++)
		{
			BRect	curr = invalid.RectAt(i);
			nsRect	r;
			r.x = (nscoord)curr.left;
			r.y = (nscoord)curr.top;
			r.width = (nscoord)curr.Width() + 1;
			r.height = (nscoord)curr.Height() + 1;
			OnPaint(r);
		}

		mView->UnlockLooper();
	}

	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Every function that needs a thread switch goes through this function
// by calling SendMessage (..WM_CALLMETHOD..) in nsToolkit::CallMethod.
//
//-------------------------------------------------------------------------
bool nsWindow::CallMethod(MethodInfo *info)
{
    bool bRet = TRUE;

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

		case nsWindow::CLOSEWINDOW :
            NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
			DispatchStandardEvent(NS_DESTROY);
			break;

        case nsWindow::SET_FOCUS:
            NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
            SetFocus();
            break;

		case nsWindow::ONMOUSE :
            NS_ASSERTION(info->nArgs == 5, "Wrong number of arguments to CallMethod");
			DispatchMouseEvent(((int32 *)info->args)[0],
				nsPoint(((int32 *)info->args)[1], ((int32 *)info->args)[2]),
				((int32 *)info->args)[3],
				((int32 *)info->args)[4]);
			break;

		case nsWindow::ONKEY :
            NS_ASSERTION(info->nArgs == 4, "Wrong number of arguments to CallMethod");
			OnKey(((int32 *)info->args)[0],
				(const char *)(&((uint32 *)info->args)[1]), ((int32 *)info->args)[2],
				((uint32 *)info->args)[3]);
			break;

		case nsWindow::ONPAINT :
			NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
			if(mView && mView->LockLooper())
			{
				nsRect r;
				nsViewBeOS	*bv = dynamic_cast<nsViewBeOS *>(mView);
				if(bv && bv->GetPaintRect(r))
					OnPaint(r);
				mView->UnlockLooper();
			}
			break;

		case nsWindow::ONRESIZE :
			NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
			if(mView && mView->LockLooper())
			{
				nsRect r;
				nsViewBeOS	*bv = dynamic_cast<nsViewBeOS *>(mView);
				if(bv && bv->GetSizeRect(r))
				{
					// have to disable frame events otherwise we'll be notified
					// of the mozilla internals forced resize
					mView->SetFlags(mView->Flags() & ~B_FRAME_EVENTS);
					OnResize(r);
					mView->SetFlags(mView->Flags() | B_FRAME_EVENTS);
				}
				mView->UnlockLooper();
			}
			break;

		case nsWindow::ONSCROLL:
			NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
			OnScroll();
			break;

		case nsWindow::BTNCLICK :
            NS_ASSERTION(info->nArgs == 5, "Wrong number of arguments to CallMethod");
			DispatchMouseEvent(((int32 *)info->args)[0],
				nsPoint(((int32 *)info->args)[1], ((int32 *)info->args)[2]),
				((int32 *)info->args)[3],
				((int32 *)info->args)[4]);
			break;

		case nsWindow::MENU :
			NS_ASSERTION(info->nArgs == 1, "Wrong number of arguments to CallMethod");
			{
				nsIMenuListener *menuListener = nsnull;
				nsIMenuItem *menuItem = (nsIMenuItem *)(info->args[0]);

				nsMenuEvent mevent;
				mevent.message = NS_MENU_SELECTED;
				mevent.eventStructType = NS_MENU_EVENT;
				mevent.point.x = 0;
				mevent.point.y = 0;
				menuItem->GetTarget(mevent.widget);
				menuItem->GetCommand(mevent.mCommand);

				mevent.mMenuItem = menuItem;
				mevent.time = PR_IntervalNow();

				//nsEventStatus status;
				// FIXME - THIS SHOULD WORK.  FIX EVENTS FOR XP CODE!!!!! (pav)
				//    mevent.widget->DispatchEvent((nsGUIEvent *)&mevent, status);

				menuItem->QueryInterface(kIMenuListenerIID, (void**)&menuListener);
				if(menuListener)
				{
					menuListener->MenuSelected(mevent);
					NS_IF_RELEASE(menuListener);
				}
			}
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
PRBool nsWindow::OnKey(PRUint32 aEventType, const char *bytes, int32 numBytes, PRUint32 mod)
{
	if(numBytes == 1)
	{
		nsKeyEvent event;
		InitEvent(event, aEventType);

printf("nsWindow::OnKey - FIXME: keycode translation incomplete\n");
		event.charCode   = bytes[0];

		char c = bytes[0];
		if(numBytes == 1 && (c == 10 || c == 13))
			c = NS_VK_RETURN;

		event.keyCode  = c;
		event.isShift   = mod & B_SHIFT_KEY;
		event.isControl = mod & B_CONTROL_KEY;
		event.isAlt     = mod & B_COMMAND_KEY;
		event.eventStructType = NS_KEY_EVENT;

		PRBool result = DispatchWindowEvent(&event);
		NS_RELEASE(event.widget);
		return result;
	}
	else
	{
		printf("nsWindow::OnKey - FIXME: handle non ASCII chars\n");
		return NS_OK;
	}
}

#if 0
//-------------------------------------------------------------------------
//
// OnKey
//
//-------------------------------------------------------------------------

PRBool nsWindow::OnKey(PRUint32 aEventType, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
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

  // Now let windows do the conversion to the ascii code
  WORD asciiChar = 0;
  BYTE kbstate[256];
  ::GetKeyboardState(kbstate);
  ToAscii(nChar, nFlags & 0xff, kbstate, &asciiChar, 0);

  event.keyCode = nChar;
  event.charCode = (char)asciiChar;

  event.isShift   = mIsShiftDown;
  event.isControl = mIsControlDown;
  event.isAlt     = mIsAltDown;
  event.eventStructType = NS_KEY_EVENT;

  PRBool result = DispatchWindowEvent(&event);
  NS_RELEASE(event.widget);
  return result;
}
#endif

#if 0
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


//-------------------------------------------------------------------------
nsresult nsWindow::MenuHasBeenSelected(HMENU aNativeMenu, UINT aItemNum, UINT aFlags, UINT aCommand)
{
  nsMenuEvent event;
  event.mCommand = aCommand;
  event.eventStructType = NS_MENU_EVENT;
  InitEvent(event, NS_MENU_SELECTED);

  // The MF_POPUP flag tells us if we are a menu item or a menu
  // the aItemNum is either the command ID of the menu item or 
  // the position of the menu as a child pf its parent
  PRBool isMenuItem = !(aFlags & MF_POPUP);

  // uItem is the position of the item that was clicked
  // aNativeMenu is a handle to the menu that was clicked

  // if aNativeMenu is NULL then the menu is being deselected
  if (!aNativeMenu) {
    printf("///////////// Menu is NULL!\n");
    // check to make sure something had been selected
    AdjustMenus(mHitMenu, nsnull, event);
    NS_IF_RELEASE(mHitMenu);
    // Clear All SubMenu items
    while (mHitSubMenus->Count() > 0) {
      PRUint32 inx = mHitSubMenus->Count()-1;
      nsIMenu * menu = (nsIMenu *)mHitSubMenus->ElementAt(inx);
      AdjustMenus(menu, nsnull, event);
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
        AdjustMenus(mHitMenu, hitMenu, event);
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
          AdjustMenus(m, nsnull, event);
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
          AdjustMenus(nsnull, newMenu, event);
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
#endif

//---------------------------------------------------------
NS_METHOD nsWindow::EnableFileDrop(PRBool aEnable)
{
#if 0
  ::DragAcceptFiles(mView, (aEnable?TRUE:FALSE));
#endif
  return NS_OK;
}

#if 0
//-------------------------------------------------------------------------
//
// Process all nsWindows messages
//
//-------------------------------------------------------------------------
PRBool nsWindow::ProcessMessage(UINT msg, WPARAM wParam, LPARAM lParam, LRESULT *aRetValue)
{
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

        case WM_KEYUP: 

            mIsShiftDown   = IS_VK_DOWN(NS_VK_SHIFT);
            mIsControlDown = IS_VK_DOWN(NS_VK_CONTROL);
            mIsAltDown     = IS_VK_DOWN(NS_VK_ALT);
            {
            LONG data = (LONG)lParam;
            LONG newdata = (data & 0x00FF00);
            //LONG newdata2 = (data & 0xFFFF00F);
            int x = 0;
            }
            result = OnKey(NS_KEY_UP, wParam, LOWORD(lParam), HIWORD(lParam));
            break;

        case WM_KEYDOWN:

            mIsShiftDown   = IS_VK_DOWN(NS_VK_SHIFT);
            mIsControlDown = IS_VK_DOWN(NS_VK_CONTROL);
            mIsAltDown     = IS_VK_DOWN(NS_VK_ALT);

            result = OnKey(NS_KEY_DOWN, wParam, LOWORD(lParam), HIWORD(lParam));
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
                nsWindow* scrollbar = (nsWindow*)::GetWindowLong((BWindow *)lParam, GWL_USERDATA);

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
              nsWindow* control = (nsWindow*)::GetWindowLong((BWindow *)lParam, GWL_USERDATA);
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
              ::GetWindowRect(mView, &r);
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

//                ::InvalidateRect(mView, NULL, FALSE);
//                ::InvalidateRect(mView, &drect, FALSE);
                ::RedrawWindow(mView, &drect, NULL,
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

//                ::InvalidateRect(mView, NULL, FALSE);
//                ::InvalidateRect(mView, &drect, FALSE);
                ::RedrawWindow(mView, &drect, NULL,
                               RDW_INVALIDATE | RDW_NOERASE | RDW_NOINTERNALPAINT | RDW_ERASENOW | RDW_ALLCHILDREN);
              }
              mBounds.width  = newWidth;
              mBounds.height = newHeight;
              ///nsRect rect(wp->x, wp->y, wp->cx, wp->cy);

              // recalculate the width and height
              // this time based on the client area
              if (::GetClientRect(mView, &r)) {
                rect.width  = PRInt32(r.right - r.left);
                rect.height = PRInt32(r.bottom - r.top);
              }
              result = OnResize(rect);
            }
            break;
        }
#if 0 // these are needed for now
        case WM_INITMENU: {
          printf("WM_INITMENU\n");
          } break;

        case WM_INITMENUPOPUP: {
          printf("WM_INITMENUPOPUP\n");
          } break;
#endif

#if 0
        case WM_MENUSELECT: 
          if (mMenuBar) {
            MenuHasBeenSelected((HMENU)lParam, (UINT)LOWORD(wParam), (UINT)HIWORD(wParam), (UINT) LOWORD(wParam));
          }
          break;
#endif

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
         BWindow * scrollbar = NULL;
         if (nsnull != mVScrollbar) {
           scrollbar = (BWindow *)mVScrollbar->GetNativeData(NS_NATIVE_WINDOW);
         }
         if (scrollbar) {
            if (iDeltaPerLine == 0)
              break ;

            iAccumDelta += (short) HIWORD (wParam) ;     // 120 or -120

            while (iAccumDelta >= iDeltaPerLine) {    
              //printf("iAccumDelta %d\n", iAccumDelta);
              SendMessage (mView, WM_VSCROLL, SB_LINEUP, (LONG)scrollbar) ;
              iAccumDelta -= iDeltaPerLine ;
            }

            while (iAccumDelta <= -iDeltaPerLine) {
              //printf("iAccumDelta %d\n", iAccumDelta);
              SendMessage (mView, WM_VSCROLL, SB_LINEDOWN, (LONG)scrollbar) ;
              iAccumDelta += iDeltaPerLine ;
            }
         }
            return 0 ;
      } break;


#if 0
        case WM_PALETTECHANGED:
            if ((BWindow *)wParam == mView) {
                // We caused the WM_PALETTECHANGED message so avoid realizing
                // another foreground palette
                result = PR_TRUE;
                break;
            }
            // fall thru...

        case WM_QUERYNEWPALETTE:
            mContext->GetPaletteInfo(palInfo);
            if (palInfo.isPaletteDevice && palInfo.palette) {
                HDC hDC = ::GetDC(mView);
                HPALETTE hOldPal = ::SelectPalette(hDC, (HPALETTE)palInfo.palette, FALSE);
                
                // Realize the drawing palette
                int i = ::RealizePalette(hDC);

                // Did any of our colors change?
                if (i > 0) {
                  // Yes, so repaint
                  ::InvalidateRect(mView, (LPRECT)NULL, TRUE);
                }

                ::SelectPalette(hDC, hOldPal, TRUE);
                ::RealizePalette(hDC);
                ::ReleaseDC(mView, hDC);
                *aRetValue = TRUE;
            }
            result = PR_TRUE;
            break;

        case WM_DROPFILES: {
          HDROP hDropInfo = (HDROP) wParam;
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
	        }
        } break;
#endif
    }

    return result;
}
#endif


//-------------------------------------------------------------------------
//
// WM_DESTROY has been called
//
//-------------------------------------------------------------------------
void nsWindow::OnDestroy()
{
    mOnDestroyCalled = PR_TRUE;

#if 0
    // free tooltip window
    if (mTooltip) {
      VERIFY(::DestroyWindow(mTooltip));
      mTooltip = NULL;
    }
#endif

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
PRBool nsWindow::OnPaint(nsRect &r)
{
	PRBool result = PR_TRUE;

	if((r.width || r.height) && mEventCallback)
	{
		if(mView && mView->LockLooper())
		{
			// set clipping
			BRegion invalid;
			invalid.Include(BRect(r.x, r.y, r.x + r.width - 1, r.y + r.height - 1));
			mView->ConstrainClippingRegion(&invalid);

            nsPaintEvent event;

            InitEvent(event, NS_PAINT);
            event.rect = &r;
            event.eventStructType = NS_PAINT_EVENT;

            static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
            static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);

            if (NS_OK == nsComponentManager::CreateInstance(kRenderingContextCID, nsnull, kRenderingContextIID, (void **)&event.renderingContext))
            {
              event.renderingContext->Init(mContext, this);
              result = DispatchWindowEvent(&event);

              NS_RELEASE(event.renderingContext);
            }
            else
              result = PR_FALSE;

            NS_RELEASE(event.widget);

			mView->UnlockLooper();
        }
    }

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
	if (mEventCallback)
	{
		nsSizeEvent event;
		InitEvent(event, NS_SIZE);
		event.windowSize = &aWindowRect;
		event.eventStructType = NS_SIZE_EVENT;
		if(mView && mView->LockLooper())
		{
			BRect r = mView->Bounds();
			event.mWinWidth  = PRInt32(r.right - r.left);
			event.mWinHeight = PRInt32(r.bottom - r.top);
			mView->UnlockLooper();
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
PRBool nsWindow::DispatchMouseEvent(PRUint32 aEventType, nsPoint aPoint, PRUint32 clicks, PRUint32 mod)
{
	if(nsnull != mEventCallback || nsnull != mMouseListener)
	{
		nsMouseEvent event;
		InitEvent(event, aEventType, &aPoint);
		event.isShift   = mod & B_SHIFT_KEY;
		event.isControl = mod & B_CONTROL_KEY;
		event.isAlt     = mod & B_COMMAND_KEY;
		event.clickCount = clicks;
		event.eventStructType = NS_MOUSE_EVENT;

		// call the event callback
		if(nsnull != mEventCallback)
			return DispatchWindowEvent(&event);
		else
		{
			switch(aEventType)
			{
				case NS_MOUSE_MOVE :
					mMouseListener->MouseMoved(event);
					break;
	
				case NS_MOUSE_LEFT_BUTTON_DOWN :
				case NS_MOUSE_MIDDLE_BUTTON_DOWN :
				case NS_MOUSE_RIGHT_BUTTON_DOWN :
					mMouseListener->MousePressed(event);
					break;
	
				case NS_MOUSE_LEFT_BUTTON_UP :
				case NS_MOUSE_MIDDLE_BUTTON_UP :
				case NS_MOUSE_RIGHT_BUTTON_UP :
					mMouseListener->MouseReleased(event);
					mMouseListener->MouseClicked(event);
					break;
			}
		}
		NS_RELEASE(event.widget);
	}

	return PR_FALSE;
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
        return(DispatchStandardEvent(aEventType));
    }

    return PR_FALSE;
}


//-------------------------------------------------------------------------
//
// Deal with scrollbar messages (actually implemented only in nsScrollbar)
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnScroll()
{
    return PR_FALSE;
}

NS_METHOD nsWindow::SetTitle(const nsString& aTitle) 
{
	const char *text = aTitle.ToNewCString();
	if(text && mView->LockLooper())
	{
		mView->Window()->SetTitle(text);
		mView->UnlockLooper();
	}
	delete [] text;
	return NS_OK;
} 


PRBool nsWindow::AutoErase()
{
  return(PR_FALSE);
}

NS_METHOD nsWindow::SetMenuBar(nsIMenuBar * aMenuBar) 
{
	if(mMenuBar == aMenuBar)
	{
		// Ignore duplicate calls
		return NS_OK;
	}

	if(mMenuBar)
	{
		// Get rid of the old menubar
printf("nsWindow::SetMenuBar - FIXME: Get rid of the old menubar!\n");
//		GtkWidget* oldMenuBar;
//		mMenuBar->GetNativeData((void*&) oldMenuBar);
//		if (oldMenuBar) {
//			gtk_container_remove(GTK_CONTAINER(mVBox), oldMenuBar);
//		}
		NS_RELEASE(mMenuBar);
	}
	
	mMenuBar = aMenuBar;

	if(aMenuBar)
	{
	    NS_ADDREF(mMenuBar);
		BMenuBar *menubar;
		void *data;
		aMenuBar->GetNativeData(data);
		menubar = (BMenuBar *)data;
	
		if(mView && mView->LockLooper())
		{
			mView->Window()->AddChild(menubar);
			float sz = menubar->Bounds().Height() + 1;
	
			// FIXME: this is probably not correct, but seems to work ok;
			// I think only the first view should be moved/resized...
			for(BView *v = mView->Window()->ChildAt(0); v; v = v->NextSibling())
				if(v != menubar)
				{
					v->ResizeBy(0, -sz);
					v->MoveBy(0, sz);
				}
			mView->UnlockLooper();
		}
	}

	return NS_OK;
} 

NS_METHOD nsWindow::ShowMenuBar(PRBool aShow)
{
printf("nsWindow::ShowMenuBar - FIXME: not implemented!\n");
//  if (!mMenuBar)
//    //    return NS_ERROR_FAILURE;
//    return NS_OK;
//
//  GtkWidget *menubar;
//  void *voidData;
//  mMenuBar->GetNativeData(voidData);
//  menubar = GTK_WIDGET(voidData);
//  
//  if (aShow == PR_TRUE)
//    gtk_widget_show(menubar);
//  else
//    gtk_widget_hide(menubar);
//
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

//----------------------------------------------------
// Special Sub-Class
//----------------------------------------------------
nsIWidgetStore::nsIWidgetStore( nsIWidget *aWidget )
	: mWidget( aWidget )
{
	NS_ADDREF(mWidget);
}

nsIWidgetStore::~nsIWidgetStore()
{
	NS_RELEASE(mWidget);
}

nsIWidget *nsIWidgetStore::GetMozillaWidget(void)
{
	return mWidget;
}

//----------------------------------------------------
// BeOS Sub-Class Window
//----------------------------------------------------

nsWindowBeOS::nsWindowBeOS( nsIWidget *aWidgetWindow, BRect aFrame, const char *aName, window_look aLook,
    window_feel aFeel, int32 aFlags, int32 aWorkspace )
  : BWindow( aFrame, aName, aLook, aFeel, aFlags, aWorkspace ),
    nsIWidgetStore( aWidgetWindow )
{
}

bool nsWindowBeOS::QuitRequested( void )
{
	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0)
	{
		if(ChildAt(0))
			RemoveChild(ChildAt(0));
		MethodInfo *info = new MethodInfo(w, w, nsWindow::CLOSEWINDOW);
		t->CallMethodAsync(info);
		NS_RELEASE(t);
	}
	return true;
}

void nsWindowBeOS::MessageReceived(BMessage *msg)
{
	switch(msg->what)
	{
		case 'menu' :
			{
			nsIMenuItem *menuItem;
			if(msg->FindPointer("nsMenuItem", (void **)&menuItem) == B_OK &&
				menuItem != NULL)
			{
				nsWindow	*w = (nsWindow *)GetMozillaWidget();
				nsToolkit	*t;
				if(w && (t = w->GetToolkit()) != 0)
				{
					uint32	args[1];
					args[0] = (uint32)menuItem;
					MethodInfo *info = new MethodInfo(w, w, nsWindow::MENU, 1, args);
					t->CallMethodAsync(info);
					NS_RELEASE(t);
				}
			}
			}
			break;

		default :
			BWindow::MessageReceived(msg);
			break;
	}
}

//----------------------------------------------------
// BeOS Sub-Class View
//----------------------------------------------------

nsViewBeOS::nsViewBeOS(nsIWidget *aWidgetWindow, BRect aFrame, const char *aName, uint32 aResizingMode, uint32 aFlags)
 : BView(aFrame, aName, aResizingMode, aFlags), nsIWidgetStore(aWidgetWindow), buttons(0), currsizechanged(false)
{
}

void nsViewBeOS::AttachedToWindow()
{
	nsWindow *w = (nsWindow *)GetMozillaWidget();
	SetHighColor(255, 255, 255);
	FillRect(Bounds());
	if(! w->AutoErase())
		// view shouldn't erase
		SetViewColor(B_TRANSPARENT_32_BIT);
}

void nsViewBeOS::Draw(BRect updateRect)
{
	paintregion.Include(updateRect);
	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0)
	{
		MethodInfo *info = new MethodInfo(w, w, nsWindow::ONPAINT);
		t->CallMethodAsync(info);
		NS_RELEASE(t);
	}
}

bool nsViewBeOS::GetPaintRect(nsRect &r)
{
	if(paintregion.CountRects() == 0)
		return false;
	BRect	paint = paintregion.Frame();
	r.x = (nscoord)paint.left;
	r.y = (nscoord)paint.top;
	r.width = (nscoord)(paint.Width() + 1);
	r.height = (nscoord)(paint.Height() + 1);
	paintregion.MakeEmpty();
	return true;
}

void nsViewBeOS::FrameResized(float width, float height)
{
	currsizerect.x = (nscoord)Bounds().left;
	currsizerect.y = (nscoord)Bounds().top;
	currsizerect.width = (nscoord)width;
	currsizerect.height = (nscoord)height;
	currsizechanged = true;

	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0)
	{
		MethodInfo *info = new MethodInfo(w, w, nsWindow::ONRESIZE);
		t->CallMethodAsync(info);
		NS_RELEASE(t);
	}
}

bool nsViewBeOS::GetSizeRect(nsRect &r)
{
	if(! currsizechanged)
		return false;
	r = currsizerect;
	currsizechanged = false;
	return true;
}

void nsViewBeOS::MouseDown(BPoint point)
{
	SetMouseEventMask(B_POINTER_EVENTS | B_KEYBOARD_EVENTS);

	uint32 clicks = 0;
	Window()->CurrentMessage()->FindInt32("buttons", (int32 *)&buttons);
	Window()->CurrentMessage()->FindInt32("clicks", (int32 *)&clicks);

	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0)
	{
		if(buttons & (B_PRIMARY_MOUSE_BUTTON | B_SECONDARY_MOUSE_BUTTON | B_TERTIARY_MOUSE_BUTTON))
		{
			int32 ev = (buttons & B_PRIMARY_MOUSE_BUTTON) ? NS_MOUSE_LEFT_BUTTON_DOWN :
						((buttons & B_SECONDARY_MOUSE_BUTTON) ? NS_MOUSE_RIGHT_BUTTON_DOWN :
						NS_MOUSE_MIDDLE_BUTTON_DOWN);
			uint32	args[5];
			args[0] = ev;
			args[1] = (uint32)point.x;
			args[2] = (uint32)point.y;
			args[3] = clicks;
			args[4] = modifiers();
			MethodInfo *info = new MethodInfo(w, w, nsWindow::ONMOUSE, 5, args);
			t->CallMethodAsync(info);
		}
		NS_RELEASE(t);
	}
}

void nsViewBeOS::MouseMoved(BPoint point, uint32 transit, const BMessage *message)
{
	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0)
	{
		uint32	args[5];
		args[1] = (uint32)point.x;
		args[2] = (uint32)point.y;
		args[3] = 0;
		args[4] = modifiers();
	
		if(transit == B_ENTERED_VIEW)
		{
			args[0] = NS_MOUSE_ENTER;
			MethodInfo *info = new MethodInfo(w, w, nsWindow::ONMOUSE, 5, args);
			t->CallMethodAsync(info);
		}
	
		args[0] = NS_MOUSE_MOVE;
		MethodInfo *info = new MethodInfo(w, w, nsWindow::ONMOUSE, 5, args);
		t->CallMethodAsync(info);
	
		if(transit == B_EXITED_VIEW)
		{
			args[0] = NS_MOUSE_EXIT;
			MethodInfo *info = new MethodInfo(w, w, nsWindow::ONMOUSE, 5, args);
			t->CallMethodAsync(info);
		}
		NS_RELEASE(t);
	}
}

void nsViewBeOS::MouseUp(BPoint point)
{
	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0)
	{
		if(buttons & (B_PRIMARY_MOUSE_BUTTON | B_SECONDARY_MOUSE_BUTTON | B_TERTIARY_MOUSE_BUTTON))
		{
			int32 ev = (buttons & B_PRIMARY_MOUSE_BUTTON) ? NS_MOUSE_LEFT_BUTTON_UP :
						((buttons & B_SECONDARY_MOUSE_BUTTON) ? NS_MOUSE_RIGHT_BUTTON_UP :
						NS_MOUSE_MIDDLE_BUTTON_UP);
			uint32	args[5];
			args[0] = ev;
			args[1] = (uint32)point.x;
			args[2] = (int32)point.y;
			args[3] = 0;
			args[4] = modifiers();
			MethodInfo *info = new MethodInfo(w, w, nsWindow::ONMOUSE, 5, args);
			t->CallMethodAsync(info);
		}
		NS_RELEASE(t);
	}
	buttons = 0;
}

void nsViewBeOS::KeyDown(const char *bytes, int32 numBytes)
{
	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0)
	{
		uint32 bytebuf = 0;
		uint8 *byteptr = (uint8 *)&bytebuf;
		for(int32 i = 0; i < numBytes; i++)
			byteptr[i] = bytes[i];

		uint32	args[4];
		args[0] = NS_KEY_DOWN;
		args[1] = bytebuf;
		args[2] = numBytes;
		args[3] = modifiers();

		MethodInfo *info = new MethodInfo(w, w, nsWindow::ONKEY, 4, args);
		t->CallMethodAsync(info);
	}
}

void nsViewBeOS::KeyUp(const char *bytes, int32 numBytes)
{
	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0)
	{
		uint32 bytebuf = 0;
		uint8 *byteptr = (uint8 *)&bytebuf;
		for(int32 i = 0; i < numBytes; i++)
			byteptr[i] = bytes[i];

		uint32	args[4];
		args[0] = NS_KEY_UP;
		args[1] = (int32)bytebuf;
		args[2] = numBytes;
		args[3] = modifiers();

		MethodInfo *info = new MethodInfo(w, w, nsWindow::ONKEY, 4, args);
		t->CallMethodAsync(info);
	}
}
