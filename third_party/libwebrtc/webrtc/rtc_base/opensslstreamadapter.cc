/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "rtc_base/opensslstreamadapter.h"

#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/tls1.h>
#include <openssl/x509v3.h>
#ifndef OPENSSL_IS_BORINGSSL
#include <openssl/dtls1.h>
#include <openssl/ssl.h>
#endif

#include <memory>
#include <vector>

#include "rtc_base/checks.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/openssl.h"
#include "rtc_base/openssladapter.h"
#include "rtc_base/openssldigest.h"
#include "rtc_base/opensslidentity.h"
#include "rtc_base/stream.h"
#include "rtc_base/stringutils.h"
#include "rtc_base/thread.h"
#include "rtc_base/timeutils.h"

namespace {
  bool g_use_time_callback_for_testing = false;
}

namespace rtc {

#if (OPENSSL_VERSION_NUMBER < 0x10001000L)
#error "webrtc requires at least OpenSSL version 1.0.1, to support DTLS-SRTP"
#endif

// SRTP cipher suite table. |internal_name| is used to construct a
// colon-separated profile strings which is needed by
// SSL_CTX_set_tlsext_use_srtp().
struct SrtpCipherMapEntry {
  const char* internal_name;
  const int id;
};

// This isn't elegant, but it's better than an external reference
static SrtpCipherMapEntry SrtpCipherMap[] = {
    {"SRTP_AES128_CM_SHA1_80", SRTP_AES128_CM_SHA1_80},
    {"SRTP_AES128_CM_SHA1_32", SRTP_AES128_CM_SHA1_32},
    {"SRTP_AEAD_AES_128_GCM", SRTP_AEAD_AES_128_GCM},
    {"SRTP_AEAD_AES_256_GCM", SRTP_AEAD_AES_256_GCM},
    {nullptr, 0}};

#ifdef OPENSSL_IS_BORINGSSL
// Not used in production code. Actual time should be relative to Jan 1, 1970.
static void TimeCallbackForTesting(const SSL* ssl, struct timeval* out_clock) {
  int64_t time = TimeNanos();
  out_clock->tv_sec = time / kNumNanosecsPerSec;
  out_clock->tv_usec = (time % kNumNanosecsPerSec) / kNumNanosecsPerMicrosec;
}
#else  // #ifdef OPENSSL_IS_BORINGSSL

// Cipher name table. Maps internal OpenSSL cipher ids to the RFC name.
struct SslCipherMapEntry {
  uint32_t openssl_id;
  const char* rfc_name;
};

#define DEFINE_CIPHER_ENTRY_SSL3(name)  {SSL3_CK_##name, "TLS_"#name}
#define DEFINE_CIPHER_ENTRY_TLS1(name)  {TLS1_CK_##name, "TLS_"#name}

// There currently is no method available to get a RFC-compliant name for a
// cipher suite from BoringSSL, so we need to define the mapping manually here.
// This should go away once BoringSSL supports "SSL_CIPHER_standard_name"
// (as available in OpenSSL if compiled with tracing enabled) or a similar
// method.
static const SslCipherMapEntry kSslCipherMap[] = {
    // TLS v1.0 ciphersuites from RFC2246.
    DEFINE_CIPHER_ENTRY_SSL3(RSA_RC4_128_SHA),
    {SSL3_CK_RSA_DES_192_CBC3_SHA, "TLS_RSA_WITH_3DES_EDE_CBC_SHA"},

    // AES ciphersuites from RFC3268.
    {TLS1_CK_RSA_WITH_AES_128_SHA, "TLS_RSA_WITH_AES_128_CBC_SHA"},
    {TLS1_CK_DHE_RSA_WITH_AES_128_SHA, "TLS_DHE_RSA_WITH_AES_128_CBC_SHA"},
    {TLS1_CK_RSA_WITH_AES_256_SHA, "TLS_RSA_WITH_AES_256_CBC_SHA"},
    {TLS1_CK_DHE_RSA_WITH_AES_256_SHA, "TLS_DHE_RSA_WITH_AES_256_CBC_SHA"},

    // ECC ciphersuites from RFC4492.
    DEFINE_CIPHER_ENTRY_TLS1(ECDHE_ECDSA_WITH_RC4_128_SHA),
    {TLS1_CK_ECDHE_ECDSA_WITH_DES_192_CBC3_SHA,
     "TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA"},
    DEFINE_CIPHER_ENTRY_TLS1(ECDHE_ECDSA_WITH_AES_128_CBC_SHA),
    DEFINE_CIPHER_ENTRY_TLS1(ECDHE_ECDSA_WITH_AES_256_CBC_SHA),

    DEFINE_CIPHER_ENTRY_TLS1(ECDHE_RSA_WITH_RC4_128_SHA),
    {TLS1_CK_ECDHE_RSA_WITH_DES_192_CBC3_SHA,
     "TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA"},
    DEFINE_CIPHER_ENTRY_TLS1(ECDHE_RSA_WITH_AES_128_CBC_SHA),
    DEFINE_CIPHER_ENTRY_TLS1(ECDHE_RSA_WITH_AES_256_CBC_SHA),

    // TLS v1.2 ciphersuites.
    {TLS1_CK_RSA_WITH_AES_128_SHA256, "TLS_RSA_WITH_AES_128_CBC_SHA256"},
    {TLS1_CK_RSA_WITH_AES_256_SHA256, "TLS_RSA_WITH_AES_256_CBC_SHA256"},
    {TLS1_CK_DHE_RSA_WITH_AES_128_SHA256,
     "TLS_DHE_RSA_WITH_AES_128_CBC_SHA256"},
    {TLS1_CK_DHE_RSA_WITH_AES_256_SHA256,
     "TLS_DHE_RSA_WITH_AES_256_CBC_SHA256"},

    // TLS v1.2 GCM ciphersuites from RFC5288.
    DEFINE_CIPHER_ENTRY_TLS1(RSA_WITH_AES_128_GCM_SHA256),
    DEFINE_CIPHER_ENTRY_TLS1(RSA_WITH_AES_256_GCM_SHA384),
    DEFINE_CIPHER_ENTRY_TLS1(DHE_RSA_WITH_AES_128_GCM_SHA256),
    DEFINE_CIPHER_ENTRY_TLS1(DHE_RSA_WITH_AES_256_GCM_SHA384),
    DEFINE_CIPHER_ENTRY_TLS1(DH_RSA_WITH_AES_128_GCM_SHA256),
    DEFINE_CIPHER_ENTRY_TLS1(DH_RSA_WITH_AES_256_GCM_SHA384),

    // ECDH HMAC based ciphersuites from RFC5289.
    {TLS1_CK_ECDHE_ECDSA_WITH_AES_128_SHA256,
     "TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256"},
    {TLS1_CK_ECDHE_ECDSA_WITH_AES_256_SHA384,
     "TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384"},
    {TLS1_CK_ECDHE_RSA_WITH_AES_128_SHA256,
     "TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256"},
    {TLS1_CK_ECDHE_RSA_WITH_AES_256_SHA384,
     "TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384"},

    // ECDH GCM based ciphersuites from RFC5289.
    DEFINE_CIPHER_ENTRY_TLS1(ECDHE_ECDSA_WITH_AES_128_GCM_SHA256),
    DEFINE_CIPHER_ENTRY_TLS1(ECDHE_ECDSA_WITH_AES_256_GCM_SHA384),
    DEFINE_CIPHER_ENTRY_TLS1(ECDHE_RSA_WITH_AES_128_GCM_SHA256),
    DEFINE_CIPHER_ENTRY_TLS1(ECDHE_RSA_WITH_AES_256_GCM_SHA384),

    {0, nullptr}};
#endif  // #ifndef OPENSSL_IS_BORINGSSL

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4309)
#pragma warning(disable : 4310)
#endif  // defined(_MSC_VER)

