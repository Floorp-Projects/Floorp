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

#include "nsDragService.h"
#include "nsIDragSource.h"
#include "nsITransferable.h"
#include "nsDragSource.h"
#include "nsDataObj.h"
//#include "nsTransferable.h"

#include "OLEIDL.h"
#include "OLE2.h"

static NS_DEFINE_IID(kIDragServiceIID,   NS_IDRAGSERVICE_IID);

NS_IMPL_ADDREF(nsDragService)
NS_IMPL_RELEASE(nsDragService)

//-------------------------------------------------------------------------
//
// DragService constructor
//
//-------------------------------------------------------------------------
nsDragService::nsDragService()
{
  NS_INIT_REFCNT();
  mDragSource  = nsnull;

}

//-------------------------------------------------------------------------
//
// DragService destructor
//
//-------------------------------------------------------------------------
nsDragService::~nsDragService()
{
  NS_IF_RELEASE(mDragSource);
}

/**
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsDragService::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_NOINTERFACE;

  if (aIID.Equals(kIDragServiceIID)) {
    *aInstancePtr = (void*) ((nsIDragService*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return rv;
}


NS_IMETHODIMP nsDragService::StartDragSession (nsIDragSource * aDragSrc, 
                                           nsPoint       * aStartLocation, 
                                           nsPoint       * aImageOffset, 
                                           nsIImage      * aImage, 
                                           PRBool          aDoFlyback)

{
 /* NS_IF_RELEASE(mDragSource);
  mDragSource = aDragSrc;
  NS_ADDREF(mDragSource);

  nsITransferable * trans;
  mDragSource->GetTransferable(&trans);
  nsDataObj * dataObj = ((nsTransferable *)trans)->GetDataObj();
        
  DWORD dropRes;
  HRESULT res = 0;
  res = ::DoDragDrop((IDataObject*)dataObj,
                     (IDropSource *)((nsDragSource *)mDragSource)->GetNativeDragSrc(), 
                     DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_SCROLL, &dropRes);
                     */
  return NS_OK;
}

