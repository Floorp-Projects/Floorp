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
  EmptyClipboard(kSelectionClipboard);
  EmptyClipboard(kGlobalClipboard);
}


/**
  * Sets the transferable object
  *
  */
NS_IMETHODIMP nsBaseClipboard::SetData(nsITransferable * aTransferable, nsIClipboardOwner * anOwner,
                                        PRInt32 aWhichClipboard)
{
  if (aTransferable == mTransferable && anOwner == mClipboardOwner) {
    return NS_OK;
  }

  EmptyClipboard(aWhichClipboard);

  mClipboardOwner = anOwner;
  if (nsnull != anOwner) {
    NS_ADDREF(mClipboardOwner);
  }

  mTransferable = aTransferable;
  
  nsresult rv = NS_ERROR_FAILURE;

  if (nsnull != mTransferable) {
    NS_ADDREF(mTransferable);
    rv = SetNativeClipboardData(aWhichClipboard);
  } else {
    printf("  nsBaseClipboard::SetData(), aTransferable is NULL.\n");
  }

  return rv;
}

/**
  * Gets the transferable object
  *
  */
NS_IMETHODIMP nsBaseClipboard::GetData(nsITransferable * aTransferable, PRInt32 aWhichClipboard)
{
  if (nsnull != aTransferable) {
    return GetNativeClipboardData(aTransferable, aWhichClipboard);
  } else {
    printf("  nsBaseClipboard::GetData(), aTransferable is NULL.\n");
  }

  return NS_ERROR_FAILURE;
}


/**
  * 
  *
  */
NS_IMETHODIMP nsBaseClipboard::EmptyClipboard(PRInt32 aWhichClipboard)
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
NS_IMETHODIMP nsBaseClipboard::ForceDataToClipboard(PRInt32 aWhichClipboard)
{
  return NS_OK;
}


NS_IMETHODIMP
nsBaseClipboard :: HasDataMatchingFlavors ( nsISupportsArray* aFlavorList, PRInt32 aWhichClipboard, PRBool * outResult ) 
{
  *outResult = PR_TRUE;  // say we always do.
  return NS_OK;
}


NS_IMETHODIMP
nsBaseClipboard :: SupportsSelectionClipboard ( PRBool *_retval )
{
  *_retval = PR_FALSE;   // we don't suport the selection clipboard by default.
  return NS_OK;
}