#if defined(_MSC_VER)
#pragma warning(pop)
#endif  // defined(_MSC_VER)

//////////////////////////////////////////////////////////////////////
// StreamBIO
//////////////////////////////////////////////////////////////////////

static int stream_write(BIO* h, const char* buf, int num);
static int stream_read(BIO* h, char* buf, int size);
static int stream_puts(BIO* h, const char* str);
static long stream_ctrl(BIO* h, int cmd, long arg1, void* arg2);
static int stream_new(BIO* h);
static int stream_free(BIO* data);

static const BIO_METHOD methods_stream = {
    BIO_TYPE_BIO, "stream",   stream_write, stream_read, stream_puts, 0,
    stream_ctrl,  stream_new, stream_free,  nullptr,
};

static BIO* BIO_new_stream(StreamInterface* stream) {
  // TODO(davidben): Remove the const_cast when BoringSSL is assumed.
  BIO* ret = BIO_new(const_cast<BIO_METHOD*>(&methods_stream));
  if (ret == nullptr)
    return nullptr;
  ret->ptr = stream;
  return ret;
}

// bio methods return 1 (or at least non-zero) on success and 0 on failure.

static int stream_new(BIO* b) {
  b->shutdown = 0;
  b->init = 1;
  b->num = 0;  // 1 means end-of-stream
  b->ptr = 0;
  return 1;
}

static int stream_free(BIO* b) {
  if (b == nullptr)
    return 0;
  return 1;
}

static int stream_read(BIO* b, char* out, int outl) {
  if (!out)
    return -1;
  StreamInterface* stream = static_cast<StreamInterface*>(b->ptr);
  BIO_clear_retry_flags(b);
  size_t read;
  int error;
  StreamResult result = stream->Read(out, outl, &read, &error);
  if (result == SR_SUCCESS) {
    return checked_cast<int>(read);
  } else if (result == SR_EOS) {
    b->num = 1;
  } else if (result == SR_BLOCK) {
    BIO_set_retry_read(b);
  }
  return -1;
}

static int stream_write(BIO* b, const char* in, int inl) {
  if (!in)
    return -1;
  StreamInterface* stream = static_cast<StreamInterface*>(b->ptr);
  BIO_clear_retry_flags(b);
  size_t written;
  int error;
  StreamResult result = stream->Write(in, inl, &written, &error);
  if (result == SR_SUCCESS) {
    return checked_cast<int>(written);
  } else if (result == SR_BLOCK) {
    BIO_set_retry_write(b);
  }
  return -1;
}

static int stream_puts(BIO* b, const char* str) {
  return stream_write(b, str, checked_cast<int>(strlen(str)));
}

static long stream_ctrl(BIO* b, int cmd, long num, void* ptr) {
  switch (cmd) {
    case BIO_CTRL_RESET:
      return 0;
    case BIO_CTRL_EOF:
      return b->num;
    case BIO_CTRL_WPENDING:
    case BIO_CTRL_PENDING:
      return 0;
    case BIO_CTRL_FLUSH:
      return 1;
    case BIO_CTRL_DGRAM_QUERY_MTU:
      // openssl defaults to mtu=256 unless we return something here.
      // The handshake doesn't actually need to send packets above 1k,
      // so this seems like a sensible value that should work in most cases.
      // Webrtc uses the same value for video packets.
      return 1200;
    default:
      return 0;
  }
}

/////////////////////////////////////////////////////////////////////////////
// OpenSSLStreamAdapter
/////////////////////////////////////////////////////////////////////////////

