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

// 
// nsAppShellCocoa
//
// This file contains the default implementation of the application shell. Clients
// may either use this implementation or write their own. If you write your
// own, you must create a message sink to route events to. (The message sink
// interface may change, so this comment must be updated accordingly.)
//

#undef DARWIN
#import <Cocoa/Cocoa.h>

#include "nsAppShellCocoa.h"


NS_IMPL_THREADSAFE_ISUPPORTS1(nsAppShellCocoa, nsIAppShell)


//-------------------------------------------------------------------------
//
// nsAppShellCocoa constructor
//
//-------------------------------------------------------------------------
nsAppShellCocoa::nsAppShellCocoa()
{
  NS_INIT_REFCNT();
}

//-------------------------------------------------------------------------
//
// nsAppShellCocoa destructor
//
//-------------------------------------------------------------------------
nsAppShellCocoa::~nsAppShellCocoa()
{
}


//-------------------------------------------------------------------------
//
// Create the application shell
//
//-------------------------------------------------------------------------

NS_IMETHODIMP
nsAppShellCocoa::Create(int* argc, char ** argv)
{
  // There's reallly not a whole lot that needs to be done here. The
  // window will register its own interest in the necessary events
  // so there's no need for us to create a pump or a sink. 

	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Enter a message handler loop
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsAppShellCocoa::Run(void)
{
  [NSApp run];
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Exit appshell
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsAppShellCocoa::Exit(void)
{
  [NSApp stop];
	return NS_OK;
}


NS_IMETHODIMP
nsAppShellCocoa::SetDispatchListener(nsDispatchListener* aDispatchListener)
{
  // nobody uses this except viewer
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// respond to notifications that an event queue has come or gone
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsAppShellCocoa::ListenToEventQueue(nsIEventQueue * aQueue, PRBool aListen)
{ 
  // unnecessary; handled elsewhere
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Prepare to process events
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsAppShellCocoa::Spinup(void)
{
	return NS_OK;
}


//-------------------------------------------------------------------------
//
// Stop being prepared to process events.
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsAppShellCocoa::Spindown(void)
{
	return NS_OK;
}


//
// GetNativeEvent
// DispatchNativeEvent
//
// These are generally used in the tight loop of a modal event loop. Cocoa
// has other ways of dealing with this, perhaps those should be investigated
// here.
//

NS_METHOD
nsAppShellCocoa::GetNativeEvent(PRBool &aRealEvent, void *&aEvent)
{
  NS_WARNING("GetNativeEvent NOT YET IMPLEMENTED");
  
#if 0
	static EventRecord	theEvent;	// icky icky static (can't really do any better)

	if (!mMacPump.get())
		return NS_ERROR_NOT_INITIALIZED;

	aRealEvent = mMacPump->GetEvent(theEvent);
	aEvent = &theEvent;
#endif
	return NS_OK;
}

NS_METHOD
nsAppShellCocoa::DispatchNativeEvent(PRBool aRealEvent, void *aEvent)
{
  NS_WARNING("DispatchNativeEvent NOT YET IMPLEMENTED");
  
#if 0
	if (!mMacPump.get())
		return NS_ERROR_NOT_INITIALIZED;

	mMacPump->DispatchEvent(aRealEvent, (EventRecord *) aEvent);
#endif
	return NS_OK;
}


