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
#include "OCSPVerificationTrustDomain.h"
#include "PublicKeyPinningService.h"
#include "cert.h"
#include "certdb.h"
#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/Move.h"
#include "mozilla/PodOperations.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Unused.h"
#include "nsNSSCertificate.h"
#include "nsServiceManagerUtils.h"
#include "nss.h"
#include "pk11pub.h"
#include "pkix/Result.h"
#include "pkix/pkix.h"
#include "pkix/pkixnss.h"
#include "prerror.h"
#include "prmem.h"
#include "secerr.h"

#include "CNNICHashWhitelist.inc"
#include "StartComAndWoSignData.inc"

using namespace mozilla;
using namespace mozilla::pkix;

extern LazyLogModule gCertVerifierLog;

static const uint64_t ServerFailureDelaySeconds = 5 * 60;

namespace mozilla { namespace psm {

NSSCertDBTrustDomain::NSSCertDBTrustDomain(SECTrustType certDBTrustType,
                                           OCSPFetching ocspFetching,
                                           OCSPCache& ocspCache,
             /*optional but shouldn't be*/ void* pinArg,
                                           CertVerifier::OcspGetConfig ocspGETConfig,
                                           TimeDuration ocspTimeoutSoft,
                                           TimeDuration ocspTimeoutHard,
                                           uint32_t certShortLifetimeInDays,
                                           CertVerifier::PinningMode pinningMode,
                                           unsigned int minRSABits,
                                           ValidityCheckingMode validityCheckingMode,
                                           CertVerifier::SHA1Mode sha1Mode,
                                           NetscapeStepUpPolicy netscapeStepUpPolicy,
                                           const OriginAttributes& originAttributes,
                                           UniqueCERTCertList& builtChain,
                              /*optional*/ PinningTelemetryInfo* pinningTelemetryInfo,
                              /*optional*/ const char* hostname)
  : mCertDBTrustType(certDBTrustType)
  , mOCSPFetching(ocspFetching)
  , mOCSPCache(ocspCache)
  , mPinArg(pinArg)
  , mOCSPGetConfig(ocspGETConfig)
  , mOCSPTimeoutSoft(ocspTimeoutSoft)
  , mOCSPTimeoutHard(ocspTimeoutHard)
  , mCertShortLifetimeInDays(certShortLifetimeInDays)
  , mPinningMode(pinningMode)
  , mMinRSABits(minRSABits)
  , mValidityCheckingMode(validityCheckingMode)
  , mSHA1Mode(sha1Mode)
  , mNetscapeStepUpPolicy(netscapeStepUpPolicy)
  , mOriginAttributes(originAttributes)
  , mBuiltChain(builtChain)
  , mPinningTelemetryInfo(pinningTelemetryInfo)
  , mHostname(hostname)
  , mCertBlocklist(do_GetService(NS_CERTBLOCKLIST_CONTRACTID))
  , mOCSPStaplingStatus(CertVerifier::OCSP_STAPLING_NEVER_CHECKED)
  , mSCTListFromCertificate()
  , mSCTListFromOCSPStapling()
{
}

// If useRoots is true, we only use root certificates in the candidate list.
// If useRoots is false, we only use non-root certificates in the list.
static Result
FindIssuerInner(const UniqueCERTCertList& candidates, bool useRoots,
                Input encodedIssuerName, TrustDomain::IssuerChecker& checker,
                /*out*/ bool& keepGoing)
{
  keepGoing = true;
  for (CERTCertListNode* n = CERT_LIST_HEAD(candidates);
       !CERT_LIST_END(n, candidates); n = CERT_LIST_NEXT(n)) {
    bool candidateIsRoot = !!n->cert->isRoot;
    if (candidateIsRoot != useRoots) {
      continue;
    }
    Input certDER;
    Result rv = certDER.Init(n->cert->derCert.data, n->cert->derCert.len);
    if (rv != Success) {
      continue; // probably too big
    }

    const SECItem encodedIssuerNameItem = {
      siBuffer,
      const_cast<unsigned char*>(encodedIssuerName.UnsafeGetData()),
      encodedIssuerName.GetLength()
    };
    ScopedAutoSECItem nameConstraints;
    SECStatus srv = CERT_GetImposedNameConstraints(&encodedIssuerNameItem,
                                                   &nameConstraints);
    if (srv != SECSuccess) {
      if (PR_GetError() != SEC_ERROR_EXTENSION_NOT_FOUND) {
        return Result::FATAL_ERROR_LIBRARY_FAILURE;
      }

      // If no imposed name constraints were found, continue without them
      rv = checker.Check(certDER, nullptr, keepGoing);
    } else {
      // Otherwise apply the constraints
      Input nameConstraintsInput;
      if (nameConstraintsInput.Init(nameConstraints.data, nameConstraints.len)
            != Success) {
        return Result::FATAL_ERROR_LIBRARY_FAILURE;
      }
      rv = checker.Check(certDER, &nameConstraintsInput, keepGoing);
    }
    if (rv != Success) {
      return rv;
    }
    if (!keepGoing) {
      break;
    }
  }

  return Success;
}

Result
NSSCertDBTrustDomain::FindIssuer(Input encodedIssuerName,
                                 IssuerChecker& checker, Time)
{
  // TODO: NSS seems to be ambiguous between "no potential issuers found" and
  // "there was an error trying to retrieve the potential issuers."
  SECItem encodedIssuerNameItem = UnsafeMapInputToSECItem(encodedIssuerName);
  UniqueCERTCertList
    candidates(CERT_CreateSubjectCertList(nullptr, CERT_GetDefaultCertDB(),
                                          &encodedIssuerNameItem, 0,
                                          false));
  if (candidates) {
    // First, try all the root certs; then try all the non-root certs.
    bool keepGoing;
    Result rv = FindIssuerInner(candidates, true, encodedIssuerName, checker,
                                keepGoing);
    if (rv != Success) {
      return rv;
    }
    if (keepGoing) {
      rv = FindIssuerInner(candidates, false, encodedIssuerName, checker,
                           keepGoing);
      if (rv != Success) {
        return rv;
      }
    }
  }

  return Success;
}

Result
NSSCertDBTrustDomain::GetCertTrust(EndEntityOrCA endEntityOrCA,
                                   const CertPolicyId& policy,
                                   Input candidateCertDER,
                                   /*out*/ TrustLevel& trustLevel)
{
  // XXX: This would be cleaner and more efficient if we could get the trust
  // information without constructing a CERTCertificate here, but NSS doesn't
  // expose it in any other easy-to-use fashion. The use of
  // CERT_NewTempCertificate to get a CERTCertificate shouldn't be a
  // performance problem because NSS will just find the existing
  // CERTCertificate in its in-memory cache and return it.
  SECItem candidateCertDERSECItem = UnsafeMapInputToSECItem(candidateCertDER);
  UniqueCERTCertificate candidateCert(
    CERT_NewTempCertificate(CERT_GetDefaultCertDB(), &candidateCertDERSECItem,
                            nullptr, false, true));
  if (!candidateCert) {
    return MapPRErrorCodeToResult(PR_GetError());
  }

  // Check the certificate against the OneCRL cert blocklist
  if (!mCertBlocklist) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  // The certificate blocklist currently only applies to TLS server
  // certificates.
  if (mCertDBTrustType == trustSSL) {
    bool isCertRevoked;
    nsresult nsrv = mCertBlocklist->IsCertRevoked(
                      candidateCert->derIssuer.data,
                      candidateCert->derIssuer.len,
                      candidateCert->serialNumber.data,
                      candidateCert->serialNumber.len,
                      candidateCert->derSubject.data,
                      candidateCert->derSubject.len,
                      candidateCert->derPublicKey.data,
                      candidateCert->derPublicKey.len,
                      &isCertRevoked);
    if (NS_FAILED(nsrv)) {
      return Result::FATAL_ERROR_LIBRARY_FAILURE;
    }

    if (isCertRevoked) {
      MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
             ("NSSCertDBTrustDomain: certificate is in blocklist"));
      return Result::ERROR_REVOKED_CERTIFICATE;
    }
  }

