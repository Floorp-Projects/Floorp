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

class nsNavHistoryQuery : public nsINavHistoryQuery
{
public:
  nsNavHistoryQuery();
  // note: we use a copy constructor in Clone(), the default is good enough

  NS_DECL_ISUPPORTS
  NS_DECL_NSINAVHISTORYQUERY

  const nsTArray<PRInt64>& Folders() const { return mFolders; }

private:
  ~nsNavHistoryQuery() {}

protected:

  PRTime mBeginTime;
  PRUint32 mBeginTimeReference;
  PRTime mEndTime;
  PRUint32 mEndTimeReference;
  nsString mSearchTerms;
  PRBool mOnlyBookmarked;
  PRBool mDomainIsHost;
  nsString mDomain;
  nsTArray<PRInt64> mFolders;
  PRUint32 mItemTypes;
};


// nsNavHistoryQueryOptions

#define NS_NAVHISTORYQUERYOPTIONS_IID \
{0x95f8ba3b, 0xd681, 0x4d89, {0xab, 0xd1, 0xfd, 0xae, 0xf2, 0xa3, 0xde, 0x18}}

class nsNavHistoryQueryOptions : public nsINavHistoryQueryOptions
{
public:
  nsNavHistoryQueryOptions() : mSort(0), mResultType(0),
                               mGroupCount(0), mGroupings(nsnull),
                               mExpandPlaces(PR_FALSE),
                               mForceOriginalTitle(PR_FALSE),
                               mIncludeHidden(PR_FALSE),
                               mMaxResults(0)
  { }

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_NAVHISTORYQUERYOPTIONS_IID)

  NS_DECL_ISUPPORTS
  NS_DECL_NSINAVHISTORYQUERYOPTIONS

  PRInt32 SortingMode() const { return mSort; }
  PRInt32 ResultType() const { return mResultType; }
  const PRInt32* GroupingMode(PRUint32 *count) const {
    *count = mGroupCount; return mGroupings;
  }
  PRBool ExpandPlaces() const { return mExpandPlaces; }
  PRBool ForceOriginalTitle() const { return mForceOriginalTitle; }
  PRBool IncludeHidden() const { return mIncludeHidden; }
  PRUint32 MaxResults() const { return mMaxResults; }

  nsresult Clone(nsNavHistoryQueryOptions **aResult);

private:
  nsNavHistoryQueryOptions(const nsNavHistoryQueryOptions& other) {} // no copy

  ~nsNavHistoryQueryOptions() { delete[] mGroupings; }

  PRInt32 mSort;
  PRInt32 mResultType;
  PRUint32 mGroupCount;
  PRInt32 *mGroupings;
  PRBool mExpandPlaces;
  PRBool mForceOriginalTitle;
  PRBool mIncludeHidden;
  PRUint32 mMaxResults;
};

#endif // nsNavHistoryQuery_h_

