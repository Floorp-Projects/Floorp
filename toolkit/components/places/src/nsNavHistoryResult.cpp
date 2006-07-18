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
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsFaviconService.h"
#include "nsHashPropertyBag.h"
#include "nsIComponentManager.h"
#include "nsIDateTimeFormat.h"
#include "nsIDOMElement.h"
#include "nsILocale.h"
#include "nsILocaleService.h"
#include "nsILocalFile.h"
#include "nsIRemoteContainer.h"
#include "nsIServiceManager.h"
#include "nsISupportsPrimitives.h"
#include "nsITreeColumns.h"
#include "nsIURI.h"
#include "nsIURL.h"
#include "nsIWritablePropertyBag.h"
#include "nsNetUtil.h"
#include "nsPrintfCString.h"
#include "nsPromiseFlatString.h"
#include "nsString.h"
#include "nsUnicharUtils.h"
#include "prtime.h"
#include "prprf.h"
#include "mozStorageHelper.h"

#define ICONURI_QUERY "chrome://browser/skin/places/query.png"

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

nsNavHistoryResultNode::nsNavHistoryResultNode(
    const nsACString& aTitle, PRUint32 aAccessCount,
    PRTime aTime, const nsACString& aIconURI) :
  mParent(nsnull),
  mTitle(aTitle),
  mAccessCount(aAccessCount),
  mTime(aTime),
  mFaviconURI(aIconURI),
  mIndentLevel(-1),
  mVisibleIndex(-1)
{
}

NS_IMETHODIMP
nsNavHistoryResultNode::GetIcon(nsIURI** aURI)
{
  nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
  NS_ENSURE_TRUE(faviconService, NS_ERROR_NO_INTERFACE);
  return faviconService->GetFaviconLinkForIconString(mFaviconURI, aURI);
}

NS_IMETHODIMP
nsNavHistoryResultNode::GetParent(nsINavHistoryContainerResultNode** aParent)
{
  NS_IF_ADDREF(*aParent = mParent);
  return NS_OK;
}

NS_IMETHODIMP
nsNavHistoryResultNode::GetPropertyBag(nsIWritablePropertyBag** aBag)
{
  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);
  return result->PropertyBagFor(this, aBag);
}


// nsNavHistoryResultNode::OnRemoving
//
//    This will zero out some values in case somebody still holds a reference

void
nsNavHistoryResultNode::OnRemoving()
{
  mParent = nsnull;
  mVisibleIndex = -1;
}


// nsNavHistoryResultNode::GetResult
//
//    This will find the result for this node. We can ask the nearest container
//    for this value (either ourselves or our parents should be a container,
//    and all containers have result pointers).

nsNavHistoryResult*
nsNavHistoryResultNode::GetResult()
{
  nsNavHistoryResultNode* node = this;
  do {
    if (node->IsContainer()) {
      nsNavHistoryContainerResultNode* container =
        NS_STATIC_CAST(nsNavHistoryContainerResultNode*, node);
      NS_ASSERTION(container->mResult, "Containers must have valid results");
      return container->mResult;
    }
    node = node->mParent;
  } while (node);
  NS_NOTREACHED("No container node found in hierarchy!");
  return nsnull;
}


// nsNavHistoryResultNode::GetGeneratingOptions
//
//    Searches up the tree for the closest node that has an options structure.
//    This will tell us the options that were used to generate this node.
//
//    Be careful, this function walks up the tree, so it can not be used when
//    result nodes are created because they have no parent. Only call this
//    function after the tree has been built.

nsNavHistoryQueryOptions*
nsNavHistoryResultNode::GetGeneratingOptions()
{
  if (! mParent) {
    // When we have no parent, it either means we haven't built the tree yet,
    // in which case calling this function is a bug, or this node is the root
    // of the tree. When we are the root of the tree, our own options are the
    // generating options, and we know we are either a query of a folder node.
    if (IsFolder())
      return GetAsFolder()->mOptions;
    else if (IsQuery())
      return GetAsQuery()->mOptions;
    NS_NOTREACHED("Can't find a generating node for this container, perhaps FillStats has not been called on this tree yet?");
    return nsnull;
  }

  nsNavHistoryContainerResultNode* cur = mParent;
  while (cur) {
    if (cur->IsFolder())
      return cur->GetAsFolder()->mOptions;
    else if (cur->IsQuery())
      return cur->GetAsQuery()->mOptions;
    cur = cur->mParent;
  }
  // we should always find a folder or query node as an ancestor
  NS_NOTREACHED("Can't find a generating node for this container, the tree seemes corrupted.");
  return nsnull;
}


// nsNavHistoryURIResultNode ***************************************************

NS_IMPL_ISUPPORTS_INHERITED1(nsNavHistoryURIResultNode,
                             nsNavHistoryResultNode,
                             nsINavHistoryURIResultNode)

nsNavHistoryURIResultNode::nsNavHistoryURIResultNode(
    const nsACString& aTitle, PRUint32 aAccessCount,
    PRTime aTime, const nsACString& aIconURI, const nsACString& aURI) :
  nsNavHistoryResultNode(aTitle, aAccessCount, aTime, aIconURI),
  mURI(aURI)
{
}


// nsNavHistoryVisitResultNode *************************************************

NS_IMPL_ISUPPORTS_INHERITED1(nsNavHistoryVisitResultNode,
                             nsNavHistoryURIResultNode,
                             nsINavHistoryVisitResultNode)

nsNavHistoryVisitResultNode::nsNavHistoryVisitResultNode(
    const nsACString& aTitle, PRUint32 aAccessCount, PRTime aTime,
    const nsACString& aIconURI, const nsACString& aURI, PRInt64 aSession) :
  nsNavHistoryURIResultNode(aTitle, aAccessCount, aTime, aIconURI, aURI),
  mSessionId(aSession)
{
}


// nsNavHistoryFullVisitResultNode *********************************************

NS_IMPL_ISUPPORTS_INHERITED1(nsNavHistoryFullVisitResultNode,
                             nsNavHistoryVisitResultNode,
                             nsINavHistoryFullVisitResultNode)

nsNavHistoryFullVisitResultNode::nsNavHistoryFullVisitResultNode(
    const nsACString& aTitle, PRUint32 aAccessCount, PRTime aTime,
    const nsACString& aIconURI, const nsACString& aURI, PRInt64 aSession,
    PRInt64 aVisitId, PRInt64 aReferringVisitId, PRInt32 aTransitionType) :
  nsNavHistoryVisitResultNode(aTitle, aAccessCount, aTime, aIconURI, aURI,
                              aSession),
  mVisitId(aVisitId),
  mReferringVisitId(aReferringVisitId),
  mTransitionType(aTransitionType)
{
}

// nsNavHistoryContainerResultNode *********************************************

NS_IMPL_ISUPPORTS_INHERITED1(nsNavHistoryContainerResultNode,
                             nsNavHistoryResultNode,
                             nsINavHistoryContainerResultNode)

nsNavHistoryContainerResultNode::nsNavHistoryContainerResultNode(
    const nsACString& aTitle, const nsACString& aIconURI,
    PRUint32 aContainerType, PRBool aReadOnly,
    const nsACString& aRemoteContainerType) :
  nsNavHistoryResultNode(aTitle, 0, 0, aIconURI),
  mResult(nsnull),
  mContainerType(aContainerType),
  mExpanded(PR_FALSE),
  mChildrenReadOnly(aReadOnly),
  mRemoteContainerType(aRemoteContainerType)
{
}


// nsNavHistoryContainerResultNode::OnRemoving
//
//    Containers should notify their children that they are being removed
//    when the container is being removed.

void
nsNavHistoryContainerResultNode::OnRemoving()
{
  nsNavHistoryResultNode::OnRemoving();
  for (PRInt32 i = 0; i < mChildren.Count(); i ++)
    mChildren[i]->OnRemoving();
  mChildren.Clear();
}


// nsNavHistoryContainerResultNode::AreChildrenVisible
//
//    Folders can't always depend on their mVisibleIndex value to determine if
//    their children are visible because they can be root nodes. Root nodes
//    are visible if a tree is attached to the result.

PRBool
nsNavHistoryContainerResultNode::AreChildrenVisible()
{
  // can't see children when we're invisible
  if (! mExpanded)
    return PR_FALSE;

  // easy case, the node itself is visible
  if (mVisibleIndex >= 0)
    return PR_TRUE;

  nsNavHistoryResult* result = GetResult();
  if (! result) {
    NS_NOTREACHED("Invalid result");
    return PR_FALSE;
  }
  if (result->mRootNode == this && result->mTree)
    return PR_TRUE;
  return PR_FALSE;
}


// nsNavHistoryContainerResultNode::GetContainerOpen

NS_IMETHODIMP
nsNavHistoryContainerResultNode::GetContainerOpen(PRBool *aContainerOpen)
{
  *aContainerOpen = mExpanded;
  return NS_OK;
}


// nsNavHistoryContainerResultNode::SetContainerOpen

NS_IMETHODIMP
nsNavHistoryContainerResultNode::SetContainerOpen(PRBool aContainerOpen)
{
  if (mExpanded && ! aContainerOpen)
    CloseContainer();
  else if (! mExpanded && aContainerOpen)
    OpenContainer();
  return NS_OK;
}


// nsNavHistoryContainerResultNode::OpenContainer
//
//    This handles the generic container case. Other container types should
//    override this to do their own handling.

nsresult
nsNavHistoryContainerResultNode::OpenContainer()
{
  nsresult rv;
  NS_ASSERTION(! mExpanded, "Container must be expanded to close it");
  mExpanded = PR_TRUE;

  if (! mRemoteContainerType.IsEmpty()) {
    // remote container API may want to fill us
    nsCOMPtr<nsIRemoteContainer> remote = do_GetService(mRemoteContainerType.get(), &rv);
    if (NS_SUCCEEDED(rv)) {
      remote->OnContainerOpening(this, GetGeneratingOptions());
    } else {
      NS_WARNING("Unable to get remote container for ");
      NS_WARNING(mRemoteContainerType.get());
    }
    PRInt32 oldAccessCount = mAccessCount;
    FillStats();
    ReverseUpdateStats(mAccessCount - oldAccessCount);
  }

  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);
  rv = result->RefreshVisibleSection(this);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


// nsNavHistoryContainerResultNode::CloseContainer

nsresult
nsNavHistoryContainerResultNode::CloseContainer()
{
  NS_ASSERTION(mExpanded, "Container must be expanded to close it");
  mExpanded = PR_FALSE;

  nsresult rv;
  if (! mRemoteContainerType.IsEmpty()) {
    // notify remote containers that we are closing
    nsCOMPtr<nsIRemoteContainer> remote = do_GetService(mRemoteContainerType.get(), &rv);
    if (NS_SUCCEEDED(rv))
      remote->OnContainerClosed(this);
  }

  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);
  rv = result->RefreshVisibleSection(this);
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


// nsNavHistoryContainerResultNode::FillStats
//
//    This builds up tree statistics from the bottom up. Call with a container
//    and the indent level of that container. To init the full tree, call with
//    the root container. The default indent level is -1, which is appropriate
//    for the root level.
//
//    CALL THIS AFTER FILLING ANY CONTAINER to update the parent and result
//    node pointers, even if you don't care about visit counts and last visit
//    dates.

void
nsNavHistoryContainerResultNode::FillStats()
{
  mAccessCount = 0;
  mTime = 0;
  for (PRInt32 i = 0; i < mChildren.Count(); i ++) {
    nsNavHistoryResultNode* node = mChildren[i];
    node->mParent = this;
    node->mIndentLevel = mIndentLevel + 1;
    if (node->IsContainer()) {
      nsNavHistoryContainerResultNode* container = node->GetAsContainer();
      container->mResult = mResult;
      container->FillStats();
    }
    mAccessCount += node->mAccessCount;
    if (node->mTime > mTime)
      mTime = node->mTime;
  }
}


// nsNavHistoryContainerResultNode::ReverseUpdateStats
//
//    This is used when one container changes to do a minimal update of the
//    tree structure. When something changes, you want to call FillStats if
//    necessary and update this container completely. Then call this function
//    which will walk up the tree and fill in the previous containers.
//
//    Note that you have to tell us by how much our access count changed. Our
//    access count should already be set to the new value; this is used to
//    change the parents without having to re-count all their children.
//
//    This does NOT update the last visit date downward. Therefore, if you are
//    deleting a node that has the most recent last visit date, the parents
//    will not get their last visit dates downshifted accordingly. This is a
//    rather unusual case: we don't often delete things, and we usually don't
//    even show the last visit date for folders. Updating would be slower
//    because we would have to recompute it from scratch.

void
nsNavHistoryContainerResultNode::ReverseUpdateStats(PRInt32 aAccessCountChange)
{
  if (mParent) {
    mParent->mAccessCount += aAccessCountChange;
    PRBool timeChanged = PR_FALSE;
    if (mTime > mParent->mTime) {
      timeChanged = PR_TRUE;
      mParent->mTime = mTime;
    }

    // check sorting, the stats may have caused this node to move if the
    // sorting depended on something we are changing.
    PRUint32 sortMode = mParent->GetSortType();
    PRBool resorted = PR_FALSE;
    if (((sortMode == nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_ASCENDING ||
          sortMode == nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_DESCENDING) &&
         aAccessCountChange != 0) ||
        ((sortMode == nsINavHistoryQueryOptions::SORT_BY_DATE_ASCENDING ||
          sortMode == nsINavHistoryQueryOptions::SORT_BY_DATE_DESCENDING) &&
         timeChanged)) {

      SortComparator comparator = GetSortingComparator(sortMode);
      int ourIndex = mParent->FindChild(this);
      if (DoesChildNeedResorting(ourIndex, comparator)) {
        // prevent us from being destroyed when removed from the parent
        nsRefPtr<nsNavHistoryContainerResultNode> ourLock = this;
        nsNavHistoryContainerResultNode* ourParent = mParent;

        // Performance: moving items by removing and re-inserting is not very
        // efficient because there may be a lot of unnecessary renumbering of
        // items. I don't think the overhead is worth the extra complexity in
        // this case.
        ourParent->RemoveChildAt(ourIndex, PR_TRUE);
        ourParent->InsertSortedChild(this, PR_TRUE);
        resorted = PR_TRUE;
      }
    }
    if (! resorted) {
      // repaint visible rows
      if (mParent->mVisibleIndex >= 0 && mResult->mTree)
        mResult->RowChanged(mParent->mVisibleIndex);
    }

    mParent->ReverseUpdateStats(aAccessCountChange);
  }
}


// nsNavHistoryContainerResultNode::GetSortType
//
//    This walks up the tree until we find a query result node or the root to
//    get the sorting type.
//
//    See nsNavHistoryQueryResultNode::GetSortType

PRUint32
nsNavHistoryContainerResultNode::GetSortType()
{
  if (mParent)
    return mParent->GetSortType();
  else if (mResult)
    return mResult->mSortingMode;
  NS_NOTREACHED("We should always have a result");
  return nsINavHistoryQueryOptions::SORT_BY_NONE;
}


// nsNavHistoryContainerResultNode::GetSortingComparator
//
//    Returns the sorting comparator function for the give sort type.
//    RETURNS NULL if there is no comparator.

nsNavHistoryContainerResultNode::SortComparator
nsNavHistoryContainerResultNode::GetSortingComparator(PRUint32 aSortType)
{
  switch (aSortType)
  {
    case nsINavHistoryQueryOptions::SORT_BY_NONE:
      return nsnull;
    case nsINavHistoryQueryOptions::SORT_BY_TITLE_ASCENDING:
      return &SortComparison_TitleLess;
    case nsINavHistoryQueryOptions::SORT_BY_TITLE_DESCENDING:
      return &SortComparison_TitleGreater;
    case nsINavHistoryQueryOptions::SORT_BY_DATE_ASCENDING:
      return &SortComparison_DateLess;
    case nsINavHistoryQueryOptions::SORT_BY_DATE_DESCENDING:
      return &SortComparison_DateGreater;
    case nsINavHistoryQueryOptions::SORT_BY_URI_ASCENDING:
      return &SortComparison_URILess;
    case nsINavHistoryQueryOptions::SORT_BY_URI_DESCENDING:
      return &SortComparison_URIGreater;
    case nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_ASCENDING:
      return &SortComparison_VisitCountLess;
    case nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_DESCENDING:
      return &SortComparison_VisitCountGreater;
    default:
      NS_NOTREACHED("Bad sorting type");
      return nsnull;
  }
}


// nsNavHistoryContainerResultNode::RecursiveSort
//
//    This is used by Result::SortAll and it is static so it can also be used
//    by nsNavHistory::GetQueryResults to sort node arrays without having a
//    root container.
//
//    This does NOT update any visibility or tree information. The caller will
//    have to completely rebuild the visible list after this.

void
nsNavHistoryContainerResultNode::RecursiveSort(
    nsICollation* aCollation, SortComparator aComparator)
{
  mChildren.Sort(aComparator, NS_STATIC_CAST(void*, aCollation));
  for (PRInt32 i = 0; i < mChildren.Count(); i ++) {
    if (mChildren[i]->IsContainer())
      mChildren[i]->GetAsContainer()->RecursiveSort(aCollation, aComparator);
  }
}


// nsNavHistoryContainerResultNode::FindInsertionPoint
//
//    This returns the index that the given item would fall on if it were to
//    be inserted using the given sorting.

PRUint32
nsNavHistoryContainerResultNode::FindInsertionPoint(
    nsNavHistoryResultNode* aNode, SortComparator aComparator)
{
  if (mChildren.Count() == 0)
    return 0;

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, 0);
  nsICollation* collation = history->GetCollation();

  // The common case is the beginning or the end because this is used to insert
  // new items that are added to history, which is usually sorted by date.
  if (aComparator(aNode, mChildren[0], collation) <= 0)
    return 0;
  if (aComparator(aNode, mChildren[mChildren.Count() - 1], collation) >= 0)
    return mChildren.Count();

  PRUint32 beginRange = 0; // inclusive
  PRUint32 endRange = mChildren.Count(); // exclusive
  while (1) {
    if (beginRange == endRange)
      return endRange;
    PRUint32 center = beginRange + (endRange - beginRange) / 2;
    if (aComparator(aNode, mChildren[center], collation) <= 0)
      endRange = center; // left side
    else
      beginRange = center + 1; // right site
  }
}


// nsNavHistoryContainerResultNode::DoesChildNeedResorting
//
//    This checks the child node at the given index to see if its sorting is
//    correct. Returns true if not and it should be resorted. This is called
//    when nodes are updated and we need to see whether we need to move it.

PRBool
nsNavHistoryContainerResultNode::DoesChildNeedResorting(PRUint32 aIndex,
    SortComparator aComparator)
{
  NS_ASSERTION(aIndex >= 0 && aIndex < PRUint32(mChildren.Count()),
               "Input index out of range");
  if (mChildren.Count() == 1)
    return PR_FALSE;

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, 0);
  nsICollation* collation = history->GetCollation();

  if (aIndex > 0) {
    // compare to previous item
    if (aComparator(mChildren[aIndex - 1], mChildren[aIndex], collation) > 0)
      return PR_TRUE;
  }
  if (aIndex < PRUint32(mChildren.Count()) - 1) {
    // compare to next item
    if (aComparator(mChildren[aIndex], mChildren[aIndex + 1], collation) > 0)
      return PR_TRUE;
  }
  return PR_FALSE;
}


