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
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brad Lassey <blassey@mozilla.com>
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

#include "mozilla/dom/ContentChild.h"
#include "nsClipboard.h"
#include "nsISupportsPrimitives.h"
#include "AndroidBridge.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsXULAppAPI.h"

using namespace mozilla;
using mozilla::dom::ContentChild;

NS_IMPL_ISUPPORTS1(nsClipboard, nsIClipboard)

/* The Android clipboard only supports text and doesn't support mime types
 * so we assume all clipboard data is text/unicode for now. Documentation
 * indicates that support for other data types is planned for future
 * releases.
 */

nsClipboard::nsClipboard()
{
}

NS_IMETHODIMP
nsClipboard::SetData(nsITransferable *aTransferable,
                     nsIClipboardOwner *anOwner, PRInt32 aWhichClipboard)
{
  if (aWhichClipboard != kGlobalClipboard)
    return NS_ERROR_NOT_IMPLEMENTED;

  nsCOMPtr<nsISupports> tmp;
  PRUint32 len;
  nsresult rv  = aTransferable->GetTransferData(kUnicodeMime, getter_AddRefs(tmp),
                                                &len);
  NS_ENSURE_SUCCESS(rv, rv);
  nsCOMPtr<nsISupportsString> supportsString = do_QueryInterface(tmp);
  // No support for non-text data
  NS_ENSURE_TRUE(supportsString, NS_ERROR_NOT_IMPLEMENTED);
  nsAutoString buffer;
  supportsString->GetData(buffer);

  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    if (AndroidBridge::Bridge())
      AndroidBridge::Bridge()->SetClipboardText(buffer);
    else
      return NS_ERROR_NOT_IMPLEMENTED;

  } else {
    ContentChild::GetSingleton()->SendSetClipboardText(buffer, aWhichClipboard);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::GetData(nsITransferable *aTransferable, PRInt32 aWhichClipboard)
{
  if (aWhichClipboard != kGlobalClipboard)
    return NS_ERROR_NOT_IMPLEMENTED;

  nsAutoString buffer;
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    if (!AndroidBridge::Bridge())
      return NS_ERROR_NOT_IMPLEMENTED;
    if (!AndroidBridge::Bridge()->GetClipboardText(buffer))
      return NS_ERROR_UNEXPECTED;
  } else {
    ContentChild::GetSingleton()->SendGetClipboardText(aWhichClipboard, &buffer);
  }

  nsresult rv;
  nsCOMPtr<nsISupportsString> dataWrapper =
    do_CreateInstance(NS_SUPPORTS_STRING_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = dataWrapper->SetData(buffer);
  NS_ENSURE_SUCCESS(rv, rv);

  // If our data flavor has already been added, this will fail. But we don't care
  aTransferable->AddDataFlavor(kUnicodeMime);

  nsCOMPtr<nsISupports> nsisupportsDataWrapper =
    do_QueryInterface(dataWrapper);
  rv = aTransferable->SetTransferData(kUnicodeMime, nsisupportsDataWrapper,
                                      buffer.Length() * sizeof(PRUnichar));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::EmptyClipboard(PRInt32 aWhichClipboard)
{
  if (aWhichClipboard != kGlobalClipboard)
    return NS_ERROR_NOT_IMPLEMENTED;
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    if (AndroidBridge::Bridge())
      AndroidBridge::Bridge()->EmptyClipboard();
  } else {
    ContentChild::GetSingleton()->SendEmptyClipboard();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::HasDataMatchingFlavors(const char **aFlavorList,
                                    PRUint32 aLength, PRInt32 aWhichClipboard,
                                    bool *aHasText NS_OUTPARAM)
{
  *aHasText = PR_FALSE;
  if (aWhichClipboard != kGlobalClipboard)
    return NS_ERROR_NOT_IMPLEMENTED;
  if (XRE_GetProcessType() == GeckoProcessType_Default) {
    if (AndroidBridge::Bridge())
      *aHasText = AndroidBridge::Bridge()->ClipboardHasText();
  } else {
    ContentChild::GetSingleton()->SendClipboardHasText(aHasText);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsClipboard::SupportsSelectionClipboard(bool *aIsSupported NS_OUTPARAM)
{
  *aIsSupported = PR_FALSE;
  return NS_OK;
}

