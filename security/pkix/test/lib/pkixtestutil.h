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

#ifndef mozilla_pkix_test__pkixtestutils_h
#define mozilla_pkix_test__pkixtestutils_h

#include <ctime>
#include <stdint.h> // Some Mozilla-supported compilers lack <cstdint>
#include <string>

#include "pkix/enumclass.h"
#include "pkix/pkixtypes.h"
#include "pkix/ScopedPtr.h"

static const unsigned int MINIMUM_TEST_KEY_BITS = 1024;

namespace mozilla { namespace pkix { namespace test {

typedef std::basic_string<uint8_t> ByteString;

inline bool ENCODING_FAILED(const ByteString& bs) { return bs.empty(); }

// XXX: Ideally, we should define this instead:
//
//   template <typename T, std::size_t N>
//   constexpr inline std::size_t
//   ArrayLength(T (&)[N])
//   {
//     return N;
//   }
//
// However, we don't because not all supported compilers support constexpr,
// and we need to calculate array lengths in static_assert sometimes.
//
// XXX: Evaluates its argument twice
#define MOZILLA_PKIX_ARRAY_LENGTH(x) (sizeof(x) / sizeof((x)[0]))

class TestInput : public Input
{
public:
  template <size_t N>
  explicit TestInput(const char (&valueString)[N])
    : Input(reinterpret_cast<const uint8_t(&)[N-1]>(valueString))
  {
  }
};

bool InputEqualsByteString(Input input, const ByteString& bs);

// python DottedOIDToCode.py --tlv id-kp-OCSPSigning 1.3.6.1.5.5.7.3.9
static const uint8_t tlv_id_kp_OCSPSigning[] = {
  0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x09
};

// python DottedOIDToCode.py --tlv id-kp-serverAuth 1.3.6.1.5.5.7.3.1
static const uint8_t tlv_id_kp_serverAuth[] = {
  0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x01
};

// python DottedOIDToCode.py --alg sha256WithRSAEncryption 1.2.840.113549.1.1.11
const uint8_t alg_sha256WithRSAEncryption[] = {
  0x30, 0x0b, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b
};

const ByteString sha256WithRSAEncryption(alg_sha256WithRSAEncryption,
  MOZILLA_PKIX_ARRAY_LENGTH(alg_sha256WithRSAEncryption));

// python DottedOIDToCode.py --alg md5WithRSAEncryption 1.2.840.113549.1.1.4
const uint8_t alg_md5WithRSAEncryption[] = {
  0x30, 0x0b, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x04
};

const ByteString md5WithRSAEncryption(alg_md5WithRSAEncryption,
  MOZILLA_PKIX_ARRAY_LENGTH(alg_md5WithRSAEncryption));

// python DottedOIDToCode.py --alg md2WithRSAEncryption 1.2.840.113549.1.1.2
const uint8_t alg_md2WithRSAEncryption[] = {
  0x30, 0x0b, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x02
};

const ByteString md2WithRSAEncryption(alg_md2WithRSAEncryption,
  MOZILLA_PKIX_ARRAY_LENGTH(alg_md2WithRSAEncryption));

// e.g. YMDHMS(2016, 12, 31, 1, 23, 45) => 2016-12-31:01:23:45 (GMT)
mozilla::pkix::Time YMDHMS(int16_t year, int16_t month, int16_t day,
                           int16_t hour, int16_t minutes, int16_t seconds);

// e.g. YMDHMS(2016, 12, 31, 1, 23, 45) => 2016-12-31:01:23:45 (GMT)
mozilla::pkix::Time YMDHMS(int16_t year, int16_t month, int16_t day,
                           int16_t hour, int16_t minutes, int16_t seconds);

ByteString TLV(uint8_t tag, const ByteString& value);

ByteString CN(const ByteString&);

inline ByteString
CN(const char* value)
{
  return CN(ByteString(reinterpret_cast<const uint8_t*>(value),
                       std::strlen(value)));
}

ByteString OU(const ByteString&);

inline ByteString
OU(const char* value)
{
  return OU(ByteString(reinterpret_cast<const uint8_t*>(value),
                       std::strlen(value)));
}

// RelativeDistinguishedName ::=
//   SET SIZE (1..MAX) OF AttributeTypeAndValue
//
ByteString RDN(const ByteString& avas);

// Name ::= CHOICE { -- only one possibility for now --
//   rdnSequence  RDNSequence }
//
// RDNSequence ::= SEQUENCE OF RelativeDistinguishedName
//
ByteString Name(const ByteString& rdns);

inline ByteString
CNToDERName(const ByteString& cn)
{
  return Name(RDN(CN(cn)));
}

inline ByteString
CNToDERName(const char* cn)
{
  return Name(RDN(CN(cn)));
}

// GeneralName ::= CHOICE {
//      otherName                       [0]     OtherName,
//      rfc822Name                      [1]     IA5String,
//      dNSName                         [2]     IA5String,
//      x400Address                     [3]     ORAddress,
//      directoryName                   [4]     Name,
//      ediPartyName                    [5]     EDIPartyName,
//      uniformResourceIdentifier       [6]     IA5String,
//      iPAddress                       [7]     OCTET STRING,
//      registeredID                    [8]     OBJECT IDENTIFIER }

inline ByteString
RFC822Name(const ByteString& name)
{
  // (2 << 6) means "context-specific", 1 is the GeneralName tag.
  return TLV((2 << 6) | 1, name);
}

template <size_t L>
inline ByteString
RFC822Name(const char (&bytes)[L])
{
  return RFC822Name(ByteString(reinterpret_cast<const uint8_t (&)[L]>(bytes),
                               L - 1));
}

inline ByteString
DNSName(const ByteString& name)
{
  // (2 << 6) means "context-specific", 2 is the GeneralName tag.
  return TLV((2 << 6) | 2, name);
}

template <size_t L>
inline ByteString
DNSName(const char (&bytes)[L])
{
  return DNSName(ByteString(reinterpret_cast<const uint8_t (&)[L]>(bytes),
                            L - 1));
}

template <size_t L>
inline ByteString
IPAddress(const uint8_t (&bytes)[L])
{
  // (2 << 6) means "context-specific", 7 is the GeneralName tag.
  return TLV((2 << 6) | 7, ByteString(bytes, L));
}

// Names should be zero or more GeneralNames, like DNSName and IPAddress return,
// concatenated together.
//
// CreatedEncodedSubjectAltName(ByteString()) results in a SAN with an empty
// sequence. CreateEmptyEncodedSubjectName() results in a SAN without any
// sequence.
ByteString CreateEncodedSubjectAltName(const ByteString& names);
ByteString CreateEncodedEmptySubjectAltName();

class TestKeyPair
{
public:
  virtual ~TestKeyPair() { }

