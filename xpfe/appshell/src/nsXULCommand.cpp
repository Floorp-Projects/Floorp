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

// FOr JS Execution
#include "nsIScriptContextOwner.h"

#include "nsIComponentManager.h"

#include "nsXULCommand.h"

#define DEBUG_MENUSDEL 1
//----------------------------------------------------------------------

// Class IID's

// IID's
static NS_DEFINE_IID(kIDOMNodeIID,             NS_IDOMNODE_IID);
static NS_DEFINE_IID(kISupportsIID,            NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIScriptContextOwnerIID,  NS_ISCRIPTCONTEXTOWNER_IID);
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
  const PRUnichar * name;
  mWebShell->GetName( &name);
  nsAutoString str(name);

  if (DEBUG_MENUSDEL)
  {
	  char * cstr = str.ToNewCString();

      printf("DoCommand -  mWebShell is [%s] 0x%x\n",cstr, mWebShell);

	  if (cstr) delete [] cstr;
  }
  return ExecuteJavaScriptString(mWebShell, mCommandStr);
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
  if (0 == aJavaScript.Length()) {
    return NS_ERROR_FAILURE;
  }
  nsresult status;

  NS_ASSERTION(nsnull != aWebShell, "null webshell passed to EvaluateJavaScriptString");

  // Get nsIScriptContextOwner
  nsCOMPtr<nsIScriptContextOwner> scriptContextOwner ( do_QueryInterface(aWebShell) );
  if ( scriptContextOwner ) {
    const char* url = "";
      // Get nsIScriptContext
    nsCOMPtr<nsIScriptContext> scriptContext;
    status = scriptContextOwner->GetScriptContext(getter_AddRefs(scriptContext));
    if (NS_OK == status) {
      // Ask the script context to evalute the javascript string
      PRBool isUndefined = PR_FALSE;
      nsString rVal("xxx");
      scriptContext->EvaluateString(aJavaScript, url, 0, rVal, &isUndefined);
      if (DEBUG_MENUSDEL) printf("EvaluateString - %d [%s]\n", isUndefined, rVal.ToNewCString());
    }

  }
  return status;
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

