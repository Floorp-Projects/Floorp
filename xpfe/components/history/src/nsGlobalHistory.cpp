/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author(s):
 *   Chris Waterson <waterson@netscape.com>
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

/*

  A global browser history implementation that also supports the RDF
  datasource interface.

  TODO

  1) Hook up Assert() etc. so that we can delete stuff.

*/

#include "nsIFileSpec.h"
#include "nsCRT.h"
#include "nsFileStream.h"
#include "nsIEnumerator.h"
#include "nsIGenericFactory.h"
#include "nsIGlobalHistory.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFService.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsEnumeratorUtils.h"
#include "nsRDFCID.h"
#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "plhash.h"
#include "plstr.h"
#include "prprf.h"
#include "prtime.h"
#include "rdf.h"
#include "nsIRDFRemoteDataSource.h"

#include "nsMdbPtr.h"
#include "nsInt64.h"
#include "nsMorkCID.h"
#include "nsIMdbFactoryFactory.h"
#include "mdb.h"

#include "nsIPref.h"

#ifdef DEBUG_sspitzer
#define DEBUG_LAST_PAGE_VISITED 1
#endif /* DEBUG_sspitzer */

#define BROWSER_HISTORY_LAST_PAGE_VISITED_PREF "browser.history.last_page_visited"

//----------------------------------------------------------------------
//
// CIDs

static NS_DEFINE_CID(kRDFServiceCID,        NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kPrefCID,              NS_PREF_CID);

static nsresult
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

//----------------------------------------------------------------------

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

//----------------------------------------------------------------------
//
//  nsMdbTableEnumerator
//
//    An nsISimpleEnumerator implementation that returns the value of
//    a column as an nsISupports. Allows for some simple selection.
//

class nsMdbTableEnumerator : public nsISimpleEnumerator
{
protected:
  nsIMdbEnv*   mEnv;
  nsIMdbTable* mTable;

  nsIMdbTableRowCursor* mCursor;
  nsIMdbRow*            mCurrent;

  nsMdbTableEnumerator();
  virtual ~nsMdbTableEnumerator();

public:
  // nsISupports methods
  NS_DECL_ISUPPORTS

  // nsISimpleEnumeratorMethods
  NS_IMETHOD HasMoreElements(PRBool* _result);
  NS_IMETHOD GetNext(nsISupports** _result);

  // Implementation methods
  virtual nsresult Init(nsIMdbEnv* aEnv, nsIMdbTable* aTable);

protected:
  virtual PRBool   IsResult(nsIMdbRow* aRow) = 0;
  virtual nsresult ConvertToISupports(nsIMdbRow* aRow, nsISupports** aResult) = 0;
};

//----------------------------------------------------------------------

nsMdbTableEnumerator::nsMdbTableEnumerator()
  : mEnv(nsnull), mTable(nsnull), mCursor(nsnull), mCurrent(nsnull)
{
  NS_INIT_REFCNT();
}


nsresult
nsMdbTableEnumerator::Init(nsIMdbEnv* aEnv,
                           nsIMdbTable* aTable)
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

  mdb_err err;
  err = mTable->GetTableRowCursor(mEnv, -1, &mCursor);
  if (err != 0) return NS_ERROR_FAILURE;

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
}


NS_IMPL_ISUPPORTS(nsMdbTableEnumerator, NS_GET_IID(nsISimpleEnumerator));

