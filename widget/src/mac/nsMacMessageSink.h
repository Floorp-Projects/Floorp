/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#if USE_MENUSELECT
	PRBool					DispatchMenuCommand(EventRecord &anEvent, long menuResult, WindowPtr aWindow);
#endif

	static void			AddRaptorWindowToList(WindowPtr wind, nsMacWindow* theRaptorWindow);
	static void			RemoveRaptorWindowFromList(WindowPtr wind);
	static nsMacWindow*		GetNSWindowFromMacWindow(WindowPtr wind);

	PRBool					IsRaptorWindow(WindowPtr inWindow);

protected:
	
	static TWindowMap&		GetRaptorWindowList();

};



#endif // nsMacMessageSink_h__

