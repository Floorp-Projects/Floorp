/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef UNSAFE_FUZZER_MODE  // See Bug 1709750

#include <memory>
#include "blapi.h"
#include "gtest/gtest.h"
#include "nss.h"
#include "nss_scoped_ptrs.h"
#include "pk11hpke.h"
#include "pk11pub.h"
#include "secerr.h"
#include "sechash.h"
#include "testvectors/hpke-vectors.h"
#include "util.h"

namespace nss_test {

/* See note in pk11pub.h. */
#include "cpputil.h"

class HpkeTest {
 protected:
  void CheckEquality(const std::vector<uint8_t> &expected, SECItem *actual) {
    if (!actual) {
      EXPECT_TRUE(expected.empty());
      return;
    }
    std::vector<uint8_t> vact(actual->data, actual->data + actual->len);
    EXPECT_EQ(expected, vact);
  }

  void CheckEquality(SECItem *expected, SECItem *actual) {
    EXPECT_EQ(!!expected, !!actual);
    if (expected && actual) {
      EXPECT_EQ(expected->len, actual->len);
      if (expected->len == actual->len) {
        EXPECT_EQ(0, memcmp(expected->data, actual->data, actual->len));
      }
    }
  }

  void CheckEquality(const std::vector<uint8_t> &expected, PK11SymKey *actual) {
    if (!actual) {
      EXPECT_TRUE(expected.empty());
      return;
    }
    SECStatus rv = PK11_ExtractKeyValue(actual);
    EXPECT_EQ(SECSuccess, rv);
    if (rv != SECSuccess) {
      return;
    }
    SECItem *rawkey = PK11_GetKeyData(actual);
    CheckEquality(expected, rawkey);
  }

  void CheckEquality(PK11SymKey *expected, PK11SymKey *actual) {
    if (!actual || !expected) {
      EXPECT_EQ(!!expected, !!actual);
      return;
    }
    SECStatus rv = PK11_ExtractKeyValue(expected);
    EXPECT_EQ(SECSuccess, rv);
    if (rv != SECSuccess) {
      return;
    }
    SECItem *raw = PK11_GetKeyData(expected);
    ASSERT_NE(nullptr, raw);
    ASSERT_NE(nullptr, raw->data);
    std::vector<uint8_t> expected_vec(raw->data, raw->data + raw->len);
    CheckEquality(expected_vec, actual);
  }

  void Seal(const ScopedHpkeContext &cx, std::vector<uint8_t> &aad_vec,
            std::vector<uint8_t> &pt_vec, std::vector<uint8_t> &out_sealed) {
    SECItem aad_item = {siBuffer, toUcharPtr(aad_vec.data()),
                        static_cast<unsigned int>(aad_vec.size())};
    SECItem pt_item = {siBuffer, toUcharPtr(pt_vec.data()),
                       static_cast<unsigned int>(pt_vec.size())};

    SECItem *sealed_item = nullptr;
    EXPECT_EQ(SECSuccess,
              PK11_HPKE_Seal(cx.get(), &aad_item, &pt_item, &sealed_item));
    ASSERT_NE(nullptr, sealed_item);
    ScopedSECItem sealed(sealed_item);
    out_sealed.assign(sealed->data, sealed->data + sealed->len);
  }

  void Open(const ScopedHpkeContext &cx, std::vector<uint8_t> &aad_vec,
            std::vector<uint8_t> &ct_vec, std::vector<uint8_t> &out_opened) {
    SECItem aad_item = {siBuffer, toUcharPtr(aad_vec.data()),
                        static_cast<unsigned int>(aad_vec.size())};
    SECItem ct_item = {siBuffer, toUcharPtr(ct_vec.data()),
                       static_cast<unsigned int>(ct_vec.size())};
    SECItem *opened_item = nullptr;
    EXPECT_EQ(SECSuccess,
              PK11_HPKE_Open(cx.get(), &aad_item, &ct_item, &opened_item));
    ASSERT_NE(nullptr, opened_item);
    ScopedSECItem opened(opened_item);
    out_opened.assign(opened->data, opened->data + opened->len);
  }

