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

#include "nsCRT.h"

#include "nsCEvent.h"

//*****************************************************************************
//***    nsCEvent: Object Management
//*****************************************************************************

nsCEvent::nsCEvent(void* platformEventData)
{
	NS_INIT_REFCNT();

	if (platformEventData)
	{
		mEventBufferSz = PhGetMsgSize ( (PhEvent_t *) platformEventData );
		m_msg = (PhEvent_t *) malloc( mEventBufferSz );
		if (m_msg);
		  nsCRT::memcpy(m_msg, platformEventData, mEventBufferSz);
	}
	else
	{
		mEventBufferSz = sizeof(PhEvent_t);
		m_msg = (PhEvent_t *) malloc( mEventBufferSz );
		if (m_msg);
		  nsCRT::memset(m_msg, 0, mEventBufferSz);
	}
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
	NS_ENSURE_ARG(nsNativeEventDataTypes::PhotonMsgStruct == dataType);
	NS_ENSURE_ARG_POINTER(data);

	*data = m_msg;

	return NS_OK;
}

NS_IMETHODIMP nsCEvent::SetNativeData(nsNativeEventDataType dataType,
	void* data)
{
	NS_ENSURE_ARG(nsNativeEventDataTypes::PhotonMsgStruct == dataType);

	if(!data)
	{
		/* Get rid of the Event */
		nsCRT::memset(m_msg, 0, mEventBufferSz);
	}
	else
	{
		unsigned long locEventBufferSz = PhGetMsgSize ( (PhEvent_t *) data );
		if (locEventBufferSz > mEventBufferSz)
		{
		  mEventBufferSz = locEventBufferSz;
		  m_msg = (PhEvent_t *) realloc( mEventBufferSz, mEventBufferSz );
		  NS_ENSURE_ARG_POINTER(m_msg);
		}
		nsCRT::memcpy(m_msg, data, mEventBufferSz);
	}

	return NS_OK;
}