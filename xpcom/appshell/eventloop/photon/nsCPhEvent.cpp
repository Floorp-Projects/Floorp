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
 *   Jerry Kirk    <Jerry.Kirk@NexwareCorp.com>
 */

#include "nsCRT.h"
#include "nsCPhEvent.h"

#include "nsPhEventLog.h"

//*****************************************************************************
//***    nsCPhEvent: Object Management
//*****************************************************************************

nsCPhEvent::nsCPhEvent(PhEvent_t* platformEventData)
{
  PR_LOG(PhEventLog, PR_LOG_DEBUG, ("nsCPhEvent::nsCPhEvent platformEventData=<%p>\n", platformEventData));
  
	if (platformEventData)
	{
		mEventBufferSz = PhGetMsgSize ( platformEventData );
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

nsCPhEvent::~nsCPhEvent()
{
  PR_LOG(PhEventLog, PR_LOG_DEBUG, ("nsCPhEvent::~nsCPhEvent\n"));

  if (m_msg)
  {
    free(m_msg);
    m_msg = nsnull;
    mEventBufferSz = 0;
  }	
}

unsigned long nsCPhEvent::GetEventBufferSize()
{
	return mEventBufferSz;
}

unsigned long nsCPhEvent::GetEventSize()
{
  unsigned long theEventSize = 0;
  if (m_msg)
  	theEventSize = PhGetMsgSize ( (PhEvent_t *) m_msg );

  PR_LOG(PhEventLog, PR_LOG_DEBUG, ("nsCPhEvent::GetEventSize theEventSize=<%d>\n", theEventSize));

  return theEventSize;
}

nsresult nsCPhEvent::ResizeEvent(unsigned long aEventSize)
{
  PR_LOG(PhEventLog, PR_LOG_DEBUG, ("nsCPhEvent::ResizeEvent old_size=<%d> new_size=<%d>\n", mEventBufferSz, aEventSize));

  mEventBufferSz = aEventSize;
  m_msg = (PhEvent_t *) realloc( m_msg, mEventBufferSz );
  NS_ENSURE_ARG_POINTER(m_msg);
  return NS_OK;
}
	