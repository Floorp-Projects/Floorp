/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "dtlsidentity.h"

#include "cert.h"
#include "cryptohi.h"
#include "keyhi.h"
#include "nsError.h"
#include "pk11pub.h"
#include "sechash.h"
#include "ssl.h"
#include "mozpkix/nss_scoped_ptrs.h"
#include "secerr.h"

#include "mozilla/Sprintf.h"
#include "mozilla/dom/CryptoBuffer.h"
#include "mozilla/dom/CryptoKey.h"
#include "ipc/IPCMessageUtils.h"

namespace mozilla {

// TODO(bug 1522632): This function should be moved to NSS
static SECItem* ExportDEREncryptedPrivateKeyInfo(
    PK11SlotInfo* slot,    /* optional, encrypt key in this slot */
    SECOidTag algTag,      /* encrypt key with this algorithm */
    const SECItem* pwitem, /* password for PBE encryption */
    SECKEYPrivateKey* pk,  /* encrypt this private key */
    int iteration,         /* interations for PBE alg */
    void* wincx)           /* context for password callback ? */
{
  SECKEYEncryptedPrivateKeyInfo* pki = PK11_ExportEncryptedPrivKeyInfo(
      slot, algTag, const_cast<SECItem*>(pwitem), pk, iteration, wincx);
  SECItem* derPKI;

  if (!pki) {
    return NULL;
  }
  derPKI = SEC_ASN1EncodeItem(
      NULL, NULL, pki,
      NSS_Get_SECKEY_EncryptedPrivateKeyInfoTemplate(NULL, PR_FALSE));
  SECKEY_DestroyEncryptedPrivateKeyInfo(pki, PR_TRUE);
  return derPKI;
}

// This function should be moved to NSS
static SECStatus ImportDEREncryptedPrivateKeyInfoAndReturnKey(
    PK11SlotInfo* slot, SECItem* derPKI, SECItem* pwitem, SECItem* nickname,
    SECItem* publicValue, PRBool isPerm, PRBool isPrivate, KeyType type,
    unsigned int keyUsage, SECKEYPrivateKey** privk, void* wincx) {
  SECKEYEncryptedPrivateKeyInfo* epki = NULL;
  PLArenaPool* temparena = NULL;
  SECStatus rv = SECFailure;

  temparena = PORT_NewArena(DER_DEFAULT_CHUNKSIZE);
  if (!temparena) return rv;
  epki = PORT_ArenaZNew(temparena, SECKEYEncryptedPrivateKeyInfo);
  if (!epki) {
    PORT_FreeArena(temparena, PR_FALSE);
    return rv;
  }
  epki->arena = temparena;

  rv = SEC_ASN1DecodeItem(
      epki->arena, epki,
      NSS_Get_SECKEY_EncryptedPrivateKeyInfoTemplate(NULL, PR_FALSE), derPKI);
  if (rv != SECSuccess) {
    /* If SEC_ASN1DecodeItem fails, we cannot assume anything about the
     * validity of the data in epki. The best we can do is free the arena
     * and return. */
    PORT_FreeArena(temparena, PR_TRUE);
    return rv;
  }
  if (epki->encryptedData.data == NULL) {
    /* If SEC_ASN1DecodeItems succeeds but
     * SECKEYEncryptedPrivateKeyInfo.encryptedData is a zero-length octet
     * string, free the arena and return a failure to avoid trying to zero the
     * corresponding SECItem in SECKEY_DestroyPrivateKeyInfo(). */
    PORT_FreeArena(temparena, PR_TRUE);
    PORT_SetError(SEC_ERROR_BAD_KEY);
    return SECFailure;
  }

  rv = PK11_ImportEncryptedPrivateKeyInfoAndReturnKey(
      slot, epki, pwitem, nickname, publicValue, isPerm, isPrivate, type,
      keyUsage, privk, wincx);

  /* this zeroes the key and frees the arena */
  SECKEY_DestroyEncryptedPrivateKeyInfo(epki, PR_TRUE /*freeit*/);
  return rv;
}

nsresult DtlsIdentity::Serialize(nsTArray<uint8_t>* aKeyDer,
                                 nsTArray<uint8_t>* aCertDer) {
  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) {
    MOZ_ASSERT(false);
    return NS_ERROR_FAILURE;
  }

