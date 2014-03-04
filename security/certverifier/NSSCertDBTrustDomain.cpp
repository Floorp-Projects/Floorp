/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NSSCertDBTrustDomain.h"

#include <stdint.h>

#include "ExtendedValidation.h"
#include "insanity/pkix.h"
#include "certdb.h"
#include "nss.h"
#include "ocsp.h"
#include "pk11pub.h"
#include "prerror.h"
#include "prmem.h"
#include "prprf.h"
#include "secerr.h"
#include "secmod.h"

using namespace insanity::pkix;

#ifdef PR_LOGGING
extern PRLogModuleInfo* gCertVerifierLog;
#endif

namespace mozilla { namespace psm {

const char BUILTIN_ROOTS_MODULE_DEFAULT_NAME[] = "Builtin Roots Module";

namespace {

inline void PORT_Free_string(char* str) { PORT_Free(str); }

typedef ScopedPtr<SECMODModule, SECMOD_DestroyModule> ScopedSECMODModule;

} // unnamed namespace

NSSCertDBTrustDomain::NSSCertDBTrustDomain(SECTrustType certDBTrustType,
                                           OCSPFetching ocspFetching,
                                           void* pinArg)
  : mCertDBTrustType(certDBTrustType)
  , mOCSPFetching(ocspFetching)
  , mPinArg(pinArg)
{
}

SECStatus
NSSCertDBTrustDomain::FindPotentialIssuers(
  const SECItem* encodedIssuerName, PRTime time,
  /*out*/ insanity::pkix::ScopedCERTCertList& results)
{
  // TODO: normalize encodedIssuerName
  // TODO: NSS seems to be ambiguous between "no potential issuers found" and
  // "there was an error trying to retrieve the potential issuers."
  results = CERT_CreateSubjectCertList(nullptr, CERT_GetDefaultCertDB(),
                                       encodedIssuerName, time, true);
  return SECSuccess;
}

SECStatus
NSSCertDBTrustDomain::GetCertTrust(EndEntityOrCA endEntityOrCA,
                                   SECOidTag policy,
                                   const CERTCertificate* candidateCert,
                                   /*out*/ TrustLevel* trustLevel)
{
  PR_ASSERT(candidateCert);
  PR_ASSERT(trustLevel);

  if (!candidateCert || !trustLevel) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }

#ifdef MOZ_NO_EV_CERTS
  if (policy != SEC_OID_X509_ANY_POLICY) {
    PR_SetError(SEC_ERROR_POLICY_VALIDATION_FAILED, 0);
    return SECFailure;
  }
#endif

  // XXX: CERT_GetCertTrust seems to be abusing SECStatus as a boolean, where
  // SECSuccess means that there is a trust record and SECFailure means there
  // is not a trust record. I looked at NSS's internal uses of
  // CERT_GetCertTrust, and all that code uses the result as a boolean meaning
  // "We have a trust record."
  CERTCertTrust trust;
  if (CERT_GetCertTrust(candidateCert, &trust) == SECSuccess) {
    PRUint32 flags = SEC_GET_TRUST_FLAGS(&trust, mCertDBTrustType);

    // For DISTRUST, we use the CERTDB_TRUSTED or CERTDB_TRUSTED_CA bit,
    // because we can have active distrust for either type of cert. Note that
    // CERTDB_TERMINAL_RECORD means "stop trying to inherit trust" so if the
    // relevant trust bit isn't set then that means the cert must be considered
    // distrusted.
    PRUint32 relevantTrustBit = endEntityOrCA == MustBeCA ? CERTDB_TRUSTED_CA
                                                          : CERTDB_TRUSTED;
    if (((flags & (relevantTrustBit|CERTDB_TERMINAL_RECORD)))
            == CERTDB_TERMINAL_RECORD) {
      *trustLevel = ActivelyDistrusted;
      return SECSuccess;
    }

    // For TRUST, we only use the CERTDB_TRUSTED_CA bit, because Gecko hasn't
    // needed to consider end-entity certs to be their own trust anchors since
    // Gecko implemented nsICertOverrideService.
    if (flags & CERTDB_TRUSTED_CA) {
      if (policy == SEC_OID_X509_ANY_POLICY) {
        *trustLevel = TrustAnchor;
        return SECSuccess;
      }
#ifndef MOZ_NO_EV_CERTS
      if (CertIsAuthoritativeForEVPolicy(candidateCert, policy)) {
        *trustLevel = TrustAnchor;
        return SECSuccess;
      }
#endif
    }
  }

