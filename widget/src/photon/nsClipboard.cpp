/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
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
 * Copyright (C) 1999-2000 Netscape Communications Corporation.
 * All Rights Reserved.
 * 
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Mike Pinkerton <pinkerton@netscape.com>
 *   Dan Rosen <dr@netscape.com>
 */

#include "nsClipboard.h"

#include <Pt.h>

#include "nsCOMPtr.h"
#include "nsFileSpec.h"
#include "nsCRT.h"
#include "nsISupportsArray.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsReadableUtils.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsPrimitiveHelpers.h"

#include "nsTextFormatter.h"
#include "nsVoidArray.h"

#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"

#include "prtime.h"
#include "prthread.h"

// unicode conversion
#include "nsIPlatformCharset.h"


// Define this to enable the obsolete X cut buffer mechanism
// In general, a bad idea (see http://www.jwz.org/doc/x-cut-and-paste.html)
// but it might have its uses for backwards compatibility.

NS_IMPL_ISUPPORTS1(nsClipboard, nsIClipboard);

#define Ph_CLIPBOARD_TYPE_MOZ_BOOKMARK	"BOOK"
#define Ph_CLIPBOARD_TYPE_IMAGE			"IMAG"
#define Ph_CLIPBOARD_TYPE_HTML			"HTML"

//-------------------------------------------------------------------------
//
// nsClipboard constructor
//
//-------------------------------------------------------------------------
nsClipboard::nsClipboard()
{
#ifdef DEBUG_CLIPBOARD
  printf("nsClipboard::nsClipboard()\n");
#endif /* DEBUG_CLIPBOARD */

  mIgnoreEmptyNotification = PR_FALSE;
  mGlobalTransferable = nsnull;
  mSelectionTransferable = nsnull;
  mGlobalOwner = nsnull;
  mSelectionOwner = nsnull;
}

//-------------------------------------------------------------------------
//
// nsClipboard destructor
//
//-------------------------------------------------------------------------
nsClipboard::~nsClipboard()
{
#ifdef DEBUG_CLIPBOARD
  printf("nsClipboard::~nsClipboard()\n");  
#endif /* DEBUG_CLIPBOARD */
}

/**
  * Sets the transferable object
  *
  */
NS_IMETHODIMP nsClipboard::SetData(nsITransferable * aTransferable,
                                   nsIClipboardOwner * anOwner,
                                   PRInt32 aWhichClipboard)
{

	if ((aTransferable == mGlobalTransferable.get() && anOwner == mGlobalOwner.get() && 
			aWhichClipboard == kGlobalClipboard ) || (aTransferable == mSelectionTransferable.get() && 
			anOwner == mSelectionOwner.get() && aWhichClipboard == kSelectionClipboard))
	{
		return NS_OK;
	}

	EmptyClipboard(aWhichClipboard);

	switch (aWhichClipboard) 
	{
		case kSelectionClipboard:
			mSelectionOwner = anOwner;
			mSelectionTransferable = aTransferable;
			break;
		case kGlobalClipboard:
			mGlobalOwner = anOwner;
			mGlobalTransferable = aTransferable;
			break;
	}

	return SetNativeClipboardData(aWhichClipboard);
}

/**
  * Gets the transferable object
  *
  */
NS_IMETHODIMP nsClipboard::GetData(nsITransferable * aTransferable, PRInt32 aWhichClipboard)
{
	if (nsnull != aTransferable)
		return GetNativeClipboardData(aTransferable, aWhichClipboard);
	else 
	{
#ifdef DEBUG_CLIPBOARD
		printf("  nsClipboard::GetData(), aTransferable is NULL.\n");
#endif
	}

	return NS_ERROR_FAILURE;
}


/**
  * 
  *
  */
