/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Joe Hewitt <hewitt@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*

  A global browser history implementation that also supports the RDF
  datasource interface.

  TODO

  1) Hook up Assert() etc. so that we can delete stuff.

*/
#include "nsNetUtil.h"
#include "nsGlobalHistory.h"
#include "nsIFileSpec.h"
#include "nsCRT.h"
#include "nsFileStream.h"
#include "nsIEnumerator.h"
#include "nsIServiceManager.h"
#include "nsEnumeratorUtils.h"
#include "nsRDFCID.h"
#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsXPIDLString.h"
#include "plhash.h"
#include "plstr.h"
#include "prprf.h"
#include "prtime.h"
#include "rdf.h"
#include "nsQuickSort.h"
#include "nsIIOService.h"

#include "nsIURL.h"
#include "nsNetCID.h"

#include "nsInt64.h"
#include "nsMorkCID.h"
#include "nsIMdbFactoryFactory.h"

#include "nsIPref.h"
#include "nsIObserverService.h"

PRInt32 nsGlobalHistory::gRefCnt;
nsIRDFService* nsGlobalHistory::gRDFService;
nsIRDFResource* nsGlobalHistory::kNC_Page;
nsIRDFResource* nsGlobalHistory::kNC_Date;
nsIRDFResource* nsGlobalHistory::kNC_FirstVisitDate;
nsIRDFResource* nsGlobalHistory::kNC_VisitCount;
nsIRDFResource* nsGlobalHistory::kNC_AgeInDays;
nsIRDFResource* nsGlobalHistory::kNC_Name;
nsIRDFResource* nsGlobalHistory::kNC_NameSort;
nsIRDFResource* nsGlobalHistory::kNC_Hostname;
nsIRDFResource* nsGlobalHistory::kNC_Referrer;
nsIRDFResource* nsGlobalHistory::kNC_child;
nsIRDFResource* nsGlobalHistory::kNC_URL;
nsIRDFResource* nsGlobalHistory::kNC_HistoryRoot;
nsIRDFResource* nsGlobalHistory::kNC_HistoryByDate;

#define PREF_BROWSER_HISTORY_LAST_PAGE_VISITED "browser.history.last_page_visited"
#define PREF_BROWSER_HISTORY_EXPIRE_DAYS "browser.history_expire_days"
#define PREF_AUTOCOMPLETE_ENABLED "browser.urlbar.autocomplete.enabled"

#define FIND_BY_AGEINDAYS_PREFIX "find:datasource=history&match=AgeInDays&method="

// sync history every 10 seconds
#define HISTORY_SYNC_TIMEOUT 10 * PR_MSEC_PER_SEC
//#define HISTORY_SYNC_TIMEOUT 3000 // every 3 seconds - testing only!

// the value of mLastNow expires every 3 seconds
#define HISTORY_EXPIRE_NOW_TIMEOUT 3 * PR_MSEC_PER_SEC

#define MSECS_PER_DAY PR_MSEC_PER_SEC * 60 * 60 * 24

//----------------------------------------------------------------------
//
// CIDs

static NS_DEFINE_CID(kRDFServiceCID,        NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kPrefCID,              NS_PREF_CID);
static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

// closure structures for RemoveMatchingRows
struct matchExpiration_t {
  PRInt64 *expirationDate;
  nsGlobalHistory *history;
};

struct matchHost_t {
  const char *host;
  PRBool entireDomain;          // should we delete the entire domain?
  nsGlobalHistory *history;
  nsIURI* cachedUrl;
};

struct matchSearchTerm_t {
  nsIMdbEnv *env;
  nsIMdbStore *store;
  
  searchTerm *term;
  PRBool haveClosure;           // are the rest of the fields valid?
  PRInt64 now;
  PRInt32 intValue;
};

// simple token/value struct
class tokenPair {
public:
  tokenPair(const char *aName, PRUint32 aNameLen,
            const char *aValue, PRUint32 aValueLen) :
    tokenName(aName, aNameLen),
    tokenValue(aValue, aValueLen) {}
  nsDependentCString tokenName;
  nsDependentCString tokenValue;
};

// individual search term, pulled from token/value structs
class searchTerm {
public:
  searchTerm(const char* aDatasource, PRUint32 datasourceLen,
             const char *aProperty, PRUint32 aPropertyLen,
             const char* aMethod, PRUint32 methodLen,
             const char* aText, PRUint32 textLen):
    datasource(aDatasource, datasourceLen),
    property(aProperty, aPropertyLen),
    method(aMethod, methodLen)
  {
    // need to do UTF8-conversion/unescaping here, using
    // nsITextToSubURI
    text.AssignWithConversion(aText, textLen);
  }
  
  nsDependentCString datasource;  // should always be "history" ?
  nsDependentCString property;    // AgeInDays, Hostname, etc
  nsDependentCString method;      // is, isgreater, isless
  nsAutoString text;            // text to match
  rowMatchCallback match;      // matching callback if needed
};

// list of terms, plus an optional groupby column
struct searchQuery {
  nsVoidArray terms;            // array of searchTerms
  mdb_column groupBy;           // column to group by
};

static nsresult
PRInt64ToChars(const PRInt64& aValue, nsAWritableCString& aResult)
{
  // Convert an unsigned 64-bit value to a string of up to aSize
  // decimal digits, placed in aBuf.
  nsInt64 value(aValue);

  aResult.Truncate(0);

  if (value == nsInt64(0)) {
    aResult.Append('0');
  }

  while (value != nsInt64(0)) {
    PRInt32 ones = PRInt32(value % nsInt64(10));
    value /= nsInt64(10);

    if (ones <=9) 
      aResult.Insert(char('0' + ones), 0);
  }
  
  return NS_OK;
}

//----------------------------------------------------------------------

static nsresult
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

static PRTime
NormalizeTime(PRInt64 aTime)
{
  // normalize both now and date to midnight of the day they occur on
  PRExplodedTime explodedTime;
  PR_ExplodeTime(aTime, PR_LocalTimeParameters, &explodedTime);

  // set to midnight (0:00)
  explodedTime.tm_min =
    explodedTime.tm_hour =
    explodedTime.tm_sec =
    explodedTime.tm_usec = 0;

  return PR_ImplodeTime(&explodedTime);
}

// pass in a pre-normalized now and a date, and we'll find
// the difference since midnight on each of the days..
static PRInt32
GetAgeInDays(PRInt64 aNormalizedNow, PRInt64 aDate)
{
  PRInt64 dateMidnight = NormalizeTime(aDate);

  PRInt64 diff;
  LL_SUB(diff, aNormalizedNow, dateMidnight);

  // two-step process since I can't seem to load
  // MSECS_PER_DAY * PR_MSEC_PER_SEC into a PRInt64 at compile time
  PRInt64 msecPerSec;
  LL_I2L(msecPerSec, PR_MSEC_PER_SEC);
  PRInt64 ageInSeconds;
  LL_DIV(ageInSeconds, diff, msecPerSec);

  PRInt32 ageSec; LL_L2I(ageSec, ageInSeconds);
  
  PRInt64 msecPerDay;
  LL_I2L(msecPerDay, MSECS_PER_DAY);
  
  PRInt64 ageInDays;
  LL_DIV(ageInDays, ageInSeconds, msecPerDay);

  PRInt32 retval;
  LL_L2I(retval, ageInDays);
  return retval;
}


PRBool
nsGlobalHistory::MatchExpiration(nsIMdbRow *row, PRInt64* expirationDate)
{
  nsresult rv;
  
  PRInt64 lastVisitedTime;
  rv = GetRowValue(row, kToken_LastVisitDateColumn, &lastVisitedTime);

  if (NS_FAILED(rv)) 
    return PR_FALSE;
  
  return LL_CMP(lastVisitedTime, <, *expirationDate);
}

static PRBool
matchAgeInDaysCallback(nsIMdbRow *row, void *aClosure)
{
  matchSearchTerm_t *matchSearchTerm = (matchSearchTerm_t*)aClosure;
  const searchTerm *term = matchSearchTerm->term;
  nsIMdbEnv *env = matchSearchTerm->env;
  nsIMdbStore *store = matchSearchTerm->store;
  
  // fill in the rest of the closure if it's not filled in yet
  // this saves us from recalculating this stuff on every row
  if (!matchSearchTerm->haveClosure) {
    PRInt32 err;
    matchSearchTerm->intValue = term->text.ToInteger(&err);
    matchSearchTerm->now = NormalizeTime(PR_Now());
    if (err != 0) return PR_FALSE;
    matchSearchTerm->haveClosure = PR_TRUE;
  }
  
  // XXX convert the property to a column, get the column value

  PRInt64 rowDate;
  mdb_column column;
  mdb_err err = store->StringToToken(env, "LastVisitDate", &column);
  if (err != 0) return PR_FALSE;

  mdbYarn yarn;
  err = row->AliasCellYarn(env, column, &yarn);
  if (err != 0) return PR_FALSE;
  
  CharsToPRInt64((const char*)yarn.mYarn_Buf, yarn.mYarn_Fill, &rowDate);

  PRInt32 days = GetAgeInDays(matchSearchTerm->now, rowDate);
  
  if (term->method.Equals("is"))
    return (days == matchSearchTerm->intValue);
  else if (term->method.Equals("isgreater"))
    return (days >  matchSearchTerm->intValue);
  else if (term->method.Equals("isless"))
    return (days <  matchSearchTerm->intValue);
  
  return PR_FALSE;
}

static PRBool
matchExpirationCallback(nsIMdbRow *row, void *aClosure)
{
  matchExpiration_t *expires = (matchExpiration_t*)aClosure;
  return expires->history->MatchExpiration(row, expires->expirationDate);
}

static PRBool
matchAllCallback(nsIMdbRow *row, void *aClosure)
{
  return PR_TRUE;
}

static PRBool
matchHostCallback(nsIMdbRow *row, void *aClosure)
{
  matchHost_t *hostInfo = (matchHost_t*)aClosure;
  return hostInfo->history->MatchHost(row, hostInfo);
}
//----------------------------------------------------------------------

nsMdbTableEnumerator::nsMdbTableEnumerator()
  : mEnv(nsnull),
    mTable(nsnull),
    mCursor(nsnull),
    mCurrent(nsnull)
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


NS_IMPL_ISUPPORTS1(nsMdbTableEnumerator, nsISimpleEnumerator);

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
//   ctor dtor etc.
//


nsGlobalHistory::nsGlobalHistory()
  : mExpireDays(9), // make default be nine days
    mNowValid(PR_FALSE),
    mDirty(PR_FALSE),
    mEnv(nsnull),
    mStore(nsnull),
    mTable(nsnull)
{
  NS_INIT_REFCNT();
  LL_I2L(mFileSizeOnDisk, 0);
  
  // commonly used prefixes that should be chopped off all 
  // history and input urls before comparison

  mIgnoreSchemes.AppendString(NS_LITERAL_STRING("http://"));
  mIgnoreSchemes.AppendString(NS_LITERAL_STRING("https://"));
  mIgnoreSchemes.AppendString(NS_LITERAL_STRING("ftp://"));
  mIgnoreHostnames.AppendString(NS_LITERAL_STRING("www."));
  mIgnoreHostnames.AppendString(NS_LITERAL_STRING("ftp."));
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
    NS_IF_RELEASE(kNC_FirstVisitDate);
    NS_IF_RELEASE(kNC_VisitCount);
    NS_IF_RELEASE(kNC_AgeInDays);
    NS_IF_RELEASE(kNC_Name);
    NS_IF_RELEASE(kNC_NameSort);
    NS_IF_RELEASE(kNC_Hostname);
    NS_IF_RELEASE(kNC_Referrer);
    NS_IF_RELEASE(kNC_child);
    NS_IF_RELEASE(kNC_URL);
    NS_IF_RELEASE(kNC_HistoryRoot);
    NS_IF_RELEASE(kNC_HistoryByDate);
  }

  if (mSyncTimer)
    mSyncTimer->Cancel();

  if (mExpireNowTimer)
    mExpireNowTimer->Cancel();

}



//----------------------------------------------------------------------
//
// nsGlobalHistory
//
//   nsISupports methods

NS_IMPL_ISUPPORTS7(nsGlobalHistory,
                   nsIGlobalHistory,
                   nsIBrowserHistory,
                   nsIObserver,
                   nsISupportsWeakReference,
                   nsIRDFDataSource,
                   nsIRDFRemoteDataSource,
                   nsIAutoCompleteSession)

//----------------------------------------------------------------------
//
// nsGlobalHistory
//
//   nsIGlobalHistory methods
//


