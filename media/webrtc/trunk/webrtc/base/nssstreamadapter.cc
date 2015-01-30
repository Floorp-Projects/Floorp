/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <vector>

#if HAVE_CONFIG_H
#include "config.h"
#endif  // HAVE_CONFIG_H

#if HAVE_NSS_SSL_H

#include "webrtc/base/nssstreamadapter.h"

#include "keyhi.h"
#include "nspr.h"
#include "nss.h"
#include "pk11pub.h"
#include "secerr.h"

#ifdef NSS_SSL_RELATIVE_PATH
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"
#else
#include "net/third_party/nss/ssl/ssl.h"
#include "net/third_party/nss/ssl/sslerr.h"
#include "net/third_party/nss/ssl/sslproto.h"
#endif

#include "webrtc/base/nssidentity.h"
#include "webrtc/base/safe_conversions.h"
#include "webrtc/base/thread.h"

namespace rtc {

PRDescIdentity NSSStreamAdapter::nspr_layer_identity = PR_INVALID_IO_LAYER;

#define UNIMPLEMENTED \
  PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0); \
  LOG(LS_ERROR) \
  << "Call to unimplemented function "<< __FUNCTION__; ASSERT(false)

#ifdef SRTP_AES128_CM_HMAC_SHA1_80
#define HAVE_DTLS_SRTP
#endif

#ifdef HAVE_DTLS_SRTP
// SRTP cipher suite table
struct SrtpCipherMapEntry {
  const char* external_name;
  PRUint16 cipher_id;
};

// This isn't elegant, but it's better than an external reference
static const SrtpCipherMapEntry kSrtpCipherMap[] = {
  {"AES_CM_128_HMAC_SHA1_80", SRTP_AES128_CM_HMAC_SHA1_80 },
  {"AES_CM_128_HMAC_SHA1_32", SRTP_AES128_CM_HMAC_SHA1_32 },
  {NULL, 0}
};
#endif


// Implementation of NSPR methods
static PRStatus StreamClose(PRFileDesc *socket) {
  ASSERT(!socket->lower);
  socket->dtor(socket);
  return PR_SUCCESS;
}

static PRInt32 StreamRead(PRFileDesc *socket, void *buf, PRInt32 length) {
  StreamInterface *stream = reinterpret_cast<StreamInterface *>(socket->secret);
  size_t read;
  int error;
  StreamResult result = stream->Read(buf, length, &read, &error);
  if (result == SR_SUCCESS) {
    return checked_cast<PRInt32>(read);
  }

  if (result == SR_EOS) {
    return 0;
  }

  if (result == SR_BLOCK) {
    PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
    return -1;
  }

  PR_SetError(PR_UNKNOWN_ERROR, error);
  return -1;
}

static PRInt32 StreamWrite(PRFileDesc *socket, const void *buf,
                           PRInt32 length) {
  StreamInterface *stream = reinterpret_cast<StreamInterface *>(socket->secret);
  size_t written;
  int error;
  StreamResult result = stream->Write(buf, length, &written, &error);
  if (result == SR_SUCCESS) {
    return checked_cast<PRInt32>(written);
  }

  if (result == SR_BLOCK) {
    LOG(LS_INFO) <<
        "NSSStreamAdapter: write to underlying transport would block";
    PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
    return -1;
  }

  LOG(LS_ERROR) << "Write error";
  PR_SetError(PR_UNKNOWN_ERROR, error);
  return -1;
}

static PRInt32 StreamAvailable(PRFileDesc *socket) {
  UNIMPLEMENTED;
  return -1;
}

PRInt64 StreamAvailable64(PRFileDesc *socket) {
  UNIMPLEMENTED;
  return -1;
}

static PRStatus StreamSync(PRFileDesc *socket) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static PROffset32 StreamSeek(PRFileDesc *socket, PROffset32 offset,
                             PRSeekWhence how) {
  UNIMPLEMENTED;
  return -1;
}

static PROffset64 StreamSeek64(PRFileDesc *socket, PROffset64 offset,
                               PRSeekWhence how) {
  UNIMPLEMENTED;
  return -1;
}

