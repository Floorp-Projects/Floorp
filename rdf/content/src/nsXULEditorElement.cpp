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

  Implementation methods for the XUL tree element APIs.

*/

#include "nsCOMPtr.h"
#include "nsRDFCID.h"
#include "nsXULEditorElement.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsString.h"

NS_IMPL_ADDREF_INHERITED(nsXULEditorElement, nsXULAggregateElement);
NS_IMPL_RELEASE_INHERITED(nsXULEditorElement, nsXULAggregateElement);

nsresult
nsXULEditorElement::QueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(nsCOMTypeInfo<nsIDOMXULEditorElement>::GetIID())) {
        *aResult = NS_STATIC_CAST(nsIDOMXULEditorElement*, this);
    }
    else {
        return nsXULAggregateElement::QueryInterface(aIID, aResult);
    }

    NS_ADDREF(NS_REINTERPRET_CAST(nsISupports*, *aResult));
    return NS_OK;
}

MOZ_DECL_CTOR_COUNTER(RDF_nsXULEditorElement);

nsXULEditorElement::nsXULEditorElement(nsIDOMXULElement* aOuter)
  : nsXULAggregateElement(aOuter)
{
}

nsXULEditorElement::~nsXULEditorElement()
{
}

NS_IMETHODIMP
nsXULEditorElement::GetEditorShell(nsIEditorShell** aEditorShell)
{
   NS_ENSURE_ARG_POINTER(aEditorShell);

   if(!mEditorShell)
      {
      NS_ENSURE_SUCCESS(nsComponentManager::CreateInstance("component://netscape/editor/editorshell",
                                            nsnull,
                                            NS_GET_IID(nsIEditorShell),
                                            getter_AddRefs(mEditorShell)), 
                                            NS_ERROR_FAILURE);
      }

   *aEditorShell = mEditorShell;
   NS_ADDREF(*aEditorShell);
   return NS_OK;
}
