/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsASN1Tree.h"

#include "mozilla/Assertions.h"
#include "nsArrayUtils.h"
#include "nsDebug.h"
#include "nsIMutableArray.h"
#include "nsString.h"

NS_IMPL_ISUPPORTS(nsNSSASN1Tree, nsIASN1Tree, nsITreeView)

nsNSSASN1Tree::nsNSSASN1Tree()
  : mTopNode(nullptr)
{
}

nsNSSASN1Tree::~nsNSSASN1Tree()
{
  ClearNodes();
}

void
nsNSSASN1Tree::ClearNodesRecursively(myNode* n)
{
  // Note: |n| is allowed to be null.

  myNode *walk = n;
  while (walk) {
    myNode *kill = walk;

    if (walk->child) {
      ClearNodesRecursively(walk->child);
    }

    walk = walk->next;
    delete kill;
  }
}

void
nsNSSASN1Tree::ClearNodes()
{
  ClearNodesRecursively(mTopNode);
  mTopNode = nullptr;
}

void
nsNSSASN1Tree::InitChildsRecursively(myNode* n)
{
  MOZ_ASSERT(n);
  if (!n) {
    return;
  }

  if (!n->obj)
    return;

  n->seq = do_QueryInterface(n->obj);
  if (!n->seq)
    return;

  // If the object is a sequence, there might still be a reason
  // why it should not be displayed as a container.
  // If we decide that it has all the properties to justify
  // displaying as a container, we will create a new child chain.
  // If we decide, it does not make sense to display as a container,
  // we forget that it is a sequence by erasing n->seq.
  // That way, n->seq and n->child will be either both set or both null.

  bool isContainer;
  n->seq->GetIsValidContainer(&isContainer);
  if (!isContainer) {
    n->seq = nullptr;
    return;
  }

  nsCOMPtr<nsIMutableArray> asn1Objects;
  n->seq->GetASN1Objects(getter_AddRefs(asn1Objects));
  uint32_t numObjects;
  asn1Objects->GetLength(&numObjects);
  if (!numObjects) {
    n->seq = nullptr;
    return;
  }

  myNode *walk = nullptr;
  myNode *prev = nullptr;
  for (uint32_t i = 0; i < numObjects; i++) {
    if (0 == i) {
      n->child = walk = new myNode;
    }
    else {
      walk = new myNode;
    }

    walk->parent = n;
    if (prev) {
      prev->next = walk;
    }

    walk->obj = do_QueryElementAt(asn1Objects, i);

    InitChildsRecursively(walk);

    prev = walk;
  }
}

void
nsNSSASN1Tree::InitNodes()
{
  ClearNodes();

  mTopNode = new myNode;
  mTopNode->obj = mASN1Object;

  InitChildsRecursively(mTopNode);
}

