/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include "transportlayerdtls.h"

#include <algorithm>
#include <queue>
#include <sstream>

#include "dtlsidentity.h"
#include "keyhi.h"
#include "logging.h"
#include "mozilla/Move.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsIEventTarget.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"
#include "transportflow.h"


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

#define MAX_ALPN_LENGTH 255

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
  Packet() : data_(nullptr), len_(0) {}

  void Assign(const void *data, int32_t len) {
    data_.reset(new uint8_t[len]);
    memcpy(data_.get(), data, len);
    len_ = len;
  }

  UniquePtr<uint8_t[]> data_;
  int32_t len_;
};

void TransportLayerNSPRAdapter::PacketReceived(const void *data, int32_t len) {
  if (enabled_) {
    input_.push(new Packet());
    input_.back()->Assign(data, len);
  }
}

int32_t TransportLayerNSPRAdapter::Recv(void *buf, int32_t buflen) {
  if (input_.empty()) {
    PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
    return -1;
  }

  Packet* front = input_.front();
  if (buflen < front->len_) {
    MOZ_ASSERT(false, "Not enough buffer space to receive into");
    PR_SetError(PR_BUFFER_OVERFLOW_ERROR, 0);
    return -1;
  }

  int32_t count = front->len_;
  memcpy(buf, front->data_.get(), count);

  input_.pop();
  delete front;

  return count;
}

int32_t TransportLayerNSPRAdapter::Write(const void *buf, int32_t length) {
  if (!enabled_) {
    MOZ_MTLOG(ML_WARNING, "Writing to disabled transport layer");
    return -1;
  }

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
  f->dtor(f);
  return PR_SUCCESS;
}

static int32_t TransportLayerRead(PRFileDesc *f, void *buf, int32_t length) {
  UNIMPLEMENTED;
  return -1;
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
  // This is only called from NSS when we are the server and the client refuses
  // to provide a certificate.  In this case, the handshake is destined for
  // failure, so we will just let this pass.
  TransportLayerNSPRAdapter *io = reinterpret_cast<TransportLayerNSPRAdapter *>(f->secret);
  io->SetEnabled(false);
  return PR_SUCCESS;
}

