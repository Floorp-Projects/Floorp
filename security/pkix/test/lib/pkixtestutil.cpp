/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2013 Mozilla Contributors
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

#include "pkixtestutil.h"

#include <cerrno>
#include <limits>
#include <new>

#include "cryptohi.h"
#include "hasht.h"
#include "pk11pub.h"
#include "pkixcheck.h"
#include "pkixder.h"
#include "prinit.h"
#include "prprf.h"
#include "secder.h"

using namespace std;

namespace mozilla { namespace pkix { namespace test {

const PRTime ONE_DAY = PRTime(24) * PRTime(60) * PRTime(60) * PR_USEC_PER_SEC;

namespace {

inline void
deleteCharArray(char* chars)
{
  delete[] chars;
}

} // unnamed namespace

FILE*
OpenFile(const char* dir, const char* filename, const char* mode)
{
  PR_ASSERT(dir);
  PR_ASSERT(*dir);
  PR_ASSERT(filename);
  PR_ASSERT(*filename);

  ScopedPtr<char, deleteCharArray>
    path(new (nothrow) char[strlen(dir) + 1 + strlen(filename) + 1]);
  if (!path) {
    PR_SetError(SEC_ERROR_NO_MEMORY, 0);
    return nullptr;
  }
  strcpy(path.get(), dir);
  strcat(path.get(), "/");
  strcat(path.get(), filename);

  ScopedFILE file;
#ifdef _MSC_VER
  {
    FILE* rawFile;
    errno_t error = fopen_s(&rawFile, path.get(), mode);
    if (error) {
      // TODO: map error to NSPR error code
      PR_SetError(PR_FILE_NOT_FOUND_ERROR, error);
      rawFile = nullptr;
    }
    file = rawFile;
  }
#else
  file = fopen(path.get(), mode);
  if (!file) {
    // TODO: map errno to NSPR error code
    PR_SetError(PR_FILE_NOT_FOUND_ERROR, errno);
  }
#endif
  return file.release();
}

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

  static const size_t MaxSequenceItems = 10;
  const SECItem* contents[MaxSequenceItems];
  size_t numItems;
  size_t length;

  Output(const Output&) /* = delete */;
  void operator=(const Output&) /* = delete */;
};

OCSPResponseContext::OCSPResponseContext(PLArenaPool* arena,
                                         const CertID& certID, PRTime time)
  : arena(arena)
  , certID(certID)
  , responseStatus(successful)
  , skipResponseBytes(false)
  , signerNameDER(nullptr)
  , producedAt(time)
  , extensions(nullptr)
  , includeEmptyExtensions(false)
  , badSignature(false)
  , certs(nullptr)

