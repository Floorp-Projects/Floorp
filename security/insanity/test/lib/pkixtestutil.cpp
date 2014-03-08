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

#include "pkixcheck.h"
#include "pkixder.h"
#include "pkixtestutil.h"

#include "cryptohi.h"
#include "hasht.h"
#include "pk11pub.h"
#include "prinit.h"
#include "secder.h"

namespace insanity { namespace test {

class Output
{
public:
  Output()
    : numItems(0)
    , length(0)
  {
  }

  // Makes a shallow copy of the input item. All input items must have a
  // lifetime that extends at least to where Squash is called.
  der::Result Add(const SECItem* item)
  {
    PR_ASSERT(item);
    PR_ASSERT(item->data);

    if (numItems >= MaxSequenceItems) {
      return der::Fail(SEC_ERROR_INVALID_ARGS);
    }
    if (length + item->len > 65535) {
      return der::Fail(SEC_ERROR_INVALID_ARGS);
    }

    contents[numItems] = item;
    numItems++;
    length += item->len;
    return der::Success;
  }

  SECItem* Squash(PLArenaPool* arena, uint8_t tag)
  {
    PR_ASSERT(arena);

    size_t lengthLength = length < 128 ? 1
                        : length < 256 ? 2
                                       : 3;
    size_t totalLength = 1 + lengthLength + length;
    SECItem* output = SECITEM_AllocItem(arena, nullptr, totalLength);
    if (!output) {
      return nullptr;
    }
    uint8_t* d = output->data;
    *d++ = tag;
    EncodeLength(d, length, lengthLength);
    d += lengthLength;
    for (size_t i = 0; i < numItems; i++) {
      memcpy(d, contents[i]->data, contents[i]->len);
      d += contents[i]->len;
    }
    return output;
  }

private:
  void
  EncodeLength(uint8_t* data, size_t length, size_t lengthLength)
  {
    switch (lengthLength) {
      case 1:
        data[0] = length;
        break;
      case 2:
        data[0] = 0x81;
        data[1] = length;
        break;
      case 3:
        data[0] = 0x82;
        data[1] = length / 256;
        data[2] = length % 256;
        break;
      default:
        PR_NOT_REACHED("EncodeLength: bad lengthLength");
        PR_Abort();
    }
  }

  static const size_t MaxSequenceItems = 5;
  const SECItem* contents[MaxSequenceItems];
  size_t numItems;
  size_t length;

