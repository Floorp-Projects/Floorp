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
 * The Original Code is Nokia Corporation Code.
 *
 * The Initial Developer of the Original Code is Nokia Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
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
