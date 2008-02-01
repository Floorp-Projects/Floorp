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
 * But places with frecency (-1) are included, as that means that these items
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

// This is the maximum results we'll return for a "typed" search
// This happens in response to clicking the down arrow next to the URL.
// XXX todo, check if doing rich autocomplete, and if so, limit to the max results?
#define AUTOCOMPLETE_MAX_PER_TYPED 100

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
    "SELECT h.url, h.title, f.url, b.id, b.parent, b.title "
    "FROM moz_places h "
    "LEFT OUTER JOIN moz_bookmarks b ON b.fk = h.id "
    "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
    "WHERE h.frecency <> 0 ");

  if (mAutoCompleteOnlyTyped)
    sql += NS_LITERAL_CSTRING("AND h.typed = 1 ");

  // NOTE:
  // after migration or clear all private data, we might end up with
  // a lot of places with frecency = -1 (until idle)
  //
  // XXX bug 412736
  // in the case of a frecency tie, break it with h.typed and h.visit_count
  // which is better than nothing.  but this is slow, so not doing it yet.
  sql += NS_LITERAL_CSTRING(
    "ORDER BY h.frecency DESC LIMIT ?1 OFFSET ?2");

  nsresult rv = mDBConn->CreateStatement(sql, 
    getter_AddRefs(mDBAutoCompleteQuery));
  NS_ENSURE_SUCCESS(rv, rv);

  // NOTE:
  // after migration or clear all private data, we might end up with
  // a lot of places with frecency = -1 (until idle)
  // 
  // XXX bug 412736
  // in the case of a frecency tie, break it with h.typed and h.visit_count
  // which is better than nothing.  but this is slow, so not doing it yet.
  sql = NS_LITERAL_CSTRING(
    "SELECT h.url, h.title, f.url, b.id, b.parent "
    "FROM moz_places h "
    "JOIN moz_bookmarks b ON b.fk = h.id "
    "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
    "WHERE h.frecency <> 0 AND "
    "(b.parent in (SELECT t.id FROM moz_bookmarks t WHERE t.parent = ?1 AND LOWER(t.title) = LOWER(?2))) "
    "ORDER BY h.frecency DESC");

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
    // but before we search places, as we want tagged
    // items to show up first.
    if (!mCurrentChunkOffset) {
      rv = AutoCompleteTagsSearch();
      NS_ENSURE_SUCCESS(rv, rv);
    }

    rv = AutoCompleteFullHistorySearch(&moreChunksToSearch);
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
 
  // if we're not done searching, adjust our current offset
  // and search the next chunk
  if (moreChunksToSearch) {
    mCurrentChunkOffset += mAutoCompleteSearchChunkSize;
    rv = StartAutoCompleteTimer(mAutoCompleteSearchTimeout);
    NS_ENSURE_SUCCESS(rv, rv);
  } else {
    DoneSearching();
  }
  return NS_OK;
}

