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

#include "nsMacWindow.h"
#include "nsMacEventHandler.h"
#include "nsMacControl.h"
#include "nsToolkit.h"

#include "nsIServiceManager.h"    // for drag and drop
#include "nsWidgetsCID.h"
#include "nsIDragHelperService.h"
#include "nsIScreen.h"
#include "nsIScreenManager.h"
#include "nsGUIEvent.h"
#include "nsCarbonHelpers.h"
#include "nsGfxUtils.h"
#include "DefProcFakery.h"
#include "nsMacResources.h"
#include "nsIRollupListener.h"
#include "nsCRT.h"
#include "nsWidgetSupport.h"

#if TARGET_CARBON
#include <CFString.h>
#endif

#include <Gestalt.h>
#include <Quickdraw.h>
#include <MacWindows.h>

#if UNIVERSAL_INTERFACES_VERSION < 0x0340
enum {
  kEventWindowConstrain = 83
};
const UInt32 kWindowLiveResizeAttribute = (1L << 28);
#endif

static const char sScreenManagerContractID[] = "@mozilla.org/gfx/screenmanager;1";

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

#if !TARGET_CARBON
pascal long BorderlessWDEF ( short inCode, WindowPtr inWindow, short inMessage, long inParam ) ;
long CallSystemWDEF ( short inCode, WindowPtr inWindow, short inMessage, long inParam ) ;
#endif

#define kWindowPositionSlop 20

// These are only initial guesses. Real values are filled in
// after window creation.
const short kWindowTitleBarHeight = 22;
const short kWindowMarginWidth = 6;
const short kDialogTitleBarHeight = 26;
const short kDialogMarginWidth = 6;

#pragma mark -

pascal OSErr
nsMacWindow :: DragTrackingHandler ( DragTrackingMessage theMessage, WindowPtr theWindow, 
                    void *handlerRefCon, DragReference theDrag)
{
  static nsCOMPtr<nsIDragHelperService> sDragHelper;

  nsCOMPtr<nsIEventSink> windowEventSink;
  nsToolkit::GetWindowEventSink(theWindow, getter_AddRefs(windowEventSink));
  if ( !theWindow || !windowEventSink )
    return dragNotAcceptedErr;
    
  switch ( theMessage ) {
  
    case kDragTrackingEnterHandler:
      break;
      
    case kDragTrackingEnterWindow:
    {
      sDragHelper = do_GetService ( "@mozilla.org/widget/draghelperservice;1" );
      NS_ASSERTION ( sDragHelper, "Couldn't get a drag service, we're in biiig trouble" );
      if ( sDragHelper )
        sDragHelper->Enter ( theDrag, windowEventSink );
      break;
    }
    
    case kDragTrackingInWindow:
    {
      if ( sDragHelper ) {
        PRBool dropAllowed = PR_FALSE;
        sDragHelper->Tracking ( theDrag, windowEventSink, &dropAllowed );
      }
      break;
    }
    
    case kDragTrackingLeaveWindow:
    {
      if ( sDragHelper ) {
        sDragHelper->Leave ( theDrag, windowEventSink );
        sDragHelper = nsnull;      
      }
      break;
    }
    
  } // case of each drag message

  return noErr;
  
} // DragTrackingHandler


pascal OSErr
nsMacWindow :: DragReceiveHandler (WindowPtr theWindow, void *handlerRefCon,
                  DragReference theDragRef)
{
  nsCOMPtr<nsIEventSink> windowEventSink;
  nsToolkit::GetWindowEventSink(theWindow, getter_AddRefs(windowEventSink));
  if ( !theWindow || !windowEventSink )
    return dragNotAcceptedErr;
  
  // tell Gecko about the drop. It will tell us if one of the handlers
  // accepted it. If not, we need to tell the drag manager.
  OSErr result = noErr;
  nsCOMPtr<nsIDragHelperService> helper ( do_GetService("@mozilla.org/widget/draghelperservice;1") );
  if ( helper ) {
    PRBool dragAccepted = PR_FALSE;
    helper->Drop ( theDragRef, windowEventSink, &dragAccepted );
    if ( !dragAccepted )
      result = dragNotAcceptedErr;
  }
    
  return result;
  
} // DragReceiveHandler


NS_IMPL_ISUPPORTS_INHERITED4(nsMacWindow, Inherited, nsIEventSink, nsPIWidgetMac, nsPIEventSinkStandalone, 
                                          nsIMacTextInputEventSink)


//-------------------------------------------------------------------------
//
// nsMacWindow constructor
//
//-------------------------------------------------------------------------
nsMacWindow::nsMacWindow() : Inherited()
  , mWindowMadeHere(PR_FALSE)
  , mIsSheet(PR_FALSE)
  , mIgnoreDeactivate(PR_FALSE)
  , mAcceptsActivation(PR_TRUE)
  , mIsActive(PR_FALSE)
  , mZoomOnShow(PR_FALSE)
  , mZooming(PR_FALSE)
  , mResizeIsFromUs(PR_FALSE)
  , mMacEventHandler(nsnull)
#if !TARGET_CARBON
  , mPhantomScrollbar(nsnull)
  , mPhantomScrollbarData(nsnull)
