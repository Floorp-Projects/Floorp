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
#include "prinrval.h"

// from MacHeaders.c
#ifndef topLeft
	#define topLeft(r)	(((Point *) &(r))[0])
#endif
#ifndef botRight
	#define botRight(r)	(((Point *) &(r))[1])
#endif


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
		case nullEvent:
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
															EventRecord&		aOSEvent,
															long						aMenuResult)
{
	// get the focused widget
	nsWindow*	focusedWidget = nsnull;
	nsToolkit* toolkit = (nsToolkit*)mTopLevelWidget->GetToolkit();
	if (toolkit)
	{
		focusedWidget = toolkit->GetFocus();
		NS_RELEASE(toolkit);
	}
	
	if (focusedWidget == nsnull)
		focusedWidget = mTopLevelWidget;


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
		nsIWidget* grandParent;
		nsIWidget* parent = focusedWidget->GetParent();
		while (parent)
		{
			menuEvent.widget = parent;
			eventHandled = ((nsWindow*)parent)->DispatchWindowEvent(menuEvent);
			if (eventHandled)
			{
				NS_IF_RELEASE(parent);
				break;
			}
			else
			{
				grandParent = parent->GetParent();
				NS_IF_RELEASE(parent);
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
PRBool nsMacEventHandler::HandleKeyEvent(EventRecord& aOSEvent)
{
	// get the focused widget
	nsWindow*	focusedWidget = nsnull;
	nsToolkit* toolkit = (nsToolkit*)mTopLevelWidget->GetToolkit();
	if (toolkit)
	{
		focusedWidget = toolkit->GetFocus();
		NS_RELEASE(toolkit);
	}
	
	if (focusedWidget == nsnull)
		focusedWidget = mTopLevelWidget;

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
  keyEvent.keyCode		= (aOSEvent.message & charCodeMask);	//¥TODO: do special keys conversions for NS_VK_F1 etc...

	return(focusedWidget->DispatchWindowEvent(keyEvent));
}


//-------------------------------------------------------------------------
//
// HandleActivateEvent
//
//-------------------------------------------------------------------------
PRBool nsMacEventHandler::HandleActivateEvent(EventRecord& aOSEvent)
{
	Boolean isActive = ((aOSEvent.modifiers & activeFlag) != 0);
	if (isActive)
	{
		// get focused widget
		nsWindow*	focusedWidget = mTopLevelWidget;
		nsIMenuBar* menuBar = focusedWidget->GetMenuBar();
					//¥TODO:	should get the focus from the toolkit and
					//				look all the way up to the window
					//				until one of the parents has a menubar

		//¥TODO: set the menu bar here
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
				nsToolkit* toolkit = (nsToolkit*)widgetHit->GetToolkit();
				if (toolkit)
				{
					toolkit->SetFocus(widgetHit);
					NS_RELEASE(toolkit);
				}

				// dispatch the event
				retVal = widgetHit->DispatchMouseEvent(mouseEvent);
			}
			mLastWidgetHit = widgetHit;
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

	if (mLastWidgetHit != nsnull)
	{
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

	nsWindow* widgetPointed = (nsWindow*)mouseEvent.widget;
	if (widgetPointed != mLastWidgetPointed)
	{
		if (mLastWidgetPointed != nsnull)
		{
			mouseEvent.widget = mLastWidgetPointed;
				mouseEvent.message = NS_MOUSE_EXIT;
				retVal |= mLastWidgetPointed->DispatchMouseEvent(mouseEvent);
				mLastWidgetPointed = nsnull;
			mouseEvent.widget = widgetPointed;
		}
		if (widgetPointed != nsnull)
		{
			mouseEvent.message = NS_MOUSE_ENTER;
			retVal |= widgetPointed->DispatchMouseEvent(mouseEvent);
			mLastWidgetPointed = widgetPointed;
		}
	}
	else
	{
		if (widgetPointed != nsnull)
			retVal |= widgetPointed->DispatchMouseEvent(mouseEvent);
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
			lastClickCount ++;
	}
	else
	{
		if (! ::StillDown())
			lastClickCount = 0;
	}
	lastWhen  = aOSEvent.when;
	lastWhere = aOSEvent.where;

	// get the widget hit and its hit point
	Point hitPoint = aOSEvent.where;
	::SetPort(static_cast<GrafPort*>(mTopLevelWidget->GetNativeData(NS_NATIVE_DISPLAY)));
	::SetOrigin(0, 0);
	::GlobalToLocal(&hitPoint);
	nsPoint widgetHitPoint(hitPoint.h, hitPoint.v);

	nsWindow* widgetHit = mTopLevelWidget->FindWidgetHit(hitPoint);
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
