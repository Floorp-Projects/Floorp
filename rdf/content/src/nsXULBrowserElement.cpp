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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

/*

  Implementation methods for the XUL tree element APIs.

*/

#include "nsCOMPtr.h"
#include "nsRDFCID.h"
#include "nsXULBrowserElement.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsString.h"

NS_IMPL_ADDREF_INHERITED(nsXULBrowserElement, nsXULAggregateElement);
NS_IMPL_RELEASE_INHERITED(nsXULBrowserElement, nsXULAggregateElement);

nsresult
nsXULBrowserElement::QueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(NS_GET_IID(nsIDOMXULBrowserElement))) {
        *aResult = NS_STATIC_CAST(nsIDOMXULBrowserElement*, this);
    }
    else {
        return nsXULAggregateElement::QueryInterface(aIID, aResult);
    }

    NS_ADDREF(NS_REINTERPRET_CAST(nsISupports*, *aResult));
    return NS_OK;
}

MOZ_DECL_CTOR_COUNTER(RDF_nsXULBrowserElement);

nsXULBrowserElement::nsXULBrowserElement(nsIDOMXULElement* aOuter)
  : nsXULAggregateElement(aOuter)
{
}

nsXULBrowserElement::~nsXULBrowserElement()
{
}

NS_IMETHODIMP
nsXULBrowserElement::GetWebBrowser(nsIWebBrowser** aWebBrowser)
{
   NS_ENSURE_ARG_POINTER(aWebBrowser);

   nsCOMPtr<nsIContent> content(do_QueryInterface(mOuter));
   nsCOMPtr<nsIDocument> document;
   content->GetDocument(*getter_AddRefs(document));

   // First we need to obtain the popup set frame that encapsulates the target popup.
   // Without a popup set, we're dead in the water.
   nsCOMPtr<nsIPresShell> presShell = getter_AddRefs(document->GetShellAt(0));
   if(!presShell)
      return NS_OK;

   nsCOMPtr<nsISupports> subShell;
   presShell->GetSubShellFor(content, getter_AddRefs(subShell));
   if(!subShell)
      return NS_OK;

   CallQueryInterface(subShell, aWebBrowser);
   return NS_OK;
}
