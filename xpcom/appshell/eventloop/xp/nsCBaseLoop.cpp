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

#include "nsCBaseLoop.h"
#include "nsCEvent.h"
#include "nsCEventFilter.h"

//*****************************************************************************
//***    nsCBaseLoop: Object Management
//*****************************************************************************

nsCBaseLoop::nsCBaseLoop(nsEventLoopType type) : m_Type(type)
{
	NS_INIT_REFCNT();
}

nsCBaseLoop::~nsCBaseLoop()
{
}

//*****************************************************************************
// nsCBaseLoop::nsISupports
//*****************************************************************************   

NS_IMPL_ISUPPORTS3(nsCBaseLoop, nsIEventLoop, nsPIEventLoop, 
	nsISupportsWeakReference)

//*****************************************************************************
// nsCBaseLoop::nsIEventLoop
//*****************************************************************************   

NS_IMETHODIMP nsCBaseLoop::Run(nsIEventFilter* filter, 
	nsITranslateListener* translateListener, 
	nsIDispatchListener* dispatchListener, PRInt32* retCode)
{
	nsCOMPtr<nsIEvent> event;
	CreateNewEvent(getter_AddRefs(event));
	nsresult rv;

	nsCOMPtr<nsIEventFilter> localFilter(dont_QueryInterface(filter));
	if(!localFilter)
		NS_ENSURE_SUCCESS(CreateNewEventFilter(getter_AddRefs(localFilter)), 
			NS_ERROR_FAILURE);

	if(translateListener)
		{
		if(dispatchListener)
			rv = RunWithTranslateAndDispatchListener(event, localFilter, 
				translateListener, dispatchListener);
		else
			rv = RunWithTranslateListener(event, localFilter, translateListener);
		}
	else if(dispatchListener)
		rv = RunWithDispatchListener(event, localFilter, dispatchListener);
	else
		rv = RunWithNoListener(event, localFilter);

	if(NS_FAILED(rv))
		{
		if(retCode)
			*retCode = PlatformGetReturnCode(nsnull);
		return NS_ERROR_FAILURE;
		}
	if(retCode)	
		*retCode = PlatformGetReturnCode(event);
	return NS_OK;
}

NS_IMETHODIMP nsCBaseLoop::Exit(PRInt32 exitCode)
{
	return PlatformExit(exitCode);
}

NS_IMETHODIMP nsCBaseLoop::CloneEvent(nsIEvent* event, nsIEvent** newEvent)
{
	NS_ENSURE_ARG(event);
	NS_ENSURE_ARG_POINTER(newEvent);

	void* platformEventData = nsnull;
	if(event)
		{
		platformEventData = GetPlatformEventData(event);
		NS_ENSURE(platformEventData, NS_ERROR_FAILURE);
		}

	nsCEvent* cEvent = new nsCEvent(platformEventData);
	if(!cEvent)
		return NS_ERROR_OUT_OF_MEMORY;

	cEvent->AddRef();
	nsresult rv = cEvent->QueryInterface(NS_GET_IID(nsIEvent), (void**)newEvent);
	cEvent->Release();

	NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

	return NS_OK;
}

NS_IMETHODIMP nsCBaseLoop::CreateNewEvent(nsIEvent** newEvent)
{
	NS_ENSURE_ARG_POINTER(newEvent);

	nsCEvent* event = new nsCEvent();
	NS_ENSURE(event, NS_ERROR_OUT_OF_MEMORY);

	event->AddRef();
	nsresult rv = event->QueryInterface(NS_GET_IID(nsIEvent), (void**)newEvent);
	event->Release();

	NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

	return NS_OK;
}

NS_IMETHODIMP nsCBaseLoop::CreateNewEventFilter(nsIEventFilter** newFilter)
{
	NS_ENSURE_ARG_POINTER(newFilter);

	nsCEventFilter* filter = new nsCEventFilter();
	NS_ENSURE(filter, NS_ERROR_OUT_OF_MEMORY);

	filter->AddRef();
	nsresult rv = filter->QueryInterface(NS_GET_IID(nsIEventFilter), (void**)newFilter);
	filter->Release();

	NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

	return NS_OK;
}

