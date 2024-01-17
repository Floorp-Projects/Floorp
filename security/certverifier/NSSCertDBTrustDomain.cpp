/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "NSSCertDBTrustDomain.h"

#include <stdint.h>
#include <utility>

#include "CRLiteTimestamp.h"
#include "ExtendedValidation.h"
#include "MultiLogCTVerifier.h"
#include "NSSErrorsService.h"
#include "PublicKeyPinningService.h"
#include "cert.h"
#include "cert_storage/src/cert_storage.h"
#include "certdb.h"
#include "mozilla/AppShutdown.h"
#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Logging.h"
#include "mozilla/PodOperations.h"
#include "mozilla/Services.h"
#include "mozilla/StaticPrefs_security.h"
#include "mozilla/SyncRunnable.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/Unused.h"
#include "mozpkix/Result.h"
#include "mozpkix/pkix.h"
#include "mozpkix/pkixnss.h"
#include "mozpkix/pkixutil.h"
#include "nsCRTGlue.h"
#include "nsIObserverService.h"
#include "nsNetCID.h"
#include "nsNSSCallbacks.h"
#include "nsNSSCertHelper.h"
#include "nsNSSCertificate.h"
#include "nsNSSCertificateDB.h"
#include "nsNSSIOLayer.h"
#include "nsPrintfCString.h"
#include "nsServiceManagerUtils.h"
#include "nsThreadUtils.h"
#include "nss.h"
#include "pk11pub.h"
#include "prerror.h"
#include "secder.h"
#include "secerr.h"

#include "TrustOverrideUtils.h"
#include "TrustOverride-AppleGoogleDigiCertData.inc"
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
    unsigned int minRSABits, ValidityCheckingMode validityCheckingMode,
    NetscapeStepUpPolicy netscapeStepUpPolicy, CRLiteMode crliteMode,
    const OriginAttributes& originAttributes,
    const nsTArray<Input>& thirdPartyRootInputs,
    const nsTArray<Input>& thirdPartyIntermediateInputs,
    const Maybe<nsTArray<nsTArray<uint8_t>>>& extraCertificates,
    /*out*/ nsTArray<nsTArray<uint8_t>>& builtChain,
    /*optional*/ PinningTelemetryInfo* pinningTelemetryInfo,
    /*optional*/ const char* hostname)
    : mCertDBTrustType(certDBTrustType),
      mOCSPFetching(ocspFetching),
      mOCSPCache(ocspCache),
      mPinArg(pinArg),
      mOCSPTimeoutSoft(ocspTimeoutSoft),
      mOCSPTimeoutHard(ocspTimeoutHard),
      mCertShortLifetimeInDays(certShortLifetimeInDays),
      mMinRSABits(minRSABits),
      mValidityCheckingMode(validityCheckingMode),
      mNetscapeStepUpPolicy(netscapeStepUpPolicy),
      mCRLiteMode(crliteMode),
      mSawDistrustedCAByPolicyError(false),
      mOriginAttributes(originAttributes),
      mThirdPartyRootInputs(thirdPartyRootInputs),
      mThirdPartyIntermediateInputs(thirdPartyIntermediateInputs),
      mExtraCertificates(extraCertificates),
      mBuiltChain(builtChain),
      mIsBuiltChainRootBuiltInRoot(false),
      mPinningTelemetryInfo(pinningTelemetryInfo),
      mHostname(hostname),
      mCertStorage(do_GetService(NS_CERT_STORAGE_CID)),
      mOCSPStaplingStatus(CertVerifier::OCSP_STAPLING_NEVER_CHECKED),
      mBuiltInRootsModule(SECMOD_FindModule(kRootModuleName)),
      mOCSPFetchStatus(OCSPFetchStatus::NotFetched) {}

