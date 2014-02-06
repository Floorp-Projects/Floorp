/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CertVerifier.h"

#include <stdint.h>

#include "insanity/pkixtypes.h"
#include "ExtendedValidation.h"
#include "NSSCertDBTrustDomain.h"
#include "cert.h"
#include "ocsp.h"
#include "secerr.h"
#include "prerror.h"
#include "sslerr.h"

// ScopedXXX in this file are insanity::pkix::ScopedXXX, not
// mozilla::ScopedXXX.
using namespace insanity::pkix;
using namespace mozilla::psm;

#ifdef PR_LOGGING
static PRLogModuleInfo* gCertVerifierLog = nullptr;
#endif

namespace mozilla { namespace psm {

const CertVerifier::Flags CertVerifier::FLAG_LOCAL_ONLY = 1;
const CertVerifier::Flags CertVerifier::FLAG_MUST_BE_EV = 2;

CertVerifier::CertVerifier(implementation_config ic,
                           missing_cert_download_config mcdc,
                           crl_download_config cdc,
                           ocsp_download_config odc,
                           ocsp_strict_config osc,
                           ocsp_get_config ogc)
  : mImplementation(ic)
  , mMissingCertDownloadEnabled(mcdc == missing_cert_download_on)
  , mCRLDownloadEnabled(cdc == crl_download_allowed)
  , mOCSPDownloadEnabled(odc == ocsp_on)
  , mOCSPStrict(osc == ocsp_strict)
  , mOCSPGETEnabled(ogc == ocsp_get_enabled)
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

#if 0
// Once we migrate to insanity::pkix or change the overridable error
// logic this will become unnecesary.
static SECStatus
insertErrorIntoVerifyLog(CERTCertificate* cert, const PRErrorCode err,
                         CERTVerifyLog* verifyLog){
  CERTVerifyLogNode* node;
  node = (CERTVerifyLogNode *)PORT_ArenaAlloc(verifyLog->arena,
                                              sizeof(CERTVerifyLogNode));
  if (!node) {
    PR_SetError(PR_UNKNOWN_ERROR, 0);
    return SECFailure;
  }
  node->cert = CERT_DupCertificate(cert);
  node->error = err;
  node->depth = 0;
  node->arg = nullptr;
  //and at to head!
  node->prev = nullptr;
  node->next = verifyLog->head;
  if (verifyLog->head) {
    verifyLog->head->prev = node;
  }
  verifyLog->head = node;
  if (!verifyLog->tail) {
    verifyLog->tail = node;
  }
  verifyLog->count++;

  return SECSuccess;
}
#endif

static SECStatus
ClassicVerifyCert(CERTCertificate* cert,
                  const SECCertificateUsage usage,
                  const PRTime time,
                  void* pinArg,
                  /*optional out*/ ScopedCERTCertList* validationChain,
                  /*optional out*/ CERTVerifyLog* verifyLog)
{
  SECStatus rv;
  SECCertUsage enumUsage;
  if (validationChain) {
    switch(usage){
      case  certificateUsageSSLClient:
        enumUsage = certUsageSSLClient;
        break;
      case  certificateUsageSSLServer:
        enumUsage = certUsageSSLServer;
        break;
      case certificateUsageSSLServerWithStepUp:
        enumUsage = certUsageSSLServerWithStepUp;
        break;
      case certificateUsageSSLCA:
        enumUsage = certUsageSSLCA;
        break;
      case certificateUsageEmailSigner:
        enumUsage = certUsageEmailSigner;
        break;
      case certificateUsageEmailRecipient:
        enumUsage = certUsageEmailRecipient;
        break;
      case certificateUsageObjectSigner:
        enumUsage = certUsageObjectSigner;
        break;
      case certificateUsageUserCertImport:
        enumUsage = certUsageUserCertImport;
        break;
      case certificateUsageVerifyCA:
        enumUsage = certUsageVerifyCA;
        break;
      case certificateUsageProtectedObjectSigner:
        enumUsage = certUsageProtectedObjectSigner;
        break;
      case certificateUsageStatusResponder:
        enumUsage = certUsageStatusResponder;
        break;
      case certificateUsageAnyCA:
        enumUsage = certUsageAnyCA;
        break;
       default:
        return SECFailure;
    }
  }
  if (usage == certificateUsageSSLServer) {
    // SSL server cert verification has always used CERT_VerifyCert, so we
    // continue to use it for SSL cert verification to minimize the risk of
    // there being any differnce in results between CERT_VerifyCert and
    // CERT_VerifyCertificate.
    rv = CERT_VerifyCert(CERT_GetDefaultCertDB(), cert, true,
                         certUsageSSLServer, time, pinArg, verifyLog);
  } else {
    rv = CERT_VerifyCertificate(CERT_GetDefaultCertDB(), cert, true,
                                usage, time, pinArg, verifyLog, nullptr);
  }
  if (rv == SECSuccess && validationChain) {
    PR_LOG(gCertVerifierLog, PR_LOG_DEBUG, ("VerifyCert: getting chain in 'classic' \n"));
    *validationChain = CERT_GetCertChainFromCert(cert, time, enumUsage);
    if (!*validationChain) {
      rv = SECFailure;
    }
  }
  return rv;
}

#ifndef NSS_NO_LIBPKIX
static void
destroyCertListThatShouldNotExist(CERTCertList** certChain)
{
  PR_ASSERT(certChain);
  PR_ASSERT(!*certChain);
  if (certChain && *certChain) {
    // There SHOULD not be a validation chain on failure, asserion here for
    // the debug builds AND a fallback for production builds
    CERT_DestroyCertList(*certChain);
    *certChain = nullptr;
  }
}
#endif

SECStatus
CertVerifier::VerifyCert(CERTCertificate* cert,
            /*optional*/ const SECItem* stapledOCSPResponse,
                         const SECCertificateUsage usage,
                         const PRTime time,
                         void* pinArg,
                         const Flags flags,
                         /*optional out*/ ScopedCERTCertList* validationChain,
                         /*optional out*/ SECOidTag* evOidPolicy,
                         /*optional out*/ CERTVerifyLog* verifyLog)
{
  if (!cert)
  {
    PR_NOT_REACHED("Invalid arguments to CertVerifier::VerifyCert");
    PORT_SetError(SEC_ERROR_INVALID_ARGS);
    return SECFailure;
  }
  if (validationChain) {
    *validationChain = nullptr;
  }
  if (evOidPolicy) {
    *evOidPolicy = SEC_OID_UNKNOWN;
  }

  switch(usage){
    case certificateUsageSSLClient:
    case certificateUsageSSLServer:
    case certificateUsageSSLCA:
    case certificateUsageEmailSigner:
    case certificateUsageEmailRecipient:
    case certificateUsageObjectSigner:
    case certificateUsageStatusResponder:
      break;
    default:
      PORT_SetError(SEC_ERROR_INVALID_ARGS);
      return SECFailure;
  }

  if ((flags & FLAG_MUST_BE_EV) && usage != certificateUsageSSLServer) {
      PORT_SetError(SEC_ERROR_INVALID_ARGS);
      return SECFailure;
  }

#ifndef NSS_NO_LIBPKIX
  ScopedCERTCertList trustAnchors;
  SECStatus rv;
  SECOidTag evPolicy = SEC_OID_UNKNOWN;

  // Do EV checking only for sslserver usage
  if (usage == certificateUsageSSLServer) {
    SECStatus srv = GetFirstEVPolicy(cert, evPolicy);
    if (srv == SECSuccess) {
      if (evPolicy != SEC_OID_UNKNOWN) {
        trustAnchors = GetRootsForOid(evPolicy);
      }
      if (!trustAnchors) {
        return SECFailure;
      }
      // pkix ignores an empty trustanchors list and
      // decides then to use the whole set of trust in the DB
      // so we set the evPolicy to unkown in this case
      if (CERT_LIST_EMPTY(trustAnchors)) {
        evPolicy = SEC_OID_UNKNOWN;
      }
    } else {
      // No known EV policy found
      if (flags & FLAG_MUST_BE_EV) {
        PORT_SetError(SEC_ERROR_EXTENSION_NOT_FOUND);
        return SECFailure;
      }
      // Do not setup EV verification params
      evPolicy = SEC_OID_UNKNOWN;
    }
    if ((evPolicy == SEC_OID_UNKNOWN) && (flags & FLAG_MUST_BE_EV)) {
      PORT_SetError(SEC_ERROR_UNKNOWN_ISSUER);
      return SECFailure;
    }
  }

  PR_ASSERT(evPolicy == SEC_OID_UNKNOWN || trustAnchors);

  size_t i = 0;
  size_t validationChainLocation = 0;
  size_t validationTrustAnchorLocation = 0;
  CERTValOutParam cvout[4];
  if (verifyLog) {
     cvout[i].type = cert_po_errorLog;
     cvout[i].value.pointer.log = verifyLog;
     ++i;
  }
  if (validationChain) {
    PR_LOG(gCertVerifierLog, PR_LOG_DEBUG, ("VerifyCert: setting up validation chain outparam.\n"));
    validationChainLocation = i;
    cvout[i].type = cert_po_certList;
    cvout[i].value.pointer.chain = nullptr;
    ++i;
    validationTrustAnchorLocation = i;
    cvout[i].type = cert_po_trustAnchor;
    cvout[i].value.pointer.cert = nullptr;
    ++i;
  }
  cvout[i].type = cert_po_end;

  CERTRevocationFlags rev;

  CERTRevocationMethodIndex revPreferredMethods[2];
  rev.leafTests.preferred_methods =
  rev.chainTests.preferred_methods = revPreferredMethods;

  uint64_t revFlagsPerMethod[2];
  rev.leafTests.cert_rev_flags_per_method =
  rev.chainTests.cert_rev_flags_per_method = revFlagsPerMethod;
  rev.leafTests.number_of_preferred_methods =
  rev.chainTests.number_of_preferred_methods = 1;

  rev.leafTests.number_of_defined_methods =
  rev.chainTests.number_of_defined_methods = cert_revocation_method_ocsp + 1;

  const bool localOnly = flags & FLAG_LOCAL_ONLY;
  CERTValInParam cvin[6];

  // Parameters for both EV and DV validation
  cvin[0].type = cert_pi_useAIACertFetch;
  cvin[0].value.scalar.b = mMissingCertDownloadEnabled && !localOnly;
  cvin[1].type = cert_pi_revocationFlags;
  cvin[1].value.pointer.revocation = &rev;
  cvin[2].type = cert_pi_date;
  cvin[2].value.scalar.time = time;
  i = 3;
  const size_t evParamLocation = i;

  if (evPolicy != SEC_OID_UNKNOWN) {
    // EV setup!
    // XXX 859872 The current flags are not quite correct. (use
    // of ocsp flags for crl preferences).
    uint64_t ocspRevMethodFlags =
      CERT_REV_M_TEST_USING_THIS_METHOD
      | ((mOCSPDownloadEnabled && !localOnly) ?
          CERT_REV_M_ALLOW_NETWORK_FETCHING : CERT_REV_M_FORBID_NETWORK_FETCHING)
      | CERT_REV_M_ALLOW_IMPLICIT_DEFAULT_SOURCE
      | CERT_REV_M_REQUIRE_INFO_ON_MISSING_SOURCE
      | CERT_REV_M_IGNORE_MISSING_FRESH_INFO
      | CERT_REV_M_STOP_TESTING_ON_FRESH_INFO
      | (mOCSPGETEnabled ? 0 : CERT_REV_M_FORCE_POST_METHOD_FOR_OCSP);

    rev.leafTests.cert_rev_flags_per_method[cert_revocation_method_crl] =
    rev.chainTests.cert_rev_flags_per_method[cert_revocation_method_crl]
      = CERT_REV_M_DO_NOT_TEST_USING_THIS_METHOD;

    rev.leafTests.cert_rev_flags_per_method[cert_revocation_method_ocsp] =
    rev.chainTests.cert_rev_flags_per_method[cert_revocation_method_ocsp]
      = ocspRevMethodFlags;

    rev.leafTests.cert_rev_method_independent_flags =
    rev.chainTests.cert_rev_method_independent_flags =
      // avoiding the network is good, let's try local first
      CERT_REV_MI_TEST_ALL_LOCAL_INFORMATION_FIRST
      // is overall revocation requirement strict or relaxed?
      |  CERT_REV_MI_REQUIRE_SOME_FRESH_INFO_AVAILABLE
      ;

    rev.leafTests.preferred_methods[0] =
    rev.chainTests.preferred_methods[0] = cert_revocation_method_ocsp;

    cvin[i].type = cert_pi_policyOID;
    cvin[i].value.arraySize = 1;
    cvin[i].value.array.oids = &evPolicy;
    ++i;
    PR_ASSERT(trustAnchors);
    cvin[i].type = cert_pi_trustAnchors;
    cvin[i].value.pointer.chain = trustAnchors.get();
    ++i;

    cvin[i].type = cert_pi_end;

    rv = CERT_PKIXVerifyCert(cert, usage, cvin, cvout, pinArg);
    if (rv == SECSuccess) {
      if (evOidPolicy) {
        *evOidPolicy = evPolicy;
      }
      PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
             ("VerifyCert: successful CERT_PKIXVerifyCert(ev) \n"));
      goto pkix_done;
    }
    PR_LOG(gCertVerifierLog, PR_LOG_DEBUG,
           ("VerifyCert: failed CERT_PKIXVerifyCert(ev)\n"));
    if (flags & FLAG_MUST_BE_EV) {
      return rv;
    }
    if (validationChain) {
      destroyCertListThatShouldNotExist(
        &cvout[validationChainLocation].value.pointer.chain);
    }

    if (verifyLog) {
      // Cleanup the log so that it is ready the the next validation
      CERTVerifyLogNode* i_node;
      for (i_node = verifyLog->head; i_node; i_node = i_node->next) {
         //destroy cert if any.
         if (i_node->cert) {
           CERT_DestroyCertificate(i_node->cert);
         }
         // No need to cleanup the actual nodes in the arena.
      }
      verifyLog->count = 0;
      verifyLog->head = nullptr;
      verifyLog->tail = nullptr;
    }

  }
#endif

