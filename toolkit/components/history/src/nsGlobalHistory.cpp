/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 *   Blake Ross <blaker@netscape.com>
 *   Chris Sears <cbsears_sf@yahoo.com>
 *   Michael Lowe <michael.lowe@bigfoot.com>
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

/*

  A global browser history implementation that also supports the RDF
  datasource interface.

  TODO

  1) Hook up Assert() etc. so that we can delete stuff.

*/
#include "nsNetUtil.h"
#include "nsGlobalHistory.h"
#include "nsCRT.h"
#include "nsIEnumerator.h"
#include "nsIServiceManager.h"
#include "nsEnumeratorUtils.h"
#include "nsRDFCID.h"
#include "nsIDirectoryService.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "nsXPIDLString.h"
#include "plhash.h"
#include "plstr.h"
#include "prprf.h"
#include "prtime.h"
#include "rdf.h"
#include "nsCOMArray.h"
#include "nsIIOService.h"
#include "nsILocalFile.h"

#include "nsIURL.h"
#include "nsNetCID.h"

#include "nsInt64.h"
#include "nsMorkCID.h"
#include "nsIMdbFactoryFactory.h"

#include "nsIPrefService.h"
#include "nsIPrefBranch2.h"

#include "nsIObserverService.h"
#include "nsITextToSubURI.h"

#include "nsIGenericFactory.h"
#include "nsToolkitCompsCID.h"
#include "nsDocShellCID.h"

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
nsIRDFResource* nsGlobalHistory::kNC_HistoryByDateAndSite;
nsIRDFResource* nsGlobalHistory::kNC_HistoryByDate;
nsIRDFResource* nsGlobalHistory::kNC_DayFolderIndex;
nsIMdbFactory* nsGlobalHistory::gMdbFactory = nsnull;
nsIPrefBranch* nsGlobalHistory::gPrefBranch = nsnull;

#define PREF_BRANCH_BASE                        "browser."
#define PREF_BROWSER_HISTORY_EXPIRE_DAYS        "history_expire_days"
#define PREF_AUTOCOMPLETE_ONLY_TYPED            "urlbar.matchOnlyTyped"
#define PREF_AUTOCOMPLETE_ENABLED               "urlbar.autocomplete.enabled"

#define FIND_BY_AGEINDAYS_PREFIX "find:datasource=history&match=AgeInDays&method="

// see bug #319004 -- clamp title and URL to generously-large but not too large
// length
#define HISTORY_URI_LENGTH_MAX 65536
#define HISTORY_TITLE_LENGTH_MAX 4096

// sync history every 10 seconds
#define HISTORY_SYNC_TIMEOUT (10 * PR_MSEC_PER_SEC)
//#define HISTORY_SYNC_TIMEOUT 3000 // every 3 seconds - testing only!

// the value of mLastNow expires every 3 seconds
#define HISTORY_EXPIRE_NOW_TIMEOUT (3 * PR_MSEC_PER_SEC)

#define MSECS_PER_DAY (PR_MSEC_PER_SEC * 60 * 60 * 24)

//----------------------------------------------------------------------
//
// CIDs

static NS_DEFINE_CID(kRDFServiceCID,        NS_RDFSERVICE_CID);

// closure structures for RemoveMatchingRows
struct matchExpiration_t {
  PRTime *expirationDate;
  nsGlobalHistory *history;
};

struct matchHost_t {
  const char *host;
  PRBool entireDomain;          // should we delete the entire domain?
  nsGlobalHistory *history;
};

struct matchSearchTerm_t {
  nsIMdbEnv *env;
  nsIMdbStore *store;
  
  searchTerm *term;
  PRBool haveClosure;           // are the rest of the fields valid?
  PRTime now;
  PRInt32 intValue;
};

struct matchQuery_t {
  searchQuery* query;
  nsGlobalHistory* history;
};

// simple token/value struct
class tokenPair {
public:
  tokenPair(const char *aName, PRUint32 aNameLen,
            const char *aValue, PRUint32 aValueLen) :
    tokenName(aName), tokenNameLength(aNameLen),
    tokenValue(aValue), tokenValueLength(aValueLen) { MOZ_COUNT_CTOR(tokenPair); }
  ~tokenPair() { MOZ_COUNT_DTOR(tokenPair); }
  const char* tokenName;
  PRUint32 tokenNameLength;
  const char* tokenValue;
  PRUint32 tokenValueLength;
};

// individual search term, pulled from token/value structs
class searchTerm {
public:
  searchTerm(const char* aDatasource, PRUint32 aDatasourceLen,
             const char *aProperty, PRUint32 aPropertyLen,
             const char* aMethod, PRUint32 aMethodLen,
             const char* aText, PRUint32 aTextLen):
    datasource(aDatasource, aDatasource+aDatasourceLen),
    property(aProperty, aProperty+aPropertyLen),
    method(aMethod, aMethod+aMethodLen)
  {
    MOZ_COUNT_CTOR(searchTerm);
    nsresult rv;
    nsCOMPtr<nsITextToSubURI> textToSubURI = do_GetService(NS_ITEXTTOSUBURI_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
      textToSubURI->UnEscapeAndConvert("UTF-8", PromiseFlatCString(Substring(aText, aText + aTextLen)).get(), getter_Copies(text));
  }
  ~searchTerm() {
    MOZ_COUNT_DTOR(searchTerm);
  }
  
  nsDependentCSubstring datasource;  // should always be "history" ?
  nsDependentCSubstring property;    // AgeInDays, Hostname, etc
  nsDependentCSubstring method;      // is, isgreater, isless
  nsXPIDLString text;          // text to match
  rowMatchCallback match;      // matching callback if needed
};

// list of terms, plus an optional groupby column
struct searchQuery {
  nsVoidArray terms;            // array of searchTerms
  mdb_column groupBy;           // column to group by
};

static PRBool HasCell(nsIMdbEnv *aEnv, nsIMdbRow* aRow, mdb_column aCol)
{
  mdbYarn yarn;
  mdb_err err = aRow->AliasCellYarn(aEnv, aCol, &yarn);

  // no cell
  if (err != 0)
    return PR_FALSE;

  // if we have the cell, make sure it has a value??
  return (yarn.mYarn_Fill != 0);
}

static PRTime
NormalizeTime(PRTime aTime)
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
GetAgeInDays(PRTime aNormalizedNow, PRTime aDate)
{
  PRTime dateMidnight = NormalizeTime(aDate);

  PRTime diff;
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
nsGlobalHistory::MatchExpiration(nsIMdbRow *row, PRTime* expirationDate)
{
  nsresult rv;
  
  // hidden and typed urls always match because they're invalid,
  // so we want to expire them asap.  (if they were valid, they'd
  // have been unhidden -- see AddExistingPageToDatabase)
  if (HasCell(mEnv, row, kToken_HiddenColumn) && HasCell(mEnv, row, kToken_TypedColumn))
    return PR_TRUE;

  PRTime lastVisitedTime;
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
    // Need to create an nsAutoString to use ToInteger
    matchSearchTerm->intValue = nsAutoString(term->text).ToInteger(&err);
    matchSearchTerm->now = NormalizeTime(PR_Now());
    if (err != 0) return PR_FALSE;
    matchSearchTerm->haveClosure = PR_TRUE;
  }
  
  // XXX convert the property to a column, get the column value

  mdb_column column;
  mdb_err err = store->StringToToken(env, "LastVisitDate", &column);
  if (err != 0) return PR_FALSE;

  mdbYarn yarn;
  err = row->AliasCellYarn(env, column, &yarn);
  if (err != 0) return PR_FALSE;
  
  PRTime rowDate;
  PR_sscanf((const char*)yarn.mYarn_Buf, "%lld", &rowDate);

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

static PRBool
matchQueryCallback(nsIMdbRow *row, void *aClosure)
{
  matchQuery_t *query = (matchQuery_t*)aClosure;
  return query->history->RowMatches(row, query->query, PR_TRUE);
}
//----------------------------------------------------------------------

nsMdbTableEnumerator::nsMdbTableEnumerator()
  : mEnv(nsnull),
    mTable(nsnull),
    mCursor(nsnull),
    mCurrent(nsnull)
{
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
  NS_ADDREF(mEnv);

  mTable = aTable;
  NS_ADDREF(mTable);

  mdb_err err;
  err = mTable->GetTableRowCursor(mEnv, -1, &mCursor);
  if (err != 0) return NS_ERROR_FAILURE;

  return NS_OK;
}


nsMdbTableEnumerator::~nsMdbTableEnumerator()
{
  NS_IF_RELEASE(mCurrent);

  NS_IF_RELEASE(mCursor);

  NS_IF_RELEASE(mTable);

  NS_IF_RELEASE(mEnv);
}


NS_IMPL_ISUPPORTS1(nsMdbTableEnumerator, nsISimpleEnumerator)

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
      NS_RELEASE(mCurrent);
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

  NS_RELEASE(mCurrent);
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
    mAutocompleteOnlyTyped(PR_FALSE),
    mBatchesInProgress(0),
    mNowValid(PR_FALSE),
    mDirty(PR_FALSE),
    mEnv(nsnull),
    mStore(nsnull),
    mTable(nsnull)
{
  LL_I2L(mFileSizeOnDisk, 0);
  
  // commonly used prefixes that should be chopped off all 
  // history and input urls before comparison

  mIgnoreSchemes.AppendString(NS_LITERAL_STRING("http://"));
  mIgnoreSchemes.AppendString(NS_LITERAL_STRING("https://"));
  mIgnoreSchemes.AppendString(NS_LITERAL_STRING("ftp://"));
  mIgnoreHostnames.AppendString(NS_LITERAL_STRING("www."));
  mIgnoreHostnames.AppendString(NS_LITERAL_STRING("ftp."));
  
  mTypedHiddenURIs.Init(3);
}

nsGlobalHistory::~nsGlobalHistory()
{
  gRDFService->UnregisterDataSource(this);

  nsresult rv;
  rv = CloseDB();

  NS_IF_RELEASE(mTable);
  NS_IF_RELEASE(mStore);
  
  if (--gRefCnt == 0) {
    NS_IF_RELEASE(gRDFService);

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
    NS_IF_RELEASE(kNC_HistoryByDateAndSite);
    NS_IF_RELEASE(kNC_HistoryByDate);
    NS_IF_RELEASE(kNC_DayFolderIndex);
    
    NS_IF_RELEASE(gMdbFactory);
    NS_IF_RELEASE(gPrefBranch);
  }

  NS_IF_RELEASE(mEnv);
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

NS_IMPL_CYCLE_COLLECTION_CLASS(nsGlobalHistory)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsGlobalHistory)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mObservers)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsGlobalHistory)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mObservers)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END


NS_IMPL_CYCLE_COLLECTING_ADDREF_AMBIGUOUS(nsGlobalHistory, nsIBrowserHistory)
NS_IMPL_CYCLE_COLLECTING_RELEASE_AMBIGUOUS(nsGlobalHistory, nsIBrowserHistory)

NS_INTERFACE_MAP_BEGIN(nsGlobalHistory)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsIGlobalHistory2, nsIGlobalHistory3)
  NS_INTERFACE_MAP_ENTRY(nsIGlobalHistory3)
  NS_INTERFACE_MAP_ENTRY(nsIBrowserHistory)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIRDFDataSource)
  NS_INTERFACE_MAP_ENTRY(nsIRDFRemoteDataSource)
  NS_INTERFACE_MAP_ENTRY(nsIAutoCompleteSearch)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIBrowserHistory)
  NS_INTERFACE_MAP_ENTRIES_CYCLE_COLLECTION(nsGlobalHistory)