  // For private keys, NSS cannot export anything other than SHA, but we need EC
  // also. So, we use the private key encryption function to serialize instead,
  // using a hard-coded dummy password; this is not intended to provide any
  // additional security, it just works around a limitation in NSS.
  SECItem dummyPassword = {siBuffer, nullptr, 0};
  ScopedSECItem derPki(ExportDEREncryptedPrivateKeyInfo(
      slot.get(), SEC_OID_AES_128_CBC, &dummyPassword, private_key_.get(), 1,
      nullptr));
  if (!derPki) {
    return NS_ERROR_FAILURE;
  }

  aKeyDer->AppendElements(derPki->data, derPki->len);
  aCertDer->AppendElements(cert_->derCert.data, cert_->derCert.len);
  return NS_OK;
}

/* static */
RefPtr<DtlsIdentity> DtlsIdentity::Deserialize(
    const nsTArray<uint8_t>& aKeyDer, const nsTArray<uint8_t>& aCertDer,
    SSLKEAType authType) {
  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) {
    MOZ_ASSERT(false);
    return nullptr;
  }

  SECItem certDer = {siBuffer, const_cast<uint8_t*>(aCertDer.Elements()),
                     static_cast<unsigned int>(aCertDer.Length())};
  UniqueCERTCertificate cert(CERT_NewTempCertificate(
      CERT_GetDefaultCertDB(), &certDer, nullptr, true, true));

  // Extract the public value needed for decryption of private key
  // (why did we not need this for encryption?)
  ScopedSECKEYPublicKey publicKey(CERT_ExtractPublicKey(cert.get()));
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
      return nullptr;
  }

  SECItem derPKI = {siBuffer, const_cast<uint8_t*>(aKeyDer.Elements()),
                    static_cast<unsigned int>(aKeyDer.Length())};

  // See comment in Serialize about this dummy password stuff
  SECItem dummyPassword = {siBuffer, nullptr, 0};
  SECKEYPrivateKey* privateKey;
  if (ImportDEREncryptedPrivateKeyInfoAndReturnKey(
          slot.get(), &derPKI, &dummyPassword, nullptr, publicValue, false,
          false, publicKey->keyType, KU_ALL, &privateKey,
          nullptr) != SECSuccess) {
    MOZ_ASSERT(false);
    return nullptr;
  }

  return new DtlsIdentity(UniqueSECKEYPrivateKey(privateKey), std::move(cert),
                          authType);
}