// nsNavHistoryContainerResultNode::SortComparison_Title*
//
//    These are a little more complicated because they do a localization
//    conversion. If this is too slow, we can compute the sort keys once in
//    advance, sort that array, and then reorder the real array accordingly.
//    This would save some key generations.
//
//    The collation object must be allocated before sorting on title!

PRInt32 PR_CALLBACK nsNavHistoryContainerResultNode::SortComparison_TitleLess(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  PRUint32 aType, bType;
  a->GetType(&aType);
  b->GetType(&bType);

  if (aType != bType) {
    return bType - aType;
  }

  nsICollation* collation = NS_STATIC_CAST(nsICollation*, closure);
  PRInt32 value = -1; // default to returning "true" on failure
  collation->CompareString(
      nsICollation::kCollationCaseInSensitive,
      NS_ConvertUTF8toUTF16(a->mTitle),
      NS_ConvertUTF8toUTF16(b->mTitle), &value);
  if (value == 0) {
    // resolve by URI
    if (a->IsURI()) {
      nsNavHistoryURIResultNode* aURI = a->GetAsURI();
      nsNavHistoryURIResultNode* bURI = b->GetAsURI();
      value = aURI->mURI.Compare(bURI->mURI.get());
    }
    if (value == 0) {
      // resolve by date
      return ComparePRTime(a->mTime, b->mTime);
    }
  }
  return value;
}
PRInt32 PR_CALLBACK nsNavHistoryContainerResultNode::SortComparison_TitleGreater(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  return -SortComparison_TitleLess(a, b, closure);
}

// nsNavHistoryContainerResultNode::SortComparison_Date*
//
//    Equal times will be very unusual, but it is important that there is some
//    deterministic ordering of the results so they don't move around.

PRInt32 PR_CALLBACK nsNavHistoryContainerResultNode::SortComparison_DateLess(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  PRInt32 value = ComparePRTime(a->mTime, b->mTime);
  if (value == 0) {
    nsICollation* collation = NS_STATIC_CAST(nsICollation*, closure);
    collation->CompareString(
        nsICollation::kCollationCaseInSensitive,
        NS_ConvertUTF8toUTF16(a->mTitle),
        NS_ConvertUTF8toUTF16(b->mTitle), &value);
  }
  return value;
}
PRInt32 PR_CALLBACK nsNavHistoryContainerResultNode::SortComparison_DateGreater(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  PRInt32 value = -ComparePRTime(a->mTime, b->mTime);
  if (value == 0) {
    nsICollation* collation = NS_STATIC_CAST(nsICollation*, closure);
    collation->CompareString(
        nsICollation::kCollationCaseInSensitive,
        NS_ConvertUTF8toUTF16(a->mTitle),
        NS_ConvertUTF8toUTF16(b->mTitle), &value);
    value = -value;
  }
  return value;
}


// nsNavHistoryContainerResultNode::SortComparison_URI*
//
//    Certain types of parent nodes are treated specially because URIs are not
//    valid (like days or hosts).

PRInt32 PR_CALLBACK nsNavHistoryContainerResultNode::SortComparison_URILess(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  PRUint32 aType, bType;
  a->GetType(&aType);
  b->GetType(&bType);

  if (aType != bType) {
    // comparing different types
    return bType - aType;
  }

  PRInt32 value;
  if (a->IsURI()) {
    // normal URI or visit
    nsNavHistoryURIResultNode* aURI = a->GetAsURI();
    nsNavHistoryURIResultNode* bURI = b->GetAsURI();
    value = aURI->mURI.Compare(bURI->mURI.get());
  } else {
    // for everything else, use title (= host name)
    nsICollation* collation = NS_STATIC_CAST(nsICollation*, closure);
    collation->CompareString(
        nsICollation::kCollationCaseInSensitive,
        NS_ConvertUTF8toUTF16(a->mTitle),
        NS_ConvertUTF8toUTF16(b->mTitle), &value);
  }

  // resolve conflicts using date
  if (value == 0) {
    return ComparePRTime(a->mTime, b->mTime);
  }
  return value;
}
PRInt32 PR_CALLBACK nsNavHistoryContainerResultNode::SortComparison_URIGreater(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  return -SortComparison_URILess(a, b, closure);
}


// nsNavHistoryContainerResultNode::SortComparison_VisitCount*
//
//    Fall back on dates for conflict resolution

PRInt32 PR_CALLBACK nsNavHistoryContainerResultNode::SortComparison_VisitCountLess(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  PRInt32 value = CompareIntegers(a->mAccessCount, b->mAccessCount);
  if (value == 0)
    return ComparePRTime(a->mTime, b->mTime);
  return value;
}
PRInt32 PR_CALLBACK nsNavHistoryContainerResultNode::SortComparison_VisitCountGreater(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  PRInt32 value = -CompareIntegers(a->mAccessCount, b->mAccessCount);
  if (value == 0)
    return -ComparePRTime(a->mTime, b->mTime);
  return value;
}


// nsNavHistoryContainerResultNode::FindChildURI
//
//    Searches this folder for a node with the given URI. Returns null if not
//    found. Does not addref the node!

nsNavHistoryURIResultNode*
nsNavHistoryContainerResultNode::FindChildURI(const nsACString& aSpec,
    PRUint32* aNodeIndex)
{
  for (PRInt32 i = 0; i < mChildren.Count(); i ++) {
    if (mChildren[i]->IsURI()) {
      nsNavHistoryURIResultNode* uri = mChildren[i]->GetAsURI();
      if (aSpec.Equals(uri->mURI)) {
        *aNodeIndex = i;
        return uri;
      }
    }
  }
  return nsnull;
}


// nsNavHistoryContainerResultNode::FindChildFolder
//
//    Searches this folder for the given subfolder. Returns null if not found.
//    DOES NOT ADDREF.

nsNavHistoryFolderResultNode*
nsNavHistoryContainerResultNode::FindChildFolder(PRInt64 aFolderId,
    PRUint32* aNodeIndex)
{
  for (PRInt32 i = 0; i < mChildren.Count(); i ++) {
    if (mChildren[i]->IsFolder()) {
      nsNavHistoryFolderResultNode* folder = mChildren[i]->GetAsFolder();
      if (folder->mFolderId == aFolderId) {
        *aNodeIndex = i;
        return folder;
      }
    }
  }
  return nsnull;
}


//  nsNavHistoryContainerResultNode::FindChildContainerByName
//
//    Searches this container for a subfolder with the given name. This is used
//    to find host and "day" nodes. Returns null if not found. DOES NOT ADDREF.

nsNavHistoryContainerResultNode*
nsNavHistoryContainerResultNode::FindChildContainerByName(
    const nsACString& aTitle, PRUint32* aNodeIndex)
{
  for (PRInt32 i = 0; i < mChildren.Count(); i ++) {
    if (mChildren[i]->IsContainer()) {
      nsNavHistoryContainerResultNode* container =
          mChildren[i]->GetAsContainer();
      if (container->mTitle.Equals(aTitle)) {
        *aNodeIndex = i;
        return container;
      }
    }
  }
  return nsnull;
}


// nsNavHistoryContainerResultNode::InsertChildAt
//
//    This does the work of adding a child to the container. This child can be
//    a container, or a single item that may even be collapsed with the
//    adjacent ones.
//
//    Some inserts are "temporary" meaning that they are happening immediately
//    after a temporary remove. We do this when movings elements when they
//    change to keep them in the proper sorting position. In these cases, we
//    don't need to recompute any statistics.

nsresult
nsNavHistoryContainerResultNode::InsertChildAt(nsNavHistoryResultNode* aNode,
                                               PRInt32 aIndex,
                                               PRBool aIsTemporary)
{
  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);

  aNode->mVisibleIndex = -1;
  aNode->mParent = this;
  aNode->mIndentLevel = mIndentLevel + 1;
  if (! aIsTemporary && aNode->IsContainer()) {
    // need to update all the new item's children
    nsNavHistoryContainerResultNode* container = aNode->GetAsContainer();
    container->mResult = mResult;
    container->FillStats();
  }

  if (! mChildren.InsertObjectAt(aNode, aIndex))
    return NS_ERROR_OUT_OF_MEMORY;

  // update our (the container) stats and refresh our row on the screen
  if (! aIsTemporary) {
    mAccessCount += aNode->mAccessCount;
    if (mTime < aNode->mTime)
      mTime = aNode->mTime;
    if (mVisibleIndex >= 0)
      result->RowChanged(mVisibleIndex);
    ReverseUpdateStats(aNode->mAccessCount);
  }

  // Update tree if we are visible. Note that we could be here and not expanded,
  // like when there is a bookmark folder being updated because its parent is
  // visible.
  if (AreChildrenVisible()) {
    // this node will take the visible index of the element that used to be at
    // its location
    PRInt32 nodeVisibleIndex = -1;

    if (aIndex == 0) {
      // item is the first thing in our child list, it takes our index +1. Note
      // that this computation still works if we are an invisible root node,
      // because our index + 1 = -1 + 1 = 0
      nodeVisibleIndex = mVisibleIndex + 1;
    } else {
      // Here, we try to find the next visible element in the child list so we
      // can set the new visible index to be right before that. Note that we
      // have to search DOWN instead of up, because some siblings could have
      // children themselves that would be in the way.
      for (PRInt32 i = aIndex + 1; i < mChildren.Count(); i ++) {
        if (mChildren[i]->mVisibleIndex >= 0) {
          // the visible indices of subsequent children have not been shiftedn
          // so the next item will have what should be our index
          nodeVisibleIndex = mChildren[i]->mVisibleIndex;
          break;
        }
      }
      if (nodeVisibleIndex < 0) {
        // At the end of our child list without finding a visible sibling: This
        // is a little harder because we don't know how many rows the last item
        // in our list takes up (it could be a container with many children).
        PRUint32 lastRowCount =
          result->CountVisibleRowsForItem(mChildren[aIndex - 1]);
        nodeVisibleIndex = mChildren[aIndex - 1]->mVisibleIndex + lastRowCount;
      }
    }

    // this will update the node's visible index
    result->RowAdded(nodeVisibleIndex, aNode);
    if (aNode->IsContainer() && aNode->GetAsContainer()->mExpanded)
      result->RefreshVisibleSection(aNode->GetAsContainer());
  }

  return NS_OK;
}


// nsNavHistoryContainerResultNode::InsertSortedChild
//
//    This locates the proper place for insertion according to the current sort
//    and calls InsertChildAt

nsresult
nsNavHistoryContainerResultNode::InsertSortedChild(
    nsNavHistoryResultNode* aNode, PRBool aIsTemporary)
{

  if (mChildren.Count() == 0)
    return InsertChildAt(aNode, 0, aIsTemporary);

  SortComparator comparator = GetSortingComparator(GetSortType());
  if (comparator) {
    // When inserting a new node, it must have proper statistics because we use
    // them to find the correct insertion point. The insert function will then
    // recompute these statistics and fill in the proper parents and hierarchy
    // level. Doing this twice shouldn't be a large performance penalty because
    // when we are inserting new containers, they typically contain only one
    // item (because we've browsed a new page).
    if (! aIsTemporary && aNode->IsContainer())
      aNode->GetAsContainer()->FillStats();

    return InsertChildAt(aNode, FindInsertionPoint(aNode, comparator),
                         aIsTemporary);
  }
  return InsertChildAt(aNode, mChildren.Count(), aIsTemporary);
}


// nsNavHistoryContainerResultNode::MergeResults
//
//    This takes a fully grouped list of nodes and merges them into the
//    current result set. Any containers that are added must already be
//    sorted.
//
//    This assumes that the items in 'aAddition' are new visits or
//    replacement URIs. We do not update visits.
//
//    PERFORMANCE: In the future, we can do more updates incrementally using
//    this function. When a URI changes in a way we can't easily handle,
//    construct a query with each query object specifying an exact match for
//    the URI in question. Then remove all instances of that URI in the result
//    and call this function.

void
nsNavHistoryContainerResultNode::MergeResults(
    nsCOMArray<nsNavHistoryResultNode>* aAddition)
{
  // Generally we will have very few (one) entries in the addition list, so
  // just iterate through it. If we find we may have a lot, we may want to do
  // some hashing to help with the merge.
  for (PRUint32 i = 0; i < PRUint32(aAddition->Count()); i ++) {
    nsNavHistoryResultNode* curAddition = (*aAddition)[i];
    if (curAddition->IsContainer()) {
      PRUint32 containerIndex;
      nsNavHistoryContainerResultNode* container =
        FindChildContainerByName(curAddition->mTitle, &containerIndex);
      if (container) {
        // recursively merge with the existing container
        container->MergeResults(&curAddition->GetAsContainer()->mChildren);
      } else {
        // need to add the new container to our result.
        InsertSortedChild(curAddition);
      }
    } else {
      if (curAddition->IsVisit()) {
        // insert the new visit
        InsertSortedChild(curAddition);
      } else {
        // add the URI, replacing a current one if any
        PRUint32 oldIndex;
        nsNavHistoryURIResultNode* oldNode =
          FindChildURI(curAddition->GetAsURI()->mURI, &oldIndex);
        if (oldNode)
          ReplaceChildURIAt(oldIndex, curAddition->GetAsURI());
        else
          InsertSortedChild(curAddition);
      }
    }
  }
}


// nsNavHistoryContainerResultNode::ReplaceChildURIAt
//
//    This is called to replace a leaf node. It will update tree stats
//    and redraw the view if any. You can not use this to replace a container.
//
//    This assumes that the node is being replaced with a newer version of
//    itself and so its visit count will not go down. Also, this means that the
//    collapsing of duplicates will not change.

nsresult
nsNavHistoryContainerResultNode::ReplaceChildURIAt(PRUint32 aIndex,
    nsNavHistoryURIResultNode* aNode)
{
  NS_ASSERTION(aIndex < PRUint32(mChildren.Count()),
               "Invalid index for replacement");
  NS_ASSERTION(mChildren[aIndex]->IsURI(),
               "Can not use ReplaceChildAt for a node of another type");
  NS_ASSERTION(mChildren[aIndex]->GetAsURI()->mURI.Equals(aNode->mURI),
               "We must replace a URI with an updated one of the same");

  aNode->mParent = this;
  aNode->mIndentLevel = mIndentLevel + 1;

  // update tree stats if needed
  PRUint32 accessCountChange = aNode->mAccessCount - mChildren[aIndex]->mAccessCount;
  if (accessCountChange != 0 || mChildren[aIndex]->mTime != aNode->mTime) {
    NS_ASSERTION(aNode->mAccessCount >= mChildren[aIndex]->mAccessCount,
                 "Replacing a node with one back in time or some nonmatching node");

    mAccessCount += accessCountChange;
    if (mTime < aNode->mTime)
      mTime = aNode->mTime;
    ReverseUpdateStats(accessCountChange);
  }

  // clear out old child and replace
  PRInt32 visibleIndex = mChildren[aIndex]->mVisibleIndex;
  mChildren[aIndex]->OnRemoving();
  if (! mChildren.ReplaceObjectAt(aNode, aIndex))
    return NS_ERROR_FAILURE;

  // update screen
  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);
  if (mVisibleIndex >= 0)
    result->RowReplaced(visibleIndex, aNode);
  return NS_OK;
}


// nsNavHistoryContainerResultNode::RemoveChildAt
//
//    This does all the work of removing a child from this container, including
//    updating the tree if necessary. Note that we do not need to be open for
//    this to work.
//
//    Some removes are "temporary" meaning that they'll just get inserted
//    again. We do this for resorting. In these cases, we don't need to
//    recompute any statistics, and we shouldn't notify those container that
//    they are being removed.

nsresult
nsNavHistoryContainerResultNode::RemoveChildAt(PRInt32 aIndex,
                                               PRBool aIsTemporary)
{
  NS_ASSERTION(aIndex >= 0 && aIndex < mChildren.Count(), "Invalid index");

  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);

  PRBool isContainer = mChildren[aIndex]->IsContainer();
  PRBool isContainerOpen = PR_FALSE;
  PRInt32 visibleRows = 1;
  if (isContainer) {
    isContainerOpen = mChildren[aIndex]->GetAsContainer()->mExpanded;
    if (isContainerOpen)
      visibleRows = result->CountVisibleRowsForItem(mChildren[aIndex]);
  }
  PRInt32 childVisibleIndex = mChildren[aIndex]->mVisibleIndex;

  // stats
  PRUint32 oldAccessCount = 0;
  if (! aIsTemporary) {
    oldAccessCount = mAccessCount;
    mAccessCount -= mChildren[aIndex]->mAccessCount;
    NS_ASSERTION(mAccessCount >= 0, "Invalid access count while updating!");

    mChildren[aIndex]->OnRemoving();
  }

  // remove from our list and notify the tree
  mChildren.RemoveObjectAt(aIndex);
  if (AreChildrenVisible() && childVisibleIndex >= 0)
    result->RowsRemoved(childVisibleIndex, visibleRows);

  if (! aIsTemporary)
    ReverseUpdateStats(mAccessCount - oldAccessCount);
  return NS_OK;
}


// nsNavHistoryContainerResultNode::CanRemoteContainersChange
//
//    Returns true if remote containers can manipulate the contents of this
//    container. This is false for folders and queries, true for everything
//    else.

PRBool
nsNavHistoryContainerResultNode::CanRemoteContainersChange()
{
  return (mContainerType != nsINavHistoryResultNode::RESULT_TYPE_FOLDER &&
          mContainerType != nsINavHistoryResultNode::RESULT_TYPE_QUERY);
}


// nsNavHistoryContainerResultNode::GetHasChildren
//
//    Complex containers (folders and queries) will override this. Here, we
//    handle the case of simple containers (like host groups) where the children
//    are always stored.

NS_IMETHODIMP
nsNavHistoryContainerResultNode::GetHasChildren(PRBool *aHasChildren)
{
  *aHasChildren = (mChildren.Count() > 0);
  return NS_OK;
}


// nsNavHistoryContainerResultNode::GetChildCount
//
//    This function is only valid when the node is opened.

NS_IMETHODIMP
nsNavHistoryContainerResultNode::GetChildCount(PRUint32* aChildCount)
{
  if (! mExpanded)
    return NS_ERROR_NOT_AVAILABLE;
  *aChildCount = mChildren.Count();
  return NS_OK;
}


// nsNavHistoryContainerResultNode::GetChild

NS_IMETHODIMP
nsNavHistoryContainerResultNode::GetChild(PRUint32 aIndex,
                                          nsINavHistoryResultNode** _retval)
{
  if (! mExpanded)
    return NS_ERROR_NOT_AVAILABLE;
  if (aIndex >= PRUint32(mChildren.Count()))
    return NS_ERROR_INVALID_ARG;
  NS_ADDREF(*_retval = mChildren[aIndex]);
  return NS_OK;
}


