//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file contains the definitions of nsNavHistoryQuery,
 * nsNavHistoryQueryOptions, and those functions in nsINavHistory that directly
 * support queries (specifically QueryStringToQuery and QueryToQueryString).
 */

#include "mozilla/DebugOnly.h"

#include "nsNavHistory.h"
#include "nsNavBookmarks.h"
#include "nsEscape.h"
#include "nsCOMArray.h"
#include "nsNetUtil.h"
#include "nsTArray.h"
#include "prprf.h"
#include "nsVariant.h"

using namespace mozilla;
using namespace mozilla::places;

static nsresult ParseQueryBooleanString(const nsCString& aString, bool* aValue);

// query getters
typedef decltype(&nsINavHistoryQuery::GetOnlyBookmarked) BoolQueryGetter;
typedef decltype(&nsINavHistoryQuery::GetBeginTimeReference) Uint32QueryGetter;
typedef decltype(&nsINavHistoryQuery::GetBeginTime) Int64QueryGetter;
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
typedef decltype(&nsINavHistoryQuery::SetOnlyBookmarked) BoolQuerySetter;
typedef decltype(&nsINavHistoryQuery::SetBeginTimeReference) Uint32QuerySetter;
typedef decltype(&nsINavHistoryQuery::SetBeginTime) Int64QuerySetter;
static void SetQueryKeyBool(const nsCString& aValue, nsINavHistoryQuery* aQuery,
                            BoolQuerySetter setter);
static void SetQueryKeyUint32(const nsCString& aValue,
                              nsINavHistoryQuery* aQuery,
                              Uint32QuerySetter setter);
static void SetQueryKeyInt64(const nsCString& aValue,
                             nsINavHistoryQuery* aQuery,
                             Int64QuerySetter setter);

// options setters
typedef decltype(
    &nsINavHistoryQueryOptions::SetExpandQueries) BoolOptionsSetter;
typedef decltype(&nsINavHistoryQueryOptions::SetMaxResults) Uint32OptionsSetter;
typedef decltype(&nsINavHistoryQueryOptions::SetResultType) Uint16OptionsSetter;
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
#define QUERYKEY_PARENT "parent"
#define QUERYKEY_NOTANNOTATION "!annotation"
#define QUERYKEY_ANNOTATION "annotation"
#define QUERYKEY_URI "uri"
#define QUERYKEY_GROUP "group"
#define QUERYKEY_SORT "sort"
#define QUERYKEY_RESULT_TYPE "type"
#define QUERYKEY_EXCLUDE_ITEMS "excludeItems"
#define QUERYKEY_EXCLUDE_QUERIES "excludeQueries"
#define QUERYKEY_EXPAND_QUERIES "expandQueries"
#define QUERYKEY_FORCE_ORIGINAL_TITLE "originalTitle"
#define QUERYKEY_INCLUDE_HIDDEN "includeHidden"
#define QUERYKEY_MAX_RESULTS "maxResults"
#define QUERYKEY_QUERY_TYPE "queryType"
#define QUERYKEY_TAG "tag"
#define QUERYKEY_NOTTAGS "!tags"
#define QUERYKEY_ASYNC_ENABLED "asyncEnabled"
#define QUERYKEY_TRANSITION "transition"

inline void AppendAmpersandIfNonempty(nsACString& aString) {
  if (!aString.IsEmpty()) aString.Append('&');
}
inline void AppendInt16(nsACString& str, int16_t i) {
  nsAutoCString tmp;
  tmp.AppendInt(i);
  str.Append(tmp);
}
inline void AppendInt32(nsACString& str, int32_t i) {
  nsAutoCString tmp;
  tmp.AppendInt(i);
  str.Append(tmp);
}
inline void AppendInt64(nsACString& str, int64_t i) {
  nsCString tmp;
  tmp.AppendInt(i);
  str.Append(tmp);
}

NS_IMETHODIMP
nsNavHistory::QueryStringToQuery(const nsACString& aQueryString,
                                 nsINavHistoryQuery** _query,
                                 nsINavHistoryQueryOptions** _options) {
  NS_ENSURE_ARG_POINTER(_query);
  NS_ENSURE_ARG_POINTER(_options);

  nsTArray<QueryKeyValuePair> tokens;
  nsresult rv = TokenizeQueryString(aQueryString, &tokens);
  NS_ENSURE_SUCCESS(rv, rv);

  RefPtr<nsNavHistoryQueryOptions> options = new nsNavHistoryQueryOptions();
  RefPtr<nsNavHistoryQuery> query = new nsNavHistoryQuery();
  rv = TokensToQuery(tokens, query, options);
  MOZ_ASSERT(NS_SUCCEEDED(rv), "The query string should be valid");
  if (NS_FAILED(rv)) {
    NS_WARNING("Unable to parse the query string: ");
    NS_WARNING(PromiseFlatCString(aQueryString).get());
  }

  options.forget(_options);
  query.forget(_query);
  return NS_OK;
}