  void SealOpen(const ScopedHpkeContext &sender,
                const ScopedHpkeContext &receiver, std::vector<uint8_t> &msg,
                std::vector<uint8_t> &aad, const std::vector<uint8_t> *expect) {
    std::vector<uint8_t> sealed;
    std::vector<uint8_t> opened;
    Seal(sender, aad, msg, sealed);
    if (expect) {
      EXPECT_EQ(*expect, sealed);
    }
    Open(receiver, aad, sealed, opened);
    EXPECT_EQ(msg, opened);
  }

  void ExportSecret(const ScopedHpkeContext &receiver,
                    ScopedPK11SymKey &exported) {
    std::vector<uint8_t> context = {'c', 't', 'x', 't'};
    SECItem context_item = {siBuffer, context.data(),
                            static_cast<unsigned int>(context.size())};
    PK11SymKey *tmp_exported = nullptr;
    ASSERT_EQ(SECSuccess, PK11_HPKE_ExportSecret(receiver.get(), &context_item,
                                                 64, &tmp_exported));
    exported.reset(tmp_exported);
  }

  void ExportImportRecvContext(ScopedHpkeContext &scoped_cx,
                               PK11SymKey *wrapping_key) {
    SECItem *tmp_exported = nullptr;
    EXPECT_EQ(SECSuccess, PK11_HPKE_ExportContext(scoped_cx.get(), wrapping_key,
                                                  &tmp_exported));
    EXPECT_NE(nullptr, tmp_exported);
    ScopedSECItem context(tmp_exported);
    scoped_cx.reset();

    HpkeContext *tmp_imported =
        PK11_HPKE_ImportContext(context.get(), wrapping_key);
    EXPECT_NE(nullptr, tmp_imported);
    scoped_cx.reset(tmp_imported);
  }

  bool GenerateKeyPair(ScopedSECKEYPublicKey &pub_key,
                       ScopedSECKEYPrivateKey &priv_key) {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    if (!slot) {
      ADD_FAILURE() << "Couldn't get slot";
      return false;
    }

    unsigned char param_buf[65];
    SECItem ecdsa_params = {siBuffer, param_buf, sizeof(param_buf)};
    SECOidData *oid_data = SECOID_FindOIDByTag(SEC_OID_CURVE25519);
    if (!oid_data) {
      ADD_FAILURE() << "Couldn't get oid_data";
      return false;
    }
    ecdsa_params.data[0] = SEC_ASN1_OBJECT_ID;
    ecdsa_params.data[1] = oid_data->oid.len;
    memcpy(ecdsa_params.data + 2, oid_data->oid.data, oid_data->oid.len);
    ecdsa_params.len = oid_data->oid.len + 2;

    SECKEYPublicKey *pub_tmp;
    SECKEYPrivateKey *priv_tmp;
    priv_tmp =
        PK11_GenerateKeyPair(slot.get(), CKM_EC_KEY_PAIR_GEN, &ecdsa_params,
                             &pub_tmp, PR_FALSE, PR_TRUE, nullptr);
    if (!pub_tmp || !priv_tmp) {
      ADD_FAILURE() << "PK11_GenerateKeyPair failed";
      return false;
    }

    pub_key.reset(pub_tmp);
    priv_key.reset(priv_tmp);
    return true;
  }

