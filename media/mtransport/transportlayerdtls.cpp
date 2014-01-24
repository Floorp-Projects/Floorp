/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include <queue>
#include <algorithm>

#include "logging.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"
#include "keyhi.h"

#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsIEventTarget.h"
#include "nsNetCID.h"
#include "nsComponentManagerUtils.h"
#include "nsServiceManagerUtils.h"

#include "dtlsidentity.h"
#include "transportflow.h"
#include "transportlayerdtls.h"

namespace mozilla {

MOZ_MTLOG_MODULE("mtransport")

static PRDescIdentity transport_layer_identity = PR_INVALID_IO_LAYER;

// TODO: Implement a mode for this where
// the channel is not ready until confirmed externally
// (e.g., after cert check).

#define UNIMPLEMENTED                                           \
  MOZ_MTLOG(ML_ERROR,                                           \
       "Call to unimplemented function "<< __FUNCTION__);       \
  MOZ_ASSERT(false);                                            \
  PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0)


// We need to adapt the NSPR/libssl model to the TransportFlow model.
// The former wants pull semantics and TransportFlow wants push.
//
// - A TransportLayerDtls assumes it is sitting on top of another
//   TransportLayer, which means that events come in asynchronously.
// - NSS (libssl) wants to sit on top of a PRFileDesc and poll.
// - The TransportLayerNSPRAdapter is a PRFileDesc containing a
//   FIFO.
// - When TransportLayerDtls.PacketReceived() is called, we insert
//   the packets in the FIFO and then do a PR_Recv() on the NSS
//   PRFileDesc, which eventually reads off the FIFO.
//
// All of this stuff is assumed to happen solely in a single thread
// (generally the SocketTransportService thread)
struct Packet {
  Packet() : data_(nullptr), len_(0), offset_(0) {}

  void Assign(const void *data, int32_t len) {
    data_ = new uint8_t[len];
    memcpy(data_, data, len);
    len_ = len;
  }

  ScopedDeleteArray<uint8_t> data_;
  int32_t len_;
  int32_t offset_;
};

void TransportLayerNSPRAdapter::PacketReceived(const void *data, int32_t len) {
  input_.push(new Packet());
  input_.back()->Assign(data, len);
}

int32_t TransportLayerNSPRAdapter::Read(void *data, int32_t len) {
  if (input_.empty()) {
    PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
    return TE_WOULDBLOCK;
  }

  Packet* front = input_.front();
  int32_t to_read = std::min(len, front->len_ - front->offset_);
  memcpy(data, front->data_, to_read);
  front->offset_ += to_read;

  if (front->offset_ == front->len_) {
    input_.pop();
    delete front;
  }

  return to_read;
}

int32_t TransportLayerNSPRAdapter::Write(const void *buf, int32_t length) {
  TransportResult r = output_->SendPacket(
      static_cast<const unsigned char *>(buf), length);
  if (r >= 0) {
    return r;
  }

  if (r == TE_WOULDBLOCK) {
    PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
  } else {
    PR_SetError(PR_IO_ERROR, 0);
  }

  return -1;
}


// Implementation of NSPR methods
static PRStatus TransportLayerClose(PRFileDesc *f) {
  f->secret = nullptr;
  return PR_SUCCESS;
}

static int32_t TransportLayerRead(PRFileDesc *f, void *buf, int32_t length) {
  TransportLayerNSPRAdapter *io = reinterpret_cast<TransportLayerNSPRAdapter *>(f->secret);
  return io->Read(buf, length);
}

static int32_t TransportLayerWrite(PRFileDesc *f, const void *buf, int32_t length) {
  TransportLayerNSPRAdapter *io = reinterpret_cast<TransportLayerNSPRAdapter *>(f->secret);
  return io->Write(buf, length);
}

static int32_t TransportLayerAvailable(PRFileDesc *f) {
  UNIMPLEMENTED;
  return -1;
}

int64_t TransportLayerAvailable64(PRFileDesc *f) {
  UNIMPLEMENTED;
  return -1;
}

static PRStatus TransportLayerSync(PRFileDesc *f) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static int32_t TransportLayerSeek(PRFileDesc *f, int32_t offset,
                                  PRSeekWhence how) {
  UNIMPLEMENTED;
  return -1;
}

