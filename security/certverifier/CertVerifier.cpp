/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CertVerifier.h"

#include <stdint.h>

#include "CTKnownLogs.h"
#include "CTLogVerifier.h"
#include "ExtendedValidation.h"
#include "MultiLogCTVerifier.h"
#include "NSSCertDBTrustDomain.h"
#include "NSSErrorsService.h"
#include "cert.h"
#include "mozilla/Assertions.h"
#include "mozilla/Casting.h"
#include "nsNSSComponent.h"
#include "nsServiceManagerUtils.h"
#include "pk11pub.h"
#include "pkix/pkix.h"
#include "pkix/pkixnss.h"
#include "prerror.h"
#include "secerr.h"
#include "secmod.h"
#include "sslerr.h"

using namespace mozilla::ct;
using namespace mozilla::pkix;
using namespace mozilla::psm;

mozilla::LazyLogModule gCertVerifierLog("certverifier");

namespace mozilla { namespace psm {

const CertVerifier::Flags CertVerifier::FLAG_LOCAL_ONLY = 1;
const CertVerifier::Flags CertVerifier::FLAG_MUST_BE_EV = 2;
const CertVerifier::Flags CertVerifier::FLAG_TLS_IGNORE_STATUS_REQUEST = 4;

CertVerifier::CertVerifier(OcspDownloadConfig odc,
                           OcspStrictConfig osc,
                           OcspGetConfig ogc,
                           uint32_t certShortLifetimeInDays,
                           PinningMode pinningMode,
                           SHA1Mode sha1Mode,
                           BRNameMatchingPolicy::Mode nameMatchingMode,
                           NetscapeStepUpPolicy netscapeStepUpPolicy,
                           CertificateTransparencyMode ctMode)
  : mOCSPDownloadConfig(odc)
  , mOCSPStrict(osc == ocspStrict)
  , mOCSPGETEnabled(ogc == ocspGetEnabled)
  , mCertShortLifetimeInDays(certShortLifetimeInDays)
  , mPinningMode(pinningMode)
  , mSHA1Mode(sha1Mode)
  , mNameMatchingMode(nameMatchingMode)
  , mNetscapeStepUpPolicy(netscapeStepUpPolicy)
  , mCTMode(ctMode)
{
  LoadKnownCTLogs();
}

CertVerifier::~CertVerifier()
{
}

Result
IsCertChainRootBuiltInRoot(const UniqueCERTCertList& chain, bool& result)
{
  if (!chain || CERT_LIST_EMPTY(chain)) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  CERTCertListNode* rootNode = CERT_LIST_TAIL(chain);
  if (!rootNode) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  CERTCertificate* root = rootNode->cert;
  if (!root) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  return IsCertBuiltInRoot(root, result);
}

Result
IsCertBuiltInRoot(CERTCertificate* cert, bool& result)
{
  result = false;
#ifdef DEBUG
  nsCOMPtr<nsINSSComponent> component(do_GetService(PSM_COMPONENT_CONTRACTID));
  if (!component) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  nsresult rv = component->IsCertTestBuiltInRoot(cert, result);
  if (NS_FAILED(rv)) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  if (result) {
    return Success;
  }
#endif // DEBUG
  AutoSECMODListReadLock lock;
  for (SECMODModuleList* list = SECMOD_GetDefaultModuleList(); list;
       list = list->next) {
    for (int i = 0; i < list->module->slotCount; i++) {
      PK11SlotInfo* slot = list->module->slots[i];
      // PK11_HasRootCerts should return true if and only if the given slot has
      // an object with a CKA_CLASS of CKO_NETSCAPE_BUILTIN_ROOT_LIST, which
      // should be true only of the builtin root list.
      // If we can find a copy of the given certificate on the slot with the
      // builtin root list, that certificate must be a builtin.
      if (PK11_IsPresent(slot) && PK11_HasRootCerts(slot) &&
          PK11_FindCertInSlot(slot, cert, nullptr) != CK_INVALID_HANDLE) {
        result = true;
        return Success;
      }
    }
  }
  return Success;
}

static Result
BuildCertChainForOneKeyUsage(NSSCertDBTrustDomain& trustDomain, Input certDER,
                             Time time, KeyUsage ku1, KeyUsage ku2,
                             KeyUsage ku3, KeyPurposeId eku,
                             const CertPolicyId& requiredPolicy,
                             const Input* stapledOCSPResponse,
                             /*optional out*/ CertVerifier::OCSPStaplingStatus*
                                                ocspStaplingStatus)
{
  trustDomain.ResetAccumulatedState();
  Result rv = BuildCertChain(trustDomain, certDER, time,
                             EndEntityOrCA::MustBeEndEntity, ku1,
                             eku, requiredPolicy, stapledOCSPResponse);
  if (rv == Result::ERROR_INADEQUATE_KEY_USAGE) {
    trustDomain.ResetAccumulatedState();
    rv = BuildCertChain(trustDomain, certDER, time,
                        EndEntityOrCA::MustBeEndEntity, ku2,
                        eku, requiredPolicy, stapledOCSPResponse);
    if (rv == Result::ERROR_INADEQUATE_KEY_USAGE) {
      trustDomain.ResetAccumulatedState();
      rv = BuildCertChain(trustDomain, certDER, time,
                          EndEntityOrCA::MustBeEndEntity, ku3,
                          eku, requiredPolicy, stapledOCSPResponse);
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

void
CertVerifier::LoadKnownCTLogs()
{
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

    rv = mCTVerifier->AddLog(Move(logVerifier));
    if (rv != Success) {
      MOZ_ASSERT_UNREACHABLE("Failed activating a known CT Log");
      continue;
    }
  }
}

Result
CertVerifier::VerifySignedCertificateTimestamps(
  NSSCertDBTrustDomain& trustDomain, const UniqueCERTCertList& builtChain,
  Input sctsFromTLS, Time time,
  /*optional out*/ CertificateTransparencyInfo* ctInfo)
{
  if (ctInfo) {
    ctInfo->Reset();
  }
  if (mCTMode == CertificateTransparencyMode::Disabled) {
    return Success;
  }
  if (ctInfo) {
    ctInfo->enabled = true;
  }

  if (!builtChain || CERT_LIST_EMPTY(builtChain)) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }

  bool gotScts = false;
  Input embeddedSCTs = trustDomain.GetSCTListFromCertificate();
  if (embeddedSCTs.GetLength() > 0) {
    gotScts = true;
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
            ("Got embedded SCT data of length %zu\n",
              static_cast<size_t>(embeddedSCTs.GetLength())));
  }
  Input sctsFromOCSP = trustDomain.GetSCTListFromOCSPStapling();
  if (sctsFromOCSP.GetLength() > 0) {
    gotScts = true;
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
            ("Got OCSP SCT data of length %zu\n",
              static_cast<size_t>(sctsFromOCSP.GetLength())));
  }
  if (sctsFromTLS.GetLength() > 0) {
    gotScts = true;
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
            ("Got TLS SCT data of length %zu\n",
              static_cast<size_t>(sctsFromTLS.GetLength())));
  }
  if (!gotScts) {
    return Success;
  }

