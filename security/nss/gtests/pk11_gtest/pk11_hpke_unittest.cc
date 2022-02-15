/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <memory>
#include "blapi.h"
#include "gtest/gtest.h"
#include "json.h"
#include "nss.h"
#include "nss_scoped_ptrs.h"
#include "pk11hpke.h"
#include "pk11pub.h"
#include "secerr.h"
#include "sechash.h"
#include "util.h"

extern std::string g_source_dir;

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

  void Seal(const ScopedHpkeContext &cx, const std::vector<uint8_t> &aad_vec,
            const std::vector<uint8_t> &pt_vec,
            std::vector<uint8_t> *out_sealed) {
    SECItem aad_item = {siBuffer, toUcharPtr(aad_vec.data()),
                        static_cast<unsigned int>(aad_vec.size())};
    SECItem pt_item = {siBuffer, toUcharPtr(pt_vec.data()),
                       static_cast<unsigned int>(pt_vec.size())};

    SECItem *sealed_item = nullptr;
    EXPECT_EQ(SECSuccess,
              PK11_HPKE_Seal(cx.get(), &aad_item, &pt_item, &sealed_item));
    ASSERT_NE(nullptr, sealed_item);
    ScopedSECItem sealed(sealed_item);
    out_sealed->assign(sealed->data, sealed->data + sealed->len);
  }

  void Open(const ScopedHpkeContext &cx, const std::vector<uint8_t> &aad_vec,
            const std::vector<uint8_t> &ct_vec,
            std::vector<uint8_t> *out_opened) {
    SECItem aad_item = {siBuffer, toUcharPtr(aad_vec.data()),
                        static_cast<unsigned int>(aad_vec.size())};
    SECItem ct_item = {siBuffer, toUcharPtr(ct_vec.data()),
                       static_cast<unsigned int>(ct_vec.size())};
    SECItem *opened_item = nullptr;
    EXPECT_EQ(SECSuccess,
              PK11_HPKE_Open(cx.get(), &aad_item, &ct_item, &opened_item));
    ASSERT_NE(nullptr, opened_item);
    ScopedSECItem opened(opened_item);
    out_opened->assign(opened->data, opened->data + opened->len);
  }

  void SealOpen(const ScopedHpkeContext &sender,
                const ScopedHpkeContext &receiver,
                const std::vector<uint8_t> &msg,
                const std::vector<uint8_t> &aad,
                const std::vector<uint8_t> *expect) {
    std::vector<uint8_t> sealed;
    std::vector<uint8_t> opened;
    Seal(sender, aad, msg, &sealed);
    if (expect) {
      EXPECT_EQ(*expect, sealed);
    }
    Open(receiver, aad, sealed, &opened);
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

struct HpkeEncryptVector {
  std::vector<uint8_t> pt;
  std::vector<uint8_t> aad;
  std::vector<uint8_t> ct;

  static std::vector<HpkeEncryptVector> ReadVec(JsonReader &r) {
    std::vector<HpkeEncryptVector> all;

    while (r.NextItemArray()) {
      HpkeEncryptVector enc;
      while (r.NextItem()) {
        std::string n = r.ReadLabel();
        if (n == "") {
          break;
        }
        if (n == "plaintext") {
          enc.pt = r.ReadHex();
        } else if (n == "aad") {
          enc.aad = r.ReadHex();
        } else if (n == "ciphertext") {
          enc.ct = r.ReadHex();
        } else {
          r.SkipValue();
        }
      }
      all.push_back(enc);
    }

    return all;
  }
};

struct HpkeExportVector {
  std::vector<uint8_t> ctxt;
  size_t len;
  std::vector<uint8_t> exported;

  static std::vector<HpkeExportVector> ReadVec(JsonReader &r) {
    std::vector<HpkeExportVector> all;

    while (r.NextItemArray()) {
      HpkeExportVector exp;
      while (r.NextItem()) {
        std::string n = r.ReadLabel();
        if (n == "") {
          break;
        }
        if (n == "exporter_context") {
          exp.ctxt = r.ReadHex();
        } else if (n == "L") {
          exp.len = r.ReadInt();
        } else if (n == "exported_value") {
          exp.exported = r.ReadHex();
        } else {
          r.SkipValue();
        }
      }
      all.push_back(exp);
    }

    return all;
  }
};

struct HpkeVector {
  uint32_t test_id;
  HpkeModeId mode;
  HpkeKemId kem_id;
  HpkeKdfId kdf_id;
  HpkeAeadId aead_id;
  std::vector<uint8_t> info;
  std::vector<uint8_t> pkcs8_e;
  std::vector<uint8_t> pkcs8_r;
  std::vector<uint8_t> psk;
  std::vector<uint8_t> psk_id;
  std::vector<uint8_t> enc;
  std::vector<uint8_t> key;
  std::vector<uint8_t> nonce;
  std::vector<HpkeEncryptVector> encryptions;
  std::vector<HpkeExportVector> exports;

  static std::vector<uint8_t> Pkcs8(const std::vector<uint8_t> &sk,
                                    const std::vector<uint8_t> &pk) {
    // Only X25519 format.
    std::vector<uint8_t> v(105);
    v.assign({
        0x30, 0x67, 0x02, 0x01, 0x00, 0x30, 0x14, 0x06, 0x07, 0x2a, 0x86, 0x48,
        0xce, 0x3d, 0x02, 0x01, 0x06, 0x09, 0x2b, 0x06, 0x01, 0x04, 0x01, 0xda,
        0x47, 0x0f, 0x01, 0x04, 0x4c, 0x30, 0x4a, 0x02, 0x01, 0x01, 0x04, 0x20,
    });
    v.insert(v.end(), sk.begin(), sk.end());
    v.insert(v.end(), {
                          0xa1, 0x23, 0x03, 0x21, 0x00,
                      });
    v.insert(v.end(), pk.begin(), pk.end());
    return v;
  }

  static std::vector<HpkeVector> Read(JsonReader &r) {
    std::vector<HpkeVector> all_tests;
    uint32_t test_id = 0;

    while (r.NextItemArray()) {
      HpkeVector vec = {0};
      uint32_t fields = 0;
      enum class RequiredFields {
        mode,
        kem,
        kdf,
        aead,
        skEm,
        skRm,
        pkEm,
        pkRm,
        all
      };
      std::vector<uint8_t> sk_e, pk_e, sk_r, pk_r;
      test_id++;

      while (r.NextItem()) {
        std::string n = r.ReadLabel();
        if (n == "") {
          break;
        }
        if (n == "mode") {
          vec.mode = static_cast<HpkeModeId>(r.ReadInt());
          fields |= 1 << static_cast<uint32_t>(RequiredFields::mode);
        } else if (n == "kem_id") {
          vec.kem_id = static_cast<HpkeKemId>(r.ReadInt());
          fields |= 1 << static_cast<uint32_t>(RequiredFields::kem);
        } else if (n == "kdf_id") {
          vec.kdf_id = static_cast<HpkeKdfId>(r.ReadInt());
          fields |= 1 << static_cast<uint32_t>(RequiredFields::kdf);
        } else if (n == "aead_id") {
          vec.aead_id = static_cast<HpkeAeadId>(r.ReadInt());
          fields |= 1 << static_cast<uint32_t>(RequiredFields::aead);
        } else if (n == "info") {
          vec.info = r.ReadHex();
        } else if (n == "skEm") {
          sk_e = r.ReadHex();
          fields |= 1 << static_cast<uint32_t>(RequiredFields::skEm);
        } else if (n == "pkEm") {
          pk_e = r.ReadHex();
          fields |= 1 << static_cast<uint32_t>(RequiredFields::pkEm);
        } else if (n == "skRm") {
          sk_r = r.ReadHex();
          fields |= 1 << static_cast<uint32_t>(RequiredFields::skRm);
        } else if (n == "pkRm") {
          pk_r = r.ReadHex();
          fields |= 1 << static_cast<uint32_t>(RequiredFields::pkRm);
        } else if (n == "psk") {
          vec.psk = r.ReadHex();
        } else if (n == "psk_id") {
          vec.psk_id = r.ReadHex();
        } else if (n == "enc") {
          vec.enc = r.ReadHex();
        } else if (n == "key") {
          vec.key = r.ReadHex();
        } else if (n == "base_nonce") {
          vec.nonce = r.ReadHex();
        } else if (n == "encryptions") {
          vec.encryptions = HpkeEncryptVector::ReadVec(r);
        } else if (n == "exports") {
          vec.exports = HpkeExportVector::ReadVec(r);
        } else {
          r.SkipValue();
        }
      }

      if (fields != (1 << static_cast<uint32_t>(RequiredFields::all)) - 1) {
        std::cerr << "Skipping entry " << test_id << " for missing fields"
                  << std::endl;
        continue;
      }
      // Skip modes and configurations we don't support.
      if (vec.mode != HpkeModeBase && vec.mode != HpkeModePsk) {
        continue;
      }
      SECStatus rv =
          PK11_HPKE_ValidateParameters(vec.kem_id, vec.kdf_id, vec.aead_id);
      if (rv != SECSuccess) {
        continue;
      }

      vec.test_id = test_id;
      vec.pkcs8_e = HpkeVector::Pkcs8(sk_e, pk_e);
      vec.pkcs8_r = HpkeVector::Pkcs8(sk_r, pk_r);
      all_tests.push_back(vec);
    }

    return all_tests;
  }
};

class TestVectors : public HpkeTest, public ::testing::Test {
  struct Endpoint {
    bool init(const HpkeVector &vec, const std::vector<uint8_t> &sk_data) {
      ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
      if (!slot) {
        ADD_FAILURE() << "No slot";
        return false;
      }

      cx_ = Endpoint::MakeContext(slot, vec);

      SECItem item = {siBuffer, toUcharPtr(sk_data.data()),
                      static_cast<unsigned int>(sk_data.size())};
      SECKEYPrivateKey *sk = nullptr;
      SECStatus rv = PK11_ImportDERPrivateKeyInfoAndReturnKey(
          slot.get(), &item, nullptr, nullptr, false, false, KU_ALL, &sk,
          nullptr);
      if (rv != SECSuccess) {
        ADD_FAILURE() << "Failed to import secret";
        return false;
      }
      sk_.reset(sk);
      SECKEYPublicKey *pk = SECKEY_ConvertToPublicKey(sk_.get());
      pk_.reset(pk);
      return cx_ && sk_ && pk_;
    }

    static ScopedHpkeContext MakeContext(const ScopedPK11SlotInfo &slot,
                                         const HpkeVector &vec) {
      ScopedPK11SymKey psk = Endpoint::ReadPsk(slot, vec);
      SECItem psk_id_item = {siBuffer, toUcharPtr(vec.psk_id.data()),
                             static_cast<unsigned int>(vec.psk_id.size())};
      SECItem *psk_id = psk ? &psk_id_item : nullptr;
      return ScopedHpkeContext(PK11_HPKE_NewContext(
          vec.kem_id, vec.kdf_id, vec.aead_id, psk.get(), psk_id));
    }

    static ScopedPK11SymKey ReadPsk(const ScopedPK11SlotInfo &slot,
                                    const HpkeVector &vec) {
      ScopedPK11SymKey psk;
      if (!vec.psk.empty()) {
        SECItem psk_item = {siBuffer, toUcharPtr(vec.psk.data()),
                            static_cast<unsigned int>(vec.psk.size())};
        PK11SymKey *psk_key =
            PK11_ImportSymKey(slot.get(), CKM_HKDF_KEY_GEN, PK11_OriginUnwrap,
                              CKA_WRAP, &psk_item, nullptr);
        EXPECT_NE(nullptr, psk_key);
        psk.reset(psk_key);
      }
      return psk;
    }

    ScopedHpkeContext cx_;
    ScopedSECKEYPublicKey pk_;
    ScopedSECKEYPrivateKey sk_;
  };

 protected:
  void TestExports(const HpkeVector &vec, const Endpoint &sender,
                   const Endpoint &receiver) {
    for (auto &exp : vec.exports) {
      SECItem context_item = {siBuffer, toUcharPtr(exp.ctxt.data()),
                              static_cast<unsigned int>(exp.ctxt.size())};
      PK11SymKey *actual_r = nullptr;
      PK11SymKey *actual_s = nullptr;
      ASSERT_EQ(SECSuccess,
                PK11_HPKE_ExportSecret(sender.cx_.get(), &context_item, exp.len,
                                       &actual_s));
      ASSERT_EQ(SECSuccess,
                PK11_HPKE_ExportSecret(receiver.cx_.get(), &context_item,
                                       exp.len, &actual_r));
      ScopedPK11SymKey scoped_act_s(actual_s);
      ScopedPK11SymKey scoped_act_r(actual_r);
      CheckEquality(exp.exported, scoped_act_s.get());
      CheckEquality(exp.exported, scoped_act_r.get());
    }
  }

  void TestEncryptions(const HpkeVector &vec, const Endpoint &sender,
                       const Endpoint &receiver) {
    for (auto &enc : vec.encryptions) {
      SealOpen(sender.cx_, receiver.cx_, enc.pt, enc.aad, &enc.ct);
    }
  }

  void SetupS(const ScopedHpkeContext &cx, const ScopedSECKEYPublicKey &pkE,
              const ScopedSECKEYPrivateKey &skE,
              const ScopedSECKEYPublicKey &pkR,
              const std::vector<uint8_t> &info) {
    SECItem info_item = {siBuffer, toUcharPtr(info.data()),
                         static_cast<unsigned int>(info.size())};
    EXPECT_EQ(SECSuccess, PK11_HPKE_SetupS(cx.get(), pkE.get(), skE.get(),
                                           pkR.get(), &info_item));
  }

  void SetupR(const ScopedHpkeContext &cx, const ScopedSECKEYPublicKey &pkR,
              const ScopedSECKEYPrivateKey &skR,
              const std::vector<uint8_t> &enc,
              const std::vector<uint8_t> &info) {
    SECItem enc_item = {siBuffer, toUcharPtr(enc.data()),
                        static_cast<unsigned int>(enc.size())};
    SECItem info_item = {siBuffer, toUcharPtr(info.data()),
                         static_cast<unsigned int>(info.size())};
    EXPECT_EQ(SECSuccess, PK11_HPKE_SetupR(cx.get(), pkR.get(), skR.get(),
                                           &enc_item, &info_item));
  }

  void SetupSenderReceiver(const HpkeVector &vec, const Endpoint &sender,
                           const Endpoint &receiver) {
    SetupS(sender.cx_, sender.pk_, sender.sk_, receiver.pk_, vec.info);
    uint8_t buf[32];  // Curve25519 only, fixed size.
    SECItem encap_item = {siBuffer, const_cast<uint8_t *>(buf), sizeof(buf)};
    ASSERT_EQ(SECSuccess, PK11_HPKE_Serialize(sender.pk_.get(), encap_item.data,
                                              &encap_item.len, encap_item.len));
    CheckEquality(vec.enc, &encap_item);
    SetupR(receiver.cx_, receiver.pk_, receiver.sk_, vec.enc, vec.info);
  }

  void RunTestVector(const HpkeVector &vec) {
    Endpoint sender;
    ASSERT_TRUE(sender.init(vec, vec.pkcs8_e));
    Endpoint receiver;
    ASSERT_TRUE(receiver.init(vec, vec.pkcs8_r));

    SetupSenderReceiver(vec, sender, receiver);
    TestEncryptions(vec, sender, receiver);
    TestExports(vec, sender, receiver);
  }
};

TEST_F(TestVectors, HpkeVectors) {
  JsonReader r(::g_source_dir + "/hpke-vectors.json");
  auto all_tests = HpkeVector::Read(r);
  for (auto &vec : all_tests) {
    std::cout << "HPKE vector " << vec.test_id << std::endl;
    RunTestVector(vec);
  }
}

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
