/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 *
 */

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
  mCached_NonInadequateReason = SECSuccess;
}

void
nsUsageArrayHelper::check(char *suffix,
                        SECCertUsage aCertUsage,
                        PRUint32 &aCounter,
                        PRUnichar **outUsages)
{
  nsNSSShutDownPreventionLock locker;
  if (CERT_VerifyCertNow(defaultcertdb, mCert, PR_TRUE, 
                         aCertUsage, NULL) == SECSuccess) {
    nsAutoString typestr;
    switch (aCertUsage) {
      case certUsageSSLClient:
        typestr = NS_LITERAL_STRING("VerifySSLClient");
        break;
      case certUsageSSLServer:
        typestr = NS_LITERAL_STRING("VerifySSLServer");
        break;
      case certUsageSSLServerWithStepUp:
        typestr = NS_LITERAL_STRING("VerifySSLStepUp");
        break;
      case certUsageEmailSigner:
        typestr = NS_LITERAL_STRING("VerifyEmailSigner");
        break;
      case certUsageEmailRecipient:
        typestr = NS_LITERAL_STRING("VerifyEmailRecip");
        break;
      case certUsageObjectSigner:
        typestr = NS_LITERAL_STRING("VerifyObjSign");
        break;
      case certUsageProtectedObjectSigner:
        typestr = NS_LITERAL_STRING("VerifyProtectObjSign");
        break;
      case certUsageUserCertImport:
        typestr = NS_LITERAL_STRING("VerifyUserImport");
        break;
      case certUsageSSLCA:
        typestr = NS_LITERAL_STRING("VerifySSLCA");
        break;
      case certUsageVerifyCA:
        typestr = NS_LITERAL_STRING("VerifyCAVerifier");
        break;
      case certUsageStatusResponder:
        typestr = NS_LITERAL_STRING("VerifyStatusResponder");
        break;
      case certUsageAnyCA:
        typestr = NS_LITERAL_STRING("VerifyAnyCA");
        break;
      default:
        break;
    }
    if (!typestr.IsEmpty()) {
      typestr.AppendWithConversion(suffix);
      nsAutoString verifyDesc;
      m_rv = nssComponent->GetPIPNSSBundleString(typestr.get(), verifyDesc);
      if (NS_SUCCEEDED(m_rv)) {
        outUsages[aCounter++] = ToNewUnicode(verifyDesc);
      }
    }
  }
  else {
    int err = PR_GetError();
    
    if (SECSuccess == mCached_NonInadequateReason) {
      // we have not yet cached anything
      mCached_NonInadequateReason = err;
    }
    else {
      switch (err) {
        case SEC_ERROR_INADEQUATE_KEY_USAGE:
        case SEC_ERROR_INADEQUATE_CERT_TYPE:
          // this code should not override a possibly cached more informative reason
          break;
        
        default:
          mCached_NonInadequateReason = err;
          break;
      }
    }
  }
}

void
nsUsageArrayHelper::verifyFailed(PRUint32 *_verified)
{
  switch (mCached_NonInadequateReason) {
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
nsUsageArrayHelper::GetUsagesArray(char *suffix,
                      PRBool ignoreOcsp,
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

  if (ignoreOcsp) {
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
  
  // The following list of checks must be < max_returned_out_array_size
  
  check(suffix, certUsageSSLClient, count, outUsages);
  check(suffix, certUsageSSLServer, count, outUsages);
  check(suffix, certUsageSSLServerWithStepUp, count, outUsages);
  check(suffix, certUsageEmailSigner, count, outUsages);
  check(suffix, certUsageEmailRecipient, count, outUsages);
  check(suffix, certUsageObjectSigner, count, outUsages);
#if 0
  check(suffix, certUsageProtectedObjectSigner, count, outUsages);
  check(suffix, certUsageUserCertImport, count, outUsages);
#endif
  check(suffix, certUsageSSLCA, count, outUsages);
#if 0
  check(suffix, certUsageVerifyCA, count, outUsages);
#endif
  check(suffix, certUsageStatusResponder, count, outUsages);
#if 0
  check(suffix, certUsageAnyCA, count, outUsages);
#endif

  if (ignoreOcsp && nssComponent) {
    nssComponent->SkipOcspOff();
  }

  if (count == 0) {
    verifyFailed(_verified);
  } else {
    *_verified = nsNSSCertificate::VERIFIED_OK;
  }
  return NS_OK;
}
