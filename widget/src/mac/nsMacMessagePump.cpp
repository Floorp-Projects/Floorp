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
#include "nsTextWidget.h"
#include "nsWidgetsCID.h"
#include <LPeriodical.h>


#define DRAW_ON_RESIZE

#define IsUserWindow(wp) (wp && ((((WindowPeek)wp)->windowKind) >= userKind))

nsWindow* nsMacMessagePump::gCurrentWindow = nsnull;   
nsWindow* nsMacMessagePump::gGrabWindow = nsnull;			// need this for grabmouse

static NS_DEFINE_IID(kITEXTWIDGETIID, NS_TEXTFIELD_CID);


//=================================================================
/*  Constructor
 *  @update  dc 08/31/98
 *  @param   aToolkit -- The toolkit created by the application
 *  @return  NONE
 */
nsMacMessagePump::nsMacMessagePump(nsToolkit	*aToolkit)
{

	mRunning = PR_FALSE;
	mToolkit = aToolkit;

}

//=================================================================
/*  Destructor
 *  @update  dc 08/31/98
 *  @param   NONE
 *  @return  NONE
 */
nsMacMessagePump::~nsMacMessagePump()
{

}

//=================================================================
/*  Runs the message pump for the macintosh.  Turns them into Raptor events
 *  @update  dc 08/31/98
 *  @param   NONE
 *  @return  A boolean which states how the pump terminated
 */
PRBool 
nsMacMessagePump::DoMessagePump()
{
PRBool				stillrunning = PR_TRUE;
EventRecord		theevent;
long					sleep=0;
PRInt16				haveevent;
WindowPtr			whichwindow;
RgnHandle			currgn;
unsigned char	evtype;

#define SUSPENDRESUMEMESSAGE 		0x01
#define MOUSEMOVEDMESSAGE				0xFA

	mRunning = PR_TRUE;
	mInBackground = PR_FALSE;
	
	// calculate the regions to watch
	currgn = ::NewRgn();
	SetRectRgn(currgn,-32000,-32000,-32000,-32000);
	
	
	while(mRunning && stillrunning)
		{			
		haveevent = ::WaitNextEvent(everyEvent,&theevent,sleep,0l);

		if(haveevent)
			{
			switch(theevent.what)
				{
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
					DoMouseUp(&theevent);
					break;
				case updateEvt:
					DoPaintEvent(&theevent);
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
				default:
					evtype = (unsigned char) (theevent.message>>24)&0x00ff;
					switch(evtype)
						{
						//case MOUSEMOVEDMESSAGE:
							//DoMouseMove(&theevent);
							//break;
						case SUSPENDRESUMEMESSAGE:
							if(theevent.message&0x00000001)
								{
								// resume message
								mInBackground = PR_FALSE;
								}
							else
								{
								// suspend message
								mInBackground = PR_TRUE;
								}
							break;
						}
					break;
				}
			}
		else
		    {
			switch(theevent.what)
				{
				case nullEvent:
					DoIdleWidgets();	
					
					// construct the mousemove myself
					
					::GetMouse(&theevent.where);
					LocalToGlobal(&theevent.where);
					DoMouseMove(&theevent);
					break;
				}		
			}
			
	  	LPeriodical::DevoteTimeToRepeaters(theevent);
		}

    //if (mDispatchListener)
      //mDispatchListener->AfterDispatch();

  return NS_OK;
}

//=================================================================
/*  Turns an update event into a raptor paint event
 *  @update  dc 08/31/98
 *  @param   aTheEvent -- A pointer to a Macintosh EventRecord
 *  @return  NONE
 */
void 
nsMacMessagePump::DoPaintEvent(EventRecord *aTheEvent)
{
GrafPtr				curport;
WindowPtr			whichwindow;
nsWindow			*thewindow;
nsRect 				rect;
RgnHandle			updateregion;
nsPaintEvent 		pevent;
 
 	::GetPort(&curport);
	whichwindow = (WindowPtr)aTheEvent->message;
	
	if (IsUserWindow(whichwindow)) 
		{
		SetPort(whichwindow);
		BeginUpdate(whichwindow);
		
		thewindow = (nsWindow*)(((WindowPeek)whichwindow)->refCon);
		if(thewindow != nsnull)
			{
			updateregion = whichwindow->visRgn;
			thewindow->DoPaintWidgets(updateregion);

			Rect bounds = (**updateregion).rgnBBox;
			rect.x = bounds.left;
			rect.y = bounds.top;
			rect.width = bounds.left + (bounds.right-bounds.left);
			rect.height = bounds.top + (bounds.bottom-bounds.top);
          
			// generate a paint event
			pevent.message = NS_PAINT;
			pevent.widget = thewindow;
			pevent.eventStructType = NS_PAINT_EVENT;
			pevent.point.x = 0;
			pevent.point.y = 0;
			pevent.rect = &rect;
			pevent.time = 0; 
			thewindow->OnPaint(pevent);
	    	}
		EndUpdate(whichwindow);
		}
	
	::SetPort(curport);

}

