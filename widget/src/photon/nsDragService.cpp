/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
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

NS_IMPL_ADDREF_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_RELEASE_INHERITED(nsDragService, nsBaseDragService)
NS_IMPL_QUERY_INTERFACE2(nsDragService, nsIDragService, nsIDragSession)

char *nsDragService::mDndEvent = NULL;
int nsDragService::mDndEventLen;

#define kMimeCustom			"text/_moz_htmlcontext"

//-------------------------------------------------------------------------
//
// DragService constructor
//
//-------------------------------------------------------------------------
nsDragService::nsDragService()
{
  mDndWidget = nsnull;
  mDndEvent = nsnull;
	mNativeCtrl = nsnull;
	mRawData = nsnull;
	mFlavourStr = nsnull;
	mTransportFile = nsnull;
}

//-------------------------------------------------------------------------
//
// DragService destructor
//
//-------------------------------------------------------------------------
nsDragService::~nsDragService()
{
	if( mNativeCtrl ) PtReleaseTransportCtrl( mNativeCtrl );
	if( mFlavourStr ) free( mFlavourStr );
	if( mTransportFile ) {
		unlink( mTransportFile );
		free( mTransportFile );
		}
}

NS_IMETHODIMP nsDragService::SetNativeDndData( PtWidget_t *widget, PhEvent_t *event ) {
	mDndWidget = widget;
	if( !mDndEvent ) {
		mDndEventLen = sizeof( PhEvent_t ) + event->num_rects * sizeof( PhRect_t ) + event->data_len;
		mDndEvent = ( char * ) malloc( mDndEventLen );
		}
	memcpy( mDndEvent, (char*)event, mDndEventLen );
	return NS_OK;
	}

NS_IMETHODIMP nsDragService::SetDropData( char *data ) {

	if( mRawData ) free( mRawData );

	/* data is the filename used for passing the data */
	FILE *fp = fopen( data, "r" );
	PRUint32 n;
	fread( &n, sizeof( PRUint32 ), 1, fp );
	mRawData = ( char * ) malloc( n );
	if( !mRawData ) { fclose( fp ); return NS_ERROR_FAILURE; }

	fseek( fp, 0, SEEK_SET );
	fread( mRawData, 1, n, fp );
	fclose( fp );

  return NS_OK;
  }


/* remove this function with the one from libph.so, when it becomes available */
int CancelDrag( PhRid_t rid, unsigned input_group )
{
  struct dragevent {
      PhEvent_t hdr;
      PhDragEvent_t drag;
      } ev;
  memset( &ev, 0, sizeof(ev) );
  ev.hdr.type = Ph_EV_DRAG;
  ev.hdr.emitter.rid = Ph_DEV_RID;
  ev.hdr.flags = Ph_EVENT_INCLUSIVE | Ph_EMIT_TOWARD;
  ev.hdr.data_len = sizeof( ev.drag );
  ev.hdr.subtype = Ph_EV_DRAG_COMPLETE;
  ev.hdr.input_group = input_group;
  ev.drag.rid = rid;
  return PhEmit( &ev.hdr, NULL, &ev.drag );
} 