#endif
{
  mMacEventHandler.reset(new nsMacEventHandler(this));
  WIDGET_SET_CLASSNAME("nsMacWindow");  

  // create handlers for drag&drop
  mDragTrackingHandlerUPP = NewDragTrackingHandlerUPP(DragTrackingHandler);
  mDragReceiveHandlerUPP = NewDragReceiveHandlerUPP(DragReceiveHandler);
  mBoundsOffset.v = kWindowTitleBarHeight; // initial guesses
  mBoundsOffset.h = kWindowMarginWidth;
}


//-------------------------------------------------------------------------
//
// nsMacWindow destructor
//
//-------------------------------------------------------------------------
nsMacWindow::~nsMacWindow()
{
  if ( mWindowPtr && mWindowMadeHere ) {
    
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
      
    // clean up DragManager stuff
    if ( mDragTrackingHandlerUPP ) {
      ::RemoveTrackingHandler ( mDragTrackingHandlerUPP, mWindowPtr );
      ::DisposeDragTrackingHandlerUPP ( mDragTrackingHandlerUPP );
     }
    if ( mDragReceiveHandlerUPP ) {
      ::RemoveReceiveHandler ( mDragReceiveHandlerUPP, mWindowPtr );
      ::DisposeDragReceiveHandlerUPP ( mDragReceiveHandlerUPP );
    }

    // ensure we leave the port in a happy state
    CGrafPtr    curPort;
    CGrafPtr    windowPort = ::GetWindowPort(mWindowPtr);
    ::GetPort((GrafPtr*)&curPort);
    PRBool      mustResetPort = (curPort == windowPort);
    
    
    ::DisposeWindow(mWindowPtr);
    mWindowPtr = nsnull;
    
    if (mustResetPort)
      nsGraphicsUtils::SetPortToKnownGoodPort();
  }
  else if ( mWindowPtr && !mWindowMadeHere ) {
    (void)::RemoveWindowProperty(mWindowPtr, kTopLevelWidgetPropertyCreator,
        kTopLevelWidgetRefPropertyTag);
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
  short bottomPinDelta = 0;     // # of pixels to subtract to pin window bottom
  nsCOMPtr<nsIToolkit> theToolkit = aToolkit;

  // build the main native window
  if (aNativeParent == nsnull)
  {
    PRBool allOrDefault;

    if (aInitData) {
      allOrDefault = aInitData->mBorderStyle == eBorderStyle_all ||
                     aInitData->mBorderStyle == eBorderStyle_default;
      mWindowType = aInitData->mWindowType;
      // if a toplevel window was requested without a titlebar, use a dialog windowproc
      if (aInitData->mWindowType == eWindowType_toplevel &&
          (aInitData->mBorderStyle == eBorderStyle_none ||
           !allOrDefault &&
           !(aInitData->mBorderStyle & eBorderStyle_title)))
        mWindowType = eWindowType_dialog;
    }
    else
    {
      allOrDefault = PR_TRUE;
      mWindowType = eWindowType_toplevel;
    }

    static const WindowAttributes kWindowResizableAttributes =
      kWindowResizableAttribute | kWindowLiveResizeAttribute;

    WindowClass windowClass;
    WindowAttributes attributes = kWindowNoAttributes;
    short hOffset = 0, vOffset = 0;

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

        // XXX kSimpleWindowClass is only defined in the 10.3 (or
        //     higher) SDK but MacWindows.h claims it should work on 10.1
        //     and higher.
        windowClass = 18; // kSimpleWindowClass
        break;

      case eWindowType_child:
        windowClass = kPlainWindowClass;
        break;

      case eWindowType_dialog:
        mIsTopWidgetWindow = PR_TRUE;
        if (aInitData)
        {
          // I'm making the assumption here that any dialog created w/out a titlebar is modal and am
          // therefore keeping the old modal dialog proc. I'm only special-casing dialogs with a
          // titlebar since those are the only ones that might end up not being modal.
          // We never give dialog boxes a close box.

          switch (aInitData->mBorderStyle)
          {
            case eBorderStyle_none:
            case eBorderStyle_default:
              windowClass = kModalWindowClass;
              break;

            case eBorderStyle_all:
              windowClass = kDocumentWindowClass;
              attributes = kWindowCollapseBoxAttribute |
                           kWindowResizableAttributes;
              break;

            default:
              if (aParent && (aInitData->mBorderStyle & eBorderStyle_sheet))
              {
                nsWindowType parentType;
                aParent->GetWindowType(parentType);
                if (parentType != eWindowType_invisible)
                {
                  // Mac OS X sheet support
                  mIsSheet = PR_TRUE;
                  windowClass = kSheetWindowClass;
                  if (aInitData->mBorderStyle & eBorderStyle_resizeh)
                  {
                    attributes = kWindowResizableAttributes;
                  }
                }
                else
                {
                  windowClass = kDocumentWindowClass;
                  attributes = kWindowCollapseBoxAttribute;
                }
              }
              else
              {
                windowClass = kDocumentWindowClass;

                // we ignore the close flag here, since mac dialogs should never have a close box.
                switch(aInitData->mBorderStyle & (eBorderStyle_resizeh | eBorderStyle_title))
                {
                  // combinations of individual options.
                  case eBorderStyle_title:
                    attributes = kWindowCollapseBoxAttribute;
                    break;

                  case eBorderStyle_resizeh:
                  case (eBorderStyle_title | eBorderStyle_resizeh):
                    attributes =
                      kWindowCollapseBoxAttribute | kWindowResizableAttributes;
                    break;

                  default:
                    NS_WARNING("Unhandled combination of window flags");
                    break;
                }
              }
          }
        }
        else
        {
          windowClass = kMovableModalWindowClass;
          attributes = kWindowCollapseBoxAttribute;
        }

        hOffset = kDialogMarginWidth;
        vOffset = kDialogTitleBarHeight;
        break;

      case eWindowType_toplevel:
        mIsTopWidgetWindow = PR_TRUE;
        windowClass = kDocumentWindowClass;
        attributes =
          kWindowCollapseBoxAttribute | kWindowToolbarButtonAttribute;

        if (allOrDefault || aInitData->mBorderStyle & eBorderStyle_close)
          attributes |= kWindowCloseBoxAttribute;

        if (allOrDefault || aInitData->mBorderStyle & eBorderStyle_resizeh)
          attributes |= kWindowFullZoomAttribute | kWindowResizableAttributes;

        hOffset = kWindowMarginWidth;
        vOffset = kWindowTitleBarHeight;
        break;

      case eWindowType_invisible:
        // XXX Hack to make the hidden window not show up as a normal window in
        //     OS X (hide it from Expose, don't expose it through
        //     option-minimize, ...).
        windowClass = kPlainWindowClass;

        // XXX kWindowDoesNotCycleAttribute is only defined in the 10.3 (or
        //     higher) SDK but MacWindows.h claims it should work on 10.2
        //     and higher.
        if (nsToolkit::OSXVersion() >= MAC_OS_X_VERSION_10_2_HEX)
        {
          attributes = (1L << 15); // kWindowDoesNotCycleAttribute
        }
        break;

      default:
        NS_ERROR("Unhandled window type!");

        return NS_ERROR_FAILURE;
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

    ::CreateNewWindow(windowClass, attributes, &wRect, &mWindowPtr);

    mWindowMadeHere = PR_TRUE;

    Rect content, structure;
    ::GetWindowBounds(mWindowPtr, kWindowStructureRgn, &structure);
    ::GetWindowBounds(mWindowPtr, kWindowContentRgn, &content);
    mBoundsOffset.v = content.top - structure.top;
    mBoundsOffset.h = content.left - structure.left;
  }
  else
  {
    mWindowPtr = (WindowPtr)aNativeParent;
    mWindowMadeHere = PR_FALSE;
    mVisible = PR_TRUE;
  }

  if (mWindowPtr == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  // In order to get back to this nsIWidget from a WindowPtr, we hang
  // ourselves off a property of the window. This allows places like
  // event handlers to get our widget or event sink when all they have
  // is a native WindowPtr.
  nsIWidget* temp = NS_STATIC_CAST(nsIWidget*, this);
  OSStatus err = ::SetWindowProperty ( mWindowPtr,
                          kTopLevelWidgetPropertyCreator, kTopLevelWidgetRefPropertyTag,
                          sizeof(nsIWidget*), &temp );
  NS_ASSERTION ( err == noErr, "couldn't set a property on the window, event handling will fail" );
  if ( err != noErr )
    return NS_ERROR_FAILURE;

  // reset the coordinates to (0,0) because it's the top level widget
  // and adjust for any adjustment required to requested window bottom
  nsRect bounds(0, 0, aRect.width, aRect.height - bottomPinDelta);

  // We only need a valid aParent if we have a sheet
  if (!aInitData || (aInitData->mBorderStyle == eBorderStyle_default) ||
      !(aInitData->mBorderStyle & eBorderStyle_sheet))
    aParent = nil;

  // init base class
  // (note: aParent is ignored. Mac (real) windows don't want parents)
  Inherited::StandardCreate(aParent, bounds, aHandleEventFunction, aContext, aAppShell, theToolkit, aInitData);

  // there is a lot of work that we only want to do if we actually created
  // the window. Embedding apps would be really torqed off if we mucked around
  // with the window they gave us. Do things like tweak chrome, register
  // event handlers, and install root controls and hacked scrollbars.
  if ( mWindowMadeHere ) {
    if ( mWindowType == eWindowType_popup ) {
      // Here, we put popups in the same layer as native tooltips so that they float above every
      // type of window including modal dialogs. We also ensure that popups do not receive activate
      // events and do not steal focus from other windows.
      ::SetWindowGroup(mWindowPtr, ::GetWindowGroupOfClass(kHelpWindowClass));
      ::SetWindowActivationScope(mWindowPtr, kWindowActivationScopeNone);
    }
    else if ( mWindowType == eWindowType_toplevel ) {
      EventTypeSpec scrollEventList[] = { {kEventClassMouse, kEventMouseWheelMoved} };
      err = ::InstallWindowEventHandler ( mWindowPtr, NewEventHandlerUPP(ScrollEventHandler), 1, scrollEventList, this, NULL );
        // note, passing NULL as the final param to IWEH() causes the UPP to be disposed automatically
        // when the event target (the window) goes away. See CarbonEvents.h for info.

      NS_ASSERTION(err == noErr, "Couldn't install Carbon Scroll Event handlers");
    }

    if (mIsSheet)
    {
      // Mac OS X sheet support
      EventTypeSpec windEventList[] = { {kEventClassWindow, kEventWindowUpdate},
                                        {kEventClassWindow, kEventWindowDrawContent} };
      err = ::InstallWindowEventHandler ( mWindowPtr,
          NewEventHandlerUPP(WindowEventHandler), 2, windEventList, this, NULL );

      NS_ASSERTION(err == noErr, "Couldn't install sheet Event handlers");
    }

    // Since we can only call IWEH() once for each event class such as kEventClassWindow, we register all the event types that
    // we are going to handle here
    const EventTypeSpec windEventList[] = {
                                            {kEventClassWindow, kEventWindowBoundsChanged}, // to enable live resizing
                                            {kEventClassWindow, kEventWindowCollapse},      // to roll up popups when we're minimized
                                            {kEventClassWindow, kEventWindowConstrain}      // to keep invisible windows off the screen
                                          };
    err = ::InstallWindowEventHandler ( mWindowPtr, NewEventHandlerUPP(WindowEventHandler),
                                        GetEventTypeCount(windEventList), windEventList, this, NULL );
    NS_ASSERTION(err == noErr, "Couldn't install Carbon window event handler");

    // register tracking and receive handlers with the native Drag Manager
    if ( mDragTrackingHandlerUPP ) {
      err = ::InstallTrackingHandler ( mDragTrackingHandlerUPP, mWindowPtr, nsnull );
      NS_ASSERTION ( err == noErr, "can't install drag tracking handler");
    }
    if ( mDragReceiveHandlerUPP ) {
      err = ::InstallReceiveHandler ( mDragReceiveHandlerUPP, mWindowPtr, nsnull );
      NS_ASSERTION ( err == noErr, "can't install drag receive handler");
    }

    // If we're a popup, we don't want a border (we want CSS to draw it for us). So
    // install our own window defProc.
    if ( mWindowType == eWindowType_popup )
      InstallBorderlessDefProc(mWindowPtr);

  } // if we created windowPtr

  nsGraphicsUtils::SafeSetPortWindowPort(mWindowPtr);

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
    if ( self ) {
      // convert mouse to local coordinates since that's how the event handler wants them
      StPortSetter portSetter(self->mWindowPtr);
      StOriginSetter originSetter(self->mWindowPtr);
      ::GlobalToLocal(&mouseLoc);
      self->mMacEventHandler->Scroll ( axis, delta, mouseLoc );
    }
  }
  return noErr;
  
} // ScrollEventHandler


pascal OSStatus
nsMacWindow :: WindowEventHandler ( EventHandlerCallRef inHandlerChain, EventRef inEvent, void* userData )
{
  OSStatus retVal = eventNotHandledErr;  // Presume we won't consume the event
  
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
          retVal = noErr;  // We did consume the resize event
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
            retVal = ::CallNextEventHandler(inHandlerChain, inEvent);
          else
            retVal = noErr;  // consume the event for the hidden window
        }
        break;
      }      

      case kEventWindowUpdate:
      case kEventWindowDrawContent:
      {
        nsMacWindow *self = NS_REINTERPRET_CAST(nsMacWindow *, userData);
        if (self) self->mMacEventHandler->UpdateEvent();
      }
      break;

      // roll up popups when we're minimized
      case kEventWindowCollapse:
      {
        if ( gRollupListener && gRollupWidget )
          gRollupListener->Rollup();        
        retVal = ::CallNextEventHandler(inHandlerChain, inEvent);
      }
      break;
        
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
NS_IMETHODIMP nsMacWindow::Create(nsNativeWidget aNativeParent,   // this is a windowPtr
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
#if TARGET_CARBON
  // Mac OS X sheet support
  nsIWidget *parentWidget = mParent;
  nsCOMPtr<nsPIWidgetMac> piParentWidget ( do_QueryInterface(parentWidget) );
  WindowRef parentWindowRef = (parentWidget) ?
    reinterpret_cast<WindowRef>(parentWidget->GetNativeData(NS_NATIVE_DISPLAY)) : nsnull;
#endif

  Inherited::Show(bState);
  
  // we need to make sure we call ::Show/HideWindow() to generate the 
  // necessary activate/deactivate events. Calling ::ShowHide() is
  // not adequate, unless we don't want activation (popups). (pinkerton).
  if ( bState && !mBounds.IsEmpty() ) {
#if TARGET_CARBON
    if ( mIsSheet && parentWindowRef ) {
        WindowPtr top = GetWindowTop(parentWindowRef);
        if (piParentWidget)
        {
          piParentWidget->SetIgnoreDeactivate(PR_TRUE);
          PRBool sheetFlag = PR_FALSE;
          if (parentWindowRef &&
            NS_SUCCEEDED(piParentWidget->GetIsSheet(&sheetFlag)) && (sheetFlag))
          {
            ::GetSheetWindowParent(parentWindowRef, &top);
            ::HideSheetWindow(parentWindowRef);
          }
        }
        ::ShowSheetWindow(mWindowPtr, top);
        UpdateWindowMenubar(parentWindowRef, PR_FALSE);
        gEventDispatchHandler.DispatchGuiEvent(this, NS_GOTFOCUS);
        gEventDispatchHandler.DispatchGuiEvent(this, NS_ACTIVATE);
    }
    else
#endif
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
#if TARGET_CARBON
    // Mac OS X sheet support
    if (mIsSheet) {
      if (piParentWidget)
        piParentWidget->SetIgnoreDeactivate(PR_FALSE);

      // get sheet's parent *before* hiding the sheet (which breaks the linkage)
      WindowPtr sheetParent = nsnull;
      ::GetSheetWindowParent(mWindowPtr, &sheetParent);

      ::HideSheetWindow(mWindowPtr);

      gEventDispatchHandler.DispatchGuiEvent(this, NS_DEACTIVATE);

      // if we had several sheets open, when the last one goes away
      // we need to ensure that the top app window is active
      WindowPtr top = GetWindowTop(parentWindowRef);
      piParentWidget = do_QueryInterface(parentWidget);

      PRBool sheetFlag = PR_FALSE;
      if (parentWindowRef && piParentWidget &&
        NS_SUCCEEDED(piParentWidget->GetIsSheet(&sheetFlag)) && (sheetFlag))
      {
        nsCOMPtr<nsIWidget> parentWidget;
        nsToolkit::GetTopWidget(sheetParent, getter_AddRefs(parentWidget));
        piParentWidget = do_QueryInterface(parentWidget);
        if (piParentWidget)
          piParentWidget->SetIgnoreDeactivate(PR_TRUE);
        ::ShowSheetWindow(parentWindowRef, sheetParent);
      }
      else if ( mAcceptsActivation ) {
        ::ShowWindow(top);
      }
      else {
        ::ShowHide(top, true);
        ::BringToFront(top); // competes with ComeToFront, but makes popups work
      }
      ComeToFront();

      if (top == parentWindowRef) {
        UpdateWindowMenubar(parentWindowRef, PR_TRUE);
      }
    }
    else
#endif
      if ( mWindowPtr ) {
        ::HideWindow(mWindowPtr);
      }
    }
    return NS_OK;
}


