/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if HAVE_OPENSSL_SSL_H

#include "webrtc/base/opensslidentity.h"

// Must be included first before openssl headers.
#include "webrtc/base/win32.h"  // NOLINT

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/crypto.h>

#include "webrtc/base/checks.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/openssl.h"
#include "webrtc/base/openssldigest.h"

namespace rtc {

// We could have exposed a myriad of parameters for the crypto stuff,
// but keeping it simple seems best.

// Strength of generated keys. Those are RSA.
static const int KEY_LENGTH = 1024;

// Random bits for certificate serial number
static const int SERIAL_RAND_BITS = 64;

// Certificate validity lifetime
static const int CERTIFICATE_LIFETIME = 60*60*24*30;  // 30 days, arbitrarily
// Certificate validity window.
// This is to compensate for slightly incorrect system clocks.
static const int CERTIFICATE_WINDOW = -60*60*24;

// Generate a key pair. Caller is responsible for freeing the returned object.
static EVP_PKEY* MakeKey() {
  LOG(LS_INFO) << "Making key pair";
  EVP_PKEY* pkey = EVP_PKEY_new();
  // RSA_generate_key is deprecated. Use _ex version.
  BIGNUM* exponent = BN_new();
  RSA* rsa = RSA_new();
  if (!pkey || !exponent || !rsa ||
      !BN_set_word(exponent, 0x10001) ||  // 65537 RSA exponent
      !RSA_generate_key_ex(rsa, KEY_LENGTH, exponent, NULL) ||
      !EVP_PKEY_assign_RSA(pkey, rsa)) {
    EVP_PKEY_free(pkey);
    BN_free(exponent);
    RSA_free(rsa);
    return NULL;
  }
  // ownership of rsa struct was assigned, don't free it.
  BN_free(exponent);
  LOG(LS_INFO) << "Returning key pair";
  return pkey;
}

// Generate a self-signed certificate, with the public key from the
// given key pair. Caller is responsible for freeing the returned object.
static X509* MakeCertificate(EVP_PKEY* pkey, const SSLIdentityParams& params) {
  LOG(LS_INFO) << "Making certificate for " << params.common_name;
  X509* x509 = NULL;
  BIGNUM* serial_number = NULL;
  X509_NAME* name = NULL;

  if ((x509=X509_new()) == NULL)
    goto error;

  if (!X509_set_pubkey(x509, pkey))
    goto error;

  // serial number
  // temporary reference to serial number inside x509 struct
  ASN1_INTEGER* asn1_serial_number;
  if ((serial_number = BN_new()) == NULL ||
      !BN_pseudo_rand(serial_number, SERIAL_RAND_BITS, 0, 0) ||
      (asn1_serial_number = X509_get_serialNumber(x509)) == NULL ||
      !BN_to_ASN1_INTEGER(serial_number, asn1_serial_number))
    goto error;

  if (!X509_set_version(x509, 0L))  // version 1
    goto error;

  // There are a lot of possible components for the name entries. In
  // our P2P SSL mode however, the certificates are pre-exchanged
  // (through the secure XMPP channel), and so the certificate
  // identification is arbitrary. It can't be empty, so we set some
  // arbitrary common_name. Note that this certificate goes out in
  // clear during SSL negotiation, so there may be a privacy issue in
  // putting anything recognizable here.
  if ((name = X509_NAME_new()) == NULL ||
      !X509_NAME_add_entry_by_NID(
          name, NID_commonName, MBSTRING_UTF8,
          (unsigned char*)params.common_name.c_str(), -1, -1, 0) ||
      !X509_set_subject_name(x509, name) ||
      !X509_set_issuer_name(x509, name))
    goto error;

  if (!X509_gmtime_adj(X509_get_notBefore(x509), params.not_before) ||
      !X509_gmtime_adj(X509_get_notAfter(x509), params.not_after))
    goto error;

  if (!X509_sign(x509, pkey, EVP_sha1()))
    goto error;

  BN_free(serial_number);
  X509_NAME_free(name);
  LOG(LS_INFO) << "Returning certificate";
  return x509;

 error:
  BN_free(serial_number);
  X509_NAME_free(name);
  X509_free(x509);
  return NULL;
}

// This dumps the SSL error stack to the log.
static void LogSSLErrors(const std::string& prefix) {
  char error_buf[200];
  unsigned long err;

  while ((err = ERR_get_error()) != 0) {
    ERR_error_string_n(err, error_buf, sizeof(error_buf));
    LOG(LS_ERROR) << prefix << ": " << error_buf << "\n";
  }
}

OpenSSLKeyPair* OpenSSLKeyPair::Generate() {
  EVP_PKEY* pkey = MakeKey();
  if (!pkey) {
    LogSSLErrors("Generating key pair");
    return NULL;
  }
  return new OpenSSLKeyPair(pkey);
}

OpenSSLKeyPair::~OpenSSLKeyPair() {
  EVP_PKEY_free(pkey_);
}

OpenSSLKeyPair* OpenSSLKeyPair::GetReference() {
  AddReference();
  return new OpenSSLKeyPair(pkey_);
}

void OpenSSLKeyPair::AddReference() {
  CRYPTO_add(&pkey_->references, 1, CRYPTO_LOCK_EVP_PKEY);
}

#ifdef _DEBUG
// Print a certificate to the log, for debugging.
static void PrintCert(X509* x509) {
  BIO* temp_memory_bio = BIO_new(BIO_s_mem());
  if (!temp_memory_bio) {
    LOG_F(LS_ERROR) << "Failed to allocate temporary memory bio";
    return;
  }
  X509_print_ex(temp_memory_bio, x509, XN_FLAG_SEP_CPLUS_SPC, 0);
  BIO_write(temp_memory_bio, "\0", 1);
  char* buffer;
  BIO_get_mem_data(temp_memory_bio, &buffer);
  LOG(LS_VERBOSE) << buffer;
  BIO_free(temp_memory_bio);
}
#endif

OpenSSLCertificate* OpenSSLCertificate::Generate(
    OpenSSLKeyPair* key_pair, const SSLIdentityParams& params) {
  SSLIdentityParams actual_params(params);
  if (actual_params.common_name.empty()) {
    // Use a random string, arbitrarily 8chars long.
    actual_params.common_name = CreateRandomString(8);
  }
  X509* x509 = MakeCertificate(key_pair->pkey(), actual_params);
  if (!x509) {
    LogSSLErrors("Generating certificate");
    return NULL;
  }
#ifdef _DEBUG
  PrintCert(x509);
#endif
  OpenSSLCertificate* ret = new OpenSSLCertificate(x509);
  X509_free(x509);
  return ret;
}

OpenSSLCertificate* OpenSSLCertificate::FromPEMString(
    const std::string& pem_string) {
  BIO* bio = BIO_new_mem_buf(const_cast<char*>(pem_string.c_str()), -1);
  if (!bio)
    return NULL;
  BIO_set_mem_eof_return(bio, 0);
  X509 *x509 = PEM_read_bio_X509(bio, NULL, NULL,
                                 const_cast<char*>("\0"));
  BIO_free(bio);  // Frees the BIO, but not the pointed-to string.

  if (!x509)
    return NULL;

  OpenSSLCertificate* ret = new OpenSSLCertificate(x509);
  X509_free(x509);
  return ret;
}

// NOTE: This implementation only functions correctly after InitializeSSL
// and before CleanupSSL.
bool OpenSSLCertificate::GetSignatureDigestAlgorithm(
    std::string* algorithm) const {
  int nid = OBJ_obj2nid(x509_->sig_alg->algorithm);
  switch (nid) {
    case NID_md5WithRSA:
    case NID_md5WithRSAEncryption:
      *algorithm = DIGEST_MD5;
      break;
    case NID_ecdsa_with_SHA1:
    case NID_dsaWithSHA1:
    case NID_dsaWithSHA1_2:
    case NID_sha1WithRSA:
    case NID_sha1WithRSAEncryption:
      *algorithm = DIGEST_SHA_1;
      break;
    case NID_ecdsa_with_SHA224:
    case NID_sha224WithRSAEncryption:
    case NID_dsa_with_SHA224:
      *algorithm = DIGEST_SHA_224;
      break;
    case NID_ecdsa_with_SHA256:
    case NID_sha256WithRSAEncryption:
    case NID_dsa_with_SHA256:
      *algorithm = DIGEST_SHA_256;
      break;
    case NID_ecdsa_with_SHA384:
    case NID_sha384WithRSAEncryption:
      *algorithm = DIGEST_SHA_384;
      break;
    case NID_ecdsa_with_SHA512:
    case NID_sha512WithRSAEncryption:
      *algorithm = DIGEST_SHA_512;
      break;
    default:
      // Unknown algorithm.  There are several unhandled options that are less
      // common and more complex.
      LOG(LS_ERROR) << "Unknown signature algorithm NID: " << nid;
      algorithm->clear();
      return false;
  }
  return true;
}

bool OpenSSLCertificate::GetChain(SSLCertChain** chain) const {
  // Chains are not yet supported when using OpenSSL.
  // OpenSSLStreamAdapter::SSLVerifyCallback currently requires the remote
  // certificate to be self-signed.
  return false;
}

bool OpenSSLCertificate::ComputeDigest(const std::string& algorithm,
                                       unsigned char* digest,
                                       size_t size,
                                       size_t* length) const {
  return ComputeDigest(x509_, algorithm, digest, size, length);
}

bool OpenSSLCertificate::ComputeDigest(const X509* x509,
                                       const std::string& algorithm,
                                       unsigned char* digest,
                                       size_t size,
                                       size_t* length) {
  const EVP_MD *md;
  unsigned int n;

  if (!OpenSSLDigest::GetDigestEVP(algorithm, &md))
    return false;

  if (size < static_cast<size_t>(EVP_MD_size(md)))
    return false;

  X509_digest(x509, md, digest, &n);

  *length = n;

  return true;
}

OpenSSLCertificate::~OpenSSLCertificate() {
  X509_free(x509_);
}

OpenSSLCertificate* OpenSSLCertificate::GetReference() const {
  return new OpenSSLCertificate(x509_);
}

std::string OpenSSLCertificate::ToPEMString() const {
  BIO* bio = BIO_new(BIO_s_mem());
  if (!bio) {
    FATAL() << "unreachable code";
  }
  if (!PEM_write_bio_X509(bio, x509_)) {
    BIO_free(bio);
    FATAL() << "unreachable code";
  }
  BIO_write(bio, "\0", 1);
  char* buffer;
  BIO_get_mem_data(bio, &buffer);
  std::string ret(buffer);
  BIO_free(bio);
  return ret;
}

void OpenSSLCertificate::ToDER(Buffer* der_buffer) const {
  // In case of failure, make sure to leave the buffer empty.
  der_buffer->SetData(NULL, 0);

  // Calculates the DER representation of the certificate, from scratch.
  BIO* bio = BIO_new(BIO_s_mem());
  if (!bio) {
    FATAL() << "unreachable code";
  }
  if (!i2d_X509_bio(bio, x509_)) {
    BIO_free(bio);
    FATAL() << "unreachable code";
  }
  char* data;
  size_t length = BIO_get_mem_data(bio, &data);
  der_buffer->SetData(data, length);
  BIO_free(bio);
}

void OpenSSLCertificate::AddReference() const {
  ASSERT(x509_ != NULL);
  CRYPTO_add(&x509_->references, 1, CRYPTO_LOCK_X509);
}

OpenSSLIdentity::OpenSSLIdentity(OpenSSLKeyPair* key_pair,
                                 OpenSSLCertificate* certificate)
    : key_pair_(key_pair), certificate_(certificate) {
  ASSERT(key_pair != NULL);
  ASSERT(certificate != NULL);
}

OpenSSLIdentity::~OpenSSLIdentity() = default;

OpenSSLIdentity* OpenSSLIdentity::GenerateInternal(
    const SSLIdentityParams& params) {
  OpenSSLKeyPair *key_pair = OpenSSLKeyPair::Generate();
  if (key_pair) {
    OpenSSLCertificate *certificate = OpenSSLCertificate::Generate(
        key_pair, params);
    if (certificate)
      return new OpenSSLIdentity(key_pair, certificate);
    delete key_pair;
  }
  LOG(LS_INFO) << "Identity generation failed";
  return NULL;
}

OpenSSLIdentity* OpenSSLIdentity::Generate(const std::string& common_name) {
  SSLIdentityParams params;
  params.common_name = common_name;
  params.not_before = CERTIFICATE_WINDOW;
  params.not_after = CERTIFICATE_LIFETIME;
  return GenerateInternal(params);
}

OpenSSLIdentity* OpenSSLIdentity::GenerateForTest(
    const SSLIdentityParams& params) {
  return GenerateInternal(params);
}

SSLIdentity* OpenSSLIdentity::FromPEMStrings(
    const std::string& private_key,
    const std::string& certificate) {
  scoped_ptr<OpenSSLCertificate> cert(
      OpenSSLCertificate::FromPEMString(certificate));
  if (!cert) {
    LOG(LS_ERROR) << "Failed to create OpenSSLCertificate from PEM string.";
    return NULL;
  }

  BIO* bio = BIO_new_mem_buf(const_cast<char*>(private_key.c_str()), -1);
  if (!bio) {
    LOG(LS_ERROR) << "Failed to create a new BIO buffer.";
    return NULL;
  }
  BIO_set_mem_eof_return(bio, 0);
  EVP_PKEY *pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL,
                                           const_cast<char*>("\0"));
  BIO_free(bio);  // Frees the BIO, but not the pointed-to string.

  if (!pkey) {
    LOG(LS_ERROR) << "Failed to create the private key from PEM string.";
    return NULL;
  }

  return new OpenSSLIdentity(new OpenSSLKeyPair(pkey),
                             cert.release());
}

const OpenSSLCertificate& OpenSSLIdentity::certificate() const {
  return *certificate_;
}

OpenSSLIdentity* OpenSSLIdentity::GetReference() const {
  return new OpenSSLIdentity(key_pair_->GetReference(),
                             certificate_->GetReference());
}

bool OpenSSLIdentity::ConfigureIdentity(SSL_CTX* ctx) {
  // 1 is the documented success return code.
  if (SSL_CTX_use_certificate(ctx, certificate_->x509()) != 1 ||
     SSL_CTX_use_PrivateKey(ctx, key_pair_->pkey()) != 1) {
    LogSSLErrors("Configuring key and certificate");
    return false;
  }
  return true;
}

}  // namespace rtc

#endif  // HAVE_OPENSSL_SSL_H
