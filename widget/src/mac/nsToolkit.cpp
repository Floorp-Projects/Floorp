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

// Class IDs...
static NS_DEFINE_IID(kEventQueueServiceCID,  NS_EVENTQUEUESERVICE_CID);

// Interface IDs...
static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);


static nsMacNSPREventQueueHandler*	gEventQueueHandler = nsnull;

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsMacNSPREventQueueHandler::nsMacNSPREventQueueHandler(): Repeater()
{
	mRefCnt = 0;
	mEventQService = nsnull;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsMacNSPREventQueueHandler::~nsMacNSPREventQueueHandler()
{
	if (mEventQService == nsnull)
		return;

	StopRepeating();
	nsServiceManager::ReleaseService(kEventQueueServiceCID, mEventQService);
	mEventQService = nsnull;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
void nsMacNSPREventQueueHandler::StartPumping()
{
	if (mRefCnt == 0)
	{
		nsServiceManager::GetService(kEventQueueServiceCID,
																				kIEventQueueServiceIID,
	                                      (nsISupports **)&mEventQService);
		if (mEventQService == nsnull)
		{
			NS_WARNING("GetService(kEventQueueServiceCID) failed");
			return;
		}
	}

	++mRefCnt;
	StartRepeating();
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
PRBool nsMacNSPREventQueueHandler::StopPumping()
{
	if (mEventQService == nsnull)
		return PR_TRUE;

	if (mRefCnt > 0) {
		if (--mRefCnt == 0) {
			StopRepeating();
		 	nsServiceManager::ReleaseService(kEventQueueServiceCID, mEventQService);
		 	mEventQService = nsnull;
			return PR_TRUE;
		}
	}

	return PR_FALSE;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
void nsMacNSPREventQueueHandler::RepeatAction(const EventRecord& inMacEvent)
{
	// Handle pending NSPR events
	if (mEventQService)
	  mEventQService->ProcessEvents();
}


#pragma mark -

NS_DEFINE_IID(kIToolkitIID, NS_ITOOLKIT_IID);
NS_IMPL_ISUPPORTS(nsToolkit,kIToolkitIID);

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsToolkit::nsToolkit()
{
	NS_INIT_REFCNT();
	if (gEventQueueHandler == nsnull)
		gEventQueueHandler = new nsMacNSPREventQueueHandler;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
nsToolkit::~nsToolkit()
{
	if (gEventQueueHandler) {
		if (gEventQueueHandler->StopPumping()) {
			delete gEventQueueHandler;
			gEventQueueHandler = nsnull;
		}
	}
}


//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
NS_IMETHODIMP nsToolkit::Init(PRThread */*aThread*/)
{
	if (gEventQueueHandler)
		gEventQueueHandler->StartPumping();
	return NS_OK;
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
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