//=================================================================
/*  Idles Raptor
 *  @update  dc 08/31/98
 *  @param   NONE
 *  @return  NONE
 */
void 
nsMacMessagePump::DoIdleWidgets()
{
WindowPtr			whichwindow;
nsWindow			*thewindow;
		
	whichwindow = ::FrontWindow();
	while(whichwindow)
		{
		// idle the widget
		thewindow = (nsWindow*)(((WindowPeek)whichwindow)->refCon);
		
		whichwindow = (WindowPtr)((WindowPeek)whichwindow)->nextWindow;
		}

}

//==============================================================

// XXXX Quick and Extra Ugly hack: These #defines copied from resources.h in the viewer
// project. This makes the the widget project tied to the viewer project. This is bad. This
// is ugly. This should be fixed soon.

#define VIEWER_OPEN             40000
#define VIEWER_EXIT             40002
#define PREVIEW_CLOSE           40003

#define VIEWER_WINDOW_OPEN      40009
#define VIEWER_FILE_OPEN        40010

// Note: must be in ascending sequential order
#define VIEWER_DEMO0            40011

#define VIEWER_VISUAL_DEBUGGING     40021

// Note: must be in ascending sequential order
#define VIEWER_ONE_COLUMN       40040

#define JS_CONSOLE              40100
#define EDITOR_MODE             40120

#define VIEWER_EDIT_CUT         40201

#define FILE_MENU 2000
#define EDIT_MENU 2001
#define DEBU_MENU 2002
#define DEMO_MENU 128
#define PRIN_MENU 129

static PRUint32 translateMenu (long aMenuResult)
{
	PRUint32 result = 0;
	long menuid = HiWord(aMenuResult);
	long menuItem = LoWord(aMenuResult);
	
	switch (menuid)
	{
		case FILE_MENU:
			switch (menuItem)
			{
				//case XXX: result = XXXX; break;
			}
			break;
		case EDIT_MENU:
			result = VIEWER_EDIT_CUT + menuItem;
			break;
		case DEBU_MENU:
			result = VIEWER_VISUAL_DEBUGGING + menuItem;
			break;
		case DEMO_MENU:
			  result = VIEWER_DEMO0 - 1 + menuItem;
			break;
		case PRIN_MENU:
			result = VIEWER_ONE_COLUMN + menuItem;
			break;
	}
	return result;
}

//=================================================================
/*  Turns a mousedown event into a raptor mousedown event
 *  @update  dc 08/31/98
 *  @param   aTheEvent -- A pointer to a Macintosh EventRecord
 *  @return  NONE
 */

