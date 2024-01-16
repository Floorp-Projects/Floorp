/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "kyber.h"
#include "pk11_keygen.h"

#include "pk11pub.h"
#include "pk11pqg.h"
#include "prerror.h"

#include "gtest/gtest.h"

namespace nss_test {

class ParamHolder {
 public:
  virtual void* get() = 0;
  virtual ~ParamHolder() = default;

 protected:
  ParamHolder() = default;
};

void Pkcs11KeyPairGenerator::GenerateKey(ScopedSECKEYPrivateKey* priv_key,
                                         ScopedSECKEYPublicKey* pub_key,
                                         bool sensitive) const {
  // This function returns if an assertion fails, so don't leak anything.
  priv_key->reset(nullptr);
  pub_key->reset(nullptr);

  auto params = MakeParams();
  ASSERT_NE(nullptr, params);

  ScopedPK11SlotInfo slot(PK11_GetInternalKeySlot());
  ASSERT_TRUE(slot);

  SECKEYPublicKey* pub_tmp;
  ScopedSECKEYPrivateKey priv_tmp(
      PK11_GenerateKeyPair(slot.get(), mech_, params->get(), &pub_tmp, PR_FALSE,
                           sensitive ? PR_TRUE : PR_FALSE, nullptr));
  ASSERT_NE(nullptr, priv_tmp)
      << "PK11_GenerateKeyPair failed: " << PORT_ErrorToName(PORT_GetError());
  ASSERT_NE(nullptr, pub_tmp);

  priv_key->swap(priv_tmp);
  pub_key->reset(pub_tmp);
}

class RsaParamHolder : public ParamHolder {
 public:
  RsaParamHolder() : params_({1024, 0x010001}) {}
  ~RsaParamHolder() = default;

  void* get() override { return &params_; }

 private:
  PK11RSAGenParams params_;
};

class PqgParamHolder : public ParamHolder {
 public:
  PqgParamHolder(PQGParams* params) : params_(params) {}
  ~PqgParamHolder() = default;

  void* get() override { return params_.get(); }

 private:
  ScopedPQGParams params_;
};

class DhParamHolder : public PqgParamHolder {
 public:
  DhParamHolder(PQGParams* params)
      : PqgParamHolder(params),
        params_({nullptr, params->prime, params->base}) {}
  ~DhParamHolder() = default;

  void* get() override { return &params_; }

 private:
  SECKEYDHParams params_;
};

class EcParamHolder : public ParamHolder {
 public:
  EcParamHolder(SECOidTag curve_oid) {
    SECOidData* curve = SECOID_FindOIDByTag(curve_oid);
    EXPECT_NE(nullptr, curve);

    size_t plen = curve->oid.len + 2;
    extra_.reset(new uint8_t[plen]);
    extra_[0] = SEC_ASN1_OBJECT_ID;
    extra_[1] = static_cast<uint8_t>(curve->oid.len);
    memcpy(&extra_[2], curve->oid.data, curve->oid.len);

    ec_params_ = {siBuffer, extra_.get(), static_cast<unsigned int>(plen)};
  }
  ~EcParamHolder() = default;

  void* get() override { return &ec_params_; }

 private:
  SECKEYECParams ec_params_;
  std::unique_ptr<uint8_t[]> extra_;
};

class KyberParamHolder : public ParamHolder {
 public:
  KyberParamHolder(CK_NSS_KEM_PARAMETER_SET_TYPE aParams) : mParams(aParams) {}
  void* get() override { return &mParams; }

 private:
  CK_NSS_KEM_PARAMETER_SET_TYPE mParams;
};

std::unique_ptr<ParamHolder> Pkcs11KeyPairGenerator::MakeParams() const {
  switch (mech_) {
    case CKM_RSA_PKCS_KEY_PAIR_GEN:
      std::cerr << "Generate RSA pair" << std::endl;
      return std::unique_ptr<ParamHolder>(new RsaParamHolder());

    case CKM_DSA_KEY_PAIR_GEN:
    case CKM_DH_PKCS_KEY_PAIR_GEN: {
      PQGParams* pqg_params = nullptr;
      PQGVerify* pqg_verify = nullptr;
      const unsigned int key_size = 1024;
      SECStatus rv = PK11_PQG_ParamGenV2(key_size, 0, key_size / 16,
                                         &pqg_params, &pqg_verify);
      if (rv != SECSuccess) {
        ADD_FAILURE() << "PK11_PQG_ParamGenV2 failed";
        return nullptr;
      }
      EXPECT_NE(nullptr, pqg_verify);
      EXPECT_NE(nullptr, pqg_params);
      PK11_PQG_DestroyVerify(pqg_verify);
      if (mech_ == CKM_DSA_KEY_PAIR_GEN) {
        std::cerr << "Generate DSA pair" << std::endl;
        return std::unique_ptr<ParamHolder>(new PqgParamHolder(pqg_params));
      }
      std::cerr << "Generate DH pair" << std::endl;
      return std::unique_ptr<ParamHolder>(new DhParamHolder(pqg_params));
    }

    case CKM_EC_KEY_PAIR_GEN:
      std::cerr << "Generate EC pair on " << curve_ << std::endl;
      return std::unique_ptr<ParamHolder>(new EcParamHolder(curve_));

    case CKM_NSS_KYBER_KEY_PAIR_GEN:
      std::cerr << "Generate Kyber768 pair" << std::endl;
      return std::unique_ptr<ParamHolder>(
          new KyberParamHolder(CKP_NSS_KYBER_768_ROUND3));

    default:
      ADD_FAILURE() << "unknown OID " << mech_;
  }
  return nullptr;
}

}  // namespace nss_test