  // If we're here, PKIX EV verification failed.
  // If requested, don't do DV fallback.
  if (flags & FLAG_MUST_BE_EV) {
    PR_ASSERT(*evOidPolicy == SEC_OID_UNKNOWN);
#ifdef NSS_NO_LIBPKIX
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
#else
    PR_SetError(PR_INVALID_STATE_ERROR, 0);
#endif
    return SECFailure;
  }

  if (mImplementation == classic) {
    // XXX: we do not care about the localOnly flag (currently) as the
    // caller that wants localOnly should disable and reenable the fetching.
    return ClassicVerifyCert(cert, usage, time, pinArg, validationChain,
                             verifyLog);
  }

#ifdef NSS_NO_LIBPKIX
  PR_NOT_REACHED("libpkix implementation chosen but not even compiled in");
  PR_SetError(PR_INVALID_STATE_ERROR, 0);
  return SECFailure;
#else
  PR_ASSERT(mImplementation == libpkix);

  // The current flags check the chain the same way as the leafs
  rev.leafTests.cert_rev_flags_per_method[cert_revocation_method_crl] =
  rev.chainTests.cert_rev_flags_per_method[cert_revocation_method_crl] =
    // implicit default source - makes no sense for CRLs
    CERT_REV_M_IGNORE_IMPLICIT_DEFAULT_SOURCE

