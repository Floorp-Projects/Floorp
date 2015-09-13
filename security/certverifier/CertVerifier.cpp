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
#include "pk11pub.h"
#include "pkix/pkix.h"
#include "pkix/pkixnss.h"
#include "prerror.h"
#include "secerr.h"
#include "sslerr.h"

using namespace mozilla::pkix;
using namespace mozilla::psm;

PRLogModuleInfo* gCertVerifierLog = nullptr;

namespace mozilla { namespace psm {

const CertVerifier::Flags CertVerifier::FLAG_LOCAL_ONLY = 1;
const CertVerifier::Flags CertVerifier::FLAG_MUST_BE_EV = 2;

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
  if (!gCertVerifierLog) {
    gCertVerifierLog = PR_NewLogModule("certverifier");
  }
}

SECStatus
IsCertBuiltInRoot(CERTCertificate* cert, bool& result) {
  result = false;
  ScopedPK11SlotList slots;
  slots = PK11_GetAllSlotsForCert(cert, nullptr);
  if (!slots) {
    if (PORT_GetError() == SEC_ERROR_NO_TOKEN) {
      // no list
      return SECSuccess;
    }
    return SECFailure;
  }
  for (PK11SlotListElement* le = slots->head; le; le = le->next) {
    char* token = PK11_GetTokenName(le->slot);
    MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
           ("BuiltInRoot? subject=%s token=%s",cert->subjectName, token));
    if (strcmp("Builtin Object Token", token) == 0) {
      result = true;
      return SECSuccess;
    }
  }
  return SECSuccess;
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

static const unsigned int MIN_RSA_BITS = 2048;
static const unsigned int MIN_RSA_BITS_WEAK = 1024;

SECStatus
CertVerifier::VerifyCert(CERTCertificate* cert, SECCertificateUsage usage,
                         Time time, void* pinArg, const char* hostname,
                         const Flags flags,
            /*optional*/ const SECItem* stapledOCSPResponseSECItem,
        /*optional out*/ ScopedCERTCertList* builtChain,
        /*optional out*/ SECOidTag* evOidPolicy,
        /*optional out*/ OCSPStaplingStatus* ocspStaplingStatus,
        /*optional out*/ KeySizeStatus* keySizeStatus,
        /*optional out*/ SignatureDigestStatus* sigDigestStatus,
        /*optional out*/ PinningTelemetryInfo* pinningTelemetryInfo)
{
  MOZ_LOG(gCertVerifierLog, LogLevel::Debug, ("Top of VerifyCert\n"));

  PR_ASSERT(cert);
  PR_ASSERT(usage == certificateUsageSSLServer || !(flags & FLAG_MUST_BE_EV));
  PR_ASSERT(usage == certificateUsageSSLServer || !keySizeStatus);
  PR_ASSERT(usage == certificateUsageSSLServer || !sigDigestStatus);

  if (builtChain) {
    *builtChain = nullptr;
  }
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

  if (sigDigestStatus) {
    if (usage != certificateUsageSSLServer) {
      PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
      return SECFailure;
    }
    *sigDigestStatus = SignatureDigestStatus::NeverChecked;
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
                                       AcceptAllAlgorithms, SHA1Mode::Allowed,
                                       nullptr, nullptr, builtChain);
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

      SignatureDigestOption digestAlgorithmOptions[] = {
        DisableSHA1Everywhere,
        DisableSHA1ForCA,
        DisableSHA1ForEE,
        AcceptAllAlgorithms
      };

      SignatureDigestStatus digestAlgorithmStatuses[] = {
        SignatureDigestStatus::GoodAlgorithmsOnly,
        SignatureDigestStatus::WeakEECert,
        SignatureDigestStatus::WeakCACert,
        SignatureDigestStatus::WeakCAAndEE
      };

      size_t digestAlgorithmOptionsCount = MOZ_ARRAY_LENGTH(digestAlgorithmStatuses);

      static_assert(MOZ_ARRAY_LENGTH(digestAlgorithmOptions) ==
                    MOZ_ARRAY_LENGTH(digestAlgorithmStatuses),
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
      for (size_t i=0;
           i < digestAlgorithmOptionsCount && rv != Success && srv == SECSuccess;
           i++) {
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
                      digestAlgorithmOptions[i], mSHA1Mode,
                      pinningTelemetryInfo, hostname, builtChain);
        rv = BuildCertChainForOneKeyUsage(trustDomain, certDER, time,
                                          KeyUsage::digitalSignature,// (EC)DHE
                                          KeyUsage::keyEncipherment, // RSA
                                          KeyUsage::keyAgreement,    // (EC)DH
                                          KeyPurposeId::id_kp_serverAuth,
                                          evPolicy, stapledOCSPResponse,
                                          ocspStaplingStatus);
        if (rv == Success) {
          MOZ_LOG(gCertVerifierLog, LogLevel::Debug,
                  ("cert is EV with status %i\n", digestAlgorithmStatuses[i]));
          if (evOidPolicy) {
            *evOidPolicy = evPolicyOidTag;
          }
          if (sigDigestStatus) {
            *sigDigestStatus = digestAlgorithmStatuses[i];
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

      for (size_t i=0; i<keySizeOptionsCount && rv != Success; i++) {
        for (size_t j=0; j<digestAlgorithmOptionsCount && rv != Success; j++) {

          // invalidate any telemetry info relating to failed chains
          if (pinningTelemetryInfo) {
            pinningTelemetryInfo->Reset();
          }

          // If we're not going to do SHA-1 in any case, don't try
          if (mSHA1Mode == SHA1Mode::Forbidden &&
              digestAlgorithmOptions[i] != DisableSHA1Everywhere) {
            continue;
          }

          NSSCertDBTrustDomain trustDomain(trustSSL, defaultOCSPFetching,
                                           mOCSPCache, pinArg, ocspGETConfig,
                                           mCertShortLifetimeInDays,
                                           mPinningMode, keySizeOptions[i],
                                           ValidityCheckingMode::CheckingOff,
                                           digestAlgorithmOptions[j],
                                           mSHA1Mode, pinningTelemetryInfo,
                                           hostname, builtChain);
          rv = BuildCertChainForOneKeyUsage(trustDomain, certDER, time,
                                            KeyUsage::digitalSignature,//(EC)DHE
                                            KeyUsage::keyEncipherment,//RSA
                                            KeyUsage::keyAgreement,//(EC)DH
                                            KeyPurposeId::id_kp_serverAuth,
                                            CertPolicyId::anyPolicy,
                                            stapledOCSPResponse,
                                            ocspStaplingStatus);
          if (rv == Success) {
            if (keySizeStatus) {
              *keySizeStatus = keySizeStatuses[i];
            }
            if (sigDigestStatus) {
              *sigDigestStatus = digestAlgorithmStatuses[j];
            }
          }
        }
      }

      if (rv == Success) {
        // If SHA-1 is forbidden by preference, don't accumulate SHA-1
        // telemetry, to avoid skewing the results.
        if (sigDigestStatus && mSHA1Mode == SHA1Mode::Forbidden) {
          *sigDigestStatus = SignatureDigestStatus::NeverChecked;
        }

        break;
      }

      if (keySizeStatus) {
        *keySizeStatus = KeySizeStatus::AlreadyBad;
      }
      if (sigDigestStatus && mSHA1Mode != SHA1Mode::Forbidden) {
        *sigDigestStatus = SignatureDigestStatus::AlreadyBad;
      }

      break;
    }

    case certificateUsageSSLCA: {
      NSSCertDBTrustDomain trustDomain(trustSSL, defaultOCSPFetching,
                                       mOCSPCache, pinArg, ocspGETConfig,
                                       mCertShortLifetimeInDays,
                                       pinningDisabled, MIN_RSA_BITS_WEAK,
                                       ValidityCheckingMode::CheckingOff,
                                       AcceptAllAlgorithms, mSHA1Mode,
                                       nullptr, nullptr, builtChain);
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
                                       AcceptAllAlgorithms, SHA1Mode::Allowed,
                                       nullptr, nullptr, builtChain);
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
                                       AcceptAllAlgorithms, SHA1Mode::Allowed,
                                       nullptr, nullptr, builtChain);
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
                                       AcceptAllAlgorithms, SHA1Mode::Allowed,
                                       nullptr, nullptr, builtChain);
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
                                    AcceptAllAlgorithms, SHA1Mode::Allowed,
                                    nullptr, nullptr, builtChain);
      rv = BuildCertChain(sslTrust, certDER, time, endEntityOrCA,
                          keyUsage, eku, CertPolicyId::anyPolicy,
                          stapledOCSPResponse);
      if (rv == Result::ERROR_UNKNOWN_ISSUER) {
        NSSCertDBTrustDomain emailTrust(trustEmail, defaultOCSPFetching,
                                        mOCSPCache, pinArg, ocspGETConfig,
                                        mCertShortLifetimeInDays,
                                        pinningDisabled, MIN_RSA_BITS_WEAK,
                                        ValidityCheckingMode::CheckingOff,
                                        AcceptAllAlgorithms, SHA1Mode::Allowed,
                                        nullptr, nullptr, builtChain);
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
                                                  AcceptAllAlgorithms, SHA1Mode::Allowed,
                                                  nullptr, nullptr, builtChain);
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
                                  bool saveIntermediatesInPermanentDatabase,
                                  Flags flags,
                 /*optional out*/ ScopedCERTCertList* builtChain,
                 /*optional out*/ SECOidTag* evOidPolicy,
                 /*optional out*/ OCSPStaplingStatus* ocspStaplingStatus,
                 /*optional out*/ KeySizeStatus* keySizeStatus,
                 /*optional out*/ SignatureDigestStatus* sigDigestStatus,
                 /*optional out*/ PinningTelemetryInfo* pinningTelemetryInfo)
{
  PR_ASSERT(peerCert);
  // XXX: PR_ASSERT(pinarg)
  PR_ASSERT(hostname);
  PR_ASSERT(hostname[0]);

  if (builtChain) {
    *builtChain = nullptr;
  }
  if (evOidPolicy) {
    *evOidPolicy = SEC_OID_UNKNOWN;
  }

  if (!hostname || !hostname[0]) {
    PR_SetError(SSL_ERROR_BAD_CERT_DOMAIN, 0);
    return SECFailure;
  }

  ScopedCERTCertList builtChainTemp;
  // CreateCertErrorRunnable assumes that CheckCertHostname is only called
  // if VerifyCert succeeded.
  SECStatus rv = VerifyCert(peerCert, certificateUsageSSLServer, time, pinarg,
                            hostname, flags, stapledOCSPResponse,
                            &builtChainTemp, evOidPolicy, ocspStaplingStatus,
                            keySizeStatus, sigDigestStatus,
                            pinningTelemetryInfo);
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
    SaveIntermediateCerts(builtChainTemp);
  }

  if (builtChain) {
    *builtChain = builtChainTemp.forget();
  }

  return SECSuccess;
}

} } // namespace mozilla::psm
