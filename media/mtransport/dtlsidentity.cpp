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

#include "mozilla/Sprintf.h"

namespace mozilla {

RefPtr<DtlsIdentity> DtlsIdentity::Generate() {
  UniquePK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) {
    return nullptr;
  }

  uint8_t random_name[16];

  SECStatus rv = PK11_GenerateRandomOnSlot(slot.get(), random_name,
                                           sizeof(random_name));
  if (rv != SECSuccess)
    return nullptr;

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

  unsigned char paramBuf[12]; // OIDs are small
  SECItem ecdsaParams = { siBuffer, paramBuf, sizeof(paramBuf) };
  SECOidData* oidData = SECOID_FindOIDByTag(SEC_OID_SECG_EC_SECP256R1);
  if (!oidData || (oidData->oid.len > (sizeof(paramBuf) - 2))) {
    return nullptr;
  }
  ecdsaParams.data[0] = SEC_ASN1_OBJECT_ID;
  ecdsaParams.data[1] = oidData->oid.len;
  memcpy(ecdsaParams.data + 2, oidData->oid.data, oidData->oid.len);
  ecdsaParams.len = oidData->oid.len + 2;

  SECKEYPublicKey *pubkey;
  UniqueSECKEYPrivateKey private_key(
      PK11_GenerateKeyPair(slot.get(),
                           CKM_EC_KEY_PAIR_GEN, &ecdsaParams, &pubkey,
                           PR_FALSE, PR_TRUE, nullptr));
  if (private_key == nullptr)
    return nullptr;
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
  static const PRTime oneDay = PRTime(PR_USEC_PER_SEC)
                             * PRTime(60)  // sec
                             * PRTime(60)  // min
                             * PRTime(24); // hours
  PRTime now = PR_Now();
  PRTime notBefore = now - oneDay;
  PRTime notAfter = now + (PRTime(30) * oneDay);

  UniqueCERTValidity validity(CERT_CreateValidity(notBefore, notAfter));
  if (!validity) {
    return nullptr;
  }

  unsigned long serial;
  // Note: This serial in principle could collide, but it's unlikely
  rv = PK11_GenerateRandomOnSlot(slot.get(),
                                 reinterpret_cast<unsigned char *>(&serial),
                                 sizeof(serial));
  if (rv != SECSuccess) {
    return nullptr;
  }

  UniqueCERTCertificate certificate(
      CERT_CreateCertificate(serial, subject_name.get(), validity.get(),
                             certreq.get()));
  if (!certificate) {
    return nullptr;
  }

  PLArenaPool *arena = certificate->arena;

  rv = SECOID_SetAlgorithmID(arena, &certificate->signature,
                             SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE, nullptr);
  if (rv != SECSuccess)
    return nullptr;

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

  SECItem *signedCert = PORT_ArenaZNew(arena, SECItem);
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

  RefPtr<DtlsIdentity> identity = new DtlsIdentity(std::move(private_key),
                                                   std::move(certificate),
                                                   ssl_kea_ecdh);
  return identity.forget();
}

const std::string DtlsIdentity::DEFAULT_HASH_ALGORITHM = "sha-256";

nsresult DtlsIdentity::ComputeFingerprint(const std::string algorithm,
                                          uint8_t *digest,
                                          size_t size,
                                          size_t *digest_length) const {
  const UniqueCERTCertificate& c = cert();
  MOZ_ASSERT(c);

  return ComputeFingerprint(c, algorithm, digest, size, digest_length);
}

nsresult DtlsIdentity::ComputeFingerprint(const UniqueCERTCertificate& cert,
                                          const std::string algorithm,
                                          uint8_t *digest,
                                          size_t size,
                                          size_t *digest_length) {
  MOZ_ASSERT(cert);

  HASH_HashType ht;

  if (algorithm == "sha-1") {
    ht = HASH_AlgSHA1;
  } else if (algorithm == "sha-224") {
    ht = HASH_AlgSHA224;
  } else if (algorithm == "sha-256") {
    ht = HASH_AlgSHA256;
  } else if (algorithm == "sha-384") {
    ht = HASH_AlgSHA384;
  }  else if (algorithm == "sha-512") {
    ht = HASH_AlgSHA512;
  } else {
    return NS_ERROR_INVALID_ARG;
  }

  const SECHashObject *ho = HASH_GetHashObject(ht);
  MOZ_ASSERT(ho);
  if (!ho) {
    return NS_ERROR_INVALID_ARG;
  }

  MOZ_ASSERT(ho->length >= 20);  // Double check

  if (size < ho->length) {
    return NS_ERROR_INVALID_ARG;
  }

  SECStatus rv = HASH_HashBuf(ho->type, digest,
                              cert->derCert.data,
                              cert->derCert.len);
  if (rv != SECSuccess) {
    return NS_ERROR_FAILURE;
  }

  *digest_length = ho->length;

  return NS_OK;
}

}  // close namespace