    // let's not stop on fresh CRL. If OCSP is enabled, too, let's check it
    | CERT_REV_M_CONTINUE_TESTING_ON_FRESH_INFO

    // no fresh CRL? well, let other flag decide whether to fail or not
    | CERT_REV_M_IGNORE_MISSING_FRESH_INFO

    // testing using local CRLs is always allowed
    | CERT_REV_M_TEST_USING_THIS_METHOD

    // no local crl and don't know where to get it from? ignore
    | CERT_REV_M_SKIP_TEST_ON_MISSING_SOURCE

    // crl download based on parameter
    | ((mCRLDownloadEnabled && !localOnly) ?
        CERT_REV_M_ALLOW_NETWORK_FETCHING : CERT_REV_M_FORBID_NETWORK_FETCHING)
    ;

  rev.leafTests.cert_rev_flags_per_method[cert_revocation_method_ocsp] =
  rev.chainTests.cert_rev_flags_per_method[cert_revocation_method_ocsp] =
    // use OCSP
      CERT_REV_M_TEST_USING_THIS_METHOD

    // if app has a default OCSP responder configured, let's use it
    | CERT_REV_M_ALLOW_IMPLICIT_DEFAULT_SOURCE

    // of course OCSP doesn't work without a source. let's accept such certs
    | CERT_REV_M_SKIP_TEST_ON_MISSING_SOURCE