RefPtr<DtlsIdentity> DtlsIdentity::Generate() {
  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) {
    return nullptr;
  }

  uint8_t random_name[16];

  SECStatus rv =
      PK11_GenerateRandomOnSlot(slot.get(), random_name, sizeof(random_name));
  if (rv != SECSuccess) return nullptr;

  std::string name;
  char chunk[3];
  for (unsigned char r_name : random_name) {
    SprintfLiteral(chunk, "%.2x", r_name);
    name += chunk;
  }

  std::string subject_name_string = "CN=" + name;
  UniqueCERTName subject_name(CERT_AsciiToName(subject_name_string.c_str()));
  if (!subject_name) {
    return nullptr;
  }

  unsigned char paramBuf[12];  // OIDs are small
  SECItem ecdsaParams = {siBuffer, paramBuf, sizeof(paramBuf)};
  SECOidData* oidData = SECOID_FindOIDByTag(SEC_OID_SECG_EC_SECP256R1);
  if (!oidData || (oidData->oid.len > (sizeof(paramBuf) - 2))) {
    return nullptr;
  }
  ecdsaParams.data[0] = SEC_ASN1_OBJECT_ID;
  ecdsaParams.data[1] = oidData->oid.len;
  memcpy(ecdsaParams.data + 2, oidData->oid.data, oidData->oid.len);
  ecdsaParams.len = oidData->oid.len + 2;

  SECKEYPublicKey* pubkey;
  UniqueSECKEYPrivateKey private_key(
      PK11_GenerateKeyPair(slot.get(), CKM_EC_KEY_PAIR_GEN, &ecdsaParams,
                           &pubkey, PR_FALSE, PR_TRUE, nullptr));
  if (private_key == nullptr) return nullptr;
  UniqueSECKEYPublicKey public_key(pubkey);
  pubkey = nullptr;

  UniqueCERTSubjectPublicKeyInfo spki(
      SECKEY_CreateSubjectPublicKeyInfo(public_key.get()));
  if (!spki) {
    return nullptr;
  }

  UniqueCERTCertificateRequest certreq(
      CERT_CreateCertificateRequest(subject_name.get(), spki.get(), nullptr));
  if (!certreq) {
    return nullptr;
  }

  // From 1 day before todayto 30 days after.
  // This is a sort of arbitrary range designed to be valid
  // now with some slack in case the other side expects
  // some before expiry.
  //
  // Note: explicit casts necessary to avoid
  //       warning C4307: '*' : integral constant overflow
  static const PRTime oneDay = PRTime(PR_USEC_PER_SEC) * PRTime(60)  // sec
                               * PRTime(60)                          // min
                               * PRTime(24);                         // hours
  PRTime now = PR_Now();
  PRTime notBefore = now - oneDay;
  PRTime notAfter = now + (PRTime(30) * oneDay);

  UniqueCERTValidity validity(CERT_CreateValidity(notBefore, notAfter));
  if (!validity) {
    return nullptr;
  }

  unsigned long serial;
  // Note: This serial in principle could collide, but it's unlikely
  rv = PK11_GenerateRandomOnSlot(
      slot.get(), reinterpret_cast<unsigned char*>(&serial), sizeof(serial));
  if (rv != SECSuccess) {
    return nullptr;
  }

  UniqueCERTCertificate certificate(CERT_CreateCertificate(
      serial, subject_name.get(), validity.get(), certreq.get()));
  if (!certificate) {
    return nullptr;
  }

  PLArenaPool* arena = certificate->arena;

  rv = SECOID_SetAlgorithmID(arena, &certificate->signature,
                             SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE, nullptr);
  if (rv != SECSuccess) return nullptr;

  // Set version to X509v3.
  *(certificate->version.data) = SEC_CERTIFICATE_VERSION_3;
  certificate->version.len = 1;

  SECItem innerDER;
  innerDER.len = 0;
  innerDER.data = nullptr;

  if (!SEC_ASN1EncodeItem(arena, &innerDER, certificate.get(),
                          SEC_ASN1_GET(CERT_CertificateTemplate))) {
    return nullptr;
  }

  SECItem* signedCert = PORT_ArenaZNew(arena, SECItem);
  if (!signedCert) {
    return nullptr;
  }

  rv = SEC_DerSignData(arena, signedCert, innerDER.data, innerDER.len,
                       private_key.get(),
                       SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE);
  if (rv != SECSuccess) {
    return nullptr;
  }
  certificate->derCert = *signedCert;

  RefPtr<DtlsIdentity> identity = new DtlsIdentity(
      std::move(private_key), std::move(certificate), ssl_kea_ecdh);
  return identity.forget();
}

const std::string DtlsIdentity::DEFAULT_HASH_ALGORITHM = "sha-256";

nsresult DtlsIdentity::ComputeFingerprint(DtlsDigest* digest) const {
  const UniqueCERTCertificate& c = cert();
  MOZ_ASSERT(c);

  return ComputeFingerprint(c, digest);
}

nsresult DtlsIdentity::ComputeFingerprint(const UniqueCERTCertificate& cert,
                                          DtlsDigest* digest) {
  MOZ_ASSERT(cert);

  HASH_HashType ht;

  if (digest->algorithm_ == "sha-1") {
    ht = HASH_AlgSHA1;
  } else if (digest->algorithm_ == "sha-224") {
    ht = HASH_AlgSHA224;
  } else if (digest->algorithm_ == "sha-256") {
    ht = HASH_AlgSHA256;
  } else if (digest->algorithm_ == "sha-384") {
    ht = HASH_AlgSHA384;
  } else if (digest->algorithm_ == "sha-512") {
    ht = HASH_AlgSHA512;
  } else {
    return NS_ERROR_INVALID_ARG;
  }

  const SECHashObject* ho = HASH_GetHashObject(ht);
  MOZ_ASSERT(ho);
  if (!ho) {
    return NS_ERROR_INVALID_ARG;
  }

  MOZ_ASSERT(ho->length >= 20);  // Double check
  digest->value_.resize(ho->length);

  SECStatus rv = HASH_HashBuf(ho->type, digest->value_.data(),
                              cert->derCert.data, cert->derCert.len);
  if (rv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

}  // namespace mozilla
