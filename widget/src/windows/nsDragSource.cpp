/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsDragSource.h"
#include "nsITransferable.h"
//#include "DROPSRC.h"


static NS_DEFINE_IID(kIDragSourceIID,   NS_IDRAGSOURCE_IID);

NS_IMPL_ADDREF(nsDragSource)
NS_IMPL_RELEASE(nsDragSource)

//-------------------------------------------------------------------------
//
// DragSource constructor
//
//-------------------------------------------------------------------------
nsDragSource::nsDragSource()
{
  NS_INIT_REFCNT();

  //mNativeDragSrc = new CfDropSource(this);
  //mNativeDragSrc->AddRef();

  mTransferable = nsnull;

}

//-------------------------------------------------------------------------
//
// DragSource destructor
//
//-------------------------------------------------------------------------
nsDragSource::~nsDragSource()
{
  //mNativeDragSrc->Release();
}

/**
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsDragSource::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_NOINTERFACE;

  if (aIID.Equals(kIDragSourceIID)) {
    *aInstancePtr = (void*) ((nsIDragSource*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return rv;
}


NS_IMETHODIMP nsDragSource::GetTransferable(nsITransferable ** aTransferable)
{
  if (nsnull != mTransferable) {
    *aTransferable = mTransferable;
    NS_ADDREF(mTransferable);
  } else {
    aTransferable = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP nsDragSource::SetTransferable(nsITransferable * aTransferable)
{
  NS_IF_RELEASE(mTransferable);
  mTransferable = aTransferable;
  NS_ADDREF(mTransferable);
  return NS_OK;
}

NS_IMETHODIMP nsDragSource:: DragStopped(nsIDraggedObject * aDraggedObj, PRInt32 anAction)
{
  return NS_OK;
}

