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

/*

  A simple composite data source implementation. A composit data
  source is just a strategy for combining individual data sources into
  a collective graph.


  1) A composite data source holds a sequence of data sources. The set
     of data sources can be specified during creation of the
     database. Data sources can also be added/deleted from a database
     later.

  2) The aggregation mechanism is based on simple super-positioning of
     the graphs from the datasources. If there is a conflict (i.e., 
     data source A has a true arc from foo to bar while data source B
     has a false arc from foo to bar), the data source that it earlier
     in the sequence wins.

     The implementation below doesn't really do this and needs to be
     fixed.

*/

#include "nsIRDFCursor.h"
#include "nsIRDFNode.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFObserver.h"
#include "nsRepository.h"
#include "nsVoidArray.h"
#include "prlog.h"

static NS_DEFINE_IID(kIRDFArcsInCursorIID,    NS_IRDFARCSINCURSOR_IID);
static NS_DEFINE_IID(kIRDFArcsOutCursorIID,   NS_IRDFARCSOUTCURSOR_IID);
static NS_DEFINE_IID(kIRDFAssertionCursorIID, NS_IRDFASSERTIONCURSOR_IID);
static NS_DEFINE_IID(kIRDFCursorIID,          NS_IRDFCURSOR_IID);
static NS_DEFINE_IID(kIRDFCompositeDataSourceIID, NS_IRDFCOMPOSITEDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,      NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kIRDFObserverIID,        NS_IRDFOBSERVER_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

////////////////////////////////////////////////////////////////////////
// CompositeDataSourceImpl

class CompositeDataSourceImpl : public nsIRDFCompositeDataSource,
                 public nsIRDFObserver
{
protected:
    nsVoidArray*  mObservers;
        
    virtual ~CompositeDataSourceImpl(void);

public:
    CompositeDataSourceImpl(void);
    CompositeDataSourceImpl(char** dataSources);

    nsVoidArray mDataSources;

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFDataSource interface
    NS_IMETHOD Init(const char* uri);

    NS_IMETHOD GetURI(const char* *uri) const;

    NS_IMETHOD GetSource(nsIRDFResource* property,
                         nsIRDFNode* target,
                         PRBool tv,
                         nsIRDFResource** source);

    NS_IMETHOD GetSources(nsIRDFResource* property,
                          nsIRDFNode* target,
                          PRBool tv,
                          nsIRDFAssertionCursor** sources);

    NS_IMETHOD GetTarget(nsIRDFResource* source,
                         nsIRDFResource* property,
                         PRBool tv,
                         nsIRDFNode** target);

    NS_IMETHOD GetTargets(nsIRDFResource* source,
                          nsIRDFResource* property,
                          PRBool tv,
                          nsIRDFAssertionCursor** targets);

    NS_IMETHOD Assert(nsIRDFResource* source, 
                      nsIRDFResource* property, 
                      nsIRDFNode* target,
                      PRBool tv);

    NS_IMETHOD Unassert(nsIRDFResource* source,
                        nsIRDFResource* property,
                        nsIRDFNode* target);

    NS_IMETHOD HasAssertion(nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv,
                            PRBool* hasAssertion);

    NS_IMETHOD AddObserver(nsIRDFObserver* n);

    NS_IMETHOD RemoveObserver(nsIRDFObserver* n);

    NS_IMETHOD ArcLabelsIn(nsIRDFNode* node,
                           nsIRDFArcsInCursor** labels);

    NS_IMETHOD ArcLabelsOut(nsIRDFResource* source,
                            nsIRDFArcsOutCursor** labels);

    NS_IMETHOD Flush();

    NS_IMETHOD IsCommandEnabled(const char* aCommand,
                                nsIRDFResource* aCommandTarget,
                                PRBool* aResult);

    NS_IMETHOD DoCommand(const char* aCommand,
                         nsIRDFResource* aCommandTarget);


    // nsIRDFCompositeDataSource interface
    NS_IMETHOD AddDataSource(nsIRDFDataSource* source);
    NS_IMETHOD RemoveDataSource(nsIRDFDataSource* source);

    // nsIRDFObserver interface
    NS_IMETHOD OnAssert(nsIRDFResource* subject,
                        nsIRDFResource* predicate,
                        nsIRDFNode* object);

    NS_IMETHOD OnUnassert(nsIRDFResource* subject,
                          nsIRDFResource* predicate,
                          nsIRDFNode* object);

    // Implementation methods
    PRBool HasAssertionN(int n, nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv);
};



class DBArcsInOutCursor : public nsIRDFArcsOutCursor,
                          public nsIRDFArcsInCursor
{
    CompositeDataSourceImpl* mCompositeDataSourceImpl;
    nsIRDFResource*      mSource;
    nsIRDFNode*          mTarget;
    PRInt32              mCount;
    nsIRDFArcsOutCursor* mOutCursor;
    nsIRDFArcsInCursor*  mInCursor;
    nsVoidArray          mResults;

public:
    DBArcsInOutCursor(CompositeDataSourceImpl* db, nsIRDFNode* node, PRBool arcsOutp);

    virtual ~DBArcsInOutCursor();
    
    NS_DECL_ISUPPORTS

    NS_IMETHOD Advance();

    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource) { 
        return (mInCursor ? mInCursor->GetDataSource(aDataSource) : 
                mOutCursor->GetDataSource(aDataSource));
    }

    NS_IMETHOD GetSubject(nsIRDFResource** aResource) {
        return mOutCursor->GetSubject(aResource);
    }

    NS_IMETHOD GetObject(nsIRDFNode** aNode) {
        return mInCursor->GetObject(aNode);
    }

    NS_IMETHOD GetPredicate(nsIRDFResource** aPredicate) {     
        return (mInCursor ? mInCursor->GetPredicate(aPredicate) : 
                mOutCursor->GetPredicate(aPredicate));
    }

    NS_IMETHOD GetValue(nsIRDFNode** aValue) {
        return (mInCursor ? mInCursor->GetValue(aValue) : 
                mOutCursor->GetValue(aValue));
    }


};

        
DBArcsInOutCursor::DBArcsInOutCursor(CompositeDataSourceImpl* db,
                                     nsIRDFNode* node,
                                     PRBool arcsOutp)
    : mCompositeDataSourceImpl(db), 
	  mTarget(0),
	  mSource(0),
	  mCount(0),
	  mInCursor(0),
	  mOutCursor(0)
{
    if (arcsOutp) {
        mSource = (nsIRDFResource*) node;
    } else {
        mTarget = node;
    }
    NS_IF_ADDREF(node); 

    // XXX there better be at least _one_ datasource in this here
    // CompositeDataSourceImpl, else this'll be a real short ride...
    PR_ASSERT(db->mDataSources.Count() > 0);
    nsIRDFDataSource* ds = (nsIRDFDataSource*) db->mDataSources[mCount++];

    if (mTarget) {
        ds->ArcLabelsIn(mTarget,  &mInCursor);
    } else {
        ds->ArcLabelsOut(mSource,  &mOutCursor);
    }
}