OpenSSLStreamAdapter::OpenSSLStreamAdapter(StreamInterface* stream)
    : SSLStreamAdapter(stream),
      state_(SSL_NONE),
      role_(SSL_CLIENT),
      ssl_read_needs_write_(false),
      ssl_write_needs_read_(false),
      ssl_(nullptr),
      ssl_ctx_(nullptr),
      ssl_mode_(SSL_MODE_TLS),
      ssl_max_version_(SSL_PROTOCOL_TLS_12) {}

OpenSSLStreamAdapter::~OpenSSLStreamAdapter() {
  Cleanup(0);
}

void OpenSSLStreamAdapter::SetIdentity(SSLIdentity* identity) {
  RTC_DCHECK(!identity_);
  identity_.reset(static_cast<OpenSSLIdentity*>(identity));
}

void OpenSSLStreamAdapter::SetServerRole(SSLRole role) {
  role_ = role;
}

std::unique_ptr<SSLCertificate> OpenSSLStreamAdapter::GetPeerCertificate()
    const {
  return peer_certificate_ ? std::unique_ptr<SSLCertificate>(
                                 peer_certificate_->GetReference())
                           : nullptr;
}

bool OpenSSLStreamAdapter::SetPeerCertificateDigest(
    const std::string& digest_alg,
    const unsigned char* digest_val,
    size_t digest_len,
    SSLPeerCertificateDigestError* error) {
  RTC_DCHECK(!peer_certificate_verified_);
  RTC_DCHECK(!has_peer_certificate_digest());
  size_t expected_len;
  if (error) {
    *error = SSLPeerCertificateDigestError::NONE;
  }

  if (!OpenSSLDigest::GetDigestSize(digest_alg, &expected_len)) {
    RTC_LOG(LS_WARNING) << "Unknown digest algorithm: " << digest_alg;
    if (error) {
      *error = SSLPeerCertificateDigestError::UNKNOWN_ALGORITHM;
    }
    return false;
  }
  if (expected_len != digest_len) {
    if (error) {
      *error = SSLPeerCertificateDigestError::INVALID_LENGTH;
    }
    return false;
  }

  peer_certificate_digest_value_.SetData(digest_val, digest_len);
  peer_certificate_digest_algorithm_ = digest_alg;

  if (!peer_certificate_) {
    // Normal case, where the digest is set before we obtain the certificate
    // from the handshake.
    return true;
  }

  if (!VerifyPeerCertificate()) {
    Error("SetPeerCertificateDigest", -1, SSL_AD_BAD_CERTIFICATE, false);
    if (error) {
      *error = SSLPeerCertificateDigestError::VERIFICATION_FAILED;
    }
    return false;
  }

  if (state_ == SSL_CONNECTED) {
    // Post the event asynchronously to unwind the stack. The caller
    // of ContinueSSL may be the same object listening for these
    // events and may not be prepared for reentrancy.
    PostEvent(SE_OPEN | SE_READ | SE_WRITE, 0);
  }

  return true;
}

std::string OpenSSLStreamAdapter::SslCipherSuiteToName(int cipher_suite) {
#ifdef OPENSSL_IS_BORINGSSL
  const SSL_CIPHER* ssl_cipher = SSL_get_cipher_by_value(cipher_suite);
  if (!ssl_cipher) {
    return std::string();
  }
  return SSL_CIPHER_standard_name(ssl_cipher);
#else
  for (const SslCipherMapEntry* entry = kSslCipherMap; entry->rfc_name;
       ++entry) {
    if (cipher_suite == static_cast<int>(entry->openssl_id)) {
      return entry->rfc_name;
    }
  }
  return std::string();
#endif
}

bool OpenSSLStreamAdapter::GetSslCipherSuite(int* cipher_suite) {
  if (state_ != SSL_CONNECTED)
    return false;

  const SSL_CIPHER* current_cipher = SSL_get_current_cipher(ssl_);
  if (current_cipher == nullptr) {
    return false;
  }

  *cipher_suite = static_cast<uint16_t>(SSL_CIPHER_get_id(current_cipher));
  return true;
}

int OpenSSLStreamAdapter::GetSslVersion() const {
  if (state_ != SSL_CONNECTED)
    return -1;

  int ssl_version = SSL_version(ssl_);
  if (ssl_mode_ == SSL_MODE_DTLS) {
    if (ssl_version == DTLS1_VERSION)
      return SSL_PROTOCOL_DTLS_10;
    else if (ssl_version == DTLS1_2_VERSION)
      return SSL_PROTOCOL_DTLS_12;
  } else {
    if (ssl_version == TLS1_VERSION)
      return SSL_PROTOCOL_TLS_10;
    else if (ssl_version == TLS1_1_VERSION)
      return SSL_PROTOCOL_TLS_11;
    else if (ssl_version == TLS1_2_VERSION)
      return SSL_PROTOCOL_TLS_12;
  }

  return -1;
}

// Key Extractor interface
bool OpenSSLStreamAdapter::ExportKeyingMaterial(const std::string& label,
                                                const uint8_t* context,
                                                size_t context_len,
                                                bool use_context,
                                                uint8_t* result,
                                                size_t result_len) {
  int i;

  i = SSL_export_keying_material(ssl_, result, result_len, label.c_str(),
                                 label.length(), const_cast<uint8_t*>(context),
                                 context_len, use_context);

  if (i != 1)
    return false;

  return true;
}

bool OpenSSLStreamAdapter::SetDtlsSrtpCryptoSuites(
    const std::vector<int>& ciphers) {
  std::string internal_ciphers;

  if (state_ != SSL_NONE)
    return false;

  for (std::vector<int>::const_iterator cipher = ciphers.begin();
       cipher != ciphers.end(); ++cipher) {
    bool found = false;
    for (SrtpCipherMapEntry* entry = SrtpCipherMap; entry->internal_name;
         ++entry) {
      if (*cipher == entry->id) {
        found = true;
        if (!internal_ciphers.empty())
          internal_ciphers += ":";
        internal_ciphers += entry->internal_name;
        break;
      }
    }

    if (!found) {
      RTC_LOG(LS_ERROR) << "Could not find cipher: " << *cipher;
      return false;
    }
  }

  if (internal_ciphers.empty())
    return false;

  srtp_ciphers_ = internal_ciphers;
  return true;
}