static PRStatus StreamFileInfo(PRFileDesc *socket, PRFileInfo *info) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static PRStatus StreamFileInfo64(PRFileDesc *socket, PRFileInfo64 *info) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static PRInt32 StreamWritev(PRFileDesc *socket, const PRIOVec *iov,
                     PRInt32 iov_size, PRIntervalTime timeout) {
  UNIMPLEMENTED;
  return -1;
}

static PRStatus StreamConnect(PRFileDesc *socket, const PRNetAddr *addr,
                              PRIntervalTime timeout) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static PRFileDesc *StreamAccept(PRFileDesc *sd, PRNetAddr *addr,
                                PRIntervalTime timeout) {
  UNIMPLEMENTED;
  return NULL;
}

static PRStatus StreamBind(PRFileDesc *socket, const PRNetAddr *addr) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static PRStatus StreamListen(PRFileDesc *socket, PRIntn depth) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static PRStatus StreamShutdown(PRFileDesc *socket, PRIntn how) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

// Note: this is always nonblocking and ignores the timeout.
// TODO(ekr@rtfm.com): In future verify that the socket is
// actually in non-blocking mode.
// This function does not support peek.
static PRInt32 StreamRecv(PRFileDesc *socket, void *buf, PRInt32 amount,
                   PRIntn flags, PRIntervalTime to) {
  ASSERT(flags == 0);

  if (flags != 0) {
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return -1;
  }

  return StreamRead(socket, buf, amount);
}

// Note: this is always nonblocking and assumes a zero timeout.
// This function does not support peek.
static PRInt32 StreamSend(PRFileDesc *socket, const void *buf,
                          PRInt32 amount, PRIntn flags,
                          PRIntervalTime to) {
  ASSERT(flags == 0);

  return StreamWrite(socket, buf, amount);
}

static PRInt32 StreamRecvfrom(PRFileDesc *socket, void *buf,
                              PRInt32 amount, PRIntn flags,
                              PRNetAddr *addr, PRIntervalTime to) {
  UNIMPLEMENTED;
  return -1;
}

static PRInt32 StreamSendto(PRFileDesc *socket, const void *buf,
                            PRInt32 amount, PRIntn flags,
                            const PRNetAddr *addr, PRIntervalTime to) {
  UNIMPLEMENTED;
  return -1;
}

static PRInt16 StreamPoll(PRFileDesc *socket, PRInt16 in_flags,
                          PRInt16 *out_flags) {
  UNIMPLEMENTED;
  return -1;
}

static PRInt32 StreamAcceptRead(PRFileDesc *sd, PRFileDesc **nd,
                                PRNetAddr **raddr,
                                void *buf, PRInt32 amount, PRIntervalTime t) {
  UNIMPLEMENTED;
  return -1;
}

static PRInt32 StreamTransmitFile(PRFileDesc *sd, PRFileDesc *socket,
                                  const void *headers, PRInt32 hlen,
                                  PRTransmitFileFlags flags, PRIntervalTime t) {
  UNIMPLEMENTED;
  return -1;
}

static PRStatus StreamGetPeerName(PRFileDesc *socket, PRNetAddr *addr) {
  // TODO(ekr@rtfm.com): Modify to return unique names for each channel
  // somehow, as opposed to always the same static address. The current
  // implementation messes up the session cache, which is why it's off
  // elsewhere
  addr->inet.family = PR_AF_INET;
  addr->inet.port = 0;
  addr->inet.ip = 0;

  return PR_SUCCESS;
}

static PRStatus StreamGetSockName(PRFileDesc *socket, PRNetAddr *addr) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static PRStatus StreamGetSockOption(PRFileDesc *socket, PRSocketOptionData *opt) {
  switch (opt->option) {
    case PR_SockOpt_Nonblocking:
      opt->value.non_blocking = PR_TRUE;
      return PR_SUCCESS;
    default:
      UNIMPLEMENTED;
      break;
  }

  return PR_FAILURE;
}

// Imitate setting socket options. These are mostly noops.
static PRStatus StreamSetSockOption(PRFileDesc *socket,
                                    const PRSocketOptionData *opt) {
  switch (opt->option) {
    case PR_SockOpt_Nonblocking:
      return PR_SUCCESS;
    case PR_SockOpt_NoDelay:
      return PR_SUCCESS;
    default:
      UNIMPLEMENTED;
      break;
  }

  return PR_FAILURE;
}

