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

// 
// nsMacMessagePump
//
// This file contains the default implementation for the mac event loop. Events that
// pertain to the layout engine are routed there via a MessageSink that is passed in
// at creation time. Events not destined for layout are handled here (such as window
// moved).
//
// Clients may either use this implementation or write their own. Embedding applications
// will almost certainly write their own because they will want control of the event
// loop to do other processing. There is nothing in the architecture which forces the
// embedding app to use anything called a "message pump" so the event loop can actually
// live anywhere the app wants.
//

#include "nsMacMessagePump.h"
#include "nsMacMessageSink.h"
#include "nsWidgetsCID.h"
#include "nsToolkit.h"
#include "nscore.h"

#include "nsRepeater.h"

#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "plevent.h"
#include "prthread.h"
#include "nsMacTSMMessagePump.h"

#include <MacWindows.h>
#include <ToolUtils.h>
#include <DiskInit.h>
#include <LowMem.h>

#ifndef topLeft
#define topLeft(r)	(((Point *) &(r))[0])
#endif

#ifndef botRight
#define botRight(r)	(((Point *) &(r))[1])
#endif

#if DEBUG
#include <SIOUX.h>
#include "macstdlibextras.h"
#endif

#define DRAW_ON_RESIZE	1		// if 1, enable live-resize except when the command key is down

const short	kMinWindowWidth = 300;
const short kMinWindowHeight = 150;

NS_WIDGET nsMacMessagePump::nsWindowlessMenuEventHandler nsMacMessagePump::gWindowlessMenuEventHandler = nsnull;

//======================================================================================
//									PROFILE
//======================================================================================
#ifdef DEBUG

// Important Notes:
// ----------------
//
//  - To turn the profiler on, define "#pragma profile on" in IDE_Options.h
//    then set $PROFILE to 1 in BuildNGLayoutDebug.pl and recompile everything.
//
//  - You may need to turn the profiler off ("#pragma profile off")
//	  in NSPR.Debug.Prefix because of incompatiblity with NSPR threads.
//    It usually isn't a problem but it may be one when profiling things like
//    imap or network i/o.
//
//	- The profiler utilities (ProfilerUtils.c) and the profiler
//	  shared library (ProfilerLib) sit in NSRuntime.mcp.
//

		// Define this if you want to start profiling when the Caps Lock
		// key is pressed. Press Caps Lock, start the command you want to
		// profile, release Caps Lock when the command is done. It works
		// for all the major commands: display a page, open a window, etc...
		//
		// If you want to profile the project, you must make sure that the
		// global prefix file (IDE_Options.h) contains "#pragma profile on".

//#define PROFILE


		// Define this if you want to let the profiler run while you're
		// spending time in other apps. Usually you don't.

//#define PROFILE_WAITNEXTEVENT


#ifdef PROFILE
#include "ProfilerUtils.h"
#endif //PROFILE
#endif //DEBUG

//======================================================================================

static Boolean KeyDown(const UInt8 theKey)
{
	KeyMap map;
	GetKeys(map);
	return ((*((UInt8 *)map + (theKey >> 3)) >> (theKey & 7)) & 1) != 0;
}

//=================================================================

static long ConvertOSMenuResultToPPMenuResult(long menuResult)
{
	// Convert MacOS menu item to PowerPlant menu item because
	// in our sample app, we use Constructor for resource editing
	long menuID = HiWord(menuResult);
	long menuItem = LoWord(menuResult);
	SInt16**	theMcmdH = (SInt16**) ::GetResource('Mcmd', menuID);
	if (theMcmdH != nil)
	{
		if (::GetHandleSize((Handle)theMcmdH) > 0)
		{
			SInt16 numCommands = (*theMcmdH)[0];
			if (numCommands >= menuItem)
			{
				SInt32* theCommandNums = (SInt32*)(&(*theMcmdH)[1]);
				menuItem = theCommandNums[menuItem-1];
			}
		}
		::ReleaseResource((Handle) theMcmdH);
	}
	menuResult = (menuID << 16) + menuItem;
	return (menuResult);
}

