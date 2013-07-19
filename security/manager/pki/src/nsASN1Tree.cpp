/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "nsASN1Tree.h"
#include "nsIComponentManager.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsIMutableArray.h"
#include "nsArrayUtils.h"

NS_IMPL_ISUPPORTS2(nsNSSASN1Tree, nsIASN1Tree, nsITreeView)

nsNSSASN1Tree::nsNSSASN1Tree() 
:mTopNode(nullptr)
{
}

nsNSSASN1Tree::~nsNSSASN1Tree()
{
  ClearNodes();
}

void nsNSSASN1Tree::ClearNodesRecursively(myNode *n)
{
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

void nsNSSASN1Tree::ClearNodes()
{
  ClearNodesRecursively(mTopNode);
  mTopNode = nullptr;
}

void nsNSSASN1Tree::InitChildsRecursively(myNode *n)
{
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
  
  uint32_t i;
  nsCOMPtr<nsISupports> isupports;
  for (i=0; i<numObjects; i++) {
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

void nsNSSASN1Tree::InitNodes()
{
  ClearNodes();

  mTopNode = new myNode;
  mTopNode->obj = mASN1Object;

  InitChildsRecursively(mTopNode);
}

/* void loadASN1Structure (in nsIASN1Object asn1Object); */
NS_IMETHODIMP 
nsNSSASN1Tree::LoadASN1Structure(nsIASN1Object *asn1Object)
{
  //
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

/* readonly attribute long rowCount; */
NS_IMETHODIMP 
nsNSSASN1Tree::GetRowCount(int32_t *aRowCount)
{
  if (mASN1Object) {
    *aRowCount = CountVisibleNodes(mTopNode);
  } else {
    *aRowCount = 0;
  }
  return NS_OK;
}

/* attribute nsITreeSelection selection; */
NS_IMETHODIMP 
nsNSSASN1Tree::GetSelection(nsITreeSelection * *aSelection)
{
  *aSelection = mSelection;
  NS_IF_ADDREF(*aSelection);
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Tree::SetSelection(nsITreeSelection * aSelection)
{
  mSelection = aSelection;
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Tree::GetRowProperties(int32_t index, nsAString& aProps)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Tree::GetCellProperties(int32_t row, nsITreeColumn* col, 
                                 nsAString& aProps)
{
  return NS_OK;
}

NS_IMETHODIMP 
nsNSSASN1Tree::GetColumnProperties(nsITreeColumn* col, nsAString& aProps)
{
  return NS_OK;
}

/* boolean isContainer (in long index); */
NS_IMETHODIMP 
nsNSSASN1Tree::IsContainer(int32_t index, bool *_retval)
{
  myNode *n = FindNodeFromIndex(index);
  if (!n)
    return NS_ERROR_FAILURE;

  *_retval = (n->seq != nullptr);
  return NS_OK; 
}

/* boolean isContainerOpen (in long index); */
NS_IMETHODIMP 
nsNSSASN1Tree::IsContainerOpen(int32_t index, bool *_retval)
{
  myNode *n = FindNodeFromIndex(index);
  if (!n || !n->seq)
    return NS_ERROR_FAILURE;

  n->seq->GetIsExpanded(_retval);
  return NS_OK;
}

/* boolean isContainerEmpty (in long index); */
NS_IMETHODIMP 
nsNSSASN1Tree::IsContainerEmpty(int32_t index, bool *_retval)
{
  *_retval = false;
  return NS_OK;
}

/* boolean isSeparator (in long index); */
NS_IMETHODIMP 
nsNSSASN1Tree::IsSeparator(int32_t index, bool *_retval)
{
  *_retval = false;
  return NS_OK; 
}

/* long getLevel (in long index); */
NS_IMETHODIMP 
nsNSSASN1Tree::GetLevel(int32_t index, int32_t *_retval)
{
  int32_t parentIndex;
  int32_t nodeLevel;

  myNode *n = FindNodeFromIndex(index, &parentIndex, &nodeLevel);
  if (!n)
    return NS_ERROR_FAILURE;

  *_retval = nodeLevel;
  return NS_OK; 
}

/* Astring getImageSrc (in long row, in nsITreeColumn col); */
NS_IMETHODIMP 
nsNSSASN1Tree::GetImageSrc(int32_t row, nsITreeColumn* col, 
                           nsAString& _retval)
{
  return NS_OK;
}

/* long getProgressMode (in long row, in nsITreeColumn col); */
NS_IMETHODIMP 
nsNSSASN1Tree::GetProgressMode(int32_t row, nsITreeColumn* col, int32_t* _retval)
{
  return NS_OK;
}

/* Astring getCellValue (in long row, in nsITreeColumn col); */
NS_IMETHODIMP 
nsNSSASN1Tree::GetCellValue(int32_t row, nsITreeColumn* col, 
                            nsAString& _retval)
{
  return NS_OK;
}

/* Astring getCellText (in long row, in nsITreeColumn col); */
NS_IMETHODIMP 
nsNSSASN1Tree::GetCellText(int32_t row, nsITreeColumn* col, 
                           nsAString& _retval)
{
  _retval.Truncate();

  myNode* n = FindNodeFromIndex(row);
  if (!n)
    return NS_ERROR_FAILURE;

  // There's only one column for ASN1 dump.
  return n->obj->GetDisplayName(_retval);
}

/* wstring getDisplayData (in unsigned long index); */
NS_IMETHODIMP 
nsNSSASN1Tree::GetDisplayData(uint32_t index, nsAString &_retval)
{
  myNode *n = FindNodeFromIndex(index);
  if (!n)
    return NS_ERROR_FAILURE;

  n->obj->GetDisplayValue(_retval);
  return NS_OK;
}

/* void setTree (in nsITreeBoxObject tree); */
NS_IMETHODIMP 
nsNSSASN1Tree::SetTree(nsITreeBoxObject *tree)
{
  mTree = tree;
  return NS_OK;
}

/* void toggleOpenState (in long index); */
NS_IMETHODIMP 
nsNSSASN1Tree::ToggleOpenState(int32_t index)
{
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

/* void cycleHeader (in nsITreeColumn col); */
NS_IMETHODIMP 
nsNSSASN1Tree::CycleHeader(nsITreeColumn* col)
{
  return NS_OK;
}

/* void selectionChanged (); */
NS_IMETHODIMP 
nsNSSASN1Tree::SelectionChanged()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

/* void cycleCell (in long row, in nsITreeColumn col); */
NS_IMETHODIMP 
nsNSSASN1Tree::CycleCell(int32_t row, nsITreeColumn* col)
{
  return NS_OK;
}

/* boolean isEditable (in long row, in nsITreeColumn col); */
NS_IMETHODIMP 
nsNSSASN1Tree::IsEditable(int32_t row, nsITreeColumn* col, 
                          bool *_retval)
{
  *_retval = false;
  return NS_OK;
}

/* boolean isSelectable (in long row, in nsITreeColumn col); */
NS_IMETHODIMP 
nsNSSASN1Tree::IsSelectable(int32_t row, nsITreeColumn* col, 
                            bool *_retval)
{
  *_retval = false;
  return NS_OK;
}

/* void setCellValue (in long row, in nsITreeColumn col, in AString value); */
NS_IMETHODIMP 
nsNSSASN1Tree::SetCellValue(int32_t row, nsITreeColumn* col, 
                            const nsAString& value)
{
  return NS_OK;
}

/* void setCellText (in long row, in nsITreeColumn col, in AString value); */
NS_IMETHODIMP 
nsNSSASN1Tree::SetCellText(int32_t row, nsITreeColumn* col, 
                           const nsAString& value)
{
  return NS_OK;
}

/* void performAction (in wstring action); */
NS_IMETHODIMP 
nsNSSASN1Tree::PerformAction(const PRUnichar *action)
{
  return NS_OK;
}

/* void performActionOnRow (in wstring action, in long row); */
NS_IMETHODIMP 
nsNSSASN1Tree::PerformActionOnRow(const PRUnichar *action, int32_t row)
{
  return NS_OK;
}

/* void performActionOnCell (in wstring action, in long row, in nsITreeColumn col); */
NS_IMETHODIMP 
nsNSSASN1Tree::PerformActionOnCell(const PRUnichar *action, int32_t row, 
                                   nsITreeColumn* col)
{
  return NS_OK;
}

//
// CanDrop
//
NS_IMETHODIMP nsNSSASN1Tree::CanDrop(int32_t index, int32_t orientation,
                                     nsIDOMDataTransfer* aDataTransfer, bool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = false;
  
  return NS_OK;
}


//
// Drop
//
NS_IMETHODIMP nsNSSASN1Tree::Drop(int32_t row, int32_t orient, nsIDOMDataTransfer* aDataTransfer)
{
  return NS_OK;
}


//
// IsSorted
//
// ...
//
NS_IMETHODIMP nsNSSASN1Tree::IsSorted(bool *_retval)
{
  *_retval = false;
  return NS_OK;
}


/* long getParentIndex (in long rowIndex); */
NS_IMETHODIMP 
nsNSSASN1Tree::GetParentIndex(int32_t rowIndex, int32_t *_retval)
{
  int32_t parentIndex = -1;

  myNode *n = FindNodeFromIndex(rowIndex, &parentIndex);
  if (!n)
    return NS_ERROR_FAILURE;

  *_retval = parentIndex;
  return NS_OK; 
}

/* boolean hasNextSibling (in long rowIndex, in long afterIndex); */
NS_IMETHODIMP 
nsNSSASN1Tree::HasNextSibling(int32_t rowIndex, int32_t afterIndex, 
                              bool *_retval)
{
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

int32_t nsNSSASN1Tree::CountVisibleNodes(myNode *n)
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
nsNSSASN1Tree::myNode *
nsNSSASN1Tree::FindNodeFromIndex(int32_t wantedIndex, 
                                 int32_t *optionalOutParentIndex, int32_t *optionalOutLevel)
{
  if (0 == wantedIndex) {
    if (optionalOutLevel) {
      *optionalOutLevel = 0;
    }
    if (optionalOutParentIndex) {
      *optionalOutParentIndex = -1;
    }
    return mTopNode;
  }
  else {
    int32_t index = 0;
    int32_t level = 0;
    return FindNodeFromIndex(mTopNode, wantedIndex, index, level, 
                             optionalOutParentIndex, optionalOutLevel);
  }
}

// Internal recursive helper function
nsNSSASN1Tree::myNode *
nsNSSASN1Tree::FindNodeFromIndex(myNode *n, int32_t wantedIndex,
                                 int32_t &index_counter, int32_t &level_counter,
                                 int32_t *optionalOutParentIndex, int32_t *optionalOutLevel)
{
  if (!n)
    return nullptr;

  myNode *walk = n;
  int32_t parentIndex = index_counter-1;
  
  while (walk) {
    if (index_counter == wantedIndex) {
      if (optionalOutLevel) {
        *optionalOutLevel = level_counter;
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
        ++index_counter; // set to walk->child

        ++level_counter;
        myNode *found = FindNodeFromIndex(walk->child, wantedIndex, index_counter, level_counter,
                                          optionalOutParentIndex, optionalOutLevel);
        --level_counter;

        if (found)
          return found;
      }
    }

    walk = walk->next;
    if (walk) {
      ++index_counter;
    }
  }

  return nullptr;
}

