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
#include "mozilla/Telemetry.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Unused.h"
#include "nsCOMPtr.h"
#include "nsComponentManagerUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsIEventTarget.h"
#include "nsNetCID.h"
#include "nsServiceManagerUtils.h"
#include "sslexp.h"
#include "sslproto.h"
#include "transportflow.h"

namespace mozilla {

MOZ_MTLOG_MODULE("mtransport")

static PRDescIdentity transport_layer_identity = PR_INVALID_IO_LAYER;

// TODO: Implement a mode for this where
// the channel is not ready until confirmed externally
// (e.g., after cert check).

#define UNIMPLEMENTED                                                     \
  MOZ_MTLOG(ML_ERROR, "Call to unimplemented function " << __FUNCTION__); \
  MOZ_ASSERT(false);                                                      \
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

void TransportLayerNSPRAdapter::PacketReceived(MediaPacket& packet) {
  if (enabled_) {
    input_.push(new MediaPacket(std::move(packet)));
  }
}

int32_t TransportLayerNSPRAdapter::Recv(void* buf, int32_t buflen) {
  if (input_.empty()) {
    PR_SetError(PR_WOULD_BLOCK_ERROR, 0);
    return -1;
  }

  MediaPacket* front = input_.front();
  int32_t count = static_cast<int32_t>(front->len());

  if (buflen < count) {
    MOZ_ASSERT(false, "Not enough buffer space to receive into");
    PR_SetError(PR_BUFFER_OVERFLOW_ERROR, 0);
    return -1;
  }

  memcpy(buf, front->data(), count);

  input_.pop();
  delete front;

  return count;
}

int32_t TransportLayerNSPRAdapter::Write(const void* buf, int32_t length) {
  if (!enabled_) {
    MOZ_MTLOG(ML_WARNING, "Writing to disabled transport layer");
    return -1;
  }

  MediaPacket packet;
  // Copies. Oh well.
  packet.Copy(static_cast<const uint8_t*>(buf), static_cast<size_t>(length));
  packet.SetType(MediaPacket::DTLS);

  TransportResult r = output_->SendPacket(packet);
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
static PRStatus TransportLayerClose(PRFileDesc* f) {
  f->dtor(f);
  return PR_SUCCESS;
}

static int32_t TransportLayerRead(PRFileDesc* f, void* buf, int32_t length) {
  UNIMPLEMENTED;
  return -1;
}

static int32_t TransportLayerWrite(PRFileDesc* f, const void* buf,
                                   int32_t length) {
  TransportLayerNSPRAdapter* io =
      reinterpret_cast<TransportLayerNSPRAdapter*>(f->secret);
  return io->Write(buf, length);
}

static int32_t TransportLayerAvailable(PRFileDesc* f) {
  UNIMPLEMENTED;
  return -1;
}

int64_t TransportLayerAvailable64(PRFileDesc* f) {
  UNIMPLEMENTED;
  return -1;
}

static PRStatus TransportLayerSync(PRFileDesc* f) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static int32_t TransportLayerSeek(PRFileDesc* f, int32_t offset,
                                  PRSeekWhence how) {
  UNIMPLEMENTED;
  return -1;
}

static int64_t TransportLayerSeek64(PRFileDesc* f, int64_t offset,
                                    PRSeekWhence how) {
  UNIMPLEMENTED;
  return -1;
}

static PRStatus TransportLayerFileInfo(PRFileDesc* f, PRFileInfo* info) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static PRStatus TransportLayerFileInfo64(PRFileDesc* f, PRFileInfo64* info) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static int32_t TransportLayerWritev(PRFileDesc* f, const PRIOVec* iov,
                                    int32_t iov_size, PRIntervalTime to) {
  UNIMPLEMENTED;
  return -1;
}

static PRStatus TransportLayerConnect(PRFileDesc* f, const PRNetAddr* addr,
                                      PRIntervalTime to) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static PRFileDesc* TransportLayerAccept(PRFileDesc* sd, PRNetAddr* addr,
                                        PRIntervalTime to) {
  UNIMPLEMENTED;
  return nullptr;
}

static PRStatus TransportLayerBind(PRFileDesc* f, const PRNetAddr* addr) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static PRStatus TransportLayerListen(PRFileDesc* f, int32_t depth) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static PRStatus TransportLayerShutdown(PRFileDesc* f, int32_t how) {
  // This is only called from NSS when we are the server and the client refuses
  // to provide a certificate.  In this case, the handshake is destined for
  // failure, so we will just let this pass.
  TransportLayerNSPRAdapter* io =
      reinterpret_cast<TransportLayerNSPRAdapter*>(f->secret);
  io->SetEnabled(false);
  return PR_SUCCESS;
}

// This function does not support peek, or waiting until `to`
static int32_t TransportLayerRecv(PRFileDesc* f, void* buf, int32_t buflen,
                                  int32_t flags, PRIntervalTime to) {
  MOZ_ASSERT(flags == 0);
  if (flags != 0) {
    PR_SetError(PR_NOT_IMPLEMENTED_ERROR, 0);
    return -1;
  }

  TransportLayerNSPRAdapter* io =
      reinterpret_cast<TransportLayerNSPRAdapter*>(f->secret);
  return io->Recv(buf, buflen);
}

// Note: this is always nonblocking and assumes a zero timeout.
static int32_t TransportLayerSend(PRFileDesc* f, const void* buf,
                                  int32_t amount, int32_t flags,
                                  PRIntervalTime to) {
  int32_t written = TransportLayerWrite(f, buf, amount);
  return written;
}

static int32_t TransportLayerRecvfrom(PRFileDesc* f, void* buf, int32_t amount,
                                      int32_t flags, PRNetAddr* addr,
                                      PRIntervalTime to) {
  UNIMPLEMENTED;
  return -1;
}

static int32_t TransportLayerSendto(PRFileDesc* f, const void* buf,
                                    int32_t amount, int32_t flags,
                                    const PRNetAddr* addr, PRIntervalTime to) {
  UNIMPLEMENTED;
  return -1;
}

static int16_t TransportLayerPoll(PRFileDesc* f, int16_t in_flags,
                                  int16_t* out_flags) {
  UNIMPLEMENTED;
  return -1;
}

static int32_t TransportLayerAcceptRead(PRFileDesc* sd, PRFileDesc** nd,
                                        PRNetAddr** raddr, void* buf,
                                        int32_t amount, PRIntervalTime t) {
  UNIMPLEMENTED;
  return -1;
}

static int32_t TransportLayerTransmitFile(PRFileDesc* sd, PRFileDesc* f,
                                          const void* headers, int32_t hlen,
                                          PRTransmitFileFlags flags,
                                          PRIntervalTime t) {
  UNIMPLEMENTED;
  return -1;
}

static PRStatus TransportLayerGetpeername(PRFileDesc* f, PRNetAddr* addr) {
  // TODO: Modify to return unique names for each channel
  // somehow, as opposed to always the same static address. The current
  // implementation messes up the session cache, which is why it's off
  // elsewhere
  addr->inet.family = PR_AF_INET;
  addr->inet.port = 0;
  addr->inet.ip = 0;

  return PR_SUCCESS;
}

static PRStatus TransportLayerGetsockname(PRFileDesc* f, PRNetAddr* addr) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static PRStatus TransportLayerGetsockoption(PRFileDesc* f,
                                            PRSocketOptionData* opt) {
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
static PRStatus TransportLayerSetsockoption(PRFileDesc* f,
                                            const PRSocketOptionData* opt) {
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

static int32_t TransportLayerSendfile(PRFileDesc* out, PRSendFileData* in,
                                      PRTransmitFileFlags flags,
                                      PRIntervalTime to) {
  UNIMPLEMENTED;
  return -1;
}

static PRStatus TransportLayerConnectContinue(PRFileDesc* f, int16_t flags) {
  UNIMPLEMENTED;
  return PR_FAILURE;
}

static int32_t TransportLayerReserved(PRFileDesc* f) {
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
    TransportLayerReserved};

TransportLayerDtls::~TransportLayerDtls() {
  // Destroy the NSS instance first so it can still send out an alert before
  // we disable the nspr_io_adapter_.
  ssl_fd_ = nullptr;
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

  timer_ = NS_NewTimer();
  if (!timer_) {
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
nsresult TransportLayerDtls::SetAlpn(const std::set<std::string>& alpn_allowed,
                                     const std::string& alpn_default) {
  alpn_allowed_ = alpn_allowed;
  alpn_default_ = alpn_default;

  return NS_OK;
}

nsresult TransportLayerDtls::SetVerificationAllowAll() {
  // Defensive programming
  if (verification_mode_ != VERIFY_UNSET) return NS_ERROR_ALREADY_INITIALIZED;

  verification_mode_ = VERIFY_ALLOW_ALL;

  return NS_OK;
}

nsresult TransportLayerDtls::SetVerificationDigest(const DtlsDigest& digest) {
  // Defensive programming
  if (verification_mode_ != VERIFY_UNSET &&
      verification_mode_ != VERIFY_DIGEST) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }

  digests_.push_back(digest);
  verification_mode_ = VERIFY_DIGEST;
  return NS_OK;
}

// These are the named groups that we will allow.
static const SSLNamedGroup NamedGroupPreferences[] = {
    ssl_grp_ec_curve25519, ssl_grp_ec_secp256r1, ssl_grp_ec_secp384r1,
    ssl_grp_ffdhe_2048, ssl_grp_ffdhe_3072};

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

  UniquePRFileDesc pr_fd(
      PR_CreateIOLayerStub(transport_layer_identity, &TransportLayerMethods));
  MOZ_ASSERT(pr_fd != nullptr);
  if (!pr_fd) return false;
  pr_fd->secret = reinterpret_cast<PRFilePrivate*>(nspr_io_adapter_.get());

  UniquePRFileDesc ssl_fd(DTLS_ImportFD(nullptr, pr_fd.get()));
  MOZ_ASSERT(ssl_fd != nullptr);  // This should never happen
  if (!ssl_fd) {
    return false;
  }

  Unused << pr_fd.release();  // ownership transfered to ssl_fd;

  if (role_ == CLIENT) {
    MOZ_MTLOG(ML_INFO, "Setting up DTLS as client");
    rv = SSL_GetClientAuthDataHook(ssl_fd.get(), GetClientAuthDataHook, this);
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
  SSLVersionRange version_range = {SSL_LIBRARY_VERSION_TLS_1_1,
                                   SSL_LIBRARY_VERSION_TLS_1_2};

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
                               reinterpret_cast<void*>(this));
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
  ssl_fd_ = std::move(ssl_fd);

  // Finally, get ready to receive data
  downward_->SignalStateChange.connect(this, &TransportLayerDtls::StateChange);
  downward_->SignalPacketReceived.connect(this,
                                          &TransportLayerDtls::PacketReceived);

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
  for (const auto& tag : alpn_allowed_) {
    if ((offset + 1 + tag.length()) >= sizeof(buf)) {
      MOZ_MTLOG(ML_ERROR, "ALPN too long");
      return false;
    }
    buf[offset++] = tag.length();
    memcpy(buf + offset, tag.c_str(), tag.length());
    offset += tag.length();
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
    TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA};

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

    TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
    TLS_DHE_RSA_WITH_AES_256_CBC_SHA,

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

bool TransportLayerDtls::SetupCipherSuites(UniquePRFileDesc& ssl_fd) {
  SECStatus rv;

  // Set the SRTP ciphers
  if (!enabled_srtp_ciphers_.empty()) {
    rv = SSL_InstallExtensionHooks(ssl_fd.get(), ssl_use_srtp_xtn,
                                   TransportLayerDtls::WriteSrtpXtn, this,
                                   TransportLayerDtls::HandleSrtpXtn, this);
    if (rv != SECSuccess) {
      MOZ_MTLOG(ML_ERROR, LAYER_INFO << "unable to set SRTP extension handler");
      return false;
    }
  }

  for (const auto& cipher : EnabledCiphers) {
    MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "Enabling: " << cipher);
    rv = SSL_CipherPrefSet(ssl_fd.get(), cipher, PR_TRUE);
    if (rv != SECSuccess) {
      MOZ_MTLOG(ML_ERROR, LAYER_INFO << "Unable to enable suite: " << cipher);
      return false;
    }
  }

  for (const auto& cipher : DisabledCiphers) {
    MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "Disabling: " << cipher);

    PRBool enabled = false;
    rv = SSL_CipherPrefGet(ssl_fd.get(), cipher, &enabled);
    if (rv != SECSuccess) {
      MOZ_MTLOG(ML_NOTICE, LAYER_INFO << "Unable to check if suite is enabled: "
                                      << cipher);
      return false;
    }
    if (enabled) {
      rv = SSL_CipherPrefSet(ssl_fd.get(), cipher, PR_FALSE);
      if (rv != SECSuccess) {
        MOZ_MTLOG(ML_NOTICE,
                  LAYER_INFO << "Unable to disable suite: " << cipher);
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

std::vector<uint16_t> TransportLayerDtls::GetDefaultSrtpCiphers() {
  std::vector<uint16_t> ciphers;

  ciphers.push_back(kDtlsSrtpAeadAes128Gcm);
  // Since we don't support DTLS 1.3 or SHA384 ciphers (see bug 1312976)
  // we don't really enough entropy to prefer this over 128 bit
  ciphers.push_back(kDtlsSrtpAeadAes256Gcm);
  ciphers.push_back(kDtlsSrtpAes128CmHmacSha1_80);
#ifndef NIGHTLY_BUILD
  // To support bug 1491583 lets try to find out if we get bug reports if we no
  // longer offer this in Nightly builds.
  ciphers.push_back(kDtlsSrtpAes128CmHmacSha1_32);
#endif

  return ciphers;
}

void TransportLayerDtls::StateChange(TransportLayer* layer, State state) {
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
      MOZ_MTLOG(ML_INFO, LAYER_INFO << "Lower layer is connecting.");
      break;

    case TS_OPEN:
      if (timer_) {
        MOZ_MTLOG(ML_INFO,
                  LAYER_INFO << "Lower layer is now open; starting TLS");
        timer_->Cancel();
        timer_->SetTarget(target_);
        // Async, since the ICE layer might need to send a STUN response, and we
        // don't want the handshake to start until that is sent.
        timer_->InitWithNamedFuncCallback(TimerCallback, this, 0,
                                          nsITimer::TYPE_ONE_SHOT,
                                          "TransportLayerDtls::TimerCallback");
        TL_SET_STATE(TS_CONNECTING);
      } else {
        // We have already completed DTLS. Can happen if the ICE layer failed
        // due to a loss of network, and then recovered.
        TL_SET_STATE(TS_OPEN);
      }
      break;

    case TS_CLOSED:
      MOZ_MTLOG(ML_INFO, LAYER_INFO << "Lower layer is now closed");
      TL_SET_STATE(TS_CLOSED);
      break;

    case TS_ERROR:
      MOZ_MTLOG(ML_ERROR, LAYER_INFO << "Lower layer experienced an error");
      TL_SET_STATE(TS_ERROR);
      break;
  }
}

void TransportLayerDtls::Handshake() {
  if (!timer_) {
    // We are done with DTLS, regardless of the state changes of lower layers
    return;
  }

  // Clear the retransmit timer
  timer_->Cancel();

  MOZ_ASSERT(state_ == TS_CONNECTING);

  SECStatus rv = SSL_ForceHandshake(ssl_fd_.get());

  if (rv == SECSuccess) {
    MOZ_MTLOG(ML_NOTICE, LAYER_INFO << "****** SSL handshake completed ******");
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

    RecordTlsTelemetry();
    timer_ = nullptr;
  } else {
    int32_t err = PR_GetError();
    switch (err) {
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
          timer_->InitWithNamedFuncCallback(
              TimerCallback, this, timeout_ms, nsITimer::TYPE_ONE_SHOT,
              "TransportLayerDtls::TimerCallback");
        }
        break;
      default:
        const char* err_msg = PR_ErrorToName(err);
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
      break;  // OK

    case SSL_NEXT_PROTO_NO_SUPPORT:
      MOZ_MTLOG(ML_NOTICE,
                LAYER_INFO << "ALPN not negotiated, "
                           << (alpn_default_.empty() ? "failing"
                                                     : "selecting default"));
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
                                        MediaPacket& packet) {
  CheckThread();
  MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "PacketReceived(" << packet.len() << ")");

  if (state_ != TS_CONNECTING && state_ != TS_OPEN) {
    MOZ_MTLOG(ML_DEBUG,
              LAYER_INFO << "Discarding packet in inappropriate state");
    return;
  }

  if (!packet.data()) {
    // Something ate this, probably the SRTP layer
    return;
  }

  if (packet.type() != MediaPacket::DTLS) {
    return;
  }

  nspr_io_adapter_->PacketReceived(packet);
  GetDecryptedPackets();
}

void TransportLayerDtls::GetDecryptedPackets() {
  // If we're still connecting, try to handshake
  if (state_ == TS_CONNECTING) {
    Handshake();
  }

  // Now try a recv if we're open, since there might be data left
  if (state_ == TS_OPEN) {
    int32_t rv;
    // One packet might contain several DTLS packets
    do {
      // nICEr uses a 9216 bytes buffer to allow support for jumbo frames
      // Can we peek to get a better idea of the actual size?
      static const size_t kBufferSize = 9216;
      auto buffer = MakeUnique<uint8_t[]>(kBufferSize);
      rv = PR_Recv(ssl_fd_.get(), buffer.get(), kBufferSize, 0,
                   PR_INTERVAL_NO_WAIT);
      if (rv > 0) {
        // We have data
        MOZ_MTLOG(ML_DEBUG, LAYER_INFO << "Read " << rv << " bytes from NSS");
        MediaPacket packet;
        packet.SetType(MediaPacket::SCTP);
        packet.Take(std::move(buffer), static_cast<size_t>(rv));
        SignalPacketReceived(this, packet);
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

void TransportLayerDtls::SetState(State state, const char* file,
                                  unsigned line) {
  if (timer_) {
    switch (state) {
      case TS_NONE:
      case TS_INIT:
        MOZ_ASSERT(false);
        break;
      case TS_CONNECTING:
        handshake_started_ = TimeStamp::Now();
        break;
      case TS_OPEN:
      case TS_CLOSED:
      case TS_ERROR:
        timer_->Cancel();
        if (state_ == TS_CONNECTING) {
          RecordHandshakeCompletionTelemetry(state);
        }
        break;
    }
  }

  TransportLayer::SetState(state, file, line);
}

TransportResult TransportLayerDtls::SendPacket(MediaPacket& packet) {
  CheckThread();
  if (state_ != TS_OPEN) {
    MOZ_MTLOG(ML_ERROR,
              LAYER_INFO << "Can't call SendPacket() in state " << state_);
    return TE_ERROR;
  }

  int32_t rv = PR_Send(ssl_fd_.get(), packet.data(), packet.len(), 0,
                       PR_INTERVAL_NO_WAIT);

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

SECStatus TransportLayerDtls::GetClientAuthDataHook(
    void* arg, PRFileDesc* fd, CERTDistNames* caNames,
    CERTCertificate** pRetCert, SECKEYPrivateKey** pRetKey) {
  MOZ_MTLOG(ML_DEBUG, "Server requested client auth");

  TransportLayerDtls* stream = reinterpret_cast<TransportLayerDtls*>(arg);
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

nsresult TransportLayerDtls::SetSrtpCiphers(
    const std::vector<uint16_t>& ciphers) {
  enabled_srtp_ciphers_ = std::move(ciphers);
  return NS_OK;
}

nsresult TransportLayerDtls::GetSrtpCipher(uint16_t* cipher) const {
  CheckThread();
  if (srtp_cipher_ == 0) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  *cipher = srtp_cipher_;
  return NS_OK;
}

static uint8_t* WriteUint16(uint8_t* cursor, uint16_t v) {
  *cursor++ = v >> 8;
  *cursor++ = v & 0xff;
  return cursor;
}

static SSLHandshakeType SrtpXtnServerMessage(PRFileDesc* fd) {
  SSLPreliminaryChannelInfo preinfo;
  SECStatus rv = SSL_GetPreliminaryChannelInfo(fd, &preinfo, sizeof(preinfo));
  if (rv != SECSuccess) {
    MOZ_ASSERT(false, "Can't get version info");
    return ssl_hs_client_hello;
  }
  return (preinfo.protocolVersion >= SSL_LIBRARY_VERSION_TLS_1_3)
             ? ssl_hs_encrypted_extensions
             : ssl_hs_server_hello;
}

/* static */
PRBool TransportLayerDtls::WriteSrtpXtn(PRFileDesc* fd,
                                        SSLHandshakeType message, uint8_t* data,
                                        unsigned int* len, unsigned int max_len,
                                        void* arg) {
  auto self = reinterpret_cast<TransportLayerDtls*>(arg);

  // ClientHello: send all supported versions.
  if (message == ssl_hs_client_hello) {
    MOZ_ASSERT(self->role_ == CLIENT);
    MOZ_ASSERT(self->enabled_srtp_ciphers_.size(), "Haven't enabled SRTP");
    // We will take 2 octets for each cipher, plus a 2 octet length and 1 octet
    // for the length of the empty MKI.
    if (max_len < self->enabled_srtp_ciphers_.size() * 2 + 3) {
      MOZ_ASSERT(false, "Not enough space to send SRTP extension");
      return false;
    }
    uint8_t* cursor = WriteUint16(data, self->enabled_srtp_ciphers_.size() * 2);
    for (auto cs : self->enabled_srtp_ciphers_) {
      cursor = WriteUint16(cursor, cs);
    }
    *cursor++ = 0;  // MKI is empty
    *len = cursor - data;
    return true;
  }

  if (message == SrtpXtnServerMessage(fd)) {
    MOZ_ASSERT(self->role_ == SERVER);
    if (!self->srtp_cipher_) {
      // Not negotiated. Definitely bad, but the connection can fail later.
      return false;
    }
    if (max_len < 5) {
      MOZ_ASSERT(false, "Not enough space to send SRTP extension");
      return false;
    }

    uint8_t* cursor = WriteUint16(data, 2);  // Length = 2.
    cursor = WriteUint16(cursor, self->srtp_cipher_);
    *cursor++ = 0;  // No MKI
    *len = cursor - data;
    return true;
  }

  return false;
}

class TlsParser {
 public:
  TlsParser(const uint8_t* data, size_t len) : cursor_(data), remaining_(len) {}

  bool error() const { return error_; }
  size_t remaining() const { return remaining_; }

  template <typename T,
            class = typename std::enable_if<std::is_unsigned<T>::value>::type>
  void Read(T* v, size_t sz = sizeof(T)) {
    MOZ_ASSERT(sz <= sizeof(T),
               "Type is too small to hold the value requested");
    if (remaining_ < sz) {
      error_ = true;
      return;
    }

    T result = 0;
    for (size_t i = 0; i < sz; ++i) {
      result = (result << 8) | *cursor_++;
      remaining_--;
    }
    *v = result;
  }

  template <typename T,
            class = typename std::enable_if<std::is_unsigned<T>::value>::type>
  void ReadVector(std::vector<T>* v, size_t w) {
    MOZ_ASSERT(v->empty(), "vector needs to be empty");

    uint32_t len;
    Read(&len, w);
    if (error_ || len % sizeof(T) != 0 || len > remaining_) {
      error_ = true;
      return;
    }

    size_t count = len / sizeof(T);
    v->reserve(count);
    for (T i = 0; !error_ && i < count; ++i) {
      T item;
      Read(&item);
      if (!error_) {
        v->push_back(item);
      }
    }
  }

  void Skip(size_t n) {
    if (remaining_ < n) {
      error_ = true;
    } else {
      cursor_ += n;
      remaining_ -= n;
    }
  }

  size_t SkipVector(size_t w) {
    uint32_t len = 0;
    Read(&len, w);
    Skip(len);
    return len;
  }

 private:
  const uint8_t* cursor_;
  size_t remaining_;
  bool error_ = false;
};

/* static */
SECStatus TransportLayerDtls::HandleSrtpXtn(
    PRFileDesc* fd, SSLHandshakeType message, const uint8_t* data,
    unsigned int len, SSLAlertDescription* alert, void* arg) {
  static const uint8_t kTlsAlertHandshakeFailure = 40;
  static const uint8_t kTlsAlertIllegalParameter = 47;
  static const uint8_t kTlsAlertDecodeError = 50;
  static const uint8_t kTlsAlertUnsupportedExtension = 110;

  auto self = reinterpret_cast<TransportLayerDtls*>(arg);

  // Parse the extension.
  TlsParser parser(data, len);
  std::vector<uint16_t> advertised;
  parser.ReadVector(&advertised, 2);
  size_t mki_len = parser.SkipVector(1);
  if (parser.error() || parser.remaining() > 0) {
    *alert = kTlsAlertDecodeError;
    return SECFailure;
  }

  if (message == ssl_hs_client_hello) {
    MOZ_ASSERT(self->role_ == SERVER);
    if (self->enabled_srtp_ciphers_.empty()) {
      // We don't have SRTP enabled, which is probably bad, but no sense in
      // having the handshake fail at this point, let the client decide if this
      // is a problem.
      return SECSuccess;
    }

    for (auto supported : self->enabled_srtp_ciphers_) {
      auto it = std::find(advertised.begin(), advertised.end(), supported);
      if (it != advertised.end()) {
        self->srtp_cipher_ = supported;
        return SECSuccess;
      }
    }

    // No common cipher.
    *alert = kTlsAlertHandshakeFailure;
    return SECFailure;
  }

  if (message == SrtpXtnServerMessage(fd)) {
    MOZ_ASSERT(self->role_ == CLIENT);
    if (advertised.size() != 1 || mki_len > 0) {
      *alert = kTlsAlertIllegalParameter;
      return SECFailure;
    }
    self->srtp_cipher_ = advertised[0];
    return SECSuccess;
  }

  *alert = kTlsAlertUnsupportedExtension;
  return SECFailure;
}

nsresult TransportLayerDtls::ExportKeyingMaterial(const std::string& label,
                                                  bool use_context,
                                                  const std::string& context,
                                                  unsigned char* out,
                                                  unsigned int outlen) {
  CheckThread();
  if (state_ != TS_OPEN) {
    MOZ_ASSERT(false, "Transport must be open for ExportKeyingMaterial");
    return NS_ERROR_NOT_AVAILABLE;
  }
  SECStatus rv = SSL_ExportKeyingMaterial(
      ssl_fd_.get(), label.c_str(), label.size(), use_context,
      reinterpret_cast<const unsigned char*>(context.c_str()), context.size(),
      out, outlen);
  if (rv != SECSuccess) {
    MOZ_MTLOG(ML_ERROR, "Couldn't export SSL keying material");
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

SECStatus TransportLayerDtls::AuthCertificateHook(void* arg, PRFileDesc* fd,
                                                  PRBool checksig,
                                                  PRBool isServer) {
  TransportLayerDtls* stream = reinterpret_cast<TransportLayerDtls*>(arg);
  stream->CheckThread();
  return stream->AuthCertificateHook(fd, checksig, isServer);
}

SECStatus TransportLayerDtls::CheckDigest(
    const DtlsDigest& digest, UniqueCERTCertificate& peer_cert) const {
  DtlsDigest computed_digest(digest.algorithm_);

  MOZ_MTLOG(ML_DEBUG,
            LAYER_INFO << "Checking digest, algorithm=" << digest.algorithm_);
  nsresult res = DtlsIdentity::ComputeFingerprint(peer_cert, &computed_digest);
  if (NS_FAILED(res)) {
    MOZ_MTLOG(ML_ERROR, "Could not compute peer fingerprint for digest "
                            << digest.algorithm_);
    // Go to end
    PR_SetError(SSL_ERROR_BAD_CERTIFICATE, 0);
    return SECFailure;
  }

  if (computed_digest != digest) {
    MOZ_MTLOG(ML_ERROR, "Digest does not match");
    PR_SetError(SSL_ERROR_BAD_CERTIFICATE, 0);
    return SECFailure;
  }

  return SECSuccess;
}

SECStatus TransportLayerDtls::AuthCertificateHook(PRFileDesc* fd,
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

    case VERIFY_DIGEST: {
      MOZ_ASSERT(digests_.size() != 0);
      // Check all the provided digests

      // Checking functions call PR_SetError()
      SECStatus rv = SECFailure;
      for (auto digest : digests_) {
        rv = CheckDigest(digest, peer_cert);

        // Matches a digest, we are good to go
        if (rv == SECSuccess) {
          cert_ok_ = true;
          return SECSuccess;
        }
      }
    } break;
    default:
      MOZ_CRASH();  // Can't happen
  }

  return SECFailure;
}

void TransportLayerDtls::TimerCallback(nsITimer* timer, void* arg) {
  TransportLayerDtls* dtls = reinterpret_cast<TransportLayerDtls*>(arg);

  MOZ_MTLOG(ML_DEBUG, "DTLS timer expired");

  dtls->Handshake();
}

void TransportLayerDtls::RecordHandshakeCompletionTelemetry(
    TransportLayer::State endState) {
  int32_t delta = (TimeStamp::Now() - handshake_started_).ToMilliseconds();

  switch (endState) {
    case TransportLayer::State::TS_OPEN:
      if (role_ == TransportLayerDtls::CLIENT) {
        Telemetry::Accumulate(Telemetry::WEBRTC_DTLS_CLIENT_SUCCESS_TIME,
                              delta);
      } else {
        Telemetry::Accumulate(Telemetry::WEBRTC_DTLS_SERVER_SUCCESS_TIME,
                              delta);
      }
      return;
    case TransportLayer::State::TS_ERROR:
      if (role_ == TransportLayerDtls::CLIENT) {
        Telemetry::Accumulate(Telemetry::WEBRTC_DTLS_CLIENT_FAILURE_TIME,
                              delta);
      } else {
        Telemetry::Accumulate(Telemetry::WEBRTC_DTLS_SERVER_FAILURE_TIME,
                              delta);
      }
      return;
    case TransportLayer::State::TS_CLOSED:
      if (role_ == TransportLayerDtls::CLIENT) {
        Telemetry::Accumulate(Telemetry::WEBRTC_DTLS_CLIENT_ABORT_TIME, delta);
      } else {
        Telemetry::Accumulate(Telemetry::WEBRTC_DTLS_SERVER_ABORT_TIME, delta);
      }
      return;
    default:
      MOZ_ASSERT(false);
  }
}

void TransportLayerDtls::RecordTlsTelemetry() {
  MOZ_ASSERT(state_ == TS_OPEN);
  SSLChannelInfo info;
  SECStatus ss = SSL_GetChannelInfo(ssl_fd_.get(), &info, sizeof(info));
  if (ss != SECSuccess) {
    MOZ_MTLOG(ML_NOTICE,
              LAYER_INFO << "RecordTlsTelemetry failed to get channel info");
    return;
  }

  auto protocol_label =
      mozilla::Telemetry::LABELS_WEBRTC_DTLS_PROTOCOL_VERSION::Unknown;

  switch (info.protocolVersion) {
    case SSL_LIBRARY_VERSION_TLS_1_1:
      protocol_label =
          Telemetry::LABELS_WEBRTC_DTLS_PROTOCOL_VERSION::Dtls_version_1_0;
      break;
    case SSL_LIBRARY_VERSION_TLS_1_2:
      protocol_label =
          Telemetry::LABELS_WEBRTC_DTLS_PROTOCOL_VERSION::Dtls_version_1_2;
      break;
    case SSL_LIBRARY_VERSION_TLS_1_3:
      protocol_label =
          Telemetry::LABELS_WEBRTC_DTLS_PROTOCOL_VERSION::Dtls_version_1_3;
      break;
  }

  Telemetry::AccumulateCategorical(protocol_label);

  uint16_t telemetry_cipher = 0;

  switch (info.cipherSuite) {
    /* Old DHE ciphers: candidates for removal, see bug 1227519 */
    case TLS_DHE_RSA_WITH_AES_128_CBC_SHA:
      telemetry_cipher = 1;
      break;
    case TLS_DHE_RSA_WITH_AES_256_CBC_SHA:
      telemetry_cipher = 2;
      break;
    /* Current ciphers */
    case TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA:
      telemetry_cipher = 3;
      break;
    case TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA:
      telemetry_cipher = 4;
      break;
    case TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA:
      telemetry_cipher = 5;
      break;
    case TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA:
      telemetry_cipher = 6;
      break;
    case TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256:
      telemetry_cipher = 7;
      break;
    case TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256:
      telemetry_cipher = 8;
      break;
    case TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256:
      telemetry_cipher = 9;
      break;
    case TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256:
      telemetry_cipher = 10;
      break;
    /* TLS 1.3 ciphers */
    case TLS_AES_128_GCM_SHA256:
      telemetry_cipher = 11;
      break;
    case TLS_CHACHA20_POLY1305_SHA256:
      telemetry_cipher = 12;
      break;
    case TLS_AES_256_GCM_SHA384:
      telemetry_cipher = 13;
      break;
  }

  Telemetry::Accumulate(Telemetry::WEBRTC_DTLS_CIPHER, telemetry_cipher);

  uint16_t cipher;
  nsresult rv = GetSrtpCipher(&cipher);

  if (NS_FAILED(rv)) {
    MOZ_MTLOG(ML_DEBUG, "No SRTP cipher suite");
    return;
  }

  auto cipher_label = mozilla::Telemetry::LABELS_WEBRTC_SRTP_CIPHER::Unknown;

  switch (cipher) {
    case kDtlsSrtpAes128CmHmacSha1_80:
      cipher_label = Telemetry::LABELS_WEBRTC_SRTP_CIPHER::Aes128CmHmacSha1_80;
      break;
    case kDtlsSrtpAes128CmHmacSha1_32:
      cipher_label = Telemetry::LABELS_WEBRTC_SRTP_CIPHER::Aes128CmHmacSha1_32;
      break;
    case kDtlsSrtpAeadAes128Gcm:
      cipher_label = Telemetry::LABELS_WEBRTC_SRTP_CIPHER::AeadAes128Gcm;
      break;
    case kDtlsSrtpAeadAes256Gcm:
      cipher_label = Telemetry::LABELS_WEBRTC_SRTP_CIPHER::AeadAes256Gcm;
      break;
  }

  Telemetry::AccumulateCategorical(cipher_label);
}

}  // namespace mozilla