void 
nsMacMessagePump::DoMouseDown(EventRecord *aTheEvent)
{
PRBool				result;
Rect					therect;
Point					windowcoord;
WindowPtr			whichwindow;
PRInt16				partcode;
nsWindow			*thewindow;
nsMouseEvent	mouseevent;


	partcode = FindWindow(aTheEvent->where,&whichwindow);

	if (inMenuBar == partcode)
	{
	  nsMenuEvent theEvent;
	  nsWindow *raptorWindow;
	  long menuItem = MenuSelect(aTheEvent->where);
	  
	  HiliteMenu(0);

	#if 1
	  whichwindow = FrontWindow();
      raptorWindow = (nsWindow *) GetWRefCon (whichwindow);
    #else
      // XXX For some reason this returns null... which is bad...
	  raptorWindow = mToolkit->GetFocus();
    #endif
    
      theEvent.eventStructType = NS_MENU_EVENT;
      theEvent.message  = NS_MENU_SELECTED;
	  theEvent.menuItem = translateMenu (menuItem);
	  theEvent.widget   = raptorWindow;
	  theEvent.nativeMsg = aTheEvent;
	  
      raptorWindow->DispatchEvent(&theEvent);
	  return;
	}

	if(whichwindow!=0)
		{
		SelectWindow(whichwindow);
		thewindow = (nsWindow*)(((WindowPeek)whichwindow)->refCon);
			
		if(thewindow != nsnull)
			{
			thewindow = thewindow->FindWidgetHit(aTheEvent->where);
			}

		switch(partcode)
			{
			case inSysWindow:
				break;
			case inContent:
				if(thewindow)
					{
					SetPort(whichwindow);					
					mouseevent.message = NS_MOUSE_LEFT_BUTTON_DOWN;
					mouseevent.widget  = (nsWindow *) thewindow;
					windowcoord = aTheEvent->where;
					GlobalToLocal(&windowcoord);
					mouseevent.point.x = windowcoord.h;
					mouseevent.point.y = windowcoord.v;					
					mouseevent.time = 0;
					mouseevent.isShift = FALSE;
					mouseevent.isControl = FALSE;
					mouseevent.isAlt = FALSE;
					mouseevent.clickCount = 1;
					mouseevent.eventStructType = NS_MOUSE_EVENT;
					// dispatch the event, if returns false, the widget does not want to grab the mouseup
					result = thewindow->DispatchMouseEvent(mouseevent);
					if(result == PR_TRUE)
						gGrabWindow = (nsWindow*)thewindow;		// grab is in effect
					this->SetCurrentWindow(thewindow);
					mToolkit->SetFocus(thewindow);
					}
				break;
			case inDrag:
				therect = qd.screenBits.bounds;
				InsetRect(&therect, 3,3);
				therect.top += 20;    /* Allow space for menu bar */
				DragWindow(whichwindow, aTheEvent->where, &therect);
				therect = whichwindow->portRect;
				if (thewindow != nsnull)
				{
					LocalToGlobal(&topLeft(therect));
					LocalToGlobal(&botRight(therect));
					thewindow->SetBounds(therect);
				}
				break;
				
			case inGrow:
				SetPort(whichwindow);

				Point oldPt, newPt;
				oldPt = aTheEvent->where;
				GlobalToLocal(&oldPt);
#ifdef DRAW_ON_RESIZE
				while (WaitMouseUp())
				{
					LPeriodical::DevoteTimeToRepeaters(*aTheEvent);
					GetMouse(&newPt);
					if (DeltaPoint(oldPt, newPt))
					{
						oldPt = newPt;

						// Resize Mac window (draw on resize)
						short width = max((short)75, newPt.h);
						short height = max((short)75, newPt.v);
						SizeWindow(whichwindow, width, height, true);
#else
						// Resize Mac window (the usual way)
						therect.bottom = qd.screenBits.bounds.bottom;
						therect.right = qd.screenBits.bounds.right;
						therect.top = therect.left = 75;
						long newsize = GrowWindow(whichwindow, aTheEvent->where, &therect);
						if(newsize != 0)
							SizeWindow(whichwindow, newsize & 0x0FFFF, (newsize >> 16) & 0x0FFFF, true);
#endif
						// Draw the grow icon & validate that area
						therect = whichwindow->portRect;
						therect.left = therect.right - 16;
						therect.top = therect.bottom - 16;
						DrawGrowIcon(whichwindow);
						ValidRect(&therect);
						
						// Resize layout objects
						thewindow = (nsWindow *) GetWRefCon (whichwindow);
						if (thewindow != nsnull)
						{
							therect = whichwindow->portRect;
							LocalToGlobal(&topLeft(therect));
							LocalToGlobal(&botRight(therect));
							thewindow->SetBounds(therect);

							nsSizeEvent sizeevent;
							nsRect windowSize;
							windowSize.SetRect(0, 0, therect.right - therect.left, therect.bottom - therect.top);
							sizeevent.eventStructType = NS_SIZE_EVENT;
							sizeevent.message = NS_SIZE;
							sizeevent.widget = (nsWindow *)thewindow;
							sizeevent.windowSize = &windowSize;

							thewindow->DoResizeWidgets(sizeevent);
							result = thewindow->OnResize(sizeevent);
						}
#ifdef DRAW_ON_RESIZE
					}
				}
#endif
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
			}
		}
}

//=================================================================
/*  Turns a mouseup event into a raptor mouseup event
 *  @update  dc 08/31/98
 *  @param   aTheEvent -- A pointer to a Macintosh EventRecord
 *  @return  NONE
 */
