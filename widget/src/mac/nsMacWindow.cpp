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

#include "nsMacWindow.h"
#include "nsMacEventHandler.h"
#include "nsMacMessageSink.h"

#include "nsIServiceManager.h"    // for drag and drop
#include "nsWidgetsCID.h"
#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsIDragSessionMac.h"
#include "nsGUIEvent.h"

#include <LowMem.h>


// Define Class IDs -- i hate having to do this
static NS_DEFINE_IID(kCDragServiceCID,  NS_DRAGSERVICE_CID);


// from MacHeaders.c
#ifndef topLeft
	#define topLeft(r)	(((Point *) &(r))[0])
#endif
#ifndef botRight
	#define botRight(r)	(((Point *) &(r))[1])
#endif


// These magic adjustments are so that the contained webshells hangs one pixel
// off the right and bottom sides of the window. This aligns the scroll bar
// correctly, and compensates for different window frame dimentions on
// Windows and Mac.
#define WINDOW_SIZE_TWEAKING

// these should come from the system, not be hard-coded. What if I'm running
// an elaborate theme with wide window borders?
const short kWindowTitleBarHeight = 22;
const short kWindowMarginWidth = 6;
const short kDialogTitleBarHeight = 26;
const short kDialogMarginWidth = 6;


DragTrackingHandlerUPP nsMacWindow::sDragTrackingHandlerUPP = NewDragTrackingHandlerProc(DragTrackingHandler);
DragReceiveHandlerUPP nsMacWindow::sDragReceiveHandlerUPP = NewDragReceiveHandlerProc(DragReceiveHandler);

void SetDragActionBasedOnModifiers ( nsIDragService* inDragService, short inModifiers ) ; 


//
// SetDragActionsBasedOnModifiers [static]
//
// Examines the MacOS modifier keys and sets the appropriate drag action on the
// drag session to copy/move/etc
//
void
SetDragActionBasedOnModifiers ( nsIDragService* inDragService, short inModifiers ) 
{
	nsCOMPtr<nsIDragSession> dragSession;
	inDragService->GetCurrentSession ( getter_AddRefs(dragSession) );
	if ( dragSession ) {
		PRUint32 action = nsIDragService::DRAGDROP_ACTION_NONE;
		
		// force copy = option, alias = cmd-option
		if ( inModifiers & optionKey ) {
			if ( inModifiers & cmdKey )
				action = nsIDragService::DRAGDROP_ACTION_LINK;
			else
				action = nsIDragService::DRAGDROP_ACTION_COPY;
		}

		// I think we only need to set this when it's not "none"
		if ( action != nsIDragService::DRAGDROP_ACTION_NONE )
			dragSession->SetDragAction ( action );		
	}

} // SetDragActionBasedOnModifiers