static PRInt32 StreamSendfile(PRFileDesc *out, PRSendFileData *in,
                              PRTransmitFileFlags flags, PRIntervalTime to) {
  UNIMPLEMENTED;
  return -1;
}

static PRStatus StreamConnectContinue(PRFileDesc *socket, PRInt16 flags) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static PRIntn StreamReserved(PRFileDesc *socket) {
  UNIMPLEMENTED;
  return -1;
}

static const struct PRIOMethods nss_methods = {
  PR_DESC_LAYERED,
  StreamClose,
  StreamRead,
  StreamWrite,
  StreamAvailable,
  StreamAvailable64,
  StreamSync,
  StreamSeek,
  StreamSeek64,
  StreamFileInfo,
  StreamFileInfo64,
  StreamWritev,
  StreamConnect,
  StreamAccept,
  StreamBind,
  StreamListen,
  StreamShutdown,
  StreamRecv,
  StreamSend,
  StreamRecvfrom,
  StreamSendto,
  StreamPoll,
  StreamAcceptRead,
  StreamTransmitFile,
  StreamGetSockName,
  StreamGetPeerName,
  StreamReserved,
  StreamReserved,
  StreamGetSockOption,
  StreamSetSockOption,
  StreamSendfile,
  StreamConnectContinue,
  StreamReserved,
  StreamReserved,
  StreamReserved,
  StreamReserved
};

NSSStreamAdapter::NSSStreamAdapter(StreamInterface *stream)
    : SSLStreamAdapterHelper(stream),
      ssl_fd_(NULL),
      cert_ok_(false) {
}

bool NSSStreamAdapter::Init() {
  if (nspr_layer_identity == PR_INVALID_IO_LAYER) {
    nspr_layer_identity = PR_GetUniqueIdentity("nssstreamadapter");
  }
  PRFileDesc *pr_fd = PR_CreateIOLayerStub(nspr_layer_identity, &nss_methods);
  if (!pr_fd)
    return false;
  pr_fd->secret = reinterpret_cast<PRFilePrivate *>(stream());

  PRFileDesc *ssl_fd;
  if (ssl_mode_ == SSL_MODE_DTLS) {
    ssl_fd = DTLS_ImportFD(NULL, pr_fd);
  } else {
    ssl_fd = SSL_ImportFD(NULL, pr_fd);
  }
  ASSERT(ssl_fd != NULL);  // This should never happen
  if (!ssl_fd) {
    PR_Close(pr_fd);
    return false;
  }

  SECStatus rv;
  // Turn on security.
  rv = SSL_OptionSet(ssl_fd, SSL_SECURITY, PR_TRUE);
  if (rv != SECSuccess) {
    LOG(LS_ERROR) << "Error enabling security on SSL Socket";
    return false;
  }

  // Disable SSLv2.
  rv = SSL_OptionSet(ssl_fd, SSL_ENABLE_SSL2, PR_FALSE);
  if (rv != SECSuccess) {
    LOG(LS_ERROR) << "Error disabling SSL2";
    return false;
  }

  // Disable caching.
  // TODO(ekr@rtfm.com): restore this when I have the caching
  // identity set.
  rv = SSL_OptionSet(ssl_fd, SSL_NO_CACHE, PR_TRUE);
  if (rv != SECSuccess) {
    LOG(LS_ERROR) << "Error disabling cache";
    return false;
  }

  // Disable session tickets.
  rv = SSL_OptionSet(ssl_fd, SSL_ENABLE_SESSION_TICKETS, PR_FALSE);
  if (rv != SECSuccess) {
    LOG(LS_ERROR) << "Error enabling tickets";
    return false;
  }

  // Disable renegotiation.
  rv = SSL_OptionSet(ssl_fd, SSL_ENABLE_RENEGOTIATION,
                     SSL_RENEGOTIATE_NEVER);
  if (rv != SECSuccess) {
    LOG(LS_ERROR) << "Error disabling renegotiation";
    return false;
  }

  // Disable false start.
  rv = SSL_OptionSet(ssl_fd, SSL_ENABLE_FALSE_START, PR_FALSE);
  if (rv != SECSuccess) {
    LOG(LS_ERROR) << "Error disabling false start";
    return false;
  }

  ssl_fd_ = ssl_fd;

  return true;
}