void
nsNavHistory::DoneSearching()
{
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
  mCurrentSearchString = aSearchString;
  // remove whitespace, see bug #392141 for details
  mCurrentSearchString.Trim(" \r\n\t\b");

  mCurrentListener = aListener;

  nsresult rv;
  mCurrentResult = do_CreateInstance(NS_AUTOCOMPLETESIMPLERESULT_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  mCurrentChunkOffset = 0;
  mCurrentResultURLs.Clear();
  mLivemarkFeedItemIds.Clear();
  mLivemarkFeedURIs.Clear();

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

  mCurrentSearchString.Truncate();
  DoneSearching();

  return NS_OK;
}

// nsNavHistory::AutoCompleteTypedSearch
//
//    Called when there is no search string. This happens when you press
//    down arrow from the URL bar: the most "frecent" things you typed are listed.
//

nsresult nsNavHistory::AutoCompleteTypedSearch()
{
  nsCOMPtr<mozIStorageStatement> dbSelectStatement;

  // NOTE:
  // after migration or clear all private data, we might end up with
  // a lot of places with frecency = -1 (until idle)
  //
  // XXX bug 412736
  // in the case of a frecency tie, break it with h.typed and h.visit_count
  // which is better than nothing.  but this is slow, so not doing it yet.
  //
  // GROUP BY h.id to prevent duplicates.  For mDBAutoCompleteQuery,
  // we use the mCurrentResultURLs hash to accomplish this.
  //
  // NOTE:  because we are grouping by h.id, b.id and b.parent 
  // get collapsed, so if something is both a livemark and a bookmark
  // we might not show it as a "star" if the parentId we return is 
  // the one for the livemark item, and not the bookmark item.
  // XXX bug 412734
  nsCString sql = NS_LITERAL_CSTRING(
    "SELECT h.url, h.title, f.url, b.id, b.parent "
    "FROM moz_places h "
    "LEFT OUTER JOIN moz_bookmarks b ON b.fk = h.id "
    "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
    "WHERE h.frecency <> 0 AND h.typed = 1 "
    "GROUP BY h.id ORDER BY h.frecency DESC LIMIT ");
  sql.AppendInt(AUTOCOMPLETE_MAX_PER_TYPED);
  
  nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
  NS_ENSURE_TRUE(faviconService, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = mDBConn->CreateStatement(sql, 
    getter_AddRefs(dbSelectStatement));
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
    if (itemId)
      dbSelectStatement->GetInt64(kAutoCompleteIndex_ParentId, &parentId);

    PRBool dummy;
    // don't show rss feed items as bookmarked,
    // but do show rss feed URIs as bookmarked.
    PRBool isBookmark = (itemId && 
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
  nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
  NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv;

  PRInt64 tagsFolder = GetTagsFolder();

  nsString::const_iterator strStart, strEnd;
  mCurrentSearchString.BeginReading(strStart);
  mCurrentSearchString.EndReading(strEnd);
  nsString::const_iterator start = strStart, end = strEnd;

  nsStringArray tagTokens;

  // check if we have any delimiters
  while (FindInReadable(NS_LITERAL_STRING(" "), start, end,
                        nsDefaultStringComparator())) {
    nsAutoString currentMatch(Substring(strStart, start));
    currentMatch.Trim("\r\n\t\b");
    if (!currentMatch.IsEmpty())
      tagTokens.AppendString(currentMatch);
    strStart = start = end;
    end = strEnd;
  }

  nsCOMPtr<mozIStorageStatement> tagAutoCompleteQuery;

  // we didn't find any spaces, so we only have one possible tag, which is
  // the search string.  this is the common case, so we use 
  // our pre-compiled query
  if (!tagTokens.Count()) {
    tagAutoCompleteQuery = mDBTagAutoCompleteQuery;

    rv = tagAutoCompleteQuery->BindInt64Parameter(0, tagsFolder);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = tagAutoCompleteQuery->BindStringParameter(1, mCurrentSearchString);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else {
    // add in the last match (if it is non-empty)
    nsAutoString lastMatch(Substring(strStart, strEnd));
    lastMatch.Trim("\r\n\t\b");
    if (!lastMatch.IsEmpty())
      tagTokens.AppendString(lastMatch);

    nsCString tagQuery = NS_LITERAL_CSTRING(
      "SELECT h.url, h.title, f.url, b.id, b.parent "
      "FROM moz_places h "
      "JOIN moz_bookmarks b ON b.fk = h.id "
      "LEFT OUTER JOIN moz_favicons f ON h.favicon_id = f.id "
      "WHERE h.frecency <> 0 AND "
      "(b.parent in "
      " (SELECT t.id FROM moz_bookmarks t WHERE t.parent = ?1 AND (");

    nsStringArray terms;
    CreateTermsFromTokens(tagTokens, terms);

    for (PRUint32 i=0; i<terms.Count(); i++) {
      if (i)
        tagQuery += NS_LITERAL_CSTRING(" OR");

      // +2 to skip over the "?1", which is the tag root parameter
      tagQuery += NS_LITERAL_CSTRING(" LOWER(t.title) = ") +
                  nsPrintfCString("LOWER(?%d)", i+2);
    }

    tagQuery += NS_LITERAL_CSTRING("))) "
      "GROUP BY h.id ORDER BY h.frecency DESC");

    rv = mDBConn->CreateStatement(tagQuery, getter_AddRefs(tagAutoCompleteQuery));
    NS_ENSURE_SUCCESS(rv, rv);

    rv = tagAutoCompleteQuery->BindInt64Parameter(0, tagsFolder);
    NS_ENSURE_SUCCESS(rv, rv);

    for (PRUint32 i=0; i<terms.Count(); i++) {
      // +1 to skip over the "?1", which is the tag root parameter
      rv = tagAutoCompleteQuery->BindStringParameter(i+1, *(terms.StringAt(i)));
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
  NS_ENSURE_TRUE(faviconService, NS_ERROR_OUT_OF_MEMORY);

  mozStorageStatementScoper scope(tagAutoCompleteQuery);

  PRBool hasMore = PR_FALSE;

  // Determine the result of the search
  while (NS_SUCCEEDED(tagAutoCompleteQuery->ExecuteStep(&hasMore)) && hasMore) {
    nsAutoString entryURL;
    rv = tagAutoCompleteQuery->GetString(kAutoCompleteIndex_URL, entryURL);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool dummy;
    // prevent duplicates.  this can happen when chunking as we
    // may have already seen this URL.
    // NOTE:  because we use mCurrentResultURLs to remove duplicates,
    // the first url wins.
    // so we might not show it as a "star" if the parentId we get first is
    // the one for the livemark item, and not the bookmark item,
    // we may not show the "star" even though we should.
    // XXX bug 412734
    if (!mCurrentResultURLs.Get(entryURL, &dummy)) {
      nsAutoString entryTitle, entryFavicon;
      rv = tagAutoCompleteQuery->GetString(kAutoCompleteIndex_Title, entryTitle);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = tagAutoCompleteQuery->GetString(kAutoCompleteIndex_FaviconURL, entryFavicon);
      NS_ENSURE_SUCCESS(rv, rv);
      PRInt64 itemId = 0;
      rv = tagAutoCompleteQuery->GetInt64(kAutoCompleteIndex_ItemId, &itemId);
      NS_ENSURE_SUCCESS(rv, rv);
      PRInt64 parentId = 0;
      if (itemId) {
        rv = tagAutoCompleteQuery->GetInt64(kAutoCompleteIndex_ParentId, &parentId);
        NS_ENSURE_SUCCESS(rv, rv);
      }

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

  nsresult rv = mDBAutoCompleteQuery->BindInt32Parameter(0, mAutoCompleteSearchChunkSize);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = mDBAutoCompleteQuery->BindInt32Parameter(1, mCurrentChunkOffset);
  NS_ENSURE_SUCCESS(rv, rv);

  nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
  NS_ENSURE_TRUE(faviconService, NS_ERROR_OUT_OF_MEMORY);

  PRBool hasMore = PR_FALSE;

  // Determine the result of the search
  while (NS_SUCCEEDED(mDBAutoCompleteQuery->ExecuteStep(&hasMore)) && hasMore) {
    *aHasMoreResults = PR_TRUE;
    nsAutoString escapedEntryURL;
    rv = mDBAutoCompleteQuery->GetString(kAutoCompleteIndex_URL, escapedEntryURL);
    NS_ENSURE_SUCCESS(rv, rv);

    // prevent duplicates.  this can happen when chunking as we
    // may have already seen this URL from our tag search or an earlier
    // chunk.
    PRBool dummy;
    if (!mCurrentResultURLs.Get(escapedEntryURL, &dummy)) {
      // Convert the escaped UTF16 (all ascii) to ASCII then unescape to UTF16
      NS_LossyConvertUTF16toASCII cEntryURL(escapedEntryURL);
      NS_UnescapeURL(cEntryURL);
      NS_ConvertUTF8toUTF16 entryURL(cEntryURL);

      nsAutoString entryTitle, entryFavicon, entryBookmarkTitle;
      rv = mDBAutoCompleteQuery->GetString(kAutoCompleteIndex_Title, entryTitle);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = mDBAutoCompleteQuery->GetString(kAutoCompleteIndex_FaviconURL, entryFavicon);
      NS_ENSURE_SUCCESS(rv, rv);
      PRInt64 itemId = 0;
      rv = mDBAutoCompleteQuery->GetInt64(kAutoCompleteIndex_ItemId, &itemId);
      NS_ENSURE_SUCCESS(rv, rv);

      PRInt64 parentId = 0;
      // only bother to fetch parent id and bookmark title 
      // if we have a bookmark (itemId != 0)
      if (itemId) {
        rv = mDBAutoCompleteQuery->GetInt64(kAutoCompleteIndex_ParentId, &parentId);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = mDBAutoCompleteQuery->GetString(kAutoCompleteIndex_BookmarkTitle, entryBookmarkTitle);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // If the search string is in the bookmark title, show that in the result
      // (instead of the page title)
      PRBool matchInBookmarkTitle = itemId && 
        CaseInsensitiveFindInReadable(mCurrentSearchString, entryBookmarkTitle);

      // If we don't match the bookmark, title or url, don't add the result
      if (!matchInBookmarkTitle &&
          !CaseInsensitiveFindInReadable(mCurrentSearchString, entryTitle) &&
          !CaseInsensitiveFindInReadable(mCurrentSearchString, entryURL))
        continue;

      // don't show rss feed items as bookmarked,
      // but do show rss feed URIs as bookmarked.
      //
      // NOTE:  because we use mCurrentResultURLs to remove duplicates,
      // the first url wins.
      // so we might not show it as a "star" if the parentId we get first is
      // the one for the livemark item, and not the bookmark item,
      // we may not show the "star" even though we should.
      // XXX bug 412734
      PRBool isBookmark = (itemId && 
                           !mLivemarkFeedItemIds.Get(parentId, &dummy)) ||
                           mLivemarkFeedURIs.Get(entryURL, &dummy);  

      // new item, append to our results and put it in our hash table.
      nsCAutoString faviconSpec;
      faviconService->GetFaviconSpecForIconString(
        NS_ConvertUTF16toUTF8(entryFavicon), faviconSpec);

      rv = mCurrentResult->AppendMatch(entryURL, 
        matchInBookmarkTitle ? entryBookmarkTitle : entryTitle, 
        NS_ConvertUTF8toUTF16(faviconSpec), 
        isBookmark ? NS_LITERAL_STRING("bookmark") : NS_LITERAL_STRING("favicon"));
      NS_ENSURE_SUCCESS(rv, rv);

      mCurrentResultURLs.Put(entryURL, PR_TRUE);

      if (mCurrentResultURLs.Count() >= mAutoCompleteMaxResults) {
        *aHasMoreResults = PR_FALSE;
        break;
      }
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
