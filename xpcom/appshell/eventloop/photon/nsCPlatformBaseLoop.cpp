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
 *   Jerry Kirk    <Jerry.Kirk@NexwareCorp.com>
 */

#include "nsCPlatformBaseLoop.h"
#include "nsCPhFilter.h"

//*****************************************************************************
//***    nsCPlatformBaseLoop: Object Management
//*****************************************************************************

nsCPlatformBaseLoop::nsCPlatformBaseLoop(nsEventLoopType type) : 
	nsCBaseLoop(type)
{
	m_PhThreadId = pthread_self();
}

nsCPlatformBaseLoop::~nsCPlatformBaseLoop()
{
}

//*****************************************************************************
// nsCPlatformBaseLoop:: Internal Platform Implementations of nsIEventLoop 
//							(Error checking is ensured above)
//*****************************************************************************   

nsresult nsCPlatformBaseLoop::PlatformGetNextEvent(void* platformFilterData, 
	void* platformEventData)
{
	nsCPhFilter* filter=(nsCPhFilter*)platformFilterData;
	PhEvent_t* pEvent = (PhEvent_t*) platformEventData;
	if(::GetMessage(pMsg, filter->hWnd, filter->wMsgFilterMin, 
		filter->wMsgFilterMax))
		return NS_OK;
	return  NS_COMFALSE; 
}

nsresult nsCPlatformBaseLoop::PlatformPeekNextEvent(void* platformFilterData, 
	void* platformEventData, PRBool fRemoveEvent)
{
	nsCWinFilter* filter=(nsCWinFilter*)platformFilterData;
	MSG* pMsg=(MSG*)platformEventData;
	
	if(fRemoveEvent)
		filter->wRemoveFlags|= PM_REMOVE;
	else
		filter->wRemoveFlags&= ~PM_REMOVE;
	if(::PeekMessage(pMsg, filter->hWnd, filter->wMsgFilterMin, 
		filter->wMsgFilterMax, filter->wRemoveFlags))
		return NS_OK;
	return NS_COMFALSE;
}

nsresult nsCPlatformBaseLoop::PlatformTranslateEvent(void* platformEventData)
{
	MSG* pMsg=(MSG*)platformEventData;
	::TranslateMessage(pMsg);
	return NS_OK;
}

nsresult nsCPlatformBaseLoop::PlatformDispatchEvent(void* platformEventData)
{
	MSG* pMsg=(MSG*)platformEventData;
	::DispatchMessage(pMsg);
	return NS_OK;
}

nsresult nsCPlatformBaseLoop::PlatformSendLoopEvent(void* platformEventData, PRInt32* result)
{
	MSG* pMsg=(MSG*)platformEventData;
	*result = ::SendMessage(pMsg->hwnd, pMsg->message, pMsg->wParam,pMsg->lParam);
	return NS_OK;
}

nsresult nsCPlatformBaseLoop::PlatformPostLoopEvent(void* platformEventData)
{
	MSG* pMsg=(MSG*)platformEventData;
	if(!pMsg->hwnd)
		{
		if(!::PostThreadMessage(m_WinThreadId, pMsg->message, pMsg->wParam,
			pMsg->lParam))
			return NS_ERROR_FAILURE;
		}
	else if(!::PostMessage(pMsg->hwnd, pMsg->message, pMsg->wParam, pMsg->lParam))
		return NS_ERROR_FAILURE;
	return NS_OK;
}

nsNativeEventDataType nsCPlatformBaseLoop::PlatformGetEventType()
{
	return nsNativeEventDataTypes::WinMsgStruct;
}

nsNativeEventDataType nsCPlatformBaseLoop::PlatformGetFilterType()
{
	return nsNativeFilterDataTypes::WinFilter;
}

PRInt32 nsCPlatformBaseLoop::PlatformGetReturnCode(void* platformEventData)
{
	MSG* pMsg=(MSG*)platformEventData;
	return pMsg->wParam;
}