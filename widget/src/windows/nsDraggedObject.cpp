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

#include "nsDraggedObject.h"
#include "nsIDraggedObject.h"

static NS_DEFINE_IID(kIDraggedObjectIID,   NS_IDRAGGEDOBJECT_IID);

NS_IMPL_ADDREF(nsDraggedObject)
NS_IMPL_RELEASE(nsDraggedObject)

//-------------------------------------------------------------------------
//
// DraggedObject constructor
//
//-------------------------------------------------------------------------
nsDraggedObject::nsDraggedObject()
{
  NS_INIT_REFCNT();

}

//-------------------------------------------------------------------------
//
// DraggedObject destructor
//
//-------------------------------------------------------------------------
nsDraggedObject::~nsDraggedObject()
{
}

/**
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsDraggedObject::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_NOINTERFACE;

  if (aIID.Equals(kIDraggedObjectIID)) {
    *aInstancePtr = (void*) ((nsIDraggedObject*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return rv;
}


NS_IMETHODIMP nsDraggedObject::GetDragOffset(nscoord * aX, nscoord * aY)
{
  return NS_OK;
}

NS_IMETHODIMP nsDraggedObject::GetTransferable(nsITransferable ** aTransferable)
{
  return NS_OK;
}

NS_IMETHODIMP nsDraggedObject::GetSource(nsIDragSource ** aDragSrc)
{
  return NS_OK;
}

NS_IMETHODIMP nsDraggedObject::GetTarget(nsIDragTarget ** aTarget)
{
  return NS_OK;
}

NS_IMETHODIMP nsDraggedObject::GetTargetnFrame(nsIFrame ** aFrame)
{
  return NS_OK;
}

