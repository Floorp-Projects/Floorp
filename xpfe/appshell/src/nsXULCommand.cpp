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
#include "nsIDOMNode.h"
#include "nsIMenuItem.h"
#include "nsDOMEvent.h"
#include "nsGUIEvent.h"

#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIPresContext.h"
#include "nsIContent.h"

#include "nsCOMPtr.h"

#include "nsIComponentManager.h"

#include "nsXULCommand.h"

#define DEBUG_MENUSDEL 1
//----------------------------------------------------------------------
nsXULCommand::nsXULCommand()
{
  NS_INIT_REFCNT();
  mMenuItem  = nsnull;
}

//----------------------------------------------------------------------
nsXULCommand::~nsXULCommand()
{
  //NS_IF_RELEASE(mMenuItem);
}

//----------------------------------------------------------------------
NS_IMPL_ADDREF(nsXULCommand)
NS_IMPL_RELEASE(nsXULCommand)

//----------------------------------------------------------------------
NS_IMPL_QUERY_INTERFACE2(nsXULCommand, nsIMenuListener, nsIXULCommand)


//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::GetDOMElement(nsIDOMElement ** aDOMElement)
{
  *aDOMElement = mDOMElement;
  return NS_OK;

}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::SetCommand(const nsString & aStrCmd)
{
  mCommandStr = aStrCmd;
  return NS_OK;
}


//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::SetMenuItem(nsIMenuItem * aMenuItem)
{
  mMenuItem = aMenuItem;
  //NS_ADDREF(mMenuItem);
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::AttributeHasBeenSet(const nsString & aAttr)
{
  nsAutoString value;
  mDOMElement->GetAttribute(aAttr, value);
  if (DEBUG_MENUSDEL) printf("New value is [%s]=[%s]\n", aAttr.ToNewCString(), value.ToNewCString());
  if (aAttr.EqualsWithConversion("disabled")) {
    mMenuItem->SetEnabled((PRBool)(!value.EqualsWithConversion("true")));
  }
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::DoCommand()
{
  nsresult rv = NS_ERROR_FAILURE;
 
  nsCOMPtr<nsIContentViewer> contentViewer;
  NS_ENSURE_SUCCESS(mDocShell->GetContentViewer(getter_AddRefs(contentViewer)),
   NS_ERROR_FAILURE);

  nsCOMPtr<nsIDocumentViewer> docViewer;
  docViewer = do_QueryInterface(contentViewer);
  if (!docViewer) {
      NS_ERROR("Document viewer interface not supported by the content viewer.");
      return rv;
  }

  nsCOMPtr<nsIPresContext> presContext;
  if (NS_FAILED(rv = docViewer->GetPresContext(*getter_AddRefs(presContext)))) {
      NS_ERROR("Unable to retrieve the doc viewer's presentation context.");
      return rv;
  }

  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event;
  event.eventStructType = NS_MOUSE_EVENT;
  event.message = NS_MENU_ACTION;

  nsCOMPtr<nsIContent> contentNode;
  contentNode = do_QueryInterface(mDOMElement);
  if (!contentNode) {
      NS_ERROR("DOM Node doesn't support the nsIContent interface required to handle DOM events.");
      return rv;
  }

  rv = contentNode->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);

  return rv;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::SetDocShell(nsIDocShell * aDocShell)
{
  mDocShell = aDocShell;
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::SetDOMElement(nsIDOMElement * aDOMElement)
{
  mDOMElement = aDOMElement;
  return NS_OK;
}

/////////////////////////////////////////////////////////////////////////
// nsIMenuListener Method(s)
/////////////////////////////////////////////////////////////////////////
nsEventStatus nsXULCommand::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  DoCommand();
  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus nsXULCommand::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  DoCommand();
  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus nsXULCommand::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  DoCommand();
  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus nsXULCommand::MenuConstruct(
    const nsMenuEvent & aMenuEvent,
    nsIWidget         * aParentWindow, 
    void              * menubarNode,
	void              * aWebShell)
{
  DoCommand();
  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus nsXULCommand::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  DoCommand();
  return nsEventStatus_eConsumeNoDefault;
}