  // XXX: CERT_GetCertTrust seems to be abusing SECStatus as a boolean, where
  // SECSuccess means that there is a trust record and SECFailure means there
  // is not a trust record. I looked at NSS's internal uses of
  // CERT_GetCertTrust, and all that code uses the result as a boolean meaning
  // "We have a trust record."
  CERTCertTrust trust;
  if (CERT_GetCertTrust(candidateCert.get(), &trust) == SECSuccess) {
    uint32_t flags = SEC_GET_TRUST_FLAGS(&trust, mCertDBTrustType);

    // For DISTRUST, we use the CERTDB_TRUSTED or CERTDB_TRUSTED_CA bit,
    // because we can have active distrust for either type of cert. Note that
    // CERTDB_TERMINAL_RECORD means "stop trying to inherit trust" so if the
    // relevant trust bit isn't set then that means the cert must be considered
    // distrusted.
    uint32_t relevantTrustBit =
      endEntityOrCA == EndEntityOrCA::MustBeCA ? CERTDB_TRUSTED_CA
                                               : CERTDB_TRUSTED;
    if (((flags & (relevantTrustBit|CERTDB_TERMINAL_RECORD)))
            == CERTDB_TERMINAL_RECORD) {
      trustLevel = TrustLevel::ActivelyDistrusted;
      return Success;
    }

    // For TRUST, we only use the CERTDB_TRUSTED_CA bit, because Gecko hasn't
    // needed to consider end-entity certs to be their own trust anchors since
    // Gecko implemented nsICertOverrideService.
    // Of course, for this to work as expected, we need to make sure we're
    // inquiring about the trust of a CA and not an end-entity. If an end-entity
    // has the CERTDB_TRUSTED_CA bit set, Gecko does not consider it to be a
    // trust anchor; it must inherit its trust.
    if (flags & CERTDB_TRUSTED_CA && endEntityOrCA == EndEntityOrCA::MustBeCA) {
      if (policy.IsAnyPolicy()) {
        trustLevel = TrustLevel::TrustAnchor;
        return Success;
      }
      if (CertIsAuthoritativeForEVPolicy(candidateCert, policy)) {
        trustLevel = TrustLevel::TrustAnchor;
        return Success;
      }
    }
  }

  trustLevel = TrustLevel::InheritsTrust;
  return Success;
}

Result
NSSCertDBTrustDomain::DigestBuf(Input item, DigestAlgorithm digestAlg,
                                /*out*/ uint8_t* digestBuf, size_t digestBufLen)
{
  return DigestBufNSS(item, digestAlg, digestBuf, digestBufLen);
}

TimeDuration
NSSCertDBTrustDomain::GetOCSPTimeout() const
{
  switch (mOCSPFetching) {
    case NSSCertDBTrustDomain::FetchOCSPForDVSoftFail:
      return mOCSPTimeoutSoft;
    case NSSCertDBTrustDomain::FetchOCSPForEV:
    case NSSCertDBTrustDomain::FetchOCSPForDVHardFail:
      return mOCSPTimeoutHard;
    // The rest of these are error cases. Assert in debug builds, but return
    // the soft timeout value in release builds.
    case NSSCertDBTrustDomain::NeverFetchOCSP:
    case NSSCertDBTrustDomain::LocalOnlyOCSPForEV:
      MOZ_ASSERT_UNREACHABLE("we should never see this OCSPFetching type here");
      break;
  }

  MOZ_ASSERT_UNREACHABLE("we're not handling every OCSPFetching type");
  return mOCSPTimeoutSoft;
}

// Copied and modified from CERT_GetOCSPAuthorityInfoAccessLocation and
// CERT_GetGeneralNameByType. Returns a non-Result::Success result on error,
// Success with url == nullptr when an OCSP URI was not found, and Success with
// url != nullptr when an OCSP URI was found. The output url will be owned
// by the arena.
static Result
GetOCSPAuthorityInfoAccessLocation(const UniquePLArenaPool& arena,
                                   Input aiaExtension,
                                   /*out*/ char const*& url)
{
  MOZ_ASSERT(arena.get());
  if (!arena.get()) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }

  url = nullptr;
  SECItem aiaExtensionSECItem = UnsafeMapInputToSECItem(aiaExtension);
  CERTAuthInfoAccess** aia =
    CERT_DecodeAuthInfoAccessExtension(arena.get(), &aiaExtensionSECItem);
  if (!aia) {
    return Result::ERROR_CERT_BAD_ACCESS_LOCATION;
  }
  for (size_t i = 0; aia[i]; ++i) {
    if (SECOID_FindOIDTag(&aia[i]->method) == SEC_OID_PKIX_OCSP) {
      // NSS chooses the **last** OCSP URL; we choose the **first**
      CERTGeneralName* current = aia[i]->location;
      if (!current) {
        continue;
      }
      do {
        if (current->type == certURI) {
          const SECItem& location = current->name.other;
          // (location.len + 1) must be small enough to fit into a uint32_t,
          // but we limit it to a smaller bound to reduce OOM risk.
          if (location.len > 1024 || memchr(location.data, 0, location.len)) {
            // Reject embedded nulls. (NSS doesn't do this)
            return Result::ERROR_CERT_BAD_ACCESS_LOCATION;
          }
          // Copy the non-null-terminated SECItem into a null-terminated string.
          char* nullTerminatedURL(
            static_cast<char*>(PORT_ArenaAlloc(arena.get(), location.len + 1)));
          if (!nullTerminatedURL) {
            return Result::FATAL_ERROR_NO_MEMORY;
          }
          memcpy(nullTerminatedURL, location.data, location.len);
          nullTerminatedURL[location.len] = 0;
          url = nullTerminatedURL;
          return Success;
        }
        current = CERT_GetNextGeneralName(current);
      } while (current != aia[i]->location);
    }
  }

  return Success;
}