// This function does not support peek, or waiting until `to`
static int32_t TransportLayerRecv(PRFileDesc *f, void *buf, int32_t buflen,
                                  int32_t flags, PRIntervalTime to) {
  MOZ_ASSERT(flags == 0);
  if (flags != 0) {
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return -1;
  }

  TransportLayerNSPRAdapter *io = reinterpret_cast<TransportLayerNSPRAdapter *>(f->secret);
  return io->Recv(buf, buflen);
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
  nspr_io_adapter_->SetEnabled(false);
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


// Set the permitted and default ALPN identifiers.
// The default is here to allow for peers that don't want to negotiate ALPN
// in that case, the default string will be reported from GetNegotiatedAlpn().
// Setting the default to the empty string causes the transport layer to fail
// if ALPN is not negotiated.
// Note: we only support Unicode strings here, which are encoded into UTF-8,
// even though ALPN ostensibly allows arbitrary octet sequences.
nsresult TransportLayerDtls::SetAlpn(
  const std::set<std::string>& alpn_allowed,
  const std::string& alpn_default) {

  alpn_allowed_ = alpn_allowed;
  alpn_default_ = alpn_default;

  return NS_OK;
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

// These are the named groups that we will allow.
static const SSLNamedGroup NamedGroupPreferences[] = {
  ssl_grp_ec_curve25519,
  ssl_grp_ec_secp256r1,
  ssl_grp_ec_secp384r1,
  ssl_grp_ffdhe_2048,
  ssl_grp_ffdhe_3072
};

// TODO: make sure this is called from STS. Otherwise
// we have thread safety issues
bool TransportLayerDtls::Setup() {
  CheckThread();
  SECStatus rv;

  if (!downward_) {
    MOZ_MTLOG(ML_ERROR, "DTLS layer with nothing below. This is useless");
    return false;
  }
  nspr_io_adapter_ = MakeUnique<TransportLayerNSPRAdapter>(downward_);

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

  UniquePRFileDesc pr_fd(PR_CreateIOLayerStub(transport_layer_identity,
                                              &TransportLayerMethods));
  MOZ_ASSERT(pr_fd != nullptr);
  if (!pr_fd)
    return false;
  pr_fd->secret = reinterpret_cast<PRFilePrivate *>(nspr_io_adapter_.get());

  UniquePRFileDesc ssl_fd(DTLS_ImportFD(nullptr, pr_fd.get()));
  MOZ_ASSERT(ssl_fd != nullptr);  // This should never happen
  if (!ssl_fd) {
    return false;
  }

  Unused << pr_fd.release(); // ownership transfered to ssl_fd;

  if (role_ == CLIENT) {
    MOZ_MTLOG(ML_INFO, "Setting up DTLS as client");
    rv = SSL_GetClientAuthDataHook(ssl_fd.get(), GetClientAuthDataHook,
                                   this);
    if (rv != SECSuccess) {
      MOZ_MTLOG(ML_ERROR, "Couldn't set identity");
      return false;
    }
  } else {
    MOZ_MTLOG(ML_INFO, "Setting up DTLS as server");
    // Server side
    rv = SSL_ConfigSecureServer(ssl_fd.get(), identity_->cert().get(),
                                identity_->privkey().get(),
                                identity_->auth_type());
    if (rv != SECSuccess) {
      MOZ_MTLOG(ML_ERROR, "Couldn't set identity");
      return false;
    }

    UniqueCERTCertList zero_certs(CERT_NewCertList());
    rv = SSL_SetTrustAnchors(ssl_fd.get(), zero_certs.get());
    if (rv != SECSuccess) {
      MOZ_MTLOG(ML_ERROR, "Couldn't set trust anchors");
      return false;
    }

    // Insist on a certificate from the client
    rv = SSL_OptionSet(ssl_fd.get(), SSL_REQUEST_CERTIFICATE, PR_TRUE);
    if (rv != SECSuccess) {
      MOZ_MTLOG(ML_ERROR, "Couldn't request certificate");
      return false;
    }

    rv = SSL_OptionSet(ssl_fd.get(), SSL_REQUIRE_CERTIFICATE, PR_TRUE);
    if (rv != SECSuccess) {
      MOZ_MTLOG(ML_ERROR, "Couldn't require certificate");
      return false;
    }
  }

  // Require TLS 1.1 or 1.2. Perhaps some day in the future we will allow TLS
  // 1.0 for stream modes.
  SSLVersionRange version_range = {
    SSL_LIBRARY_VERSION_TLS_1_1,
    SSL_LIBRARY_VERSION_TLS_1_2
  };

  rv = SSL_VersionRangeSet(ssl_fd.get(), &version_range);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Can't disable SSLv3");
    return false;
  }

  rv = SSL_OptionSet(ssl_fd.get(), SSL_ENABLE_SESSION_TICKETS, PR_FALSE);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't disable session tickets");
    return false;
  }

  rv = SSL_OptionSet(ssl_fd.get(), SSL_NO_CACHE, PR_TRUE);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't disable session caching");
    return false;
  }

  rv = SSL_OptionSet(ssl_fd.get(), SSL_ENABLE_DEFLATE, PR_FALSE);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't disable deflate");
    return false;
  }

  rv = SSL_OptionSet(ssl_fd.get(), SSL_ENABLE_RENEGOTIATION,
                     SSL_RENEGOTIATE_NEVER);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't disable renegotiation");
    return false;
  }

  rv = SSL_OptionSet(ssl_fd.get(), SSL_ENABLE_FALSE_START, PR_FALSE);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't disable false start");
    return false;
  }

  rv = SSL_OptionSet(ssl_fd.get(), SSL_NO_LOCKS, PR_TRUE);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't disable locks");
    return false;
  }

  rv = SSL_OptionSet(ssl_fd.get(), SSL_REUSE_SERVER_ECDHE_KEY, PR_FALSE);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't disable ECDHE key reuse");
    return false;
  }

  if (!SetupCipherSuites(ssl_fd)) {
    return false;
  }

  rv = SSL_NamedGroupConfig(ssl_fd.get(), NamedGroupPreferences,
                            mozilla::ArrayLength(NamedGroupPreferences));
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't set named groups");
    return false;
  }

  // Certificate validation
  rv = SSL_AuthCertificateHook(ssl_fd.get(), AuthCertificateHook,
                               reinterpret_cast<void *>(this));
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't set certificate validation hook");
    return false;
  }

  if (!SetupAlpn(ssl_fd)) {
    return false;
  }

  // Now start the handshake
  rv = SSL_ResetHandshake(ssl_fd.get(), role_ == SERVER ? PR_TRUE : PR_FALSE);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't reset handshake");
    return false;
  }
  ssl_fd_ = Move(ssl_fd);

  // Finally, get ready to receive data
  downward_->SignalStateChange.connect(this, &TransportLayerDtls::StateChange);
  downward_->SignalPacketReceived.connect(this, &TransportLayerDtls::PacketReceived);

  if (downward_->state() == TS_OPEN) {
    TL_SET_STATE(TS_CONNECTING);
    Handshake();
  }

  return true;
}