  // The DER encoding of the entire SubjectPublicKeyInfo structure. This is
  // what is encoded in certificates.
  const ByteString subjectPublicKeyInfo;

  // The DER encoding of subjectPublicKeyInfo.subjectPublicKey. This is what is
  // hashed to create CertIDs for OCSP.
  const ByteString subjectPublicKey;

  virtual Result SignData(const ByteString& tbs,
                          const ByteString& signatureAlgorithm,
                          /*out*/ ByteString& signature) const = 0;

  virtual TestKeyPair* Clone() const = 0;
protected:
  TestKeyPair(const ByteString& spki, const ByteString& spk)
    : subjectPublicKeyInfo(spki)
    , subjectPublicKey(spk)
  {
  }

  TestKeyPair(const TestKeyPair&) /*= delete*/;
  void operator=(const TestKeyPair&) /*= delete*/;
};

TestKeyPair* CloneReusedKeyPair();
TestKeyPair* GenerateKeyPair();
inline void DeleteTestKeyPair(TestKeyPair* keyPair) { delete keyPair; }
typedef ScopedPtr<TestKeyPair, DeleteTestKeyPair> ScopedTestKeyPair;

ByteString SHA1(const ByteString& toHash);

Result TestCheckPublicKey(Input subjectPublicKeyInfo);
Result TestVerifySignedData(const SignedDataWithSignature& signedData,
                            Input subjectPublicKeyInfo);
Result TestDigestBuf(Input item, /*out*/ uint8_t* digestBuf,
                     size_t digestBufLen);

// Replace one substring in item with another of the same length, but only if
// the substring was found exactly once. The "same length" restriction is
// useful for avoiding invalidating lengths encoded within the item. The
// "only once" restriction is helpful for avoiding making accidental changes.
//
// The string to search for must be 8 or more bytes long so that it is
// extremely unlikely that there will ever be any false positive matches
// in digital signatures, keys, hashes, etc.
Result TamperOnce(/*in/out*/ ByteString& item, const ByteString& from,
                  const ByteString& to);

///////////////////////////////////////////////////////////////////////////////
// Encode Certificates

enum Version { v1 = 0, v2 = 1, v3 = 2 };

// signature is assumed to be the DER encoding of an AlgorithmIdentifer. It is
// put into the signature field of the TBSCertificate. In most cases, it will
// be the same as signatureAlgorithm, which is the algorithm actually used
// to sign the certificate.
// serialNumber is assumed to be the DER encoding of an INTEGER.
//
// If extensions is null, then no extensions will be encoded. Otherwise,
// extensions must point to an array of ByteStrings, terminated with an empty
// ByteString. (If the first item of the array is empty then an empty
// Extensions sequence will be encoded.)
ByteString CreateEncodedCertificate(long version, const ByteString& signature,
                                    const ByteString& serialNumber,
                                    const ByteString& issuerNameDER,
                                    time_t notBefore, time_t notAfter,
                                    const ByteString& subjectNameDER,
                                    const TestKeyPair& subjectKeyPair,
                                    /*optional*/ const ByteString* extensions,
                                    const TestKeyPair& issuerKeyPair,
                                    const ByteString& signatureAlgorithm);

ByteString CreateEncodedSerialNumber(long value);

MOZILLA_PKIX_ENUM_CLASS ExtensionCriticality { NotCritical = 0, Critical = 1 };

ByteString CreateEncodedBasicConstraints(bool isCA,
                                         /*optional*/ long* pathLenConstraint,
                                         ExtensionCriticality criticality);

// Creates a DER-encoded extKeyUsage extension with one EKU OID.
ByteString CreateEncodedEKUExtension(Input eku,
                                     ExtensionCriticality criticality);

///////////////////////////////////////////////////////////////////////////////
// Encode OCSP responses

class OCSPResponseExtension
{
public:
  ByteString id;
  bool critical;
  ByteString value;
  OCSPResponseExtension* next;
};

class OCSPResponseContext
{
public:
  OCSPResponseContext(const CertID& certID, std::time_t time);