NS_IMETHODIMP
nsNSSASN1Tree::LoadASN1Structure(nsIASN1Object* asn1Object)
{
  // Note: |asn1Object| is allowed to be null.

  // The tree won't automatically re-draw if the contents
  // have been changed.  So I do a quick test here to let
  // me know if I should forced the tree to redraw itself
  // by calling RowCountChanged on it.
  //
  bool redraw = (mASN1Object && mTree);
  int32_t rowsToDelete = 0;

  if (redraw) {
    // This is the number of rows we will be deleting after
    // the contents have changed.
    rowsToDelete = 0-CountVisibleNodes(mTopNode);
  }

  mASN1Object = asn1Object;
  InitNodes();

  if (redraw) {
    // The number of rows in the new content.
    int32_t newRows = CountVisibleNodes(mTopNode);
    mTree->BeginUpdateBatch();
    // Erase all of the old rows.
    mTree->RowCountChanged(0, rowsToDelete);
    // Replace them with the new contents
    mTree->RowCountChanged(0, newRows);
    mTree->EndUpdateBatch();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::GetRowCount(int32_t* aRowCount)
{
  NS_ENSURE_ARG_POINTER(aRowCount);

  if (mASN1Object) {
    *aRowCount = CountVisibleNodes(mTopNode);
  } else {
    *aRowCount = 0;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::GetSelection(nsITreeSelection** aSelection)
{
  NS_ENSURE_ARG_POINTER(aSelection);
  *aSelection = mSelection;
  NS_IF_ADDREF(*aSelection);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::SetSelection(nsITreeSelection* aSelection)
{
  // Note: |aSelection| is allowed to be null.
  mSelection = aSelection;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::GetRowProperties(int32_t, nsAString&)
{
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::GetCellProperties(int32_t, nsTreeColumn*, nsAString&)
{
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::GetColumnProperties(nsTreeColumn*, nsAString&)
{
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::IsContainer(int32_t index, bool* _retval)
{
  NS_ENSURE_ARG_MIN(index, 0);
  NS_ENSURE_ARG_POINTER(_retval);

  myNode *n = FindNodeFromIndex(index);
  if (!n)
    return NS_ERROR_FAILURE;

  *_retval = (n->seq != nullptr);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::IsContainerOpen(int32_t index, bool* _retval)
{
  NS_ENSURE_ARG_MIN(index, 0);
  NS_ENSURE_ARG_POINTER(_retval);

  myNode *n = FindNodeFromIndex(index);
  if (!n || !n->seq)
    return NS_ERROR_FAILURE;

  return n->seq->GetIsExpanded(_retval);
}

NS_IMETHODIMP
nsNSSASN1Tree::IsContainerEmpty(int32_t, bool* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::IsSeparator(int32_t, bool* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::GetLevel(int32_t index, int32_t* _retval)
{
  NS_ENSURE_ARG_MIN(index, 0);
  NS_ENSURE_ARG_POINTER(_retval);

  int32_t nodeLevel;
  myNode* n = FindNodeFromIndex(index, nullptr, &nodeLevel);
  if (!n)
    return NS_ERROR_FAILURE;

  *_retval = nodeLevel;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::GetImageSrc(int32_t, nsTreeColumn*, nsAString&)
{
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::GetCellValue(int32_t, nsTreeColumn*, nsAString&)
{
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::GetCellText(int32_t row, nsTreeColumn*, nsAString& _retval)
{
  NS_ENSURE_ARG_MIN(row, 0);

  _retval.Truncate();

  myNode* n = FindNodeFromIndex(row);
  if (!n)
    return NS_ERROR_FAILURE;

  // There's only one column for ASN1 dump.
  return n->obj->GetDisplayName(_retval);
}

NS_IMETHODIMP
nsNSSASN1Tree::GetDisplayData(uint32_t index, nsAString& _retval)
{
  myNode *n = FindNodeFromIndex(index);
  if (!n)
    return NS_ERROR_FAILURE;

  return n->obj->GetDisplayValue(_retval);
}

NS_IMETHODIMP
nsNSSASN1Tree::SetTree(nsITreeBoxObject* tree)
{
  // Note: |tree| is allowed to be null.
  mTree = tree;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::ToggleOpenState(int32_t index)
{
  NS_ENSURE_ARG_MIN(index, 0);

  myNode *n = FindNodeFromIndex(index);
  if (!n)
    return NS_ERROR_FAILURE;

  if (!n->seq)
    return NS_ERROR_FAILURE;

  bool IsExpanded;
  n->seq->GetIsExpanded(&IsExpanded);
  int32_t rowCountChange;
  if (IsExpanded) {
    rowCountChange = -CountVisibleNodes(n->child);
    n->seq->SetIsExpanded(false);
  } else {
    n->seq->SetIsExpanded(true);
    rowCountChange = CountVisibleNodes(n->child);
  }
  if (mTree)
    mTree->RowCountChanged(index, rowCountChange);
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::CycleHeader(nsTreeColumn*)
{
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::SelectionChanged()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsNSSASN1Tree::CycleCell(int32_t, nsTreeColumn*)
{
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::IsEditable(int32_t, nsTreeColumn*, bool* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::IsSelectable(int32_t, nsTreeColumn*, bool* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::SetCellValue(int32_t, nsTreeColumn*, const nsAString&)
{
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::SetCellText(int32_t, nsTreeColumn*, const nsAString&)
{
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::PerformAction(const char16_t*)
{
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::PerformActionOnRow(const char16_t*, int32_t)
{
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::PerformActionOnCell(const char16_t*, int32_t, nsTreeColumn*)
{
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::CanDrop(int32_t, int32_t, mozilla::dom::DataTransfer*, bool* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::Drop(int32_t, int32_t, mozilla::dom::DataTransfer*)
{
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::IsSorted(bool* _retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::GetParentIndex(int32_t rowIndex, int32_t* _retval)
{
  NS_ENSURE_ARG_MIN(rowIndex, 0);
  NS_ENSURE_ARG_POINTER(_retval);

  int32_t parentIndex = -1;

  myNode *n = FindNodeFromIndex(rowIndex, &parentIndex);
  if (!n)
    return NS_ERROR_FAILURE;

  *_retval = parentIndex;
  return NS_OK;
}

NS_IMETHODIMP
nsNSSASN1Tree::HasNextSibling(int32_t rowIndex, int32_t afterIndex,
                              bool* _retval)
{
  NS_ENSURE_ARG_MIN(rowIndex, 0);
  NS_ENSURE_ARG_MIN(afterIndex, 0);
  NS_ENSURE_ARG_POINTER(_retval);

  myNode *n = FindNodeFromIndex(rowIndex);
  if (!n)
    return NS_ERROR_FAILURE;

  if (!n->next) {
    *_retval = false;
  }
  else {
    int32_t nTotalSize = CountVisibleNodes(n);
    int32_t nLastChildPos = rowIndex + nTotalSize -1;
    int32_t nextSiblingPos = nLastChildPos +1;
    *_retval = (nextSiblingPos > afterIndex);
  }

  return NS_OK;
}

int32_t
nsNSSASN1Tree::CountVisibleNodes(myNode* n)
{
  if (!n)
    return 0;

  myNode *walk = n;
  int32_t count = 0;
  while (walk) {
    ++count;

    if (walk->seq) {
      bool IsExpanded;
      walk->seq->GetIsExpanded(&IsExpanded);
      if (IsExpanded) {
        count += CountVisibleNodes(walk->child);
      }
    }

    walk = walk->next;
  }

  return count;
}

// Entry point for find
nsNSSASN1Tree::myNode*
nsNSSASN1Tree::FindNodeFromIndex(int32_t wantedIndex,
                                 int32_t* optionalOutParentIndex,
                                 int32_t* optionalOutLevel)
{
  MOZ_ASSERT(wantedIndex >= 0);
  if (wantedIndex < 0) {
    return nullptr;
  }

  if (0 == wantedIndex) {
    if (optionalOutLevel) {
      *optionalOutLevel = 0;
    }
    if (optionalOutParentIndex) {
      *optionalOutParentIndex = -1;
    }
    return mTopNode;
  }

  int32_t index = 0;
  int32_t level = 0;
  return FindNodeFromIndex(mTopNode, wantedIndex, index, level,
                           optionalOutParentIndex, optionalOutLevel);
}

// Internal recursive helper function
nsNSSASN1Tree::myNode*
nsNSSASN1Tree::FindNodeFromIndex(myNode* n, int32_t wantedIndex,
                                 int32_t& indexCounter, int32_t& levelCounter,
                                 int32_t* optionalOutParentIndex,
                                 int32_t* optionalOutLevel)
{
  MOZ_ASSERT(wantedIndex >= 0);
  MOZ_ASSERT(indexCounter >= 0);
  MOZ_ASSERT(levelCounter >= 0);
  if (!n || wantedIndex < 0 || indexCounter < 0 || levelCounter < 0) {
    return nullptr;
  }

  myNode *walk = n;
  int32_t parentIndex = indexCounter - 1;

  while (walk) {
    if (indexCounter == wantedIndex) {
      if (optionalOutLevel) {
        *optionalOutLevel = levelCounter;
      }
      if (optionalOutParentIndex) {
        *optionalOutParentIndex = parentIndex;
      }
      return walk;
    }

    if (walk->seq) {
      bool IsExpanded;
      walk->seq->GetIsExpanded(&IsExpanded);
      if (IsExpanded) {
        ++indexCounter; // set to walk->child

        ++levelCounter;
        myNode* found = FindNodeFromIndex(walk->child, wantedIndex, indexCounter,
                                          levelCounter, optionalOutParentIndex,
                                          optionalOutLevel);
        --levelCounter;

        if (found)
          return found;
      }
    }

    walk = walk->next;
    if (walk) {
      ++indexCounter;
    }
  }

  return nullptr;
}