Result
NSSCertDBTrustDomain::CheckRevocation(EndEntityOrCA endEntityOrCA,
                                      const CertID& certID, Time time,
                                      Duration validityDuration,
                         /*optional*/ const Input* stapledOCSPResponse,
                         /*optional*/ const Input* aiaExtension)
{
  // Actively distrusted certificates will have already been blocked by
  // GetCertTrust.

  // TODO: need to verify that IsRevoked isn't called for trust anchors AND
  // that that fact is documented in mozillapkix.

  MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
         ("NSSCertDBTrustDomain: Top of CheckRevocation\n"));

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
  //
  // We only set the OCSP stapling status if we're validating the end-entity
  // certificate. Non-end-entity certificates would always be
  // OCSP_STAPLING_NONE unless/until we implement multi-stapling.
  Result stapledOCSPResponseResult = Success;
  if (stapledOCSPResponse) {
    MOZ_ASSERT(endEntityOrCA == EndEntityOrCA::MustBeEndEntity);
    bool expired;
    stapledOCSPResponseResult =
      VerifyAndMaybeCacheEncodedOCSPResponse(certID, time,
                                             maxOCSPLifetimeInDays,
                                             *stapledOCSPResponse,
                                             ResponseWasStapled, expired);
    if (stapledOCSPResponseResult == Success) {
      // stapled OCSP response present and good
      mOCSPStaplingStatus = CertVerifier::OCSP_STAPLING_GOOD;
      MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
             ("NSSCertDBTrustDomain: stapled OCSP response: good"));
      return Success;
    }
    if (stapledOCSPResponseResult == Result::ERROR_OCSP_OLD_RESPONSE ||
        expired) {
      // stapled OCSP response present but expired
      mOCSPStaplingStatus = CertVerifier::OCSP_STAPLING_EXPIRED;
      MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
             ("NSSCertDBTrustDomain: expired stapled OCSP response"));
    } else {
      // stapled OCSP response present but invalid for some reason
      mOCSPStaplingStatus = CertVerifier::OCSP_STAPLING_INVALID;
      MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
             ("NSSCertDBTrustDomain: stapled OCSP response: failure"));
      return stapledOCSPResponseResult;
    }
  } else if (endEntityOrCA == EndEntityOrCA::MustBeEndEntity) {
    // no stapled OCSP response
    mOCSPStaplingStatus = CertVerifier::OCSP_STAPLING_NONE;
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
           ("NSSCertDBTrustDomain: no stapled OCSP response"));
  }

  Result cachedResponseResult = Success;
  Time cachedResponseValidThrough(Time::uninitialized);
  bool cachedResponsePresent = mOCSPCache.Get(certID, mOriginAttributes,
                                              cachedResponseResult,
                                              cachedResponseValidThrough);
  if (cachedResponsePresent) {
    if (cachedResponseResult == Success && cachedResponseValidThrough >= time) {
      MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
             ("NSSCertDBTrustDomain: cached OCSP response: good"));
      return Success;
    }
    // If we have a cached revoked response, use it.
    if (cachedResponseResult == Result::ERROR_REVOKED_CERTIFICATE) {
      MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
             ("NSSCertDBTrustDomain: cached OCSP response: revoked"));
      return Result::ERROR_REVOKED_CERTIFICATE;
    }
    // The cached response may indicate an unknown certificate or it may be
    // expired. Don't return with either of these statuses yet - we may be
    // able to fetch a more recent one.
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
           ("NSSCertDBTrustDomain: cached OCSP response: error %d",
            static_cast<int>(cachedResponseResult)));
    // When a good cached response has expired, it is more convenient
    // to convert that to an error code and just deal with
    // cachedResponseResult from here on out.
    if (cachedResponseResult == Success && cachedResponseValidThrough < time) {
      cachedResponseResult = Result::ERROR_OCSP_OLD_RESPONSE;
    }
    // We may have a cached indication of server failure. Ignore it if
    // it has expired.
    if (cachedResponseResult != Success &&
        cachedResponseResult != Result::ERROR_OCSP_UNKNOWN_CERT &&
        cachedResponseResult != Result::ERROR_OCSP_OLD_RESPONSE &&
        cachedResponseValidThrough < time) {
      cachedResponseResult = Success;
      cachedResponsePresent = false;
    }
  } else {
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
           ("NSSCertDBTrustDomain: no cached OCSP response"));
  }
  // At this point, if and only if cachedErrorResult is Success, there was no
  // cached response.
  MOZ_ASSERT((!cachedResponsePresent && cachedResponseResult == Success) ||
             (cachedResponsePresent && cachedResponseResult != Success));

  // If we have a fresh OneCRL Blocklist we can skip OCSP for CA certs
  bool blocklistIsFresh;
  nsresult nsrv = mCertBlocklist->IsBlocklistFresh(&blocklistIsFresh);
  if (NS_FAILED(nsrv)) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  // TODO: We still need to handle the fallback for expired responses. But,
  // if/when we disable OCSP fetching by default, it would be ambiguous whether
  // security.OCSP.enable==0 means "I want the default" or "I really never want
  // you to ever fetch OCSP."

  Duration shortLifetime(mCertShortLifetimeInDays * Time::ONE_DAY_IN_SECONDS);

  if ((mOCSPFetching == NeverFetchOCSP) ||
      (validityDuration < shortLifetime) ||
      (endEntityOrCA == EndEntityOrCA::MustBeCA &&
       (mOCSPFetching == FetchOCSPForDVHardFail ||
        mOCSPFetching == FetchOCSPForDVSoftFail ||
        blocklistIsFresh))) {
    // We're not going to be doing any fetching, so if there was a cached
    // "unknown" response, say so.
    if (cachedResponseResult == Result::ERROR_OCSP_UNKNOWN_CERT) {
      return Result::ERROR_OCSP_UNKNOWN_CERT;
    }
    // If we're doing hard-fail, we want to know if we have a cached response
    // that has expired.
    if (mOCSPFetching == FetchOCSPForDVHardFail &&
        cachedResponseResult == Result::ERROR_OCSP_OLD_RESPONSE) {
      return Result::ERROR_OCSP_OLD_RESPONSE;
    }

    return Success;
  }

  if (mOCSPFetching == LocalOnlyOCSPForEV) {
    if (cachedResponseResult != Success) {
      return cachedResponseResult;
    }
    return Result::ERROR_OCSP_UNKNOWN_CERT;
  }

  UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!arena) {
    return Result::FATAL_ERROR_NO_MEMORY;
  }

  Result rv;
  const char* url = nullptr; // owned by the arena

  if (aiaExtension) {
    rv = GetOCSPAuthorityInfoAccessLocation(arena, *aiaExtension, url);
    if (rv != Success) {
      return rv;
    }
  }

  if (!url) {
    if (mOCSPFetching == FetchOCSPForEV ||
        cachedResponseResult == Result::ERROR_OCSP_UNKNOWN_CERT) {
      return Result::ERROR_OCSP_UNKNOWN_CERT;
    }
    if (cachedResponseResult == Result::ERROR_OCSP_OLD_RESPONSE) {
      return Result::ERROR_OCSP_OLD_RESPONSE;
    }
    if (stapledOCSPResponseResult != Success) {
      return stapledOCSPResponseResult;
    }

    // Nothing to do if we don't have an OCSP responder URI for the cert; just
    // assume it is good. Note that this is the confusing, but intended,
    // interpretation of "strict" revocation checking in the face of a
    // certificate that lacks an OCSP responder URI.
    return Success;
  }

  // Only request a response if we didn't have a cached indication of failure
  // (don't keep requesting responses from a failing server).
  Input response;
  bool attemptedRequest;
  if (cachedResponseResult == Success ||
      cachedResponseResult == Result::ERROR_OCSP_UNKNOWN_CERT ||
      cachedResponseResult == Result::ERROR_OCSP_OLD_RESPONSE) {
    uint8_t ocspRequest[OCSP_REQUEST_MAX_LENGTH];
    size_t ocspRequestLength;
    rv = CreateEncodedOCSPRequest(*this, certID, ocspRequest,
                                  ocspRequestLength);
    if (rv != Success) {
      return rv;
    }
    SECItem ocspRequestItem = {
      siBuffer,
      ocspRequest,
      static_cast<unsigned int>(ocspRequestLength)
    };
    // Owned by arena
    SECItem* responseSECItem = nullptr;
    Result tempRV =
      DoOCSPRequest(arena, url, mOriginAttributes, &ocspRequestItem,
                    GetOCSPTimeout(),
                    mOCSPGetConfig == CertVerifier::ocspGetEnabled,
                    responseSECItem);
    MOZ_ASSERT((tempRV != Success) || responseSECItem);
    if (tempRV != Success) {
      rv = tempRV;
    } else if (response.Init(responseSECItem->data, responseSECItem->len)
                 != Success) {
      rv = Result::ERROR_OCSP_MALFORMED_RESPONSE; // too big
    }
    attemptedRequest = true;
  } else {
    rv = cachedResponseResult;
    attemptedRequest = false;
  }

  if (response.GetLength() == 0) {
    Result error = rv;
    if (attemptedRequest) {
      Time timeout(time);
      if (timeout.AddSeconds(ServerFailureDelaySeconds) != Success) {
        return Result::FATAL_ERROR_LIBRARY_FAILURE; // integer overflow
      }
      rv = mOCSPCache.Put(certID, mOriginAttributes, error, time, timeout);
      if (rv != Success) {
        return rv;
      }
    }
    if (mOCSPFetching != FetchOCSPForDVSoftFail) {
      MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
             ("NSSCertDBTrustDomain: returning SECFailure after "
              "OCSP request failure"));
      return error;
    }
    if (cachedResponseResult == Result::ERROR_OCSP_UNKNOWN_CERT) {
      MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
             ("NSSCertDBTrustDomain: returning SECFailure from cached "
              "response after OCSP request failure"));
      return cachedResponseResult;
    }
    if (stapledOCSPResponseResult != Success) {
      MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
             ("NSSCertDBTrustDomain: returning SECFailure from expired "
              "stapled response after OCSP request failure"));
      return stapledOCSPResponseResult;
    }

    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
           ("NSSCertDBTrustDomain: returning SECSuccess after "
            "OCSP request failure"));
    return Success; // Soft fail -> success :(
  }

  // If the response from the network has expired but indicates a revoked
  // or unknown certificate, PR_GetError() will return the appropriate error.
  // We actually ignore expired here.
  bool expired;
  rv = VerifyAndMaybeCacheEncodedOCSPResponse(certID, time,
                                              maxOCSPLifetimeInDays,
                                              response, ResponseIsFromNetwork,
                                              expired);
  if (rv == Success || mOCSPFetching != FetchOCSPForDVSoftFail) {
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
      ("NSSCertDBTrustDomain: returning after VerifyEncodedOCSPResponse"));
    return rv;
  }

  if (rv == Result::ERROR_OCSP_UNKNOWN_CERT ||
      rv == Result::ERROR_REVOKED_CERTIFICATE) {
    return rv;
  }
  if (stapledOCSPResponseResult != Success) {
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
           ("NSSCertDBTrustDomain: returning SECFailure from expired stapled "
            "response after OCSP request verification failure"));
    return stapledOCSPResponseResult;
  }

  MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
         ("NSSCertDBTrustDomain: end of CheckRevocation"));

  return Success; // Soft fail -> success :(
}