void 
nsMacMessagePump::DoMouseUp(EventRecord *aTheEvent)
{
WindowPtr			whichwindow;
PRInt16				partcode;
Point					windowcoord;
nsWindow			*thewindow;
nsMouseEvent	mouseevent;

	partcode = FindWindow(aTheEvent->where,&whichwindow);

	if(gGrabWindow)
		{
		mouseevent.message = NS_MOUSE_LEFT_BUTTON_UP;
		mouseevent.widget  = (nsWindow *) gGrabWindow;
		windowcoord = aTheEvent->where;
		GlobalToLocal(&windowcoord);
		mouseevent.point.x = windowcoord.h;
		mouseevent.point.y = windowcoord.v;					
		mouseevent.time = 0;
		mouseevent.isShift = FALSE;
		mouseevent.isControl = FALSE;
		mouseevent.isAlt = FALSE;
		mouseevent.clickCount = 1;
		mouseevent.eventStructType = NS_MOUSE_EVENT;
		gGrabWindow->DispatchMouseEvent(mouseevent);
		gGrabWindow = nsnull;		// mouse grab no longer in effect
		return;
		}

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
					SetPort(whichwindow);
					mouseevent.message = NS_MOUSE_LEFT_BUTTON_UP;
					mouseevent.widget  = (nsWindow *) thewindow;
					windowcoord = aTheEvent->where;
					GlobalToLocal(&windowcoord);
					mouseevent.point.x = windowcoord.h;
					mouseevent.point.y = windowcoord.v;					
					mouseevent.time = 0;
					mouseevent.isShift = FALSE;
					mouseevent.isControl = FALSE;
					mouseevent.isAlt = FALSE;
					mouseevent.clickCount = 1;
					mouseevent.eventStructType = NS_MOUSE_EVENT;
					thewindow->DispatchMouseEvent(mouseevent);
					}
				break;
			}
		}
	gGrabWindow = nsnull;		// mouse grab no longer in effect
}

//=================================================================
/*  Turns a null event into a raptor mousemove event
 *  @update  dc 08/31/98
 *  @param   aTheEvent -- A pointer to a Macintosh EventRecord
 *  @return  NONE
 */