bool OpenSSLStreamAdapter::GetDtlsSrtpCryptoSuite(int* crypto_suite) {
  RTC_DCHECK(state_ == SSL_CONNECTED);
  if (state_ != SSL_CONNECTED)
    return false;

  const SRTP_PROTECTION_PROFILE *srtp_profile =
      SSL_get_selected_srtp_profile(ssl_);

  if (!srtp_profile)
    return false;

  *crypto_suite = srtp_profile->id;
  RTC_DCHECK(!SrtpCryptoSuiteToName(*crypto_suite).empty());
  return true;
}

bool OpenSSLStreamAdapter::IsTlsConnected() {
  return state_ == SSL_CONNECTED;
}

int OpenSSLStreamAdapter::StartSSL() {
  if (state_ != SSL_NONE) {
    // Don't allow StartSSL to be called twice.
    return -1;
  }

  if (StreamAdapterInterface::GetState() != SS_OPEN) {
    state_ = SSL_WAIT;
    return 0;
  }

  state_ = SSL_CONNECTING;
  if (int err = BeginSSL()) {
    Error("BeginSSL", err, 0, false);
    return err;
  }

  return 0;
}

void OpenSSLStreamAdapter::SetMode(SSLMode mode) {
  RTC_DCHECK(state_ == SSL_NONE);
  ssl_mode_ = mode;
}

void OpenSSLStreamAdapter::SetMaxProtocolVersion(SSLProtocolVersion version) {
  RTC_DCHECK(ssl_ctx_ == nullptr);
  ssl_max_version_ = version;
}

void OpenSSLStreamAdapter::SetInitialRetransmissionTimeout(
    int timeout_ms) {
  RTC_DCHECK(ssl_ctx_ == nullptr);
  dtls_handshake_timeout_ms_ = timeout_ms;
}

//
// StreamInterface Implementation
//

StreamResult OpenSSLStreamAdapter::Write(const void* data, size_t data_len,
                                         size_t* written, int* error) {
  RTC_LOG(LS_VERBOSE) << "OpenSSLStreamAdapter::Write(" << data_len << ")";

  switch (state_) {
  case SSL_NONE:
    // pass-through in clear text
    return StreamAdapterInterface::Write(data, data_len, written, error);

  case SSL_WAIT:
  case SSL_CONNECTING:
    return SR_BLOCK;

  case SSL_CONNECTED:
    if (waiting_to_verify_peer_certificate()) {
      return SR_BLOCK;
    }
    break;

  case SSL_ERROR:
  case SSL_CLOSED:
  default:
    if (error)
      *error = ssl_error_code_;
    return SR_ERROR;
  }

  // OpenSSL will return an error if we try to write zero bytes
  if (data_len == 0) {
    if (written)
      *written = 0;
    return SR_SUCCESS;
  }

  ssl_write_needs_read_ = false;

  int code = SSL_write(ssl_, data, checked_cast<int>(data_len));
  int ssl_error = SSL_get_error(ssl_, code);
  switch (ssl_error) {
  case SSL_ERROR_NONE:
    RTC_LOG(LS_VERBOSE) << " -- success";
    RTC_DCHECK_GT(code, 0);
    RTC_DCHECK_LE(code, data_len);
    if (written)
      *written = code;
    return SR_SUCCESS;
  case SSL_ERROR_WANT_READ:
    RTC_LOG(LS_VERBOSE) << " -- error want read";
    ssl_write_needs_read_ = true;
    return SR_BLOCK;
  case SSL_ERROR_WANT_WRITE:
    RTC_LOG(LS_VERBOSE) << " -- error want write";
    return SR_BLOCK;

  case SSL_ERROR_ZERO_RETURN:
  default:
    Error("SSL_write", (ssl_error ? ssl_error : -1), 0, false);
    if (error)
      *error = ssl_error_code_;
    return SR_ERROR;
  }
  // not reached
}

StreamResult OpenSSLStreamAdapter::Read(void* data, size_t data_len,
                                        size_t* read, int* error) {
  RTC_LOG(LS_VERBOSE) << "OpenSSLStreamAdapter::Read(" << data_len << ")";
  switch (state_) {
    case SSL_NONE:
      // pass-through in clear text
      return StreamAdapterInterface::Read(data, data_len, read, error);

    case SSL_WAIT:
    case SSL_CONNECTING:
      return SR_BLOCK;

    case SSL_CONNECTED:
      if (waiting_to_verify_peer_certificate()) {
        return SR_BLOCK;
      }
      break;

    case SSL_CLOSED:
      return SR_EOS;

    case SSL_ERROR:
    default:
      if (error)
        *error = ssl_error_code_;
      return SR_ERROR;
  }

  // Don't trust OpenSSL with zero byte reads
  if (data_len == 0) {
    if (read)
      *read = 0;
    return SR_SUCCESS;
  }

  ssl_read_needs_write_ = false;

  int code = SSL_read(ssl_, data, checked_cast<int>(data_len));
  int ssl_error = SSL_get_error(ssl_, code);
  switch (ssl_error) {
    case SSL_ERROR_NONE:
      RTC_LOG(LS_VERBOSE) << " -- success";
      RTC_DCHECK_GT(code, 0);
      RTC_DCHECK_LE(code, data_len);
      if (read)
        *read = code;

      if (ssl_mode_ == SSL_MODE_DTLS) {
        // Enforce atomic reads -- this is a short read
        unsigned int pending = SSL_pending(ssl_);

        if (pending) {
          RTC_LOG(LS_INFO) << " -- short DTLS read. flushing";
          FlushInput(pending);
          if (error)
            *error = SSE_MSG_TRUNC;
          return SR_ERROR;
        }
      }
      return SR_SUCCESS;
    case SSL_ERROR_WANT_READ:
      RTC_LOG(LS_VERBOSE) << " -- error want read";
      return SR_BLOCK;
    case SSL_ERROR_WANT_WRITE:
      RTC_LOG(LS_VERBOSE) << " -- error want write";
      ssl_read_needs_write_ = true;
      return SR_BLOCK;
    case SSL_ERROR_ZERO_RETURN:
      RTC_LOG(LS_VERBOSE) << " -- remote side closed";
      Close();
      return SR_EOS;
      break;
    default:
      RTC_LOG(LS_VERBOSE) << " -- error " << code;
      Error("SSL_read", (ssl_error ? ssl_error : -1), 0, false);
      if (error)
        *error = ssl_error_code_;
      return SR_ERROR;
  }
  // not reached
}

