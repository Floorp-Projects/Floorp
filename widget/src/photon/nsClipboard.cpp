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

#include "nsClipboard.h"
#include <Pt.h>

#include "nsCOMPtr.h"
//#include "nsDataObj.h"
#include "nsIClipboardOwner.h"
#include "nsString.h"
#include "nsIFormatConverter.h"
#include "nsITransferable.h"

#include "nsIWidget.h"
#include "nsIComponentManager.h"
#include "nsWidgetsCID.h"

#include "nsVoidArray.h"
#include "nsFileSpec.h"


//-------------------------------------------------------------------------
//
// nsClipboard constructor
//
//-------------------------------------------------------------------------
nsClipboard::nsClipboard() : nsBaseClipboard()
{
  //NS_INIT_REFCNT();
  //mDataObj        = nsnull;
  mIgnoreEmptyNotification = PR_FALSE;
  mWindow         = nsnull;

  // Create a Native window for the shell container...
  //nsresult rv = nsComponentManager::CreateInstance(kWindowCID, nsnull, kIWidgetIID, (void**)&mWindow);
  //mWindow->Show(PR_FALSE);
  //mWindow->Resize(1,1,PR_FALSE);
}

//-------------------------------------------------------------------------
// nsClipboard destructor
//-------------------------------------------------------------------------
nsClipboard::~nsClipboard()
{
  //NS_IF_RELEASE(mWindow);

  //EmptyClipboard();
//  if (nsnull != mDataObj) {
//    mDataObj->Release();
//  }

}


//-------------------------------------------------------------------------
NS_IMETHODIMP nsClipboard::ForceDataToClipboard()
{
printf( "ForceDataToClipboard()\n" );
  nsresult res = NS_ERROR_FAILURE;

// REVISIT - Need to fix this
#if 0
  // make sure we have a good transferable
  if( mTransferable )
  {

  //  HWND nativeWin = nsnull;//(HWND)mWindow->GetNativeData(NS_NATIVE_WINDOW);
  //  ::OpenClipboard(nativeWin);
  ///  ::EmptyClipboard();

    // Get the transferable list of data flavors
    nsVoidArray * dfList;
    mTransferable->GetTransferDataFlavors(&dfList);

    // Walk through flavors and see which flavor is on the native clipboard,
    int i;
    int cnt = dfList->Count();

    if( cnt )
    {
      PhClipHeader  *cliphdr = (PhClipHeader *) malloc( cnt * sizeof( PhClipHeader ));
      if( cliphdr )
      {
        for(i=0;i<cnt;i++)
        {
          nsString  *df = (nsString *)dfList->ElementAt(i);
          void      *data;
          PRUint32  dataLen;

          if( nsnull != df )
          {
            GetFormat( *df, &cliphdr[i] );
            // Get the data as a bunch-o-bytes from the clipboard
            mTransferable->GetTransferData( df, &data, &dataLen );
            cliphdr[i].length = dataLen;
            cliphdr[i].data = data;
          }
        }

        PhClipboardCopy( 1, cnt, cliphdr );
        res = NS_OK;
      }

      free( cliphdr );
    }

    delete dfList;
  }

  if( res == NS_OK )
    printf( "  ok.\n" );
  else
    printf( "  failed.\n" );
#endif

  return res;
}


//-------------------------------------------------------------------------
NS_IMETHODIMP nsClipboard::SetNativeClipboardData()
{
printf( "SetNativeClipboardData()\n" );

  nsresult res = NS_ERROR_FAILURE;

// REVISIT - Need to fix this
#if 0
  // make sure we have a good transferable
  if( mTransferable )
  {
    mIgnoreEmptyNotification = PR_TRUE;

    nsVoidArray * dfList;
    mTransferable->FlavorsTransferableCanExport( &dfList );

    PRUint32 cnt = dfList->Count();

    if( cnt )
    {
      PhClipHeader *cliphdr = (PhClipHeader *) malloc( cnt * sizeof( PhClipHeader ));

      if( cliphdr )
      {    
        PRUint32 i;
        nsString *df;
        void      *data;
        PRUint32  dataLen;

        for(i=0;i<cnt;i++)
        {
          df = (nsString *)dfList->ElementAt(i);
          if (nsnull != df)
          {
            GetFormat( *df, &cliphdr[i] );
            mTransferable->GetTransferData( df, &data, &dataLen );
            cliphdr[i].length = dataLen;
            cliphdr[i].data = data;
          }
        }

        PhClipboardCopy( 1, cnt, cliphdr );
        res = NS_OK;
        free( cliphdr );
      }
    }

    // Delete the data flavors list
    delete dfList;

    mIgnoreEmptyNotification = PR_FALSE;
  }

  if( res == NS_OK )
    printf( "  ok.\n" );
  else
    printf( "  failed.\n" );
#endif
  return res;
}


//-------------------------------------------------------------------------
NS_IMETHODIMP nsClipboard::GetNativeClipboardData(nsITransferable * aTransferable)
{
printf( "GetNativeClipboardData()\n" );

  nsresult res = NS_ERROR_FAILURE;

// REVISIT - Need to fix this
#if 0
  if( aTransferable )
  {
    nsVoidArray * dfList;
    aTransferable->GetTransferDataFlavors(&dfList);

    // Walk through flavors and see which flavor is on the clipboard them on the native clipboard,
    int i;
    int cnt = dfList->Count();

    if( cnt > 0 )
    {
      PhClipHeader *cliphdr;
      PhClipHeader cliptype;
      void         *data;
      PRUint32     dataLen;
      nsString     *df;
      void         *clipPtr;

      clipPtr = PhClipboardPasteStart( 1 );
        
      for(i=0;i<cnt;i++)
      {
        df = (nsString *)dfList->ElementAt(i);
        if( nsnull != df )
        {
          GetFormat( *df, &cliptype );
          cliphdr = PhClipboardPasteType( clipPtr, cliptype.type );

          aTransferable->SetTransferData( df, cliphdr->data, cliphdr->length );
        } 
      }
      
      PhClipboardPasteFinish( clipPtr );
      res = NS_OK;
    }

    delete dfList;
  }

  if( res == NS_OK )
    printf( "  ok.\n" );
  else
    printf( "  failed.\n" );
#endif

  return res;
}

//=========================================================================

//-------------------------------------------------------------------------
void nsClipboard::GetFormat(const nsString & aMimeStr, PhClipHeader *cliphdr )
{
  cliphdr->type[0]=0;

  if (aMimeStr.Equals(kTextMime))
  {
    strcpy( cliphdr->type, "TEXT" );
  }
  else if (aMimeStr.Equals(kUnicodeMime))
  {
    strcpy( cliphdr->type, "TEXT" );
  }
  else if (aMimeStr.Equals(kJPEGImageMime))
  {
    strcpy( cliphdr->type, "BMAP" );
  }
//  else if (aMimeStr.Equals(kDropFilesMime)) {
}
