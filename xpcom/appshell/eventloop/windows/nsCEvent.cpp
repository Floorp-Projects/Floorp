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

#include "nsCRT.h"

#include "nsCEvent.h"

//*****************************************************************************
//***    nsCEvent: Object Management
//*****************************************************************************

nsCEvent::nsCEvent(void* platformEventData)
{
	NS_INIT_REFCNT();

	if(platformEventData)
		nsCRT::memcpy(&m_msg, platformEventData, sizeof(m_msg));
	else
		nsCRT::memset(&m_msg, 0, sizeof(m_msg));
}

nsCEvent::~nsCEvent()
{
}

//*****************************************************************************
// nsCEvent::nsISupports
//*****************************************************************************   

NS_IMPL_ISUPPORTS2(nsCEvent, nsIEvent, nsIWinEvent)

//*****************************************************************************
// nsCEvent::nsIEvent
//*****************************************************************************   

NS_IMETHODIMP nsCEvent::GetNativeData(nsNativeEventDataType dataType, 
	void** data)
{
	NS_ENSURE_ARG(nsNativeEventDataTypes::WinMsgStruct == dataType);
	NS_ENSURE_ARG_POINTER(data);

	*data = &m_msg;

	return NS_OK;
}

NS_IMETHODIMP nsCEvent::SetNativeData(nsNativeEventDataType dataType,
	void* data)
{
	NS_ENSURE_ARG(nsNativeEventDataTypes::WinMsgStruct == dataType);

	if(!data)
		nsCRT::memset(&m_msg, 0, sizeof(m_msg));
	else
		nsCRT::memcpy(&m_msg, data, sizeof(m_msg));

	return NS_OK;
}
//*****************************************************************************
// nsCEvent::nsIWinEvent
//*****************************************************************************   

NS_IMETHODIMP nsCEvent::GetHwnd(void** aHwnd)
{
	NS_ENSURE_ARG_POINTER(aHwnd);
	*aHwnd = m_msg.hwnd;
	return NS_OK;
}

NS_IMETHODIMP nsCEvent::SetHwnd(void* aHwnd)
{
	m_msg.hwnd = (HWND)aHwnd;
	return NS_OK;
}

NS_IMETHODIMP nsCEvent::GetMessage(PRUint32*	aMessage)
{
	NS_ENSURE_ARG_POINTER(aMessage);
	*aMessage = m_msg.message;
	return NS_OK;
}

NS_IMETHODIMP nsCEvent::SetMessage(PRUint32 aMessage)
{
	m_msg.message = aMessage;
	return NS_OK;
}

NS_IMETHODIMP nsCEvent::GetWParam(PRUint32* aWParam)
{
	NS_ENSURE_ARG_POINTER(aWParam);
	*aWParam = m_msg.wParam;
	return NS_OK;
}

NS_IMETHODIMP nsCEvent::SetWParam(PRUint32 aWParam)
{
	m_msg.wParam = aWParam;
	return NS_OK;
}

NS_IMETHODIMP nsCEvent::GetLParam(PRUint32* aLParam)
{
	NS_ENSURE_ARG_POINTER(aLParam);
	*aLParam = m_msg.lParam;
	return NS_OK;
}

NS_IMETHODIMP nsCEvent::SetLParam(PRUint32 aLParam)
{
	m_msg.lParam = aLParam;
	return NS_OK;
}

NS_IMETHODIMP nsCEvent::GetTime(PRUint32* aTime)
{
	NS_ENSURE_ARG_POINTER(aTime);
	*aTime = m_msg.time;
	return NS_OK;
}

NS_IMETHODIMP nsCEvent::SetTime(PRUint32 aTime)
{
	m_msg.time = aTime;
	return NS_OK;
}

NS_IMETHODIMP nsCEvent::GetPointX(PRInt32* aPointX)
{
	NS_ENSURE_ARG_POINTER(aPointX);
	*aPointX = m_msg.pt.x;
	return NS_OK;
}

NS_IMETHODIMP nsCEvent::SetPointX(PRInt32 aPointX)
{
	m_msg.pt.x = aPointX;
	return NS_OK;
}

NS_IMETHODIMP nsCEvent::GetPointY(PRInt32* aPointY)
{
	NS_ENSURE_ARG_POINTER(aPointY);
	*aPointY = m_msg.pt.y;
	return NS_OK;
}

NS_IMETHODIMP nsCEvent::SetPointY(PRInt32 aPointY)
{
	m_msg.pt.y = aPointY;
	return NS_OK;
}