// nsNavHistoryContainerResultNode::GetChildrenReadOnly

NS_IMETHODIMP
nsNavHistoryContainerResultNode::GetChildrenReadOnly(PRBool *aChildrenReadOnly)
{
  *aChildrenReadOnly = mChildrenReadOnly;
  return NS_OK;
}


// nsNavHistoryContainerResultNode::GetRemoteContainerType

NS_IMETHODIMP
nsNavHistoryContainerResultNode::GetRemoteContainerType(
    nsACString& aRemoteContainerType)
{
  aRemoteContainerType = mRemoteContainerType;
  return NS_OK;
}


// nsNavHistoryContainerResultNode::AppendURINode

NS_IMETHODIMP
nsNavHistoryContainerResultNode::AppendURINode(
    const nsACString& aTitle, PRUint32 aAccessCount, PRTime aTime,
    const nsACString& aIconURI, const nsACString& aURI,
    nsINavHistoryURIResultNode** _retval)
{
  *_retval = nsnull;
  if (mRemoteContainerType.IsEmpty() || ! CanRemoteContainersChange())
    return NS_ERROR_INVALID_ARG; // we must be a remote container

  nsRefPtr<nsNavHistoryURIResultNode> result =
      new nsNavHistoryURIResultNode(aTitle, aAccessCount, aTime, aIconURI, aURI);
  NS_ENSURE_TRUE(result, NS_ERROR_OUT_OF_MEMORY);

  // append to our list
  if (! mChildren.AppendObject(result))
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*_retval = result);
  return NS_OK;
}


// nsNavHistoryContainerResultNode::AppendVisitNode

NS_IMETHODIMP
nsNavHistoryContainerResultNode::AppendVisitNode(
    const nsACString& aTitle, PRUint32 aAccessCount, PRTime aTime,
    const nsACString& aIconURI, const nsACString& aURI, PRInt64 aSession,
    nsINavHistoryVisitResultNode** _retval)
{
  *_retval = nsnull;
  if (mRemoteContainerType.IsEmpty() || ! CanRemoteContainersChange())
    return NS_ERROR_INVALID_ARG; // we must be a remote container

  nsRefPtr<nsNavHistoryVisitResultNode> result =
      new nsNavHistoryVisitResultNode(aTitle, aAccessCount, aTime,
                                      aIconURI, aURI, aSession);
  NS_ENSURE_TRUE(result, NS_ERROR_OUT_OF_MEMORY);

  // append to our list
  if (! mChildren.AppendObject(result))
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*_retval = result);
  return NS_OK;
}


// nsNavHistoryContainerResultNode::AppendFullVisitNode

NS_IMETHODIMP
nsNavHistoryContainerResultNode::AppendFullVisitNode(
    const nsACString& aTitle, PRUint32 aAccessCount, PRTime aTime,
    const nsACString& aIconURI, const nsACString& aURI, PRInt64 aSession,
    PRInt64 aVisitId, PRInt64 aReferringVisitId, PRInt32 aTransitionType,
    nsINavHistoryFullVisitResultNode** _retval)
{
  *_retval = nsnull;
  if (mRemoteContainerType.IsEmpty() || ! CanRemoteContainersChange())
    return NS_ERROR_INVALID_ARG; // we must be a remote container

  nsRefPtr<nsNavHistoryFullVisitResultNode> result =
      new nsNavHistoryFullVisitResultNode(aTitle, aAccessCount, aTime,
                                          aIconURI, aURI, aSession,
                                          aVisitId, aReferringVisitId,
                                          aTransitionType);
  NS_ENSURE_TRUE(result, NS_ERROR_OUT_OF_MEMORY);

  // append to our list
  if (! mChildren.AppendObject(result))
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*_retval = result);
  return NS_OK;
}


// nsNavHistoryContainerResultNode::AppendContainerNode

NS_IMETHODIMP
nsNavHistoryContainerResultNode::AppendContainerNode(
    const nsACString& aTitle, const nsACString& aIconURI,
    PRUint32 aContainerType, const nsACString& aRemoteContainerType,
    nsINavHistoryContainerResultNode** _retval)
{
  *_retval = nsnull;
  if (mRemoteContainerType.IsEmpty() || ! CanRemoteContainersChange())
    return NS_ERROR_INVALID_ARG; // we must be a remote container
  if (! IsTypeContainer(aContainerType) || IsTypeFolder(aContainerType) ||
      IsTypeQuery(aContainerType))
    return NS_ERROR_INVALID_ARG; // not proper container type
  if (aContainerType == nsINavHistoryResultNode::RESULT_TYPE_REMOTE_CONTAINER &&
      aRemoteContainerType.IsEmpty())
    return NS_ERROR_INVALID_ARG; // remote containers must have r.c. type
  if (aContainerType != nsINavHistoryResultNode::RESULT_TYPE_REMOTE_CONTAINER &&
      ! aRemoteContainerType.IsEmpty())
    return NS_ERROR_INVALID_ARG; // non-remote containers must NOT have r.c. type

  nsRefPtr<nsNavHistoryContainerResultNode> result =
      new nsNavHistoryContainerResultNode(aTitle, aIconURI, aContainerType,
                                          PR_TRUE, aRemoteContainerType);
  NS_ENSURE_TRUE(result, NS_ERROR_OUT_OF_MEMORY);

  // append to our list
  if (! mChildren.AppendObject(result))
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*_retval = result);
  return NS_OK;
}


// nsNavHistoryContainerResultNode::AppendQueryNode

NS_IMETHODIMP
nsNavHistoryContainerResultNode::AppendQueryNode(
    const nsACString& aTitle, const nsACString& aIconURI,
    const nsACString& aQueryString, nsINavHistoryQueryResultNode** _retval)
{
  *_retval = nsnull;
  if (mRemoteContainerType.IsEmpty() || ! CanRemoteContainersChange())
    return NS_ERROR_INVALID_ARG; // we must be a remote container

  nsRefPtr<nsNavHistoryQueryResultNode> result =
      new nsNavHistoryQueryResultNode(aTitle, aIconURI, aQueryString);
  NS_ENSURE_TRUE(result, NS_ERROR_OUT_OF_MEMORY);

  // append to our list
  if (! mChildren.AppendObject(result))
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*_retval = result);
  return NS_OK;
}


// nsNavHistoryContainerResultNode::AppendFolderNode

NS_IMETHODIMP
nsNavHistoryContainerResultNode::AppendFolderNode(
    PRInt64 aFolderId, nsINavHistoryFolderResultNode** _retval)
{
  *_retval = nsnull;
  if (mRemoteContainerType.IsEmpty() || ! CanRemoteContainersChange())
    return NS_ERROR_INVALID_ARG; // we must be a remote container

  nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
  NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);

  // create the node, it will be addrefed for us
  nsRefPtr<nsNavHistoryResultNode> result;
  nsresult rv = bookmarks->ResultNodeForFolder(aFolderId,
                                               GetGeneratingOptions(),
                                               getter_AddRefs(result));
  NS_ENSURE_SUCCESS(rv, rv);

  // append to our list
  if (! mChildren.AppendObject(result))
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*_retval = result->GetAsFolder());
  return NS_OK;
}


// nsNavHistoryContainerResultNode::ClearContents
//
//    Used by the remote container API to clear this container

NS_IMETHODIMP
nsNavHistoryContainerResultNode::ClearContents()
{
  if (mRemoteContainerType.IsEmpty() || ! CanRemoteContainersChange())
    return NS_ERROR_INVALID_ARG; // we must be a remote container

  // we know if CanRemoteContainersChange() then we are a regular container
  // and not a query or folder, so clearing doesn't need anything else to
  // happen (like unregistering observers). Also, since this should only
  // happen when the container is closed, we don't need to redraw the screen.
  mChildren.Clear();

  PRUint32 oldAccessCount = mAccessCount;
  mAccessCount = 0;
  mTime = 0;
  ReverseUpdateStats(-oldAccessCount);
  return NS_OK;
}


// nsNavHistoryQueryResultNode *************************************************
//
//    HOW QUERY UPDATING WORKS
//
//    Queries are different than bookmark folders in that we can not always
//    do dynamic updates (easily) and updates are more expensive. Therefore,
//    we do NOT query if we are not open and want to see if we have any children
//    (for drawing a twisty) and always assume we will.
//
//    When the container is opened, we execute the query and register the
//    listeners. Like bookmark folders, we stay registered even when closed,
//    and clear ourselves as soon as a message comes in. This lets us respond
//    quickly if the user closes and reopens the container.
//
//    We try to handle the most common notifications for the most common query
//    types dynamically, that is, figuring out what should happen in response
//    to a message without doing a requery. For complex changes or complex
//    queries, we give up and requery.

NS_IMPL_ISUPPORTS_INHERITED1(nsNavHistoryQueryResultNode,
                             nsNavHistoryContainerResultNode,
                             nsINavHistoryQueryResultNode)

nsNavHistoryQueryResultNode::nsNavHistoryQueryResultNode(
    const nsACString& aTitle, const nsACString& aIconURI,
    const nsACString& aQueryURI) :
  nsNavHistoryContainerResultNode(aTitle, aIconURI,
                                  nsINavHistoryResultNode::RESULT_TYPE_QUERY,
                                  PR_TRUE, EmptyCString()),
  mQueryURI(aQueryURI),
  mHasSearchTerms(PR_FALSE),
  mContentsValid(PR_FALSE),
  mBatchInProgress(PR_FALSE)
{
  // queries have special icons if not otherwise set
  if (mFaviconURI.IsEmpty())
    mFaviconURI.AppendLiteral(ICONURI_QUERY);
}

nsNavHistoryQueryResultNode::nsNavHistoryQueryResultNode(
    const nsACString& aTitle, const nsACString& aIconURI,
    const nsCOMArray<nsNavHistoryQuery>& aQueries,
    nsNavHistoryQueryOptions* aOptions) :
  nsNavHistoryContainerResultNode(aTitle, aIconURI,
                                  nsINavHistoryResultNode::RESULT_TYPE_QUERY,
                                  PR_TRUE, EmptyCString()),
  mQueries(aQueries),
  mOptions(aOptions),
  mContentsValid(PR_FALSE),
  mBatchInProgress(PR_FALSE)
{
  NS_ASSERTION(aQueries.Count() > 0, "Must have at least one query");
  /*
  for (PRUint32 i = 0; i < aQueryCount; i ++)
    mQueries.AppendObject(aQueries[i]);
  aOptions->Clone(getter_AddRefs(mOptions));
*/

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ASSERTION(history, "History service missing");
  mLiveUpdate = history->GetUpdateRequirements(mQueries, mOptions,
                                               &mHasSearchTerms);

  // queries have special icons if not otherwise set
  if (mFaviconURI.IsEmpty())
    mFaviconURI.AppendLiteral(ICONURI_QUERY);
}


// nsNavHistoryQueryResultNode::CanExpand
//
//    Whoever made us may want non-expanding queries. However, we always
//    expand when we are the root node, or else asking for non-expanding
//    queries would be useless.

PRBool
nsNavHistoryQueryResultNode::CanExpand()
{
  nsNavHistoryQueryOptions* options = GetGeneratingOptions();
  if (options && options->ExpandQueries())
    return PR_TRUE;
  if (mResult && mResult->mRootNode == this)
    return PR_TRUE;
  return PR_FALSE;
}


// nsNavHistoryQueryResultNode::OnRemoving

void
nsNavHistoryQueryResultNode::OnRemoving()
{
  ClearChildren(PR_TRUE);
}


// nsNavHistoryQueryResultNode::OpenContainer
//
//    Marks the container as open, rebuilding results if they are invalid. We
//    may still have valid results if the container was previously open and
//    nothing happened since closing it.
//
//    We do not handle CloseContainer specially. The default one just marks the
//    container as closed, but doesn't actually mark the results as invalid.
//    The results will be invalidated by the next history or bookmark
//    notification that comes in. This means if you open and close the item
//    without anything happening in between, it will be fast (this actually
//    happens when results are used as menus).

nsresult
nsNavHistoryQueryResultNode::OpenContainer()
{
  NS_ASSERTION(! mExpanded, "Container must be expanded to close it");
  if (! CanExpand())
    return NS_OK;
  if (! mContentsValid) {
    nsresult rv = FillChildren();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mExpanded = PR_TRUE;
  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);
  return result->RefreshVisibleSection(this);
}


// nsNavHistoryQueryResultNode::GetHasChildren
//
//    When we have valid results we can always give an exact answer. When we
//    don't we just assume we'll have results, since actually doing the query
//    might be hard. This is used to draw twisties on the tree, so precise
//    results don't matter.

NS_IMETHODIMP
nsNavHistoryQueryResultNode::GetHasChildren(PRBool* aHasChildren)
{
  if (! CanExpand()) {
    *aHasChildren = PR_FALSE;
    return NS_OK;
  }
  if (mContentsValid) {
    *aHasChildren = (mChildren.Count() > 0);
    return NS_OK;
  }
  *aHasChildren = PR_TRUE;
  return NS_OK;
}


// nsNavHistoryQueryResultNode::GetQueryURI

NS_IMETHODIMP
nsNavHistoryQueryResultNode::GetQueryURI(nsIURI** aQueryURI)
{
  nsresult rv = VerifyQueriesSerialized();
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_NewURI(aQueryURI, mQueryURI);
}


// nsNavHistoryQueryResultNode::GetQueries

NS_IMETHODIMP
nsNavHistoryQueryResultNode::GetQueries(PRUint32* queryCount,
                                        nsINavHistoryQuery*** queries)
{
  nsresult rv = VerifyQueriesParsed();
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(mQueries.Count() > 0, "Must have >= 1 query");

  *queries = NS_STATIC_CAST(nsINavHistoryQuery**,
               nsMemory::Alloc(mQueries.Count() * sizeof(nsINavHistoryQuery*)));
  NS_ENSURE_TRUE(*queries, NS_ERROR_OUT_OF_MEMORY);

  for (PRInt32 i = 0; i < mQueries.Count(); ++i)
    NS_ADDREF((*queries)[i] = mQueries[i]);
  *queryCount = mQueries.Count();
  return NS_OK;
}


// nsNavHistoryQueryResultNode::GetQueryOptions

NS_IMETHODIMP
nsNavHistoryQueryResultNode::GetQueryOptions(
                                      nsINavHistoryQueryOptions** aQueryOptions)
{
  nsresult rv = VerifyQueriesParsed();
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(mOptions, "Options invalid");

  *aQueryOptions = mOptions;
  NS_ADDREF(*aQueryOptions);
  return NS_OK;
}


// nsNavHistoryQueryResultNode::VerifyQueriesParsed

nsresult
nsNavHistoryQueryResultNode::VerifyQueriesParsed()
{
  if (mQueries.Count() > 0) {
    NS_ASSERTION(mOptions, "If a result has queries, it also needs options");
    return NS_OK;
  }
  NS_ASSERTION(! mQueryURI.IsEmpty(),
               "Query nodes must have either a URI or query/options");

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = history->QueryStringToQueryArray(mQueryURI, &mQueries,
                                                 getter_AddRefs(mOptions));
  NS_ENSURE_SUCCESS(rv, rv);

  mLiveUpdate = history->GetUpdateRequirements(mQueries, mOptions,
                                               &mHasSearchTerms);
  return NS_OK;
}


// nsNavHistoryQueryResultNode::VerifyQueriesSerialized

nsresult
nsNavHistoryQueryResultNode::VerifyQueriesSerialized()
{
  if (! mQueryURI.IsEmpty()) {
    return NS_OK;
  }
  NS_ASSERTION(mQueries.Count() > 0 && mOptions,
               "Query nodes must have either a URI or query/options");

  nsTArray<nsINavHistoryQuery*> flatQueries;
  flatQueries.SetCapacity(mQueries.Count());
  for (PRInt32 i = 0; i < mQueries.Count(); i ++)
    flatQueries.AppendElement(mQueries.ObjectAt(i));

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = history->QueriesToQueryString(flatQueries.Elements(),
                                              flatQueries.Length(), mOptions, mQueryURI);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(! mQueryURI.IsEmpty(), NS_ERROR_FAILURE);
  return NS_OK;
}


// nsNavHistoryQueryResultNode::FillChildren

nsresult
nsNavHistoryQueryResultNode::FillChildren()
{
  NS_ASSERTION(! mContentsValid,
               "Don't call FillChildren when contents are valid");
  NS_ASSERTION(mChildren.Count() == 0,
               "We are trying to fill children when there already are some");

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);

  // get the results from the history service
  nsresult rv = VerifyQueriesParsed();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = history->GetQueryResults(mQueries, mOptions, &mChildren);
  NS_ENSURE_SUCCESS(rv, rv);

  // it is important to call FillStats to fill in the parents on all
  // nodes and the result node pointers on the containers
  FillStats();

  // once we've computed all tree stats, we can sort, because containers will
  // not have proper visit counts and dates
  SortComparator comparator = GetSortingComparator(GetSortType());
  if (comparator)
    RecursiveSort(history->GetCollation(), comparator);

  // register with the result for updates
  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);
  result->AddEverythingObserver(this);

  mContentsValid = PR_TRUE;
  return NS_OK;
}


// nsNavHistoryQueryResultNode::ClearChildren
//
//    Call with unregister = false when we are going to update the children (for
//    exmple, when the container is open). This will clear the list and notify
//    all the children that they are going away.
//
//    When the results are becoming invalid and we are not going to refresh
//    them, set unregister = true, which will unregister the listener from the
//    result if any. We use unregister = false when we are refreshing the list
//    immediately so want to stay a notifier.

void
nsNavHistoryQueryResultNode::ClearChildren(PRBool aUnregister)
{
  for (PRInt32 i = 0; i < mChildren.Count(); i ++)
    mChildren[i]->OnRemoving();
  mChildren.Clear();
  mContentsValid = PR_FALSE;

  if (aUnregister) {
    nsNavHistoryResult* result = GetResult();
    if (result)
      result->RemoveEverythingObserver(this);
  }
}


// nsNavHistoryQueryResultNode::Refresh
//
//    This is called to update the result when something has changed that we
//    can not incrementally update.

nsresult
nsNavHistoryQueryResultNode::Refresh()
{
  // Ignore refreshes when there is a batch, EndUpdateBatch will do a refresh
  // to get all the changes.
  if (mBatchInProgress)
    return NS_OK;

  if (! mExpanded) {
    // when we are not expanded, we don't update, just invalidate and unhook
    ClearChildren(PR_TRUE);
    return NS_OK; // no updates in tree state
  }

  // ignore errors from FillChildren, since we will still want to refresh
  // the tree (there just might not be anything in it on error)
  ClearChildren(PR_FALSE);
  FillChildren();

  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);
  return result->RefreshVisibleSection(this);
}


