/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CertVerifier.h"

#include <stdint.h>

#include "CTDiversityPolicy.h"
#include "CTKnownLogs.h"
#include "CTLogVerifier.h"
#include "CSTrustDomain.h"
#include "ExtendedValidation.h"
#include "MultiLogCTVerifier.h"
#include "NSSCertDBTrustDomain.h"
#include "NSSErrorsService.h"
#include "cert.h"
#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Logging.h"
#include "nsNSSComponent.h"
#include "nsPromiseFlatString.h"
#include "nsServiceManagerUtils.h"
#include "pk11pub.h"
#include "mozpkix/pkix.h"
#include "mozpkix/pkixcheck.h"
#include "mozpkix/pkixnss.h"
#include "mozpkix/pkixutil.h"
#include "secmod.h"

using namespace mozilla::ct;
using namespace mozilla::pkix;
using namespace mozilla::psm;

mozilla::LazyLogModule gCertVerifierLog("certverifier");

// Returns the certificate validity period in calendar months (rounded down).
// "extern" to allow unit tests in CTPolicyEnforcerTest.cpp.
extern mozilla::pkix::Result GetCertLifetimeInFullMonths(Time certNotBefore,
                                                         Time certNotAfter,
                                                         size_t& months) {
  if (certNotBefore >= certNotAfter) {
    MOZ_ASSERT_UNREACHABLE("Expected notBefore < notAfter");
    return mozilla::pkix::Result::FATAL_ERROR_INVALID_ARGS;
  }
  uint64_t notBeforeSeconds;
  Result rv = SecondsSinceEpochFromTime(certNotBefore, &notBeforeSeconds);
  if (rv != Success) {
    return rv;
  }
  uint64_t notAfterSeconds;
  rv = SecondsSinceEpochFromTime(certNotAfter, &notAfterSeconds);
  if (rv != Success) {
    return rv;
  }
  // PRTime is microseconds
  PRTime notBeforePR = static_cast<PRTime>(notBeforeSeconds) * 1000000;
  PRTime notAfterPR = static_cast<PRTime>(notAfterSeconds) * 1000000;

  PRExplodedTime explodedNotBefore;
  PRExplodedTime explodedNotAfter;

  PR_ExplodeTime(notBeforePR, PR_LocalTimeParameters, &explodedNotBefore);
  PR_ExplodeTime(notAfterPR, PR_LocalTimeParameters, &explodedNotAfter);

  PRInt32 signedMonths =
      (explodedNotAfter.tm_year - explodedNotBefore.tm_year) * 12 +
      (explodedNotAfter.tm_month - explodedNotBefore.tm_month);
  if (explodedNotAfter.tm_mday < explodedNotBefore.tm_mday) {
    --signedMonths;
  }

  // Can't use `mozilla::AssertedCast<size_t>(signedMonths)` below
  // since it currently generates a warning on Win x64 debug.
  if (signedMonths < 0) {
    MOZ_ASSERT_UNREACHABLE("Expected explodedNotBefore < explodedNotAfter");
    return mozilla::pkix::Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  months = static_cast<size_t>(signedMonths);

  return Success;
}

namespace mozilla {
namespace psm {

const CertVerifier::Flags CertVerifier::FLAG_LOCAL_ONLY = 1;
const CertVerifier::Flags CertVerifier::FLAG_MUST_BE_EV = 2;
const CertVerifier::Flags CertVerifier::FLAG_TLS_IGNORE_STATUS_REQUEST = 4;
static const unsigned int MIN_RSA_BITS = 2048;
static const unsigned int MIN_RSA_BITS_WEAK = 1024;

void CertificateTransparencyInfo::Reset() {
  enabled = false;
  verifyResult.Reset();
  policyCompliance = CTPolicyCompliance::Unknown;
}

CertVerifier::CertVerifier(OcspDownloadConfig odc, OcspStrictConfig osc,
                           mozilla::TimeDuration ocspTimeoutSoft,
                           mozilla::TimeDuration ocspTimeoutHard,
                           uint32_t certShortLifetimeInDays, SHA1Mode sha1Mode,
                           BRNameMatchingPolicy::Mode nameMatchingMode,
                           NetscapeStepUpPolicy netscapeStepUpPolicy,
                           CertificateTransparencyMode ctMode,
                           CRLiteMode crliteMode,
                           uint64_t crliteCTMergeDelaySeconds,
                           const Vector<EnterpriseCert>& thirdPartyCerts)
    : mOCSPDownloadConfig(odc),
      mOCSPStrict(osc == ocspStrict),
      mOCSPTimeoutSoft(ocspTimeoutSoft),
      mOCSPTimeoutHard(ocspTimeoutHard),
      mCertShortLifetimeInDays(certShortLifetimeInDays),
      mSHA1Mode(sha1Mode),
      mNameMatchingMode(nameMatchingMode),
      mNetscapeStepUpPolicy(netscapeStepUpPolicy),
      mCTMode(ctMode),
      mCRLiteMode(crliteMode),
      mCRLiteCTMergeDelaySeconds(crliteCTMergeDelaySeconds) {
  LoadKnownCTLogs();
  for (const auto& root : thirdPartyCerts) {
    EnterpriseCert rootCopy;
    // Best-effort. If we run out of memory, users might see untrusted issuer
    // errors, but the browser will probably crash before then.
    if (NS_SUCCEEDED(rootCopy.Init(root))) {
      Unused << mThirdPartyCerts.append(std::move(rootCopy));
    }
  }
  for (const auto& root : mThirdPartyCerts) {
    Input input;
    if (root.GetInput(input) == Success) {
      // mThirdPartyCerts consists of roots and intermediates.
      if (root.GetIsRoot()) {
        // Best effort again.
        Unused << mThirdPartyRootInputs.append(input);
      } else {
        Unused << mThirdPartyIntermediateInputs.append(input);
      }
    }
  }
}

CertVerifier::~CertVerifier() = default;

Result IsCertChainRootBuiltInRoot(const nsTArray<nsTArray<uint8_t>>& chain,
                                  bool& result) {
  if (chain.IsEmpty()) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  const nsTArray<uint8_t>& rootBytes = chain.LastElement();
  Input rootInput;
  Result rv = rootInput.Init(rootBytes.Elements(), rootBytes.Length());
  if (rv != Result::Success) {
    return rv;
  }
  return IsCertBuiltInRoot(rootInput, result);
}

Result IsDelegatedCredentialAcceptable(const DelegatedCredentialInfo& dcInfo) {
  bool isEcdsa = dcInfo.scheme == ssl_sig_ecdsa_secp256r1_sha256 ||
                 dcInfo.scheme == ssl_sig_ecdsa_secp384r1_sha384 ||
                 dcInfo.scheme == ssl_sig_ecdsa_secp521r1_sha512;

  // Firefox currently does not advertise any RSA schemes for use
  // with Delegated Credentials. As a secondary (on top of NSS)
  // check, disallow any RSA SPKI here. When ssl_sig_rsa_pss_pss_*
  // schemes are supported, check the modulus size and allow RSA here.
  if (!isEcdsa) {
    return Result::ERROR_INVALID_KEY;
  }

  return Result::Success;
}

// The term "builtin root" traditionally refers to a root CA certificate that
// has been added to the NSS trust store, because it has been approved
// for inclusion according to the Mozilla CA policy, and might be accepted
// by Mozilla applications as an issuer for certificates seen on the public web.
Result IsCertBuiltInRoot(Input certInput, bool& result) {
  if (NS_FAILED(BlockUntilLoadableCertsLoaded())) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  CERTCertDBHandle* certDB(CERT_GetDefaultCertDB());
  SECItem certDER(UnsafeMapInputToSECItem(certInput));
  UniqueCERTCertificate cert(
      CERT_NewTempCertificate(certDB, &certDER, nullptr, false, true));
  if (!cert) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  result = false;
#ifdef DEBUG
  nsCOMPtr<nsINSSComponent> component(do_GetService(PSM_COMPONENT_CONTRACTID));
  if (!component) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  nsresult rv = component->IsCertTestBuiltInRoot(cert.get(), &result);
  if (NS_FAILED(rv)) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  if (result) {
    return Success;
  }
#endif  // DEBUG
  AutoSECMODListReadLock lock;
  for (SECMODModuleList* list = SECMOD_GetDefaultModuleList(); list;
       list = list->next) {
    for (int i = 0; i < list->module->slotCount; i++) {
      PK11SlotInfo* slot = list->module->slots[i];
      // We're searching for the "builtin root module", which is a module that
      // contains an object with a CKA_CLASS of CKO_NETSCAPE_BUILTIN_ROOT_LIST.
      // We use PK11_HasRootCerts() to identify a module with that property.
      // In the past, we exclusively used the PKCS#11 module named nssckbi,
      // which is provided by the NSS library.
      // Nowadays, some distributions use a replacement module, which contains
      // the builtin roots, but which also contains additional CA certificates,
      // such as CAs trusted in a local deployment.
      // We want to be able to distinguish between these two categories,
      // because a CA, which may issue certificates for the public web,
      // is expected to comply with additional requirements.
      // If the certificate has attribute CKA_NSS_MOZILLA_CA_POLICY set to true,
      // then we treat it as a "builtin root".
      if (PK11_IsPresent(slot) && PK11_HasRootCerts(slot)) {
        CK_OBJECT_HANDLE handle =
            PK11_FindCertInSlot(slot, cert.get(), nullptr);
        if (handle != CK_INVALID_HANDLE &&
            PK11_HasAttributeSet(slot, handle, CKA_NSS_MOZILLA_CA_POLICY,
                                 false)) {
          // Attribute was found, and is set to true
          result = true;
          break;
        }
      }
    }
  }
  return Success;
}

static Result BuildCertChainForOneKeyUsage(
    NSSCertDBTrustDomain& trustDomain, Input certDER, Time time, KeyUsage ku1,
    KeyUsage ku2, KeyUsage ku3, KeyPurposeId eku,
    const CertPolicyId& requiredPolicy, const Input* stapledOCSPResponse,
    /*optional out*/ CertVerifier::OCSPStaplingStatus* ocspStaplingStatus) {
  trustDomain.ResetAccumulatedState();
  Result rv =
      BuildCertChain(trustDomain, certDER, time, EndEntityOrCA::MustBeEndEntity,
                     ku1, eku, requiredPolicy, stapledOCSPResponse);
  if (rv == Result::ERROR_INADEQUATE_KEY_USAGE) {
    trustDomain.ResetAccumulatedState();
    rv = BuildCertChain(trustDomain, certDER, time,
                        EndEntityOrCA::MustBeEndEntity, ku2, eku,
                        requiredPolicy, stapledOCSPResponse);
    if (rv == Result::ERROR_INADEQUATE_KEY_USAGE) {
      trustDomain.ResetAccumulatedState();
      rv = BuildCertChain(trustDomain, certDER, time,
                          EndEntityOrCA::MustBeEndEntity, ku3, eku,
                          requiredPolicy, stapledOCSPResponse);
      if (rv != Success) {
        rv = Result::ERROR_INADEQUATE_KEY_USAGE;
      }
    }
  }
  if (ocspStaplingStatus) {
    *ocspStaplingStatus = trustDomain.GetOCSPStaplingStatus();
  }
  return rv;
}

void CertVerifier::LoadKnownCTLogs() {
  if (mCTMode == CertificateTransparencyMode::Disabled) {
    return;
  }
  mCTVerifier = MakeUnique<MultiLogCTVerifier>();
  for (const CTLogInfo& log : kCTLogList) {
    Input publicKey;
    Result rv = publicKey.Init(
        BitwiseCast<const uint8_t*, const char*>(log.key), log.keyLength);
    if (rv != Success) {
      MOZ_ASSERT_UNREACHABLE("Failed reading a log key for a known CT Log");
      continue;
    }

    CTLogVerifier logVerifier;
    const CTLogOperatorInfo& logOperator =
        kCTLogOperatorList[log.operatorIndex];
    rv = logVerifier.Init(publicKey, logOperator.id, log.status,
                          log.disqualificationTime);
    if (rv != Success) {
      MOZ_ASSERT_UNREACHABLE("Failed initializing a known CT Log");
      continue;
    }

    mCTVerifier->AddLog(std::move(logVerifier));
  }
  // TBD: Initialize mCTDiversityPolicy with the CA dependency map
  // of the known CT logs operators.
  mCTDiversityPolicy = MakeUnique<CTDiversityPolicy>();
}

Result CertVerifier::VerifyCertificateTransparencyPolicy(
    NSSCertDBTrustDomain& trustDomain,
    const nsTArray<nsTArray<uint8_t>>& builtChain, Input sctsFromTLS, Time time,
    /*optional out*/ CertificateTransparencyInfo* ctInfo) {
  if (ctInfo) {
    ctInfo->Reset();
  }
  if (mCTMode == CertificateTransparencyMode::Disabled) {
    return Success;
  }
  if (ctInfo) {
    ctInfo->enabled = true;
  }

  if (builtChain.IsEmpty()) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }

  Input embeddedSCTs = trustDomain.GetSCTListFromCertificate();
  if (embeddedSCTs.GetLength() > 0) {
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
            ("Got embedded SCT data of length %zu\n",
             static_cast<size_t>(embeddedSCTs.GetLength())));
  }
  Input sctsFromOCSP = trustDomain.GetSCTListFromOCSPStapling();
  if (sctsFromOCSP.GetLength() > 0) {
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
            ("Got OCSP SCT data of length %zu\n",
             static_cast<size_t>(sctsFromOCSP.GetLength())));
  }
  if (sctsFromTLS.GetLength() > 0) {
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
            ("Got TLS SCT data of length %zu\n",
             static_cast<size_t>(sctsFromTLS.GetLength())));
  }

  if (builtChain.Length() == 1) {
    // Issuer certificate is required for SCT verification.
    // If we've arrived here, we probably have a "trust chain" with only one
    // certificate (i.e. a self-signed end-entity that has been set as a trust
    // anchor either by a third party modifying our trust DB or via the
    // enterprise roots feature). If this is the case, certificate transparency
    // information will probably not be present, and it certainly won't verify
    // correctly. To simplify things, we return an empty CTVerifyResult and a
    // "not enough SCTs" CTPolicyCompliance result.
    if (ctInfo) {
      CTVerifyResult emptyResult;
      ctInfo->verifyResult = std::move(emptyResult);
      ctInfo->policyCompliance = CTPolicyCompliance::NotEnoughScts;
    }
    return Success;
  }

  const nsTArray<uint8_t>& endEntityBytes = builtChain.ElementAt(0);
  Input endEntityInput;
  Result rv =
      endEntityInput.Init(endEntityBytes.Elements(), endEntityBytes.Length());
  if (rv != Success) {
    return rv;
  }

  const nsTArray<uint8_t>& issuerBytes = builtChain.ElementAt(1);
  Input issuerInput;
  rv = issuerInput.Init(issuerBytes.Elements(), issuerBytes.Length());
  if (rv != Success) {
    return rv;
  }

  BackCert issuerBackCert(issuerInput, EndEntityOrCA::MustBeCA, nullptr);
  rv = issuerBackCert.Init();
  if (rv != Success) {
    return rv;
  }
  Input issuerPublicKeyInput = issuerBackCert.GetSubjectPublicKeyInfo();

  CTVerifyResult result;
  rv = mCTVerifier->Verify(endEntityInput, issuerPublicKeyInput, embeddedSCTs,
                           sctsFromOCSP, sctsFromTLS, time, result);
  if (rv != Success) {
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
            ("SCT verification failed with fatal error %" PRId32 "\n",
             static_cast<uint32_t>(rv)));
    return rv;
  }

  if (MOZ_LOG_TEST(gCertVerifierLog, LogLevel::Debug)) {
    size_t validCount = 0;
    size_t unknownLogCount = 0;
    size_t disqualifiedLogCount = 0;
    size_t invalidSignatureCount = 0;
    size_t invalidTimestampCount = 0;
    for (const VerifiedSCT& verifiedSct : result.verifiedScts) {
      switch (verifiedSct.status) {
        case VerifiedSCT::Status::Valid:
          validCount++;
          break;
        case VerifiedSCT::Status::ValidFromDisqualifiedLog:
          disqualifiedLogCount++;
          break;
        case VerifiedSCT::Status::UnknownLog:
          unknownLogCount++;
          break;
        case VerifiedSCT::Status::InvalidSignature:
          invalidSignatureCount++;
          break;
        case VerifiedSCT::Status::InvalidTimestamp:
          invalidTimestampCount++;
          break;
        case VerifiedSCT::Status::None:
        default:
          MOZ_ASSERT_UNREACHABLE("Unexpected SCT verification status");
      }
    }
    MOZ_LOG(
        gCertVerifierLog, LogLevel::Debug,
        ("SCT verification result: "
         "valid=%zu unknownLog=%zu disqualifiedLog=%zu "
         "invalidSignature=%zu invalidTimestamp=%zu "
         "decodingErrors=%zu\n",
         validCount, unknownLogCount, disqualifiedLogCount,
         invalidSignatureCount, invalidTimestampCount, result.decodingErrors));
  }

  BackCert endEntityBackCert(endEntityInput, EndEntityOrCA::MustBeEndEntity,
                             nullptr);
  rv = endEntityBackCert.Init();
  if (rv != Success) {
    return rv;
  }
  Time notBefore(Time::uninitialized);
  Time notAfter(Time::uninitialized);
  rv = ParseValidity(endEntityBackCert.GetValidity(), &notBefore, &notAfter);
  if (rv != Success) {
    return rv;
  }
  size_t lifetimeInMonths;
  rv = GetCertLifetimeInFullMonths(notBefore, notAfter, lifetimeInMonths);
  if (rv != Success) {
    return rv;
  }

  CTLogOperatorList allOperators;
  GetCTLogOperatorsFromVerifiedSCTList(result.verifiedScts, allOperators);

  CTLogOperatorList dependentOperators;
  rv = mCTDiversityPolicy->GetDependentOperators(builtChain, allOperators,
                                                 dependentOperators);
  if (rv != Success) {
    return rv;
  }

  CTPolicyEnforcer ctPolicyEnforcer;
  CTPolicyCompliance ctPolicyCompliance;
  ctPolicyEnforcer.CheckCompliance(result.verifiedScts, lifetimeInMonths,
                                   dependentOperators, ctPolicyCompliance);

  if (ctInfo) {
    ctInfo->verifyResult = std::move(result);
    ctInfo->policyCompliance = ctPolicyCompliance;
  }
  return Success;
}

