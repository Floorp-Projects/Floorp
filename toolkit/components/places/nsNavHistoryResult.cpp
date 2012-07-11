//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include "nsNavHistory.h"
#include "nsNavBookmarks.h"
#include "nsFaviconService.h"
#include "nsITaggingService.h"
#include "nsAnnotationService.h"
#include "Helpers.h"

#include "nsDebug.h"
#include "nsNetUtil.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsUnicharUtils.h"
#include "prtime.h"
#include "prprf.h"

#include "nsCycleCollectionParticipant.h"

#define TO_ICONTAINER(_node)                                                  \
    static_cast<nsINavHistoryContainerResultNode*>(_node)                      

#define TO_CONTAINER(_node)                                                   \
    static_cast<nsNavHistoryContainerResultNode*>(_node)

#define NOTIFY_RESULT_OBSERVERS_RET(_result, _method, _ret)                   \
  PR_BEGIN_MACRO                                                              \
  NS_ENSURE_TRUE(_result, _ret);                                              \
  if (!_result->mSuppressNotifications) {                                     \
    ENUMERATE_WEAKARRAY(_result->mObservers, nsINavHistoryResultObserver,     \
                        _method)                                              \
  }                                                                           \
  PR_END_MACRO

#define NOTIFY_RESULT_OBSERVERS(_result, _method)                             \
  NOTIFY_RESULT_OBSERVERS_RET(_result, _method, NS_ERROR_UNEXPECTED)

// What we want is: NS_INTERFACE_MAP_ENTRY(self) for static IID accessors,
// but some of our classes (like nsNavHistoryResult) have an ambiguous base
// class of nsISupports which prevents this from working (the default macro
// converts it to nsISupports, then addrefs it, then returns it). Therefore, we
// expand the macro here and change it so that it works. Yuck.
#define NS_INTERFACE_MAP_STATIC_AMBIGUOUS(_class) \
  if (aIID.Equals(NS_GET_IID(_class))) { \
    NS_ADDREF(this); \
    *aInstancePtr = this; \
    return NS_OK; \
  } else

// Number of changes to handle separately in a batch.  If more changes are
// requested the node will switch to full refresh mode.
#define MAX_BATCH_CHANGES_BEFORE_REFRESH 5

// Emulate string comparison (used for sorting) for PRTime and int.
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

using namespace mozilla::places;

NS_IMPL_CYCLE_COLLECTION_CLASS(nsNavHistoryResultNode)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsNavHistoryResultNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mParent)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END 

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsNavHistoryResultNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mParent, nsINavHistoryContainerResultNode);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsNavHistoryResultNode)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsINavHistoryResultNode)
  NS_INTERFACE_MAP_ENTRY(nsINavHistoryResultNode)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsNavHistoryResultNode)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsNavHistoryResultNode)

nsNavHistoryResultNode::nsNavHistoryResultNode(
    const nsACString& aURI, const nsACString& aTitle, PRUint32 aAccessCount,
    PRTime aTime, const nsACString& aIconURI) :
  mParent(nsnull),
  mURI(aURI),
  mTitle(aTitle),
  mAreTagsSorted(false),
  mAccessCount(aAccessCount),
  mTime(aTime),
  mFaviconURI(aIconURI),
  mBookmarkIndex(-1),
  mItemId(-1),
  mFolderId(-1),
  mDateAdded(0),
  mLastModified(0),
  mIndentLevel(-1),
  mFrecency(0)
{
  mTags.SetIsVoid(true);
}


NS_IMETHODIMP
nsNavHistoryResultNode::GetIcon(nsACString& aIcon)
{
  if (mFaviconURI.IsEmpty()) {
    aIcon.Truncate();
    return NS_OK;
  }

  nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
  NS_ENSURE_TRUE(faviconService, NS_ERROR_OUT_OF_MEMORY);
  faviconService->GetFaviconSpecForIconString(mFaviconURI, aIcon);
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryResultNode::GetParent(nsINavHistoryContainerResultNode** aParent)
{
  NS_IF_ADDREF(*aParent = mParent);
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryResultNode::GetParentResult(nsINavHistoryResult** aResult)
{
  *aResult = nsnull;
  if (IsContainer())
    NS_IF_ADDREF(*aResult = GetAsContainer()->mResult);
  else if (mParent)
    NS_IF_ADDREF(*aResult = mParent->mResult);

  NS_ENSURE_STATE(*aResult);
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryResultNode::GetTags(nsAString& aTags) {
  // Only URI-nodes may be associated with tags
  if (!IsURI()) {
    aTags.Truncate();
    return NS_OK;
  }

  // Initially, the tags string is set to a void string (see constructor).  We
  // then build it the first time this method called is called (and by that,
  // implicitly unset the void flag). Result observers may re-set the void flag
  // in order to force rebuilding of the tags string.
  if (!mTags.IsVoid()) {
    // If mTags is assigned by a history query it is unsorted for performance
    // reasons, it must be sorted by name on first read access.
    if (!mAreTagsSorted) {
      nsTArray<nsCString> tags;
      ParseString(NS_ConvertUTF16toUTF8(mTags), ',', tags);
      tags.Sort();
      mTags.SetIsVoid(true);
      for (nsTArray<nsCString>::index_type i = 0; i < tags.Length(); ++i) {
        AppendUTF8toUTF16(tags[i], mTags);
        if (i < tags.Length() - 1 )
          mTags.AppendLiteral(", ");
      }
      mAreTagsSorted = true;
    }
    aTags.Assign(mTags);
    return NS_OK;
  }

  // Fetch the tags
  nsRefPtr<Database> DB = Database::GetDatabase();
  NS_ENSURE_STATE(DB);
  nsCOMPtr<mozIStorageStatement> stmt = DB->GetStatement(
    "/* do not warn (bug 487594) */ "
    "SELECT GROUP_CONCAT(tag_title, ', ') "
    "FROM ( "
      "SELECT t.title AS tag_title "
      "FROM moz_bookmarks b "
      "JOIN moz_bookmarks t ON t.id = +b.parent "
      "WHERE b.fk = (SELECT id FROM moz_places WHERE url = :page_url) "
        "AND t.parent = :tags_folder "
      "ORDER BY t.title COLLATE NOCASE ASC "
    ") "
  );
  NS_ENSURE_STATE(stmt);
  mozStorageStatementScoper scoper(stmt);

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_STATE(history);
  nsresult rv = stmt->BindInt64ByName(NS_LITERAL_CSTRING("tags_folder"),
                                      history->GetTagsFolder());
  NS_ENSURE_SUCCESS(rv, rv);
  rv = URIBinder::Bind(stmt, NS_LITERAL_CSTRING("page_url"), mURI);
  NS_ENSURE_SUCCESS(rv, rv);

  bool hasTags = false;
  if (NS_SUCCEEDED(stmt->ExecuteStep(&hasTags)) && hasTags) {
    rv = stmt->GetString(0, mTags);
    NS_ENSURE_SUCCESS(rv, rv);
    aTags.Assign(mTags);
    mAreTagsSorted = true;
  }

  // If this node is a child of a history query, we need to make sure changes
  // to tags are properly live-updated.
  if (mParent && mParent->IsQuery() &&
      mParent->mOptions->QueryType() == nsINavHistoryQueryOptions::QUERY_TYPE_HISTORY) {
    nsNavHistoryQueryResultNode* query = mParent->GetAsQuery();
    nsNavHistoryResult* result = query->GetResult();
    NS_ENSURE_STATE(result);
    result->AddAllBookmarksObserver(query);
  }

  return NS_OK;
}


void
nsNavHistoryResultNode::OnRemoving()
{
  mParent = nsnull;
}


/**
 * This will find the result for this node.  We can ask the nearest container
 * for this value (either ourselves or our parents should be a container,
 * and all containers have result pointers).
 */
nsNavHistoryResult*
nsNavHistoryResultNode::GetResult()
{
  nsNavHistoryResultNode* node = this;
  do {
    if (node->IsContainer()) {
      nsNavHistoryContainerResultNode* container = TO_CONTAINER(node);
      NS_ASSERTION(container->mResult, "Containers must have valid results");
      return container->mResult;
    }
    node = node->mParent;
  } while (node);
  NS_NOTREACHED("No container node found in hierarchy!");
  return nsnull;
}


/**
 * Searches up the tree for the closest ancestor node that has an options
 * structure.  This will tell us the options that were used to generate this
 * node.
 *
 * Be careful, this function walks up the tree, so it can not be used when
 * result nodes are created because they have no parent.  Only call this
 * function after the tree has been built.
 */
nsNavHistoryQueryOptions*
nsNavHistoryResultNode::GetGeneratingOptions()
{
  if (!mParent) {
    // When we have no parent, it either means we haven't built the tree yet,
    // in which case calling this function is a bug, or this node is the root
    // of the tree.  When we are the root of the tree, our own options are the
    // generating options.
    if (IsContainer())
      return GetAsContainer()->mOptions;

    NS_NOTREACHED("Can't find a generating node for this container, perhaps FillStats has not been called on this tree yet?");
    return nsnull;
  }

  // Look up the tree.  We want the options that were used to create this node,
  // and since it has a parent, it's the options of an ancestor, not of the node
  // itself.  So start at the parent.
  nsNavHistoryContainerResultNode* cur = mParent;
  while (cur) {
    if (cur->IsContainer())
      return cur->GetAsContainer()->mOptions;
    cur = cur->mParent;
  }

  // We should always find a container node as an ancestor.
  NS_NOTREACHED("Can't find a generating node for this container, the tree seemes corrupted.");
  return nsnull;
}


NS_IMPL_ISUPPORTS_INHERITED1(nsNavHistoryVisitResultNode,
                             nsNavHistoryResultNode,
                             nsINavHistoryVisitResultNode)

nsNavHistoryVisitResultNode::nsNavHistoryVisitResultNode(
    const nsACString& aURI, const nsACString& aTitle, PRUint32 aAccessCount,
    PRTime aTime, const nsACString& aIconURI, PRInt64 aSession) :
  nsNavHistoryResultNode(aURI, aTitle, aAccessCount, aTime, aIconURI),
  mSessionId(aSession)
{
}


NS_IMPL_ISUPPORTS_INHERITED1(nsNavHistoryFullVisitResultNode,
                             nsNavHistoryVisitResultNode,
                             nsINavHistoryFullVisitResultNode)

nsNavHistoryFullVisitResultNode::nsNavHistoryFullVisitResultNode(
    const nsACString& aURI, const nsACString& aTitle, PRUint32 aAccessCount,
    PRTime aTime, const nsACString& aIconURI, PRInt64 aSession,
    PRInt64 aVisitId, PRInt64 aReferringVisitId, PRInt32 aTransitionType) :
  nsNavHistoryVisitResultNode(aURI, aTitle, aAccessCount, aTime, aIconURI,
                              aSession),
  mVisitId(aVisitId),
  mReferringVisitId(aReferringVisitId),
  mTransitionType(aTransitionType)
{
}


NS_IMPL_CYCLE_COLLECTION_CLASS(nsNavHistoryContainerResultNode)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(nsNavHistoryContainerResultNode, nsNavHistoryResultNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mResult)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMARRAY(mChildren)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END 

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsNavHistoryContainerResultNode, nsNavHistoryResultNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mResult, nsINavHistoryResult)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mChildren)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsNavHistoryContainerResultNode, nsNavHistoryResultNode)
NS_IMPL_RELEASE_INHERITED(nsNavHistoryContainerResultNode, nsNavHistoryResultNode)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(nsNavHistoryContainerResultNode)
  NS_INTERFACE_MAP_STATIC_AMBIGUOUS(nsNavHistoryContainerResultNode)
  NS_INTERFACE_MAP_ENTRY(nsINavHistoryContainerResultNode)
NS_INTERFACE_MAP_END_INHERITING(nsNavHistoryResultNode)

nsNavHistoryContainerResultNode::nsNavHistoryContainerResultNode(
    const nsACString& aURI, const nsACString& aTitle,
    const nsACString& aIconURI, PRUint32 aContainerType, bool aReadOnly,
    nsNavHistoryQueryOptions* aOptions) :
  nsNavHistoryResultNode(aURI, aTitle, 0, 0, aIconURI),
  mResult(nsnull),
  mContainerType(aContainerType),
  mExpanded(false),
  mChildrenReadOnly(aReadOnly),
  mOptions(aOptions),
  mAsyncCanceledState(NOT_CANCELED)
{
}

nsNavHistoryContainerResultNode::nsNavHistoryContainerResultNode(
    const nsACString& aURI, const nsACString& aTitle,
    PRTime aTime,
    const nsACString& aIconURI, PRUint32 aContainerType, bool aReadOnly,
    nsNavHistoryQueryOptions* aOptions) :
  nsNavHistoryResultNode(aURI, aTitle, 0, aTime, aIconURI),
  mResult(nsnull),
  mContainerType(aContainerType),
  mExpanded(false),
  mChildrenReadOnly(aReadOnly),
  mOptions(aOptions),
  mAsyncCanceledState(NOT_CANCELED)
{
}


nsNavHistoryContainerResultNode::~nsNavHistoryContainerResultNode()
{
  // Explicitly clean up array of children of this container.  We must ensure
  // all references are gone and all of their destructors are called.
  mChildren.Clear();
}


/**
 * Containers should notify their children that they are being removed when the
 * container is being removed.
 */
void
nsNavHistoryContainerResultNode::OnRemoving()
{
  nsNavHistoryResultNode::OnRemoving();
  for (PRInt32 i = 0; i < mChildren.Count(); ++i)
    mChildren[i]->OnRemoving();
  mChildren.Clear();
}


bool
nsNavHistoryContainerResultNode::AreChildrenVisible()
{
  nsNavHistoryResult* result = GetResult();
  if (!result) {
    NS_NOTREACHED("Invalid result");
    return false;
  }

  if (!mExpanded)
    return false;

  // Now check if any ancestor is closed.
  nsNavHistoryContainerResultNode* ancestor = mParent;
  while (ancestor) {
    if (!ancestor->mExpanded)
      return false;

    ancestor = ancestor->mParent;
  }

  return true;
}


