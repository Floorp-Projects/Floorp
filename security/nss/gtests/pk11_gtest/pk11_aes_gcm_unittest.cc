/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "pk11priv.h"
#include "secerr.h"
#include "sechash.h"

#include "nss_scoped_ptrs.h"

#include "testvectors/gcm-vectors.h"
#include "gtest/gtest.h"
#include "util.h"

namespace nss_test {

class Pkcs11AesGcmTest : public ::testing::TestWithParam<AesGcmKatValue> {
 protected:
  void RunTest(const AesGcmKatValue vec) {
    std::vector<uint8_t> key = hex_string_to_bytes(vec.key);
    std::vector<uint8_t> iv = hex_string_to_bytes(vec.iv);
    std::vector<uint8_t> plaintext = hex_string_to_bytes(vec.plaintext);
    std::vector<uint8_t> aad = hex_string_to_bytes(vec.additional_data);
    std::vector<uint8_t> result = hex_string_to_bytes(vec.result);
    bool invalid_ct = vec.invalid_ct;
    bool invalid_iv = vec.invalid_iv;
    std::string msg = "Test #" + std::to_string(vec.id) + " failed";
    // Ignore GHASH-only vectors.
    if (key.empty()) {
      return;
    }

    // Prepare AEAD params.
    CK_NSS_GCM_PARAMS gcm_params;
    gcm_params.pIv = iv.data();
    gcm_params.ulIvLen = iv.size();
    gcm_params.pAAD = aad.data();
    gcm_params.ulAADLen = aad.size();
    gcm_params.ulTagBits = 128;

    SECItem params = {siBuffer, reinterpret_cast<unsigned char*>(&gcm_params),
                      sizeof(gcm_params)};

    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    SECItem key_item = {siBuffer, key.data(),
                        static_cast<unsigned int>(key.size())};

    // Import key.
    ScopedPK11SymKey sym_key(PK11_ImportSymKey(
        slot.get(), mech, PK11_OriginUnwrap, CKA_ENCRYPT, &key_item, nullptr));
    ASSERT_TRUE(!!sym_key) << msg;

    // Encrypt with bogus parameters.
    unsigned int output_len = 0;
    std::vector<uint8_t> output(plaintext.size() + gcm_params.ulTagBits / 8);
    // "maxout" must be at least "inlen + tagBytes", or, in this case:
    // "output.size()" must be at least "plaintext.size() + tagBytes"
    gcm_params.ulTagBits = 128;
    SECStatus rv =
        PK11_Encrypt(sym_key.get(), mech, &params, output.data(), &output_len,
                     output.size() - 10, plaintext.data(), plaintext.size());
    EXPECT_EQ(SECFailure, rv);
    EXPECT_EQ(0U, output_len);

    // The valid values for tag size in AES_GCM are:
    // 32, 64, 96, 104, 112, 120 and 128.
    gcm_params.ulTagBits = 110;
    rv = PK11_Encrypt(sym_key.get(), mech, &params, output.data(), &output_len,
                      output.size(), plaintext.data(), plaintext.size());
    EXPECT_EQ(SECFailure, rv);
    EXPECT_EQ(0U, output_len);

    // Encrypt.
    gcm_params.ulTagBits = 128;
    rv = PK11_Encrypt(sym_key.get(), mech, &params, output.data(), &output_len,
                      output.size(), plaintext.data(), plaintext.size());
    if (invalid_iv) {
      EXPECT_EQ(SECFailure, rv) << msg;
      EXPECT_EQ(0U, output_len);
      return;
    }
    EXPECT_EQ(SECSuccess, rv) << msg;

    ASSERT_EQ(output_len, output.size()) << msg;

    // Check ciphertext and tag.
    if (invalid_ct) {
      EXPECT_NE(result, output) << msg;
    } else {
      EXPECT_EQ(result, output) << msg;
    }

    // Decrypt.
    unsigned int decrypted_len = 0;
    // The PK11 AES API is stupid, it expects an explicit IV and thus wants
    // a block more of available output memory.
    std::vector<uint8_t> decrypted(output.size());
    rv = PK11_Decrypt(sym_key.get(), mech, &params, decrypted.data(),
                      &decrypted_len, decrypted.size(), output.data(),
                      output_len);
    EXPECT_EQ(SECSuccess, rv) << msg;
    ASSERT_EQ(decrypted_len, plaintext.size()) << msg;

    // Check the plaintext.
    EXPECT_EQ(plaintext,
              std::vector<uint8_t>(decrypted.begin(),
                                   decrypted.begin() + decrypted_len))
        << msg;
  }

