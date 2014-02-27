/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* Copyright 2013 Mozilla Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <limits>

#include "insanity/bind.h"
#include "insanity/pkix.h"
#include "pkixcheck.h"
#include "pkixder.h"

#include "hasht.h"
#include "pk11pub.h"
#include "secder.h"

#ifdef _MSC_VER
// C4480: nonstandard extension used: specifying underlying type for enum
#define ENUM_CLASS  __pragma(warning(disable: 4480)) enum
#else
#define ENUM_CLASS enum class
#endif

// TODO: use typed/qualified typedefs everywhere?
// TODO: When should we return SEC_ERROR_OCSP_UNAUTHORIZED_RESPONSE?

namespace insanity { namespace pkix {

static const PRTime ONE_DAY
  = INT64_C(24) * INT64_C(60) * INT64_C(60) * PR_USEC_PER_SEC;
static const PRTime SLOP = ONE_DAY;

// These values correspond to the tag values in the ASN.1 CertStatus
ENUM_CLASS CertStatus : uint8_t {
  Good = der::CONTEXT_SPECIFIC | 0,
  Revoked = der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 1,
  Unknown = der::CONTEXT_SPECIFIC | 2
};

class Context
{
public:
  Context(TrustDomain& trustDomain,
          const CERTCertificate& cert,
          CERTCertificate& issuerCert,
          PRTime time)
    : trustDomain(trustDomain)
    , cert(cert)
    , issuerCert(issuerCert)
    , time(time)
    , certStatus(CertStatus::Unknown)
  {
  }