NS_IMETHODIMP
nsNavHistory::QueryToQueryString(nsINavHistoryQuery* aQuery,
                                 nsINavHistoryQueryOptions* aOptions,
                                 nsACString& aQueryString) {
  NS_ENSURE_ARG(aQuery);
  NS_ENSURE_ARG(aOptions);

  RefPtr<nsNavHistoryQuery> query = do_QueryObject(aQuery);
  NS_ENSURE_STATE(query);
  RefPtr<nsNavHistoryQueryOptions> options = do_QueryObject(aOptions);
  NS_ENSURE_STATE(options);

  nsAutoCString queryString;
  bool hasIt;

  // begin time
  query->GetHasBeginTime(&hasIt);
  if (hasIt) {
    AppendInt64KeyValueIfNonzero(queryString,
                                 NS_LITERAL_CSTRING(QUERYKEY_BEGIN_TIME), query,
                                 &nsINavHistoryQuery::GetBeginTime);
    AppendUint32KeyValueIfNonzero(
        queryString, NS_LITERAL_CSTRING(QUERYKEY_BEGIN_TIME_REFERENCE), query,
        &nsINavHistoryQuery::GetBeginTimeReference);
  }

  // end time
  query->GetHasEndTime(&hasIt);
  if (hasIt) {
    AppendInt64KeyValueIfNonzero(queryString,
                                 NS_LITERAL_CSTRING(QUERYKEY_END_TIME), query,
                                 &nsINavHistoryQuery::GetEndTime);
    AppendUint32KeyValueIfNonzero(
        queryString, NS_LITERAL_CSTRING(QUERYKEY_END_TIME_REFERENCE), query,
        &nsINavHistoryQuery::GetEndTimeReference);
  }

  // search terms
  if (!query->SearchTerms().IsEmpty()) {
    const nsString& searchTerms = query->SearchTerms();
    nsCString escapedTerms;
    if (!NS_Escape(NS_ConvertUTF16toUTF8(searchTerms), escapedTerms,
                   url_XAlphas))
      return NS_ERROR_OUT_OF_MEMORY;

    AppendAmpersandIfNonempty(queryString);
    queryString += NS_LITERAL_CSTRING(QUERYKEY_SEARCH_TERMS "=");
    queryString += escapedTerms;
  }

  // min and max visits
  int32_t minVisits;
  if (NS_SUCCEEDED(query->GetMinVisits(&minVisits)) && minVisits >= 0) {
    AppendAmpersandIfNonempty(queryString);
    queryString.AppendLiteral(QUERYKEY_MIN_VISITS "=");
    AppendInt32(queryString, minVisits);
  }

  int32_t maxVisits;
  if (NS_SUCCEEDED(query->GetMaxVisits(&maxVisits)) && maxVisits >= 0) {
    AppendAmpersandIfNonempty(queryString);
    queryString.AppendLiteral(QUERYKEY_MAX_VISITS "=");
    AppendInt32(queryString, maxVisits);
  }

  // only bookmarked
  AppendBoolKeyValueIfTrue(queryString,
                           NS_LITERAL_CSTRING(QUERYKEY_ONLY_BOOKMARKED), query,
                           &nsINavHistoryQuery::GetOnlyBookmarked);

  // domain (+ is host), only call if hasDomain, which means non-IsVoid
  // this means we may get an empty string for the domain in the result,
  // which is valid
  if (!query->Domain().IsVoid()) {
    AppendBoolKeyValueIfTrue(queryString,
                             NS_LITERAL_CSTRING(QUERYKEY_DOMAIN_IS_HOST), query,
                             &nsINavHistoryQuery::GetDomainIsHost);
    const nsCString& domain = query->Domain();
    nsCString escapedDomain;
    bool success = NS_Escape(domain, escapedDomain, url_XAlphas);
    NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

    AppendAmpersandIfNonempty(queryString);
    queryString.AppendLiteral(QUERYKEY_DOMAIN "=");
    queryString.Append(escapedDomain);
  }

  // uri
  if (query->Uri()) {
    nsCOMPtr<nsIURI> uri = query->Uri();
    nsAutoCString uriSpec;
    nsresult rv = uri->GetSpec(uriSpec);
    NS_ENSURE_SUCCESS(rv, rv);
    nsAutoCString escaped;
    bool success = NS_Escape(uriSpec, escaped, url_XAlphas);
    NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);

    AppendAmpersandIfNonempty(queryString);
    queryString.AppendLiteral(QUERYKEY_URI "=");
    queryString.Append(escaped);
  }

  // annotation
  if (!query->Annotation().IsEmpty()) {
    AppendAmpersandIfNonempty(queryString);
    if (query->AnnotationIsNot()) {
      queryString.AppendLiteral(QUERYKEY_NOTANNOTATION "=");
    } else {
      queryString.AppendLiteral(QUERYKEY_ANNOTATION "=");
    }
    const nsCString& annot = query->Annotation();
    nsAutoCString escaped;
    bool success = NS_Escape(annot, escaped, url_XAlphas);
    NS_ENSURE_TRUE(success, NS_ERROR_OUT_OF_MEMORY);
    queryString.Append(escaped);
  }

  // parents
  const nsTArray<nsCString>& parents = query->Parents();
  for (uint32_t i = 0; i < parents.Length(); ++i) {
    AppendAmpersandIfNonempty(queryString);
    queryString += NS_LITERAL_CSTRING(QUERYKEY_PARENT "=");
    queryString += parents[i];
  }

  // tags
  const nsTArray<nsString>& tags = query->Tags();
  for (uint32_t i = 0; i < tags.Length(); ++i) {
    nsAutoCString escapedTag;
    if (!NS_Escape(NS_ConvertUTF16toUTF8(tags[i]), escapedTag, url_XAlphas))
      return NS_ERROR_OUT_OF_MEMORY;

    AppendAmpersandIfNonempty(queryString);
    queryString += NS_LITERAL_CSTRING(QUERYKEY_TAG "=");
    queryString += escapedTag;
  }
  AppendBoolKeyValueIfTrue(queryString, NS_LITERAL_CSTRING(QUERYKEY_NOTTAGS),
                           query, &nsINavHistoryQuery::GetTagsAreNot);

  // transitions
  const nsTArray<uint32_t>& transitions = query->Transitions();
  for (uint32_t i = 0; i < transitions.Length(); ++i) {
    AppendAmpersandIfNonempty(queryString);
    queryString += NS_LITERAL_CSTRING(QUERYKEY_TRANSITION "=");
    AppendInt64(queryString, transitions[i]);
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
  if (options->QueryType() != nsINavHistoryQueryOptions::QUERY_TYPE_HISTORY) {
    AppendAmpersandIfNonempty(queryString);
    queryString += NS_LITERAL_CSTRING(QUERYKEY_QUERY_TYPE "=");
    AppendInt16(queryString, options->QueryType());
  }

  // async enabled
  if (options->AsyncEnabled()) {
    AppendAmpersandIfNonempty(queryString);
    queryString += NS_LITERAL_CSTRING(QUERYKEY_ASYNC_ENABLED "=1");
  }

  aQueryString.AssignLiteral("place:");
  aQueryString.Append(queryString);
  return NS_OK;
}

