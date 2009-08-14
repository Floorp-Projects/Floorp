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
 *   Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Vladimir Vukicevic <vladimir@pobox.com>
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

#include "nsClipboardCE.h"

#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"
#include "nsCRT.h"
#include "nsComponentManagerUtils.h"

#include <winuserm.h>

nsClipboard::nsClipboard()
{
}

nsClipboard::~nsClipboard()
{
}

UINT nsClipboard::GetFormat(const char* aMimeStr)
{
  UINT format;

  if (strcmp(aMimeStr, kTextMime) == 0)
    format = CF_TEXT;
  else if (strcmp(aMimeStr, kUnicodeMime) == 0)
    format = CF_UNICODETEXT;
  else if (strcmp(aMimeStr, kJPEGImageMime) == 0 ||
           strcmp(aMimeStr, kPNGImageMime) == 0)
    format = CF_DIB;
  else if (strcmp(aMimeStr, kFileMime) == 0 || 
           strcmp(aMimeStr, kFilePromiseMime) == 0)
    format = CF_HDROP; // XXX CF_HDROP not listed as supported but it compiles!
  else
    format = ::RegisterClipboardFormat(NS_ConvertASCIItoUTF16(aMimeStr).get());

  // CE doesn't support CF_HTML (kNativeHTMLMime)

  return format;
}

NS_IMETHODIMP
nsClipboard::SetNativeClipboardData(PRInt32 aWhichClipboard)
{
  if (aWhichClipboard != kGlobalClipboard || !mTransferable)
    return NS_ERROR_INVALID_ARG;

  if (!::OpenClipboard(NULL))
    return NS_ERROR_FAILURE;

  if (!::EmptyClipboard())
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISupportsArray> flavorList;
  mTransferable->FlavorsTransferableCanExport(getter_AddRefs(flavorList));

  PRUint32 count, i;
  flavorList->Count(&count);
  nsresult rv = NS_OK;

  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> listItem;
    flavorList->GetElementAt(i, getter_AddRefs(listItem));

    nsCOMPtr<nsISupportsCString> flavor(do_QueryInterface(listItem));
    if (!flavor)
      continue;
    nsXPIDLCString flavorStr;
    flavor->ToString(getter_Copies(flavorStr));

    UINT format = GetFormat(flavorStr);

    PRUint32 len;
    nsCOMPtr<nsISupports> wrapper;
    mTransferable->GetTransferData(flavorStr, getter_AddRefs(wrapper), &len);
    if (!wrapper)
      continue;

    char *memory = nsnull;
    nsCOMPtr<nsISupportsString> textItem(do_QueryInterface(wrapper));
    nsCOMPtr<nsISupportsPRBool> boolItem(do_QueryInterface(wrapper));

    if (format == CF_TEXT || format == CF_DIB || format == CF_HDROP) {
      NS_WARNING("Setting this clipboard format not implemented");
      continue;
    } else if (textItem) {
      // format == CF_UNICODETEXT, or is otherwise unicode data.
      nsAutoString text;
      textItem->GetData(text);
      PRInt32 len = text.Length() * 2;

      memory = reinterpret_cast<char*>(::LocalAlloc(LMEM_FIXED, len + 2));
      if (!memory) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        break;
      }
      memcpy(memory, nsPromiseFlatString(text).get(), len);
      memory[len]   = '\0';
      memory[len+1] = '\0';
    } else if (boolItem) {
      // Private browsing sets a boolean type.
      PRBool value;
      boolItem->GetData(&value);
      memory = reinterpret_cast<char*>(::LocalAlloc(LMEM_FIXED, 1));
      if (!memory) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        break;
      }
      *memory = value ? 1 : 0;
    } else {
      NS_WARNING("Can't set unknown transferrable primitive");
      continue;
    }

    if (!::SetClipboardData(format, memory)) {
      NS_WARNING("::SetClipboardData failed");
      if (memory)
        ::LocalFree(memory);
    }
  }

  ::CloseClipboard();

  return rv;
}

NS_IMETHODIMP
nsClipboard::GetNativeClipboardData(nsITransferable *aTransferable,
				    PRInt32 aWhichClipboard)
{
  if (aWhichClipboard != kGlobalClipboard || !aTransferable)
    return NS_ERROR_INVALID_ARG;

  if (!::OpenClipboard(NULL))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsISupportsArray> flavorList;
  aTransferable->FlavorsTransferableCanImport(getter_AddRefs(flavorList));

  PRUint32 count, i;
  flavorList->Count(&count);
  nsresult rv = NS_OK;

  for (i = 0; i < count; i++) {
    nsCOMPtr<nsISupports> listItem;
    flavorList->GetElementAt(i, getter_AddRefs(listItem));

    nsCOMPtr<nsISupportsCString> flavor(do_QueryInterface(listItem));
    if (!flavor)
      continue;
    nsXPIDLCString flavorStr;
    flavor->ToString(getter_Copies(flavorStr));

    UINT format = GetFormat(flavorStr);

    void *data;
    data = ::GetClipboardData(format);
    if (!data)
      continue;

    if (format == CF_UNICODETEXT) {
      PRUnichar *dataStr = reinterpret_cast<PRUnichar*>(data);
      nsString *stringCopy = new nsString(dataStr);

      nsCOMPtr<nsISupportsString> primitive =
        do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID);
      if (!primitive) {
        rv = NS_ERROR_OUT_OF_MEMORY;
        break;
      }

      primitive->SetData(*stringCopy);
      aTransferable->SetTransferData(flavorStr, primitive,
                                     stringCopy->Length() * sizeof(PRUnichar));
    } else {
      NS_WARNING("Getting this clipboard format not implemented");
      continue;
    }

  }

  ::CloseClipboard();

  return rv;
}

NS_IMETHODIMP nsClipboard::HasDataMatchingFlavors(const char** aFlavorList,
                                                  PRUint32 aLength,
                                                  PRInt32 aWhichClipboard,
                                                  PRBool *_retval)
{
  *_retval = PR_FALSE;
  if (aWhichClipboard != kGlobalClipboard || !aFlavorList)
    return NS_OK;
  for (PRUint32 i = 0;i < aLength; ++i) {

    UINT format = GetFormat(aFlavorList[i]);
    if (::IsClipboardFormatAvailable(format)) {
      *_retval = PR_TRUE;
      break;
    }
  }

  return NS_OK;
}
