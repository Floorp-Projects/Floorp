/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Benjamin Smedberg <benjamin@smedbergs.us>
 * 
 * Code moved from nsEmptyEnumerator.cpp:
 *   L. David Baron <dbaron@dbaron.org>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsEnumeratorUtils.h"

#include "nsISimpleEnumerator.h"
#include "nsIStringEnumerator.h"

#include "nsCOMPtr.h"

class EmptyEnumeratorImpl : public nsISimpleEnumerator,
                            public nsIUTF8StringEnumerator,
                            public nsIStringEnumerator
{
public:
    EmptyEnumeratorImpl() {}
    // nsISupports interface
    NS_DECL_ISUPPORTS_INHERITED  // not really inherited, but no mRefCnt

    // nsISimpleEnumerator
    NS_DECL_NSISIMPLEENUMERATOR
    NS_DECL_NSIUTF8STRINGENUMERATOR
    // can't use NS_DECL_NSISTRINGENUMERATOR because they share the
    // HasMore() signature
    NS_IMETHOD GetNext(nsAString& aResult);

    static EmptyEnumeratorImpl* GetInstance() {
        return const_cast<EmptyEnumeratorImpl*>(&kInstance);
    }

private:
    static const EmptyEnumeratorImpl kInstance;
};

// nsISupports interface
NS_IMETHODIMP_(nsrefcnt) EmptyEnumeratorImpl::AddRef(void)
{
    return 2;
}

NS_IMETHODIMP_(nsrefcnt) EmptyEnumeratorImpl::Release(void)
{
    return 1;
}

NS_IMPL_QUERY_INTERFACE3(EmptyEnumeratorImpl, nsISimpleEnumerator,
                         nsIUTF8StringEnumerator, nsIStringEnumerator)

// nsISimpleEnumerator interface
NS_IMETHODIMP EmptyEnumeratorImpl::HasMoreElements(bool* aResult)
{
    *aResult = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP EmptyEnumeratorImpl::HasMore(bool* aResult)
{
    *aResult = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP EmptyEnumeratorImpl::GetNext(nsISupports** aResult)
{
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP EmptyEnumeratorImpl::GetNext(nsACString& aResult)
{
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP EmptyEnumeratorImpl::GetNext(nsAString& aResult)
{
    return NS_ERROR_UNEXPECTED;
}

const EmptyEnumeratorImpl EmptyEnumeratorImpl::kInstance;

nsresult
NS_NewEmptyEnumerator(nsISimpleEnumerator** aResult)
{
    *aResult = EmptyEnumeratorImpl::GetInstance();
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

class nsSingletonEnumerator : public nsISimpleEnumerator
{
public:
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator methods
    NS_IMETHOD HasMoreElements(bool* aResult);
    NS_IMETHOD GetNext(nsISupports** aResult);

    nsSingletonEnumerator(nsISupports* aValue);

private:
    ~nsSingletonEnumerator();

protected:
    nsISupports* mValue;
    bool mConsumed;
};

nsSingletonEnumerator::nsSingletonEnumerator(nsISupports* aValue)
    : mValue(aValue)
{
    NS_IF_ADDREF(mValue);
    mConsumed = (mValue ? PR_FALSE : PR_TRUE);
}

nsSingletonEnumerator::~nsSingletonEnumerator()
{
    NS_IF_RELEASE(mValue);
}

NS_IMPL_ISUPPORTS1(nsSingletonEnumerator, nsISimpleEnumerator)

NS_IMETHODIMP
nsSingletonEnumerator::HasMoreElements(bool* aResult)
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

    *aResult = mValue;
    NS_ADDREF(*aResult);
    return NS_OK;
}

nsresult
NS_NewSingletonEnumerator(nsISimpleEnumerator* *result,
                          nsISupports* singleton)
{
    nsSingletonEnumerator* enumer = new nsSingletonEnumerator(singleton);
    if (enumer == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    *result = enumer; 
    NS_ADDREF(*result);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

class nsUnionEnumerator : public nsISimpleEnumerator
{
public:
    NS_DECL_ISUPPORTS

    // nsISimpleEnumerator methods
    NS_IMETHOD HasMoreElements(bool* aResult);
    NS_IMETHOD GetNext(nsISupports** aResult);

    nsUnionEnumerator(nsISimpleEnumerator* firstEnumerator,
                      nsISimpleEnumerator* secondEnumerator);

private:
    ~nsUnionEnumerator();

protected:
    nsCOMPtr<nsISimpleEnumerator> mFirstEnumerator, mSecondEnumerator;
    bool mConsumed;
    bool mAtSecond;
};

nsUnionEnumerator::nsUnionEnumerator(nsISimpleEnumerator* firstEnumerator,
                                     nsISimpleEnumerator* secondEnumerator)
    : mFirstEnumerator(firstEnumerator),
      mSecondEnumerator(secondEnumerator),
      mConsumed(PR_FALSE), mAtSecond(PR_FALSE)
{
}

nsUnionEnumerator::~nsUnionEnumerator()
{
}

NS_IMPL_ISUPPORTS1(nsUnionEnumerator, nsISimpleEnumerator)

NS_IMETHODIMP
nsUnionEnumerator::HasMoreElements(bool* aResult)
{
    NS_PRECONDITION(aResult != 0, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    if (mConsumed) {
        *aResult = PR_FALSE;
        return NS_OK;
    }

    if (! mAtSecond) {
        rv = mFirstEnumerator->HasMoreElements(aResult);
        if (NS_FAILED(rv)) return rv;

        if (*aResult)
            return NS_OK;

        mAtSecond = PR_TRUE;
    }

    rv = mSecondEnumerator->HasMoreElements(aResult);
    if (NS_FAILED(rv)) return rv;

    if (*aResult)
        return NS_OK;

    *aResult = PR_FALSE;
    mConsumed = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
nsUnionEnumerator::GetNext(nsISupports** aResult)
{
    NS_PRECONDITION(aResult != 0, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (mConsumed)
        return NS_ERROR_UNEXPECTED;

    if (! mAtSecond)
        return mFirstEnumerator->GetNext(aResult);

    return mSecondEnumerator->GetNext(aResult);
}

nsresult
NS_NewUnionEnumerator(nsISimpleEnumerator* *result,
                      nsISimpleEnumerator* firstEnumerator,
                      nsISimpleEnumerator* secondEnumerator)
{
    *result = nsnull;
    if (! firstEnumerator) {
        *result = secondEnumerator;
    } else if (! secondEnumerator) {
        *result = firstEnumerator;
    } else {
        nsUnionEnumerator* enumer = new nsUnionEnumerator(firstEnumerator, secondEnumerator);
        if (enumer == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        *result = enumer; 
    }
    NS_ADDREF(*result);
    return NS_OK;
}