nsresult nsNavHistory::TokensToQuery(const nsTArray<QueryKeyValuePair>& aTokens,
                                     nsNavHistoryQuery* aQuery,
                                     nsNavHistoryQueryOptions* aOptions) {
  nsresult rv;

  if (aTokens.Length() == 0) return NS_OK;

  nsTArray<nsCString> parents;
  nsTArray<nsString> tags;
  nsTArray<uint32_t> transitions;
  for (uint32_t i = 0; i < aTokens.Length(); i++) {
    const QueryKeyValuePair& kvp = aTokens[i];

    // begin time
    if (kvp.key.EqualsLiteral(QUERYKEY_BEGIN_TIME)) {
      SetQueryKeyInt64(kvp.value, aQuery, &nsINavHistoryQuery::SetBeginTime);

      // begin time reference
    } else if (kvp.key.EqualsLiteral(QUERYKEY_BEGIN_TIME_REFERENCE)) {
      SetQueryKeyUint32(kvp.value, aQuery,
                        &nsINavHistoryQuery::SetBeginTimeReference);

      // end time
    } else if (kvp.key.EqualsLiteral(QUERYKEY_END_TIME)) {
      SetQueryKeyInt64(kvp.value, aQuery, &nsINavHistoryQuery::SetEndTime);

      // end time reference
    } else if (kvp.key.EqualsLiteral(QUERYKEY_END_TIME_REFERENCE)) {
      SetQueryKeyUint32(kvp.value, aQuery,
                        &nsINavHistoryQuery::SetEndTimeReference);

      // search terms
    } else if (kvp.key.EqualsLiteral(QUERYKEY_SEARCH_TERMS)) {
      nsCString unescapedTerms = kvp.value;
      NS_UnescapeURL(unescapedTerms);  // modifies input
      rv = aQuery->SetSearchTerms(NS_ConvertUTF8toUTF16(unescapedTerms));
      NS_ENSURE_SUCCESS(rv, rv);

      // min visits
    } else if (kvp.key.EqualsLiteral(QUERYKEY_MIN_VISITS)) {
      int32_t visits = kvp.value.ToInteger(&rv);
      if (NS_SUCCEEDED(rv))
        aQuery->SetMinVisits(visits);
      else
        NS_WARNING("Bad number for minVisits in query");

      // max visits
    } else if (kvp.key.EqualsLiteral(QUERYKEY_MAX_VISITS)) {
      int32_t visits = kvp.value.ToInteger(&rv);
      if (NS_SUCCEEDED(rv))
        aQuery->SetMaxVisits(visits);
      else
        NS_WARNING("Bad number for maxVisits in query");

      // onlyBookmarked flag
    } else if (kvp.key.EqualsLiteral(QUERYKEY_ONLY_BOOKMARKED)) {
      SetQueryKeyBool(kvp.value, aQuery,
                      &nsINavHistoryQuery::SetOnlyBookmarked);

      // domainIsHost flag
    } else if (kvp.key.EqualsLiteral(QUERYKEY_DOMAIN_IS_HOST)) {
      SetQueryKeyBool(kvp.value, aQuery, &nsINavHistoryQuery::SetDomainIsHost);

      // domain string
    } else if (kvp.key.EqualsLiteral(QUERYKEY_DOMAIN)) {
      nsAutoCString unescapedDomain(kvp.value);
      NS_UnescapeURL(unescapedDomain);  // modifies input
      rv = aQuery->SetDomain(unescapedDomain);
      NS_ENSURE_SUCCESS(rv, rv);

      // parent folders (guids)
    } else if (kvp.key.EqualsLiteral(QUERYKEY_PARENT)) {
      parents.AppendElement(kvp.value);

      // uri
    } else if (kvp.key.EqualsLiteral(QUERYKEY_URI)) {
      nsAutoCString unescapedUri(kvp.value);
      NS_UnescapeURL(unescapedUri);  // modifies input
      nsCOMPtr<nsIURI> uri;
      nsresult rv = NS_NewURI(getter_AddRefs(uri), unescapedUri);
      if (NS_FAILED(rv)) {
        NS_WARNING("Unable to parse URI");
      }
      rv = aQuery->SetUri(uri);
      NS_ENSURE_SUCCESS(rv, rv);

      // not annotation
    } else if (kvp.key.EqualsLiteral(QUERYKEY_NOTANNOTATION)) {
      nsAutoCString unescaped(kvp.value);
      NS_UnescapeURL(unescaped);  // modifies input
      aQuery->SetAnnotationIsNot(true);
      aQuery->SetAnnotation(unescaped);

      // annotation
    } else if (kvp.key.EqualsLiteral(QUERYKEY_ANNOTATION)) {
      nsAutoCString unescaped(kvp.value);
      NS_UnescapeURL(unescaped);  // modifies input
      aQuery->SetAnnotationIsNot(false);
      aQuery->SetAnnotation(unescaped);

      // tag
    } else if (kvp.key.EqualsLiteral(QUERYKEY_TAG)) {
      nsAutoCString unescaped(kvp.value);
      NS_UnescapeURL(unescaped);  // modifies input
      NS_ConvertUTF8toUTF16 tag(unescaped);
      if (!tags.Contains(tag)) {
        tags.AppendElement(tag);
      }

      // not tags
    } else if (kvp.key.EqualsLiteral(QUERYKEY_NOTTAGS)) {
      SetQueryKeyBool(kvp.value, aQuery, &nsINavHistoryQuery::SetTagsAreNot);

      // transition
    } else if (kvp.key.EqualsLiteral(QUERYKEY_TRANSITION)) {
      uint32_t transition = kvp.value.ToInteger(&rv);
      if (NS_SUCCEEDED(rv)) {
        if (!transitions.Contains(transition))
          transitions.AppendElement(transition);
      } else {
        NS_WARNING("Invalid Int32 transition value.");
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

  if (parents.Length() != 0) {
    rv = aQuery->SetParents(parents);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (tags.Length() > 0) {
    aQuery->SetTags(std::move(tags));
  }

  if (transitions.Length() > 0) {
    rv = aQuery->SetTransitions(transitions);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

// ParseQueryBooleanString
//
//    Converts a 0/1 or true/false string into a bool

nsresult ParseQueryBooleanString(const nsCString& aString, bool* aValue) {
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

NS_IMPL_ISUPPORTS(nsNavHistoryQuery, nsNavHistoryQuery, nsINavHistoryQuery)

// nsINavHistoryQuery::nsNavHistoryQuery
//
//    This must initialize the object such that the default values will cause
//    all history to be returned if this query is used. Then the caller can
//    just set the things it's interested in.

nsNavHistoryQuery::nsNavHistoryQuery()
    : mMinVisits(-1),
      mMaxVisits(-1),
      mBeginTime(0),
      mBeginTimeReference(TIME_RELATIVE_EPOCH),
      mEndTime(0),
      mEndTimeReference(TIME_RELATIVE_EPOCH),
      mOnlyBookmarked(false),
      mDomainIsHost(false),
      mAnnotationIsNot(false),
      mTagsAreNot(false) {
  // differentiate not set (IsVoid) from empty string (local files)
  mDomain.SetIsVoid(true);
}

nsNavHistoryQuery::nsNavHistoryQuery(const nsNavHistoryQuery& aOther)
    : mMinVisits(aOther.mMinVisits),
      mMaxVisits(aOther.mMaxVisits),
      mBeginTime(aOther.mBeginTime),
      mBeginTimeReference(aOther.mBeginTimeReference),
      mEndTime(aOther.mEndTime),
      mEndTimeReference(aOther.mEndTimeReference),
      mSearchTerms(aOther.mSearchTerms),
      mOnlyBookmarked(aOther.mOnlyBookmarked),
      mDomainIsHost(aOther.mDomainIsHost),
      mDomain(aOther.mDomain),
      mUri(aOther.mUri),
      mAnnotationIsNot(aOther.mAnnotationIsNot),
      mAnnotation(aOther.mAnnotation),
      mParents(aOther.mParents),
      mTags(aOther.mTags),
      mTagsAreNot(aOther.mTagsAreNot),
      mTransitions(aOther.mTransitions) {}

NS_IMETHODIMP nsNavHistoryQuery::GetBeginTime(PRTime* aBeginTime) {
  *aBeginTime = mBeginTime;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetBeginTime(PRTime aBeginTime) {
  mBeginTime = aBeginTime;
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetBeginTimeReference(uint32_t* _retval) {
  *_retval = mBeginTimeReference;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetBeginTimeReference(uint32_t aReference) {
  if (aReference > TIME_RELATIVE_NOW) return NS_ERROR_INVALID_ARG;
  mBeginTimeReference = aReference;
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetHasBeginTime(bool* _retval) {
  *_retval = !(mBeginTimeReference == TIME_RELATIVE_EPOCH && mBeginTime == 0);
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetAbsoluteBeginTime(PRTime* _retval) {
  *_retval = nsNavHistory::NormalizeTime(mBeginTimeReference, mBeginTime);
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetEndTime(PRTime* aEndTime) {
  *aEndTime = mEndTime;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetEndTime(PRTime aEndTime) {
  mEndTime = aEndTime;
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetEndTimeReference(uint32_t* _retval) {
  *_retval = mEndTimeReference;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetEndTimeReference(uint32_t aReference) {
  if (aReference > TIME_RELATIVE_NOW) return NS_ERROR_INVALID_ARG;
  mEndTimeReference = aReference;
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetHasEndTime(bool* _retval) {
  *_retval = !(mEndTimeReference == TIME_RELATIVE_EPOCH && mEndTime == 0);
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetAbsoluteEndTime(PRTime* _retval) {
  *_retval = nsNavHistory::NormalizeTime(mEndTimeReference, mEndTime);
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetSearchTerms(nsAString& aSearchTerms) {
  aSearchTerms = mSearchTerms;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetSearchTerms(const nsAString& aSearchTerms) {
  mSearchTerms = aSearchTerms;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::GetHasSearchTerms(bool* _retval) {
  *_retval = (!mSearchTerms.IsEmpty());
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetMinVisits(int32_t* _retval) {
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = mMinVisits;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetMinVisits(int32_t aVisits) {
  mMinVisits = aVisits;
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetMaxVisits(int32_t* _retval) {
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = mMaxVisits;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetMaxVisits(int32_t aVisits) {
  mMaxVisits = aVisits;
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetOnlyBookmarked(bool* aOnlyBookmarked) {
  *aOnlyBookmarked = mOnlyBookmarked;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetOnlyBookmarked(bool aOnlyBookmarked) {
  mOnlyBookmarked = aOnlyBookmarked;
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetDomainIsHost(bool* aDomainIsHost) {
  *aDomainIsHost = mDomainIsHost;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetDomainIsHost(bool aDomainIsHost) {
  mDomainIsHost = aDomainIsHost;
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetDomain(nsACString& aDomain) {
  aDomain = mDomain;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetDomain(const nsACString& aDomain) {
  mDomain = aDomain;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::GetHasDomain(bool* _retval) {
  // note that empty but not void is still a valid query (local files)
  *_retval = (!mDomain.IsVoid());
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetUri(nsIURI** aUri) {
  NS_IF_ADDREF(*aUri = mUri);
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetUri(nsIURI* aUri) {
  mUri = aUri;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::GetHasUri(bool* aHasUri) {
  *aHasUri = (mUri != nullptr);
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetAnnotationIsNot(bool* aIsNot) {
  *aIsNot = mAnnotationIsNot;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetAnnotationIsNot(bool aIsNot) {
  mAnnotationIsNot = aIsNot;
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetAnnotation(nsACString& aAnnotation) {
  aAnnotation = mAnnotation;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::SetAnnotation(const nsACString& aAnnotation) {
  mAnnotation = aAnnotation;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryQuery::GetHasAnnotation(bool* aHasIt) {
  *aHasIt = !mAnnotation.IsEmpty();
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetTags(nsIVariant** aTags) {
  NS_ENSURE_ARG_POINTER(aTags);

  RefPtr<nsVariant> out = new nsVariant();

  uint32_t arrayLen = mTags.Length();

  nsresult rv;
  if (arrayLen == 0)
    rv = out->SetAsEmptyArray();
  else {
    // Note: The resulting nsIVariant dupes both the array and its elements.
    const char16_t** array = reinterpret_cast<const char16_t**>(
        moz_xmalloc(arrayLen * sizeof(char16_t*)));
    for (uint32_t i = 0; i < arrayLen; ++i) {
      array[i] = mTags[i].get();
    }

    rv = out->SetAsArray(nsIDataType::VTYPE_WCHAR_STR, nullptr, arrayLen,
                         reinterpret_cast<void*>(array));
    free(array);
  }
  NS_ENSURE_SUCCESS(rv, rv);

  out.forget(aTags);
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::SetTags(nsIVariant* aTags) {
  NS_ENSURE_ARG(aTags);

  uint16_t dataType = aTags->GetDataType();

  // Caller passed in empty array.  Easy -- clear our mTags array and return.
  if (dataType == nsIDataType::VTYPE_EMPTY_ARRAY) {
    mTags.Clear();
    return NS_OK;
  }

  // Before we go any further, make sure caller passed in an array.
  NS_ENSURE_TRUE(dataType == nsIDataType::VTYPE_ARRAY, NS_ERROR_ILLEGAL_VALUE);

  uint16_t eltType;
  nsIID eltIID;
  uint32_t arrayLen;
  void* array;

  // Convert the nsIVariant to an array.  We own the resulting buffer and its
  // elements.
  nsresult rv = aTags->GetAsArray(&eltType, &eltIID, &arrayLen, &array);
  NS_ENSURE_SUCCESS(rv, rv);

  // If element type is not wstring, thanks a lot.  Your memory die now.
  if (eltType != nsIDataType::VTYPE_WCHAR_STR) {
    switch (eltType) {
      case nsIDataType::VTYPE_ID:
      case nsIDataType::VTYPE_CHAR_STR: {
        char** charArray = reinterpret_cast<char**>(array);
        for (uint32_t i = 0; i < arrayLen; ++i) {
          if (charArray[i]) free(charArray[i]);
        }
      } break;
      case nsIDataType::VTYPE_INTERFACE:
      case nsIDataType::VTYPE_INTERFACE_IS: {
        nsISupports** supportsArray = reinterpret_cast<nsISupports**>(array);
        for (uint32_t i = 0; i < arrayLen; ++i) {
          NS_IF_RELEASE(supportsArray[i]);
        }
      } break;
        // The other types are primitives that do not need to be freed.
    }
    free(array);
    return NS_ERROR_ILLEGAL_VALUE;
  }

  char16_t** tags = reinterpret_cast<char16_t**>(array);
  mTags.Clear();

  // Finally, add each passed-in tag to our mTags array and then sort it.
  for (uint32_t i = 0; i < arrayLen; ++i) {
    // Don't allow nulls.
    if (!tags[i]) {
      free(tags);
      return NS_ERROR_ILLEGAL_VALUE;
    }

    nsDependentString tag(tags[i]);

    // Don't store duplicate tags.  This isn't just to save memory or to be
    // fancy; the SQL that's built from the tags relies on no dupes.
    if (!mTags.Contains(tag)) {
      // XXX(Bug 1631371) Check if this should use a fallible operation as it
      // pretended earlier.
      mTags.AppendElement(tag);
    }
    free(tags[i]);
  }
  free(tags);

  mTags.Sort();

  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetTagsAreNot(bool* aTagsAreNot) {
  NS_ENSURE_ARG_POINTER(aTagsAreNot);
  *aTagsAreNot = mTagsAreNot;
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::SetTagsAreNot(bool aTagsAreNot) {
  mTagsAreNot = aTagsAreNot;
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetParents(nsTArray<nsCString>& aGuids) {
  aGuids = mParents;
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetParentCount(uint32_t* aGuidCount) {
  *aGuidCount = mParents.Length();
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::SetParents(const nsTArray<nsCString>& aGuids) {
  mParents.Clear();
  if (!mParents.Assign(aGuids, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetTransitions(
    nsTArray<uint32_t>& aTransitions) {
  aTransitions = mTransitions;
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::GetTransitionCount(uint32_t* aCount) {
  *aCount = mTransitions.Length();
  return NS_OK;
}

NS_IMETHODIMP nsNavHistoryQuery::SetTransitions(
    const nsTArray<uint32_t>& aTransitions) {
  if (!mTransitions.Assign(aTransitions, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNavHistoryQuery::Clone(nsINavHistoryQuery** _clone) {
  nsNavHistoryQuery* clone = nullptr;
  Unused << Clone(&clone);
  *_clone = clone;
  return NS_OK;
}

nsresult nsNavHistoryQuery::Clone(nsNavHistoryQuery** _clone) {
  *_clone = nullptr;
  RefPtr<nsNavHistoryQuery> clone = new nsNavHistoryQuery(*this);
  clone.forget(_clone);
  return NS_OK;
}

// nsNavHistoryQueryOptions
NS_IMPL_ISUPPORTS(nsNavHistoryQueryOptions, nsNavHistoryQueryOptions,
                  nsINavHistoryQueryOptions)

nsNavHistoryQueryOptions::nsNavHistoryQueryOptions()
    : mSort(0),
      mResultType(0),
      mExcludeItems(false),
      mExcludeQueries(false),
      mExpandQueries(true),
      mIncludeHidden(false),
      mMaxResults(0),
      mQueryType(nsINavHistoryQueryOptions::QUERY_TYPE_HISTORY),
      mAsyncEnabled(false) {}

nsNavHistoryQueryOptions::nsNavHistoryQueryOptions(
    const nsNavHistoryQueryOptions& other)
    : mSort(other.mSort),
      mResultType(other.mResultType),
      mExcludeItems(other.mExcludeItems),
      mExcludeQueries(other.mExcludeQueries),
      mExpandQueries(other.mExpandQueries),
      mIncludeHidden(other.mIncludeHidden),
      mMaxResults(other.mMaxResults),
      mQueryType(other.mQueryType),
      mAsyncEnabled(other.mAsyncEnabled) {}

// sortingMode
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetSortingMode(uint16_t* aMode) {
  *aMode = mSort;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetSortingMode(uint16_t aMode) {
  if (aMode > SORT_BY_FRECENCY_DESCENDING) return NS_ERROR_INVALID_ARG;
  mSort = aMode;
  return NS_OK;
}

// resultType
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetResultType(uint16_t* aType) {
  *aType = mResultType;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetResultType(uint16_t aType) {
  if (aType > RESULTS_AS_LEFT_PANE_QUERY) return NS_ERROR_INVALID_ARG;
  // Tag queries, containers and the roots query are bookmarks related, so we
  // set the QueryType accordingly.
  if (aType == RESULTS_AS_TAGS_ROOT || aType == RESULTS_AS_ROOTS_QUERY ||
      aType == RESULTS_AS_LEFT_PANE_QUERY) {
    mQueryType = QUERY_TYPE_BOOKMARKS;
  }
  mResultType = aType;
  return NS_OK;
}

// excludeItems
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetExcludeItems(bool* aExclude) {
  *aExclude = mExcludeItems;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetExcludeItems(bool aExclude) {
  mExcludeItems = aExclude;
  return NS_OK;
}

// excludeQueries
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetExcludeQueries(bool* aExclude) {
  *aExclude = mExcludeQueries;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetExcludeQueries(bool aExclude) {
  mExcludeQueries = aExclude;
  return NS_OK;
}
// expandQueries
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetExpandQueries(bool* aExpand) {
  *aExpand = mExpandQueries;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetExpandQueries(bool aExpand) {
  mExpandQueries = aExpand;
  return NS_OK;
}

// includeHidden
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetIncludeHidden(bool* aIncludeHidden) {
  *aIncludeHidden = mIncludeHidden;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetIncludeHidden(bool aIncludeHidden) {
  mIncludeHidden = aIncludeHidden;
  return NS_OK;
}

// maxResults
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetMaxResults(uint32_t* aMaxResults) {
  *aMaxResults = mMaxResults;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetMaxResults(uint32_t aMaxResults) {
  mMaxResults = aMaxResults;
  return NS_OK;
}

// queryType
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetQueryType(uint16_t* _retval) {
  *_retval = mQueryType;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetQueryType(uint16_t aQueryType) {
  // Tag query and containers are forced to QUERY_TYPE_BOOKMARKS when the
  // resultType is set.
  if (mResultType == RESULTS_AS_TAGS_ROOT ||
      mResultType == RESULTS_AS_LEFT_PANE_QUERY ||
      mResultType == RESULTS_AS_ROOTS_QUERY)
    return NS_OK;
  mQueryType = aQueryType;
  return NS_OK;
}

// asyncEnabled
NS_IMETHODIMP
nsNavHistoryQueryOptions::GetAsyncEnabled(bool* _asyncEnabled) {
  *_asyncEnabled = mAsyncEnabled;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryOptions::SetAsyncEnabled(bool aAsyncEnabled) {
  mAsyncEnabled = aAsyncEnabled;
  return NS_OK;
}

NS_IMETHODIMP
nsNavHistoryQueryOptions::Clone(nsINavHistoryQueryOptions** _clone) {
  nsNavHistoryQueryOptions* clone = nullptr;
  Unused << Clone(&clone);
  *_clone = clone;
  return NS_OK;
}

nsresult nsNavHistoryQueryOptions::Clone(nsNavHistoryQueryOptions** _clone) {
  *_clone = nullptr;
  RefPtr<nsNavHistoryQueryOptions> clone = new nsNavHistoryQueryOptions(*this);
  clone.forget(_clone);
  return NS_OK;
}

// AppendBoolKeyValueIfTrue

void  // static
AppendBoolKeyValueIfTrue(nsACString& aString, const nsCString& aName,
                         nsINavHistoryQuery* aQuery, BoolQueryGetter getter) {
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

void  // static
AppendUint32KeyValueIfNonzero(nsACString& aString, const nsCString& aName,
                              nsINavHistoryQuery* aQuery,
                              Uint32QueryGetter getter) {
  uint32_t value;
  DebugOnly<nsresult> rv = (aQuery->*getter)(&value);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Failure getting value");
  if (value) {
    AppendAmpersandIfNonempty(aString);
    aString += aName;

    // AppendInt requires a concrete string
    nsAutoCString appendMe("=");
    appendMe.AppendInt(value);
    aString.Append(appendMe);
  }
}

// AppendInt64KeyValueIfNonzero

void  // static
AppendInt64KeyValueIfNonzero(nsACString& aString, const nsCString& aName,
                             nsINavHistoryQuery* aQuery,
                             Int64QueryGetter getter) {
  PRTime value;
  DebugOnly<nsresult> rv = (aQuery->*getter)(&value);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Failure getting value");
  if (value) {
    AppendAmpersandIfNonempty(aString);
    aString += aName;
    nsAutoCString appendMe("=");
    appendMe.AppendInt(static_cast<int64_t>(value));
    aString.Append(appendMe);
  }
}

// SetQuery/OptionsKeyBool

void  // static
SetQueryKeyBool(const nsCString& aValue, nsINavHistoryQuery* aQuery,
                BoolQuerySetter setter) {
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
void  // static
SetOptionsKeyBool(const nsCString& aValue, nsINavHistoryQueryOptions* aOptions,
                  BoolOptionsSetter setter) {
  bool value = false;
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

void  // static
SetQueryKeyUint32(const nsCString& aValue, nsINavHistoryQuery* aQuery,
                  Uint32QuerySetter setter) {
  nsresult rv;
  uint32_t value = aValue.ToInteger(&rv);
  if (NS_SUCCEEDED(rv)) {
    rv = (aQuery->*setter)(value);
    if (NS_FAILED(rv)) {
      NS_WARNING("Error setting Int32 key value");
    }
  } else {
    NS_WARNING("Invalid Int32 key value in query string.");
  }
}
void  // static
SetOptionsKeyUint32(const nsCString& aValue,
                    nsINavHistoryQueryOptions* aOptions,
                    Uint32OptionsSetter setter) {
  nsresult rv;
  uint32_t value = aValue.ToInteger(&rv);
  if (NS_SUCCEEDED(rv)) {
    rv = (aOptions->*setter)(value);
    if (NS_FAILED(rv)) {
      NS_WARNING("Error setting Int32 key value");
    }
  } else {
    NS_WARNING("Invalid Int32 key value in query string.");
  }
}

void  // static
SetOptionsKeyUint16(const nsCString& aValue,
                    nsINavHistoryQueryOptions* aOptions,
                    Uint16OptionsSetter setter) {
  nsresult rv;
  uint16_t value = static_cast<uint16_t>(aValue.ToInteger(&rv));
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
                      Int64QuerySetter setter) {
  nsresult rv;
  int64_t value;
  if (PR_sscanf(aValue.get(), "%lld", &value) == 1) {
    rv = (aQuery->*setter)(value);
    if (NS_FAILED(rv)) {
      NS_WARNING("Error setting Int64 key value");
    }
  } else {
    NS_WARNING("Invalid Int64 value in query string.");
  }
}