  CERTCertListNode* endEntityNode = CERT_LIST_HEAD(builtChain);
  if (!endEntityNode) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }
  CERTCertListNode* issuerNode = CERT_LIST_NEXT(endEntityNode);
  if (!issuerNode) {
    // Issuer certificate is required for SCT verification.
    return Success;
  }

  CERTCertificate* endEntity = endEntityNode->cert;
  CERTCertificate* issuer = issuerNode->cert;
  if (!endEntity || !issuer) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }

  Input endEntityDER;
  Result rv = endEntityDER.Init(endEntity->derCert.data,
                                endEntity->derCert.len);
  if (rv != Success) {
    return rv;
  }

  Input issuerPublicKeyDER;
  rv = issuerPublicKeyDER.Init(issuer->derPublicKey.data,
                               issuer->derPublicKey.len);
  if (rv != Success) {
    return rv;
  }

  CTVerifyResult result;
  rv = mCTVerifier->Verify(endEntityDER, issuerPublicKeyDER,
                           embeddedSCTs, sctsFromOCSP, sctsFromTLS, time,
                           result);
  if (rv != Success) {
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
            ("SCT verification failed with fatal error %i\n", rv));
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
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
            ("SCT verification result: "
             "valid=%zu unknownLog=%zu disqualifiedLog=%zu "
             "invalidSignature=%zu invalidTimestamp=%zu "
             "decodingErrors=%zu\n",
             validCount, unknownLogCount, disqualifiedLogCount,
             invalidSignatureCount, invalidTimestampCount,
             result.decodingErrors));
  }

  if (ctInfo) {
    ctInfo->processedSCTs = true;
    ctInfo->verifyResult = Move(result);
  }
  return Success;
}

