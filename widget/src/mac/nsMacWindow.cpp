/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsMacWindow.h"
#include "nsMacEventHandler.h"
#include "nsMacMessageSink.h"
#include "nsMacControl.h"

#include "nsIServiceManager.h"    // for drag and drop
#include "nsWidgetsCID.h"
#include "nsIDragService.h"
#include "nsIDragSession.h"
#include "nsIDragSessionMac.h"
#include "nsIScreen.h"
#include "nsIScreenManager.h"
#include "nsGUIEvent.h"
#include "nsCarbonHelpers.h"
#include "nsGFXUtils.h"
#include "DefProcFakery.h"
#include "nsMacResources.h"

#include <Quickdraw.h>

// Define Class IDs -- i hate having to do this
static NS_DEFINE_CID(kCDragServiceCID,  NS_DRAGSERVICE_CID);
static const char *sScreenManagerContractID = "@mozilla.org/gfx/screenmanager;1";

// from MacHeaders.c
#ifndef topLeft
	#define topLeft(r)	(((Point *) &(r))[0])
#endif
#ifndef botRight
	#define botRight(r)	(((Point *) &(r))[1])
#endif


#if !TARGET_CARBON
pascal long BorderlessWDEF ( short inCode, WindowPtr inWindow, short inMessage, long inParam ) ;
long CallSystemWDEF ( short inCode, WindowPtr inWindow, short inMessage, long inParam ) ;
#endif

// These magic adjustments are so that the contained webshells hangs one pixel
// off the right and bottom sides of the window. This aligns the scroll bar
// correctly, and compensates for different window frame dimentions on
// Windows and Mac.
#define WINDOW_SIZE_TWEAKING

#define kWindowPositionSlop 10

// еее TODO: these should come from the system, not be hard-coded. What if I'm running
// an elaborate theme with wide window borders?
const short kWindowTitleBarHeight = 22;
const short kWindowMarginWidth = 6;
const short kDialogTitleBarHeight = 26;
const short kDialogMarginWidth = 6;


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