Result
NSSCertDBTrustDomain::VerifyAndMaybeCacheEncodedOCSPResponse(
  const CertID& certID, Time time, uint16_t maxLifetimeInDays,
  Input encodedResponse, EncodedResponseSource responseSource,
  /*out*/ bool& expired)
{
  Time thisUpdate(Time::uninitialized);
  Time validThrough(Time::uninitialized);

  // We use a try and fallback approach which first mandates good signature
  // digest algorithms, then falls back to SHA-1 if this fails. If a delegated
  // OCSP response signing certificate was issued with a SHA-1 signature,
  // verification initially fails. We cache the failure and then re-use that
  // result even when doing fallback (i.e. when weak signature digest algorithms
  // should succeed). To address this we use an OCSPVerificationTrustDomain
  // here, rather than using *this, to ensure verification succeeds for all
  // allowed signature digest algorithms.
  OCSPVerificationTrustDomain trustDomain(*this);
  Result rv = VerifyEncodedOCSPResponse(trustDomain, certID, time,
                                        maxLifetimeInDays, encodedResponse,
                                        expired, &thisUpdate, &validThrough);
  // If a response was stapled and expired, we don't want to cache it. Return
  // early to simplify the logic here.
  if (responseSource == ResponseWasStapled && expired) {
    MOZ_ASSERT(rv != Success);
    return rv;
  }
  // validThrough is only trustworthy if the response successfully verifies
  // or it indicates a revoked or unknown certificate.
  // If this isn't the case, store an indication of failure (to prevent
  // repeatedly requesting a response from a failing server).
  if (rv != Success && rv != Result::ERROR_REVOKED_CERTIFICATE &&
      rv != Result::ERROR_OCSP_UNKNOWN_CERT) {
    validThrough = time;
    if (validThrough.AddSeconds(ServerFailureDelaySeconds) != Success) {
      return Result::FATAL_ERROR_LIBRARY_FAILURE; // integer overflow
    }
  }
  if (responseSource == ResponseIsFromNetwork ||
      rv == Success ||
      rv == Result::ERROR_REVOKED_CERTIFICATE ||
      rv == Result::ERROR_OCSP_UNKNOWN_CERT) {
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
           ("NSSCertDBTrustDomain: caching OCSP response"));
    Result putRV = mOCSPCache.Put(certID, mOriginAttributes, rv, thisUpdate,
                                  validThrough);
    if (putRV != Success) {
      return putRV;
    }
  }

  return rv;
}

static const uint8_t CNNIC_ROOT_CA_SUBJECT_DATA[] =
  "\x30\x32\x31\x0B\x30\x09\x06\x03\x55\x04\x06\x13\x02\x43\x4E\x31\x0E\x30"
  "\x0C\x06\x03\x55\x04\x0A\x13\x05\x43\x4E\x4E\x49\x43\x31\x13\x30\x11\x06"
  "\x03\x55\x04\x03\x13\x0A\x43\x4E\x4E\x49\x43\x20\x52\x4F\x4F\x54";

static const uint8_t CNNIC_EV_ROOT_CA_SUBJECT_DATA[] =
  "\x30\x81\x8A\x31\x0B\x30\x09\x06\x03\x55\x04\x06\x13\x02\x43\x4E\x31\x32"
  "\x30\x30\x06\x03\x55\x04\x0A\x0C\x29\x43\x68\x69\x6E\x61\x20\x49\x6E\x74"
  "\x65\x72\x6E\x65\x74\x20\x4E\x65\x74\x77\x6F\x72\x6B\x20\x49\x6E\x66\x6F"
  "\x72\x6D\x61\x74\x69\x6F\x6E\x20\x43\x65\x6E\x74\x65\x72\x31\x47\x30\x45"
  "\x06\x03\x55\x04\x03\x0C\x3E\x43\x68\x69\x6E\x61\x20\x49\x6E\x74\x65\x72"
  "\x6E\x65\x74\x20\x4E\x65\x74\x77\x6F\x72\x6B\x20\x49\x6E\x66\x6F\x72\x6D"
  "\x61\x74\x69\x6F\x6E\x20\x43\x65\x6E\x74\x65\x72\x20\x45\x56\x20\x43\x65"
  "\x72\x74\x69\x66\x69\x63\x61\x74\x65\x73\x20\x52\x6F\x6F\x74";

class WhitelistedCNNICHashBinarySearchComparator
{
public:
  explicit WhitelistedCNNICHashBinarySearchComparator(const uint8_t* aTarget,
                                                      size_t aTargetLength)
    : mTarget(aTarget)
  {
    MOZ_ASSERT(aTargetLength == CNNIC_WHITELIST_HASH_LEN,
               "Hashes should be of the same length.");
  }

  int operator()(const WhitelistedCNNICHash val) const {
    return memcmp(mTarget, val.hash, CNNIC_WHITELIST_HASH_LEN);
  }

private:
  const uint8_t* mTarget;
};

static bool
CertIsStartComOrWoSign(const CERTCertificate* cert)
{
  for (const DataAndLength& dn : StartComAndWoSignDNs) {
    if (cert->derSubject.len == dn.len &&
        PodEqual(cert->derSubject.data, dn.data, dn.len)) {
      return true;
    }
  }
  return false;
}

