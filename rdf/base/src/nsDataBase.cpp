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

  A simple "database" implementation. An RDF database is just a
  strategy for combining individual data sources into a collective
  graph.


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

#include "nsIRDFCursor.h"
#include "nsIRDFNode.h"
#include "nsIRDFDataBase.h"
#include "nsISupportsArray.h"
#include "nsRepository.h"
#include "nsVoidArray.h"
#include "prlog.h"

static NS_DEFINE_IID(kIRDFArcsInCursorIID,    NS_IRDFARCSINCURSOR_IID);
static NS_DEFINE_IID(kIRDFArcsOutCursorIID,   NS_IRDFARCSOUTCURSOR_IID);
static NS_DEFINE_IID(kIRDFAssertionCursorIID, NS_IRDFASSERTIONCURSOR_IID);
static NS_DEFINE_IID(kIRDFCursorIID,          NS_IRDFCURSOR_IID);
static NS_DEFINE_IID(kIRDFDataBaseIID,        NS_IRDFDATABASE_IID);
static NS_DEFINE_IID(kIRDFDataSourceIID,      NS_IRDFDATASOURCE_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

////////////////////////////////////////////////////////////////////////
// DataBase

class DataBase : public nsIRDFDataBase {
protected:
    nsVoidArray*  mObservers;
        
    virtual ~DataBase(void);

public:
    DataBase(void);
    DataBase(char** dataSources);

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

    // nsIRDFDataBase interface
    NS_IMETHOD AddDataSource(nsIRDFDataSource* source);
    NS_IMETHOD RemoveDataSource(nsIRDFDataSource* source);

    PRBool HasAssertionN(int n, nsIRDFResource* source,
                            nsIRDFResource* property,
                            nsIRDFNode* target,
                            PRBool tv);


};

//NS_IMPL_ISUPPORTS(DataBase, kIRDFDataBaseIID);



class DBArcsInOutCursor : public nsIRDFArcsOutCursor,
                          public nsIRDFArcsInCursor
{
    DataBase* mDataBase;
    nsIRDFResource* mSource;
    nsIRDFNode*     mTarget;
    PRInt32         mCount;
    nsIRDFArcsOutCursor* mOutCursor;
    nsIRDFArcsInCursor* mInCursor;

public:
    DBArcsInOutCursor(DataBase* db, nsIRDFNode* node, PRBool arcsOutp);

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

// NS_IMPL_ISUPPORTS(DBArcsInOutCursor, kIRDFArcsOutCursorIID);
//NS_IMPL_ISUPPORTS(DBArcsInOutCursor, kIRDFArcsInCursorIID);
        
DBArcsInOutCursor::DBArcsInOutCursor (DataBase* db, nsIRDFNode* node, PRBool arcsOutp)
    : mDataBase(db), 
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
    // database, else this'll be a real short ride...
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
        if (NS_ERROR_RDF_CURSOR_EMPTY != result)
            return NS_OK;

        if (mCount >= mDataBase->mDataSources.Count())
            break;

        ds = (nsIRDFDataSource*) mDataBase->mDataSources[mCount];
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
    DataBase*       mDataBase;
    nsIRDFResource* mSource;
    nsIRDFResource* mLabel;    
    nsIRDFNode*     mTarget;
    PRInt32         mCount;
    PRBool          mTruthValue;
    nsIRDFAssertionCursor* mCurrentCursor;

public:
    DBGetSTCursor(DataBase* db, nsIRDFNode* u,  
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

DBGetSTCursor::DBGetSTCursor (DataBase* db, nsIRDFNode* u,
                              nsIRDFResource* property, PRBool inversep, 
                              PRBool tv)
    : mDataBase(db),
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

    // XXX assume that at least one data source exists in the database.
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
DBGetSTCursor::QueryInterface(REFNSIID iid, void** result) {
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
            if (!mDataBase->HasAssertionN(mCount-1, src, mLabel, trg, !mTruthValue)) {
                return NS_OK;
            } else {
                result = mCurrentCursor->Advance();
            }            
        }

        if (mCount >= mDataBase->mDataSources.Count())
            break;

        ds = (nsIRDFDataSource*) mDataBase->mDataSources[mCount];
        ++mCount;

        if (mSource)
            ds->GetTargets(mSource, mLabel, mTruthValue, &mCurrentCursor);
        else 
            ds->GetSources(mLabel, mTarget, mTruthValue, &mCurrentCursor);
    }
    return NS_ERROR_RDF_CURSOR_EMPTY;
}


////////////////////////////////////////////////////////////////////////


DataBase::DataBase(void)
{
    mObservers = 0;
    NS_INIT_REFCNT();
}


DataBase::~DataBase(void)
{
    for (PRInt32 i = mDataSources.Count() - 1; i >= 0; --i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        NS_IF_RELEASE(ds);
    }
}

////////////////////////////////////////////////////////////////////////
// nsISupports interface

NS_IMPL_ADDREF(DataBase);
NS_IMPL_RELEASE(DataBase);

NS_IMETHODIMP
DataBase::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(kIRDFDataBaseIID) ||
        iid.Equals(kIRDFDataSourceIID) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIRDFDataBase*, this);
		NS_ADDREF(this);
        return NS_OK;
    }
    return NS_NOINTERFACE;
}