bool
CertVerifier::SHA1ModeMoreRestrictiveThanGivenMode(SHA1Mode mode)
{
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

static const unsigned int MIN_RSA_BITS = 2048;
static const unsigned int MIN_RSA_BITS_WEAK = 1024;

Result
CertVerifier::VerifyCert(CERTCertificate* cert, SECCertificateUsage usage,
                         Time time, void* pinArg, const char* hostname,
                 /*out*/ UniqueCERTCertList& builtChain,
            /*optional*/ const Flags flags,
            /*optional*/ const SECItem* stapledOCSPResponseSECItem,
            /*optional*/ const SECItem* sctsFromTLSSECItem,
            /*optional*/ const NeckoOriginAttributes& originAttributes,
        /*optional out*/ SECOidTag* evOidPolicy,
        /*optional out*/ OCSPStaplingStatus* ocspStaplingStatus,
        /*optional out*/ KeySizeStatus* keySizeStatus,
        /*optional out*/ SHA1ModeResult* sha1ModeResult,
        /*optional out*/ PinningTelemetryInfo* pinningTelemetryInfo,
        /*optional out*/ CertificateTransparencyInfo* ctInfo)
{
  MOZ_LOG(gCertVerifierLog, LogLevel::Debug, ("Top of VerifyCert\n"));

  PR_ASSERT(cert);
  PR_ASSERT(usage == certificateUsageSSLServer || !(flags & FLAG_MUST_BE_EV));
  PR_ASSERT(usage == certificateUsageSSLServer || !keySizeStatus);
  PR_ASSERT(usage == certificateUsageSSLServer || !sha1ModeResult);

  if (evOidPolicy) {
    *evOidPolicy = SEC_OID_UNKNOWN;
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

  if (!cert ||
      (usage != certificateUsageSSLServer && (flags & FLAG_MUST_BE_EV))) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }

  Input certDER;
  Result rv = certDER.Init(cert->derCert.data, cert->derCert.len);
  if (rv != Success) {
    return rv;
  }

  // We configure the OCSP fetching modes separately for EV and non-EV
  // verifications.
  NSSCertDBTrustDomain::OCSPFetching defaultOCSPFetching
    = (mOCSPDownloadConfig == ocspOff) ||
      (mOCSPDownloadConfig == ocspEVOnly) ||
      (flags & FLAG_LOCAL_ONLY) ? NSSCertDBTrustDomain::NeverFetchOCSP
    : !mOCSPStrict              ? NSSCertDBTrustDomain::FetchOCSPForDVSoftFail
                                : NSSCertDBTrustDomain::FetchOCSPForDVHardFail;

  OcspGetConfig ocspGETConfig = mOCSPGETEnabled ? ocspGetEnabled
                                                : ocspGetDisabled;

  Input stapledOCSPResponseInput;
  const Input* stapledOCSPResponse = nullptr;
  if (stapledOCSPResponseSECItem) {
    rv = stapledOCSPResponseInput.Init(stapledOCSPResponseSECItem->data,
                                       stapledOCSPResponseSECItem->len);
    if (rv != Success) {
      // The stapled OCSP response was too big.
      return Result::ERROR_OCSP_MALFORMED_RESPONSE;
    }
    stapledOCSPResponse = &stapledOCSPResponseInput;
  }

  Input sctsFromTLSInput;
  if (sctsFromTLSSECItem) {
    rv = sctsFromTLSInput.Init(sctsFromTLSSECItem->data,
                               sctsFromTLSSECItem->len);
    // Silently discard the error of the extension being too big,
    // do not fail the verification.
    MOZ_ASSERT(rv == Success);
  }

  switch (usage) {
    case certificateUsageSSLClient: {
      // XXX: We don't really have a trust bit for SSL client authentication so
      // just use trustEmail as it is the closest alternative.
      NSSCertDBTrustDomain trustDomain(trustEmail, defaultOCSPFetching,
                                       mOCSPCache, pinArg, ocspGETConfig,
                                       mCertShortLifetimeInDays,
                                       pinningDisabled, MIN_RSA_BITS_WEAK,
                                       ValidityCheckingMode::CheckingOff,
                                       SHA1Mode::Allowed,
                                       NetscapeStepUpPolicy::NeverMatch,
                                       originAttributes,
                                       builtChain, nullptr, nullptr);
      rv = BuildCertChain(trustDomain, certDER, time,
                          EndEntityOrCA::MustBeEndEntity,
                          KeyUsage::digitalSignature,
                          KeyPurposeId::id_kp_clientAuth,
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

      size_t sha1ModeConfigurationsCount = MOZ_ARRAY_LENGTH(sha1ModeConfigurations);

      static_assert(MOZ_ARRAY_LENGTH(sha1ModeConfigurations) ==
                    MOZ_ARRAY_LENGTH(sha1ModeResults),
                    "digestAlgorithm array lengths differ");

      rv = Result::ERROR_UNKNOWN_ERROR;

      // Try to validate for EV first.
      NSSCertDBTrustDomain::OCSPFetching evOCSPFetching
        = (mOCSPDownloadConfig == ocspOff) ||
          (flags & FLAG_LOCAL_ONLY) ? NSSCertDBTrustDomain::LocalOnlyOCSPForEV
                                    : NSSCertDBTrustDomain::FetchOCSPForEV;

      CertPolicyId evPolicy;
      SECOidTag evPolicyOidTag;
      SECStatus srv = GetFirstEVPolicy(cert, evPolicy, evPolicyOidTag);
      for (size_t i = 0;
           i < sha1ModeConfigurationsCount && rv != Success && srv == SECSuccess;
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
        // previously noted telemetry information
        if (pinningTelemetryInfo) {
          pinningTelemetryInfo->Reset();
        }

        NSSCertDBTrustDomain
          trustDomain(trustSSL, evOCSPFetching,
                      mOCSPCache, pinArg, ocspGETConfig,
                      mCertShortLifetimeInDays, mPinningMode, MIN_RSA_BITS,
                      ValidityCheckingMode::CheckForEV,
                      sha1ModeConfigurations[i], mNetscapeStepUpPolicy,
                      originAttributes, builtChain, pinningTelemetryInfo,
                      hostname);
        rv = BuildCertChainForOneKeyUsage(trustDomain, certDER, time,
                                          KeyUsage::digitalSignature,// (EC)DHE
                                          KeyUsage::keyEncipherment, // RSA
                                          KeyUsage::keyAgreement,    // (EC)DH
                                          KeyPurposeId::id_kp_serverAuth,
                                          evPolicy, stapledOCSPResponse,
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
                  ("cert is EV with status %i\n", sha1ModeResults[i]));
          if (evOidPolicy) {
            *evOidPolicy = evPolicyOidTag;
          }
          if (sha1ModeResult) {
            *sha1ModeResult = sha1ModeResults[i];
          }
          rv = VerifySignedCertificateTimestamps(trustDomain, builtChain,
                                                 sctsFromTLSInput, time,
                                                 ctInfo);
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
      unsigned int keySizeOptions[] = {
        MIN_RSA_BITS,
        MIN_RSA_BITS_WEAK
      };

      KeySizeStatus keySizeStatuses[] = {
        KeySizeStatus::LargeMinimumSucceeded,
        KeySizeStatus::CompatibilityRisk
      };

      static_assert(MOZ_ARRAY_LENGTH(keySizeOptions) ==
                    MOZ_ARRAY_LENGTH(keySizeStatuses),
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

          NSSCertDBTrustDomain trustDomain(trustSSL, defaultOCSPFetching,
                                           mOCSPCache, pinArg, ocspGETConfig,
                                           mCertShortLifetimeInDays,
                                           mPinningMode, keySizeOptions[i],
                                           ValidityCheckingMode::CheckingOff,
                                           sha1ModeConfigurations[j],
                                           mNetscapeStepUpPolicy,
                                           originAttributes, builtChain,
                                           pinningTelemetryInfo, hostname);
          rv = BuildCertChainForOneKeyUsage(trustDomain, certDER, time,
                                            KeyUsage::digitalSignature,//(EC)DHE
                                            KeyUsage::keyEncipherment,//RSA
                                            KeyUsage::keyAgreement,//(EC)DH
                                            KeyPurposeId::id_kp_serverAuth,
                                            CertPolicyId::anyPolicy,
                                            stapledOCSPResponse,
                                            ocspStaplingStatus);
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
            rv = VerifySignedCertificateTimestamps(trustDomain, builtChain,
                                                   sctsFromTLSInput, time,
                                                   ctInfo);
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
      // from users who have manually set the policy to Allowed or Forbidden, so
      // we only collect for ImportedRoot or ImportedRootOrBefore2016.
      if (sha1ModeResult &&
          (mSHA1Mode == SHA1Mode::ImportedRoot ||
           mSHA1Mode == SHA1Mode::ImportedRootOrBefore2016)) {
        *sha1ModeResult = SHA1ModeResult::Failed;
      }

      break;
    }

    case certificateUsageSSLCA: {
      NSSCertDBTrustDomain trustDomain(trustSSL, defaultOCSPFetching,
                                       mOCSPCache, pinArg, ocspGETConfig,
                                       mCertShortLifetimeInDays,
                                       pinningDisabled, MIN_RSA_BITS_WEAK,
                                       ValidityCheckingMode::CheckingOff,
                                       SHA1Mode::Allowed, mNetscapeStepUpPolicy,
                                       originAttributes, builtChain, nullptr,
                                       nullptr);
      rv = BuildCertChain(trustDomain, certDER, time,
                          EndEntityOrCA::MustBeCA, KeyUsage::keyCertSign,
                          KeyPurposeId::id_kp_serverAuth,
                          CertPolicyId::anyPolicy, stapledOCSPResponse);
      break;
    }

    case certificateUsageEmailSigner: {
      NSSCertDBTrustDomain trustDomain(trustEmail, defaultOCSPFetching,
                                       mOCSPCache, pinArg, ocspGETConfig,
                                       mCertShortLifetimeInDays,
                                       pinningDisabled, MIN_RSA_BITS_WEAK,
                                       ValidityCheckingMode::CheckingOff,
                                       SHA1Mode::Allowed,
                                       NetscapeStepUpPolicy::NeverMatch,
                                       originAttributes, builtChain, nullptr,
                                       nullptr);
      rv = BuildCertChain(trustDomain, certDER, time,
                          EndEntityOrCA::MustBeEndEntity,
                          KeyUsage::digitalSignature,
                          KeyPurposeId::id_kp_emailProtection,
                          CertPolicyId::anyPolicy, stapledOCSPResponse);
      if (rv == Result::ERROR_INADEQUATE_KEY_USAGE) {
        rv = BuildCertChain(trustDomain, certDER, time,
                            EndEntityOrCA::MustBeEndEntity,
                            KeyUsage::nonRepudiation,
                            KeyPurposeId::id_kp_emailProtection,
                            CertPolicyId::anyPolicy, stapledOCSPResponse);
      }
      break;
    }

    case certificateUsageEmailRecipient: {
      // TODO: The higher level S/MIME processing should pass in which key
      // usage it is trying to verify for, and base its algorithm choices
      // based on the result of the verification(s).
      NSSCertDBTrustDomain trustDomain(trustEmail, defaultOCSPFetching,
                                       mOCSPCache, pinArg, ocspGETConfig,
                                       mCertShortLifetimeInDays,
                                       pinningDisabled, MIN_RSA_BITS_WEAK,
                                       ValidityCheckingMode::CheckingOff,
                                       SHA1Mode::Allowed,
                                       NetscapeStepUpPolicy::NeverMatch,
                                       originAttributes, builtChain, nullptr,
                                       nullptr);
      rv = BuildCertChain(trustDomain, certDER, time,
                          EndEntityOrCA::MustBeEndEntity,
                          KeyUsage::keyEncipherment, // RSA
                          KeyPurposeId::id_kp_emailProtection,
                          CertPolicyId::anyPolicy, stapledOCSPResponse);
      if (rv == Result::ERROR_INADEQUATE_KEY_USAGE) {
        rv = BuildCertChain(trustDomain, certDER, time,
                            EndEntityOrCA::MustBeEndEntity,
                            KeyUsage::keyAgreement, // ECDH/DH
                            KeyPurposeId::id_kp_emailProtection,
                            CertPolicyId::anyPolicy, stapledOCSPResponse);
      }
      break;
    }

    case certificateUsageObjectSigner: {
      NSSCertDBTrustDomain trustDomain(trustObjectSigning, defaultOCSPFetching,
                                       mOCSPCache, pinArg, ocspGETConfig,
                                       mCertShortLifetimeInDays,
                                       pinningDisabled, MIN_RSA_BITS_WEAK,
                                       ValidityCheckingMode::CheckingOff,
                                       SHA1Mode::Allowed,
                                       NetscapeStepUpPolicy::NeverMatch,
                                       originAttributes, builtChain, nullptr,
                                       nullptr);
      rv = BuildCertChain(trustDomain, certDER, time,
                          EndEntityOrCA::MustBeEndEntity,
                          KeyUsage::digitalSignature,
                          KeyPurposeId::id_kp_codeSigning,
                          CertPolicyId::anyPolicy, stapledOCSPResponse);
      break;
    }

    case certificateUsageVerifyCA:
    case certificateUsageStatusResponder: {
      // XXX This is a pretty useless way to verify a certificate. It is used
      // by the certificate viewer UI. Because we don't know what trust bit is
      // interesting, we just try them all.
      mozilla::pkix::EndEntityOrCA endEntityOrCA;
      mozilla::pkix::KeyUsage keyUsage;
      KeyPurposeId eku;
      if (usage == certificateUsageVerifyCA) {
        endEntityOrCA = EndEntityOrCA::MustBeCA;
        keyUsage = KeyUsage::keyCertSign;
        eku = KeyPurposeId::anyExtendedKeyUsage;
      } else {
        endEntityOrCA = EndEntityOrCA::MustBeEndEntity;
        keyUsage = KeyUsage::digitalSignature;
        eku = KeyPurposeId::id_kp_OCSPSigning;
      }

      NSSCertDBTrustDomain sslTrust(trustSSL, defaultOCSPFetching, mOCSPCache,
                                    pinArg, ocspGETConfig, mCertShortLifetimeInDays,
                                    pinningDisabled, MIN_RSA_BITS_WEAK,
                                    ValidityCheckingMode::CheckingOff,
                                    SHA1Mode::Allowed,
                                    NetscapeStepUpPolicy::NeverMatch,
                                    originAttributes, builtChain, nullptr,
                                    nullptr);
      rv = BuildCertChain(sslTrust, certDER, time, endEntityOrCA,
                          keyUsage, eku, CertPolicyId::anyPolicy,
                          stapledOCSPResponse);
      if (rv == Result::ERROR_UNKNOWN_ISSUER) {
        NSSCertDBTrustDomain emailTrust(trustEmail, defaultOCSPFetching,
                                        mOCSPCache, pinArg, ocspGETConfig,
                                        mCertShortLifetimeInDays,
                                        pinningDisabled, MIN_RSA_BITS_WEAK,
                                        ValidityCheckingMode::CheckingOff,
                                        SHA1Mode::Allowed,
                                        NetscapeStepUpPolicy::NeverMatch,
                                        originAttributes, builtChain, nullptr,
                                        nullptr);
        rv = BuildCertChain(emailTrust, certDER, time, endEntityOrCA,
                            keyUsage, eku, CertPolicyId::anyPolicy,
                            stapledOCSPResponse);
        if (rv == Result::ERROR_UNKNOWN_ISSUER) {
          NSSCertDBTrustDomain objectSigningTrust(trustObjectSigning,
                                                  defaultOCSPFetching, mOCSPCache,
                                                  pinArg, ocspGETConfig,
                                                  mCertShortLifetimeInDays,
                                                  pinningDisabled,
                                                  MIN_RSA_BITS_WEAK,
                                                  ValidityCheckingMode::CheckingOff,
                                                  SHA1Mode::Allowed,
                                                  NetscapeStepUpPolicy::NeverMatch,
                                                  originAttributes, builtChain,
                                                  nullptr, nullptr);
          rv = BuildCertChain(objectSigningTrust, certDER, time,
                              endEntityOrCA, keyUsage, eku,
                              CertPolicyId::anyPolicy, stapledOCSPResponse);
        }
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

Result
CertVerifier::VerifySSLServerCert(const UniqueCERTCertificate& peerCert,
                     /*optional*/ const SECItem* stapledOCSPResponse,
                     /*optional*/ const SECItem* sctsFromTLS,
                                  Time time,
                     /*optional*/ void* pinarg,
                                  const char* hostname,
                          /*out*/ UniqueCERTCertList& builtChain,
                     /*optional*/ bool saveIntermediatesInPermanentDatabase,
                     /*optional*/ Flags flags,
                     /*optional*/ const NeckoOriginAttributes& originAttributes,
                 /*optional out*/ SECOidTag* evOidPolicy,
                 /*optional out*/ OCSPStaplingStatus* ocspStaplingStatus,
                 /*optional out*/ KeySizeStatus* keySizeStatus,
                 /*optional out*/ SHA1ModeResult* sha1ModeResult,
                 /*optional out*/ PinningTelemetryInfo* pinningTelemetryInfo,
                 /*optional out*/ CertificateTransparencyInfo* ctInfo)
{
  PR_ASSERT(peerCert);
  // XXX: PR_ASSERT(pinarg)
  PR_ASSERT(hostname);
  PR_ASSERT(hostname[0]);

  if (evOidPolicy) {
    *evOidPolicy = SEC_OID_UNKNOWN;
  }

  if (!hostname || !hostname[0]) {
    return Result::ERROR_BAD_CERT_DOMAIN;
  }

  // CreateCertErrorRunnable assumes that CheckCertHostname is only called
  // if VerifyCert succeeded.
  Result rv = VerifyCert(peerCert.get(), certificateUsageSSLServer, time,
                         pinarg, hostname, builtChain, flags,
                         stapledOCSPResponse, sctsFromTLS, originAttributes,
                         evOidPolicy, ocspStaplingStatus, keySizeStatus,
                         sha1ModeResult, pinningTelemetryInfo, ctInfo);
  if (rv != Success) {
    return rv;
  }

  Input peerCertInput;
  rv = peerCertInput.Init(peerCert->derCert.data, peerCert->derCert.len);
  if (rv != Success) {
    return rv;
  }

  Input stapledOCSPResponseInput;
  Input* responseInputPtr = nullptr;
  if (stapledOCSPResponse) {
    rv = stapledOCSPResponseInput.Init(stapledOCSPResponse->data,
                                       stapledOCSPResponse->len);
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
  rv = hostnameInput.Init(BitwiseCast<const uint8_t*, const char*>(hostname),
                          strlen(hostname));
  if (rv != Success) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }
  bool isBuiltInRoot;
  rv = IsCertChainRootBuiltInRoot(builtChain, isBuiltInRoot);
  if (rv != Success) {
    return rv;
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

  if (saveIntermediatesInPermanentDatabase) {
    SaveIntermediateCerts(builtChain);
  }

  return Success;
}

} } // namespace mozilla::psm