#pragma mark -

static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);

//=================================================================
/*  Constructor
 *  @update  dc 08/31/98
 *  @param   aToolkit -- The toolkit created by the application
 *  @return  NONE
 */
nsMacMessagePump::nsMacMessagePump(nsToolkit *aToolkit, nsMacMessageSink* aSink)
	: mToolkit(aToolkit), mMessageSink(aSink), mTSMMessagePump(NULL)
{
	mRunning = PR_FALSE;
	mMouseRgn = ::NewRgn();

	//
	// create the TSM Message Pump
	//
	mTSMMessagePump = new nsMacTSMMessagePump();
	NS_ASSERTION(mTSMMessagePump!=NULL,"nsMacMessagePump::nsMacMessagePump: Unable to create TSM Message Pump.");
	
}

//=================================================================
/*  Destructor
 *  @update  dc 08/31/98
 *  @param   NONE
 *  @return  NONE
 */
nsMacMessagePump::~nsMacMessagePump()
{
	if (mMouseRgn)
		::DisposeRgn(mMouseRgn);

  //¥TODO? release the toolkits and sinks? not if we use COM_auto_ptr.
	
  //
  // release the TSM Message Pump
  //
  if (mTSMMessagePump)
  	delete mTSMMessagePump;
}

//=================================================================
/*  Runs the message pump for the macintosh
 *  @update  dc 08/31/98
 *  @param   NONE
 */
void nsMacMessagePump::DoMessagePump()
{
	PRBool				haveEvent;
	EventRecord			theEvent;

	mInBackground = PR_FALSE;
	
	while (mRunning)
	{			
#ifdef PROFILE 
		if (KeyDown(0x39))	// press [caps lock] to start the profile
			ProfileStart();
		else
			ProfileStop(); 
#endif // PROFILE 

#ifdef PROFILE 
#ifndef PROFILE_WAITNEXTEVENT 
	 	ProfileSuspend(); 
#endif // PROFILE_WAITNEXTEVENT 
#endif // PROFILE 

		haveEvent = GetEvent(theEvent);

#ifdef PROFILE 
#ifndef PROFILE_WAITNEXTEVENT 
		ProfileResume(); 
#endif // PROFILE_WAITNEXTEVENT 
#endif // PROFILE 

		DispatchEvent(haveEvent, &theEvent);
	}
}

//=================================================================
/*  Fetch a single event
 *  @update  dc 08/31/98
 *  @param   NONE
 *  @return  A boolean which states whether we have a real event
 */
PRBool nsMacMessagePump::GetEvent(EventRecord &theEvent)
{
	long				sleep		= 0;
	unsigned short		eventMask 	= everyEvent;

	::LMSetSysEvtMask(eventMask);	// we need keyUp events
	PRBool haveEvent = ::WaitNextEvent(eventMask, &theEvent, sleep, mMouseRgn) ? PR_TRUE : PR_FALSE;
	if (haveEvent && TSMEvent(&theEvent) )
	{
		haveEvent = PR_FALSE;
	}

	if (mMouseRgn)
	{
		Point globalMouse = theEvent.where;
		::SetRectRgn(mMouseRgn, globalMouse.h, globalMouse.v, globalMouse.h + 1, globalMouse.v + 1);
	}

	return haveEvent;
}

//=================================================================
/*  Dispatch a single event
 *  @param   theEvent - the event to dispatch
 */