NSSStreamAdapter::~NSSStreamAdapter() {
  if (ssl_fd_)
    PR_Close(ssl_fd_);
};


int NSSStreamAdapter::BeginSSL() {
  SECStatus rv;

  if (!Init()) {
    Error("Init", -1, false);
    return -1;
  }

  ASSERT(state_ == SSL_CONNECTING);
  // The underlying stream has been opened. If we are in peer-to-peer mode
  // then a peer certificate must have been specified by now.
  ASSERT(!ssl_server_name_.empty() ||
         peer_certificate_.get() != NULL ||
         !peer_certificate_digest_algorithm_.empty());
  LOG(LS_INFO) << "BeginSSL: "
               << (!ssl_server_name_.empty() ? ssl_server_name_ :
                                               "with peer");

  if (role_ == SSL_CLIENT) {
    LOG(LS_INFO) << "BeginSSL: as client";

    rv = SSL_GetClientAuthDataHook(ssl_fd_, GetClientAuthDataHook,
                                   this);
    if (rv != SECSuccess) {
      Error("BeginSSL", -1, false);
      return -1;
    }
  } else {
    LOG(LS_INFO) << "BeginSSL: as server";
    NSSIdentity *identity;

    if (identity_.get()) {
      identity = static_cast<NSSIdentity *>(identity_.get());
    } else {
      LOG(LS_ERROR) << "Can't be an SSL server without an identity";
      Error("BeginSSL", -1, false);
      return -1;
    }
    rv = SSL_ConfigSecureServer(ssl_fd_, identity->certificate().certificate(),
                                identity->keypair()->privkey(),
                                kt_rsa);
    if (rv != SECSuccess) {
      Error("BeginSSL", -1, false);
      return -1;
    }

    // Insist on a certificate from the client
    rv = SSL_OptionSet(ssl_fd_, SSL_REQUEST_CERTIFICATE, PR_TRUE);
    if (rv != SECSuccess) {
      Error("BeginSSL", -1, false);
      return -1;
    }

    // TODO(juberti): Check for client_auth_enabled()

    rv = SSL_OptionSet(ssl_fd_, SSL_REQUIRE_CERTIFICATE, PR_TRUE);
    if (rv != SECSuccess) {
      Error("BeginSSL", -1, false);
      return -1;
    }
  }

  // Set the version range.
  SSLVersionRange vrange;
  vrange.min =  (ssl_mode_ == SSL_MODE_DTLS) ?
      SSL_LIBRARY_VERSION_TLS_1_1 :
      SSL_LIBRARY_VERSION_TLS_1_0;
  vrange.max = SSL_LIBRARY_VERSION_TLS_1_1;

  rv = SSL_VersionRangeSet(ssl_fd_, &vrange);
  if (rv != SECSuccess) {
    Error("BeginSSL", -1, false);
    return -1;
  }

  // SRTP
#ifdef HAVE_DTLS_SRTP
  if (!srtp_ciphers_.empty()) {
    rv = SSL_SetSRTPCiphers(
        ssl_fd_, &srtp_ciphers_[0],
        checked_cast<unsigned int>(srtp_ciphers_.size()));
    if (rv != SECSuccess) {
      Error("BeginSSL", -1, false);
      return -1;
    }
  }
#endif

  // Certificate validation
  rv = SSL_AuthCertificateHook(ssl_fd_, AuthCertificateHook, this);
  if (rv != SECSuccess) {
    Error("BeginSSL", -1, false);
    return -1;
  }

  // Now start the handshake
  rv = SSL_ResetHandshake(ssl_fd_, role_ == SSL_SERVER ? PR_TRUE : PR_FALSE);
  if (rv != SECSuccess) {
    Error("BeginSSL", -1, false);
    return -1;
  }

  return ContinueSSL();
}