  *trustLevel = InheritsTrust;
  return SECSuccess;
}

SECStatus
NSSCertDBTrustDomain::VerifySignedData(const CERTSignedData* signedData,
                                       const CERTCertificate* cert)
{
  return ::insanity::pkix::VerifySignedData(signedData, cert, mPinArg);
}

SECStatus
NSSCertDBTrustDomain::CheckRevocation(
  insanity::pkix::EndEntityOrCA endEntityOrCA,
  const CERTCertificate* cert,
  /*const*/ CERTCertificate* issuerCert,
  PRTime time,
  /*optional*/ const SECItem* stapledOCSPResponse)
{
  // Actively distrusted certificates will have already been blocked by
  // GetCertTrust.

  // TODO: need to verify that IsRevoked isn't called for trust anchors AND
  // that that fact is documented in insanity.

  PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
         ("NSSCertDBTrustDomain: Top of CheckRevocation\n"));

  PORT_Assert(cert);
  PORT_Assert(issuerCert);
  if (!cert || !issuerCert) {
    PORT_SetError(SEC_ERROR_INVALID_ARGS);
    return SECFailure;
  }

  // If we have a stapled OCSP response then the verification of that response
  // determines the result unless the OCSP response is expired. We make an
  // exception for expired responses because some servers, nginx in particular,
  // are known to serve expired responses due to bugs.
  if (stapledOCSPResponse) {
    PR_ASSERT(endEntityOrCA == MustBeEndEntity);
    SECStatus rv = VerifyEncodedOCSPResponse(*this, cert, issuerCert, time,
                                             stapledOCSPResponse);
    if (rv == SECSuccess) {
      return rv;
    }
    if (PR_GetError() != SEC_ERROR_OCSP_OLD_RESPONSE) {
      return rv;
    }
  }

  // TODO(bug 915932): Need to change this when we add the OCSP cache.

  // TODO: We still need to handle the fallback for expired responses. But,
  // if/when we disable OCSP fetching by default, it would be ambiguous whether
  // security.OCSP.enable==0 means "I want the default" or "I really never want
  // you to ever fetch OCSP."

  if ((mOCSPFetching == NeverFetchOCSP) ||
      (endEntityOrCA == MustBeCA && (mOCSPFetching == FetchOCSPForDVHardFail ||
                                     mOCSPFetching == FetchOCSPForDVSoftFail))) {
    return SECSuccess;
  }

  if (mOCSPFetching == LocalOnlyOCSPForEV) {
    PR_SetError(SEC_ERROR_OCSP_UNKNOWN_CERT, 0);
    return SECFailure;
  }

  ScopedPtr<char, PORT_Free_string>
    url(CERT_GetOCSPAuthorityInfoAccessLocation(cert));

  if (!url) {
    if (stapledOCSPResponse) {
      PR_SetError(SEC_ERROR_OCSP_OLD_RESPONSE, 0);
      return SECFailure;
    }
    if (mOCSPFetching == FetchOCSPForEV) {
      PR_SetError(SEC_ERROR_OCSP_UNKNOWN_CERT, 0);
      return SECFailure;
    }

    // Nothing to do if we don't have an OCSP responder URI for the cert; just
    // assume it is good. Note that this is the confusing, but intended,
    // interpretation of "strict" revocation checking in the face of a
    // certificate that lacks an OCSP responder URI.
    return SECSuccess;
  }

  ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return SECFailure;
  }

  const SECItem* request(CreateEncodedOCSPRequest(arena.get(), cert,
                                                  issuerCert));
  if (!request) {
    return SECFailure;
  }

  const SECItem* response(CERT_PostOCSPRequest(arena.get(), url.get(),
                                               request));
  if (!response) {
    if (mOCSPFetching != FetchOCSPForDVSoftFail) {
      PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
             ("NSSCertDBTrustDomain: returning SECFailure after "
              "CERT_PostOCSPRequest failure"));
      return SECFailure;
    }

    PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
           ("NSSCertDBTrustDomain: returning SECSuccess after "
            "CERT_PostOCSPRequest failure"));
    return SECSuccess; // Soft fail -> success :(
  }

  SECStatus rv = VerifyEncodedOCSPResponse(*this, cert, issuerCert, time,
                                           response);
  if (rv == SECSuccess || mOCSPFetching != FetchOCSPForDVSoftFail) {
    PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
      ("NSSCertDBTrustDomain: returning after VerifyEncodedOCSPResponse"));
    return rv;
  }

  PRErrorCode error = PR_GetError();
  if (error == SEC_ERROR_OCSP_UNKNOWN_CERT ||
      error == SEC_ERROR_REVOKED_CERTIFICATE) {
    return rv;
  }

  PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
         ("NSSCertDBTrustDomain: end of CheckRevocation"));

  return SECSuccess;
}

