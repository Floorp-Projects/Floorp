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
 *  Mark Mentovai <mark@moxienet.com>
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
#include "nsToolkit.h"

#include "nsIServiceManager.h"    // for drag and drop
#include "nsWidgetsCID.h"
#include "nsIDragHelperService.h"
#include "nsIScreen.h"
#include "nsIScreenManager.h"
#include "nsGUIEvent.h"
#include "nsCarbonHelpers.h"
#include "nsGfxUtils.h"
#include "nsMacResources.h"
#include "nsIRollupListener.h"
#include "nsCRT.h"
#include "nsWidgetSupport.h"

#include <CFString.h>

#include <Gestalt.h>
#include <Quickdraw.h>
#include <MacWindows.h>

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


#define kWindowPositionSlop 20

// These are only initial guesses. Real values are filled in
// after window creation.
const short kWindowTitleBarHeight = 22;
const short kWindowMarginWidth = 0;
const short kDialogTitleBarHeight = 22;
const short kDialogMarginWidth = 0;

#if 0

// routines for debugging port state issues (which are legion)
static void PrintRgn(const char* inLabel, RgnHandle inRgn)
{
  Rect regionBounds;
  GetRegionBounds(inRgn, &regionBounds);
  printf("%s left %d, top %d, right %d, bottom %d\n", inLabel,
    regionBounds.left, regionBounds.top, regionBounds.right, regionBounds.bottom);
}

static void PrintPortState(WindowPtr inWindow, const char* label)
{
  CGrafPtr currentPort = CGrafPtr(GetQDGlobalsThePort());
  Rect bounds;
  GetPortBounds(currentPort, &bounds);
  printf("%s: Current port: %p, top, left = %d, %d\n", label, currentPort, bounds.top, bounds.left);

  StRegionFromPool savedClip;
  ::GetClip(savedClip);
  PrintRgn("  clip:", savedClip);

  StRegionFromPool updateRgn;
  ::GetWindowUpdateRegion(inWindow, updateRgn);
  Rect windowBounds;
  ::GetWindowBounds(inWindow, kWindowContentRgn, &windowBounds);
  ::OffsetRgn(updateRgn, -windowBounds.left, -windowBounds.top);

  PrintRgn("  update:", updateRgn);
}

#endif

#pragma mark -

