/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsDebug.h"
#include "nsWindow.h"
#include "nsIAppShell.h"
#include "nsIFontMetrics.h"
#include "nsFont.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsIDeviceContext.h"
#include "nsRect.h"
#include "nsIRegion.h"
#include "nsTransform2D.h"
//#include "sysmets.h"
#include "nsGfxCIID.h"
#include "resource.h"
#include "prtime.h"
#include "nsReadableUtils.h"

#include <InterfaceDefs.h>
#include <Region.h>
#include <Debug.h>
#include <MenuBar.h>
#include <app/Message.h>
#include <app/MessageRunner.h>
#include <support/String.h>

#ifdef DRAG_DROP
//#include "nsDropTarget.h"
#include "DragDrop.h"
#include "DropTar.h"
#include "DropSrc.h"
#endif

#include "nsIRollupListener.h"
#include "nsIMenuRollup.h"

////////////////////////////////////////////////////
// Rollup Listener - static variable defintions
////////////////////////////////////////////////////
static nsIRollupListener * gRollupListener           = nsnull;
static nsIWidget         * gRollupWidget             = nsnull;
static PRBool              gRollupConsumeRollupEvent = PR_FALSE;
////////////////////////////////////////////////////

static NS_DEFINE_IID(kIWidgetIID,       NS_IWIDGET_IID);

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
//    mVScrollbar         = nsnull;

    mWindowType         = eWindowType_child;
    mBorderStyle        = eBorderStyle_default;
    mBorderlessParent   = 0;

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
  
  nsCOMPtr <nsIWidget> mWidget = event->widget;
  
  if (mEventCallback)
    aStatus = (*mEventCallback)(event);

  if ((aStatus != nsEventStatus_eIgnore) && (mEventListener))
    aStatus = mEventListener->ProcessEvent(*event);
  
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
  nsIWidget *baseParent = aInitData &&
                          (aInitData->mWindowType == eWindowType_dialog ||
                          aInitData->mWindowType == eWindowType_toplevel) ?
                            nsnull : aParent;
	
  mIsTopWidgetWindow = (nsnull == baseParent);

  BaseCreate(baseParent, aRect, aHandleEventFunction, aContext, 
             aAppShell, aToolkit, aInitData);

  // Switch to the "main gui thread" if necessary... This method must
  // be executed on the "gui thread"...
  //
  
  nsToolkit* toolkit = (nsToolkit *)mToolkit;
  if (toolkit) 
  {
    if (!toolkit->IsGuiThread()) 
    {
      uint32 args[7];
      args[0] = (uint32)aParent;
      args[1] = (uint32)&aRect;
      args[2] = (uint32)aHandleEventFunction;
      args[3] = (uint32)aContext;
      args[4] = (uint32)aAppShell;
      args[5] = (uint32)aToolkit;
      args[6] = (uint32)aInitData;

      if (nsnull != aParent) 
      {
        // nsIWidget parent dispatch
        MethodInfo info(this, this, nsWindow::CREATE, 7, args);
        toolkit->CallMethod(&info);
        return NS_OK;
      }
      else 
      {
        // Native parent dispatch
        MethodInfo info(this, this, nsWindow::CREATE_NATIVE, 5, args);
        toolkit->CallMethod(&info);
        return NS_OK;
      }
    }
  }
 
  BView *parent;
  if (nsnull != aParent) // has a nsIWidget parent
  {
    parent = ((aParent) ? (BView *)aParent->GetNativeData(NS_NATIVE_WINDOW) : nsnull);
  } 
  else // has a nsNative parent 
  { 
    parent = (BView *)aNativeParent;
  }

  if (nsnull != aInitData) 
  {
    SetWindowType(aInitData->mWindowType);
    SetBorderStyle(aInitData->mBorderStyle);
  }