static void FindRootsWithSubject(UniqueSECMODModule& rootsModule,
                                 SECItem subject,
                                 /*out*/ nsTArray<nsTArray<uint8_t>>& roots) {
  MOZ_ASSERT(rootsModule);
  AutoSECMODListReadLock lock;
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
  if (VerifySignedData(trustDomain, cert.GetSignedData(),
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
    // Stop path building if the program is shutting down.
    if (AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownConfirmed)) {
      keepGoing = false;
      return Success;
    }
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

  // Synchronously dispatch a task to the socket thread to find
  // CERTCertificates with the given subject. This involves querying NSS
  // structures and databases, so it should be done on the socket thread.
  nsTArray<nsTArray<uint8_t>> nssRootCandidates;
  nsTArray<nsTArray<uint8_t>> nssIntermediateCandidates;
  RefPtr<Runnable> getCandidatesTask =
      NS_NewRunnableFunction("NSSCertDBTrustDomain::FindIssuer", [&]() {
        if (AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownConfirmed)) {
          return;
        }
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
      Telemetry::AccumulateCategorical(
          Telemetry::LABELS_CERT_REVOCATION_MECHANISMS::OneCRL);
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
  // structures and databases, so it should be done on the socket thread.
  Result result = Result::FATAL_ERROR_LIBRARY_FAILURE;
  RefPtr<Runnable> getTrustTask =
      NS_NewRunnableFunction("NSSCertDBTrustDomain::GetCertTrust", [&]() {
        if (AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownConfirmed)) {
          result = Result::FATAL_ERROR_LIBRARY_FAILURE;
          return;
        }
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

NS_IMPL_ISUPPORTS(CRLiteTimestamp, nsICRLiteTimestamp)

NS_IMETHODIMP
CRLiteTimestamp::GetLogID(nsTArray<uint8_t>& aLogID) {
  aLogID.Clear();
  aLogID.AppendElements(mLogID);
  return NS_OK;
}

NS_IMETHODIMP
CRLiteTimestamp::GetTimestamp(uint64_t* aTimestamp) {
  *aTimestamp = mTimestamp;
  return NS_OK;
}

Result BuildCRLiteTimestampArray(
    Input sctExtension,
    /*out*/ nsTArray<RefPtr<nsICRLiteTimestamp>>& timestamps) {
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

  for (const auto& sct : decodedSCTs) {
    timestamps.AppendElement(new CRLiteTimestamp(sct));
  }
  return Success;
}

Result NSSCertDBTrustDomain::CheckCRLiteStash(
    const nsTArray<uint8_t>& issuerSubjectPublicKeyInfoBytes,
    const nsTArray<uint8_t>& serialNumberBytes) {
  // This information is deterministic and has already been validated by our
  // infrastructure (it comes from signed CRLs), so if the stash says a
  // certificate is revoked, it is.
  bool isRevokedByStash = false;
  nsresult rv = mCertStorage->IsCertRevokedByStash(
      issuerSubjectPublicKeyInfoBytes, serialNumberBytes, &isRevokedByStash);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
            ("NSSCertDBTrustDomain::CheckCRLiteStash: IsCertRevokedByStash "
             "failed"));
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  if (isRevokedByStash) {
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
            ("NSSCertDBTrustDomain::CheckCRLiteStash: IsCertRevokedByStash "
             "returned true"));
    return Result::ERROR_REVOKED_CERTIFICATE;
  }
  return Success;
}

Result NSSCertDBTrustDomain::CheckCRLite(
    const nsTArray<uint8_t>& issuerBytes,
    const nsTArray<uint8_t>& issuerSubjectPublicKeyInfoBytes,
    const nsTArray<uint8_t>& serialNumberBytes,
    const nsTArray<RefPtr<nsICRLiteTimestamp>>& timestamps,
    /*out*/ bool& filterCoversCertificate) {
  filterCoversCertificate = false;
  int16_t crliteRevocationState;
  nsresult rv = mCertStorage->GetCRLiteRevocationState(
      issuerBytes, issuerSubjectPublicKeyInfoBytes, serialNumberBytes,
      timestamps, &crliteRevocationState);
  if (NS_FAILED(rv)) {
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
            ("NSSCertDBTrustDomain::CheckCRLite: CRLite call failed"));
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
          ("NSSCertDBTrustDomain::CheckCRLite: CRLite check returned "
           "state=%hd",
           crliteRevocationState));

  switch (crliteRevocationState) {
    case nsICertStorage::STATE_ENFORCE:
      filterCoversCertificate = true;
      return Result::ERROR_REVOKED_CERTIFICATE;
    case nsICertStorage::STATE_UNSET:
      filterCoversCertificate = true;
      return Success;
    case nsICertStorage::STATE_NOT_ENROLLED:
      filterCoversCertificate = false;
      return Success;
    case nsICertStorage::STATE_NOT_COVERED:
      filterCoversCertificate = false;
      return Success;
    case nsICertStorage::STATE_NO_FILTER:
      filterCoversCertificate = false;
      return Success;
    default:
      MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
              ("NSSCertDBTrustDomain::CheckCRLite: Unknown CRLite revocation "
               "state"));
      return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
}

Result NSSCertDBTrustDomain::CheckRevocation(
    EndEntityOrCA endEntityOrCA, const CertID& certID, Time time,
    Duration validityDuration,
    /*optional*/ const Input* stapledOCSPResponse,
    /*optional*/ const Input* aiaExtension,
    /*optional*/ const Input* sctExtension) {
  // Actively distrusted certificates will have already been blocked by
  // GetCertTrust.

  MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
          ("NSSCertDBTrustDomain: Top of CheckRevocation\n"));

  // None of the revocation methods in this function are consulted for CA
  // certificates. Revocation for CAs is handled by GetCertTrust.
  if (endEntityOrCA == EndEntityOrCA::MustBeCA) {
    return Success;
  }

  // Look for an OCSP Authority Information Access URL. Our behavior in
  // ConfirmRevocations mode depends on whether a synchronous OCSP
  // request is possible.
  nsCString aiaLocation(VoidCString());
  if (aiaExtension) {
    UniquePLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
    if (!arena) {
      return Result::FATAL_ERROR_NO_MEMORY;
    }
    Result rv =
        GetOCSPAuthorityInfoAccessLocation(arena, *aiaExtension, aiaLocation);
    if (rv != Success) {
      return rv;
    }
  }

  bool crliteCoversCertificate = false;
  Result crliteResult = Success;
  if (mCRLiteMode != CRLiteMode::Disabled && sctExtension) {
    crliteResult =
        CheckRevocationByCRLite(certID, *sctExtension, crliteCoversCertificate);

    // If CheckCRLite returned an error other than "revoked certificate",
    // propagate that error.
    if (crliteResult != Success &&
        crliteResult != Result::ERROR_REVOKED_CERTIFICATE) {
      return crliteResult;
    }

    if (crliteCoversCertificate) {
      Telemetry::AccumulateCategorical(
          Telemetry::LABELS_CERT_REVOCATION_MECHANISMS::CRLite);
      // If we don't return here we will consult OCSP.
      // In Enforce CRLite mode we can return "Revoked" or "Not Revoked"
      // without consulting OCSP.
      if (mCRLiteMode == CRLiteMode::Enforce) {
        return crliteResult;
      }
      // If we don't have a URL for an OCSP responder, then we can return any
      // result ConfirmRevocations mode. Note that we might have a
      // stapled or cached OCSP response which we ignore in this case.
      if (mCRLiteMode == CRLiteMode::ConfirmRevocations &&
          aiaLocation.IsVoid()) {
        return crliteResult;
      }
      // In ConfirmRevocations mode we can return "Not Revoked"
      // without consulting OCSP.
      if (mCRLiteMode == CRLiteMode::ConfirmRevocations &&
          crliteResult == Success) {
        return Success;
      }
    }
  }

  bool ocspSoftFailure = false;
  Result ocspResult = CheckRevocationByOCSP(
      certID, time, validityDuration, aiaLocation, crliteCoversCertificate,
      crliteResult, stapledOCSPResponse, ocspSoftFailure);

  // In ConfirmRevocations mode we treat any OCSP failure as confirmation
  // of a CRLite revoked result.
  if (crliteCoversCertificate &&
      crliteResult == Result::ERROR_REVOKED_CERTIFICATE &&
      mCRLiteMode == CRLiteMode::ConfirmRevocations &&
      (ocspResult != Success || ocspSoftFailure)) {
    return Result::ERROR_REVOKED_CERTIFICATE;
  }

  MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
          ("NSSCertDBTrustDomain: end of CheckRevocation"));

  return ocspResult;
}

