/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CertVerifier.h"

#include <stdint.h>

#include "ExtendedValidation.h"
#include "NSSCertDBTrustDomain.h"
#include "NSSErrorsService.h"
#include "cert.h"
#include "nsNSSComponent.h"
#include "nsServiceManagerUtils.h"
#include "pk11pub.h"
#include "pkix/pkix.h"
#include "pkix/pkixnss.h"
#include "prerror.h"
#include "secerr.h"
#include "secmod.h"
#include "sslerr.h"

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
                           SHA1Mode sha1Mode)
  : mOCSPDownloadConfig(odc)
  , mOCSPStrict(osc == ocspStrict)
  , mOCSPGETEnabled(ogc == ocspGetEnabled)
  , mCertShortLifetimeInDays(certShortLifetimeInDays)
  , mPinningMode(pinningMode)
  , mSHA1Mode(sha1Mode)
{
}

CertVerifier::~CertVerifier()
{
}

void
InitCertVerifierLog()
{
}

Result
IsCertChainRootBuiltInRoot(CERTCertList* chain, bool& result)
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
  nsCOMPtr<nsINSSComponent> component(do_GetService(PSM_COMPONENT_CONTRACTID));
  if (!component) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  nsresult rv;
#ifdef DEBUG
  rv = component->IsCertTestBuiltInRoot(cert, result);
  if (NS_FAILED(rv)) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  if (result) {
    return Success;
  }
