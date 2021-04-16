/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NSSCertDBTrustDomain.h"

#include <stdint.h>
#include <utility>

#include "ExtendedValidation.h"
#include "MultiLogCTVerifier.h"
#include "NSSErrorsService.h"
#include "OCSPVerificationTrustDomain.h"
#include "PublicKeyPinningService.h"
#include "cert.h"
#include "cert_storage/src/cert_storage.h"
#include "certdb.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Services.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Unused.h"
#include "mozpkix/Result.h"
#include "mozpkix/pkix.h"
#include "mozpkix/pkixnss.h"
#include "mozpkix/pkixutil.h"
#include "nsCRTGlue.h"
#include "nsIObserverService.h"
#include "nsNSSCertHelper.h"
#include "nsNSSCertificate.h"
#include "nsNSSCertificateDB.h"
#include "nsNetCID.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nss.h"
#include "pk11pub.h"
#include "prerror.h"
#include "secder.h"
#include "secerr.h"

#ifdef MOZ_WIDGET_COCOA
#  include "nsCocoaFeatures.h"
#endif

#include "TrustOverrideUtils.h"
#include "TrustOverride-AppleGoogleDigiCertData.inc"
#include "TrustOverride-StartComAndWoSignData.inc"
#include "TrustOverride-SymantecData.inc"

using namespace mozilla;
using namespace mozilla::ct;
using namespace mozilla::pkix;

extern LazyLogModule gCertVerifierLog;

static const uint64_t ServerFailureDelaySeconds = 5 * 60;

namespace mozilla {
namespace psm {

NSSCertDBTrustDomain::NSSCertDBTrustDomain(
    SECTrustType certDBTrustType, OCSPFetching ocspFetching,
    OCSPCache& ocspCache,
    /*optional but shouldn't be*/ void* pinArg, TimeDuration ocspTimeoutSoft,
    TimeDuration ocspTimeoutHard, uint32_t certShortLifetimeInDays,
    CertVerifier::PinningMode pinningMode, unsigned int minRSABits,
    ValidityCheckingMode validityCheckingMode, CertVerifier::SHA1Mode sha1Mode,
    NetscapeStepUpPolicy netscapeStepUpPolicy, CRLiteMode crliteMode,
    uint64_t crliteCTMergeDelaySeconds,
    const OriginAttributes& originAttributes,
    const Vector<Input>& thirdPartyRootInputs,
    const Vector<Input>& thirdPartyIntermediateInputs,
    const Maybe<nsTArray<nsTArray<uint8_t>>>& extraCertificates,
    /*out*/ UniqueCERTCertList& builtChain,
    /*optional*/ PinningTelemetryInfo* pinningTelemetryInfo,
    /*optional*/ CRLiteLookupResult* crliteLookupResult,
    /*optional*/ const char* hostname)
    : mCertDBTrustType(certDBTrustType),
      mOCSPFetching(ocspFetching),
      mOCSPCache(ocspCache),
      mPinArg(pinArg),
      mOCSPTimeoutSoft(ocspTimeoutSoft),
      mOCSPTimeoutHard(ocspTimeoutHard),
      mCertShortLifetimeInDays(certShortLifetimeInDays),
      mPinningMode(pinningMode),
      mMinRSABits(minRSABits),
      mValidityCheckingMode(validityCheckingMode),
      mSHA1Mode(sha1Mode),
      mNetscapeStepUpPolicy(netscapeStepUpPolicy),
      mCRLiteMode(crliteMode),
      mCRLiteCTMergeDelaySeconds(crliteCTMergeDelaySeconds),
      mSawDistrustedCAByPolicyError(false),
      mOriginAttributes(originAttributes),
      mThirdPartyRootInputs(thirdPartyRootInputs),
      mThirdPartyIntermediateInputs(thirdPartyIntermediateInputs),
      mExtraCertificates(extraCertificates),
      mBuiltChain(builtChain),
      mPinningTelemetryInfo(pinningTelemetryInfo),
      mCRLiteLookupResult(crliteLookupResult),
      mHostname(hostname),
      mCertStorage(do_GetService(NS_CERT_STORAGE_CID)),
      mOCSPStaplingStatus(CertVerifier::OCSP_STAPLING_NEVER_CHECKED),
      mSCTListFromCertificate(),
      mSCTListFromOCSPStapling(),
      mBuiltInRootsModule(SECMOD_FindModule(kRootModuleName)) {}

static void FindRootsWithSubject(UniqueSECMODModule& rootsModule,
                                 SECItem subject,
                                 /*out*/ nsTArray<nsTArray<uint8_t>>& roots) {
  MOZ_ASSERT(rootsModule);
  for (int slotIndex = 0; slotIndex < rootsModule->slotCount; slotIndex++) {
    CERTCertificateList* rawResults = nullptr;
    if (PK11_FindRawCertsWithSubject(rootsModule->slots[slotIndex], &subject,
                                     &rawResults) != SECSuccess) {
      continue;
    }
    // rawResults == nullptr means we didn't find any matching certificates
    if (!rawResults) {
      continue;
    }
    UniqueCERTCertificateList results(rawResults);
    for (int certIndex = 0; certIndex < results->len; certIndex++) {
      nsTArray<uint8_t> root;
      root.AppendElements(results->certs[certIndex].data,
                          results->certs[certIndex].len);
      roots.AppendElement(std::move(root));
    }
  }
}

// A self-signed issuer certificate should never be necessary in order to build
// a trusted certificate chain unless it is a trust anchor. This is because if
// it were necessary, there would exist another certificate with the same
// subject and public key that is also a valid issing certificate. Given this
// certificate, it is possible to build another chain using just it instead of
// it and the self-signed certificate. This is only true as long as the
// certificate extensions we support are restrictive rather than additive in
// terms of the rest of the chain (for example, we don't support policy mapping
// and we ignore any SCT information in intermediates).
static bool ShouldSkipSelfSignedNonTrustAnchor(TrustDomain& trustDomain,
                                               Input certDER) {
  BackCert cert(certDER, EndEntityOrCA::MustBeCA, nullptr);
  if (cert.Init() != Success) {
    return false;  // turn any failures into "don't skip trying this cert"
  }
  // If subject != issuer, this isn't a self-signed cert.
  if (!InputsAreEqual(cert.GetSubject(), cert.GetIssuer())) {
    return false;
  }
  TrustLevel trust;
  if (trustDomain.GetCertTrust(EndEntityOrCA::MustBeCA, CertPolicyId::anyPolicy,
                               certDER, trust) != Success) {
    return false;
  }
  // If the trust for this certificate is anything other than "inherit", we want
  // to process it like normal.
  if (trust != TrustLevel::InheritsTrust) {
    return false;
  }
  uint8_t digestBuf[MAX_DIGEST_SIZE_IN_BYTES];
  pkix::der::PublicKeyAlgorithm publicKeyAlg;
  SignedDigest signature;
  if (DigestSignedData(trustDomain, cert.GetSignedData(), digestBuf,
                       publicKeyAlg, signature) != Success) {
    return false;
  }
  if (VerifySignedDigest(trustDomain, publicKeyAlg, signature,
                         cert.GetSubjectPublicKeyInfo()) != Success) {
    return false;
  }
  // This is a self-signed, non-trust-anchor certificate, so we shouldn't use it
  // for path building. See bug 1056341.
  return true;
}

static Result CheckCandidates(TrustDomain& trustDomain,
                              TrustDomain::IssuerChecker& checker,
                              nsTArray<Input>& candidates,
                              Input* nameConstraintsInputPtr, bool& keepGoing) {
  for (Input candidate : candidates) {
    if (ShouldSkipSelfSignedNonTrustAnchor(trustDomain, candidate)) {
      continue;
    }
    Result rv = checker.Check(candidate, nameConstraintsInputPtr, keepGoing);
    if (rv != Success) {
      return rv;
    }
    if (!keepGoing) {
      return Success;
    }
  }

  return Success;
}

Result NSSCertDBTrustDomain::FindIssuer(Input encodedIssuerName,
                                        IssuerChecker& checker, Time) {
  SECItem encodedIssuerNameItem = UnsafeMapInputToSECItem(encodedIssuerName);
  // Handle imposed name constraints, if any.
  ScopedAutoSECItem nameConstraints;
  Input nameConstraintsInput;
  Input* nameConstraintsInputPtr = nullptr;
  SECStatus srv =
      CERT_GetImposedNameConstraints(&encodedIssuerNameItem, &nameConstraints);
  if (srv == SECSuccess) {
    if (nameConstraintsInput.Init(nameConstraints.data, nameConstraints.len) !=
        Success) {
      return Result::FATAL_ERROR_LIBRARY_FAILURE;
    }
    nameConstraintsInputPtr = &nameConstraintsInput;
  } else if (PR_GetError() != SEC_ERROR_EXTENSION_NOT_FOUND) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  // First try all relevant certificates known to Gecko, which avoids calling
  // CERT_CreateSubjectCertList, because that can be expensive.
  nsTArray<Input> geckoRootCandidates;
  nsTArray<Input> geckoIntermediateCandidates;

  if (!mCertStorage) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  nsTArray<uint8_t> subject;
  subject.AppendElements(encodedIssuerName.UnsafeGetData(),
                         encodedIssuerName.GetLength());
  nsTArray<nsTArray<uint8_t>> certs;
  nsresult rv = mCertStorage->FindCertsBySubject(subject, certs);
  if (NS_FAILED(rv)) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  for (auto& cert : certs) {
    Input certDER;
    Result rv = certDER.Init(cert.Elements(), cert.Length());
    if (rv != Success) {
      continue;  // probably too big
    }
    // Currently we're only expecting intermediate certificates in cert storage.
    geckoIntermediateCandidates.AppendElement(std::move(certDER));
  }

  // We might not have this module if e.g. we're on a Linux distribution that
  // does something unexpected.
  nsTArray<nsTArray<uint8_t>> builtInRoots;
  if (mBuiltInRootsModule) {
    FindRootsWithSubject(mBuiltInRootsModule, encodedIssuerNameItem,
                         builtInRoots);
    for (const auto& root : builtInRoots) {
      Input rootInput;
      Result rv = rootInput.Init(root.Elements(), root.Length());
      if (rv != Success) {
        continue;  // probably too big
      }
      geckoRootCandidates.AppendElement(rootInput);
    }
  } else {
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
            ("NSSCertDBTrustDomain::FindIssuer: no built-in roots module"));
  }

