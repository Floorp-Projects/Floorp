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
 * The current algorithm searches history from now backwards to the oldest
 * visit in chunks of time (AUTOCOMPLETE_SEARCH_CHUNK).  We currently
 * do SQL LIKE searches of the search term in the url and title
 * within in each chunk of time.  the results are ordered by visit_count 
 * (and then visit_date), giving us poor man's "frecency".
 */

#include "nsNavHistory.h"
#include "nsNetUtil.h"

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

#define NS_AUTOCOMPLETESIMPLERESULT_CONTRACTID \
  "@mozilla.org/autocomplete/simple-result;1"

// This is the maximum results we'll return for a "typed" search
// This happens in response to clicking the down arrow next to the URL.
#define AUTOCOMPLETE_MAX_PER_TYPED 100
#define LMANNO_FEEDURI "livemark/feedURI"

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
    "SELECT annos.item_id, annos.content FROM moz_anno_attributes attrs " 
    "JOIN moz_items_annos annos ON attrs.id = annos.anno_attribute_id "
    "WHERE attrs.name = \"" LMANNO_FEEDURI "\";");
  nsresult rv = mDBConn->CreateStatement(sql, getter_AddRefs(mLivemarkFeedsQuery));
  NS_ENSURE_SUCCESS(rv, rv);

  sql = NS_LITERAL_CSTRING(
    "SELECT h.url, h.title, f.url, b.id, b.parent "
    "FROM moz_places h "
    "JOIN moz_historyvisits v ON h.id = v.place_id "
    "LEFT OUTER JOIN moz_bookmarks b ON b.fk = h.id "
    "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
    "WHERE v.visit_date >= ?1 AND v.visit_date <= ?2 AND h.hidden <> 1 AND "
    " v.visit_type <> 0 AND v.visit_type <> 4 AND ");

  if (mAutoCompleteOnlyTyped)
    sql += NS_LITERAL_CSTRING("h.typed = 1 AND ");

  sql += NS_LITERAL_CSTRING(
    "(h.title LIKE ?3 ESCAPE '/' OR h.url LIKE ?3 ESCAPE '/') "
    "GROUP BY h.id ORDER BY h.visit_count DESC, MAX(v.visit_date) DESC;");

  rv = mDBConn->CreateStatement(sql, getter_AddRefs(mDBAutoCompleteQuery));
  NS_ENSURE_SUCCESS(rv, rv);

  sql = NS_LITERAL_CSTRING(
    "SELECT h.url, h.title, f.url, b.id, b.parent "
    "FROM moz_places h "
    "JOIN moz_bookmarks b ON b.fk = h.id "
    "LEFT OUTER JOIN moz_historyvisits v ON h.id = v.place_id "
    "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
    "WHERE "
    "(b.parent = (SELECT t.id FROM moz_bookmarks t WHERE t.parent = ?1 and t.title LIKE ?2 ESCAPE '/')) "
    "GROUP BY h.id ORDER BY h.visit_count DESC, MAX(v.visit_date) DESC;");
  rv = mDBConn->CreateStatement(sql, getter_AddRefs(mDBTagAutoCompleteQuery));
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

static const PRInt64 USECS_PER_DAY = LL_INIT(20, 500654080);

// search in day chunks.  too big, and the UI will be unresponsive
// as we will be off searching the database.
// too short, and because of AUTOCOMPLETE_SEARCH_TIMEOUT
// results won't come back in fast enough to feel snappy.
// because we sort within chunks by visit_count (then visit_date)
// choose 4 days so that Friday's searches are in the first chunk
// and those will affect Monday's results
#define AUTOCOMPLETE_SEARCH_CHUNK (USECS_PER_DAY * 4)