void nsMacMessagePump::DispatchEvent(PRBool aRealEvent, EventRecord *anEvent)
{

	if (aRealEvent == PR_TRUE)
	{

#if DEBUG
		if (SIOUXHandleOneEvent(anEvent))
			return;
#endif

		switch(anEvent->what)
		{
			case keyUp:
			case keyDown:
			case autoKey:
				DoKey(*anEvent);
				break;

			case mouseDown:
				DoMouseDown(*anEvent);
				break;

			case mouseUp:
				DoMouseUp(*anEvent);
				break;

			case updateEvt:
				DoUpdate(*anEvent);
				break;

			case activateEvt:
				DoActivate(*anEvent);
				break;

			case diskEvt:
				DoDisk(*anEvent);
				break;

			case osEvt:
				unsigned char eventType = ((anEvent->message >> 24) & 0x00ff);
				switch (eventType)
				{
					case suspendResumeMessage:
						if ((anEvent->message & 1) == resumeFlag)
							mInBackground = PR_FALSE;		// resume message
						else
							mInBackground = PR_TRUE;		// suspend message
						DoMouseMove(*anEvent);
						break;

					case mouseMovedMessage:
						DoMouseMove(*anEvent);
						break;
				}
				break;
		}
	} else {
		DoIdle(*anEvent);
		if (mRunning)
			Repeater::DoIdlers(*anEvent);
	}

	if (mRunning)
		Repeater::DoRepeaters(*anEvent);
}

#pragma mark -
//-------------------------------------------------------------------------
//
// DoUpdate
//
//-------------------------------------------------------------------------
void nsMacMessagePump::DoUpdate(EventRecord &anEvent)
{
	WindowPtr whichWindow = reinterpret_cast<WindowPtr>(anEvent.message)	;
	GrafPtr savePort;
	::GetPort(&savePort);
#if TARGET_CARBON
	::SetPortWindowPort(whichWindow);
#else
	::SetPort(whichWindow);
#endif
	::BeginUpdate(whichWindow);
	// The app can do its own updates here
	DispatchOSEventToRaptor(anEvent, whichWindow);
	::EndUpdate(whichWindow);
	::SetPort(savePort);
}


//-------------------------------------------------------------------------
//
// DoMouseDown
//
//-------------------------------------------------------------------------