//-------------------------------------------------------------------------
NS_IMETHODIMP
nsDragService::InvokeDragSession (nsIDOMNode *aDOMNode,
                                  nsISupportsArray * aArrayTransferables,
                                  nsIScriptableRegion * aRegion,
                                  PRUint32 aActionType)
{
#ifdef DEBUG
	printf( "nsDragService::InvokeDragSession\n" );
#endif
  nsBaseDragService::InvokeDragSession (aDOMNode, aArrayTransferables, aRegion, aActionType);

  if(!aArrayTransferables) return NS_ERROR_INVALID_ARG;

	/*  this will also addref the transferables since we're going to hang onto this beyond the length of this call */
	mSourceDataItems = aArrayTransferables;

  PRUint32 numDragItems = 0;
  mSourceDataItems->Count(&numDragItems);
  if ( ! numDragItems ) return NS_ERROR_FAILURE;

	/* cancel a previous drag ( PhInitDrag ) if one is in place */
	CancelDrag( PtWidgetRid( mDndWidget ), ((PhEvent_t*)mDndEvent)->input_group );

	mActionType = aActionType;

	PRUint32 pDataLen = sizeof( PRUint32 ) + sizeof( PRUint32 ), totalItems = 0;
	char *pdata = ( char * ) malloc( pDataLen ); /* we reserve space for a total size and totalItems */
	if( !pdata ) return NS_ERROR_FAILURE;


  for(PRUint32 itemIndex = 0; itemIndex < numDragItems; ++itemIndex) {
    nsCOMPtr<nsISupports> genericItem;
    mSourceDataItems->GetElementAt(itemIndex, getter_AddRefs(genericItem));
    nsCOMPtr<nsITransferable> currItem (do_QueryInterface(genericItem));
    if(currItem) {
      nsCOMPtr <nsISupportsArray> flavorList;
      currItem->FlavorsTransferableCanExport(getter_AddRefs(flavorList));
      if(flavorList) {
        PRUint32 numFlavors;
        flavorList->Count( &numFlavors );
        for( PRUint32 flavorIndex = 0; flavorIndex < numFlavors ; ++flavorIndex ) {
          nsCOMPtr<nsISupports> genericWrapper;
          flavorList->GetElementAt (flavorIndex, getter_AddRefs(genericWrapper));
          nsCOMPtr<nsISupportsCString> currentFlavor;
          currentFlavor = do_QueryInterface(genericWrapper);
          if(currentFlavor) {
            nsXPIDLCString flavorStr;
            currentFlavor->ToString ( getter_Copies(flavorStr) );
						const char *FlavourStr = ( const char * ) flavorStr;
						nsCOMPtr<nsISupports> data;
						PRUint32 tmpDataLen = 0;
						nsresult rv = currItem->GetTransferData( FlavourStr, getter_AddRefs(data), &tmpDataLen );
						if( NS_SUCCEEDED( rv ) ) {
							/* insert FlavourStr, data into the PtTransportCtrl_t */
							int len = sizeof( PRUint32 ) + sizeof( PRUint32 ) + strlen( FlavourStr ) + 1 + tmpDataLen;
							/* we reserve space for itemIndex|tmpDataLen|flavorStr|data */
							len = ( ( len + 3 ) / 4 ) * 4;
							pdata = ( char * ) realloc( pdata, len + pDataLen );
							if( pdata ) {
								char *p = pdata + pDataLen;
								PRUint32 *d = ( PRUint32 * ) p;
								d[0] = itemIndex; /* copy itemIndex*/
								d[1] = tmpDataLen; /* copy PRUint32 tmpDataLen */
								strcpy( p + sizeof( PRUint32 ) + sizeof( PRUint32 ), FlavourStr ); /* copy flavorStr */
	
								void *mem_data;
								nsPrimitiveHelpers::CreateDataFromPrimitive ( FlavourStr, data, &mem_data, tmpDataLen );
	
								memcpy( p + sizeof( PRUint32 ) + sizeof( PRUint32 ) + strlen( FlavourStr ) + 1, mem_data, tmpDataLen ); /* copy the data */
								pDataLen += len;
								totalItems++;
								}
							}
						}
          }
        }
      }
    }

	if( totalItems ) {
		PRUint32 *p = ( PRUint32 * ) pdata;
		p[0] = pDataLen;
		p[1] = totalItems;
		mNativeCtrl = PtCreateTransportCtrl( );

		if( !mTransportFile ) mTransportFile = strdup( (char*) tmpnam( NULL ) );

		FILE *fp = fopen( mTransportFile, "w" );
		fwrite( pdata, 1, pDataLen, fp );
		fclose( fp );
		free( pdata );

		PtTransportType( mNativeCtrl, "Mozilla", "dnddata", 1, Ph_TRANSPORT_INLINE, "string", (void*)mTransportFile, 0, 0 );
		PtInitDnd( mNativeCtrl, mDndWidget, (PhEvent_t*)mDndEvent, NULL, 0 );
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

	if( mRawData ) {
		PRUint32 *d = ( PRUint32 * ) mRawData, totalItems = d[1];
		PRUint32 i, pdataLen = sizeof( PRUint32 ) + sizeof( PRUint32 );

		/* search for aItemIndex */
		for( i=0; i<totalItems; i++ ) {
			char *p = mRawData + pdataLen;
			PRUint32 *d = ( PRUint32 * ) p;

			char *flavorStr = p + sizeof( PRUint32 ) + sizeof( PRUint32 );
			PRUint32 this_len = sizeof( PRUint32 ) + sizeof( PRUint32 ) + strlen( flavorStr ) + 1 + d[1];
			this_len = ( ( this_len + 3 ) / 4 ) * 4;
			char *raw_data = flavorStr + strlen( flavorStr ) + 1;

			if( d[0] == aItemIndex && !strcmp( mFlavourStr, flavorStr ) ) {
				nsPrimitiveHelpers::CreatePrimitiveForData( flavorStr, raw_data, d[1], getter_AddRefs( genericDataWrapper ) );
				rv = aTransferable->SetTransferData( flavorStr, genericDataWrapper, d[1] );
				break;
				}

			pdataLen += this_len;
			}
		}
	return rv;
	}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsDragService::IsDataFlavorSupported(const char *aDataFlavor, PRBool *_retval)
{
  if (!aDataFlavor || !_retval)
    return NS_ERROR_FAILURE;

	// set this to no by default
	*_retval = PR_FALSE;

	const char *ask;
	if( !strcmp( aDataFlavor, kMimeCustom ) )  ask = kHTMLMime;
	else ask = aDataFlavor;

	if(!mSourceDataItems) return NS_OK;
	PRUint32 numDragItems = 0;
	mSourceDataItems->Count(&numDragItems);
	for(PRUint32 itemIndex = 0; itemIndex < numDragItems; ++itemIndex) {
  	nsCOMPtr<nsISupports> genericItem;
  	mSourceDataItems->GetElementAt(itemIndex, getter_AddRefs(genericItem));
  	nsCOMPtr<nsITransferable> currItem (do_QueryInterface(genericItem));
  	if(currItem) {
  	  nsCOMPtr <nsISupportsArray> flavorList;
  	  currItem->FlavorsTransferableCanExport(getter_AddRefs(flavorList));
  	  if(flavorList) {
  	    PRUint32 numFlavors;
  	    flavorList->Count( &numFlavors );
  	    for( PRUint32 flavorIndex = 0; flavorIndex < numFlavors ; ++flavorIndex ) {
  	      nsCOMPtr<nsISupports> genericWrapper;
  	      flavorList->GetElementAt (flavorIndex, getter_AddRefs(genericWrapper));
  		    nsCOMPtr<nsISupportsCString> currentFlavor;
  	      currentFlavor = do_QueryInterface(genericWrapper);
 		      if(currentFlavor) {
  	        nsXPIDLCString flavorStr;
  	        currentFlavor->ToString ( getter_Copies(flavorStr) );
						if(strcmp(flavorStr, ask) == 0) {
  						*_retval = PR_TRUE;
							if( mFlavourStr ) free( mFlavourStr );
							mFlavourStr = strdup( ask );
							}
						}
					}
				}
			}
		}

  return NS_OK;
}

void
nsDragService::SourceEndDrag(void)
{
  // this just releases the list of data items that we provide
  mSourceDataItems = 0;
}
