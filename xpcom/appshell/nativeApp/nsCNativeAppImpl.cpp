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

#include "nsIComponentManager.h"
#include "nsString.h"

#include "nsCNativeAppImpl.h"
#include "nsPIEventLoop.h"

//*****************************************************************************
//***    nsCNativeAppImpl: Object Management
//*****************************************************************************

nsCNativeAppImpl::nsCNativeAppImpl()
{
	NS_INIT_REFCNT();
}

nsCNativeAppImpl::~nsCNativeAppImpl()
{
}

NS_IMETHODIMP nsCNativeAppImpl::Create(nsISupports* aOuter, const nsIID& aIID, 
	void** ppv)
{
	NS_ENSURE_ARG_POINTER(ppv);
	NS_ENSURE_NO_AGGREGATION(aOuter);

	nsCNativeAppImpl* app = new nsCNativeAppImpl();
	NS_ENSURE_TRUE(app, NS_ERROR_OUT_OF_MEMORY);

	NS_ADDREF(app);
	nsresult rv = app->QueryInterface(aIID, ppv);
	NS_RELEASE(app);
	return rv;
}

//*****************************************************************************
// nsCNativeAppImpl::nsISupports
//*****************************************************************************   

NS_IMPL_ISUPPORTS1(nsCNativeAppImpl, nsINativeApp)

//*****************************************************************************
// nsCNativeAppImpl::nsINativeApp
//*****************************************************************************   

NS_IMETHODIMP nsCNativeAppImpl::CreateEventLoop(const PRUnichar* EventLoopName,
	nsEventLoopType type, nsIEventLoop** eventLoop)
{
	NS_ENSURE_ARG_POINTER(eventLoop);
			  
	char* contractID = nsnull;

	switch(type)
		{
		case nsEventLoopTypes::MainAppLoop:
			NS_ENSURE_FALSE(GetLoop(type), NS_ERROR_UNEXPECTED);
			contractID = NS_EVENTLOOP_APP_CONTRACTID;
			break;

		case nsEventLoopTypes::ThreadLoop:
			NS_ENSURE_FALSE(GetLoop(PR_CurrentThread()), NS_ERROR_UNEXPECTED);
			contractID = NS_EVENTLOOP_THREAD_CONTRACTID;
			break;

		case nsEventLoopTypes::AppBreathLoop:
			contractID = NS_EVENTLOOP_BREATH_CONTRACTID;
			break;

		default:
			NS_ERROR("Need to update switch");
			return NS_ERROR_FAILURE;
		}

	nsCOMPtr<nsIEventLoop> loop;

	nsresult rv = nsComponentManager::CreateInstance(contractID, nsnull, 
		NS_GET_IID(nsIEventLoop), getter_AddRefs(loop));

	NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

	nsCOMPtr<nsPIEventLoop> pvtLoop(do_QueryInterface(loop));
	if(NS_FAILED(pvtLoop->LoopInit(EventLoopName)))
		return NS_ERROR_FAILURE;

	NS_ENSURE_TRUE(AddLoop(EventLoopName, PR_CurrentThread(), loop, type), 
		NS_ERROR_FAILURE);

	*eventLoop = loop;
	NS_ADDREF(*eventLoop);

	return NS_OK;
} 

NS_IMETHODIMP nsCNativeAppImpl::FindEventLoop(const PRUnichar* EventLoopName,
	nsIEventLoop** eventLoop)
{
	NS_ENSURE_ARG_POINTER(eventLoop);
	*eventLoop = nsnull;
	nsCLoopInfo* loopInfo;

	while(PR_TRUE)	// Do this in case GetEventLoop fails.  Provides list cleanup
		{
		loopInfo = GetLoop(EventLoopName);
		NS_ENSURE_TRUE(loopInfo, NS_ERROR_INVALID_ARG);

		loopInfo->GetEventLoop(eventLoop);
		if(!eventLoop)
			RemoveLoop(loopInfo);
		else
			break;
		}

	NS_ENSURE_TRUE(*eventLoop, NS_ERROR_INVALID_ARG);
	return NS_OK;
}

