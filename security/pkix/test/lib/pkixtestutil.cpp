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
#include <cstdio>
#include <limits>
#include <new>

#include "cert.h"
#include "cryptohi.h"
#include "hasht.h"
#include "pk11pub.h"
#include "pkix/pkixnss.h"
#include "pkixder.h"
#include "pkixutil.h"
#include "prinit.h"
#include "prprf.h"
#include "secerr.h"

using namespace std;

namespace mozilla { namespace pkix { namespace test {

// python DottedOIDToCode.py --alg sha256WithRSAEncryption 1.2.840.113549.1.1.11
static const uint8_t alg_sha256WithRSAEncryption[] = {
  0x30, 0x0b, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b
};
const Input sha256WithRSAEncryption(alg_sha256WithRSAEncryption);

namespace {

inline void
deleteCharArray(char* chars)
{
  delete[] chars;
}

inline void
fclose_void(FILE* file) {
  (void) fclose(file);
}

typedef mozilla::pkix::ScopedPtr<FILE, fclose_void> ScopedFILE;

FILE*
OpenFile(const char* dir, const char* filename, const char* mode)
{
  assert(dir);
  assert(*dir);
  assert(filename);
  assert(*filename);

  ScopedPtr<char, deleteCharArray>
    path(new (nothrow) char[strlen(dir) + 1 + strlen(filename) + 1]);
  if (!path) {
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
      rawFile = nullptr;
    }
    file = rawFile;
  }
#else
  file = fopen(path.get(), mode);
#endif
  return file.release();
}

} // unnamed namespace

Result
TamperOnce(/*in/out*/ ByteString& item, const ByteString& from,
           const ByteString& to)
{
  if (from.length() < 8) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }
  if (from.length() != to.length()) {
    return Result::FATAL_ERROR_INVALID_ARGS;
  }
  size_t pos = item.find(from);
  if (pos == string::npos) {
    return Result::FATAL_ERROR_INVALID_ARGS; // No matches.
  }
  if (item.find(from, pos + from.length()) != string::npos) {
    return Result::FATAL_ERROR_INVALID_ARGS; // More than once match.
  }
  item.replace(pos, from.length(), to);
  return Success;
}

// An empty string returned from an encoding function signifies failure.
const ByteString ENCODING_FAILED;

// Given a tag and a value, generates a DER-encoded tag-length-value item.
static ByteString
TLV(uint8_t tag, const ByteString& value)
{
  ByteString result;
  result.push_back(tag);

  if (value.length() < 128) {
    result.push_back(value.length());
  } else if (value.length() < 256) {
    result.push_back(0x81u);
    result.push_back(value.length());
  } else if (value.length() < 65536) {
    result.push_back(0x82u);
    result.push_back(static_cast<uint8_t>(value.length() / 256));
    result.push_back(static_cast<uint8_t>(value.length() % 256));
  } else {
    assert(false);
    return ENCODING_FAILED;
  }
  result.append(value);
  return result;
}

static SECItem*
ArenaDupByteString(PLArenaPool* arena, const ByteString& value)
{
  SECItem* result = SECITEM_AllocItem(arena, nullptr, value.length());
  if (!result) {
    return nullptr;
  }
  memcpy(result->data, value.data(), value.length());
  return result;
}

class Output
{
public:
  Output()
  {
  }

  // Makes a shallow copy of the input item. All input items must have a
  // lifetime that extends at least to where Squash is called.
  Result Add(const SECItem* item)
  {
    assert(item);
    assert(item->data);
    contents.append(item->data, item->len);
    return Success; // XXX: return type should be void
  }

  void Add(const ByteString& item)
  {
    contents.append(item);
  }

  SECItem* Squash(PLArenaPool* arena, uint8_t tag)
  {
    assert(arena);

    size_t lengthLength = contents.length() < 128 ? 1
                        : contents.length() < 256 ? 2
                        : 3;
    size_t totalLength = 1 + lengthLength + contents.length();
    SECItem* output = SECITEM_AllocItem(arena, nullptr, totalLength);
    if (!output) {
      return nullptr;
    }
    uint8_t* d = output->data;
    *d++ = tag;
    EncodeLength(d, contents.length(), lengthLength);
    d += lengthLength;
    memcpy(d, contents.data(), contents.length());
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
        abort();
    }
  }

  ByteString contents;
};

