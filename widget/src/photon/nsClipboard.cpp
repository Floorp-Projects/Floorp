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

#if defined(DEBUG)
#define DEBUG_CLIPBOARD
#endif

//-------------------------------------------------------------------------
//
// nsClipboard constructor
//
//-------------------------------------------------------------------------
nsClipboard::nsClipboard() : nsBaseClipboard()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsClipboard::nsClipboard this=<%p>\n", this));
}

//-------------------------------------------------------------------------
// nsClipboard destructor
//-------------------------------------------------------------------------
nsClipboard::~nsClipboard()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsClipboard::~nsClipboard this=<%p>\n", this));
}


//-------------------------------------------------------------------------
NS_IMETHODIMP nsClipboard::ForceDataToClipboard(PRInt32 aWhichClipboard)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsClipboard::ForceDataToClipboard this=<%p>\n", this));

#ifdef DEBUG_CLIPBOARD
  printf("nsClipboard::ForceDataToClipboard this=<%p>\n", this);
#endif

  // make sure we have a good transferable
  if (nsnull == mTransferable) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}


//-------------------------------------------------------------------------
NS_IMETHODIMP nsClipboard::SetNativeClipboardData(PRInt32 aWhichClipboard)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsClipboard::SetNativeClipboardData this=<%p>\n", this));

#ifdef DEBUG_CLIPBOARD
  printf("nsClipboard::SetNativeClipboardData this=<%p>\n", this);
#endif

  nsresult res = NS_ERROR_FAILURE;

  // make sure we have a good transferable
  if (nsnull == mTransferable)
  {
    printf("nsClipboard::SetNativeClipboardData(): no transferable!\n");
    return NS_ERROR_FAILURE;
  }

  // get flavor list that includes all flavors that can be written (including ones 
  // obtained through conversion)
  nsCOMPtr<nsISupportsArray> flavorList;
  nsresult errCode = mTransferable->FlavorsTransferableCanExport ( getter_AddRefs(flavorList) );
  if ( NS_FAILED(errCode) )
    return NS_ERROR_FAILURE;
	
  PRUint32 cnt;
  flavorList->Count(&cnt);
  if (cnt)
  {
    PhClipHeader *cliphdr = (PhClipHeader *) malloc( cnt * sizeof( PhClipHeader ));

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsClipboard::SetNativeClipboardData cnt=<%d> cliphdr=<%p>\n", cnt, cliphdr));

    if( cliphdr )
    {    
      PRUint32   i=0, index=0;
      nsString  *df;
      void      *data = nsnull;
      PRUint32   dataLen;

       mIgnoreEmptyNotification = PR_TRUE;

       for ( PRUint32 i=0; i<cnt; ++i )
       {
         nsCOMPtr<nsISupports> genericFlavor;
         flavorList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
         nsCOMPtr<nsISupportsString> currentFlavor ( do_QueryInterface(genericFlavor) );
         if ( currentFlavor )
		 {
           nsXPIDLCString flavorStr;
           currentFlavor->ToString(getter_Copies(flavorStr));
 		   nsresult err = GetFormat( flavorStr, &cliphdr[index] );
           if (err != NS_OK)
		     continue;

           // Get data out of transferable.
           nsCOMPtr<nsISupports> genericDataWrapper;
           mTransferable->GetTransferData(flavorStr, 
                                          getter_AddRefs(genericDataWrapper),
                                          &dataLen);
										  
           nsPrimitiveHelpers::CreateDataFromPrimitive ( flavorStr, genericDataWrapper, &data, dataLen );
 
           PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsClipboard::SetNativeClipboardData adding %d index=<%d> type=<%s> length=%d data=<%s>\n", i, index, cliphdr[index].type, dataLen, data));


printf("nsClipboard::setNativeClipboardData 1 data=<%s> dataLen=<%d> \n", data, dataLen );

#if defined(DEBUG) && 0
{
  unsigned char *ptr = data;
  int i=0;
   printf("nsClipboard::setNativeClipboardData before:  ");
   for(i=0; i < dataLen; ptr++,i++)
   {
     printf("%x ",*ptr);
   }
   printf("\n");
}
#endif

           int     len;
           char *mbsbuffer = (char *) calloc(dataLen,1);					/* MEMORY LEAK */
           len = wcstombs (mbsbuffer, (const wchar_t*) data, dataLen);

printf("nsClipboard::setNativeClipboardData 2 mbsbuffer=<%s> dataLen=<%d> len=<%d> \n", mbsbuffer, dataLen, len );

#if defined(DEBUG) && 0
{
  unsigned char *ptr = mbsbuffer;
  int i=0;
   printf("nsClipboard::setNativeClipboardData after:  ");
   for(i=0; i < len; ptr++,i++)
   {
     printf("%x ",*ptr);
   }
   printf("\n");

}
#endif

#if 0
           cliphdr[index].length = dataLen;
           cliphdr[index].data = data;
#endif

#if 0
           cliphdr[index].length = len;
           cliphdr[index].data = mbsbuffer;
#endif

#if 1
           cliphdr[index].length = strlen(mbsbuffer) + 1;	/* Add the NULL */
           cliphdr[index].data = mbsbuffer;
#endif

           index++;
         }
       }
       PhClipboardCopy( 1, index, cliphdr );
	   for(i=0; i<index; i++)
	     nsCRT::free ( NS_REINTERPRET_CAST(char*, cliphdr[i].data) );

       res = NS_OK;
       mIgnoreEmptyNotification = PR_FALSE;
    }
  }
  
  return res;
}


