/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsEnumeratorUtils.h"


nsArrayEnumerator::nsArrayEnumerator(nsISupportsArray* aValueArray)
    : mValueArray(aValueArray),
      mIndex(0)
{
    NS_INIT_REFCNT();
    NS_IF_ADDREF(mValueArray);
}

nsArrayEnumerator::~nsArrayEnumerator(void)
{
    NS_IF_RELEASE(mValueArray);
}

NS_IMPL_ISUPPORTS(nsArrayEnumerator, nsISimpleEnumerator::GetIID());

NS_IMETHODIMP
nsArrayEnumerator::HasMoreElements(PRBool* aResult)
{
    NS_PRECONDITION(aResult != 0, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    *aResult = (mIndex < (PRInt32) mValueArray->Count());
    return NS_OK;
}

NS_IMETHODIMP
nsArrayEnumerator::GetNext(nsISupports** aResult)
{
    NS_PRECONDITION(aResult != 0, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (mIndex >= (PRInt32) mValueArray->Count())
        return NS_ERROR_UNEXPECTED;

    *aResult = mValueArray->ElementAt(mIndex++);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsSingletonEnumerator::nsSingletonEnumerator(nsISupports* aValue)
    : mValue(aValue)
{
    NS_INIT_REFCNT();
    NS_IF_ADDREF(mValue);
    mConsumed = (mValue ? PR_FALSE : PR_TRUE);
}

nsSingletonEnumerator::~nsSingletonEnumerator()
{
    NS_IF_RELEASE(mValue);
}

NS_IMPL_ISUPPORTS(nsSingletonEnumerator, nsISimpleEnumerator::GetIID());

NS_IMETHODIMP
nsSingletonEnumerator::HasMoreElements(PRBool* aResult)
{
    NS_PRECONDITION(aResult != 0, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    *aResult = !mConsumed;
    return NS_OK;
}


NS_IMETHODIMP
nsSingletonEnumerator::GetNext(nsISupports** aResult)
{
    NS_PRECONDITION(aResult != 0, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (mConsumed)
        return NS_ERROR_UNEXPECTED;

    mConsumed = PR_TRUE;

    NS_ADDREF(mValue);
    *aResult = mValue;
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////


nsAdapterEnumerator::nsAdapterEnumerator(nsIEnumerator* aEnum)
    : mEnum(aEnum), mCurrent(0), mStarted(PR_FALSE)
{
    NS_INIT_REFCNT();
    NS_ADDREF(mEnum);
}


nsAdapterEnumerator::~nsAdapterEnumerator()
{
    NS_RELEASE(mEnum);
    NS_IF_RELEASE(mCurrent);
}


NS_IMPL_ISUPPORTS(nsAdapterEnumerator, nsISimpleEnumerator::GetIID());


NS_IMETHODIMP
nsAdapterEnumerator::HasMoreElements(PRBool* aResult)
{
    nsresult rv;

    if (mCurrent) {
        *aResult = PR_TRUE;
        return NS_OK;
    }

    if (! mStarted) {
        mStarted = PR_TRUE;
        rv = mEnum->First();
        if (rv == NS_OK) {
            mEnum->CurrentItem(&mCurrent);
            *aResult = PR_TRUE;
        }
        else {
            *aResult = PR_FALSE;
        }
    }
    else {
        *aResult = PR_FALSE;

        rv = mEnum->IsDone();
        if (rv != NS_OK) {
            // We're not done. Advance to the next one.
            rv = mEnum->Next();
            if (rv == NS_OK) {
                mEnum->CurrentItem(&mCurrent);
                *aResult = PR_TRUE;
            }
        }
    }
    return NS_OK;
}


NS_IMETHODIMP
nsAdapterEnumerator::GetNext(nsISupports** aResult)
{
    nsresult rv;

    PRBool hasMore;
    rv = HasMoreElements(&hasMore);
    if (NS_FAILED(rv)) return rv;

    if (! hasMore)
        return NS_ERROR_UNEXPECTED;

    // No need to addref, we "transfer" the ownership to the caller.
    *aResult = mCurrent;
    mCurrent = 0;
    return NS_OK;
}


