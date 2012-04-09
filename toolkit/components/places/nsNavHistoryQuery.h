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
