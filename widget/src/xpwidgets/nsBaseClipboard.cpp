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

#include "nsBaseClipboard.h"

#include "nsIClipboardOwner.h"
#include "nsString.h"

#include "nsIWidget.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsISupportsPrimitives.h"


NS_IMPL_ADDREF(nsBaseClipboard)
NS_IMPL_RELEASE(nsBaseClipboard)
NS_IMPL_QUERY_INTERFACE1(nsBaseClipboard, nsIClipboard);


//-------------------------------------------------------------------------
//
// nsBaseClipboard constructor
//
//-------------------------------------------------------------------------
nsBaseClipboard::nsBaseClipboard()
{
  NS_INIT_REFCNT();
  mClipboardOwner          = nsnull;
  mTransferable            = nsnull;
  mIgnoreEmptyNotification = PR_FALSE;

}

//-------------------------------------------------------------------------
//
// nsBaseClipboard destructor
//
//-------------------------------------------------------------------------
nsBaseClipboard::~nsBaseClipboard()
{
  EmptyClipboard();
}


/**
  * Sets the transferable object
  *
  */
NS_IMETHODIMP nsBaseClipboard::SetData(nsITransferable * aTransferable, nsIClipboardOwner * anOwner)
{
  if (aTransferable == mTransferable && anOwner == mClipboardOwner) {
    return NS_OK;
  }

  EmptyClipboard();

  mClipboardOwner = anOwner;
  if (nsnull != anOwner) {
    NS_ADDREF(mClipboardOwner);
  }

  mTransferable = aTransferable;
  
  nsresult rv = NS_ERROR_FAILURE;

  if (nsnull != mTransferable) {
    NS_ADDREF(mTransferable);
    rv = SetNativeClipboardData();
  } else {
    printf("  nsBaseClipboard::SetData(), aTransferable is NULL.\n");
  }

  return rv;
}

/**
  * Gets the transferable object
  *
  */
NS_IMETHODIMP nsBaseClipboard::GetData(nsITransferable * aTransferable)
{
  if (nsnull != aTransferable) {
    return GetNativeClipboardData(aTransferable);
  } else {
    printf("  nsBaseClipboard::GetData(), aTransferable is NULL.\n");
  }

  return NS_ERROR_FAILURE;
}


/**
  * 
  *
  */
NS_IMETHODIMP nsBaseClipboard::EmptyClipboard()
{
  if (mIgnoreEmptyNotification) {
    return NS_OK;
  }

  if (nsnull != mClipboardOwner) {
    mClipboardOwner->LosingOwnership(mTransferable);
    NS_RELEASE(mClipboardOwner);
  }

  if (nsnull != mTransferable) {
    NS_RELEASE(mTransferable);
  }

  return NS_OK;
}


/**
  * 
  *
  */
NS_IMETHODIMP nsBaseClipboard::ForceDataToClipboard()
{
  return NS_OK;
}


//еее skanky hack until i can correctly re-create primitives from native data. i know this code sucks,
//еее please forgive me.
void
nsBaseClipboard :: CreatePrimitiveForData ( const char* aFlavor, void* aDataBuff, PRUint32 aDataLen, nsISupports** aPrimitive )
{
  if ( !aPrimitive )
    return;

  if ( strcmp(aFlavor,kTextMime) == 0 ) {
    nsCOMPtr<nsISupportsString> primitive;
    nsresult rv = nsComponentManager::CreateInstance(NS_SUPPORTS_STRING_PROGID, nsnull, 
                                                      NS_GET_IID(nsISupportsString), getter_AddRefs(primitive));
    if ( primitive ) {
      primitive->SetData ( (char*)aDataBuff );
      nsCOMPtr<nsISupports> genericPrimitive ( do_QueryInterface(primitive) );
      *aPrimitive = genericPrimitive;
      NS_ADDREF(*aPrimitive);
    }
  }
  else {
    nsCOMPtr<nsISupportsWString> primitive;
    nsresult rv = nsComponentManager::CreateInstance(NS_SUPPORTS_WSTRING_PROGID, nsnull, 
                                                      NS_GET_IID(nsISupportsWString), getter_AddRefs(primitive));
    if ( primitive ) {
      primitive->SetData ( (unsigned short*)aDataBuff );
      nsCOMPtr<nsISupports> genericPrimitive ( do_QueryInterface(primitive) );
      *aPrimitive = genericPrimitive;
      NS_ADDREF(*aPrimitive);
    }  
  }

} // CreatePrimitiveForData


void
nsBaseClipboard :: CreateDataFromPrimitive ( const char* aFlavor, nsISupports* aPrimitive, void** aDataBuff, PRUint32 aDataLen )
{
  if ( !aDataBuff )
    return;

  if ( strcmp(aFlavor,kTextMime) == 0 ) {
    nsCOMPtr<nsISupportsString> plainText ( do_QueryInterface(aPrimitive) );
    if ( plainText )
      plainText->GetData ( (char**)aDataBuff );
  }
  else {
    nsCOMPtr<nsISupportsWString> doubleByteText ( do_QueryInterface(aPrimitive) );
    if ( doubleByteText )
      doubleByteText->GetData ( (unsigned short**)aDataBuff );
  }

}

