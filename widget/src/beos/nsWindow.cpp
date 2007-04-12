/* -*- Mode: C++; tab-width: 2; c-basic-offset: 2 -*- */
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
 *   Paul Ashford <arougthopher@lizardland.net>
 *   Sergei Dolgov <sergei_d@fi.tartu.ee>
 *   Fredrik Holmqvist <thesuckiestemail@yahoo.se>
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

#include "nsDebug.h"
#include "nsWindow.h"
#include "nsIAppShell.h"
#include "nsIFontMetrics.h"
#include "nsFont.h"
#include "nsGUIEvent.h"
#include "nsWidgetsCID.h"
#include "nsIDragService.h"
#include "nsIDragSessionBeOS.h"
#include "nsIDeviceContext.h"
#include "nsRect.h"
#include "nsIRegion.h"
#include "nsTransform2D.h"
#include "nsGfxCIID.h"
#include "resource.h"
#include "prtime.h"
#include "nsReadableUtils.h"
#include "nsVoidArray.h"
#include "nsIProxyObjectManager.h"

#include <Application.h>
#include <InterfaceDefs.h>
#include <Region.h>
#include <ScrollBar.h>
#include <app/Message.h>
#include <support/String.h>
#include <Screen.h>

#include <nsBeOSCursors.h>
#if defined(BeIME)
#include <Input.h>
#include <InputServerMethod.h>
#include <String.h>
#endif
#include "nsIRollupListener.h"
#include "nsIMenuRollup.h"

#ifdef MOZ_CAIRO_GFX
#include "gfxBeOSSurface.h"
#include "gfxContext.h"
#endif

// See comments in nsWindow.h as to why we override these calls from nsBaseWidget
NS_IMPL_THREADSAFE_ADDREF(nsWindow)
NS_IMPL_THREADSAFE_RELEASE(nsWindow)

static NS_DEFINE_IID(kIWidgetIID,       NS_IWIDGET_IID);
static NS_DEFINE_IID(kRegionCID, NS_REGION_CID);
static NS_DEFINE_IID(kCDragServiceCID,  NS_DRAGSERVICE_CID);
//-------------------------------------------------------------------------
// Global Definitions
//-------------------------------------------------------------------------

// Rollup Listener - static variable defintions
static nsIRollupListener * gRollupListener           = nsnull;
static nsIWidget         * gRollupWidget             = nsnull;
static PRBool              gRollupConsumeRollupEvent = PR_FALSE;
// Tracking last activated BWindow
static BWindow           * gLastActiveWindow = NULL;

// BCursor objects can't be created until they are used.  Some mozilla utilities, 
// such as regxpcom, do not create a BApplication object, and therefor fail to run.,
// since a BCursor requires a vaild BApplication (see Bug#129964).  But, we still want
// to cache them for performance.  Currently, there are 17 cursors available;
static nsVoidArray		gCursorArray(21);
// Used in contrain position.  Specifies how much of a window must remain on screen
#define kWindowPositionSlop 20
// BeOS does not provide this information, so we must hard-code it
#define kWindowBorderWidth 5
#define kWindowTitleBarHeight 24

// TODO: make a #def for using OutLine view or not (see TODO below)
#if defined(BeIME)
#include "nsUTF8Utils.h"
static inline uint32 utf8_str_len(const char* ustring, int32 length) 
{
	CalculateUTF8Length cutf8;
	cutf8.write(ustring, length);
	return cutf8.Length();       
}

nsIMEBeOS::nsIMEBeOS()
	: imeTarget(NULL)
	, imeState(NS_COMPOSITION_END), imeWidth(14)
{
}
/* placeholder for possible cleanup
nsIMEBeOS::~nsIMEBeOS()
{
}
*/								
void nsIMEBeOS::RunIME(uint32 *args, nsWindow *target, BView *fView)
{
	BMessage msg;
	msg.Unflatten((const char*)args);

	switch (msg.FindInt32("be:opcode")) 
	{
	case B_INPUT_METHOD_CHANGED:
		if (msg.HasString("be:string")) 
		{
			const char* src = msg.FindString("be:string");
			CopyUTF8toUTF16(src, imeText);
 
    		if (msg.FindBool("be:confirmed")) 
    		{	
    			if (imeState != NS_COMPOSITION_END)
   					DispatchText(imeText, 0, NULL);
   			}
   			else 
   			{
   				nsTextRange txtRuns[2];
   				PRUint32 txtCount = 2;

	 	    	int32 select[2];
 				select[0] = msg.FindInt32("be:selection", int32(0));
				select[1] = msg.FindInt32("be:selection", 1);

	 			txtRuns[0].mStartOffset = (select[0] == select[1]) ? 0 : utf8_str_len(src, select[1]);
	 			txtRuns[0].mEndOffset	= imeText.Length();
				txtRuns[0].mRangeType	= NS_TEXTRANGE_CONVERTEDTEXT;
				if (select[0] == select[1])
					txtCount = 1;
				else 
				{
	 				txtRuns[1].mStartOffset = utf8_str_len(src, select[0]);
	 				txtRuns[1].mEndOffset	= utf8_str_len(src, select[1]);
	 				txtRuns[1].mRangeType	= NS_TEXTRANGE_SELECTEDCONVERTEDTEXT;
	 			}
	 			imeTarget = target;
				DispatchText(imeText, txtCount, txtRuns);
			}	
		}	
		break;

	case B_INPUT_METHOD_LOCATION_REQUEST:
		if (fView && fView->LockLooper()) 
		{
			BPoint caret(imeCaret);
			DispatchIME(NS_COMPOSITION_QUERY);
			if (caret.x > imeCaret.x) 
				caret.x = imeCaret.x - imeWidth * imeText.Length();	/* back */

			BMessage reply(B_INPUT_METHOD_EVENT);
			reply.AddInt32("be:opcode", B_INPUT_METHOD_LOCATION_REQUEST);
			for (int32 s= 0; imeText[s]; s++) 
			{ 
				reply.AddPoint("be:location_reply", fView->ConvertToScreen(caret));
				reply.AddFloat("be:height_reply", imeHeight);
				caret.x += imeWidth;
			}
			imeMessenger.SendMessage(&reply);
			fView->UnlockLooper();
		}
		break;

	case B_INPUT_METHOD_STARTED:
		imeTarget = target;
		DispatchIME(NS_COMPOSITION_START);
		DispatchIME(NS_COMPOSITION_QUERY);

		msg.FindMessenger("be:reply_to", &imeMessenger);
		break;
	
	case B_INPUT_METHOD_STOPPED:
		if (imeState != NS_COMPOSITION_END)
			DispatchIME(NS_COMPOSITION_END);
		imeText.Truncate();
		break;
	};
}

void nsIMEBeOS::DispatchText(nsString &text, PRUint32 txtCount, nsTextRange* txtRuns)
{
	nsTextEvent textEvent(PR_TRUE,NS_TEXT_TEXT, imeTarget);

	textEvent.time 		= 0;
	textEvent.isShift   = 
	textEvent.isControl =
	textEvent.isAlt 	= 
	textEvent.isMeta 	= PR_FALSE;
  
	textEvent.refPoint.x	= 
	textEvent.refPoint.y	= 0;

	textEvent.theText 	= text.get();
	textEvent.isChar	= PR_TRUE;
	textEvent.rangeCount= txtCount;
	textEvent.rangeArray= txtRuns;

	DispatchWindowEvent(&textEvent);
}

void nsIMEBeOS::DispatchCancelIME()
{
	if (imeText.Length() && imeState != NS_COMPOSITION_END) 
	{
		BMessage reply(B_INPUT_METHOD_EVENT);
		reply.AddInt32("be:opcode", B_INPUT_METHOD_STOPPED);
		imeMessenger.SendMessage(&reply);

		DispatchText(imeText, 0, NULL);
		DispatchIME(NS_COMPOSITION_END);

		imeText.Truncate();
	}
}

void nsIMEBeOS::DispatchIME(PRUint32 what)
{
	nsCompositionEvent compEvent(PR_TRUE, what, imeTarget);

	compEvent.refPoint.x =
	compEvent.refPoint.y = 0;
	compEvent.time 	 = 0;

	DispatchWindowEvent(&compEvent);
	imeState = what;

	if (what == NS_COMPOSITION_QUERY) 
	{
		imeCaret.Set(compEvent.theReply.mCursorPosition.x,
		           compEvent.theReply.mCursorPosition.y);
		imeHeight = compEvent.theReply.mCursorPosition.height+4;
	}
}

PRBool nsIMEBeOS::DispatchWindowEvent(nsGUIEvent* event)
{
	nsEventStatus status;
	imeTarget->DispatchEvent(event, status);
	return PR_FALSE;
}
// There is only one IME instance per app, actually it may be set as global
nsIMEBeOS *nsIMEBeOS::GetIME()
{
	if(beosIME == 0)
		beosIME = new nsIMEBeOS();
	return beosIME;
}
nsIMEBeOS *nsIMEBeOS::beosIME = 0;
#endif
//-------------------------------------------------------------------------
//
// nsWindow constructor
//
//-------------------------------------------------------------------------
nsWindow::nsWindow() : nsBaseWidget()
{
	mView               = 0;
	mPreferredWidth     = 0;
	mPreferredHeight    = 0;
	mFontMetrics        = nsnull;
	mIsVisible          = PR_FALSE;
	mEnabled            = PR_TRUE;
	mIsScrolling        = PR_FALSE;
	mParent             = nsnull;
	mWindowParent       = nsnull;
	mUpdateArea = do_CreateInstance(kRegionCID);
	mForeground = NS_RGBA(0xFF,0xFF,0xFF,0xFF);
	mBackground = mForeground;
	mBWindowFeel        = B_NORMAL_WINDOW_FEEL;
	mBWindowLook        = B_NO_BORDER_WINDOW_LOOK;

	if (mUpdateArea)
	{
		mUpdateArea->Init();
		mUpdateArea->SetTo(0, 0, 0, 0);
	}
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
	if (NULL != mView) 
	{
		Destroy();
	}
	NS_IF_RELEASE(mFontMetrics);
}

NS_METHOD nsWindow::BeginResizingChildren(void)
{
	// HideKids(PR_TRUE) may be used here
	NS_NOTYETIMPLEMENTED("BeginResizingChildren not yet implemented"); // to be implemented
	return NS_OK;
}

NS_METHOD nsWindow::EndResizingChildren(void)
{
	// HideKids(PR_FALSE) may be used here
	NS_NOTYETIMPLEMENTED("EndResizingChildren not yet implemented"); // to be implemented
	return NS_OK;
}