  SECStatus EncryptWithIV(std::vector<uint8_t>& iv) {
    // Generate a random key.
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    ScopedPK11SymKey sym_key(
        PK11_KeyGen(slot.get(), mech, nullptr, 16, nullptr));
    EXPECT_TRUE(!!sym_key);

    std::vector<uint8_t> data(17);
    std::vector<uint8_t> output(33);
    std::vector<uint8_t> aad(0);

    // Prepare AEAD params.
    CK_NSS_GCM_PARAMS gcm_params;
    gcm_params.pIv = iv.data();
    gcm_params.ulIvLen = iv.size();
    gcm_params.pAAD = aad.data();
    gcm_params.ulAADLen = aad.size();
    gcm_params.ulTagBits = 128;

    SECItem params = {siBuffer, reinterpret_cast<unsigned char*>(&gcm_params),
                      sizeof(gcm_params)};

    // Try to encrypt.
    unsigned int output_len = 0;
    return PK11_Encrypt(sym_key.get(), mech, &params, output.data(),
                        &output_len, output.size(), data.data(), data.size());
  }

  SECStatus MessageInterfaceTest(int iterations, int ivFixedBits,
                                 CK_GENERATOR_FUNCTION ivGen,
                                 PRBool separateTag) {
    // Generate a random key.
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    EXPECT_NE(nullptr, slot);
    ScopedPK11SymKey sym_key(
        PK11_KeyGen(slot.get(), mech, nullptr, 16, nullptr));
    EXPECT_NE(nullptr, sym_key);

    const int kTagSize = 16;
    int cipher_simulated_size;
    int output_len_message = 0;
    int output_len_simulated = 0;
    unsigned int output_len_v24 = 0;

    std::vector<uint8_t> plainIn(17);
    std::vector<uint8_t> plainOut_message(17);
    std::vector<uint8_t> plainOut_simulated(17);
    std::vector<uint8_t> plainOut_v24(17);
    std::vector<uint8_t> iv(16);
    std::vector<uint8_t> iv_init(16);
    std::vector<uint8_t> iv_simulated(16);
    std::vector<uint8_t> cipher_message(33);
    std::vector<uint8_t> cipher_simulated(33);
    std::vector<uint8_t> cipher_v24(33);
    std::vector<uint8_t> aad(16);
    std::vector<uint8_t> tag_message(16);
    std::vector<uint8_t> tag_simulated(16);

    // Prepare AEAD v2.40 params.
    CK_GCM_PARAMS_V3 gcm_params;
    gcm_params.pIv = iv.data();
    gcm_params.ulIvLen = iv.size();
    gcm_params.ulIvBits = iv.size() * 8;
    gcm_params.pAAD = aad.data();
    gcm_params.ulAADLen = aad.size();
    gcm_params.ulTagBits = kTagSize * 8;

    // Prepare AEAD MESSAGE params.
    CK_GCM_MESSAGE_PARAMS gcm_message_params;
    gcm_message_params.pIv = iv.data();
    gcm_message_params.ulIvLen = iv.size();
    gcm_message_params.ulTagBits = kTagSize * 8;
    gcm_message_params.ulIvFixedBits = ivFixedBits;
    gcm_message_params.ivGenerator = ivGen;
    if (separateTag) {
      gcm_message_params.pTag = tag_message.data();
    } else {
      gcm_message_params.pTag = cipher_message.data() + plainIn.size();
    }

    // Prepare AEAD MESSAGE params for simulated case
    CK_GCM_MESSAGE_PARAMS gcm_simulated_params;
    gcm_simulated_params = gcm_message_params;
    if (separateTag) {
      // The simulated case, we have to allocate temp bufs for separate
      // tags, make sure that works in both the encrypt and the decrypt
      // cases.
      gcm_simulated_params.pTag = tag_simulated.data();
      cipher_simulated_size = cipher_simulated.size() - kTagSize;
    } else {
      gcm_simulated_params.pTag = cipher_simulated.data() + plainIn.size();
      cipher_simulated_size = cipher_simulated.size();
    }
    /* when we are using CKG_GENERATE_RANDOM, don't independently generate
     * the IV in the simulated case. Since the IV's would be random, none of
     * the generated results would be the same. Just use the IV we generated
     * in message interface */
    if (ivGen == CKG_GENERATE_RANDOM) {
      gcm_simulated_params.ivGenerator = CKG_NO_GENERATE;
    } else {
      gcm_simulated_params.pIv = iv_simulated.data();
    }

    SECItem params = {siBuffer, reinterpret_cast<unsigned char*>(&gcm_params),
                      sizeof(gcm_params)};
    SECItem empty = {siBuffer, NULL, 0};

    // initialize our plain text, IV and aad.
    EXPECT_EQ(PK11_GenerateRandom(plainIn.data(), plainIn.size()), SECSuccess);
    EXPECT_EQ(PK11_GenerateRandom(aad.data(), aad.size()), SECSuccess);
    EXPECT_EQ(PK11_GenerateRandom(iv_init.data(), iv_init.size()), SECSuccess);
    iv_simulated = iv_init;  // vector assignment actually copies data
    iv = iv_init;

    // Initialize message encrypt context
    ScopedPK11Context encrypt_message_context(PK11_CreateContextBySymKey(
        mech, CKA_NSS_MESSAGE | CKA_ENCRYPT, sym_key.get(), &empty));
    EXPECT_NE(nullptr, encrypt_message_context);
    if (!encrypt_message_context) {
      return SECFailure;
    }
    EXPECT_FALSE(_PK11_ContextGetAEADSimulation(encrypt_message_context.get()));

    // Initialize simulated encrypt context
    ScopedPK11Context encrypt_simulated_context(PK11_CreateContextBySymKey(
        mech, CKA_NSS_MESSAGE | CKA_ENCRYPT, sym_key.get(), &empty));
    EXPECT_NE(nullptr, encrypt_simulated_context);
    if (!encrypt_simulated_context) {
      return SECFailure;
    }
    EXPECT_EQ(SECSuccess,
              _PK11_ContextSetAEADSimulation(encrypt_simulated_context.get()));

    // Initialize message decrypt context
    ScopedPK11Context decrypt_message_context(PK11_CreateContextBySymKey(
        mech, CKA_NSS_MESSAGE | CKA_DECRYPT, sym_key.get(), &empty));
    EXPECT_NE(nullptr, decrypt_message_context);
    if (!decrypt_message_context) {
      return SECFailure;
    }
    EXPECT_FALSE(_PK11_ContextGetAEADSimulation(decrypt_message_context.get()));

    // Initialize simulated decrypt context
    ScopedPK11Context decrypt_simulated_context(PK11_CreateContextBySymKey(
        mech, CKA_NSS_MESSAGE | CKA_DECRYPT, sym_key.get(), &empty));
    EXPECT_NE(nullptr, decrypt_simulated_context);
    if (!decrypt_simulated_context) {
      return SECFailure;
    }
    EXPECT_EQ(SECSuccess,
              _PK11_ContextSetAEADSimulation(decrypt_simulated_context.get()));

    // Now walk down our iterations. Each method of calculating the operation
    // should agree at each step.
    for (int i = 0; i < iterations; i++) {
      SECStatus rv;
      /* recopy the initial vector each time */
      iv_simulated = iv_init;
      iv = iv_init;

      // First encrypt. We don't test the error code here, because
      // we may be testing error conditions with this function (namely
      // do we fail if we try to generate to many Random IV's).
      rv =
          PK11_AEADRawOp(encrypt_message_context.get(), &gcm_message_params,
                         sizeof(gcm_message_params), aad.data(), aad.size(),
                         cipher_message.data(), &output_len_message,
                         cipher_message.size(), plainIn.data(), plainIn.size());
      if (rv != SECSuccess) {
        return rv;
      }
      rv =
          PK11_AEADRawOp(encrypt_simulated_context.get(), &gcm_simulated_params,
                         sizeof(gcm_simulated_params), aad.data(), aad.size(),
                         cipher_simulated.data(), &output_len_simulated,
                         cipher_simulated_size, plainIn.data(), plainIn.size());
      if (rv != SECSuccess) {
        return rv;
      }
      // make sure simulated and message is the same
      EXPECT_EQ(output_len_message, output_len_simulated);
      EXPECT_EQ(0, memcmp(cipher_message.data(), cipher_simulated.data(),
                          output_len_message));
      EXPECT_EQ(0, memcmp(gcm_message_params.pTag, gcm_simulated_params.pTag,
                          kTagSize));
      EXPECT_EQ(0, memcmp(iv.data(), gcm_simulated_params.pIv, iv.size()));
      // make sure v2.40 is the same. it inherits the generated iv from
      // encrypt_message_context.
      EXPECT_EQ(SECSuccess,
                PK11_Encrypt(sym_key.get(), mech, &params, cipher_v24.data(),
                             &output_len_v24, cipher_v24.size(), plainIn.data(),
                             plainIn.size()));
      EXPECT_EQ(output_len_message, (int)output_len_v24 - kTagSize);
      EXPECT_EQ(0, memcmp(cipher_message.data(), cipher_v24.data(),
                          output_len_message));
      EXPECT_EQ(0, memcmp(gcm_message_params.pTag,
                          cipher_v24.data() + output_len_message, kTagSize));
      // now make sure we can decrypt
      EXPECT_EQ(SECSuccess,
                PK11_AEADRawOp(decrypt_message_context.get(),
                               &gcm_message_params, sizeof(gcm_message_params),
                               aad.data(), aad.size(), plainOut_message.data(),
                               &output_len_message, plainOut_message.size(),
                               cipher_message.data(), output_len_message));
      EXPECT_EQ(output_len_message, (int)plainIn.size());
      EXPECT_EQ(
          0, memcmp(plainOut_message.data(), plainIn.data(), plainIn.size()));
      EXPECT_EQ(
          SECSuccess,
          PK11_AEADRawOp(decrypt_simulated_context.get(), &gcm_simulated_params,
                         sizeof(gcm_simulated_params), aad.data(), aad.size(),
                         plainOut_simulated.data(), &output_len_simulated,
                         plainOut_simulated.size(), cipher_message.data(),
                         output_len_simulated));
      EXPECT_EQ(output_len_simulated, (int)plainIn.size());
      EXPECT_EQ(
          0, memcmp(plainOut_simulated.data(), plainIn.data(), plainIn.size()));
      if (separateTag) {
        // in the separateTag case, we need to copy the tag back to the
        // end of the cipher_message.data() before using the v2.4 interface
        memcpy(cipher_message.data() + output_len_message,
               gcm_message_params.pTag, kTagSize);
      }
      EXPECT_EQ(SECSuccess,
                PK11_Decrypt(sym_key.get(), mech, &params, plainOut_v24.data(),
                             &output_len_v24, plainOut_v24.size(),
                             cipher_message.data(), output_len_v24));
      EXPECT_EQ(output_len_v24, plainIn.size());
      EXPECT_EQ(0, memcmp(plainOut_v24.data(), plainIn.data(), plainIn.size()));
    }
    return SECSuccess;
  }

