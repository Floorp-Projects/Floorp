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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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


//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::InvokeDragSession (nsISupportsArray * anArrayTransferables, nsIScriptableRegion * aRegion, PRUint32 aActionType)
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
  if ( mDataObject->QueryGetData(&fe) == S_OK ) {
    // If it is the get the number of items in the collection
    nsDataObjCollection * dataObjCol = NS_STATIC_CAST(nsDataObjCollection*, mDataObject);
    if ( dataObjCol )
      *aNumItems = dataObjCol->GetNumDataObjects();
  }
  else {
    // Next check if we have a file drop. Return the number of files in
    // the file drop as the number of items we have, pretending like we
    // actually have > 1 drag item.
    FORMATETC fe2;
    SET_FORMATETC(fe2, CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);
    if ( mDataObject->QueryGetData(&fe2) == S_OK ) {
      STGMEDIUM stm;
      if ( mDataObject->GetData(&fe2, &stm) == S_OK ) {      
        HDROP hdrop = (HDROP) GlobalLock(stm.hGlobal);
        *aNumItems = ::DragQueryFile(hdrop, 0xFFFFFFFF, NULL, 0);
        ::GlobalUnlock(stm.hGlobal);
        ::ReleaseStgMedium(&stm);
      }
    }
    else
      *aNumItems = 1;
  }

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
      return nsClipboard::GetDataFromDataObject(mDataObject, anItem, nsnull, aTransferable);
    } else {
      return NS_ERROR_FAILURE;
    }
  }

  nsDataObjCollection * dataObjCol = NS_STATIC_CAST(nsDataObjCollection*, mDataObject);
  if ( dataObjCol ) {
    PRUint32 cnt = dataObjCol->GetNumDataObjects();   
    if (anItem >= 0 && anItem < cnt) {
      IDataObject * dataObj = dataObjCol->GetDataObjectAt(anItem);
      return nsClipboard::GetDataFromDataObject(dataObj, 0, nsnull, aTransferable);
    }
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

#ifdef NS_DEBUG
  if ( strcmp(aDataFlavor, kTextMime) == 0 )
    NS_WARNING ( "DO NOT USE THE text/plain DATA FLAVOR ANY MORE. USE text/unicode INSTEAD" );
#endif

  *_retval = PR_FALSE;

  // Check to see if the mDataObject is a collection of IDataObjects or just a 
  // single (which would be the case if something came from an external app).
  FORMATETC fe;
  UINT format = nsClipboard::GetFormat(MULTI_MIME);
  SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);

  if ( mDataObject->QueryGetData(&fe) == S_OK ) {
    // We know we have one of our special collection objects.
    format = nsClipboard::GetFormat(aDataFlavor);
    SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL | TYMED_FILE | TYMED_GDI);

    // See if any one of the IDataObjects in the collection supports this data type
    nsDataObjCollection* dataObjCol = NS_STATIC_CAST(nsDataObjCollection*, mDataObject);
    if ( dataObjCol ) {
      PRUint32 cnt = dataObjCol->GetNumDataObjects();
      for (PRUint32 i=0;i<cnt;++i) {
        IDataObject * dataObj = dataObjCol->GetDataObjectAt(i);
        if (S_OK == dataObj->QueryGetData(&fe))
          *_retval = PR_TRUE;             // found it!
      }
    }
  } // if special collection object
  else {
    // Ok, so we have a single object. Check to see if has the correct data type. Since
    // this can come from an outside app, we also need to see if we need to perform
    // text->unicode conversion if the client asked for unicode and it wasn't available.
    format = nsClipboard::GetFormat(aDataFlavor);
    SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL | TYMED_FILE | TYMED_GDI);
    if ( mDataObject->QueryGetData(&fe) == S_OK )
      *_retval = PR_TRUE;                 // found it!
    else {
      // try again, this time looking for plain text.
      format = nsClipboard::GetFormat(kTextMime);
      SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL | TYMED_FILE | TYMED_GDI);
      if ( mDataObject->QueryGetData(&fe) == S_OK )
        *_retval = PR_TRUE;                 // found it!
    } // else try again
  }

  return NS_OK;
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
