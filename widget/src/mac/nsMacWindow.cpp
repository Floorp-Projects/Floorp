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
#include "nsRegionMac.h"
#include "nsIRollupListener.h"

#if TARGET_CARBON
#include <CFString.h>
#endif

#include <Gestalt.h>
#include <Quickdraw.h>

#if UNIVERSAL_INTERFACES_VERSION < 0x0340
enum {
  kEventWindowConstrain = 83
};
const UInt32 kWindowLiveResizeAttribute = (1L << 28);
#endif

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

// externs defined in nsWindow.cpp
extern nsIRollupListener * gRollupListener;
extern nsIWidget         * gRollupWidget;

#if !TARGET_CARBON
pascal long BorderlessWDEF ( short inCode, WindowPtr inWindow, short inMessage, long inParam ) ;
long CallSystemWDEF ( short inCode, WindowPtr inWindow, short inMessage, long inParam ) ;
#endif
PRBool OnMacOSX();

#define kWindowPositionSlop 20

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
		PRUint32 action = nsIDragService::DRAGDROP_ACTION_MOVE;
		
		// force copy = option, alias = cmd-option, default is move
		if ( inModifiers & optionKey ) {
			if ( inModifiers & cmdKey )
				action = nsIDragService::DRAGDROP_ACTION_LINK;
			else
				action = nsIDragService::DRAGDROP_ACTION_COPY;
		}

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
#if !TARGET_CARBON
	, mPhantomScrollbar(nsnull)
	, mPhantomScrollbarData(nsnull)
