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

NS_IMPL_ISUPPORTS_INHERITED(nsXULTreeElement, nsXULElement, nsIDOMXULTreeElement);

nsXULTreeElement::nsXULTreeElement(nsIDOMXULElement* aOuter)
:nsXULElement(aOuter)
{
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

  mSelectedRows = children;

  children = nsnull;
  rv = nsRDFDOMNodeList::Create(&children);
  NS_ASSERTION(NS_SUCCEEDED(rv), "unable to create DOM node list");
  if (NS_FAILED(rv)) return;

  mSelectedCells = children;
}

nsXULTreeElement::~nsXULTreeElement()
{
  NS_IF_RELEASE(mSelectedItems);
  NS_IF_RELEASE(mSelectedRows);
  NS_IF_RELEASE(mSelectedCells);
}

NS_IMETHODIMP
nsXULTreeElement::GetSelectedItems(nsIDOMNodeList** aSelectedItems)
{
  NS_IF_ADDREF(mSelectedItems);
  *aSelectedItems = mSelectedItems;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::GetSelectedRows(nsIDOMNodeList** aSelectedRows)
{
  NS_IF_ADDREF(mSelectedRows);
  *aSelectedRows = mSelectedRows;
  return NS_OK;
}

NS_IMETHODIMP
nsXULTreeElement::GetSelectedCells(nsIDOMNodeList** aSelectedCells)
{
  NS_IF_ADDREF(mSelectedCells);
  *aSelectedCells = mSelectedCells;
  return NS_OK;
}
