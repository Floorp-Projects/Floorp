/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsUsageArrayHelper.h"

#include "mozilla/Assertions.h"
#include "nsCOMPtr.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsComponentManagerUtils.h"
#include "nsReadableUtils.h"
#include "nsNSSCertificate.h"

#include "nspr.h"
#include "secerr.h"

using namespace mozilla;

#ifdef PR_LOGGING
extern PRLogModuleInfo* gPIPNSSLog;
#endif

static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID); // XXX? needed?::

nsUsageArrayHelper::nsUsageArrayHelper(CERTCertificate *aCert)
:mCert(aCert)
{
  nsNSSShutDownPreventionLock locker;
  defaultcertdb = CERT_GetDefaultCertDB();
  nssComponent = do_GetService(kNSSComponentCID, &m_rv);
}

// XXX: old, non-libpkix version of check that will be removed after the switch
// to libpkix is final.
void
nsUsageArrayHelper::check(const char *suffix,
                        SECCertificateUsage aCertUsage,
                        uint32_t &aCounter,
                        PRUnichar **outUsages)
{
  if (!aCertUsage) return;
  nsAutoCString typestr;
  switch (aCertUsage) {
  case certificateUsageSSLClient:
    typestr = "VerifySSLClient";
    break;
  case certificateUsageSSLServer:
    typestr = "VerifySSLServer";
    break;
  case certificateUsageSSLServerWithStepUp:
    typestr = "VerifySSLStepUp";
    break;
  case certificateUsageEmailSigner:
    typestr = "VerifyEmailSigner";
    break;
  case certificateUsageEmailRecipient:
    typestr = "VerifyEmailRecip";
    break;
  case certificateUsageObjectSigner:
    typestr = "VerifyObjSign";
    break;
  case certificateUsageProtectedObjectSigner:
    typestr = "VerifyProtectObjSign";
    break;
  case certificateUsageUserCertImport:
    typestr = "VerifyUserImport";
    break;
  case certificateUsageSSLCA:
    typestr = "VerifySSLCA";
    break;
  case certificateUsageVerifyCA:
    typestr = "VerifyCAVerifier";
    break;
  case certificateUsageStatusResponder:
    typestr = "VerifyStatusResponder";
    break;
  case certificateUsageAnyCA:
    typestr = "VerifyAnyCA";
    break;
  default:
    break;
  }
  if (!typestr.IsEmpty()) {
    typestr.Append(suffix);
    nsAutoString verifyDesc;
    m_rv = nssComponent->GetPIPNSSBundleString(typestr.get(), verifyDesc);
    if (NS_SUCCEEDED(m_rv)) {
      outUsages[aCounter++] = ToNewUnicode(verifyDesc);
    }
  }
}

namespace {

// Some validation errors are non-fatal in that, we should keep checking the
// cert for other usages after receiving them; i.e. they are errors that NSS
// returns when a certificate isn't valid for a particular usage, but which
// don't indicate that the certificate is invalid for ANY usage. Others errors
// (e.g. revocation) are fatal, and we should immediately stop validation of
// the cert when we encounter them.
bool
isFatalError(uint32_t checkResult)
{
  return checkResult != nsIX509Cert::VERIFIED_OK &&
         checkResult != nsIX509Cert::USAGE_NOT_ALLOWED &&
         checkResult != nsIX509Cert::ISSUER_NOT_TRUSTED &&
         checkResult != nsIX509Cert::ISSUER_UNKNOWN;
}

} // unnamed namespace