//еее this should probably go into the drag session as a static
pascal OSErr
nsMacWindow :: DragTrackingHandler ( DragTrackingMessage theMessage, WindowPtr theWindow, 
										void *handlerRefCon, DragReference theDrag)
{
	// holds our drag service across multiple calls to this callback. The reference to
	// the service is obtained when the mouse enters the window and is released when
	// the mouse leaves the window (or there is a drop). This prevents us from having
	// to re-establish the connection to the service manager 15 times a second when
	// handling the |kDragTrackingInWindow| message.
	static nsIDragService* sDragService = nsnull;

	nsMacWindow* geckoWindow = reinterpret_cast<nsMacWindow*>(handlerRefCon);
	if ( !theWindow || !geckoWindow )
		return dragNotAcceptedErr;
		
	nsresult rv = NS_OK;
	switch ( theMessage ) {
	
		case kDragTrackingEnterHandler:
			break;
			
		case kDragTrackingEnterWindow:
		{
			// get our drag service for the duration of the drag.
			nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                       					nsIDragService::GetIID(),
      	                            					(nsISupports **)&sDragService);
      	    NS_ASSERTION ( sDragService, "Couldn't get a drag service, we're in biiig trouble" );

			// tell the session about this drag
			if ( sDragService ) {
				sDragService->StartDragSession();
				nsCOMPtr<nsIDragSessionMac> macSession ( do_QueryInterface(sDragService) );
				if ( macSession )
					macSession->SetDragReference ( theDrag );
			}
			
			// let gecko know that the mouse has entered the window so it
			// can start tracking and sending enter/exit events to frames.
			Point mouseLocGlobal;
			::GetDragMouse ( theDrag, &mouseLocGlobal, nsnull );
			geckoWindow->DragEvent ( NS_DRAGDROP_ENTER, mouseLocGlobal, 0L );			
			break;
		}
		
		case kDragTrackingInWindow:
		{
			Point mouseLocGlobal;
			::GetDragMouse ( theDrag, &mouseLocGlobal, nsnull );
			short modifiers;
			::GetDragModifiers ( theDrag, &modifiers, nsnull, nsnull );
			
			NS_ASSERTION ( sDragService, "If we don't have a drag service, we're fucked" );
			
			// set the drag action on the service so the frames know what is going on
			SetDragActionBasedOnModifiers ( sDragService, modifiers );
			
			// pass into gecko for handling...
			geckoWindow->DragEvent ( NS_DRAGDROP_OVER, mouseLocGlobal, modifiers );
			break;
		}
		
		case kDragTrackingLeaveWindow:
		{
			// tell the drag service that we're done with it.
			if ( sDragService ) {
				sDragService->EndDragSession();
				
				// clear out the dragRef in the drag session. We are guaranteed that
				// this will be called _after_ the drop has been processed (if there
				// is one), so we're not destroying valuable information if the drop
				// was in our window.
				nsCOMPtr<nsIDragSessionMac> macSession ( do_QueryInterface(sDragService) );
				if ( macSession )
					macSession->SetDragReference ( 0 );
					
			}
			
			// let gecko know that the mouse has left the window so it
			// can stop tracking and sending enter/exit events to frames.
			Point mouseLocGlobal;
			::GetDragMouse ( theDrag, &mouseLocGlobal, nsnull );
			geckoWindow->DragEvent ( NS_DRAGDROP_EXIT, mouseLocGlobal, 0L );
			
			::HideDragHilite ( theDrag );
 	
 			// we're _really_ done with it, so let go of the service.
			if ( sDragService ) {
				nsServiceManager::ReleaseService(kCDragServiceCID, sDragService);
				sDragService = nsnull;			
			}
			
			break;
		}
		
	} // case of each drag message

	return noErr;
	
} // DragTrackingHandler


//еее this should probably go into the drag session as a static
pascal OSErr
nsMacWindow :: DragReceiveHandler (WindowPtr theWindow, void *handlerRefCon,
									DragReference theDragRef)
{
	// get our window back from the refCon
	nsMacWindow* geckoWindow = reinterpret_cast<nsMacWindow*>(handlerRefCon);
	if ( !theWindow || !geckoWindow )
		return dragNotAcceptedErr;

	OSErr result = noErr;
	printf("DragReceiveHandler called. We got a drop!!!!, DragRef is %ld\n", theDragRef);

	// pass the drop event along to Gecko
	Point mouseLocGlobal;
	::GetDragMouse ( theDragRef, &mouseLocGlobal, nsnull );
	short modifiers;
	::GetDragModifiers ( theDragRef, &modifiers, nsnull, nsnull );
	geckoWindow->DragEvent ( NS_DRAGDROP_DROP, mouseLocGlobal, modifiers );
	
	// once the event has gone to gecko, check the "canDrop" state in the 
	// drag session to see what we should return to the OS (drag accepted or not).
	nsIDragService* dragService;
	nsresult rv = nsServiceManager::GetService(kCDragServiceCID,
                                       			nsIDragService::GetIID(),
      	                            			(nsISupports **)&dragService);
	if ( NS_SUCCEEDED(rv) && dragService ) {
		nsCOMPtr<nsIDragSession> dragSession;
		dragService->GetCurrentSession ( getter_AddRefs(dragSession) );
		if ( dragSession ) {
			// fail if the target has set that it can't accept the drag
			PRBool canDrop = PR_FALSE;
			if ( NS_SUCCEEDED(dragSession->GetCanDrop(&canDrop)) )
				if ( canDrop == PR_FALSE )
					result = dragNotAcceptedErr;	
		}
		
		// we don't need the drag session anymore, the user has released the
		// mouse and the event has already gone to gecko.
		dragService->EndDragSession();
		
		nsServiceManager::ReleaseService(kCDragServiceCID, dragService);
	}
	
	return result;
	
} // DragReceiveHandler



//-------------------------------------------------------------------------
//
// nsMacWindow constructor
//
//-------------------------------------------------------------------------
nsMacWindow::nsMacWindow() : Inherited()
	, mWindowMadeHere(PR_FALSE)
	, mIsDialog(PR_FALSE)
	, mMacEventHandler(nsnull)
{
	mMacEventHandler.reset(new nsMacEventHandler(this));
	strcpy(gInstanceClassName, "nsMacWindow");
}