//-------------------------------------------------------------------------
NS_IMETHODIMP
nsClipboard::GetNativeClipboardData(nsITransferable * aTransferable, 
                                    PRInt32 aWhichClipboard)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsClipboard::GetNativeClipboardData this=<%p>\n", this));
#ifdef DEBUG_CLIPBOARD
  printf("nsClipboard::GetNativeClipboardData this=<%p>\n", this);
#endif

  nsresult res = NS_ERROR_FAILURE;

  // make sure we have a good transferable
  if (nsnull == aTransferable) {
    printf("  GetNativeClipboardData: Transferable is null!\n");
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
    PhClipHeader  cliptype;
    PhClipHeader *cliphdr;
    void         *data = nsnull;
    PRUint32      dataLen;

    clipPtr = PhClipboardPasteStart( 1 );

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsClipboard::GetNativeClipboardData cnt=<%d>\n", cnt));

    for ( PRUint32 i = 0; i < cnt; ++i )
    {
      nsCOMPtr<nsISupports> genericFlavor;
      flavorList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
      nsCOMPtr<nsISupportsString> currentFlavor ( do_QueryInterface(genericFlavor) );
      if ( currentFlavor )
	  {
        nsXPIDLCString flavorStr;
        currentFlavor->ToString ( getter_Copies(flavorStr) );
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsClipboard::GetNativeClipboardData looking for=<%s>\n", (const char *) flavorStr));
        nsresult err = GetFormat( flavorStr, &cliptype);
        if (err != NS_OK)
		     continue;

        cliphdr = PhClipboardPasteType( clipPtr, cliptype.type );
        if (cliphdr)
		{
		  data = cliphdr->data;
		  dataLen = cliphdr->length;

printf("nsClipboard::GetNativeClipboardData 1 data=<%s> dataLen=<%d> \n", data, dataLen );

#if defined(DEBUG) && 0
{
  unsigned char *ptr = data;
  int i=0;
   printf("nsClipboard::GetNativeClipboardData before:  ");
   for(i=0; i < dataLen; ptr++,i++)
   {
     printf("%x ",*ptr);
   }
   printf("\n");
}
#endif

           int     len;
           wchar_t    *wbuffer  = (wchar_t *) calloc(dataLen*3, 1);					/* MEMORY LEAK */
           len = mbstowcs(wbuffer, (const char *) data, dataLen*3);

        printf("nsClipboard::GetNativeClipboardData  %s(%d)\n", data, len );
        
           data = wbuffer;         
           dataLen = len*2;

printf("nsClipboard::GetNativeClipboardData 2 data=<%s> dataLen=<%d>\n", data, dataLen );

#if defined(DEBUG) && 0
{
  unsigned char *ptr = data;
  int i=0;
   printf("nsClipboard::GetNativeClipboardData after:  ");
   for(i=0; i < dataLen; ptr++,i++)
   {
     printf("%x ",*ptr);
   }
   printf("\n");
}
#endif

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
          nsPrimitiveHelpers::CreatePrimitiveForData ( flavorStr, data, dataLen, getter_AddRefs(genericDataWrapper) );
          aTransferable->SetTransferData(flavorStr,
                                         genericDataWrapper,
                                        dataLen);
          printf("nsClipboard::GetNativeClipboardData flavorStr=<%s> length=<%d> data=<%s>\n", cliptype.type, dataLen, data );
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
                                    PRBool * outResult)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsClipboard::HasDataMatchingFlavors this=<%p>\n", this));

#ifdef DEBUG_CLIPBOARD
  PRUint32 cnt;
  aFlavorList->Count(&cnt);
  if (cnt)
  {
    PRUint32   i;

      for ( PRUint32 i=0; i<cnt; ++i )
      {
         nsCOMPtr<nsISupports> genericFlavor;
         aFlavorList->GetElementAt ( i, getter_AddRefs(genericFlavor) );
         nsCOMPtr<nsISupportsString> currentFlavor ( do_QueryInterface(genericFlavor) );
         if ( currentFlavor )
		 {
           nsXPIDLCString flavorStr;
           currentFlavor->ToString(getter_Copies(flavorStr));
           printf("nsClipboard::HasDataMatchingFlavors : <%s>\n", (const char *) flavorStr);
         }
      }
  }
#endif

  *outResult = PR_TRUE;  // say we always do.
  return NS_OK;
}

//=========================================================================

//-------------------------------------------------------------------------
nsresult nsClipboard::GetFormat(const char* aMimeStr, PhClipHeader *cliphdr )
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsClipboard::GetFormat this=<%p> aMimeStr=<%s>\n", this, aMimeStr));

#ifdef DEBUG_CLIPBOARD
  printf("nsClipboard::GetFormat this=<%p> aMimeStr=<%s>\n", this, aMimeStr);
#endif


  nsCAutoString mimeStr ( CBufDescriptor(NS_CONST_CAST(char*,aMimeStr), PR_TRUE, PL_strlen(aMimeStr)+1) );

  cliphdr->type[0]=0;

  if (mimeStr.Equals(kUnicodeMime))
  {
    strcpy( cliphdr->type, Ph_CLIPBOARD_TYPE_TEXT );
  }

 if (cliphdr->type[0] == 0)
   return NS_ERROR_FAILURE;
 else
   return NS_OK;
}