NS_METHOD nsWindow::WidgetToScreen(const nsRect& aOldRect, nsRect& aNewRect)
{
	BPoint	point;
	point.x = aOldRect.x;
	point.y = aOldRect.y;
	if (mView && mView->LockLooper())
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
	if (mView && mView->LockLooper())
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
// Initialize an event to dispatch
//
//-------------------------------------------------------------------------
void nsWindow::InitEvent(nsGUIEvent& event, nsPoint* aPoint)
{
	NS_ADDREF(event.widget);

	if (nsnull == aPoint) // use the point from the event
	{
		// get the message position in client coordinates and in twips
		event.refPoint.x = 0;
		event.refPoint.y = 0;
	}
	else // use the point override if provided
	{
		event.refPoint.x = aPoint->x;
		event.refPoint.y = aPoint->y;
	}
	event.time = PR_IntervalNow();
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
//
// Dispatch Window Event
//
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
	nsGUIEvent event(PR_TRUE, aMsg, this);
	InitEvent(event);

	PRBool result = DispatchWindowEvent(&event);
	NS_RELEASE(event.widget);
	return result;
}

NS_IMETHODIMP nsWindow::PreCreateWidget(nsWidgetInitData *aInitData)
{
	if ( nsnull == aInitData)
		return NS_ERROR_FAILURE;
	
	SetWindowType(aInitData->mWindowType);
	SetBorderStyle(aInitData->mBorderStyle);
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Utility method for implementing both Create(nsIWidget ...) and
// Create(nsNativeWidget...)
//-------------------------------------------------------------------------
nsresult nsWindow::StandardWindowCreate(nsIWidget *aParent,
                                        const nsRect &aRect,
                                        EVENT_CALLBACK aHandleEventFunction,
                                        nsIDeviceContext *aContext,
                                        nsIAppShell *aAppShell,
                                        nsIToolkit *aToolkit,
                                        nsWidgetInitData *aInitData,
                                        nsNativeWidget aNativeParent)
{

	//Do as little as possible for invisible windows, why are these needed?
	if (aInitData->mWindowType == eWindowType_invisible)
		return NS_ERROR_FAILURE;
		
	NS_ASSERTION(aInitData->mWindowType == eWindowType_dialog
		|| aInitData->mWindowType == eWindowType_toplevel,
		"The windowtype is not handled by this class.");

	mIsTopWidgetWindow = PR_TRUE;
	
	BaseCreate(nsnull, aRect, aHandleEventFunction, aContext,
	           aAppShell, aToolkit, aInitData);

	mListenForResizes = aNativeParent ? PR_TRUE : aInitData->mListenForResizes;
		
	mParent = aParent;
	// Useful shortcut, wondering if we can use it also in GetParent() instead
	// nsIWidget* type mParent.
	mWindowParent = (nsWindow *)aParent;
	SetBounds(aRect);

	// Default mode for window, everything switched off.
	uint32 flags = B_NOT_RESIZABLE | B_NOT_MINIMIZABLE | B_NOT_ZOOMABLE
		| B_NOT_CLOSABLE | B_ASYNCHRONOUS_CONTROLS;

	//eBorderStyle_default is to ask the OS to handle it as it sees best.
	//eBorderStyle_all is same as top_level window default.
	if (eBorderStyle_default == mBorderStyle || eBorderStyle_all & mBorderStyle)
	{
		//(Firefox prefs doesn't go this way, so apparently it wants titlebar, zoom, 
		//resize and close.)

		//Look and feel for others are set ok at init.
		if (eWindowType_toplevel==mWindowType)
		{
			mBWindowLook = B_TITLED_WINDOW_LOOK;
			flags = B_ASYNCHRONOUS_CONTROLS;
		}
	}
	else
	{
		if (eBorderStyle_border & mBorderStyle)
			mBWindowLook = B_MODAL_WINDOW_LOOK;

		if (eBorderStyle_resizeh & mBorderStyle)
		{
			//Resize demands at least border
			mBWindowLook = B_MODAL_WINDOW_LOOK;
			flags &= !B_NOT_RESIZABLE;
		}

		//We don't have titlebar menus, so treat like title as it demands titlebar.
		if (eBorderStyle_title & mBorderStyle || eBorderStyle_menu & mBorderStyle)
			mBWindowLook = B_TITLED_WINDOW_LOOK;

		if (eBorderStyle_minimize & mBorderStyle)
			flags &= !B_NOT_MINIMIZABLE;

		if (eBorderStyle_maximize & mBorderStyle)
			flags &= !B_NOT_ZOOMABLE;

		if (eBorderStyle_close & mBorderStyle)
			flags &= !B_NOT_CLOSABLE;
	}

	nsWindowBeOS * w = new nsWindowBeOS(this, 
		BRect(aRect.x, aRect.y, aRect.x + aRect.width - 1, aRect.y + aRect.height - 1),
		"", mBWindowLook, mBWindowFeel, flags);
	if (!w)
		return NS_ERROR_OUT_OF_MEMORY;

	mView = new nsViewBeOS(this, w->Bounds(), "Toplevel view", B_FOLLOW_ALL, 0);

	if (!mView)
		return NS_ERROR_OUT_OF_MEMORY;

	w->AddChild(mView);
	// I'm wondering if we can move part of that code to above
	if (eWindowType_dialog == mWindowType && mWindowParent) 
	{
		nsWindow *topparent = mWindowParent;
		while (topparent->mWindowParent)
			topparent = topparent->mWindowParent;
		// may be got via mView and mView->Window() of topparent explicitly	
		BWindow* subsetparent = (BWindow *)
			topparent->GetNativeData(NS_NATIVE_WINDOW);
		if (subsetparent)
		{
			mBWindowFeel = B_FLOATING_SUBSET_WINDOW_FEEL;
			w->SetFeel(mBWindowFeel);
			w->AddToSubset(subsetparent);
		}
	} 
	// Run Looper. No proper destroy without it.
	w->Run();
	DispatchStandardEvent(NS_CREATE);
	return NS_OK;
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
	// Switch to the "main gui thread" if necessary... This method must
	// be executed on the "gui thread"...

	nsToolkit* toolkit = (nsToolkit *)mToolkit;
	if (toolkit && !toolkit->IsGuiThread())
	{
		nsCOMPtr<nsIWidget> widgetProxy;
		nsresult rv = NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
										NS_GET_IID(nsIWidget),
										this, 
										NS_PROXY_SYNC | NS_PROXY_ALWAYS, 
										getter_AddRefs(widgetProxy));
	
		if (NS_FAILED(rv))
			return rv;
		return widgetProxy->Create(aParent, aRect, aHandleEventFunction, aContext,
                           			aAppShell, aToolkit, aInitData);
	}
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
	// Switch to the "main gui thread" if necessary... This method must
	// be executed on the "gui thread"...

	nsToolkit* toolkit = (nsToolkit *)mToolkit;
	if (toolkit && !toolkit->IsGuiThread())
	{
		nsCOMPtr<nsIWidget> widgetProxy;
		nsresult rv = NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
										NS_GET_IID(nsIWidget),
										this, 
										NS_PROXY_SYNC | NS_PROXY_ALWAYS, 
										getter_AddRefs(widgetProxy));
	
		if (NS_FAILED(rv))
			return rv;
		return widgetProxy->Create(aParent, aRect, aHandleEventFunction, aContext,
                           			aAppShell, aToolkit, aInitData);
	}
	return(StandardWindowCreate(nsnull, aRect, aHandleEventFunction,
	                            aContext, aAppShell, aToolkit, aInitData,
	                            aParent));
}

#ifdef MOZ_CAIRO_GFX
gfxASurface*
nsWindow::GetThebesSurface()
{
	mThebesSurface = nsnull;
	if (!mThebesSurface) {
		mThebesSurface = new gfxBeOSSurface(mView);
	}
	return mThebesSurface;
}
#endif

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
	if (toolkit != nsnull && !toolkit->IsGuiThread())
	{
		nsCOMPtr<nsIWidget> widgetProxy;
		nsresult rv = NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
										NS_GET_IID(nsIWidget),
										this, 
										NS_PROXY_SYNC | NS_PROXY_ALWAYS, 
										getter_AddRefs(widgetProxy));
	
		if (NS_FAILED(rv))
			return rv;
		return widgetProxy->Destroy();
	}
	// Ok, now tell the nsBaseWidget class to clean up what it needs to
	if (!mIsDestroying)
	{
		nsBaseWidget::Destroy();
	}	
	//our windows can be subclassed by
	//others and these namless, faceless others
	//may not let us know about WM_DESTROY. so,
	//if OnDestroy() didn't get called, just call
	//it now.
	if (PR_FALSE == mOnDestroyCalled)
		OnDestroy();
	
	// Destroy the BView, if no mView, it is probably destroyed before
	// automatically with BWindow::Quit()
	if (mView)
	{
		// prevent the widget from causing additional events
		mEventCallback = nsnull;
	
		if (mView->LockLooper())
		{
			while(mView->ChildAt(0))
				mView->RemoveChild(mView->ChildAt(0));
			// destroy from inside
			BWindow	*w = mView->Window();
			// if no window, it was destroyed as result of B_QUIT_REQUESTED and 
			// took also all its children away
			if (w)
			{
				w->Sync();
				if (mView->Parent())
				{
					mView->Parent()->RemoveChild(mView);
					if (eWindowType_child != mWindowType)
						w->Quit();
					else
					w->Unlock();
				}
				else
				{
					w->RemoveChild(mView);
					w->Quit();
				}
			}
			else
				mView->RemoveSelf();

			delete mView;
		}

		// window is already gone
		mView = NULL;
	}
	mParent = nsnull;
	mWindowParent = nsnull;
	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Get this nsWindow parent
//
//-------------------------------------------------------------------------
nsIWidget* nsWindow::GetParent(void)
{
	//We cannot addref mParent directly
	nsIWidget	*widget = 0;
	if (mIsDestroying || mOnDestroyCalled)
		return nsnull;
	widget = (nsIWidget *)mParent;
	return  widget;
}


//-------------------------------------------------------------------------
//
// Hide or show this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Show(PRBool bState)
{
	if (!mEnabled)
		return NS_OK;
		

	if (!mView || !mView->LockLooper())
		return NS_OK;
		
	//We need to do the IsHidden() checks
	//because BeOS counts no of Hide()
	//and Show() checks. BeBook:
	// If Hide() is called more than once, you'll need to call Show()
	// an equal number of times for the window to become visible again.
	if (bState == PR_FALSE)
	{
		if (mView->Window() && !mView->Window()->IsHidden())
			mView->Window()->Hide();
	}
	else
	{
		if (mView->Window() && mView->Window()->IsHidden())
			mView->Window()->Show();
	}

	mView->UnlockLooper();
	mIsVisible = bState;	
	
	return NS_OK;
}
//-------------------------------------------------------------------------
// Set/unset mouse capture
//-------------------------------------------------------------------------
NS_METHOD nsWindow::CaptureMouse(PRBool aCapture)
{
	if (mView && mView->LockLooper())
	{
		if (PR_TRUE == aCapture)
			mView->SetEventMask(B_POINTER_EVENTS);
		else
			mView->SetEventMask(0);
		mView->UnlockLooper();
	}
	return NS_OK;
}
//-------------------------------------------------------------------------
// Capture Roolup Events
//-------------------------------------------------------------------------
NS_METHOD nsWindow::CaptureRollupEvents(nsIRollupListener * aListener, PRBool aDoCapture, PRBool aConsumeRollupEvent)
{
	if (!mEnabled)
		return NS_OK;
		
	if (aDoCapture) 
	{
		// we haven't bothered carrying a weak reference to gRollupWidget because
		// we believe lifespan is properly scoped. this next assertion helps 
		// assure that remains true.
		NS_ASSERTION(!gRollupWidget, "rollup widget reassigned before release");
		gRollupConsumeRollupEvent = aConsumeRollupEvent;
		NS_IF_RELEASE(gRollupListener);
		NS_IF_RELEASE(gRollupWidget);
		gRollupListener = aListener;
		NS_ADDREF(aListener);
		gRollupWidget = this;
		NS_ADDREF(this);
	} 
	else 
	{
		NS_IF_RELEASE(gRollupListener);
		NS_IF_RELEASE(gRollupWidget);
	}

	return NS_OK;
}

