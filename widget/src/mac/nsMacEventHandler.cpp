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

#include <LowMem.h>
#include "nsMacEventHandler.h"
#include "nsWindow.h"
#include "nsToolkit.h"
#include "prinrval.h"

// from MacHeaders.c
#ifndef topLeft
	#define topLeft(r)	(((Point *) &(r))[0])
#endif
#ifndef botRight
	#define botRight(r)	(((Point *) &(r))[1])
#endif

PRBool	nsMacEventHandler::mInBackground = PR_FALSE;

//-------------------------------------------------------------------------
//
// nsMacEventHandler constructor/destructor
//
//-------------------------------------------------------------------------
nsMacEventHandler::nsMacEventHandler(nsWindow* aTopLevelWidget)
{
	mTopLevelWidget			= aTopLevelWidget;
	mLastWidgetHit			= nsnull;
	mLastWidgetPointed	= nsnull;
}


nsMacEventHandler::~nsMacEventHandler()
{
	if (mLastWidgetPointed)
		mLastWidgetPointed->RemoveDeleteObserver(this);

	if (mLastWidgetHit)
		mLastWidgetHit->RemoveDeleteObserver(this);
}


void nsMacEventHandler::NotifyDelete(void* aDeletedObject)
{
	if (mLastWidgetPointed == aDeletedObject)
		mLastWidgetPointed = nsnull;

	if (mLastWidgetHit == aDeletedObject)
		mLastWidgetHit = nsnull;
}


#pragma mark -
//-------------------------------------------------------------------------
//
// HandleOSEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleOSEvent(
															EventRecord&		aOSEvent)
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
			retVal = HandleUpdateEvent(aOSEvent);
			break;

		case mouseDown:
			retVal = HandleMouseDownEvent(aOSEvent);
			break;

		case mouseUp:
			retVal = HandleMouseUpEvent(aOSEvent);
			break;

		case osEvt:
			unsigned char eventType = ((aOSEvent.message >> 24) & 0x00ff);
			if (eventType == suspendResumeMessage)
			{
					if ((aOSEvent.message & 1) == resumeFlag)
						mInBackground = PR_FALSE;		// resume message
					else
						mInBackground = PR_TRUE;		// suspend message
					break;
			}
			// no break;
	
		case nullEvent:
			if (! mInBackground)
				retVal = HandleMouseMoveEvent(aOSEvent);
			break;
	}

	return retVal;
}

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
	nsWindow* focusedWidget = mTopLevelWidget;
	nsCOMPtr<nsToolkit> toolkit ( dont_AddRef((nsToolkit*)mTopLevelWidget->GetToolkit()) );
	if (toolkit)
		focusedWidget = toolkit->GetFocus();
	
	// nsEvent
	nsMenuEvent menuEvent;
	menuEvent.eventStructType = NS_MENU_EVENT;
	menuEvent.message		= NS_MENU_SELECTED;
	menuEvent.point.x		= aOSEvent.where.h;
	menuEvent.point.y		= aOSEvent.where.v;
	menuEvent.time			= PR_IntervalNow();

	// nsGUIEvent
	menuEvent.widget		= focusedWidget;
	menuEvent.nativeMsg	= (void*)&aOSEvent;

	// nsMenuEvent
	menuEvent.mMenuItem	= nsnull;						//¥TODO: initialize mMenuItem
	menuEvent.mCommand	= aMenuResult;

	// dispatch the menu event: if it is not processed by the focused widget,
	// propagate the event through the different parents all the way up to the window
	PRBool eventHandled = focusedWidget->DispatchWindowEvent(menuEvent);
	
	if (! eventHandled)
	{
		nsCOMPtr<nsWindow> grandParent;
		nsCOMPtr<nsWindow> parent ( dont_AddRef((nsWindow*)focusedWidget->GetParent()) );
		while (parent)
		{
			menuEvent.widget = parent;
			eventHandled = parent->DispatchWindowEvent(menuEvent);
			if (eventHandled)
			{
				break;
			}
			else
			{
				grandParent = dont_AddRef((nsWindow*)parent->GetParent());
				parent = grandParent;
			}
		}
	}

	return eventHandled;
}


#pragma mark -

//-------------------------------------------------------------------------
//
// HandleKeyEvent
//
//-------------------------------------------------------------------------