  TrustDomain& trustDomain;
  const CERTCertificate& cert;
  CERTCertificate& issuerCert;
  const PRTime time;
  CertStatus certStatus;

private:
  Context(const Context&); // delete
  void operator=(const Context&); // delete
};

// Verify that potentialSigner is a valid delegated OCSP response signing cert
// according to RFC 6960 section 4.2.2.2.
static Result
CheckOCSPResponseSignerCert(TrustDomain& trustDomain,
                            CERTCertificate& potentialSigner,
                            const CERTCertificate& issuerCert, PRTime time)
{
  Result rv;

  BackCert cert(&potentialSigner, nullptr, BackCert::ExcludeCN);
  rv = cert.Init();
  if (rv != Success) {
    return rv;
  }

  // We don't need to do a complete verification of the signer (i.e. we don't
  // have to call BuildCertChain to verify the entire chain) because we
  // already know that the issuerCert is valid, since revocation checking is
  // done from the root to the parent after we've built a complete chain that
  // we know is otherwise valid. Rather, we just need to do a one-step
  // validation from potentialSigner to issuerCert.
  //
  // It seems reasonable to require the KU_DIGITAL_SIGNATURE key usage on the
  // OCSP responder certificate if the OCSP responder certificate has a
  // key usage extension. However, according to bug 240456, some OCSP responder
  // certificates may have only the nonRepudiation bit set. Also, the OCSP
  // specification (RFC 6960) does not mandate any particular key usage to be
  // asserted for OCSP responde signers. Oddly, the CABForum Baseline
  // Requirements v.1.1.5 do say "If the Root CA Private Key is used for
  // signing OCSP responses, then the digitalSignature bit MUST be set."
  //
  // Note that CheckIssuerIndependentProperties processes
  // SEC_OID_OCSP_RESPONDER in the way that the OCSP specification requires us
  // to--in particular, it doesn't allow SEC_OID_OCSP_RESPONDER to be implied
  // by a missing EKU extension, unlike other EKUs.
  //
  // TODO(bug 926261): If we're validating for a policy then the policy OID we
  // are validating for should be passed to CheckIssuerIndependentProperties.
  rv = CheckIssuerIndependentProperties(trustDomain, cert, time,
                                        MustBeEndEntity, 0,
                                        SEC_OID_OCSP_RESPONDER,
                                        SEC_OID_X509_ANY_POLICY, 0);
  if (rv != Success) {
    return rv;
  }

  // It is possible that there exists a certificate with the same key as the
  // issuer but with a different name, so we need to compare names
  // TODO: needs test
  if (!SECITEM_ItemsAreEqual(&cert.GetNSSCert()->derIssuer,
                             &issuerCert.derSubject) &&
      CERT_CompareName(&cert.GetNSSCert()->issuer,
                       &issuerCert.subject) != SECEqual) {
    return Fail(RecoverableError, SEC_ERROR_OCSP_RESPONDER_CERT_INVALID);
  }

  // TODO(bug 926260): check name constraints

  if (trustDomain.VerifySignedData(&potentialSigner.signatureWrap,
                                   &issuerCert) != SECSuccess) {
    return MapSECStatus(SECFailure);
  }

  // TODO: check for revocation of the OCSP responder certificate unless no-check
  // or the caller forcing no-check. To properly support the no-check policy, we'd
  // need to enforce policy constraints from the issuerChain.

  return Success;
}

//typedef enum {
//    ocspResponderID_byName = 1,
//    ocspResponderID_byKey = 2
//} ResponderIDType;

ENUM_CLASS ResponderIDType : uint8_t
{
  byName = der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 1,
  byKey = der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 2
};

static inline der::Result OCSPResponse(der::Input&, Context&);
static inline der::Result ResponseBytes(der::Input&, Context&);
static inline der::Result BasicResponse(der::Input&, Context&);
static inline der::Result ResponseData(
                              der::Input& tbsResponseData, Context& context,
                              const CERTSignedData& signedResponseData,
                              /*const*/ SECItem* certs, size_t numCerts);
static inline der::Result SingleResponse(der::Input& input,
                                          Context& context);
static inline der::Result CheckExtensionsForCriticality(der::Input&);
static inline der::Result CertID(der::Input& input,
                                  const Context& context,
                                  /*out*/ bool& match);
static der::Result MatchIssuerKey(const SECItem& issuerKeyHash,
                                   const CERTCertificate& issuer,
                                   /*out*/ bool& match);

// RFC 6960 section 4.2.2.2: The OCSP responder must either be the issuer of
// the cert or it must be a delegated OCSP response signing cert directly
// issued by the issuer. If the OCSP responder is a delegated OCSP response
// signer, then its certificate is (probably) embedded within the OCSP
// response and we'll need to verify that it is a valid certificate that chains
// *directly* to issuerCert.
static CERTCertificate*
GetOCSPSignerCertificate(TrustDomain& trustDomain,
                         ResponderIDType responderIDType,
                         const SECItem& responderIDItem,
                         const SECItem* certs, size_t numCerts,
                         CERTCertificate& issuerCert, PRTime time)
{
  bool isIssuer = true;
  size_t i = 0;
  for (;;) {
    ScopedCERTCertificate potentialSigner;
    if (isIssuer) {
      potentialSigner = CERT_DupCertificate(&issuerCert);
    } else if (i < numCerts) {
      potentialSigner = CERT_NewTempCertificate(
                          CERT_GetDefaultCertDB(),
                          /*TODO*/const_cast<SECItem*>(&certs[i]), nullptr,
                          false, false);
      if (!potentialSigner) {
        return nullptr;
      }
      ++i;
    } else {
      PR_SetError(SEC_ERROR_OCSP_INVALID_SIGNING_CERT, 0);
      return nullptr;
    }

    bool match;
    switch (responderIDType) {
      case ResponderIDType::byName:
        // The CA is very likely to have encoded the name in the OCSP response
        // exactly the same as the name is encoded in the signing certificate.
        // Consequently, most of the time we will avoid parsing the name
        // completely. We're assuming here that the signer's subject name is
        // correctly formatted.
        // TODO: need test for exact name
        // TODO: need test for non-exact name match
        match = SECITEM_ItemsAreEqual(&responderIDItem,
                                      &potentialSigner->derSubject);
        if (!match) {
          ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
          if (!arena) {
            return nullptr;
          }
          CERTName name;
          if (SEC_QuickDERDecodeItem(arena.get(), &name,
                                     SEC_ASN1_GET(CERT_NameTemplate),
                                     &responderIDItem) != SECSuccess) {
            return nullptr;
          }
          match = CERT_CompareName(&name, &potentialSigner->subject) == SECEqual;
        }
        break;

      case ResponderIDType::byKey:
      {
        der::Input responderID;
        if (responderID.Init(responderIDItem.data, responderIDItem.len)
              != der::Success) {
          return nullptr;
        }
        SECItem issuerKeyHash;
        if (der::Skip(responderID, der::OCTET_STRING, issuerKeyHash) != der::Success) {
          return nullptr;
        }
        if (MatchIssuerKey(issuerKeyHash, *potentialSigner.get(), match)
              != der::Success) {
          return nullptr;
        }
        break;
      }

      default:
        PR_SetError(SEC_ERROR_OCSP_MALFORMED_RESPONSE, 0);
        return nullptr;
    }

    if (match && !isIssuer) {
      Result rv = CheckOCSPResponseSignerCert(trustDomain,
                                              *potentialSigner.get(),
                                              issuerCert, time);
      if (rv == RecoverableError) {
        match = false;
      } else if (rv != Success) {
        return nullptr;
      }
    }

    if (match) {
      return potentialSigner.release();
    }

    isIssuer = false;
  }
}

static SECStatus
VerifySignature(Context& context, ResponderIDType responderIDType,
                const SECItem& responderID, const SECItem* certs,
                size_t numCerts, const CERTSignedData& signedResponseData)
{
  ScopedCERTCertificate signer(
    GetOCSPSignerCertificate(context.trustDomain, responderIDType, responderID,
                             certs, numCerts, context.issuerCert,
                             context.time));
  if (!signer) {
    return SECFailure;
  }

  if (context.trustDomain.VerifySignedData(&signedResponseData, signer.get())
        != SECSuccess) {
    if (PR_GetError() == SEC_ERROR_BAD_SIGNATURE) {
      PR_SetError(SEC_ERROR_OCSP_BAD_SIGNATURE, 0);
    }
    return SECFailure;
  }

  return SECSuccess;
}

SECStatus
VerifyEncodedOCSPResponse(TrustDomain& trustDomain,
                          const CERTCertificate* cert,
                          CERTCertificate* issuerCert, PRTime time,
                          const SECItem* encodedResponse)
{
  PR_ASSERT(cert);
  PR_ASSERT(issuerCert);
  // TODO: PR_Assert(pinArg)
  PR_ASSERT(encodedResponse);
  if (!cert || !issuerCert || !encodedResponse || !encodedResponse->data) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }

  der::Input input;
  if (input.Init(encodedResponse->data, encodedResponse->len) != der::Success) {
    return SECFailure;
  }

  Context context(trustDomain, *cert, *issuerCert, time);

  if (der::Nested(input, der::SEQUENCE,
                  bind(OCSPResponse, _1, ref(context))) != der::Success) {
    return SECFailure;
  }

  if (der::End(input) != der::Success) {
    return SECFailure;
  }

  switch (context.certStatus) {
    case CertStatus::Good:
      return SECSuccess;
    case CertStatus::Revoked:
      PR_SetError(SEC_ERROR_REVOKED_CERTIFICATE, 0);
      return SECFailure;
    case CertStatus::Unknown:
      PR_SetError(SEC_ERROR_OCSP_UNKNOWN_CERT, 0);
      return SECFailure;
  }

  PR_NOT_REACHED("unknown CertStatus");
  PR_SetError(SEC_ERROR_OCSP_UNKNOWN_CERT, 0);
  return SECFailure;
}

// OCSPResponse ::= SEQUENCE {
//       responseStatus         OCSPResponseStatus,
//       responseBytes          [0] EXPLICIT ResponseBytes OPTIONAL }
//
static inline der::Result
OCSPResponse(der::Input& input, Context& context)
{
  // OCSPResponseStatus ::= ENUMERATED {
  //     successful            (0),  -- Response has valid confirmations
  //     malformedRequest      (1),  -- Illegal confirmation request
  //     internalError         (2),  -- Internal error in issuer
  //     tryLater              (3),  -- Try again later
  //                                 -- (4) is not used
  //     sigRequired           (5),  -- Must sign the request
  //     unauthorized          (6)   -- Request unauthorized
  // }
  uint8_t responseStatus;

  if (der::Enumerated(input, responseStatus) != der::Success) {
    return der::Failure;
  }
  switch (responseStatus) {
    case 0: break; // successful
    case 1: return der::Fail(SEC_ERROR_OCSP_MALFORMED_REQUEST);
    case 2: return der::Fail(SEC_ERROR_OCSP_SERVER_ERROR);
    case 3: return der::Fail(SEC_ERROR_OCSP_TRY_SERVER_LATER);
    case 5: return der::Fail(SEC_ERROR_OCSP_REQUEST_NEEDS_SIG);
    case 6: return der::Fail(SEC_ERROR_OCSP_UNAUTHORIZED_REQUEST);
    default: return der::Fail(SEC_ERROR_OCSP_UNKNOWN_RESPONSE_STATUS);
  }

  return der::Nested(input, der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 0,
                     der::SEQUENCE, bind(ResponseBytes, _1, ref(context)));
}

// ResponseBytes ::=       SEQUENCE {
//     responseType   OBJECT IDENTIFIER,
//     response       OCTET STRING }
static inline der::Result
ResponseBytes(der::Input& input, Context& context)
{
  static const uint8_t id_pkix_ocsp_basic[] = {
    0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x01
  };

  if (der::OID(input, id_pkix_ocsp_basic) != der::Success) {
    return der::Failure;
  }

  return der::Nested(input, der::OCTET_STRING, der::SEQUENCE,
                     bind(BasicResponse, _1, ref(context)));
}

// BasicOCSPResponse       ::= SEQUENCE {
//    tbsResponseData      ResponseData,
//    signatureAlgorithm   AlgorithmIdentifier,
//    signature            BIT STRING,
//    certs            [0] EXPLICIT SEQUENCE OF Certificate OPTIONAL }
der::Result
BasicResponse(der::Input& input, Context& context)
{
  der::Input::Mark mark(input.GetMark());

  uint16_t length;
  if (der::ExpectTagAndGetLength(input, der::SEQUENCE, length)
        != der::Success) {
    return der::Failure;
  }

  // The signature covers the entire DER encoding of tbsResponseData, including
  // the beginning tag and length. However, when we're parsing tbsResponseData,
  // we want to strip off the tag and length because we don't need it after
  // we've confirmed it's there and figured out what length it is.

  der::Input tbsResponseData;

  if (input.Skip(length, tbsResponseData) != der::Success) {
    return der::Failure;
  }

  CERTSignedData signedData;

  input.GetSECItem(siBuffer, mark, signedData.data);

  if (der::Nested(input, der::SEQUENCE,
                  bind(der::AlgorithmIdentifier, _1,
                       ref(signedData.signatureAlgorithm))) != der::Success) {
    return der::Failure;
  }

  if (der::Skip(input, der::BIT_STRING, signedData.signature) != der::Success) {
    return der::Failure;
  }
  if (signedData.signature.len == 0) {
    return der::Fail(SEC_ERROR_OCSP_BAD_SIGNATURE);
  }
  unsigned int unusedBitsAtEnd = signedData.signature.data[0];
  // XXX: Really the constraint should be that unusedBitsAtEnd must be less
  // than 7. But, we suspect there are no valid OCSP response signatures with
  // non-zero unused bits. It seems like NSS assumes this in various places, so
  // we enforce it. If we find compatibility issues, we'll know we're wrong.
  if (unusedBitsAtEnd != 0) {
    return der::Fail(SEC_ERROR_OCSP_BAD_SIGNATURE);
  }
  ++signedData.signature.data;
  --signedData.signature.len;
  signedData.signature.len = (signedData.signature.len << 3); // Bytes to bits

  // Parse certificates, if any

  SECItem certs[8];
  size_t numCerts = 0;

  if (!input.AtEnd()) {
    // We ignore the lengths of the wrappers because we'll detect bad lengths
    // during parsing--too short and we'll run out of input for parsing a cert,
    // and too long and we'll have leftover data that won't parse as a cert.

    // [0] wrapper
    if (der::ExpectTagAndIgnoreLength(
          input, der::CONSTRUCTED | der::CONTEXT_SPECIFIC | 0)
        != der::Success) {
      return der::Failure;
    }

    // SEQUENCE wrapper
    if (der::ExpectTagAndIgnoreLength(input, der::SEQUENCE) != der::Success) {
      return der::Failure;
    }

    // sequence of certificates
    while (!input.AtEnd()) {
      if (numCerts == PR_ARRAY_SIZE(certs)) {
        return der::Fail(SEC_ERROR_BAD_DER);
      }

      // Unwrap the SEQUENCE that contains the certificate, which is itself a
      // SEQUENCE.
      der::Input::Mark mark(input.GetMark());
      if (der::Skip(input, der::SEQUENCE) != der::Success) {
        return der::Failure;
      }

      input.GetSECItem(siBuffer, mark, certs[numCerts]);
      ++numCerts;
    }
  }

  return ResponseData(tbsResponseData, context, signedData, certs, numCerts);
}

// ResponseData ::= SEQUENCE {
//    version             [0] EXPLICIT Version DEFAULT v1,
//    responderID             ResponderID,
//    producedAt              GeneralizedTime,
//    responses               SEQUENCE OF SingleResponse,
//    responseExtensions  [1] EXPLICIT Extensions OPTIONAL }
static inline der::Result
ResponseData(der::Input& input, Context& context,
             const CERTSignedData& signedResponseData,
             /*const*/ SECItem* certs, size_t numCerts)
{
  uint8_t version;
  if (der::OptionalVersion(input, version) != der::Success) {
    return der::Failure;
  }
  if (version != der::v1) {
    // TODO: more specific error code for bad version?
    return der::Fail(SEC_ERROR_BAD_DER);
  }

  // ResponderID ::= CHOICE {
  //    byName              [1] Name,
  //    byKey               [2] KeyHash }
  SECItem responderID;
  uint16_t responderIDLength;
  ResponderIDType responderIDType
    = input.Peek(static_cast<uint8_t>(ResponderIDType::byName))
    ? ResponderIDType::byName
    : ResponderIDType::byKey;
  if (ExpectTagAndGetLength(input, static_cast<uint8_t>(responderIDType),
                            responderIDLength) != der::Success) {
    return der::Failure;
  }
  // TODO: responderID probably needs to have another level of ASN1 tag/length
  // checked and stripped.
  if (input.Skip(responderIDLength, responderID) != der::Success) {
    return der::Failure;
  }

  // This is the soonest we can verify the signature. We verify the signature
  // right away to follow the principal of minimizing the processing of data
  // before verifying its signature.
  if (VerifySignature(context, responderIDType, responderID, certs, numCerts,
                      signedResponseData) != SECSuccess) {
    return der::Failure;
  }

  // TODO: Do we even need to parse this? Should we just skip it?
  PRTime producedAt;
  if (der::GeneralizedTime(input, producedAt) != der::Success) {
    return der::Failure;
  }

  // We don't accept an empty sequence of responses. In practice, a legit OCSP
  // responder will never return an empty response, and handling the case of an
  // empty response makes things unnecessarily complicated.
  if (der::NestedOf(input, der::SEQUENCE, der::SEQUENCE,
                    der::MustNotBeEmpty,
                    bind(SingleResponse, _1, ref(context))) != der::Success) {
    return der::Failure;
  }

  if (!input.AtEnd()) {
    if (CheckExtensionsForCriticality(input) != der::Success) {
      return der::Failure;
    }
  }

  return der::Success;
}

// SingleResponse ::= SEQUENCE {
//    certID                       CertID,
//    certStatus                   CertStatus,
//    thisUpdate                   GeneralizedTime,
//    nextUpdate           [0]     EXPLICIT GeneralizedTime OPTIONAL,
//    singleExtensions     [1]     EXPLICIT Extensions{{re-ocsp-crl |
//                                              re-ocsp-archive-cutoff |
//                                              CrlEntryExtensions, ...}
//                                              } OPTIONAL }
static inline der::Result
SingleResponse(der::Input& input, Context& context)
{
  bool match = false;
  if (der::Nested(input, der::SEQUENCE,
                  bind(CertID, _1, cref(context), ref(match)))
        != der::Success) {
    return der::Failure;
  }

  if (!match) {
    return der::Success;
  }

  // CertStatus ::= CHOICE {
  //     good        [0]     IMPLICIT NULL,
  //     revoked     [1]     IMPLICIT RevokedInfo,
  //     unknown     [2]     IMPLICIT UnknownInfo }
  //
  // In the event of multiple SingleResponses for a cert that have conflicting
  // statuses, we use the following precedence rules:
  //
  // * revoked overrides good and unknown
  // * good overrides unknown
  if (input.Peek(static_cast<uint8_t>(CertStatus::Good))) {
    if (ExpectTagAndLength(input, static_cast<uint8_t>(CertStatus::Good), 0)
          != der::Success) {
      return der::Failure;
    }
    if (context.certStatus != CertStatus::Revoked) {
      context.certStatus = CertStatus::Good;
    }
  } else if (input.Peek(static_cast<uint8_t>(CertStatus::Revoked))) {
    // We don't need any info from the RevokedInfo structure, so we don't even
    // parse it. TODO: We should mention issues like this in the explanation of
    // why we treat invalid OCSP responses equivalently to revoked for OCSP
    // stapling.
    if (der::Skip(input, static_cast<uint8_t>(CertStatus::Revoked))
          != der::Success) {
      return der::Failure;
    }
    context.certStatus = CertStatus::Revoked;
  } else if (ExpectTagAndLength(input,
                                static_cast<uint8_t>(CertStatus::Unknown),
                                0) != der::Success) {
    return der::Failure;
  }

  // http://tools.ietf.org/html/rfc6960#section-3.2
  // 5. The time at which the status being indicated is known to be
  //    correct (thisUpdate) is sufficiently recent;
  // 6. When available, the time at or before which newer information will
  //    be available about the status of the certificate (nextUpdate) is
  //    greater than the current time.

  // We won't accept any OCSP responses that are more than 10 days old, even if
  // the nextUpdate time is further in the future.
  static const PRTime OLDEST_ACCEPTABLE = INT64_C(10) * ONE_DAY;

  PRTime thisUpdate;
  if (der::GeneralizedTime(input, thisUpdate) != der::Success) {
    return der::Failure;
  }

  if (thisUpdate > context.time + SLOP) {
    return der::Fail(SEC_ERROR_OCSP_FUTURE_RESPONSE);
  }

  PRTime notAfter;
  static const uint8_t NEXT_UPDATE_TAG =
    der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 0;
  if (input.Peek(NEXT_UPDATE_TAG)) {
    PRTime nextUpdate;
    if (der::Nested(input, NEXT_UPDATE_TAG,
                    bind(der::GeneralizedTime, _1, ref(nextUpdate)))
          != der::Success) {
      return der::Failure;
    }

    if (nextUpdate < thisUpdate) {
      return der::Fail(SEC_ERROR_OCSP_MALFORMED_RESPONSE);
    }
    if (nextUpdate - thisUpdate <= OLDEST_ACCEPTABLE) {
      notAfter = nextUpdate;
    } else {
      notAfter = thisUpdate + OLDEST_ACCEPTABLE;
    }
  } else {
    // NSS requires all OCSP responses without a nextUpdate to be recent.
    // Match that stricter behavior.
    notAfter = thisUpdate + ONE_DAY;
  }

  if (context.time < SLOP) { // prevent underflow
    return der::Fail(SEC_ERROR_INVALID_ARGS);
  }
  if (context.time - SLOP > notAfter) {
    return der::Fail(SEC_ERROR_OCSP_OLD_RESPONSE);
  }


  if (!input.AtEnd()) {
    if (CheckExtensionsForCriticality(input) != der::Success) {
      return der::Failure;
    }
  }

  return der::Success;
}

// CertID          ::=     SEQUENCE {
//        hashAlgorithm       AlgorithmIdentifier,
//        issuerNameHash      OCTET STRING, -- Hash of issuer's DN
//        issuerKeyHash       OCTET STRING, -- Hash of issuer's public key
//        serialNumber        CertificateSerialNumber }
static inline der::Result
CertID(der::Input& input, const Context& context, /*out*/ bool& match)
{
  match = false;

  SECAlgorithmID hashAlgorithm;
  if (der::Nested(input, der::SEQUENCE,
                  bind(der::AlgorithmIdentifier, _1, ref(hashAlgorithm)))
         != der::Success) {
    return der::Failure;
  }

  SECItem issuerNameHash;
  if (der::Skip(input, der::OCTET_STRING, issuerNameHash) != der::Success) {
    return der::Failure;
  }

  SECItem issuerKeyHash;
  if (der::Skip(input, der::OCTET_STRING, issuerKeyHash) != der::Success) {
    return der::Failure;
  }

  SECItem serialNumber;
  if (der::CertificateSerialNumber(input, serialNumber) != der::Success) {
    return der::Failure;
  }

  const CERTCertificate& cert = context.cert;
  const CERTCertificate& issuerCert = context.issuerCert;

  if (!SECITEM_ItemsAreEqual(&serialNumber, &cert.serialNumber)) {
    return der::Success;
  }

  // TODO: support SHA-2 hashes.

  SECOidTag hashAlg = SECOID_GetAlgorithmTag(&hashAlgorithm);
  if (hashAlg != SEC_OID_SHA1) {
    return der::Success;
  }

  if (issuerNameHash.len != SHA1_LENGTH) {
    return der::Fail(SEC_ERROR_OCSP_MALFORMED_RESPONSE);
  }

  // From http://tools.ietf.org/html/rfc6960#section-4.1.1:
  // "The hash shall be calculated over the DER encoding of the
  // issuer's name field in the certificate being checked."
  uint8_t hashBuf[SHA1_LENGTH];
  if (PK11_HashBuf(SEC_OID_SHA1, hashBuf, cert.derIssuer.data,
                   cert.derIssuer.len) != SECSuccess) {
    return der::Failure;
  }
  if (memcmp(hashBuf, issuerNameHash.data, issuerNameHash.len)) {
    return der::Success;
  }

  return MatchIssuerKey(issuerKeyHash, issuerCert, match);
}

// From http://tools.ietf.org/html/rfc6960#section-4.1.1:
// "The hash shall be calculated over the value (excluding tag and length) of
// the subject public key field in the issuer's certificate."
static der::Result
MatchIssuerKey(const SECItem& issuerKeyHash, const CERTCertificate& issuer,
               /*out*/ bool& match)
{
  if (issuerKeyHash.len != SHA1_LENGTH)  {
    return der::Fail(SEC_ERROR_OCSP_MALFORMED_RESPONSE);
  }

  // TODO(bug 966856): support SHA-2 hashes

  // Copy just the length and data pointer (nothing needs to be freed) of the
  // subject public key so we can convert the length from bits to bytes, which
  // is what the digest function expects.
  SECItem spk = issuer.subjectPublicKeyInfo.subjectPublicKey;
  DER_ConvertBitString(&spk);

  static uint8_t hashBuf[SHA1_LENGTH];
  if (PK11_HashBuf(SEC_OID_SHA1, hashBuf, spk.data, spk.len) != SECSuccess) {
    return der::Failure;
  }

  match = !memcmp(hashBuf, issuerKeyHash.data, issuerKeyHash.len);
  return der::Success;
}

// Extension  ::=  SEQUENCE  {
//      extnID      OBJECT IDENTIFIER,
//      critical    BOOLEAN DEFAULT FALSE,
//      extnValue   OCTET STRING
//      }
static der::Result
CheckExtensionForCriticality(der::Input& input)
{
  uint16_t toSkip;
  if (ExpectTagAndGetLength(input, der::OIDTag, toSkip) != der::Success) {
    return der::Failure;
  }

  // TODO: maybe we should check the syntax of the OID value
  if (input.Skip(toSkip) != der::Success) {
    return der::Failure;
  }

  // The only valid explicit encoding of the value is TRUE, so don't even
  // bother parsing it, since we're going to fail either way.
  if (input.Peek(der::BOOLEAN)) {
    return der::Fail(SEC_ERROR_UNKNOWN_CRITICAL_EXTENSION);
  }

  if (ExpectTagAndGetLength(input, der::OCTET_STRING, toSkip)
        != der::Success) {
    return der::Failure;
  }
  return input.Skip(toSkip);
}

static der::Result
CheckExtensionsForCriticality(der::Input& input)
{
  return der::NestedOf(input, der::SEQUENCE | 1, der::SEQUENCE,
                       der::MustNotBeEmpty, CheckExtensionForCriticality);
}

//   1. The certificate identified in a received response corresponds to
//      the certificate that was identified in the corresponding request;
//   2. The signature on the response is valid;
//   3. The identity of the signer matches the intended recipient of the
//      request;
//   4. The signer is currently authorized to provide a response for the
//      certificate in question;
//   5. The time at which the status being indicated is known to be
//      correct (thisUpdate) is sufficiently recent;
//   6. When available, the time at or before which newer information will
//      be available about the status of the certificate (nextUpdate) is
//      greater than the current time.
//
//   Responses whose nextUpdate value is earlier than
//   the local system time value SHOULD be considered unreliable.
//   Responses whose thisUpdate time is later than the local system time
//   SHOULD be considered unreliable.
//
//   If nextUpdate is not set, the responder is indicating that newer
//   revocation information is available all the time.
//
// http://tools.ietf.org/html/rfc5019#section-4

SECItem*
CreateEncodedOCSPRequest(PLArenaPool* arena,
                         const CERTCertificate* cert,
                         const CERTCertificate* issuerCert)
{
  if (!arena || !cert || !issuerCert) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return nullptr;
  }

