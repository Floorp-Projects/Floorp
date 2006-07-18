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

#include "nsNavHistory.h"

#include "mozIStorageService.h"
#include "mozIStorageConnection.h"
#include "mozIStorageValueArray.h"
#include "mozIStorageStatement.h"
#include "mozIStorageFunction.h"
#include "mozStorageCID.h"
#include "mozStorageHelper.h"

#define NS_AUTOCOMPLETESIMPLERESULT_CONTRACTID \
  "@mozilla.org/autocomplete/simple-result;1"


PRInt32 ComputeAutoCompletePriority(const nsAString& aUrl, PRInt32 aVisitCount,
                                    PRBool aWasTyped);

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
  nsString url;
  nsString title;
  PRUint32 priority;
};
class AutoCompleteIntermediateResultSet
{
public:
  AutoCompleteIntermediateResultSet()
  {
  }
  ~AutoCompleteIntermediateResultSet()
  {
    for (PRInt32 i = 0; i < mArray.Count(); i ++) {
      AutoCompleteIntermediateResult* cur = NS_STATIC_CAST(
                                    AutoCompleteIntermediateResult*, mArray[i]);
      delete cur;
    }
  }
  PRBool Add(const nsString& aURL, const nsString& aTitle, PRInt32 aPriority)
  {
    AutoCompleteIntermediateResult* cur = new AutoCompleteIntermediateResult;
    if (! cur)
      return PR_FALSE;
    cur->url = aURL;
    cur->title = aTitle;
    cur->priority = aPriority;
    mArray.AppendElement(cur);
    return PR_TRUE;
  }
  void Sort(nsNavHistory* navHistory)
  {
    mArray.Sort(nsNavHistory::AutoCompleteSortComparison, navHistory);
  }
  nsresult FillResult(nsIAutoCompleteSimpleResult* aResult)
  {
    for (PRInt32 i = 0; i < mArray.Count(); i ++)
    {
      AutoCompleteIntermediateResult* cur = NS_STATIC_CAST(
                                    AutoCompleteIntermediateResult*, mArray[i]);
      nsresult rv = aResult->AppendMatch(cur->url, cur->title);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    return NS_OK;
  }
protected:
  nsVoidArray mArray;
};

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

  PRBool usePrevious = (aPreviousResult != nsnull);

  // throw away previous results if the search string is empty after it has had
  // the prefixes removed: if the user types "http://" throw away the search
  // for "http:/" and start fresh with the new query.
  if (usePrevious) {
    nsAutoString cut(aSearchString);
    AutoCompleteCutPrefix(&cut, nsnull);
    if (cut.IsEmpty())
      usePrevious = PR_FALSE;
  }

  // throw away previous results if the new string doesn't begin with the
  // previous search string
  if (usePrevious) {
    nsAutoString previousSearch;
    aPreviousResult->GetSearchString(previousSearch);
    if (previousSearch.Length() > aSearchString.Length()  ||
        !Substring(aSearchString, 0, previousSearch.Length()).Equals(previousSearch)) {
      usePrevious = PR_FALSE;
    }
  }