NS_IMETHODIMP
nsGlobalHistory::AddPage(const char *aURL)
{
  NS_ENSURE_ARG_POINTER(aURL);
  NS_ENSURE_SUCCESS(OpenDB(), NS_ERROR_FAILURE);

  nsresult rv;

  rv = SaveLastPageVisited(aURL);
  if (NS_FAILED(rv)) return rv;

  rv = AddPageToDatabase(aURL, GetNow());
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsGlobalHistory::AddPageToDatabase(const char *aURL,
                                   PRInt64 aDate)
{
  nsresult rv;
  
  // Sanity check the URL
  PRInt32 len = PL_strlen(aURL);
  NS_ASSERTION(len != 0, "no URL");
  if (! len)
    return NS_ERROR_INVALID_ARG;
  
  // For notifying observers, later...
  nsCOMPtr<nsIRDFResource> url;
  rv = gRDFService->GetResource(aURL, getter_AddRefs(url));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFDate> date;
  rv = gRDFService->GetDateLiteral(aDate, getter_AddRefs(date));
  if (NS_FAILED(rv)) return rv;

  nsMdbPtr<nsIMdbRow> row(mEnv);
  rv = FindRow(kToken_URLColumn, aURL, getter_Acquires(row));

  if (NS_SUCCEEDED(rv)) {

    // update the database, and get the old info back
    PRInt64 oldDate;
    PRInt32 oldCount;
    rv = AddExistingPageToDatabase(row, aDate, &oldDate, &oldCount);
    NS_ASSERTION(NS_SUCCEEDED(rv), "AddExistingPageToDatabase failed; see bug 88961");
    if (NS_FAILED(rv)) return rv;
    
    // Notify observers
    
    // visit date
    nsCOMPtr<nsIRDFDate> oldDateLiteral;
    rv = gRDFService->GetDateLiteral(oldDate, getter_AddRefs(oldDateLiteral));
    if (NS_FAILED(rv)) return rv;
    
    rv = NotifyChange(url, kNC_Date, oldDateLiteral, date);
    if (NS_FAILED(rv)) return rv;

    // visit count
    nsCOMPtr<nsIRDFInt> oldCountLiteral;
    rv = gRDFService->GetIntLiteral(oldCount, getter_AddRefs(oldCountLiteral));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIRDFInt> newCountLiteral;
    rv = gRDFService->GetIntLiteral(oldCount+1,
                                    getter_AddRefs(newCountLiteral));
    if (NS_FAILED(rv)) return rv;

    rv = NotifyChange(url, kNC_VisitCount, oldCountLiteral, newCountLiteral);
    if (NS_FAILED(rv)) return rv;
    
  }
  else {
    rv = AddNewPageToDatabase(aURL, aDate, getter_Acquires(row));
    NS_ASSERTION(NS_SUCCEEDED(rv), "AddNewPageToDatabase failed; see bug 88961");
    if (NS_FAILED(rv)) return rv;
    
    // Notify observers
    rv = NotifyAssert(url, kNC_Date, date);
    if (NS_FAILED(rv)) return rv;
    
    rv = NotifyAssert(kNC_HistoryRoot, kNC_child, url);
    if (NS_FAILED(rv)) return rv;
  }

  NotifyFindAssertions(url, row);
  
  SetDirty();
  
  return rv;
}

nsresult
nsGlobalHistory::AddExistingPageToDatabase(nsIMdbRow *row,
                                           PRInt64 aDate,
                                           PRInt64 *aOldDate,
                                           PRInt32 *aOldCount)
{

  nsresult rv;
  // Update last visit date.
  // First get the old date so we can update observers...
  rv = GetRowValue(row, kToken_LastVisitDateColumn, aOldDate);
  if (NS_FAILED(rv)) return rv;

  // get the old count, so we can update it
  rv = GetRowValue(row, kToken_VisitCountColumn, aOldCount);
  if (NS_FAILED(rv) || *aOldCount < 1)
    *aOldCount = 1;             // assume we've visited at least once

  // ...now set the new date.
  SetRowValue(row, kToken_LastVisitDateColumn, aDate);
  SetRowValue(row, kToken_VisitCountColumn, (*aOldCount) + 1);

  return NS_OK;
}

nsresult
nsGlobalHistory::AddNewPageToDatabase(const char *aURL,
                                      PRInt64 aDate,
                                      nsIMdbRow **aResult)
{
  mdb_err err;
  
  // Create a new row
  mdbOid rowId;
  rowId.mOid_Scope = kToken_HistoryRowScope;
  rowId.mOid_Id    = mdb_id(-1);
  
  NS_PRECONDITION(mTable != nsnull, "not initialized");
  if (! mTable)
    return NS_ERROR_NOT_INITIALIZED;

  nsMdbPtr<nsIMdbRow> row(mEnv);
  err = mTable->NewRow(mEnv, &rowId, getter_Acquires(row));
  if (err != 0) return NS_ERROR_FAILURE;

  // Set the URL
  SetRowValue(row, kToken_URLColumn, aURL);
  
  // Set the date.
  SetRowValue(row, kToken_LastVisitDateColumn, aDate);
  SetRowValue(row, kToken_FirstVisitDateColumn, aDate);

  nsXPIDLCString hostname;
  nsCOMPtr<nsIIOService> ioService = do_GetService(NS_IOSERVICE_CONTRACTID);
  if (!ioService) return NS_ERROR_FAILURE;
  ioService->ExtractUrlPart(aURL, nsIIOService::url_Host, 0, 0, getter_Copies(hostname));

  SetRowValue(row, kToken_HostnameColumn, hostname);

  *aResult = row;
  (*aResult)->AddStrongRef(mEnv);

  return NS_OK;
}

nsresult
nsGlobalHistory::SetRowValue(nsIMdbRow *aRow, mdb_column aCol, const PRInt64& aValue)
{
  mdb_err err;
  nsCAutoString val;
  PRInt64ToChars(aValue, val);

  mdbYarn yarn = { (void *)val.get(), val.Length(), val.Length(), 0, 0, nsnull };
  
  err = aRow->AddColumn(mEnv, aCol, &yarn);

  if ( err != 0 ) return NS_ERROR_FAILURE;
  
  return NS_OK;
}

nsresult
nsGlobalHistory::SetRowValue(nsIMdbRow *aRow, mdb_column aCol,
                             const PRUnichar* aValue)
{
  mdb_err err;

  PRInt32 len = (nsCRT::strlen(aValue) * sizeof(PRUnichar));

  // eventually turn this on when we're confident in mork's abilitiy
  // to handle yarn forms properly
#if 0
  NS_ConvertUCS2toUTF8 utf8Value(aValue);
  printf("Storing utf8 value %s\n", utf8Value.get());
  mdbYarn yarn = { (void *)utf8Value.get(), utf8Value.Length(), utf8Value.Length(), 0, 1, nsnull };
#else

  mdbYarn yarn = { (void *)aValue, len, len, 0, 0, nsnull };
  
#endif
  err = aRow->AddColumn(mEnv, aCol, &yarn);
  if (err != 0) return NS_ERROR_FAILURE;
  return NS_OK;
}

nsresult
nsGlobalHistory::SetRowValue(nsIMdbRow *aRow, mdb_column aCol,
                             const char* aValue)
{
  mdb_err err;
  PRInt32 len = PL_strlen(aValue);
  mdbYarn yarn = { (void*) aValue, len, len, 0, 0, nsnull };
  err = aRow->AddColumn(mEnv, aCol, &yarn);
  if (err != 0) return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
nsGlobalHistory::SetRowValue(nsIMdbRow *aRow, mdb_column aCol, const PRInt32 aValue)
{
  mdb_err err;
  
  nsCAutoString buf; buf.AppendInt(aValue);
  mdbYarn yarn = { (void *)buf.get(), buf.Length(), buf.Length(), 0, 0, nsnull };

  err = aRow->AddColumn(mEnv, aCol, &yarn);
  
  if (err != 0) return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
nsGlobalHistory::GetRowValue(nsIMdbRow *aRow, mdb_column aCol,
                             nsAWritableString& aResult)
{
  mdb_err err;
  
  mdbYarn yarn;
  err = aRow->AliasCellYarn(mEnv, aCol, &yarn);
  if (err != 0) return NS_ERROR_FAILURE;

  aResult.Truncate(0);
  if (!yarn.mYarn_Fill)
    return NS_OK;
  
  switch (yarn.mYarn_Form) {
  case 0:                       // unicode
    aResult.Assign((const PRUnichar *)yarn.mYarn_Buf, yarn.mYarn_Fill/sizeof(PRUnichar));
    break;

    // eventually we'll be supporting this in SetRowValue()
  case 1:                       // UTF8
    aResult.Assign(NS_ConvertUTF8toUCS2((const char*)yarn.mYarn_Buf, yarn.mYarn_Fill));
    break;

  default:
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

nsresult
nsGlobalHistory::GetRowValue(nsIMdbRow *aRow, mdb_column aCol,
                             PRInt64 *aResult)
{
  mdb_err err;
  
  mdbYarn yarn;
  err = aRow->AliasCellYarn(mEnv, aCol, &yarn);
  if (err != 0) return NS_ERROR_FAILURE;

  *aResult = LL_ZERO;
  
  if (!yarn.mYarn_Fill || !yarn.mYarn_Buf)
    return NS_OK;

  return CharsToPRInt64((char *)yarn.mYarn_Buf, yarn.mYarn_Fill, aResult);
}

nsresult
nsGlobalHistory::GetRowValue(nsIMdbRow *aRow, mdb_column aCol,
                             PRInt32 *aResult)
{
  mdb_err err;
  
  mdbYarn yarn;
  err = aRow->AliasCellYarn(mEnv, aCol, &yarn);
  if (err != 0) return NS_ERROR_FAILURE;

  if (yarn.mYarn_Buf)
    *aResult = atoi((char *)yarn.mYarn_Buf);
  else
    *aResult = 0;
  
  return NS_OK;
}

nsresult
nsGlobalHistory::GetRowValue(nsIMdbRow *aRow, mdb_column aCol,
                             nsAWritableCString& aResult)
{
  mdb_err err;
  
  mdbYarn yarn;
  err = aRow->AliasCellYarn(mEnv, aCol, &yarn);
  if (err != 0) return NS_ERROR_FAILURE;

  nsDependentCString url((const char *)yarn.mYarn_Buf, yarn.mYarn_Fill);
  aResult.Assign(url);
  
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalHistory::SetPageTitle(const char *aURL, const PRUnichar *aTitle)
{
  NS_PRECONDITION(aURL != nsnull, "null ptr");
  if (! aURL)
    return NS_ERROR_NULL_POINTER;

  // avoid this one well-known url since we can avoid
  // reading in the db
  if (PL_strcmp(aURL, "about:blank")==0)
    return NS_OK;
  
  NS_ENSURE_SUCCESS(OpenDB(), NS_ERROR_FAILURE);
  
  nsresult rv;
  
  // Be defensive if somebody sends us a null title.
  static PRUnichar kEmptyString[] = { 0 };
  if (! aTitle)
    aTitle = kEmptyString;

  nsMdbPtr<nsIMdbRow> row(mEnv);
  rv = FindRow(kToken_URLColumn, aURL, getter_Acquires(row));
  if (NS_FAILED(rv))
    return rv;

  // Get the old title so we can notify observers
  nsAutoString oldtitle;
  rv = GetRowValue(row, kToken_NameColumn, oldtitle);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFLiteral> oldname;
  if (!oldtitle.IsEmpty()) {
    rv = gRDFService->GetLiteral(oldtitle.get(), getter_AddRefs(oldname));
    if (NS_FAILED(rv)) return rv;
  }

  SetRowValue(row, kToken_NameColumn, aTitle);

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
  mdb_err err;
  nsresult rv;

  if (!mTable) return NS_ERROR_NOT_INITIALIZED;
  // find the old row, ignore it if we don't have it
  nsMdbPtr<nsIMdbRow> row(mEnv);
  rv = FindRow(kToken_URLColumn, aURL, getter_Acquires(row));
  if (NS_FAILED(rv)) return NS_OK;

  // get the resource so we can do the notification
  nsCOMPtr<nsIRDFResource> oldRowResource;
  gRDFService->GetResource(aURL, getter_AddRefs(oldRowResource));

  // remove the row
  err = mTable->CutRow(mEnv, row);
  NS_ENSURE_TRUE(err == 0, NS_ERROR_FAILURE);

  NotifyFindUnassertions(oldRowResource, row);

  // not a fatal error if we can't cut all column
  err = row->CutAllColumns(mEnv);
  NS_ASSERTION(err == 0, "couldn't cut all columns");

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalHistory::RemovePagesFromHost(const char *aHost, PRBool aEntireDomain)
{
  matchHost_t hostInfo;
  hostInfo.history = this;
  hostInfo.entireDomain = aEntireDomain;
  hostInfo.host = aHost;
  
  hostInfo.cachedUrl = nsnull; // todo: leak?
  return RemoveMatchingRows(matchHostCallback, (void *)&hostInfo, PR_TRUE);
}

PRBool
nsGlobalHistory::MatchHost(nsIMdbRow *aRow,
                           matchHost_t *hostInfo)
{
  mdb_err err;
  nsresult rv;

  mdbYarn yarn;
  err = aRow->AliasCellYarn(mEnv, kToken_URLColumn, &yarn);
  if (err != 0) return PR_FALSE;

  // do smart zero-termination
  nsDependentCString url((const char *)yarn.mYarn_Buf, yarn.mYarn_Fill);
  rv = NS_NewURI(&hostInfo->cachedUrl, nsCAutoString(url).get());
  if (NS_FAILED(rv)) return PR_FALSE;

  nsXPIDLCString urlHost;
  rv = hostInfo->cachedUrl->GetHost(getter_Copies(urlHost));
  if (NS_FAILED(rv)) return PR_FALSE;

  if (PL_strcmp(urlHost, hostInfo->host) == 0)
    return PR_TRUE;

  // now try for a domain match, if necessary
  if (hostInfo->entireDomain) {
    // do a reverse-search to match the end of the string
    char *domain = PL_strrstr(urlHost, hostInfo->host);
    
    // now verify that we're matching EXACTLY the domain, and
    // not some random string inside the hostname
    if (domain && (PL_strcmp(domain, hostInfo->host) == 0))
      return PR_TRUE;
  }
  
  return PR_FALSE;
}

NS_IMETHODIMP
nsGlobalHistory::RemoveAllPages()
{
  nsresult rv;

  rv = RemoveMatchingRows(matchAllCallback, nsnull, PR_TRUE);
  if (NS_FAILED(rv)) return rv;
  
  return Commit(kCompressCommit);
}

nsresult
nsGlobalHistory::RemoveMatchingRows(rowMatchCallback aMatchFunc,
                                    void *aClosure,
                                    PRBool notify)
{
  nsresult rv;
  if (!mTable) return NS_OK;

  mdb_err err;
  mdb_count count;
  err = mTable->GetCount(mEnv, &count);
  if (err != 0) return NS_ERROR_FAILURE;

  // XXX tell RDF observers that we're about to do a batch update
  
  // Begin the batch.
  int marker;
  err = mTable->StartBatchChangeHint(mEnv, &marker);
  NS_ASSERTION(err == 0, "unable to start batch");
  if (err != 0) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIRDFResource> resource;
  // XXX from here until end batch, no early returns!
  for (mdb_pos pos = count - 1; pos >= 0; --pos) {
    nsMdbPtr<nsIMdbRow> row(mEnv);
    err = mTable->PosToRow(mEnv, pos, getter_Acquires(row));
    NS_ASSERTION(err == 0, "unable to get row");
    if (err != 0)
      break;

    NS_ASSERTION(row != nsnull, "no row");
    if (! row)
      continue;

    // now we actually do the match. If this row doesn't match, loop again
    if (!(aMatchFunc)(row, aClosure))
      continue;

    if (notify) {
      // What's the URL? We need to know to properly notify our RDF
      // observers.
      mdbYarn yarn;
      err = row->AliasCellYarn(mEnv, kToken_URLColumn, &yarn);
      if (err != 0)
        continue;
      
      nsDependentCString uri((const char*) yarn.mYarn_Buf, yarn.mYarn_Fill);
      
      rv = gRDFService->GetResource(nsCAutoString(uri).get(), getter_AddRefs(resource));
      NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get resource");
      if (NS_FAILED(rv))
        continue;
    }
    // Officially cut the row *now*, before notifying any observers:
    // that way, any re-entrant calls won't find the row.
    err = mTable->CutRow(mEnv, row);
    NS_ASSERTION(err == 0, "couldn't cut row");
    if (err != 0)
      continue;
    
    // Notify observers that the row is, er, history.
    if (notify)
      NotifyFindUnassertions(resource, row);
    
    // possibly avoid leakage
    err = row->CutAllColumns(mEnv);
    NS_ASSERTION(err == 0, "couldn't cut all columns");
    // we'll notify regardless of whether we could successfully
    // CutAllColumns or not.
    
    
  }
  
  // Finish the batch.
  err = mTable->EndBatchChangeHint(mEnv, &marker);
  NS_ASSERTION(err == 0, "error ending batch");

  // XXX tell RDF observers that we're done with the batch
  
  return ( err == 0) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGlobalHistory::IsVisited(const char *aURL, PRBool *_retval)
{
  NS_PRECONDITION(aURL != nsnull, "null ptr");
  if (! aURL)
    return NS_ERROR_NULL_POINTER;

  nsresult rv;
  NS_ENSURE_SUCCESS(OpenDB(), NS_ERROR_NOT_INITIALIZED);

  nsMdbPtr<nsIMdbRow> row(mEnv);
  rv = FindRow(kToken_URLColumn, aURL, getter_Acquires(row));

  if (NS_FAILED(rv))
    *_retval = PR_FALSE;
  else
    *_retval = PR_TRUE;

  return NS_OK;
}

nsresult
nsGlobalHistory::SaveLastPageVisited(const char *aURL)
{
  nsresult rv;

  if (!aURL) return NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv));
  if (NS_FAILED(rv)) return rv;

  rv = prefs->SetCharPref(PREF_BROWSER_HISTORY_LAST_PAGE_VISITED, aURL);

  return rv;
}

NS_IMETHODIMP
nsGlobalHistory::GetLastPageVisited(char **_retval)
{ 
  nsresult rv;

  if (!_retval) return NS_ERROR_NULL_POINTER;

  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv));
  if (NS_FAILED(rv)) return rv;

  nsXPIDLCString lastPageVisited;
  rv = prefs->CopyCharPref(PREF_BROWSER_HISTORY_LAST_PAGE_VISITED, getter_Copies(lastPageVisited));
  if (NS_FAILED(rv)) return rv;

  *_retval = nsCRT::strdup((const char *)lastPageVisited);


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

  *aURI = nsCRT::strdup("rdf:history");
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
    if (IsURLInHistory(target))
      return CallQueryInterface(aTarget, aSource);
    
  }
  else if ((aProperty == kNC_Date) ||
           (aProperty == kNC_FirstVisitDate) ||
           (aProperty == kNC_VisitCount) ||
           (aProperty == kNC_Name) ||
           (aProperty == kNC_Hostname) ||
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

    // PRInt64/date properties
    if (aProperty == kNC_Date ||
        aProperty == kNC_FirstVisitDate) {
      nsCOMPtr<nsIRDFDate> date = do_QueryInterface(aTarget);
      if (date) {
        PRInt64 n;

        rv = date->GetValue(&n);
        if (NS_FAILED(rv)) return rv;

        nsCAutoString valueStr;
        rv = PRInt64ToChars(n, valueStr);
        if (NS_FAILED(rv)) return rv;
        
        value = (void *)ToNewCString(valueStr);
        if (aProperty == kNC_Date)
          col = kToken_LastVisitDateColumn;
        else
          col = kToken_FirstVisitDateColumn;
      }
    }
    // PRInt32 properties
    else if (aProperty == kNC_VisitCount) {
      nsCOMPtr<nsIRDFInt> countLiteral = do_QueryInterface(aTarget);
      if (countLiteral) {
        PRInt32 intValue;
        rv = countLiteral->GetValue(&intValue);
        if (NS_FAILED(rv)) return rv;

        nsAutoString valueStr; valueStr.AppendInt(intValue);
        value = ToNewUnicode(valueStr);
        len = valueStr.Length() * sizeof(PRUnichar);
        col = kToken_VisitCountColumn;
      }
      
    }
    // PRUnichar* properties
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

    // char* properties
    else if (aProperty == kNC_Hostname ||
             aProperty == kNC_Referrer) {
      col = kToken_ReferrerColumn;
      nsCOMPtr<nsIRDFResource> referrer = do_QueryInterface(aTarget);
      if (referrer) {
        char* p;
        rv = referrer->GetValue(&p);
        if (NS_FAILED(rv)) return rv;

        len = PL_strlen(p);
        value = p;

        if (aProperty == kNC_Hostname)
          col = kToken_HostnameColumn;
        else if (aProperty == kNC_Referrer)
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

    // XXX eventually use IsFindResource to simply return the first
    // matching row?
  if (aProperty == kNC_child &&
      (aSource == kNC_HistoryRoot ||
       aSource == kNC_HistoryByDate ||
       IsFindResource(aSource))) {
      
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
           (aProperty == kNC_FirstVisitDate) ||
           (aProperty == kNC_VisitCount) ||
           (aProperty == kNC_AgeInDays) ||
           (aProperty == kNC_Name) ||
           (aProperty == kNC_NameSort) ||
           (aProperty == kNC_Hostname) ||
           (aProperty == kNC_Referrer) ||
           (aProperty == kNC_URL)) {

    const char* uri;
    rv = aSource->GetValueConst(&uri);
    if (NS_FAILED(rv)) return rv;

    // url is self-referential, so we'll always just return itself
    // however, don't return the URLs of find resources
    if (aProperty == kNC_URL && !IsFindResource(aSource)) {
      
      nsCOMPtr<nsIRDFLiteral> uriLiteral;
      rv = gRDFService->GetLiteral(NS_ConvertUTF8toUCS2(uri).get(), getter_AddRefs(uriLiteral));
      if (NS_FAILED(rv))    return(rv);
      *aTarget = uriLiteral;
      NS_ADDREF(*aTarget);
      return NS_OK;
    }

    // find URIs are special
    if (((aProperty == kNC_Name) || (aProperty == kNC_NameSort)) &&
        IsFindResource(aSource)) {
      
      // for sorting, we sort by uri, so just return the URI as a literal
      if (aProperty == kNC_NameSort) {
        nsCOMPtr<nsIRDFLiteral> uriLiteral;
        rv = gRDFService->GetLiteral(NS_ConvertUTF8toUCS2(uri).get(),
                                     getter_AddRefs(uriLiteral));
        if (NS_FAILED(rv))    return(rv);
        
        *aTarget = uriLiteral;
        NS_ADDREF(*aTarget);
        return NS_OK;
      }
      else 
        return GetFindUriName(uri, aTarget);
    }
    // ok, we got this far, so we have to retrieve something from
    // the row in the database
    nsMdbPtr<nsIMdbRow> row(mEnv);
    rv = FindRow(kToken_URLColumn, uri, getter_Acquires(row));
    if (NS_FAILED(rv)) return NS_RDF_NO_VALUE;

    mdb_err err;
    // ...and then depending on the property they want, we'll pull the
    // cell they want out of it.
    if (aProperty == kNC_Date  ||
        aProperty == kNC_FirstVisitDate) {
      // Last visit date
      PRInt64 i;
      if (aProperty == kNC_Date)
        rv = GetRowValue(row, kToken_LastVisitDateColumn, &i);
      else
        rv = GetRowValue(row, kToken_FirstVisitDateColumn, &i);

      if (NS_FAILED(rv)) return rv;

      nsCOMPtr<nsIRDFDate> date;
      rv = gRDFService->GetDateLiteral(i, getter_AddRefs(date));
      if (NS_FAILED(rv)) return rv;

      return CallQueryInterface(date, aTarget);
    }
    else if (aProperty == kNC_VisitCount) {
      mdbYarn yarn;
      err = row->AliasCellYarn(mEnv, kToken_VisitCountColumn, &yarn);
      if (err != 0) return NS_ERROR_FAILURE;

      PRInt32 visitCount = 0;
      rv = GetRowValue(row, kToken_VisitCountColumn, &visitCount);
      if (NS_FAILED(rv) || visitCount <1)
        visitCount = 1;         // assume we've visited at least once

      nsCOMPtr<nsIRDFInt> visitCountLiteral;
      rv = gRDFService->GetIntLiteral(visitCount,
                                      getter_AddRefs(visitCountLiteral));
      if (NS_FAILED(rv)) return rv;

      return CallQueryInterface(visitCountLiteral, aTarget);
    }
    else if (aProperty == kNC_AgeInDays) {
      PRInt64 lastVisitDate;
      rv = GetRowValue(row, kToken_LastVisitDateColumn, &lastVisitDate);
      if (NS_FAILED(rv)) return rv;
      
      PRInt32 days = GetAgeInDays(NormalizeTime(GetNow()), lastVisitDate);

      nsCOMPtr<nsIRDFInt> ageLiteral;
      rv = gRDFService->GetIntLiteral(days, getter_AddRefs(ageLiteral));
      if (NS_FAILED(rv)) return rv;

      *aTarget = ageLiteral;
      NS_ADDREF(*aTarget);
      return NS_OK;
    }
    else if (aProperty == kNC_Name ||
             aProperty == kNC_NameSort) {
      // Site name (i.e., page title)
      nsAutoString title;
      rv = GetRowValue(row, kToken_NameColumn, title);
      if (NS_FAILED(rv) || title.IsEmpty()) {
        // yank out the filename from the url, use that
        nsCOMPtr<nsIURI> aUri;
        rv = NS_NewURI(getter_AddRefs(aUri), uri);
        if (NS_FAILED(rv)) return rv;
        nsCOMPtr<nsIURL> urlObj(do_QueryInterface(aUri));
        if (!urlObj)
            return NS_ERROR_FAILURE;

        nsXPIDLCString filename;
        rv = urlObj->GetFileName(getter_Copies(filename));
        if (NS_FAILED(rv) || !(const char*)filename) {
          // ok fine. we'll use the file path. then we give up!
          rv = urlObj->GetPath(getter_Copies(filename));
          if (PL_strcmp(filename, "/") == 0) {
            // if the top of a site does not have a title
            // (common for redirections) then return the hostname
            return GetTarget(aSource, kNC_Hostname, aTruthValue, aTarget);
            
          }
        }

        if (NS_FAILED(rv)) return rv;
        
        // assume the url is in UTF8
        title = NS_ConvertUTF8toUCS2(filename);
      }
      if (NS_FAILED(rv)) return rv;

      nsCOMPtr<nsIRDFLiteral> name;
      rv = gRDFService->GetLiteral(title.get(), getter_AddRefs(name));
      if (NS_FAILED(rv)) return rv;

      return CallQueryInterface(name, aTarget);
    }
    else if (aProperty == kNC_Hostname ||
             aProperty == kNC_Referrer) {
      
      nsCAutoString str;
      if (aProperty == kNC_Hostname)
        rv = GetRowValue(row, kToken_HostnameColumn, str);
      else if (aProperty == kNC_Referrer)
        rv = GetRowValue(row, kToken_ReferrerColumn, str);
      
      if (NS_FAILED(rv)) return rv;
      
      nsCOMPtr<nsIRDFResource> resource;
      rv = gRDFService->GetResource(str.get(),
                                    getter_AddRefs(resource));
      if (NS_FAILED(rv)) return rv;

      return CallQueryInterface(resource, aTarget);
    }

    else {
      NS_NOTREACHED("huh, how'd I get here?");
    }
  }
  return NS_RDF_NO_VALUE;
}

void
nsGlobalHistory::Sync()
{
  if (mDirty)
    Flush();
  
  mDirty = PR_FALSE;
}

void
nsGlobalHistory::ExpireNow()
{
  mNowValid = PR_FALSE;
}

// when we're dirty, we want to make sure we sync again soon,
// but make sure that we don't keep syncing if someone is surfing
// a lot, so cancel the existing timer if any is still waiting to fire
nsresult
nsGlobalHistory::SetDirty()
{
  nsresult rv;

  if (mSyncTimer)
    mSyncTimer->Cancel();

  if (!mSyncTimer)
    mSyncTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_FAILED(rv)) return rv;
  
  mDirty = PR_TRUE;
  mSyncTimer->Init(fireSyncTimer, this, HISTORY_SYNC_TIMEOUT,
                   NS_PRIORITY_LOWEST, NS_TYPE_ONE_SHOT);
  
  return NS_OK;
}

// hack to avoid calling PR_Now() too often, as is the case when
// we're asked the ageindays of many history entries in a row
PRInt64
nsGlobalHistory::GetNow()
{
  if (!mNowValid) {             // not dirty, mLastNow is crufty
    mLastNow = PR_Now();
    mNowValid = PR_TRUE;
    if (!mExpireNowTimer)
      mExpireNowTimer = do_CreateInstance("@mozilla.org/timer;1");

    if (mExpireNowTimer)
      mExpireNowTimer->Init(expireNowTimer, this, HISTORY_EXPIRE_NOW_TIMEOUT,
                            NS_PRIORITY_LOWEST, NS_TYPE_ONE_SHOT);
  }
  
  return mLastNow;
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

  if (!aTruthValue)
    return NS_NewEmptyEnumerator(aTargets);

  NS_ENSURE_SUCCESS(OpenDB(), NS_ERROR_FAILURE);
  
  // list all URLs off the root
  if ((aSource == kNC_HistoryRoot) &&
      (aProperty == kNC_child)) {
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
  else if ((aSource == kNC_HistoryByDate) &&
           (aProperty == kNC_child)) {

    return GetRootDayQueries(aTargets);
  }
  else if (aProperty == kNC_child &&
           IsFindResource(aSource)) {
    return CreateFindEnumerator(aSource, aTargets);
  }
  
  else if ((aProperty == kNC_Date) ||
           (aProperty == kNC_FirstVisitDate) ||
           (aProperty == kNC_VisitCount) ||
           (aProperty == kNC_AgeInDays) ||
           (aProperty == kNC_Name) ||
           (aProperty == kNC_Hostname) ||
           (aProperty == kNC_Referrer)) {
    nsresult rv;
    
    nsCOMPtr<nsIRDFNode> target;
    rv = GetTarget(aSource, aProperty, aTruthValue, getter_AddRefs(target));
    if (NS_FAILED(rv)) return rv;
    
    if (rv == NS_OK) {
      return NS_NewSingletonEnumerator(aTargets, target);
    }
  }

  // we've already answered the queries from the root, so we must be
  // looking for real entries

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
  // translate into an appropriate removehistory call
  nsresult rv;
  if ((aSource == kNC_HistoryRoot || aSource == kNC_HistoryByDate || IsFindResource(aSource)) &&
      aProperty == kNC_child) {

    nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(aTarget, &rv);

    if (NS_FAILED(rv)) return NS_RDF_ASSERTION_REJECTED; 

    const char* targetUrl;
    rv = resource->GetValueConst(&targetUrl);
    if (NS_FAILED(rv)) return NS_RDF_ASSERTION_REJECTED;

    // ignore any error
    rv = RemovePage(targetUrl);
    if (NS_FAILED(rv)) return NS_RDF_ASSERTION_REJECTED;
    
    return NS_OK;
  }
  
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
  if (!aTruthValue) {
    *aHasAssertion = PR_FALSE;
    return NS_OK;
  }

  nsresult rv;
  
  // answer if a specific row matches a find URI
  // 
  // at some point, we should probably match groupby= findURIs with
  // findURIs that match all their criteria
  //
  nsCOMPtr<nsIRDFResource> target = do_QueryInterface(aTarget);
  if (target &&
      aProperty == kNC_child &&
      IsFindResource(aSource) &&
      !IsFindResource(target)) {

    const char *uri;
    rv = target->GetValueConst(&uri);
    if (NS_FAILED(rv)) return rv;

    searchQuery query;
    FindUrlToSearchQuery(uri, query);
    
    nsMdbPtr<nsIMdbRow> row(mEnv);
    rv = FindRow(kToken_URLColumn, uri, getter_Acquires(row));
    // not even in history. don't bother trying
    if (NS_FAILED(rv)) {
      *aHasAssertion = PR_FALSE;
      return NS_OK;
    }
    
    *aHasAssertion = RowMatches(row, &query);
    return NS_OK;
  }
  
  // Do |GetTargets()| and grovel through the results to see if we
  // have the assertion.
  //
  // XXX *AHEM*, this could be implemented much more efficiently...

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
      (aSource == kNC_HistoryByDate)) {
    *result = (aArc == kNC_child);
  }
  else if (IsFindResource(aSource)) {
    // we handle children of find urls
    *result = (aArc == kNC_child ||
               aArc == kNC_Name ||
               aArc == kNC_NameSort);
  }
  else if (IsURLInHistory(aSource)) {
    // If the URL is in the history, then it'll have all the
    // appropriate attributes.
    *result = (aArc == kNC_Date ||
               aArc == kNC_FirstVisitDate ||
               aArc == kNC_VisitCount ||
               aArc == kNC_Name ||
               aArc == kNC_Hostname ||
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
    array->AppendElement(kNC_FirstVisitDate);
    array->AppendElement(kNC_VisitCount);
    array->AppendElement(kNC_Name);
    array->AppendElement(kNC_Hostname);
    array->AppendElement(kNC_Referrer);

    return NS_NewArrayEnumerator(aLabels, array);
  }
  else if (IsFindResource(aSource)) {
    nsCOMPtr<nsISupportsArray> array;
    rv = NS_NewISupportsArray(getter_AddRefs(array));
    if (NS_FAILED(rv)) return rv;

    array->AppendElement(kNC_child);
    array->AppendElement(kNC_Name);
    array->AppendElement(kNC_NameSort);
    
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
  return Commit(kLargeCommit);
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

  // we'd like to get this pref when we need it, but at that point,
  // we can't get the pref service. This means if the user changes
  // this pref, we won't notice until the next time we run.
  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv));
  if (NS_SUCCEEDED(rv))
    rv = prefs->GetIntPref(PREF_BROWSER_HISTORY_EXPIRE_DAYS, &mExpireDays);

  if (gRefCnt++ == 0) {
    rv = nsServiceManager::GetService(kRDFServiceCID,
                                      NS_GET_IID(nsIRDFService),
                                      (nsISupports**) &gRDFService);

    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
    if (NS_FAILED(rv)) return rv;

    gRDFService->GetResource(NC_NAMESPACE_URI "Page",        &kNC_Page);
    gRDFService->GetResource(NC_NAMESPACE_URI "Date",        &kNC_Date);
    gRDFService->GetResource(NC_NAMESPACE_URI "FirstVisitDate",
                             &kNC_FirstVisitDate);
    gRDFService->GetResource(NC_NAMESPACE_URI "VisitCount",  &kNC_VisitCount);
    gRDFService->GetResource(NC_NAMESPACE_URI "AgeInDays",  &kNC_AgeInDays);
    gRDFService->GetResource(NC_NAMESPACE_URI "Name",        &kNC_Name);
    gRDFService->GetResource(NC_NAMESPACE_URI "Name?sort=true", &kNC_NameSort);
    gRDFService->GetResource(NC_NAMESPACE_URI "Hostname",    &kNC_Hostname);
    gRDFService->GetResource(NC_NAMESPACE_URI "Referrer",    &kNC_Referrer);
    gRDFService->GetResource(NC_NAMESPACE_URI "child",       &kNC_child);
    gRDFService->GetResource(NC_NAMESPACE_URI "URL",         &kNC_URL);
    gRDFService->GetResource("NC:HistoryRoot",               &kNC_HistoryRoot);
    gRDFService->GetResource("NC:HistoryByDate",           &kNC_HistoryByDate);
  }

  // register this as a named data source with the RDF service
  rv = gRDFService->RegisterDataSource(this, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStringBundleService> bundleService =
    do_GetService(kStringBundleServiceCID, &rv);
  
  if (NS_SUCCEEDED(rv)) {
    rv = bundleService->CreateBundle("chrome://communicator/locale/history/history.properties", getter_AddRefs(mBundle));
  }

  // register to observe profile changes
  nsCOMPtr<nsIObserverService> observerService = 
           do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ASSERTION(observerService, "failed to get observer service");
  if (observerService) {
    observerService->AddObserver(this, "profile-before-change", PR_TRUE);
    observerService->AddObserver(this, "profile-do-change", PR_TRUE);
  }
  
  return NS_OK;
}


nsresult
nsGlobalHistory::OpenDB()
{
  nsresult rv;

  if (mStore) return NS_OK;
  
  nsCOMPtr <nsIFile> historyFile;
  rv = NS_GetSpecialDirectory(NS_APP_HISTORY_50_FILE, getter_AddRefs(historyFile));
  NS_ENSURE_SUCCESS(rv, rv);

  static NS_DEFINE_CID(kMorkCID, NS_MORK_CID);
  nsCOMPtr<nsIMdbFactoryFactory> factoryfactory;
  rv = nsComponentManager::CreateInstance(kMorkCID,
                                          nsnull,
                                          NS_GET_IID(nsIMdbFactoryFactory),
                                          getter_AddRefs(factoryfactory));
  NS_ENSURE_SUCCESS(rv, rv);

  // Leaving XPCOM, entering MDB. They may look like XPCOM interfaces,
  // but they're not. The 'factory' is an interface; however, it isn't
  // reference counted. So no, this isn't a leak.
  nsIMdbFactory* factory;
  rv = factoryfactory->GetMdbFactory(&factory);
  NS_ENSURE_SUCCESS(rv, rv);

  mdb_err err;

  err = factory->MakeEnv(nsnull, &mEnv);
  mEnv->SetAutoClear(PR_TRUE);
  NS_ASSERTION((err == 0), "unable to create mdb env");
  if (err != 0) return NS_ERROR_FAILURE;

  nsXPIDLCString filePath;
  rv = historyFile->GetPath(getter_Copies(filePath));
  NS_ENSURE_SUCCESS(rv, rv);


  PRBool exists = PR_TRUE;

  historyFile->Exists(&exists);
    
  if (!exists || NS_FAILED(rv = OpenExistingFile(factory, filePath))) {

    // we couldn't open the file, so it's either corrupt or doesn't exist.
    // attempt to delete the file, but ignore the error
    historyFile->Remove(PR_FALSE);
    rv = OpenNewFile(factory, filePath);
  }

  NS_ENSURE_SUCCESS(rv, rv);
  
  return NS_OK;
}

nsresult
nsGlobalHistory::OpenExistingFile(nsIMdbFactory *factory, const char *filePath)
{

  mdb_err err;
  nsresult rv;
  mdb_bool canopen = 0;
  mdbYarn outfmt = { nsnull, 0, 0, 0, 0, nsnull };

  nsIMdbHeap* dbHeap = 0;
  mdb_bool dbFrozen = mdbBool_kFalse; // not readonly, we want modifiable
  nsMdbPtr<nsIMdbFile> oldFile(mEnv); // ensures file is released
  err = factory->OpenOldFile(mEnv, dbHeap, filePath,
                             dbFrozen, getter_Acquires(oldFile));

  // don't assert, the file might just not be there
  if ((err !=0) || !oldFile) return NS_ERROR_FAILURE;

  err = factory->CanOpenFilePort(mEnv, oldFile, // the file to investigate
                                 &canopen, &outfmt);

  // XXX possible that format out of date, in which case we should
  // just re-write the file.
  if ((err !=0) || !canopen) return NS_ERROR_FAILURE;

  nsIMdbThumb* thumb = nsnull;
  mdbOpenPolicy policy = { { 0, 0 }, 0, 0 };

  err = factory->OpenFileStore(mEnv, dbHeap, oldFile, &policy, &thumb);
  if ((err !=0) || !thumb) return NS_ERROR_FAILURE;

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

  if (err != 0) return NS_ERROR_FAILURE;

  rv = CreateTokens();
  NS_ENSURE_SUCCESS(rv, rv);

  mdbOid oid = { kToken_HistoryRowScope, 1 };
  err = mStore->GetTable(mEnv, &oid, &mTable);
  NS_ENSURE_TRUE(err == 0, NS_ERROR_FAILURE);
  if (!mTable) {
    NS_WARNING("Your history file is somehow corrupt.. deleting it.");
    return NS_ERROR_FAILURE;
  }

  CheckHostnameEntries();

  if (err != 0) return NS_ERROR_FAILURE;
  
  return NS_OK;
}

nsresult
nsGlobalHistory::OpenNewFile(nsIMdbFactory *factory, const char *filePath)
{
  nsresult rv;
  mdb_err err;
  
  nsIMdbHeap* dbHeap = 0;
  nsMdbPtr<nsIMdbFile> newFile(mEnv); // ensures file is released
  err = factory->CreateNewFile(mEnv, dbHeap, filePath,
                               getter_Acquires(newFile));

  if ((err != 0) || !newFile) return NS_ERROR_FAILURE;
  
  mdbOpenPolicy policy = { { 0, 0 }, 0, 0 };
  err = factory->CreateNewFileStore(mEnv, dbHeap, newFile, &policy, &mStore);
  
  if (err != 0) return NS_ERROR_FAILURE;
  
  rv = CreateTokens();
  NS_ENSURE_SUCCESS(rv, rv);

  // Create the one and only table in the history db
  err = mStore->NewTable(mEnv, kToken_HistoryRowScope, kToken_HistoryKind, PR_TRUE, nsnull, &mTable);
  if (err != 0) return NS_ERROR_FAILURE;
  if (!mTable) return NS_ERROR_FAILURE;

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

  return NS_OK;
}

// break the uri down into a search query, and pass off to
// SearchEnumerator
nsresult
nsGlobalHistory::CreateFindEnumerator(nsIRDFResource *aSource,
                                      nsISimpleEnumerator **aResult)
{
  nsresult rv;
  // make sure this was a find query
  if (!IsFindResource(aSource))
    return NS_ERROR_FAILURE;

  const char* uri;
  rv = aSource->GetValueConst(&uri);
  if (NS_FAILED(rv)) return rv;

  // convert uri to a query
  searchQuery* query = new searchQuery;
  if (!query) return NS_ERROR_OUT_OF_MEMORY;
  FindUrlToSearchQuery(uri, *query);

  // the enumerator will take ownership of the query
  SearchEnumerator *result =
    new SearchEnumerator(query, this);
  if (!result) return NS_ERROR_OUT_OF_MEMORY;

  rv = result->Init(mEnv, mTable);
  if (NS_FAILED(rv)) return rv;

  // return the value
  *aResult = result;
  NS_ADDREF(*aResult);
  
  return NS_OK;
}


// for each row, we need to parse out the hostname from the url
// then store it in a column
nsresult
nsGlobalHistory::CheckHostnameEntries()
{
  nsresult rv;

  mdb_err err;

  nsMdbPtr<nsIMdbTableRowCursor> cursor(mEnv);
  nsMdbPtr<nsIMdbRow> row(mEnv);

  err = mTable->GetTableRowCursor(mEnv, -1, getter_Acquires(cursor));
  if (err != 0) return NS_ERROR_FAILURE;

  int marker;
  err = mTable->StartBatchChangeHint(mEnv, &marker);
  NS_ASSERTION(err == 0, "unable to start batch");
  if (err != 0) return NS_ERROR_FAILURE;
  
  mdb_pos pos;
  err = cursor->NextRow(mEnv, getter_Acquires(row), &pos);
  if (err != 0) return NS_ERROR_FAILURE;

  // comment out this code to rebuild the hostlist at startup
#if 1
  // bail early if the first row has a hostname
  if (row) {
    nsCAutoString hostname;
    rv = GetRowValue(row, kToken_HostnameColumn, hostname);
    if (NS_SUCCEEDED(rv) && !hostname.IsEmpty())
      return NS_OK;
  }
#endif
  
  // cached variables used in the loop
  nsCAutoString url;
  nsXPIDLCString hostname;

  nsCOMPtr<nsIIOService> ioService = do_GetService(NS_IOSERVICE_CONTRACTID);
  if (!ioService) return NS_ERROR_FAILURE;
  

  while (row) {
#if 0
    rv = GetRowValue(row, kToken_URLColumn, url);
    if (NS_FAILED(rv)) break;

    ioService->ExtractUrlPart(url, nsIIOService::url_Host, 0, 0, getter_Copies(hostname));

    SetRowValue(row, kToken_HostnameColumn, hostname);
    
#endif

    // to be turned on when we're confident in mork's ability
    // to handle yarn forms properly
#if 0
    nsAutoString title;
    rv = GetRowValue(row, kToken_NameColumn, title);
    // reencode back into UTF8
    if (NS_SUCCEEDED(rv))
      SetRowValue(row, kToken_NameColumn, title.get());
#endif
    cursor->NextRow(mEnv, getter_Acquires(row), &pos);
  }

  // Finish the batch.
  err = mTable->EndBatchChangeHint(mEnv, &marker);
  NS_ASSERTION(err == 0, "error ending batch");
  
  return rv;
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

  err = mStore->StringToToken(mEnv, "FirstVisitDate", &kToken_FirstVisitDateColumn);
  if (err != 0) return NS_ERROR_FAILURE;

  err = mStore->StringToToken(mEnv, "VisitCount", &kToken_VisitCountColumn);
  if (err != 0) return NS_ERROR_FAILURE;

  err = mStore->StringToToken(mEnv, "Name", &kToken_NameColumn);
  if (err != 0) return NS_ERROR_FAILURE;

  err = mStore->StringToToken(mEnv, "Hostname", &kToken_HostnameColumn);
  if (err != 0) return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult nsGlobalHistory::Commit(eCommitType commitType)
{
  if (!mStore || !mTable)
    return NS_OK;

  nsresult	err = NS_OK;
  nsMdbPtr<nsIMdbThumb> thumb(mEnv);

  if (commitType == kLargeCommit || commitType == kSessionCommit)
  {
    mdb_percent outActualWaste = 0;
    mdb_bool outShould;
    if (mStore) 
    {
      // check how much space would be saved by doing a compress commit.
      // If it's more than 30%, go for it.
      // N.B. - I'm not sure this calls works in Mork for all cases.
      err = mStore->ShouldCompress(mEnv, 30, &outActualWaste, &outShould);
      if (NS_SUCCEEDED(err) && outShould)
      {
          commitType = kCompressCommit;
      }
      else
      {
        mdb_count count;
        err = mTable->GetCount(mEnv, &count);
        // Since Mork's shouldCompress doesn't work, we need to look
        // at the file size and the number of rows, and make a stab
        // at guessing if we've got a lot of deleted rows. The file
        // size is the size when we opened the db, and we really want
        // it to be the size after we've written out the file,
        // but I think this is a good enough approximation.
        if (count > 0)
        {
          PRInt64 numRows;
          PRInt64 bytesPerRow;
          PRInt64 desiredAvgRowSize;

          LL_UI2L(numRows, count);
          LL_DIV(bytesPerRow, mFileSizeOnDisk, numRows);
          LL_I2L(desiredAvgRowSize, 400);
          if (LL_CMP(bytesPerRow, >, desiredAvgRowSize))
            commitType = kCompressCommit;
        }
      }
    }
  }
  switch (commitType)
  {
  case kLargeCommit:
    err = mStore->LargeCommit(mEnv, getter_Acquires(thumb));
    break;
  case kSessionCommit:
    err = mStore->SessionCommit(mEnv, getter_Acquires(thumb));
    break;
  case kCompressCommit:
    err = mStore->CompressCommit(mEnv, getter_Acquires(thumb));
    break;
  }
  if (err == 0) {
    mdb_count total;
    mdb_count current;
    mdb_bool done;
    mdb_bool broken;

    do {
      err = thumb->DoMore(mEnv, &total, &current, &done, &broken);
    } while ((err == 0) && !broken && !done);
  }
  if (err != 0) // mork doesn't return NS error codes. Yet.
    return NS_ERROR_FAILURE;
  else
    return NS_OK;

}
// if notify is true, we'll notify rdf of deleted rows.
// If we're shutting down history, then (maybe?) we don't
// need or want to notify rdf.
nsresult nsGlobalHistory::ExpireEntries(PRBool notify)
{
  PRTime expirationDate;
  PRInt64 microSecondsPerSecond, secondsInDays, microSecondsInExpireDays;
  
  LL_I2L(microSecondsPerSecond, PR_USEC_PER_SEC);
  LL_UI2L(secondsInDays, 60 * 60 * 24 * mExpireDays);
  LL_MUL(microSecondsInExpireDays, secondsInDays, microSecondsPerSecond);
  LL_SUB(expirationDate, GetNow(), microSecondsInExpireDays);

  matchExpiration_t expiration;
  expiration.history = this;
  expiration.expirationDate = &expirationDate;
  
  return RemoveMatchingRows(matchExpirationCallback, (void *)&expiration, notify);
}

nsresult
nsGlobalHistory::CloseDB()
{
  mdb_err err;

  ExpireEntries(PR_FALSE /* don't notify */);
  err = Commit(kSessionCommit);

  if (mTable)
    mTable->CutStrongRef(mEnv);

  if (mStore)
    mStore->CutStrongRef(mEnv);

  if (mEnv)
    mEnv->CloseMdbObject(mEnv /* XXX */);

  mTable = nsnull; mEnv = nsnull; mStore = nsnull;

  return NS_OK;
}

nsresult
nsGlobalHistory::FindRow(mdb_column aCol,
                         const char *aValue, nsIMdbRow **aResult)
{
  mdb_err err;
  PRInt32 len = PL_strlen(aValue);
  mdbYarn yarn = { (void*) aValue, len, len, 0, 0, nsnull };

  mdbOid rowId;
  nsMdbPtr<nsIMdbRow> row(mEnv);
  err = mStore->FindRow(mEnv, kToken_HistoryRowScope,
                        aCol, &yarn,
                        &rowId, getter_Acquires(row));


  if (!row) return NS_ERROR_NOT_AVAILABLE;
  
  
  // make sure it's actually stored in the main table
  mdb_bool hasRow;
  mTable->HasRow(mEnv, row, &hasRow);

  if (!hasRow) return NS_ERROR_NOT_AVAILABLE;
  
  *aResult = row;
  (*aResult)->AddStrongRef(mEnv);
  
  return NS_OK;
}

PRBool
nsGlobalHistory::IsURLInHistory(nsIRDFResource* aResource)
{
  nsresult rv;

  const char* url;
  rv = aResource->GetValueConst(&url);
  if (NS_FAILED(rv)) return PR_FALSE;

  nsMdbPtr<nsIMdbRow> row(mEnv);
  rv = FindRow(kToken_URLColumn, url, getter_Acquires(row));

  return (NS_SUCCEEDED(rv)) ? PR_TRUE : PR_FALSE;
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
nsGlobalHistory::NotifyUnassert(nsIRDFResource* aSource,
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

      rv = observer->OnUnassert(this, aSource, aProperty, aValue);
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

//
// this is just generates static list of find-style queries
// 
nsresult
nsGlobalHistory::GetRootDayQueries(nsISimpleEnumerator **aResult)
{
  nsresult rv;
  
  nsCOMPtr<nsISupportsArray> dayArray;
  NS_NewISupportsArray(getter_AddRefs(dayArray));
  
  PRInt32 i;
  nsCOMPtr<nsIRDFResource> finduri;
  nsDependentCString
    prefix(FIND_BY_AGEINDAYS_PREFIX "is" "&text=");
  nsCAutoString uri;
  for (i=0; i<7; i++) {
    uri = prefix;
    uri.AppendInt(i);
    uri.Append("&groupby=Hostname");
    rv = gRDFService->GetResource(uri.get(), getter_AddRefs(finduri));
    if (NS_SUCCEEDED(rv))
      dayArray->AppendElement(finduri);
  }

  uri = FIND_BY_AGEINDAYS_PREFIX "isgreater" "&text=";
  uri.AppendInt(i-1);
  uri.Append("&groupby=Hostname");
  rv = gRDFService->GetResource(uri.get(), getter_AddRefs(finduri));

  if (NS_SUCCEEDED(rv))
    rv = dayArray->AppendElement(finduri);

  PRUint32 arraylength;
  dayArray->Count(&arraylength);
  return NS_NewArrayEnumerator(aResult, dayArray);
}

//
// convert the name/value pairs stored in a string into an array of
// these pairs
// find:a=b&c=d&e=f&g=h
// becomes an array containing
// {"a" = "b", "c" = "d", "e" = "f", "g" = "h" }
//
nsresult
nsGlobalHistory::FindUrlToTokenList(const char *aURL, nsVoidArray& aResult)
{
  if (PL_strncmp(aURL, "find:", 5) != 0)
    return NS_ERROR_UNEXPECTED;
  
  const char *curpos = aURL + 5;
  const char *tokenstart = curpos;

  // this is where we will store the current name and value
  const char *tokenName = nsnull;
  const char *tokenValue = nsnull;
  PRUint32 tokenNameLength=0;
  PRUint32 tokenValueLength=0;
  
  PRBool haveValue = PR_FALSE;  // needed because some values are 0-length
  while (PR_TRUE) {
    while (*curpos && (*curpos != '&') && (*curpos != '='))
      curpos++;

    if (*curpos == '=')  {        // just found a token name
      tokenName = tokenstart;
      tokenNameLength = (curpos - tokenstart);
    }
    else if ((!*curpos || *curpos == '&') &&
             (tokenNameLength>0)) { // found a value, and we have a
                                    // name
      tokenValue = tokenstart;
      tokenValueLength = (curpos - tokenstart);
      haveValue = PR_TRUE;
    }

    // once we have a name/value pair, store it away
    // note we're looking at lengths, so that
    // "find:&a=b" doesn't connect with a=""
    if (tokenNameLength>0 && haveValue) {

      tokenPair *tokenStruct = new tokenPair(tokenName, tokenNameLength,
                                             tokenValue, tokenValueLength);
      aResult.AppendElement((void *)tokenStruct);
      
      // reset our state
      tokenName = tokenValue = nsnull;
      tokenNameLength = tokenValueLength = 0;
      haveValue = PR_FALSE;
    }

    // the test has to be here to catch empty values
    if (!*curpos) break;
    
    curpos++;
    tokenstart = curpos;
  }

  return NS_OK;
}

void
nsGlobalHistory::FreeTokenList(nsVoidArray& tokens)
{
  PRUint32 length = tokens.Count();
  PRUint32 i;
  for (i=0; i<length; i++) {
    tokenPair *token = (tokenPair*)tokens[i];
    delete token;
  }
  tokens.Clear();
}

//
// helper function to figure out if something starts with "find"
//
PRBool
nsGlobalHistory::IsFindResource(nsIRDFResource *aResource)
{
  nsresult rv;
  const char *value;
  rv = aResource->GetValueConst(&value);
  if (NS_FAILED(rv)) return PR_FALSE;

  return (PL_strncmp(value, "find:", 5)==0);
}


//
// convert a list of name/value pairs into a search query with 0 or
// more terms and an optional groupby
//
// a term consists of the values of the 4 name/value pairs
// {datasource, match, method, text}
// groupby is stored as a column #
//
nsresult
nsGlobalHistory::TokenListToSearchQuery(const nsVoidArray& aTokens,
                                        searchQuery& aResult)
{

  PRInt32 i;
  PRInt32 length = aTokens.Count();

  aResult.groupBy = 0;
  const char *datasource=nsnull, *property=nsnull,
    *method=nsnull, *text=nsnull;

  PRUint32 datasourceLen=0, propertyLen=0, methodLen=0, textLen=0;
  rowMatchCallback matchCallback=nsnull; // matching callback if needed
  
  for (i=0; i<length; i++) {
    tokenPair *token = (tokenPair *)aTokens[i];

    // per-term tokens
    if (token->tokenName.Equals("datasource")) {
      datasource = token->tokenValue.get();
      datasourceLen = token->tokenValue.Length();
    }
    else if (token->tokenName.Equals("match")) {
      if (token->tokenValue.Equals("AgeInDays"))
        matchCallback = matchAgeInDaysCallback;
      
      property = token->tokenValue.get();
      propertyLen = token->tokenValue.Length();
    }
    else if (token->tokenName.Equals("method")) {
      method = token->tokenValue.get();
      methodLen = token->tokenValue.Length();
    }    
    else if (token->tokenName.Equals("text")) {
      text = token->tokenValue.get();
      textLen = token->tokenValue.Length();
    }
    
    // really, we should be storing the group-by as a column number or
    // rdf resource
    else if (token->tokenName.Equals("groupby")) {
      mdb_err err;
      err = mStore->QueryToken(mEnv,
                               nsCAutoString(token->tokenValue),
                               &aResult.groupBy);
      if (err != 0)
        aResult.groupBy = 0;
    }
    
    // once we complete a term, we move on to the next one
    if (datasource && property && method && text) {
      searchTerm *currentTerm = new searchTerm(datasource, datasourceLen,
                                               property, propertyLen,
                                               method, methodLen,
                                               text, textLen);
      currentTerm->match = matchCallback;
      
      // append the old one, then create a new one
      aResult.terms.AppendElement((void *)currentTerm);

      // reset our state
      matchCallback=nsnull;
      currentTerm = nsnull;
      datasource = property = method = text = 0;
    }
  }

  return NS_OK;
}

nsresult
nsGlobalHistory::FindUrlToSearchQuery(const char *aUrl, searchQuery& aResult)
{

  nsresult rv;
  // convert uri to list of tokens
  nsVoidArray tokenPairs;
  rv = FindUrlToTokenList(aUrl, tokenPairs);
  if (NS_FAILED(rv)) return rv;

  // now convert the tokens to a query
  rv = TokenListToSearchQuery(tokenPairs, aResult);
  
  FreeTokenList(tokenPairs);

  return rv;
}

// preemptively construct some common find-queries so that we show up
// asychronously when a search is open

// we have to do the following assertions:
// (a=AgeInDays, h=hostname; g=groupby, -> = #child)
// 1) NC:HistoryRoot -> uri
//
// 2) NC:HistoryByDate -> a&g=h
// 3)                     a&g=h -> a&h
// 4)                              a&h -> uri
//
// 5) g=h -> h
// 6)        h->uri
nsresult
nsGlobalHistory::NotifyFindAssertions(nsIRDFResource *aSource,
                                      nsIMdbRow *aRow)
{
  // we'll construct a bunch of sample queries, and then do
  // appropriate assertions

  // first pull out the appropriate values
  PRInt64 lastVisited;
  GetRowValue(aRow, kToken_LastVisitDateColumn, &lastVisited);

  PRInt32 ageInDays = GetAgeInDays(NormalizeTime(GetNow()), lastVisited);
  nsCAutoString ageString; ageString.AppendInt(ageInDays);

  nsCAutoString hostname;
  GetRowValue(aRow, kToken_HostnameColumn, hostname);
  
  // construct some terms that we'll use later
  
  // Hostname=<hostname>
  searchTerm hostterm("history", sizeof("history")-1,
                      "Hostname", sizeof("Hostname")-1,
                      "is", sizeof("is")-1,
                      hostname.get(), hostname.Length());

  // AgeInDays=<age>
  searchTerm ageterm("history", sizeof("history") -1,
                     "AgeInDays", sizeof("AgeInDays")-1,
                     "is", sizeof("is")-1,
                     ageString.get(), ageString.Length());

  searchQuery query;
  nsCAutoString findUri;
  nsCOMPtr<nsIRDFResource> childFindResource;
  nsCOMPtr<nsIRDFResource> parentFindResource;

  // 2) NC:HistoryByDate -> AgeInDays=<age>&groupby=Hostname
  query.groupBy = kToken_HostnameColumn;
  query.terms.AppendElement((void *)&ageterm);

  GetFindUriPrefix(query, PR_TRUE, findUri);
  gRDFService->GetResource(findUri.get(), getter_AddRefs(childFindResource));
  NotifyAssert(kNC_HistoryByDate, kNC_child, childFindResource);
  
  query.terms.Clear();

  // 3) AgeInDays=<age>&groupby=Hostname ->
  //    AgeInDays=<age>&Hostname=<host>
  
  parentFindResource=childFindResource; // AgeInDays=<age>&groupby=Hostname

  query.groupBy = 0;            // create AgeInDays=<age>&Hostname=<host>
  query.terms.AppendElement((void *)&ageterm);
  query.terms.AppendElement((void *)&hostterm);
  
  GetFindUriPrefix(query, PR_FALSE, findUri);
  gRDFService->GetResource(findUri.get(), getter_AddRefs(childFindResource));
  NotifyAssert(parentFindResource, kNC_child, childFindResource);
  
  query.terms.Clear();

  // 4) AgeInDays=<age>&Hostname=<host> -> uri
  parentFindResource = childFindResource; // AgeInDays=<age>&hostname=<host>
  NotifyAssert(childFindResource, kNC_child, aSource);
  
  // 5) groupby=Hostname -> Hostname=<host>
  query.groupBy = kToken_HostnameColumn; // create groupby=Hostname
  
  GetFindUriPrefix(query, PR_TRUE, findUri);
  gRDFService->GetResource(findUri.get(), getter_AddRefs(parentFindResource));

  query.groupBy = 0;            // create Hostname=<host>
  query.terms.AppendElement((void *)&hostterm);
  GetFindUriPrefix(query, PR_FALSE, findUri);
  findUri.Append(hostname);     // append <host>
  gRDFService->GetResource(findUri.get(), getter_AddRefs(childFindResource));
  
  NotifyAssert(parentFindResource, kNC_child, childFindResource);

  // 6) Hostname=<host> -> uri
  parentFindResource = childFindResource; // Hostname=<host>
  NotifyAssert(parentFindResource, kNC_child, aSource);

  return NS_OK;
}


// simpler than NotifyFindAssertions - basically just notifies
// unassertions from
// 1) NC:HistoryRoot -> uri
// 2) a&h -> uri
// 3) h -> uri

nsresult
nsGlobalHistory::NotifyFindUnassertions(nsIRDFResource *aSource,
                                        nsIMdbRow* aRow)
{
  // 1) NC:HistoryRoot
  NotifyUnassert(kNC_HistoryRoot, kNC_child, aSource);

  //    first get age in days
  PRInt64 lastVisited;
  GetRowValue(aRow, kToken_LastVisitDateColumn, &lastVisited);
  PRInt32 ageInDays = GetAgeInDays(NormalizeTime(GetNow()), lastVisited);
  nsCAutoString ageString; ageString.AppendInt(ageInDays);

  //    now get hostname
  nsCAutoString hostname;
  GetRowValue(aRow, kToken_HostnameColumn, hostname);

  //    construct some terms
  //    Hostname=<hostname>
  searchTerm hostterm("history", sizeof("history")-1,
                      "Hostname", sizeof("Hostname")-1,
                      "is", sizeof("is")-1,
                      hostname.get(), hostname.Length());
  
  //    AgeInDays=<age>
  searchTerm ageterm("history", sizeof("history") -1,
                     "AgeInDays", sizeof("AgeInDays")-1,
                     "is", sizeof("is")-1,
                     ageString.get(), ageString.Length());

  searchQuery query;
  query.groupBy = 0;
  
  nsCAutoString findUri;
  nsCOMPtr<nsIRDFResource> findResource;
  
  // 2) AgeInDays=<age>&Hostname=<host>
  query.terms.AppendElement((void *)&ageterm);
  query.terms.AppendElement((void *)&hostterm);
  GetFindUriPrefix(query, PR_FALSE, findUri);
  
    // XXX |sourceStr| unused ... why are we doing this?
  const char* sourceStr;
  aSource->GetValueConst(&sourceStr);

  gRDFService->GetResource(findUri.get(), getter_AddRefs(findResource));
  
  NotifyUnassert(findResource, kNC_child, aSource);

  // 3) Hostname=<host>
  query.terms.Clear();
  
  query.terms.AppendElement((void *)&hostterm);
  GetFindUriPrefix(query, PR_FALSE, findUri);
  
  gRDFService->GetResource(findUri.get(), getter_AddRefs(findResource));
  NotifyUnassert(findResource, kNC_child, aSource);

  return NS_OK;
}

//
// get the user-visible "name" of a find resource
// we basically parse the string, and use the data stored in the last
// term to generate an appropriate string
//
nsresult
nsGlobalHistory::GetFindUriName(const char *aURL, nsIRDFNode **aResult)
{

  nsresult rv;

  searchQuery query;
  rv = FindUrlToSearchQuery(aURL, query);

  // can't exactly get a name if there's nothing to search for
  if (query.terms.Count() < 1)
    return NS_OK;

  // now build up a string from the query
  searchTerm *term = (searchTerm*)query.terms[query.terms.Count()-1];
    
  // automatically build up string in the form
  // findurl-<property>-<method>[-<text>]
  // such as "finduri-AgeInDays-is" or "find-uri-AgeInDays-is-0"
  nsAutoString stringName(NS_LITERAL_STRING("finduri-"));

  // property
  stringName.AppendWithConversion(term->property.get(),
                                  term->property.Length());
  stringName.Append(PRUnichar('-'));

  // and now the method, such as "is" or "isgreater"
  stringName.AppendWithConversion(nsCAutoString(term->method.get(),
                                                term->method.Length()));

  // try adding -<text> to see if there's a match
  // for example, to match
  // finduri-LastVisitDate-is-0=Today
  PRInt32 preTextLength = stringName.Length();
  stringName.Append(PRUnichar('-'));
  stringName.Append(term->text);
  stringName.Append(PRUnichar(0));

  // try to find a localizable string
  const PRUnichar *strings[] = {
    term->text.get()
  };
  nsXPIDLString value;

  // first with the search text
  rv = mBundle->FormatStringFromName(stringName.get(),
                                     strings, 1, getter_Copies(value));

  // ok, try it without the -<text>, to match
  // finduri-LastVisitDate-is=%S days ago
  if (NS_FAILED(rv)) {
    stringName.Truncate(preTextLength);
    rv = mBundle->FormatStringFromName(stringName.get(),
                                       strings, 1, getter_Copies(value));
  }

  nsCOMPtr<nsIRDFLiteral> literal;
  if (NS_SUCCEEDED(rv)) {
    rv = gRDFService->GetLiteral(value, getter_AddRefs(literal));
  } else {
    // ok, no such string, so just put the match text itself there
    rv = gRDFService->GetLiteral(term->text.get(),
                                 getter_AddRefs(literal));
  }
  if (NS_FAILED(rv)) return rv;
  
  *aResult = literal;
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalHistory::Observe(nsISupports *aSubject, 
                         const char *aTopic,
                         const PRUnichar *aSomeData)
{
  nsresult rv;

  // pref changing - update member vars
  if (!nsCRT::strcmp(aTopic, "nsPref:changed")) {

    // expiration date
    if (!nsCRT::strcmp(aSomeData, NS_LITERAL_STRING(PREF_BROWSER_HISTORY_EXPIRE_DAYS).get())) {
      nsCOMPtr<nsIPref> prefs = do_GetService(kPrefCID, &rv);
      if (NS_SUCCEEDED(rv))
        prefs->GetIntPref(PREF_BROWSER_HISTORY_EXPIRE_DAYS, &mExpireDays);
    }

  }
  else if (!nsCRT::strcmp(aTopic, "profile-before-change")) {
    rv = CloseDB();    
    if (!nsCRT::strcmp(aSomeData, NS_LITERAL_STRING("shutdown-cleanse").get())) {
      nsCOMPtr <nsIFile> historyFile;
      rv = NS_GetSpecialDirectory(NS_APP_HISTORY_50_FILE, getter_AddRefs(historyFile));
      if (NS_SUCCEEDED(rv))
        rv = historyFile->Remove(PR_FALSE);
    }
  }
  else if (!nsCRT::strcmp(aTopic, "profile-do-change"))
    rv = OpenDB();

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
  nsDependentCString uri((const char*) yarn.mYarn_Buf, yarn.mYarn_Fill);

  nsresult rv;
  nsCOMPtr<nsIRDFResource> resource;
  rv = gRDFService->GetResource(nsCAutoString(uri).get(), getter_AddRefs(resource));
  if (NS_FAILED(rv)) return rv;

  *aResult = resource;
  NS_ADDREF(*aResult);
  return NS_OK;
}

//----------------------------------------------------------------------
// nsGlobalHistory::SearchEnumerator
//
//   Implementation

nsGlobalHistory::SearchEnumerator::~SearchEnumerator()
{
  // free up the query
  
}


// convert the query in mQuery into a find URI
// if there is a groupby= in the query, then convert that
// into the start of another search term
// for example, in the following query with one term:
//
// term[0] = { history, AgeInDays, is, 0 }
// groupby = Hostname
//
// we generate the following uri:
//
// find:datasource=history&match=AgeInDays&method=is&text=0&datasource=history
//   &match=Hostname&method=is&text=
//
// and then the caller will append some text after after the "text="
//
void
nsGlobalHistory::GetFindUriPrefix(const searchQuery& aQuery,
                                  const PRBool aDoGroupBy,
                                  nsAWritableCString& aResult)
{
  mdb_err err;
  
  aResult.Assign("find:");
  PRUint32 length = aQuery.terms.Count();
  PRUint32 i;
  
  for (i=0; i<length; i++) {
    searchTerm *term = (searchTerm*)aQuery.terms[i];
    if (i != 0)
      aResult.Append('&');
    aResult.Append("datasource=");
    aResult.Append(term->datasource);
    
    aResult.Append("&match=");
    aResult.Append(term->property);
    
    aResult.Append("&method=");
    aResult.Append(term->method);

    aResult.Append("&text=");
    aResult.Append(NS_ConvertUCS2toUTF8(term->text));
  }

  if (aQuery.groupBy == 0) return;

  // find out the name of the column we're grouping by
  char groupby[100];
  mdbYarn yarn = { groupby, 0, sizeof(groupby), 0, 0, nsnull };
  err = mStore->TokenToString(mEnv, aQuery.groupBy, &yarn);
  
  // put a "groupby=<colname>"
  if (aDoGroupBy) {
    aResult.Append("&groupby=");
    if (err == 0)
      aResult.Append((const char*)yarn.mYarn_Buf, yarn.mYarn_Fill);
  }

  // put &datasource=history&match=<colname>&method=is&text=
  else {
    // if the query has a groupby=<foo> then we want to append that
    // field as the last field to match.. caller has to be sure to
    // append that!
    aResult.Append("&datasource=history");
    
    aResult.Append("&match=");
    if (err == 0)
      aResult.Append((const char*)yarn.mYarn_Buf, yarn.mYarn_Fill);
    // herep  
    aResult.Append("&method=is");
    aResult.Append("&text=");
  }
  
}

//
// determines if the given row matches all terms
//
// if there is a "groupBy" column, then we have to remember that we've
// seen a row with the given value in that column, and then make sure
// all future rows with that value in that column DON'T match, no
// matter if they match the terms or not.
PRBool
nsGlobalHistory::SearchEnumerator::IsResult(nsIMdbRow *aRow)
{
  mdb_err err;

  mdbYarn groupColumnValue = { nsnull, 0, 0, 0, 0, nsnull};
  if (mQuery->groupBy!=0) {

    // if we have a 'groupby', then we use the hashtable to make sure
    // we only match the FIRST row with the column value that we're
    // grouping by
    
    err = mCurrent->AliasCellYarn(mEnv, mQuery->groupBy, &groupColumnValue);
    if (err!=0) return PR_FALSE;

    nsCStringKey key(nsCAutoString((const char*)groupColumnValue.mYarn_Buf,
                                      groupColumnValue.mYarn_Fill));

    void *otherRow = mUniqueRows.Get(&key);

    // Hey! we've seen this row before, so ignore it
    if (otherRow) return PR_FALSE;
  }

  // now do the actual match
  if (!mHistory->RowMatches(mCurrent, mQuery))
    return PR_FALSE;

  if (mQuery->groupBy != 0) {
    // we got this far, so we must have matched.
    // add ourselves to the hashtable so we don't match rows like this
    // in the future
    nsCStringKey key(nsCAutoString((const char*)groupColumnValue.mYarn_Buf,
                                      groupColumnValue.mYarn_Fill));
    
    // note - weak ref, don't worry about releasing
    mUniqueRows.Put(&key, (void *)mCurrent);
  }

  return PR_TRUE;
}

//
// determines if the row matches the given terms, used above
//
PRBool
nsGlobalHistory::RowMatches(nsIMdbRow *aRow,
                            searchQuery *aQuery)
{
  PRUint32 length = aQuery->terms.Count();
  PRUint32 i;

  for (i=0; i<length; i++) {
    
    searchTerm *term = (searchTerm*)aQuery->terms[i];

    if (!term->datasource.Equals("history"))
      continue;                 // we only match against history queries
    
    // use callback if it exists
    if (term->match) {
      // queue up some values just in case callback needs it
      // (how would we do this dynamically?)
      matchSearchTerm_t matchSearchTerm = { mEnv, mStore, term , PR_FALSE};
      
      if (!term->match(aRow, (void *)&matchSearchTerm))
        return PR_FALSE;
    } else {
      mdb_err err;

      mdb_column property_column;
      nsCAutoString property_name(term->property.get(),term->property.Length());
      property_name.Append(char(0));
      
      err = mStore->QueryToken(mEnv, property_name.get(), &property_column);
      if (err != 0) {
        NS_WARNING("Unrecognized column!");
        continue;               // assume we match???
      }
      
      // match the term directly against the column?
      mdbYarn yarn;
      aRow->AliasCellYarn(mEnv, property_column, &yarn);

      nsDependentCString rowVal((const char *)yarn.mYarn_Buf, yarn.mYarn_Fill);

      // set up some iterators
      nsACString::const_iterator start, end;
      rowVal.BeginReading(start);
      rowVal.EndReading(end);
      
      // is the cell in unicode or not? Hmm...let's assume so?
      NS_ConvertUCS2toUTF8 utf8Value(term->text);
      
      if (term->method.Equals("is")) {

        if (utf8Value != rowVal)
          return PR_FALSE;
      }

      else if (term->method.Equals("isnot")) {
        if (utf8Value == rowVal)
          return PR_FALSE;
      }

      else if (term->method.Equals("contains")) {
        if (!FindInReadable(utf8Value, start, end))
          return PR_FALSE;
      }

      else if (term->method.Equals("doesntcontain")) {
        if (FindInReadable(utf8Value, start, end))
          return PR_FALSE;
      }

      else if (term->method.Equals("startswith")) {
        // need to make sure that the found string is 
        // at the beginning of the string
        nsACString::const_iterator real_start = start;
        if (!(FindInReadable(utf8Value, start, end) &&
              real_start == start))
          return PR_FALSE;
      }

      else if (term->method.Equals("endswith")) {
        // need to make sure that the found string ends
        // at the end of the string
        nsACString::const_iterator real_end = end;
        if (!(RFindInReadable(utf8Value, start, end) &&
              real_end == end))
          return PR_FALSE;
      }

      else {
        NS_WARNING("Unrecognized search method in SearchEnumerator::RowMatches");
        // don't handle other match types like isgreater/etc yet,
        // so assume the match failed and bail
        return PR_FALSE;
      }
      
    }
  }
  
  // we've gone through each term and didn't bail, so they must have
  // all matched!
  return PR_TRUE;
}

// 
// return either the row, or another find resource.
// if we're doing grouping, then we don't want to return a real row,
// instead we want to expand the current query into a deeper query
// where we match up the groupby attribute.
// if we're not doing grouping, then we just return the URL for the
// current row
nsresult
nsGlobalHistory::SearchEnumerator::ConvertToISupports(nsIMdbRow* aRow,
                                                      nsISupports** aResult)

{
  mdb_err err;
  nsresult rv;
  
  nsCOMPtr<nsIRDFResource> resource;
  if (mQuery->groupBy == 0) {
    // no column to group by
    // just create a resource based on the URL of the current row
    mdbYarn yarn;
    err = aRow->AliasCellYarn(mEnv, mHistory->kToken_URLColumn, &yarn);
    if (err != 0) return NS_ERROR_FAILURE;

    
    nsDependentCString uri((const char*)yarn.mYarn_Buf, yarn.mYarn_Fill);

    rv = gRDFService->GetResource(nsCAutoString(uri).get(),
                                  getter_AddRefs(resource));
    if (NS_FAILED(rv)) return rv;

    *aResult = resource;
    NS_ADDREF(*aResult);
    return NS_OK;
  }

  // we have a group by, so now we recreate the find url, but add a
  // query for the row asked for by groupby
  mdbYarn groupByValue;
  err = aRow->AliasCellYarn(mEnv, mQuery->groupBy, &groupByValue);
  if (err != 0) return NS_ERROR_FAILURE;

  if (mFindUriPrefix.IsEmpty())
    mHistory->GetFindUriPrefix(*mQuery, PR_FALSE, mFindUriPrefix);
  
  nsCAutoString findUri(mFindUriPrefix);

  nsDependentCString rowValue((const char *)groupByValue.mYarn_Buf,
                            groupByValue.mYarn_Fill);
  
  findUri.Append(rowValue);
  findUri.Append('\0');

  rv = gRDFService->GetResource(findUri.get(), getter_AddRefs(resource));
  if (NS_FAILED(rv)) return rv;

  *aResult = resource;
  NS_ADDREF(*aResult);
  return NS_OK;
}

//----------------------------------------------------------------------
//
// nsGlobalHistory::AutoCompleteEnumerator
//
//   Implementation

nsGlobalHistory::AutoCompleteEnumerator::~AutoCompleteEnumerator()
{
}


PRBool
nsGlobalHistory::AutoCompleteEnumerator::IsResult(nsIMdbRow* aRow)
{
  nsCAutoString url;
  mHistory->GetRowValue(aRow, mURLColumn, url);
  
  nsAutoString url2;
  url2.AssignWithConversion(url.get());
  PRBool result = mHistory->AutoCompleteCompare(url2, mSelectValue, mExclude); 
  
  return result;
}

nsresult
nsGlobalHistory::AutoCompleteEnumerator::ConvertToISupports(nsIMdbRow* aRow, nsISupports** aResult)
{
  nsCAutoString url;
  mHistory->GetRowValue(aRow, mURLColumn, url);
  nsAutoString comments;
  mHistory->GetRowValue(aRow, mCommentColumn, comments);

  nsCOMPtr<nsIAutoCompleteItem> newItem(do_CreateInstance(NS_AUTOCOMPLETEITEM_CONTRACTID));
  NS_ENSURE_TRUE(newItem, NS_ERROR_FAILURE);

  newItem->SetValue(NS_ConvertASCIItoUCS2(url.get()));
  
  newItem->SetComment(comments.get());

  *aResult = newItem;
  NS_ADDREF(*aResult);
  return NS_OK;
}

//----------------------------------------------------------------------
//
// nsIAutoCompleteSession implementation
//

NS_IMETHODIMP 
nsGlobalHistory::OnStartLookup(const PRUnichar *searchString,
                               nsIAutoCompleteResults *previousSearchResult,
                               nsIAutoCompleteListener *listener)
{
  NS_ASSERTION(searchString, "searchString can't be null, fix your caller");
  NS_ENSURE_SUCCESS(OpenDB(), NS_ERROR_FAILURE);

  if (!listener)
    return NS_ERROR_NULL_POINTER;
      
  nsresult rv = NS_OK;

  nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID, &rv));
  if (NS_FAILED(rv)) return rv;

  PRBool enabled = PR_FALSE;
  prefs->GetBoolPref(PREF_AUTOCOMPLETE_ENABLED, &enabled);      

  if (!enabled || searchString[0] == 0) {
    listener->OnAutoComplete(nsnull, nsIAutoCompleteStatus::ignored);
    return NS_OK;
  }
  
  nsCOMPtr<nsIAutoCompleteResults> results;
  results = do_CreateInstance(NS_AUTOCOMPLETERESULTS_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  AutoCompleteStatus status = nsIAutoCompleteStatus::failed;

  // if the search string is empty after it has had prefixes removed, then 
  // there is no need to proceed with the search
  nsAutoString cut(searchString);
  AutoCompleteCutPrefix(cut, nsnull);
  if (cut.Length() == 0) {
    listener->OnAutoComplete(results, status);
    return NS_OK;
  }
  
  // pass string through filter and then determine which prefixes to exclude
  // when chopping prefixes off of history urls during comparison
  nsSharableString filtered = AutoCompletePrefilter(nsDependentString(searchString));
  AutocompleteExclude exclude;
  AutoCompleteGetExcludeInfo(filtered, &exclude);
  
  // perform the actual search here
  rv = AutoCompleteSearch(filtered, &exclude, previousSearchResult, results);

  // describe the search results
  if (NS_SUCCEEDED(rv)) {
  
    results->SetSearchString(searchString);
    results->SetDefaultItemIndex(0);
  
    // determine if we have found any matches or not
    nsCOMPtr<nsISupportsArray> array;
    rv = results->GetItems(getter_AddRefs(array));
    if (NS_SUCCEEDED(rv)) {
      PRUint32 nbrOfItems;
      rv = array->Count(&nbrOfItems);
      if (NS_SUCCEEDED(rv)) {
        if (nbrOfItems >= 1) {
          status = nsIAutoCompleteStatus::matchFound;
        } else {
          status = nsIAutoCompleteStatus::noMatch;
        }
      }
    }
    
    // notify the listener
    listener->OnAutoComplete(results, status);
  }
  
  return NS_OK;
}


NS_IMETHODIMP
nsGlobalHistory::OnStopLookup()
{
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalHistory::OnAutoComplete(const PRUnichar *searchString,
                                nsIAutoCompleteResults *previousSearchResult,
                                nsIAutoCompleteListener *listener)
{
  return NS_OK;
}

//----------------------------------------------------------------------
//
// AutoComplete stuff
//

nsresult
nsGlobalHistory::AutoCompleteSearch(const nsAReadableString& aSearchString,
                                    AutocompleteExclude* aExclude,
                                    nsIAutoCompleteResults* aPrevResults,
                                    nsIAutoCompleteResults* aResults)
{
  // determine if we can skip searching the whole history and only search
  // through the previous search results
  PRBool searchPrevious = PR_FALSE;
  if (aPrevResults) {
    nsXPIDLString prevURL;
    aPrevResults->GetSearchString(getter_Copies(prevURL));
    nsDependentString prevURLStr(prevURL);
    // if search string begins with the previous search string, it's a go
    searchPrevious = Substring(aSearchString, 0, prevURLStr.Length()).Equals(prevURLStr);
  }
    
  nsCOMPtr<nsISupportsArray> resultItems;
  nsresult rv = aResults->GetItems(getter_AddRefs(resultItems));

  if (searchPrevious) {
    // searching through the previous results...
    
    nsCOMPtr<nsISupportsArray> prevResultItems;
    aPrevResults->GetItems(getter_AddRefs(prevResultItems));
    
    PRUint32 count;
    prevResultItems->Count(&count);
    for (PRUint32 i = 0; i < count; ++i) {
      nsCOMPtr<nsIAutoCompleteItem> item;
      prevResultItems->GetElementAt(i, getter_AddRefs(item));

      // make a copy of the value because AutoCompleteCompare
      // is destructive
      nsAutoString url;
      item->GetValue(url);
      
      if (AutoCompleteCompare(url, aSearchString, aExclude))
        resultItems->AppendElement(item);
    }    
  } else {
    // searching through the entire history...
        
    // prepare the search enumerator
    AutoCompleteEnumerator* enumerator;
    enumerator = new AutoCompleteEnumerator(this, kToken_URLColumn, 
                                            kToken_NameColumn, aSearchString, aExclude);
    rv = enumerator->Init(mEnv, mTable);
    if (NS_FAILED(rv)) return rv;
  
    // store hits in an auto array initially
    nsAutoVoidArray array;
      
    // not using nsCOMPtr here to avoid time spent
    // refcounting while passing these around between the 3 arrays
    nsISupports* entry; 

    // step through the enumerator to get the items into 'array'
    // because we don't know how many items there will be
    PRBool hasMore;
    while (PR_TRUE) {
      enumerator->HasMoreElements(&hasMore);
      if (!hasMore) break;
      
      // addref's each entry as it enters 'array'
      enumerator->GetNext(&entry);
      array.AppendElement(entry);
    }
  
    // turn auto array into flat array for quick sort, now that we
    // know how many items there are
    PRUint32 count = array.Count();
    nsIAutoCompleteItem** items = new nsIAutoCompleteItem*[count];
    PRUint32 i;
    for (i = 0; i < count; ++i)
      items[i] = (nsIAutoCompleteItem*)array.ElementAt(i);
    
    // sort it
    NS_QuickSort(items, count, sizeof(nsIAutoCompleteItem*),
                 AutoCompleteSortComparison, nsnull);
  
    // place the sorted array into the autocomplete results
    for (i = 0; i < count; ++i) {
      nsISupports* item = (nsISupports*)items[i];
      resultItems->AppendElement(item);
      NS_IF_RELEASE(item); // release manually since we didn't use nsCOMPtr above
    }
    
    delete[] items;
  }
    
  return NS_OK;
}

// If aURL begins with a protocol or domain prefix from our lists,
// then mark their index in an AutocompleteExclude struct.
void
nsGlobalHistory::AutoCompleteGetExcludeInfo(nsAReadableString& aURL, AutocompleteExclude* aExclude)
{
  aExclude->schemePrefix = -1;
  aExclude->hostnamePrefix = -1;
  
  PRInt32 index = 0;
  PRInt32 i;
  for (i = 0; i < mIgnoreSchemes.Count(); ++i) {
    nsString* string = mIgnoreSchemes.StringAt(i);    
    if (Substring(aURL, 0, string->Length()).Equals(*string)) {
      aExclude->schemePrefix = i;
      index = string->Length();
      break;
    }
  }
  
  for (i = 0; i < mIgnoreHostnames.Count(); ++i) {
    nsString* string = mIgnoreHostnames.StringAt(i);    
    if (Substring(aURL, index, string->Length()).Equals(*string)) {
      aExclude->hostnamePrefix = i;
      break;
    }
  }
}

// Cut any protocol and domain prefixes from aURL, except for those which
// are specified in aExclude
void
nsGlobalHistory::AutoCompleteCutPrefix(nsAWritableString& aURL, AutocompleteExclude* aExclude)
{
  // This comparison is case-sensitive.  Therefore, it assumes that aUserURL is a 
  // potential URL whose host name is in all lower case.
  PRInt32 idx = 0;
  PRInt32 i;
  for (i = 0; i < mIgnoreSchemes.Count(); ++i) {
    if (aExclude && i == aExclude->schemePrefix)
      continue;
    nsString* string = mIgnoreSchemes.StringAt(i);    
    if (Substring(aURL, 0, string->Length()).Equals(*string)) {
      idx = string->Length();
      break;
    }
  }

  if (idx > 0)
    aURL.Cut(0, idx);

  idx = 0;
  for (i = 0; i < mIgnoreHostnames.Count(); ++i) {
    if (aExclude && i == aExclude->hostnamePrefix)
      continue;
    nsString* string = mIgnoreHostnames.StringAt(i);    
    if (Substring(aURL, 0, string->Length()).Equals(*string)) {
      idx = string->Length();
      break;
    }
  }

  if (idx > 0)
    aURL.Cut(0, idx);
}

nsSharableString
nsGlobalHistory::AutoCompletePrefilter(const nsAReadableString& aSearchString)
{
  nsAutoString url(aSearchString);

  PRInt32 slash = url.FindChar('/', 0);
  if (slash >= 0) {
    // if user is typing a url but has already typed past the host,
    // then convert the host to lowercase
    nsAutoString host;
    url.Left(host, slash);
    host.ToLowerCase();
    url.Assign(host + Substring(url, slash, url.Length()-slash));
  } else {
    // otherwise, assume the user could still be typing the host, and
    // convert everything to lowercase
    url.ToLowerCase();
  }
  
  return nsSharableString(url);
}

PRBool
nsGlobalHistory::AutoCompleteCompare(nsAString& aHistoryURL, 
                                     const nsAReadableString& aUserURL, 
                                     AutocompleteExclude* aExclude)
{
  AutoCompleteCutPrefix(aHistoryURL, aExclude);
  
  return Substring(aHistoryURL, 0, aUserURL.Length()).Equals(aUserURL);
}

int PR_CALLBACK 
AutoCompleteSortComparison(const void *v1, const void *v2, void *unused) 
{
  nsIAutoCompleteItem *item1 = *(nsIAutoCompleteItem**) v1;
  nsIAutoCompleteItem *item2 = *(nsIAutoCompleteItem**) v2;
  
  nsAutoString s1;
  item1->GetValue(s1);
  nsAutoString s2;
  item2->GetValue(s2);
  
  return nsCRT::strcmp(s1.get(), s2.get());
}