  for (const auto& thirdPartyRootInput : mThirdPartyRootInputs) {
    BackCert root(thirdPartyRootInput, EndEntityOrCA::MustBeCA, nullptr);
    Result rv = root.Init();
    if (rv != Success) {
      continue;
    }
    // Filter out 3rd party roots that can't be issuers we're looking for
    // because the subject distinguished name doesn't match. This prevents
    // mozilla::pkix from accumulating spurious errors during path building.
    if (!InputsAreEqual(encodedIssuerName, root.GetSubject())) {
      continue;
    }
    geckoRootCandidates.AppendElement(thirdPartyRootInput);
  }

  for (const auto& thirdPartyIntermediateInput :
       mThirdPartyIntermediateInputs) {
    BackCert intermediate(thirdPartyIntermediateInput, EndEntityOrCA::MustBeCA,
                          nullptr);
    Result rv = intermediate.Init();
    if (rv != Success) {
      continue;
    }
    // Filter out 3rd party intermediates that can't be issuers we're looking
    // for because the subject distinguished name doesn't match. This prevents
    // mozilla::pkix from accumulating spurious errors during path building.
    if (!InputsAreEqual(encodedIssuerName, intermediate.GetSubject())) {
      continue;
    }
    geckoIntermediateCandidates.AppendElement(thirdPartyIntermediateInput);
  }

  if (mExtraCertificates.isSome()) {
    for (const auto& extraCert : *mExtraCertificates) {
      Input certInput;
      Result rv = certInput.Init(extraCert.Elements(), extraCert.Length());
      if (rv != Success) {
        continue;
      }
      BackCert cert(certInput, EndEntityOrCA::MustBeCA, nullptr);
      rv = cert.Init();
      if (rv != Success) {
        continue;
      }
      // Filter out certificates that can't be issuers we're looking for because
      // the subject distinguished name doesn't match. This prevents
      // mozilla::pkix from accumulating spurious errors during path building.
      if (!InputsAreEqual(encodedIssuerName, cert.GetSubject())) {
        continue;
      }
      // We assume that extra certificates (presumably from the TLS handshake)
      // are intermediates, since sending trust anchors would be superfluous.
      geckoIntermediateCandidates.AppendElement(certInput);
    }
  }

  // Try all root certs first and then all (presumably) intermediates.
  geckoRootCandidates.AppendElements(std::move(geckoIntermediateCandidates));

  bool keepGoing = true;
  Result result = CheckCandidates(*this, checker, geckoRootCandidates,
                                  nameConstraintsInputPtr, keepGoing);
  if (result != Success) {
    return result;
  }
  if (!keepGoing) {
    return Success;
  }

  // Synchronously dispatch a task to the socket thread to find CERTCertificates
  // with the given subject. This involves querying NSS structures and
  // databases, so it must be done on the socket thread.
  nsTArray<nsTArray<uint8_t>> nssRootCandidates;
  nsTArray<nsTArray<uint8_t>> nssIntermediateCandidates;
  RefPtr<Runnable> getCandidatesTask =
      NS_NewRunnableFunction("NSSCertDBTrustDomain::FindIssuer", [&]() {
        // NSS seems not to differentiate between "no potential issuers found"
        // and "there was an error trying to retrieve the potential issuers." We
        // assume there was no error if CERT_CreateSubjectCertList returns
        // nullptr.
        UniqueCERTCertList candidates(
            CERT_CreateSubjectCertList(nullptr, CERT_GetDefaultCertDB(),
                                       &encodedIssuerNameItem, 0, false));
        if (candidates) {
          for (CERTCertListNode* n = CERT_LIST_HEAD(candidates);
               !CERT_LIST_END(n, candidates); n = CERT_LIST_NEXT(n)) {
            nsTArray<uint8_t> candidate;
            candidate.AppendElements(n->cert->derCert.data,
                                     n->cert->derCert.len);
            if (n->cert->isRoot) {
              nssRootCandidates.AppendElement(std::move(candidate));
            } else {
              nssIntermediateCandidates.AppendElement(std::move(candidate));
            }
          }
        }
      });
  nsCOMPtr<nsIEventTarget> socketThread(
      do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID));
  if (!socketThread) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  rv = SyncRunnable::DispatchToThread(socketThread, getCandidatesTask);
  if (NS_FAILED(rv)) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  nsTArray<Input> nssCandidates;
  for (const auto& rootCandidate : nssRootCandidates) {
    Input certDER;
    Result rv = certDER.Init(rootCandidate.Elements(), rootCandidate.Length());
    if (rv != Success) {
      continue;  // probably too big
    }
    nssCandidates.AppendElement(std::move(certDER));
  }
  for (const auto& intermediateCandidate : nssIntermediateCandidates) {
    Input certDER;
    Result rv = certDER.Init(intermediateCandidate.Elements(),
                             intermediateCandidate.Length());
    if (rv != Success) {
      continue;  // probably too big
    }
    nssCandidates.AppendElement(std::move(certDER));
  }

  return CheckCandidates(*this, checker, nssCandidates, nameConstraintsInputPtr,
                         keepGoing);
}

