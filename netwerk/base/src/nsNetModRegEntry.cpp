/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsNetModRegEntry.h"
#include "plstr.h"
#include "nsIAllocator.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsProxyObjectManager.h"


static NS_DEFINE_IID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

    
//////////////////////////////
//// nsISupports
//////////////////////////////
NS_IMPL_ISUPPORTS(nsNetModRegEntry, nsCOMTypeInfo<nsINetModRegEntry>::GetIID());


//////////////////////////////
//// nsINetModRegEntry
//////////////////////////////

NS_IMETHODIMP
nsNetModRegEntry::GetSyncProxy(nsINetNotify **aNotify) {
    *aNotify = mSyncProxy;
    NS_ADDREF(*aNotify);
    return NS_OK;
}


NS_IMETHODIMP
nsNetModRegEntry::GetAsyncProxy(nsINetNotify **aNotify) {
    *aNotify = mAsyncProxy;
    NS_ADDREF(*aNotify);
    return NS_OK;
}

NS_IMETHODIMP
nsNetModRegEntry::GetTopic(char **topic) 
{
    if (mTopic) 
	{
		*topic = (char *) nsAllocator::Clone(mTopic, nsCRT::strlen(mTopic) + 1);
		return NS_OK;
	}
    return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsNetModRegEntry::Equals(nsINetModRegEntry* aEntry, PRBool *_retVal) 
{
    nsresult rv = NS_OK;
    *_retVal = PR_FALSE;

    NS_ADDREF(aEntry);

    char* topic;

    rv = aEntry->GetTopic(&topic);
    if (NS_FAILED(rv)) 
        return rv;
     
    if (topic) {
        nsAllocator::Free(topic);
	topic=0;
	}
    
    if (PL_strcmp(topic, mTopic)) 
        return NS_OK;

    nsCOMPtr<nsIEventQueue> entryEventQ;
    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, &rv); 
    
    if (NS_FAILED(rv)) 
        return rv;

    rv = eventQService->GetThreadEventQueue(PR_CurrentThread(), getter_AddRefs(entryEventQ)); 
     
    if (NS_FAILED(rv) || mEventQ != entryEventQ)
    {
        return rv;
    }

    *_retVal = PR_TRUE;
    return rv;
}


//////////////////////////////
//// nsNetModRegEntry
//////////////////////////////

nsNetModRegEntry::nsNetModRegEntry(const char *aTopic, 
                                   nsINetNotify *aNotify, 
                                   nsresult *result)
{
    NS_INIT_REFCNT();
    mTopic = new char [PL_strlen(aTopic) + 1];
    PL_strcpy(mTopic, aTopic);
   
    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, result); 
    
    if (NS_FAILED(*result)) return;
    
    *result = eventQService->GetThreadEventQueue(PR_CurrentThread(), getter_AddRefs(mEventQ)); 
     
    if (NS_FAILED(*result)) return;

    NS_WITH_SERVICE( nsIProxyObjectManager, proxyManager, kProxyObjectManagerCID, result);
    
    if (NS_FAILED(*result)) return;

    *result = proxyManager->GetProxyObject(  mEventQ,
                                             nsCOMTypeInfo<nsINetNotify>::GetIID(),
                                             aNotify,
                                             PROXY_SYNC | PROXY_ALWAYS,
                                             getter_AddRefs(mSyncProxy));
    if (NS_FAILED(*result)) return;
    
    *result = proxyManager->GetProxyObject(  mEventQ,
                                             nsCOMTypeInfo<nsINetNotify>::GetIID(),
                                             aNotify,
                                             PROXY_ASYNC | PROXY_ALWAYS,
                                             getter_AddRefs(mAsyncProxy));
}

nsNetModRegEntry::~nsNetModRegEntry() 
{
    delete [] mTopic;
}
