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

  Implementation methods for the XUL checkbox element APIs.

*/

#include "nsCOMPtr.h"
#include "nsRDFCID.h"
#include "nsXULCheckboxElement.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsString.h"
//#include "nsIPopupSetFrame.h"
//#include "nsIMenuFrame.h"
//#include "nsIFrame.h"

NS_IMPL_ADDREF_INHERITED(nsXULCheckboxElement, nsXULAggregateElement);
NS_IMPL_RELEASE_INHERITED(nsXULCheckboxElement, nsXULAggregateElement);

nsresult
nsXULCheckboxElement::QueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(NS_GET_IID(nsIDOMXULCheckboxElement))) {
        *aResult = NS_STATIC_CAST(nsIDOMXULCheckboxElement*, this);
    }
    else {
        return nsXULAggregateElement::QueryInterface(aIID, aResult);
    }

    NS_ADDREF(NS_REINTERPRET_CAST(nsISupports*, *aResult));
    return NS_OK;
}

MOZ_DECL_CTOR_COUNTER(RDF_nsXULCheckboxElement);

nsXULCheckboxElement::nsXULCheckboxElement(nsIDOMXULElement* aOuter)
: nsXULAggregateElement(aOuter)
{
}

nsXULCheckboxElement::~nsXULCheckboxElement()
{
}

NS_IMETHODIMP
nsXULCheckboxElement::GetValue(nsString& aValue)
{
  return mOuter->GetAttribute("value", aValue);
}

NS_IMETHODIMP
nsXULCheckboxElement::SetValue(const nsString& aValue)
{
  return mOuter->SetAttribute("value", aValue);
}

NS_IMETHODIMP
nsXULCheckboxElement::GetCrop(nsString& aCrop)
{
  return mOuter->GetAttribute("crop", aCrop);
}

NS_IMETHODIMP
nsXULCheckboxElement::SetCrop(const nsString& aCrop)
{
  return mOuter->SetAttribute("crop", aCrop);
}

NS_IMETHODIMP
nsXULCheckboxElement::GetSrc(nsString& aSrc)
{
  return mOuter->GetAttribute("src", aSrc);
}

NS_IMETHODIMP
nsXULCheckboxElement::SetSrc(const nsString& aSrc)
{
  return mOuter->SetAttribute("src", aSrc);
}

NS_IMETHODIMP
nsXULCheckboxElement::GetImgalign(nsString& aImgalign)
{
  return mOuter->GetAttribute("imgalign", aImgalign);
}

NS_IMETHODIMP
nsXULCheckboxElement::SetImgalign(const nsString& aImgalign)
{
  return mOuter->SetAttribute("imgalign", aImgalign);
}

NS_IMETHODIMP
nsXULCheckboxElement::GetAccesskey(nsString& aAccesskey)
{
  return mOuter->GetAttribute("accesskey", aAccesskey);
}

NS_IMETHODIMP
nsXULCheckboxElement::SetAccesskey(const nsString& aAccesskey)
{
  return mOuter->SetAttribute("accesskey", aAccesskey);
}

NS_IMETHODIMP
nsXULCheckboxElement::GetChecked(PRBool* aChecked)
{
  nsAutoString value;
  mOuter->GetAttribute("checked", value);
  if(value == "true")
    *aChecked = PR_TRUE;
  else
    *aChecked = PR_FALSE;

  return NS_OK;
}

NS_IMETHODIMP
nsXULCheckboxElement::SetChecked(PRBool aChecked)
{
  PRBool disabled;
  GetDisabled(&disabled);
  if (disabled)
    return NS_OK;

  if(aChecked)
    mOuter->SetAttribute("checked", "true");
  else
    mOuter->RemoveAttribute("checked");

  return NS_OK;
}

NS_IMETHODIMP
nsXULCheckboxElement::GetDisabled(PRBool* aDisabled)
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
nsXULCheckboxElement::SetDisabled(PRBool aDisabled)
{
  if(aDisabled)
    mOuter->SetAttribute("disabled", "true");
  else
    mOuter->RemoveAttribute("disabled");

  return NS_OK;
}