#endif
	, mResizeIsFromUs(PR_FALSE)
{
	mMacEventHandler.reset(new nsMacEventHandler(this));
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

#if !TARGET_CARBON
  	// cleanup the struct we hang off the scrollbar's refcon	
  	if ( mPhantomScrollbar ) {
  	  ::SetControlReference(mPhantomScrollbar, (long)nsnull);
  	  delete mPhantomScrollbarData;
  	}
#endif  	
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

		short			wDefProcID = kWindowDocumentProc;
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
#if TARGET_CARBON
				wDefProcID = kWindowSimpleProc;
#else
				wDefProcID = plainDBox;
#endif
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
          // Prior to Carbon, defProcs were solely about appearance. If told to create a dialog,
          // we could use, for example, |kWindowMovableModalDialogProc| even if the dialog wasn't
          // supposed to be modal. Carbon breaks this assumption, enforcing modality when using these
          // particular proc ids. As a result, when compiling under Carbon we have to use the
          // standard window proc id's and below, after we have a windowPtr, we'll hide the closebox
          // that comes with the standard window proc.
          //
          // I'm making the assumption here that any dialog created w/out a titlebar is modal and am
          // therefore keeping the old modal dialog proc. I'm only special-casing dialogs with a 
          // titlebar since those are the only ones that might end up not being modal.
          
          switch (aInitData->mBorderStyle)
          {
            case eBorderStyle_none:
					    wDefProcID = kWindowModalDialogProc;
              break;
              
            case eBorderStyle_all:
              #if TARGET_CARBON
                wDefProcID = kWindowGrowDocumentProc;
              #else
                wDefProcID = kWindowMovableModalGrowProc;   // should we add a close box (kWindowGrowDocumentProc) ?
              #endif
              break;
              
            case eBorderStyle_default:
					    wDefProcID = kWindowModalDialogProc;
              break;
            
            default:
              // we ignore the close flag here, since mac dialogs should never have a close box.
              switch(aInitData->mBorderStyle & (eBorderStyle_resizeh | eBorderStyle_title))
              {
                // combinations of individual options.
                case eBorderStyle_title:
                  #if TARGET_CARBON
                    wDefProcID = kWindowDocumentProc;
                  #else
                    wDefProcID = kWindowMovableModalDialogProc;
                  #endif
                  break;
                                    
                case eBorderStyle_resizeh:
                case (eBorderStyle_title | eBorderStyle_resizeh):
                  #if TARGET_CARBON
                    wDefProcID = kWindowGrowDocumentProc;
                  #else
                    wDefProcID = kWindowMovableModalGrowProc;   // this is the only kind of resizable dialog.
                  #endif
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

      case eWindowType_invisible:
        // don't do anything
        break;
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

#if TARGET_CARBON
  if ( mWindowType == eWindowType_popup ) {
    // OSX enforces window layering so we have to make sure that popups can
    // appear over modal dialogs (at the top of the layering chain). Create
    // the popup like normal and change its window class to the modal layer.
    //
    // XXX This needs to use ::SetWindowGroup() when we move to headers that
    // XXX support it.
    ::SetWindowClass(mWindowPtr, kModalWindowClass);
  }
  else if ( mWindowType == eWindowType_dialog ) {
    // Dialogs on mac don't have a close box, but we probably used a defproc above that
    // contains one. Thankfully, carbon lets us turn it off after the window has been 
    // created. Do so. We currently leave the collapse widget for all dialogs.
    ::ChangeWindowAttributes(mWindowPtr, 0L, kWindowCloseBoxAttribute );
  }
  
  // Setup the live window resizing
  if ( mWindowType == eWindowType_toplevel || mWindowType == eWindowType_invisible ) {
    WindowAttributes removeAttributes = kWindowNoAttributes;
    if ( mWindowType == eWindowType_invisible )
      removeAttributes |= kWindowInWindowMenuAttribute;     
    ::ChangeWindowAttributes ( mWindowPtr, kWindowLiveResizeAttribute, removeAttributes );
    
    EventTypeSpec windEventList[] = { {kEventClassWindow, kEventWindowBoundsChanged},
                                      {kEventClassWindow, kEventWindowConstrain} };
    EventTypeSpec scrollEventList[] = { {kEventClassMouse, kEventMouseWheelMoved} };
    OSStatus err1 = ::InstallWindowEventHandler ( mWindowPtr, NewEventHandlerUPP(WindowEventHandler), 2, windEventList, this, NULL );
    OSStatus err2 = ::InstallWindowEventHandler ( mWindowPtr, NewEventHandlerUPP(ScrollEventHandler), 1, scrollEventList, this, NULL );
      // note, passing NULL as the final param to IWEH() causes the UPP to be disposed automatically
      // when the event target (the window) goes away. See CarbonEvents.h for info.
    
    NS_ASSERTION(err1 == noErr && err2 == noErr, "Couldn't install Carbon Event handlers");
  }  
#endif
  
#if !TARGET_CARBON
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
  //
  // Even after all this, we still have to make one more tweak for Kensington
  // mice. With their new driver, the scrollbar has to be two pixels wide due
  // to a bug in the appearance manager that doesn't put the correct widgetry
  // on 1px wide scrollbars in every case. Rather than have it sticking out 2px
  // into the window, we straddle the window border so only 1px is actually
  // in the content area. Luckily, this is good enough for Logitech ;)
  //
  // NONE of this is required on OSX, which uses CarbonEvents ;)
  Rect sbRect = { 100, -1, 150, 1 };
  mPhantomScrollbarData = new PhantomScrollbarData;
  mPhantomScrollbar = ::NewControl ( mWindowPtr, &sbRect, nil, true, 50, 0, 100, 
                                            kControlScrollBarLiveProc, (long)mPhantomScrollbarData );
  ::EmbedControl ( rootControl, mPhantomScrollbar );
#endif
    
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


#if TARGET_CARBON

pascal OSStatus
nsMacWindow :: ScrollEventHandler ( EventHandlerCallRef inHandlerChain, EventRef inEvent, void* userData )
{
  EventMouseWheelAxis axis = kEventMouseWheelAxisY;
  SInt32 delta = 0;
  Point mouseLoc;
  OSErr err1 = ::GetEventParameter ( inEvent, kEventParamMouseWheelAxis, typeMouseWheelAxis,
                        NULL, sizeof(EventMouseWheelAxis), NULL, &axis );	
  OSErr err2 = ::GetEventParameter ( inEvent, kEventParamMouseWheelDelta, typeLongInteger,
                        NULL, sizeof(SInt32), NULL, &delta );	
  OSErr err3 = ::GetEventParameter ( inEvent, kEventParamMouseLocation, typeQDPoint,
                        NULL, sizeof(Point), NULL, &mouseLoc );	

  if ( err1 == noErr && err2 == noErr && err3 == noErr ) {
    nsMacWindow* self = NS_REINTERPRET_CAST(nsMacWindow*, userData);
    if ( self )
      self->mMacEventHandler->Scroll ( axis, delta, mouseLoc );
  }
  return noErr;
  
} // ScrollEventHandler


pascal OSStatus
nsMacWindow :: WindowEventHandler ( EventHandlerCallRef inHandlerChain, EventRef inEvent, void* userData )
{
  OSStatus retVal = noErr;
  
  WindowRef myWind = NULL;
  ::GetEventParameter ( inEvent, kEventParamDirectObject, typeWindowRef, NULL, sizeof(myWind), NULL, &myWind );
  if ( myWind ) {
    UInt32 what = ::GetEventKind ( inEvent );
    switch ( what ) {
    
      case kEventWindowBoundsChanged:
      {
        // are we moving or resizing the window? we only care about resize.
        UInt32 attributes = 0;
        ::GetEventParameter ( inEvent, kEventParamAttributes, typeUInt32, NULL, sizeof(attributes), NULL, &attributes );
        if ( attributes & kWindowBoundsChangeSizeChanged ) {
          Rect bounds;
          ::InvalWindowRect(myWind, ::GetWindowPortBounds(myWind, &bounds));
          
          // resize the window and repaint
          nsMacWindow* self = NS_REINTERPRET_CAST(nsMacWindow*, userData);
          if ( self && !self->mResizeIsFromUs ) {
            self->mMacEventHandler->ResizeEvent(myWind);
            self->mMacEventHandler->UpdateEvent();
          }
        }
        break;
      }
      
      case kEventWindowConstrain:
      {
        // Ignore this event if we're an invisible window, otherwise pass along the
        // chain to ensure it's onscreen.
        nsMacWindow* self = NS_REINTERPRET_CAST(nsMacWindow*, userData);
        if ( self ) {
          if ( self->mWindowType != eWindowType_invisible )
            retVal = ::CallNextEventHandler( inHandlerChain, inEvent );
        }
        break;
      }      
      	
      default:
        // do nothing...
        break;
    
    } // case of which event?
  }
  
  return retVal;
  
} // WindowEventHandler

#endif


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
      SetSizeMode(nsSizeMode_Maximized);
      mZoomOnShow = PR_FALSE;
    }
    ComeToFront();
  }
  else {
    // when a toplevel window goes away, make sure we rollup any popups that may 
    // be lurking. We want to catch this here because we're guaranteed that
    // we hide a window before we destroy it, and doing it here more closely
    // approximates where we do the same thing on windows.
  	if ( mWindowType == eWindowType_toplevel ) {
  	  if ( gRollupListener )
        gRollupListener->Rollup();
      NS_IF_RELEASE(gRollupListener);
      NS_IF_RELEASE(gRollupWidget);
  	}
    ::HideWindow(mWindowPtr);
  }
  
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

	// get the window bounds
	Rect portBounds;
	::GetWindowPortBounds(mWindowPtr, &portBounds);
	short pos;
	short windowWidth = portBounds.right - portBounds.left;
	short windowHeight = portBounds.bottom - portBounds.top;

	// now get our playing field. use the current screen, or failing that for any reason,
	// the GrayRgn (which of course is arguably more correct but has drawbacks as well)
	Rect screenRect;
	nsCOMPtr<nsIScreenManager> screenmgr = do_GetService(sScreenManagerContractID);
	if (screenmgr) {
		nsCOMPtr<nsIScreen> screen;
		PRInt32 left, top, width, height, fullHeight;

		// zero size rects can happen during window creation, and confuse
		// the screen manager
		width = windowWidth > 0 ? windowWidth : 1;
		height = windowHeight > 0 ? windowHeight : 1;
		screenmgr->ScreenForRect(*aX, *aY, width, height,
		                        getter_AddRefs(screen));
		if (screen) {
			screen->GetAvailRect(&left, &top, &width, &height);
			screen->GetRect(&left, &top, &width, &fullHeight);
			screenRect.left = left;
			screenRect.right = left+width;
			screenRect.top = top;
			screenRect.bottom = top+height;
		}
	} else
		::GetRegionBounds(::GetGrayRgn(), &screenRect);

	pos = screenRect.left;
	if (windowWidth > kWindowPositionSlop)
		pos -= windowWidth - kWindowPositionSlop;
	if (*aX < pos)
		*aX = pos;
	else if (*aX >= screenRect.right - kWindowPositionSlop)
		*aX = screenRect.right - kWindowPositionSlop;

	pos = screenRect.top;
	if (windowHeight > kWindowPositionSlop)
		pos -= windowHeight - kWindowPositionSlop;
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
			// coordinates (within a pixel or two) as a window's current location, it will 
			// move to (0,0,-1,-1). The fix is to not move the window if we're already
			// there. (radar# 2669004)
#if TARGET_CARBON
      const PRInt32 kMoveThreshold = 2;
#else
      const PRInt32 kMoveThreshold = 0;
#endif
			Rect currBounds;
			::GetWindowBounds ( mWindowPtr, kWindowGlobalPortRgn, &currBounds );
			if ( abs(currBounds.left-aX) > kMoveThreshold || abs(currBounds.top-aY) > kMoveThreshold ) {
			  ::MoveWindow(mWindowPtr, aX, aY, false);
			  
			  Rect newBounds;
			  ::GetWindowBounds ( mWindowPtr, kWindowGlobalPortRgn, &newBounds );
			}  
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
  PRInt32  currentMode;

  if (aMode == nsSizeMode_Minimized) // unlikely on the Mac
    return NS_ERROR_UNEXPECTED;

  // already done? it's bad to rezoom a window, so do nothing
  rv = nsBaseWidget::GetSizeMode(&currentMode);
  if (NS_SUCCEEDED(rv) && currentMode == aMode)
    return NS_OK;

  if (!mVisible) {
    /* zooming on the Mac doesn't seem to work until the window is visible.
       the rest of the app is structured to zoom before the window is visible
       to avoid flashing. here's where we defeat that. */
    if (aMode == nsSizeMode_Maximized)
      mZoomOnShow = PR_TRUE;
  } else {
    Rect macRect;
    rv = nsBaseWidget::SetSizeMode(aMode);
    if (NS_SUCCEEDED(rv)) {
      if (aMode == nsSizeMode_Maximized) {
        CalculateAndSetZoomedSize();
        ::ZoomWindow(mWindowPtr, inZoomOut, ::FrontWindow() == mWindowPtr);
      } else
        ::ZoomWindow(mWindowPtr, inZoomIn, ::FrontWindow() == mWindowPtr);
      ::GetWindowPortBounds(mWindowPtr, &macRect);
      Resize(macRect.right - macRect.left, macRect.bottom - macRect.top, PR_FALSE);
    }
  }

  return rv;
}

