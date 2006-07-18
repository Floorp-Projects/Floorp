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
#include "nsFaviconService.h"
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
#include "mozStorageHelper.h"

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

nsNavHistoryResultNode::nsNavHistoryResultNode() :
    mParent(nsnull),
    mID(0),
    mAccessCount(0),
    mTime(0),
    mExpanded(PR_FALSE)
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
  *aID = 0;
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

/* attribute string folderType; */
NS_IMETHODIMP nsNavHistoryResultNode::GetFolderType(nsAString& aFolderType)
{
  aFolderType.Truncate(0);
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

/* attribute nsIURI con; */
NS_IMETHODIMP nsNavHistoryResultNode::GetIcon(nsIURI** aURI)
{
  nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
  NS_ENSURE_TRUE(faviconService, NS_ERROR_NO_INTERFACE);
  return faviconService->GetFaviconLinkForIconString(mFaviconURL, aURI);
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

/* readonly attribute boolean childrenReadOnly; */
NS_IMETHODIMP nsNavHistoryResultNode::GetChildrenReadOnly(PRBool *aResult)
{
  *aResult = PR_TRUE;
  return NS_OK;
}

// nsINavBookmarkObserver implementation

/* void onBeginUpdateBatch(); */
NS_IMETHODIMP
nsNavHistoryResultNode::OnBeginUpdateBatch()
{
  return NS_OK;
}

/* void onEndUpdateBatch(); */
NS_IMETHODIMP
nsNavHistoryResultNode::OnEndUpdateBatch()
{
  return NS_OK;
}

/* readonly attribute boolean wantAllDetails; */
NS_IMETHODIMP
nsNavHistoryResultNode::GetWantAllDetails(PRBool *aResult)
{
  *aResult = PR_TRUE;
  return NS_OK;
}

/* void onItemAdded(in nsIURI bookmark, in PRInt64 folder, in PRInt32 index); */
NS_IMETHODIMP
nsNavHistoryResultNode::OnItemAdded(nsIURI *aBookmark,
                                    PRInt64 aFolder,
                                    PRInt32 aIndex)
{
  return NS_OK;
}

/* void onItemRemoved(in nsIURI bookmark, in PRInt64 folder, in PRInt32 index); */
NS_IMETHODIMP
nsNavHistoryResultNode::OnItemRemoved(nsIURI *aBookmark,
                                      PRInt64 aFolder, PRInt32 aIndex)
{
  return NS_OK;
}

/* void onItemMoved(in nsIURI bookmark, in PRInt64 folder,
                    in PRInt32 oldIndex, in PRInt32 newIndex); */
NS_IMETHODIMP
nsNavHistoryResultNode::OnItemMoved(nsIURI *aBookmark, PRInt64 aFolder,
                                    PRInt32 aOldIndex, PRInt32 aNewIndex)
{
  return NS_OK;

}

/* void onItemChanged(in nsIURI bookmark, in ACString property, in AString value); */
NS_IMETHODIMP
nsNavHistoryResultNode::OnItemChanged(nsIURI *aBookmark,
                                      const nsACString &aProperty,
                                      const nsAString &aValue)
{
  // We let OnPageChanged handle this case
  return NS_OK;
}

/* void onItemVisited(in nsIURI bookmark, in PRTime time); */
NS_IMETHODIMP
nsNavHistoryResultNode::OnItemVisited(nsIURI* aBookmark, PRTime aVisitTime)
{
  return NS_OK;
}

/* void onItemReplaced(in PRInt64 folder, in nsIURI item, in nsIURI newItem); */
NS_IMETHODIMP
nsNavHistoryResultNode::OnItemReplaced(PRInt64 aFolder,
                                       nsIURI *aItem, nsIURI *aNewItem)
{
  return NS_OK;
}

/* void onFolderAdded(in PRInt64 folder, in PRInt64 parent, in PRInt32 index); */
NS_IMETHODIMP
nsNavHistoryResultNode::OnFolderAdded(PRInt64 aFolder,
                                      PRInt64 aParent, PRInt32 aIndex)
{
  return NS_OK;
}

/* void onFolderRemoved(in PRInt64 folder, in PRInt64 parent, in PRInt32 index); */
NS_IMETHODIMP
nsNavHistoryResultNode::OnFolderRemoved(PRInt64 aFolder,
                                        PRInt64 aParent, PRInt32 aIndex)
{
  return NS_OK;
}

/* void onFolderMoved(in PRInt64 folder,
                      in PRInt64 oldParent, in PRInt32 oldIndex,
                      in PRInt64 newParent, in PRInt32 newIndex); */
NS_IMETHODIMP
nsNavHistoryResultNode::OnFolderMoved(PRInt64 aFolder,
                                      PRInt64 aOldParent, PRInt32 aOldIndex,
                                      PRInt64 aNewParent, PRInt32 aNewIndex)
{
  return NS_OK;
}

/* void onFolderChanged(in PRInt64 folder, in ACString property); */
NS_IMETHODIMP
nsNavHistoryResultNode::OnFolderChanged(PRInt64 aFolder,
                                        const nsACString &aProperty)
{
  return NS_OK;
}

// nsINavHistoryObserver implementation

/* void onVisit(in nsIURI aURI, in PRInt64 aVisitID, in PRTime aTime,
                in PRInt64 aSessionID, in PRInt64 aReferringID,
                in PRUint32 aTransitionType); */
NS_IMETHODIMP
nsNavHistoryResultNode::OnVisit(nsIURI* aURI, PRInt64 aVisitID, PRTime aTime,
               PRInt64 aSessionID, PRInt64 aReferringID,
               PRUint32 aTransitionType)
{
  return NS_OK;
}

/* void onTitleChanged(in nsIURI aURI, in AString aPageTitle,
                       in AString aUserTitle, in PRBool aUserTitleChanged); */
NS_IMETHODIMP
nsNavHistoryResultNode::OnTitleChanged(nsIURI* aURI, const nsAString& aPageTitle,
                                      const nsAString& aUserTitle,
                                      PRBool aIsUserTitleChanged)
{
  return NS_OK;
}

/* void onDeleteURI(in nsIURI aURI); */
NS_IMETHODIMP
nsNavHistoryResultNode::OnDeleteURI(nsIURI *aURI)
{
  return NS_OK;
}

/* void onClearHistory(); */
NS_IMETHODIMP
nsNavHistoryResultNode::OnClearHistory()
{
  return NS_OK;
}

/* void onPageChanged(in nsIURI aURI, in PRUint32 aWhat, in AString aValue); */
NS_IMETHODIMP
nsNavHistoryResultNode::OnPageChanged(nsIURI *aURI,
                                      PRUint32 aWhat, const nsAString &aValue)
{
  // We're an item node in a bookmark folder.  Check if the given URL
  // matches ours, and rebuild the row if so.
  nsCAutoString spec;
  aURI->GetSpec(spec);
  if (! spec.Equals(mUrl))
    return NS_OK; // not ours

  // TODO(bryner): only rebuild if aProperty is being shown

  if (aWhat == nsINavHistoryObserver::ATTRIBUTE_FAVICON) {
    mFaviconURL = NS_ConvertUTF16toUTF8(aValue);
    return NS_OK;
  }

  Rebuild();
  return NS_OK;
}

nsNavHistoryResult*
nsNavHistoryResultNode::GetResult()
{
  nsNavHistoryResultNode *parent, *node = this;
  while ((parent = node->mParent)) {
    node = parent;
  }

  nsCOMPtr<nsINavHistoryResult> result = do_QueryInterface(node);
  NS_ASSERTION(result, "toplevel node must be a result");
  return NS_STATIC_CAST(nsNavHistoryResult*,
                        NS_STATIC_CAST(nsINavHistoryResult*, result));
}

nsresult
nsNavHistoryResultNode::Rebuild()
{
  if (mID == 0) {
    return NS_OK;
  }

  nsNavHistory *history = nsNavHistory::GetHistoryService();
  mozIStorageStatement *statement = history->DBGetIdPageInfoFull();
  mozStorageStatementScoper scope(statement);

  nsresult rv = statement->BindInt64Parameter(0, mID);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool results;
  rv = statement->ExecuteStep(&results);
  NS_ASSERTION(results, "node must be in history!");

  // get the concrete options class that generated this node
  nsCOMPtr<nsINavHistoryQueryOptions> optionsInterface;
  rv = GetResult()->GetQueryOptions(getter_AddRefs(optionsInterface));
  nsCOMPtr<nsNavHistoryQueryOptions> options =
    do_QueryInterface(optionsInterface, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  return history->FillURLResult(statement, options, this);
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
  nsCOMPtr<nsINavHistoryQueryOptions> options;
  nsresult rv = history->QueryStringToQueries(QueryURIToQuery(mUrl),
                                              &mQueries, &mQueryCount,
                                              getter_AddRefs(options));
  mOptions = do_QueryInterface(options);
  return rv;
}

PRInt64
nsNavHistoryQueryNode::FolderId() const
{
  PRInt64 id;
  if (mQueryCount > 0) {
    const nsTArray<PRInt64>& folders =
      NS_STATIC_CAST(nsNavHistoryQuery*, mQueries[0])->Folders();
    id = (folders.Length() == 1) ? folders[0] : 0;
  } else {
    id = 0;
  }
  return id;
}

/* attribute string folderType; */
NS_IMETHODIMP 
nsNavHistoryQueryNode::GetFolderType(nsAString& aFolderType)
{
  aFolderType = mFolderType;
  return NS_OK;
}

NS_IMETHODIMP
nsNavHistoryQueryNode::GetQueries(PRUint32 *aQueryCount,
                                  nsINavHistoryQuery ***aQueries)
{
  if (!mOptions) {
    nsresult rv = ParseQueries();
    NS_ENSURE_SUCCESS(rv, rv);
  }

  nsINavHistoryQuery **queries = nsnull;
  if (mQueryCount > 0) {
    queries = NS_STATIC_CAST(nsINavHistoryQuery**,
                             nsMemory::Alloc(mQueryCount * sizeof(nsINavHistoryQuery*)));
    NS_ENSURE_TRUE(queries, NS_ERROR_OUT_OF_MEMORY);

    for (PRUint32 i = 0; i < mQueryCount; ++i) {
      NS_ADDREF(queries[i] = mQueries[i]);
    }
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
nsNavHistoryQueryNode::BuildChildren(PRBool *aBuilt)
{
  if (mBuiltChildren) {
    *aBuilt = PR_FALSE;
    return NS_OK;
  }

  *aBuilt = PR_TRUE;
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

  mChildren.Clear();
  NS_ENSURE_TRUE(mChildren.AppendObjects(*(result->GetTopLevel())),
                 NS_ERROR_OUT_OF_MEMORY);
  mBuiltChildren = PR_TRUE;

  return NS_OK;
}

nsresult
nsNavHistoryQueryNode::UpdateQuery()
{
  mBuiltChildren = PR_FALSE;
  if (mExpanded) {
    nsNavHistoryResult *result = GetResult();
    nsresult rv = result->BuildChildrenFor(this);
    NS_ENSURE_SUCCESS(rv, rv);
    result->Invalidate();
  }
  return NS_OK;
}

// nsINavBookmarkObserver implementation

/* void onBeginUpdateBatch(); */
NS_IMETHODIMP
nsNavHistoryQueryNode::OnBeginUpdateBatch()
{
  return NS_OK;
}

/* void onEndUpdateBatch(); */
NS_IMETHODIMP
nsNavHistoryQueryNode::OnEndUpdateBatch()
{
  // TODO(bryner): Batch updates
  return NS_OK;
}

/* readonly attribute boolean wantAllDetails; */
NS_IMETHODIMP
nsNavHistoryQueryNode::GetWantAllDetails(PRBool *aResult)
{
  *aResult = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsNavHistoryQueryNode::GetChildrenReadOnly(PRBool *aResult)
{
  PRInt64 folderId = FolderId();
  if (folderId == 0) {
    *aResult = PR_TRUE;
    return NS_OK;
  }

  return nsNavBookmarks::GetBookmarksService()->GetFolderReadonly(folderId,
                                                                  aResult);
}

nsresult
nsNavHistoryQueryNode::CreateNode(nsIURI *aBookmark,
                                  nsNavHistoryResultNode **aNode)
{
  nsNavHistory *history = nsNavHistory::GetHistoryService();
  mozIStorageStatement *statement = history->DBGetURLPageInfoFull();
  mozStorageStatementScoper scope(statement);

  nsresult rv = BindStatementURI(statement, 0, aBookmark);
  NS_ENSURE_SUCCESS(rv, rv);

  PRBool results;
  rv = statement->ExecuteStep(&results);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ASSERTION(results, "item must be in history!");

  return history->RowToResult(statement, PR_FALSE, aNode);
}

PRBool
nsNavHistoryQueryNode::HasFilteredChildren() const
{
  if (mQueryCount != 1) {
    return PR_TRUE;
  }

  PRUint32 itemTypes;
  mQueries[0]->GetItemTypes(&itemTypes);
  return itemTypes != (nsINavHistoryQuery::INCLUDE_ITEMS |
                       nsINavHistoryQuery::INCLUDE_QUERIES);
}

/* void onItemAdded(in nsIURI bookmark, in PRInt64 folder, in PRInt32 index); */
NS_IMETHODIMP
nsNavHistoryQueryNode::OnItemAdded(nsIURI *aBookmark,
                                   PRInt64 aFolder, PRInt32 aIndex)
{
  nsresult rv;
  if (FolderId() == aFolder) {
    // If we're not expanded, we can just invalidate our child list
    // and rebuild it the next time we're opened.
    if (!mExpanded) {
      mBuiltChildren = PR_FALSE; // causes a rebuild on next open
      return NS_OK;
    }

    // If we're a special query type, just requery.
    if (HasFilteredChildren()) {
      rv = UpdateQuery();
      NS_ENSURE_SUCCESS(rv, rv);
    } else {
      // This node is expanded and is the target of this add, so
      // create a new result node and insert it.
      nsRefPtr<nsNavHistoryResultNode> node;
      rv = CreateNode(aBookmark, getter_AddRefs(node));
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_TRUE(mChildren.InsertObjectAt(node, aIndex),
                     NS_ERROR_OUT_OF_MEMORY);
      GetResult()->RowAdded(this, aIndex);
    }
  } else {
    for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
      rv = mChildren[i]->OnItemAdded(aBookmark, aFolder, aIndex);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

/* void onItemRemoved(in nsIURI bookmark, in PRInt64 folder, in PRInt32 index); */
NS_IMETHODIMP
nsNavHistoryQueryNode::OnItemRemoved(nsIURI *aBookmark,
                                     PRInt64 aFolder, PRInt32 aIndex)
{
  if (FolderId() == aFolder) {
    // If we're not expanded, we can just invalidate our child list
    // and rebuild it the next time we're opened.
    if (!mExpanded) {
      mBuiltChildren = PR_FALSE; // causes a rebuild on next open
      return NS_OK;
    }

    // If we're a special query type, just requery.
    if (HasFilteredChildren()) {
      return UpdateQuery();
    } else {
      // Remove the ResultNode for the URI
      PRInt32 index = mChildren[aIndex]->VisibleIndex();
      mChildren.RemoveObjectAt(aIndex);
      GetResult()->RowRemoved(index);
    }
  } else {
    for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
      nsresult rv = mChildren[i]->OnItemRemoved(aBookmark, aFolder, aIndex);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

/* void onItemMoved(in nsIURI bookmark, in PRInt64 folder,
                    in PRInt32 oldIndex, in PRInt32 newIndex); */
NS_IMETHODIMP
nsNavHistoryQueryNode::OnItemMoved(nsIURI *aBookmark, PRInt64 aFolder,
                                   PRInt32 aOldIndex, PRInt32 aNewIndex)
{
  if (FolderId() == aFolder) {
    // If we're not expanded, we can just invalidate our child list
    // and rebuild it the next time we're opened.
    if (!mExpanded) {
      mBuiltChildren = PR_FALSE; // causes a rebuild on next open
      return NS_OK;
    }

    // If we're a special query type, just requery.
    if (HasFilteredChildren()) {
      return UpdateQuery();
    } else {
      nsRefPtr<nsNavHistoryResultNode> node = mChildren[aOldIndex];
      PRInt32 delta = (aNewIndex > aOldIndex) ? 1 : -1;
      for (PRInt32 i = aOldIndex; i != aNewIndex; i += delta) {
        mChildren.ReplaceObjectAt(mChildren[i + delta], i);
      }
      mChildren.ReplaceObjectAt(node, aNewIndex);
      // TODO(bryner): we should only invalidate the affected rows
      GetResult()->Invalidate();
    }
  } else {
    for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
      nsresult rv = mChildren[i]->OnItemMoved(aBookmark, aFolder,
                                              aOldIndex, aNewIndex);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

/* void onItemChanged(in nsIURI bookmark, in ACString property, in AStirng value); */
NS_IMETHODIMP
nsNavHistoryQueryNode::OnItemChanged(nsIURI *aBookmark,
                                     const nsACString &aProperty,
                                     const nsAString &aValue)
{
  // We let OnPageChanged handle this case. FIXME: This should be able to do
  // all the work necessary from bookmark callbacks, so this needs to handle
  // all bookmark cases.
  return NS_OK;
}

/* void onItemVisited(in nsIURI bookmark, in PRTime time); */
NS_IMETHODIMP
nsNavHistoryQueryNode::OnItemVisited(nsIURI* aBookmark, PRTime aVisitTime)
{
  return NS_OK;
}

/* void onItemReplaced(in PRInt64 folder, in nsIURI item, in nsIURI newItem); */
NS_IMETHODIMP
nsNavHistoryQueryNode::OnItemReplaced(PRInt64 aFolder,
                                      nsIURI *aItem, nsIURI *aNewItem)
{
  nsresult rv;
  if (FolderId() == aFolder) {
    // If we're not expanded, we can just invalidate our child list
    // and rebuild it the next time we're opened.
    if (!mExpanded) {
      mBuiltChildren = PR_FALSE; // causes a rebuild on next open
      return NS_OK;
    }

    // If we're a special query type, just requery.
    if (HasFilteredChildren()) {
      return UpdateQuery();
    } else {
      // Replace the old node (aItem) with a new node for aNewItem.
      // We copy the indices from aItem since it is at the same location.
      nsRefPtr<nsNavHistoryResultNode> node;
      rv = CreateNode(aNewItem, getter_AddRefs(node));
      NS_ENSURE_SUCCESS(rv, rv);

      nsNavBookmarks *bookmarks = nsNavBookmarks::GetBookmarksService();
      PRInt32 index;
      bookmarks->IndexOfItem(aFolder, aNewItem, &index);

      PRInt32 visibleIndex = mChildren[index]->VisibleIndex();
      node->SetParent(this);
      node->SetVisibleIndex(visibleIndex);
      node->SetIndentLevel(mChildren[index]->IndentLevel());
      mChildren.ReplaceObjectAt(node, index);
      GetResult()->RowChanged(node->VisibleIndex());
    }
  } else {
    for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
      rv = mChildren[i]->OnItemReplaced(aFolder, aItem, aNewItem);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

/* void onFolderAdded(in PRInt64 folder, in PRInt64 parent, in PRInt32 index); */
NS_IMETHODIMP
nsNavHistoryQueryNode::OnFolderAdded(PRInt64 aFolder,
                                     PRInt64 aParent, PRInt32 aIndex)
{
  nsresult rv;
  if (FolderId() == aParent) {
    // If we're not expanded, we can just invalidate our child list
    // and rebuild it the next time we're opened.
    if (!mExpanded) {
      mBuiltChildren = PR_FALSE; // causes a rebuild on next open
      return NS_OK;
    }

    // If we're a special query type, just requery.
    if (HasFilteredChildren()) {
      return UpdateQuery();
    } else {
      // This node is expanded and is the target of this add, so
      // create a new result node and insert it.
      nsRefPtr<nsNavHistoryResultNode> node;
      rv = nsNavBookmarks::GetBookmarksService()->
        ResultNodeForFolder(aFolder, mQueries[0], mOptions,
                            getter_AddRefs(node));
      NS_ENSURE_SUCCESS(rv, rv);
      NS_ENSURE_TRUE(mChildren.InsertObjectAt(node, aIndex),
                     NS_ERROR_OUT_OF_MEMORY);
      GetResult()->RowAdded(this, aIndex);
    }
  } else {
    for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
      rv = mChildren[i]->OnFolderAdded(aFolder, aParent, aIndex);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

/* void onFolderRemoved(in PRInt64 folder, in PRInt64 parent, in PRInt32 index); */
NS_IMETHODIMP
nsNavHistoryQueryNode::OnFolderRemoved(PRInt64 aFolder,
                                       PRInt64 aParent, PRInt32 aIndex)
{
  if (FolderId() == aParent) {
    // If we're not expanded, we can just invalidate our child list
    // and rebuild it the next time we're opened.
    if (!mExpanded) {
      mBuiltChildren = PR_FALSE; // causes a rebuild on next open
      return NS_OK;
    }

    // If we're a special query type, just requery.
    if (HasFilteredChildren()) {
      return UpdateQuery();
    } else {
      // Remove the folder node from our child list.
      PRInt32 index = mChildren[aIndex]->VisibleIndex();
      mChildren.RemoveObjectAt(aIndex);
      GetResult()->RowRemoved(index);
    }
  } else {
    for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
      nsresult rv = mChildren[i]->OnFolderRemoved(aFolder, aParent, aIndex);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

/* void onFolderMoved(in PRInt64 folder,
                      in PRInt64 oldParent, in PRInt32 oldIndex,
                      in PRInt64 newParent, in PRInt32 newIndex); */
NS_IMETHODIMP
nsNavHistoryQueryNode::OnFolderMoved(PRInt64 aFolder,
                                     PRInt64 aOldParent, PRInt32 aOldIndex,
                                     PRInt64 aNewParent, PRInt32 aNewIndex)
{
  nsresult rv;
  PRInt64 nodeFolder = FolderId();

  if (aOldParent == aNewParent && aOldParent == nodeFolder) {
    // If we're not expanded, we can just invalidate our child list
    // and rebuild it the next time we're opened.
    if (!mExpanded) {
      mBuiltChildren = PR_FALSE; // causes a rebuild on next open
      return NS_OK;
    }

    // If we're a special query type, just requery.
    if (HasFilteredChildren()) {
      return UpdateQuery();
    } else {
      nsRefPtr<nsNavHistoryResultNode> node = mChildren[aOldIndex];
      PRInt32 delta = (aNewIndex > aOldIndex) ? 1 : -1;
      for (PRInt32 i = aOldIndex; i != aNewIndex; i += delta) {
        mChildren.ReplaceObjectAt(mChildren[i + delta], i);
      }
      mChildren.ReplaceObjectAt(node, aNewIndex);
      // TODO(bryner): we should only invalidate the affected rows
      GetResult()->Invalidate();
    }
  } else if (aOldParent == nodeFolder) {
    OnFolderRemoved(aFolder, aOldParent, aOldIndex);
  } else if (aNewParent == nodeFolder) {
    OnFolderAdded(aFolder, aNewParent, aNewIndex);
  } else {
    for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
      rv = mChildren[i]->OnFolderMoved(aFolder,
                                       aOldParent, aOldIndex,
                                       aNewParent, aNewIndex);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

/* void onFolderChanged(in PRInt64 folder, in ACString property); */
NS_IMETHODIMP
nsNavHistoryQueryNode::OnFolderChanged(PRInt64 aFolder,
                                       const nsACString &aProperty)
{
  if (FolderId() == aFolder) {
    // TODO(bryner): only rebuild if aProperty is being shown
    Rebuild();
  } else {
    for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
      nsresult rv = mChildren[i]->OnFolderChanged(aFolder, aProperty);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

// nsINavHistoryObserver implementation

/* void onVisit(in nsIURI aURI, in PRInt64 aVisitID, in PRTime aTime,
                in PRInt64 aSessionID, in PRInt64 aReferringID,
                in PRUint32 aTransitionType); */
NS_IMETHODIMP
nsNavHistoryQueryNode::OnVisit(nsIURI* aURI, PRInt64 aVisitID, PRTime aTime,
               PRInt64 aSessionID, PRInt64 aReferringID,
               PRUint32 aTransitionType)
{
  nsresult rv;

  // embedded transitions are not visible in queries unless you want to include
  // hidden ones, so we can ignore these notifications (which comprise the bulk
  // of history)
  if (aTransitionType == nsINavHistoryService::TRANSITION_EMBED &&
      ! mOptions->IncludeHidden())
    return NS_OK;

  if (FolderId() == 0) {
    // We're a non-folder query, so we need to requery.
    return UpdateQuery();
  } else {
    for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
      rv = mChildren[i]->OnVisit(aURI, aVisitID, aTime, aSessionID,
                                 aReferringID, aTransitionType);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

/* void onTitleChange */
NS_IMETHODIMP
nsNavHistoryQueryNode::OnTitleChanged(nsIURI* aURI, const nsAString& aPageTitle,
                                      const nsAString& aUserTitle,
                                      PRBool aIsUserTitleChanged)
{
  nsresult rv;
  PRInt64 folder = nsNavHistoryQueryNode::FolderId();
  if (folder == 0) {
    // If we're a query node (other than a folder), we need to re-execute
    // our queries in case aBookmark should be added/removed from the
    // results.
    return UpdateQuery();
  } else {
    // We're a bookmark folder.  Run through our children and notify them.
    for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
      rv = mChildren[i]->OnTitleChanged(aURI, aPageTitle, aUserTitle, aIsUserTitleChanged);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}


/* void onDeleteURI(in nsIURI aURI); */
NS_IMETHODIMP
nsNavHistoryQueryNode::OnDeleteURI(nsIURI *aURI)
{
  nsresult rv;
  if (FolderId() == 0) {
    // We're a non-folder query, so we need to requery.
    return UpdateQuery();
  } else {
    for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
      rv = mChildren[i]->OnDeleteURI(aURI);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

/* void onClearHistory(); */
NS_IMETHODIMP
nsNavHistoryQueryNode::OnClearHistory()
{
  nsresult rv;
  if (FolderId() == 0) {
    // We're a non-folder query, so we need to requery.
    return UpdateQuery();
  } else {
    for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
      rv = mChildren[i]->OnClearHistory();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

/* void onPageChanged(in nsIURI aURI, in PRUint32 aWhat, in AString aValue); */
NS_IMETHODIMP
nsNavHistoryQueryNode::OnPageChanged(nsIURI *aURI,
                                     PRUint32 aWhat, const nsAString &aValue)
{
  nsresult rv;
  PRInt64 folder = nsNavHistoryQueryNode::FolderId();
  if (folder == 0) {
    // If we're a query node (other than a folder), we need to re-execute
    // our queries in case aBookmark should be added/removed from the
    // results.
    return UpdateQuery();
  } else {
    // We're a bookmark folder.  Run through our children and notify them.
    for (PRInt32 i = 0; i < mChildren.Count(); ++i) {
      rv = mChildren[i]->OnPageChanged(aURI, aWhat, aValue);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  return NS_OK;
}

nsresult
nsNavHistoryQueryNode::Rebuild()
{
  PRInt64 folderId = FolderId();
  if (folderId != 0) {
    nsNavBookmarks *bookmarks = nsNavBookmarks::GetBookmarksService();
    return bookmarks->FillFolderNode(folderId, this);
  }
  return NS_OK;
}

// nsNavHistoryResult **********************************************************

NS_IMPL_ISUPPORTS_INHERITED5(nsNavHistoryResult, nsNavHistoryQueryNode,
                             nsINavHistoryResult,
                             nsITreeView,
                             nsINavBookmarkObserver,
                             nsINavHistoryObserver,
                             nsISupportsWeakReference)


// nsNavHistoryResult::nsNavHistoryResult

nsNavHistoryResult::nsNavHistoryResult(nsNavHistory* aHistoryService,
                                       nsIStringBundle* aHistoryBundle,
                                       nsINavHistoryQuery** aQueries,
                                       PRUint32 aQueryCount,
                                       nsNavHistoryQueryOptions* aOptions)
  : mBundle(aHistoryBundle), mHistoryService(aHistoryService),
    mCollapseDuplicates(PR_TRUE),
    mCurrentSort(nsINavHistoryQueryOptions::SORT_BY_NONE)
{
  NS_ASSERTION(aOptions, "must have options!");
  // Fill saved source queries with copies of the original (the caller might
  // change their original objects, and we always want to reflect the source
  // parameters).
  // XXX this should be in Init() since it could fail to alloc
  if (aQueryCount > 0) {
    mQueries = NS_STATIC_CAST(nsINavHistoryQuery**,
                              nsMemory::Alloc(aQueryCount * sizeof(nsINavHistoryQuery*)));

    for (PRUint32 i = 0; i < aQueryCount; i ++) {
      nsCOMPtr<nsINavHistoryQuery> queryClone;
      if (NS_SUCCEEDED(aQueries[i]->Clone(getter_AddRefs(queryClone)))) {
        mQueries[i] = nsnull;
        queryClone.swap(mQueries[i]);
      }
    }
    mQueryCount = aQueryCount;
  }
  if (aOptions)
    aOptions->Clone(getter_AddRefs(mOptions));

  PRInt64 folderId = 0;
  GetFolderId(&folderId);
  if (folderId != 0) {
    nsNavBookmarks::GetBookmarksService()->FillFolderNode(folderId, this);
  }

  mType = nsINavHistoryResult::RESULT_TYPE_QUERY;
  mVisibleIndex = -1;
  mExpanded = PR_TRUE; // the result itself can never be "collapsed"
}

// nsNavHistoryResult::~nsNavHistoryResult

nsNavHistoryResult::~nsNavHistoryResult()
{
  nsNavBookmarks::GetBookmarksService()->RemoveObserver(this);
  nsNavHistory::GetHistoryService()->RemoveObserver(this);
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

  // Hook up as an observer
  nsNavBookmarks::GetBookmarksService()->AddObserver(this, PR_TRUE);
  nsNavHistory::GetHistoryService()->AddObserver(this, PR_TRUE);

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
  PRBool built;
  nsresult rv = aNode->BuildChildren(&built);
  NS_ENSURE_SUCCESS(rv, rv);
  if (!built) {
    // No children needed to be built.
    return NS_OK;
  }

  PRInt32 flatIndex = mAllElements.IndexOf(aNode) + 1;
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


NS_IMETHODIMP 
nsNavHistoryResult::AddObserver(nsINavHistoryResultViewObserver* aObserver,
                                PRBool aOwnsWeak)
{
  return mObservers.AppendWeakElement(aObserver, aOwnsWeak);
}

NS_IMETHODIMP
nsNavHistoryResult::RemoveObserver(nsINavHistoryResultViewObserver* aObserver)
{
  return mObservers.RemoveWeakElement(aObserver);
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
  mDateFormatter->FormatPRTime(mLocale, dateFormat,
                               nsIScriptableDateFormat::timeFormatNoSeconds,
                               aTime, resultString);
  aResult = resultString;
  return NS_OK;
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
    } else {
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
              (node->mType == nsINavHistoryResult::RESULT_TYPE_QUERY &&
               (node->FolderId() > 0 ||
                (mOptions && mOptions->ExpandPlaces()))));
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
  PRUint32 count = mObservers.Length();
  for (PRUint32 i = 0; i < count; ++i) {
    const nsCOMPtr<nsINavHistoryResultViewObserver> &obs = mObservers[i];
    if (obs) {
      obs->CanDrop(index, orientation, _retval);
      if (*_retval) {
        break;
      }
    }
  }
  return NS_OK;
}


// nsNavHistoryResult::Drop (nsITreeView)
//
//    No drag-and-drop to history.

NS_IMETHODIMP nsNavHistoryResult::Drop(PRInt32 row, PRInt32 orientation)
{
  ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryResultViewObserver,
                      OnDrop(row, orientation))
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
  *_retval = VisibleElementAt(index)->mIndentLevel;
  return NS_OK;
}


// nsNavHistoryResult::GetImageSrc (nsITreeView)

NS_IMETHODIMP nsNavHistoryResult::GetImageSrc(PRInt32 row, nsITreeColumn *col,
                                              nsAString& _retval)
{
  if (row < 0 || row >= mVisibleElements.Count())
    return NS_ERROR_INVALID_ARG;

  // only the first column has an image
  PRInt32 colIndex;
  col->GetIndex(&colIndex);
  if (colIndex != 0) {
    _retval.Truncate(0);
    return NS_OK;
  }

  // Don't set the favicon for containers, we will use the default folder icon
  // The special icons for the bookmarks menu and toolbar are set in CSS from
  // attributes defined by GetCellProperties.
  PRBool isContainer;
  IsContainer(row, &isContainer);
  if (isContainer) {
    _retval.Truncate(0);
    return NS_OK;
  }

  nsFaviconService* faviconService = nsFaviconService::GetFaviconService();
  NS_ENSURE_TRUE(faviconService, NS_ERROR_NO_INTERFACE);

  nsCAutoString spec;
  faviconService->GetFaviconSpecForIconString(VisibleElementAt(row)->mFaviconURL, spec);
//  _retval = NS_ConvertUTF8toUTF16(spec);
  CopyUTF8toUTF16(spec, _retval);
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
      if (elt->mTime == 0 ||
          (elt->mType != nsINavHistoryResult::RESULT_TYPE_URL &&
           elt->mType != nsINavHistoryResult::RESULT_TYPE_VISIT)) {
        // hosts and days shouldn't have a value for the date column. Actually,
        // you could argue this point, but looking at the results, seeing the
        // most recently visited date is not what I expect, and gives me no
        // information I know how to use. Only show this for URLs and visits
        _retval.Truncate(0);
      } else {
        return FormatFriendlyTime(elt->mTime, _retval);
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

  ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryResultViewObserver,
                      OnToggleOpenState(index))

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
  ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryResultViewObserver,
                      OnCycleHeader(col))

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
  ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryResultViewObserver,
                      OnSelectionChanged())
  return NS_OK;
}

/* void cycleCell (in long row, in nsITreeColumn col); */
NS_IMETHODIMP nsNavHistoryResult::CycleCell(PRInt32 row, nsITreeColumn *col)
{
  ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryResultViewObserver,
                      OnCycleCell(row, col))
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
  ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryResultViewObserver,
                      OnPerformAction(action))
  return NS_OK;
}

/* void performActionOnRow (in wstring action, in long row); */
NS_IMETHODIMP nsNavHistoryResult::PerformActionOnRow(const PRUnichar *action, PRInt32 row)
{
  ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryResultViewObserver,
                      OnPerformActionOnRow(action, row))
  return NS_OK;
}

/* void performActionOnCell (in wstring action, in long row, in nsITreeColumn col); */
NS_IMETHODIMP nsNavHistoryResult::PerformActionOnCell(const PRUnichar *action, PRInt32 row, nsITreeColumn *col)
{
  ENUMERATE_WEAKARRAY(mObservers, nsINavHistoryResultViewObserver,
                      OnPerformActionOnCell(action, row, col))
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

void
nsNavHistoryResult::RowAdded(nsNavHistoryResultNode *aParent, PRInt32 aIndex)
{
  mAllElements.Clear();
  mVisibleElements.Clear();
  FilledAllResults();
  if (mTree) {
    mTree->RowCountChanged(aParent->ChildAt(aIndex)->mVisibleIndex, 1);
  }
}

void
nsNavHistoryResult::RowRemoved(PRInt32 aVisibleIndex)
{
  mAllElements.Clear();
  mVisibleElements.Clear();
  FilledAllResults();
  if (mTree) {
    mTree->RowCountChanged(aVisibleIndex, -1);
  }
}

void
nsNavHistoryResult::RowChanged(PRInt32 aVisibleIndex)
{
  if (mTree) {
    mTree->InvalidateRow(aVisibleIndex);
  }
}

void
nsNavHistoryResult::Invalidate()
{
  FillTreeStats(this, -1);
  RebuildList();
}