  if (aSearchString.IsEmpty()) {
    rv = AutoCompleteTypedSearch(result);
  } else if (usePrevious) {
    rv = AutoCompleteRefineHistorySearch(aSearchString, aPreviousResult, result);
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
  // note: need to test against hidden = 1 since the column can also be null
  // (meaning not hidden)
  nsCOMPtr<mozIStorageStatement> dbSelectStatement;
  nsresult rv = mDBConn->CreateStatement(
      NS_LITERAL_CSTRING("SELECT url,title FROM moz_history WHERE typed = 1 AND hidden <> 1 ORDER BY visit_count DESC LIMIT 1000"),
      getter_AddRefs(dbSelectStatement));
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool hasMore = PR_FALSE;
  while (NS_SUCCEEDED(dbSelectStatement->ExecuteStep(&hasMore)) && hasMore) {
    nsAutoString entryURL, entryTitle;
    dbSelectStatement->GetString(0, entryURL);
    dbSelectStatement->GetString(1, entryTitle);

    rv = result->AppendMatch(entryURL, entryTitle);
    NS_ENSURE_SUCCESS(rv, rv);
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
                                            nsIAutoCompleteSimpleResult* result)
{
  // figure out whether any known prefixes are present in the search string
  // so we know not to ignore them when comparing results later
  AutoCompleteExclude exclude;
  AutoCompleteGetExcludeInfo(aSearchString, &exclude);

  mozStorageStatementScoper scoper(mDBFullAutoComplete);
  nsresult rv = mDBFullAutoComplete->BindInt32Parameter(0, mAutoCompleteMaxCount);
  NS_ENSURE_SUCCESS(rv, rv);

  // load the intermediate result so it can be sorted
  AutoCompleteIntermediateResultSet resultSet;
  PRBool hasMore = PR_FALSE;
  while (NS_SUCCEEDED(mDBFullAutoComplete->ExecuteStep(&hasMore)) && hasMore) {

    PRBool typed = mDBFullAutoComplete->AsInt32(kAutoCompleteIndex_Typed);
    if (!typed) {
      if (mAutoCompleteOnlyTyped)
        continue;
    }

    nsAutoString entryurl;
    mDBFullAutoComplete->GetString(kAutoCompleteIndex_URL, entryurl);
    if (AutoCompleteCompare(entryurl, aSearchString, exclude)) {
      nsAutoString entrytitle;
      rv = mDBFullAutoComplete->GetString(kAutoCompleteIndex_Title, entrytitle);
      NS_ENSURE_SUCCESS(rv, rv);
      PRInt32 priority = ComputeAutoCompletePriority(entryurl,
                    mDBFullAutoComplete->AsInt32(kAutoCompleteIndex_VisitCount),
                    typed);

      if (! resultSet.Add(entryurl, entrytitle, priority))
        return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  // sort and populate the final result
  resultSet.Sort(this);
  return resultSet.FillResult(result);
}


// nsNavHistory::AutoCompleteRefineHistorySearch
//
//    Called when a previous autocomplete result exists and we are adding to
//    the query (making it more specific). The list is already nicely sorted,
//    so we can just remove all the old elements that DON'T match the new query.

nsresult
nsNavHistory::AutoCompleteRefineHistorySearch(
                                  const nsAString& aSearchString,
                                  nsIAutoCompleteResult* aPreviousResult,
                                  nsIAutoCompleteSimpleResult* aNewResult)
{
  // figure out whether any known prefixes are present in the search string
  // so we know not to ignore them when comparing results later
  AutoCompleteExclude exclude;
  AutoCompleteGetExcludeInfo(aSearchString, &exclude);

  PRUint32 matchCount;
  aPreviousResult->GetMatchCount(&matchCount);

  for (PRUint32 i = 0; i < matchCount; i ++) {
    nsAutoString cururl;
    nsresult rv = aPreviousResult->GetValueAt(i, cururl);
    NS_ENSURE_SUCCESS(rv, rv);
    if (AutoCompleteCompare(cururl, aSearchString, exclude)) {
      nsAutoString curcomment;
      rv = aPreviousResult->GetCommentAt(i, curcomment);
      NS_ENSURE_SUCCESS(rv, rv);
      rv = aNewResult->AppendMatch(cururl, curcomment);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }
  return NS_OK;
}


// nsNavHistory::AutoCompleteGetExcludeInfo
//
//    If aURL begins with a protocol or domain prefix from our lists, then mark
//    their index in an AutocompleteExclude struct.

void
nsNavHistory::AutoCompleteGetExcludeInfo(const nsAString& aURL,
                                         AutoCompleteExclude* aExclude)
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
      index += string->Length();
      break;
    }
  }

  aExclude->postPrefixOffset = index;
}


// nsNavHistory::AutoCompleteCutPrefix
//
//    Cut any protocol and domain prefixes from aURL, except for those which
//    are specified in aExclude.
//
//    aExclude can be NULL to cut all possible prefixes
//
//    This comparison is case-sensitive.  Therefore, it assumes that aUserURL
//    is a potential URL whose host name is in all lower case.
//
//    The aExclude identifies which prefixes were present in the user's typed
//    entry, which we DON'T want to cut so that it can match the user input.
//    For example. Typed "xyz.com" matches "xyz.com" AND "http://www.xyz.com"
//    but "www.xyz.com" still matches "www.xyz.com".

void
nsNavHistory::AutoCompleteCutPrefix(nsString* aURL,
                                    const AutoCompleteExclude* aExclude)
{
  PRInt32 idx = 0;
  PRInt32 i;
  for (i = 0; i < mIgnoreSchemes.Count(); ++i) {
    if (aExclude && i == aExclude->schemePrefix)
      continue;
    nsString* string = mIgnoreSchemes.StringAt(i);
    if (Substring(*aURL, 0, string->Length()).Equals(*string)) {
      idx = string->Length();
      break;
    }
  }

  if (idx > 0)
    aURL->Cut(0, idx);

  idx = 0;
  for (i = 0; i < mIgnoreHostnames.Count(); ++i) {
    if (aExclude && i == aExclude->hostnamePrefix)
      continue;
    nsString* string = mIgnoreHostnames.StringAt(i);
    if (Substring(*aURL, 0, string->Length()).Equals(*string)) {
      idx = string->Length();
      break;
    }
  }

  if (idx > 0)
    aURL->Cut(0, idx);
}



// nsNavHistory::AutoCompleteCompare
//
//    Determines if a given URL from the history matches the user input.
//    See AutoCompleteCutPrefix for the meaning of aExclude.

PRBool
nsNavHistory::AutoCompleteCompare(const nsAString& aHistoryURL,
                                  const nsAString& aUserURL,
                                  const AutoCompleteExclude& aExclude)
{
  nsAutoString cutHistoryURL(aHistoryURL);
  AutoCompleteCutPrefix(&cutHistoryURL, &aExclude);
  return StringBeginsWith(cutHistoryURL, aUserURL);
}


// nsGlobalHistory::AutoCompleteSortComparison
//
//    The design and reasoning behind the following autocomplete sort
//    implementation is documented in bug 78270. In most cases, the precomputed
//    priorities (by ComputeAutoCompletePriority) are used without further
//    computation.

PRInt32 PR_CALLBACK // static
nsNavHistory::AutoCompleteSortComparison(const void* match1Void,
                                         const void* match2Void,
                                         void *navHistoryVoid)
{
  // get our class pointer (this is a static function)
  nsNavHistory* navHistory = NS_STATIC_CAST(nsNavHistory*, navHistoryVoid);

  const AutoCompleteIntermediateResult* match1 = NS_STATIC_CAST(
                             const AutoCompleteIntermediateResult*, match1Void);
  const AutoCompleteIntermediateResult* match2 = NS_STATIC_CAST(
                             const AutoCompleteIntermediateResult*, match2Void);

  // In most cases the priorities will be different and we just use them
  if (match1->priority != match2->priority)
  {
    return match2->priority - match1->priority;
  }
  else
  {
    // secondary sorting gives priority to site names and paths (ending in a /)
    PRBool isPath1 = PR_FALSE, isPath2 = PR_FALSE;
    if (!match1->url.IsEmpty())
      isPath1 = (match1->url.Last() == PRUnichar('/'));
    if (!match2->url.IsEmpty())
      isPath2 = (match2->url.Last() == PRUnichar('/'));

    if (isPath1 && !isPath2) return -1; // match1->url is a website/path, match2->url isn't
    if (!isPath1 && isPath2) return  1; // match1->url isn't a website/path, match2->url is

    // find the prefixes so we can sort by the stuff after the prefixes
    AutoCompleteExclude prefix1, prefix2;
    navHistory->AutoCompleteGetExcludeInfo(match1->url, &prefix1);
    navHistory->AutoCompleteGetExcludeInfo(match2->url, &prefix2);

    // compare non-prefixed urls
    PRInt32 ret = Compare(
      Substring(match1->url, prefix1.postPrefixOffset, match1->url.Length()),
      Substring(match2->url, prefix2.postPrefixOffset, match2->url.Length()));
    if (ret != 0)
      return ret;

    // sort http://xyz.com before http://www.xyz.com
    return prefix1.postPrefixOffset - prefix2.postPrefixOffset;
  }
  return 0;
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
                            PRBool aWasTyped)
{
  PRInt32 aPriority = aVisitCount;

  if (!aUrl.IsEmpty()) {
    // url is a site/path if it has a trailing slash
    if (aUrl.Last() == PRUnichar('/'))
      aPriority += AUTOCOMPLETE_NONPAGE_VISIT_COUNT_BOOST;
  }

  if (aWasTyped)
    aPriority += AUTOCOMPLETE_NONPAGE_VISIT_COUNT_BOOST;

  return aPriority;
}
