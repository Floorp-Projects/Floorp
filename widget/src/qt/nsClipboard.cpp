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
 *   Denis Issoupov <denis@macadamian.com>
 *   John C. Griggs <johng@corel.com>
 *   Dan Rosen <dr@netscape.com>
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

#ifdef NDEBUG
#define NO_DEBUG
#endif

#include "nsClipboard.h"
#include "nsMime.h"
#include "nsCOMPtr.h"
#include "nsCRT.h"
#include "nsString.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"
#include "nsPrimitiveHelpers.h"
#include "nsIComponentManager.h"
#include "nsWidgetsCID.h"

#include <qapplication.h>
#include <qclipboard.h>
#include <qdragobject.h> 

// interface definitions
static NS_DEFINE_CID(kCClipboardCID, NS_CLIPBOARD_CID);

NS_IMPL_ISUPPORTS1(nsClipboard, nsIClipboard);

//-------------------------------------------------------------------------
//
// nsClipboard constructor
//
//-------------------------------------------------------------------------
nsClipboard::nsClipboard()
{
  NS_INIT_REFCNT();
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
}

NS_IMETHODIMP nsClipboard::SetNativeClipboardData(PRInt32 aWhichClipboard)
{
  mIgnoreEmptyNotification = PR_TRUE;

  nsCOMPtr<nsITransferable> transferable(getter_AddRefs(GetTransferable(aWhichClipboard)));

    // make sure we have a good transferable
  if (nsnull == transferable) {
#ifdef NS_DEBUG
    printf("nsClipboard::SetNativeClipboardData(): no transferable!\n");
#endif
    return NS_ERROR_FAILURE;
  }
  // get flavor list that includes all flavors that can be written (including 
  // ones obtained through conversion)
  nsCOMPtr<nsISupportsArray> flavorList;
  nsresult errCode = transferable->FlavorsTransferableCanExport(getter_AddRefs(flavorList));
  if (NS_FAILED(errCode)) {
#ifdef NS_DEBUG
    printf("nsClipboard::SetNativeClipboardData(): no FlavorsTransferable !\n");
#endif
    return NS_ERROR_FAILURE;
  }
  QClipboard *cb = QApplication::clipboard();
  nsMimeStore* mimeStore =  new nsMimeStore();
  PRUint32 cnt;

  flavorList->Count(&cnt);
  for (PRUint32 i = 0; i < cnt; ++i) {
    nsCOMPtr<nsISupports> genericFlavor;

    flavorList->GetElementAt(i,getter_AddRefs(genericFlavor));

    nsCOMPtr<nsISupportsString> currentFlavor(do_QueryInterface(genericFlavor));

    if (currentFlavor) {
      nsXPIDLCString flavorStr;

      currentFlavor->ToString(getter_Copies(flavorStr));

      // add these types as selection targets
      PRUint32   len;
      void* data;
      nsCOMPtr<nsISupports> clip;

      transferable->GetTransferData(flavorStr,getter_AddRefs(clip),&len);

      nsPrimitiveHelpers::CreateDataFromPrimitive(flavorStr,clip,&data,len);
      mimeStore->AddFlavorData(flavorStr,data,len);
    }
  }
  cb->setData(mimeStore);
  mIgnoreEmptyNotification = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP 
nsClipboard::GetNativeClipboardData(nsITransferable *aTransferable,
                                    PRInt32 aWhichClipboard)
{
  // make sure we have a good transferable
  if (nsnull == aTransferable) {
#ifdef NS_DEBUG
    printf("  GetNativeClipboardData: Transferable is null!\n");
#endif
    return NS_ERROR_FAILURE;
  }
  // get flavor list that includes all acceptable flavors (including
  // ones obtained through conversion)
  nsCOMPtr<nsISupportsArray> flavorList;
  nsresult errCode = aTransferable->FlavorsTransferableCanImport(getter_AddRefs(flavorList));

  if (NS_FAILED(errCode)) {
#ifdef NS_DEBUG
    printf("nsClipboard::GetNativeClipboardData(): no FlavorsTransferable %i !\n",
           errCode);
#endif
    return NS_ERROR_FAILURE;
  }
  QClipboard *cb = QApplication::clipboard();
  QMimeSource *ms = cb->data();

  // Walk through flavors and see which flavor matches the one being pasted:
  PRUint32 cnt;

  flavorList->Count(&cnt);

  nsCAutoString foundFlavor;
  for (PRUint32 i = 0; i < cnt; ++i) {
    nsCOMPtr<nsISupports> genericFlavor;

    flavorList->GetElementAt(i,getter_AddRefs(genericFlavor));
    nsCOMPtr<nsISupportsString> currentFlavor(do_QueryInterface(genericFlavor));

    if (currentFlavor) {
      nsXPIDLCString flavorStr;

      currentFlavor->ToString(getter_Copies(flavorStr));
      foundFlavor = nsCAutoString(flavorStr);

      if (ms->provides((const char*)flavorStr)) {
	QByteArray ba = ms->encodedData((const char*)flavorStr);
        nsCOMPtr<nsISupports> genericDataWrapper;
	PRUint32 len = (PRUint32)ba.count();

        nsPrimitiveHelpers::CreatePrimitiveForData(foundFlavor, 
 						   (void*)ba.data(),len, 
                                                   getter_AddRefs(genericDataWrapper));
        aTransferable->SetTransferData(foundFlavor,genericDataWrapper,len);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP nsClipboard::ForceDataToClipboard(PRInt32 aWhichClipboard)
{
  // make sure we have a good transferable
  nsCOMPtr<nsITransferable> transferable(getter_AddRefs(GetTransferable(aWhichClipboard)));
  if (nsnull == transferable) {
    return NS_ERROR_FAILURE;
  }
   return NS_OK;
}

/* inline */
nsITransferable *nsClipboard::GetTransferable(PRInt32 aWhichClipboard)
{
  nsITransferable *transferable = nsnull;

  switch (aWhichClipboard) {
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

//-------------------------------------------------------------------------
NS_IMETHODIMP nsClipboard::HasDataMatchingFlavors(nsISupportsArray *aFlavorList,
                                                  PRInt32 aWhichClipboard,
                                                  PRBool           *_retval)
{
  *_retval = PR_FALSE;
  if (aWhichClipboard != kGlobalClipboard)
    return NS_OK;

  QClipboard *cb = QApplication::clipboard();
  QMimeSource *ms = cb->data();
  PRUint32 cnt;

  aFlavorList->Count(&cnt);
  for (PRUint32 i = 0;i < cnt; ++i) {
    nsCOMPtr<nsISupports> genericFlavor;

    aFlavorList->GetElementAt(i,getter_AddRefs(genericFlavor));

    nsCOMPtr<nsISupportsString> currentFlavor(do_QueryInterface(genericFlavor));
    if (currentFlavor) {
      nsXPIDLCString flavorStr;

      currentFlavor->ToString(getter_Copies(flavorStr));

#ifdef NS_DEBUG
      if (strcmp(flavorStr,kTextMime) == 0)
        NS_WARNING("DO NOT USE THE text/plain DATA FLAVOR ANY MORE. USE text/unicode INSTEAD");
#endif

      if (ms->provides((const char*)flavorStr)) {
        *_retval = PR_TRUE;
#ifdef NS_DEBUG
        printf("GetFormat %s\n",(const char*)flavorStr);
#endif
        break;
      }
    }
  }
  return NS_OK;
}

/**
  * Sets the transferable object
  */
NS_IMETHODIMP nsClipboard::SetData(nsITransferable *aTransferable,
                                   nsIClipboardOwner *anOwner,
                                   PRInt32 aWhichClipboard)
{
  if ((aTransferable == mGlobalTransferable.get()
       && anOwner == mGlobalOwner.get()
       && aWhichClipboard == kGlobalClipboard)
      || (aTransferable == mSelectionTransferable.get()
          && anOwner == mSelectionOwner.get()
          && aWhichClipboard == kSelectionClipboard)) {
    return NS_OK;
  }
  EmptyClipboard(aWhichClipboard);

  switch (aWhichClipboard) {
    case kSelectionClipboard:
      mSelectionOwner = anOwner;
      mSelectionTransferable = aTransferable;
      break;

    case kGlobalClipboard:
      mGlobalOwner = anOwner;
      mGlobalTransferable = aTransferable;
      break;
  }
  QApplication::clipboard()->clear();
  return SetNativeClipboardData(aWhichClipboard);
}

/**
  * Gets the transferable object
  */
NS_IMETHODIMP nsClipboard::GetData(nsITransferable *aTransferable,PRInt32 aWhichClipboard)
{
  if (nsnull != aTransferable) {
     return GetNativeClipboardData(aTransferable,aWhichClipboard);
#ifdef NS_DEBUG
  } else {
     printf("  nsClipboard::GetData(), aTransferable is NULL.\n");
#endif
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsClipboard::EmptyClipboard(PRInt32 aWhichClipboard)
{
  if (mIgnoreEmptyNotification) {
    return NS_OK;
  }
  switch(aWhichClipboard) {
    case kSelectionClipboard:
      if (mSelectionOwner) {
        mSelectionOwner->LosingOwnership(mSelectionTransferable);
        mSelectionOwner = nsnull;
      }
      mSelectionTransferable = nsnull;
      break;

    case kGlobalClipboard:
      if (mGlobalOwner) {
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
