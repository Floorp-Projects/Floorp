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

#include "nsRepository.h"
#include "nsXPComFactory.h"
#include "nsIScriptObjectOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMXPConnectFactory.h"

// Declare IIDs...
static NS_DEFINE_IID(kIDOMXPConnectFactoryIID,  NS_IDOMXPCONNECTFACTORY_IID);
static NS_DEFINE_IID(kIScriptObjectOwnerIID,    NS_ISCRIPTOBJECTOWNER_IID);
static NS_DEFINE_IID(kISupportsIID,             NS_ISUPPORTS_IID);


class XPConnectFactoryImpl : public nsIDOMXPConnectFactory,
                             public nsIScriptObjectOwner
{
public:
  XPConnectFactoryImpl();

  NS_DECL_ISUPPORTS

  // nsIXPConnectFactory interface...
  NS_IMETHOD CreateInstance(const nsString &progID, nsISupports**_retval);

  // nsIScriptObjectOwner interface...
  NS_IMETHOD GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void *aScriptObject);

protected:
  virtual ~XPConnectFactoryImpl();

private:
  void *mScriptObject;
};


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

NS_IMETHODIMP 
XPConnectFactoryImpl::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if ( aIID.Equals(kIDOMXPConnectFactoryIID) ) {
    nsIDOMXPConnectFactory* tmp = this;
    *aInstancePtr = tmp;
    AddRef();
    return NS_OK;
  }
  else if ( aIID.Equals(kIScriptObjectOwnerIID)) {   
    nsIScriptObjectOwner* tmp = this;
    *aInstancePtr = tmp;
    AddRef();
    return NS_OK;
  }
  else if ( aIID.Equals(kISupportsIID) ) {
    nsISupports* tmp = (nsIDOMXPConnectFactory*)this;
    *aInstancePtr = tmp;
    AddRef();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

// -----
// Implementation of nsIXPConnectFactory interface methods...
// -----

NS_IMETHODIMP
XPConnectFactoryImpl::CreateInstance(const nsString &progID, nsISupports**_retval)
{
  nsresult rv;
  nsCID cid;
  char cidBuffer[48];

  // Argument validation...
  if (!_retval) {
    rv = NS_ERROR_NULL_POINTER;
    goto done;
  }

  // Convert the unicode string into a char*
  progID.ToCString(cidBuffer, sizeof(cidBuffer));

  // Parse the CID string...
  //
  // XXX: The progID should *not* be treated as a raw CID...
  //
  if (cid.Parse(cidBuffer)) {
    rv = nsRepository::CreateInstance(cid, nsnull, kISupportsIID, (void**)_retval);
    if (NS_SUCCEEDED(rv)) {
    }
  } else {
    // the progID does not represent a valid IID...
    rv = NS_ERROR_INVALID_ARG;
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



// -----
// Entry point to create nsXPConnectFactory factory instances...
// -----
NS_DEF_FACTORY(XPConnectFactory, XPConnectFactoryImpl)

nsresult NS_NewXPConnectFactoryFactory(nsIFactory** aResult)
{
  nsresult rv = NS_OK;
  nsIFactory* inst = new nsXPConnectFactoryFactory();
  if (nsnull == inst) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    NS_ADDREF(inst);
  }
  *aResult = inst;
  return rv;
}