  void SetUpEphemeralContexts(ScopedHpkeContext &sender,
                              ScopedHpkeContext &receiver,
                              HpkeModeId mode = HpkeModeBase,
                              HpkeKemId kem = HpkeDhKemX25519Sha256,
                              HpkeKdfId kdf = HpkeKdfHkdfSha256,
                              HpkeAeadId aead = HpkeAeadAes128Gcm) {
    // Generate a PSK, if the mode calls for it.
    PRUint8 psk_id_buf[] = {'p', 's', 'k', '-', 'i', 'd'};
    SECItem psk_id = {siBuffer, psk_id_buf, sizeof(psk_id_buf)};
    SECItem *psk_id_item = (mode == HpkeModePsk) ? &psk_id : nullptr;
    ScopedPK11SymKey psk;
    if (mode == HpkeModePsk) {
      ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
      ASSERT_TRUE(slot);
      PK11SymKey *tmp_psk =
          PK11_KeyGen(slot.get(), CKM_HKDF_DERIVE, nullptr, 16, nullptr);
      ASSERT_NE(nullptr, tmp_psk);
      psk.reset(tmp_psk);
    }

    std::vector<uint8_t> info = {'t', 'e', 's', 't', '-', 'i', 'n', 'f', 'o'};
    SECItem info_item = {siBuffer, info.data(),
                         static_cast<unsigned int>(info.size())};
    sender.reset(PK11_HPKE_NewContext(kem, kdf, aead, psk.get(), psk_id_item));
    receiver.reset(
        PK11_HPKE_NewContext(kem, kdf, aead, psk.get(), psk_id_item));
    ASSERT_TRUE(sender);
    ASSERT_TRUE(receiver);

    ScopedSECKEYPublicKey pub_key_r;
    ScopedSECKEYPrivateKey priv_key_r;
    ASSERT_TRUE(GenerateKeyPair(pub_key_r, priv_key_r));
    EXPECT_EQ(SECSuccess, PK11_HPKE_SetupS(sender.get(), nullptr, nullptr,
                                           pub_key_r.get(), &info_item));

    const SECItem *enc = PK11_HPKE_GetEncapPubKey(sender.get());
    EXPECT_NE(nullptr, enc);
    EXPECT_EQ(SECSuccess, PK11_HPKE_SetupR(
                              receiver.get(), pub_key_r.get(), priv_key_r.get(),
                              const_cast<SECItem *>(enc), &info_item));
  }
};

class TestVectors : public HpkeTest,
                    public ::testing::TestWithParam<hpke_vector> {
 protected:
  void ReadVector(const hpke_vector &vec) {
    ScopedPK11SymKey vec_psk;
    if (!vec.psk.empty()) {
      ASSERT_FALSE(vec.psk_id.empty());
      vec_psk_id = hex_string_to_bytes(vec.psk_id);

      std::vector<uint8_t> psk_bytes = hex_string_to_bytes(vec.psk);
      SECItem psk_item = {siBuffer, toUcharPtr(psk_bytes.data()),
                          static_cast<unsigned int>(psk_bytes.size())};
      ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
      ASSERT_TRUE(slot);
      PK11SymKey *psk_key =
          PK11_ImportSymKey(slot.get(), CKM_HKDF_KEY_GEN, PK11_OriginUnwrap,
                            CKA_WRAP, &psk_item, nullptr);
      ASSERT_NE(nullptr, psk_key);
      vec_psk_key.reset(psk_key);
    }

    vec_pkcs8_r = hex_string_to_bytes(vec.pkcs8_r);
    vec_pkcs8_e = hex_string_to_bytes(vec.pkcs8_e);
    vec_key = hex_string_to_bytes(vec.key);
    vec_nonce = hex_string_to_bytes(vec.nonce);
    vec_enc = hex_string_to_bytes(vec.enc);
    vec_info = hex_string_to_bytes(vec.info);
    vec_encryptions = vec.encrypt_vecs;
    vec_exports = vec.export_vecs;
  }

  void TestExports(const ScopedHpkeContext &sender,
                   const ScopedHpkeContext &receiver) {
    for (auto &vec : vec_exports) {
      std::vector<uint8_t> context = hex_string_to_bytes(vec.ctxt);
      std::vector<uint8_t> expected = hex_string_to_bytes(vec.exported);
      SECItem context_item = {siBuffer, toUcharPtr(context.data()),
                              static_cast<unsigned int>(context.size())};
      PK11SymKey *actual_r = nullptr;
      PK11SymKey *actual_s = nullptr;
      ASSERT_EQ(SECSuccess, PK11_HPKE_ExportSecret(sender.get(), &context_item,
                                                   vec.len, &actual_s));
      ASSERT_EQ(SECSuccess,
                PK11_HPKE_ExportSecret(receiver.get(), &context_item, vec.len,
                                       &actual_r));
      ScopedPK11SymKey scoped_act_s(actual_s);
      ScopedPK11SymKey scoped_act_r(actual_r);
      CheckEquality(expected, scoped_act_s.get());
      CheckEquality(expected, scoped_act_r.get());
    }
  }

  void TestEncryptions(const ScopedHpkeContext &sender,
                       const ScopedHpkeContext &receiver) {
    for (auto &enc_vec : vec_encryptions) {
      std::vector<uint8_t> msg = hex_string_to_bytes(enc_vec.pt);
      std::vector<uint8_t> aad = hex_string_to_bytes(enc_vec.aad);
      std::vector<uint8_t> expect_ct = hex_string_to_bytes(enc_vec.ct);
      SealOpen(sender, receiver, msg, aad, &expect_ct);
    }
  }

  void ImportKeyPairs(const ScopedHpkeContext &sender,
                      const ScopedHpkeContext &receiver) {
    ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
    if (!slot) {
      ADD_FAILURE() << "No slot";
      return;
    }

    SECItem pkcs8_e_item = {siBuffer, toUcharPtr(vec_pkcs8_e.data()),
                            static_cast<unsigned int>(vec_pkcs8_e.size())};
    SECKEYPrivateKey *sk_e = nullptr;
    EXPECT_EQ(SECSuccess, PK11_ImportDERPrivateKeyInfoAndReturnKey(
                              slot.get(), &pkcs8_e_item, nullptr, nullptr,
                              false, false, KU_ALL, &sk_e, nullptr));
    skE_derived.reset(sk_e);
    SECKEYPublicKey *pk_e = SECKEY_ConvertToPublicKey(skE_derived.get());
    ASSERT_NE(nullptr, pk_e);
    pkE_derived.reset(pk_e);

    SECItem pkcs8_r_item = {siBuffer, toUcharPtr(vec_pkcs8_r.data()),
                            static_cast<unsigned int>(vec_pkcs8_r.size())};
    SECKEYPrivateKey *sk_r = nullptr;
    EXPECT_EQ(SECSuccess, PK11_ImportDERPrivateKeyInfoAndReturnKey(
                              slot.get(), &pkcs8_r_item, nullptr, nullptr,
                              false, false, KU_ALL, &sk_r, nullptr));
    skR_derived.reset(sk_r);
    SECKEYPublicKey *pk_r = SECKEY_ConvertToPublicKey(skR_derived.get());
    ASSERT_NE(nullptr, pk_r);
    pkR_derived.reset(pk_r);
  }

  void SetupS(const ScopedHpkeContext &cx, const ScopedSECKEYPublicKey &pkE,
              const ScopedSECKEYPrivateKey &skE,
              const ScopedSECKEYPublicKey &pkR,
              const std::vector<uint8_t> &info) {
    SECItem info_item = {siBuffer, toUcharPtr(vec_info.data()),
                         static_cast<unsigned int>(vec_info.size())};
    EXPECT_EQ(SECSuccess, PK11_HPKE_SetupS(cx.get(), pkE.get(), skE.get(),
                                           pkR.get(), &info_item));
  }

  void SetupR(const ScopedHpkeContext &cx, const ScopedSECKEYPublicKey &pkR,
              const ScopedSECKEYPrivateKey &skR,
              const std::vector<uint8_t> &enc,
              const std::vector<uint8_t> &info) {
    SECItem enc_item = {siBuffer, toUcharPtr(enc.data()),
                        static_cast<unsigned int>(enc.size())};
    SECItem info_item = {siBuffer, toUcharPtr(vec_info.data()),
                         static_cast<unsigned int>(vec_info.size())};
    EXPECT_EQ(SECSuccess, PK11_HPKE_SetupR(cx.get(), pkR.get(), skR.get(),
                                           &enc_item, &info_item));
  }

  void SetupSenderReceiver(const ScopedHpkeContext &sender,
                           const ScopedHpkeContext &receiver) {
    SetupS(sender, pkE_derived, skE_derived, pkR_derived, vec_info);
    uint8_t buf[32];  // Curve25519 only, fixed size.
    SECItem encap_item = {siBuffer, const_cast<uint8_t *>(buf), sizeof(buf)};
    ASSERT_EQ(SECSuccess,
              PK11_HPKE_Serialize(pkE_derived.get(), encap_item.data,
                                  &encap_item.len, encap_item.len));
    CheckEquality(vec_enc, &encap_item);
    SetupR(receiver, pkR_derived, skR_derived, vec_enc, vec_info);
  }

  void RunTestVector(const hpke_vector &vec) {
    ReadVector(vec);
    SECItem psk_id_item = {siBuffer, toUcharPtr(vec_psk_id.data()),
                           static_cast<unsigned int>(vec_psk_id.size())};
    PK11SymKey *psk = vec_psk_key ? vec_psk_key.get() : nullptr;
    SECItem *psk_id = psk ? &psk_id_item : nullptr;

    ScopedHpkeContext sender(
        PK11_HPKE_NewContext(vec.kem_id, vec.kdf_id, vec.aead_id, psk, psk_id));
    ScopedHpkeContext receiver(
        PK11_HPKE_NewContext(vec.kem_id, vec.kdf_id, vec.aead_id, psk, psk_id));
    ASSERT_TRUE(sender);
    ASSERT_TRUE(receiver);

    ImportKeyPairs(sender, receiver);
    SetupSenderReceiver(sender, receiver);
    TestEncryptions(sender, receiver);
    TestExports(sender, receiver);
  }

 private:
  ScopedPK11SymKey vec_psk_key;
  std::vector<uint8_t> vec_psk_id;
  std::vector<uint8_t> vec_pkcs8_e;
  std::vector<uint8_t> vec_pkcs8_r;
  std::vector<uint8_t> vec_enc;
  std::vector<uint8_t> vec_info;
  std::vector<uint8_t> vec_key;
  std::vector<uint8_t> vec_nonce;
  std::vector<hpke_encrypt_vector> vec_encryptions;
  std::vector<hpke_export_vector> vec_exports;
  ScopedSECKEYPublicKey pkE_derived;
  ScopedSECKEYPublicKey pkR_derived;
  ScopedSECKEYPrivateKey skE_derived;
  ScopedSECKEYPrivateKey skR_derived;
};

TEST_P(TestVectors, TestVectors) { RunTestVector(GetParam()); }

INSTANTIATE_TEST_SUITE_P(Pk11Hpke, TestVectors,
                         ::testing::ValuesIn(kHpkeTestVectors));

class ModeParameterizedTest
    : public HpkeTest,
      public ::testing::TestWithParam<
          std::tuple<HpkeModeId, HpkeKemId, HpkeKdfId, HpkeAeadId>> {};

static const HpkeModeId kHpkeModesAll[] = {HpkeModeBase, HpkeModePsk};
static const HpkeKemId kHpkeKemIdsAll[] = {HpkeDhKemX25519Sha256};
static const HpkeKdfId kHpkeKdfIdsAll[] = {HpkeKdfHkdfSha256, HpkeKdfHkdfSha384,
                                           HpkeKdfHkdfSha512};
static const HpkeAeadId kHpkeAeadIdsAll[] = {HpkeAeadAes128Gcm,
                                             HpkeAeadChaCha20Poly1305};

INSTANTIATE_TEST_SUITE_P(
    Pk11Hpke, ModeParameterizedTest,
    ::testing::Combine(::testing::ValuesIn(kHpkeModesAll),
                       ::testing::ValuesIn(kHpkeKemIdsAll),
                       ::testing::ValuesIn(kHpkeKdfIdsAll),
                       ::testing::ValuesIn(kHpkeAeadIdsAll)));

TEST_F(ModeParameterizedTest, BadEncapsulatedPubKey) {
  ScopedHpkeContext sender(
      PK11_HPKE_NewContext(HpkeDhKemX25519Sha256, HpkeKdfHkdfSha256,
                           HpkeAeadAes128Gcm, nullptr, nullptr));
  ScopedHpkeContext receiver(
      PK11_HPKE_NewContext(HpkeDhKemX25519Sha256, HpkeKdfHkdfSha256,
                           HpkeAeadAes128Gcm, nullptr, nullptr));

  SECItem empty = {siBuffer, nullptr, 0};
  uint8_t buf[100];
  SECItem short_encap = {siBuffer, buf, 1};
  SECItem long_encap = {siBuffer, buf, sizeof(buf)};

  SECKEYPublicKey *tmp_pub_key;
  ScopedSECKEYPublicKey pub_key;
  ScopedSECKEYPrivateKey priv_key;
  ASSERT_TRUE(GenerateKeyPair(pub_key, priv_key));

  // Decapsulating an empty buffer should fail.
  EXPECT_EQ(SECFailure, PK11_HPKE_Deserialize(sender.get(), empty.data,
                                              empty.len, &tmp_pub_key));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());

