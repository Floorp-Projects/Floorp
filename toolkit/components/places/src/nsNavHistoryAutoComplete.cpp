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
 * Scoring
 * -------
 * Generally ordering is by visit count. We given boosts to items that have
 * been bookmarked or typed into the address bar (as opposed to clicked links).
 * The penalties below for schemes and prefix matches also apply. We also
 * prefer paths (URLs ending in '/') and try to have shorter ones generally
 * appear first. The results are then presented in descending numeric order
 * using this score.
 *
 * Database queries
 * ----------------
 * It is tempting to do a select for "url LIKE user_input" but this is a very
 * slow query since it is brute-force over all URLs in histroy. We can,
 * however, do very efficient queries using < and > for strings. These
 * operators use the index over the URL column.
 *
 * Therefore, we try to prepend any prefixes that should be considered and
 * query for them individually. Therefore, we execute several very efficient
 * queries, score the results, and sort it.
 *
 * To limit the amount of searching we do, we ask the database to order the
 * results based on visit count for us and give us on the top N results.
 * These results will be in approximate order of score. As long as each query
 * has more results than the user is likely to see, they will not notice the
 * effects of this.
 *
 * First see if any specs match that from the beginning
 * ----------------------------------------------------
 * - If it matches the beginning of a known prefix prefix, exclude that prefix
 *   when querying. We would therefore exclude "http://" and "https://" if you type
 *   "ht". But match any other schemes that begin with "ht" (probably none).
 *
 * - Penalize all results. Most people don't type the scheme and don't want
 *   matches like this. This will make "file://" links go below
 *   "http://www.fido.com/". If one is typing the scheme "file:" for example, by
 *   the time you type the colon it won't match anything else (like "http://file:")
 *   and the penalty won't have any effect on the ordering (it will be applied to
 *   all results).
 *
 * Try different known prefixes
 * ----------------------------
 * - Prepend each prefix, running a query. If the concatenated string is itself a
 *   prefix of another known prefix (ie input is "w" and we prepend "http://", it
 *   will be a prefix of "http://www."), select out that known prefix. In this
 *   case we'll query for everything starting with "http://w" except things
 *   starting with "http://www."
 *
 * - For each selected out prefix above, run a query but apply prefix match
 *   penalty to the results. This way you'll still get "http://www." results
 *   if you type "w", but they will generally be lower than "http://wookie.net/"
 *   For your favorite few sites with many visits, you might still get matches
 *   for "www" first, which is probably what you want for your favorite sites.
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

#define NS_AUTOCOMPLETESIMPLERESULT_CONTRACTID \
  "@mozilla.org/autocomplete/simple-result;1"

// Size of visit count boost to give to URLs which are sites or paths
#define AUTOCOMPLETE_NONPAGE_VISIT_COUNT_BOOST 5

// Boost to give to URLs which have been typed
#define AUTOCOMPLETE_TYPED_BOOST 5

// Boost to give to URLs which are bookmarked
#define AUTOCOMPLETE_BOOKMARKED_BOOST 5

// Penalty to add to sites that match a prefix. For example, if I type "w"
// we will complte on "http://www.w..." like normal. We we will also complete
// on "http://w" which will match almost every site, but will have this penalty
// applied so they will come later. We want a pretty big penalty so that you'll
// only get "www" beating domain names that start with w for your very favorite
// sites.
#define AUTOCOMPLETE_MATCHES_PREFIX_PENALTY (-50)

// Penalty applied to matches that don't have prefixes applied. See
// discussion above.
#define AUTOCOMPLETE_MATCHES_SCHEME_PENALTY (-20)

// Number of results we will consider for each prefix. Each prefix lookup is
// done separately. Typically, we will only match one prefix, so this should be
// a sufficient number to give "enough" autocomplete matches per prefix. The
// total number of results that could ever be returned is this times the number
// of prefixes. This should be as small as is reasonable to make it faster.
#define AUTOCOMPLETE_MAX_PER_PREFIX 50

// This is the maximum results we'll return for a "typed" search (usually
// happens in response to clicking the down arrow next to the URL).
#define AUTOCOMPLETE_MAX_PER_TYPED 100

// This is the maximum number of visits that an item can have for us to
// try to remove the path and put a virtual item with just the hostname as the
// first entry. The virtual item helps the case where you've visited a site deep
// down and want to visit the main site. This limit makes sure we don't take
// the first autocomplete spot for a page you are more likely to go to.
#define AUTOCOMPLETE_MAX_TRUNCATION_VISIT 6

