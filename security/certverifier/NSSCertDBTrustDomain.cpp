/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NSSCertDBTrustDomain.h"

#include <stdint.h>

#include "ExtendedValidation.h"
#include "NSSErrorsService.h"
#include "OCSPRequestor.h"
#include "certdb.h"
#include "mozilla/Telemetry.h"
#include "nss.h"
#include "ocsp.h"
#include "pk11pub.h"
#include "pkix/pkix.h"
#include "prerror.h"
#include "prmem.h"
#include "prprf.h"
#include "secerr.h"
#include "secmod.h"

using namespace mozilla::pkix;

#ifdef PR_LOGGING
extern PRLogModuleInfo* gCertVerifierLog;
#endif

namespace mozilla { namespace psm {

const char BUILTIN_ROOTS_MODULE_DEFAULT_NAME[] = "Builtin Roots Module";

void PORT_Free_string(char* str) { PORT_Free(str); }

namespace {

typedef ScopedPtr<SECMODModule, SECMOD_DestroyModule> ScopedSECMODModule;

} // unnamed namespace

NSSCertDBTrustDomain::NSSCertDBTrustDomain(SECTrustType certDBTrustType,
                                           OCSPFetching ocspFetching,
                                           OCSPCache& ocspCache,
                                           void* pinArg,
                                           CertVerifier::ocsp_get_config ocspGETConfig,
                                           CERTChainVerifyCallback* checkChainCallback)
  : mCertDBTrustType(certDBTrustType)
  , mOCSPFetching(ocspFetching)
  , mOCSPCache(ocspCache)
  , mPinArg(pinArg)
  , mOCSPGetConfig(ocspGETConfig)
  , mCheckChainCallback(checkChainCallback)
{
}

SECStatus
NSSCertDBTrustDomain::FindPotentialIssuers(
  const SECItem* encodedIssuerName, PRTime time,
  /*out*/ mozilla::pkix::ScopedCERTCertList& results)
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
                                   const CertPolicyId& policy,
                                   const SECItem& candidateCertDER,
                                   /*out*/ TrustLevel* trustLevel)
{
  PR_ASSERT(trustLevel);

  if (!trustLevel) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }

#ifdef MOZ_NO_EV_CERTS
  if (!policy.IsAnyPolicy()) {
    PR_SetError(SEC_ERROR_POLICY_VALIDATION_FAILED, 0);
    return SECFailure;
  }
#endif

  // XXX: This would be cleaner and more efficient if we could get the trust
  // information without constructing a CERTCertificate here, but NSS doesn't
  // expose it in any other easy-to-use fashion. The use of
  // CERT_NewTempCertificate to get a CERTCertificate shouldn't be a
  // performance problem because NSS will just find the existing
  // CERTCertificate in its in-memory cache and return it.
  ScopedCERTCertificate candidateCert(
    CERT_NewTempCertificate(CERT_GetDefaultCertDB(),
                            const_cast<SECItem*>(&candidateCertDER), nullptr,
                            false, true));
  if (!candidateCert) {
    return SECFailure;
  }

  // XXX: CERT_GetCertTrust seems to be abusing SECStatus as a boolean, where
  // SECSuccess means that there is a trust record and SECFailure means there
  // is not a trust record. I looked at NSS's internal uses of
  // CERT_GetCertTrust, and all that code uses the result as a boolean meaning
  // "We have a trust record."
  CERTCertTrust trust;
  if (CERT_GetCertTrust(candidateCert.get(), &trust) == SECSuccess) {
    PRUint32 flags = SEC_GET_TRUST_FLAGS(&trust, mCertDBTrustType);

    // For DISTRUST, we use the CERTDB_TRUSTED or CERTDB_TRUSTED_CA bit,
    // because we can have active distrust for either type of cert. Note that
    // CERTDB_TERMINAL_RECORD means "stop trying to inherit trust" so if the
    // relevant trust bit isn't set then that means the cert must be considered
    // distrusted.
    PRUint32 relevantTrustBit =
      endEntityOrCA == EndEntityOrCA::MustBeCA ? CERTDB_TRUSTED_CA
                                               : CERTDB_TRUSTED;
    if (((flags & (relevantTrustBit|CERTDB_TERMINAL_RECORD)))
            == CERTDB_TERMINAL_RECORD) {
      *trustLevel = TrustLevel::ActivelyDistrusted;
      return SECSuccess;
    }

    // For TRUST, we only use the CERTDB_TRUSTED_CA bit, because Gecko hasn't
    // needed to consider end-entity certs to be their own trust anchors since
    // Gecko implemented nsICertOverrideService.
    if (flags & CERTDB_TRUSTED_CA) {
      if (policy.IsAnyPolicy()) {
        *trustLevel = TrustLevel::TrustAnchor;
        return SECSuccess;
      }
#ifndef MOZ_NO_EV_CERTS
      if (CertIsAuthoritativeForEVPolicy(candidateCert.get(), policy)) {
        *trustLevel = TrustLevel::TrustAnchor;
        return SECSuccess;
      }
#endif
    }
  }

