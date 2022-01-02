/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "CTVerifyResult.h"
#include "PSMIPCCommon.h"

namespace mozilla {
namespace psm {

SECItem* WrapPrivateKeyInfoWithEmptyPassword(
    SECKEYPrivateKey* pk) /* encrypt this private key */
{
  if (!pk) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return nullptr;
  }

  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) {
    return nullptr;
  }

  // For private keys, NSS cannot export anything other than RSA, but we need EC
  // also. So, we use the private key encryption function to serialize instead,
  // using a hard-coded dummy password; this is not intended to provide any
  // additional security, it just works around a limitation in NSS.
  SECItem dummyPassword = {siBuffer, nullptr, 0};
  UniqueSECKEYEncryptedPrivateKeyInfo epki(PK11_ExportEncryptedPrivKeyInfo(
      slot.get(), SEC_OID_AES_128_CBC, &dummyPassword, pk, 1, nullptr));

  if (!epki) {
    return nullptr;
  }

  return SEC_ASN1EncodeItem(
      nullptr, nullptr, epki.get(),
      NSS_Get_SECKEY_EncryptedPrivateKeyInfoTemplate(nullptr, false));
}

SECStatus UnwrapPrivateKeyInfoWithEmptyPassword(
    SECItem* derPKI, const UniqueCERTCertificate& aCert,
    SECKEYPrivateKey** privk) {
  if (!derPKI || !aCert || !privk) {
    PR_SetError(SEC_ERROR_INVALID_ARGS, 0);
    return SECFailure;
  }

  UniqueSECKEYPublicKey publicKey(CERT_ExtractPublicKey(aCert.get()));
  // This is a pointer to data inside publicKey
  SECItem* publicValue = nullptr;
  switch (publicKey->keyType) {
    case dsaKey:
      publicValue = &publicKey->u.dsa.publicValue;
      break;
    case dhKey:
      publicValue = &publicKey->u.dh.publicValue;
      break;
    case rsaKey:
      publicValue = &publicKey->u.rsa.modulus;
      break;
    case ecKey:
      publicValue = &publicKey->u.ec.publicValue;
      break;
    default:
      MOZ_ASSERT(false);
      PR_SetError(SSL_ERROR_BAD_CERTIFICATE, 0);
      return SECFailure;
  }

  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) {
    return SECFailure;
  }

  UniquePLArenaPool temparena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  if (!temparena) {
    return SECFailure;
  }

  SECKEYEncryptedPrivateKeyInfo* epki =
      PORT_ArenaZNew(temparena.get(), SECKEYEncryptedPrivateKeyInfo);
  if (!epki) {
    return SECFailure;
  }

  SECStatus rv = SEC_ASN1DecodeItem(
      temparena.get(), epki,
      NSS_Get_SECKEY_EncryptedPrivateKeyInfoTemplate(nullptr, false), derPKI);
  if (rv != SECSuccess) {
    // If SEC_ASN1DecodeItem fails, we cannot assume anything about the
    // validity of the data in epki. The best we can do is free the arena
    // and return.
    return rv;
  }

  // See comment in WrapPrivateKeyInfoWithEmptyPassword about this
  // dummy password stuff.
  SECItem dummyPassword = {siBuffer, nullptr, 0};
  return PK11_ImportEncryptedPrivateKeyInfoAndReturnKey(
      slot.get(), epki, &dummyPassword, nullptr, publicValue, false, false,
      publicKey->keyType, KU_ALL, privk, nullptr);
}

void SerializeClientCertAndKey(const UniqueCERTCertificate& aCert,
                               const UniqueSECKEYPrivateKey& aKey,
                               ByteArray& aOutSerializedCert,
                               ByteArray& aOutSerializedKey) {
  if (!aCert || !aKey) {
    return;
  }

  UniqueSECItem derPki(WrapPrivateKeyInfoWithEmptyPassword(aKey.get()));
  if (!derPki) {
    return;
  }

  aOutSerializedCert.data().AppendElements(aCert->derCert.data,
                                           aCert->derCert.len);
  aOutSerializedKey.data().AppendElements(derPki->data, derPki->len);
}

void DeserializeClientCertAndKey(const ByteArray& aSerializedCert,
                                 const ByteArray& aSerializedKey,
                                 UniqueCERTCertificate& aOutCert,
                                 UniqueSECKEYPrivateKey& aOutKey) {
  if (aSerializedCert.data().IsEmpty() || aSerializedKey.data().IsEmpty()) {
    return;
  }

  SECItem item = {siBuffer,
                  const_cast<uint8_t*>(aSerializedCert.data().Elements()),
                  static_cast<unsigned int>(aSerializedCert.data().Length())};

  UniqueCERTCertificate cert(CERT_NewTempCertificate(
      CERT_GetDefaultCertDB(), &item, nullptr, false, true));

  if (!cert) {
    return;
  }

  SECItem derPKI = {siBuffer,
                    const_cast<uint8_t*>(aSerializedKey.data().Elements()),
                    static_cast<unsigned int>(aSerializedKey.data().Length())};

  SECKEYPrivateKey* privateKey;
  if (UnwrapPrivateKeyInfoWithEmptyPassword(&derPKI, cert, &privateKey) !=
      SECSuccess) {
    MOZ_ASSERT(false);
    return;
  }

  aOutCert = std::move(cert);
  aOutKey.reset(privateKey);
}

}  // namespace psm
}  // namespace mozilla
