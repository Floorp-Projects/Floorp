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

#include "nsCOMPtr.h"
#include "nsIComponentManager.h"
#include "nsIEnumerator.h"
#include "nsIRDFCompositeDataSource.h"
#include "nsIRDFCursor.h"
#include "nsIRDFNode.h"
#include "nsIRDFObserver.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"
#include "rdf.h"

#ifdef NS_DEBUG
#include "prlog.h"
#include "prprf.h"
#include <stdio.h>
PRLogModuleInfo* nsRDFLog = nsnull;
#endif

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
public:
    CompositeDataSourceImpl(void);
    CompositeDataSourceImpl(char** dataSources);

    nsVoidArray mDataSources;

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIRDFDataSource interface
    NS_IMETHOD Init(const char* uri);

    NS_IMETHOD GetURI(char* *uri);

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

    NS_IMETHOD GetAllResources(nsIRDFResourceCursor** aCursor);

    NS_IMETHOD Flush();

    NS_IMETHOD GetAllCommands(nsIRDFResource* source,
                              nsIEnumerator/*<nsIRDFResource>*/** commands);

    NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                nsIRDFResource*   aCommand,
                                nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                PRBool* aResult);

    NS_IMETHOD DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                         nsIRDFResource*   aCommand,
                         nsISupportsArray/*<nsIRDFResource>*/* aArguments);

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
protected:
    nsVoidArray*  mObservers;
        
    virtual ~CompositeDataSourceImpl(void);
};



