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

#include <Gestalt.h>
#include <Appearance.h>
#include "nsIEventQueueService.h"
#include "nsIServiceManager.h"
#include "nsXPComCIID.h"
// Class IDs...
static NS_DEFINE_IID(kEventQueueServiceCID,  NS_EVENTQUEUESERVICE_CID);

// Interface IDs...
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);

nsWindow* nsToolkit::mFocusedWidget = nsnull;

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
	if (mFocusedWidget)
	{
	  mFocusedWidget->RemoveDeleteObserver(this);
	  mFocusedWidget = nsnull;
	}

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
	if (aFocusWidget == mFocusedWidget)
		return;
		
	nsGUIEvent guiEvent;
	guiEvent.eventStructType = NS_GUI_EVENT;
	guiEvent.point.x = 0, guiEvent.point.y = 0;
	guiEvent.time = PR_IntervalNow();
	guiEvent.widget = nsnull;
	guiEvent.nativeMsg = nsnull;

	// tell the old widget, it is not focused
	if (mFocusedWidget)
	{
		mFocusedWidget->RemoveDeleteObserver(this);

		guiEvent.message = NS_LOSTFOCUS;
		guiEvent.widget = mFocusedWidget;
		mFocusedWidget->DispatchWindowEvent(guiEvent);
		mFocusedWidget = nsnull;
	}
	
	// let the new one know
	if (aFocusWidget)
	{
		mFocusedWidget = aFocusWidget;
		mFocusedWidget->AddDeleteObserver(this);

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
	nsIEventQueueService* eventQService = NULL;
	if ( NS_SUCCEEDED( nsServiceManager::GetService(kEventQueueServiceCID,
                                      kIEventQueueServiceIID,
                                      (nsISupports **)&eventQService) ) )
    {
    	eventQService->ProcessEvents();
    	nsServiceManager::ReleaseService(kEventQueueServiceCID, eventQService);
    }
}


//=================================================================
/*   
 */
void	nsToolkit::NotifyDelete(void* aDeletedObject)
{
	if (mFocusedWidget == aDeletedObject)
		mFocusedWidget = nsnull;
}


//=================================================================
/*   
 */
bool nsToolkit::HasAppearanceManager()
{

#define APPEARANCE_MIN_VERSION	0x0110		// we require version 1.1
	
	static bool inited = false;
	static bool hasAppearanceManager = false;

	if (inited)
		return hasAppearanceManager;
	inited = true;

	SInt32 result;
	if (::Gestalt(gestaltAppearanceAttr, &result) != noErr)
		return false;		// no Appearance Mgr

	if (::Gestalt(gestaltAppearanceVersion, &result) != noErr)
		return false;		// still version 1.0

	hasAppearanceManager = (result >= APPEARANCE_MIN_VERSION);

	return hasAppearanceManager;
}