NS_IMETHODIMP nsCBaseLoop::GetNextEvent(nsIEventFilter* filter, 
	nsIEvent* event)
{
	NS_ENSURE_ARG(event);
	nsCOMPtr<nsIEventFilter> localFilter(dont_QueryInterface(filter));
	if(!localFilter)
		NS_ENSURE_SUCCESS(CreateNewEventFilter(getter_AddRefs(localFilter)), 
			NS_ERROR_FAILURE);
	
	void* platformEventData = GetPlatformEventData(event);
	void* platformFilterData = GetPlatformFilterData(localFilter);
	NS_ENSURE(platformEventData, NS_ERROR_FAILURE);

	return PlatformGetNextEvent(platformFilterData, platformEventData);
}

NS_IMETHODIMP nsCBaseLoop::PeekNextEvent(nsIEventFilter* filter, 
	nsIEvent* event, PRBool fRemoveEvent)
{
	NS_ENSURE_ARG(event);
	nsCOMPtr<nsIEventFilter> localFilter(dont_QueryInterface(filter));
	if(!localFilter)
		NS_ENSURE_SUCCESS(CreateNewEventFilter(getter_AddRefs(localFilter)), 
			NS_ERROR_FAILURE);

	void* platformEventData = GetPlatformEventData(event);
	void* platformFilterData = GetPlatformFilterData(localFilter);
	NS_ENSURE(platformEventData, NS_ERROR_FAILURE);

	return PlatformPeekNextEvent(platformFilterData, platformEventData, fRemoveEvent);
}

NS_IMETHODIMP nsCBaseLoop::TranslateEvent(nsIEvent* event)
{
	NS_ENSURE_ARG(event);

	void* platformEventData = GetPlatformEventData(event);
	NS_ENSURE(platformEventData, NS_ERROR_FAILURE);

	return PlatformTranslateEvent(platformEventData);
}

NS_IMETHODIMP nsCBaseLoop::DispatchEvent(nsIEvent* event)
{
	NS_ENSURE_ARG(event);

	void* platformEventData = GetPlatformEventData(event);
	NS_ENSURE(platformEventData, NS_ERROR_FAILURE);

	return PlatformDispatchEvent(platformEventData);
}

NS_IMETHODIMP nsCBaseLoop::SendLoopEvent(nsIEvent* event, PRInt32* result)
{
	NS_ENSURE_ARG(event);

	void* platformEventData = GetPlatformEventData(event);
	NS_ENSURE(platformEventData, NS_ERROR_FAILURE);

	return PlatformSendLoopEvent(platformEventData, result);
}

NS_IMETHODIMP nsCBaseLoop::PostLoopEvent(nsIEvent* event)
{
	NS_ENSURE_ARG(event);

	void* platformEventData = GetPlatformEventData(event);
	NS_ENSURE(platformEventData, NS_ERROR_FAILURE);

	return PlatformPostLoopEvent(platformEventData);
}

NS_IMETHODIMP nsCBaseLoop::GetEventLoopType(nsEventLoopType* pType)
{
	NS_ENSURE_ARG_POINTER(pType);
	*pType = m_Type;
	return NS_OK;
}

NS_IMETHODIMP nsCBaseLoop::GetEventLoopName(PRUnichar** pName)
{
	NS_ENSURE_ARG_POINTER(pName);
	
	*pName = m_LoopName.ToNewUnicode();

	return *pName ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

//*****************************************************************************
// nsCBaseLoop::nsPIEventLoop
//*****************************************************************************   

NS_IMETHODIMP nsCBaseLoop::LoopInit(const PRUnichar* pLoopName)
{
	m_LoopName.Assign(pLoopName);
	return NS_OK;
}

//*****************************************************************************
// nsCBaseLoop:: Internal Helpers
//*****************************************************************************   

void* nsCBaseLoop::GetPlatformEventData(nsIEvent* event)
{
	void* platformEventData = nsnull;
	nsNativeEventDataType platformEventType = PlatformGetEventType();

	event->GetNativeData(platformEventType, &platformEventData);

	return platformEventData;
}

void* nsCBaseLoop::GetPlatformFilterData(nsIEventFilter* filter)
{
	void* platformFilterData = nsnull;
	nsNativeFilterDataType platformFilterType = PlatformGetFilterType();

	filter->GetNativeData(platformFilterType, &platformFilterData);

	return platformFilterData;
}