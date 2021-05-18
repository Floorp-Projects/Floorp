/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This code is made available to you under your choice of the following sets
 * of licensing terms:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/* Copyright 2016 Mozilla Contributors
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

#include "pkixgtest.h"

#include "mozpkix/pkixder.h"
#include "mozpkix/pkixutil.h"

using namespace mozilla::pkix;
using namespace mozilla::pkix::test;

namespace mozilla { namespace pkix {

extern Result CheckExtendedKeyUsage(EndEntityOrCA endEntityOrCA,
                                    const Input* encodedExtendedKeyUsage,
                                    KeyPurposeId requiredEKU,
                                    TrustDomain& trustDomain, Time notBefore);

} } // namespace mozilla::pkix

class pkixcheck_CheckExtendedKeyUsage : public ::testing::Test
{
protected:
  DefaultCryptoTrustDomain mTrustDomain;
};

#define ASSERT_BAD(x) ASSERT_EQ(Result::ERROR_INADEQUATE_CERT_TYPE, x)

// tlv_id_kp_OCSPSigning and tlv_id_kp_serverAuth are defined in pkixtestutil.h

// tlv_id_kp_clientAuth and tlv_id_kp_codeSigning are defined in pkixgtest.h

// python DottedOIDToCode.py --tlv id_kp_emailProtection 1.3.6.1.5.5.7.3.4
static const uint8_t tlv_id_kp_emailProtection[] = {
  0x06, 0x08, 0x2b, 0x06, 0x01, 0x05, 0x05, 0x07, 0x03, 0x04
};

// python DottedOIDToCode.py --tlv id-Netscape-stepUp 2.16.840.1.113730.4.1
static const uint8_t tlv_id_Netscape_stepUp[] = {
  0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x86, 0xf8, 0x42, 0x04, 0x01
};

// python DottedOIDToCode.py --tlv unknownOID 1.3.6.1.4.1.13769.666.666.666.1.500.9.3
static const uint8_t tlv_unknownOID[] = {
  0x06, 0x12, 0x2b, 0x06, 0x01, 0x04, 0x01, 0xeb, 0x49, 0x85, 0x1a, 0x85, 0x1a,
  0x85, 0x1a, 0x01, 0x83, 0x74, 0x09, 0x03
};

// python DottedOIDToCode.py --tlv anyExtendedKeyUsage 2.5.29.37.0
static const uint8_t tlv_anyExtendedKeyUsage[] = {
  0x06, 0x04, 0x55, 0x1d, 0x25, 0x00
};

TEST_F(pkixcheck_CheckExtendedKeyUsage, none)
{
  // The input Input is nullptr. This means the cert had no extended key usage
  // extension. This is always valid except for when the certificate is an
  // end-entity and the required usage is id-kp-OCSPSigning.

  ASSERT_EQ(Success, CheckExtendedKeyUsage(EndEntityOrCA::MustBeEndEntity,
                                           nullptr,
                                           KeyPurposeId::anyExtendedKeyUsage,
                                           mTrustDomain, Now()));
  ASSERT_EQ(Success, CheckExtendedKeyUsage(EndEntityOrCA::MustBeCA, nullptr,
                                           KeyPurposeId::anyExtendedKeyUsage,
                                           mTrustDomain, Now()));
  ASSERT_EQ(Success, CheckExtendedKeyUsage(EndEntityOrCA::MustBeEndEntity,
                                           nullptr,
                                           KeyPurposeId::id_kp_serverAuth,
                                           mTrustDomain, Now()));
  ASSERT_EQ(Success, CheckExtendedKeyUsage(EndEntityOrCA::MustBeCA, nullptr,
                                           KeyPurposeId::id_kp_serverAuth,
                                           mTrustDomain, Now()));
  ASSERT_EQ(Success, CheckExtendedKeyUsage(EndEntityOrCA::MustBeEndEntity,
                                           nullptr,
                                           KeyPurposeId::id_kp_clientAuth,
                                           mTrustDomain, Now()));
  ASSERT_EQ(Success, CheckExtendedKeyUsage(EndEntityOrCA::MustBeCA, nullptr,
                                           KeyPurposeId::id_kp_clientAuth,
                                           mTrustDomain, Now()));
  ASSERT_EQ(Success, CheckExtendedKeyUsage(EndEntityOrCA::MustBeEndEntity,
                                           nullptr,
                                           KeyPurposeId::id_kp_codeSigning,
                                           mTrustDomain, Now()));
  ASSERT_EQ(Success, CheckExtendedKeyUsage(EndEntityOrCA::MustBeCA, nullptr,
                                           KeyPurposeId::id_kp_codeSigning,
                                           mTrustDomain, Now()));
  ASSERT_EQ(Success, CheckExtendedKeyUsage(EndEntityOrCA::MustBeEndEntity,
                                           nullptr,
                                           KeyPurposeId::id_kp_emailProtection,
                                           mTrustDomain, Now()));
  ASSERT_EQ(Success, CheckExtendedKeyUsage(EndEntityOrCA::MustBeCA, nullptr,
                                           KeyPurposeId::id_kp_emailProtection,
                                           mTrustDomain, Now()));
  ASSERT_BAD(CheckExtendedKeyUsage(EndEntityOrCA::MustBeEndEntity, nullptr,
                                   KeyPurposeId::id_kp_OCSPSigning,
                                   mTrustDomain, Now()));
  ASSERT_EQ(Success, CheckExtendedKeyUsage(EndEntityOrCA::MustBeCA, nullptr,
                                           KeyPurposeId::id_kp_OCSPSigning,
                                           mTrustDomain, Now()));
}

static const Input empty_null;

TEST_F(pkixcheck_CheckExtendedKeyUsage, empty)
{
  // The input Input is empty. The cert has an empty extended key usage
  // extension, which is syntactically invalid.
  ASSERT_BAD(CheckExtendedKeyUsage(EndEntityOrCA::MustBeEndEntity, &empty_null,
                                   KeyPurposeId::id_kp_serverAuth,
                                   mTrustDomain, Now()));
  ASSERT_BAD(CheckExtendedKeyUsage(EndEntityOrCA::MustBeCA, &empty_null,
                                   KeyPurposeId::id_kp_serverAuth,
                                   mTrustDomain, Now()));

  static const uint8_t dummy = 0x00;
  Input empty_nonnull;
  ASSERT_EQ(Success, empty_nonnull.Init(&dummy, 0));
  ASSERT_BAD(CheckExtendedKeyUsage(EndEntityOrCA::MustBeEndEntity, &empty_nonnull,
                                   KeyPurposeId::id_kp_serverAuth,
                                   mTrustDomain, Now()));
  ASSERT_BAD(CheckExtendedKeyUsage(EndEntityOrCA::MustBeCA, &empty_nonnull,
                                   KeyPurposeId::id_kp_serverAuth,
                                   mTrustDomain, Now()));
}

struct EKUTestcase
{
  ByteString ekuSEQUENCE;
  KeyPurposeId keyPurposeId;
  Result expectedResultEndEntity;
  Result expectedResultCA;
};

::std::ostream& operator<<(::std::ostream& os, const EKUTestcase&)
{
  return os << "TODO (bug 1318770)";
}

class CheckExtendedKeyUsageTest
  : public ::testing::Test
  , public ::testing::WithParamInterface<EKUTestcase>
{
protected:
  DefaultCryptoTrustDomain mTrustDomain;
};

TEST_P(CheckExtendedKeyUsageTest, EKUTestcase)
{
  const EKUTestcase& param(GetParam());
  Input encodedEKU;
  ASSERT_EQ(Success, encodedEKU.Init(param.ekuSEQUENCE.data(),
                                     param.ekuSEQUENCE.length()));
  ASSERT_EQ(param.expectedResultEndEntity,
            CheckExtendedKeyUsage(EndEntityOrCA::MustBeEndEntity, &encodedEKU,
                                  param.keyPurposeId,
                                  mTrustDomain, Now()));
  ASSERT_EQ(param.expectedResultCA,
            CheckExtendedKeyUsage(EndEntityOrCA::MustBeCA, &encodedEKU,
                                  param.keyPurposeId,
                                  mTrustDomain, Now()));
}

#define SINGLE_EKU_SUCCESS(oidBytes, keyPurposeId) \
  { TLV(der::SEQUENCE, BytesToByteString(oidBytes)), keyPurposeId, \
    Success, Success }
#define SINGLE_EKU_SUCCESS_CA(oidBytes, keyPurposeId) \
  { TLV(der::SEQUENCE, BytesToByteString(oidBytes)), keyPurposeId, \
    Result::ERROR_INADEQUATE_CERT_TYPE, Success }
#define SINGLE_EKU_FAILURE(oidBytes, keyPurposeId) \
  { TLV(der::SEQUENCE, BytesToByteString(oidBytes)), keyPurposeId, \
    Result::ERROR_INADEQUATE_CERT_TYPE, Result::ERROR_INADEQUATE_CERT_TYPE }
#define DOUBLE_EKU_SUCCESS(oidBytes1, oidBytes2, keyPurposeId) \
  { TLV(der::SEQUENCE, \
        BytesToByteString(oidBytes1) + BytesToByteString(oidBytes2)), \
    keyPurposeId, \
    Success, Success }
#define DOUBLE_EKU_SUCCESS_CA(oidBytes1, oidBytes2, keyPurposeId) \
  { TLV(der::SEQUENCE, \
        BytesToByteString(oidBytes1) + BytesToByteString(oidBytes2)), \
    keyPurposeId, \
    Result::ERROR_INADEQUATE_CERT_TYPE, Success }
#define DOUBLE_EKU_FAILURE(oidBytes1, oidBytes2, keyPurposeId) \
  { TLV(der::SEQUENCE, \
        BytesToByteString(oidBytes1) + BytesToByteString(oidBytes2)), \
    keyPurposeId, \
    Result::ERROR_INADEQUATE_CERT_TYPE, Result::ERROR_INADEQUATE_CERT_TYPE }

static const EKUTestcase EKU_TESTCASES[] =
{
  SINGLE_EKU_SUCCESS(tlv_id_kp_serverAuth, KeyPurposeId::anyExtendedKeyUsage),
  SINGLE_EKU_SUCCESS(tlv_id_kp_serverAuth, KeyPurposeId::id_kp_serverAuth),
  SINGLE_EKU_FAILURE(tlv_id_kp_serverAuth, KeyPurposeId::id_kp_clientAuth),
  SINGLE_EKU_FAILURE(tlv_id_kp_serverAuth, KeyPurposeId::id_kp_codeSigning),
  SINGLE_EKU_FAILURE(tlv_id_kp_serverAuth, KeyPurposeId::id_kp_emailProtection),
  SINGLE_EKU_FAILURE(tlv_id_kp_serverAuth, KeyPurposeId::id_kp_OCSPSigning),

  SINGLE_EKU_SUCCESS(tlv_id_kp_clientAuth, KeyPurposeId::anyExtendedKeyUsage),
  SINGLE_EKU_FAILURE(tlv_id_kp_clientAuth, KeyPurposeId::id_kp_serverAuth),
  SINGLE_EKU_SUCCESS(tlv_id_kp_clientAuth, KeyPurposeId::id_kp_clientAuth),
  SINGLE_EKU_FAILURE(tlv_id_kp_clientAuth, KeyPurposeId::id_kp_codeSigning),
  SINGLE_EKU_FAILURE(tlv_id_kp_clientAuth, KeyPurposeId::id_kp_emailProtection),
  SINGLE_EKU_FAILURE(tlv_id_kp_clientAuth, KeyPurposeId::id_kp_OCSPSigning),

  SINGLE_EKU_SUCCESS(tlv_id_kp_codeSigning, KeyPurposeId::anyExtendedKeyUsage),
  SINGLE_EKU_FAILURE(tlv_id_kp_codeSigning, KeyPurposeId::id_kp_serverAuth),
  SINGLE_EKU_FAILURE(tlv_id_kp_codeSigning, KeyPurposeId::id_kp_clientAuth),
  SINGLE_EKU_SUCCESS(tlv_id_kp_codeSigning, KeyPurposeId::id_kp_codeSigning),
  SINGLE_EKU_FAILURE(tlv_id_kp_codeSigning, KeyPurposeId::id_kp_emailProtection),
  SINGLE_EKU_FAILURE(tlv_id_kp_codeSigning, KeyPurposeId::id_kp_OCSPSigning),

  SINGLE_EKU_SUCCESS(tlv_id_kp_emailProtection, KeyPurposeId::anyExtendedKeyUsage),
  SINGLE_EKU_FAILURE(tlv_id_kp_emailProtection, KeyPurposeId::id_kp_serverAuth),
  SINGLE_EKU_FAILURE(tlv_id_kp_emailProtection, KeyPurposeId::id_kp_clientAuth),
  SINGLE_EKU_FAILURE(tlv_id_kp_emailProtection, KeyPurposeId::id_kp_codeSigning),
  SINGLE_EKU_SUCCESS(tlv_id_kp_emailProtection, KeyPurposeId::id_kp_emailProtection),
  SINGLE_EKU_FAILURE(tlv_id_kp_emailProtection, KeyPurposeId::id_kp_OCSPSigning),

  // For end-entities, if id-kp-OCSPSigning is present, no usage is allowed
  // except OCSPSigning.
  SINGLE_EKU_SUCCESS_CA(tlv_id_kp_OCSPSigning, KeyPurposeId::anyExtendedKeyUsage),
  SINGLE_EKU_FAILURE(tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_serverAuth),
  SINGLE_EKU_FAILURE(tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_clientAuth),
  SINGLE_EKU_FAILURE(tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_codeSigning),
  SINGLE_EKU_FAILURE(tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_emailProtection),
  SINGLE_EKU_SUCCESS(tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_OCSPSigning),

  SINGLE_EKU_SUCCESS(tlv_id_Netscape_stepUp, KeyPurposeId::anyExtendedKeyUsage),
  // For compatibility, id-Netscape-stepUp is treated as equivalent to
  // id-kp-serverAuth for CAs.
  SINGLE_EKU_SUCCESS_CA(tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_serverAuth),
  SINGLE_EKU_FAILURE(tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_clientAuth),
  SINGLE_EKU_FAILURE(tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_codeSigning),
  SINGLE_EKU_FAILURE(tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_emailProtection),
  SINGLE_EKU_FAILURE(tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_OCSPSigning),

  SINGLE_EKU_SUCCESS(tlv_unknownOID, KeyPurposeId::anyExtendedKeyUsage),
  SINGLE_EKU_FAILURE(tlv_unknownOID, KeyPurposeId::id_kp_serverAuth),
  SINGLE_EKU_FAILURE(tlv_unknownOID, KeyPurposeId::id_kp_clientAuth),
  SINGLE_EKU_FAILURE(tlv_unknownOID, KeyPurposeId::id_kp_codeSigning),
  SINGLE_EKU_FAILURE(tlv_unknownOID, KeyPurposeId::id_kp_emailProtection),
  SINGLE_EKU_FAILURE(tlv_unknownOID, KeyPurposeId::id_kp_OCSPSigning),

  SINGLE_EKU_SUCCESS(tlv_anyExtendedKeyUsage, KeyPurposeId::anyExtendedKeyUsage),
  SINGLE_EKU_FAILURE(tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_serverAuth),
  SINGLE_EKU_FAILURE(tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_clientAuth),
  SINGLE_EKU_FAILURE(tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_codeSigning),
  SINGLE_EKU_FAILURE(tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_emailProtection),
  SINGLE_EKU_FAILURE(tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS(tlv_id_kp_serverAuth, tlv_id_kp_clientAuth, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_serverAuth, tlv_id_kp_clientAuth, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_serverAuth, tlv_id_kp_clientAuth, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_id_kp_clientAuth, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_id_kp_clientAuth, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_id_kp_clientAuth, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS(tlv_id_kp_serverAuth, tlv_id_kp_codeSigning, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_serverAuth, tlv_id_kp_codeSigning, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_id_kp_codeSigning, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_serverAuth, tlv_id_kp_codeSigning, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_id_kp_codeSigning, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_id_kp_codeSigning, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS(tlv_id_kp_serverAuth, tlv_id_kp_emailProtection, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_serverAuth, tlv_id_kp_emailProtection, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_id_kp_emailProtection, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_id_kp_emailProtection, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_serverAuth, tlv_id_kp_emailProtection, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_id_kp_emailProtection, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS_CA(tlv_id_kp_serverAuth, tlv_id_kp_OCSPSigning, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_SUCCESS_CA(tlv_id_kp_serverAuth, tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_serverAuth, tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS(tlv_id_kp_serverAuth, tlv_id_Netscape_stepUp, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_serverAuth, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS(tlv_id_kp_serverAuth, tlv_unknownOID, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_serverAuth, tlv_unknownOID, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_unknownOID, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_unknownOID, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_unknownOID, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_unknownOID, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS(tlv_id_kp_serverAuth, tlv_anyExtendedKeyUsage, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_serverAuth, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_FAILURE(tlv_id_kp_serverAuth, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS(tlv_id_kp_clientAuth, tlv_id_kp_codeSigning, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_FAILURE(tlv_id_kp_clientAuth, tlv_id_kp_codeSigning, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_clientAuth, tlv_id_kp_codeSigning, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_clientAuth, tlv_id_kp_codeSigning, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_FAILURE(tlv_id_kp_clientAuth, tlv_id_kp_codeSigning, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_FAILURE(tlv_id_kp_clientAuth, tlv_id_kp_codeSigning, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS(tlv_id_kp_clientAuth, tlv_id_kp_emailProtection, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_FAILURE(tlv_id_kp_clientAuth, tlv_id_kp_emailProtection, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_clientAuth, tlv_id_kp_emailProtection, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_clientAuth, tlv_id_kp_emailProtection, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_clientAuth, tlv_id_kp_emailProtection, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_FAILURE(tlv_id_kp_clientAuth, tlv_id_kp_emailProtection, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS_CA(tlv_id_kp_clientAuth, tlv_id_kp_OCSPSigning, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_FAILURE(tlv_id_kp_clientAuth, tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_SUCCESS_CA(tlv_id_kp_clientAuth, tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_clientAuth, tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_FAILURE(tlv_id_kp_clientAuth, tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_clientAuth, tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS(tlv_id_kp_clientAuth, tlv_id_Netscape_stepUp, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_SUCCESS_CA(tlv_id_kp_clientAuth, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_clientAuth, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_clientAuth, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_FAILURE(tlv_id_kp_clientAuth, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_FAILURE(tlv_id_kp_clientAuth, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS(tlv_id_kp_clientAuth, tlv_unknownOID, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_FAILURE(tlv_id_kp_clientAuth, tlv_unknownOID, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_clientAuth, tlv_unknownOID, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_clientAuth, tlv_unknownOID, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_FAILURE(tlv_id_kp_clientAuth, tlv_unknownOID, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_FAILURE(tlv_id_kp_clientAuth, tlv_unknownOID, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS(tlv_id_kp_clientAuth, tlv_anyExtendedKeyUsage, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_FAILURE(tlv_id_kp_clientAuth, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_clientAuth, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_clientAuth, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_FAILURE(tlv_id_kp_clientAuth, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_FAILURE(tlv_id_kp_clientAuth, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS(tlv_id_kp_codeSigning, tlv_id_kp_emailProtection, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_FAILURE(tlv_id_kp_codeSigning, tlv_id_kp_emailProtection, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_codeSigning, tlv_id_kp_emailProtection, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_codeSigning, tlv_id_kp_emailProtection, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_codeSigning, tlv_id_kp_emailProtection, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_FAILURE(tlv_id_kp_codeSigning, tlv_id_kp_emailProtection, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS_CA(tlv_id_kp_codeSigning, tlv_id_kp_OCSPSigning, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_FAILURE(tlv_id_kp_codeSigning, tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_codeSigning, tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_SUCCESS_CA(tlv_id_kp_codeSigning, tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_FAILURE(tlv_id_kp_codeSigning, tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_codeSigning, tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS(tlv_id_kp_codeSigning, tlv_id_Netscape_stepUp, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_SUCCESS_CA(tlv_id_kp_codeSigning, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_codeSigning, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_codeSigning, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_FAILURE(tlv_id_kp_codeSigning, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_FAILURE(tlv_id_kp_codeSigning, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS(tlv_id_kp_codeSigning, tlv_unknownOID, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_FAILURE(tlv_id_kp_codeSigning, tlv_unknownOID, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_codeSigning, tlv_unknownOID, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_codeSigning, tlv_unknownOID, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_FAILURE(tlv_id_kp_codeSigning, tlv_unknownOID, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_FAILURE(tlv_id_kp_codeSigning, tlv_unknownOID, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS(tlv_id_kp_codeSigning, tlv_anyExtendedKeyUsage, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_FAILURE(tlv_id_kp_codeSigning, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_codeSigning, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_codeSigning, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_FAILURE(tlv_id_kp_codeSigning, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_FAILURE(tlv_id_kp_codeSigning, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS_CA(tlv_id_kp_emailProtection, tlv_id_kp_OCSPSigning, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_FAILURE(tlv_id_kp_emailProtection, tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_emailProtection, tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_emailProtection, tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_SUCCESS_CA(tlv_id_kp_emailProtection, tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_emailProtection, tlv_id_kp_OCSPSigning, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS(tlv_id_kp_emailProtection, tlv_id_Netscape_stepUp, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_SUCCESS_CA(tlv_id_kp_emailProtection, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_emailProtection, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_emailProtection, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_emailProtection, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_FAILURE(tlv_id_kp_emailProtection, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS(tlv_id_kp_emailProtection, tlv_unknownOID, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_FAILURE(tlv_id_kp_emailProtection, tlv_unknownOID, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_emailProtection, tlv_unknownOID, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_emailProtection, tlv_unknownOID, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_emailProtection, tlv_unknownOID, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_FAILURE(tlv_id_kp_emailProtection, tlv_unknownOID, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS(tlv_id_kp_emailProtection, tlv_anyExtendedKeyUsage, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_FAILURE(tlv_id_kp_emailProtection, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_emailProtection, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_emailProtection, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_emailProtection, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_FAILURE(tlv_id_kp_emailProtection, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS_CA(tlv_id_kp_OCSPSigning, tlv_id_Netscape_stepUp, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_SUCCESS_CA(tlv_id_kp_OCSPSigning, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_OCSPSigning, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_OCSPSigning, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_FAILURE(tlv_id_kp_OCSPSigning, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_OCSPSigning, tlv_id_Netscape_stepUp, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS_CA(tlv_id_kp_OCSPSigning, tlv_unknownOID, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_FAILURE(tlv_id_kp_OCSPSigning, tlv_unknownOID, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_OCSPSigning, tlv_unknownOID, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_OCSPSigning, tlv_unknownOID, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_FAILURE(tlv_id_kp_OCSPSigning, tlv_unknownOID, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_OCSPSigning, tlv_unknownOID, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS_CA(tlv_id_kp_OCSPSigning, tlv_anyExtendedKeyUsage, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_FAILURE(tlv_id_kp_OCSPSigning, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_OCSPSigning, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_FAILURE(tlv_id_kp_OCSPSigning, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_FAILURE(tlv_id_kp_OCSPSigning, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_SUCCESS(tlv_id_kp_OCSPSigning, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS(tlv_id_Netscape_stepUp, tlv_unknownOID, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_SUCCESS_CA(tlv_id_Netscape_stepUp, tlv_unknownOID, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_FAILURE(tlv_id_Netscape_stepUp, tlv_unknownOID, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_FAILURE(tlv_id_Netscape_stepUp, tlv_unknownOID, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_FAILURE(tlv_id_Netscape_stepUp, tlv_unknownOID, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_FAILURE(tlv_id_Netscape_stepUp, tlv_unknownOID, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS(tlv_id_Netscape_stepUp, tlv_anyExtendedKeyUsage, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_SUCCESS_CA(tlv_id_Netscape_stepUp, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_FAILURE(tlv_id_Netscape_stepUp, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_FAILURE(tlv_id_Netscape_stepUp, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_FAILURE(tlv_id_Netscape_stepUp, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_FAILURE(tlv_id_Netscape_stepUp, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_OCSPSigning),

  DOUBLE_EKU_SUCCESS(tlv_unknownOID, tlv_anyExtendedKeyUsage, KeyPurposeId::anyExtendedKeyUsage),
  DOUBLE_EKU_FAILURE(tlv_unknownOID, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_serverAuth),
  DOUBLE_EKU_FAILURE(tlv_unknownOID, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_clientAuth),
  DOUBLE_EKU_FAILURE(tlv_unknownOID, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_codeSigning),
  DOUBLE_EKU_FAILURE(tlv_unknownOID, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_emailProtection),
  DOUBLE_EKU_FAILURE(tlv_unknownOID, tlv_anyExtendedKeyUsage, KeyPurposeId::id_kp_OCSPSigning),
};

INSTANTIATE_TEST_SUITE_P(pkixcheck_CheckExtendedKeyUsage,
                        CheckExtendedKeyUsageTest,
                        ::testing::ValuesIn(EKU_TESTCASES));

struct EKUChainTestcase
{
  ByteString ekuExtensionEE;
  ByteString ekuExtensionCA;
  KeyPurposeId keyPurposeId;
  Result expectedResult;
};

::std::ostream& operator<<(::std::ostream& os, const EKUChainTestcase&)
{
  return os << "TODO (bug 1318770)";
}

class CheckExtendedKeyUsageChainTest
  : public ::testing::Test
  , public ::testing::WithParamInterface<EKUChainTestcase>
{
};

static ByteString
CreateCert(const char* issuerCN, const char* subjectCN,
           EndEntityOrCA endEntityOrCA, ByteString encodedEKU)
{
  static long serialNumberValue = 0;
  ++serialNumberValue;
  ByteString serialNumber(CreateEncodedSerialNumber(serialNumberValue));
  EXPECT_FALSE(ENCODING_FAILED(serialNumber));

  ByteString issuerDER(CNToDERName(issuerCN));
  ByteString subjectDER(CNToDERName(subjectCN));

  ByteString extensions[3];
  extensions[0] =
    CreateEncodedBasicConstraints(endEntityOrCA == EndEntityOrCA::MustBeCA,
                                  nullptr, Critical::Yes);
  EXPECT_FALSE(ENCODING_FAILED(extensions[0]));
  if (encodedEKU.length() > 0) {
    extensions[1] = encodedEKU;
  }

  ScopedTestKeyPair reusedKey(CloneReusedKeyPair());
  ByteString certDER(CreateEncodedCertificate(
                       v3, sha256WithRSAEncryption(), serialNumber, issuerDER,
                       oneDayBeforeNow, oneDayAfterNow, subjectDER,
                       *reusedKey, extensions, *reusedKey,
                       sha256WithRSAEncryption()));
  EXPECT_FALSE(ENCODING_FAILED(certDER));

  return certDER;
}

class EKUTrustDomain final : public DefaultCryptoTrustDomain
{
public:
  explicit EKUTrustDomain(ByteString issuerCertDER)
    : mIssuerCertDER(issuerCertDER)
  {
  }

private:
  Result GetCertTrust(EndEntityOrCA, const CertPolicyId&, Input candidateCert,
                      TrustLevel& trustLevel) override
  {
    trustLevel = InputEqualsByteString(candidateCert, mIssuerCertDER)
               ? TrustLevel::TrustAnchor
               : TrustLevel::InheritsTrust;
    return Success;
  }

  Result FindIssuer(Input, IssuerChecker& checker, Time) override
  {
    Input derCert;
    Result rv = derCert.Init(mIssuerCertDER.data(), mIssuerCertDER.length());
    if (rv != Success) {
      return rv;
    }
    bool keepGoing;
    return checker.Check(derCert, nullptr, keepGoing);
  }

  Result CheckRevocation(EndEntityOrCA, const CertID&, Time, Duration,
                         const Input*, const Input*, const Input*) override
  {
    return Success;
  }

  Result IsChainValid(const DERArray&, Time, const CertPolicyId&) override
  {
    return Success;
  }

  ByteString mIssuerCertDER;
};

TEST_P(CheckExtendedKeyUsageChainTest, EKUChainTestcase)
{
  const EKUChainTestcase& param(GetParam());
  ByteString issuerCertDER(CreateCert("CA", "CA", EndEntityOrCA::MustBeCA,
                                      param.ekuExtensionCA));
  ByteString subjectCertDER(CreateCert("CA", "EE",
                                       EndEntityOrCA::MustBeEndEntity,
                                       param.ekuExtensionEE));

  EKUTrustDomain trustDomain(issuerCertDER);

  Input subjectCertDERInput;
  ASSERT_EQ(Success, subjectCertDERInput.Init(subjectCertDER.data(),
                                              subjectCertDER.length()));
  ASSERT_EQ(param.expectedResult,
            BuildCertChain(trustDomain, subjectCertDERInput, Now(),
                           EndEntityOrCA::MustBeEndEntity,
                           KeyUsage::noParticularKeyUsageRequired,
                           param.keyPurposeId,
                           CertPolicyId::anyPolicy,
                           nullptr));
}

static const EKUChainTestcase EKU_CHAIN_TESTCASES[] =
{
  {
    // Both end-entity and CA have id-kp-serverAuth => should succeed
    CreateEKUExtension(BytesToByteString(tlv_id_kp_serverAuth)),
    CreateEKUExtension(BytesToByteString(tlv_id_kp_serverAuth)),
    KeyPurposeId::id_kp_serverAuth,
    Success
  },
  {
    // CA has no EKU extension => should succeed
    CreateEKUExtension(BytesToByteString(tlv_id_kp_serverAuth)),
    ByteString(),
    KeyPurposeId::id_kp_serverAuth,
    Success
  },
  {
    // End-entity has no EKU extension => should succeed
    ByteString(),
    CreateEKUExtension(BytesToByteString(tlv_id_kp_serverAuth)),
    KeyPurposeId::id_kp_serverAuth,
    Success
  },
  {
    // No EKU extensions at all => should succeed
    ByteString(),
    ByteString(),
    KeyPurposeId::id_kp_serverAuth,
    Success
  },
  {
    // CA has EKU without id-kp-serverAuth => should fail
    CreateEKUExtension(BytesToByteString(tlv_id_kp_serverAuth)),
    CreateEKUExtension(BytesToByteString(tlv_id_kp_clientAuth)),
    KeyPurposeId::id_kp_serverAuth,
    Result::ERROR_INADEQUATE_CERT_TYPE
  },
  {
    // End-entity has EKU without id-kp-serverAuth => should fail
    CreateEKUExtension(BytesToByteString(tlv_id_kp_clientAuth)),
    CreateEKUExtension(BytesToByteString(tlv_id_kp_serverAuth)),
    KeyPurposeId::id_kp_serverAuth,
    Result::ERROR_INADEQUATE_CERT_TYPE
  },
  {
    // Both end-entity and CA have EKU without id-kp-serverAuth => should fail
    CreateEKUExtension(BytesToByteString(tlv_id_kp_clientAuth)),
    CreateEKUExtension(BytesToByteString(tlv_id_kp_clientAuth)),
    KeyPurposeId::id_kp_serverAuth,
    Result::ERROR_INADEQUATE_CERT_TYPE
  },
  {
    // End-entity has no EKU, CA doesn't have id-kp-serverAuth => should fail
    ByteString(),
    CreateEKUExtension(BytesToByteString(tlv_id_kp_clientAuth)),
    KeyPurposeId::id_kp_serverAuth,
    Result::ERROR_INADEQUATE_CERT_TYPE
  },
  {
    // End-entity doesn't have id-kp-serverAuth, CA has no EKU => should fail
    CreateEKUExtension(BytesToByteString(tlv_id_kp_clientAuth)),
    ByteString(),
    KeyPurposeId::id_kp_serverAuth,
    Result::ERROR_INADEQUATE_CERT_TYPE
  },
  {
    // CA has id-Netscape-stepUp => should succeed
    CreateEKUExtension(BytesToByteString(tlv_id_kp_serverAuth)),
    CreateEKUExtension(BytesToByteString(tlv_id_Netscape_stepUp)),
    KeyPurposeId::id_kp_serverAuth,
    Success
  },
  {
    // End-entity has id-Netscape-stepUp => should fail
    CreateEKUExtension(BytesToByteString(tlv_id_Netscape_stepUp)),
    CreateEKUExtension(BytesToByteString(tlv_id_kp_serverAuth)),
    KeyPurposeId::id_kp_serverAuth,
    Result::ERROR_INADEQUATE_CERT_TYPE
  },
  {
    // End-entity and CA have id-kp-serverAuth and id-kp-clientAuth => should
    // succeed
    CreateEKUExtension(BytesToByteString(tlv_id_kp_serverAuth) +
                       BytesToByteString(tlv_id_kp_clientAuth)),
    CreateEKUExtension(BytesToByteString(tlv_id_kp_serverAuth) +
                       BytesToByteString(tlv_id_kp_clientAuth)),
    KeyPurposeId::id_kp_serverAuth,
    Success
  },
  {
    // End-entity has id-kp-serverAuth and id-kp-OCSPSigning => should fail
    CreateEKUExtension(BytesToByteString(tlv_id_kp_serverAuth) +
                       BytesToByteString(tlv_id_kp_OCSPSigning)),
    CreateEKUExtension(BytesToByteString(tlv_id_kp_serverAuth) +
                       BytesToByteString(tlv_id_kp_clientAuth)),
    KeyPurposeId::id_kp_serverAuth,
    Result::ERROR_INADEQUATE_CERT_TYPE
  },
  {
    // CA has id-kp-serverAuth and id-kp-OCSPSigning => should succeed
    CreateEKUExtension(BytesToByteString(tlv_id_kp_serverAuth) +
                       BytesToByteString(tlv_id_kp_clientAuth)),
    CreateEKUExtension(BytesToByteString(tlv_id_kp_serverAuth) +
                       BytesToByteString(tlv_id_kp_OCSPSigning)),
    KeyPurposeId::id_kp_serverAuth,
    Success
  },
};

INSTANTIATE_TEST_SUITE_P(pkixcheck_CheckExtendedKeyUsage,
                        CheckExtendedKeyUsageChainTest,
                        ::testing::ValuesIn(EKU_CHAIN_TESTCASES));