  *trustLevel = TrustLevel::InheritsTrust;
  return SECSuccess;
}

SECStatus
NSSCertDBTrustDomain::VerifySignedData(const CERTSignedData* signedData,
                                       const SECItem& subjectPublicKeyInfo)
{
  return ::mozilla::pkix::VerifySignedData(signedData, subjectPublicKeyInfo,
                                           mPinArg);
}

static PRIntervalTime
OCSPFetchingTypeToTimeoutTime(NSSCertDBTrustDomain::OCSPFetching ocspFetching)
{
  switch (ocspFetching) {
    case NSSCertDBTrustDomain::FetchOCSPForDVSoftFail:
      return PR_SecondsToInterval(2);
    case NSSCertDBTrustDomain::FetchOCSPForEV:
    case NSSCertDBTrustDomain::FetchOCSPForDVHardFail:
      return PR_SecondsToInterval(10);
    // The rest of these are error cases. Assert in debug builds, but return
    // the default value corresponding to 2 seconds in release builds.
    case NSSCertDBTrustDomain::NeverFetchOCSP:
    case NSSCertDBTrustDomain::LocalOnlyOCSPForEV:
      PR_NOT_REACHED("we should never see this OCSPFetching type here");
    default:
      PR_NOT_REACHED("we're not handling every OCSPFetching type");
  }
  return PR_SecondsToInterval(2);
}