DBArcsInOutCursor::~DBArcsInOutCursor(void)
{
    for (PRInt32 i = mResults.Count() - 1; i >= 0; --i) {
        nsIRDFNode* node = (nsIRDFNode*) mResults[i];
        NS_RELEASE(node);
    }

    NS_IF_RELEASE(mSource);
    NS_IF_RELEASE(mTarget);
}


NS_IMPL_ADDREF(DBArcsInOutCursor);
NS_IMPL_RELEASE(DBArcsInOutCursor);

NS_IMETHODIMP_(nsresult)
DBArcsInOutCursor::QueryInterface(REFNSIID iid, void** result) {
    if (! result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kIRDFArcsOutCursorIID) ||
        iid.Equals(kIRDFCursorIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFArcsOutCursor*, this);
        /* AddRef(); // not necessary */
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP
DBArcsInOutCursor::Advance(void)
{
    nsIRDFDataSource* ds;
    while (mInCursor || mOutCursor) {
        nsresult result = (mInCursor ? mInCursor->Advance() : mOutCursor->Advance());
        
        while (NS_SUCCEEDED(result)) {
            nsIRDFNode* obj ;
            GetValue(&obj);
            if (mResults.IndexOf(obj) < 0) {
                mResults.AppendElement(obj);
                return NS_OK;
            }
            result = (mInCursor ? mInCursor->Advance() : mOutCursor->Advance());        
        }

        if (result != NS_ERROR_RDF_CURSOR_EMPTY)
            return result;

        NS_IF_RELEASE(mInCursor);
        NS_IF_RELEASE(mOutCursor);

        if (mCount >= mCompositeDataSourceImpl->mDataSources.Count())
            break;

        ds = (nsIRDFDataSource*) mCompositeDataSourceImpl->mDataSources[mCount];
        ++mCount;

        if (mTarget) {
            ds->ArcLabelsIn(mTarget, &mInCursor);
        } else {
            ds->ArcLabelsOut(mSource, &mOutCursor);
        }
    }
    return NS_ERROR_RDF_CURSOR_EMPTY;
}

////////////////////////////////////////////////////////////////////////
// DBAssertionCursor
//
//   An assertion cursor implementation for the db.
//
class DBGetSTCursor : public nsIRDFAssertionCursor
{
private:
    CompositeDataSourceImpl* mCompositeDataSourceImpl;
    nsIRDFResource* mSource;
    nsIRDFResource* mLabel;    
    nsIRDFNode*     mTarget;
    PRInt32         mCount;
    PRBool          mTruthValue;
    nsIRDFAssertionCursor* mCurrentCursor;

public:
    DBGetSTCursor(CompositeDataSourceImpl* db, nsIRDFNode* u,  
                       nsIRDFResource* property, PRBool inversep, PRBool tv);

    virtual ~DBGetSTCursor();

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFAssertionCursor interface
    NS_IMETHOD Advance();

    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource) { 
        return mCurrentCursor->GetDataSource(aDataSource);
    }

    NS_IMETHOD GetSubject(nsIRDFResource** aResource) {
        return mCurrentCursor->GetSubject(aResource);
    }

    NS_IMETHOD GetPredicate(nsIRDFResource** aPredicate) {     
        return mCurrentCursor->GetPredicate(aPredicate);
    }

    NS_IMETHOD GetObject(nsIRDFNode** aObject) {
        return mCurrentCursor->GetObject(aObject);
    }

    NS_IMETHOD GetTruthValue(PRBool* aTruthValue) {
        return mCurrentCursor->GetTruthValue(aTruthValue);
    }

    NS_IMETHOD GetValue(nsIRDFNode** aValue) {
        return mCurrentCursor->GetValue(aValue);
    }

};

