/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nss.h"
#include "secoid.h"

#include "nss_scoped_ptrs.h"

namespace nss_test {

class ParamHolder;

class Pkcs11KeyPairGenerator {
 public:
  Pkcs11KeyPairGenerator(CK_MECHANISM_TYPE mech, SECOidTag curve_oid)
      : mech_(mech), curve_(curve_oid) {}
  Pkcs11KeyPairGenerator(CK_MECHANISM_TYPE mech)
      : Pkcs11KeyPairGenerator(mech, SEC_OID_UNKNOWN) {}

  CK_MECHANISM_TYPE mechanism() const { return mech_; }
  SECOidTag curve() const { return curve_; }

  void GenerateKey(ScopedSECKEYPrivateKey* priv_key,
                   ScopedSECKEYPublicKey* pub_key) const;

 private:
  std::unique_ptr<ParamHolder> MakeParams() const;

  CK_MECHANISM_TYPE mech_;
  SECOidTag curve_;
};

}  // namespace nss_test
