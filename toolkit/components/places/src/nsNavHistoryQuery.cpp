//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Places code
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brett Wilson <brettw@gmail.com> (original author)
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
#include "nsEscape.h"
#include "nsCOMArray.h"
#include "nsVoidArray.h"
#include "prprf.h"

static void FreeTokenList(nsVoidArray* aTokens);
static nsresult TokenizeQueryString(const nsACString& aQuery,
                                    nsVoidArray* aTokens);
static nsresult ParseQueryBooleanString(const nsCString& aString,
                                        PRBool* aValue);
static nsresult ParseQueryTimeString(const nsCString& aString,
                                     PRTime* aTime);

// components of a query string
#define QUERYKEY_BEGIN_TIME "beginTime"
#define QUERYKEY_BEGIN_TIME_REFERENCE "beginTimeRef"
#define QUERYKEY_END_TIME "endTime"
#define QUERYKEY_END_TIME_REFERENCE "endTimeRef"
#define QUERYKEY_SEARCH_TERMS "terms"
#define QUERYKEY_ONLY_BOOKMARKED "onlyBookmarked"
#define QUERYKEY_DOMAIN_IS_HOST "domainIsHost"
#define QUERYKEY_DOMAIN "domain"
#define QUERYKEY_FOLDERS "folders"
#define QUERYKEY_SEPARATOR "OR"
#define QUERYKEY_GROUP "group"
#define QUERYKEY_SORT "sort"
#define QUERYKEY_RESULT_TYPE "type"
#define QUERYKEY_EXPAND_PLACES "expandplaces"

inline void AppendAmpersandIfNonempty(nsACString& aString)
{
  if (! aString.IsEmpty())
    aString.Append('&');
}
inline void AppendInt32(nsACString& str, PRInt32 i)
{
  nsCAutoString tmp;
  tmp.AppendInt(i);
  str.Append(tmp);
}
inline void AppendInt64(nsACString& str, PRInt64 i)
{
  nsCString tmp;
  tmp.AppendInt(i);
  str.Append(tmp);
}

// nsNavHistory::QueryStringToQueries

NS_IMETHODIMP
nsNavHistory::QueryStringToQueries(const nsACString& aQueryString,
                                   nsINavHistoryQuery*** aQueries,
                                   PRUint32* aResultCount,
                                   nsINavHistoryQueryOptions** aOptions)
{
  nsresult rv;
  *aQueries = nsnull;
  *aResultCount = 0;
  *aOptions = nsnull;

  nsRefPtr<nsNavHistoryQueryOptions> options(new nsNavHistoryQueryOptions());
  if (! options)
    return NS_ERROR_OUT_OF_MEMORY;

  nsVoidArray tokens;
  rv = TokenizeQueryString(aQueryString, &tokens);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMArray<nsINavHistoryQuery> queries;
  rv = TokensToQueries(tokens, &queries, options);
  FreeTokenList(&tokens);
  NS_ENSURE_SUCCESS(rv, rv);

  NS_STATIC_CAST(nsRefPtr<nsINavHistoryQueryOptions>, options).swap(*aOptions);
  *aResultCount = queries.Count();
  if (queries.Count() == 0) {
    // need to special-case 0 queries since we don't allocate an array
    *aQueries = nsnull;
    return NS_OK;
  }

  // convert COM array to raw
  *aQueries = NS_STATIC_CAST(nsINavHistoryQuery**,
                nsMemory::Alloc(sizeof(nsINavHistoryQuery*) * queries.Count()));
  if (! *aQueries)
    return NS_ERROR_OUT_OF_MEMORY;
  for (PRInt32 i = 0; i < queries.Count(); i ++) {
    (*aQueries)[i] = queries[i];
    NS_ADDREF((*aQueries)[i]);
  }

  NS_ADDREF(*aOptions = options);

  return NS_OK;
}


// QueriesToQueryString