namespace {

static char*
nss_addEscape(const char* string, char quote)
{
  char* newString = 0;
  int escapes = 0, size = 0;
  const char* src;
  char* dest;

  for (src = string; *src; src++) {
  if ((*src == quote) || (*src == '\\')) {
    escapes++;
  }
  size++;
  }

  newString = (char*) PORT_ZAlloc(escapes + size + 1);
  if (!newString) {
    return nullptr;
  }

  for (src = string, dest = newString; *src; src++, dest++) {
    if ((*src == quote) || (*src == '\\')) {
      *dest++ = '\\';
    }
    *dest = *src;
  }

  return newString;
}

} // unnamed namespace

SECStatus
InitializeNSS(const char* dir, bool readOnly)
{
  // The NSS_INIT_NOROOTINIT flag turns off the loading of the root certs
  // module by NSS_Initialize because we will load it in InstallLoadableRoots
  // later.  It also allows us to work around a bug in the system NSS in
  // Ubuntu 8.04, which loads any nonexistent "<configdir>/libnssckbi.so" as
  // "/usr/lib/nss/libnssckbi.so".
  uint32_t flags = NSS_INIT_NOROOTINIT | NSS_INIT_OPTIMIZESPACE;
  if (readOnly) {
    flags |= NSS_INIT_READONLY;
  }
  return ::NSS_Initialize(dir, "", "", SECMOD_DB, flags);
}

void
DisableMD5()
{
  NSS_SetAlgorithmPolicy(SEC_OID_MD5,
    0, NSS_USE_ALG_IN_CERT_SIGNATURE | NSS_USE_ALG_IN_CMS_SIGNATURE);
  NSS_SetAlgorithmPolicy(SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION,
    0, NSS_USE_ALG_IN_CERT_SIGNATURE | NSS_USE_ALG_IN_CMS_SIGNATURE);
  NSS_SetAlgorithmPolicy(SEC_OID_PKCS5_PBE_WITH_MD5_AND_DES_CBC,
    0, NSS_USE_ALG_IN_CERT_SIGNATURE | NSS_USE_ALG_IN_CMS_SIGNATURE);
}

SECStatus
LoadLoadableRoots(/*optional*/ const char* dir, const char* modNameUTF8)
{
  PR_ASSERT(modNameUTF8);

  if (!modNameUTF8) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }

  ScopedPtr<char, PR_FreeLibraryName> fullLibraryPath(
    PR_GetLibraryName(dir, "nssckbi"));
  if (!fullLibraryPath) {
    return SECFailure;
  }

  ScopedPtr<char, PORT_Free_string> escaped_fullLibraryPath(
    nss_addEscape(fullLibraryPath.get(), '\"'));
  if (!escaped_fullLibraryPath) {
    return SECFailure;
  }

  // If a module exists with the same name, delete it.
  int modType;
  SECMOD_DeleteModule(modNameUTF8, &modType);

  ScopedPtr<char, PR_smprintf_free> pkcs11ModuleSpec(
    PR_smprintf("name=\"%s\" library=\"%s\"", modNameUTF8,
                escaped_fullLibraryPath.get()));
  if (!pkcs11ModuleSpec) {
    return SECFailure;
  }

  ScopedSECMODModule rootsModule(SECMOD_LoadUserModule(pkcs11ModuleSpec.get(),
                                                       nullptr, false));
  if (!rootsModule) {
    return SECFailure;
  }

  if (!rootsModule->loaded) {
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }

  return SECSuccess;
}

