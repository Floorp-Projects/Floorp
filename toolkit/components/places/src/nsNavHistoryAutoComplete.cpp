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

#define NS_AUTOCOMPLETESIMPLERESULT_CONTRACTID \
  "@mozilla.org/autocomplete/simple-result;1"

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
  NS_LITERAL_CSTRING(") " name)

// Get three named columns from the bookmarks and tags table
#define BOOK_TAG_SQL (\
  SQL_STR_FRAGMENT_GET_BOOK_TAG("parent", "b.parent", "!=", PR_TRUE) + \
  SQL_STR_FRAGMENT_GET_BOOK_TAG("bookmark", "b.title", "!=", PR_TRUE) + \
  SQL_STR_FRAGMENT_GET_BOOK_TAG("tags", "GROUP_CONCAT(t.title, ',')", "=", PR_FALSE))

// This separator is used as an RTL-friendly way to split the title and tags.
// It can also be used by an nsIAutoCompleteResult consumer to re-split the
// "comment" back into the title and tag.
NS_NAMED_LITERAL_STRING(kTitleTagsSeparator, " \u2013 ");

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


// nsNavHistory::CreateAutoCompleteQueries
//
//    The auto complete queries we use depend on options, so we have them in
//    a separate function so it can be re-created when the option changes.

