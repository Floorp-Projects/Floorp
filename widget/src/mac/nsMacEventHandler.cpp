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

#include "nsMacEventHandler.h"

#include "nsWindow.h"
#include "nsMacWindow.h"
#include "nsCRT.h"
#include "prinrval.h"

#include <ToolUtils.h>
#include <TextServices.h>
#include <UnicodeConverter.h>

#include "nsCarbonHelpers.h"
#include "nsIRollupListener.h"
#include "nsIMenuRollup.h"
#include "nsTSMStrategy.h"
#include "nsGfxUtils.h"

#ifndef XP_MACOSX
#include <locale>
#endif

#define PINK_PROFILING_ACTIVATE 0
#if PINK_PROFILING_ACTIVATE
#include "profilerutils.h"
#endif

//#define DEBUG_TSM
extern nsIRollupListener * gRollupListener;
extern nsIWidget         * gRollupWidget;

#if PINK_PROFILING_ACTIVATE
static Boolean KeyDown(const UInt8 theKey);
static Boolean KeyDown(const UInt8 theKey)
{
	KeyMap map;
	GetKeys(map);
	return ((*((UInt8 *)map + (theKey >> 3)) >> (theKey & 7)) & 1) != 0;
}
#endif


// from MacHeaders.c
#ifndef topLeft
	#define topLeft(r)	(((Point *) &(r))[0])
#endif
#ifndef botRight
	#define botRight(r)	(((Point *) &(r))[1])
#endif

PRBool	nsMacEventHandler::sInBackground = PR_FALSE;
PRBool	nsMacEventHandler::sMouseInWidgetHit = PR_FALSE;

nsMacEventDispatchHandler	gEventDispatchHandler;

static nsEventStatus HandleScrollEvent ( EventMouseWheelAxis inAxis, PRBool inByLine, PRInt32 inDelta,
                                          Point inMouseLoc, nsIWidget* inWidget ) ;
static void ConvertKeyEventToContextMenuEvent(const nsKeyEvent* inKeyEvent, nsMouseEvent* outCMEvent);
static inline PRBool IsContextMenuKey(const nsKeyEvent& inKeyEvent);


#if !TARGET_CARBON
//
// ScrollActionProc
//
// Called from ::TrackControl(), this senses which part of the phantom
// scrollbar the click from the wheelMouse driver was in and sends
// the correct NS_MOUSE_SCROLL event into Gecko. We have to retrieve the
// mouse location from the event dispatcher because it will
// just be the location of the phantom scrollbar, not actually the real
// mouse position.
//
static pascal void ScrollActionProc(ControlHandle ctrl, ControlPartCode partCode)
{
	switch (partCode)
	{
		case kControlUpButtonPart:
		case kControlDownButtonPart:
		case kControlPageUpPart:
		case kControlPageDownPart:
		  PhantomScrollbarData* data = NS_REINTERPRET_CAST(PhantomScrollbarData*, ::GetControlReference(ctrl));
		  if ( data && (data->mWidgetToGetEvent || gEventDispatchHandler.GetActive()) ) {
		    WindowRef window = (**ctrl).contrlOwner;
        StPortSetter portSetter(window);
        StOriginSetter originSetter(window);
        PRBool scrollByLine = !(partCode == kControlPageUpPart || partCode == kControlPageDownPart);
        PRInt32 delta = 
          (partCode == kControlUpButtonPart || partCode == kControlPageUpPart) ? -1 : 1;
      	nsIWidget* widget = data->mWidgetToGetEvent ? 
                              data->mWidgetToGetEvent : gEventDispatchHandler.GetActive();
        
        Point thePoint = gEventDispatchHandler.GetGlobalPoint();
        ::GlobalToLocal(&thePoint);
        HandleScrollEvent ( kEventMouseWheelAxisY, scrollByLine, delta, thePoint, widget );
      }
      break;
  }
}
#endif