Result NSSCertDBTrustDomain::CheckRevocationByCRLite(
    const CertID& certID, const Input& sctExtension,
    /*out*/ bool& crliteCoversCertificate) {
  crliteCoversCertificate = false;
  MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
          ("NSSCertDBTrustDomain::CheckRevocation: checking CRLite"));
  nsTArray<uint8_t> issuerSubjectPublicKeyInfoBytes;
  issuerSubjectPublicKeyInfoBytes.AppendElements(
      certID.issuerSubjectPublicKeyInfo.UnsafeGetData(),
      certID.issuerSubjectPublicKeyInfo.GetLength());
  nsTArray<uint8_t> serialNumberBytes;
  serialNumberBytes.AppendElements(certID.serialNumber.UnsafeGetData(),
                                   certID.serialNumber.GetLength());
  // The CRLite stash is essentially a subset of a collection of CRLs, so if
  // it says a certificate is revoked, it is.
  Result rv =
      CheckCRLiteStash(issuerSubjectPublicKeyInfoBytes, serialNumberBytes);
  if (rv != Success) {
    crliteCoversCertificate = (rv == Result::ERROR_REVOKED_CERTIFICATE);
    return rv;
  }

  nsTArray<uint8_t> issuerBytes;
  issuerBytes.AppendElements(certID.issuer.UnsafeGetData(),
                             certID.issuer.GetLength());

  nsTArray<RefPtr<nsICRLiteTimestamp>> timestamps;
  rv = BuildCRLiteTimestampArray(sctExtension, timestamps);
  if (rv != Success) {
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
            ("decoding SCT extension failed - CRLite will be not be "
             "consulted"));
    return Success;
  }
  return CheckCRLite(issuerBytes, issuerSubjectPublicKeyInfoBytes,
                     serialNumberBytes, timestamps, crliteCoversCertificate);
}

