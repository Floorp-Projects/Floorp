/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsMacMessageSink_h__
#define nsMacMessageSink_h__

#include "prtypes.h"

#include <map>

using std::map;

#include <Events.h>
#include <MacWindows.h>

class nsMacWindow;

/*================================================
¥¥¥IMPORTANT
This class should be COM'd and made XP somehow
Right now it is just a normal class as the first step of breaking
it out from the "client" code. It's goal is to be the point of contact between
client code (say, the viewer) and the embedded webshell for event dispatching. From
here, it goes to the appropriate window, however that may be done (and that still
needs to be figured out).
(pinkerton)

This class now has an added responsibility, which
is to maintain a list of raptor windows. This is
stored as an STL map, which maps from a WindowPtr
to a nsMacWindow *.
================================================*/

// Macintosh Message Sink Class
class nsMacMessageSink
{		    	    
private:
	
	typedef map<WindowPtr, nsMacWindow*>	TWindowMap;

public:
									
	PRBool					DispatchOSEvent(EventRecord &anEvent, WindowPtr aWindow);
	PRBool					DispatchMenuCommand(EventRecord &anEvent, long menuResult);

	static void			AddRaptorWindowToList(WindowPtr wind, nsMacWindow* theRaptorWindow);
	static void			RemoveRaptorWindowFromList(WindowPtr wind);
	static nsMacWindow*		GetNSWindowFromMacWindow(WindowPtr wind);

	PRBool					IsRaptorWindow(WindowPtr inWindow);

protected:
	
	static TWindowMap&		GetRaptorWindowList();

};



#endif // nsMacMessageSink_h__