//-------------------------------------------------------------------------
//
// GetWindowTop
//
// find the topmost sheet currently assigned to a baseWindowRef or,
// if no sheet is attached to it, just return the baseWindowRef itself
//
//-------------------------------------------------------------------------
WindowPtr
nsMacWindow::GetWindowTop(WindowPtr baseWindowRef)
{
    if (!baseWindowRef) return(nsnull);

/*
    Note: it would be nice to use window groups... something like the
          following;  however, ::CountWindowGroupContents() and
          ::GetIndexedWindow() are available on Mac OS X but NOT in
          CarbonLib on pre-Mac OS X systems which we need to support

    WindowGroupRef group = ::WindowGroupRef(baseWindowRef);
    if (group)
    {
        WindowGroupContentOptions options =
            kWindowGroupContentsReturnWindows | kWindowGroupContentsVisible;

        ItemCount numWins = ::CountWindowGroupContents(group, options);
        if (numWins >= 1)
        {
            ::GetIndexedWindow(group, 1, options, &baseWindowRef);
        }
    }
*/

    WindowPtr aSheet = ::GetFrontWindowOfClass(kSheetWindowClass, true);
    while(aSheet)
    {
        WindowRef   sheetParent;
        GetSheetWindowParent(aSheet, &sheetParent);
        if (sheetParent == baseWindowRef)
        {
            return(aSheet);
        }
        aSheet = GetNextWindowOfClass(aSheet, kSheetWindowClass, true);
    }
    return(baseWindowRef);
}