NS_IMETHODIMP
nsNavHistory::QueriesToQueryString(nsINavHistoryQuery **aQueries,
                                   PRUint32 aQueryCount,
                                   nsINavHistoryQueryOptions* aOptions,
                                   nsACString& aQueryString)
{
  nsresult rv;

  nsCOMPtr<nsNavHistoryQueryOptions> options = do_QueryInterface(aOptions);
  NS_ENSURE_TRUE(options, NS_ERROR_INVALID_ARG);

  aQueryString.AssignLiteral("place:");
  for (PRUint32 queryIndex = 0; queryIndex < aQueryCount;  queryIndex ++) {
    nsINavHistoryQuery* query = aQueries[queryIndex];
    if (queryIndex > 0) {
      AppendAmpersandIfNonempty(aQueryString);
      aQueryString += NS_LITERAL_CSTRING(QUERYKEY_SEPARATOR);
    }

    PRBool hasIt;

    // begin time
    rv = query->GetHasBeginTime(&hasIt);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasIt) {
      PRTime beginTime;
      rv = query->GetBeginTime(&beginTime);
      NS_ENSURE_SUCCESS(rv, rv);

      AppendAmpersandIfNonempty(aQueryString);
      aQueryString += NS_LITERAL_CSTRING(QUERYKEY_BEGIN_TIME "=");
      AppendInt64(aQueryString, beginTime);

      // reference
      PRUint32 reference;
      query->GetBeginTimeReference(&reference);
      if (reference != nsINavHistoryQuery::TIME_RELATIVE_EPOCH) {
        AppendAmpersandIfNonempty(aQueryString);
        aQueryString += NS_LITERAL_CSTRING(QUERYKEY_BEGIN_TIME_REFERENCE "=");
        AppendInt32(aQueryString, reference);
      }
    }

    // end time
    rv = query->GetHasEndTime(&hasIt);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasIt) {
      PRTime endTime;
      rv = query->GetEndTime(&endTime);
      NS_ENSURE_SUCCESS(rv, rv);

      AppendAmpersandIfNonempty(aQueryString);
      aQueryString += NS_LITERAL_CSTRING(QUERYKEY_END_TIME "=");
      AppendInt64(aQueryString, endTime);

      // reference
      PRUint32 reference;
      query->GetEndTimeReference(&reference);
      if (reference != nsINavHistoryQuery::TIME_RELATIVE_EPOCH) {
        AppendAmpersandIfNonempty(aQueryString);
        aQueryString += NS_LITERAL_CSTRING(QUERYKEY_END_TIME_REFERENCE "=");
        AppendInt32(aQueryString, reference);
      }
    }

    // search terms
    rv = query->GetHasSearchTerms(&hasIt);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasIt) {
      nsAutoString searchTerms;
      rv = query->GetSearchTerms(searchTerms);
      NS_ENSURE_SUCCESS(rv, rv);
      nsCString escapedTerms;
      if (! NS_Escape(NS_ConvertUTF16toUTF8(searchTerms), escapedTerms,
                      url_XAlphas))
        return NS_ERROR_OUT_OF_MEMORY;

      AppendAmpersandIfNonempty(aQueryString);
      aQueryString += NS_LITERAL_CSTRING(QUERYKEY_SEARCH_TERMS);
      aQueryString += NS_LITERAL_CSTRING("=");
      aQueryString += escapedTerms;
    }

    // only bookmarked
    PRBool onlyBookmarked;
    rv = query->GetOnlyBookmarked(&onlyBookmarked);
    NS_ENSURE_SUCCESS(rv, rv);
    if (onlyBookmarked) {
      AppendAmpersandIfNonempty(aQueryString);
      aQueryString += NS_LITERAL_CSTRING(QUERYKEY_ONLY_BOOKMARKED);
      aQueryString += NS_LITERAL_CSTRING("=1");
    }

    // domain is host
    PRBool domainIsHost;
    rv = query->GetDomainIsHost(&domainIsHost);
    NS_ENSURE_SUCCESS(rv, rv);
    if (domainIsHost) {
      AppendAmpersandIfNonempty(aQueryString);
      aQueryString += NS_LITERAL_CSTRING(QUERYKEY_DOMAIN_IS_HOST);
      aQueryString += NS_LITERAL_CSTRING("=1");
    }

    // domain
    rv = query->GetHasDomain(&hasIt);
    NS_ENSURE_SUCCESS(rv, rv);
    if (hasIt) {
      nsAutoString domain;
      rv = query->GetDomain(domain);
      NS_ENSURE_SUCCESS(rv, rv);
      nsCString escapedDomain;
      if (! NS_Escape(NS_ConvertUTF16toUTF8(domain), escapedDomain,
                      url_XAlphas))
        return NS_ERROR_OUT_OF_MEMORY;

      AppendAmpersandIfNonempty(aQueryString);
      aQueryString += NS_LITERAL_CSTRING(QUERYKEY_DOMAIN);
      aQueryString.Append('=');
      aQueryString += escapedDomain;
    }

    // folders
    PRInt64 *folders = nsnull;
    PRUint32 folderCount = 0;
    rv = query->GetFolders(&folderCount, &folders);
    for (PRUint32 i = 0; i < folderCount; ++i) {
      AppendAmpersandIfNonempty(aQueryString);
      aQueryString += NS_LITERAL_CSTRING(QUERYKEY_FOLDERS);
      aQueryString.Append('=');
      AppendInt64(aQueryString, folders[0]);
    }
  }

  // grouping
  PRUint32 groupCount;
  const PRInt32* groups = options->GroupingMode(&groupCount);
  for (PRUint32 i = 0; i < groupCount; i ++) {
    AppendAmpersandIfNonempty(aQueryString);
    aQueryString += NS_LITERAL_CSTRING(QUERYKEY_GROUP);
    aQueryString.Append('=');
    AppendInt32(aQueryString, groups[i]);
  }

  // sorting
  if (options->SortingMode() != nsINavHistoryQueryOptions::SORT_BY_NONE) {
    AppendAmpersandIfNonempty(aQueryString);
    aQueryString += NS_LITERAL_CSTRING(QUERYKEY_SORT);
    aQueryString.Append('=');
    AppendInt32(aQueryString, options->SortingMode());
  }

  // result type
  if (options->ResultType() != nsINavHistoryQueryOptions::RESULT_TYPE_URL) {
    AppendAmpersandIfNonempty(aQueryString);
    aQueryString += NS_LITERAL_CSTRING(QUERYKEY_RESULT_TYPE);
    aQueryString.Append('=');
    AppendInt32(aQueryString, options->ResultType());
  }

  return NS_OK;
}