  // Decapsulating anything short will succeed, but the setup will fail.
  EXPECT_EQ(SECSuccess, PK11_HPKE_Deserialize(sender.get(), short_encap.data,
                                              short_encap.len, &tmp_pub_key));
  ScopedSECKEYPublicKey bad_pub_key(tmp_pub_key);

  EXPECT_EQ(SECFailure,
            PK11_HPKE_SetupS(receiver.get(), pub_key.get(), priv_key.get(),
                             bad_pub_key.get(), &empty));
  EXPECT_EQ(SEC_ERROR_INVALID_KEY, PORT_GetError());

  // Test the same for a receiver.
  EXPECT_EQ(SECFailure, PK11_HPKE_SetupR(sender.get(), pub_key.get(),
                                         priv_key.get(), &empty, &empty));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
  EXPECT_EQ(SECFailure, PK11_HPKE_SetupR(sender.get(), pub_key.get(),
                                         priv_key.get(), &short_encap, &empty));
  EXPECT_EQ(SEC_ERROR_INVALID_KEY, PORT_GetError());

  // Encapsulated key too long
  EXPECT_EQ(SECSuccess, PK11_HPKE_Deserialize(sender.get(), long_encap.data,
                                              long_encap.len, &tmp_pub_key));
  bad_pub_key.reset(tmp_pub_key);
  EXPECT_EQ(SECFailure,
            PK11_HPKE_SetupS(receiver.get(), pub_key.get(), priv_key.get(),
                             bad_pub_key.get(), &empty));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());

  EXPECT_EQ(SECFailure, PK11_HPKE_SetupR(sender.get(), pub_key.get(),
                                         priv_key.get(), &long_encap, &empty));
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
}

