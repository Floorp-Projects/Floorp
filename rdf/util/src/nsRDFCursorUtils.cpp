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
    : nsSupportsArrayEnumerator(valueArray), mDataSource(aDataSource),
      mStarted(PR_FALSE)
{
    NS_IF_ADDREF(mDataSource);
}

nsRDFArrayCursor::~nsRDFArrayCursor(void)
{
    NS_IF_RELEASE(mDataSource);
}

NS_IMPL_ISUPPORTS_INHERITED(nsRDFArrayCursor, 
                            nsSupportsArrayEnumerator,
                            nsIRDFCursor);

NS_IMETHODIMP nsRDFArrayCursor::Advance(void)
{ 
    if (!mStarted) {
        mStarted = PR_TRUE;
        nsresult rv = First();
        if (NS_FAILED(rv)) return NS_RDF_CURSOR_EMPTY;
    }
    else {
        nsresult rv = Next();
        if (NS_FAILED(rv)) return NS_RDF_CURSOR_EMPTY;
    }
    return IsDone() == NS_OK ? NS_RDF_CURSOR_EMPTY : NS_OK;
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

NS_IMPL_ISUPPORTS_INHERITED(nsRDFArrayAssertionCursor,
                            nsRDFArrayCursor,
                            nsIRDFAssertionCursor);

NS_IMETHODIMP 
nsRDFArrayAssertionCursor::GetSource(nsIRDFResource* *aSubject)
{
    *aSubject = mSubject;
    NS_ADDREF(mSubject);
    return NS_OK;
}

NS_IMETHODIMP nsRDFArrayAssertionCursor::GetLabel(nsIRDFResource* *aPredicate)
{
    *aPredicate = mPredicate;
    NS_ADDREF(mPredicate);
    return NS_OK;
}

NS_IMETHODIMP nsRDFArrayAssertionCursor::GetTarget(nsIRDFNode* *aObject)
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
    NS_INIT_REFCNT();
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

    if (iid.Equals(nsIRDFAssertionCursor::GetIID()) ||
        iid.Equals(nsIRDFCursor::GetIID()) ||
        iid.Equals(::nsISupports::GetIID())) {
        *result = NS_STATIC_CAST(nsIRDFAssertionCursor*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP nsRDFSingletonAssertionCursor::Advance(void)
{
    if (!mConsumed) {
        mConsumed = PR_TRUE;    
        return NS_RDF_CURSOR_EMPTY;
    }
    return NS_OK;
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
        return NS_RDF_CURSOR_EMPTY;
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

NS_IMETHODIMP nsRDFSingletonAssertionCursor::GetSource(nsIRDFResource* *aSubject)
{
    if (mConsumed)
        return NS_RDF_CURSOR_EMPTY;
    if (mInverse) {
        return GetValue((nsIRDFNode**)aSubject); 
    }
    else {
        *aSubject = NS_STATIC_CAST(nsIRDFResource*, mNode);
        NS_ADDREF(mNode);
        return NS_OK;
    }
}

NS_IMETHODIMP nsRDFSingletonAssertionCursor::GetLabel(nsIRDFResource* *aPredicate)
{
    if (mConsumed)
        return NS_RDF_CURSOR_EMPTY;
    *aPredicate = mPredicate;
    NS_ADDREF(mPredicate);
    return NS_OK;
}

NS_IMETHODIMP nsRDFSingletonAssertionCursor::GetTarget(nsIRDFNode* *aObject)
{
    if (mConsumed)
        return NS_RDF_CURSOR_EMPTY;
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
        return NS_RDF_CURSOR_EMPTY;
    *aTruthValue = mTruthValue;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsRDFArrayArcsCursor::nsRDFArrayArcsCursor(nsIRDFDataSource* aDataSource,
                                           nsIRDFNode* node,
                                           nsISupportsArray* arcs)
    : nsRDFArrayCursor(aDataSource, arcs), mNode(node)
{
    NS_ADDREF(mNode);
}

nsRDFArrayArcsCursor::~nsRDFArrayArcsCursor()
{
    NS_RELEASE(mNode);
}

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED(nsRDFArrayArcsOutCursor,
                            nsRDFArrayArcsCursor,
                            nsIRDFArcsOutCursor);

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED(nsRDFArrayArcsInCursor,
                            nsRDFArrayArcsCursor,
                            nsIRDFArcsInCursor);

////////////////////////////////////////////////////////////////////////////////

nsRDFEnumeratorCursor::nsRDFEnumeratorCursor(nsIRDFDataSource* aDataSource,
                                             nsIEnumerator* valueEnumerator)
    : mDataSource(aDataSource), mEnum(valueEnumerator), mStarted(PR_FALSE)
{
    NS_INIT_REFCNT();
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

    if (iid.Equals(nsIRDFCursor::GetIID()) ||
        iid.Equals(::nsISupports::GetIID())) {
        *result = NS_STATIC_CAST(nsIRDFCursor*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP nsRDFEnumeratorCursor::Advance(void)
{ 
    if (!mStarted) {
        mStarted = PR_TRUE;
        nsresult rv = mEnum->First();
        if (NS_FAILED(rv)) return NS_RDF_CURSOR_EMPTY;
    }
    else {
        nsresult rv = mEnum->Next();
        if (NS_FAILED(rv)) return NS_RDF_CURSOR_EMPTY;
    }
    return mEnum->IsDone() == NS_OK ? NS_RDF_CURSOR_EMPTY : NS_OK;
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

NS_IMPL_ISUPPORTS_INHERITED(nsRDFEnumeratorAssertionCursor,
                            nsRDFEnumeratorCursor,
                            nsIRDFAssertionCursor);

NS_IMETHODIMP 
nsRDFEnumeratorAssertionCursor::GetSource(nsIRDFResource* *aSubject)
{
    *aSubject = mSubject;
    NS_ADDREF(mSubject);
    return NS_OK;
}

NS_IMETHODIMP nsRDFEnumeratorAssertionCursor::GetLabel(nsIRDFResource* *aPredicate)
{
    *aPredicate = mPredicate;
    NS_ADDREF(mPredicate);
    return NS_OK;
}

NS_IMETHODIMP nsRDFEnumeratorAssertionCursor::GetTarget(nsIRDFNode* *aObject)
{
    return nsRDFEnumeratorCursor::GetValue(aObject);
}

NS_IMETHODIMP nsRDFEnumeratorAssertionCursor::GetTruthValue(PRBool *aTruthValue)
{
    *aTruthValue = mTruthValue;
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

nsRDFEnumeratorArcsCursor::nsRDFEnumeratorArcsCursor(nsIRDFDataSource* aDataSource,
                                                     nsIRDFNode* node,
                                                     nsIEnumerator* arcs)
    : nsRDFEnumeratorCursor(aDataSource, arcs), mNode(node)
{
    NS_ADDREF(mNode);
}

nsRDFEnumeratorArcsCursor::~nsRDFEnumeratorArcsCursor()
{
    NS_RELEASE(mNode);
}

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED(nsRDFEnumeratorArcsOutCursor,
                            nsRDFEnumeratorArcsCursor,
                            nsIRDFArcsOutCursor);

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS_INHERITED(nsRDFEnumeratorArcsInCursor,
                            nsRDFEnumeratorArcsCursor,
                            nsIRDFArcsInCursor);

////////////////////////////////////////////////////////////////////////////////

