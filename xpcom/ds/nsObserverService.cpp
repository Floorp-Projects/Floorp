/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   IBM Corp.
 */

#define NS_IMPL_IDS
#include "prlock.h"
#include "nsIFactory.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsIObserverService.h"
#include "nsObserverService.h"
#include "nsIObserverList.h"
#include "nsObserverList.h"
#include "nsHashtable.h"

static NS_DEFINE_CID(kObserverServiceCID, NS_OBSERVERSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

static nsObserverService* gObserverService = nsnull; // The one-and-only ObserverService

////////////////////////////////////////////////////////////////////////////////
// nsObserverService Implementation


NS_IMPL_THREADSAFE_ISUPPORTS1(nsObserverService, nsIObserverService)

NS_COM nsresult NS_NewObserverService(nsIObserverService** anObserverService)
{
    return nsObserverService::GetObserverService(anObserverService);
}

nsObserverService::nsObserverService()
    : mObserverTopicTable(NULL)
{
    NS_INIT_REFCNT();
    mObserverTopicTable = nsnull;
}

nsObserverService::~nsObserverService(void)
{
    if(mObserverTopicTable)
        delete mObserverTopicTable;
    gObserverService = nsnull;
}

NS_METHOD
nsObserverService::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    nsresult rv;
    nsObserverService* os = new nsObserverService();
    if (os == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(os);
    rv = os->QueryInterface(aIID, aInstancePtr);
    NS_RELEASE(os);
    return rv;
}

nsresult nsObserverService::GetObserverService(nsIObserverService** anObserverService)
{
    if (! gObserverService) {
        nsObserverService* it = new nsObserverService();
        if (! it)
            return NS_ERROR_OUT_OF_MEMORY;
        gObserverService = it;
    }

    NS_ADDREF(gObserverService);
    *anObserverService = gObserverService;
    return NS_OK;
}

static PRBool PR_CALLBACK 
ReleaseObserverList(nsHashKey *aKey, void *aData, void* closure)
{
    nsIObserverList* observerList = NS_STATIC_CAST(nsIObserverList*, aData);
    NS_RELEASE(observerList);
    return PR_TRUE;
}

nsresult nsObserverService::GetObserverList(const PRUnichar* aTopic, nsIObserverList** anObserverList)
{
    if (anObserverList == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }
	
	if(mObserverTopicTable == NULL) {
        mObserverTopicTable = new nsObjectHashtable(nsnull, nsnull,   // should never be cloned
                                                    ReleaseObserverList, nsnull,
                                                    256, PR_TRUE);
        if (mObserverTopicTable == NULL)
            return NS_ERROR_OUT_OF_MEMORY;
    }


	 nsStringKey key(aTopic);

    nsIObserverList *topicObservers = nsnull;
    if (mObserverTopicTable->Exists(&key)) {
        topicObservers = (nsIObserverList *) mObserverTopicTable->Get(&key);
        if (topicObservers != NULL) {
	        *anObserverList = topicObservers;	
        } else {
            NS_NewObserverList(&topicObservers);
            mObserverTopicTable->Put(&key, topicObservers);
        }
    } else {
        NS_NewObserverList(&topicObservers);
        *anObserverList = topicObservers;
        mObserverTopicTable->Put(&key, topicObservers);

    }

	return NS_OK;
}

NS_IMETHODIMP nsObserverService::AddObserver(nsIObserver* anObserver, const PRUnichar* aTopic)
{
	nsIObserverList* anObserverList;
	nsresult rv;

    if (anObserver == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

	if (aTopic == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

	rv = GetObserverList(aTopic, &anObserverList);
	if (NS_FAILED(rv)) return rv;

	if (anObserverList) {
        return anObserverList->AddObserver(anObserver);
    }
 	
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsObserverService::RemoveObserver(nsIObserver* anObserver, const PRUnichar* aTopic)
{
	nsIObserverList* anObserverList;
	nsresult rv;

    if (anObserver == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

	if (aTopic == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

	rv = GetObserverList(aTopic, &anObserverList);
	if (NS_FAILED(rv)) return rv;

	if (anObserverList) {
        return anObserverList->RemoveObserver(anObserver);
    }
 	
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsObserverService::EnumerateObserverList(const PRUnichar* aTopic, nsIEnumerator** anEnumerator)
{
	nsIObserverList* anObserverList;
	nsresult rv;

    if (anEnumerator == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

	if (aTopic == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

	rv = GetObserverList(aTopic, &anObserverList);
	if (NS_FAILED(rv)) return rv;

	if (anObserverList) {
        return anObserverList->EnumerateObserverList(anEnumerator);
    }
 	
	return NS_ERROR_FAILURE;
}

// Enumerate observers of aTopic and call Observe on each.
NS_IMETHODIMP nsObserverService::Notify( nsISupports *aSubject,
                                    const PRUnichar *aTopic,
                                    const PRUnichar *someData ) {
    nsresult rv = NS_OK;
    nsIEnumerator *observers;
    // Get observer list enumerator.
    rv = this->EnumerateObserverList( aTopic, &observers );
    if ( NS_SUCCEEDED( rv ) ) {
        // Go to start of observer list.
        rv = observers->First();
        // Continue until error or end of list.
        while ( observers->IsDone() != NS_OK && NS_SUCCEEDED(rv) ) {
            PRBool advanceToNext = PR_TRUE;
            // Get current item (observer).
            nsISupports *base;
            rv = observers->CurrentItem( &base );
            if ( NS_SUCCEEDED( rv ) ) {                           
                // Convert item to nsIObserver.
                nsIObserver *observer;
                rv = base->QueryInterface( NS_GET_IID(nsIObserver), (void**)&observer );
                if ( NS_SUCCEEDED( rv ) && observer ) {
                    // Tell the observer what's up.
                    observer->Observe( aSubject, aTopic, someData );
                    nsCOMPtr <nsISupports> currentItem;
                    observers->CurrentItem(getter_AddRefs(currentItem));
                    // check if the current item has changed, because the
                    // observer removed the old current item.
                    advanceToNext = (currentItem == base);
                    // Release the observer.
                    observer->Release();
                }
            NS_IF_RELEASE(base);
            }
            // Go on to next observer in list.
            if (advanceToNext)
              rv = observers->Next();
        }
        // Release the observer list.
        observers->Release();
        rv = NS_OK;
    }
    return rv;
}
////////////////////////////////////////////////////////////////////////////////

