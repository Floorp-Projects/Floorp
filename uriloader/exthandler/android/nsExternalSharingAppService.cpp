/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
                                            PRUint32 *aLen,
                                            nsISharingHandlerApp ***aHandlers)
{
  nsresult rv;
  NS_NAMED_LITERAL_STRING(sendAction, "android.intent.action.SEND");
  nsCOMPtr<nsIMutableArray> array = do_CreateInstance(NS_ARRAY_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  NS_ConvertUTF16toUTF8 nMimeType(aMIMEType);
  if (!AndroidBridge::Bridge())
    return NS_OK;
  AndroidBridge::Bridge()->GetHandlersForMimeType(nMimeType.get(), array,
                                                  nullptr, sendAction);
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