SECStatus
NSSCertDBTrustDomain::CheckRevocation(
  mozilla::pkix::EndEntityOrCA endEntityOrCA,
  const CERTCertificate* cert,
  /*const*/ CERTCertificate* issuerCert,
  PRTime time,
  /*optional*/ const SECItem* stapledOCSPResponse)
{
  // Actively distrusted certificates will have already been blocked by
  // GetCertTrust.

  // TODO: need to verify that IsRevoked isn't called for trust anchors AND
  // that that fact is documented in mozillapkix.

  PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
         ("NSSCertDBTrustDomain: Top of CheckRevocation\n"));

  PORT_Assert(cert);
  PORT_Assert(issuerCert);
  if (!cert || !issuerCert) {
    PORT_SetError(SEC_ERROR_INVALID_ARGS);
    return SECFailure;
  }

  // Bug 991815: The BR allow OCSP for intermediates to be up to one year old.
  // Since this affects EV there is no reason why DV should be more strict
  // so all intermediatates are allowed to have OCSP responses up to one year
  // old.
  uint16_t maxOCSPLifetimeInDays = 10;
  if (endEntityOrCA == EndEntityOrCA::MustBeCA) {
    maxOCSPLifetimeInDays = 365;
  }

  // If we have a stapled OCSP response then the verification of that response
  // determines the result unless the OCSP response is expired. We make an
  // exception for expired responses because some servers, nginx in particular,
  // are known to serve expired responses due to bugs.
  // We keep track of the result of verifying the stapled response but don't
  // immediately return failure if the response has expired.
  PRErrorCode stapledOCSPResponseErrorCode = 0;
  if (stapledOCSPResponse) {
    PR_ASSERT(endEntityOrCA == EndEntityOrCA::MustBeEndEntity);
    bool expired;
    SECStatus rv = VerifyAndMaybeCacheEncodedOCSPResponse(cert, issuerCert,
                                                          time,
                                                          maxOCSPLifetimeInDays,
                                                          stapledOCSPResponse,
                                                          ResponseWasStapled,
                                                          expired);
    if (rv == SECSuccess) {
      // stapled OCSP response present and good
      Telemetry::Accumulate(Telemetry::SSL_OCSP_STAPLING, 1);
      PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
             ("NSSCertDBTrustDomain: stapled OCSP response: good"));
      return rv;
    }
    stapledOCSPResponseErrorCode = PR_GetError();
    if (stapledOCSPResponseErrorCode == SEC_ERROR_OCSP_OLD_RESPONSE ||
        expired) {
      // stapled OCSP response present but expired
      Telemetry::Accumulate(Telemetry::SSL_OCSP_STAPLING, 3);
      PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
             ("NSSCertDBTrustDomain: expired stapled OCSP response"));
    } else {
      // stapled OCSP response present but invalid for some reason
      Telemetry::Accumulate(Telemetry::SSL_OCSP_STAPLING, 4);
      PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
             ("NSSCertDBTrustDomain: stapled OCSP response: failure"));
      return rv;
    }
  } else {
    // no stapled OCSP response
    Telemetry::Accumulate(Telemetry::SSL_OCSP_STAPLING, 2);
    PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
           ("NSSCertDBTrustDomain: no stapled OCSP response"));
  }

  PRErrorCode cachedResponseErrorCode = 0;
  PRTime cachedResponseValidThrough = 0;
  bool cachedResponsePresent = mOCSPCache.Get(cert, issuerCert,
                                              cachedResponseErrorCode,
                                              cachedResponseValidThrough);
  if (cachedResponsePresent) {
    if (cachedResponseErrorCode == 0 && cachedResponseValidThrough >= time) {
      PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
             ("NSSCertDBTrustDomain: cached OCSP response: good"));
      return SECSuccess;
    }
    // If we have a cached revoked response, use it.
    if (cachedResponseErrorCode == SEC_ERROR_REVOKED_CERTIFICATE) {
      PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
             ("NSSCertDBTrustDomain: cached OCSP response: revoked"));
      PR_SetError(SEC_ERROR_REVOKED_CERTIFICATE, 0);
      return SECFailure;
    }
    // The cached response may indicate an unknown certificate or it may be
    // expired. Don't return with either of these statuses yet - we may be
    // able to fetch a more recent one.
    PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
           ("NSSCertDBTrustDomain: cached OCSP response: error %ld valid "
           "until %lld", cachedResponseErrorCode, cachedResponseValidThrough));
    // When a good cached response has expired, it is more convenient
    // to convert that to an error code and just deal with
    // cachedResponseErrorCode from here on out.
    if (cachedResponseErrorCode == 0 && cachedResponseValidThrough < time) {
      cachedResponseErrorCode = SEC_ERROR_OCSP_OLD_RESPONSE;
    }
    // We may have a cached indication of server failure. Ignore it if
    // it has expired.
    if (cachedResponseErrorCode != 0 &&
        cachedResponseErrorCode != SEC_ERROR_OCSP_UNKNOWN_CERT &&
        cachedResponseErrorCode != SEC_ERROR_OCSP_OLD_RESPONSE &&
        cachedResponseValidThrough < time) {
      cachedResponseErrorCode = 0;
      cachedResponsePresent = false;
    }
  } else {
    PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
           ("NSSCertDBTrustDomain: no cached OCSP response"));
  }
  // At this point, if and only if cachedErrorResponseCode is 0, there was no
  // cached response.
  PR_ASSERT((!cachedResponsePresent && cachedResponseErrorCode == 0) ||
            (cachedResponsePresent && cachedResponseErrorCode != 0));

  // TODO: We still need to handle the fallback for expired responses. But,
  // if/when we disable OCSP fetching by default, it would be ambiguous whether
  // security.OCSP.enable==0 means "I want the default" or "I really never want
  // you to ever fetch OCSP."

  if ((mOCSPFetching == NeverFetchOCSP) ||
      (endEntityOrCA == EndEntityOrCA::MustBeCA &&
       (mOCSPFetching == FetchOCSPForDVHardFail ||
        mOCSPFetching == FetchOCSPForDVSoftFail))) {
    // We're not going to be doing any fetching, so if there was a cached
    // "unknown" response, say so.
    if (cachedResponseErrorCode == SEC_ERROR_OCSP_UNKNOWN_CERT) {
      PR_SetError(SEC_ERROR_OCSP_UNKNOWN_CERT, 0);
      return SECFailure;
    }
    // If we're doing hard-fail, we want to know if we have a cached response
    // that has expired.
    if (mOCSPFetching == FetchOCSPForDVHardFail &&
        cachedResponseErrorCode == SEC_ERROR_OCSP_OLD_RESPONSE) {
      PR_SetError(SEC_ERROR_OCSP_OLD_RESPONSE, 0);
      return SECFailure;
    }

    return SECSuccess;
  }

  if (mOCSPFetching == LocalOnlyOCSPForEV) {
    PR_SetError(cachedResponseErrorCode != 0 ? cachedResponseErrorCode
                                             : SEC_ERROR_OCSP_UNKNOWN_CERT, 0);
    return SECFailure;
  }

  ScopedPtr<char, PORT_Free_string>
    url(CERT_GetOCSPAuthorityInfoAccessLocation(cert));

  if (!url) {
    if (mOCSPFetching == FetchOCSPForEV ||
        cachedResponseErrorCode == SEC_ERROR_OCSP_UNKNOWN_CERT) {
      PR_SetError(SEC_ERROR_OCSP_UNKNOWN_CERT, 0);
      return SECFailure;
    }
    if (cachedResponseErrorCode == SEC_ERROR_OCSP_OLD_RESPONSE) {
      PR_SetError(SEC_ERROR_OCSP_OLD_RESPONSE, 0);
      return SECFailure;
    }
    if (stapledOCSPResponseErrorCode != 0) {
      PR_SetError(stapledOCSPResponseErrorCode, 0);
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

  // Only request a response if we didn't have a cached indication of failure
  // (don't keep requesting responses from a failing server).
  const SECItem* response;
  bool attemptedRequest;
  // Initialize error here to a value we check that it isn't later, because
  // the compiler on windows isn't smart enough to figure out that it's
  // always initialized before it gets used.
  PRErrorCode error = 0;
  if (cachedResponseErrorCode == 0 ||
      cachedResponseErrorCode == SEC_ERROR_OCSP_UNKNOWN_CERT ||
      cachedResponseErrorCode == SEC_ERROR_OCSP_OLD_RESPONSE) {
    const SECItem* request(CreateEncodedOCSPRequest(arena.get(), cert,
                                                    issuerCert));
    if (!request) {
      return SECFailure;
    }

    response = DoOCSPRequest(arena.get(), url.get(), request,
                             OCSPFetchingTypeToTimeoutTime(mOCSPFetching),
                             mOCSPGetConfig == CertVerifier::ocsp_get_enabled);
    if (!response) {
      error = PR_GetError();
    }
    attemptedRequest = true;
  } else {
    error = cachedResponseErrorCode;
    response = nullptr;
    attemptedRequest = false;
  }

  // If we don't have a response, either something went wrong when fetching it
  // or we didn't attempt to fetch a response because of a failing responder.
  if (!response) {
    MOZ_ASSERT(error != 0);
    // If we haven't actually attempted to fetch a response, we have nothing
    // new to tell the cache. Otherwise, we do.
    if (attemptedRequest) {
      PRTime timeout = time + ServerFailureDelay;
      SECStatus rv = mOCSPCache.Put(cert, issuerCert, error, time, timeout);
      if (rv != SECSuccess) {
        return SECFailure;
      }
    }
    PR_SetError(error, 0);
    if (mOCSPFetching != FetchOCSPForDVSoftFail) {
      PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
             ("NSSCertDBTrustDomain: returning SECFailure after "
              "OCSP request failure"));
      return SECFailure;
    }
    if (cachedResponseErrorCode == SEC_ERROR_OCSP_UNKNOWN_CERT) {
      PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
             ("NSSCertDBTrustDomain: returning SECFailure from cached "
              "response after OCSP request failure"));
      PR_SetError(cachedResponseErrorCode, 0);
      return SECFailure;
    }
    if (stapledOCSPResponseErrorCode != 0) {
      PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
             ("NSSCertDBTrustDomain: returning SECFailure from expired "
              "stapled response after OCSP request failure"));
      PR_SetError(stapledOCSPResponseErrorCode, 0);
      return SECFailure;
    }

    PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
           ("NSSCertDBTrustDomain: returning SECSuccess after "
            "OCSP request failure"));
    return SECSuccess; // Soft fail -> success :(
  }

  // If the response from the network has expired but indicates a revoked
  // or unknown certificate, PR_GetError() will return the appropriate error.
  // We actually ignore expired here.
  bool expired;
  SECStatus rv = VerifyAndMaybeCacheEncodedOCSPResponse(cert, issuerCert, time,
                                                        maxOCSPLifetimeInDays,
                                                        response,
                                                        ResponseIsFromNetwork,
                                                        expired);
  if (rv == SECSuccess || mOCSPFetching != FetchOCSPForDVSoftFail) {
    PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
      ("NSSCertDBTrustDomain: returning after VerifyEncodedOCSPResponse"));
    return rv;
  }

  error = PR_GetError();
  if (error == SEC_ERROR_OCSP_UNKNOWN_CERT ||
      error == SEC_ERROR_REVOKED_CERTIFICATE) {
    return rv;
  }

  if (stapledOCSPResponseErrorCode != 0) {
    PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
           ("NSSCertDBTrustDomain: returning SECFailure from expired stapled "
            "response after OCSP request verification failure"));
    PR_SetError(stapledOCSPResponseErrorCode, 0);
    return SECFailure;
  }

  PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
         ("NSSCertDBTrustDomain: end of CheckRevocation"));

  return SECSuccess; // Soft fail -> success :(
}

