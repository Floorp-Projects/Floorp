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
#include "PublicKeyPinningService.h"
#include "cert.h"
#include "pk11pub.h"
#include "pkix/pkix.h"
#include "pkix/pkixnss.h"
#include "prerror.h"
#include "secerr.h"
#include "sslerr.h"

using namespace mozilla::pkix;
using namespace mozilla::psm;

#ifdef PR_LOGGING
PRLogModuleInfo* gCertVerifierLog = nullptr;
#endif

namespace mozilla { namespace psm {

const CertVerifier::Flags CertVerifier::FLAG_LOCAL_ONLY = 1;
const CertVerifier::Flags CertVerifier::FLAG_MUST_BE_EV = 2;

CertVerifier::CertVerifier(OcspDownloadConfig odc,
                           OcspStrictConfig osc,
                           OcspGetConfig ogc,
                           PinningMode pinningMode)
  : mOCSPDownloadEnabled(odc == ocspOn)
  , mOCSPStrict(osc == ocspStrict)
  , mOCSPGETEnabled(ogc == ocspGetEnabled)
  , mPinningMode(pinningMode)
{
}

CertVerifier::~CertVerifier()
{
}

void
InitCertVerifierLog()
{
#ifdef PR_LOGGING
  if (!gCertVerifierLog) {
    gCertVerifierLog = PR_NewLogModule("certverifier");
  }
#endif
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
    PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
           ("BuiltInRoot? subject=%s token=%s",cert->subjectName, token));
    if (strcmp("Builtin Object Token", token) == 0) {
      result = true;
      return SECSuccess;
    }
  }
  return SECSuccess;
}

Result
CertListContainsExpectedKeys(const CERTCertList* certList,
                             const char* hostname, Time time,
                             CertVerifier::PinningMode pinningMode)
{
  if (pinningMode == CertVerifier::pinningDisabled) {
    PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
           ("Pinning is disabled; not checking keys."));
    return Success;
  }

  if (!certList) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }

  CERTCertListNode* rootNode = CERT_LIST_TAIL(certList);
  if (CERT_LIST_END(rootNode, certList)) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }

  bool isBuiltInRoot = false;
  SECStatus srv = IsCertBuiltInRoot(rootNode->cert, isBuiltInRoot);
  if (srv != SECSuccess) {
    return MapPRErrorCodeToResult(PR_GetError());
  }
  // If desired, the user can enable "allow user CA MITM mode", in which
  // case key pinning is not enforced for certificates that chain to trust
  // anchors that are not in Mozilla's root program
  if (!isBuiltInRoot && pinningMode == CertVerifier::pinningAllowUserCAMITM) {
    return Success;
  }

  bool enforceTestMode = (pinningMode == CertVerifier::pinningEnforceTestMode);
  if (PublicKeyPinningService::ChainHasValidPins(certList, hostname, time,
                                                 enforceTestMode)) {
    return Success;
  }

  return Result::ERROR_KEY_PINNING_FAILURE;
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