#pragma mark -


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
                                       					NS_GET_IID(nsIDragService),
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
			
			// clear out the |canDrop| property of the drag session. If it's meant to
			// be, it will be set again.
			nsCOMPtr<nsIDragSession> session;
			sDragService->GetCurrentSession(getter_AddRefs(session));
			NS_ASSERTION ( session, "If we don't have a drag session, we're fucked" );
			if ( session )
			  session->SetCanDrop(PR_FALSE);

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
    
    // We make the assuption that the dragOver handlers have correctly set
    // the |canDrop| property of the Drag Session. Before we dispatch the event
    // into Gecko, check that value and either dispatch it or set the result
    // code to "spring-back" and show the user the drag failed. 
    OSErr result = noErr;
	nsCOMPtr<nsIDragService> dragService ( do_GetService(kCDragServiceCID) );
	if ( dragService ) {
		nsCOMPtr<nsIDragSession> dragSession;
		dragService->GetCurrentSession ( getter_AddRefs(dragSession) );
		if ( dragSession ) {
			// if the target has set that it can accept the drag, pass along
			// to gecko, otherwise set phasers for failure.
			PRBool canDrop = PR_FALSE;
			if ( NS_SUCCEEDED(dragSession->GetCanDrop(&canDrop)) )
				if ( canDrop ) {
                	// pass the drop event along to Gecko
                	Point mouseLocGlobal;
                	::GetDragMouse ( theDragRef, &mouseLocGlobal, nsnull );
                	short modifiers;
                	::GetDragModifiers ( theDragRef, &modifiers, nsnull, nsnull );
                	geckoWindow->DragEvent ( NS_DRAGDROP_DROP, mouseLocGlobal, modifiers );
                }
                else
					result = dragNotAcceptedErr;	
		} // if a valid drag session
        
		// we don't need the drag session anymore, the user has released the
		// mouse and the event has already gone to gecko.
		dragService->EndDragSession();
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
	, mAcceptsActivation(PR_TRUE)
	, mIsActive(PR_FALSE)
	, mZoomOnShow(PR_FALSE)
	, mPhantomScrollbar(nsnull)
	, mPhantomScrollbarData(nsnull)
{
  //mMacEventHandler.reset(new nsMacEventHandler(this));
	mMacEventHandler = (auto_ptr<nsMacEventHandler>) new nsMacEventHandler(this);
	WIDGET_SET_CLASSNAME("nsMacWindow");	

  // create handlers for drag&drop
  mDragTrackingHandlerUPP = NewDragTrackingHandlerUPP(DragTrackingHandler);
  mDragReceiveHandlerUPP = NewDragReceiveHandlerUPP(DragReceiveHandler);
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
	  // cleanup our special defproc if we are a popup
    if ( mWindowType == eWindowType_popup )
      RemoveBorderlessDefProc ( mWindowPtr );

  	// cleanup the struct we hang off the scrollbar's refcon	
  	if ( mPhantomScrollbar ) {
  	  ::SetControlReference(mPhantomScrollbar, (long)nsnull);
  	  delete mPhantomScrollbarData;
  	}
  	
		if (mWindowMadeHere)
			::DisposeWindow(mWindowPtr);
      
		// clean up DragManager stuff
		if ( mDragTrackingHandlerUPP ) {
		  ::RemoveTrackingHandler ( mDragTrackingHandlerUPP, mWindowPtr );
		  ::DisposeDragTrackingHandlerUPP ( mDragTrackingHandlerUPP );
		 }
		if ( mDragReceiveHandlerUPP ) {
		  ::RemoveReceiveHandler ( mDragReceiveHandlerUPP, mWindowPtr );
		  ::DisposeDragReceiveHandlerUPP ( mDragReceiveHandlerUPP );
		}

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
	nsCOMPtr<nsIToolkit> theToolkit = aToolkit;
	
	// build the main native window
	if (aNativeParent == nsnull)
	{
		nsWindowType windowType;
		if (aInitData)
		{
			mWindowType = aInitData->mWindowType;
			// if a toplevel window was requested without a titlebar, use a dialog windowproc
			if (aInitData->mWindowType == eWindowType_toplevel &&
				(aInitData->mBorderStyle == eBorderStyle_none ||
				 aInitData->mBorderStyle != eBorderStyle_all && !(aInitData->mBorderStyle & eBorderStyle_title)))
				windowType = eWindowType_dialog;
		} else
			mWindowType = (mIsDialog ? eWindowType_dialog : eWindowType_toplevel);

		short			wDefProcID;
		Boolean		goAwayFlag;
		short			hOffset;
		short			vOffset;

		switch (mWindowType)
		{
			case eWindowType_popup:
		    // We're a popup, context menu, etc. Sets
		    // mAcceptsActivation to false so we don't activate the window
		    // when we show it.
		    mOffsetParent = aParent;
		    if( !aParent )
		    	theToolkit = getter_AddRefs(aParent->GetToolkit());

        mAcceptsActivation = PR_FALSE;
				goAwayFlag = false;
				hOffset = 0;
				vOffset = 0;
				wDefProcID = plainDBox;
        break;

			case eWindowType_child:
				wDefProcID = plainDBox;
				goAwayFlag = false;
				hOffset = 0;
				vOffset = 0;
				break;

			case eWindowType_dialog:
        if (aInitData)
        {
          switch (aInitData->mBorderStyle)
          {
            case eBorderStyle_none:
					    wDefProcID = kWindowModalDialogProc;
              break;
              
            case eBorderStyle_all:
              wDefProcID = kWindowMovableModalGrowProc;   // should we add a close box (kWindowGrowDocumentProc) ?
              break;
              
            case eBorderStyle_default:
					    wDefProcID = kWindowModalDialogProc;
              break;
            
            default:
              // we ignore the clase flag here, since mac dialogs should never have a close box.
              switch(aInitData->mBorderStyle & (eBorderStyle_resizeh | eBorderStyle_title))
              {
                // combinations of individual options.
                case eBorderStyle_title:
                  wDefProcID = kWindowMovableModalDialogProc;
                  break;
                                    
                case eBorderStyle_resizeh:
                case (eBorderStyle_title | eBorderStyle_resizeh):
                  wDefProcID = kWindowMovableModalGrowProc;   // this is the only kind of resizable dialog.
                  break;
                                  
                default:
                  NS_WARNING("Unhandled combination of window flags");
                  break;
              }
          }
        }
        else
        {
					wDefProcID = kWindowModalDialogProc;
					goAwayFlag = true; // revisit this below
        }
        
        hOffset = kDialogMarginWidth;
        vOffset = kDialogTitleBarHeight;
				break;

			case eWindowType_toplevel:
				if (aInitData &&
					aInitData->mBorderStyle != eBorderStyle_all &&
					aInitData->mBorderStyle != eBorderStyle_default &&
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
					aInitData->mBorderStyle == eBorderStyle_default &&
					windowType == eWindowType_dialog ||
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

		if (eWindowType_popup != mWindowType)
			::OffsetRect(&wRect, hOffset, vOffset + ::GetMBarHeight());
		else
			::OffsetRect(&wRect, hOffset, vOffset);

		nsCOMPtr<nsIScreenManager> screenmgr = do_GetService(sScreenManagerContractID);
		if (screenmgr) {
			nsCOMPtr<nsIScreen> screen;
			//screenmgr->GetPrimaryScreen(getter_AddRefs(screen));
			screenmgr->ScreenForRect(wRect.left, wRect.top,
		                             wRect.right - wRect.left, wRect.bottom - wRect.top,
		                             getter_AddRefs(screen));
			if (screen) {
		  		PRInt32 left, top, width, height;
				screen->GetAvailRect(&left, &top, &width, &height);
				if (wRect.bottom > top+height) {
					bottomPinDelta = wRect.bottom - (top+height);
					wRect.bottom -= bottomPinDelta;
				}
			}
		}
		mWindowPtr = ::NewCWindow(nil, &wRect, "\p", false, wDefProcID, (WindowRef)-1, goAwayFlag, (long)nsnull);
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
  // (note: aParent is ignored. Mac (real) windows don't want parents)
	Inherited::StandardCreate(nil, bounds, aHandleEventFunction, aContext, aAppShell, theToolkit, aInitData);

  // create a phantom scrollbar to catch the attention of mousewheel 
  // drivers. We'll catch events sent to this scrollbar in the eventhandler
  // and dispatch them into gecko as NS_SCROLL_EVENTs at that point. We need
  // to hang a struct off the refCon in order to provide some data
  // to the action proc.
  //
  // For Logitech, the scrollbar has to be in the content area but can have
  // zero width. For USBOverdrive (used also by MSFT), the scrollbar can be
  // anywhere, but must have a valid width (one pixel wide works). The
  // current location (one pixel wide, and flush along the left side of the
  // window) is a reasonable comprimise in the short term. It is not intended
  // to fix all cases. 
  Rect sbRect = { 100, 0, 200, 1 };
  mPhantomScrollbarData = new PhantomScrollbarData;
  mPhantomScrollbar = ::NewControl ( mWindowPtr, &sbRect, nil, true, 50, 0, 100, 
                                            kControlScrollBarLiveProc, (long)mPhantomScrollbarData );
  ::EmbedControl ( rootControl, mPhantomScrollbar );
    
	// register tracking and receive handlers with the native Drag Manager
	if ( mDragTrackingHandlerUPP ) {
		OSErr result = ::InstallTrackingHandler ( mDragTrackingHandlerUPP, mWindowPtr, this );
		NS_ASSERTION ( result == noErr, "can't install drag tracking handler");
	}
	if ( mDragReceiveHandlerUPP ) {
		OSErr result = ::InstallReceiveHandler ( mDragReceiveHandlerUPP, mWindowPtr, this );
		NS_ASSERTION ( result == noErr, "can't install drag receive handler");
	}

  // If we're a popup, we don't want a border (we want CSS to draw it for us). So
  // install our own window defProc.
  if ( mWindowType == eWindowType_popup )
    InstallBorderlessDefProc(mWindowPtr);

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


//
// InstallBorderlessDefProc
//
// For xul popups, we want borderless windows to match win32's borderless windows. Stash
// our fake WDEF into this window which takes care of not-drawing the borders.
//
void 
nsMacWindow :: InstallBorderlessDefProc ( WindowPtr inWindow )
{
#if !TARGET_CARBON
  // stash the real WDEF so we can call it later
  Handle systemPopupWDEF = ((WindowPeek)inWindow)->windowDefProc;

  // load the stub WDEF and stash it away. If this fails, we'll just use the normal one.
  WindowDefUPP wdef = NewWindowDefUPP( BorderlessWDEF );
  Handle fakedDefProc;
  DefProcFakery::CreateDefProc ( wdef, systemPopupWDEF, &fakedDefProc );
  if ( fakedDefProc )
    ((WindowPeek)inWindow)->windowDefProc = fakedDefProc;
#endif
} // InstallBorderlessDefProc


//
// RemoveBorderlessDefProc
//
// Clean up the mess we've made with our fake defproc. Reset it to
// the system one, just in case someone needs it around after we're
// through with it.
//
void
nsMacWindow :: RemoveBorderlessDefProc ( WindowPtr inWindow )
{
#if !TARGET_CARBON
  Handle fakedProc = ((WindowPeek)inWindow)->windowDefProc;
  Handle oldProc = DefProcFakery::GetSystemDefProc(fakedProc);
  DefProcFakery::DestroyDefProc ( fakedProc );
  ((WindowPeek)inWindow)->windowDefProc = oldProc;
#endif
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
  // not adequate, unless we don't want activation (popups). (pinkerton).
  if ( bState && !mBounds.IsEmpty() ) {
    if ( mAcceptsActivation )
      ::ShowWindow(mWindowPtr);
    else {
      ::ShowHide(mWindowPtr, true);
      ::BringToFront(mWindowPtr); // competes with ComeToFront, but makes popups work
    }
    if (mZoomOnShow) {
      PRInt32 sizemode;
      GetSizeMode(&sizemode); // value was earlier saved but not acted on
      SetSizeMode(sizemode);
      mZoomOnShow = PR_FALSE;
    }
    ComeToFront();
  }
  else
    ::HideWindow(mWindowPtr);

  return NS_OK;
}


/*
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
*/

NS_IMETHODIMP nsMacWindow::ConstrainPosition(PRInt32 *aX, PRInt32 *aY)
{
	if (eWindowType_popup == mWindowType || !mWindowMadeHere)
		return NS_OK;

	// Sanity check against screen size
	// make sure the window stays visible
	Rect screenRect;
	::GetRegionBounds(::GetGrayRgn(), &screenRect);

	Rect portBounds;
	::GetWindowPortBounds(mWindowPtr, &portBounds);
	short pos;
	short windowWidth = portBounds.right - portBounds.left;
	short windowHeight = portBounds.bottom - portBounds.top;

	/* heh. as far as I can tell, the window size is always zero when
	   this function is called. that's annoying, but we can mostly deal.
	   one bit of extra grief it causes is that the screenRect generally
	   starts below the menubar, while the coordinate system does not.
	   here, we assume no window will ever actually remain at 0 size,
	   and give it an artificial size large enough that it won't run
	   aground on the menubar. the size we give it is about titlebar
	   size, anyway, so there's not much danger. */
	if (windowHeight == 0)
		windowHeight = 20;

	pos = screenRect.left;
	if (windowWidth > 0) // sometimes it isn't during window creation
		pos -= windowWidth + kWindowPositionSlop;
	if (*aX < pos)
		*aX = pos;
	else if (*aX >= screenRect.right - kWindowPositionSlop)
		*aX = screenRect.right - kWindowPositionSlop;

	pos = screenRect.top;
	if (windowHeight > 0)
		pos -= windowHeight + kWindowPositionSlop;
	if (*aY < pos)
		*aY = pos;
	else if (*aY >= screenRect.bottom - kWindowPositionSlop)
		*aY = screenRect.bottom - kWindowPositionSlop;

	return NS_OK;
}

//-------------------------------------------------------------------------
//
// Move this window
//
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
// Move
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMacWindow::Move(PRInt32 aX, PRInt32 aY)
{
  StPortSetter setOurPortForLocalToGlobal ( mWindowPtr );
  
	if (eWindowType_popup == mWindowType) {
		PRInt32	xOffset=0,yOffset=0;
		nsRect	localRect,globalRect;

		// convert to screen coordinates
		localRect.x = aX;
		localRect.y = aY;
		localRect.width = 100;
		localRect.height = 100;	

		if ( mOffsetParent ) {
			mOffsetParent->WidgetToScreen(localRect,globalRect);
			aX=globalRect.x;
			aY=globalRect.y;
			
			// there is a bug on OSX where if we call ::MoveWindow() with the same
			// coordinates as it's current location, it will go to (0,0,-1,-1). The
			// fix is to not move the window if we're already there. (radar# 2669004)
			Rect currBounds;
			::GetWindowBounds ( mWindowPtr, kWindowGlobalPortRgn, &currBounds );
			if ( currBounds.left != aX || currBounds.top != aY )
			  ::MoveWindow(mWindowPtr, aX, aY, false);
		}

		return NS_OK;
	} else if (mWindowMadeHere) {
		Rect portBounds;
		::GetWindowPortBounds(mWindowPtr, &portBounds);

		if (mIsDialog) {
			aX += kDialogMarginWidth;
			aY += kDialogTitleBarHeight;
		} else {
			aX += kWindowMarginWidth;
			aY += kWindowTitleBarHeight;
		}

		nsCOMPtr<nsIScreenManager> screenmgr = do_GetService(sScreenManagerContractID);
		if (screenmgr) {
			nsCOMPtr<nsIScreen> screen;
			PRInt32 left, top, width, height, fullTop;
			// adjust for unset bounds, which confuses the screen manager
			width = portBounds.right - portBounds.left;
			height = portBounds.bottom - portBounds.top;
			if (height <= 0) height = 1;
			if (width <= 0) width = 1;

			screenmgr->ScreenForRect(aX, aY, width, height,
			                         getter_AddRefs(screen));
			if (screen) {
				screen->GetAvailRect(&left, &top, &width, &height);
				screen->GetRect(&left, &fullTop, &width, &height);
				aY += top-fullTop;
			}
		}

		// move the window if it has not been moved yet
		// (ie. if this function isn't called in response to a DragWindow event)
		Point macPoint = topLeft(portBounds);
		::LocalToGlobal(&macPoint);
		if (macPoint.h != aX || macPoint.v != aY)
			::MoveWindow(mWindowPtr, aX, aY, false);

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
// Position the window behind the given window
//
//-------------------------------------------------------------------------
NS_METHOD nsMacWindow::PlaceBehind(nsIWidget *aWidget, PRBool aActivate)
{
  if (aWidget) {
    WindowPtr behind = (WindowPtr)aWidget->GetNativeData(NS_NATIVE_DISPLAY);
    ::SendBehind(mWindowPtr, behind);
    ::HiliteWindow(mWindowPtr, FALSE);
  } else {
    if (::FrontWindow() != mWindowPtr)
      ::SelectWindow(mWindowPtr);
  }
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// zoom/restore
//
//-------------------------------------------------------------------------
NS_METHOD nsMacWindow::SetSizeMode(PRInt32 aMode)
{
	nsresult rv;

	if (aMode == nsSizeMode_Minimized) // unlikely on the Mac
		return NS_ERROR_UNEXPECTED;

	rv = nsBaseWidget::SetSizeMode(aMode);
	if (NS_FAILED(rv))
		return rv;

	/* zooming on the Mac doesn't seem to work until the window is visible.
	   the rest of the app is structured to zoom before the window is visible
	   to avoid flashing. here's where we defeat that. */
	if (!mVisible) {
		if (aMode == nsSizeMode_Maximized)
			mZoomOnShow = PR_TRUE;
	} else {
		Rect macRect;
		WindowPtr w = ::FrontWindow();
		::ZoomWindow(mWindowPtr, aMode == nsSizeMode_Normal ? inZoomIn : inZoomOut , ::FrontWindow() == mWindowPtr);
		::GetWindowPortBounds(mWindowPtr, &macRect);
		Resize(macRect.right - macRect.left, macRect.bottom - macRect.top, PR_FALSE);
	}

	return rv;
}

//-------------------------------------------------------------------------
//
// Resize this window to a point given in global (screen) coordinates. This
// differs from simple Move(): that method makes JavaScript place windows
// like other browsers: it puts the top-left corner of the outer edge of the
// window border at the given coordinates, offset from the menubar.
// MoveToGlobalPoint expects the top-left corner of the portrect, which
// is inside the border, and is not offset by the menubar height.
//
//-------------------------------------------------------------------------
void nsMacWindow::MoveToGlobalPoint(PRInt32 aX, PRInt32 aY)
{
	PRInt32 left, top, width, height, fullTop;
	Rect portBounds;

	StPortSetter doThatThingYouDo(mWindowPtr);
	::GetWindowPortBounds(mWindowPtr, &portBounds);

	width = portBounds.right - portBounds.left;
	height = portBounds.bottom - portBounds.top;
	::LocalToGlobal(&topLeft(portBounds));

	nsCOMPtr<nsIScreenManager> screenmgr = do_GetService(sScreenManagerContractID);
	if (screenmgr) {
		nsCOMPtr<nsIScreen> screen;
		//screenmgr->GetPrimaryScreen(getter_AddRefs(screen));
		screenmgr->ScreenForRect(portBounds.left, portBounds.top, width, height,
		                         getter_AddRefs(screen));
		if (screen) {
			screen->GetAvailRect(&left, &top, &width, &height);
			screen->GetRect(&left, &fullTop, &width, &height);
			aY -= top-fullTop;
		}
	}

	if (mIsDialog) {
		aX -= kDialogMarginWidth;
		aY -= kDialogTitleBarHeight;
	} else {
		aX -= kWindowMarginWidth;
		aY -= kWindowTitleBarHeight;
	}
	Move(aX, aY);
}

//-------------------------------------------------------------------------
//
// Resize this window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMacWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
	if (mWindowMadeHere) {
      // Sanity check against screen size
      Rect screenRect;
	  ::GetRegionBounds(::GetGrayRgn(), &screenRect);

      // Need to use non-negative coordinates
      PRInt32 screenWidth;
      if(screenRect.left < 0)
        screenWidth = screenRect.right - screenRect.left;
      else
        screenWidth = screenRect.right;
        
      PRInt32 screenHeight;
      if(screenRect.top < 0)
        screenHeight = screenRect.bottom - screenRect.top;
      else
        screenHeight = screenRect.bottom;
          
      if(aHeight > screenHeight)
        aHeight = screenHeight;
        
      if(aWidth > screenWidth)
        aWidth = screenWidth;      
		
		Rect macRect;
		::GetWindowPortBounds ( mWindowPtr, &macRect );
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

NS_IMETHODIMP nsMacWindow::GetScreenBounds(nsRect &aRect) {
 
	nsRect localBounds;
	PRInt32 yAdjust = 0;

	GetBounds(localBounds);
	// nsMacWindow local bounds are always supposed to be local (0,0) but in the middle of a move
	// can be global. This next adjustment assures they are in local coordinates, even then.
	localBounds.MoveBy(-localBounds.x, -localBounds.y);
	WidgetToScreen(localBounds, aRect);

	nsCOMPtr<nsIScreenManager> screenmgr = do_GetService(sScreenManagerContractID);
	if (screenmgr) {
		nsCOMPtr<nsIScreen> screen;
		//screenmgr->GetPrimaryScreen(getter_AddRefs(screen));
		screenmgr->ScreenForRect(aRect.x, aRect.y, aRect.width, aRect.height,
		                         getter_AddRefs(screen));
		if (screen) {
			PRInt32 left, top, width, height, fullTop;
			screen->GetAvailRect(&left, &top, &width, &height);
			screen->GetRect(&left, &fullTop, &width, &height);
			yAdjust = top-fullTop;
		}
	}
 
	if (mIsDialog)
		aRect.MoveBy(-kDialogMarginWidth, -kDialogTitleBarHeight-yAdjust);
	else
		aRect.MoveBy(-kWindowMarginWidth, -kWindowTitleBarHeight-yAdjust);

	return NS_OK;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
PRBool nsMacWindow::OnPaint(nsPaintEvent &event)
{
	return PR_TRUE;	// don't dispatch the update event
}

//-------------------------------------------------------------------------
//
// Set this window's title
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMacWindow::SetTitle(const nsString& aTitle)
{
	Str255 title;
	// unicode to file system charset
	nsMacControl::StringToStr255(aTitle, title);
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

//-------------------------------------------------------------------------
//
// Like ::BringToFront, but constrains the window to its z-level
//
//-------------------------------------------------------------------------
void nsMacWindow::ComeToFront() {

  nsZLevelEvent  event;

  event.point.x = mBounds.x;
  event.point.y = mBounds.y;
  event.time = PR_IntervalNow();
  event.widget = this;
  event.nativeMsg = nsnull;
  event.eventStructType = NS_ZLEVEL_EVENT;
  event.message = NS_SETZLEVEL;

  event.mPlacement = nsWindowZTop;
  event.mReqBelow = 0;
  event.mImmediate = PR_TRUE;
  event.mAdjusted = PR_FALSE;

  DispatchWindowEvent(event);
}


NS_IMETHODIMP nsMacWindow::ResetInputState()
{
	return mMacEventHandler->ResetInputState();
}

void nsMacWindow::SetIsActive(PRBool aActive)
{
  mIsActive = aActive;
}

void nsMacWindow::IsActive(PRBool* aActive)
{
  *aActive = mIsActive;
}


#if !TARGET_CARBON

// needed for CallWindowDefProc() to work correctly
#pragma options align=mac68k


//
// BorderlessWDEF
//
// The window defproc for borderless windows. 
//
// NOTE: Assumes the window was created with a variant of |plainDBox| so our
// content/structure adjustments work correctly.
// 
pascal long
BorderlessWDEF ( short inCode, WindowPtr inWindow, short inMessage, long inParam )
{
  switch ( inMessage ) {
    case kWindowMsgDraw:
    case kWindowMsgDrawGrowOutline:
    case kWindowMsgGetFeatures:
      break;
      
    case kWindowMsgCalculateShape:
      // Make the content area bigger so that it draws over the structure region. Use
      // the sytem wdef to compute the struct/content regions and then play.
      long result = CallSystemWDEF(inCode, inWindow, inMessage, inParam);
      ::InsetRgn(((WindowPeek)inWindow)->contRgn, -1, -1);
      
      // for some reason, the topleft corner doesn't draw correctly unless i do this.
	    Rect& structRect = (**((WindowPeek)inWindow)->strucRgn).rgnBBox;
	    structRect.top++; structRect.left++;
      break;
      
    default:
      return CallSystemWDEF(inCode, inWindow, inMessage, inParam);
      break;
  }

  return 0;
}


//
// CallSystemWDEF
//
// We really don't want to reinvent the wheel, so call back into the system wdef we have
// stashed away.
//
long
CallSystemWDEF ( short inCode, WindowPtr inWindow, short inMessage, long inParam )
{
  // extract the real system wdef out of the fake one we've stored in the window
  Handle fakedWDEF = ((WindowPeek)inWindow)->windowDefProc;
  Handle systemDefProc = DefProcFakery::GetSystemDefProc ( fakedWDEF );
  
  SInt8 state = ::HGetState(systemDefProc);
  ::HLock(systemDefProc);

  long retval = CallWindowDefProc( (RoutineDescriptorPtr)*systemDefProc, inCode, inWindow, inMessage, inParam);

  ::HSetState(systemDefProc, state);
  
  return retval;
}

#pragma options align=reset

#endif