OCSPResponseContext::OCSPResponseContext(PLArenaPool* arena,
                                         const CertID& certID, time_t time)
  : arena(arena)
  , certID(certID)
  , responseStatus(successful)
  , skipResponseBytes(false)
  , producedAt(time)
  , extensions(nullptr)
  , includeEmptyExtensions(false)
  , badSignature(false)
  , certs(nullptr)

  , certStatus(good)
  , revocationTime(0)
  , thisUpdate(time)
  , nextUpdate(time + 10)
  , includeNextUpdate(true)
{
}

static ByteString ResponseBytes(OCSPResponseContext& context);
static ByteString BasicOCSPResponse(OCSPResponseContext& context);
static SECItem* ResponseData(OCSPResponseContext& context);
static ByteString ResponderID(OCSPResponseContext& context);
static ByteString KeyHash(OCSPResponseContext& context);
static SECItem* SingleResponse(OCSPResponseContext& context);
static SECItem* CertID(OCSPResponseContext& context);
static ByteString CertStatus(OCSPResponseContext& context);

static SECItem*
EncodeNested(PLArenaPool* arena, uint8_t tag, const SECItem* inner)
{
  Output output;
  if (output.Add(inner) != Success) {
    return nullptr;
  }
  return output.Squash(arena, tag);
}

static ByteString
HashedOctetString(const SECItem& bytes)
{
  uint8_t hashBuf[TrustDomain::DIGEST_LENGTH];
  Input input;
  if (input.Init(bytes.data, bytes.len) != Success) {
    return ENCODING_FAILED;
  }
  if (DigestBuf(input, hashBuf, sizeof(hashBuf)) != Success) {
    return ENCODING_FAILED;
  }
  return TLV(der::OCTET_STRING, ByteString(hashBuf, sizeof(hashBuf)));
}

static ByteString
KeyHashHelper(const CERTSubjectPublicKeyInfo* spki)
{
  // We only need a shallow copy here.
  SECItem spk = spki->subjectPublicKey;
  DER_ConvertBitString(&spk); // bits to bytes
  return HashedOctetString(spk);
}

static ByteString
BitString(const ByteString& rawBytes, bool corrupt)
{
  ByteString prefixed;
  // We have to add a byte at the beginning indicating no unused bits.
  // TODO: add ability to have bit strings of bit length not divisible by 8,
  // resulting in unused bits in the bitstring encoding
  prefixed.push_back(0);
  prefixed.append(rawBytes);
  if (corrupt) {
    assert(prefixed.length() > 8);
    prefixed[8]++;
  }
  return TLV(der::BIT_STRING, prefixed);
}

static ByteString
Boolean(bool value)
{
  ByteString encodedValue;
  encodedValue.push_back(value ? 0xff : 0x00);
  return TLV(der::BOOLEAN, encodedValue);
}

static ByteString
Integer(long value)
{
  if (value < 0 || value > 127) {
    // TODO: add encoding of larger values
    return ENCODING_FAILED;
  }

  ByteString encodedValue;
  encodedValue.push_back(static_cast<uint8_t>(value));
  return TLV(der::INTEGER, encodedValue);
}

enum TimeEncoding { UTCTime = 0, GeneralizedTime = 1 };

// Windows doesn't provide gmtime_r, but it provides something very similar.
#ifdef WIN32
static tm*
gmtime_r(const time_t* t, /*out*/ tm* exploded)
{
  if (gmtime_s(exploded, t) != 0) {
    return nullptr;
  }
  return exploded;
}
#endif