#define homeKey				0x01		/* ascii code for home key */
#define enterKey			0x03		/* ascii code for enter key */
#define endKey				0x04		/* ascii code for end key */
#define helpKey				0x05		/* ascii code for help key */
#define deleteKey			0x08		/* ascii code for delete/backspace */
#define tabKey				0x09		/* ascii code for tab key */
#define pageUpKey			0x0B		/* ascii code for page up key */
#define pageDownKey		0x0C		/* ascii code for page down key */
#define returnKey			0x0D		/* ascii code for return key */
#define leftArrow			0x1C		/* ascii code for left arrow key */
#define rightArrow		0x1D		/* ascii code for right arrow key */
#define upArrow				0x1E			/* ascii code for up arrow key */
#define downArrow			0x1F			/* ascii code for down arrow key */
#define forwardDelKey	0x7F			/* ascii code for forward delete key */
#define spaceKey			0x20		/* ascii code for a space */

#define escapeKeyCode		0x35		/* key code for escape key */
#define clearKeyCode		0x47		/* key code for clear key */

static PRUint32 ConvertMacToRaptorKeyCode(UInt32 eventMessage, UInt32 eventModifiers)
{
	UInt8			charCode = (eventMessage & charCodeMask);
	UInt8			keyCode = (eventMessage & keyCodeMask) >> 8;
	PRUint32	raptorKeyCode;
	
	// temporary hack until we figure out the key handling strategy (sfraser)
	
	if (charCode >= ' ' && charCode <= '~')			// ~ is 0x7E
	{
		raptorKeyCode = charCode;
	}
	else
	{
		switch (charCode)
		{
			case homeKey:					raptorKeyCode = NS_VK_HOME;				break;
			case enterKey:				raptorKeyCode = NS_VK_RETURN;			break;			// fix me!
			case endKey:					raptorKeyCode = NS_VK_END;				break;
		//case helpKey:					raptorKeyCode = ;			break;
			case deleteKey:				raptorKeyCode = NS_VK_DELETE;			break;
			case tabKey:					raptorKeyCode = NS_VK_TAB;				break;

			case pageUpKey:				raptorKeyCode = NS_VK_PAGE_UP;		break;
			case pageDownKey:			raptorKeyCode = NS_VK_PAGE_DOWN;	break;
			case returnKey:				raptorKeyCode = NS_VK_RETURN;			break;

			case leftArrow:				raptorKeyCode = NS_VK_LEFT;				break;
			case rightArrow:			raptorKeyCode = NS_VK_RIGHT;			break;
			case upArrow:					raptorKeyCode = NS_VK_UP;					break;
			case downArrow:				raptorKeyCode = NS_VK_DOWN;				break;

			case escapeKeyCode:		raptorKeyCode = NS_VK_ESCAPE;			break;
			case clearKeyCode:		raptorKeyCode = NS_VK_CLEAR;			break;
		}
	}

	return raptorKeyCode;
}

//-------------------------------------------------------------------------
//
// HandleKeyEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleKeyEvent(EventRecord& aOSEvent)
{
	// get the focused widget
	nsWindow* focusedWidget = mTopLevelWidget;
	nsCOMPtr<nsToolkit> toolkit ( dont_AddRef((nsToolkit*)mTopLevelWidget->GetToolkit()) );
	if (toolkit)
		focusedWidget = toolkit->GetFocus();
	
	// nsEvent
	nsKeyEvent	keyEvent;
	keyEvent.eventStructType = NS_KEY_EVENT;
	switch (aOSEvent.what)
	{
		case keyUp:		keyEvent.message = NS_KEY_UP;			break;
		case keyDown:	keyEvent.message = NS_KEY_DOWN;		break;
		case autoKey:	keyEvent.message = NS_KEY_DOWN;		break;
	}
	keyEvent.point.x		= 0;
	keyEvent.point.y		= 0;
	keyEvent.time				= PR_IntervalNow();

	// nsGUIEvent
	keyEvent.widget			= focusedWidget;
	keyEvent.nativeMsg	= (void*)&aOSEvent;

	// nsInputEvent
	keyEvent.isShift		= ((aOSEvent.modifiers & shiftKey) != 0);
	keyEvent.isControl	= ((aOSEvent.modifiers & controlKey) != 0);
	keyEvent.isAlt			= ((aOSEvent.modifiers & optionKey) != 0);
	keyEvent.isCommand	= ((aOSEvent.modifiers & cmdKey) != 0);

	// nsKeyEvent
  keyEvent.keyCode		= ConvertMacToRaptorKeyCode(aOSEvent.message, aOSEvent.modifiers);
    keyEvent.charCode    = (aOSEvent.message & charCodeMask);

	return(focusedWidget->DispatchWindowEvent(keyEvent));
}