  , certIDHashAlg(SEC_OID_SHA1)
  , certStatus(good)
  , revocationTime(0)
  , thisUpdate(time)
  , nextUpdate(time + 10 * PR_USEC_PER_SEC)
  , includeNextUpdate(true)
{
}

static SECItem* ResponseBytes(OCSPResponseContext& context);
static SECItem* BasicOCSPResponse(OCSPResponseContext& context);
static SECItem* ResponseData(OCSPResponseContext& context);
static SECItem* ResponderID(OCSPResponseContext& context);
static SECItem* KeyHash(OCSPResponseContext& context);
static SECItem* SingleResponse(OCSPResponseContext& context);
static SECItem* CertID(OCSPResponseContext& context);
static SECItem* CertStatus(OCSPResponseContext& context);

static SECItem*
EncodeNested(PLArenaPool* arena, uint8_t tag, const SECItem* inner)
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
HashedOctetString(PLArenaPool* arena, const SECItem& bytes, SECOidTag hashAlg)
{
  size_t hashLen = HashAlgorithmToLength(hashAlg);
  if (hashLen == 0) {
    return nullptr;
  }
  SECItem* hashBuf = SECITEM_AllocItem(arena, nullptr, hashLen);
  if (!hashBuf) {
    return nullptr;
  }
  if (PK11_HashBuf(hashAlg, hashBuf->data, bytes.data, bytes.len)
        != SECSuccess) {
    return nullptr;
  }

  return EncodeNested(arena, der::OCTET_STRING, hashBuf);
}

static SECItem*
KeyHashHelper(PLArenaPool* arena, const CERTSubjectPublicKeyInfo* spki)
{
  // We only need a shallow copy here.
  SECItem spk = spki->subjectPublicKey;
  DER_ConvertBitString(&spk); // bits to bytes
  return HashedOctetString(arena, spk, SEC_OID_SHA1);
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
BitString(PLArenaPool* arena, const SECItem* rawBytes, bool corrupt)
{
  // We have to add a byte at the beginning indicating no unused bits.
  // TODO: add ability to have bit strings of bit length not divisible by 8,
  // resulting in unused bits in the bitstring encoding
  SECItem* prefixed = SECITEM_AllocItem(arena, nullptr, rawBytes->len + 1);
  if (!prefixed) {
    return nullptr;
  }
  prefixed->data[0] = 0;
  memcpy(prefixed->data + 1, rawBytes->data, rawBytes->len);
  if (corrupt) {
    PR_ASSERT(prefixed->len > 8);
    prefixed->data[8]++;
  }
  return EncodeNested(arena, der::BIT_STRING, prefixed);
}

static SECItem*
Boolean(PLArenaPool* arena, bool value)
{
  PR_ASSERT(arena);
  SECItem* result(SECITEM_AllocItem(arena, nullptr, 3));
  if (!result) {
    return nullptr;
  }
  result->data[0] = der::BOOLEAN;
  result->data[1] = 1; // length
  result->data[2] = value ? 0xff : 0x00;
  return result;
}

static SECItem*
Integer(PLArenaPool* arena, long value)
{
  if (value < 0 || value > 127) {
    // TODO: add encoding of larger values
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return nullptr;
  }

  SECItem* encoded = SECITEM_AllocItem(arena, nullptr, 3);
  if (!encoded) {
    return nullptr;
  }
  encoded->data[0] = der::INTEGER;
  encoded->data[1] = 1; // length
  encoded->data[2] = value;
  return encoded;
}

static SECItem*
OID(PLArenaPool* arena, SECOidTag tag)
{
  const SECOidData* extnIDData(SECOID_FindOIDByTag(tag));
  if (!extnIDData) {
    return nullptr;
  }
  return EncodeNested(arena, der::OIDTag, &extnIDData->oid);
}

enum TimeEncoding { UTCTime = 0, GeneralizedTime = 1 };

// http://tools.ietf.org/html/rfc5280#section-4.1.2.5
// UTCTime:           YYMMDDHHMMSSZ (years 1950-2049 only)
// GeneralizedTime: YYYYMMDDHHMMSSZ
static SECItem*
PRTimeToEncodedTime(PLArenaPool* arena, PRTime time, TimeEncoding encoding)
{
  PR_ASSERT(encoding == UTCTime || encoding == GeneralizedTime);

  PRExplodedTime exploded;
  PR_ExplodeTime(time, PR_GMTParameters, &exploded);
  if (exploded.tm_sec >= 60) {
    // round down for leap seconds
    exploded.tm_sec = 59;
  }

  if (encoding == UTCTime &&
      (exploded.tm_year < 1950 || exploded.tm_year >= 2050)) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return nullptr;
  }

  SECItem* derTime = SECITEM_AllocItem(arena, nullptr,
                                       encoding == UTCTime ? 15 : 17);
  if (!derTime) {
    return nullptr;
  }

  size_t i = 0;

  derTime->data[i++] = encoding == GeneralizedTime ? 0x18 : 0x17; // tag
  derTime->data[i++] = static_cast<uint8_t>(derTime->len - 2); // length

  if (encoding == GeneralizedTime) {
    derTime->data[i++] = '0' + (exploded.tm_year / 1000);
    derTime->data[i++] = '0' + ((exploded.tm_year % 1000) / 100);
  }

  derTime->data[i++] = '0' + ((exploded.tm_year % 100) / 10);
  derTime->data[i++] = '0' + (exploded.tm_year % 10);
  derTime->data[i++] = '0' + ((exploded.tm_month + 1) / 10);
  derTime->data[i++] = '0' + ((exploded.tm_month + 1) % 10);
  derTime->data[i++] = '0' + (exploded.tm_mday / 10);
  derTime->data[i++] = '0' + (exploded.tm_mday % 10);
  derTime->data[i++] = '0' + (exploded.tm_hour / 10);
  derTime->data[i++] = '0' + (exploded.tm_hour % 10);
  derTime->data[i++] = '0' + (exploded.tm_min / 10);
  derTime->data[i++] = '0' + (exploded.tm_min % 10);
  derTime->data[i++] = '0' + (exploded.tm_sec / 10);
  derTime->data[i++] = '0' + (exploded.tm_sec % 10);
  derTime->data[i++] = 'Z';

  return derTime;
}

static SECItem*
PRTimeToGeneralizedTime(PLArenaPool* arena, PRTime time)
{
  return PRTimeToEncodedTime(arena, time, GeneralizedTime);
}

// http://tools.ietf.org/html/rfc5280#section-4.1.2.5: "CAs conforming to this
// profile MUST always encode certificate validity dates through the year 2049
// as UTCTime; certificate validity dates in 2050 or later MUST be encoded as
// GeneralizedTime." (This is a special case of the rule that we must always
// use the shortest possible encoding.)
static SECItem*
PRTimeToTimeChoice(PLArenaPool* arena, PRTime time)
{
  PRExplodedTime exploded;
  PR_ExplodeTime(time, PR_GMTParameters, &exploded);
  return PRTimeToEncodedTime(arena, time,
    (exploded.tm_year >= 1950 && exploded.tm_year < 2050) ? UTCTime
                                                          : GeneralizedTime);
}

PRTime
YMDHMS(int16_t year, int16_t month, int16_t day,
       int16_t hour, int16_t minutes, int16_t seconds)
{
  PRExplodedTime tm;
  tm.tm_usec = 0;
  tm.tm_sec = seconds;
  tm.tm_min = minutes;
  tm.tm_hour = hour;
  tm.tm_mday = day;
  tm.tm_month = month - 1; // tm_month is zero-based
  tm.tm_year = year;
  tm.tm_params.tp_gmt_offset = 0;
  tm.tm_params.tp_dst_offset = 0;
  return PR_ImplodeTime(&tm);
}

static SECItem*
SignedData(PLArenaPool* arena, const SECItem* tbsData,
           SECKEYPrivateKey* privKey, SECOidTag hashAlg,
           bool corrupt, /*optional*/ SECItem const* const* certs)
{
  PR_ASSERT(arena);
  PR_ASSERT(tbsData);
  PR_ASSERT(privKey);
  if (!arena || !tbsData || !privKey) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return nullptr;
  }