// http://tools.ietf.org/html/rfc5280#section-4.1.2.5
// UTCTime:           YYMMDDHHMMSSZ (years 1950-2049 only)
// GeneralizedTime: YYYYMMDDHHMMSSZ
//
// This assumes that time/time_t are POSIX-compliant in that time() returns
// the number of seconds since the Unix epoch.
static ByteString
TimeToEncodedTime(time_t time, TimeEncoding encoding)
{
  assert(encoding == UTCTime || encoding == GeneralizedTime);

  tm exploded;
  if (!gmtime_r(&time, &exploded)) {
    return ENCODING_FAILED;
  }

  if (exploded.tm_sec >= 60) {
    // round down for leap seconds
    exploded.tm_sec = 59;
  }

  // exploded.tm_year is the year offset by 1900.
  int year = exploded.tm_year + 1900;

  if (encoding == UTCTime && (year < 1950 || year >= 2050)) {
    return ENCODING_FAILED;
  }

  ByteString value;

  if (encoding == GeneralizedTime) {
    value.push_back('0' + (year / 1000));
    value.push_back('0' + ((year % 1000) / 100));
  }

  value.push_back('0' + ((year % 100) / 10));
  value.push_back('0' + (year % 10));
  value.push_back('0' + ((exploded.tm_mon + 1) / 10));
  value.push_back('0' + ((exploded.tm_mon + 1) % 10));
  value.push_back('0' + (exploded.tm_mday / 10));
  value.push_back('0' + (exploded.tm_mday % 10));
  value.push_back('0' + (exploded.tm_hour / 10));
  value.push_back('0' + (exploded.tm_hour % 10));
  value.push_back('0' + (exploded.tm_min / 10));
  value.push_back('0' + (exploded.tm_min % 10));
  value.push_back('0' + (exploded.tm_sec / 10));
  value.push_back('0' + (exploded.tm_sec % 10));
  value.push_back('Z');

  return TLV(encoding == GeneralizedTime ? der::GENERALIZED_TIME : der::UTCTime,
             value);
}

static ByteString
TimeToGeneralizedTime(time_t time)
{
  return TimeToEncodedTime(time, GeneralizedTime);
}

// http://tools.ietf.org/html/rfc5280#section-4.1.2.5: "CAs conforming to this
// profile MUST always encode certificate validity dates through the year 2049
// as UTCTime; certificate validity dates in 2050 or later MUST be encoded as
// GeneralizedTime." (This is a special case of the rule that we must always
// use the shortest possible encoding.)
static ByteString
TimeToTimeChoice(time_t time)
{
  tm exploded;
  if (!gmtime_r(&time, &exploded)) {
    return ENCODING_FAILED;
  }
  TimeEncoding encoding = (exploded.tm_year + 1900 >= 1950 &&
                           exploded.tm_year + 1900 < 2050)
                        ? UTCTime
                        : GeneralizedTime;

  return TimeToEncodedTime(time, encoding);
}

Time
YMDHMS(int16_t year, int16_t month, int16_t day,
       int16_t hour, int16_t minutes, int16_t seconds)
{
  assert(year <= 9999);
  assert(month >= 1);
  assert(month <= 12);
  assert(day >= 1);
  assert(hour >= 0);
  assert(hour < 24);
  assert(minutes >= 0);
  assert(minutes < 60);
  assert(seconds >= 0);
  assert(seconds < 60);

  uint64_t days = DaysBeforeYear(year);

  {
    static const int16_t DAYS_IN_MONTH[] = {
      31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };

    int16_t i = 1;
    for (;;) {
      int16_t daysInMonth = DAYS_IN_MONTH[i - 1];
      if (i == 2 &&
          ((year % 4 == 0) && ((year % 100 != 0) || (year % 400 == 0)))) {
        // Add leap day
        ++daysInMonth;
      }
      if (i == month) {
        assert(day <= daysInMonth);
        break;
      }
      days += daysInMonth;
      ++i;
    }
  }

  days += (day - 1);

  uint64_t totalSeconds = days * Time::ONE_DAY_IN_SECONDS;
  totalSeconds += hour * 60 * 60;
  totalSeconds += minutes * 60;
  totalSeconds += seconds;
  return TimeFromElapsedSecondsAD(totalSeconds);
}