SECStatus
CertVerifier::VerifyCert(CERTCertificate* cert, SECCertificateUsage usage,
                         Time time, void* pinArg, const char* hostname,
                         const Flags flags,
            /*optional*/ const SECItem* stapledOCSPResponseSECItem,
        /*optional out*/ ScopedCERTCertList* builtChain,
        /*optional out*/ SECOidTag* evOidPolicy,
        /*optional out*/ OCSPStaplingStatus* ocspStaplingStatus)
{
  PR_LOG(gCertVerifierLog, PR_LOG_DEBUG, ("Top of VerifyCert\n"));

  PR_ASSERT(cert);
  PR_ASSERT(usage == certificateUsageSSLServer || !(flags & FLAG_MUST_BE_EV));

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

  NSSCertDBTrustDomain::OCSPFetching ocspFetching
    = !mOCSPDownloadEnabled ||
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
      NSSCertDBTrustDomain trustDomain(trustEmail, ocspFetching, mOCSPCache,
                                       pinArg, ocspGETConfig, pinningDisabled,
                                       false, nullptr, builtChain);
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

#ifndef MOZ_NO_EV_CERTS
      // Try to validate for EV first.
      CertPolicyId evPolicy;
      SECOidTag evPolicyOidTag;
      SECStatus srv = GetFirstEVPolicy(cert, evPolicy, evPolicyOidTag);
      if (srv == SECSuccess) {
        NSSCertDBTrustDomain
          trustDomain(trustSSL,
                      ocspFetching == NSSCertDBTrustDomain::NeverFetchOCSP
                        ? NSSCertDBTrustDomain::LocalOnlyOCSPForEV
                        : NSSCertDBTrustDomain::FetchOCSPForEV,
                      mOCSPCache, pinArg, ocspGETConfig, mPinningMode, true,
                      hostname, builtChain);
        rv = BuildCertChainForOneKeyUsage(trustDomain, certDER, time,
                                          KeyUsage::digitalSignature,// (EC)DHE
                                          KeyUsage::keyEncipherment, // RSA
                                          KeyUsage::keyAgreement,    // (EC)DH
                                          KeyPurposeId::id_kp_serverAuth,
                                          evPolicy, stapledOCSPResponse,
                                          ocspStaplingStatus);
        if (rv == Success) {
          if (evOidPolicy) {
            *evOidPolicy = evPolicyOidTag;
          }
          break;
        }
      }
#endif

      if (flags & FLAG_MUST_BE_EV) {
        rv = Result::ERROR_POLICY_VALIDATION_FAILED;
        break;
      }

      // Now try non-EV.
      NSSCertDBTrustDomain trustDomain(trustSSL, ocspFetching, mOCSPCache,
                                       pinArg, ocspGETConfig, mPinningMode,
                                       false, hostname, builtChain);
      rv = BuildCertChainForOneKeyUsage(trustDomain, certDER, time,
                                        KeyUsage::digitalSignature, // (EC)DHE
                                        KeyUsage::keyEncipherment, // RSA
                                        KeyUsage::keyAgreement, // (EC)DH
                                        KeyPurposeId::id_kp_serverAuth,
                                        CertPolicyId::anyPolicy,
                                        stapledOCSPResponse,
                                        ocspStaplingStatus);
      break;
    }

    case certificateUsageSSLCA: {
      NSSCertDBTrustDomain trustDomain(trustSSL, ocspFetching, mOCSPCache,
                                       pinArg, ocspGETConfig, pinningDisabled,
                                       false, nullptr, builtChain);
      rv = BuildCertChain(trustDomain, certDER, time,
                          EndEntityOrCA::MustBeCA, KeyUsage::keyCertSign,
                          KeyPurposeId::id_kp_serverAuth,
                          CertPolicyId::anyPolicy, stapledOCSPResponse);
      break;
    }

    case certificateUsageEmailSigner: {
      NSSCertDBTrustDomain trustDomain(trustEmail, ocspFetching, mOCSPCache,
                                       pinArg, ocspGETConfig, pinningDisabled,
                                       false, nullptr, builtChain);
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
      NSSCertDBTrustDomain trustDomain(trustEmail, ocspFetching, mOCSPCache,
                                       pinArg, ocspGETConfig, pinningDisabled,
                                       false, nullptr, builtChain);
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
      NSSCertDBTrustDomain trustDomain(trustObjectSigning, ocspFetching,
                                       mOCSPCache, pinArg, ocspGETConfig,
                                       pinningDisabled, false, nullptr,
                                       builtChain);
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

      NSSCertDBTrustDomain sslTrust(trustSSL, ocspFetching, mOCSPCache, pinArg,
                                    ocspGETConfig, pinningDisabled, false,
                                    nullptr, builtChain);
      rv = BuildCertChain(sslTrust, certDER, time, endEntityOrCA,
                          keyUsage, eku, CertPolicyId::anyPolicy,
                          stapledOCSPResponse);
      if (rv == Result::ERROR_UNKNOWN_ISSUER) {
        NSSCertDBTrustDomain emailTrust(trustEmail, ocspFetching, mOCSPCache,
                                        pinArg, ocspGETConfig, pinningDisabled,
                                        false, nullptr, builtChain);
        rv = BuildCertChain(emailTrust, certDER, time, endEntityOrCA,
                            keyUsage, eku, CertPolicyId::anyPolicy,
                            stapledOCSPResponse);
        if (rv == Result::ERROR_UNKNOWN_ISSUER) {
          NSSCertDBTrustDomain objectSigningTrust(trustObjectSigning,
                                                  ocspFetching, mOCSPCache,
                                                  pinArg, ocspGETConfig,
                                                  pinningDisabled, false,
                                                  nullptr, builtChain);
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
    if (rv != Result::ERROR_KEY_PINNING_FAILURE &&
        usage == certificateUsageSSLServer) {
      ScopedCERTCertificate certCopy(CERT_DupCertificate(cert));
      if (!certCopy) {
        return SECFailure;
      }
      ScopedCERTCertList certList(CERT_NewCertList());
      if (!certList) {
        return SECFailure;
      }
      SECStatus srv = CERT_AddCertToListTail(certList.get(), certCopy.get());
      if (srv != SECSuccess) {
        return SECFailure;
      }
      certCopy.forget(); // now owned by certList
      Result pinningResult = CertListContainsExpectedKeys(certList, hostname,
                                                          time, mPinningMode);
      if (pinningResult != Success) {
        rv = pinningResult;
      }
    }
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
                 /*optional out*/ OCSPStaplingStatus* ocspStaplingStatus)
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
                            &builtChainTemp, evOidPolicy, ocspStaplingStatus);
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
    PR_SetError(MapResultToPRErrorCode(result), 0);
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