void OpenSSLStreamAdapter::FlushInput(unsigned int left) {
  unsigned char buf[2048];

  while (left) {
    // This should always succeed
    int toread = (sizeof(buf) < left) ? sizeof(buf) : left;
    int code = SSL_read(ssl_, buf, toread);

    int ssl_error = SSL_get_error(ssl_, code);
    RTC_DCHECK(ssl_error == SSL_ERROR_NONE);

    if (ssl_error != SSL_ERROR_NONE) {
      RTC_LOG(LS_VERBOSE) << " -- error " << code;
      Error("SSL_read", (ssl_error ? ssl_error : -1), 0, false);
      return;
    }

    RTC_LOG(LS_VERBOSE) << " -- flushed " << code << " bytes";
    left -= code;
  }
}

void OpenSSLStreamAdapter::Close() {
  Cleanup(0);
  RTC_DCHECK(state_ == SSL_CLOSED || state_ == SSL_ERROR);
  // When we're closed at SSL layer, also close the stream level which
  // performs necessary clean up. Otherwise, a new incoming packet after
  // this could overflow the stream buffer.
  StreamAdapterInterface::Close();
}

StreamState OpenSSLStreamAdapter::GetState() const {
  switch (state_) {
    case SSL_WAIT:
    case SSL_CONNECTING:
      return SS_OPENING;
    case SSL_CONNECTED:
      if (waiting_to_verify_peer_certificate()) {
        return SS_OPENING;
      }
      return SS_OPEN;
    default:
      return SS_CLOSED;
  };
  // not reached
}

void OpenSSLStreamAdapter::OnEvent(StreamInterface* stream, int events,
                                   int err) {
  int events_to_signal = 0;
  int signal_error = 0;
  RTC_DCHECK(stream == this->stream());
  if ((events & SE_OPEN)) {
    RTC_LOG(LS_VERBOSE) << "OpenSSLStreamAdapter::OnEvent SE_OPEN";
    if (state_ != SSL_WAIT) {
      RTC_DCHECK(state_ == SSL_NONE);
      events_to_signal |= SE_OPEN;
    } else {
      state_ = SSL_CONNECTING;
      if (int err = BeginSSL()) {
        Error("BeginSSL", err, 0, true);
        return;
      }
    }
  }
  if ((events & (SE_READ|SE_WRITE))) {
    RTC_LOG(LS_VERBOSE) << "OpenSSLStreamAdapter::OnEvent"
                        << ((events & SE_READ) ? " SE_READ" : "")
                        << ((events & SE_WRITE) ? " SE_WRITE" : "");
    if (state_ == SSL_NONE) {
      events_to_signal |= events & (SE_READ|SE_WRITE);
    } else if (state_ == SSL_CONNECTING) {
      if (int err = ContinueSSL()) {
        Error("ContinueSSL", err, 0, true);
        return;
      }
    } else if (state_ == SSL_CONNECTED) {
      if (((events & SE_READ) && ssl_write_needs_read_) ||
          (events & SE_WRITE)) {
        RTC_LOG(LS_VERBOSE) << " -- onStreamWriteable";
        events_to_signal |= SE_WRITE;
      }
      if (((events & SE_WRITE) && ssl_read_needs_write_) ||
          (events & SE_READ)) {
        RTC_LOG(LS_VERBOSE) << " -- onStreamReadable";
        events_to_signal |= SE_READ;
      }
    }
  }
  if ((events & SE_CLOSE)) {
    RTC_LOG(LS_VERBOSE) << "OpenSSLStreamAdapter::OnEvent(SE_CLOSE, " << err
                        << ")";
    Cleanup(0);
    events_to_signal |= SE_CLOSE;
    // SE_CLOSE is the only event that uses the final parameter to OnEvent().
    RTC_DCHECK(signal_error == 0);
    signal_error = err;
  }
  if (events_to_signal)
    StreamAdapterInterface::OnEvent(stream, events_to_signal, signal_error);
}

