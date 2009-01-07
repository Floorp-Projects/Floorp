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
 * The Original Code is Places code.
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com>
 *   Joe Hewitt <hewitt@netscape.com>
 *   Blake Ross <blaker@netscape.com>
 *   Seth Spitzer <sspitzer@mozilla.org>
 *   Dietrich Ayala <dietrich@mozilla.com>
 *   Edward Lee <edward.lee@engineering.uiuc.edu>
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com>
 *   Marco Bonardo <mak77@bonardo.net>
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


/**
 * Autocomplete algorithm:
 *
 * Searches moz_places by frecency (in descending order)
 * in chunks (mAutoCompleteSearchChunkSize).  We currently
 * do SQL LIKE searches of the search term in the place title, place url
 * and bookmark titles (since a "place" can have multiple bookmarks)
 * within in each chunk. The results are ordered by frecency.
 * Note, we exclude places with no frecency (0) because 
 * frecency = 0 means "don't show this in autocomplete". place: queries should
 * have that, as should unvisited children of livemark feeds (that aren't
 * bookmarked elsewhere).
 *
 * But places with frecency < 0 are included, as that means that these items
 * have not had their frecency calculated yet (will happen on idle).
 */

#include "nsNavHistory.h"
#include "nsNetUtil.h"
#include "nsEscape.h"
#include "nsThreadUtils.h"

#include "mozIStorageService.h"
#include "mozIStorageConnection.h"
#include "mozIStorageValueArray.h"
#include "mozIStorageStatement.h"
#include "mozIStorageFunction.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"
#include "nsFaviconService.h"
#include "nsUnicharUtils.h"
#include "nsNavBookmarks.h"
#include "nsPrintfCString.h"
#include "nsILivemarkService.h"
#include "mozIStoragePendingStatement.h"
#include "mozIStorageStatementCallback.h"
#include "mozIStorageError.h"

#define NS_AUTOCOMPLETESIMPLERESULT_CONTRACTID \
  "@mozilla.org/autocomplete/simple-result;1"

// Helpers to get and set fields in the mAutoCompleteCurrentBehavior bitmap
#define GET_BEHAVIOR(aBitName) \
  (mAutoCompleteCurrentBehavior & kAutoCompleteBehavior##aBitName)
#define SET_BEHAVIOR(aBitName) \
  mAutoCompleteCurrentBehavior |= kAutoCompleteBehavior##aBitName

// Helper to get a particular column with a desired name from the bookmark and
// tags table based on if we want to include tags or not
#define SQL_STR_FRAGMENT_GET_BOOK_TAG(name, column, comparison, getMostRecent) \
  NS_LITERAL_CSTRING(", (" \
  "SELECT " column " " \
  "FROM moz_bookmarks b " \
  "JOIN moz_bookmarks t ON t.id = b.parent AND t.parent " comparison " ?1 " \
  "WHERE b.type = ") + nsPrintfCString("%d", \
    nsINavBookmarksService::TYPE_BOOKMARK) + \
    NS_LITERAL_CSTRING(" AND b.fk = h.id") + \
  (getMostRecent ? NS_LITERAL_CSTRING(" " \
    "ORDER BY b.lastModified DESC LIMIT 1") : EmptyCString()) + \
  NS_LITERAL_CSTRING(") AS " name)

// Get three named columns from the bookmarks and tags table
#define BOOK_TAG_SQL (\
  SQL_STR_FRAGMENT_GET_BOOK_TAG("parent", "b.parent", "!=", PR_TRUE) + \
  SQL_STR_FRAGMENT_GET_BOOK_TAG("bookmark", "b.title", "!=", PR_TRUE) + \
  SQL_STR_FRAGMENT_GET_BOOK_TAG("tags", "GROUP_CONCAT(t.title, ',')", "=", PR_FALSE))

// This separator is used as an RTL-friendly way to split the title and tags.
// It can also be used by an nsIAutoCompleteResult consumer to re-split the
// "comment" back into the title and tag.
// Use a Unichar array to avoid problems with 2-byte char strings: " \u2013 "
const PRUnichar kTitleTagsSeparatorChars[] = { ' ', 0x2013, ' ', 0 };
#define TITLE_TAGS_SEPARATOR nsAutoString(kTitleTagsSeparatorChars)

// This fragment is used to get best favicon for a rev_host
#define BEST_FAVICON_FOR_REVHOST( __table_name ) \
  "(SELECT f.url FROM " __table_name " " \
   "JOIN moz_favicons f ON f.id = favicon_id " \
   "WHERE rev_host = IFNULL( " \
     "(SELECT rev_host FROM moz_places_temp WHERE id = b.fk), " \
     "(SELECT rev_host FROM moz_places WHERE id = b.fk)) " \
   "ORDER BY frecency DESC LIMIT 1) "

#define PLACES_AUTOCOMPLETE_FEEDBACK_UPDATED_TOPIC "places-autocomplete-feedback-updated"

// Used to notify a topic to system observers on async execute completion.
// Will throw on error.
class AutoCompleteStatementCallbackNotifier : public mozIStorageStatementCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_MOZISTORAGESTATEMENTCALLBACK
};

// AutocompleteStatementCallbackNotifier
NS_IMPL_ISUPPORTS1(AutoCompleteStatementCallbackNotifier,
                   mozIStorageStatementCallback)

