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
#include "nsCOMPtr.h"
#include "nsIWeakReference.h"


#define NS_AUTOLOCK(__monitor) nsAutoLock __lock(__monitor)

static NS_DEFINE_CID(kObserverListCID, NS_OBSERVERLIST_CID);


class nsObserverListEnumerator : public nsIBidirectionalEnumerator
	{
		public:
			nsObserverListEnumerator( nsISupportsArray* );

			NS_DECL_ISUPPORTS
			NS_DECL_NSIENUMERATOR
			NS_DECL_NSIBIDIRECTIONALENUMERATOR

		private:
			PRUint32 GetTargetArraySize() const;
			nsresult MoveToIndex( PRUint32 );

		private:
			nsCOMPtr<nsISupportsArray>	mTargetArray;
			PRUint32										mCurrentItemIndex;
	};

NS_IMPL_ISUPPORTS2(nsObserverListEnumerator, nsIBidirectionalEnumerator, nsIEnumerator)

nsObserverListEnumerator::nsObserverListEnumerator( nsISupportsArray* anArray )
		: mRefCnt(0),
			mTargetArray(anArray),
			mCurrentItemIndex(0)
	{
		// nothing else to do here
	}

PRUint32
nsObserverListEnumerator::GetTargetArraySize() const
	{
		PRUint32 array_size = 0;
		mTargetArray->Count(&array_size);
		return array_size;
	}

nsresult
nsObserverListEnumerator::MoveToIndex( PRUint32 aNewIndex )
	{
		nsresult status;
		if ( aNewIndex < GetTargetArraySize() )
			{
				mCurrentItemIndex = aNewIndex;
				status = NS_OK;
			}
		else
			status = NS_ERROR_FAILURE; // array bounds violation

		return status;
	}


NS_IMETHODIMP
nsObserverListEnumerator::First()
	{
		NS_ASSERTION(mTargetArray, "must enumerate over an non-null list");

		return MoveToIndex(0);
	}

NS_IMETHODIMP
nsObserverListEnumerator::Last()
	{
		NS_ASSERTION(mTargetArray, "must enumerate over an non-null list");

		return MoveToIndex(GetTargetArraySize()-1);
	}

NS_IMETHODIMP
nsObserverListEnumerator::Prev()
	{
		NS_ASSERTION(mTargetArray, "must enumerate over an non-null list");

		return MoveToIndex(mCurrentItemIndex-1);
	}

NS_IMETHODIMP
nsObserverListEnumerator::Next()
	{
		NS_ASSERTION(mTargetArray, "must enumerate over an non-null list");

		return MoveToIndex(mCurrentItemIndex+1);
	}

NS_IMETHODIMP
nsObserverListEnumerator::CurrentItem( nsISupports** aItemPtr )
	{
		NS_ASSERTION(mTargetArray, "must enumerate over an non-null list");
		NS_ASSERTION(aItemPtr, "must supply a place to put the result");

		nsWeakPtr weakPtr = getter_AddRefs(NS_REINTERPRET_CAST(nsIWeakReference*, mTargetArray->ElementAt(mCurrentItemIndex)));

		if ( !aItemPtr || !weakPtr )
			return NS_ERROR_NULL_POINTER;

		return weakPtr->QueryReferent(NS_GET_IID(nsIObserver),
                                      (void **)aItemPtr);
	}

NS_IMETHODIMP
nsObserverListEnumerator::IsDone()
	{
		NS_ASSERTION(mTargetArray, "must enumerate over an non-null list");

		return  (mCurrentItemIndex == (GetTargetArraySize()-1)) ? NS_OK : NS_ENUMERATOR_FALSE;
	}



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

#if NS_WEAK_OBSERVERS
	nsWeakPtr observer_ref = getter_AddRefs(NS_GetWeakReference(*anObserver));
	if(observer_ref) {
		inserted = mObserverList->AppendElement(observer_ref); 
#else
	if(*anObserver) {
		inserted = mObserverList->AppendElement(*anObserver);
#endif
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

#if NS_WEAK_OBSERVERS
	nsWeakPtr observer_ref = getter_AddRefs(NS_GetWeakReference(*anObserver));
	if(observer_ref) {
		removed = mObserverList->RemoveElement(observer_ref);  
#else
	if(*anObserver) {
		removed = mObserverList->RemoveElement(*anObserver);
#endif
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

#if NS_WEAK_OBSERVERS
	nsCOMPtr<nsIBidirectionalEnumerator> enumerator = new nsObserverListEnumerator(mObserverList);
	if ( !enumerator )
		return NS_ERROR_OUT_OF_MEMORY;

	return CallQueryInterface(enumerator, anEnumerator);
#else
	return mObserverList->Enumerate(anEnumerator);
#endif
}

