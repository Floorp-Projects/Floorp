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
 * The Original Code is Mozilla browser.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 *
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

#include "nsExternalSharingAppService.h"

#include "mozilla/ModuleUtils.h"
#include "nsIClassInfoImpl.h"

#include "AndroidBridge.h"
#include "nsArrayUtils.h"
#include "nsISupportsUtils.h"
#include "nsComponentManagerUtils.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS1(nsExternalSharingAppService, nsIExternalSharingAppService)

nsExternalSharingAppService::nsExternalSharingAppService()
{
}

nsExternalSharingAppService::~nsExternalSharingAppService()
{
}

NS_IMETHODIMP
nsExternalSharingAppService::ShareWithDefault(const nsAString & data,
                                              const nsAString & mime,
                                              const nsAString & title)
{
  NS_NAMED_LITERAL_STRING(sendAction, "android.intent.action.SEND");
  const nsString emptyString = EmptyString();
  if (AndroidBridge::Bridge())
    return AndroidBridge::Bridge()->
      OpenUriExternal(NS_ConvertUTF16toUTF8(data), NS_ConvertUTF16toUTF8(mime),
                      emptyString,emptyString, sendAction, title) ? NS_OK : NS_ERROR_FAILURE;

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsExternalSharingAppService::GetSharingApps(const nsAString & aMIMEType,
                                            PRUint32 *aLen NS_OUTPARAM,
                                            nsISharingHandlerApp ***aHandlers NS_OUTPARAM)
{
  nsresult rv;
  NS_NAMED_LITERAL_STRING(sendAction, "android.intent.action.SEND");
  nsCOMPtr<nsIMutableArray> array = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ConvertUTF16toUTF8 nMimeType(aMIMEType);
  if (!AndroidBridge::Bridge())
    return NS_OK;
  AndroidBridge::Bridge()->GetHandlersForMimeType(nMimeType.get(), array,
                                                  nsnull, sendAction);
  array->GetLength(aLen);
  *aHandlers =
    static_cast<nsISharingHandlerApp**>(NS_Alloc(sizeof(nsISharingHandlerApp*)
                                                 * *aLen));
  for (int i = 0; i < *aLen; i++) {
    rv = array->QueryElementAt(i, nsISharingHandlerApp::GetIID(),
                               (void**)(*aHandlers + i));
    NS_ENSURE_SUCCESS(rv, rv);
  }
  return NS_OK;
}
