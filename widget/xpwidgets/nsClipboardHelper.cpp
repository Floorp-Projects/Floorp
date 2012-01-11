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
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
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

#include "nsClipboardHelper.h"

// basics
#include "nsCOMPtr.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsIServiceManager.h"

// helpers
#include "nsIClipboard.h"
#include "nsITransferable.h"
#include "nsReadableUtils.h"

NS_IMPL_ISUPPORTS1(nsClipboardHelper, nsIClipboardHelper)

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
                                         PRInt32 aClipboardID)
{
  nsresult rv;

  // get the clipboard
  nsCOMPtr<nsIClipboard>
    clipboard(do_GetService("@mozilla.org/widget/clipboard;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(clipboard, NS_ERROR_FAILURE);

  // don't go any further if they're asking for the selection
  // clipboard on a platform which doesn't support it (i.e., unix)
  if (nsIClipboard::kSelectionClipboard == aClipboardID) {
    bool clipboardSupported;
    rv = clipboard->SupportsSelectionClipboard(&clipboardSupported);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!clipboardSupported)
      return NS_ERROR_FAILURE;
  }

  // create a transferable for putting data on the clipboard
  nsCOMPtr<nsITransferable>
    trans(do_CreateInstance("@mozilla.org/widget/transferable;1", &rv));
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ENSURE_TRUE(trans, NS_ERROR_FAILURE);

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
  rv = clipboard->SetData(trans, nsnull, aClipboardID);
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