//-------------------------------------------------------------------------
// Check if event happened inside the given nsWindow
//-------------------------------------------------------------------------
PRBool nsWindow::EventIsInsideWindow(nsWindow* aWindow, nsPoint pos)
{
	BRect r;
	BWindow *window = (BWindow *)aWindow->GetNativeData(NS_NATIVE_WINDOW);
	if (window)
	{
		r = window->Frame();
	}
	else
	{
		// Bummer!
		return PR_FALSE;
	}

	if (pos.x < r.left || pos.x > r.right ||
	    pos.y < r.top || pos.y > r.bottom)
	{
		return PR_FALSE;
	}

	return PR_TRUE;
}

//-------------------------------------------------------------------------
// DealWithPopups
//
// Handle events that may cause a popup (combobox, XPMenu, etc) to need to rollup.
//-------------------------------------------------------------------------
PRBool
nsWindow::DealWithPopups(uint32 methodID, nsPoint pos)
{
	if (gRollupListener && gRollupWidget) 
	{
		// Rollup if the event is outside the popup.
		PRBool rollup = !nsWindow::EventIsInsideWindow((nsWindow*)gRollupWidget, pos);

		// If we're dealing with menus, we probably have submenus and we don't
		// want to rollup if the click is in a parent menu of the current submenu.
		if (rollup) 
		{
			nsCOMPtr<nsIMenuRollup> menuRollup ( do_QueryInterface(gRollupListener) );
			if ( menuRollup ) 
			{
				nsCOMPtr<nsISupportsArray> widgetChain;
				menuRollup->GetSubmenuWidgetChain ( getter_AddRefs(widgetChain) );
				if ( widgetChain ) 
				{
					PRUint32 count = 0;
					widgetChain->Count(&count);
					for ( PRUint32 i = 0; i < count; ++i ) 
					{
						nsCOMPtr<nsISupports> genericWidget;
						widgetChain->GetElementAt ( i, getter_AddRefs(genericWidget) );
						nsCOMPtr<nsIWidget> widget ( do_QueryInterface(genericWidget) );
						if ( widget ) 
						{
							nsIWidget* temp = widget.get();
							if ( nsWindow::EventIsInsideWindow((nsWindow*)temp, pos) ) 
							{
								rollup = PR_FALSE;
								break;
							}
						}
					} // foreach parent menu widget
				} // if widgetChain
			} // if rollup listener knows about menus
		} // if rollup

		if (rollup) 
		{
			gRollupListener->Rollup();

			if (gRollupConsumeRollupEvent) 
			{
				return PR_TRUE;
			}
		}
	} // if rollup listeners registered

	return PR_FALSE;
}


//-------------------------------------------------------------------------
//
// IsVisible
//
// Return PR_TRUE if the whether the component is visible, PR_FALSE otherwise
//-------------------------------------------------------------------------
NS_METHOD nsWindow::IsVisible(PRBool & bState)
{
	bState = mIsVisible && mView && mView->Visible();
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Hide window borders/decorations for this widget
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::HideWindowChrome(PRBool aShouldHide)
{
	if(mWindowType == eWindowType_child || mView == 0 || mView->Window() == 0)
		return NS_ERROR_FAILURE;
	// B_BORDERED 
	if (aShouldHide)
		mView->Window()->SetLook(B_NO_BORDER_WINDOW_LOOK);
	else
		mView->Window()->SetLook(mBWindowLook);
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Sanity check potential move coordinates
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::ConstrainPosition(PRBool aAllowSlop, PRInt32 *aX, PRInt32 *aY)
{
	if (mIsTopWidgetWindow && mView->Window()) 
	{
		BScreen screen;
		// If no valid screen, just return
		if (! screen.IsValid()) return NS_OK;
		
		BRect screen_rect = screen.Frame();
		BRect win_bounds = mView->Window()->Frame();

#ifdef DEBUG_CONSTRAIN_POSITION
		printf("ConstrainPosition: allowSlop=%s, x=%d, y=%d\n\tScreen :", (aAllowSlop?"T":"F"),*aX,*aY);
		screen_rect.PrintToStream();
		printf("\tWindow: ");
		win_bounds.PrintToStream();
#endif
		
		if (aAllowSlop) 
		{
			if (*aX < kWindowPositionSlop - win_bounds.IntegerWidth() + kWindowBorderWidth)
				*aX = kWindowPositionSlop - win_bounds.IntegerWidth() + kWindowBorderWidth;
			else if (*aX > screen_rect.IntegerWidth() - kWindowPositionSlop - kWindowBorderWidth)
				*aX = screen_rect.IntegerWidth() - kWindowPositionSlop - kWindowBorderWidth;
				
			if (*aY < kWindowPositionSlop - win_bounds.IntegerHeight() + kWindowTitleBarHeight)
				*aY = kWindowPositionSlop - win_bounds.IntegerHeight() + kWindowTitleBarHeight;
			else if (*aY > screen_rect.IntegerHeight() - kWindowPositionSlop - kWindowBorderWidth)
				*aY = screen_rect.IntegerHeight() - kWindowPositionSlop - kWindowBorderWidth;
				
		} 
		else 
		{
			
			if (*aX < kWindowBorderWidth)
				*aX = kWindowBorderWidth;
			else if (*aX > screen_rect.IntegerWidth() - win_bounds.IntegerWidth() - kWindowBorderWidth)
				*aX = screen_rect.IntegerWidth() - win_bounds.IntegerWidth() - kWindowBorderWidth;
				
			if (*aY < kWindowTitleBarHeight)
				*aY = kWindowTitleBarHeight;
			else if (*aY > screen_rect.IntegerHeight() - win_bounds.IntegerHeight() - kWindowBorderWidth)
				*aY = screen_rect.IntegerHeight() - win_bounds.IntegerHeight() - kWindowBorderWidth;
		}
	}
	return NS_OK;
}

void nsWindow::HideKids(PRBool state)	
{
	for (nsIWidget* kid = mFirstChild; kid; kid = kid->GetNextSibling()) 
	{
		nsWindow *childWidget = NS_STATIC_CAST(nsWindow*, kid);
		nsRect kidrect = ((nsWindow *)kid)->mBounds;
		//Don't bother about invisible
		if (mBounds.Intersects(kidrect))
		{	
			childWidget->Show(!state);
		}
	}
}

//-------------------------------------------------------------------------
//
// Move this component
//
//-------------------------------------------------------------------------
nsresult nsWindow::Move(PRInt32 aX, PRInt32 aY)
{
	// Only perform this check for non-popup windows, since the positioning can
	// in fact change even when the x/y do not.  We always need to perform the
	// check. See bug #97805 for details.
	if (mWindowType != eWindowType_popup && (mBounds.x == aX) && (mBounds.y == aY))
	{
		// Nothing to do, since it is already positioned correctly.
		return NS_OK;    
	}


	// Set cached value for lightweight and printing
	mBounds.x = aX;
	mBounds.y = aY;

	// We may reset children visibility here, but it needs special care 
	// - see comment 18 in Bug 311651. More sofisticated code needed.

	// until we lack separate window and widget, we "cannot" move BWindow without BView
	if (mView && mView->LockLooper())
	{
		if (mView->Parent() || !mView->Window())
			mView->MoveTo(aX, aY);
		else
			((nsWindowBeOS *)mView->Window())->MoveTo(aX, aY);
			
		mView->UnlockLooper();
	}

	OnMove(aX,aY);

	return NS_OK;
}



//-------------------------------------------------------------------------
//
// Resize this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{

	if (aWidth < 0 || aHeight < 0)
		return NS_OK;

	mBounds.width  = aWidth;
	mBounds.height = aHeight;
	
	// until we lack separate window and widget, we "cannot" resize BWindow without BView
	if (mView && mView->LockLooper())
	{
		if (mView->Parent() || !mView->Window())
			mView->ResizeTo(aWidth - 1, aHeight - 1);
		else
			((nsWindowBeOS *)mView->Window())->ResizeTo(aWidth - 1, aHeight - 1);

		mView->UnlockLooper();
	}


	OnResize(mBounds);
	if (aRepaint)
		Update();
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
	Move(aX,aY);
	Resize(aWidth,aHeight,aRepaint);
	return NS_OK;
}

NS_METHOD nsWindow::SetModal(PRBool aModal)
{
	if(!(mView && mView->Window()))
		return NS_ERROR_FAILURE;
	if(aModal)
	{
		window_feel newfeel;
		switch(mBWindowFeel)
		{
			case B_FLOATING_SUBSET_WINDOW_FEEL:
				newfeel = B_MODAL_SUBSET_WINDOW_FEEL;
				break;
 			case B_FLOATING_APP_WINDOW_FEEL:
				newfeel = B_MODAL_APP_WINDOW_FEEL;
				break;
 			case B_FLOATING_ALL_WINDOW_FEEL:
				newfeel = B_MODAL_ALL_WINDOW_FEEL;
				break;				
			default:
				return NS_OK;
		}
		mView->Window()->SetFeel(newfeel);
	}
	else
	{
		mView->Window()->SetFeel(mBWindowFeel);
	}
	return NS_OK;
}
//-------------------------------------------------------------------------
//
// Enable/disable this component
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Enable(PRBool aState)
{
	//TODO: Needs real corect implementation in future
	mEnabled = aState;
	return NS_OK;
}


NS_METHOD nsWindow::IsEnabled(PRBool *aState)
{
	NS_ENSURE_ARG_POINTER(aState);
	// looks easy enough, but...
	*aState = mEnabled;
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
	if (toolkit && !toolkit->IsGuiThread()) 
	{
		nsCOMPtr<nsIWidget> widgetProxy;
		nsresult rv = NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
										NS_GET_IID(nsIWidget),
										this, 
										NS_PROXY_SYNC | NS_PROXY_ALWAYS, 
										getter_AddRefs(widgetProxy));
	
		if (NS_FAILED(rv))
			return rv;
		return widgetProxy->SetFocus(aRaise);
	}
	
	// Don't set focus on disabled widgets or popups
	if (!mEnabled || eWindowType_popup == mWindowType)
		return NS_OK;
		
	if (mView && mView->LockLooper())
	{
		if (mView->Window() && 
		    aRaise == PR_TRUE &&
		    eWindowType_popup != mWindowType && 
			  !mView->Window()->IsActive() && 
			  gLastActiveWindow != mView->Window())
			mView->Window()->Activate(true);
			
		mView->MakeFocus(true);
		mView->UnlockLooper();
		DispatchFocus(NS_GOTFOCUS);
	}

	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get this component size and position in screen coordinates
//
//-------------------------------------------------------------------------    
NS_IMETHODIMP nsWindow::GetScreenBounds(nsRect &aRect)
{
	// A window's Frame() value is cached, so locking is not needed
	if (mView && mView->Window()) 
	{
		BRect r = mView->Window()->Frame();
		aRect.x = nscoord(r.left);
		aRect.y = nscoord(r.top);
		aRect.width  = r.IntegerWidth()+1;
		aRect.height = r.IntegerHeight()+1;
	} 
	else 
	{
		aRect = mBounds;
	}
	return NS_OK;
}  

//-------------------------------------------------------------------------
//
// Set the background/foreground color
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetBackgroundColor(const nscolor &aColor)
{
	nsBaseWidget::SetBackgroundColor(aColor);

	// We set the background of toplevel windows so that resizing doesn't show thru
	// to Desktop and resizing artifacts. Child windows has transparent background.
	if (!mIsTopWidgetWindow)
		return NS_OK;

	if (mView && mView->LockLooper())
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
	return mFontMetrics;
}


//-------------------------------------------------------------------------
//
// Set this component font
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::SetFont(const nsFont &aFont)
{
  // Cache Font for owner draw
	NS_IF_RELEASE(mFontMetrics);
	if (mContext)
		mContext->GetMetricsFor(aFont, mFontMetrics);
	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set this component cursor
//
//-------------------------------------------------------------------------

NS_METHOD nsWindow::SetCursor(nsCursor aCursor)
{
	if (!mView)
		return NS_ERROR_FAILURE;

	// Only change cursor if it's changing
	if (aCursor != mCursor) 
	{
		BCursor const *newCursor = B_CURSOR_SYSTEM_DEFAULT;
		
		// Check to see if the array has been loaded, if not, do it.
		if (gCursorArray.Count() == 0) 
		{
			gCursorArray.InsertElementAt((void*) new BCursor(cursorHyperlink),0);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorHorizontalDrag),1);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorVerticalDrag),2);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorUpperLeft),3);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorLowerRight),4);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorUpperRight),5);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorLowerLeft),6);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorCrosshair),7);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorHelp),8);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorGrab),9);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorGrabbing),10);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorCopy),11);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorAlias),12);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorWatch2),13);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorCell),14);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorZoomIn),15);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorZoomOut),16);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorLeft),17);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorRight),18);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorTop),19);
			gCursorArray.InsertElementAt((void*) new BCursor(cursorBottom),20);
		}

		switch (aCursor) 
		{
			case eCursor_standard:
			case eCursor_move:
				newCursor = B_CURSOR_SYSTEM_DEFAULT;
				break;
	
			case eCursor_select:
				newCursor = B_CURSOR_I_BEAM;
				break;
	
			case eCursor_hyperlink:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(0);
				break;
	
			case eCursor_n_resize:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(19);
				break;

			case eCursor_s_resize:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(20);
				break;
	
			case eCursor_w_resize:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(17);
				break;

			case eCursor_e_resize:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(18);
				break;
	
			case eCursor_nw_resize:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(3);
				break;
	
			case eCursor_se_resize:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(4);
				break;
	
			case eCursor_ne_resize:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(5);
				break;
	
			case eCursor_sw_resize:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(6);
				break;
	
			case eCursor_crosshair:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(7);
				break;
	
			case eCursor_help:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(8);
				break;
	
			case eCursor_copy:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(11);
				break;
	
			case eCursor_alias:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(12);
				break;

			case eCursor_context_menu:
				// XXX: No suitable cursor, needs implementing
				break;
				
			case eCursor_cell:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(14);
				break;

			case eCursor_grab:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(9);
				break;
	
			case eCursor_grabbing:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(10);
				break;
	
			case eCursor_wait:
			case eCursor_spinning:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(13);
				break;
	
			case eCursor_zoom_in:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(15);
				break;

			case eCursor_zoom_out:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(16);
				break;

			case eCursor_not_allowed:
			case eCursor_no_drop:
				// XXX: No suitable cursor, needs implementing
				break;

			case eCursor_col_resize:
				// XXX not 100% appropriate perhaps
				newCursor = (BCursor *)gCursorArray.SafeElementAt(1);
				break;

			case eCursor_row_resize:
				// XXX not 100% appropriate perhaps
				newCursor = (BCursor *)gCursorArray.SafeElementAt(2);
				break;

			case eCursor_vertical_text:
				// XXX not 100% appropriate perhaps
				newCursor = B_CURSOR_I_BEAM;
				break;

			case eCursor_all_scroll:
				// XXX: No suitable cursor, needs implementing
				break;

			case eCursor_nesw_resize:
				// XXX not 100% appropriate perhaps
				newCursor = (BCursor *)gCursorArray.SafeElementAt(1);
				break;

			case eCursor_nwse_resize:
				// XXX not 100% appropriate perhaps
				newCursor = (BCursor *)gCursorArray.SafeElementAt(1);
				break;

			case eCursor_ns_resize:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(2);
				break;

			case eCursor_ew_resize:
				newCursor = (BCursor *)gCursorArray.SafeElementAt(1);
				break;

			default:
				NS_ASSERTION(0, "Invalid cursor type");
				break;
		}
		NS_ASSERTION(newCursor != nsnull, "Cursor not stored in array properly!");
		mCursor = aCursor;
		be_app->SetCursor(newCursor, true);
	}
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Invalidate(PRBool aIsSynchronous)
{
	nsresult rv = NS_ERROR_FAILURE;
	// Asynchronous painting is performed with via nsViewBeOS::Draw() call and its message queue. 
	// All update rects are collected in nsViewBeOS member  "paintregion".
	// Flushing of paintregion happens in nsViewBeOS::GetPaintRegion(),
	// cleanup  - in nsViewBeOS::Validate(), called in OnPaint().
	BRegion reg;
	reg.MakeEmpty();
	if (mView && mView->LockLooper())
	{
		if (PR_TRUE == aIsSynchronous)
		{
			mView->paintregion.Include(mView->Bounds());
			reg.Include(mView->Bounds());
		}
		else
		{
			mView->Draw(mView->Bounds());
			rv = NS_OK;
		}
		mView->UnlockLooper();
	}
	// Instant repaint.
	if (PR_TRUE == aIsSynchronous)
		rv = OnPaint(&reg);
	return rv;
}

