/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "nss.h"
#include "pk11pub.h"
#include "sechash.h"
#include "prerror.h"

#include "cpputil.h"
#include "nss_scoped_ptrs.h"
#include "databuffer.h"

#include "gtest/gtest.h"
#include "pk11_signature_test.h"

namespace nss_test {

ScopedSECKEYPrivateKey Pk11SignatureTest::ImportPrivateKey(
    const DataBuffer& pkcs8) {
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) {
    ADD_FAILURE() << "No slot";
    return nullptr;
  }

  SECItem pkcs8Item = {siBuffer, toUcharPtr(pkcs8.data()),
                       static_cast<unsigned int>(pkcs8.len())};

  SECKEYPrivateKey* key = nullptr;
  SECStatus rv = PK11_ImportDERPrivateKeyInfoAndReturnKey(
      slot.get(), &pkcs8Item, nullptr, nullptr, false, false, KU_ALL, &key,
      nullptr);

  if (rv != SECSuccess) {
    return nullptr;
  }

  return ScopedSECKEYPrivateKey(key);
}

ScopedSECKEYPublicKey Pk11SignatureTest::ImportPublicKey(
    const DataBuffer& spki) {
  SECItem spkiItem = {siBuffer, toUcharPtr(spki.data()),
                      static_cast<unsigned int>(spki.len())};

  ScopedCERTSubjectPublicKeyInfo certSpki(
      SECKEY_DecodeDERSubjectPublicKeyInfo(&spkiItem));
  if (!certSpki) {
    return nullptr;
  }

  return ScopedSECKEYPublicKey(SECKEY_ExtractPublicKey(certSpki.get()));
}

bool Pk11SignatureTest::SignHashedData(ScopedSECKEYPrivateKey& privKey,
                                       const DataBuffer& hash,
                                       DataBuffer* sig) {
  SECItem hashItem = {siBuffer, toUcharPtr(hash.data()),
                      static_cast<unsigned int>(hash.len())};
  unsigned int sigLen = PK11_SignatureLen(privKey.get());
  EXPECT_LT(0, (int)sigLen);
  sig->Allocate(static_cast<size_t>(sigLen));
  SECItem sigItem = {siBuffer, toUcharPtr(sig->data()),
                     static_cast<unsigned int>(sig->len())};
  SECStatus rv = PK11_SignWithMechanism(privKey.get(), mechanism_, parameters(),
                                        &sigItem, &hashItem);
  EXPECT_EQ(sigLen, sigItem.len);
  return rv == SECSuccess;
}

bool Pk11SignatureTest::SignData(ScopedSECKEYPrivateKey& privKey,
                                 const DataBuffer& data, DataBuffer* sig) {
  unsigned int sigLen = PK11_SignatureLen(privKey.get());
  bool result = true;
  EXPECT_LT(0, (int)sigLen);
  sig->Allocate(static_cast<size_t>(sigLen));

  // test the hash and verify interface */
  PK11Context* context = PK11_CreateContextByPrivKey(
      combo_, CKA_SIGN, privKey.get(), parameters());
  if (context == NULL) {
    ADD_FAILURE() << "Failed to sign data: couldn't create context"
                  << "\n"
                  << "mech=0x" << std::hex << combo_ << "\n"
                  << "Error: " << PORT_ErrorToString(PORT_GetError());
    return false;
  }
  SECStatus rv = PK11_DigestOp(context, data.data(), data.len());
  if (rv != SECSuccess) {
    ADD_FAILURE() << "Failed to sign data: Update failed\n"
                  << "Error: " << PORT_ErrorToString(PORT_GetError());
    PK11_DestroyContext(context, PR_TRUE);
    return false;
  }
  unsigned int len = sigLen;
  rv = PK11_DigestFinal(context, sig->data(), &len, sigLen);
  if (rv != SECSuccess) {
    ADD_FAILURE() << "Failed to sign data: final failed\n"
                  << "Error: " << PORT_ErrorToString(PORT_GetError());
    result = false;
  }
  if (len != sigLen) {
    ADD_FAILURE() << "sign data: unexpected len " << len << "expected"
                  << sigLen;
    result = false;
  }
  PK11_DestroyContext(context, PR_TRUE);
  return result;
}

bool Pk11SignatureTest::ImportPrivateKeyAndSignHashedData(
    const DataBuffer& pkcs8, const DataBuffer& data, DataBuffer* sig,
    DataBuffer* sig2) {
  ScopedSECKEYPrivateKey privKey(ImportPrivateKey(pkcs8));
  if (!privKey) {
    return false;
  }

  DataBuffer hash;
  if (!ComputeHash(data, &hash)) {
    ADD_FAILURE() << "Failed to compute hash";
    return false;
  }
  if (!SignHashedData(privKey, hash, sig)) {
    ADD_FAILURE() << "Failed to sign hashed data";
    return false;
  }
  if (!SignData(privKey, data, sig2)) {
    /* failure was already added by SignData, with an error message */
    return false;
  }
  return true;
}

void Pk11SignatureTest::Verify(ScopedSECKEYPublicKey& pubKey,
                               const DataBuffer& data, const DataBuffer& sig,
                               bool valid) {
  SECStatus rv;
  DataBuffer hash;

  SECItem sigItem = {siBuffer, toUcharPtr(sig.data()),
                     static_cast<unsigned int>(sig.len())};

  /* RSA single shot requires encoding the hash before calling
   * VerifyWithMechanism. We already check that mechanism
   * with the VFY_ interface, so just do the combined hash/Verify
   * in that case */
  if (!skip_raw_) {
    ASSERT_TRUE(ComputeHash(data, &hash));

    // Verify.
    SECItem hashItem = {siBuffer, toUcharPtr(hash.data()),
                        static_cast<unsigned int>(hash.len())};
    rv = PK11_VerifyWithMechanism(pubKey.get(), mechanism_, parameters(),
                                  &sigItem, &hashItem, nullptr);
    EXPECT_EQ(rv, valid ? SECSuccess : SECFailure);
  }

  // test the hash and verify interface */
  PK11Context* context = PK11_CreateContextByPubKey(
      combo_, CKA_VERIFY, pubKey.get(), parameters(), NULL);
  /* we assert here because we'll crash if we try to continue
   * without a context. */
  ASSERT_NE((void*)context, (void*)NULL)
      << "CreateContext failed Error:" << PORT_ErrorToString(PORT_GetError())
      << "\n";
  rv = PK11_DigestOp(context, data.data(), data.len());
  /* expect success unconditionally here */
  EXPECT_EQ(rv, SECSuccess);
  unsigned int len;
  rv = PK11_DigestFinal(context, sigItem.data, &len, sigItem.len);
  EXPECT_EQ(rv, valid ? SECSuccess : SECFailure)
      << "verify failed Error:" << PORT_ErrorToString(PORT_GetError()) << "\n";
  PK11_DestroyContext(context, PR_TRUE);
}

}  // namespace nss_test