class DBArcsInOutCursor : public nsIRDFArcsOutCursor,
                          public nsIRDFArcsInCursor
{
public:
    DBArcsInOutCursor(CompositeDataSourceImpl* db, nsIRDFNode* node, PRBool arcsOutp);

    virtual ~DBArcsInOutCursor();
    
    NS_DECL_ISUPPORTS

    NS_IMETHOD Advance();

    NS_IMETHOD GetDataSource(nsIRDFDataSource** aDataSource) { 
        return (mInCursor ? mInCursor->GetDataSource(aDataSource) : 
                mOutCursor->GetDataSource(aDataSource));
    }

    NS_IMETHOD GetSource(nsIRDFResource** aResource) {
        return mOutCursor->GetSource(aResource);
    }

    NS_IMETHOD GetTarget(nsIRDFNode** aNode) {
        return mInCursor->GetTarget(aNode);
    }

    NS_IMETHOD GetLabel(nsIRDFResource** aPredicate) {     
        return (mInCursor ? mInCursor->GetLabel(aPredicate) : 
                mOutCursor->GetLabel(aPredicate));
    }

    NS_IMETHOD GetValue(nsIRDFNode** aValue) {
        return (mInCursor ? mInCursor->GetValue(aValue) : 
                mOutCursor->GetValue(aValue));
    }

protected:
    CompositeDataSourceImpl* mCompositeDataSourceImpl;
    nsIRDFResource*      mSource;
    nsIRDFNode*          mTarget;
    PRInt32              mCount;
    nsIRDFArcsOutCursor* mOutCursor;
    nsIRDFArcsInCursor*  mInCursor;
    nsVoidArray          mResults;
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
	NS_INIT_REFCNT();
    NS_ADDREF(mCompositeDataSourceImpl);

    if (arcsOutp) {
        mSource = (nsIRDFResource*) node;
    } else {
        mTarget = node;
    }
    NS_IF_ADDREF(node); 

    // XXX there better be at least _one_ datasource in this here
    // CompositeDataSourceImpl, else this'll be a real short ride...
//    PR_ASSERT(db->mDataSources.Count() > 0);
    // but if there's not (because some datasource failed to initialize)
    // then just skip this...
    if (db->mDataSources.Count() > 0) {
        nsIRDFDataSource* ds = (nsIRDFDataSource*) db->mDataSources[mCount++];

        if (mTarget) {
            ds->ArcLabelsIn(mTarget,  &mInCursor);
        } else {
            ds->ArcLabelsOut(mSource,  &mOutCursor);
        }
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
    NS_IF_RELEASE(mInCursor);
    NS_IF_RELEASE(mOutCursor);
    NS_RELEASE(mCompositeDataSourceImpl);
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
        NS_ADDREF(this);
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
        
        while (NS_SUCCEEDED(result) && (result != NS_RDF_CURSOR_EMPTY)) {
            nsIRDFNode* obj ;
            result = GetValue(&obj);
            NS_ASSERTION(NS_SUCCEEDED(result), "Advance is broken");
            if (NS_SUCCEEDED(result) && mResults.IndexOf(obj) < 0) {
                mResults.AppendElement(obj);
                return NS_OK;
            }
            result = (mInCursor ? mInCursor->Advance() : mOutCursor->Advance());        
        }

        if (NS_FAILED(result))
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
    return NS_RDF_CURSOR_EMPTY;
}

////////////////////////////////////////////////////////////////////////
// DBAssertionCursor
//
//   An assertion cursor implementation for the db.
//
class DBGetSTCursor : public nsIRDFAssertionCursor
{
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

    NS_IMETHOD GetSource(nsIRDFResource** aResource) {
        return mCurrentCursor->GetSource(aResource);
    }

    NS_IMETHOD GetLabel(nsIRDFResource** aPredicate) {     
        return mCurrentCursor->GetLabel(aPredicate);
    }

    NS_IMETHOD GetTarget(nsIRDFNode** aObject) {
        nsresult rv = mCurrentCursor->GetTarget(aObject);
#ifdef NS_DEBUG
        if (NS_SUCCEEDED(rv)) {
            Trace(mSource ? "GetTargets" : "GetSources", *aObject);
        }
#endif
        return rv;
    }

    NS_IMETHOD GetTruthValue(PRBool* aTruthValue) {
        return mCurrentCursor->GetTruthValue(aTruthValue);
    }

    NS_IMETHOD GetValue(nsIRDFNode** aValue) {
        nsresult rv = mCurrentCursor->GetValue(aValue);
#ifdef NS_DEBUG
        if (NS_SUCCEEDED(rv)) {
            Trace(mSource ? "GetTargets" : "GetSources", *aValue);
        }
#endif
        return rv;
    }

#ifdef NS_DEBUG
    void Trace(const char* msg, nsIRDFNode* valueNode) {
        if (PR_LOG_TEST(nsRDFLog, PR_LOG_ALWAYS)) {
            nsresult rv;
            nsIRDFResource* subRes;
            nsIRDFResource* predRes;
            nsIRDFResource* valRes;
            nsXPIDLCString dsName;
            nsXPIDLCString subject;
            nsXPIDLCString predicate;
            nsXPIDLCString value;
            char* valueStr;
            nsIRDFDataSource* ds;

            rv = GetDataSource(&ds);
            if (NS_FAILED(rv)) return;
            rv = ds->GetURI(getter_Copies(dsName));
            if (NS_FAILED(rv)) return;
            rv = GetSource(&subRes);
            if (NS_FAILED(rv)) return;
            rv = subRes->GetValue(getter_Copies(subject));
            if (NS_FAILED(rv)) return;
            rv = GetLabel(&predRes);
            if (NS_FAILED(rv)) return;
            rv = predRes->GetValue(getter_Copies(predicate));
            if (NS_FAILED(rv)) return;
            if (NS_SUCCEEDED(valueNode->QueryInterface(nsIRDFResource::GetIID(), (void**)&valRes))) {
                rv = valRes->GetValue(getter_Copies(value));
                if (NS_FAILED(rv)) return;
                NS_RELEASE(valRes);
                valueStr = PR_smprintf("%s", (const char*) value);   // freed below
            }
            else {
                valueStr = PR_smprintf("<nsIRDFNode 0x%x>", valueNode);
            }
            if (valueStr == nsnull) return;
            printf("RDF %s: datasource=%s\n  subject: %s\n     pred: %s\n    value: %s\n",
                   msg, (const char*) dsName, (const char*) subject, (const char*) predicate, valueStr);
            NS_RELEASE(predRes);
            NS_RELEASE(subRes);
            PR_smprintf_free(valueStr);
        }
    }
#endif
private:
    CompositeDataSourceImpl* mCompositeDataSourceImpl;
    nsIRDFResource* mSource;
    nsIRDFResource* mLabel;    
    nsIRDFNode*     mTarget;
    PRInt32         mCount;
    PRBool          mTruthValue;
    nsIRDFAssertionCursor* mCurrentCursor;
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
      mTruthValue(tv),
      mCurrentCursor(nsnull)
{
	NS_INIT_REFCNT();
    NS_ADDREF(mCompositeDataSourceImpl);

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
    NS_IF_RELEASE(mCurrentCursor);
    NS_IF_RELEASE(mLabel);
    NS_IF_RELEASE(mSource);
    NS_IF_RELEASE(mTarget);
    NS_RELEASE(mCompositeDataSourceImpl);
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
        NS_ADDREF(this);
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
        if (NS_FAILED(result)) return result;

        while (NS_RDF_CURSOR_EMPTY != result) {
            nsCOMPtr<nsIRDFResource> src;
            nsCOMPtr<nsIRDFNode>     trg;            
            mCurrentCursor->GetSource(getter_AddRefs(src));
            mCurrentCursor->GetTarget(getter_AddRefs(trg));
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

        NS_RELEASE(mCurrentCursor);

        if (mSource)
            ds->GetTargets(mSource, mLabel, mTruthValue, &mCurrentCursor);
        else 
            ds->GetSources(mLabel, mTarget, mTruthValue, &mCurrentCursor);
    }
    return NS_RDF_CURSOR_EMPTY;
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
#ifdef NS_DEBUG
    if (nsRDFLog == nsnull) {
        nsRDFLog = PR_NewLogModule("RDF");
    }
#endif
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
    NS_NOTREACHED("CompositeDataSourceImpl::Init");
    return NS_ERROR_UNEXPECTED; // XXX CompositeDataSourceImpl doesn't have a URI?
}

NS_IMETHODIMP
CompositeDataSourceImpl::GetURI(char* *uri)
{
    NS_NOTREACHED("CompositeDataSourceImpl::GetURI");
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

        nsresult rv;
        rv = ds->GetSource(property, target, tv, source);
        if (NS_FAILED(rv)) return rv;

        if (rv == NS_RDF_NO_VALUE)
            continue;

        // okay, found it. make sure we don't have the opposite
        // asserted in a more local data source
        if (!HasAssertionN(count-1, *source, property, target, !tv)) 
            return NS_OK;

        NS_RELEASE(*source);
        return NS_RDF_NO_VALUE;
    }
    return NS_RDF_NO_VALUE;
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

        nsresult rv;
        rv = ds->GetTarget(source, property, tv, target);
        if (NS_FAILED(rv))
            return rv;

        if (rv == NS_RDF_NO_VALUE)
            continue;

        // okay, found it. make sure we don't have the opposite
        // asserted in the "local" data source
        if (!HasAssertionN(count-1, source, property, *target, !tv)) 
            return NS_OK;

        NS_RELEASE(*target);
        return NS_RDF_NO_VALUE;
    }

    return NS_RDF_NO_VALUE;
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
CompositeDataSourceImpl::Assert(nsIRDFResource* aSource, 
                                nsIRDFResource* aProperty, 
                                nsIRDFNode* aTarget,
                                PRBool aTruthValue)
{
    nsresult rv;

    // XXX Need to add back the stuff for unblocking ...

    // We iterate backwards from the last data source which was added
    // ("the most remote") to the first ("the most local"), trying to
    // apply the assertion in each.
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        rv = ds->Assert(aSource, aProperty, aTarget, aTruthValue);
        if (NS_RDF_ASSERTION_ACCEPTED == rv)
            return rv;

        if (NS_FAILED(rv))
            return rv;
    }

    // nobody wanted to accept it
    return NS_RDF_ASSERTION_REJECTED;
}

