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

#include "nsCThreadLoop.h"
#include "nsCWinFilter.h"

//*****************************************************************************
//***    nsCThreadLoop: Object Management
//*****************************************************************************

nsCThreadLoop::nsCThreadLoop() : nsCBaseThreadLoop()
{
	m_WinThreadId = ::GetCurrentThreadId();
}

nsCThreadLoop::~nsCThreadLoop()
{
}

NS_METHOD nsCThreadLoop::Create(nsISupports* aOuter, const nsIID& aIID, 
	void** ppv)
{
	NS_ENSURE_ARG_POINTER(ppv);
	NS_ENSURE_NO_AGGREGATION(aOuter);

	nsCThreadLoop* app = new nsCThreadLoop();
	NS_ENSURE(app, NS_ERROR_OUT_OF_MEMORY);

	NS_ADDREF(app);
	nsresult rv = app->QueryInterface(aIID, ppv);
	NS_RELEASE(app);
	return rv;
}

//*****************************************************************************
// nsCThreadLoop:: Internal Platform Implementations of nsIEventLoop 
//							(Error checking is ensured above)
//*****************************************************************************   

nsresult nsCThreadLoop::PlatformExit(PRInt32 exitCode)
{
	::PostThreadMessage(m_WinThreadId, WM_QUIT, 0, 0);
	return NS_OK;
}

nsresult nsCThreadLoop::PlatformGetNextEvent(void* platformFilterData, 
	void* platformEventData)
{
	nsCWinFilter* filter=(nsCWinFilter*)platformFilterData;
	MSG* pMsg=(MSG*)platformEventData;
	if(::GetMessage(pMsg, filter->hWnd, filter->wMsgFilterMin, 
		filter->wMsgFilterMax))
		return NS_OK;
	return  NS_COMFALSE; 
}

nsresult nsCThreadLoop::PlatformPeekNextEvent(void* platformFilterData, 
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

nsresult nsCThreadLoop::PlatformTranslateEvent(void* platformEventData)
{
	MSG* pMsg=(MSG*)platformEventData;
	::TranslateMessage(pMsg);
	return NS_OK;
}

nsresult nsCThreadLoop::PlatformDispatchEvent(void* platformEventData)
{
	MSG* pMsg=(MSG*)platformEventData;
	::DispatchMessage(pMsg);
	return NS_OK;
}

nsresult nsCThreadLoop::PlatformSendLoopEvent(void* platformEventData, PRInt32* result)
{
	MSG* pMsg=(MSG*)platformEventData;
	*result = ::SendMessage(pMsg->hwnd, pMsg->message, pMsg->wParam,pMsg->lParam);
	return NS_OK;
}

nsresult nsCThreadLoop::PlatformPostLoopEvent(void* platformEventData)
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

nsNativeEventDataType nsCThreadLoop::PlatformGetEventType()
{
	return nsNativeEventDataTypes::WinMsgStruct;
}

nsNativeEventDataType nsCThreadLoop::PlatformGetFilterType()
{
	return nsNativeFilterDataTypes::WinFilter;
}

PRInt32 nsCThreadLoop::PlatformGetReturnCode(void* platformEventData)
{
	MSG* pMsg=(MSG*)platformEventData;
	return pMsg->wParam;
}