//
// HandleScrollEvent
//
// Actually dispatch the mouseWheel scroll event to the appropriate widget. If |inByLine| is false,
// then scroll by a full page. |inMouseLoc| is in OS local coordinates. We convert it to widget-relative
// coordinates before sending it into Gecko.
//
static nsEventStatus
HandleScrollEvent ( EventMouseWheelAxis inAxis, PRBool inByLine, PRInt32 inDelta,
                     Point inMouseLoc, nsIWidget* inWidget )
{
  NS_ASSERTION(inWidget, "HandleScrollEvent doesn't work with a null widget");
  if (!inWidget)
    return nsEventStatus_eIgnore;

  nsMouseScrollEvent scrollEvent(NS_MOUSE_SCROLL, inWidget);
  
  scrollEvent.scrollFlags = 
    (inAxis == kEventMouseWheelAxisX) ? nsMouseScrollEvent::kIsHorizontal : nsMouseScrollEvent::kIsVertical;
  if ( !inByLine )
    scrollEvent.scrollFlags |= nsMouseScrollEvent::kIsFullPage;
  
  // convert window-relative (local) mouse coordinates to widget-relative
  // coords for Gecko.
  nsPoint mouseLocRelativeToWidget(inMouseLoc.h, inMouseLoc.v);
  nsRect bounds;
  inWidget->GetBounds(bounds);
  nsPoint widgetOrigin(bounds.x, bounds.y);
  inWidget->ConvertToDeviceCoordinates(widgetOrigin.x, widgetOrigin.y);
  mouseLocRelativeToWidget.MoveBy(-widgetOrigin.x, -widgetOrigin.y);
		
  scrollEvent.delta = inDelta;
	scrollEvent.point.x = mouseLocRelativeToWidget.x;
	scrollEvent.point.y = mouseLocRelativeToWidget.y;
	scrollEvent.time = PR_IntervalNow();

  // dispatch scroll event
  nsEventStatus rv;
  scrollEvent.widget->DispatchEvent(&scrollEvent, rv);
  return rv;
  
} // HandleScrollEvent


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsMacEventDispatchHandler::nsMacEventDispatchHandler()
{
	mActiveWidget	= nsnull;
	mWidgetHit		= nsnull;
	mWidgetPointed	= nsnull;
#if TRACK_MOUSE_LOC
  mLastGlobalMouseLoc.h = mLastGlobalMouseLoc.v = 0;
#endif
}


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsMacEventDispatchHandler::~nsMacEventDispatchHandler()
{
	if (mActiveWidget)
	{
	  mActiveWidget->RemoveDeleteObserver(this);
	  mActiveWidget = nsnull;
	}

	if (mWidgetHit)
	{
	  mWidgetHit->RemoveDeleteObserver(this);
	  mWidgetHit = nsnull;
	}

	if (mWidgetPointed)
	{
	  mWidgetPointed->RemoveDeleteObserver(this);
	  mWidgetPointed = nsnull;
	}	
}


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
void nsMacEventDispatchHandler::DispatchGuiEvent(nsWindow *aWidget, PRUint32 aEventType)
{
	NS_ASSERTION(aWidget,"attempted to dispatch gui event to null widget");
	if (!aWidget)
		return;

	nsGUIEvent	guiEvent(aEventType, aWidget);
	guiEvent.time = PR_IntervalNow();
	aWidget->DispatchWindowEvent(guiEvent);
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
void nsMacEventDispatchHandler::DispatchSizeModeEvent(nsWindow *aWidget, nsSizeMode aMode)
{
	NS_ASSERTION(aWidget,"attempted to dispatch gui event to null widget");
	if (!aWidget)
		return;

	nsSizeModeEvent	event(NS_SIZEMODE, aWidget);
	event.time = PR_IntervalNow();
	event.mSizeMode = aMode;
	aWidget->DispatchWindowEvent(event);
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
void nsMacEventDispatchHandler::SetFocus(nsWindow *aFocusedWidget)
{
    // This short circut lives inside of nsEventStateManager now
	if (aFocusedWidget == mActiveWidget)
		return;
		
	// tell the old widget it is not focused
	if (mActiveWidget)
	{
		mActiveWidget->ResetInputState();
		mActiveWidget->RemoveDeleteObserver(this);
		//printf("nsMacEventDispatcher::SetFocus sends NS_LOSTFOCUS\n");
		DispatchGuiEvent(mActiveWidget, NS_LOSTFOCUS);
	}

	mActiveWidget = aFocusedWidget;

	// let the new one know it got the focus
	if (mActiveWidget)
	{
		mActiveWidget->AddDeleteObserver(this);
		//printf("nsMacEventDispatcher::SetFocus sends NS_GOTFOCUS\n");
		DispatchGuiEvent(mActiveWidget, NS_GOTFOCUS);
	}
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

void nsMacEventDispatchHandler::SetActivated(nsWindow *aActivatedWidget)
{
  //printf("nsMacEventDispatcher::SetActivated \n");
  
	if (aActivatedWidget == mActiveWidget) {
		//printf("already the active widget. Bailing!\n");
		return;
	}

  if(aActivatedWidget) {
    nsWindowType wtype;
    aActivatedWidget->GetWindowType(wtype);
    if ( eWindowType_popup == wtype ) {
      //printf("nsMacEventDispatcher::SetActivated type popup, bail\n");
      return;
    }
  }
  
	// tell the old widget it is not focused
	if (mActiveWidget)
	{
		mActiveWidget->ResetInputState();
		mActiveWidget->RemoveDeleteObserver(this);
		//printf("nsMacEventDispatchHandler::SetActivated sends NS_LOSTFOCUS\n");
		DispatchGuiEvent(mActiveWidget, NS_LOSTFOCUS);
	}

	mActiveWidget = aActivatedWidget;

	// let the new one know it got activation
	if (mActiveWidget)
	{
		mActiveWidget->AddDeleteObserver(this);
		//printf("nsMacEventDispatcher::SetActivated sends NS_GOTFOCUS\n");
		DispatchGuiEvent(mActiveWidget, NS_GOTFOCUS);
		
		//printf("nsMacEventDispatchHandler::SetActivated sends NS_ACTIVATE\n");
		DispatchGuiEvent(mActiveWidget, NS_ACTIVATE);
	}
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------

void nsMacEventDispatchHandler::SetDeactivated(nsWindow *aDeactivatedWidget)
{
    //printf("nsMacEventDispatchHandler::SetDeactivated\n");
    if(aDeactivatedWidget) {
      nsWindowType wtype;
      aDeactivatedWidget->GetWindowType(wtype);
      if ( eWindowType_popup == wtype ) {
        //printf("nsMacEventDispatchHandler::SetDeactivated type popup, bail\n");
        return;
      }
    }
      
    // If the deactivated toplevel window contains mActiveWidget, then
    // clear out mActiveWidget.

    if (mActiveWidget) {
      nsCOMPtr<nsIWidget> curWin = do_QueryInterface(NS_STATIC_CAST(nsIWidget*, mActiveWidget));
      for (;;) {
        nsCOMPtr<nsIWidget> parent = dont_AddRef(curWin->GetParent());
        if (!parent)
          break;
        curWin = parent;
      }

      if (NS_STATIC_CAST(nsWindow*, NS_STATIC_CAST(nsIWidget*, curWin)) == aDeactivatedWidget) {
        //printf("   nsMacEventDispatchHandler::SetDeactivated sends NS_DEACTIVATE\n");
        mActiveWidget->RemoveDeleteObserver(this);
        mActiveWidget = nsnull;
      }
    }

    DispatchGuiEvent(aDeactivatedWidget, NS_DEACTIVATE);	
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
void nsMacEventDispatchHandler::SetWidgetHit(nsWindow *aWidgetHit)
{
	if (aWidgetHit == mWidgetHit)
		return;

	if (mWidgetHit)
	  if (! mWidgetHit->RemoveDeleteObserver(this))
	  	NS_WARNING("nsMacFocusHandler wasn't in the WidgetHit observer list");

	mWidgetHit = aWidgetHit;

	if (mWidgetHit)
	  mWidgetHit->AddDeleteObserver(this);
}


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
void nsMacEventDispatchHandler::SetWidgetPointed(nsWindow *aWidgetPointed)
{
	if (aWidgetPointed == mWidgetPointed) {
		return;
    }
       
	if (mWidgetPointed) 
	  if (! mWidgetPointed->RemoveDeleteObserver(this)) 
	  	NS_WARNING("nsMacFocusHandler wasn't in the WidgetPointed observer list");
    
	mWidgetPointed = aWidgetPointed;
    
	if (mWidgetPointed)
	  mWidgetPointed->AddDeleteObserver(this);
}


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
void nsMacEventDispatchHandler::NotifyDelete(void* aDeletedObject)
{
	if (mActiveWidget == aDeletedObject)
		mActiveWidget = nsnull;
	
	if (mWidgetHit == aDeletedObject)
		mWidgetHit = nsnull;

	if (mWidgetPointed == aDeletedObject)
		mWidgetPointed = nsnull;

}


#if TRACK_MOUSE_LOC

void nsMacEventDispatchHandler::SetGlobalPoint(Point inPoint)
{
  mLastGlobalMouseLoc = inPoint;
}

#endif


#pragma mark -

//-------------------------------------------------------------------------
//
// nsMacEventHandler constructor/destructor
//
//-------------------------------------------------------------------------
nsMacEventHandler::nsMacEventHandler(nsMacWindow* aTopLevelWidget)
{
	OSErr	err;
	InterfaceTypeList supportedServices;
	
	mTopLevelWidget = aTopLevelWidget;
	
  nsTSMStrategy tsmstrategy;
  
	//
	// create a TSMDocument for this window.  We are allocating a TSM document for
	// each Mac window
	//
	mTSMDocument = nsnull;
  if (tsmstrategy.UseUnicodeForInputMethod()) {
    supportedServices[0] = kUnicodeDocument;
  } else {
	supportedServices[0] = kTextService;
  }
  err = ::NewTSMDocument(1, supportedServices,&mTSMDocument, (long)this);
  NS_ASSERTION(err==noErr, "nsMacEventHandler::nsMacEventHandler: NewTSMDocument failed.");
#ifdef DEBUG_TSM
  printf("nsMacEventHandler::nsMacEventHandler: created TSMDocument[%p]\n", mTSMDocument);
#endif

  // make sure we do not use input widnow even some other code turn it for default by calling 
  // ::UseInputWindow(nsnull, TRUE); 
  if (mTSMDocument)
    ::UseInputWindow(mTSMDocument, FALSE); 
         
  mIMEIsComposing = PR_FALSE;
  mIMECompositionStr = nsnull;

#if !TARGET_CARBON
    mControlActionProc = NewControlActionUPP(ScrollActionProc);
#endif
}


nsMacEventHandler::~nsMacEventHandler()
{
	if (mTSMDocument)
		(void)::DeleteTSMDocument(mTSMDocument);
	if(nsnull != mIMECompositionStr) {
		delete mIMECompositionStr;
		mIMECompositionStr = nsnull;
	}
#if !TARGET_CARBON
	if ( mControlActionProc ) {
	  DisposeControlActionUPP(mControlActionProc); 
	  mControlActionProc = nsnull;
	}
#endif
}



#pragma mark -
//-------------------------------------------------------------------------
//
// HandleOSEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleOSEvent ( EventRecord& aOSEvent )
{
	PRBool retVal = PR_FALSE;

	switch (aOSEvent.what)
	{
		case keyUp:
		case keyDown:
		case autoKey:
			retVal = HandleKeyEvent(aOSEvent);
			break;

		case activateEvt:
			retVal = HandleActivateEvent(aOSEvent);
			break;

		case updateEvt:
			retVal = UpdateEvent();
			break;

		case mouseDown:
			retVal = HandleMouseDownEvent(aOSEvent);
			break;

		case mouseUp:
			retVal = HandleMouseUpEvent(aOSEvent);
			break;

		case osEvt:
		{
			unsigned char eventType = ((aOSEvent.message >> 24) & 0x00ff);
			if (eventType == suspendResumeMessage)
			{
				if ((aOSEvent.message & 1) == resumeFlag) {
					sInBackground = PR_FALSE;		// resume message
				} else {
					sInBackground = PR_TRUE;		// suspend message
					if (nsnull != gRollupListener && (nsnull != gRollupWidget) ) {
						gRollupListener->Rollup();
					}
				}
				HandleActivateEvent(aOSEvent);
			}
			else if (eventType == mouseMovedMessage)
			{
				if (! sInBackground)
					retVal = HandleMouseMoveEvent(aOSEvent);
			}
		}
		break;
	
		case nullEvent:
			if (! sInBackground)
				retVal = HandleMouseMoveEvent(aOSEvent);
			break;
	}

	return retVal;
}


#if USE_MENUSELECT

//-------------------------------------------------------------------------
//
// Handle Menu commands
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleMenuCommand(
  EventRecord& aOSEvent,
  long         aMenuResult)
{
	// get the focused widget
	nsWindow* focusedWidget = gEventDispatchHandler.GetActive();
	if (!focusedWidget)
		focusedWidget = mTopLevelWidget;

	// nsEvent
	nsMenuEvent menuEvent(NS_MENU_SELECTED, focusedWidget);
	menuEvent.point.x		= aOSEvent.where.h;
	menuEvent.point.y		= aOSEvent.where.v;
	menuEvent.time			= PR_IntervalNow();

	// nsGUIEvent
	menuEvent.nativeMsg	= (void*)&aOSEvent;

	// nsMenuEvent
  // TODO: initialize mMenuItem
	menuEvent.mCommand	= aMenuResult;

	// dispatch the menu event
	PRBool eventHandled = focusedWidget->DispatchWindowEvent(menuEvent);
	
	// if the menu event is not processed by the focused widget, propagate it
	// through the different parents all the way up to the top-level window
	if (! eventHandled)
	{
		// make sure the focusedWidget wasn't changed or deleted
		// when we dispatched the event (even though if we get here, 
		// the event is supposed to not have been handled)
		if (focusedWidget == gEventDispatchHandler.GetActive())
		{
			nsCOMPtr<nsIWidget> grandParent;
			nsCOMPtr<nsIWidget> parent ( dont_AddRef(focusedWidget->GetParent()) );
			while (parent)
			{
				menuEvent.widget = parent;
				eventHandled = (static_cast<nsWindow*>(static_cast<nsIWidget*>(parent)))->DispatchWindowEvent(menuEvent);
				if (eventHandled)
				{
					break;
				}
				else
				{
					grandParent = dont_AddRef(parent->GetParent());
					parent = grandParent;
				}
			}
		}
	}

	return eventHandled;
}

#endif

//-------------------------------------------------------------------------
//
// DragEvent
//
//-------------------------------------------------------------------------
//
// Someone on the outside told us that something related to a drag is happening. The
// exact event type is passed in as |aMessage|. We need to send this event into Gecko 
// for processing. Create a Gecko event (using the appropriate message type) and pass
// it along.
//
// ÄÄÄTHIS REALLY NEEDS TO BE CLEANED UP! TOO MUCH CODE COPIED FROM ConvertOSEventToMouseEvent
//
PRBool nsMacEventHandler::DragEvent ( unsigned int aMessage, Point aMouseGlobal, UInt16 aKeyModifiers )
{
	// convert the mouse to local coordinates. We have to do all the funny port origin
	// stuff just in case it has been changed.
	Point hitPointLocal = aMouseGlobal;
	WindowRef wind = reinterpret_cast<WindowRef>(mTopLevelWidget->GetNativeData(NS_NATIVE_DISPLAY));
	nsGraphicsUtils::SafeSetPortWindowPort(wind);
	{
	    StOriginSetter  originSetter(wind);
    	::GlobalToLocal(&hitPointLocal);
	}
	nsPoint widgetHitPoint(hitPointLocal.h, hitPointLocal.v);

	nsWindow* widgetHit = mTopLevelWidget->FindWidgetHit(hitPointLocal);
	if ( widgetHit ) {
		// adjust from local coordinates to window coordinates in case the hit widget
		// isn't at 0, 0
		nsRect bounds;
		widgetHit->GetBounds(bounds);
		nsPoint widgetOrigin(bounds.x, bounds.y);
		widgetHit->LocalToWindowCoordinate(widgetOrigin);
		widgetHitPoint.MoveBy(-widgetOrigin.x, -widgetOrigin.y);		
	}
	else {
	  // this is most likely the case of a drag exit, so we need to make sure
	  // we send the event to the last pointed to widget. We don't really care
	  // about the mouse coordinates because we know they're outside the window.
	  widgetHit = gEventDispatchHandler.GetWidgetPointed();
	  widgetHitPoint = nsPoint(0,0);
	}	

	// update the tracking of which widget the mouse is now over.
	gEventDispatchHandler.SetWidgetPointed(widgetHit);
	
	nsMouseEvent geckoEvent(aMessage, widgetHit);

	// nsEvent
	geckoEvent.point = widgetHitPoint;
	geckoEvent.time	= PR_IntervalNow();

	// nsInputEvent
	geckoEvent.isShift = ((aKeyModifiers & shiftKey) != 0);
	geckoEvent.isControl = ((aKeyModifiers & controlKey) != 0);
	geckoEvent.isAlt = ((aKeyModifiers & optionKey) != 0);
	geckoEvent.isMeta = ((aKeyModifiers & cmdKey) != 0);

	// nsMouseEvent
	geckoEvent.clickCount = 1;
	
	if ( widgetHit )
		widgetHit->DispatchMouseEvent(geckoEvent);
	else
		NS_WARNING ("Oh shit, no widget to dispatch event to, we're in trouble" );
	
	return PR_TRUE;
	
} // DropOccurred


#pragma mark -

//-------------------------------------------------------------------------
//
// ConvertMacToRaptorKeyCode
//
//-------------------------------------------------------------------------


// Key code constants
enum
{
	kEscapeKeyCode			= 0x35,
	kCommandKeyCode     = 0x37,
	kShiftKeyCode				= 0x38,
	kCapsLockKeyCode		= 0x39,
	kControlKeyCode			= 0x3B,
	kOptionkeyCode			= 0x3A,		// left and right option keys
	kClearKeyCode				= 0x47,
	
	// function keys
	kF1KeyCode					= 0x7A,
	kF2KeyCode					= 0x78,
	kF3KeyCode					= 0x63,
	kF4KeyCode					= 0x76,
	kF5KeyCode					= 0x60,
	kF6KeyCode					= 0x61,
	kF7KeyCode					= 0x62,
	kF8KeyCode					= 0x64,
	kF9KeyCode					= 0x65,
	kF10KeyCode					= 0x6D,
	kF11KeyCode					= 0x67,
	kF12KeyCode					= 0x6F,
	kF13KeyCode					= 0x69,
	kF14KeyCode					= 0x6B,
	kF15KeyCode					= 0x71,
	
	kPrintScreenKeyCode	= kF13KeyCode,
	kScrollLockKeyCode	= kF14KeyCode,
	kPauseKeyCode				= kF15KeyCode,
	
	// keypad
	kKeypad0KeyCode			= 0x52,
	kKeypad1KeyCode			= 0x53,
	kKeypad2KeyCode			= 0x54,
	kKeypad3KeyCode			= 0x55,
	kKeypad4KeyCode			= 0x56,
	kKeypad5KeyCode			= 0x57,
	kKeypad6KeyCode			= 0x58,
	kKeypad7KeyCode			= 0x59,
	kKeypad8KeyCode			= 0x5B,
	kKeypad9KeyCode			= 0x5C,
	
	kKeypadMultiplyKeyCode	= 0x43,
	kKeypadAddKeyCode				= 0x45,
	kKeypadSubtractKeyCode	= 0x4E,
	kKeypadDecimalKeyCode		= 0x41,
	kKeypadDivideKeyCode		= 0x4B,
	kKeypadEqualsKeyCode		= 0x51,			// no correpsonding raptor key code
	kEnterKeyCode           = 0x4C,
	kReturnKeyCode          = 0x24,
	kPowerbookEnterKeyCode  = 0x34,     // Enter on Powerbook's keyboard is different
	
	kHelpKeyCode						= 0x72,				// also insert key
	kDeleteKeyCode					= 0x75,				// also forward delete key
	kTabKeyCode							= 0x30,
	kBackspaceKeyCode       = 0x33,
	kHomeKeyCode						= 0x73,	
	kEndKeyCode							= 0x77,
	kPageUpKeyCode					= 0x74,
	kPageDownKeyCode				= 0x79,
	kLeftArrowKeyCode				= 0x7B,
	kRightArrowKeyCode			= 0x7C,
	kUpArrowKeyCode					= 0x7E,
	kDownArrowKeyCode				= 0x7D
	
};

static PRUint32 ConvertMacToRaptorKeyCode(UInt32 eventMessage, UInt32 eventModifiers)
{
	UInt8			charCode = (eventMessage & charCodeMask);
	UInt8			keyCode = (eventMessage & keyCodeMask) >> 8;
	PRUint32	raptorKeyCode = 0;
	
	switch (keyCode)
	{
//  case ??            :       raptorKeyCode = nsIDOMKeyEvent::DOM_VK_CANCEL;    break;     // don't know what this key means. Nor does joki

// modifiers. We don't get separate events for these
    case kShiftKeyCode:         raptorKeyCode = nsIDOMKeyEvent::DOM_VK_SHIFT;         break;
    case kCommandKeyCode:       raptorKeyCode = nsIDOMKeyEvent::DOM_VK_META;          break;
    case kCapsLockKeyCode:      raptorKeyCode = nsIDOMKeyEvent::DOM_VK_CAPS_LOCK;     break;
    case kControlKeyCode:       raptorKeyCode = nsIDOMKeyEvent::DOM_VK_CONTROL;       break;
    case kOptionkeyCode:        raptorKeyCode = nsIDOMKeyEvent::DOM_VK_ALT;           break;

// function keys
    case kF1KeyCode:            raptorKeyCode = nsIDOMKeyEvent::DOM_VK_F1;            break;
    case kF2KeyCode:            raptorKeyCode = nsIDOMKeyEvent::DOM_VK_F2;            break;
    case kF3KeyCode:            raptorKeyCode = nsIDOMKeyEvent::DOM_VK_F3;            break;
    case kF4KeyCode:            raptorKeyCode = nsIDOMKeyEvent::DOM_VK_F4;            break;
    case kF5KeyCode:            raptorKeyCode = nsIDOMKeyEvent::DOM_VK_F5;            break;
    case kF6KeyCode:            raptorKeyCode = nsIDOMKeyEvent::DOM_VK_F6;            break;
    case kF7KeyCode:            raptorKeyCode = nsIDOMKeyEvent::DOM_VK_F7;            break;
    case kF8KeyCode:            raptorKeyCode = nsIDOMKeyEvent::DOM_VK_F8;            break;
    case kF9KeyCode:            raptorKeyCode = nsIDOMKeyEvent::DOM_VK_F9;            break;
    case kF10KeyCode:           raptorKeyCode = nsIDOMKeyEvent::DOM_VK_F10;           break;
    case kF11KeyCode:           raptorKeyCode = nsIDOMKeyEvent::DOM_VK_F11;           break;
    case kF12KeyCode:           raptorKeyCode = nsIDOMKeyEvent::DOM_VK_F12;           break;
//  case kF13KeyCode:           raptorKeyCode = nsIDOMKeyEvent::DOM_VK_F13;           break;    // clash with the 3 below
//  case kF14KeyCode:           raptorKeyCode = nsIDOMKeyEvent::DOM_VK_F14;           break;
//  case kF15KeyCode:           raptorKeyCode = nsIDOMKeyEvent::DOM_VK_F15;           break;
    case kPauseKeyCode:         raptorKeyCode = nsIDOMKeyEvent::DOM_VK_PAUSE;         break;
    case kScrollLockKeyCode:    raptorKeyCode = nsIDOMKeyEvent::DOM_VK_SCROLL_LOCK;   break;
    case kPrintScreenKeyCode:   raptorKeyCode = nsIDOMKeyEvent::DOM_VK_PRINTSCREEN;   break;

// keypad
    case kKeypad0KeyCode:        raptorKeyCode = nsIDOMKeyEvent::DOM_VK_NUMPAD0;      break;
    case kKeypad1KeyCode:        raptorKeyCode = nsIDOMKeyEvent::DOM_VK_NUMPAD1;      break;
    case kKeypad2KeyCode:        raptorKeyCode = nsIDOMKeyEvent::DOM_VK_NUMPAD2;      break;
    case kKeypad3KeyCode:        raptorKeyCode = nsIDOMKeyEvent::DOM_VK_NUMPAD3;      break;
    case kKeypad4KeyCode:        raptorKeyCode = nsIDOMKeyEvent::DOM_VK_NUMPAD4;      break;
    case kKeypad5KeyCode:        raptorKeyCode = nsIDOMKeyEvent::DOM_VK_NUMPAD5;      break;
    case kKeypad6KeyCode:        raptorKeyCode = nsIDOMKeyEvent::DOM_VK_NUMPAD6;      break;
    case kKeypad7KeyCode:        raptorKeyCode = nsIDOMKeyEvent::DOM_VK_NUMPAD7;      break;
    case kKeypad8KeyCode:        raptorKeyCode = nsIDOMKeyEvent::DOM_VK_NUMPAD8;      break;
    case kKeypad9KeyCode:        raptorKeyCode = nsIDOMKeyEvent::DOM_VK_NUMPAD9;      break;

    case kKeypadMultiplyKeyCode: raptorKeyCode = nsIDOMKeyEvent::DOM_VK_MULTIPLY;     break;
    case kKeypadAddKeyCode:      raptorKeyCode = nsIDOMKeyEvent::DOM_VK_ADD;          break;
    case kKeypadSubtractKeyCode: raptorKeyCode = nsIDOMKeyEvent::DOM_VK_SUBTRACT;     break;
    case kKeypadDecimalKeyCode:  raptorKeyCode = nsIDOMKeyEvent::DOM_VK_DECIMAL;      break;
    case kKeypadDivideKeyCode:   raptorKeyCode = nsIDOMKeyEvent::DOM_VK_DIVIDE;       break;
//  case ??             :        raptorKeyCode = nsIDOMKeyEvent::DOM_VK_SEPARATOR;    break;

// this may clash with vk_insert, but help key is more useful in mozilla
    case kHelpKeyCode:          raptorKeyCode = nsIDOMKeyEvent::DOM_VK_HELP;          break;
    case kDeleteKeyCode:        raptorKeyCode = nsIDOMKeyEvent::DOM_VK_DELETE;        break;
    case kEscapeKeyCode:        raptorKeyCode = nsIDOMKeyEvent::DOM_VK_ESCAPE;        break;
    case kClearKeyCode:         raptorKeyCode = nsIDOMKeyEvent::DOM_VK_CLEAR;         break;

    case kBackspaceKeyCode:     raptorKeyCode = nsIDOMKeyEvent::DOM_VK_BACK_SPACE;    break;
    case kTabKeyCode:           raptorKeyCode = nsIDOMKeyEvent::DOM_VK_TAB;           break;
    case kHomeKeyCode:          raptorKeyCode = nsIDOMKeyEvent::DOM_VK_HOME;          break;
    case kEndKeyCode:           raptorKeyCode = nsIDOMKeyEvent::DOM_VK_END;           break;
    case kPageUpKeyCode:        raptorKeyCode = nsIDOMKeyEvent::DOM_VK_PAGE_UP;       break;
    case kPageDownKeyCode:      raptorKeyCode = nsIDOMKeyEvent::DOM_VK_PAGE_DOWN;     break;
    case kLeftArrowKeyCode:     raptorKeyCode = nsIDOMKeyEvent::DOM_VK_LEFT;          break;
    case kRightArrowKeyCode:    raptorKeyCode = nsIDOMKeyEvent::DOM_VK_RIGHT;         break;
    case kUpArrowKeyCode:       raptorKeyCode = nsIDOMKeyEvent::DOM_VK_UP;            break;
    case kDownArrowKeyCode:     raptorKeyCode = nsIDOMKeyEvent::DOM_VK_DOWN;          break;

		default:
				if ((eventModifiers & controlKey) != 0)
				  charCode += 64;
	  	
				// if we haven't gotten the key code already, look at the char code
				switch (charCode)
				{
          case kReturnCharCode: raptorKeyCode = nsIDOMKeyEvent::DOM_VK_RETURN;       break;
          case kEnterCharCode:  raptorKeyCode = nsIDOMKeyEvent::DOM_VK_RETURN;       break;    // fix me!
          case ' ':             raptorKeyCode = nsIDOMKeyEvent::DOM_VK_SPACE;        break;
          case ';':             raptorKeyCode = nsIDOMKeyEvent::DOM_VK_SEMICOLON;    break;
          case '=':             raptorKeyCode = nsIDOMKeyEvent::DOM_VK_EQUALS;       break;
          case ',':             raptorKeyCode = nsIDOMKeyEvent::DOM_VK_COMMA;        break;
          case '.':             raptorKeyCode = nsIDOMKeyEvent::DOM_VK_PERIOD;       break;
          case '/':             raptorKeyCode = nsIDOMKeyEvent::DOM_VK_SLASH;        break;
          case '`':             raptorKeyCode = nsIDOMKeyEvent::DOM_VK_BACK_QUOTE;   break;
          case '{':
          case '[':             raptorKeyCode = nsIDOMKeyEvent::DOM_VK_OPEN_BRACKET; break;
          case '\\':            raptorKeyCode = nsIDOMKeyEvent::DOM_VK_BACK_SLASH;   break;
          case '}':
          case ']':             raptorKeyCode = nsIDOMKeyEvent::DOM_VK_CLOSE_BRACKET; break;
          case '\'':
          case '"':             raptorKeyCode = nsIDOMKeyEvent::DOM_VK_QUOTE;        break;

					default:
						
						if (charCode >= '0' && charCode <= '9')		// numerals
						{
							raptorKeyCode = charCode;
						}
						else if (charCode >= 'a' && charCode <= 'z')		// lowercase
						{
							raptorKeyCode = toupper(charCode);
						}
						else if (charCode >= 'A' && charCode <= 'Z')		// uppercase
						{
							raptorKeyCode = charCode;
						}

						break;
				}
	}

	return raptorKeyCode;
}


//-------------------------------------------------------------------------
//
// InitializeKeyEvent
//
//-------------------------------------------------------------------------

void nsMacEventHandler::InitializeKeyEvent(nsKeyEvent& aKeyEvent, 
    EventRecord& aOSEvent, nsWindow* aFocusedWidget, PRUint32 aMessage,
    PRBool* aIsChar, PRBool aConvertChar)
{
	//
	// initalize the basic message parts
	//
	aKeyEvent.time				= PR_IntervalNow();
	
	//
	// initalize the GUI event parts
	//
  aKeyEvent.widget = aFocusedWidget;
	aKeyEvent.nativeMsg		= (void*)&aOSEvent;
	
	//
	// nsInputEvent parts
	//
	aKeyEvent.isShift			= ((aOSEvent.modifiers & shiftKey) != 0);
	aKeyEvent.isControl		= ((aOSEvent.modifiers & controlKey) != 0);
	aKeyEvent.isAlt				= ((aOSEvent.modifiers & optionKey) != 0);
	aKeyEvent.isMeta		= ((aOSEvent.modifiers & cmdKey) != 0);
	
	//
	// nsKeyEvent parts
	//
  if (aIsChar)
    *aIsChar = PR_FALSE; 
  if (aMessage == NS_KEY_PRESS 
		&& !IsSpecialRaptorKey((aOSEvent.message & keyCodeMask) >> 8) )
	{
    if (aKeyEvent.isControl)
		{
      if (aIsChar)
        *aIsChar = PR_TRUE;
      if (aConvertChar) 
      {
			aKeyEvent.charCode = (aOSEvent.message & charCodeMask);
        if (aKeyEvent.charCode <= 26)
			{
          if (aKeyEvent.isShift)
					aKeyEvent.charCode += 'A' - 1;
				else
					aKeyEvent.charCode += 'a' - 1;
        } // if (aKeyEvent.charCode <= 26)
      }
			aKeyEvent.keyCode	= 0;
    } // if (aKeyEvent.isControl)
    else // else for if (aKeyEvent.isControl)
		{
      if (!aKeyEvent.isMeta)
			{
				aKeyEvent.isControl = aKeyEvent.isAlt = aKeyEvent.isMeta = 0;
      } // if (!aKeyEvent.isMeta)
    
			aKeyEvent.keyCode	= 0;
      if (aIsChar)
        *aIsChar =  PR_TRUE; 
      if (aConvertChar) 
      {
			aKeyEvent.charCode = ConvertKeyEventToUnicode(aOSEvent);
        if (aKeyEvent.isShift && aKeyEvent.charCode <= 'z' && aKeyEvent.charCode >= 'a') 
        {
			  aKeyEvent.charCode -= 32;
			}
			NS_ASSERTION(0 != aKeyEvent.charCode, "nsMacEventHandler::InitializeKeyEvent: ConvertKeyEventToUnicode returned 0.");
      }
    } // else for if (aKeyEvent.isControl)
	} // if (message == NS_KEY_PRESS && !IsSpecialRaptorKey((aOSEvent.message & keyCodeMask) >> 8) )
	else
	{
		aKeyEvent.keyCode = ConvertMacToRaptorKeyCode(aOSEvent.message, aOSEvent.modifiers);
		aKeyEvent.charCode = 0;
	} // else for  if (message == NS_KEY_PRESS && !IsSpecialRaptorKey((aOSEvent.message & keyCodeMask) >> 8) )
  
  //
  // obscure cursor if appropriate
  //
  if (aMessage == NS_KEY_PRESS 
		&& !aKeyEvent.isMeta 
		&& aKeyEvent.keyCode != nsIDOMKeyEvent::DOM_VK_PAGE_UP 
		&& aKeyEvent.keyCode != nsIDOMKeyEvent::DOM_VK_PAGE_DOWN
		// also consider:  function keys and sole modifier keys
	) 
	{
    ::ObscureCursor();
  }
}



//-------------------------------------------------------------------------
//
// IsSpecialRaptorKey
//
//-------------------------------------------------------------------------

PRBool nsMacEventHandler::IsSpecialRaptorKey(UInt32 macKeyCode)
{
	PRBool	isSpecial;

	// 
	// this table is used to determine which keys are special and should not generate a charCode
	//	
	switch (macKeyCode)
	{
// modifiers. We don't get separate events for these
// yet
		case kEscapeKeyCode:				isSpecial = PR_TRUE; break;
		case kShiftKeyCode:					isSpecial = PR_TRUE; break;
		case kCommandKeyCode:       isSpecial = PR_TRUE; break;
		case kCapsLockKeyCode:			isSpecial = PR_TRUE; break;
		case kControlKeyCode:				isSpecial = PR_TRUE; break;
		case kOptionkeyCode:				isSpecial = PR_TRUE; break;
		case kClearKeyCode:					isSpecial = PR_TRUE; break;

// function keys
		case kF1KeyCode:					isSpecial = PR_TRUE; break;
		case kF2KeyCode:					isSpecial = PR_TRUE; break;
		case kF3KeyCode:					isSpecial = PR_TRUE; break;
		case kF4KeyCode:					isSpecial = PR_TRUE; break;
		case kF5KeyCode:					isSpecial = PR_TRUE; break;
		case kF6KeyCode:					isSpecial = PR_TRUE; break;
		case kF7KeyCode:					isSpecial = PR_TRUE; break;
		case kF8KeyCode:					isSpecial = PR_TRUE; break;
		case kF9KeyCode:					isSpecial = PR_TRUE; break;
		case kF10KeyCode:					isSpecial = PR_TRUE; break;
		case kF11KeyCode:					isSpecial = PR_TRUE; break;
		case kF12KeyCode:					isSpecial = PR_TRUE; break;
		case kPauseKeyCode:				isSpecial = PR_TRUE; break;
		case kScrollLockKeyCode:	isSpecial = PR_TRUE; break;
		case kPrintScreenKeyCode:	isSpecial = PR_TRUE; break;

		case kHelpKeyCode:					isSpecial = PR_TRUE; break;
		case kDeleteKeyCode:				isSpecial = PR_TRUE; break;
		case kTabKeyCode:						isSpecial = PR_TRUE; break;
		case kBackspaceKeyCode:     isSpecial = PR_TRUE; break;

		case kHomeKeyCode:					isSpecial = PR_TRUE; break;	
		case kEndKeyCode:						isSpecial = PR_TRUE; break;
		case kPageUpKeyCode:				isSpecial = PR_TRUE; break;
		case kPageDownKeyCode:			isSpecial = PR_TRUE; break;
		case kLeftArrowKeyCode:			isSpecial = PR_TRUE; break;
		case kRightArrowKeyCode:		isSpecial = PR_TRUE; break;
		case kUpArrowKeyCode:				isSpecial = PR_TRUE; break;
		case kDownArrowKeyCode:			isSpecial = PR_TRUE; break;
		case kReturnKeyCode:        isSpecial = PR_TRUE; break;
		case kEnterKeyCode:         isSpecial = PR_TRUE; break;
		case kPowerbookEnterKeyCode: isSpecial = PR_TRUE; break;

		default:							isSpecial = PR_FALSE; break;
	}
	return isSpecial;
}


//-------------------------------------------------------------------------
//
// ConvertKeyEventToUnicode
//
//-------------------------------------------------------------------------
// we currently set the following to 5. We should fix this function later...
#define UNICODE_BUFFER_SIZE_FOR_KEY 5
PRUint32 nsMacEventHandler::ConvertKeyEventToUnicode(EventRecord& aOSEvent)
{
	char charResult = aOSEvent.message & charCodeMask;
	
	//
	// get the script of text for Unicode conversion
	//
	ScriptCode textScript = (ScriptCode)GetScriptManagerVariable(smKeyScript);

	//
	// convert our script code (smKeyScript) to a TextEncoding 
	//
	TextEncoding textEncodingFromScript;
	OSErr err = ::UpgradeScriptInfoToTextEncoding(textScript, kTextLanguageDontCare, kTextRegionDontCare, nsnull,
											&textEncodingFromScript);
	NS_ASSERTION(err == noErr, "nsMacEventHandler::ConvertKeyEventToUnicode: UpgradeScriptInfoToTextEncoding failed.");
	if (err != noErr) return 0;
	
	TextToUnicodeInfo	textToUnicodeInfo;
	err = ::CreateTextToUnicodeInfoByEncoding(textEncodingFromScript,&textToUnicodeInfo);
	NS_ASSERTION(err == noErr, "nsMacEventHandler::ConvertKeyEventToUnicode: CreateUnicodeToTextInfoByEncoding failed.");
	if (err != noErr) return 0;

	//
	// convert to Unicode
	//
	ByteCount result_size, source_read;
	PRUnichar unicharResult[UNICODE_BUFFER_SIZE_FOR_KEY];
	err = ::ConvertFromTextToUnicode(textToUnicodeInfo,
									sizeof(char),&charResult,
									kUnicodeLooseMappingsMask,
									0,NULL,NULL,NULL,
									sizeof(PRUnichar)*UNICODE_BUFFER_SIZE_FOR_KEY,&source_read,
									&result_size,NS_REINTERPRET_CAST(PRUint16*, unicharResult));
	::DisposeTextToUnicodeInfo(&textToUnicodeInfo);
	NS_ASSERTION(err == noErr, "nsMacEventHandler::ConvertKeyEventToUnicode: ConverFromTextToUnicode failed.");
	// if we got the following result, then it mean we got more than one Unichar from it.
	NS_ASSERTION(result_size == 2, "nsMacEventHandler::ConvertKeyEventToUnicode: ConverFromTextToUnicode failed.");
	// Fix Me!!!
	// the result_size will not equal to 2 in the following cases:
	//
	// 1. Hebrew/Arabic scripts, when we convert ( ) { } [ ]  < >  etc. 
	// The TEC will produce result_size = 6 (3 PRUnichar*) :
	// one Right-To-Left-Mark, the char and one Left-To-Right-Mark
	// We should fix this later...
	// See the following URL for the details...
	// ftp://ftp.unicode.org/Public/MAPPINGS/VENDORS/APPLE/ARABIC.TXT
	// ftp://ftp.unicode.org/Public/MAPPINGS/VENDORS/APPLE/FARSI.TXT
	// ftp://ftp.unicode.org/Public/MAPPINGS/VENDORS/APPLE/HEBREW.TXT
	//
	// 2. Also, it probably won't work in Thai and Indic keyboard since one char may convert to
	// several PRUnichar. It sometimes add zwj or zwnj. See the following url for details.
	// ftp://ftp.unicode.org/Public/MAPPINGS/VENDORS/APPLE/DEVANAGA.TXT
	// ftp://ftp.unicode.org/Public/MAPPINGS/VENDORS/APPLE/GUJARATI.TXT
	// ftp://ftp.unicode.org/Public/MAPPINGS/VENDORS/APPLE/GURMUKHI.TXT
	// ftp://ftp.unicode.org/Public/MAPPINGS/VENDORS/APPLE/THAI.TXT
	
//	if (err != noErr) return 0;
// I think we should ignore the above error since we already have the result we want

	return unicharResult[0];
}


//-------------------------------------------------------------------------
//
// HandleKeyEvent
//
//-------------------------------------------------------------------------

PRBool nsMacEventHandler::HandleKeyEvent(EventRecord& aOSEvent)
{
	nsresult result;
	nsWindow* checkFocusedWidget;

	// get the focused widget
	nsWindow* focusedWidget = gEventDispatchHandler.GetActive();
	if (!focusedWidget)
		focusedWidget = mTopLevelWidget;
	
	// nsEvent
	switch (aOSEvent.what)
	{
		case keyUp:
      {
        nsKeyEvent keyUpEvent(NS_KEY_UP);
        InitializeKeyEvent(keyUpEvent, aOSEvent, focusedWidget, NS_KEY_UP);
        result = focusedWidget->DispatchWindowEvent(keyUpEvent);
        break;
      }

    case keyDown:	
      {
        nsKeyEvent keyDownEvent(NS_KEY_DOWN), keyPressEvent(NS_KEY_PRESS);
        InitializeKeyEvent(keyDownEvent, aOSEvent, focusedWidget, NS_KEY_DOWN);
        result = focusedWidget->DispatchWindowEvent(keyDownEvent);

        // get the focused widget again in case something happened to it on the previous event
        checkFocusedWidget = gEventDispatchHandler.GetActive();
        if (!checkFocusedWidget)
          checkFocusedWidget = mTopLevelWidget;

        // if this isn't the same widget we had before, we should not send a keypress
        if (checkFocusedWidget != focusedWidget)
          return result;

        InitializeKeyEvent(keyPressEvent, aOSEvent, focusedWidget, NS_KEY_PRESS);

        // before we dispatch this key, check if it's the contextmenu key.
        // If so, send a context menu event instead.
        if ( IsContextMenuKey(keyPressEvent) ) {
          nsMouseEvent contextMenuEvent;
          ConvertKeyEventToContextMenuEvent(&keyPressEvent, &contextMenuEvent);
          result = focusedWidget->DispatchWindowEvent(contextMenuEvent);
          NS_ASSERTION(NS_SUCCEEDED(result), "cannot DispatchWindowEvent");
        }
        else {
          result = focusedWidget->DispatchWindowEvent(keyPressEvent);
          NS_ASSERTION(NS_SUCCEEDED(result), "cannot DispatchWindowEvent");
        }
        break;
      }

    case autoKey:
      {
        nsKeyEvent keyPressEvent(NS_KEY_PRESS);
        InitializeKeyEvent(keyPressEvent, aOSEvent, focusedWidget, NS_KEY_PRESS);
        result = focusedWidget->DispatchWindowEvent(keyPressEvent);
        break;
      }
	}

	return result;
}



//
// ConvertKeyEventToContextMenuEvent
//
// Take a key event and all of its attributes at convert it into
// a context menu event. We want just about everything (focused
// widget, etc) but a few things need to bt tweaked.
//
static void
ConvertKeyEventToContextMenuEvent(const nsKeyEvent* inKeyEvent, nsMouseEvent* outCMEvent)
{
  *(nsInputEvent*)outCMEvent = *(nsInputEvent*)inKeyEvent;
  
  outCMEvent->eventStructType = NS_MOUSE_EVENT;
  outCMEvent->message = NS_CONTEXTMENU_KEY;
  outCMEvent->isShift = outCMEvent->isControl = outCMEvent->isAlt = outCMEvent->isMeta = PR_FALSE;
  
  outCMEvent->clickCount = 0;
  outCMEvent->acceptActivation = PR_FALSE;
}


//
// IsContextMenuKey
//
// Check if the event should be a context menu event instead. Currently,
// that is a control-space.
//
static inline PRBool
IsContextMenuKey(const nsKeyEvent& inKeyEvent)
{
  enum { kContextMenuKey = ' ' } ;

  return ( inKeyEvent.charCode == kContextMenuKey && inKeyEvent.isControl &&
            !inKeyEvent.isShift && !inKeyEvent.isMeta && !inKeyEvent.isAlt );
}


//-------------------------------------------------------------------------
//
// HandleUKeyEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleUKeyEvent(const PRUnichar* text, long charCount, EventRecord& aOSEvent)
{
  nsresult result;
  // get the focused widget
  nsWindow* focusedWidget = gEventDispatchHandler.GetActive();
  if (!focusedWidget)
    focusedWidget = mTopLevelWidget;
  
  PRBool isCharacter = PR_FALSE;

  // simulate key down event if this isn't an autoKey event
  if (aOSEvent.what == keyDown)
  {
    nsKeyEvent keyDownEvent(NS_KEY_DOWN);
    InitializeKeyEvent(keyDownEvent, aOSEvent, focusedWidget, NS_KEY_DOWN, &isCharacter, PR_FALSE);
    result = focusedWidget->DispatchWindowEvent(keyDownEvent);
    NS_ASSERTION(NS_SUCCEEDED(result), "cannot DispatchWindowEvent keydown");

    // check if focus changed; see also HandleKeyEvent above
    nsWindow *checkFocusedWidget = gEventDispatchHandler.GetActive();
    if (!checkFocusedWidget)
      checkFocusedWidget = mTopLevelWidget;
    if (checkFocusedWidget != focusedWidget)
      return result;
  }

  // simulate key press events
  nsKeyEvent keyPressEvent(NS_KEY_PRESS);
  InitializeKeyEvent(keyPressEvent, aOSEvent, focusedWidget, NS_KEY_PRESS, &isCharacter, PR_FALSE);

  if (isCharacter) 
  {
    // it is a message with text, send all the unicode characters
    PRInt32 i;
    for (i = 0; i < charCount; i++)
    {
      keyPressEvent.charCode = text[i];

      // control key is special in that it doesn't give us letters
      // it generates a charcode of 0x01 for control-a
      // so we offset to do the right thing for gecko (as in HandleKeyEvent)
      // this doesn't happen for us in InitializeKeyEvent because we pass
      // PR_FALSE so no character translation occurs.
      // I'm guessing we don't want to do the translation there because
      // translation already occurred for the string passed to this method.
      if (keyPressEvent.isControl && keyPressEvent.charCode <= 26)       
      {
        if (keyPressEvent.isShift)
          keyPressEvent.charCode += 'A' - 1;
        else
          keyPressEvent.charCode += 'a' - 1;
      }

      // this block of code is triggered when user presses
      // a combination such as command-shift-M
      if (keyPressEvent.isShift && keyPressEvent.charCode <= 'z' && keyPressEvent.charCode >= 'a') 
        keyPressEvent.charCode -= 32;

      // before we dispatch a key, check if it's the context menu key.
      // If so, send a context menu event instead.
      if ( IsContextMenuKey(keyPressEvent) ) {
        nsMouseEvent contextMenuEvent;
        ConvertKeyEventToContextMenuEvent(&keyPressEvent, &contextMenuEvent);
        result = focusedWidget->DispatchWindowEvent(contextMenuEvent);
        NS_ASSERTION(NS_SUCCEEDED(result), "cannot DispatchWindowEvent");
      }
      else {
        // command / shift keys, etc. only send once
        result = focusedWidget->DispatchWindowEvent(keyPressEvent);
        NS_ASSERTION(NS_SUCCEEDED(result), "cannot DispatchWindowEvent");
      }
    }
  }
  else {
    // command / shift keys, etc. only send once
    result = focusedWidget->DispatchWindowEvent(keyPressEvent);
    NS_ASSERTION(NS_SUCCEEDED(result), "cannot DispatchWindowEvent");
  }
  return result;
}

#pragma mark -
//-------------------------------------------------------------------------
//
// HandleActivateEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleActivateEvent(EventRecord& aOSEvent)
{
#if PINK_PROFILING_ACTIVATE
if (KeyDown(0x39))	// press [caps lock] to start the profile
	ProfileStart();
#endif

  OSErr err;
  Boolean isActive;

  switch (aOSEvent.what)
  {
    case activateEvt:
      isActive = ((aOSEvent.modifiers & activeFlag) != 0);
      break;

    case osEvt:
      isActive = ! sInBackground;
      break;
  }

	if (isActive)
	{
		//
		// Activate The TSMDocument associated with this handler
		//
		if (mTSMDocument) {
		  // make sure we do not use input widnow even some other code turn it for default by calling 
		  // ::UseInputWindow(nsnull, TRUE); 
		  ::UseInputWindow(mTSMDocument, FALSE); 
		  err = ::ActivateTSMDocument(mTSMDocument);
		}

#ifdef DEBUG_TSM
#if 0
		NS_ASSERTION(err==noErr,"nsMacEventHandler::HandleActivateEvent: ActivateTSMDocument failed");
#endif
		printf("nsEventHandler::HandleActivateEvent: ActivateTSMDocument[%p] %s return %d\n",mTSMDocument,
		(err==noErr)?"":"ERROR", err);
#endif
		
		
		PRBool active;
		mTopLevelWidget->IsActive(&active);
		nsWindow*	focusedWidget = mTopLevelWidget;
		if (!active) {
		  gEventDispatchHandler.SetActivated(focusedWidget);
		  mTopLevelWidget->SetIsActive(PR_TRUE);
		}
		else if (!gEventDispatchHandler.GetActive()) {
		  NS_ASSERTION(0, "We think we're active, but there is no active widget!");
		  gEventDispatchHandler.SetActivated(focusedWidget);
		}
		
		// Twiddle menu bars
		nsIMenuBar* menuBar = focusedWidget->GetMenuBar();
		if (menuBar)
		{
		  menuBar->Paint();
		}
		else
		{
		  //ÄTODO:	if the focusedWidget doesn't have a menubar,
		  //				look all the way up to the window
		  //				until one of the parents has a menubar
		}
	}
	else
	{

		if (nsnull != gRollupListener && (nsnull != gRollupWidget) ) {
			if( mTopLevelWidget == gRollupWidget)
			gRollupListener->Rollup();
		}
		//
		// Deactivate the TSMDocument assoicated with this EventHandler
		//
		if (mTSMDocument) {
		  // make sure we do not use input widnow even some other code turn it for default by calling 
		  // ::UseInputWindow(nsnull, TRUE); 
		  ::UseInputWindow(mTSMDocument, FALSE); 
		  err = ::DeactivateTSMDocument(mTSMDocument);
		}

#ifdef DEBUG_TSM
		NS_ASSERTION((noErr==err)||(tsmDocNotActiveErr==err),"nsMacEventHandler::HandleActivateEvent: DeactivateTSMDocument failed");
		printf("nsEventHandler::HandleActivateEvent: DeactivateTSMDocument[%p] %s return %d\n",mTSMDocument,
		(err==noErr)?"":"ERROR", err);
#endif
		// Dispatch an NS_DEACTIVATE event 
		gEventDispatchHandler.SetDeactivated(mTopLevelWidget);
		mTopLevelWidget->SetIsActive(PR_FALSE);
	}
#if PINK_PROFILING_ACTIVATE
	ProfileSuspend();
	ProfileStop();
#endif

	return PR_TRUE;
}


//-------------------------------------------------------------------------
//
// UpdateEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::UpdateEvent ( )
{
	mTopLevelWidget->HandleUpdateEvent(nil);

	return PR_TRUE;
}


//-------------------------------------------------------------------------
//
// ResizeEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::ResizeEvent ( WindowRef inWindow )
{
	Rect macRect;
	::GetWindowPortBounds ( inWindow, &macRect );
	::LocalToGlobal(&topLeft(macRect));
	::LocalToGlobal(&botRight(macRect));
	mTopLevelWidget->Resize(macRect.right - macRect.left, macRect.bottom - macRect.top, PR_FALSE);
	if (nsnull != gRollupListener && (nsnull != gRollupWidget) )
		gRollupListener->Rollup();
	mTopLevelWidget->UserStateForResize(); // size a zoomed window and it's no longer zoomed

	return PR_TRUE;
}


//
// Scroll
//
// Called from a mouseWheel carbon event, tell Gecko to scroll.
// 
PRBool
nsMacEventHandler :: Scroll ( EventMouseWheelAxis inAxis, PRInt32 inDelta, const Point& inMouseLoc )
{
  // figure out which widget should be scrolled. First try the widget the mouse is under,
  // then try the last focussed widget.
  nsIWidget* widgetToScroll = gEventDispatchHandler.GetWidgetPointed();
  if ( !widgetToScroll )
    widgetToScroll = gEventDispatchHandler.GetActive();
  
  // the direction we get from the carbon event is opposite from the way mozilla looks at
  // it. Reverse the direction. Also, scroll by 3 lines at a time. |inDelta| represents the
  // number of groups of lines to scroll, not the exact number of lines to scroll.
  inDelta *= -3;
  
  HandleScrollEvent ( inAxis, PR_TRUE, inDelta, inMouseLoc, widgetToScroll );
  
  return PR_TRUE;
  
} // Scroll


//-------------------------------------------------------------------------
//
// HandleMouseDownEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleMouseDownEvent(EventRecord&	aOSEvent)
{    
	PRBool retVal = PR_FALSE;

	WindowPtr		whichWindow;
	short partCode = ::FindWindow(aOSEvent.where, &whichWindow);

  PRBool ignoreClickInContent = PR_FALSE;

  // Deal with any popups (comboboxes, xul popups, XP menus, etc) that might have to 
  // be closed down since they are implemented as stand-alone windows on top of
  // the current content window. If the click is not in the front window, we need to
  // hide the popup if one is visible. Furthermore, we want to ignore the click that
  // caused the popup to close and not pass it along to gecko.
  if ( whichWindow != ::FrontWindow() ) {
    if ( gRollupListener && gRollupWidget ) {
      PRBool rollup = PR_TRUE;

      // if we're dealing with menus, we probably have submenus and we don't want
      // to rollup if the click is in a parent menu of the current submenu.
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
              if ( NS_REINTERPRET_CAST(WindowPtr,widget->GetNativeData(NS_NATIVE_DISPLAY)) == whichWindow )
                 rollup = PR_FALSE;
            }         
          } // foreach parent menu widget
        }
      } // if rollup listener knows about menus
      
      // if we've determined that we should still rollup everything, do it.
      if ( rollup ) {
  			gRollupListener->Rollup();
  			ignoreClickInContent = PR_TRUE;
  	  }
 
		} // if a popup is active
  } // if click in a window not the frontmost
 
	switch (partCode)
	{
		case inDrag:
		{
			Point macPoint;
			Rect portRect;
			::GetWindowPortBounds(whichWindow, &portRect);
			macPoint = topLeft(portRect);
			::LocalToGlobal(&macPoint);
			mTopLevelWidget->MoveToGlobalPoint(macPoint.h, macPoint.v);
			break;
		}

		case inGrow:
		{
      ResizeEvent ( whichWindow );
      break;
		}

		case inGoAway:
		{
			ResetInputState();	// IM:TEXT 7-23 said we need to call FixTSMDocument when we go away...
			if (nsnull != gRollupListener && (nsnull != gRollupWidget) ) {
				gRollupListener->Rollup();
			}
			gEventDispatchHandler.DispatchGuiEvent(mTopLevelWidget, NS_XUL_CLOSE);		
			// mTopLevelWidget->Destroy(); (this, by contrast, would immediately close the window)
			break;
		}

		case inContent:
		{
		  // don't allow clicks that rolled up a popup through to the content area.
      if ( ignoreClickInContent )
        break;
						
			nsMouseEvent mouseEvent;
			PRUint32 mouseButton = NS_MOUSE_LEFT_BUTTON_DOWN;
			if ( aOSEvent.modifiers & controlKey )
			  mouseButton = NS_MOUSE_RIGHT_BUTTON_DOWN;
			ConvertOSEventToMouseEvent(aOSEvent, mouseEvent, mouseButton);

#if !TARGET_CARBON
      // Check if the mousedown is in our window's phantom scrollbar. If so, track
      // the movement of the mouse. The scrolling code is in the action proc.
      Point local = aOSEvent.where;
      ::GlobalToLocal ( &local );
      ControlHandle scrollbar;
      ControlPartCode partCode = ::FindControl(local, whichWindow, &scrollbar);
      if ( partCode >= kControlUpButtonPart && partCode <= kControlPageDownPart && scrollbar ) {
        PhantomScrollbarData* data = NS_REINTERPRET_CAST(PhantomScrollbarData*, ::GetControlReference(scrollbar));
        if ( data && data->mTag == PhantomScrollbarData::kUniqueTag ) {

#if USEMOUSEPOSITIONFORSCROLLWHEEL
// Uncomment this in order to set the widget to scroll the widget the mouse is over. However,
// we end up getting an idle event while scrolling quickly with the wheel, and the end result
// is that our idle-time mouseMove event kicks in a moves where we think the mouse is to where
// the scrollwheel driver has convinced the OS the mouse really is. Net result: we lose track
// of the widget and scrolling stops until you stop the wheel and move it again :(
       	  data->mWidgetToGetEvent = gEventDispatchHandler.GetWidgetPointed();            // tell action proc which widget to use
#endif

    	    ::TrackControl(scrollbar, local, mControlActionProc);
    	    data->mWidgetToGetEvent = nsnull;
          break;
        }
      }
#endif

			nsCOMPtr<nsIWidget> kungFuDeathGrip ( mouseEvent.widget );            // ensure widget doesn't go away
			nsWindow* widgetHit = NS_STATIC_CAST(nsWindow*, mouseEvent.widget);   //   while we're processing event
			if (widgetHit)
			{        
				// set the activation and focus on the widget hit, if it accepts it
				{
					nsMouseEvent mouseActivateEvent;
			        ConvertOSEventToMouseEvent(aOSEvent, mouseActivateEvent, NS_MOUSE_ACTIVATE);
					widgetHit->DispatchMouseEvent(mouseActivateEvent);
				}

				// dispatch the event
				retVal = widgetHit->DispatchMouseEvent(mouseEvent);
				
				// if we're a control-click, send in an additional NS_CONTEXTMENU event
				// after the mouse down.
				if ( mouseButton == NS_MOUSE_RIGHT_BUTTON_DOWN ) {
    			nsMouseEvent contextMenuEvent;
    			ConvertOSEventToMouseEvent(aOSEvent, contextMenuEvent, NS_CONTEXTMENU);
    			contextMenuEvent.isControl = PR_FALSE;    			
					widgetHit->DispatchMouseEvent(contextMenuEvent);
        } 
			} 
						
			gEventDispatchHandler.SetWidgetHit(widgetHit);
			sMouseInWidgetHit = PR_TRUE;
			break;
		}


		case inZoomIn:
		case inZoomOut:
		{
			gEventDispatchHandler.DispatchSizeModeEvent(mTopLevelWidget,
				partCode == inZoomIn ? nsSizeMode_Normal : nsSizeMode_Maximized);
			break;
		}

#if TARGET_CARBON
    case inToolbarButton:           // we get this part on Mac OS X only
      gEventDispatchHandler.DispatchGuiEvent(mTopLevelWidget, NS_OS_TOOLBAR);		
      break;
#endif
    
	}
	return retVal;
}


//-------------------------------------------------------------------------
//
// HandleMouseUpEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleMouseUpEvent(
														EventRecord&		aOSEvent)
{
	PRBool retVal = PR_FALSE;

	nsMouseEvent mouseEvent;
	ConvertOSEventToMouseEvent(aOSEvent, mouseEvent, NS_MOUSE_LEFT_BUTTON_UP);

	nsWindow* widgetReleased = (nsWindow*)mouseEvent.widget;
	nsWindow* widgetHit = gEventDispatchHandler.GetWidgetHit();

	if ( widgetReleased )
		retVal |= widgetReleased->DispatchMouseEvent(mouseEvent);
	
	if ( widgetReleased != widgetHit ) {
	  //XXX we should send a mouse exit event to the last widget, right?!?! But
	  //XXX we cannot use the same event, because the coordinates we just
	  //XXX computed are in the wrong window/widget coordinate space. I'm 
	  //XXX unclear what we should do in this case. (pinkerton).
	}
	
	gEventDispatchHandler.SetWidgetHit(nsnull);

	return retVal;
}


