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
#include "nsGfxUtils.h"

static nsMacEventHandler* sLastActive;

//#define DEBUG_TSM
extern nsIRollupListener * gRollupListener;
extern nsIWidget         * gRollupWidget;


// from MacHeaders.c
#ifndef topLeft
	#define topLeft(r)	(((Point *) &(r))[0])
#endif
#ifndef botRight
	#define botRight(r)	(((Point *) &(r))[1])
#endif

static void ConvertKeyEventToContextMenuEvent(const nsKeyEvent* inKeyEvent, nsMouseEvent* outCMEvent);
static inline PRBool IsContextMenuKey(const nsKeyEvent& inKeyEvent);


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

	nsGUIEvent guiEvent(PR_TRUE, aEventType, aWidget);
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

	nsSizeModeEvent event(PR_TRUE, NS_SIZEMODE, aWidget);
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
        nsIWidget* parent = curWin->GetParent();
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
nsMacEventHandler::nsMacEventHandler(nsMacWindow* aTopLevelWidget,
                                     nsMacEventDispatchHandler* aEventDispatchHandler)
{
  OSErr err;
  InterfaceTypeList supportedServices;

  mTopLevelWidget = aTopLevelWidget;

  //
  // create a TSMDocument for this window.  We are allocating a TSM document for
  // each Mac window
  //
  mTSMDocument = nsnull;
  supportedServices[0] = kUnicodeDocument;
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

  mKeyIgnore = PR_FALSE;
  mKeyHandled = PR_FALSE;

  mLastModifierState = 0;

  mMouseInWidgetHit = PR_FALSE;

  ClearLastMouseUp();

  if (aEventDispatchHandler) {
    mEventDispatchHandler = aEventDispatchHandler;
    mOwnEventDispatchHandler = PR_FALSE;
  }
  else {
    mEventDispatchHandler = new nsMacEventDispatchHandler();
    mOwnEventDispatchHandler = PR_TRUE;
  }
}


nsMacEventHandler::~nsMacEventHandler()
{
	if (mTSMDocument)
		(void)::DeleteTSMDocument(mTSMDocument);
	if(nsnull != mIMECompositionStr) {
		delete mIMECompositionStr;
		mIMECompositionStr = nsnull;
	}

	if (mOwnEventDispatchHandler)
		delete mEventDispatchHandler;

  if (sLastActive == this) {
    // This shouldn't happen
    sLastActive = nsnull;
  }
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
		case mouseDown:
			retVal = HandleMouseDownEvent(aOSEvent);
			break;

		case mouseUp:
			retVal = HandleMouseUpEvent(aOSEvent);
			break;

		case osEvt:
		{
			unsigned char eventType = ((aOSEvent.message >> 24) & 0x00ff);
			if (eventType == mouseMovedMessage)
			{
				retVal = HandleMouseMoveEvent(aOSEvent);
			}
		}
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
	nsWindow* focusedWidget = mEventDispatchHandler->GetActive();
	if (!focusedWidget)
		focusedWidget = mTopLevelWidget;

	// nsEvent
	nsMenuEvent menuEvent(PR_TRUE, NS_MENU_SELECTED, focusedWidget);
	menuEvent.refPoint.x		= aOSEvent.where.h;
	menuEvent.refPoint.y		= aOSEvent.where.v;
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
		if (focusedWidget == mEventDispatchHandler->GetActive())
		{
      // Hold a ref across event dispatch
			nsCOMPtr<nsIWidget> parent = focusedWidget->GetParent();
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
					parent = parent->GetParent();
				}
			}
		}
	}

	return eventHandled;
}

#endif


void
nsMacEventHandler::InitializeMouseEvent(nsMouseEvent& aMouseEvent,
                                        nsPoint&      aPoint,
                                        PRInt16       aModifiers,
                                        PRUint32      aClickCount)
{
  // nsEvent
  aMouseEvent.refPoint   = aPoint;
  aMouseEvent.time       = PR_IntervalNow();

  // nsInputEvent
  aMouseEvent.isShift    = ((aModifiers & shiftKey)   != 0);
  aMouseEvent.isControl  = ((aModifiers & controlKey) != 0);
  aMouseEvent.isAlt      = ((aModifiers & optionKey)  != 0);
  aMouseEvent.isMeta     = ((aModifiers & cmdKey)     != 0);

  // nsMouseEvent
  aMouseEvent.clickCount = aClickCount;
}