NS_IMETHODIMP
CompositeDataSourceImpl::Unassert(nsIRDFResource* aSource,
                                  nsIRDFResource* aProperty,
                                  nsIRDFNode* aTarget)
{
    nsresult rv;

    // Iterate through each of the datasources, starting with "the
    // most local" and moving to "the most remote". If _any_ of the
    // datasources have the assertion, attempt to unassert it.
    PRBool unasserted = PR_TRUE;
    PRInt32 i;
    PRInt32 count = mDataSources.Count();
    for (i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);

        PRBool hasAssertion;
        rv = ds->HasAssertion(aSource, aProperty, aTarget, PR_TRUE, &hasAssertion);
        if (NS_FAILED(rv)) return rv;

        if (hasAssertion) {
            rv = ds->Unassert(aSource, aProperty, aTarget);
            if (NS_FAILED(rv)) return rv;

            if (rv != NS_RDF_ASSERTION_ACCEPTED) {
                unasserted = PR_FALSE;
                break;
            }
        }
    }

    // Either none of the datasources had it, or they were all willing
    // to let it be unasserted.
    if (unasserted)
        return NS_RDF_ASSERTION_ACCEPTED;

    // If we get here, one of the datasources already had the
    // assertion, and was adamant about not letting us remove
    // it. Iterate from the "most local" to the "most remote"
    // attempting to assert the negation...
    for (i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        rv = ds->Assert(aSource, aProperty, aTarget, PR_FALSE);
        if (NS_FAILED(rv)) return rv;

        // Did it take?
        if (rv == NS_RDF_ASSERTION_ACCEPTED)
            return rv;
    }

    // Couln't get anyone to accept the negation, either.
    return NS_RDF_ASSERTION_REJECTED;
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
    *hasAssertion = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::AddObserver(nsIRDFObserver* obs)
{
    if (!mObservers) {
        if ((mObservers = new nsVoidArray()) == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    // XXX ensure uniqueness?

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
        return NS_ERROR_OUT_OF_MEMORY;

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
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(result);
    *labels = result;
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::GetAllResources(nsIRDFResourceCursor** aCursor)
{
    NS_NOTYETIMPLEMENTED("write me!");
    return NS_ERROR_NOT_IMPLEMENTED;
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

#if 0
NS_IMETHODIMP
CompositeDataSourceImpl::GetEnabledCommands(nsISupportsArray* aSources,
                                            nsISupportsArray* aArguments,
                                            nsIEnumerator**   aResult)
{
    nsCOMPtr<nsIEnumerator> commands;        // union of enabled commands
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        nsIEnumerator* dsCmds;
        nsresult rv = ds->GetEnabledCommands(aSources, aArguments, &dsCmds);
        if (NS_SUCCEEDED(rv)) {
            if (commands == nsnull) {
                commands = dont_QueryInterface(dsCmds);
            }
            else {
                nsIEnumerator* unionCmds;
                rv = NS_NewUnionEnumerator(commands, dsCmds, &unionCmds);
                if (NS_FAILED(rv)) return rv;
                NS_RELEASE(dsCmds);
                commands = dont_QueryInterface(unionCmds);
            }
        }
    }
    *aResult = commands;
    return NS_OK;
}
#endif

NS_IMETHODIMP
CompositeDataSourceImpl::GetAllCommands(nsIRDFResource* source,
                                        nsIEnumerator/*<nsIRDFResource>*/** result)
{
    nsCOMPtr<nsIEnumerator> commands;        // union of enabled commands
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        nsIEnumerator* dsCmds;
        nsresult rv = ds->GetAllCommands(source, &dsCmds);
        if (NS_SUCCEEDED(rv)) {
            if (commands == nsnull) {
                commands = dont_QueryInterface(dsCmds);
            }
            else {
                nsIEnumerator* unionCmds;
                rv = NS_NewUnionEnumerator(commands, dsCmds, &unionCmds);
                if (NS_FAILED(rv)) return rv;
                NS_RELEASE(dsCmds);
                commands = dont_QueryInterface(unionCmds);
            }
        }
    }
    *result = commands;
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                          nsIRDFResource*   aCommand,
                                          nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                          PRBool* aResult)
{
    nsresult rv;
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);

        PRBool enabled;
        rv = ds->IsCommandEnabled(aSources, aCommand, aArguments, &enabled);
        if (NS_FAILED(rv)) return rv;

        if (! enabled) {
            *aResult = PR_FALSE;
            return NS_OK;
        }
    }
    *aResult = PR_TRUE;
    return NS_OK;
}

NS_IMETHODIMP
CompositeDataSourceImpl::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                   nsIRDFResource*   aCommand,
                                   nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        nsresult rv = ds->DoCommand(aSources, aCommand, aArguments);
        if (NS_FAILED(rv)) return rv;   // all datasources must succeed
    }
    return NS_OK;
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
    NS_ASSERTION(source != nsnull, "null ptr");
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
    NS_ASSERTION(source != nsnull, "null ptr");
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



