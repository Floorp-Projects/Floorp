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

  Implementation methods for the XUL radio element APIs.

*/

#include "nsCOMPtr.h"
#include "nsRDFCID.h"
#include "nsXULRadioGroupElement.h"
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

NS_IMPL_ADDREF_INHERITED(nsXULRadioGroupElement, nsXULAggregateElement);
NS_IMPL_RELEASE_INHERITED(nsXULRadioGroupElement, nsXULAggregateElement);

nsresult
nsXULRadioGroupElement::QueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(NS_GET_IID(nsIDOMXULRadioGroupElement))) {
        *aResult = NS_STATIC_CAST(nsIDOMXULRadioGroupElement*, this);
    }
    else {
        return nsXULAggregateElement::QueryInterface(aIID, aResult);
    }

    NS_ADDREF(NS_REINTERPRET_CAST(nsISupports*, *aResult));
    return NS_OK;
}

MOZ_DECL_CTOR_COUNTER(RDF_nsXULRadioGroupElement);

nsXULRadioGroupElement::nsXULRadioGroupElement(nsIDOMXULElement* aOuter)
  : nsXULAggregateElement(aOuter)
{
}

nsXULRadioGroupElement::~nsXULRadioGroupElement()
{
}

NS_IMETHODIMP
nsXULRadioGroupElement::GetSelectedItem(nsIDOMXULRadioElement** aResult)
{
  *aResult = mSelectedItem;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXULRadioGroupElement::SetSelectedItem(nsIDOMXULRadioElement* aItem)
{
  PRBool disabled;
  aItem->GetDisabled(&disabled);
  if (disabled)
    return NS_OK;

  if (mSelectedItem)
    mSelectedItem->SetChecked(PR_FALSE);
  mSelectedItem = aItem;
  if (mSelectedItem)
    mSelectedItem->SetChecked(PR_TRUE);
  return NS_OK;
}
