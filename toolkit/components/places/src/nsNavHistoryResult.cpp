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
 * The Original Code is Mozilla History System
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


#include <stdio.h>
#include "nsNavHistory.h"
#include "nsNavBookmarks.h"

#include "nsArray.h"
#include "nsCollationCID.h"
#include "nsCOMPtr.h"
#include "nsDateTimeFormatCID.h"
#include "nsDebug.h"
#include "nsIComponentManager.h"
#include "nsIDateTimeFormat.h"
#include "nsIDOMElement.h"
#include "nsILocale.h"
#include "nsILocaleService.h"
#include "nsILocalFile.h"
#include "nsIServiceManager.h"
#include "nsISupportsPrimitives.h"
#include "nsITreeColumns.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsPromiseFlatString.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "prtime.h"
#include "prprf.h"

// emulate string comparison (used for sorting) for PRTime and int
inline PRInt32 ComparePRTime(PRTime a, PRTime b)
{
  if (LL_CMP(a, <, b))
    return -1;
  else if (LL_CMP(a, >, b))
    return 1;
  return 0;
}
inline PRInt32 CompareIntegers(PRUint32 a, PRUint32 b)
{
  return a - b;
}


// nsNavHistoryResultNode ******************************************************

NS_IMPL_ISUPPORTS2(nsNavHistoryResultNode,
                   nsNavHistoryResultNode, nsINavHistoryResultNode)

nsNavHistoryResultNode::nsNavHistoryResultNode() : mID(0), mExpanded(PR_FALSE)
{
}

/* attribute nsINavHistoryResultNode parent */
NS_IMETHODIMP nsNavHistoryResultNode::GetParent(nsINavHistoryResultNode** parent)
{
  NS_IF_ADDREF(*parent = mParent);
  return NS_OK;
}

/* attribute PRInt32 type; */
NS_IMETHODIMP nsNavHistoryResultNode::GetType(PRUint32 *aType)
{
  *aType = mType;
  return NS_OK;
}

/* attribute string url; */
NS_IMETHODIMP nsNavHistoryResultNode::GetUrl(nsACString& aUrl)
{
  aUrl = mUrl;
  return NS_OK;
}

/* attribute PRInt64 folderId; */
NS_IMETHODIMP nsNavHistoryResultNode::GetFolderId(PRInt64 *aID)
{
  *aID = mType == nsINavHistoryResult::RESULT_TYPE_FOLDER ? mID : 0;
  return NS_OK;
}

/* void getQueries(out unsigned long queryCount,
                   [retval,array,size_is(queryCount)] out nsINavHistoryQuery queries); */
NS_IMETHODIMP nsNavHistoryResultNode::GetQueries(PRUint32 *aQueryCount,
                                                 nsINavHistoryQuery ***aQueries)
{
  return NS_ERROR_NOT_AVAILABLE;
}

/* readonly attribute nsINavHistoryQueryOptions options */                   
NS_IMETHODIMP nsNavHistoryResultNode::GetQueryOptions(nsINavHistoryQueryOptions **aOptions)
{
  return NS_ERROR_NOT_AVAILABLE;
}

/* attribute string title; */
NS_IMETHODIMP nsNavHistoryResultNode::GetTitle(nsAString& aTitle)
{
  aTitle = mTitle;
  return NS_OK;
}

/* attribute PRInt32 accessCount; */
NS_IMETHODIMP nsNavHistoryResultNode::GetAccessCount(PRInt32 *aAccessCount)
{
  *aAccessCount = mAccessCount;
  return NS_OK;
}

/* attribute PRTime time; */
NS_IMETHODIMP nsNavHistoryResultNode::GetTime(PRTime *aTime)
{
  *aTime = mTime;
  return NS_OK;
}

/* attribute pRInt32 indentLevel; */
NS_IMETHODIMP nsNavHistoryResultNode::GetIndentLevel(PRInt32* aIndentLevel)
{
  *aIndentLevel = mIndentLevel;
  return NS_OK;
}

/* attribute PRInt32 childCount; */
NS_IMETHODIMP nsNavHistoryResultNode::GetChildCount(PRInt32* aChildCount)
{
  *aChildCount = mChildren.Count();
  return NS_OK;
}

/* nsINavHistoryResultNode getChild(in PRInt32 index); */
NS_IMETHODIMP nsNavHistoryResultNode::GetChild(PRInt32 aIndex,
                                               nsINavHistoryResultNode** _retval)
{
  if (aIndex < 0 || aIndex >= mChildren.Count())
    return NS_ERROR_INVALID_ARG;
  NS_ADDREF(*_retval = mChildren[aIndex]);
  return NS_OK;
}

// nsNavHistoryQueryNode ******************************************************

nsNavHistoryQueryNode::~nsNavHistoryQueryNode()
{
  for (PRUint32 i = 0; i < mQueryCount; ++i) {
    NS_RELEASE(mQueries[i]);
  }
  nsMemory::Free(mQueries);
}

nsresult
nsNavHistoryQueryNode::ParseQueries()
{
  nsNavHistory *history = nsNavHistory::GetHistoryService();
  return history->QueryStringToQueries(QueryURIToQuery(mUrl),
                                       &mQueries, &mQueryCount,
                                       getter_AddRefs(mOptions));
}