void nsMacWindow::CalculateAndSetZoomedSize()
{
  StPortSetter setOurPort(mWindowPtr);

  // calculate current window portbounds
  Rect windRect;
  ::GetWindowPortBounds(mWindowPtr, &windRect);
  ::LocalToGlobal((Point *)&windRect.top);
  ::LocalToGlobal((Point *)&windRect.bottom);

  // calculate window's borders on each side, these differ in Aqua / Platinum
  short wTitleHeight;
  short wLeftBorder;
  short wRightBorder;
  short wBottomBorder;
       
  RgnHandle structRgn = ::NewRgn();
  ::GetWindowRegion(mWindowPtr, kWindowStructureRgn, structRgn);
  Rect structRgnBounds;
  ::GetRegionBounds(structRgn, &structRgnBounds);
  wTitleHeight = windRect.top - structRgnBounds.top;
  wLeftBorder = windRect.left - structRgnBounds.left;
  wRightBorder =  structRgnBounds.right - windRect.right;
  wBottomBorder = structRgnBounds.bottom - windRect.bottom;

  ::DisposeRgn(structRgn);

  windRect.top -= wTitleHeight;
  windRect.bottom += wBottomBorder;
  windRect.right += wRightBorder;
  windRect.left -= wLeftBorder;

  // find which screen the window is (mostly) on and get its rect. GetAvailRect()
  // handles subtracting out the menubar and the dock for us. Set the zoom rect
  // to the screen rect, less some fudging and room for icons on the primary screen.
	nsCOMPtr<nsIScreenManager> screenMgr = do_GetService(sScreenManagerContractID);
  if ( screenMgr ) {
    nsCOMPtr<nsIScreen> screen;
    screenMgr->ScreenForRect ( windRect.left, windRect.top, windRect.right - windRect.left, windRect.bottom - windRect.top,
                                getter_AddRefs(screen) );
    if ( screen ) {
      nsRect newWindowRect;
      screen->GetAvailRect ( &newWindowRect.x, &newWindowRect.y, &newWindowRect.width, &newWindowRect.height );
      
      // leave room for icons on primary screen
      nsCOMPtr<nsIScreen> primaryScreen;
      screenMgr->GetPrimaryScreen ( getter_AddRefs(primaryScreen) );
      if (screen == primaryScreen) {
        int iconSpace = 96;
#if TARGET_CARBON
        if(::OnMacOSX()) {
          iconSpace = 128;  //icons/grid is wider on Mac OS X
        }
#endif
        newWindowRect.width -= iconSpace;
      }

    	Rect zoomRect;
    	::SetRect(&zoomRect,
                  newWindowRect.x + wLeftBorder,
                  newWindowRect.y + wTitleHeight,
                  newWindowRect.x + newWindowRect.width - wRightBorder,
                  newWindowRect.y + newWindowRect.height - wBottomBorder); 
    	::SetWindowStandardState ( mWindowPtr, &zoomRect );
    }
  }
  
} // CalculateAndSetZoomedSize


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

		if (((macRect.right - macRect.left) != aWidth)
			|| ((macRect.bottom - macRect.top) != aHeight))
		{
		  // make sure that we don't infinitely recurse if live-resize is on
      mResizeIsFromUs = PR_TRUE;
			::SizeWindow(mWindowPtr, aWidth, aHeight, aRepaint);
      mResizeIsFromUs = PR_FALSE;
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
#if TARGET_CARBON
  if(::OnMacOSX()) {
    // On MacOS X try to use the unicode friendly CFString version first
    CFStringRef labelRef = ::CFStringCreateWithCharacters(kCFAllocatorDefault, (UniChar*)aTitle.get(), aTitle.Length());
    if(labelRef) {
      ::SetWindowTitleWithCFString(mWindowPtr, labelRef);
      ::CFRelease(labelRef);
      return NS_OK;
    }
  }
#endif
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

//
// Return true if we are on Mac OS X, caching the result after the first call
//
PRBool
OnMacOSX()
{

  static PRBool gInitVer = PR_FALSE;
  static PRBool gOnMacOSX = PR_FALSE;
  if(! gInitVer) {
    long version;
    OSErr err = ::Gestalt(gestaltSystemVersion, &version);
    gOnMacOSX = (err == noErr && version >= 0x00001000);
    gInitVer = PR_TRUE;
  }
  return gOnMacOSX;
}