#endif // DEBUG
  nsAutoString modName;
  rv = component->GetPIPNSSBundleString("RootCertModuleName", modName);
  if (NS_FAILED(rv)) {
    return Result::FATAL_ERROR_LIBRARY_FAILURE;
  }
  NS_ConvertUTF16toUTF8 modNameUTF8(modName);
  UniqueSECMODModule builtinRootsModule(SECMOD_FindModule(modNameUTF8.get()));
  // If the built-in roots module isn't loaded, nothing is a built-in root.
  if (!builtinRootsModule) {
    return Success;
  }
  UniquePK11SlotInfo builtinSlot(SECMOD_FindSlot(builtinRootsModule.get(),
                                                 "Builtin Object Token"));
  // This could happen if the user loaded a module that is acting like the
  // built-in roots module but doesn't actually have a slot called "Builtin
  // Object Token". In that case, again nothing is a built-in root.
  if (!builtinSlot) {
    return Success;
  }
  // Attempt to find a copy of the given certificate in the "Builtin Object
  // Token" slot of the built-in root module. If we get a valid handle, this
  // certificate exists in the root module, so we consider it a built-in root.
  CK_OBJECT_HANDLE handle = PK11_FindCertInSlot(builtinSlot.get(), cert,
                                                nullptr);
  result = (handle != CK_INVALID_HANDLE);
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
  trustDomain.ResetOCSPStaplingStatus();
  Result rv = BuildCertChain(trustDomain, certDER, time,
                             EndEntityOrCA::MustBeEndEntity, ku1,
                             eku, requiredPolicy, stapledOCSPResponse);
  if (rv == Result::ERROR_INADEQUATE_KEY_USAGE) {
    trustDomain.ResetOCSPStaplingStatus();
    rv = BuildCertChain(trustDomain, certDER, time,
                        EndEntityOrCA::MustBeEndEntity, ku2,
                        eku, requiredPolicy, stapledOCSPResponse);
    if (rv == Result::ERROR_INADEQUATE_KEY_USAGE) {
      trustDomain.ResetOCSPStaplingStatus();
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

bool
CertVerifier::SHA1ModeMoreRestrictiveThanGivenMode(SHA1Mode mode)
{
  switch (mSHA1Mode) {
    case SHA1Mode::Forbidden:
      return mode != SHA1Mode::Forbidden;
    case SHA1Mode::Before2016:
      return mode != SHA1Mode::Forbidden && mode != SHA1Mode::Before2016;
    case SHA1Mode::ImportedRoot:
      return mode == SHA1Mode::Allowed;
    case SHA1Mode::Allowed:
      return false;
    default:
      MOZ_ASSERT(false, "unexpected SHA1Mode type");
      return true;
  }
}

static const unsigned int MIN_RSA_BITS = 2048;
static const unsigned int MIN_RSA_BITS_WEAK = 1024;

SECStatus
CertVerifier::VerifyCert(CERTCertificate* cert, SECCertificateUsage usage,
                         Time time, void* pinArg, const char* hostname,
                 /*out*/ ScopedCERTCertList& builtChain,
            /*optional*/ const Flags flags,
            /*optional*/ const SECItem* stapledOCSPResponseSECItem,
        /*optional out*/ SECOidTag* evOidPolicy,
        /*optional out*/ OCSPStaplingStatus* ocspStaplingStatus,
        /*optional out*/ KeySizeStatus* keySizeStatus,
        /*optional out*/ SHA1ModeResult* sha1ModeResult,
        /*optional out*/ PinningTelemetryInfo* pinningTelemetryInfo)
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
      PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
      return SECFailure;
    }
    *ocspStaplingStatus = OCSP_STAPLING_NEVER_CHECKED;
  }

  if (keySizeStatus) {
    if (usage != certificateUsageSSLServer) {
      PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
      return SECFailure;
    }
    *keySizeStatus = KeySizeStatus::NeverChecked;
  }

  if (sha1ModeResult) {
    if (usage != certificateUsageSSLServer) {
      PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
      return SECFailure;
    }
    *sha1ModeResult = SHA1ModeResult::NeverChecked;
  }

  if (!cert ||
      (usage != certificateUsageSSLServer && (flags & FLAG_MUST_BE_EV))) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }

  Result rv;

  Input certDER;
  rv = certDER.Init(cert->derCert.data, cert->derCert.len);
  if (rv != Success) {
    PR_SetError(MapResultToPRErrorCode(rv), 0);
    return SECFailure;
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
      PR_SetError(SEC_ERROR_OCSP_MALFORMED_RESPONSE, 0);
      return SECFailure;
    }
    stapledOCSPResponse = &stapledOCSPResponseInput;
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
                                       SHA1Mode::Allowed, builtChain, nullptr,
                                       nullptr);
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
        SHA1Mode::Before2016,
        SHA1Mode::ImportedRoot,
        SHA1Mode::Allowed,
      };

      SHA1ModeResult sha1ModeResults[] = {
        SHA1ModeResult::SucceededWithoutSHA1,
        SHA1ModeResult::SucceededWithSHA1Before2016,
        SHA1ModeResult::SucceededWithImportedRoot,
        SHA1ModeResult::SucceededWithSHA1,
      };

      size_t sha1ModeConfigurationsCount = MOZ_ARRAY_LENGTH(sha1ModeConfigurations);

      static_assert(MOZ_ARRAY_LENGTH(sha1ModeConfigurations) ==
                    MOZ_ARRAY_LENGTH(sha1ModeResults),
                    "digestAlgorithm array lengths differ");

      rv = Result::ERROR_UNKNOWN_ERROR;