static int64_t TransportLayerSeek64(PRFileDesc *f, int64_t offset,
                                    PRSeekWhence how) {
  UNIMPLEMENTED;
  return -1;
}

static PRStatus TransportLayerFileInfo(PRFileDesc *f, PRFileInfo *info) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static PRStatus TransportLayerFileInfo64(PRFileDesc *f, PRFileInfo64 *info) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static int32_t TransportLayerWritev(PRFileDesc *f, const PRIOVec *iov,
                                    int32_t iov_size, PRIntervalTime to) {
  UNIMPLEMENTED;
  return -1;
}

static PRStatus TransportLayerConnect(PRFileDesc *f, const PRNetAddr *addr,
                                      PRIntervalTime to) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static PRFileDesc *TransportLayerAccept(PRFileDesc *sd, PRNetAddr *addr,
                                        PRIntervalTime to) {
  UNIMPLEMENTED;
  return nullptr;
}

static PRStatus TransportLayerBind(PRFileDesc *f, const PRNetAddr *addr) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static PRStatus TransportLayerListen(PRFileDesc *f, int32_t depth) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static PRStatus TransportLayerShutdown(PRFileDesc *f, int32_t how) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

// This function does not support peek.
static int32_t TransportLayerRecv(PRFileDesc *f, void *buf, int32_t amount,
                                  int32_t flags, PRIntervalTime to) {
  MOZ_ASSERT(flags == 0);
  if (flags != 0) {
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return -1;
  }

  return TransportLayerRead(f, buf, amount);
}

// Note: this is always nonblocking and assumes a zero timeout.
static int32_t TransportLayerSend(PRFileDesc *f, const void *buf, int32_t amount,
                                  int32_t flags, PRIntervalTime to) {
  int32_t written = TransportLayerWrite(f, buf, amount);
  return written;
}

static int32_t TransportLayerRecvfrom(PRFileDesc *f, void *buf, int32_t amount,
                                      int32_t flags, PRNetAddr *addr, PRIntervalTime to) {
  UNIMPLEMENTED;
  return -1;
}

static int32_t TransportLayerSendto(PRFileDesc *f, const void *buf, int32_t amount,
                                    int32_t flags, const PRNetAddr *addr, PRIntervalTime to) {
  UNIMPLEMENTED;
  return -1;
}

static int16_t TransportLayerPoll(PRFileDesc *f, int16_t in_flags, int16_t *out_flags) {
  UNIMPLEMENTED;
  return -1;
}

static int32_t TransportLayerAcceptRead(PRFileDesc *sd, PRFileDesc **nd,
                                        PRNetAddr **raddr,
                                        void *buf, int32_t amount, PRIntervalTime t) {
  UNIMPLEMENTED;
  return -1;
}

static int32_t TransportLayerTransmitFile(PRFileDesc *sd, PRFileDesc *f,
                                          const void *headers, int32_t hlen,
                                          PRTransmitFileFlags flags, PRIntervalTime t) {
  UNIMPLEMENTED;
  return -1;
}

static PRStatus TransportLayerGetpeername(PRFileDesc *f, PRNetAddr *addr) {
  // TODO: Modify to return unique names for each channel
  // somehow, as opposed to always the same static address. The current
  // implementation messes up the session cache, which is why it's off
  // elsewhere
  addr->inet.family = PR_AF_INET;
  addr->inet.port = 0;
  addr->inet.ip = 0;

  return PR_SUCCESS;
}