//-------------------------------------------------------------------------
//
// HandleMouseMoveEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleMouseMoveEvent( EventRecord& aOSEvent )
{
	nsWindow* lastWidgetHit = gEventDispatchHandler.GetWidgetHit();
	nsWindow* lastWidgetPointed = gEventDispatchHandler.GetWidgetPointed();
  
	PRBool retVal = PR_FALSE;

	nsMouseEvent mouseEvent;
	ConvertOSEventToMouseEvent(aOSEvent, mouseEvent, NS_MOUSE_MOVE);

	if (lastWidgetHit)
	{
		Point macPoint = aOSEvent.where;
		WindowRef wind = reinterpret_cast<WindowRef>(mTopLevelWidget->GetNativeData(NS_NATIVE_DISPLAY));
		nsGraphicsUtils::SafeSetPortWindowPort(wind);
		{
			StOriginSetter  originSetter(wind);
			::GlobalToLocal(&macPoint);
		}
		PRBool inWidgetHit = lastWidgetHit->PointInWidget(macPoint);
		if (sMouseInWidgetHit != inWidgetHit)
		{
			sMouseInWidgetHit = inWidgetHit;
			mouseEvent.message = (inWidgetHit ? NS_MOUSE_ENTER : NS_MOUSE_EXIT);
		}
		retVal |= lastWidgetHit->DispatchMouseEvent(mouseEvent);
	}
	else
	{
		nsWindow* widgetPointed = (nsWindow*)mouseEvent.widget;
		nsCOMPtr<nsIWidget> kungFuDeathGrip(widgetPointed);     // Protect during processing
		if (widgetPointed != lastWidgetPointed)
		{
			if (lastWidgetPointed)
			{
				mouseEvent.widget = lastWidgetPointed;
				mouseEvent.message = NS_MOUSE_EXIT;
				retVal |= lastWidgetPointed->DispatchMouseEvent(mouseEvent);
			}

      gEventDispatchHandler.SetWidgetPointed(widgetPointed);
#if TRACK_MOUSE_LOC
      gEventDispatchHandler.SetGlobalPoint(aOSEvent.where);
#endif

			if (widgetPointed)
			{
				mouseEvent.widget = widgetPointed;
				mouseEvent.message = NS_MOUSE_ENTER;
				retVal |= widgetPointed->DispatchMouseEvent(mouseEvent);
			}
		}
		else
		{
			if (widgetPointed)
				retVal |= widgetPointed->DispatchMouseEvent(mouseEvent);
		}
	}

	return retVal;
}