int OpenSSLStreamAdapter::BeginSSL() {
  RTC_DCHECK(state_ == SSL_CONNECTING);
  // The underlying stream has opened.
  RTC_LOG(LS_INFO) << "BeginSSL with peer.";

  BIO* bio = nullptr;

  // First set up the context.
  RTC_DCHECK(ssl_ctx_ == nullptr);
  ssl_ctx_ = SetupSSLContext();
  if (!ssl_ctx_)
    return -1;

  bio = BIO_new_stream(static_cast<StreamInterface*>(stream()));
  if (!bio)
    return -1;

  ssl_ = SSL_new(ssl_ctx_);
  if (!ssl_) {
    BIO_free(bio);
    return -1;
  }

  SSL_set_app_data(ssl_, this);

  SSL_set_bio(ssl_, bio, bio);  // the SSL object owns the bio now.
  if (ssl_mode_ == SSL_MODE_DTLS) {
#ifdef OPENSSL_IS_BORINGSSL
    DTLSv1_set_initial_timeout_duration(ssl_, dtls_handshake_timeout_ms_);
#else
    // Enable read-ahead for DTLS so whole packets are read from internal BIO
    // before parsing. This is done internally by BoringSSL for DTLS.
    SSL_set_read_ahead(ssl_, 1);
#endif
  }

  SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE |
               SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

#if !defined(OPENSSL_IS_BORINGSSL)
  // Specify an ECDH group for ECDHE ciphers, otherwise OpenSSL cannot
  // negotiate them when acting as the server. Use NIST's P-256 which is
  // commonly supported. BoringSSL doesn't need explicit configuration and has
  // a reasonable default set.
  EC_KEY* ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  if (ecdh == nullptr)
    return -1;
  SSL_set_options(ssl_, SSL_OP_SINGLE_ECDH_USE);
  SSL_set_tmp_ecdh(ssl_, ecdh);
  EC_KEY_free(ecdh);
#endif

  // Do the connect
  return ContinueSSL();
}

int OpenSSLStreamAdapter::ContinueSSL() {
  RTC_LOG(LS_VERBOSE) << "ContinueSSL";
  RTC_DCHECK(state_ == SSL_CONNECTING);

  // Clear the DTLS timer
  Thread::Current()->Clear(this, MSG_TIMEOUT);

  int code = (role_ == SSL_CLIENT) ? SSL_connect(ssl_) : SSL_accept(ssl_);
  int ssl_error;
  switch (ssl_error = SSL_get_error(ssl_, code)) {
    case SSL_ERROR_NONE:
      RTC_LOG(LS_VERBOSE) << " -- success";
      // By this point, OpenSSL should have given us a certificate, or errored
      // out if one was missing.
      RTC_DCHECK(peer_certificate_ || !client_auth_enabled());

      state_ = SSL_CONNECTED;
      if (!waiting_to_verify_peer_certificate()) {
        // We have everything we need to start the connection, so signal
        // SE_OPEN. If we need a client certificate fingerprint and don't have
        // it yet, we'll instead signal SE_OPEN in SetPeerCertificateDigest.
        //
        // TODO(deadbeef): Post this event asynchronously to unwind the stack.
        // The caller of ContinueSSL may be the same object listening for these
        // events and may not be prepared for reentrancy.
        // PostEvent(SE_OPEN | SE_READ | SE_WRITE, 0);
        StreamAdapterInterface::OnEvent(stream(), SE_OPEN | SE_READ | SE_WRITE,
                                        0);
      }
      break;

    case SSL_ERROR_WANT_READ: {
      RTC_LOG(LS_VERBOSE) << " -- error want read";
      struct timeval timeout;
      if (DTLSv1_get_timeout(ssl_, &timeout)) {
        int delay = timeout.tv_sec * 1000 + timeout.tv_usec / 1000;

        Thread::Current()->PostDelayed(RTC_FROM_HERE, delay, this, MSG_TIMEOUT,
                                       0);
        }
      }
      break;

    case SSL_ERROR_WANT_WRITE:
      RTC_LOG(LS_VERBOSE) << " -- error want write";
      break;

    case SSL_ERROR_ZERO_RETURN:
    default:
      RTC_LOG(LS_VERBOSE) << " -- error " << code;
      SSLHandshakeError ssl_handshake_err = SSLHandshakeError::UNKNOWN;
      int err_code = ERR_peek_last_error();
      if (err_code != 0 && ERR_GET_REASON(err_code) == SSL_R_NO_SHARED_CIPHER) {
        ssl_handshake_err = SSLHandshakeError::INCOMPATIBLE_CIPHERSUITE;
      }
      SignalSSLHandshakeError(ssl_handshake_err);
      return (ssl_error != 0) ? ssl_error : -1;
  }

  return 0;
}

void OpenSSLStreamAdapter::Error(const char* context,
                                 int err,
                                 uint8_t alert,
                                 bool signal) {
  RTC_LOG(LS_WARNING) << "OpenSSLStreamAdapter::Error(" << context << ", "
                      << err << ", " << static_cast<int>(alert) << ")";
  state_ = SSL_ERROR;
  ssl_error_code_ = err;
  Cleanup(alert);
  if (signal)
    StreamAdapterInterface::OnEvent(stream(), SE_CLOSE, err);
}

void OpenSSLStreamAdapter::Cleanup(uint8_t alert) {
  RTC_LOG(LS_INFO) << "Cleanup";

  if (state_ != SSL_ERROR) {
    state_ = SSL_CLOSED;
    ssl_error_code_ = 0;
  }

  if (ssl_) {
    int ret;
// SSL_send_fatal_alert is only available in BoringSSL.
#ifdef OPENSSL_IS_BORINGSSL
    if (alert) {
      ret = SSL_send_fatal_alert(ssl_, alert);
      if (ret < 0) {
        RTC_LOG(LS_WARNING) << "SSL_send_fatal_alert failed, error = "
                            << SSL_get_error(ssl_, ret);
      }
    } else {
#endif
      ret = SSL_shutdown(ssl_);
      if (ret < 0) {
        RTC_LOG(LS_WARNING)
            << "SSL_shutdown failed, error = " << SSL_get_error(ssl_, ret);
      }
#ifdef OPENSSL_IS_BORINGSSL
    }
#endif
    SSL_free(ssl_);
    ssl_ = nullptr;
  }
  if (ssl_ctx_) {
    SSL_CTX_free(ssl_ctx_);
    ssl_ctx_ = nullptr;
  }
  identity_.reset();
  peer_certificate_.reset();

  // Clear the DTLS timer
  Thread::Current()->Clear(this, MSG_TIMEOUT);
}