PRInt32 ComputeAutoCompletePriority(const nsAString& aUrl, PRInt32 aVisitCount,
                                    PRBool aWasTyped, PRBool aIsBookmarked);
nsresult NormalizeAutocompleteInput(const nsAString& aInput,
                                    nsString& aOutput);

// nsIAutoCompleteSearch *******************************************************


// AutoCompleteIntermediateResult/Set
//
//    This class holds intermediate autocomplete results so that they can be
//    sorted. This list is then handed off to a result using FillResult. The
//    major reason for this is so that we can use nsArray's sorting functions,
//    not use COM, yet have well-defined lifetimes for the objects. This uses
//    a void array, but makes sure to delete the objects on desctruction.

struct AutoCompleteIntermediateResult
{
  AutoCompleteIntermediateResult(const nsString& aUrl, const nsString& aTitle,
                                 PRInt32 aVisitCount, PRInt32 aPriority) :
    url(aUrl), title(aTitle), visitCount(aVisitCount), priority(aPriority) {}
  nsString url;
  nsString title;
  PRInt32 visitCount;
  PRInt32 priority;
};


// AutoCompleteResultComparator

class AutoCompleteResultComparator
{
public:
  AutoCompleteResultComparator(nsNavHistory* history) : mHistory(history) {}

  PRBool Equals(const AutoCompleteIntermediateResult& a,
                const AutoCompleteIntermediateResult& b) const {
    // Don't need an equals, this call will be optimized out when it
    // is used by nsQuickSortComparator above
    return PR_FALSE;
  }
  PRBool LessThan(const AutoCompleteIntermediateResult& match1,
                  const AutoCompleteIntermediateResult& match2) const {
    // we actually compare GREATER than here, since we want the array to be in
    // most relevant (highest priority value) first

    // In most cases the priorities will be different and we just use them
    if (match1.priority != match2.priority)
    {
      return match1.priority > match2.priority;
    }
    else
    {
      // secondary sorting gives priority to site names and paths (ending in a /)
      PRBool isPath1 = PR_FALSE, isPath2 = PR_FALSE;
      if (!match1.url.IsEmpty())
        isPath1 = (match1.url.Last() == PRUnichar('/'));
      if (!match2.url.IsEmpty())
        isPath2 = (match2.url.Last() == PRUnichar('/'));

      if (isPath1 && !isPath2) return PR_FALSE; // match1->url is a website/path, match2->url isn't
      if (!isPath1 && isPath2) return PR_TRUE;  // match1->url isn't a website/path, match2->url is

      // find the prefixes so we can sort by the stuff after the prefixes
      PRInt32 prefix1 = mHistory->AutoCompleteGetPrefixLength(match1.url);
      PRInt32 prefix2 = mHistory->AutoCompleteGetPrefixLength(match2.url);

      // Compare non-prefixed urls using the current locale string compare. This will sort
      // things alphabetically (ignoring common prefixes). For example, "http://www.abc.com/"
      // will come before "ftp://ftp.xyz.com"
      PRInt32 ret = 0;
      mHistory->mCollation->CompareString(
          nsICollation::kCollationCaseInSensitive,
          Substring(match1.url, prefix1), Substring(match2.url, prefix2),
          &ret);
      if (ret != 0)
        return ret > 0;

      // sort http://xyz.com before http://www.xyz.com
      return prefix1 > prefix2;
    }
    return PR_FALSE;
  }
protected:
  nsNavHistory* mHistory;
};


// nsNavHistory::InitAutoComplete

nsresult
nsNavHistory::InitAutoComplete()
{
  nsresult rv = CreateAutoCompleteQuery();
  NS_ENSURE_SUCCESS(rv, rv);

  AutoCompletePrefix* ok;

  // These are the prefixes we check for implicitly. Prefixes with a
  // host portion (like "www.") get their second level flag set.
  ok = mAutoCompletePrefixes.AppendElement(AutoCompletePrefix(NS_LITERAL_STRING("http://"), PR_FALSE));
  NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);
  ok = mAutoCompletePrefixes.AppendElement(AutoCompletePrefix(NS_LITERAL_STRING("http://www."), PR_TRUE));
  NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);
  ok = mAutoCompletePrefixes.AppendElement(AutoCompletePrefix(NS_LITERAL_STRING("ftp://"), PR_FALSE));
  NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);
  ok = mAutoCompletePrefixes.AppendElement(AutoCompletePrefix(NS_LITERAL_STRING("ftp://ftp."), PR_TRUE));
  NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);
  ok = mAutoCompletePrefixes.AppendElement(AutoCompletePrefix(NS_LITERAL_STRING("https://"), PR_FALSE));
  NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);
  ok = mAutoCompletePrefixes.AppendElement(AutoCompletePrefix(NS_LITERAL_STRING("https://www."), PR_TRUE));
  NS_ENSURE_TRUE(ok, NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}


