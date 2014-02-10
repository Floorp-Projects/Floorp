/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NSSCertDBTrustDomain.h"

#include <stdint.h>

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
                                           bool /*ocspDownloadEnabled*/,
                                           bool /*ocspStrict*/,
                                           void* pinArg)
  : mCertDBTrustType(certDBTrustType)
//  , mOCSPDownloadEnabled(ocspDownloadEnabled)
//  , mOCSPStrict(ocspStrict)
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
  if (!results) {
    return SECFailure;
  }

  return SECSuccess;
}

SECStatus
NSSCertDBTrustDomain::GetCertTrust(EndEntityOrCA endEntityOrCA,
                                   const CERTCertificate* candidateCert,
                                   /*out*/ TrustLevel* trustLevel)
{
  PORT_Assert(candidateCert);
  PORT_Assert(trustLevel);
  if (!candidateCert || !trustLevel) {
    PORT_SetError(SEC_ERROR_INVALID_ARGS);
    return SECFailure;
  }

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
      *trustLevel = TrustAnchor;
      return SECSuccess;
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