//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_METHOD nsWindow::Invalidate(const nsRect & aRect, PRBool aIsSynchronous)
{
	nsresult rv = NS_ERROR_FAILURE;
	// Very temporary region for double accounting.
	BRegion reg;
	reg.MakeEmpty();
	if (mView && mView->LockLooper()) 
	{
		BRect	r(aRect.x, 
				aRect.y, 
				aRect.x + aRect.width - 1, 
				aRect.y + aRect.height - 1);
		if (PR_TRUE == aIsSynchronous)
		{
			mView->paintregion.Include(r);
			reg.Include(r);
		}
		else
		{
			// we use Draw() instead direct addition to paintregion,
			// as it sets queue of notification messages for painting.
			mView->Draw(r);
			rv = NS_OK;
		}
		mView->UnlockLooper();
	}
	// Instant repaint - for given rect only. 
	// Don't repaint area which isn't marked here for synchronous repaint explicitly.
	// BRegion "reg" (equal to aRect) will be substracted from paintregion in OnPaint().
	if (PR_TRUE == aIsSynchronous)
		rv = OnPaint(&reg);
	return rv;
}

//-------------------------------------------------------------------------
//
// Invalidate this component visible area
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::InvalidateRegion(const nsIRegion *aRegion, PRBool aIsSynchronous)
{
	
	nsRegionRectSet *rectSet = nsnull;
	if (!aRegion)
		return NS_ERROR_FAILURE;
	nsresult rv = ((nsIRegion *)aRegion)->GetRects(&rectSet);
	if (NS_FAILED(rv))
		return rv;
	BRegion reg;
	reg.MakeEmpty();
	if (mView && mView->LockLooper())
	{
		for (PRUint32 i=0; i< rectSet->mRectsLen; ++i)
		{
			BRect br(rectSet->mRects[i].x, rectSet->mRects[i].y,
					rectSet->mRects[i].x + rectSet->mRects[i].width-1,
					rectSet->mRects[i].y + rectSet->mRects[i].height -1);
			if (PR_TRUE == aIsSynchronous)
			{
				mView->paintregion.Include(br);
				reg.Include(br);
			}
			else
			{
				mView->Draw(br);
				rv = NS_OK;
			}
		}
		mView->UnlockLooper();
	}
	// Instant repaint - for given region only. 
	// BRegion "reg"(equal to aRegion) will be substracted from paintregion in OnPaint().
	if (PR_TRUE == aIsSynchronous)
		rv = OnPaint(&reg);

	return rv;
}

//-------------------------------------------------------------------------
//
// Force a synchronous repaint of the window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsWindow::Update()
{
	nsresult rv = NS_ERROR_FAILURE;
	//Switching scrolling trigger off
	mIsScrolling = PR_FALSE;
	if (mWindowType == eWindowType_child)
		return NS_OK;
	BRegion reg;
	reg.MakeEmpty();
	if(mView && mView->LockLooper())
	{
		//Flushing native pending updates if any
		if (mView->Window())
			mView->Window()->UpdateIfNeeded();
		// Let app_server to invalidate
		mView->Invalidate();
		bool nonempty = mView->GetPaintRegion(&reg);
		mView->UnlockLooper();
		// Look if native update calls above filled update region and paint it
		if (nonempty)
			rv = OnPaint(&reg);
	}
	return rv;
}

