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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Takashi Toyoshima <toyoshim@be-in.org>
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
#include "nsITransferable.h"
#include "nsWidgetsCID.h"
#include "nsXPIDLString.h"
#include "nsPrimitiveHelpers.h"
#include "nsISupportsPrimitives.h"
#include "nsString.h"

#include <View.h>
#include <Clipboard.h>
#include <Message.h>

// The class statics:
BView *nsClipboard::sView = 0;

#if defined(DEBUG_akkana) || defined(DEBUG_mcafee) || defined(DEBUG_toyoshim)
#  define DEBUG_CLIPBOARD
#endif
 

//-------------------------------------------------------------------------
//
// nsClipboard constructor
//
//-------------------------------------------------------------------------
nsClipboard::nsClipboard() : nsBaseClipboard()
{
#ifdef DEBUG_CLIPBOARD
  printf("  nsClipboard::nsClipboard()\n");
#endif /* DEBUG_CLIPBOARD */

  //NS_INIT_REFCNT();
  mIgnoreEmptyNotification = PR_FALSE;
  mClipboardOwner = nsnull;
  mTransferable   = nsnull;
}

//-------------------------------------------------------------------------
//
// nsClipboard destructor
//
//-------------------------------------------------------------------------
nsClipboard::~nsClipboard()
{
#ifdef DEBUG_CLIPBOARD
  printf("  nsClipboard::~nsClipboard()\n");  
#endif /* DEBUG_CLIPBOARD */
}

void nsClipboard::SetTopLevelView(BView *v)
{
  // Don't set up any more event handlers if we're being called twice
  // for the same toplevel widget
  if (sView == v)
    return;

  if (sView != 0 && sView->Window() != 0)
    return;

  if(v == 0 || v->Window() == 0)
  {
#ifdef DEBUG_CLIPBOARD
    printf("  nsClipboard::SetTopLevelView: widget passed in is null or has no window!\n");
#endif /* DEBUG_CLIPBOARD */
    return;
  }

#ifdef DEBUG_CLIPBOARD
  printf("  nsClipboard::SetTopLevelView\n");
#endif /* DEBUG_CLIPBOARD */
}