Result NSSCertDBTrustDomain::GetCertTrust(EndEntityOrCA endEntityOrCA,
                                          const CertPolicyId& policy,
                                          Input candidateCertDER,
                                          /*out*/ TrustLevel& trustLevel) {
  // Check the certificate against the OneCRL cert blocklist
  if (!mCertStorage) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  // The certificate blocklist currently only applies to TLS server
  // certificates.
  if (mCertDBTrustType == trustSSL) {
    int16_t revocationState;

    nsTArray<uint8_t> issuerBytes;
    nsTArray<uint8_t> serialBytes;
    nsTArray<uint8_t> subjectBytes;
    nsTArray<uint8_t> pubKeyBytes;

    Result result =
        BuildRevocationCheckArrays(candidateCertDER, endEntityOrCA, issuerBytes,
                                   serialBytes, subjectBytes, pubKeyBytes);
    if (result != Success) {
      return result;
    }

    nsresult nsrv = mCertStorage->GetRevocationState(
        issuerBytes, serialBytes, subjectBytes, pubKeyBytes, &revocationState);
    if (NS_FAILED(nsrv)) {
      return Result::FATAL_ERROR_LIBRARY_FAILURE;
    }

    if (revocationState == nsICertStorage::STATE_ENFORCE) {
      MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
              ("NSSCertDBTrustDomain: certificate is in blocklist"));
      return Result::ERROR_REVOKED_CERTIFICATE;
    }
  }

  // This may be a third-party root.
  for (const auto& thirdPartyRootInput : mThirdPartyRootInputs) {
    if (InputsAreEqual(candidateCertDER, thirdPartyRootInput)) {
      trustLevel = TrustLevel::TrustAnchor;
      return Success;
    }
  }

  // This may be a third-party intermediate.
  for (const auto& thirdPartyIntermediateInput :
       mThirdPartyIntermediateInputs) {
    if (InputsAreEqual(candidateCertDER, thirdPartyIntermediateInput)) {
      trustLevel = TrustLevel::InheritsTrust;
      return Success;
    }
  }

  // Synchronously dispatch a task to the socket thread to construct a
  // CERTCertificate and get its trust from NSS. This involves querying NSS
  // structures and databases, so it must be done on the socket thread.
  Result result = Result::FATAL_ERROR_LIBRARY_FAILURE;
  RefPtr<Runnable> getTrustTask =
      NS_NewRunnableFunction("NSSCertDBTrustDomain::GetCertTrust", [&]() {
        // This would be cleaner and more efficient if we could get the trust
        // information without constructing a CERTCertificate here, but NSS
        // doesn't expose it in any other easy-to-use fashion. The use of
        // CERT_NewTempCertificate to get a CERTCertificate shouldn't be a
        // performance problem for certificates already known to NSS because NSS
        // will just find the existing CERTCertificate in its in-memory cache
        // and return it. For certificates not already in NSS (namely
        // third-party roots and intermediates), we want to avoid calling
        // CERT_NewTempCertificate repeatedly, so we've already checked if the
        // candidate certificate is a third-party certificate, above.
        SECItem candidateCertDERSECItem =
            UnsafeMapInputToSECItem(candidateCertDER);
        UniqueCERTCertificate candidateCert(CERT_NewTempCertificate(
            CERT_GetDefaultCertDB(), &candidateCertDERSECItem, nullptr, false,
            true));
        if (!candidateCert) {
          result = MapPRErrorCodeToResult(PR_GetError());
          return;
        }
        // NB: CERT_GetCertTrust seems to be abusing SECStatus as a boolean,
        // where SECSuccess means that there is a trust record and SECFailure
        // means there is not a trust record. I looked at NSS's internal uses of
        // CERT_GetCertTrust, and all that code uses the result as a boolean
        // meaning "We have a trust record."
        CERTCertTrust trust;
        if (CERT_GetCertTrust(candidateCert.get(), &trust) == SECSuccess) {
          uint32_t flags = SEC_GET_TRUST_FLAGS(&trust, mCertDBTrustType);

          // For DISTRUST, we use the CERTDB_TRUSTED or CERTDB_TRUSTED_CA bit,
          // because we can have active distrust for either type of cert. Note
          // that CERTDB_TERMINAL_RECORD means "stop trying to inherit trust" so
          // if the relevant trust bit isn't set then that means the cert must
          // be considered distrusted.
          uint32_t relevantTrustBit = endEntityOrCA == EndEntityOrCA::MustBeCA
                                          ? CERTDB_TRUSTED_CA
                                          : CERTDB_TRUSTED;
          if (((flags & (relevantTrustBit | CERTDB_TERMINAL_RECORD))) ==
              CERTDB_TERMINAL_RECORD) {
            trustLevel = TrustLevel::ActivelyDistrusted;
            result = Success;
            return;
          }

          // For TRUST, we use the CERTDB_TRUSTED_CA bit.
          if (flags & CERTDB_TRUSTED_CA) {
            if (policy.IsAnyPolicy()) {
              trustLevel = TrustLevel::TrustAnchor;
              result = Success;
              return;
            }

            nsTArray<uint8_t> certBytes(candidateCert->derCert.data,
                                        candidateCert->derCert.len);
            if (CertIsAuthoritativeForEVPolicy(certBytes, policy)) {
              trustLevel = TrustLevel::TrustAnchor;
              result = Success;
              return;
            }
          }
        }
        trustLevel = TrustLevel::InheritsTrust;
        result = Success;
      });
  nsCOMPtr<nsIEventTarget> socketThread(
      do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID));
  if (!socketThread) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  nsresult rv = SyncRunnable::DispatchToThread(socketThread, getTrustTask);
  if (NS_FAILED(rv)) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  return result;
}

Result NSSCertDBTrustDomain::DigestBuf(Input item, DigestAlgorithm digestAlg,
                                       /*out*/ uint8_t* digestBuf,
                                       size_t digestBufLen) {
  return DigestBufNSS(item, digestAlg, digestBuf, digestBufLen);
}

TimeDuration NSSCertDBTrustDomain::GetOCSPTimeout() const {
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
// Success with result.IsVoid() == true when an OCSP URI was not found, and
// Success with result.IsVoid() == false when an OCSP URI was found.
static Result GetOCSPAuthorityInfoAccessLocation(const UniquePLArenaPool& arena,
                                                 Input aiaExtension,
                                                 /*out*/ nsCString& result) {
  MOZ_ASSERT(arena.get());
  if (!arena.get()) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }

  result.Assign(VoidCString());
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
          result.Assign(nsDependentCSubstring(
              reinterpret_cast<const char*>(location.data), location.len));
          return Success;
        }
        current = CERT_GetNextGeneralName(current);
      } while (current != aia[i]->location);
    }
  }

  return Success;
}

Result GetEarliestSCTTimestamp(Input sctExtension,
                               Maybe<uint64_t>& earliestTimestamp) {
  earliestTimestamp.reset();

  Input sctList;
  Result rv =
      ExtractSignedCertificateTimestampListFromExtension(sctExtension, sctList);
  if (rv != Success) {
    return rv;
  }
  std::vector<SignedCertificateTimestamp> decodedSCTs;
  size_t decodingErrors;
  DecodeSCTs(sctList, decodedSCTs, decodingErrors);
  Unused << decodingErrors;
  for (const auto& scts : decodedSCTs) {
    if (!earliestTimestamp.isSome() || scts.timestamp < *earliestTimestamp) {
      earliestTimestamp = Some(scts.timestamp);
    }
  }
  return Success;
}

