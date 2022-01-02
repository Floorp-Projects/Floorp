/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string>

#include "gtest/gtest.h"

#include "nss_scoped_ptrs.h"
#include "cryptohi.h"
#include "secitem.h"
#include "secerr.h"

namespace nss_test {

class SignParamsTestF : public ::testing::Test {
 protected:
  ScopedPLArenaPool arena_;
  ScopedSECKEYPrivateKey privk_;
  ScopedSECKEYPublicKey pubk_;
  ScopedSECKEYPrivateKey ecPrivk_;
  ScopedSECKEYPublicKey ecPubk_;

  void SetUp() {
    arena_.reset(PORT_NewArena(2048));

    SECKEYPublicKey *pubk;
    SECKEYPrivateKey *privk = SECKEY_CreateRSAPrivateKey(1024, &pubk, NULL);
    ASSERT_NE(nullptr, pubk);
    pubk_.reset(pubk);
    ASSERT_NE(nullptr, privk);
    privk_.reset(privk);

    SECKEYECParams ecParams = {siBuffer, NULL, 0};
    SECOidData *oidData;
    oidData = SECOID_FindOIDByTag(SEC_OID_CURVE25519);
    ASSERT_NE(nullptr, oidData);
    ASSERT_NE(nullptr,
              SECITEM_AllocItem(NULL, &ecParams, (2 + oidData->oid.len)))
        << "Couldn't allocate memory for OID.";
    ecParams.data[0] = SEC_ASN1_OBJECT_ID; /* we have to prepend 0x06 */
    ecParams.data[1] = oidData->oid.len;
    memcpy(ecParams.data + 2, oidData->oid.data, oidData->oid.len);
    SECKEYPublicKey *ecPubk;
    SECKEYPrivateKey *ecPrivk =
        SECKEY_CreateECPrivateKey(&ecParams, &ecPubk, NULL);
    ASSERT_NE(nullptr, ecPubk);
    ecPubk_.reset(ecPubk);
    ASSERT_NE(nullptr, ecPrivk);
    ecPrivk_.reset(ecPrivk);
  }

  void CreatePssParams(SECKEYRSAPSSParams *params, SECOidTag hashAlgTag) {
    PORT_Memset(params, 0, sizeof(SECKEYRSAPSSParams));

    params->hashAlg = (SECAlgorithmID *)PORT_ArenaZAlloc(
        arena_.get(), sizeof(SECAlgorithmID));
    ASSERT_NE(nullptr, params->hashAlg);
    SECStatus rv =
        SECOID_SetAlgorithmID(arena_.get(), params->hashAlg, hashAlgTag, NULL);
    ASSERT_EQ(SECSuccess, rv);
  }

  void CreatePssParams(SECKEYRSAPSSParams *params, SECOidTag hashAlgTag,
                       SECOidTag maskHashAlgTag) {
    CreatePssParams(params, hashAlgTag);

    SECAlgorithmID maskHashAlg;
    PORT_Memset(&maskHashAlg, 0, sizeof(maskHashAlg));
    SECStatus rv =
        SECOID_SetAlgorithmID(arena_.get(), &maskHashAlg, maskHashAlgTag, NULL);
    ASSERT_EQ(SECSuccess, rv);

    SECItem *maskHashAlgItem =
        SEC_ASN1EncodeItem(arena_.get(), NULL, &maskHashAlg,
                           SEC_ASN1_GET(SECOID_AlgorithmIDTemplate));

    params->maskAlg = (SECAlgorithmID *)PORT_ArenaZAlloc(
        arena_.get(), sizeof(SECAlgorithmID));
    ASSERT_NE(nullptr, params->maskAlg);

    rv = SECOID_SetAlgorithmID(arena_.get(), params->maskAlg,
                               SEC_OID_PKCS1_MGF1, maskHashAlgItem);
    ASSERT_EQ(SECSuccess, rv);
  }

  void CreatePssParams(SECKEYRSAPSSParams *params, SECOidTag hashAlgTag,
                       SECOidTag maskHashAlgTag, unsigned long saltLength) {
    CreatePssParams(params, hashAlgTag, maskHashAlgTag);

    SECItem *saltLengthItem =
        SEC_ASN1EncodeInteger(arena_.get(), &params->saltLength, saltLength);
    ASSERT_EQ(&params->saltLength, saltLengthItem);
  }