  SECOidTag signatureAlgTag = SEC_GetSignatureAlgorithmOidTag(privKey->keyType,
                                                              hashAlg);
  if (signatureAlgTag == SEC_OID_UNKNOWN) {
    return nullptr;
  }
  SECItem* signatureAlgorithm = AlgorithmIdentifier(arena, signatureAlgTag);
  if (!signatureAlgorithm) {
    return nullptr;
  }

  // SEC_SignData doesn't take an arena parameter, so we have to manage
  // the memory allocated in signature.
  SECItem signature;
  if (SEC_SignData(&signature, tbsData->data, tbsData->len, privKey,
                   signatureAlgTag) != SECSuccess)
  {
    return nullptr;
  }
  // TODO: add ability to have signatures of bit length not divisible by 8,
  // resulting in unused bits in the bitstring encoding
  SECItem* signatureNested = BitString(arena, &signature, corrupt);
  SECITEM_FreeItem(&signature, false);
  if (!signatureNested) {
    return nullptr;
  }

  SECItem* certsNested = nullptr;
  if (certs) {
    Output certsOutput;
    while (*certs) {
      certsOutput.Add(*certs);
      ++certs;
    }
    SECItem* certsSequence = certsOutput.Squash(arena, der::SEQUENCE);
    if (!certsSequence) {
      return nullptr;
    }
    certsNested = EncodeNested(arena,
                               der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 0,
                               certsSequence);
    if (!certsNested) {
      return nullptr;
    }
  }