#pragma mark -
//-------------------------------------------------------------------------
//
// ConvertOSEventToMouseEvent
//
//-------------------------------------------------------------------------
void nsMacEventHandler::ConvertOSEventToMouseEvent(
														EventRecord&		aOSEvent,
														nsMouseEvent&		aMouseEvent,
														PRUint32				aMessage)
{
	static UInt32	sLastMouseUp = 0;
	static Point	sLastWhere = {0};
	static SInt16	sLastClickCount = 0;
	
	// we're going to time double-clicks from mouse *up* to next mouse *down*
	if (aMessage == NS_MOUSE_LEFT_BUTTON_UP)
	{
		// remember when this happened for the next mouse down
		sLastMouseUp = aOSEvent.when;
		sLastWhere = aOSEvent.where;
	}
	else if (aMessage == NS_MOUSE_LEFT_BUTTON_DOWN)
	{
		// now look to see if we want to convert this to a double- or triple-click
		const short kDoubleClickMoveThreshold	= 5;
		
		if (((aOSEvent.when - sLastMouseUp) < ::GetDblTime()) &&
				(((abs(aOSEvent.where.h - sLastWhere.h) < kDoubleClickMoveThreshold) &&
				 	(abs(aOSEvent.where.v - sLastWhere.v) < kDoubleClickMoveThreshold))))
		{		
			sLastClickCount ++;
			
//			if (sLastClickCount == 2)
//				aMessage = NS_MOUSE_LEFT_DOUBLECLICK;
		}
		else
		{
			// reset the click count, to count *this* click
			sLastClickCount = 1;
		}
	}

	// get the widget hit and the hit point inside that widget
	nsWindowType wtype;
	mTopLevelWidget->GetWindowType(wtype);
	PRBool          topLevelIsAPopup = (wtype == eWindowType_popup);
	WindowRef       eventTargetWindow = reinterpret_cast<WindowRef>(mTopLevelWidget->GetNativeData(NS_NATIVE_DISPLAY));

	WindowPtr       windowThatHasEvent = nsnull;
	ControlPartCode partCode = ::FindWindow(aOSEvent.where, &windowThatHasEvent);

	// if the mouse button is still down, send events to the last widget hit unless the
	// new event is in a popup window.
	nsWindow* lastWidgetHit = gEventDispatchHandler.GetWidgetHit();
	nsWindow* widgetHit = nsnull;
	if (lastWidgetHit)
	{
        // make sure we in the same window as where we started before we go assuming
        // that we know where the event will go.
        WindowRef   lastWind = reinterpret_cast<WindowRef>(lastWidgetHit->GetNativeData(NS_NATIVE_DISPLAY));
        PRBool      eventInSameWindowAsLastEvent = (windowThatHasEvent == lastWind);
        if ( eventInSameWindowAsLastEvent || !topLevelIsAPopup ) {	  
            if (::StillDown() || aMessage == NS_MOUSE_LEFT_BUTTON_UP)
            {
                widgetHit = lastWidgetHit;
                eventTargetWindow = lastWind;   // make sure we use the correct window to fix the coords
            }
            else
            {
                // Some widgets can eat mouseUp events (text widgets in TEClick, sbars in TrackControl).
                // In that case, stop considering this widget as being still hit.
                gEventDispatchHandler.SetWidgetHit(nsnull);
            }
        }
	}

	Point hitPoint = aOSEvent.where;
	nsGraphicsUtils::SafeSetPortWindowPort(eventTargetWindow);

	{
	    StOriginSetter  originSetter(eventTargetWindow);
    	::GlobalToLocal(&hitPoint);     // gives the mouse loc in local coords of the hit window
	}
	
	nsPoint widgetHitPoint(hitPoint.h, hitPoint.v);

	// if the mouse is in the grow box, pretend that it has left the window
	if ( partCode != inGrow ) {
		if (! widgetHit)
			widgetHit = mTopLevelWidget->FindWidgetHit(hitPoint);
	
		if (widgetHit)
		{
			nsRect bounds;
			widgetHit->GetBounds(bounds);
			nsPoint widgetOrigin(bounds.x, bounds.y);
			widgetHit->LocalToWindowCoordinate(widgetOrigin);
			widgetHitPoint.MoveBy(-widgetOrigin.x, -widgetOrigin.y);
		}
	}

    // if we haven't found anything and we're tracking the mouse for a popup, then
    // it's probable that the coordinates are just negative and we were dispatched
    // here just cuz we're the top window in the app right now. In that case, just
    // set the widget hit to this one. It's harmless (I hope), and it avoids asserts
    // in the view code about null widgets.
    if ( !widgetHit && topLevelIsAPopup && (hitPoint.h < 0 || hitPoint.v < 0) )
        widgetHit = mTopLevelWidget;
		
    // nsEvent
    aMouseEvent.message     = aMessage;
    aMouseEvent.point       = widgetHitPoint;
    aMouseEvent.time        = PR_IntervalNow();

    // nsGUIEvent
    aMouseEvent.widget      = widgetHit;
    aMouseEvent.nativeMsg   = (void*)&aOSEvent;

    // nsInputEvent
    aMouseEvent.isShift     = ((aOSEvent.modifiers & shiftKey) != 0);
    aMouseEvent.isControl   = ((aOSEvent.modifiers & controlKey) != 0);
    aMouseEvent.isAlt       = ((aOSEvent.modifiers & optionKey) != 0);
    aMouseEvent.isMeta      = ((aOSEvent.modifiers & cmdKey) != 0);

    // nsMouseEvent
    aMouseEvent.clickCount  = sLastClickCount;
    aMouseEvent.acceptActivation = PR_TRUE;
}