  // We do not add any extensions to the request.

  // RFC 6960 says "An OCSP client MAY wish to specify the kinds of response
  // types it understands. To do so, it SHOULD use an extension with the OID
  // id-pkix-ocsp-response." This use of MAY and SHOULD is unclear. MSIE11
  // on Windows 8.1 does not include any extensions, whereas NSS has always
  // included the id-pkix-ocsp-response extension. Avoiding the sending the
  // extension is better for OCSP GET because it makes the request smaller,
  // and thus more likely to fit within the 255 byte limit for OCSP GET that
  // is specified in RFC 5019 Section 5.

  // Bug 966856: Add the id-pkix-ocsp-pref-sig-algs extension.

  // Since we don't know whether the OCSP responder supports anything other
  // than SHA-1, we have no choice but to use SHA-1 for issuerNameHash and
  // issuerKeyHash.
  static const uint8_t hashAlgorithm[11] = {
    0x30, 0x09,                               // SEQUENCE
    0x06, 0x05, 0x2B, 0x0E, 0x03, 0x02, 0x1A, //   OBJECT IDENTIFIER id-sha1
    0x05, 0x00,                               //   NULL
  };
  static const uint8_t hashLen = SHA1_LENGTH;

  static const unsigned int totalLenWithoutSerialNumberData
    = 2                             // OCSPRequest
    + 2                             //   tbsRequest
    + 2                             //     requestList
    + 2                             //       Request
    + 2                             //         reqCert (CertID)
    + PR_ARRAY_SIZE(hashAlgorithm)  //           hashAlgorithm
    + 2 + hashLen                   //           issuerNameHash
    + 2 + hashLen                   //           issuerKeyHash
    + 2;                            //           serialNumber (header)