// nsNavHistory::CreateAutoCompleteQuery
//
//    The auto complete query we use depends on options, so we have it in
//    a separate function so it can be re-created when the option changes.

nsresult
nsNavHistory::CreateAutoCompleteQuery()
{
  nsCString sql;
  if (mAutoCompleteOnlyTyped) {
    sql = NS_LITERAL_CSTRING(
        "SELECT p.url, p.title, p.visit_count, p.typed, "
          "(SELECT b.fk FROM moz_bookmarks b WHERE b.fk = p.id AND b.type = ?3) "
        "FROM moz_places p "
        "WHERE p.url >= ?1 AND p.url < ?2 "
        "AND p.typed = 1 "
        "ORDER BY p.visit_count DESC "
        "LIMIT ");
  } else {
    sql = NS_LITERAL_CSTRING(
        "SELECT p.url, p.title, p.visit_count, p.typed, "
          "(SELECT b.fk FROM moz_bookmarks b WHERE b.fk = p.id AND b.type = ?3) "
        "FROM moz_places p "
        "WHERE p.url >= ?1 AND p.url < ?2 "
        "AND (p.hidden <> 1 OR p.typed = 1) "
        "ORDER BY p.visit_count DESC "
        "LIMIT ");
  }
  sql.AppendInt(AUTOCOMPLETE_MAX_PER_PREFIX);
  nsresult rv = mDBConn->CreateStatement(sql,
      getter_AddRefs(mDBAutoCompleteQuery));
  NS_ENSURE_SUCCESS(rv, rv);
  rv = mDBAutoCompleteQuery->BindInt32Parameter(2, nsINavBookmarksService::TYPE_BOOKMARK);
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
  nsresult rv;

  NS_ENSURE_ARG_POINTER(aListener);

  nsCOMPtr<nsIAutoCompleteSimpleResult> result =
      do_CreateInstance(NS_AUTOCOMPLETESIMPLERESULT_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  result->SetSearchString(aSearchString);

  // Performance: We can improve performance for refinements of a previous
  // result by filtering the old result with the new string. However, since
  // our results are not a full match of history, we'll need to requery if
  // any of the subresults returned the maximum number of elements (i.e. we
  // didn't load all of them).
  //
  // Timing measurements show that the performance of this is actually very
  // good for specific queries. Thus, the times where we can do the
  // optimization (when there are few results) are exactly the times when
  // we don't have to. As a result, we keep it this much simpler way.
  if (aSearchString.IsEmpty()) {
    rv = AutoCompleteTypedSearch(result);
  } else {
    rv = AutoCompleteFullHistorySearch(aSearchString, result);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  // Determine the result of the search
  PRUint32 count;
  result->GetMatchCount(&count);
  if (count > 0) {
    result->SetSearchResult(nsIAutoCompleteResult::RESULT_SUCCESS);
    result->SetDefaultIndex(0);
  } else {
    result->SetSearchResult(nsIAutoCompleteResult::RESULT_NOMATCH);
    result->SetDefaultIndex(-1);
  }

  aListener->OnSearchResult(this, result);
  return NS_OK;
}


// nsNavHistory::StopSearch

NS_IMETHODIMP
nsNavHistory::StopSearch()
{
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

nsresult nsNavHistory::AutoCompleteTypedSearch(
                                            nsIAutoCompleteSimpleResult* result)
{
  // need to get more than the required minimum number since some will be dupes
  nsCOMPtr<mozIStorageStatement> dbSelectStatement;
  nsCString sql = NS_LITERAL_CSTRING(
      "SELECT url, title "
      "FROM moz_historyvisits v JOIN moz_places h ON v.place_id = h.id "
      "WHERE h.typed = 1 ORDER BY visit_date DESC LIMIT ");
  sql.AppendInt(AUTOCOMPLETE_MAX_PER_TYPED * 3);
  nsresult rv = mDBConn->CreateStatement(sql, getter_AddRefs(dbSelectStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  // prevent duplicates
  nsDataHashtable<nsStringHashKey, PRInt32> urls;
  if (! urls.Init(500))
    return NS_ERROR_OUT_OF_MEMORY;

  PRInt32 dummy;
  PRInt32 count = 0;
  PRBool hasMore = PR_FALSE;
  while (count < AUTOCOMPLETE_MAX_PER_TYPED &&
         NS_SUCCEEDED(dbSelectStatement->ExecuteStep(&hasMore)) && hasMore) {
    nsAutoString entryURL, entryTitle;
    dbSelectStatement->GetString(0, entryURL);
    dbSelectStatement->GetString(1, entryTitle);

    if (! urls.Get(entryURL, &dummy)) {
      // new item
      rv = result->AppendMatch(entryURL, entryTitle);
      NS_ENSURE_SUCCESS(rv, rv);

      urls.Put(entryURL, 1);
      count ++;
    }
  }

  return NS_OK;
}


// nsNavHistory::AutoCompleteFullHistorySearch
//
//    A brute-force search of the entire history. This matches the given input
//    with every possible history entry, and sorts them by likelihood.
//
//    This may be slow for people on slow computers with large histories.

nsresult
nsNavHistory::AutoCompleteFullHistorySearch(const nsAString& aSearchString,
                                            nsIAutoCompleteSimpleResult* aResult)
{
  nsString searchString;
  nsresult rv = NormalizeAutocompleteInput(aSearchString, searchString);
  if (NS_FAILED(rv))
    return NS_OK; // no matches for invalid input

  nsTArray<AutoCompleteIntermediateResult> matches;

  // Try a query using this search string and every prefix. Keep track of
  // known prefixes that the input matches for exclusion later
  PRUint32 i;
  const nsTArray<PRInt32> emptyArray;
  nsTArray<PRInt32> firstLevelMatches;
  nsTArray<PRInt32> secondLevelMatches;
  for (i = 0; i < mAutoCompletePrefixes.Length(); i ++) {
    if (StringBeginsWith(mAutoCompletePrefixes[i].prefix, searchString)) {
      if (mAutoCompletePrefixes[i].secondLevel)
        secondLevelMatches.AppendElement(i);
      else
        firstLevelMatches.AppendElement(i);
    }

    // current string to search for
    nsString cur = mAutoCompletePrefixes[i].prefix + searchString;

    // see if the concatenated string itself matches any prefixes
    nsTArray<PRInt32> curPrefixMatches;
    for (PRUint32 prefix = 0; prefix < mAutoCompletePrefixes.Length(); prefix ++) {
      if (StringBeginsWith(mAutoCompletePrefixes[prefix].prefix, cur))
        curPrefixMatches.AppendElement(prefix);
    }

    // search for the current string, excluding those matching prefixes
    AutoCompleteQueryOnePrefix(cur, curPrefixMatches, 0, &matches);

    // search for each of those matching prefixes, applying the prefix penalty
    for (PRUint32 match = 0; match < curPrefixMatches.Length(); match ++) {
      AutoCompleteQueryOnePrefix(mAutoCompletePrefixes[curPrefixMatches[match]].prefix,
                                 emptyArray, AUTOCOMPLETE_MATCHES_PREFIX_PENALTY,
                                 &matches);
    }
  }

  // Now try searching with no prefix
  if (firstLevelMatches.Length() > 0) {
    // This will get all matches that DON'T match any prefix. For example, if
    // the user types "http://w" we will match "http://westinghouse.com" but
    // not "http://www.something".
    AutoCompleteQueryOnePrefix(searchString,
                               firstLevelMatches, 0, &matches);
  } else if (secondLevelMatches.Length() > 0) {
    // if there are no first level matches (i.e. "http://") then we fall back on
    // second level matches. Here, we assume that a first level match implies
    // a second level match as well, so we only have to check when there are no
    // first level matches.
    AutoCompleteQueryOnePrefix(searchString,
                               secondLevelMatches, 0, &matches);

    // now we try to fill in matches of the prefix. For example, if you type
    // "http://w" we will still match against "http://www." but with a penalty.
    // We only do this for second level prefixes.
    for (PRUint32 match = 0; match < secondLevelMatches.Length(); match ++) {
      AutoCompleteQueryOnePrefix(mAutoCompletePrefixes[secondLevelMatches[match]].prefix,
                                 emptyArray, AUTOCOMPLETE_MATCHES_SCHEME_PENALTY,
                                 &matches);
    }
  } else {
    // Input matched no prefix, try to query for all URLs beinning with this
    // exact input.
    AutoCompleteQueryOnePrefix(searchString, emptyArray,
                               AUTOCOMPLETE_MATCHES_SCHEME_PENALTY, &matches);
  }

  // sort according to priorities
  AutoCompleteResultComparator comparator(this);
  matches.Sort(comparator);

  // fill into result
  nsAutoString zerothEntry;
  if (matches.Length() > 0 &&
      matches[0].visitCount <= AUTOCOMPLETE_MAX_TRUNCATION_VISIT) {
    // Here, we try to make sure that the first match is always a host name
    // we take the previous first match and extract its host name and add it
    // before. If the first item has been visited a lot, don't do that because
    // they probably want to go there instead
    nsCOMPtr<nsIURI> uri;
    rv = NS_NewURI(getter_AddRefs(uri), NS_ConvertUTF16toUTF8(matches[0].url));
    NS_ENSURE_SUCCESS(rv, rv);
    uri->SetPath(NS_LITERAL_CSTRING("/"));

    nsCAutoString spec;
    uri->GetSpec(spec);
    zerothEntry = NS_ConvertUTF8toUTF16(spec);

    if (! zerothEntry.Equals(matches[0].url))
      aResult->AppendMatch(zerothEntry, EmptyString());
    rv = aResult->AppendMatch(matches[0].url, matches[0].title);
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (matches.Length() > 0) {
    // otherwise, just append the first match
    rv = aResult->AppendMatch(matches[0].url, matches[0].title);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  for (i = 1; i < matches.Length(); i ++) {
    // only add ones that are NOT the same as the previous one. It's possible
    // to get duplicates from the queries.
    if (! matches[i].url.Equals(matches[i-1].url) &&
        ! zerothEntry.Equals(matches[i].url)) {
      rv = aResult->AppendMatch(matches[i].url, matches[i].title);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}


// nsNavHistory::AutoCompleteQueryOnePrefix
//
//    The values in aExcludePrefixes are indices into mAutoCompletePrefixes
//    of prefixes to exclude during this query. For example, if I type
//    "ht" this function will be called to match everything starting with
//    "ht" EXCEPT "http://" and "https://".

nsresult
nsNavHistory::AutoCompleteQueryOnePrefix(const nsString& aSearchString,
    const nsTArray<PRInt32>& aExcludePrefixes,
    PRInt32 aPriorityDelta,
    nsTArray<AutoCompleteIntermediateResult>* aResult)
{
  // All URL queries are in UTF-8. Compute the beginning (inclusive) and
  // ending (exclusive) of the range of URLs to include when compared
  // using strcmp (which is what sqlite does).
  nsCAutoString beginQuery = NS_ConvertUTF16toUTF8(aSearchString);
  if (beginQuery.IsEmpty())
    return NS_OK;
  nsCAutoString endQuery = beginQuery;
  unsigned char maxChar[6] = { 0xfd, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf };
  endQuery.Append(NS_REINTERPRET_CAST(const char*, maxChar), 6);

  nsTArray<nsCString> ranges;
  if (aExcludePrefixes.Length() > 0) {
    // we've been requested to include holes in our range, sort these ranges
    ranges.AppendElement(beginQuery);
    for (PRUint32 i = 0; i < aExcludePrefixes.Length(); i ++) {
      nsCAutoString thisPrefix = NS_ConvertUTF16toUTF8(
          mAutoCompletePrefixes[aExcludePrefixes[i]].prefix);
      ranges.AppendElement(thisPrefix);
      thisPrefix.Append(NS_REINTERPRET_CAST(const char*, maxChar), 6);
      ranges.AppendElement(thisPrefix);
    }
    ranges.AppendElement(endQuery);
    ranges.Sort();
  } else {
    // simple range with no holes
    ranges.AppendElement(beginQuery);
    ranges.AppendElement(endQuery);
  }

  NS_ASSERTION(ranges.Length() % 2 == 0, "Ranges should be pairs!");

  // The nested select expands to nonzero when the item is bookmarked.
  // It might be nice to also select hidden bookmarks (unvisited) but that
  // made this statement more complicated and should be an unusual case.
  nsresult rv;
  for (PRUint32 range = 0; range < ranges.Length() - 1; range += 2) {
    mozStorageStatementScoper scoper(mDBAutoCompleteQuery);

    rv = mDBAutoCompleteQuery->BindUTF8StringParameter(0, ranges[range]);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = mDBAutoCompleteQuery->BindUTF8StringParameter(1, ranges[range + 1]);
    NS_ENSURE_SUCCESS(rv, rv);

    PRBool hasMore;
    nsAutoString url, title;
    while (NS_SUCCEEDED(mDBAutoCompleteQuery->ExecuteStep(&hasMore)) && hasMore) {
      mDBAutoCompleteQuery->GetString(0, url);
      mDBAutoCompleteQuery->GetString(1, title);
      PRInt32 visitCount = mDBAutoCompleteQuery->AsInt32(2);
      PRInt32 priority = ComputeAutoCompletePriority(url, visitCount,
          mDBAutoCompleteQuery->AsInt32(3) > 0,
          mDBAutoCompleteQuery->AsInt32(4) > 0) + aPriorityDelta;
      aResult->AppendElement(AutoCompleteIntermediateResult(
          url, title, visitCount, priority));
    }
  }
  return NS_OK;
}


// nsNavHistory::AutoCompleteGetPrefixLength

PRInt32
nsNavHistory::AutoCompleteGetPrefixLength(const nsString& aSpec)
{
  for (PRUint32 i = 0; i < mAutoCompletePrefixes.Length(); ++i) {
    if (StringBeginsWith(aSpec, mAutoCompletePrefixes[i].prefix))
      return mAutoCompletePrefixes[i].prefix.Length();
  }
  return 0; // no prefix
}


// ComputeAutoCompletePriority
//
//    Favor websites and webpaths more than webpages by boosting
//    their visit counts.  This assumes that URLs have been normalized,
//    appending a trailing '/'.
//
//    We use addition to boost the visit count rather than multiplication
//    since we want URLs with large visit counts to remain pretty much
//    in raw visit count order - we assume the user has visited these urls
//    often for a reason and there shouldn't be a problem with putting them
//    high in the autocomplete list regardless of whether they are sites/
//    paths or pages.  However for URLs visited only a few times, sites
//    & paths should be presented before pages since they are generally
//    more likely to be visited again.

PRInt32
ComputeAutoCompletePriority(const nsAString& aUrl, PRInt32 aVisitCount,
                            PRBool aWasTyped, PRBool aIsBookmarked)
{
  PRInt32 aPriority = aVisitCount;

  if (!aUrl.IsEmpty()) {
    // url is a site/path if it has a trailing slash
    if (aUrl.Last() == PRUnichar('/'))
      aPriority += AUTOCOMPLETE_NONPAGE_VISIT_COUNT_BOOST;
  }

  if (aWasTyped)
    aPriority += AUTOCOMPLETE_TYPED_BOOST;
  if (aIsBookmarked)
    aPriority += AUTOCOMPLETE_BOOKMARKED_BOOST;

  return aPriority;
}


// NormalizeAutocompleteInput

nsresult NormalizeAutocompleteInput(const nsAString& aInput,
                                    nsString& aOutput)
{
  nsresult rv;

  if (aInput.IsEmpty()) {
    aOutput.Truncate();
    return NS_OK;
  }
  nsCAutoString input = NS_ConvertUTF16toUTF8(aInput);

  nsCOMPtr<nsIURI> uri;
  rv = NS_NewURI(getter_AddRefs(uri), input);
  PRBool isSchemeAdded = PR_FALSE;
  if (NS_FAILED(rv)) {
    // it may have failed because there is no scheme, prepend one
    isSchemeAdded = PR_TRUE;
    input = NS_LITERAL_CSTRING("http://") + input;

    rv = NS_NewURI(getter_AddRefs(uri), input);
    if (NS_FAILED(rv))
      return rv; // still not valid, can't autocomplete this URL
  }

  nsCAutoString spec;
  rv = uri->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);
  if (spec.IsEmpty())
    return NS_OK; // should never happen but we assume it's not empty below, so check

  aOutput = NS_ConvertUTF8toUTF16(spec);

  // trim the "http://" scheme if we added it
  if (isSchemeAdded) {
    NS_ASSERTION(aOutput.Length() > 7, "String impossibly short");
    aOutput = Substring(aOutput, 7);
  }

  // it may have appended a slash, get rid of it
  // example: input was "http://www.mozil" the URI spec will be
  // "http://www.mozil/" which is obviously wrong to complete against.
  // However, it won't always append a slash, for example, for the input
  // "http://www.mozilla.org/supp"
  if (input[input.Length() - 1] != '/' && aOutput[aOutput.Length() - 1] == '/')
    aOutput.Truncate(aOutput.Length() - 1);

  return NS_OK;
}