#ifndef MOZ_NO_EV_CERTS
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
        // Because of the try-strict and fallback approach, we have to clear any
        // previously noted telemetry information
        if (pinningTelemetryInfo) {
          pinningTelemetryInfo->Reset();
        }
        // Don't attempt verification if the SHA1 mode set by preferences
        // (mSHA1Mode) is more restrictive than the SHA1 mode option we're on.
        // (To put it another way, only attempt verification if the SHA1 mode
        // option we're on is as restrictive or more restrictive than
        // mSHA1Mode.) This allows us to gather telemetry information while
        // still enforcing the mode set by preferences.
        if (SHA1ModeMoreRestrictiveThanGivenMode(sha1ModeConfigurations[i])) {
          continue;
        }
        NSSCertDBTrustDomain
          trustDomain(trustSSL, evOCSPFetching,
                      mOCSPCache, pinArg, ocspGETConfig,
                      mCertShortLifetimeInDays, mPinningMode, MIN_RSA_BITS,
                      ValidityCheckingMode::CheckForEV,
                      sha1ModeConfigurations[i], builtChain,
                      pinningTelemetryInfo, hostname);
        rv = BuildCertChainForOneKeyUsage(trustDomain, certDER, time,
                                          KeyUsage::digitalSignature,// (EC)DHE
                                          KeyUsage::keyEncipherment, // RSA
                                          KeyUsage::keyAgreement,    // (EC)DH
                                          KeyPurposeId::id_kp_serverAuth,
                                          evPolicy, stapledOCSPResponse,
                                          ocspStaplingStatus);
        // If we succeeded with the SHA1Mode of only allowing imported roots to
        // issue SHA1 certificates after 2015, if the chain we built doesn't
        // terminate with an imported root, we must reject it. (This only works
        // because we try SHA1 configurations in order of decreasing
        // strictness.)
        // Note that if there existed a certificate chain with a built-in root
        // that had SHA1 certificates issued before 2016, it would have already
        // been accepted. If such a chain had SHA1 certificates issued after
        // 2015, it will only be accepted in the SHA1Mode::Allowed case.
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
        }
      }
      if (rv == Success) {
        break;
      }
#endif

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
          // invalidate any telemetry info relating to failed chains
          if (pinningTelemetryInfo) {
            pinningTelemetryInfo->Reset();
          }

          // Don't attempt verification if the SHA1 mode set by preferences
          // (mSHA1Mode) is more restrictive than the SHA1 mode option we're on.
          // (To put it another way, only attempt verification if the SHA1 mode
          // option we're on is as restrictive or more restrictive than
          // mSHA1Mode.) This allows us to gather telemetry information while
          // still enforcing the mode set by preferences.
          if (SHA1ModeMoreRestrictiveThanGivenMode(sha1ModeConfigurations[j])) {
            continue;
          }

          NSSCertDBTrustDomain trustDomain(trustSSL, defaultOCSPFetching,
                                           mOCSPCache, pinArg, ocspGETConfig,
                                           mCertShortLifetimeInDays,
                                           mPinningMode, keySizeOptions[i],
                                           ValidityCheckingMode::CheckingOff,
                                           sha1ModeConfigurations[j],
                                           builtChain, pinningTelemetryInfo,
                                           hostname);
          rv = BuildCertChainForOneKeyUsage(trustDomain, certDER, time,
                                            KeyUsage::digitalSignature,//(EC)DHE
                                            KeyUsage::keyEncipherment,//RSA
                                            KeyUsage::keyAgreement,//(EC)DH
                                            KeyPurposeId::id_kp_serverAuth,
                                            CertPolicyId::anyPolicy,
                                            stapledOCSPResponse,
                                            ocspStaplingStatus);
          // If we succeeded with the SHA1Mode of only allowing imported roots
          // to issue SHA1 certificates after 2015, if the chain we built
          // doesn't terminate with an imported root, we must reject it. (This
          // only works because we try SHA1 configurations in order of
          // decreasing strictness.)
          // Note that if there existed a certificate chain with a built-in root
          // that had SHA1 certificates issued before 2016, it would have
          // already been accepted. If such a chain had SHA1 certificates issued
          // after 2015, it will only be accepted in the SHA1Mode::Allowed case.
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
          }
        }
      }

      if (rv == Success) {
        break;
      }

      if (keySizeStatus) {
        *keySizeStatus = KeySizeStatus::AlreadyBad;
      }
      // Only collect CERT_CHAIN_SHA1_POLICY_STATUS telemetry indicating a
      // failure when mSHA1Mode is the default.
      // NB: When we change the default, we have to change this.
      if (sha1ModeResult && mSHA1Mode == SHA1Mode::ImportedRoot) {
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
                                       mSHA1Mode, builtChain, nullptr, nullptr);
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
                                       SHA1Mode::Allowed, builtChain, nullptr,
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
                                       SHA1Mode::Allowed, builtChain, nullptr,
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
                                       SHA1Mode::Allowed, builtChain, nullptr,
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
                                    SHA1Mode::Allowed, builtChain, nullptr,
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
                                        SHA1Mode::Allowed, builtChain, nullptr,
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
                                                  SHA1Mode::Allowed, builtChain,
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
    PR_SetError(MapResultToPRErrorCode(rv), 0);
    return SECFailure;
  }

  return SECSuccess;
}

