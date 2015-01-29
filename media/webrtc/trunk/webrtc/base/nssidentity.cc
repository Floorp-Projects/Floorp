/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <string>
#include <vector>

#if HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG_H

#if HAVE_NSS_SSL_H

#include "webrtc/base/nssidentity.h"

#include "cert.h"
#include "cryptohi.h"
#include "keyhi.h"
#include "nss.h"
#include "pk11pub.h"
#include "sechash.h"

#include "webrtc/base/logging.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/nssstreamadapter.h"
#include "webrtc/base/safe_conversions.h"

namespace rtc {

// Certificate validity lifetime in seconds.
static const int CERTIFICATE_LIFETIME = 60*60*24*30;  // 30 days, arbitrarily
// Certificate validity window in seconds.
// This is to compensate for slightly incorrect system clocks.
static const int CERTIFICATE_WINDOW = -60*60*24;

NSSKeyPair::~NSSKeyPair() {
  if (privkey_)
    SECKEY_DestroyPrivateKey(privkey_);
  if (pubkey_)
    SECKEY_DestroyPublicKey(pubkey_);
}

NSSKeyPair *NSSKeyPair::Generate() {
  SECKEYPrivateKey *privkey = NULL;
  SECKEYPublicKey *pubkey = NULL;
  PK11RSAGenParams rsaparams;
  rsaparams.keySizeInBits = 1024;
  rsaparams.pe = 0x010001;  // 65537 -- a common RSA public exponent.

  privkey = PK11_GenerateKeyPair(NSSContext::GetSlot(),
                                 CKM_RSA_PKCS_KEY_PAIR_GEN,
                                 &rsaparams, &pubkey, PR_FALSE /*permanent*/,
                                 PR_FALSE /*sensitive*/, NULL);
  if (!privkey) {
    LOG(LS_ERROR) << "Couldn't generate key pair";
    return NULL;
  }

  return new NSSKeyPair(privkey, pubkey);
}

// Just make a copy.
NSSKeyPair *NSSKeyPair::GetReference() {
  SECKEYPrivateKey *privkey = SECKEY_CopyPrivateKey(privkey_);
  if (!privkey)
    return NULL;

  SECKEYPublicKey *pubkey = SECKEY_CopyPublicKey(pubkey_);
  if (!pubkey) {
    SECKEY_DestroyPrivateKey(privkey);
    return NULL;
  }

  return new NSSKeyPair(privkey, pubkey);
}

NSSCertificate::NSSCertificate(CERTCertificate* cert)
    : certificate_(CERT_DupCertificate(cert)) {
  ASSERT(certificate_ != NULL);
}

static void DeleteCert(SSLCertificate* cert) {
  delete cert;
}

NSSCertificate::NSSCertificate(CERTCertList* cert_list) {
  // Copy the first cert into certificate_.
  CERTCertListNode* node = CERT_LIST_HEAD(cert_list);
  certificate_ = CERT_DupCertificate(node->cert);

  // Put any remaining certificates into the chain.
  node = CERT_LIST_NEXT(node);
  std::vector<SSLCertificate*> certs;
  for (; !CERT_LIST_END(node, cert_list); node = CERT_LIST_NEXT(node)) {
    certs.push_back(new NSSCertificate(node->cert));
  }

  if (!certs.empty())
    chain_.reset(new SSLCertChain(certs));

  // The SSLCertChain constructor copies its input, so now we have to delete
  // the originals.
  std::for_each(certs.begin(), certs.end(), DeleteCert);
}

NSSCertificate::NSSCertificate(CERTCertificate* cert, SSLCertChain* chain)
    : certificate_(CERT_DupCertificate(cert)) {
  ASSERT(certificate_ != NULL);
  if (chain)
    chain_.reset(chain->Copy());
}


NSSCertificate *NSSCertificate::FromPEMString(const std::string &pem_string) {
  std::string der;
  if (!SSLIdentity::PemToDer(kPemTypeCertificate, pem_string, &der))
    return NULL;

  SECItem der_cert;
  der_cert.data = reinterpret_cast<unsigned char *>(const_cast<char *>(
      der.data()));
  der_cert.len = checked_cast<unsigned int>(der.size());
  CERTCertificate *cert = CERT_NewTempCertificate(CERT_GetDefaultCertDB(),
      &der_cert, NULL, PR_FALSE, PR_TRUE);

  if (!cert)
    return NULL;

  NSSCertificate* ret = new NSSCertificate(cert);
  CERT_DestroyCertificate(cert);
  return ret;
}

NSSCertificate *NSSCertificate::GetReference() const {
  return new NSSCertificate(certificate_, chain_.get());
}

std::string NSSCertificate::ToPEMString() const {
  return SSLIdentity::DerToPem(kPemTypeCertificate,
                               certificate_->derCert.data,
                               certificate_->derCert.len);
}

void NSSCertificate::ToDER(Buffer* der_buffer) const {
  der_buffer->SetData(certificate_->derCert.data, certificate_->derCert.len);
}

static bool Certifies(CERTCertificate* parent, CERTCertificate* child) {
  // TODO(bemasc): Identify stricter validation checks to use here.  In the
  // context of some future identity standard, it might make sense to check
  // the certificates' roles, expiration dates, self-signatures (if
  // self-signed), certificate transparency logging, or many other attributes.
  // NOTE: Future changes to this validation may reject some previously allowed
  // certificate chains.  Users should be advised not to deploy chained
  // certificates except in controlled environments until the validity
  // requirements are finalized.

  // Check that the parent's name is the same as the child's claimed issuer.
  SECComparison name_status =
      CERT_CompareName(&child->issuer, &parent->subject);
  if (name_status != SECEqual)
    return false;

  // Extract the parent's public key, or fail if the key could not be read
  // (e.g. certificate is corrupted).
  SECKEYPublicKey* parent_key = CERT_ExtractPublicKey(parent);
  if (!parent_key)
    return false;

  // Check that the parent's privkey was actually used to generate the child's
  // signature.
  SECStatus verified = CERT_VerifySignedDataWithPublicKey(
      &child->signatureWrap, parent_key, NULL);
  SECKEY_DestroyPublicKey(parent_key);
  return verified == SECSuccess;
}

bool NSSCertificate::IsValidChain(const CERTCertList* cert_list) {
  CERTCertListNode* child = CERT_LIST_HEAD(cert_list);
  for (CERTCertListNode* parent = CERT_LIST_NEXT(child);
       !CERT_LIST_END(parent, cert_list);
       child = parent, parent = CERT_LIST_NEXT(parent)) {
    if (!Certifies(parent->cert, child->cert))
      return false;
  }
  return true;
}

bool NSSCertificate::GetDigestLength(const std::string& algorithm,
                                     size_t* length) {
  const SECHashObject *ho;

  if (!GetDigestObject(algorithm, &ho))
    return false;

  *length = ho->length;

  return true;
}

bool NSSCertificate::GetSignatureDigestAlgorithm(std::string* algorithm) const {
  // The function sec_DecodeSigAlg in NSS provides this mapping functionality.
  // Unfortunately it is private, so the functionality must be duplicated here.
  // See https://bugzilla.mozilla.org/show_bug.cgi?id=925165 .
  SECOidTag sig_alg = SECOID_GetAlgorithmTag(&certificate_->signature);
  switch (sig_alg) {
    case SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION:
      *algorithm = DIGEST_MD5;
      break;
    case SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION:
    case SEC_OID_ISO_SHA_WITH_RSA_SIGNATURE:
    case SEC_OID_ISO_SHA1_WITH_RSA_SIGNATURE:
    case SEC_OID_ANSIX9_DSA_SIGNATURE_WITH_SHA1_DIGEST:
    case SEC_OID_BOGUS_DSA_SIGNATURE_WITH_SHA1_DIGEST:
    case SEC_OID_ANSIX962_ECDSA_SHA1_SIGNATURE:
    case SEC_OID_MISSI_DSS:
    case SEC_OID_MISSI_KEA_DSS:
    case SEC_OID_MISSI_KEA_DSS_OLD:
    case SEC_OID_MISSI_DSS_OLD:
      *algorithm = DIGEST_SHA_1;
      break;
    case SEC_OID_ANSIX962_ECDSA_SHA224_SIGNATURE:
    case SEC_OID_PKCS1_SHA224_WITH_RSA_ENCRYPTION:
    case SEC_OID_NIST_DSA_SIGNATURE_WITH_SHA224_DIGEST:
      *algorithm = DIGEST_SHA_224;
      break;
    case SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE:
    case SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION:
    case SEC_OID_NIST_DSA_SIGNATURE_WITH_SHA256_DIGEST:
      *algorithm = DIGEST_SHA_256;
      break;
    case SEC_OID_ANSIX962_ECDSA_SHA384_SIGNATURE:
    case SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION:
      *algorithm = DIGEST_SHA_384;
      break;
    case SEC_OID_ANSIX962_ECDSA_SHA512_SIGNATURE:
    case SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION:
      *algorithm = DIGEST_SHA_512;
      break;
    default:
      // Unknown algorithm.  There are several unhandled options that are less
      // common and more complex.
      algorithm->clear();
      return false;
  }
  return true;
}

bool NSSCertificate::ComputeDigest(const std::string& algorithm,
                                   unsigned char* digest,
                                   size_t size,
                                   size_t* length) const {
  const SECHashObject *ho;

  if (!GetDigestObject(algorithm, &ho))
    return false;

  if (size < ho->length)  // Sanity check for fit
    return false;

  SECStatus rv = HASH_HashBuf(ho->type, digest,
                              certificate_->derCert.data,
                              certificate_->derCert.len);
  if (rv != SECSuccess)
    return false;

  *length = ho->length;

  return true;
}

bool NSSCertificate::GetChain(SSLCertChain** chain) const {
  if (!chain_)
    return false;

  *chain = chain_->Copy();
  return true;
}

bool NSSCertificate::Equals(const NSSCertificate *tocompare) const {
  if (!certificate_->derCert.len)
    return false;
  if (!tocompare->certificate_->derCert.len)
    return false;

  if (certificate_->derCert.len != tocompare->certificate_->derCert.len)
    return false;

  return memcmp(certificate_->derCert.data,
                tocompare->certificate_->derCert.data,
                certificate_->derCert.len) == 0;
}


bool NSSCertificate::GetDigestObject(const std::string &algorithm,
                                     const SECHashObject **hop) {
  const SECHashObject *ho;
  HASH_HashType hash_type;

  if (algorithm == DIGEST_SHA_1) {
    hash_type = HASH_AlgSHA1;
  // HASH_AlgSHA224 is not supported in the chromium linux build system.
#if 0
  } else if (algorithm == DIGEST_SHA_224) {
    hash_type = HASH_AlgSHA224;
#endif
  } else if (algorithm == DIGEST_SHA_256) {
    hash_type = HASH_AlgSHA256;
  } else if (algorithm == DIGEST_SHA_384) {
    hash_type = HASH_AlgSHA384;
  } else if (algorithm == DIGEST_SHA_512) {
    hash_type = HASH_AlgSHA512;
  } else {
    return false;
  }

  ho = HASH_GetHashObject(hash_type);

  ASSERT(ho->length >= 20);  // Can't happen
  *hop = ho;

  return true;
}


NSSIdentity* NSSIdentity::GenerateInternal(const SSLIdentityParams& params) {
  std::string subject_name_string = "CN=" + params.common_name;
  CERTName *subject_name = CERT_AsciiToName(
      const_cast<char *>(subject_name_string.c_str()));
  NSSIdentity *identity = NULL;
  CERTSubjectPublicKeyInfo *spki = NULL;
  CERTCertificateRequest *certreq = NULL;
  CERTValidity *validity = NULL;
  CERTCertificate *certificate = NULL;
  NSSKeyPair *keypair = NSSKeyPair::Generate();
  SECItem inner_der;
  SECStatus rv;
  PLArenaPool* arena;
  SECItem signed_cert;
  PRTime now = PR_Now();
  PRTime not_before =
      now + static_cast<PRTime>(params.not_before) * PR_USEC_PER_SEC;
  PRTime not_after =
      now + static_cast<PRTime>(params.not_after) * PR_USEC_PER_SEC;

  inner_der.len = 0;
  inner_der.data = NULL;

  if (!keypair) {
    LOG(LS_ERROR) << "Couldn't generate key pair";
    goto fail;
  }

  if (!subject_name) {
    LOG(LS_ERROR) << "Couldn't convert subject name " << subject_name;
    goto fail;
  }

  spki = SECKEY_CreateSubjectPublicKeyInfo(keypair->pubkey());
  if (!spki) {
    LOG(LS_ERROR) << "Couldn't create SPKI";
    goto fail;
  }

  certreq = CERT_CreateCertificateRequest(subject_name, spki, NULL);
  if (!certreq) {
    LOG(LS_ERROR) << "Couldn't create certificate signing request";
    goto fail;
  }

  validity = CERT_CreateValidity(not_before, not_after);
  if (!validity) {
    LOG(LS_ERROR) << "Couldn't create validity";
    goto fail;
  }

  unsigned long serial;
  // Note: This serial in principle could collide, but it's unlikely
  rv = PK11_GenerateRandom(reinterpret_cast<unsigned char *>(&serial),
                           sizeof(serial));
  if (rv != SECSuccess) {
    LOG(LS_ERROR) << "Couldn't generate random serial";
    goto fail;
  }

  certificate = CERT_CreateCertificate(serial, subject_name, validity, certreq);
  if (!certificate) {
    LOG(LS_ERROR) << "Couldn't create certificate";
    goto fail;
  }

  arena = certificate->arena;

  rv = SECOID_SetAlgorithmID(arena, &certificate->signature,
                             SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION, NULL);
  if (rv != SECSuccess)
    goto fail;

  // Set version to X509v3.
  *(certificate->version.data) = 2;
  certificate->version.len = 1;

  if (!SEC_ASN1EncodeItem(arena, &inner_der, certificate,
                          SEC_ASN1_GET(CERT_CertificateTemplate)))
    goto fail;

  rv = SEC_DerSignData(arena, &signed_cert, inner_der.data, inner_der.len,
                       keypair->privkey(),
                       SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION);
  if (rv != SECSuccess) {
    LOG(LS_ERROR) << "Couldn't sign certificate";
    goto fail;
  }
  certificate->derCert = signed_cert;

  identity = new NSSIdentity(keypair, new NSSCertificate(certificate));

  goto done;

 fail:
  delete keypair;

 done:
  if (certificate) CERT_DestroyCertificate(certificate);
  if (subject_name) CERT_DestroyName(subject_name);
  if (spki) SECKEY_DestroySubjectPublicKeyInfo(spki);
  if (certreq) CERT_DestroyCertificateRequest(certreq);
  if (validity) CERT_DestroyValidity(validity);
  return identity;
}

NSSIdentity* NSSIdentity::Generate(const std::string &common_name) {
  SSLIdentityParams params;
  params.common_name = common_name;
  params.not_before = CERTIFICATE_WINDOW;
  params.not_after = CERTIFICATE_LIFETIME;
  return GenerateInternal(params);
}

NSSIdentity* NSSIdentity::GenerateForTest(const SSLIdentityParams& params) {
  return GenerateInternal(params);
}

SSLIdentity* NSSIdentity::FromPEMStrings(const std::string& private_key,
                                         const std::string& certificate) {
  std::string private_key_der;
  if (!SSLIdentity::PemToDer(
      kPemTypeRsaPrivateKey, private_key, &private_key_der))
    return NULL;

  SECItem private_key_item;
  private_key_item.data = reinterpret_cast<unsigned char *>(
      const_cast<char *>(private_key_der.c_str()));
  private_key_item.len = checked_cast<unsigned int>(private_key_der.size());

  const unsigned int key_usage = KU_KEY_ENCIPHERMENT | KU_DATA_ENCIPHERMENT |
      KU_DIGITAL_SIGNATURE;

  SECKEYPrivateKey* privkey = NULL;
  SECStatus rv =
      PK11_ImportDERPrivateKeyInfoAndReturnKey(NSSContext::GetSlot(),
                                               &private_key_item,
                                               NULL, NULL, PR_FALSE, PR_FALSE,
                                               key_usage, &privkey, NULL);
  if (rv != SECSuccess) {
    LOG(LS_ERROR) << "Couldn't import private key";
    return NULL;
  }

  SECKEYPublicKey *pubkey = SECKEY_ConvertToPublicKey(privkey);
  if (rv != SECSuccess) {
    SECKEY_DestroyPrivateKey(privkey);
    LOG(LS_ERROR) << "Couldn't convert private key to public key";
    return NULL;
  }

  // Assign to a scoped_ptr so we don't leak on error.
  scoped_ptr<NSSKeyPair> keypair(new NSSKeyPair(privkey, pubkey));

  scoped_ptr<NSSCertificate> cert(NSSCertificate::FromPEMString(certificate));
  if (!cert) {
    LOG(LS_ERROR) << "Couldn't parse certificate";
    return NULL;
  }

  // TODO(ekr@rtfm.com): Check the public key against the certificate.

  return new NSSIdentity(keypair.release(), cert.release());
}

NSSIdentity *NSSIdentity::GetReference() const {
  NSSKeyPair *keypair = keypair_->GetReference();
  if (!keypair)
    return NULL;

  NSSCertificate *certificate = certificate_->GetReference();
  if (!certificate) {
    delete keypair;
    return NULL;
  }

  return new NSSIdentity(keypair, certificate);
}


NSSCertificate &NSSIdentity::certificate() const {
  return *certificate_;
}


}  // rtc namespace

#endif  // HAVE_NSS_SSL_H