Result NSSCertDBTrustDomain::CheckRevocationByOCSP(
    const CertID& certID, Time time, Duration validityDuration,
    const nsCString& aiaLocation, const bool crliteCoversCertificate,
    const Result crliteResult,
    /*optional*/ const Input* stapledOCSPResponse,
    /*out*/ bool& softFailure) {
  softFailure = false;
  const uint16_t maxOCSPLifetimeInDays = 10;
  // If we have a stapled OCSP response then the verification of that response
  // determines the result unless the OCSP response is expired. We make an
  // exception for expired responses because some servers, nginx in particular,
  // are known to serve expired responses due to bugs.
  // We keep track of the result of verifying the stapled response but don't
  // immediately return failure if the response has expired.
  Result stapledOCSPResponseResult = Success;
  if (stapledOCSPResponse) {
    bool expired;
    uint32_t ageInHours;
    stapledOCSPResponseResult = VerifyAndMaybeCacheEncodedOCSPResponse(
        certID, time, maxOCSPLifetimeInDays, *stapledOCSPResponse,
        ResponseWasStapled, expired, ageInHours);
    Telemetry::AccumulateCategorical(
        Telemetry::LABELS_CERT_REVOCATION_MECHANISMS::StapledOCSP);
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
  } else {
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
    Telemetry::AccumulateCategorical(
        Telemetry::LABELS_CERT_REVOCATION_MECHANISMS::CachedOCSP);
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

  // TODO: We still need to handle the fallback for invalid stapled responses.
  // But, if/when we disable OCSP fetching by default, it would be ambiguous
  // whether security.OCSP.enable==0 means "I want the default" or "I really
  // never want you to ever fetch OCSP."
  // Additionally, this doesn't properly handle OCSP-must-staple when OCSP
  // fetching is disabled.
  Duration shortLifetime(mCertShortLifetimeInDays * Time::ONE_DAY_IN_SECONDS);
  if (validityDuration < shortLifetime) {
    Telemetry::AccumulateCategorical(
        Telemetry::LABELS_CERT_REVOCATION_MECHANISMS::ShortValidity);
  }
  if ((mOCSPFetching == NeverFetchOCSP) || (validityDuration < shortLifetime)) {
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

    softFailure = true;
    return Success;
  }

  if (mOCSPFetching == LocalOnlyOCSPForEV) {
    if (cachedResponseResult != Success) {
      return cachedResponseResult;
    }
    return Result::ERROR_OCSP_UNKNOWN_CERT;
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
    // certificate that lacks an OCSP responder URI. There's no need to set
    // softFailure here---we check for the presence of an AIA before attempting
    // OCSP when CRLite is configured in confirm revocations mode.
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
        stapledOCSPResponseResult, crliteCoversCertificate, crliteResult,
        softFailure);
  }

  return HandleOCSPFailure(cachedResponseResult, stapledOCSPResponseResult,
                           cachedResponseResult, softFailure);
}

