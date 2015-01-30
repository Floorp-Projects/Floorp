/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG_H

#if HAVE_OPENSSL_SSL_H

#include "webrtc/base/opensslstreamadapter.h"

#include <openssl/bio.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#include <openssl/x509v3.h>

#include <vector>

#include "webrtc/base/common.h"
#include "webrtc/base/logging.h"
#include "webrtc/base/safe_conversions.h"
#include "webrtc/base/stream.h"
#include "webrtc/base/openssl.h"
#include "webrtc/base/openssladapter.h"
#include "webrtc/base/openssldigest.h"
#include "webrtc/base/opensslidentity.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/thread.h"

namespace rtc {

#if (OPENSSL_VERSION_NUMBER >= 0x10001000L)
#define HAVE_DTLS_SRTP
#endif

#ifdef HAVE_DTLS_SRTP
// SRTP cipher suite table
struct SrtpCipherMapEntry {
  const char* external_name;
  const char* internal_name;
};

// This isn't elegant, but it's better than an external reference
static SrtpCipherMapEntry SrtpCipherMap[] = {
  {"AES_CM_128_HMAC_SHA1_80", "SRTP_AES128_CM_SHA1_80"},
  {"AES_CM_128_HMAC_SHA1_32", "SRTP_AES128_CM_SHA1_32"},
  {NULL, NULL}
};
#endif

//////////////////////////////////////////////////////////////////////
// StreamBIO
//////////////////////////////////////////////////////////////////////

static int stream_write(BIO* h, const char* buf, int num);
static int stream_read(BIO* h, char* buf, int size);
static int stream_puts(BIO* h, const char* str);
static long stream_ctrl(BIO* h, int cmd, long arg1, void* arg2);
static int stream_new(BIO* h);
static int stream_free(BIO* data);

static BIO_METHOD methods_stream = {
  BIO_TYPE_BIO,
  "stream",
  stream_write,
  stream_read,
  stream_puts,
  0,
  stream_ctrl,
  stream_new,
  stream_free,
  NULL,
};

static BIO_METHOD* BIO_s_stream() { return(&methods_stream); }

static BIO* BIO_new_stream(StreamInterface* stream) {
  BIO* ret = BIO_new(BIO_s_stream());
  if (ret == NULL)
    return NULL;
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
  if (b == NULL)
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
  RTC_UNUSED(num);
  RTC_UNUSED(ptr);

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
      ssl_read_needs_write_(false), ssl_write_needs_read_(false),
      ssl_(NULL), ssl_ctx_(NULL),
      custom_verification_succeeded_(false),
      ssl_mode_(SSL_MODE_TLS) {
}

OpenSSLStreamAdapter::~OpenSSLStreamAdapter() {
  Cleanup();
}

void OpenSSLStreamAdapter::SetIdentity(SSLIdentity* identity) {
  ASSERT(!identity_);
  identity_.reset(static_cast<OpenSSLIdentity*>(identity));
}

void OpenSSLStreamAdapter::SetServerRole(SSLRole role) {
  role_ = role;
}

bool OpenSSLStreamAdapter::GetPeerCertificate(SSLCertificate** cert) const {
  if (!peer_certificate_)
    return false;

  *cert = peer_certificate_->GetReference();
  return true;
}

bool OpenSSLStreamAdapter::SetPeerCertificateDigest(const std::string
                                                    &digest_alg,
                                                    const unsigned char*
                                                    digest_val,
                                                    size_t digest_len) {
  ASSERT(!peer_certificate_);
  ASSERT(peer_certificate_digest_algorithm_.size() == 0);
  ASSERT(ssl_server_name_.empty());
  size_t expected_len;

  if (!OpenSSLDigest::GetDigestSize(digest_alg, &expected_len)) {
    LOG(LS_WARNING) << "Unknown digest algorithm: " << digest_alg;
    return false;
  }
  if (expected_len != digest_len)
    return false;

  peer_certificate_digest_value_.SetData(digest_val, digest_len);
  peer_certificate_digest_algorithm_ = digest_alg;

  return true;
}

// Key Extractor interface
bool OpenSSLStreamAdapter::ExportKeyingMaterial(const std::string& label,
                                                const uint8* context,
                                                size_t context_len,
                                                bool use_context,
                                                uint8* result,
                                                size_t result_len) {
#ifdef HAVE_DTLS_SRTP
  int i;

  i = SSL_export_keying_material(ssl_, result, result_len,
                                 label.c_str(), label.length(),
                                 const_cast<uint8 *>(context),
                                 context_len, use_context);

  if (i != 1)
    return false;

  return true;
#else
  return false;
#endif
}

bool OpenSSLStreamAdapter::SetDtlsSrtpCiphers(
    const std::vector<std::string>& ciphers) {
#ifdef HAVE_DTLS_SRTP
  std::string internal_ciphers;

  if (state_ != SSL_NONE)
    return false;

  for (std::vector<std::string>::const_iterator cipher = ciphers.begin();
       cipher != ciphers.end(); ++cipher) {
    bool found = false;
    for (SrtpCipherMapEntry *entry = SrtpCipherMap; entry->internal_name;
         ++entry) {
      if (*cipher == entry->external_name) {
        found = true;
        if (!internal_ciphers.empty())
          internal_ciphers += ":";
        internal_ciphers += entry->internal_name;
        break;
      }
    }

    if (!found) {
      LOG(LS_ERROR) << "Could not find cipher: " << *cipher;
      return false;
    }
  }

  if (internal_ciphers.empty())
    return false;

  srtp_ciphers_ = internal_ciphers;
  return true;
#else
  return false;
#endif
}

bool OpenSSLStreamAdapter::GetDtlsSrtpCipher(std::string* cipher) {
#ifdef HAVE_DTLS_SRTP
  ASSERT(state_ == SSL_CONNECTED);
  if (state_ != SSL_CONNECTED)
    return false;

  SRTP_PROTECTION_PROFILE *srtp_profile =
      SSL_get_selected_srtp_profile(ssl_);

  if (!srtp_profile)
    return false;

  for (SrtpCipherMapEntry *entry = SrtpCipherMap;
       entry->internal_name; ++entry) {
    if (!strcmp(entry->internal_name, srtp_profile->name)) {
      *cipher = entry->external_name;
      return true;
    }
  }

  ASSERT(false);  // This should never happen

  return false;
#else
  return false;
#endif
}

int OpenSSLStreamAdapter::StartSSLWithServer(const char* server_name) {
  ASSERT(server_name != NULL && server_name[0] != '\0');
  ssl_server_name_ = server_name;
  return StartSSL();
}

int OpenSSLStreamAdapter::StartSSLWithPeer() {
  ASSERT(ssl_server_name_.empty());
  // It is permitted to specify peer_certificate_ only later.
  return StartSSL();
}

void OpenSSLStreamAdapter::SetMode(SSLMode mode) {
  ASSERT(state_ == SSL_NONE);
  ssl_mode_ = mode;
}

//
// StreamInterface Implementation
//

StreamResult OpenSSLStreamAdapter::Write(const void* data, size_t data_len,
                                         size_t* written, int* error) {
  LOG(LS_VERBOSE) << "OpenSSLStreamAdapter::Write(" << data_len << ")";

  switch (state_) {
  case SSL_NONE:
    // pass-through in clear text
    return StreamAdapterInterface::Write(data, data_len, written, error);

  case SSL_WAIT:
  case SSL_CONNECTING:
    return SR_BLOCK;

  case SSL_CONNECTED:
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
    LOG(LS_VERBOSE) << " -- success";
    ASSERT(0 < code && static_cast<unsigned>(code) <= data_len);
    if (written)
      *written = code;
    return SR_SUCCESS;
  case SSL_ERROR_WANT_READ:
    LOG(LS_VERBOSE) << " -- error want read";
    ssl_write_needs_read_ = true;
    return SR_BLOCK;
  case SSL_ERROR_WANT_WRITE:
    LOG(LS_VERBOSE) << " -- error want write";
    return SR_BLOCK;

  case SSL_ERROR_ZERO_RETURN:
  default:
    Error("SSL_write", (ssl_error ? ssl_error : -1), false);
    if (error)
      *error = ssl_error_code_;
    return SR_ERROR;
  }
  // not reached
}

StreamResult OpenSSLStreamAdapter::Read(void* data, size_t data_len,
                                        size_t* read, int* error) {
  LOG(LS_VERBOSE) << "OpenSSLStreamAdapter::Read(" << data_len << ")";
  switch (state_) {
    case SSL_NONE:
      // pass-through in clear text
      return StreamAdapterInterface::Read(data, data_len, read, error);

    case SSL_WAIT:
    case SSL_CONNECTING:
      return SR_BLOCK;

    case SSL_CONNECTED:
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
      LOG(LS_VERBOSE) << " -- success";
      ASSERT(0 < code && static_cast<unsigned>(code) <= data_len);
      if (read)
        *read = code;

      if (ssl_mode_ == SSL_MODE_DTLS) {
        // Enforce atomic reads -- this is a short read
        unsigned int pending = SSL_pending(ssl_);

        if (pending) {
          LOG(LS_INFO) << " -- short DTLS read. flushing";
          FlushInput(pending);
          if (error)
            *error = SSE_MSG_TRUNC;
          return SR_ERROR;
        }
      }
      return SR_SUCCESS;
    case SSL_ERROR_WANT_READ:
      LOG(LS_VERBOSE) << " -- error want read";
      return SR_BLOCK;
    case SSL_ERROR_WANT_WRITE:
      LOG(LS_VERBOSE) << " -- error want write";
      ssl_read_needs_write_ = true;
      return SR_BLOCK;
    case SSL_ERROR_ZERO_RETURN:
      LOG(LS_VERBOSE) << " -- remote side closed";
      return SR_EOS;
      break;
    default:
      LOG(LS_VERBOSE) << " -- error " << code;
      Error("SSL_read", (ssl_error ? ssl_error : -1), false);
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
    ASSERT(ssl_error == SSL_ERROR_NONE);

    if (ssl_error != SSL_ERROR_NONE) {
      LOG(LS_VERBOSE) << " -- error " << code;
      Error("SSL_read", (ssl_error ? ssl_error : -1), false);
      return;
    }

    LOG(LS_VERBOSE) << " -- flushed " << code << " bytes";
    left -= code;
  }
}

void OpenSSLStreamAdapter::Close() {
  Cleanup();
  ASSERT(state_ == SSL_CLOSED || state_ == SSL_ERROR);
  StreamAdapterInterface::Close();
}

StreamState OpenSSLStreamAdapter::GetState() const {
  switch (state_) {
    case SSL_WAIT:
    case SSL_CONNECTING:
      return SS_OPENING;
    case SSL_CONNECTED:
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
  ASSERT(stream == this->stream());
  if ((events & SE_OPEN)) {
    LOG(LS_VERBOSE) << "OpenSSLStreamAdapter::OnEvent SE_OPEN";
    if (state_ != SSL_WAIT) {
      ASSERT(state_ == SSL_NONE);
      events_to_signal |= SE_OPEN;
    } else {
      state_ = SSL_CONNECTING;
      if (int err = BeginSSL()) {
        Error("BeginSSL", err, true);
        return;
      }
    }
  }
  if ((events & (SE_READ|SE_WRITE))) {
    LOG(LS_VERBOSE) << "OpenSSLStreamAdapter::OnEvent"
                 << ((events & SE_READ) ? " SE_READ" : "")
                 << ((events & SE_WRITE) ? " SE_WRITE" : "");
    if (state_ == SSL_NONE) {
      events_to_signal |= events & (SE_READ|SE_WRITE);
    } else if (state_ == SSL_CONNECTING) {
      if (int err = ContinueSSL()) {
        Error("ContinueSSL", err, true);
        return;
      }
    } else if (state_ == SSL_CONNECTED) {
      if (((events & SE_READ) && ssl_write_needs_read_) ||
          (events & SE_WRITE)) {
        LOG(LS_VERBOSE) << " -- onStreamWriteable";
        events_to_signal |= SE_WRITE;
      }
      if (((events & SE_WRITE) && ssl_read_needs_write_) ||
          (events & SE_READ)) {
        LOG(LS_VERBOSE) << " -- onStreamReadable";
        events_to_signal |= SE_READ;
      }
    }
  }
  if ((events & SE_CLOSE)) {
    LOG(LS_VERBOSE) << "OpenSSLStreamAdapter::OnEvent(SE_CLOSE, " << err << ")";
    Cleanup();
    events_to_signal |= SE_CLOSE;
    // SE_CLOSE is the only event that uses the final parameter to OnEvent().
    ASSERT(signal_error == 0);
    signal_error = err;
  }
  if (events_to_signal)
    StreamAdapterInterface::OnEvent(stream, events_to_signal, signal_error);
}

int OpenSSLStreamAdapter::StartSSL() {
  ASSERT(state_ == SSL_NONE);

  if (StreamAdapterInterface::GetState() != SS_OPEN) {
    state_ = SSL_WAIT;
    return 0;
  }

  state_ = SSL_CONNECTING;
  if (int err = BeginSSL()) {
    Error("BeginSSL", err, false);
    return err;
  }

  return 0;
}

int OpenSSLStreamAdapter::BeginSSL() {
  ASSERT(state_ == SSL_CONNECTING);
  // The underlying stream has open. If we are in peer-to-peer mode
  // then a peer certificate must have been specified by now.
  ASSERT(!ssl_server_name_.empty() ||
         !peer_certificate_digest_algorithm_.empty());
  LOG(LS_INFO) << "BeginSSL: "
               << (!ssl_server_name_.empty() ? ssl_server_name_ :
                                               "with peer");

  BIO* bio = NULL;

  // First set up the context
  ASSERT(ssl_ctx_ == NULL);
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

  SSL_set_mode(ssl_, SSL_MODE_ENABLE_PARTIAL_WRITE |
               SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);

  // Specify an ECDH group for ECDHE ciphers, otherwise they cannot be
  // negotiated when acting as the server. Use NIST's P-256 which is commonly
  // supported.
  EC_KEY* ecdh = EC_KEY_new_by_curve_name(NID_X9_62_prime256v1);
  if (ecdh == NULL)
    return -1;
  SSL_set_options(ssl_, SSL_OP_SINGLE_ECDH_USE);
  SSL_set_tmp_ecdh(ssl_, ecdh);
  EC_KEY_free(ecdh);

  // Do the connect
  return ContinueSSL();
}

int OpenSSLStreamAdapter::ContinueSSL() {
  LOG(LS_VERBOSE) << "ContinueSSL";
  ASSERT(state_ == SSL_CONNECTING);

  // Clear the DTLS timer
  Thread::Current()->Clear(this, MSG_TIMEOUT);

  int code = (role_ == SSL_CLIENT) ? SSL_connect(ssl_) : SSL_accept(ssl_);
  int ssl_error;
  switch (ssl_error = SSL_get_error(ssl_, code)) {
    case SSL_ERROR_NONE:
      LOG(LS_VERBOSE) << " -- success";

      if (!SSLPostConnectionCheck(ssl_, ssl_server_name_.c_str(), NULL,
                                  peer_certificate_digest_algorithm_)) {
        LOG(LS_ERROR) << "TLS post connection check failed";
        return -1;
      }

      state_ = SSL_CONNECTED;
      StreamAdapterInterface::OnEvent(stream(), SE_OPEN|SE_READ|SE_WRITE, 0);
      break;

    case SSL_ERROR_WANT_READ: {
        LOG(LS_VERBOSE) << " -- error want read";
        struct timeval timeout;
        if (DTLSv1_get_timeout(ssl_, &timeout)) {
          int delay = timeout.tv_sec * 1000 + timeout.tv_usec/1000;

          Thread::Current()->PostDelayed(delay, this, MSG_TIMEOUT, 0);
        }
      }
      break;

    case SSL_ERROR_WANT_WRITE:
      LOG(LS_VERBOSE) << " -- error want write";
      break;

    case SSL_ERROR_ZERO_RETURN:
    default:
      LOG(LS_VERBOSE) << " -- error " << code;
      return (ssl_error != 0) ? ssl_error : -1;
  }

  return 0;
}

void OpenSSLStreamAdapter::Error(const char* context, int err, bool signal) {
  LOG(LS_WARNING) << "OpenSSLStreamAdapter::Error("
                  << context << ", " << err << ")";
  state_ = SSL_ERROR;
  ssl_error_code_ = err;
  Cleanup();
  if (signal)
    StreamAdapterInterface::OnEvent(stream(), SE_CLOSE, err);
}

void OpenSSLStreamAdapter::Cleanup() {
  LOG(LS_INFO) << "Cleanup";

  if (state_ != SSL_ERROR) {
    state_ = SSL_CLOSED;
    ssl_error_code_ = 0;
  }

  if (ssl_) {
    int ret = SSL_shutdown(ssl_);
    if (ret < 0) {
      LOG(LS_WARNING) << "SSL_shutdown failed, error = "
                      << SSL_get_error(ssl_, ret);
    }

    SSL_free(ssl_);
    ssl_ = NULL;
  }
  if (ssl_ctx_) {
    SSL_CTX_free(ssl_ctx_);
    ssl_ctx_ = NULL;
  }
  identity_.reset();
  peer_certificate_.reset();

  // Clear the DTLS timer
  Thread::Current()->Clear(this, MSG_TIMEOUT);
}


void OpenSSLStreamAdapter::OnMessage(Message* msg) {
  // Process our own messages and then pass others to the superclass
  if (MSG_TIMEOUT == msg->message_id) {
    LOG(LS_INFO) << "DTLS timeout expired";
    DTLSv1_handle_timeout(ssl_);
    ContinueSSL();
  } else {
    StreamInterface::OnMessage(msg);
  }
}

SSL_CTX* OpenSSLStreamAdapter::SetupSSLContext() {
  SSL_CTX *ctx = NULL;

  if (role_ == SSL_CLIENT) {
    ctx = SSL_CTX_new(ssl_mode_ == SSL_MODE_DTLS ?
        DTLSv1_client_method() : TLSv1_client_method());
  } else {
    ctx = SSL_CTX_new(ssl_mode_ == SSL_MODE_DTLS ?
        DTLSv1_server_method() : TLSv1_server_method());
  }
  if (ctx == NULL)
    return NULL;

  if (identity_ && !identity_->ConfigureIdentity(ctx)) {
    SSL_CTX_free(ctx);
    return NULL;
  }

#ifdef _DEBUG
  SSL_CTX_set_info_callback(ctx, OpenSSLAdapter::SSLInfoCallback);
#endif

  int mode = SSL_VERIFY_PEER;
  if (client_auth_enabled()) {
    // Require a certificate from the client.
    // Note: Normally this is always true in production, but it may be disabled
    // for testing purposes (e.g. SSLAdapter unit tests).
    mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
  }

  SSL_CTX_set_verify(ctx, mode, SSLVerifyCallback);
  SSL_CTX_set_verify_depth(ctx, 4);
  SSL_CTX_set_cipher_list(ctx, "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH");

#ifdef HAVE_DTLS_SRTP
  if (!srtp_ciphers_.empty()) {
    if (SSL_CTX_set_tlsext_use_srtp(ctx, srtp_ciphers_.c_str())) {
      SSL_CTX_free(ctx);
      return NULL;
    }
  }
#endif

  return ctx;
}

int OpenSSLStreamAdapter::SSLVerifyCallback(int ok, X509_STORE_CTX* store) {
  // Get our SSL structure from the store
  SSL* ssl = reinterpret_cast<SSL*>(X509_STORE_CTX_get_ex_data(
                                        store,
                                        SSL_get_ex_data_X509_STORE_CTX_idx()));
  OpenSSLStreamAdapter* stream =
    reinterpret_cast<OpenSSLStreamAdapter*>(SSL_get_app_data(ssl));

  if (stream->peer_certificate_digest_algorithm_.empty()) {
    return 0;
  }
  X509* cert = X509_STORE_CTX_get_current_cert(store);
  int depth = X509_STORE_CTX_get_error_depth(store);

  // For now We ignore the parent certificates and verify the leaf against
  // the digest.
  //
  // TODO(jiayl): Verify the chain is a proper chain and report the chain to
  // |stream->peer_certificate_|, like what NSS does.
  if (depth > 0) {
    LOG(LS_INFO) << "Ignored chained certificate at depth " << depth;
    return 1;
  }

  unsigned char digest[EVP_MAX_MD_SIZE];
  size_t digest_length;
  if (!OpenSSLCertificate::ComputeDigest(
           cert,
           stream->peer_certificate_digest_algorithm_,
           digest, sizeof(digest),
           &digest_length)) {
    LOG(LS_WARNING) << "Failed to compute peer cert digest.";
    return 0;
  }

  Buffer computed_digest(digest, digest_length);
  if (computed_digest != stream->peer_certificate_digest_value_) {
    LOG(LS_WARNING) << "Rejected peer certificate due to mismatched digest.";
    return 0;
  }
  // Ignore any verification error if the digest matches, since there is no
  // value in checking the validity of a self-signed cert issued by untrusted
  // sources.
  LOG(LS_INFO) << "Accepted peer certificate.";

  // Record the peer's certificate.
  stream->peer_certificate_.reset(new OpenSSLCertificate(cert));
  return 1;
}

// This code is taken from the "Network Security with OpenSSL"
// sample in chapter 5
bool OpenSSLStreamAdapter::SSLPostConnectionCheck(SSL* ssl,
                                                  const char* server_name,
                                                  const X509* peer_cert,
                                                  const std::string
                                                  &peer_digest) {
  ASSERT(server_name != NULL);
  bool ok;
  if (server_name[0] != '\0') {  // traditional mode
    ok = OpenSSLAdapter::VerifyServerName(ssl, server_name, ignore_bad_cert());

    if (ok) {
      ok = (SSL_get_verify_result(ssl) == X509_V_OK ||
            custom_verification_succeeded_);
    }
  } else {  // peer-to-peer mode
    ASSERT((peer_cert != NULL) || (!peer_digest.empty()));
    // no server name validation
    ok = true;
  }

  if (!ok && ignore_bad_cert()) {
    LOG(LS_ERROR) << "SSL_get_verify_result(ssl) = "
                  << SSL_get_verify_result(ssl);
    LOG(LS_INFO) << "Other TLS post connection checks failed.";
    ok = true;
  }

  return ok;
}

bool OpenSSLStreamAdapter::HaveDtls() {
  return true;
}

bool OpenSSLStreamAdapter::HaveDtlsSrtp() {
#ifdef HAVE_DTLS_SRTP
  return true;
#else
  return false;
#endif
}

bool OpenSSLStreamAdapter::HaveExporter() {
#ifdef HAVE_DTLS_SRTP
  return true;
#else
  return false;
#endif
}

}  // namespace rtc

#endif  // HAVE_OPENSSL_SSL_H