  // The only way we could have a request this large is if the serialNumber was
  // ridiculously and unreasonably large. RFC 5280 says "Conforming CAs MUST
  // NOT use serialNumber values longer than 20 octets." With this restriction,
  // we allow for some amount of non-conformance with that requirement while
  // still ensuring we can encode the length values in the ASN.1 TLV structures
  // in a single byte.
  if (issuerCert->serialNumber.len > 127u - totalLenWithoutSerialNumberData) {
    PR_SetError(SEC_ERROR_BAD_DATA, 0);
    return nullptr;
  }

  uint8_t totalLen = static_cast<uint8_t>(totalLenWithoutSerialNumberData +
    cert->serialNumber.len);

  SECItem* encodedRequest = SECITEM_AllocItem(arena, nullptr, totalLen);
  if (!encodedRequest) {
    return nullptr;
  }

  uint8_t* d = encodedRequest->data;
  *d++ = 0x30; *d++ = totalLen - 2;  // OCSPRequest (SEQUENCE)
  *d++ = 0x30; *d++ = totalLen - 4;  //   tbsRequest (SEQUENCE)
  *d++ = 0x30; *d++ = totalLen - 6;  //     requestList (SEQUENCE OF)
  *d++ = 0x30; *d++ = totalLen - 8;  //       Request (SEQUENCE)
  *d++ = 0x30; *d++ = totalLen - 10; //         reqCert (CertID SEQUENCE)