// NEED INPLEMENT
//  DWORD style = WindowStyle();
// NEED INPLEMENT
//  DWORD extendedStyle = WindowExStyle();

  mBorderlessParent = NULL;
  if (mWindowType == eWindowType_popup) 
  {
    mBorderlessParent = parent;
    // Don't set the parent of a popup window. 
    parent = NULL;
  } 
  else if (nsnull != aInitData) 
  {
    // See if the caller wants to explictly set clip children and clip siblings
    if (aInitData->clipChildren) 
    {
// NEED INPLEMENT
//  style |= WS_CLIPCHILDREN;
    } 
    else 
    {
//		NEED INPLEMENT
//        style &= ~WS_CLIPCHILDREN;
    }
    
    if (aInitData->clipSiblings) 
    {
//		NEED INPLEMENT
//        style |= WS_CLIPSIBLINGS;
    }
  }

  mView = CreateBeOSView();
  if(mView)
  {
    if (mWindowType == eWindowType_dialog) 
    {
      // create window (dialog)
      bool is_subset = (parent)? true : false;
      
      // see bugzilla bug 66809 regarding this change -wade <guru@startrek.com>
      //window_feel feel = (is_subset) ? B_MODAL_SUBSET_WINDOW_FEEL : B_MODAL_APP_WINDOW_FEEL;
      window_feel feel = B_NORMAL_WINDOW_FEEL;
      
      BRect winrect = BRect(aRect.x, aRect.y, aRect.x + aRect.width - 1, aRect.y + aRect.height - 1);
      
      winrect.OffsetBy( 10, 30 );
      
      nsWindowBeOS *w = new nsWindowBeOS(this, winrect,	"", B_TITLED_WINDOW_LOOK, feel,
                                         B_ASYNCHRONOUS_CONTROLS);
      if(w)
      {
        w->AddChild(mView);
        if (is_subset) 
        {
          w->AddToSubset(parent->Window());
        }

        // FIXME: we have to use the window size because
        // the window might not like sizes less then 30x30 or something like that
        mView->MoveTo(0, 0);
        mView->ResizeTo(w->Bounds().Width(), w->Bounds().Height());
        mView->SetResizingMode(B_FOLLOW_ALL);
      }
    }
    else if (mWindowType == eWindowType_child) 
    {
      // create view only
      bool mustunlock=false;

      if(parent->LockLooper())
      {
        mustunlock = true;
      }
      
      parent->AddChild(mView);
      mView->MoveTo(aRect.x, aRect.y);
      mView->ResizeTo(aRect.width-1, GetHeight(aRect.height)-1);

      if(mustunlock)
      {
        parent->UnlockLooper();
      }
    } 
    else 
    {
      // create window (normal or popup)
      BRect winrect = BRect(aRect.x, aRect.y, 
                            aRect.x + aRect.width - 1, aRect.y + aRect.height - 1);
      nsWindowBeOS *w;

      if (mWindowType == eWindowType_popup) 
      {
        bool is_subset = (mBorderlessParent)? true : false;
        window_feel feel = (is_subset) ? B_FLOATING_SUBSET_WINDOW_FEEL : B_FLOATING_APP_WINDOW_FEEL;
        
        w = new nsWindowBeOS(this, winrect, "", B_NO_BORDER_WINDOW_LOOK, feel,
                             B_NOT_CLOSABLE | B_AVOID_FOCUS | B_ASYNCHRONOUS_CONTROLS
                             | B_NO_WORKSPACE_ACTIVATION);
        if (w)
        {
          // popup window : no border
          if (is_subset) 
          {
            w->AddToSubset(mBorderlessParent->Window());
          }
        }
      } 
      else 
      {
        // normal window :normal look & feel
        winrect.OffsetBy( 10, 30 );
        w = new nsWindowBeOS(this, winrect, "", B_TITLED_WINDOW_LOOK, B_NORMAL_WINDOW_FEEL,
                             B_ASYNCHRONOUS_CONTROLS);
      }

      if(w)
      {
        w->AddChild(mView);

        // FIXME: we have to use the window size because
        // the window might not like sizes less then 30x30 or something like that
        mView->MoveTo(0, 0);
        mView->ResizeTo(w->Bounds().Width(), w->Bounds().Height());
        mView->SetResizingMode(B_FOLLOW_ALL);
      }
    }

#if 0
    // Initial Drag & Drop Work
#ifdef DRAG_DROP
    if (!gOLEInited) 
    {
      DWORD dwVer = ::OleBuildVersion();

      if (FAILED(::OleInitialize(NULL)))
      {
        NS_WARNING("***** OLE has been initialized!");
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
  
  return NS_ERROR_OUT_OF_MEMORY;
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
       return new nsViewBeOS(this, BRect(0, 0, 0, 0), "", 0, B_WILL_DRAW);
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
			// be careful : BWindow and BView's Show() and Hide() can be nested
			if (!mView->IsHidden())
				mView->Hide();
			if(havewindow && !mView->Window()->IsHidden())
				mView->Window()->Hide();
		}
		else
		{
			// be careful : BWindow and BView's Show() and Hide() can be nested
			if (mView->IsHidden())
				mView->Show();
			if(havewindow && mView->Window()->IsHidden())
				mView->Window()->Show();
		}

		if(mustunlock)
			mView->UnlockLooper();
	}

	mIsVisible = bState;     
	return NS_OK;
}


NS_METHOD nsWindow::CaptureRollupEvents(nsIRollupListener * aListener, PRBool aDoCapture, PRBool aConsumeRollupEvent)
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
    NS_IF_RELEASE(gRollupWidget); 
  } 
 
  return NS_OK; 
} 
 
PRBool 
nsWindow::EventIsInsideWindow(nsWindow* aWindow, nsPoint pos) 
{ 
  BRect r; 
  BView *view = (BView *) aWindow->GetNativeData(NS_NATIVE_WIDGET); 
  if (view && view->LockLooper() ) { 
    r = view->ConvertToScreen(view->Bounds()); 
    view->UnlockLooper(); 
  } 
  
  if (pos.x < r.left || pos.x > r.right || 
      pos.y < r.top || pos.y > r.bottom) { 
    return PR_FALSE; 
  } 
 
  return PR_TRUE; 
} 

// 
// DealWithPopups 
// 
// Handle events that may cause a popup (combobox, XPMenu, etc) to need to rollup. 
// 
PRBool 
nsWindow::DealWithPopups(uint32 methodID, nsPoint pos) 
{ 
  if (gRollupListener && gRollupWidget) { 
 
      // Rollup if the event is outside the popup. 
      PRBool rollup = !nsWindow::EventIsInsideWindow((nsWindow*)gRollupWidget, pos); 
 
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
                if ( nsWindow::EventIsInsideWindow((nsWindow*)temp, pos) ) { 
                  rollup = PR_FALSE; 
                  break; 
                } 
              } 
            } // foreach parent menu widget 
          } 
        } // if rollup listener knows about menus 
      } 
 
      if ( rollup ) { 
        gRollupListener->Rollup(); 
 
        if (gRollupConsumeRollupEvent) { 
          return PR_TRUE; 
        } 
      } 
 } // if rollup listeners registered 
 
  return PR_FALSE; 

} // DealWithPopups


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
// Sanity check potential move coordinates
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::ConstrainPosition(PRInt32 *aX, PRInt32 *aY)
{
	NS_WARNING("nsWindow::ConstrainPosition - not implemented");
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
		// Popup window should be placed relative to its parent window.
		if (mWindowType == eWindowType_popup && mBorderlessParent) {
			BWindow *parentwindow = mBorderlessParent->Window();
			if (parentwindow && parentwindow->Lock()) {
				BPoint p = mBorderlessParent->ConvertToScreen(BPoint(aX,aY));
				aX = (nscoord)p.x;
				aY = (nscoord)p.y;
				parentwindow->Unlock();
			}
		}
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

#ifdef MOZ_DEBUG
		if(! aRepaint)
			printf("nsWindow::Resize FIXME: no repaint not implemented\n");
#endif

		if(mView->Parent() || ! havewindow)
			mView->ResizeTo(aWidth-1, GetHeight(aHeight)-1);
		else
			((nsWindowBeOS *)mView->Window())->ResizeToWithoutEvent(aWidth-1, GetHeight(aHeight)-1);

		if(mustunlock)
			mView->UnlockLooper();

               //inform the xp layer of the change in size
		OnResize(mBounds);

	}
	else
		OnResize(mBounds);

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