NS_IMETHODIMP nsClipboard::EmptyClipboard(PRInt32 aWhichClipboard)
{
	if (mIgnoreEmptyNotification)
		return NS_OK;

	switch(aWhichClipboard) 
	{
		case kSelectionClipboard:
			if (mSelectionOwner) 
			{
				mSelectionOwner->LosingOwnership(mSelectionTransferable);
				mSelectionOwner = nsnull;
			}
			mSelectionTransferable = nsnull;
			break;
		case kGlobalClipboard:
			if (mGlobalOwner) 
			{
				mGlobalOwner->LosingOwnership(mGlobalTransferable);
				mGlobalOwner = nsnull;
			}
			mGlobalTransferable = nsnull;
			break;
	}

	return NS_OK;
}

NS_IMETHODIMP nsClipboard::SupportsSelectionClipboard(PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = PR_TRUE; // we support the selection clipboard on unix.
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsClipboard::SetNativeClipboardData(PRInt32 aWhichClipboard)
{
	mIgnoreEmptyNotification = PR_TRUE;

#ifdef DEBUG_CLIPBOARD
	printf("  nsClipboard::SetNativeClipboardData(%i)\n", aWhichClipboard);
#endif /* DEBUG_CLIPBOARD */

  	nsCOMPtr<nsITransferable> transferable(getter_AddRefs(GetTransferable(aWhichClipboard)));

	// make sure we have a good transferable
	if (nsnull == transferable) 
	{
#ifdef DEBUG_CLIPBOARD
		printf("nsClipboard::SetNativeClipboardData(): no transferable!\n");
#endif
		return NS_ERROR_FAILURE;
	}

	// get flavor list that includes all flavors that can be written (including ones 
	// obtained through conversion)
	nsCOMPtr<nsISupportsArray> flavorList;
	nsresult errCode = transferable->FlavorsTransferableCanExport ( getter_AddRefs(flavorList) );
	if ( NS_FAILED(errCode) )
		return NS_ERROR_FAILURE;

	PRUint32 cnt;
	flavorList->Count(&cnt);
	for ( PRUint32 i=0; i<cnt; ++i )
	{
		PhClipHeader *cliphdr = (PhClipHeader *) calloc( cnt, sizeof( PhClipHeader ));

		if( cliphdr ) 
		{
			PRUint32   i=0, index=0;
			void      *data = nsnull;
			PRUint32   dataLen;

			nsCOMPtr<nsISupports> genericFlavor;
			flavorList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
			nsCOMPtr<nsISupportsCString> currentFlavor ( do_QueryInterface(genericFlavor) );
			if ( currentFlavor ) 
			{
				nsXPIDLCString flavorStr;
				currentFlavor->ToString(getter_Copies(flavorStr));

	        	nsresult err = GetFormat( flavorStr, &cliphdr[index] );
				if( err != NS_OK ) 
					continue;

				// Get data out of transferable.
				nsCOMPtr<nsISupports> genericDataWrapper;
				transferable->GetTransferData( flavorStr, getter_AddRefs(genericDataWrapper), &dataLen );
				nsPrimitiveHelpers::CreateDataFromPrimitive ( flavorStr, genericDataWrapper, &data, dataLen );

				if (strcmp(cliphdr[index].type, Ph_CLIPBOARD_TYPE_MOZ_BOOKMARK) == 0)
				{
            		cliphdr[index].length = dataLen;
            		cliphdr[index].data = data;
				}
				else if (strcmp(cliphdr[index].type, Ph_CLIPBOARD_TYPE_TEXT) == 0)
				{
					int len = 0;
					char *plain;
					nsPrimitiveHelpers::ConvertUnicodeToPlatformPlainText( (PRUnichar*)data, dataLen/2, &plain, &len );
            		cliphdr[index].length = len+1;
            		cliphdr[index].data = plain;
				}
    			index++;	
			}

			PhClipboardCopy( 1, index, cliphdr );
			for(i=0; i<index; i++)
				nsCRT::free ( NS_REINTERPRET_CAST(char*, cliphdr[i].data) );

			free( cliphdr );
		}
	}

  	mIgnoreEmptyNotification = PR_FALSE;

  	return NS_OK;
}


//-------------------------------------------------------------------------
//
// The blocking Paste routine
//
//-------------------------------------------------------------------------
NS_IMETHODIMP
nsClipboard::GetNativeClipboardData(nsITransferable * aTransferable, 
                                    PRInt32 aWhichClipboard)
{
#ifdef DEBUG_CLIPBOARD
	printf("nsClipboard::GetNativeClipboardData(%i)\n", aWhichClipboard);
#endif /* DEBUG_CLIPBOARD */

  	// make sure we have a good transferable
	if (nsnull == aTransferable) 
  	{
#ifdef DEBUG_CLIPBOARD
    	printf("  GetNativeClipboardData: Transferable is null!\n");
#endif
    	return NS_ERROR_FAILURE;
  	}

	// get flavor list that includes all acceptable flavors (including ones obtained through
	// conversion)
	nsCOMPtr<nsISupportsArray> flavorList;
	nsresult errCode = aTransferable->FlavorsTransferableCanImport ( getter_AddRefs(flavorList) );
	if ( NS_FAILED(errCode) )
		return NS_ERROR_FAILURE;

	// Walk through flavors and see which flavor matches the one being pasted:
	PRUint32 cnt;
	flavorList->Count(&cnt);
	nsCAutoString foundFlavor;
	PRBool foundData = PR_FALSE;

	if (cnt > 0) 
	{
  		void         *clipPtr;
  		PhClipHeader  cliptype;
  		PhClipHeader *cliphdr;
  		void         *data = nsnull;
  		PRUint32      dataLen;

  		clipPtr = PhClipboardPasteStart( 1 );
  		if(!clipPtr)
     		return NS_ERROR_FAILURE;

		for ( PRUint32 i = 0; i < cnt; ++i ) 
		{
			nsCOMPtr<nsISupports> genericFlavor;
			flavorList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
			nsCOMPtr<nsISupportsCString> currentFlavor ( do_QueryInterface(genericFlavor) );
			if ( currentFlavor ) 
			{
				nsXPIDLCString flavorStr;
				currentFlavor->ToString ( getter_Copies(flavorStr) );
				nsresult err = GetFormat( flavorStr, &cliptype);
				if (err != NS_OK) 
					continue;

				cliphdr = PhClipboardPasteType( clipPtr, cliptype.type );
				if (cliphdr) 
				{
					int len_unicode;
					PRUnichar *unicode;

					data = cliphdr->data;
					dataLen = cliphdr->length;

					nsCOMPtr<nsISupports> genericDataWrapper;
					if (strcmp(cliptype.type, Ph_CLIPBOARD_TYPE_MOZ_BOOKMARK) == 0)
					{
						len_unicode = cliphdr->length;
						nsPrimitiveHelpers::CreatePrimitiveForData ( flavorStr, cliphdr->data, len_unicode, getter_AddRefs(genericDataWrapper) );
					}
					else if (strcmp(cliptype.type, Ph_CLIPBOARD_TYPE_TEXT) == 0)
					{
						nsPrimitiveHelpers::ConvertPlatformPlainTextToUnicode( (char*) data, dataLen, &unicode, &len_unicode );
						len_unicode--;
						len_unicode *= 2;
						nsPrimitiveHelpers::CreatePrimitiveForData ( flavorStr, unicode, len_unicode, getter_AddRefs(genericDataWrapper) );
					}

					aTransferable->SetTransferData(flavorStr, genericDataWrapper, len_unicode );
            		break;
    			}
			}
		}
		PhClipboardPasteFinish( clipPtr );
	}

#if 0
  // We're back from the callback, no longer blocking:
  mBlocking = PR_FALSE;

  // 
  // Now we have data in mSelectionData.data.
  // We just have to copy it to the transferable.
  // 

  if ( foundData ) {
    nsCOMPtr<nsISupports> genericDataWrapper;
    nsPrimitiveHelpers::CreatePrimitiveForData(foundFlavor.get(), mSelectionData.data,
                                               mSelectionData.length, getter_AddRefs(genericDataWrapper));
    aTransferable->SetTransferData(foundFlavor.get(),
                                   genericDataWrapper,
                                   mSelectionData.length);
  }
    
  // transferable is now owning the data, so we can free it.
  nsMemory::Free(mSelectionData.data);
  mSelectionData.data = nsnull;
  mSelectionData.length = 0;
#endif

	return NS_OK;
}

/**
 * Some platforms support deferred notification for putting data on the clipboard
 * This method forces the data onto the clipboard in its various formats
 * This may be used if the application going away.
 *
 * @result NS_OK if successful.
 */
NS_IMETHODIMP nsClipboard::ForceDataToClipboard(PRInt32 aWhichClipboard)
{
#ifdef DEBUG_CLIPBOARD
	printf("  nsClipboard::ForceDataToClipboard()\n");
#endif /* DEBUG_CLIPBOARD */

  	// make sure we have a good transferable
  	nsCOMPtr<nsITransferable> transferable(getter_AddRefs(GetTransferable(aWhichClipboard)));

  	if (nsnull == transferable) 
 		return NS_ERROR_FAILURE;

  	return NS_OK;
}

NS_IMETHODIMP
nsClipboard::HasDataMatchingFlavors(nsISupportsArray* aFlavorList, 
                                    PRInt32 aWhichClipboard, 
                                    PRBool * outResult)
{
  // XXX this doesn't work right.  need to fix it.
  
  // Note to implementor...(from pink the clipboard bitch).
  //
  // If a client asks for unicode, first check if unicode is present. If not, then 
  // check for plain text. If it's there, say "yes" as we will do the conversion
  // in GetNativeClipboardData(). From this point on, no client will
  // ever ask for text/plain explicitly. If they do, you must ASSERT!
#ifdef DEBUG_CLIPBOARD
	printf("  nsClipboard::HasDataMatchingFlavors()\n  {\n");
#endif

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
      nsCOMPtr<nsISupportsCString> currentFlavor ( do_QueryInterface(genericFlavor) );

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


nsresult nsClipboard::GetFormat(const char* aMimeStr, PhClipHeader *cliphdr ) 
{

  	nsCAutoString mimeStr ( CBufDescriptor(NS_CONST_CAST(char*,aMimeStr), PR_TRUE, PL_strlen(aMimeStr)+1) );

  	cliphdr->type[0]=0;
  	if (mimeStr.Equals(kUnicodeMime) || mimeStr.Equals(kHTMLMime) || mimeStr.Equals(kTextMime))
    	strcpy( cliphdr->type, Ph_CLIPBOARD_TYPE_TEXT );
  	else if (mimeStr.Equals(kHTMLMime))
    	strcpy( cliphdr->type, Ph_CLIPBOARD_TYPE_HTML );
  	else if (mimeStr.Equals("moz/bookmarkclipboarditem"))
    	strcpy( cliphdr->type, Ph_CLIPBOARD_TYPE_MOZ_BOOKMARK );
//  	else if (mimeStr.Equals(kPNGImageMime) || mimeStr.Equals(kJPEGImageMime) || mimeStr.Equals(kGIFImageMime))
//		strcpy( cliphdr->type, Ph_CLIPBOARD_TYPE_IMAGE);

    if (cliphdr->type[0] == 0)
        return NS_ERROR_FAILURE;
    else
        return NS_OK;
}

/* inline */
nsITransferable *nsClipboard::GetTransferable(PRInt32 aWhichClipboard)
{
  nsITransferable *transferable = nsnull;
  switch (aWhichClipboard)
  {
  case kGlobalClipboard:
    transferable = mGlobalTransferable;
    break;
  case kSelectionClipboard:
    transferable = mSelectionTransferable;
    break;
  }
  NS_IF_ADDREF(transferable);
  return transferable;
}