//-------------------------------------------------------------------------
//
// HandleActivateEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleActivateEvent(EventRecord& aOSEvent)
{
	nsCOMPtr<nsToolkit> toolkit ( dont_AddRef((nsToolkit*)mTopLevelWidget->GetToolkit()) );
	if (toolkit)
	{
		Boolean isActive = ((aOSEvent.modifiers & activeFlag) != 0);
		if (isActive)
		{
			//¥TODO: retrieve the focused widget for that window
			nsWindow*	focusedWidget = mTopLevelWidget;
			toolkit->SetFocus(focusedWidget);
			nsIMenuBar* menuBar = focusedWidget->GetMenuBar();
			if (menuBar)
			{
			  MenuHandle menuHandle = nsnull;

			  menuBar->GetNativeData((void *)menuHandle);
			  ::SetMenuBar((Handle)menuHandle);
			  menuBar->Paint();
			}
			else
			{
			  //¥TODO:	if the focusedWidget doesn't have a menubar,
			  //				look all the way up to the window
			  //				until one of the parents has a menubar
			}

			//¥TODO: set the menu bar here
		}
		else
		{
			//¥TODO: save the focused widget for that window
			toolkit->SetFocus(nsnull);
	
			nsIMenuBar* menuBarInterface = mTopLevelWidget->GetMenuBar();
			if (menuBarInterface)
			{
				Handle menuBar = ::GetMenuBar(); // Get a copy of the menu list
				menuBarInterface->SetNativeData((void*)menuBar);
			}
		}
	}
	return PR_TRUE;
}


//-------------------------------------------------------------------------
//
// HandleUpdateEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleUpdateEvent(EventRecord& aOSEvent)
{
	mTopLevelWidget->HandleUpdateEvent();

	return PR_TRUE;
}


