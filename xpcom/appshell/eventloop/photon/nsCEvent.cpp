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

#include <prlog.h>
PRLogModuleInfo  *PhEventLog =  PR_NewLogModule("PhEventLog");

#include "nsPhEventLog.h"

//*****************************************************************************
//***    nsCEvent: Object Management
//*****************************************************************************

nsCEvent::nsCEvent(void* platformEventData)
{
  PR_LOG(PhEventLog, PR_LOG_DEBUG, ("nsCEvent::nsCEvent platformEventData=<%p>\n", platformEventData));
  
	NS_INIT_REFCNT();
	m_msg = new nsCPhEvent( (PhEvent_t *) platformEventData);
}

nsCEvent::~nsCEvent()
{
  PR_LOG(PhEventLog, PR_LOG_DEBUG, ("nsCEvent::~nsCEvent\n"));

  if (m_msg)
  {
	delete m_msg;
  }	
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
  PR_LOG(PhEventLog, PR_LOG_DEBUG, ("nsCEvent::GetNativeData dataType=<%d> data=<%p>\n", dataType, data));


	NS_ENSURE_ARG(nsNativeEventDataTypes::PhotonMsgStruct == dataType);
	NS_ENSURE_ARG_POINTER(data);

	*data = m_msg;
	return NS_OK;
}

NS_IMETHODIMP nsCEvent::SetNativeData(nsNativeEventDataType dataType,
	void* data)
{
  PR_LOG(PhEventLog, PR_LOG_DEBUG, ("nsCEvent::SetNativeData dataType=<%d>\n", dataType));

	NS_ENSURE_ARG(nsNativeEventDataTypes::PhotonMsgStruct == dataType);

	if(!data)
	{
		/* Get rid of the Event */
		if (m_msg)
		  delete m_msg;
	     m_msg = new nsCPhEvent(NULL);
	}
	else
	{
		/* Get rid of the Event */
		if (m_msg)
		  delete m_msg;

		nsCPhEvent *aEvent = (nsCPhEvent *) data;
        m_msg = new nsCPhEvent(aEvent->m_msg);
 	}

	return NS_OK;
}
	