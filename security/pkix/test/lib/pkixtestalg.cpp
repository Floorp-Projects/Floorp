/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2015 Mozilla Contributors
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

#include "pkixder.h"

// python DottedOIDToCode.py --prefixdefine PREFIX_1_2_840_10040 1.2.840.10040
#define PREFIX_1_2_840_10040 0x2a, 0x86, 0x48, 0xce, 0x38

// python DottedOIDToCode.py --prefixdefine PREFIX_1_2_840_10045 1.2.840.10045
#define PREFIX_1_2_840_10045 0x2a, 0x86, 0x48, 0xce, 0x3d

// python DottedOIDToCode.py --prefixdefine PREFIX_1_2_840_113549 1.2.840.113549
#define PREFIX_1_2_840_113549 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d

namespace mozilla { namespace pkix { namespace test {

namespace {

enum class NULLParam { NO, YES };

template <size_t SIZE>
ByteString
OID(const uint8_t (&rawValue)[SIZE])
{
  return TLV(der::OIDTag, ByteString(rawValue, SIZE));
}

template <size_t SIZE>
ByteString
SimpleAlgID(const uint8_t (&rawValue)[SIZE],
            NULLParam nullParam = NULLParam::NO)
{
  ByteString sequenceValue(OID(rawValue));
  if (nullParam == NULLParam::YES) {
    sequenceValue.append(TLV(der::NULLTag, ByteString()));
  }
  return TLV(der::SEQUENCE, sequenceValue);
}

} // unnamed namespace

TestSignatureAlgorithm::TestSignatureAlgorithm(
  const TestPublicKeyAlgorithm& publicKeyAlg,
  TestDigestAlgorithmID digestAlg,
  const ByteString& algorithmIdentifier,
  bool accepted)
  : publicKeyAlg(publicKeyAlg)
  , digestAlg(digestAlg)
  , algorithmIdentifier(algorithmIdentifier)
  , accepted(accepted)
{
}

// RFC 3279 Section 2.3.1
TestPublicKeyAlgorithm
RSA_PKCS1()
{
  static const uint8_t rsaEncryption[] = { PREFIX_1_2_840_113549, 1, 1, 1 };
  return TestPublicKeyAlgorithm(SimpleAlgID(rsaEncryption, NULLParam::YES));
}

// RFC 3279 Section 2.2.1
TestSignatureAlgorithm md2WithRSAEncryption()
{
  static const uint8_t oidValue[] = { PREFIX_1_2_840_113549, 1, 1, 2 };
  return TestSignatureAlgorithm(RSA_PKCS1(), TestDigestAlgorithmID::MD2,
                                SimpleAlgID(oidValue), false);
}

// RFC 3279 Section 2.2.1
TestSignatureAlgorithm md5WithRSAEncryption()
{
  static const uint8_t oidValue[] = { PREFIX_1_2_840_113549, 1, 1, 4 };
  return TestSignatureAlgorithm(RSA_PKCS1(), TestDigestAlgorithmID::MD5,
                                SimpleAlgID(oidValue), false);
}

// RFC 3279 Section 2.2.1
TestSignatureAlgorithm sha1WithRSAEncryption()
{
  static const uint8_t oidValue[] = { PREFIX_1_2_840_113549, 1, 1, 5 };
  return TestSignatureAlgorithm(RSA_PKCS1(), TestDigestAlgorithmID::SHA1,
                                SimpleAlgID(oidValue), true);
}

// RFC 4055 Section 5
TestSignatureAlgorithm sha256WithRSAEncryption()
{
  static const uint8_t oidValue[] = { PREFIX_1_2_840_113549, 1, 1, 11 };
  return TestSignatureAlgorithm(RSA_PKCS1(), TestDigestAlgorithmID::SHA256,
                                SimpleAlgID(oidValue), true);
}

} } } // namespace mozilla::pkix