static PRStatus TransportLayerGetsockname(PRFileDesc *f, PRNetAddr *addr) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static PRStatus TransportLayerGetsockoption(PRFileDesc *f, PRSocketOptionData *opt) {
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
static PRStatus TransportLayerSetsockoption(PRFileDesc *f,
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

static int32_t TransportLayerSendfile(PRFileDesc *out, PRSendFileData *in,
                                      PRTransmitFileFlags flags, PRIntervalTime to) {
  UNIMPLEMENTED;
  return -1;
}

static PRStatus TransportLayerConnectContinue(PRFileDesc *f, int16_t flags) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static int32_t TransportLayerReserved(PRFileDesc *f) {
  UNIMPLEMENTED;
  return -1;
}

static const struct PRIOMethods TransportLayerMethods = {
  PR_DESC_LAYERED,
  TransportLayerClose,
  TransportLayerRead,
  TransportLayerWrite,
  TransportLayerAvailable,
  TransportLayerAvailable64,
  TransportLayerSync,
  TransportLayerSeek,
  TransportLayerSeek64,
  TransportLayerFileInfo,
  TransportLayerFileInfo64,
  TransportLayerWritev,
  TransportLayerConnect,
  TransportLayerAccept,
  TransportLayerBind,
  TransportLayerListen,
  TransportLayerShutdown,
  TransportLayerRecv,
  TransportLayerSend,
  TransportLayerRecvfrom,
  TransportLayerSendto,
  TransportLayerPoll,
  TransportLayerAcceptRead,
  TransportLayerTransmitFile,
  TransportLayerGetsockname,
  TransportLayerGetpeername,
  TransportLayerReserved,
  TransportLayerReserved,
  TransportLayerGetsockoption,
  TransportLayerSetsockoption,
  TransportLayerSendfile,
  TransportLayerConnectContinue,
  TransportLayerReserved,
  TransportLayerReserved,
  TransportLayerReserved,
  TransportLayerReserved
};

TransportLayerDtls::~TransportLayerDtls() {
  if (timer_) {
    timer_->Cancel();
  }
}

nsresult TransportLayerDtls::InitInternal() {
  // Get the transport service as an event target
  nsresult rv;
  target_ = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);

  if (NS_FAILED(rv)) {
    MOZ_MTLOG(ML_ERROR, "Couldn't get socket transport service");
    return rv;
  }

  timer_ = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) {
    MOZ_MTLOG(ML_ERROR, "Couldn't get timer");
    return rv;
  }

  return NS_OK;
}


void TransportLayerDtls::WasInserted() {
  // Connect to the lower layers
  if (!Setup()) {
    TL_SET_STATE(TS_ERROR);
  }
}


nsresult TransportLayerDtls::SetVerificationAllowAll() {
  // Defensive programming
  if (verification_mode_ != VERIFY_UNSET)
    return NS_ERROR_ALREADY_INITIALIZED;

  verification_mode_ = VERIFY_ALLOW_ALL;

  return NS_OK;
}

nsresult
TransportLayerDtls::SetVerificationDigest(const std::string digest_algorithm,
                                          const unsigned char *digest_value,
                                          size_t digest_len) {
  // Defensive programming
  if (verification_mode_ != VERIFY_UNSET &&
      verification_mode_ != VERIFY_DIGEST) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  // Note that we do not sanity check these values for length.
  // We merely ensure they will fit into the buffer.
  // TODO: is there a Data construct we could use?
  if (digest_len > kMaxDigestLength)
    return NS_ERROR_INVALID_ARG;

  digests_.push_back(new VerificationDigest(
      digest_algorithm, digest_value, digest_len));

  verification_mode_ = VERIFY_DIGEST;

  return NS_OK;
}