NS_IMETHODIMP
AutoCompleteStatementCallbackNotifier::HandleCompletion(PRUint16 aReason)
{
  if (aReason != mozIStorageStatementCallback::REASON_FINISHED)
    return NS_ERROR_UNEXPECTED;

  nsresult rv;
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService("@mozilla.org/observer-service;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = observerService->NotifyObservers(nsnull,
                                        PLACES_AUTOCOMPLETE_FEEDBACK_UPDATED_TOPIC,
                                        nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
AutoCompleteStatementCallbackNotifier::HandleError(mozIStorageError *aError)
{
  PRInt32 result;
  nsresult rv = aError->GetResult(&result);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCAutoString message;
  rv = aError->GetMessage(message);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString warnMsg;
  warnMsg.Append("An error occured while executing an async statement: ");
  warnMsg.Append(result);
  warnMsg.Append(" ");
  warnMsg.Append(message);
  NS_WARNING(warnMsg.get());

  return NS_OK;
}

NS_IMETHODIMP
AutoCompleteStatementCallbackNotifier::HandleResult(mozIStorageResultSet *aResultSet)
{
  NS_ASSERTION(PR_FALSE, "You cannot use AutoCompleteStatementCallbackNotifier to get async statements resultset");
  return NS_OK;
}

void GetAutoCompleteBaseQuery(nsACString& aQuery) {
// Define common pieces of various queries
// XXX bug 412736
// in the case of a frecency tie, break it with h.typed and h.visit_count
// which is better than nothing.  but this is slow, so not doing it yet.

// Try to reduce size of compound table since with partitioning this became
// slower. Limiting moz_places with OFFSET+LIMIT will mostly help speed
// of first chunks, that are usually most wanted.
// Can do this only if there aren't additional conditions on final resultset.

// Note: h.frecency is selected because we need it for ordering, but will
// not be read later and we don't have an associated kAutoCompleteIndex_
  aQuery = NS_LITERAL_CSTRING(
      "SELECT h.url, h.title, f.url") + BOOK_TAG_SQL + NS_LITERAL_CSTRING(", "
        "h.visit_count, h.typed, h.frecency "
      "FROM moz_places_temp h "
      "LEFT OUTER JOIN moz_favicons f ON f.id = h.favicon_id "
      "WHERE h.frecency <> 0 "
      "{ADDITIONAL_CONDITIONS} "
      "UNION ALL "
      "SELECT * FROM ( "
        "SELECT h.url, h.title, f.url") + BOOK_TAG_SQL + NS_LITERAL_CSTRING(", "
          "h.visit_count, h.typed, h.frecency "
        "FROM moz_places h "
        "LEFT OUTER JOIN moz_favicons f ON f.id = h.favicon_id "
        "WHERE h.id NOT IN (SELECT id FROM moz_places_temp) "
        "AND h.frecency <> 0 "
        "{ADDITIONAL_CONDITIONS} "
        "ORDER BY h.frecency DESC LIMIT (?2 + ?3) "
      ") "
      // ORDER BY h.frecency
      "ORDER BY 9 DESC LIMIT ?2 OFFSET ?3");
}

////////////////////////////////////////////////////////////////////////////////
//// nsNavHistoryAutoComplete Helper Functions

/**
 * Returns true if the string starts with javascript:
 */
inline PRBool
StartsWithJS(const nsAString &aString)
{
  return StringBeginsWith(aString, NS_LITERAL_STRING("javascript:"));
}

/**
 * Callback function for putting URLs from a nsDataHashtable<nsStringHashKey,
 * PRBool> into a nsStringArray.
 *
 * @param aKey
 *        The hashtable entry's key (the url)
 * @param aData
 *        Unused data
 * @param aArg
 *        The nsStringArray pointer for collecting URLs
 */
PLDHashOperator
HashedURLsToArray(const nsAString &aKey, PRBool aData, void *aArg)
{
  // Append the current url to the array of urls
  static_cast<nsStringArray *>(aArg)->AppendString(aKey);
  return PL_DHASH_NEXT;
}

/**
 * Returns true if the unicode character is a word boundary. I.e., anything
 * that *isn't* used to build up a word from a string of characters. We are
 * conservative here because anything that we don't list will be treated as
 * word boundary. This means searching for that not-actually-a-word-boundary
 * character can still be matched in the middle of a word.
 */
inline PRBool
IsWordBoundary(const PRUnichar &aChar)
{
  // Lower-case alphabetic, so upper-case matches CamelCase. Because
  // upper-case is treated as a word boundary, matches will also happen
  // _after_ an upper-case character.
  return !(PRUnichar('a') <= aChar && aChar <= PRUnichar('z'));
}

/**
 * Returns true if the token matches the target on a word boundary
 *
 * @param aToken
 *        Token to search for that must match on a word boundary
 * @param aTarget
 *        Target string to search against
 */
PRBool
FindOnBoundary(const nsAString &aToken, const nsAString &aTarget)
{
  // Define a const instance of this class so it is created once
  const nsCaseInsensitiveStringComparator caseInsensitiveCompare;

  // Can't match anything if there's nothing to match
  if (aTarget.IsEmpty())
    return PR_FALSE;

  nsAString::const_iterator tokenStart, tokenEnd;
  aToken.BeginReading(tokenStart);
  aToken.EndReading(tokenEnd);

  nsAString::const_iterator targetStart, targetEnd;
  aTarget.BeginReading(targetStart);
  aTarget.EndReading(targetEnd);

  // Go straight into checking the token at the beginning of the target because
  // the beginning is considered a word boundary
  do {
    // We're on a word boundary, so prepare to match by copying the iterators
    nsAString::const_iterator testToken(tokenStart);
    nsAString::const_iterator testTarget(targetStart);

    // Keep trying to match the token one by one until it doesn't match
    while (!caseInsensitiveCompare(*testToken, *testTarget)) {
      // We matched something, so move down one
      testToken++;
      testTarget++;

      // Matched the token! We're done!
      if (testToken == tokenEnd)
        return PR_TRUE;

      // If we ran into the end while matching the token, we won't find it
      if (testTarget == targetEnd)
        return PR_FALSE;
    }

    // Unconditionally move past the current position in the target, but if
    // we're not currently on a word boundary, eat up as many non-word boundary
    // characters as possible -- don't kill characters if we're currently on a
    // word boundary so that we can match tokens that start on a word boundary.
    if (!IsWordBoundary(ToLowerCase(*targetStart++)))
      while (targetStart != targetEnd && !IsWordBoundary(*targetStart))
        targetStart++;

    // If we hit the end eating up non-boundaries then boundaries, we're done
  } while (targetStart != targetEnd);

  return PR_FALSE;
}

/**
 * A local wrapper to CaseInsensitiveFindInReadable that isn't overloaded
 */
inline PRBool
FindAnywhere(const nsAString &aToken, const nsAString &aTarget)
{
  return CaseInsensitiveFindInReadable(aToken, aTarget);
}

/**
 * A local wrapper to case insensitive StringBeginsWith
 */
inline PRBool
FindBeginning(const nsAString &aToken, const nsAString &aTarget)
{
  return StringBeginsWith(aTarget, aToken, nsCaseInsensitiveStringComparator());
}

// nsNavHistory::InitAutoComplete
nsresult
nsNavHistory::InitAutoComplete()
{
  nsresult rv = CreateAutoCompleteQueries();
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mCurrentResultURLs.Init(128))
    return NS_ERROR_OUT_OF_MEMORY;

  if (!mLivemarkFeedItemIds.Init(128))
    return NS_ERROR_OUT_OF_MEMORY;

  if (!mLivemarkFeedURIs.Init(128))
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

// nsNavHistory::GetDBAutoCompleteHistoryQuery()
//
//    Returns the auto complete statement used when autocomplete results are
//    restricted to history entries.
mozIStorageStatement*
nsNavHistory::GetDBAutoCompleteHistoryQuery()
{
  if (mDBAutoCompleteHistoryQuery)
    return mDBAutoCompleteHistoryQuery;

  nsCString AutoCompleteHistoryQuery;
  GetAutoCompleteBaseQuery(AutoCompleteHistoryQuery);
  AutoCompleteHistoryQuery.ReplaceSubstring("{ADDITIONAL_CONDITIONS}",
                                            "AND h.visit_count > 0");
  nsresult rv = mDBConn->CreateStatement(AutoCompleteHistoryQuery,
    getter_AddRefs(mDBAutoCompleteHistoryQuery));
  NS_ENSURE_SUCCESS(rv, nsnull);

  return mDBAutoCompleteHistoryQuery;
}

// nsNavHistory::GetDBAutoCompleteStarQuery()
//
//    Returns the auto complete statement used when autocomplete results are
//    restricted to bookmarked entries.
mozIStorageStatement*
nsNavHistory::GetDBAutoCompleteStarQuery()
{
  if (mDBAutoCompleteStarQuery)
    return mDBAutoCompleteStarQuery;

  nsCString AutoCompleteStarQuery;
  GetAutoCompleteBaseQuery(AutoCompleteStarQuery);
  AutoCompleteStarQuery.ReplaceSubstring("{ADDITIONAL_CONDITIONS}",
                                         "AND bookmark IS NOT NULL");
  nsresult rv = mDBConn->CreateStatement(AutoCompleteStarQuery,
    getter_AddRefs(mDBAutoCompleteStarQuery));
  NS_ENSURE_SUCCESS(rv, nsnull);

  return mDBAutoCompleteStarQuery;
}

// nsNavHistory::GetDBAutoCompleteTagsQuery()
//
//    Returns the auto complete statement used when autocomplete results are
//    restricted to tagged entries.
mozIStorageStatement*
nsNavHistory::GetDBAutoCompleteTagsQuery()
{
  if (mDBAutoCompleteTagsQuery)
    return mDBAutoCompleteTagsQuery;

  nsCString AutoCompleteTagsQuery;
  GetAutoCompleteBaseQuery(AutoCompleteTagsQuery);
  AutoCompleteTagsQuery.ReplaceSubstring("{ADDITIONAL_CONDITIONS}",
                                         "AND tags IS NOT NULL");
  nsresult rv = mDBConn->CreateStatement(AutoCompleteTagsQuery,
    getter_AddRefs(mDBAutoCompleteTagsQuery));
  NS_ENSURE_SUCCESS(rv, nsnull);

  return mDBAutoCompleteTagsQuery;
}

// nsNavHistory::GetDBFeedbackIncrease()
//
//    Returns the statement to update the input history that keeps track of
//    selections in the locationbar.  Input history is used for adaptive query.
mozIStorageStatement*
nsNavHistory::GetDBFeedbackIncrease()
{
  if (mDBFeedbackIncrease)
    return mDBFeedbackIncrease;

  nsresult rv = mDBConn->CreateStatement(NS_LITERAL_CSTRING(
    // Leverage the PRIMARY KEY (place_id, input) to insert/update entries
    "INSERT OR REPLACE INTO moz_inputhistory "
      // use_count will asymptotically approach the max of 10
      "SELECT h.id, IFNULL(i.input, ?1), IFNULL(i.use_count, 0) * .9 + 1 "
      "FROM moz_places_temp h "
      "LEFT JOIN moz_inputhistory i ON i.place_id = h.id AND i.input = ?1 "
      "WHERE url = ?2 "
      "UNION ALL "
      "SELECT h.id, IFNULL(i.input, ?1), IFNULL(i.use_count, 0) * .9 + 1 "
      "FROM moz_places h "
      "LEFT JOIN moz_inputhistory i ON i.place_id = h.id AND i.input = ?1 "
      "WHERE url = ?2 "
        "AND h.id NOT IN (SELECT id FROM moz_places_temp)"),
    getter_AddRefs(mDBFeedbackIncrease));
  NS_ENSURE_SUCCESS(rv, nsnull);

  return mDBFeedbackIncrease;
}

// nsNavHistory::CreateAutoCompleteQueries
//
//    The auto complete queries we use depend on options, so we have them in
//    a separate function so it can be re-created when the option changes.
//    We are not lazy creating these queries because they will be most likely
//    used on first search, and we don't want to lag on first autocomplete use.
nsresult
nsNavHistory::CreateAutoCompleteQueries()
{
  nsCString AutoCompleteQuery;
  GetAutoCompleteBaseQuery(AutoCompleteQuery);
  AutoCompleteQuery.ReplaceSubstring("{ADDITIONAL_CONDITIONS}", "");
  nsresult rv = mDBConn->CreateStatement(AutoCompleteQuery,
    getter_AddRefs(mDBAutoCompleteQuery));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCString AutoCompleteTypedQuery;
  GetAutoCompleteBaseQuery(AutoCompleteTypedQuery);
  AutoCompleteTypedQuery.ReplaceSubstring("{ADDITIONAL_CONDITIONS}",
                                          "AND h.typed = 1");
  rv = mDBConn->CreateStatement(AutoCompleteTypedQuery,
    getter_AddRefs(mDBAutoCompleteTypedQuery));
  NS_ENSURE_SUCCESS(rv, rv);

  // In this query we are taking BOOK_TAG_SQL only for h.id because it
  // uses data from moz_bookmarks table and we sync tables on bookmark insert.
  // So, most likely, h.id will always be populated when we have any bookmark.
  // We still need to join on moz_places_temp for other data (eg. title).
  nsCString sql = NS_LITERAL_CSTRING(
    "SELECT IFNULL(h_t.url, h.url), IFNULL(h_t.title, h.title), f.url ") +
      BOOK_TAG_SQL + NS_LITERAL_CSTRING(", "
      "IFNULL(h_t.visit_count, h.visit_count), IFNULL(h_t.typed, h.typed), rank "
    "FROM ( "
      "SELECT ROUND(MAX(((i.input = ?2) + (SUBSTR(i.input, 1, LENGTH(?2)) = ?2)) * "
        "i.use_count), 1) AS rank, place_id "
      "FROM moz_inputhistory i "
      "GROUP BY i.place_id HAVING rank > 0 "
      ") AS i "
    "LEFT JOIN moz_places h ON h.id = i.place_id "
    "LEFT JOIN moz_places_temp h_t ON h_t.id = i.place_id "
    "LEFT JOIN moz_favicons f ON f.id = IFNULL(h_t.favicon_id, h.favicon_id) "
    "WHERE IFNULL(h_t.url, h.url) NOTNULL "
    "ORDER BY rank DESC, IFNULL(h_t.frecency, h.frecency) DESC");
  rv = mDBConn->CreateStatement(sql, getter_AddRefs(mDBAdaptiveQuery));
  NS_ENSURE_SUCCESS(rv, rv);

  sql = NS_LITERAL_CSTRING(
    "SELECT IFNULL( "
        "(SELECT REPLACE(url, '%s', ?2) FROM moz_places_temp WHERE id = b.fk), "
        "(SELECT REPLACE(url, '%s', ?2) FROM moz_places WHERE id = b.fk) "
      ") AS search_url, IFNULL(h_t.title, h.title), "
      "COALESCE(f.url, "
        BEST_FAVICON_FOR_REVHOST("moz_places_temp") ", "
        BEST_FAVICON_FOR_REVHOST("moz_places")
      "), "
      "b.parent, b.title, NULL, IFNULL(h_t.visit_count, h.visit_count), "
      "IFNULL(h_t.typed, h.typed) "
    "FROM moz_keywords k "
    "JOIN moz_bookmarks b ON b.keyword_id = k.id "
    "LEFT JOIN moz_places AS h ON h.url = search_url "
    "LEFT JOIN moz_places_temp AS h_t ON h_t.url = search_url "
    "LEFT JOIN moz_favicons f ON f.id = IFNULL(h_t.favicon_id, h.favicon_id) "
    "WHERE LOWER(k.keyword) = LOWER(?1) "
    "ORDER BY IFNULL(h_t.frecency, h.frecency) DESC");
  rv = mDBConn->CreateStatement(sql, getter_AddRefs(mDBKeywordQuery));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// nsNavHistory::StartAutoCompleteTimer

nsresult
nsNavHistory::StartAutoCompleteTimer(PRUint32 aMilliseconds)
{
  nsresult rv;

  if (!mAutoCompleteTimer) {
    mAutoCompleteTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  
  rv = mAutoCompleteTimer->InitWithFuncCallback(AutoCompleteTimerCallback, this,
                                                aMilliseconds,
                                                nsITimer::TYPE_ONE_SHOT);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

// nsNavHistory::AutoCompleteTimerCallback

void // static
nsNavHistory::AutoCompleteTimerCallback(nsITimer* aTimer, void* aClosure)
{
  nsNavHistory* history = static_cast<nsNavHistory*>(aClosure);
  (void)history->PerformAutoComplete();
}

nsresult 
nsNavHistory::PerformAutoComplete()
{
  // if there is no listener, our search has been stopped
  if (!mCurrentListener)
    return NS_OK;

  nsresult rv;
  // Only do some extra searches on the first chunk
  if (!mCurrentChunkOffset) {
    // Only show keywords if there's a search
    if (mCurrentSearchTokens.Count()) {
      rv = AutoCompleteKeywordSearch();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    // Get adaptive results first
    rv = AutoCompleteAdaptiveSearch();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  PRBool moreChunksToSearch = PR_FALSE;
  // If we constructed a previous search query, use it instead of full
  if (mDBPreviousQuery) {
    rv = AutoCompletePreviousSearch();
    NS_ENSURE_SUCCESS(rv, rv);

    // We want to continue searching if we didn't finish last time, so move to
    // one before the previous chunk so that we move up to the previous chunk
    if (moreChunksToSearch = mPreviousChunkOffset != -1)
      mCurrentChunkOffset = mPreviousChunkOffset - mAutoCompleteSearchChunkSize;
  } else {
    rv = AutoCompleteFullHistorySearch(&moreChunksToSearch);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // If we ran out of pages to search, set offset to -1, so we can tell the
  // difference between completing and stopping because we have enough results
  PRBool notEnoughResults = !AutoCompleteHasEnoughResults();
  if (!moreChunksToSearch) {
    // But check first to see if we don't have enough results, and we're
    // matching word boundaries, so try again without the match restriction
    if (notEnoughResults && mCurrentMatchType == MATCH_BOUNDARY_ANYWHERE) {
      mCurrentMatchType = MATCH_ANYWHERE;
      mCurrentChunkOffset = -mAutoCompleteSearchChunkSize;
      moreChunksToSearch = PR_TRUE;
    } else {
      mCurrentChunkOffset = -1;
    }
  } else {
    // We know that we do have more chunks, so make sure we want more results
    moreChunksToSearch = notEnoughResults;
  }

  // Determine the result of the search
  PRUint32 count;
  mCurrentResult->GetMatchCount(&count); 

  if (count > 0) {
    mCurrentResult->SetSearchResult(moreChunksToSearch ?
      nsIAutoCompleteResult::RESULT_SUCCESS_ONGOING :
      nsIAutoCompleteResult::RESULT_SUCCESS);
    mCurrentResult->SetDefaultIndex(0);
  } else {
    mCurrentResult->SetSearchResult(moreChunksToSearch ?
      nsIAutoCompleteResult::RESULT_NOMATCH_ONGOING :
      nsIAutoCompleteResult::RESULT_NOMATCH);
    mCurrentResult->SetDefaultIndex(-1);
  }

  rv = mCurrentResult->SetListener(this);
  NS_ENSURE_SUCCESS(rv, rv);

  mCurrentListener->OnSearchResult(this, mCurrentResult);
 
  // if we're not done searching, adjust our current offset
  // and search the next chunk
  if (moreChunksToSearch) {
    mCurrentChunkOffset += mAutoCompleteSearchChunkSize;
    rv = StartAutoCompleteTimer(mAutoCompleteSearchTimeout);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    DoneSearching(PR_TRUE);
  }
  return NS_OK;
}

void
nsNavHistory::DoneSearching(PRBool aFinished)
{
  mPreviousMatchType = mCurrentMatchType;
  mPreviousChunkOffset = mCurrentChunkOffset;
  mAutoCompleteFinishedSearch = aFinished;
  mCurrentResult = nsnull;
  mCurrentListener = nsnull;
}

// nsNavHistory::StartSearch
//

NS_IMETHODIMP
nsNavHistory::StartSearch(const nsAString & aSearchString,
                          const nsAString & aSearchParam,
                          nsIAutoCompleteResult *aPreviousResult,
                          nsIAutoCompleteObserver *aListener)
{
  // We don't use aPreviousResult to get some matches from previous results in
  // order to make sure ordering of results are consistent between reusing and
  // not reusing results, see bug #412730 for details

  NS_ENSURE_ARG_POINTER(aListener);
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");

  // Lazily init nsITextToSubURI service
  if (!mTextURIService)
    mTextURIService = do_GetService(NS_ITEXTTOSUBURI_CONTRACTID);

  // Keep track of the previous search results to try optimizing
  PRUint32 prevMatchCount = mCurrentResultURLs.Count();
  nsAutoString prevSearchString(mCurrentSearchString);

  // Keep a copy of the original search for case-sensitive usage
  mOrigSearchString = aSearchString;
  // Remove whitespace, see bug #392141 for details
  mOrigSearchString.Trim(" \r\n\t\b");
  // Copy the input search string for case-insensitive search
  ToLowerCase(mOrigSearchString, mCurrentSearchString);

  // Fix up the search the same way we fix up the entry url
  mCurrentSearchString = FixupURIText(mCurrentSearchString);

  mCurrentListener = aListener;

  nsresult rv;
  mCurrentResult = do_CreateInstance(NS_AUTOCOMPLETESIMPLERESULT_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  mCurrentResult->SetSearchString(aSearchString);

  // Short circuit to give no results if we don't need to search for matches.
  // Use the previous in-progress search by looking at which urls it found if
  // the new search begins with the old one and both aren't empty. We don't run
  // into bug 412730 because we only specify urls and not titles to look at.
  // Also, only reuse the search if the previous and new search both start with
  // javascript: or both don't. (bug 417798)
  if (mAutoCompleteSearchSources == SEARCH_NONE ||
      (!prevSearchString.IsEmpty() &&
       StringBeginsWith(mCurrentSearchString, prevSearchString) &&
       (StartsWithJS(prevSearchString) == StartsWithJS(mCurrentSearchString)))) {

    // Got nothing before? We won't get anything new, so stop now
    if (mAutoCompleteSearchSources == SEARCH_NONE ||
        (mAutoCompleteFinishedSearch && prevMatchCount == 0)) {
      // Set up the result to let the listener know that there's nothing
      mCurrentResult->SetSearchResult(nsIAutoCompleteResult::RESULT_NOMATCH);
      mCurrentResult->SetDefaultIndex(-1);

      rv = mCurrentResult->SetListener(this);
      NS_ENSURE_SUCCESS(rv, rv);

      (void)mCurrentListener->OnSearchResult(this, mCurrentResult);
      DoneSearching(PR_TRUE);

      return NS_OK;
    } else {
      // We either have a previous in-progress search or a finished search that
      // has more than 0 results. We can continue from where the previous
      // search left off, but first we want to create an optimized query that
      // only searches through the urls that were previously found

      // We have to do the bindings for both tables, so we build a temporary
      // string
      nsCString bindings;
      for (PRUint32 i = 0; i < prevMatchCount; i++) {
        if (i)
          bindings += NS_LITERAL_CSTRING(",");

        // +2 to skip over the ?1 for the tag root parameter
        bindings += nsPrintfCString("?%d", i + 2);
      }

      nsCString sql = NS_LITERAL_CSTRING(
        "SELECT h.url, h.title, f.url") + BOOK_TAG_SQL + NS_LITERAL_CSTRING(", "
          "h.visit_count, h.typed "
        "FROM ( "
          "SELECT * FROM moz_places_temp "
          "WHERE url IN (") + bindings + NS_LITERAL_CSTRING(") "
          "UNION ALL "
          "SELECT * FROM moz_places "
          "WHERE id NOT IN (SELECT id FROM moz_places_temp) "
          "AND url IN (") + bindings + NS_LITERAL_CSTRING(") "
        ") AS h "
        "LEFT OUTER JOIN moz_favicons f ON f.id = h.favicon_id "
        "ORDER BY h.frecency DESC");

      rv = mDBConn->CreateStatement(sql, getter_AddRefs(mDBPreviousQuery));
      NS_ENSURE_SUCCESS(rv, rv);

      // Collect the previous result's URLs that we want to process
      nsStringArray urls;
      (void)mCurrentResultURLs.EnumerateRead(HashedURLsToArray, &urls);

      // Bind the parameters right away. We can only use the query once.
      for (PRUint32 i = 0; i < prevMatchCount; i++) {
        rv = mDBPreviousQuery->BindStringParameter(i + 1, *urls[i]);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Use the same match behavior as the previous search
      mCurrentMatchType = mPreviousMatchType;
    }
  } else {
    // Clear out any previous result queries
    mDBPreviousQuery = nsnull;
    // Default to matching based on the user's preference
    mCurrentMatchType = mAutoCompleteMatchBehavior;
  }

  mAutoCompleteFinishedSearch = PR_FALSE;
  mCurrentChunkOffset = 0;
  mCurrentResultURLs.Clear();
  mCurrentSearchTokens.Clear();
  mLivemarkFeedItemIds.Clear();
  mLivemarkFeedURIs.Clear();

  // Make the array of search tokens from the search string
  GenerateSearchTokens();

  // Figure out if we need to do special searches
  ProcessTokensForSpecialSearch();

  // find all the items that have the "livemark/feedURI" annotation
  // and save off their item ids and URIs. when doing autocomplete, 
  // if a result's parent item id matches a saved item id, the result
  // it is not really a bookmark, but a rss feed item.
  // if a results URI matches a saved URI, the result is a bookmark,
  // so we should show the star.
  mozStorageStatementScoper scope(mFoldersWithAnnotationQuery);

  rv = mFoldersWithAnnotationQuery->BindUTF8StringParameter(0, NS_LITERAL_CSTRING(LMANNO_FEEDURI));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  while (NS_SUCCEEDED(mFoldersWithAnnotationQuery->ExecuteStep(&hasMore)) && hasMore) {
    PRInt64 itemId = 0;
    rv = mFoldersWithAnnotationQuery->GetInt64(0, &itemId);
    NS_ENSURE_SUCCESS(rv, rv);
    mLivemarkFeedItemIds.Put(itemId, PR_TRUE);
    nsAutoString feedURI;
    // no need to worry about duplicates.
    rv = mFoldersWithAnnotationQuery->GetString(1, feedURI);
    NS_ENSURE_SUCCESS(rv, rv);
    mLivemarkFeedURIs.Put(feedURI, PR_TRUE);
  }

  // fire right away, we already waited to start searching
  rv = StartAutoCompleteTimer(0);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}

// nsNavHistory::StopSearch

NS_IMETHODIMP
nsNavHistory::StopSearch()
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");

  if (mAutoCompleteTimer)
    mAutoCompleteTimer->Cancel();

  DoneSearching(PR_FALSE);

  return NS_OK;
}

void
nsNavHistory::GenerateSearchTokens()
{
  // Split the search string into multiple search tokens
  nsString::const_iterator strStart, strEnd;
  mCurrentSearchString.BeginReading(strStart);
  mCurrentSearchString.EndReading(strEnd);
  nsString::const_iterator start = strStart, end = strEnd;
  while (FindInReadable(NS_LITERAL_STRING(" "), start, end)) {
    // Add in the current match
    nsAutoString currentMatch(Substring(strStart, start));
    AddSearchToken(currentMatch);

    // Reposition iterators
    strStart = start = end;
    end = strEnd;
  }
  
  // Add in the last match
  nsAutoString lastMatch(Substring(strStart, strEnd));
  AddSearchToken(lastMatch);
} 

inline void
nsNavHistory::AddSearchToken(nsAutoString &aToken)
{
  aToken.Trim("\r\n\t\b");
  if (!aToken.IsEmpty())
    mCurrentSearchTokens.AppendString(aToken);
}

void
nsNavHistory::ProcessTokensForSpecialSearch()
{
  // Start with the default behavior
  mAutoCompleteCurrentBehavior = mAutoCompleteDefaultBehavior;

  // If we're searching only one of history or bookmark, we can use filters
  if (mAutoCompleteSearchSources == SEARCH_HISTORY)
    SET_BEHAVIOR(History);
  else if (mAutoCompleteSearchSources == SEARCH_BOOKMARK)
    SET_BEHAVIOR(Bookmark);
  // SEARCH_BOTH doesn't require any filtering

  // Determine which special searches to apply
  for (PRInt32 i = mCurrentSearchTokens.Count(); --i >= 0; ) {
    PRBool needToRemove = PR_TRUE;
    const nsString *token = mCurrentSearchTokens.StringAt(i);

    if (token->Equals(mAutoCompleteRestrictHistory))
      SET_BEHAVIOR(History);
    else if (token->Equals(mAutoCompleteRestrictBookmark))
      SET_BEHAVIOR(Bookmark);
    else if (token->Equals(mAutoCompleteRestrictTag))
      SET_BEHAVIOR(Tag);
    else if (token->Equals(mAutoCompleteMatchTitle))
      SET_BEHAVIOR(Title);
    else if (token->Equals(mAutoCompleteMatchUrl))
      SET_BEHAVIOR(Url);
    else if (token->Equals(mAutoCompleteRestrictTyped))
      SET_BEHAVIOR(Typed);
    else
      needToRemove = PR_FALSE;

    // Remove the token if it's special search token
    if (needToRemove)
      (void)mCurrentSearchTokens.RemoveStringAt(i);
  }

  // Search only typed pages in history for empty searches
  if (mOrigSearchString.IsEmpty()) {
    SET_BEHAVIOR(History);
    SET_BEHAVIOR(Typed);
  }

  // We can use optimized queries for restricts, so check for the most
  // restrictive query first
  mDBCurrentQuery = GET_BEHAVIOR(Tag) ? GetDBAutoCompleteTagsQuery() :
    GET_BEHAVIOR(Bookmark) ? GetDBAutoCompleteStarQuery() :
    GET_BEHAVIOR(Typed) ? static_cast<mozIStorageStatement *>(mDBAutoCompleteTypedQuery) :
    GET_BEHAVIOR(History) ? GetDBAutoCompleteHistoryQuery() :
    static_cast<mozIStorageStatement *>(mDBAutoCompleteQuery);
}

nsresult
nsNavHistory::AutoCompleteKeywordSearch()
{
  mozStorageStatementScoper scope(mDBKeywordQuery);

  // Get the keyword parameters to replace the %s in the keyword search
  nsCAutoString params;
  PRInt32 paramPos = mOrigSearchString.FindChar(' ') + 1;
  // Make sure to escape them as if they were the query in a url, so " " become
  // "+"; "+" become "%2B"; non-ascii escaped
  NS_Escape(NS_ConvertUTF16toUTF8(Substring(mOrigSearchString, paramPos)),
    params, url_XPAlphas);

  // The first search term might be a keyword
  const nsAString &keyword = Substring(mOrigSearchString, 0,
    paramPos ? paramPos - 1 : mOrigSearchString.Length());
  nsresult rv = mDBKeywordQuery->BindStringParameter(0, keyword);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBKeywordQuery->BindUTF8StringParameter(1, params);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AutoCompleteProcessSearch(mDBKeywordQuery, QUERY_KEYWORD);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsNavHistory::AutoCompleteAdaptiveSearch()
{
  mozStorageStatementScoper scope(mDBAdaptiveQuery);

  nsresult rv = mDBAdaptiveQuery->BindInt32Parameter(0, GetTagsFolder());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBAdaptiveQuery->BindStringParameter(1, mCurrentSearchString);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AutoCompleteProcessSearch(mDBAdaptiveQuery, QUERY_FILTERED);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsNavHistory::AutoCompletePreviousSearch()
{
  nsresult rv = mDBPreviousQuery->BindInt32Parameter(0, GetTagsFolder());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AutoCompleteProcessSearch(mDBPreviousQuery, QUERY_FILTERED);
  NS_ENSURE_SUCCESS(rv, rv);

  // Don't use this query more than once
  mDBPreviousQuery = nsnull;

  return NS_OK;
}

// nsNavHistory::AutoCompleteFullHistorySearch
//
// Search for places that have a title, url, 
// or bookmark title(s) that contains mCurrentSearchString
// and are within our current chunk of "frecency".
//
// @param aHasMoreResults is false if the query found no matching items
//

nsresult
nsNavHistory::AutoCompleteFullHistorySearch(PRBool* aHasMoreResults)
{
  mozStorageStatementScoper scope(mDBCurrentQuery);

  nsresult rv = mDBCurrentQuery->BindInt32Parameter(0, GetTagsFolder());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBCurrentQuery->BindInt32Parameter(1, mAutoCompleteSearchChunkSize);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBCurrentQuery->BindInt32Parameter(2, mCurrentChunkOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AutoCompleteProcessSearch(mDBCurrentQuery, QUERY_FILTERED, aHasMoreResults);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsNavHistory::AutoCompleteProcessSearch(mozIStorageStatement* aQuery,
                                        const QueryType aType,
                                        PRBool *aHasMoreResults)
{
  // Unless we're checking if there are any results for the query, don't bother
  // processing results if we already have enough results
  if (!aHasMoreResults && AutoCompleteHasEnoughResults())
    return NS_OK;

  nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
  NS_ENSURE_TRUE(faviconService, NS_ERROR_OUT_OF_MEMORY);

  // We want to filter javascript: URIs if the search doesn't start with it
  PRBool filterJavascript = mAutoCompleteFilterJavascript &&
    !StartsWithJS(mCurrentSearchString);

  // Determine what type of search to try matching tokens against targets
  PRBool (*tokenMatchesTarget)(const nsAString &, const nsAString &);
  switch (mCurrentMatchType) {
    case MATCH_ANYWHERE:
      tokenMatchesTarget = FindAnywhere;
      break;
    case MATCH_BEGINNING:
      tokenMatchesTarget = FindBeginning;
      break;
    case MATCH_BOUNDARY_ANYWHERE:
    case MATCH_BOUNDARY:
    default:
      tokenMatchesTarget = FindOnBoundary;
      break;
  }

  PRBool hasMore = PR_FALSE;
  // Determine the result of the search
  while (NS_SUCCEEDED(aQuery->ExecuteStep(&hasMore)) && hasMore) {
    nsAutoString escapedEntryURL;
    nsresult rv = aQuery->GetString(kAutoCompleteIndex_URL, escapedEntryURL);
    NS_ENSURE_SUCCESS(rv, rv);

    // If we need to filter and have a javascript URI.. skip!
    if (filterJavascript && StartsWithJS(escapedEntryURL))
      continue;

    // Prevent duplicates that might appear from previous searches such as tag
    // results and chunking. Because we use mCurrentResultURLs to remove
    // duplicates, the first url wins, so we might not show it as a "star" if
    // the parentId we get first is the one for the livemark and not the
    // bookmark or no "star" at all.
    // XXX bug 412734
    PRBool dummy;
    if (!mCurrentResultURLs.Get(escapedEntryURL, &dummy)) {
      PRInt64 parentId = 0;
      nsAutoString entryTitle, entryFavicon, entryBookmarkTitle;
      rv = aQuery->GetString(kAutoCompleteIndex_Title, entryTitle);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = aQuery->GetString(kAutoCompleteIndex_FaviconURL, entryFavicon);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = aQuery->GetInt64(kAutoCompleteIndex_ParentId, &parentId);
      NS_ENSURE_SUCCESS(rv, rv);
      
      // Only fetch the bookmark title if we have a bookmark
      if (parentId) {
        rv = aQuery->GetString(kAutoCompleteIndex_BookmarkTitle, entryBookmarkTitle);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      nsAutoString entryTags;
      rv = aQuery->GetString(kAutoCompleteIndex_Tags, entryTags);
      NS_ENSURE_SUCCESS(rv, rv);
      PRInt32 visitCount = 0;
      rv = aQuery->GetInt32(kAutoCompleteIndex_VisitCount, &visitCount);
      NS_ENSURE_SUCCESS(rv, rv);
      PRInt32 typed = 0;
      rv = aQuery->GetInt32(kAutoCompleteIndex_Typed, &typed);

      // Always prefer the bookmark title unless it's empty
      nsAutoString title =
        entryBookmarkTitle.IsEmpty() ? entryTitle : entryBookmarkTitle;

      nsString style;
      switch (aType) {
        case QUERY_KEYWORD: {
          // If we don't have a title, then we must have a keyword, so let the
          // UI know it's a keyword; otherwise, we found an exact page match,
          // so just show the page like a regular result
          if (entryTitle.IsEmpty())
            style = NS_LITERAL_STRING("keyword");
          else
            title = entryTitle;

          break;
        }
        case QUERY_FILTERED: {
          // If we get any results, there's potentially another chunk to proces
          if (aHasMoreResults)
            *aHasMoreResults = PR_TRUE;

          // Keep track of if we've matched all the filter requirements such as
          // only history items, only bookmarks, only tags. If a given restrict
          // is active, make sure a corresponding condition is *not* true. If
          // any are violated, matchAll will be false.
          PRBool matchAll = !((GET_BEHAVIOR(History) && visitCount == 0) ||
                              (GET_BEHAVIOR(Typed) && typed == 0) ||
                              (GET_BEHAVIOR(Bookmark) && !parentId) ||
                              (GET_BEHAVIOR(Tag) && entryTags.IsEmpty()));

          // Unescape the url to search for unescaped terms
          nsString entryURL = FixupURIText(escapedEntryURL);

          // Determine if every token matches either the bookmark title, tags,
          // page title, or page url
          for (PRInt32 i = 0; i < mCurrentSearchTokens.Count() && matchAll; i++) {
            const nsString *token = mCurrentSearchTokens.StringAt(i);

            // Check if the tags match the search term
            PRBool matchTags = (*tokenMatchesTarget)(*token, entryTags);
            // Check if the title matches the search term
            PRBool matchTitle = (*tokenMatchesTarget)(*token, title);

            // Make sure we match something in the title or tags if we have to
            matchAll = matchTags || matchTitle;
            if (GET_BEHAVIOR(Title) && !matchAll)
              break;

            // Check if the url matches the search term
            PRBool matchUrl = (*tokenMatchesTarget)(*token, entryURL);
            // If we don't match the url when we have to, reset matchAll to
            // false; otherwise keep track that we did match the current search
            if (GET_BEHAVIOR(Url) && !matchUrl)
              matchAll = PR_FALSE;
            else
              matchAll |= matchUrl;
          }

          // Skip if we don't match all terms in the bookmark, tag, title or url
          if (!matchAll)
            continue;

          break;
        }
      }

      // Always prefer to show tags if we have them
      PRBool showTags = !entryTags.IsEmpty();

      // Add the tags to the title if necessary
      if (showTags)
        title += TITLE_TAGS_SEPARATOR + entryTags;

      // Tags have a special style to show a tag icon; otherwise, style the
      // bookmarks that aren't feed items and feed URIs as bookmark
      if (style.IsEmpty()) {
        if (showTags)
          style = NS_LITERAL_STRING("tag");
        else if ((parentId && !mLivemarkFeedItemIds.Get(parentId, &dummy)) ||
                 mLivemarkFeedURIs.Get(escapedEntryURL, &dummy))
          style = NS_LITERAL_STRING("bookmark");
        else
          style = NS_LITERAL_STRING("favicon");
      }

      // Get the URI for the favicon
      nsCAutoString faviconSpec;
      faviconService->GetFaviconSpecForIconString(
        NS_ConvertUTF16toUTF8(entryFavicon), faviconSpec);
      NS_ConvertUTF8toUTF16 faviconURI(faviconSpec);

      // New item: append to our results and put it in our hash table
      rv = mCurrentResult->AppendMatch(escapedEntryURL, title, faviconURI, style);
      NS_ENSURE_SUCCESS(rv, rv);
      mCurrentResultURLs.Put(escapedEntryURL, PR_TRUE);

      // Stop processing if we have enough results
      if (AutoCompleteHasEnoughResults())
        break;
    }
  }

  return NS_OK;
}

inline PRBool
nsNavHistory::AutoCompleteHasEnoughResults()
{
  return mCurrentResultURLs.Count() >= (PRUint32)mAutoCompleteMaxResults;
}

// nsNavHistory::OnValueRemoved (nsIAutoCompleteSimpleResultListener)

NS_IMETHODIMP
nsNavHistory::OnValueRemoved(nsIAutoCompleteSimpleResult* aResult,
                             const nsAString& aValue, PRBool aRemoveFromDb)
{
  NS_ASSERTION(NS_IsMainThread(), "This can only be called on the main thread");

  if (!aRemoveFromDb)
    return NS_OK;

  nsresult rv;
  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), aValue);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = RemovePage(uri);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsNavHistory::AutoCompleteFeedback(PRInt32 aIndex,
                                   nsIAutoCompleteController *aController)
{
  // we don't track user choices in the awesomebar in private browsing mode
  if (InPrivateBrowsingMode())
    return NS_OK;

  mozIStorageStatement *stmt = GetDBFeedbackIncrease();
  mozStorageStatementScoper scope(stmt);

  nsAutoString input;
  nsresult rv = aController->GetSearchString(input);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindStringParameter(0, input);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString url;
  rv = aController->GetValueAt(aIndex, url);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = stmt->BindStringParameter(1, url);
  NS_ENSURE_SUCCESS(rv, rv);

  // We do the update async and we don't care about failures
  nsCOMPtr<AutoCompleteStatementCallbackNotifier> callback =
    new AutoCompleteStatementCallbackNotifier();
  nsCOMPtr<mozIStoragePendingStatement> canceler;
  rv = stmt->ExecuteAsync(callback, getter_AddRefs(canceler));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsString
nsNavHistory::FixupURIText(const nsAString &aURIText)
{
  // Unescaping utilities expect UTF8 strings
  NS_ConvertUTF16toUTF8 escaped(aURIText);

  // Strip off some prefixes so we don't have to match the exact protocol for
  // sites. E.g., http://mozilla.org/ can match other mozilla.org pages.
  if (StringBeginsWith(escaped, NS_LITERAL_CSTRING("https://")))
    escaped.Cut(0, 8);
  else if (StringBeginsWith(escaped, NS_LITERAL_CSTRING("http://")))
    escaped.Cut(0, 7);
  else if (StringBeginsWith(escaped, NS_LITERAL_CSTRING("ftp://")))
    escaped.Cut(0, 6);

  nsString fixedUp;
  // Use the service if we have it to avoid invalid UTF8 strings
  if (mTextURIService) {
    mTextURIService->UnEscapeURIForUI(NS_LITERAL_CSTRING("UTF-8"),
      escaped, fixedUp);
    return fixedUp;
  }

  // Fallback on using this if the service is unavailable for some reason
  NS_UnescapeURL(escaped);
  CopyUTF8toUTF16(escaped, fixedUp);
  return fixedUp;
}
