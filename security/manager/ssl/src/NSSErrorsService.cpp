/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NSSErrorsService.h"

#include "nsNSSComponent.h"
#include "nsServiceManagerUtils.h"
#include "pkix/pkixnss.h"
#include "secerr.h"
#include "sslerr.h"

#define PIPNSS_STRBUNDLE_URL "chrome://pipnss/locale/pipnss.properties"
#define NSSERR_STRBUNDLE_URL "chrome://pipnss/locale/nsserrors.properties"

namespace mozilla {
namespace psm {

static_assert(mozilla::pkix::ERROR_BASE ==
                nsINSSErrorsService::MOZILLA_PKIX_ERROR_BASE,
              "MOZILLA_PKIX_ERROR_BASE and "
                "nsINSSErrorsService::MOZILLA_PKIX_ERROR_BASE do not match.");
static_assert(mozilla::pkix::ERROR_LIMIT ==
                nsINSSErrorsService::MOZILLA_PKIX_ERROR_LIMIT,
              "MOZILLA_PKIX_ERROR_LIMIT and "
                "nsINSSErrorsService::MOZILLA_PKIX_ERROR_LIMIT do not match.");

static bool
IsPSMError(PRErrorCode error)
{
  return (error >= mozilla::pkix::ERROR_BASE &&
          error < mozilla::pkix::ERROR_LIMIT);
}

NS_IMPL_ISUPPORTS(NSSErrorsService, nsINSSErrorsService)

NSSErrorsService::~NSSErrorsService() { }

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

bool
IsNSSErrorCode(PRErrorCode code)
{
  return IS_SEC_ERROR(code) || IS_SSL_ERROR(code) || IsPSMError(code);
}

nsresult
GetXPCOMFromNSSError(PRErrorCode code)
{
  if (!code) {
    MOZ_CRASH("Function failed without calling PR_GetError");
  }

  // The error codes within each module must be a 16 bit value.
  // For simplicity we use the positive value of the NSS code.
  return (nsresult)NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_SECURITY,
                                             -1 * code);
}

NS_IMETHODIMP
NSSErrorsService::IsNSSErrorCode(int32_t aNSPRCode, bool *_retval)
{
  if (!_retval) {
    return NS_ERROR_INVALID_ARG;
  }

  *_retval = mozilla::psm::IsNSSErrorCode(aNSPRCode);
  return NS_OK;
}

NS_IMETHODIMP
NSSErrorsService::GetXPCOMFromNSSError(int32_t aNSPRCode, nsresult *aXPCOMErrorCode)
{
  if (!aXPCOMErrorCode) {
    return NS_ERROR_INVALID_ARG;
  }

  if (!mozilla::psm::IsNSSErrorCode(aNSPRCode)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aXPCOMErrorCode = mozilla::psm::GetXPCOMFromNSSError(aNSPRCode);

  return NS_OK;
}

NS_IMETHODIMP
NSSErrorsService::GetErrorClass(nsresult aXPCOMErrorCode, uint32_t *aErrorClass)
{
  NS_ENSURE_ARG(aErrorClass);

  if (NS_ERROR_GET_MODULE(aXPCOMErrorCode) != NS_ERROR_MODULE_SECURITY ||
      NS_ERROR_GET_SEVERITY(aXPCOMErrorCode) != NS_ERROR_SEVERITY_ERROR) {
    return NS_ERROR_FAILURE;
  }
  
  int32_t aNSPRCode = -1 * NS_ERROR_GET_CODE(aXPCOMErrorCode);

  if (!mozilla::psm::IsNSSErrorCode(aNSPRCode)) {
    return NS_ERROR_FAILURE;
  }

  switch (aNSPRCode)
  {
    // Overridable errors.
    case SEC_ERROR_UNKNOWN_ISSUER:
    case SEC_ERROR_UNTRUSTED_ISSUER:
    case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
    case SEC_ERROR_UNTRUSTED_CERT:
    case SSL_ERROR_BAD_CERT_DOMAIN:
    case SEC_ERROR_EXPIRED_CERTIFICATE:
    case SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED:
    case SEC_ERROR_CA_CERT_INVALID:
    case mozilla::pkix::MOZILLA_PKIX_ERROR_CA_CERT_USED_AS_END_ENTITY:
    case mozilla::pkix::MOZILLA_PKIX_ERROR_INADEQUATE_KEY_SIZE:
      *aErrorClass = ERROR_CLASS_BAD_CERT;
      break;
    // Non-overridable errors.
    default:
      *aErrorClass = ERROR_CLASS_SSL_PROTOCOL;
      break;
  }
  return NS_OK;
}

NS_IMETHODIMP
NSSErrorsService::GetErrorMessage(nsresult aXPCOMErrorCode, nsAString &aErrorMessage)
{
  if (NS_ERROR_GET_MODULE(aXPCOMErrorCode) != NS_ERROR_MODULE_SECURITY ||
      NS_ERROR_GET_SEVERITY(aXPCOMErrorCode) != NS_ERROR_SEVERITY_ERROR) {
    return NS_ERROR_FAILURE;
  }
  
  int32_t aNSPRCode = -1 * NS_ERROR_GET_CODE(aXPCOMErrorCode);

  if (!mozilla::psm::IsNSSErrorCode(aNSPRCode)) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIStringBundle> theBundle = mPIPNSSBundle;
  const char *id_str = nsNSSErrors::getOverrideErrorStringName(aNSPRCode);

  if (!id_str) {
    id_str = nsNSSErrors::getDefaultErrorStringName(aNSPRCode);
    theBundle = mNSSErrorsBundle;
  }

  if (!id_str || !theBundle) {
    return NS_ERROR_FAILURE;
  }

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
