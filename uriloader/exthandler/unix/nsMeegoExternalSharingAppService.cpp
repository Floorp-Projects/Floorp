/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "shareuiinterface.h"
#include <MDataUri>
#include "nsExternalSharingAppService.h"
#include "nsString.h"

NS_IMPL_ISUPPORTS1(nsExternalSharingAppService, nsIExternalSharingAppService)

nsExternalSharingAppService::nsExternalSharingAppService()
  : mShareUi(nsnull)
{
}

nsExternalSharingAppService::~nsExternalSharingAppService()
{
}

NS_IMETHODIMP
nsExternalSharingAppService::ShareWithDefault(const nsAString & aData,
                                              const nsAString & aMime,
                                              const nsAString & aTitle)
{
  if (!mShareUi)
    mShareUi = new ShareUiInterface();

  if (!mShareUi || !mShareUi->isValid())
    return NS_ERROR_NOT_AVAILABLE;

  if (aData.IsEmpty())
    return NS_ERROR_INVALID_ARG;

  MDataUri uri;
  uri.setTextData(QString::fromUtf16(aData.BeginReading(), aData.Length()));
  uri.setMimeType(QString::fromUtf16(aMime.BeginReading(), aMime.Length())); 
  uri.setAttribute("title", QString::fromUtf16(aTitle.BeginReading(), 0));

  mShareUi->share(QStringList(uri.toString()));

  return NS_OK;
}

NS_IMETHODIMP
nsExternalSharingAppService::GetSharingApps(const nsAString & aMIMEType,
                                            PRUint32 *aLen NS_OUTPARAM,
                                            nsISharingHandlerApp ***aHandlers NS_OUTPARAM)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}