  // reqCert.hashAlgorithm
  for (size_t i = 0; i < PR_ARRAY_SIZE(hashAlgorithm); ++i) {
    *d++ = hashAlgorithm[i];
  }

  // reqCert.issuerNameHash (OCTET STRING)
  *d++ = 0x04;
  *d++ = hashLen;
  if (PK11_HashBuf(SEC_OID_SHA1, d, issuerCert->derSubject.data,
                   issuerCert->derSubject.len) != SECSuccess) {
    return nullptr;
  }
  d += hashLen;

  // reqCert.issuerKeyHash (OCTET STRING)
  *d++ = 0x04;
  *d++ = hashLen;
  SECItem key = issuerCert->subjectPublicKeyInfo.subjectPublicKey;
  DER_ConvertBitString(&key);
  if (PK11_HashBuf(SEC_OID_SHA1, d, key.data, key.len) != SECSuccess) {
    return nullptr;
  }
  d += hashLen;

  // reqCert.serialNumber (INTEGER)
  *d++ = 0x02; // INTEGER
  *d++ = static_cast<uint8_t>(cert->serialNumber.len);
  for (size_t i = 0; i < cert->serialNumber.len; ++i) {
    *d++ = cert->serialNumber.data[i];
  }

  PR_ASSERT(d == encodedRequest->data + totalLen);

  return encodedRequest;
}

} } // namespace insanity::pkix
