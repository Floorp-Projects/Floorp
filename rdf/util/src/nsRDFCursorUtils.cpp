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

#include "nsRDFCursorUtils.h"
#include "nsIRDFDataSource.h"
#include "nscore.h"

nsRDFArrayCursor::nsRDFArrayCursor(nsIRDFDataSource* aDataSource,
                                   nsISupportsArray* valueArray)
    : mDataSource(aDataSource), nsSupportsArrayEnumerator(valueArray)
{
    NS_IF_ADDREF(mDataSource);
}

nsRDFArrayCursor::~nsRDFArrayCursor(void)
{
    NS_IF_RELEASE(mDataSource);
}

NS_IMPL_ADDREF(nsRDFArrayCursor);
NS_IMPL_RELEASE(nsRDFArrayCursor);

NS_IMETHODIMP
nsRDFArrayCursor::QueryInterface(REFNSIID iid, void** result)
{
    if (!result)
        return NS_ERROR_NULL_POINTER;

    static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
    if (iid.Equals(nsIRDFCursor::IID()) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFCursor*, this);
        AddRef();
        return NS_OK;
    }
    else if (iid.Equals(nsIEnumerator::IID())) {
        *result = NS_STATIC_CAST(nsIEnumerator*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP nsRDFArrayCursor::Advance(void)
{ 
    nsresult rv = Next();
    if (NS_SUCCEEDED(rv)) return rv;
    return NS_ERROR_RDF_CURSOR_EMPTY;
}

NS_IMETHODIMP nsRDFArrayCursor::GetDataSource(nsIRDFDataSource** aDataSource)
{
    *aDataSource = mDataSource;
    NS_IF_ADDREF(mDataSource);
    return NS_OK;
}
 
NS_IMETHODIMP nsRDFArrayCursor::GetValue(nsIRDFNode** aValue)
{
    return CurrentItem((nsISupports**)aValue);
}

////////////////////////////////////////////////////////////////////////////////

nsRDFArrayAssertionCursor::nsRDFArrayAssertionCursor(nsIRDFDataSource* aDataSource,
                                                     nsIRDFResource* subject,
                                                     nsIRDFResource* predicate,
                                                     nsISupportsArray* objectsArray,
                                                     PRBool truthValue)
    : nsRDFArrayCursor(aDataSource, objectsArray),
      mSubject(subject), mPredicate(predicate), mTruthValue(truthValue)
{
    NS_ADDREF(mSubject);
    NS_ADDREF(mPredicate);
}
 
nsRDFArrayAssertionCursor::~nsRDFArrayAssertionCursor()
{
    NS_RELEASE(mSubject);
    NS_RELEASE(mPredicate);
}

NS_IMPL_ADDREF(nsRDFArrayAssertionCursor);
NS_IMPL_RELEASE(nsRDFArrayAssertionCursor);

NS_IMETHODIMP
nsRDFArrayAssertionCursor::QueryInterface(REFNSIID iid, void** result)
{
    if (!result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(nsIRDFAssertionCursor::IID())) {
        *result = NS_STATIC_CAST(nsIRDFAssertionCursor*, this);
        AddRef();
        return NS_OK;
    }
    return nsRDFArrayCursor::QueryInterface(iid, result);
}

NS_IMETHODIMP 
nsRDFArrayAssertionCursor::GetSubject(nsIRDFResource* *aSubject)
{
    *aSubject = mSubject;
    NS_ADDREF(mSubject);
    return NS_OK;
}

NS_IMETHODIMP nsRDFArrayAssertionCursor::GetPredicate(nsIRDFResource* *aPredicate)
{
    *aPredicate = mPredicate;
    NS_ADDREF(mPredicate);
    return NS_OK;
}

NS_IMETHODIMP nsRDFArrayAssertionCursor::GetObject(nsIRDFNode* *aObject)
{
    return nsRDFArrayCursor::GetValue(aObject);
}

NS_IMETHODIMP nsRDFArrayAssertionCursor::GetTruthValue(PRBool *aTruthValue)
{
    *aTruthValue = mTruthValue;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsRDFSingletonAssertionCursor::nsRDFSingletonAssertionCursor(nsIRDFDataSource* aDataSource,
                                                             nsIRDFNode* node,
                                                             nsIRDFResource* predicate,
                                                             PRBool inverse,
                                                             PRBool truthValue)
    : mDataSource(aDataSource), mNode(node), mPredicate(predicate),
      mValue(nsnull), mInverse(inverse), mTruthValue(truthValue), mConsumed(PR_FALSE)
{
    NS_ADDREF(mDataSource);
    NS_ADDREF(mNode);
    NS_ADDREF(mPredicate);
}

nsRDFSingletonAssertionCursor::~nsRDFSingletonAssertionCursor()
{
    NS_RELEASE(mDataSource);
    NS_RELEASE(mNode);
    NS_RELEASE(mPredicate);
    NS_IF_RELEASE(mValue);
}

NS_IMPL_ADDREF(nsRDFSingletonAssertionCursor);
NS_IMPL_RELEASE(nsRDFSingletonAssertionCursor);

NS_IMETHODIMP
nsRDFSingletonAssertionCursor::QueryInterface(REFNSIID iid, void** result)
{
    if (!result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(nsIRDFAssertionCursor::IID()) ||
        iid.Equals(nsIRDFCursor::IID()) ||
        iid.Equals(::nsISupports::IID())) {
        *result = NS_STATIC_CAST(nsIRDFAssertionCursor*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP nsRDFSingletonAssertionCursor::Advance(void)
{
    mConsumed = PR_TRUE;    
    return NS_ERROR_RDF_CURSOR_EMPTY;
}

NS_IMETHODIMP nsRDFSingletonAssertionCursor::GetDataSource(nsIRDFDataSource** aDataSource)
{
    *aDataSource = mDataSource;
    NS_ADDREF(mDataSource);
    return NS_OK;
}

NS_IMETHODIMP nsRDFSingletonAssertionCursor::GetValue(nsIRDFNode** aValue)
{
    if (mConsumed)
        return NS_ERROR_RDF_CURSOR_EMPTY;
    if (mValue == nsnull) {
        if (mInverse)
            return mDataSource->GetSource(mPredicate, mNode, mTruthValue,
                                          (nsIRDFResource**)&mValue);
        else
            return mDataSource->GetTarget(NS_STATIC_CAST(nsIRDFResource*, mNode),
                                          mPredicate,  mTruthValue, &mValue);
    }
    *aValue = mValue;
    return NS_OK;
}

NS_IMETHODIMP nsRDFSingletonAssertionCursor::GetSubject(nsIRDFResource* *aSubject)
{
    if (mConsumed)
        return NS_ERROR_RDF_CURSOR_EMPTY;
    if (mInverse) {
        return GetValue((nsIRDFNode**)aSubject); 
    }
    else {
        *aSubject = NS_STATIC_CAST(nsIRDFResource*, mNode);
        NS_ADDREF(mNode);
        return NS_OK;
    }
}

NS_IMETHODIMP nsRDFSingletonAssertionCursor::GetPredicate(nsIRDFResource* *aPredicate)
{
    if (mConsumed)
        return NS_ERROR_RDF_CURSOR_EMPTY;
    *aPredicate = mPredicate;
    NS_ADDREF(mPredicate);
    return NS_OK;
}

NS_IMETHODIMP nsRDFSingletonAssertionCursor::GetObject(nsIRDFNode* *aObject)
{
    if (mConsumed)
        return NS_ERROR_RDF_CURSOR_EMPTY;
    if (mInverse) {
        *aObject = mNode;
        NS_ADDREF(mNode);
        return NS_OK;
    }
    else {
        return GetValue(aObject); 
    }
}

NS_IMETHODIMP nsRDFSingletonAssertionCursor::GetTruthValue(PRBool *aTruthValue)
{
    if (mConsumed)
        return NS_ERROR_RDF_CURSOR_EMPTY;
    *aTruthValue = mTruthValue;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsRDFEnumeratorCursor::nsRDFEnumeratorCursor(nsIRDFDataSource* aDataSource,
                                             nsIEnumerator* valueEnumerator)
    : mDataSource(aDataSource), mEnum(valueEnumerator)
{
    NS_IF_ADDREF(mEnum);
    NS_IF_ADDREF(mDataSource);
}

nsRDFEnumeratorCursor::~nsRDFEnumeratorCursor(void)
{
    NS_IF_RELEASE(mEnum);
    NS_IF_RELEASE(mDataSource);
}

NS_IMPL_ADDREF(nsRDFEnumeratorCursor);
NS_IMPL_RELEASE(nsRDFEnumeratorCursor);

NS_IMETHODIMP
nsRDFEnumeratorCursor::QueryInterface(REFNSIID iid, void** result)
{
    if (!result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(nsIRDFCursor::IID()) ||
        iid.Equals(::nsISupports::IID())) {
        *result = NS_STATIC_CAST(nsIRDFCursor*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP nsRDFEnumeratorCursor::Advance(void)
{ 
    nsresult rv = mEnum->Next();
    if (NS_SUCCEEDED(rv)) return rv;
    return NS_ERROR_RDF_CURSOR_EMPTY;
}

NS_IMETHODIMP nsRDFEnumeratorCursor::GetDataSource(nsIRDFDataSource** aDataSource)
{
    *aDataSource = mDataSource;
    NS_IF_ADDREF(mDataSource);
    return NS_OK;
}
 
NS_IMETHODIMP nsRDFEnumeratorCursor::GetValue(nsIRDFNode** aValue)
{
    return mEnum->CurrentItem((nsISupports**)aValue);
}

////////////////////////////////////////////////////////////////////////////////

nsRDFEnumeratorAssertionCursor::nsRDFEnumeratorAssertionCursor(nsIRDFDataSource* aDataSource,
                                                               nsIRDFResource* subject,
                                                               nsIRDFResource* predicate,
                                                               nsIEnumerator* objectsEnumerator,
                                                               PRBool truthValue)
    : nsRDFEnumeratorCursor(aDataSource, objectsEnumerator),
      mSubject(subject), mPredicate(predicate), mTruthValue(truthValue)
{
    NS_ADDREF(mSubject);
    NS_ADDREF(mPredicate);
}
 
nsRDFEnumeratorAssertionCursor::~nsRDFEnumeratorAssertionCursor()
{
    NS_RELEASE(mSubject);
    NS_RELEASE(mPredicate);
}

NS_IMPL_ADDREF(nsRDFEnumeratorAssertionCursor);
NS_IMPL_RELEASE(nsRDFEnumeratorAssertionCursor);

NS_IMETHODIMP
nsRDFEnumeratorAssertionCursor::QueryInterface(REFNSIID iid, void** result)
{
    if (!result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(nsIRDFAssertionCursor::IID())) {
        *result = NS_STATIC_CAST(nsIRDFAssertionCursor*, this);
        AddRef();
        return NS_OK;
    }
    return nsRDFEnumeratorCursor::QueryInterface(iid, result);
}

NS_IMETHODIMP 
nsRDFEnumeratorAssertionCursor::GetSubject(nsIRDFResource* *aSubject)
{
    *aSubject = mSubject;
    NS_ADDREF(mSubject);
    return NS_OK;
}

NS_IMETHODIMP nsRDFEnumeratorAssertionCursor::GetPredicate(nsIRDFResource* *aPredicate)
{
    *aPredicate = mPredicate;
    NS_ADDREF(mPredicate);
    return NS_OK;
}

NS_IMETHODIMP nsRDFEnumeratorAssertionCursor::GetObject(nsIRDFNode* *aObject)
{
    return nsRDFEnumeratorCursor::GetValue(aObject);
}

NS_IMETHODIMP nsRDFEnumeratorAssertionCursor::GetTruthValue(PRBool *aTruthValue)
{
    *aTruthValue = mTruthValue;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