// If a certificate in the given chain appears to have been issued by one of
// seven roots operated by StartCom and WoSign that are not trusted to issue new
// certificates, verify that the end-entity has a notBefore date before 21
// October 2016. If the value of notBefore is after this time, the chain is not
// valid.
// (NB: While there are seven distinct roots being checked for, two of them
// share distinguished names, resulting in six distinct distinguished names to
// actually look for.)
static Result
CheckForStartComOrWoSign(const UniqueCERTCertList& certChain)
{
  if (CERT_LIST_EMPTY(certChain)) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  const CERTCertListNode* endEntityNode = CERT_LIST_HEAD(certChain);
  if (!endEntityNode || !endEntityNode->cert) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  PRTime notBefore;
  PRTime notAfter;
  if (CERT_GetCertTimes(endEntityNode->cert, &notBefore, &notAfter)
        != SECSuccess) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  // PRTime is microseconds since the epoch, whereas JS time is milliseconds.
  // (new Date("2016-10-21T00:00:00Z")).getTime() * 1000
  static const PRTime OCTOBER_21_2016 = 1477008000000000;
  if (notBefore <= OCTOBER_21_2016) {
    return Success;
  }

  for (const CERTCertListNode* node = CERT_LIST_HEAD(certChain);
       !CERT_LIST_END(node, certChain); node = CERT_LIST_NEXT(node)) {
    if (!node || !node->cert) {
      return Result::FATAL_ERROR_LIBRARY_FAILURE;
    }
    if (CertIsStartComOrWoSign(node->cert)) {
      return Result::ERROR_REVOKED_CERTIFICATE;
    }
  }
  return Success;
}

// python DottedOIDToCode.py sGlobalSignEVPolicyBytes 1.3.6.1.4.1.4146.1.1
static const uint8_t sGlobalSignEVPolicyBytes[] = {
  0x2b, 0x06, 0x01, 0x04, 0x01, 0xa0, 0x32, 0x01, 0x01
};

static const CertPolicyId sGlobalSignEVPolicy = {
  sizeof(sGlobalSignEVPolicyBytes),
  // It's unfortunate, but there isn't a nice way to do this.
  // Just make sure these bytes match sGlobalSignEVPolicyBytes.
  { 0x2b, 0x06, 0x01, 0x04, 0x01, 0xa0, 0x32, 0x01, 0x01 }
};

static const unsigned char sGlobalSignRootCAR2SubjectBytes[] = {
  0x30, 0x4c, 0x31, 0x20, 0x30, 0x1e, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x13, 0x17,
  0x47, 0x6c, 0x6f, 0x62, 0x61, 0x6c, 0x53, 0x69, 0x67, 0x6e, 0x20, 0x52, 0x6f,
  0x6f, 0x74, 0x20, 0x43, 0x41, 0x20, 0x2d, 0x20, 0x52, 0x32, 0x31, 0x13, 0x30,
  0x11, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x13, 0x0a, 0x47, 0x6c, 0x6f, 0x62, 0x61,
  0x6c, 0x53, 0x69, 0x67, 0x6e, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04,
  0x03, 0x13, 0x0a, 0x47, 0x6c, 0x6f, 0x62, 0x61, 0x6c, 0x53, 0x69, 0x67, 0x6e,
};

static const unsigned char sGlobalSignRootCAR2SPKIBytes[] = {
  0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7,
  0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00, 0x30, 0x82,
  0x01, 0x0a, 0x02, 0x82, 0x01, 0x01, 0x00, 0xa6, 0xcf, 0x24, 0x0e, 0xbe, 0x2e,
  0x6f, 0x28, 0x99, 0x45, 0x42, 0xc4, 0xab, 0x3e, 0x21, 0x54, 0x9b, 0x0b, 0xd3,
  0x7f, 0x84, 0x70, 0xfa, 0x12, 0xb3, 0xcb, 0xbf, 0x87, 0x5f, 0xc6, 0x7f, 0x86,
  0xd3, 0xb2, 0x30, 0x5c, 0xd6, 0xfd, 0xad, 0xf1, 0x7b, 0xdc, 0xe5, 0xf8, 0x60,
  0x96, 0x09, 0x92, 0x10, 0xf5, 0xd0, 0x53, 0xde, 0xfb, 0x7b, 0x7e, 0x73, 0x88,
  0xac, 0x52, 0x88, 0x7b, 0x4a, 0xa6, 0xca, 0x49, 0xa6, 0x5e, 0xa8, 0xa7, 0x8c,
  0x5a, 0x11, 0xbc, 0x7a, 0x82, 0xeb, 0xbe, 0x8c, 0xe9, 0xb3, 0xac, 0x96, 0x25,
  0x07, 0x97, 0x4a, 0x99, 0x2a, 0x07, 0x2f, 0xb4, 0x1e, 0x77, 0xbf, 0x8a, 0x0f,
  0xb5, 0x02, 0x7c, 0x1b, 0x96, 0xb8, 0xc5, 0xb9, 0x3a, 0x2c, 0xbc, 0xd6, 0x12,
  0xb9, 0xeb, 0x59, 0x7d, 0xe2, 0xd0, 0x06, 0x86, 0x5f, 0x5e, 0x49, 0x6a, 0xb5,
  0x39, 0x5e, 0x88, 0x34, 0xec, 0xbc, 0x78, 0x0c, 0x08, 0x98, 0x84, 0x6c, 0xa8,
  0xcd, 0x4b, 0xb4, 0xa0, 0x7d, 0x0c, 0x79, 0x4d, 0xf0, 0xb8, 0x2d, 0xcb, 0x21,
  0xca, 0xd5, 0x6c, 0x5b, 0x7d, 0xe1, 0xa0, 0x29, 0x84, 0xa1, 0xf9, 0xd3, 0x94,
  0x49, 0xcb, 0x24, 0x62, 0x91, 0x20, 0xbc, 0xdd, 0x0b, 0xd5, 0xd9, 0xcc, 0xf9,
  0xea, 0x27, 0x0a, 0x2b, 0x73, 0x91, 0xc6, 0x9d, 0x1b, 0xac, 0xc8, 0xcb, 0xe8,
  0xe0, 0xa0, 0xf4, 0x2f, 0x90, 0x8b, 0x4d, 0xfb, 0xb0, 0x36, 0x1b, 0xf6, 0x19,
  0x7a, 0x85, 0xe0, 0x6d, 0xf2, 0x61, 0x13, 0x88, 0x5c, 0x9f, 0xe0, 0x93, 0x0a,
  0x51, 0x97, 0x8a, 0x5a, 0xce, 0xaf, 0xab, 0xd5, 0xf7, 0xaa, 0x09, 0xaa, 0x60,
  0xbd, 0xdc, 0xd9, 0x5f, 0xdf, 0x72, 0xa9, 0x60, 0x13, 0x5e, 0x00, 0x01, 0xc9,
  0x4a, 0xfa, 0x3f, 0xa4, 0xea, 0x07, 0x03, 0x21, 0x02, 0x8e, 0x82, 0xca, 0x03,
  0xc2, 0x9b, 0x8f, 0x02, 0x03, 0x01, 0x00, 0x01,
};