NS_IMETHODIMP
nsNavHistoryContainerResultNode::GetContainerOpen(bool *aContainerOpen)
{
  *aContainerOpen = mExpanded;
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryContainerResultNode::SetContainerOpen(bool aContainerOpen)
{
  if (aContainerOpen) {
    if (!mExpanded) {
      nsNavHistoryQueryOptions* options = GetGeneratingOptions();
      if (options && options->AsyncEnabled())
        OpenContainerAsync();
      else
        OpenContainer();
    }
  }
  else {
    if (mExpanded)
      CloseContainer();
    else if (mAsyncPendingStmt)
      CancelAsyncOpen(false);
  }

  return NS_OK;
}


/**
 * Notifies the result's observers of a change in the container's state.  The
 * notification includes both the old and new states:  The old is aOldState, and
 * the new is the container's current state.
 *
 * @param aOldState
 *        The state being transitioned out of.
 */
nsresult
nsNavHistoryContainerResultNode::NotifyOnStateChange(PRUint16 aOldState)
{
  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_STATE(result);

  nsresult rv;
  PRUint16 currState;
  rv = GetState(&currState);
  NS_ENSURE_SUCCESS(rv, rv);

  // Notify via the new ContainerStateChanged observer method.
  NOTIFY_RESULT_OBSERVERS(result,
                          ContainerStateChanged(this, aOldState, currState));
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryContainerResultNode::GetState(PRUint16* _state)
{
  NS_ENSURE_ARG_POINTER(_state);

  *_state = mExpanded ? (PRUint16)STATE_OPENED
                      : mAsyncPendingStmt ? (PRUint16)STATE_LOADING
                                          : (PRUint16)STATE_CLOSED;

  return NS_OK;
}


/**
 * This handles the generic container case.  Other container types should
 * override this to do their own handling.
 */
nsresult
nsNavHistoryContainerResultNode::OpenContainer()
{
  NS_ASSERTION(!mExpanded, "Container must not be expanded to open it");
  mExpanded = true;

  nsresult rv = NotifyOnStateChange(STATE_CLOSED);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


/**
 * Unset aSuppressNotifications to notify observers on this change.  That is
 * the normal operation.  This is set to false for the recursive calls since the
 * root container that is being closed will handle recomputation of the visible
 * elements for its entire subtree.
 */
nsresult
nsNavHistoryContainerResultNode::CloseContainer(bool aSuppressNotifications)
{
  NS_ASSERTION((mExpanded && !mAsyncPendingStmt) ||
               (!mExpanded && mAsyncPendingStmt),
               "Container must be expanded or loading to close it");

  nsresult rv;
  PRUint16 oldState;
  rv = GetState(&oldState);
  NS_ENSURE_SUCCESS(rv, rv);

  if (mExpanded) {
    // Recursively close all child containers.
    for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
      if (mChildren[i]->IsContainer() &&
          mChildren[i]->GetAsContainer()->mExpanded)
        mChildren[i]->GetAsContainer()->CloseContainer(true);
    }

    mExpanded = false;
  }

  // Be sure to set this to null before notifying observers.  It signifies that
  // the container is no longer loading (if it was in the first place).
  mAsyncPendingStmt = nsnull;

  if (!aSuppressNotifications) {
    rv = NotifyOnStateChange(oldState);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // If this is the root container of a result, we can tell the result to stop
  // observing changes, otherwise the result will stay in memory and updates
  // itself till it is cycle collected.
  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_STATE(result);
  if (result->mRootNode == this) {
    result->StopObserving();
    // When reopening this node its result will be out of sync.
    // We must clear our children to ensure we will call FillChildren
    // again in such a case.
    if (this->IsQuery())
      this->GetAsQuery()->ClearChildren(true);
    else if (this->IsFolder())
      this->GetAsFolder()->ClearChildren(true);
  }

  return NS_OK;
}


/**
 * The async version of OpenContainer.
 */
nsresult
nsNavHistoryContainerResultNode::OpenContainerAsync()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


/**
 * Cancels the pending asynchronous Storage execution triggered by
 * FillChildrenAsync, if it exists.  This method doesn't do much, because after
 * cancelation Storage will call this node's HandleCompletion callback, where
 * the real work is done.
 *
 * @param aRestart
 *        If true, async execution will be restarted by HandleCompletion.
 */
void
nsNavHistoryContainerResultNode::CancelAsyncOpen(bool aRestart)
{
  NS_ASSERTION(mAsyncPendingStmt, "Async execution canceled but not pending");

  mAsyncCanceledState = aRestart ? CANCELED_RESTART_NEEDED : CANCELED;

  // Cancel will fail if the pending statement has already been canceled.
  // That's OK since this method may be called multiple times, and multiple
  // cancels don't harm anything.
  (void)mAsyncPendingStmt->Cancel();
}


/**
 * This builds up tree statistics from the bottom up.  Call with a container
 * and the indent level of that container.  To init the full tree, call with
 * the root container.  The default indent level is -1, which is appropriate
 * for the root level.
 *
 * CALL THIS AFTER FILLING ANY CONTAINER to update the parent and result node
 * pointers, even if you don't care about visit counts and last visit dates.
 */
void
nsNavHistoryContainerResultNode::FillStats()
{
  PRUint32 accessCount = 0;
  PRTime newTime = 0;

  for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
    nsNavHistoryResultNode* node = mChildren[i];
    node->mParent = this;
    node->mIndentLevel = mIndentLevel + 1;
    if (node->IsContainer()) {
      nsNavHistoryContainerResultNode* container = node->GetAsContainer();
      container->mResult = mResult;
      container->FillStats();
    }
    accessCount += node->mAccessCount;
    // this is how container nodes get sorted by date
    // The container gets the most recent time of the child nodes.
    if (node->mTime > newTime)
      newTime = node->mTime;
  }

  if (mExpanded) {
    mAccessCount = accessCount;
    if (!IsQuery() || newTime > mTime)
      mTime = newTime;
  }
}


/**
 * This is used when one container changes to do a minimal update of the tree
 * structure.  When something changes, you want to call FillStats if necessary
 * and update this container completely.  Then call this function which will
 * walk up the tree and fill in the previous containers.
 *
 * Note that you have to tell us by how much our access count changed.  Our
 * access count should already be set to the new value; this is used tochange
 * the parents without having to re-count all their children.
 *
 * This does NOT update the last visit date downward.  Therefore, if you are
 * deleting a node that has the most recent last visit date, the parents will
 * not get their last visit dates downshifted accordingly.  This is a rather
 * unusual case: we don't often delete things, and we usually don't even show
 * the last visit date for folders.  Updating would be slower because we would
 * have to recompute it from scratch.
 */
nsresult
nsNavHistoryContainerResultNode::ReverseUpdateStats(PRInt32 aAccessCountChange)
{
  if (mParent) {
    nsNavHistoryResult* result = GetResult();
    bool shouldNotify = result && mParent->mParent &&
                          mParent->mParent->AreChildrenVisible();

    mParent->mAccessCount += aAccessCountChange;
    bool timeChanged = false;
    if (mTime > mParent->mTime) {
      timeChanged = true;
      mParent->mTime = mTime;
    }

    if (shouldNotify) {
      NOTIFY_RESULT_OBSERVERS(result,
                              NodeHistoryDetailsChanged(TO_ICONTAINER(mParent),
                                                        mParent->mTime,
                                                        mParent->mAccessCount));
    }

    // check sorting, the stats may have caused this node to move if the
    // sorting depended on something we are changing.
    PRUint16 sortMode = mParent->GetSortType();
    bool sortingByVisitCount =
      sortMode == nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_ASCENDING ||
      sortMode == nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_DESCENDING;
    bool sortingByTime =
      sortMode == nsINavHistoryQueryOptions::SORT_BY_DATE_ASCENDING ||
      sortMode == nsINavHistoryQueryOptions::SORT_BY_DATE_DESCENDING;

    if ((sortingByVisitCount && aAccessCountChange != 0) ||
        (sortingByTime && timeChanged)) {
      PRUint32 ourIndex = mParent->FindChild(this);
      NS_ASSERTION(ourIndex >= 0, "Could not find self in parent");
      if (ourIndex >= 0)
        EnsureItemPosition(ourIndex);
    }

    nsresult rv = mParent->ReverseUpdateStats(aAccessCountChange);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}


/**
 * This walks up the tree until we find a query result node or the root to get
 * the sorting type.
 */
PRUint16
nsNavHistoryContainerResultNode::GetSortType()
{
  if (mParent)
    return mParent->GetSortType();
  if (mResult)
    return mResult->mSortingMode;

  NS_NOTREACHED("We should always have a result");
  return nsINavHistoryQueryOptions::SORT_BY_NONE;
}


nsresult nsNavHistoryContainerResultNode::Refresh() {
  NS_WARNING("Refresh() is supported by queries or folders, not generic containers.");
  return NS_OK;
}

void
nsNavHistoryContainerResultNode::GetSortingAnnotation(nsACString& aAnnotation)
{
  if (mParent)
    mParent->GetSortingAnnotation(aAnnotation);
  else if (mResult)
    aAnnotation.Assign(mResult->mSortingAnnotation);
  else
    NS_NOTREACHED("We should always have a result");
}

/**
 * @return the sorting comparator function for the give sort type, or null if
 * there is no comparator.
 */
nsNavHistoryContainerResultNode::SortComparator
nsNavHistoryContainerResultNode::GetSortingComparator(PRUint16 aSortType)
{
  switch (aSortType)
  {
    case nsINavHistoryQueryOptions::SORT_BY_NONE:
      return &SortComparison_Bookmark;
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
    case nsINavHistoryQueryOptions::SORT_BY_KEYWORD_ASCENDING:
      return &SortComparison_KeywordLess;
    case nsINavHistoryQueryOptions::SORT_BY_KEYWORD_DESCENDING:
      return &SortComparison_KeywordGreater;
    case nsINavHistoryQueryOptions::SORT_BY_ANNOTATION_ASCENDING:
      return &SortComparison_AnnotationLess;
    case nsINavHistoryQueryOptions::SORT_BY_ANNOTATION_DESCENDING:
      return &SortComparison_AnnotationGreater;
    case nsINavHistoryQueryOptions::SORT_BY_DATEADDED_ASCENDING:
      return &SortComparison_DateAddedLess;
    case nsINavHistoryQueryOptions::SORT_BY_DATEADDED_DESCENDING:
      return &SortComparison_DateAddedGreater;
    case nsINavHistoryQueryOptions::SORT_BY_LASTMODIFIED_ASCENDING:
      return &SortComparison_LastModifiedLess;
    case nsINavHistoryQueryOptions::SORT_BY_LASTMODIFIED_DESCENDING:
      return &SortComparison_LastModifiedGreater;
    case nsINavHistoryQueryOptions::SORT_BY_TAGS_ASCENDING:
      return &SortComparison_TagsLess;
    case nsINavHistoryQueryOptions::SORT_BY_TAGS_DESCENDING:
      return &SortComparison_TagsGreater;
    case nsINavHistoryQueryOptions::SORT_BY_FRECENCY_ASCENDING:
      return &SortComparison_FrecencyLess;
    case nsINavHistoryQueryOptions::SORT_BY_FRECENCY_DESCENDING:
      return &SortComparison_FrecencyGreater;
    default:
      NS_NOTREACHED("Bad sorting type");
      return nsnull;
  }
}


/**
 * This is used by Result::SetSortingMode and QueryResultNode::FillChildren to
 * sort the child list.
 *
 * This does NOT update any visibility or tree information.  The caller will
 * have to completely rebuild the visible list after this.
 */
void
nsNavHistoryContainerResultNode::RecursiveSort(
    const char* aData, SortComparator aComparator)
{
  void* data = const_cast<void*>(static_cast<const void*>(aData));

  mChildren.Sort(aComparator, data);
  for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
    if (mChildren[i]->IsContainer())
      mChildren[i]->GetAsContainer()->RecursiveSort(aData, aComparator);
  }
}


/**
 * @return the index that the given item would fall on if it were to be
 * inserted using the given sorting.
 */
PRUint32
nsNavHistoryContainerResultNode::FindInsertionPoint(
    nsNavHistoryResultNode* aNode, SortComparator aComparator,
    const char* aData, bool* aItemExists)
{
  if (aItemExists)
    (*aItemExists) = false;

  if (mChildren.Count() == 0)
    return 0;

  void* data = const_cast<void*>(static_cast<const void*>(aData));

  // The common case is the beginning or the end because this is used to insert
  // new items that are added to history, which is usually sorted by date.
  PRInt32 res;
  res = aComparator(aNode, mChildren[0], data);
  if (res <= 0) {
    if (aItemExists && res == 0)
      (*aItemExists) = true;
    return 0;
  }
  res = aComparator(aNode, mChildren[mChildren.Count() - 1], data);
  if (res >= 0) {
    if (aItemExists && res == 0)
      (*aItemExists) = true;
    return mChildren.Count();
  }

  PRUint32 beginRange = 0; // inclusive
  PRUint32 endRange = mChildren.Count(); // exclusive
  while (1) {
    if (beginRange == endRange)
      return endRange;
    PRUint32 center = beginRange + (endRange - beginRange) / 2;
    PRInt32 res = aComparator(aNode, mChildren[center], data);
    if (res <= 0) {
      endRange = center; // left side
      if (aItemExists && res == 0)
        (*aItemExists) = true;
    }
    else {
      beginRange = center + 1; // right site
    }
  }
}


/**
 * This checks the child node at the given index to see if its sorting is
 * correct.  This is called when nodes are updated and we need to see whether
 * we need to move it.
 *
 * @returns true if not and it should be resorted.
*/
bool
nsNavHistoryContainerResultNode::DoesChildNeedResorting(PRUint32 aIndex,
    SortComparator aComparator, const char* aData)
{
  NS_ASSERTION(aIndex >= 0 && aIndex < PRUint32(mChildren.Count()),
               "Input index out of range");
  if (mChildren.Count() == 1)
    return false;

  void* data = const_cast<void*>(static_cast<const void*>(aData));

  if (aIndex > 0) {
    // compare to previous item
    if (aComparator(mChildren[aIndex - 1], mChildren[aIndex], data) > 0)
      return true;
  }
  if (aIndex < PRUint32(mChildren.Count()) - 1) {
    // compare to next item
    if (aComparator(mChildren[aIndex], mChildren[aIndex + 1], data) > 0)
      return true;
  }
  return false;
}


/* static */
PRInt32 nsNavHistoryContainerResultNode::SortComparison_StringLess(
    const nsAString& a, const nsAString& b) {

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, 0);
  nsICollation* collation = history->GetCollation();
  NS_ENSURE_TRUE(collation, 0);

  PRInt32 res = 0;
  collation->CompareString(nsICollation::kCollationCaseInSensitive, a, b, &res);
  return res;
}


/**
 * When there are bookmark indices, we should never have ties, so we don't
 * need to worry about tiebreaking.  When there are no bookmark indices,
 * everything will be -1 and we don't worry about sorting.
 */
PRInt32 nsNavHistoryContainerResultNode::SortComparison_Bookmark(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  return a->mBookmarkIndex - b->mBookmarkIndex;
}

/**
 * These are a little more complicated because they do a localization
 * conversion.  If this is too slow, we can compute the sort keys once in
 * advance, sort that array, and then reorder the real array accordingly.
 * This would save some key generations.
 *
 * The collation object must be allocated before sorting on title!
 */
PRInt32 nsNavHistoryContainerResultNode::SortComparison_TitleLess(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  PRUint32 aType;
  a->GetType(&aType);

  PRInt32 value = SortComparison_StringLess(NS_ConvertUTF8toUTF16(a->mTitle),
                                            NS_ConvertUTF8toUTF16(b->mTitle));
  if (value == 0) {
    // resolve by URI
    if (a->IsURI()) {
      value = a->mURI.Compare(b->mURI.get());
    }
    if (value == 0) {
      // resolve by date
      value = ComparePRTime(a->mTime, b->mTime);
      if (value == 0)
        value = nsNavHistoryContainerResultNode::SortComparison_Bookmark(a, b, closure);
    }
  }
  return value;
}
PRInt32 nsNavHistoryContainerResultNode::SortComparison_TitleGreater(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  return -SortComparison_TitleLess(a, b, closure);
}

/**
 * Equal times will be very unusual, but it is important that there is some
 * deterministic ordering of the results so they don't move around.
 */
PRInt32 nsNavHistoryContainerResultNode::SortComparison_DateLess(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  PRInt32 value = ComparePRTime(a->mTime, b->mTime);
  if (value == 0) {
    value = SortComparison_StringLess(NS_ConvertUTF8toUTF16(a->mTitle),
                                      NS_ConvertUTF8toUTF16(b->mTitle));
    if (value == 0)
      value = nsNavHistoryContainerResultNode::SortComparison_Bookmark(a, b, closure);
  }
  return value;
}
PRInt32 nsNavHistoryContainerResultNode::SortComparison_DateGreater(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  return -nsNavHistoryContainerResultNode::SortComparison_DateLess(a, b, closure);
}


PRInt32 nsNavHistoryContainerResultNode::SortComparison_DateAddedLess(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  PRInt32 value = ComparePRTime(a->mDateAdded, b->mDateAdded);
  if (value == 0) {
    value = SortComparison_StringLess(NS_ConvertUTF8toUTF16(a->mTitle),
                                      NS_ConvertUTF8toUTF16(b->mTitle));
    if (value == 0)
      value = nsNavHistoryContainerResultNode::SortComparison_Bookmark(a, b, closure);
  }
  return value;
}
PRInt32 nsNavHistoryContainerResultNode::SortComparison_DateAddedGreater(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  return -nsNavHistoryContainerResultNode::SortComparison_DateAddedLess(a, b, closure);
}


PRInt32 nsNavHistoryContainerResultNode::SortComparison_LastModifiedLess(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  PRInt32 value = ComparePRTime(a->mLastModified, b->mLastModified);
  if (value == 0) {
    value = SortComparison_StringLess(NS_ConvertUTF8toUTF16(a->mTitle),
                                      NS_ConvertUTF8toUTF16(b->mTitle));
    if (value == 0)
      value = nsNavHistoryContainerResultNode::SortComparison_Bookmark(a, b, closure);
  }
  return value;
}
PRInt32 nsNavHistoryContainerResultNode::SortComparison_LastModifiedGreater(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  return -nsNavHistoryContainerResultNode::SortComparison_LastModifiedLess(a, b, closure);
}


/**
 * Certain types of parent nodes are treated specially because URIs are not
 * valid (like days or hosts).
 */
PRInt32 nsNavHistoryContainerResultNode::SortComparison_URILess(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  PRInt32 value;
  if (a->IsURI() && b->IsURI()) {
    // normal URI or visit
    value = a->mURI.Compare(b->mURI.get());
  } else {
    // for everything else, use title (= host name)
    value = SortComparison_StringLess(NS_ConvertUTF8toUTF16(a->mTitle),
                                      NS_ConvertUTF8toUTF16(b->mTitle));
  }

  if (value == 0) {
    value = ComparePRTime(a->mTime, b->mTime);
    if (value == 0)
      value = nsNavHistoryContainerResultNode::SortComparison_Bookmark(a, b, closure);
  }
  return value;
}
PRInt32 nsNavHistoryContainerResultNode::SortComparison_URIGreater(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  return -SortComparison_URILess(a, b, closure);
}


PRInt32 nsNavHistoryContainerResultNode::SortComparison_KeywordLess(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  PRInt32 value = 0;
  if (a->mItemId != -1 || b->mItemId != -1) {
    // compare the keywords
    nsAutoString keywordA, keywordB;
    nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
    NS_ENSURE_TRUE(bookmarks, 0);

    nsresult rv;
    if (a->mItemId != -1) {
      rv = bookmarks->GetKeywordForBookmark(a->mItemId, keywordA);
      NS_ENSURE_SUCCESS(rv, 0);
    }
    if (b->mItemId != -1) {
      rv = bookmarks->GetKeywordForBookmark(b->mItemId, keywordB);
      NS_ENSURE_SUCCESS(rv, 0);
    }

    value = SortComparison_StringLess(keywordA, keywordB);
  }

  // Fall back to title sorting.
  if (value == 0)
    value = SortComparison_TitleLess(a, b, closure);

  return value;
}

PRInt32 nsNavHistoryContainerResultNode::SortComparison_KeywordGreater(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  return -SortComparison_KeywordLess(a, b, closure);
}

PRInt32 nsNavHistoryContainerResultNode::SortComparison_AnnotationLess(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  nsCAutoString annoName(static_cast<char*>(closure));
  NS_ENSURE_TRUE(!annoName.IsEmpty(), 0);

  bool a_itemAnno = false;
  bool b_itemAnno = false;

  // Not used for item annos
  nsCOMPtr<nsIURI> a_uri, b_uri;
  if (a->mItemId != -1) {
    a_itemAnno = true;
  } else {
    nsCAutoString spec;
    if (NS_SUCCEEDED(a->GetUri(spec)))
      NS_NewURI(getter_AddRefs(a_uri), spec);
    NS_ENSURE_TRUE(a_uri, 0);
  }

  if (b->mItemId != -1) {
    b_itemAnno = true;
  } else {
    nsCAutoString spec;
    if (NS_SUCCEEDED(b->GetUri(spec)))
      NS_NewURI(getter_AddRefs(b_uri), spec);
    NS_ENSURE_TRUE(b_uri, 0);
  }

  nsAnnotationService* annosvc = nsAnnotationService::GetAnnotationService();
  NS_ENSURE_TRUE(annosvc, 0);

  bool a_hasAnno, b_hasAnno;
  if (a_itemAnno) {
    NS_ENSURE_SUCCESS(annosvc->ItemHasAnnotation(a->mItemId, annoName,
                                                 &a_hasAnno), 0);
  } else {
    NS_ENSURE_SUCCESS(annosvc->PageHasAnnotation(a_uri, annoName,
                                                 &a_hasAnno), 0);
  }
  if (b_itemAnno) {
    NS_ENSURE_SUCCESS(annosvc->ItemHasAnnotation(b->mItemId, annoName,
                                                 &b_hasAnno), 0);
  } else {
    NS_ENSURE_SUCCESS(annosvc->PageHasAnnotation(b_uri, annoName,
                                                 &b_hasAnno), 0);    
  }

  PRInt32 value = 0;
  if (a_hasAnno || b_hasAnno) {
    PRUint16 annoType;
    if (a_hasAnno) {
      if (a_itemAnno) {
        NS_ENSURE_SUCCESS(annosvc->GetItemAnnotationType(a->mItemId,
                                                         annoName,
                                                         &annoType), 0);
      } else {
        NS_ENSURE_SUCCESS(annosvc->GetPageAnnotationType(a_uri, annoName,
                                                         &annoType), 0);
      }
    }
    if (b_hasAnno) {
      PRUint16 b_type;
      if (b_itemAnno) {
        NS_ENSURE_SUCCESS(annosvc->GetItemAnnotationType(b->mItemId,
                                                         annoName,
                                                         &b_type), 0);
      } else {
        NS_ENSURE_SUCCESS(annosvc->GetPageAnnotationType(b_uri, annoName,
                                                         &b_type), 0);
      }
      // We better make the API not support this state, really
      // XXXmano: this is actually wrong for double<->int and int64<->int32
      if (a_hasAnno && b_type != annoType)
        return 0;
      annoType = b_type;
    }

#define GET_ANNOTATIONS_VALUES(METHOD_ITEM, METHOD_PAGE, A_VAL, B_VAL)        \
        if (a_hasAnno) {                                                      \
          if (a_itemAnno) {                                                   \
            NS_ENSURE_SUCCESS(annosvc->METHOD_ITEM(a->mItemId, annoName,      \
                                                   A_VAL), 0);                \
          } else {                                                            \
            NS_ENSURE_SUCCESS(annosvc->METHOD_PAGE(a_uri, annoName,           \
                                                   A_VAL), 0);                \
          }                                                                   \
        }                                                                     \
        if (b_hasAnno) {                                                      \
          if (b_itemAnno) {                                                   \
            NS_ENSURE_SUCCESS(annosvc->METHOD_ITEM(b->mItemId, annoName,      \
                                                   B_VAL), 0);                \
          } else {                                                            \
            NS_ENSURE_SUCCESS(annosvc->METHOD_PAGE(b_uri, annoName,           \
                                                   B_VAL), 0);                \
          }                                                                   \
        }

    // Surprising as it is, we don't support sorting by a binary annotation
    if (annoType != nsIAnnotationService::TYPE_BINARY) {
      if (annoType == nsIAnnotationService::TYPE_STRING) {
        nsAutoString a_val, b_val;
        GET_ANNOTATIONS_VALUES(GetItemAnnotationString,
                               GetPageAnnotationString, a_val, b_val);
        value = SortComparison_StringLess(a_val, b_val);
      }
      else if (annoType == nsIAnnotationService::TYPE_INT32) {
        PRInt32 a_val = 0, b_val = 0;
        GET_ANNOTATIONS_VALUES(GetItemAnnotationInt32,
                               GetPageAnnotationInt32, &a_val, &b_val);
        value = (a_val < b_val) ? -1 : (a_val > b_val) ? 1 : 0;
      }
      else if (annoType == nsIAnnotationService::TYPE_INT64) {
        PRInt64 a_val = 0, b_val = 0;
        GET_ANNOTATIONS_VALUES(GetItemAnnotationInt64,
                               GetPageAnnotationInt64, &a_val, &b_val);
        value = (a_val < b_val) ? -1 : (a_val > b_val) ? 1 : 0;
      }
      else if (annoType == nsIAnnotationService::TYPE_DOUBLE) {
        double a_val = 0, b_val = 0;
        GET_ANNOTATIONS_VALUES(GetItemAnnotationDouble,
                               GetPageAnnotationDouble, &a_val, &b_val);
        value = (a_val < b_val) ? -1 : (a_val > b_val) ? 1 : 0;
      }
    }
  }

  // Note we also fall back to the title-sorting route one of the items didn't
  // have the annotation set or if both had it set but in a different storage
  // type
  if (value == 0)
    return SortComparison_TitleLess(a, b, nsnull);

  return value;
}
PRInt32 nsNavHistoryContainerResultNode::SortComparison_AnnotationGreater(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  return -SortComparison_AnnotationLess(a, b, closure);
}

/**
 * Fall back on dates for conflict resolution
 */
PRInt32 nsNavHistoryContainerResultNode::SortComparison_VisitCountLess(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  PRInt32 value = CompareIntegers(a->mAccessCount, b->mAccessCount);
  if (value == 0) {
    value = ComparePRTime(a->mTime, b->mTime);
    if (value == 0)
      value = nsNavHistoryContainerResultNode::SortComparison_Bookmark(a, b, closure);
  }
  return value;
}
PRInt32 nsNavHistoryContainerResultNode::SortComparison_VisitCountGreater(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  return -nsNavHistoryContainerResultNode::SortComparison_VisitCountLess(a, b, closure);
}


PRInt32 nsNavHistoryContainerResultNode::SortComparison_TagsLess(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  PRInt32 value = 0;
  nsAutoString aTags, bTags;

  nsresult rv = a->GetTags(aTags);
  NS_ENSURE_SUCCESS(rv, 0);

  rv = b->GetTags(bTags);
  NS_ENSURE_SUCCESS(rv, 0);

  value = SortComparison_StringLess(aTags, bTags);

  // fall back to title sorting
  if (value == 0)
    value = SortComparison_TitleLess(a, b, closure);

  return value;
}

PRInt32 nsNavHistoryContainerResultNode::SortComparison_TagsGreater(
    nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure)
{
  return -SortComparison_TagsLess(a, b, closure);
}

/**
 * Fall back on date and bookmarked status, for conflict resolution.
 */
PRInt32
nsNavHistoryContainerResultNode::SortComparison_FrecencyLess(
  nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure
)
{
  PRInt32 value = CompareIntegers(a->mFrecency, b->mFrecency);
  if (value == 0) {
    value = ComparePRTime(a->mTime, b->mTime);
    if (value == 0) {
      value = nsNavHistoryContainerResultNode::SortComparison_Bookmark(a, b, closure);
    }
  }
  return value;
}
PRInt32
nsNavHistoryContainerResultNode::SortComparison_FrecencyGreater(
  nsNavHistoryResultNode* a, nsNavHistoryResultNode* b, void* closure
)
{
  return -nsNavHistoryContainerResultNode::SortComparison_FrecencyLess(a, b, closure);
}

/**
 * Searches this folder for a node with the given URI.  Returns null if not
 * found.
 *
 * @note Does not addref the node!
 */
nsNavHistoryResultNode*
nsNavHistoryContainerResultNode::FindChildURI(const nsACString& aSpec,
    PRUint32* aNodeIndex)
{
  for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
    if (mChildren[i]->IsURI()) {
      if (aSpec.Equals(mChildren[i]->mURI)) {
        *aNodeIndex = i;
        return mChildren[i];
      }
    }
  }
  return nsnull;
}