//NS_IMPL_ISUPPORTS(DBGetSTCursor, kIRDFAssertionCursorIID);        

DBGetSTCursor::DBGetSTCursor(CompositeDataSourceImpl* db,
                             nsIRDFNode* u,
                             nsIRDFResource* property,
                             PRBool inversep, 
                             PRBool tv)
    : mCompositeDataSourceImpl(db),
      mSource(nsnull),
      mLabel(property),
      mTarget(nsnull),
      mCount(0),
      mTruthValue(tv)
{
    if (!inversep) {
        mSource = (nsIRDFResource*) u;
    } else {
        mTarget = u;
    }

    NS_IF_ADDREF(mSource);
    NS_IF_ADDREF(mTarget);
    NS_IF_ADDREF(mLabel);

    // XXX assume that at least one data source exists in the CompositeDataSourceImpl.
    nsIRDFDataSource* ds = (nsIRDFDataSource*) db->mDataSources[mCount++];
    if (mSource)
        ds->GetTargets(mSource, mLabel, mTruthValue, &mCurrentCursor);
    else 
        ds->GetSources(mLabel, mTarget,  mTruthValue, &mCurrentCursor);
}


DBGetSTCursor::~DBGetSTCursor(void)
{
    NS_IF_RELEASE(mLabel);
    NS_IF_RELEASE(mSource);
    NS_IF_ADDREF(mTarget);
}


