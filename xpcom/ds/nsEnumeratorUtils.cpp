/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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

NS_IMPL_ISUPPORTS1(nsArrayEnumerator, nsISimpleEnumerator)

NS_IMETHODIMP
nsArrayEnumerator::HasMoreElements(PRBool* aResult)
{
    NS_PRECONDITION(aResult != 0, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (!mValueArray) {
        *aResult = PR_FALSE;
        return NS_OK;
    }

    PRUint32 cnt;
    nsresult rv = mValueArray->Count(&cnt);
    if (NS_FAILED(rv)) return rv;
    *aResult = (mIndex < (PRInt32) cnt);
    return NS_OK;
}

NS_IMETHODIMP
nsArrayEnumerator::GetNext(nsISupports** aResult)
{
    NS_PRECONDITION(aResult != 0, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (!mValueArray) {
        *aResult = nsnull;
        return NS_OK;
    }

    PRUint32 cnt;
    nsresult rv = mValueArray->Count(&cnt);
    if (NS_FAILED(rv)) return rv;
    if (mIndex >= (PRInt32) cnt)
        return NS_ERROR_UNEXPECTED;

    *aResult = mValueArray->ElementAt(mIndex++);
    return NS_OK;
}

extern "C" NS_COM nsresult
NS_NewArrayEnumerator(nsISimpleEnumerator* *result,
                      nsISupportsArray* array)
{
    nsArrayEnumerator* enumer = new nsArrayEnumerator(array);
    if (enumer == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(enumer);
    *result = enumer; 
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

NS_IMPL_ISUPPORTS1(nsSingletonEnumerator, nsISimpleEnumerator)

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

extern "C" NS_COM nsresult
NS_NewSingletonEnumerator(nsISimpleEnumerator* *result,
                          nsISupports* singleton)
{
    nsSingletonEnumerator* enumer = new nsSingletonEnumerator(singleton);
    if (enumer == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(enumer);
    *result = enumer; 
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


NS_IMPL_ISUPPORTS1(nsAdapterEnumerator, nsISimpleEnumerator)

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

extern "C" NS_COM nsresult
NS_NewAdapterEnumerator(nsISimpleEnumerator* *result,
                        nsIEnumerator* enumerator)
{
    nsAdapterEnumerator* enumer = new nsAdapterEnumerator(enumerator);
    if (enumer == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(enumer);
    *result = enumer; 
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////