static const unsigned char sGlobalSignExtendedValidationCASHA256G2SubjectBytes[] = {
  0x30, 0x62, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02,
  0x42, 0x45, 0x31, 0x19, 0x30, 0x17, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x13, 0x10,
  0x47, 0x6c, 0x6f, 0x62, 0x61, 0x6c, 0x53, 0x69, 0x67, 0x6e, 0x20, 0x6e, 0x76,
  0x2d, 0x73, 0x61, 0x31, 0x38, 0x30, 0x36, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13,
  0x2f, 0x47, 0x6c, 0x6f, 0x62, 0x61, 0x6c, 0x53, 0x69, 0x67, 0x6e, 0x20, 0x45,
  0x78, 0x74, 0x65, 0x6e, 0x64, 0x65, 0x64, 0x20, 0x56, 0x61, 0x6c, 0x69, 0x64,
  0x61, 0x74, 0x69, 0x6f, 0x6e, 0x20, 0x43, 0x41, 0x20, 0x2d, 0x20, 0x53, 0x48,
  0x41, 0x32, 0x35, 0x36, 0x20, 0x2d, 0x20, 0x47, 0x32,
};

static const unsigned char sGlobalSignExtendedValidationCASHA256G2SPKIBytes[] = {
  0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7,
  0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00, 0x30, 0x82,
  0x01, 0x0a, 0x02, 0x82, 0x01, 0x01, 0x00, 0xa3, 0xea, 0xa1, 0xd2, 0xc3, 0x49,
  0xe5, 0xf7, 0x1c, 0x5d, 0xaf, 0xc3, 0x92, 0x42, 0xaf, 0x8a, 0x3c, 0xdc, 0xef,
  0x4c, 0xe6, 0x2f, 0x5f, 0x0c, 0x2b, 0x9f, 0x8a, 0x50, 0x30, 0x66, 0xef, 0x4e,
  0xc8, 0x4f, 0x21, 0x4a, 0xf6, 0xe7, 0xf2, 0x4e, 0x1b, 0x8c, 0x53, 0x57, 0xb0,
  0x9e, 0xc8, 0x5b, 0xf7, 0xb8, 0x46, 0x55, 0xb3, 0x1a, 0xed, 0xc2, 0x6a, 0xfe,
  0xf4, 0x1b, 0xec, 0x48, 0x46, 0x0e, 0x8f, 0xe0, 0xfb, 0xe0, 0x91, 0x19, 0xdf,
  0x99, 0x18, 0x6f, 0x2e, 0x51, 0xaf, 0xda, 0xf6, 0x9a, 0xca, 0x64, 0x6f, 0x99,
  0x54, 0x10, 0x74, 0xea, 0x3c, 0xc8, 0xaa, 0x80, 0x4d, 0x43, 0x37, 0xfb, 0xc8,
  0xa4, 0x7f, 0x05, 0x9d, 0x37, 0x92, 0xbd, 0x98, 0x00, 0x35, 0x5a, 0xaf, 0xbb,
  0x5b, 0x74, 0x15, 0x0e, 0xbc, 0xbc, 0xc6, 0xe9, 0xb7, 0x86, 0xe7, 0xee, 0xae,
  0x4d, 0x4b, 0x04, 0x4c, 0x2b, 0xa0, 0xb4, 0x65, 0x48, 0xb8, 0xc3, 0x3a, 0xcd,
  0x75, 0xbb, 0x37, 0xc9, 0x4a, 0xc0, 0x01, 0x11, 0xd9, 0xbf, 0x3f, 0x15, 0x86,
  0x60, 0x19, 0x6b, 0x34, 0x20, 0x46, 0xf5, 0x86, 0x66, 0x0f, 0x24, 0xf4, 0xcc,
  0x62, 0x9f, 0x9f, 0x9e, 0x1d, 0xfd, 0x10, 0xa4, 0x99, 0x5e, 0xf0, 0x41, 0xeb,
  0xb0, 0x94, 0xff, 0x2c, 0xb3, 0x36, 0xd6, 0xeb, 0x1d, 0xa7, 0x17, 0x5f, 0xdf,
  0xce, 0x6a, 0x77, 0xc7, 0x9a, 0xc4, 0x32, 0x63, 0xa7, 0x06, 0xad, 0xf3, 0x12,
  0x1b, 0x9d, 0x30, 0x72, 0x59, 0x0b, 0xeb, 0x72, 0xeb, 0x2a, 0xd2, 0x77, 0x7b,
  0x91, 0x77, 0xdb, 0x00, 0xfc, 0xd8, 0x6f, 0xf5, 0x2f, 0xd8, 0x7a, 0xc5, 0x0c,
  0x3a, 0xa0, 0x7b, 0x5e, 0x90, 0xf3, 0x9d, 0x84, 0x59, 0xc8, 0x01, 0xd9, 0x91,
  0x37, 0x56, 0xe5, 0x3a, 0x53, 0x93, 0xad, 0x60, 0x49, 0x27, 0x25, 0xd9, 0xe1,
  0xda, 0x82, 0xd7, 0x02, 0x03, 0x01, 0x00, 0x01,
};

template<size_t T, size_t R>
static bool
CertMatchesStaticData(const CERTCertificate* cert,
                      const unsigned char (&subject)[T],
                      const unsigned char (&spki)[R]) {
  MOZ_ASSERT(cert);
  if (!cert) {
    return false;
  }
  return cert->derSubject.len == T &&
         mozilla::PodEqual(cert->derSubject.data, subject, T) &&
         cert->derPublicKey.len == R &&
         mozilla::PodEqual(cert->derPublicKey.data, spki, R);
}

Result
NSSCertDBTrustDomain::IsChainValid(const DERArray& certArray, Time time,
                                   const CertPolicyId& requiredPolicy)
{
  MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
         ("NSSCertDBTrustDomain: IsChainValid"));

  UniqueCERTCertList certList;
  SECStatus srv = ConstructCERTCertListFromReversedDERArray(certArray,
                                                            certList);
  if (srv != SECSuccess) {
    return MapPRErrorCodeToResult(PR_GetError());
  }
  if (CERT_LIST_EMPTY(certList)) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  Result rv = CheckForStartComOrWoSign(certList);
  if (rv != Success) {
    return rv;
  }

  // If the certificate appears to have been issued by a CNNIC root, only allow
  // it if it is on the whitelist.
  CERTCertListNode* rootNode = CERT_LIST_TAIL(certList);
  if (!rootNode) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  CERTCertificate* root = rootNode->cert;
  if (!root) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  if ((root->derSubject.len == sizeof(CNNIC_ROOT_CA_SUBJECT_DATA) - 1 &&
       memcmp(root->derSubject.data, CNNIC_ROOT_CA_SUBJECT_DATA,
              root->derSubject.len) == 0) ||
      (root->derSubject.len == sizeof(CNNIC_EV_ROOT_CA_SUBJECT_DATA) - 1 &&
       memcmp(root->derSubject.data, CNNIC_EV_ROOT_CA_SUBJECT_DATA,
              root->derSubject.len) == 0)) {
    CERTCertListNode* certNode = CERT_LIST_HEAD(certList);
    if (!certNode) {
      return Result::FATAL_ERROR_LIBRARY_FAILURE;
    }
    CERTCertificate* cert = certNode->cert;
    if (!cert) {
      return Result::FATAL_ERROR_LIBRARY_FAILURE;
    }
    Digest digest;
    nsresult nsrv = digest.DigestBuf(SEC_OID_SHA256, cert->derCert.data,
                                     cert->derCert.len);
    if (NS_FAILED(nsrv)) {
      return Result::FATAL_ERROR_LIBRARY_FAILURE;
    }
    const uint8_t* certHash(
      BitwiseCast<uint8_t*, unsigned char*>(digest.get().data));
    size_t certHashLen = digest.get().len;
    size_t unused;
    if (!mozilla::BinarySearchIf(WhitelistedCNNICHashes, 0,
                                 ArrayLength(WhitelistedCNNICHashes),
                                 WhitelistedCNNICHashBinarySearchComparator(
                                   certHash, certHashLen),
                                 &unused)) {
      return Result::ERROR_REVOKED_CERTIFICATE;
    }
  }

  bool isBuiltInRoot = false;
  rv = IsCertBuiltInRoot(root, isBuiltInRoot);
  if (rv != Success) {
    return rv;
  }
  bool skipPinningChecksBecauseOfMITMMode =
    (!isBuiltInRoot && mPinningMode == CertVerifier::pinningAllowUserCAMITM);
  // If mHostname isn't set, we're not verifying in the context of a TLS
  // handshake, so don't verify HPKP in those cases.
  if (mHostname && (mPinningMode != CertVerifier::pinningDisabled) &&
      !skipPinningChecksBecauseOfMITMMode) {
    bool enforceTestMode =
      (mPinningMode == CertVerifier::pinningEnforceTestMode);
    bool chainHasValidPins;
    nsresult nsrv = PublicKeyPinningService::ChainHasValidPins(
      certList, mHostname, time, enforceTestMode, mOriginAttributes,
      chainHasValidPins, mPinningTelemetryInfo);
    if (NS_FAILED(nsrv)) {
      return Result::FATAL_ERROR_LIBRARY_FAILURE;
    }
    if (!chainHasValidPins) {
      return Result::ERROR_KEY_PINNING_FAILURE;
    }
  }

  // See bug 1349762. If the root is "GlobalSign Root CA - R2", don't consider
  // the end-entity valid for EV unless the
  // "GlobalSign Extended Validation CA - SHA256 - G2" intermediate is in the
  // chain as well. It should be possible to remove this workaround after
  // January 2019 as per bug 1349727 comment 17.
  if (requiredPolicy == sGlobalSignEVPolicy &&
      CertMatchesStaticData(root, sGlobalSignRootCAR2SubjectBytes,
                            sGlobalSignRootCAR2SPKIBytes)) {
    bool foundRequiredIntermediate = false;
    for (CERTCertListNode* node = CERT_LIST_HEAD(certList);
         !CERT_LIST_END(node, certList); node = CERT_LIST_NEXT(node)) {
      if (CertMatchesStaticData(
            node->cert,
            sGlobalSignExtendedValidationCASHA256G2SubjectBytes,
            sGlobalSignExtendedValidationCASHA256G2SPKIBytes)) {
        foundRequiredIntermediate = true;
        break;
      }
    }
    if (!foundRequiredIntermediate) {
      return Result::ERROR_POLICY_VALIDATION_FAILED;
    }
  }

  mBuiltChain = Move(certList);

  return Success;
}

