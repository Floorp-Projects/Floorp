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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

/*

  Implementation methods for the XUL tree element APIs.

*/

#include "nsCOMPtr.h"
#include "nsRDFCID.h"
#include "nsXULIFrameElement.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsINameSpaceManager.h"
#include "nsIServiceManager.h"
#include "nsString.h"

NS_IMPL_ADDREF_INHERITED(nsXULIFrameElement, nsXULAggregateElement);
NS_IMPL_RELEASE_INHERITED(nsXULIFrameElement, nsXULAggregateElement);

nsresult
nsXULIFrameElement::QueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(NS_GET_IID(nsIDOMXULIFrameElement))) {
        *aResult = NS_STATIC_CAST(nsIDOMXULIFrameElement*, this);
    }
    else {
        return nsXULAggregateElement::QueryInterface(aIID, aResult);
    }

    NS_ADDREF(NS_REINTERPRET_CAST(nsISupports*, *aResult));
    return NS_OK;
}

MOZ_DECL_CTOR_COUNTER(RDF_nsXULIFrameElement);

nsXULIFrameElement::nsXULIFrameElement(nsIDOMXULElement* aOuter)
  : nsXULAggregateElement(aOuter)
{
}

nsXULIFrameElement::~nsXULIFrameElement()
{
}

NS_IMETHODIMP
nsXULIFrameElement::GetDocShell(nsIDocShell** aDocShell)
{
   NS_ENSURE_ARG_POINTER(aDocShell);

   NS_ERROR("Not Yet Implemented");

   return NS_OK;
/*

  nsresult rv = NS_OK;

  nsCOMPtr<nsIContent> content(do_QueryInterface(mOuter));
  nsCOMPtr<nsIDocument> document;
  content->GetDocument(*getter_AddRefs(document));

  // First we need to obtain the popup set frame that encapsulates the target popup.
  // Without a popup set, we're dead in the water.
  nsCOMPtr<nsIPresShell> presShell = getter_AddRefs(document->GetShellAt(0));
  if (!presShell)
    return NS_OK;

  // Get the parent of the popup content.
  nsCOMPtr<nsIDOMNode> popupSet;
  mOuter->GetParentNode(getter_AddRefs(popupSet));
  if (!popupSet)
    return NS_OK;

  // Do a sanity check to ensure we have a popup set or menu element.
  nsString tagName;
  nsCOMPtr<nsIDOMElement> popupSetElement = do_QueryInterface(popupSet);
  popupSetElement->GetTagName(tagName);
  if (tagName != "popupset" && tagName != "menu")
    return NS_OK;

  // Now obtain the popup set frame.
  nsCOMPtr<nsIContent> popupSetContent = do_QueryInterface(popupSet);
  nsIFrame* frame;
  presShell->GetPrimaryFrameFor(popupSetContent, &frame);  
  if (!frame)
    return NS_OK;

  // Obtain the element frame.
  nsCOMPtr<nsIContent> elementContent = do_QueryInterface(aElement);
  nsIFrame* elementFrame;
  presShell->GetPrimaryFrameFor(elementContent, &elementFrame);  
  if (!elementFrame)
    return NS_OK;

  //XXX Once we have the frame we need to get to the inner frame and then 
  // extract the object the inner frame is holding on to.
  
  nsCOMPtr<nsIHTMLInnerFrame> innerFrame;
  elementFrame->GetInnerFrame(getter_AddRefs(innerFrame));

  nsCOMPtr<nsIHTMLIFrameInnerFrame> iframeInnerFrame(do_QueryInterface(innerFrame));
  if(!iframeInnerFrame)
    return NS_OK;

  iframeInnerFrame->GetDocShell(aDocShell);

  return NS_OK; */
}