NS_IMPL_ADDREF(DBGetSTCursor);
NS_IMPL_RELEASE(DBGetSTCursor);

NS_IMETHODIMP_(nsresult)
DBGetSTCursor::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kIRDFAssertionCursorIID) ||
        iid.Equals(kIRDFCursorIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFAssertionCursor*, this);
        /* AddRef(); // not necessary */
        return NS_OK;
    }
    return NS_NOINTERFACE;
}

NS_IMETHODIMP
DBGetSTCursor::Advance(void)
{
    nsIRDFDataSource* ds;
    while (mCurrentCursor) {
        nsresult result = mCurrentCursor->Advance();
        while (NS_ERROR_RDF_CURSOR_EMPTY != result) {
            nsIRDFResource* src;
            nsIRDFNode*     trg;            
            mCurrentCursor->GetSubject(&src);
            mCurrentCursor->GetObject(&trg);
            if (!mCompositeDataSourceImpl->HasAssertionN(mCount-1, src, mLabel, trg, !mTruthValue)) {
                return NS_OK;
            } else {
                result = mCurrentCursor->Advance();
            }            
        }

        if (mCount >= mCompositeDataSourceImpl->mDataSources.Count())
            break;

        ds = (nsIRDFDataSource*) mCompositeDataSourceImpl->mDataSources[mCount];
        ++mCount;

        if (mSource)
            ds->GetTargets(mSource, mLabel, mTruthValue, &mCurrentCursor);
        else 
            ds->GetSources(mLabel, mTarget, mTruthValue, &mCurrentCursor);
    }
    return NS_ERROR_RDF_CURSOR_EMPTY;
}


////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFCompositeDataSource(nsIRDFCompositeDataSource** result)
{
    CompositeDataSourceImpl* db = new CompositeDataSourceImpl();
    if (! db)
        return NS_ERROR_OUT_OF_MEMORY;

    *result = db;
    NS_ADDREF(*result);
    return NS_OK;
}


CompositeDataSourceImpl::CompositeDataSourceImpl(void)
    : mObservers(nsnull)
{
    NS_INIT_REFCNT();
}


CompositeDataSourceImpl::~CompositeDataSourceImpl(void)
{
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        ds->RemoveObserver(this);
        NS_IF_RELEASE(ds);
    }
    delete mObservers;
}

////////////////////////////////////////////////////////////////////////
// nsISupports interface

NS_IMPL_ADDREF(CompositeDataSourceImpl);
NS_IMPL_RELEASE(CompositeDataSourceImpl);