//-------------------------------------------------------------------------
//
// HandlePositionToOffsetEvent
//
//-------------------------------------------------------------------------
long nsMacEventHandler::HandlePositionToOffset(Point aPoint,short* regionClass)
{
	*regionClass = kTSMOutsideOfBody;
	return 0;
}

//-------------------------------------------------------------------------
//
// HandleOffsetToPosition Event
//
//-------------------------------------------------------------------------
nsresult nsMacEventHandler::HandleOffsetToPosition(long offset,Point* thePoint)
{
	thePoint->v = mIMEPos.y;
	thePoint->h = mIMEPos.x;
	//printf("local (x,y) = (%d, %d)\n", thePoint->h, thePoint->v);
	WindowRef wind = reinterpret_cast<WindowRef>(mTopLevelWidget->GetNativeData(NS_NATIVE_DISPLAY));
	nsGraphicsUtils::SafeSetPortWindowPort(wind);
	Rect savePortRect;
	::GetWindowPortBounds(wind, &savePortRect);
	::LocalToGlobal(thePoint);
	//printf("global (x,y) = (%d, %d)\n", thePoint->h, thePoint->v);

	return PR_TRUE;
}

//-------------------------------------------------------------------------
//
// HandleUpdate Event
//
//-------------------------------------------------------------------------
// See ftp://ftp.unicode.org/Public/MAPPINGS/VENDORS/APPLE/CORPCHAR.TXT for detail of IS_APPLE_HINT_IN_PRIVATE_ZONE
#define IS_APPLE_HINT_IN_PRIVATE_ZONE(u) ((0xF850 <= (u)) && ((u)<=0xF883))
nsresult nsMacEventHandler::HandleUpdateInputArea(const char* text,Size text_size, ScriptCode textScript,long fixedLength,TextRangeArray* textRangeList)
{
#ifdef DEBUG_TSM
	printf("********************************************************************************\n");
	printf("nsMacEventHandler::HandleUpdateInputArea size=%d fixlen=%d\n",text_size, fixedLength);
#endif
	TextToUnicodeInfo	textToUnicodeInfo;
	TextEncoding		textEncodingFromScript;
	int					i;
	OSErr				err;
	ByteCount			source_read;
	nsresult res = NS_OK;
	long committedLen = 0;
	PRUnichar* ubuf;

	//====================================================================================================
	// 0. Create Unicode Converter
	//====================================================================================================

	//
	// convert our script code  to a TextEncoding 
	//
	err = ::UpgradeScriptInfoToTextEncoding(textScript,kTextLanguageDontCare,kTextRegionDontCare,nsnull,
											&textEncodingFromScript);
	NS_ASSERTION(err==noErr,"nsMacEventHandler::UpdateInputArea: UpgradeScriptInfoToTextEncoding failed.");
	if (err!=noErr) { 
		res = NS_ERROR_FAILURE;
		return res; 
	}
	
	err = ::CreateTextToUnicodeInfoByEncoding(textEncodingFromScript,&textToUnicodeInfo);
	NS_ASSERTION(err==noErr,"nsMacEventHandler::UpdateInputArea: CreateUnicodeToTextInfoByEncoding failed.");
	if (err!=noErr) { 
		res = NS_ERROR_FAILURE;
		return res; 
	}
	//------------------------------------------------------------------------------------------------
	// if we aren't in composition mode alredy, signal the backing store w/ the mode change
	//------------------------------------------------------------------------------------------------
	if (!mIMEIsComposing) {
		res = HandleStartComposition();
		NS_ASSERTION(NS_SUCCEEDED(res),"nsMacEventHandler::UpdateInputArea: HandleStartComposition failed.");
		if(NS_FAILED(res))
			goto error;
	}
	// mIMECompositionStr should be created in the HandleStartComposition
	NS_ASSERTION(mIMECompositionStr, "do not have mIMECompositionStr"); 
	if(nsnull == mIMECompositionStr)
	{
		res = NS_ERROR_OUT_OF_MEMORY;
		goto error;
	}
	// Prepare buffer....
	mIMECompositionStr->SetCapacity(text_size+1);
	ubuf = mIMECompositionStr->BeginWriting();
	size_t len;

	//====================================================================================================
	// Note- It is possible that the UnpdateInputArea event sent both committed text and uncommitted text
	// in the same time. The easies way to do that is using Korean input method w/ "Enter by Character" option
	//====================================================================================================
	//	1. Handle the committed text
	//====================================================================================================
	committedLen = (fixedLength == -1) ? text_size : fixedLength;
	if(0 != committedLen)
	{
#ifdef DEBUG_TSM
		printf("Have commit text from 0 to %d\n",committedLen);
#endif
		//------------------------------------------------------------------------------------------------
		// 1.1 send textEvent to commit the text
		//------------------------------------------------------------------------------------------------
		len = 0;
		err = ::ConvertFromTextToUnicode(textToUnicodeInfo,committedLen,text,kUnicodeLooseMappingsMask,
						0,NULL,NULL,NULL,
						(text_size + 1) * sizeof(PRUnichar),
						&source_read,&len,NS_REINTERPRET_CAST(PRUint16*, ubuf));
		NS_ASSERTION(err==noErr,"nsMacEventHandler::UpdateInputArea: ConvertFromTextToUnicode failed.\n");
		if (err!=noErr)
		{
			res = NS_ERROR_FAILURE;
			goto error; 
		}
		len /= sizeof(PRUnichar);
		// 1.2 Strip off the Apple Private U+F850-U+F87F ( source hint characters, transcodeing hints
		// Metric characters
		// See ftp://ftp.unicode.org/Public/MAPPINGS/VENDORS/APPLE/CORPCHAR.TXT for detail
		PRUint32 s,d;
		for(s=d=0;s<len;s++)
		{
			if(! IS_APPLE_HINT_IN_PRIVATE_ZONE(ubuf[s]))
				ubuf[d++] = ubuf[s];
		}
		len = d;
		ubuf[len] = '\0';		 // null terminate
		mIMECompositionStr->SetLength(len);
		// for committed text, set no highlight ? (Do we need to set CaretPosition here ??? )
#ifdef DEBUG_TSM
			printf("1.2====================================\n");
#endif
		res = HandleTextEvent(0,nsnull);
		NS_ASSERTION(NS_SUCCEEDED(res),"nsMacEventHandler::UpdateInputArea: HandleTextEvent failed.");
		if(NS_FAILED(res)) 
			goto error; 
		//------------------------------------------------------------------------------------------------
		// 1.3 send compositionEvent to end the comosition
		//------------------------------------------------------------------------------------------------
		res = nsMacEventHandler::HandleEndComposition();
		NS_ASSERTION(NS_SUCCEEDED(res),"nsMacEventHandler::UpdateInputArea: HandleEndComposition failed.");
		if(NS_FAILED(res)) 
			goto error; 
	}  	//	1. Handle the committed text

	//====================================================================================================
	//	2. Handle the uncommitted text
	//====================================================================================================
	if((-1 != fixedLength) && (text_size != fixedLength ))
	{	
#ifdef DEBUG_TSM
		printf("Have new uncommited text from %d to text_size(%d)\n",committedLen,text_size);
#endif
		//------------------------------------------------------------------------------------------------
		// 2.1 send compositionEvent to start the comosition
		//------------------------------------------------------------------------------------------------
		//
		// if we aren't in composition mode alredy, signal the backing store w/ the mode change
		//	
		if (!mIMEIsComposing) {
			res = HandleStartComposition();
			NS_ASSERTION(NS_SUCCEEDED(res),"nsMacEventHandler::UpdateInputArea: HandleStartComposition failed.");
			if(NS_FAILED(res))
				goto error; 
		} 	// 2.1 send compositionEvent to start the comosition
		//------------------------------------------------------------------------------------------------
		// 2.2 send textEvent for the uncommitted text
		//------------------------------------------------------------------------------------------------
		//------------------------------------------------------------------------------------------------
		// 2.2.1 make sure we have one range array
		//------------------------------------------------------------------------------------------------

		TextRangeArray rawTextRangeArray;
		TextRangeArray *rangeArray;
		if(textRangeList && textRangeList->fNumOfRanges ) {
			rangeArray = textRangeList;
		} else {
			rangeArray = &rawTextRangeArray;
			rawTextRangeArray.fNumOfRanges = 1;
			rawTextRangeArray.fRange[0].fStart = committedLen;
			rawTextRangeArray.fRange[0].fEnd = text_size;
			rawTextRangeArray.fRange[0].fHiliteStyle = NS_TEXTRANGE_RAWINPUT;			
		}

		
#ifdef DEBUG_TSM
		printf("nsMacEventHandler::HandleUpdateInputArea textRangeList is %s\n", textRangeList ? "NOT NULL" : "NULL");
#endif
		nsTextRangeArray	xpTextRangeArray  = new nsTextRange[rangeArray->fNumOfRanges];
		NS_ASSERTION(xpTextRangeArray!=NULL,"nsMacEventHandler::UpdateInputArea: xpTextRangeArray memory allocation failed.");
		if (xpTextRangeArray==NULL)
		{
			res = NS_ERROR_OUT_OF_MEMORY;
			goto error; 
		}
	
		//------------------------------------------------------------------------------------------------
		// 2.2.2 convert range array into our xp range array
		//------------------------------------------------------------------------------------------------
		//
		// the TEC offset mapping capabilities won't work here because you need to have unique, ordered offsets
		//  so instead we iterate over the range list and map each range individually.  it's probably faster than
		//  trying to do collapse all the ranges into a single offset list
		//
		for(i=0;i<rangeArray->fNumOfRanges;i++) {			
			ByteOffset			sourceOffset[2], destinationOffset[2];
			ItemCount			destinationLength;
			// 2.2.2.1 check each range item in NS_ASSERTION
			NS_ASSERTION(
				(NS_TEXTRANGE_CARETPOSITION==rangeArray->fRange[i].fHiliteStyle)||
				(NS_TEXTRANGE_RAWINPUT==rangeArray->fRange[i].fHiliteStyle)||
				(NS_TEXTRANGE_SELECTEDRAWTEXT==rangeArray->fRange[i].fHiliteStyle)||
				(NS_TEXTRANGE_CONVERTEDTEXT==rangeArray->fRange[i].fHiliteStyle)||
				(NS_TEXTRANGE_SELECTEDCONVERTEDTEXT==rangeArray->fRange[i].fHiliteStyle),
				"illegal range type");
			NS_ASSERTION( rangeArray->fRange[i].fStart <= text_size,"illegal range");
			NS_ASSERTION( rangeArray->fRange[i].fEnd <= text_size,"illegal range");

#ifdef DEBUG_TSM
			printf("nsMacEventHandler::HandleUpdateInputArea textRangeList[%d] = (%d,%d) text_size = %d\n",i,
				rangeArray->fRange[i].fStart, rangeArray->fRange[i].fEnd, text_size);
#endif			
			// 2.2.2.2 fill sourceOffset array
			typedef enum {
				kEqualToDest0,
				kEqualToDest1,
				kEqualToLength
			} rangePairType;
			rangePairType tpStart,tpEnd;
			
			if(rangeArray->fRange[i].fStart < text_size) {
				sourceOffset[0] = rangeArray->fRange[i].fStart-committedLen;
				tpStart = kEqualToDest0;
				destinationLength = 1;
				if(rangeArray->fRange[i].fStart == rangeArray->fRange[i].fEnd) {
					tpEnd = kEqualToDest0;
				} else if(rangeArray->fRange[i].fEnd < text_size) {
					sourceOffset[1] = rangeArray->fRange[i].fEnd-committedLen;
					tpEnd = kEqualToDest1;
					destinationLength++;
				} else { 
					// fEnd >= text_size
					tpEnd = kEqualToLength;
				}
			} else { 
				// fStart >= text_size
				tpStart = kEqualToLength;
				tpEnd = kEqualToLength;
				destinationLength = 0;
			} // if(rangeArray->fRange[i].fStart < text_size) 
			
			// 2.2.2.3 call unicode converter to convert the sourceOffset into destinationOffset
			len = 0;
			// Note : The TEC will return -50 if sourceOffset[0,1] >= text_size-committedLen
			err = ::ConvertFromTextToUnicode(textToUnicodeInfo,text_size-committedLen,text+committedLen,kUnicodeLooseMappingsMask,
							destinationLength,sourceOffset,&destinationLength,destinationOffset,
							(text_size + 1) * sizeof(PRUnichar),
							&source_read,&len, NS_REINTERPRET_CAST(PRUint16*, ubuf));
			NS_ASSERTION(err==noErr,"nsMacEventHandler::UpdateInputArea: ConvertFromTextToUnicode failed.\n");
			if (err!=noErr) 
			{
				res = NS_ERROR_FAILURE;
				goto error; 
			}
			// 2.2.2.4 Convert len, destinationOffset[0,1] into the unicode of PRUnichar.
			len /= sizeof(PRUnichar);
			if(destinationLength > 0 ){
				destinationOffset[0] /= sizeof(PRUnichar);
				if(destinationLength > 1 ) {
					destinationOffset[1] /= sizeof(PRUnichar);
				}
			}
			// 2.2.2.5 Strip off the Apple Private U+F850-U+F87F ( source hint characters, transcodeing hints
			// Metric characters
			// See ftp://ftp.unicode.org/Public/MAPPINGS/VENDORS/APPLE/CORPCHAR.TXT for detail
			// If we don't do this, Trad Chinese input method won't handle ',' correctly
			PRUint32 s,d;
			for(s=d=0;s<len;s++)
			{
				if(! IS_APPLE_HINT_IN_PRIVATE_ZONE(ubuf[s]))
				{
					ubuf[d++] = ubuf[s];
				}
				else 
				{
					if(destinationLength > 0 ){
						if(destinationOffset[0] >= s) {
							destinationOffset[0]--;
						}
						if(destinationLength > 1 ) {
							if(destinationOffset[1] >= s) {
								destinationOffset[1]--;
							}
						}
					}	
				}
			}
			len = d;
						 
			// 2.2.2.6 put destinationOffset into xpTextRangeArray[i].mStartOffset 
			xpTextRangeArray[i].mRangeType = rangeArray->fRange[i].fHiliteStyle;
			switch(tpStart) {
				case kEqualToDest0:
					xpTextRangeArray[i].mStartOffset = destinationOffset[0];
				break;
				case kEqualToLength:
					xpTextRangeArray[i].mStartOffset = len;
				break;
				case kEqualToDest1:
				default:
					NS_ASSERTION(PR_FALSE, "tpStart is wrong");
				break;
			}
			switch(tpEnd) {
				case kEqualToDest0:
					xpTextRangeArray[i].mEndOffset = destinationOffset[0];
				break;
				case kEqualToDest1:
					xpTextRangeArray[i].mEndOffset = destinationOffset[1];
				break;
				case kEqualToLength:
					xpTextRangeArray[i].mEndOffset = len;
				break;
				default:
					NS_ASSERTION(PR_FALSE, "tpEnd is wrong");
				break;
			}
			// 2.2.2.7 Check the converted result in NS_ASSERTION
			NS_ASSERTION(xpTextRangeArray[i].mStartOffset <= len,"illegal range");
			NS_ASSERTION(xpTextRangeArray[i].mEndOffset <= len,"illegal range");
#ifdef DEBUG_TSM
			printf("nsMacEventHandler::HandleUpdateInputArea textRangeList[%d] => type=%d (%d,%d)\n",i,
				xpTextRangeArray[i].mRangeType,
				xpTextRangeArray[i].mStartOffset, xpTextRangeArray[i].mEndOffset);
#endif			

			NS_ASSERTION((NS_TEXTRANGE_CARETPOSITION!=xpTextRangeArray[i].mRangeType) ||
						 (xpTextRangeArray[i].mStartOffset == xpTextRangeArray[i].mEndOffset),
						 "start != end in CaretPosition");
			
		}
		//------------------------------------------------------------------------------------------------
		// 2.2.3 null terminate the uncommitted text
		//------------------------------------------------------------------------------------------------
		mIMECompositionStr->SetLength(len);			
		//------------------------------------------------------------------------------------------------
		// 2.2.4 send the text event
		//------------------------------------------------------------------------------------------------
#ifdef DEBUG_TSM
			printf("2.2.4====================================\n");
#endif			

		res = HandleTextEvent(rangeArray->fNumOfRanges,xpTextRangeArray);
		NS_ASSERTION(NS_SUCCEEDED(res), "nsMacEventHandler::UpdateInputArea: HandleTextEvent failed.");
		if(NS_FAILED(res)) 
			goto error; 
		if(xpTextRangeArray) 
			delete [] xpTextRangeArray;
	} //	2. Handle the uncommitted text
	else if((0==text_size) && (0==fixedLength))
	{
		// 3. Handle empty text event
		// This is needed when we input some uncommitted text, and then delete all of them
		// When the last delete come, we will got a text_size = 0 and fixedLength = 0
		// In that case, we need to send a text event to clean un the input hole....
		mIMECompositionStr->SetLength(0);			
#ifdef DEBUG_TSM
			printf("3.====================================\n");
#endif
		// 3.1 send the empty text event.
		res = HandleTextEvent(0,nsnull);
		NS_ASSERTION(NS_SUCCEEDED(res),"nsMacEventHandler::UpdateInputArea: HandleTextEvent failed.");
		if(NS_FAILED(res)) 
			goto error; 
		// 3.2 send an endComposition event, we need this to make sure the delete after this work properly.
		res = nsMacEventHandler::HandleEndComposition();
		NS_ASSERTION(NS_SUCCEEDED(res),"nsMacEventHandler::UpdateInputArea: HandleEndComposition failed.");
		if(NS_FAILED(res)) 
			goto error; 		
	}
	return res;
error:
	::DisposeTextToUnicodeInfo(&textToUnicodeInfo); 
	return res; 
}


