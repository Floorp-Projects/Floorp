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
#include "nsISupportsArray.h"
#include "nsDataObjCollection.h"

#include <OLE2.h>
#include "OLEIDL.H"


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
// clean me up! ;)
nsresult nsDragService::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = nsBaseDragService::QueryInterface(aIID, aInstancePtr);
  if (NS_OK == rv) {
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIDragService))) {
    *aInstancePtr = (void*) ((nsIDragService*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIDragSession))) {
    *aInstancePtr = (void*) ((nsIDragSession*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return rv;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::InvokeDragSession (nsISupportsArray * anArrayTransferables, nsIRegion * aRegion, PRUint32 aActionType)
{
  nsresult rv;
  PRUint32 cnt;
  rv = anArrayTransferables->Count(&cnt);
  if (NS_FAILED(rv)) return rv;
  if (cnt == 0) {
    return NS_ERROR_FAILURE;
  }

  // The clipboard class contains some static utility methods
  // that we can use to create an IDataObject from the transferable

  nsDataObjCollection * dataObjCollection = new nsDataObjCollection();
  IDataObject * dataObj = nsnull;
  PRUint32 i;
  for (i=0;i<cnt;i++) {
    nsCOMPtr<nsISupports> supports;
    anArrayTransferables->GetElementAt(i, getter_AddRefs(supports));
    nsCOMPtr<nsITransferable> trans(do_QueryInterface(supports));
    if ( trans ) {
      nsClipboard::CreateNativeDataObject(trans, &dataObj);
      dataObjCollection->AddDataObject(dataObj);
      NS_IF_RELEASE(dataObj);
    }
  }

  StartInvokingDragSession((IDataObject *)dataObjCollection, aActionType);

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
NS_IMETHODIMP nsDragService::GetNumDropItems (PRUint32 * aNumItems)
{
  // First check to see if the mDataObject is is Collection of IDataObjects
  UINT format = nsClipboard::GetFormat(MULTI_MIME);
  FORMATETC fe;
  SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);

  if (S_OK != mDataObject->QueryGetData(&fe)) {
    *aNumItems = 1;
    return NS_OK;
  }

  // If it is the get the number of items in the collection
  nsDataObjCollection * dataObjCol = (nsDataObjCollection *)mDataObject;
  *aNumItems = (PRUint32)dataObjCol->GetNumDataObjects();
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::GetData (nsITransferable * aTransferable, PRUint32 anItem)
{
  // This typcially happens on a drop, the target would be asking
  // for it's transferable to be filled in
  // Use a static clipboard utility method for this
  if ( nsnull == mDataObject ) {
    return NS_ERROR_FAILURE;
  }

  // First check to see if the mDataObject is is Collection of IDataObjects
  UINT format = nsClipboard::GetFormat(MULTI_MIME);
  FORMATETC fe;
  SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);

  // if it is a single object then get the data for it
  if (S_OK != mDataObject->QueryGetData(&fe)) {
    // Since there is only one object, they better be asking for item "0"
    if (anItem == 0) {
      return nsClipboard::GetDataFromDataObject(mDataObject, nsnull, aTransferable);
    } else {
      return NS_ERROR_FAILURE;
    }
  }

  // If it is a collection of objects then get the data for the specified index
  nsDataObjCollection * dataObjCol = (nsDataObjCollection *)mDataObject;
  PRUint32 cnt = dataObjCol->GetNumDataObjects();
  if (anItem >= 0 && anItem < cnt) {
    IDataObject * dataObj = dataObjCol->GetDataObjectAt(anItem);
    return nsClipboard::GetDataFromDataObject(dataObj, nsnull, aTransferable);
  }

  return NS_ERROR_FAILURE;
}

//---------------------------------------------------------
NS_IMETHODIMP nsDragService::SetIDataObject (IDataObject * aDataObj)
{
  // When the native drag starts the DragService gets 
  // the IDataObject that is being dragged
  NS_IF_RELEASE(mDataObject);
  mDataObject = aDataObj;
  NS_ADDREF(mDataObject);

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::IsDataFlavorSupported(const char *aDataFlavor, PRBool *_retval)
{
  if ( !aDataFlavor || !mDataObject || !_retval )
    return NS_ERROR_FAILURE;

  // First check to see if the mDataObject is is Collection of IDataObjects
  UINT format = nsClipboard::GetFormat(MULTI_MIME);
  FORMATETC fe;
  SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);

  if (S_OK != mDataObject->QueryGetData(&fe)) {
    // Ok, so we have a single object
    // now check to see if has the correct data type
    format = nsClipboard::GetFormat(aDataFlavor);
    SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL | TYMED_FILE | TYMED_GDI);

    if (S_OK == mDataObject->QueryGetData(&fe)) {
      return NS_OK;
    } else {
      return NS_ERROR_FAILURE;
    }
  }

  // Set it up for the data flavor
  format = nsClipboard::GetFormat(aDataFlavor);
  SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL | TYMED_FILE | TYMED_GDI);

  // Ok, now see if any one of the IDataObjects in the collection
  // supports this data type
  nsDataObjCollection * dataObjCol = (nsDataObjCollection *)mDataObject;
  PRUint32 i;
  PRUint32 cnt = dataObjCol->GetNumDataObjects();
  for (i=0;i<cnt;i++) {
    IDataObject * dataObj = dataObjCol->GetDataObjectAt(i);
    if (S_OK == dataObj->QueryGetData(&fe)) {
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::GetCurrentSession (nsIDragSession ** aSession)
{
  if ( !aSession )
    return NS_ERROR_FAILURE;

  *aSession = (nsIDragSession *)this;
  NS_ADDREF(*aSession);
  return NS_OK;
}