void
UnloadLoadableRoots(const char* modNameUTF8)
{
  PR_ASSERT(modNameUTF8);
  ScopedSECMODModule rootsModule(SECMOD_FindModule(modNameUTF8));

  if (rootsModule) {
    SECMOD_UnloadUserModule(rootsModule.get());
  }
}

void
SetClassicOCSPBehavior(CertVerifier::ocsp_download_config enabled,
                       CertVerifier::ocsp_strict_config strict,
                       CertVerifier::ocsp_get_config get)
{
  CERT_DisableOCSPDefaultResponder(CERT_GetDefaultCertDB());
  if (enabled == CertVerifier::ocsp_off) {
    CERT_DisableOCSPChecking(CERT_GetDefaultCertDB());
  } else {
    CERT_EnableOCSPChecking(CERT_GetDefaultCertDB());
  }

  SEC_OcspFailureMode failureMode = strict == CertVerifier::ocsp_strict
                                  ? ocspMode_FailureIsVerificationFailure
                                  : ocspMode_FailureIsNotAVerificationFailure;
  (void) CERT_SetOCSPFailureMode(failureMode);

  CERT_ForcePostMethodForOCSP(get != CertVerifier::ocsp_get_enabled);

  int OCSPTimeoutSeconds = 3;
  if (strict == CertVerifier::ocsp_strict) {
    OCSPTimeoutSeconds = 10;
  }
  CERT_SetOCSPTimeout(OCSPTimeoutSeconds);
}

char*
DefaultServerNicknameForCert(CERTCertificate* cert)
{
  char* nickname = nullptr;
  int count;
  bool conflict;
  char* servername = nullptr;

  servername = CERT_GetCommonName(&cert->subject);
  if (!servername) {
    // Certs without common names are strange, but they do exist...
    // Let's try to use another string for the nickname
    servername = CERT_GetOrgUnitName(&cert->subject);
    if (!servername) {
      servername = CERT_GetOrgName(&cert->subject);
      if (!servername) {
        servername = CERT_GetLocalityName(&cert->subject);
        if (!servername) {
          servername = CERT_GetStateName(&cert->subject);
          if (!servername) {
            servername = CERT_GetCountryName(&cert->subject);
            if (!servername) {
              // We tried hard, there is nothing more we can do.
              // A cert without any names doesn't really make sense.
              return nullptr;
            }
          }
        }
      }
    }
  }

  count = 1;
  while (1) {
    if (count == 1) {
      nickname = PR_smprintf("%s", servername);
    }
    else {
      nickname = PR_smprintf("%s #%d", servername, count);
    }
    if (!nickname) {
      break;
    }

    conflict = SEC_CertNicknameConflict(nickname, &cert->derSubject,
                                        cert->dbhandle);
    if (!conflict) {
      break;
    }
    PR_Free(nickname);
    count++;
  }
  PR_FREEIF(servername);
  return nickname;
}

void
SaveIntermediateCerts(const ScopedCERTCertList& certList)
{
  if (!certList) {
    return;
  }

  bool isEndEntity = true;
  for (CERTCertListNode* node = CERT_LIST_HEAD(certList);
        !CERT_LIST_END(node, certList);
        node = CERT_LIST_NEXT(node)) {
    if (isEndEntity) {
      // Skip the end-entity; we only want to store intermediates
      isEndEntity = false;
      continue;
    }

    if (node->cert->slot) {
      // This cert was found on a token, no need to remember it in the temp db.
      continue;
    }

    if (node->cert->isperm) {
      // We don't need to remember certs already stored in perm db.
      continue;
    }

    // We have found a signer cert that we want to remember.
    char* nickname = DefaultServerNicknameForCert(node->cert);
    if (nickname && *nickname) {
      ScopedPtr<PK11SlotInfo, PK11_FreeSlot> slot(PK11_GetInternalKeySlot());
      if (slot) {
        PK11_ImportCert(slot.get(), node->cert, CK_INVALID_HANDLE,
                        nickname, false);
      }
    }
    PR_FREEIF(nickname);
  }
}

} } // namespace mozilla::psm