// Validates the certificate for the given usage. If the certificate is valid
// for the given usage, aCounter is incremented, a string description of the
// usage is appended to outUsages, and nsNSSCertificate::VERIFIED_OK is
// returned. Otherwise, if validation failed, one of the other "Constants for
// certificate verification results" in nsIX509Cert is returned.
uint32_t
nsUsageArrayHelper::check(uint32_t previousCheckResult,
                          const char *suffix,
                          SECCertificateUsage aCertUsage,
                          nsCERTValInParamWrapper * aValInParams,
                          uint32_t &aCounter,
                          PRUnichar **outUsages)
{
  if (!aCertUsage) {
    MOZ_NOT_REACHED("caller should have supplied non-zero aCertUsage");
    return nsIX509Cert::NOT_VERIFIED_UNKNOWN;
  }

  if (isFatalError(previousCheckResult)) {
      return previousCheckResult;
  }

  nsAutoCString typestr;
  switch (aCertUsage) {
  case certificateUsageSSLClient:
    typestr = "VerifySSLClient";
    break;
  case certificateUsageSSLServer:
    typestr = "VerifySSLServer";
    break;
  case certificateUsageSSLServerWithStepUp:
    typestr = "VerifySSLStepUp";
    break;
  case certificateUsageEmailSigner:
    typestr = "VerifyEmailSigner";
    break;
  case certificateUsageEmailRecipient:
    typestr = "VerifyEmailRecip";
    break;
  case certificateUsageObjectSigner:
    typestr = "VerifyObjSign";
    break;
  case certificateUsageProtectedObjectSigner:
    typestr = "VerifyProtectObjSign";
    break;
  case certificateUsageUserCertImport:
    typestr = "VerifyUserImport";
    break;
  case certificateUsageSSLCA:
    typestr = "VerifySSLCA";
    break;
  case certificateUsageVerifyCA:
    typestr = "VerifyCAVerifier";
    break;
  case certificateUsageStatusResponder:
    typestr = "VerifyStatusResponder";
    break;
  case certificateUsageAnyCA:
    typestr = "VerifyAnyCA";
    break;
  default:
    MOZ_NOT_REACHED("unknown cert usage passed to check()");
    return nsIX509Cert::NOT_VERIFIED_UNKNOWN;
  }

  SECStatus rv;
  CERTValOutParam cvout[1];
  cvout[0].type = cert_po_end;

  rv = CERT_PKIXVerifyCert(mCert, aCertUsage,
                           aValInParams->GetRawPointerForNSS(),
                           cvout, nullptr);

  if (rv == SECSuccess) {
    typestr.Append(suffix);
    nsAutoString verifyDesc;
    m_rv = nssComponent->GetPIPNSSBundleString(typestr.get(), verifyDesc);
    if (NS_SUCCEEDED(m_rv)) {
      outUsages[aCounter++] = ToNewUnicode(verifyDesc);
    }
    return nsIX509Cert::VERIFIED_OK;
  }

  PRErrorCode error = PR_GetError();

  const char * errorString = PR_ErrorToName(error);
  uint32_t result = nsIX509Cert::NOT_VERIFIED_UNKNOWN;
  verifyFailed(&result, error);

  // USAGE_NOT_ALLOWED is the weakest non-fatal error; let all other errors
  // override it.
  if (result == nsIX509Cert::USAGE_NOT_ALLOWED &&
      previousCheckResult != nsIX509Cert::VERIFIED_OK) {
      result = previousCheckResult;
  }

  PR_LOG(gPIPNSSLog, PR_LOG_DEBUG,
          ("error validating certificate for usage %s: %s (%d) -> %ud \n",
          typestr.get(), errorString, (int) error, (int) result));

  return result;
}

// Maps the error code to one of the Constants for certificate verification
// results" in nsIX509Cert.
void
nsUsageArrayHelper::verifyFailed(uint32_t *_verified, int err)
{
  switch (err) {
  /* For these cases, verify only failed for the particular usage */
  case SEC_ERROR_INADEQUATE_KEY_USAGE:
  case SEC_ERROR_INADEQUATE_CERT_TYPE:
    *_verified = nsNSSCertificate::USAGE_NOT_ALLOWED; break;
  /* These are the cases that have individual error messages */
  case SEC_ERROR_REVOKED_CERTIFICATE:
    *_verified = nsNSSCertificate::CERT_REVOKED; break;
  case SEC_ERROR_EXPIRED_CERTIFICATE:
    *_verified = nsNSSCertificate::CERT_EXPIRED; break;
  case SEC_ERROR_UNTRUSTED_CERT:
    *_verified = nsNSSCertificate::CERT_NOT_TRUSTED; break;
  case SEC_ERROR_UNTRUSTED_ISSUER:
    *_verified = nsNSSCertificate::ISSUER_NOT_TRUSTED; break;
  case SEC_ERROR_UNKNOWN_ISSUER:
    *_verified = nsNSSCertificate::ISSUER_UNKNOWN; break;
  case SEC_ERROR_EXPIRED_ISSUER_CERTIFICATE:
    // XXX are there other error for this?
    *_verified = nsNSSCertificate::INVALID_CA; break;
  case SEC_ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED:
    *_verified = nsNSSCertificate::SIGNATURE_ALGORITHM_DISABLED; break;
  default:
    *_verified = nsNSSCertificate::NOT_VERIFIED_UNKNOWN; break;
  }
}

