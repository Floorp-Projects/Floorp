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
#include "nsCPhEvent.h"
#include "nsCPhFilter.h"

#include "nsPhEventLog.h"
#include <Pt.h>

//*****************************************************************************
//***    nsCPlatformBaseLoop: Object Management
//*****************************************************************************

nsCPlatformBaseLoop::nsCPlatformBaseLoop(nsEventLoopType type) : 
	nsCBaseLoop(type)
{
	m_PhThreadId = pthread_self();

  PR_LOG(PhEventLog, PR_LOG_DEBUG, ("nsCPlatformBaseLoop::nsCPlatformBaseLoop nsEventLoopType=<%d> m_PhThreadId=<%d>\n", type, m_PhThreadId)); 
}

nsCPlatformBaseLoop::~nsCPlatformBaseLoop()
{
  PR_LOG(PhEventLog, PR_LOG_DEBUG, ("nsCPlatformBaseLoop::~nsCPlatformBaseLoop m_PhThreadId=<%d>\n", m_PhThreadId)); 
}

//*****************************************************************************
// nsCPlatformBaseLoop:: Internal Platform Implementations of nsIEventLoop 
//							(Error checking is ensured above)
//*****************************************************************************   

nsresult nsCPlatformBaseLoop::PlatformGetNextEvent(void* platformFilterData, 
	void* platformEventData)
{
  PR_LOG(PhEventLog, PR_LOG_DEBUG, ("nsCPlatformBaseLoop::PlatformGetNextEvent platformFilterData=<%p>  platformEventData=<%p>\n", platformFilterData, platformEventData)); 

	nsCPhFilter* filter=(nsCPhFilter*)platformFilterData;
	nsCPhEvent* pEvent = (nsCPhEvent*) platformEventData;
    PRBool done = PR_FALSE;

	if(filter != NULL)
    {
      PR_LOG(PhEventLog, PR_LOG_DEBUG, ("nsCPlatformBaseLoop::PlatformGetNextEvent Filters not supported\n"));

	  return NS_COMFALSE;
	}
	  	
    while(!done)
	{
      switch( PhEventNext(pEvent->m_msg, pEvent->GetEventBufferSize()))
  	  {
 	  case Ph_EVENT_MSG:
        PR_LOG(PhEventLog, PR_LOG_DEBUG, ("nsCPlatformBaseLoop::PlatformGetNextEvent GetMessage\n"));
		done = PR_TRUE;
		break;
      case Ph_RESIZE_MSG:
        PR_LOG(PhEventLog, PR_LOG_DEBUG, ("nsCPlatformBaseLoop::PlatformGetNextEvent Resize Message from <%d> to <%d>\n", pEvent->GetEventBufferSize(), pEvent->GetEventSize()));
		pEvent->ResizeEvent(pEvent->GetEventSize());
		break;
	  }
	}
	
	return  NS_OK; 
}

nsresult nsCPlatformBaseLoop::PlatformPeekNextEvent(void* platformFilterData, 
	void* platformEventData, PRBool fRemoveEvent)
{
  PR_LOG(PhEventLog, PR_LOG_DEBUG, ("nsCPlatformBaseLoop::PlatformPeekNextEvent platformFilterData=<%p>  platformEventData=<%p> fRemoveEvent=<%d>\n", platformFilterData, platformEventData, fRemoveEvent)); 

	if(fRemoveEvent == PR_FALSE)
    {
      PR_LOG(PhEventLog, PR_LOG_DEBUG, ("nsCPlatformBaseLoop::PlatformPeekNextEvent Leaving the element on the queue is not supported\n"));
	  return NS_COMFALSE;
	}
		  
#if 0
	nsCWinFilter* filter=(nsCWinFilter*)platformFilterData;
	MSG* pMsg=(MSG*)platformEventData;
	
	if(fRemoveEvent)
		filter->wRemoveFlags|= PM_REMOVE;
	else
		filter->wRemoveFlags&= ~PM_REMOVE;
	if(::PeekMessage(pMsg, filter->hWnd, filter->wMsgFilterMin, 
		filter->wMsgFilterMax, filter->wRemoveFlags))
		return NS_OK;
#endif
	return NS_COMFALSE;
}

nsresult nsCPlatformBaseLoop::PlatformTranslateEvent(void* platformEventData)
{
  PR_LOG(PhEventLog, PR_LOG_DEBUG, ("nsCPlatformBaseLoop::PlatformTranslateEvent platformEventData=<%p>\n", platformEventData)); 

#if 0
	MSG* pMsg=(MSG*)platformEventData;
	::TranslateMessage(pMsg);
#endif

	return NS_OK;
}

nsresult nsCPlatformBaseLoop::PlatformDispatchEvent(void* platformEventData)
{
  PR_LOG(PhEventLog, PR_LOG_DEBUG, ("nsCPlatformBaseLoop::PlatformDispatchEvent platformEventData=<%p>\n", platformEventData)); 

#if 0
	MSG* pMsg=(MSG*)platformEventData;
	::DispatchMessage(pMsg);
#endif

	return NS_OK;
}

nsresult nsCPlatformBaseLoop::PlatformSendLoopEvent(void* platformEventData, PRInt32* result)
{
  PR_LOG(PhEventLog, PR_LOG_DEBUG, ("nsCPlatformBaseLoop::PlatformSendLoopEvent platformEventData=<%p>\n", platformEventData)); 

#if 0
	MSG* pMsg=(MSG*)platformEventData;
	*result = ::SendMessage(pMsg->hwnd, pMsg->message, pMsg->wParam,pMsg->lParam);
#endif

	return NS_OK;
}

nsresult nsCPlatformBaseLoop::PlatformPostLoopEvent(void* platformEventData)
{
  PR_LOG(PhEventLog, PR_LOG_DEBUG, ("nsCPlatformBaseLoop::PlatformPostLoopEvent platformEventData=<%p>\n", platformEventData)); 

#if 0
	MSG* pMsg=(MSG*)platformEventData;
	if(!pMsg->hwnd)
		{
		if(!::PostThreadMessage(m_WinThreadId, pMsg->message, pMsg->wParam,
			pMsg->lParam))
			return NS_ERROR_FAILURE;
		}
	else if(!::PostMessage(pMsg->hwnd, pMsg->message, pMsg->wParam, pMsg->lParam))
		return NS_ERROR_FAILURE;
#endif

	return NS_OK;
}

nsNativeEventDataType nsCPlatformBaseLoop::PlatformGetEventType()
{
	return nsNativeEventDataTypes::PhotonMsgStruct;
}

nsNativeEventDataType nsCPlatformBaseLoop::PlatformGetFilterType()
{
	return nsNativeFilterDataTypes::PhotonFilter;
}

PRInt32 nsCPlatformBaseLoop::PlatformGetReturnCode(void* platformEventData)
{
  PR_LOG(PhEventLog, PR_LOG_DEBUG, ("nsCPlatformBaseLoop::PlatformGetReturnCode platformEventData=<%p>\n", platformEventData)); 

#if 0
	MSG* pMsg=(MSG*)platformEventData;
	return pMsg->wParam;
#endif

    return -1;
}