  const CK_MECHANISM_TYPE mech = CKM_AES_GCM;
};

TEST_P(Pkcs11AesGcmTest, TestVectors) { RunTest(GetParam()); }

INSTANTIATE_TEST_SUITE_P(NISTTestVector, Pkcs11AesGcmTest,
                         ::testing::ValuesIn(kGcmKatValues));

INSTANTIATE_TEST_SUITE_P(WycheproofTestVector, Pkcs11AesGcmTest,
                         ::testing::ValuesIn(kGcmWycheproofVectors));

TEST_F(Pkcs11AesGcmTest, ZeroLengthIV) {
  std::vector<uint8_t> iv(0);
  EXPECT_EQ(SECFailure, EncryptWithIV(iv));
}

TEST_F(Pkcs11AesGcmTest, AllZeroIV) {
  std::vector<uint8_t> iv(16, 0);
  EXPECT_EQ(SECSuccess, EncryptWithIV(iv));
}

TEST_F(Pkcs11AesGcmTest, TwelveByteZeroIV) {
  std::vector<uint8_t> iv(12, 0);
  EXPECT_EQ(SECSuccess, EncryptWithIV(iv));
}

// basic message interface it's the most common configuration
TEST_F(Pkcs11AesGcmTest, MessageInterfaceBasic) {
  EXPECT_EQ(SECSuccess,
            MessageInterfaceTest(16, 0, CKG_GENERATE_COUNTER, PR_FALSE));
}

// basic interface, but return the tags in a separate buffer. This triggers
// different behaviour in the simulated case, which has to buffer the
// intermediate values in a separate buffer.
TEST_F(Pkcs11AesGcmTest, MessageInterfaceSeparateTags) {
  EXPECT_EQ(SECSuccess,
            MessageInterfaceTest(16, 0, CKG_GENERATE_COUNTER, PR_TRUE));
}

// test the case where we are only allowing a portion of the iv to be generated
TEST_F(Pkcs11AesGcmTest, MessageInterfaceIVMask) {
  EXPECT_EQ(SECSuccess,
            MessageInterfaceTest(16, 124, CKG_GENERATE_COUNTER, PR_FALSE));
}

// test the case where we using the tls1.3 iv generation
TEST_F(Pkcs11AesGcmTest, MessageInterfaceXorCounter) {
  EXPECT_EQ(SECSuccess,
            MessageInterfaceTest(16, 0, CKG_GENERATE_COUNTER_XOR, PR_FALSE));
}

// test the case where we overflow the counter (requires restricted iv)
// 128-124 = 4 bits;
TEST_F(Pkcs11AesGcmTest, MessageInterfaceCounterOverflow) {
  EXPECT_EQ(SECFailure,
            MessageInterfaceTest(17, 124, CKG_GENERATE_COUNTER, PR_FALSE));
}

// overflow the tla1.2 iv case
TEST_F(Pkcs11AesGcmTest, MessageInterfaceXorCounterOverflow) {
  EXPECT_EQ(SECFailure,
            MessageInterfaceTest(17, 124, CKG_GENERATE_COUNTER_XOR, PR_FALSE));
}

// test random generation of the IV (uses an aligned restricted iv)
TEST_F(Pkcs11AesGcmTest, MessageInterfaceRandomIV) {
  EXPECT_EQ(SECSuccess,
            MessageInterfaceTest(16, 56, CKG_GENERATE_RANDOM, PR_FALSE));
}

// test the case where we try to generate too many random IVs for the size of
// our our restricted IV (notice for counters, we can generate 16 IV with
// 4 bits, but for random we need at least 72 bits to generate 16 IVs).
// 128-56 = 72 bits
TEST_F(Pkcs11AesGcmTest, MessageInterfaceRandomOverflow) {
  EXPECT_EQ(SECFailure,
            MessageInterfaceTest(17, 56, CKG_GENERATE_RANDOM, PR_FALSE));
}
}  // namespace nss_test