void
nsMacWindow::UpdateWindowMenubar(WindowPtr nativeWindowPtr, PRBool enableFlag)
{
    // Mac OS X sheet support
    // when a sheet is up, disable parent window's menus
    // (at least, until there is a better architecture for menus)

    if (!nativeWindowPtr) return;

    nsCOMPtr<nsIWidget> windowWidget;
    nsToolkit::GetTopWidget ( nativeWindowPtr, getter_AddRefs(windowWidget));
    if (!windowWidget) return;

    nsCOMPtr<nsPIWidgetMac> parentWindow ( do_QueryInterface(windowWidget) );
    if (!parentWindow)  return;
    nsCOMPtr<nsIMenuBar> menubar;
    parentWindow->GetMenuBar(getter_AddRefs(menubar));
    if (!menubar) return;

    PRUint32    numMenus=0;
    menubar->GetMenuCount(numMenus);
    for (PRInt32 i = numMenus-1; i >= 0; i--)
    {
        nsCOMPtr<nsIMenu> menu;
        menubar->GetMenuAt(i, *getter_AddRefs(menu));
        if (menu)
        {
            menu->SetEnabled(enableFlag);
        }
    }
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

NS_IMETHODIMP nsMacWindow::ConstrainPosition(PRBool aAllowSlop,
                                             PRInt32 *aX, PRInt32 *aY)
{
  if (eWindowType_popup == mWindowType || !mWindowMadeHere)
    return NS_OK;

  // Sanity check against screen size
  // make sure the window stays visible

  // get the window bounds
  Rect portBounds;
  ::GetWindowPortBounds(mWindowPtr, &portBounds);
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

  if (aAllowSlop) {
    short pos;
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
  } else {
    if (*aX < screenRect.left)
      *aX = screenRect.left;
    else if (*aX >= screenRect.right - windowWidth)
      *aX = screenRect.right - windowWidth;

    if (*aY < screenRect.top)
      *aY = screenRect.top;
    else if (*aY >= screenRect.bottom - windowHeight)
      *aY = screenRect.bottom - windowHeight;
  }

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
        
        // update userstate to match, if appropriate
        PRInt32 sizeMode;
        nsBaseWidget::GetSizeMode ( &sizeMode );
        if ( sizeMode == nsSizeMode_Normal ) {
          ::GetWindowBounds ( mWindowPtr, kWindowGlobalPortRgn, &currBounds );
          ::SetWindowUserState ( mWindowPtr, &currBounds );
        }  
      }  
    }

    return NS_OK;
  } else if (mWindowMadeHere) {
    Rect portBounds;
    ::GetWindowPortBounds(mWindowPtr, &portBounds);

    aX += mBoundsOffset.h;
    aY += mBoundsOffset.v;

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
    ::LocalToGlobal((Point *) &portBounds.top);
    ::LocalToGlobal((Point *) &portBounds.bottom);
    if (portBounds.left != aX || portBounds.top != aY) {
      ::MoveWindow(mWindowPtr, aX, aY, false);

      // update userstate to match, if appropriate
      PRInt32 sizeMode;
      GetSizeMode(&sizeMode);
      if (sizeMode == nsSizeMode_Normal)
        ::SetWindowUserState(mWindowPtr, &portBounds);
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
// Position the window behind the given window
//
//-------------------------------------------------------------------------
NS_METHOD nsMacWindow::PlaceBehind(nsTopLevelWidgetZPlacement aPlacement,
                                   nsIWidget *aWidget, PRBool aActivate)
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

  // resize during zoom may attempt to unzoom us. here's where we put a stop to that.
  if (mZooming)
    return NS_OK;

  // even if our current mode is the same as aMode, keep going
  // because the window can be maximized and zoomed, but partially
  // offscreen, and we want a click of the maximize button to move
  // it back onscreen.

  if (!mVisible) {
    /* zooming on the Mac doesn't seem to work until the window is visible.
       the rest of the app is structured to zoom before the window is visible
       to avoid flashing. here's where we defeat that. */
    mZoomOnShow = aMode == nsSizeMode_Maximized;
  } else {
    Rect macRect;
    mZooming = PR_TRUE;
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
    mZooming = PR_FALSE;
  }

  return rv;
}


// we're resizing. if we happen to be zoomed ("standard" state),
// switch to the user state without changing the window size.
void
nsMacWindow::UserStateForResize()
{
  nsresult rv;
  PRInt32  currentMode;

  // but not if we're currently zooming
  if (mZooming)
    return;

  // nothing to do if we're already in the user state
  rv = nsBaseWidget::GetSizeMode(&currentMode);
  if (NS_SUCCEEDED(rv) && currentMode == nsSizeMode_Normal)
    return;

  // we've just finished sizing, so make our current size our user state,
  // and switch to the user state
  StPortSetter portState(mWindowPtr);
  StOriginSetter originState(mWindowPtr);
  Rect bounds;

  ::GetWindowPortBounds(mWindowPtr, &bounds);
  ::LocalToGlobal((Point *)&bounds.top);
  ::LocalToGlobal((Point *)&bounds.bottom);
  ::SetWindowUserState(mWindowPtr, &bounds);
  ::ZoomWindow(mWindowPtr, inZoomIn, false);

  // finally, reset our internal state to match
  nsBaseWidget::SetSizeMode(nsSizeMode_Normal);
  // Tell container window we've moved. We stopped reporting move events
  // when we were in the StandardState.
  ReportMoveEvent();
}


//
// CalculateAndSetZoomedSize
//
// Recomputes the zoomed window size taking things such as window chrome,
// dock position, menubar, and finder icons into account
//
NS_IMETHODIMP
nsMacWindow::CalculateAndSetZoomedSize()
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
        int iconSpace = 128;
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
  
  return NS_OK;

} // CalculateAndSetZoomedSize