NS_IMETHODIMP
CompositeDataSourceImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kIRDFCompositeDataSourceIID) ||
        iid.Equals(kIRDFDataSourceIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFCompositeDataSource*, this);
		NS_ADDREF(this);
        return NS_OK;
    }
    else if (iid.Equals(kIRDFObserverIID)) {
        *result = NS_STATIC_CAST(nsIRDFObserver*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else {
        *result = nsnull;
        return NS_NOINTERFACE;
    }
}



////////////////////////////////////////////////////////////////////////
// nsIRDFDataSource interface

NS_IMETHODIMP
CompositeDataSourceImpl::Init(const char* uri)
{
    PR_ASSERT(0);
    return NS_ERROR_UNEXPECTED; // XXX CompositeDataSourceImpl doesn't have a URI?
}

NS_IMETHODIMP
CompositeDataSourceImpl::GetURI(const char* *uri) const
{
    PR_ASSERT(0);
    return NS_ERROR_UNEXPECTED; // XXX CompositeDataSourceImpl doesn't have a URI?
}

NS_IMETHODIMP
CompositeDataSourceImpl::GetSource(nsIRDFResource* property,
                                   nsIRDFNode* target,
                                   PRBool tv,
                                   nsIRDFResource** source)
{
    PRInt32 count = mDataSources.Count();
    for (PRInt32 i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);

        if (NS_FAILED(ds->GetSource(property, target, tv, source)))
            continue;

        // okay, found it. make sure we don't have the opposite
        // asserted in a more local data source
        if (!HasAssertionN(count-1, *source, property, target, !tv)) 
            return NS_OK;

        NS_RELEASE(*source);
        return NS_ERROR_FAILURE;
    }
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CompositeDataSourceImpl::GetSources(nsIRDFResource* property,
                                    nsIRDFNode* target,
                                    PRBool tv,
                                    nsIRDFAssertionCursor** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = new DBGetSTCursor(this, target, property, 1, tv);
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*result);
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::GetTarget(nsIRDFResource* source,
                                   nsIRDFResource* property,
                                   PRBool tv,
                                   nsIRDFNode** target)
{
    PRInt32 count = mDataSources.Count();
    for (PRInt32 i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);

        if (NS_FAILED(ds->GetTarget(source, property, tv, target)))
            continue;

        // okay, found it. make sure we don't have the opposite
        // asserted in the "local" data source
        if (!HasAssertionN(count-1, source, property, *target, !tv)) 
            return NS_OK;


        NS_RELEASE(*target);
        return NS_ERROR_FAILURE;
    }

    return NS_ERROR_FAILURE;
}

PRBool
CompositeDataSourceImpl::HasAssertionN(int n,
                                       nsIRDFResource* source,
                                       nsIRDFResource* property,
                                       nsIRDFNode* target,
                                       PRBool tv)
{
    int m = 0;
    PRBool result = 0;
    while (m < n) {
        nsIRDFDataSource* ds = (nsIRDFDataSource*) mDataSources[m];
        ds->HasAssertion(source, property, target, tv, &result);
        if (result) return 1;
        m++;
    }
    return 0;
}
    


NS_IMETHODIMP
CompositeDataSourceImpl::GetTargets(nsIRDFResource* source,
                                    nsIRDFResource* property,
                                    PRBool tv,
                                    nsIRDFAssertionCursor** targets)
{
    if (! targets)
        return NS_ERROR_NULL_POINTER;

    nsIRDFAssertionCursor* result;
    result = new DBGetSTCursor(this, source, property, 0, tv);
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);
    *targets = result;
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::Assert(nsIRDFResource* source, 
                                nsIRDFResource* property, 
                                nsIRDFNode* target,
                                PRBool tv)
{
    // Need to add back the stuff for unblocking ...
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        if (NS_SUCCEEDED(ds->Assert(source, property, target, tv)))
            return NS_OK;
    }

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
CompositeDataSourceImpl::Unassert(nsIRDFResource* source,
                                  nsIRDFResource* property,
                                  nsIRDFNode* target)
{
    nsresult rv;
    PRInt32 count = mDataSources.Count();

    for (PRInt32 i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        if (NS_FAILED(rv = ds->Unassert(source, property, target)))
            break;
    }

    if (NS_FAILED(rv)) {
        nsIRDFDataSource* ds0 = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[0]);
        rv = ds0->Assert(source, property, target, PR_FALSE);
    }
    return rv;
}

