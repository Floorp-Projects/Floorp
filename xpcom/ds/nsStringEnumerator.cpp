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
 * The Original Code is String Enumerator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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


#include "nsStringEnumerator.h"
#include "prtypes.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsISimpleEnumerator.h"
#include "nsSupportsPrimitives.h"

//
// nsStringEnumerator
//

class nsStringEnumerator : public nsIStringEnumerator,
                           public nsIUTF8StringEnumerator,
                           public nsISimpleEnumerator
{
public:
    nsStringEnumerator(const nsTArray<nsString>* aArray, bool aOwnsArray) :
        mArray(aArray), mIndex(0), mOwnsArray(aOwnsArray), mIsUnicode(true)
    {}
    
    nsStringEnumerator(const nsTArray<nsCString>* aArray, bool aOwnsArray) :
        mCArray(aArray), mIndex(0), mOwnsArray(aOwnsArray), mIsUnicode(false)
    {}

    nsStringEnumerator(const nsTArray<nsString>* aArray, nsISupports* aOwner) :
        mArray(aArray), mIndex(0), mOwner(aOwner), mOwnsArray(false), mIsUnicode(true)
    {}
    
    nsStringEnumerator(const nsTArray<nsCString>* aArray, nsISupports* aOwner) :
        mCArray(aArray), mIndex(0), mOwner(aOwner), mOwnsArray(false), mIsUnicode(false)
    {}

    NS_DECL_ISUPPORTS
    NS_DECL_NSIUTF8STRINGENUMERATOR

    // have to declare nsIStringEnumerator manually, because of
    // overlapping method names
    NS_IMETHOD GetNext(nsAString& aResult);
    NS_DECL_NSISIMPLEENUMERATOR

private:
    ~nsStringEnumerator() {
        if (mOwnsArray) {
            // const-casting is safe here, because the NS_New*
            // constructors make sure mOwnsArray is consistent with
            // the constness of the objects
            if (mIsUnicode)
                delete const_cast<nsTArray<nsString>*>(mArray);
            else
                delete const_cast<nsTArray<nsCString>*>(mCArray);
        }
    }

    union {
        const nsTArray<nsString>* mArray;
        const nsTArray<nsCString>* mCArray;
    };

    inline PRUint32 Count() {
        return mIsUnicode ? mArray->Length() : mCArray->Length();
    }
    
    PRUint32 mIndex;

    // the owner allows us to hold a strong reference to the object
    // that owns the array. Having a non-null value in mOwner implies
    // that mOwnsArray is false, because we rely on the real owner
    // to release the array
    nsCOMPtr<nsISupports> mOwner;
    bool mOwnsArray;
    bool mIsUnicode;
};

NS_IMPL_ISUPPORTS3(nsStringEnumerator,
                   nsIStringEnumerator,
                   nsIUTF8StringEnumerator,
                   nsISimpleEnumerator)

NS_IMETHODIMP
nsStringEnumerator::HasMore(bool* aResult)
{
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = mIndex < Count();
    return NS_OK;
}

NS_IMETHODIMP
nsStringEnumerator::HasMoreElements(bool* aResult)
{
    return HasMore(aResult);
}

NS_IMETHODIMP
nsStringEnumerator::GetNext(nsISupports** aResult)
{
    if (mIsUnicode) {
        nsSupportsStringImpl* stringImpl = new nsSupportsStringImpl();
        if (!stringImpl) return NS_ERROR_OUT_OF_MEMORY;
        
        stringImpl->SetData(mArray->ElementAt(mIndex++));
        *aResult = stringImpl;
    }
    else {
        nsSupportsCStringImpl* cstringImpl = new nsSupportsCStringImpl();
        if (!cstringImpl) return NS_ERROR_OUT_OF_MEMORY;

        cstringImpl->SetData(mCArray->ElementAt(mIndex++));
        *aResult = cstringImpl;
    }
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMETHODIMP
nsStringEnumerator::GetNext(nsAString& aResult)
{
    NS_ENSURE_TRUE(mIndex < Count(), NS_ERROR_UNEXPECTED);

    if (mIsUnicode)
        aResult = mArray->ElementAt(mIndex++);
    else
        CopyUTF8toUTF16(mCArray->ElementAt(mIndex++), aResult);
    
    return NS_OK;
}

NS_IMETHODIMP
nsStringEnumerator::GetNext(nsACString& aResult)
{
    NS_ENSURE_TRUE(mIndex < Count(), NS_ERROR_UNEXPECTED);
    
    if (mIsUnicode)
        CopyUTF16toUTF8(mArray->ElementAt(mIndex++), aResult);
    else
        aResult = mCArray->ElementAt(mIndex++);
    
    return NS_OK;
}

template<class T>
static inline nsresult
StringEnumeratorTail(T** aResult NS_INPARAM)
{
    if (!*aResult)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(*aResult);
    return NS_OK;
}

//
// constructors
//

nsresult
NS_NewStringEnumerator(nsIStringEnumerator** aResult,
                       const nsTArray<nsString>* aArray, nsISupports* aOwner)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_ARG_POINTER(aArray);
    
    *aResult = new nsStringEnumerator(aArray, aOwner);
    return StringEnumeratorTail(aResult);
}


nsresult
NS_NewUTF8StringEnumerator(nsIUTF8StringEnumerator** aResult,
                           const nsTArray<nsCString>* aArray, nsISupports* aOwner)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_ARG_POINTER(aArray);
    
    *aResult = new nsStringEnumerator(aArray, aOwner);
    return StringEnumeratorTail(aResult);
}

nsresult
NS_NewAdoptingStringEnumerator(nsIStringEnumerator** aResult,
                               nsTArray<nsString>* aArray)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_ARG_POINTER(aArray);
    
    *aResult = new nsStringEnumerator(aArray, true);
    return StringEnumeratorTail(aResult);
}

nsresult
NS_NewAdoptingUTF8StringEnumerator(nsIUTF8StringEnumerator** aResult,
                                   nsTArray<nsCString>* aArray)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_ARG_POINTER(aArray);
    
    *aResult = new nsStringEnumerator(aArray, true);
    return StringEnumeratorTail(aResult);
}

// const ones internally just forward to the non-const equivalents
nsresult
NS_NewStringEnumerator(nsIStringEnumerator** aResult,
                       const nsTArray<nsString>* aArray)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_ARG_POINTER(aArray);
    
    *aResult = new nsStringEnumerator(aArray, false);
    return StringEnumeratorTail(aResult);
}

nsresult
NS_NewUTF8StringEnumerator(nsIUTF8StringEnumerator** aResult,
                           const nsTArray<nsCString>* aArray)
{
    NS_ENSURE_ARG_POINTER(aResult);
    NS_ENSURE_ARG_POINTER(aArray);
    
    *aResult = new nsStringEnumerator(aArray, false);
    return StringEnumeratorTail(aResult);
}