Result NSSCertDBTrustDomain::CheckRevocation(
    EndEntityOrCA endEntityOrCA, const CertID& certID, Time time,
    Duration validityDuration,
    /*optional*/ const Input* stapledOCSPResponse,
    /*optional*/ const Input* aiaExtension,
    /*optional*/ const Input* sctExtension) {
  // Actively distrusted certificates will have already been blocked by
  // GetCertTrust.

  // TODO: need to verify that IsRevoked isn't called for trust anchors AND
  // that that fact is documented in mozillapkix.

  MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
          ("NSSCertDBTrustDomain: Top of CheckRevocation\n"));

  Maybe<uint64_t> earliestSCTTimestamp = Nothing();
  if (sctExtension) {
    Result rv = GetEarliestSCTTimestamp(*sctExtension, earliestSCTTimestamp);
    if (rv != Success) {
      MOZ_LOG(
          gCertVerifierLog, LogLevel::Debug,
          ("decoding SCT extension failed - CRLite will be not be consulted"));
    }
  }

  if (endEntityOrCA == EndEntityOrCA::MustBeEndEntity &&
      mCRLiteMode != CRLiteMode::Disabled && earliestSCTTimestamp.isSome()) {
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
            ("NSSCertDBTrustDomain::CheckRevocation: checking CRLite"));
    nsTArray<uint8_t> issuerBytes;
    issuerBytes.AppendElements(certID.issuer.UnsafeGetData(),
                               certID.issuer.GetLength());
    nsTArray<uint8_t> issuerSubjectPublicKeyInfoBytes;
    issuerSubjectPublicKeyInfoBytes.AppendElements(
        certID.issuerSubjectPublicKeyInfo.UnsafeGetData(),
        certID.issuerSubjectPublicKeyInfo.GetLength());
    nsTArray<uint8_t> serialNumberBytes;
    serialNumberBytes.AppendElements(certID.serialNumber.UnsafeGetData(),
                                     certID.serialNumber.GetLength());
    uint64_t filterTimestamp;
    int16_t crliteRevocationState;
    nsresult rv = mCertStorage->GetCRLiteRevocationState(
        issuerBytes, issuerSubjectPublicKeyInfoBytes, serialNumberBytes,
        &filterTimestamp, &crliteRevocationState);
    bool certificateFoundValidInCRLiteFilter = false;
    if (NS_FAILED(rv)) {
      MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
              ("NSSCertDBTrustDomain::CheckRevocation: CRLite call failed"));
      if (mCRLiteLookupResult) {
        *mCRLiteLookupResult = CRLiteLookupResult::LibraryFailure;
      }
      if (mCRLiteMode == CRLiteMode::Enforce) {
        return Result::FATAL_ERROR_LIBRARY_FAILURE;
      }
    } else {
      MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
              ("NSSCertDBTrustDomain::CheckRevocation: CRLite check returned "
               "state=%hd filter timestamp=%llu",
               crliteRevocationState,
               // The cast is to silence warnings on compilers where uint64_t is
               // an unsigned long as opposed to an unsigned long long.
               static_cast<unsigned long long>(filterTimestamp)));
      Time filterTimestampTime(TimeFromEpochInSeconds(filterTimestamp));
      // We can only use this result if the earliest embedded signed
      // certificate timestamp from the certificate is older than what cert
      // storage returned for its CRLite timestamp. Otherwise, the CRLite
      // filter cascade may have been created before this certificate existed,
      // and if it would create a false positive, it hasn't been accounted for.
      // SCT timestamps are milliseconds since the epoch.
      Time earliestCertificateTimestamp(
          TimeFromEpochInSeconds(*earliestSCTTimestamp / 1000));
      Result result =
          earliestCertificateTimestamp.AddSeconds(mCRLiteCTMergeDelaySeconds);
      if (result != Success) {
        // This shouldn't happen - the merge delay is at most a year in seconds,
        // and the SCT timestamp is supposed to be in the past.
        MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
                ("NSSCertDBTrustDomain::CheckRevocation: integer overflow "
                 "calculating sct timestamp + merge delay (%llu + %llu)",
                 static_cast<unsigned long long>(*earliestSCTTimestamp / 1000),
                 static_cast<unsigned long long>(mCRLiteCTMergeDelaySeconds)));
        if (mCRLiteMode == CRLiteMode::Enforce) {
          // While we do have control over the possible values of the CT merge
          // delay parameter, we don't have control over the SCT timestamp.
          // Thus, if we've reached this point, the CA has probably made a
          // mistake and we should treat this certificate as revoked.
          return Result::ERROR_REVOKED_CERTIFICATE;
        }
        // If Time::AddSeconds fails, the original value is unchanged. Since in
        // this case `earliestCertificateTimestamp` must represent a value far
        // in the future, any CRLite result will be discarded.
      }
      if (earliestCertificateTimestamp <= filterTimestampTime &&
          crliteRevocationState == nsICertStorage::STATE_ENFORCE) {
        if (mCRLiteLookupResult) {
          *mCRLiteLookupResult = CRLiteLookupResult::CertificateRevoked;
        }
        if (mCRLiteMode == CRLiteMode::Enforce) {
          MOZ_LOG(
              gCertVerifierLog, LogLevel::Debug,
              ("NSSCertDBTrustDomain::CheckRevocation: certificate revoked via "
               "CRLite"));
          return Result::ERROR_REVOKED_CERTIFICATE;
        }
        MOZ_LOG(
            gCertVerifierLog, LogLevel::Debug,
            ("NSSCertDBTrustDomain::CheckRevocation: certificate revoked via "
             "CRLite (not enforced - telemetry only)"));
      }

      if (crliteRevocationState == nsICertStorage::STATE_NOT_ENROLLED) {
        if (mCRLiteLookupResult) {
          *mCRLiteLookupResult = CRLiteLookupResult::IssuerNotEnrolled;
        }
        MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
                ("NSSCertDBTrustDomain::CheckRevocation: issuer not enrolled"));
      }
      if (filterTimestamp == 0) {
        MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
                ("NSSCertDBTrustDomain::CheckRevocation: no timestamp"));
        if (mCRLiteLookupResult) {
          *mCRLiteLookupResult = CRLiteLookupResult::FilterNotAvailable;
        }
      } else if (earliestCertificateTimestamp > filterTimestampTime) {
        MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
                ("NSSCertDBTrustDomain::CheckRevocation: cert too new"));
        if (mCRLiteLookupResult) {
          *mCRLiteLookupResult = CRLiteLookupResult::CertificateTooNew;
        }
      } else if (crliteRevocationState == nsICertStorage::STATE_UNSET) {
        certificateFoundValidInCRLiteFilter = true;
        if (mCRLiteLookupResult) {
          *mCRLiteLookupResult = CRLiteLookupResult::CertificateValid;
        }
      }
    }

    // Also check stashed CRLite revocations. This information is
    // deterministic and has already been validated by our infrastructure (it
    // comes from signed CRLs), so if the stash says a certificate is revoked,
    // it is.
    bool isRevokedByStash = false;
    rv = mCertStorage->IsCertRevokedByStash(
        issuerSubjectPublicKeyInfoBytes, serialNumberBytes, &isRevokedByStash);
    if (NS_FAILED(rv)) {
      MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
              ("NSSCertDBTrustDomain::CheckRevocation: IsCertRevokedByStash "
               "failed"));
      if (mCRLiteLookupResult) {
        *mCRLiteLookupResult = CRLiteLookupResult::LibraryFailure;
      }
      if (mCRLiteMode == CRLiteMode::Enforce) {
        return Result::FATAL_ERROR_LIBRARY_FAILURE;
      }
    } else if (isRevokedByStash) {
      MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
              ("NSSCertDBTrustDomain::CheckRevocation: IsCertRevokedByStash "
               "returned true"));
      if (mCRLiteLookupResult) {
        *mCRLiteLookupResult = CRLiteLookupResult::CertRevokedByStash;
      }
      if (mCRLiteMode == CRLiteMode::Enforce) {
        return Result::ERROR_REVOKED_CERTIFICATE;
      }
    } else if (certificateFoundValidInCRLiteFilter &&
               mCRLiteMode == CRLiteMode::Enforce) {
      MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
              ("NSSCertDBTrustDomain::CheckRevocation: certificate covered by "
               "CRLite, found to be valid -> skipping OCSP processing"));
      return Success;
    }
  }

  // Bug 991815: The BR allow OCSP for intermediates to be up to one year old.
  // Since this affects EV there is no reason why DV should be more strict
  // so all intermediates are allowed to have OCSP responses up to one year
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
    stapledOCSPResponseResult = VerifyAndMaybeCacheEncodedOCSPResponse(
        certID, time, maxOCSPLifetimeInDays, *stapledOCSPResponse,
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
    } else if (stapledOCSPResponseResult ==
                   Result::ERROR_OCSP_TRY_SERVER_LATER ||
               stapledOCSPResponseResult ==
                   Result::ERROR_OCSP_INVALID_SIGNING_CERT) {
      // Stapled OCSP response present but invalid for a small number of reasons
      // CAs/servers commonly get wrong. This will be treated similarly to an
      // expired stapled response.
      mOCSPStaplingStatus = CertVerifier::OCSP_STAPLING_INVALID;
      MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
              ("NSSCertDBTrustDomain: stapled OCSP response: "
               "failure (allowed for compatibility)"));
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
  bool cachedResponsePresent =
      mOCSPCache.Get(certID, mOriginAttributes, cachedResponseResult,
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
  nsresult nsrv = mCertStorage->IsBlocklistFresh(&blocklistIsFresh);
  if (NS_FAILED(nsrv)) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  // TODO: We still need to handle the fallback for invalid stapled responses.
  // But, if/when we disable OCSP fetching by default, it would be ambiguous
  // whether security.OCSP.enable==0 means "I want the default" or "I really
  // never want you to ever fetch OCSP."
  // Additionally, this doesn't properly handle OCSP-must-staple when OCSP
  // fetching is disabled.
  Duration shortLifetime(mCertShortLifetimeInDays * Time::ONE_DAY_IN_SECONDS);
  if ((mOCSPFetching == NeverFetchOCSP) || (validityDuration < shortLifetime) ||
      (endEntityOrCA == EndEntityOrCA::MustBeCA &&
       (mOCSPFetching == FetchOCSPForDVHardFail ||
        mOCSPFetching == FetchOCSPForDVSoftFail || blocklistIsFresh))) {
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
  nsCString aiaLocation(VoidCString());

  if (aiaExtension) {
    rv = GetOCSPAuthorityInfoAccessLocation(arena, *aiaExtension, aiaLocation);
    if (rv != Success) {
      return rv;
    }
  }

  if (aiaLocation.IsVoid()) {
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

  if (cachedResponseResult == Success ||
      cachedResponseResult == Result::ERROR_OCSP_UNKNOWN_CERT ||
      cachedResponseResult == Result::ERROR_OCSP_OLD_RESPONSE) {
    // Only send a request to, and process a response from, the server if we
    // didn't have a cached indication of failure.  Also, don't keep requesting
    // responses from a failing server.
    return SynchronousCheckRevocationWithServer(
        certID, aiaLocation, time, maxOCSPLifetimeInDays, cachedResponseResult,
        stapledOCSPResponseResult);
  }

  return HandleOCSPFailure(cachedResponseResult, stapledOCSPResponseResult,
                           cachedResponseResult);
}

Result NSSCertDBTrustDomain::SynchronousCheckRevocationWithServer(
    const CertID& certID, const nsCString& aiaLocation, Time time,
    uint16_t maxOCSPLifetimeInDays, const Result cachedResponseResult,
    const Result stapledOCSPResponseResult) {
  uint8_t ocspRequestBytes[OCSP_REQUEST_MAX_LENGTH];
  size_t ocspRequestLength;

  Result rv = CreateEncodedOCSPRequest(*this, certID, ocspRequestBytes,
                                       ocspRequestLength);
  if (rv != Success) {
    return rv;
  }

  Vector<uint8_t> ocspResponse;
  Input response;
  rv = DoOCSPRequest(aiaLocation, mOriginAttributes, ocspRequestBytes,
                     ocspRequestLength, GetOCSPTimeout(), ocspResponse);
  if (rv == Success &&
      response.Init(ocspResponse.begin(), ocspResponse.length()) != Success) {
    rv = Result::ERROR_OCSP_MALFORMED_RESPONSE;  // too big
  }

  if (rv != Success) {
    Time timeout(time);
    if (timeout.AddSeconds(ServerFailureDelaySeconds) != Success) {
      return Result::FATAL_ERROR_LIBRARY_FAILURE;  // integer overflow
    }

    Result cacheRV =
        mOCSPCache.Put(certID, mOriginAttributes, rv, time, timeout);
    if (cacheRV != Success) {
      return cacheRV;
    }

    return HandleOCSPFailure(cachedResponseResult, stapledOCSPResponseResult,
                             rv);
  }

  // If the response from the network has expired but indicates a revoked
  // or unknown certificate, PR_GetError() will return the appropriate error.
  // We actually ignore expired here.
  bool expired;
  rv = VerifyAndMaybeCacheEncodedOCSPResponse(certID, time,
                                              maxOCSPLifetimeInDays, response,
                                              ResponseIsFromNetwork, expired);
  if (rv == Success || mOCSPFetching != FetchOCSPForDVSoftFail) {
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
            ("NSSCertDBTrustDomain: returning after "
             "VerifyEncodedOCSPResponse"));
    return rv;
  }

  if (rv == Result::ERROR_OCSP_UNKNOWN_CERT ||
      rv == Result::ERROR_REVOKED_CERTIFICATE) {
    return rv;
  }

  if (stapledOCSPResponseResult != Success) {
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
            ("NSSCertDBTrustDomain: returning SECFailure from expired/invalid "
             "stapled response after OCSP request verification failure"));
    return stapledOCSPResponseResult;
  }

  MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
          ("NSSCertDBTrustDomain: end of CheckRevocation"));

  return Success;  // Soft fail -> success :(
}

