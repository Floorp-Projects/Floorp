/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
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

#include "nsMacMessagePump.h"
#include "nsWindow.h"


#define IsUserWindow(wp) (wp && ((((WindowPeek)wp)->windowKind) >= userKind))



//-------------------------------------------------------------------------
//
// Initialize the message pump
//
//-------------------------------------------------------------------------
nsMacMessagePump::nsMacMessagePump(nsMacMessenger *aTheMessageProc)
{

	mMessenger = aTheMessageProc;
	mRunning = PR_FALSE;

}


//-------------------------------------------------------------------------
//
// Initialize the message pump
//
//-------------------------------------------------------------------------
nsMacMessagePump::~nsMacMessagePump()
{

}

//-------------------------------------------------------------------------
//
// Enter a message handler loop
//
//-------------------------------------------------------------------------
PRBool 
nsMacMessagePump::DoMessagePump()
{
PRBool				stillrunning = PR_TRUE;
EventRecord		theevent;
RgnHandle			fMouseRgn=NULL;
long					sleep=0;
PRInt16				haveevent;
WindowPtr			whichwindow;

#define SUSPENDRESUMEMESSAGE 		0x01
#define MOUSEMOVEDMESSAGE				0xFA

	mRunning = PR_TRUE;
	
	
	while(mRunning && stillrunning)
		{			
		haveevent = ::WaitNextEvent(everyEvent,&theevent,sleep,fMouseRgn);
		if(haveevent)
			{
			switch(theevent.what)
				{
				case nullEvent:
					DoIdleWidgets();
					break;
				case diskEvt:
					if(theevent.message<0)
						{
						// error, bad disk mount
						}
					break;
				case keyUp:
					break;
				case keyDown:
				case autoKey:
					DoKey(&theevent);
					break;
				case mouseDown:
					DoMouseDown(&theevent);
					break;
				case mouseUp:
					break;
				case updateEvt:
					whichwindow = (WindowPtr)theevent.message;
					if (IsUserWindow(whichwindow)) 
						{
						SetPort(whichwindow);
						BeginUpdate(whichwindow);
						
						// generate a paint event


						EndUpdate(whichwindow);
						}
					break;
				case activateEvt:
					whichwindow = (WindowPtr)theevent.message;
					SetPort(whichwindow);
					if(theevent.modifiers & activeFlag)
						{
						::BringToFront(whichwindow);
						::HiliteWindow(whichwindow,TRUE);
						}
					else
						{
						::HiliteWindow(whichwindow,FALSE);
						}
					break;
				case osEvt:
					unsigned char	evtype;
					
					whichwindow = (WindowPtr)theevent.message;
					evtype = (unsigned char) (theevent.message>>24)&0x00ff;
					switch(evtype)
						{
						case MOUSEMOVEDMESSAGE:
							break;
						case SUSPENDRESUMEMESSAGE:
							if(theevent.message&0x00000001)
								{
								// resume message
								}
							else
								{
								// suspend message
								}
						}
					break;
				}
			}
		if(mMessenger)
			stillrunning = mMessenger->IsRunning();
		}

    //if (mDispatchListener)
      //mDispatchListener->AfterDispatch();

  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Handle and pass on Idle events
//
//-------------------------------------------------------------------------
void 
nsMacMessagePump::DoIdleWidgets()
{
WindowPtr			whichwindow;
PRInt16				partcode;
nsWindow			*thewindow;
nsIWidget			*thewidget;
		
	whichwindow = ::FrontWindow();
	while(whichwindow)
		{
		// idle the widget
		thewindow = (nsWindow*)(((WindowPeek)whichwindow)->refCon);
		
		whichwindow = (WindowPtr)((WindowPeek)whichwindow)->nextWindow;
		}

}

//-------------------------------------------------------------------------
//
// Handle the mousedown event
//
//-------------------------------------------------------------------------
void 
nsMacMessagePump::DoMouseDown(EventRecord *aTheEvent)
{
WindowPtr			whichwindow;
PRInt16				partcode;
nsWindow			*thewindow;
nsWindow			*thewidget;
Rect					therect;
long					newsize;			// window's new size
nsMouseEvent	mevent;

	partcode = FindWindow(aTheEvent->where,&whichwindow);

	if(whichwindow!=0)
		{
		SelectWindow(whichwindow);
		thewindow = (nsWindow*)(((WindowPeek)whichwindow)->refCon);
			
		if(thewindow != nsnull)
			thewindow = thewindow->FindWidgetHit(aTheEvent->where);

		switch(partcode)
			{
			case inSysWindow:
				break;
			case inContent:
				if(thewindow)
					{
					// mousedown inside the content region
					mevent.time = 1000;
					mevent.isShift = FALSE;
					mevent.isControl = FALSE;
					mevent.isAlt = FALSE;
					mevent.clickCount = 1;
					mevent.eventStructType = NS_MOUSE_EVENT;
					thewindow->DispatchMouseEvent(mevent);
					}
				break;
			case inDrag:
				therect = qd.screenBits.bounds;
				InsetRect(&therect, 3,3);
				therect.top += 20;    /* Allow space for menu bar */
				DragWindow(whichwindow, aTheEvent->where, &therect);
				break;
			case inGrow:
				SetPort(whichwindow);
				therect = whichwindow->portRect;
				EraseRect(&therect); 
				InvalRect(&therect);
				
				// Set up the window's allowed minimum and maximum sizes
				therect.bottom = qd.screenBits.bounds.bottom;
				therect.right = qd.screenBits.bounds.right;
				therect.top = therect.left = 75;
				newsize = GrowWindow(whichwindow, aTheEvent->where, &therect);
				if(newsize != 0)
					SizeWindow(whichwindow, newsize & 0x0FFFF, (newsize >> 16) & 0x0FFFF, true);

				// Draw the grow icon & validate that area
				therect = whichwindow->portRect;
				therect.left = therect.right - 16;
				therect.top = therect.bottom - 16;
				DrawGrowIcon(whichwindow);
				ValidRect(&therect);
				break;
			case inGoAway:
				if(TrackGoAway(whichwindow,aTheEvent->where))
					if(thewindow)
						{
						thewindow->Destroy();
						mRunning = PR_FALSE;
						}
					else
						{
						}
				break;
			case inZoomIn:
			case inZoomOut:
				break;
			case inMenuBar:
				break;
			}
		}
}

//-------------------------------------------------------------------------
//
// Handle the key events
//
//-------------------------------------------------------------------------
void nsMacMessagePump::DoKey(EventRecord *aTheEvent)
{
char				ch;
WindowPtr		whichwindow;

	ch = (char)(aTheEvent->message & charCodeMask);
	if(aTheEvent->modifiers&cmdKey)
		{
		// do a menu key command
		}
	else
		{
		whichwindow = FrontWindow();
		if(whichwindow)
			{
			// generate a keydown event for the widget
			}
		}
}