class QueryKeyValuePair
{
public:

  // QueryKeyValuePair
  //
  //                  01234567890
  //    input : qwerty&key=value&qwerty
  //                  ^   ^     ^
  //          aKeyBegin   |     aPastEnd (may point to NULL terminator)
  //                      aEquals
  //
  //    Special case: if aKeyBegin == aEquals, then there is only one string
  //    and no equal sign, so we treat the entire thing as a key with no value

  QueryKeyValuePair(const nsCSubstring& aSource, PRInt32 aKeyBegin,
                    PRInt32 aEquals, PRInt32 aPastEnd)
  {
    if (aEquals == aKeyBegin)
      aEquals = aPastEnd;
    key = Substring(aSource, aKeyBegin, aEquals - aKeyBegin);
    if (aPastEnd - aEquals > 0)
      value = Substring(aSource, aEquals + 1, aPastEnd - aEquals - 1);
  }
  nsCString key;
  nsCString value;
};


// FreeTokenList

void
FreeTokenList(nsVoidArray* aTokens)
{
  for (PRInt32 i = 0; i < aTokens->Count(); i ++) {
    QueryKeyValuePair* kvp = NS_STATIC_CAST(QueryKeyValuePair*, (*aTokens)[i]);
    delete kvp;
  }
  aTokens->Clear();
}


// TokenizeQueryString

nsresult
TokenizeQueryString(const nsACString& aQuery, nsVoidArray* aTokens)
{
  // Strip of the "place:" prefix
  const nsCSubstring &query = Substring(aQuery, strlen("place:"));

  PRInt32 keyFirstIndex = 0;
  PRInt32 equalsIndex = 0;
  for (PRUint32 i = 0; i < query.Length(); i ++) {
    if (query[i] == '&') {
      // new clause, save last one
      if (i - keyFirstIndex > 1) {
        QueryKeyValuePair* kvp = new QueryKeyValuePair(query, keyFirstIndex,
                                                       equalsIndex, i);
        if (! kvp) {
          FreeTokenList(aTokens);
          return NS_ERROR_OUT_OF_MEMORY;
        }
        aTokens->AppendElement(kvp);
      }
      keyFirstIndex = equalsIndex = i + 1;
    } else if (query[i] == '=') {
      equalsIndex = i;
    }
  }

  // handle last pair, if any
  if (query.Length() - keyFirstIndex > 1) {
    QueryKeyValuePair* kvp = new QueryKeyValuePair(query, keyFirstIndex,
                                                   equalsIndex,
                                                   query.Length());
    if (! kvp) {
      FreeTokenList(aTokens);
      return NS_ERROR_OUT_OF_MEMORY;
    }
    aTokens->AppendElement(kvp);
  }
  return NS_OK;
}

