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
    // No need to search for tags if there's nothing to search
    if (!mCurrentSearchString.IsEmpty()) {
      rv = AutoCompleteTagsSearch();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  PRBool moreChunksToSearch = PR_FALSE;
  rv = AutoCompleteFullHistorySearch(&moreChunksToSearch);
  NS_ENSURE_SUCCESS(rv, rv);

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

  // Copy the input search string for case-insensitive search
  ToLowerCase(aSearchString, mCurrentSearchString);
  // remove whitespace, see bug #392141 for details
  mCurrentSearchString.Trim(" \r\n\t\b");

  mCurrentListener = aListener;

  nsresult rv;
  mCurrentResult = do_CreateInstance(NS_AUTOCOMPLETESIMPLERESULT_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

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

  mCurrentSearchString.Truncate();
  DoneSearching();

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
nsNavHistory::AutoCompleteTagsSearch()
{
  // NOTE:
  // after migration or clear all private data, we might end up with
  // a lot of places with frecency = -1 (until idle)
  // 
  // XXX bug 412736
  // in the case of a frecency tie, break it with h.typed and h.visit_count
  // which is better than nothing.  but this is slow, so not doing it yet.
  nsCString sql = NS_LITERAL_CSTRING(
    "SELECT h.url, h.title, f.url, b.id, b.parent, b.title "
    "FROM moz_places h "
    "JOIN moz_bookmarks b ON b.fk = h.id "
    "LEFT OUTER JOIN moz_favicons f ON f.id = h.favicon_id "
    "WHERE h.frecency <> 0 AND (b.parent IN "
      "(SELECT t.id FROM moz_bookmarks t WHERE t.parent = ?1 AND (");

  nsStringArray terms;
  CreateTermsFromTokens(mCurrentSearchTokens, terms);

  for (PRInt32 i = 0; i < terms.Count(); i++) {
    if (i)
      sql += NS_LITERAL_CSTRING(" OR");

    // +2 to skip over the "?1", which is the tag root parameter
    sql += NS_LITERAL_CSTRING(" LOWER(t.title) = ") +
           nsPrintfCString("LOWER(?%d)", i + 2);
  }

  sql += NS_LITERAL_CSTRING("))) "
    "ORDER BY h.frecency DESC");

  nsCOMPtr<mozIStorageStatement> tagAutoCompleteQuery;
  nsresult rv = mDBConn->CreateStatement(sql,
    getter_AddRefs(tagAutoCompleteQuery));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = tagAutoCompleteQuery->BindInt64Parameter(0, GetTagsFolder());
  NS_ENSURE_SUCCESS(rv, rv);

  for (PRInt32 i = 0; i < terms.Count(); i++) {
    // +1 to skip over the "?1", which is the tag root parameter
    rv = tagAutoCompleteQuery->BindStringParameter(i + 1, *(terms.StringAt(i)));
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = AutoCompleteProcessSearch(tagAutoCompleteQuery, QUERY_TAGS);
  NS_ENSURE_SUCCESS(rv, rv);

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

  rv = AutoCompleteProcessSearch(mDBAutoCompleteQuery, QUERY_FULL, aHasMoreResults);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

nsresult
nsNavHistory::AutoCompleteProcessSearch(mozIStorageStatement* aQuery,
                                        const QueryType aType,
                                        PRBool *aHasMoreResults)
{
  // Don't bother processing results if we already have enough results
  if (AutoCompleteHasEnoughResults())
    return NS_OK;

  nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
  NS_ENSURE_TRUE(faviconService, NS_ERROR_OUT_OF_MEMORY);

  // We want to filter javascript: URIs if the search doesn't start with it
  const nsString &javascriptColon = NS_LITERAL_STRING("javascript:");
  PRBool filterJavascript = mAutoCompleteFilterJavascript &&
    mCurrentSearchString.Find(javascriptColon) != 0;

  PRBool hasMore = PR_FALSE;
  // Determine the result of the search
  while (NS_SUCCEEDED(aQuery->ExecuteStep(&hasMore)) && hasMore) {
    nsAutoString escapedEntryURL;
    nsresult rv = aQuery->GetString(kAutoCompleteIndex_URL, escapedEntryURL);
    NS_ENSURE_SUCCESS(rv, rv);

    // If we need to filter and have a javascript URI.. skip!
    if (filterJavascript && escapedEntryURL.Find(javascriptColon) == 0)
      continue;

    // Prevent duplicates that might appear from previous searches such as tag
    // results and chunking. Because we use mCurrentResultURLs to remove
    // duplicates, the first url wins, so we might not show it as a "star" if
    // the parentId we get first is the one for the livemark and not the
    // bookmark or no "star" at all.
    // XXX bug 412734
    PRBool dummy;
    if (!mCurrentResultURLs.Get(escapedEntryURL, &dummy)) {
      // Convert the escaped UTF16 (all ascii) to ASCII then unescape to UTF16
      NS_LossyConvertUTF16toASCII cEntryURL(escapedEntryURL);
      NS_UnescapeURL(cEntryURL);
      NS_ConvertUTF8toUTF16 entryURL(cEntryURL);

      nsAutoString entryTitle, entryFavicon, entryBookmarkTitle;
      rv = aQuery->GetString(kAutoCompleteIndex_Title, entryTitle);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = aQuery->GetString(kAutoCompleteIndex_FaviconURL, entryFavicon);
      NS_ENSURE_SUCCESS(rv, rv);
      PRInt64 itemId = 0;
      rv = aQuery->GetInt64(kAutoCompleteIndex_ItemId, &itemId);
      NS_ENSURE_SUCCESS(rv, rv);

      PRInt64 parentId = 0;
      // Only bother to fetch parent id and bookmark title if we have a bookmark
      if (itemId) {
        rv = aQuery->GetInt64(kAutoCompleteIndex_ParentId, &parentId);
        NS_ENSURE_SUCCESS(rv, rv);
        rv = aQuery->GetString(kAutoCompleteIndex_BookmarkTitle, entryBookmarkTitle);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      PRBool useBookmark = PR_FALSE;
      nsString style;
      switch (aType) {
        case QUERY_TAGS: {
          // Always prefer the bookmark title unless we don't have one
          useBookmark = !entryBookmarkTitle.IsEmpty();

          // Tags have a special stype to show a tag icon
          style = NS_LITERAL_STRING("tag");

          break;
        }
        case QUERY_FULL: {
          // If we get any results, there's potentially another chunk to proces
          if (aHasMoreResults)
            *aHasMoreResults = PR_TRUE;

          // Determine if every token matches either the bookmark, title, or url
          PRBool matchAll = PR_TRUE;
          for (PRInt32 i = 0; i < mCurrentSearchTokens.Count() && matchAll; i++) {
            const nsString *token = mCurrentSearchTokens.StringAt(i);

            // Check if the current token matches the bookmark
            PRBool bookmarkMatch = itemId &&
              CaseInsensitiveFindInReadable(*token, entryBookmarkTitle);
            // If any part of the search string is in the bookmark title, show
            // that in the result instead of the page title
            useBookmark |= bookmarkMatch;

            // True if any of them match; false makes us quit the loop
            matchAll = bookmarkMatch ||
              CaseInsensitiveFindInReadable(*token, entryTitle) ||
              CaseInsensitiveFindInReadable(*token, entryURL);
          }

          // Skip if we don't match all terms in the bookmark, title or url
          if (!matchAll)
            continue;

          // Style bookmarks that aren't feed items and feed URIs as bookmark
          style = (itemId && !mLivemarkFeedItemIds.Get(parentId, &dummy)) ||
            mLivemarkFeedURIs.Get(escapedEntryURL, &dummy) ?
            NS_LITERAL_STRING("bookmark") : NS_LITERAL_STRING("favicon");

          break;
        }
        default: {
          // Always prefer the bookmark title unless we don't have one
          useBookmark = !entryBookmarkTitle.IsEmpty();

          // Style bookmarks that aren't feed items and feed URIs as bookmark
          style = (itemId && !mLivemarkFeedItemIds.Get(parentId, &dummy)) ||
            mLivemarkFeedURIs.Get(escapedEntryURL, &dummy) ?
            NS_LITERAL_STRING("bookmark") : NS_LITERAL_STRING("favicon");

          break;
        }
      }

      // Pick the right title to show based on the result of the query type
      const nsAString &title = useBookmark ? entryBookmarkTitle : entryTitle;

      // Get the URI for the favicon
      nsCAutoString faviconSpec;
      faviconService->GetFaviconSpecForIconString(
        NS_ConvertUTF16toUTF8(entryFavicon), faviconSpec);
      NS_ConvertUTF8toUTF16 faviconURI(faviconSpec);

      // New item: append to our results and put it in our hash table
      rv = mCurrentResult->AppendMatch(entryURL, title, faviconURI, style);
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