int NSSStreamAdapter::ContinueSSL() {
  LOG(LS_INFO) << "ContinueSSL";
  ASSERT(state_ == SSL_CONNECTING);

  // Clear the DTLS timer
  Thread::Current()->Clear(this, MSG_DTLS_TIMEOUT);

  SECStatus rv = SSL_ForceHandshake(ssl_fd_);

  if (rv == SECSuccess) {
    LOG(LS_INFO) << "Handshake complete";

    ASSERT(cert_ok_);
    if (!cert_ok_) {
      Error("ContinueSSL", -1, true);
      return -1;
    }

    state_ = SSL_CONNECTED;
    StreamAdapterInterface::OnEvent(stream(), SE_OPEN|SE_READ|SE_WRITE, 0);
    return 0;
  }

  PRInt32 err = PR_GetError();
  switch (err) {
    case SSL_ERROR_RX_MALFORMED_HANDSHAKE:
      if (ssl_mode_ != SSL_MODE_DTLS) {
        Error("ContinueSSL", -1, true);
        return -1;
      } else {
        LOG(LS_INFO) << "Malformed DTLS message. Ignoring.";
        // Fall through
      }
    case PR_WOULD_BLOCK_ERROR:
      LOG(LS_INFO) << "Would have blocked";
      if (ssl_mode_ == SSL_MODE_DTLS) {
        PRIntervalTime timeout;

        SECStatus rv = DTLS_GetHandshakeTimeout(ssl_fd_, &timeout);
        if (rv == SECSuccess) {
          LOG(LS_INFO) << "Timeout is " << timeout << " ms";
          Thread::Current()->PostDelayed(PR_IntervalToMilliseconds(timeout),
                                         this, MSG_DTLS_TIMEOUT, 0);
        }
      }

      return 0;
    default:
      LOG(LS_INFO) << "Error " << err;
      break;
  }

  Error("ContinueSSL", -1, true);
  return -1;
}

void NSSStreamAdapter::Cleanup() {
  if (state_ != SSL_ERROR) {
    state_ = SSL_CLOSED;
  }

  if (ssl_fd_) {
    PR_Close(ssl_fd_);
    ssl_fd_ = NULL;
  }

  identity_.reset();
  peer_certificate_.reset();

  Thread::Current()->Clear(this, MSG_DTLS_TIMEOUT);
}