nsresult nsMacEventHandler::UnicodeHandleUpdateInputArea(const PRUnichar* text, long charCount,
                                                         long fixedLength, TextRangeArray* textRangeList)
{
#ifdef DEBUG_TSM
  printf("********************************************************************************\n");
  printf("nsMacEventHandler::UnicodeHandleUpdateInputArea size=%d fixlen=%d\n",charCount, fixedLength);
#endif
  nsresult res = NS_OK;
  long committedLen = 0;
  //------------------------------------------------------------------------------------------------
  // if we aren't in composition mode alredy, signal the backing store w/ the mode change
  //------------------------------------------------------------------------------------------------
  if (!mIMEIsComposing) {
    res = HandleStartComposition();
    NS_ASSERTION(NS_SUCCEEDED(res), "nsMacEventHandler::UnicodeHandleUpdateInputArea: HandleStartComposition failed.");
    if (NS_FAILED(res))
      goto error;
  }
  // mIMECompositionStr should be created in the HandleStartComposition
  NS_ASSERTION(mIMECompositionStr, "do not have mIMECompositionStr");
  if (nsnull == mIMECompositionStr)
  {
    res = NS_ERROR_OUT_OF_MEMORY;
    goto error;
  }

  //====================================================================================================
  // Note- It is possible that the UpdateInputArea event sent both committed text and uncommitted text
  // at the same time. The easiest way to do that is using Korean input method w/ "Enter by Character" option
  //====================================================================================================
  //  1. Handle the committed text
  //====================================================================================================
  committedLen = (fixedLength == -1) ? charCount : fixedLength;
  if (0 != committedLen)
  {
#ifdef DEBUG_TSM
    printf("Have commit text from 0 to %d\n", committedLen);
#endif
    //------------------------------------------------------------------------------------------------
    // 1.1 send textEvent to commit the text
    //------------------------------------------------------------------------------------------------
    mIMECompositionStr->Assign(text, committedLen);
#ifdef DEBUG_TSM
    printf("1.2====================================\n");
#endif
    res = HandleTextEvent(0, nsnull);
    NS_ASSERTION(NS_SUCCEEDED(res), "nsMacEventHandler::UnicodeHandleUpdateInputArea: HandleTextEvent failed.");
    if (NS_FAILED(res)) 
      goto error; 
    //------------------------------------------------------------------------------------------------
    // 1.3 send compositionEvent to end the composition
    //------------------------------------------------------------------------------------------------
    res = nsMacEventHandler::HandleEndComposition();
    NS_ASSERTION(NS_SUCCEEDED(res), "nsMacEventHandler::UnicodeHandleUpdateInputArea: HandleEndComposition failed.");
    if (NS_FAILED(res)) 
      goto error; 
  }    //  1. Handle the committed text

  //====================================================================================================
  //  2. Handle the uncommitted text
  //====================================================================================================
  if ((-1 != fixedLength) && (charCount != fixedLength))
  {  
#ifdef DEBUG_TSM
    printf("Have new uncommitted text from %d to text_size(%d)\n", committedLen, charCount);
#endif
    //------------------------------------------------------------------------------------------------
    // 2.1 send compositionEvent to start the composition
    //------------------------------------------------------------------------------------------------
    //
    // if we aren't in composition mode already, signal the backing store w/ the mode change
    //  
    if (!mIMEIsComposing) {
      res = HandleStartComposition();
      NS_ASSERTION(NS_SUCCEEDED(res), "nsMacEventHandler::UnicodeHandleUpdateInputArea: HandleStartComposition failed.");
      if (NS_FAILED(res))
        goto error; 
    }   // 2.1 send compositionEvent to start the composition
    //------------------------------------------------------------------------------------------------
    // 2.2 send textEvent for the uncommitted text
    //------------------------------------------------------------------------------------------------
    //------------------------------------------------------------------------------------------------
    // 2.2.1 make sure we have one range array
    //------------------------------------------------------------------------------------------------

    TextRangeArray rawTextRangeArray;
    TextRangeArray *rangeArray;
    if (textRangeList && textRangeList->fNumOfRanges) {
      rangeArray = textRangeList;
    } else {
      rangeArray = &rawTextRangeArray;
      rawTextRangeArray.fNumOfRanges = 1;
      rawTextRangeArray.fRange[0].fStart = committedLen * 2;
      rawTextRangeArray.fRange[0].fEnd = charCount * 2;
      rawTextRangeArray.fRange[0].fHiliteStyle = NS_TEXTRANGE_RAWINPUT;      
    }

    
#ifdef DEBUG_TSM
    printf("nsMacEventHandler::UnicodeHandleUpdateInputArea textRangeList is %s\n", textRangeList ? "NOT NULL" : "NULL");
#endif
    nsTextRangeArray xpTextRangeArray = new nsTextRange[rangeArray->fNumOfRanges];
    NS_ASSERTION(xpTextRangeArray!=NULL, "nsMacEventHandler::UnicodeHandleUpdateInputArea: xpTextRangeArray memory allocation failed.");
    if (xpTextRangeArray == NULL)
    {
      res = NS_ERROR_OUT_OF_MEMORY;
      goto error; 
    }
  
    //------------------------------------------------------------------------------------------------
    // 2.2.2 convert range array into our xp range array
    //------------------------------------------------------------------------------------------------
    //
    // the TEC offset mapping capabilities won't work here because you need to have unique, ordered offsets
    //  so instead we iterate over the range list and map each range individually.  it's probably faster than
    //  trying to do collapse all the ranges into a single offset list
    //
    PRUint32 i;
    for(i = 0; i < rangeArray->fNumOfRanges; i++) {      
      // 2.2.2.1 check each range item in NS_ASSERTION
      NS_ASSERTION(
        (NS_TEXTRANGE_CARETPOSITION==rangeArray->fRange[i].fHiliteStyle)||
        (NS_TEXTRANGE_RAWINPUT==rangeArray->fRange[i].fHiliteStyle)||
        (NS_TEXTRANGE_SELECTEDRAWTEXT==rangeArray->fRange[i].fHiliteStyle)||
        (NS_TEXTRANGE_CONVERTEDTEXT==rangeArray->fRange[i].fHiliteStyle)||
        (NS_TEXTRANGE_SELECTEDCONVERTEDTEXT==rangeArray->fRange[i].fHiliteStyle),
        "illegal range type");
      NS_ASSERTION( rangeArray->fRange[i].fStart/2 <= charCount, "illegal range");
      NS_ASSERTION( rangeArray->fRange[i].fEnd/2 <= charCount, "illegal range");

#ifdef DEBUG_TSM
      printf("nsMacEventHandler::UnicodeHandleUpdateInputArea textRangeList[%d] = (%d,%d) text_size = %d\n",i,
        rangeArray->fRange[i].fStart/2, rangeArray->fRange[i].fEnd/2, charCount);
#endif      
      // 2.2.2.6 put destinationOffset into xpTextRangeArray[i].mStartOffset 
      xpTextRangeArray[i].mRangeType = rangeArray->fRange[i].fHiliteStyle;
      xpTextRangeArray[i].mStartOffset = rangeArray->fRange[i].fStart / 2;
      xpTextRangeArray[i].mEndOffset   = rangeArray->fRange[i].fEnd / 2;

      // 2.2.2.7 Check the converted result in NS_ASSERTION
#ifdef DEBUG_TSM
      printf("nsMacEventHandler::UnicodeHandleUpdateInputArea textRangeList[%d] => type=%d (%d,%d)\n",i,
        xpTextRangeArray[i].mRangeType,
        xpTextRangeArray[i].mStartOffset, xpTextRangeArray[i].mEndOffset);
#endif      

      NS_ASSERTION((NS_TEXTRANGE_CARETPOSITION!=xpTextRangeArray[i].mRangeType) ||
             (xpTextRangeArray[i].mStartOffset == xpTextRangeArray[i].mEndOffset),
             "start != end in CaretPosition");
    }
    mIMECompositionStr->Assign(text+committedLen, charCount-committedLen);
    //------------------------------------------------------------------------------------------------
    // 2.2.4 send the text event
    //------------------------------------------------------------------------------------------------
#ifdef DEBUG_TSM
      printf("2.2.4====================================\n");
#endif      

    res = HandleTextEvent(rangeArray->fNumOfRanges,xpTextRangeArray);
    NS_ASSERTION(NS_SUCCEEDED(res), "nsMacEventHandler::UnicodeHandleUpdateInputArea: HandleTextEvent failed.");
    if (NS_FAILED(res)) 
      goto error; 
    delete [] xpTextRangeArray;
  } //  2. Handle the uncommitted text
  else if ((0==charCount) && (0==fixedLength))
  {
    // 3. Handle empty text event
    // This is needed when we input some uncommitted text, and then delete all of them
    // When the last delete come, we will got a text_size = 0 and fixedLength = 0
    // In that case, we need to send a text event to clean up the input hole....
    mIMECompositionStr->Truncate();      
#ifdef DEBUG_TSM
      printf("3.====================================\n");
#endif
    // 3.1 send the empty text event.
    res = HandleTextEvent(0, nsnull);
    NS_ASSERTION(NS_SUCCEEDED(res),"nsMacEventHandler::UnicodeHandleUpdateInputArea: HandleTextEvent failed.");
    if (NS_FAILED(res)) 
      goto error; 
    // 3.2 send an endComposition event, we need this to make sure the delete after this work properly.
    res = nsMacEventHandler::HandleEndComposition();
    NS_ASSERTION(NS_SUCCEEDED(res),"nsMacEventHandler::UnicodeHandleUpdateInputArea: HandleEndComposition failed.");
    if (NS_FAILED(res)) 
      goto error;     
  }
  
error:
  return res;
}