void nsMacMessagePump::DoMouseDown(EventRecord &anEvent)
{
		WindowPtr			whichWindow;
		PRInt16				partCode;

	partCode = ::FindWindow(anEvent.where, &whichWindow);

	switch (partCode)
	{
			case inSysWindow:
				break;

			case inMenuBar:
			{
			  long menuResult = ::MenuSelect(anEvent.where);
			  if (HiWord(menuResult) != 0)
			  {
				    menuResult = ConvertOSMenuResultToPPMenuResult(menuResult);
			      DoMenu(anEvent, menuResult);
			  }
				break;
			}

			case inContent:
			{
#if TARGET_CARBON
				::SetPortWindowPort(whichWindow);
#else
				::SetPort(whichWindow);
#endif
				if (IsWindowHilited(whichWindow))
					DispatchOSEventToRaptor(anEvent, whichWindow);
				else
					::SelectWindow(whichWindow);
				break;
			}

			case inDrag:
			{
#if TARGET_CARBON
				::SetPortWindowPort(whichWindow);
#else
				::SetPort(whichWindow);
#endif
				if (!(anEvent.modifiers & cmdKey))
					::SelectWindow(whichWindow);
#if TARGET_CARBON
				Rect screenRect;
				::GetRegionBounds(::GetGrayRgn(), &screenRect);
#else
				Rect screenRect = (**::GetGrayRgn()).rgnBBox;
#endif
				::DragWindow(whichWindow, anEvent.where, &screenRect);

				// Dispatch the event because some windows may want to know that they have been moved.
#if 0
				// Hack: we can't use GetMouse here because by the time DragWindow returns, the mouse
				// can be located somewhere else than in the drag bar.
				::GetMouse(&anEvent.where);
				::LocalToGlobal(&anEvent.where);
#else
  #if TARGET_CARBON
				RgnHandle strucRgn = NewRgn();
				::GetWindowRegion ( whichWindow, kWindowStructureRgn, strucRgn );
				Rect strucRect;
				::GetRegionBounds(strucRgn, &strucRect);
				::SetPt(&anEvent.where, strucRect.left, strucRect.top);
				::DisposeRgn ( strucRgn );
  #else
				RgnHandle strucRgn = ((WindowPeek)whichWindow)->strucRgn;
				Rect* strucRect = &(*strucRgn)->rgnBBox;
				::SetPt(&anEvent.where, strucRect->left, strucRect->top);
  #endif
#endif
				DispatchOSEventToRaptor(anEvent, whichWindow);
				break;
			}

			case inGrow:
			{
#if TARGET_CARBON
				::SetPortWindowPort(whichWindow);
#else
				::SetPort(whichWindow);
#endif

//#if DEBUG
				Boolean drawOnResize = (DRAW_ON_RESIZE && ((anEvent.modifiers & cmdKey) == 0));
//#else
//  		Boolean drawOnResize = false;
//#endif
				if (drawOnResize)
				{
					Point oldPt = anEvent.where;
					while (::WaitMouseUp())
					{
				        Repeater::DoRepeaters(anEvent);

						Point origin = {0,0};
						::LocalToGlobal(&origin);
						Point newPt;
						::GetMouse(&newPt);
						::LocalToGlobal(&newPt);
						if (::DeltaPoint(oldPt, newPt))
						{
#if TARGET_CARBON
							Rect portRect;
							::GetWindowPortBounds(whichWindow, &portRect);
#else
							Rect portRect = whichWindow->portRect;
#endif
							short	width = newPt.h - origin.h;
							short	height = newPt.v - origin.v;
							if (width < kMinWindowWidth)
								width = kMinWindowWidth;
							if (height < kMinWindowHeight)
								height = kMinWindowHeight;

							oldPt = newPt;
							::SizeWindow(whichWindow, width, height, true);
							::DrawGrowIcon(whichWindow);

							anEvent.where.h = width;	// simulate a click in the grow icon
							anEvent.where.v = height;
							::LocalToGlobal(&anEvent.where);
							DispatchOSEventToRaptor(anEvent, whichWindow);

							Boolean					haveEvent;
							EventRecord			updateEvent;
							haveEvent = ::WaitNextEvent(updateMask, &updateEvent, 0, nil);
							if (haveEvent && TSMEvent(&updateEvent))
							{
								haveEvent = PR_FALSE;
							}
							if (haveEvent)
								DoUpdate(updateEvent);
						}
					}
				}
				else
				{
#if TARGET_CARBON
					Rect sizeRect;
					::GetRegionBounds(::GetGrayRgn(), &sizeRect);
#else
					Rect sizeRect = (**::GetGrayRgn()).rgnBBox;
#endif
					sizeRect.top = kMinWindowHeight;
					sizeRect.left = kMinWindowWidth;
					long newSize = ::GrowWindow(whichWindow, anEvent.where, &sizeRect);
					if (newSize != 0)
						::SizeWindow(whichWindow, newSize & 0x0FFFF, (newSize >> 16) & 0x0FFFF, true);
					::DrawGrowIcon(whichWindow);
#if TARGET_CARBON
					Rect portRect;
					Point newPt = botRight(*::GetWindowPortBounds(whichWindow, &portRect));
#else
					Point newPt = botRight(whichWindow->portRect);
#endif
					::LocalToGlobal(&newPt);
					anEvent.where = newPt;	// important!
					DispatchOSEventToRaptor(anEvent, whichWindow);
				}
				break;
			}

			case inGoAway:
			{
#if TARGET_CARBON
				::SetPortWindowPort(whichWindow);
#else
				::SetPort(whichWindow);
#endif
				if (::TrackGoAway(whichWindow, anEvent.where))
					DispatchOSEventToRaptor(anEvent, whichWindow);
				break;
			}

			case inZoomIn:
			case inZoomOut:
				GrafPtr		savePort;
				GDHandle	gdNthDevice;
				GDHandle	gdZoomDevice;
				Rect		theSect;
				Rect		tempRect;
				Rect		zoomRect;
				short		wTitleHeight;
				long		sectArea, greatestArea = 0;
				Boolean		sectFlag;
				
				GetPort(&savePort);
#if TARGET_CARBON
				::SetPortWindowPort(whichWindow);
				Rect windRect;
				::GetWindowPortBounds(whichWindow, &windRect);
				::EraseRect(&windRect);
#else
				SetPort(whichWindow);
				EraseRect(&whichWindow->portRect);
#endif
				
				if (partCode == inZoomOut)
				{
#if !TARGET_CARBON
					WindowPeek wPeek = (WindowPeek)whichWindow;
					Rect windRect = whichWindow->portRect;
#endif
					LocalToGlobal((Point *)&windRect.top);
					LocalToGlobal((Point *)&windRect.bottom);
#if TARGET_CARBON
					RgnHandle structRgn = ::NewRgn();
					::GetWindowRegion ( whichWindow, kWindowStructureRgn, structRgn );
					Rect structRgnBounds;
					::GetRegionBounds ( structRgn, &structRgnBounds );
					wTitleHeight = windRect.top - 1 - structRgnBounds.top;
					::DisposeRgn ( structRgn );
#else
					wTitleHeight = windRect.top - 1 - (*(wPeek->strucRgn))->rgnBBox.top;
#endif
					windRect.top -= wTitleHeight;
					gdNthDevice = GetDeviceList();
					while (gdNthDevice)
					{
						if (TestDeviceAttribute(gdNthDevice, screenDevice))
							if (TestDeviceAttribute(gdNthDevice, screenActive))
							{
								sectFlag = SectRect(&windRect, &(**gdNthDevice).gdRect, &theSect);
								sectArea = (theSect.right - theSect.left) * (theSect.bottom - theSect.top);
								if (sectArea > greatestArea)
								{
									greatestArea = sectArea;
									gdZoomDevice = gdNthDevice;
								}
							}
						gdNthDevice = GetNextDevice(gdNthDevice);
					}
					if (gdZoomDevice == GetMainDevice())
						wTitleHeight += GetMBarHeight();
					tempRect = (**gdZoomDevice).gdRect;
					SetRect(&zoomRect,
						tempRect.left + 3,
						tempRect.top + wTitleHeight + 3,
						tempRect.right - 64,
						tempRect.bottom - 3);
#if TARGET_CARBON
					::SetWindowStandardState ( whichWindow, &zoomRect );
#else
					(**(WStateDataHandle)(wPeek->dataHandle)).stdState = zoomRect;
#endif
				}
				
				SetPort(savePort);
				
				// !!!	Do not call ZoomWindow before calling DispatchOSEventToRaptor
				// 		otherwise nsMacEventHandler::HandleMouseDownEvent won't get
				//		the right partcode for the click location
				
				DispatchOSEventToRaptor(anEvent, whichWindow);
				
				break;
	}
}


