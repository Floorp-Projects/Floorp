/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

#include "nsToolkit.h"
#include "nsWindow.h"
#include "nsGUIEvent.h"
#include "plevent.h"


nsWindow* nsToolkit::mFocusedWidget = nsnull;
PLEventQueue*	nsToolkit::sPLEventQueue = nsnull;

extern "C" NS_EXPORT PLEventQueue* GetMacPLEventQueue()
{
	return nsToolkit::GetEventQueue();
 }
 
//=================================================================
/*  Constructor
 *  @update  dc 08/31/98
 *  @param   NONE
 *  @return  NONE
 */
nsToolkit::nsToolkit(): Repeater()
{
	NS_INIT_REFCNT();
}


//=================================================================
/*  Destructor.
 *  @update  dc 08/31/98
 *  @param   NONE
 *  @return  NONE
 */
nsToolkit::~nsToolkit()
{
	StopRepeating();
}

//=================================================================
/*  Set the focus to a widget, send out the appropriate focus/defocus events
 *  @update  dc 08/31/98
 *  @param   aMouseInside -- A boolean indicating if the mouse is inside the control
 *  @return  NONE
 */
void nsToolkit::SetFocus(nsWindow *aFocusWidget)
{ 
	nsGUIEvent		guiEvent;
	
	guiEvent.eventStructType = NS_GUI_EVENT;

	if (aFocusWidget == mFocusedWidget)
		return;
		
	// tell the old widget, it is not focused
	if (mFocusedWidget)
	{
		guiEvent.message = NS_LOSTFOCUS;
		guiEvent.widget = mFocusedWidget;
		mFocusedWidget->DispatchWindowEvent(guiEvent);
		mFocusedWidget = nil;
	}
	
	// let the new one know
	if (aFocusWidget)
	{
		mFocusedWidget =  aFocusWidget;
		guiEvent.message = NS_GOTFOCUS;
		guiEvent.widget = mFocusedWidget;		
		mFocusedWidget->DispatchWindowEvent(guiEvent);
	}
}

//=================================================================
/*  nsISupports implementation macro's
 *  @update  dc 08/31/98
 *  @param   NONE
 *  @return  NONE
 */
NS_DEFINE_IID(kIToolkitIID, NS_ITOOLKIT_IID);
NS_IMPL_ISUPPORTS(nsToolkit,kIToolkitIID);

//=================================================================
/*  Initialize the Toolbox
 *  @update  dc 08/31/98
 *  @param   aThread -- A pointer to a PRThread, not really sure of its use for the Mac yet
 *  @return  NONE
 */
NS_IMETHODIMP nsToolkit::Init(PRThread *aThread)
{
	 // Create the NSPR event Queue and start the repeater
	if ( sPLEventQueue == NULL )
		sPLEventQueue = PL_CreateEventQueue("toolkit", aThread);
 	StartRepeating();
	
	return NS_OK;
}


//=================================================================
/*  Process the NSPR event queue. 
 *  @update  dc 08/31/98
 *  @param   inMacEvent -- A mac os event, Not used
 *  @return  NONE
 */
void	nsToolkit::RepeatAction(const EventRecord& /*inMacEvent*/)
{
	// Handle pending NSPR events
	PL_ProcessPendingEvents( sPLEventQueue );
}
