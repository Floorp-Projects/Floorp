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

#include "nsClipboard.h"

#include "nsCOMPtr.h"
#include "nsFileSpec.h"
#include "nsISupportsArray.h"
#include "nsIClipboardOwner.h"
#include "nsITransferable.h"   // kTextMime
#include "nsISupportsPrimitives.h"

#include "nsIWidget.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsXPIDLString.h"
#include "nsPrimitiveHelpers.h"

#include "nsVoidArray.h"
#include "nsPhWidgetLog.h"

// Initialize the  class statics:

//-------------------------------------------------------------------------
//
// nsClipboard constructor
//
//-------------------------------------------------------------------------
nsClipboard::nsClipboard() : nsBaseClipboard()
{
}

//-------------------------------------------------------------------------
// nsClipboard destructor
//-------------------------------------------------------------------------
nsClipboard::~nsClipboard()
{
}


//-------------------------------------------------------------------------
NS_IMETHODIMP nsClipboard::ForceDataToClipboard(PRInt32 aWhichClipboard)
{
  // make sure we have a good transferable
  if (nsnull == mTransferable) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


//-------------------------------------------------------------------------
NS_IMETHODIMP nsClipboard::SetNativeClipboardData(PRInt32 aWhichClipboard)
{
  nsresult res = NS_ERROR_FAILURE;

  // make sure we have a good transferable
  if( nsnull == mTransferable ) return NS_ERROR_FAILURE;

  // get flavor list that includes all flavors that can be written (including ones 
  // obtained through conversion)
  nsCOMPtr<nsISupportsArray> flavorList;
  nsresult errCode = mTransferable->FlavorsTransferableCanExport ( getter_AddRefs(flavorList) );
  if ( NS_FAILED(errCode) ) return NS_ERROR_FAILURE;
	
  PRUint32 cnt;
  flavorList->Count(&cnt);
  if (cnt) {
    PhClipHeader *cliphdr = (PhClipHeader *) calloc( cnt, sizeof( PhClipHeader ));

    if( cliphdr ) {
      PRUint32   i=0, index=0;
      void      *data = nsnull;
      PRUint32   dataLen;

      mIgnoreEmptyNotification = PR_TRUE;

      for ( PRUint32 i=0; i<cnt; ++i ) {
         nsCOMPtr<nsISupports> genericFlavor;
         flavorList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
         nsCOMPtr<nsISupportsString> currentFlavor ( do_QueryInterface(genericFlavor) );
         if ( currentFlavor )
		 				{
           	nsXPIDLCString flavorStr;
           	currentFlavor->ToString(getter_Copies(flavorStr));
 		   			nsresult err = GetFormat( flavorStr, &cliphdr[index] );
          	if( err != NS_OK ) continue;

           	// Get data out of transferable.
           	nsCOMPtr<nsISupports> genericDataWrapper;
           	mTransferable->GetTransferData( flavorStr, getter_AddRefs(genericDataWrapper), &dataLen ); 
										  
            nsPrimitiveHelpers::CreateDataFromPrimitive ( flavorStr, genericDataWrapper, &data, dataLen );
  
					  int len = 0;
					  char *plain;

					  nsPrimitiveHelpers::ConvertUnicodeToPlatformPlainText( (PRUnichar*)data, dataLen/2, &plain, &len );

						cliphdr[index].length = len+1;
						cliphdr[index].data = plain;

         		index++;
         	}
       	}
     	PhClipboardCopy( 1, index, cliphdr );
	   	for(i=0; i<index; i++)
	     nsCRT::free ( NS_REINTERPRET_CAST(char*, cliphdr[i].data) );

      res = NS_OK;
      mIgnoreEmptyNotification = PR_FALSE;
			free( cliphdr );
    	}
  	}
  
  return res;
	}


//-------------------------------------------------------------------------
NS_IMETHODIMP
nsClipboard::GetNativeClipboardData( nsITransferable * aTransferable, PRInt32 aWhichClipboard ) {

  nsresult res = NS_ERROR_FAILURE;

  // make sure we have a good transferable
  if( nsnull == aTransferable ) return NS_ERROR_FAILURE;

  // get flavor list that includes all acceptable flavors (including ones obtained through
  // conversion)
  nsCOMPtr<nsISupportsArray> flavorList;
  nsresult errCode = aTransferable->FlavorsTransferableCanImport ( getter_AddRefs(flavorList) );
  if ( NS_FAILED(errCode) ) return NS_ERROR_FAILURE;

 // Walk through flavors and see which flavor matches the one being pasted:
  PRUint32 cnt;

  flavorList->Count(&cnt);
  nsCAutoString foundFlavor;
  if (cnt > 0) {
    void         *clipPtr;
    PhClipHeader  cliptype;
    PhClipHeader *cliphdr;
    void         *data = nsnull;
    PRUint32      dataLen;

    clipPtr = PhClipboardPasteStart( 1 );
	if(!clipPtr)
	   return NS_ERROR_FAILURE;

	for ( PRUint32 i = 0; i < cnt; ++i ) {
      nsCOMPtr<nsISupports> genericFlavor;
      flavorList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
      nsCOMPtr<nsISupportsString> currentFlavor ( do_QueryInterface(genericFlavor) );

      if ( currentFlavor ) {

        nsXPIDLCString flavorStr;
        currentFlavor->ToString ( getter_Copies(flavorStr) );

        nsresult err = GetFormat( flavorStr, &cliptype);
        if (err != NS_OK) continue;

        cliphdr = PhClipboardPasteType( clipPtr, cliptype.type );
        if (cliphdr) {
		  		data = cliphdr->data;
		  		dataLen = cliphdr->length;

					int len_unicode;
					PRUnichar *unicode;
					nsPrimitiveHelpers::ConvertPlatformPlainTextToUnicode( (char*) data, dataLen, &unicode, &len_unicode );
			        len_unicode--;
					len_unicode *= 2;

#if 0
/* Kirk - 2/25/99 Disable this for unicode text */
		  /* If this is a TEXT, and is NULL teminated then strip it off */
          if ( (strcmp(cliptype.type, Ph_CLIPBOARD_TYPE_TEXT) == 0)
	           && (*(dataStr+(dataLen-1)) == NULL)
			 )
		  {
			dataLen--;		  
		  }
#endif

		
          	nsCOMPtr<nsISupports> genericDataWrapper;
          	nsPrimitiveHelpers::CreatePrimitiveForData ( flavorStr, unicode, len_unicode, getter_AddRefs(genericDataWrapper) );
          	aTransferable->SetTransferData(flavorStr, genericDataWrapper, len_unicode );

          	res = NS_OK;
		  			break;
        	}
      	}
    	}

   	PhClipboardPasteFinish( clipPtr );
  	}
    
  return res;
	}

NS_IMETHODIMP
nsClipboard::HasDataMatchingFlavors(nsISupportsArray* aFlavorList, 
                                    PRInt32 aWhichClipboard, 
                                    PRBool * outResult) {

  nsresult res = NS_OK;
  * outResult = PR_FALSE;

 // Walk through flavors and see which flavor matches the one being pasted:
  PRUint32 cnt;

  aFlavorList->Count(&cnt);
  nsCAutoString foundFlavor;
  if (cnt > 0) {
    void         *clipPtr;
    PhClipHeader  cliptype;
    PhClipHeader *cliphdr;

    clipPtr = PhClipboardPasteStart( 1 );
	if(nsnull == clipPtr)
		return res;

    for ( PRUint32 i = 0; i < cnt; ++i ) {
      nsCOMPtr<nsISupports> genericFlavor;
      aFlavorList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
      nsCOMPtr<nsISupportsString> currentFlavor ( do_QueryInterface(genericFlavor) );

      if ( currentFlavor ) {

        nsXPIDLCString flavorStr;
        currentFlavor->ToString ( getter_Copies(flavorStr) );

        nsresult err = GetFormat( flavorStr, &cliptype);
        if (err != NS_OK) continue;

        cliphdr = PhClipboardPasteType( clipPtr, cliptype.type );
        if (cliphdr) 
		{

          	res = NS_OK;
			*outResult = PR_TRUE;
		  			break;
    	}
	  }
	}

   	PhClipboardPasteFinish( clipPtr );
  }
    
  return res;
}

//=========================================================================

//-------------------------------------------------------------------------
nsresult nsClipboard::GetFormat(const char* aMimeStr, PhClipHeader *cliphdr ) {

  nsCAutoString mimeStr ( CBufDescriptor(NS_CONST_CAST(char*,aMimeStr), PR_TRUE, PL_strlen(aMimeStr)+1) );

  cliphdr->type[0]=0;

  if (mimeStr.Equals(kUnicodeMime))
    strcpy( cliphdr->type, Ph_CLIPBOARD_TYPE_TEXT );

	if (cliphdr->type[0] == 0)
		return NS_ERROR_FAILURE;
	else
		return NS_OK;
	}
