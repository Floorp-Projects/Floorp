/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * Portions created by the Initial Developer are Copyright (C) 1999-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Mike Pinkerton <pinkerton@netscape.com>
 *   Dan Rosen <dr@netscape.com>
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

#include "nsClipboard.h"

#include <Pt.h>

#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsReadableUtils.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsWidgetsCID.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsPrimitiveHelpers.h"

#include "nsTextFormatter.h"

#include "nsIServiceManager.h"
#include "nsICharsetConverterManager.h"

#include "prtime.h"
#include "prthread.h"

// unicode conversion
#include "nsIPlatformCharset.h"
#include "nsICharsetConverterManager.h"

//#define DEBUG_CLIPBOARD


// Define this to enable the obsolete X cut buffer mechanism
// In general, a bad idea (see http://www.jwz.org/doc/x-cut-and-paste.html)
// but it might have its uses for backwards compatibility.

NS_IMPL_ISUPPORTS1(nsClipboard, nsIClipboard)

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
  mInputGroup = 1;
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
	if (aWhichClipboard == kSelectionClipboard)
		return (NS_ERROR_FAILURE);

	if ((aTransferable == mGlobalTransferable.get() && anOwner == mGlobalOwner.get() && 
			aWhichClipboard == kGlobalClipboard ) || (aTransferable == mSelectionTransferable.get() && 
			anOwner == mSelectionOwner.get() && aWhichClipboard == kSelectionClipboard))
	{
		return NS_OK;
	}

  nsresult rv;
  if (!mPrivacyHandler) {
    rv = NS_NewClipboardPrivacyHandler(getter_AddRefs(mPrivacyHandler));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  rv = mPrivacyHandler->PrepareDataForClipboard(aTransferable);
  NS_ENSURE_SUCCESS(rv, rv);

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
	if (aWhichClipboard == kSelectionClipboard)
		return (NS_ERROR_FAILURE);
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

	if (aWhichClipboard == kSelectionClipboard)
		return (NS_ERROR_FAILURE);

	switch(aWhichClipboard) 
	{
		case kSelectionClipboard:
			return NS_ERROR_FAILURE;
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

  *_retval = PR_FALSE; // we support the selection clipboard on unix.
  return NS_ERROR_FAILURE;
}

//-------------------------------------------------------------------------
NS_IMETHODIMP nsClipboard::SetNativeClipboardData(PRInt32 aWhichClipboard)
{
	mIgnoreEmptyNotification = PR_TRUE;
	if (aWhichClipboard == kSelectionClipboard)
		return (NS_ERROR_FAILURE);

#ifdef DEBUG_CLIPBOARD
	printf("  nsClipboard::SetNativeClipboardData(%i)\n", aWhichClipboard);
#endif /* DEBUG_CLIPBOARD */

		nsCOMPtr<nsITransferable> transferable(GetTransferable(aWhichClipboard));

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

	PRUint32 cnt, index = 0;
	flavorList->Count(&cnt);
	PhClipHeader *cliphdr = (PhClipHeader *) calloc( cnt, sizeof( PhClipHeader ));
	if( !cliphdr ) return NS_ERROR_FAILURE;

	for ( PRUint32 k=0; k<cnt; ++k )
	{
		void      *data = nsnull;
		PRUint32   dataLen;
	
		nsCOMPtr<nsISupports> genericFlavor;
		flavorList->GetElementAt ( k, getter_AddRefs(genericFlavor) );
		nsCOMPtr<nsISupportsCString> currentFlavor ( do_QueryInterface(genericFlavor) );
		if ( currentFlavor ) 
		{
			nsXPIDLCString flavorStr;
			currentFlavor->ToString(getter_Copies(flavorStr));

     	nsresult err = GetFormat( flavorStr, cliphdr[index].type );
			if( err != NS_OK ) 
				continue;

			// Get data out of transferable.
			nsCOMPtr<nsISupports> genericDataWrapper;
			transferable->GetTransferData( flavorStr, getter_AddRefs(genericDataWrapper), &dataLen );
			nsPrimitiveHelpers::CreateDataFromPrimitive ( flavorStr, genericDataWrapper, &data, dataLen );

			if( !strcmp(cliphdr[index].type, Ph_CLIPBOARD_TYPE_TEXT) ||
					!strcmp(cliphdr[index].type, Ph_CLIPBOARD_TYPE_HTML) ||
					!strcmp(cliphdr[index].type, Ph_CLIPBOARD_TYPE_MOZ_BOOKMARK) )
			{
				PRUnichar* castedUnicode = reinterpret_cast<PRUnichar*>(data);
				char *utf8String = ToNewUTF8String(nsDependentString(castedUnicode, dataLen/2));
				nsMemory::Free(reinterpret_cast<char*>(data));

				if( !strcmp(cliphdr[index].type, Ph_CLIPBOARD_TYPE_TEXT) )
				{
					/* we have to create a null terminated string, because
						PhClipboardCopyString does that and some other applications
						rely on the null terminated thing
					*/
					PRInt32 len = strlen(utf8String);
					char *temp = ( char * ) nsMemory::Alloc( len + 1 );
					memcpy( temp, utf8String, len );
					temp[len] = 0;
					nsMemory::Free(reinterpret_cast<char*>(utf8String));

        	cliphdr[index].length = len+1;
        	cliphdr[index].data = temp;
				}
			else {
				cliphdr[index].length = strlen(utf8String);
				cliphdr[index].data = utf8String;
				}
			}
   	index++;	
		}
	}

	PhClipboardCopy( mInputGroup, index, cliphdr );
	for( PRUint32 k=0; k<index; k++)
		nsMemory::Free(reinterpret_cast<char*>(cliphdr[k].data));

	free( cliphdr );

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
	if (aWhichClipboard == kSelectionClipboard)
		return (NS_ERROR_FAILURE);

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

	if (cnt > 0) 
	{
  	void         *clipPtr;
  	PhClipHeader *cliphdr;
  	char         *data = nsnull, type[8];
  	PRUint32      dataLen;

  	clipPtr = PhClipboardPasteStart( mInputGroup );
  	if(!clipPtr) return NS_ERROR_FAILURE;

	/*
		Look at the timestamps of the data in the clipboard and eliminate the flavours if they are not synchronized.
		We can have a HTML flavour from a previous copy and a TEXT flavour from a more recent copy from another application
		( from instance from ped or pterm ). The HTML flavour and TEXT flavour are desynchronized and we have
		to use only the most recent one */
		unsigned long *dont_use_flavour = ( unsigned long * ) calloc( cnt, sizeof( unsigned long ) );
		if( !dont_use_flavour ) {
			PhClipboardPasteFinish( clipPtr );
			return NS_ERROR_FAILURE;
			}

		unsigned long max_time = 0;
		PRUint32 i;

		for ( i = 0; i < cnt; ++i ) 
		{
			nsCOMPtr<nsISupports> genericFlavor;
			flavorList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
			nsCOMPtr<nsISupportsCString> currentFlavor ( do_QueryInterface(genericFlavor) );
			if ( currentFlavor ) 
			{
				nsXPIDLCString flavorStr;
				currentFlavor->ToString ( getter_Copies(flavorStr) );

				nsresult err = GetFormat( flavorStr, type );
				if (err != NS_OK) 
					continue;

			dont_use_flavour[i] = GetFlavourTimestamp( type );
			if( dont_use_flavour[i] > max_time ) max_time = dont_use_flavour[i];
			}
		}

		for ( i = 0; i < cnt; ++i )
		{
		if( abs( dont_use_flavour[i] - max_time ) >= 4 )
			dont_use_flavour[i] = 1; /* this flavour is desynchronized */
		else dont_use_flavour[i] = 0; /* this flavour is ok */
		}

		for ( i = 0; i < cnt; ++i ) 
		{
			if( dont_use_flavour[i] ) continue; /* this flavour is desynchronized */
			nsCOMPtr<nsISupports> genericFlavor;
			flavorList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
			nsCOMPtr<nsISupportsCString> currentFlavor ( do_QueryInterface(genericFlavor) );
			if ( currentFlavor ) 
			{
				nsXPIDLCString flavorStr;
				currentFlavor->ToString ( getter_Copies(flavorStr) );

				nsresult err = GetFormat( flavorStr, type );
				if (err != NS_OK) 
					continue;

				cliphdr = PhClipboardPasteType( clipPtr, type );
				if (cliphdr) 
				{
					data = (char*)cliphdr->data;

					if( !strcmp(type, Ph_CLIPBOARD_TYPE_TEXT) )
						/* for the Ph_CLIPBOARD_TYPE_TEXT, we null terminate the data, since PhClipboardCopyString() does that */
						dataLen = cliphdr->length - 1;
					else dataLen = cliphdr->length;


					if( !strcmp(type, Ph_CLIPBOARD_TYPE_TEXT) ||
							!strcmp(type, Ph_CLIPBOARD_TYPE_HTML) ||
							!strcmp(type, Ph_CLIPBOARD_TYPE_MOZ_BOOKMARK) )
					{
						nsresult rv;
						PRInt32 outUnicodeLen;
						PRUnichar *unicodeData = nsnull;

    				// get the decoder
    				nsCOMPtr<nsIUnicodeDecoder> decoder;
    				nsCOMPtr<nsICharsetConverterManager> ccm = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
    				rv = ccm->GetUnicodeDecoderRaw("UTF-8", getter_AddRefs(decoder));

    				if( NS_SUCCEEDED(rv) )
							{

    					decoder->GetMaxLength(data, dataLen, &outUnicodeLen);   // |outUnicodeLen| is number of chars
    					if (outUnicodeLen) {
      					unicodeData = reinterpret_cast<PRUnichar*>(nsMemory::Alloc((outUnicodeLen + 1) * sizeof(PRUnichar)));
      					if ( unicodeData ) {
        					PRInt32 numberTmp = dataLen;
        					rv = decoder->Convert(data, &numberTmp, unicodeData, &outUnicodeLen);
#ifdef DEBUG_CLIPBOARD
        					if (numberTmp != dataLen)
        					  printf("didn't consume all the bytes\n");
#endif

        					(unicodeData)[outUnicodeLen] = '\0';    // null terminate. Convert() doesn't do it for us
      						}
								} // if valid length


    					data = reinterpret_cast<char*>(unicodeData);
    					dataLen = outUnicodeLen * 2;

							nsCOMPtr<nsISupports> genericDataWrapper;
							nsPrimitiveHelpers::CreatePrimitiveForData( flavorStr, data, dataLen, getter_AddRefs(genericDataWrapper) );
							aTransferable->SetTransferData( flavorStr, genericDataWrapper, dataLen );

							/* free the allocated memory */
							nsMemory::Free( unicodeData );

     					break;
							}
					}
    		}
			}
		}

		free( dont_use_flavour );
		PhClipboardPasteFinish( clipPtr );
	}

	return NS_OK;
}

NS_IMETHODIMP
nsClipboard::HasDataMatchingFlavors(const char** aFlavorList,
                                    PRUint32 aLength,
                                    PRInt32 aWhichClipboard,
                                    PRBool * outResult)
{
	if (aWhichClipboard == kSelectionClipboard)
		return (NS_ERROR_FAILURE);
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
  nsCAutoString foundFlavor;
  if (aLength > 0) {
    void         *clipPtr;
		char					type[8];
    PhClipHeader *cliphdr;

    clipPtr = PhClipboardPasteStart( 1 );
    if(nsnull == clipPtr)
        return res;

    for ( PRUint32 i = 0; i < aLength; ++i ) {
      nsresult err = GetFormat( aFlavorList[i], type );
      if (err != NS_OK) continue;

      cliphdr = PhClipboardPasteType( clipPtr, type );
      if (cliphdr)
      {
        res = NS_OK;
        *outResult = PR_TRUE;
        break;
      }
    }
    PhClipboardPasteFinish( clipPtr );
  }

  return res;
}


nsresult nsClipboard::GetFormat(const char* aMimeStr, char *format ) 
{
 	nsDependentCString mimeStr(aMimeStr);
	int ret = NS_OK;

 	if( mimeStr.Equals(kUnicodeMime) || mimeStr.Equals(kTextMime) )
   	strcpy( format, Ph_CLIPBOARD_TYPE_TEXT );
 	else if( mimeStr.Equals(kHTMLMime) )
   	strcpy( format, Ph_CLIPBOARD_TYPE_HTML );
 	else if (mimeStr.Equals("moz/bookmarkclipboarditem"))
   	strcpy( format, Ph_CLIPBOARD_TYPE_MOZ_BOOKMARK );
	else ret = NS_ERROR_FAILURE;
	return ret;
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
    break;
  }
  return transferable;
}

unsigned long nsClipboard::GetFlavourTimestamp( char *type)
{
	char fname[512];
	extern struct _Ph_ctrl *_Ph_;

  strcpy( fname, "/var/clipboard/" );
  if( access( fname, X_OK ) != 0 )
		return 0;

	struct stat buf;
	if( fstat( _Ph_->fd, &buf ) != 0 )
		return 0;

  if(gethostname(&fname[strlen(fname)],PATH_MAX-40)!=0)
    strcpy(&fname[strlen(fname)],"localhost");

  sprintf( &fname[strlen(fname)], "/%08x/%d.%s",buf.st_uid, mInputGroup, type );
	struct stat st;
	if( stat( fname, &st ) != 0 )
		return 0;

	return st.st_mtime;
}