Result NSSCertDBTrustDomain::SynchronousCheckRevocationWithServer(
    const CertID& certID, const nsCString& aiaLocation, Time time,
    uint16_t maxOCSPLifetimeInDays, const Result cachedResponseResult,
    const Result stapledOCSPResponseResult, const bool crliteCoversCertificate,
    const Result crliteResult, /*out*/ bool& softFailure) {
  if (AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownConfirmed)) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  uint8_t ocspRequestBytes[OCSP_REQUEST_MAX_LENGTH];
  size_t ocspRequestLength;
  Result rv = CreateEncodedOCSPRequest(*this, certID, ocspRequestBytes,
                                       ocspRequestLength);
  if (rv != Success) {
    return rv;
  }

  Vector<uint8_t> ocspResponse;
  Input response;
  mOCSPFetchStatus = OCSPFetchStatus::Fetched;
  rv = DoOCSPRequest(aiaLocation, mOriginAttributes, ocspRequestBytes,
                     ocspRequestLength, GetOCSPTimeout(), ocspResponse);
  Telemetry::AccumulateCategorical(
      Telemetry::LABELS_CERT_REVOCATION_MECHANISMS::OCSP);
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

    if (crliteCoversCertificate) {
      if (crliteResult == Success) {
        // CRLite says the certificate is OK, but OCSP fetching failed.
        Telemetry::AccumulateCategorical(
            Telemetry::LABELS_CRLITE_VS_OCSP_RESULT::CRLiteOkOCSPFail);
      } else {
        // CRLite says the certificate is revoked, but OCSP fetching failed.
        Telemetry::AccumulateCategorical(
            Telemetry::LABELS_CRLITE_VS_OCSP_RESULT::CRLiteRevOCSPFail);
      }
    }

    return HandleOCSPFailure(cachedResponseResult, stapledOCSPResponseResult,
                             rv, softFailure);
  }

  // If the response from the network has expired but indicates a revoked
  // or unknown certificate, PR_GetError() will return the appropriate error.
  // We actually ignore expired here.
  bool expired;
  uint32_t ageInHours;
  rv = VerifyAndMaybeCacheEncodedOCSPResponse(
      certID, time, maxOCSPLifetimeInDays, response, ResponseIsFromNetwork,
      expired, ageInHours);

  // If the CRLite filter covers the certificate, compare the CRLite result
  // with the OCSP fetching result. OCSP may have succeeded, said the
  // certificate is revoked, said the certificate doesn't exist, or it may have
  // failed for a reason that results in a "soft fail" (i.e. there is no
  // indication that the certificate is either definitely revoked or definitely
  // not revoked, so for usability, revocation checking says the certificate is
  // valid by default).
  if (crliteCoversCertificate) {
    if (rv == Success) {
      if (crliteResult == Success) {
        // CRLite and OCSP fetching agree the certificate is OK.
        Telemetry::AccumulateCategorical(
            Telemetry::LABELS_CRLITE_VS_OCSP_RESULT::CRLiteOkOCSPOk);
      } else {
        // CRLite says the certificate is revoked, but OCSP says it is OK.
        Telemetry::AccumulateCategorical(
            Telemetry::LABELS_CRLITE_VS_OCSP_RESULT::CRLiteRevOCSPOk);

        if (mCRLiteMode == CRLiteMode::ConfirmRevocations) {
          Telemetry::Accumulate(Telemetry::OCSP_AGE_AT_CRLITE_OVERRIDE,
                                ageInHours);
        }
      }
    } else if (rv == Result::ERROR_REVOKED_CERTIFICATE) {
      if (crliteResult == Success) {
        // CRLite says the certificate is OK, but OCSP says it is revoked.
        Telemetry::AccumulateCategorical(
            Telemetry::LABELS_CRLITE_VS_OCSP_RESULT::CRLiteOkOCSPRev);
      } else {
        // CRLite and OCSP fetching agree the certificate is revoked.
        Telemetry::AccumulateCategorical(
            Telemetry::LABELS_CRLITE_VS_OCSP_RESULT::CRLiteRevOCSPRev);
      }
    } else if (rv == Result::ERROR_OCSP_UNKNOWN_CERT) {
      if (crliteResult == Success) {
        // CRLite says the certificate is OK, but OCSP says it doesn't exist.
        Telemetry::AccumulateCategorical(
            Telemetry::LABELS_CRLITE_VS_OCSP_RESULT::CRLiteOkOCSPUnk);
      } else {
        // CRLite says the certificate is revoked, but OCSP says it doesn't
        // exist.
        Telemetry::AccumulateCategorical(
            Telemetry::LABELS_CRLITE_VS_OCSP_RESULT::CRLiteRevOCSPUnk);
      }
    } else {
      if (crliteResult == Success) {
        // CRLite says the certificate is OK, but OCSP encountered a soft-fail
        // error.
        Telemetry::AccumulateCategorical(
            Telemetry::LABELS_CRLITE_VS_OCSP_RESULT::CRLiteOkOCSPSoft);
      } else {
        // CRLite says the certificate is revoked, but OCSP encountered a
        // soft-fail error.
        Telemetry::AccumulateCategorical(
            Telemetry::LABELS_CRLITE_VS_OCSP_RESULT::CRLiteRevOCSPSoft);
      }
    }
  }

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

  softFailure = true;
  return Success;  // Soft fail -> success :(
}

Result NSSCertDBTrustDomain::HandleOCSPFailure(
    const Result cachedResponseResult, const Result stapledOCSPResponseResult,
    const Result error, /*out*/ bool& softFailure) {
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

  softFailure = true;
  return Success;  // Soft fail -> success :(
}