//-------------------------------------------------------------------------
//
// DoMouseUp
//
//-------------------------------------------------------------------------
void nsMacMessagePump::DoMouseUp(EventRecord &anEvent)
{
		WindowPtr			whichWindow;
		PRInt16				partCode;

	partCode = ::FindWindow(anEvent.where, &whichWindow);
	if (whichWindow == nil)
	{
		// We need to report the event even when it happens over no window:
		// when the user clicks a widget, keeps the mouse button pressed and
		// releases it outside the window, the event needs to be reported to
		// the widget so that it can deactivate itself.
		whichWindow = ::FrontWindow();
	}
	DispatchOSEventToRaptor(anEvent, whichWindow);
}


//-------------------------------------------------------------------------
//
// DoMouseMove
//
//-------------------------------------------------------------------------
void  nsMacMessagePump::DoMouseMove(EventRecord &anEvent)
{
	// same thing as DoMouseUp
		WindowPtr			whichWindow;
		PRInt16				partCode;

	partCode = ::FindWindow(anEvent.where, &whichWindow);
	if (whichWindow == nil)
		whichWindow = ::FrontWindow();
	DispatchOSEventToRaptor(anEvent, whichWindow);
}


//-------------------------------------------------------------------------
//
// DoKey
// 
// This is called for keydown, keyup, and key repeating events. So we need
// to be careful not to do things twice.
//-------------------------------------------------------------------------
void  nsMacMessagePump::DoKey(EventRecord &anEvent)
{
	char theChar = (char)(anEvent.message & charCodeMask);
	//if ((anEvent.what == keyDown) && ((anEvent.modifiers & cmdKey) != 0))
	//{
		// do a menu key command
	//	long menuResult = ::MenuKey(theChar);
	//	if (HiWord(menuResult) != 0)
	//	{
	//    menuResult = ConvertOSMenuResultToPPMenuResult(menuResult);
	//		DoMenu(anEvent, menuResult);
	//	}
	//}
	//else
	{
		PRBool handled = DispatchOSEventToRaptor(anEvent, ::FrontWindow());
		if((!handled) && (anEvent.what == keyDown) && ((anEvent.modifiers & cmdKey) != 0) )
		{
			// do a menu key command
			long menuResult = ::MenuKey(theChar);
			if (HiWord(menuResult) != 0)
			{
			    menuResult = ConvertOSMenuResultToPPMenuResult(menuResult);
				DoMenu(anEvent, menuResult);
			}
		}
	}
}


