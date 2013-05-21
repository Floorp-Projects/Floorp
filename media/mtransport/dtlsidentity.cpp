/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "logging.h"
#include "nspr.h"
#include "cryptohi.h"
#include "ssl.h"
#include "keyhi.h"
#include "pk11pub.h"
#include "sechash.h"
#include "nsError.h"
#include "dtlsidentity.h"

namespace mozilla {

MOZ_MTLOG_MODULE("mtransport")

DtlsIdentity::~DtlsIdentity() {
  // XXX: make cert_ a smart pointer to avoid this, after we figure
  // out the linking problem.
  if (cert_)
    CERT_DestroyCertificate(cert_);
}

TemporaryRef<DtlsIdentity> DtlsIdentity::Generate() {

  ScopedPK11SlotInfo slot(PK11_GetInternalSlot());
  if (!slot) {
    return nullptr;
  }

  uint8_t random_name[16];

  SECStatus rv = PK11_GenerateRandomOnSlot(slot, random_name,
                                           sizeof(random_name));
  if (rv != SECSuccess)
    return nullptr;

  std::string name;
  char chunk[3];
  for (size_t i = 0; i < sizeof(random_name); ++i) {
    PR_snprintf(chunk, sizeof(chunk), "%.2x", random_name[i]);
    name += chunk;
  }

  std::string subject_name_string = "CN=" + name;
  ScopedCERTName subject_name(CERT_AsciiToName(subject_name_string.c_str()));
  if (!subject_name) {
    return nullptr;
  }

  PK11RSAGenParams rsaparams;
  rsaparams.keySizeInBits = 1024; // TODO: make this stronger when we
                                  // pre-generate.
  rsaparams.pe = 65537; // We are too paranoid to use 3 as the exponent.

  ScopedSECKEYPrivateKey private_key;
  ScopedSECKEYPublicKey public_key;
  SECKEYPublicKey *pubkey;

  private_key =
      PK11_GenerateKeyPair(slot,
                           CKM_RSA_PKCS_KEY_PAIR_GEN, &rsaparams, &pubkey,
                           PR_FALSE, PR_TRUE, nullptr);
  if (private_key == nullptr)
    return nullptr;
  public_key = pubkey;

  ScopedCERTSubjectPublicKeyInfo spki(
      SECKEY_CreateSubjectPublicKeyInfo(pubkey));
  if (!spki) {
    return nullptr;
  }

  ScopedCERTCertificateRequest certreq(
      CERT_CreateCertificateRequest(subject_name, spki, nullptr));
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

  ScopedCERTValidity validity(CERT_CreateValidity(notBefore, notAfter));
  if (!validity) {
    return nullptr;
  }

  unsigned long serial;
  // Note: This serial in principle could collide, but it's unlikely
  rv = PK11_GenerateRandomOnSlot(slot,
                                 reinterpret_cast<unsigned char *>(&serial),
                                 sizeof(serial));
  if (rv != SECSuccess) {
    return nullptr;
  }

  ScopedCERTCertificate certificate(
      CERT_CreateCertificate(serial, subject_name, validity, certreq));
  if (!certificate) {
    return nullptr;
  }

  PLArenaPool *arena = certificate->arena;

  rv = SECOID_SetAlgorithmID(arena, &certificate->signature,
                             SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION, 0);
  if (rv != SECSuccess)
    return nullptr;

  // Set version to X509v3.
  *(certificate->version.data) = SEC_CERTIFICATE_VERSION_3;
  certificate->version.len = 1;

  SECItem innerDER;
  innerDER.len = 0;
  innerDER.data = nullptr;

  if (!SEC_ASN1EncodeItem(arena, &innerDER, certificate,
                          SEC_ASN1_GET(CERT_CertificateTemplate))) {
    return nullptr;
  }

  SECItem *signedCert = PORT_ArenaZNew(arena, SECItem);
  if (!signedCert) {
    return nullptr;
  }

  rv = SEC_DerSignData(arena, signedCert, innerDER.data, innerDER.len,
                       private_key,
                       SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION);
  if (rv != SECSuccess) {
    return nullptr;
  }
  certificate->derCert = *signedCert;

  return new DtlsIdentity(private_key.forget(), certificate.forget());
}


nsresult DtlsIdentity::ComputeFingerprint(const std::string algorithm,
                                          unsigned char *digest,
                                          std::size_t size,
                                          std::size_t *digest_length) {
  MOZ_ASSERT(cert_);

  return ComputeFingerprint(cert_, algorithm, digest, size, digest_length);
}

nsresult DtlsIdentity::ComputeFingerprint(const CERTCertificate *cert,
                                          const std::string algorithm,
                                          unsigned char *digest,
                                          std::size_t size,
                                          std::size_t *digest_length) {
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
  if (!ho)
    return NS_ERROR_INVALID_ARG;

  MOZ_ASSERT(ho->length >= 20);  // Double check

  if (size < ho->length)
    return NS_ERROR_INVALID_ARG;

  SECStatus rv = HASH_HashBuf(ho->type, digest,
                              cert->derCert.data,
                              cert->derCert.len);
  if (rv != SECSuccess)
    return NS_ERROR_FAILURE;

  *digest_length = ho->length;

  return NS_OK;
}

// Format the fingerprint in RFC 4572 Section 5 format, colons and
// all.
std::string DtlsIdentity::FormatFingerprint(const unsigned char *digest,
                                            std::size_t size) {
  std::string str("");
  char group[3];

  for (std::size_t i=0; i < size; i++) {
    PR_snprintf(group, sizeof(group), "%.2X", digest[i]);
    if (i != 0){
      str += ":";
    }
    str += group;
  }

  MOZ_ASSERT(str.size() == (size * 3 - 1));  // Check result length
  return str;
}

// Parse a fingerprint in RFC 4572 format.
// Note that this tolerates some badly formatted data, in particular:
// (a) arbitrary runs of colons
// (b) colons at the beginning or end.
nsresult DtlsIdentity::ParseFingerprint(const std::string fp,
                                        unsigned char *digest,
                                        size_t size,
                                        size_t *length) {
  size_t offset = 0;
  bool top_half = true;
  uint8_t val = 0;

  for (size_t i=0; i<fp.length(); i++) {
    if (offset >= size) {
      // Note: no known way for offset to get > size
      MOZ_MTLOG(PR_LOG_ERROR, "Fingerprint too long for buffer");
      return NS_ERROR_INVALID_ARG;
    }

    if (top_half && (fp[i] == ':')) {
      continue;
    } else if ((fp[i] >= '0') && (fp[i] <= '9')) {
      val |= fp[i] - '0';
    } else if ((fp[i] >= 'A') && (fp[i] <= 'F')) {
      val |= fp[i] - 'A' + 10;
    } else {
      MOZ_MTLOG(PR_LOG_ERROR, "Invalid fingerprint value " << fp[i]);
      return NS_ERROR_ILLEGAL_VALUE;
    }

    if (top_half) {
      val <<= 4;
      top_half = false;
    } else {
      digest[offset++] = val;
      top_half = true;
      val = 0;
    }
  }

  *length = offset;

  return NS_OK;
}

}  // close namespace
