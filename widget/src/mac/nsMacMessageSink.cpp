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

#include "nsMacMessageSink.h"
#include "nsMacWindow.h"


//-------------------------------------------------------------------------
//
// DispatchOSEvent
//
//	Return PR_TRUE if the event was handled
//
//-------------------------------------------------------------------------
NS_EXPORT PRBool nsMacMessageSink::DispatchOSEvent(
													EventRecord 	&anEvent,
													WindowPtr			aWindow)
{
	PRBool		eventHandled = PR_FALSE;
	
	nsMacWindow* raptorWindow = GetNSWindowFromMacWindow(aWindow);

	if (raptorWindow)
	{
		eventHandled = raptorWindow->HandleOSEvent(anEvent);
  }
  
  return eventHandled;
}


//-------------------------------------------------------------------------
//
// DispatchMenuCommand
//
//-------------------------------------------------------------------------
NS_EXPORT PRBool nsMacMessageSink::DispatchMenuCommand(
													EventRecord 	&anEvent,
													long					menuResult)
{
	PRBool		eventHandled = PR_FALSE;

	nsMacWindow* raptorWindow = GetNSWindowFromMacWindow(::FrontWindow());
	
	if (raptorWindow)
	{
		eventHandled = raptorWindow->HandleMenuCommand(anEvent, menuResult);
	}
  return eventHandled;
}


#pragma mark -

//-------------------------------------------------------------------------
//
// GetNSWindowFromWindow
//
//-------------------------------------------------------------------------
NS_EXPORT nsMacWindow *nsMacMessageSink::GetNSWindowFromMacWindow(WindowPtr inWindow)
{
	if (!inWindow) return nsnull;
	
	nsMacWindow	*foundMacWindow = GetRaptorWindowList()[inWindow];
	return foundMacWindow;
}

//-------------------------------------------------------------------------
//
// GetRaptorWindowList
//
//-------------------------------------------------------------------------
/* static */nsMacMessageSink::TWindowMap& nsMacMessageSink::GetRaptorWindowList()
{
	static TWindowMap		sRaptorWindowList;

	return sRaptorWindowList;
}


//-------------------------------------------------------------------------
//
// IsRaptorWindow
//
//-------------------------------------------------------------------------
NS_EXPORT PRBool nsMacMessageSink::IsRaptorWindow(WindowPtr inWindow)
{
	if (!inWindow) return PR_FALSE;
	return (GetRaptorWindowList()[inWindow] != nsnull);
}


//-------------------------------------------------------------------------
//
// AddRaptorWindowToList
//
//-------------------------------------------------------------------------
/* static */ void nsMacMessageSink::AddRaptorWindowToList(WindowPtr wind, nsMacWindow* theRaptorWindow)
{
	TWindowMap&		windowList = GetRaptorWindowList();
	windowList[wind] = theRaptorWindow;
}


//-------------------------------------------------------------------------
//
// RemoveRaptorWindowFromList
//
//-------------------------------------------------------------------------
/* static */ void	nsMacMessageSink::RemoveRaptorWindowFromList(WindowPtr wind)
{
	TWindowMap&		windowList = GetRaptorWindowList();
	windowList.erase(wind);
}