  void CheckHashAlg(SECKEYRSAPSSParams *params, SECOidTag hashAlgTag) {
    // If hash algorithm is SHA-1, it must be omitted in the parameters
    if (hashAlgTag == SEC_OID_SHA1) {
      EXPECT_EQ(nullptr, params->hashAlg);
    } else {
      EXPECT_NE(nullptr, params->hashAlg);
      EXPECT_EQ(hashAlgTag, SECOID_GetAlgorithmTag(params->hashAlg));
    }
  }

  void CheckMaskAlg(SECKEYRSAPSSParams *params, SECOidTag hashAlgTag) {
    SECStatus rv;

    // If hash algorithm is SHA-1, it must be omitted in the parameters
    if (hashAlgTag == SEC_OID_SHA1)
      EXPECT_EQ(nullptr, params->hashAlg);
    else {
      EXPECT_NE(nullptr, params->maskAlg);
      EXPECT_EQ(SEC_OID_PKCS1_MGF1, SECOID_GetAlgorithmTag(params->maskAlg));

      SECAlgorithmID hashAlg;
      rv = SEC_QuickDERDecodeItem(arena_.get(), &hashAlg,
                                  SEC_ASN1_GET(SECOID_AlgorithmIDTemplate),
                                  &params->maskAlg->parameters);
      ASSERT_EQ(SECSuccess, rv);

      EXPECT_EQ(hashAlgTag, SECOID_GetAlgorithmTag(&hashAlg));
    }
  }

