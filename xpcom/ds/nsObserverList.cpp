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
 */

#define NS_IMPL_IDS
#include "pratom.h"
#include "nsIObserverList.h"
#include "nsObserverList.h"
#include "nsString.h"
#include "nsAutoLock.h"


#define NS_AUTOLOCK(__monitor) nsAutoLock __lock(__monitor)

static NS_DEFINE_CID(kObserverListCID, NS_OBSERVERLIST_CID);


////////////////////////////////////////////////////////////////////////////////
// nsObserverList Implementation


NS_IMPL_ISUPPORTS1(nsObserverList, nsIObserverList)

NS_COM nsresult NS_NewObserverList(nsIObserverList** anObserverList)
{

    if (anObserverList == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }  
    nsObserverList* it = new nsObserverList();

    if (it == 0) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    return it->QueryInterface(NS_GET_IID(nsIObserverList), (void **) anObserverList);
}

nsObserverList::nsObserverList()
    : mLock(nsnull),
        mObserverList(NULL)
{
    NS_INIT_REFCNT();
    mLock = PR_NewLock();
}

nsObserverList::~nsObserverList(void)
{
    PR_DestroyLock(mLock);
    NS_IF_RELEASE(mObserverList);
}


nsresult nsObserverList::AddObserver(nsIObserver** anObserver)
{
	nsresult rv;
	PRBool inserted;
    
	NS_AUTOLOCK(mLock);

    if (anObserver == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }  

	if(!mObserverList) {
        rv = NS_NewISupportsArray(&mObserverList);
		if (NS_FAILED(rv)) return rv;
    }

	if(*anObserver) {
		inserted = mObserverList->AppendElement(*anObserver); 
		return inserted ? NS_OK : NS_ERROR_FAILURE;
    }

	return NS_ERROR_FAILURE;
}

nsresult nsObserverList::RemoveObserver(nsIObserver** anObserver)
{
	PRBool removed;

 	NS_AUTOLOCK(mLock);

    if (anObserver == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }  

    if(!mObserverList) {
        return NS_ERROR_FAILURE;
    }

	if(*anObserver) {
		removed = mObserverList->RemoveElement(*anObserver);  
		return removed ? NS_OK : NS_ERROR_FAILURE;
    }

    return NS_ERROR_FAILURE;

}

NS_IMETHODIMP nsObserverList::EnumerateObserverList(nsIEnumerator** anEnumerator)
{
	NS_AUTOLOCK(mLock);

    if (anEnumerator == NULL)
    {
        return NS_ERROR_NULL_POINTER;
    }

    if(!mObserverList) {
        return NS_ERROR_FAILURE;
    }
    
 	return mObserverList->Enumerate(anEnumerator);
}