////////////////////////////////////////////////////////////////////////
// nsIRDFDataSource interface

NS_IMETHODIMP
DataBase::Init(const char* uri)
{
    PR_ASSERT(0);
    return NS_ERROR_UNEXPECTED; // XXX database doesn't have a URI?
}

NS_IMETHODIMP
DataBase::GetURI(const char* *uri) const
{
    PR_ASSERT(0);
    return NS_ERROR_UNEXPECTED; // XXX database doesn't have a URI?
}

NS_IMETHODIMP
DataBase::GetSource(nsIRDFResource* property,
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
DataBase::GetSources(nsIRDFResource* property,
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
DataBase::GetTarget(nsIRDFResource* source,
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
DataBase::HasAssertionN (int n, nsIRDFResource* source,
                      nsIRDFResource* property,  nsIRDFNode* target, PRBool tv) {
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
DataBase::GetTargets(nsIRDFResource* source,
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
DataBase::Assert(nsIRDFResource* source, 
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
DataBase::Unassert(nsIRDFResource* source,
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
DataBase::HasAssertion(nsIRDFResource* source,
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

        if (hasAssertion)
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
DataBase::AddObserver(nsIRDFObserver* n)
{
    PRInt32 count = mDataSources.Count();
    for (PRInt32 i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        ds->AddObserver(n);
    }

    if (!mObservers) {
        if ((mObservers = new nsVoidArray()) == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
    }

    mObservers->AppendElement(n);

    return NS_OK;
}

NS_IMETHODIMP
DataBase::RemoveObserver(nsIRDFObserver* n)
{
    if (!mObservers) return NS_OK;

    PRInt32 count = mDataSources.Count();
    for (PRInt32 i = 0; i < count; ++i) {
        nsIRDFDataSource* ds = NS_STATIC_CAST(nsIRDFDataSource*, mDataSources[i]);
        ds->RemoveObserver(n);
    }
    return NS_OK;
}

NS_IMETHODIMP
DataBase::ArcLabelsIn(nsIRDFNode* node,
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
DataBase::ArcLabelsOut(nsIRDFResource* source,
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
DataBase::Flush()
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
// need to add the observers of the database to the new data source.

NS_IMETHODIMP
DataBase::AddDataSource(nsIRDFDataSource* source)
{
    if (! source)
        return NS_ERROR_NULL_POINTER;

    mDataSources.InsertElementAt(source, 0);
    NS_ADDREF(source);
    return NS_OK;
}



NS_IMETHODIMP
DataBase::RemoveDataSource(nsIRDFDataSource* source)
{
    if (! source)
        return NS_ERROR_NULL_POINTER;


    if (mDataSources.IndexOf(source) >= 0) {
        mDataSources.RemoveElement(source);
        NS_RELEASE(source);
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFDataBase(nsIRDFDataBase** result)
{
    DataBase* db = new DataBase();
    if (! db)
        return NS_ERROR_OUT_OF_MEMORY;

    *result = db;
    NS_ADDREF(*result);
    return NS_OK;
}
