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

#include "nsISupportsArray.h"
#include "nsIClipboardOwner.h"
#include "nsIDataFlavor.h"

#include "nsIWidget.h"
#include "nsIComponentManager.h"
#include "nsWidgetsCID.h"

// interface definitions
static NS_DEFINE_IID(kIDataFlavorIID,    NS_IDATAFLAVOR_IID);

static NS_DEFINE_IID(kIWidgetIID,        NS_IWIDGET_IID);
static NS_DEFINE_IID(kWindowCID,         NS_WINDOW_CID);

NS_IMPL_ADDREF(nsBaseClipboard)
NS_IMPL_RELEASE(nsBaseClipboard)

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
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
*/ 
nsresult nsBaseClipboard::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv = NS_NOINTERFACE;

  /*static NS_DEFINE_IID(kIClipboard, NS_ICLIPBOARD_IID);
  if (aIID.Equals(kIClipboard)) {
    *aInstancePtr = (void*) ((nsIClipboard*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }*/

  return rv;
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
  * This checks to see if the transferable in the clipboard object supports
  * a particular data flavor
  *
  */
NS_IMETHODIMP nsBaseClipboard::IsDataFlavorSupported(nsIDataFlavor * aDataFlavor)
{
  // make sure we have a good transferable
  if (nsnull == mTransferable) {
    return NS_ERROR_FAILURE;
  }

  return mTransferable->IsDataFlavorSupported(aDataFlavor);
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