nsresult nsMacEventHandler::HandleUnicodeGetSelectedText(nsAString& outString)
{
  outString.Truncate(0);
  nsWindow* focusedWidget = gEventDispatchHandler.GetActive();
  if (!focusedWidget)
    focusedWidget = mTopLevelWidget;

  nsReconversionEvent reconversionEvent(NS_RECONVERSION_QUERY, focusedWidget);
  reconversionEvent.time = PR_IntervalNow();

  nsresult res = focusedWidget->DispatchWindowEvent(reconversionEvent);

  if (NS_SUCCEEDED(res)) {
     outString.Assign(reconversionEvent.theReply.mReconversionString);
     nsMemory::Free(reconversionEvent.theReply.mReconversionString);
  } 
  return res; 
  
}
//-------------------------------------------------------------------------
//
// HandleStartComposition
//
//-------------------------------------------------------------------------
nsresult nsMacEventHandler::HandleStartComposition(void)
{
#ifdef DEBUG_TSM
	printf("HandleStartComposition\n");
#endif
	mIMEIsComposing = PR_TRUE;
	if(nsnull == mIMECompositionStr)
		mIMECompositionStr = new nsAutoString();
	NS_ASSERTION(mIMECompositionStr, "cannot allocate mIMECompositionStr");
	if(nsnull == mIMECompositionStr)
	{
		return NS_ERROR_OUT_OF_MEMORY;
	}	
	// 
	// get the focused widget [tague: may need to rethink this later]
	//
	nsWindow* focusedWidget = gEventDispatchHandler.GetActive();
	if (!focusedWidget)
		focusedWidget = mTopLevelWidget;
	
	//
	// create the nsCompositionEvent
	//
	nsCompositionEvent		compositionEvent(NS_COMPOSITION_START, focusedWidget);
	compositionEvent.time = PR_IntervalNow();

	nsresult res = focusedWidget->DispatchWindowEvent(compositionEvent);
	if(NS_SUCCEEDED(res)) {
		mIMEPos.x = compositionEvent.theReply.mCursorPosition.x;
		mIMEPos.y = compositionEvent.theReply.mCursorPosition.y
		          + compositionEvent.theReply.mCursorPosition.height;
		focusedWidget->LocalToWindowCoordinate(mIMEPos);
#ifdef DEBUG_TSM
		printf("HandleStartComposition reply (%d,%d)\n", mIMEPos.x , mIMEPos.y);
#endif
	}
	return res;
}