  Output output;
  if (output.Add(tbsData) != der::Success) {
    return nullptr;
  }
  if (output.Add(signatureAlgorithm) != der::Success) {
    return nullptr;
  }
  if (output.Add(signatureNested) != der::Success) {
    return nullptr;
  }
  if (certsNested) {
    if (output.Add(certsNested) != der::Success) {
      return nullptr;
    }
  }
  return output.Squash(arena, der::SEQUENCE);
}

// Extension  ::=  SEQUENCE  {
//      extnID      OBJECT IDENTIFIER,
//      critical    BOOLEAN DEFAULT FALSE,
//      extnValue   OCTET STRING
//                  -- contains the DER encoding of an ASN.1 value
//                  -- corresponding to the extension type identified
//                  -- by extnID
//      }
static SECItem*
Extension(PLArenaPool* arena, SECOidTag extnIDTag,
          ExtensionCriticality criticality, Output& value)
{
  PR_ASSERT(arena);
  if (!arena) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return nullptr;
  }

  Output output;

  const SECItem* extnID(OID(arena, extnIDTag));
  if (!extnID) {
    return nullptr;
  }
  if (output.Add(extnID) != der::Success) {
    return nullptr;
  }

  if (criticality == ExtensionCriticality::Critical) {
    SECItem* critical(Boolean(arena, true));
    if (output.Add(critical) != der::Success) {
      return nullptr;
    }
  }

  SECItem* extnValueBytes(value.Squash(arena, der::SEQUENCE));
  if (!extnValueBytes) {
    return nullptr;
  }
  SECItem* extnValue(EncodeNested(arena, der::OCTET_STRING, extnValueBytes));
  if (!extnValue) {
    return nullptr;
  }
  if (output.Add(extnValue) != der::Success) {
    return nullptr;
  }

  return output.Squash(arena, der::SEQUENCE);
}

SECItem*
MaybeLogOutput(SECItem* result, const char* suffix)
{
  PR_ASSERT(suffix);

  if (!result) {
    return nullptr;
  }

  // This allows us to more easily debug the generated output, by creating a
  // file in the directory given by MOZILLA_PKIX_TEST_LOG_DIR for each
  // NOT THREAD-SAFE!!!
  const char* logPath = getenv("MOZILLA_PKIX_TEST_LOG_DIR");
  if (logPath) {
    static int counter = 0;
    ScopedPtr<char, PR_smprintf_free>
      filename(PR_smprintf("%u-%s.der", counter, suffix));
    ++counter;
    if (filename) {
      ScopedFILE file(OpenFile(logPath, filename.get(), "wb"));
      if (file) {
        (void) fwrite(result->data, result->len, 1, file.get());
      }
    }
  }

  return result;
}

///////////////////////////////////////////////////////////////////////////////
// Key Pairs

SECStatus
GenerateKeyPair(/*out*/ ScopedSECKEYPublicKey& publicKey,
                /*out*/ ScopedSECKEYPrivateKey& privateKey)
{
  ScopedPtr<PK11SlotInfo, PK11_FreeSlot> slot(PK11_GetInternalSlot());
  if (!slot) {
    return SECFailure;
  }

  // Bug 1012786: PK11_GenerateKeyPair can fail if there is insufficient
  // entropy to generate a random key. Attempting to add some entropy and
  // retrying appears to solve this issue.
  for (uint32_t retries = 0; retries < 10; retries++) {
    PK11RSAGenParams params;
    params.keySizeInBits = 2048;
    params.pe = 3;
    SECKEYPublicKey* publicKeyTemp = nullptr;
    privateKey = PK11_GenerateKeyPair(slot.get(), CKM_RSA_PKCS_KEY_PAIR_GEN,
                                      &params, &publicKeyTemp, false, true,
                                      nullptr);
    if (privateKey) {
      publicKey = publicKeyTemp;
      PR_ASSERT(publicKey);
      return SECSuccess;
    }

    PR_ASSERT(!publicKeyTemp);

    if (PR_GetError() != SEC_ERROR_PKCS11_FUNCTION_FAILED) {
      return SECFailure;
    }

    PRTime now = PR_Now();
    if (PK11_RandomUpdate(&now, sizeof(PRTime)) != SECSuccess) {
      return SECFailure;
    }
  }
  return SECFailure;
}