NS_INTERFACE_MAP_END

//----------------------------------------------------------------------
//
// nsGlobalHistory
//
//   nsIGlobalHistory2 methods
//

NS_IMETHODIMP
nsGlobalHistory::AddURI(nsIURI *aURI, PRBool aRedirect, PRBool aTopLevel, nsIURI *aReferrer)
{
  PRTime now = GetNow();

  return AddPageToDatabase(aURI, aRedirect, aTopLevel, now, aReferrer);
}

nsresult
nsGlobalHistory::AddPageToDatabase(nsIURI* aURI, PRBool aRedirect, PRBool aTopLevel,
                                   PRTime aLastVisitDate, nsIURI *aReferrer)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(aURI);

  // If history is set to expire after 0 days,
  // then it's technically disabled. Don't even
  // bother adding the page
  if (mExpireDays == 0) {
    NS_WARNING("mExpireDays == 0");
    return NS_OK;
  }

  // filter out unwanted URIs such as chrome: mailbox: etc
  // The model is really if we don't know differently then add which basically
  // means we are suppose to try all the things we know not to allow in and
  // then if we don't bail go on and allow it in.  But here lets compare
  // against the most common case we know to allow in and go on and say yes
  // to it.

  PRBool isHTTP = PR_FALSE;
  PRBool isHTTPS = PR_FALSE;

  NS_ENSURE_SUCCESS(rv = aURI->SchemeIs("http", &isHTTP), rv);
  NS_ENSURE_SUCCESS(rv = aURI->SchemeIs("https", &isHTTPS), rv);

  if (!isHTTP && !isHTTPS) {
    PRBool isAbout, isImap, isNews, isMailbox, isViewSource, isChrome, isData;

    rv = aURI->SchemeIs("about", &isAbout);
    rv |= aURI->SchemeIs("imap", &isImap);
    rv |= aURI->SchemeIs("news", &isNews);
    rv |= aURI->SchemeIs("mailbox", &isMailbox);
    rv |= aURI->SchemeIs("view-source", &isViewSource);
    rv |= aURI->SchemeIs("chrome", &isChrome);
    rv |= aURI->SchemeIs("data", &isData);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

    if (isAbout || isImap || isNews || isMailbox || isViewSource || isChrome || isData) {
#ifdef DEBUG_bsmedberg
      printf("Filtering out unwanted scheme.\n");
#endif
      return NS_OK;
    }
  }

  rv = OpenDB();
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString URISpec;
  rv = aURI->GetSpec(URISpec);
  NS_ENSURE_SUCCESS(rv, rv);

  if (URISpec.Length() > HISTORY_URI_LENGTH_MAX)
     return NS_OK;

#ifdef DEBUG_bsmedberg
  printf("AddURI: %s%s%s",
         URISpec.get(),
         aRedirect ? ", redirect" : "",
         aTopLevel ? ", toplevel" : "");
#endif

  nsCOMPtr<nsIMdbRow> row;
  rv = FindRow(kToken_URLColumn, URISpec.get(), getter_AddRefs(row));

  if (NS_SUCCEEDED(rv)) {
    // update the database, and get the old info back
    PRTime oldDate;
    PRInt32 oldCount;
    rv = AddExistingPageToDatabase(row, aLastVisitDate, aReferrer, &oldDate, &oldCount);
    NS_ASSERTION(NS_SUCCEEDED(rv), "AddExistingPageToDatabase failed; see bug 88961");
    if (NS_FAILED(rv)) return rv;
    
#ifdef DEBUG_bsmedberg
    printf("Existing page succeeded.\n");
#endif
  }
  else {
    rv = AddNewPageToDatabase(aURI, aLastVisitDate, aRedirect, 
                              aTopLevel, aReferrer, getter_AddRefs(row));
    NS_ASSERTION(NS_SUCCEEDED(rv), "AddNewPageToDatabase failed; see bug 88961");
    if (NS_FAILED(rv)) return rv;

#ifdef DEBUG_bsmedberg
    printf("New page succeeded.\n");
#endif
  }

  // Store last visited page if we have the pref set accordingly
  if (aTopLevel) {
    PRInt32 choice = 0;
    if (NS_SUCCEEDED(gPrefBranch->GetIntPref("startup.page", &choice))) {
      if (choice != 2) {
        if (NS_SUCCEEDED(gPrefBranch->GetIntPref("windows.loadOnNewWindow", &choice))) {
          if (choice != 2) {
            gPrefBranch->GetIntPref("tabs.loadOnNewTab", &choice);
          }
        }
      }
    }
    if (choice == 2) {
      NS_ENSURE_STATE(mMetaRow);

      SetRowValue(mMetaRow, kToken_LastPageVisited, URISpec.get());
    }
  }
 
  SetDirty();
  
  return NS_OK;
}

