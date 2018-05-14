/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsClipboardHelper.h"

// basics
#include "nsCOMPtr.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIServiceManager.h"

// helpers
#include "nsIClipboard.h"
#include "nsIDocument.h"
#include "nsITransferable.h"
#include "nsReadableUtils.h"

NS_IMPL_ISUPPORTS(nsClipboardHelper, nsIClipboardHelper)

/*****************************************************************************
 * nsClipboardHelper ctor / dtor
 *****************************************************************************/

nsClipboardHelper::nsClipboardHelper()
{
}

nsClipboardHelper::~nsClipboardHelper()
{
  // no members, nothing to destroy
}

/*****************************************************************************
 * nsIClipboardHelper methods
 *****************************************************************************/

NS_IMETHODIMP
nsClipboardHelper::CopyStringToClipboard(const nsAString& aString,
                                         int32_t aClipboardID)
{
  nsresult rv;

  // get the clipboard
  nsCOMPtr<nsIClipboard>
    clipboard(do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(clipboard, NS_ERROR_FAILURE);

  bool clipboardSupported;
  // don't go any further if they're asking for the selection
  // clipboard on a platform which doesn't support it (i.e., unix)
  if (nsIClipboard::kSelectionClipboard == aClipboardID) {
    rv = clipboard->SupportsSelectionClipboard(&clipboardSupported);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!clipboardSupported)
      return NS_ERROR_FAILURE;
  }

  // don't go any further if they're asking for the find clipboard on a platform
  // which doesn't support it (i.e., non-osx)
  if (nsIClipboard::kFindClipboard == aClipboardID) {
    rv = clipboard->SupportsFindClipboard(&clipboardSupported);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!clipboardSupported)
      return NS_ERROR_FAILURE;
  }

  // create a transferable for putting data on the clipboard
  nsCOMPtr<nsITransferable>
    trans(do_CreateInstance("@mozilla.org/widget/transferable;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(trans, NS_ERROR_FAILURE);

  trans->Init(nullptr);

  // Add the text data flavor to the transferable
  rv = trans->AddDataFlavor(kUnicodeMime);
  NS_ENSURE_SUCCESS(rv, rv);

  // get wStrings to hold clip data
  nsCOMPtr<nsISupportsString>
    data(do_CreateInstance("@mozilla.org/supports-string;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(data, NS_ERROR_FAILURE);

  // populate the string
  rv = data->SetData(aString);
  NS_ENSURE_SUCCESS(rv, rv);

  // qi the data object an |nsISupports| so that when the transferable holds
  // onto it, it will addref the correct interface.
  nsCOMPtr<nsISupports> genericData(do_QueryInterface(data, &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(genericData, NS_ERROR_FAILURE);

  // set the transfer data
  rv = trans->SetTransferData(kUnicodeMime, genericData,
                              aString.Length() * 2);
  NS_ENSURE_SUCCESS(rv, rv);

  // put the transferable on the clipboard
  rv = clipboard->SetData(trans, nullptr, aClipboardID);
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsClipboardHelper::CopyString(const nsAString& aString)
{
  nsresult rv;

  // copy to the global clipboard. it's bad if this fails in any way.
  rv = CopyStringToClipboard(aString, nsIClipboard::kGlobalClipboard);
  NS_ENSURE_SUCCESS(rv, rv);

  // unix also needs us to copy to the selection clipboard. this will
  // fail in CopyStringToClipboard if we're not on a platform that
  // supports the selection clipboard. (this could have been #ifdef
  // XP_UNIX, but using the SupportsSelectionClipboard call is the
  // more correct thing to do.
  //
  // if this fails in any way other than "not being unix", we'll get
  // the assertion we need in CopyStringToClipboard, and we needn't
  // assert again here.
  CopyStringToClipboard(aString, nsIClipboard::kSelectionClipboard);

  return NS_OK;
}