// nsNavHistoryQueryResultNode::GetSortType
//
//    Here, we override GetSortType to return the current sorting for this
//    query. GetSortType is used when dynamically inserting query results so we
//    can see which comparator we should use to find the proper insertion point
//    (it shouldn't be called from folder containers which maintain their own
//    sorting).
//
//    Normally, the container just forwards it up the chain. This is what
//    we want for host groups, for example. For queries, we often want to
//    use the query's sorting mode.
//
//    However, we only use this query node's sorting when it is not the root.
//    When it is the root, we use the result's sorting mode, which is set
//    according to the column headers in the tree view (if attached). This is
//    because there are two cases:
//
//    - You are looking at a bookmark hierarchy that contains an embedded
//      result. We should always use the query's sort ordering since the result
//      node's headers have nothing to do with us (and are disabled).
//
//    - You are looking at a query in the tree. In this case, we want the
//      result sorting to override ours (it should be initialized to the same
//      sorting mode).
//
//    It might seem like the column headers should set the sorting mode for the
//    query in the result so we don't have to have this special case. But, the
//    UI allows you to save the query in a different place or edit it, and just
//    grabs the parameters and options from the query node. It would be weird
//    to build a query, click a column header, and have the options you built
//    up in the query builder be changed from under you.

PRUint32
nsNavHistoryQueryResultNode::GetSortType()
{
  if (mParent) {
    // use our sorting, we are not the root
    return mOptions->SortingMode();
  } else if (mResult) {
    return mResult->mSortingMode;
  }
  NS_NOTREACHED("We should always have a result");
  return nsINavHistoryQueryOptions::SORT_BY_NONE;
}


// nsNavHistoryQueryResultNode::UpdateURIs
//
//    If aUpdateSort is true, we will also update the sorting of this item.
//    Normally you want this to be true, but it can be false if the thing you
//    are changing can not affect sorting (like favicons).
//
//    You should NOT change any child lists as part of the callback function.

void
nsNavHistoryQueryResultNode::UpdateURIs(PRBool aOnlyOne, PRBool aUpdateSort,
    const nsCString& aSpec,
    void (*aCallback)(nsNavHistoryURIResultNode*,void*), void* aClosure)
{
  nsNavHistoryResult* result = GetResult();
  if (! result) {
    NS_NOTREACHED("Must have a result for this query");
    return;
  }

  // this needs to be owning since sometimes we remove and re-insert nodes
  // in their parents and we don't want them to go away.
  nsCOMArray<nsNavHistoryResultNode> matches;

  RecursiveFindURIs(aOnlyOne, this, aSpec, &matches);
  if (matches.Count() == 0)
    return;

  SortComparator comparator = nsnull;
  if (aUpdateSort)
    comparator = GetSortingComparator(GetSortType());

  // PERFORMANCE: This updates each container for each child in it that
  // changes. In some cases, many elements have changed inside the same
  // container. It would be better to compose a list of containers, and
  // update each one only once for all the items that have changed in it.
  for (PRInt32 i = 0; i < matches.Count(); i ++)
  {
    nsNavHistoryResultNode* node = matches[i];
    nsNavHistoryContainerResultNode* parent = node->mParent;
    if (! parent) {
      NS_NOTREACHED("All URI nodes being updated must have parents");
      continue;
    }
    PRUint32 oldAccessCount = node->mAccessCount;
    PRTime oldTime = node->mTime;
    aCallback(node->GetAsURI(), aClosure);

    if (oldAccessCount != node->mAccessCount || oldTime != node->mTime) {
      // need to update/redraw the parent
      parent->mAccessCount += node->mAccessCount - oldAccessCount;
      if (node->mTime > parent->mTime)
        parent->mTime = node->mTime;
      if (parent->mVisibleIndex >= 0)
        result->RowChanged(parent->mVisibleIndex);
      parent->ReverseUpdateStats(node->mAccessCount - oldAccessCount);
    }

    if (aUpdateSort) {
      PRInt32 childIndex = parent->FindChild(node);
      if (childIndex >= 0 && DoesChildNeedResorting(childIndex, comparator)) {
        // child position changed
        parent->RemoveChildAt(childIndex, PR_TRUE);
        parent->InsertChildAt(node, parent->FindInsertionPoint(node, comparator),
                              PR_TRUE);
      } else if (node->mVisibleIndex >= 0) {
        result->RowChanged(node->mVisibleIndex);
      }
    } else if (node->mVisibleIndex >= 0) {
      result->RowChanged(node->mVisibleIndex);
    }
  }
}


// nsNavHistoryQueryResultNode::RecursiveFindURIs
//
//    This function searches for matches for the given URI.
//
//    If aOnlyOne is set, it will terminate as soon as it finds a single match.
//    (The return value of TRUE in this case indicates that a match was found).
//    This would be used when there are URI results so there will only ever be
//    one copy of any URI.
//
//    When aOnlyOne is false, it will check all elements. This is for visit
//    style results that may have multiple copies of any given URI. In this
//    mode, the return value is not used.

void
nsNavHistoryQueryResultNode::RecursiveFindURIs(PRBool aOnlyOne,
    nsNavHistoryContainerResultNode* aContainer, const nsCString& aSpec,
    nsCOMArray<nsNavHistoryResultNode>* aMatches)
{
  for (PRInt32 child = 0; child < aContainer->mChildren.Count(); child ++) {
    PRUint32 type;
    aContainer->mChildren[child]->GetType(&type);
    if (nsNavHistoryResultNode::IsTypeURI(type)) {
      // compare URIs
      nsNavHistoryURIResultNode* uriNode = aContainer->mChildren[child]->GetAsURI();
      if (uriNode->mURI.Equals(aSpec)) {
        // found
        aMatches->AppendObject(uriNode);
        if (aOnlyOne)
          return;
      }
    } else if (nsNavHistoryResultNode::IsTypeQuerySubcontainer(type)) {
      // search into sub-containers
      RecursiveFindURIs(aOnlyOne, aContainer->mChildren[child]->GetAsContainer(),
                        aSpec, aMatches);
      if (aOnlyOne && aMatches->Count() > 0)
        return;
    }
  }
}


// nsNavHistoryResultNode::OnBeginUpdateBatch

NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnBeginUpdateBatch()
{
  mBatchInProgress = PR_TRUE;
  return NS_OK;
}


// nsNavHistoryResultNode::OnEndUpdateBatch
//
//    Always requery when batches are done. These will typically involve large
//    operations (currently delete) and it is likely more efficient to requery
//    than to incrementally update as each message comes in.

NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnEndUpdateBatch()
{
  NS_ASSERTION(mBatchInProgress, "EndUpdateBatch without a begin");
  // must set to false before calling Refresh or Refresh will ignore us
  mBatchInProgress = PR_FALSE;
  return Refresh();
}


// nsNavHistoryQueryResultNode::OnVisit
//
//    Here we need to update all copies of the URI we have with the new visit
//    count, and potentially add a new entry in our query. This is the most
//    common update operation and it is important that it be as efficient as
//    possible.

NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnVisit(nsIURI* aURI, PRInt64 aVisitId,
                                     PRTime aTime, PRInt64 aSessionId,
                                     PRInt64 aReferringId,
                                     PRUint32 aTransitionType)
{
  // ignore everything during batches
  if (mBatchInProgress)
    return NS_OK;

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv;
  nsCOMPtr<nsNavHistoryResultNode> addition;
  switch(mLiveUpdate) {
    case QUERYUPDATE_TIME: {
      // For these simple yet common cases we can check the time ourselves
      // before doing the overhead of creating a new result node.
      NS_ASSERTION(mQueries.Count() == 1, "Time updated queries can have only one object");
      nsCOMPtr<nsNavHistoryQuery> query = do_QueryInterface(mQueries[0], &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      PRBool hasIt;
      query->GetHasBeginTime(&hasIt);
      if (hasIt) {
        PRTime beginTime = history->NormalizeTime(query->BeginTimeReference(),
                                                  query->BeginTime());
        if (aTime < beginTime)
          return NS_OK; // before our time range
      }
      query->GetHasEndTime(&hasIt);
      if (hasIt) {
        PRTime endTime = history->NormalizeTime(query->EndTimeReference(),
                                                query->EndTime());
        if (aTime > endTime)
          return NS_OK; // after our time range
      }
      // now we know that our visit satisfies the time range, create a new node
      rv = history->VisitIdToResultNode(aVisitId, mOptions,
                                        getter_AddRefs(addition));
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    }
    case QUERYUPDATE_SIMPLE: {
      // The history service can tell us whether the new item should appear
      // in the result. We first have to construct a node for it to check.
      rv = history->VisitIdToResultNode(aVisitId, mOptions,
                                        getter_AddRefs(addition));
      NS_ENSURE_SUCCESS(rv, rv);
      if (! history->EvaluateQueryForNode(mQueries, mOptions,
                                          addition->GetAsURI()))
        return NS_OK; // don't need to include in our query
      break;
    }
    case QUERYUPDATE_COMPLEX:
    case QUERYUPDATE_COMPLEX_WITH_BOOKMARKS:
      // need to requery in complex cases
      return Refresh();
    default:
      NS_NOTREACHED("Invalid value for mLiveUpdate");
      return Refresh();
  }

  PRUint32 groupCount;
  const PRUint32* groupings = mOptions->GroupingMode(&groupCount);
  nsCOMArray<nsNavHistoryResultNode> grouped;
  if (groupCount > 0) {
    // feed this one node into the results grouper for this query to see where
    // it should go in the results
    nsCOMArray<nsNavHistoryResultNode> itemSource;
    if (! itemSource.AppendObject(addition))
      return NS_ERROR_OUT_OF_MEMORY;
    history->RecursiveGroup(itemSource, groupings, groupCount, &grouped);
  } else {
    // no grouping
    if (! grouped.AppendObject(addition))
      return NS_ERROR_OUT_OF_MEMORY;
  }

  MergeResults(&grouped);
  return NS_OK;
}


// nsNavHistoryQueryResultNode::OnTitleChanged
//
//    Find every node that matches this URI and rename it. We try to do
//    incremental updates here, even when we are closed, because changing titles
//    is easier than requerying if we are invalid.
//
//    This actually gets called a lot. Typically, we will get an AddURI message
//    when the user visits the page, and then the title will be set asynchronously
//    when the title element of the page is parsed.

static void PR_CALLBACK setTitleCallback(
    nsNavHistoryURIResultNode* aNode, void* aClosure)
{
  const nsCString* newTitle = NS_REINTERPRET_CAST(nsCString*, aClosure);
  aNode->mTitle = *newTitle;
}
NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnTitleChanged(nsIURI* aURI,
                                            const nsAString& aPageTitle,
                                            const nsAString& aUserTitle,
                                            PRBool aIsUserTitleChanged)
{
  if (mBatchInProgress)
    return NS_OK; // ignore everything during batches
  if (! mExpanded) {
    // When we are not expanded, we don't update, just invalidate and unhook.
    // It would still be pretty easy to traverse the results and update the
    // titles, but when a title changes, its unlikely that it will be the only
    // thing. Therefore, we just give up.
    ClearChildren(PR_TRUE);
    return NS_OK; // no updates in tree state
  }

  // See if our queries have any keyword matching. In this case, we can't just
  // replace the title on the items, but must redo the query. This could be
  // handled more efficiently, but it is hard because keyword matching might
  // match anything, including the title and other things.
  if (mHasSearchTerms) {
    return Refresh();
  }

  // compute what the new title should be
  nsCAutoString newTitle;
  if (mOptions->ForceOriginalTitle() || aUserTitle.IsVoid()) {
    newTitle = NS_ConvertUTF16toUTF8(aPageTitle);
  } else {
    newTitle = NS_ConvertUTF16toUTF8(aUserTitle);
  }

  // uri string
  nsCAutoString uriString;
  nsresult rv = aURI->GetSpec(uriString);
  NS_ENSURE_SUCCESS(rv, rv);

  // The recursive function will update the result's tree nodes, but only if we
  // give it a non-null pointer. So if there isn't a tree, just pass NULL so
  // it doesn't bother trying to call the result.
  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);

  PRBool onlyOneEntry = (mOptions->ResultType() ==
                         nsINavHistoryQueryOptions::RESULTS_AS_URI);

  PRUint32 sortType = GetSortType();
  PRBool updateSorting =
    (sortType == nsINavHistoryQueryOptions::SORT_BY_TITLE_ASCENDING ||
     sortType == nsINavHistoryQueryOptions::SORT_BY_TITLE_DESCENDING);

  UpdateURIs(onlyOneEntry, updateSorting, uriString,
             setTitleCallback, &newTitle);

  // need to resort everything that was changed
  return NS_OK;
}


// nsNavHistoryQueryResultNode::OnDeleteURI
//
//    Here, we can always live update by just deleting all occurances of
//    the given URI.

NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnDeleteURI(nsIURI *aURI)
{
  PRBool onlyOneEntry = (mOptions->ResultType() ==
                         nsINavHistoryQueryOptions::RESULTS_AS_URI);
  nsCAutoString spec;
  nsresult rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMArray<nsNavHistoryResultNode> matches;
  RecursiveFindURIs(onlyOneEntry, this, spec, &matches);
  if (matches.Count() == 0)
    return NS_OK; // not found

  for (PRInt32 i = 0; i < matches.Count(); i ++) {
    nsNavHistoryResultNode* node = matches[i];
    nsNavHistoryContainerResultNode* parent = node->mParent;
    NS_ASSERTION(parent, "URI nodes should always have parents");

    PRInt32 childIndex = parent->FindChild(node);
    NS_ASSERTION(childIndex >= 0, "Child not found in parent");
    parent->RemoveChildAt(childIndex);

    if (parent->mChildren.Count() == 0 && parent->IsQuerySubcontainer()) {
      // when query subcontainers (like hosts) get empty we should remove them
      // as well. Just append this to our list and it will get evaluated later
      // in the loop.
      matches.AppendObject(parent);
    }
  }
  return NS_OK;
}


// nsNavHistoryQueryResultNode::OnClearHistory

NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnClearHistory()
{
  Refresh();
  return NS_OK;
}


// nsNavHistoryQueryResultNode::OnPageChanged
//

static void PR_CALLBACK setFaviconCallback(
   nsNavHistoryURIResultNode* aNode, void* aClosure)
{
  const nsCString* newFavicon = NS_STATIC_CAST(nsCString*, aClosure);
  aNode->mFaviconURI = *newFavicon;
}
NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnPageChanged(nsIURI *aURI, PRUint32 aWhat,
                         const nsAString &aValue)
{
  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);

  nsCAutoString spec;
  nsresult rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  switch (aWhat) {
    case nsINavHistoryObserver::ATTRIBUTE_FAVICON: {
      nsCString newFavicon = NS_ConvertUTF16toUTF8(aValue);
      PRBool onlyOneEntry = (mOptions->ResultType() ==
                             nsINavHistoryQueryOptions::RESULTS_AS_URI);
      UpdateURIs(onlyOneEntry, PR_FALSE, spec, setFaviconCallback, &newFavicon);
      break;
    }
    default:
      NS_WARNING("Unknown page changed notification");
  }
  return NS_OK;
}


// nsNavHistoryQueryResultNode bookmark observers
//
//    These are the bookmark observer functions for query nodes. They listen
//    for bookmark events and refresh the results if we have any dependence on
//    the bookmark system.

NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnItemAdded(nsIURI* aBookmark, PRInt64 aFolder,
                                          PRInt32 aIndex)
{
  if (mLiveUpdate == QUERYUPDATE_COMPLEX_WITH_BOOKMARKS)
    return Refresh();
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnItemRemoved(nsIURI* aBookmark, PRInt64 aFolder,
                                            PRInt32 aIndex)
{
  if (mLiveUpdate == QUERYUPDATE_COMPLEX_WITH_BOOKMARKS)
    return Refresh();
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnItemMoved(nsIURI* aBookmark, PRInt64 aFolder,
                                          PRInt32 aOldIndex, PRInt32 aNewIndex)
{
  if (mLiveUpdate == QUERYUPDATE_COMPLEX_WITH_BOOKMARKS)
    return Refresh();
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnItemChanged(nsIURI* aBookmark,
                                            const nsACString& aProperty,
                                            const nsAString& aValue)
{
  if (mLiveUpdate == QUERYUPDATE_COMPLEX_WITH_BOOKMARKS)
    return Refresh();
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnItemVisited(nsIURI* aBookmark, PRInt64 aVisitId,
                                            PRTime aTime)
{
  if (mLiveUpdate == QUERYUPDATE_COMPLEX_WITH_BOOKMARKS)
    return Refresh();
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnItemReplaced(PRInt64 aFolder, nsIURI *aOldItem,
                                             nsIURI* aNewItem)
{
  if (mLiveUpdate == QUERYUPDATE_COMPLEX_WITH_BOOKMARKS)
    return Refresh();
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnFolderAdded(PRInt64 aFolder, PRInt64 aParent,
                                            PRInt32 aIndex)
{
  if (mLiveUpdate == QUERYUPDATE_COMPLEX_WITH_BOOKMARKS)
    return Refresh();
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnFolderRemoved(PRInt64 aFolder, PRInt64 aParent,
                                              PRInt32 aIndex)
{
  if (mLiveUpdate == QUERYUPDATE_COMPLEX_WITH_BOOKMARKS)
    return Refresh();
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnFolderMoved(PRInt64 aFolder, PRInt64 aOldParent,
                                            PRInt32 aOldIndex, PRInt64 aNewParent,
                                            PRInt32 aNewIndex)
{
  if (mLiveUpdate == QUERYUPDATE_COMPLEX_WITH_BOOKMARKS)
    return Refresh();
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnFolderChanged(PRInt64 aFolder,
                                              const nsACString& property)
{
  if (mLiveUpdate == QUERYUPDATE_COMPLEX_WITH_BOOKMARKS)
    return Refresh();
  return NS_OK;
}


// nsNavHistoryFolderResultNode ************************************************
//
//    HOW DYNAMIC FOLDER UPDATING WORKS
//
//    When you create a result, it will automatically keep itself in sync with
//    stuff that happens in the system. For folder nodes, this means changes
//    to bookmarks.
//
//    A folder will fill its children "when necessary." This means it is being
//    opened or whether we need to see if it is empty for twisty drawing. It
//    will then register its ID with the main result object that owns it. This
//    result object will listen for all bookmark notifications and pass those
//    notifications to folder nodes that have registered for that specific
//    folder ID.
//
//    When a bookmark folder is closed, it will not clear its children. Instead,
//    it will keep them and also stay registered as a listener. This means that
//    you can more quickly re-open the same folder without doing any work. This
//    happens a lot for menus, and bookmarks don't change very often.
//
//    When a message comes in and the folder is open, we will do the correct
//    operations to keep ourselves in sync with the bookmark service. If the
//    folder is closed, we just clear our list to mark it as invalid and
//    unregister as a listener. This means we do not have to keep maintaining
//    an up-to-date list for the entire bookmark menu structure in every place
//    it is used.
//
//    There is an additional wrinkle. If the result is attached to a treeview,
//    and we invalidate but the folder itself is visible (its parent is open),
//    we will immediately get asked if we are full. The folder will then query
//    its children. Thus, the whole benefit of incremental updating is lost.
//    Therefore, if we are in a treeview AND our parent is visible, we will
//    keep incremental updating.

NS_IMPL_ISUPPORTS_INHERITED2(nsNavHistoryFolderResultNode,
                             nsNavHistoryContainerResultNode,
                             nsINavHistoryQueryResultNode,
                             nsINavHistoryFolderResultNode)

nsNavHistoryFolderResultNode::nsNavHistoryFolderResultNode(
    const nsACString& aTitle, nsNavHistoryQueryOptions* aOptions,
    PRInt64 aFolderId, const nsACString& aRemoteContainerType) :
  nsNavHistoryContainerResultNode(aTitle, EmptyCString(),
                                  nsINavHistoryResultNode::RESULT_TYPE_FOLDER,
                                  PR_FALSE, aRemoteContainerType),
  mContentsValid(PR_FALSE),
  mOptions(aOptions),
  mFolderId(aFolderId)
{
  // Get the favicon, if any, for this folder. Errors aren't too important
  // here, so just give up if anything bad happens.
  //
  // PERFORMANCE: This is not very efficient, we may want to pass this as
  // a parameter and integrate it into the query that generates the
  // bookmark results.
  nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
  if (! bookmarks)
    return;

  nsCOMPtr<nsIURI> folderURI;
  nsresult rv = bookmarks->GetFolderURI(aFolderId, getter_AddRefs(folderURI));
  if (NS_FAILED(rv))
    return;

  nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
  if (! faviconService)
    return;
  nsCOMPtr<nsIURI> favicon;
  rv = faviconService->GetFaviconForPage(folderURI, getter_AddRefs(favicon));
  if (NS_FAILED(rv))
    return; // this will happen when there is no favicon (most common case)

  // we have a favicon, save it
  favicon->GetSpec(mFaviconURI);
}


// nsNavHistoryFolderResultNode::OnRemoving

void
nsNavHistoryFolderResultNode::OnRemoving()
{
  ClearChildren(PR_TRUE);
}


// nsNavHistoryFolderResultNode::OpenContainer
//
//    See nsNavHistoryQueryResultNode::OpenContainer

nsresult
nsNavHistoryFolderResultNode::OpenContainer()
{
  NS_ASSERTION(! mExpanded, "Container must be expanded to close it");
  nsresult rv;
  if (! mRemoteContainerType.IsEmpty()) {
    // remote container API may want to change the bookmarks for this folder.
    nsCOMPtr<nsIRemoteContainer> remote = do_GetService(mRemoteContainerType.get(), &rv);
    if (NS_SUCCEEDED(rv)) {
      remote->OnContainerOpening(NS_STATIC_CAST(nsINavHistoryFolderResultNode*, this),
                                 mOptions);
    } else {
      NS_WARNING("Unable to get remote container for ");
      NS_WARNING(mRemoteContainerType.get());
    }
  }

  if (! mContentsValid) {
    rv = FillChildren();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  mExpanded = PR_TRUE;
  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);
  return result->RefreshVisibleSection(this);
}


// nsNavHistoryFolderResultNode::GetHasChildren
//
//    See nsNavHistoryQueryResultNode::HasChildren.
//
//    The semantics here are a little different. Querying the contents of a
//    bookmark folder is relatively fast and it is common to have empty folders.
//    Therefore, we always want to return the correct result so that twisties
//    are drawn properly.

NS_IMETHODIMP
nsNavHistoryFolderResultNode::GetHasChildren(PRBool* aHasChildren)
{
  if (! mContentsValid) {
    nsresult rv = FillChildren();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  *aHasChildren = (mChildren.Count() > 0);
  return NS_OK;
}


// nsNavHistoryFolderResultNode::GetQueryURI

NS_IMETHODIMP
nsNavHistoryFolderResultNode::GetQueryURI(nsIURI** aQueryURI)
{
  PRUint32 queryCount;
  nsINavHistoryQuery** queries;
  nsresult rv = GetQueries(&queryCount, &queries);
  NS_ENSURE_SUCCESS(rv, rv);

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
  nsCAutoString queryString;
  rv = history->QueriesToQueryString(queries, queryCount, mOptions, queryString);
  nsMemory::Free(queries);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_NewURI(aQueryURI, queryString);
}


// nsNavHistoryFolderResultNode::GetQueries
//
//    This just returns the queries that give you this bookmarks folder

NS_IMETHODIMP
nsNavHistoryFolderResultNode::GetQueries(PRUint32* queryCount,
                                         nsINavHistoryQuery*** queries)
{
  // get the query object
  nsCOMPtr<nsINavHistoryQuery> query;
  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv = history->GetNewQuery(getter_AddRefs(query));
  NS_ENSURE_SUCCESS(rv, rv);

  // query just has the folder ID set and nothing else
  rv = query->SetFolders(&mFolderId, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  // make array of our 1 query
  *queries = NS_STATIC_CAST(nsINavHistoryQuery**,
                            nsMemory::Alloc(sizeof(nsINavHistoryQuery*)));
  if (! *queries)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF((*queries)[0] = query);
  *queryCount = 1;
  return NS_OK;
}


// nsNavHistoryFolderResultNode::GetQueryOptions
//
//    Options for the query that gives you this bookmarks folder. This is just
//    the options for the folder with the current folder ID set.

NS_IMETHODIMP
nsNavHistoryFolderResultNode::GetQueryOptions(
                                      nsINavHistoryQueryOptions** aQueryOptions)
{
  return mOptions->Clone(aQueryOptions);
}


// nsNavHistoryFolderResultNode::FillChildren
//
//    Call to fill the actual children of this folder.

nsresult
nsNavHistoryFolderResultNode::FillChildren()
{
  NS_ASSERTION(! mContentsValid,
               "Don't call FillChildren when contents are valid");
  NS_ASSERTION(mChildren.Count() == 0,
               "We are trying to fill children when there already are some");

  nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
  NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);

  // actually get the folder children from the bookmark service
  nsresult rv = bookmarks->QueryFolderChildren(mFolderId, mOptions, &mChildren);
  NS_ENSURE_SUCCESS(rv, rv);

  // PERFORMANCE: it may be better to also fill any child folders at this point
  // so that we can draw tree twisties without doing a separate query later. If
  // we don't end up drawing twisties a lot, it doesn't matter. If we do this,
  // we should wrap everything in a transaction here on the bookmark service's
  // connection.

  // it is important to call FillStats to fill in the parents on all
  // nodes and the result node pointers on the containers
  FillStats();

  // register with the result for updates
  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);
  result->AddBookmarkObserver(this, mFolderId);

  mContentsValid = PR_TRUE;
  return NS_OK;
}


// nsNavHistoryFolderResultNode::ClearChildren
//
//    See nsNavHistoryQueryResultNode::FillChildren

void
nsNavHistoryFolderResultNode::ClearChildren(PRBool unregister)
{
  for (PRInt32 i = 0; i < mChildren.Count(); i ++)
    mChildren[i]->OnRemoving();
  mChildren.Clear();
  mContentsValid = PR_FALSE;

  if (unregister) {
    nsNavHistoryResult* result = GetResult();
    if (result)
      result->RemoveBookmarkObserver(this, mFolderId);
  }
}


// nsNavHistoryFolderResultNode::GetSortType
//
//    Folders have their own sorting. This may get called when a query is
//    inside a folder and it wants to see if its position has changed in its
//    parent as a result of something changing. Therefore, we always want to
//    return no sorting so nothing changes. Bookmark folders use our own sorting.

PRUint32
nsNavHistoryFolderResultNode::GetSortType()
{
  return nsINavHistoryQueryOptions::SORT_BY_NONE;
}


// nsNavHistoryFolderResultNode::StartIncrementalUpdate
//
//    This implements the logic described above the constructor. This sees if
//    we should do an incremental update and returns true if so. If not, it
//    invalidates our children, unregisters us an observer, and returns false.

PRBool
nsNavHistoryFolderResultNode::StartIncrementalUpdate()
{
  // if any items are excluded, we can not do incremental updates since the
  // indices from the bookmark service will not be valid
  if (! mOptions->ExcludeItems() && ! mOptions->ExcludeQueries()) {

    // easy case: we are visible, always do incremental update
    if (AreChildrenVisible())
      return PR_TRUE;

    nsNavHistoryResult* result = GetResult();
    NS_ENSURE_TRUE(result, PR_FALSE);

    // when a tree is attached also do incremental updates if our parent is
    // visible so that twisties are drawn correctly.
    if (result->mTree && mParent && mParent->mVisibleIndex >= 0)
      return PR_TRUE;
  }

  // otherwise, we don't do incremental updates, invalidate and unregister
  ClearChildren(PR_TRUE);
  return PR_FALSE;
}


// nsNavHistoryFolderResultNode::OnBeginUpdateBatch (nsINavBookmarkObserver)

NS_IMETHODIMP
nsNavHistoryFolderResultNode::OnBeginUpdateBatch()
{
  return NS_OK;
}


// nsNavHistoryFolderResultNode::OnEndUpdateBatch (nsINavBookmarkObserver)

NS_IMETHODIMP
nsNavHistoryFolderResultNode::OnEndUpdateBatch()
{
  return NS_OK;
}


// nsNavHistoryFolderResultNode::OnItemAdded (nsINavBookmarkObserver)

NS_IMETHODIMP
nsNavHistoryFolderResultNode::OnItemAdded(nsIURI* aBookmark, PRInt64 aFolder,
                                          PRInt32 aIndex)
{
  NS_ASSERTION(aFolder == mFolderId, "Got wrong bookmark update");
  if (mOptions->ExcludeItems())
    return NS_OK; // don't update items when we aren't displaying them
  if (aIndex < 0 || aIndex > mChildren.Count())
    return NS_ERROR_INVALID_ARG;
  if (! StartIncrementalUpdate())
    return NS_OK;

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);

  nsNavHistoryResultNode* node;
  nsresult rv = history->UriToResultNode(aBookmark, mOptions, &node);
  NS_ENSURE_SUCCESS(rv, rv);

  return InsertChildAt(node, aIndex);
}


// nsNavHistoryFolderResultNode::OnItemRemoved (nsINavBookmarkObserver)

NS_IMETHODIMP
nsNavHistoryFolderResultNode::OnItemRemoved(nsIURI* aBookmark, PRInt64 aFolder,
                                            PRInt32 aIndex)
{
  NS_ASSERTION(aFolder == mFolderId, "Got wrong bookmark update");
  if (mOptions->ExcludeItems())
    return NS_OK; // don't update items when we aren't displaying them
  if (! StartIncrementalUpdate())
    return NS_OK;

  // don't trust the index from the bookmark service, find it ourselves. The
  // sorting could be different, or the bookmark services indices and ours might
  // be out of sync somehow.
  PRUint32 nodeIndex;
  nsNavHistoryURIResultNode* node = FindChildURI(aBookmark, &nodeIndex);
  if (! node)
    return NS_ERROR_FAILURE; // can't find it

  return RemoveChildAt(nodeIndex);
}


// nsNavHistoryFolderResultNode::OnItemMoved (nsINavBookmarkObserver)
//
//    Moved within our folder.

NS_IMETHODIMP
nsNavHistoryFolderResultNode::OnItemMoved(nsIURI* aBookmark, PRInt64 aFolder,
                                          PRInt32 aOldIndex, PRInt32 aNewIndex)
{
  NS_ASSERTION(aFolder == mFolderId, "Got wrong bookmark update");
  if (mOptions->ExcludeItems())
    return NS_OK; // don't update items when we aren't displaying them
  if (! StartIncrementalUpdate())
    return NS_OK;

  PRUint32 nodeIndex;
  nsNavHistoryURIResultNode* node = FindChildURI(aBookmark, &nodeIndex);
  if (! node)
    return NS_ERROR_FAILURE; // can't find bookmark

  // Just refresh the entire container: a lot of stuff (possibly even open
  // containers) may have moved, and we don't want to handle all the cases
  // here in this uncommon and non-time-critical update.
  if (mVisibleIndex >= 0) {
    nsNavHistoryResult* result = GetResult();
    NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);
    nsresult rv = result->RefreshVisibleSection(this);
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}


// nsNavHistoryFolderResultNode::OnItemChanged (nsINavBookmarkObserver)

NS_IMETHODIMP
nsNavHistoryFolderResultNode::OnItemChanged(nsIURI* aBookmark,
                                            const nsACString& aProperty,
                                            const nsAString& aValue)
{
  if (mOptions->ExcludeItems())
    return NS_OK; // don't update items when we aren't displaying them
  if (! StartIncrementalUpdate())
    return NS_OK;

  nsCAutoString spec;
  nsresult rv = aBookmark->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 nodeIndex;
  nsNavHistoryURIResultNode* node = FindChildURI(aBookmark, &nodeIndex);
  if (! node)
    return NS_ERROR_FAILURE;

  if (aProperty.EqualsLiteral("favicon")) {
    mFaviconURI = NS_ConvertUTF16toUTF8(aValue);
  } else if (aProperty.EqualsLiteral("title")) {
    mTitle = NS_ConvertUTF16toUTF8(aValue);
  } else if (aProperty.EqualsLiteral("cleartime")) {
    mTime = 0;
  } else {
    NS_NOTREACHED("Unknown bookmark property changing.");
  }

  if (mVisibleIndex >= 0) {
    nsNavHistoryResult* result = GetResult();
    NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);
    result->RowChanged(node->mVisibleIndex);
  }
  return NS_OK;
}


// nsNavHistoryFolderResultNode::OnItemVisited (nsINavBookmarkObserver)
//
//    Update visit count and last visit time and refresh.

NS_IMETHODIMP
nsNavHistoryFolderResultNode::OnItemVisited(nsIURI* aBookmark, PRInt64 aVisitId,
                                            PRTime aTime)
{
  if (mOptions->ExcludeItems())
    return NS_OK; // don't update items when we aren't displaying them
  if (! StartIncrementalUpdate())
    return NS_OK;

  PRUint32 nodeIndex;
  nsNavHistoryURIResultNode* node = FindChildURI(aBookmark, &nodeIndex);
  if (! node)
    return NS_ERROR_FAILURE;

  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);

  // update node
  node->mTime = aTime;
  node->mAccessCount ++;

  // update us
  PRInt32 oldAccessCount = mAccessCount;
  mAccessCount ++;
  if (aTime > mTime)
    mTime = aTime;
  ReverseUpdateStats(mAccessCount - oldAccessCount);

  if (mVisibleIndex >= 0)
    result->RowChanged(node->mVisibleIndex);
  return NS_OK;
}


// nsNavHistoryFolderResultNode::OnItemReplaced (nsINavBookmarkObserver)
//
//    This is uncommon so we won't waste a lot of effort on this. Just do
//    a remove and add.

NS_IMETHODIMP
nsNavHistoryFolderResultNode::OnItemReplaced(PRInt64 aFolder, nsIURI *aOldItem,
                                             nsIURI* aNewItem)
{
  NS_ASSERTION(aFolder == mFolderId, "Got wrong bookmark update");
  if (mOptions->ExcludeItems())
    return NS_OK; // don't update items when we aren't displaying them
  if (! StartIncrementalUpdate())
    return NS_OK;

  PRUint32 nodeIndex;
  nsNavHistoryURIResultNode* oldNode = FindChildURI(aOldItem, &nodeIndex);
  if (! oldNode) {
    NS_NOTREACHED("Bad bookmark message");
    return NS_ERROR_FAILURE;
  }

  nsresult rv = OnItemRemoved(aOldItem, aFolder, nodeIndex);
  NS_ENSURE_SUCCESS(rv, rv);
  return OnItemAdded(aNewItem, aFolder, nodeIndex);
}


// nsNavHistoryFolderResultNode::OnFolderAdded (nsINavBookmarkObserver)
//
//    Create the new folder and add it

NS_IMETHODIMP
nsNavHistoryFolderResultNode::OnFolderAdded(PRInt64 aFolder, PRInt64 aParent,
                                            PRInt32 aIndex)
{
  NS_ASSERTION(aFolder == mFolderId, "Got wrong bookmark update");
  if (! StartIncrementalUpdate())
    return NS_OK;

  nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
  NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);

  nsNavHistoryResultNode* node;
  nsresult rv = bookmarks->ResultNodeForFolder(aFolder, mOptions, &node);
  NS_ENSURE_SUCCESS(rv, rv);

  return InsertChildAt(node, aIndex);
}


// nsNavHistoryFolderResultNode::OnFolderRemoved (nsINavBookmarkObserver)

NS_IMETHODIMP
nsNavHistoryFolderResultNode::OnFolderRemoved(PRInt64 aFolder, PRInt64 aParent,
                                              PRInt32 aIndex)
{
  // We only care about notifications when a child changes. When the deleted
  // folder is us, our parent should also be registered and will remove us from
  // its list.
  if (mFolderId == aFolder)
    return NS_OK;

  NS_ASSERTION(aParent == mFolderId, "Got wrong bookmark update");
  if (! StartIncrementalUpdate())
    return NS_OK;

  // trust our own index over what the bookmark service provides
  PRUint32 index;
  nsNavHistoryFolderResultNode* node = FindChildFolder(aFolder, &index);
  if (! node) {
    NS_NOTREACHED("Removing folder we don't have");
    return NS_ERROR_FAILURE;
  }
  return RemoveChildAt(index);
}


// nsNavHistoryFolderResultNode::OnFolderMoved (nsINavBookmarkObserver)
//
//    This scenario should not be too common and is not performance critical.
//    Therefore, we just do two steps, a remove and an insert.

NS_IMETHODIMP
nsNavHistoryFolderResultNode::OnFolderMoved(PRInt64 aFolder, PRInt64 aOldParent,
                                            PRInt32 aOldIndex, PRInt64 aNewParent,
                                            PRInt32 aNewIndex)
{
  NS_ASSERTION(aOldParent == mFolderId || aNewParent == mFolderId,
               "Got a bookmark message that doesn't belong to us");
  if (! StartIncrementalUpdate())
    return NS_OK;

  if (aOldParent == mFolderId)
    OnFolderRemoved(aFolder, aOldParent, aOldIndex);
  if (aNewParent == mFolderId)
    OnFolderAdded(aFolder, aNewParent, aNewIndex);
  return NS_OK;
}


// nsNavHistoryFolderResultNode::OnFolderChanged (nsINavBookmarkObserver)

NS_IMETHODIMP
nsNavHistoryFolderResultNode::OnFolderChanged(PRInt64 aFolder,
                                              const nsACString& property)
{
  if (! StartIncrementalUpdate())
    return NS_OK;

  // find this folder in our list
  PRUint32 folderIndex;
  nsNavHistoryFolderResultNode* folderNode = FindChildFolder(aFolder, &folderIndex);
  if (! folderNode) {
    NS_NOTREACHED("Folder change message for a folder we don't have");
    return NS_ERROR_FAILURE;
  }

  if (property.EqualsLiteral("title")) {
    folderNode->mTitle = property;
  } else {
    NS_NOTREACHED("Unknown folder change event");
    return NS_ERROR_FAILURE;
  }

  // update folder if visible
  if (folderNode->mVisibleIndex >= 0) {
    nsNavHistoryResult* result = GetResult();
    NS_ENSURE_TRUE(result, NS_ERROR_FAILURE);
    result->RowChanged(folderNode->mVisibleIndex);
  }
  return NS_OK;
}


// nsNavHistoryResult **********************************************************

NS_IMPL_ISUPPORTS5(nsNavHistoryResult,
                   nsINavHistoryResult,
                   nsITreeView,
                   nsINavBookmarkObserver,
                   nsINavHistoryObserver,
                   nsISupportsWeakReference)

nsNavHistoryResult::nsNavHistoryResult(nsNavHistoryContainerResultNode* aRoot) :
  mRootNode(aRoot),
  mCollapseDuplicates(PR_TRUE),
  mShowSessions(PR_FALSE),
  mIsHistoryObserver(PR_FALSE),
  mIsBookmarksObserver(PR_FALSE)
{
  mRootNode->mResult = this;
}

PR_STATIC_CALLBACK(PLDHashOperator)
RemoveBookmarkObserversCallback(nsTrimInt64HashKey::KeyType aKey,
                                nsNavHistoryResult::FolderObserverList*& aData,
                                void* userArg)
{
  delete aData;
  return PL_DHASH_REMOVE;
}

nsNavHistoryResult::~nsNavHistoryResult()
{
  // delete all bookmark observer arrays which are allocated on the heap
  mBookmarkObservers.Enumerate(&RemoveBookmarkObserversCallback, nsnull);
}


// nsNavHistoryResult::Init
//
//    Call AddRef before this, since we may do things like register ourselves.

nsresult
nsNavHistoryResult::Init(nsINavHistoryQuery** aQueries,
                         PRUint32 aQueryCount,
                         nsNavHistoryQueryOptions *aOptions)
{
  nsresult rv;
  NS_ASSERTION(aOptions, "Must have valid options");
  NS_ASSERTION(aQueries && aQueryCount > 0, "Must have >1 query in result");

  // Fill saved source queries with copies of the original (the caller might
  // change their original objects, and we always want to reflect the source
  // parameters).
  for (PRUint32 i = 0; i < aQueryCount; i ++) {
    nsCOMPtr<nsINavHistoryQuery> queryClone;
    rv = aQueries[i]->Clone(getter_AddRefs(queryClone));
    NS_ENSURE_SUCCESS(rv, rv);
    if (!mQueries.AppendObject(queryClone))
      return NS_ERROR_OUT_OF_MEMORY;
  }
  rv = aOptions->Clone(getter_AddRefs(mOptions));
  mSortingMode = aOptions->SortingMode();
  NS_ENSURE_SUCCESS(rv, rv);

  mPropertyBags.Init();
  if (! mBookmarkObservers.Init(128))
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ASSERTION(mRootNode->mIndentLevel == -1,
               "Root node's indent level initialized wrong");
  mRootNode->FillStats();
  ComputeShowSessions();

  return NS_OK;
}


// nsNavHistoryResult::NewHistoryResult
//
//    Constructs a new history result object.

nsresult // static
nsNavHistoryResult::NewHistoryResult(nsINavHistoryQuery** aQueries,
                                     PRUint32 aQueryCount,
                                     nsNavHistoryQueryOptions* aOptions,
                                     nsNavHistoryContainerResultNode* aRoot,
                                     nsNavHistoryResult** result)
{
  *result = new nsNavHistoryResult(aRoot);
  if (! *result)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*result); // must happen before Init
  nsresult rv = (*result)->Init(aQueries, aQueryCount, aOptions);
  if (NS_FAILED(rv)) {
    NS_RELEASE(*result);
    *result = nsnull;
    return rv;
  }
  return NS_OK;
}


// nsNavHistoryResult::ComputeShowSessions
//
//    Computes the value of mShowSessions, which is used to see if we should
//    try to set styles for session groupings.  We only want to do session
//    grouping if we're in an appropriate view, which is view by visit and
//    sorted by date.

void
nsNavHistoryResult::ComputeShowSessions()
{
  mShowSessions = PR_FALSE;
  NS_ASSERTION(mOptions, "navHistoryResults must have valid options");
  if (mOptions->ResultType() != nsINavHistoryQueryOptions::RESULTS_AS_VISIT &&
      mOptions->ResultType() != nsINavHistoryQueryOptions::RESULTS_AS_FULL_VISIT)
    return; // not visits
  if (mSortingMode != nsINavHistoryQueryOptions::SORT_BY_DATE_ASCENDING &&
      mSortingMode != nsINavHistoryQueryOptions::SORT_BY_DATE_DESCENDING)
    return; // not date sorting

  PRUint32 groupCount;
  mOptions->GroupingMode(&groupCount);
  if (groupCount > 0)
    return;

  mShowSessions = PR_TRUE;
}


// nsNavHistoryResult::PropertyBagFor
//
//    Given a pointer to a result node, this will give you the property bag
//    corresponding to it. Each node exposes a property bag to be used to
//    store temporary data. It is designed primarily for those implementing
//    container sources for storing data they need.
//
//    Since we expect very few result nodes will ever have their property bags
//    used, and since we can have a LOT of result nodes, we store the property
//    bags separately in a hash table in the parent result.
//
//    This function is called by a node when somebody wants the property bag.
//    It will create a property bag if necessary and store it for later
//    retrieval.

nsresult
nsNavHistoryResult::PropertyBagFor(nsISupports* aObject,
                                   nsIWritablePropertyBag** aBag)
{
  *aBag = nsnull;
  if (mPropertyBags.Get(aObject, aBag) && *aBag)
    return NS_OK;

  nsresult rv = NS_NewHashPropertyBag(aBag);
  NS_ENSURE_SUCCESS(rv, rv);
  if (! mPropertyBags.Put(aObject, *aBag)) {
    NS_RELEASE(*aBag);
    *aBag = nsnull;
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}


// nsNavHistoryResult::BuildVisibleList
//
//    Call to completely rebuild the list of visible items. Note if there is no
//    tree or root this will just clear out the list, so you can also call this
//    when a tree is detached to clear the list.
//
//    This does NOT update the screen.

nsresult
nsNavHistoryResult::BuildVisibleList()
{
  // any current visible elements need to be marked as invisible
  for (PRUint32 i = 0; i < mVisibleElements.Length(); i ++)
    mVisibleElements[i]->mVisibleIndex = -1;
  mVisibleElements.Clear();

  if (! mRootNode || ! mTree)
    return NS_OK;

  return BuildVisibleSection(mRootNode, &mVisibleElements, 0);
}


// nsNavHistoryResult::BuildVisibleSection
//
//    This takes a container and recursively appends visible elements to the
//    given array. This is used to build the visible element list (with
//    mVisibleElements passed as the array), or portions thereof (with a
//    separate array that is merged with the main list later.
//
//    aVisibleStartIndex is the visible index of the beginning of the 'aVisible'
//    array. When aVisible is mVisibleElements, this is 0. This is non-zero
//    when we are building up a sub-region for insertion. Then, this is the
//    index where the new array will be inserted into mVisibleElements. It is
//    used to compute each node's mVisibleIndex.

nsresult
nsNavHistoryResult::BuildVisibleSection(
    nsNavHistoryContainerResultNode* aContainer,
    VisibleList* aVisible,
    PRUint32 aVisibleStartIndex)
{
  if (! aContainer->mExpanded)
    return NS_OK; // nothing to do
  for (PRInt32 i = 0; i < aContainer->mChildren.Count(); i ++) {
    nsNavHistoryResultNode* cur = aContainer->mChildren[i];

    // collapse all duplicates starting from here
    if (mCollapseDuplicates) {
      PRUint32 showThis;
      while (i < aContainer->mChildren.Count() - 1 &&
             CanCollapseDuplicates(cur, aContainer->mChildren[i+1], &showThis)) {
        if (showThis) {
          // collapse the first and use the second
          cur->mVisibleIndex = -1;
          cur = aContainer->mChildren[i + 1];
        } else {
          // collapse the second and use the first
          aContainer->mChildren[i + 1]->mVisibleIndex = -1;
        }
        i ++;
      }
    }

    // add item
    cur->mVisibleIndex = aVisibleStartIndex + aVisible->Length();
    if (! aVisible->AppendElement(cur))
      return NS_ERROR_OUT_OF_MEMORY;

    // recursively do containers
    if (cur->IsContainer()) {
      nsNavHistoryContainerResultNode* curContainer = cur->GetAsContainer();
      if (curContainer->mExpanded && curContainer->mChildren.Count() > 0) {
        nsresult rv = BuildVisibleSection(curContainer, aVisible,
                                          aVisibleStartIndex);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }
  return NS_OK;
}


// nsNavHistoryResult::CountVisibleRowsForItem
//
//    This counts how many rows an item takes in the tree, that is, the item
//    itself plus any nodes following it with an increased indent. This allows
//    you to figure out how many rows an item (=1) or a container with all of
//    its children takes.

PRUint32
nsNavHistoryResult::CountVisibleRowsForItem(nsNavHistoryResultNode* aNode)
{
  NS_ASSERTION(aNode->mVisibleIndex >= 0, "Item is not visible, no rows to count");
  PRInt32 outerLevel = aNode->mIndentLevel;
  for (PRUint32 i = aNode->mVisibleIndex + 1; i < mVisibleElements.Length(); i ++) {
    if (mVisibleElements[i]->mIndentLevel <= outerLevel)
      return i - aNode->mVisibleIndex;
  }
  // this node plus its children occupy the bottom of the list
  return mVisibleElements.Length() - aNode->mVisibleIndex;
}


// nsNavHistoryResult::RefreshVisibleSection
//
//    This is called by containers when they change and we need to update
//    everything about the container. We build a new visible section with the
//    container as a separate object so we first know how the list changes. This
//    way we only have to do one realloc/memcpy to update the list.
//
//    We also try to be smart here about redrawing the screen.

nsresult
nsNavHistoryResult::RefreshVisibleSection(
                                    nsNavHistoryContainerResultNode* aContainer)
{
  // don't update if there is no tree
  if (! mTree)
    return NS_OK;

  // The root node is invisible, so we can't check its index and indent level.
  // Therefore, special case it. This is easy because we just rebuild everything
  if (aContainer == mRootNode)
    return BuildVisibleList();

  // When the container is not the root, it better be visible if we are updating
  NS_ASSERTION(aContainer->mVisibleIndex >= 0 &&
               aContainer->mVisibleIndex < PRInt32(mVisibleElements.Length()),
               "Trying to expand a node that is not visible");
  NS_ASSERTION(mVisibleElements[aContainer->mVisibleIndex] == aContainer,
               "Visible index is out of sync!");

  PRUint32 startReplacement = aContainer->mVisibleIndex + 1;
  PRUint32 replaceCount = CountVisibleRowsForItem(aContainer) - 1;

  // Mark the removees as invisible
  PRUint32 i;
  for (i = startReplacement; i < replaceCount; i ++)
    mVisibleElements[startReplacement + i]->mVisibleIndex = -1;

  // Building the new list will set the new elements' visible indices.
  VisibleList newElements;
  nsresult rv = BuildVisibleSection(aContainer, &newElements, startReplacement);
  NS_ENSURE_SUCCESS(rv, rv);

  // actually update the visible list
  if (! mVisibleElements.ReplaceElementsAt(aContainer->mVisibleIndex + 1,
      replaceCount, newElements.Elements(), newElements.Length()))
    return NS_ERROR_OUT_OF_MEMORY;

  if (replaceCount == newElements.Length()) {
    // length is the same, just repaint the changed area
    if (replaceCount > 0)
      mTree->InvalidateRange(startReplacement, startReplacement + replaceCount - 1);
  } else {
    // If the new area is a different size, we'll have to renumber the elements
    // following the area.
    for (i = startReplacement + newElements.Length();
         i < mVisibleElements.Length(); i ++)
      mVisibleElements[i]->mVisibleIndex = i;

    // repaint the region that stayed the same, also including the container
    // element itself since its twisty could have changed.
    PRUint32 minLength;
    if (replaceCount > newElements.Length())
      minLength = newElements.Length();
    else
      minLength = replaceCount;
    mTree->InvalidateRange(startReplacement - 1,
                           startReplacement + minLength - 1);

    // now update the number of elements
    mTree->RowCountChanged(startReplacement + minLength,
                           newElements.Length() - replaceCount);
  }
  return NS_OK;
}


// nsNavHistoryResult::AddEverythingObserver

void
nsNavHistoryResult::AddEverythingObserver(nsNavHistoryQueryResultNode* aNode)
{
  if (! mIsHistoryObserver) {
      nsNavHistory* history = nsNavHistory::GetHistoryService();
      NS_ASSERTION(history, "Can't create history service");
      history->AddObserver(this, PR_TRUE);
      mIsHistoryObserver = PR_TRUE;
  }
  mEverythingObservers.AppendElement(aNode);
}


// nsNavHistoryResult::AddBookmarkObserver
//
//    Here, we also attach as a bookmark service observer if necessary

void
nsNavHistoryResult::AddBookmarkObserver(nsNavHistoryFolderResultNode* aNode,
                                        PRInt64 aFolder)
{
  if (! mIsBookmarksObserver) {
    nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
    if (! bookmarks) {
      NS_NOTREACHED("Can't create bookmark service");
      return;
    }
    bookmarks->AddObserver(this, PR_TRUE);
    mIsBookmarksObserver = PR_TRUE;
  }
  FolderObserverList* list = BookmarkObserversForId(aFolder, PR_TRUE);
  list->AppendElement(aNode);
}


// nsNavHistoryResult::RemoveEverythingObserver

void
nsNavHistoryResult::RemoveEverythingObserver(nsNavHistoryQueryResultNode* aNode)
{
  mEverythingObservers.RemoveElement(aNode);
}


// nsNavHistoryResult::RemoveBookmarkObserver

void
nsNavHistoryResult::RemoveBookmarkObserver(nsNavHistoryFolderResultNode* aNode,
                                           PRInt64 aFolder)
{
  FolderObserverList* list = BookmarkObserversForId(aFolder, PR_FALSE);
  if (! list)
    return; // we don't even have an entry for that folder
  list->RemoveElement(aNode);
}


// nsNavHistoryResult::BookmarkObserversForId

nsNavHistoryResult::FolderObserverList*
nsNavHistoryResult::BookmarkObserversForId(PRInt64 aFolderId, PRBool aCreate)
{
  FolderObserverList* list;
  if (mBookmarkObservers.Get(aFolderId, &list))
    return list;
  if (! aCreate)
    return nsnull;

  // need to create a new list
  list = new FolderObserverList;
  mBookmarkObservers.Put(aFolderId, list);
  return list;
}


// nsNavHistoryResultNode::CanCollapseDuplicates
//
//    This returns true if the two results can be collapsed as duplicates.
//    *aShowThisOne will be either 0 or 1, indicating which of the duplicates
//    should be shown.

PRBool
nsNavHistoryResult::CanCollapseDuplicates(nsNavHistoryResultNode* aTop,
                                          nsNavHistoryResultNode* aNext,
                                          PRUint32* aShowThisOne)
{
  if (! mCollapseDuplicates)
    return PR_FALSE;
  if (! aTop->IsVisit() || ! aNext->IsVisit())
    return PR_FALSE; // only collapse two visits

  nsNavHistoryVisitResultNode* topVisit = aTop->GetAsVisit();
  nsNavHistoryVisitResultNode* nextVisit = aNext->GetAsVisit();

  if (! topVisit->mURI.Equals(nextVisit->mURI))
    return PR_FALSE; // don't collapse nonmatching URIs

  // now we know we want to collapse, show the one with the more recent time
  *aShowThisOne = topVisit->mTime < nextVisit->mTime;
  return PR_TRUE;
}


// nsNavHistoryResult::GetColumnType

nsNavHistoryResult::ColumnType
nsNavHistoryResult::GetColumnType(nsITreeColumn* col)
{
  const PRUnichar* idConst;
  col->GetIdConst(&idConst);
  switch(idConst[0]) {
    case PRUnichar('t'):
      return Column_Title;
    case PRUnichar('u'):
      return Column_URI;
    case PRUnichar('d'):
      return Column_Date;
    case PRUnichar('v'):
      return Column_VisitCount;
    default:
      return Column_Unknown;
  }
}


// nsNavHistoryResult::SortTypeToColumnType
//
//    Places whether it's ascending or descending into the given boolean buffer

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
    case nsINavHistoryQueryOptions::SORT_BY_URI_ASCENDING:
      *aDescending = PR_FALSE;
      return Column_URI;
    case nsINavHistoryQueryOptions::SORT_BY_URI_DESCENDING:
      *aDescending = PR_TRUE;
      return Column_URI;
    case nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_ASCENDING:
      *aDescending = PR_FALSE;
      return Column_VisitCount;
    case nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_DESCENDING:
      *aDescending = PR_TRUE;
      return Column_VisitCount;
    default:
      *aDescending = PR_FALSE;
      return Column_Unknown;
  }
}


// nsNavHistoryResult::SetTreeSortingIndicator
//
//    This is called to assign the correct arrow to the column header. It is
//    called both when you resort, and when a tree is first attached (to
//    initialize its headers).

void
nsNavHistoryResult::SetTreeSortingIndicator()
{
  NS_ASSERTION(mTree, "should only be called if we have a tree");

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
    element->SetAttribute(sortDirectionKey, EmptyString());
  }

  // set new sorting indicator by looking through all columns for ours
  NS_ASSERTION(mOptions, "Options should always be present for a root query");
  if (mSortingMode == nsINavHistoryQueryOptions::SORT_BY_NONE)
    return;
  PRBool desiredIsDescending;
  ColumnType desiredColumn = SortTypeToColumnType(mSortingMode,
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


// nsNavHistoryResult::InvalidateTree
//
//    Call to invalidate all rows on the tree

void
nsNavHistoryResult::InvalidateTree()
{
  if (! mTree)
    return;
  mTree->Invalidate();
}


// nsNavHistoryResult::FormatFriendlyTime

nsresult
nsNavHistoryResult::FormatFriendlyTime(PRTime aTime, nsAString& aResult)
{
  // use navHistory's GetNow function for performance
  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
  PRTime now = history->GetNow();

  const PRInt64 ago = now - aTime;

  /*
   * This code generates strings like "X minutes ago" when the time is very
   * recent. This looks much nicer, but we will have to redraw the list
   * periodically. I also found it a little strange watching the numbers in
   * the list change as I browse. For now, I'll leave this commented out,
   * perhaps we can revisit whether it is a good idea to do this and put in
   * the proper auto-refresh code so the numbers stay correct.
   *
   * To enable, you'll need to put these in places.properties:
   *   0MinutesAgo=<1 minute ago
   *   1MinutesAgo=1 minute ago
   *   XMinutesAgo=%s minutes ago

  static const PRInt64 minuteThreshold = (PRInt64)1000000 * 60 * 60;
  if (ago > -10000000 && ago < minuteThreshold) {
    // show "X minutes ago"
    PRInt64 minutesAgo = NSToIntFloor(
              NS_STATIC_CAST(float, (ago / 1000000)) / 60.0);

    nsXPIDLString resultString;
    if (minutesAgo == 0) {
      nsresult rv = mBundle->GetStringFromName(
          NS_LITERAL_STRING("0MinutesAgo").get(), getter_Copies(resultString));
      NS_ENSURE_SUCCESS(rv, rv);
    } else if (minutesAgo == 1) {
      nsresult rv = mBundle->GetStringFromName(
          NS_LITERAL_STRING("1MinutesAgo").get(), getter_Copies(resultString));
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      nsAutoString minutesAgoString;
      minutesAgoString.AppendInt(minutesAgo);
      const PRUnichar* stringPtr = minutesAgoString.get();
      nsresult rv = mBundle->FormatStringFromName(
          NS_LITERAL_STRING("XMinutesAgo").get(),
          &stringPtr, 1, getter_Copies(resultString));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    aResult = resultString;
    return NS_OK;
  } */

  // Check if it is today and only display the time.  Only bother checking for
  // today if it's within the last 24 hours, since computing midnight is not
  // really cheap. Sometimes we may get dates in the future, so always show
  // those. Note the 10s slop in case the cached GetNow value in NavHistory is
  // out-of-date.
  nsDateFormatSelector dateFormat = nsIScriptableDateFormat::dateFormatShort;
  if (ago > -10000000 && ago < (PRInt64)1000000 * 24 * 60 * 60) {
    PRExplodedTime explodedTime;
    PR_ExplodeTime(aTime, PR_LocalTimeParameters, &explodedTime);
    explodedTime.tm_min =
      explodedTime.tm_hour =
      explodedTime.tm_sec =
      explodedTime.tm_usec = 0;
    PRTime midnight = PR_ImplodeTime(&explodedTime);
    if (aTime > midnight)
      dateFormat = nsIScriptableDateFormat::dateFormatNone;
  }
  nsAutoString resultString;
  history->GetDateFormatter()->FormatPRTime(history->GetLocale(), dateFormat,
                               nsIScriptableDateFormat::timeFormatNoSeconds,
                               aTime, resultString);
  aResult = resultString;
  return NS_OK;
}


// nsNavHistoryResult::SortAll (nsINavHistoryResult)

NS_IMETHODIMP
nsNavHistoryResult::SortAll(PRUint32 aSortingMode)
{
  if (aSortingMode > nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_DESCENDING)
    return NS_ERROR_INVALID_ARG;
  if (! mRootNode)
    return NS_ERROR_FAILURE;

  PRUint32 oldVisibleCount = mVisibleElements.Length();

  // keep everything in sync
  NS_ASSERTION(mOptions, "Options should always be present for a root query");
  mSortingMode = aSortingMode;
  ComputeShowSessions();

  // actually do sorting
  nsNavHistoryContainerResultNode::SortComparator comparator =
      nsNavHistoryContainerResultNode::GetSortingComparator(aSortingMode);
  if (comparator) {
    nsNavHistory* history = nsNavHistory::GetHistoryService();
    NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
    mRootNode->RecursiveSort(history->GetCollation(), comparator);
  }

  if (mTree) {
    BuildVisibleList();
    SetTreeSortingIndicator();

    // sorting may have caused the row count to change from duplicate collapsing
    if (mVisibleElements.Length() != oldVisibleCount)
      mTree->RowCountChanged(0, mVisibleElements.Length() - oldVisibleCount);
    mTree->InvalidateRange(0, mVisibleElements.Length());
  }
  return NS_OK;
}


// nsNavHistoryResult::Get/SetCollapseDuplicates (nsINavHistoryResult)

NS_IMETHODIMP
nsNavHistoryResult::GetCollapseDuplicates(PRBool* aCollapseDuplicates)
{
  *aCollapseDuplicates = mCollapseDuplicates;
  return NS_OK;
}
NS_IMETHODIMP
nsNavHistoryResult::SetCollapseDuplicates(PRBool aCollapseDuplicates)
{
  if (aCollapseDuplicates == mCollapseDuplicates)
    return NS_OK;
  mCollapseDuplicates = aCollapseDuplicates;
  if (mTree) {
    // all we need to do here is rebuild the visible list with the new
    // collapse flag value
    BuildVisibleList();
    InvalidateTree();
  }
  return NS_OK;
}


// nsNavHistoryResult::NodeForTreeIndex (nsINavHistoryResult)

NS_IMETHODIMP
nsNavHistoryResult::NodeForTreeIndex(PRUint32 index,
                                     nsINavHistoryResultNode** aResult)
{
  if (index >= PRUint32(mVisibleElements.Length()))
    return NS_ERROR_INVALID_ARG;
  NS_ADDREF(*aResult = mVisibleElements[index]);
  return NS_OK;
}


// nsNavHistoryResult::TreeIndexForNode (nsINavHistoryResult)

NS_IMETHODIMP
nsNavHistoryResult::TreeIndexForNode(nsINavHistoryResultNode* aNode,
                                     PRUint32* aResult)
{
  nsresult rv;
  nsCOMPtr<nsNavHistoryResultNode> node = do_QueryInterface(aNode, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  if (node->mVisibleIndex < 0) {
    *aResult = nsINavHistoryResult::INDEX_INVISIBLE;
  } else {
    *aResult = node->mVisibleIndex;
    NS_ASSERTION(mVisibleElements[*aResult] == node,
                 "Node's visible index and array out of sync");
  }
  return NS_OK;
}


// nsNavHistoryResult::AddObserver (nsINavHistoryResult)

NS_IMETHODIMP
nsNavHistoryResult::AddObserver(nsINavHistoryResultViewObserver* aObserver,
                                PRBool aOwnsWeak)
{
  return mObservers.AppendWeakElement(aObserver, aOwnsWeak);
}


// nsNavHistoryResult::RemoveObserver (nsINavHistoryResult)

NS_IMETHODIMP
nsNavHistoryResult::RemoveObserver(nsINavHistoryResultViewObserver* aObserver)
{
  return mObservers.RemoveWeakElement(aObserver);
}


// nsNavHistoryResult::GetRoot (nsINavHistoryResult)
//
//    We have a pointer to a container, but it will either be a folder or
//    query node, both of which QI to QueryResultNode (even though folder
//    does not inherit from a concrete query).

NS_IMETHODIMP
nsNavHistoryResult::GetRoot(nsINavHistoryQueryResultNode** aRoot)
{
  if (! mRootNode) {
    NS_NOTREACHED("Root is null");
    *aRoot = nsnull;
    return NS_ERROR_FAILURE;
  }
  return mRootNode->QueryInterface(NS_GET_IID(nsINavHistoryQueryResultNode),
                                   NS_REINTERPRET_CAST(void**, aRoot));
}


// nsNavHistoryResult::GetRowCount (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::GetRowCount(PRInt32 *aRowCount)
{
  *aRowCount = mVisibleElements.Length();
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


// nsNavHistoryResult::GetRowProperties (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::GetRowProperties(PRInt32 row,
                                                   nsISupportsArray *properties)
{
  if (row < 0 || row >= PRInt32(mVisibleElements.Length()))
    return NS_ERROR_INVALID_ARG;

  if (! mShowSessions)
    return NS_OK; // don't need to bother to compute session boundaries
  nsNavHistoryResultNode *node = mVisibleElements[row];
  if (! node->IsVisit())
    return NS_OK; // not a visit, so there are no sessions

  nsNavHistoryVisitResultNode* visit = node->GetAsVisit();
  if (visit->mSessionId != 0) {
    if (row == 0) {
      properties->AppendElement(nsNavHistory::sSessionStartAtom);
    } else {
      nsNavHistoryResultNode* previousNode = mVisibleElements[row - 1];
      if (previousNode->IsVisit() &&
          visit->mSessionId != previousNode->GetAsVisit()->mSessionId) {
        properties->AppendElement(nsNavHistory::sSessionStartAtom);
      } else {
        properties->AppendElement(nsNavHistory::sSessionContinueAtom);
      }
    }
  }
  return NS_OK;
}


// nsNavHistoryResult::GetCellProperties (nsITreeView)

NS_IMETHODIMP
nsNavHistoryResult::GetCellProperties(PRInt32 row, nsITreeColumn *col,
                                      nsISupportsArray *properties)
{
  /*
  if (row < 0 || row >= mVisibleElements.Length())
    return NS_ERROR_INVALID_ARG;

  nsNavHistoryResultNode *node = mVisibleElements[row];
  PRInt64 folderId, bookmarksRootId, toolbarRootId;
  node->GetFolderId(&folderId);

  nsCOMPtr<nsINavBookmarksService> bms(do_GetService(
                                            NS_NAVBOOKMARKSSERVICE_CONTRACTID));
  bms->GetBookmarksRoot(&bookmarksRootId);
  bms->GetToolbarRoot(&toolbarRootId);
  if (bookmarksRootId == folderId)
    properties->AppendElement(nsNavHistory::sMenuRootAtom);
  else if (toolbarRootId == folderId)
    properties->AppendElement(nsNavHistory::sToolbarRootAtom);
  
  if (mShowSessions && node->mSessionID != 0) {
    if (row == 0 ||
        node->mSessionID != VisibleElementAt(row - 1)->mSessionID) {
      properties->AppendElement(nsNavHistory::sSessionStartAtom);
    } else {
      properties->AppendElement(nsNavHistory::sSessionContinueAtom);
    }
  }
  */
  return NS_OK;
}


// nsNavHistoryResult::GetColumnProperties (nsITreeView)

NS_IMETHODIMP
nsNavHistoryResult::GetColumnProperties(nsITreeColumn *col,
                                        nsISupportsArray *properties)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


// nsNavHistoryResult::IsContainer (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::IsContainer(PRInt32 row, PRBool *_retval)
{
  if (row < 0 || row >= PRInt32(mVisibleElements.Length()))
    return NS_ERROR_INVALID_ARG;
  *_retval = mVisibleElements[row]->IsContainer();
  if (*_retval) {
    // treat non-expandable queries as non-containers
    if (mVisibleElements[row]->IsQuery() &&
        ! mVisibleElements[row]->GetAsQuery()->CanExpand())
      *_retval = PR_FALSE;
  }
  return NS_OK;
}


// nsNavHistoryResult::IsContainerOpen (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::IsContainerOpen(PRInt32 row, PRBool *_retval)
{
  if (row < 0 || row >= PRInt32(mVisibleElements.Length()))
    return NS_ERROR_INVALID_ARG;
  if (! mVisibleElements[row]->IsContainer())
    return NS_ERROR_INVALID_ARG;

  nsINavHistoryContainerResultNode* container =
    mVisibleElements[row]->GetAsContainer();
  return container->GetContainerOpen(_retval);
}


// nsNavHistoryResult::IsContainerEmpty (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::IsContainerEmpty(PRInt32 row, PRBool *_retval)
{
  if (row < 0 || row >= PRInt32(mVisibleElements.Length()))
    return NS_ERROR_INVALID_ARG;
  if (! mVisibleElements[row]->IsContainer())
    return NS_ERROR_INVALID_ARG;

  nsINavHistoryContainerResultNode* container =
    mVisibleElements[row]->GetAsContainer();
  PRBool hasChildren = PR_FALSE;
  nsresult rv = container->GetHasChildren(&hasChildren);
  *_retval = ! hasChildren;
  return rv;
}


// nsNavHistoryResult::IsSeparator (nsITreeView)
//
//    We don't support separators
//    FIXME: do bookmark separators

NS_IMETHODIMP nsNavHistoryResult::IsSeparator(PRInt32 row, PRBool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}


// nsNavHistoryResult::IsSorted (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::IsSorted(PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


// nsNavHistoryResult::CanDrop (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::CanDrop(PRInt32 row, PRInt32 orientation,
                                          PRBool *_retval)
{
  PRUint32 count = mObservers.Length();
  for (PRUint32 i = 0; i < count; ++i) {
    const nsCOMPtr<nsINavHistoryResultViewObserver> &obs = mObservers[i];
    if (obs) {
      obs->CanDrop(row, orientation, _retval);
      if (*_retval) {
        break;
      }
    }
  }
  return NS_OK;
}


// nsNavHistoryResult::Drop (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::Drop(PRInt32 row, PRInt32 orientation)
{
  ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryResultViewObserver,
                      OnDrop(row, orientation))
  return NS_OK;
}


// nsNavHistoryResult::GetParentIndex (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::GetParentIndex(PRInt32 row, PRInt32* _retval)
{
  if (row < 0 || row >= PRInt32(mVisibleElements.Length()))
    return NS_ERROR_INVALID_ARG;
  nsNavHistoryResultNode* parent = mVisibleElements[row]->mParent;
  if (! parent || parent->mVisibleIndex < 0)
    *_retval = -1;
  else
    *_retval = parent->mVisibleIndex;
  return NS_OK;
}


// nsNavHistoryResult::HasNextSibling (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::HasNextSibling(PRInt32 row,
                                                 PRInt32 afterIndex,
                                                 PRBool* _retval)
{
  if (row < 0 || row >= PRInt32(mVisibleElements.Length()))
    return NS_ERROR_INVALID_ARG;
  if (row == PRInt32(mVisibleElements.Length()) - 1) {
    // this is the last thing in the list -> no next sibling
    *_retval = PR_FALSE;
    return NS_OK;
  }
  *_retval = (mVisibleElements[row]->mIndentLevel ==
              mVisibleElements[row + 1]->mIndentLevel);
  return NS_OK;
}


// nsNavHistoryResult::GetLevel (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::GetLevel(PRInt32 row, PRInt32* _retval)
{
  if (row < 0 || row >= PRInt32(mVisibleElements.Length()))
    return NS_ERROR_INVALID_ARG;
  *_retval = mVisibleElements[row]->mIndentLevel;
  return NS_OK;
}


// nsNavHistoryResult::GetImageSrc (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::GetImageSrc(PRInt32 row, nsITreeColumn *col,
                                              nsAString& _retval)
{
  if (row < 0 || row >= PRInt32(mVisibleElements.Length()))
    return NS_ERROR_INVALID_ARG;

  // only the first column has an image
  PRInt32 colIndex;
  col->GetIndex(&colIndex);
  if (colIndex != 0) {
    _retval.Truncate(0);
    return NS_OK;
  }

  nsNavHistoryResultNode* node = mVisibleElements[row];

  // Containers may or may not have favicons. If not, we will return nothing
  // as the image, and the style rule should pick up the default.
  if (node->IsContainer() && node->mFaviconURI.IsEmpty()) {
    _retval.Truncate(0);
    return NS_OK;
  }

  // For consistency, we always return a favicon for non-containers, even if
  // it is just the default one.
  nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
  NS_ENSURE_TRUE(faviconService, NS_ERROR_NO_INTERFACE);

  nsCAutoString spec;
  faviconService->GetFaviconSpecForIconString(
                                      mVisibleElements[row]->mFaviconURI, spec);
  CopyUTF8toUTF16(spec, _retval);
  return NS_OK;
}


// nsNavHistoryResult::GetProgressMode (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::GetProgressMode(PRInt32 row,
                                                  nsITreeColumn* col,
                                                  PRInt32* _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


// nsNavHistoryResult::GetCellValue (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::GetCellValue(PRInt32 row, nsITreeColumn *col,
                                               nsAString& _retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


// nsNavHistoryResult::GetCellText (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::GetCellText(PRInt32 row,
                                              nsITreeColumn *col,
                                              nsAString& _retval)
{
  if (row < 0 || row >= PRInt32(mVisibleElements.Length()))
    return NS_ERROR_INVALID_ARG;
  PRInt32 colIndex;
  col->GetIndex(&colIndex);

  nsNavHistoryResultNode* node = mVisibleElements[row];

  switch (GetColumnType(col)) {
    case Column_Title:
    {
      // title: normally, this is just the title, but we don't want empty
      // items in the tree view so return a special string if the title is
      // empty. Do it here so that callers can still get at the 0 length title
      // if they go through the "result" API.
      if (! node->mTitle.IsEmpty()) {
        _retval = NS_ConvertUTF8toUTF16(node->mTitle);
      } else {
        nsXPIDLString value;
        nsresult rv =
          nsNavHistory::GetHistoryService()->GetBundle()->GetStringFromName(
            NS_LITERAL_STRING("noTitle").get(), getter_Copies(value));
        NS_ENSURE_SUCCESS(rv, rv);
        _retval = value;
      }
      break;
    }
    case Column_URI:
    {
      if (node->IsURI())
        _retval = NS_ConvertUTF8toUTF16(node->GetAsURI()->mURI);
      else
        _retval.Truncate(0);
      break;
    }
    case Column_Date:
    {
      if (node->mTime == 0 || ! node->IsURI()) {
        // hosts and days shouldn't have a value for the date column. Actually,
        // you could argue this point, but looking at the results, seeing the
        // most recently visited date is not what I expect, and gives me no
        // information I know how to use. Only show this for URI-based items.
        _retval.Truncate(0);
      } else {
        return FormatFriendlyTime(node->mTime, _retval);
      }
      break;
    }
    case Column_VisitCount:
    {
      _retval = NS_ConvertUTF8toUTF16(nsPrintfCString("%d", node->mAccessCount));
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

NS_IMETHODIMP nsNavHistoryResult::SetTree(nsITreeBoxObject* tree)
{
  mTree = tree;
  if (mTree)
    SetTreeSortingIndicator();
  // if there is no tree, BuildVisibleList will clear everything for us
  return BuildVisibleList();
}


// nsNavHistoryResult::ToggleOpenState (nsITreeView)

NS_IMETHODIMP
nsNavHistoryResult::ToggleOpenState(PRInt32 row)
{
  if (row < 0 || row >= PRInt32(mVisibleElements.Length()))
    return NS_ERROR_INVALID_ARG;

  ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryResultViewObserver,
                      OnToggleOpenState(row))

  nsNavHistoryResultNode* node = mVisibleElements[row];
  if (! node->IsContainer())
    return NS_OK; // not a container, nothing to do
  nsNavHistoryContainerResultNode* container = node->GetAsContainer();

  nsresult rv;
  if (container->mExpanded)
    rv = container->CloseContainer();
  else
    rv = container->OpenContainer();
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


// nsNavHistoryResult::CycleHeader (nsITreeView)
//
//    If we already sorted by that column, the sorting is reversed, otherwise
//    we use the default sorting direction for that data type.

NS_IMETHODIMP
nsNavHistoryResult::CycleHeader(nsITreeColumn *col)
{
  ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryResultViewObserver,
                      OnCycleHeader(col))

  PRInt32 colIndex;
  col->GetIndex(&colIndex);

  NS_ASSERTION(mOptions, "Options should always be present for a root query");
  PRInt32 oldSort = mSortingMode;
  PRInt32 newSort;
  switch (GetColumnType(col)) {
    case Column_Title:
      if (oldSort == nsINavHistoryQueryOptions::SORT_BY_TITLE_ASCENDING)
        newSort = nsINavHistoryQueryOptions::SORT_BY_TITLE_DESCENDING;
      else
        newSort = nsINavHistoryQueryOptions::SORT_BY_TITLE_ASCENDING;
      break;
    case Column_URI:
      if (oldSort == nsINavHistoryQueryOptions::SORT_BY_URI_ASCENDING)
        newSort = nsINavHistoryQueryOptions::SORT_BY_URI_DESCENDING;
      else
        newSort = nsINavHistoryQueryOptions::SORT_BY_URI_ASCENDING;
      break;
    case Column_Date:
      if (oldSort == nsINavHistoryQueryOptions::SORT_BY_DATE_ASCENDING)
        newSort = nsINavHistoryQueryOptions::SORT_BY_DATE_DESCENDING;
      else
        newSort = nsINavHistoryQueryOptions::SORT_BY_DATE_ASCENDING;
      break;
    case Column_VisitCount:
      // visit count default is unusual because it is descending
      if (oldSort == nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_DESCENDING)
        newSort = nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_ASCENDING;
      else
        newSort = nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_DESCENDING;
      break;
    default:
      return NS_ERROR_INVALID_ARG;
  }
  return SortAll(newSort);
}


// nsNavHistoryResult::SelectionChanged  (nsITreeView)

NS_IMETHODIMP
nsNavHistoryResult::SelectionChanged()
{
  ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryResultViewObserver,
                      OnSelectionChanged())
  return NS_OK;
}


// nsNavHistoryResult::CycleCell (nsITreeView)

NS_IMETHODIMP
nsNavHistoryResult::CycleCell(PRInt32 row, nsITreeColumn *col)
{
  ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryResultViewObserver,
                      OnCycleCell(row, col))
  return NS_OK;
}


// nsNavHistoryResult::IsEditable (nsITreeView)

NS_IMETHODIMP
nsNavHistoryResult::IsEditable(PRInt32 row, nsITreeColumn *col, PRBool *_retval)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


//  nsNavHistoryResult::SetCellValue (nsITreeView)

NS_IMETHODIMP
nsNavHistoryResult::SetCellValue(PRInt32 row, nsITreeColumn *col, const nsAString & value)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


// nsNavHistoryResult::SetCellText (nsITreeView)

NS_IMETHODIMP
nsNavHistoryResult::SetCellText(PRInt32 row, nsITreeColumn *col, const nsAString & value)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


// nsNavHistoryResult::PerformAction (nsITreeView)

NS_IMETHODIMP
nsNavHistoryResult::PerformAction(const PRUnichar *action)
{
  ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryResultViewObserver,
                      OnPerformAction(action))
  return NS_OK;
}


// nsNavHistoryResult::PerformActionOnRow (nsITreeView)

NS_IMETHODIMP
nsNavHistoryResult::PerformActionOnRow(const PRUnichar *action, PRInt32 row)
{
  ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryResultViewObserver,
                      OnPerformActionOnRow(action, row))
  return NS_OK;
}