void OpenSSLStreamAdapter::OnMessage(Message* msg) {
  // Process our own messages and then pass others to the superclass
  if (MSG_TIMEOUT == msg->message_id) {
    RTC_LOG(LS_INFO) << "DTLS timeout expired";
    DTLSv1_handle_timeout(ssl_);
    ContinueSSL();
  } else {
    StreamInterface::OnMessage(msg);
  }
}

SSL_CTX* OpenSSLStreamAdapter::SetupSSLContext() {
  SSL_CTX* ctx = nullptr;

#ifdef OPENSSL_IS_BORINGSSL
    ctx = SSL_CTX_new(ssl_mode_ == SSL_MODE_DTLS ?
        DTLS_method() : TLS_method());
    // Version limiting for BoringSSL will be done below.
#else
  const SSL_METHOD* method;
  switch (ssl_max_version_) {
    case SSL_PROTOCOL_TLS_10:
    case SSL_PROTOCOL_TLS_11:
      // OpenSSL doesn't support setting min/max versions, so we always use
      // (D)TLS 1.0 if a max. version below the max. available is requested.
      if (ssl_mode_ == SSL_MODE_DTLS) {
        if (role_ == SSL_CLIENT) {
          method = DTLSv1_client_method();
        } else {
          method = DTLSv1_server_method();
        }
      } else {
        if (role_ == SSL_CLIENT) {
          method = TLSv1_client_method();
        } else {
          method = TLSv1_server_method();
        }
      }
      break;
    case SSL_PROTOCOL_TLS_12:
    default:
      if (ssl_mode_ == SSL_MODE_DTLS) {
#if (OPENSSL_VERSION_NUMBER >= 0x10002000L)
        // DTLS 1.2 only available starting from OpenSSL 1.0.2
        if (role_ == SSL_CLIENT) {
          method = DTLS_client_method();
        } else {
          method = DTLS_server_method();
        }
#else
        if (role_ == SSL_CLIENT) {
          method = DTLSv1_client_method();
        } else {
          method = DTLSv1_server_method();
        }
#endif
      } else {
#if (OPENSSL_VERSION_NUMBER >= 0x10100000L)
        // New API only available starting from OpenSSL 1.1.0
        if (role_ == SSL_CLIENT) {
          method = TLS_client_method();
        } else {
          method = TLS_server_method();
        }
#else
        if (role_ == SSL_CLIENT) {
          method = SSLv23_client_method();
        } else {
          method = SSLv23_server_method();
        }
#endif
      }
      break;
  }
  ctx = SSL_CTX_new(method);
#endif  // OPENSSL_IS_BORINGSSL

  if (ctx == nullptr)
    return nullptr;

#ifdef OPENSSL_IS_BORINGSSL
  SSL_CTX_set_min_proto_version(ctx, ssl_mode_ == SSL_MODE_DTLS ?
      DTLS1_VERSION : TLS1_VERSION);
  switch (ssl_max_version_) {
    case SSL_PROTOCOL_TLS_10:
      SSL_CTX_set_max_proto_version(ctx, ssl_mode_ == SSL_MODE_DTLS ?
          DTLS1_VERSION : TLS1_VERSION);
      break;
    case SSL_PROTOCOL_TLS_11:
      SSL_CTX_set_max_proto_version(ctx, ssl_mode_ == SSL_MODE_DTLS ?
          DTLS1_VERSION : TLS1_1_VERSION);
      break;
    case SSL_PROTOCOL_TLS_12:
    default:
      SSL_CTX_set_max_proto_version(ctx, ssl_mode_ == SSL_MODE_DTLS ?
          DTLS1_2_VERSION : TLS1_2_VERSION);
      break;
  }
  if (g_use_time_callback_for_testing) {
    SSL_CTX_set_current_time_cb(ctx, &TimeCallbackForTesting);
  }
#endif

  if (identity_ && !identity_->ConfigureIdentity(ctx)) {
    SSL_CTX_free(ctx);
    return nullptr;
  }

#if !defined(NDEBUG)
  SSL_CTX_set_info_callback(ctx, OpenSSLAdapter::SSLInfoCallback);
#endif

  int mode = SSL_VERIFY_PEER;
  if (client_auth_enabled()) {
    // Require a certificate from the client.
    // Note: Normally this is always true in production, but it may be disabled
    // for testing purposes (e.g. SSLAdapter unit tests).
    mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
  }

  // Configure a custom certificate verification callback to check the peer
  // certificate digest. Note the second argument to SSL_CTX_set_verify is to
  // override individual errors in the default verification logic, which is not
  // what we want here.
  SSL_CTX_set_verify(ctx, mode, nullptr);
  SSL_CTX_set_cert_verify_callback(ctx, SSLVerifyCallback, nullptr);

  // Select list of available ciphers. Note that !SHA256 and !SHA384 only
  // remove HMAC-SHA256 and HMAC-SHA384 cipher suites, not GCM cipher suites
  // with SHA256 or SHA384 as the handshake hash.
  // This matches the list of SSLClientSocketOpenSSL in Chromium.
  SSL_CTX_set_cipher_list(
      ctx, "DEFAULT:!NULL:!aNULL:!SHA256:!SHA384:!aECDH:!AESGCM+AES256:!aPSK");

  if (!srtp_ciphers_.empty()) {
    if (SSL_CTX_set_tlsext_use_srtp(ctx, srtp_ciphers_.c_str())) {
      SSL_CTX_free(ctx);
      return nullptr;
    }
  }

  return ctx;
}