//-------------------------------------------------------------------------
//
// Return some native data according to aDataType
//
//-------------------------------------------------------------------------
void* nsWindow::GetNativeData(PRUint32 aDataType)
{
	if (!mView)
		return NULL;	
	switch(aDataType) 
	{
		case NS_NATIVE_WINDOW:
			return (void *)(mView->Window());
		case NS_NATIVE_WIDGET:
		case NS_NATIVE_PLUGIN_PORT:
			return (void *)((nsViewBeOS *)mView);
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
	// Switching trigger on
	mIsScrolling = PR_TRUE;
	//Preventing main view invalidation loop-chain  when children are moving
	//by by hiding children nsWidgets.
	//Maybe this method must be used wider, in move and resize chains
	// and implemented in BeginResizingChildren or in Reset*Visibility() methods
	//Children will be unhidden in ::Update() when called by other than gkview::Scroll() method.
	HideKids(PR_TRUE);
	if (mView && mView->LockLooper())
	{
		// Kill any attempt to invalidate until scroll is finished
		mView->SetVisible(false);
		
		BRect src;
		BRect b = mView->Bounds();

		if (aClipRect)
		{
			src.left = aClipRect->x;
			src.top = aClipRect->y;
			src.right = aClipRect->XMost() - 1;
			src.bottom = aClipRect->YMost() - 1;
		}
		else
		{
			src = b;
		}
		// Restricting source by on-screen part of BView
		if (mView->Window())
		{
			BRect screenframe = mView->ConvertFromScreen(BScreen(mView->Window()).Frame());
			src = src & screenframe;
			if (mView->Parent())
			{
				BRect parentframe = mView->ConvertFromParent(mView->Parent()->Frame());
				src = src & parentframe;
			}
		}

		BRegion	invalid;
		invalid.Include(src);
		// Next source clipping check, for same level siblings
		if ( BView *v = mView->Parent() )
		{
			for (BView *child = v->ChildAt(0); child; child = child->NextSibling() )
			{
				BRect siblingframe = mView->ConvertFromParent(child->Frame());
				if (child != mView && child->Parent() != mView)
				{
					invalid.Exclude(siblingframe);
					mView->paintregion.Exclude(siblingframe);
				}
			}
			src = invalid.Frame();
		}

		// make sure we only reference visible bits
		// so we don't trigger a BView invalidate

		if (src.left + aDx < 0)
			src.left = -aDx;
		if (src.right + aDx > b.right)
			src.right = b.right - aDx;
		if (src.top + aDy < 0)
			src.top = -aDy;
		if (src.bottom + aDy > b.bottom)
			src.bottom = b.bottom - aDy;
		
		BRect dest = src.OffsetByCopy(aDx, aDy);
		mView->ConstrainClippingRegion(&invalid);
		// Moving visible content 
		if (src.IsValid() && dest.IsValid())
			mView->CopyBits(src, dest);

		invalid.Exclude(dest);	
		// Native paintregion needs shifting too, it is very important action
		// (as app_server doesn't know about Mozilla viewmanager tricks) -
		// it allows proper update after scroll for areas covered by other windows.
		mView->paintregion.OffsetBy(aDx, aDy);
		mView->ConstrainClippingRegion(&invalid);
		// Time to silently move now invisible children
		for (nsIWidget* kid = mFirstChild; kid; kid = kid->GetNextSibling()) 
		{
			nsWindow *childWidget = NS_STATIC_CAST(nsWindow*, kid);
			// No need to Lock/UnlockLooper with GetBounds() and Move() methods
			// using cached values and native MoveBy() instead
			nsRect bounds = childWidget->mBounds;
			bounds.x += aDx;
			bounds.y += aDy; 
			childWidget->Move(bounds.x, bounds.y);
			BView *child = ((BView *)kid->GetNativeData(NS_NATIVE_WIDGET));
			if (child)
				mView->paintregion.Exclude(child->Frame());
		}
		
		// Painting calculated region now,
		// letting Update() to paint remaining content of paintregion
		OnPaint(&invalid);
		HideKids(PR_FALSE);
		// re-allow updates
		mView->SetVisible(true);
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
	case nsSwitchToUIThread::CLOSEWINDOW :
		{
			NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
			if (eWindowType_popup != mWindowType && eWindowType_child != mWindowType)
				DealWithPopups(nsSwitchToUIThread::CLOSEWINDOW,nsPoint(0,0));

			// Bit more Kung-fu. We do care ourselves about children destroy notofication.
			// Including those floating dialogs we added to Gecko hierarchy in StandardWindowCreate()

			for (nsIWidget* kid = mFirstChild; kid; kid = kid->GetNextSibling()) 
			{
				nsWindow *childWidget = NS_STATIC_CAST(nsWindow*, kid);
				BWindow* kidwindow = (BWindow *)kid->GetNativeData(NS_NATIVE_WINDOW);
				if (kidwindow)
				{
					// PostMessage() is unsafe, so using BMessenger
					BMessenger bm(kidwindow);
					bm.SendMessage(B_QUIT_REQUESTED);
				}
			}
			DispatchStandardEvent(NS_DESTROY);
		}
		break;

#ifdef DEBUG_FOCUS
	case nsSwitchToUIThread::GOT_FOCUS:
		NS_ASSERTION(info->nArgs == 1, "Wrong number of arguments to CallMethod");
		if (!mEnabled)
			return false;
		if ((uint32)info->args[0] != (uint32)mView)
			printf("Wrong view to get focus\n");*/
		break;
#endif
	case nsSwitchToUIThread::KILL_FOCUS:
		NS_ASSERTION(info->nArgs == 1, "Wrong number of arguments to CallMethod");
		if ((uint32)info->args[0] == (uint32)mView)
			DispatchFocus(NS_LOSTFOCUS);
#ifdef DEBUG_FOCUS
		else
			printf("Wrong view to de-focus\n");
#endif
#if defined BeIME
		nsIMEBeOS::GetIME()->DispatchCancelIME();
		if (mView && mView->LockLooper())
 		{
 			mView->SetFlags(mView->Flags() & ~B_NAVIGABLE);
 			mView->UnlockLooper();
 		}
#endif
		break;

	case nsSwitchToUIThread::BTNCLICK :
		{
			NS_ASSERTION(info->nArgs == 6, "Wrong number of arguments to CallMethod");
			if (!mEnabled)
				return false;
			// close popup when clicked outside of the popup window
			uint32 eventID = ((int32 *)info->args)[0];
			PRBool rollup = PR_FALSE;

			if (eventID == NS_MOUSE_BUTTON_DOWN &&
			        mView && mView->LockLooper())
			{
				BPoint p(((int32 *)info->args)[1], ((int32 *)info->args)[2]);
				mView->ConvertToScreen(&p);
				rollup = DealWithPopups(nsSwitchToUIThread::ONMOUSE, nsPoint(p.x, p.y));
				mView->UnlockLooper();
			}
			// Drop click event - bug 314330
			if (rollup)
				return false;
			DispatchMouseEvent(((int32 *)info->args)[0],
			                   nsPoint(((int32 *)info->args)[1], ((int32 *)info->args)[2]),
			                   ((int32 *)info->args)[3],
			                   ((int32 *)info->args)[4],
			                   ((int32 *)info->args)[5]);

			if (((int32 *)info->args)[0] == NS_MOUSE_BUTTON_DOWN &&
			    ((int32 *)info->args)[5] == nsMouseEvent::eRightButton)
			{
				DispatchMouseEvent (NS_CONTEXTMENU,
				                    nsPoint(((int32 *)info->args)[1], ((int32 *)info->args)[2]),
				                    ((int32 *)info->args)[3],
				                    ((int32 *)info->args)[4],
				                    ((int32 *)info->args)[5]);
			}
		}
		break;

	case nsSwitchToUIThread::ONWHEEL :
		{
			NS_ASSERTION(info->nArgs == 1, "Wrong number of arguments to CallMethod");
			// avoid mistargeting
			if ((uint32)info->args[0] != (uint32)mView)
				return false;
			BPoint cursor(0,0);
			uint32 buttons;
			BPoint delta;
			if (mView && mView->LockLooper())
			{
				mView->GetMouse(&cursor, &buttons, false);
				delta = mView->GetWheel();
				mView->UnlockLooper();
			}
			else
				return false;
			// BeOS TwoWheel input-filter is bit buggy atm, generating sometimes X-wheel with no reason,
			// so we're setting priority for Y-wheel.
			// Also hardcoding here _system_ scroll-step value to 3 lines.
			if (nscoord(delta.y) != 0)
			{
				OnWheel(nsMouseScrollEvent::kIsVertical, buttons, cursor, nscoord(delta.y)*3);
			}
			else if(nscoord(delta.x) != 0)
				OnWheel(nsMouseScrollEvent::kIsHorizontal, buttons, cursor, nscoord(delta.x)*3);
		}
		break;

	case nsSwitchToUIThread::ONKEY :
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

	case nsSwitchToUIThread::ONPAINT :
		NS_ASSERTION(info->nArgs == 1, "Wrong number of arguments to CallMethod");
		{
			if ((uint32)mView != ((uint32 *)info->args)[0])
				return false;
			BRegion reg;
			reg.MakeEmpty();
			if(mView && mView->LockLooper())
			{
				bool nonempty = mView->GetPaintRegion(&reg);
				mView->UnlockLooper();
				if (nonempty)
					OnPaint(&reg);
			}
		}
		break;

	case nsSwitchToUIThread::ONRESIZE :
		{
			NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
			if (eWindowType_popup != mWindowType && eWindowType_child != mWindowType)
				DealWithPopups(nsSwitchToUIThread::ONRESIZE,nsPoint(0,0));
			// This should be called only from BWindow::FrameResized()
			if (!mIsTopWidgetWindow  || !mView  || !mView->Window())
				return false;
			
			nsRect r(mBounds);
			if (mView->LockLooper())
			{
				BRect br = mView->Frame();
				r.x = nscoord(br.left);
				r.y = nscoord(br.top);
				r.width  = br.IntegerWidth() + 1;
				r.height = br.IntegerHeight() + 1;
				((nsWindowBeOS *)mView->Window())->fJustGotBounds = true;
				mView->UnlockLooper();
			}
			OnResize(r);
		}
		break;

	case nsSwitchToUIThread::ONMOUSE :
		{
			NS_ASSERTION(info->nArgs == 4, "Wrong number of arguments to CallMethod");
			if (!mEnabled)
				return false;
			DispatchMouseEvent(((int32 *)info->args)[0],
			                   nsPoint(((int32 *)info->args)[1], ((int32 *)info->args)[2]),
			                   0,
		    	               ((int32 *)info->args)[3]);
		}
		break;

	case nsSwitchToUIThread::ONDROP :
		{
			NS_ASSERTION(info->nArgs == 4, "Wrong number of arguments to CallMethod");

			nsMouseEvent event(PR_TRUE, (int32)  info->args[0], this, nsMouseEvent::eReal);
			nsPoint point(((int32 *)info->args)[1], ((int32 *)info->args)[2]);
			InitEvent (event, &point);
			uint32 mod = (uint32) info->args[3];
			event.isShift   = mod & B_SHIFT_KEY;
			event.isControl = mod & B_CONTROL_KEY;
			event.isAlt     = mod & B_COMMAND_KEY;
			event.isMeta     = mod & B_OPTION_KEY;

			// Setting drag action, must be done before event dispatch
			nsCOMPtr<nsIDragService> dragService = do_GetService(kCDragServiceCID);
			if (dragService)
			{
				nsCOMPtr<nsIDragSession> dragSession;
				dragService->GetCurrentSession(getter_AddRefs(dragSession));
				if (dragSession)
				{
					// Original action mask stored in dragsession.
					// For native events such mask must be set in nsDragServiceBeOS::UpdateDragMessageIfNeeded()
	
					PRUint32 action_mask = 0;
					dragSession->GetDragAction(&action_mask);
					PRUint32 action = nsIDragService::DRAGDROP_ACTION_MOVE;
					if (mod & B_OPTION_KEY)
					{
						if (mod & B_COMMAND_KEY)
							action = nsIDragService::DRAGDROP_ACTION_LINK & action_mask;
						else
							action = nsIDragService::DRAGDROP_ACTION_COPY & action_mask;
					}
					dragSession->SetDragAction(action);
				}
			}
			DispatchWindowEvent(&event);
			NS_RELEASE(event.widget);

			if (dragService)
				dragService->EndDragSession(PR_TRUE);
		}
		break;

	case nsSwitchToUIThread::ONACTIVATE:
		NS_ASSERTION(info->nArgs == 2, "Wrong number of arguments to CallMethod");
		if (!mEnabled || eWindowType_popup == mWindowType || 0 == mView->Window())
			return false;
		if ((BWindow *)info->args[1] != mView->Window())
			return false;
		if (mEventCallback || eWindowType_child == mWindowType )
		{
			bool active = (bool)info->args[0];
			if (!active) 
			{
				if (eWindowType_dialog == mWindowType || 
				    eWindowType_toplevel == mWindowType)
					DealWithPopups(nsSwitchToUIThread::ONACTIVATE,nsPoint(0,0));
				//Testing if BWindow is really deactivated.
				if (!mView->Window()->IsActive())
				{
					// BeOS is poor in windows hierarchy and variations support. In lot of aspects.
					// Here is workaround for flacky Activate() handling for B_FLOATING windows.
					// We should force parent (de)activation to allow main window to regain control after closing floating dialog.
					if (mWindowParent &&  mView->Window()->IsFloating())
						mWindowParent->DispatchFocus(NS_ACTIVATE);

					DispatchFocus(NS_DEACTIVATE);
#if defined(BeIME)
					nsIMEBeOS::GetIME()->DispatchCancelIME();
#endif
				}
			} 
			else 
			{

				if (mView->Window()->IsActive())
				{
					// See comment above.
					if (mWindowParent &&  mView->Window()->IsFloating())
						mWindowParent->DispatchFocus(NS_DEACTIVATE);
					
					DispatchFocus(NS_ACTIVATE);
					if (mView && mView->Window())
						gLastActiveWindow = mView->Window();
				}
			}
		}
		break;

	case nsSwitchToUIThread::ONMOVE:
		{
			NS_ASSERTION(info->nArgs == 0, "Wrong number of arguments to CallMethod");
			nsRect r;
			// We use this only for tracking whole window moves
			GetScreenBounds(r);		
			if (eWindowType_popup != mWindowType && eWindowType_child != mWindowType)
				DealWithPopups(nsSwitchToUIThread::ONMOVE,nsPoint(0,0));
			OnMove(r.x, r.y);
		}
		break;
		
	case nsSwitchToUIThread::ONWORKSPACE:
		{
			NS_ASSERTION(info->nArgs == 2, "Wrong number of arguments to CallMethod");
			if (eWindowType_popup != mWindowType && eWindowType_child != mWindowType)
				DealWithPopups(nsSwitchToUIThread::ONWORKSPACE,nsPoint(0,0));
		}
		break;

#if defined(BeIME)
 	case nsSwitchToUIThread::ONIME:
 		//No assertion used, as number of arguments varies here
 		if (mView && mView->LockLooper())
 		{
 			mView->SetFlags(mView->Flags() | B_NAVIGABLE);
 			mView->UnlockLooper();
 		}
 		nsIMEBeOS::GetIME()->RunIME(info->args, this, mView);
 		break;
#endif
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

	        // The "Windows Key"
	        { NS_VK_META,       0x66 },
	        { NS_VK_META,       0x67 },

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
#ifdef KB_DEBUG
	printf("TranslateBeOSKeyCode: bekeycode = 0x%x\n",bekeycode);
#endif
	int i;
	int length = sizeof(nsKeycodesBeOS) / sizeof(struct nsKeyConverter);
	int length_numlock = sizeof(nsKeycodesBeOSNumLock) / sizeof(struct nsKeyConverter);
	int length_nonumlock = sizeof(nsKeycodesBeOSNoNumLock) / sizeof(struct nsKeyConverter);

	// key code conversion
	for (i = 0; i < length; i++)
	{
		if (nsKeycodesBeOS[i].bekeycode == bekeycode)
			return(nsKeycodesBeOS[i].vkCode);
	}
	// numpad keycode vary with numlock
	if (isnumlock)
	{
		for (i = 0; i < length_numlock; i++)
		{
			if (nsKeycodesBeOSNumLock[i].bekeycode == bekeycode)
				return(nsKeycodesBeOSNumLock[i].vkCode);
		}
	}
	else
	{
		for (i = 0; i < length_nonumlock; i++)
		{
			if (nsKeycodesBeOSNoNumLock[i].bekeycode == bekeycode)
				return(nsKeycodesBeOSNoNumLock[i].vkCode);
		}
	}
#ifdef KB_DEBUG
	printf("TranslateBeOSKeyCode: ####### Translation not Found #######\n");
#endif
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
	PRBool noDefault = PR_FALSE;

	mIsShiftDown   = (mod & B_SHIFT_KEY) ? PR_TRUE : PR_FALSE;
	mIsControlDown = (mod & B_CONTROL_KEY) ? PR_TRUE : PR_FALSE;
	mIsAltDown     = ((mod & B_COMMAND_KEY) && !(mod & B_RIGHT_OPTION_KEY))? PR_TRUE : PR_FALSE;
	mIsMetaDown    = (mod & B_LEFT_OPTION_KEY) ? PR_TRUE : PR_FALSE;	
	bool IsNumLocked = ((mod & B_NUM_LOCK) != 0);

	aTranslatedKeyCode = TranslateBeOSKeyCode(bekeycode, IsNumLocked);

	if (numBytes <= 1)
	{
		noDefault  = DispatchKeyEvent(NS_KEY_DOWN, 0, aTranslatedKeyCode);
	}
	else
	{
		//   non ASCII chars
	}

	// ------------  On Char  ------------
	PRUint32	uniChar;

	if ((mIsControlDown || mIsAltDown || mIsMetaDown) && rawcode >= 'a' && rawcode <= 'z')
	{
		if (mIsShiftDown)
			uniChar = rawcode + 'A' - 'a';
		else
			uniChar = rawcode;
		aTranslatedKeyCode = 0;
	} 
	else
	{
		if (numBytes == 0) // deal with unmapped key
			return noDefault;

		switch((unsigned char)bytes[0])
		{
		case 0xc8://System Request
		case 0xca://Break
			return noDefault;// do not send 'KEY_PRESS' message

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
			if (numBytes >= 1 && (bytes[0] & 0x80) == 0)
			{
				// 1 byte utf-8 char
				uniChar = bytes[0];
			} 
			else
			{
				if (numBytes >= 2 && (bytes[0] & 0xe0) == 0xc0)
				{
					// 2 byte utf-8 char
					uniChar = ((uint16)(bytes[0] & 0x1f) << 6) | (uint16)(bytes[1] & 0x3f);
				}
				else
				{
					if (numBytes >= 3 && (bytes[0] & 0xf0) == 0xe0)
					{
						// 3 byte utf-8 char
						uniChar = ((uint16)(bytes[0] & 0x0f) << 12) | ((uint16)(bytes[1] & 0x3f) << 6)
						          | (uint16)(bytes[2] & 0x3f);
					}
					else
					{
						//error
						uniChar = 0;
						NS_WARNING("nsWindow::OnKeyDown() error: bytes[] has not enough chars.");
					}
				}
			}
			aTranslatedKeyCode = 0;
			break;
		}
	}

	// If prevent default set for onkeydown, do the same for onkeypress
	PRUint32 extraFlags = (noDefault ? NS_EVENT_FLAG_NO_DEFAULT : 0);
	return DispatchKeyEvent(NS_KEY_PRESS, uniChar, aTranslatedKeyCode, extraFlags) && noDefault;
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
	mIsAltDown     = ((mod & B_COMMAND_KEY) && !(mod & B_RIGHT_OPTION_KEY))? PR_TRUE : PR_FALSE;
	mIsMetaDown    = (mod & B_LEFT_OPTION_KEY) ? PR_TRUE : PR_FALSE;	

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
                                  PRUint32 aKeyCode, PRUint32 aFlags)
{
	nsKeyEvent event(PR_TRUE, aEventType, this);
	nsPoint point;

	point.x = 0;
	point.y = 0;

	InitEvent(event, &point); // this add ref's event.widget

	event.flags |= aFlags;
	event.charCode = aCharCode;
	event.keyCode  = aKeyCode;

#ifdef KB_DEBUG
	static int cnt=0;
	printf("%d DispatchKE Type: %s charCode 0x%x  keyCode 0x%x ", cnt++,
	       (NS_KEY_PRESS == aEventType)?"PRESS":(aEventType == NS_KEY_UP?"Up":"Down"),
	       event.charCode, event.keyCode);
	printf("Shift: %s Control %s Alt: %s Meta: %s\n",  
	       (mIsShiftDown?"D":"U"), 
	       (mIsControlDown?"D":"U"), 
	       (mIsAltDown?"D":"U"),
	       (mIsMetaDown?"D":"U"));
#endif

	event.isShift   = mIsShiftDown;
	event.isControl = mIsControlDown;
	event.isMeta   =  mIsMetaDown;
	event.isAlt     = mIsAltDown;

	PRBool result = DispatchWindowEvent(&event);
	NS_RELEASE(event.widget);

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

	// release references to children, device context, toolkit, and app shell
	nsBaseWidget::OnDestroy();

	// dispatch the event
	if (!mIsDestroying) 
	{
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
	nsGUIEvent event(PR_TRUE, NS_MOVE, this);
	InitEvent(event);
	event.refPoint.x = aX;
	event.refPoint.y = aY;

	PRBool result = DispatchWindowEvent(&event);
	NS_RELEASE(event.widget);
	return result;
}

void nsWindow::OnWheel(PRInt32 aDirection, uint32 aButtons, BPoint aPoint, nscoord aDelta)
{
		// we don't use the mIsXDown bools because
		// they get reset on Gecko reload (makes it harder
		// to use stuff like Alt+Wheel)

		nsMouseScrollEvent scrollEvent(PR_TRUE, NS_MOUSE_SCROLL, this);
		uint32 mod (modifiers());
		scrollEvent.isControl = mod & B_CONTROL_KEY;
		scrollEvent.isShift = mod & B_SHIFT_KEY;
		scrollEvent.isAlt   = mod & B_COMMAND_KEY;
		scrollEvent.isMeta  = mod & B_OPTION_KEY;
						
		scrollEvent.scrollFlags = aDirection;
		scrollEvent.delta = aDelta;
		scrollEvent.time      = PR_IntervalNow();
		scrollEvent.refPoint.x = nscoord(aPoint.x);
		scrollEvent.refPoint.y = nscoord(aPoint.y);

		nsEventStatus rv;
		DispatchEvent (&scrollEvent, rv);
}

//-------------------------------------------------------------------------
//
// Paint
//
//-------------------------------------------------------------------------
nsresult nsWindow::OnPaint(BRegion *breg)
{
	nsresult rv = NS_ERROR_FAILURE;
	if (mView && mView->LockLooper())
	{
		// Substracting area from paintregion
		mView->Validate(breg);
		// looks like it should be done by Mozilla via nsRenderingContext methods,
		// but we saw in some cases how it follows Win32 ideas and don't care about clipping there
		mView->ConstrainClippingRegion(breg);
		mView->UnlockLooper();
	}
	else
		return rv;
	BRect br = breg->Frame();
	if (!br.IsValid() || !mEventCallback || !mView  || (eWindowType_child != mWindowType && eWindowType_popup != mWindowType))
		return rv;
	nsRect nsr(nscoord(br.left), nscoord(br.top), 
			nscoord(br.IntegerWidth() + 1), nscoord(br.IntegerHeight() + 1));
	mUpdateArea->SetTo(0,0,0,0);
	int numrects = breg->CountRects();
	for (int i = 0; i< numrects; i++)
	{
		BRect br = breg->RectAt(i);
		mUpdateArea->Union(int(br.left), int(br.top), 
							br.IntegerWidth() + 1, br.IntegerHeight() + 1);
	}	

	nsIRenderingContext* rc = GetRenderingContext();
	// Double buffering for cairo builds is done here
#ifdef MOZ_CAIRO_GFX
	nsRefPtr<gfxContext> ctx =
		(gfxContext*)rc->GetNativeGraphicData(nsIRenderingContext::NATIVE_THEBES_CONTEXT);
	ctx->Save();

	// Clip
	ctx->NewPath();
	for (int i = 0; i< numrects; i++)
	{
		BRect br = breg->RectAt(i);
		ctx->Rectangle(gfxRect(int(br.left), int(br.top), 
			       br.IntegerWidth() + 1, br.IntegerHeight() + 1));
	}
	ctx->Clip();

	// double buffer
	ctx->PushGroup(gfxContext::CONTENT_COLOR);
#endif

	nsPaintEvent event(PR_TRUE, NS_PAINT, this);

	InitEvent(event);
	event.region = mUpdateArea;
	event.rect = &nsr;
	event.renderingContext = rc;
	if (event.renderingContext != nsnull)
	{
		// TODO: supply nsRenderingContextBeOS with font, colors and other state variables here.
		// It will help toget rid of some hacks in LockAndUpdateView and
		// allow non-permanent nsDrawingSurface for BeOS - currently it fails for non-bitmapped BViews/widgets.
		// Something like this:
		//if (mFontMetrics)
		//	event.renderingContext->SetFont(mFontMetrics);
		rv = DispatchWindowEvent(&event) ? NS_OK : NS_ERROR_FAILURE;
		NS_RELEASE(event.renderingContext);
	}

	NS_RELEASE(event.widget);

#ifdef MOZ_CAIRO_GFX
	// The second half of double buffering
	if (rv == NS_OK) {
		ctx->SetOperator(gfxContext::OPERATOR_SOURCE);
		ctx->PopGroupToSource();
		ctx->Paint();
	} else {
		// ignore
		ctx->PopGroup();
	}

	ctx->Restore();
#endif

	return rv;
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
		nsSizeEvent event(PR_TRUE, NS_SIZE, this);
		InitEvent(event);
		event.windowSize = &aWindowRect;
		// We have same size for windows rect and "client area" rect
		event.mWinWidth  = aWindowRect.width;
		event.mWinHeight = aWindowRect.height;
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
PRBool nsWindow::DispatchMouseEvent(PRUint32 aEventType, nsPoint aPoint, PRUint32 clicks, PRUint32 mod,
                                    PRUint16 aButton)
{
	PRBool result = PR_FALSE;
	if (nsnull != mEventCallback || nsnull != mMouseListener)
	{
		nsMouseEvent event(PR_TRUE, aEventType, this, nsMouseEvent::eReal);
		InitEvent (event, &aPoint);
		event.isShift   = mod & B_SHIFT_KEY;
		event.isControl = mod & B_CONTROL_KEY;
		event.isAlt     = mod & B_COMMAND_KEY;
		event.isMeta     = mod & B_OPTION_KEY;
		event.clickCount = clicks;
		event.button = aButton;

		// call the event callback
		if (nsnull != mEventCallback)
		{
			result = DispatchWindowEvent(&event);
			NS_RELEASE(event.widget);
			return result;
		}
		else
		{
			switch(aEventType)
			{
			case NS_MOUSE_MOVE :
				result = ConvertStatus(mMouseListener->MouseMoved(event));
				break;

			case NS_MOUSE_BUTTON_DOWN :
				result = ConvertStatus(mMouseListener->MousePressed(event));
				break;

			case NS_MOUSE_BUTTON_UP :
				result = ConvertStatus(mMouseListener->MouseReleased(event)) && ConvertStatus(mMouseListener->MouseClicked(event));
				break;
			}
			NS_RELEASE(event.widget);
			return result;
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
	if (mEventCallback)
		return(DispatchStandardEvent(aEventType));

	return PR_FALSE;
}

NS_METHOD nsWindow::SetTitle(const nsAString& aTitle)
{
	if (mView && mView->LockLooper())
	{
		mView->Window()->SetTitle(NS_ConvertUTF16toUTF8(aTitle).get());
		mView->UnlockLooper();
	}
	return NS_OK;
}

//----------------------------------------------------
//
// Get/Set the preferred size
//
//----------------------------------------------------
NS_METHOD nsWindow::GetPreferredSize(PRInt32& aWidth, PRInt32& aHeight)
{
	// TODO:  Check to see how often this is called.  If too much, leave as is,
	// otherwise, call mView->GetPreferredSize
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
}

nsIWidgetStore::~nsIWidgetStore()
{
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
	fJustGotBounds = true;
}

nsWindowBeOS::~nsWindowBeOS()
{
	//placeholder for clean up
}

bool nsWindowBeOS::QuitRequested( void )
{
	if (CountChildren() != 0)
	{
		nsWindow	*w = (nsWindow *)GetMozillaWidget();
		nsToolkit	*t;
		if (w && (t = w->GetToolkit()) != 0)
		{
			MethodInfo *info = nsnull;
			if (nsnull != (info = new MethodInfo(w, w, nsSwitchToUIThread::CLOSEWINDOW)))
				t->CallMethodAsync(info);
			NS_RELEASE(t);
		}
	}
	return true;
}

void nsWindowBeOS::MessageReceived(BMessage *msg)
{
	// Temp replacement for real DnD. Supports file drop onto window.
	if (msg->what == B_SIMPLE_DATA)
	{
		printf("BWindow::SIMPLE_DATA\n");
		be_app_messenger.SendMessage(msg);
	}
	BWindow::MessageReceived(msg);
}

// This function calls KeyDown() for Alt+whatever instead of app_server,
// also used for Destroy workflow 
void nsWindowBeOS::DispatchMessage(BMessage *msg, BHandler *handler)
{
	if (msg->what == B_KEY_DOWN && modifiers() & B_COMMAND_KEY)
	{
		BString bytes;
		if (B_OK == msg->FindString("bytes", &bytes))
		{
			BView *view = this->CurrentFocus();
			if (view)
				view->KeyDown(bytes.String(), bytes.Length());
		}
		if (strcmp(bytes.String(),"w") && strcmp(bytes.String(),"W"))
			BWindow::DispatchMessage(msg, handler);
	}
	// In some cases the message don't reach QuitRequested() hook,
	// so do it here
	else if(msg->what == B_QUIT_REQUESTED)
	{
		// tells nsWindow to kill me
		nsWindow	*w = (nsWindow *)GetMozillaWidget();
		nsToolkit	*t;
		if (w && (t = w->GetToolkit()) != 0)
		{
			MethodInfo *info = nsnull;
			if (nsnull != (info = new MethodInfo(w, w, nsSwitchToUIThread::CLOSEWINDOW)))
				t->CallMethodAsync(info);
			NS_RELEASE(t);
		}		
	}
	else
		BWindow::DispatchMessage(msg, handler);
}

//This method serves single purpose here - allows Mozilla to save current window position,
//and restore position on new start. 
void nsWindowBeOS::FrameMoved(BPoint origin)
{	

	//determine if the window position actually changed
	if (origin.x == lastWindowPoint.x && origin.x == lastWindowPoint.x) 
	{
		//it didn't - don't bother
		return;
	}
	lastWindowPoint = origin;
	nsWindow  *w = (nsWindow *)GetMozillaWidget();
	nsToolkit *t;
	if (w && (t = w->GetToolkit()) != 0) 
	{
		MethodInfo *info = nsnull;
		if (nsnull != (info = new MethodInfo(w, w, nsSwitchToUIThread::ONMOVE)))
			t->CallMethodAsync(info);
		NS_RELEASE(t);
	}
}

void nsWindowBeOS::WindowActivated(bool active)
{
// Calls method ONACTIVATE to dispatch focus ACTIVATE messages
	nsWindow        *w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if (w && (t = w->GetToolkit()) != 0)
	{
		uint32	args[2];
		args[0] = (uint32)active;
		args[1] = (uint32)this;
		MethodInfo *info = nsnull;
		if (nsnull != (info = new MethodInfo(w, w, nsSwitchToUIThread::ONACTIVATE, 2, args)))
			t->CallMethodAsync(info);
		NS_RELEASE(t);
	}
}

void  nsWindowBeOS::WorkspacesChanged(uint32 oldworkspace, uint32 newworkspace)
{
	if (oldworkspace == newworkspace)
		return;
	nsWindow        *w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if (w && (t = w->GetToolkit()) != 0)
	{
		uint32	args[2];
		args[0] = newworkspace;
		args[1] = oldworkspace;
		MethodInfo *info = nsnull;
		if (nsnull != (info = new MethodInfo(w, w, nsSwitchToUIThread::ONWORKSPACE, 2, args)))
			t->CallMethodAsync(info);
		NS_RELEASE(t);
	}	
}

void  nsWindowBeOS::FrameResized(float width, float height)
{
	// We have send message already, and Mozilla still didn't get it
	// so don't poke it endlessly with no reason
	if (!fJustGotBounds)
		return;
	nsWindow        *w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if (w && (t = w->GetToolkit()) != 0)
	{
		MethodInfo *info = nsnull;
		if (nsnull != (info = new MethodInfo(w, w, nsSwitchToUIThread::ONRESIZE)))
		{
			//Memorize fact of sending message
			if (t->CallMethodAsync(info))
				fJustGotBounds = false;
		}
		NS_RELEASE(t);
	}	
}

//----------------------------------------------------
// BeOS Sub-Class View
//----------------------------------------------------

nsViewBeOS::nsViewBeOS(nsIWidget *aWidgetWindow, BRect aFrame, const char *aName, uint32 aResizingMode, uint32 aFlags)
	: BView(aFrame, aName, aResizingMode, aFlags), nsIWidgetStore(aWidgetWindow), wheel(.0,.0)
{
	SetViewColor(B_TRANSPARENT_COLOR);
	paintregion.MakeEmpty();	
	buttons = 0;
	fRestoreMouseMask = false;
	fJustValidated = true;
	fWheelDispatched = true;
	fVisible = true;
}

void nsViewBeOS::SetVisible(bool visible)
{
	if (visible)
		SetFlags(Flags() | B_WILL_DRAW);
	else
		SetFlags(Flags() & ~B_WILL_DRAW);
	fVisible = visible;
}

inline bool nsViewBeOS::Visible()
{
	return fVisible;
}
 
void nsViewBeOS::Draw(BRect updateRect)
{
	// Ignore all, we are scrolling.
	if (!fVisible)
		return;

	paintregion.Include(updateRect);

	// We have send message already, and Mozilla still didn't get it
	// so don't poke it endlessly with no reason. Also don't send message
	// if update region is empty.
	if (paintregion.CountRects() == 0 || !paintregion.Frame().IsValid() || !fJustValidated)
		return;
	uint32	args[1];
	args[0] = (uint32)this;
	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if (w && (t = w->GetToolkit()) != 0)
	{
		MethodInfo *info = nsnull;
		info = new MethodInfo(w, w, nsSwitchToUIThread::ONPAINT, 1, args);
		if (info)
		{
			//Memorize fact of sending message
			if (t->CallMethodAsync(info))
				fJustValidated = false;
		}
		NS_RELEASE(t);
	}
}

// Method to get update rects for asynchronous drawing.
bool nsViewBeOS::GetPaintRegion(BRegion *r)
{

	// Mozilla got previous ONPAINT message,
	// ready for next event.
	fJustValidated = true;
	if (paintregion.CountRects() == 0)
		return false;
	r->Include(&paintregion);
	return true;
}

// Method to remove painted rects from pending update region
void nsViewBeOS::Validate(BRegion *reg)
{
	paintregion.Exclude(reg);
}

BPoint nsViewBeOS::GetWheel()
{
	BPoint retvalue = wheel;
	// Mozilla got wheel event, so setting flag and cleaning delta storage
	fWheelDispatched = true;
	wheel.x = 0;
	wheel.y = 0;
	return retvalue;
}

void nsViewBeOS::MouseDown(BPoint point)
{
	if (!fRestoreMouseMask)
		mouseMask = SetMouseEventMask(B_POINTER_EVENTS);
	fRestoreMouseMask = true;
	
	//To avoid generating extra mouseevents when there is no change in pos.
	mousePos = point;

	uint32 clicks = 0;
	BMessage *msg = Window()->CurrentMessage();
	msg->FindInt32("buttons", (int32 *) &buttons);
	msg->FindInt32("clicks", (int32 *) &clicks);

	if (0 == buttons)
		return;

	nsWindow	*w = (nsWindow *) GetMozillaWidget();
	if (w == NULL)
		return;
		
	nsToolkit	*t = w->GetToolkit();
	if (t == NULL)
		return;

	PRUint16 eventButton =
	  (buttons & B_PRIMARY_MOUSE_BUTTON) ? nsMouseEvent::eLeftButton :
	    ((buttons & B_SECONDARY_MOUSE_BUTTON) ? nsMouseEvent::eRightButton :
	      nsMouseEvent::eMiddleButton);
	uint32	args[6];
	args[0] = NS_MOUSE_BUTTON_DOWN;
	args[1] = (uint32) point.x;
	args[2] = (uint32) point.y;
	args[3] = clicks;
	args[4] = modifiers();
	args[5] = eventButton;
	MethodInfo *info = nsnull;
	if (nsnull != (info = new MethodInfo(w, w, nsSwitchToUIThread::BTNCLICK, 6, args)))
		t->CallMethodAsync(info);
	NS_RELEASE(t);
}

void nsViewBeOS::MouseMoved(BPoint point, uint32 transit, const BMessage *msg)
{
	//To avoid generating extra mouseevents when there is no change in pos.
	//and not entering exiting view.
	if (mousePos == point && (transit == B_INSIDE_VIEW || transit == B_OUTSIDE_VIEW))
		return;

	mousePos = point;
		
	//We didn't start the mouse down and there is no drag in progress, so ignore.
	if (NULL == msg && !fRestoreMouseMask && buttons)
		return;
		
	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	if (w == NULL)
		return;
	nsToolkit	*t = t = w->GetToolkit();
	if (t == NULL)
		return;
	uint32	args[4];
	args[1] = (int32) point.x;
	args[2] = (int32) point.y;
	args[3] = modifiers();

	switch (transit)
 	{
 	case B_ENTERED_VIEW:
		{
			args[0] = NULL != msg ? NS_DRAGDROP_ENTER : NS_MOUSE_ENTER;
			if (msg == NULL)
				break;
			nsCOMPtr<nsIDragService> dragService = do_GetService(kCDragServiceCID);
			dragService->StartDragSession();
			//As it may have come from the outside we need to update this.
			nsCOMPtr<nsIDragSessionBeOS> dragSessionBeOS = do_QueryInterface(dragService);
			dragSessionBeOS->UpdateDragMessageIfNeeded(new BMessage(*msg));
		}
		break;
	case B_EXITED_VIEW:
		{
			args[0] = NULL != msg ? NS_DRAGDROP_EXIT : NS_MOUSE_EXIT;
			if (msg == NULL)
				break;
			nsCOMPtr<nsIDragService> dragService = do_GetService(kCDragServiceCID);
			dragService->EndDragSession(PR_FALSE);
		}
		break;
	default:
		args[0]= msg == NULL ? NS_MOUSE_MOVE : NS_DRAGDROP_OVER;
        // fire the drag event at the source
        if (msg != NULL) {
			nsCOMPtr<nsIDragService> dragService = do_GetService(kCDragServiceCID);
			dragService->FireDragEventAtSource(NS_DRAGDROP_DRAG);
        }
 	}
 	
	MethodInfo *moveInfo = nsnull;
	if (nsnull != (moveInfo = new MethodInfo(w, w, nsSwitchToUIThread::ONMOUSE, 4, args)))
		t->CallMethodAsync(moveInfo);
	NS_RELEASE(t);
}

void nsViewBeOS::MouseUp(BPoint point)
{
	if (fRestoreMouseMask) 
	{
		SetMouseEventMask(mouseMask);
		fRestoreMouseMask = false;
	}
	
	//To avoid generating extra mouseevents when there is no change in pos.
	mousePos = point;

	PRUint16 eventButton =
	  (buttons & B_PRIMARY_MOUSE_BUTTON) ? nsMouseEvent::eLeftButton :
	    ((buttons & B_SECONDARY_MOUSE_BUTTON) ? nsMouseEvent::eRightButton :
	      nsMouseEvent::eMiddleButton);
	
	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	if (w == NULL)
		return;
	nsToolkit	*t = t = w->GetToolkit();
	if (t == NULL)
		return;


	uint32	args[6];
	args[0] = NS_MOUSE_BUTTON_UP;
	args[1] = (uint32) point.x;
	args[2] = (int32) point.y;
	args[3] = 0;
	args[4] = modifiers();
	args[5] = eventButton;
	MethodInfo *info = nsnull;
	if (nsnull != (info = new MethodInfo(w, w, nsSwitchToUIThread::BTNCLICK, 6, args)))
		t->CallMethodAsync(info);
	NS_RELEASE(t);
}

void nsViewBeOS::MessageReceived(BMessage *msg)
{
	if(msg->WasDropped())
	{
		nsWindow	*w = (nsWindow *)GetMozillaWidget();
		if (w == NULL)
			return;
		nsToolkit	*t = t = w->GetToolkit();
		if (t == NULL)
			return;

		uint32	args[4];
		args[0] = NS_DRAGDROP_DROP;

		//Drop point is in screen-cordinates
		BPoint aPoint = ConvertFromScreen(msg->DropPoint()); 
	
		args[1] = (uint32) aPoint.x;
		args[2] = (uint32) aPoint.y;
		args[3] = modifiers();

		MethodInfo *info = new MethodInfo(w, w, nsSwitchToUIThread::ONDROP, 4, args);
		t->CallMethodAsync(info);
		BView::MessageReceived(msg);
		return;
	}

	switch(msg->what)
	{
	//Native drag'n'drop negotiation
	case B_COPY_TARGET:
	case B_MOVE_TARGET:
	case B_LINK_TARGET:
	case B_TRASH_TARGET:
		{
			nsCOMPtr<nsIDragService> dragService = do_GetService(kCDragServiceCID);
			nsCOMPtr<nsIDragSessionBeOS> dragSessionBeOS = do_QueryInterface(dragService);
			dragSessionBeOS->TransmitData(new BMessage(*msg));
		}
		break;
	case B_UNMAPPED_KEY_DOWN:
		//printf("unmapped_key_down\n");
		KeyDown(NULL, 0);
		break;

	case B_UNMAPPED_KEY_UP:
		//printf("unmapped_key_up\n");
		KeyUp(NULL, 0);
		break;

	case B_MOUSE_WHEEL_CHANGED:
		{
			float wheel_y;
			float wheel_x;

			msg->FindFloat ("be:wheel_delta_y", &wheel_y);
			msg->FindFloat ("be:wheel_delta_x", &wheel_x);
			wheel.x += wheel_x;
			wheel.y += wheel_y;

			if(!fWheelDispatched || (nscoord(wheel_x) == 0 && nscoord(wheel_y) == 0))
				return;
			uint32	args[1];
			args[0] = (uint32)this;
			nsWindow    *w = (nsWindow *)GetMozillaWidget();
			nsToolkit   *t;

			if (w && (t = w->GetToolkit()) != 0)
			{
					
				MethodInfo *info = nsnull;
				if (nsnull != (info = new MethodInfo(w, w, nsSwitchToUIThread::ONWHEEL, 1, args)))
				{
					if (t->CallMethodAsync(info))
						fWheelDispatched = false;
					
				}
				NS_RELEASE(t);
			}
		}
		break;
		
#if defined(BeIME)
	case B_INPUT_METHOD_EVENT:
		DoIME(msg);
		break;
#endif
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
	if (msg)
	{
		msg->FindInt32("key", &keycode);
		msg->FindInt32("raw_char", &rawcode);
	}

	if (w && (t = w->GetToolkit()) != 0)
	{
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
		MethodInfo *info = nsnull;
		if (nsnull != (info = new MethodInfo(w, w, nsSwitchToUIThread::ONKEY, 6, args)))
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
	if (msg)
	{
		msg->FindInt32("key", &keycode);
		msg->FindInt32("raw_char", &rawcode);
	}

	if (w && (t = w->GetToolkit()) != 0)
	{
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
		MethodInfo *info = nsnull;
		if (nsnull != (info = new MethodInfo(w, w, nsSwitchToUIThread::ONKEY, 6, args)))
			t->CallMethodAsync(info);
		NS_RELEASE(t);
	}
}

void nsViewBeOS::MakeFocus(bool focused)
{
	if (!IsFocus() && focused)
		BView::MakeFocus(focused);
	uint32	args[1];
	args[0] = (uint32)this;
	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;
	if (w && (t = w->GetToolkit()) != 0)
	{
		MethodInfo *info = nsnull;
		if (!focused)
		{
			if (nsnull != (info = new MethodInfo(w, w, nsSwitchToUIThread::KILL_FOCUS, 1, args)))
				t->CallMethodAsync(info);
		}
#ifdef DEBUG_FOCUS
		else
		{
			if (nsnull != (info = new MethodInfo(w, w, nsSwitchToUIThread::GOT_FOCUS, 1, args)))
				t->CallMethodAsync(info);
		}
#endif		
		NS_RELEASE(t);
	}
}

#if defined(BeIME)
// Inline Input Method implementation
void nsViewBeOS::DoIME(BMessage *msg)
{
	nsWindow	*w = (nsWindow *)GetMozillaWidget();
	nsToolkit	*t;

	if(w && (t = w->GetToolkit()) != 0) 
	{
		ssize_t size = msg->FlattenedSize();
		int32		argc = (size+3)/4;
		uint32 *args = new uint32[argc];
		if (args) 
		{
			msg->Flatten((char*)args, size);
			MethodInfo *info = new MethodInfo(w, w, nsSwitchToUIThread::ONIME, argc, args);
			if (info) 
			{
				t->CallMethodAsync(info);
				NS_RELEASE(t);
			}
			delete[] args;
		}	
	}
}
#endif
