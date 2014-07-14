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

CertVerifier::CertVerifier(ocsp_download_config odc,
                           ocsp_strict_config osc,
                           ocsp_get_config ogc,
                           pinning_enforcement_config pel)
  : mOCSPDownloadEnabled(odc == ocsp_on)
  , mOCSPStrict(osc == ocsp_strict)
  , mOCSPGETEnabled(ogc == ocsp_get_enabled)
  , mPinningEnforcementLevel(pel)
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

struct ChainValidationCallbackState
{
  const char* hostname;
  const CertVerifier::pinning_enforcement_config pinningEnforcementLevel;
  const SECCertificateUsage usage;
  const PRTime time;
};

SECStatus chainValidationCallback(void* state, const CERTCertList* certList,
                                  PRBool* chainOK)
{
  ChainValidationCallbackState* callbackState =
    reinterpret_cast<ChainValidationCallbackState*>(state);

  *chainOK = PR_FALSE;

  PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
         ("verifycert: Inside the Callback \n"));

  // On sanity failure we fail closed.
  if (!certList) {
    PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
           ("verifycert: Short circuit, callback, sanity check failed \n"));
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }
  if (!callbackState) {
    PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
           ("verifycert: Short circuit, callback, no state! \n"));
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
    return SECFailure;
  }

  if (callbackState->usage != certificateUsageSSLServer ||
      callbackState->pinningEnforcementLevel == CertVerifier::pinningDisabled) {
    PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
           ("verifycert: Callback shortcut pel=%d \n",
            callbackState->pinningEnforcementLevel));
    *chainOK = PR_TRUE;
    return SECSuccess;
  }

  for (CERTCertListNode* node = CERT_LIST_HEAD(certList);
       !CERT_LIST_END(node, certList);
       node = CERT_LIST_NEXT(node)) {
    CERTCertificate* currentCert = node->cert;
    if (CERT_LIST_END(CERT_LIST_NEXT(node), certList)) {
      bool isBuiltInRoot = false;
      SECStatus srv = IsCertBuiltInRoot(currentCert, isBuiltInRoot);
      if (srv != SECSuccess) {
        PR_LOG(gCertVerifierLog, PR_LOG_DEBUG, ("Is BuiltInRoot failure"));
        return srv;
      }
      // If desired, the user can enable "allow user CA MITM mode", in which
      // case key pinning is not enforced for certificates that chain to trust
      // anchors that are not in Mozilla's root program
      if (!isBuiltInRoot &&
          (callbackState->pinningEnforcementLevel ==
             CertVerifier::pinningAllowUserCAMITM)) {
        *chainOK = PR_TRUE;
        return SECSuccess;
      }
    }
  }

  bool enforceTestMode = (callbackState->pinningEnforcementLevel ==
                          CertVerifier::pinningEnforceTestMode);
  *chainOK = PublicKeyPinningService::
    ChainHasValidPins(certList, callbackState->hostname, callbackState->time,
                      enforceTestMode);

  return SECSuccess;
}

static SECStatus
BuildCertChainForOneKeyUsage(TrustDomain& trustDomain, CERTCertificate* cert,
                             PRTime time, KeyUsage ku1, KeyUsage ku2,
                             KeyUsage ku3, KeyPurposeId eku,
                             const CertPolicyId& requiredPolicy,
                             const SECItem* stapledOCSPResponse)
{
  SECStatus rv = BuildCertChain(trustDomain, cert->derCert, time,
                                EndEntityOrCA::MustBeEndEntity, ku1,
                                eku, requiredPolicy, stapledOCSPResponse);
  if (rv != SECSuccess && PR_GetError() == SEC_ERROR_INADEQUATE_KEY_USAGE) {
    rv = BuildCertChain(trustDomain, cert->derCert, time,
                        EndEntityOrCA::MustBeEndEntity, ku2,
                        eku, requiredPolicy, stapledOCSPResponse);
    if (rv != SECSuccess && PR_GetError() == SEC_ERROR_INADEQUATE_KEY_USAGE) {
      rv = BuildCertChain(trustDomain, cert->derCert, time,
                          EndEntityOrCA::MustBeEndEntity, ku3,
                          eku, requiredPolicy, stapledOCSPResponse);
      if (rv != SECSuccess) {
        PR_SetError(SEC_ERROR_INADEQUATE_KEY_USAGE, 0);
      }
    }
  }
  return rv;
}