Result NSSCertDBTrustDomain::HandleOCSPFailure(
    const Result cachedResponseResult, const Result stapledOCSPResponseResult,
    const Result error) {
  if (mOCSPFetching != FetchOCSPForDVSoftFail) {
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
            ("NSSCertDBTrustDomain: returning SECFailure after OCSP request "
             "failure"));
    return error;
  }

  if (cachedResponseResult == Result::ERROR_OCSP_UNKNOWN_CERT) {
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
            ("NSSCertDBTrustDomain: returning SECFailure from cached response "
             "after OCSP request failure"));
    return cachedResponseResult;
  }

  if (stapledOCSPResponseResult != Success) {
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
            ("NSSCertDBTrustDomain: returning SECFailure from expired/invalid "
             "stapled response after OCSP request failure"));
    return stapledOCSPResponseResult;
  }

  MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
          ("NSSCertDBTrustDomain: returning SECSuccess after OCSP request "
           "failure"));
  return Success;  // Soft fail -> success :(
}

Result NSSCertDBTrustDomain::VerifyAndMaybeCacheEncodedOCSPResponse(
    const CertID& certID, Time time, uint16_t maxLifetimeInDays,
    Input encodedResponse, EncodedResponseSource responseSource,
    /*out*/ bool& expired) {
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
      return Result::FATAL_ERROR_LIBRARY_FAILURE;  // integer overflow
    }
  }
  if (responseSource == ResponseIsFromNetwork || rv == Success ||
      rv == Result::ERROR_REVOKED_CERTIFICATE ||
      rv == Result::ERROR_OCSP_UNKNOWN_CERT) {
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
            ("NSSCertDBTrustDomain: caching OCSP response"));
    Result putRV =
        mOCSPCache.Put(certID, mOriginAttributes, rv, thisUpdate, validThrough);
    if (putRV != Success) {
      return putRV;
    }
  }

  return rv;
}