bool TransportLayerDtls::SetupAlpn(UniquePRFileDesc& ssl_fd) const {
  if (alpn_allowed_.empty()) {
    return true;
  }

  SECStatus rv = SSL_OptionSet(ssl_fd.get(), SSL_ENABLE_NPN, PR_FALSE);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't disable NPN");
    return false;
  }

  rv = SSL_OptionSet(ssl_fd.get(), SSL_ENABLE_ALPN, PR_TRUE);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't enable ALPN");
    return false;
  }

  unsigned char buf[MAX_ALPN_LENGTH];
  size_t offset = 0;
  for (auto tag = alpn_allowed_.begin();
       tag != alpn_allowed_.end(); ++tag) {
    if ((offset + 1 + tag->length()) >= sizeof(buf)) {
      MOZ_MTLOG(ML_ERROR, "ALPN too long");
      return false;
    }
    buf[offset++] = tag->length();
    memcpy(buf + offset, tag->c_str(), tag->length());
    offset += tag->length();
  }
  rv = SSL_SetNextProtoNego(ssl_fd.get(), buf, offset);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't set ALPN string");
    return false;
  }
  return true;
}

// Ciphers we need to enable.  These are on by default in standard firefox
// builds, but can be disabled with prefs and they aren't on in our unit tests
// since that uses NSS default configuration.
//
// Only override prefs to comply with MUST statements in the security-arch doc.
// Anything outside this list is governed by the usual combination of policy
// and user preferences.
static const uint32_t EnabledCiphers[] = {
  TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
  TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA
};

// Disable all NSS suites modes without PFS or with old and rusty ciphersuites.
// Anything outside this list is governed by the usual combination of policy
// and user preferences.
static const uint32_t DisabledCiphers[] = {
  // Bug 1310061: disable all SHA384 ciphers until fixed
  TLS_AES_256_GCM_SHA384,
  TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
  TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
  TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
  TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
  TLS_DHE_RSA_WITH_AES_256_GCM_SHA384,
  TLS_DHE_DSS_WITH_AES_256_GCM_SHA384,

  TLS_ECDHE_ECDSA_WITH_3DES_EDE_CBC_SHA,
  TLS_ECDHE_RSA_WITH_3DES_EDE_CBC_SHA,
  TLS_ECDHE_ECDSA_WITH_RC4_128_SHA,
  TLS_ECDHE_RSA_WITH_RC4_128_SHA,

  TLS_DHE_RSA_WITH_3DES_EDE_CBC_SHA,
  TLS_DHE_DSS_WITH_3DES_EDE_CBC_SHA,
  TLS_DHE_DSS_WITH_RC4_128_SHA,

  TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
  TLS_ECDH_RSA_WITH_AES_128_CBC_SHA,
  TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
  TLS_ECDH_RSA_WITH_AES_256_CBC_SHA,
  TLS_ECDH_ECDSA_WITH_3DES_EDE_CBC_SHA,
  TLS_ECDH_RSA_WITH_3DES_EDE_CBC_SHA,
  TLS_ECDH_ECDSA_WITH_RC4_128_SHA,
  TLS_ECDH_RSA_WITH_RC4_128_SHA,

  TLS_RSA_WITH_AES_128_GCM_SHA256,
  TLS_RSA_WITH_AES_256_GCM_SHA384,
  TLS_RSA_WITH_AES_128_CBC_SHA,
  TLS_RSA_WITH_AES_128_CBC_SHA256,
  TLS_RSA_WITH_CAMELLIA_128_CBC_SHA,
  TLS_RSA_WITH_AES_256_CBC_SHA,
  TLS_RSA_WITH_AES_256_CBC_SHA256,
  TLS_RSA_WITH_CAMELLIA_256_CBC_SHA,
  TLS_RSA_WITH_SEED_CBC_SHA,
  TLS_RSA_WITH_3DES_EDE_CBC_SHA,
  TLS_RSA_WITH_RC4_128_SHA,
  TLS_RSA_WITH_RC4_128_MD5,

  TLS_DHE_RSA_WITH_DES_CBC_SHA,
  TLS_DHE_DSS_WITH_DES_CBC_SHA,
  TLS_RSA_WITH_DES_CBC_SHA,

  TLS_ECDHE_ECDSA_WITH_NULL_SHA,
  TLS_ECDHE_RSA_WITH_NULL_SHA,
  TLS_ECDH_ECDSA_WITH_NULL_SHA,
  TLS_ECDH_RSA_WITH_NULL_SHA,
  TLS_RSA_WITH_NULL_SHA,
  TLS_RSA_WITH_NULL_SHA256,
  TLS_RSA_WITH_NULL_MD5,
};

