/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Pinkerton (pinkerton@netscape.com)
 *   Mark Hammond (MarkH@ActiveState.com)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
NS_IMETHODIMP nsDragService::InvokeDragSession (nsIDOMNode *aDOMNode, nsISupportsArray * anArrayTransferables, nsIScriptableRegion * aRegion, PRUint32 aActionType)
{
  nsBaseDragService::InvokeDragSession ( aDOMNode, anArrayTransferables, aRegion, aActionType );
  
  nsresult rv;
  PRUint32 numItemsToDrag = 0;
  rv = anArrayTransferables->Count(&numItemsToDrag);
  if ( !numItemsToDrag )
    return NS_ERROR_FAILURE;

  // The clipboard class contains some static utility methods
  // that we can use to create an IDataObject from the transferable

  // if we're dragging more than one item, we need to create a "collection" object to fake out
  // the OS. This collection contains one |IDataObject| for each transerable. If there is just
  // the one (most cases), only pass around the native |IDataObject|.
  IDataObject* itemToDrag = nsnull;
  if ( numItemsToDrag > 1 ) {
    nsDataObjCollection * dataObjCollection = new nsDataObjCollection();
    IDataObject* dataObj = nsnull;
    for ( PRUint32 i=0; i<numItemsToDrag; ++i ) {
      nsCOMPtr<nsISupports> supports;
      anArrayTransferables->GetElementAt(i, getter_AddRefs(supports));
      nsCOMPtr<nsITransferable> trans(do_QueryInterface(supports));
      if ( trans ) {
        if ( NS_SUCCEEDED(nsClipboard::CreateNativeDataObject(trans, &dataObj)) ) {
          dataObjCollection->AddDataObject(dataObj);
          NS_IF_RELEASE(dataObj);
        }
        else
          return NS_ERROR_FAILURE;
      }
    }
    itemToDrag = NS_STATIC_CAST ( IDataObject*, dataObjCollection );
  } // if dragging multiple items
  else {
    nsCOMPtr<nsISupports> supports;
    anArrayTransferables->GetElementAt(0, getter_AddRefs(supports));
    nsCOMPtr<nsITransferable> trans(do_QueryInterface(supports));
    if ( trans ) {
      if ( NS_FAILED(nsClipboard::CreateNativeDataObject(trans, &itemToDrag)) )
        return NS_ERROR_FAILURE;
    }
  } // else dragging a single object
  
  return StartInvokingDragSession ( itemToDrag, aActionType );
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::StartInvokingDragSession(IDataObject * aDataObj, PRUint32 aActionType)
{
  // To do the drag we need to create an object that 
  // implements the IDataObject interface (for OLE)
  NS_IF_RELEASE(mNativeDragSrc);
  mNativeDragSrc = (IDropSource *)new nsNativeDragSource();
  if ( !mNativeDragSrc )
    return NS_ERROR_OUT_OF_MEMORY;
    
  mNativeDragSrc->AddRef();

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

  mDragAction = aActionType;      //XXX not sure why we bother to cache this, it can change during the drag
  mDoingDrag  = PR_TRUE;

  // Call the native D&D method
  HRESULT res = ::DoDragDrop(aDataObj, mNativeDragSrc, effects, &dropRes);

  mDoingDrag  = PR_FALSE;

  return (DRAGDROP_S_DROP == res?NS_OK:NS_ERROR_FAILURE);
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::GetNumDropItems (PRUint32 * aNumItems)
{
  if ( IsCollectionObject(mDataObject) ) {
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
  if ( !mDataObject )
    return NS_ERROR_FAILURE;

  nsresult dataFound = NS_ERROR_FAILURE;

  if ( IsCollectionObject(mDataObject) ) {
    // multiple items, use |anItem| as an index into our collection
    nsDataObjCollection * dataObjCol = NS_STATIC_CAST(nsDataObjCollection*, mDataObject);
    PRUint32 cnt = dataObjCol->GetNumDataObjects();   
    if (anItem >= 0 && anItem < cnt) {
      IDataObject * dataObj = dataObjCol->GetDataObjectAt(anItem);
      dataFound = nsClipboard::GetDataFromDataObject(dataObj, 0, nsnull, aTransferable);
    }
    else
      NS_WARNING ( "Index out of range!" );
  }
  else {
    // If they are asking for item "0", we can just get it...
    if (anItem == 0) 
       dataFound = nsClipboard::GetDataFromDataObject(mDataObject, anItem, nsnull, aTransferable);
    else {
      // It better be a file drop, or else non-zero indexes are invalid!
      FORMATETC fe2;
      SET_FORMATETC(fe2, CF_HDROP, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);
      if ( mDataObject->QueryGetData(&fe2) == S_OK )
        dataFound = nsClipboard::GetDataFromDataObject(mDataObject, anItem, nsnull, aTransferable);
      else
        NS_WARNING ( "Reqesting non-zero index, but clipboard data is not a collection!" );
    }
  }
  return dataFound;
}

//---------------------------------------------------------
NS_IMETHODIMP nsDragService::SetIDataObject (IDataObject * aDataObj)
{
  // When the native drag starts the DragService gets 
  // the IDataObject that is being dragged
  NS_IF_RELEASE(mDataObject);
  mDataObject = aDataObj;
  NS_IF_ADDREF(mDataObject);

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

  FORMATETC fe;
  UINT format = 0;
  
  if ( IsCollectionObject(mDataObject) ) {
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
      // We haven't found the exact flavor the client asked for, but maybe we can
      // still find it from something else that's on the clipboard
      if ( strcmp(aDataFlavor, kUnicodeMime) == 0 ) {
        // client asked for unicode and it wasn't present, check if we have CF_TEXT.
        // We'll handle the actual data substitution in the data object.
        format = nsClipboard::GetFormat(kTextMime);
        SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL | TYMED_FILE | TYMED_GDI);
        if ( mDataObject->QueryGetData(&fe) == S_OK )
          *_retval = PR_TRUE;                 // found it!
      }
      else if ( strcmp(aDataFlavor, kURLMime) == 0 ) {
        // client asked for a url and it wasn't present, but if we have a file, then
        // we have a URL to give them (the path, or the internal URL if an InternetShortcut).
        format = nsClipboard::GetFormat(kFileMime);
        SET_FORMATETC(fe, format, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL | TYMED_FILE | TYMED_GDI);
        if ( mDataObject->QueryGetData(&fe) == S_OK )
          *_retval = PR_TRUE;                 // found it!
      }
    } // else try again
  }

  return NS_OK;
}


//
// IsCollectionObject
//
// Determine if this is a single |IDataObject| or one of our private collection
// objects. We know the difference because our collection object will respond to supporting
// the private |MULTI_MIME| format.
//
PRBool
nsDragService :: IsCollectionObject ( IDataObject* inDataObj )
{
  PRBool isCollection = PR_FALSE;
  
  // setup the format object to ask for the MULTI_MIME format. We only need to do this once
  static UINT sFormat = 0;
  static FORMATETC sFE;
  if ( !sFormat ) {
    sFormat = nsClipboard::GetFormat(MULTI_MIME);
    SET_FORMATETC(sFE, sFormat, 0, DVASPECT_CONTENT, -1, TYMED_HGLOBAL);
  }
  
  // ask the object if it supports it. If yes, we have a collection object
  if ( inDataObj->QueryGetData(&sFE) == S_OK )
    isCollection = PR_TRUE; 

  return isCollection;

} // IsCollectionObject


//
// EndDragSession
//
// Override the default to make sure that we release the data object when the drag ends. It
// seems that OLE doesn't like to let apps quit w/out crashing when we're still holding onto 
// their data
//
NS_IMETHODIMP
nsDragService::EndDragSession ()
{
  nsBaseDragService::EndDragSession();
  NS_IF_RELEASE(mDataObject);

  return NS_OK;
}