// If a certificate in the given chain appears to have been issued by one of
// seven roots operated by StartCom and WoSign that are not trusted to issue new
// certificates, verify that the end-entity has a notBefore date before 21
// October 2016. If the value of notBefore is after this time, the chain is not
// valid.
// (NB: While there are seven distinct roots being checked for, two of them
// share distinguished names, resulting in six distinct distinguished names to
// actually look for.)
static Result CheckForStartComOrWoSign(const UniqueCERTCertList& certChain) {
  if (CERT_LIST_EMPTY(certChain)) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  const CERTCertListNode* endEntityNode = CERT_LIST_HEAD(certChain);
  if (!endEntityNode || !endEntityNode->cert) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  PRTime notBefore;
  PRTime notAfter;
  if (CERT_GetCertTimes(endEntityNode->cert, &notBefore, &notAfter) !=
      SECSuccess) {
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
    nsTArray<uint8_t> certDER(node->cert->derCert.data,
                              node->cert->derCert.len);
    if (CertDNIsInList(certDER, StartComAndWoSignDNs)) {
      return Result::ERROR_REVOKED_CERTIFICATE;
    }
  }
  return Success;
}

SECStatus GetCertDistrustAfterValue(const SECItem* distrustItem,
                                    PRTime& distrustTime) {
  if (!distrustItem || !distrustItem->data || distrustItem->len != 13) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }
  return DER_DecodeTimeChoice(&distrustTime, distrustItem);
}

SECStatus GetCertNotBeforeValue(const CERTCertificate* cert,
                                PRTime& distrustTime) {
  return DER_DecodeTimeChoice(&distrustTime, &cert->validity.notBefore);
}

nsresult isDistrustedCertificateChain(const UniqueCERTCertList& certList,
                                      const SECTrustType certDBTrustType,
                                      bool& isDistrusted) {
  // Set the default result to be distrusted.
  isDistrusted = true;

  // There is no distrust to set if the certDBTrustType is not SSL or Email.
  if (certDBTrustType != trustSSL && certDBTrustType != trustEmail) {
    isDistrusted = false;
    return NS_OK;
  }

  // Allocate objects and retreive the root and end-entity certificates.
  const CERTCertificate* certRoot = CERT_LIST_TAIL(certList)->cert;
  const CERTCertificate* certLeaf = CERT_LIST_HEAD(certList)->cert;

  // Set isDistrusted to false if there is no distrust for the root.
  if (!certRoot->distrust) {
    isDistrusted = false;
    return NS_OK;
  }

  // Create a pointer to refer to the selected distrust struct.
  SECItem* distrustPtr = nullptr;
  if (certDBTrustType == trustSSL) {
    distrustPtr = &certRoot->distrust->serverDistrustAfter;
  }
  if (certDBTrustType == trustEmail) {
    distrustPtr = &certRoot->distrust->emailDistrustAfter;
  }

  // Get validity for the current end-entity certificate
  // and get the distrust field for the root certificate.
  PRTime certRootDistrustAfter;
  PRTime certLeafNotBefore;

  SECStatus rv = GetCertDistrustAfterValue(distrustPtr, certRootDistrustAfter);
  if (rv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  rv = GetCertNotBeforeValue(certLeaf, certLeafNotBefore);
  if (rv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  // Compare the validity of the end-entity certificate with
  // the distrust value of the root.
  if (certLeafNotBefore <= certRootDistrustAfter) {
    isDistrusted = false;
  }

  return NS_OK;
}

Result NSSCertDBTrustDomain::IsChainValid(const DERArray& certArray, Time time,
                                          const CertPolicyId& requiredPolicy) {
  MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
          ("NSSCertDBTrustDomain: IsChainValid"));

  UniqueCERTCertList certList;
  SECStatus srv =
      ConstructCERTCertListFromReversedDERArray(certArray, certList);
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

  // Modernization in-progress: Keep certList as a CERTCertList for storage into
  // the mBuiltChain variable at the end.
  nsTArray<RefPtr<nsIX509Cert>> nssCertList;
  nsresult nsrv = nsNSSCertificateDB::ConstructCertArrayFromUniqueCertList(
      certList, nssCertList);

  if (NS_FAILED(nsrv)) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  nsCOMPtr<nsIX509Cert> rootCert;
  nsrv = nsNSSCertificate::GetRootCertificate(nssCertList, rootCert);
  if (NS_FAILED(nsrv)) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  UniqueCERTCertificate root(rootCert->GetCert());
  if (!root) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  bool isBuiltInRoot = false;
  nsrv = rootCert->GetIsBuiltInRoot(&isBuiltInRoot);
  if (NS_FAILED(nsrv)) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
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

    nsTArray<Span<const uint8_t>> derCertSpanList;
    size_t numCerts = certArray.GetLength();
    for (size_t i = numCerts; i > 0; --i) {
      const Input* der = certArray.GetDER(i - 1);
      if (!der) {
        return Result::FATAL_ERROR_LIBRARY_FAILURE;
      }
      derCertSpanList.EmplaceBack(der->UnsafeGetData(), der->GetLength());
    }

    nsrv = PublicKeyPinningService::ChainHasValidPins(
        derCertSpanList, mHostname, time, enforceTestMode, mOriginAttributes,
        chainHasValidPins, mPinningTelemetryInfo);
    if (NS_FAILED(nsrv)) {
      return Result::FATAL_ERROR_LIBRARY_FAILURE;
    }
    if (!chainHasValidPins) {
      return Result::ERROR_KEY_PINNING_FAILURE;
    }
  }

  // Check that the childs' certificate NotBefore date is anterior to
  // the NotAfter value of the parent when the root is a builtin.
  if (isBuiltInRoot) {
    bool isDistrusted;
    nsrv =
        isDistrustedCertificateChain(certList, mCertDBTrustType, isDistrusted);
    if (NS_FAILED(nsrv)) {
      return Result::FATAL_ERROR_LIBRARY_FAILURE;
    }
    if (isDistrusted) {
      return Result::ERROR_UNTRUSTED_ISSUER;
    }
  }

  // See bug 1434300. If the root is a Symantec root, see if we distrust this
  // path. Since we already have the root available, we can check that cheaply
  // here before proceeding with the rest of the algorithm.

  // This algorithm only applies if we are verifying in the context of a TLS
  // handshake. To determine this, we check mHostname: If it isn't set, this is
  // not TLS, so don't run the algorithm.
  nsTArray<uint8_t> rootCertDER(root.get()->derCert.data,
                                root.get()->derCert.len);
  if (mHostname && CertDNIsInList(rootCertDER, RootSymantecDNs)) {
    nsTArray<nsTArray<uint8_t>> intCerts;

    nsrv = nsNSSCertificate::GetIntermediatesAsDER(nssCertList, intCerts);
    if (NS_FAILED(nsrv)) {
      // This chain is supposed to be complete, so this is an error.
      return Result::ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED;
    }

    bool isDistrusted = false;
    nsrv = CheckForSymantecDistrust(intCerts, RootAppleAndGoogleSPKIs,
                                    isDistrusted);
    if (NS_FAILED(nsrv)) {
      return Result::FATAL_ERROR_LIBRARY_FAILURE;
    }
    if (isDistrusted) {
      mSawDistrustedCAByPolicyError = true;
      return Result::ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED;
    }
  }

  mBuiltChain = std::move(certList);

  return Success;
}