TEST_P(ModeParameterizedTest, ContextExportImportEncrypt) {
  std::vector<uint8_t> msg = {'s', 'e', 'c', 'r', 'e', 't'};
  std::vector<uint8_t> aad = {'a', 'a', 'd'};

  ScopedHpkeContext sender;
  ScopedHpkeContext receiver;
  SetUpEphemeralContexts(sender, receiver, std::get<0>(GetParam()),
                         std::get<1>(GetParam()), std::get<2>(GetParam()),
                         std::get<3>(GetParam()));
  SealOpen(sender, receiver, msg, aad, nullptr);
  ExportImportRecvContext(receiver, nullptr);
  SealOpen(sender, receiver, msg, aad, nullptr);
}

TEST_P(ModeParameterizedTest, ContextExportImportExport) {
  ScopedHpkeContext sender;
  ScopedHpkeContext receiver;
  ScopedPK11SymKey sender_export;
  ScopedPK11SymKey receiver_export;
  ScopedPK11SymKey receiver_reexport;
  SetUpEphemeralContexts(sender, receiver, std::get<0>(GetParam()),
                         std::get<1>(GetParam()), std::get<2>(GetParam()),
                         std::get<3>(GetParam()));
  ExportSecret(sender, sender_export);
  ExportSecret(receiver, receiver_export);
  CheckEquality(sender_export.get(), receiver_export.get());
  ExportImportRecvContext(receiver, nullptr);
  ExportSecret(receiver, receiver_reexport);
  CheckEquality(receiver_export.get(), receiver_reexport.get());
}