// hack because there's no convenient templatized array, caller should check
// mItems for NULL in case of memory allocation error
class GroupingQueryList
{
public:
  PRInt32* mItems;
  static const PRInt32 sMaxGroups;
  PRInt32 mGroupCount;
  GroupingQueryList() : mGroupCount(0)
  {
    mItems = new PRInt32[sMaxGroups];
  }
  ~GroupingQueryList()
  {
    if (mItems)
      delete[] mItems;
  }
  void Add(PRInt32 g)
  {
    if (mGroupCount < sMaxGroups) {
      mItems[mGroupCount] = g;
      mGroupCount ++;
    } else {
      NS_WARNING("Unreasonable number of groupings, ignoring");
    }
  }
};

const PRInt32 GroupingQueryList::sMaxGroups = 32;

// TokensToQueries

nsresult
nsNavHistory::TokensToQueries(const nsVoidArray& aTokens,
                              nsCOMArray<nsINavHistoryQuery>* aQueries,
                              nsNavHistoryQueryOptions* aOptions)
{
  nsresult rv;
  if (aTokens.Count() == 0)
    return NS_OK; // nothing to do

  GroupingQueryList groups;
  if (! groups.mItems)
    return NS_ERROR_OUT_OF_MEMORY;

  nsTArray<PRInt64> folders;
  nsCOMPtr<nsINavHistoryQuery> query(new nsNavHistoryQuery());
  if (! query)
    return NS_ERROR_OUT_OF_MEMORY;
  if (! aQueries->AppendObject(query))
    return NS_ERROR_OUT_OF_MEMORY;
  for (PRInt32 i = 0; i < aTokens.Count(); i ++) {
    QueryKeyValuePair* kvp = NS_STATIC_CAST(QueryKeyValuePair*, aTokens[i]);
    if (kvp->key.EqualsLiteral(QUERYKEY_BEGIN_TIME)) {

      // begin time
      PRTime time;
      rv = ParseQueryTimeString(kvp->value, &time);
      if (NS_SUCCEEDED(rv)) {
        rv = query->SetBeginTime(time);
        NS_ENSURE_SUCCESS(rv, rv);
      } else {
        NS_WARNING("Begin time format in query is invalid, ignoring");
      }

    } else if (kvp->key.EqualsLiteral(QUERYKEY_BEGIN_TIME_REFERENCE)) {

      // begin time reference
      PRUint32 ref = kvp->value.ToInteger((PRInt32*)&rv);
      if (NS_SUCCEEDED(rv)) {
        if (NS_FAILED(query->SetBeginTimeReference(ref))) {
          NS_WARNING("Begin time reference is out of range, ignoring");
        }
      } else {
        NS_WARNING("Begin time reference is invalid number, ignoring");
      }

    } else if (kvp->key.EqualsLiteral(QUERYKEY_END_TIME)) {

      // end time
      PRTime time;
      rv = ParseQueryTimeString(kvp->value, &time);
      if (NS_SUCCEEDED(rv)) {
        rv = query->SetEndTime(time);
        NS_ENSURE_SUCCESS(rv, rv);
      } else {
        NS_WARNING("Begin time format in query is invalid, ignoring");
      }

    } else if (kvp->key.EqualsLiteral(QUERYKEY_END_TIME_REFERENCE)) {

      // end time reference
      PRUint32 ref = kvp->value.ToInteger((PRInt32*)&rv);
      if (NS_SUCCEEDED(rv)) {
        if (NS_FAILED(query->SetEndTimeReference(ref))) {
          NS_WARNING("End time reference is out of range, ignoring");
        }
      } else {
        NS_WARNING("End time reference is invalid number, ignoring");
      }

    } else if (kvp->key.EqualsLiteral(QUERYKEY_SEARCH_TERMS)) {

      // search terms
      nsCString unescapedTerms = kvp->value;
      NS_UnescapeURL(unescapedTerms); // modifies input
      rv = query->SetSearchTerms(NS_ConvertUTF8toUTF16(unescapedTerms));
      NS_ENSURE_SUCCESS(rv, rv);

    } else if (kvp->key.EqualsLiteral(QUERYKEY_ONLY_BOOKMARKED)) {

      // onlyBookmarked flag
      PRBool onlyBookmarked;
      rv = ParseQueryBooleanString(kvp->value, &onlyBookmarked);
      if (NS_SUCCEEDED(rv)) {
        rv = query->SetOnlyBookmarked(onlyBookmarked);
        NS_ENSURE_SUCCESS(rv, rv);
      } else {
        NS_WARNING("onlyBookmarked value in query is invalid, ignoring");
      }

    } else if (kvp->key.EqualsLiteral(QUERYKEY_DOMAIN_IS_HOST)) {

      // domainIsHost flag
      PRBool domainIsHost;
      rv = ParseQueryBooleanString(kvp->value, &domainIsHost);
      if (NS_SUCCEEDED(rv)) {
        rv = query->SetDomainIsHost(domainIsHost);
        NS_ENSURE_SUCCESS(rv, rv);
      } else {
        NS_WARNING("domainIsHost value in query is invalid, ignoring");
      }

    } else if (kvp->key.EqualsLiteral(QUERYKEY_DOMAIN)) {

      // domain string
      nsCAutoString unescapedDomain(kvp->value);
      NS_UnescapeURL(unescapedDomain); // modifies input
      rv = query->SetDomain(NS_ConvertUTF8toUTF16(unescapedDomain));
      NS_ENSURE_SUCCESS(rv, rv);

    } else if (kvp->key.EqualsLiteral(QUERYKEY_FOLDERS)) {

      // folders
      PRInt64 folder;
      rv = ParseQueryTimeString(kvp->value, &folder);
      if (NS_SUCCEEDED(rv)) {
        NS_ENSURE_TRUE(folders.AppendElement(folder), NS_ERROR_OUT_OF_MEMORY);
      } else {
        NS_WARNING("folders value in query is invalid, ignoring");
      }

    } else if (kvp->key.EqualsLiteral(QUERYKEY_SEPARATOR)) {

      if (folders.Length() != 0) {
        query->SetFolders(folders.Elements(), folders.Length());
        folders.Clear();
      }

      // new query component
      query = new nsNavHistoryQuery();
      if (! query)
        return NS_ERROR_OUT_OF_MEMORY;
      if (! aQueries->AppendObject(query))
        return NS_ERROR_OUT_OF_MEMORY;

    } else if (kvp->key.EqualsLiteral(QUERYKEY_GROUP)) {

      // grouping
      PRUint32 grouping = kvp->value.ToInteger((PRInt32*)&rv);
      if (NS_SUCCEEDED(rv)) {
        groups.Add(grouping);
      } else {
        NS_WARNING("Bad number for grouping in query");
      }

    } else if (kvp->key.EqualsLiteral(QUERYKEY_SORT)) {

      // sorting mode
      PRUint32 sorting = kvp->value.ToInteger((PRInt32*)&rv);
      if (NS_SUCCEEDED(rv)) {
        aOptions->SetSortingMode(sorting);
      } else {
        NS_WARNING("Invalid number for sorting mode");
      }

    } else if (kvp->key.EqualsLiteral(QUERYKEY_RESULT_TYPE)) {

      // result type
      PRInt32 resultType = kvp->value.ToInteger((PRInt32*)&rv);
      if (NS_SUCCEEDED(rv)) {
        aOptions->SetResultType(resultType);
      } else {
        NS_WARNING("Invalid valie for result type in query");
      }

    } else if (kvp->key.EqualsLiteral(QUERYKEY_EXPAND_PLACES)) {

      // expand places
      PRBool expand;
      rv = ParseQueryBooleanString(kvp->value, &expand);
      if (NS_SUCCEEDED(rv)) {
        aOptions->SetExpandPlaces(expand);
      } else {
        NS_WARNING("Invalid value for expandplaces");
      }

    } else {

      // unknown key
      aQueries->Clear();
      return NS_ERROR_INVALID_ARG;
    }
  }

  if (folders.Length() != 0) {
    query->SetFolders(folders.Elements(), folders.Length());
  }

  aOptions->SetGroupingMode(groups.mItems, groups.mGroupCount);

  return NS_OK;
}


// ParseQueryTimeString

nsresult
ParseQueryTimeString(const nsCString& aString, PRTime* aTime)
{
  if (PR_sscanf(aString.get(), "%lld", aTime) != 1)
    return NS_ERROR_INVALID_ARG;
  return NS_OK;
}


// ParseQueryBooleanString
//
//    Converts a 0/1 or true/false string into a bool

nsresult
ParseQueryBooleanString(const nsCString& aString, PRBool* aValue)
{
  if (aString.EqualsLiteral("1") || aString.EqualsLiteral("true")) {
    *aValue = PR_TRUE;
    return NS_OK;
  } else if (aString.EqualsLiteral("0") || aString.EqualsLiteral("false")) {
    *aValue = PR_FALSE;
    return NS_OK;
  }
  return NS_ERROR_INVALID_ARG;
}
