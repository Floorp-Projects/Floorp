/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/*

  A global browser history implementation that also supports the RDF
  datasource interface.

  TODO

  1) Hook up Assert() etc. so that we can delete stuff.

*/

#include "nsCOMPtr.h"
#include "nsFileSpec.h"
#include "nsFileStream.h"
#include "nsIEnumerator.h"
#include "nsIGenericFactory.h"
#include "nsIGlobalHistory.h"
#include "nsIProfile.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsEnumeratorUtils.h"
#include "nsRDFCID.h"
#include "nsSpecialSystemDirectory.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "plhash.h"
#include "plstr.h"
#include "prprf.h"
#include "prtime.h"
#include "rdf.h"

#include "nsMdbPtr.h"
#include "nsInt64.h"
#include "nsMorkCID.h"
#include "nsIMdbFactoryFactory.h"
#include "mdb.h"

#ifdef MOZ_BRPROF
#include "nsIBrowsingProfile.h"
#endif

////////////////////////////////////////////////////////////////////////
// Common CIDs

static NS_DEFINE_CID(kComponentManagerCID,  NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kGenericFactoryCID,    NS_GENERICFACTORY_CID);
static NS_DEFINE_CID(kGlobalHistoryCID,     NS_GLOBALHISTORY_CID);
static NS_DEFINE_CID(kRDFServiceCID,        NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kProfileCID,           NS_PROFILE_CID);

#ifdef MOZ_BRPROF
static NS_DEFINE_CID(kBrowsingProfileCID,   NS_BROWSINGPROFILE_CID);
#endif


nsresult
PRInt64ToChars(PRInt64 aValue, char* aBuf, PRInt32 aSize)
{
  // Convert an unsigned 64-bit value to a string of up to aSize
  // decimal digits, placed in aBuf.
  nsInt64 value(aValue);

  if (aSize < 2)
    return NS_ERROR_FAILURE;

  if (value == nsInt64(0)) {
    aBuf[0] = '0';
    aBuf[1] = '\0';
  }

  char buf[64];
  char* p = buf;

  while (value != nsInt64(0)) {
    PRInt32 ones = PRInt32(value % nsInt64(10));
    value /= nsInt64(10);

    *p++ = '0' + ones;
  }

  if (p - buf >= aSize)
    return NS_ERROR_FAILURE;

  while (--p >= buf)
    *aBuf++ = *p;

  *aBuf = '\0';
  return NS_OK;
}

nsresult
CharsToPRInt64(const char* aBuf, PRUint32 aCount, PRInt64* aResult)
{
  // Convert aBuf of exactly aCount decimal characters to a 64-bit
  // unsigned integer value.
  nsInt64 result(0);

  while (aCount-- > 0) {
    PRInt32 digit = (*aBuf++) - '0';
    result *= nsInt64(10);
    result += nsInt64(digit);
  }

  *aResult = result;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsGlobalHistory
//
//   This class is the browser's implementation of the
//   nsIGlobalHistory interface.
//

class nsGlobalHistory : public nsIGlobalHistory,
                        public nsIRDFDataSource
{
public:
  // nsISupports methods 
  NS_DECL_ISUPPORTS

  // nsIGlobalHistory
  NS_DECL_NSIGLOBALHISTORY

  // nsIRDFDataSource
  NS_IMETHOD GetURI(char* *aURI);

  NS_IMETHOD GetSource(nsIRDFResource* aProperty,
                       nsIRDFNode* aTarget,
                       PRBool aTruthValue,
                       nsIRDFResource** aSource);

  NS_IMETHOD GetSources(nsIRDFResource* aProperty,
                        nsIRDFNode* aTarget,
                        PRBool aTruthValue,
                        nsISimpleEnumerator** aSources);

  NS_IMETHOD GetTarget(nsIRDFResource* aSource,
                       nsIRDFResource* aProperty,
                       PRBool aTruthValue,
                       nsIRDFNode** aTarget);

  NS_IMETHOD GetTargets(nsIRDFResource* aSource,
                        nsIRDFResource* aProperty,
                        PRBool aTruthValue,
                        nsISimpleEnumerator** aTargets);

  NS_IMETHOD Assert(nsIRDFResource* aSource, 
                    nsIRDFResource* aProperty, 
                    nsIRDFNode* aTarget,
                    PRBool aTruthValue);

  NS_IMETHOD Unassert(nsIRDFResource* aSource,
                      nsIRDFResource* aProperty,
                      nsIRDFNode* aTarget);

  NS_IMETHOD Change(nsIRDFResource* aSource,
                    nsIRDFResource* aProperty,
                    nsIRDFNode* aOldTarget,
                    nsIRDFNode* aNewTarget);

  NS_IMETHOD Move(nsIRDFResource* aOldSource,
                  nsIRDFResource* aNewSource,
                  nsIRDFResource* aProperty,
                  nsIRDFNode* aTarget);

  NS_IMETHOD HasAssertion(nsIRDFResource* aSource,
                          nsIRDFResource* aProperty,
                          nsIRDFNode* aTarget,
                          PRBool aTruthValue,
                          PRBool* aHasAssertion);

  NS_IMETHOD AddObserver(nsIRDFObserver* aObserver);

  NS_IMETHOD RemoveObserver(nsIRDFObserver* aObserver);

  NS_IMETHOD ArcLabelsIn(nsIRDFNode* aNode,
                         nsISimpleEnumerator** aLabels);

  NS_IMETHOD ArcLabelsOut(nsIRDFResource* aSource,
                          nsISimpleEnumerator** aLabels);

  NS_IMETHOD GetAllCommands(nsIRDFResource* aSource,
                            nsIEnumerator/*<nsIRDFResource>*/** aCommands);

  NS_IMETHOD GetAllCmds(nsIRDFResource* aSource,
                            nsISimpleEnumerator/*<nsIRDFResource>*/** aCommands);

  NS_IMETHOD IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                              nsIRDFResource*   aCommand,
                              nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                              PRBool* aResult);

  NS_IMETHOD DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                       nsIRDFResource*   aCommand,
                       nsISupportsArray/*<nsIRDFResource>*/* aArguments);

  NS_IMETHOD GetAllResources(nsISimpleEnumerator** aResult);

