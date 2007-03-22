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

#ifdef MOZILLA_1_8_BRANCH
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_NAVHISTORYQUERY_IID)
#else
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NAVHISTORYQUERY_IID)
#endif
  NS_DECL_ISUPPORTS
  NS_DECL_NSINAVHISTORYQUERY

  PRInt32 MinVisits() { return mMinVisits; }
  PRInt32 MaxVisits() { return mMaxVisits; }
  PRTime BeginTime() { return mBeginTime; }
  PRUint32 BeginTimeReference() { return mBeginTimeReference; }
  PRTime EndTime() { return mEndTime; }
  PRUint32 EndTimeReference() { return mEndTimeReference; }
  const nsString& SearchTerms() { return mSearchTerms; }
  PRBool OnlyBookmarked() { return mOnlyBookmarked; }
  PRBool DomainIsHost() { return mDomainIsHost; }
  const nsCString& Domain() { return mDomain; }
  PRBool UriIsPrefix() { return mUriIsPrefix; }
  nsIURI* Uri() { return mUri; } // NOT AddRef-ed!
  PRBool AnnotationIsNot() { return mAnnotationIsNot; }
  const nsCString& Annotation() { return mAnnotation; }
  const nsTArray<PRInt64>& Folders() const { return mFolders; }

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
  PRBool mOnlyBookmarked;
  PRBool mDomainIsHost;
  nsCString mDomain; // Default is IsVoid, empty string is valid query
  PRBool mUriIsPrefix;
  nsCOMPtr<nsIURI> mUri;
  PRBool mAnnotationIsNot;
  nsCString mAnnotation;
  nsTArray<PRInt64> mFolders;
};

#ifndef MOZILLA_1_8_BRANCH
NS_DEFINE_STATIC_IID_ACCESSOR(nsNavHistoryQuery, NS_NAVHISTORYQUERY_IID)
#endif

// nsNavHistoryQueryOptions

#define NS_NAVHISTORYQUERYOPTIONS_IID \
{0x95f8ba3b, 0xd681, 0x4d89, {0xab, 0xd1, 0xfd, 0xae, 0xf2, 0xa3, 0xde, 0x18}}

class nsNavHistoryQueryOptions : public nsINavHistoryQueryOptions
{
public:
  nsNavHistoryQueryOptions() : mSort(0), mResultType(0),
                               mGroupCount(0), mGroupings(nsnull),
                               mExcludeItems(PR_FALSE),
                               mExcludeQueries(PR_FALSE),
                               mExcludeReadOnlyFolders(PR_FALSE),
                               mExpandQueries(PR_FALSE),
                               mForceOriginalTitle(PR_FALSE),
                               mIncludeHidden(PR_FALSE),
                               mShowSessions(PR_FALSE),
                               mMaxResults(0)
  { }

#ifdef MOZILLA_1_8_BRANCH
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_NAVHISTORYQUERYOPTIONS_IID)
#else
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NAVHISTORYQUERYOPTIONS_IID)
#endif

  NS_DECL_ISUPPORTS
  NS_DECL_NSINAVHISTORYQUERYOPTIONS

  PRUint32 SortingMode() const { return mSort; }
  PRUint32 ResultType() const { return mResultType; }
  const PRUint32* GroupingMode(PRUint32 *count) const {
    *count = mGroupCount; return mGroupings;
  }
  PRBool ExcludeItems() const { return mExcludeItems; }
  PRBool ExcludeQueries() const { return mExcludeQueries; }
  PRBool ExcludeReadOnlyFolders() const { return mExcludeReadOnlyFolders; }
  PRBool ExpandQueries() const { return mExpandQueries; }
  PRBool ForceOriginalTitle() const { return mForceOriginalTitle; }
  PRBool IncludeHidden() const { return mIncludeHidden; }
  PRBool ShowSessions() const { return mShowSessions; }
  PRUint32 MaxResults() const { return mMaxResults; }

  nsresult Clone(nsNavHistoryQueryOptions **aResult);

private:
  nsNavHistoryQueryOptions(const nsNavHistoryQueryOptions& other) {} // no copy

  ~nsNavHistoryQueryOptions() { delete[] mGroupings; }

  // IF YOU ADD MORE ITEMS:
  //  * Add a new getter for C++ above if it makes sense
  //  * Add to the serialization code (see nsNavHistory::QueriesToQueryString())
  //  * Add to the deserialization code (see nsNavHistory::QueryStringToQueries)
  //  * Add to the nsNavHistoryQueryOptions::Clone() function
  //  * Add to the nsNavHistory.cpp::GetSimpleBookmarksQueryFolder function if applicable
  PRUint32 mSort;
  PRUint32 mResultType;
  PRUint32 mGroupCount;
  PRUint32 *mGroupings;
  PRBool mExcludeItems;
  PRBool mExcludeQueries;
  PRBool mExcludeReadOnlyFolders;
  PRBool mExpandQueries;
  PRBool mForceOriginalTitle;
  PRBool mIncludeHidden;
  PRBool mShowSessions;
  PRUint32 mMaxResults;
};

#ifndef MOZILLA_1_8_BRANCH
NS_DEFINE_STATIC_IID_ACCESSOR(nsNavHistoryQueryOptions, NS_NAVHISTORYQUERYOPTIONS_IID)
#endif

#endif // nsNavHistoryQuery_h_

