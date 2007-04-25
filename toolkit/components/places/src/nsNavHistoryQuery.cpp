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

/**
 * This file contains the definitions of nsNavHistoryQuery,
 * nsNavHistoryQueryOptions, and those functions in nsINavHistory that directly
 * support queries (specifically QueryStringToQueries and QueriesToQueryString).
 */

#include "nsNavHistory.h"
#include "nsEscape.h"
#include "nsCOMArray.h"
#include "nsNetUtil.h"
#include "nsTArray.h"
#include "prprf.h"

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

static nsresult TokenizeQueryString(const nsACString& aQuery,
                                    nsTArray<QueryKeyValuePair>* aTokens);
static nsresult ParseQueryBooleanString(const nsCString& aString,
                                        PRBool* aValue);

// query getters
typedef NS_STDCALL_FUNCPROTO(nsresult, BoolQueryGetter, nsINavHistoryQuery,
                             GetOnlyBookmarked, (PRBool*));
typedef NS_STDCALL_FUNCPROTO(nsresult, Uint32QueryGetter, nsINavHistoryQuery,
                             GetBeginTimeReference, (PRUint32*));
typedef NS_STDCALL_FUNCPROTO(nsresult, Int64QueryGetter, nsINavHistoryQuery,
                             GetBeginTime, (PRInt64*));
static void AppendBoolKeyValueIfTrue(nsACString& aString,
                                     const nsCString& aName,
                                     nsINavHistoryQuery* aQuery,
                                     BoolQueryGetter getter);
static void AppendUint32KeyValueIfNonzero(nsACString& aString,
                                          const nsCString& aName,
                                          nsINavHistoryQuery* aQuery,
                                          Uint32QueryGetter getter);
static void AppendInt64KeyValueIfNonzero(nsACString& aString,
                                         const nsCString& aName,
                                         nsINavHistoryQuery* aQuery,
                                         Int64QueryGetter getter);

// query setters
typedef NS_STDCALL_FUNCPROTO(nsresult, BoolQuerySetter, nsINavHistoryQuery,
                             SetOnlyBookmarked, (PRBool));
typedef NS_STDCALL_FUNCPROTO(nsresult, Uint32QuerySetter, nsINavHistoryQuery,
                             SetBeginTimeReference, (PRUint32));
typedef NS_STDCALL_FUNCPROTO(nsresult, Int64QuerySetter, nsINavHistoryQuery,
                             SetBeginTime, (PRInt64));
static void SetQueryKeyBool(const nsCString& aValue, nsINavHistoryQuery* aQuery,
                            BoolQuerySetter setter);
static void SetQueryKeyUint32(const nsCString& aValue, nsINavHistoryQuery* aQuery,
                              Uint32QuerySetter setter);
static void SetQueryKeyInt64(const nsCString& aValue, nsINavHistoryQuery* aQuery,
                             Int64QuerySetter setter);

// options setters
typedef NS_STDCALL_FUNCPROTO(nsresult, BoolOptionsSetter,
                             nsINavHistoryQueryOptions,
                             SetExpandQueries, (PRBool));
typedef NS_STDCALL_FUNCPROTO(nsresult, Uint32OptionsSetter,
                             nsINavHistoryQueryOptions,
                             SetMaxResults, (PRUint32));
typedef NS_STDCALL_FUNCPROTO(nsresult, Uint16OptionsSetter,
                             nsINavHistoryQueryOptions,
                             SetResultType, (PRUint16));
static void SetOptionsKeyBool(const nsCString& aValue,
                              nsINavHistoryQueryOptions* aOptions,
                              BoolOptionsSetter setter);
static void SetOptionsKeyUint16(const nsCString& aValue,
                                nsINavHistoryQueryOptions* aOptions,
                                Uint16OptionsSetter setter);
static void SetOptionsKeyUint32(const nsCString& aValue,
                                nsINavHistoryQueryOptions* aOptions,
                                Uint32OptionsSetter setter);

// Components of a query string.
// Note that query strings are also generated in nsNavBookmarks::GetFolderURI
// for performance reasons, so if you change these values, change that, too.
#define QUERYKEY_BEGIN_TIME "beginTime"
#define QUERYKEY_BEGIN_TIME_REFERENCE "beginTimeRef"
#define QUERYKEY_END_TIME "endTime"
#define QUERYKEY_END_TIME_REFERENCE "endTimeRef"
#define QUERYKEY_SEARCH_TERMS "terms"
#define QUERYKEY_MIN_VISITS "minVisits"
#define QUERYKEY_MAX_VISITS "maxVisits"
#define QUERYKEY_ONLY_BOOKMARKED "onlyBookmarked"
#define QUERYKEY_DOMAIN_IS_HOST "domainIsHost"
#define QUERYKEY_DOMAIN "domain"
#define QUERYKEY_FOLDER "folder"
#define QUERYKEY_NOTANNOTATION "!annotation"
#define QUERYKEY_ANNOTATION "annotation"
#define QUERYKEY_URI "uri"
#define QUERYKEY_URIISPREFIX "uriIsPrefix"
#define QUERYKEY_SEPARATOR "OR"
#define QUERYKEY_GROUP "group"
#define QUERYKEY_SORT "sort"
#define QUERYKEY_RESULT_TYPE "type"
#define QUERYKEY_EXCLUDE_ITEMS "excludeItems"
#define QUERYKEY_EXCLUDE_QUERIES "excludeQueries"
#define QUERYKEY_EXCLUDE_READ_ONLY_FOLDERS "excludeReadOnlyFolders"
#define QUERYKEY_EXPAND_QUERIES "expandQueries"
#define QUERYKEY_FORCE_ORIGINAL_TITLE "originalTitle"
#define QUERYKEY_INCLUDE_HIDDEN "includeHidden"
#define QUERYKEY_SHOW_SESSIONS "showSessions"
#define QUERYKEY_MAX_RESULTS "maxResults"
#define QUERYKEY_QUERY_TYPE "queryType"

