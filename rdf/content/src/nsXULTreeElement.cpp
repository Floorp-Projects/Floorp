/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/*

  Implementation methods for the XUL tree element APIs.

*/

#include "nsCOMPtr.h"
#include "nsRDFCID.h"
#include "nsXULTreeElement.h"
#include "nsIContent.h"
#include "nsINameSpaceManager.h"

nsIAtom*             nsXULTreeElement::kSelectedAtom;
int                  nsXULTreeElement::gRefCnt = 0;

NS_IMPL_ISUPPORTS_INHERITED(nsXULTreeElement, nsXULElement, nsIDOMXULTreeElement);

nsXULTreeElement::nsXULTreeElement(nsIDOMXULElement* aOuter)
:nsXULElement(aOuter)
{
  if (gRefCnt++ == 0) {
    kSelectedAtom    = NS_NewAtom("selected");
  }

  nsresult rv;

  nsRDFDOMNodeList* children;
  rv = nsRDFDOMNodeList::Create(&children);
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create DOM node list");
  if (NS_FAILED(rv)) return;

  mSelectedItems = children;

  children = nsnull;
  rv = nsRDFDOMNodeList::Create(&children);
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create DOM node list");
  if (NS_FAILED(rv)) return;

  mSelectedCells = children;
}

nsXULTreeElement::~nsXULTreeElement()
{
  NS_IF_RELEASE(mSelectedItems);
  NS_IF_RELEASE(mSelectedCells);

  if (--gRefCnt == 0) {
    NS_IF_RELEASE(kSelectedAtom);
  }
}

NS_IMETHODIMP
nsXULTreeElement::GetSelectedItems(nsIDOMNodeList** aSelectedItems)
{
  NS_IF_ADDREF(mSelectedItems);
  *aSelectedItems = mSelectedItems;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::GetSelectedCells(nsIDOMNodeList** aSelectedCells)
{
  NS_IF_ADDREF(mSelectedCells);
  *aSelectedCells = mSelectedCells;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::SelectItem(nsIDOMXULElement* aTreeItem)
{
  // First clear our selection.
  ClearSelection();

  // Now add ourselves to the selection by setting our selected attribute.
  AddItemToSelection(aTreeItem);

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::SelectCell(nsIDOMXULElement* aTreeCell)
{
  // First clear our selection.
  ClearSelection();

  // Now add ourselves to the selection by setting our selected attribute.
  AddCellToSelection(aTreeCell);

  return NS_OK;
}

NS_IMETHODIMP    
nsXULTreeElement::ClearSelection()
{
  // Enumerate the elements and remove them from the selection.
  PRUint32 length;
  mSelectedItems->GetLength(&length);
  PRUint32 i;
  for (i = 0; i < length; i++) {
    nsCOMPtr<nsIDOMNode> node;
    mSelectedItems->Item(0, getter_AddRefs(node));
    nsCOMPtr<nsIContent> content = do_QueryInterface(node);
    content->UnsetAttribute(kNameSpaceID_None, kSelectedAtom, PR_TRUE);
  }
  
  mSelectedCells->GetLength(&length);
  for (i = 0; i < length; i++) {
    nsCOMPtr<nsIDOMNode> node;
    mSelectedCells->Item(0, getter_AddRefs(node));
    nsCOMPtr<nsIContent> content = do_QueryInterface(node);
    content->UnsetAttribute(kNameSpaceID_None, kSelectedAtom, PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::AddItemToSelection(nsIDOMXULElement* aTreeItem)
{
  // Without clearing the selection, perform the add.
  nsCOMPtr<nsIContent> content = do_QueryInterface(aTreeItem);
  content->SetAttribute(kNameSpaceID_None, kSelectedAtom, "true", PR_TRUE);

  return NS_OK;
}


NS_IMETHODIMP
nsXULTreeElement::RemoveItemFromSelection(nsIDOMXULElement* aTreeItem)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aTreeItem);
  content->UnsetAttribute(kNameSpaceID_None, kSelectedAtom, PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::AddCellToSelection(nsIDOMXULElement* aTreeCell)
{
  // Without clearing the selection, perform the add.
  nsCOMPtr<nsIContent> content = do_QueryInterface(aTreeCell);
  content->SetAttribute(kNameSpaceID_None, kSelectedAtom, "true", PR_TRUE);

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::RemoveCellFromSelection(nsIDOMXULElement* aTreeCell)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(aTreeCell);
  content->UnsetAttribute(kNameSpaceID_None, kSelectedAtom, PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::ToggleItemSelection(nsIDOMXULElement* aTreeItem)
{
  nsAutoString isSelected;
  aTreeItem->GetAttribute("selected", isSelected);
  if (isSelected == "true")
    RemoveItemFromSelection(aTreeItem);
  else AddItemToSelection(aTreeItem);

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::ToggleCellSelection(nsIDOMXULElement* aTreeCell)
{
  nsAutoString isSelected;
  aTreeCell->GetAttribute("selected", isSelected);
  if (isSelected == "true")
    RemoveItemFromSelection(aTreeCell);
  else AddItemToSelection(aTreeCell);

  return NS_OK;
}


NS_IMETHODIMP
nsXULTreeElement::SelectItemRange(nsIDOMXULElement* aStartItem, nsIDOMXULElement* aEndItem)
{
  // XXX Fill in.
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::SelectCellRange(nsIDOMXULElement* aStartItem, nsIDOMXULElement* aEndItem)
{
  // XXX Fill in.
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::SelectAll()
{
  // Do a clear.
  ClearSelection();

  // Now do an invert. That will select everything.
  InvertSelection();

  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::InvertSelection()
{
  // XXX Woo hoo. Write this later.
  // Yikes. Involves an enumeration of the whole tree.
  return NS_OK;
}