//
// The copy routine
//
NS_IMETHODIMP nsClipboard::SetNativeClipboardData(PRInt32 aWhichClipboard)
{
  mIgnoreEmptyNotification = PR_TRUE;

#ifdef DEBUG_CLIPBOARD
  printf("  nsClipboard::SetNativeClipboardData()\n");
#endif /* DEBUG_CLIPBOARD */

  // make sure we have a good transferable
  if (nsnull == mTransferable) {
#ifdef DEBUG_CLIPBOARD
    printf("  SetNativeClipboardData: no transferable!\n");
#endif /* DEBUG_CLIPBOARD */
    return NS_ERROR_FAILURE;
  }

  // lock the native clipboard
  if (!be_clipboard->Lock())
    return NS_ERROR_FAILURE;

  // clear the native clipboard
  nsresult rv = NS_OK;
  if (B_OK == be_clipboard->Clear()) {
    // set data to the native clipboard
    if (BMessage *msg = be_clipboard->Data()) {
      // Get the transferable list of data flavors
      nsCOMPtr<nsISupportsArray> dfList;
      mTransferable->FlavorsTransferableCanExport(getter_AddRefs(dfList));

      // Walk through flavors that contain data and register them
      // into the BMessage as supported flavors
      PRUint32 i;
      PRUint32 cnt;
      dfList->Count(&cnt);
      for (i = 0; i < cnt && rv == NS_OK; i++) {
        nsCOMPtr<nsISupports> genericFlavor;
        dfList->GetElementAt(i, getter_AddRefs(genericFlavor));
        nsCOMPtr<nsISupportsString> currentFlavor (do_QueryInterface(genericFlavor));
        if (currentFlavor) {
          nsXPIDLCString flavorStr;
          currentFlavor->ToString(getter_Copies(flavorStr));

#ifdef DEBUG_CLIPBOARD
          printf("nsClipboard: %d/%d = %s\n", i, cnt, (const char *)flavorStr);
#endif /* DEBUG_CLIPBOARD */
          if (0 == strncmp(flavorStr, "text/", 5)) {
            // [NS] text/ * => [Be] text/ *
            void *data = nsnull;
            PRUint32 dataSize = 0;
            nsCOMPtr<nsISupports> genericDataWrapper;
            rv = mTransferable->GetTransferData(flavorStr, getter_AddRefs(genericDataWrapper), &dataSize);
            nsPrimitiveHelpers::CreateDataFromPrimitive(flavorStr, genericDataWrapper, &data, dataSize);
#ifdef DEBUG_CLIPBOARD
            if (NS_FAILED(rv))
              printf("nsClipboard: Error getting data from transferable\n");
#endif /* DEBUG_CLIPBOARD */
            if (dataSize && data != nsnull) {
              NS_ConvertUCS2toUTF8 cv((const PRUnichar *)data, (PRUint32)dataSize / 2);
              const char *utf8Str = cv.get();
              uint32 utf8Len = strlen(utf8Str);
#ifdef DEBUG_CLIPBOARD
              if (0 == strcmp(flavorStr, kUnicodeMime))
                printf(" => [%s]%s\n", kTextMime, utf8Str);
              else
                printf(" => [%s]%s\n", (const char *)flavorStr, utf8Str);
#endif /* DEBUG_CLIPBOARD */
              status_t rc;
              if (0 == strcmp(flavorStr, kUnicodeMime)) {
                // [NS] text/unicode => [Be] text/plain
                rc = msg->AddData(kTextMime, B_MIME_TYPE, (void *)utf8Str, utf8Len);
              } else {
                // [NS] text/ * => [Be] text/ *
                rc = msg->AddData((const char *)flavorStr, B_MIME_TYPE, (void *)utf8Str, utf8Len);
              }
              if (rc != B_OK)
                rv = NS_ERROR_FAILURE;
            } else {
#ifdef DEBUG_CLIPBOARD
              printf("nsClipboard: Error null data from transferable\n");
#endif /* DEBUG_CLIPBOARD */
                // not fatal. force to continue...
                rv = NS_OK;
            }
          } else {
            // [NS] * / * => [Be] * / *
            void *data = nsnull;
            PRUint32 dataSize = 0;
            nsCOMPtr<nsISupports> genericDataWrapper;
            rv = mTransferable->GetTransferData(flavorStr, getter_AddRefs(genericDataWrapper), &dataSize);
            nsPrimitiveHelpers::CreateDataFromPrimitive(flavorStr, genericDataWrapper, &data, dataSize);
#ifdef DEBUG_CLIPBOARD
            if (NS_FAILED(rv))
              printf("nsClipboard: Error getting data from transferable\n");
#endif /* DEBUG_CLIPBOARD */
            if (dataSize && data != nsnull) {
#ifdef DEBUG_CLIPBOARD
              printf("[%s](binary)\n", (const char *)flavorStr);
#endif /* DEBUG_CLIPBOARD */
              if (B_OK != msg->AddData((const char *)flavorStr, B_MIME_TYPE, data, dataSize))
                rv = NS_ERROR_FAILURE;
            }
          }
        } else {
#ifdef DEBUG_CLIPBOARD
          printf("nsClipboard: Error getting flavor\n");
#endif /* DEBUG_CLIPBOARD */
          rv = NS_ERROR_FAILURE;
        }
      } /* for */
    } else {
      rv = NS_ERROR_FAILURE;
    }
  } else {
    rv = NS_ERROR_FAILURE;
  }
  if (B_OK != be_clipboard->Commit())
    rv = NS_ERROR_FAILURE;
  be_clipboard->Unlock();

  mIgnoreEmptyNotification = PR_FALSE;

  return rv;
}