/**
 * Searches this container for a subfolder with the given name.  This is used
 * to find host and "day" nodes.
 *
 * @return null if not found.
 * @note Does not addref the node!
 */
nsNavHistoryContainerResultNode*
nsNavHistoryContainerResultNode::FindChildContainerByName(
    const nsACString& aTitle, PRUint32* aNodeIndex)
{
  for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
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


/**
 * This does the work of adding a child to the container.  The child can be
 * either a container or or a single item that may even be collapsed with the
 * adjacent ones.
 *
 * Some inserts are "temporary" meaning that they are happening immediately
 * after a temporary remove.  We do this when movings elements when they
 * change to keep them in the proper sorting position.  In these cases, we
 * don't need to recompute any statistics.
 */
nsresult
nsNavHistoryContainerResultNode::InsertChildAt(nsNavHistoryResultNode* aNode,
                                               PRInt32 aIndex,
                                               bool aIsTemporary)
{
  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_STATE(result);

  aNode->mParent = this;
  aNode->mIndentLevel = mIndentLevel + 1;
  if (!aIsTemporary && aNode->IsContainer()) {
    // need to update all the new item's children
    nsNavHistoryContainerResultNode* container = aNode->GetAsContainer();
    container->mResult = result;
    container->FillStats();
  }

  if (!mChildren.InsertObjectAt(aNode, aIndex))
    return NS_ERROR_OUT_OF_MEMORY;

  // Update our stats and notify the result's observers.
  if (!aIsTemporary) {
    mAccessCount += aNode->mAccessCount;
    if (mTime < aNode->mTime)
      mTime = aNode->mTime;
    if (!mParent || mParent->AreChildrenVisible()) {
      NOTIFY_RESULT_OBSERVERS(result,
                              NodeHistoryDetailsChanged(TO_ICONTAINER(this),
                                                        mTime,
                                                        mAccessCount));
    }

    nsresult rv = ReverseUpdateStats(aNode->mAccessCount);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Update tree if we are visible.  Note that we could be here and not
  // expanded, like when there is a bookmark folder being updated because its
  // parent is visible.
  if (AreChildrenVisible())
    NOTIFY_RESULT_OBSERVERS(result, NodeInserted(this, aNode, aIndex));

  return NS_OK;
}


/**
 * This locates the proper place for insertion according to the current sort
 * and calls InsertChildAt
 */
nsresult
nsNavHistoryContainerResultNode::InsertSortedChild(
    nsNavHistoryResultNode* aNode, 
    bool aIsTemporary, bool aIgnoreDuplicates)
{

  if (mChildren.Count() == 0)
    return InsertChildAt(aNode, 0, aIsTemporary);

  SortComparator comparator = GetSortingComparator(GetSortType());
  if (comparator) {
    // When inserting a new node, it must have proper statistics because we use
    // them to find the correct insertion point.  The insert function will then
    // recompute these statistics and fill in the proper parents and hierarchy
    // level.  Doing this twice shouldn't be a large performance penalty because
    // when we are inserting new containers, they typically contain only one
    // item (because we've browsed a new page).
    if (!aIsTemporary && aNode->IsContainer()) {
      // need to update all the new item's children
      nsNavHistoryContainerResultNode* container = aNode->GetAsContainer();
      container->mResult = mResult;
      container->FillStats();
    }

    nsCAutoString sortingAnnotation;
    GetSortingAnnotation(sortingAnnotation);
    bool itemExists;
    PRUint32 position = FindInsertionPoint(aNode, comparator, 
                                           sortingAnnotation.get(), 
                                           &itemExists);
    if (aIgnoreDuplicates && itemExists)
      return NS_OK;

    return InsertChildAt(aNode, position, aIsTemporary);
  }
  return InsertChildAt(aNode, mChildren.Count(), aIsTemporary);
}

/**
 * This checks if the item at aIndex is located correctly given the sorting
 * move.  If it's not, the item is moved, and the result's observers are
 * notified.
 *
 * @return true if the item position has been changed, false otherwise.
 */
bool
nsNavHistoryContainerResultNode::EnsureItemPosition(PRUint32 aIndex) {
  NS_ASSERTION(aIndex < (PRUint32)mChildren.Count(), "Invalid index");
  if (aIndex >= (PRUint32)mChildren.Count())
    return false;

  SortComparator comparator = GetSortingComparator(GetSortType());
  if (!comparator)
    return false;

  nsCAutoString sortAnno;
  GetSortingAnnotation(sortAnno);
  if (!DoesChildNeedResorting(aIndex, comparator, sortAnno.get()))
    return false;

  nsRefPtr<nsNavHistoryResultNode> node(mChildren[aIndex]);
  mChildren.RemoveObjectAt(aIndex);

  PRUint32 newIndex = FindInsertionPoint(
                          node, comparator,sortAnno.get(), nsnull);
  mChildren.InsertObjectAt(node.get(), newIndex);

  if (AreChildrenVisible()) {
    nsNavHistoryResult* result = GetResult();
    NOTIFY_RESULT_OBSERVERS_RET(result,
                                NodeMoved(node, this, aIndex, this, newIndex),
                                false);
  }

  return true;
}


/**
 * This takes a list of nodes and merges them into the current result set.
 * Any containers that are added must already be sorted.
 *
 * This assumes that the items in 'aAddition' are new visits or replacement
 * URIs.  We do not update visits.
 *
 * @note In the future, we can do more updates incrementally using.  When a URI
 * changes in a way we can't easily handle, construct a query with each query
 * object specifying an exact match for the URI in question.  Then remove all
 * instances of that URI in the result and call this function.
 */
void
nsNavHistoryContainerResultNode::MergeResults(
    nsCOMArray<nsNavHistoryResultNode>* aAddition)
{
  // Generally we will have very few (one) entries in the addition list, so
  // just iterate through it.  If we find we may have a lot, we may want to do
  // some hashing to help with the merge.
  for (PRUint32 i = 0; i < PRUint32(aAddition->Count()); ++i) {
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
        nsNavHistoryResultNode* oldNode =
          FindChildURI(curAddition->mURI, &oldIndex);
        if (oldNode) {
          // if we don't have a parent (for example, the history
          // sidebar, when sorted by last visited or most visited)
          // we have to manually Remove/Insert instead of Replace
          // see bug #389782 for details
          if (mParent)
            ReplaceChildURIAt(oldIndex, curAddition);
          else {
            RemoveChildAt(oldIndex, true);
            InsertSortedChild(curAddition, true);
          }
        }
        else
          InsertSortedChild(curAddition);
      }
    }
  }
}


/**
 * This is called to replace a leaf node.  It will update tree stats and notify
 * the result's observers.  You can not use this to replace a container.
 *
 * This assumes that the node is being replaced with a newer version of itself
 * and so its visit count will not go down.  Also, this means that the
 * collapsing of duplicates will not change.
 */
