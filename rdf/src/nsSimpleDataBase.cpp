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

#include "nsSimpleDataBase.h"
#include "nsIRDFCursor.h"
#include "nsIRDFNode.h"
#include "nsISupportsArray.h"
#include "nsRDFCID.h"
#include "nsRepository.h"
#include "prlog.h"

/*

  XXX rvg --- chris, are you happy with this (I rewrote it).

  A simple "database" implementation. An RDF database is just a
  "strategy" pattern for combining individual data sources into a
  collective graph.


  1) A database is a sequence of data sources. The set of data sources
     can be specified during creation of the database. Data sources
     can also be added/deleted from a database later.

  2) The aggregation mechanism is based on simple super-positioning of
     the graphs from the datasources. If there is a conflict (i.e., 
     data source A has a true arc from foo to bar while data source B
     has a false arc from foo to bar), the data source that it earlier
     in the sequence wins.

     The implementation below doesn't really do this and needs to be
     fixed.

*/

static NS_DEFINE_IID(kIRDFCursorIID,     NS_IRDFCURSOR_IID);
static NS_DEFINE_IID(kIRDFDataBaseIID,   NS_IRDFDATABASE_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID, NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kISupportsIID,      NS_ISUPPORTS_IID);

static NS_DEFINE_CID(kRDFBookmarkDataSourceCID, NS_RDFBOOKMARKDATASOURCE_CID);

////////////////////////////////////////////////////////////////////////
// MultiCursor

class MultiCursor : public nsIRDFCursor {
private:
    nsIRDFDataSource* mDataSource0;
    nsIRDFDataSource** mDataSources;
    nsIRDFCursor* mCurrentCursor;
    nsIRDFNode* mNextResult;
    PRBool mNextTruthValue;
    PRInt32 mNextDataSource;
    PRInt32 mCount;

public:
    MultiCursor(nsVoidArray& dataSources);
    virtual ~MultiCursor(void);

    NS_DECL_ISUPPORTS

    NS_IMETHOD HasMoreElements(PRBool& result);
    NS_IMETHOD GetNext(nsIRDFNode*& next, PRBool& tv);

    virtual nsresult
    GetCursor(nsIRDFDataSource* ds, nsIRDFCursor*& result) = 0;

    virtual nsresult
    HasNegation(nsIRDFDataSource* ds0,
                nsIRDFNode* nodeToTest,
                PRBool tv,
                PRBool& result) = 0;
};


MultiCursor::MultiCursor(nsVoidArray& dataSources)
    : mDataSource0(nsnull),
      mDataSources(nsnull),
      mCurrentCursor(nsnull),
      mNextResult(nsnull),
      mCount(0),
      mNextDataSource(0)
{
    NS_INIT_REFCNT();

    mCount = dataSources.Count();
    mDataSources = new nsIRDFDataSource*[mCount];

    PR_ASSERT(mDataSources);
    if (! mDataSources)
        return;

    for (PRInt32 i = 0; i < mCount; ++i) {
        mDataSources[i] = NS_STATIC_CAST(nsIRDFDataSource*, dataSources[i]);
        NS_ADDREF(mDataSources[i]);
    }

    mDataSource0 = mDataSources[0];
    NS_ADDREF(mDataSource0);
}


MultiCursor::~MultiCursor(void)
{
    NS_IF_RELEASE(mNextResult);
    NS_IF_RELEASE(mCurrentCursor);
    for (PRInt32 i = mCount - 1; i >= 0; --i) {
        NS_IF_RELEASE(mDataSources[i]);
    }
    NS_IF_RELEASE(mDataSource0);
}

NS_IMPL_ISUPPORTS(MultiCursor, kIRDFCursorIID);