nsresult
nsGlobalHistory::AddExistingPageToDatabase(nsIMdbRow *row,
                                           PRTime aDate,
                                           nsIURI* aReferrer,
                                           PRTime *aOldDate,
                                           PRInt32 *aOldCount)
{
  nsresult rv;
  nsCAutoString oldReferrer;
  
  nsCAutoString URISpec;
  rv = GetRowValue(row, kToken_URLColumn, URISpec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString referrerSpec;
  if (aReferrer) {
    rv = aReferrer->GetSpec(referrerSpec);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // if the page was typed, unhide it now because it's
  // known to be valid
  if (HasCell(mEnv, row, kToken_TypedColumn)) {
    mTypedHiddenURIs.Remove(URISpec);
    row->CutColumn(mEnv, kToken_HiddenColumn);
  }

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

  if (aReferrer) {
    rv = GetRowValue(row, kToken_ReferrerColumn, oldReferrer);
    // No referrer? Now there is!
    if ((NS_FAILED(rv) || oldReferrer.IsEmpty()))
       SetRowValue(row, kToken_ReferrerColumn, referrerSpec.get());
  }

  // Notify observers
  nsCOMPtr<nsIRDFResource> url;
  rv = gRDFService->GetResource(URISpec, getter_AddRefs(url));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRDFDate> date;
  rv = gRDFService->GetDateLiteral(aDate, getter_AddRefs(date));
  NS_ENSURE_SUCCESS(rv, rv);

  // visit date
  nsCOMPtr<nsIRDFDate> oldDateLiteral;
  rv = gRDFService->GetDateLiteral(*aOldDate, getter_AddRefs(oldDateLiteral));
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = NotifyChange(url, kNC_Date, oldDateLiteral, date);
  NS_ENSURE_SUCCESS(rv, rv);

  // visit count
  nsCOMPtr<nsIRDFInt> oldCountLiteral;
  rv = gRDFService->GetIntLiteral(*aOldCount, getter_AddRefs(oldCountLiteral));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIRDFInt> newCountLiteral;
  rv = gRDFService->GetIntLiteral(*aOldCount+1,
                                  getter_AddRefs(newCountLiteral));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NotifyChange(url, kNC_VisitCount, oldCountLiteral, newCountLiteral);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsGlobalHistory::AddNewPageToDatabase(nsIURI* aURI,
                                      PRTime aDate, 
                                      PRBool aRedirect,
                                      PRBool aTopLevel,
                                      nsIURI* aReferrer,
                                      nsIMdbRow **aResult)
{
  mdb_err err;
  
  NS_ENSURE_SUCCESS(OpenDB(), NS_ERROR_NOT_INITIALIZED);

  nsCAutoString URISpec;
  nsresult rv = aURI->GetSpec(URISpec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString referrerSpec;
  if (aReferrer) {
    rv = aReferrer->GetSpec(referrerSpec);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Create a new row
  mdbOid rowId;
  rowId.mOid_Scope = kToken_HistoryRowScope;
  rowId.mOid_Id    = mdb_id(-1);
  
  NS_PRECONDITION(mTable != nsnull, "not initialized");
  if (! mTable)
    return NS_ERROR_NOT_INITIALIZED;

  nsCOMPtr<nsIMdbRow> row;
  err = mTable->NewRow(mEnv, &rowId, getter_AddRefs(row));
  if (err != 0) return NS_ERROR_FAILURE;

  // Set the URL
  SetRowValue(row, kToken_URLColumn, URISpec.get());
  
  // Set the date.
  SetRowValue(row, kToken_LastVisitDateColumn, aDate);
  SetRowValue(row, kToken_FirstVisitDateColumn, aDate);

  // Set the referrer if there is one.
  if (aReferrer)
    SetRowValue(row, kToken_ReferrerColumn, referrerSpec.get());

  nsCOMPtr<nsIURI> uri;
  NS_NewURI(getter_AddRefs(uri), URISpec, nsnull, nsnull);
  nsCAutoString hostname;
  if (uri)
      uri->GetHost(hostname);

  // Strip www.
  if (Substring(hostname, 0, 4).EqualsLiteral("www."))
    hostname.Cut(0, 4);

  SetRowValue(row, kToken_HostnameColumn, hostname.get());

  *aResult = row;
  NS_ADDREF(*aResult);

  PRBool isJavascript;
  rv = aURI->SchemeIs("javascript", &isJavascript);
  NS_ENSURE_SUCCESS(rv, rv);

  if (isJavascript || aRedirect || !aTopLevel) {
    // if this is a JS url, or a redirected URI or in a frame, hide it in
    // global history so that it doesn't show up in the autocomplete
    // dropdown. AddExistingPageToDatabase has logic to override this
    // behavior for URIs which were typed. See bug 197127 and bug 161531
    // for details.
    rv = SetRowValue(row, kToken_HiddenColumn, 1);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // Notify observers
    nsCOMPtr<nsIRDFResource> url;
    rv = gRDFService->GetResource(URISpec, getter_AddRefs(url));
    NS_ENSURE_SUCCESS(rv, rv);

    nsCOMPtr<nsIRDFDate> date;
    rv = gRDFService->GetDateLiteral(aDate, getter_AddRefs(date));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = NotifyAssert(url, kNC_Date, date);
    if (NS_FAILED(rv)) return rv;

    rv = NotifyAssert(kNC_HistoryRoot, kNC_child, url);
    if (NS_FAILED(rv)) return rv;
  
    NotifyFindAssertions(url, row);
  }

  return NS_OK;
}

nsresult
nsGlobalHistory::RemovePageInternal(const char *aSpec)
{
  if (!mTable) return NS_ERROR_NOT_INITIALIZED;
  // find the old row, ignore it if we don't have it
  nsCOMPtr<nsIMdbRow> row;
  nsresult rv = FindRow(kToken_URLColumn, aSpec, getter_AddRefs(row));
  if (NS_FAILED(rv)) return NS_OK;

  // remove the row
  mdb_err err = mTable->CutRow(mEnv, row);
  NS_ENSURE_TRUE(err == 0, NS_ERROR_FAILURE);

  // if there are batches in progress, we don't want to notify
  // observers that we're deleting items. the caller promises
  // to handle whatever UI updating is necessary when we're finished.  
  if (!mBatchesInProgress) {
    // get the resource so we can do the notification
    nsCOMPtr<nsIRDFResource> oldRowResource;
    gRDFService->GetResource(nsDependentCString(aSpec), getter_AddRefs(oldRowResource));
    NotifyFindUnassertions(oldRowResource, row);
  }

  // not a fatal error if we can't cut all column
  err = row->CutAllColumns(mEnv);
  NS_ASSERTION(err == 0, "couldn't cut all columns");

  // Sigh. This is pretty bad - if the user selects a bunch of things then
  // hits delete we'll be re-writing history over and over. Still, we will
  // be whacking global history pretty hard after 1.0 so I don't feel too 
  // bad.
  return Commit(kCompressCommit);
}

nsresult
nsGlobalHistory::SetRowValue(nsIMdbRow *aRow, mdb_column aCol, const PRTime& aValue)
{
  mdb_err err;
  nsCAutoString val;
  val.AppendInt(aValue);

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
  PRUnichar *swapval = nsnull;

  // eventually turn this on when we're confident in mork's ability
  // to handle yarn forms properly
#if 0
  NS_ConvertUTF16toUTF8 utf8Value(aValue);
  printf("Storing utf8 value %s\n", utf8Value.get());
  mdbYarn yarn = { (void *)utf8Value.get(), utf8Value.Length(), utf8Value.Length(), 0, 1, nsnull };
#else

  if (mReverseByteOrder) {
    // The file is other-endian.  Byte-swap the value.
    swapval = (PRUnichar *)malloc(len);
    if (!swapval)
      return NS_ERROR_OUT_OF_MEMORY;
    SwapBytes(aValue, swapval, len / sizeof(PRUnichar));
    aValue = swapval;
  }
  mdbYarn yarn = { (void *)aValue, len, len, 0, 0, nsnull };
  
#endif
  err = aRow->AddColumn(mEnv, aCol, &yarn);
  if (swapval)
    free(swapval);
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
                             nsAString& aResult)
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
    if (mReverseByteOrder) {
      // The file is other-endian; we must byte-swap the result.
      PRUnichar *swapval;
      int len = yarn.mYarn_Fill / sizeof(PRUnichar);
      swapval = (PRUnichar *)malloc(yarn.mYarn_Fill);
      if (!swapval)
        return NS_ERROR_OUT_OF_MEMORY;
      SwapBytes((const PRUnichar *)yarn.mYarn_Buf, swapval, len);
      aResult.Assign(swapval, len);
      free(swapval);
    }
    else
      aResult.Assign((const PRUnichar *)yarn.mYarn_Buf, yarn.mYarn_Fill/sizeof(PRUnichar));
    break;

    // eventually we'll be supporting this in SetRowValue()
  case 1:                       // UTF8
    aResult.Assign(NS_ConvertUTF8toUTF16((const char*)yarn.mYarn_Buf, yarn.mYarn_Fill));
    break;

  default:
    return NS_ERROR_UNEXPECTED;
  }
  return NS_OK;
}

// Copy an array of 16-bit values, reversing the byte order.
void
nsGlobalHistory::SwapBytes(const PRUnichar *source, PRUnichar *dest,
                           PRInt32 aLen)
{
  PRUint16 c;
  const PRUnichar *inp;
  PRUnichar *outp;
  PRInt32 i;

  inp = source;
  outp = dest;
  for (i = 0; i < aLen; i++) {
    c = *inp++;
    *outp++ = (((c >> 8) & 0xff) | (c << 8));
  }
  return;
}
      
nsresult
nsGlobalHistory::GetRowValue(nsIMdbRow *aRow, mdb_column aCol,
                             PRTime *aResult)
{
  mdb_err err;
  
  mdbYarn yarn;
  err = aRow->AliasCellYarn(mEnv, aCol, &yarn);
  if (err != 0) return NS_ERROR_FAILURE;

  *aResult = LL_ZERO;
  
  if (!yarn.mYarn_Fill || !yarn.mYarn_Buf)
    return NS_OK;

  PR_sscanf((const char*)yarn.mYarn_Buf, "%lld", aResult);
  
  return NS_OK;
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
                             nsACString& aResult)
{
  mdb_err err;
  
  mdbYarn yarn;
  err = aRow->AliasCellYarn(mEnv, aCol, &yarn);
  if (err != 0) return NS_ERROR_FAILURE;

  const char* startPtr = (const char*)yarn.mYarn_Buf;
  if (startPtr)
    aResult.Assign(Substring(startPtr, startPtr + yarn.mYarn_Fill));
  else
    aResult.Truncate();
  
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalHistory::AddPageWithDetails(nsIURI *aURI, const PRUnichar *aTitle, 
                                    PRTime aLastVisitDate)
{
  nsresult rv = AddPageToDatabase(aURI, PR_FALSE, PR_TRUE, aLastVisitDate, nsnull);
  if (NS_FAILED(rv)) return rv;

  return SetPageTitle(aURI, nsDependentString(aTitle));
}

NS_IMETHODIMP
nsGlobalHistory::GetCount(PRUint32* aCount)
{
  NS_ENSURE_ARG_POINTER(aCount);
  NS_ENSURE_SUCCESS(OpenDB(), NS_ERROR_FAILURE);
  if (!mTable) return NS_ERROR_FAILURE;

  mdb_err err = mTable->GetCount(mEnv, aCount);
  return (err == 0) ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsGlobalHistory::SetPageTitle(nsIURI *aURI, const nsAString& aTitle)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(aURI);

  nsAutoString titleString(StringHead(aTitle, HISTORY_TITLE_LENGTH_MAX));
  if (titleString.Length() < aTitle.Length() &&
      NS_IS_HIGH_SURROGATE(titleString.Last()))
    titleString.Truncate(titleString.Length()-1);

  // skip about: URIs to avoid reading in the db (about:blank, especially)
  PRBool isAbout;
  rv = aURI->SchemeIs("about", &isAbout);
  NS_ENSURE_SUCCESS(rv, rv);
  if (isAbout) return NS_OK;

  NS_ENSURE_SUCCESS(OpenDB(), NS_ERROR_FAILURE);
  
  nsCAutoString URISpec;
  rv = aURI->GetSpec(URISpec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIMdbRow> row;
  rv = FindRow(kToken_URLColumn, URISpec.get(), getter_AddRefs(row));

  // if the row doesn't exist, we silently succeed
  if (rv == NS_ERROR_NOT_AVAILABLE) return NS_OK;
  NS_ENSURE_SUCCESS(rv, rv);

  // Get the old title so we can notify observers
  nsAutoString oldtitle;
  rv = GetRowValue(row, kToken_NameColumn, oldtitle);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFLiteral> oldname;
  if (!oldtitle.IsEmpty()) {
    rv = gRDFService->GetLiteral(oldtitle.get(), getter_AddRefs(oldname));
    if (NS_FAILED(rv)) return rv;
  }

  SetRowValue(row, kToken_NameColumn, titleString.get());

  // ...and update observers
  nsCOMPtr<nsIRDFResource> url;
  rv = gRDFService->GetResource(URISpec, getter_AddRefs(url));
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIRDFLiteral> name;
  rv = gRDFService->GetLiteral(titleString.get(), getter_AddRefs(name));
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
nsGlobalHistory::RemovePage(nsIURI *aURI)
{
  nsCAutoString spec;
  nsresult rv = aURI->GetSpec(spec);
  if (NS_SUCCEEDED(rv))
    rv = RemovePageInternal(spec.get());
  return rv;
}

NS_IMETHODIMP
nsGlobalHistory::RemovePagesFromHost(const nsACString &aHost, PRBool aEntireDomain)
{
  const nsCString &host = PromiseFlatCString(aHost);

  matchHost_t hostInfo;
  hostInfo.history = this;
  hostInfo.entireDomain = aEntireDomain;
  hostInfo.host = host.get();

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

  nsCOMPtr<nsIURI> uri;
  // do smart zero-termination
  const char* startPtr = (const char *)yarn.mYarn_Buf;
  rv = NS_NewURI(getter_AddRefs(uri),
                 Substring(startPtr, startPtr + yarn.mYarn_Fill));
  if (NS_FAILED(rv)) return PR_FALSE;

  nsCAutoString urlHost;
  rv = uri->GetHost(urlHost);
  if (NS_FAILED(rv)) return PR_FALSE;

  if (PL_strcmp(urlHost.get(), hostInfo->host) == 0)
    return PR_TRUE;

  // now try for a domain match, if necessary
  if (hostInfo->entireDomain) {
    // do a reverse-search to match the end of the string
    const char *domain = PL_strrstr(urlHost.get(), hostInfo->host);
    
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
  
  // Reset the file byte order.
  rv = InitByteOrder(PR_TRUE);
  if (NS_FAILED(rv)) return rv;

  return Commit(kCompressCommit);
}

nsresult
nsGlobalHistory::RemoveMatchingRows(rowMatchCallback aMatchFunc,
                                    void *aClosure,
                                    PRBool notify)
{
  NS_ENSURE_SUCCESS(OpenDB(), NS_ERROR_FAILURE);
  nsresult rv;
  if (!mTable) return NS_OK;

  mdb_err err;
  mdb_count count;
  err = mTable->GetCount(mEnv, &count);
  if (err != 0) return NS_ERROR_FAILURE;

  BeginUpdateBatch();

  // Begin the batch.
  int marker;
  err = mTable->StartBatchChangeHint(mEnv, &marker);
  NS_ASSERTION(err == 0, "unable to start batch");
  if (err != 0) return NS_ERROR_FAILURE;

  nsCOMPtr<nsIRDFResource> resource;
  // XXX from here until end batch, no early returns!
  for (mdb_pos pos = count - 1; pos >= 0; --pos) {
    nsCOMPtr<nsIMdbRow> row;
    err = mTable->PosToRow(mEnv, pos, getter_AddRefs(row));
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
      
      const char* startPtr = (const char*) yarn.mYarn_Buf;
      nsCAutoString uri(Substring(startPtr, startPtr+yarn.mYarn_Fill));
      rv = gRDFService->GetResource(uri, getter_AddRefs(resource));
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
  
    // possibly avoid leakage
    err = row->CutAllColumns(mEnv);
    NS_ASSERTION(err == 0, "couldn't cut all columns");
    // we'll notify regardless of whether we could successfully
    // CutAllColumns or not.
    
    
  }
  
  // Finish the batch.
  err = mTable->EndBatchChangeHint(mEnv, &marker);
  NS_ASSERTION(err == 0, "error ending batch");

  EndUpdateBatch();

  if (err != 0)
    return NS_ERROR_FAILURE;

  return Commit(kCompressCommit);
}

NS_IMETHODIMP
nsGlobalHistory::IsVisited(nsIURI* aURI, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(aURI);

  nsresult rv;
  NS_ENSURE_SUCCESS(OpenDB(), NS_ERROR_NOT_INITIALIZED);

  nsCAutoString URISpec;
  rv = aURI->GetSpec(URISpec);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = FindRow(kToken_URLColumn, URISpec.get(), nsnull);
  *_retval = NS_SUCCEEDED(rv);
  
  // Hidden, typed URIs haven't really been visited yet. They've only
  // been typed in and the actual load hasn't happened yet. We maintain
  // the list of hidden+typed URIs in memory in mTypedHiddenURIs because
  // the list will usually be small and checking the actual Mork row
  // would require several dynamic memory allocations.
  if (*_retval && mTypedHiddenURIs.Contains(URISpec))
  {
    *_retval = PR_FALSE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsGlobalHistory::GetLastPageVisited(nsACString& _retval)
{ 
  NS_ENSURE_SUCCESS(OpenDB(), NS_ERROR_FAILURE);

  NS_ENSURE_STATE(mMetaRow);

  mdb_err err = GetRowValue(mMetaRow, kToken_LastPageVisited, _retval);
  NS_ENSURE_TRUE(err == 0, NS_ERROR_FAILURE);
  
  return NS_OK;
}

// Set the byte order in the history file.  The given string value should
// be either "BE" (big-endian) or "LE" (little-endian).
nsresult
nsGlobalHistory::SaveByteOrder(const char *aByteOrder)
{
  if (PL_strcmp(aByteOrder, "BE") != 0 && PL_strcmp(aByteOrder, "LE") != 0) {
    NS_WARNING("Invalid byte order argument.");
    return NS_ERROR_INVALID_ARG;
  }
  NS_ENSURE_STATE(mMetaRow);

  mdb_err err = SetRowValue(mMetaRow, kToken_ByteOrder, aByteOrder);
  NS_ENSURE_TRUE(err == 0, NS_ERROR_FAILURE);

  return NS_OK;
}

// Get the file byte order.
nsresult
nsGlobalHistory::GetByteOrder(char **_retval)
{ 
  NS_ENSURE_SUCCESS(OpenDB(), NS_ERROR_FAILURE);

  NS_ENSURE_ARG_POINTER(_retval);
  NS_ENSURE_STATE(mMetaRow);

  nsCAutoString byteOrder;
  mdb_err err = GetRowValue(mMetaRow, kToken_ByteOrder, byteOrder);
  NS_ENSURE_TRUE(err == 0, NS_ERROR_FAILURE);

  *_retval = ToNewCString(byteOrder);
  NS_ENSURE_TRUE(*_retval, NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}


NS_IMETHODIMP
nsGlobalHistory::HidePage(nsIURI *aURI)
{
  nsresult rv;
  NS_ENSURE_ARG_POINTER(aURI);

  nsCAutoString URISpec;
  rv = aURI->GetSpec(URISpec);
  NS_ENSURE_SUCCESS(rv, rv);

  if (URISpec.Length() > HISTORY_URI_LENGTH_MAX)
     return NS_OK;

#ifdef DEBUG_bsmedberg
  printf("nsGlobalHistory::HidePage: %s\n", URISpec.get());
#endif
  
  nsCOMPtr<nsIMdbRow> row;

  rv = FindRow(kToken_URLColumn, URISpec.get(), getter_AddRefs(row));

  if (NS_FAILED(rv)) {
    // it hasn't been visited yet, but if one ever comes in, we need
    // to hide it when it is visited
    rv = AddURI(aURI, PR_FALSE, PR_FALSE, nsnull);
    if (NS_FAILED(rv)) return rv;
    
    rv = FindRow(kToken_URLColumn, URISpec.get(), getter_AddRefs(row));
    if (NS_FAILED(rv)) return rv;
  }

  rv = SetRowValue(row, kToken_HiddenColumn, 1);
  if (NS_FAILED(rv)) return rv;

  // now pretend as if this row was deleted
  // HasAssertion() correctly checks the Hidden column to show that
  // the row is hidden
  nsCOMPtr<nsIRDFResource> urlResource;
  rv = gRDFService->GetResource(URISpec, getter_AddRefs(urlResource));
  if (NS_FAILED(rv)) return rv;
  return NotifyFindUnassertions(urlResource, row);
}

NS_IMETHODIMP
nsGlobalHistory::MarkPageAsTyped(nsIURI *aURI)
{
  NS_ENSURE_ARG_POINTER(aURI);
  nsCAutoString spec;
  nsresult rv = aURI->GetSpec(spec);
  if (NS_FAILED(rv))
    return rv;
  
  if (spec.Length() > HISTORY_URI_LENGTH_MAX)
     return NS_OK;

  nsCOMPtr<nsIMdbRow> row;
  rv = FindRow(kToken_URLColumn, spec.get(), getter_AddRefs(row));
  if (NS_FAILED(rv)) {
    rv = AddNewPageToDatabase(aURI, GetNow(), PR_FALSE, PR_TRUE, nsnull, getter_AddRefs(row));
    NS_ENSURE_SUCCESS(rv, rv);

    // We don't know if this is a valid URI yet. Hide it until it finishes
    // loading.
    SetRowValue(row, kToken_HiddenColumn, 1);
    mTypedHiddenURIs.Put(spec);
  }
  
  return SetRowValue(row, kToken_TypedColumn, 1);
}

NS_IMETHODIMP
nsGlobalHistory::AddDocumentRedirect(nsIChannel *aOldChannel,
                                     nsIChannel *aNewChannel,
                                     PRInt32 aFlags,
                                     PRBool aTopLevel)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsGlobalHistory::SetURIGeckoFlags(nsIURI *aURI, PRUint32 aFlags)
{
  NS_ENSURE_ARG_POINTER(aURI);
  nsCAutoString spec;
  nsresult rv = aURI->GetSpec(spec);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIMdbRow> row;
  rv = FindRow(kToken_URLColumn, spec.get(), getter_AddRefs(row));
  if (NS_FAILED(rv))
    return rv;

  return SetRowValue(row, kToken_GeckoFlagsColumn, (PRInt32)aFlags);
}

NS_IMETHODIMP
nsGlobalHistory::GetURIGeckoFlags(nsIURI *aURI, PRUint32* aFlags)
{
  NS_ENSURE_ARG_POINTER(aURI);
  nsCAutoString spec;
  nsresult rv = aURI->GetSpec(spec);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIMdbRow> row;
  rv = FindRow(kToken_URLColumn, spec.get(), getter_AddRefs(row));
  if (NS_FAILED(rv))
    return rv;

  if (!HasCell(mEnv, row, kToken_GeckoFlagsColumn))
    return NS_ERROR_FAILURE;

  PRInt32 val;
  mdb_err err = GetRowValue(row, kToken_GeckoFlagsColumn, &val);
  NS_ENSURE_TRUE(err == 0, NS_ERROR_FAILURE);
  *aFlags = val;
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
        valueStr.AppendInt(n);
        
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
      URLEnumerator* result = new URLEnumerator(kToken_URLColumn, col,
                                                kToken_HiddenColumn,
                                                value, len);
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
       aSource == kNC_HistoryByDateAndSite ||
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
           (aProperty == kNC_URL) ||
           (aProperty == kNC_DayFolderIndex)) {

    const char* uri;
    rv = aSource->GetValueConst(&uri);
    if (NS_FAILED(rv)) return rv;

    // url is self-referential, so we'll always just return itself
    // however, don't return the URLs of find resources
    if (aProperty == kNC_URL && !IsFindResource(aSource)) {
      
      nsCOMPtr<nsIRDFLiteral> uriLiteral;
      rv = gRDFService->GetLiteral(NS_ConvertUTF8toUTF16(uri).get(), getter_AddRefs(uriLiteral));
      if (NS_FAILED(rv))    return(rv);
      *aTarget = uriLiteral;
      NS_ADDREF(*aTarget);
      return NS_OK;
    }

    // find URIs are special
    if (IsFindResource(aSource)) {
      if (aProperty == kNC_Name)
        return GetFindUriName(uri, aTarget);
        
      if (aProperty == kNC_NameSort) {
        // parse out the 'text' token
        nsVoidArray tokenList;
        FindUrlToTokenList(uri, tokenList);

        nsCOMPtr<nsIRDFLiteral> literal; 

        for (PRInt32 i = 0; i < tokenList.Count(); ++i) {
          tokenPair* token = NS_STATIC_CAST(tokenPair*, tokenList[i]);

          if (!strncmp(token->tokenName, "text", token->tokenNameLength)) {
            rv = gRDFService->GetLiteral(NS_ConvertUTF8toUTF16(Substring(token->tokenValue, token->tokenValue + token->tokenValueLength)).get(),
                                         getter_AddRefs(literal));
            // We don't break out of the loop here because there could be other text tokens in the string.
            // The last one is the most specific so wait and see if we've got one...
          }
        }

        FreeTokenList(tokenList);
  
        if (literal && NS_SUCCEEDED(rv)) {
          *aTarget = literal;
          NS_ADDREF(*aTarget);
          return NS_OK;
        }
        *aTarget = nsnull;
        return rv;
      }
    }
    
    // ok, we got this far, so we have to retrieve something from
    // the row in the database
    nsCOMPtr<nsIMdbRow> row;
    rv = FindRow(kToken_URLColumn, uri, getter_AddRefs(row));
    if (NS_FAILED(rv)) return NS_RDF_NO_VALUE;

    mdb_err err;
    // ...and then depending on the property they want, we'll pull the
    // cell they want out of it.
    if (aProperty == kNC_Date  ||
        aProperty == kNC_FirstVisitDate) {
      // Last visit date
      PRTime i;
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
      PRTime lastVisitDate;
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

        nsCAutoString filename;
        rv = urlObj->GetFileName(filename);
        if (NS_FAILED(rv) || filename.IsEmpty()) {
          // ok fine. we'll use the file path. then we give up!
          rv = urlObj->GetPath(filename);
          if (strcmp(filename.get(), "/") == 0) {
            // if the top of a site does not have a title
            // (common for redirections) then return the hostname
            rv = GetRowValue(row, kToken_HostnameColumn, filename);
          }
        }

        if (NS_FAILED(rv)) return rv;
        
        // assume the url is in UTF8
        title = NS_ConvertUTF8toUTF16(filename);
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
      // Avoid trying to create a resource from an empty string, which
      // will raise an exception
      if (str.IsEmpty()) return NS_RDF_NO_VALUE;

      nsCOMPtr<nsIRDFResource> resource;
      rv = gRDFService->GetResource(str, getter_AddRefs(resource));
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
  mSyncTimer = nsnull;
}

void
nsGlobalHistory::ExpireNow()
{
  mNowValid = PR_FALSE;
  mExpireNowTimer = nsnull;
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

  if (!mSyncTimer) {
    mSyncTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    if (NS_FAILED(rv)) return rv;
  }
  
  mDirty = PR_TRUE;
  mSyncTimer->InitWithFuncCallback(fireSyncTimer, this, HISTORY_SYNC_TIMEOUT,
                                   nsITimer::TYPE_ONE_SHOT);
  

  return NS_OK;
}

// hack to avoid calling PR_Now() too often, as is the case when
// we're asked the ageindays of many history entries in a row
PRTime
nsGlobalHistory::GetNow()
{
  if (!mNowValid) {             // not dirty, mLastNow is crufty
    mLastNow = PR_Now();
    mNowValid = PR_TRUE;
    if (!mExpireNowTimer)
      mExpireNowTimer = do_CreateInstance("@mozilla.org/timer;1");

    if (mExpireNowTimer)
      mExpireNowTimer->InitWithFuncCallback(expireNowTimer, this, HISTORY_EXPIRE_NOW_TIMEOUT,
                                            nsITimer::TYPE_ONE_SHOT);
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
    URLEnumerator* result = new URLEnumerator(kToken_URLColumn,
                                              kToken_HiddenColumn);
    if (! result)
      return NS_ERROR_OUT_OF_MEMORY;
    
    nsresult rv;
    rv = result->Init(mEnv, mTable);
    if (NS_FAILED(rv)) return rv;
    
    *aTargets = result;
    NS_ADDREF(*aTargets);
    return NS_OK;
  }
  else if ((aSource == kNC_HistoryByDateAndSite) &&
           (aProperty == kNC_child)) {

    return GetRootDayQueries(aTargets, PR_TRUE);
  }
  else if ((aSource == kNC_HistoryByDate) &&
           (aProperty == kNC_child)) {

    return GetRootDayQueries(aTargets, PR_FALSE);
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
           (aProperty == kNC_Referrer) ||
           (aProperty == kNC_DayFolderIndex)) {
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
  if ((aSource == kNC_HistoryRoot || aSource == kNC_HistoryByDateAndSite || aSource == kNC_HistoryByDate 
       || IsFindResource(aSource)) &&
      aProperty == kNC_child) {

    nsCOMPtr<nsIRDFResource> resource = do_QueryInterface(aTarget, &rv);
    if (NS_FAILED(rv)) return NS_RDF_ASSERTION_REJECTED; 

    const char* targetUrl;
    rv = resource->GetValueConst(&targetUrl);
    if (NS_FAILED(rv)) return NS_RDF_ASSERTION_REJECTED;

    if (IsFindResource(resource)) {
      // convert uri to a query
      searchQuery query;
      rv = FindUrlToSearchQuery(targetUrl, query);
      if (NS_FAILED(rv)) return NS_RDF_ASSERTION_REJECTED;
 
      matchQuery_t matchQuery;
      matchQuery.history = this;
      matchQuery.query = &query;
      rv = RemoveMatchingRows(matchQueryCallback, (void*)&matchQuery, PR_TRUE); 
      FreeSearchQuery(query);
      if (NS_FAILED(rv)) return NS_RDF_ASSERTION_REJECTED;

      // if there are batches in progress, we don't want to notify
      // observers that we're deleting items. the caller promises
      // to handle whatever UI updating is necessary when we're finished.
      if (!mBatchesInProgress)
        NotifyUnassert(aSource, aProperty, aTarget);

      return NS_OK;
    }

    // ignore any error
    rv = RemovePageInternal(targetUrl);
    if (NS_FAILED(rv)) return NS_RDF_ASSERTION_REJECTED;

    if (!mBatchesInProgress && IsFindResource(aSource)) {
      // if there are batches in progress, we don't want to notify
      // observers that we're deleting items. the caller promises
      // to handle whatever UI updating is necessary when we're finished.
      NotifyUnassert(aSource, aProperty, aTarget);
    }

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
    
    nsCOMPtr<nsIMdbRow> row;
    rv = FindRow(kToken_URLColumn, uri, getter_AddRefs(row));
    // not even in history. don't bother trying
    if (NS_FAILED(rv) || HasCell(mEnv, row, kToken_HiddenColumn)) {
      *aHasAssertion = PR_FALSE;
      return NS_OK;
    }
    
    *aHasAssertion = RowMatches(row, &query, PR_TRUE);
    FreeSearchQuery(query);
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

  mObservers.AppendObject(aObserver);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalHistory::RemoveObserver(nsIRDFObserver* aObserver)
{
  NS_PRECONDITION(aObserver != nsnull, "null ptr");
  if (! aObserver)
    return NS_ERROR_NULL_POINTER;

  mObservers.RemoveObject(aObserver);

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
      (aSource == kNC_HistoryByDateAndSite) ||
      (aSource == kNC_HistoryByDate)) {
    *result = (aArc == kNC_child);
  }
  else if (IsFindResource(aSource)) {
    // we handle children of find urls
    *result = (aArc == kNC_child ||
               aArc == kNC_Name ||
               aArc == kNC_NameSort ||
               aArc == kNC_DayFolderIndex);
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
      (aSource == kNC_HistoryByDateAndSite) ||
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
    array->AppendElement(kNC_DayFolderIndex);
    
    return NS_NewArrayEnumerator(aLabels, array);
  }
  else {
    return NS_NewEmptyEnumerator(aLabels);
  }
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
  URLEnumerator* result = new URLEnumerator(kToken_URLColumn,
                                            kToken_HiddenColumn);
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  nsresult rv;
  rv = result->Init(mEnv, mTable);
  if (NS_FAILED(rv)) return rv;

  *aResult = result;
  NS_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsGlobalHistory::BeginUpdateBatch()
{
  nsresult rv = NS_OK;

  ++mBatchesInProgress;
  
  PRUint32 i = mObservers.Count();
  while (i > 0) {
    rv = mObservers[--i]->OnBeginUpdateBatch(this);
  }
  return rv;
}

NS_IMETHODIMP
nsGlobalHistory::EndUpdateBatch()
{
  nsresult rv = NS_OK;

  --mBatchesInProgress;

  PRUint32 i = mObservers.Count();
  while (i > 0) {
    rv = mObservers[--i]->OnEndUpdateBatch(this);
  }
  return rv;
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

NS_IMETHODIMP
nsGlobalHistory::FlushTo(const char* aURI)
{
  // Do not ever implement this (security)
  return NS_ERROR_NOT_IMPLEMENTED;
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
  // we can't get the pref service. register a pref observer so we update
  // if the pref changes

  if (!gPrefBranch) {
    nsCOMPtr<nsIPrefService> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = prefs->GetBranch(PREF_BRANCH_BASE, &gPrefBranch);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  gPrefBranch->GetIntPref(PREF_BROWSER_HISTORY_EXPIRE_DAYS, &mExpireDays);
  gPrefBranch->GetBoolPref(PREF_AUTOCOMPLETE_ONLY_TYPED, &mAutocompleteOnlyTyped);
  nsCOMPtr<nsIPrefBranch2> pbi = do_QueryInterface(gPrefBranch);
  if (pbi) {
    pbi->AddObserver(PREF_AUTOCOMPLETE_ONLY_TYPED, this, PR_FALSE);
    pbi->AddObserver(PREF_BROWSER_HISTORY_EXPIRE_DAYS, this, PR_FALSE);
  }

  if (gRefCnt++ == 0) {
    rv = CallGetService(kRDFServiceCID, &gRDFService);

    NS_ASSERTION(NS_SUCCEEDED(rv), "unable to get RDF service");
    if (NS_FAILED(rv)) return rv;

    gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Page"),        &kNC_Page);
    gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Date"),        &kNC_Date);
    gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "FirstVisitDate"),
                             &kNC_FirstVisitDate);
    gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "VisitCount"),  &kNC_VisitCount);
    gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "AgeInDays"),  &kNC_AgeInDays);
    gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Name"),        &kNC_Name);
    gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Name?sort=true"), &kNC_NameSort);
    gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Hostname"),    &kNC_Hostname);
    gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "Referrer"),    &kNC_Referrer);
    gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "child"),       &kNC_child);
    gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "URL"),         &kNC_URL);
    gRDFService->GetResource(NS_LITERAL_CSTRING(NC_NAMESPACE_URI "DayFolderIndex"), &kNC_DayFolderIndex);
    gRDFService->GetResource(NS_LITERAL_CSTRING("NC:HistoryRoot"),               &kNC_HistoryRoot);
    gRDFService->GetResource(NS_LITERAL_CSTRING("NC:HistoryByDateAndSite"),           &kNC_HistoryByDateAndSite);
    gRDFService->GetResource(NS_LITERAL_CSTRING("NC:HistoryByDate"),           &kNC_HistoryByDate);
  }

  // register this as a named data source with the RDF service
  rv = gRDFService->RegisterDataSource(this, PR_FALSE);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIStringBundleService> bundleService =
    do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv);
  
  if (NS_SUCCEEDED(rv)) {
    rv = bundleService->CreateBundle("chrome://global/locale/history/history.properties", getter_AddRefs(mBundle));
  }

  // register to observe profile changes
  nsCOMPtr<nsIObserverService> observerService = 
           do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ASSERTION(observerService, "failed to get observer service");
  if (observerService) {
    observerService->AddObserver(this, "profile-before-change", PR_TRUE);
    observerService->AddObserver(this, "profile-do-change", PR_TRUE);
    observerService->AddObserver(this, "quit-application", PR_TRUE);
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

  nsCOMPtr<nsIMdbFactoryFactory> factoryfactory =
      do_CreateInstance(NS_MORK_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = factoryfactory->GetMdbFactory(&gMdbFactory);
  NS_ENSURE_SUCCESS(rv, rv);

  mdb_err err;

  err = gMdbFactory->MakeEnv(nsnull, &mEnv);
  mEnv->SetAutoClear(PR_TRUE);
  NS_ASSERTION((err == 0), "unable to create mdb env");
  if (err != 0) return NS_ERROR_FAILURE;

  // MDB requires native file paths

  nsCAutoString filePath;
  rv = historyFile->GetNativePath(filePath);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool exists = PR_TRUE;

  historyFile->Exists(&exists);
    
  if (!exists || NS_FAILED(rv = OpenExistingFile(gMdbFactory, filePath.get()))) {

    // we couldn't open the file, so it's either corrupt or doesn't exist.
    // attempt to delete the file, but ignore the error
    historyFile->Remove(PR_FALSE);
    rv = OpenNewFile(gMdbFactory, filePath.get());
  }

  NS_ENSURE_SUCCESS(rv, rv);

  // get the initial filesize. Used in Commit() to determine type of commit.
  rv = historyFile->GetFileSize(&mFileSizeOnDisk);
  if (NS_FAILED(rv)) {
    LL_I2L(mFileSizeOnDisk, 0);
  }
  
  // See if we need to byte-swap.
  InitByteOrder(PR_FALSE);

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
  nsCOMPtr<nsIMdbFile> oldFile; // ensures file is released
  err = factory->OpenOldFile(mEnv, dbHeap, filePath,
                             dbFrozen, getter_AddRefs(oldFile));

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

  NS_IF_RELEASE(thumb);

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

  err = mTable->GetMetaRow(mEnv, &oid, nsnull, getter_AddRefs(mMetaRow));
  if (err != 0)
    NS_WARNING("Could not get meta row\n");

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
  nsCOMPtr<nsIMdbFile> newFile; // ensures file is released
  err = factory->CreateNewFile(mEnv, dbHeap, filePath,
                               getter_AddRefs(newFile));

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

  // Create the meta row.
  mdbOid oid = { kToken_HistoryRowScope, 1 };
  err = mTable->GetMetaRow(mEnv, &oid, nsnull, getter_AddRefs(mMetaRow));
  if (err != 0)
    NS_WARNING("Could not get meta row\n");

  // Force a commit now to get it written out.
  nsCOMPtr<nsIMdbThumb> thumb;
  err = mStore->LargeCommit(mEnv, getter_AddRefs(thumb));
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

// Set the history file byte order if necessary, and determine if
// we need to byte-swap Unicode values.
// If the force argument is true, the file byte order will be set
// to that of this machine.
nsresult
nsGlobalHistory::InitByteOrder(PRBool aForce)
{
#ifdef IS_LITTLE_ENDIAN
  NS_NAMED_LITERAL_CSTRING(machine_byte_order, "LE");
#endif
#ifdef IS_BIG_ENDIAN
  NS_NAMED_LITERAL_CSTRING(machine_byte_order, "BE");
#endif
  nsXPIDLCString file_byte_order;
  nsresult rv = NS_OK;

  if (!aForce)
    rv = GetByteOrder(getter_Copies(file_byte_order));
  if (aForce || NS_FAILED(rv) ||
      !(file_byte_order.Equals(NS_LITERAL_CSTRING("BE")) ||
        file_byte_order.Equals(NS_LITERAL_CSTRING("LE")))) {
    // Byte order is not yet set, or needs to be reset; initialize it.
    mReverseByteOrder = PR_FALSE;
    rv = SaveByteOrder(machine_byte_order.get());
    if (NS_FAILED(rv))
      return rv;
  }
  else
    mReverseByteOrder = !file_byte_order.Equals(machine_byte_order);

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
    new SearchEnumerator(query, kToken_HiddenColumn, this);
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
  nsresult rv = NS_OK;

  mdb_err err;

  nsCOMPtr<nsIMdbTableRowCursor> cursor;
  nsCOMPtr<nsIMdbRow> row;
 
  err = mTable->GetTableRowCursor(mEnv, -1, getter_AddRefs(cursor));
  if (err != 0) return NS_ERROR_FAILURE;

  int marker;
  err = mTable->StartBatchChangeHint(mEnv, &marker);
  NS_ASSERTION(err == 0, "unable to start batch");
  if (err != 0) return NS_ERROR_FAILURE;
  
  mdb_pos pos;
  err = cursor->NextRow(mEnv, getter_AddRefs(row), &pos);
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
    cursor->NextRow(mEnv, getter_AddRefs(row), &pos);
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
  
  err = mStore->StringToToken(mEnv, "Hidden", &kToken_HiddenColumn);
  if (err != 0) return NS_ERROR_FAILURE;

  err = mStore->StringToToken(mEnv, "Typed", &kToken_TypedColumn);
  if (err != 0) return NS_ERROR_FAILURE;

  err = mStore->StringToToken(mEnv, "GeckoFlags", &kToken_GeckoFlagsColumn);
  if (err != 0) return NS_ERROR_FAILURE;

  // meta-data tokens
  err = mStore->StringToToken(mEnv, "LastPageVisited", &kToken_LastPageVisited);
  err = mStore->StringToToken(mEnv, "ByteOrder", &kToken_ByteOrder);

  return NS_OK;
}

nsresult nsGlobalHistory::Commit(eCommitType commitType)
{
  if (!mStore || !mTable)
    return NS_OK;

  nsresult  err = NS_OK;
  nsCOMPtr<nsIMdbThumb> thumb;

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
    err = mStore->LargeCommit(mEnv, getter_AddRefs(thumb));
    break;
  case kSessionCommit:
    err = mStore->SessionCommit(mEnv, getter_AddRefs(thumb));
    break;
  case kCompressCommit:
    err = mStore->CompressCommit(mEnv, getter_AddRefs(thumb));
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

  // order is important here - logically smallest objects first
  mMetaRow = nsnull;
  
  if (mTable)
    mTable->Release();

  if (mStore)
    mStore->Release();

  if (mEnv)
    mEnv->Release();

  mTable = nsnull; mEnv = nsnull; mStore = nsnull;

  return NS_OK;
}

nsresult
nsGlobalHistory::FindRow(mdb_column aCol,
                         const char *aValue, nsIMdbRow **aResult)
{
  if (! mStore)
    return NS_ERROR_NOT_INITIALIZED;

  mdb_err err;
  PRInt32 len = PL_strlen(aValue);
  mdbYarn yarn = { (void*) aValue, len, len, 0, 0, nsnull };

  mdbOid rowId;
  nsCOMPtr<nsIMdbRow> row;
  if (aResult) {
    err = mStore->FindRow(mEnv, kToken_HistoryRowScope,
                          aCol, &yarn, &rowId, getter_AddRefs(row));

    if (!row) return NS_ERROR_NOT_AVAILABLE;
  } else {
    err = mStore->FindRow(mEnv, kToken_HistoryRowScope,
                          aCol, &yarn, &rowId, nsnull);
  }

  // make sure it's actually stored in the main table
  mdb_bool hasRow;
  mTable->HasOid(mEnv, &rowId, &hasRow);

  if (!hasRow) return NS_ERROR_NOT_AVAILABLE;
  
  if (aResult) {
    *aResult = row;
    (*aResult)->AddRef();
  }

  return NS_OK;
}

PRBool
nsGlobalHistory::IsURLInHistory(nsIRDFResource* aResource)
{
  nsresult rv;

  const char* url;
  rv = aResource->GetValueConst(&url);
  if (NS_FAILED(rv)) return PR_FALSE;

  rv = FindRow(kToken_URLColumn, url, nsnull);
  return (NS_SUCCEEDED(rv)) ? PR_TRUE : PR_FALSE;
}


nsresult
nsGlobalHistory::NotifyAssert(nsIRDFResource* aSource,
                              nsIRDFResource* aProperty,
                              nsIRDFNode* aValue)
{
  PRUint32 i = mObservers.Count();
  while (i > 0) {
    mObservers[--i]->OnAssert(this, aSource, aProperty, aValue);
  }

  return NS_OK;
}


nsresult
nsGlobalHistory::NotifyUnassert(nsIRDFResource* aSource,
                                nsIRDFResource* aProperty,
                                nsIRDFNode* aValue)
{
  PRUint32 i = mObservers.Count();
  while (i > 0) {
    mObservers[--i]->OnUnassert(this, aSource, aProperty, aValue);
  }

  return NS_OK;
}



nsresult
nsGlobalHistory::NotifyChange(nsIRDFResource* aSource,
                              nsIRDFResource* aProperty,
                              nsIRDFNode* aOldValue,
                              nsIRDFNode* aNewValue)
{
  PRUint32 i = mObservers.Count();
  while (i > 0) {
    mObservers[--i]->OnChange(this, aSource, aProperty, aOldValue, aNewValue);
  }

  return NS_OK;
}

//
// this just generates a static list of find-style queries
// only returns queries that currently have matches in global history
// 
nsresult
nsGlobalHistory::GetRootDayQueries(nsISimpleEnumerator **aResult, PRBool aBySite)
{
  nsresult rv;
  nsCOMPtr<nsISupportsArray> dayArray;
  NS_NewISupportsArray(getter_AddRefs(dayArray));
  
  PRInt32 i;
  nsCOMPtr<nsIRDFResource> finduri;
  nsDependentCString
    prefix(FIND_BY_AGEINDAYS_PREFIX "is" "&text=");
  nsCAutoString uri;
  nsCOMPtr<nsISimpleEnumerator> findEnumerator;
  PRBool hasMore = PR_FALSE;
  for (i=0; i<7; i++) {
    uri = prefix;
    uri.AppendInt(i);
    if (aBySite)
      uri.Append("&groupby=Hostname");
    rv = gRDFService->GetResource(uri, getter_AddRefs(finduri));
    if (NS_FAILED(rv)) continue;
    rv = CreateFindEnumerator(finduri, getter_AddRefs(findEnumerator));
    if (NS_FAILED(rv)) continue;
    rv = findEnumerator->HasMoreElements(&hasMore);
    if (NS_SUCCEEDED(rv) && hasMore)
      dayArray->AppendElement(finduri);
  }

  uri = FIND_BY_AGEINDAYS_PREFIX "isgreater" "&text=";
  uri.AppendInt(i-1);
  if (aBySite)
    uri.Append("&groupby=Hostname");
  rv = gRDFService->GetResource(uri, getter_AddRefs(finduri));
  if (NS_SUCCEEDED(rv)) {
    rv = CreateFindEnumerator(finduri, getter_AddRefs(findEnumerator));
    if (NS_SUCCEEDED(rv)) {
      rv = findEnumerator->HasMoreElements(&hasMore);
      if (NS_SUCCEEDED(rv) && hasMore)
        dayArray->AppendElement(finduri);
    }
  }

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
      if (tokenStruct)
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

void nsGlobalHistory::FreeSearchQuery(searchQuery& aQuery)
{
  // free up the token pairs
  PRInt32 i;
  for (i=0; i<aQuery.terms.Count(); i++) {
    searchTerm *term = (searchTerm*)aQuery.terms.ElementAt(i);
    delete term;
  }
  // clean out the array, just for good measure
  aQuery.terms.Clear();
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
    const nsASingleFragmentCString& tokenName =
        Substring(token->tokenName, token->tokenName + token->tokenNameLength);
    if (tokenName.EqualsLiteral("datasource")) {
      datasource = token->tokenValue;
      datasourceLen = token->tokenValueLength;
    }
    else if (tokenName.EqualsLiteral("match")) {
      if (Substring(token->tokenValue, token->tokenValue+token->tokenValueLength).Equals("AgeInDays"))
        matchCallback = matchAgeInDaysCallback;
      
      property = token->tokenValue;
      propertyLen = token->tokenValueLength;
    }
    else if (tokenName.EqualsLiteral("method")) {
      method = token->tokenValue;
      methodLen = token->tokenValueLength;
    }    
    else if (tokenName.EqualsLiteral("text")) {
      text = token->tokenValue;
      textLen = token->tokenValueLength;
    }
    
    // really, we should be storing the group-by as a column number or
    // rdf resource
    else if (tokenName.EqualsLiteral("groupby")) {
      mdb_err err;
      err = mStore->QueryToken(mEnv,
                               nsCAutoString(token->tokenValue).get(),
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
  PRTime lastVisited;
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
  gRDFService->GetResource(findUri, getter_AddRefs(childFindResource));
  NotifyAssert(kNC_HistoryByDateAndSite, kNC_child, childFindResource);
  parentFindResource = childFindResource;
  
  query.terms.Clear();

  query.groupBy = 0;
  query.terms.AppendElement((void *)&ageterm);

  GetFindUriPrefix(query, PR_TRUE, findUri);
  gRDFService->GetResource(findUri, getter_AddRefs(childFindResource));
  NotifyAssert(kNC_HistoryByDate, kNC_child, childFindResource);

  query.terms.Clear();
  

  query.groupBy = 0;
  query.terms.AppendElement((void *)&ageterm);

  GetFindUriPrefix(query, PR_TRUE, findUri);
  gRDFService->GetResource(findUri, getter_AddRefs(childFindResource));
  NotifyAssert(childFindResource, kNC_child, aSource);

  query.terms.Clear();

  // 3) AgeInDays=<age>&groupby=Hostname ->
  //    AgeInDays=<age>&Hostname=<host>
  

  query.groupBy = 0;            // create AgeInDays=<age>&Hostname=<host>
  query.terms.AppendElement((void *)&ageterm);
  query.terms.AppendElement((void *)&hostterm);
  
  GetFindUriPrefix(query, PR_FALSE, findUri);
  gRDFService->GetResource(findUri, getter_AddRefs(childFindResource));
  NotifyAssert(parentFindResource, kNC_child, childFindResource);
  
  query.terms.Clear();

  // 4) AgeInDays=<age>&Hostname=<host> -> uri
  parentFindResource = childFindResource; // AgeInDays=<age>&hostname=<host>
  NotifyAssert(childFindResource, kNC_child, aSource);
  
  // 5) groupby=Hostname -> Hostname=<host>
  query.groupBy = kToken_HostnameColumn; // create groupby=Hostname
  
  GetFindUriPrefix(query, PR_TRUE, findUri);
  gRDFService->GetResource(findUri, getter_AddRefs(parentFindResource));

  query.groupBy = 0;            // create Hostname=<host>
  query.terms.AppendElement((void *)&hostterm);
  GetFindUriPrefix(query, PR_FALSE, findUri);
  findUri.Append(hostname);     // append <host>
  gRDFService->GetResource(findUri, getter_AddRefs(childFindResource));
  
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
  PRTime lastVisited;
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
  
  gRDFService->GetResource(findUri, getter_AddRefs(findResource));
  
  NotifyUnassert(findResource, kNC_child, aSource);

  // 3) Hostname=<host>
  query.terms.Clear();
  
  query.terms.AppendElement((void *)&hostterm);
  GetFindUriPrefix(query, PR_FALSE, findUri);
  
  gRDFService->GetResource(findUri, getter_AddRefs(findResource));
  NotifyUnassert(findResource, kNC_child, aSource);

  query.terms.Clear();

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

  // now build up a string from the query (using only the last term)
  searchTerm *term = (searchTerm*)query.terms[query.terms.Count()-1];

  // automatically build up string in the form
  // findurl-<property>-<method>[-<text>]
  // such as "finduri-AgeInDays-is" or "find-uri-AgeInDays-is-0"
  nsAutoString stringName(NS_LITERAL_STRING("finduri-"));

  // property
  stringName.Append(NS_ConvertASCIItoUTF16(term->property));
  stringName.Append(PRUnichar('-'));

  // and now the method, such as "is" or "isgreater"
  stringName.Append(NS_ConvertASCIItoUTF16(term->method));

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
  FreeSearchQuery(query);
    
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
    NS_ENSURE_STATE(gPrefBranch);

    // expiration date
    if (!nsCRT::strcmp(aSomeData, NS_LITERAL_STRING(PREF_BROWSER_HISTORY_EXPIRE_DAYS).get())) {
      gPrefBranch->GetIntPref(PREF_BROWSER_HISTORY_EXPIRE_DAYS, &mExpireDays);
    }
    else if (!nsCRT::strcmp(aSomeData, NS_LITERAL_STRING(PREF_AUTOCOMPLETE_ONLY_TYPED).get())) {
      gPrefBranch->GetBoolPref(PREF_AUTOCOMPLETE_ONLY_TYPED, &mAutocompleteOnlyTyped);
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
  else if (!nsCRT::strcmp(aTopic, "quit-application"))
    rv = Flush();

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
  if (HasCell(mEnv, aRow, mHiddenColumn))
    return PR_FALSE;
  
  if (mSelectColumn) {
    mdb_err err;

    mdbYarn yarn;
    err = aRow->AliasCellYarn(mEnv, mURLColumn, &yarn);
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
  err = aRow->AliasCellYarn(mEnv, mURLColumn, &yarn);
  if (err != 0) return NS_ERROR_FAILURE;

  // Since the URLEnumerator always returns the value of the URL
  // column, we create an RDF resource.
  nsresult rv;
  nsCOMPtr<nsIRDFResource> resource;
  const char* startPtr = (const char*) yarn.mYarn_Buf;
  rv = gRDFService->GetResource(
            Substring(startPtr, startPtr+yarn.mYarn_Fill),
            getter_AddRefs(resource));
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
  nsGlobalHistory::FreeSearchQuery(*mQuery);
  delete mQuery;
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
                                  nsACString& aResult)
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
    aResult.Append(NS_ConvertUTF16toUTF8(term->text));
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
  if (HasCell(mEnv, aRow, mHiddenColumn))
    return PR_FALSE;
  
  mdb_err err;

  mdbYarn groupColumnValue = { nsnull, 0, 0, 0, 0, nsnull};
  if (mQuery->groupBy!=0) {

    // if we have a 'groupby', then we use the hashtable to make sure
    // we only match the FIRST row with the column value that we're
    // grouping by
    
    err = aRow->AliasCellYarn(mEnv, mQuery->groupBy, &groupColumnValue);
    if (err!=0) return PR_FALSE;
    if (!groupColumnValue.mYarn_Buf) return PR_FALSE;

    const char* startPtr = (const char*)groupColumnValue.mYarn_Buf;
    nsCStringKey key(Substring(startPtr,
                               startPtr +  groupColumnValue.mYarn_Fill));

    void *otherRow = mUniqueRows.Get(&key);

    // Hey! we've seen this row before, so ignore it
    if (otherRow) return PR_FALSE;
  }

  // now do the actual match
  if (!mHistory->RowMatches(aRow, mQuery, PR_FALSE))
    return PR_FALSE;

  if (mQuery->groupBy != 0) {
    // we got this far, so we must have matched.
    // add ourselves to the hashtable so we don't match rows like this
    // in the future
    const char* startPtr = (const char*)groupColumnValue.mYarn_Buf;
    nsCStringKey key(Substring(startPtr,
                               startPtr + groupColumnValue.mYarn_Fill));
    
    // note - weak ref, don't worry about releasing
    mUniqueRows.Put(&key, (void *)aRow);
  }

  return PR_TRUE;
}

//
// determines if the row matches the given terms, used above
//
PRBool
nsGlobalHistory::RowMatches(nsIMdbRow *aRow,
                            searchQuery *aQuery,
                            PRBool caseSensitive)
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
      nsCAutoString property_name(term->property);
      property_name.Append(char(0));
      
      err = mStore->QueryToken(mEnv, property_name.get(), &property_column);
      if (err != 0) {
        NS_WARNING("Unrecognized column!");
        continue;               // assume we match???
      }
      
      // match the term directly against the column?
      mdbYarn yarn;
      err = aRow->AliasCellYarn(mEnv, property_column, &yarn);
      if (err != 0 || !yarn.mYarn_Buf) return PR_FALSE;
      
      nsAutoString rowVal;

      PRInt32 yarnLength = yarn.mYarn_Fill;;
      if (property_column == kToken_NameColumn) {
        // The name column (page title) is stored as UTF-16.
        rowVal.Assign((const PRUnichar*)yarn.mYarn_Buf, yarnLength / 2);
      }
      else {
        // Other columns are stored as UTF-8 and can be null.
        if (yarn.mYarn_Buf)
          rowVal = NS_ConvertUTF8toUTF16((const char*)yarn.mYarn_Buf, yarnLength);
      }
      
      // set up some iterators
      nsString::const_iterator start, end;
      rowVal.BeginReading(start);
      rowVal.EndReading(end);

      const nsXPIDLString& searchText = term->text;
      
      if (term->method.Equals("is")) {
        if (caseSensitive) {
          if (!searchText.Equals(rowVal, nsDefaultStringComparator()))
            return PR_FALSE;
        }
        else {
          if (!searchText.Equals(rowVal, nsCaseInsensitiveStringComparator()))
            return PR_FALSE;
        }
      }

      else if (term->method.Equals("isnot")) {        
        if (caseSensitive) {
          if (searchText.Equals(rowVal, nsDefaultStringComparator()))
            return PR_FALSE;
        }
        else {
          if (searchText.Equals(rowVal, nsCaseInsensitiveStringComparator()))
            return PR_FALSE;
        }
      }

      else if (term->method.Equals("contains")) {
        if (caseSensitive) {
          if (!FindInReadable(searchText, start, end, nsDefaultStringComparator()))
            return PR_FALSE;
        }
        else {
          if (!FindInReadable(searchText, start, end, nsCaseInsensitiveStringComparator()))
            return PR_FALSE;
        }
      }

      else if (term->method.Equals("doesntcontain")) {
        if (caseSensitive) {
          if (FindInReadable(searchText, start, end, nsDefaultStringComparator()))
            return PR_FALSE;
        }
        else {
          if (FindInReadable(searchText, start, end, nsCaseInsensitiveStringComparator()))
            return PR_FALSE;
        }
      }

      else if (term->method.Equals("startswith")) {
        // need to make sure that the found string is 
        // at the beginning of the string
        nsAString::const_iterator real_start = start;
        if (caseSensitive) {
          if (!(FindInReadable(searchText, start, end, nsDefaultStringComparator()) && real_start == start))
            return PR_FALSE;
        }
        else {
          if (!(FindInReadable(searchText, start, end, nsCaseInsensitiveStringComparator()) &&
                real_start == start))
            return PR_FALSE;
        }
      }

      else if (term->method.Equals("endswith")) {
        // need to make sure that the found string ends
        // at the end of the string
        nsAString::const_iterator real_end = end;
        if (caseSensitive) {
          if (!(RFindInReadable(searchText, start, end, nsDefaultStringComparator()) && real_end == end))
            return PR_FALSE;
        }
        else {
          if (!(RFindInReadable(searchText, start, end, nsCaseInsensitiveStringComparator()) &&
                real_end == end))
          return PR_FALSE;
        }
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

    
    const char* startPtr = (const char*)yarn.mYarn_Buf;
    rv = gRDFService->GetResource(
            Substring(startPtr, startPtr+yarn.mYarn_Fill),
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

  const char* startPtr = (const char *)groupByValue.mYarn_Buf;
  findUri.Append(Substring(startPtr, startPtr+groupByValue.mYarn_Fill));
  findUri.Append('\0');

  rv = gRDFService->GetResource(findUri, getter_AddRefs(resource));
  if (NS_FAILED(rv)) return rv;

  *aResult = resource;
  NS_ADDREF(*aResult);
  return NS_OK;
}

//----------------------------------------------------------------------
//
// nsIAutoCompleteSession implementation
//

NS_IMETHODIMP
nsGlobalHistory::StartSearch(const nsAString &aSearchString,
                             const nsAString &aSearchParam,
                             nsIAutoCompleteResult *aPreviousResult,
                             nsIAutoCompleteObserver *aListener)
{
  NS_ENSURE_ARG_POINTER(aListener);
  NS_ENSURE_STATE(gPrefBranch);

  NS_ENSURE_SUCCESS(OpenDB(), NS_ERROR_FAILURE);
  
  nsCOMPtr<nsIAutoCompleteMdbResult2> result;
  if (aSearchString.IsEmpty()) {
    AutoCompleteTypedSearch(getter_AddRefs(result));
  } else {
    // if the search string is empty after it has had prefixes removed, then 
    // we need to ignore the previous result set
    nsAutoString cut(aSearchString);
    AutoCompleteCutPrefix(cut, nsnull);
    if (cut.Length() == 0)
      aPreviousResult = nsnull;
    
    // pass string through filter and then determine which prefixes to exclude
    // when chopping prefixes off of history urls during comparison
    nsString filtered = AutoCompletePrefilter(aSearchString);
    AutocompleteExclude exclude;
    AutoCompleteGetExcludeInfo(filtered, &exclude);
    
    // perform the actual search here
    nsresult rv = AutoCompleteSearch(filtered, &exclude,
                                     NS_STATIC_CAST(nsIAutoCompleteMdbResult2 *,
                                                    aPreviousResult),
                                     getter_AddRefs(result));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  aListener->OnSearchResult(this, result);  
  
  return NS_OK;
}


NS_IMETHODIMP
nsGlobalHistory::StopSearch()
{
  return NS_OK;
}

//----------------------------------------------------------------------
//
// AutoComplete stuff
//

nsresult
nsGlobalHistory::AutoCompleteTypedSearch(nsIAutoCompleteMdbResult2 **aResult)
{
  mdb_count count;
  mdb_err err = mTable->GetCount(mEnv, &count);

  // Get a cursor to iterate through all rows in the database
  nsCOMPtr<nsIMdbTableRowCursor> rowCursor;
   err = mTable->GetTableRowCursor(mEnv, count, getter_AddRefs(rowCursor));
  NS_ENSURE_TRUE(!err, NS_ERROR_FAILURE);

  nsresult rv;
  nsCOMPtr<nsIAutoCompleteMdbResult2> result = do_CreateInstance("@mozilla.org/autocomplete/mdb-result;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  result->Init(mEnv, mTable);
  result->SetTokens(kToken_URLColumn, nsIAutoCompleteMdbResult2::kCharType, kToken_NameColumn, nsIAutoCompleteMdbResult2::kUnicharType);
  result->SetReverseByteOrder(mReverseByteOrder);

  nsCOMPtr<nsIMdbRow> row;
  mdb_pos pos;
  do {
    rowCursor->PrevRow(mEnv, getter_AddRefs(row), &pos);
    if (!row) break;
    
    if (HasCell(mEnv, row, kToken_TypedColumn)) {
      result->AddRow(row);
    }
  } while (row);

  // Determine the result of the search
  PRUint32 matchCount;
  rv = result->GetMatchCount(&matchCount);
  if (matchCount > 0) {
    result->SetSearchResult(nsIAutoCompleteResult::RESULT_SUCCESS);
    result->SetDefaultIndex(0);
  } else {
    result->SetSearchResult(nsIAutoCompleteResult::RESULT_NOMATCH);
    result->SetDefaultIndex(-1);
  }

  *aResult = result;
  NS_ADDREF(*aResult);
  
  return NS_OK;
}

nsresult
nsGlobalHistory::AutoCompleteSearch(const nsAString &aSearchString,
                                    AutocompleteExclude *aExclude,
                                    nsIAutoCompleteMdbResult2 *aPrevResult,
                                    nsIAutoCompleteMdbResult2 **aResult)
{
  // determine if we can skip searching the whole history and only search
  // through the previous search results
  PRBool searchPrevious = PR_FALSE;
  if (aPrevResult) {
    nsAutoString prevURLStr;
    aPrevResult->GetSearchString(prevURLStr);
    // if search string begins with the previous search string, it's a go
    searchPrevious = Substring(aSearchString, 0, prevURLStr.Length()).Equals(prevURLStr);
  }
    
  if (searchPrevious) {
    // Search through the previous result
    PRUint32 matchCount;
    aPrevResult->GetMatchCount(&matchCount);
    for (PRInt32 i = matchCount-1; i >= 0; --i) {
      // Make a copy of the value because AutoCompleteCompare is destructive
      nsAutoString url;
      aPrevResult->GetValueAt(i, url);
      
      if (!AutoCompleteCompare(url, aSearchString, aExclude))
        aPrevResult->RemoveValueAt(i, PR_FALSE);
    }
    
    NS_ADDREF(*aResult = aPrevResult);
  } else {
    // Search through the entire history
        
    // Create and initialize a new result object
    nsresult rv = NS_OK;
    nsCOMPtr<nsIAutoCompleteMdbResult2> result = do_CreateInstance("@mozilla.org/autocomplete/mdb-result;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
    result->Init(mEnv, mTable);
    result->SetTokens(kToken_URLColumn, nsIAutoCompleteMdbResult2::kCharType, kToken_NameColumn, nsIAutoCompleteMdbResult2::kUnicharType);
    result->SetReverseByteOrder(mReverseByteOrder);
    result->SetSearchString(aSearchString);

    // Get a cursor to iterate through all rows in the database
    nsCOMPtr<nsIMdbTableRowCursor> rowCursor;
    mdb_err err = mTable->GetTableRowCursor(mEnv, -1, getter_AddRefs(rowCursor));
    NS_ENSURE_TRUE(!err, NS_ERROR_FAILURE);

    // Store hits in an nsIArray initially
    nsCOMArray<nsIMdbRow> resultArray;

    nsCOMPtr<nsIMdbRow> row;
    mdb_pos pos;
    do {
      rowCursor->NextRow(mEnv, getter_AddRefs(row), &pos);
      if (!row) break;
      
      if (!HasCell(mEnv, row, kToken_TypedColumn))
        if (mAutocompleteOnlyTyped || HasCell(mEnv, row, kToken_HiddenColumn))
          continue;

      nsCAutoString url;
      GetRowValue(row, kToken_URLColumn, url);

      NS_ConvertUTF8toUTF16 utf8Url(url);
      if (AutoCompleteCompare(utf8Url, aSearchString, aExclude))
        resultArray.AppendObject(row);
    } while (row);
    
    // Setup the structure we pass into the sort function,
    // including a set of url prefixes to ignore.   These prefixes 
    // must match with the logic in nsGlobalHistory::nsGlobalHistory().
    NS_NAMED_LITERAL_STRING(prefixHWStr, "http://www.");
    NS_NAMED_LITERAL_STRING(prefixHStr, "http://");
    NS_NAMED_LITERAL_STRING(prefixHSWStr, "https://www.");
    NS_NAMED_LITERAL_STRING(prefixHSStr, "https://");
    NS_NAMED_LITERAL_STRING(prefixFFStr, "ftp://ftp.");
    NS_NAMED_LITERAL_STRING(prefixFStr, "ftp://");

    // note: the number of prefixes stored in the closure below 
    // must match with the constant AUTOCOMPLETE_PREFIX_LIST_COUNT
    AutoCompleteSortClosure closure;
    closure.history = this;
    closure.prefixCount = AUTOCOMPLETE_PREFIX_LIST_COUNT;
    closure.prefixes[0] = &prefixHWStr;
    closure.prefixes[1] = &prefixHStr;
    closure.prefixes[2] = &prefixHSWStr;
    closure.prefixes[3] = &prefixHSStr;
    closure.prefixes[4] = &prefixFFStr;
    closure.prefixes[5] = &prefixFStr;

    // sort it
    resultArray.Sort(AutoCompleteSortComparison, NS_STATIC_CAST(void*, &closure));

    // place the sorted array into the autocomplete results
    PRUint32 count = resultArray.Count();
    PRUint32 i;
    for (i = 0; i < count; ++i) {
      result->AddRow(resultArray[i]);
    }

    // Determine the result of the search
    PRUint32 matchCount;
    rv = result->GetMatchCount(&matchCount);
    if (matchCount > 0) {
      result->SetSearchResult(nsIAutoCompleteResult::RESULT_SUCCESS);
      result->SetDefaultIndex(0);
    } else {
      result->SetSearchResult(nsIAutoCompleteResult::RESULT_NOMATCH);
      result->SetDefaultIndex(-1);
    }
    
    *aResult = result;
    NS_ADDREF(*aResult);
  }
    
  return NS_OK;
}

// If aURL begins with a protocol or domain prefix from our lists,
// then mark their index in an AutocompleteExclude struct.
void
nsGlobalHistory::AutoCompleteGetExcludeInfo(const nsAString& aURL, AutocompleteExclude* aExclude)
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
nsGlobalHistory::AutoCompleteCutPrefix(nsAString& aURL, AutocompleteExclude* aExclude)
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

nsString
nsGlobalHistory::AutoCompletePrefilter(const nsAString& aSearchString)
{
  nsAutoString url(aSearchString);

  PRInt32 slash = url.FindChar('/', 0);
  if (slash >= 0) {
    // if user is typing a url but has already typed past the host,
    // then convert the host to lowercase
    nsAutoString host;
    url.Left(host, slash);
    ToLowerCase(host);
    url.Assign(host + Substring(url, slash, url.Length()-slash));
  } else {
    // otherwise, assume the user could still be typing the host, and
    // convert everything to lowercase
    ToLowerCase(url);
  }
  
  return nsString(url);
}

PRBool
nsGlobalHistory::AutoCompleteCompare(nsAString& aHistoryURL, 
                                     const nsAString& aUserURL, 
                                     AutocompleteExclude* aExclude)
{
  AutoCompleteCutPrefix(aHistoryURL, aExclude);
  
  return Substring(aHistoryURL, 0, aUserURL.Length()).Equals(aUserURL);
}

int PR_CALLBACK 
nsGlobalHistory::AutoCompleteSortComparison(nsIMdbRow *row1, nsIMdbRow *row2, 
                                            void *closureVoid) 
{
  //
  // NOTE: The design and reasoning behind the following autocomplete 
  // sort implementation is documented in bug 78270.
  //
  // cast our function parameters back into their real form
  AutoCompleteSortClosure* closure = 
      NS_STATIC_CAST(AutoCompleteSortClosure*, closureVoid);

  // get visit counts - we're ignoring all errors from GetRowValue(), 
  // and relying on default values
  PRInt32 item1visits = 0, item2visits = 0;
  closure->history->GetRowValue(row1, 
                                closure->history->kToken_VisitCountColumn, 
                                &item1visits);
  closure->history->GetRowValue(row2, 
                                closure->history->kToken_VisitCountColumn, 
                                &item2visits);

  // get URLs
  nsAutoString url1, url2;
  closure->history->GetRowValue(row1, closure->history->kToken_URLColumn, url1);
  closure->history->GetRowValue(row2, closure->history->kToken_URLColumn, url2);

  // Favour websites and webpaths more than webpages by boosting 
  // their visit counts.  This assumes that URLs have been normalized, 
  // appending a trailing '/'.
  // 
  // We use addition to boost the visit count rather than multiplication 
  // since we want URLs with large visit counts to remain pretty much 
  // in raw visit count order - we assume the user has visited these urls
  // often for a reason and there shouldn't be a problem with putting them 
  // high in the autocomplete list regardless of whether they are sites/
  // paths or pages.  However for URLs visited only a few times, sites 
  // & paths should be presented before pages since they are generally 
  // more likely to be visited again.
  //
  PRBool isPath1 = PR_FALSE, isPath2 = PR_FALSE;
  if (!url1.IsEmpty())
  {
    // url is a site/path if it has a trailing slash
    isPath1 = (url1.Last() == PRUnichar('/'));
    if (isPath1)
      item1visits += AUTOCOMPLETE_NONPAGE_VISIT_COUNT_BOOST;
  }
  if (!url2.IsEmpty())
  {
    // url is a site/path if it has a trailing slash
    isPath2 = (url2.Last() == PRUnichar('/'));
    if (isPath2)
      item2visits += AUTOCOMPLETE_NONPAGE_VISIT_COUNT_BOOST;
  }

  if (HasCell(closure->history->mEnv, row1, closure->history->kToken_TypedColumn))
    item1visits += AUTOCOMPLETE_NONPAGE_VISIT_COUNT_BOOST;
  if (HasCell(closure->history->mEnv, row2, closure->history->kToken_TypedColumn))
    item2visits += AUTOCOMPLETE_NONPAGE_VISIT_COUNT_BOOST;
  
  // primary sort by visit count
  if (item1visits != item2visits)
  {
    // return visit count comparison
    return item2visits - item1visits;
  }
  else
  {
    // Favour websites and webpaths more than webpages
    if (isPath1 && !isPath2) return -1; // url1 is a website/path, url2 isn't
    if (!isPath1 && isPath2) return  1; // url1 isn't a website/path, url2 is

    // We have two websites/paths.. ignore "http[s]://[www.]" & "ftp://[ftp.]"
    // prefixes.  Find a starting position in the string, just past any of the 
    // above prefixes.  Only check for the prefix once, in the far left of the 
    // string - it is assumed there is no whitespace.
    PRInt32 postPrefix1 = 0, postPrefix2 = 0;

    size_t i;
    // iterate through our prefixes looking for a match
    for (i=0; i<closure->prefixCount; i++)
    {
      // Check if string is prefixed.  Note: the parameters of the Find() 
      // method specify the url is searched at the 0th character and if there
      // is no match the rest of the url is not searched.
      if (url1.Find((*closure->prefixes[i]), 0, 1) == 0)
      {
        // found a match - record post prefix position
        postPrefix1 = closure->prefixes[i]->Length();
        // bail out of the for loop
        break;
      }
    }

    // iterate through our prefixes looking for a match
    for (i=0; i<closure->prefixCount; i++)
    {
      // Check if string is prefixed.  Note: the parameters of the Find() 
      // method specify the url is searched at the 0th character and if there
      // is no match the rest of the url is not searched.
      if (url2.Find((*closure->prefixes[i]), 0, 1) == 0)
      {
        // found a match - record post prefix position
        postPrefix2 = closure->prefixes[i]->Length();
        // bail out of the for loop
        break;
      }
    }

    // compare non-prefixed urls
    PRInt32 ret = Compare(
      Substring(url1, postPrefix1, url1.Length()),
      Substring(url2, postPrefix2, url2.Length()));
    if (ret != 0) return ret;

    // sort http://xyz.com before http://www.xyz.com
    return postPrefix1 - postPrefix2;
  }
  return 0;
}

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsGlobalHistory, Init)

static const nsModuleComponentInfo components[] =
{
  { "Global History",
    NS_GLOBALHISTORY_CID,
    NS_GLOBALHISTORY2_CONTRACTID,
    nsGlobalHistoryConstructor },
    
  { "Global History",
    NS_GLOBALHISTORY_CID,
    NS_GLOBALHISTORY_DATASOURCE_CONTRACTID,
    nsGlobalHistoryConstructor },
    
  { "Global History",
    NS_GLOBALHISTORY_CID,
    NS_GLOBALHISTORY_AUTOCOMPLETE_CONTRACTID,
    nsGlobalHistoryConstructor }
};

NS_IMPL_NSGETMODULE(nsToolkitHistory, components)
