/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsIDOMNode.h"
#include "nsIMenuItem.h"
#include "nsDOMEvent.h"
#include "nsGUIEvent.h"

#include "nsIContentViewerContainer.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIPresContext.h"
#include "nsIContent.h"

#include "nsCOMPtr.h"

#include "nsIComponentManager.h"

#include "nsXULCommand.h"

#define DEBUG_MENUSDEL 1
//----------------------------------------------------------------------

// Class IID's

// IID's
static NS_DEFINE_IID(kIDOMNodeIID,             NS_IDOMNODE_IID);
static NS_DEFINE_IID(kISupportsIID,            NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIXULCommandIID,          NS_IXULCOMMAND_IID);

//----------------------------------------------------------------------

nsXULCommand::nsXULCommand()
{
  NS_INIT_REFCNT();
  mMenuItem  = nsnull;

}

//----------------------------------------------------------------------
nsXULCommand::~nsXULCommand()
{
  NS_IF_RELEASE(mMenuItem);
}


NS_IMPL_ADDREF(nsXULCommand)
NS_IMPL_RELEASE(nsXULCommand)

//----------------------------------------------------------------------
nsresult
nsXULCommand::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  nsresult rv = NS_NOINTERFACE;

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  if (aIID.Equals(kIMenuListenerIID)) {
    *aInstancePtr = (void*)(nsIMenuListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIXULCommandIID)) {
    *aInstancePtr = (void*)(nsIXULCommand*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_ERROR_NO_INTERFACE;
}


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
  NS_ADDREF(mMenuItem);
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::AttributeHasBeenSet(const nsString & aAttr)
{
  nsAutoString value;
  mDOMElement->GetAttribute(aAttr, value);
  if (DEBUG_MENUSDEL) printf("New value is [%s]=[%s]\n", aAttr.ToNewCString(), value.ToNewCString());
  if (aAttr.Equals("disabled")) {
    mMenuItem->SetEnabled((PRBool)(!value.Equals("true")));
  }
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::DoCommand()
{
  nsresult rv = NS_ERROR_FAILURE;
 
  nsCOMPtr<nsIContentViewerContainer> contentViewerContainer;
  contentViewerContainer = do_QueryInterface(mWebShell);
  if (!contentViewerContainer) {
      NS_ERROR("Webshell doesn't support the content viewer container interface");
      return rv;
  }

  nsCOMPtr<nsIContentViewer> contentViewer;
  if (NS_FAILED(rv = contentViewerContainer->GetContentViewer(getter_AddRefs(contentViewer)))) {
      NS_ERROR("Unable to retrieve content viewer.");
      return rv;
  }

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
  event.message = NS_MOUSE_LEFT_CLICK;

  nsCOMPtr<nsIContent> contentNode;
  contentNode = do_QueryInterface(mDOMElement);
  if (!contentNode) {
      NS_ERROR("DOM Node doesn't support the nsIContent interface required to handle DOM events.");
      return rv;
  }

  rv = contentNode->HandleDOMEvent(*presContext, &event, nsnull, NS_EVENT_FLAG_INIT, status);

  return rv;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::SetWebShell(nsIWebShell * aWebShell)
{
  mWebShell = do_QueryInterface(aWebShell);
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::SetDOMElement(nsIDOMElement * aDOMElement)
{
  mDOMElement = do_QueryInterface(aDOMElement);
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::ExecuteJavaScriptString(nsIWebShell* aWebShell, nsString& aJavaScript)
{
  return NS_OK; // XXX Kill this method. It's worthless.
}

/////////////////////////////////////////////////////////////////////////
// nsIMenuListener Method(s)
/////////////////////////////////////////////////////////////////////////

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

nsEventStatus nsXULCommand::MenuConstruct(const nsMenuEvent & aMenuEvent)
{
  DoCommand();
  return nsEventStatus_eConsumeNoDefault;
}

nsEventStatus nsXULCommand::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  DoCommand();
  return nsEventStatus_eConsumeNoDefault;
}