TEST_P(ModeParameterizedTest, ContextExportImportWithWrap) {
  std::vector<uint8_t> msg = {'s', 'e', 'c', 'r', 'e', 't'};
  std::vector<uint8_t> aad = {'a', 'a', 'd'};

  // Generate a wrapping key, then use it for export.
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_TRUE(slot);
  ScopedPK11SymKey kek(
      PK11_KeyGen(slot.get(), CKM_AES_CBC, nullptr, 16, nullptr));
  ASSERT_NE(nullptr, kek);

  ScopedHpkeContext sender;
  ScopedHpkeContext receiver;
  SetUpEphemeralContexts(sender, receiver, std::get<0>(GetParam()),
                         std::get<1>(GetParam()), std::get<2>(GetParam()),
                         std::get<3>(GetParam()));
  SealOpen(sender, receiver, msg, aad, nullptr);
  ExportImportRecvContext(receiver, kek.get());
  SealOpen(sender, receiver, msg, aad, nullptr);
}

TEST_P(ModeParameterizedTest, ExportSenderContext) {
  std::vector<uint8_t> msg = {'s', 'e', 'c', 'r', 'e', 't'};
  std::vector<uint8_t> aad = {'a', 'a', 'd'};

  ScopedHpkeContext sender;
  ScopedHpkeContext receiver;
  SetUpEphemeralContexts(sender, receiver, std::get<0>(GetParam()),
                         std::get<1>(GetParam()), std::get<2>(GetParam()),
                         std::get<3>(GetParam()));

  SECItem *tmp_exported = nullptr;
  EXPECT_EQ(SECFailure,
            PK11_HPKE_ExportContext(sender.get(), nullptr, &tmp_exported));
  EXPECT_EQ(nullptr, tmp_exported);
  EXPECT_EQ(SEC_ERROR_NOT_A_RECIPIENT, PORT_GetError());
}