  const CertID& certID;
  // TODO(bug 980538): add a way to specify what certificates are included.

  // The fields below are in the order that they appear in an OCSP response.

  enum OCSPResponseStatus {
    successful = 0,
    malformedRequest = 1,
    internalError = 2,
    tryLater = 3,
    // 4 is not used
    sigRequired = 5,
    unauthorized = 6,
  };
  uint8_t responseStatus; // an OCSPResponseStatus or an invalid value
  bool skipResponseBytes; // If true, don't include responseBytes

  // responderID
  ByteString signerNameDER; // If set, responderID will use the byName
                            // form; otherwise responderID will use the
                            // byKeyHash form.

  std::time_t producedAt;

  OCSPResponseExtension* extensions;
  bool includeEmptyExtensions; // If true, include the extension wrapper
                               // regardless of if there are any actual
                               // extensions.
  ScopedTestKeyPair signerKeyPair;
  ByteString signatureAlgorithm; // DER encoding of signature algorithm to use.
  bool badSignature; // If true, alter the signature to fail verification
  const ByteString* certs; // optional; array terminated by an empty string

  // The following fields are on a per-SingleResponse basis. In the future we
  // may support including multiple SingleResponses per response.
  enum CertStatus {
    good = 0,
    revoked = 1,
    unknown = 2,
  };
  uint8_t certStatus; // CertStatus or an invalid value
  std::time_t revocationTime; // For certStatus == revoked
  std::time_t thisUpdate;
  std::time_t nextUpdate;
  bool includeNextUpdate;
};

ByteString CreateEncodedOCSPResponse(OCSPResponseContext& context);

} } } // namespace mozilla::pkix::test

#endif // mozilla_pkix_test__pkixtestutils_h