// nsNavHistoryResult::PerformActionOnCell (nsITreeView)

NS_IMETHODIMP
nsNavHistoryResult::PerformActionOnCell(const PRUnichar *action, PRInt32 row,
                                        nsITreeColumn *col)
{
  ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryResultViewObserver,
                      OnPerformActionOnCell(action, row, col))
  return NS_OK;
}


// nsNavHistoryResult::RowAdded
//
//    Called by nodes to perform dynamic updates of the visible list.  This
//    will add the item to the visible list and notify the tree that we
//    inserted something.
//
//    Should only be called when there is an attached tree.
//
//    This may not actually insert a row due to duplicate collapsing.

void
nsNavHistoryResult::RowAdded(PRInt32 aVisibleIndex,
                             nsNavHistoryResultNode* aNode)
{
  if (! mTree) {
    NS_NOTREACHED("RowAdded should only be called when there is a tree");
    return;
  }
  if (aVisibleIndex < 0 || aVisibleIndex > PRInt32(mVisibleElements.Length())) {
    NS_NOTREACHED("Trying to insert invalid row");
    return;
  }

  // Try collapsing with the previous node. Note that we do not have to try
  // to redraw the surrounding rows (which we normally would because session
  // boundaries may have changed) because when an item is merged, it will
  // always be in the same session.
  PRUint32 showThis;
  if (aVisibleIndex > 0 &&
      CanCollapseDuplicates(mVisibleElements[aVisibleIndex - 1],
                            aNode, &showThis)) {
    if (showThis == 0) {
      // new node is invisible, collapsed with previous one
      aNode->mVisibleIndex = -1;
      return;
    } else {
      // new node replaces the previous (RowReplaced won't update replacee)
      mVisibleElements[aVisibleIndex - 1]->mVisibleIndex = -1;
      RowReplaced(aVisibleIndex - 1, aNode);
      return;
    }
  }

  // try collapsing with the next node (which is currently at the same index
  // we are trying to insert at)
  if (aVisibleIndex < PRInt32(mVisibleElements.Length()) &&
      CanCollapseDuplicates(aNode, mVisibleElements[aVisibleIndex],
                            &showThis)) {
    if (showThis == 0) {
      // new node replaces the next node (RowReplaced won't update replacee)
      mVisibleElements[aVisibleIndex]->mVisibleIndex = -1;
      RowReplaced(aVisibleIndex, aNode);
      return;
    } else {
      // new node is invisible, replaced by next one
      aNode->mVisibleIndex = -1;
      return;
    }
  }

  // no collapsing
  aNode->mVisibleIndex = aVisibleIndex;
  mVisibleElements.InsertElementAt(aVisibleIndex, aNode);
  for (PRUint32 i = aVisibleIndex + 1; i < mVisibleElements.Length(); i ++)
    mVisibleElements[i]->mVisibleIndex = i;
  mTree->RowCountChanged(aVisibleIndex, 1);

  // Need to redraw the rows around this one because session boundaries may
  // have changed. For example, if we add a page to a session, the previous
  // page will need to be redrawn because it's session border will disappear.
  if (mShowSessions) {
    if (aVisibleIndex > 0)
      mTree->InvalidateRow(aVisibleIndex - 1);
    if (aVisibleIndex < PRInt32(mVisibleElements.Length()) - 1)
      mTree->InvalidateRow(aVisibleIndex + 1);
  }
}