void 
nsMacMessagePump::DoMouseMove(EventRecord *aTheEvent)
{
WindowPtr			whichwindow;
PRInt16				partcode;
Point					windowcoord;
nsWindow			*thewindow,*lastwindow;
nsMouseEvent	mouseevent;


	if (*(long*)&mMousePoint == *(long*)&aTheEvent->where)
		return;

	mouseevent.time = 0;
	mouseevent.isShift = FALSE;
	mouseevent.isControl = FALSE;
	mouseevent.isAlt = FALSE;
	mouseevent.clickCount = 1;
	mouseevent.eventStructType = NS_MOUSE_EVENT;
	lastwindow = this->GetCurrentWindow();
	mMousePoint = aTheEvent->where;

	partcode = FindWindow(aTheEvent->where,&whichwindow);
	thewindow = nsnull;
	
	if(whichwindow!=nsnull)
		{
		SetPort(whichwindow);
		thewindow = (nsWindow*)(((WindowPeek)whichwindow)->refCon);
		}
	if( (thewindow != nsnull))
		thewindow = thewindow->FindWidgetHit(aTheEvent->where);


	if(gGrabWindow)
		{
		if( /*(gGrabWindow==thewindow) ||*/ thewindow==lastwindow)
			{
			// JUST A MOUSE MOVE
			mouseevent.message = NS_MOUSE_MOVE;
			mouseevent.widget  = (nsWindow *) gGrabWindow;
			windowcoord = aTheEvent->where;
			GlobalToLocal(&windowcoord);
			mouseevent.point.x = windowcoord.h;
			mouseevent.point.y = windowcoord.v;				
			gGrabWindow->DispatchMouseEvent(mouseevent);
			this->SetCurrentWindow(thewindow);
			}
		else
			{
			// EXIT or ENTER?
			if(lastwindow == nsnull || thewindow != lastwindow)
				{
				if(lastwindow == gGrabWindow)
					{
					mouseevent.message = NS_MOUSE_EXIT;
					mouseevent.widget  = (nsWindow *) gGrabWindow;
					windowcoord = aTheEvent->where;
					GlobalToLocal(&windowcoord);
					mouseevent.point.x = windowcoord.h;
					mouseevent.point.y = windowcoord.v;					
					gGrabWindow->DispatchMouseEvent(mouseevent);	
					this->SetCurrentWindow(thewindow);				
					}
				else
					{
					mouseevent.message = NS_MOUSE_ENTER;
					mouseevent.widget  = (nsWindow *) gGrabWindow;
					windowcoord = aTheEvent->where;
					GlobalToLocal(&windowcoord);
					mouseevent.point.x = windowcoord.h;
					mouseevent.point.y = windowcoord.v;					
					gGrabWindow->DispatchMouseEvent(mouseevent);
					this->SetCurrentWindow(thewindow);				
					}
				}
			}
		return;
		}


	//partcode = FindWindow(aTheEvent->where,&whichwindow);
	switch(partcode)
		{
		case inContent:
			if(thewindow)
				{
				if(lastwindow == nsnull || thewindow != lastwindow)
					{
					// mouseexit
					if(lastwindow != nsnull)
						{
						mouseevent.message = NS_MOUSE_EXIT;
						mouseevent.widget  = (nsWindow *) lastwindow;
						windowcoord = aTheEvent->where;
						GlobalToLocal(&windowcoord);
						mouseevent.point.x = windowcoord.h;
						mouseevent.point.y = windowcoord.v;					
						lastwindow->DispatchMouseEvent(mouseevent);
						}

					// mouseenter
					this->SetCurrentWindow(thewindow);
					mouseevent.message = NS_MOUSE_ENTER;
					mouseevent.widget  = (nsWindow *) thewindow;
					windowcoord = aTheEvent->where;
					GlobalToLocal(&windowcoord);
					mouseevent.point.x = windowcoord.h;
					mouseevent.point.y = windowcoord.v;					
					thewindow->DispatchMouseEvent(mouseevent);	
					}
				else
					{
					// mousedown inside the content region
					mouseevent.message = NS_MOUSE_MOVE;
					mouseevent.widget  = (nsWindow *) thewindow;
					windowcoord = aTheEvent->where;
					GlobalToLocal(&windowcoord);
					mouseevent.point.x = windowcoord.h;
					mouseevent.point.y = windowcoord.v;		
					thewindow->DispatchMouseEvent(mouseevent);
					break;
					}
				}
			break;
		default:
			if(lastwindow != nsnull)
				{
				this->SetCurrentWindow(nsnull);
				mouseevent.message = NS_MOUSE_EXIT;
				mouseevent.widget  = (nsWindow *) lastwindow;
				windowcoord = aTheEvent->where;
				GlobalToLocal(&windowcoord);
				mouseevent.point.x = windowcoord.h;
				mouseevent.point.y = windowcoord.v;					
				lastwindow->DispatchMouseEvent(mouseevent);
				}
			break;
		}

}

//=================================================================
/*  Turns a keydown event into a raptor key event
 *  @update  dc 08/31/98
 *  @param   aTheEvent -- A pointer to a Macintosh EventRecord
 *  @return  NONE
 */
void 
nsMacMessagePump::DoKey(EventRecord *aTheEvent)
{
char					ch;
PRInt16				thechar;
WindowPtr			whichwindow;
nsWindow			*thewidget;
nsKeyEvent 		keyEvent;
nsTextWidget	*widget;

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
			thewidget = mToolkit->GetFocus();
			if(thewidget != nsnull)
				{
				keyEvent.message = NS_KEY_DOWN;
			  keyEvent.keyCode   = 1;
			  keyEvent.time      = 0; 
			  keyEvent.isShift   = PR_FALSE; 
			  keyEvent.isControl = PR_FALSE;
			  keyEvent.isAlt     = PR_FALSE;
			  keyEvent.eventStructType = NS_KEY_EVENT;
			  
			  thechar = aTheEvent->message&charCodeMask;
			  
			  if (!thewidget->DispatchEvent(&keyEvent))
			  	{
			  	// if this is a nsTextWidget
			  	//if (NS_OK == thewidget->QueryInterface(kITEXTWIDGETIID, (void**) &widget) )
			  	widget = (nsTextWidget*)thewidget;
			  	widget->PrimitiveKeyDown(thechar,0);
			  	}
			  
			  //((nsWindow*)thewidget)->OnKey(NS_KEY_DOWN, 1, &keyEvent);
				//thewidget->kdsjfkj()
				}
			}
		}
}

//==============================================================
