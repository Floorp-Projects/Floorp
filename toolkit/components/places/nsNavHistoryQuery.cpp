//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file contains the definitions of nsNavHistoryQuery,
 * nsNavHistoryQueryOptions, and those functions in nsINavHistory that directly
 * support queries (specifically QueryStringToQueries and QueriesToQueryString).
 */

#include "nsNavHistory.h"
#include "nsNavBookmarks.h"
#include "nsEscape.h"
#include "nsCOMArray.h"
#include "nsNetUtil.h"
#include "nsTArray.h"
#include "prprf.h"
#include "mozilla/Util.h"

using namespace mozilla;

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
                                        bool* aValue);

// query getters
typedef NS_STDCALL_FUNCPROTO(nsresult, BoolQueryGetter, nsINavHistoryQuery,
                             GetOnlyBookmarked, (bool*));
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
                             SetOnlyBookmarked, (bool));
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
                             SetExpandQueries, (bool));
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
#define QUERYKEY_SORTING_ANNOTATION "sortingAnnotation"
#define QUERYKEY_RESULT_TYPE "type"
#define QUERYKEY_EXCLUDE_ITEMS "excludeItems"
#define QUERYKEY_EXCLUDE_QUERIES "excludeQueries"
#define QUERYKEY_EXCLUDE_READ_ONLY_FOLDERS "excludeReadOnlyFolders"
#define QUERYKEY_EXPAND_QUERIES "expandQueries"
#define QUERYKEY_FORCE_ORIGINAL_TITLE "originalTitle"
#define QUERYKEY_INCLUDE_HIDDEN "includeHidden"
#define QUERYKEY_MAX_RESULTS "maxResults"
#define QUERYKEY_QUERY_TYPE "queryType"
#define QUERYKEY_TAG "tag"
#define QUERYKEY_NOTTAGS "!tags"
#define QUERYKEY_ASYNC_ENABLED "asyncEnabled"
#define QUERYKEY_TRANSITION "transition"

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

namespace PlacesFolderConversion {
  #define PLACES_ROOT_FOLDER "PLACES_ROOT"
  #define BOOKMARKS_MENU_FOLDER "BOOKMARKS_MENU"
  #define TAGS_FOLDER "TAGS"
  #define UNFILED_BOOKMARKS_FOLDER "UNFILED_BOOKMARKS"
  #define TOOLBAR_FOLDER "TOOLBAR"
  
  /**
   * Converts a folder name to a folder id.
   *
   * @param aName
   *        The name of the folder to convert to a folder id.
   * @returns the folder id if aName is a recognizable name, -1 otherwise.
   */
  inline PRInt64 DecodeFolder(const nsCString &aName)
  {
    nsNavBookmarks *bs = nsNavBookmarks::GetBookmarksService();
    NS_ENSURE_TRUE(bs, false);
    PRInt64 folderID = -1;

    if (aName.EqualsLiteral(PLACES_ROOT_FOLDER))
      (void)bs->GetPlacesRoot(&folderID);
    else if (aName.EqualsLiteral(BOOKMARKS_MENU_FOLDER))
      (void)bs->GetBookmarksMenuFolder(&folderID);
    else if (aName.EqualsLiteral(TAGS_FOLDER))
      (void)bs->GetTagsFolder(&folderID);
    else if (aName.EqualsLiteral(UNFILED_BOOKMARKS_FOLDER))
      (void)bs->GetUnfiledBookmarksFolder(&folderID);
    else if (aName.EqualsLiteral(TOOLBAR_FOLDER))
      (void)bs->GetToolbarFolder(&folderID);

    return folderID;
  }