  Output(const Output&) /* = delete */;
  void operator=(const Output&) /* = delete */;
};

static SECItem* ResponseBytes(OCSPResponseContext& context);
static SECItem* BasicOCSPResponse(OCSPResponseContext& context);
static SECItem* ResponseData(OCSPResponseContext& context);
static SECItem* ResponderID(OCSPResponseContext& context);
static SECItem* KeyHash(OCSPResponseContext& context);
static SECItem* SingleResponse(OCSPResponseContext& context);
static SECItem* CertID(OCSPResponseContext& context);
static SECItem* CertStatus(OCSPResponseContext& context);

static SECItem*
EncodeNested(PLArenaPool* arena, uint8_t tag, SECItem* inner)
{
  Output output;
  if (output.Add(inner) != der::Success) {
    return nullptr;
  }
  return output.Squash(arena, tag);
}

// A return value of 0 is an error, but this should never happen in practice
// because this function aborts in that case.
static size_t
HashAlgorithmToLength(SECOidTag hashAlg)
{
  switch (hashAlg) {
    case SEC_OID_SHA1:
      return SHA1_LENGTH;
    case SEC_OID_SHA256:
      return SHA256_LENGTH;
    case SEC_OID_SHA384:
      return SHA384_LENGTH;
    case SEC_OID_SHA512:
      return SHA512_LENGTH;
    default:
      PR_NOT_REACHED("HashAlgorithmToLength: bad hashAlg");
      PR_Abort();
  }
  return 0;
}

static SECItem*
HashedOctetString(PLArenaPool* arena, const SECItem* bytes, SECOidTag hashAlg)
{
  size_t hashLen = HashAlgorithmToLength(hashAlg);
  if (hashLen == 0) {
    return nullptr;
  }
  SECItem* hashBuf = SECITEM_AllocItem(arena, nullptr, hashLen);
  if (!hashBuf) {
    return nullptr;
  }
  if (PK11_HashBuf(hashAlg, hashBuf->data, bytes->data, bytes->len)
        != SECSuccess) {
    return nullptr;
  }

  return EncodeNested(arena, der::OCTET_STRING, hashBuf);
}

static SECItem*
KeyHashHelper(PLArenaPool* arena, const CERTCertificate* cert)
{
  // We only need a shallow copy here.
  SECItem spk = cert->subjectPublicKeyInfo.subjectPublicKey;
  DER_ConvertBitString(&spk); // bits to bytes
  return HashedOctetString(arena, &spk, SEC_OID_SHA1);
}

static SECItem*
AlgorithmIdentifier(PLArenaPool* arena, SECOidTag algTag)
{
  SECAlgorithmIDStr aid;
  aid.algorithm.data = nullptr;
  aid.algorithm.len = 0;
  aid.parameters.data = nullptr;
  aid.parameters.len = 0;
  if (SECOID_SetAlgorithmID(arena, &aid, algTag, nullptr) != SECSuccess) {
    return nullptr;
  }
  static const SEC_ASN1Template algorithmIDTemplate[] = {
    { SEC_ASN1_SEQUENCE, 0, NULL, sizeof(SECAlgorithmID) },
    { SEC_ASN1_OBJECT_ID, offsetof(SECAlgorithmID, algorithm) },
    { SEC_ASN1_OPTIONAL | SEC_ASN1_ANY, offsetof(SECAlgorithmID, parameters) },
    { 0 }
  };
  SECItem* algorithmID = SEC_ASN1EncodeItem(arena, nullptr, &aid,
                                            algorithmIDTemplate);
  return algorithmID;
}

static SECItem*
PRTimeToEncodedTime(PLArenaPool* arena, PRTime time)
{
  SECItem derTime;
  if (DER_TimeToGeneralizedTimeArena(arena, &derTime, time) != SECSuccess) {
    return nullptr;
  }
  return EncodeNested(arena, der::GENERALIZED_TIME, &derTime);
}

SECItem*
CreateEncodedOCSPResponse(OCSPResponseContext& context)
{
  if (!context.arena || !context.cert || !context.issuerCert ||
      !context.signerCert) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return nullptr;
  }

  // OCSPResponse ::= SEQUENCE {
  //    responseStatus          OCSPResponseStatus,
  //    responseBytes       [0] EXPLICIT ResponseBytes OPTIONAL }

  // OCSPResponseStatus ::= ENUMERATED {
  //    successful          (0),  -- Response has valid confirmations
  //    malformedRequest    (1),  -- Illegal confirmation request
  //    internalError       (2),  -- Internal error in issuer
  //    tryLater            (3),  -- Try again later
  //                              -- (4) is not used
  //    sigRequired         (5),  -- Must sign the request
  //    unauthorized        (6)   -- Request unauthorized
  // }
  SECItem* responseStatus = SECITEM_AllocItem(context.arena, nullptr, 3);
  if (!responseStatus) {
    return nullptr;
  }
  responseStatus->data[0] = der::ENUMERATED;
  responseStatus->data[1] = 1;
  responseStatus->data[2] = context.responseStatus;

  SECItem* responseBytes = ResponseBytes(context);
  if (!responseBytes) {
    return nullptr;
  }

  SECItem* responseBytesNested = EncodeNested(context.arena,
                                              der::CONSTRUCTED |
                                              der::CONTEXT_SPECIFIC |
                                              0,
                                              responseBytes);
  if (!responseBytesNested) {
    return nullptr;
  }