TEST_P(ModeParameterizedTest, ContextUnwrapBadKey) {
  std::vector<uint8_t> msg = {'s', 'e', 'c', 'r', 'e', 't'};
  std::vector<uint8_t> aad = {'a', 'a', 'd'};

  // Generate a wrapping key, then use it for export.
  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  ASSERT_TRUE(slot);
  ScopedPK11SymKey kek(
      PK11_KeyGen(slot.get(), CKM_AES_CBC, nullptr, 16, nullptr));
  ASSERT_NE(nullptr, kek);
  ScopedPK11SymKey not_kek(
      PK11_KeyGen(slot.get(), CKM_AES_CBC, nullptr, 16, nullptr));
  ASSERT_NE(nullptr, not_kek);
  ScopedHpkeContext sender;
  ScopedHpkeContext receiver;

  SetUpEphemeralContexts(sender, receiver, std::get<0>(GetParam()),
                         std::get<1>(GetParam()), std::get<2>(GetParam()),
                         std::get<3>(GetParam()));

  SECItem *tmp_exported = nullptr;
  EXPECT_EQ(SECSuccess,
            PK11_HPKE_ExportContext(receiver.get(), kek.get(), &tmp_exported));
  EXPECT_NE(nullptr, tmp_exported);
  ScopedSECItem context(tmp_exported);

  EXPECT_EQ(nullptr, PK11_HPKE_ImportContext(context.get(), not_kek.get()));
  EXPECT_EQ(SEC_ERROR_BAD_DATA, PORT_GetError());
}

TEST_P(ModeParameterizedTest, EphemeralKeys) {
  std::vector<uint8_t> msg = {'s', 'e', 'c', 'r', 'e', 't'};
  std::vector<uint8_t> aad = {'a', 'a', 'd'};
  SECItem msg_item = {siBuffer, msg.data(),
                      static_cast<unsigned int>(msg.size())};
  SECItem aad_item = {siBuffer, aad.data(),
                      static_cast<unsigned int>(aad.size())};
  ScopedHpkeContext sender;
  ScopedHpkeContext receiver;
  SetUpEphemeralContexts(sender, receiver, std::get<0>(GetParam()),
                         std::get<1>(GetParam()), std::get<2>(GetParam()),
                         std::get<3>(GetParam()));

  SealOpen(sender, receiver, msg, aad, nullptr);

  // Seal for negative tests
  SECItem *tmp_sealed = nullptr;
  SECItem *tmp_unsealed = nullptr;
  EXPECT_EQ(SECSuccess,
            PK11_HPKE_Seal(sender.get(), &aad_item, &msg_item, &tmp_sealed));
  ASSERT_NE(nullptr, tmp_sealed);
  ScopedSECItem sealed(tmp_sealed);

  // Drop AAD
  EXPECT_EQ(SECFailure, PK11_HPKE_Open(receiver.get(), nullptr, sealed.get(),
                                       &tmp_unsealed));
  EXPECT_EQ(SEC_ERROR_BAD_DATA, PORT_GetError());
  EXPECT_EQ(nullptr, tmp_unsealed);

  // Modify AAD
  aad_item.data[0] ^= 0xff;
  EXPECT_EQ(SECFailure, PK11_HPKE_Open(receiver.get(), &aad_item, sealed.get(),
                                       &tmp_unsealed));
  EXPECT_EQ(SEC_ERROR_BAD_DATA, PORT_GetError());
  EXPECT_EQ(nullptr, tmp_unsealed);
  aad_item.data[0] ^= 0xff;

  // Modify ciphertext
  sealed->data[0] ^= 0xff;
  EXPECT_EQ(SECFailure, PK11_HPKE_Open(receiver.get(), &aad_item, sealed.get(),
                                       &tmp_unsealed));
  EXPECT_EQ(SEC_ERROR_BAD_DATA, PORT_GetError());
  EXPECT_EQ(nullptr, tmp_unsealed);
  sealed->data[0] ^= 0xff;

  EXPECT_EQ(SECSuccess, PK11_HPKE_Open(receiver.get(), &aad_item, sealed.get(),
                                       &tmp_unsealed));
  EXPECT_NE(nullptr, tmp_unsealed);
  ScopedSECItem unsealed(tmp_unsealed);
  CheckEquality(&msg_item, unsealed.get());
}

