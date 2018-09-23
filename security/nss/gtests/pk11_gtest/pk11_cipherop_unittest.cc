// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this file,
// You can obtain one at http://mozilla.org/MPL/2.0/.

#include "gtest/gtest.h"

#include <assert.h>
#include <limits.h>
#include <prinit.h>
#include <nss.h>
#include <pk11pub.h>

static const size_t kKeyLen = 128/8;

namespace nss_test {

//
// The ciper tests using the bltest command cover a great deal of testing.
// However, Bug 1489691 revealed a corner case which is covered here.
// This test will make multiple calls to PK11_CipherOp using the same
// cipher context with data that is not cipher block aligned.
//

static SECStatus GetBytes(PK11Context *ctx, uint8_t *bytes, size_t len)
{
  std::vector<uint8_t> in(len, 0);

  int outlen;
  SECStatus rv = PK11_CipherOp(ctx, bytes, &outlen, len, &in[0], len);
  if (static_cast<size_t>(outlen) != len) {
    return SECFailure;
  }
  return rv;
}

TEST(Pkcs11CipherOp, SingleCtxMultipleUnalignedCipherOps) {
  PK11SlotInfo* slot;
  PK11SymKey* key;
  PK11Context* ctx;

  NSSInitContext* globalctx = NSS_InitContext("", "", "", "", NULL,
                    NSS_INIT_READONLY | NSS_INIT_NOCERTDB | NSS_INIT_NOMODDB |
                      NSS_INIT_FORCEOPEN | NSS_INIT_NOROOTINIT);

  const CK_MECHANISM_TYPE cipher = CKM_AES_CTR;

  slot = PK11_GetInternalSlot();
  ASSERT_TRUE(slot);

  // Use arbitrary bytes for the AES key
  uint8_t key_bytes[kKeyLen];
  for (size_t i = 0; i < kKeyLen; i++) {
    key_bytes[i] = i;
  }

  SECItem keyItem = { siBuffer, key_bytes, kKeyLen };

  // The IV can be all zeros since we only encrypt once with
  // each AES key.
  CK_AES_CTR_PARAMS param = { 128, {} };
  SECItem paramItem = { siBuffer, reinterpret_cast<unsigned char*>(&param), sizeof(CK_AES_CTR_PARAMS) };

  key = PK11_ImportSymKey(slot, cipher, PK11_OriginUnwrap,
                                        CKA_ENCRYPT, &keyItem, NULL);
  ctx = PK11_CreateContextBySymKey(cipher, CKA_ENCRYPT, key, &paramItem);
  ASSERT_TRUE(key);
  ASSERT_TRUE(ctx);

  uint8_t outbuf[128];
  ASSERT_EQ(GetBytes(ctx, outbuf, 7), SECSuccess);
  ASSERT_EQ(GetBytes(ctx, outbuf, 17), SECSuccess);

  PK11_FreeSymKey(key);
  PK11_FreeSlot(slot);
  PK11_DestroyContext(ctx, PR_TRUE);
  NSS_ShutdownContext(globalctx);
}

}  // namespace nss_test
