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

#ifndef nsMacMessagePump_h__
#define nsMacMessagePump_h__

#include <Events.h>
#include "prtypes.h"

class nsToolkit;
class nsMacMessageSink;
struct PLEventQueue;

//================================================

// Macintosh Message Pump Class
class nsMacMessagePump
{
	// CLASS MEMBERS
private:
	PRBool					mRunning;
	Point					mMousePoint;	// keep track of where the mouse is at all times
	PRBool					mInBackground;
	nsToolkit*				mToolkit;
	nsMacMessageSink*       mMessageSink;
	PLEventQueue*			mEventQueue;

	// CLASS METHODS
		    	    
public:
					nsMacMessagePump(nsToolkit *aToolKit, nsMacMessageSink* aSink);
	virtual 		~nsMacMessagePump();
  
	void			DoMessagePump();
	PRBool			GetEvent(EventRecord &theEvent);
	void			DispatchEvent(PRBool aRealEvent, EventRecord *anEvent);
	void			StartRunning() {mRunning = PR_TRUE;}
	void			StopRunning() {mRunning = PR_FALSE;}

private:
	void 			DoMouseDown(EventRecord &anEvent);
	void			DoMouseUp(EventRecord &anEvent);
	void			DoMouseMove(EventRecord &anEvent);
	void			DoUpdate(EventRecord &anEvent);
	void 			DoKey(EventRecord &anEvent);
	void 			DoMenu(EventRecord &anEvent, long menuResult);
	void 			DoDisk(const EventRecord &anEvent);
	void			DoActivate(EventRecord &anEvent);
	void			DoIdle(EventRecord &anEvent);

	PRBool		DispatchOSEventToRaptor(EventRecord &anEvent, WindowPtr aWindow);
	PRBool		DispatchMenuCommandToRaptor(EventRecord &anEvent, long menuResult);

private:
	typedef void (*nsWindowlessMenuEventHandler) (PRInt32 menuResult);
	static nsWindowlessMenuEventHandler gWindowlessMenuEventHandler;
public:
	static void SetWindowlessMenuEventHandler(nsWindowlessMenuEventHandler func)
									{gWindowlessMenuEventHandler = func;}
};





#endif // nsMacMessagePump_h__