// TODO: make sure this is called from STS. Otherwise
// we have thread safety issues
bool TransportLayerDtls::Setup() {
  CheckThread();
  SECStatus rv;

  if (!downward_) {
    MOZ_MTLOG(ML_ERROR, "DTLS layer with nothing below. This is useless");
    return false;
  }
  nspr_io_adapter_ = new TransportLayerNSPRAdapter(downward_);

  if (!identity_) {
    MOZ_MTLOG(ML_ERROR, "Can't start DTLS without an identity");
    return false;
  }

  if (verification_mode_ == VERIFY_UNSET) {
    MOZ_MTLOG(ML_ERROR,
              "Can't start DTLS without specifying a verification mode");
    return false;
  }

  if (transport_layer_identity == PR_INVALID_IO_LAYER) {
    transport_layer_identity = PR_GetUniqueIdentity("nssstreamadapter");
  }

  ScopedPRFileDesc pr_fd(PR_CreateIOLayerStub(transport_layer_identity,
                                              &TransportLayerMethods));
  MOZ_ASSERT(pr_fd != nullptr);
  if (!pr_fd)
    return false;
  pr_fd->secret = reinterpret_cast<PRFilePrivate *>(nspr_io_adapter_.get());

  ScopedPRFileDesc ssl_fd;
  if (mode_ == DGRAM) {
    ssl_fd = DTLS_ImportFD(nullptr, pr_fd);
  } else {
    ssl_fd = SSL_ImportFD(nullptr, pr_fd);
  }

  MOZ_ASSERT(ssl_fd != nullptr);  // This should never happen
  if (!ssl_fd) {
    return false;
  }

  pr_fd.forget(); // ownership transfered to ssl_fd;

  if (role_ == CLIENT) {
    MOZ_MTLOG(ML_DEBUG, "Setting up DTLS as client");
    rv = SSL_GetClientAuthDataHook(ssl_fd, GetClientAuthDataHook,
                                   this);
    if (rv != SECSuccess) {
      MOZ_MTLOG(ML_ERROR, "Couldn't set identity");
      return false;
    }
  } else {
    MOZ_MTLOG(ML_DEBUG, "Setting up DTLS as server");
    // Server side
    rv = SSL_ConfigSecureServer(ssl_fd, identity_->cert(),
                                identity_->privkey(),
                                kt_rsa);
    if (rv != SECSuccess) {
      MOZ_MTLOG(ML_ERROR, "Couldn't set identity");
      return false;
    }

    // Insist on a certificate from the client
    rv = SSL_OptionSet(ssl_fd, SSL_REQUEST_CERTIFICATE, PR_TRUE);
    if (rv != SECSuccess) {
      MOZ_MTLOG(ML_ERROR, "Couldn't request certificate");
      return false;
    }

    rv = SSL_OptionSet(ssl_fd, SSL_REQUIRE_CERTIFICATE, PR_TRUE);
    if (rv != SECSuccess) {
      MOZ_MTLOG(ML_ERROR, "Couldn't require certificate");
      return false;
    }
  }

  // Require TLS 1.1. Perhaps some day in the future we will allow
  // TLS 1.0 for stream modes.
  SSLVersionRange version_range = {
    SSL_LIBRARY_VERSION_TLS_1_1,
    SSL_LIBRARY_VERSION_TLS_1_1
  };

  rv = SSL_VersionRangeSet(ssl_fd, &version_range);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Can't disable SSLv3");
    return false;
  }

  rv = SSL_OptionSet(ssl_fd, SSL_ENABLE_SESSION_TICKETS, PR_FALSE);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't disable session tickets");
    return false;
  }

  rv = SSL_OptionSet(ssl_fd, SSL_NO_CACHE, PR_TRUE);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't disable session caching");
    return false;
  }

  rv = SSL_OptionSet(ssl_fd, SSL_ENABLE_DEFLATE, PR_FALSE);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't disable deflate");
    return false;
  }

  rv = SSL_OptionSet(ssl_fd, SSL_ENABLE_RENEGOTIATION, SSL_RENEGOTIATE_NEVER);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't disable renegotiation");
    return false;
  }

  rv = SSL_OptionSet(ssl_fd, SSL_ENABLE_FALSE_START, PR_FALSE);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't disable false start");
    return false;
  }

  rv = SSL_OptionSet(ssl_fd, SSL_NO_LOCKS, PR_TRUE);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't disable locks");
    return false;
  }

  // Set the SRTP ciphers
  if (srtp_ciphers_.size()) {
    // Note: std::vector is guaranteed to contiguous
    rv = SSL_SetSRTPCiphers(ssl_fd, &srtp_ciphers_[0],
                            srtp_ciphers_.size());

    if (rv != SECSuccess) {
      MOZ_MTLOG(ML_ERROR, "Couldn't set SRTP cipher suite");
      return false;
    }
  }

  // Certificate validation
  rv = SSL_AuthCertificateHook(ssl_fd, AuthCertificateHook,
                               reinterpret_cast<void *>(this));
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't set certificate validation hook");
    return false;
  }

  // Now start the handshake
  rv = SSL_ResetHandshake(ssl_fd, role_ == SERVER ? PR_TRUE : PR_FALSE);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't reset handshake");
    return false;
  }
  ssl_fd_ = ssl_fd.forget();

  // Finally, get ready to receive data
  downward_->SignalStateChange.connect(this, &TransportLayerDtls::StateChange);
  downward_->SignalPacketReceived.connect(this, &TransportLayerDtls::PacketReceived);

  if (downward_->state() == TS_OPEN) {
    Handshake();
  }

  return true;
}