  Output output;
  if (output.Add(responseStatus) != der::Success) {
    return nullptr;
  }
  if (output.Add(responseBytesNested) != der::Success) {
    return nullptr;
  }
  return output.Squash(context.arena, der::SEQUENCE);
}

// ResponseBytes ::= SEQUENCE {
//    responseType            OBJECT IDENTIFIER,
//    response                OCTET STRING }
SECItem*
ResponseBytes(OCSPResponseContext& context)
{
  // Includes tag and length
  static const uint8_t id_pkix_ocsp_basic_encoded[] = {
    0x06, 0x09, 0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x01
  };
  SECItem id_pkix_ocsp_basic = {
    siBuffer,
    const_cast<uint8_t*>(id_pkix_ocsp_basic_encoded),
    PR_ARRAY_SIZE(id_pkix_ocsp_basic_encoded)
  };
  SECItem* response = BasicOCSPResponse(context);
  if (!response) {
    return nullptr;
  }
  SECItem* responseNested = EncodeNested(context.arena, der::OCTET_STRING,
                                         response);
  if (!responseNested) {
    return nullptr;
  }

  Output output;
  if (output.Add(&id_pkix_ocsp_basic) != der::Success) {
    return nullptr;
  }
  if (output.Add(responseNested) != der::Success) {
    return nullptr;
  }
  return output.Squash(context.arena, der::SEQUENCE);
}

// BasicOCSPResponse ::= SEQUENCE {
//   tbsResponseData          ResponseData,
//   signatureAlgorithm       AlgorithmIdentifier,
//   signature                BIT STRING,
//   certs                [0] EXPLICIT SEQUENCE OF Certificate OPTIONAL }
SECItem*
BasicOCSPResponse(OCSPResponseContext& context)
{
  SECItem* tbsResponseData = ResponseData(context);
  if (!tbsResponseData) {
    return nullptr;
  }


  pkix::ScopedPtr<SECKEYPrivateKey, SECKEY_DestroyPrivateKey> privKey(
    PK11_FindKeyByAnyCert(context.signerCert.get(), nullptr));
  if (!privKey) {
    return nullptr;
  }
  SECOidTag signatureAlgTag = SEC_GetSignatureAlgorithmOidTag(privKey->keyType,
                                                              SEC_OID_SHA1);
  if (signatureAlgTag == SEC_OID_UNKNOWN) {
    return nullptr;
  }
  SECItem* signatureAlgorithm = AlgorithmIdentifier(context.arena,
                                                    signatureAlgTag);
  if (!signatureAlgorithm) {
    return nullptr;
  }

  // SEC_SignData doesn't take an arena parameter, so we have to manage
  // the memory allocated in signature.
  SECItem signature;
  if (SEC_SignData(&signature, tbsResponseData->data, tbsResponseData->len,
                   privKey.get(), signatureAlgTag) != SECSuccess)
  {
    return nullptr;
  }
  // We have to add a byte at the beginning indicating no unused bits.
  // TODO: add ability to have signatures of bit length not divisible by 8,
  // resulting in unused bits in the bitstring encoding
  SECItem* prefixedSignature = SECITEM_AllocItem(context.arena, nullptr,
                                                 signature.len + 1);
  if (!prefixedSignature) {
    SECITEM_FreeItem(&signature, false);
    return nullptr;
  }
  prefixedSignature->data[0] = 0;
  memcpy(prefixedSignature->data + 1, signature.data, signature.len);
  SECITEM_FreeItem(&signature, false);
  if (context.badSignature) {
    PR_ASSERT(prefixedSignature->len > 8);
    prefixedSignature->data[8]++;
  }
  SECItem* signatureNested = EncodeNested(context.arena, der::BIT_STRING,
                                          prefixedSignature);
  if (!signatureNested) {
    return nullptr;
  }

  // TODO(bug 980538): certificates
  Output output;
  if (output.Add(tbsResponseData) != der::Success) {
    return nullptr;
  }
  if (output.Add(signatureAlgorithm) != der::Success) {
    return nullptr;
  }
  if (output.Add(signatureNested) != der::Success) {
    return nullptr;
  }
  return output.Squash(context.arena, der::SEQUENCE);
}

// ResponseData ::= SEQUENCE {
//    version             [0] EXPLICIT Version DEFAULT v1,
//    responderID             ResponderID,
//    producedAt              GeneralizedTime,
//    responses               SEQUENCE OF SingleResponse,
//    responseExtensions  [1] EXPLICIT Extensions OPTIONAL }
SECItem*
ResponseData(OCSPResponseContext& context)
{
  SECItem* responderID = ResponderID(context);
  if (!responderID) {
    return nullptr;
  }
  SECItem* producedAtEncoded = PRTimeToEncodedTime(context.arena,
                                                   context.producedAt);
  if (!producedAtEncoded) {
    return nullptr;
  }
  SECItem* responses = SingleResponse(context);
  if (!responses) {
    return nullptr;
  }
  SECItem* responsesNested = EncodeNested(context.arena, der::SEQUENCE,
                                          responses);
  if (!responsesNested) {
    return nullptr;
  }

  Output output;
  if (output.Add(responderID) != der::Success) {
    return nullptr;
  }
  if (output.Add(producedAtEncoded) != der::Success) {
    return nullptr;
  }
  if (output.Add(responsesNested) != der::Success) {
    return nullptr;
  }
  return output.Squash(context.arena, der::SEQUENCE);
}

// ResponderID ::= CHOICE {
//    byName              [1] Name,
//    byKey               [2] KeyHash }
// }
SECItem*
ResponderID(OCSPResponseContext& context)
{
  SECItem* contents = nullptr;
  if (context.responderIDType == OCSPResponseContext::ByName) {
    contents = &context.signerCert->derSubject;
  } else if (context.responderIDType == OCSPResponseContext::ByKeyHash) {
    contents = KeyHash(context);
    if (!contents) {
      return nullptr;
    }
  } else {
    return nullptr;
  }

  return EncodeNested(context.arena,
                      der::CONSTRUCTED |
                      der::CONTEXT_SPECIFIC |
                      context.responderIDType,
                      contents);
}

// KeyHash ::= OCTET STRING -- SHA-1 hash of responder's public key
//                          -- (i.e., the SHA-1 hash of the value of the
//                          -- BIT STRING subjectPublicKey [excluding
//                          -- the tag, length, and number of unused
//                          -- bits] in the responder's certificate)
SECItem*
KeyHash(OCSPResponseContext& context)
{
  return KeyHashHelper(context.arena, context.signerCert.get());
}

// SingleResponse ::= SEQUENCE {
//    certID                  CertID,
//    certStatus              CertStatus,
//    thisUpdate              GeneralizedTime,
//    nextUpdate          [0] EXPLICIT GeneralizedTime OPTIONAL,
//    singleExtensions    [1] EXPLICIT Extensions OPTIONAL }
SECItem*
SingleResponse(OCSPResponseContext& context)
{
  SECItem* certID = CertID(context);
  if (!certID) {
    return nullptr;
  }
  SECItem* certStatus = CertStatus(context);
  if (!certStatus) {
    return nullptr;
  }
  SECItem* thisUpdateEncoded = PRTimeToEncodedTime(context.arena,
                                                   context.thisUpdate);
  if (!thisUpdateEncoded) {
    return nullptr;
  }
  SECItem* nextUpdateEncodedNested = nullptr;
  if (context.includeNextUpdate) {
    SECItem* nextUpdateEncoded = PRTimeToEncodedTime(context.arena,
                                                     context.nextUpdate);
    if (!nextUpdateEncoded) {
      return nullptr;
    }
    nextUpdateEncodedNested = EncodeNested(context.arena,
                                           der::CONSTRUCTED |
                                           der::CONTEXT_SPECIFIC |
                                           0,
                                           nextUpdateEncoded);
    if (!nextUpdateEncodedNested) {
      return nullptr;
    }
  }

  Output output;
  if (output.Add(certID) != der::Success) {
    return nullptr;
  }
  if (output.Add(certStatus) != der::Success) {
    return nullptr;
  }
  if (output.Add(thisUpdateEncoded) != der::Success) {
    return nullptr;
  }
  if (nextUpdateEncodedNested) {
    if (output.Add(nextUpdateEncodedNested) != der::Success) {
      return nullptr;
    }
  }
  return output.Squash(context.arena, der::SEQUENCE);
}

// CertID          ::=     SEQUENCE {
//        hashAlgorithm       AlgorithmIdentifier,
//        issuerNameHash      OCTET STRING, -- Hash of issuer's DN
//        issuerKeyHash       OCTET STRING, -- Hash of issuer's public key
//        serialNumber        CertificateSerialNumber }
SECItem*
CertID(OCSPResponseContext& context)
{
  SECItem* hashAlgorithm = AlgorithmIdentifier(context.arena,
                                               context.certIDHashAlg);
  if (!hashAlgorithm) {
    return nullptr;
  }
  SECItem* issuerNameHash = HashedOctetString(context.arena,
                                              &context.issuerCert->derSubject,
                                              context.certIDHashAlg);
  if (!issuerNameHash) {
    return nullptr;
  }
  SECItem* issuerKeyHash = KeyHashHelper(context.arena,
                                         context.issuerCert.get());
  if (!issuerKeyHash) {
    return nullptr;
  }
  static const SEC_ASN1Template serialTemplate[] = {
    { SEC_ASN1_INTEGER, offsetof(CERTCertificate, serialNumber) },
    { 0 }
  };
  SECItem* serialNumber = SEC_ASN1EncodeItem(context.arena, nullptr,
                                             context.cert.get(),
                                             serialTemplate);
  if (!serialNumber) {
    return nullptr;
  }

  Output output;
  if (output.Add(hashAlgorithm) != der::Success) {
    return nullptr;
  }
  if (output.Add(issuerNameHash) != der::Success) {
    return nullptr;
  }
  if (output.Add(issuerKeyHash) != der::Success) {
    return nullptr;
  }
  if (output.Add(serialNumber) != der::Success) {
    return nullptr;
  }
  return output.Squash(context.arena, der::SEQUENCE);
}

// CertStatus ::= CHOICE {
//    good                [0] IMPLICIT NULL,
//    revoked             [1] IMPLICIT RevokedInfo,
//    unknown             [2] IMPLICIT UnknownInfo }
//
// RevokedInfo ::= SEQUENCE {
//    revocationTime              GeneralizedTime,
//    revocationReason    [0]     EXPLICIT CRLReason OPTIONAL }
//
// UnknownInfo ::= NULL
//
SECItem*
CertStatus(OCSPResponseContext& context)
{
  switch (context.certStatus) {
    // Both good and unknown are ultimately represented as NULL - the only
    // difference is in the tag that identifies them.
    case 0:
    case 2:
    {
      SECItem* status = SECITEM_AllocItem(context.arena, nullptr, 2);
      if (!status) {
        return nullptr;
      }
      status->data[0] = der::CONTEXT_SPECIFIC | context.certStatus;
      status->data[1] = 0;
      return status;
    }
    case 1:
    {
      SECItem* revocationTime = PRTimeToEncodedTime(context.arena,
                                                    context.revocationTime);
      if (!revocationTime) {
        return nullptr;
      }
      // TODO(bug 980536): add support for revocationReason
      return EncodeNested(context.arena,
                          der::CONSTRUCTED | der::CONTEXT_SPECIFIC | 1,
                          revocationTime);
    }
    default:
      PR_NOT_REACHED("CertStatus: bad context.certStatus");
      PR_Abort();
  }
  return nullptr;
}

} } // namespace insanity::test
