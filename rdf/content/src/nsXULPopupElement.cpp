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
#include "nsXULPopupElement.h"
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

NS_IMPL_ADDREF_INHERITED(nsXULPopupElement, nsXULAggregateElement);
NS_IMPL_RELEASE_INHERITED(nsXULPopupElement, nsXULAggregateElement);

nsresult
nsXULPopupElement::QueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(NS_GET_IID(nsIDOMXULPopupElement))) {
        *aResult = NS_STATIC_CAST(nsIDOMXULPopupElement*, this);
    }
    else {
        return nsXULAggregateElement::QueryInterface(aIID, aResult);
    }

    NS_ADDREF(NS_REINTERPRET_CAST(nsISupports*, *aResult));
    return NS_OK;
}

MOZ_DECL_CTOR_COUNTER(RDF_nsXULPopupElement);

nsXULPopupElement::nsXULPopupElement(nsIDOMXULElement* aOuter)
  : nsXULAggregateElement(aOuter)
{
}

nsXULPopupElement::~nsXULPopupElement()
{
}

NS_IMETHODIMP
nsXULPopupElement::OpenPopup(nsIDOMElement* aElement, PRInt32 aXPos, PRInt32 aYPos, 
                             const nsString& aPopupType, const nsString& aAnchorAlignment, 
                             const nsString& aPopupAlignment)
{
  nsCOMPtr<nsIContent> content = do_QueryInterface(mOuter);
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
  if (!tagName.Equals("popupset") && !tagName.Equals("menu"))
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

  // Pass this all off to the popup set frame.
  nsCOMPtr<nsIPopupSetFrame> popupSetFrame = do_QueryInterface(frame);
  if (popupSetFrame)
    popupSetFrame->CreatePopup(elementFrame, content, aXPos, aYPos,
                               aPopupType, aAnchorAlignment,
                               aPopupAlignment);
  else {
    nsCOMPtr<nsIMenuFrame> menuFrame = do_QueryInterface(frame);
    menuFrame->OpenMenu(PR_TRUE);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsXULPopupElement::ClosePopup()
{
  // Close the popup by simulating proper dismissal.
  nsCOMPtr<nsIContent> content = do_QueryInterface(mOuter);
  nsCOMPtr<nsIDocument> document;
  content->GetDocument(*getter_AddRefs(document));

  // First we need to obtain the popup set frame that encapsulates the target popup.
  // Without a popup set, we're dead in the water.
  nsCOMPtr<nsIPresShell> presShell = getter_AddRefs(document->GetShellAt(0));
  if (!presShell)
    return NS_OK;

  // Retrieve the popup set.

  // Get the parent of the popup content.
  nsCOMPtr<nsIDOMNode> popupSet;
  mOuter->GetParentNode(getter_AddRefs(popupSet));
  if (!popupSet)
    return NS_OK;

  // Do a sanity check to ensure we have a popup set or menu element.
  nsString tagName;
  nsCOMPtr<nsIDOMElement> popupSetElement = do_QueryInterface(popupSet);
  popupSetElement->GetTagName(tagName);
  if (!tagName.Equals("popupset") && !tagName.Equals("menu"))
    return NS_OK;

  // Now obtain the popup set frame.
  nsCOMPtr<nsIContent> popupSetContent = do_QueryInterface(popupSet);
  nsIFrame* frame;
  presShell->GetPrimaryFrameFor(popupSetContent, &frame);  
  if (!frame)
    return NS_OK;

  nsCOMPtr<nsIPopupSetFrame> popupSetFrame = do_QueryInterface(frame);
  if (popupSetFrame)
    popupSetFrame->DestroyPopup();
  else {
    nsCOMPtr<nsIMenuFrame> menuFrame = do_QueryInterface(frame);
    menuFrame->OpenMenu(PR_FALSE);
  }

  return NS_OK;
}