StreamResult NSSStreamAdapter::Read(void* data, size_t data_len,
                                    size_t* read, int* error) {
  // SSL_CONNECTED sanity check.
  switch (state_) {
    case SSL_NONE:
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

  PRInt32 rv = PR_Read(ssl_fd_, data, checked_cast<PRInt32>(data_len));

  if (rv == 0) {
    return SR_EOS;
  }

  // Error
  if (rv < 0) {
    PRInt32 err = PR_GetError();

    switch (err) {
      case PR_WOULD_BLOCK_ERROR:
        return SR_BLOCK;
      default:
        Error("Read", -1, false);
        *error = err;  // libjingle semantics are that this is impl-specific
        return SR_ERROR;
    }
  }

  // Success
  *read = rv;

  return SR_SUCCESS;
}

StreamResult NSSStreamAdapter::Write(const void* data, size_t data_len,
                                     size_t* written, int* error) {
  // SSL_CONNECTED sanity check.
  switch (state_) {
    case SSL_NONE:
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

  PRInt32 rv = PR_Write(ssl_fd_, data, checked_cast<PRInt32>(data_len));

  // Error
  if (rv < 0) {
    PRInt32 err = PR_GetError();

    switch (err) {
      case PR_WOULD_BLOCK_ERROR:
        return SR_BLOCK;
      default:
        Error("Write", -1, false);
        *error = err;  // libjingle semantics are that this is impl-specific
        return SR_ERROR;
    }
  }

  // Success
  *written = rv;

  return SR_SUCCESS;
}

void NSSStreamAdapter::OnEvent(StreamInterface* stream, int events,
                               int err) {
  int events_to_signal = 0;
  int signal_error = 0;
  ASSERT(stream == this->stream());
  if ((events & SE_OPEN)) {
    LOG(LS_INFO) << "NSSStreamAdapter::OnEvent SE_OPEN";
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
    LOG(LS_INFO) << "NSSStreamAdapter::OnEvent"
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
      if (events & SE_WRITE) {
        LOG(LS_INFO) << " -- onStreamWriteable";
        events_to_signal |= SE_WRITE;
      }
      if (events & SE_READ) {
        LOG(LS_INFO) << " -- onStreamReadable";
        events_to_signal |= SE_READ;
      }
    }
  }
  if ((events & SE_CLOSE)) {
    LOG(LS_INFO) << "NSSStreamAdapter::OnEvent(SE_CLOSE, " << err << ")";
    Cleanup();
    events_to_signal |= SE_CLOSE;
    // SE_CLOSE is the only event that uses the final parameter to OnEvent().
    ASSERT(signal_error == 0);
    signal_error = err;
  }
  if (events_to_signal)
    StreamAdapterInterface::OnEvent(stream, events_to_signal, signal_error);
}

void NSSStreamAdapter::OnMessage(Message* msg) {
  // Process our own messages and then pass others to the superclass
  if (MSG_DTLS_TIMEOUT == msg->message_id) {
    LOG(LS_INFO) << "DTLS timeout expired";
    ContinueSSL();
  } else {
    StreamInterface::OnMessage(msg);
  }
}

// Certificate verification callback. Called to check any certificate
SECStatus NSSStreamAdapter::AuthCertificateHook(void *arg,
                                                PRFileDesc *fd,
                                                PRBool checksig,
                                                PRBool isServer) {
  LOG(LS_INFO) << "NSSStreamAdapter::AuthCertificateHook";
  // SSL_PeerCertificate returns a pointer that is owned by the caller, and
  // the NSSCertificate constructor copies its argument, so |raw_peer_cert|
  // must be destroyed in this function.
  CERTCertificate* raw_peer_cert = SSL_PeerCertificate(fd);
  NSSCertificate peer_cert(raw_peer_cert);
  CERT_DestroyCertificate(raw_peer_cert);

  NSSStreamAdapter *stream = reinterpret_cast<NSSStreamAdapter *>(arg);
  stream->cert_ok_ = false;

  // Read the peer's certificate chain.
  CERTCertList* cert_list = SSL_PeerCertificateChain(fd);
  ASSERT(cert_list != NULL);

  // If the peer provided multiple certificates, check that they form a valid
  // chain as defined by RFC 5246 Section 7.4.2: "Each following certificate
  // MUST directly certify the one preceding it.".  This check does NOT
  // verify other requirements, such as whether the chain reaches a trusted
  // root, self-signed certificates have valid signatures, certificates are not
  // expired, etc.
  // Even if the chain is valid, the leaf certificate must still match a
  // provided certificate or digest.
  if (!NSSCertificate::IsValidChain(cert_list)) {
    CERT_DestroyCertList(cert_list);
    PORT_SetError(SEC_ERROR_BAD_SIGNATURE);
    return SECFailure;
  }

  if (stream->peer_certificate_.get()) {
    LOG(LS_INFO) << "Checking against specified certificate";

    // The peer certificate was specified
    if (reinterpret_cast<NSSCertificate *>(stream->peer_certificate_.get())->
        Equals(&peer_cert)) {
      LOG(LS_INFO) << "Accepted peer certificate";
      stream->cert_ok_ = true;
    }
  } else if (!stream->peer_certificate_digest_algorithm_.empty()) {
    LOG(LS_INFO) << "Checking against specified digest";
    // The peer certificate digest was specified
    unsigned char digest[64];  // Maximum size
    size_t digest_length;

    if (!peer_cert.ComputeDigest(
            stream->peer_certificate_digest_algorithm_,
            digest, sizeof(digest), &digest_length)) {
      LOG(LS_ERROR) << "Digest computation failed";
    } else {
      Buffer computed_digest(digest, digest_length);
      if (computed_digest == stream->peer_certificate_digest_value_) {
        LOG(LS_INFO) << "Accepted peer certificate";
        stream->cert_ok_ = true;
      }
    }
  } else {
    // Other modes, but we haven't implemented yet
    // TODO(ekr@rtfm.com): Implement real certificate validation
    UNIMPLEMENTED;
  }

  if (!stream->cert_ok_ && stream->ignore_bad_cert()) {
    LOG(LS_WARNING) << "Ignoring cert error while verifying cert chain";
    stream->cert_ok_ = true;
  }

  if (stream->cert_ok_)
    stream->peer_certificate_.reset(new NSSCertificate(cert_list));

  CERT_DestroyCertList(cert_list);

  if (stream->cert_ok_)
    return SECSuccess;

  PORT_SetError(SEC_ERROR_UNTRUSTED_CERT);
  return SECFailure;
}


SECStatus NSSStreamAdapter::GetClientAuthDataHook(void *arg, PRFileDesc *fd,
                                                  CERTDistNames *caNames,
                                                  CERTCertificate **pRetCert,
                                                  SECKEYPrivateKey **pRetKey) {
  LOG(LS_INFO) << "Client cert requested";
  NSSStreamAdapter *stream = reinterpret_cast<NSSStreamAdapter *>(arg);

  if (!stream->identity_.get()) {
    LOG(LS_ERROR) << "No identity available";
    return SECFailure;
  }

  NSSIdentity *identity = static_cast<NSSIdentity *>(stream->identity_.get());
  // Destroyed internally by NSS
  *pRetCert = CERT_DupCertificate(identity->certificate().certificate());
  *pRetKey = SECKEY_CopyPrivateKey(identity->keypair()->privkey());

  return SECSuccess;
}

// RFC 5705 Key Exporter
bool NSSStreamAdapter::ExportKeyingMaterial(const std::string& label,
                                            const uint8* context,
                                            size_t context_len,
                                            bool use_context,
                                            uint8* result,
                                            size_t result_len) {
  SECStatus rv = SSL_ExportKeyingMaterial(
      ssl_fd_,
      label.c_str(),
      checked_cast<unsigned int>(label.size()),
      use_context,
      context,
      checked_cast<unsigned int>(context_len),
      result,
      checked_cast<unsigned int>(result_len));

  return rv == SECSuccess;
}

bool NSSStreamAdapter::SetDtlsSrtpCiphers(
    const std::vector<std::string>& ciphers) {
#ifdef HAVE_DTLS_SRTP
  std::vector<PRUint16> internal_ciphers;
  if (state_ != SSL_NONE)
    return false;

  for (std::vector<std::string>::const_iterator cipher = ciphers.begin();
       cipher != ciphers.end(); ++cipher) {
    bool found = false;
    for (const SrtpCipherMapEntry *entry = kSrtpCipherMap; entry->cipher_id;
         ++entry) {
      if (*cipher == entry->external_name) {
        found = true;
        internal_ciphers.push_back(entry->cipher_id);
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

bool NSSStreamAdapter::GetDtlsSrtpCipher(std::string* cipher) {
#ifdef HAVE_DTLS_SRTP
  ASSERT(state_ == SSL_CONNECTED);
  if (state_ != SSL_CONNECTED)
    return false;

  PRUint16 selected_cipher;

  SECStatus rv = SSL_GetSRTPCipher(ssl_fd_, &selected_cipher);
  if (rv == SECFailure)
    return false;

  for (const SrtpCipherMapEntry *entry = kSrtpCipherMap;
       entry->cipher_id; ++entry) {
    if (selected_cipher == entry->cipher_id) {
      *cipher = entry->external_name;
      return true;
    }
  }

  ASSERT(false);  // This should never happen
#endif
  return false;
}


bool NSSContext::initialized;
NSSContext *NSSContext::global_nss_context;

// Static initialization and shutdown
NSSContext *NSSContext::Instance() {
  if (!global_nss_context) {
    scoped_ptr<NSSContext> new_ctx(new NSSContext());
    new_ctx->slot_ = PK11_GetInternalSlot();
    if (new_ctx->slot_)
      global_nss_context = new_ctx.release();
  }
  return global_nss_context;
}



bool NSSContext::InitializeSSL(VerificationCallback callback) {
  ASSERT(!callback);

  if (!initialized) {
    SECStatus rv;

    rv = NSS_NoDB_Init(NULL);
    if (rv != SECSuccess) {
      LOG(LS_ERROR) << "Couldn't initialize NSS error=" <<
          PORT_GetError();
      return false;
    }

    NSS_SetDomesticPolicy();

    initialized = true;
  }

  return true;
}

bool NSSContext::InitializeSSLThread() {
  // Not needed
  return true;
}

bool NSSContext::CleanupSSL() {
  // Not needed
  return true;
}

bool NSSStreamAdapter::HaveDtls() {
  return true;
}

bool NSSStreamAdapter::HaveDtlsSrtp() {
#ifdef HAVE_DTLS_SRTP
  return true;
#else
  return false;
#endif
}

bool NSSStreamAdapter::HaveExporter() {
  return true;
}

}  // namespace rtc

#endif  // HAVE_NSS_SSL_H
