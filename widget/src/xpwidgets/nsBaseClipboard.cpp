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

#include "nsBaseClipboard.h"

#include "nsIClipboardOwner.h"
#include "nsCOMPtr.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"

nsBaseClipboard::nsBaseClipboard()
{
  mClipboardOwner          = nsnull;
  mTransferable            = nsnull;
  mIgnoreEmptyNotification = PR_FALSE;

}

nsBaseClipboard::~nsBaseClipboard()
{
  EmptyClipboard(kSelectionClipboard);
  EmptyClipboard(kGlobalClipboard);
}

NS_IMPL_ISUPPORTS1(nsBaseClipboard, nsIClipboard)

/**
  * Sets the transferable object
  *
  */
NS_IMETHODIMP nsBaseClipboard::SetData(nsITransferable * aTransferable, nsIClipboardOwner * anOwner,
                                        PRInt32 aWhichClipboard)
{
  NS_ASSERTION ( aTransferable, "clipboard given a null transferable" );

  if (aTransferable == mTransferable && anOwner == mClipboardOwner)
    return NS_OK;
  PRBool selectClipPresent;
  SupportsSelectionClipboard(&selectClipPresent);
  if ( !selectClipPresent && aWhichClipboard != kGlobalClipboard )
    return NS_ERROR_FAILURE;

  EmptyClipboard(aWhichClipboard);

  mClipboardOwner = anOwner;
  if ( anOwner )
    NS_ADDREF(mClipboardOwner);

  mTransferable = aTransferable;
  
  nsresult rv = NS_ERROR_FAILURE;

  if ( mTransferable ) {
    NS_ADDREF(mTransferable);
    if (!mPrivacyHandler) {
      rv = NS_NewClipboardPrivacyHandler(getter_AddRefs(mPrivacyHandler));
      NS_ENSURE_SUCCESS(rv, rv);
    }
    rv = mPrivacyHandler->PrepareDataForClipboard(mTransferable);
    NS_ENSURE_SUCCESS(rv, rv);
    rv = SetNativeClipboardData(aWhichClipboard);
  }
  
  return rv;
}

/**
  * Gets the transferable object
  *
  */
NS_IMETHODIMP nsBaseClipboard::GetData(nsITransferable * aTransferable, PRInt32 aWhichClipboard)
{
  NS_ASSERTION ( aTransferable, "clipboard given a null transferable" );
  
  PRBool selectClipPresent;
  SupportsSelectionClipboard(&selectClipPresent);
  if ( !selectClipPresent && aWhichClipboard != kGlobalClipboard )
    return NS_ERROR_FAILURE;

  if ( aTransferable )
    return GetNativeClipboardData(aTransferable, aWhichClipboard);

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsBaseClipboard::EmptyClipboard(PRInt32 aWhichClipboard)
{
  PRBool selectClipPresent;
  SupportsSelectionClipboard(&selectClipPresent);
  if ( !selectClipPresent && aWhichClipboard != kGlobalClipboard )
    return NS_ERROR_FAILURE;

  if (mIgnoreEmptyNotification)
    return NS_OK;

  if ( mClipboardOwner ) {
    mClipboardOwner->LosingOwnership(mTransferable);
    NS_RELEASE(mClipboardOwner);
  }

  NS_IF_RELEASE(mTransferable);

  return NS_OK;
}

NS_IMETHODIMP
nsBaseClipboard::HasDataMatchingFlavors(const char** aFlavorList,
                                        PRUint32 aLength,
                                        PRInt32 aWhichClipboard,
                                        PRBool* outResult) 
{
  *outResult = PR_TRUE;  // say we always do.
  return NS_OK;
}

NS_IMETHODIMP
nsBaseClipboard::SupportsSelectionClipboard(PRBool* _retval)
{
  *_retval = PR_FALSE;   // we don't support the selection clipboard by default.
  return NS_OK;
}
