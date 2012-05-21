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

#define NS_NAVHISTORYQUERY_IID \
{ 0xb10185e0, 0x86eb, 0x4612, { 0x95, 0x7c, 0x09, 0x34, 0xf2, 0xb1, 0xce, 0xd7 } }

class nsNavHistoryQuery : public nsINavHistoryQuery
{
public:
  nsNavHistoryQuery();
  // note: we use a copy constructor in Clone(), the default is good enough

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NAVHISTORYQUERY_IID)
  NS_DECL_ISUPPORTS
  NS_DECL_NSINAVHISTORYQUERY

  PRInt32 MinVisits() { return mMinVisits; }
  PRInt32 MaxVisits() { return mMaxVisits; }
  PRTime BeginTime() { return mBeginTime; }
  PRUint32 BeginTimeReference() { return mBeginTimeReference; }
  PRTime EndTime() { return mEndTime; }
  PRUint32 EndTimeReference() { return mEndTimeReference; }
  const nsString& SearchTerms() { return mSearchTerms; }
  bool OnlyBookmarked() { return mOnlyBookmarked; }
  bool DomainIsHost() { return mDomainIsHost; }
  const nsCString& Domain() { return mDomain; }
  bool UriIsPrefix() { return mUriIsPrefix; }
  nsIURI* Uri() { return mUri; } // NOT AddRef-ed!
  bool AnnotationIsNot() { return mAnnotationIsNot; }
  const nsCString& Annotation() { return mAnnotation; }
  const nsTArray<PRInt64>& Folders() const { return mFolders; }
  const nsTArray<nsString>& Tags() const { return mTags; }
  nsresult SetTags(const nsTArray<nsString>& aTags)
  {
    if (!mTags.ReplaceElementsAt(0, mTags.Length(), aTags))
      return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
  }
  bool TagsAreNot() { return mTagsAreNot; }

  const nsTArray<PRUint32>& Transitions() const { return mTransitions; }
  nsresult SetTransitions(const nsTArray<PRUint32>& aTransitions)
  {
    if (!mTransitions.ReplaceElementsAt(0, mTransitions.Length(),
                                        aTransitions))
      return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
  }

private:
  ~nsNavHistoryQuery() {}

protected:

  PRInt32 mMinVisits;
  PRInt32 mMaxVisits;
  PRTime mBeginTime;
  PRUint32 mBeginTimeReference;
  PRTime mEndTime;
  PRUint32 mEndTimeReference;
  nsString mSearchTerms;
  bool mOnlyBookmarked;
  bool mDomainIsHost;
  nsCString mDomain; // Default is IsVoid, empty string is valid query
  bool mUriIsPrefix;
  nsCOMPtr<nsIURI> mUri;
  bool mAnnotationIsNot;
  nsCString mAnnotation;
  nsTArray<PRInt64> mFolders;
  nsTArray<nsString> mTags;
  bool mTagsAreNot;
  nsTArray<PRUint32> mTransitions;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsNavHistoryQuery, NS_NAVHISTORYQUERY_IID)

// nsNavHistoryQueryOptions

#define NS_NAVHISTORYQUERYOPTIONS_IID \
{0x95f8ba3b, 0xd681, 0x4d89, {0xab, 0xd1, 0xfd, 0xae, 0xf2, 0xa3, 0xde, 0x18}}

class nsNavHistoryQueryOptions : public nsINavHistoryQueryOptions
{
public:
  nsNavHistoryQueryOptions()
  : mSort(0)
  , mResultType(0)
  , mExcludeItems(false)
  , mExcludeQueries(false)
  , mExcludeReadOnlyFolders(false)
  , mExpandQueries(true)
  , mIncludeHidden(false)
  , mMaxResults(0)
  , mQueryType(nsINavHistoryQueryOptions::QUERY_TYPE_HISTORY)
  , mAsyncEnabled(false)
  { }

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NAVHISTORYQUERYOPTIONS_IID)

  NS_DECL_ISUPPORTS
  NS_DECL_NSINAVHISTORYQUERYOPTIONS

  PRUint16 SortingMode() const { return mSort; }
  PRUint16 ResultType() const { return mResultType; }
  bool ExcludeItems() const { return mExcludeItems; }
  bool ExcludeQueries() const { return mExcludeQueries; }
  bool ExcludeReadOnlyFolders() const { return mExcludeReadOnlyFolders; }
  bool ExpandQueries() const { return mExpandQueries; }
  bool IncludeHidden() const { return mIncludeHidden; }
  PRUint32 MaxResults() const { return mMaxResults; }
  PRUint16 QueryType() const { return mQueryType; }
  bool AsyncEnabled() const { return mAsyncEnabled; }

  nsresult Clone(nsNavHistoryQueryOptions **aResult);

private:
  nsNavHistoryQueryOptions(const nsNavHistoryQueryOptions& other) {} // no copy

  // IF YOU ADD MORE ITEMS:
  //  * Add a new getter for C++ above if it makes sense
  //  * Add to the serialization code (see nsNavHistory::QueriesToQueryString())
  //  * Add to the deserialization code (see nsNavHistory::QueryStringToQueries)
  //  * Add to the nsNavHistoryQueryOptions::Clone() function
  //  * Add to the nsNavHistory.cpp::GetSimpleBookmarksQueryFolder function if applicable
  PRUint16 mSort;
  nsCString mSortingAnnotation;
  nsCString mParentAnnotationToExclude;
  PRUint16 mResultType;
  bool mExcludeItems;
  bool mExcludeQueries;
  bool mExcludeReadOnlyFolders;
  bool mExpandQueries;
  bool mIncludeHidden;
  PRUint32 mMaxResults;
  PRUint16 mQueryType;
  bool mAsyncEnabled;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsNavHistoryQueryOptions, NS_NAVHISTORYQUERYOPTIONS_IID)

#endif // nsNavHistoryQuery_h_