//*****************************************************************************
// nsCNativeAppImpl:: EventLoop List management
//*****************************************************************************   

nsCLoopInfo* nsCNativeAppImpl::AddLoop(const PRUnichar* loopName, PRThread* thread,
	nsIEventLoop* eventLoop, nsEventLoopType type)
{
	nsCLoopInfo* loopInfo = new nsCLoopInfo(loopName, thread, eventLoop, type);

	if(loopInfo)
		m_EventLoopList.AppendElement(loopInfo);

	return loopInfo;
}

void nsCNativeAppImpl::RemoveLoop(nsCLoopInfo* loopInfo)
{
	m_EventLoopList.RemoveElement(loopInfo);
	delete loopInfo;
}

PRBool nsCNativeAppImpl::VerifyLoop(nsCLoopInfo* loopInfo)
{
	nsCOMPtr<nsIEventLoop> eventLoop;
	loopInfo->GetEventLoop(getter_AddRefs(eventLoop));
	return eventLoop ? PR_TRUE : PR_FALSE;	
}

nsCLoopInfo* nsCNativeAppImpl::GetLoop(PRThread* thread)
{
	PRInt32 count = m_EventLoopList.Count();
	PRInt32 x;
	nsCLoopInfo* loopInfo;

	for(x = 0; x < count; x++)
		{
		loopInfo = (nsCLoopInfo*)m_EventLoopList[x];
		if(loopInfo->Thread() == thread)
			{
			if(VerifyLoop(loopInfo))
				return loopInfo;
			else
				{
				x--;
				count--;
				RemoveLoop(loopInfo);
				}
			}
		}
	return nsnull;
}

nsCLoopInfo* nsCNativeAppImpl::GetLoop(nsEventLoopType type)
{
	PRInt32 count = m_EventLoopList.Count();
	PRInt32 x;
	nsCLoopInfo* loopInfo;

	for(x = 0; x < count; x++)
		{
		loopInfo = (nsCLoopInfo*)m_EventLoopList[x];
		if(loopInfo->Type() == type)
			{
			if(VerifyLoop(loopInfo))
				return loopInfo;
			else
				{
				x--;
				count--;
				RemoveLoop(loopInfo);
				}
			}
		}
	return nsnull;
}

nsCLoopInfo* nsCNativeAppImpl::GetLoop(const PRUnichar* loopName)
{
	PRInt32 count = m_EventLoopList.Count();
	PRInt32 x;
	nsCLoopInfo* loopInfo;

	for(x = 0; x < count; x++)
		{
		loopInfo = (nsCLoopInfo*)m_EventLoopList[x];
		if(loopInfo->CompareName(loopName))
			{
			if(VerifyLoop(loopInfo))
				return loopInfo;
			else
				{
				x--;
				count--;
				RemoveLoop(loopInfo);
				}
			}
		}
	return nsnull;
}

//*****************************************************************************
//*** nsCLoopInfo:: Object Management
//*****************************************************************************   

nsCLoopInfo::nsCLoopInfo(const PRUnichar* loopName, PRThread* thread, 
	nsIEventLoop* eventLoop, nsEventLoopType type) : m_Thread(thread), 
	m_Type(type)
{
	m_pEventLoop = getter_AddRefs(NS_GetWeakReference(eventLoop));
	m_LoopName.Assign(loopName);
}

//*****************************************************************************
// nsCLoopInfo:: Interaction Functions
//*****************************************************************************   

void nsCLoopInfo::GetEventLoop(nsIEventLoop** eventLoop)
{
	m_pEventLoop->QueryReferent(NS_GET_IID(nsIEventLoop), (void**)eventLoop);
}

PRBool nsCLoopInfo::CompareName(const PRUnichar* loopName)
{
	return (m_LoopName.CompareWithConversion(loopName) == 0);
} 

//*****************************************************************************
// nsCLoopInfo:: Accessor Functions
//*****************************************************************************   

PRThread* nsCLoopInfo::Thread()
{
	return m_Thread;
}	

nsEventLoopType nsCLoopInfo::Type()
{
	return m_Type;
}