inline void AppendAmpersandIfNonempty(nsACString& aString)
{
  if (! aString.IsEmpty())
    aString.Append('&');
}
inline void AppendInt16(nsACString& str, PRInt16 i)
{
  nsCAutoString tmp;
  tmp.AppendInt(i);
  str.Append(tmp);
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
//
//    From C++ places code, you should use QueryStringToQueryArray, this is
//    the harder-to-use XPCOM version.

NS_IMETHODIMP
nsNavHistory::QueryStringToQueries(const nsACString& aQueryString,
                                   nsINavHistoryQuery*** aQueries,
                                   PRUint32* aResultCount,
                                   nsINavHistoryQueryOptions** aOptions)
{
  *aQueries = nsnull;
  *aResultCount = 0;
  nsCOMPtr<nsNavHistoryQueryOptions> options;
  nsCOMArray<nsNavHistoryQuery> queries;
  nsresult rv = QueryStringToQueryArray(aQueryString, &queries,
                                        getter_AddRefs(options));
  NS_ENSURE_SUCCESS(rv, rv);

  *aResultCount = queries.Count();
  if (queries.Count() > 0) {
    // convert COM array to raw
    *aQueries = NS_STATIC_CAST(nsINavHistoryQuery**,
                  nsMemory::Alloc(sizeof(nsINavHistoryQuery*) * queries.Count()));
    NS_ENSURE_TRUE(*aQueries, NS_ERROR_OUT_OF_MEMORY);
    for (PRInt32 i = 0; i < queries.Count(); i ++) {
      (*aQueries)[i] = queries[i];
      NS_ADDREF((*aQueries)[i]);
    }
  }
  NS_ADDREF(*aOptions = options);
  return NS_OK;
}


// nsNavHistory::QueryStringToQueryArray
//
//    An internal version of QueryStringToQueries that fills a COM array for
//    ease-of-use.

nsresult
nsNavHistory::QueryStringToQueryArray(const nsACString& aQueryString,
                                      nsCOMArray<nsNavHistoryQuery>* aQueries,
                                      nsNavHistoryQueryOptions** aOptions)
{
  nsresult rv;
  aQueries->Clear();
  *aOptions = nsnull;

  nsRefPtr<nsNavHistoryQueryOptions> options(new nsNavHistoryQueryOptions());
  if (! options)
    return NS_ERROR_OUT_OF_MEMORY;

  nsTArray<QueryKeyValuePair> tokens;
  rv = TokenizeQueryString(aQueryString, &tokens);
  NS_ENSURE_SUCCESS(rv, rv);

  if (tokens.Length() > 0) {
    rv = TokensToQueries(tokens, aQueries, options);
    if (NS_FAILED(rv)) {
      NS_WARNING("Unable to parse the query string: ");
      NS_WARNING(PromiseFlatCString(aQueryString).get());
    }
    NS_ENSURE_SUCCESS(rv, rv);
  }
  // when there are no tokens, leave the query array empty

  NS_ADDREF(*aOptions = options);
  return NS_OK;
}


// nsNavHistory::QueriesToQueryString

NS_IMETHODIMP
nsNavHistory::QueriesToQueryString(nsINavHistoryQuery **aQueries,
                                   PRUint32 aQueryCount,
                                   nsINavHistoryQueryOptions* aOptions,
                                   nsACString& aQueryString)
{
  nsCOMPtr<nsNavHistoryQueryOptions> options = do_QueryInterface(aOptions);
  NS_ENSURE_TRUE(options, NS_ERROR_INVALID_ARG);

  nsCAutoString queryString;
  for (PRUint32 queryIndex = 0; queryIndex < aQueryCount;  queryIndex ++) {
    nsINavHistoryQuery* query = aQueries[queryIndex];
    if (queryIndex > 0) {
      AppendAmpersandIfNonempty(queryString);
      queryString += NS_LITERAL_CSTRING(QUERYKEY_SEPARATOR);
    }

    PRBool hasIt;

    // begin time
    query->GetHasBeginTime(&hasIt);
    if (hasIt) {
      AppendInt64KeyValueIfNonzero(queryString,
                                   NS_LITERAL_CSTRING(QUERYKEY_BEGIN_TIME),
                                   query, &nsINavHistoryQuery::GetBeginTime);
      AppendUint32KeyValueIfNonzero(queryString,
                                    NS_LITERAL_CSTRING(QUERYKEY_BEGIN_TIME_REFERENCE),
                                    query, &nsINavHistoryQuery::GetBeginTimeReference);
    }

    // end time
    query->GetHasEndTime(&hasIt);
    if (hasIt) {
      AppendInt64KeyValueIfNonzero(queryString,
                                   NS_LITERAL_CSTRING(QUERYKEY_END_TIME),
                                   query, &nsINavHistoryQuery::GetEndTime);
      AppendUint32KeyValueIfNonzero(queryString,
                                    NS_LITERAL_CSTRING(QUERYKEY_END_TIME_REFERENCE),
                                    query, &nsINavHistoryQuery::GetEndTimeReference);
    }

    // search terms
    query->GetHasSearchTerms(&hasIt);
    if (hasIt) {
      nsAutoString searchTerms;
      query->GetSearchTerms(searchTerms);
      nsCString escapedTerms;
      if (! NS_Escape(NS_ConvertUTF16toUTF8(searchTerms), escapedTerms,
                      url_XAlphas))
        return NS_ERROR_OUT_OF_MEMORY;

      AppendAmpersandIfNonempty(queryString);
      queryString += NS_LITERAL_CSTRING(QUERYKEY_SEARCH_TERMS "=");
      queryString += escapedTerms;
    }

    // min and max visits
    PRInt32 minVisits;
    if (NS_SUCCEEDED(query->GetMinVisits(&minVisits)) && minVisits >= 0) {
      AppendAmpersandIfNonempty(queryString);
      queryString.Append(NS_LITERAL_CSTRING(QUERYKEY_MIN_VISITS "="));
      AppendInt32(queryString, minVisits);
    }

    PRInt32 maxVisits;
    if (NS_SUCCEEDED(query->GetMaxVisits(&maxVisits)) && maxVisits >= 0) {
      AppendAmpersandIfNonempty(queryString);
      queryString.Append(NS_LITERAL_CSTRING(QUERYKEY_MAX_VISITS "="));
      AppendInt32(queryString, maxVisits);
    }

    // only bookmarked
    AppendBoolKeyValueIfTrue(queryString,
                             NS_LITERAL_CSTRING(QUERYKEY_ONLY_BOOKMARKED),
                             query, &nsINavHistoryQuery::GetOnlyBookmarked);

    // domain (+ is host), only call if hasDomain, which means non-IsVoid
    // this means we may get an empty string for the domain in the result,
    // which is valid
    query->GetHasDomain(&hasIt);
    if (hasIt) {
      AppendBoolKeyValueIfTrue(queryString,
                               NS_LITERAL_CSTRING(QUERYKEY_DOMAIN_IS_HOST),
                               query, &nsINavHistoryQuery::GetDomainIsHost);
      nsCAutoString domain;
      nsresult rv = query->GetDomain(domain);
      NS_ASSERTION(NS_SUCCEEDED(rv), "Failure getting value");
      nsCString escapedDomain;
      PRBool success = NS_Escape(domain, escapedDomain, url_XAlphas);
      NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

      AppendAmpersandIfNonempty(queryString);
      queryString.Append(NS_LITERAL_CSTRING(QUERYKEY_DOMAIN "="));
      queryString.Append(escapedDomain);
    }

    // uri
    query->GetHasUri(&hasIt);
    if (hasIt) {
      AppendBoolKeyValueIfTrue(aQueryString,
                               NS_LITERAL_CSTRING(QUERYKEY_URIISPREFIX),
                               query, &nsINavHistoryQuery::GetUriIsPrefix);
      nsCOMPtr<nsIURI> uri;
      query->GetUri(getter_AddRefs(uri));
      NS_ENSURE_TRUE(uri, NS_ERROR_FAILURE); // hasURI should tell is if invalid
      nsCAutoString uriSpec;
      nsresult rv = uri->GetSpec(uriSpec);
      NS_ENSURE_SUCCESS(rv, rv);
      nsCAutoString escaped;
      PRBool success = NS_Escape(uriSpec, escaped, url_XAlphas);
      NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

      AppendAmpersandIfNonempty(queryString);
      queryString.Append(NS_LITERAL_CSTRING(QUERYKEY_URI "="));
      queryString.Append(escaped);
    }

    // annotation
    query->GetHasAnnotation(&hasIt);
    if (hasIt) {
      AppendAmpersandIfNonempty(queryString);
      PRBool annotationIsNot;
      query->GetAnnotationIsNot(&annotationIsNot);
      if (annotationIsNot)
        queryString.AppendLiteral(QUERYKEY_NOTANNOTATION "=");
      else
        queryString.AppendLiteral(QUERYKEY_ANNOTATION "=");
      nsCAutoString annot;
      query->GetAnnotation(annot);
      nsCAutoString escaped;
      PRBool success = NS_Escape(annot, escaped, url_XAlphas);
      NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);
      queryString.Append(escaped);
    }

    // folders
    PRInt64 *folders = nsnull;
    PRUint32 folderCount = 0;
    query->GetFolders(&folderCount, &folders);
    for (PRUint32 i = 0; i < folderCount; ++i) {
      AppendAmpersandIfNonempty(queryString);
      queryString += NS_LITERAL_CSTRING(QUERYKEY_FOLDER "=");
      AppendInt64(queryString, folders[0]);
    }
    nsMemory::Free(folders);
  }

  // grouping
  PRUint32 groupCount;
  const PRUint16* groups = options->GroupingMode(&groupCount);
  for (PRUint32 i = 0; i < groupCount; i ++) {
    AppendAmpersandIfNonempty(queryString);
    queryString += NS_LITERAL_CSTRING(QUERYKEY_GROUP "=");
    AppendInt16(queryString, groups[i]);
  }

  // sorting
  if (options->SortingMode() != nsINavHistoryQueryOptions::SORT_BY_NONE) {
    AppendAmpersandIfNonempty(queryString);
    queryString += NS_LITERAL_CSTRING(QUERYKEY_SORT "=");
    AppendInt16(queryString, options->SortingMode());
  }

  // result type
  if (options->ResultType() != nsINavHistoryQueryOptions::RESULTS_AS_URI) {
    AppendAmpersandIfNonempty(queryString);
    queryString += NS_LITERAL_CSTRING(QUERYKEY_RESULT_TYPE "=");
    AppendInt16(queryString, options->ResultType());
  }

  // exclude items
  if (options->ExcludeItems()) {
    AppendAmpersandIfNonempty(queryString);
    queryString += NS_LITERAL_CSTRING(QUERYKEY_EXCLUDE_ITEMS "=1");
  }

  // exclude queries
  if (options->ExcludeQueries()) {
    AppendAmpersandIfNonempty(queryString);
    queryString += NS_LITERAL_CSTRING(QUERYKEY_EXCLUDE_QUERIES "=1");
  }

  // exclude read only folders
  if (options->ExcludeReadOnlyFolders()) {
    AppendAmpersandIfNonempty(queryString);
    queryString += NS_LITERAL_CSTRING(QUERYKEY_EXCLUDE_READ_ONLY_FOLDERS "=1");
  }

  // expand queries
  if (options->ExpandQueries()) {
    AppendAmpersandIfNonempty(queryString);
    queryString += NS_LITERAL_CSTRING(QUERYKEY_EXPAND_QUERIES "=1");
  }

  // include hidden
  if (options->IncludeHidden()) {
    AppendAmpersandIfNonempty(queryString);
    queryString += NS_LITERAL_CSTRING(QUERYKEY_INCLUDE_HIDDEN "=1");
  }

  // show sessions
  if (options->ShowSessions()) {
    AppendAmpersandIfNonempty(queryString);
    queryString += NS_LITERAL_CSTRING(QUERYKEY_SHOW_SESSIONS "=1");
  }

  // max results
  if (options->MaxResults()) {
    AppendAmpersandIfNonempty(queryString);
    queryString += NS_LITERAL_CSTRING(QUERYKEY_MAX_RESULTS "=");
    AppendInt32(queryString, options->MaxResults());
  }

  // queryType
  if (options->QueryType() !=  nsINavHistoryQueryOptions::QUERY_TYPE_HISTORY) {
    AppendAmpersandIfNonempty(queryString);
    queryString += NS_LITERAL_CSTRING(QUERYKEY_QUERY_TYPE "=");
    AppendInt16(queryString, options->QueryType());
  }

  aQueryString.Assign(NS_LITERAL_CSTRING("place:") + queryString);
  return NS_OK;
}