// nsNavHistoryResult::RowsRemoved
//
//    Called by the container nodes to perform incremental updates. Only call
//    when there is an attached tree and the new item was visible. This will
//    update the visible list and notify the tree.
//
//    THIS FUNCTION DOES NOT HANDLE cases where a collapsed node is being
//    removed but the node it is collapsed with is not being removed (this
//    then just swap out the removee with it's collapsing partner). The only
//    time when we really remove things is when deleting URIs, which will
//    apply to all collapsees. This function is called sometimes when
//    resorting items. However, we won't do this when sorted by date because
//    dates will never change for visits, and date sorting is the only time
//    things are collapsed.
//
//    THIS FUNCTION SHOULD NOT TOUCH THE EXISTING NODE. This is called when
//    an item is being removed, and it probably was already removed from
//    its container which owns the reference. The visible element list does
//    NOT own references, so our pointer may be invalid.

void
nsNavHistoryResult::RowsRemoved(PRInt32 aVisibleIndex, PRUint32 aCount)
{
  if (! mTree) {
    NS_NOTREACHED("RowsRemoved should only be called when there is a tree");
    return;
  }
  if (aVisibleIndex < 0 || aVisibleIndex + aCount >=
      mVisibleElements.Length()) {
    NS_NOTREACHED("Trying to remove invalid row");
    return;
  }
  mVisibleElements.RemoveElementsAt(aVisibleIndex, aCount);
  for (PRUint32 i = aVisibleIndex; i < mVisibleElements.Length(); i ++)
    mVisibleElements[i]->mVisibleIndex = i;
  mTree->RowCountChanged(aVisibleIndex, -PRInt32(aCount));
}