void TransportLayerDtls::StateChange(TransportLayer *layer, State state) {
  if (state <= state_) {
    MOZ_MTLOG(ML_ERROR, "Lower layer state is going backwards from ours");
    TL_SET_STATE(TS_ERROR);
    return;
  }

  switch (state) {
    case TS_NONE:
      MOZ_ASSERT(false);  // Can't happen
      break;

    case TS_INIT:
      MOZ_MTLOG(ML_ERROR,
                LAYER_INFO << "State change of lower layer to INIT forbidden");
      TL_SET_STATE(TS_ERROR);
      break;

    case TS_CONNECTING:
      MOZ_MTLOG(ML_ERROR, LAYER_INFO << "Lower lower is connecting.");
      break;

    case TS_OPEN:
      MOZ_MTLOG(ML_ERROR,
                LAYER_INFO << "Lower lower is now open; starting TLS");
      Handshake();
      break;

    case TS_CLOSED:
      MOZ_MTLOG(ML_ERROR, LAYER_INFO << "Lower lower is now closed");
      TL_SET_STATE(TS_CLOSED);
      break;

    case TS_ERROR:
      MOZ_MTLOG(ML_ERROR, LAYER_INFO << "Lower lower experienced an error");
      TL_SET_STATE(TS_ERROR);
      break;
  }
}

void TransportLayerDtls::Handshake() {
  TL_SET_STATE(TS_CONNECTING);

  // Clear the retransmit timer
  timer_->Cancel();

  SECStatus rv = SSL_ForceHandshake(ssl_fd_);

  if (rv == SECSuccess) {
    MOZ_MTLOG(ML_NOTICE,
              LAYER_INFO << "****** SSL handshake completed ******");
    if (!cert_ok_) {
      MOZ_MTLOG(ML_ERROR, LAYER_INFO << "Certificate check never occurred");
      TL_SET_STATE(TS_ERROR);
      return;
    }
    TL_SET_STATE(TS_OPEN);
  } else {
    int32_t err = PR_GetError();
    switch(err) {
      case SSL_ERROR_RX_MALFORMED_HANDSHAKE:
        if (mode_ != DGRAM) {
          MOZ_MTLOG(ML_ERROR, LAYER_INFO << "Malformed TLS message");
          TL_SET_STATE(TS_ERROR);
        } else {
          MOZ_MTLOG(ML_ERROR, LAYER_INFO << "Malformed DTLS message; ignoring");
        }
        // Fall through
      case PR_WOULD_BLOCK_ERROR:
        MOZ_MTLOG(ML_NOTICE, LAYER_INFO << "Would have blocked");
        if (mode_ == DGRAM) {
          PRIntervalTime timeout;
          rv = DTLS_GetHandshakeTimeout(ssl_fd_, &timeout);
          if (rv == SECSuccess) {
            uint32_t timeout_ms = PR_IntervalToMilliseconds(timeout);

            MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "Setting DTLS timeout to " <<
                 timeout_ms);
            timer_->SetTarget(target_);
            timer_->InitWithFuncCallback(TimerCallback,
                                         this, timeout_ms,
                                         nsITimer::TYPE_ONE_SHOT);
          }
        }
        break;
      default:
        MOZ_MTLOG(ML_ERROR, LAYER_INFO << "SSL handshake error "<< err);
        TL_SET_STATE(TS_ERROR);
        break;
    }
  }
}