Result
NSSCertDBTrustDomain::CheckSignatureDigestAlgorithm(DigestAlgorithm aAlg,
                                                    EndEntityOrCA endEntityOrCA,
                                                    Time notBefore)
{
  // (new Date("2016-01-01T00:00:00Z")).getTime() / 1000
  static const Time JANUARY_FIRST_2016 = TimeFromEpochInSeconds(1451606400);

  MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
          ("NSSCertDBTrustDomain: CheckSignatureDigestAlgorithm"));
  if (aAlg == DigestAlgorithm::sha1) {
    switch (mSHA1Mode) {
      case CertVerifier::SHA1Mode::Forbidden:
        MOZ_LOG(gCertVerifierLog, LogLevel::Debug, ("SHA-1 certificate rejected"));
        return Result::ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED;
      case CertVerifier::SHA1Mode::ImportedRootOrBefore2016:
        if (JANUARY_FIRST_2016 <= notBefore) {
          MOZ_LOG(gCertVerifierLog, LogLevel::Debug, ("Post-2015 SHA-1 certificate rejected"));
          return Result::ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED;
        }
        break;
      case CertVerifier::SHA1Mode::Allowed:
      // Enforcing that the resulting chain uses an imported root is only
      // possible at a higher level. This is done in CertVerifier::VerifyCert.
      case CertVerifier::SHA1Mode::ImportedRoot:
      default:
        break;
      // MSVC warns unless we explicitly handle this now-unused option.
      case CertVerifier::SHA1Mode::UsedToBeBefore2016ButNowIsForbidden:
        MOZ_ASSERT_UNREACHABLE("unexpected SHA1Mode type");
        return Result::FATAL_ERROR_LIBRARY_FAILURE;
    }
  }

  return Success;
}

Result
NSSCertDBTrustDomain::CheckRSAPublicKeyModulusSizeInBits(
  EndEntityOrCA /*endEntityOrCA*/, unsigned int modulusSizeInBits)
{
  if (modulusSizeInBits < mMinRSABits) {
    return Result::ERROR_INADEQUATE_KEY_SIZE;
  }
  return Success;
}

Result
NSSCertDBTrustDomain::VerifyRSAPKCS1SignedDigest(
  const SignedDigest& signedDigest,
  Input subjectPublicKeyInfo)
{
  return VerifyRSAPKCS1SignedDigestNSS(signedDigest, subjectPublicKeyInfo,
                                       mPinArg);
}

Result
NSSCertDBTrustDomain::CheckECDSACurveIsAcceptable(
  EndEntityOrCA /*endEntityOrCA*/, NamedCurve curve)
{
  switch (curve) {
    case NamedCurve::secp256r1: // fall through
    case NamedCurve::secp384r1: // fall through
    case NamedCurve::secp521r1:
      return Success;
  }

  return Result::ERROR_UNSUPPORTED_ELLIPTIC_CURVE;
}

Result
NSSCertDBTrustDomain::VerifyECDSASignedDigest(const SignedDigest& signedDigest,
                                              Input subjectPublicKeyInfo)
{
  return VerifyECDSASignedDigestNSS(signedDigest, subjectPublicKeyInfo,
                                    mPinArg);
}

Result
NSSCertDBTrustDomain::CheckValidityIsAcceptable(Time notBefore, Time notAfter,
                                                EndEntityOrCA endEntityOrCA,
                                                KeyPurposeId keyPurpose)
{
  if (endEntityOrCA != EndEntityOrCA::MustBeEndEntity) {
    return Success;
  }
  if (keyPurpose == KeyPurposeId::id_kp_OCSPSigning) {
    return Success;
  }

  Duration DURATION_27_MONTHS_PLUS_SLOP((2 * 365 + 3 * 31 + 7) *
                                        Time::ONE_DAY_IN_SECONDS);
  Duration maxValidityDuration(UINT64_MAX);
  Duration validityDuration(notBefore, notAfter);

  switch (mValidityCheckingMode) {
    case ValidityCheckingMode::CheckingOff:
      return Success;
    case ValidityCheckingMode::CheckForEV:
      // The EV Guidelines say the maximum is 27 months, but we use a slightly
      // higher limit here to (hopefully) minimize compatibility breakage.
      maxValidityDuration = DURATION_27_MONTHS_PLUS_SLOP;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("We're not handling every ValidityCheckingMode type");
  }

  if (validityDuration > maxValidityDuration) {
    return Result::ERROR_VALIDITY_TOO_LONG;
  }

  return Success;
}

Result
NSSCertDBTrustDomain::NetscapeStepUpMatchesServerAuth(Time notBefore,
                                                      /*out*/ bool& matches)
{
  // (new Date("2015-08-23T00:00:00Z")).getTime() / 1000
  static const Time AUGUST_23_2015 = TimeFromEpochInSeconds(1440288000);
  // (new Date("2016-08-23T00:00:00Z")).getTime() / 1000
  static const Time AUGUST_23_2016 = TimeFromEpochInSeconds(1471910400);

  switch (mNetscapeStepUpPolicy) {
    case NetscapeStepUpPolicy::AlwaysMatch:
      matches = true;
      return Success;
    case NetscapeStepUpPolicy::MatchBefore23August2016:
      matches = notBefore < AUGUST_23_2016;
      return Success;
    case NetscapeStepUpPolicy::MatchBefore23August2015:
      matches = notBefore < AUGUST_23_2015;
      return Success;
    case NetscapeStepUpPolicy::NeverMatch:
      matches = false;
      return Success;
    default:
      MOZ_ASSERT_UNREACHABLE("unhandled NetscapeStepUpPolicy type");
  }
  return Result::FATAL_ERROR_LIBRARY_FAILURE;
}