//-------------------------------------------------------------------------
//
// nsMacWindow destructor
//
//-------------------------------------------------------------------------
nsMacWindow::~nsMacWindow()
{
	if (mWindowPtr)
	{
		if (mWindowMadeHere)
#if TARGET_CARBON
			::DisposeWindow(mWindowPtr);
#else
			::CloseWindow(mWindowPtr);
#endif
		
		// clean up DragManager stuff
		::RemoveTrackingHandler ( sDragTrackingHandlerUPP, mWindowPtr );
		::RemoveReceiveHandler ( sDragReceiveHandlerUPP, mWindowPtr );
		
		nsMacMessageSink::RemoveRaptorWindowFromList(mWindowPtr);
		mWindowPtr = nsnull;
	}
}


//-------------------------------------------------------------------------
//
// Utility method for implementing both Create(nsIWidget ...) and
// Create(nsNativeWidget...)
//-------------------------------------------------------------------------

nsresult nsMacWindow::StandardCreate(nsIWidget *aParent,
	                      const nsRect &aRect,
	                      EVENT_CALLBACK aHandleEventFunction,
	                      nsIDeviceContext *aContext,
	                      nsIAppShell *aAppShell,
	                      nsIToolkit *aToolkit,
	                      nsWidgetInitData *aInitData,
	                      nsNativeWidget aNativeParent)
{
	short	bottomPinDelta = 0;			// # of pixels to subtract to pin window bottom
	
	// build the main native window
	if (aNativeParent == nsnull)
	{
		nsWindowType windowType;
		if (aInitData)
		{
			windowType = aInitData->mWindowType;
			// if a toplevel window was requested without a titlebar, use a dialog windowproc
			if (aInitData->mWindowType == eWindowType_toplevel &&
				(aInitData->mBorderStyle == eBorderStyle_none ||
				 aInitData->mBorderStyle != eBorderStyle_all && !(aInitData->mBorderStyle & eBorderStyle_title)))
				windowType = eWindowType_dialog;
		} else
			windowType = (mIsDialog ? eWindowType_dialog : eWindowType_toplevel);

		short			wDefProcID;
		Boolean		goAwayFlag;
		short			hOffset;
		short			vOffset;

		switch (windowType)
		{
			case eWindowType_popup:
			    // (pinkerton)
			    // Added very very early support for |eBorderStyle_BorderlessTopLevel| but
			    // it isn't correct because it takes the focus away from the main window
			    // The main window must remain active.
			    //
			    // ...fall through...
			    
			case eWindowType_child:
				wDefProcID = plainDBox;
				goAwayFlag = false;
				hOffset = 0;
				vOffset = 0;
				break;

			case eWindowType_dialog:
				if (aInitData &&
					aInitData->mBorderStyle != eBorderStyle_all &&
					(aInitData->mBorderStyle == eBorderStyle_none ||
					 !(aInitData->mBorderStyle & eBorderStyle_title)))
				{
					wDefProcID = kWindowModalDialogProc;
					goAwayFlag = false;
				} else {
					wDefProcID = kWindowMovableModalDialogProc;
					goAwayFlag = true; // revisit this below
				}
				hOffset = kDialogMarginWidth;
				vOffset = kDialogTitleBarHeight;
				break;

			case eWindowType_toplevel:
				if (aInitData &&
					aInitData->mBorderStyle != eBorderStyle_all &&
					(aInitData->mBorderStyle == eBorderStyle_none ||
					 !(aInitData->mBorderStyle & eBorderStyle_resizeh)))
					wDefProcID = kWindowDocumentProc;
				else
					wDefProcID = kWindowFullZoomGrowDocumentProc;
				goAwayFlag = true;
				hOffset = kWindowMarginWidth;
				vOffset = kWindowTitleBarHeight;
				break;
        /*
			case eBorderStyle_3DChildWindow:
				wDefProcID = altDBoxProc;
				goAwayFlag = false;
				hOffset = 0;
				vOffset = 0;
				break;
        */
		}

		// now turn off some default features if requested by aInitData
		if (aInitData && aInitData->mBorderStyle != eBorderStyle_all)
		{
			if (aInitData->mBorderStyle == eBorderStyle_none ||
					!(aInitData->mBorderStyle & eBorderStyle_close))
				goAwayFlag = false;
		}

		Rect wRect;
		nsRectToMacRect(aRect, wRect);

#ifdef WINDOW_SIZE_TWEAKING
		// see also the Resize method
		wRect.right --;
		wRect.bottom --;
#endif
#if TARGET_CARBON
		::OffsetRect(&wRect, hOffset, vOffset + ::GetMBarHeight());
#else
		::OffsetRect(&wRect, hOffset, vOffset + ::LMGetMBarHeight());
#endif	
		
		// HACK!!!!! This really should be part of the window manager
		// Make sure window bottom of window doesn't exceed max monitor size
#if TARGET_CARBON
		Rect tempRect;
		GetRegionBounds(GetGrayRgn(), &tempRect);
#else
		RgnHandle	theGrayRegion = GetGrayRgn();
		Rect		tempRect;
		SetRect(&tempRect,
				(**theGrayRegion).rgnBBox.left,
				(**theGrayRegion).rgnBBox.top,
				(**theGrayRegion).rgnBBox.right,
				(**theGrayRegion).rgnBBox.bottom);
#endif

		if (wRect.bottom > tempRect.bottom)
		{
			bottomPinDelta = wRect.bottom - tempRect.bottom;
			wRect.bottom -= bottomPinDelta;
		}
		
#if TARGET_CARBON
		mWindowPtr = ::NewCWindow(nil, &wRect, "\p", false, wDefProcID, (WindowRef)-1, goAwayFlag, (long)nsnull);
#else
		mWindowPtr = ::NewCWindow(nil, &wRect, "\p", false, wDefProcID, (GrafPort*)-1, goAwayFlag, (long)nsnull);
#endif
		mWindowMadeHere = PR_TRUE;
	}
	else
	{
		mWindowPtr = (WindowPtr)aNativeParent;
		mWindowMadeHere = PR_FALSE;
	}

	if (mWindowPtr == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;
		
	nsMacMessageSink::AddRaptorWindowToList(mWindowPtr, this);

	// create the root control
	ControlHandle		rootControl = nil;
	if (GetRootControl(mWindowPtr, &rootControl) != noErr)
	{
		OSErr	err = CreateRootControl(mWindowPtr, &rootControl);
		NS_ASSERTION(err == noErr, "Error creating window root control");
	}

	// reset the coordinates to (0,0) because it's the top level widget
	// and adjust for any adjustment required to requested window bottom
	nsRect bounds(0, 0, aRect.width, aRect.height - bottomPinDelta);

	// init base class
	Inherited::StandardCreate(aParent, bounds, aHandleEventFunction, 
														aContext, aAppShell, aToolkit, aInitData);


	// register tracking and receive handlers with the native Drag Manager
	if ( sDragTrackingHandlerUPP ) {
		OSErr result = ::InstallTrackingHandler ( sDragTrackingHandlerUPP, mWindowPtr, this );
		NS_ASSERTION ( result == noErr, "can't install drag tracking handler");
	}
	if ( sDragReceiveHandlerUPP ) {
		OSErr result = ::InstallReceiveHandler ( sDragReceiveHandlerUPP, mWindowPtr, this );
		NS_ASSERTION ( result == noErr, "can't install drag receive handler");
	}
	
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Create a nsMacWindow using a native window provided by the application
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMacWindow::Create(nsNativeWidget aNativeParent,		// this is a windowPtr
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
	return(StandardCreate(nsnull, aRect, aHandleEventFunction,
													aContext, aAppShell, aToolkit, aInitData,
														aNativeParent));
}

//-------------------------------------------------------------------------
//
// Hide or show this window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMacWindow::Show(PRBool bState)
{
  Inherited::Show(bState);
	
  // we need to make sure we call ::Show/HideWindow() to generate the 
  // necessary activate/deactivate events. Calling ::ShowHide() is
  // not adequate (pinkerton).
  if ( bState )
  {
    ::ShowWindow(mWindowPtr);
    ::SelectWindow(mWindowPtr);
  }
  else
    ::HideWindow(mWindowPtr);

  return NS_OK;
}



NS_METHOD nsWindow::Minimize(void)
{
  return NS_OK;
}

NS_METHOD nsWindow::Maximize(void)
{
  return NS_OK;
}

NS_METHOD nsWindow::Restore(void)
{
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Move this window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMacWindow::Move(PRInt32 aX, PRInt32 aY)
{
	if (mWindowMadeHere)
	{
		// make sure the window stays visible
#if TARGET_CARBON
		Rect screenRect;
		::GetRegionBounds(::GetGrayRgn(), &screenRect);

		Rect portBounds;
		::GetWindowPortBounds(mWindowPtr, &portBounds);
		short windowWidth = portBounds.right - portBounds.left;
#else
		Rect screenRect = (**::GetGrayRgn()).rgnBBox;
		short windowWidth = mWindowPtr->portRect.right - mWindowPtr->portRect.left;
#endif
		if (((PRInt32)aX) < screenRect.left - windowWidth)
			aX = screenRect.left - windowWidth;
		else if (((PRInt32)aX) > screenRect.right)
			aX = screenRect.right;

		if (((PRInt32)aY) < screenRect.top)
			aY = screenRect.top;
		else if (((PRInt32)aY) > screenRect.bottom)
			aY = screenRect.bottom;

		// move the window if it has not been moved yet
		// (ie. if this function isn't called in response to a DragWindow event)
#if TARGET_CARBON
		Point macPoint = topLeft(portBounds);
#else
		Point macPoint;
		macPoint = topLeft(mWindowPtr->portRect);
#endif
		::LocalToGlobal(&macPoint);
		if ((macPoint.h != aX) || (macPoint.v != aY))
		{
			// in that case, the window borders should be visible too
			PRUint32 minX, minY;
			if (mIsDialog)
			{
				minX = kDialogMarginWidth;
#if TARGET_CARBON
				minY = kDialogTitleBarHeight + ::GetMBarHeight();
#else
				minY = kDialogTitleBarHeight + ::LMGetMBarHeight();
#endif
			}
			else
			{
				minX = kWindowMarginWidth;
#if TARGET_CARBON
				minY = kWindowTitleBarHeight + ::GetMBarHeight();
#else
				minY = kWindowTitleBarHeight + ::LMGetMBarHeight();
#endif
			}
			if (aX < minX)
				aX = minX;
			if (aY < minY)
				aY = minY;

			::MoveWindow(mWindowPtr, aX, aY, false);
		}

		// propagate the event in global coordinates
		Inherited::Move(aX, aY);

		// reset the coordinates to (0,0) because it's the top level widget
		mBounds.x = 0;
		mBounds.y = 0;
	}
	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Resize this window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMacWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
	if (mWindowMadeHere)
	{
#if TARGET_CARBON
		Rect macRect;
		::GetWindowPortBounds ( mWindowPtr, &macRect );
#else
		Rect macRect = mWindowPtr->portRect;
#endif
#ifdef WINDOW_SIZE_TWEAKING
		macRect.right ++;
		macRect.bottom ++;
#endif
		if (((macRect.right - macRect.left) != aWidth)
			|| ((macRect.bottom - macRect.top) != aHeight))
		{
#ifdef WINDOW_SIZE_TWEAKING
			::SizeWindow(mWindowPtr, aWidth - 1, aHeight - 1, aRepaint);
#else
			::SizeWindow(mWindowPtr, aWidth, aHeight, aRepaint);
#endif
		}
	}
	Inherited::Resize(aWidth, aHeight, aRepaint);
	return NS_OK;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
PRBool nsMacWindow::OnPaint(nsPaintEvent &event)
{
										// nothing to draw here
  return PR_FALSE;	// don't dispatch the update event
}

//-------------------------------------------------------------------------
//
// Set this window's title
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMacWindow::SetTitle(const nsString& aTitle)
{
	Str255 title;
	title[0] = aTitle.Length();
	aTitle.ToCString((char*)title + 1, sizeof(title) - 1);
	::SetWTitle(mWindowPtr, title);
	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Handle OS events
//
//-------------------------------------------------------------------------
PRBool nsMacWindow::HandleOSEvent ( EventRecord& aOSEvent )
{
	PRBool retVal;
	if (mMacEventHandler.get())
		retVal = mMacEventHandler->HandleOSEvent(aOSEvent);
	else
		retVal = PR_FALSE;
	return retVal;
}


//-------------------------------------------------------------------------
//
// Handle Menu commands
//
//-------------------------------------------------------------------------
PRBool nsMacWindow::HandleMenuCommand ( EventRecord& aOSEvent, long aMenuResult )
{
	PRBool retVal;
	if (mMacEventHandler.get())
		retVal = mMacEventHandler->HandleMenuCommand(aOSEvent, aMenuResult);
	else
		retVal = PR_FALSE;
	return retVal;
}

//-------------------------------------------------------------------------
// Pass notification of some drag event to Gecko
//
// The drag manager has let us know that something related to a drag has
// occurred in this window. It could be any number of things, ranging from 
// a drop, to a drag enter/leave, or a drag over event. The actual event
// is passed in |aMessage| and is passed along to our event hanlder so Gecko
// knows about it.
//-------------------------------------------------------------------------
PRBool nsMacWindow::DragEvent ( unsigned int aMessage, Point aMouseGlobal, UInt16 aKeyModifiers )
{
	PRBool retVal;
	if (mMacEventHandler.get())
		retVal = mMacEventHandler->DragEvent(aMessage, aMouseGlobal, aKeyModifiers);
	else
		retVal = PR_FALSE;
	return retVal;
}