SECStatus
CertVerifier::VerifyCert(CERTCertificate* cert, SECCertificateUsage usage,
                         PRTime time, void* pinArg, const char* hostname,
                         const Flags flags,
            /*optional*/ const SECItem* stapledOCSPResponse,
        /*optional out*/ ScopedCERTCertList* builtChain,
        /*optional out*/ SECOidTag* evOidPolicy)
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

  if (!cert ||
      (usage != certificateUsageSSLServer && (flags & FLAG_MUST_BE_EV))) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }

  ChainValidationCallbackState callbackState = {
    hostname, mPinningEnforcementLevel, usage, time
  };
  CERTChainVerifyCallback callbackContainer;
  callbackContainer.isChainValid = chainValidationCallback;
  callbackContainer.isChainValidArg = &callbackState;

  NSSCertDBTrustDomain::OCSPFetching ocspFetching
    = !mOCSPDownloadEnabled ||
      (flags & FLAG_LOCAL_ONLY) ? NSSCertDBTrustDomain::NeverFetchOCSP
    : !mOCSPStrict              ? NSSCertDBTrustDomain::FetchOCSPForDVSoftFail
                                : NSSCertDBTrustDomain::FetchOCSPForDVHardFail;

  ocsp_get_config ocspGETConfig = mOCSPGETEnabled ? ocsp_get_enabled
                                                  : ocsp_get_disabled;

  SECStatus rv;

  switch (usage) {
    case certificateUsageSSLClient: {
      // XXX: We don't really have a trust bit for SSL client authentication so
      // just use trustEmail as it is the closest alternative.
      NSSCertDBTrustDomain trustDomain(trustEmail, ocspFetching, mOCSPCache,
                                       pinArg, ocspGETConfig, nullptr,
                                       builtChain);
      rv = BuildCertChain(trustDomain, cert->derCert, time,
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
      rv = GetFirstEVPolicy(cert, evPolicy, evPolicyOidTag);
      if (rv == SECSuccess) {
        NSSCertDBTrustDomain
          trustDomain(trustSSL,
                      ocspFetching == NSSCertDBTrustDomain::NeverFetchOCSP
                        ? NSSCertDBTrustDomain::LocalOnlyOCSPForEV
                        : NSSCertDBTrustDomain::FetchOCSPForEV,
                      mOCSPCache, pinArg, ocspGETConfig,
                      &callbackContainer, builtChain);
        rv = BuildCertChainForOneKeyUsage(trustDomain, cert, time,
                                          KeyUsage::digitalSignature,// (EC)DHE
                                          KeyUsage::keyEncipherment, // RSA
                                          KeyUsage::keyAgreement,    // (EC)DH
                                          KeyPurposeId::id_kp_serverAuth,
                                          evPolicy, stapledOCSPResponse);
        if (rv == SECSuccess) {
          if (evOidPolicy) {
            *evOidPolicy = evPolicyOidTag;
          }
          break;
        }
      }
#endif

      if (flags & FLAG_MUST_BE_EV) {
        PR_SetError(SEC_ERROR_POLICY_VALIDATION_FAILED, 0);
        rv = SECFailure;
        break;
      }

      // Now try non-EV.
      NSSCertDBTrustDomain trustDomain(trustSSL, ocspFetching, mOCSPCache,
                                       pinArg, ocspGETConfig, &callbackContainer,
                                       builtChain);
      rv = BuildCertChainForOneKeyUsage(trustDomain, cert, time,
                                        KeyUsage::digitalSignature, // (EC)DHE
                                        KeyUsage::keyEncipherment, // RSA
                                        KeyUsage::keyAgreement, // (EC)DH
                                        KeyPurposeId::id_kp_serverAuth,
                                        CertPolicyId::anyPolicy,
                                        stapledOCSPResponse);
      break;
    }

    case certificateUsageSSLCA: {
      NSSCertDBTrustDomain trustDomain(trustSSL, ocspFetching, mOCSPCache,
                                       pinArg, ocspGETConfig, nullptr,
                                       builtChain);
      rv = BuildCertChain(trustDomain, cert->derCert, time,
                          EndEntityOrCA::MustBeCA, KeyUsage::keyCertSign,
                          KeyPurposeId::id_kp_serverAuth,
                          CertPolicyId::anyPolicy, stapledOCSPResponse);
      break;
    }

    case certificateUsageEmailSigner: {
      NSSCertDBTrustDomain trustDomain(trustEmail, ocspFetching, mOCSPCache,
                                       pinArg, ocspGETConfig, nullptr,
                                       builtChain);
      rv = BuildCertChain(trustDomain, cert->derCert, time,
                          EndEntityOrCA::MustBeEndEntity,
                          KeyUsage::digitalSignature,
                          KeyPurposeId::id_kp_emailProtection,
                          CertPolicyId::anyPolicy, stapledOCSPResponse);
      break;
    }

    case certificateUsageEmailRecipient: {
      // TODO: The higher level S/MIME processing should pass in which key
      // usage it is trying to verify for, and base its algorithm choices
      // based on the result of the verification(s).
      NSSCertDBTrustDomain trustDomain(trustEmail, ocspFetching, mOCSPCache,
                                       pinArg, ocspGETConfig, nullptr,
                                       builtChain);
      rv = BuildCertChain(trustDomain, cert->derCert, time,
                          EndEntityOrCA::MustBeEndEntity,
                          KeyUsage::keyEncipherment, // RSA
                          KeyPurposeId::id_kp_emailProtection,
                          CertPolicyId::anyPolicy, stapledOCSPResponse);
      if (rv != SECSuccess && PR_GetError() == SEC_ERROR_INADEQUATE_KEY_USAGE) {
        rv = BuildCertChain(trustDomain, cert->derCert, time,
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
                                       nullptr, builtChain);
      rv = BuildCertChain(trustDomain, cert->derCert, time,
                          EndEntityOrCA::MustBeEndEntity,
                          KeyUsage::digitalSignature,
                          KeyPurposeId::id_kp_codeSigning,
                          CertPolicyId::anyPolicy, stapledOCSPResponse);
      break;
    }

    case certificateUsageVerifyCA:
    case certificateUsageStatusResponder: {
      // XXX This is a pretty useless way to verify a certificate. It is used
      // by the implementation of window.crypto.importCertificates and in the
      // certificate viewer UI. Because we don't know what trust bit is
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
                                    ocspGETConfig, nullptr, builtChain);
      rv = BuildCertChain(sslTrust, cert->derCert, time, endEntityOrCA,
                          keyUsage, eku, CertPolicyId::anyPolicy,
                          stapledOCSPResponse);
      if (rv == SECFailure && PR_GetError() == SEC_ERROR_UNKNOWN_ISSUER) {
        NSSCertDBTrustDomain emailTrust(trustEmail, ocspFetching, mOCSPCache,
                                        pinArg, ocspGETConfig, nullptr,
                                        builtChain);
        rv = BuildCertChain(emailTrust, cert->derCert, time, endEntityOrCA,
                            keyUsage, eku, CertPolicyId::anyPolicy,
                            stapledOCSPResponse);
        if (rv == SECFailure && PR_GetError() == SEC_ERROR_UNKNOWN_ISSUER) {
          NSSCertDBTrustDomain objectSigningTrust(trustObjectSigning,
                                                  ocspFetching, mOCSPCache,
                                                  pinArg, ocspGETConfig,
                                                  nullptr, builtChain);
          rv = BuildCertChain(objectSigningTrust, cert->derCert, time,
                              endEntityOrCA, keyUsage, eku,
                              CertPolicyId::anyPolicy, stapledOCSPResponse);
        }
      }

      break;
    }

    default:
      PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
      return SECFailure;
  }

  return rv;
}

SECStatus
CertVerifier::VerifySSLServerCert(CERTCertificate* peerCert,
                     /*optional*/ const SECItem* stapledOCSPResponse,
                                  PRTime time,
                     /*optional*/ void* pinarg,
                                  const char* hostname,
                                  bool saveIntermediatesInPermanentDatabase,
                 /*optional out*/ ScopedCERTCertList* builtChain,
                 /*optional out*/ SECOidTag* evOidPolicy)
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
  // CreateCertErrorRunnable assumes that CERT_VerifyCertName is only called
  // if VerifyCert succeeded.
  SECStatus rv = VerifyCert(peerCert, certificateUsageSSLServer, time, pinarg,
                            hostname, 0, stapledOCSPResponse, &builtChainTemp,
                            evOidPolicy);
  if (rv != SECSuccess) {
    return rv;
  }

  rv = CERT_VerifyCertName(peerCert, hostname);
  if (rv != SECSuccess) {
    return rv;
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
