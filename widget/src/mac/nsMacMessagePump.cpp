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
nsMacMessagePump::nsWindowlessMenuEventHandler nsMacMessagePump::gWindowlessMenuEventHandler = nsnull;


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
	
	
	while(mRunning)
		{			
		haveevent = ::WaitNextEvent(everyEvent,&theevent,sleep,0l);

	  	LPeriodical::DevoteTimeToRepeaters(theevent);

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
nsPaintEvent 	pEvent;
nsRefData			*theRefData;
	
 
 	::GetPort(&curport);
	whichwindow = (WindowPtr)aTheEvent->message;
	
	if (IsUserWindow(whichwindow)) 
		{
		SetPort(whichwindow);
		BeginUpdate(whichwindow);
		// we have to layout the entire window, so this does us no good
		EndUpdate(whichwindow);
		
		theRefData = (nsRefData*)GetWRefCon (whichwindow);
		thewindow = (nsWindow*)theRefData->GetTopWidget();

		if(thewindow != nsnull)
			{
			updateregion = whichwindow->visRgn;
			Rect bounds = (**updateregion).rgnBBox;
			rect.x = bounds.left;
			rect.y = bounds.top;
			rect.width = bounds.left + (bounds.right-bounds.left);
			rect.height = bounds.top + (bounds.bottom-bounds.top);
        
			// generate a paint event
			pEvent.message = NS_PAINT;
      pEvent.renderingContext = thewindow->GetRenderingContext();
			pEvent.widget = thewindow;
			pEvent.eventStructType = NS_PAINT_EVENT;
			pEvent.point.x = 0;
			pEvent.point.y = 0;
			pEvent.rect = &rect;
			pEvent.time = 0; 
			thewindow->OnPaint(pEvent);

			// take care of the childern
			thewindow->DoPaintWidgets(updateregion,pEvent.renderingContext);
	    }
		
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
nsRefData				*theRefData;
		
	whichwindow = ::FrontWindow();
	while(whichwindow)
		{
		// idle the widget
		theRefData = (nsRefData*)GetWRefCon (whichwindow);
		thewindow = (nsWindow*)theRefData->GetTopWidget();
		
		whichwindow = (WindowPtr)((WindowPeek)whichwindow)->nextWindow;
		}

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
Point					hitPoint;
PRInt32				newx,newy;
WindowPtr			whichwindow;
PRInt16				partcode;
nsWindow			*thewindow;
nsMouseEvent	mouseevent;
nsSizeEvent 	event;
nsEventStatus	eventStatus;
nsRect				sizerect;
Point					newPt;
nsRefData			*theRefData;

	partcode = FindWindow(aTheEvent->where,&whichwindow);

	if (inMenuBar == partcode)
	{
	  long menuResult = MenuSelect(aTheEvent->where);
	  if (HiWord(menuResult) != 0)
	      DoMenu(aTheEvent, menuResult);
	  return;
	}

	if(whichwindow!=0)
		{
		SelectWindow(whichwindow);
		theRefData = (nsRefData*)GetWRefCon (whichwindow);
		thewindow = (nsWindow*)theRefData->GetTopWidget();
			
		if(thewindow != nsnull)
			{
			hitPoint = aTheEvent->where;
			::SetPort(whichwindow);
			::GlobalToLocal(&hitPoint);
			thewindow = thewindow->FindWidgetHit(hitPoint);
			}

		switch(partcode)
			{
			case inSysWindow:
				break;
			case inContent:
				if(thewindow)
					{
					mouseevent.message = NS_MOUSE_LEFT_BUTTON_DOWN;
					mouseevent.widget  = (nsWindow *) thewindow;
					
					// calculate the offset for the event passed in	
					newx = hitPoint.h;
					newy = hitPoint.v;
					thewindow->ConvertToDeviceCoordinates(newx,newy);
					
					mouseevent.point.x = newx;
					mouseevent.point.y = newy;					
					mouseevent.time = 0;
					mouseevent.isShift = FALSE;
					mouseevent.isControl = FALSE;
					mouseevent.isAlt = FALSE;
					mouseevent.clickCount = 1;
					mouseevent.eventStructType = NS_MOUSE_EVENT;
					// dispatch the event, if returns false, the widget does not want to grab the mouseup
					result = thewindow->DispatchMouseEvent(mouseevent);
					if(result == nsEventStatus_eConsumeDoDefault)
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
				theRefData = (nsRefData*)GetWRefCon (whichwindow);
				thewindow = (nsWindow*)theRefData->GetTopWidget();

				if (thewindow != nsnull)
					{
					LocalToGlobal(&topLeft(therect));
					LocalToGlobal(&botRight(therect));
					thewindow->SetBounds(therect);
					}
				break;
				
			case inGrow:
				SetPort(whichwindow);
				Point oldPt;
				oldPt = aTheEvent->where;
				//GlobalToLocal(&oldPt);
#ifdef DRAW_ON_RESIZE
				while (WaitMouseUp())
				{
					LPeriodical::DevoteTimeToRepeaters(*aTheEvent);
					GetMouse(&newPt);
					LocalToGlobal(&newPt);
					if (DeltaPoint(oldPt, newPt))
					{
						// Resize Mac window (draw on resize)
						therect = whichwindow->portRect;
						short	width = (therect.right - therect.left)+(newPt.h - oldPt.h);
						short	height = (therect.bottom - therect.top)+(newPt.v - oldPt.v);
						
						oldPt = newPt;
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
						//ValidRect(&therect);
						
						// Resize layout objects
						theRefData = (nsRefData*)GetWRefCon (whichwindow);
						thewindow = (nsWindow*)theRefData->GetTopWidget();

						if (thewindow != nsnull)
						{
							therect = whichwindow->portRect;
							
							LocalToGlobal(&topLeft(therect));
							LocalToGlobal(&botRight(therect));
							thewindow->SetBounds(therect);			// set the bounds in mac
							thewindow->GetBounds(sizerect);			// tricky-- get it back in nsRect

			        event.message = NS_SIZE;
			        event.point.x = 0;
			        event.point.y = 0;
			        event.windowSize = &sizerect;
			        event.eventStructType = NS_SIZE_EVENT;
			        event.widget = thewindow;
			       	thewindow->DispatchEvent(&event, eventStatus);

							//thewindow->DoResizeWidgets(event);
							//result = thewindow->OnResize(event);
							//::InvalRect(&therect);
						}
#ifdef DRAW_ON_RESIZE
					}
				}
#endif
				break;
			case inGoAway:
				if(TrackGoAway(whichwindow,aTheEvent->where))
					{
					theRefData = (nsRefData*)GetWRefCon (whichwindow);
					thewindow = (nsWindow*)theRefData->GetTopWidget();
					if(thewindow)
						{
						thewindow->Destroy();
						//mRunning = PR_FALSE;
						}
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
PRInt32				newx,newy;
Point					hitPoint;
nsWindow			*thewindow;
nsMouseEvent	mouseevent;
nsRefData			*theRefData;

	partcode = FindWindow(aTheEvent->where,&whichwindow);



	if(gGrabWindow)
		{
		mouseevent.message = NS_MOUSE_LEFT_BUTTON_UP;
		mouseevent.widget  = (nsWindow *) gGrabWindow;
		hitPoint = aTheEvent->where;
		GlobalToLocal(&hitPoint);

					// calculate the offset for the event passed in	
					newx = hitPoint.h;
					newy = hitPoint.v;
					gGrabWindow->ConvertToDeviceCoordinates(newx,newy);

		mouseevent.point.x = newx;
		mouseevent.point.y = newy;					
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
		theRefData = (nsRefData*)GetWRefCon (whichwindow);
		thewindow = (nsWindow*)theRefData->GetTopWidget();
			
		if(thewindow != nsnull)
			{
			hitPoint = aTheEvent->where;
			SetPort(whichwindow);
			GlobalToLocal(&hitPoint);
			thewindow = thewindow->FindWidgetHit(hitPoint);
			}

		switch(partcode)
			{
			case inSysWindow:
				break;
			case inContent:
				if(thewindow)
					{
					mouseevent.message = NS_MOUSE_LEFT_BUTTON_UP;
					mouseevent.widget  = (nsWindow *) thewindow;
					
					// calculate the offset for the event passed in	
					newx = hitPoint.h;
					newy = hitPoint.v;
					thewindow->ConvertToDeviceCoordinates(newx,newy);
					mouseevent.point.x = newx;
					mouseevent.point.y = newy;					
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
 *  @update  dc 10/02/98
 *  @param   aTheEvent -- A pointer to a Macintosh EventRecord
 *  @return  NONE
 */
void 
nsMacMessagePump::DoMouseMove(EventRecord *aTheEvent)
{
WindowPtr			whichwindow;
PRInt16				partcode;
PRInt32				newx,newy;
Point					hitPoint;
nsWindow			*thewindow,*lastwindow;
nsMouseEvent	mouseevent;
nsRefData			*theRefData;


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
		theRefData = (nsRefData*)GetWRefCon (whichwindow);
		thewindow = (nsWindow*)theRefData->GetTopWidget();
		}
	if( (thewindow != nsnull))
		{
		hitPoint = aTheEvent->where;
		GlobalToLocal(&hitPoint);		
		thewindow = thewindow->FindWidgetHit(hitPoint);
		}

	// the mouse is down, and moving
	if(gGrabWindow)
		{
		if(thewindow==lastwindow)
			{
			// JUST A MOUSE MOVE
			mouseevent.message = NS_MOUSE_MOVE;
			mouseevent.widget  = (nsWindow *) gGrabWindow;
			hitPoint = aTheEvent->where;
			GlobalToLocal(&hitPoint);

					// calculate the offset for the event passed in	
					newx = hitPoint.h;
					newy = hitPoint.v;
					gGrabWindow->ConvertToDeviceCoordinates(newx,newy);
			mouseevent.point.x = newx;
			mouseevent.point.y = newy;				
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
					hitPoint = aTheEvent->where;
					GlobalToLocal(&hitPoint);
					
					// calculate the offset for the event passed in	
					newx = hitPoint.h;
					newy = hitPoint.v;
					gGrabWindow->ConvertToDeviceCoordinates(newx,newy);

					mouseevent.point.x = newx;
					mouseevent.point.y = newy;					
					gGrabWindow->DispatchMouseEvent(mouseevent);	
					this->SetCurrentWindow(thewindow);				
					}
				else
					{
					mouseevent.message = NS_MOUSE_ENTER;
					mouseevent.widget  = (nsWindow *) gGrabWindow;
					hitPoint = aTheEvent->where;
					GlobalToLocal(&hitPoint);
					// calculate the offset for the event passed in	
					newx = hitPoint.h;
					newy = hitPoint.v;
					gGrabWindow->ConvertToDeviceCoordinates(newx,newy);

					mouseevent.point.x = newx;
					mouseevent.point.y = newy;					
					gGrabWindow->DispatchMouseEvent(mouseevent);
					this->SetCurrentWindow(thewindow);				
					}
				}
			}
		return;
		}
	else
		{	
		if(whichwindow)
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
							hitPoint = aTheEvent->where;
							GlobalToLocal(&hitPoint);
					// calculate the offset for the event passed in	
					newx = hitPoint.h;
					newy = hitPoint.v;
					thewindow->ConvertToDeviceCoordinates(newx,newy);

					mouseevent.point.x = newx;
					mouseevent.point.y = newy;					
							//mouseevent.point.x = hitPoint.h;
							//mouseevent.point.y = hitPoint.v;					
							lastwindow->DispatchMouseEvent(mouseevent);
							}

						// mouseenter
						this->SetCurrentWindow(thewindow);
						mouseevent.message = NS_MOUSE_ENTER;
						mouseevent.widget  = (nsWindow *) thewindow;
						hitPoint = aTheEvent->where;
						GlobalToLocal(&hitPoint);
					// calculate the offset for the event passed in	
					newx = hitPoint.h;
					newy = hitPoint.v;
					thewindow->ConvertToDeviceCoordinates(newx,newy);

					mouseevent.point.x = newx;
					mouseevent.point.y = newy;					
						//mouseevent.point.x = hitPoint.h;
						//mouseevent.point.y = hitPoint.v;					
						thewindow->DispatchMouseEvent(mouseevent);	
						}
					else
						{
						// mousedown inside the content region
						mouseevent.message = NS_MOUSE_MOVE;
						mouseevent.widget  = (nsWindow *) thewindow;
						hitPoint = aTheEvent->where;
						GlobalToLocal(&hitPoint);
					// calculate the offset for the event passed in	
					newx = hitPoint.h;
					newy = hitPoint.v;
					thewindow->ConvertToDeviceCoordinates(newx,newy);

					mouseevent.point.x = newx;
					mouseevent.point.y = newy;					
						//mouseevent.point.x = hitPoint.h;
						//mouseevent.point.y = hitPoint.v;		
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
					hitPoint = aTheEvent->where;
					GlobalToLocal(&hitPoint);
					// calculate the offset for the event passed in	
					newx = hitPoint.h;
					newy = hitPoint.v;
					lastwindow->ConvertToDeviceCoordinates(newx,newy);

					mouseevent.point.x = newx;
					mouseevent.point.y = newy;					
					//mouseevent.point.x = hitPoint.h;
					//mouseevent.point.y = hitPoint.v;					
					lastwindow->DispatchMouseEvent(mouseevent);
					}
				break;
			}
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
nsEventStatus	eventStatus;
nsTextWidget	*widget;

	ch = (char)(aTheEvent->message & charCodeMask);
	if(aTheEvent->modifiers&cmdKey)
		{
		// do a menu key command
		long menuResult = MenuKey(ch);
		if (HiWord(menuResult) != 0)
			DoMenu(aTheEvent, menuResult);
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
			  keyEvent.keyCode   = 1;
			  keyEvent.time      = 0; 
			  keyEvent.isShift   = PR_FALSE; 
			  keyEvent.isControl = PR_FALSE;
			  keyEvent.isAlt     = PR_FALSE;
			  keyEvent.eventStructType = NS_KEY_EVENT;
			  keyEvent.widget = thewidget;
			  
			  thechar = aTheEvent->message&charCodeMask;
			  keyEvent.keyCode = thechar;
			  
			  if(thechar == 13)
			  	keyEvent.message = NS_KEY_UP;
			  else
			  	keyEvent.message = NS_KEY_DOWN;
			  
			  
			  if (!thewidget->DispatchEvent(&keyEvent, eventStatus))
			  	{
			  	// if this is a nsTextWidget
			  	//if (NS_OK == thewidget->QueryInterface(kITEXTWIDGETIID, (void**) &widget) )
			  	if (thechar != 13 )  // do not process enter
			  		{
			  		widget = (nsTextWidget*)thewidget;
			  		widget->PrimitiveKeyDown(thechar,0);
			  		}
			  	}
			  
			  //((nsWindow*)thewidget)->OnKey(NS_KEY_DOWN, 1, &keyEvent);
				//thewidget->kdsjfkj()
				}
			}
		}
}

//=================================================================
/*  Turns a menu event into a raptor menu event
 *  @update  ps 09/21/98
 *  @param   aTheEvent -- A pointer to a Macintosh EventRecord
 *  @return  NONE
 */
void 
nsMacMessagePump::DoMenu(EventRecord *aTheEvent, long menuResult)
{
WindowPtr 	whichwindow;
nsMenuEvent theEvent;
nsEventStatus eventStatus;
nsWindow* 	raptorWindow = nsnull;
nsRefData		*theRefData;

#if 1
	whichwindow = FrontWindow();
	if (whichwindow)
	{
		theRefData = (nsRefData*)GetWRefCon (whichwindow);
		raptorWindow = (nsWindow*)theRefData->GetTopWidget();
	}
#else
	// XXX For some reason this returns null... which is bad...
	raptorWindow = mToolkit->GetFocus();
#endif
    

	// Convert MacOS menu item to PowerPlant menu item
	// because we use Constructor for resource editing
	long menuID = HiWord(menuResult);
	long menuItem = LoWord(menuResult);
	Int16**	theMcmdH = (Int16**) ::GetResource('Mcmd', menuID);
	if (theMcmdH != nil)
	{
		if (::GetHandleSize((Handle)theMcmdH) > 0)
		{
			Int16 numCommands = (*theMcmdH)[0];
			if (numCommands >= menuItem)
			{
				CommandT* theCommandNums = (CommandT*)(&(*theMcmdH)[1]);
				menuItem = theCommandNums[menuItem-1];
			}
		}
		::ReleaseResource((Handle) theMcmdH);
	}
	menuResult = (menuID << 16) + menuItem;

	// Handle the menu command
	if (raptorWindow)
	{
		theEvent.eventStructType = NS_MENU_EVENT;
		theEvent.message  = NS_MENU_SELECTED;
		theEvent.mCommand = menuResult;
		theEvent.widget   = raptorWindow;
		theEvent.nativeMsg = aTheEvent;
		raptorWindow->DispatchEvent(&theEvent, eventStatus);
	}
	else
	{
		if (gWindowlessMenuEventHandler != nsnull)
			gWindowlessMenuEventHandler(menuResult);
	}
	HiliteMenu(0);
}