//-------------------------------------------------------------------------
//
// Obtain the menubar for a window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMacWindow::GetMenuBar(nsIMenuBar **_retval)
{
  *_retval = mMenuBar;
  NS_IF_ADDREF(*_retval);
  return(NS_OK);
}

//-------------------------------------------------------------------------
//
// getter/setter for window to ignore the next deactivate event received
// if a Mac OS X sheet is being opened
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMacWindow::GetIgnoreDeactivate(PRBool *_retval)
{
  *_retval = mIgnoreDeactivate;
  return(NS_OK);
}

NS_IMETHODIMP
nsMacWindow::SetIgnoreDeactivate(PRBool ignoreDeactivate)
{
  mIgnoreDeactivate = ignoreDeactivate;
  return(NS_OK);
}

NS_IMETHODIMP
nsMacWindow::GetIsSheet(PRBool *_retval)
{
  *_retval = mIsSheet;
  return(NS_OK);
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

  Move(aX-mBoundsOffset.h, aY-mBoundsOffset.v);
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

    short w = macRect.right - macRect.left;
    short h = macRect.bottom - macRect.top;

    if ((w != aWidth) || (h != aHeight))
    {
      // make sure that we don't infinitely recurse if live-resize is on
      mResizeIsFromUs = PR_TRUE;
      ::SizeWindow(mWindowPtr, aWidth, aHeight, aRepaint);

      // update userstate to match, if appropriate
      PRInt32 sizeMode;
      GetSizeMode(&sizeMode);
      if (sizeMode == nsSizeMode_Normal) {
        Rect portBounds;
        ::GetWindowBounds(mWindowPtr, kWindowGlobalPortRgn, &portBounds);
        ::SetWindowUserState(mWindowPtr, &portBounds);
      }

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
 
  aRect.MoveBy(-mBoundsOffset.h, -mBoundsOffset.v-yAdjust);

  return NS_OK;
}

//-------------------------------------------------------------------------
//
//
//-------------------------------------------------------------------------
PRBool nsMacWindow::OnPaint(nsPaintEvent &event)
{
  return PR_TRUE; // don't dispatch the update event
}

//-------------------------------------------------------------------------
//
// Set this window's title
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMacWindow::SetTitle(const nsAString& aTitle)
{
  nsAString::const_iterator begin;
  const PRUnichar *strTitle = aTitle.BeginReading(begin).get();
  // Try to use the unicode friendly CFString version first
  CFStringRef labelRef = ::CFStringCreateWithCharacters(kCFAllocatorDefault, (UniChar*)strTitle, aTitle.Length());
  if(labelRef) {
    ::SetWindowTitleWithCFString(mWindowPtr, labelRef);
    ::CFRelease(labelRef);
    return NS_OK;
  }

  Str255 title;
  // unicode to file system charset
  nsMacControl::StringToStr255(aTitle, title);
  ::SetWTitle(mWindowPtr, title);
  return NS_OK;
}


#pragma mark -

//
// impleemnt nsIMacTextInputEventSink: forward to the right method in nsMacEventHandler 
//

/* OSStatus HandleUpdateActiveInputArea (in wstring text, in long textLength, in short script, in short language, in long fixLen, in voidPtr hiliteRng); */
NS_IMETHODIMP 
nsMacWindow::HandleUpdateActiveInputArea(const nsAString & text, 
                                         PRInt16 script, PRInt16 language, PRInt32 fixLen, void * hiliteRng, 
                                         OSStatus *_retval)
{
  *_retval = eventNotHandledErr;
  NS_ENSURE_TRUE(mMacEventHandler.get(), NS_ERROR_FAILURE);
  const nsPromiseFlatString& buffer = PromiseFlatString(text);
  // ignore script and langauge information for now. 
  nsresult res = mMacEventHandler->UnicodeHandleUpdateInputArea(buffer.get(), buffer.Length(), fixLen, (TextRangeArray*) hiliteRng);
  // we will lost the real OSStatus for now untill we change the nsMacEventHandler
  if (NS_SUCCEEDED(res))
    *_retval = noErr;
  return res;
}

/* OSStatus HandleUpdateActiveInputAreaForNonUnicode (in string text, in long textLength, in short script, in short language, in long fixLen, in voidPtr hiliteRng); */
NS_IMETHODIMP 
nsMacWindow::HandleUpdateActiveInputAreaForNonUnicode(const nsACString & text, 
                                                      PRInt16 script, PRInt16 language, 
                                                      PRInt32 fixLen, void * hiliteRng, 
                                                      OSStatus *_retval)
{
  *_retval = eventNotHandledErr;
  NS_ENSURE_TRUE(mMacEventHandler.get(), NS_ERROR_FAILURE);
  const nsPromiseFlatCString& buffer = PromiseFlatCString(text);
  // ignore langauge information for now. 
  nsresult res = mMacEventHandler->HandleUpdateInputArea(buffer.get(), buffer.Length(), script, fixLen, (TextRangeArray*) hiliteRng);
  // we will lost the real OSStatus for now untill we change the nsMacEventHandler
  if (NS_SUCCEEDED(res))
    *_retval = noErr;
  return res;
}

/* OSStatus HandleUnicodeForKeyEvent (in wstring text, in long textLength, in short script, in short language, in voidPtr keyboardEvent); */
NS_IMETHODIMP 
nsMacWindow::HandleUnicodeForKeyEvent(const nsAString & text, 
                                      PRInt16 script, PRInt16 language, void * keyboardEvent, 
                                      OSStatus *_retval)
{
  *_retval = eventNotHandledErr;
  NS_ENSURE_TRUE(mMacEventHandler.get(), NS_ERROR_FAILURE);
  // ignore langauge information for now. 
  // we will lost the real OSStatus for now untill we change the nsMacEventHandler
  EventRecord* eventPtr = (EventRecord*)keyboardEvent;
  const nsPromiseFlatString& buffer = PromiseFlatString(text);
  nsresult res = mMacEventHandler->HandleUKeyEvent(buffer.get(), buffer.Length(), *eventPtr);
  // we will lost the real OSStatus for now untill we change the nsMacEventHandler
  if(NS_SUCCEEDED(res))
    *_retval = noErr;
  return res;
}

/* OSStatus HandleOffsetToPos (in long offset, out short pointX, out short pointY); */
NS_IMETHODIMP 
nsMacWindow::HandleOffsetToPos(PRInt32 offset, PRInt16 *pointX, PRInt16 *pointY, OSStatus *_retval)
{
  *_retval = eventNotHandledErr;
  NS_ENSURE_TRUE(mMacEventHandler.get(), NS_ERROR_FAILURE);
  *pointX = *pointY = 0;
  Point thePoint = {0,0};
  nsresult res = mMacEventHandler->HandleOffsetToPosition(offset, &thePoint);
  // we will lost the real OSStatus for now untill we change the nsMacEventHandler
  if(NS_SUCCEEDED(res))
    *_retval = noErr;
  *pointX = thePoint.h;
  *pointY = thePoint.v;  
  return res;
}

/* OSStatus 
HandlePosToOffset (in short currentPointX, in short currentPointY, out long offset, out short regionClass); */
NS_IMETHODIMP 
nsMacWindow::HandlePosToOffset(PRInt16 currentPointX, PRInt16 currentPointY, 
                               PRInt32 *offset, PRInt16 *regionClass, OSStatus *_retval)
{
  *_retval = eventNotHandledErr;
  NS_ENSURE_TRUE(mMacEventHandler.get(), NS_ERROR_FAILURE);
  *_retval = noErr;
  Point thePoint;
  thePoint.h = currentPointX;
  thePoint.v = currentPointY;
  *offset = mMacEventHandler->HandlePositionToOffset(thePoint, regionClass);
  return NS_OK;
}

/* OSStatus HandleGetSelectedText (out AString selectedText); */
NS_IMETHODIMP 
nsMacWindow::HandleGetSelectedText(nsAString & selectedText, OSStatus *_retval)
{
  *_retval = noErr;
  NS_ENSURE_TRUE(mMacEventHandler.get(), NS_ERROR_FAILURE);
  return mMacEventHandler->HandleUnicodeGetSelectedText(selectedText);
}

#pragma mark -


//
// DispatchEvent
//
// Handle an event coming into us and send it to gecko.
//
NS_IMETHODIMP
nsMacWindow::DispatchEvent ( void* anEvent, PRBool *_retval )
{
  *_retval = PR_FALSE;
  if (mMacEventHandler.get())
    *_retval = mMacEventHandler->HandleOSEvent(*NS_REINTERPRET_CAST(EventRecord*,anEvent));

  return NS_OK;
}


//
// DispatchEvent
//
// Handle an event coming into us and send it to gecko.
//
NS_IMETHODIMP
nsMacWindow::DispatchMenuEvent ( void* anEvent, PRInt32 aNativeResult, PRBool *_retval )
{
#if USE_MENUSELECT
  *_retval = PR_FALSE;
  if (mMacEventHandler.get())
    *_retval = mMacEventHandler->HandleMenuCommand(*NS_REINTERPRET_CAST(EventRecord*,anEvent), aNativeResult);
#endif

  return NS_OK;
}


//
// DragEvent
//
// The drag manager has let us know that something related to a drag has
// occurred in this window. It could be any number of things, ranging from 
// a drop, to a drag enter/leave, or a drag over event. The actual event
// is passed in |aMessage| and is passed along to our event hanlder so Gecko
// knows about it.
//
NS_IMETHODIMP
nsMacWindow::DragEvent(PRUint32 aMessage, PRInt16 aMouseGlobalX, PRInt16 aMouseGlobalY,
                         PRUint16 aKeyModifiers, PRBool *_retval)
{
  *_retval = PR_FALSE;
  Point globalPoint = {aMouseGlobalY, aMouseGlobalX};         // QD Point stored as v, h
  if (mMacEventHandler.get())
    *_retval = mMacEventHandler->DragEvent(aMessage, globalPoint, aKeyModifiers);
  
  return NS_OK;
}


//
// Scroll
//
// Someone wants us to scroll in the current window, probably as the result
// of a scrollWheel event or external scrollbars. Pass along to the 
// eventhandler.
//
NS_IMETHODIMP
nsMacWindow::Scroll ( PRBool aVertical, PRInt16 aNumLines, PRInt16 aMouseLocalX, 
                        PRInt16 aMouseLocalY, PRBool *_retval )
{
  *_retval = PR_FALSE;
  Point localPoint = {aMouseLocalY, aMouseLocalX};
  if ( mMacEventHandler.get() )
    *_retval = mMacEventHandler->Scroll(aVertical ? kEventMouseWheelAxisY : kEventMouseWheelAxisX,
                                          aNumLines, localPoint);
  return NS_OK;
}


NS_IMETHODIMP
nsMacWindow::Idle()
{
  // do some idle stuff?
  return NS_ERROR_NOT_IMPLEMENTED;
}


#pragma mark -


//-------------------------------------------------------------------------
//
// Like ::BringToFront, but constrains the window to its z-level
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsMacWindow::ComeToFront()
{
  nsZLevelEvent  event(NS_SETZLEVEL, this);

  event.point.x = mBounds.x;
  event.point.y = mBounds.y;
  event.time = PR_IntervalNow();

  event.mImmediate = PR_TRUE;

  DispatchWindowEvent(event);
  
  return NS_OK;
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