//-------------------------------------------------------------------------
//
// DragEvent
//
//-------------------------------------------------------------------------
//
// Someone on the outside told us that something related to a drag is
// happening.  The exact event type is passed in as |aMessage|. We need to
// send this event into Gecko for processing.  Create a Gecko event (using
// the appropriate message type) and pass it along.
//
PRBool nsMacEventHandler::DragEvent(unsigned int aMessage,
                                    Point        aMouseGlobal,
                                    UInt16       aKeyModifiers)
{
  // Convert the mouse to local coordinates.  We have to do all the funny port
  // origin stuff just in case it has been changed.
  Point hitPointLocal = aMouseGlobal;
  WindowRef wind =
   NS_STATIC_CAST(WindowRef, mTopLevelWidget->GetNativeData(NS_NATIVE_DISPLAY));
  nsGraphicsUtils::SafeSetPortWindowPort(wind);
  {
    StOriginSetter  originSetter(wind);
    ::GlobalToLocal(&hitPointLocal);
  }
  nsPoint widgetHitPoint(hitPointLocal.h, hitPointLocal.v);

  nsWindow* widgetHit = mTopLevelWidget->FindWidgetHit(hitPointLocal);
  if (widgetHit) {
    // Adjust from local coordinates to window coordinates in case the hit
    // widget isn't at (0, 0).
    nsRect bounds;
    widgetHit->GetBounds(bounds);
    nsPoint widgetOrigin(bounds.x, bounds.y);
    widgetHit->LocalToWindowCoordinate(widgetOrigin);
    widgetHitPoint.MoveBy(-widgetOrigin.x, -widgetOrigin.y);		
  }

  // Note that aMessage will be NS_DRAGDROP_(ENTER|EXIT|OVER|DROP) depending
  // on the state of the drag relative to the top-level window, not the
  // widget that interests us.  As a result, NS_DRAGDROP_ENTER and
  // NS_DRAGDROP_EXIT events must be synthesized as the drag moves between
  // widgets in this window.

  if (aMessage == NS_DRAGDROP_EXIT) {
    // If the drag is leaving the window, it can't be over a widget contained
    // within the window.
    widgetHit = nsnull;
  }

  nsWindow* lastWidget = mEventDispatchHandler->GetWidgetPointed();
  if (lastWidget != widgetHit) {
    if (aMessage != NS_DRAGDROP_ENTER) {
      if (lastWidget) {
        // Send an NS_DRAGDROP_EXIT event to the last widget.  This is not done
        // if aMessage == NS_DRAGDROP_ENTER, because that indicates that the
        // drag is either beginning (in which case there is no valid last
        // widget for the drag) or reentering the window (in which case
        // NS_DRAGDROP_EXIT was sent when the mouse left the window).

        nsMouseEvent exitEvent(PR_TRUE, NS_DRAGDROP_EXIT, lastWidget,
                               nsMouseEvent::eReal);
        nsPoint zero(0, 0);
        InitializeMouseEvent(exitEvent, zero, aKeyModifiers, 1);
        lastWidget->DispatchMouseEvent(exitEvent);
      }

      if (aMessage != NS_DRAGDROP_EXIT && widgetHit) {
        // Send an NS_DRAGDROP_ENTER event to the new widget.  This is not
        // done when aMessage == NS_DRAGDROP_EXIT, because that indicates
        // that the drag is leaving the window.

        nsMouseEvent enterEvent(PR_TRUE, NS_DRAGDROP_ENTER, widgetHit,
                                nsMouseEvent::eReal);
        InitializeMouseEvent(enterEvent, widgetHitPoint, aKeyModifiers, 1);
        widgetHit->DispatchMouseEvent(enterEvent);
      }
    }
  }

  // update the tracking of which widget the mouse is now over.
  mEventDispatchHandler->SetWidgetPointed(widgetHit);

  if (!widgetHit) {
    // There is no widget to receive the event.  This happens when the drag
    // leaves the window or when the drag moves over a part of the window
    // not containing any widget, such as the title bar.  When appropriate,
    // NS_DRAGDROP_EXIT was dispatched to lastWidget above, so there's nothing
    // to do but return.
    return PR_TRUE;
  }

  nsMouseEvent geckoEvent(PR_TRUE, aMessage, widgetHit, nsMouseEvent::eReal);
  InitializeMouseEvent(geckoEvent, widgetHitPoint, aKeyModifiers, 1);
  widgetHit->DispatchMouseEvent(geckoEvent);

  return PR_TRUE;
}


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

