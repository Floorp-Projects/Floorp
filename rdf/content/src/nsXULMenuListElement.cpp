/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*

  Implementation methods for the XULMenuList element APIs.

*/

#include "nsCOMPtr.h"
#include "nsRDFCID.h"
#include "nsXULMenuListElement.h"
#include "nsIDOMXULPopupElement.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsIPopupSetFrame.h"
#include "nsIMenuFrame.h"
#include "nsIFrame.h"

NS_IMPL_ADDREF_INHERITED(nsXULMenuListElement, nsXULAggregateElement);
NS_IMPL_RELEASE_INHERITED(nsXULMenuListElement, nsXULAggregateElement);

nsresult
nsXULMenuListElement::QueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(NS_GET_IID(nsIDOMXULMenuListElement))) {
        *aResult = NS_STATIC_CAST(nsIDOMXULMenuListElement*, this);
    }
    else {
        return nsXULAggregateElement::QueryInterface(aIID, aResult);
    }

    NS_ADDREF(NS_REINTERPRET_CAST(nsISupports*, *aResult));
    return NS_OK;
}

MOZ_DECL_CTOR_COUNTER(RDF_nsXULMenuListElement);

nsXULMenuListElement::nsXULMenuListElement(nsIDOMXULElement* aOuter)
  : nsXULAggregateElement(aOuter)
{
}

nsXULMenuListElement::~nsXULMenuListElement()
{
}

NS_IMETHODIMP
nsXULMenuListElement::GetValue(nsString& aValue)
{
  return mOuter->GetAttribute("value", aValue);
}

NS_IMETHODIMP
nsXULMenuListElement::SetValue(const nsString& aValue)
{
  return mOuter->SetAttribute("value", aValue);
}

NS_IMETHODIMP
nsXULMenuListElement::GetCrop(nsString& aCrop)
{
  return mOuter->GetAttribute("crop", aCrop);
}

NS_IMETHODIMP
nsXULMenuListElement::SetCrop(const nsString& aCrop)
{
  return mOuter->SetAttribute("crop", aCrop);
}

NS_IMETHODIMP
nsXULMenuListElement::GetSrc(nsString& aSrc)
{
  return mOuter->GetAttribute("src", aSrc);
}

NS_IMETHODIMP
nsXULMenuListElement::SetSrc(const nsString& aSrc)
{
  return mOuter->SetAttribute("src", aSrc);
}

NS_IMETHODIMP
nsXULMenuListElement::GetData(nsString& aData)
{
  return mOuter->GetAttribute("data", aData);
}

NS_IMETHODIMP
nsXULMenuListElement::SetData(const nsString& aData)
{
  return mOuter->SetAttribute("data", aData);
}

NS_IMETHODIMP
nsXULMenuListElement::GetDisabled(PRBool* aDisabled)
{
  nsAutoString value;
  mOuter->GetAttribute("disabled", value);
  if(value == "true")
    *aDisabled = PR_TRUE;
  else
    *aDisabled = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsXULMenuListElement::SetDisabled(PRBool aDisabled)
{
  if(aDisabled)
    mOuter->SetAttribute("disabled", "true");
  else
    mOuter->RemoveAttribute("disabled");

  return NS_OK;
}

static void
GetMenuChildrenElement(nsIContent* aParent, nsIContent** aResult)
{
  PRInt32 count;
  aParent->ChildCount(count);

  for (PRInt32 i = 0; i < count; i++) {
    nsCOMPtr<nsIContent> child;
    aParent->ChildAt(i, *getter_AddRefs(child));
    nsCOMPtr<nsIDOMXULPopupElement> menuPopup(do_QueryInterface(child));
    if (child) {
      *aResult = child.get();
      NS_ADDREF(*aResult);
      return;
    }
  }
}

NS_IMETHODIMP
nsXULMenuListElement::GetSelectedIndex(PRInt32* aResult)
{
  // XXX Quick and dirty.  Doesn't work with hierarchies, or
  // when other things are put in the <menupopup>.
  *aResult = -1;
  if (mSelectedItem) {
    nsCOMPtr<nsIContent> parent;
    nsCOMPtr<nsIContent> child(do_QueryInterface(mSelectedItem));
    child->GetParent(*getter_AddRefs(parent));
    parent->IndexOf(child, *aResult);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXULMenuListElement::SetSelectedIndex(PRInt32 anIndex)
{
  // XXX Quick and dirty.  Doesn't work with hierarchies, or
  // when other things are put in the <menupopup>.
  if (anIndex == -1)
    SetSelectedItem(nsnull);

  if (anIndex < 0)
    return NS_OK;

  nsCOMPtr<nsIContent> child;
  nsCOMPtr<nsIContent> parent(do_QueryInterface(mOuter));
  GetMenuChildrenElement(parent, getter_AddRefs(child));
  if (child) {
    PRInt32 count;
    child->ChildCount(count);
    if (anIndex >= count)
      return NS_OK;

    nsCOMPtr<nsIContent> item;
    child->ChildAt(anIndex, *getter_AddRefs(item));
    nsCOMPtr<nsIDOMElement> element(do_QueryInterface(item));
    SetSelectedItem(element);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsXULMenuListElement::GetSelectedItem(nsIDOMElement** aResult)
{
  if (!mSelectedItem) {
    nsAutoString value;
    GetValue(value);
  
    if (value == "") {
      nsCOMPtr<nsIContent> parent(do_QueryInterface(mOuter));
      nsCOMPtr<nsIContent> child;
      GetMenuChildrenElement(parent, getter_AddRefs(child));
      if (child) {
        PRInt32 count;
        child->ChildCount(count);
        PRInt32 i;
        for (i = 0; i < count; i++) {
          nsCOMPtr<nsIContent> item;
          child->ChildAt(i, *getter_AddRefs(item));
          nsCOMPtr<nsIDOMElement> selectedElement(do_QueryInterface(item));

          nsAutoString isSelected;
          selectedElement->GetAttribute(nsAutoString("selected"), isSelected);
          if (isSelected == "true") {
            SetSelectedItem(selectedElement);
            break;
          }
        }

        if (i == count && count > 0) {
          nsCOMPtr<nsIContent> item;
          child->ChildAt(0, *getter_AddRefs(item));
          nsCOMPtr<nsIDOMElement> selectedElement(do_QueryInterface(item));
          SetSelectedItem(selectedElement);  
        }
      }
    }
  }

  *aResult = mSelectedItem;
  NS_IF_ADDREF(*aResult);

  return NS_OK;
}

NS_IMETHODIMP
nsXULMenuListElement::SetSelectedItem(nsIDOMElement* aElement)
{
  if (mSelectedItem.get() == aElement)
    return NS_OK;

  if (mSelectedItem) {
    mSelectedItem->RemoveAttribute(nsAutoString("selected"));
  }

  mSelectedItem = aElement;
  
  if (!mSelectedItem)
    return NS_OK;

  mSelectedItem->SetAttribute(nsAutoString("selected"), nsAutoString("true"));

  nsAutoString value, src, data;
  aElement->GetAttribute(nsAutoString("value"), value);
  aElement->GetAttribute(nsAutoString("src"), src);
  aElement->GetAttribute(nsAutoString("data"), data);
  mOuter->SetAttribute(nsAutoString("value"), value);
  mOuter->SetAttribute(nsAutoString("src"), src);
  mOuter->SetAttribute(nsAutoString("data"), data);
  
  return NS_OK;
}