Result NSSCertDBTrustDomain::VerifyAndMaybeCacheEncodedOCSPResponse(
    const CertID& certID, Time time, uint16_t maxLifetimeInDays,
    Input encodedResponse, EncodedResponseSource responseSource,
    /*out*/ bool& expired,
    /*out*/ uint32_t& ageInHours) {
  Time thisUpdate(Time::uninitialized);
  Time validThrough(Time::uninitialized);

  Result rv = VerifyEncodedOCSPResponse(*this, certID, time, maxLifetimeInDays,
                                        encodedResponse, expired, &thisUpdate,
                                        &validThrough);
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
  // The `thisUpdate` field holds the latest time at which the server knew the
  // response was correct. The age of the response is the time that has elapsed
  // since. We only use this for the telemetry defined in Bug 1794479.
  uint64_t timeInSeconds;
  uint64_t thisUpdateInSeconds;
  uint64_t ageInSeconds;
  SecondsSinceEpochFromTime(time, &timeInSeconds);
  SecondsSinceEpochFromTime(thisUpdate, &thisUpdateInSeconds);
  if (timeInSeconds >= thisUpdateInSeconds) {
    ageInSeconds = timeInSeconds - thisUpdateInSeconds;
    // ageInHours is 32 bits because of the telemetry api.
    if (ageInSeconds > UINT32_MAX) {
      // We could divide by 3600 before checking the UINT32_MAX bound, but if
      // ageInSeconds is more than UINT32_MAX then there's been some sort of
      // error.
      ageInHours = UINT32_MAX;
    } else {
      // We start at 1 and divide with truncation to reserve ageInHours=0 for
      // the case where `thisUpdate` is in the future.
      ageInHours = 1 + ageInSeconds / (60 * 60);
    }
  } else {
    ageInHours = 0;
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

nsresult isDistrustedCertificateChain(
    const nsTArray<nsTArray<uint8_t>>& certArray,
    const SECTrustType certDBTrustType, bool& isDistrusted) {
  if (certArray.Length() == 0) {
    return NS_ERROR_FAILURE;
  }

  // Set the default result to be distrusted.
  isDistrusted = true;

  // There is no distrust to set if the certDBTrustType is not SSL or Email.
  if (certDBTrustType != trustSSL && certDBTrustType != trustEmail) {
    isDistrusted = false;
    return NS_OK;
  }

  SECStatus runnableRV = SECFailure;

  RefPtr<Runnable> isDistrustedChainTask =
      NS_NewRunnableFunction("isDistrustedCertificateChain", [&]() {
        if (AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownConfirmed)) {
          runnableRV = SECFailure;
          return;
        }
        // Allocate objects and retreive the root and end-entity certificates.
        CERTCertDBHandle* certDB(CERT_GetDefaultCertDB());
        const nsTArray<uint8_t>& certRootDER = certArray.LastElement();
        SECItem certRootDERItem = {
            siBuffer, const_cast<unsigned char*>(certRootDER.Elements()),
            AssertedCast<unsigned int>(certRootDER.Length())};
        UniqueCERTCertificate certRoot(CERT_NewTempCertificate(
            certDB, &certRootDERItem, nullptr, false, true));
        if (!certRoot) {
          runnableRV = SECFailure;
          return;
        }
        const nsTArray<uint8_t>& certLeafDER = certArray.ElementAt(0);
        SECItem certLeafDERItem = {
            siBuffer, const_cast<unsigned char*>(certLeafDER.Elements()),
            AssertedCast<unsigned int>(certLeafDER.Length())};
        UniqueCERTCertificate certLeaf(CERT_NewTempCertificate(
            certDB, &certLeafDERItem, nullptr, false, true));
        if (!certLeaf) {
          runnableRV = SECFailure;
          return;
        }

        // Set isDistrusted to false if there is no distrust for the root.
        if (!certRoot->distrust) {
          isDistrusted = false;
          runnableRV = SECSuccess;
          return;
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

        runnableRV =
            GetCertDistrustAfterValue(distrustPtr, certRootDistrustAfter);
        if (runnableRV != SECSuccess) {
          return;
        }

        runnableRV = GetCertNotBeforeValue(certLeaf.get(), certLeafNotBefore);
        if (runnableRV != SECSuccess) {
          return;
        }

        // Compare the validity of the end-entity certificate with
        // the distrust value of the root.
        if (certLeafNotBefore <= certRootDistrustAfter) {
          isDistrusted = false;
        }

        runnableRV = SECSuccess;
      });
  nsCOMPtr<nsIEventTarget> socketThread(
      do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID));
  if (!socketThread) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv =
      SyncRunnable::DispatchToThread(socketThread, isDistrustedChainTask);
  if (NS_FAILED(rv) || runnableRV != SECSuccess) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

Result NSSCertDBTrustDomain::IsChainValid(const DERArray& reversedDERArray,
                                          Time time,
                                          const CertPolicyId& requiredPolicy) {
  MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
          ("NSSCertDBTrustDomain: IsChainValid"));

  size_t numCerts = reversedDERArray.GetLength();
  if (numCerts < 1) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  nsTArray<nsTArray<uint8_t>> certArray;
  for (size_t i = numCerts; i > 0; --i) {
    const Input* derInput = reversedDERArray.GetDER(i - 1);
    certArray.EmplaceBack(derInput->UnsafeGetData(), derInput->GetLength());
  }

  const nsTArray<uint8_t>& rootBytes = certArray.LastElement();
  Input rootInput;
  Result rv = rootInput.Init(rootBytes.Elements(), rootBytes.Length());
  if (rv != Success) {
    return rv;
  }
  rv = IsCertBuiltInRoot(rootInput, mIsBuiltChainRootBuiltInRoot);
  if (rv != Result::Success) {
    return rv;
  }
  nsresult nsrv;
  // If mHostname isn't set, we're not verifying in the context of a TLS
  // handshake, so don't verify key pinning in those cases.
  if (mHostname) {
    nsTArray<Span<const uint8_t>> derCertSpanList;
    for (const auto& certDER : certArray) {
      derCertSpanList.EmplaceBack(certDER.Elements(), certDER.Length());
    }

    bool chainHasValidPins;
    nsrv = PublicKeyPinningService::ChainHasValidPins(
        derCertSpanList, mHostname, time, mIsBuiltChainRootBuiltInRoot,
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
  if (mIsBuiltChainRootBuiltInRoot) {
    bool isDistrusted;
    nsrv =
        isDistrustedCertificateChain(certArray, mCertDBTrustType, isDistrusted);
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
  const nsTArray<uint8_t>& rootCertDER = certArray.LastElement();
  if (mHostname && CertDNIsInList(rootCertDER, RootSymantecDNs)) {
    if (numCerts <= 1) {
      // This chain is supposed to be complete, so this is an error.
      return Result::ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED;
    }
    nsTArray<Input> intCerts;

    for (size_t i = 1; i < certArray.Length() - 1; ++i) {
      const nsTArray<uint8_t>& certBytes = certArray.ElementAt(i);
      Input certInput;
      rv = certInput.Init(certBytes.Elements(), certBytes.Length());
      if (rv != Success) {
        return Result::FATAL_ERROR_LIBRARY_FAILURE;
      }

      intCerts.EmplaceBack(certInput);
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

  mBuiltChain = std::move(certArray);

  return Success;
}

Result NSSCertDBTrustDomain::CheckSignatureDigestAlgorithm(
    DigestAlgorithm aAlg, EndEntityOrCA /*endEntityOrCA*/, Time /*notBefore*/) {
  switch (aAlg) {
    case DigestAlgorithm::sha256:  // fall through
    case DigestAlgorithm::sha384:  // fall through
    case DigestAlgorithm::sha512:
      return Success;
    case DigestAlgorithm::sha1:
      return Result::ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED;
  }
  return Result::FATAL_ERROR_LIBRARY_FAILURE;
}

Result NSSCertDBTrustDomain::CheckRSAPublicKeyModulusSizeInBits(
    EndEntityOrCA /*endEntityOrCA*/, unsigned int modulusSizeInBits) {
  if (modulusSizeInBits < mMinRSABits) {
    return Result::ERROR_INADEQUATE_KEY_SIZE;
  }
  return Success;
}

Result NSSCertDBTrustDomain::VerifyRSAPKCS1SignedData(
    Input data, DigestAlgorithm digestAlgorithm, Input signature,
    Input subjectPublicKeyInfo) {
  return VerifyRSAPKCS1SignedDataNSS(data, digestAlgorithm, signature,
                                     subjectPublicKeyInfo, mPinArg);
}

Result NSSCertDBTrustDomain::VerifyRSAPSSSignedData(
    Input data, DigestAlgorithm digestAlgorithm, Input signature,
    Input subjectPublicKeyInfo) {
  return VerifyRSAPSSSignedDataNSS(data, digestAlgorithm, signature,
                                   subjectPublicKeyInfo, mPinArg);
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

Result NSSCertDBTrustDomain::VerifyECDSASignedData(
    Input data, DigestAlgorithm digestAlgorithm, Input signature,
    Input subjectPublicKeyInfo) {
  return VerifyECDSASignedDataNSS(data, digestAlgorithm, signature,
                                  subjectPublicKeyInfo, mPinArg);
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
  mIsBuiltChainRootBuiltInRoot = false;
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

bool NSSCertDBTrustDomain::GetIsBuiltChainRootBuiltInRoot() const {
  return mIsBuiltChainRootBuiltInRoot;
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

// Load a given PKCS#11 module located in the given directory. It will be named
// the given module name. Optionally pass some string parameters to it via
// 'params'. This argument will be provided to C_Initialize when called on the
// module.
// |libraryName| and |dir| are encoded in UTF-8.
bool LoadUserModuleAt(const char* moduleName, const char* libraryName,
                      const nsCString& dir, /* optional */ const char* params) {
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
  if (params) {
    pkcs11ModuleSpec.AppendLiteral("\" parameters=\"");
    pkcs11ModuleSpec.Append(params);
    pkcs11ModuleSpec.AppendLiteral("\"");
  }

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

const char* kIPCClientCertsModuleName = "IPC Client Cert Module";

bool LoadIPCClientCertsModule(const nsCString& dir) {
  // The IPC client certs module needs to be able to call back into gecko to be
  // able to communicate with the parent process over IPC. This is achieved by
  // serializing the addresses of the relevant functions and passing them as an
  // extra string parameter that will be available when C_Initialize is called
  // on IPC client certs.
  nsPrintfCString addrs("%p,%p", DoFindObjects, DoSign);
  if (!LoadUserModuleAt(kIPCClientCertsModuleName, "ipcclientcerts", dir,
                        addrs.get())) {
    return false;
  }
  RunOnShutdown(
      []() {
        UniqueSECMODModule ipcClientCertsModule(
            SECMOD_FindModule(kIPCClientCertsModuleName));
        if (ipcClientCertsModule) {
          SECMOD_UnloadUserModule(ipcClientCertsModule.get());
        }
      },
      ShutdownPhase::XPCOMWillShutdown);
  return true;
}

const char* kOSClientCertsModuleName = "OS Client Cert Module";

bool LoadOSClientCertsModule(const nsCString& dir) {
  nsLiteralCString params =
      StaticPrefs::security_osclientcerts_assume_rsa_pss_support()
          ? "RSA-PSS"_ns
          : ""_ns;
  return LoadUserModuleAt(kOSClientCertsModuleName, "osclientcerts", dir,
                          params.get());
}

bool LoadLoadableRoots(const nsCString& dir) {
  // Some NSS command-line utilities will load a roots module under the name
  // "Root Certs" if there happens to be a `MOZ_DLL_PREFIX "nssckbi"
  // MOZ_DLL_SUFFIX` file in the directory being operated on. In some cases this
  // can cause us to fail to load our roots module. In these cases, deleting the
  // "Root Certs" module allows us to load the correct one. See bug 1406396.
  int unusedModType;
  Unused << SECMOD_DeleteModule("Root Certs", &unusedModType);
  return LoadUserModuleAt(kRootModuleName, "nssckbi", dir, nullptr);
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

bool CertIsInCertStorage(const nsTArray<uint8_t>& certDER,
                         nsICertStorage* certStorage) {
  MOZ_ASSERT(certStorage);
  if (!certStorage) {
    return false;
  }
  Input certInput;
  Result rv = certInput.Init(certDER.Elements(), certDER.Length());
  if (rv != Success) {
    return false;
  }
  BackCert cert(certInput, EndEntityOrCA::MustBeCA, nullptr);
  rv = cert.Init();
  if (rv != Success) {
    return false;
  }
  nsTArray<uint8_t> subject;
  subject.AppendElements(cert.GetSubject().UnsafeGetData(),
                         cert.GetSubject().GetLength());
  nsTArray<nsTArray<uint8_t>> certStorageCerts;
  if (NS_FAILED(certStorage->FindCertsBySubject(subject, certStorageCerts))) {
    return false;
  }
  for (const auto& certStorageCert : certStorageCerts) {
    if (certStorageCert.Length() != certDER.Length()) {
      continue;
    }
    if (memcmp(certStorageCert.Elements(), certDER.Elements(),
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
  if (certList.IsEmpty()) {
    return;
  }
  nsTArray<nsTArray<uint8_t>> intermediates;
  // Skip the end-entity; we only want to store intermediates. Similarly,
  // there's no need to save the trust anchor - it's either already a permanent
  // certificate or it's the Microsoft Family Safety root or an enterprise root
  // temporarily imported via the child mode or enterprise root features. We
  // don't want to import these because they're intended to be temporary (and
  // because importing them happens to reset their trust settings, which breaks
  // these features).
  for (size_t index = 1; index < certList.Length() - 1; index++) {
    intermediates.AppendElement(certList.ElementAt(index).Clone());
  }
  nsCOMPtr<nsIRunnable> importCertsRunnable(NS_NewRunnableFunction(
      "IdleSaveIntermediateCerts",
      [intermediates = std::move(intermediates)]() -> void {
        if (AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownConfirmed)) {
          return;
        }
        UniquePK11SlotInfo slot(PK11_GetInternalKeySlot());
        if (!slot) {
          return;
        }
        size_t numCertsImported = 0;
        nsCOMPtr<nsICertStorage> certStorage(
            do_GetService(NS_CERT_STORAGE_CID));
        for (const auto& certDER : intermediates) {
          if (AppShutdown::IsInOrBeyond(ShutdownPhase::AppShutdownConfirmed)) {
            return;
          }
          if (CertIsInCertStorage(certDER, certStorage)) {
            continue;
          }
          SECItem certDERItem = {siBuffer,
                                 const_cast<unsigned char*>(certDER.Elements()),
                                 AssertedCast<unsigned int>(certDER.Length())};
          UniqueCERTCertificate cert(CERT_NewTempCertificate(
              CERT_GetDefaultCertDB(), &certDERItem, nullptr, false, true));
          if (!cert) {
            continue;
          }
          if (cert->slot) {
            // This cert was found on a token; no need to remember it in the
            // permanent database.
            continue;
          }
          PRBool isperm;
          if (CERT_GetCertIsPerm(cert.get(), &isperm) != SECSuccess) {
            continue;
          }
          if (isperm) {
            // We don't need to remember certs already stored in perm db.
            continue;
          }
          // This is a best-effort attempt at avoiding unknown issuer errors
          // in the future, so ignore failures here.
          nsAutoCString nickname;
          if (NS_FAILED(DefaultServerNicknameForCert(cert.get(), nickname))) {
            continue;
          }
          Unused << PK11_ImportCert(slot.get(), cert.get(), CK_INVALID_HANDLE,
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

}  // namespace psm
}  // namespace mozilla
