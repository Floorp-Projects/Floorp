/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

/*

  This file provides the implementation for the XUL "controllers"
  object.

*/

#include "nsIControllers.h"
#include "nsIDOMElement.h"
#include "nsIDOMXULCommandDispatcher.h"
#include "nsXULControllers.h"

//----------------------------------------------------------------------

nsXULControllers::nsXULControllers()
{
	NS_INIT_REFCNT();
}

nsXULControllers::~nsXULControllers(void)
{
}


NS_IMETHODIMP
NS_NewXULControllers(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aOuter == nsnull, "no aggregation");
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsXULControllers* controllers = new nsXULControllers();
    if (! controllers)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    NS_ADDREF(controllers);
    rv = controllers->QueryInterface(aIID, aResult);
    NS_RELEASE(controllers);
    return rv;
}

NS_IMPL_ISUPPORTS2(nsXULControllers, nsIControllers, nsISecurityCheckedComponent);


NS_IMETHODIMP
nsXULControllers::GetCommandDispatcher(nsIDOMXULCommandDispatcher** _result)
{
    nsCOMPtr<nsIDOMXULCommandDispatcher> dispatcher = do_QueryReferent(mCommandDispatcher);
    *_result = dispatcher;
    NS_IF_ADDREF(*_result);
    return NS_OK;
}


NS_IMETHODIMP
nsXULControllers::SetCommandDispatcher(nsIDOMXULCommandDispatcher* aCommandDispatcher)
{
    mCommandDispatcher = getter_AddRefs(NS_GetWeakReference(aCommandDispatcher));
    return NS_OK;
}

NS_IMETHODIMP
nsXULControllers::GetControllerForCommand(const PRUnichar *command, nsIController** _retval)
{
    *_retval = nsnull;
    if(!mControllers) 
        return NS_OK;

    PRUint32 count;
    mControllers->Count(&count);
    for(PRUint32 i=0; i < count; i++) {
        nsCOMPtr<nsIController> controller;
        nsCOMPtr<nsISupports>   supports;
        mControllers->GetElementAt(i, getter_AddRefs(supports));
        controller = do_QueryInterface(supports);
        if( controller ) {
            PRBool supportsCommand;
            controller->SupportsCommand(command, &supportsCommand);
            if(supportsCommand) {
                *_retval = controller;
                NS_ADDREF(*_retval);
                return NS_OK;
            }
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsXULControllers::InsertControllerAt(PRUint32 index, nsIController *controller)
{
    if(! mControllers ) {
        nsresult rv;
        rv = NS_NewISupportsArray(getter_AddRefs(mControllers));
        if (NS_FAILED(rv)) return rv;
    }

    mControllers->InsertElementAt(controller, index);
    return NS_OK;
}

NS_IMETHODIMP
nsXULControllers::RemoveControllerAt(PRUint32 index, nsIController **_retval)
{
    if(mControllers) {
        nsCOMPtr<nsISupports> supports;
        mControllers->GetElementAt(index, getter_AddRefs(supports));
        if (supports) {
           supports->QueryInterface(NS_GET_IID(nsIController), (void**)_retval);
           mControllers->RemoveElementAt(index);  
        } else {
            *_retval = nsnull;
        }
    } else
        *_retval = nsnull;
    
    return NS_OK;
}


NS_IMETHODIMP
nsXULControllers::GetControllerAt(PRUint32 index, nsIController **_retval)
{
    if(mControllers) {
        nsCOMPtr<nsISupports> supports;
        mControllers->GetElementAt(index, getter_AddRefs(supports));
        if (supports)
            supports->QueryInterface(NS_GET_IID(nsIController), (void**)_retval);
        else
            *_retval = nsnull;
    } else 
        *_retval = nsnull;
    
    return NS_OK;
}

NS_IMETHODIMP
nsXULControllers::AppendController(nsIController *controller)
{
    if(! mControllers ) {
        nsresult rv;
        rv = NS_NewISupportsArray(getter_AddRefs(mControllers));
        if (NS_FAILED(rv)) return rv;
    }
     
    mControllers->AppendElement(controller);
    return NS_OK;
}

NS_IMETHODIMP
nsXULControllers::RemoveController(nsIController *controller)
{
    if(mControllers) {
        nsCOMPtr<nsISupports> supports = do_QueryInterface(controller);
        mControllers->RemoveElement(supports);  
    }
    
    return NS_OK;
}

NS_IMETHODIMP
nsXULControllers::GetControllerCount(PRUint32 *_retval)
{
    *_retval = 0;
    if(mControllers) 
        mControllers->Count(_retval);
    
    return NS_OK;
}


/* string canCreateWrapper (in nsIIDPtr iid); */
NS_IMETHODIMP nsXULControllers::CanCreateWrapper(const nsIID * iid, char **_retval)
{
  nsCAutoString str("AllAccess");
  *_retval = str.ToNewCString();
  return NS_OK;
}

/* string canCallMethod (in nsIIDPtr iid, in wstring methodName); */
NS_IMETHODIMP nsXULControllers::CanCallMethod(const nsIID * iid, const PRUnichar *methodName, char **_retval)
{
  nsCAutoString str("AllAccess");
  *_retval = str.ToNewCString();
  return NS_OK;
}

/* string canGetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP nsXULControllers::CanGetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
  nsCAutoString str("AllAccess");
  *_retval = str.ToNewCString();
  return NS_OK;
}

/* string canSetProperty (in nsIIDPtr iid, in wstring propertyName); */
NS_IMETHODIMP nsXULControllers::CanSetProperty(const nsIID * iid, const PRUnichar *propertyName, char **_retval)
{
  nsCAutoString str("AllAccess");
  *_retval = str.ToNewCString();
  return NS_OK;
}