NS_IMETHODIMP
MultiCursor::HasMoreElements(PRBool& result)
{
    nsresult rv;
    if (! mDataSources)
        return NS_ERROR_OUT_OF_MEMORY;

    // If we've already queued up a next target, then yep, there are
    // more elements.
    if (mNextResult) {
        result = PR_TRUE;
        return NS_OK;
    }

    // Otherwise, we'll need to find a next target, switching cursors
    // if necessary.
    result = PR_FALSE;

    while (mNextDataSource < mCount) {
        if (! mCurrentCursor) {
            // We don't have a current cursor, so create a new one on
            // the next data source.
            rv = GetCursor(mDataSources[mNextDataSource], mCurrentCursor);

            if (NS_FAILED(rv))
                return rv;
        }

        do {
            if (NS_FAILED(rv = mCurrentCursor->HasMoreElements(result)))
                return rv;

            // Is the current cursor depleted?
            if (!result)
                break;

            // Even if the current cursor has more elements, we still
            // need to check that the current element isn't masked by
            // the "main" data source.
            
            // "Peek" ahead and pull out the next target.
            if (NS_FAILED(mCurrentCursor->GetNext(mNextResult, mNextTruthValue)))
                return rv;

            // See if data source zero has the negation
            // XXX rvg --- this needs to be fixed so that we look at all the prior 
            // data sources for negations
            PRBool hasNegation;
            if (NS_FAILED(rv = HasNegation(mDataSource0,
                                           mNextResult,
                                           mNextTruthValue,
                                           hasNegation)))
                return rv;

            // if not, we're done
            if (! hasNegation)
                return NS_OK;

            // Otherwise, we found the negation in data source
            // zero. Gotta keep lookin'...
            NS_RELEASE(mNextResult);
        } while (1);

        NS_RELEASE(mCurrentCursor);
        NS_RELEASE(mDataSources[mNextDataSource]);
        ++mNextDataSource;
    }

    // if we get here, there aren't any elements left.
    return NS_OK;
}


NS_IMETHODIMP
MultiCursor::GetNext(nsIRDFNode*& next, PRBool& tv)
{
    nsresult rv;
    PRBool hasMore;
    if (NS_FAILED(rv = HasMoreElements(hasMore)))
        return rv;

    if (! hasMore)
        return NS_ERROR_UNEXPECTED;

    next = mNextResult; // no need to AddRef() again...
    tv   = mNextTruthValue;
    
    mNextResult = nsnull; // ...because we'll just "transfer ownership".
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
// dbNodeCursorImpl

class dbNodeCursorImpl : public MultiCursor {
private:
    nsIRDFNode* mSource;
    nsIRDFNode* mProperty;
    PRBool mTruthValue;

public:
    dbNodeCursorImpl(nsVoidArray& dataSources,
                   nsIRDFNode* source,
                   nsIRDFNode* property,
                   PRBool tv);

    virtual ~dbNodeCursorImpl();

    virtual nsresult
    GetCursor(nsIRDFDataSource* ds, nsIRDFCursor*& result);

    virtual nsresult
    HasNegation(nsIRDFDataSource* ds0,
                nsIRDFNode* nodeToTest,
                PRBool tv,
                PRBool& result);
};

dbNodeCursorImpl::dbNodeCursorImpl(nsVoidArray& dataSources,
                               nsIRDFNode* source,
                               nsIRDFNode* property,
                               PRBool tv)
    : MultiCursor(dataSources),
      mSource(source),
      mProperty(property),
      mTruthValue(tv)
{
    NS_IF_ADDREF(mSource);
    NS_IF_ADDREF(mProperty);
}


dbNodeCursorImpl::~dbNodeCursorImpl(void)
{
    NS_IF_RELEASE(mProperty);
    NS_IF_RELEASE(mSource);
}

nsresult
dbNodeCursorImpl::GetCursor(nsIRDFDataSource* ds, nsIRDFCursor*& result)
{
    return ds->GetTargets(mSource, mProperty, mTruthValue, result);
}

nsresult
dbNodeCursorImpl::HasNegation(nsIRDFDataSource* ds0,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool& result)
{
    return ds0->HasAssertion(mSource, mProperty, target, !tv, result);
}

////////////////////////////////////////////////////////////////////////
// dbArcCursorImpl

class dbArcCursorImpl : public MultiCursor {
private:
    nsIRDFNode* mSource;

public:
    dbArcCursorImpl(nsVoidArray& dataSources, nsIRDFNode* source);

    virtual ~dbArcCursorImpl();

    virtual nsresult
    GetCursor(nsIRDFDataSource* ds, nsIRDFCursor*& result);

    virtual nsresult
    HasNegation(nsIRDFDataSource* ds0,
                nsIRDFNode* nodeToTest,
                PRBool tv,
                PRBool& result);
};

dbArcCursorImpl::dbArcCursorImpl(nsVoidArray& dataSources,
                             nsIRDFNode* source)
    : MultiCursor(dataSources),
      mSource(source)
{
    NS_IF_ADDREF(mSource);
}


dbArcCursorImpl::~dbArcCursorImpl(void)
{
    NS_IF_RELEASE(mSource);
}

nsresult
dbArcCursorImpl::GetCursor(nsIRDFDataSource* ds, nsIRDFCursor*& result)
{
    return ds->ArcLabelsOut(mSource, result);
}

nsresult
dbArcCursorImpl::HasNegation(nsIRDFDataSource* ds0,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool& result)
{
    result = PR_FALSE;
    return NS_OK;
}



////////////////////////////////////////////////////////////////////////
// nsSimpleDataBase
// XXX rvg  --- shouldn't this take a char** argument indicating the data sources
// we want to aggregate?

nsSimpleDataBase::nsSimpleDataBase(void)
{
    NS_INIT_REFCNT();

    // Add standard data sources here.
    // XXX this is so wrong.
    nsIRDFDataSource* ds;
    if (NS_SUCCEEDED(nsRepository::CreateInstance(kRDFBookmarkDataSourceCID,
                                                  nsnull,
                                                  kIRDFDataSourceIID,
                                                  (void**) &ds))) {
        AddDataSource(ds);
        NS_RELEASE(ds);
    }
}


nsSimpleDataBase::~nsSimpleDataBase(void)
{
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        NS_IF_RELEASE(ds);
    }
}

