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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

/*

  Implementation methods for the XULMenuList element APIs.

*/

#include "nsCOMPtr.h"
#include "nsRDFCID.h"
#include "nsXULMenuListElement.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsString.h"
#include "nsXULAtoms.h"


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
nsXULMenuListElement::GetValue(nsAWritableString& aValue)
{
  return mOuter->GetAttribute(NS_ConvertASCIItoUCS2("value"), aValue);
}

NS_IMETHODIMP
nsXULMenuListElement::SetValue(const nsAReadableString& aValue)
{
  return mOuter->SetAttribute(NS_ConvertASCIItoUCS2("value"), aValue);
}

NS_IMETHODIMP
nsXULMenuListElement::GetCrop(nsAWritableString& aCrop)
{
  return mOuter->GetAttribute(NS_ConvertASCIItoUCS2("crop"), aCrop);
}

NS_IMETHODIMP
nsXULMenuListElement::SetCrop(const nsAReadableString& aCrop)
{
  return mOuter->SetAttribute(NS_ConvertASCIItoUCS2("crop"), aCrop);
}

NS_IMETHODIMP
nsXULMenuListElement::GetSrc(nsAWritableString& aSrc)
{
  return mOuter->GetAttribute(NS_ConvertASCIItoUCS2("src"), aSrc);
}

NS_IMETHODIMP
nsXULMenuListElement::SetSrc(const nsAReadableString& aSrc)
{
  return mOuter->SetAttribute(NS_ConvertASCIItoUCS2("src"), aSrc);
}

NS_IMETHODIMP
nsXULMenuListElement::GetData(nsAWritableString& aData)
{
  return mOuter->GetAttribute(NS_ConvertASCIItoUCS2("data"), aData);
}

NS_IMETHODIMP
nsXULMenuListElement::SetData(const nsAReadableString& aData)
{
  return mOuter->SetAttribute(NS_ConvertASCIItoUCS2("data"), aData);
}

NS_IMETHODIMP
nsXULMenuListElement::GetDisabled(PRBool* aDisabled)
{
  nsAutoString value;
  mOuter->GetAttribute(NS_ConvertASCIItoUCS2("disabled"), value);
  if(value.EqualsWithConversion("true"))
    *aDisabled = PR_TRUE;
  else
    *aDisabled = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsXULMenuListElement::SetDisabled(PRBool aDisabled)
{
  if(aDisabled)
    mOuter->SetAttribute(NS_ConvertASCIItoUCS2("disabled"), NS_ConvertASCIItoUCS2("true"));
  else
    mOuter->RemoveAttribute(NS_ConvertASCIItoUCS2("disabled"));

  return NS_OK;
}

static void
GetMenuChildrenElement(nsIContent* aParent, nsIContent** aResult)
{
  *aResult = nsnull;

  PRInt32 count;
  aParent->ChildCount(count);

  if (count == 0)
    return;
  
  nsCOMPtr<nsIContent> child;
  aParent->ChildAt(0, *getter_AddRefs(child));
  
  // make sure we're not working with a template, if we are, get the 1st item instead.
  nsCOMPtr<nsIAtom> tag;
  child->GetTag ( *getter_AddRefs(tag) );
  if ( tag.get() == nsXULAtoms::Template ) {
    if ( count == 1 )
      return;
    aParent->ChildAt(1, *getter_AddRefs(child));
  }
  
  *aResult = child;
  NS_IF_ADDREF(*aResult);
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
  
    if (value.IsEmpty()) {
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
          selectedElement->GetAttribute(NS_ConvertASCIItoUCS2("selected"), isSelected);
          if (isSelected.EqualsWithConversion("true")) {
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
    mSelectedItem->RemoveAttribute(NS_ConvertASCIItoUCS2("selected"));
  }

  mSelectedItem = aElement;
  
  if (!mSelectedItem) {
    // Remove all the old attributes.
    nsAutoString attribstr;
    attribstr.AssignWithConversion("value");
    mOuter->RemoveAttribute(attribstr);
    attribstr.AssignWithConversion("src");
    mOuter->RemoveAttribute(attribstr);
    attribstr.AssignWithConversion("data");
    mOuter->RemoveAttribute(attribstr);
    return NS_OK;
  }

  mSelectedItem->SetAttribute(NS_ConvertASCIItoUCS2("selected"), NS_ConvertASCIItoUCS2("true"));

  nsAutoString value, src, data;
  aElement->GetAttribute(NS_ConvertASCIItoUCS2("value"), value);
  aElement->GetAttribute(NS_ConvertASCIItoUCS2("src"), src);
  aElement->GetAttribute(NS_ConvertASCIItoUCS2("data"), data);
  mOuter->SetAttribute(NS_ConvertASCIItoUCS2("value"), value);
  mOuter->SetAttribute(NS_ConvertASCIItoUCS2("src"), src);
  mOuter->SetAttribute(NS_ConvertASCIItoUCS2("data"), data);
  
  return NS_OK;
}

