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

#include "nsAppShell.h"
#include "nsIAppShell.h"
#include "nsWindow.h"
#include <stdlib.h>


//XtAppContext gAppContext;

//-------------------------------------------------------------------------
//
// nsISupports implementation macro
//
//-------------------------------------------------------------------------
NS_DEFINE_IID(kIAppShellIID, NS_IAPPSHELL_IID);
NS_IMPL_ISUPPORTS(nsAppShell,kIAppShellIID);

void nsAppShell::SetDispatchListener(nsDispatchListener* aDispatchListener)
{
  mDispatchListener = aDispatchListener;
}

//-------------------------------------------------------------------------
//
// Create the application shell
//
//-------------------------------------------------------------------------

void nsAppShell::Create(int* argc, char ** argv)
{
		mToolKit = new nsToolkit();
}

//-------------------------------------------------------------------------
//
// Enter a message handler loop
//
//-------------------------------------------------------------------------

nsresult nsAppShell::Run()
{
EventRecord		theevent;
RgnHandle			fMouseRgn=NULL;
long					sleep=0;
PRInt16				haveevent;
WindowPtr			whichwindow;

#define SUSPENDRESUMEMESSAGE 0x01
#define MOUSEMOVEDMESSAGE				0xFA

	mRunning = TRUE;

	
	while(mRunning)
		{
		haveevent = ::WaitNextEvent(everyEvent,&theevent,sleep,fMouseRgn);
		if(haveevent)
			{
			switch(theevent.what)
				{
				case nullEvent:
					IdleWidgets();
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
					doKey(&theevent);
					break;
					this->Exit();
					break;
				case mouseDown:
					DoMouseDown(&theevent);
					break;
				case mouseUp:
					break;
				case updateEvt:
					whichwindow = (WindowPtr)theevent.message;
					break;
				case activateEvt:
					whichwindow = (WindowPtr)theevent.message;
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
void nsAppShell::IdleWidgets()
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
void nsAppShell::DoMouseDown(EventRecord *aTheEvent)
{
WindowPtr			whichwindow;
PRInt16				partcode;
nsWindow			*thewindow;
nsIWidget			*thewidget;

	partcode = FindWindow(aTheEvent->where,&whichwindow);

	if(whichwindow!=0)
		{
		thewindow = (nsWindow*)(((WindowPeek)whichwindow)->refCon);
		thewidget = thewindow->FindWidgetHit(aTheEvent->where);
		
		switch(partcode)
			{
			case inSysWindow:
				break;
			case inContent:
				break;
			case inDrag:
				break;
			case inGrow:
				break;
			case inGoAway:
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
void nsAppShell::doKey(EventRecord *aTheEvent)
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

//-------------------------------------------------------------------------
//
// Exit a message handler loop
//
//-------------------------------------------------------------------------

void nsAppShell::Exit()
{
  mRunning = FALSE;
}

//-------------------------------------------------------------------------
//
// nsAppShell constructor
//
//-------------------------------------------------------------------------
nsAppShell::nsAppShell()
{ 
  mRefCnt = 0;
  mDispatchListener = 0;
}

//-------------------------------------------------------------------------
//
// nsAppShell destructor
//
//-------------------------------------------------------------------------
nsAppShell::~nsAppShell()
{
}

//-------------------------------------------------------------------------
//
// GetNativeData
//
//-------------------------------------------------------------------------
void* nsAppShell::GetNativeData(PRUint32 aDataType)
{
  if (aDataType == NS_NATIVE_SHELL) 
  	{
    //return mTopLevel;
  	}
  return nsnull;
}