///////////////////////////////////////////////////////////////////////////////
// Certificates

static SECItem* TBSCertificate(PLArenaPool* arena, long version,
                               const SECItem* serialNumber, SECOidTag signature,
                               const SECItem* issuer, PRTime notBefore,
                               PRTime notAfter, const SECItem* subject,
                               const SECKEYPublicKey* subjectPublicKey,
                               /*optional*/ SECItem const* const* extensions);

// Certificate  ::=  SEQUENCE  {
//         tbsCertificate       TBSCertificate,
//         signatureAlgorithm   AlgorithmIdentifier,
//         signatureValue       BIT STRING  }
SECItem*
CreateEncodedCertificate(PLArenaPool* arena, long version,
                         SECOidTag signature, const SECItem* serialNumber,
                         const SECItem* issuerNameDER, PRTime notBefore,
                         PRTime notAfter, const SECItem* subjectNameDER,
                         /*optional*/ SECItem const* const* extensions,
                         /*optional*/ SECKEYPrivateKey* issuerPrivateKey,
                         SECOidTag signatureHashAlg,
                         /*out*/ ScopedSECKEYPrivateKey& privateKeyResult)
{
  PR_ASSERT(arena);
  PR_ASSERT(issuerNameDER);
  PR_ASSERT(subjectNameDER);
  if (!arena || !issuerNameDER || !subjectNameDER) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return nullptr;
  }

  // It may be the case that privateKeyResult refers to the
  // ScopedSECKEYPrivateKey that owns issuerPrivateKey; thus, we can't set
  // privateKeyResult until after we're done with issuerPrivateKey.
  ScopedSECKEYPublicKey publicKey;
  ScopedSECKEYPrivateKey privateKeyTemp;
  if (GenerateKeyPair(publicKey, privateKeyTemp) != SECSuccess) {
    return nullptr;
  }

  SECItem* tbsCertificate(TBSCertificate(arena, version, serialNumber,
                                         signature, issuerNameDER, notBefore,
                                         notAfter, subjectNameDER,
                                         publicKey.get(), extensions));
  if (!tbsCertificate) {
    return nullptr;
  }

  SECItem*
    result(MaybeLogOutput(SignedData(arena, tbsCertificate,
                                     issuerPrivateKey ? issuerPrivateKey
                                                      : privateKeyTemp.get(),
                                     signatureHashAlg, false, nullptr),
                          "cert"));
  if (!result) {
    return nullptr;
  }
  privateKeyResult = privateKeyTemp.release();
  return result;
}