// wait this many milliseconds between searches
// too short, and the UI will be unresponsive
// as we will be off searching the database.
// too big, and results won't come back in fast enough to feel snappy.
#define AUTOCOMPLETE_SEARCH_TIMEOUT 100

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
  PRBool moreChunksToSearch = PR_FALSE;

  nsresult rv;
  // results will be put into mCurrentResult  
  if (mCurrentSearchString.IsEmpty())
    rv = AutoCompleteTypedSearch();
  else {
    // only search tags on the first chunk,
    // but before we search history, as we want tagged
    // items to show up first.
    if (mFirstChunk) {
      rv = AutoCompleteTagsSearch();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = AutoCompleteFullHistorySearch();
    moreChunksToSearch = (mCurrentChunkEndTime >= mCurrentOldestVisit);
  }
  NS_ENSURE_SUCCESS(rv, rv);
 
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
 
  // if we're not done searching, adjust our end time and 
  // search the next earlier chunk of time
  if (moreChunksToSearch) {
    mFirstChunk = PR_FALSE;
    mCurrentChunkEndTime -= AUTOCOMPLETE_SEARCH_CHUNK;
    rv = StartAutoCompleteTimer(AUTOCOMPLETE_SEARCH_TIMEOUT);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return rv;
}

// nsNavHistory::StartSearch
//

NS_IMETHODIMP
nsNavHistory::StartSearch(const nsAString & aSearchString,
                          const nsAString & aSearchParam,
                          nsIAutoCompleteResult *aPreviousResult,
                          nsIAutoCompleteObserver *aListener)
{
  NS_ENSURE_ARG_POINTER(aListener);
  mCurrentSearchString = aSearchString;
  // remove whitespace, see bug #392141 for details
  mCurrentSearchString.Trim(" \r\n\t\b");
  mCurrentListener = aListener;
  nsresult rv;

  // determine if we can start by searching through the previous search results.
  // if we can't, we need to reset mCurrentChunkEndTime and mCurrentOldestVisit.
  // if we can, we will search through our previous search results and then resume 
  // searching using the previous mCurrentChunkEndTime and mCurrentOldestVisit values.
  PRBool searchPrevious = PR_FALSE;
  if (aPreviousResult) {
    nsAutoString prevSearchString;
    aPreviousResult->GetSearchString(prevSearchString);

    // if search string begins with the previous search string, it's a go.
    // but don't search previous results if the previous search string was empty
    // or if the current search string is empty.  (an empty search string is a "typed only"
    // search from when clicking on the drop down to the right of the url bar.)
    searchPrevious = !prevSearchString.IsEmpty() && Substring(mCurrentSearchString, 0,
                      prevSearchString.Length()).Equals(prevSearchString);
  }

  mCurrentResult = do_CreateInstance(NS_AUTOCOMPLETESIMPLERESULT_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mCurrentResultURLs.Clear();

  // if we are searching through our previous results,
  // we don't need to regenerate these hash tables
  if (!searchPrevious) {
    mLivemarkFeedItemIds.Clear();
    mLivemarkFeedURIs.Clear();

    // find all the items that have the "livemark/feedURI" annotation
    // and save off their item ids and URIs. when doing autocomplete, 
    // if a result's parent item id matches a saved item id, the result
    // it is not really a bookmark, but a rss feed item.
    // if a results URI matches a saved URI, the result is a bookmark,
    // so we should show the star.
    mozStorageStatementScoper scope(mLivemarkFeedsQuery);
    PRBool hasMore = PR_FALSE;
    while (NS_SUCCEEDED(mLivemarkFeedsQuery->ExecuteStep(&hasMore)) && hasMore) {
      PRInt64 itemId = 0;
      mLivemarkFeedsQuery->GetInt64(0, &itemId);
      mLivemarkFeedItemIds.Put(itemId, PR_TRUE);
      nsAutoString feedURI;
      // no need to worry about duplicates.
      mLivemarkFeedsQuery->GetString(1, feedURI);
      mLivemarkFeedURIs.Put(feedURI, PR_TRUE);
    }
  }

  // Search through the previous result
  if (searchPrevious) {
    // when searching our previous results
    // we need to first re-search tags.
    // if our previous search was for "foo", we would have shown
    // items tagged with "foo".  if our current search is "food"
    // we need to re-search for tags matching "food" first.
    // then, when processing results we have to remove any previous tag matches
    rv = AutoCompleteTagsSearch();
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 matchCount;
    aPreviousResult->GetMatchCount(&matchCount);
    for (PRUint32 i = 0; i < matchCount; i++) {
      // if the previous result item was a tagged item, and it matches
      // the current search term (due to url or title) don't include it.
      // find it "naturally" with a history or bookmark search.
      // 
      // note, if the previous result item is tagged with the current search term,
      // our call to AutoCompleteTagsSearch() above will find it.
      nsAutoString style;
      aPreviousResult->GetStyleAt(i, style);
      if (!style.Equals(NS_LITERAL_STRING("tag"))) {
        nsAutoString url, title;
        aPreviousResult->GetValueAt(i, url);
        aPreviousResult->GetCommentAt(i, title);

        PRBool isMatch = CaseInsensitiveFindInReadable(mCurrentSearchString, url);
        if (!isMatch)
          isMatch = CaseInsensitiveFindInReadable(mCurrentSearchString, title);

        if (isMatch) {
          nsAutoString image;
          aPreviousResult->GetImageAt(i, image);

          mCurrentResultURLs.Put(url, PR_TRUE);
  
          rv = mCurrentResult->AppendMatch(url, title, image, style);
          NS_ENSURE_SUCCESS(rv, rv);
        }
      }
    }
    // if we found some results, announce them now instead of waiting
    // to do the first db search.
    PRUint32 count;
    mCurrentResult->GetMatchCount(&count); 

    if (count > 0) {
      mCurrentResult->SetSearchResult(nsIAutoCompleteResult::RESULT_SUCCESS_ONGOING);
      mCurrentResult->SetDefaultIndex(0);
    }
  }
  else if (!mCurrentSearchString.IsEmpty()) {
    // reset to mCurrentChunkEndTime 
    mCurrentChunkEndTime = PR_Now();
    mFirstChunk = PR_TRUE;

    // determine our earliest visit
    nsCOMPtr<mozIStorageStatement> dbSelectStatement;
    rv = mDBConn->CreateStatement(
      NS_LITERAL_CSTRING("SELECT MIN(visit_date) id FROM moz_historyvisits WHERE visit_type <> 4 AND visit_type <> 0"),
      getter_AddRefs(dbSelectStatement));
    NS_ENSURE_SUCCESS(rv, rv);
    PRBool hasMinVisit;
    rv = dbSelectStatement->ExecuteStep(&hasMinVisit);
    NS_ENSURE_SUCCESS(rv, rv);
  
    if (hasMinVisit) {
      rv = dbSelectStatement->GetInt64(0, &mCurrentOldestVisit);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    else {
      // if we have no visits, use a reasonable value
      mCurrentOldestVisit = PR_Now() - USECS_PER_DAY;
    }
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

  mCurrentSearchString.Truncate();
  mCurrentListener = nsnull;

  return NS_OK;
}

// nsNavHistory::AutoCompleteTypedSearch
//
//    Called when there is no search string. This happens when you press
//    down arrow from the URL bar: the most recent things you typed are listed.
//
//    Ordering here is simpler because there are no boosts for typing, and there
//    is no URL information to use. The ordering just comes out of the DB by
//    visit count (primary) and time since last visited (secondary).

nsresult nsNavHistory::AutoCompleteTypedSearch()
{
  nsCOMPtr<mozIStorageStatement> dbSelectStatement;

  nsCString sql = NS_LITERAL_CSTRING(
    "SELECT h.url, h.title, f.url, b.id, b.parent "
    "FROM moz_places h "
    "LEFT OUTER JOIN moz_bookmarks b ON b.fk = h.id "
    "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
    "JOIN moz_historyvisits v ON h.id = v.place_id WHERE (h.id IN "
    "(SELECT DISTINCT h.id from moz_historyvisits v, moz_places h WHERE "
    "v.place_id = h.id AND h.typed = 1 AND v.visit_type <> 0 AND v.visit_type <> 4 "
    "ORDER BY v.visit_date DESC LIMIT ");
  sql.AppendInt(AUTOCOMPLETE_MAX_PER_TYPED);
  sql += NS_LITERAL_CSTRING(")) GROUP BY h.id ORDER BY MAX(v.visit_date) DESC");  
  
  nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
  NS_ENSURE_TRUE(faviconService, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = mDBConn->CreateStatement(sql, getter_AddRefs(dbSelectStatement));
  NS_ENSURE_SUCCESS(rv, rv);
 
  PRBool hasMore = PR_FALSE;
  while (NS_SUCCEEDED(dbSelectStatement->ExecuteStep(&hasMore)) && hasMore) {
    nsAutoString entryURL, entryTitle, entryFavicon;
    dbSelectStatement->GetString(kAutoCompleteIndex_URL, entryURL);
    dbSelectStatement->GetString(kAutoCompleteIndex_Title, entryTitle);
    dbSelectStatement->GetString(kAutoCompleteIndex_FaviconURL, entryFavicon);
    PRInt64 itemId = 0;
    dbSelectStatement->GetInt64(kAutoCompleteIndex_ItemId, &itemId);
    PRInt64 parentId = 0;
    dbSelectStatement->GetInt64(kAutoCompleteIndex_ParentId, &parentId);

    PRBool dummy;
    // don't show rss feed items as bookmarked,
    // but do show rss feed URIs as bookmarked.
    PRBool isBookmark = (itemId != 0 && 
                         !mLivemarkFeedItemIds.Get(parentId, &dummy)) ||
                        mLivemarkFeedURIs.Get(entryURL, &dummy);   

    nsCAutoString imageSpec;
    faviconService->GetFaviconSpecForIconString(
      NS_ConvertUTF16toUTF8(entryFavicon), imageSpec);
    rv = mCurrentResult->AppendMatch(entryURL, entryTitle, 
      NS_ConvertUTF8toUTF16(imageSpec), isBookmark ? NS_LITERAL_STRING("bookmark") : NS_LITERAL_STRING("favicon"));
    NS_ENSURE_SUCCESS(rv, rv);
  } 
  return NS_OK;
}

nsresult
nsNavHistory::AutoCompleteTagsSearch()
{
  mozStorageStatementScoper scope(mDBTagAutoCompleteQuery);

  nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
  NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);

  PRInt64 tagRoot;
  nsresult rv = bookmarks->GetTagRoot(&tagRoot);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBTagAutoCompleteQuery->BindInt64Parameter(0, tagRoot);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString escapedSearchString;
  rv = mDBTagAutoCompleteQuery->EscapeStringForLIKE(mCurrentSearchString, PRUnichar('/'), escapedSearchString);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBTagAutoCompleteQuery->BindStringParameter(1, escapedSearchString);
  NS_ENSURE_SUCCESS(rv, rv);

  nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
  NS_ENSURE_TRUE(faviconService, NS_ERROR_OUT_OF_MEMORY);

  PRBool hasMore = PR_FALSE;

  // Determine the result of the search
  while (NS_SUCCEEDED(mDBTagAutoCompleteQuery->ExecuteStep(&hasMore)) && hasMore) {
    nsAutoString entryURL, entryTitle, entryFavicon;
    rv = mDBTagAutoCompleteQuery->GetString(kAutoCompleteIndex_URL, entryURL);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBTagAutoCompleteQuery->GetString(kAutoCompleteIndex_Title, entryTitle);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBTagAutoCompleteQuery->GetString(kAutoCompleteIndex_FaviconURL, entryFavicon);
    NS_ENSURE_SUCCESS(rv, rv);
    PRInt64 itemId = 0;
    rv = mDBTagAutoCompleteQuery->GetInt64(kAutoCompleteIndex_ItemId, &itemId);
    NS_ENSURE_SUCCESS(rv, rv);
    PRInt64 parentId = 0;
    rv = mDBTagAutoCompleteQuery->GetInt64(kAutoCompleteIndex_ParentId, &parentId);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool dummy;
    // prevent duplicates.  this can happen when chunking as we
    // may have already seen this URL from an earlier chunk of time
    if (!mCurrentResultURLs.Get(entryURL, &dummy)) {
      // new item, append to our results and put it in our hash table.
      nsCAutoString faviconSpec;
      faviconService->GetFaviconSpecForIconString(
        NS_ConvertUTF16toUTF8(entryFavicon), faviconSpec);
      rv = mCurrentResult->AppendMatch(entryURL, entryTitle, 
        NS_ConvertUTF8toUTF16(faviconSpec), NS_LITERAL_STRING("tag"));
      NS_ENSURE_SUCCESS(rv, rv);

      mCurrentResultURLs.Put(entryURL, PR_TRUE);
    }
  }

  return NS_OK;
}

// nsNavHistory::AutoCompleteFullHistorySearch
//
// Search history for visits that have a url or title that contains mCurrentSearchString
// and are within our current chunk of time:
// between (mCurrentChunkEndTime - AUTOCOMPLETE_SEARCH_CHUNK) and (mCurrentChunkEndTime)
//

nsresult
nsNavHistory::AutoCompleteFullHistorySearch()
{
  mozStorageStatementScoper scope(mDBAutoCompleteQuery);

  nsresult rv = mDBAutoCompleteQuery->BindInt64Parameter(0, mCurrentChunkEndTime - AUTOCOMPLETE_SEARCH_CHUNK);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBAutoCompleteQuery->BindInt64Parameter(1, mCurrentChunkEndTime);
  NS_ENSURE_SUCCESS(rv, rv);

  nsString escapedSearchString;
  rv = mDBAutoCompleteQuery->EscapeStringForLIKE(mCurrentSearchString, PRUnichar('/'), escapedSearchString);
  NS_ENSURE_SUCCESS(rv, rv);

  // prepend and append with % for "contains"
  rv = mDBAutoCompleteQuery->BindStringParameter(2, NS_LITERAL_STRING("%") + escapedSearchString + NS_LITERAL_STRING("%"));
  NS_ENSURE_SUCCESS(rv, rv);

  nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
  NS_ENSURE_TRUE(faviconService, NS_ERROR_OUT_OF_MEMORY);

  PRBool hasMore = PR_FALSE;

  // Determine the result of the search
  while (NS_SUCCEEDED(mDBAutoCompleteQuery->ExecuteStep(&hasMore)) && hasMore) {
    nsAutoString entryURL, entryTitle, entryFavicon;
    rv = mDBAutoCompleteQuery->GetString(kAutoCompleteIndex_URL, entryURL);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBAutoCompleteQuery->GetString(kAutoCompleteIndex_Title, entryTitle);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBAutoCompleteQuery->GetString(kAutoCompleteIndex_FaviconURL, entryFavicon);
    NS_ENSURE_SUCCESS(rv, rv);
    PRInt64 itemId = 0;
    rv = mDBAutoCompleteQuery->GetInt64(kAutoCompleteIndex_ItemId, &itemId);
    NS_ENSURE_SUCCESS(rv, rv);
    PRInt64 parentId = 0;
    rv = mDBAutoCompleteQuery->GetInt64(kAutoCompleteIndex_ParentId, &parentId);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool dummy;
    // don't show rss feed items as bookmarked,
    // but do show rss feed URIs as bookmarked.
    PRBool isBookmark = (itemId != 0 && 
                         !mLivemarkFeedItemIds.Get(parentId, &dummy)) ||
                         mLivemarkFeedURIs.Get(entryURL, &dummy);  

    // prevent duplicates.  this can happen when chunking as we
    // may have already seen this URL from an earlier chunk of time
    if (!mCurrentResultURLs.Get(entryURL, &dummy)) {
      // new item, append to our results and put it in our hash table.
      nsCAutoString faviconSpec;
      faviconService->GetFaviconSpecForIconString(
        NS_ConvertUTF16toUTF8(entryFavicon), faviconSpec);
      rv = mCurrentResult->AppendMatch(entryURL, entryTitle, 
        NS_ConvertUTF8toUTF16(faviconSpec), isBookmark ? NS_LITERAL_STRING("bookmark") : NS_LITERAL_STRING("favicon"));
      NS_ENSURE_SUCCESS(rv, rv);

      mCurrentResultURLs.Put(entryURL, PR_TRUE);
    }
  }

  return NS_OK;
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
