/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 */

#include "nsCOMPtr.h"

#include "nsCBaseAppLoop.h"
#include "nsCEvent.h"
#include "nsCEventFilter.h"

//*****************************************************************************
//***    nsCBaseAppLoop: Object Management
//*****************************************************************************

nsCBaseAppLoop::nsCBaseAppLoop() : nsCPlatformBaseLoop(nsEventLoopTypes::MainAppLoop)
{
}

nsCBaseAppLoop::~nsCBaseAppLoop()
{
}


//*****************************************************************************
// nsCBaseAppLoop:: App Loop Runs
//*****************************************************************************   

nsresult nsCBaseAppLoop::RunWithNoListener(nsIEvent* event,
	nsIEventFilter* filter)
{
	nsresult rv;

	void* platformEventData = GetPlatformEventData(event);
	void* platformFilterData = GetPlatformFilterData(filter);
	NS_ENSURE(platformEventData, NS_ERROR_FAILURE);
								
	while(NS_OK == (rv = PlatformGetNextEvent(platformFilterData, platformEventData)))
		{
		if(NS_FAILED(rv = PlatformTranslateEvent(platformEventData)))
			break;

		if(NS_FAILED(rv = PlatformDispatchEvent(platformEventData)))
			break;
		}
			
	return rv;
}

nsresult nsCBaseAppLoop::RunWithTranslateListener(nsIEvent* event,
	nsIEventFilter* filter, nsITranslateListener* translateListener)
{
	nsresult rv;

	void* platformEventData = GetPlatformEventData(event);
	void* platformFilterData = GetPlatformFilterData(filter);
	NS_ENSURE(platformEventData, NS_ERROR_FAILURE);
								
	while(NS_OK == (rv = PlatformGetNextEvent(platformFilterData, platformEventData)))
		{
		if(NS_FAILED(rv = translateListener->PreTranslate(event)))
			break;
		if(NS_OK == rv)
			{
			if(NS_FAILED(rv = PlatformTranslateEvent(platformEventData)))
				break;
			if(NS_FAILED(rv = translateListener->PostTranslate(event,
				NS_OK == rv ? PR_TRUE : PR_FALSE)))
				break;
			}

		if(NS_FAILED(rv = PlatformDispatchEvent(platformEventData)))
			break;
		}
			
	return rv;
}

nsresult nsCBaseAppLoop::RunWithDispatchListener(nsIEvent* event,
	nsIEventFilter* filter, nsIDispatchListener* dispatchListener)
{
	nsresult rv;

	void* platformEventData = GetPlatformEventData(event);
	void* platformFilterData = GetPlatformFilterData(filter);
	NS_ENSURE(platformEventData, NS_ERROR_FAILURE);
								
	while(NS_OK == (rv = PlatformGetNextEvent(platformFilterData, platformEventData)))
		{
		if(NS_FAILED(rv = PlatformTranslateEvent(platformEventData)))
			break;

		if(NS_FAILED(rv = dispatchListener->PreDispatch(event)))
			break;
		if(NS_OK == rv)
			{
			if(NS_FAILED(rv = PlatformDispatchEvent(platformEventData)))
				break;
			if(NS_FAILED(rv = dispatchListener->PostDispatch(event,
				NS_OK == rv ? PR_TRUE : PR_FALSE)))
				break;
			}
		}
			
	return rv;
}

nsresult nsCBaseAppLoop::RunWithTranslateAndDispatchListener(nsIEvent* event,
	nsIEventFilter* filter,	nsITranslateListener* translateListener, 
	nsIDispatchListener* dispatchListener)
{
	nsresult rv;

	void* platformEventData = GetPlatformEventData(event);
	void* platformFilterData = GetPlatformFilterData(filter);
	NS_ENSURE(platformEventData, NS_ERROR_FAILURE);
								
	while(NS_OK == (rv = PlatformGetNextEvent(platformFilterData, platformEventData)))
		{
		if(NS_FAILED(rv = translateListener->PreTranslate(event)))
			break;
		if(NS_OK == rv)
			{
			if(NS_FAILED(rv = PlatformTranslateEvent(platformEventData)))
				break;
			if(NS_FAILED(rv = translateListener->PostTranslate(event,
				NS_OK == rv ? PR_TRUE : PR_FALSE)))
				break;
			}

		if(NS_FAILED(rv = dispatchListener->PreDispatch(event)))
			break;
		if(NS_OK == rv)
			{
			if(NS_FAILED(rv = PlatformDispatchEvent(platformEventData)))
				break;
			if(NS_FAILED(rv = dispatchListener->PostDispatch(event,
				NS_OK == rv ? PR_TRUE : PR_FALSE)))
				break;
			}
		}
			
	return rv;
}