bool OpenSSLStreamAdapter::VerifyPeerCertificate() {
  if (!has_peer_certificate_digest() || !peer_certificate_) {
    RTC_LOG(LS_WARNING) << "Missing digest or peer certificate.";
    return false;
  }

  unsigned char digest[EVP_MAX_MD_SIZE];
  size_t digest_length;
  if (!OpenSSLCertificate::ComputeDigest(
          peer_certificate_->x509(), peer_certificate_digest_algorithm_, digest,
          sizeof(digest), &digest_length)) {
    RTC_LOG(LS_WARNING) << "Failed to compute peer cert digest.";
    return false;
  }

  Buffer computed_digest(digest, digest_length);
  if (computed_digest != peer_certificate_digest_value_) {
    RTC_LOG(LS_WARNING)
        << "Rejected peer certificate due to mismatched digest.";
    return false;
  }
  // Ignore any verification error if the digest matches, since there is no
  // value in checking the validity of a self-signed cert issued by untrusted
  // sources.
  RTC_LOG(LS_INFO) << "Accepted peer certificate.";
  peer_certificate_verified_ = true;
  return true;
}

std::unique_ptr<SSLCertChain> OpenSSLStreamAdapter::GetPeerSSLCertChain()
    const {
  return std::unique_ptr<SSLCertChain>(peer_cert_chain_->Copy());
}

int OpenSSLStreamAdapter::SSLVerifyCallback(X509_STORE_CTX* store, void* arg) {
  // Get our SSL structure and OpenSSLStreamAdapter from the store.
  SSL* ssl = reinterpret_cast<SSL*>(
      X509_STORE_CTX_get_ex_data(store, SSL_get_ex_data_X509_STORE_CTX_idx()));
  OpenSSLStreamAdapter* stream =
      reinterpret_cast<OpenSSLStreamAdapter*>(SSL_get_app_data(ssl));

#if defined(OPENSSL_IS_BORINGSSL)
  STACK_OF(X509)* chain = SSL_get_peer_full_cert_chain(ssl);
  // Creates certificate.
  stream->peer_certificate_.reset(
      new OpenSSLCertificate(sk_X509_value(chain, 0)));
  // Creates certificate chain.
  std::vector<std::unique_ptr<SSLCertificate>> cert_chain;
  for (X509* cert : chain) {
    cert_chain.emplace_back(new OpenSSLCertificate(cert));
  }
  stream->peer_cert_chain_.reset(new SSLCertChain(std::move(cert_chain)));
#else
  // Record the peer's certificate.
  X509* cert = SSL_get_peer_certificate(ssl);
  stream->peer_certificate_.reset(new OpenSSLCertificate(cert));
  X509_free(cert);
#endif

  // If the peer certificate digest isn't known yet, we'll wait to verify
  // until it's known, and for now just return a success status.
  if (stream->peer_certificate_digest_algorithm_.empty()) {
    RTC_LOG(LS_INFO) << "Waiting to verify certificate until digest is known.";
    return 1;
  }

  if (!stream->VerifyPeerCertificate()) {
    X509_STORE_CTX_set_error(store, X509_V_ERR_CERT_REJECTED);
    return 0;
  }

  return 1;
}

bool OpenSSLStreamAdapter::IsBoringSsl() {
#ifdef OPENSSL_IS_BORINGSSL
  return true;
#else
  return false;
#endif
}

#define CDEF(X) \
  { static_cast<uint16_t>(TLS1_CK_##X & 0xffff), "TLS_" #X }

struct cipher_list {
  uint16_t cipher;
  const char* cipher_str;
};

// TODO(torbjorng): Perhaps add more cipher suites to these lists.
static const cipher_list OK_RSA_ciphers[] = {
  CDEF(ECDHE_RSA_WITH_AES_128_CBC_SHA),
  CDEF(ECDHE_RSA_WITH_AES_256_CBC_SHA),
  CDEF(ECDHE_RSA_WITH_AES_128_GCM_SHA256),
#ifdef TLS1_CK_ECDHE_RSA_WITH_AES_256_GCM_SHA256
  CDEF(ECDHE_RSA_WITH_AES_256_GCM_SHA256),
#endif
#ifdef TLS1_CK_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256
  CDEF(ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256),
#endif
};

static const cipher_list OK_ECDSA_ciphers[] = {
  CDEF(ECDHE_ECDSA_WITH_AES_128_CBC_SHA),
  CDEF(ECDHE_ECDSA_WITH_AES_256_CBC_SHA),
  CDEF(ECDHE_ECDSA_WITH_AES_128_GCM_SHA256),
#ifdef TLS1_CK_ECDHE_ECDSA_WITH_AES_256_GCM_SHA256
  CDEF(ECDHE_ECDSA_WITH_AES_256_GCM_SHA256),
#endif
#ifdef TLS1_CK_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256
  CDEF(ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256),
#endif
};
#undef CDEF

bool OpenSSLStreamAdapter::IsAcceptableCipher(int cipher, KeyType key_type) {
  if (key_type == KT_RSA) {
    for (const cipher_list& c : OK_RSA_ciphers) {
      if (cipher == c.cipher)
        return true;
    }
  }

  if (key_type == KT_ECDSA) {
    for (const cipher_list& c : OK_ECDSA_ciphers) {
      if (cipher == c.cipher)
        return true;
    }
  }

  return false;
}

bool OpenSSLStreamAdapter::IsAcceptableCipher(const std::string& cipher,
                                              KeyType key_type) {
  if (key_type == KT_RSA) {
    for (const cipher_list& c : OK_RSA_ciphers) {
      if (cipher == c.cipher_str)
        return true;
    }
  }

  if (key_type == KT_ECDSA) {
    for (const cipher_list& c : OK_ECDSA_ciphers) {
      if (cipher == c.cipher_str)
        return true;
    }
  }

  return false;
}

void OpenSSLStreamAdapter::enable_time_callback_for_testing() {
  g_use_time_callback_for_testing = true;
}

}  // namespace rtc