void TransportLayerDtls::PacketReceived(TransportLayer* layer,
                                        const unsigned char *data,
                                        size_t len) {
  CheckThread();
  MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "PacketReceived(" << len << ")");

  if (state_ != TS_CONNECTING && state_ != TS_OPEN) {
    MOZ_MTLOG(ML_DEBUG,
              LAYER_INFO << "Discarding packet in inappropriate state");
    return;
  }

  nspr_io_adapter_->PacketReceived(data, len);

  // If we're still connecting, try to handshake
  if (state_ == TS_CONNECTING) {
    Handshake();
  }

  // Now try a recv if we're open, since there might be data left
  if (state_ == TS_OPEN) {
    unsigned char buf[2000];

    int32_t rv = PR_Recv(ssl_fd_, buf, sizeof(buf), 0, PR_INTERVAL_NO_WAIT);
    if (rv > 0) {
      // We have data
      MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "Read " << rv << " bytes from NSS");
      SignalPacketReceived(this, buf, rv);
    } else if (rv == 0) {
      TL_SET_STATE(TS_CLOSED);
    } else {
      int32_t err = PR_GetError();

      if (err == PR_WOULD_BLOCK_ERROR) {
        // This gets ignored
        MOZ_MTLOG(ML_NOTICE, LAYER_INFO << "Would have blocked");
      } else {
        MOZ_MTLOG(ML_NOTICE, LAYER_INFO << "NSS Error " << err);
        TL_SET_STATE(TS_ERROR);
      }
    }
  }
}

TransportResult TransportLayerDtls::SendPacket(const unsigned char *data,
                                               size_t len) {
  CheckThread();
  if (state_ != TS_OPEN) {
    MOZ_MTLOG(ML_ERROR, LAYER_INFO << "Can't call SendPacket() in state "
              << state_);
    return TE_ERROR;
  }

  int32_t rv = PR_Send(ssl_fd_, data, len, 0, PR_INTERVAL_NO_WAIT);

  if (rv > 0) {
    // We have data
    MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "Wrote " << rv << " bytes to SSL Layer");
    return rv;
  }

  if (rv == 0) {
    TL_SET_STATE(TS_CLOSED);
    return 0;
  }

  int32_t err = PR_GetError();

  if (err == PR_WOULD_BLOCK_ERROR) {
    // This gets ignored
    MOZ_MTLOG(ML_NOTICE, LAYER_INFO << "Would have blocked");
    return TE_WOULDBLOCK;
  }

  MOZ_MTLOG(ML_NOTICE, LAYER_INFO << "NSS Error " << err);
  TL_SET_STATE(TS_ERROR);
  return TE_ERROR;
}

SECStatus TransportLayerDtls::GetClientAuthDataHook(void *arg, PRFileDesc *fd,
                                                    CERTDistNames *caNames,
                                                    CERTCertificate **pRetCert,
                                                    SECKEYPrivateKey **pRetKey) {
  MOZ_MTLOG(ML_DEBUG, "Server requested client auth");

  TransportLayerDtls *stream = reinterpret_cast<TransportLayerDtls *>(arg);
  stream->CheckThread();

  if (!stream->identity_) {
    MOZ_MTLOG(ML_ERROR, "No identity available");
    PR_SetError(SSL_ERROR_NO_CERTIFICATE, 0);
    return SECFailure;
  }

  *pRetCert = CERT_DupCertificate(stream->identity_->cert());
  if (!*pRetCert) {
    PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    return SECFailure;
  }

  *pRetKey = SECKEY_CopyPrivateKey(stream->identity_->privkey());
  if (!*pRetKey) {
    CERT_DestroyCertificate(*pRetCert);
    *pRetCert = nullptr;
    PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    return SECFailure;
  }

  return SECSuccess;
}

nsresult TransportLayerDtls::SetSrtpCiphers(std::vector<uint16_t> ciphers) {
  // TODO: We should check these
  srtp_ciphers_ = ciphers;

  return NS_OK;
}

nsresult TransportLayerDtls::GetSrtpCipher(uint16_t *cipher) {
  CheckThread();
  SECStatus rv = SSL_GetSRTPCipher(ssl_fd_, cipher);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_DEBUG, "No SRTP cipher negotiated");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