SECStatus
CertVerifier::VerifySSLServerCert(CERTCertificate* peerCert,
                     /*optional*/ const SECItem* stapledOCSPResponse,
                                  Time time,
                     /*optional*/ void* pinarg,
                                  const char* hostname,
                          /*out*/ ScopedCERTCertList& builtChain,
                     /*optional*/ bool saveIntermediatesInPermanentDatabase,
                     /*optional*/ Flags flags,
                 /*optional out*/ SECOidTag* evOidPolicy,
                 /*optional out*/ OCSPStaplingStatus* ocspStaplingStatus,
                 /*optional out*/ KeySizeStatus* keySizeStatus,
                 /*optional out*/ SHA1ModeResult* sha1ModeResult,
                 /*optional out*/ PinningTelemetryInfo* pinningTelemetryInfo)
{
  PR_ASSERT(peerCert);
  // XXX: PR_ASSERT(pinarg)
  PR_ASSERT(hostname);
  PR_ASSERT(hostname[0]);

  if (evOidPolicy) {
    *evOidPolicy = SEC_OID_UNKNOWN;
  }

  if (!hostname || !hostname[0]) {
    PR_SetError(SSL_ERROR_BAD_CERT_DOMAIN, 0);
    return SECFailure;
  }

  // CreateCertErrorRunnable assumes that CheckCertHostname is only called
  // if VerifyCert succeeded.
  SECStatus rv = VerifyCert(peerCert, certificateUsageSSLServer, time, pinarg,
                            hostname, builtChain, flags, stapledOCSPResponse,
                            evOidPolicy, ocspStaplingStatus, keySizeStatus,
                            sha1ModeResult, pinningTelemetryInfo);
  if (rv != SECSuccess) {
    return rv;
  }

  Input peerCertInput;
  Result result = peerCertInput.Init(peerCert->derCert.data,
                                     peerCert->derCert.len);
  if (result != Success) {
    PR_SetError(MapResultToPRErrorCode(result), 0);
    return SECFailure;
  }

  Input stapledOCSPResponseInput;
  Input* responseInputPtr = nullptr;
  if (stapledOCSPResponse) {
    result = stapledOCSPResponseInput.Init(stapledOCSPResponse->data,
                                           stapledOCSPResponse->len);
    if (result != Success) {
      // The stapled OCSP response was too big.
      PR_SetError(SEC_ERROR_OCSP_MALFORMED_RESPONSE, 0);
      return SECFailure;
    }
    responseInputPtr = &stapledOCSPResponseInput;
  }

  if (!(flags & FLAG_TLS_IGNORE_STATUS_REQUEST)) {
    result = CheckTLSFeaturesAreSatisfied(peerCertInput, responseInputPtr);

    if (result != Success) {
      PR_SetError(MapResultToPRErrorCode(result), 0);
      return SECFailure;
    }
  }

  Input hostnameInput;
  result = hostnameInput.Init(uint8_t_ptr_cast(hostname), strlen(hostname));
  if (result != Success) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }
  result = CheckCertHostname(peerCertInput, hostnameInput);
  if (result != Success) {
    // Treat malformed name information as a domain mismatch.
    if (result == Result::ERROR_BAD_DER) {
      PR_SetError(SSL_ERROR_BAD_CERT_DOMAIN, 0);
    } else {
      PR_SetError(MapResultToPRErrorCode(result), 0);
    }
    return SECFailure;
  }

  if (saveIntermediatesInPermanentDatabase) {
    SaveIntermediateCerts(builtChain);
  }

  return SECSuccess;
}

} } // namespace mozilla::psm
