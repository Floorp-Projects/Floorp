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

#include "nsXPConnectFactory.h"
#include "nsRepository.h"
#include "nsXPComFactory.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMXPConnectFactory.h"




XPConnectFactoryImpl::XPConnectFactoryImpl()
{
  NS_INIT_REFCNT();

  mScriptObject  = nsnull;
}

XPConnectFactoryImpl::~XPConnectFactoryImpl()
{
}

// -----
// Implementation of nsISupports interface methods...
// -----

NS_IMPL_ADDREF(XPConnectFactoryImpl)
NS_IMPL_RELEASE(XPConnectFactoryImpl)
NS_IMPL_QUERY_INTERFACE2(XPConnectFactoryImpl, nsIDOMXPConnectFactory, nsIScriptObjectOwner)

// -----
// Implementation of nsIXPConnectFactory interface methods...
// -----

NS_IMETHODIMP
XPConnectFactoryImpl::CreateInstance(const nsString &contractID, nsISupports**_retval)
{
  nsresult rv;
  char *contractIdStr;

  // Argument validation...
  if (!_retval) {
    rv = NS_ERROR_NULL_POINTER;
    goto done;
  }

  *_retval = nsnull;
  contractIdStr = contractID.ToNewCString();

  if (contractIdStr) {
    rv = nsRepository::CreateInstance(contractIdStr, 
                                      nsnull,           // No Aggregration
                                      NS_GET_IID(nsISupports), 
                                      (void**)_retval);
    delete [] contractIdStr;
  } else {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }

done:
  return rv;
}

// -----
// Implementation of nsIScriptObjectOwner interface methods...
// -----

NS_IMETHODIMP
XPConnectFactoryImpl::GetScriptObject(nsIScriptContext *aContext, void **aScriptObject)
{
  nsresult rv = NS_OK;

  // Argument validation...
  if (!aScriptObject) {
    rv = NS_ERROR_NULL_POINTER;
  } else {
    if (!mScriptObject)  {
      nsIScriptGlobalObject *global = aContext->GetGlobalObject();
      rv = NS_NewScriptXPConnectFactory(aContext, (nsISupports *)(nsIDOMXPConnectFactory*)this, global, (void**)&mScriptObject);
      NS_IF_RELEASE(global);
    }

    *aScriptObject = mScriptObject;
  }

  return rv;
}

NS_IMETHODIMP
XPConnectFactoryImpl::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}