nsresult
nsNavHistoryContainerResultNode::ReplaceChildURIAt(PRUint32 aIndex,
    nsNavHistoryResultNode* aNode)
{
  NS_ASSERTION(aIndex < PRUint32(mChildren.Count()),
               "Invalid index for replacement");
  NS_ASSERTION(mChildren[aIndex]->IsURI(),
               "Can not use ReplaceChildAt for a node of another type");
  NS_ASSERTION(mChildren[aIndex]->mURI.Equals(aNode->mURI),
               "We must replace a URI with an updated one of the same");

  aNode->mParent = this;
  aNode->mIndentLevel = mIndentLevel + 1;

  // Update tree stats if needed.
  PRUint32 accessCountChange = aNode->mAccessCount - mChildren[aIndex]->mAccessCount;
  if (accessCountChange != 0 || mChildren[aIndex]->mTime != aNode->mTime) {
    NS_ASSERTION(aNode->mAccessCount >= mChildren[aIndex]->mAccessCount,
                 "Replacing a node with one back in time or some nonmatching node");

    mAccessCount += accessCountChange;
    if (mTime < aNode->mTime)
      mTime = aNode->mTime;
    nsresult rv = ReverseUpdateStats(accessCountChange);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // Hold a reference so it doesn't go away as soon as we remove it from the
  // array.
  nsRefPtr<nsNavHistoryResultNode> oldItem = mChildren[aIndex];
  if (!mChildren.ReplaceObjectAt(aNode, aIndex))
    return NS_ERROR_FAILURE;

  if (AreChildrenVisible()) {
    nsNavHistoryResult* result = GetResult();
    NOTIFY_RESULT_OBSERVERS(result,
                            NodeReplaced(this, oldItem, aNode, aIndex));
  }

  mChildren[aIndex]->OnRemoving();
  return NS_OK;
}


/**
 * This does all the work of removing a child from this container, including
 * updating the tree if necessary.  Note that we do not need to be open for
 * this to work.
 *
 * Some removes are "temporary" meaning that they'll just get inserted again.
 * We do this for resorting.  In these cases, we don't need to recompute any
 * statistics, and we shouldn't notify those container that they are being
 * removed.
 */
nsresult
nsNavHistoryContainerResultNode::RemoveChildAt(PRInt32 aIndex,
                                               bool aIsTemporary)
{
  NS_ASSERTION(aIndex >= 0 && aIndex < mChildren.Count(), "Invalid index");

  // Hold an owning reference to keep from expiring while we work with it.
  nsRefPtr<nsNavHistoryResultNode> oldNode = mChildren[aIndex];

  // Update stats.
  PRUint32 oldAccessCount = 0;
  if (!aIsTemporary) {
    oldAccessCount = mAccessCount;
    mAccessCount -= mChildren[aIndex]->mAccessCount;
    NS_ASSERTION(mAccessCount >= 0, "Invalid access count while updating!");
  }

  // Remove it from our list and notify the result's observers.
  mChildren.RemoveObjectAt(aIndex);
  if (AreChildrenVisible()) {
    nsNavHistoryResult* result = GetResult();
    NOTIFY_RESULT_OBSERVERS(result,
                            NodeRemoved(this, oldNode, aIndex));
  }

  if (!aIsTemporary) {
    nsresult rv = ReverseUpdateStats(mAccessCount - oldAccessCount);
    NS_ENSURE_SUCCESS(rv, rv);
    oldNode->OnRemoving();
  }
  return NS_OK;
}


/**
 * Searches for matches for the given URI.  If aOnlyOne is set, it will
 * terminate as soon as it finds a single match.  This would be used when there
 * are URI results so there will only ever be one copy of any URI.
 *
 * When aOnlyOne is false, it will check all elements.  This is for visit
 * style results that may have multiple copies of any given URI.
 */
void
nsNavHistoryContainerResultNode::RecursiveFindURIs(bool aOnlyOne,
    nsNavHistoryContainerResultNode* aContainer, const nsCString& aSpec,
    nsCOMArray<nsNavHistoryResultNode>* aMatches)
{
  for (PRInt32 child = 0; child < aContainer->mChildren.Count(); ++child) {
    PRUint32 type;
    aContainer->mChildren[child]->GetType(&type);
    if (nsNavHistoryResultNode::IsTypeURI(type)) {
      // compare URIs
      nsNavHistoryResultNode* uriNode = aContainer->mChildren[child];
      if (uriNode->mURI.Equals(aSpec)) {
        // found
        aMatches->AppendObject(uriNode);
        if (aOnlyOne)
          return;
      }
    }
  }
}


/**
 * If aUpdateSort is true, we will also update the sorting of this item.
 * Normally you want this to be true, but it can be false if the thing you are
 * changing can not affect sorting (like favicons).
 *
 * You should NOT change any child lists as part of the callback function.
 */
nsresult
nsNavHistoryContainerResultNode::UpdateURIs(bool aRecursive, bool aOnlyOne,
    bool aUpdateSort, const nsCString& aSpec,
    nsresult (*aCallback)(nsNavHistoryResultNode*,void*, nsNavHistoryResult*), void* aClosure)
{
  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_STATE(result);

  // this needs to be owning since sometimes we remove and re-insert nodes
  // in their parents and we don't want them to go away.
  nsCOMArray<nsNavHistoryResultNode> matches;

  if (aRecursive) {
    RecursiveFindURIs(aOnlyOne, this, aSpec, &matches);
  } else if (aOnlyOne) {
    PRUint32 nodeIndex;
    nsNavHistoryResultNode* node = FindChildURI(aSpec, &nodeIndex);
    if (node)
      matches.AppendObject(node);
  } else {
    NS_NOTREACHED("UpdateURIs does not handle nonrecursive updates of multiple items.");
    // this case easy to add if you need it, just find all the matching URIs
    // at this level.  However, this isn't currently used. History uses
    // recursive, Bookmarks uses one level and knows that the match is unique.
    return NS_ERROR_FAILURE;
  }
  if (matches.Count() == 0)
    return NS_OK;

  // PERFORMANCE: This updates each container for each child in it that
  // changes.  In some cases, many elements have changed inside the same
  // container.  It would be better to compose a list of containers, and
  // update each one only once for all the items that have changed in it.
  for (PRInt32 i = 0; i < matches.Count(); ++i)
  {
    nsNavHistoryResultNode* node = matches[i];
    nsNavHistoryContainerResultNode* parent = node->mParent;
    if (!parent) {
      NS_NOTREACHED("All URI nodes being updated must have parents");
      continue;
    }

    PRUint32 oldAccessCount = node->mAccessCount;
    PRTime oldTime = node->mTime;
    aCallback(node, aClosure, result);

    if (oldAccessCount != node->mAccessCount || oldTime != node->mTime) {
      parent->mAccessCount += node->mAccessCount - oldAccessCount;
      if (node->mTime > parent->mTime)
        parent->mTime = node->mTime;
      if (parent->AreChildrenVisible()) {
          NOTIFY_RESULT_OBSERVERS(result,
                                  NodeHistoryDetailsChanged(
                                    TO_ICONTAINER(parent),
                                    parent->mTime,
                                    parent->mAccessCount));
      }
      nsresult rv = parent->ReverseUpdateStats(node->mAccessCount - oldAccessCount);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    if (aUpdateSort) {
      PRInt32 childIndex = parent->FindChild(node);
      NS_ASSERTION(childIndex >= 0, "Could not find child we just got a reference to");
      if (childIndex >= 0)
        parent->EnsureItemPosition(childIndex);
    }
  }

  return NS_OK;
}


/**
 * This is used to update the titles in the tree.  This is called from both
 * query and bookmark folder containers to update the tree.  Bookmark folders
 * should be sure to set recursive to false, since child folders will have
 * their own callbacks registered.
 */
static nsresult setTitleCallback(
    nsNavHistoryResultNode* aNode, void* aClosure,
    nsNavHistoryResult* aResult)
{
  const nsACString* newTitle = reinterpret_cast<nsACString*>(aClosure);
  aNode->mTitle = *newTitle;

  if (aResult && (!aNode->mParent || aNode->mParent->AreChildrenVisible()))
    NOTIFY_RESULT_OBSERVERS(aResult, NodeTitleChanged(aNode, *newTitle));

  return NS_OK;
}
nsresult
nsNavHistoryContainerResultNode::ChangeTitles(nsIURI* aURI,
                                              const nsACString& aNewTitle,
                                              bool aRecursive,
                                              bool aOnlyOne)
{
  // uri string
  nsCAutoString uriString;
  nsresult rv = aURI->GetSpec(uriString);
  NS_ENSURE_SUCCESS(rv, rv);

  // The recursive function will update the result's tree nodes, but only if we
  // give it a non-null pointer.  So if there isn't a tree, just pass NULL so
  // it doesn't bother trying to call the result.
  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_STATE(result);

  PRUint16 sortType = GetSortType();
  bool updateSorting =
    (sortType == nsINavHistoryQueryOptions::SORT_BY_TITLE_ASCENDING ||
     sortType == nsINavHistoryQueryOptions::SORT_BY_TITLE_DESCENDING);

  rv = UpdateURIs(aRecursive, aOnlyOne, updateSorting, uriString,
                 setTitleCallback,
                 const_cast<void*>(reinterpret_cast<const void*>(&aNewTitle)));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


/**
 * Complex containers (folders and queries) will override this.  Here, we
 * handle the case of simple containers (like host groups) where the children
 * are always stored.
 */
NS_IMETHODIMP
nsNavHistoryContainerResultNode::GetHasChildren(bool *aHasChildren)
{
  *aHasChildren = (mChildren.Count() > 0);
  return NS_OK;
}


/**
 * @throws if this node is closed.
 */
NS_IMETHODIMP
nsNavHistoryContainerResultNode::GetChildCount(PRUint32* aChildCount)
{
  if (!mExpanded)
    return NS_ERROR_NOT_AVAILABLE;
  *aChildCount = mChildren.Count();
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryContainerResultNode::GetChild(PRUint32 aIndex,
                                          nsINavHistoryResultNode** _retval)
{
  if (!mExpanded)
    return NS_ERROR_NOT_AVAILABLE;
  if (aIndex >= PRUint32(mChildren.Count()))
    return NS_ERROR_INVALID_ARG;
  NS_ADDREF(*_retval = mChildren[aIndex]);
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryContainerResultNode::GetChildIndex(nsINavHistoryResultNode* aNode,
                                               PRUint32* _retval)
{
  if (!mExpanded)
    return NS_ERROR_NOT_AVAILABLE;

  PRInt32 nodeIndex = FindChild(static_cast<nsNavHistoryResultNode*>(aNode));
  if (nodeIndex == -1)
    return NS_ERROR_INVALID_ARG;

  *_retval = nodeIndex;
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryContainerResultNode::FindNodeByDetails(const nsACString& aURIString,
                                                   PRTime aTime,
                                                   PRInt64 aItemId,
                                                   bool aRecursive,
                                                   nsINavHistoryResultNode** _retval) {
  if (!mExpanded)
    return NS_ERROR_NOT_AVAILABLE;

  *_retval = nsnull;
  for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
    if (mChildren[i]->mURI.Equals(aURIString) &&
        mChildren[i]->mTime == aTime &&
        mChildren[i]->mItemId == aItemId) {
      *_retval = mChildren[i];
      break;
    }

    if (aRecursive && mChildren[i]->IsContainer()) {
      nsNavHistoryContainerResultNode* asContainer =
        mChildren[i]->GetAsContainer();
      if (asContainer->mExpanded) {
        nsresult rv = asContainer->FindNodeByDetails(aURIString, aTime,
                                                     aItemId,
                                                     aRecursive,
                                                     _retval);
                                                      
        if (NS_SUCCEEDED(rv) && _retval)
          break;
      }
    }
  }
  NS_IF_ADDREF(*_retval);
  return NS_OK;
}

/**
 * @note Overridden for folders to query the bookmarks service directly.
 */
NS_IMETHODIMP
nsNavHistoryContainerResultNode::GetChildrenReadOnly(bool *aChildrenReadOnly)
{
  *aChildrenReadOnly = mChildrenReadOnly;
  return NS_OK;
}

/**
 * HOW QUERY UPDATING WORKS
 *
 * Queries are different than bookmark folders in that we can not always do
 * dynamic updates (easily) and updates are more expensive.  Therefore, we do
 * NOT query if we are not open and want to see if we have any children (for
 * drawing a twisty) and always assume we will.
 *
 * When the container is opened, we execute the query and register the
 * listeners.  Like bookmark folders, we stay registered even when closed, and
 * clear ourselves as soon as a message comes in.  This lets us respond quickly
 * if the user closes and reopens the container.
 *
 * We try to handle the most common notifications for the most common query
 * types dynamically, that is, figuring out what should happen in response to
 * a message without doing a requery.  For complex changes or complex queries,
 * we give up and requery.
 */
NS_IMPL_ISUPPORTS_INHERITED1(nsNavHistoryQueryResultNode,
                             nsNavHistoryContainerResultNode,
                             nsINavHistoryQueryResultNode)

nsNavHistoryQueryResultNode::nsNavHistoryQueryResultNode(
    const nsACString& aTitle, const nsACString& aIconURI,
    const nsACString& aQueryURI) :
  nsNavHistoryContainerResultNode(aQueryURI, aTitle, aIconURI,
                                  nsNavHistoryResultNode::RESULT_TYPE_QUERY,
                                  true, nsnull),
  mLiveUpdate(QUERYUPDATE_COMPLEX_WITH_BOOKMARKS),
  mHasSearchTerms(false),
  mContentsValid(false),
  mBatchChanges(0)
{
}

nsNavHistoryQueryResultNode::nsNavHistoryQueryResultNode(
    const nsACString& aTitle, const nsACString& aIconURI,
    const nsCOMArray<nsNavHistoryQuery>& aQueries,
    nsNavHistoryQueryOptions* aOptions) :
  nsNavHistoryContainerResultNode(EmptyCString(), aTitle, aIconURI,
                                  nsNavHistoryResultNode::RESULT_TYPE_QUERY,
                                  true, aOptions),
  mQueries(aQueries),
  mContentsValid(false),
  mBatchChanges(0)
{
  NS_ASSERTION(aQueries.Count() > 0, "Must have at least one query");

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ASSERTION(history, "History service missing");
  if (history) {
    mLiveUpdate = history->GetUpdateRequirements(mQueries, mOptions,
                                                 &mHasSearchTerms);
  }
}

nsNavHistoryQueryResultNode::nsNavHistoryQueryResultNode(
    const nsACString& aTitle, const nsACString& aIconURI,
    PRTime aTime,
    const nsCOMArray<nsNavHistoryQuery>& aQueries,
    nsNavHistoryQueryOptions* aOptions) :
  nsNavHistoryContainerResultNode(EmptyCString(), aTitle, aTime, aIconURI,
                                  nsNavHistoryResultNode::RESULT_TYPE_QUERY,
                                  true, aOptions),
  mQueries(aQueries),
  mContentsValid(false),
  mBatchChanges(0)
{
  NS_ASSERTION(aQueries.Count() > 0, "Must have at least one query");

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ASSERTION(history, "History service missing");
  if (history) {
    mLiveUpdate = history->GetUpdateRequirements(mQueries, mOptions,
                                                 &mHasSearchTerms);
  }
}

nsNavHistoryQueryResultNode::~nsNavHistoryQueryResultNode() {
  // Remove this node from result's observers.  We don't need to be notified
  // anymore.
  if (mResult && mResult->mAllBookmarksObservers.IndexOf(this) !=
                   mResult->mAllBookmarksObservers.NoIndex)
    mResult->RemoveAllBookmarksObserver(this);
  if (mResult && mResult->mHistoryObservers.IndexOf(this) !=
                   mResult->mHistoryObservers.NoIndex)
    mResult->RemoveHistoryObserver(this);
}

/**
 * Whoever made us may want non-expanding queries. However, we always expand
 * when we are the root node, or else asking for non-expanding queries would be
 * useless.  A query node is not expandable if excludeItems is set or if
 * expandQueries is unset.
 */
bool
nsNavHistoryQueryResultNode::CanExpand()
{
  if (IsContainersQuery())
    return true;

  // If ExcludeItems is set on the root or on the node itself, don't expand.
  if ((mResult && mResult->mRootNode->mOptions->ExcludeItems()) ||
      Options()->ExcludeItems())
    return false;

  // Check the ancestor container.
  nsNavHistoryQueryOptions* options = GetGeneratingOptions();
  if (options) {
    if (options->ExcludeItems())
      return false;
    if (options->ExpandQueries())
      return true;
  }

  if (mResult && mResult->mRootNode == this)
    return true;

  return false;
}


/**
 * Some query with a particular result type can contain other queries.  They
 * must be always expandable
 */
bool
nsNavHistoryQueryResultNode::IsContainersQuery()
{
  PRUint16 resultType = Options()->ResultType();
  return resultType == nsINavHistoryQueryOptions::RESULTS_AS_DATE_QUERY ||
         resultType == nsINavHistoryQueryOptions::RESULTS_AS_DATE_SITE_QUERY ||
         resultType == nsINavHistoryQueryOptions::RESULTS_AS_TAG_QUERY ||
         resultType == nsINavHistoryQueryOptions::RESULTS_AS_SITE_QUERY;
}


/**
 * Here we do not want to call ContainerResultNode::OnRemoving since our own
 * ClearChildren will do the same thing and more (unregister the observers).
 * The base ResultNode::OnRemoving will clear some regular node stats, so it
 * is OK.
 */
void
nsNavHistoryQueryResultNode::OnRemoving()
{
  nsNavHistoryResultNode::OnRemoving();
  ClearChildren(true);
}


/**
 * Marks the container as open, rebuilding results if they are invalid.  We
 * may still have valid results if the container was previously open and
 * nothing happened since closing it.
 *
 * We do not handle CloseContainer specially.  The default one just marks the
 * container as closed, but doesn't actually mark the results as invalid.
 * The results will be invalidated by the next history or bookmark
 * notification that comes in.  This means if you open and close the item
 * without anything happening in between, it will be fast (this actually
 * happens when results are used as menus).
 */
nsresult
nsNavHistoryQueryResultNode::OpenContainer()
{
  NS_ASSERTION(!mExpanded, "Container must be closed to open it");
  mExpanded = true;

  nsresult rv;

  if (!CanExpand())
    return NS_OK;
  if (!mContentsValid) {
    rv = FillChildren();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  rv = NotifyOnStateChange(STATE_CLOSED);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


/**
 * When we have valid results we can always give an exact answer.  When we
 * don't we just assume we'll have results, since actually doing the query
 * might be hard.  This is used to draw twisties on the tree, so precise results
 * don't matter.
 */
NS_IMETHODIMP
nsNavHistoryQueryResultNode::GetHasChildren(bool* aHasChildren)
{
  *aHasChildren = false;

  if (!CanExpand()) {
    return NS_OK;
  }

  PRUint16 resultType = mOptions->ResultType();

  // Tags are always populated, otherwise they are removed.
  if (resultType == nsINavHistoryQueryOptions::RESULTS_AS_TAG_CONTENTS) {
    *aHasChildren = true;
    return NS_OK;
  }

  // For tag containers query we must check if we have any tag
  if (resultType == nsINavHistoryQueryOptions::RESULTS_AS_TAG_QUERY) {
    nsCOMPtr<nsITaggingService> tagging =
      do_GetService(NS_TAGGINGSERVICE_CONTRACTID);
    if (tagging) {
      bool hasTags;
      *aHasChildren = NS_SUCCEEDED(tagging->GetHasTags(&hasTags)) && hasTags;
    }
    return NS_OK;
  }

  // For history containers query we must check if we have any history
  if (resultType == nsINavHistoryQueryOptions::RESULTS_AS_DATE_QUERY ||
      resultType == nsINavHistoryQueryOptions::RESULTS_AS_DATE_SITE_QUERY ||
      resultType == nsINavHistoryQueryOptions::RESULTS_AS_SITE_QUERY) {
    nsNavHistory* history = nsNavHistory::GetHistoryService();
    NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
    return history->GetHasHistoryEntries(aHasChildren);
  }

  //XXX: For other containers queries we must:
  // 1. If it's open, just check mChildren for containers
  // 2. Else null the view (keep it in a var), open container, check mChildren
  //    for containers, close container, reset the view

  if (mContentsValid) {
    *aHasChildren = (mChildren.Count() > 0);
    return NS_OK;
  }
  *aHasChildren = true;
  return NS_OK;
}


/**
 * This doesn't just return mURI because in the case of queries that may
 * be lazily constructed from the query objects.
 */
NS_IMETHODIMP
nsNavHistoryQueryResultNode::GetUri(nsACString& aURI)
{
  nsresult rv = VerifyQueriesSerialized();
  NS_ENSURE_SUCCESS(rv, rv);
  aURI = mURI;
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryQueryResultNode::GetFolderItemId(PRInt64* aItemId)
{
  *aItemId = mItemId;
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryQueryResultNode::GetQueries(PRUint32* queryCount,
                                        nsINavHistoryQuery*** queries)
{
  nsresult rv = VerifyQueriesParsed();
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(mQueries.Count() > 0, "Must have >= 1 query");

  *queries = static_cast<nsINavHistoryQuery**>
                        (nsMemory::Alloc(mQueries.Count() * sizeof(nsINavHistoryQuery*)));
  NS_ENSURE_TRUE(*queries, NS_ERROR_OUT_OF_MEMORY);

  for (PRInt32 i = 0; i < mQueries.Count(); ++i)
    NS_ADDREF((*queries)[i] = mQueries[i]);
  *queryCount = mQueries.Count();
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryQueryResultNode::GetQueryOptions(
                                      nsINavHistoryQueryOptions** aQueryOptions)
{
  *aQueryOptions = Options();
  NS_ADDREF(*aQueryOptions);
  return NS_OK;
}

/**
 * Safe options getter, ensures queries are parsed first.
 */
nsNavHistoryQueryOptions*
nsNavHistoryQueryResultNode::Options()
{
  nsresult rv = VerifyQueriesParsed();
  if (NS_FAILED(rv))
    return nsnull;
  NS_ASSERTION(mOptions, "Options invalid, cannot generate from URI");
  return mOptions;
}


nsresult
nsNavHistoryQueryResultNode::VerifyQueriesParsed()
{
  if (mQueries.Count() > 0) {
    NS_ASSERTION(mOptions, "If a result has queries, it also needs options");
    return NS_OK;
  }
  NS_ASSERTION(!mURI.IsEmpty(),
               "Query nodes must have either a URI or query/options");

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = history->QueryStringToQueryArray(mURI, &mQueries,
                                                 getter_AddRefs(mOptions));
  NS_ENSURE_SUCCESS(rv, rv);

  mLiveUpdate = history->GetUpdateRequirements(mQueries, mOptions,
                                               &mHasSearchTerms);
  return NS_OK;
}


nsresult
nsNavHistoryQueryResultNode::VerifyQueriesSerialized()
{
  if (!mURI.IsEmpty()) {
    return NS_OK;
  }
  NS_ASSERTION(mQueries.Count() > 0 && mOptions,
               "Query nodes must have either a URI or query/options");

  nsTArray<nsINavHistoryQuery*> flatQueries;
  flatQueries.SetCapacity(mQueries.Count());
  for (PRInt32 i = 0; i < mQueries.Count(); ++i)
    flatQueries.AppendElement(static_cast<nsINavHistoryQuery*>
                                         (mQueries.ObjectAt(i)));

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv = history->QueriesToQueryString(flatQueries.Elements(),
                                              flatQueries.Length(),
                                              mOptions, mURI);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_STATE(!mURI.IsEmpty());
  return NS_OK;
}


nsresult
nsNavHistoryQueryResultNode::FillChildren()
{
  NS_ASSERTION(!mContentsValid,
               "Don't call FillChildren when contents are valid");
  NS_ASSERTION(mChildren.Count() == 0,
               "We are trying to fill children when there already are some");

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);

  // get the results from the history service
  nsresult rv = VerifyQueriesParsed();
  NS_ENSURE_SUCCESS(rv, rv);
  rv = history->GetQueryResults(this, mQueries, mOptions, &mChildren);
  NS_ENSURE_SUCCESS(rv, rv);

  // it is important to call FillStats to fill in the parents on all
  // nodes and the result node pointers on the containers
  FillStats();

  PRUint16 sortType = GetSortType();

  if (mResult->mNeedsToApplySortingMode) {
    // We should repopulate container and then apply sortingMode.  To avoid
    // sorting 2 times we simply do that here.
    mResult->SetSortingMode(mResult->mSortingMode);
  }
  else if (mOptions->QueryType() != nsINavHistoryQueryOptions::QUERY_TYPE_HISTORY ||
           sortType != nsINavHistoryQueryOptions::SORT_BY_NONE) {
    // The default SORT_BY_NONE sorts by the bookmark index (position), 
    // which we do not have for history queries.
    // Once we've computed all tree stats, we can sort, because containers will
    // then have proper visit counts and dates.
    SortComparator comparator = GetSortingComparator(GetSortType());
    if (comparator) {
      nsCAutoString sortingAnnotation;
      GetSortingAnnotation(sortingAnnotation);
      // Usually containers queries results comes already sorted from the
      // database, but some locales could have special rules to sort by title.
      // RecursiveSort won't apply these rules to containers in containers
      // queries because when setting sortingMode on the result we want to sort
      // contained items (bug 473157).
      // Base container RecursiveSort will sort both our children and all
      // descendants, and is used in this case because we have to do manual
      // title sorting.
      // Query RecursiveSort will instead only sort descendants if we are a
      // constinaersQuery, e.g. a grouped query that will return other queries.
      // For other type of queries it will act as the base one.
      if (IsContainersQuery() &&
          sortType == mOptions->SortingMode() &&
          (sortType == nsINavHistoryQueryOptions::SORT_BY_TITLE_ASCENDING ||
           sortType == nsINavHistoryQueryOptions::SORT_BY_TITLE_DESCENDING))
        nsNavHistoryContainerResultNode::RecursiveSort(sortingAnnotation.get(), comparator);
      else
        RecursiveSort(sortingAnnotation.get(), comparator);
    }
  }

  // if we are limiting our results remove items from the end of the
  // mChildren array after sorting. This is done for root node only.
  // note, if count < max results, we won't do anything.
  if (!mParent && mOptions->MaxResults()) {
    while ((PRUint32)mChildren.Count() > mOptions->MaxResults())
      mChildren.RemoveObjectAt(mChildren.Count() - 1);
  }

  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_STATE(result);

  if (mOptions->QueryType() == nsINavHistoryQueryOptions::QUERY_TYPE_HISTORY ||
      mOptions->QueryType() == nsINavHistoryQueryOptions::QUERY_TYPE_UNIFIED) {
    // Date containers that contain site containers have no reason to observe
    // history, if the inside site container is expanded it will update,
    // otherwise we are going to refresh the parent query.
    if (!mParent || mParent->mOptions->ResultType() != nsINavHistoryQueryOptions::RESULTS_AS_DATE_SITE_QUERY) {
      // register with the result for history updates
      result->AddHistoryObserver(this);
    }
  }

  if (mOptions->QueryType() == nsINavHistoryQueryOptions::QUERY_TYPE_BOOKMARKS ||
      mOptions->QueryType() == nsINavHistoryQueryOptions::QUERY_TYPE_UNIFIED ||
      mLiveUpdate == QUERYUPDATE_COMPLEX_WITH_BOOKMARKS ||
      mHasSearchTerms) {
    // register with the result for bookmark updates
    result->AddAllBookmarksObserver(this);
  }

  mContentsValid = true;
  return NS_OK;
}


/**
 * Call with unregister = false when we are going to update the children (for
 * example, when the container is open).  This will clear the list and notify
 * all the children that they are going away.
 *
 * When the results are becoming invalid and we are not going to refresh them,
 * set unregister = true, which will unregister the listener from the
 * result if any.  We use unregister = false when we are refreshing the list
 * immediately so want to stay a notifier.
 */
void
nsNavHistoryQueryResultNode::ClearChildren(bool aUnregister)
{
  for (PRInt32 i = 0; i < mChildren.Count(); ++i)
    mChildren[i]->OnRemoving();
  mChildren.Clear();

  if (aUnregister && mContentsValid) {
    nsNavHistoryResult* result = GetResult();
    if (result) {
      result->RemoveHistoryObserver(this);
      result->RemoveAllBookmarksObserver(this);
    }
  }
  mContentsValid = false;
}


/**
 * This is called to update the result when something has changed that we
 * can not incrementally update.
 */
nsresult
nsNavHistoryQueryResultNode::Refresh()
{
  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_STATE(result);
  if (result->mBatchInProgress) {
    result->requestRefresh(this);
    return NS_OK;
  }

  // This is not a root node but it does not have a parent - this means that 
  // the node has already been cleared and it is now called, because it was 
  // left in a local copy of the observers array.
  if (mIndentLevel > -1 && !mParent)
    return NS_OK;

  // Do not refresh if we are not expanded or if we are child of a query
  // containing other queries.  In this case calling Refresh for each child
  // query could cause a major slowdown.  We should not refresh nested
  // queries, since we will already refresh the parent one.
  if (!mExpanded ||
      (mParent && mParent->IsQuery() &&
       mParent->GetAsQuery()->IsContainersQuery())) {
    // Don't update, just invalidate and unhook
    ClearChildren(true);
    return NS_OK; // no updates in tree state
  }

  if (mLiveUpdate == QUERYUPDATE_COMPLEX_WITH_BOOKMARKS)
    ClearChildren(true);
  else
    ClearChildren(false);

  // Ignore errors from FillChildren, since we will still want to refresh
  // the tree (there just might not be anything in it on error).
  (void)FillChildren();

  NOTIFY_RESULT_OBSERVERS(result, InvalidateContainer(TO_CONTAINER(this)));
  return NS_OK;
}


/**
 * Here, we override GetSortType to return the current sorting for this
 * query.  GetSortType is used when dynamically inserting query results so we
 * can see which comparator we should use to find the proper insertion point
 * (it shouldn't be called from folder containers which maintain their own
 * sorting).
 *
 * Normally, the container just forwards it up the chain.  This is what we want
 * for host groups, for example.  For queries, we often want to use the query's
 * sorting mode.
 *
 * However, we only use this query node's sorting when it is not the root.
 * When it is the root, we use the result's sorting mode.  This is because
 * there are two cases:
 *   - You are looking at a bookmark hierarchy that contains an embedded
 *     result.  We should always use the query's sort ordering since the result
 *     node's headers have nothing to do with us (and are disabled).
 *   - You are looking at a query in the tree.  In this case, we want the
 *     result sorting to override ours (it should be initialized to the same
 *     sorting mode).
 */
PRUint16
nsNavHistoryQueryResultNode::GetSortType()
{
  if (mParent)
    return mOptions->SortingMode();
  if (mResult)
    return mResult->mSortingMode;

  NS_NOTREACHED("We should always have a result");
  return nsINavHistoryQueryOptions::SORT_BY_NONE;
}


void
nsNavHistoryQueryResultNode::GetSortingAnnotation(nsACString& aAnnotation) {
  if (mParent) {
    // use our sorting, we are not the root
    mOptions->GetSortingAnnotation(aAnnotation);
  }
  else if (mResult) {
    aAnnotation.Assign(mResult->mSortingAnnotation);
  }
  else
    NS_NOTREACHED("We should always have a result");
}

void
nsNavHistoryQueryResultNode::RecursiveSort(
    const char* aData, SortComparator aComparator)
{
  void* data = const_cast<void*>(static_cast<const void*>(aData));

  if (!IsContainersQuery())
    mChildren.Sort(aComparator, data);

  for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
    if (mChildren[i]->IsContainer())
      mChildren[i]->GetAsContainer()->RecursiveSort(aData, aComparator);
  }
}


NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnBeginUpdateBatch()
{
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnEndUpdateBatch()
{
  // If the query has no children it's possible it's not yet listening to
  // bookmarks changes, in such a case it's safer to force a refresh to gather
  // eventual new nodes matching query options.
  if (mChildren.Count() == 0) {
    nsresult rv = Refresh();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mBatchChanges = 0;
  return NS_OK;
}


/**
 * Here we need to update all copies of the URI we have with the new visit
 * count, and potentially add a new entry in our query.  This is the most
 * common update operation and it is important that it be as efficient as
 * possible.
 */
NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnVisit(nsIURI* aURI, PRInt64 aVisitId,
                                     PRTime aTime, PRInt64 aSessionId,
                                     PRInt64 aReferringId,
                                     PRUint32 aTransitionType,
                                     const nsACString& aGUID,
                                     PRUint32* aAdded)
{
  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_STATE(result);
  if (result->mBatchInProgress &&
      ++mBatchChanges > MAX_BATCH_CHANGES_BEFORE_REFRESH) {
    nsresult rv = Refresh();
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);

  nsresult rv;
  nsRefPtr<nsNavHistoryResultNode> addition;
  switch(mLiveUpdate) {

    case QUERYUPDATE_HOST: {
      // For these simple yet common cases we can check the host ourselves
      // before doing the overhead of creating a new result node.
      NS_ASSERTION(mQueries.Count() == 1, 
          "Host updated queries can have only one object");
      nsCOMPtr<nsNavHistoryQuery> queryHost = 
          do_QueryInterface(mQueries[0], &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      bool hasDomain;
      queryHost->GetHasDomain(&hasDomain);
      if (!hasDomain)
        return NS_OK;

      nsCAutoString host;
      if (NS_FAILED(aURI->GetAsciiHost(host)))
        return NS_OK;

      if (!queryHost->Domain().Equals(host))
        return NS_OK;

    } // Let it fall through - we want to check the time too,
      // if the time is not present it will match too.
    case QUERYUPDATE_TIME: {
      // For these simple yet common cases we can check the time ourselves
      // before doing the overhead of creating a new result node.
      NS_ASSERTION(mQueries.Count() == 1, 
          "Time updated queries can have only one object");
      nsCOMPtr<nsNavHistoryQuery> query = 
          do_QueryInterface(mQueries[0], &rv);
      NS_ENSURE_SUCCESS(rv, rv);

      bool hasIt;
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
      // Now we know that our visit satisfies the time range, fallback to the
      // QUERYUPDATE_SIMPLE case.
    }
    case QUERYUPDATE_SIMPLE: {
      // The history service can tell us whether the new item should appear
      // in the result.  We first have to construct a node for it to check.
      rv = history->VisitIdToResultNode(aVisitId, mOptions,
                                        getter_AddRefs(addition));
      if (NS_FAILED(rv) || !addition ||
          !history->EvaluateQueryForNode(mQueries, mOptions, addition))
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

  // NOTE: The dynamic updating never deletes any nodes.  Sometimes it replaces
  // URI nodes or adds visits, but never deletes old ones.
  //
  // The only time this might happen in the current implementation is if the
  // title changes and it no longer matches a keyword search.  This is not
  // important enough to handle given that we don't do any other deleting.
  // It is arguably more useful behavior anyway, since you're searching your
  // history and the page used to match.
  //
  // When more queries are possible (show pages I've visited less than 5 times)
  // this will be important to add.

  nsCOMArray<nsNavHistoryResultNode> mergerNode;

  if (!mergerNode.AppendObject(addition))
    return NS_ERROR_OUT_OF_MEMORY;

  MergeResults(&mergerNode);

  if (aAdded)
    ++(*aAdded);

  return NS_OK;
}


/**
 * Find every node that matches this URI and rename it.  We try to do
 * incremental updates here, even when we are closed, because changing titles
 * is easier than requerying if we are invalid.
 *
 * This actually gets called a lot.  Typically, we will get an AddURI message
 * when the user visits the page, and then the title will be set asynchronously
 * when the title element of the page is parsed.
 */
NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnTitleChanged(nsIURI* aURI,
                                            const nsAString& aPageTitle,
                                            const nsACString& aGUID)
{
  if (!mExpanded) {
    // When we are not expanded, we don't update, just invalidate and unhook.
    // It would still be pretty easy to traverse the results and update the
    // titles, but when a title changes, its unlikely that it will be the only
    // thing.  Therefore, we just give up.
    ClearChildren(true);
    return NS_OK; // no updates in tree state
  }

  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_STATE(result);
  if (result->mBatchInProgress &&
      ++mBatchChanges > MAX_BATCH_CHANGES_BEFORE_REFRESH) {
    nsresult rv = Refresh();
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  // compute what the new title should be
  NS_ConvertUTF16toUTF8 newTitle(aPageTitle);

  bool onlyOneEntry = (mOptions->ResultType() ==
                         nsINavHistoryQueryOptions::RESULTS_AS_URI ||
                         mOptions->ResultType() ==
                         nsINavHistoryQueryOptions::RESULTS_AS_TAG_CONTENTS
                         );

  // See if our queries have any search term matching.
  if (mHasSearchTerms) {
    // Find all matching URI nodes.
    nsCOMArray<nsNavHistoryResultNode> matches;
    nsCAutoString spec;
    nsresult rv = aURI->GetSpec(spec);
    NS_ENSURE_SUCCESS(rv, rv);
    RecursiveFindURIs(onlyOneEntry, this, spec, &matches);
    if (matches.Count() == 0) {
      // This could be a new node matching the query, thus we could need
      // to add it to the result.
      nsRefPtr<nsNavHistoryResultNode> node;
      nsNavHistory* history = nsNavHistory::GetHistoryService();
      NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
      rv = history->URIToResultNode(aURI, mOptions, getter_AddRefs(node));
      NS_ENSURE_SUCCESS(rv, rv);
      if (history->EvaluateQueryForNode(mQueries, mOptions, node)) {
        rv = InsertSortedChild(node, true);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
    for (PRInt32 i = 0; i < matches.Count(); ++i) {
      // For each matched node we check if it passes the query filter, if not
      // we remove the node from the result, otherwise we'll update the title
      // later.
      nsNavHistoryResultNode* node = matches[i];
      // We must check the node with the new title.
      node->mTitle = newTitle;

      nsNavHistory* history = nsNavHistory::GetHistoryService();
      NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
      if (!history->EvaluateQueryForNode(mQueries, mOptions, node)) {
        nsNavHistoryContainerResultNode* parent = node->mParent;
        // URI nodes should always have parents
        NS_ENSURE_TRUE(parent, NS_ERROR_UNEXPECTED);
        PRInt32 childIndex = parent->FindChild(node);
        NS_ASSERTION(childIndex >= 0, "Child not found in parent");
        parent->RemoveChildAt(childIndex);
      }
    }
  }

  return ChangeTitles(aURI, newTitle, true, onlyOneEntry);
}


NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnBeforeDeleteURI(nsIURI* aURI,
                                               const nsACString& aGUID,
                                               PRUint16 aReason)
{
  return NS_OK;
}

/**
 * Here, we can always live update by just deleting all occurrences of
 * the given URI.
 */
NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnDeleteURI(nsIURI* aURI,
                                         const nsACString& aGUID,
                                         PRUint16 aReason)
{
  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_STATE(result);
  if (result->mBatchInProgress &&
      ++mBatchChanges > MAX_BATCH_CHANGES_BEFORE_REFRESH) {
    nsresult rv = Refresh();
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  if (IsContainersQuery()) {
    // Incremental updates of query returning queries are pretty much
    // complicated.  In this case it's possible one of the child queries has
    // no more children and it should be removed.  Unfortunately there is no
    // way to know that without executing the child query and counting results.
    nsresult rv = Refresh();
    NS_ENSURE_SUCCESS(rv, rv);
    return NS_OK;
  }

  bool onlyOneEntry = (mOptions->ResultType() ==
                         nsINavHistoryQueryOptions::RESULTS_AS_URI ||
                         mOptions->ResultType() ==
                         nsINavHistoryQueryOptions::RESULTS_AS_TAG_CONTENTS);
  nsCAutoString spec;
  nsresult rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMArray<nsNavHistoryResultNode> matches;
  RecursiveFindURIs(onlyOneEntry, this, spec, &matches);
  for (PRInt32 i = 0; i < matches.Count(); ++i) {
    nsNavHistoryResultNode* node = matches[i];
    nsNavHistoryContainerResultNode* parent = node->mParent;
    // URI nodes should always have parents
    NS_ENSURE_TRUE(parent, NS_ERROR_UNEXPECTED);

    PRInt32 childIndex = parent->FindChild(node);
    NS_ASSERTION(childIndex >= 0, "Child not found in parent");
    parent->RemoveChildAt(childIndex);
    if (parent->mChildren.Count() == 0 && parent->IsQuery() &&
        parent->mIndentLevel > -1) {
      // When query subcontainers (like hosts) get empty we should remove them
      // as well.  If the parent is not the root node, append it to our list
      // and it will get evaluated later in the loop.
      matches.AppendObject(parent);
    }
  }
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnClearHistory()
{
  nsresult rv = Refresh();
  NS_ENSURE_SUCCESS(rv, rv);
  return NS_OK;
}


static nsresult setFaviconCallback(
   nsNavHistoryResultNode* aNode, void* aClosure,
   nsNavHistoryResult* aResult)
{
  const nsCString* newFavicon = static_cast<nsCString*>(aClosure);
  aNode->mFaviconURI = *newFavicon;

  if (aResult && (!aNode->mParent || aNode->mParent->AreChildrenVisible()))
    NOTIFY_RESULT_OBSERVERS(aResult, NodeIconChanged(aNode));

  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnPageChanged(nsIURI* aURI,
                                           PRUint32 aChangedAttribute,
                                           const nsAString& aNewValue,
                                           const nsACString& aGUID)
{
  nsCAutoString spec;
  nsresult rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  switch (aChangedAttribute) {
    case nsINavHistoryObserver::ATTRIBUTE_FAVICON: {
      NS_ConvertUTF16toUTF8 newFavicon(aNewValue);
      bool onlyOneEntry = (mOptions->ResultType() ==
                             nsINavHistoryQueryOptions::RESULTS_AS_URI ||
                             mOptions->ResultType() ==
                             nsINavHistoryQueryOptions::RESULTS_AS_TAG_CONTENTS);
      rv = UpdateURIs(true, onlyOneEntry, false, spec, setFaviconCallback,
                      &newFavicon);
      NS_ENSURE_SUCCESS(rv, rv);
      break;
    }
    default:
      NS_WARNING("Unknown page changed notification");
  }
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnDeleteVisits(nsIURI* aURI,
                                            PRTime aVisitTime,
                                            const nsACString& aGUID,
                                            PRUint16 aReason)
{
  NS_PRECONDITION(mOptions->QueryType() == nsINavHistoryQueryOptions::QUERY_TYPE_HISTORY,
                  "Bookmarks queries should not get a OnDeleteVisits notification");
  if (aVisitTime == 0) {
    // All visits for this uri have been removed, but the uri won't be removed
    // from the databse, most likely because it's a bookmark.  For a history
    // query this is equivalent to a onDeleteURI notification.
    nsresult rv = OnDeleteURI(aURI, aGUID, aReason);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return NS_OK;
}

nsresult
nsNavHistoryQueryResultNode::NotifyIfTagsChanged(nsIURI* aURI)
{
  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_STATE(result);
  nsCAutoString spec;
  nsresult rv = aURI->GetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);
  bool onlyOneEntry = (mOptions->ResultType() ==
                         nsINavHistoryQueryOptions::RESULTS_AS_URI ||
                         mOptions->ResultType() ==
                         nsINavHistoryQueryOptions::RESULTS_AS_TAG_CONTENTS
                         );

  // Find matching URI nodes.
  nsRefPtr<nsNavHistoryResultNode> node;
  nsNavHistory* history = nsNavHistory::GetHistoryService();

  nsCOMArray<nsNavHistoryResultNode> matches;
  RecursiveFindURIs(onlyOneEntry, this, spec, &matches);

  if (matches.Count() == 0 && mHasSearchTerms && !mRemovingURI) {
    // A new tag has been added, it's possible it matches our query.
    NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
    rv = history->URIToResultNode(aURI, mOptions, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);
    if (history->EvaluateQueryForNode(mQueries, mOptions, node)) {
      rv = InsertSortedChild(node, true);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  for (PRInt32 i = 0; i < matches.Count(); ++i) {
    nsNavHistoryResultNode* node = matches[i];
    // Force a tags update before checking the node.
    node->mTags.SetIsVoid(true);
    nsAutoString tags;
    rv = node->GetTags(tags);
    NS_ENSURE_SUCCESS(rv, rv);
    // It's possible now this node does not respect anymore the conditions.
    // In such a case it should be removed.
    if (mHasSearchTerms &&
        !history->EvaluateQueryForNode(mQueries, mOptions, node)) {
      nsNavHistoryContainerResultNode* parent = node->mParent;
      // URI nodes should always have parents
      NS_ENSURE_TRUE(parent, NS_ERROR_UNEXPECTED);
      PRInt32 childIndex = parent->FindChild(node);
      NS_ASSERTION(childIndex >= 0, "Child not found in parent");
      parent->RemoveChildAt(childIndex);
    }
    else {
      NOTIFY_RESULT_OBSERVERS(result, NodeTagsChanged(node));
    }
  }

  return NS_OK;
}

/**
 * These are the bookmark observer functions for query nodes.  They listen
 * for bookmark events and refresh the results if we have any dependence on
 * the bookmark system.
 */
NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnItemAdded(PRInt64 aItemId,
                                         PRInt64 aParentId,
                                         PRInt32 aIndex,
                                         PRUint16 aItemType,
                                         nsIURI* aURI,
                                         const nsACString& aTitle,
                                         PRTime aDateAdded,
                                         const nsACString& aGUID,
                                         const nsACString& aParentGUID)
{
  if (aItemType == nsINavBookmarksService::TYPE_BOOKMARK &&
      mLiveUpdate != QUERYUPDATE_SIMPLE &&  mLiveUpdate != QUERYUPDATE_TIME) {
    nsresult rv = Refresh();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnBeforeItemRemoved(PRInt64 aItemId,
                                                 PRUint16 aItemType,
                                                 PRInt64 aParentId,
                                                 const nsACString& aGUID,
                                                 const nsACString& aParentGUID)
{
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnItemRemoved(PRInt64 aItemId,
                                           PRInt64 aParentId,
                                           PRInt32 aIndex,
                                           PRUint16 aItemType,
                                           nsIURI* aURI,
                                           const nsACString& aGUID,
                                           const nsACString& aParentGUID)
{
  mRemovingURI = aURI;
  if (aItemType == nsINavBookmarksService::TYPE_BOOKMARK &&
      mLiveUpdate != QUERYUPDATE_SIMPLE && mLiveUpdate != QUERYUPDATE_TIME) {
    nsresult rv = Refresh();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnItemChanged(PRInt64 aItemId,
                                           const nsACString& aProperty,
                                           bool aIsAnnotationProperty,
                                           const nsACString& aNewValue,
                                           PRTime aLastModified,
                                           PRUint16 aItemType,
                                           PRInt64 aParentId,
                                           const nsACString& aGUID,
                                           const nsACString& aParentGUID)
{
  // History observers should not get OnItemChanged
  // but should get the corresponding history notifications instead.
  // For bookmark queries, "all bookmark" observers should get OnItemChanged.
  // For example, when a title of a bookmark changes, we want that to refresh.

  if (mLiveUpdate == QUERYUPDATE_COMPLEX_WITH_BOOKMARKS) {
    switch (aItemType) {
      case nsINavBookmarksService::TYPE_SEPARATOR:
        // No separators in queries.
        return NS_OK;
      case nsINavBookmarksService::TYPE_FOLDER:
        // Queries never result as "folders", but the tags-query results as
        // special "tag" containers, which should follow their corresponding
        // folders titles.
        if (mOptions->ResultType() != nsINavHistoryQueryOptions::RESULTS_AS_TAG_QUERY)
          return NS_OK;
      default:
        (void)Refresh();
    }
  }
  else {
    // Some node could observe both bookmarks and history.  But a node observing
    // only history should never get a bookmark notification.
    NS_WARN_IF_FALSE(mResult && (mResult->mIsAllBookmarksObserver || mResult->mIsBookmarkFolderObserver),
                     "history observers should not get OnItemChanged, but should get the corresponding history notifications instead");

    // Tags in history queries are a special case since tags are per uri and
    // we filter tags based on searchterms.
    if (aItemType == nsINavBookmarksService::TYPE_BOOKMARK &&
        aProperty.EqualsLiteral("tags")) {
      nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
      NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);
      nsCOMPtr<nsIURI> uri;
      nsresult rv = bookmarks->GetBookmarkURI(aItemId, getter_AddRefs(uri));
      NS_ENSURE_SUCCESS(rv, rv);
      rv = NotifyIfTagsChanged(uri);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return nsNavHistoryResultNode::OnItemChanged(aItemId, aProperty,
                                               aIsAnnotationProperty,
                                               aNewValue, aLastModified,
                                               aItemType, aParentId, aGUID,
                                               aParentGUID);
}

NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnItemVisited(PRInt64 aItemId,
                                           PRInt64 aVisitId,
                                           PRTime aTime,
                                           PRUint32 aTransitionType,
                                           nsIURI* aURI,
                                           PRInt64 aParentId,
                                           const nsACString& aGUID,
                                           const nsACString& aParentGUID)
{
  // for bookmark queries, "all bookmark" observer should get OnItemVisited
  // but it is ignored.
  if (mLiveUpdate != QUERYUPDATE_COMPLEX_WITH_BOOKMARKS)
    NS_WARN_IF_FALSE(mResult && (mResult->mIsAllBookmarksObserver || mResult->mIsBookmarkFolderObserver),
                     "history observers should not get OnItemVisited, but should get OnVisit instead");
  return NS_OK;
}

NS_IMETHODIMP
nsNavHistoryQueryResultNode::OnItemMoved(PRInt64 aFolder,
                                         PRInt64 aOldParent,
                                         PRInt32 aOldIndex,
                                         PRInt64 aNewParent,
                                         PRInt32 aNewIndex,
                                         PRUint16 aItemType,
                                         const nsACString& aGUID,
                                         const nsACString& aOldParentGUID,
                                         const nsACString& aNewParentGUID)
{
  // 1. The query cannot be affected by the item's position
  // 2. For the time being, we cannot optimize this not to update
  //    queries which are not restricted to some folders, due to way
  //    sub-queries are updated (see Refresh)
  if (mLiveUpdate == QUERYUPDATE_COMPLEX_WITH_BOOKMARKS &&
      aItemType != nsINavBookmarksService::TYPE_SEPARATOR &&
      aOldParent != aNewParent) {
    return Refresh();
  }
  return NS_OK;
}

/**
 * HOW DYNAMIC FOLDER UPDATING WORKS
 *
 * When you create a result, it will automatically keep itself in sync with
 * stuff that happens in the system.  For folder nodes, this means changes to
 * bookmarks.
 *
 * A folder will fill its children "when necessary." This means it is being
 * opened or whether we need to see if it is empty for twisty drawing.  It will
 * then register its ID with the main result object that owns it.  This result
 * object will listen for all bookmark notifications and pass those
 * notifications to folder nodes that have registered for that specific folder
 * ID.
 *
 * When a bookmark folder is closed, it will not clear its children.  Instead,
 * it will keep them and also stay registered as a listener.  This means that
 * you can more quickly re-open the same folder without doing any work.  This
 * happens a lot for menus, and bookmarks don't change very often.
 *
 * When a message comes in and the folder is open, we will do the correct
 * operations to keep ourselves in sync with the bookmark service.  If the
 * folder is closed, we just clear our list to mark it as invalid and
 * unregister as a listener.  This means we do not have to keep maintaining
 * an up-to-date list for the entire bookmark menu structure in every place
 * it is used.
 */
NS_IMPL_ISUPPORTS_INHERITED1(nsNavHistoryFolderResultNode,
                             nsNavHistoryContainerResultNode,
                             nsINavHistoryQueryResultNode)

nsNavHistoryFolderResultNode::nsNavHistoryFolderResultNode(
    const nsACString& aTitle, nsNavHistoryQueryOptions* aOptions,
    PRInt64 aFolderId) :
  nsNavHistoryContainerResultNode(EmptyCString(), aTitle, EmptyCString(),
                                  nsNavHistoryResultNode::RESULT_TYPE_FOLDER,
                                  false, aOptions),
  mContentsValid(false),
  mQueryItemId(-1),
  mIsRegisteredFolderObserver(false)
{
  mItemId = aFolderId;
}

nsNavHistoryFolderResultNode::~nsNavHistoryFolderResultNode()
{
  if (mIsRegisteredFolderObserver && mResult)
    mResult->RemoveBookmarkFolderObserver(this, mItemId);
}


/**
 * Here we do not want to call ContainerResultNode::OnRemoving since our own
 * ClearChildren will do the same thing and more (unregister the observers).
 * The base ResultNode::OnRemoving will clear some regular node stats, so it is
 * OK.
 */
void
nsNavHistoryFolderResultNode::OnRemoving()
{
  nsNavHistoryResultNode::OnRemoving();
  ClearChildren(true);
}


nsresult
nsNavHistoryFolderResultNode::OpenContainer()
{
  NS_ASSERTION(!mExpanded, "Container must be expanded to close it");
  nsresult rv;

  if (!mContentsValid) {
    rv = FillChildren();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  mExpanded = true;

  rv = NotifyOnStateChange(STATE_CLOSED);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


/**
 * The async version of OpenContainer.
 */
nsresult
nsNavHistoryFolderResultNode::OpenContainerAsync()
{
  NS_ASSERTION(!mExpanded, "Container already expanded when opening it");

  // If the children are valid, open the container synchronously.  This will be
  // the case when the container has already been opened and any other time
  // FillChildren or FillChildrenAsync has previously been called.
  if (mContentsValid)
    return OpenContainer();

  nsresult rv = FillChildrenAsync();
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NotifyOnStateChange(STATE_CLOSED);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}


/**
 * @see nsNavHistoryQueryResultNode::HasChildren.  The semantics here are a
 * little different.  Querying the contents of a bookmark folder is relatively
 * fast and it is common to have empty folders.  Therefore, we always want to
 * return the correct result so that twisties are drawn properly.
 */
NS_IMETHODIMP
nsNavHistoryFolderResultNode::GetHasChildren(bool* aHasChildren)
{
  if (!mContentsValid) {
    nsresult rv = FillChildren();
    NS_ENSURE_SUCCESS(rv, rv);
  }
  *aHasChildren = (mChildren.Count() > 0);
  return NS_OK;
}

/**
 * @return the id of the item from which the folder node was generated, it
 * could be either a concrete folder-itemId or the id used in a
 * simple-folder-query-bookmark (place:folder=X).
 */
NS_IMETHODIMP
nsNavHistoryFolderResultNode::GetItemId(PRInt64* aItemId)
{
  *aItemId = mQueryItemId == -1 ? mItemId : mQueryItemId;
  return NS_OK;
}

/**
 * Here, we override the getter and ignore the value stored in our object.
 * The bookmarks service can tell us whether this folder should be read-only
 * or not.
 *
 * It would be nice to put this code in the folder constructor, but the
 * database was complaining.  I believe it is because most folders are created
 * while enumerating the bookmarks table and having a statement open, and doing
 * another statement might make it unhappy in some cases.
 */
NS_IMETHODIMP
nsNavHistoryFolderResultNode::GetChildrenReadOnly(bool *aChildrenReadOnly)
{
  nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
  NS_ENSURE_TRUE(bookmarks, NS_ERROR_UNEXPECTED);
  return bookmarks->GetFolderReadonly(mItemId, aChildrenReadOnly);
}


NS_IMETHODIMP
nsNavHistoryFolderResultNode::GetFolderItemId(PRInt64* aItemId)
{
  *aItemId = mItemId;
  return NS_OK;
}

/**
 * Lazily computes the URI for this specific folder query with the current
 * options.
 */
NS_IMETHODIMP
nsNavHistoryFolderResultNode::GetUri(nsACString& aURI)
{
  if (!mURI.IsEmpty()) {
    aURI = mURI;
    return NS_OK;
  }

  PRUint32 queryCount;
  nsINavHistoryQuery** queries;
  nsresult rv = GetQueries(&queryCount, &queries);
  NS_ENSURE_SUCCESS(rv, rv);

  nsNavHistory* history = nsNavHistory::GetHistoryService();
  NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);

  rv = history->QueriesToQueryString(queries, queryCount, mOptions, aURI);
  for (PRUint32 queryIndex = 0; queryIndex < queryCount; ++queryIndex) {
    NS_RELEASE(queries[queryIndex]);
  }
  nsMemory::Free(queries);
  return rv;
}


/**
 * @return the queries that give you this bookmarks folder
 */
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
  rv = query->SetFolders(&mItemId, 1);
  NS_ENSURE_SUCCESS(rv, rv);

  // make array of our 1 query
  *queries = static_cast<nsINavHistoryQuery**>
                        (nsMemory::Alloc(sizeof(nsINavHistoryQuery*)));
  if (!*queries)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF((*queries)[0] = query);
  *queryCount = 1;
  return NS_OK;
}


/**
 * Options for the query that gives you this bookmarks folder.  This is just
 * the options for the folder with the current folder ID set.
 */
NS_IMETHODIMP
nsNavHistoryFolderResultNode::GetQueryOptions(
                                      nsINavHistoryQueryOptions** aQueryOptions)
{
  NS_ASSERTION(mOptions, "Options invalid");

  *aQueryOptions = mOptions;
  NS_ADDREF(*aQueryOptions);
  return NS_OK;
}


nsresult
nsNavHistoryFolderResultNode::FillChildren()
{
  NS_ASSERTION(!mContentsValid,
               "Don't call FillChildren when contents are valid");
  NS_ASSERTION(mChildren.Count() == 0,
               "We are trying to fill children when there already are some");

  nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
  NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);

  // Actually get the folder children from the bookmark service.
  nsresult rv = bookmarks->QueryFolderChildren(mItemId, mOptions, &mChildren);
  NS_ENSURE_SUCCESS(rv, rv);

  // PERFORMANCE: it may be better to also fill any child folders at this point
  // so that we can draw tree twisties without doing a separate query later.
  // If we don't end up drawing twisties a lot, it doesn't matter. If we do
  // this, we should wrap everything in a transaction here on the bookmark
  // service's connection.

  return OnChildrenFilled();
}


/**
 * Performs some tasks after all the children of the container have been added.
 * The container's contents are not valid until this method has been called.
 */
nsresult
nsNavHistoryFolderResultNode::OnChildrenFilled()
{
  // It is important to call FillStats to fill in the parents on all
  // nodes and the result node pointers on the containers.
  FillStats();

  if (mResult->mNeedsToApplySortingMode) {
    // We should repopulate container and then apply sortingMode.  To avoid
    // sorting 2 times we simply do that here.
    mResult->SetSortingMode(mResult->mSortingMode);
  }
  else {
    // Once we've computed all tree stats, we can sort, because containers will
    // then have proper visit counts and dates.
    SortComparator comparator = GetSortingComparator(GetSortType());
    if (comparator) {
      nsCAutoString sortingAnnotation;
      GetSortingAnnotation(sortingAnnotation);
      RecursiveSort(sortingAnnotation.get(), comparator);
    }
  }

  // If we are limiting our results remove items from the end of the
  // mChildren array after sorting.  This is done for root node only.
  // Note, if count < max results, we won't do anything.
  if (!mParent && mOptions->MaxResults()) {
    while ((PRUint32)mChildren.Count() > mOptions->MaxResults())
      mChildren.RemoveObjectAt(mChildren.Count() - 1);
  }

  // Register with the result for updates.
  EnsureRegisteredAsFolderObserver();

  mContentsValid = true;
  return NS_OK;
}


/**
 * Registers the node with its result as a folder observer if it is not already
 * registered.
 */
void
nsNavHistoryFolderResultNode::EnsureRegisteredAsFolderObserver()
{
  if (!mIsRegisteredFolderObserver && mResult) {
    mResult->AddBookmarkFolderObserver(this, mItemId);
    mIsRegisteredFolderObserver = true;
  }
}


/**
 * The async version of FillChildren.  This begins asynchronous execution by
 * calling nsNavBookmarks::QueryFolderChildrenAsync.  During execution, this
 * node's async Storage callbacks, HandleResult and HandleCompletion, will be
 * called.
 */
nsresult
nsNavHistoryFolderResultNode::FillChildrenAsync()
{
  NS_ASSERTION(!mContentsValid, "FillChildrenAsync when contents are valid");
  NS_ASSERTION(mChildren.Count() == 0, "FillChildrenAsync when children exist");

  // ProcessFolderNodeChild, called in HandleResult, increments this for every
  // result row it processes.  Initialize it here as we begin async execution.
  mAsyncBookmarkIndex = -1;

  nsNavBookmarks* bmSvc = nsNavBookmarks::GetBookmarksService();
  NS_ENSURE_TRUE(bmSvc, NS_ERROR_OUT_OF_MEMORY);
  nsresult rv =
    bmSvc->QueryFolderChildrenAsync(this, mItemId,
                                    getter_AddRefs(mAsyncPendingStmt));
  NS_ENSURE_SUCCESS(rv, rv);

  // Register with the result for updates.  All updates during async execution
  // will cause it to be restarted.
  EnsureRegisteredAsFolderObserver();

  return NS_OK;
}


/**
 * A mozIStorageStatementCallback method.  Called during the async execution
 * begun by FillChildrenAsync.
 *
 * @param aResultSet
 *        The result set containing the data from the database.
 */
NS_IMETHODIMP
nsNavHistoryFolderResultNode::HandleResult(mozIStorageResultSet* aResultSet)
{
  NS_ENSURE_ARG_POINTER(aResultSet);

  nsNavBookmarks* bmSvc = nsNavBookmarks::GetBookmarksService();
  if (!bmSvc) {
    CancelAsyncOpen(false);
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // Consume all the currently available rows of the result set.
  nsCOMPtr<mozIStorageRow> row;
  while (NS_SUCCEEDED(aResultSet->GetNextRow(getter_AddRefs(row))) && row) {
    nsresult rv = bmSvc->ProcessFolderNodeRow(row, mOptions, &mChildren,
                                              mAsyncBookmarkIndex);
    if (NS_FAILED(rv)) {
      CancelAsyncOpen(false);
      return rv;
    }
  }

  return NS_OK;
}


/**
 * A mozIStorageStatementCallback method.  Called during the async execution
 * begun by FillChildrenAsync.
 *
 * @param aReason
 *        Indicates the final state of execution.
 */
NS_IMETHODIMP
nsNavHistoryFolderResultNode::HandleCompletion(PRUint16 aReason)
{
  if (aReason == mozIStorageStatementCallback::REASON_FINISHED &&
      mAsyncCanceledState == NOT_CANCELED) {
    // Async execution successfully completed.  The container is ready to open.

    nsresult rv = OnChildrenFilled();
    NS_ENSURE_SUCCESS(rv, rv);

    mExpanded = true;
    mAsyncPendingStmt = nsnull;

    // Notify observers only after mExpanded and mAsyncPendingStmt are set.
    rv = NotifyOnStateChange(STATE_LOADING);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  else if (mAsyncCanceledState == CANCELED_RESTART_NEEDED) {
    // Async execution was canceled and needs to be restarted.
    mAsyncCanceledState = NOT_CANCELED;
    ClearChildren(false);
    FillChildrenAsync();
  }

  else {
    // Async execution failed or was canceled without restart.  Remove all
    // children and close the container, notifying observers.
    mAsyncCanceledState = NOT_CANCELED;
    ClearChildren(true);
    CloseContainer();
  }

  return NS_OK;
}


void
nsNavHistoryFolderResultNode::ClearChildren(bool unregister)
{
  for (PRInt32 i = 0; i < mChildren.Count(); ++i)
    mChildren[i]->OnRemoving();
  mChildren.Clear();

  bool needsUnregister = unregister && (mContentsValid || mAsyncPendingStmt);
  if (needsUnregister && mResult && mIsRegisteredFolderObserver) {
    mResult->RemoveBookmarkFolderObserver(this, mItemId);
    mIsRegisteredFolderObserver = false;
  }
  mContentsValid = false;
}


/**
 * This is called to update the result when something has changed that we
 * can not incrementally update.
 */
nsresult
nsNavHistoryFolderResultNode::Refresh()
{
  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_STATE(result);
  if (result->mBatchInProgress) {
    result->requestRefresh(this);
    return NS_OK;
  }

  ClearChildren(true);

  if (!mExpanded) {
    // When we are not expanded, we don't update, just invalidate and unhook.
    return NS_OK;
  }

  // Ignore errors from FillChildren, since we will still want to refresh
  // the tree (there just might not be anything in it on error).  ClearChildren
  // has unregistered us as an observer since FillChildren will try to
  // re-register us.
  (void)FillChildren();

  NOTIFY_RESULT_OBSERVERS(result, InvalidateContainer(TO_CONTAINER(this)));
  return NS_OK;
}


/**
 * Implements the logic described above the constructor.  This sees if we
 * should do an incremental update and returns true if so.  If not, it
 * invalidates our children, unregisters us an observer, and returns false.
 */
bool
nsNavHistoryFolderResultNode::StartIncrementalUpdate()
{
  // if any items are excluded, we can not do incremental updates since the
  // indices from the bookmark service will not be valid

  if (!mOptions->ExcludeItems() &&
      !mOptions->ExcludeQueries() &&
      !mOptions->ExcludeReadOnlyFolders()) {
    // easy case: we are visible, always do incremental update
    if (mExpanded || AreChildrenVisible())
      return true;

    nsNavHistoryResult* result = GetResult();
    NS_ENSURE_TRUE(result, false);

    // When any observers are attached also do incremental updates if our
    // parent is visible, so that twisties are drawn correctly.
    if (mParent)
      return result->mObservers.Length() > 0;
  }

  // otherwise, we don't do incremental updates, invalidate and unregister
  (void)Refresh();
  return false;
}


/**
 * This function adds aDelta to all bookmark indices between the two endpoints,
 * inclusive.  It is used when items are added or removed from the bookmark
 * folder.
 */
void
nsNavHistoryFolderResultNode::ReindexRange(PRInt32 aStartIndex,
                                           PRInt32 aEndIndex,
                                           PRInt32 aDelta)
{
  for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
    nsNavHistoryResultNode* node = mChildren[i];
    if (node->mBookmarkIndex >= aStartIndex &&
        node->mBookmarkIndex <= aEndIndex)
      node->mBookmarkIndex += aDelta;
  }
}


/**
 * Searches this folder for a node with the given id.
 *
 * @return the node if found, null otherwise.
 * @note Does not addref the node!
 */
nsNavHistoryResultNode*
nsNavHistoryFolderResultNode::FindChildById(PRInt64 aItemId,
    PRUint32* aNodeIndex)
{
  for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
    if (mChildren[i]->mItemId == aItemId ||
        (mChildren[i]->IsFolder() &&
         mChildren[i]->GetAsFolder()->mQueryItemId == aItemId)) {
      *aNodeIndex = i;
      return mChildren[i];
    }
  }
  return nsnull;
}


// Used by nsNavHistoryFolderResultNode's nsINavBookmarkObserver methods below.
// If the container is notified of a bookmark event while asynchronous execution
// is pending, this restarts it and returns.
#define RESTART_AND_RETURN_IF_ASYNC_PENDING() \
  if (mAsyncPendingStmt) { \
    CancelAsyncOpen(true); \
    return NS_OK; \
  }


NS_IMETHODIMP
nsNavHistoryFolderResultNode::OnBeginUpdateBatch()
{
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryFolderResultNode::OnEndUpdateBatch()
{
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryFolderResultNode::OnItemAdded(PRInt64 aItemId,
                                          PRInt64 aParentFolder,
                                          PRInt32 aIndex,
                                          PRUint16 aItemType,
                                          nsIURI* aURI,
                                          const nsACString& aTitle,
                                          PRTime aDateAdded,
                                          const nsACString& aGUID,
                                          const nsACString& aParentGUID)
{
  NS_ASSERTION(aParentFolder == mItemId, "Got wrong bookmark update");

  bool excludeItems = (mResult && mResult->mRootNode->mOptions->ExcludeItems()) ||
                        (mParent && mParent->mOptions->ExcludeItems()) ||
                        mOptions->ExcludeItems();

  // here, try to do something reasonable if the bookmark service gives us
  // a bogus index.
  if (aIndex < 0) {
    NS_NOTREACHED("Invalid index for item adding: <0");
    aIndex = 0;
  }
  else if (aIndex > mChildren.Count()) {
    if (!excludeItems) {
      // Something wrong happened while updating indexes.
      NS_NOTREACHED("Invalid index for item adding: greater than count");
    }
    aIndex = mChildren.Count();
  }

  RESTART_AND_RETURN_IF_ASYNC_PENDING();

  nsresult rv;

  // Check for query URIs, which are bookmarks, but treated as containers
  // in results and views.
  bool isQuery = false;
  if (aItemType == nsINavBookmarksService::TYPE_BOOKMARK) {
    NS_ASSERTION(aURI, "Got a null URI when we are a bookmark?!");
    nsCAutoString itemURISpec;
    rv = aURI->GetSpec(itemURISpec);
    NS_ENSURE_SUCCESS(rv, rv);
    isQuery = IsQueryURI(itemURISpec);
  }

  if (aItemType != nsINavBookmarksService::TYPE_FOLDER &&
      !isQuery && excludeItems) {
    // don't update items when we aren't displaying them, but we still need
    // to adjust bookmark indices to account for the insertion
    ReindexRange(aIndex, PR_INT32_MAX, 1);
    return NS_OK;
  }

  if (!StartIncrementalUpdate())
    return NS_OK; // folder was completely refreshed for us

  // adjust indices to account for insertion
  ReindexRange(aIndex, PR_INT32_MAX, 1);

  nsRefPtr<nsNavHistoryResultNode> node;
  if (aItemType == nsINavBookmarksService::TYPE_BOOKMARK) {
    nsNavHistory* history = nsNavHistory::GetHistoryService();
    NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
    rv = history->BookmarkIdToResultNode(aItemId, mOptions, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else if (aItemType == nsINavBookmarksService::TYPE_FOLDER) {
    nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
    NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);
    rv = bookmarks->ResultNodeForContainer(aItemId, mOptions, getter_AddRefs(node));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  else if (aItemType == nsINavBookmarksService::TYPE_SEPARATOR) {
    node = new nsNavHistorySeparatorResultNode();
    NS_ENSURE_TRUE(node, NS_ERROR_OUT_OF_MEMORY);
    node->mItemId = aItemId;
  }

  node->mBookmarkIndex = aIndex;

  if (aItemType == nsINavBookmarksService::TYPE_SEPARATOR ||
      GetSortType() == nsINavHistoryQueryOptions::SORT_BY_NONE) {
    // insert at natural bookmarks position
    return InsertChildAt(node, aIndex);
  }

  // insert at sorted position
  return InsertSortedChild(node, false);
}


NS_IMETHODIMP
nsNavHistoryFolderResultNode::OnBeforeItemRemoved(PRInt64 aItemId,
                                                  PRUint16 aItemType,
                                                  PRInt64 aParentId,
                                                  const nsACString& aGUID,
                                                  const nsACString& aParentGUID)
{
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryFolderResultNode::OnItemRemoved(PRInt64 aItemId,
                                            PRInt64 aParentFolder,
                                            PRInt32 aIndex,
                                            PRUint16 aItemType,
                                            nsIURI* aURI,
                                            const nsACString& aGUID,
                                            const nsACString& aParentGUID)
{
  // We only care about notifications when a child changes.  When the deleted
  // item is us, our parent should also be registered and will remove us from
  // its list.
  if (mItemId == aItemId)
    return NS_OK;

  NS_ASSERTION(aParentFolder == mItemId, "Got wrong bookmark update");

  RESTART_AND_RETURN_IF_ASYNC_PENDING();

  bool excludeItems = (mResult && mResult->mRootNode->mOptions->ExcludeItems()) ||
                        (mParent && mParent->mOptions->ExcludeItems()) ||
                        mOptions->ExcludeItems();

  // don't trust the index from the bookmark service, find it ourselves.  The
  // sorting could be different, or the bookmark services indices and ours might
  // be out of sync somehow.
  PRUint32 index;
  nsNavHistoryResultNode* node = FindChildById(aItemId, &index);
  if (!node) {
    if (excludeItems)
      return NS_OK;

    NS_NOTREACHED("Removing item we don't have");
    return NS_ERROR_FAILURE;
  }

  if ((node->IsURI() || node->IsSeparator()) && excludeItems) {
    // don't update items when we aren't displaying them, but we do need to
    // adjust everybody's bookmark indices to account for the removal
    ReindexRange(aIndex, PR_INT32_MAX, -1);
    return NS_OK;
  }

  if (!StartIncrementalUpdate())
    return NS_OK; // we are completely refreshed

  // shift all following indices down
  ReindexRange(aIndex + 1, PR_INT32_MAX, -1);

  return RemoveChildAt(index);
}


NS_IMETHODIMP
nsNavHistoryResultNode::OnItemChanged(PRInt64 aItemId,
                                      const nsACString& aProperty,
                                      bool aIsAnnotationProperty,
                                      const nsACString& aNewValue,
                                      PRTime aLastModified,
                                      PRUint16 aItemType,
                                      PRInt64 aParentId,
                                      const nsACString& aGUID,
                                      const nsACString& aParentGUID)
{
  if (aItemId != mItemId)
    return NS_OK;

  mLastModified = aLastModified;

  nsNavHistoryResult* result = GetResult();
  NS_ENSURE_STATE(result);

  bool shouldNotify = !mParent || mParent->AreChildrenVisible();

  if (aIsAnnotationProperty) {
    if (shouldNotify)
      NOTIFY_RESULT_OBSERVERS(result, NodeAnnotationChanged(this, aProperty));
  }
  else if (aProperty.EqualsLiteral("title")) {
    // XXX: what should we do if the new title is void?
    mTitle = aNewValue;
    if (shouldNotify)
      NOTIFY_RESULT_OBSERVERS(result, NodeTitleChanged(this, mTitle));
  }
  else if (aProperty.EqualsLiteral("uri")) {
    // clear the tags string as well
    mTags.SetIsVoid(true);
    mURI = aNewValue;
    if (shouldNotify)
      NOTIFY_RESULT_OBSERVERS(result, NodeURIChanged(this, mURI));
  }
  else if (aProperty.EqualsLiteral("favicon")) {
    mFaviconURI = aNewValue;
    if (shouldNotify)
      NOTIFY_RESULT_OBSERVERS(result, NodeIconChanged(this));
  }
  else if (aProperty.EqualsLiteral("cleartime")) {
    mTime = 0;
    if (shouldNotify) {
      NOTIFY_RESULT_OBSERVERS(result,
                              NodeHistoryDetailsChanged(this, 0, mAccessCount));
    }
  }
  else if (aProperty.EqualsLiteral("tags")) {
    mTags.SetIsVoid(true);
    if (shouldNotify)
      NOTIFY_RESULT_OBSERVERS(result, NodeTagsChanged(this));
  }
  else if (aProperty.EqualsLiteral("dateAdded")) {
    // aNewValue has the date as a string, but we can use aLastModified,
    // because it's set to the same value when dateAdded is changed.
    mDateAdded = aLastModified;
    if (shouldNotify)
      NOTIFY_RESULT_OBSERVERS(result, NodeDateAddedChanged(this, mDateAdded));
  }
  else if (aProperty.EqualsLiteral("lastModified")) {
    if (shouldNotify) {
      NOTIFY_RESULT_OBSERVERS(result,
                              NodeLastModifiedChanged(this, aLastModified));
    }
  }
  else if (aProperty.EqualsLiteral("keyword")) {
    if (shouldNotify)
      NOTIFY_RESULT_OBSERVERS(result, NodeKeywordChanged(this, aNewValue));
  }
  else
    NS_NOTREACHED("Unknown bookmark property changing.");

  if (!mParent)
    return NS_OK;

  // DO NOT OPTIMIZE THIS TO CHECK aProperty
  // The sorting methods fall back to each other so we need to re-sort the
  // result even if it's not set to sort by the given property.
  PRInt32 ourIndex = mParent->FindChild(this);
  NS_ASSERTION(ourIndex >= 0, "Could not find self in parent");
  if (ourIndex >= 0)
    mParent->EnsureItemPosition(ourIndex);

  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryFolderResultNode::OnItemChanged(PRInt64 aItemId,
                                            const nsACString& aProperty,
                                            bool aIsAnnotationProperty,
                                            const nsACString& aNewValue,
                                            PRTime aLastModified,
                                            PRUint16 aItemType,
                                            PRInt64 aParentId,
                                            const nsACString& aGUID,
                                            const nsACString&aParentGUID)
{
  // The query-item's title is used for simple-query nodes
  if (mQueryItemId != -1) {
    bool isTitleChange = aProperty.EqualsLiteral("title");
    if ((mQueryItemId == aItemId && !isTitleChange) ||
        (mQueryItemId != aItemId && isTitleChange)) {
      return NS_OK;
    }
  }

  RESTART_AND_RETURN_IF_ASYNC_PENDING();

  return nsNavHistoryResultNode::OnItemChanged(aItemId, aProperty,
                                               aIsAnnotationProperty,
                                               aNewValue, aLastModified,
                                               aItemType, aParentId, aGUID,
                                               aParentGUID);
}

/**
 * Updates visit count and last visit time and refreshes.
 */
NS_IMETHODIMP
nsNavHistoryFolderResultNode::OnItemVisited(PRInt64 aItemId,
                                            PRInt64 aVisitId,
                                            PRTime aTime,
                                            PRUint32 aTransitionType,
                                            nsIURI* aURI,
                                            PRInt64 aParentId,
                                            const nsACString& aGUID,
                                            const nsACString& aParentGUID)
{
  bool excludeItems = (mResult && mResult->mRootNode->mOptions->ExcludeItems()) ||
                        (mParent && mParent->mOptions->ExcludeItems()) ||
                        mOptions->ExcludeItems();
  if (excludeItems)
    return NS_OK; // don't update items when we aren't displaying them

  RESTART_AND_RETURN_IF_ASYNC_PENDING();

  if (!StartIncrementalUpdate())
    return NS_OK;

  PRUint32 nodeIndex;
  nsNavHistoryResultNode* node = FindChildById(aItemId, &nodeIndex);
  if (!node)
    return NS_ERROR_FAILURE;

  // Update node.
  node->mTime = aTime;
  ++node->mAccessCount;

  // Update us.
  PRInt32 oldAccessCount = mAccessCount;
  ++mAccessCount;
  if (aTime > mTime)
    mTime = aTime;
  nsresult rv = ReverseUpdateStats(mAccessCount - oldAccessCount);
  NS_ENSURE_SUCCESS(rv, rv);

  if (AreChildrenVisible()) {
    // Sorting has not changed, just redraw the row if it's visible.
    nsNavHistoryResult* result = GetResult();
    NOTIFY_RESULT_OBSERVERS(result,
                            NodeHistoryDetailsChanged(node, mTime, mAccessCount));
  }

  // Update sorting if necessary.
  PRUint32 sortType = GetSortType();
  if (sortType == nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_ASCENDING ||
      sortType == nsINavHistoryQueryOptions::SORT_BY_VISITCOUNT_DESCENDING ||
      sortType == nsINavHistoryQueryOptions::SORT_BY_DATE_ASCENDING ||
      sortType == nsINavHistoryQueryOptions::SORT_BY_DATE_DESCENDING) {
    PRInt32 childIndex = FindChild(node);
    NS_ASSERTION(childIndex >= 0, "Could not find child we just got a reference to");
    if (childIndex >= 0) {
      EnsureItemPosition(childIndex);
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryFolderResultNode::OnItemMoved(PRInt64 aItemId,
                                          PRInt64 aOldParent,
                                          PRInt32 aOldIndex,
                                          PRInt64 aNewParent,
                                          PRInt32 aNewIndex,
                                          PRUint16 aItemType,
                                          const nsACString& aGUID,
                                          const nsACString& aOldParentGUID,
                                          const nsACString& aNewParentGUID)
{
  NS_ASSERTION(aOldParent == mItemId || aNewParent == mItemId,
               "Got a bookmark message that doesn't belong to us");

  RESTART_AND_RETURN_IF_ASYNC_PENDING();

  if (!StartIncrementalUpdate())
    return NS_OK; // entire container was refreshed for us

  if (aOldParent == aNewParent) {
    // getting moved within the same folder, we don't want to do a remove and
    // an add because that will lose your tree state.

    // adjust bookmark indices
    ReindexRange(aOldIndex + 1, PR_INT32_MAX, -1);
    ReindexRange(aNewIndex, PR_INT32_MAX, 1);

    PRUint32 index;
    nsNavHistoryResultNode* node = FindChildById(aItemId, &index);
    if (!node) {
      NS_NOTREACHED("Can't find folder that is moving!");
      return NS_ERROR_FAILURE;
    }
    NS_ASSERTION(index >= 0 && index < PRUint32(mChildren.Count()),
                 "Invalid index!");
    node->mBookmarkIndex = aNewIndex;

    // adjust position
    if (index >= 0)
      EnsureItemPosition(index);
    return NS_OK;
  } else {
    // moving between two different folders, just do a remove and an add
    nsCOMPtr<nsIURI> itemURI;
    nsCAutoString itemTitle;
    if (aItemType == nsINavBookmarksService::TYPE_BOOKMARK) {
      nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
      NS_ENSURE_TRUE(bookmarks, NS_ERROR_OUT_OF_MEMORY);
      nsresult rv = bookmarks->GetBookmarkURI(aItemId, getter_AddRefs(itemURI));
      NS_ENSURE_SUCCESS(rv, rv);
      rv = bookmarks->GetItemTitle(aItemId, itemTitle);
      NS_ENSURE_SUCCESS(rv, rv);
    }
    if (aOldParent == mItemId) {
      OnItemRemoved(aItemId, aOldParent, aOldIndex, aItemType, itemURI,
                    aGUID, aOldParentGUID);
    }
    if (aNewParent == mItemId) {
      OnItemAdded(aItemId, aNewParent, aNewIndex, aItemType, itemURI, itemTitle,
                  PR_Now(), // This is a dummy dateAdded, not the real value.
                  aGUID, aNewParentGUID);
    }
  }
  return NS_OK;
}


/**
 * Separator nodes do not hold any data.
 */
nsNavHistorySeparatorResultNode::nsNavHistorySeparatorResultNode()
  : nsNavHistoryResultNode(EmptyCString(), EmptyCString(),
                           0, 0, EmptyCString())
{
}


NS_IMPL_CYCLE_COLLECTION_CLASS(nsNavHistoryResult)

static PLDHashOperator
RemoveBookmarkFolderObserversCallback(nsTrimInt64HashKey::KeyType aKey,
                                      nsNavHistoryResult::FolderObserverList*& aData,
                                      void* userArg)
{
  delete aData;
  return PL_DHASH_REMOVE;
}

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsNavHistoryResult)
  tmp->StopObserving();
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSCOMPTR(mRootNode)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSTARRAY(mObservers)
  tmp->mBookmarkFolderObservers.Enumerate(&RemoveBookmarkFolderObserversCallback, nsnull);
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSTARRAY(mAllBookmarksObservers)
  NS_IMPL_CYCLE_COLLECTION_UNLINK_NSTARRAY(mHistoryObservers)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

static PLDHashOperator
TraverseBookmarkFolderObservers(nsTrimInt64HashKey::KeyType aKey,
                                nsNavHistoryResult::FolderObserverList* &aData,
                                void *aClosure)
{
  nsCycleCollectionTraversalCallback* cb =
    static_cast<nsCycleCollectionTraversalCallback*>(aClosure);
  for (PRUint32 i = 0; i < aData->Length(); ++i) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb,
                                       "mBookmarkFolderObservers value[i]");
    nsNavHistoryResultNode* node = aData->ElementAt(i);
    cb->NoteXPCOMChild(node);
  }
  return PL_DHASH_NEXT;
}

static void
traverseResultObservers(nsMaybeWeakPtrArray<nsINavHistoryResultObserver> aObservers,
                        void *aClosure)
{
  nsCycleCollectionTraversalCallback* cb =
    static_cast<nsCycleCollectionTraversalCallback*>(aClosure);
  for (PRUint32 i = 0; i < aObservers.Length(); ++i) {
    NS_CYCLE_COLLECTION_NOTE_EDGE_NAME(*cb, "mResultObservers value[i]");
    const nsCOMPtr<nsINavHistoryResultObserver> &obs = aObservers.ElementAt(i);
    cb->NoteXPCOMChild(obs);
  }
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsNavHistoryResult)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMPTR_AMBIGUOUS(mRootNode, nsINavHistoryContainerResultNode)
  traverseResultObservers(tmp->mObservers, &cb);
  tmp->mBookmarkFolderObservers.Enumerate(&TraverseBookmarkFolderObservers, &cb);
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSTARRAY_MEMBER(mAllBookmarksObservers, nsNavHistoryQueryResultNode)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSTARRAY_MEMBER(mHistoryObservers, nsNavHistoryQueryResultNode)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsNavHistoryResult)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsNavHistoryResult)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsNavHistoryResult)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsINavHistoryResult)
  NS_INTERFACE_MAP_STATIC_AMBIGUOUS(nsNavHistoryResult)
  NS_INTERFACE_MAP_ENTRY(nsINavHistoryResult)
  NS_INTERFACE_MAP_ENTRY(nsINavBookmarkObserver)
  NS_INTERFACE_MAP_ENTRY(nsINavHistoryObserver)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

nsNavHistoryResult::nsNavHistoryResult(nsNavHistoryContainerResultNode* aRoot)
: mRootNode(aRoot)
, mNeedsToApplySortingMode(false)
, mIsHistoryObserver(false)
, mIsBookmarkFolderObserver(false)
, mIsAllBookmarksObserver(false)
, mBatchInProgress(false)
, mSuppressNotifications(false)
{
  mRootNode->mResult = this;
}

nsNavHistoryResult::~nsNavHistoryResult()
{
  // delete all bookmark folder observer arrays which are allocated on the heap
  mBookmarkFolderObservers.Enumerate(&RemoveBookmarkFolderObserversCallback, nsnull);
}

void
nsNavHistoryResult::StopObserving()
{
  if (mIsBookmarkFolderObserver || mIsAllBookmarksObserver) {
    nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
    if (bookmarks) {
      bookmarks->RemoveObserver(this);
      mIsBookmarkFolderObserver = false;
      mIsAllBookmarksObserver = false;
    }
  }
  if (mIsHistoryObserver) {
    nsNavHistory* history = nsNavHistory::GetHistoryService();
    if (history) {
      history->RemoveObserver(this);
      mIsHistoryObserver = false;
    }
  }
}

/**
 * @note you must call AddRef before this, since we may do things like
 * register ourselves.
 */
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
  for (PRUint32 i = 0; i < aQueryCount; ++i) {
    nsCOMPtr<nsINavHistoryQuery> queryClone;
    rv = aQueries[i]->Clone(getter_AddRefs(queryClone));
    NS_ENSURE_SUCCESS(rv, rv);
    if (!mQueries.AppendObject(queryClone))
      return NS_ERROR_OUT_OF_MEMORY;
  }
  rv = aOptions->Clone(getter_AddRefs(mOptions));
  NS_ENSURE_SUCCESS(rv, rv);
  mSortingMode = aOptions->SortingMode();
  rv = aOptions->GetSortingAnnotation(mSortingAnnotation);
  NS_ENSURE_SUCCESS(rv, rv);

  mBookmarkFolderObservers.Init(128);

  NS_ASSERTION(mRootNode->mIndentLevel == -1,
               "Root node's indent level initialized wrong");
  mRootNode->FillStats();

  return NS_OK;
}


/**
 * Constructs a new history result object.
 */
nsresult // static
nsNavHistoryResult::NewHistoryResult(nsINavHistoryQuery** aQueries,
                                     PRUint32 aQueryCount,
                                     nsNavHistoryQueryOptions* aOptions,
                                     nsNavHistoryContainerResultNode* aRoot,
                                     bool aBatchInProgress,
                                     nsNavHistoryResult** result)
{
  *result = new nsNavHistoryResult(aRoot);
  if (!*result)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*result); // must happen before Init
  // Correctly set mBatchInProgress for the result based on the root node value.
  (*result)->mBatchInProgress = aBatchInProgress;
  nsresult rv = (*result)->Init(aQueries, aQueryCount, aOptions);
  if (NS_FAILED(rv)) {
    NS_RELEASE(*result);
    *result = nsnull;
    return rv;
  }

  return NS_OK;
}


void
nsNavHistoryResult::AddHistoryObserver(nsNavHistoryQueryResultNode* aNode)
{
  if (!mIsHistoryObserver) {
      nsNavHistory* history = nsNavHistory::GetHistoryService();
      NS_ASSERTION(history, "Can't create history service");
      history->AddObserver(this, true);
      mIsHistoryObserver = true;
  }
  // Don't add duplicate observers.  In some case we don't unregister when
  // children are cleared (see ClearChildren) and the next FillChildren call
  // will try to add the observer again.
  if (mHistoryObservers.IndexOf(aNode) == mHistoryObservers.NoIndex) {
    mHistoryObservers.AppendElement(aNode);
  }
}


void
nsNavHistoryResult::AddAllBookmarksObserver(nsNavHistoryQueryResultNode* aNode)
{
  if (!mIsAllBookmarksObserver && !mIsBookmarkFolderObserver) {
    nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
    if (!bookmarks) {
      NS_NOTREACHED("Can't create bookmark service");
      return;
    }
    bookmarks->AddObserver(this, true);
    mIsAllBookmarksObserver = true;
  }
  // Don't add duplicate observers.  In some case we don't unregister when
  // children are cleared (see ClearChildren) and the next FillChildren call
  // will try to add the observer again.
  if (mAllBookmarksObservers.IndexOf(aNode) == mAllBookmarksObservers.NoIndex) {
    mAllBookmarksObservers.AppendElement(aNode);
  }
}


void
nsNavHistoryResult::AddBookmarkFolderObserver(nsNavHistoryFolderResultNode* aNode,
                                              PRInt64 aFolder)
{
  if (!mIsBookmarkFolderObserver && !mIsAllBookmarksObserver) {
    nsNavBookmarks* bookmarks = nsNavBookmarks::GetBookmarksService();
    if (!bookmarks) {
      NS_NOTREACHED("Can't create bookmark service");
      return;
    }
    bookmarks->AddObserver(this, true);
    mIsBookmarkFolderObserver = true;
  }
  // Don't add duplicate observers.  In some case we don't unregister when
  // children are cleared (see ClearChildren) and the next FillChildren call
  // will try to add the observer again.
  FolderObserverList* list = BookmarkFolderObserversForId(aFolder, true);
  if (list->IndexOf(aNode) == list->NoIndex) {
    list->AppendElement(aNode);
  }
}


void
nsNavHistoryResult::RemoveHistoryObserver(nsNavHistoryQueryResultNode* aNode)
{
  mHistoryObservers.RemoveElement(aNode);
}


void
nsNavHistoryResult::RemoveAllBookmarksObserver(nsNavHistoryQueryResultNode* aNode)
{
  mAllBookmarksObservers.RemoveElement(aNode);
}


void
nsNavHistoryResult::RemoveBookmarkFolderObserver(nsNavHistoryFolderResultNode* aNode,
                                                 PRInt64 aFolder)
{
  FolderObserverList* list = BookmarkFolderObserversForId(aFolder, false);
  if (!list)
    return; // we don't even have an entry for that folder
  list->RemoveElement(aNode);
}


nsNavHistoryResult::FolderObserverList*
nsNavHistoryResult::BookmarkFolderObserversForId(PRInt64 aFolderId, bool aCreate)
{
  FolderObserverList* list;
  if (mBookmarkFolderObservers.Get(aFolderId, &list))
    return list;
  if (!aCreate)
    return nsnull;

  // need to create a new list
  list = new FolderObserverList;
  mBookmarkFolderObservers.Put(aFolderId, list);
  return list;
}


NS_IMETHODIMP
nsNavHistoryResult::GetSortingMode(PRUint16* aSortingMode)
{
  *aSortingMode = mSortingMode;
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryResult::SetSortingMode(PRUint16 aSortingMode)
{
  NS_ENSURE_STATE(mRootNode);

  if (aSortingMode > nsINavHistoryQueryOptions::SORT_BY_FRECENCY_DESCENDING)
    return NS_ERROR_INVALID_ARG;

  // Keep everything in sync.
  NS_ASSERTION(mOptions, "Options should always be present for a root query");

  mSortingMode = aSortingMode;

  if (!mRootNode->mExpanded) {
    // Need to do this later when node will be expanded.
    mNeedsToApplySortingMode = true;
    return NS_OK;
  }

  // Actually do sorting.
  nsNavHistoryContainerResultNode::SortComparator comparator =
      nsNavHistoryContainerResultNode::GetSortingComparator(aSortingMode);
  if (comparator) {
    nsNavHistory* history = nsNavHistory::GetHistoryService();
    NS_ENSURE_TRUE(history, NS_ERROR_OUT_OF_MEMORY);
    mRootNode->RecursiveSort(mSortingAnnotation.get(), comparator);
  }

  NOTIFY_RESULT_OBSERVERS(this, SortingChanged(aSortingMode));
  NOTIFY_RESULT_OBSERVERS(this, InvalidateContainer(mRootNode));
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryResult::GetSortingAnnotation(nsACString& _result) {
  _result.Assign(mSortingAnnotation);
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryResult::SetSortingAnnotation(const nsACString& aSortingAnnotation) {
  mSortingAnnotation.Assign(aSortingAnnotation);
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryResult::AddObserver(nsINavHistoryResultObserver* aObserver,
                                bool aOwnsWeak)
{
  NS_ENSURE_ARG(aObserver);
  nsresult rv = mObservers.AppendWeakElement(aObserver, aOwnsWeak);
  NS_ENSURE_SUCCESS(rv, rv);
  
  rv = aObserver->SetResult(this);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we are batching, notify a fake batch start to the observers.
  // Not doing so would then notify a not coupled batch end.
  if (mBatchInProgress) {
    NOTIFY_RESULT_OBSERVERS(this, Batching(true));
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryResult::RemoveObserver(nsINavHistoryResultObserver* aObserver)
{
  NS_ENSURE_ARG(aObserver);
  return mObservers.RemoveWeakElement(aObserver);
}


NS_IMETHODIMP
nsNavHistoryResult::GetSuppressNotifications(bool* _retval)
{
  *_retval = mSuppressNotifications;
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryResult::SetSuppressNotifications(bool aSuppressNotifications)
{
  mSuppressNotifications = aSuppressNotifications;
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryResult::GetRoot(nsINavHistoryContainerResultNode** aRoot)
{
  if (!mRootNode) {
    NS_NOTREACHED("Root is null");
    *aRoot = nsnull;
    return NS_ERROR_FAILURE;
  }
  return mRootNode->QueryInterface(NS_GET_IID(nsINavHistoryContainerResultNode),
                                   reinterpret_cast<void**>(aRoot));
}


void
nsNavHistoryResult::requestRefresh(nsNavHistoryContainerResultNode* aContainer)
{
  // Don't add twice the same container.
  if (mRefreshParticipants.IndexOf(aContainer) == mRefreshParticipants.NoIndex)
    mRefreshParticipants.AppendElement(aContainer);
}

// nsINavBookmarkObserver implementation

// Here, it is important that we create a COPY of the observer array. Some
// observers will requery themselves, which may cause the observer array to
// be modified or added to.
#define ENUMERATE_BOOKMARK_FOLDER_OBSERVERS(_folderId, _functionCall) \
  PR_BEGIN_MACRO \
    FolderObserverList* _fol = BookmarkFolderObserversForId(_folderId, false); \
    if (_fol) { \
      FolderObserverList _listCopy(*_fol); \
      for (PRUint32 _fol_i = 0; _fol_i < _listCopy.Length(); ++_fol_i) { \
        if (_listCopy[_fol_i]) \
          _listCopy[_fol_i]->_functionCall; \
      } \
    } \
  PR_END_MACRO
#define ENUMERATE_LIST_OBSERVERS(_listType, _functionCall, _observersList, _conditionCall) \
  PR_BEGIN_MACRO \
    _listType _listCopy(_observersList); \
    for (PRUint32 _obs_i = 0; _obs_i < _listCopy.Length(); ++_obs_i) { \
      if (_listCopy[_obs_i] && _listCopy[_obs_i]->_conditionCall) \
        _listCopy[_obs_i]->_functionCall; \
    } \
  PR_END_MACRO
#define ENUMERATE_QUERY_OBSERVERS(_functionCall, _observersList, _conditionCall) \
  ENUMERATE_LIST_OBSERVERS(QueryObserverList, _functionCall, _observersList, _conditionCall)
#define ENUMERATE_ALL_BOOKMARKS_OBSERVERS(_functionCall) \
  ENUMERATE_QUERY_OBSERVERS(_functionCall, mAllBookmarksObservers, IsQuery())
#define ENUMERATE_HISTORY_OBSERVERS(_functionCall) \
  ENUMERATE_QUERY_OBSERVERS(_functionCall, mHistoryObservers, IsQuery())

#define NOTIFY_REFRESH_PARTICIPANTS() \
  PR_BEGIN_MACRO \
  ENUMERATE_LIST_OBSERVERS(ContainerObserverList, Refresh(), mRefreshParticipants, IsContainer()); \
  mRefreshParticipants.Clear(); \
  PR_END_MACRO

NS_IMETHODIMP
nsNavHistoryResult::OnBeginUpdateBatch()
{
  // Since we could be observing both history and bookmarks, it's possible both
  // notify the batch.  We can safely ignore nested calls.
  if (!mBatchInProgress) {
    mBatchInProgress = true;
    ENUMERATE_HISTORY_OBSERVERS(OnBeginUpdateBatch());
    ENUMERATE_ALL_BOOKMARKS_OBSERVERS(OnBeginUpdateBatch());

    NOTIFY_RESULT_OBSERVERS(this, Batching(true));
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryResult::OnEndUpdateBatch()
{
  // Since we could be observing both history and bookmarks, it's possible both
  // notify the batch.  We can safely ignore nested calls.
  // Notice it's possible we are notified OnEndUpdateBatch more times than
  // onBeginUpdateBatch, since the result could be created in the middle of
  // nested batches.
  if (mBatchInProgress) {
    ENUMERATE_HISTORY_OBSERVERS(OnEndUpdateBatch());
    ENUMERATE_ALL_BOOKMARKS_OBSERVERS(OnEndUpdateBatch());

    // Setting mBatchInProgress before notifying the end of the batch to
    // observers would make evantual calls to Refresh() directly handled rather
    // than enqueued.  Thus set it just before handling refreshes.
    mBatchInProgress = false;
    NOTIFY_REFRESH_PARTICIPANTS();
    NOTIFY_RESULT_OBSERVERS(this, Batching(false));
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryResult::OnItemAdded(PRInt64 aItemId,
                                PRInt64 aParentId,
                                PRInt32 aIndex,
                                PRUint16 aItemType,
                                nsIURI* aURI,
                                const nsACString& aTitle,
                                PRTime aDateAdded,
                                const nsACString& aGUID,
                                const nsACString& aParentGUID)
{
  ENUMERATE_BOOKMARK_FOLDER_OBSERVERS(aParentId,
    OnItemAdded(aItemId, aParentId, aIndex, aItemType, aURI, aTitle, aDateAdded,
                aGUID, aParentGUID)
  );
  ENUMERATE_HISTORY_OBSERVERS(
    OnItemAdded(aItemId, aParentId, aIndex, aItemType, aURI, aTitle, aDateAdded,
                aGUID, aParentGUID)
  );
  ENUMERATE_ALL_BOOKMARKS_OBSERVERS(
    OnItemAdded(aItemId, aParentId, aIndex, aItemType, aURI, aTitle, aDateAdded,
                aGUID, aParentGUID)
  );
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryResult::OnBeforeItemRemoved(PRInt64 aItemId,
                                        PRUint16 aItemType,
                                        PRInt64 aParentId,
                                        const nsACString& aGUID,
                                        const nsACString& aParentGUID)
{
  ENUMERATE_ALL_BOOKMARKS_OBSERVERS(
    OnBeforeItemRemoved(aItemId, aItemType, aParentId, aGUID, aParentGUID);
  );
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryResult::OnItemRemoved(PRInt64 aItemId,
                                  PRInt64 aParentId,
                                  PRInt32 aIndex,
                                  PRUint16 aItemType,
                                  nsIURI* aURI,
                                  const nsACString& aGUID,
                                  const nsACString& aParentGUID)
{
  ENUMERATE_BOOKMARK_FOLDER_OBSERVERS(aParentId,
      OnItemRemoved(aItemId, aParentId, aIndex, aItemType, aURI, aGUID,
                    aParentGUID));
  ENUMERATE_ALL_BOOKMARKS_OBSERVERS(
      OnItemRemoved(aItemId, aParentId, aIndex, aItemType, aURI, aGUID,
                    aParentGUID));
  ENUMERATE_HISTORY_OBSERVERS(
      OnItemRemoved(aItemId, aParentId, aIndex, aItemType, aURI, aGUID,
                    aParentGUID));
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryResult::OnItemChanged(PRInt64 aItemId,
                                  const nsACString &aProperty,
                                  bool aIsAnnotationProperty,
                                  const nsACString &aNewValue,
                                  PRTime aLastModified,
                                  PRUint16 aItemType,
                                  PRInt64 aParentId,
                                  const nsACString& aGUID,
                                  const nsACString& aParentGUID)
{
  ENUMERATE_ALL_BOOKMARKS_OBSERVERS(
    OnItemChanged(aItemId, aProperty, aIsAnnotationProperty, aNewValue,
                  aLastModified, aItemType, aParentId, aGUID, aParentGUID));

  // Note: folder-nodes set their own bookmark observer only once they're
  // opened, meaning we cannot optimize this code path for changes done to
  // folder-nodes.

  FolderObserverList* list = BookmarkFolderObserversForId(aParentId, false);
  if (!list)
    return NS_OK;

  for (PRUint32 i = 0; i < list->Length(); ++i) {
    nsRefPtr<nsNavHistoryFolderResultNode> folder = list->ElementAt(i);
    if (folder) {
      PRUint32 nodeIndex;
      nsRefPtr<nsNavHistoryResultNode> node =
        folder->FindChildById(aItemId, &nodeIndex);
      // if ExcludeItems is true we don't update non visible items
      bool excludeItems = (mRootNode->mOptions->ExcludeItems()) ||
                             folder->mOptions->ExcludeItems();
      if (node &&
          (!excludeItems || !(node->IsURI() || node->IsSeparator())) &&
          folder->StartIncrementalUpdate()) {
        node->OnItemChanged(aItemId, aProperty, aIsAnnotationProperty,
                            aNewValue, aLastModified, aItemType, aParentId,
                            aGUID, aParentGUID);
      }
    }
  }

  // Note: we do NOT call history observers in this case.  This notification is
  // the same as other history notification, except that here we know the item
  // is a bookmark.  History observers will handle the history notification
  // instead.
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryResult::OnItemVisited(PRInt64 aItemId,
                                  PRInt64 aVisitId,
                                  PRTime aVisitTime,
                                  PRUint32 aTransitionType,
                                  nsIURI* aURI,
                                  PRInt64 aParentId,
                                  const nsACString& aGUID,
                                  const nsACString& aParentGUID)
{
  ENUMERATE_BOOKMARK_FOLDER_OBSERVERS(aParentId,
      OnItemVisited(aItemId, aVisitId, aVisitTime, aTransitionType, aURI,
                    aParentId, aGUID, aParentGUID));
  ENUMERATE_ALL_BOOKMARKS_OBSERVERS(
      OnItemVisited(aItemId, aVisitId, aVisitTime, aTransitionType, aURI,
                    aParentId, aGUID, aParentGUID));
  // Note: we do NOT call history observers in this case.  This notification is
  // the same as OnVisit, except that here we know the item is a bookmark.
  // History observers will handle the history notification instead.
  return NS_OK;
}


/**
 * Need to notify both the source and the destination folders (if they are
 * different).
 */
NS_IMETHODIMP
nsNavHistoryResult::OnItemMoved(PRInt64 aItemId,
                                PRInt64 aOldParent,
                                PRInt32 aOldIndex,
                                PRInt64 aNewParent,
                                PRInt32 aNewIndex,
                                PRUint16 aItemType,
                                const nsACString& aGUID,
                                const nsACString& aOldParentGUID,
                                const nsACString& aNewParentGUID)
{
  ENUMERATE_BOOKMARK_FOLDER_OBSERVERS(aOldParent,
      OnItemMoved(aItemId, aOldParent, aOldIndex, aNewParent, aNewIndex,
                  aItemType, aGUID, aOldParentGUID, aNewParentGUID));
  if (aNewParent != aOldParent) {
    ENUMERATE_BOOKMARK_FOLDER_OBSERVERS(aNewParent,
        OnItemMoved(aItemId, aOldParent, aOldIndex, aNewParent, aNewIndex,
                    aItemType, aGUID, aOldParentGUID, aNewParentGUID));
  }
  ENUMERATE_ALL_BOOKMARKS_OBSERVERS(OnItemMoved(aItemId, aOldParent, aOldIndex,
                                                aNewParent, aNewIndex,
                                                aItemType, aGUID,
                                                aOldParentGUID,
                                                aNewParentGUID));
  ENUMERATE_HISTORY_OBSERVERS(OnItemMoved(aItemId, aOldParent, aOldIndex,
                                          aNewParent, aNewIndex, aItemType,
                                          aGUID, aOldParentGUID,
                                          aNewParentGUID));
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryResult::OnVisit(nsIURI* aURI, PRInt64 aVisitId, PRTime aTime,
                            PRInt64 aSessionId, PRInt64 aReferringId,
                            PRUint32 aTransitionType, const nsACString& aGUID,
                            PRUint32* aAdded)
{
  PRUint32 added = 0;

  ENUMERATE_HISTORY_OBSERVERS(OnVisit(aURI, aVisitId, aTime, aSessionId,
                                      aReferringId, aTransitionType, aGUID,
                                      &added));

  if (!mRootNode->mExpanded)
    return NS_OK;

  // If this visit is accepted by an overlapped container, and not all
  // overlapped containers are visible, we should still call Refresh if the
  // visit falls into any of them.
  bool todayIsMissing = false;
  PRUint32 resultType = mRootNode->mOptions->ResultType();
  if (resultType == nsINavHistoryQueryOptions::RESULTS_AS_DATE_QUERY ||
      resultType == nsINavHistoryQueryOptions::RESULTS_AS_DATE_SITE_QUERY) {
    PRUint32 childCount;
    nsresult rv = mRootNode->GetChildCount(&childCount);
    NS_ENSURE_SUCCESS(rv, rv);
    if (childCount) {
      nsCOMPtr<nsINavHistoryResultNode> firstChild;
      rv = mRootNode->GetChild(0, getter_AddRefs(firstChild));
      NS_ENSURE_SUCCESS(rv, rv);
      nsCAutoString title;
      rv = firstChild->GetTitle(title);
      NS_ENSURE_SUCCESS(rv, rv);
      nsNavHistory* history = nsNavHistory::GetHistoryService();
      NS_ENSURE_TRUE(history, 0);
      nsCAutoString todayLabel;
      history->GetStringFromName(
        NS_LITERAL_STRING("finduri-AgeInDays-is-0").get(), todayLabel);
      todayIsMissing = !todayLabel.Equals(title);
    }
  }

  if (!added || todayIsMissing) {
    // None of registered query observers has accepted our URI.  This means,
    // that a matching query either was not expanded or it does not exist.
    PRUint32 resultType = mRootNode->mOptions->ResultType();
    if (resultType == nsINavHistoryQueryOptions::RESULTS_AS_DATE_QUERY ||
        resultType == nsINavHistoryQueryOptions::RESULTS_AS_DATE_SITE_QUERY ||
        resultType == nsINavHistoryQueryOptions::RESULTS_AS_SITE_QUERY)
      (void)mRootNode->GetAsQuery()->Refresh();
    else {
      // We are result of a folder node, then we should run through history
      // observers that are containers queries and refresh them.
      // We use a copy of the observers array since requerying could potentially
      // cause changes to the array.
      ENUMERATE_QUERY_OBSERVERS(Refresh(), mHistoryObservers, IsContainersQuery());
    }
  }

  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryResult::OnTitleChanged(nsIURI* aURI,
                                   const nsAString& aPageTitle,
                                   const nsACString& aGUID)
{
  ENUMERATE_HISTORY_OBSERVERS(OnTitleChanged(aURI, aPageTitle, aGUID));
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryResult::OnBeforeDeleteURI(nsIURI *aURI,
                                      const nsACString& aGUID,
                                      PRUint16 aReason)
{
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryResult::OnDeleteURI(nsIURI *aURI,
                                const nsACString& aGUID,
                                PRUint16 aReason)
{
  ENUMERATE_HISTORY_OBSERVERS(OnDeleteURI(aURI, aGUID, aReason));
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryResult::OnClearHistory()
{
  ENUMERATE_HISTORY_OBSERVERS(OnClearHistory());
  return NS_OK;
}


NS_IMETHODIMP
nsNavHistoryResult::OnPageChanged(nsIURI* aURI,
                                  PRUint32 aChangedAttribute,
                                  const nsAString& aValue,
                                  const nsACString& aGUID)
{
  ENUMERATE_HISTORY_OBSERVERS(OnPageChanged(aURI, aChangedAttribute, aValue, aGUID));
  return NS_OK;
}


/**
 * Don't do anything when visits expire.
 */
NS_IMETHODIMP
nsNavHistoryResult::OnDeleteVisits(nsIURI* aURI,
                                   PRTime aVisitTime,
                                   const nsACString& aGUID,
                                   PRUint16 aReason)
{
  ENUMERATE_HISTORY_OBSERVERS(OnDeleteVisits(aURI, aVisitTime, aGUID, aReason));
  return NS_OK;
}
