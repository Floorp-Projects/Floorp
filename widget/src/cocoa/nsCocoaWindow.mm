/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
#include "nsMacResources.h"
#include "nsIRollupListener.h"
#import "nsChildView.h"

#include "nsIEventQueueService.h"

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
//static const char *sScreenManagerContractID = "@mozilla.org/gfx/screenmanager;1";

// from MacHeaders.c
#ifndef topLeft
  #define topLeft(r)  (((Point *) &(r))[0])
#endif
#ifndef botRight
  #define botRight(r) (((Point *) &(r))[1])
#endif

// externs defined in nsWindow.cpp
extern nsIRollupListener * gRollupListener;
extern nsIWidget         * gRollupWidget;

static PRBool OnMacOSX();

#define kWindowPositionSlop 20


#if 0
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


//¥¥¥ this should probably go into the drag session as a static
pascal OSErr
nsCocoaWindow :: DragTrackingHandler ( DragTrackingMessage theMessage, WindowPtr theWindow, 
                    void *handlerRefCon, DragReference theDrag)
{
  // holds our drag service across multiple calls to this callback. The reference to
  // the service is obtained when the mouse enters the window and is released when
  // the mouse leaves the window (or there is a drop). This prevents us from having
  // to re-establish the connection to the service manager 15 times a second when
  // handling the |kDragTrackingInWindow| message.
  static nsIDragService* sDragService = nsnull;

  nsCocoaWindow* geckoWindow = reinterpret_cast<nsCocoaWindow*>(handlerRefCon);
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


//¥¥¥ this should probably go into the drag session as a static
pascal OSErr
nsCocoaWindow :: DragReceiveHandler (WindowPtr theWindow, void *handlerRefCon,
                  DragReference theDragRef)
{
  // get our window back from the refCon
  nsCocoaWindow* geckoWindow = reinterpret_cast<nsCocoaWindow*>(handlerRefCon);
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

#endif


NS_IMPL_ISUPPORTS_INHERITED0(nsCocoaWindow, Inherited)


//-------------------------------------------------------------------------
//
// nsCocoaWindow constructor
//
//-------------------------------------------------------------------------
nsCocoaWindow::nsCocoaWindow()
#if 0
  , mWindowMadeHere(PR_FALSE)
  , mIsDialog(PR_FALSE)
  , mMacEventHandler(nsnull)
  , mAcceptsActivation(PR_TRUE)
  , mIsActive(PR_FALSE)
  , mZoomOnShow(PR_FALSE)
#endif
: 
  mOffsetParent(nsnull)
, mIsDialog(PR_FALSE)
, mIsResizing(PR_FALSE)
, mWindowMadeHere(PR_FALSE)
, mWindow(nil)
{
#if 0
  mMacEventHandler.reset(new nsMacEventHandler(this));
  WIDGET_SET_CLASSNAME("nsCocoaWindow");  

  // create handlers for drag&drop
  mDragTrackingHandlerUPP = NewDragTrackingHandlerUPP(DragTrackingHandler);
  mDragReceiveHandlerUPP = NewDragReceiveHandlerUPP(DragReceiveHandler);
#endif
}


//-------------------------------------------------------------------------
//
// nsCocoaWindow destructor
//
//-------------------------------------------------------------------------
nsCocoaWindow::~nsCocoaWindow()
{
  if ( mWindow && mWindowMadeHere ) {
    [mWindow autorelease];
    [mDelegate autorelease];
  }
  
#if 0
  if (mWindowPtr)
  {
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
#endif
  
}


//-------------------------------------------------------------------------
//
// Utility method for implementing both Create(nsIWidget ...) and
// Create(nsNativeWidget...)
//-------------------------------------------------------------------------

nsresult nsCocoaWindow::StandardCreate(nsIWidget *aParent,
                        const nsRect &aRect,
                        EVENT_CALLBACK aHandleEventFunction,
                        nsIDeviceContext *aContext,
                        nsIAppShell *aAppShell,
                        nsIToolkit *aToolkit,
                        nsWidgetInitData *aInitData,
                        nsNativeWidget aNativeParent)
{
  Inherited::BaseCreate ( aParent, aRect, aHandleEventFunction, aContext, aAppShell,
                            aToolkit, aInitData );
                            
  if ( !aNativeParent ) {
    mOffsetParent = aParent;

    nsWindowType windowType = eWindowType_toplevel;
    if (aInitData) {
      mWindowType = aInitData->mWindowType;
      // if a toplevel window was requested without a titlebar, use a dialog windowproc
      if (aInitData->mWindowType == eWindowType_toplevel &&
        (aInitData->mBorderStyle == eBorderStyle_none ||
         aInitData->mBorderStyle != eBorderStyle_all && !(aInitData->mBorderStyle & eBorderStyle_title)))
        windowType = eWindowType_dialog;
    } 
    else
      mWindowType = (mIsDialog ? eWindowType_dialog : eWindowType_toplevel);
    
    // create the cocoa window
    NSRect rect;
    rect.origin.x = rect.origin.y = 1.0;
    rect.size.width = rect.size.height = 1.0;
    unsigned int features = NSTitledWindowMask | NSClosableWindowMask | NSMiniaturizableWindowMask 
                              | NSResizableWindowMask;
    if ( mWindowType == eWindowType_popup || mWindowType == eWindowType_invisible )
      features = 0;

    // XXXdwh Just don't make popup windows yet.  They mess up the world.
    if (mWindowType == eWindowType_popup)
      return NS_OK;

    mWindow = [[NSWindow alloc] initWithContentRect:rect styleMask:features 
                        backing:NSBackingStoreBuffered defer:NO];
    
    // Popups will receive a "close" message when an app terminates
    // that causes an extra release to occur.  Make sure popups
    // are set not to release when closed.
    if (features == 0)
      [mWindow setReleasedWhenClosed: NO];

    // create a quickdraw view as the toplevel content view of the window
    NSQuickDrawView* content = [[[NSQuickDrawView alloc] init] autorelease];
    [content setFrame:[[mWindow contentView] frame]];
    [mWindow setContentView:content];
    
    // register for mouse-moved events. The default is to ignore them for perf reasons.
    [mWindow setAcceptsMouseMovedEvents:YES];
    
    // setup our notification delegate. Note that setDelegate: does NOT retain.
    mDelegate = [[WindowDelegate alloc] initWithGeckoWindow:this];
    [mWindow setDelegate:mDelegate];
    
    mWindowMadeHere = PR_TRUE;    
  }

#if 0
  short bottomPinDelta = 0;     // # of pixels to subtract to pin window bottom
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

    short     wDefProcID = kWindowDocumentProc;
    Boolean   goAwayFlag;
    short     hOffset;
    short     vOffset;

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
  ControlHandle   rootControl = nil;
  if (GetRootControl(mWindowPtr, &rootControl) != noErr)
  {
    OSErr err = CreateRootControl(mWindowPtr, &rootControl);
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

#endif

  return NS_OK;
}


#if 0

pascal OSStatus
nsCocoaWindow :: ScrollEventHandler ( EventHandlerCallRef inHandlerChain, EventRef inEvent, void* userData )
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
    nsCocoaWindow* self = NS_REINTERPRET_CAST(nsCocoaWindow*, userData);
    if ( self )
      self->mMacEventHandler->Scroll ( axis, delta, mouseLoc );
  }
  return noErr;
  
} // ScrollEventHandler


pascal OSStatus
nsCocoaWindow :: WindowEventHandler ( EventHandlerCallRef inHandlerChain, EventRef inEvent, void* userData )
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
          nsCocoaWindow* self = NS_REINTERPRET_CAST(nsCocoaWindow*, userData);
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
        nsCocoaWindow* self = NS_REINTERPRET_CAST(nsCocoaWindow*, userData);
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
// Create a nsCocoaWindow using a native window provided by the application
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsCocoaWindow::Create(nsNativeWidget aNativeParent,
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


NS_IMETHODIMP nsCocoaWindow::Create(nsIWidget* aParent,
                      const nsRect &aRect,
                      EVENT_CALLBACK aHandleEventFunction,
                      nsIDeviceContext *aContext,
                      nsIAppShell *aAppShell,
                      nsIToolkit *aToolkit,
                      nsWidgetInitData *aInitData)
{
  return(StandardCreate(aParent, aRect, aHandleEventFunction,
                          aContext, aAppShell, aToolkit, aInitData,
                            nsnull));
}


void*
nsCocoaWindow::GetNativeData(PRUint32 aDataType)
{
  void* retVal = nsnull;
  
  switch ( aDataType ) {
    
    // to emulate how windows works, we always have to return a NSView
    // for NS_NATIVE_WIDGET
    case NS_NATIVE_WIDGET:
    case NS_NATIVE_DISPLAY:
      retVal = [mWindow contentView];
      break;
      
    case NS_NATIVE_WINDOW:  
      retVal = mWindow;
      break;
      
    case NS_NATIVE_GRAPHIC:          // quickdraw port of top view (for now)
      retVal = [[mWindow contentView] qdPort];
      break;
      
#if 0
    case NS_NATIVE_REGION:
    retVal = (void*)mVisRegion;
      break;

    case NS_NATIVE_COLORMAP:
      //¥TODO
      break;

    case NS_NATIVE_OFFSETX:
      point.MoveTo(mBounds.x, mBounds.y);
      LocalToWindowCoordinate(point);
      retVal = (void*)point.x;
      break;

    case NS_NATIVE_OFFSETY:
      point.MoveTo(mBounds.x, mBounds.y);
      LocalToWindowCoordinate(point);
      retVal = (void*)point.y;
      break;
    
    case NS_NATIVE_PLUGIN_PORT:
      // this needs to be a combination of the port and the offsets.
      if (mPluginPort == nsnull)
        mPluginPort = new nsPluginPort;
      
      point.MoveTo(mBounds.x, mBounds.y);
      LocalToWindowCoordinate(point);
      
      // for compatibility with 4.X, this origin is what you'd pass
      // to SetOrigin.
      mPluginPort->port = ::GetWindowPort(mWindowPtr);
      mPluginPort->portx = -point.x;
      mPluginPort->porty = -point.y;
    
      retVal = (void*)mPluginPort;
      break;
#endif
  }

  return retVal;
}

NS_IMETHODIMP
nsCocoaWindow::IsVisible(PRBool & aState)
{
  aState = mVisible;
  return NS_OK;
}
   
//-------------------------------------------------------------------------
//
// Hide or show this window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsCocoaWindow::Show(PRBool bState)
{
  if ( bState )
    [mWindow orderFront:NULL];
  else
    [mWindow orderOut:NULL];

  mVisible = bState;

#if 0
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
  
#endif

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

NS_IMETHODIMP nsCocoaWindow::ConstrainPosition(PRBool aAllowSlop,
                                               PRInt32 *aX, PRInt32 *aY)
{
#if 0
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

#endif
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
NS_IMETHODIMP nsCocoaWindow::Move(PRInt32 aX, PRInt32 aY)
{  
  if ( mWindow ) {  
    // if we're a popup, we have to convert from our parent widget's coord
    // system to the global coord system first because the (x,y) we're given
    // is in its coordinate system.
    if ( mWindowType == eWindowType_popup ) {
      nsRect localRect, globalRect; 
      localRect.x = aX;
      localRect.y = aY;  
      if ( mOffsetParent ) {
        mOffsetParent->WidgetToScreen(localRect,globalRect);
        aX=globalRect.x;
        aY=globalRect.y;
     }
    }
    
    NSPoint coord = {aX, aY};
    //coord = [mWindow convertBaseToScreen:coord];
//printf("moving to %d %d. screen coords %f %f\n", aX, aY, coord.x, coord.y);

 //FIXME -- ensure it's on the screen. Cocoa automatically corrects for windows
 //   with title bars, but for other windows, we have to do this...
 
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
  
#if 0
  StPortSetter setOurPortForLocalToGlobal ( mWindowPtr );
  
  if (eWindowType_popup == mWindowType) {
    PRInt32 xOffset=0,yOffset=0;
    nsRect  localRect,globalRect;

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
#endif
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Position the window behind the given window
//
//-------------------------------------------------------------------------
NS_METHOD nsCocoaWindow::PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                     nsIWidget *aWidget, PRBool aActivate)
{
#if 0
  if (aWidget) {
    WindowPtr behind = (WindowPtr)aWidget->GetNativeData(NS_NATIVE_DISPLAY);
    ::SendBehind(mWindowPtr, behind);
    ::HiliteWindow(mWindowPtr, FALSE);
  } else {
    if (::FrontWindow() != mWindowPtr)
      ::SelectWindow(mWindowPtr);
  }
#endif
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// zoom/restore
//
//-------------------------------------------------------------------------
NS_METHOD nsCocoaWindow::SetSizeMode(PRInt32 aMode)
{
#if 0
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
#endif

  return NS_OK;
}

void nsCocoaWindow::CalculateAndSetZoomedSize()
{
#if 0
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
#endif
  
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
void nsCocoaWindow::MoveToGlobalPoint(PRInt32 aX, PRInt32 aY)
{
#if 0
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
#endif
}


NS_IMETHODIMP nsCocoaWindow::Resize(PRInt32 aX, PRInt32 aY, PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  Move(aX, aY);
  Resize(aWidth, aHeight, aRepaint);
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Resize this window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsCocoaWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint)
{
  if ( mWindow ) {
    NSRect newBounds = [mWindow frame];
    newBounds.size.width = aWidth;
    if ( mWindowType == eWindowType_popup )
      newBounds.size.height = aHeight;
    else
      newBounds.size.height = aHeight + kTitleBarHeight;     // add height of title bar
    StartResizing();
    [mWindow setFrame:newBounds display:NO];
    StopResizing();
  }

  mBounds.width  = aWidth;
  mBounds.height = aHeight;
  
  // tell gecko to update all the child widgets
  ReportSizeEvent();
  
#if 0
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

    short w = macRect.right - macRect.left;
    short h = macRect.bottom - macRect.top;
    Boolean needReposition = (w == 1 && h == 1);

    if ((w != aWidth) || (h != aHeight))
    {
      // make sure that we don't infinitely recurse if live-resize is on
      mResizeIsFromUs = PR_TRUE;
      ::SizeWindow(mWindowPtr, aWidth, aHeight, aRepaint);
      mResizeIsFromUs = PR_FALSE;

#if defined(XP_MACOSX)
      // workaround for bug in MacOSX if windows start life as 1x1.
      if (needReposition)
        RepositionWindow(mWindowPtr, NULL, kWindowCascadeOnMainScreen);
#endif
    }
  }
#endif
  //Inherited::Resize(aWidth, aHeight, aRepaint);
  return NS_OK;
}

NS_IMETHODIMP nsCocoaWindow::GetScreenBounds(nsRect &aRect) {
#if 0 
  nsRect localBounds;
  PRInt32 yAdjust = 0;

  GetBounds(localBounds);
  // nsCocoaWindow local bounds are always supposed to be local (0,0) but in the middle of a move
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

#endif
  return NS_OK;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
PRBool nsCocoaWindow::OnPaint(nsPaintEvent &event)
{
  return PR_TRUE; // don't dispatch the update event
}

//-------------------------------------------------------------------------
//
// Set this window's title
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsCocoaWindow::SetTitle(const nsAString& aTitle)
{
  const nsString& strTitle = PromiseFlatString(aTitle);
  NSString* title = [NSString stringWithCharacters:strTitle.get() length:strTitle.Length()];
  [mWindow setTitle:title];

  return NS_OK;
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
PRBool nsCocoaWindow::DragEvent ( unsigned int aMessage, Point aMouseGlobal, UInt16 aKeyModifiers )
{
#if 0
  PRBool retVal;
  if (mMacEventHandler.get())
    retVal = mMacEventHandler->DragEvent(aMessage, aMouseGlobal, aKeyModifiers);
  else
    retVal = PR_FALSE;
  return retVal;
#endif
  return PR_FALSE;
}

//-------------------------------------------------------------------------
//
// Like ::BringToFront, but constrains the window to its z-level
//
//-------------------------------------------------------------------------
void nsCocoaWindow::ComeToFront() {
#if 0
  nsZLevelEvent  event(NS_SETZLEVEL, this);

  event.point.x = mBounds.x;
  event.point.y = mBounds.y;
  event.time = PR_IntervalNow();

  event.mImmediate = PR_TRUE;

  DispatchWindowEvent(event);
#endif
}


NS_IMETHODIMP nsCocoaWindow::ResetInputState()
{
//  return mMacEventHandler->ResetInputState();
  return NS_OK;
}

void nsCocoaWindow::SetIsActive(PRBool aActive)
{
//  mIsActive = aActive;
}

void nsCocoaWindow::IsActive(PRBool* aActive)
{
//  *aActive = mIsActive;
}


//
// Return true if we are on Mac OS X, caching the result after the first call
//
static PRBool
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


#if 0

//
// DispatchEvent
//
// 
NS_IMETHODIMP
nsCocoaWindow::DispatchEvent ( void* anEvent, void* aView, PRBool *_retval )
{
  *_retval = PR_FALSE;

#if 0  
  NSEvent* event = NS_REINTERPRET_CAST(NSEvent*, anEvent);
  NS_ASSERTION(event, "null event");
  
  ChildView* view = NS_REINTERPRET_CAST(ChildView*, aView);
  
  nsMouseEvent geckoEvent(0, view ? [view widget] : this)
  
  geckoEvent.nativeMsg = anEvent;
  geckoEvent.time = PR_IntervalNow();
  NSPoint mouseLoc = [event locationInWindow];
  geckoEvent.refPoint.x = NS_STATIC_CAST(nscoord, mouseLoc.x);
  geckoEvent.refPoint.y = NS_STATIC_CAST(nscoord, mouseLoc.y);
//printf("-- global mouse click at (%ld,%ld)\n", geckoEvent.refPoint.x, geckoEvent.refPoint.y );
  if ( view ) {
    // convert point to view coordinate system
    NSPoint localPoint = [view convertPoint:mouseLoc fromView:nil];
    geckoEvent.point.x = NS_STATIC_CAST(nscoord, localPoint.x);
    geckoEvent.point.y = NS_STATIC_CAST(nscoord, localPoint.y);
//printf("-- local mouse click at (%ld,%ld) widget = %ld\n", geckoEvent.point.x, geckoEvent.point.y,
//          geckoEvent.widget );
  }
  else
    geckoEvent.point = geckoEvent.refPoint;

  NSEventType type = [event type];
  switch ( type ) {
    case NSLeftMouseDown:
      //printf("left mouse down\n");
      geckoEvent.message = NS_MOUSE_LEFT_BUTTON_DOWN;
      geckoEvent.clickCount = [event clickCount];
      break;
    case NSLeftMouseUp:
      //printf("left mouse up\n");
      break;
    case NSMouseMoved:
      printf("mouse move\n");
      break;
    case NSKeyDown:
      printf("key down\n");
      break;
    case NSKeyUp:
      printf("key up\n");
      break;
    case NSScrollWheel:
      printf("scroll wheel\n");
      break;
    case NSLeftMouseDragged:
      printf("drag\n");
      break;

    default:
      printf("other\n");
      break;    
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  DispatchEvent ( &geckoEvent, status );
  
  *_retval = PR_TRUE;
#endif

  return NS_OK;
}

#endif


//-------------------------------------------------------------------------
//
// Invokes callback and  ProcessEvent method on Event Listener object
//
//-------------------------------------------------------------------------
NS_IMETHODIMP 
nsCocoaWindow::DispatchEvent(nsGUIEvent* event, nsEventStatus& aStatus)
{
  aStatus = nsEventStatus_eIgnore;

  nsIWidget* aWidget = event->widget;
  NS_IF_ADDREF(aWidget);
  
  if (nsnull != mMenuListener){
    if(NS_MENU_EVENT == event->eventStructType)
      aStatus = mMenuListener->MenuSelected( static_cast<nsMenuEvent&>(*event) );
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
  nsSizeEvent sizeEvent(NS_SIZE, this);
  sizeEvent.time        = PR_IntervalNow();

  // nsSizeEvent
  sizeEvent.windowSize  = &mBounds;
  sizeEvent.mWinWidth   = mBounds.width;
  sizeEvent.mWinHeight  = mBounds.height;
  
  // dispatch event
  nsEventStatus status = nsEventStatus_eIgnore;
  DispatchEvent(&sizeEvent, status);
}


#if 0

PRBool
nsCocoaWindow::IsResizing ( ) const 
{ 
  return mIsResizing;
}

void StartResizing ( ) 
{ 
  mIsResizing = PR_TRUE;
}

void StopResizing ( ) 
{ 
  mIsResizing = PR_FALSE;
}

#endif


#pragma mark -


@implementation WindowDelegate


- (id)initWithGeckoWindow:(nsCocoaWindow*)geckoWind
{
  [super init];
  mGeckoWindow = geckoWind;
  return self;
}

- (void)windowDidResize:(NSNotification *)aNotification
{
  if ( !mGeckoWindow->IsResizing() ) {
    // must remember to give Gecko top-left, not straight cocoa origin
    // and that Gecko already compensates for the title bar, so we have to
    // strip it out here.
    NSRect frameRect = [[aNotification object] frame];
    mGeckoWindow->Resize ( NS_STATIC_CAST(PRInt32,frameRect.size.width),
                            NS_STATIC_CAST(PRInt32,frameRect.size.height - nsCocoaWindow::kTitleBarHeight), PR_TRUE );
  }
}


- (void)windowDidBecomeMain:(NSNotification *)aNotification
{
  //printf("got activation\n");
}


- (void)windowDidResignMain:(NSNotification *)aNotification
{
  //printf("got deactivate\n");
}


- (void)windowDidBecomeKey:(NSNotification *)aNotification
{
  //printf("we're key window\n");
}


- (void)windowDidResignKey:(NSNotification *)aNotification
{
  //printf("we're not the key window\n");
}


- (void)windowDidMove:(NSNotification *)aNotification
{
}

@end