// TBSCertificate  ::=  SEQUENCE  {
//      version         [0]  Version DEFAULT v1,
//      serialNumber         CertificateSerialNumber,
//      signature            AlgorithmIdentifier,
//      issuer               Name,
//      validity             Validity,
//      subject              Name,
//      subjectPublicKeyInfo SubjectPublicKeyInfo,
//      issuerUniqueID  [1]  IMPLICIT UniqueIdentifier OPTIONAL,
//                           -- If present, version MUST be v2 or v3
//      subjectUniqueID [2]  IMPLICIT UniqueIdentifier OPTIONAL,
//                           -- If present, version MUST be v2 or v3
//      extensions      [3]  Extensions OPTIONAL
//                           -- If present, version MUST be v3 --  }
static SECItem*
TBSCertificate(PLArenaPool* arena, long versionValue,
               const SECItem* serialNumber, SECOidTag signatureOidTag,
               const SECItem* issuer, PRTime notBeforeTime,
               PRTime notAfterTime, const SECItem* subject,
               const SECKEYPublicKey* subjectPublicKey,
               /*optional*/ SECItem const* const* extensions)
{
  PR_ASSERT(arena);
  PR_ASSERT(issuer);
  PR_ASSERT(subject);
  PR_ASSERT(subjectPublicKey);
  if (!arena || !issuer || !subject || !subjectPublicKey) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return nullptr;
  }

  Output output;

  if (versionValue != static_cast<long>(der::Version::v1)) {
    SECItem* versionInteger(Integer(arena, versionValue));
    if (!versionInteger) {
      return nullptr;
    }
    SECItem* version(EncodeNested(arena,
                                  der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 0,
                                  versionInteger));
    if (!version) {
      return nullptr;
    }
    if (output.Add(version) != der::Success) {
      return nullptr;
    }
  }

  if (output.Add(serialNumber) != der::Success) {
    return nullptr;
  }

  SECItem* signature(AlgorithmIdentifier(arena, signatureOidTag));
  if (!signature) {
    return nullptr;
  }
  if (output.Add(signature) != der::Success) {
    return nullptr;
  }

  if (output.Add(issuer) != der::Success) {
    return nullptr;
  }

  // Validity ::= SEQUENCE {
  //       notBefore      Time,
  //       notAfter       Time }
  SECItem* validity;
  {
    SECItem* notBefore(PRTimeToTimeChoice(arena, notBeforeTime));
    if (!notBefore) {
      return nullptr;
    }
    SECItem* notAfter(PRTimeToTimeChoice(arena, notAfterTime));
    if (!notAfter) {
      return nullptr;
    }
    Output validityOutput;
    if (validityOutput.Add(notBefore) != der::Success) {
      return nullptr;
    }
    if (validityOutput.Add(notAfter) != der::Success) {
      return nullptr;
    }
    validity = validityOutput.Squash(arena, der::SEQUENCE);
    if (!validity) {
      return nullptr;
    }
  }
  if (output.Add(validity) != der::Success) {
    return nullptr;
  }

  if (output.Add(subject) != der::Success) {
    return nullptr;
  }

  // SubjectPublicKeyInfo  ::=  SEQUENCE  {
  //       algorithm            AlgorithmIdentifier,
  //       subjectPublicKey     BIT STRING  }
  ScopedSECItem subjectPublicKeyInfo(
    SECKEY_EncodeDERSubjectPublicKeyInfo(subjectPublicKey));
  if (!subjectPublicKeyInfo) {
    return nullptr;
  }
  if (output.Add(subjectPublicKeyInfo.get()) != der::Success) {
    return nullptr;
  }

  if (extensions) {
    Output extensionsOutput;
    while (*extensions) {
      if (extensionsOutput.Add(*extensions) != der::Success) {
        return nullptr;
      }
      ++extensions;
    }
    SECItem* allExtensions(extensionsOutput.Squash(arena, der::SEQUENCE));
    if (!allExtensions) {
      return nullptr;
    }
    SECItem* extensionsWrapped(
      EncodeNested(arena, der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 3,
                   allExtensions));
    if (!extensions) {
      return nullptr;
    }
    if (output.Add(extensionsWrapped) != der::Success) {
      return nullptr;
    }
  }

  return output.Squash(arena, der::SEQUENCE);
}

const SECItem*
ASCIIToDERName(PLArenaPool* arena, const char* cn)
{
  ScopedPtr<CERTName, CERT_DestroyName> certName(CERT_AsciiToName(cn));
  if (!certName) {
    return nullptr;
  }
  return SEC_ASN1EncodeItem(arena, nullptr, certName.get(),
                            SEC_ASN1_GET(CERT_NameTemplate));
}

SECItem*
CreateEncodedSerialNumber(PLArenaPool* arena, long serialNumberValue)
{
  return Integer(arena, serialNumberValue);
}