//
// The blocking Paste routine
//
NS_IMETHODIMP
nsClipboard::GetNativeClipboardData(nsITransferable * aTransferable, PRInt32 aWhichClipboard )
{
#ifdef DEBUG_CLIPBOARD
  printf("  nsClipboard::GetNativeClipboardData()\n");
#endif /* DEBUG_CLIPBOARD */

  // make sure we have a good transferable
  if (nsnull == aTransferable) {
    printf("  GetNativeClipboardData: Transferable is null!\n");
    return NS_ERROR_FAILURE;
  }

  // get flavor list
  nsresult rv;
  nsCOMPtr<nsISupportsArray> flavorList;
  rv = aTransferable->FlavorsTransferableCanImport(getter_AddRefs(flavorList));
  if (NS_FAILED(rv))
    return NS_ERROR_FAILURE;

  // get native clipboard data
  if (!be_clipboard->Lock())
    return NS_ERROR_FAILURE;

  BMessage *msg = be_clipboard->Data();
  if (!msg)
    return NS_ERROR_FAILURE;  
#ifdef DEBUG_CLIPBOARD
  msg->PrintToStream();
#endif /* DEBUG_CLIPBOARD */

  PRUint32 cnt;
  flavorList->Count(&cnt);
  for (PRUint32 i = 0; i < cnt; i++) {
    nsCOMPtr<nsISupports> genericFlavor;
    flavorList->GetElementAt(i, getter_AddRefs(genericFlavor));
    nsCOMPtr<nsISupportsString> currentFlavor(do_QueryInterface(genericFlavor));
    if (currentFlavor) {
      nsXPIDLCString flavorStr;
      currentFlavor->ToString(getter_Copies(flavorStr));

#ifdef DEBUG_CLIPBOARD
      printf("nsClipboard: %d/%d = %s\n", i, cnt, (const char *)flavorStr);
#endif /* DEBUG_CLIPBOARD */
      const void *data;
      ssize_t size;
      if (0 == strncmp(flavorStr, "text/", 5)) {
        // [Be] text/ * => [NS] text/ *
        status_t rc;
        if (0 == strcmp(flavorStr, kUnicodeMime))
          rc = msg->FindData(kTextMime, B_MIME_TYPE, &data, &size);
        else
          rc = msg->FindData(flavorStr, B_MIME_TYPE, &data, &size);
        if (rc != B_OK || !data || !size) {
#ifdef DEBUG_CLIPBOARD
          printf("nsClipboard: not found in BMessage\n");
#endif /* DEBUG_CLIPBOARD */
        } else {
          nsString ucs2Str = NS_ConvertUTF8toUCS2((const char *)data, (PRUint32)size);
          nsCOMPtr<nsISupports> genericDataWrapper;
          nsPrimitiveHelpers::CreatePrimitiveForData(flavorStr, (void *)ucs2Str.get(), ucs2Str.Length() * 2, getter_AddRefs(genericDataWrapper));
          rv = aTransferable->SetTransferData(flavorStr, genericDataWrapper, ucs2Str.Length() * 2);
        }
      } else {
        // [Be] * / * => [NS] * / *
        if (B_OK != msg->FindData(flavorStr, B_MIME_TYPE, &data, &size)) {
#ifdef DEBUG_CLIPBOARD
          printf("nsClipboard: not found in BMessage\n");
#endif /* DEBUG_CLIPBOARD */
        } else {
          nsCOMPtr<nsISupports> genericDataWrapper;
          nsPrimitiveHelpers::CreatePrimitiveForData(flavorStr, (void *)data, (PRUint32)size, getter_AddRefs(genericDataWrapper));
          rv = aTransferable->SetTransferData(flavorStr, genericDataWrapper, size);
        }
      }
#ifdef DEBUG_CLIPBOARD
      if (NS_FAILED(rv))
        printf("nsClipboard: Error SetTransferData\n");
#endif /* DEBUG_CLIPBOARD */
    } else {
      rv = NS_ERROR_FAILURE;
#ifdef DEBUG_CLIPBOARD
      printf("nsClipboard: Error gerring flavor");
#endif /* DEBUG_CLIPBOARD */
    }
    if (rv != NS_OK)
      break;
  } /* for */

  be_clipboard->Unlock();

  return rv;
}

//
// No-op.
//
NS_IMETHODIMP nsClipboard::ForceDataToClipboard()
{
#ifdef DEBUG_CLIPBOARD
  printf("  nsClipboard::ForceDataToClipboard()\n");
#endif /* DEBUG_CLIPBOARD */

  // make sure we have a good transferable
  if (nsnull == mTransferable) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}