void
NSSCertDBTrustDomain::ResetAccumulatedState()
{
  mOCSPStaplingStatus = CertVerifier::OCSP_STAPLING_NEVER_CHECKED;
  mSCTListFromOCSPStapling = nullptr;
  mSCTListFromCertificate = nullptr;
}

static Input
SECItemToInput(const UniqueSECItem& item)
{
  Input result;
  if (item) {
    MOZ_ASSERT(item->type == siBuffer);
    Result rv = result.Init(item->data, item->len);
    // As used here, |item| originally comes from an Input,
    // so there should be no issues converting it back.
    MOZ_ASSERT(rv == Success);
    Unused << rv; // suppresses warnings in release builds
  }
  return result;
}

Input
NSSCertDBTrustDomain::GetSCTListFromCertificate() const
{
  return SECItemToInput(mSCTListFromCertificate);
}

Input
NSSCertDBTrustDomain::GetSCTListFromOCSPStapling() const
{
  return SECItemToInput(mSCTListFromOCSPStapling);
}

void
NSSCertDBTrustDomain::NoteAuxiliaryExtension(AuxiliaryExtension extension,
                                             Input extensionData)
{
  UniqueSECItem* out = nullptr;
  switch (extension) {
    case AuxiliaryExtension::EmbeddedSCTList:
      out = &mSCTListFromCertificate;
      break;
    case AuxiliaryExtension::SCTListFromOCSPResponse:
      out = &mSCTListFromOCSPStapling;
      break;
    default:
      MOZ_ASSERT_UNREACHABLE("unhandled AuxiliaryExtension");
  }
  if (out) {
    SECItem extensionDataItem = UnsafeMapInputToSECItem(extensionData);
    out->reset(SECITEM_DupItem(&extensionDataItem));
  }
}

SECStatus
InitializeNSS(const char* dir, bool readOnly, bool loadPKCS11Modules)
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
  if (!loadPKCS11Modules) {
    flags |= NSS_INIT_NOMODDB;
  }
  MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
          ("InitializeNSS(%s, %d, %d)", dir, readOnly, loadPKCS11Modules));
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

bool
LoadLoadableRoots(const nsCString& dir, const nsCString& modNameUTF8)
{
  UniquePRLibraryName fullLibraryPath(
    PR_GetLibraryName(dir.IsEmpty() ? nullptr : dir.get(), "nssckbi"));
  if (!fullLibraryPath) {
    return false;
  }

  // Escape the \ and " characters.
  nsAutoCString escapedFullLibraryPath(fullLibraryPath.get());
  escapedFullLibraryPath.ReplaceSubstring("\\", "\\\\");
  escapedFullLibraryPath.ReplaceSubstring("\"", "\\\"");
  if (escapedFullLibraryPath.IsEmpty()) {
    return false;
  }

  // If a module exists with the same name, make a best effort attempt to delete
  // it. Note that it isn't possible to delete the internal module, so checking
  // the return value would be detrimental in that case.
  int unusedModType;
  Unused << SECMOD_DeleteModule(modNameUTF8.get(), &unusedModType);

  nsAutoCString pkcs11ModuleSpec;
  pkcs11ModuleSpec.AppendPrintf("name=\"%s\" library=\"%s\"", modNameUTF8.get(),
                                escapedFullLibraryPath.get());
  if (pkcs11ModuleSpec.IsEmpty()) {
    return false;
  }

  UniqueSECMODModule rootsModule(
    SECMOD_LoadUserModule(const_cast<char*>(pkcs11ModuleSpec.get()), nullptr,
                          false));
  if (!rootsModule) {
    return false;
  }

  if (!rootsModule->loaded) {
    return false;
  }

  return true;
}

void
UnloadLoadableRoots(const char* modNameUTF8)
{
  MOZ_ASSERT(modNameUTF8);
  UniqueSECMODModule rootsModule(SECMOD_FindModule(modNameUTF8));

  if (rootsModule) {
    SECMOD_UnloadUserModule(rootsModule.get());
  }
}

nsresult
DefaultServerNicknameForCert(const CERTCertificate* cert,
                     /*out*/ nsCString& nickname)
{
  MOZ_ASSERT(cert);
  NS_ENSURE_ARG_POINTER(cert);

  UniquePORTString baseName(CERT_GetCommonName(&cert->subject));
  if (!baseName) {
    baseName = UniquePORTString(CERT_GetOrgUnitName(&cert->subject));
  }
  if (!baseName) {
    baseName = UniquePORTString(CERT_GetOrgName(&cert->subject));
  }
  if (!baseName) {
    baseName = UniquePORTString(CERT_GetLocalityName(&cert->subject));
  }
  if (!baseName) {
    baseName = UniquePORTString(CERT_GetStateName(&cert->subject));
  }
  if (!baseName) {
    baseName = UniquePORTString(CERT_GetCountryName(&cert->subject));
  }
  if (!baseName) {
    return NS_ERROR_FAILURE;
  }

  // This function is only used in contexts where a failure to find a suitable
  // nickname does not block the overall task from succeeding.
  // As such, we use an arbitrary limit to prevent this nickname searching
  // process from taking forever.
  static const uint32_t ARBITRARY_LIMIT = 500;
  for (uint32_t count = 1; count < ARBITRARY_LIMIT; count++) {
    nickname = baseName.get();
    if (count != 1) {
      nickname.AppendPrintf(" #%u", count);
    }
    if (nickname.IsEmpty()) {
      return NS_ERROR_FAILURE;
    }

    bool conflict = SEC_CertNicknameConflict(nickname.get(), &cert->derSubject,
                                             cert->dbhandle);
    if (!conflict) {
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

/**
 * Given a list of certificates representing a verified certificate path from an
 * end-entity certificate to a trust anchor, imports the intermediate
 * certificates into the permanent certificate database. This is an attempt to
 * cope with misconfigured servers that don't include the appropriate
 * intermediate certificates in the TLS handshake.
 *
 * @param certList the verified certificate list
 */
void
SaveIntermediateCerts(const UniqueCERTCertList& certList)
{
  if (!certList) {
    return;
  }

  UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
  if (!slot) {
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
      // This cert was found on a token; no need to remember it in the permanent
      // database.
      continue;
    }

    if (node->cert->isperm) {
      // We don't need to remember certs already stored in perm db.
      continue;
    }

    // No need to save the trust anchor - it's either already a permanent
    // certificate or it's the Microsoft Family Safety root or an enterprise
    // root temporarily imported via the child mode or enterprise root features.
    // We don't want to import these because they're intended to be temporary
    // (and because importing them happens to reset their trust settings, which
    // breaks these features).
    if (node == CERT_LIST_TAIL(certList)) {
      continue;
    }

    nsAutoCString nickname;
    nsresult rv = DefaultServerNicknameForCert(node->cert, nickname);
    if (NS_FAILED(rv)) {
      continue;
    }

    // As mentioned in the documentation of this function, we're importing only
    // to cope with misconfigured servers. As such, we ignore the return value
    // below, since it doesn't really matter if the import fails.
    Unused << PK11_ImportCert(slot.get(), node->cert, CK_INVALID_HANDLE,
                              nickname.get(), false);
  }
}

} } // namespace mozilla::psm