// BasicConstraints ::= SEQUENCE {
//         cA                      BOOLEAN DEFAULT FALSE,
//         pathLenConstraint       INTEGER (0..MAX) OPTIONAL }
SECItem*
CreateEncodedBasicConstraints(PLArenaPool* arena, bool isCA,
                              /*optional*/ long* pathLenConstraintValue,
                              ExtensionCriticality criticality)
{
  PR_ASSERT(arena);
  if (!arena) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return nullptr;
  }

  Output value;

  if (isCA) {
    if (value.Add(Boolean(arena, true)) != der::Success) {
      return nullptr;
    }
  }

  if (pathLenConstraintValue) {
    SECItem* pathLenConstraint(Integer(arena, *pathLenConstraintValue));
    if (!pathLenConstraint) {
      return nullptr;
    }
    if (value.Add(pathLenConstraint) != der::Success) {
      return nullptr;
    }
  }

  return Extension(arena, SEC_OID_X509_BASIC_CONSTRAINTS, criticality, value);
}

// ExtKeyUsageSyntax ::= SEQUENCE SIZE (1..MAX) OF KeyPurposeId
// KeyPurposeId ::= OBJECT IDENTIFIER
SECItem*
CreateEncodedEKUExtension(PLArenaPool* arena, SECOidTag const* ekus,
                          size_t ekusCount, ExtensionCriticality criticality)
{
  PR_ASSERT(arena);
  PR_ASSERT(ekus);
  if (!arena || (!ekus && ekusCount != 0)) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return nullptr;
  }

  Output value;
  for (size_t i = 0; i < ekusCount; ++i) {
    SECItem* encodedEKUOID = OID(arena, ekus[i]);
    if (!encodedEKUOID) {
      return nullptr;
    }
    if (value.Add(encodedEKUOID) != der::Success) {
      return nullptr;
    }
  }

  return Extension(arena, SEC_OID_X509_EXT_KEY_USAGE, criticality, value);
}

///////////////////////////////////////////////////////////////////////////////
// OCSP responses

SECItem*
CreateEncodedOCSPResponse(OCSPResponseContext& context)
{
  if (!context.arena) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return nullptr;
  }

  if (!context.skipResponseBytes) {
    if (!context.signerPrivateKey) {
      PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
      return nullptr;
    }
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

  SECItem* responseBytesNested = nullptr;
  if (!context.skipResponseBytes) {
    SECItem* responseBytes = ResponseBytes(context);
    if (!responseBytes) {
      return nullptr;
    }

    responseBytesNested = EncodeNested(context.arena,
                                       der::CONSTRUCTED |
                                       der::CONTEXT_SPECIFIC,
                                       responseBytes);
    if (!responseBytesNested) {
      return nullptr;
    }
  }

  Output output;
  if (output.Add(responseStatus) != der::Success) {
    return nullptr;
  }
  if (responseBytesNested) {
    if (output.Add(responseBytesNested) != der::Success) {
      return nullptr;
    }
  }
  return MaybeLogOutput(output.Squash(context.arena, der::SEQUENCE), "ocsp");
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

  // TODO(bug 980538): certs
  return SignedData(context.arena, tbsResponseData,
                    context.signerPrivateKey.get(), SEC_OID_SHA1,
                    context.badSignature, context.certs);
}

// Extension ::= SEQUENCE {
//   id               OBJECT IDENTIFIER,
//   critical         BOOLEAN DEFAULT FALSE
//   value            OCTET STRING
// }
static SECItem*
OCSPExtension(OCSPResponseContext& context, OCSPResponseExtension* extension)
{
  Output output;
  if (output.Add(&extension->id) != der::Success) {
    return nullptr;
  }
  if (extension->critical) {
    static const uint8_t trueEncoded[3] = { 0x01, 0x01, 0xFF };
    SECItem critical = {
      siBuffer,
      const_cast<uint8_t*>(trueEncoded),
      PR_ARRAY_SIZE(trueEncoded)
    };
    if (output.Add(&critical) != der::Success) {
      return nullptr;
    }
  }
  SECItem* value = EncodeNested(context.arena, der::OCTET_STRING,
                                &extension->value);
  if (!value) {
    return nullptr;
  }
  if (output.Add(value) != der::Success) {
    return nullptr;
  }
  return output.Squash(context.arena, der::SEQUENCE);
}

