/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsNetModRegEntry.h"
#include "nsCRT.h"
#include "plstr.h"
#include "nsAutoLock.h"
#include "nsMemory.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"
#include "nsIProxyObjectManager.h"


static NS_DEFINE_IID(kProxyObjectManagerCID, NS_PROXYEVENT_MANAGER_CID);
static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

    
//////////////////////////////
//// nsISupports
//////////////////////////////
NS_IMPL_THREADSAFE_ISUPPORTS1(nsNetModRegEntry, nsINetModRegEntry);


//////////////////////////////
//// nsINetModRegEntry
//////////////////////////////

NS_IMETHODIMP
nsNetModRegEntry::GetSyncProxy(nsINetNotify **aNotify) 
{
    nsAutoMonitor mon(mMonitor);

    if (mSyncProxy)
    {
        *aNotify = mSyncProxy;
        NS_ADDREF(*aNotify);
        return NS_OK;
    }
    
    nsresult rv = BuildProxy(PR_TRUE);
    
    if (NS_SUCCEEDED(rv))
    {
        *aNotify = mSyncProxy;
        NS_ADDREF(*aNotify);
    }
    return rv;
}


NS_IMETHODIMP
nsNetModRegEntry::GetAsyncProxy(nsINetNotify **aNotify) 
{
    nsAutoMonitor mon(mMonitor);

    if (mAsyncProxy)
    {
        *aNotify = mAsyncProxy;
        NS_ADDREF(*aNotify);
        return NS_OK;
    }

    nsresult rv = BuildProxy(PR_FALSE);
    
    if (NS_SUCCEEDED(rv))
    {
        *aNotify = mAsyncProxy;
        NS_ADDREF(*aNotify);
    }
    return rv;
}

NS_IMETHODIMP
nsNetModRegEntry::GetTopic(char **topic) 
{
    nsAutoMonitor mon(mMonitor);

    if (mTopic) 
    {
        *topic = (char *) nsMemory::Clone(mTopic, nsCRT::strlen(mTopic) + 1);
        return NS_OK;
    }
    return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsNetModRegEntry::Equals(nsINetModRegEntry* aEntry, PRBool *_retVal) 
{
    nsresult rv = NS_OK;
    *_retVal = PR_FALSE;

    char* topic;

    rv = aEntry->GetTopic(&topic);
    if (NS_FAILED(rv)) 
        return rv;
    
    if (topic && PL_strcmp(topic, mTopic)) 
    {
        nsCOMPtr<nsINetNotify> aSyncProxy;
        rv = aEntry->GetSyncProxy(getter_AddRefs(aSyncProxy));
        
        if(aSyncProxy == mSyncProxy)
        {
            *_retVal = PR_TRUE;
        }
        nsMemory::Free(topic);
    }
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
   
    mAsyncProxy = nsnull;
    mSyncProxy = nsnull;
    mRealNotifier = aNotify;

    NS_WITH_SERVICE(nsIEventQueueService, eventQService, kEventQueueServiceCID, result); 
    
    if (NS_FAILED(*result)) return;
    
    *result = eventQService->GetThreadEventQueue(NS_CURRENT_THREAD, getter_AddRefs(mEventQ)); 

    mMonitor = nsAutoMonitor::NewMonitor("nsNetModRegEntry");
}

nsresult
nsNetModRegEntry::BuildProxy(PRBool sync)
{
    if (mEventQ == nsnull)
        return NS_ERROR_NULL_POINTER;

    nsresult result;
    
    NS_WITH_SERVICE( nsIProxyObjectManager, proxyManager, kProxyObjectManagerCID, &result);
    
    if (NS_FAILED(result)) 
        return result;
    
    if (sync)
    {
        result = proxyManager->GetProxyForObject(  mEventQ,
                                                NS_GET_IID(nsINetNotify),
                                                mRealNotifier,
                                                PROXY_SYNC | PROXY_ALWAYS,
                                                getter_AddRefs(mSyncProxy));
    }
    else
    {
         result = proxyManager->GetProxyForObject( mEventQ,
                                                NS_GET_IID(nsINetNotify),
                                                mRealNotifier,
                                                PROXY_ASYNC | PROXY_ALWAYS,
                                                getter_AddRefs(mAsyncProxy));
    }
 
    return result;
}

nsNetModRegEntry::~nsNetModRegEntry() 
{
    delete [] mTopic;
    nsAutoMonitor::DestroyMonitor(mMonitor);
}