TEST_F(ModeParameterizedTest, InvalidContextParams) {
  HpkeContext *cx =
      PK11_HPKE_NewContext(static_cast<HpkeKemId>(0xff), HpkeKdfHkdfSha256,
                           HpkeAeadChaCha20Poly1305, nullptr, nullptr);
  EXPECT_EQ(nullptr, cx);
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());

  cx = PK11_HPKE_NewContext(HpkeDhKemX25519Sha256, static_cast<HpkeKdfId>(0xff),
                            HpkeAeadChaCha20Poly1305, nullptr, nullptr);
  EXPECT_EQ(nullptr, cx);
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
  cx = PK11_HPKE_NewContext(HpkeDhKemX25519Sha256, HpkeKdfHkdfSha256,
                            static_cast<HpkeAeadId>(0xff), nullptr, nullptr);
  EXPECT_EQ(nullptr, cx);
  EXPECT_EQ(SEC_ERROR_INVALID_ARGS, PORT_GetError());
}

TEST_F(ModeParameterizedTest, InvalidReceiverKeyType) {
  ScopedHpkeContext sender(
      PK11_HPKE_NewContext(HpkeDhKemX25519Sha256, HpkeKdfHkdfSha256,
                           HpkeAeadChaCha20Poly1305, nullptr, nullptr));
  ASSERT_TRUE(!!sender);

  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) {
    ADD_FAILURE() << "No slot";
    return;
  }

  // Give the client an RSA key
  PK11RSAGenParams rsa_param;
  rsa_param.keySizeInBits = 1024;
  rsa_param.pe = 65537L;
  SECKEYPublicKey *pub_tmp;
  ScopedSECKEYPublicKey pub_key;
  ScopedSECKEYPrivateKey priv_key(
      PK11_GenerateKeyPair(slot.get(), CKM_RSA_PKCS_KEY_PAIR_GEN, &rsa_param,
                           &pub_tmp, PR_FALSE, PR_FALSE, nullptr));
  ASSERT_NE(nullptr, priv_key);
  ASSERT_NE(nullptr, pub_tmp);
  pub_key.reset(pub_tmp);

  SECItem info_item = {siBuffer, nullptr, 0};
  EXPECT_EQ(SECFailure, PK11_HPKE_SetupS(sender.get(), nullptr, nullptr,
                                         pub_key.get(), &info_item));
  EXPECT_EQ(SEC_ERROR_BAD_KEY, PORT_GetError());

  // Try with an unexpected curve
  StackSECItem ecParams;
  SECOidData *oidData = SECOID_FindOIDByTag(SEC_OID_ANSIX962_EC_PRIME256V1);
  ASSERT_NE(oidData, nullptr);
  if (!SECITEM_AllocItem(nullptr, &ecParams, (2 + oidData->oid.len))) {
    FAIL() << "Couldn't allocate memory for OID.";
  }
  ecParams.data[0] = SEC_ASN1_OBJECT_ID;
  ecParams.data[1] = oidData->oid.len;
  memcpy(ecParams.data + 2, oidData->oid.data, oidData->oid.len);

  priv_key.reset(PK11_GenerateKeyPair(slot.get(), CKM_EC_KEY_PAIR_GEN,
                                      &ecParams, &pub_tmp, PR_FALSE, PR_FALSE,
                                      nullptr));
  ASSERT_NE(nullptr, priv_key);
  ASSERT_NE(nullptr, pub_tmp);
  pub_key.reset(pub_tmp);
  EXPECT_EQ(SECFailure, PK11_HPKE_SetupS(sender.get(), nullptr, nullptr,
                                         pub_key.get(), &info_item));
  EXPECT_EQ(SEC_ERROR_BAD_KEY, PORT_GetError());
}

}  // namespace nss_test

#endif