bool TransportLayerDtls::SetupCipherSuites(UniquePRFileDesc& ssl_fd) const {
  SECStatus rv;

  // Set the SRTP ciphers
  if (!srtp_ciphers_.empty()) {
    // Note: std::vector is guaranteed to contiguous
    rv = SSL_SetSRTPCiphers(ssl_fd.get(), &srtp_ciphers_[0],
                            srtp_ciphers_.size());
    if (rv != SECSuccess) {
      MOZ_MTLOG(ML_ERROR, "Couldn't set SRTP cipher suite");
      return false;
    }
  }

  for (const auto& cipher : EnabledCiphers) {
    MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "Enabling: " << cipher);
    rv = SSL_CipherPrefSet(ssl_fd.get(), cipher, PR_TRUE);
    if (rv != SECSuccess) {
      MOZ_MTLOG(ML_ERROR, LAYER_INFO <<
                "Unable to enable suite: " << cipher);
      return false;
    }
  }

  for (const auto& cipher : DisabledCiphers) {
    MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "Disabling: " << cipher);

    PRBool enabled = false;
    rv = SSL_CipherPrefGet(ssl_fd.get(), cipher, &enabled);
    if (rv != SECSuccess) {
      MOZ_MTLOG(ML_NOTICE, LAYER_INFO <<
                "Unable to check if suite is enabled: " << cipher);
      return false;
    }
    if (enabled) {
      rv = SSL_CipherPrefSet(ssl_fd.get(), cipher, PR_FALSE);
      if (rv != SECSuccess) {
        MOZ_MTLOG(ML_NOTICE, LAYER_INFO <<
                  "Unable to disable suite: " << cipher);
        return false;
      }
    }
  }

  return true;
}

nsresult TransportLayerDtls::GetCipherSuite(uint16_t* cipherSuite) const {
  CheckThread();
  if (!cipherSuite) {
    MOZ_MTLOG(ML_ERROR, LAYER_INFO << "GetCipherSuite passed a nullptr");
    return NS_ERROR_NULL_POINTER;
  }
  if (state_ != TS_OPEN) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  SSLChannelInfo info;
  SECStatus rv = SSL_GetChannelInfo(ssl_fd_.get(), &info, sizeof(info));
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_NOTICE, LAYER_INFO << "GetCipherSuite can't get channel info");
    return NS_ERROR_FAILURE;
  }
  *cipherSuite = info.cipherSuite;
  return NS_OK;
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
      MOZ_MTLOG(ML_ERROR, LAYER_INFO << "Lower layer is connecting.");
      break;

    case TS_OPEN:
      MOZ_MTLOG(ML_ERROR,
                LAYER_INFO << "Lower layer is now open; starting TLS");
      // Async, since the ICE layer might need to send a STUN response, and we
      // don't want the handshake to start until that is sent.
      TL_SET_STATE(TS_CONNECTING);
      timer_->Cancel();
      timer_->SetTarget(target_);
      timer_->InitWithFuncCallback(TimerCallback,
                                   this,
                                   0,
                                   nsITimer::TYPE_ONE_SHOT);
      break;

    case TS_CLOSED:
      MOZ_MTLOG(ML_ERROR, LAYER_INFO << "Lower layer is now closed");
      TL_SET_STATE(TS_CLOSED);
      break;

    case TS_ERROR:
      MOZ_MTLOG(ML_ERROR, LAYER_INFO << "Lower layer experienced an error");
      TL_SET_STATE(TS_ERROR);
      break;
  }
}