static ByteString
SignedData(const SECItem* tbsData,
           SECKEYPrivateKey* privKey,
           SignatureAlgorithm signatureAlgorithm,
           bool corrupt, /*optional*/ SECItem const* const* certs)
{
  assert(tbsData);
  assert(privKey);
  if (!tbsData || !privKey) {
    return ENCODING_FAILED;
  }

  SECOidTag signatureAlgorithmOidTag;
  ByteString signatureAlgorithmDER;
  switch (signatureAlgorithm) {
    case SignatureAlgorithm::rsa_pkcs1_with_sha256:
      signatureAlgorithmOidTag = SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION;
      signatureAlgorithmDER.assign(alg_sha256WithRSAEncryption,
                                   sizeof(alg_sha256WithRSAEncryption));
      break;
    default:
      return ENCODING_FAILED;
  }

  SECItem signature;
  if (SEC_SignData(&signature, tbsData->data, tbsData->len, privKey,
                   signatureAlgorithmOidTag) != SECSuccess)
  {
    return nullptr;
  }
  // TODO: add ability to have signatures of bit length not divisible by 8,
  // resulting in unused bits in the bitstring encoding
  ByteString signatureNested(BitString(ByteString(signature.data, signature.len),
                                       corrupt));
  SECITEM_FreeItem(&signature, false);
  if (signatureNested == ENCODING_FAILED) {
    return nullptr;
  }

  ByteString certsNested;
  if (certs) {
    ByteString certsSequenceValue;
    while (*certs) {
      certsSequenceValue.append(ByteString((*certs)->data, (*certs)->len));
      ++certs;
    }
    ByteString certsSequence(TLV(der::SEQUENCE, certsSequenceValue));
    if (certsSequence == ENCODING_FAILED) {
      return ENCODING_FAILED;
    }
    certsNested = TLV(der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 0,
                      certsSequence);
    if (certsNested == ENCODING_FAILED) {
      return ENCODING_FAILED;
    }
  }

  ByteString value;
  value.append(ByteString(tbsData->data, tbsData->len));
  value.append(signatureAlgorithmDER);
  value.append(signatureNested);
  value.append(certsNested);
  return TLV(der::SEQUENCE, value);
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
Extension(PLArenaPool* arena, Input extnID,
          ExtensionCriticality criticality, Output& value)
{
  assert(arena);
  if (!arena) {
    return nullptr;
  }

  Output output;

  const SECItem extnIDItem = UnsafeMapInputToSECItem(extnID);
  if (output.Add(&extnIDItem) != Success) {
    return nullptr;
  }

  if (criticality == ExtensionCriticality::Critical) {
    ByteString critical(Boolean(true));
    if (critical == ENCODING_FAILED) {
      return nullptr;
    }
    output.Add(critical);
  }

  SECItem* extnValueBytes(value.Squash(arena, der::SEQUENCE));
  if (!extnValueBytes) {
    return nullptr;
  }
  SECItem* extnValue(EncodeNested(arena, der::OCTET_STRING, extnValueBytes));
  if (!extnValue) {
    return nullptr;
  }
  if (output.Add(extnValue) != Success) {
    return nullptr;
  }

  return output.Squash(arena, der::SEQUENCE);
}

void
MaybeLogOutput(const ByteString& result, const char* suffix)
{
  assert(suffix);

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
        (void) fwrite(result.data(), result.length(), 1, file.get());
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
// Key Pairs

Result
GenerateKeyPair(/*out*/ ScopedSECKEYPublicKey& publicKey,
                /*out*/ ScopedSECKEYPrivateKey& privateKey)
{
  ScopedPtr<PK11SlotInfo, PK11_FreeSlot> slot(PK11_GetInternalSlot());
  if (!slot) {
    return MapPRErrorCodeToResult(PR_GetError());
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
      assert(publicKey);
      return Success;
    }

    assert(!publicKeyTemp);

    if (PR_GetError() != SEC_ERROR_PKCS11_FUNCTION_FAILED) {
      break;
    }

    // Since these keys are only for testing, we don't need them to be good,
    // random keys.
    // https://xkcd.com/221/
    static const uint8_t RANDOM_NUMBER[] = { 4, 4, 4, 4, 4, 4, 4, 4 };
    if (PK11_RandomUpdate((void*) &RANDOM_NUMBER,
                          sizeof(RANDOM_NUMBER)) != SECSuccess) {
      break;
    }
  }

  return MapPRErrorCodeToResult(PR_GetError());
}


///////////////////////////////////////////////////////////////////////////////
// Certificates

static SECItem* TBSCertificate(PLArenaPool* arena, long version,
                               const ByteString& serialNumber, Input signature,
                               const ByteString& issuer, time_t notBefore,
                               time_t notAfter, const ByteString& subject,
                               const SECKEYPublicKey* subjectPublicKey,
                               /*optional*/ SECItem const* const* extensions);

// Certificate  ::=  SEQUENCE  {
//         tbsCertificate       TBSCertificate,
//         signatureAlgorithm   AlgorithmIdentifier,
//         signatureValue       BIT STRING  }
SECItem*
CreateEncodedCertificate(PLArenaPool* arena, long version, Input signature,
                         const ByteString& serialNumber,
                         const ByteString& issuerNameDER,
                         time_t notBefore, time_t notAfter,
                         const ByteString& subjectNameDER,
                         /*optional*/ SECItem const* const* extensions,
                         /*optional*/ SECKEYPrivateKey* issuerPrivateKey,
                         SignatureAlgorithm signatureAlgorithm,
                         /*out*/ ScopedSECKEYPrivateKey& privateKeyResult)
{
  assert(arena);
  if (!arena) {
    return nullptr;
  }

  // It may be the case that privateKeyResult refers to the
  // ScopedSECKEYPrivateKey that owns issuerPrivateKey; thus, we can't set
  // privateKeyResult until after we're done with issuerPrivateKey.
  ScopedSECKEYPublicKey publicKey;
  ScopedSECKEYPrivateKey privateKeyTemp;
  if (GenerateKeyPair(publicKey, privateKeyTemp) != Success) {
    return nullptr;
  }

  SECItem* tbsCertificate(TBSCertificate(arena, version, serialNumber,
                                         signature, issuerNameDER, notBefore,
                                         notAfter, subjectNameDER,
                                         publicKey.get(), extensions));
  if (!tbsCertificate) {
    return nullptr;
  }

  ByteString result(SignedData(tbsCertificate,
                               issuerPrivateKey ? issuerPrivateKey
                                                : privateKeyTemp.get(),
                               signatureAlgorithm, false, nullptr));
  if (result == ENCODING_FAILED) {
    return nullptr;
  }

  MaybeLogOutput(result, "cert");

  privateKeyResult = privateKeyTemp.release();

  return ArenaDupByteString(arena, result);
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
               const ByteString& serialNumber, Input signature,
               const ByteString& issuer, time_t notBeforeTime,
               time_t notAfterTime, const ByteString& subject,
               const SECKEYPublicKey* subjectPublicKey,
               /*optional*/ SECItem const* const* extensions)
{
  assert(arena);
  assert(subjectPublicKey);
  if (!arena || !subjectPublicKey) {
    return nullptr;
  }

  Output output;

  if (versionValue != static_cast<long>(der::Version::v1)) {
    ByteString versionInteger(Integer(versionValue));
    if (versionInteger == ENCODING_FAILED) {
      return nullptr;
    }
    ByteString version(TLV(der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 0,
                           versionInteger));
    if (version == ENCODING_FAILED) {
      return nullptr;
    }
    output.Add(version);
  }

  output.Add(serialNumber);

  SECItem signatureItem = UnsafeMapInputToSECItem(signature);
  if (output.Add(&signatureItem) != Success) {
    return nullptr;
  }

  output.Add(issuer);

  // Validity ::= SEQUENCE {
  //       notBefore      Time,
  //       notAfter       Time }
  ByteString validity;
  {
    ByteString notBefore(TimeToTimeChoice(notBeforeTime));
    if (notBefore == ENCODING_FAILED) {
      return nullptr;
    }
    ByteString notAfter(TimeToTimeChoice(notAfterTime));
    if (notAfter == ENCODING_FAILED) {
      return nullptr;
    }
    ByteString validityValue;
    validityValue.append(notBefore);
    validityValue.append(notAfter);
    validity = TLV(der::SEQUENCE, validityValue);
    if (validity == ENCODING_FAILED) {
      return nullptr;
    }
  }
  output.Add(validity);

  output.Add(subject);

  // SubjectPublicKeyInfo  ::=  SEQUENCE  {
  //       algorithm            AlgorithmIdentifier,
  //       subjectPublicKey     BIT STRING  }
  ScopedSECItem subjectPublicKeyInfo(
    SECKEY_EncodeDERSubjectPublicKeyInfo(subjectPublicKey));
  if (!subjectPublicKeyInfo) {
    return nullptr;
  }
  if (output.Add(subjectPublicKeyInfo.get()) != Success) {
    return nullptr;
  }

  if (extensions) {
    Output extensionsOutput;
    while (*extensions) {
      if (extensionsOutput.Add(*extensions) != Success) {
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
    if (output.Add(extensionsWrapped) != Success) {
      return nullptr;
    }
  }

  return output.Squash(arena, der::SEQUENCE);
}

ByteString
CNToDERName(const char* cn)
{
  // Name ::= CHOICE { -- only one possibility for now --
  //   rdnSequence  RDNSequence }
  //
  // RDNSequence ::= SEQUENCE OF RelativeDistinguishedName
  //
  // RelativeDistinguishedName ::=
  //   SET SIZE (1..MAX) OF AttributeTypeAndValue
  //
  // AttributeTypeAndValue ::= SEQUENCE {
  //   type     AttributeType,
  //   value    AttributeValue }
  //
  // AttributeType ::= OBJECT IDENTIFIER
  //
  // AttributeValue ::= ANY -- DEFINED BY AttributeType
  //
  // DirectoryString ::= CHOICE {
  //       teletexString           TeletexString (SIZE (1..MAX)),
  //       printableString         PrintableString (SIZE (1..MAX)),
  //       universalString         UniversalString (SIZE (1..MAX)),
  //       utf8String              UTF8String (SIZE (1..MAX)),
  //       bmpString               BMPString (SIZE (1..MAX)) }
  //
  // id-at OBJECT IDENTIFIER ::= { joint-iso-ccitt(2) ds(5) 4 }
  // id-at-commonName        AttributeType ::= { id-at 3 }

  // python DottedOIDToCode.py --tlv id-at-commonName 2.5.4.3
  static const uint8_t tlv_id_at_commonName[] = {
    0x06, 0x03, 0x55, 0x04, 0x03
  };

  ByteString value(reinterpret_cast<const ByteString::value_type*>(cn));
  value = TLV(der::UTF8String, value);

  ByteString ava;
  ava.append(tlv_id_at_commonName, sizeof(tlv_id_at_commonName));
  ava.append(value);
  ava = TLV(der::SEQUENCE, ava);
  if (ava == ENCODING_FAILED) {
    return ENCODING_FAILED;
  }

  ByteString rdn(TLV(der::SET, ava));
  if (rdn == ENCODING_FAILED) {
    return ENCODING_FAILED;
  }

  return TLV(der::SEQUENCE, rdn);
}

ByteString
CreateEncodedSerialNumber(long serialNumberValue)
{
  return Integer(serialNumberValue);
}

// BasicConstraints ::= SEQUENCE {
//         cA                      BOOLEAN DEFAULT FALSE,
//         pathLenConstraint       INTEGER (0..MAX) OPTIONAL }
SECItem*
CreateEncodedBasicConstraints(PLArenaPool* arena, bool isCA,
                              /*optional*/ long* pathLenConstraintValue,
                              ExtensionCriticality criticality)
{
  assert(arena);
  if (!arena) {
    return nullptr;
  }

  Output value;

  if (isCA) {
    ByteString cA(Boolean(true));
    if (cA == ENCODING_FAILED) {
      return nullptr;
    }
    value.Add(cA);
  }

  if (pathLenConstraintValue) {
    ByteString pathLenConstraint(Integer(*pathLenConstraintValue));
    if (pathLenConstraint == ENCODING_FAILED) {
      return nullptr;
    }
    value.Add(pathLenConstraint);
  }

  // python DottedOIDToCode.py --tlv id-ce-basicConstraints 2.5.29.19
  static const uint8_t tlv_id_ce_basicConstraints[] = {
    0x06, 0x03, 0x55, 0x1d, 0x13
  };

  return Extension(arena, Input(tlv_id_ce_basicConstraints), criticality, value);
}

// ExtKeyUsageSyntax ::= SEQUENCE SIZE (1..MAX) OF KeyPurposeId
// KeyPurposeId ::= OBJECT IDENTIFIER
SECItem*
CreateEncodedEKUExtension(PLArenaPool* arena, Input ekuOID,
                          ExtensionCriticality criticality)
{
  assert(arena);

  Output value;
  SECItem ekuOIDItem = UnsafeMapInputToSECItem(ekuOID);
  if (value.Add(&ekuOIDItem) != Success) {
    return nullptr;
  }

  // python DottedOIDToCode.py --tlv id-ce-extKeyUsage 2.5.29.37
  static const uint8_t tlv_id_ce_extKeyUsage[] = {
    0x06, 0x03, 0x55, 0x1d, 0x25
  };

  return Extension(arena, Input(tlv_id_ce_extKeyUsage), criticality, value);
}

///////////////////////////////////////////////////////////////////////////////
// OCSP responses

SECItem*
CreateEncodedOCSPResponse(OCSPResponseContext& context)
{
  if (!context.arena) {
    return nullptr;
  }

  if (!context.skipResponseBytes) {
    if (!context.signerPrivateKey) {
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
  ByteString reponseStatusValue;
  reponseStatusValue.push_back(context.responseStatus);
  ByteString responseStatus(TLV(der::ENUMERATED, reponseStatusValue));
  if (responseStatus == ENCODING_FAILED) {
    return nullptr;
  }

  ByteString responseBytesNested;
  if (!context.skipResponseBytes) {
    ByteString responseBytes(ResponseBytes(context));
    if (responseBytes == ENCODING_FAILED) {
      return nullptr;
    }

    responseBytesNested = TLV(der::CONSTRUCTED | der::CONTEXT_SPECIFIC,
                              responseBytes);
    if (responseBytesNested == ENCODING_FAILED) {
      return nullptr;
    }
  }

  ByteString value;
  value.append(responseStatus);
  value.append(responseBytesNested);
  ByteString result(TLV(der::SEQUENCE, value));
  if (result == ENCODING_FAILED) {
    return nullptr;
  }

  MaybeLogOutput(result, "ocsp");

  return ArenaDupByteString(context.arena, result);
}

// ResponseBytes ::= SEQUENCE {
//    responseType            OBJECT IDENTIFIER,
//    response                OCTET STRING }
ByteString
ResponseBytes(OCSPResponseContext& context)
{
  // Includes tag and length
  static const uint8_t id_pkix_ocsp_basic_encoded[] = {
    0x06, 0x09, 0x2B, 0x06, 0x01, 0x05, 0x05, 0x07, 0x30, 0x01, 0x01
  };
  ByteString response(BasicOCSPResponse(context));
  if (response == ENCODING_FAILED) {
    return nullptr;
  }
  ByteString responseNested = TLV(der::OCTET_STRING, response);
  if (responseNested == ENCODING_FAILED) {
    return nullptr;
  }

  ByteString value;
  value.append(id_pkix_ocsp_basic_encoded,
               sizeof(id_pkix_ocsp_basic_encoded));
  value.append(responseNested);
  return TLV(der::SEQUENCE, value);
}

// BasicOCSPResponse ::= SEQUENCE {
//   tbsResponseData          ResponseData,
//   signatureAlgorithm       AlgorithmIdentifier,
//   signature                BIT STRING,
//   certs                [0] EXPLICIT SEQUENCE OF Certificate OPTIONAL }
ByteString
BasicOCSPResponse(OCSPResponseContext& context)
{
  SECItem* tbsResponseData = ResponseData(context);
  if (!tbsResponseData) {
    return nullptr;
  }

  // TODO(bug 980538): certs
  return SignedData(tbsResponseData, context.signerPrivateKey.get(),
                    SignatureAlgorithm::rsa_pkcs1_with_sha256,
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
  if (output.Add(&extension->id) != Success) {
    return nullptr;
  }
  if (extension->critical) {
    static const uint8_t trueEncoded[3] = { 0x01, 0x01, 0xFF };
    SECItem critical = {
      siBuffer,
      const_cast<uint8_t*>(trueEncoded),
      sizeof(trueEncoded)
    };
    if (output.Add(&critical) != Success) {
      return nullptr;
    }
  }
  SECItem* value = EncodeNested(context.arena, der::OCTET_STRING,
                                &extension->value);
  if (!value) {
    return nullptr;
  }
  if (output.Add(value) != Success) {
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
    if (output.Add(extensionEncoded) != Success) {
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
  ByteString responderID(ResponderID(context));
  if (responderID == ENCODING_FAILED) {
    return nullptr;
  }
  ByteString producedAtEncoded(TimeToGeneralizedTime(context.producedAt));
  if (producedAtEncoded == ENCODING_FAILED) {
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
  output.Add(responderID);
  output.Add(producedAtEncoded);
  if (output.Add(responsesNested) != Success) {
    return nullptr;
  }
  if (responseExtensions) {
    if (output.Add(responseExtensions) != Success) {
      return nullptr;
    }
  }
  return output.Squash(context.arena, der::SEQUENCE);
}

// ResponderID ::= CHOICE {
//    byName              [1] Name,
//    byKey               [2] KeyHash }
// }
ByteString
ResponderID(OCSPResponseContext& context)
{
  ByteString contents;
  uint8_t responderIDType;
  if (!context.signerNameDER.empty()) {
    contents = context.signerNameDER;
    responderIDType = 1; // byName
  } else {
    contents = KeyHash(context);
    if (contents == ENCODING_FAILED) {
      return ENCODING_FAILED;
    }
    responderIDType = 2; // byKey
  }

  return TLV(der::CONSTRUCTED | der::CONTEXT_SPECIFIC | responderIDType,
             contents);
}

// KeyHash ::= OCTET STRING -- SHA-1 hash of responder's public key
//                          -- (i.e., the SHA-1 hash of the value of the
//                          -- BIT STRING subjectPublicKey [excluding
//                          -- the tag, length, and number of unused
//                          -- bits] in the responder's certificate)
ByteString
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
  return KeyHashHelper(signerSPKI.get());
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
  ByteString certStatus(CertStatus(context));
  if (certStatus == ENCODING_FAILED) {
    return nullptr;
  }
  ByteString thisUpdateEncoded(TimeToGeneralizedTime(context.thisUpdate));
  if (thisUpdateEncoded == ENCODING_FAILED) {
    return nullptr;
  }
  ByteString nextUpdateEncodedNested;
  if (context.includeNextUpdate) {
    ByteString nextUpdateEncoded(TimeToGeneralizedTime(context.nextUpdate));
    if (nextUpdateEncoded == ENCODING_FAILED) {
      return nullptr;
    }
    nextUpdateEncodedNested = TLV(der::CONSTRUCTED | der::CONTEXT_SPECIFIC | 0,
                                  nextUpdateEncoded);
    if (nextUpdateEncodedNested == ENCODING_FAILED) {
      return nullptr;
    }
  }

  Output output;
  if (output.Add(certID) != Success) {
    return nullptr;
  }
  output.Add(certStatus);
  output.Add(thisUpdateEncoded);
  if (!nextUpdateEncodedNested.empty()) {
    output.Add(nextUpdateEncodedNested);
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
  SECItem issuerSECItem = UnsafeMapInputToSECItem(context.certID.issuer);
  ByteString issuerNameHash(HashedOctetString(issuerSECItem));
  if (issuerNameHash == ENCODING_FAILED) {
    return nullptr;
  }

  SECItem issuerSubjectPublicKeyInfoSECItem =
    UnsafeMapInputToSECItem(context.certID.issuerSubjectPublicKeyInfo);
  ScopedPtr<CERTSubjectPublicKeyInfo, SECKEY_DestroySubjectPublicKeyInfo>
    spki(SECKEY_DecodeDERSubjectPublicKeyInfo(
           &issuerSubjectPublicKeyInfoSECItem));
  if (!spki) {
    return nullptr;
  }
  ByteString issuerKeyHash(KeyHashHelper(spki.get()));
  if (issuerKeyHash == ENCODING_FAILED) {
    return nullptr;
  }

  ByteString serialNumberValue(context.certID.serialNumber.UnsafeGetData(),
                               context.certID.serialNumber.GetLength());
  ByteString serialNumber(TLV(der::INTEGER, serialNumberValue));
  if (serialNumber == ENCODING_FAILED) {
    return nullptr;
  }

  Output output;

  // python DottedOIDToCode.py --alg id-sha1 1.3.14.3.2.26
  static const uint8_t alg_id_sha1[] = {
    0x30, 0x07, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a
  };
  static const SECItem id_sha1 = {
    siBuffer,
    const_cast<uint8_t*>(alg_id_sha1),
    sizeof(alg_id_sha1)
  };

  if (output.Add(&id_sha1) != Success) {
    return nullptr;
  }
  output.Add(issuerNameHash);
  output.Add(issuerKeyHash);
  output.Add(serialNumber);
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
ByteString
CertStatus(OCSPResponseContext& context)
{
  switch (context.certStatus) {
    // Both good and unknown are ultimately represented as NULL - the only
    // difference is in the tag that identifies them.
    case 0:
    case 2:
    {
      return TLV(der::CONTEXT_SPECIFIC | context.certStatus, ByteString());
    }
    case 1:
    {
      ByteString revocationTime(TimeToGeneralizedTime(context.revocationTime));
      if (revocationTime == ENCODING_FAILED) {
        return ENCODING_FAILED;
      }
      // TODO(bug 980536): add support for revocationReason
      return TLV(der::CONTEXT_SPECIFIC | der::CONSTRUCTED | 1, revocationTime);
    }
    default:
      assert(false);
      // fall through
  }
  return ENCODING_FAILED;
}

} } } // namespace mozilla::pkix::test