// Extensions ::= [1] {
//   SEQUENCE OF Extension
// }
static SECItem*
Extensions(OCSPResponseContext& context)
{
  Output output;
  for (OCSPResponseExtension* extension = context.extensions;
       extension; extension = extension->next) {
    SECItem* extensionEncoded = OCSPExtension(context, extension);
    if (!extensionEncoded) {
      return nullptr;
    }
    if (output.Add(extensionEncoded) != der::Success) {
      return nullptr;
    }
  }
  SECItem* extensionsEncoded = output.Squash(context.arena, der::SEQUENCE);
  if (!extensionsEncoded) {
    return nullptr;
  }
  return EncodeNested(context.arena,
                      der::CONSTRUCTED |
                      der::CONTEXT_SPECIFIC |
                      1,
                      extensionsEncoded);
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
  SECItem* producedAtEncoded = PRTimeToGeneralizedTime(context.arena,
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
  SECItem* responseExtensions = nullptr;
  if (context.extensions || context.includeEmptyExtensions) {
    responseExtensions = Extensions(context);
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
  if (responseExtensions) {
    if (output.Add(responseExtensions) != der::Success) {
      return nullptr;
    }
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
  const SECItem* contents;
  uint8_t responderIDType;
  if (context.signerNameDER) {
    contents = context.signerNameDER;
    responderIDType = 1; // byName
  } else {
    contents = KeyHash(context);
    responderIDType = 2; // byKey
  }
  if (!contents) {
    return nullptr;
  }

  return EncodeNested(context.arena,
                      der::CONSTRUCTED |
                      der::CONTEXT_SPECIFIC |
                      responderIDType,
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
  ScopedSECKEYPublicKey
    signerPublicKey(SECKEY_ConvertToPublicKey(context.signerPrivateKey.get()));
  if (!signerPublicKey) {
    return nullptr;
  }
  ScopedPtr<CERTSubjectPublicKeyInfo, SECKEY_DestroySubjectPublicKeyInfo>
    signerSPKI(SECKEY_CreateSubjectPublicKeyInfo(signerPublicKey.get()));
  if (!signerSPKI) {
    return nullptr;
  }
  return KeyHashHelper(context.arena, signerSPKI.get());
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
  SECItem* thisUpdateEncoded = PRTimeToGeneralizedTime(context.arena,
                                                       context.thisUpdate);
  if (!thisUpdateEncoded) {
    return nullptr;
  }
  SECItem* nextUpdateEncodedNested = nullptr;
  if (context.includeNextUpdate) {
    SECItem* nextUpdateEncoded = PRTimeToGeneralizedTime(context.arena,
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
                                              context.certID.issuer,
                                              context.certIDHashAlg);
  if (!issuerNameHash) {
    return nullptr;
  }

  ScopedPtr<CERTSubjectPublicKeyInfo, SECKEY_DestroySubjectPublicKeyInfo>
    spki(SECKEY_DecodeDERSubjectPublicKeyInfo(
          &context.certID.issuerSubjectPublicKeyInfo));
  if (!spki) {
    return nullptr;
  }
  SECItem* issuerKeyHash(KeyHashHelper(context.arena, spki.get()));
  if (!issuerKeyHash) {
    return nullptr;
  }

  static const SEC_ASN1Template serialTemplate[] = {
    { SEC_ASN1_INTEGER, 0 },
    { 0 }
  };
  SECItem* serialNumber = SEC_ASN1EncodeItem(context.arena, nullptr,
                                             &context.certID.serialNumber,
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
      SECItem* revocationTime = PRTimeToGeneralizedTime(context.arena,
                                                        context.revocationTime);
      if (!revocationTime) {
        return nullptr;
      }
      // TODO(bug 980536): add support for revocationReason
      return EncodeNested(context.arena,
                          der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 1,
                          revocationTime);
    }
    default:
      PR_NOT_REACHED("CertStatus: bad context.certStatus");
      PR_Abort();
  }
  return nullptr;
}

} } } // namespace mozilla::pkix::test
