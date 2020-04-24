/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * The definitions of nsNavHistoryQuery and nsNavHistoryQueryOptions. This
 * header file should only be included from nsNavHistory.h, include that if
 * you want these classes.
 */

#ifndef nsNavHistoryQuery_h_
#define nsNavHistoryQuery_h_

// nsNavHistoryQuery
//
//    This class encapsulates the parameters for basic history queries for
//    building UI, trees, lists, etc.

#include "mozilla/Attributes.h"

#define NS_NAVHISTORYQUERY_IID                       \
  {                                                  \
    0xb10185e0, 0x86eb, 0x4612, {                    \
      0x95, 0x7c, 0x09, 0x34, 0xf2, 0xb1, 0xce, 0xd7 \
    }                                                \
  }

class nsNavHistoryQuery final : public nsINavHistoryQuery {
 public:
  nsNavHistoryQuery();
  nsNavHistoryQuery(const nsNavHistoryQuery& aOther);

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NAVHISTORYQUERY_IID)
  NS_DECL_ISUPPORTS
  NS_DECL_NSINAVHISTORYQUERY

  int32_t MinVisits() { return mMinVisits; }
  int32_t MaxVisits() { return mMaxVisits; }
  PRTime BeginTime() { return mBeginTime; }
  uint32_t BeginTimeReference() { return mBeginTimeReference; }
  PRTime EndTime() { return mEndTime; }
  uint32_t EndTimeReference() { return mEndTimeReference; }
  const nsString& SearchTerms() { return mSearchTerms; }
  bool OnlyBookmarked() { return mOnlyBookmarked; }
  bool DomainIsHost() { return mDomainIsHost; }
  const nsCString& Domain() { return mDomain; }
  nsIURI* Uri() { return mUri; }  // NOT AddRef-ed!
  bool AnnotationIsNot() { return mAnnotationIsNot; }
  const nsCString& Annotation() { return mAnnotation; }
  const nsTArray<nsCString>& Parents() const { return mParents; }

  const nsTArray<nsString>& Tags() const { return mTags; }
  void SetTags(nsTArray<nsString> aTags) { mTags = std::move(aTags); }
  bool TagsAreNot() { return mTagsAreNot; }

  const nsTArray<uint32_t>& Transitions() const { return mTransitions; }

  nsresult Clone(nsNavHistoryQuery** _clone);

 private:
  ~nsNavHistoryQuery() = default;

 protected:
  // IF YOU ADD MORE ITEMS:
  //  * Add to the copy constructor
  int32_t mMinVisits;
  int32_t mMaxVisits;
  PRTime mBeginTime;
  uint32_t mBeginTimeReference;
  PRTime mEndTime;
  uint32_t mEndTimeReference;
  nsString mSearchTerms;
  bool mOnlyBookmarked;
  bool mDomainIsHost;
  nsCString mDomain;  // Default is IsVoid, empty string is valid query
  nsCOMPtr<nsIURI> mUri;
  bool mAnnotationIsNot;
  nsCString mAnnotation;
  nsTArray<nsCString> mParents;
  nsTArray<nsString> mTags;
  bool mTagsAreNot;
  nsTArray<uint32_t> mTransitions;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsNavHistoryQuery, NS_NAVHISTORYQUERY_IID)

// nsNavHistoryQueryOptions

#define NS_NAVHISTORYQUERYOPTIONS_IID                \
  {                                                  \
    0x95f8ba3b, 0xd681, 0x4d89, {                    \
      0xab, 0xd1, 0xfd, 0xae, 0xf2, 0xa3, 0xde, 0x18 \
    }                                                \
  }

class nsNavHistoryQueryOptions final : public nsINavHistoryQueryOptions {
 public:
  nsNavHistoryQueryOptions();
  nsNavHistoryQueryOptions(const nsNavHistoryQueryOptions& other);

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NAVHISTORYQUERYOPTIONS_IID)

  NS_DECL_ISUPPORTS
  NS_DECL_NSINAVHISTORYQUERYOPTIONS

  uint16_t SortingMode() const { return mSort; }
  uint16_t ResultType() const { return mResultType; }
  bool ExcludeItems() const { return mExcludeItems; }
  bool ExcludeQueries() const { return mExcludeQueries; }
  bool ExpandQueries() const { return mExpandQueries; }
  bool IncludeHidden() const { return mIncludeHidden; }
  uint32_t MaxResults() const { return mMaxResults; }
  uint16_t QueryType() const { return mQueryType; }
  bool AsyncEnabled() const { return mAsyncEnabled; }

  nsresult Clone(nsNavHistoryQueryOptions** _clone);

 private:
  ~nsNavHistoryQueryOptions() = default;

  // IF YOU ADD MORE ITEMS:
  //  * Add to the copy constructor
  //  * Add a new getter for C++ above if it makes sense
  //  * Add to the serialization code (see nsNavHistory::QueriesToQueryString())
  //  * Add to the deserialization code (see nsNavHistory::QueryStringToQueries)
  //  * Add to the nsNavHistory.cpp::GetSimpleBookmarksQueryFolder function if
  //  applicable
  uint16_t mSort;
  uint16_t mResultType;
  bool mExcludeItems;
  bool mExcludeQueries;
  bool mExpandQueries;
  bool mIncludeHidden;
  uint32_t mMaxResults;
  uint16_t mQueryType;
  bool mAsyncEnabled;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsNavHistoryQueryOptions,
                              NS_NAVHISTORYQUERYOPTIONS_IID)

#endif  // nsNavHistoryQuery_h_