SECStatus
NSSCertDBTrustDomain::VerifyAndMaybeCacheEncodedOCSPResponse(
  const CERTCertificate* cert, CERTCertificate* issuerCert, PRTime time,
  uint16_t maxLifetimeInDays, const SECItem* encodedResponse,
  EncodedResponseSource responseSource, /*out*/ bool& expired)
{
  PRTime thisUpdate = 0;
  PRTime validThrough = 0;
  SECStatus rv = VerifyEncodedOCSPResponse(*this, cert, issuerCert, time,
                                           maxLifetimeInDays, encodedResponse,
                                           expired, &thisUpdate, &validThrough);
  PRErrorCode error = (rv == SECSuccess ? 0 : PR_GetError());
  // If a response was stapled and expired, we don't want to cache it. Return
  // early to simplify the logic here.
  if (responseSource == ResponseWasStapled && expired) {
    PR_ASSERT(rv != SECSuccess);
    return rv;
  }
  // validThrough is only trustworthy if the response successfully verifies
  // or it indicates a revoked or unknown certificate.
  // If this isn't the case, store an indication of failure (to prevent
  // repeatedly requesting a response from a failing server).
  if (rv != SECSuccess && error != SEC_ERROR_REVOKED_CERTIFICATE &&
      error != SEC_ERROR_OCSP_UNKNOWN_CERT) {
    validThrough = time + ServerFailureDelay;
  }
  if (responseSource == ResponseIsFromNetwork ||
      rv == SECSuccess ||
      error == SEC_ERROR_REVOKED_CERTIFICATE ||
      error == SEC_ERROR_OCSP_UNKNOWN_CERT) {
    PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
           ("NSSCertDBTrustDomain: caching OCSP response"));
    if (mOCSPCache.Put(cert, issuerCert, error, thisUpdate, validThrough)
          != SECSuccess) {
      return SECFailure;
    }
  }

  // If the verification failed, re-set to that original error
  // (the call to Put may have un-set it).
  if (rv != SECSuccess) {
    PR_SetError(error, 0);
  }
  return rv;
}

