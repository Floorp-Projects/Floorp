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
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  John Gardiner Myers <jgmyers@speakeasy.net>
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

#include "nsUsageArrayHelper.h"

#include "nsCOMPtr.h"
#include "nsIDateTimeFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsComponentManagerUtils.h"
#include "nsReadableUtils.h"
#include "nsNSSCertificate.h"

#include "nspr.h"
#include "nsNSSCertHeader.h"

extern "C" {
#include "secerr.h"
}

static NS_DEFINE_CID(kNSSComponentCID, NS_NSSCOMPONENT_CID);

nsUsageArrayHelper::nsUsageArrayHelper(CERTCertificate *aCert)
:mCert(aCert)
{
  nsNSSShutDownPreventionLock locker;
  defaultcertdb = CERT_GetDefaultCertDB();
  nssComponent = do_GetService(kNSSComponentCID, &m_rv);
}

void
nsUsageArrayHelper::check(const char *suffix,
                        SECCertificateUsage aCertUsage,
                        PRUint32 &aCounter,
                        PRUnichar **outUsages)
{
  if (!aCertUsage) return;
  nsCAutoString typestr;
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

void
nsUsageArrayHelper::verifyFailed(PRUint32 *_verified, int err)
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
  case SEC_ERROR_CERT_USAGES_INVALID: // XXX what is this?
  // there are some OCSP errors from PSM 1.x to add here
  case SECSuccess:
    // this means, no verification result has ever been received
  default:
    *_verified = nsNSSCertificate::NOT_VERIFIED_UNKNOWN; break;
  }
}

nsresult
nsUsageArrayHelper::GetUsagesArray(const char *suffix,
                      bool localOnly,
                      PRUint32 outArraySize,
                      PRUint32 *_verified,
                      PRUint32 *_count,
                      PRUnichar **outUsages)
{
  nsNSSShutDownPreventionLock locker;
  if (NS_FAILED(m_rv))
    return m_rv;

  if (outArraySize < max_returned_out_array_size)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsINSSComponent> nssComponent;

  if (!nsNSSComponent::globalConstFlagUsePKIXVerification && localOnly) {
    nsresult rv;
    nssComponent = do_GetService(kNSSComponentCID, &rv);
    if (NS_FAILED(rv))
      return rv;
    
    if (nssComponent) {
      nssComponent->SkipOcsp();
    }
  }
  
  PRUint32 &count = *_count;
  count = 0;
  SECCertificateUsage usages = 0;
  int err = 0;
  
if (!nsNSSComponent::globalConstFlagUsePKIXVerification) {
  // CERT_VerifyCertificateNow returns SECFailure unless the certificate is
  // valid for all the given usages. Hoewver, we are only looking for the list
  // of usages for which the cert *is* valid.
  (void)
  CERT_VerifyCertificateNow(defaultcertdb, mCert, PR_TRUE,
			    certificateUsageSSLClient |
			    certificateUsageSSLServer |
			    certificateUsageSSLServerWithStepUp |
			    certificateUsageEmailSigner |
			    certificateUsageEmailRecipient |
			    certificateUsageObjectSigner |
			    certificateUsageSSLCA |
			    certificateUsageStatusResponder,
			    NULL, &usages);
  err = PR_GetError();
}
else {
  nsresult nsrv;
  nsCOMPtr<nsINSSComponent> inss = do_GetService(kNSSComponentCID, &nsrv);
  if (!inss)
    return nsrv;
  nsRefPtr<nsCERTValInParamWrapper> survivingParams;
  if (localOnly)
    nsrv = inss->GetDefaultCERTValInParamLocalOnly(survivingParams);
  else
    nsrv = inss->GetDefaultCERTValInParam(survivingParams);
  
  if (NS_FAILED(nsrv))
    return nsrv;

  CERTValOutParam cvout[2];
  cvout[0].type = cert_po_usages;
  cvout[0].value.scalar.usages = 0;
  cvout[1].type = cert_po_end;
  
  CERT_PKIXVerifyCert(mCert, certificateUsageCheckAllUsages,
                      survivingParams->GetRawPointerForNSS(),
                      cvout, NULL);
  err = PR_GetError();
  usages = cvout[0].value.scalar.usages;
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

  if (!nsNSSComponent::globalConstFlagUsePKIXVerification && localOnly && nssComponent) {
    nssComponent->SkipOcspOff();
  }

  if (count == 0) {
    verifyFailed(_verified, err);
  } else {
    *_verified = nsNSSCertificate::VERIFIED_OK;
  }
  return NS_OK;
}