    // if ocsp is required stop on lack of freshness
    | (mOCSPStrict ?
       CERT_REV_M_FAIL_ON_MISSING_FRESH_INFO : CERT_REV_M_IGNORE_MISSING_FRESH_INFO)

    // ocsp success is sufficient
    | CERT_REV_M_STOP_TESTING_ON_FRESH_INFO

    // ocsp enabled controls network fetching, too
    | ((mOCSPDownloadEnabled && !localOnly) ?
        CERT_REV_M_ALLOW_NETWORK_FETCHING : CERT_REV_M_FORBID_NETWORK_FETCHING)

    | (mOCSPGETEnabled ? 0 : CERT_REV_M_FORCE_POST_METHOD_FOR_OCSP);
    ;

  rev.leafTests.preferred_methods[0] =
  rev.chainTests.preferred_methods[0] = cert_revocation_method_ocsp;

  rev.leafTests.cert_rev_method_independent_flags =
  rev.chainTests.cert_rev_method_independent_flags =
    // avoiding the network is good, let's try local first
    CERT_REV_MI_TEST_ALL_LOCAL_INFORMATION_FIRST;

  // Skip EV parameters
  cvin[evParamLocation].type = cert_pi_end;

  PR_LOG(gCertVerifierLog, PR_LOG_DEBUG, ("VerifyCert: calling CERT_PKIXVerifyCert(dv) \n"));
  rv = CERT_PKIXVerifyCert(cert, usage, cvin, cvout, pinArg);

pkix_done:
  if (validationChain) {
    PR_LOG(gCertVerifierLog, PR_LOG_DEBUG, ("VerifyCert: validation chain requested\n"));
    ScopedCERTCertificate trustAnchor(cvout[validationTrustAnchorLocation].value.pointer.cert);

    if (rv == SECSuccess) {
      if (! cvout[validationChainLocation].value.pointer.chain) {
        PR_SetError(PR_UNKNOWN_ERROR, 0);
        return SECFailure;
      }
      PR_LOG(gCertVerifierLog, PR_LOG_DEBUG, ("VerifyCert: I have a chain\n"));
      *validationChain = cvout[validationChainLocation].value.pointer.chain;
      if (trustAnchor) {
        // we should only add the issuer to the chain if it is not already
        // present. On CA cert checking, the issuer is the same cert, so in
        // that case we do not add the cert to the chain.
        if (!CERT_CompareCerts(trustAnchor.get(), cert)) {
          PR_LOG(gCertVerifierLog, PR_LOG_DEBUG, ("VerifyCert:  adding issuer to tail for display\n"));
          // note: rv is reused to catch errors on cert creation!
          ScopedCERTCertificate tempCert(CERT_DupCertificate(trustAnchor.get()));
          rv = CERT_AddCertToListTail(validationChain->get(), tempCert.get());
          if (rv == SECSuccess) {
            tempCert.release(); // ownership traferred to validationChain
          } else {
            *validationChain = nullptr;
          }
        }
      }
    } else {
      destroyCertListThatShouldNotExist(
        &cvout[validationChainLocation].value.pointer.chain);
    }
  }