//-------------------------------------------------------------------------
//
// DoDisk
//
//-------------------------------------------------------------------------
void nsMacMessagePump::DoDisk(const EventRecord& anEvent)
{
	if (HiWord(anEvent.message) != noErr)
	{
		// Error mounting disk. Ask if user wishes to format it.	
		Point pt = {120, 120};	// System 7 will auto-center dialog
		::DILoad();
		::DIBadMount(pt, (SInt32) anEvent.message);
		::DIUnload();
	}
}


//-------------------------------------------------------------------------
//
// DoMenu
//
//-------------------------------------------------------------------------
void  nsMacMessagePump::DoMenu(EventRecord &anEvent, long menuResult)
{
	// The app can handle its menu commands here or
	// in the nsNativeBrowserWindow and nsNativeViewerApp
	if (mMessageSink->IsRaptorWindow(::FrontWindow()))
	{
		DispatchMenuCommandToRaptor(anEvent, menuResult);
	}
	else
	{
		if (gWindowlessMenuEventHandler != nsnull)
			gWindowlessMenuEventHandler(menuResult);
	}
	HiliteMenu(0);
}


//-------------------------------------------------------------------------
//
// DoActivate
//
//-------------------------------------------------------------------------
void  nsMacMessagePump::DoActivate(EventRecord &anEvent)
{
	WindowPtr whichWindow = (WindowPtr)anEvent.message;
#if TARGET_CARBON
	::SetPortWindowPort(whichWindow);
#else
	::SetPort(whichWindow);
#endif
	if (anEvent.modifiers & activeFlag)
	{
		::BringToFront(whichWindow);
		::HiliteWindow(whichWindow,TRUE);
	}
	else
	{
		::HiliteWindow(whichWindow,FALSE);
	}

	DispatchOSEventToRaptor(anEvent, whichWindow);
}


//-------------------------------------------------------------------------
//
// DoIdle
//
//-------------------------------------------------------------------------
void  nsMacMessagePump::DoIdle(EventRecord &anEvent)
{
	// send mouseMove event
		static Point	lastWhere = {0, 0};

	if (*(long*)&lastWhere == *(long*)&anEvent.where)
		return;

	lastWhere = anEvent.where;
	DoMouseMove(anEvent);
}


#pragma mark -
//-------------------------------------------------------------------------
//
// DispatchOSEventToRaptor
//
//-------------------------------------------------------------------------
PRBool  nsMacMessagePump::DispatchOSEventToRaptor(
													EventRecord 	&anEvent,
													WindowPtr			aWindow)
{
	PRBool		handled = PR_FALSE;
	
	if (mMessageSink->IsRaptorWindow(aWindow))
		handled = mMessageSink->DispatchOSEvent(anEvent, aWindow);
		
	return handled;
}


//-------------------------------------------------------------------------
//
// DispatchMenuCommandToRaptor
//
//-------------------------------------------------------------------------
PRBool nsMacMessagePump::DispatchMenuCommandToRaptor(
													EventRecord 	&anEvent,
													long					menuResult)
{
	PRBool		handled = PR_FALSE;

	if (mMessageSink->IsRaptorWindow(::FrontWindow()))
		handled = mMessageSink->DispatchMenuCommand(anEvent, menuResult);

	return handled;
}