private:
  nsGlobalHistory(void);
  virtual ~nsGlobalHistory();

  friend NS_IMETHODIMP
  NS_NewGlobalHistory(nsISupports* aOuter, REFNSIID aIID, void** aResult);

  // Implementation Methods
  nsresult Init();
  nsresult OpenDB();
  nsresult CreateTokens();
  nsresult CloseDB();

  // N.B., these are MDB interfaces, _not_ XPCOM interfaces.
  nsIMdbEnv* mEnv;         // OWNER
  nsIMdbStore* mStore;     // OWNER
  nsIMdbTable* mTable;     // OWNER


  nsresult NotifyAssert(nsIRDFResource* aSource, nsIRDFResource* aProperty, nsIRDFNode* aValue);
  nsresult NotifyChange(nsIRDFResource* aSource, nsIRDFResource* aProperty, nsIRDFNode* aOldValue, nsIRDFNode* aNewValue);

  nsCOMPtr<nsISupportsArray> mObservers;

  mdb_scope  kToken_HistoryRowScope;
  mdb_kind   kToken_HistoryKind;
  mdb_column kToken_URLColumn;
  mdb_column kToken_ReferrerColumn;
  mdb_column kToken_LastVisitDateColumn;
  mdb_column kToken_NameColumn;

  // pseudo-constants. although the global history really is a
  // singleton, we'll use this metaphor to be consistent.
  static PRInt32 gRefCnt;
  static nsIRDFService* gRDFService;
  static nsIRDFResource* kNC_Page; // XXX do we need?
  static nsIRDFResource* kNC_Date;
  static nsIRDFResource* kNC_VisitCount;
  static nsIRDFResource* kNC_Name;
  static nsIRDFResource* kNC_Referrer;
  static nsIRDFResource* kNC_child;
  static nsIRDFResource* kNC_URL;  // XXX do we need?
  static nsIRDFResource* kNC_HistoryRoot;
  static nsIRDFResource* kNC_HistoryBySite;
  static nsIRDFResource* kNC_HistoryByDate;
};


PRInt32 nsGlobalHistory::gRefCnt;
nsIRDFService* nsGlobalHistory::gRDFService;
nsIRDFResource* nsGlobalHistory::kNC_Page;
nsIRDFResource* nsGlobalHistory::kNC_Date;
nsIRDFResource* nsGlobalHistory::kNC_VisitCount;
nsIRDFResource* nsGlobalHistory::kNC_Name;
nsIRDFResource* nsGlobalHistory::kNC_Referrer;
nsIRDFResource* nsGlobalHistory::kNC_child;
nsIRDFResource* nsGlobalHistory::kNC_URL;
nsIRDFResource* nsGlobalHistory::kNC_HistoryRoot;
nsIRDFResource* nsGlobalHistory::kNC_HistoryBySite;
nsIRDFResource* nsGlobalHistory::kNC_HistoryByDate;


////////////////////////////////////////////////////////////////////////


class nsMdbTableEnumerator : public nsISimpleEnumerator
{
protected:
  nsIMdbEnv*   mEnv;
  nsIMdbTable* mTable;
  mdb_column   mColumn;

  nsIMdbTableRowCursor* mCursor;
  nsIMdbRow*            mCurrent;

  static PRInt32        gRefCnt;
  static nsIRDFService* gRDFService;

  nsMdbTableEnumerator();
  virtual ~nsMdbTableEnumerator();
  nsresult Init(nsIMdbEnv* aEnv, nsIMdbTable* aTable, mdb_column aColumn);

public:
  static nsresult
  CreateInstance(nsIMdbEnv* aEnv,
                 nsIMdbTable* aTable,
                 mdb_column aColumn,
                 nsISimpleEnumerator** aResult);

  // nsISupports methods
  NS_DECL_ISUPPORTS

  // nsISimpleEnumeratorMethods
  NS_IMETHOD HasMoreElements(PRBool* _result);
  NS_IMETHOD GetNext(nsISupports** _result);
};

PRInt32 nsMdbTableEnumerator::gRefCnt;
nsIRDFService* nsMdbTableEnumerator::gRDFService;

nsMdbTableEnumerator::nsMdbTableEnumerator()
  : mEnv(nsnull), mTable(nsnull), mColumn(0), mCursor(nsnull), mCurrent(nsnull)
{
  NS_INIT_REFCNT();
}