// TokenizeQueryString

nsresult
TokenizeQueryString(const nsACString& aQuery,
                    nsTArray<QueryKeyValuePair>* aTokens)
{
  // Strip off the "place:" prefix
  const PRUint32 prefixlen = 6; // = strlen("place:");
  nsCString query;
  if (aQuery.Length() > prefixlen &&
      Substring(aQuery, 0, prefixlen).EqualsLiteral("place:"))
    query = Substring(aQuery, prefixlen);
  else
    query = aQuery;

  PRInt32 keyFirstIndex = 0;
  PRInt32 equalsIndex = 0;
  for (PRUint32 i = 0; i < query.Length(); i ++) {
    if (query[i] == '&') {
      // new clause, save last one
      if (i - keyFirstIndex > 1) {
        if (! aTokens->AppendElement(QueryKeyValuePair(query, keyFirstIndex,
                                                       equalsIndex, i)))
          return NS_ERROR_OUT_OF_MEMORY;
      }
      keyFirstIndex = equalsIndex = i + 1;
    } else if (query[i] == '=') {
      equalsIndex = i;
    }
  }

  // handle last pair, if any
  if (query.Length() - keyFirstIndex > 1) {
    if (! aTokens->AppendElement(QueryKeyValuePair(query, keyFirstIndex,
                                                   equalsIndex, query.Length())))
      return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

// nsNavHistory::TokensToQueries

nsresult
nsNavHistory::TokensToQueries(const nsTArray<QueryKeyValuePair>& aTokens,
                              nsCOMArray<nsNavHistoryQuery>* aQueries,
                              nsNavHistoryQueryOptions* aOptions)
{
  nsresult rv;
  if (aTokens.Length() == 0)
    return NS_OK; // nothing to do

  nsTArray<PRUint16> groups;
  nsTArray<PRInt64> folders;

  nsCOMPtr<nsNavHistoryQuery> query(new nsNavHistoryQuery());
  if (! query)
    return NS_ERROR_OUT_OF_MEMORY;
  if (! aQueries->AppendObject(query))
    return NS_ERROR_OUT_OF_MEMORY;
  for (PRUint32 i = 0; i < aTokens.Length(); i ++) {
    const QueryKeyValuePair& kvp = aTokens[i];

    // begin time
    if (kvp.key.EqualsLiteral(QUERYKEY_BEGIN_TIME)) {
      SetQueryKeyInt64(kvp.value, query, &nsINavHistoryQuery::SetBeginTime);

    // begin time reference
    } else if (kvp.key.EqualsLiteral(QUERYKEY_BEGIN_TIME_REFERENCE)) {
      SetQueryKeyUint32(kvp.value, query, &nsINavHistoryQuery::SetBeginTimeReference);

    // end time
    } else if (kvp.key.EqualsLiteral(QUERYKEY_END_TIME)) {
      SetQueryKeyInt64(kvp.value, query, &nsINavHistoryQuery::SetEndTime);

    // end time reference
    } else if (kvp.key.EqualsLiteral(QUERYKEY_END_TIME_REFERENCE)) {
      SetQueryKeyUint32(kvp.value, query, &nsINavHistoryQuery::SetEndTimeReference);

    // search terms
    } else if (kvp.key.EqualsLiteral(QUERYKEY_SEARCH_TERMS)) {
      nsCString unescapedTerms = kvp.value;
      NS_UnescapeURL(unescapedTerms); // modifies input
      rv = query->SetSearchTerms(NS_ConvertUTF8toUTF16(unescapedTerms));
      NS_ENSURE_SUCCESS(rv, rv);

    // min visits
    } else if (kvp.key.EqualsLiteral(QUERYKEY_MIN_VISITS)) {
      PRInt32 visits = kvp.value.ToInteger((PRInt32*)&rv);
      if (NS_SUCCEEDED(rv))
        query->SetMinVisits(visits);
      else
        NS_WARNING("Bad number for minVisits in query");

    // max visits
    } else if (kvp.key.EqualsLiteral(QUERYKEY_MAX_VISITS)) {
      PRInt32 visits = kvp.value.ToInteger((PRInt32*)&rv);
      if (NS_SUCCEEDED(rv))
        query->SetMaxVisits(visits);
      else
        NS_WARNING("Bad number for maxVisits in query");

    // onlyBookmarked flag
    } else if (kvp.key.EqualsLiteral(QUERYKEY_ONLY_BOOKMARKED)) {
      SetQueryKeyBool(kvp.value, query, &nsINavHistoryQuery::SetOnlyBookmarked);

    // domainIsHost flag
    } else if (kvp.key.EqualsLiteral(QUERYKEY_DOMAIN_IS_HOST)) {
      SetQueryKeyBool(kvp.value, query, &nsINavHistoryQuery::SetDomainIsHost);

    // domain string
    } else if (kvp.key.EqualsLiteral(QUERYKEY_DOMAIN)) {
      nsCAutoString unescapedDomain(kvp.value);
      NS_UnescapeURL(unescapedDomain); // modifies input
      rv = query->SetDomain(unescapedDomain);
      NS_ENSURE_SUCCESS(rv, rv);

    // folders: FIXME use folder name???
    } else if (kvp.key.EqualsLiteral(QUERYKEY_FOLDER)) {
      PRInt64 folder;
      if (PR_sscanf(kvp.value.get(), "%lld", &folder) == 1) {
        NS_ENSURE_TRUE(folders.AppendElement(folder), NS_ERROR_OUT_OF_MEMORY);
      } else {
        NS_WARNING("folders value in query is invalid, ignoring");
      }

    // uri
    } else if (kvp.key.EqualsLiteral(QUERYKEY_URI)) {
      nsCAutoString unescapedUri(kvp.value);
      NS_UnescapeURL(unescapedUri); // modifies input
      nsCOMPtr<nsIURI> uri;
      nsresult rv = NS_NewURI(getter_AddRefs(uri), unescapedUri);
      if (NS_FAILED(rv)) {
        NS_WARNING("Unable to parse URI");
      }
      rv = query->SetUri(uri);
      NS_ENSURE_SUCCESS(rv, rv);

    // URI is prefix
    } else if (kvp.key.EqualsLiteral(QUERYKEY_URIISPREFIX)) {
      SetQueryKeyBool(kvp.value, query, &nsINavHistoryQuery::SetUriIsPrefix);

    // not annotation
    } else if (kvp.key.EqualsLiteral(QUERYKEY_NOTANNOTATION)) {
      nsCAutoString unescaped(kvp.value);
      NS_UnescapeURL(unescaped); // modifies input
      query->SetAnnotationIsNot(PR_TRUE);
      query->SetAnnotation(unescaped);

    // annotation
    } else if (kvp.key.EqualsLiteral(QUERYKEY_ANNOTATION)) {
      nsCAutoString unescaped(kvp.value);
      NS_UnescapeURL(unescaped); // modifies input
      query->SetAnnotationIsNot(PR_FALSE);
      query->SetAnnotation(unescaped);

    // new query component
    } else if (kvp.key.EqualsLiteral(QUERYKEY_SEPARATOR)) {

      if (folders.Length() != 0) {
        query->SetFolders(folders.Elements(), folders.Length());
        folders.Clear();
      }

      query = new nsNavHistoryQuery();
      if (! query)
        return NS_ERROR_OUT_OF_MEMORY;
      if (! aQueries->AppendObject(query))
        return NS_ERROR_OUT_OF_MEMORY;

    // grouping
    } else if (kvp.key.EqualsLiteral(QUERYKEY_GROUP)) {
      PRUint32 grouping = kvp.value.ToInteger((PRInt32*)&rv);
      if (NS_SUCCEEDED(rv)) {
        groups.AppendElement(grouping);
      } else {
        NS_WARNING("Bad number for grouping in query");
      }

    // sorting mode
    } else if (kvp.key.EqualsLiteral(QUERYKEY_SORT)) {
      SetOptionsKeyUint16(kvp.value, aOptions,
                          &nsINavHistoryQueryOptions::SetSortingMode);

    // result type
    } else if (kvp.key.EqualsLiteral(QUERYKEY_RESULT_TYPE)) {
      SetOptionsKeyUint16(kvp.value, aOptions,
                          &nsINavHistoryQueryOptions::SetResultType);

    // exclude items
    } else if (kvp.key.EqualsLiteral(QUERYKEY_EXCLUDE_ITEMS)) {
      SetOptionsKeyBool(kvp.value, aOptions,
                        &nsINavHistoryQueryOptions::SetExcludeItems);

    // exclude queries
    } else if (kvp.key.EqualsLiteral(QUERYKEY_EXCLUDE_QUERIES)) {
      SetOptionsKeyBool(kvp.value, aOptions,
                        &nsINavHistoryQueryOptions::SetExcludeQueries);

    // exclude read only folders
    } else if (kvp.key.EqualsLiteral(QUERYKEY_EXCLUDE_READ_ONLY_FOLDERS)) {
      SetOptionsKeyBool(kvp.value, aOptions,
                        &nsINavHistoryQueryOptions::SetExcludeReadOnlyFolders);

    // expand queries
    } else if (kvp.key.EqualsLiteral(QUERYKEY_EXPAND_QUERIES)) {
      SetOptionsKeyBool(kvp.value, aOptions,
                        &nsINavHistoryQueryOptions::SetExpandQueries);
    // include hidden
    } else if (kvp.key.EqualsLiteral(QUERYKEY_INCLUDE_HIDDEN)) {
      SetOptionsKeyBool(kvp.value, aOptions,
                        &nsINavHistoryQueryOptions::SetIncludeHidden);

    // show sessions
    } else if (kvp.key.EqualsLiteral(QUERYKEY_SHOW_SESSIONS)) {
      SetOptionsKeyBool(kvp.value, aOptions,
                        &nsINavHistoryQueryOptions::SetShowSessions);

    // max results
    } else if (kvp.key.EqualsLiteral(QUERYKEY_MAX_RESULTS)) {
      SetOptionsKeyUint32(kvp.value, aOptions,
                          &nsINavHistoryQueryOptions::SetMaxResults);
    // query type
    } else if (kvp.key.EqualsLiteral(QUERYKEY_QUERY_TYPE)) {
      SetOptionsKeyUint16(kvp.value, aOptions,
                          &nsINavHistoryQueryOptions::SetQueryType);
    // unknown key
    } else {
      aQueries->Clear();
      return NS_ERROR_INVALID_ARG;
    }
  }

  if (folders.Length() != 0)
    query->SetFolders(folders.Elements(), folders.Length());
  aOptions->SetGroupingMode(groups.Elements(), groups.Length());

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


// nsINavHistoryQuery **********************************************************

NS_IMPL_ISUPPORTS2(nsNavHistoryQuery, nsNavHistoryQuery, nsINavHistoryQuery)

// nsINavHistoryQuery::nsNavHistoryQuery
//
//    This must initialize the object such that the default values will cause
//    all history to be returned if this query is used. Then the caller can
//    just set the things it's interested in.

nsNavHistoryQuery::nsNavHistoryQuery()
  : mMinVisits(-1), mMaxVisits(-1), mBeginTime(0),
    mBeginTimeReference(TIME_RELATIVE_EPOCH),
    mEndTime(0), mEndTimeReference(TIME_RELATIVE_EPOCH),
    mOnlyBookmarked(PR_FALSE),
    mDomainIsHost(PR_FALSE), mUriIsPrefix(PR_FALSE),
    mAnnotationIsNot(PR_FALSE)
{
  // differentiate not set (IsVoid) from empty string (local files)
  mDomain.SetIsVoid(PR_TRUE);
}

/* attribute PRTime beginTime; */
NS_IMETHODIMP nsNavHistoryQuery::GetBeginTime(PRTime *aBeginTime)
{
  *aBeginTime = mBeginTime;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetBeginTime(PRTime aBeginTime)
{
  mBeginTime = aBeginTime;
  return NS_OK;
}

/* attribute long beginTimeReference; */
NS_IMETHODIMP nsNavHistoryQuery::GetBeginTimeReference(PRUint32* _retval)
{
  *_retval = mBeginTimeReference;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetBeginTimeReference(PRUint32 aReference)
{
  if (aReference > TIME_RELATIVE_NOW)
    return NS_ERROR_INVALID_ARG;
  mBeginTimeReference = aReference;
  return NS_OK;
}

/* readonly attribute boolean hasBeginTime; */
NS_IMETHODIMP nsNavHistoryQuery::GetHasBeginTime(PRBool* _retval)
{
  *_retval = ! (mBeginTimeReference == TIME_RELATIVE_EPOCH && mBeginTime == 0);
  return NS_OK;
}

/* readonly attribute PRTime absoluteBeginTime; */
NS_IMETHODIMP nsNavHistoryQuery::GetAbsoluteBeginTime(PRTime* _retval)
{
  *_retval = nsNavHistory::NormalizeTime(mBeginTimeReference, mBeginTime);
  return NS_OK;
}

/* attribute PRTime endTime; */
NS_IMETHODIMP nsNavHistoryQuery::GetEndTime(PRTime *aEndTime)
{
  *aEndTime = mEndTime;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetEndTime(PRTime aEndTime)
{
  mEndTime = aEndTime;
  return NS_OK;
}

/* attribute long endTimeReference; */
NS_IMETHODIMP nsNavHistoryQuery::GetEndTimeReference(PRUint32* _retval)
{
  *_retval = mEndTimeReference;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetEndTimeReference(PRUint32 aReference)
{
  if (aReference > TIME_RELATIVE_NOW)
    return NS_ERROR_INVALID_ARG;
  mEndTimeReference = aReference;
  return NS_OK;
}

/* readonly attribute boolean hasEndTime; */
NS_IMETHODIMP nsNavHistoryQuery::GetHasEndTime(PRBool* _retval)
{
  *_retval = ! (mEndTimeReference == TIME_RELATIVE_EPOCH && mEndTime == 0);
  return NS_OK;
}

/* readonly attribute PRTime absoluteEndTime; */
NS_IMETHODIMP nsNavHistoryQuery::GetAbsoluteEndTime(PRTime* _retval)
{
  *_retval = nsNavHistory::NormalizeTime(mEndTimeReference, mEndTime);
  return NS_OK;
}

/* attribute string searchTerms; */
NS_IMETHODIMP nsNavHistoryQuery::GetSearchTerms(nsAString& aSearchTerms)
{
  aSearchTerms = mSearchTerms;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetSearchTerms(const nsAString& aSearchTerms)
{
  mSearchTerms = aSearchTerms;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::GetHasSearchTerms(PRBool* _retval)
{
  *_retval = (! mSearchTerms.IsEmpty());
  return NS_OK;
}

/* attribute PRInt32 minVisits; */
NS_IMETHODIMP nsNavHistoryQuery::GetMinVisits(PRInt32* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = mMinVisits;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetMinVisits(PRInt32 aVisits)
{
  mMinVisits = aVisits;
  return NS_OK;
}

/* attribute PRint32 maxVisits; */
NS_IMETHODIMP nsNavHistoryQuery::GetMaxVisits(PRInt32* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = mMaxVisits;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetMaxVisits(PRInt32 aVisits)
{
  mMaxVisits = aVisits;
  return NS_OK;
}

/* attribute boolean onlyBookmarked; */
NS_IMETHODIMP nsNavHistoryQuery::GetOnlyBookmarked(PRBool *aOnlyBookmarked)
{
  *aOnlyBookmarked = mOnlyBookmarked;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetOnlyBookmarked(PRBool aOnlyBookmarked)
{
  mOnlyBookmarked = aOnlyBookmarked;
  return NS_OK;
}

/* attribute boolean domainIsHost; */
NS_IMETHODIMP nsNavHistoryQuery::GetDomainIsHost(PRBool *aDomainIsHost)
{
  *aDomainIsHost = mDomainIsHost;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetDomainIsHost(PRBool aDomainIsHost)
{
  mDomainIsHost = aDomainIsHost;
  return NS_OK;
}

/* attribute AUTF8String domain; */
NS_IMETHODIMP nsNavHistoryQuery::GetDomain(nsACString& aDomain)
{
  aDomain = mDomain;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetDomain(const nsACString& aDomain)
{
  mDomain = aDomain;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::GetHasDomain(PRBool* _retval)
{
  // note that empty but not void is still a valid query (local files)
  *_retval = (! mDomain.IsVoid());
  return NS_OK;
}

/* attribute boolean uriIsPrefix; */
NS_IMETHODIMP nsNavHistoryQuery::GetUriIsPrefix(PRBool* aIsPrefix)
{
  *aIsPrefix = mUriIsPrefix;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetUriIsPrefix(PRBool aIsPrefix)
{
  mUriIsPrefix = aIsPrefix;
  return NS_OK;
}

/* attribute nsIURI uri; */
NS_IMETHODIMP nsNavHistoryQuery::GetUri(nsIURI** aUri)
{
  *aUri = mUri;
  NS_ADDREF(*aUri);
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetUri(nsIURI* aUri)
{
  mUri = aUri;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::GetHasUri(PRBool* aHasUri)
{
  *aHasUri = (mUri != nsnull);
  return NS_OK;
}

/* attribute boolean annotationIsNot; */
NS_IMETHODIMP nsNavHistoryQuery::GetAnnotationIsNot(PRBool* aIsNot)
{
  *aIsNot = mAnnotationIsNot;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetAnnotationIsNot(PRBool aIsNot)
{
  mAnnotationIsNot = aIsNot;
  return NS_OK;
}

/* attribute AUTF8String annotation; */
NS_IMETHODIMP nsNavHistoryQuery::GetAnnotation(nsACString& aAnnotation)
{
  aAnnotation = mAnnotation;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetAnnotation(const nsACString& aAnnotation)
{
  mAnnotation = aAnnotation;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::GetHasAnnotation(PRBool* aHasIt)
{
  *aHasIt = ! mAnnotation.IsEmpty();
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetFolders(PRUint32 *aCount,
                                            PRInt64 **aFolders)
{
  PRUint32 count = mFolders.Length();
  PRInt64 *folders = nsnull;
  if (count > 0) {
    folders = NS_STATIC_CAST(PRInt64*,
                             nsMemory::Alloc(count * sizeof(PRInt64)));
    NS_ENSURE_TRUE(folders, NS_ERROR_OUT_OF_MEMORY);

    for (PRUint32 i = 0; i < count; ++i) {
      folders[i] = mFolders[i];
    }
  }
  *aCount = count;
  *aFolders = folders;
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetFolderCount(PRUint32 *aCount)
{
  *aCount = mFolders.Length();
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::SetFolders(const PRInt64 *aFolders,
                                            PRUint32 aFolderCount)
{
  if (!mFolders.ReplaceElementsAt(0, mFolders.Length(),
                                  aFolders, aFolderCount)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::Clone(nsINavHistoryQuery** _retval)
{
  *_retval = nsnull;

  nsNavHistoryQuery *clone = new nsNavHistoryQuery(*this);
  NS_ENSURE_TRUE(clone, NS_ERROR_OUT_OF_MEMORY);

  clone->mRefCnt = 0; // the clone doesn't inherit our refcount
  NS_ADDREF(*_retval = clone);
  return NS_OK;
}


// nsNavHistoryQueryOptions
NS_IMPL_ISUPPORTS2(nsNavHistoryQueryOptions, nsNavHistoryQueryOptions, nsINavHistoryQueryOptions)

NS_IMETHODIMP
nsNavHistoryQueryOptions::GetGroupingMode(PRUint32 *aGroupCount,
                                          PRUint16** aGroupingMode)
{
  if (mGroupCount == 0) {
    *aGroupCount = 0;
    *aGroupingMode = nsnull;
    return NS_OK;
  }
  *aGroupingMode = NS_STATIC_CAST(PRUint16*,
                                  nsMemory::Alloc(sizeof(PRUint16) * mGroupCount));
  if (! aGroupingMode)
    return NS_ERROR_OUT_OF_MEMORY;
  for(PRUint32 i = 0; i < mGroupCount; i ++)
    (*aGroupingMode)[i] = mGroupings[i];
  *aGroupCount = mGroupCount;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetGroupingMode(const PRUint16 *aGroupingMode,
                                          PRUint32 aGroupCount)
{
  // check input
  PRUint32 i;
  for (i = 0; i < aGroupCount; i ++) {
    if (aGroupingMode[i] > GROUP_BY_FOLDER)
      return NS_ERROR_INVALID_ARG;
  }

  if (mGroupings) {
    delete[] mGroupings;
    mGroupings = nsnull;
    mGroupCount = 0;
  }
  if (! aGroupCount)
    return NS_OK;

  mGroupings = new PRUint16[aGroupCount];
  NS_ENSURE_TRUE(mGroupings, NS_ERROR_OUT_OF_MEMORY);

  for (i = 0; i < aGroupCount; ++i) {
    mGroupings[i] = aGroupingMode[i];
  }

  mGroupCount = aGroupCount;
  return NS_OK;
}

// sortingMode
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetSortingMode(PRUint16* aMode)
{
  *aMode = mSort;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetSortingMode(PRUint16 aMode)
{
  if (aMode > SORT_BY_ANNOTATION_DESCENDING)
    return NS_ERROR_INVALID_ARG;
  mSort = aMode;
  return NS_OK;
}

// sortingAnnotation
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetSortingAnnotation(nsACString& _result) {
  _result.Assign(mSortingAnnotation);
  return NS_OK;
}

NS_IMETHODIMP
nsNavHistoryQueryOptions::SetSortingAnnotation(const nsACString& aSortingAnnotation) {
  mSortingAnnotation.Assign(aSortingAnnotation);
  return NS_OK;
}

// resultType
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetResultType(PRUint16* aType)
{
  *aType = mResultType;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetResultType(PRUint16 aType)
{
  if (aType > RESULTS_AS_FULL_VISIT)
    return NS_ERROR_INVALID_ARG;
  mResultType = aType;
  return NS_OK;
}

// excludeItems
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetExcludeItems(PRBool* aExclude)
{
  *aExclude = mExcludeItems;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetExcludeItems(PRBool aExclude)
{
  mExcludeItems = aExclude;
  return NS_OK;
}

// excludeQueries
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetExcludeQueries(PRBool* aExclude)
{
  *aExclude = mExcludeQueries;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetExcludeQueries(PRBool aExclude)
{
  mExcludeQueries = aExclude;
  return NS_OK;
}

// excludeReadOnlyFolders
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetExcludeReadOnlyFolders(PRBool* aExclude)
{
  *aExclude = mExcludeReadOnlyFolders;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetExcludeReadOnlyFolders(PRBool aExclude)
{
  mExcludeReadOnlyFolders = aExclude;
  return NS_OK;
}

// expandQueries
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetExpandQueries(PRBool* aExpand)
{
  *aExpand = mExpandQueries;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetExpandQueries(PRBool aExpand)
{
  mExpandQueries = aExpand;
  return NS_OK;
}

// includeHidden
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetIncludeHidden(PRBool* aIncludeHidden)
{
  *aIncludeHidden = mIncludeHidden;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetIncludeHidden(PRBool aIncludeHidden)
{
  mIncludeHidden = aIncludeHidden;
  return NS_OK;
}

// showSessions
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetShowSessions(PRBool* aShowSessions)
{
  *aShowSessions = mShowSessions;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetShowSessions(PRBool aShowSessions)
{
  mShowSessions = aShowSessions;
  return NS_OK;
}

// maxResults
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetMaxResults(PRUint32* aMaxResults)
{
  *aMaxResults = mMaxResults;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetMaxResults(PRUint32 aMaxResults)
{
  mMaxResults = aMaxResults;
  return NS_OK;
}

// queryType
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetQueryType(PRUint16* _retval)
{
  *_retval = mQueryType;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetQueryType(PRUint16 aQueryType)
{
  mQueryType = aQueryType;
  return NS_OK;
}

NS_IMETHODIMP
nsNavHistoryQueryOptions::Clone(nsINavHistoryQueryOptions** aResult)
{
  nsNavHistoryQueryOptions *clone = nsnull;
  nsresult rv = Clone(&clone);
  *aResult = clone;
  return rv;
}

nsresult
nsNavHistoryQueryOptions::Clone(nsNavHistoryQueryOptions **aResult)
{
  *aResult = nsnull;
  nsNavHistoryQueryOptions *result = new nsNavHistoryQueryOptions();
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  nsRefPtr<nsNavHistoryQueryOptions> resultHolder(result);
  result->mSort = mSort;
  result->mResultType = mResultType;
  result->mGroupCount = mGroupCount;
  if (mGroupCount) {
    result->mGroupings = new PRUint16[mGroupCount];
    if (! result->mGroupings) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    for (PRUint32 i = 0; i < mGroupCount; i ++)
      result->mGroupings[i] = mGroupings[i];
  } else {
    result->mGroupCount = nsnull;
  }
  result->mExcludeItems = mExcludeItems;
  result->mExcludeQueries = mExcludeQueries;
  result->mShowSessions = mShowSessions;
  result->mExpandQueries = mExpandQueries;
  result->mMaxResults = mMaxResults;
  result->mQueryType = mQueryType;

  resultHolder.swap(*aResult);
  return NS_OK;
}


// AppendBoolKeyValueIfTrue

void // static
AppendBoolKeyValueIfTrue(nsACString& aString, const nsCString& aName,
                         nsINavHistoryQuery* aQuery,
                         BoolQueryGetter getter)
{
  PRBool value;
  nsresult rv = (aQuery->*getter)(&value);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Failure getting boolean value");
  if (value) {
    AppendAmpersandIfNonempty(aString);
    aString += aName;
    aString.AppendLiteral("=1");
  }
}


// AppendUint32KeyValueIfNonzero

void // static
AppendUint32KeyValueIfNonzero(nsACString& aString,
                              const nsCString& aName,
                              nsINavHistoryQuery* aQuery,
                              Uint32QueryGetter getter)
{
  PRUint32 value;
  nsresult rv = (aQuery->*getter)(&value);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Failure getting value");
  if (value) {
    AppendAmpersandIfNonempty(aString);
    aString += aName;

    // AppendInt requires a concrete string
    nsCAutoString appendMe("=");
    appendMe.AppendInt(value);
    aString.Append(appendMe);
  }
}


// AppendInt64KeyValueIfNonzero

void // static
AppendInt64KeyValueIfNonzero(nsACString& aString,
                             const nsCString& aName,
                             nsINavHistoryQuery* aQuery,
                             Int64QueryGetter getter)
{
  PRInt64 value;
  nsresult rv = (aQuery->*getter)(&value);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Failure getting value");
  if (value) {
    AppendAmpersandIfNonempty(aString);
    aString += aName;
    nsCAutoString appendMe("=");
    appendMe.AppendInt(value);
    aString.Append(appendMe);
  }
}


// SetQuery/OptionsKeyBool

void // static
SetQueryKeyBool(const nsCString& aValue, nsINavHistoryQuery* aQuery,
                BoolQuerySetter setter)
{
  PRBool value;
  nsresult rv = ParseQueryBooleanString(aValue, &value);
  if (NS_SUCCEEDED(rv)) {
    rv = (aQuery->*setter)(value);
    if (NS_FAILED(rv)) {
      NS_WARNING("Error setting boolean key value");
    }
  } else {
    NS_WARNING("Invalid boolean key value in query string.");
  }
}
void // static
SetOptionsKeyBool(const nsCString& aValue, nsINavHistoryQueryOptions* aOptions,
                 BoolOptionsSetter setter)
{
  PRBool value;
  nsresult rv = ParseQueryBooleanString(aValue, &value);
  if (NS_SUCCEEDED(rv)) {
    rv = (aOptions->*setter)(value);
    if (NS_FAILED(rv)) {
      NS_WARNING("Error setting boolean key value");
    }
  } else {
    NS_WARNING("Invalid boolean key value in query string.");
  }
}


// SetQuery/OptionsKeyUint32

void // static
SetQueryKeyUint32(const nsCString& aValue, nsINavHistoryQuery* aQuery,
                  Uint32QuerySetter setter)
{
  nsresult rv;
  PRUint32 value = aValue.ToInteger(NS_REINTERPRET_CAST(PRInt32*, &rv));
  if (NS_SUCCEEDED(rv)) {
    rv = (aQuery->*setter)(value);
    if (NS_FAILED(rv)) {
      NS_WARNING("Error setting Int32 key value");
    }
  } else {
    NS_WARNING("Invalid Int32 key value in query string.");
  }
}
void // static
SetOptionsKeyUint32(const nsCString& aValue, nsINavHistoryQueryOptions* aOptions,
                  Uint32OptionsSetter setter)
{
  nsresult rv;
  PRUint32 value = aValue.ToInteger(NS_REINTERPRET_CAST(PRInt32*, &rv));
  if (NS_SUCCEEDED(rv)) {
    rv = (aOptions->*setter)(value);
    if (NS_FAILED(rv)) {
      NS_WARNING("Error setting Int32 key value");
    }
  } else {
    NS_WARNING("Invalid Int32 key value in query string.");
  }
}

void // static
SetOptionsKeyUint16(const nsCString& aValue, nsINavHistoryQueryOptions* aOptions,
                    Uint16OptionsSetter setter)
{
  nsresult rv;
  PRUint16 value = NS_STATIC_CAST(PRUint16,
                                  aValue.ToInteger(NS_REINTERPRET_CAST(PRInt32*, &rv)));
  if (NS_SUCCEEDED(rv)) {
    rv = (aOptions->*setter)(value);
    if (NS_FAILED(rv)) {
      NS_WARNING("Error setting Int16 key value");
    }
  } else {
    NS_WARNING("Invalid Int16 key value in query string.");
  }
}


// SetQueryKeyInt64

void SetQueryKeyInt64(const nsCString& aValue, nsINavHistoryQuery* aQuery,
                      Int64QuerySetter setter)
{
  nsresult rv;
  PRInt64 value;
  if (PR_sscanf(aValue.get(), "%lld", &value) == 1) {
    rv = (aQuery->*setter)(value);
    if (NS_FAILED(rv)) {
      NS_WARNING("Error setting Int64 key value");
    }
  } else {
    NS_WARNING("Invalid Int64 value in query string.");
  }
}