NS_IMETHODIMP
nsMdbTableEnumerator::HasMoreElements(PRBool* _result)
{
  if (! mCurrent) {
    mdb_err err;

    while (1) {
      mdb_pos pos;
      err = mCursor->NextRow(mEnv, &mCurrent, &pos);
      if (err != 0) return NS_ERROR_FAILURE;

      // If there are no more rows, then bail.
      if (! mCurrent)
        break;

      // If this is a result, the stop.
      if (IsResult(mCurrent))
        break;

      // Otherwise, drop the ref to the row we retrieved, and continue
      // on to the next one.
      mCurrent->CutStrongRef(mEnv);
      mCurrent = nsnull;
    }
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

  rv = ConvertToISupports(mCurrent, _result);

  mCurrent->CutStrongRef(mEnv);
  mCurrent = nsnull;

  return rv;
}


//----------------------------------------------------------------------
//
// nsGlobalHistory
//
//   This class is the browser's implementation of the
//   nsIGlobalHistory interface.
//

class nsGlobalHistory : public nsIGlobalHistory,
                        public nsIRDFDataSource,
                        public nsIRDFRemoteDataSource
{
public:
  // nsISupports methods 
  NS_DECL_ISUPPORTS

  // nsIGlobalHistory
  NS_DECL_NSIGLOBALHISTORY

  // nsIRDFDataSource
  NS_DECL_NSIRDFDATASOURCE

  // nsIRDFRemoteDataSource
  NS_DECL_NSIRDFREMOTEDATASOURCE

protected:
  nsGlobalHistory(void);
  virtual ~nsGlobalHistory();

  friend NS_IMETHODIMP
  NS_NewGlobalHistory(nsISupports* aOuter, REFNSIID aIID, void** aResult);

  // Implementation Methods
  nsresult Init();
  nsresult OpenDB();
  nsresult CreateTokens();
  nsresult CloseDB();

  PRBool IsURLInHistory(nsIRDFResource* aResource);

  // N.B., these are MDB interfaces, _not_ XPCOM interfaces.
  nsIMdbEnv* mEnv;         // OWNER
  nsIMdbStore* mStore;     // OWNER
  nsIMdbTable* mTable;     // OWNER

  nsresult SaveLastPageVisited(const char *);

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

  class URLEnumerator : public nsMdbTableEnumerator
  {
  protected:
    mdb_column mURLColumn;
    mdb_column mSelectColumn;
    void*      mSelectValue;
    PRInt32    mSelectValueLen;

    virtual ~URLEnumerator();

  public:
    URLEnumerator(mdb_column aURLColumn,
                  mdb_column aSelectColumn = mdb_column(0),
                  void* aSelectValue = nsnull,
                  PRInt32 aSelectValueLen = 0) :
      mURLColumn(aURLColumn),
      mSelectColumn(aSelectColumn),
      mSelectValue(aSelectValue),
      mSelectValueLen(aSelectValueLen)
    {}

  protected:
    virtual PRBool   IsResult(nsIMdbRow* aRow);
    virtual nsresult ConvertToISupports(nsIMdbRow* aRow, nsISupports** aResult);
  };

  friend class URLEnumerator;
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


//----------------------------------------------------------------------
//
// nsGlobalHistory
//
//   ctor dtor etc.
//


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


//----------------------------------------------------------------------
//
// nsGlobalHistory
//
//   nsISupports methods

NS_IMPL_ADDREF(nsGlobalHistory);
NS_IMPL_RELEASE(nsGlobalHistory);
NS_IMPL_QUERY_INTERFACE3(nsGlobalHistory, nsIGlobalHistory, nsIRDFDataSource, nsIRDFRemoteDataSource)

//----------------------------------------------------------------------
//
// nsGlobalHistory
//
//   nsIGlobalHistory methods
//


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

  // Sanity check the URL
  PRInt32 len = PL_strlen(aURL);
  NS_ASSERTION(len != 0, "no URL");
  if (! len)
    return NS_ERROR_INVALID_ARG;

  rv = SaveLastPageVisited(aURL);
  if (NS_FAILED(rv)) return rv;

  // Okay, it's good. See if we've already got it in the database.
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
    err = row->AliasCellYarn(mEnv, kToken_LastVisitDateColumn, &yarn);
    if (err != 0) return NS_ERROR_FAILURE;

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
  err = row->AliasCellYarn(mEnv, kToken_NameColumn, &yarn);
  if (err != 0) return NS_ERROR_FAILURE;

  nsresult rv;
  nsCOMPtr<nsIRDFLiteral> oldname;
  if (yarn.mYarn_Fill) {
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
  NS_PRECONDITION(aURL != nsnull, "null ptr");
  if (! aURL)
    return NS_ERROR_NULL_POINTER;

  nsresult rv;
  mdb_err err;

  PRInt32 len = PL_strlen(aURL);
  mdbYarn yarn = { (void*) aURL, len, len, 0, 0, nsnull };

  mdbOid rowId;
  nsMdbPtr<nsIMdbRow> row(mEnv);
  err = mStore->FindRow(mEnv, kToken_HistoryRowScope, kToken_URLColumn, &yarn,
                        &rowId, getter_Acquires(row));

  if (err != 0) return NS_ERROR_FAILURE;

  if (row) {
    err = row->AliasCellYarn(mEnv, kToken_LastVisitDateColumn, &yarn);
    if (err != 0) return NS_ERROR_FAILURE;

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

nsresult
nsGlobalHistory::SaveLastPageVisited(const char *aURL)
{
  nsresult rv;

  if (!aURL) return NS_ERROR_FAILURE;
  
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
  if (NS_FAILED(rv)) return rv;

  rv = prefs->SetCharPref(BROWSER_HISTORY_LAST_PAGE_VISITED_PREF, aURL);

#ifdef DEBUG_LAST_PAGE_VISITED
  printf("XXX saving last page visited as: %s\n", aURL);
#endif /* DEBUG_LAST_PAGE_VISITED */

  return rv;
}

NS_IMETHODIMP
nsGlobalHistory::GetLastPageVisted(char **_retval)
{ 
  nsresult rv;

  if (!_retval) return NS_ERROR_NULL_POINTER;

  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv);
  if (NS_FAILED(rv)) return rv;

  nsXPIDLCString lastPageVisited;
  rv = prefs->CopyCharPref(BROWSER_HISTORY_LAST_PAGE_VISITED_PREF, getter_Copies(lastPageVisited));
  if (NS_FAILED(rv)) return rv;

  *_retval = nsCRT::strdup((const char *)lastPageVisited);

#ifdef DEBUG_LAST_PAGE_VISITED
  printf("XXX getting last page visited as: %s\n", (const char *)lastPageVisited);
#endif /* DEBUG_LAST_PAGE_VISITED */

  return NS_OK;
}

//----------------------------------------------------------------------
//
// nsGlobalHistory
//
//   nsIRDFDataSource methods

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
  NS_PRECONDITION(aProperty != nsnull, "null ptr");
  if (! aProperty)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(aTarget != nsnull, "null ptr");
  if (! aTarget)
    return NS_ERROR_NULL_POINTER;

  nsresult rv;

  *aSource = nsnull;

  if (aProperty == kNC_URL) {
    // See if we have the row...

    // XXX We could be more forgiving here, and check for literal
    // values as well.
    nsCOMPtr<nsIRDFResource> target = do_QueryInterface(aTarget);
    if (! target)
      return NS_RDF_NO_VALUE;

    mdb_err err;

    nsXPIDLCString uri;
    rv = target->GetValueConst(getter_Shares(uri));
    if (NS_FAILED(rv)) return rv;

    PRInt32 len = PL_strlen(uri);
    mdbYarn yarn = { (void*)NS_STATIC_CAST(const char*, uri), len, len, 0, 0, nsnull };

    mdbOid rowId;
    nsMdbPtr<nsIMdbRow> row(mEnv);
    err = mStore->FindRow(mEnv, kToken_HistoryRowScope, kToken_URLColumn, &yarn, &rowId, getter_Acquires(row));
    if (err != 0) return NS_ERROR_FAILURE;

    if (row) {
      // ...if so, return the URL. kNC_URL is just a self-referring arc.
      return CallQueryInterface(aTarget, aSource);
    }
  }
  else if ((aProperty == kNC_Date) ||
           (aProperty == kNC_VisitCount) ||
           (aProperty == kNC_Name) ||
           (aProperty == kNC_Referrer)) {
    // Call GetSources() and return the first one we find.
    nsCOMPtr<nsISimpleEnumerator> sources;
    rv = GetSources(aProperty, aTarget, aTruthValue, getter_AddRefs(sources));
    if (NS_FAILED(rv)) return rv;

    PRBool hasMore;
    rv = sources->HasMoreElements(&hasMore);
    if (NS_FAILED(rv)) return rv;

    if (hasMore) {
      nsCOMPtr<nsISupports> isupports;
      rv = sources->GetNext(getter_AddRefs(isupports));
      if (NS_FAILED(rv)) return rv;

      return CallQueryInterface(isupports, aSource);
    }
  }

  return NS_RDF_NO_VALUE;  
}

NS_IMETHODIMP
nsGlobalHistory::GetSources(nsIRDFResource* aProperty,
                            nsIRDFNode* aTarget,
                            PRBool aTruthValue,
                            nsISimpleEnumerator** aSources)
{
  // XXX TODO: make sure each URL in history is connected back to
  // NC:HistoryRoot.
  NS_PRECONDITION(aProperty != nsnull, "null ptr");
  if (! aProperty)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(aTarget != nsnull, "null ptr");
  if (! aTarget)
    return NS_ERROR_NULL_POINTER;

  nsresult rv;

  if (aProperty == kNC_URL) {
    // Call GetSource() and return a singleton enumerator for the URL.
    nsCOMPtr<nsIRDFResource> source;
    rv = GetSource(aProperty, aTarget, aTruthValue, getter_AddRefs(source));
    if (NS_FAILED(rv)) return rv;

    return NS_NewSingletonEnumerator(aSources, source);
  }
  else {
    // See if aProperty is something we understand, and create an
    // URLEnumerator to select URLs with the appropriate value.

    mdb_column col = 0; // == "not a property that I grok"
    void* value = nsnull;
    PRInt32 len = 0;

    if (aProperty == kNC_Date) {
      nsCOMPtr<nsIRDFDate> date = do_QueryInterface(aTarget);
      if (date) {
        PRInt64 n;

        rv = date->GetValue(&n);
        if (NS_FAILED(rv)) return rv;

        char buf[64];
        rv = PRInt64ToChars(n, buf, sizeof(buf));
        if (NS_FAILED(rv)) return rv;

        len = PL_strlen(buf);
        value = nsMemory::Alloc(len + 1);
        if (! value)
          return NS_ERROR_OUT_OF_MEMORY;

        PL_strcpy(buf, NS_STATIC_CAST(char*, value));
        col = kToken_LastVisitDateColumn;
      }
    }
    else if (aProperty == kNC_VisitCount) {
      NS_NOTYETIMPLEMENTED("sorry");
    }
    else if (aProperty == kNC_Name) {
      nsCOMPtr<nsIRDFLiteral> name = do_QueryInterface(aTarget);
      if (name) {
        PRUnichar* p;
        rv = name->GetValue(&p);
        if (NS_FAILED(rv)) return rv;

        len = nsCRT::strlen(p) * sizeof(PRUnichar);
        value = p;

        col = kToken_NameColumn;
      }
    }
    else if (aProperty == kNC_Referrer) {
      col = kToken_ReferrerColumn;
      nsCOMPtr<nsIRDFResource> referrer = do_QueryInterface(aTarget);
      if (referrer) {
        char* p;
        rv = referrer->GetValue(&p);
        if (NS_FAILED(rv)) return rv;

        len = PL_strlen(p);
        value = p;

        col = kToken_ReferrerColumn;
      }
    }

    if (col) {
      // The URLEnumerator takes ownership of the bytes allocated in |value|.
      URLEnumerator* result = new URLEnumerator(kToken_URLColumn, col, value, len);
      if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

      rv = result->Init(mEnv, mTable);
      if (NS_FAILED(rv)) return rv;

      *aSources = result;
      NS_ADDREF(*aSources);
      return NS_OK;
    }
  }

  return NS_NewEmptyEnumerator(aSources);
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

  // Initialize return value.
  *aTarget = nsnull;

  // Only "positive" assertions here.
  if (! aTruthValue)
    return NS_RDF_NO_VALUE;

  if ((aSource == kNC_HistoryRoot) && (aProperty == kNC_child)) {
    // If they're asking for all the children of the HistoryRoot, call
    // through to GetTargets() and return the first one.
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

    return CallQueryInterface(isupports, aTarget);
  }
  else if ((aProperty == kNC_Date) ||
           (aProperty == kNC_VisitCount) ||
           (aProperty == kNC_Name) ||
           (aProperty == kNC_Referrer) ||
           (aProperty == kNC_URL)) {
    // It's a real property! Okay, first we'll get the row...
    mdb_err err;

    const char* uri;
    rv = aSource->GetValueConst(&uri);
    if (NS_FAILED(rv)) return rv;

    PRInt32 len = PL_strlen(uri);
    mdbYarn yarn = { NS_CONST_CAST(void*, NS_STATIC_CAST(const void*, uri)), len, len, 0, 0, nsnull };

    mdbOid rowId;
    nsMdbPtr<nsIMdbRow> row(mEnv);
    err = mStore->FindRow(mEnv, kToken_HistoryRowScope, kToken_URLColumn, &yarn, &rowId, getter_Acquires(row));
    if (err != 0) return NS_ERROR_FAILURE;

    if (!row) return NS_RDF_NO_VALUE;

    // ...and then depending on the property they want, we'll pull the
    // cell they want out of it.
    if (aProperty == kNC_Date) {
      // Last visit date
      err = row->AliasCellYarn(mEnv, kToken_LastVisitDateColumn, &yarn);
      if (err != 0) return NS_ERROR_FAILURE;

      PRInt64 i;
      rv = CharsToPRInt64((const char*) yarn.mYarn_Buf, yarn.mYarn_Fill, &i);
      if (NS_FAILED(rv)) return rv;

      nsCOMPtr<nsIRDFDate> date;
      rv = gRDFService->GetDateLiteral(i, getter_AddRefs(date));
      if (NS_FAILED(rv)) return rv;

      return CallQueryInterface(date, aTarget);
    }
    else if (aProperty == kNC_VisitCount) {
      // Visit count
      NS_NOTYETIMPLEMENTED("sorry");
      rv = NS_ERROR_NOT_IMPLEMENTED;
    }
    else if (aProperty == kNC_Name) {
      // Site name (i.e., page title)
      err = row->AliasCellYarn(mEnv, kToken_NameColumn, &yarn);
      if (err != 0) return NS_ERROR_FAILURE;

      // Can't alias, because we don't store the terminating null
      // character in the db.
      len = yarn.mYarn_Fill / sizeof(PRUnichar);
      nsAutoString str((const PRUnichar*) yarn.mYarn_Buf, len);

      nsCOMPtr<nsIRDFLiteral> name;
      rv = gRDFService->GetLiteral(str.GetUnicode(), getter_AddRefs(name));
      if (NS_FAILED(rv)) return rv;

      return CallQueryInterface(name, aTarget);
    }
    else if (aProperty == kNC_Referrer) {
      // Referrer field
      err = row->AliasCellYarn(mEnv, kToken_ReferrerColumn, &yarn);
      if (err != 0) return NS_ERROR_FAILURE;

      // XXX Could probably alias the buffer here to avoid copy
      nsCAutoString str((const char*) yarn.mYarn_Buf, yarn.mYarn_Fill);

      nsCOMPtr<nsIRDFResource> referrer;
      rv = gRDFService->GetResource((const char*) str, getter_AddRefs(referrer));
      if (NS_FAILED(rv)) return rv;

      return CallQueryInterface(referrer, aTarget);
    }
    else if (aProperty == kNC_URL) {
      // URL. This is just a self-referring arc.
      return CallQueryInterface(aSource, aTarget);
    }
    else {
      NS_NOTREACHED("huh, how'd I get here?");
    }
  }

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

  if (aTruthValue) {
    if ((aSource == kNC_HistoryRoot) && (aProperty == kNC_child) && (aTruthValue)) {
      URLEnumerator* result = new URLEnumerator(kToken_URLColumn);
      if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

      nsresult rv;
      rv = result->Init(mEnv, mTable);
      if (NS_FAILED(rv)) return rv;

      *aTargets = result;
      NS_ADDREF(*aTargets);
      return NS_OK;
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
  NS_PRECONDITION(aSource != nsnull, "null ptr");
  if (! aSource)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(aProperty != nsnull, "null ptr");
  if (! aProperty)
    return NS_ERROR_NULL_POINTER;

  NS_PRECONDITION(aTarget != nsnull, "null ptr");
  if (! aTarget)
    return NS_ERROR_NULL_POINTER;

  // Only "positive" assertions here.
  if (aTruthValue) {
    // Do |GetTargets()| and grovel through the results to see if we
    // have the assertion.
    //
    // XXX *AHEM*, this could be implemented much more efficiently...
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
nsGlobalHistory::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
  NS_PRECONDITION(aNode != nsnull, "null ptr");
  if (! aNode)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(aNode);
  if (resource && IsURLInHistory(resource)) {
    *result = (aArc == kNC_child);
  }
  else {
    *result = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsGlobalHistory::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result)
{
  NS_PRECONDITION(aSource != nsnull, "null ptr");
  if (! aSource)
    return NS_ERROR_NULL_POINTER;

  if ((aSource == kNC_HistoryRoot) ||
      (aSource == kNC_HistoryBySite) ||
      (aSource == kNC_HistoryByDate)) {
    *result = (aArc == kNC_child);
  }
  else if (IsURLInHistory(aSource)) {
    // If the URL is in the history, then it'll have all the
    // appropriate attributes.
    *result = (aArc == kNC_Date ||
               aArc == kNC_VisitCount ||
               aArc == kNC_Name ||
               aArc == kNC_Referrer);
  }
  else {
    *result = PR_FALSE;
  }
  return NS_OK; 
}

NS_IMETHODIMP
nsGlobalHistory::ArcLabelsIn(nsIRDFNode* aNode,
                             nsISimpleEnumerator** aLabels)
{
  NS_PRECONDITION(aNode != nsnull, "null ptr");
  if (! aNode)
    return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(aNode);
  if (resource && IsURLInHistory(resource)) {
    return NS_NewSingletonEnumerator(aLabels, kNC_child);
  }
  else {
    return NS_NewEmptyEnumerator(aLabels);
  }
}

NS_IMETHODIMP
nsGlobalHistory::ArcLabelsOut(nsIRDFResource* aSource,
                              nsISimpleEnumerator** aLabels)
{
  NS_PRECONDITION(aSource != nsnull, "null ptr");
  if (! aSource)
    return NS_ERROR_NULL_POINTER;

  nsresult rv;

  if ((aSource == kNC_HistoryRoot) ||
      (aSource == kNC_HistoryBySite) ||
      (aSource == kNC_HistoryByDate)) {
    return NS_NewSingletonEnumerator(aLabels, kNC_child);
  }
  else if (IsURLInHistory(aSource)) {
    // If the URL is in the history, then it'll have all the
    // appropriate attributes.
    nsCOMPtr<nsISupportsArray> array;
    rv = NS_NewISupportsArray(getter_AddRefs(array));
    if (NS_FAILED(rv)) return rv;

    array->AppendElement(kNC_Date);
    array->AppendElement(kNC_VisitCount);
    array->AppendElement(kNC_Name);
    array->AppendElement(kNC_Referrer);

    return NS_NewArrayEnumerator(aLabels, array);
  }
  else {
    return NS_NewEmptyEnumerator(aLabels);
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
  URLEnumerator* result = new URLEnumerator(kToken_URLColumn);
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv;
  rv = result->Init(mEnv, mTable);
  if (NS_FAILED(rv)) return rv;

  *aResult = result;
  NS_ADDREF(*aResult);
  return NS_OK;
}



////////////////////////////////////////////////////////////////////////
// nsIRDFRemoteDataSource

NS_IMETHODIMP
nsGlobalHistory::GetLoaded(PRBool* _result)
{
    *_result = PR_TRUE;
    return NS_OK;
}



NS_IMETHODIMP
nsGlobalHistory::Init(const char* aURI)
{
	return(NS_OK);
}



NS_IMETHODIMP
nsGlobalHistory::Refresh(PRBool aBlocking)
{
	return(NS_OK);
}

NS_IMETHODIMP
nsGlobalHistory::Flush()
{
	nsMdbPtr<nsIMdbThumb> thumb(mEnv);
	mdb_err err;

	err = mStore->LargeCommit(mEnv, getter_Acquires(thumb));
	if (err != 0) return NS_ERROR_FAILURE;

	mdb_count total;
	mdb_count current;
	mdb_bool done;
	mdb_bool broken;

	do
	{
		err = thumb->DoMore(mEnv, &total, &current, &done, &broken);
	} while ((err == 0) && !broken && !done);

	if ((err != 0) || !done) return NS_ERROR_FAILURE;

	return NS_OK;
}



//----------------------------------------------------------------------
//
// nsGlobalHistory
//
//   Miscellaneous implementation methods
//

nsresult
nsGlobalHistory::Init()
{
  nsresult rv;

  if (gRefCnt++ == 0) {
    rv = nsServiceManager::GetService(kRDFServiceCID,
                                      NS_GET_IID(nsIRDFService),
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

  nsCOMPtr <nsIFile> historyFile;
  rv = NS_GetSpecialDirectory(NS_APP_HISTORY_50_FILE, getter_AddRefs(historyFile));
  if (NS_FAILED(rv)) return rv;

  static NS_DEFINE_CID(kMorkCID, NS_MORK_CID);
  nsCOMPtr<nsIMdbFactoryFactory> factoryfactory;
  rv = nsComponentManager::CreateInstance(kMorkCID,
                                          nsnull,
                                          NS_GET_IID(nsIMdbFactoryFactory),
                                          getter_AddRefs(factoryfactory));
  if (NS_FAILED(rv)) return rv;

  // Leaving XPCOM, entering MDB. They may look like XPCOM interfaces,
  // but they're not. The 'factory' is an interface; however, it isn't
  // reference counted. So no, this isn't a leak.
  nsIMdbFactory* factory;
  rv = factoryfactory->GetMdbFactory(&factory);
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create mork factory factory");
  if (NS_FAILED(rv)) return rv;

  mdb_err err;

  err = factory->MakeEnv(nsnull, &mEnv);
  NS_ASSERTION((err == 0), "unable to create mdb env");
  if (err != 0) return NS_ERROR_FAILURE;

  nsIMdbHeap* dbHeap = 0;
  mdb_bool dbFrozen = mdbBool_kFalse; // not readonly, we want modifiable
  PRBool exists;

  nsXPIDLCString filePath;
  rv = historyFile->GetPath(getter_Copies(filePath));
  if (NS_FAILED(rv)) return rv;
    
  rv = historyFile->Exists(&exists);
  if (NS_FAILED(rv)) return rv;
  
  if (exists) {
    mdb_bool canopen = 0;
    mdbYarn outfmt = { nsnull, 0, 0, 0, 0, nsnull };

    nsMdbPtr<nsIMdbFile> oldFile(mEnv); // ensures file is released
    err = factory->OpenOldFile(mEnv, dbHeap, filePath,
                               dbFrozen, getter_Acquires(oldFile));

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

    nsMdbPtr<nsIMdbFile> newFile(mEnv); // ensures file is released
    err = factory->CreateNewFile(mEnv, dbHeap, filePath,
                                 getter_Acquires(newFile));

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
    mEnv->CloseMdbObject(mEnv /* XXX */);

  return NS_OK;
}


PRBool
nsGlobalHistory::IsURLInHistory(nsIRDFResource* aResource)
{
  nsresult rv;
  mdb_err err;

  const char* url;
  rv = aResource->GetValueConst(&url);
  if (NS_FAILED(rv)) return PR_FALSE;

  PRInt32 len = PL_strlen(url);
  mdbYarn yarn = { (void*) url, len, len, 0, 0, nsnull };

  mdbOid rowId;
  nsMdbPtr<nsIMdbRow> row(mEnv);
  err = mStore->FindRow(mEnv, kToken_HistoryRowScope, kToken_URLColumn, &yarn,
                        &rowId, getter_Acquires(row));

  if (err != 0) return PR_FALSE;

  return row ? PR_TRUE : PR_FALSE;
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

      rv = observer->OnAssert(this, aSource, aProperty, aValue);
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

      rv = observer->OnChange(this, aSource, aProperty, aOldValue, aNewValue);
      NS_RELEASE(observer);
    }
  }

  return NS_OK;
}


//----------------------------------------------------------------------
//
// nsGlobalHistory::URLEnumerator
//
//   Implementation

nsGlobalHistory::URLEnumerator::~URLEnumerator()
{
  nsMemory::Free(mSelectValue);
}

PRBool
nsGlobalHistory::URLEnumerator::IsResult(nsIMdbRow* aRow)
{
  if (mSelectColumn) {
    mdb_err err;

    mdbYarn yarn;
    err = mCurrent->AliasCellYarn(mEnv, mURLColumn, &yarn);
    if (err != 0) return PR_FALSE;

    // Do bitwise comparison
    PRInt32 count = PRInt32(yarn.mYarn_Fill);
    if (count != mSelectValueLen)
      return PR_FALSE;

    const char* p = (const char*) yarn.mYarn_Buf;
    const char* q = (const char*) mSelectValue;

    while (--count >= 0) {
      if (*p++ != *q++)
        return PR_FALSE;
    }
  }

  return PR_TRUE;
}

nsresult
nsGlobalHistory::URLEnumerator::ConvertToISupports(nsIMdbRow* aRow, nsISupports** aResult)
{
  mdb_err err;

  mdbYarn yarn;
  err = mCurrent->AliasCellYarn(mEnv, mURLColumn, &yarn);
  if (err != 0) return NS_ERROR_FAILURE;

  // Since the URLEnumerator always returns the value of the URL
  // column, we create an RDF resource.
  nsCAutoString uri((const char*) yarn.mYarn_Buf, yarn.mYarn_Fill);

  nsresult rv;
  nsCOMPtr<nsIRDFResource> resource;
  rv = gRDFService->GetResource(uri, getter_AddRefs(resource));
  if (NS_FAILED(rv)) return rv;

  *aResult = resource;
  NS_ADDREF(*aResult);
  return NS_OK;
}


//----------------------------------------------------------------------
//
// Global History Module
//

static nsModuleComponentInfo gHistoryComponents[] =
{
  { "Global History", NS_GLOBALHISTORY_CID, NS_GLOBALHISTORY_PROGID,
    NS_NewGlobalHistory,
  },
  { "Global History", NS_GLOBALHISTORY_CID, NS_GLOBALHISTORY_DATASOURCE_PROGID,
    NS_NewGlobalHistory,
  }
};

NS_IMPL_NSGETMODULE("nsGlobalHistoryModule", gHistoryComponents)