static PRUint32 ConvertMacToRaptorKeyCode(char charCode, UInt32 keyCode, UInt32 eventModifiers)
{
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

// this may clash with VK_INSERT, but help key is more useful in mozilla
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
    PRBool aConvertChar)
{
	//
	// initialize the basic message parts
	//
	aKeyEvent.time				= PR_IntervalNow();
	
	//
	// initialize the GUI event parts
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
  if (aMessage == NS_KEY_PRESS 
		&& !IsSpecialRaptorKey((aOSEvent.message & keyCodeMask) >> 8) )
	{
    if (aKeyEvent.isControl)
		{
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
		aKeyEvent.keyCode = ConvertMacToRaptorKeyCode(aOSEvent.message & charCodeMask, (aOSEvent.message & keyCodeMask) >> 8, aOSEvent.modifiers);
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
  outCMEvent->message = NS_CONTEXTMENU;
  outCMEvent->context = nsMouseEvent::eContextMenuKey;
  outCMEvent->button = nsMouseEvent::eRightButton;
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
  ClearLastMouseUp();

  // The focused widget changed in HandleKeyUpDownEvent, so no NS_KEY_PRESS
  // events should be generated.
  if (mKeyIgnore)
    return PR_FALSE;

  PRBool handled = PR_FALSE;

  // get the focused widget
  nsWindow* focusedWidget = mEventDispatchHandler->GetActive();
  if (!focusedWidget)
    focusedWidget = mTopLevelWidget;
  
  // simulate key press events
  if (!IsSpecialRaptorKey((aOSEvent.message & keyCodeMask) >> 8))
  {
    // it is a message with text, send all the unicode characters
    PRInt32 i;
    for (i = 0; i < charCount; i++)
    {
      nsKeyEvent keyPressEvent(PR_TRUE, NS_KEY_PRESS, nsnull);
      InitializeKeyEvent(keyPressEvent, aOSEvent, focusedWidget, NS_KEY_PRESS, PR_FALSE);
      keyPressEvent.charCode = text[i];

      // If keydown default was prevented, do same for keypress
      if (mKeyHandled)
        keyPressEvent.flags |= NS_EVENT_FLAG_NO_DEFAULT;

      // control key is special in that it doesn't give us letters
      // it generates a charcode of 0x01 for control-a
      // so we offset to do the right thing for gecko
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
        nsMouseEvent contextMenuEvent(PR_TRUE, 0, nsnull, nsMouseEvent::eReal);
        ConvertKeyEventToContextMenuEvent(&keyPressEvent, &contextMenuEvent);
        handled |= focusedWidget->DispatchWindowEvent(contextMenuEvent);
      }
      else {
        // Send ordinary keypresses
        handled |= focusedWidget->DispatchWindowEvent(keyPressEvent);
      }
    }
  }
  else {
    // "Special" keys, only send one event based on the keycode
    nsKeyEvent keyPressEvent(PR_TRUE, NS_KEY_PRESS, nsnull);
    InitializeKeyEvent(keyPressEvent, aOSEvent, focusedWidget, NS_KEY_PRESS, PR_FALSE);
    handled = focusedWidget->DispatchWindowEvent(keyPressEvent);
  }
  return handled;
}

#pragma mark -
//-------------------------------------------------------------------------
//
// HandleActivateEvent
//
//-------------------------------------------------------------------------
void nsMacEventHandler::HandleActivateEvent(EventRef aEvent)
{
  ClearLastMouseUp();

  OSErr err;
  PRUint32 eventKind = ::GetEventKind(aEvent);
  PRBool isActive = (eventKind == kEventWindowActivated) ? PR_TRUE : PR_FALSE;

  // Be paranoid about deactivating the last-active window before activating
  // the new one, and about not handling activation for the already-active
  // window.

	if (isActive && sLastActive != this)
	{
		if (sLastActive) {
			WindowRef oldWindow = NS_STATIC_CAST(WindowRef,
			 sLastActive->mTopLevelWidget->GetNativeData(NS_NATIVE_DISPLAY));
			// Deactivating also causes HandleActivateEvent to
			// be called on sLastActive with isActive = PR_FALSE,
			// so sLastActive will be nsnull after this call.
			::ActivateWindow(oldWindow, PR_FALSE);
		}

		sLastActive = this;

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
		  mEventDispatchHandler->SetActivated(focusedWidget);
		  mTopLevelWidget->SetIsActive(PR_TRUE);
		}
		else if (!mEventDispatchHandler->GetActive()) {
		  NS_ASSERTION(0, "We think we're active, but there is no active widget!");
		  mEventDispatchHandler->SetActivated(focusedWidget);
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

		// Mouse-moved events were not processed while the window was
		// inactive.  Grab the current mouse position and treat it as
		// a mouse-moved event would be.
		EventRecord eventRecord;
		::ConvertEventRefToEventRecord(aEvent, &eventRecord);
		HandleMouseMoveEvent(eventRecord);
	}
	else if (!isActive && sLastActive == this)
	{
		sLastActive = nsnull;

		if (nsnull != gRollupListener && (nsnull != gRollupWidget) ) {
			// If there's a widget to be rolled up, it's got to
			// be attached to the active window, so it's OK to
			// roll it up on any deactivate event without
			// further checking.
			gRollupListener->Rollup();
		}
		//
		// Deactivate the TSMDocument assoicated with this EventHandler
		//
		if (mTSMDocument) {
		  // We should call FixTSMDocument() before deactivate the window.
		  // see http://bugzilla.mozilla.gr.jp/show_bug.cgi?id=4135
		  ResetInputState();
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
		mEventDispatchHandler->SetDeactivated(mTopLevelWidget);
		mTopLevelWidget->SetIsActive(PR_FALSE);
	}
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
	mTopLevelWidget->Resize(macRect.right - macRect.left,
                                macRect.bottom - macRect.top,
                                PR_FALSE,
                                PR_TRUE); // resize came from the UI via event
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
nsMacEventHandler::Scroll(PRInt32 aDeltaY, PRInt32 aDeltaX,
                          PRBool aIsPixels, const Point& aMouseLoc,
                          nsWindow* aWindow, PRUint32 aModifiers) {
  PRBool resY = ScrollAxis(nsMouseScrollEvent::kIsVertical, aDeltaY,
                           aIsPixels, aMouseLoc, aWindow, aModifiers);
  PRBool resX = ScrollAxis(nsMouseScrollEvent::kIsHorizontal, aDeltaX,
                           aIsPixels, aMouseLoc, aWindow, aModifiers);

  return resY || resX;
} // Scroll


//
// ScrollAxis
//
PRBool
nsMacEventHandler::ScrollAxis(nsMouseScrollEvent::nsMouseScrollFlags aAxis,
                              PRInt32 aDelta, PRBool aIsPixels,
                              const Point& aMouseLoc, nsWindow* aWindow,
                              PRUint32 aModifiers)
{
  // Only scroll active windows.  Treat popups as active.
  WindowRef windowRef = NS_STATIC_CAST(WindowRef,
                         mTopLevelWidget->GetNativeData(NS_NATIVE_DISPLAY));
  nsWindowType windowType;
  mTopLevelWidget->GetWindowType(windowType);
  if (!::IsWindowActive(windowRef) && windowType != eWindowType_popup)
    return PR_FALSE;

  // Figure out which widget should be scrolled by traversing the widget
  // hierarchy beginning at the root nsWindow.  aMouseLoc should be
  // relative to the origin of this nsWindow.  If the scroll event came
  // from an nsMacWindow, then aWindow should refer to that nsMacWindow.
  nsIWidget* widgetToScroll = aWindow->FindWidgetHit(aMouseLoc);

  // Not all scroll events for the window are over a widget.  Consider
  // the title bar.
  if (!widgetToScroll)
    return PR_FALSE;

  if (aDelta == 0) {
    // Don't need to do anything, but eat the event anyway.
    return PR_TRUE;
  }

  if (gRollupListener && gRollupWidget) {
    // Roll up the rollup widget if the scroll isn't targeted at it
    // (or one of its children) and the listener was told to do so.

    PRBool rollup = PR_FALSE;
    gRollupListener->ShouldRollupOnMouseWheelEvent(&rollup);

    if (rollup) {
      nsCOMPtr<nsIWidget> widgetOrAncestor = widgetToScroll;
      do {
        if (widgetOrAncestor == gRollupWidget) {
          rollup = PR_FALSE;
          break;
        }
      } while (widgetOrAncestor = widgetOrAncestor->GetParent());
    }

    if (rollup)
      gRollupListener->Rollup();
  }

  nsMouseScrollEvent scrollEvent(PR_TRUE, NS_MOUSE_SCROLL, widgetToScroll);

  // The direction we get from the carbon event is opposite from the way
  // mozilla looks at it.  Reverse the direction.
  scrollEvent.delta = -aDelta;

  // If the scroll event comes from a mouse that only has a scroll wheel for
  // the vertical axis, and the shift key is held down, the system presents
  // it as a horizontal scroll and doesn't clear the shift key bit from
  // aModifiers.  The Mac is supposed to scroll horizontally in such a case.
  //
  // If the scroll event comes from a mouse that can scroll both axes, the
  // system doesn't apply any of this shift-key fixery.
  scrollEvent.scrollFlags = aAxis;

  if (aIsPixels)
    scrollEvent.scrollFlags |= nsMouseScrollEvent::kIsPixels;

  // convert window-relative (local) mouse coordinates to widget-relative
  // coords for Gecko.
  nsPoint mouseLocRelativeToWidget(aMouseLoc.h, aMouseLoc.v);
  nsRect bounds;
  widgetToScroll->GetBounds(bounds);
  nsPoint widgetOrigin(bounds.x, bounds.y);
  widgetToScroll->ConvertToDeviceCoordinates(widgetOrigin.x, widgetOrigin.y);
  mouseLocRelativeToWidget.MoveBy(-widgetOrigin.x, -widgetOrigin.y);

  scrollEvent.refPoint.x = mouseLocRelativeToWidget.x;
  scrollEvent.refPoint.y = mouseLocRelativeToWidget.y;
  scrollEvent.time = PR_IntervalNow();

  // Translate OS event modifiers into Gecko event modifiers
  scrollEvent.isShift   = ((aModifiers & shiftKey)   != 0);
  scrollEvent.isControl = ((aModifiers & controlKey) != 0);
  scrollEvent.isAlt     = ((aModifiers & optionKey)  != 0);
  scrollEvent.isMeta    = ((aModifiers & cmdKey)     != 0);

  nsEventStatus status;
  widgetToScroll->DispatchEvent(&scrollEvent, status);

  return nsWindow::ConvertStatus(status);
} // ScrollAxis


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
			retVal = PR_TRUE;
			break;
		}

		case inGrow:
		{
      ResizeEvent ( whichWindow );
      retVal = PR_TRUE;
      break;
		}

		case inGoAway:
		{
			ResetInputState();	// IM:TEXT 7-23 said we need to call FixTSMDocument when we go away...
			if (nsnull != gRollupListener && (nsnull != gRollupWidget) ) {
				gRollupListener->Rollup();
			}
			mEventDispatchHandler->DispatchGuiEvent(mTopLevelWidget, NS_XUL_CLOSE);		
			// mTopLevelWidget->Destroy(); (this, by contrast, would immediately close the window)
			retVal = PR_TRUE;
			break;
		}

		case inContent:
		{
			// don't allow clicks that rolled up a popup through to the content area.
			if ( ignoreClickInContent )
				break;
						
			nsMouseEvent mouseEvent(PR_TRUE, NS_MOUSE_BUTTON_DOWN, nsnull,
                              nsMouseEvent::eReal);
			mouseEvent.button = nsMouseEvent::eLeftButton;
			if ( aOSEvent.modifiers & controlKey )
			  mouseEvent.button = nsMouseEvent::eRightButton;

			// We've hacked our events to include the button.
			// Normally message is undefined in mouse click/drag events.
			if ( aOSEvent.message == kEventMouseButtonSecondary )
			  mouseEvent.button = nsMouseEvent::eRightButton;
			if ( aOSEvent.message == kEventMouseButtonTertiary )
			  mouseEvent.button = nsMouseEvent::eMiddleButton;

			ConvertOSEventToMouseEvent(aOSEvent, mouseEvent);

			nsCOMPtr<nsIWidget> kungFuDeathGrip ( mouseEvent.widget );            // ensure widget doesn't go away
			nsWindow* widgetHit = NS_STATIC_CAST(nsWindow*, mouseEvent.widget);   //   while we're processing event
			if (widgetHit)
			{        
				// set the activation and focus on the widget hit, if it accepts it
				{
					nsMouseEvent mouseActivateEvent(PR_TRUE, NS_MOUSE_ACTIVATE, nsnull,
                                          nsMouseEvent::eReal);
          ConvertOSEventToMouseEvent(aOSEvent, mouseActivateEvent);
					widgetHit->DispatchMouseEvent(mouseActivateEvent);
				}

				// dispatch the event
				retVal = widgetHit->DispatchMouseEvent(mouseEvent);
				
				// if we're a control-click, send in an additional NS_CONTEXTMENU event
				// after the mouse down.
				if (mouseEvent.button == nsMouseEvent::eRightButton) {
    			nsMouseEvent contextMenuEvent(PR_TRUE, NS_CONTEXTMENU, nsnull,
                                        nsMouseEvent::eReal);
    			ConvertOSEventToMouseEvent(aOSEvent, contextMenuEvent);
    			contextMenuEvent.isControl = PR_FALSE;    			
					widgetHit->DispatchMouseEvent(contextMenuEvent);
        } 

        // If we found a widget to dispatch to, say we handled the event.
        // The meaning of the result of DispatchMouseEvent() is ambiguous.
        // In Gecko terms, it means "continue processing", but that doesn't
        // say if the event was really handled (which is a simplistic notion
        // to Gecko).
        retVal = PR_TRUE;
			}
						
			mEventDispatchHandler->SetWidgetHit(widgetHit);
			mMouseInWidgetHit = PR_TRUE;
			break;
		}


		case inZoomIn:
		case inZoomOut:
		{
			mEventDispatchHandler->DispatchSizeModeEvent(mTopLevelWidget,
				partCode == inZoomIn ? nsSizeMode_Normal : nsSizeMode_Maximized);
			retVal = PR_TRUE;
			break;
		}

    case inToolbarButton:           // we get this part on Mac OS X only
      mEventDispatchHandler->DispatchGuiEvent(mTopLevelWidget, NS_OS_TOOLBAR);		
      retVal = PR_TRUE;
      break;
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

	nsMouseEvent mouseEvent(PR_TRUE, NS_MOUSE_BUTTON_UP, nsnull,
                          nsMouseEvent::eReal);
	mouseEvent.button = nsMouseEvent::eLeftButton;

	// We've hacked our events to include the button.
	// Normally message is undefined in mouse click/drag events.
	if ( aOSEvent.message == kEventMouseButtonSecondary )
		mouseEvent.button = nsMouseEvent::eRightButton;
	if ( aOSEvent.message == kEventMouseButtonTertiary )
		mouseEvent.button = nsMouseEvent::eMiddleButton;

	ConvertOSEventToMouseEvent(aOSEvent, mouseEvent);

	nsWindow* widgetReleased = (nsWindow*)mouseEvent.widget;
	nsWindow* widgetHit = mEventDispatchHandler->GetWidgetHit();

	if ( widgetReleased )
	{
		widgetReleased->DispatchMouseEvent(mouseEvent);
		// If we found a widget to dispatch the event to, say that we handled it
		// (see comments in HandleMouseDownEvent()).
		retVal = PR_TRUE;
	}
	
	if ( widgetReleased != widgetHit ) {
	  //XXX we should send a mouse exit event to the last widget, right?!?! But
	  //XXX we cannot use the same event, because the coordinates we just
	  //XXX computed are in the wrong window/widget coordinate space. I'm 
	  //XXX unclear what we should do in this case. (pinkerton).
	}
	
	mEventDispatchHandler->SetWidgetHit(nsnull);

	return retVal;
}


//-------------------------------------------------------------------------
//
// HandleMouseMoveEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleMouseMoveEvent( EventRecord& aOSEvent )
{
	nsWindow* lastWidgetHit = mEventDispatchHandler->GetWidgetHit();
	nsWindow* lastWidgetPointed = mEventDispatchHandler->GetWidgetPointed();
  
	PRBool retVal = PR_FALSE;

	WindowRef wind = reinterpret_cast<WindowRef>(mTopLevelWidget->GetNativeData(NS_NATIVE_DISPLAY));
	nsWindowType windowType;
	mTopLevelWidget->GetWindowType(windowType);
	if (!::IsWindowActive(wind) && windowType != eWindowType_popup)
		return retVal;

	nsMouseEvent mouseEvent(PR_TRUE, NS_MOUSE_MOVE, nsnull, nsMouseEvent::eReal);
	ConvertOSEventToMouseEvent(aOSEvent, mouseEvent);
	if (lastWidgetHit)
	{
		Point macPoint = aOSEvent.where;
		nsGraphicsUtils::SafeSetPortWindowPort(wind);
		{
			StOriginSetter  originSetter(wind);
			::GlobalToLocal(&macPoint);
		}
		PRBool inWidgetHit = lastWidgetHit->PointInWidget(macPoint);
		if (mMouseInWidgetHit != inWidgetHit)
		{
			mMouseInWidgetHit = inWidgetHit;
			mouseEvent.message = (inWidgetHit ? NS_MOUSE_ENTER : NS_MOUSE_EXIT);
		}
		lastWidgetHit->DispatchMouseEvent(mouseEvent);
		retVal = PR_TRUE;
	}
	else
	{
		nsWindow* widgetPointed = (nsWindow*)mouseEvent.widget;
		nsCOMPtr<nsIWidget> kungFuDeathGrip(widgetPointed);     // Protect during processing
		if (widgetPointed != lastWidgetPointed)
		{
			if (lastWidgetPointed)
			{
        // We need to convert the coords to be relative to lastWidgetPointed.
        nsPoint widgetHitPoint = mouseEvent.refPoint;

        Point macPoint = aOSEvent.where;
        nsGraphicsUtils::SafeSetPortWindowPort(wind);

        {
          StOriginSetter originSetter(wind);
          ::GlobalToLocal(&macPoint);
        }

        nsPoint lastWidgetHitPoint(macPoint.h, macPoint.v);

          nsRect bounds;
          lastWidgetPointed->GetBounds(bounds);
          nsPoint widgetOrigin(bounds.x, bounds.y);
          lastWidgetPointed->LocalToWindowCoordinate(widgetOrigin);
          lastWidgetHitPoint.MoveBy(-widgetOrigin.x, -widgetOrigin.y);
				mouseEvent.widget = lastWidgetPointed;
				mouseEvent.refPoint = lastWidgetHitPoint;
				mouseEvent.message = NS_MOUSE_EXIT;
				lastWidgetPointed->DispatchMouseEvent(mouseEvent);
				retVal = PR_TRUE;

				mouseEvent.refPoint = widgetHitPoint;
			}

      mEventDispatchHandler->SetWidgetPointed(widgetPointed);
#if TRACK_MOUSE_LOC
      mEventDispatchHandler->SetGlobalPoint(aOSEvent.where);
#endif

			if (widgetPointed)
			{
				mouseEvent.widget = widgetPointed;
				mouseEvent.message = NS_MOUSE_ENTER;
				widgetPointed->DispatchMouseEvent(mouseEvent);
				retVal = PR_TRUE;
			}
		}
		else
		{
			if (widgetPointed)
			{
				widgetPointed->DispatchMouseEvent(mouseEvent);
				retVal = PR_TRUE;
			}
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
														nsMouseEvent&		aMouseEvent)
{
	// we're going to time double-clicks from mouse *up* to next mouse *down*
	if (aMouseEvent.message == NS_MOUSE_BUTTON_UP)
	{
		// remember when this happened for the next mouse down
		mLastMouseUpWhen = aOSEvent.when;
		mLastMouseUpWhere = aOSEvent.where;
	}
	else if (aMouseEvent.message == NS_MOUSE_BUTTON_DOWN)
	{
		// now look to see if we want to convert this to a double- or triple-click
		const short kDoubleClickMoveThreshold	= 5;
		
		if (((aOSEvent.when - mLastMouseUpWhen) < ::GetDblTime()) &&
				(((abs(aOSEvent.where.h - mLastMouseUpWhere.h) < kDoubleClickMoveThreshold) &&
				 	(abs(aOSEvent.where.v - mLastMouseUpWhere.v) < kDoubleClickMoveThreshold))))
		{		
			mClickCount ++;
			
//			if (mClickCount == 2)
//				aMessage = NS_MOUSE_DOUBLECLICK;
		}
		else
		{
			// reset the click count, to count *this* click
			mClickCount = 1;
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
	nsWindow* lastWidgetHit = mEventDispatchHandler->GetWidgetHit();
	nsWindow* widgetHit = nsnull;
	if (lastWidgetHit)
	{
        // make sure we in the same window as where we started before we go assuming
        // that we know where the event will go.
        WindowRef   lastWind = reinterpret_cast<WindowRef>(lastWidgetHit->GetNativeData(NS_NATIVE_DISPLAY));
        PRBool      eventInSameWindowAsLastEvent = (windowThatHasEvent == lastWind);
        if ( eventInSameWindowAsLastEvent || !topLevelIsAPopup ) {	  
            if (::StillDown() || (aMouseEvent.message == NS_MOUSE_BUTTON_UP && 
                aMouseEvent.button == nsMouseEvent::eLeftButton))
            {
                widgetHit = lastWidgetHit;
                eventTargetWindow = lastWind;   // make sure we use the correct window to fix the coords
            }
            else
            {
                // Some widgets can eat mouseUp events (text widgets in TEClick, sbars in TrackControl).
                // In that case, stop considering this widget as being still hit.
                mEventDispatchHandler->SetWidgetHit(nsnull);
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

    InitializeMouseEvent(aMouseEvent, widgetHitPoint, aOSEvent.modifiers,
                         mClickCount);
    // nsGUIEvent
    aMouseEvent.widget      = widgetHit;
    aMouseEvent.nativeMsg   = (void*)&aOSEvent;

    // nsMouseEvent
    aMouseEvent.acceptActivation = PR_TRUE;
    if (aMouseEvent.message == NS_CONTEXTMENU)
      aMouseEvent.button = nsMouseEvent::eRightButton;
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
    PRInt32 i;
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
  nsWindow* focusedWidget = mEventDispatchHandler->GetActive();
  if (!focusedWidget)
    focusedWidget = mTopLevelWidget;

  nsReconversionEvent reconversionEvent(PR_TRUE, NS_RECONVERSION_QUERY,
                                        focusedWidget);
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
	nsWindow* focusedWidget = mEventDispatchHandler->GetActive();
	if (!focusedWidget)
		focusedWidget = mTopLevelWidget;
	
	//
	// create the nsCompositionEvent
	//
	nsCompositionEvent compositionEvent(PR_TRUE, NS_COMPOSITION_START,
                                      focusedWidget);
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
	nsWindow* focusedWidget = mEventDispatchHandler->GetActive();
	if (!focusedWidget)
		focusedWidget = mTopLevelWidget;
	
	//
	// create the nsCompositionEvent
	//
	nsCompositionEvent compositionEvent(PR_TRUE, NS_COMPOSITION_END,
                                      focusedWidget);
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
	nsWindow* focusedWidget = mEventDispatchHandler->GetActive();
	if (!focusedWidget)
		focusedWidget = mTopLevelWidget;
	
	//
	// create the nsTextEvent
	//
	nsTextEvent textEvent(PR_TRUE, NS_TEXT_TEXT, focusedWidget);
	textEvent.time = PR_IntervalNow();
	textEvent.theText = mIMECompositionStr->get();
	textEvent.rangeCount = textRangeCount;
	textEvent.rangeArray = textRangeArray;

	nsresult res = NS_OK;
	if (NS_SUCCEEDED(res = focusedWidget->DispatchWindowEvent(textEvent))) {
		mIMEPos.x = textEvent.theReply.mCursorPosition.x;
		mIMEPos.y = textEvent.theReply.mCursorPosition.y +
		            textEvent.theReply.mCursorPosition.height;
		mTopLevelWidget->LocalToWindowCoordinate(mIMEPos);
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

PRBool
nsMacEventHandler::HandleKeyUpDownEvent(EventHandlerCallRef aHandlerCallRef,
                                        EventRef aEvent)
{
  ClearLastMouseUp();

  PRUint32 eventKind = ::GetEventKind(aEvent);
  NS_ASSERTION(eventKind == kEventRawKeyDown ||
               eventKind == kEventRawKeyUp,
               "Unknown event kind");

  OSStatus err = noErr;

  PRBool sendToTSM = PR_FALSE;
  if (eventKind == kEventRawKeyDown) {
    if (mTSMDocument != ::TSMGetActiveDocument()) {
      // If some TSM document other than the one that we use for this window
      // is active, first try to call through to the TSM handler during a
      // keydown.  This can happen if a plugin installs its own TSM handler
      // and activates its own TSM document.
      // This is done early, before dispatching NS_KEY_DOWN because the
      // plugin will receive the keyDown event carried in the NS_KEY_DOWN.
      // If an IME session is active, this could cause the plugin to accept
      // both raw keydowns and text input from the IME session as input.
      err = ::CallNextEventHandler(aHandlerCallRef, aEvent);
      if (err == noErr) {
        // Someone other than us handled the event.  Don't send NS_KEY_DOWN.
        return PR_TRUE;
      }

      // No foreign handlers did anything.  Leave sendToTSM false so that
      // no subsequent attempts to call through to the foreign handler will
      // be made.
    }
    else {
      // The TSM document matches the one corresponding to the window.
      // An NS_KEY_DOWN event will be sent, and after that, it will be put
      // back into the handler chain to go to the TSM input handlers.
      // This assumes that the TSM handler is still ours, or that if
      // something else changed the TSM handler, that the new handler will
      // at least call through to ours.
      sendToTSM = PR_TRUE;
    }
  }

  PRBool handled = PR_FALSE;
  nsWindow* focusedWidget = mEventDispatchHandler->GetActive();
  if (!focusedWidget)
    focusedWidget = mTopLevelWidget;

  PRUint32 modifiers = 0;
  err = ::GetEventParameter(aEvent, kEventParamKeyModifiers,
                            typeUInt32, NULL,
                            sizeof(modifiers), NULL,
                            &modifiers);
  NS_ASSERTION(err == noErr, "Could not get kEventParamKeyModifiers");

  PRUint32 keyCode = 0;
  err = ::GetEventParameter(aEvent, kEventParamKeyCode,
                            typeUInt32, NULL,
                            sizeof(keyCode), NULL,
                            &keyCode);
  NS_ASSERTION(err == noErr, "Could not get kEventParamKeyCode");

  PRUint8 charCode = 0;
  ::GetEventParameter(aEvent, kEventParamKeyMacCharCodes,
                      typeChar, NULL,
                      sizeof(charCode), NULL,
                      &charCode);
  // Failure is not a fatal condition.

  // The event's nativeMsg field historically held an EventRecord.  Some
  // consumers (plugins) rely on this behavior.  Note that
  // ConvertEventRefToEventRecord can return false and produce a null
  // event record in some cases, such as when entering an IME session.
  EventRecord eventRecord;
  ::ConvertEventRefToEventRecord(aEvent, &eventRecord);

  // kEventRawKeyDown or kEventRawKeyUp only

  PRUint32 message = (eventKind == kEventRawKeyUp ? NS_KEY_UP : NS_KEY_DOWN);
  nsKeyEvent upDownEvent(PR_TRUE, message, nsnull);
  upDownEvent.time =      PR_IntervalNow();
  upDownEvent.widget =    focusedWidget;
  upDownEvent.nativeMsg = (void*)&eventRecord;
  upDownEvent.isShift =   ((modifiers & shiftKey) != 0);
  upDownEvent.isControl = ((modifiers & controlKey) != 0);
  upDownEvent.isAlt =     ((modifiers & optionKey) != 0);
  upDownEvent.isMeta =    ((modifiers & cmdKey) != 0);
  upDownEvent.keyCode =   ConvertMacToRaptorKeyCode(charCode, keyCode,
                                                    modifiers);
  upDownEvent.charCode =  0;
  handled = focusedWidget->DispatchWindowEvent(upDownEvent);

  if (eventKind == kEventRawKeyUp)
    return handled;

  // kEventRawKeyDown only.  Prepare for a possible NS_KEY_PRESS event.

  nsWindow* checkFocusedWidget = mEventDispatchHandler->GetActive();
  if (!checkFocusedWidget)
    checkFocusedWidget = mTopLevelWidget;

  // Set a flag indicating that NS_KEY_PRESS events should not be dispatched,
  // because focus changed.
  PRBool lastIgnore = PR_FALSE;
  if (checkFocusedWidget != focusedWidget) {
    lastIgnore = mKeyIgnore;
    mKeyIgnore = PR_TRUE;
  }

  // Set a flag indicating that the NS_KEY_DOWN event came back with
  // preventDefault, and NS_KEY_PRESS events should have the same flag set.
  PRBool lastHandled = PR_FALSE;
  if (handled) {
    lastHandled = mKeyHandled;
    mKeyHandled = PR_TRUE;
  }

  if (sendToTSM) {
    // The event needs further processing.
    //  - If no input method is active, an event will be delivered to the
    //    kEventTextInputUnicodeForKeyEvent handler, which will call
    //    HandleUKeyEvent, which takes care of dispatching NS_KEY_PRESS events.
    //  - If an input method is active, an event will be delivered to the
    //    kEventTextInputUpdateActiveInputArea handler, which will call
    //    UnicodeHandleUpdateInputArea to handle the input session.
    ::CallNextEventHandler(aHandlerCallRef, aEvent);
  }

  mKeyHandled = lastHandled;
  mKeyIgnore = lastIgnore;

  return handled;
}

PRBool
nsMacEventHandler::HandleKeyModifierEvent(EventHandlerCallRef aHandlerCallRef,
                                          EventRef aEvent)
{
  ClearLastMouseUp();

  PRBool handled = PR_FALSE;
  nsWindow* focusedWidget = mEventDispatchHandler->GetActive();
  if (!focusedWidget)
    focusedWidget = mTopLevelWidget;

  PRUint32 modifiers = 0;
  OSStatus err = ::GetEventParameter(aEvent, kEventParamKeyModifiers,
                                     typeUInt32, NULL,
                                     sizeof(modifiers), NULL,
                                     &modifiers);
  NS_ASSERTION(err == noErr, "Could not get kEventParamKeyModifiers");

  typedef struct {
    PRUint32 modifierBit;
    PRUint32 keycode;
  } ModifierToKeycode;
  const ModifierToKeycode kModifierToKeycodeTable[] = {
    { shiftKey,   NS_VK_SHIFT },
    { controlKey, NS_VK_CONTROL },
    { optionKey,  NS_VK_ALT },
    { cmdKey,     NS_VK_META },
  };
  const PRUint32 kModifierCount = sizeof(kModifierToKeycodeTable) /
                                  sizeof(ModifierToKeycode);

  // This will be a null event, but include it anyway because it'll still have
  // good when, where, and modifiers fields, and because it's present in other
  // NS_KEY_DOWN and NS_KEY_UP events.
  EventRecord eventRecord;
  ::ConvertEventRefToEventRecord(aEvent, &eventRecord);

  for(PRUint32 i = 0 ; i < kModifierCount ; i++) {
    PRUint32 modifierBit = kModifierToKeycodeTable[i].modifierBit;
    if ((modifiers & modifierBit) != (mLastModifierState & modifierBit)) {
      PRUint32 message = ((modifiers & modifierBit) != 0 ? NS_KEY_DOWN :
                                                           NS_KEY_UP);
      nsKeyEvent upDownEvent(PR_TRUE, message, nsnull);
      upDownEvent.time =      PR_IntervalNow();
      upDownEvent.widget =    focusedWidget;
      upDownEvent.nativeMsg = (void*)&eventRecord;
      upDownEvent.isShift =   ((modifiers & shiftKey) != 0);
      upDownEvent.isControl = ((modifiers & controlKey) != 0);
      upDownEvent.isAlt =     ((modifiers & optionKey) != 0);
      upDownEvent.isMeta =    ((modifiers & cmdKey) != 0);
      upDownEvent.keyCode =   kModifierToKeycodeTable[i].keycode;
      upDownEvent.charCode =  0;
      handled |= focusedWidget->DispatchWindowEvent(upDownEvent);

      // No need to preventDefault additional events.  Although
      // kEventRawKeyModifiersChanged can carry multiple modifiers going
      // up or down, Gecko treats them independently.
 
      // Stop if focus has changed.
      nsWindow* checkFocusedWidget = mEventDispatchHandler->GetActive();
      if (!checkFocusedWidget)
        checkFocusedWidget = mTopLevelWidget;

      if (checkFocusedWidget != focusedWidget)
        break;
    }
  }

  mLastModifierState = modifiers;
  return handled;
}

void
nsMacEventHandler::ClearLastMouseUp()
{
  mLastMouseUpWhere.h = 0;
  mLastMouseUpWhere.v = 0;
  mLastMouseUpWhen = 0;
  mClickCount = 0;
}