  /**
   * Converts a folder id to a named constant, or a string representation of the
   * folder id if there is no named constant for the folder, and appends it to
   * aQuery.
   *
   * @param aQuery
   *        The string to append the folder string to.  This is generally a
   *        query string, but could really be anything.
   * @param aFolderID
   *        The folder ID to convert to the proper named constant.
   */
  inline void AppendFolder(nsCString &aQuery, PRInt64 aFolderID)
  {
    nsNavBookmarks *bs = nsNavBookmarks::GetBookmarksService();
    PRInt64 folderID;

    (void)bs->GetPlacesRoot(&folderID);
    if (aFolderID == folderID) {
      aQuery.AppendLiteral(PLACES_ROOT_FOLDER);
      return;
    }

    (void)bs->GetBookmarksMenuFolder(&folderID);
    if (aFolderID == folderID) {
      aQuery.AppendLiteral(BOOKMARKS_MENU_FOLDER);
      return;
    }

    (void)bs->GetTagsFolder(&folderID);
    if (aFolderID == folderID) {
      aQuery.AppendLiteral(TAGS_FOLDER);
      return;
    }

    (void)bs->GetUnfiledBookmarksFolder(&folderID);
    if (aFolderID == folderID) {
      aQuery.AppendLiteral(UNFILED_BOOKMARKS_FOLDER);
      return;
    }

    (void)bs->GetToolbarFolder(&folderID);
    if (aFolderID == folderID) {
      aQuery.AppendLiteral(TOOLBAR_FOLDER);
      return;
    }

    // It wasn't one of our named constants, so just convert it to a string 
    aQuery.AppendInt(aFolderID);
  }
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
  NS_ENSURE_ARG_POINTER(aQueries);
  NS_ENSURE_ARG_POINTER(aResultCount);
  NS_ENSURE_ARG_POINTER(aOptions);

  *aQueries = nullptr;
  *aResultCount = 0;
  nsCOMPtr<nsNavHistoryQueryOptions> options;
  nsCOMArray<nsNavHistoryQuery> queries;
  nsresult rv = QueryStringToQueryArray(aQueryString, &queries,
                                        getter_AddRefs(options));
  NS_ENSURE_SUCCESS(rv, rv);

  *aResultCount = queries.Count();
  if (queries.Count() > 0) {
    // convert COM array to raw
    *aQueries = static_cast<nsINavHistoryQuery**>
                           (nsMemory::Alloc(sizeof(nsINavHistoryQuery*) * queries.Count()));
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
  *aOptions = nullptr;

  nsRefPtr<nsNavHistoryQueryOptions> options(new nsNavHistoryQueryOptions());
  if (! options)
    return NS_ERROR_OUT_OF_MEMORY;

  nsTArray<QueryKeyValuePair> tokens;
  rv = TokenizeQueryString(aQueryString, &tokens);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = TokensToQueries(tokens, aQueries, options);
  if (NS_FAILED(rv)) {
    NS_WARNING("Unable to parse the query string: ");
    NS_WARNING(PromiseFlatCString(aQueryString).get());
    return rv;
  }

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
  NS_ENSURE_ARG(aQueries);
  NS_ENSURE_ARG(aOptions);

  nsCOMPtr<nsNavHistoryQueryOptions> options = do_QueryInterface(aOptions);
  NS_ENSURE_TRUE(options, NS_ERROR_INVALID_ARG);

  nsCAutoString queryString;
  for (PRUint32 queryIndex = 0; queryIndex < aQueryCount;  queryIndex ++) {
    nsCOMPtr<nsNavHistoryQuery> query = do_QueryInterface(aQueries[queryIndex]);
    if (queryIndex > 0) {
      AppendAmpersandIfNonempty(queryString);
      queryString += NS_LITERAL_CSTRING(QUERYKEY_SEPARATOR);
    }

    bool hasIt;

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
      NS_ENSURE_SUCCESS(rv, rv);
      nsCString escapedDomain;
      bool success = NS_Escape(domain, escapedDomain, url_XAlphas);
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
      bool success = NS_Escape(uriSpec, escaped, url_XAlphas);
      NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

      AppendAmpersandIfNonempty(queryString);
      queryString.Append(NS_LITERAL_CSTRING(QUERYKEY_URI "="));
      queryString.Append(escaped);
    }

    // annotation
    query->GetHasAnnotation(&hasIt);
    if (hasIt) {
      AppendAmpersandIfNonempty(queryString);
      bool annotationIsNot;
      query->GetAnnotationIsNot(&annotationIsNot);
      if (annotationIsNot)
        queryString.AppendLiteral(QUERYKEY_NOTANNOTATION "=");
      else
        queryString.AppendLiteral(QUERYKEY_ANNOTATION "=");
      nsCAutoString annot;
      query->GetAnnotation(annot);
      nsCAutoString escaped;
      bool success = NS_Escape(annot, escaped, url_XAlphas);
      NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);
      queryString.Append(escaped);
    }

