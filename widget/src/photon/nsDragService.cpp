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
#include "nsString.h"
#include "nsClipboard.h"
#include "nsIRegion.h"
#include "nsVoidArray.h"
#include "nsISupportsPrimitives.h"
#include "nsPrimitiveHelpers.h"
#include "nsCOMPtr.h"
#include "nsXPIDLString.h"

#include "nsWidgetsCID.h"
#include "nsPhWidgetLog.h"

NS_IMPL_ADDREF_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_RELEASE_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_QUERY_INTERFACE2(nsDragService, nsIDragService, nsIDragSession)

#define DEBUG_DRAG 1

//-------------------------------------------------------------------------
//
// DragService constructor
//
//-------------------------------------------------------------------------
nsDragService::nsDragService()
{
  NS_INIT_REFCNT();
  mDndWidget = nsnull;
  mDndEvent = nsnull;
	mNativeCtrl = nsnull;
	mflavorStr = nsnull;
	mData = nsnull;
}

//-------------------------------------------------------------------------
//
// DragService destructor
//
//-------------------------------------------------------------------------
nsDragService::~nsDragService()
{
	if( mNativeCtrl ) PtReleaseTransportCtrl( mNativeCtrl );
	if( mflavorStr ) free( mflavorStr );
}

NS_IMETHODIMP nsDragService::SetNativeDndData( PtWidget_t *widget, PhEvent_t *event ) {
	mDndWidget = widget;
	mDndEvent = event;
	return NS_OK;
	}

NS_IMETHODIMP nsDragService::SetDropData( char *data, PRUint32 tmpDataLen, char *aflavorStr ) {

	mtmpDataLen = tmpDataLen;

	if( mData ) free( mData );
	mData = ( char * ) malloc( tmpDataLen );
	memcpy( mData, data, tmpDataLen );

	if( mflavorStr ) free( mflavorStr );
	mflavorStr = strdup( aflavorStr );

  return NS_OK;
  }

//-------------------------------------------------------------------------
NS_IMETHODIMP
nsDragService::InvokeDragSession (nsIDOMNode *aDOMNode,
                                  nsISupportsArray * aArrayTransferables,
                                  nsIScriptableRegion * aRegion,
                                  PRUint32 aActionType)
{
/* ATENTIE */ printf( "nsDragService::InvokeDragSession\n" );
  nsBaseDragService::InvokeDragSession (aDOMNode, aArrayTransferables, aRegion, aActionType);

  // make sure that we have an array of transferables to use
  if(!aArrayTransferables) return NS_ERROR_INVALID_ARG;

  nsresult rv;
  PRUint32 numItemsToDrag = 0;
  rv = aArrayTransferables->Count(&numItemsToDrag);
  if ( !numItemsToDrag )
    return NS_ERROR_FAILURE;

	PRUint32 tmpDataLen = 0;
	nsCOMPtr<nsISupports> genericDataWrapper;
	char aflavorStr[100];
	GetRawData( aArrayTransferables, getter_AddRefs( genericDataWrapper ), &tmpDataLen, aflavorStr );

	void *data;
	nsPrimitiveHelpers::CreateDataFromPrimitive ( aflavorStr, genericDataWrapper, &data, tmpDataLen );

	mNativeCtrl = PtCreateTransportCtrl( );
	static char * dupdata;
	if( dupdata ) free( dupdata );
	dupdata = (char*) calloc( 1, tmpDataLen + 4 + 100 );
	if( dupdata ) {
		*(int*)dupdata = tmpDataLen;
		strcpy( dupdata+4, aflavorStr );
		memcpy( dupdata+4+100, data, tmpDataLen );
		PtTransportType( mNativeCtrl, "Mozilla", "dnddata", 0, Ph_TRANSPORT_INLINE, "raw", dupdata, tmpDataLen+4+100, 0 );
		PtInitDnd( mNativeCtrl, mDndWidget, mDndEvent, NULL, 0 );
		}

  return NS_OK;
}



//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::GetNumDropItems (PRUint32 * aNumItems)
{
  *aNumItems = 1;
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::GetData (nsITransferable * aTransferable, PRUint32 aItemIndex ) {
	nsresult rv = NS_ERROR_FAILURE;
	nsCOMPtr<nsISupports> genericDataWrapper;
	nsPrimitiveHelpers::CreatePrimitiveForData( mflavorStr, mData, mtmpDataLen, getter_AddRefs( genericDataWrapper ) );
	rv = aTransferable->SetTransferData( mflavorStr, genericDataWrapper, mtmpDataLen );
	return rv;
	}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::GetRawData( nsISupportsArray* aArrayTransferables, nsISupports **data, PRUint32 *tmpDataLen, char *aflavorStr ) {

	int aItemIndex = 0;

	// get the item with the right index
	nsCOMPtr<nsISupports> genericItem;
	aArrayTransferables->GetElementAt(aItemIndex, getter_AddRefs(genericItem));
	nsCOMPtr<nsITransferable> aTransferable (do_QueryInterface(genericItem));

  // get flavor list that includes all acceptable flavors (including
  // ones obtained through conversion). Flavors are nsISupportsStrings
  // so that they can be seen from JS.
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsISupportsArray> flavorList;
  rv = aTransferable->FlavorsTransferableCanImport(getter_AddRefs(flavorList));
  if (NS_FAILED(rv))
    return rv;

  // count the number of flavors
  PRUint32 cnt;
  flavorList->Count (&cnt);
  unsigned int i;

  // Now walk down the list of flavors. When we find one that is
  // actually present, copy out the data into the transferable in that
  // format. SetTransferData() implicitly handles conversions.
  for ( i = 0; i < cnt; ++i ) {
    nsCOMPtr<nsISupports> genericWrapper;
    flavorList->GetElementAt(i,getter_AddRefs(genericWrapper));
    nsCOMPtr<nsISupportsString> currentFlavor;
    currentFlavor = do_QueryInterface(genericWrapper);
    if (currentFlavor) {
      // find our gtk flavor
      nsXPIDLCString flavorStr;
      currentFlavor->ToString ( getter_Copies(flavorStr) );

     // get the item with the right index
     nsCOMPtr<nsISupports> genericItem;
     aArrayTransferables->GetElementAt( aItemIndex, getter_AddRefs(genericItem));
     nsCOMPtr<nsITransferable> item (do_QueryInterface(genericItem));
     if (item) {
       rv = item->GetTransferData(flavorStr, data, tmpDataLen);
       if( NS_SUCCEEDED( rv ) ) {
					strcpy( aflavorStr, flavorStr );
					break;
					}
			 }
    	}
  	} // foreach flavor

  return NS_OK;
  
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::IsDataFlavorSupported(const char *aDataFlavor, PRBool *_retval)
{
  if (!aDataFlavor || !_retval)
    return NS_ERROR_FAILURE;
	*_retval = PR_TRUE;
  return NS_OK;
}