  return rv;
#endif
}

SECStatus
CertVerifier::VerifySSLServerCert(CERTCertificate* peerCert,
                     /*optional*/ const SECItem* stapledOCSPResponse,
                                  PRTime time,
                     /*optional*/ void* pinarg,
                                  const char* hostname,
                                  bool saveIntermediatesInPermanentDatabase,
                 /*optional out*/ insanity::pkix::ScopedCERTCertList* certChainOut,
                 /*optional out*/ SECOidTag* evOidPolicy)
{
  PR_ASSERT(peerCert);
  // XXX: PR_ASSERT(pinarg)
  PR_ASSERT(hostname);
  PR_ASSERT(hostname[0]);

  if (certChainOut) {
    *certChainOut = nullptr;
  }
  if (evOidPolicy) {
    *evOidPolicy = SEC_OID_UNKNOWN;
  }

  if (!hostname || !hostname[0]) {
    PR_SetError(SSL_ERROR_BAD_CERT_DOMAIN, 0);
    return SECFailure;
  }

  ScopedCERTCertList validationChain;
  SECStatus rv = VerifyCert(peerCert, stapledOCSPResponse,
                            certificateUsageSSLServer, time,
                            pinarg, 0, &validationChain, evOidPolicy);
  if (rv != SECSuccess) {
    return rv;
  }

  rv = CERT_VerifyCertName(peerCert, hostname);
  if (rv != SECSuccess) {
    return rv;
  }

  if (saveIntermediatesInPermanentDatabase) {
    SaveIntermediateCerts(validationChain);
  }

  if (certChainOut) {
    *certChainOut = validationChain.release();
  }

  return SECSuccess;
}

} } // namespace mozilla::psm