nsresult
nsMdbTableEnumerator::Init(nsIMdbEnv* aEnv,
                           nsIMdbTable* aTable,
                           mdb_column aColumn)
{
  NS_PRECONDITION(aEnv != nsnull, "null ptr");
  if (! aEnv)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(aTable != nsnull, "null ptr");
  if (! aTable)
    return NS_ERROR_NULL_POINTER;

  mEnv = aEnv;
  mEnv->AddStrongRef(mEnv);

  mTable = aTable;
  mTable->AddStrongRef(mEnv);

  mColumn = aColumn;

  mdb_err err;
  err = mTable->GetTableRowCursor(mEnv, -1, &mCursor);
  if (err != 0) return NS_ERROR_FAILURE;

  if (gRefCnt++ == 0) {
    nsresult rv;
    rv = nsServiceManager::GetService(kRDFServiceCID,
                                      nsCOMTypeInfo<nsIRDFService>::GetIID(),
                                      (nsISupports**) &gRDFService);
    if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}


nsMdbTableEnumerator::~nsMdbTableEnumerator()
{
  if (mCurrent)
    mCurrent->CutStrongRef(mEnv);

  if (mCursor)
    mCursor->CutStrongRef(mEnv);

  if (mTable)
    mTable->CutStrongRef(mEnv);

  if (mEnv)
    mEnv->CutStrongRef(mEnv);

  if (--gRefCnt == 0) {
    nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
  }
}


nsresult
nsMdbTableEnumerator::CreateInstance(nsIMdbEnv* aEnv,
                                     nsIMdbTable* aTable,
                                     mdb_column aColumn,
                                     nsISimpleEnumerator** aResult)
{
  nsMdbTableEnumerator* result = new nsMdbTableEnumerator();
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(result);

  nsresult rv;
  rv = result->Init(aEnv, aTable, aColumn);
  if (NS_FAILED(rv)) {
    NS_RELEASE(result);
    return rv;
  }

  *aResult = result;
  return NS_OK;
}


NS_IMPL_ISUPPORTS(nsMdbTableEnumerator, nsCOMTypeInfo<nsISimpleEnumerator>::GetIID());

NS_IMETHODIMP
nsMdbTableEnumerator::HasMoreElements(PRBool* _result)
{
  if (! mCurrent) {
    mdb_err err;

    mdb_pos pos;
    err = mCursor->NextRow(mEnv, &mCurrent, &pos);
    if (err != 0) return NS_ERROR_FAILURE;
  }

  *_result = (mCurrent != nsnull);
  return NS_OK;
}


NS_IMETHODIMP
nsMdbTableEnumerator::GetNext(nsISupports** _result)
{
  nsresult rv;

  PRBool hasMore;
  rv = HasMoreElements(&hasMore);
  if (NS_FAILED(rv)) return rv;

  if (! hasMore)
    return NS_ERROR_UNEXPECTED;

  mdb_err err;

  nsIMdbCell* cell;
  err = mCurrent->GetCell(mEnv, mColumn, &cell);
  if (err != 0) return NS_ERROR_FAILURE;

  rv = NS_ERROR_FAILURE;

  // XXX cell might be null if no value set
  mdbYarn yarn;
  err = cell->AliasYarn(mEnv, &yarn);
  if (err == 0) {
    nsCAutoString uri((const char*) yarn.mYarn_Buf, yarn.mYarn_Fill);

    nsCOMPtr<nsIRDFResource> resource;
    rv = gRDFService->GetResource(uri, getter_AddRefs(resource));
    if (NS_SUCCEEDED(rv)) {
      *_result = resource;
      NS_ADDREF(*_result);
    }
  }
  
  if (cell)
    cell->CutStrongRef(mEnv);

  mCurrent->CutStrongRef(mEnv);
  mCurrent = nsnull;
  return rv;
}

////////////////////////////////////////////////////////////////////////


nsGlobalHistory::nsGlobalHistory()
  : mEnv(nsnull),
    mStore(nsnull),
    mTable(nsnull)
{
  NS_INIT_REFCNT();
}

nsGlobalHistory::~nsGlobalHistory()
{
  gRDFService->UnregisterDataSource(this);

  nsresult rv;
  rv = CloseDB();

  if (--gRefCnt == 0) {
    if (gRDFService) {
      nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
      gRDFService = nsnull;
    }

    NS_IF_RELEASE(kNC_Page);
    NS_IF_RELEASE(kNC_Date);
    NS_IF_RELEASE(kNC_VisitCount);
    NS_IF_RELEASE(kNC_Name);
    NS_IF_RELEASE(kNC_Referrer);
    NS_IF_RELEASE(kNC_child);
    NS_IF_RELEASE(kNC_URL);
    NS_IF_RELEASE(kNC_HistoryRoot);
    NS_IF_RELEASE(kNC_HistoryBySite);
    NS_IF_RELEASE(kNC_HistoryByDate);
  }
}



NS_IMETHODIMP
NS_NewGlobalHistory(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(aOuter == nsnull, "no aggregation");
  if (aOuter)
    return NS_ERROR_NO_AGGREGATION;

  nsresult rv = NS_OK;

  nsGlobalHistory* result = new nsGlobalHistory();
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  rv = result->Init();
  if (NS_SUCCEEDED(rv))
    rv = result->QueryInterface(aIID, aResult);

  if (NS_FAILED(rv)) {
    delete result;
    *aResult = nsnull;
    return rv;
  }

  return rv;
}


////////////////////////////////////////////////////////////////////////
// nsISupports

NS_IMPL_ADDREF(nsGlobalHistory);
NS_IMPL_RELEASE(nsGlobalHistory);

NS_IMETHODIMP
nsGlobalHistory::QueryInterface(REFNSIID aIID, void** aResult)
{
  NS_PRECONDITION(aResult != nsnull, "null ptr");
  if (! aResult)
    return NS_ERROR_NULL_POINTER;

  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

  if (aIID.Equals(nsIGlobalHistory::GetIID()) ||
      aIID.Equals(kISupportsIID)) {
    *aResult = NS_STATIC_CAST(nsIGlobalHistory*, this);
  }
  else if (aIID.Equals(nsIRDFDataSource::GetIID())) {
    *aResult = NS_STATIC_CAST(nsIRDFDataSource*, this);
  }
  else {
    *aResult = nsnull;
    return NS_NOINTERFACE;
  }

  NS_ADDREF(this);
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIGlobalHistory

NS_IMETHODIMP
nsGlobalHistory::AddPage(const char *aURL, const char *aReferrerURL, PRInt64 aDate)
{
  NS_PRECONDITION(aURL != nsnull, "null ptr");
  if (! aURL)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(mEnv != nsnull, "not initialized");
  if (! mEnv)
    return NS_ERROR_NOT_INITIALIZED;

  NS_PRECONDITION(mStore != nsnull, "not initialized");
  if (! mStore)
    return NS_ERROR_NOT_INITIALIZED;

  nsresult rv;
  mdb_err err;

  PRInt32 len = PL_strlen(aURL);
  mdbYarn yarn = { (void*) aURL, len, len, 0, 0, nsnull };

  mdbOid rowId;
  nsMdbPtr<nsIMdbRow> row(mEnv);
  err = mStore->FindRow(mEnv, kToken_HistoryRowScope, kToken_URLColumn, &yarn,
                        &rowId, getter_Acquires(row));

  if (err != 0) return NS_ERROR_FAILURE;

  // For notifying observers, later...
  nsCOMPtr<nsIRDFResource> url;
  rv = gRDFService->GetResource(aURL, getter_AddRefs(url));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFDate> date;
  rv = gRDFService->GetDateLiteral(aDate, getter_AddRefs(date));
  if (NS_FAILED(rv)) return rv;

  if (row) {
    // Update last visit date. First get the old date so we can update observers...
    nsMdbPtr<nsIMdbCell> lastvisitdate(mEnv);
    err = row->GetCell(mEnv, kToken_LastVisitDateColumn, getter_Acquires(lastvisitdate));
    if (err != 0) return NS_ERROR_FAILURE;

    lastvisitdate->AliasYarn(mEnv, &yarn);

    PRInt64 oldvalue;
    rv = CharsToPRInt64((const char*) yarn.mYarn_Buf, yarn.mYarn_Fill, &oldvalue);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFDate> olddate;
    rv = gRDFService->GetDateLiteral(aDate, getter_AddRefs(olddate));
    if (NS_FAILED(rv)) return rv;

    // ...now set the new date.
    char buf[64];
    PRInt64ToChars(aDate, buf, sizeof(buf));

    yarn.mYarn_Buf = (void*) buf;
    yarn.mYarn_Fill = yarn.mYarn_Size = PL_strlen(buf);

    err = row->AddColumn(mEnv, kToken_LastVisitDateColumn, &yarn);
    if (err != 0) return NS_ERROR_FAILURE;

    // Notify observers
    rv = NotifyChange(url, kNC_Date, olddate, date);
    if (NS_FAILED(rv)) return rv;
  }
  else {
    // Create a new row
    rowId.mOid_Scope = kToken_HistoryRowScope;
    rowId.mOid_Id    = mdb_id(-1);

    err = mTable->NewRow(mEnv, &rowId, getter_Acquires(row));
    if (err != 0) return NS_ERROR_FAILURE;

    // Set the URL. 'yarn' is already set up to go.
    err = row->AddColumn(mEnv, kToken_URLColumn, &yarn);
    if (err != 0) return NS_ERROR_FAILURE;

    // Set the referrer, if one is provided
    if (aReferrerURL) {
      len = PL_strlen(aReferrerURL);
      yarn.mYarn_Buf  = (void*) aReferrerURL;
      yarn.mYarn_Fill = yarn.mYarn_Size = len;

      err = row->AddColumn(mEnv, kToken_ReferrerColumn, &yarn);
      if (err != 0) return NS_ERROR_FAILURE;

      // Notify observers
      nsCOMPtr<nsIRDFResource> referrer;
      rv = gRDFService->GetResource(aReferrerURL, getter_AddRefs(referrer));
      if (NS_FAILED(rv)) return rv;

      rv = NotifyAssert(url, kNC_Referrer, referrer);
      if (NS_FAILED(rv)) return rv;
    }

    // Set the date.
    char buf[64];
    PRInt64ToChars(aDate, buf, sizeof(buf));

    yarn.mYarn_Buf = (void*) buf;
    yarn.mYarn_Fill = yarn.mYarn_Size = PL_strlen(buf);

    err = row->AddColumn(mEnv, kToken_LastVisitDateColumn, &yarn);
    if (err != 0) return NS_ERROR_FAILURE;

    // Notify observers
    rv = NotifyAssert(url, kNC_Date, date);
    if (NS_FAILED(rv)) return rv;

    rv = NotifyAssert(kNC_HistoryRoot, kNC_child, url);
    if (NS_FAILED(rv)) return rv;
  }

  if (err != 0) return NS_ERROR_FAILURE;

  // We'd like to do a "small commit" here, but, unfortunately, we're
  // leaking the _entire_ history service :-(. So just do a "large"
  // commit for now.
  err = mStore->SmallCommit(mEnv);
  if (err != 0) return NS_ERROR_FAILURE;

#ifdef LEAKING_GLOBAL_HISTORY
  {
    nsMdbPtr<nsIMdbThumb> thumb(mEnv);
    err = mStore->LargeCommit(mEnv, getter_Acquires(thumb));
    if (err != 0) return NS_ERROR_FAILURE;

    mdb_count total;
    mdb_count current;
    mdb_bool done;
    mdb_bool broken;

    do {
      err = thumb->DoMore(mEnv, &total, &current, &done, &broken);
    } while ((err == 0) && !broken && !done);

    if ((err != 0) || !done) return NS_ERROR_FAILURE;
  }
#endif

  return NS_OK;
}


NS_IMETHODIMP
nsGlobalHistory::SetPageTitle(const char *aURL, const PRUnichar *aTitle)
{
  NS_PRECONDITION(aURL != nsnull, "null ptr");
  if (! aURL)
    return NS_ERROR_NULL_POINTER;

  // Be defensive if somebody sends us a null title.
  static PRUnichar kEmptyString[] = { 0 };
  if (! aTitle)
    aTitle = kEmptyString;

  mdb_err err;

  PRInt32 len = PL_strlen(aURL);
  mdbYarn yarn = { (void*) aURL, len, len, 0, 0, nsnull };

  mdbOid rowId;
  nsMdbPtr<nsIMdbRow> row(mEnv);
  err = mStore->FindRow(mEnv, kToken_HistoryRowScope, kToken_URLColumn, &yarn,
                        &rowId, getter_Acquires(row));

  if (err != 0) return NS_ERROR_FAILURE;

  if (! row)
    return NS_ERROR_UNEXPECTED; // we haven't seen this page yet

  // Get the old title so we can notify observers
  nsMdbPtr<nsIMdbCell> cell(mEnv);
  err = row->GetCell(mEnv, kToken_NameColumn, getter_Acquires(cell));
  if (err != 0) return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsIRDFLiteral> oldname;
  if (cell) {
    cell->AliasYarn(mEnv, &yarn);
    nsAutoString str((const PRUnichar*) yarn.mYarn_Buf, PRInt32(yarn.mYarn_Fill / sizeof(PRUnichar)));

    rv = gRDFService->GetLiteral(str.GetUnicode(), getter_AddRefs(oldname));
    if (NS_FAILED(rv)) return rv;
  }

  // Now poke in the new title
  yarn.mYarn_Buf = (void*) aTitle;
  yarn.mYarn_Fill = yarn.mYarn_Size = (nsCRT::strlen(aTitle) * sizeof(PRUnichar));

  err = row->AddColumn(mEnv, kToken_NameColumn, &yarn);
  if (err != 0) return NS_ERROR_FAILURE;

  // ...and update observers
  nsCOMPtr<nsIRDFResource> url;
  rv = gRDFService->GetResource(aURL, getter_AddRefs(url));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFLiteral> name;
  rv = gRDFService->GetLiteral(aTitle, getter_AddRefs(name));
  if (NS_FAILED(rv)) return rv;

  if (oldname) {
    rv = NotifyChange(url, kNC_Name, oldname, name);
  }
  else {
    rv = NotifyAssert(url, kNC_Name, name);
  }

  return rv;
}


NS_IMETHODIMP
nsGlobalHistory::RemovePage(const char *aURL)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsGlobalHistory::GetLastVisitDate(const char *aURL, PRInt64 *_retval)
{
  nsresult rv;
  mdb_err err;

  PRInt32 len = PL_strlen(aURL);
  mdbYarn yarn = { (void*) aURL, len, len, 0, 0, nsnull };

  mdbOid rowId;
  nsMdbPtr<nsIMdbRow> row(mEnv);
  err = mStore->FindRow(mEnv, kToken_HistoryRowScope, kToken_URLColumn, &yarn,
                        &rowId, getter_Acquires(row));

  if (err != 0) return NS_ERROR_FAILURE;

  if ((err == 0) && (row)) {
    nsMdbPtr<nsIMdbCell> lastvisitdate(mEnv);
    err = row->GetCell(mEnv, kToken_LastVisitDateColumn, getter_Acquires(lastvisitdate));
    if (err != 0) return NS_ERROR_FAILURE;

    lastvisitdate->AliasYarn(mEnv, &yarn);

    rv = CharsToPRInt64((const char*) yarn.mYarn_Buf, yarn.mYarn_Fill, _retval);
    if (NS_FAILED(rv)) return rv;
  }
  else {
    *_retval = LL_ZERO; // return zero if not found
  }

  return NS_OK;
}


NS_IMETHODIMP
nsGlobalHistory::GetURLCompletion(const char *aURL, char **_retval)
{
  NS_NOTYETIMPLEMENTED("write me");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGlobalHistory::GetLastPageVisted(char **_retval)
{ 
  *_retval = PR_smprintf("http://people.netscape.com/waterson");
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
// nsIRDFDataSource

NS_IMETHODIMP
nsGlobalHistory::GetURI(char* *aURI)
{
  NS_PRECONDITION(aURI != nsnull, "null ptr");
  if (! aURI)
    return NS_ERROR_NULL_POINTER;

  *aURI = nsXPIDLCString::Copy("rdf:history");
  if (! *aURI)
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}


NS_IMETHODIMP
nsGlobalHistory::GetSource(nsIRDFResource* aProperty,
                           nsIRDFNode* aTarget,
                           PRBool aTruthValue,
                           nsIRDFResource** aSource)
{
  NS_NOTYETIMPLEMENTED("sorry");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGlobalHistory::GetSources(nsIRDFResource* aProperty,
                            nsIRDFNode* aTarget,
                            PRBool aTruthValue,
                            nsISimpleEnumerator** aSources)
{
  NS_NOTYETIMPLEMENTED("sorry");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGlobalHistory::GetTarget(nsIRDFResource* aSource,
                           nsIRDFResource* aProperty,
                           PRBool aTruthValue,
                           nsIRDFNode** aTarget)
{
  NS_PRECONDITION(aSource != nsnull, "null ptr");
  if (! aSource)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(aProperty != nsnull, "null ptr");
  if (! aProperty)
    return NS_ERROR_NULL_POINTER;

  nsresult rv;
  *aTarget = nsnull;

  if ((aSource == kNC_HistoryRoot) && (aProperty == kNC_child)) {
    nsCOMPtr<nsISimpleEnumerator> targets;
    rv = GetTargets(aSource, aProperty, aTruthValue, getter_AddRefs(targets));
    if (NS_FAILED(rv)) return rv;

    PRBool hasMore;
    rv = targets->HasMoreElements(&hasMore);
    if (NS_FAILED(rv)) return rv;

    if (! hasMore) return NS_RDF_NO_VALUE;

    nsCOMPtr<nsISupports> isupports;
    rv = targets->GetNext(getter_AddRefs(isupports));
    if (NS_FAILED(rv)) return rv;

    return isupports->QueryInterface(nsCOMTypeInfo<nsIRDFNode>::GetIID(), (void**) aTarget);
  }
  else if (aProperty == kNC_URL) {
    return aSource->QueryInterface(nsCOMTypeInfo<nsIRDFNode>::GetIID(), (void**) aTarget);
  }
  else if ((aProperty == kNC_Date) ||
           (aProperty == kNC_VisitCount) ||
           (aProperty == kNC_Name) ||
           (aProperty == kNC_Referrer)) {
    // It's a real property! Okay, first we'll get the row...
    mdb_err err;

    nsXPIDLCString uri;
    rv = aSource->GetValueConst(getter_Shares(uri));
    if (NS_FAILED(rv)) return rv;

    PRInt32 len = PL_strlen(uri);
    mdbYarn yarn = { (void*)NS_STATIC_CAST(const char*, uri), len, len, 0, 0, nsnull };

    mdbOid rowId;
    nsMdbPtr<nsIMdbRow> row(mEnv);
    err = mStore->FindRow(mEnv, kToken_HistoryRowScope, kToken_URLColumn, &yarn, &rowId, getter_Acquires(row));
    if (err != 0) return NS_ERROR_FAILURE;

    if (!row) return NS_RDF_NO_VALUE;

    // ...and then depending on the property they want, we'll pull the
    // cell they want out of it.
    if (aProperty == kNC_Date) {
      // Last visit date
      nsMdbPtr<nsIMdbCell> cell(mEnv);
      err = row->GetCell(mEnv, kToken_LastVisitDateColumn, getter_Acquires(cell));
      if (err != 0) return NS_ERROR_FAILURE;

      if (!cell) return NS_RDF_NO_VALUE;

      cell->AliasYarn(mEnv, &yarn);

      PRInt64 i;
      rv = CharsToPRInt64((const char*) yarn.mYarn_Buf, yarn.mYarn_Fill, &i);
      if (NS_FAILED(rv)) return rv;

      nsCOMPtr<nsIRDFDate> date;
      rv = gRDFService->GetDateLiteral(i, getter_AddRefs(date));
      if (NS_FAILED(rv)) return rv;

      return date->QueryInterface(nsCOMTypeInfo<nsIRDFNode>::GetIID(), (void**) aTarget);
    }
    else if (aProperty == kNC_VisitCount) {
      // Visit count
      NS_NOTYETIMPLEMENTED("sorry");
      rv = NS_ERROR_NOT_IMPLEMENTED;
    }
    else if (aProperty == kNC_Name) {
      // Site name (i.e., page title)
      nsMdbPtr<nsIMdbCell> cell(mEnv);
      err = row->GetCell(mEnv, kToken_NameColumn, getter_Acquires(cell));
      if (err != 0) return NS_ERROR_FAILURE;

      if (!cell) return NS_RDF_NO_VALUE;

      cell->AliasYarn(mEnv, &yarn);

      nsAutoString str((const PRUnichar*) yarn.mYarn_Buf, PRInt32(yarn.mYarn_Fill / sizeof(PRUnichar)));

      nsCOMPtr<nsIRDFLiteral> name;
      rv = gRDFService->GetLiteral(str.GetUnicode(), getter_AddRefs(name));
      if (NS_FAILED(rv)) return rv;

      return name->QueryInterface(nsCOMTypeInfo<nsIRDFNode>::GetIID(), (void**) aTarget);
    }
    else if (aProperty == kNC_Referrer) {
      // Referrer field
      nsMdbPtr<nsIMdbCell> cell(mEnv);
      err = row->GetCell(mEnv, kToken_ReferrerColumn, getter_Acquires(cell));
      if (err != 0) return NS_ERROR_FAILURE;

      if (!cell) return NS_RDF_NO_VALUE;

      cell->AliasYarn(mEnv, &yarn);

      nsCAutoString str((const char*) yarn.mYarn_Buf, yarn.mYarn_Fill);

      nsCOMPtr<nsIRDFResource> referrer;
      rv = gRDFService->GetResource((const char*) str, getter_AddRefs(referrer));
      if (NS_FAILED(rv)) return rv;

      return referrer->QueryInterface(nsCOMTypeInfo<nsIRDFNode>::GetIID(), (void**) aTarget);
    }
  }

  *aTarget = nsnull;
  return NS_RDF_NO_VALUE;
}

NS_IMETHODIMP
nsGlobalHistory::GetTargets(nsIRDFResource* aSource,
                            nsIRDFResource* aProperty,
                            PRBool aTruthValue,
                            nsISimpleEnumerator** aTargets)
{
  NS_PRECONDITION(aSource != nsnull, "null ptr");
  if (! aSource)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(aProperty != nsnull, "null ptr");
  if (! aProperty)
    return NS_ERROR_NULL_POINTER;

  if ((aSource == kNC_HistoryRoot) && (aProperty == kNC_child) && (aTruthValue)) {
    return nsMdbTableEnumerator::CreateInstance(mEnv, mTable, kToken_URLColumn, aTargets);
  }
  else if ((aProperty == kNC_Date) ||
           (aProperty == kNC_VisitCount) ||
           (aProperty == kNC_Name) ||
           (aProperty == kNC_Referrer)) {
    nsresult rv;

    nsCOMPtr<nsIRDFNode> target;
    rv = GetTarget(aSource, aProperty, aTruthValue, getter_AddRefs(target));
    if (NS_FAILED(rv)) return rv;

    if (rv == NS_OK) {
      return NS_NewSingletonEnumerator(aTargets, target);
    }
  }

  return NS_NewEmptyEnumerator(aTargets);
}

NS_IMETHODIMP
nsGlobalHistory::Assert(nsIRDFResource* aSource, 
                        nsIRDFResource* aProperty, 
                        nsIRDFNode* aTarget,
                        PRBool aTruthValue)
{
  // History cannot be modified
  return NS_RDF_ASSERTION_REJECTED;
}

NS_IMETHODIMP
nsGlobalHistory::Unassert(nsIRDFResource* aSource,
                          nsIRDFResource* aProperty,
                          nsIRDFNode* aTarget)
{
  // History cannot be modified
  return NS_RDF_ASSERTION_REJECTED;
}

NS_IMETHODIMP
nsGlobalHistory::Change(nsIRDFResource* aSource,
                        nsIRDFResource* aProperty,
                        nsIRDFNode* aOldTarget,
                        nsIRDFNode* aNewTarget)
{
  return NS_RDF_ASSERTION_REJECTED;
}

NS_IMETHODIMP
nsGlobalHistory::Move(nsIRDFResource* aOldSource,
                      nsIRDFResource* aNewSource,
                      nsIRDFResource* aProperty,
                      nsIRDFNode* aTarget)
{
  return NS_RDF_ASSERTION_REJECTED;
}

NS_IMETHODIMP
nsGlobalHistory::HasAssertion(nsIRDFResource* aSource,
                              nsIRDFResource* aProperty,
                              nsIRDFNode* aTarget,
                              PRBool aTruthValue,
                              PRBool* aHasAssertion)
{
  nsresult rv;

  nsCOMPtr<nsISimpleEnumerator> targets;
  rv = GetTargets(aSource, aProperty, aTruthValue, getter_AddRefs(targets));
  if (NS_FAILED(rv)) return rv;

  while (1) {
    PRBool hasMore;
    rv = targets->HasMoreElements(&hasMore);
    if (NS_FAILED(rv)) return rv;

    if (! hasMore)
      break;

    nsCOMPtr<nsISupports> isupports;
    rv = targets->GetNext(getter_AddRefs(isupports));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFNode> node = do_QueryInterface(isupports);
    if (node.get() == aTarget) {
      *aHasAssertion = PR_TRUE;
      return NS_OK;
    }
  }

  *aHasAssertion = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalHistory::AddObserver(nsIRDFObserver* aObserver)
{
  NS_PRECONDITION(aObserver != nsnull, "null ptr");
  if (! aObserver)
    return NS_ERROR_NULL_POINTER;

  if (! mObservers) {
    nsresult rv;
    rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
    if (NS_FAILED(rv)) return rv;
  }
  mObservers->AppendElement(aObserver);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalHistory::RemoveObserver(nsIRDFObserver* aObserver)
{
  NS_PRECONDITION(aObserver != nsnull, "null ptr");
  if (! aObserver)
    return NS_ERROR_NULL_POINTER;

  if (! mObservers)
    return NS_OK;

  mObservers->RemoveElement(aObserver);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalHistory::ArcLabelsIn(nsIRDFNode* aNode,
                             nsISimpleEnumerator** aLabels)
{
  NS_NOTYETIMPLEMENTED("sorry");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGlobalHistory::ArcLabelsOut(nsIRDFResource* aSource,
                              nsISimpleEnumerator** aLabels)
{
  nsresult rv;

  if ((aSource == kNC_HistoryRoot) ||
      (aSource == kNC_HistoryBySite) ||
      (aSource == kNC_HistoryByDate)) {
    return NS_NewSingletonEnumerator(aLabels, kNC_child);
  }
  else {
    // We'll be optimistic and _assume_ that this is in the history.
    nsCOMPtr<nsISupportsArray> array;
    rv = NS_NewISupportsArray(getter_AddRefs(array));
    if (NS_FAILED(rv)) return rv;

//  XXX rjc: comment this out for the short term;
//  Note: shouldn't return kNC_child unless its really a container
//  array->AppendElement(kNC_child);

    array->AppendElement(kNC_Date);
    array->AppendElement(kNC_VisitCount);
    array->AppendElement(kNC_Name);
    array->AppendElement(kNC_Referrer);

    return NS_NewArrayEnumerator(aLabels, array);
  }
}

NS_IMETHODIMP
nsGlobalHistory::GetAllCommands(nsIRDFResource* aSource,
                                nsIEnumerator/*<nsIRDFResource>*/** aCommands)
{
  NS_NOTYETIMPLEMENTED("sorry");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGlobalHistory::GetAllCmds(nsIRDFResource* aSource,
                            nsISimpleEnumerator/*<nsIRDFResource>*/** aCommands)
{
  return NS_NewEmptyEnumerator(aCommands);
}

NS_IMETHODIMP
nsGlobalHistory::IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                                  nsIRDFResource*   aCommand,
                                  nsISupportsArray/*<nsIRDFResource>*/* aArguments,
                                  PRBool* aResult)
{
  NS_NOTYETIMPLEMENTED("sorry");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGlobalHistory::DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
                           nsIRDFResource*   aCommand,
                           nsISupportsArray/*<nsIRDFResource>*/* aArguments)
{
  NS_NOTYETIMPLEMENTED("sorry");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGlobalHistory::GetAllResources(nsISimpleEnumerator** aResult)
{
  return nsMdbTableEnumerator::CreateInstance(mEnv, mTable, kToken_URLColumn, aResult);
}


////////////////////////////////////////////////////////////////////////
// Implementation methods

nsresult
nsGlobalHistory::Init()
{
  nsresult rv;

  if (gRefCnt++ == 0) {
    rv = nsServiceManager::GetService(kRDFServiceCID,
                                      nsIRDFService::GetIID(),
                                      (nsISupports**) &gRDFService);

    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
    if (NS_FAILED(rv)) return rv;

    gRDFService->GetResource(NC_NAMESPACE_URI "Page",        &kNC_Page);
    gRDFService->GetResource(NC_NAMESPACE_URI "Date",        &kNC_Date);
    gRDFService->GetResource(NC_NAMESPACE_URI "VisitCount",  &kNC_VisitCount);
    gRDFService->GetResource(NC_NAMESPACE_URI "Name",        &kNC_Name);
    gRDFService->GetResource(NC_NAMESPACE_URI "Referrer",    &kNC_Referrer);
    gRDFService->GetResource(NC_NAMESPACE_URI "child",       &kNC_child);
    gRDFService->GetResource(NC_NAMESPACE_URI "URL",         &kNC_URL);
    gRDFService->GetResource("NC:HistoryRoot",               &kNC_HistoryRoot);
    gRDFService->GetResource("NC:HistoryBySite",             &kNC_HistoryBySite);
    gRDFService->GetResource("NC:HistoryByDate",             &kNC_HistoryByDate);
  }

  // register this as a named data source with the RDF service
  rv = gRDFService->RegisterDataSource(this, PR_FALSE);
  NS_ASSERTION(NS_SUCCEEDED(rv), "somebody already created & registered the history data source");
  if (NS_FAILED(rv)) return rv;

  rv = OpenDB();
  if (NS_FAILED(rv)) return rv;

  return NS_OK;
}


nsresult
nsGlobalHistory::OpenDB()
{
  nsresult rv;

  NS_WITH_SERVICE(nsIProfile, profile, kProfileCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsFileSpec dbfile;
  rv = profile->GetCurrentProfileDir(&dbfile);
  if (NS_FAILED(rv)) return rv;

  // Urgh. No profile directory. Use tmp instead.
  if (! dbfile.Exists()) {
    // XXX Probably should warn user before doing this. Oh well.
    dbfile = nsSpecialSystemDirectory(nsSpecialSystemDirectory::OS_TemporaryDirectory);
  }

  dbfile += "history.dat";

  // Leaving XPCOM, entering Mork. The IID is a lie; the component
  // manager appears to be used solely to get dynamic loading of the
  // Mork DLL.
  nsIMdbFactory* factory;

  static NS_DEFINE_CID(kMorkCID, NS_MORK_CID);
#ifdef BIENVENU_HAS_FIXED_THE_FACTORY
  nsCOMPtr<nsIMdbFactoryFactory> factoryfactory;
  rv = nsComponentManager::CreateInstance(kMorkCID,
                                          nsnull,
                                          nsCOMTypeInfo<nsIMdbFactoryFactory>::GetIID(),
                                          getter_AddRefs(factoryfactory));
  if (NS_FAILED(rv)) return rv;

  rv = factoryfactory->GetMdbFactory(&factory);
#else
  rv = nsComponentManager::CreateInstance(kMorkCID,
                                          nsnull,
                                          nsCOMTypeInfo<nsISupports>::GetIID(),
                                          (void**) &factory);
#endif
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create mork factory factory");
  if (NS_FAILED(rv)) return rv;

  mdb_err err;

  err = factory->MakeEnv(nsnull, &mEnv);
  NS_ASSERTION((err == 0), "unable to create mdb env");
  if (err != 0) return NS_ERROR_FAILURE;

	nsIMdbHeap* dbHeap = 0;
	mdb_bool dbFrozen = mdbBool_kFalse; // not readonly, we want modifiable

  if (dbfile.Exists()) {
    mdb_bool canopen = 0;
    mdbYarn outfmt = { nsnull, 0, 0, 0, 0, nsnull };

    nsMdbPtr<nsIMdbFile> oldFileAnchor(mEnv); // ensures file is released
		err = factory->OpenOldFile(mEnv, dbHeap, dbfile.GetNativePathCString(),
			 dbFrozen, getter_Acquires(oldFileAnchor));
		nsIMdbFile* oldFile = oldFileAnchor;
    if ((err != 0) || !oldFile) return NS_ERROR_FAILURE;

		err = factory->CanOpenFilePort(mEnv, oldFile, // the file to investigate
			&canopen, &outfmt);

    // XXX possible that format out of date, in which case we should
    // just re-write the file.
    if ((err != 0) || !canopen) return NS_ERROR_FAILURE;

    nsIMdbThumb* thumb = nsnull;
    mdbOpenPolicy policy = { { 0, 0 }, 0, 0 };

		err = factory->OpenFileStore(mEnv, dbHeap, oldFile, &policy, &thumb); 
    if ((err != 0) || !thumb) return NS_ERROR_FAILURE;

    mdb_count total;
    mdb_count current;
    mdb_bool done;
    mdb_bool broken;

    do {
      err = thumb->DoMore(mEnv, &total, &current, &done, &broken);
    } while ((err == 0) && !broken && !done);

    if ((err == 0) && done) {
      err = factory->ThumbToOpenStore(mEnv, thumb, &mStore);
    }

    thumb->CutStrongRef(mEnv);
    thumb = nsnull;

    if ((err != 0) || !done) return NS_ERROR_FAILURE;

    rv = CreateTokens();
    if (NS_FAILED(rv)) return rv;

    mdbOid oid = { kToken_HistoryRowScope, 1 };
    err = mStore->GetTable(mEnv, &oid, &mTable);
    if (err != 0) return NS_ERROR_FAILURE;
  }
  else {

    nsMdbPtr<nsIMdbFile> newFileAnchor(mEnv); // ensures file is released
		err = factory->CreateNewFile(mEnv, dbHeap, dbfile.GetNativePathCString(),
			 getter_Acquires(newFileAnchor));
		nsIMdbFile* newFile = newFileAnchor;
    if ((err != 0) || !newFile) return NS_ERROR_FAILURE;

    mdbOpenPolicy policy = { { 0, 0 }, 0, 0 };
    err = factory->CreateNewFileStore(mEnv, dbHeap, newFile, &policy, &mStore);
    if (err != 0) return NS_ERROR_FAILURE;

    rv = CreateTokens();
    if (NS_FAILED(rv)) return rv;

    // Create the one and only table in the history db
    err = mStore->NewTable(mEnv, kToken_HistoryRowScope, kToken_HistoryKind, PR_TRUE, nsnull, &mTable);
    if (err != 0) return NS_ERROR_FAILURE;

    // Force a commit now to get it written out.
    nsMdbPtr<nsIMdbThumb> thumb(mEnv);
    err = mStore->LargeCommit(mEnv, getter_Acquires(thumb));
    if (err != 0) return NS_ERROR_FAILURE;

    mdb_count total;
    mdb_count current;
    mdb_bool done;
    mdb_bool broken;

    do {
      err = thumb->DoMore(mEnv, &total, &current, &done, &broken);
    } while ((err == 0) && !broken && !done);

    if ((err != 0) || !done) return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


nsresult
nsGlobalHistory::CreateTokens()
{
  mdb_err err;

  NS_PRECONDITION(mStore != nsnull, "not initialized");
  if (! mStore)
    return NS_ERROR_NOT_INITIALIZED;

  err = mStore->StringToToken(mEnv, "ns:history:db:row:scope:history:all", &kToken_HistoryRowScope);
  if (err != 0) return NS_ERROR_FAILURE;

  err = mStore->StringToToken(mEnv, "ns:history:db:table:kind:history", &kToken_HistoryKind);
  if (err != 0) return NS_ERROR_FAILURE;

  err = mStore->StringToToken(mEnv, "URL", &kToken_URLColumn);
  if (err != 0) return NS_ERROR_FAILURE;

  err = mStore->StringToToken(mEnv, "Referrer", &kToken_ReferrerColumn);
  if (err != 0) return NS_ERROR_FAILURE;

  err = mStore->StringToToken(mEnv, "LastVisitDate", &kToken_LastVisitDateColumn);
  if (err != 0) return NS_ERROR_FAILURE;

  err = mStore->StringToToken(mEnv, "Name", &kToken_NameColumn);
  if (err != 0) return NS_ERROR_FAILURE;

  return NS_OK;
}


nsresult
nsGlobalHistory::CloseDB()
{
  mdb_err err;

  if (mTable)
    mTable->CutStrongRef(mEnv);

  if (mStore) {
    // Commit all changes here.
    nsMdbPtr<nsIMdbThumb> thumb(mEnv);
    err = mStore->SessionCommit(mEnv, getter_Acquires(thumb));
    if (err == 0) {
      mdb_count total;
      mdb_count current;
      mdb_bool done;
      mdb_bool broken;

      do {
        err = thumb->DoMore(mEnv, &total, &current, &done, &broken);
      } while ((err == 0) && !broken && !done);
    }

    mStore->CutStrongRef(mEnv);
  }

  if (mEnv)
    mEnv->CutStrongRef(mEnv /* XXX */);

  return NS_OK;
}



nsresult
nsGlobalHistory::NotifyAssert(nsIRDFResource* aSource,
                              nsIRDFResource* aProperty,
                              nsIRDFNode* aValue)
{
  nsresult rv;

  if (mObservers) {
    PRUint32 count;
    rv = mObservers->Count(&count);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = 0; i < PRInt32(count); ++i) {
      nsIRDFObserver* observer = NS_STATIC_CAST(nsIRDFObserver*, mObservers->ElementAt(i));

      NS_ASSERTION(observer != nsnull, "null ptr");
      if (! observer)
        continue;

      rv = observer->OnAssert(aSource, aProperty, aValue);
      NS_RELEASE(observer);
    }
  }

  return NS_OK;
}


nsresult
nsGlobalHistory::NotifyChange(nsIRDFResource* aSource,
                              nsIRDFResource* aProperty,
                              nsIRDFNode* aOldValue,
                              nsIRDFNode* aNewValue)
{
  nsresult rv;

  if (mObservers) {
    PRUint32 count;
    rv = mObservers->Count(&count);
    if (NS_FAILED(rv)) return rv;

    for (PRInt32 i = 0; i < PRInt32(count); ++i) {
      nsIRDFObserver* observer = NS_STATIC_CAST(nsIRDFObserver*, mObservers->ElementAt(i));

      NS_ASSERTION(observer != nsnull, "null ptr");
      if (! observer)
        continue;

      rv = observer->OnChange(aSource, aProperty, aOldValue, aNewValue);
      NS_RELEASE(observer);
    }
  }

  return NS_OK;
}




////////////////////////////////////////////////////////////////////////
// Component Exports

extern "C" PR_IMPLEMENT(nsresult)
NSGetFactory(nsISupports* aServiceMgr,
             const nsCID &aClass,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aFactory)
{
  NS_PRECONDITION(aFactory != nsnull, "null ptr");
  if (! aFactory)
    return NS_ERROR_NULL_POINTER;

  nsIGenericFactory::ConstructorProcPtr constructor;

  if (aClass.Equals(kGlobalHistoryCID)) {
    constructor = NS_NewGlobalHistory;
  }
  else {
    *aFactory = nsnull;
    return NS_NOINTERFACE; // XXX
  }

  nsresult rv;
  NS_WITH_SERVICE1(nsIComponentManager, compMgr, aServiceMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIGenericFactory> factory;
  rv = compMgr->CreateInstance(kGenericFactoryCID,
                               nsnull,
                               nsIGenericFactory::GetIID(),
                               getter_AddRefs(factory));

  if (NS_FAILED(rv)) return rv;

  rv = factory->SetConstructor(constructor);
  if (NS_FAILED(rv)) return rv;

  *aFactory = factory;
  NS_ADDREF(*aFactory);
  return NS_OK;
}



extern "C" PR_IMPLEMENT(nsresult)
NSRegisterSelf(nsISupports* aServMgr , const char* aPath)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  NS_WITH_SERVICE1(nsIComponentManager, compMgr, servMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->RegisterComponent(kGlobalHistoryCID, "Global History",
                                  NS_GLOBALHISTORY_PROGID,
                                  aPath, PR_TRUE, PR_TRUE);

  rv = compMgr->RegisterComponent(kGlobalHistoryCID, "Global History",
                                  NS_GLOBALHISTORY_DATASOURCE_PROGID,
                                  aPath, PR_TRUE, PR_TRUE);

  return NS_OK;
}



extern "C" PR_IMPLEMENT(nsresult)
NSUnregisterSelf(nsISupports* aServMgr, const char* aPath)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  NS_WITH_SERVICE1(nsIComponentManager, compMgr, servMgr, kComponentManagerCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = compMgr->UnregisterComponent(kGlobalHistoryCID, aPath);

  return NS_OK;
}