    // folders
    PRInt64 *folders = nullptr;
    PRUint32 folderCount = 0;
    query->GetFolders(&folderCount, &folders);
    for (PRUint32 i = 0; i < folderCount; ++i) {
      AppendAmpersandIfNonempty(queryString);
      queryString += NS_LITERAL_CSTRING(QUERYKEY_FOLDER "=");
      PlacesFolderConversion::AppendFolder(queryString, folders[i]);
    }
    nsMemory::Free(folders);

    // tags
    const nsTArray<nsString> &tags = query->Tags();
    for (PRUint32 i = 0; i < tags.Length(); ++i) {
      nsCAutoString escapedTag;
      if (!NS_Escape(NS_ConvertUTF16toUTF8(tags[i]), escapedTag, url_XAlphas))
        return NS_ERROR_OUT_OF_MEMORY;

      AppendAmpersandIfNonempty(queryString);
      queryString += NS_LITERAL_CSTRING(QUERYKEY_TAG "=");
      queryString += escapedTag;
    }
    AppendBoolKeyValueIfTrue(queryString,
                             NS_LITERAL_CSTRING(QUERYKEY_NOTTAGS),
                             query,
                             &nsINavHistoryQuery::GetTagsAreNot);
 
    // transitions
    const nsTArray<PRUint32>& transitions = query->Transitions();
    for (PRUint32 i = 0; i < transitions.Length(); ++i) {
      AppendAmpersandIfNonempty(queryString);
      queryString += NS_LITERAL_CSTRING(QUERYKEY_TRANSITION "=");
      AppendInt64(queryString, transitions[i]);
    }
  }

  // sorting
  if (options->SortingMode() != nsINavHistoryQueryOptions::SORT_BY_NONE) {
    AppendAmpersandIfNonempty(queryString);
    queryString += NS_LITERAL_CSTRING(QUERYKEY_SORT "=");
    AppendInt16(queryString, options->SortingMode());
    if (options->SortingMode() == nsINavHistoryQueryOptions::SORT_BY_ANNOTATION_DESCENDING ||
        options->SortingMode() == nsINavHistoryQueryOptions::SORT_BY_ANNOTATION_ASCENDING) {
      // sortingAnnotation
      nsCAutoString sortingAnnotation;
      if (NS_SUCCEEDED(options->GetSortingAnnotation(sortingAnnotation))) {
        nsCString escaped;
        if (!NS_Escape(sortingAnnotation, escaped, url_XAlphas))
          return NS_ERROR_OUT_OF_MEMORY;
        AppendAmpersandIfNonempty(queryString);
        queryString += NS_LITERAL_CSTRING(QUERYKEY_SORTING_ANNOTATION "=");
        queryString.Append(escaped);
      }
    } 
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
  if (!options->ExpandQueries()) {
    AppendAmpersandIfNonempty(queryString);
    queryString += NS_LITERAL_CSTRING(QUERYKEY_EXPAND_QUERIES "=0");
  }

  // include hidden
  if (options->IncludeHidden()) {
    AppendAmpersandIfNonempty(queryString);
    queryString += NS_LITERAL_CSTRING(QUERYKEY_INCLUDE_HIDDEN "=1");
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

  // async enabled
  if (options->AsyncEnabled()) {
    AppendAmpersandIfNonempty(queryString);
    queryString += NS_LITERAL_CSTRING(QUERYKEY_ASYNC_ENABLED "=1");
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
  if (aQuery.Length() >= prefixlen &&
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

  nsCOMPtr<nsNavHistoryQuery> query(new nsNavHistoryQuery());
  if (! query)
    return NS_ERROR_OUT_OF_MEMORY;
  if (! aQueries->AppendObject(query))
    return NS_ERROR_OUT_OF_MEMORY;

  if (aTokens.Length() == 0)
    return NS_OK; // nothing to do

  nsTArray<PRInt64> folders;
  nsTArray<nsString> tags;
  nsTArray<PRUint32> transitions;
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
      PRInt32 visits = kvp.value.ToInteger(&rv);
      if (NS_SUCCEEDED(rv))
        query->SetMinVisits(visits);
      else
        NS_WARNING("Bad number for minVisits in query");

    // max visits
    } else if (kvp.key.EqualsLiteral(QUERYKEY_MAX_VISITS)) {
      PRInt32 visits = kvp.value.ToInteger(&rv);
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

    // folders
    } else if (kvp.key.EqualsLiteral(QUERYKEY_FOLDER)) {
      PRInt64 folder;
      if (PR_sscanf(kvp.value.get(), "%lld", &folder) == 1) {
        NS_ENSURE_TRUE(folders.AppendElement(folder), NS_ERROR_OUT_OF_MEMORY);
      } else {
        folder = PlacesFolderConversion::DecodeFolder(kvp.value);
        if (folder != -1)
          NS_ENSURE_TRUE(folders.AppendElement(folder), NS_ERROR_OUT_OF_MEMORY);
        else
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
      query->SetAnnotationIsNot(true);
      query->SetAnnotation(unescaped);

    // annotation
    } else if (kvp.key.EqualsLiteral(QUERYKEY_ANNOTATION)) {
      nsCAutoString unescaped(kvp.value);
      NS_UnescapeURL(unescaped); // modifies input
      query->SetAnnotationIsNot(false);
      query->SetAnnotation(unescaped);

    // tag
    } else if (kvp.key.EqualsLiteral(QUERYKEY_TAG)) {
      nsCAutoString unescaped(kvp.value);
      NS_UnescapeURL(unescaped); // modifies input
      NS_ConvertUTF8toUTF16 tag(unescaped);
      if (!tags.Contains(tag)) {
        NS_ENSURE_TRUE(tags.AppendElement(tag), NS_ERROR_OUT_OF_MEMORY);
      }

    // not tags
    } else if (kvp.key.EqualsLiteral(QUERYKEY_NOTTAGS)) {
      SetQueryKeyBool(kvp.value, query, &nsINavHistoryQuery::SetTagsAreNot);

    // transition
    } else if (kvp.key.EqualsLiteral(QUERYKEY_TRANSITION)) {
      PRUint32 transition = kvp.value.ToInteger(&rv);
      if (NS_SUCCEEDED(rv)) {
        if (!transitions.Contains(transition))
          NS_ENSURE_TRUE(transitions.AppendElement(transition),
                         NS_ERROR_OUT_OF_MEMORY);
      }
      else {
        NS_WARNING("Invalid Int32 transition value.");
      }

    // new query component
    } else if (kvp.key.EqualsLiteral(QUERYKEY_SEPARATOR)) {

      if (folders.Length() != 0) {
        query->SetFolders(folders.Elements(), folders.Length());
        folders.Clear();
      }

      if (tags.Length() > 0) {
        rv = query->SetTags(tags);
        NS_ENSURE_SUCCESS(rv, rv);
        tags.Clear();
      }

      if (transitions.Length() > 0) {
        rv = query->SetTransitions(transitions);
        NS_ENSURE_SUCCESS(rv, rv);
        transitions.Clear();
      }

      query = new nsNavHistoryQuery();
      if (! query)
        return NS_ERROR_OUT_OF_MEMORY;
      if (! aQueries->AppendObject(query))
        return NS_ERROR_OUT_OF_MEMORY;

    // sorting mode
    } else if (kvp.key.EqualsLiteral(QUERYKEY_SORT)) {
      SetOptionsKeyUint16(kvp.value, aOptions,
                          &nsINavHistoryQueryOptions::SetSortingMode);
    // sorting annotation
    } else if (kvp.key.EqualsLiteral(QUERYKEY_SORTING_ANNOTATION)) {
      nsCString sortingAnnotation = kvp.value;
      NS_UnescapeURL(sortingAnnotation);
      rv = aOptions->SetSortingAnnotation(sortingAnnotation);
      NS_ENSURE_SUCCESS(rv, rv);
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
    // max results
    } else if (kvp.key.EqualsLiteral(QUERYKEY_MAX_RESULTS)) {
      SetOptionsKeyUint32(kvp.value, aOptions,
                          &nsINavHistoryQueryOptions::SetMaxResults);
    // query type
    } else if (kvp.key.EqualsLiteral(QUERYKEY_QUERY_TYPE)) {
      SetOptionsKeyUint16(kvp.value, aOptions,
                          &nsINavHistoryQueryOptions::SetQueryType);
    // async enabled
    } else if (kvp.key.EqualsLiteral(QUERYKEY_ASYNC_ENABLED)) {
      SetOptionsKeyBool(kvp.value, aOptions,
                        &nsINavHistoryQueryOptions::SetAsyncEnabled);
    // unknown key
    } else {
      NS_WARNING("TokensToQueries(), ignoring unknown key: ");
      NS_WARNING(kvp.key.get());
    }
  }

  if (folders.Length() != 0)
    query->SetFolders(folders.Elements(), folders.Length());

  if (tags.Length() > 0) {
    rv = query->SetTags(tags);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (transitions.Length() > 0) {
    rv = query->SetTransitions(transitions);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


// ParseQueryBooleanString
//
//    Converts a 0/1 or true/false string into a bool

nsresult
ParseQueryBooleanString(const nsCString& aString, bool* aValue)
{
  if (aString.EqualsLiteral("1") || aString.EqualsLiteral("true")) {
    *aValue = true;
    return NS_OK;
  } else if (aString.EqualsLiteral("0") || aString.EqualsLiteral("false")) {
    *aValue = false;
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
    mOnlyBookmarked(false),
    mDomainIsHost(false), mUriIsPrefix(false),
    mAnnotationIsNot(false),
    mTagsAreNot(false)
{
  // differentiate not set (IsVoid) from empty string (local files)
  mDomain.SetIsVoid(true);
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
NS_IMETHODIMP nsNavHistoryQuery::GetHasBeginTime(bool* _retval)
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
NS_IMETHODIMP nsNavHistoryQuery::GetHasEndTime(bool* _retval)
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
NS_IMETHODIMP nsNavHistoryQuery::GetHasSearchTerms(bool* _retval)
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
NS_IMETHODIMP nsNavHistoryQuery::GetOnlyBookmarked(bool *aOnlyBookmarked)
{
  *aOnlyBookmarked = mOnlyBookmarked;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetOnlyBookmarked(bool aOnlyBookmarked)
{
  mOnlyBookmarked = aOnlyBookmarked;
  return NS_OK;
}

/* attribute boolean domainIsHost; */
NS_IMETHODIMP nsNavHistoryQuery::GetDomainIsHost(bool *aDomainIsHost)
{
  *aDomainIsHost = mDomainIsHost;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetDomainIsHost(bool aDomainIsHost)
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
NS_IMETHODIMP nsNavHistoryQuery::GetHasDomain(bool* _retval)
{
  // note that empty but not void is still a valid query (local files)
  *_retval = (! mDomain.IsVoid());
  return NS_OK;
}

/* attribute boolean uriIsPrefix; */
NS_IMETHODIMP nsNavHistoryQuery::GetUriIsPrefix(bool* aIsPrefix)
{
  *aIsPrefix = mUriIsPrefix;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetUriIsPrefix(bool aIsPrefix)
{
  mUriIsPrefix = aIsPrefix;
  return NS_OK;
}

/* attribute nsIURI uri; */
NS_IMETHODIMP nsNavHistoryQuery::GetUri(nsIURI** aUri)
{
  NS_IF_ADDREF(*aUri = mUri);
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetUri(nsIURI* aUri)
{
  mUri = aUri;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::GetHasUri(bool* aHasUri)
{
  *aHasUri = (mUri != nullptr);
  return NS_OK;
}

/* attribute boolean annotationIsNot; */
NS_IMETHODIMP nsNavHistoryQuery::GetAnnotationIsNot(bool* aIsNot)
{
  *aIsNot = mAnnotationIsNot;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetAnnotationIsNot(bool aIsNot)
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
NS_IMETHODIMP nsNavHistoryQuery::GetHasAnnotation(bool* aHasIt)
{
  *aHasIt = ! mAnnotation.IsEmpty();
  return NS_OK;
}

/* attribute nsIVariant tags; */
NS_IMETHODIMP nsNavHistoryQuery::GetTags(nsIVariant **aTags)
{
  NS_ENSURE_ARG_POINTER(aTags);

  nsresult rv;
  nsCOMPtr<nsIWritableVariant> out = do_CreateInstance(NS_VARIANT_CONTRACTID,
                                                       &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 arrayLen = mTags.Length();

  if (arrayLen == 0)
    rv = out->SetAsEmptyArray();
  else {
    // Note: The resulting nsIVariant dupes both the array and its elements.
    const PRUnichar **array = reinterpret_cast<const PRUnichar **>
                              (NS_Alloc(arrayLen * sizeof(PRUnichar *)));
    NS_ENSURE_TRUE(array, NS_ERROR_OUT_OF_MEMORY);

    for (PRUint32 i = 0; i < arrayLen; ++i) {
      array[i] = mTags[i].get();
    }

    rv = out->SetAsArray(nsIDataType::VTYPE_WCHAR_STR,
                         nullptr,
                         arrayLen,
                         reinterpret_cast<void *>(array));
    NS_Free(array);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  NS_ADDREF(*aTags = out);
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::SetTags(nsIVariant *aTags)
{
  NS_ENSURE_ARG(aTags);

  PRUint16 dataType;
  aTags->GetDataType(&dataType);

  // Caller passed in empty array.  Easy -- clear our mTags array and return.
  if (dataType == nsIDataType::VTYPE_EMPTY_ARRAY) {
    mTags.Clear();
    return NS_OK;
  }

  // Before we go any further, make sure caller passed in an array.
  NS_ENSURE_TRUE(dataType == nsIDataType::VTYPE_ARRAY, NS_ERROR_ILLEGAL_VALUE);

  PRUint16 eltType;
  nsIID eltIID;
  PRUint32 arrayLen;
  void *array;

  // Convert the nsIVariant to an array.  We own the resulting buffer and its
  // elements.
  nsresult rv = aTags->GetAsArray(&eltType, &eltIID, &arrayLen, &array);
  NS_ENSURE_SUCCESS(rv, rv);

  // If element type is not wstring, thanks a lot.  Your memory die now.
  if (eltType != nsIDataType::VTYPE_WCHAR_STR) {
    switch (eltType) {
    case nsIDataType::VTYPE_ID:
    case nsIDataType::VTYPE_CHAR_STR:
      {
        char **charArray = reinterpret_cast<char **>(array);
        for (PRUint32 i = 0; i < arrayLen; ++i) {
          if (charArray[i])
            NS_Free(charArray[i]);
        }
      }
      break;
    case nsIDataType::VTYPE_INTERFACE:
    case nsIDataType::VTYPE_INTERFACE_IS:
      {
        nsISupports **supportsArray = reinterpret_cast<nsISupports **>(array);
        for (PRUint32 i = 0; i < arrayLen; ++i) {
          NS_IF_RELEASE(supportsArray[i]);
        }
      }
      break;
    // The other types are primitives that do not need to be freed.
    }
    NS_Free(array);
    return NS_ERROR_ILLEGAL_VALUE;
  }

  PRUnichar **tags = reinterpret_cast<PRUnichar **>(array);
  mTags.Clear();

  // Finally, add each passed-in tag to our mTags array and then sort it.
  for (PRUint32 i = 0; i < arrayLen; ++i) {

    // Don't allow nulls.
    if (!tags[i]) {
      NS_Free(tags);
      return NS_ERROR_ILLEGAL_VALUE;
    }

    nsDependentString tag(tags[i]);

    // Don't store duplicate tags.  This isn't just to save memory or to be
    // fancy; the SQL that's built from the tags relies on no dupes.
    if (!mTags.Contains(tag)) {
      if (!mTags.AppendElement(tag)) {
        NS_Free(tags[i]);
        NS_Free(tags);
        return NS_ERROR_OUT_OF_MEMORY;
      }
    }
    NS_Free(tags[i]);
  }
  NS_Free(tags);

  mTags.Sort();

  return NS_OK;
}

/* attribute boolean tagsAreNot; */
NS_IMETHODIMP nsNavHistoryQuery::GetTagsAreNot(bool *aTagsAreNot)
{
  NS_ENSURE_ARG_POINTER(aTagsAreNot);
  *aTagsAreNot = mTagsAreNot;
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::SetTagsAreNot(bool aTagsAreNot)
{
  mTagsAreNot = aTagsAreNot;
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetFolders(PRUint32 *aCount,
                                            PRInt64 **aFolders)
{
  PRUint32 count = mFolders.Length();
  PRInt64 *folders = nullptr;
  if (count > 0) {
    folders = static_cast<PRInt64*>
                         (nsMemory::Alloc(count * sizeof(PRInt64)));
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

NS_IMETHODIMP nsNavHistoryQuery::GetTransitions(PRUint32* aCount,
                                                PRUint32** aTransitions)
{
  PRUint32 count = mTransitions.Length();
  PRUint32* transitions = nullptr;
  if (count > 0) {
    transitions = reinterpret_cast<PRUint32*>
                  (NS_Alloc(count * sizeof(PRUint32)));
    NS_ENSURE_TRUE(transitions, NS_ERROR_OUT_OF_MEMORY);
    for (PRUint32 i = 0; i < count; ++i) {
      transitions[i] = mTransitions[i];
    }
  }
  *aCount = count;
  *aTransitions = transitions;
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetTransitionCount(PRUint32* aCount)
{
  *aCount = mTransitions.Length();
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::SetTransitions(const PRUint32* aTransitions,
                                                PRUint32 aCount)
{
  if (!mTransitions.ReplaceElementsAt(0, mTransitions.Length(), aTransitions,
                                      aCount))
    return NS_ERROR_OUT_OF_MEMORY;

  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::Clone(nsINavHistoryQuery** _retval)
{
  *_retval = nullptr;

  nsNavHistoryQuery *clone = new nsNavHistoryQuery(*this);
  NS_ENSURE_TRUE(clone, NS_ERROR_OUT_OF_MEMORY);

  clone->mRefCnt = 0; // the clone doesn't inherit our refcount
  NS_ADDREF(*_retval = clone);
  return NS_OK;
}


// nsNavHistoryQueryOptions
NS_IMPL_ISUPPORTS2(nsNavHistoryQueryOptions, nsNavHistoryQueryOptions, nsINavHistoryQueryOptions)

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
  if (aMode > SORT_BY_FRECENCY_DESCENDING)
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
  if (aType > RESULTS_AS_TAG_CONTENTS)
    return NS_ERROR_INVALID_ARG;
  // Tag queries and containers are bookmarks related, so we set the QueryType
  // accordingly.
  if (aType == RESULTS_AS_TAG_QUERY || aType == RESULTS_AS_TAG_CONTENTS)
    mQueryType = QUERY_TYPE_BOOKMARKS;
  mResultType = aType;
  return NS_OK;
}

// excludeItems
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetExcludeItems(bool* aExclude)
{
  *aExclude = mExcludeItems;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetExcludeItems(bool aExclude)
{
  mExcludeItems = aExclude;
  return NS_OK;
}

// excludeQueries
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetExcludeQueries(bool* aExclude)
{
  *aExclude = mExcludeQueries;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetExcludeQueries(bool aExclude)
{
  mExcludeQueries = aExclude;
  return NS_OK;
}

// excludeReadOnlyFolders
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetExcludeReadOnlyFolders(bool* aExclude)
{
  *aExclude = mExcludeReadOnlyFolders;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetExcludeReadOnlyFolders(bool aExclude)
{
  mExcludeReadOnlyFolders = aExclude;
  return NS_OK;
}

// expandQueries
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetExpandQueries(bool* aExpand)
{
  *aExpand = mExpandQueries;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetExpandQueries(bool aExpand)
{
  mExpandQueries = aExpand;
  return NS_OK;
}

// includeHidden
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetIncludeHidden(bool* aIncludeHidden)
{
  *aIncludeHidden = mIncludeHidden;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetIncludeHidden(bool aIncludeHidden)
{
  mIncludeHidden = aIncludeHidden;
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
  // Tag query and containers are forced to QUERY_TYPE_BOOKMARKS when the
  // resultType is set.
  if (mResultType == RESULTS_AS_TAG_CONTENTS ||
      mResultType == RESULTS_AS_TAG_QUERY)
   return NS_OK;
  mQueryType = aQueryType;
  return NS_OK;
}

// asyncEnabled
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetAsyncEnabled(bool* _asyncEnabled)
{
  *_asyncEnabled = mAsyncEnabled;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetAsyncEnabled(bool aAsyncEnabled)
{
  mAsyncEnabled = aAsyncEnabled;
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryQueryOptions::Clone(nsINavHistoryQueryOptions** aResult)
{
  nsNavHistoryQueryOptions *clone = nullptr;
  nsresult rv = Clone(&clone);
  *aResult = clone;
  return rv;
}

nsresult
nsNavHistoryQueryOptions::Clone(nsNavHistoryQueryOptions **aResult)
{
  *aResult = nullptr;
  nsNavHistoryQueryOptions *result = new nsNavHistoryQueryOptions();
  if (! result)
    return NS_ERROR_OUT_OF_MEMORY;

  nsRefPtr<nsNavHistoryQueryOptions> resultHolder(result);
  result->mSort = mSort;
  result->mResultType = mResultType;
  result->mExcludeItems = mExcludeItems;
  result->mExcludeQueries = mExcludeQueries;
  result->mExpandQueries = mExpandQueries;
  result->mMaxResults = mMaxResults;
  result->mQueryType = mQueryType;
  result->mParentAnnotationToExclude = mParentAnnotationToExclude;
  result->mAsyncEnabled = mAsyncEnabled;

  resultHolder.swap(*aResult);
  return NS_OK;
}


// AppendBoolKeyValueIfTrue

void // static
AppendBoolKeyValueIfTrue(nsACString& aString, const nsCString& aName,
                         nsINavHistoryQuery* aQuery,
                         BoolQueryGetter getter)
{
  bool value;
  DebugOnly<nsresult> rv = (aQuery->*getter)(&value);
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
  DebugOnly<nsresult> rv = (aQuery->*getter)(&value);
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
  DebugOnly<nsresult> rv = (aQuery->*getter)(&value);
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
  bool value;
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
  bool value;
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
  PRUint32 value = aValue.ToInteger(&rv);
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
  PRUint32 value = aValue.ToInteger(&rv);
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
  PRUint16 value = static_cast<PRUint16>(aValue.ToInteger(&rv));
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
