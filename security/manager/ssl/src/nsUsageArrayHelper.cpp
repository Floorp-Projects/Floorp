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
#include "nsServiceManagerUtils.h"

#include "nspr.h"
#include "secerr.h"

using namespace mozilla;
using namespace mozilla::psm;

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
                          CertVerifier * certVerifier,
                          SECCertificateUsage aCertUsage,
                          PRTime time,
                          CertVerifier::Flags flags,
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

  SECStatus rv = certVerifier->VerifyCert(mCert, aCertUsage,
                         time, nullptr /*XXX:wincx*/, flags);

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

  // Bug 860076, this disabling ocsp for all NSS is incorrect.
#ifndef NSS_NO_LIBPKIX
  const bool localOSCPDisable = !nsNSSComponent::globalConstFlagUsePKIXVerification && localOnly;
#else
  const bool localOSCPDisable = localOnly;
#endif 
  if (localOSCPDisable) {
    nsresult rv;
    nssComponent = do_GetService(kNSSComponentCID, &rv);
    if (NS_FAILED(rv))
      return rv;
    
    if (nssComponent) {
      nssComponent->SkipOcsp();
    }
  }

  uint32_t &count = *_count;
  count = 0;

  RefPtr<CertVerifier> certVerifier(GetDefaultCertVerifier());
  NS_ENSURE_TRUE(certVerifier, NS_ERROR_UNEXPECTED);

  PRTime now = PR_Now();
  CertVerifier::Flags flags = localOnly ? CertVerifier::FLAG_LOCAL_ONLY : 0;

  // The following list of checks must be < max_returned_out_array_size

  uint32_t result;
  result = check(nsIX509Cert::VERIFIED_OK, suffix, certVerifier,
                 certificateUsageSSLClient, now, flags, count, outUsages);
  result = check(result, suffix, certVerifier,
                 certificateUsageSSLServer, now, flags, count, outUsages);
  result = check(result, suffix, certVerifier,
                 certificateUsageEmailSigner, now, flags, count, outUsages);
  result = check(result, suffix, certVerifier,
                 certificateUsageEmailRecipient, now, flags, count, outUsages);
  result = check(result, suffix, certVerifier,
                 certificateUsageObjectSigner, now, flags, count, outUsages);
#if 0
  result = check(result, suffix, certVerifier,
                 certificateUsageProtectedObjectSigner, now, flags, count,
                 outUsages);
  result = check(result, suffix, certVerifier,
                 certificateUsageUserCertImport, now, flags, count, outUsages);
#endif
  result = check(result, suffix, certVerifier,
                 certificateUsageSSLCA, now, flags, count, outUsages);
#if 0
  result = check(result, suffix, certVerifier,
                 certificateUsageVerifyCA, now, flags, count, outUsages);
#endif
  result = check(result, suffix, certVerifier,
                 certificateUsageStatusResponder, now, flags, count, outUsages);
#if 0
  result = check(result, suffix, certVerifier,
                 certificateUsageAnyCA, now, flags, count, outUsages);
#endif

  // Bug 860076, this disabling ocsp for all NSS is incorrect
  if (localOSCPDisable) {
     nssComponent->SkipOcspOff();
  }

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