NS_IMETHODIMP
CompositeDataSourceImpl::HasAssertion(nsIRDFResource* source,
                                      nsIRDFResource* property,
                                      nsIRDFNode* target,
                                      PRBool tv,
                                      PRBool* hasAssertion)
{
    nsresult rv;


    // Otherwise, look through all the data sources to see if anyone
    // has the positive...
    PRInt32 count = mDataSources.Count();
    PRBool hasNegation = 0;
    for (PRInt32 i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        if (NS_FAILED(rv = ds->HasAssertion(source, property, target, tv, hasAssertion)))
            return rv;

        if (*hasAssertion)
            return NS_OK;

        if (NS_FAILED(rv = ds->HasAssertion(source, property, target, !tv, &hasNegation)))
            return rv;

        if (hasNegation) {
            *hasAssertion = 0;
            return NS_OK;
        }
    }

    // If we get here, nobody had the assertion at all
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::AddObserver(nsIRDFObserver* obs)
{
    if (!mObservers) {
        if ((mObservers = new nsVoidArray()) == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    // XXX ensure uniqueness

    mObservers->AppendElement(obs);
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::RemoveObserver(nsIRDFObserver* obs)
{
    if (!mObservers)
        return NS_OK;

    mObservers->RemoveElement(obs);
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::ArcLabelsIn(nsIRDFNode* node,
                                     nsIRDFArcsInCursor** labels)
{
    if (! labels)
        return NS_ERROR_NULL_POINTER;

    nsIRDFArcsInCursor* result = new DBArcsInOutCursor(this, node, 0);
    if (! result)
        return NS_ERROR_NULL_POINTER;

    NS_ADDREF(result);
    *labels = result;
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::ArcLabelsOut(nsIRDFResource* source,
                                      nsIRDFArcsOutCursor** labels)
{
    if (! labels)
        return NS_ERROR_NULL_POINTER;

    nsIRDFArcsOutCursor* result = new DBArcsInOutCursor(this, source, 1);
    if (! result)
        return NS_ERROR_NULL_POINTER;

    NS_ADDREF(result);
    *labels = result;
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::Flush()
{
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        ds->Flush();
    }
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::IsCommandEnabled(const char* aCommand,
                                          nsIRDFResource* aCommandTarget,
                                          PRBool* aResult)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
CompositeDataSourceImpl::DoCommand(const char* aCommand,
                                   nsIRDFResource* aCommandTarget)
{
    PR_ASSERT(0);
    return NS_ERROR_NOT_IMPLEMENTED;
}

////////////////////////////////////////////////////////////////////////
// nsIRDFCompositeDataSource methods
// XXX rvg We should make this take an additional argument specifying where
// in the sequence of data sources (of the db), the new data source should
// fit in. Right now, the new datasource gets stuck at the end.
// need to add the observers of the CompositeDataSourceImpl to the new data source.

NS_IMETHODIMP
CompositeDataSourceImpl::AddDataSource(nsIRDFDataSource* source)
{
    if (! source)
        return NS_ERROR_NULL_POINTER;

    mDataSources.InsertElementAt(source, 0);
    source->AddObserver(this);
    NS_ADDREF(source);
    return NS_OK;
}



NS_IMETHODIMP
CompositeDataSourceImpl::RemoveDataSource(nsIRDFDataSource* source)
{
    if (! source)
        return NS_ERROR_NULL_POINTER;


    if (mDataSources.IndexOf(source) >= 0) {
        mDataSources.RemoveElement(source);
        source->RemoveObserver(this);
        NS_RELEASE(source);
    }
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::OnAssert(nsIRDFResource* subject,
                                  nsIRDFResource* predicate,
                                  nsIRDFNode* object)
{
    if (mObservers) {
        for (PRInt32 i = mObservers->Count() - 1; i >= 0; --i) {
            nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
            obs->OnAssert(subject, predicate, object);
            // XXX ignore return value?
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::OnUnassert(nsIRDFResource* subject,
                                    nsIRDFResource* predicate,
                                    nsIRDFNode* object)
{
    if (mObservers) {
        for (PRInt32 i = mObservers->Count() - 1; i >= 0; --i) {
            nsIRDFObserver* obs = (nsIRDFObserver*) mObservers->ElementAt(i);
            obs->OnUnassert(subject, predicate, object);
            // XXX ignore return value?
        }
    }
    return NS_OK;
}