bool CertVerifier::SHA1ModeMoreRestrictiveThanGivenMode(SHA1Mode mode) {
  switch (mSHA1Mode) {
    case SHA1Mode::Forbidden:
      return mode != SHA1Mode::Forbidden;
    case SHA1Mode::ImportedRoot:
      return mode != SHA1Mode::Forbidden && mode != SHA1Mode::ImportedRoot;
    case SHA1Mode::ImportedRootOrBefore2016:
      return mode == SHA1Mode::Allowed;
    case SHA1Mode::Allowed:
      return false;
    // MSVC warns unless we explicitly handle this now-unused option.
    case SHA1Mode::UsedToBeBefore2016ButNowIsForbidden:
    default:
      MOZ_ASSERT(false, "unexpected SHA1Mode type");
      return true;
  }
}

Result CertVerifier::VerifyCert(
    const nsTArray<uint8_t>& certBytes, SECCertificateUsage usage, Time time,
    void* pinArg, const char* hostname,
    /*out*/ nsTArray<nsTArray<uint8_t>>& builtChain,
    /*optional*/ const Flags flags,
    /*optional*/ const Maybe<nsTArray<nsTArray<uint8_t>>>& extraCertificates,
    /*optional*/ const Maybe<nsTArray<uint8_t>>& stapledOCSPResponseArg,
    /*optional*/ const Maybe<nsTArray<uint8_t>>& sctsFromTLS,
    /*optional*/ const OriginAttributes& originAttributes,
    /*optional out*/ EVStatus* evStatus,
    /*optional out*/ OCSPStaplingStatus* ocspStaplingStatus,
    /*optional out*/ KeySizeStatus* keySizeStatus,
    /*optional out*/ SHA1ModeResult* sha1ModeResult,
    /*optional out*/ PinningTelemetryInfo* pinningTelemetryInfo,
    /*optional out*/ CertificateTransparencyInfo* ctInfo) {
  MOZ_LOG(gCertVerifierLog, LogLevel::Debug, ("Top of VerifyCert\n"));

  MOZ_ASSERT(usage == certificateUsageSSLServer || !(flags & FLAG_MUST_BE_EV));
  MOZ_ASSERT(usage == certificateUsageSSLServer || !keySizeStatus);
  MOZ_ASSERT(usage == certificateUsageSSLServer || !sha1ModeResult);

  if (NS_FAILED(BlockUntilLoadableCertsLoaded())) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  if (NS_FAILED(CheckForSmartCardChanges())) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }

  if (evStatus) {
    *evStatus = EVStatus::NotEV;
  }
  if (ocspStaplingStatus) {
    if (usage != certificateUsageSSLServer) {
      return Result::FATAL_ERROR_INVALID_ARGS;
    }
    *ocspStaplingStatus = OCSP_STAPLING_NEVER_CHECKED;
  }

  if (keySizeStatus) {
    if (usage != certificateUsageSSLServer) {
      return Result::FATAL_ERROR_INVALID_ARGS;
    }
    *keySizeStatus = KeySizeStatus::NeverChecked;
  }

  if (sha1ModeResult) {
    if (usage != certificateUsageSSLServer) {
      return Result::FATAL_ERROR_INVALID_ARGS;
    }
    *sha1ModeResult = SHA1ModeResult::NeverChecked;
  }

  if (usage != certificateUsageSSLServer && (flags & FLAG_MUST_BE_EV)) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }

  Input certDER;
  Result rv = certDER.Init(certBytes.Elements(), certBytes.Length());
  if (rv != Success) {
    return rv;
  }

  // We configure the OCSP fetching modes separately for EV and non-EV
  // verifications.
  NSSCertDBTrustDomain::OCSPFetching defaultOCSPFetching =
      (mOCSPDownloadConfig == ocspOff) || (mOCSPDownloadConfig == ocspEVOnly) ||
              (flags & FLAG_LOCAL_ONLY)
          ? NSSCertDBTrustDomain::NeverFetchOCSP
      : !mOCSPStrict ? NSSCertDBTrustDomain::FetchOCSPForDVSoftFail
                     : NSSCertDBTrustDomain::FetchOCSPForDVHardFail;

  Input stapledOCSPResponseInput;
  const Input* stapledOCSPResponse = nullptr;
  if (stapledOCSPResponseArg) {
    rv = stapledOCSPResponseInput.Init(stapledOCSPResponseArg->Elements(),
                                       stapledOCSPResponseArg->Length());
    if (rv != Success) {
      // The stapled OCSP response was too big.
      return Result::ERROR_OCSP_MALFORMED_RESPONSE;
    }
    stapledOCSPResponse = &stapledOCSPResponseInput;
  }

  Input sctsFromTLSInput;
  if (sctsFromTLS) {
    rv = sctsFromTLSInput.Init(sctsFromTLS->Elements(), sctsFromTLS->Length());
    if (rv != Success && sctsFromTLSInput.GetLength() != 0) {
      return Result::FATAL_ERROR_LIBRARY_FAILURE;
    }
  }

  switch (usage) {
    case certificateUsageSSLClient: {
      // XXX: We don't really have a trust bit for SSL client authentication so
      // just use trustEmail as it is the closest alternative.
      NSSCertDBTrustDomain trustDomain(
          trustEmail, defaultOCSPFetching, mOCSPCache, pinArg, mOCSPTimeoutSoft,
          mOCSPTimeoutHard, mCertShortLifetimeInDays, MIN_RSA_BITS_WEAK,
          ValidityCheckingMode::CheckingOff, SHA1Mode::Allowed,
          NetscapeStepUpPolicy::NeverMatch, mCRLiteMode,
          mCRLiteCTMergeDelaySeconds, originAttributes, mThirdPartyRootInputs,
          mThirdPartyIntermediateInputs, extraCertificates, builtChain, nullptr,
          nullptr);
      rv = BuildCertChain(
          trustDomain, certDER, time, EndEntityOrCA::MustBeEndEntity,
          KeyUsage::digitalSignature, KeyPurposeId::id_kp_clientAuth,
          CertPolicyId::anyPolicy, stapledOCSPResponse);
      break;
    }

    case certificateUsageSSLServer: {
      // TODO: When verifying a certificate in an SSL handshake, we should
      // restrict the acceptable key usage based on the key exchange method
      // chosen by the server.

      // These configurations are in order of most restrictive to least
      // restrictive. This enables us to gather telemetry on the expected
      // results of setting the default policy to a particular configuration.
      SHA1Mode sha1ModeConfigurations[] = {
          SHA1Mode::Forbidden,
          SHA1Mode::ImportedRoot,
          SHA1Mode::ImportedRootOrBefore2016,
          SHA1Mode::Allowed,
      };

      SHA1ModeResult sha1ModeResults[] = {
          SHA1ModeResult::SucceededWithoutSHA1,
          SHA1ModeResult::SucceededWithImportedRoot,
          SHA1ModeResult::SucceededWithImportedRootOrSHA1Before2016,
          SHA1ModeResult::SucceededWithSHA1,
      };

      size_t sha1ModeConfigurationsCount =
          MOZ_ARRAY_LENGTH(sha1ModeConfigurations);

      static_assert(MOZ_ARRAY_LENGTH(sha1ModeConfigurations) ==
                        MOZ_ARRAY_LENGTH(sha1ModeResults),
                    "digestAlgorithm array lengths differ");

      // Try to validate for EV first.
      NSSCertDBTrustDomain::OCSPFetching evOCSPFetching =
          (mOCSPDownloadConfig == ocspOff) || (flags & FLAG_LOCAL_ONLY)
              ? NSSCertDBTrustDomain::LocalOnlyOCSPForEV
              : NSSCertDBTrustDomain::FetchOCSPForEV;

      CertPolicyId evPolicy;
      bool foundEVPolicy = GetFirstEVPolicy(certBytes, evPolicy);
      rv = Result::ERROR_UNKNOWN_ERROR;
      for (size_t i = 0;
           i < sha1ModeConfigurationsCount && rv != Success && foundEVPolicy;
           i++) {
        // Don't attempt verification if the SHA1 mode set by preferences
        // (mSHA1Mode) is more restrictive than the SHA1 mode option we're on.
        // (To put it another way, only attempt verification if the SHA1 mode
        // option we're on is as restrictive or more restrictive than
        // mSHA1Mode.) This allows us to gather telemetry information while
        // still enforcing the mode set by preferences.
        if (SHA1ModeMoreRestrictiveThanGivenMode(sha1ModeConfigurations[i])) {
          continue;
        }

        // Because of the try-strict and fallback approach, we have to clear any
        // previously noted telemetry information.
        if (pinningTelemetryInfo) {
          pinningTelemetryInfo->Reset();
        }

        NSSCertDBTrustDomain trustDomain(
            trustSSL, evOCSPFetching, mOCSPCache, pinArg, mOCSPTimeoutSoft,
            mOCSPTimeoutHard, mCertShortLifetimeInDays, MIN_RSA_BITS,
            ValidityCheckingMode::CheckForEV, sha1ModeConfigurations[i],
            mNetscapeStepUpPolicy, mCRLiteMode, mCRLiteCTMergeDelaySeconds,
            originAttributes, mThirdPartyRootInputs,
            mThirdPartyIntermediateInputs, extraCertificates, builtChain,
            pinningTelemetryInfo, hostname);
        rv = BuildCertChainForOneKeyUsage(
            trustDomain, certDER, time,
            KeyUsage::digitalSignature,  // (EC)DHE
            KeyUsage::keyEncipherment,   // RSA
            KeyUsage::keyAgreement,      // (EC)DH
            KeyPurposeId::id_kp_serverAuth, evPolicy, stapledOCSPResponse,
            ocspStaplingStatus);
        if (rv == Success &&
            sha1ModeConfigurations[i] == SHA1Mode::ImportedRoot) {
          bool isBuiltInRoot = false;
          rv = IsCertChainRootBuiltInRoot(builtChain, isBuiltInRoot);
          if (rv != Success) {
            break;
          }
          if (isBuiltInRoot) {
            rv = Result::ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED;
          }
        }
        if (rv == Success) {
          MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
                  ("cert is EV with status %i\n",
                   static_cast<int>(sha1ModeResults[i])));
          if (evStatus) {
            *evStatus = foundEVPolicy ? EVStatus::EV : EVStatus::NotEV;
          }
          if (sha1ModeResult) {
            *sha1ModeResult = sha1ModeResults[i];
          }
          rv = VerifyCertificateTransparencyPolicy(
              trustDomain, builtChain, sctsFromTLSInput, time, ctInfo);
          if (rv != Success) {
            break;
          }
        }
      }
      if (rv == Success) {
        break;
      }

      if (flags & FLAG_MUST_BE_EV) {
        rv = Result::ERROR_POLICY_VALIDATION_FAILED;
        break;
      }

      // Now try non-EV.
      unsigned int keySizeOptions[] = {MIN_RSA_BITS, MIN_RSA_BITS_WEAK};

      KeySizeStatus keySizeStatuses[] = {KeySizeStatus::LargeMinimumSucceeded,
                                         KeySizeStatus::CompatibilityRisk};

      static_assert(
          MOZ_ARRAY_LENGTH(keySizeOptions) == MOZ_ARRAY_LENGTH(keySizeStatuses),
          "keySize array lengths differ");

      size_t keySizeOptionsCount = MOZ_ARRAY_LENGTH(keySizeStatuses);

      for (size_t i = 0; i < keySizeOptionsCount && rv != Success; i++) {
        for (size_t j = 0; j < sha1ModeConfigurationsCount && rv != Success;
             j++) {
          // Don't attempt verification if the SHA1 mode set by preferences
          // (mSHA1Mode) is more restrictive than the SHA1 mode option we're on.
          // (To put it another way, only attempt verification if the SHA1 mode
          // option we're on is as restrictive or more restrictive than
          // mSHA1Mode.) This allows us to gather telemetry information while
          // still enforcing the mode set by preferences.
          if (SHA1ModeMoreRestrictiveThanGivenMode(sha1ModeConfigurations[j])) {
            continue;
          }

          // invalidate any telemetry info relating to failed chains
          if (pinningTelemetryInfo) {
            pinningTelemetryInfo->Reset();
          }

          NSSCertDBTrustDomain trustDomain(
              trustSSL, defaultOCSPFetching, mOCSPCache, pinArg,
              mOCSPTimeoutSoft, mOCSPTimeoutHard, mCertShortLifetimeInDays,
              keySizeOptions[i], ValidityCheckingMode::CheckingOff,
              sha1ModeConfigurations[j], mNetscapeStepUpPolicy, mCRLiteMode,
              mCRLiteCTMergeDelaySeconds, originAttributes,
              mThirdPartyRootInputs, mThirdPartyIntermediateInputs,
              extraCertificates, builtChain, pinningTelemetryInfo, hostname);
          rv = BuildCertChainForOneKeyUsage(
              trustDomain, certDER, time,
              KeyUsage::digitalSignature,  //(EC)DHE
              KeyUsage::keyEncipherment,   // RSA
              KeyUsage::keyAgreement,      //(EC)DH
              KeyPurposeId::id_kp_serverAuth, CertPolicyId::anyPolicy,
              stapledOCSPResponse, ocspStaplingStatus);
          if (rv != Success && !IsFatalError(rv) &&
              rv != Result::ERROR_REVOKED_CERTIFICATE &&
              trustDomain.GetIsErrorDueToDistrustedCAPolicy()) {
            // Bug 1444440 - If there are multiple paths, at least one to a CA
            // distrusted-by-policy, and none of them ending in a trusted root,
            // then we might show a different error (UNKNOWN_ISSUER) than we
            // intend, confusing users.
            rv = Result::ERROR_ADDITIONAL_POLICY_CONSTRAINT_FAILED;
          }
          if (rv == Success &&
              sha1ModeConfigurations[j] == SHA1Mode::ImportedRoot) {
            bool isBuiltInRoot = false;
            rv = IsCertChainRootBuiltInRoot(builtChain, isBuiltInRoot);
            if (rv != Success) {
              break;
            }
            if (isBuiltInRoot) {
              rv = Result::ERROR_CERT_SIGNATURE_ALGORITHM_DISABLED;
            }
          }
          if (rv == Success) {
            if (keySizeStatus) {
              *keySizeStatus = keySizeStatuses[i];
            }
            if (sha1ModeResult) {
              *sha1ModeResult = sha1ModeResults[j];
            }
            rv = VerifyCertificateTransparencyPolicy(
                trustDomain, builtChain, sctsFromTLSInput, time, ctInfo);
            if (rv != Success) {
              break;
            }
          }
        }
      }

      if (rv == Success) {
        break;
      }

      if (keySizeStatus) {
        *keySizeStatus = KeySizeStatus::AlreadyBad;
      }
      // The telemetry probe CERT_CHAIN_SHA1_POLICY_STATUS gives us feedback on
      // the result of setting a specific policy. However, we don't want noise
      // from users who have manually set the policy to something other than the
      // default, so we only collect for ImportedRoot (which is the default).
      if (sha1ModeResult && mSHA1Mode == SHA1Mode::ImportedRoot) {
        *sha1ModeResult = SHA1ModeResult::Failed;
      }

      break;
    }

    case certificateUsageSSLCA: {
      NSSCertDBTrustDomain trustDomain(
          trustSSL, defaultOCSPFetching, mOCSPCache, pinArg, mOCSPTimeoutSoft,
          mOCSPTimeoutHard, mCertShortLifetimeInDays, MIN_RSA_BITS_WEAK,
          ValidityCheckingMode::CheckingOff, SHA1Mode::Allowed,
          mNetscapeStepUpPolicy, mCRLiteMode, mCRLiteCTMergeDelaySeconds,
          originAttributes, mThirdPartyRootInputs,
          mThirdPartyIntermediateInputs, extraCertificates, builtChain, nullptr,
          nullptr);
      rv = BuildCertChain(trustDomain, certDER, time, EndEntityOrCA::MustBeCA,
                          KeyUsage::keyCertSign, KeyPurposeId::id_kp_serverAuth,
                          CertPolicyId::anyPolicy, stapledOCSPResponse);
      break;
    }

    case certificateUsageEmailSigner: {
      NSSCertDBTrustDomain trustDomain(
          trustEmail, defaultOCSPFetching, mOCSPCache, pinArg, mOCSPTimeoutSoft,
          mOCSPTimeoutHard, mCertShortLifetimeInDays, MIN_RSA_BITS_WEAK,
          ValidityCheckingMode::CheckingOff, SHA1Mode::Allowed,
          NetscapeStepUpPolicy::NeverMatch, mCRLiteMode,
          mCRLiteCTMergeDelaySeconds, originAttributes, mThirdPartyRootInputs,
          mThirdPartyIntermediateInputs, extraCertificates, builtChain, nullptr,
          nullptr);
      rv = BuildCertChain(
          trustDomain, certDER, time, EndEntityOrCA::MustBeEndEntity,
          KeyUsage::digitalSignature, KeyPurposeId::id_kp_emailProtection,
          CertPolicyId::anyPolicy, stapledOCSPResponse);
      if (rv == Result::ERROR_INADEQUATE_KEY_USAGE) {
        rv = BuildCertChain(
            trustDomain, certDER, time, EndEntityOrCA::MustBeEndEntity,
            KeyUsage::nonRepudiation, KeyPurposeId::id_kp_emailProtection,
            CertPolicyId::anyPolicy, stapledOCSPResponse);
      }
      break;
    }

    case certificateUsageEmailRecipient: {
      // TODO: The higher level S/MIME processing should pass in which key
      // usage it is trying to verify for, and base its algorithm choices
      // based on the result of the verification(s).
      NSSCertDBTrustDomain trustDomain(
          trustEmail, defaultOCSPFetching, mOCSPCache, pinArg, mOCSPTimeoutSoft,
          mOCSPTimeoutHard, mCertShortLifetimeInDays, MIN_RSA_BITS_WEAK,
          ValidityCheckingMode::CheckingOff, SHA1Mode::Allowed,
          NetscapeStepUpPolicy::NeverMatch, mCRLiteMode,
          mCRLiteCTMergeDelaySeconds, originAttributes, mThirdPartyRootInputs,
          mThirdPartyIntermediateInputs, extraCertificates, builtChain, nullptr,
          nullptr);
      rv = BuildCertChain(trustDomain, certDER, time,
                          EndEntityOrCA::MustBeEndEntity,
                          KeyUsage::keyEncipherment,  // RSA
                          KeyPurposeId::id_kp_emailProtection,
                          CertPolicyId::anyPolicy, stapledOCSPResponse);
      if (rv == Result::ERROR_INADEQUATE_KEY_USAGE) {
        rv = BuildCertChain(trustDomain, certDER, time,
                            EndEntityOrCA::MustBeEndEntity,
                            KeyUsage::keyAgreement,  // ECDH/DH
                            KeyPurposeId::id_kp_emailProtection,
                            CertPolicyId::anyPolicy, stapledOCSPResponse);
      }
      break;
    }

    default:
      rv = Result::FATAL_ERROR_INVALID_ARGS;
  }

  if (rv != Success) {
    return rv;
  }

  return Success;
}

