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

#ifndef nsMacMessagePump_h__
#define nsMacMessagePump_h__

#include "nsToolKit.h"
#include "nsWindow.h"
#include <Fonts.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <Traps.h>
#include <Events.h>
#include <Menus.h>



//================================================


// Macintosh Message Pump Class
class nsMacMessagePump
{
	// CLASS MEMBERS
  private:
	PRBool						mRunning;
	Point							mMousePoint;				// keep track of where the mouse is at all times
	PRBool						mInBackground;
	nsToolkit					*mToolkit;
  static  nsWindow	*gCurrentWindow;
  static  nsWindow  *gGrabWindow;


	// CLASS METHODS
	private:		    
		    	    
  public:
    				nsMacMessagePump(nsToolkit	*aTookKit);
    virtual ~nsMacMessagePump();
  
		PRBool			DoMessagePump();
		void 				DoMouseDown(EventRecord *aTheEvent);
		void				DoMouseUp(EventRecord *aTheEvent);
		void				DoMouseMove(EventRecord *aTheEvent);
		void				DoPaintEvent(EventRecord *aTheEvent);
		void 				DoKey(EventRecord *aTheEvent);
		void 				DoMenu(EventRecord *aTheEvent, long menuItem);
		void 				DoIdleWidgets();
  	void 				SetCurrentWindow(nsWindow *aTheWin) { gCurrentWindow = aTheWin;}
  	void				StopRunning() {mRunning = PR_FALSE;}
  	nsWindow* 	GetCurrentWindow(void) {return(gCurrentWindow);}

};





#endif // nsMacMessagePump_h__