  void CheckSaltLength(SECKEYRSAPSSParams *params, SECOidTag hashAlg) {
    // If the salt length parameter is missing, that means it is 20 (default)
    if (!params->saltLength.data) {
      return;
    }

    unsigned long value;
    SECStatus rv = SEC_ASN1DecodeInteger(&params->saltLength, &value);
    ASSERT_EQ(SECSuccess, rv);

    // The salt length are usually the same as the hash length,
    // except for the case where the hash length exceeds the limit
    // set by the key length
    switch (hashAlg) {
      case SEC_OID_SHA1:
        EXPECT_EQ(20UL, value);
        break;
      case SEC_OID_SHA224:
        EXPECT_EQ(28UL, value);
        break;
      case SEC_OID_SHA256:
        EXPECT_EQ(32UL, value);
        break;
      case SEC_OID_SHA384:
        EXPECT_EQ(48UL, value);
        break;
      case SEC_OID_SHA512:
        // Truncated from 64, because our private key is 1024-bit
        EXPECT_EQ(62UL, value);
        break;
      default:
        FAIL();
    }
  }
};

class SignParamsTest
    : public SignParamsTestF,
      public ::testing::WithParamInterface<std::tuple<SECOidTag, SECOidTag>> {};

class SignParamsSourceTest : public SignParamsTestF,
                             public ::testing::WithParamInterface<SECOidTag> {};

TEST_P(SignParamsTest, CreateRsa) {
  SECOidTag hashAlg = std::get<0>(GetParam());
  SECOidTag srcHashAlg = std::get<1>(GetParam());

  SECItem *srcParams;
  if (srcHashAlg != SEC_OID_UNKNOWN) {
    SECKEYRSAPSSParams pssParams;
    ASSERT_NO_FATAL_FAILURE(
        CreatePssParams(&pssParams, srcHashAlg, srcHashAlg));
    srcParams = SEC_ASN1EncodeItem(arena_.get(), nullptr, &pssParams,
                                   SEC_ASN1_GET(SECKEY_RSAPSSParamsTemplate));
    ASSERT_NE(nullptr, srcParams);
  } else {
    srcParams = NULL;
  }

  SECItem *params = SEC_CreateSignatureAlgorithmParameters(
      arena_.get(), nullptr, SEC_OID_PKCS1_RSA_ENCRYPTION, hashAlg, srcParams,
      privk_.get());

  // PKCS#1 RSA actually doesn't take any parameters, but if it is
  // given, return a copy of it
  if (srcHashAlg != SEC_OID_UNKNOWN) {
    EXPECT_EQ(srcParams->len, params->len);
    EXPECT_EQ(0, memcmp(params->data, srcParams->data, srcParams->len));
  } else {
    EXPECT_EQ(nullptr, params);
  }
}

TEST_P(SignParamsTest, CreateRsaPss) {
  SECOidTag hashAlg = std::get<0>(GetParam());
  SECOidTag srcHashAlg = std::get<1>(GetParam());

  SECItem *srcParams;
  if (srcHashAlg != SEC_OID_UNKNOWN) {
    SECKEYRSAPSSParams pssParams;
    ASSERT_NO_FATAL_FAILURE(
        CreatePssParams(&pssParams, srcHashAlg, srcHashAlg));
    srcParams = SEC_ASN1EncodeItem(arena_.get(), nullptr, &pssParams,
                                   SEC_ASN1_GET(SECKEY_RSAPSSParamsTemplate));
    ASSERT_NE(nullptr, srcParams);
  } else {
    srcParams = NULL;
  }

  SECItem *params = SEC_CreateSignatureAlgorithmParameters(
      arena_.get(), nullptr, SEC_OID_PKCS1_RSA_PSS_SIGNATURE, hashAlg,
      srcParams, privk_.get());

  if (hashAlg != SEC_OID_UNKNOWN && srcHashAlg != SEC_OID_UNKNOWN &&
      hashAlg != srcHashAlg) {
    EXPECT_EQ(nullptr, params);
    return;
  }

  EXPECT_NE(nullptr, params);

  SECKEYRSAPSSParams pssParams;
  PORT_Memset(&pssParams, 0, sizeof(pssParams));
  SECStatus rv =
      SEC_QuickDERDecodeItem(arena_.get(), &pssParams,
                             SEC_ASN1_GET(SECKEY_RSAPSSParamsTemplate), params);
  ASSERT_EQ(SECSuccess, rv);

  if (hashAlg == SEC_OID_UNKNOWN) {
    if (!pssParams.hashAlg) {
      hashAlg = SEC_OID_SHA1;
    } else {
      hashAlg = SECOID_GetAlgorithmTag(pssParams.hashAlg);
    }

    if (srcHashAlg == SEC_OID_UNKNOWN) {
      // If both hashAlg and srcHashAlg is unset, NSS will decide the hash
      // algorithm based on the key length; in this case it's SHA256
      EXPECT_EQ(SEC_OID_SHA256, hashAlg);
    } else {
      EXPECT_EQ(srcHashAlg, hashAlg);
    }
  }

  ASSERT_NO_FATAL_FAILURE(CheckHashAlg(&pssParams, hashAlg));
  ASSERT_NO_FATAL_FAILURE(CheckMaskAlg(&pssParams, hashAlg));
  ASSERT_NO_FATAL_FAILURE(CheckSaltLength(&pssParams, hashAlg));

  // The default trailer field (1) must be omitted
  EXPECT_EQ(nullptr, pssParams.trailerField.data);
}

TEST_P(SignParamsTest, CreateRsaPssWithECPrivateKey) {
  SECOidTag hashAlg = std::get<0>(GetParam());
  SECOidTag srcHashAlg = std::get<1>(GetParam());

  SECItem *srcParams;
  if (srcHashAlg != SEC_OID_UNKNOWN) {
    SECKEYRSAPSSParams pssParams;
    ASSERT_NO_FATAL_FAILURE(
        CreatePssParams(&pssParams, srcHashAlg, srcHashAlg));
    srcParams = SEC_ASN1EncodeItem(arena_.get(), nullptr, &pssParams,
                                   SEC_ASN1_GET(SECKEY_RSAPSSParamsTemplate));
    ASSERT_NE(nullptr, srcParams);
  } else {
    srcParams = NULL;
  }

  SECItem *params = SEC_CreateSignatureAlgorithmParameters(
      arena_.get(), nullptr, SEC_OID_PKCS1_RSA_PSS_SIGNATURE, hashAlg,
      srcParams, ecPrivk_.get());

  EXPECT_EQ(nullptr, params);
}

TEST_P(SignParamsTest, CreateRsaPssWithInvalidHashAlg) {
  SECOidTag srcHashAlg = std::get<1>(GetParam());

  SECItem *srcParams;
  if (srcHashAlg != SEC_OID_UNKNOWN) {
    SECKEYRSAPSSParams pssParams;
    ASSERT_NO_FATAL_FAILURE(
        CreatePssParams(&pssParams, srcHashAlg, srcHashAlg));
    srcParams = SEC_ASN1EncodeItem(arena_.get(), nullptr, &pssParams,
                                   SEC_ASN1_GET(SECKEY_RSAPSSParamsTemplate));
    ASSERT_NE(nullptr, srcParams);
  } else {
    srcParams = NULL;
  }

  SECItem *params = SEC_CreateSignatureAlgorithmParameters(
      arena_.get(), nullptr, SEC_OID_PKCS1_RSA_PSS_SIGNATURE, SEC_OID_MD5,
      srcParams, privk_.get());

  EXPECT_EQ(nullptr, params);
}

TEST_P(SignParamsSourceTest, CreateRsaPssWithInvalidHashAlg) {
  SECOidTag hashAlg = GetParam();

  SECItem *srcParams;
  SECKEYRSAPSSParams pssParams;
  ASSERT_NO_FATAL_FAILURE(
      CreatePssParams(&pssParams, SEC_OID_MD5, SEC_OID_MD5));
  srcParams = SEC_ASN1EncodeItem(arena_.get(), nullptr, &pssParams,
                                 SEC_ASN1_GET(SECKEY_RSAPSSParamsTemplate));
  ASSERT_NE(nullptr, srcParams);

  SECItem *params = SEC_CreateSignatureAlgorithmParameters(
      arena_.get(), nullptr, SEC_OID_PKCS1_RSA_PSS_SIGNATURE, hashAlg,
      srcParams, privk_.get());

  EXPECT_EQ(nullptr, params);
}

TEST_P(SignParamsSourceTest, CreateRsaPssWithInvalidSaltLength) {
  SECOidTag hashAlg = GetParam();

  SECItem *srcParams;
  SECKEYRSAPSSParams pssParams;
  ASSERT_NO_FATAL_FAILURE(
      CreatePssParams(&pssParams, SEC_OID_SHA512, SEC_OID_SHA512, 100));
  srcParams = SEC_ASN1EncodeItem(arena_.get(), nullptr, &pssParams,
                                 SEC_ASN1_GET(SECKEY_RSAPSSParamsTemplate));
  ASSERT_NE(nullptr, srcParams);

  SECItem *params = SEC_CreateSignatureAlgorithmParameters(
      arena_.get(), nullptr, SEC_OID_PKCS1_RSA_PSS_SIGNATURE, hashAlg,
      srcParams, privk_.get());

  EXPECT_EQ(nullptr, params);
}

TEST_P(SignParamsSourceTest, CreateRsaPssWithHashMismatch) {
  SECOidTag hashAlg = GetParam();

  SECItem *srcParams;
  SECKEYRSAPSSParams pssParams;
  ASSERT_NO_FATAL_FAILURE(
      CreatePssParams(&pssParams, SEC_OID_SHA256, SEC_OID_SHA512));
  srcParams = SEC_ASN1EncodeItem(arena_.get(), nullptr, &pssParams,
                                 SEC_ASN1_GET(SECKEY_RSAPSSParamsTemplate));
  ASSERT_NE(nullptr, srcParams);

  SECItem *params = SEC_CreateSignatureAlgorithmParameters(
      arena_.get(), nullptr, SEC_OID_PKCS1_RSA_PSS_SIGNATURE, hashAlg,
      srcParams, privk_.get());

  EXPECT_EQ(nullptr, params);
}

INSTANTIATE_TEST_SUITE_P(
    SignParamsTestCases, SignParamsTest,
    ::testing::Combine(::testing::Values(SEC_OID_UNKNOWN, SEC_OID_SHA1,
                                         SEC_OID_SHA224, SEC_OID_SHA256,
                                         SEC_OID_SHA384, SEC_OID_SHA512),
                       ::testing::Values(SEC_OID_UNKNOWN, SEC_OID_SHA1,
                                         SEC_OID_SHA224, SEC_OID_SHA256,
                                         SEC_OID_SHA384, SEC_OID_SHA512)));

INSTANTIATE_TEST_SUITE_P(SignParamsSourceTestCases, SignParamsSourceTest,
                         ::testing::Values(SEC_OID_UNKNOWN, SEC_OID_SHA1,
                                           SEC_OID_SHA224, SEC_OID_SHA256,
                                           SEC_OID_SHA384, SEC_OID_SHA512));

}  // namespace nss_test