static bool CertIsSelfSigned(const BackCert& backCert, void* pinarg) {
  if (!InputsAreEqual(backCert.GetIssuer(), backCert.GetSubject())) {
    return false;
  }

  nsTArray<nsTArray<uint8_t>> emptyCertList;
  // CSTrustDomain is only used for the signature verification callbacks
  mozilla::psm::CSTrustDomain trustDomain(emptyCertList);
  Result rv = VerifySignedData(trustDomain, backCert.GetSignedData(),
                               backCert.GetSubjectPublicKeyInfo());
  return rv == Success;
}

Result CertVerifier::VerifySSLServerCert(
    const nsTArray<uint8_t>& peerCertBytes, Time time,
    /*optional*/ void* pinarg, const nsACString& hostname,
    /*out*/ nsTArray<nsTArray<uint8_t>>& builtChain,
    /*optional*/ Flags flags,
    /*optional*/ const Maybe<nsTArray<nsTArray<uint8_t>>>& extraCertificates,
    /*optional*/ const Maybe<nsTArray<uint8_t>>& stapledOCSPResponse,
    /*optional*/ const Maybe<nsTArray<uint8_t>>& sctsFromTLS,
    /*optional*/ const Maybe<DelegatedCredentialInfo>& dcInfo,
    /*optional*/ const OriginAttributes& originAttributes,
    /*optional out*/ EVStatus* evStatus,
    /*optional out*/ OCSPStaplingStatus* ocspStaplingStatus,
    /*optional out*/ KeySizeStatus* keySizeStatus,
    /*optional out*/ SHA1ModeResult* sha1ModeResult,
    /*optional out*/ PinningTelemetryInfo* pinningTelemetryInfo,
    /*optional out*/ CertificateTransparencyInfo* ctInfo,
    /*optional out*/ bool* isBuiltCertChainRootBuiltInRoot) {
  // XXX: MOZ_ASSERT(pinarg);
  MOZ_ASSERT(!hostname.IsEmpty());

  if (isBuiltCertChainRootBuiltInRoot) {
    *isBuiltCertChainRootBuiltInRoot = false;
  }

  if (evStatus) {
    *evStatus = EVStatus::NotEV;
  }

  if (hostname.IsEmpty()) {
    return Result::ERROR_BAD_CERT_DOMAIN;
  }

  // CreateCertErrorRunnable assumes that CheckCertHostname is only called
  // if VerifyCert succeeded.
  Input peerCertInput;
  Result rv =
      peerCertInput.Init(peerCertBytes.Elements(), peerCertBytes.Length());
  if (rv != Success) {
    return rv;
  }
  rv = VerifyCert(peerCertBytes, certificateUsageSSLServer, time, pinarg,
                  PromiseFlatCString(hostname).get(), builtChain, flags,
                  extraCertificates, stapledOCSPResponse, sctsFromTLS,
                  originAttributes, evStatus, ocspStaplingStatus, keySizeStatus,
                  sha1ModeResult, pinningTelemetryInfo, ctInfo);
  if (rv != Success) {
    // we don't use the certificate for path building, so this parameter doesn't
    // matter
    EndEntityOrCA notUsedForPaths = EndEntityOrCA::MustBeEndEntity;
    BackCert peerBackCert(peerCertInput, notUsedForPaths, nullptr);
    if (peerBackCert.Init() != Success) {
      return rv;
    }
    if (rv == Result::ERROR_UNKNOWN_ISSUER &&
        CertIsSelfSigned(peerBackCert, pinarg)) {
      // In this case we didn't find any issuer for the certificate and the
      // certificate is self-signed.
      return Result::ERROR_SELF_SIGNED_CERT;
    }
    if (rv == Result::ERROR_UNKNOWN_ISSUER) {
      // In this case we didn't get any valid path for the cert. Let's see if
      // the issuer is the same as the issuer for our canary probe. If yes, this
      // connection is connecting via a misconfigured proxy.
      // Note: The MitM canary might not be set. In this case we consider this
      // an unknown issuer error.
      nsCOMPtr<nsINSSComponent> component(
          do_GetService(PSM_COMPONENT_CONTRACTID));
      if (!component) {
        return Result::FATAL_ERROR_LIBRARY_FAILURE;
      }
      // IssuerMatchesMitmCanary succeeds if the issuer matches the canary and
      // the feature is enabled.
      Input issuerNameInput = peerBackCert.GetIssuer();
      SECItem issuerNameItem = UnsafeMapInputToSECItem(issuerNameInput);
      UniquePORTString issuerName(CERT_DerNameToAscii(&issuerNameItem));
      if (!issuerName) {
        return Result::ERROR_BAD_DER;
      }
      nsresult rv = component->IssuerMatchesMitmCanary(issuerName.get());
      if (NS_SUCCEEDED(rv)) {
        return Result::ERROR_MITM_DETECTED;
      }
    }
    return rv;
  }

  if (dcInfo) {
    rv = IsDelegatedCredentialAcceptable(*dcInfo);
    if (rv != Success) {
      return rv;
    }
  }

  Input stapledOCSPResponseInput;
  Input* responseInputPtr = nullptr;
  if (stapledOCSPResponse) {
    rv = stapledOCSPResponseInput.Init(stapledOCSPResponse->Elements(),
                                       stapledOCSPResponse->Length());
    if (rv != Success) {
      // The stapled OCSP response was too big.
      return Result::ERROR_OCSP_MALFORMED_RESPONSE;
    }
    responseInputPtr = &stapledOCSPResponseInput;
  }

  if (!(flags & FLAG_TLS_IGNORE_STATUS_REQUEST)) {
    rv = CheckTLSFeaturesAreSatisfied(peerCertInput, responseInputPtr);
    if (rv != Success) {
      return rv;
    }
  }

  Input hostnameInput;
  rv = hostnameInput.Init(
      BitwiseCast<const uint8_t*, const char*>(hostname.BeginReading()),
      hostname.Length());
  if (rv != Success) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }
  bool isBuiltInRoot;
  rv = IsCertChainRootBuiltInRoot(builtChain, isBuiltInRoot);
  if (rv != Success) {
    return rv;
  }

  if (isBuiltCertChainRootBuiltInRoot) {
    *isBuiltCertChainRootBuiltInRoot = isBuiltInRoot;
  }

  BRNameMatchingPolicy nameMatchingPolicy(
      isBuiltInRoot ? mNameMatchingMode
                    : BRNameMatchingPolicy::Mode::DoNotEnforce);
  rv = CheckCertHostname(peerCertInput, hostnameInput, nameMatchingPolicy);
  if (rv != Success) {
    // Treat malformed name information as a domain mismatch.
    if (rv == Result::ERROR_BAD_DER) {
      return Result::ERROR_BAD_CERT_DOMAIN;
    }

    return rv;
  }

  return Success;
}

}  // namespace psm
}  // namespace mozilla