nsresult TransportLayerDtls::ExportKeyingMaterial(const std::string& label,
                                                  bool use_context,
                                                  const std::string& context,
                                                  unsigned char *out,
                                                  unsigned int outlen) {
  CheckThread();
  SECStatus rv = SSL_ExportKeyingMaterial(ssl_fd_,
                                          label.c_str(),
                                          label.size(),
                                          use_context,
                                          reinterpret_cast<const unsigned char *>(
                                              context.c_str()),
                                          context.size(),
                                          out,
                                          outlen);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't export SSL keying material");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

SECStatus TransportLayerDtls::AuthCertificateHook(void *arg,
                                                  PRFileDesc *fd,
                                                  PRBool checksig,
                                                  PRBool isServer) {
  TransportLayerDtls *stream = reinterpret_cast<TransportLayerDtls *>(arg);
  stream->CheckThread();
  return stream->AuthCertificateHook(fd, checksig, isServer);
}

SECStatus
TransportLayerDtls::CheckDigest(const RefPtr<VerificationDigest>&
                                digest,
                                CERTCertificate *peer_cert) {
  unsigned char computed_digest[kMaxDigestLength];
  size_t computed_digest_len;

  MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "Checking digest, algorithm="
            << digest->algorithm_);
  nsresult res =
      DtlsIdentity::ComputeFingerprint(peer_cert,
                                       digest->algorithm_,
                                       computed_digest,
                                       sizeof(computed_digest),
                                       &computed_digest_len);
  if (NS_FAILED(res)) {
    MOZ_MTLOG(ML_ERROR, "Could not compute peer fingerprint for digest " <<
              digest->algorithm_);
    // Go to end
    PR_SetError(SSL_ERROR_BAD_CERTIFICATE, 0);
    return SECFailure;
  }

  if (computed_digest_len != digest->len_) {
    MOZ_MTLOG(ML_ERROR, "Digest is wrong length " << digest->len_ <<
              " should be " << computed_digest_len << " for algorithm " <<
              digest->algorithm_);
    PR_SetError(SSL_ERROR_BAD_CERTIFICATE, 0);
    return SECFailure;
  }

  if (memcmp(digest->value_, computed_digest, computed_digest_len) != 0) {
    MOZ_MTLOG(ML_ERROR, "Digest does not match");
    PR_SetError(SSL_ERROR_BAD_CERTIFICATE, 0);
    return SECFailure;
  }

  return SECSuccess;
}


SECStatus TransportLayerDtls::AuthCertificateHook(PRFileDesc *fd,
                                                  PRBool checksig,
                                                  PRBool isServer) {
  CheckThread();
  ScopedCERTCertificate peer_cert;
  peer_cert = SSL_PeerCertificate(fd);


  // We are not set up to take this being called multiple
  // times. Change this if we ever add renegotiation.
  MOZ_ASSERT(!auth_hook_called_);
  if (auth_hook_called_) {
    PR_SetError(PR_UNKNOWN_ERROR, 0);
    return SECFailure;
  }
  auth_hook_called_ = true;

  MOZ_ASSERT(verification_mode_ != VERIFY_UNSET);
  MOZ_ASSERT(peer_cert_ == nullptr);

  switch (verification_mode_) {
    case VERIFY_UNSET:
      // Break out to error exit
      PR_SetError(PR_UNKNOWN_ERROR, 0);
      break;

    case VERIFY_ALLOW_ALL:
      peer_cert_ = peer_cert.forget();
      cert_ok_ = true;
      return SECSuccess;

    case VERIFY_DIGEST:
      {
        MOZ_ASSERT(digests_.size() != 0);
        // Check all the provided digests

        // Checking functions call PR_SetError()
        SECStatus rv = SECFailure;
        for (size_t i = 0; i < digests_.size(); i++) {
          RefPtr<VerificationDigest> digest = digests_[i];
          rv = CheckDigest(digest, peer_cert);

          if (rv != SECSuccess)
            break;
        }

        if (rv == SECSuccess) {
          // Matches all digests, we are good to go
          cert_ok_ = true;
          peer_cert = peer_cert.forget();
          return SECSuccess;
        }
      }
      break;
    default:
      MOZ_CRASH();  // Can't happen
  }

  return SECFailure;
}

void TransportLayerDtls::TimerCallback(nsITimer *timer, void *arg) {
  TransportLayerDtls *dtls = reinterpret_cast<TransportLayerDtls *>(arg);

  MOZ_MTLOG(ML_DEBUG, "DTLS timer expired");

  dtls->Handshake();
}

}  // close namespace