void TransportLayerDtls::Handshake() {
  // Clear the retransmit timer
  timer_->Cancel();

  SECStatus rv = SSL_ForceHandshake(ssl_fd_.get());

  if (rv == SECSuccess) {
    MOZ_MTLOG(ML_NOTICE,
              LAYER_INFO << "****** SSL handshake completed ******");
    if (!cert_ok_) {
      MOZ_MTLOG(ML_ERROR, LAYER_INFO << "Certificate check never occurred");
      TL_SET_STATE(TS_ERROR);
      return;
    }
    if (!CheckAlpn()) {
      // Despite connecting, the connection doesn't have a valid ALPN label.
      // Forcibly close the connection so that the peer isn't left hanging
      // (assuming the close_notify isn't dropped).
      ssl_fd_ = nullptr;
      TL_SET_STATE(TS_ERROR);
      return;
    }

    TL_SET_STATE(TS_OPEN);
  } else {
    int32_t err = PR_GetError();
    switch(err) {
      case SSL_ERROR_RX_MALFORMED_HANDSHAKE:
        MOZ_MTLOG(ML_ERROR, LAYER_INFO << "Malformed DTLS message; ignoring");
        // If this were TLS (and not DTLS), this would be fatal, but
        // here we're required to ignore bad messages, so fall through
        MOZ_FALLTHROUGH;
      case PR_WOULD_BLOCK_ERROR:
        MOZ_MTLOG(ML_NOTICE, LAYER_INFO << "Handshake would have blocked");
        PRIntervalTime timeout;
        rv = DTLS_GetHandshakeTimeout(ssl_fd_.get(), &timeout);
        if (rv == SECSuccess) {
          uint32_t timeout_ms = PR_IntervalToMilliseconds(timeout);

          MOZ_MTLOG(ML_DEBUG,
                    LAYER_INFO << "Setting DTLS timeout to " << timeout_ms);
          timer_->SetTarget(target_);
          timer_->InitWithFuncCallback(TimerCallback,
                                       this, timeout_ms,
                                       nsITimer::TYPE_ONE_SHOT);
        }
        break;
      default:
        const char *err_msg = PR_ErrorToName(err);
        MOZ_MTLOG(ML_ERROR, LAYER_INFO << "DTLS handshake error " << err << " ("
                  << err_msg << ")");
        TL_SET_STATE(TS_ERROR);
        break;
    }
  }
}