#ifdef MOZ_DEBUG
		if(! aRepaint)
			printf("nsWindow::Resize FIXME: no repaint not implemented\n");
#endif
		
		if(mView->Parent() || ! havewindow)
		{
			mView->MoveTo(aX, aY);
			mView->ResizeTo(aWidth-1, GetHeight(aHeight)-1);
		}
		else
		{
			mView->Window()->MoveTo(aX, aY);
			((nsWindowBeOS *)mView->Window())->ResizeToWithoutEvent(aWidth-1, GetHeight(aHeight)-1);
		}

		if(mustunlock)
			mView->UnlockLooper();

		//inform the xp layer of the change in size
		OnResize(mBounds);

	} else
		OnResize(mBounds);

	return NS_OK;
}

    
//-------------------------------------------------------------------------
//
// Enable/disable this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Enable(PRBool bState)
{
    if(mView && mView->LockLooper()) {
        if (mView->Window()) {
            uint flags = mView->Window()->Flags();
            if (bState == PR_TRUE) {
               flags &= ~(B_AVOID_FRONT|B_AVOID_FOCUS);
            } else {
               flags |= B_AVOID_FRONT|B_AVOID_FOCUS;
            }
            mView->Window()->SetFlags(flags);
        }
        mView->UnlockLooper();
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
               aRect.width  = r.IntegerWidth()+1;
               aRect.height = r.IntegerHeight()+1;
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
               aRect.width  = r.IntegerWidth()+1;
               aRect.height = r.IntegerHeight()+1;
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
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_IMETHODIMP 
nsWindow::InvalidateRegion(const nsIRegion *aRegion, PRBool aIsSynchronous)
{
  nsresult rv = NS_ERROR_FAILURE;
  nsRect r;
  BRegion *rgn = nsnull;

  if (!aRegion)
    return NS_ERROR_FAILURE;
  
  aRegion->GetNativeRegion((void*&)rgn);

  if (rgn) {
    BRect br(0,0,0,0);
    br = rgn->Frame(); // get bounding box of the region
    if (br.IsValid()) {
      r.SetRect(br.left, br.top, br.IntegerWidth() + 1, br.IntegerHeight() + 1);
      rv = this->Invalidate(r, aIsSynchronous);
    }
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
NS_WARNING("nsWindow::SetColorMap - not implemented");
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

  switch (info->methodId)
  {
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
      break;

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
      SetFocus(PR_FALSE);
      break;

    case nsWindow::GOT_FOCUS:
      NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
      DispatchFocus(NS_GOTFOCUS);
      //if(gJustGotActivate) {
      //  gJustGotActivate = PR_FALSE;
      DispatchFocus(NS_ACTIVATE);
      //}
      break;

    case nsWindow::KILL_FOCUS:
      NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
      DispatchFocus(NS_LOSTFOCUS);
      //if(gJustGotDeactivate) {
      //  gJustGotDeactivate = PR_FALSE;
      DispatchFocus(NS_DEACTIVATE);
      //} 
      break;

    case nsWindow::ONMOUSE :
      {
        NS_ASSERTION(info->nArgs == 5, "Wrong number of arguments to CallMethod");
        // close popup when clicked outside of the popup window 
        uint32 eventID = ((int32 *)info->args)[0]; 
        bool rollup = false; 
 
        if ((eventID == NS_MOUSE_LEFT_BUTTON_DOWN || 
        eventID == NS_MOUSE_RIGHT_BUTTON_DOWN || 
        eventID == NS_MOUSE_MIDDLE_BUTTON_DOWN) && 
        mView && mView->LockLooper())
        { 
          BPoint p(((int32 *)info->args)[1], ((int32 *)info->args)[2]); 
          mView->ConvertToScreen(&p); 
          if (DealWithPopups(nsWindow::ONMOUSE, nsPoint(p.x, p.y))) 
            rollup = true; 
          mView->UnlockLooper(); 
        } 
        if (rollup)
          break; 
                               
        DispatchMouseEvent(((int32 *)info->args)[0], 
          nsPoint(((int32 *)info->args)[1], ((int32 *)info->args)[2]), 
          ((int32 *)info->args)[3], 
          ((int32 *)info->args)[4]);
			
        if (((int32 *)info->args)[0] == NS_MOUSE_RIGHT_BUTTON_DOWN)
        {
          DispatchMouseEvent (NS_CONTEXTMENU,
            nsPoint(((int32 *)info->args)[1], ((int32 *)info->args)[2]),
            ((int32 *)info->args)[3],
            ((int32 *)info->args)[4]);
        }
      }
      break;
		
    case nsWindow::ONWHEEL :
      {
        NS_ASSERTION(info->nArgs == 1, "Wrong number of arguments to CallMethod");
			  
        nsMouseScrollEvent scrollEvent;
        
        scrollEvent.scrollFlags = nsMouseScrollEvent::kIsVertical;

        scrollEvent.delta = (info->args)[0];
        
        scrollEvent.eventStructType = NS_MOUSE_SCROLL_EVENT;
        scrollEvent.message   = NS_MOUSE_SCROLL;
        scrollEvent.nativeMsg = nsnull;
        scrollEvent.widget    = this;
        scrollEvent.time      = PR_IntervalNow();

        // XXX implement these items?
        scrollEvent.point.x = 100;
        scrollEvent.point.y = 100;
       
        // we don't use the mIsXDown bools because
        // they get reset on Gecko reload (makes it harder
        // to use stuff like Alt+Wheel)
        uint32 mod (modifiers());
        
        scrollEvent.isControl = mod & B_CONTROL_KEY;
        scrollEvent.isShift = mod & B_SHIFT_KEY;
        scrollEvent.isAlt   = mod & B_COMMAND_KEY;
        scrollEvent.isMeta  = PR_FALSE;
               
        nsEventStatus rv;
        DispatchEvent (&scrollEvent, rv);
      }              
      break;
		
    case nsWindow::ONKEY :
      NS_ASSERTION(info->nArgs == 6, "Wrong number of arguments to CallMethod");
      if (((int32 *)info->args)[0] == NS_KEY_DOWN)
      {
        OnKeyDown(((int32 *)info->args)[0],
          (const char *)(&((uint32 *)info->args)[1]), ((int32 *)info->args)[2],
          ((uint32 *)info->args)[3], ((uint32 *)info->args)[4], ((int32 *)info->args)[5]);
      }
      else
      {
        if (((int32 *)info->args)[0] == NS_KEY_UP)
        {
          OnKeyUp(((int32 *)info->args)[0],
          (const char *)(&((uint32 *)info->args)[1]), ((int32 *)info->args)[2],
          ((uint32 *)info->args)[3], ((uint32 *)info->args)[4], ((int32 *)info->args)[5]);
        }
      }
      break;

    case nsWindow::ONPAINT :
      NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
      if(mView && mView->LockLooper())
      {
        nsRect r;
        nsViewBeOS *bv = dynamic_cast<nsViewBeOS *>(mView);
        if(bv && bv->GetPaintRect(r))
          OnPaint(r);
        mView->UnlockLooper();
      }
      break;

    case nsWindow::ONRESIZE :
      {
        NS_ASSERTION(info->nArgs == 2, "Wrong number of arguments to CallMethod");

        nsRect r;
        r.width=(nscoord)info->args[0];
        r.height=(nscoord)info->args[1];

        OnResize(r);
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

    default:
      bRet = FALSE;
      break;
  }

  return bRet;
}

//-------------------------------------------------------------------------
//
// Key code translation related data
//
//-------------------------------------------------------------------------

struct nsKeyConverter {
  int vkCode; // Platform independent key code
  char bekeycode; // BeOS key code
};

//
// Netscape keycodes are defined in widget/public/nsGUIEvent.h
// BeOS keycodes can be viewd at
// http://www.be.com/documentation/be_book/Keyboard/KeyboardKeyCodes.html
//

struct nsKeyConverter nsKeycodesBeOS[] = {
//  { NS_VK_CANCEL,     GDK_Cancel },
  { NS_VK_BACK,       0x1e },
  { NS_VK_TAB,        0x26 },
//  { NS_VK_TAB,        GDK_ISO_Left_Tab },
//  { NS_VK_CLEAR,      GDK_Clear },
  { NS_VK_RETURN,     0x47 },
  { NS_VK_SHIFT,      0x4b },
  { NS_VK_SHIFT,      0x56 },
  { NS_VK_CONTROL,    0x5c },
  { NS_VK_CONTROL,    0x60 },
  { NS_VK_ALT,        0x5d },
  { NS_VK_ALT,        0x5f },
  { NS_VK_PAUSE,      0x22 },
  { NS_VK_CAPS_LOCK,  0x3b },
  { NS_VK_ESCAPE,     0x1 },
  { NS_VK_SPACE,      0x5e },
  { NS_VK_PAGE_UP,    0x21 },
  { NS_VK_PAGE_DOWN,  0x36 },
  { NS_VK_END,        0x35 },
  { NS_VK_HOME,       0x20 },
  { NS_VK_LEFT,       0x61 },
  { NS_VK_UP,         0x57 },
  { NS_VK_RIGHT,      0x63 },
  { NS_VK_DOWN,       0x62 },
  { NS_VK_PRINTSCREEN, 0xe },
  { NS_VK_INSERT,     0x1f },
  { NS_VK_DELETE,     0x34 },

  // keypad keys (constant keys)
  { NS_VK_MULTIPLY,   0x24 },
  { NS_VK_ADD,        0x3a },
//  { NS_VK_SEPARATOR,   }, ???
  { NS_VK_SUBTRACT,   0x25 },
  { NS_VK_DIVIDE,     0x23 },
  { NS_VK_RETURN,     0x5b },

  { NS_VK_COMMA,      0x53 },
  { NS_VK_PERIOD,     0x54 },
  { NS_VK_SLASH,      0x55 },
  { NS_VK_BACK_SLASH, 0x33 },
  { NS_VK_BACK_SLASH, 0x6a }, // got this code on japanese keyboard
  { NS_VK_BACK_SLASH, 0x6b }, // got this code on japanese keyboard
  { NS_VK_BACK_QUOTE, 0x11 },
  { NS_VK_OPEN_BRACKET, 0x31 },
  { NS_VK_CLOSE_BRACKET, 0x32 },
  { NS_VK_SEMICOLON, 0x45 },
  { NS_VK_QUOTE, 0x46 },

  // NS doesn't have dash or equals distinct from the numeric keypad ones,
  // so we'll use those for now.  See bug 17008:
  { NS_VK_SUBTRACT, 0x1c },
  { NS_VK_EQUALS, 0x1d },

  { NS_VK_F1, B_F1_KEY },
  { NS_VK_F2, B_F2_KEY },
  { NS_VK_F3, B_F3_KEY },
  { NS_VK_F4, B_F4_KEY },
  { NS_VK_F5, B_F5_KEY },
  { NS_VK_F6, B_F6_KEY },
  { NS_VK_F7, B_F7_KEY },
  { NS_VK_F8, B_F8_KEY },
  { NS_VK_F9, B_F9_KEY },
  { NS_VK_F10, B_F10_KEY },
  { NS_VK_F11, B_F11_KEY },
  { NS_VK_F12, B_F12_KEY },

  { NS_VK_1, 0x12 },
  { NS_VK_2, 0x13 },
  { NS_VK_3, 0x14 },
  { NS_VK_4, 0x15 },
  { NS_VK_5, 0x16 },
  { NS_VK_6, 0x17 },
  { NS_VK_7, 0x18 },
  { NS_VK_8, 0x19 },
  { NS_VK_9, 0x1a },
  { NS_VK_0, 0x1b },

  { NS_VK_A, 0x3c },
  { NS_VK_B, 0x50 },
  { NS_VK_C, 0x4e },
  { NS_VK_D, 0x3e },
  { NS_VK_E, 0x29 },
  { NS_VK_F, 0x3f },
  { NS_VK_G, 0x40 },
  { NS_VK_H, 0x41 },
  { NS_VK_I, 0x2e },
  { NS_VK_J, 0x42 },
  { NS_VK_K, 0x43 },
  { NS_VK_L, 0x44 },
  { NS_VK_M, 0x52 },
  { NS_VK_N, 0x51 },
  { NS_VK_O, 0x2f },
  { NS_VK_P, 0x30 },
  { NS_VK_Q, 0x27 },
  { NS_VK_R, 0x2a },
  { NS_VK_S, 0x3d },
  { NS_VK_T, 0x2b },
  { NS_VK_U, 0x2d },
  { NS_VK_V, 0x4f },
  { NS_VK_W, 0x28 },
  { NS_VK_X, 0x4d },
  { NS_VK_Y, 0x2c },
  { NS_VK_Z, 0x4c }
};

// keycode of keypad when num-locked
struct nsKeyConverter nsKeycodesBeOSNumLock[] = {
  { NS_VK_NUMPAD0, 0x64 },
  { NS_VK_NUMPAD1, 0x58 },
  { NS_VK_NUMPAD2, 0x59 },
  { NS_VK_NUMPAD3, 0x5a },
  { NS_VK_NUMPAD4, 0x48 },
  { NS_VK_NUMPAD5, 0x49 },
  { NS_VK_NUMPAD6, 0x4a },
  { NS_VK_NUMPAD7, 0x37 },
  { NS_VK_NUMPAD8, 0x38 },
  { NS_VK_NUMPAD9, 0x39 },
  { NS_VK_DECIMAL, 0x65 }
};

// keycode of keypad when not num-locked
struct nsKeyConverter nsKeycodesBeOSNoNumLock[] = {
  { NS_VK_LEFT,       0x48 },
  { NS_VK_RIGHT,      0x4a },
  { NS_VK_UP,         0x38 },
  { NS_VK_DOWN,       0x59 },
  { NS_VK_PAGE_UP,    0x39 },
  { NS_VK_PAGE_DOWN,  0x5a },
  { NS_VK_HOME,       0x37 },
  { NS_VK_END,        0x58 },
  { NS_VK_INSERT,     0x64 },
  { NS_VK_DELETE,     0x65 }
};

//-------------------------------------------------------------------------
//
// Translate key code
// Input is BeOS keyboard key-code; output is in NS_VK format
//
//-------------------------------------------------------------------------

static int TranslateBeOSKeyCode(int32 bekeycode, bool isnumlock)
{
//printf("bekeycode=%x\n",bekeycode);
  int i;
  int length = sizeof(nsKeycodesBeOS) / sizeof(struct nsKeyConverter);
  int length_numlock = sizeof(nsKeycodesBeOSNumLock) / sizeof(struct nsKeyConverter);
  int length_nonumlock = sizeof(nsKeycodesBeOSNoNumLock) / sizeof(struct nsKeyConverter);

  // key code conversion
  for (i = 0; i < length; i++) {
    if (nsKeycodesBeOS[i].bekeycode == bekeycode)
      return(nsKeycodesBeOS[i].vkCode);
  }
  // numpad keycode vary with numlock
  if (isnumlock) {
    for (i = 0; i < length_numlock; i++) {
      if (nsKeycodesBeOSNumLock[i].bekeycode == bekeycode)
        return(nsKeycodesBeOSNumLock[i].vkCode);
    }
  } else {
    for (i = 0; i < length_nonumlock; i++) {
      if (nsKeycodesBeOSNoNumLock[i].bekeycode == bekeycode)
        return(nsKeycodesBeOSNoNumLock[i].vkCode);
    }
  }
  return((int)0);
}

//-------------------------------------------------------------------------
//
// OnKeyDown
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnKeyDown(PRUint32 aEventType, const char *bytes, 
  int32 numBytes, PRUint32 mod, PRUint32 bekeycode, int32 rawcode)
{
  PRUint32 aTranslatedKeyCode;
  PRBool result = PR_FALSE;

  mIsShiftDown   = (mod & B_SHIFT_KEY) ? PR_TRUE : PR_FALSE;
  mIsControlDown = (mod & B_CONTROL_KEY) ? PR_TRUE : PR_FALSE;
  mIsAltDown     = (mod & B_COMMAND_KEY) ? PR_TRUE : PR_FALSE;
  bool IsNumLocked = ((mod & B_NUM_LOCK) != 0);

  aTranslatedKeyCode = TranslateBeOSKeyCode(bekeycode, IsNumLocked);
  
  if (numBytes <= 1)
  {
    result = DispatchKeyEvent(NS_KEY_DOWN, 0, aTranslatedKeyCode);
  } else {
    //   non ASCII chars
  }

// ------------  On Char  ------------
  PRUint32	uniChar;

  if ((mIsControlDown || mIsAltDown) && rawcode >= 'a' && rawcode <= 'z') {
    if (mIsShiftDown)
      uniChar = rawcode + 'A' - 'a';
    else
      uniChar = rawcode;
    aTranslatedKeyCode = 0;
  } else {
    if (numBytes == 0) // deal with unmapped key
      return result;

    switch((unsigned char)bytes[0])
    {
      case 0xc8://System Request
      case 0xca://Break
        return result;// do not send 'KEY_PRESS' message

      case B_INSERT:
      case B_ESCAPE:
      case B_FUNCTION_KEY:
      case B_HOME:
      case B_PAGE_UP:
      case B_END:
      case B_PAGE_DOWN:
      case B_UP_ARROW:
      case B_LEFT_ARROW:
      case B_DOWN_ARROW:
      case B_RIGHT_ARROW:
      case B_TAB:
      case B_DELETE:
      case B_BACKSPACE:
      case B_ENTER:
        uniChar = 0;
        break;

      default:
        // UTF-8 to unicode conversion
        if (numBytes >= 1 && (bytes[0] & 0x80) == 0) { 
          // 1 byte utf-8 char
          uniChar = bytes[0];
        } else
        if (numBytes >= 2 && (bytes[0] & 0xe0) == 0xc0) { 
          // 2 byte utf-8 char
          uniChar = ((uint16)(bytes[0] & 0x1f) << 6) | (uint16)(bytes[1] & 0x3f);
        } else
        if (numBytes >= 3 && (bytes[0] & 0xf0) == 0xe0) {
          // 3 byte utf-8 char
          uniChar = ((uint16)(bytes[0] & 0x0f) << 12) | ((uint16)(bytes[1] & 0x3f) << 6)
                | (uint16)(bytes[2] & 0x3f);
        } else {
          //error
          uniChar = 0;
          NS_WARNING("nsWindow::OnKeyDown() error: bytes[] has not enough chars.");
        } 
    
        aTranslatedKeyCode = 0;
        mIsShiftDown = PR_FALSE;
        break;
    }
  }

  PRBool result2 = DispatchKeyEvent(NS_KEY_PRESS, uniChar, aTranslatedKeyCode);
  
  return result && result2;
}

//-------------------------------------------------------------------------
//
// OnKeyUp
//
//-------------------------------------------------------------------------
PRBool nsWindow::OnKeyUp(PRUint32 aEventType, const char *bytes,
  int32 numBytes, PRUint32 mod, PRUint32 bekeycode, int32 rawcode)
{
  PRUint32 aTranslatedKeyCode;
  bool IsNumLocked = ((mod & B_NUM_LOCK) != 0);

  mIsShiftDown   = (mod & B_SHIFT_KEY) ? PR_TRUE : PR_FALSE;
  mIsControlDown = (mod & B_CONTROL_KEY) ? PR_TRUE : PR_FALSE;
  mIsAltDown     = (mod & B_COMMAND_KEY) ? PR_TRUE : PR_FALSE;

  aTranslatedKeyCode = TranslateBeOSKeyCode(bekeycode, IsNumLocked);

  PRBool result = DispatchKeyEvent(NS_KEY_UP, 0, aTranslatedKeyCode);
  return result;

}

//-------------------------------------------------------------------------
//
// DispatchKeyEvent
//
//-------------------------------------------------------------------------

PRBool nsWindow::DispatchKeyEvent(PRUint32 aEventType, PRUint32 aCharCode, 
  PRUint32 aKeyCode)
{
  nsKeyEvent event;
  nsPoint point;

  point.x = 0;
  point.y = 0;

  InitEvent(event, aEventType, &point); // this add ref's event.widget

  event.charCode = aCharCode;
  event.keyCode  = aKeyCode;

#ifdef KE_DEBUG
  static int cnt=0;
  printf("%d DispatchKE Type: %s charCode 0x%x  keyCode 0x%x ", cnt++,  
        (NS_KEY_PRESS == aEventType)?"PRESS":(aEventType == NS_KEY_UP?"Up":"Down"), 
         event.charCode, event.keyCode);
  printf("Shift: %s Control %s Alt: %s \n",  (mIsShiftDown?"D":"U"), (mIsControlDown?"D":"U"), (mIsAltDown?"D":"U"));
#endif

  event.isShift   = mIsShiftDown;
  event.isControl = mIsControlDown;
  event.isMeta   =  PR_FALSE;
  event.isAlt     = mIsAltDown;
  event.eventStructType = NS_KEY_EVENT;

  PRBool result = DispatchWindowEvent(&event);
  NS_RELEASE(event.widget);

  return result;
}

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
    NS_WARNING("///////////// Menu is NULL!");
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
#ifdef DEBUG
          printf("Getting submenu by position %d from parentMenu\n", aItemNum);
#endif
          nsISupports * item;
          parentMenu->GetItemAt((PRUint32)aItemNum, item);
          if (NS_OK != item->QueryInterface(kIMenuIID, (void **)&newMenu)) {
            NS_WARNING("Item was not a menu! What are we doing here? Return early....");
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
        NS_WARNING("no menu was found. This is bad.");
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
                       event.mWinWidth  = r.IntegerWidth()+1; 
                       event.mWinHeight = r.IntegerHeight()+1;
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
		InitEvent (event, aEventType, &aPoint);
		event.isShift   = mod & B_SHIFT_KEY;
		event.isControl = mod & B_CONTROL_KEY;
		event.isAlt     = mod & B_COMMAND_KEY;
		event.isMeta     = PR_FALSE;
		event.clickCount = clicks;
		event.eventStructType = NS_MOUSE_EVENT;

		// call the event callback
		if(nsnull != mEventCallback)
		{
			PRBool result = DispatchWindowEvent(&event);
			NS_RELEASE(event.widget);
			return result;
		}
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
			
			NS_RELEASE(event.widget);
		}
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
	const char *text = ToNewUTF8String(aTitle);
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
NS_WARNING("nsWindow::SetMenuBar - FIXME: Get rid of the old menubar!");
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
NS_WARNING("nsWindow::ShowMenuBar - FIXME: not implemented!");
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
// NS_ADDREF/NS_RELEASE is not needed here.
// This class is used as internal (BeOS native) object of nsWindow,
// so it must not addref/release nsWindow here. 
// Otherwise, nsWindow object will leak. (Makoto Hamanaka)

//	NS_ADDREF(mWidget);
}

nsIWidgetStore::~nsIWidgetStore()
{
//	NS_RELEASE(mWidget);
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
    nsIWidgetStore( aWidgetWindow ),
    resizeRunner(NULL)
{
       //note that the window will be resized (and FrameResized()
       //will be called) if aFrame isn't a valid window size
    lastWidth=aFrame.Width();
    lastHeight=aFrame.Height();
}

nsWindowBeOS::~nsWindowBeOS()
{
       //clean up
       delete resizeRunner;
}

bool nsWindowBeOS::QuitRequested( void )
{
	// tells nsWindow to kill me
	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0)
	{
		MethodInfo *info = new MethodInfo(w, w, nsWindow::CLOSEWINDOW);
		t->CallMethodAsync(info);
		NS_RELEASE(t);
	}
	return false;
}

void nsWindowBeOS::MessageReceived(BMessage *msg)
{
  switch (msg->what)
  {
    case 'RESZ':
      {
        // this is the message generated by the resizeRunner - it
        // has now served its purpose
        delete resizeRunner;
        resizeRunner=NULL;


        //kick off the xp resizing stuff
        DoFrameResized();
      }
      break;

    default :
      BWindow::MessageReceived(msg);
      break;
  }
}

// This function calls KeyDown() for Alt+whatever instead of app_server
void nsWindowBeOS::DispatchMessage(BMessage *msg, BHandler *handler)
{
  if (msg->what == B_KEY_DOWN && modifiers() & B_COMMAND_KEY) {
    BString bytes;
    if (B_OK == msg->FindString("bytes", &bytes)) {
      BView *view = this->CurrentFocus();
      if (view)
        view->KeyDown(bytes.String(), bytes.Length());
    }
  }
  BWindow::DispatchMessage(msg, handler);
}

void nsWindowBeOS::FrameResized(float width, float height)
{
       //determine if the window size actually changed
       if (width==lastWidth && height==lastHeight) {
               //it didn't - don't bother
               return;
       }

       //remember new size
       lastWidth=width;
       lastHeight=height;

       //while the user resizes the window, a large number of
       //B_WINDOW_RESIZED events are generated.  because
       //the layout/drawing code invoked during a resize isn't
       //terribly fast, these events can "back up" (new ones are
       //delivered before old ones are processed), causing mozilla
       //to act sluggish (even on a fast machine, the re-layouts
       //and redraws performed in response to the B_WINDOW_RESIZED
       //events usually don't finish until long after the user is
       //done resizing the window), and possibly even overflow the
       //window's event queue.  to stop this from happening, we will
       //consolidate all B_WINDOW_RESIZED messages generated within
       //1/10 seconds of each other into a single resize event (i.e.,
       //the views won't be resized/redrawn until the user finishes
       //resizing the window)
       const bigtime_t timerLen=100000LL;
       if (resizeRunner==NULL) {
               //this is the first B_WINDOW_RESIZE event in this interval -
               //start the timer
               BMessage msg('RESZ');
               resizeRunner=new BMessageRunner(BMessenger(this), &msg, timerLen, 1);
               
       } else {
               //this is not the first B_WINDOW_RESIZE event in the
               //current interval - reset the timer
               resizeRunner->SetInterval(timerLen);
       }
}

void nsWindowBeOS::ResizeToWithoutEvent(float width, float height)
{
       //this method is just a wrapper for BWindow::ResizeTo(), except
       //it will attempt to keep the B_FRAME_RESIZED event resulting
       //from the ResizeTo() call from being passed to the xp layout
       //engine (OnResize() will already have been called for this
       //resize by nsWindow::Resize() above)
       lastWidth=width;
       lastHeight=height;
       ResizeTo(width, height);
}

void nsWindowBeOS::DoFrameResized()
{
       //let the layout engine know the window has changed size,
       //so it can resize the window's views
       nsWindow        *w = (nsWindow *)GetMozillaWidget();
       nsToolkit       *t;
       if(w && (t = w->GetToolkit()) != 0)
       {
               //the window's new size needs to be passed to OnResize() -
               //note that the values passed to FrameResized() are the
               //dimensions as reported by Bounds().Width() and
               //Bounds().Height(), which are one pixel smaller than the
               //window's true size
               uint32 args[2];
               args[0]=(uint32)lastWidth+1;
               args[1]=(uint32)lastHeight+1;

               MethodInfo *info =
                       new MethodInfo(w, w, nsWindow::ONRESIZE, 2, args);
               t->CallMethodAsync(info);
               NS_RELEASE(t);
       }
}

//----------------------------------------------------
// BeOS Sub-Class View
//----------------------------------------------------

nsViewBeOS::nsViewBeOS(nsIWidget *aWidgetWindow, BRect aFrame, const char *aName, uint32 aResizingMode, uint32 aFlags)
 : BView(aFrame, aName, aResizingMode, aFlags), nsIWidgetStore(aWidgetWindow), buttons(0)
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

void nsViewBeOS::MouseMoved(BPoint point, uint32 transit, const BMessage *msg)
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
                       MethodInfo *enterInfo =
                                               new MethodInfo(w, w, nsWindow::ONMOUSE, 5, args);
                       t->CallMethodAsync(enterInfo);
		}
	
		args[0] = NS_MOUSE_MOVE;
               MethodInfo *moveInfo =
                                               new MethodInfo(w, w, nsWindow::ONMOUSE, 5, args);
               t->CallMethodAsync(moveInfo);
	
		if(transit == B_EXITED_VIEW)
		{
			args[0] = NS_MOUSE_EXIT;
                       MethodInfo *exitInfo =
                                               new MethodInfo(w, w, nsWindow::ONMOUSE, 5, args);
                       t->CallMethodAsync(exitInfo);
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

void nsViewBeOS::MessageReceived(BMessage *msg)
{
  switch(msg->what)
  {
    case B_UNMAPPED_KEY_DOWN:
      //printf("unmapped_key_down\n");
      this->KeyDown(NULL, 0);
      break;
      
    case B_UNMAPPED_KEY_UP:
      //printf("unmapped_key_up\n");
      this->KeyUp(NULL, 0);
      break;

    case B_MOUSE_WHEEL_CHANGED:
      {
        float wheel_y;
        
        msg->FindFloat ("be:wheel_delta_y", &wheel_y);
        
        // make sure there *was* movement on the y axis
        if(wheel_y == 0)
          break;

        nsWindow    *w = (nsWindow *)GetMozillaWidget();
        nsToolkit   *t;
        
        if(w && (t = w->GetToolkit()) != 0)
        {
          uint32 args[1];
          if (wheel_y > 0)
            args[0] = (uint32)3;
          else
            args[0] = (uint32)-3;
            
          MethodInfo *info = new MethodInfo(w, w, nsWindow::ONWHEEL, 1, args);
          t->CallMethodAsync(info);
          NS_RELEASE(t);
        }
      }
      break;
      
	default :
      BView::MessageReceived(msg);
      break;
  }
}

void nsViewBeOS::KeyDown(const char *bytes, int32 numBytes)
{
  nsWindow	*w = (nsWindow *)GetMozillaWidget();
  nsToolkit	*t;
  int32 keycode = 0;
  int32 rawcode = 0;
		
  BMessage *msg = this->Window()->CurrentMessage();
  if (msg) {
    msg->FindInt32("key", &keycode);
    msg->FindInt32("raw_char", &rawcode);
  }
	
  if(w && (t = w->GetToolkit()) != 0) {
    uint32 bytebuf = 0;
    uint8 *byteptr = (uint8 *)&bytebuf;
    for(int32 i = 0; i < numBytes; i++)
      byteptr[i] = bytes[i];

    uint32	args[6];
    args[0] = NS_KEY_DOWN;
    args[1] = bytebuf;
    args[2] = numBytes;
    args[3] = modifiers();
    args[4] = keycode;
    args[5] = rawcode;

    MethodInfo *info = new MethodInfo(w, w, nsWindow::ONKEY, 6, args);
    t->CallMethodAsync(info);
    NS_RELEASE(t);
  }
}

void nsViewBeOS::KeyUp(const char *bytes, int32 numBytes)
{
  nsWindow	*w = (nsWindow *)GetMozillaWidget();
  nsToolkit	*t;
  int32 keycode = 0;
  int32 rawcode = 0;	
  BMessage *msg = this->Window()->CurrentMessage();
  if (msg) {
    msg->FindInt32("key", &keycode);
    msg->FindInt32("raw_char", &rawcode);
  }

  if(w && (t = w->GetToolkit()) != 0) {
    uint32 bytebuf = 0;
    uint8 *byteptr = (uint8 *)&bytebuf;
    for(int32 i = 0; i < numBytes; i++)
      byteptr[i] = bytes[i];

    uint32	args[6];
    args[0] = NS_KEY_UP;
    args[1] = (int32)bytebuf;
    args[2] = numBytes;
    args[3] = modifiers();
    args[4] = keycode;
    args[5] = rawcode;

    MethodInfo *info = new MethodInfo(w, w, nsWindow::ONKEY, 6, args);
    t->CallMethodAsync(info);
    NS_RELEASE(t);
  }
}

void nsViewBeOS::MakeFocus(bool focused)
{
//printf("MakeFocus %s\n",(focused)?"get":"lost");
	if (IsFocus()) return;


	BView::MakeFocus(focused);

	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if(w && (t = w->GetToolkit()) != 0)
	{
		MethodInfo *info = new MethodInfo(w, w, (focused)? nsWindow::GOT_FOCUS : nsWindow::KILL_FOCUS);
		t->CallMethodAsync(info);
		NS_RELEASE(t);
	}
}