Result NSSCertDBTrustDomain::CheckSignatureDigestAlgorithm(
    DigestAlgorithm aAlg, EndEntityOrCA endEntityOrCA, Time notBefore) {
  // (new Date("2016-01-01T00:00:00Z")).getTime() / 1000
  static const Time JANUARY_FIRST_2016 = TimeFromEpochInSeconds(1451606400);

  MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
          ("NSSCertDBTrustDomain: CheckSignatureDigestAlgorithm"));
  if (aAlg == DigestAlgorithm::sha1) {
    switch (mSHA1Mode) {
      case CertVerifier::SHA1Mode::Forbidden:
        MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
                ("SHA-1 certificate rejected"));
        return Result::ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED;
      case CertVerifier::SHA1Mode::ImportedRootOrBefore2016:
        if (JANUARY_FIRST_2016 <= notBefore) {
          MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
                  ("Post-2015 SHA-1 certificate rejected"));
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

Result NSSCertDBTrustDomain::CheckRSAPublicKeyModulusSizeInBits(
    EndEntityOrCA /*endEntityOrCA*/, unsigned int modulusSizeInBits) {
  if (modulusSizeInBits < mMinRSABits) {
    return Result::ERROR_INADEQUATE_KEY_SIZE;
  }
  return Success;
}

Result NSSCertDBTrustDomain::VerifyRSAPKCS1SignedDigest(
    const SignedDigest& signedDigest, Input subjectPublicKeyInfo) {
  return VerifyRSAPKCS1SignedDigestNSS(signedDigest, subjectPublicKeyInfo,
                                       mPinArg);
}

Result NSSCertDBTrustDomain::CheckECDSACurveIsAcceptable(
    EndEntityOrCA /*endEntityOrCA*/, NamedCurve curve) {
  switch (curve) {
    case NamedCurve::secp256r1:  // fall through
    case NamedCurve::secp384r1:  // fall through
    case NamedCurve::secp521r1:
      return Success;
  }

  return Result::ERROR_UNSUPPORTED_ELLIPTIC_CURVE;
}

Result NSSCertDBTrustDomain::VerifyECDSASignedDigest(
    const SignedDigest& signedDigest, Input subjectPublicKeyInfo) {
  return VerifyECDSASignedDigestNSS(signedDigest, subjectPublicKeyInfo,
                                    mPinArg);
}

Result NSSCertDBTrustDomain::CheckValidityIsAcceptable(
    Time notBefore, Time notAfter, EndEntityOrCA endEntityOrCA,
    KeyPurposeId keyPurpose) {
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
      MOZ_ASSERT_UNREACHABLE(
          "We're not handling every ValidityCheckingMode type");
  }

  if (validityDuration > maxValidityDuration) {
    return Result::ERROR_VALIDITY_TOO_LONG;
  }

  return Success;
}

