/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Handling of certificates and keypairs for SSLStreamAdapter's peer mode.
#if HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG_H

#include "webrtc/base/sslidentity.h"

#include <string>

#include "webrtc/base/base64.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/sslconfig.h"

#if SSL_USE_SCHANNEL

#elif SSL_USE_OPENSSL  // !SSL_USE_SCHANNEL

#include "webrtc/base/opensslidentity.h"

#elif SSL_USE_NSS  // !SSL_USE_SCHANNEL && !SSL_USE_OPENSSL

#include "webrtc/base/nssidentity.h"

#endif  // SSL_USE_SCHANNEL

namespace rtc {

const char kPemTypeCertificate[] = "CERTIFICATE";
const char kPemTypeRsaPrivateKey[] = "RSA PRIVATE KEY";

bool SSLIdentity::PemToDer(const std::string& pem_type,
                           const std::string& pem_string,
                           std::string* der) {
  // Find the inner body. We need this to fulfill the contract of
  // returning pem_length.
  size_t header = pem_string.find("-----BEGIN " + pem_type + "-----");
  if (header == std::string::npos)
    return false;

  size_t body = pem_string.find("\n", header);
  if (body == std::string::npos)
    return false;

  size_t trailer = pem_string.find("-----END " + pem_type + "-----");
  if (trailer == std::string::npos)
    return false;

  std::string inner = pem_string.substr(body + 1, trailer - (body + 1));

  *der = Base64::Decode(inner, Base64::DO_PARSE_WHITE |
                        Base64::DO_PAD_ANY |
                        Base64::DO_TERM_BUFFER);
  return true;
}

std::string SSLIdentity::DerToPem(const std::string& pem_type,
                                  const unsigned char* data,
                                  size_t length) {
  std::stringstream result;

  result << "-----BEGIN " << pem_type << "-----\n";

  std::string b64_encoded;
  Base64::EncodeFromArray(data, length, &b64_encoded);

  // Divide the Base-64 encoded data into 64-character chunks, as per
  // 4.3.2.4 of RFC 1421.
  static const size_t kChunkSize = 64;
  size_t chunks = (b64_encoded.size() + (kChunkSize - 1)) / kChunkSize;
  for (size_t i = 0, chunk_offset = 0; i < chunks;
       ++i, chunk_offset += kChunkSize) {
    result << b64_encoded.substr(chunk_offset, kChunkSize);
    result << "\n";
  }

  result << "-----END " << pem_type << "-----\n";

  return result.str();
}

#if SSL_USE_SCHANNEL

SSLCertificate* SSLCertificate::FromPEMString(const std::string& pem_string) {
  return NULL;
}

SSLIdentity* SSLIdentity::Generate(const std::string& common_name) {
  return NULL;
}

SSLIdentity* GenerateForTest(const SSLIdentityParams& params) {
  return NULL;
}

SSLIdentity* SSLIdentity::FromPEMStrings(const std::string& private_key,
                                         const std::string& certificate) {
  return NULL;
}

#elif SSL_USE_OPENSSL  // !SSL_USE_SCHANNEL

SSLCertificate* SSLCertificate::FromPEMString(const std::string& pem_string) {
  return OpenSSLCertificate::FromPEMString(pem_string);
}

SSLIdentity* SSLIdentity::Generate(const std::string& common_name) {
  return OpenSSLIdentity::Generate(common_name);
}

SSLIdentity* SSLIdentity::GenerateForTest(const SSLIdentityParams& params) {
  return OpenSSLIdentity::GenerateForTest(params);
}

SSLIdentity* SSLIdentity::FromPEMStrings(const std::string& private_key,
                                         const std::string& certificate) {
  return OpenSSLIdentity::FromPEMStrings(private_key, certificate);
}

#elif SSL_USE_NSS  // !SSL_USE_OPENSSL && !SSL_USE_SCHANNEL

SSLCertificate* SSLCertificate::FromPEMString(const std::string& pem_string) {
  return NSSCertificate::FromPEMString(pem_string);
}

SSLIdentity* SSLIdentity::Generate(const std::string& common_name) {
  return NSSIdentity::Generate(common_name);
}

SSLIdentity* SSLIdentity::GenerateForTest(const SSLIdentityParams& params) {
  return NSSIdentity::GenerateForTest(params);
}

SSLIdentity* SSLIdentity::FromPEMStrings(const std::string& private_key,
                                         const std::string& certificate) {
  return NSSIdentity::FromPEMStrings(private_key, certificate);
}

#else  // !SSL_USE_OPENSSL && !SSL_USE_SCHANNEL && !SSL_USE_NSS

#error "No SSL implementation"

#endif  // SSL_USE_SCHANNEL

}  // namespace rtc
