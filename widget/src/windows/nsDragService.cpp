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
#include "nsITransferable.h"
#include "nsDataObj.h"

#include "nsWidgetsCID.h"
#include "nsNativeDragTarget.h"
#include "nsNativeDragSource.h"
#include "nsClipboard.h"
#include "nsIDataFlavor.h"
#include "nsISupportsArray.h"

#include <OLE2.h>
#include "OLEIDL.H"
#include "DDCOMM.h" // SET_FORMATETC macro


static NS_DEFINE_IID(kIDragServiceIID, NS_IDRAGSERVICE_IID);
static NS_DEFINE_IID(kIDragSessionIID, NS_IDRAGSESSION_IID);

NS_IMPL_ADDREF_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_RELEASE_INHERITED(nsDragService, nsBaseDragService)

//-------------------------------------------------------------------------
//
// DragService constructor
//
//-------------------------------------------------------------------------
nsDragService::nsDragService()
{
  NS_INIT_REFCNT();
  mNativeDragTarget = nsnull;
  mNativeDragSrc    = nsnull;
  mDataObject       = nsnull;
}

//-------------------------------------------------------------------------
//
// DragService destructor
//
//-------------------------------------------------------------------------
nsDragService::~nsDragService()
{
  NS_IF_RELEASE(mNativeDragSrc);
  NS_IF_RELEASE(mNativeDragTarget);
  NS_IF_RELEASE(mDataObject);
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

  nsresult rv = nsBaseDragService::QueryInterface(aIID, aInstancePtr);
  if (NS_OK == rv) {
    return NS_OK;
  }

  if (aIID.Equals(kIDragServiceIID)) {
    *aInstancePtr = (void*) ((nsIDragService*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(kIDragSessionIID)) {
    *aInstancePtr = (void*) ((nsIDragSession*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return rv;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::InvokeDragSession (nsISupportsArray * anArrayTransferables, nsIRegion * aRegion, PRUint32 aActionType)
{
  if (anArrayTransferables->Count() == 0) {
    return NS_ERROR_FAILURE;
  }

  // The clipboard class contains some static utility methods
  // that we can use to create an IDataObject from the transferable

  IDataObject * dataObj = nsnull;
  PRUint32 i;
  for (i=0;i<anArrayTransferables->Count();i++) {
    nsISupports * supports = anArrayTransferables->ElementAt(i);
    nsCOMPtr<nsITransferable> trans(do_QueryInterface(supports));
    NS_RELEASE(supports);
    if (i == 0) {
      nsClipboard::CreateNativeDataObject(trans, &dataObj);
    } else {
      nsClipboard::SetupNativeDataObject(trans, dataObj);
    }
    NS_RELEASE(supports);
  }

  StartInvokingDragSession(dataObj, aActionType);

  NS_IF_RELEASE(dataObj);

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::InvokeDragSessionSingle (nsITransferable * aTransferable,  nsIRegion * aRegion, PRUint32 aActionType)
{
  // The clipboard class contains some static utility methods
  // that we can use to create an IDataObject from the transferable
  IDataObject * dataObj = nsnull;
  nsClipboard::CreateNativeDataObject(aTransferable, &dataObj);

  StartInvokingDragSession(dataObj, aActionType);

  NS_IF_RELEASE(dataObj);

  return NS_OK;

}
//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::StartInvokingDragSession(IDataObject * aDataObj, PRUint32 aActionType)
{
  // To do the drag we need to create an object that 
  // implements the IDataObject interface (for OLE)
  NS_IF_RELEASE(mNativeDragSrc);
  mNativeDragSrc = (IDropSource *)new nsNativeDragSource();
  if (nsnull != mNativeDragSrc) {
    mNativeDragSrc->AddRef();
  }

  // Now figure out what the native drag effect should be
  DWORD dropRes;
  DWORD effects = DROPEFFECT_SCROLL;
  if (aActionType & DRAGDROP_ACTION_COPY) {
    effects |= DROPEFFECT_COPY;
  }
  if (aActionType & DRAGDROP_ACTION_MOVE) {
    effects |= DROPEFFECT_MOVE;
  }
  if (aActionType & DRAGDROP_ACTION_LINK) {
    effects |= DROPEFFECT_LINK;
  }

  mDragAction = aActionType;
  mDoingDrag  = PR_TRUE;

  // Call the native D&D method
  HRESULT res = ::DoDragDrop(aDataObj, mNativeDragSrc, effects, &dropRes);

  mDoingDrag  = PR_FALSE;

  return (DRAGDROP_S_DROP == res?NS_OK:NS_ERROR_FAILURE);
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::GetData (nsITransferable * aTransferable)
{
  // This typcially happens on a drop, the target would be asking
  // for it's transferable to be filled in
  // Use a static clipboard utiloity method for this
  if (nsnull != mDataObject) {
    return nsClipboard::GetDataFromDataObject(mDataObject, nsnull, aTransferable);
  }
  return NS_ERROR_FAILURE;
}

//---------------------------------------------------------
NS_IMETHODIMP nsDragService::SetIDataObject (IDataObject * aDataObj)
{
  // When the native drag starts the DragService gets the IDataObject
  // that is being dragged
  NS_IF_RELEASE(mDataObject);
  mDataObject = aDataObj;
  NS_ADDREF(mDataObject);

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::IsDataFlavorSupported(nsIDataFlavor * aDataFlavor)
{
  if (nsnull == aDataFlavor || nsnull == mDataObject) {
    return NS_ERROR_FAILURE;
  }

  //DWORD   types[]  = {TYMED_HGLOBAL, TYMED_FILE, TYMED_ISTREAM, TYMED_GDI};
  //PRInt32 numTypes = 4;

  nsString mime;
  aDataFlavor->GetMimeType(mime);

  UINT format = nsClipboard::GetFormat(mime);

  // XXX at the moment we only support global memory transfers
  // It is here where we will add support for native images 
  // and IStream and Files
  FORMATETC fe;
  //SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, -1,TYMED_HGLOBAL | TYMED_FILE | TYMED_ISTREAM | TYMED_GDI);
  SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL | TYMED_FILE | TYMED_GDI);

  // Starting by querying for the data to see if we can get it as from global memory
  if (S_OK == mDataObject->QueryGetData(&fe)) {
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::GetCurrentSession (nsIDragSession ** aSession)
{
  if (nsnull == aSession) {
    return NS_ERROR_FAILURE;
  }
  *aSession = (nsIDragSession *)this;
  NS_ADDREF(this);
  return NS_OK;
}