Result NSSCertDBTrustDomain::NetscapeStepUpMatchesServerAuth(
    Time notBefore,
    /*out*/ bool& matches) {
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

void NSSCertDBTrustDomain::ResetAccumulatedState() {
  mOCSPStaplingStatus = CertVerifier::OCSP_STAPLING_NEVER_CHECKED;
  mSCTListFromOCSPStapling = nullptr;
  mSCTListFromCertificate = nullptr;
  mSawDistrustedCAByPolicyError = false;
}

static Input SECItemToInput(const UniqueSECItem& item) {
  Input result;
  if (item) {
    MOZ_ASSERT(item->type == siBuffer);
    Result rv = result.Init(item->data, item->len);
    // As used here, |item| originally comes from an Input,
    // so there should be no issues converting it back.
    MOZ_ASSERT(rv == Success);
    Unused << rv;  // suppresses warnings in release builds
  }
  return result;
}

Input NSSCertDBTrustDomain::GetSCTListFromCertificate() const {
  return SECItemToInput(mSCTListFromCertificate);
}

Input NSSCertDBTrustDomain::GetSCTListFromOCSPStapling() const {
  return SECItemToInput(mSCTListFromOCSPStapling);
}

bool NSSCertDBTrustDomain::GetIsErrorDueToDistrustedCAPolicy() const {
  return mSawDistrustedCAByPolicyError;
}

void NSSCertDBTrustDomain::NoteAuxiliaryExtension(AuxiliaryExtension extension,
                                                  Input extensionData) {
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

SECStatus InitializeNSS(const nsACString& dir, NSSDBConfig nssDbConfig,
                        PKCS11DBConfig pkcs11DbConfig) {
  MOZ_ASSERT(NS_IsMainThread());

  // The NSS_INIT_NOROOTINIT flag turns off the loading of the root certs
  // module by NSS_Initialize because we will load it in LoadLoadableRoots
  // later.  It also allows us to work around a bug in the system NSS in
  // Ubuntu 8.04, which loads any nonexistent "<configdir>/libnssckbi.so" as
  // "/usr/lib/nss/libnssckbi.so".
  uint32_t flags = NSS_INIT_NOROOTINIT | NSS_INIT_OPTIMIZESPACE;
  if (nssDbConfig == NSSDBConfig::ReadOnly) {
    flags |= NSS_INIT_READONLY;
  }
  if (pkcs11DbConfig == PKCS11DBConfig::DoNotLoadModules) {
    flags |= NSS_INIT_NOMODDB;
  }
  nsAutoCString dbTypeAndDirectory("sql:");
  dbTypeAndDirectory.Append(dir);
  MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
          ("InitializeNSS(%s, %d, %d)", dbTypeAndDirectory.get(),
           (int)nssDbConfig, (int)pkcs11DbConfig));
  SECStatus srv =
      NSS_Initialize(dbTypeAndDirectory.get(), "", "", SECMOD_DB, flags);
  if (srv != SECSuccess) {
    return srv;
  }

  if (nssDbConfig == NSSDBConfig::ReadWrite) {
    UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
    if (!slot) {
      return SECFailure;
    }
    // If the key DB doesn't have a password set, PK11_NeedUserInit will return
    // true. For the SQL DB, we need to set a password or we won't be able to
    // import any certificates or change trust settings.
    if (PK11_NeedUserInit(slot.get())) {
      srv = PK11_InitPin(slot.get(), nullptr, nullptr);
      MOZ_ASSERT(srv == SECSuccess);
      Unused << srv;
    }
  }

  return SECSuccess;
}

void DisableMD5() {
  NSS_SetAlgorithmPolicy(
      SEC_OID_MD5, 0,
      NSS_USE_ALG_IN_CERT_SIGNATURE | NSS_USE_ALG_IN_CMS_SIGNATURE);
  NSS_SetAlgorithmPolicy(
      SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION, 0,
      NSS_USE_ALG_IN_CERT_SIGNATURE | NSS_USE_ALG_IN_CMS_SIGNATURE);
  NSS_SetAlgorithmPolicy(
      SEC_OID_PKCS5_PBE_WITH_MD5_AND_DES_CBC, 0,
      NSS_USE_ALG_IN_CERT_SIGNATURE | NSS_USE_ALG_IN_CMS_SIGNATURE);
}

bool LoadUserModuleAt(const char* moduleName, const char* libraryName,
                      const nsCString& dir) {
  // If a module exists with the same name, make a best effort attempt to delete
  // it. Note that it isn't possible to delete the internal module, so checking
  // the return value would be detrimental in that case.
  int unusedModType;
  Unused << SECMOD_DeleteModule(moduleName, &unusedModType);

  nsAutoCString fullLibraryPath;
  if (!dir.IsEmpty()) {
    fullLibraryPath.Assign(dir);
    fullLibraryPath.AppendLiteral(FILE_PATH_SEPARATOR);
  }
  fullLibraryPath.Append(MOZ_DLL_PREFIX);
  fullLibraryPath.Append(libraryName);
  fullLibraryPath.Append(MOZ_DLL_SUFFIX);
  // Escape the \ and " characters.
  fullLibraryPath.ReplaceSubstring("\\", "\\\\");
  fullLibraryPath.ReplaceSubstring("\"", "\\\"");

  nsAutoCString pkcs11ModuleSpec("name=\"");
  pkcs11ModuleSpec.Append(moduleName);
  pkcs11ModuleSpec.AppendLiteral("\" library=\"");
  pkcs11ModuleSpec.Append(fullLibraryPath);
  pkcs11ModuleSpec.AppendLiteral("\"");

  UniqueSECMODModule userModule(SECMOD_LoadUserModule(
      const_cast<char*>(pkcs11ModuleSpec.get()), nullptr, false));
  if (!userModule) {
    return false;
  }

  if (!userModule->loaded) {
    return false;
  }

  return true;
}

const char* kOSClientCertsModuleName = "OS Client Cert Module";

bool LoadOSClientCertsModule(const nsCString& dir) {
#ifdef MOZ_WIDGET_COCOA
  // osclientcerts requires macOS 10.14 or later
  if (!nsCocoaFeatures::OnMojaveOrLater()) {
    return false;
  }
#endif
  return LoadUserModuleAt(kOSClientCertsModuleName, "osclientcerts", dir);
}

bool LoadLoadableRoots(const nsCString& dir) {
  // Some NSS command-line utilities will load a roots module under the name
  // "Root Certs" if there happens to be a `MOZ_DLL_PREFIX "nssckbi"
  // MOZ_DLL_SUFFIX` file in the directory being operated on. In some cases this
  // can cause us to fail to load our roots module. In these cases, deleting the
  // "Root Certs" module allows us to load the correct one. See bug 1406396.
  int unusedModType;
  Unused << SECMOD_DeleteModule("Root Certs", &unusedModType);
  return LoadUserModuleAt(kRootModuleName, "nssckbi", dir);
}

void UnloadUserModules() {
  UniqueSECMODModule rootsModule(SECMOD_FindModule(kRootModuleName));
  if (rootsModule) {
    SECMOD_UnloadUserModule(rootsModule.get());
  }
  UniqueSECMODModule osClientCertsModule(
      SECMOD_FindModule(kOSClientCertsModuleName));
  if (osClientCertsModule) {
    SECMOD_UnloadUserModule(osClientCertsModule.get());
  }
}

nsresult DefaultServerNicknameForCert(const CERTCertificate* cert,
                                      /*out*/ nsCString& nickname) {
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

Result BuildRevocationCheckArrays(Input certDER, EndEntityOrCA endEntityOrCA,
                                  /*out*/ nsTArray<uint8_t>& issuerBytes,
                                  /*out*/ nsTArray<uint8_t>& serialBytes,
                                  /*out*/ nsTArray<uint8_t>& subjectBytes,
                                  /*out*/ nsTArray<uint8_t>& pubKeyBytes) {
  BackCert cert(certDER, endEntityOrCA, nullptr);
  Result rv = cert.Init();
  if (rv != Success) {
    return rv;
  }
  issuerBytes.Clear();
  Input issuer(cert.GetIssuer());
  issuerBytes.AppendElements(issuer.UnsafeGetData(), issuer.GetLength());
  serialBytes.Clear();
  Input serial(cert.GetSerialNumber());
  serialBytes.AppendElements(serial.UnsafeGetData(), serial.GetLength());
  subjectBytes.Clear();
  Input subject(cert.GetSubject());
  subjectBytes.AppendElements(subject.UnsafeGetData(), subject.GetLength());
  pubKeyBytes.Clear();
  Input pubKey(cert.GetSubjectPublicKeyInfo());
  pubKeyBytes.AppendElements(pubKey.UnsafeGetData(), pubKey.GetLength());

  return Success;
}

bool CertIsInCertStorage(CERTCertificate* cert, nsICertStorage* certStorage) {
  MOZ_ASSERT(cert);
  MOZ_ASSERT(certStorage);
  if (!cert || !certStorage) {
    return false;
  }
  nsTArray<uint8_t> subject;
  subject.AppendElements(cert->derSubject.data, cert->derSubject.len);
  nsTArray<nsTArray<uint8_t>> certStorageCerts;
  nsresult rv = certStorage->FindCertsBySubject(subject, certStorageCerts);
  if (NS_FAILED(rv)) {
    return false;
  }
  for (const auto& certStorageCert : certStorageCerts) {
    if (certStorageCert.Length() != cert->derCert.len) {
      continue;
    }
    if (memcmp(certStorageCert.Elements(), cert->derCert.data,
               certStorageCert.Length()) == 0) {
      return true;
    }
  }
  return false;
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
void SaveIntermediateCerts(const nsTArray<nsTArray<uint8_t>>& certList) {
  UniqueCERTCertList intermediates(CERT_NewCertList());
  if (!intermediates) {
    return;
  }

  size_t index = 0;
  size_t numIntermediates = 0;
  for (const auto& certDER : certList) {
    // Skip the end-entity; we only want to store intermediates. Similarly,
    // there's no need to save the trust anchor - it's either already a
    // permanent certificate or it's the Microsoft Family Safety root or an
    // enterprise root temporarily imported via the child mode or enterprise
    // root features. We don't want to import these because they're intended to
    // be temporary (and because importing them happens to reset their trust
    // settings, which breaks these features).
    index++;
    if (index == 1 || index == certList.Length()) {
      continue;
    }

    SECItem certDERItem = {siBuffer,
                           const_cast<unsigned char*>(certDER.Elements()),
                           AssertedCast<unsigned int>(certDER.Length())};
    UniqueCERTCertificate certHandle(CERT_NewTempCertificate(
        CERT_GetDefaultCertDB(), &certDERItem, nullptr, false, true));
    if (!certHandle) {
      continue;
    }
    if (certHandle->slot) {
      // This cert was found on a token; no need to remember it in the permanent
      // database.
      continue;
    }

    PRBool isperm;
    if (CERT_GetCertIsPerm(certHandle.get(), &isperm) != SECSuccess) {
      continue;
    }
    if (isperm) {
      // We don't need to remember certs already stored in perm db.
      continue;
    }

    if (CERT_AddCertToListTail(intermediates.get(), certHandle.get()) !=
        SECSuccess) {
      // If this fails, we're probably out of memory. Just return.
      return;
    }
    certHandle.release();  // intermediates now owns the reference
    numIntermediates++;
  }

  if (numIntermediates > 0) {
    nsCOMPtr<nsIRunnable> importCertsRunnable(NS_NewRunnableFunction(
        "IdleSaveIntermediateCerts",
        [intermediates = std::move(intermediates)]() -> void {
          if (AppShutdown::IsShuttingDown()) {
            return;
          }

          UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
          if (!slot) {
            return;
          }
          nsCOMPtr<nsICertStorage> certStorage(
              do_GetService(NS_CERT_STORAGE_CID));
          size_t numCertsImported = 0;
          for (CERTCertListNode* node = CERT_LIST_HEAD(intermediates);
               !CERT_LIST_END(node, intermediates);
               node = CERT_LIST_NEXT(node)) {
            if (AppShutdown::IsShuttingDown()) {
              return;
            }

            if (CertIsInCertStorage(node->cert, certStorage)) {
              continue;
            }
            PRBool isperm;
            if (CERT_GetCertIsPerm(node->cert, &isperm) != SECSuccess) {
              continue;
            }
            if (isperm) {
              // This may be a certificate that has already been imported by
              // another background import task that happened to run before
              // this one.
              continue;
            }
            // This is a best-effort attempt at avoiding unknown issuer errors
            // in the future, so ignore failures here.
            nsAutoCString nickname;
            if (NS_FAILED(DefaultServerNicknameForCert(node->cert, nickname))) {
              continue;
            }
            Unused << PK11_ImportCert(slot.get(), node->cert, CK_INVALID_HANDLE,
                                      nickname.get(), false);
            numCertsImported++;
          }

          nsCOMPtr<nsIRunnable> runnable(NS_NewRunnableFunction(
              "IdleSaveIntermediateCertsDone", [numCertsImported]() -> void {
                nsCOMPtr<nsIObserverService> observerService =
                    mozilla::services::GetObserverService();
                if (observerService) {
                  NS_ConvertUTF8toUTF16 numCertsImportedString(
                      nsPrintfCString("%zu", numCertsImported));
                  observerService->NotifyObservers(
                      nullptr, "psm:intermediate-certs-cached",
                      numCertsImportedString.get());
                }
              }));
          Unused << NS_DispatchToMainThread(runnable.forget());
        }));
    Unused << NS_DispatchToCurrentThreadQueue(importCertsRunnable.forget(),
                                              EventQueuePriority::Idle);
  }
}

}  // namespace psm
}  // namespace mozilla
