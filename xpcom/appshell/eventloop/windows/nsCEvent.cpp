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

NS_IMPL_ISUPPORTS1(nsCEvent, nsIEvent)

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