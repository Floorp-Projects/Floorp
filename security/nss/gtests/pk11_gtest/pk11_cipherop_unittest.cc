// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at http://mozilla.org/MPL/2.0/.

#include "gtest/gtest.h"
#include "nss_scoped_ptrs.h"

#include <assert.h>
#include <limits.h>
#include <prinit.h>
#include <nss.h>
#include <pk11pub.h>

static const size_t kKeyLen = 128 / 8;

namespace nss_test {

//
// The ciper tests using the bltest command cover a great deal of testing.
// However, Bug 1489691 revealed a corner case which is covered here.
// This test will make multiple calls to PK11_CipherOp using the same
// cipher context with data that is not cipher block aligned.
//

static SECStatus GetBytes(const ScopedPK11Context& ctx, size_t len) {
  std::vector<uint8_t> in(len, 0);

  uint8_t outbuf[128];
  PORT_Assert(len <= sizeof(outbuf));
  int outlen;
  SECStatus rv = PK11_CipherOp(ctx.get(), outbuf, &outlen, len, in.data(), len);
  if (static_cast<size_t>(outlen) != len) {
    EXPECT_EQ(rv, SECFailure);
  }
  return rv;
}

TEST(Pkcs11CipherOp, SingleCtxMultipleUnalignedCipherOps) {
  ScopedNSSInitContext globalctx(NSS_InitContext(
      "", "", "", "", NULL, NSS_INIT_READONLY | NSS_INIT_NOCERTDB |
                                NSS_INIT_NOMODDB | NSS_INIT_FORCEOPEN |
                                NSS_INIT_NOROOTINIT));
  ASSERT_TRUE(globalctx);

  const CK_MECHANISM_TYPE cipher = CKM_AES_CTR;

  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_TRUE(slot);

  // Use arbitrary bytes for the AES key
  uint8_t key_bytes[kKeyLen];
  for (size_t i = 0; i < kKeyLen; i++) {
    key_bytes[i] = i;
  }

  SECItem keyItem = {siBuffer, key_bytes, kKeyLen};

  // The IV can be all zeros since we only encrypt once with
  // each AES key.
  CK_AES_CTR_PARAMS param = {128, {}};
  SECItem paramItem = {siBuffer, reinterpret_cast<unsigned char*>(&param),
                       sizeof(CK_AES_CTR_PARAMS)};

  ScopedPK11SymKey key(PK11_ImportSymKey(slot.get(), cipher, PK11_OriginUnwrap,
                                         CKA_ENCRYPT, &keyItem, NULL));
  ASSERT_TRUE(key);
  ScopedPK11Context ctx(
      PK11_CreateContextBySymKey(cipher, CKA_ENCRYPT, key.get(), &paramItem));
  ASSERT_TRUE(ctx);

  ASSERT_EQ(GetBytes(ctx, 7), SECSuccess);
  ASSERT_EQ(GetBytes(ctx, 17), SECSuccess);
}

// A context can't be used for Chacha20 as the underlying
// PK11_CipherOp operation is calling the C_EncryptUpdate function for
// which multi-part is disabled for ChaCha20 in counter mode.
void ChachaMulti(CK_MECHANISM_TYPE cipher, SECItem* param) {
  ScopedNSSInitContext globalctx(NSS_InitContext(
      "", "", "", "", NULL, NSS_INIT_READONLY | NSS_INIT_NOCERTDB |
                                NSS_INIT_NOMODDB | NSS_INIT_FORCEOPEN |
                                NSS_INIT_NOROOTINIT));
  ASSERT_TRUE(globalctx);

  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_TRUE(slot);

  // Use arbitrary bytes for the ChaCha20 key and IV
  uint8_t key_bytes[32];
  for (size_t i = 0; i < 32; i++) {
    key_bytes[i] = i;
  }
  SECItem keyItem = {siBuffer, key_bytes, sizeof(key_bytes)};

  ScopedPK11SymKey key(PK11_ImportSymKey(slot.get(), cipher, PK11_OriginUnwrap,
                                         CKA_ENCRYPT, &keyItem, NULL));
  ASSERT_TRUE(key);
  ScopedSECItem param_item(PK11_ParamFromIV(cipher, param));
  ASSERT_TRUE(param_item);
  ScopedPK11Context ctx(PK11_CreateContextBySymKey(
      cipher, CKA_ENCRYPT, key.get(), param_item.get()));
  ASSERT_TRUE(ctx);

  ASSERT_EQ(GetBytes(ctx, 7), SECFailure);
}

TEST(Pkcs11CipherOp, ChachaMultiLegacy) {
  uint8_t iv_bytes[16];
  for (size_t i = 0; i < 16; i++) {
    iv_bytes[i] = i;
  }
  SECItem param_item = {siBuffer, iv_bytes, sizeof(iv_bytes)};

  ChachaMulti(CKM_NSS_CHACHA20_CTR, &param_item);
}

TEST(Pkcs11CipherOp, ChachaMulti) {
  uint8_t iv_bytes[16];
  for (size_t i = 0; i < 16; i++) {
    iv_bytes[i] = i;
  }
  CK_CHACHA20_PARAMS chacha_params = {
      iv_bytes, 32, iv_bytes + 4, 96,
  };
  SECItem param_item = {siBuffer, reinterpret_cast<uint8_t*>(&chacha_params),
                        sizeof(chacha_params)};

  ChachaMulti(CKM_CHACHA20, &param_item);
}

}  // namespace nss_test