//-------------------------------------------------------------------------
//
// HandleMouseDownEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleMouseDownEvent(
														EventRecord&		aOSEvent)
{
	PRBool retVal = PR_FALSE;

	WindowPtr		whichWindow;
	short partCode = ::FindWindow(aOSEvent.where, &whichWindow);

	switch (partCode)
	{
		case inDrag:
		{
			Point macPoint;
			macPoint = topLeft(whichWindow->portRect);
			::LocalToGlobal(&macPoint);
			mTopLevelWidget->Move(macPoint.h, macPoint.v);
			break;
		}

		case inGrow:
		{
			Rect macRect = whichWindow->portRect;
			::LocalToGlobal(&topLeft(macRect));
			::LocalToGlobal(&botRight(macRect));
			mTopLevelWidget->Resize(macRect.right - macRect.left, macRect.bottom - macRect.top, PR_FALSE);
			break;
		}

		case inGoAway:
		{
			mTopLevelWidget->Destroy();
			break;
		}

		case inContent:
		{
			nsMouseEvent mouseEvent;
			ConvertOSEventToMouseEvent(aOSEvent, mouseEvent, NS_MOUSE_LEFT_BUTTON_DOWN);
			nsWindow* widgetHit = (nsWindow*)mouseEvent.widget;
			if (widgetHit)
			{
				// set the focus on the widget hit
				nsCOMPtr<nsToolkit> toolkit ( dont_AddRef((nsToolkit*)widgetHit->GetToolkit()) );
				if (toolkit)
					toolkit->SetFocus(widgetHit);

				// dispatch the event
				retVal = widgetHit->DispatchMouseEvent(mouseEvent);
			}

			if (mLastWidgetHit)
				mLastWidgetHit->RemoveDeleteObserver(this);

			mLastWidgetHit = widgetHit;
			mMouseInWidgetHit = PR_TRUE;

			if (mLastWidgetHit)
				mLastWidgetHit->AddDeleteObserver(this);
			break;
		}
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
	if ((widgetReleased != nsnull) && (widgetReleased != mLastWidgetHit))
		retVal |= widgetReleased->DispatchMouseEvent(mouseEvent);

	if (mLastWidgetHit)
	{
		mLastWidgetHit->RemoveDeleteObserver(this);

		mouseEvent.widget = mLastWidgetHit;
		retVal |= mLastWidgetHit->DispatchMouseEvent(mouseEvent);

		mLastWidgetHit = nsnull;
	}

	return retVal;
}


//-------------------------------------------------------------------------
//
// HandleMouseMoveEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleMouseMoveEvent(
														EventRecord&		aOSEvent)
{
	PRBool retVal = PR_FALSE;

	nsMouseEvent mouseEvent;
	ConvertOSEventToMouseEvent(aOSEvent, mouseEvent, NS_MOUSE_MOVE);

	if (mLastWidgetHit)
	{
		Point macPoint = aOSEvent.where;
		::GlobalToLocal(&macPoint);
		PRBool inWidgetHit = mLastWidgetHit->PointInWidget(macPoint);
		if (inWidgetHit != mMouseInWidgetHit)
		{
			mouseEvent.message = (inWidgetHit ? NS_MOUSE_ENTER : NS_MOUSE_EXIT);
			mMouseInWidgetHit = inWidgetHit;
		}
		retVal |= mLastWidgetHit->DispatchMouseEvent(mouseEvent);
	}
	else
	{
		nsWindow* widgetPointed = (nsWindow*)mouseEvent.widget;
		if (widgetPointed != mLastWidgetPointed)
		{
			if (mLastWidgetPointed)
			{
				mLastWidgetPointed->RemoveDeleteObserver(this);

				mouseEvent.widget = mLastWidgetPointed;
					mouseEvent.message = NS_MOUSE_EXIT;
					retVal |= mLastWidgetPointed->DispatchMouseEvent(mouseEvent);
					mLastWidgetPointed = nsnull;
				mouseEvent.widget = widgetPointed;
			}

			if (widgetPointed)
			{
				widgetPointed->AddDeleteObserver(this);

				mLastWidgetPointed = widgetPointed;
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
		static long		lastWhen = 0;
		static Point	lastWhere = {0, 0};
		static short	lastClickCount = 0;

	// get the click count
	if (((aOSEvent.when - lastWhen) < ::LMGetDoubleTime())
		&& (abs(aOSEvent.where.h - lastWhere.h) < 5)
		&& (abs(aOSEvent.where.v - lastWhere.v) < 5)) 
	{
		if (aOSEvent.what == mouseDown)
		{
			lastClickCount ++;
			if (lastClickCount == 2)
				aMessage = NS_MOUSE_LEFT_DOUBLECLICK;
		}
	}
	else
	{
		if (! ::StillDown())
			lastClickCount = 0;
	}
	lastWhen  = aOSEvent.when;
	lastWhere = aOSEvent.where;

	// get the widget hit and the hit point inside that widget
	Point hitPoint = aOSEvent.where;
	::SetPort(static_cast<GrafPort*>(mTopLevelWidget->GetNativeData(NS_NATIVE_DISPLAY)));
	::SetOrigin(0, 0);
	::GlobalToLocal(&hitPoint);
	nsPoint widgetHitPoint(hitPoint.h, hitPoint.v);

	// if the mouse button is still down, send events to the last widget hit
	nsWindow* widgetHit = nsnull;
	if (mLastWidgetHit)
	{
 		if (::StillDown() || aMessage == NS_MOUSE_LEFT_BUTTON_UP)
	 		widgetHit = mLastWidgetHit;
	 	else
	 	{
	 		// Patch: some widgets can eat mouseUp events (text widgets in TEClick, sbars in TrackControl).
	 		// In that case, fall back to the normal case.
	 		mLastWidgetHit->RemoveDeleteObserver(this);
	 		mLastWidgetHit = nsnull;
	 	}
	}

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

	// nsEvent
	aMouseEvent.eventStructType = NS_MOUSE_EVENT;
	aMouseEvent.message		= aMessage;
	aMouseEvent.point			= widgetHitPoint;
	aMouseEvent.time			= PR_IntervalNow();

	// nsGUIEvent
	aMouseEvent.widget		= widgetHit;
	aMouseEvent.nativeMsg	= (void*)&aOSEvent;

	// nsInputEvent
	aMouseEvent.isShift		= ((aOSEvent.modifiers & shiftKey) != 0);
	aMouseEvent.isControl	= ((aOSEvent.modifiers & controlKey) != 0);
	aMouseEvent.isAlt			= ((aOSEvent.modifiers & optionKey) != 0);

	// nsMouseEvent
	aMouseEvent.clickCount = lastClickCount;
}