pascal OSErr
nsMacWindow::DragTrackingHandler ( DragTrackingMessage theMessage, WindowPtr theWindow, 
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
nsMacWindow::DragReceiveHandler (WindowPtr theWindow, void *handlerRefCon,
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
  , mAcceptsActivation(PR_TRUE)
  , mIsActive(PR_FALSE)
  , mZoomOnShow(PR_FALSE)
  , mZooming(PR_FALSE)
  , mResizeIsFromUs(PR_FALSE)
  , mShown(PR_FALSE)
  , mSheetNeedsShow(PR_FALSE)
  , mMacEventHandler(nsnull)
#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_3
  , mNeedsResize(PR_FALSE)
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

  NS_ASSERTION(!aInitData || aInitData->mWindowType != eWindowType_popup ||
               !aParent, "Popups should not be hooked into nsIWidget hierarchy");

  // build the main native window
  if (!aNativeParent || (aInitData && aInitData->mWindowType == eWindowType_popup))
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
        if( aParent )
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
          // We never give dialog boxes a close box.
          switch (aInitData->mBorderStyle)
          {
            case eBorderStyle_none:
              windowClass = kModalWindowClass;
              break;

            case eBorderStyle_default:
              windowClass = kDocumentWindowClass;
              break;

            case eBorderStyle_all:
              windowClass = kDocumentWindowClass;
              attributes = kWindowCollapseBoxAttribute |
                           kWindowResizableAttributes;
              break;

            default:
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

                  case eBorderStyle_none: // 0
                    // kSimpleWindowClass (18) is only defined in SDK >=
                    // 10.3, but it works with runtime >= 10.1.  It's
                    // borderless and chromeless, which is more visually
                    // appropriate in Aqua than kPlainWindowClass, which
                    // gives a 1px black border.
                    windowClass = 18; // kSimpleWindowClass
                    break;

                  default: // not reached
                    NS_WARNING("Unhandled combination of window flags");
                    break;
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

      case eWindowType_sheet:
        mIsTopWidgetWindow = PR_TRUE;
        if (aInitData)
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

    // Getting kWindowContentRgn can give back bad values on Panther
    // (fixed on Tiger), but wRect is already set to the content rect anyway.
    Rect structure;
    ::GetWindowBounds(mWindowPtr, kWindowStructureRgn, &structure);
    mBoundsOffset.v = wRect.top - structure.top;
    mBoundsOffset.h = wRect.left - structure.left;
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
  if (!aInitData || aInitData->mWindowType != eWindowType_sheet)
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

    if ( mWindowType != eWindowType_invisible &&
         mWindowType != eWindowType_plugin &&
         mWindowType != eWindowType_java) {
      const EventTypeSpec kScrollEventList[] = {
        { kEventClassMouse, kEventMouseWheelMoved },
      };

      static EventHandlerUPP sScrollEventHandlerUPP;
      if (!sScrollEventHandlerUPP)
        sScrollEventHandlerUPP = ::NewEventHandlerUPP(ScrollEventHandler);

      err = ::InstallWindowEventHandler(mWindowPtr,
                                        sScrollEventHandlerUPP,
                                        GetEventTypeCount(kScrollEventList),
                                        kScrollEventList,
                                        (void*)this,
                                        NULL);
      NS_ASSERTION(err == noErr, "Couldn't install scroll event handler");
    }

    // Window event handler
    const EventTypeSpec kWindowEventList[] = {
      // to enable live resizing
      { kEventClassWindow, kEventWindowBoundsChanged },
      // to roll up popups when we're minimized
      { kEventClassWindow, kEventWindowCollapsing },
      // to activate when restoring
      { kEventClassWindow, kEventWindowExpanded },
      // to keep invisible windows off the screen
      { kEventClassWindow, kEventWindowConstrain },
      // to handle update events
      { kEventClassWindow, kEventWindowUpdate },
      // to handle activation
      { kEventClassWindow, kEventWindowActivated },
      { kEventClassWindow, kEventWindowDeactivated },
    };

    static EventHandlerUPP sWindowEventHandlerUPP;
    if (!sWindowEventHandlerUPP)
      sWindowEventHandlerUPP = ::NewEventHandlerUPP(WindowEventHandler);

    err = ::InstallWindowEventHandler(mWindowPtr,
                                      sWindowEventHandlerUPP,
                                      GetEventTypeCount(kWindowEventList),
                                      kWindowEventList,
                                      (void*)this,
                                      NULL);
    NS_ASSERTION(err == noErr, "Couldn't install window event handler");

    // Key event handler
    const EventTypeSpec kKeyEventList[] = {
      { kEventClassKeyboard, kEventRawKeyDown },
      { kEventClassKeyboard, kEventRawKeyUp },
    };

    static EventHandlerUPP sKeyEventHandlerUPP;
    if (!sKeyEventHandlerUPP)
      sKeyEventHandlerUPP = ::NewEventHandlerUPP(KeyEventHandler);

    err = ::InstallWindowEventHandler(mWindowPtr,
                                      sKeyEventHandlerUPP,
                                      GetEventTypeCount(kKeyEventList),
                                      kKeyEventList,
                                      NS_STATIC_CAST(void*, this),
                                      NULL);
    NS_ASSERTION(err == noErr, "Couldn't install key event handler");

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



pascal OSStatus
nsMacWindow::ScrollEventHandler ( EventHandlerCallRef inHandlerChain, EventRef inEvent, void* userData )
{
  OSStatus retVal = eventNotHandledErr;
  EventMouseWheelAxis axis = kEventMouseWheelAxisY;
  SInt32 delta = 0;
  Point mouseLoc;
  UInt32 modifiers = 0;
  if (::GetEventParameter(inEvent, kEventParamMouseWheelAxis,
                          typeMouseWheelAxis, NULL,
                          sizeof(EventMouseWheelAxis), NULL, &axis) == noErr &&
      ::GetEventParameter(inEvent, kEventParamMouseWheelDelta,
                          typeLongInteger, NULL,
                          sizeof(SInt32), NULL, &delta) == noErr &&
      ::GetEventParameter(inEvent, kEventParamMouseLocation,
                          typeQDPoint, NULL,
                          sizeof(Point), NULL, &mouseLoc) == noErr &&
      ::GetEventParameter(inEvent, kEventParamKeyModifiers,
                          typeUInt32, NULL,
                          sizeof(UInt32), NULL, &modifiers) == noErr) {
    nsMacWindow* self = NS_REINTERPRET_CAST(nsMacWindow*, userData);
    NS_ENSURE_TRUE(self->mMacEventHandler.get(), eventNotHandledErr);
    if (self) {
      // Convert mouse to local coordinates since that's how the event handler 
      // wants them
      StPortSetter portSetter(self->mWindowPtr);
      StOriginSetter originSetter(self->mWindowPtr);
      ::GlobalToLocal(&mouseLoc);
      self->mMacEventHandler->Scroll(axis, delta, mouseLoc, self, modifiers);
      retVal = noErr;
    }
  }
  return retVal;
} // ScrollEventHandler


pascal OSStatus
nsMacWindow::WindowEventHandler ( EventHandlerCallRef inHandlerChain, EventRef inEvent, void* userData )
{
  OSStatus retVal = eventNotHandledErr;  // Presume we won't consume the event
  nsMacWindow* self = NS_REINTERPRET_CAST(nsMacWindow*, userData);
  if (self) {
    UInt32 what = ::GetEventKind(inEvent);
    switch (what) {
    
      case kEventWindowBoundsChanged:
      {
        // are we moving or resizing the window? we only care about resize.
        UInt32 attributes = 0;
        ::GetEventParameter ( inEvent, kEventParamAttributes, typeUInt32, NULL, sizeof(attributes), NULL, &attributes );
        if ( attributes & kWindowBoundsChangeSizeChanged ) {
          WindowRef myWind = NULL;
          ::GetEventParameter(inEvent, kEventParamDirectObject, typeWindowRef, NULL, sizeof(myWind), NULL, &myWind);
          Rect bounds;
          ::InvalWindowRect(myWind, ::GetWindowPortBounds(myWind, &bounds));
          
          // resize the window and repaint
          NS_ENSURE_TRUE(self->mMacEventHandler.get(), eventNotHandledErr);
          if (!self->mResizeIsFromUs ) {
            self->mMacEventHandler->ResizeEvent(myWind);
            self->Update();
          }
          retVal = noErr;  // We did consume the resize event
        }
        break;
      }
      
      case kEventWindowConstrain:
      {
        // Ignore this event if we're an invisible window, otherwise pass along the
        // chain to ensure it's onscreen.
        if ( self->mWindowType != eWindowType_invisible )
          retVal = ::CallNextEventHandler(inHandlerChain, inEvent);
        else
          retVal = noErr;  // consume the event for the hidden window
        break;
      }      

      case kEventWindowUpdate:
      {
        self->Update();
        retVal = noErr; // consume
      }
      break;

      // roll up popups when we're minimized
      case kEventWindowCollapsing:
      {
        if ( gRollupListener && gRollupWidget )
          gRollupListener->Rollup();        
        self->mMacEventHandler->GetEventDispatchHandler()->DispatchGuiEvent(self, NS_DEACTIVATE);
        ::CallNextEventHandler(inHandlerChain, inEvent);
        retVal = noErr; // do default processing, but consume
      }
      break;

      // Activate when restoring from the minimized state.  This ensures that
      // the restored window will be able to take focus.
      case kEventWindowExpanded:
      {
        self->mMacEventHandler->GetEventDispatchHandler()->DispatchGuiEvent(self, NS_ACTIVATE);
        ::CallNextEventHandler(inHandlerChain, inEvent);
        retVal = noErr; // do default processing, but consume
      }
      break;

      case kEventWindowActivated:
      case kEventWindowDeactivated:
      {
        self->mMacEventHandler->HandleActivateEvent(inEvent);
        ::CallNextEventHandler(inHandlerChain, inEvent);
        retVal = noErr; // do default processing, but consume
      }
      break;

    } // case of which event?
  }
  
  return retVal;
  
} // WindowEventHandler



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
nsMacWindow::InstallBorderlessDefProc ( WindowPtr inWindow )
{
} // InstallBorderlessDefProc


//
// RemoveBorderlessDefProc
//
// Clean up the mess we've made with our fake defproc. Reset it to
// the system one, just in case someone needs it around after we're
// through with it.
//
void
nsMacWindow::RemoveBorderlessDefProc ( WindowPtr inWindow )
{
}


//-------------------------------------------------------------------------
//
// Hide or show this window
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsMacWindow::Show(PRBool aState)
{
  // Mac OS X sheet support
  nsIWidget *parentWidget = mParent;
  nsCOMPtr<nsPIWidgetMac> piParentWidget ( do_QueryInterface(parentWidget) );
  WindowRef parentWindowRef = (parentWidget) ?
    reinterpret_cast<WindowRef>(parentWidget->GetNativeData(NS_NATIVE_DISPLAY)) : nsnull;

  Inherited::Show(aState);
  
  // we need to make sure we call ::Show/HideWindow() to generate the 
  // necessary activate/deactivate events. Calling ::ShowHide() is
  // not adequate, unless we don't want activation (popups). (pinkerton).
  if (aState && !mBounds.IsEmpty()) {
    if (mIsSheet) {
      if (parentWindowRef) {
        WindowPtr top = parentWindowRef;
        if (piParentWidget) {
          PRBool parentIsSheet = PR_FALSE;
          if (NS_SUCCEEDED(piParentWidget->GetIsSheet(&parentIsSheet)) &&
              parentIsSheet) {
            // If this sheet is the child of another sheet, hide the parent
            // so that this sheet can be displayed.
            // Leave the parent's mShown and mSheetNeedsShow alone, those are
            // only used to handle sibling sheet contention.  The parent will
            // be displayed again when it has no more child sheets.
            ::GetSheetWindowParent(parentWindowRef, &top);
            ::HideSheetWindow(parentWindowRef);
          }
        }

        nsMacWindow* sheetShown = nsnull;
        if (NS_SUCCEEDED(piParentWidget->GetChildSheet(PR_TRUE,
                                                       &sheetShown)) &&
            (!sheetShown || sheetShown == this)) {
          if (!sheetShown) {
            // There is no sheet on the top-level window, show this one.

            // Important to set these member variables first, because
            // ShowSheetWindow may result in native event dispatch causing
            // reentrancy into this code for this window - if mSheetNeedsShow
            // is true, it's possible to show the same sheet twice, and that
            // will cause tremendous problems.
            mShown = PR_TRUE;
            mSheetNeedsShow = PR_FALSE;

            ::ShowSheetWindow(mWindowPtr, top);
          }
          UpdateWindowMenubar(parentWindowRef, PR_FALSE);
          mMacEventHandler->GetEventDispatchHandler()->DispatchGuiEvent(this, NS_GOTFOCUS);
          mMacEventHandler->GetEventDispatchHandler()->DispatchGuiEvent(this, NS_ACTIVATE);
          ComeToFront();
        }
        else {
          // A sibling of this sheet is active, don't show this sheet yet.
          // When the active sheet hides, its brothers and sisters that have
          // mSheetNeedsShow set will have their opportunities to display.
          mSheetNeedsShow = PR_TRUE;
        }
      }
    }
    else {
      if (mAcceptsActivation)
        ::ShowWindow(mWindowPtr);
      else {
        ::ShowHide(mWindowPtr, true);
        // competes with ComeToFront, but makes popups work
        ::BringToFront(mWindowPtr);
      }
      ComeToFront();
      mShown = PR_TRUE;
    }
    if (mZoomOnShow) {
      SetSizeMode(nsSizeMode_Maximized);
      mZoomOnShow = PR_FALSE;
    }
  }
  else {
    // when a toplevel window goes away, make sure we rollup any popups that
    // may be lurking. We want to catch this here because we're guaranteed that
    // we hide a window before we destroy it, and doing it here more closely
    // approximates where we do the same thing on windows.
    if ( mWindowType == eWindowType_toplevel ) {
      if ( gRollupListener )
        gRollupListener->Rollup();
      NS_IF_RELEASE(gRollupListener);
      NS_IF_RELEASE(gRollupWidget);
    }

    // Mac OS X sheet support
    if (mIsSheet) {
      if (mShown) {
        // Set mShown here because a sibling sheet may want to be activated,
        // and mShown needs to be false in order for sibling sheets to display.
        mShown = PR_FALSE;

        // get sheet's parent *before* hiding the sheet (which breaks the
        // linkage)
        WindowPtr sheetParent = nsnull;
        ::GetSheetWindowParent(mWindowPtr, &sheetParent);

        ::HideSheetWindow(mWindowPtr);

        mMacEventHandler->GetEventDispatchHandler()->DispatchGuiEvent(this, NS_DEACTIVATE);

        WindowPtr top = GetWindowTop(parentWindowRef);
        nsMacWindow* siblingSheetToShow = nsnull;
        PRBool parentIsSheet = PR_FALSE;

        if (parentWindowRef && piParentWidget &&
            NS_SUCCEEDED(piParentWidget->GetChildSheet(PR_FALSE, 
                                                       &siblingSheetToShow)) &&
            siblingSheetToShow) {
          // First, give sibling sheets an opportunity to show.
          siblingSheetToShow->Show(PR_TRUE);
        }
        else if (parentWindowRef && piParentWidget &&
                 NS_SUCCEEDED(piParentWidget->GetIsSheet(&parentIsSheet)) &&
                 parentIsSheet) {
          // If there are no sibling sheets, but the parent is a sheet, restore
          // it.  It wasn't sent any deactivate events when it was hidden, so
          // don't call through Show, just let the OS put it back up.
          ::ShowSheetWindow(parentWindowRef, sheetParent);
        }
        else {
          // Sheet, that was hard.  No more siblings or parents, going back
          // to a real window.

          // if we had several sheets open, when the last one goes away
          // we need to ensure that the top app window is active
          if (mAcceptsActivation)
            ::ShowWindow(top);
          else {
            ::ShowHide(top, true);
            // competes with ComeToFront, but makes popups work
            ::BringToFront(top);
          }
        }
        ComeToFront();

        if (top == parentWindowRef)
          UpdateWindowMenubar(parentWindowRef, PR_TRUE);
      }
      else if (mSheetNeedsShow) {
        // This is an attempt to hide a sheet that never had a chance to
        // be shown.  There's nothing to do other than make sure that it
        // won't show.
        mSheetNeedsShow = PR_FALSE;
      }
    }
    else {
      if (mWindowPtr)
        ::HideWindow(mWindowPtr);
      mShown = PR_FALSE;
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
    // there is a bug on OSX where if we call ::MoveWindow() with the same
    // coordinates (within a pixel or two) as a window's current location, it will 
    // move to (0,0,-1,-1). The fix is to not move the window if we're already
    // there. (radar# 2669004)

    const PRInt32 kMoveThreshold = 2;
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
      if (sizeMode == nsSizeMode_Normal) {
        Rect newBounds;
        ::GetWindowBounds(mWindowPtr, kWindowGlobalPortRgn, &newBounds);
        ::SetWindowUserState(mWindowPtr, &newBounds);
      }
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
  nsresult rv = NS_OK;

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
    PRInt32 previousMode;
    mZooming = PR_TRUE;

    nsBaseWidget::GetSizeMode(&previousMode);
    rv = nsBaseWidget::SetSizeMode(aMode);
    if (NS_SUCCEEDED(rv)) {
      if (aMode == nsSizeMode_Minimized) {
        ::CollapseWindow(mWindowPtr, true);
      } else {
        if (aMode == nsSizeMode_Maximized) {
          CalculateAndSetZoomedSize();
          ::ZoomWindow(mWindowPtr, inZoomOut, ::FrontWindow() == mWindowPtr);
        } else {
          // Only zoom in if the previous state was Maximized
          if (previousMode == nsSizeMode_Maximized)
            ::ZoomWindow(mWindowPtr, inZoomIn, ::FrontWindow() == mWindowPtr);
        }

        Rect macRect;
        ::GetWindowPortBounds(mWindowPtr, &macRect);
        Resize(macRect.right - macRect.left, macRect.bottom - macRect.top, PR_FALSE);
      }
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
  return Resize(aWidth, aHeight, aRepaint, PR_FALSE); // resize not from UI
}

nsresult nsMacWindow::Resize(PRInt32 aWidth, PRInt32 aHeight, PRBool aRepaint, PRBool aFromUI)
{
  if (mWindowMadeHere) {
#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_3
    if (mInUpdate && nsToolkit::OSXVersion() < MAC_OS_X_VERSION_10_3_HEX) {
      // Calling SizeWindow in the middle of an update will hang on 10.2,
      // even if care is taken to break out of the update with EndUpdate.
      // On 10.2, defer the resize until the update is done.  Update will
      // call back into Resize with these values after the update is
      // finished.
      mResizeTo.width = aWidth;
      mResizeTo.height = aHeight;
      mResizeTo.repaint = aRepaint;
      mResizeTo.fromUI = aFromUI;

      mNeedsResize = PR_TRUE;

      return NS_OK;
    }
#endif

    Rect windowRect;
    if (::GetWindowBounds(mWindowPtr, kWindowContentRgn, &windowRect)
        != noErr) {
      NS_ERROR("::GetWindowBounds() didn't get window bounds");
      return NS_ERROR_FAILURE;
    }

    if (!aFromUI) {
      // If the resize came from the UI, trust the system's constraints.
      // If it came from the application, make sure it's rational.
      // @@@mm the ultimate goal is to separate UI, chrome, and content,
      // and enforce different restrictions in each case.
      Rect desktopRect;
      if (NS_FAILED(GetDesktopRect(&desktopRect))) {
        return NS_ERROR_FAILURE;
      }

#if 1
      // Use the desktop's dimensions as the maximum size without taking
      // the window's position into account.  This is the code that will
      // eventually be used as a sanity check on chrome callers, which are
      // trusted to be able to resize windows to arbitrary sizes regardless
      // of their position on screen.  Since chrome and content can't be
      // separated here yet, use this less restrictive approach for both,
      // otherwise chrome would break.
      //
      // The maximum window dimensions are the desktop area, but if the
      // window is already larger than the desktop, don't force it to
      // shrink.  This doesn't take the window's origin into account,
      // but the window won't be able to expand so that it can't be
      // displayed entirely onscreen without being obscured by the dock
      // (unless the user already made it larger with UI resizing.)
      // Use mBoundsOffset to account for the difference between window
      // structure and content (like the title bar).
      short maxWidth  = PR_MAX(desktopRect.right  - desktopRect.left -
                                 mBoundsOffset.h,
                               windowRect.right   - windowRect.left);
      short maxHeight = PR_MAX(desktopRect.bottom - desktopRect.top  -
                                 mBoundsOffset.v,
                               windowRect.bottom  - windowRect.top);
#else
      // This will eventually become the branch that is used for resizes
      // that come from content.  It's fairly restrictive and takes the
      // window's current position into account.
      //
      // Consider the window's position when computing the maximum size.
      // The goal is to use this on resizes that come from content, but
      // it's currently not possible to distinguish between resizes that
      // come from chrome and content.
      //
      // Use PR_MAX to pick the greatest (top,left) point.  If the window's
      // left edge is to the right of the desktop's left edge, the maximum
      // width needs to be smaller than the width of the desktop to avoid
      // resizing the window past the desktop during this operation.  If
      // the window's left edge is left of the desktop's left edge (offscreen),
      // use the desktop's left edge to ensure that the window, if moved,
      // will fit on the desktop.  The max of the desktop and window right
      // edges are used so that windows already offscreen aren't forced to
      // shrink during a resize.
      short maxWidth = PR_MAX(desktopRect.right, windowRect.right) -
                       PR_MAX(desktopRect.left, windowRect.left);

      // There shouldn't be any way for the window top to be above the desktop
      // top, but paranoia dictates that the same check should be done for
      // height as width.
      short maxHeight = PR_MAX(desktopRect.bottom, windowRect.bottom) -
                        PR_MAX(desktopRect.top, windowRect.top);
#endif

      aWidth = PR_MIN(aWidth, maxWidth);
      aHeight = PR_MIN(aHeight, maxHeight);
    } // !aFromUI

    short currentWidth = windowRect.right - windowRect.left;
    short currentHeight = windowRect.bottom - windowRect.top;

    if (currentWidth != aWidth || currentHeight != aHeight) {
      if (aWidth != 0 && aHeight != 0) {
        // make sure that we don't infinitely recurse if live-resize is on
        mResizeIsFromUs = PR_TRUE;
        
        // If we're in the middle of a BeginUpdate/EndUpdate pair, SizeWindow
        // causes bad painting thereafter, because the EndUpdate restores a stale
        // visRgn on the window. So we have to temporarily break out of the
        // BeginUpdate/EndUpdate state.
        StRegionFromPool savedUpdateRgn;
        PRBool wasInUpdate = mInUpdate;
        if (mInUpdate)
        {
          // Get the visRgn (which, if we're inside BeginUpdate/EndUpdate,
          // is the intersection of the window visible region, and the
          // update region).
          ::GetPortVisibleRegion(::GetWindowPort(mWindowPtr), savedUpdateRgn);
          ::EndUpdate(mWindowPtr);
        }
          
        ::SizeWindow(mWindowPtr, aWidth, aHeight, aRepaint);

        if (wasInUpdate)
        {
          // restore the update region (note that is redundant
          // if aRepaint is true, because the SizeWindow will cause a full-window
          // invalidate anyway).
          ::InvalWindowRgn(mWindowPtr, savedUpdateRgn);
          ::BeginUpdate(mWindowPtr);
        }

        // update userstate to match, if appropriate
        PRInt32 sizeMode;
        GetSizeMode(&sizeMode);
        if (sizeMode == nsSizeMode_Normal) {
          Rect portBounds;
          ::GetWindowBounds(mWindowPtr, kWindowGlobalPortRgn, &portBounds);
          ::SetWindowUserState(mWindowPtr, &portBounds);
        }

        mResizeIsFromUs = PR_FALSE;
      } else {
        // If width or height are zero, then ::SizeWindow() has no effect. So
        // instead we just hide the window by calling Show(false), which sets
        // mVisible to false. But the window is still considered to be 'visible'
        // so we set that back to true.
        if (mVisible) {
          Show(PR_FALSE);
          mVisible = PR_TRUE;
        }
      }
    }
  } // mWindowMadeHere
  Inherited::Resize(aWidth, aHeight, aRepaint);

  // Make window visible.  Show() will not make a window visible if mBounds are
  // still empty.  So when resizing a window, we check if it is supposed to be
  // visible but has yet to be made so.  This needs to be called after
  // Inherited::Resize(), since that function sets mBounds.
  if (aWidth != 0 && aHeight != 0 && mVisible && !mShown)
    Show(PR_TRUE);

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
  CFStringRef labelRef = ::CFStringCreateWithCharacters(kCFAllocatorDefault, (UniChar*)strTitle, aTitle.Length());
  if (labelRef) {
    ::SetWindowTitleWithCFString(mWindowPtr, labelRef);
    ::CFRelease(labelRef);
  }

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
  // ignore script and language information for now. 
  nsresult res = mMacEventHandler->UnicodeHandleUpdateInputArea(buffer.get(), buffer.Length(), fixLen, (TextRangeArray*) hiliteRng);
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
  // ignore language information for now. 
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
  NS_ENSURE_TRUE(mMacEventHandler.get(), NS_ERROR_FAILURE);
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
  NS_ENSURE_TRUE(mMacEventHandler.get(), NS_ERROR_FAILURE);
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
// is passed in |aMessage| and is passed along to our event handler so Gecko
// knows about it.
//
NS_IMETHODIMP
nsMacWindow::DragEvent(PRUint32 aMessage, PRInt16 aMouseGlobalX, PRInt16 aMouseGlobalY,
                         PRUint16 aKeyModifiers, PRBool *_retval)
{
  *_retval = PR_FALSE;
  NS_ENSURE_TRUE(mMacEventHandler.get(), NS_ERROR_FAILURE);
  Point globalPoint = {aMouseGlobalY, aMouseGlobalX};         // QD Point stored as v, h
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
  NS_ENSURE_TRUE(mMacEventHandler.get(), NS_ERROR_FAILURE);
  Point localPoint = {aMouseLocalY, aMouseLocalX};
  *_retval = mMacEventHandler->Scroll(aVertical ? kEventMouseWheelAxisY : kEventMouseWheelAxisX,
                                      aNumLines, localPoint, this, 0);
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
  nsZLevelEvent event(PR_TRUE, NS_SETZLEVEL, this);

  event.refPoint.x = mBounds.x;
  event.refPoint.y = mBounds.y;
  event.time = PR_IntervalNow();

  event.mImmediate = PR_TRUE;

  DispatchWindowEvent(event);
  
  return NS_OK;
}


NS_IMETHODIMP nsMacWindow::ResetInputState()
{
  NS_ENSURE_TRUE(mMacEventHandler.get(), NS_ERROR_FAILURE);
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

//
// GetDesktopRect
//
// Determines a suitable desktop size for this window.  The desktop
// size can be used to restrict resizes and moves.
//
// If a window already lies on a single screen (possibly with portions
// beyond the visible desktop shown on any screen), then the containing
// screen's bounds are the limiting factor.  If a window lies on multiple
// multiple screens (or the resize comes from the UI as opposed to a
// script), the rect encompassing all screens is the maximum limit.
// That rect (screen size or desktop size) will be put into
// desktopRect.  If the menu bar is at the top edge of desktopRect, or
// the dock is at any other edge of desktopRect, they will be clipped out
// of desktopRect, because scripted resizes should not be able to make
// windows that don't fit in the visible area.
typedef enum {
  kDockOrientationNone       = 0,
  kDockOrientationHorizontal = 1,
  kDockOrientationVertical   = 2
} nsMacDockOrientation;

nsresult nsMacWindow::GetDesktopRect(Rect* desktopRect)
{
  // This is how to figure out where the dock and menu bar are, in global
  // coordinates, when there seems to be no way to get that information
  // directly from the system.  Compare the real dimensions of each screen
  // with the dimensions of the screen less the space used for the dock (as
  // given by GetAvailableWindowPositioningBounds) and apply deductive
  // reasoning.  While doing that, it's a convenient time to add each
  // screen's full rect to the union in desktopRect.

  ::SetRect(desktopRect, 0, 0, 0, 0);
  Rect menuBarRect = {0, 0, 0, 0};
  Rect dockRect = {0, 0, 0, 0};

  Rect windowRect;
  if (::GetWindowBounds(mWindowPtr, kWindowStructureRgn, &windowRect)
      != noErr) {
    NS_ERROR("::GetWindowBounds() didn't get window bounds");
    return NS_ERROR_FAILURE;
  }

  Rect tempRect; // for various short-lived purposes, such as holding
                 // unimportant overlap areas in ::SectRect calls

  PRUint32 windowOnScreens = 0; // number of screens containing window
  Rect windowOnScreenRect = {0, 0, 0, 0}; // only valid if windowOnScreens==1
  GDHandle screen = ::DMGetFirstScreenDevice(TRUE);
  nsMacDockOrientation dockOrientation = kDockOrientationNone;
  while (screen != NULL) {
    Rect screenRect = (*screen)->gdRect;
    if (::EmptyRect(&screenRect)) {
      NS_WARNING("Couldn't determine screen dimensions");
      continue;
    }

    ::UnionRect(desktopRect, &screenRect, desktopRect);

    if (::SectRect(&screenRect, &windowRect, &tempRect)) {
      // The window is at least partially on this screen.  Set
      // windowOnScreenRect to the bounds of this screen.
      // If, after checking any other screens, the window is only on one
      // screen, then windowOnScreenRect will be used as the bounds,
      // limiting the window to the screen that contains it.
      windowOnScreens++;
      windowOnScreenRect = screenRect;
    }

    // GetAvailableWindowPositioningBounds clips out the menu bar
    // and the dock.  Since the menu bar is clipped from the top,
    // and the dock can't be at the top, this suffices.
    Rect availableRect;
    if (::GetAvailableWindowPositioningBounds(screen, &availableRect)
        != noErr) {
      NS_ERROR("::GetAvailableWindowPositioningBounds couldn't determine" \
               "available screen dimensions");
      return NS_ERROR_FAILURE;
    }

    // The menu bar is at the top of the primary display.  Its bounds
    // could be retrieved by looking at the main display outside of the
    // screen iterator loop.
    if (availableRect.top > screenRect.top) {
      ::SetRect(&menuBarRect, screenRect.left, screenRect.top,
                              screenRect.right, availableRect.top);
    }

    // The dock, if it's not hidden, will be at the bottom, right,
    // or left edge of a screen.  Look for it on this screen.  If
    // it's found, set up dockRect to extend from the extremity of
    // availableRect to the extremity of screenRect.
    //
    // It seems that even if the dock is hidden, the system reserves
    // 4px at the screen edge for it.
    if (availableRect.bottom < screenRect.bottom) {
      // Dock on bottom
      ::SetRect(&dockRect, availableRect.left, availableRect.bottom,
                           availableRect.right, screenRect.bottom);
      dockOrientation = kDockOrientationHorizontal;
    }
    else if (availableRect.right < screenRect.right) {
      // Dock on right
      ::SetRect(&dockRect, availableRect.right, availableRect.top,
                           screenRect.right, availableRect.bottom);
      dockOrientation = kDockOrientationVertical;
    }
    else if (availableRect.left > screenRect.left) {
      // Dock on left
      ::SetRect(&dockRect, screenRect.left, availableRect.top,
                           availableRect.left, availableRect.bottom);
      dockOrientation = kDockOrientationVertical;
    }

    screen = ::DMGetNextScreenDevice(screen, TRUE);
  }

  if (windowOnScreens == 1) {
    // The window is contained entirely on a single screen.  It may actually
    // be partly offscreen, the key is that no part of the window is on a
    // screen other than the one with screen bounds in windowOnScreenRect.
    // Use that screen's bounds.
    //
    // It's tempting to save the screen's available positioning bounds and
    // return early here, but we still need to check to see if the window is
    // under the dock already, and permit it to remain there if so.
    //
    // Keep in mind that if the resize came from the UI as opposed to a
    // script, the window's dimensions will already appear to exceed the
    // screen it had formerly been contained within if the user has resized
    // it to cover multiple screens.  In other words, this won't constrain
    // UI resizes to a single screen.  That's perfect, because we only
    // want to police script resizing.
    *desktopRect = windowOnScreenRect;
  }

  if (::EmptyRect(desktopRect)) {
    NS_ERROR("Couldn't determine desktop dimensions\n");
    return NS_ERROR_FAILURE;
  }

  // *desktopRect is now the smallest rectangle enclosing all of
  // the screens, or if the window is on a single screen, the rectangle
  // corresponding to the screen itself, without any areas clipped out for
  // the menu bar or dock.

  // Clip where appropriate.

  if (::SectRect(desktopRect, &menuBarRect, &tempRect) &&
      menuBarRect.top == desktopRect->top) {
    // The menu bar is in the desktop rect at the very top.  Adjust the
    // top of the desktop to clip out the menu bar.
    // menuBarRect.bottom is the height of the menu bar because
    // menuBarRect.top is 0, so there's no need to subtract top
    // from bottom.  When using multiple monitors, the menu bar
    // might not be at the desktop top - in that case, don't clip it.
    desktopRect->top += menuBarRect.bottom;
  }

  if (dockOrientation != kDockOrientationNone &&
      ::SectRect(desktopRect, &dockRect, &tempRect)) {
    // There's a dock and it's somewhere in the desktop rect.

    // Determine the orientation of the dock before checking to see if
    // it's on an edge, so that the proper edge can be clipped.  If
    // the dock is on an edge corresponding to its orientation, clip
    // the dock out of the available desktop.
    switch (dockOrientation) {
      case kDockOrientationVertical:
        // The dock is oriented vertically, so check the left and right
        // edges.
        if (dockRect.left == desktopRect->left) {
          desktopRect->left += dockRect.right - dockRect.left;
        }
        else if (dockRect.right == desktopRect->right) {
          desktopRect->right -= dockRect.right - dockRect.left;
        }
        break;

      case kDockOrientationHorizontal:
        // The dock is oriented horizontally, so check the bottom edge.
        // The dock can't be at the top edge.
        if (dockRect.bottom == desktopRect->bottom) {
          desktopRect->bottom -= dockRect.bottom - dockRect.top;
        }
        break;

      default:
        break;
    } // switch
  } // dockRect overlaps *desktopRect

  // *desktopRect is now clear of the menu bar and dock, if they needed
  // to be cleared.
  return NS_OK;
}

//
// GetChildSheet
//
// If aShown is true, returns the child widget associated with the sheet
// attached to this nsMacWindow, or nsnull if none.
//
// If aShown is false, returns the first child widget of this nsMacWindow
// corresponding to a sheet that wishes to be displayed, or nsnull if no
// sheets are waiting to be shown.
//
NS_IMETHODIMP nsMacWindow::GetChildSheet(PRBool aShown, nsMacWindow** _retval)
{
  nsIWidget* child = GetFirstChild();

  while (child) {
    // nsWindow is the base widget in the Carbon widget library, this is safe
    nsWindow* window = NS_STATIC_CAST(nsWindow*, child);

    nsWindowType type;
    if (NS_SUCCEEDED(window->GetWindowType(type)) &&
        type == eWindowType_sheet) {
      // if it's a sheet, it must be an nsMacWindow
      nsMacWindow* macWindow = NS_STATIC_CAST(nsMacWindow*, window);

      if ((aShown && macWindow->mShown) ||
          (!aShown && macWindow->mSheetNeedsShow)) {
        *_retval = macWindow;
        return NS_OK;
      }
    }

    child = child->GetNextSibling();
  }

  *_retval = nsnull;
  return NS_OK;
}

NS_IMETHODIMP nsMacWindow::Update()
{
  nsresult rv = Inherited::Update();

#if MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_X_VERSION_10_3
  if (NS_SUCCEEDED(rv) && mNeedsResize) {
    // Something wanted to resize this window in the middle of the update
    // above, and the host OS is 10.2.  The resize would have caused a
    // hang in SizeWindow, so it was deferred.  Now that the update is
    // done, it's safe to resize.
    mNeedsResize = PR_FALSE;

    Resize(mResizeTo.width, mResizeTo.height,
           mResizeTo.repaint, mResizeTo.fromUI);
  }
#endif

  return rv;
}

pascal OSStatus
nsMacWindow::KeyEventHandler(EventHandlerCallRef aHandlerCallRef,
                             EventRef            aEvent,
                             void*               aUserData)
{
  nsMacWindow* self = NS_STATIC_CAST(nsMacWindow*, aUserData);
  NS_ASSERTION(self, "No self?");
  NS_ASSERTION(self->mMacEventHandler.get(), "No mMacEventHandler?");

  PRBool handled = self->mMacEventHandler->HandleKeyUpDownEvent(aHandlerCallRef,
                                                                aEvent);

  if (!handled)
    return eventNotHandledErr;

  return noErr;
}

NS_IMETHODIMP
nsMacWindow::GetEventDispatchHandler(nsMacEventDispatchHandler** aEventDispatchHandler) {
  *aEventDispatchHandler = mMacEventHandler->GetEventDispatchHandler();
  return NS_OK;
}