//-------------------------------------------------------------------------
//
// HandleEndComposition
//
//-------------------------------------------------------------------------
nsresult nsMacEventHandler::HandleEndComposition(void)
{
#ifdef DEBUG_TSM
	printf("HandleEndComposition\n");
#endif
	mIMEIsComposing = PR_FALSE;
	// 
	// get the focused widget [tague: may need to rethink this later]
	//
	nsWindow* focusedWidget = gEventDispatchHandler.GetActive();
	if (!focusedWidget)
		focusedWidget = mTopLevelWidget;
	
	//
	// create the nsCompositionEvent
	//
	nsCompositionEvent		compositionEvent(NS_COMPOSITION_END, focusedWidget);
	compositionEvent.time = PR_IntervalNow();

	return(focusedWidget->DispatchWindowEvent(compositionEvent));
}

//-------------------------------------------------------------------------
//
// HandleTextEvent
//
//-------------------------------------------------------------------------
nsresult nsMacEventHandler::HandleTextEvent(PRUint32 textRangeCount, nsTextRangeArray textRangeArray)
{
#ifdef DEBUG_TSM
	printf("HandleTextEvent\n");
	PRUint32 i;
	printf("text event \n[");
	const PRUnichar *ubuf = mIMECompositionStr->get();
	for(i=0; '\0' != *ubuf; i++)
		printf("U+%04X ", *ubuf++);
	printf("] len = %d\n",i);
	if(textRangeCount > 0)
	{
		for(i=0;i<textRangeCount;i++ )
		{
			NS_ASSERTION((NS_TEXTRANGE_CARETPOSITION!=textRangeArray[i].mRangeType) ||
						 (textRangeArray[i].mStartOffset == textRangeArray[i].mEndOffset),
						 "start != end in CaretPosition");
			NS_ASSERTION(
				(NS_TEXTRANGE_CARETPOSITION==textRangeArray[i].mRangeType)||
				(NS_TEXTRANGE_RAWINPUT==textRangeArray[i].mRangeType)||
				(NS_TEXTRANGE_SELECTEDRAWTEXT==textRangeArray[i].mRangeType)||
				(NS_TEXTRANGE_CONVERTEDTEXT==textRangeArray[i].mRangeType)||
				(NS_TEXTRANGE_SELECTEDCONVERTEDTEXT==textRangeArray[i].mRangeType),
				"illegal range type");
			static char *name[6] =
			{
				"Unknown",
				"CaretPosition", 
				"RawInput", 
				"SelectedRawText",
				"ConvertedText",
				"SelectedConvertedText"
			};
			printf("[%d,%d]=%s\n", 
			textRangeArray[i].mStartOffset, 
			textRangeArray[i].mEndOffset,
			((textRangeArray[i].mRangeType<=NS_TEXTRANGE_SELECTEDCONVERTEDTEXT) ?
				name[textRangeArray[i].mRangeType] : name[0])
			);
		}
	}
#endif
	// 
	// get the focused widget [tague: may need to rethink this later]
	//
	nsWindow* focusedWidget = gEventDispatchHandler.GetActive();
	if (!focusedWidget)
		focusedWidget = mTopLevelWidget;
	
	//
	// create the nsTextEvent
	//
	nsTextEvent		textEvent(NS_TEXT_TEXT, focusedWidget);
	textEvent.time = PR_IntervalNow();
	textEvent.theText = mIMECompositionStr->get();
	textEvent.rangeCount = textRangeCount;
	textEvent.rangeArray = textRangeArray;

	nsresult res = NS_OK;
	if (NS_SUCCEEDED(res = focusedWidget->DispatchWindowEvent(textEvent))) {
		mIMEPos.x = textEvent.theReply.mCursorPosition.x;
		mIMEPos.y = textEvent.theReply.mCursorPosition.y +
		            textEvent.theReply.mCursorPosition.height;
		focusedWidget->LocalToWindowCoordinate(mIMEPos);
#ifdef DEBUG_TSM
		printf("HandleTextEvent reply (%d,%d)\n", mIMEPos.x , mIMEPos.y);
#endif
	} 
	return res;
}
nsresult nsMacEventHandler::ResetInputState()
{
	OSErr err = noErr;
	if (mTSMDocument) {
		// make sure we do not use input widnow even some other code turn it for default by calling 
		// ::UseInputWindow(nsnull, TRUE); 
		::UseInputWindow(mTSMDocument, FALSE); 
		err = ::FixTSMDocument(mTSMDocument);
		NS_ASSERTION( (noErr==err)||(tsmDocNotActiveErr==err)||(tsmTSNotOpenErr), "Cannot FixTSMDocument");
	}
	return NS_OK;	
}