nsresult
nsNavHistory::CreateAutoCompleteQueries()
{
  nsCString sql = NS_LITERAL_CSTRING(
    "SELECT h.url, h.title, f.url") + BOOK_TAG_SQL + NS_LITERAL_CSTRING(" "
    "FROM moz_places h "
    "LEFT OUTER JOIN moz_favicons f ON f.id = h.favicon_id "
    "WHERE h.frecency <> 0 ");

  if (mAutoCompleteOnlyTyped)
    sql += NS_LITERAL_CSTRING("AND h.typed = 1 ");

  // NOTE:
  // after migration or clear all private data, we might end up with
  // a lot of places with frecency < 0 (until idle)
  //
  // XXX bug 412736
  // in the case of a frecency tie, break it with h.typed and h.visit_count
  // which is better than nothing.  but this is slow, so not doing it yet.
  sql += NS_LITERAL_CSTRING(
    "ORDER BY h.frecency DESC LIMIT ?2 OFFSET ?3");
  nsresult rv = mDBConn->CreateStatement(sql, 
    getter_AddRefs(mDBAutoCompleteQuery));
  NS_ENSURE_SUCCESS(rv, rv);

  sql = NS_LITERAL_CSTRING(
    "SELECT h.url, h.title, f.url") + BOOK_TAG_SQL + NS_LITERAL_CSTRING(", "
      "ROUND(MAX(((i.input = ?2) + (SUBSTR(i.input, 1, LENGTH(?2)) = ?2)) * "
                "i.use_count), 1) rank "
    "FROM moz_inputhistory i "
    "JOIN moz_places h ON h.id = i.place_id "
    "LEFT OUTER JOIN moz_favicons f ON f.id = h.favicon_id "
    "GROUP BY i.place_id HAVING rank > 0 "
    "ORDER BY rank DESC, h.frecency DESC");
  rv = mDBConn->CreateStatement(sql, getter_AddRefs(mDBAdaptiveQuery));
  NS_ENSURE_SUCCESS(rv, rv);

  sql = NS_LITERAL_CSTRING(
    // Leverage the PRIMARY KEY (place_id, input) to insert/update entries
    "INSERT OR REPLACE INTO moz_inputhistory "
    // use_count will asymptotically approach the max of 10
    "SELECT h.id, IFNULL(i.input, ?1), IFNULL(i.use_count, 0) * .9 + 1 "
    "FROM moz_places h "
    "LEFT OUTER JOIN moz_inputhistory i ON i.place_id = h.id AND i.input = ?1 "
    "WHERE h.url = ?2");
  rv = mDBConn->CreateStatement(sql, getter_AddRefs(mDBFeedbackIncrease));
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

  mCurrentResult->SetSearchString(mCurrentSearchString);

  nsresult rv;
  // Only do some extra searches on the first chunk
  if (!mCurrentChunkOffset) {
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
  if (!moreChunksToSearch)
    mCurrentChunkOffset = -1;

  // Only search more chunks if there are more and we need more results
  moreChunksToSearch &= !AutoCompleteHasEnoughResults();
 
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

  // Lazily init nsITextToSubURI service
  if (!mTextURIService)
    mTextURIService = do_GetService(NS_ITEXTTOSUBURI_CONTRACTID);

  // Keep track of the previous search results to try optimizing
  PRUint32 prevMatchCount = mCurrentResultURLs.Count();
  nsAutoString prevSearchString(mCurrentSearchString);

  // Copy the input search string for case-insensitive search
  ToLowerCase(aSearchString, mCurrentSearchString);
  // remove whitespace, see bug #392141 for details
  mCurrentSearchString.Trim(" \r\n\t\b");

  mCurrentListener = aListener;

  nsresult rv;
  mCurrentResult = do_CreateInstance(NS_AUTOCOMPLETESIMPLERESULT_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Use the previous in-progress search by looking at which urls it found if
  // the new search begins with the old one and both aren't empty. We don't run
  // into bug 412730 because we only specify urls and not titles to look at.
  // Also, only reuse the search if the previous and new search both start with
  // javascript: or both don't. (bug 417798)
  if (!prevSearchString.IsEmpty() &&
      StringBeginsWith(mCurrentSearchString, prevSearchString) &&
      (StartsWithJS(prevSearchString) == StartsWithJS(mCurrentSearchString))) {

    // Got nothing before? We won't get anything new, so stop now
    if (mAutoCompleteFinishedSearch && prevMatchCount == 0) {
      // Set up the result to let the listener know that there's nothing
      mCurrentResult->SetSearchString(mCurrentSearchString);
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
      nsCString sql = NS_LITERAL_CSTRING(
        "SELECT h.url, h.title, f.url") + BOOK_TAG_SQL + NS_LITERAL_CSTRING(" "
        "FROM moz_places h "
        "LEFT OUTER JOIN moz_favicons f ON f.id = h.favicon_id "
        "WHERE h.url IN (");

      // Put in bind spots for the urls
      for (PRUint32 i = 0; i < prevMatchCount; i++) {
        if (i)
          sql += NS_LITERAL_CSTRING(",");

        // +2 to skip over the ?1 for the tag root parameter
        sql += nsPrintfCString("?%d", i + 2);
      }

      sql += NS_LITERAL_CSTRING(") "
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
    }
  } else {
    // Clear out any previous result queries
    mDBPreviousQuery = nsnull;
  }

  mAutoCompleteFinishedSearch = PR_FALSE;
  mCurrentChunkOffset = 0;
  mCurrentResultURLs.Clear();
  mCurrentSearchTokens.Clear();
  mLivemarkFeedItemIds.Clear();
  mLivemarkFeedURIs.Clear();

  // Make the array of search tokens from the search string
  GenerateSearchTokens();

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

nsresult
nsNavHistory::AutoCompleteAdaptiveSearch()
{
  mozStorageStatementScoper scope(mDBAdaptiveQuery);

  nsresult rv = mDBAdaptiveQuery->BindInt32Parameter(0, GetTagsFolder());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBAdaptiveQuery->BindStringParameter(1, mCurrentSearchString);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AutoCompleteProcessSearch(mDBAdaptiveQuery, QUERY_ADAPTIVE);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsNavHistory::AutoCompletePreviousSearch()
{
  nsresult rv = mDBPreviousQuery->BindInt32Parameter(0, GetTagsFolder());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AutoCompleteProcessSearch(mDBPreviousQuery, QUERY_FULL);
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
  mozStorageStatementScoper scope(mDBAutoCompleteQuery);

  nsresult rv = mDBAutoCompleteQuery->BindInt32Parameter(0, GetTagsFolder());
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBAutoCompleteQuery->BindInt32Parameter(1, mAutoCompleteSearchChunkSize);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBAutoCompleteQuery->BindInt32Parameter(2, mCurrentChunkOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = AutoCompleteProcessSearch(mDBAutoCompleteQuery, QUERY_FULL, aHasMoreResults);
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
  PRBool (*tokenMatchesTarget)(const nsAString &, const nsAString &) =
    mAutoCompleteOnWordBoundary ? FindOnBoundary : FindAnywhere;

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

      // Always prefer the bookmark title unless it's empty
      nsAutoString title =
        entryBookmarkTitle.IsEmpty() ? entryTitle : entryBookmarkTitle;

      nsString style;
      switch (aType) {
        case QUERY_FULL: {
          // If we get any results, there's potentially another chunk to proces
          if (aHasMoreResults)
            *aHasMoreResults = PR_TRUE;

          // Unescape the url to search for unescaped terms
          nsString entryURL = FixupURIText(escapedEntryURL);

          // Determine if every token matches either the bookmark title, tags,
          // page title, or page url
          PRBool matchAll = PR_TRUE;
          for (PRInt32 i = 0; i < mCurrentSearchTokens.Count() && matchAll; i++) {
            const nsString *token = mCurrentSearchTokens.StringAt(i);

            // Check if the tags match the search term
            PRBool matchTags = (*tokenMatchesTarget)(*token, entryTags);
            // Check if the title matches the search term
            PRBool matchTitle = (*tokenMatchesTarget)(*token, title);
            // Check if the url matches the search term
            PRBool matchUrl = (*tokenMatchesTarget)(*token, entryURL);

            // True if any of them match; false makes us quit the loop
            matchAll = matchTags || matchTitle || matchUrl;
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
        title += kTitleTagsSeparator + entryTags;

      // Tags have a special style to show a tag icon; otherwise, style the
      // bookmarks that aren't feed items and feed URIs as bookmark
      style = showTags ? NS_LITERAL_STRING("tag") : (parentId &&
        !mLivemarkFeedItemIds.Get(parentId, &dummy)) ||
        mLivemarkFeedURIs.Get(escapedEntryURL, &dummy) ?
        NS_LITERAL_STRING("bookmark") : NS_LITERAL_STRING("favicon");

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
  mozStorageStatementScoper scope(mDBFeedbackIncrease);

  nsAutoString input;
  nsresult rv = aController->GetSearchString(input);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBFeedbackIncrease->BindStringParameter(0, input);
  NS_ENSURE_SUCCESS(rv, rv);

  nsAutoString url;
  rv = aController->GetValueAt(aIndex, url);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBFeedbackIncrease->BindStringParameter(1, url);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBFeedbackIncrease->Execute();
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsString
nsNavHistory::FixupURIText(const nsAString &aURIText)
{
  // Unescaping utilities expect UTF8 strings
  NS_ConvertUTF16toUTF8 escaped(aURIText);

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
