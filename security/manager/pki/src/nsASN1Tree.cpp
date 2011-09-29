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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Javier Delgadillo <javi@netscape.com>
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
#include "nsASN1Tree.h"
#include "nsIComponentManager.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsIMutableArray.h"
#include "nsArrayUtils.h"

NS_IMPL_THREADSAFE_ISUPPORTS2(nsNSSASN1Tree, nsIASN1Tree, 
                                                 nsITreeView)

nsNSSASN1Tree::nsNSSASN1Tree() 
:mTopNode(nsnull)
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
  mTopNode = nsnull;
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
    n->seq = nsnull;
    return;
  }

  nsCOMPtr<nsIMutableArray> asn1Objects;
  n->seq->GetASN1Objects(getter_AddRefs(asn1Objects));
  PRUint32 numObjects;
  asn1Objects->GetLength(&numObjects);
  
  if (!numObjects) {
    n->seq = nsnull;
    return;
  }
  
  myNode *walk = nsnull;
  myNode *prev = nsnull;
  
  PRUint32 i;
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
  PRInt32 rowsToDelete = 0;

  if (redraw) {
    // This is the number of rows we will be deleting after
    // the contents have changed.
    rowsToDelete = 0-CountVisibleNodes(mTopNode);
  }

  mASN1Object = asn1Object;
  InitNodes();

  if (redraw) {
    // The number of rows in the new content.
    PRInt32 newRows = CountVisibleNodes(mTopNode);
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
nsNSSASN1Tree::GetRowCount(PRInt32 *aRowCount)
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

/* void getRowProperties (in long index, in nsISupportsArray properties); */
NS_IMETHODIMP 
nsNSSASN1Tree::GetRowProperties(PRInt32 index, nsISupportsArray *properties)
{
  return NS_OK;
}

/* void getCellProperties (in long row, in nsITreeColumn col,
                           in nsISupportsArray properties); */
NS_IMETHODIMP 
nsNSSASN1Tree::GetCellProperties(PRInt32 row, nsITreeColumn* col, 
                                 nsISupportsArray *properties)
{
  return NS_OK;
}

/* void getColumnProperties (in nsITreeColumn col,
                             in nsISupportsArray properties); */
NS_IMETHODIMP 
nsNSSASN1Tree::GetColumnProperties(nsITreeColumn* col, 
                                   nsISupportsArray *properties)
{
  return NS_OK;
}

/* boolean isContainer (in long index); */
NS_IMETHODIMP 
nsNSSASN1Tree::IsContainer(PRInt32 index, bool *_retval)
{
  myNode *n = FindNodeFromIndex(index);
  if (!n)
    return NS_ERROR_FAILURE;

  *_retval = (n->seq != nsnull);
  return NS_OK; 
}

/* boolean isContainerOpen (in long index); */
NS_IMETHODIMP 
nsNSSASN1Tree::IsContainerOpen(PRInt32 index, bool *_retval)
{
  myNode *n = FindNodeFromIndex(index);
  if (!n || !n->seq)
    return NS_ERROR_FAILURE;

  n->seq->GetIsExpanded(_retval);
  return NS_OK;
}

/* boolean isContainerEmpty (in long index); */
NS_IMETHODIMP 
nsNSSASN1Tree::IsContainerEmpty(PRInt32 index, bool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

/* boolean isSeparator (in long index); */
NS_IMETHODIMP 
nsNSSASN1Tree::IsSeparator(PRInt32 index, bool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK; 
}

/* long getLevel (in long index); */
NS_IMETHODIMP 
nsNSSASN1Tree::GetLevel(PRInt32 index, PRInt32 *_retval)
{
  PRInt32 parentIndex;
  PRInt32 nodeLevel;

  myNode *n = FindNodeFromIndex(index, &parentIndex, &nodeLevel);
  if (!n)
    return NS_ERROR_FAILURE;

  *_retval = nodeLevel;
  return NS_OK; 
}

/* Astring getImageSrc (in long row, in nsITreeColumn col); */
NS_IMETHODIMP 
nsNSSASN1Tree::GetImageSrc(PRInt32 row, nsITreeColumn* col, 
                           nsAString& _retval)
{
  return NS_OK;
}

/* long getProgressMode (in long row, in nsITreeColumn col); */
NS_IMETHODIMP 
nsNSSASN1Tree::GetProgressMode(PRInt32 row, nsITreeColumn* col, PRInt32* _retval)
{
  return NS_OK;
}

/* Astring getCellValue (in long row, in nsITreeColumn col); */
NS_IMETHODIMP 
nsNSSASN1Tree::GetCellValue(PRInt32 row, nsITreeColumn* col, 
                            nsAString& _retval)
{
  return NS_OK;
}

/* Astring getCellText (in long row, in nsITreeColumn col); */
NS_IMETHODIMP 
nsNSSASN1Tree::GetCellText(PRInt32 row, nsITreeColumn* col, 
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
nsNSSASN1Tree::GetDisplayData(PRUint32 index, nsAString &_retval)
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
nsNSSASN1Tree::ToggleOpenState(PRInt32 index)
{
  myNode *n = FindNodeFromIndex(index);
  if (!n)
    return NS_ERROR_FAILURE;

  if (!n->seq)
    return NS_ERROR_FAILURE;

  bool IsExpanded;
  n->seq->GetIsExpanded(&IsExpanded);
  PRInt32 rowCountChange;
  if (IsExpanded) {
    rowCountChange = -CountVisibleNodes(n->child);
    n->seq->SetIsExpanded(PR_FALSE);
  } else {
    n->seq->SetIsExpanded(PR_TRUE);
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
nsNSSASN1Tree::CycleCell(PRInt32 row, nsITreeColumn* col)
{
  return NS_OK;
}

/* boolean isEditable (in long row, in nsITreeColumn col); */
NS_IMETHODIMP 
nsNSSASN1Tree::IsEditable(PRInt32 row, nsITreeColumn* col, 
                          bool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

/* boolean isSelectable (in long row, in nsITreeColumn col); */
NS_IMETHODIMP 
nsNSSASN1Tree::IsSelectable(PRInt32 row, nsITreeColumn* col, 
                            bool *_retval)
{
  *_retval = PR_FALSE;
  return NS_OK;
}

/* void setCellValue (in long row, in nsITreeColumn col, in AString value); */
NS_IMETHODIMP 
nsNSSASN1Tree::SetCellValue(PRInt32 row, nsITreeColumn* col, 
                            const nsAString& value)
{
  return NS_OK;
}

/* void setCellText (in long row, in nsITreeColumn col, in AString value); */
NS_IMETHODIMP 
nsNSSASN1Tree::SetCellText(PRInt32 row, nsITreeColumn* col, 
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
nsNSSASN1Tree::PerformActionOnRow(const PRUnichar *action, PRInt32 row)
{
  return NS_OK;
}

/* void performActionOnCell (in wstring action, in long row, in nsITreeColumn col); */
NS_IMETHODIMP 
nsNSSASN1Tree::PerformActionOnCell(const PRUnichar *action, PRInt32 row, 
                                   nsITreeColumn* col)
{
  return NS_OK;
}

//
// CanDrop
//
NS_IMETHODIMP nsNSSASN1Tree::CanDrop(PRInt32 index, PRInt32 orientation,
                                     nsIDOMDataTransfer* aDataTransfer, bool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);
  *_retval = PR_FALSE;
  
  return NS_OK;
}


//
// Drop
//
NS_IMETHODIMP nsNSSASN1Tree::Drop(PRInt32 row, PRInt32 orient, nsIDOMDataTransfer* aDataTransfer)
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
  *_retval = PR_FALSE;
  return NS_OK;
}


/* long getParentIndex (in long rowIndex); */
NS_IMETHODIMP 
nsNSSASN1Tree::GetParentIndex(PRInt32 rowIndex, PRInt32 *_retval)
{
  PRInt32 parentIndex = -1;

  myNode *n = FindNodeFromIndex(rowIndex, &parentIndex);
  if (!n)
    return NS_ERROR_FAILURE;

  *_retval = parentIndex;
  return NS_OK; 
}

/* boolean hasNextSibling (in long rowIndex, in long afterIndex); */
NS_IMETHODIMP 
nsNSSASN1Tree::HasNextSibling(PRInt32 rowIndex, PRInt32 afterIndex, 
                              bool *_retval)
{
  myNode *n = FindNodeFromIndex(rowIndex);
  if (!n)
    return NS_ERROR_FAILURE;

  if (!n->next) {
    *_retval = PR_FALSE;
  }
  else {
    PRInt32 nTotalSize = CountVisibleNodes(n);
    PRInt32 nLastChildPos = rowIndex + nTotalSize -1;
    PRInt32 nextSiblingPos = nLastChildPos +1;
    *_retval = (nextSiblingPos > afterIndex);
  }

  return NS_OK; 
}

PRInt32 nsNSSASN1Tree::CountVisibleNodes(myNode *n)
{
  if (!n)
    return 0;

  myNode *walk = n;
  PRInt32 count = 0;
  
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
nsNSSASN1Tree::FindNodeFromIndex(PRInt32 wantedIndex, 
                                 PRInt32 *optionalOutParentIndex, PRInt32 *optionalOutLevel)
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
    PRInt32 index = 0;
    PRInt32 level = 0;
    return FindNodeFromIndex(mTopNode, wantedIndex, index, level, 
                             optionalOutParentIndex, optionalOutLevel);
  }
}

// Internal recursive helper function
nsNSSASN1Tree::myNode *
nsNSSASN1Tree::FindNodeFromIndex(myNode *n, PRInt32 wantedIndex,
                                 PRInt32 &index_counter, PRInt32 &level_counter,
                                 PRInt32 *optionalOutParentIndex, PRInt32 *optionalOutLevel)
{
  if (!n)
    return nsnull;

  myNode *walk = n;
  PRInt32 parentIndex = index_counter-1;
  
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

  return nsnull;
}