////////////////////////////////////////////////////////////////////////
// nsISupports interface

NS_IMPL_ADDREF(nsSimpleDataBase);
NS_IMPL_RELEASE(nsSimpleDataBase);

NS_IMETHODIMP
nsSimpleDataBase::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(kIRDFDataBaseIID) ||
        iid.Equals(kIRDFDataSourceIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFDataBase*, this);
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}



////////////////////////////////////////////////////////////////////////
// nsIRDFDataSource interface

NS_IMETHODIMP
nsSimpleDataBase::Initialize(const nsString& uri)
{
    PR_ASSERT(0);
    return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsSimpleDataBase::GetSource(nsIRDFNode* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            nsIRDFNode*& source)
{
    PRInt32 count = mDataSources.Count();
    for (PRInt32 i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);

        if (NS_FAILED(ds->GetSource(property, target, tv, source)))
            continue;

        // okay, found it. make sure we don't have the opposite
        // asserted in the "main" data source
        nsIRDFDataSource* ds0 = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[0]);
        nsIRDFNode* tmp;
        if (NS_FAILED(ds->GetSource(property, target, !tv, tmp)))
            return NS_OK;

        NS_RELEASE(tmp);
        NS_RELEASE(source);
        return NS_ERROR_FAILURE;
    }

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleDataBase::GetSources(nsIRDFNode* property,
                             nsIRDFNode* target,
                             PRBool tv,
                             nsIRDFCursor*& sources)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSimpleDataBase::GetTarget(nsIRDFNode* source,
                            nsIRDFNode* property,
                            PRBool tv,
                            nsIRDFNode*& target)
{
    PRInt32 count = mDataSources.Count();
    for (PRInt32 i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);

        if (NS_FAILED(ds->GetTarget(source, property, tv, target)))
            continue;

        // okay, found it. make sure we don't have the opposite
        // asserted in the "main" data source
        nsIRDFDataSource* ds0 = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[0]);
        nsIRDFNode* tmp;
        if (NS_FAILED(ds0->GetTarget(source, property, !tv, tmp)))
            return NS_OK;

        NS_RELEASE(tmp);
        NS_RELEASE(target);
        return NS_ERROR_FAILURE;
    }

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleDataBase::GetTargets(nsIRDFNode* source,
                             nsIRDFNode* property,
                             PRBool tv,
                             nsIRDFCursor*& targets)
{
    targets = new dbNodeCursorImpl(mDataSources, source, property, tv);
    if (! targets)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(targets);
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleDataBase::Assert(nsIRDFNode* source, 
                         nsIRDFNode* property, 
                         nsIRDFNode* target,
                         PRBool tv)
{
    nsresult rv;

    // First see if we just need to remove a negative assertion from ds0. (Sigh)
    nsIRDFDataSource* ds0 = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[0]);

    PRBool ds0HasNegation;
    if (NS_FAILED(rv = ds0->HasAssertion(source, property, target, !tv, ds0HasNegation)))
        return rv;

    if (ds0HasNegation) {
        if (NS_FAILED(rv = ds0->Unassert(source, property, target)))
            return rv;
    }

    // Now, see if the assertion has been "unmasked"
    PRBool isAlreadyAsserted;
    if (NS_FAILED(rv = HasAssertion(source, property, target, tv, isAlreadyAsserted)))
        return rv;

    if (isAlreadyAsserted)
        return NS_OK;

    // If not, iterate from the "remote-est" data source to the
    // "local-est", trying to make the assertion.
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        if (NS_SUCCEEDED(ds->Assert(source, property, target, tv)))
            return NS_OK;
    }

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsSimpleDataBase::Unassert(nsIRDFNode* source,
                           nsIRDFNode* property,
                           nsIRDFNode* target)
{
    // XXX I have no idea what this is trying to do. I'm just going to
    // copy Guha's logic and punt.
    // xxx rvg - first need to check whether the data source does have the
    // assertion. Only then do you try to unassert it.
    nsresult rv;
    PRInt32 count = mDataSources.Count();

    for (PRInt32 i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        if (NS_FAILED(rv = ds->Unassert(source, property, target)))
            break;
    }

    if (NS_FAILED(rv)) {
        nsIRDFDataSource* ds0 = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[0]);
        rv = ds0->Assert(source, property, target);
    }
    return rv;
}