nsresult
nsUsageArrayHelper::GetUsagesArray(const char *suffix,
                      bool localOnly,
                      uint32_t outArraySize,
                      uint32_t *_verified,
                      uint32_t *_count,
                      PRUnichar **outUsages)
{
  nsNSSShutDownPreventionLock locker;
  if (NS_FAILED(m_rv))
    return m_rv;

  NS_ENSURE_TRUE(nssComponent, NS_ERROR_NOT_AVAILABLE);

  if (outArraySize < max_returned_out_array_size)
    return NS_ERROR_FAILURE;

  uint32_t &count = *_count;
  count = 0;

// TODO: This block will be removed as soon as the switch to libpkix is
// complete.
if (!nsNSSComponent::globalConstFlagUsePKIXVerification) {
  if (localOnly) {
    nssComponent->SkipOcsp();
  }

  SECCertificateUsage usages = 0;
  int err = 0;
  
  // CERT_VerifyCertificateNow returns SECFailure unless the certificate is
  // valid for all the given usages. Hoewver, we are only looking for the list
  // of usages for which the cert *is* valid.
  (void)
  CERT_VerifyCertificateNow(defaultcertdb, mCert, true,
			    certificateUsageSSLClient |
			    certificateUsageSSLServer |
			    certificateUsageSSLServerWithStepUp |
			    certificateUsageEmailSigner |
			    certificateUsageEmailRecipient |
			    certificateUsageObjectSigner |
			    certificateUsageSSLCA |
			    certificateUsageStatusResponder,
			    nullptr, &usages);
  err = PR_GetError();

  if (localOnly) {
     nssComponent->SkipOcspOff();
  }

  // The following list of checks must be < max_returned_out_array_size
  
  check(suffix, usages & certificateUsageSSLClient, count, outUsages);
  check(suffix, usages & certificateUsageSSLServer, count, outUsages);
  check(suffix, usages & certificateUsageSSLServerWithStepUp, count, outUsages);
  check(suffix, usages & certificateUsageEmailSigner, count, outUsages);
  check(suffix, usages & certificateUsageEmailRecipient, count, outUsages);
  check(suffix, usages & certificateUsageObjectSigner, count, outUsages);
#if 0
  check(suffix, usages & certificateUsageProtectedObjectSigner, count, outUsages);
  check(suffix, usages & certificateUsageUserCertImport, count, outUsages);
#endif
  check(suffix, usages & certificateUsageSSLCA, count, outUsages);
#if 0
  check(suffix, usages & certificateUsageVerifyCA, count, outUsages);
#endif
  check(suffix, usages & certificateUsageStatusResponder, count, outUsages);
#if 0
  check(suffix, usages & certificateUsageAnyCA, count, outUsages);
#endif

  if (count == 0) {
    verifyFailed(_verified, err);
  } else {
    *_verified = nsNSSCertificate::VERIFIED_OK;
  }
  return NS_OK;
}

  RefPtr<nsCERTValInParamWrapper> params;
  nsresult rv = localOnly ? nssComponent->GetDefaultCERTValInParamLocalOnly(params)
                          : nssComponent->GetDefaultCERTValInParam(params);
  NS_ENSURE_SUCCESS(rv, rv);

  // The following list of checks must be < max_returned_out_array_size

  uint32_t result;
  result = check(nsIX509Cert::VERIFIED_OK, suffix, certificateUsageSSLClient,
                 params, count, outUsages);
  result = check(result, suffix, certificateUsageSSLServer,
                 params, count, outUsages);
  result = check(result, suffix, certificateUsageSSLServerWithStepUp,
                 params, count, outUsages);
  result = check(result, suffix, certificateUsageEmailSigner,
                 params, count, outUsages);
  result = check(result, suffix, certificateUsageEmailRecipient,
                 params, count, outUsages);
  result = check(result, suffix, certificateUsageObjectSigner,
                 params, count, outUsages);
#if 0
  result = check(result, suffix, certificateUsageProtectedObjectSigner,
                 params, count, outUsages);
  result = check(result, suffix, certificateUsageUserCertImport,
                 params, count, outUsages);
#endif
  result = check(result, suffix, certificateUsageSSLCA,
                 params, count, outUsages);
#if 0
  result = check(result, suffix, certificateUsageVerifyCA,
                 params, count, outUsages);
#endif
  result = check(result, suffix, certificateUsageStatusResponder,
                 params, count, outUsages);
#if 0
  result = checkPKIX(result, check(suffix, certificateUsageAnyCA,
                 params, count, outUsages);
#endif

  if (isFatalError(result) || count == 0) {
    MOZ_ASSERT(result != nsIX509Cert::VERIFIED_OK);

    // Clear the output usage strings in the case where we encountered a fatal
    // error after we already successfully validated the cert for some usages.
    for (uint32_t i = 0; i < count; ++i) {
      delete outUsages[i];
      outUsages[i] = nullptr;
    }
    count = 0;
    *_verified = result;
  } else {
    *_verified = nsNSSCertificate::VERIFIED_OK;
  }
  return NS_OK;
}
