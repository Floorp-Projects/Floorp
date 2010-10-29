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
 * The Original Code is Personal Security Manager.
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Hubbie Shaw
 *   Doug Turner <dougt@netscape.com>
 *   Mitch Stoltz <mstoltz@netscape.com>
 *   Brian Ryner <bryner@brianryner.com>
 *   Kai Engert <kaie@netscape.com>
 *   Vipul Gupta <vipul.gupta@sun.com>
 *   Douglas Stebila <douglas@stebila.ca>
 *   Kai Engert <kengert@redhat.com>
 *   honzab.moz@firemni.cz
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

#include "NSSErrorsService.h"

#include "nsNSSComponent.h"
#include "nsServiceManagerUtils.h"
#include "secerr.h"
#include "sslerr.h"

#define PIPNSS_STRBUNDLE_URL "chrome://pipnss/locale/pipnss.properties"
#define NSSERR_STRBUNDLE_URL "chrome://pipnss/locale/nsserrors.properties"

namespace mozilla {
namespace psm {

NS_IMPL_ISUPPORTS1(NSSErrorsService, nsINSSErrorsService)

nsresult
NSSErrorsService::Init()
{
  nsresult rv;
  nsCOMPtr<nsIStringBundleService> bundleService(do_GetService(NS_STRINGBUNDLE_CONTRACTID, &rv));
  if (NS_FAILED(rv) || !bundleService) 
    return NS_ERROR_FAILURE;
  
  bundleService->CreateBundle(PIPNSS_STRBUNDLE_URL,
                              getter_AddRefs(mPIPNSSBundle));
  if (!mPIPNSSBundle)
    rv = NS_ERROR_FAILURE;

  bundleService->CreateBundle(NSSERR_STRBUNDLE_URL,
                              getter_AddRefs(mNSSErrorsBundle));
  if (!mNSSErrorsBundle)
    rv = NS_ERROR_FAILURE;

  return rv;
}

#define EXPECTED_SEC_ERROR_BASE (-0x2000)
#define EXPECTED_SSL_ERROR_BASE (-0x3000)

#if SEC_ERROR_BASE != EXPECTED_SEC_ERROR_BASE || SSL_ERROR_BASE != EXPECTED_SSL_ERROR_BASE
#error "Unexpected change of error code numbers in lib NSS, please adjust the mapping code"
/*
 * Please ensure the NSS error codes are mapped into the positive range 0x1000 to 0xf000
 * Search for NS_ERROR_MODULE_SECURITY to ensure there are no conflicts.
 * The current code also assumes that NSS library error codes are negative.
 */
#endif

NS_IMETHODIMP
NSSErrorsService::IsNSSErrorCode(PRInt32 aNSPRCode, PRBool *_retval)
{
  if (!_retval)
    return NS_ERROR_FAILURE;

  *_retval = IS_SEC_ERROR(aNSPRCode) || IS_SSL_ERROR(aNSPRCode);
  return NS_OK;
}

NS_IMETHODIMP
NSSErrorsService::GetXPCOMFromNSSError(PRInt32 aNSPRCode, nsresult *aXPCOMErrorCode)
{
  if (!IS_SEC_ERROR(aNSPRCode) && !IS_SSL_ERROR(aNSPRCode))
    return NS_ERROR_FAILURE;

  if (!aXPCOMErrorCode)
    return NS_ERROR_INVALID_ARG;

  // The error codes within each module may be a 16 bit value.
  // For simplicity let's use the positive value of the NSS code.

  *aXPCOMErrorCode =
    NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_SECURITY,
                              -1 * aNSPRCode);
  return NS_OK;
}

NS_IMETHODIMP
NSSErrorsService::GetErrorClass(nsresult aXPCOMErrorCode, PRUint32 *aErrorClass)
{
  NS_ENSURE_ARG(aErrorClass);

  if (NS_ERROR_GET_MODULE(aXPCOMErrorCode) != NS_ERROR_MODULE_SECURITY
      || NS_ERROR_GET_SEVERITY(aXPCOMErrorCode) != NS_ERROR_SEVERITY_ERROR)
    return NS_ERROR_FAILURE;
  
  PRInt32 aNSPRCode = -1 * NS_ERROR_GET_CODE(aXPCOMErrorCode);

  if (!IS_SEC_ERROR(aNSPRCode) && !IS_SSL_ERROR(aNSPRCode))
    return NS_ERROR_FAILURE;

  switch (aNSPRCode)
  {
    case SEC_ERROR_UNKNOWN_ISSUER:
    case SEC_ERROR_CA_CERT_INVALID:
    case SEC_ERROR_UNTRUSTED_ISSUER:
    case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
    case SEC_ERROR_UNTRUSTED_CERT:
    case SEC_ERROR_INADEQUATE_KEY_USAGE:
    case SSL_ERROR_BAD_CERT_DOMAIN:
    case SEC_ERROR_EXPIRED_CERTIFICATE:
      *aErrorClass = ERROR_CLASS_BAD_CERT;
      break;
    default:
      *aErrorClass = ERROR_CLASS_SSL_PROTOCOL;
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP
NSSErrorsService::GetErrorMessage(nsresult aXPCOMErrorCode, nsAString &aErrorMessage)
{
  if (NS_ERROR_GET_MODULE(aXPCOMErrorCode) != NS_ERROR_MODULE_SECURITY
      || NS_ERROR_GET_SEVERITY(aXPCOMErrorCode) != NS_ERROR_SEVERITY_ERROR)
    return NS_ERROR_FAILURE;
  
  PRInt32 aNSPRCode = -1 * NS_ERROR_GET_CODE(aXPCOMErrorCode);

  if (!IS_SEC_ERROR(aNSPRCode) && !IS_SSL_ERROR(aNSPRCode))
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIStringBundle> theBundle = mPIPNSSBundle;
  const char *id_str = nsNSSErrors::getOverrideErrorStringName(aNSPRCode);

  if (!id_str) {
    id_str = nsNSSErrors::getDefaultErrorStringName(aNSPRCode);
    theBundle = mNSSErrorsBundle;
  }

  if (!id_str || !theBundle)
    return NS_ERROR_FAILURE;

  nsAutoString msg;
  nsresult rv =
    theBundle->GetStringFromName(NS_ConvertASCIItoUTF16(id_str).get(),
                                 getter_Copies(msg));
  if (NS_SUCCEEDED(rv)) {
    aErrorMessage = msg;
  }
  return rv;
}

} // psm
} // mozilla