SECStatus
NSSCertDBTrustDomain::IsChainValid(const CERTCertList* certChain) {
  SECStatus rv = SECFailure;

  PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
      ("NSSCertDBTrustDomain: Top of IsChainValid mCheckCallback=%p",
       mCheckChainCallback));

  if (!mCheckChainCallback) {
    return SECSuccess;
  }
  if (!mCheckChainCallback->isChainValid) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }
  PRBool chainOK;
  rv = (mCheckChainCallback->isChainValid)(mCheckChainCallback->isChainValidArg,
                                           certChain, &chainOK);
  if (rv != SECSuccess) {
    return rv;
  }
  // rv = SECSuccess only implies successful call, now is time
  // to check the chain check status
  // we should only return success if the chain is valid
  if (chainOK) {
    return SECSuccess;
  }
  PR_SetError(PSM_ERROR_KEY_PINNING_FAILURE, 0);
  return SECFailure;
}

namespace {

static char*
nss_addEscape(const char* string, char quote)
{
  char* newString = 0;
  size_t escapes = 0, size = 0;
  const char* src;
  char* dest;

  for (src = string; *src; src++) {
  if ((*src == quote) || (*src == '\\')) {
    escapes++;
  }
  size++;
  }

  newString = (char*) PORT_ZAlloc(escapes + size + 1u);
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

  uint32_t OCSPTimeoutSeconds = 3u;
  if (strict == CertVerifier::ocsp_strict) {
    OCSPTimeoutSeconds = 10u;
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