// Checks if ALPN was negotiated correctly and returns false if it wasn't.
// After this returns successfully, alpn_ will be set to the negotiated
// protocol.
bool TransportLayerDtls::CheckAlpn() {
  if (alpn_allowed_.empty()) {
    return true;
  }

  SSLNextProtoState alpnState;
  char chosenAlpn[MAX_ALPN_LENGTH];
  unsigned int chosenAlpnLen;
  SECStatus rv = SSL_GetNextProto(ssl_fd_.get(), &alpnState,
                                  reinterpret_cast<unsigned char*>(chosenAlpn),
                                  &chosenAlpnLen, sizeof(chosenAlpn));
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, LAYER_INFO << "ALPN error");
    return false;
  }
  switch (alpnState) {
    case SSL_NEXT_PROTO_SELECTED:
    case SSL_NEXT_PROTO_NEGOTIATED:
      break; // OK

    case SSL_NEXT_PROTO_NO_SUPPORT:
      MOZ_MTLOG(ML_NOTICE, LAYER_INFO << "ALPN not negotiated, "
                << (alpn_default_.empty() ? "failing" : "selecting default"));
      alpn_ = alpn_default_;
      return !alpn_.empty();

    case SSL_NEXT_PROTO_NO_OVERLAP:
      // This only happens if there is a custom NPN/ALPN callback installed and
      // that callback doesn't properly handle ALPN.
      MOZ_MTLOG(ML_ERROR, LAYER_INFO << "error in ALPN selection callback");
      return false;

    case SSL_NEXT_PROTO_EARLY_VALUE:
      MOZ_CRASH("Unexpected 0-RTT ALPN value");
      return false;
  }

  // Warning: NSS won't null terminate the ALPN string for us.
  std::string chosen(chosenAlpn, chosenAlpnLen);
  MOZ_MTLOG(ML_NOTICE, LAYER_INFO << "Selected ALPN string: " << chosen);
  if (alpn_allowed_.find(chosen) == alpn_allowed_.end()) {
    // Maybe our peer chose a protocol we didn't offer (when we are client), or
    // something is seriously wrong.
    std::ostringstream ss;
    for (auto i = alpn_allowed_.begin(); i != alpn_allowed_.end(); ++i) {
      ss << (i == alpn_allowed_.begin() ? " '" : ", '") << *i << "'";
    }
    MOZ_MTLOG(ML_ERROR, LAYER_INFO << "Bad ALPN string: '" << chosen
              << "'; permitted:" << ss.str());
    return false;
  }
  alpn_ = chosen;
  return true;
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

  // not DTLS per RFC 7983
  if (data[0] < 20 || data[0] > 63) {
    return;
  }

  nspr_io_adapter_->PacketReceived(data, len);

  // If we're still connecting, try to handshake
  if (state_ == TS_CONNECTING) {
    Handshake();
  }

  // Now try a recv if we're open, since there might be data left
  if (state_ == TS_OPEN) {
    // nICEr uses a 9216 bytes buffer to allow support for jumbo frames
    unsigned char buf[9216];
    int32_t rv;
    // One packet might contain several DTLS packets
    do {
      rv = PR_Recv(ssl_fd_.get(), buf, sizeof(buf), 0, PR_INTERVAL_NO_WAIT);
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
          MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "Receive would have blocked");
        } else {
          MOZ_MTLOG(ML_NOTICE, LAYER_INFO << "NSS Error " << err);
          TL_SET_STATE(TS_ERROR);
        }
      }
    } while (rv > 0);
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

  int32_t rv = PR_Send(ssl_fd_.get(), data, len, 0, PR_INTERVAL_NO_WAIT);

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
    MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "Send would have blocked");
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

  *pRetCert = CERT_DupCertificate(stream->identity_->cert().get());
  if (!*pRetCert) {
    PR_SetError(PR_OUT_OF_MEMORY_ERROR, 0);
    return SECFailure;
  }

  *pRetKey = SECKEY_CopyPrivateKey(stream->identity_->privkey().get());
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

nsresult TransportLayerDtls::GetSrtpCipher(uint16_t *cipher) const {
  CheckThread();
  if (state_ != TS_OPEN) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  SECStatus rv = SSL_GetSRTPCipher(ssl_fd_.get(), cipher);
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
  if (state_ != TS_OPEN) {
    MOZ_ASSERT(false, "Transport must be open for ExportKeyingMaterial");
    return NS_ERROR_NOT_AVAILABLE;
  }
  SECStatus rv = SSL_ExportKeyingMaterial(ssl_fd_.get(),
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
TransportLayerDtls::CheckDigest(const RefPtr<VerificationDigest>& digest,
                                UniqueCERTCertificate& peer_cert) const {
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
  UniqueCERTCertificate peer_cert(SSL_PeerCertificate(fd));

  // We are not set up to take this being called multiple
  // times. Change this if we ever add renegotiation.
  MOZ_ASSERT(!auth_hook_called_);
  if (auth_hook_called_) {
    PR_SetError(PR_UNKNOWN_ERROR, 0);
    return SECFailure;
  }
  auth_hook_called_ = true;

  MOZ_ASSERT(verification_mode_ != VERIFY_UNSET);

  switch (verification_mode_) {
    case VERIFY_UNSET:
      // Break out to error exit
      PR_SetError(PR_UNKNOWN_ERROR, 0);
      break;

    case VERIFY_ALLOW_ALL:
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

          // Matches a digest, we are good to go
          if (rv == SECSuccess) {
            cert_ok_ = true;
            return SECSuccess;
          }
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