NS_IMETHODIMP
nsNavHistoryQueryNode::GetQueries(PRUint32 *aQueryCount,
                                  nsINavHistoryQuery ***aQueries)
{
  if (!mOptions) {
    nsresult rv = ParseQueries();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsINavHistoryQuery **queries =
    NS_STATIC_CAST(nsINavHistoryQuery**,
                   nsMemory::Alloc(mQueryCount * sizeof(nsINavHistoryQuery*)));
  NS_ENSURE_TRUE(queries, NS_ERROR_OUT_OF_MEMORY);

  for (PRUint32 i = 0; i < mQueryCount; ++i) {
    NS_ADDREF(queries[i] = mQueries[i]);
  }

  *aQueryCount = mQueryCount;
  *aQueries = queries;
  return NS_OK;
}

NS_IMETHODIMP
nsNavHistoryQueryNode::GetQueryOptions(nsINavHistoryQueryOptions **aOptions) 
{
  if (!mOptions) {
    nsresult rv = ParseQueries();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  NS_ADDREF(*aOptions = mOptions);
  return NS_OK;
}

nsresult
nsNavHistoryQueryNode::BuildChildren(PRUint32 aOptions)
{
  nsresult rv;
  if (!mOptions) {
    rv = ParseQueries();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsCOMPtr<nsINavHistoryResult> iResult;
  nsNavHistory *history = nsNavHistory::GetHistoryService();
  rv = history->ExecuteQueries(mQueries, mQueryCount, mOptions,
                               getter_AddRefs(iResult));
  NS_ENSURE_SUCCESS(rv, rv);

  nsNavHistoryResult *result =
    NS_STATIC_CAST(nsNavHistoryResult*, NS_STATIC_CAST(nsINavHistoryResult*,
                                                       iResult));

  NS_ENSURE_TRUE(mChildren.AppendObjects(*(result->GetTopLevel())),
                 NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

// nsNavHistoryResult **********************************************************

NS_IMPL_ISUPPORTS_INHERITED2(nsNavHistoryResult, nsNavHistoryResultNode,
                             nsINavHistoryResult,
                             nsITreeView);


// nsNavHistoryResult::nsNavHistoryResult

nsNavHistoryResult::nsNavHistoryResult(nsNavHistory* aHistoryService,
                                       nsIStringBundle* aHistoryBundle,
                                       nsINavHistoryQuery** aQueries,
                                       PRUint32 aQueryCount,
                                       nsINavHistoryQueryOptions* aOptions)
  : mBundle(aHistoryBundle), mHistoryService(aHistoryService),
    mCollapseDuplicates(PR_TRUE),
    mTimesIncludeDates(PR_TRUE),
    mCurrentSort(nsINavHistoryQueryOptions::SORT_BY_NONE),
    mBookmarkOptions(nsINavBookmarksService::ALL_CHILDREN)
{
  // Fill saved source queries with copies of the original (the caller might
  // change their original objects, and we always want to reflect the source
  // parameters).
  for (PRUint32 i = 0; i < aQueryCount; i ++) {
    nsCOMPtr<nsINavHistoryQuery> queryClone;
    if (NS_SUCCEEDED(aQueries[i]->Clone(getter_AddRefs(queryClone))))
      mSourceQueries.AppendObject(queryClone);
  }
  if (aOptions)
    aOptions->Clone(getter_AddRefs(mSourceOptions));

  mType = nsINavHistoryResult::RESULT_TYPE_QUERY;
  mFlatIndex = -1;
  mVisibleIndex = -1;
}

// nsNavHistoryResult::~nsNavHistoryResult

nsNavHistoryResult::~nsNavHistoryResult()
{
}


// nsNavHistoryResult::Init

nsresult
nsNavHistoryResult::Init()
{
  nsresult rv;
  nsCOMPtr<nsILocaleService> ls = do_GetService(NS_LOCALESERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = ls->GetApplicationLocale(getter_AddRefs(mLocale));
  NS_ENSURE_SUCCESS(rv, rv);

  // collation service for sorting
  static NS_DEFINE_CID(kCollationFactoryCID, NS_COLLATIONFACTORY_CID);
  nsCOMPtr<nsICollationFactory> cfact = do_CreateInstance(
     kCollationFactoryCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  cfact->CreateCollation(mLocale, getter_AddRefs(mCollation));

  // date time formatting
  static NS_DEFINE_CID(kDateTimeFormatCID, NS_DATETIMEFORMAT_CID);
  mDateFormatter = do_CreateInstance(kDateTimeFormatCID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


// nsNavHistoryResult::FilledAllResults
//
//    Note that the toplevel node is not actually displayed in the tree.
//    This is why we use a starting level of -1. The immediate children
//    of this result will be at level 0, along the left side of the tree.

void
nsNavHistoryResult::FilledAllResults()
{
  FillTreeStats(this, -1),
  RebuildAllListRecurse(mChildren);
  InitializeVisibleList();
}

// nsNavHistoryResult::BuildChildrenFor

nsresult
nsNavHistoryResult::BuildChildrenFor(nsNavHistoryResultNode *aNode)
{
  nsresult rv = aNode->BuildChildren(mBookmarkOptions);
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 flatIndex = aNode->mFlatIndex + 1;
  for (PRInt32 i = 0; i < aNode->mChildren.Count(); ++i) {
    nsNavHistoryResultNode *child = aNode->mChildren[i];

    // XXX inefficient, need to be able to insert multiple items at once
    mAllElements.InsertElementAt(child, flatIndex++);
    for (PRInt32 j = 0; j < child->mChildren.Count(); ++j) {
      mAllElements.InsertElementAt(child->mChildren[j], flatIndex++);
    }
  }

  FillTreeStats(aNode, aNode->mIndentLevel);
  return NS_OK;
}


// nsNavHistoryResult::RecursiveSort

NS_IMETHODIMP
nsNavHistoryResult::RecursiveSort(PRUint32 aSortingMode)
{
  if (aSortingMode > nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_DESCENDING)
    return NS_ERROR_INVALID_ARG;

  mCurrentSort = aSortingMode;
  RecursiveSortArray(mChildren, aSortingMode);

  // This sorting function is called from two contexts. First, when everything
  // is being built, executeQueries will do a sort and then call
  // FilledAllResults to build up all the bookkeeping. In this case, we don't
  // need to do anything more since everything will be built later.
  //
  // Second, anybody can call this function to cause a resort. In this case,
  // we need to rebuild the visible element lists and notify the tree box object
  // of the change. The way to disambiguate these cases is whether the flatted
  // mAllElements list has been generated yet.
  if (mAllElements.Count() > 0)
    RebuildList();

  // update the UI on the tree columns
  if (mTree)
    SetTreeSortingIndicator();
  return NS_OK;
}


// nsNavHistoryResult::Get/SetTimesIncludeDates

NS_IMETHODIMP nsNavHistoryResult::SetTimesIncludeDates(PRBool aValue)
{
  mTimesIncludeDates = aValue;
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryResult::GetTimesIncludeDates(PRBool* aValue)
{
  *aValue = mTimesIncludeDates;
  return NS_OK;
}


// nsNavHistoryResult::Get/SetCollapseDuplicates

NS_IMETHODIMP nsNavHistoryResult::SetCollapseDuplicates(PRBool aValue)
{
  PRBool oldValue = mCollapseDuplicates;
  mCollapseDuplicates = aValue;
  if (oldValue != mCollapseDuplicates && mAllElements.Count() > 0)
    RebuildList();
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryResult::GetCollapseDuplicates(PRBool* aValue)
{
  *aValue = mCollapseDuplicates;
  return NS_OK;
}


// nsNavHistoryResult::RecursiveSortArray

void
nsNavHistoryResult::RecursiveSortArray(
    nsCOMArray<nsNavHistoryResultNode>& aSources, PRUint32 aSortingMode)
{
  switch (aSortingMode)
  {
    case nsINavHistoryQueryOptions::SORT_BY_NONE:
      break;
    case nsINavHistoryQueryOptions::SORT_BY_TITLE_ASCENDING:
      aSources.Sort(SortComparison_TitleLess, NS_STATIC_CAST(void*, this));
      break;
    case nsINavHistoryQueryOptions::SORT_BY_TITLE_DESCENDING:
      aSources.Sort(SortComparison_TitleGreater, NS_STATIC_CAST(void*, this));
      break;
    case nsINavHistoryQueryOptions::SORT_BY_DATE_ASCENDING:
      aSources.Sort(SortComparison_DateLess, NS_STATIC_CAST(void*, this));
      break;
    case nsINavHistoryQueryOptions::SORT_BY_DATE_DESCENDING:
      aSources.Sort(SortComparison_DateGreater, NS_STATIC_CAST(void*, this));
      break;
    case nsINavHistoryQueryOptions::SORT_BY_URL_ASCENDING:
      aSources.Sort(SortComparison_URLLess, NS_STATIC_CAST(void*, this));
      break;
    case nsINavHistoryQueryOptions::SORT_BY_URL_DESCENDING:
      aSources.Sort(SortComparison_URLGreater, NS_STATIC_CAST(void*, this));
      break;
    case nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_ASCENDING:
      aSources.Sort(SortComparison_VisitCountLess, NS_STATIC_CAST(void*, this));
      break;
    case nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_DESCENDING:
      aSources.Sort(SortComparison_VisitCountGreater, NS_STATIC_CAST(void*, this));
      break;
    default:
      NS_NOTREACHED("Bad sorting type");
      break;
  }

  // sort any children
  for (PRInt32 i = 0; i < aSources.Count(); i ++) {
    if (aSources[i]->mChildren.Count() > 0)
      RecursiveSortArray(aSources[i]->mChildren, aSortingMode);
  }
}


// nsNavHistoryResult::ApplyTreeState
//

void nsNavHistoryResult::ApplyTreeState(
    const nsDataHashtable<nsStringHashKey, int>& aExpanded)
{
  RecursiveApplyTreeState(mChildren, aExpanded);

  // If the list has been build yet, we need to redo the visible list.
  // Normally, this function is called during object creation, and we don't
  // need to worry about the list of visible nodes.
  if (mAllElements.Count() > 0)
    RebuildList();
}


// nsNavHistoryResult::RecursiveApplyTreeState
//
//    See ApplyTreeState

void
nsNavHistoryResult::RecursiveApplyTreeState(
    nsCOMArray<nsNavHistoryResultNode>& aList,
    const nsDataHashtable<nsStringHashKey, int>& aExpanded)
{
  for (PRInt32 i = 0; i < aList.Count(); i ++) {
    if (aList[i]->mChildren.Count()) {
      PRInt32 beenExpanded = 0;
      if (aExpanded.Get(aList[i]->mTitle, &beenExpanded) && beenExpanded)
        aList[i]->mExpanded = PR_TRUE;
      else
        aList[i]->mExpanded = PR_FALSE;
      RecursiveApplyTreeState(aList[i]->mChildren, aExpanded);
    }
  }
}


// nsNavHistoryResult::ExpandAll

NS_IMETHODIMP
nsNavHistoryResult::ExpandAll()
{
  RecursiveExpandCollapse(mChildren, PR_TRUE);
  RebuildList();
  return NS_OK;
}


// nsNavHistoryResult::CollapseAll

NS_IMETHODIMP
nsNavHistoryResult::CollapseAll()
{
  RecursiveExpandCollapse(mChildren, PR_FALSE);
  RebuildList();
  return NS_OK;
}


// nsNavHistoryResult::RecursiveExpandCollapse
//
//    aExpand = true  -> expand all
//    aExpand = false -> collapse all

void
nsNavHistoryResult::RecursiveExpandCollapse(
    nsCOMArray<nsNavHistoryResultNode>& aList, PRBool aExpand)
{
  for (PRInt32 i = 0; i < aList.Count(); i ++) {
    if (aList[i]->mChildren.Count()) {
      aList[i]->mExpanded = aExpand;
      RecursiveExpandCollapse(aList[i]->mChildren, aExpand);
    }
  }
}


// nsNavHistoryResult::NodeForTreeIndex

NS_IMETHODIMP
nsNavHistoryResult::NodeForTreeIndex(PRUint32 index,
                                     nsINavHistoryResultNode** aResult)
{
  if (index >= (PRUint32)mVisibleElements.Count())
    return NS_ERROR_INVALID_ARG;
  NS_ADDREF(*aResult = VisibleElementAt(index));
  return NS_OK;
}


// nsNavHistoryResult::TreeIndexForNode

NS_IMETHODIMP
nsNavHistoryResult::TreeIndexForNode(nsINavHistoryResultNode* aNode,
                                     PRUint32* aResult)
{
  nsresult rv;
  nsCOMPtr<nsNavHistoryResultNode> node = do_QueryInterface(aNode, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (node->mVisibleIndex < 0)
    *aResult = nsINavHistoryResult::INDEX_INVISIBLE;
  else
    *aResult = node->mVisibleIndex;
  return NS_OK;
}


// nsNavHistoryResult::GetSourceQueries

NS_IMETHODIMP
nsNavHistoryResult::GetSourceQueries(PRUint32* aQueryCount, 
                                     nsINavHistoryQuery*** aQueries)
{
  *aQueries = nsnull;
  *aQueryCount = 0;
  if (mSourceQueries.Count() > 0) {
    *aQueries = NS_STATIC_CAST(nsINavHistoryQuery**,
         nsMemory::Alloc(sizeof(nsINavHistoryQuery*) * mSourceQueries.Count()));
    if (! *aQueries)
      return NS_ERROR_OUT_OF_MEMORY;
    *aQueryCount = mSourceQueries.Count();

    for (PRInt32 i = 0; i < mSourceQueries.Count(); i ++) {
      (*aQueries)[i] = mSourceQueries[i];
      NS_ADDREF((*aQueries)[i]);
    }
  }
  return NS_OK;
}

// nsNavHistoryResult::GetSourceQueryOptions

NS_IMETHODIMP
nsNavHistoryResult::GetSourceQueryOptions(nsINavHistoryQueryOptions** aOptions)
{
  *aOptions = mSourceOptions;
  NS_IF_ADDREF(*aOptions);
  return NS_OK;
}

NS_IMETHODIMP 
nsNavHistoryResult::AddObserver(nsINavHistoryResultViewObserver* aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);

  if (mObservers.IndexOf(aObserver) != -1)
    return NS_OK;
  return mObservers.AppendObject(aObserver) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsNavHistoryResult::RemoveObserver(nsINavHistoryResultViewObserver* aObserver)
{
  mObservers.RemoveObject(aObserver);
  return NS_OK;
}


// nsNavHistoryResult::SetTreeSortingIndicator
//
//    This is called to assign the correct arrow to the column header. It is
//    called both when you resort, and when a tree is first attached (to
//    initialize it's headers).

void
nsNavHistoryResult::SetTreeSortingIndicator()
{
  nsCOMPtr<nsITreeColumns> columns;
  nsresult rv = mTree->GetColumns(getter_AddRefs(columns));
  if (NS_FAILED(rv)) return;

  NS_NAMED_LITERAL_STRING(sortDirectionKey, "sortDirection");

  // clear old sorting indicator. Watch out: GetSortedColumn returns NS_OK
  // but null element if there is no sorted element
  nsCOMPtr<nsITreeColumn> column;
  rv = columns->GetSortedColumn(getter_AddRefs(column));
  if (NS_FAILED(rv)) return;
  if (column) {
    nsCOMPtr<nsIDOMElement> element;
    rv = column->GetElement(getter_AddRefs(element));
    if (NS_FAILED(rv)) return;
    element->SetAttribute(sortDirectionKey, NS_LITERAL_STRING(""));
  }

  // set new sorting indicator by looking through all columns for ours
  if (mCurrentSort == nsINavHistoryQueryOptions::SORT_BY_NONE)
    return;
  PRBool desiredIsDescending;
  ColumnType desiredColumn = SortTypeToColumnType(mCurrentSort,
                                                  &desiredIsDescending);
  PRInt32 colCount;
  rv = columns->GetCount(&colCount);
  if (NS_FAILED(rv)) return;
  for (PRInt32 i = 0; i < colCount; i ++) {
    columns->GetColumnAt(i, getter_AddRefs(column));
    if (NS_FAILED(rv)) return;
    if (GetColumnType(column) == desiredColumn) {
      // found our desired one, set
      nsCOMPtr<nsIDOMElement> element;
      rv = column->GetElement(getter_AddRefs(element));
      if (NS_FAILED(rv)) return;
      if (desiredIsDescending)
        element->SetAttribute(sortDirectionKey, NS_LITERAL_STRING("descending"));
      else
        element->SetAttribute(sortDirectionKey, NS_LITERAL_STRING("ascending"));
      break;
    }
  }
}


// nsNavHistoryResult::SortComparison_Title*
//
//    These are a little more complicated because they do a localization
//    conversion. If this is too slow, we can compute the sort keys once in
//    advance, sort that array, and then reorder the real array accordingly.
//    This would save some key generations.
//
//    The collation object must be allocated before sorting on title!

PRInt32 PR_CALLBACK nsNavHistoryResult::SortComparison_TitleLess(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  nsNavHistoryResult* result = NS_STATIC_CAST(nsNavHistoryResult*, closure);
  PRInt32 value = -1; // default to returning "true" on failure
  result->mCollation->CompareString(
      nsICollation::kCollationCaseInSensitive, a->mTitle, b->mTitle, &value);
  if (value == 0) {
    // resolve by URL
    value = a->mUrl.Compare(PromiseFlatCString(b->mUrl).get());
    if (value == 0) {
      // resolve by date
      return ComparePRTime(a->mTime, b->mTime);
    }
  }
  return value;
}
PRInt32 PR_CALLBACK nsNavHistoryResult::SortComparison_TitleGreater(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  return -SortComparison_TitleLess(a, b, closure);
}


// nsNavHistoryResult::SortComparison_Date*
//
//    Don't bother doing conflict resolution. Since dates are stored in
//    microseconds, it will be very difficult to get collisions. This would be
//    most likely for imported history, which I'm not too worried about.

PRInt32 PR_CALLBACK nsNavHistoryResult::SortComparison_DateLess(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  return ComparePRTime(a->mTime, b->mTime);
}
PRInt32 PR_CALLBACK nsNavHistoryResult::SortComparison_DateGreater(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  return -ComparePRTime(a->mTime, b->mTime);
}


// nsNavHistoryResult::SortComparison_URL*
//
//    Certain types of parent nodes are treated specially because URLs are not
//    meaningful.

PRInt32 PR_CALLBACK nsNavHistoryResult::SortComparison_URLLess(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  if (a->mType != b->mType) {
    // we really shouldn't be comparing different row types
    NS_NOTREACHED("Comparing different row types!");
    return 0;
  }

  PRInt32 value;
  if (a->mType == nsINavHistoryResult::RESULT_TYPE_HOST) {
    // for host nodes, use title (= host name)
    nsNavHistoryResult* result = NS_STATIC_CAST(nsNavHistoryResult*, closure);
    result->mCollation->CompareString(
        nsICollation::kCollationCaseInSensitive, a->mTitle, b->mTitle, &value);
  } else if (a->mType == nsINavHistoryResult::RESULT_TYPE_DAY) {
    // date nodes use date (skip conflict resolution becuase it uses date too)
    return ComparePRTime(a->mTime, b->mTime);
  } else {
    // normal URL or visit
    value = a->mUrl.Compare(PromiseFlatCString(b->mUrl).get());
  }

  // resolve conflicts using date
  if (value == 0) {
    return ComparePRTime(a->mTime, b->mTime);
  }
  return value;
}
PRInt32 PR_CALLBACK nsNavHistoryResult::SortComparison_URLGreater(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  return -SortComparison_URLLess(a, b, closure);
}


// nsNavHistory::SortComparison_VisitCount*
//
//    Fall back on dates for conflict resolution

PRInt32 PR_CALLBACK nsNavHistoryResult::SortComparison_VisitCountLess(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  PRInt32 value = CompareIntegers(a->mAccessCount, b->mAccessCount);
  if (value == 0)
    return ComparePRTime(a->mTime, b->mTime);
  return value;
}
PRInt32 PR_CALLBACK nsNavHistoryResult::SortComparison_VisitCountGreater(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  PRInt32 value = -CompareIntegers(a->mAccessCount, b->mAccessCount);
  if (value == 0)
    return -ComparePRTime(a->mTime, b->mTime);
  return value;
}


// nsNavHistoryResult::FillTreeStats
//
//    This basically does a recursive depth-first traversal of the tree to fill
//    in bookeeping information and statistics for parent nodes.

void
nsNavHistoryResult::FillTreeStats(nsNavHistoryResultNode* aResult, PRInt32 aLevel)
{
  aResult->mIndentLevel = aLevel;
  if (aResult->mChildren.Count() > 0) {
    PRInt32 totalAccessCount = 0;
    PRTime mostRecentTime = 0;
    for (PRInt32 i = 0; i < aResult->mChildren.Count(); i ++ ) {
      nsNavHistoryResultNode* child = NS_STATIC_CAST(
          nsNavHistoryResultNode*, aResult->mChildren[i]);

      child->mParent = aResult;
      FillTreeStats(aResult->mChildren[i], aLevel + 1);

      totalAccessCount += child->mAccessCount;
      if (LL_CMP(child->mTime, >, mostRecentTime))
        mostRecentTime = child->mTime;
    }

    // when there are children, the parent is the aggregate of its children
    aResult->mAccessCount = totalAccessCount;
    aResult->mTime = mostRecentTime;
  }
}


// nsNavHistoryResult::InitializeVisibleList
//
//    The root keeps its visible index of -1, since it is never visible

void
nsNavHistoryResult::InitializeVisibleList()
{
  NS_ASSERTION(mVisibleElements.Count() == 0, "Initializing visible list twice");

  // Fill the visible element list and notify those elements of their new
  // positions. We fill directly into the result list, so we need to manually
  // set the visible indices on those elements (normally this is done by
  // InsertVisibleSection)
  BuildVisibleSection(mChildren, &mVisibleElements);
  for (PRInt32 i = 0; i < mVisibleElements.Count(); i ++)
    VisibleElementAt(i)->mVisibleIndex = i;
}


// nsNavHistoryResult::RebuildList
//
//    Called when something big changes, like sorting. This rebuilds the
//    flat and visible element lists using the current toplevel array.

void
nsNavHistoryResult::RebuildList()
{
  PRInt32 oldVisibleCount = mVisibleElements.Count();

  mAllElements.Clear();
  mVisibleElements.Clear();
  RebuildAllListRecurse(mChildren);
  InitializeVisibleList();

  // We need to update the tree to tell it about the new list
  if (mTree) {
    if (mVisibleElements.Count() != oldVisibleCount)
      mTree->RowCountChanged(0, mVisibleElements.Count() - oldVisibleCount);
    mTree->InvalidateRange(0, mVisibleElements.Count());
  }
}


// nsNavHistoryResult::RebuildAllListRecurse
//
//    Does the work for RebuildList in generating the new mAllElements list

void
nsNavHistoryResult::RebuildAllListRecurse(
    const nsCOMArray<nsNavHistoryResultNode>& aSource)
{
  for (PRInt32 i = 0; i < aSource.Count(); i ++) {
    PRUint32 allCount = mAllElements.Count();
    if (mCollapseDuplicates && allCount > 0 && aSource[i]->mID != 0 &&
        AllElementAt(allCount - 1)->mID == aSource[i]->mID) {
      // ignore duplicate, that elements' flat index is the index of its dup
      // note, we don't collapse ID=0 elements since that is all parent nodes
      aSource[i]->mFlatIndex = allCount - 1;
    } else {
      aSource[i]->mFlatIndex = mAllElements.Count();
      mAllElements.AppendElement(aSource[i]);
      if (aSource[i]->mChildren.Count() > 0)
        RebuildAllListRecurse(aSource[i]->mChildren);
    }
  }
}

// nsNavHistoryResult::BuildVisibleSection
//
//    This takes all nodes in the given source array and recursively (when
//    containers are open) appends them to the flattened visible result array.
//    This is used when expanding folders and in initially building the
//    visible array list.
//
//    This will also collapse duplicate elements. As long as this is called for
//    an entire subtree, it is OK, since duplicate elements should not
//    occur next to each other at different levels of the tree.

void
nsNavHistoryResult::BuildVisibleSection(
    const nsCOMArray<nsNavHistoryResultNode>& aSources, nsVoidArray* aVisible)
{
  for (PRInt32 i = 0; i < aSources.Count(); i ++) {
    nsNavHistoryResultNode* cur = aSources[i];
    if (mCollapseDuplicates && aVisible->Count() > 0 && aSources[i]->mID != 0) {
      nsNavHistoryResultNode* prev =
        (nsNavHistoryResultNode*)(*aVisible)[aVisible->Count() - 1];
      if (prev->mID == cur->mID)
        continue; // collapse duplicate
    }

    aVisible->AppendElement(cur);
    if (cur->mExpanded) {
      BuildChildrenFor(cur);
      if (cur->mChildren.Count() > 0) {
        BuildVisibleSection(cur->mChildren, aVisible);
      }
    }
  }
}


// nsNavHistoryResult::InsertVisibleSection
//
//    This takes a list of items (probably generated by BuildVisibleSection)
//    and inserts it into the visible element list, updating those elements as
//    needed. This is used when expanding the tree.

void
nsNavHistoryResult::InsertVisibleSection(const nsVoidArray& aAddition,
                                         PRInt32 aInsertHere)
{
  NS_ASSERTION(aInsertHere >= 0 && aInsertHere <= mVisibleElements.Count(),
               "Invalid insertion point");
  mVisibleElements.InsertElementsAt(aAddition, aInsertHere);

  // we need to update all the elements from the insertion point to the end
  // of the list of their new indices
  for (PRInt32 i = aInsertHere; i < mVisibleElements.Count(); i ++)
    VisibleElementAt(i)->mVisibleIndex = i;
}


// nsNavHistoryResult::DeleteVisibleChildrenOf
//
//    This is called when an element is collapsed. We search for all elements
//    that are at a greater level of indent and remove them from the visible
//    element list. Returns the number of rows deleted

int
nsNavHistoryResult::DeleteVisibleChildrenOf(PRInt32 aIndex)
{
  NS_ASSERTION(aIndex >= 0 && aIndex < mVisibleElements.Count(),
               "Index out of bounds");

  const nsNavHistoryResultNode* parentNode = VisibleElementAt(aIndex);
  NS_ASSERTION(parentNode->mChildren.Count() > 0 && parentNode->mExpanded,
               "Trying to collapse an improper node");

  // compute the index of the element just after the end of the deleted region
  PRInt32 outerLevel = parentNode->mIndentLevel;
  PRInt32 nextOuterIndex = mVisibleElements.Count();
  PRInt32 i;
  for (i = aIndex + 1; i < mVisibleElements.Count(); i ++) {
    if (VisibleElementAt(i)->mIndentLevel <= outerLevel) {
      nextOuterIndex = i;
      break;
    }
  }

  // Mark those elements as invisible and remove them.
  for (i = aIndex + 1; i < nextOuterIndex; i ++)
    VisibleElementAt(i)->mVisibleIndex = -1;
  PRInt32 deleteCount = nextOuterIndex - aIndex - 1;
  mVisibleElements.RemoveElementsAt(aIndex + 1, deleteCount);

  // re-number the moved elements
  for (i = aIndex + 1; i < mVisibleElements.Count(); i ++)
    VisibleElementAt(i)->mVisibleIndex = i;

  return deleteCount;
}


// nsNavHistoryResult::GetRowCount (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::GetRowCount(PRInt32 *aRowCount)
{
  *aRowCount = mVisibleElements.Count();
  return NS_OK;
}


// nsNavHistoryResult::Get/SetSelection (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::GetSelection(nsITreeSelection** aSelection)
{
  if (! mSelection) {
    *aSelection = nsnull;
    return NS_ERROR_FAILURE;
  }
  NS_ADDREF(*aSelection = mSelection);
  return NS_OK;
}
NS_IMETHODIMP nsNavHistoryResult::SetSelection(nsITreeSelection* aSelection)
{
  mSelection = aSelection;
  return NS_OK;
}

/* void getRowProperties (in long index, in nsISupportsArray properties); */
NS_IMETHODIMP nsNavHistoryResult::GetRowProperties(PRInt32 index, nsISupportsArray *properties)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void getCellProperties (in long row, in nsITreeColumn col, in nsISupportsArray properties); */
NS_IMETHODIMP nsNavHistoryResult::GetCellProperties(PRInt32 row, nsITreeColumn *col, nsISupportsArray *properties)
{
  if (row < 0 || row >= mVisibleElements.Count())
    return NS_ERROR_INVALID_ARG;

  nsNavHistoryResultNode *node = VisibleElementAt(row);
  PRInt64 folderId, bookmarksRootId, toolbarRootId;
  node->GetFolderId(&folderId);

  nsCOMPtr<nsINavBookmarksService> bms(do_GetService(NS_NAVBOOKMARKSSERVICE_CONTRACTID));
  bms->GetBookmarksRoot(&bookmarksRootId);
  bms->GetToolbarRoot(&toolbarRootId);
  if (bookmarksRootId == folderId)
    properties->AppendElement(nsNavHistory::sMenuRootAtom);
  else if (toolbarRootId == folderId)
    properties->AppendElement(nsNavHistory::sToolbarRootAtom);

  return NS_OK;
}

/* void getColumnProperties (in nsITreeColumn col, in nsISupportsArray properties); */
NS_IMETHODIMP nsNavHistoryResult::GetColumnProperties(nsITreeColumn *col, nsISupportsArray *properties)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


// nsNavHistoryResult::IsContainer (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::IsContainer(PRInt32 index, PRBool *_retval)
{
  if (index < 0 || index >= mVisibleElements.Count())
    return NS_ERROR_INVALID_ARG;

  nsNavHistoryResultNode *node = VisibleElementAt(index);
  *_retval = (node->mChildren.Count() > 0 ||
              node->mType == nsINavHistoryResult::RESULT_TYPE_FOLDER ||
              node->mType == nsINavHistoryResult::RESULT_TYPE_QUERY);
  return NS_OK;
}


// nsNavHistoryResult::IsContainerOpen (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::IsContainerOpen(PRInt32 index, PRBool *_retval)
{
  if (index < 0 || index >= mVisibleElements.Count())
    return NS_ERROR_INVALID_ARG;
  *_retval = VisibleElementAt(index)->mExpanded;
  return NS_OK;
}


// nsNavHistoryResult::IsContainerEmpty (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::IsContainerEmpty(PRInt32 index, PRBool *_retval)
{
  if (index < 0 || index >= mVisibleElements.Count())
    return NS_ERROR_INVALID_ARG;

  *_retval = (VisibleElementAt(index)->mChildren.Count() == 0);
  return NS_OK;
}


// nsNavHistoryResult::IsSeparator (nsITreeView)
//
//    We don't support separators

NS_IMETHODIMP nsNavHistoryResult::IsSeparator(PRInt32 index, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

/* boolean isSorted (); */
NS_IMETHODIMP nsNavHistoryResult::IsSorted(PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


// nsNavHistoryResult::CanDrop (nsITreeView)
//
//    No drag-and-drop to history.

NS_IMETHODIMP nsNavHistoryResult::CanDrop(PRInt32 index, PRInt32 orientation,
                                          PRBool *_retval)
{
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; ++i) {
    mObservers[i]->CanDrop(index, orientation, _retval);
    if (*_retval)
      break;
  }
  return NS_OK;
}


// nsNavHistoryResult::Drop (nsITreeView)
//
//    No drag-and-drop to history.

NS_IMETHODIMP nsNavHistoryResult::Drop(PRInt32 row, PRInt32 orientation)
{
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; ++i)
    mObservers[i]->OnDrop(row, orientation);
  return NS_OK;
}


// nsNavHistoryResult::GetParentIndex (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::GetParentIndex(PRInt32 index, PRInt32 *_retval)
{
  if (index < 0 || index >= mVisibleElements.Count())
    return NS_ERROR_INVALID_ARG;
  nsNavHistoryResultNode* parent = VisibleElementAt(index)->mParent;
  if (!parent || parent->mVisibleIndex < 0)
    *_retval = -1;
  else
    *_retval = parent->mVisibleIndex;
  return NS_OK;
}


// nsNavHistoryResult::HasNextSibling (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::HasNextSibling(PRInt32 index,
                                                 PRInt32 afterIndex,
                                                 PRBool *_retval)
{
  if (index < 0 || index >= mVisibleElements.Count())
    return NS_ERROR_INVALID_ARG;
  if (index == mVisibleElements.Count() - 1) {
    // this is the last thing in the list -> no next sibling
    *_retval = PR_FALSE;
    return NS_OK;
  }
  *_retval = (VisibleElementAt(index)->mIndentLevel ==
              VisibleElementAt(index + 1)->mIndentLevel);
  return NS_OK;
}


// nsNavHistoryResult::GetLevel (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::GetLevel(PRInt32 index, PRInt32 *_retval)
{
  if (index < 0 || index >= mVisibleElements.Count())
    return NS_ERROR_INVALID_ARG;
  *_retval = ((nsNavHistoryResultNode*)mVisibleElements[index])->mIndentLevel;
  return NS_OK;
}


// nsNavHistoryResult::GetImageSrc (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::GetImageSrc(PRInt32 row, nsITreeColumn *col,
                                              nsAString& _retval)
{
  _retval.Truncate(0);
  return NS_OK;
}

/* long getProgressMode (in long row, in nsITreeColumn col); */
NS_IMETHODIMP nsNavHistoryResult::GetProgressMode(PRInt32 row, nsITreeColumn *col, PRInt32 *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* AString getCellValue (in long row, in nsITreeColumn col); */
NS_IMETHODIMP nsNavHistoryResult::GetCellValue(PRInt32 row, nsITreeColumn *col, nsAString & _retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


// nsNavHistoryResult::GetCellText (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::GetCellText(PRInt32 rowIndex,
                                              nsITreeColumn *col,
                                              nsAString& _retval)
{
  if (rowIndex < 0 || rowIndex >= mVisibleElements.Count())
    return NS_ERROR_INVALID_ARG;
  PRInt32 colIndex;
  col->GetIndex(&colIndex);

  nsNavHistoryResultNode* elt = VisibleElementAt(rowIndex);

  switch (GetColumnType(col)) {
    case Column_Title:
    {
      // title: normally, this is just the title, but we don't want empty
      // items in the tree view so return a special string if the title is
      // empty. Do it here so that callers can still get at the 0 length title
      // if they go through the "result" API.
      if (! elt->mTitle.IsEmpty()) {
        _retval = elt->mTitle;
      } else {
        nsXPIDLString value;
        nsresult rv = mBundle->GetStringFromName(
            NS_LITERAL_STRING("noTitle").get(), getter_Copies(value));
        NS_ENSURE_SUCCESS(rv, rv);
        _retval = value;
      }
      break;
    }
    case Column_URL:
    {
      _retval = NS_ConvertUTF8toUTF16(elt->mUrl);
      break;
    }
    case Column_Date:
    {
      if (elt->mType == nsINavHistoryResult::RESULT_TYPE_HOST ||
          elt->mType == nsINavHistoryResult::RESULT_TYPE_DAY) {
        // hosts and days shouldn't have a value for the date column. Actually,
        // you could argue this point, but looking at the results, seeing the
        // most recently visited date is not what I expect, and gives me no
        // information I know how to use.
        _retval.Truncate(0);
      } else {
        nsDateFormatSelector dateFormat;
        if (mTimesIncludeDates)
          dateFormat = nsIScriptableDateFormat::dateFormatShort;
        else
          dateFormat = nsIScriptableDateFormat::dateFormatNone;
        nsAutoString realString; // stupid function won't take an nsAString
        mDateFormatter->FormatPRTime(mLocale, dateFormat,
                                     nsIScriptableDateFormat::timeFormatNoSeconds,
                                     elt->mTime, realString);
        _retval = realString;
      }
      break;
    }
    case Column_VisitCount:
    {
      _retval = NS_ConvertUTF8toUTF16(nsPrintfCString("%d", elt->mAccessCount));
      break;
    }
    default:
      return NS_ERROR_INVALID_ARG;
  }
  return NS_OK;
}


// nsNavHistoryResult::SetTree (nsITreeView)
//
//    This will get called when this view is attached to the tree to tell us
//    the box object, AND when the tree is being destroyed, with NULL.

NS_IMETHODIMP nsNavHistoryResult::SetTree(nsITreeBoxObject *tree)
{
  mTree = tree;
  if (mTree) // might be a NULL tree
    SetTreeSortingIndicator();
  return NS_OK;
}


// nsNavHistoryResult::ToggleOpenState (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::ToggleOpenState(PRInt32 index)
{
  if (index < 0 || index >= mVisibleElements.Count())
    return NS_ERROR_INVALID_ARG;

  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; ++i)
    mObservers[i]->OnToggleOpenState(index);

  nsNavHistoryResultNode* curNode = VisibleElementAt(index);
  if (curNode->mExpanded) {
    // collapse
    PRInt32 deleteCount = DeleteVisibleChildrenOf(index);
    curNode->mExpanded = PR_FALSE;
    if (mTree)
      mTree->RowCountChanged(index + 1, -deleteCount);

    mHistoryService->SaveCollapseItem(curNode->mTitle);
  } else {
    // expand
    BuildChildrenFor(curNode);

    nsVoidArray addition;
    BuildVisibleSection(curNode->mChildren, &addition);
    InsertVisibleSection(addition, index + 1);

    curNode->mExpanded = PR_TRUE;
    if (mTree)
      mTree->RowCountChanged(index + 1, addition.Count());

    mHistoryService->SaveExpandItem(curNode->mTitle);
  }
  /* Maybe we don't need this (test me)
  if (mTree)
    mTree->InvalidateRow(index);
  */
  return NS_OK;
}


// nsNavHistoryResult::CycleHeader (nsITreeView)
//
//    If we already sorted by that column, the sorting is reversed, otherwise
//    we use the default sorting direction for that data type.

NS_IMETHODIMP nsNavHistoryResult::CycleHeader(nsITreeColumn *col)
{
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; ++i)
    mObservers[i]->OnCycleHeader(col);

  PRInt32 colIndex;
  col->GetIndex(&colIndex);

  PRInt32 newSort;
  switch (GetColumnType(col)) {
    case Column_Title:
      if (mCurrentSort == nsINavHistoryQueryOptions::SORT_BY_TITLE_ASCENDING)
        newSort = nsINavHistoryQueryOptions::SORT_BY_TITLE_DESCENDING;
      else
        newSort = nsINavHistoryQueryOptions::SORT_BY_TITLE_ASCENDING;
      break;
    case Column_URL:
      if (mCurrentSort == nsINavHistoryQueryOptions::SORT_BY_URL_ASCENDING)
        newSort = nsINavHistoryQueryOptions::SORT_BY_URL_DESCENDING;
      else
        newSort = nsINavHistoryQueryOptions::SORT_BY_URL_ASCENDING;
      break;
    case Column_Date:
      if (mCurrentSort == nsINavHistoryQueryOptions::SORT_BY_DATE_ASCENDING)
        newSort = nsINavHistoryQueryOptions::SORT_BY_DATE_DESCENDING;
      else
        newSort = nsINavHistoryQueryOptions::SORT_BY_DATE_ASCENDING;
      break;
    case Column_VisitCount:
      // visit count default is unusual because it is descending
      if (mCurrentSort == nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_DESCENDING)
        newSort = nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_ASCENDING;
      else
        newSort = nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_DESCENDING;
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }
  return RecursiveSort(newSort);
}

/* void selectionChanged (); */
NS_IMETHODIMP nsNavHistoryResult::SelectionChanged()
{
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; ++i)
    mObservers[i]->OnSelectionChanged();
  return NS_OK;
}

/* void cycleCell (in long row, in nsITreeColumn col); */
NS_IMETHODIMP nsNavHistoryResult::CycleCell(PRInt32 row, nsITreeColumn *col)
{
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; ++i)
    mObservers[i]->OnCycleCell(row, col);
  return NS_OK;
}

/* boolean isEditable (in long row, in nsITreeColumn col); */
NS_IMETHODIMP nsNavHistoryResult::IsEditable(PRInt32 row, nsITreeColumn *col, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setCellValue (in long row, in nsITreeColumn col, in AString value); */
NS_IMETHODIMP nsNavHistoryResult::SetCellValue(PRInt32 row, nsITreeColumn *col, const nsAString & value)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void setCellText (in long row, in nsITreeColumn col, in AString value); */
NS_IMETHODIMP nsNavHistoryResult::SetCellText(PRInt32 row, nsITreeColumn *col, const nsAString & value)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

/* void performAction (in wstring action); */
NS_IMETHODIMP nsNavHistoryResult::PerformAction(const PRUnichar *action)
{
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; ++i)
    mObservers[i]->OnPerformAction(action);
  return NS_OK;
}

/* void performActionOnRow (in wstring action, in long row); */
NS_IMETHODIMP nsNavHistoryResult::PerformActionOnRow(const PRUnichar *action, PRInt32 row)
{
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; ++i)
    mObservers[i]->OnPerformActionOnRow(action, row);
  return NS_OK;
}

/* void performActionOnCell (in wstring action, in long row, in nsITreeColumn col); */
NS_IMETHODIMP nsNavHistoryResult::PerformActionOnCell(const PRUnichar *action, PRInt32 row, nsITreeColumn *col)
{
  PRInt32 count = mObservers.Count();
  for (PRInt32 i = 0; i < count; ++i)
    mObservers[i]->OnPerformActionOnCell(action, row, col);
  return NS_OK;
}

// nsNavHistoryResult::GetColumnType
//

nsNavHistoryResult::ColumnType
nsNavHistoryResult::GetColumnType(nsITreeColumn* col)
{
  const PRUnichar* idConst;
  col->GetIdConst(&idConst);
  switch(idConst[0]) {
    case 't':
      return Column_Title;
    case 'u':
      return Column_URL;
    case 'd':
      return Column_Date;
    case 'v':
      return Column_VisitCount;
    default:
      return Column_Unknown;
  }
}


// nsNavHistoryResult::SortTypeToColumnType
//
//    Places whether its ascending or descending into the given
//    boolean buffer

nsNavHistoryResult::ColumnType
nsNavHistoryResult::SortTypeToColumnType(PRUint32 aSortType,
                                         PRBool* aDescending)
{
  switch(aSortType) {
    case nsINavHistoryQueryOptions::SORT_BY_TITLE_ASCENDING:
      *aDescending = PR_FALSE;
      return Column_Title;
    case nsINavHistoryQueryOptions::SORT_BY_TITLE_DESCENDING:
      *aDescending = PR_TRUE;
      return Column_Title;
    case nsINavHistoryQueryOptions::SORT_BY_DATE_ASCENDING:
      *aDescending = PR_FALSE;
      return Column_Date;
    case nsINavHistoryQueryOptions::SORT_BY_DATE_DESCENDING:
      *aDescending = PR_TRUE;
      return Column_Date;
    case nsINavHistoryQueryOptions::SORT_BY_URL_ASCENDING:
      *aDescending = PR_FALSE;
      return Column_URL;
    case nsINavHistoryQueryOptions::SORT_BY_URL_DESCENDING:
      *aDescending = PR_TRUE;
      return Column_URL;
    case nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_ASCENDING:
      *aDescending = PR_FALSE;
      return Column_VisitCount;
    case nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_DESCENDING:
      *aDescending = PR_TRUE;
      return Column_VisitCount;
    default:
      return Column_Unknown;
  }
}