NS_IMETHODIMP
nsSimpleDataBase::HasAssertion(nsIRDFNode* source,
                               nsIRDFNode* property,
                               nsIRDFNode* target,
                               PRBool tv,
                               PRBool& hasAssertion)
{
    nsresult rv;

    // First check to see if ds0 has the negation...
    nsIRDFDataSource* ds0 = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[0]);

    PRBool ds0HasNegation;
    if (NS_FAILED(rv = ds0->HasAssertion(source, property, target, !tv, ds0HasNegation)))
        return rv;

    if (ds0HasNegation) {
        hasAssertion = PR_FALSE;
        return NS_OK;
    }

    // Otherwise, look through all the data sources to see if anyone
    // has the positive...
    PRInt32 count = mDataSources.Count();
    for (PRInt32 i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        if (NS_FAILED(rv = ds->HasAssertion(source, property, target, tv, hasAssertion)))
            return rv;

        if (hasAssertion)
            return NS_OK;
    }

    // If we get here, nobody had the assertion at all
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleDataBase::AddObserver(nsIRDFObserver* n)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSimpleDataBase::RemoveObserver(nsIRDFObserver* n)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSimpleDataBase::ArcLabelsIn(nsIRDFNode* node,
                              nsIRDFCursor*& labels)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsSimpleDataBase::ArcLabelsOut(nsIRDFNode* source,
                               nsIRDFCursor*& labels)
{
    labels = new dbArcCursorImpl(mDataSources, source);
    if (! labels)
        return NS_ERROR_NULL_POINTER;

    NS_ADDREF(labels);
    return NS_OK;
}

NS_IMETHODIMP
nsSimpleDataBase::Flush()
{
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        ds->Flush();
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIRDFDataBase methods
// XXX rvg We should make this take an additional argument specifying where
// in the sequence of data sources (of the db), the new data source should
// fit in. Right now, the new datasource gets stuck at the end.

NS_IMETHODIMP
nsSimpleDataBase::AddDataSource(nsIRDFDataSource* source)
{
    if (! source)
        return NS_ERROR_NULL_POINTER;

    mDataSources.InsertElementAt(source, 0);
    NS_ADDREF(source);
    return NS_OK;
}



NS_IMETHODIMP
nsSimpleDataBase::RemoveDataSource(nsIRDFDataSource* source)
{
    if (! source)
        return NS_ERROR_NULL_POINTER;

    if (mDataSources.IndexOf(source) >= 0) {
        mDataSources.RemoveElement(source);
        NS_RELEASE(source);
    }
    return NS_OK;
}