// nsNavHistoryResult::RowChanged

void
nsNavHistoryResult::RowChanged(PRInt32 aVisibleIndex)
{
  if (! mTree) {
    NS_NOTREACHED("RowChanged should only be called when there is a tree");
    return;
  }
  if (aVisibleIndex < 0 || aVisibleIndex >= PRInt32(mVisibleElements.Length())) {
    NS_NOTREACHED("Trying to update invalid row");
    return;
  }
  mTree->InvalidateRow(aVisibleIndex);
}


// nsNavHistoryResult::RowReplaced
//
//    Same as RowChanged but this will replace the visible element index
//    with the new node pointer. We assume that the replacee will not be
//    duplicate collapsed and go away (this is only used by bookmarks code
//    which has no duplicate collapsing, and by InsertRow to handle the case of
//    duplicate collapsing when a node is being replaced).
//
//    THIS FUNCTION SHOULD NOT TOUCH THE EXISTING NODE. This is called when
//    an item is being replaced, and the old one was probably already removed
//    from its container which owns the reference. The visible element list
//    does NOT own references, so our pointer may be invalid.

void
nsNavHistoryResult::RowReplaced(PRInt32 aVisibleIndex,
                                nsNavHistoryResultNode* aNode)
{
  if (! mTree) {
    NS_NOTREACHED("RowReplaced should only be called when there is a tree");
    return;
  }
  if (aVisibleIndex < 0 || aVisibleIndex >= PRInt32(mVisibleElements.Length())) {
    NS_NOTREACHED("Trying to replace invalid row");
    return;
  }
  mVisibleElements[aVisibleIndex] = aNode;
  aNode->mVisibleIndex = aVisibleIndex;
  mTree->InvalidateRow(aVisibleIndex);
}

// nsINavBookmarkObserver implementation

#define ENUMERATE_BOOKMARK_OBSERVERS_FOR_FOLDER(_folderId, _functionCall) \
  FolderObserverList* _fol = BookmarkObserversForId(_folderId, PR_FALSE); \
  if (_fol) { \
    for (PRUint32 _fol_i = 0; _fol_i < _fol->Length(); _fol_i ++) \
      (*_fol)[_fol_i]->_functionCall; \
  }
#define ENUMERATE_HISTORY_OBSERVERS(_functionCall) \
  for (PRUint32 _obs_i = 0; _obs_i < mEverythingObservers.Length(); _obs_i ++) \
    mEverythingObservers[_obs_i]->_functionCall;

// nsNavHistoryResult::OnBeginUpdateBatch (nsINavBookmark/HistoryObserver)

NS_IMETHODIMP
nsNavHistoryResult::OnBeginUpdateBatch()
{
  return NS_OK;
}


// nsNavHistoryResult::OnEndUpdateBatch (nsINavBookmark/HistoryObserver)

NS_IMETHODIMP
nsNavHistoryResult::OnEndUpdateBatch()
{
  return NS_OK;
}


// nsNavHistoryResult::OnItemAdded (nsINavBookmarkObserver)

NS_IMETHODIMP
nsNavHistoryResult::OnItemAdded(nsIURI *aBookmark,
                                PRInt64 aFolder,
                                PRInt32 aIndex)
{
  ENUMERATE_BOOKMARK_OBSERVERS_FOR_FOLDER(aFolder,
      OnItemAdded(aBookmark, aFolder, aIndex));
  return NS_OK;
}


// nsNavHistoryResult::OnItemRemoved (nsINavBookmarkObserver)

NS_IMETHODIMP
nsNavHistoryResult::OnItemRemoved(nsIURI *aBookmark,
                                  PRInt64 aFolder, PRInt32 aIndex)
{
  ENUMERATE_BOOKMARK_OBSERVERS_FOR_FOLDER(aFolder,
      OnItemRemoved(aBookmark, aFolder, aIndex));
  return NS_OK;
}


// nsNavHistoryResult::OnItemMoved (nsINavBookmarkObserver)

NS_IMETHODIMP
nsNavHistoryResult::OnItemMoved(nsIURI *aBookmark, PRInt64 aFolder,
                                PRInt32 aOldIndex, PRInt32 aNewIndex)
{
  ENUMERATE_BOOKMARK_OBSERVERS_FOR_FOLDER(aFolder,
      OnItemMoved(aBookmark, aFolder, aOldIndex, aNewIndex));
  return NS_OK;

}


// nsNavHistoryResult::OnItemChanged (nsINavBookmarkObserver)

NS_IMETHODIMP
nsNavHistoryResult::OnItemChanged(nsIURI *aBookmark,
                                  const nsACString &aProperty,
                                  const nsAString &aValue)
{
  nsresult rv;
  nsNavBookmarks* bookmarkService = nsNavBookmarks::GetBookmarksService();
  NS_ENSURE_TRUE(bookmarkService, NS_ERROR_OUT_OF_MEMORY);

  PRUint32 folderCount;
  PRInt64* folders;
  rv = bookmarkService->GetBookmarkFolders(aBookmark, &folderCount, &folders);
  NS_ENSURE_SUCCESS(rv, rv);
  for (PRUint32 i = 0; i < folderCount; i ++) {
    ENUMERATE_BOOKMARK_OBSERVERS_FOR_FOLDER(folders[i],
        OnItemChanged(aBookmark, aProperty, aValue));
  }
  if (folders)
    nsMemory::Free(folders);
  return NS_OK;
}


// nsNavHistoryResult::OnItemVisited (nsINavBookmarkObserver)

NS_IMETHODIMP
nsNavHistoryResult::OnItemVisited(nsIURI* aBookmark, PRInt64 aVisitId,
                                  PRTime aVisitTime)
{
  nsresult rv;
  nsNavBookmarks* bookmarkService = nsNavBookmarks::GetBookmarksService();
  NS_ENSURE_TRUE(bookmarkService, NS_ERROR_OUT_OF_MEMORY);

  PRUint32 folderCount;
  PRInt64* folders;
  rv = bookmarkService->GetBookmarkFolders(aBookmark, &folderCount, &folders);
  NS_ENSURE_SUCCESS(rv, rv);
  for (PRUint32 i = 0; i < folderCount; i ++) {
    ENUMERATE_BOOKMARK_OBSERVERS_FOR_FOLDER(folders[i],
        OnItemVisited(aBookmark, aVisitId, aVisitTime));
  }
  if (folders)
    nsMemory::Free(folders);
  return NS_OK;
}


// nsNavHistoryResult::OnItemReplaced (nsINavBookmarkObserver)

NS_IMETHODIMP
nsNavHistoryResult::OnItemReplaced(PRInt64 aFolder,
                                   nsIURI *aItem, nsIURI *aNewItem)
{
  ENUMERATE_BOOKMARK_OBSERVERS_FOR_FOLDER(aFolder,
      OnItemReplaced(aFolder, aItem, aNewItem));
  return NS_OK;
}


// nsNavHistoryResult::OnFolderAdded (nsINavBookmarkObserver)

NS_IMETHODIMP
nsNavHistoryResult::OnFolderAdded(PRInt64 aFolder,
                                  PRInt64 aParent, PRInt32 aIndex)
{
  ENUMERATE_BOOKMARK_OBSERVERS_FOR_FOLDER(aParent,
      OnFolderAdded(aFolder, aParent, aIndex));
  return NS_OK;
}


// nsNavHistoryResult::OnFolderRemoved (nsINavBookmarkObserver)

NS_IMETHODIMP
nsNavHistoryResult::OnFolderRemoved(PRInt64 aFolder,
                                    PRInt64 aParent, PRInt32 aIndex)
{
  ENUMERATE_BOOKMARK_OBSERVERS_FOR_FOLDER(aParent,
      OnFolderRemoved(aFolder, aParent, aIndex));
  return NS_OK;
}


// nsNavHistoryResult::OnFolderMoved (nsINavBookmarkObserver)
//
//    Need to notify both the source and the destination folders (if they
//    are different).

NS_IMETHODIMP
nsNavHistoryResult::OnFolderMoved(PRInt64 aFolder,
                                  PRInt64 aOldParent, PRInt32 aOldIndex,
                                  PRInt64 aNewParent, PRInt32 aNewIndex)
{
  { // scope for loop index for VC6's broken for loop scoping
    ENUMERATE_BOOKMARK_OBSERVERS_FOR_FOLDER(aOldParent,
        OnFolderMoved(aFolder, aOldParent, aOldIndex, aNewParent, aNewIndex));
  }
  if (aNewParent != aOldParent) {
    ENUMERATE_BOOKMARK_OBSERVERS_FOR_FOLDER(aNewParent,
        OnFolderMoved(aFolder, aOldParent, aOldIndex, aNewParent, aNewIndex));
  }
  return NS_OK;
}


// nsNavHistoryResult::OnFolderChanged (nsINavBookmarkObserver)

NS_IMETHODIMP
nsNavHistoryResult::OnFolderChanged(PRInt64 aFolder,
                                   const nsACString &aProperty)
{
  ENUMERATE_BOOKMARK_OBSERVERS_FOR_FOLDER(aFolder,
      OnFolderChanged(aFolder, aProperty));
  return NS_OK;
}


// nsNavHistoryResult::OnVisit (nsINavHistoryObserver)

NS_IMETHODIMP
nsNavHistoryResult::OnVisit(nsIURI* aURI, PRInt64 aVisitId, PRTime aTime,
                            PRInt64 aSessionId, PRInt64 aReferringId,
                            PRUint32 aTransitionType)
{
  ENUMERATE_HISTORY_OBSERVERS(OnVisit(aURI, aVisitId, aTime, aSessionId,
                                      aReferringId, aTransitionType));
  return NS_OK;
}


// nsNavHistoryResult::OnTitleChanged (nsINavHistoryObserver)

NS_IMETHODIMP
nsNavHistoryResult::OnTitleChanged(nsIURI* aURI, const nsAString& aPageTitle,
                                  const nsAString& aUserTitle,
                                  PRBool aIsUserTitleChanged)
{
  ENUMERATE_HISTORY_OBSERVERS(OnTitleChanged(aURI, aPageTitle, aUserTitle,
                                             aIsUserTitleChanged));
  return NS_OK;
}


// nsNavHistoryResult::OnDeleteURI (nsINavHistoryObserver)

NS_IMETHODIMP
nsNavHistoryResult::OnDeleteURI(nsIURI *aURI)
{
  ENUMERATE_HISTORY_OBSERVERS(OnDeleteURI(aURI));
  return NS_OK;
}


// nsNavHistoryResult::OnClearHistory (nsINavHistoryObserver)

NS_IMETHODIMP
nsNavHistoryResult::OnClearHistory()
{
  ENUMERATE_HISTORY_OBSERVERS(OnClearHistory());
  return NS_OK;
}


// nsNavHistoryResult::OnPageChanged (nsINavHistoryObserver)

NS_IMETHODIMP
nsNavHistoryResult::OnPageChanged(nsIURI *aURI,
                                  PRUint32 aWhat, const nsAString &aValue)
{
  ENUMERATE_HISTORY_OBSERVERS(OnPageChanged(aURI, aWhat, aValue));
  return NS_OK;
}

