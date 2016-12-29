/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <utility>

#include "webrtc/p2p/base/dtlstransportchannel.h"

#include "webrtc/p2p/base/common.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/dscp.h"
#include "webrtc/base/messagequeue.h"
#include "webrtc/base/sslstreamadapter.h"
#include "webrtc/base/stream.h"
#include "webrtc/base/thread.h"

namespace cricket {

// We don't pull the RTP constants from rtputils.h, to avoid a layer violation.
static const size_t kDtlsRecordHeaderLen = 13;
static const size_t kMaxDtlsPacketLen = 2048;
static const size_t kMinRtpPacketLen = 12;

// Maximum number of pending packets in the queue. Packets are read immediately
// after they have been written, so a capacity of "1" is sufficient.
static const size_t kMaxPendingPackets = 1;

static bool IsDtlsPacket(const char* data, size_t len) {
  const uint8_t* u = reinterpret_cast<const uint8_t*>(data);
  return (len >= kDtlsRecordHeaderLen && (u[0] > 19 && u[0] < 64));
}
static bool IsRtpPacket(const char* data, size_t len) {
  const uint8_t* u = reinterpret_cast<const uint8_t*>(data);
  return (len >= kMinRtpPacketLen && (u[0] & 0xC0) == 0x80);
}

StreamInterfaceChannel::StreamInterfaceChannel(TransportChannel* channel)
    : channel_(channel),
      state_(rtc::SS_OPEN),
      packets_(kMaxPendingPackets, kMaxDtlsPacketLen) {
}

rtc::StreamResult StreamInterfaceChannel::Read(void* buffer,
                                                     size_t buffer_len,
                                                     size_t* read,
                                                     int* error) {
  if (state_ == rtc::SS_CLOSED)
    return rtc::SR_EOS;
  if (state_ == rtc::SS_OPENING)
    return rtc::SR_BLOCK;

  if (!packets_.ReadFront(buffer, buffer_len, read)) {
    return rtc::SR_BLOCK;
  }

  return rtc::SR_SUCCESS;
}

rtc::StreamResult StreamInterfaceChannel::Write(const void* data,
                                                      size_t data_len,
                                                      size_t* written,
                                                      int* error) {
  // Always succeeds, since this is an unreliable transport anyway.
  // TODO: Should this block if channel_'s temporarily unwritable?
  rtc::PacketOptions packet_options;
  channel_->SendPacket(static_cast<const char*>(data), data_len,
                       packet_options);
  if (written) {
    *written = data_len;
  }
  return rtc::SR_SUCCESS;
}

bool StreamInterfaceChannel::OnPacketReceived(const char* data, size_t size) {
  // We force a read event here to ensure that we don't overflow our queue.
  bool ret = packets_.WriteBack(data, size, NULL);
  RTC_CHECK(ret) << "Failed to write packet to queue.";
  if (ret) {
    SignalEvent(this, rtc::SE_READ, 0);
  }
  return ret;
}

DtlsTransportChannelWrapper::DtlsTransportChannelWrapper(
    Transport* transport,
    TransportChannelImpl* channel)
    : TransportChannelImpl(channel->transport_name(), channel->component()),
      transport_(transport),
      worker_thread_(rtc::Thread::Current()),
      channel_(channel),
      downward_(NULL),
      ssl_role_(rtc::SSL_CLIENT),
      ssl_max_version_(rtc::SSL_PROTOCOL_DTLS_12) {
  channel_->SignalWritableState.connect(this,
      &DtlsTransportChannelWrapper::OnWritableState);
  channel_->SignalReadPacket.connect(this,
      &DtlsTransportChannelWrapper::OnReadPacket);
  channel_->SignalSentPacket.connect(
      this, &DtlsTransportChannelWrapper::OnSentPacket);
  channel_->SignalReadyToSend.connect(this,
      &DtlsTransportChannelWrapper::OnReadyToSend);
  channel_->SignalGatheringState.connect(
      this, &DtlsTransportChannelWrapper::OnGatheringState);
  channel_->SignalCandidateGathered.connect(
      this, &DtlsTransportChannelWrapper::OnCandidateGathered);
  channel_->SignalRoleConflict.connect(this,
      &DtlsTransportChannelWrapper::OnRoleConflict);
  channel_->SignalRouteChange.connect(this,
      &DtlsTransportChannelWrapper::OnRouteChange);
  channel_->SignalConnectionRemoved.connect(this,
      &DtlsTransportChannelWrapper::OnConnectionRemoved);
  channel_->SignalReceivingState.connect(this,
      &DtlsTransportChannelWrapper::OnReceivingState);
}

DtlsTransportChannelWrapper::~DtlsTransportChannelWrapper() {
}

void DtlsTransportChannelWrapper::Connect() {
  // We should only get a single call to Connect.
  ASSERT(dtls_state() == DTLS_TRANSPORT_NEW);
  channel_->Connect();
}

bool DtlsTransportChannelWrapper::SetLocalCertificate(
    const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
  if (dtls_active_) {
    if (certificate == local_certificate_) {
      // This may happen during renegotiation.
      LOG_J(LS_INFO, this) << "Ignoring identical DTLS identity";
      return true;
    } else {
      LOG_J(LS_ERROR, this) << "Can't change DTLS local identity in this state";
      return false;
    }
  }

  if (certificate) {
    local_certificate_ = certificate;
    dtls_active_ = true;
  } else {
    LOG_J(LS_INFO, this) << "NULL DTLS identity supplied. Not doing DTLS";
  }

  return true;
}

rtc::scoped_refptr<rtc::RTCCertificate>
DtlsTransportChannelWrapper::GetLocalCertificate() const {
  return local_certificate_;
}

bool DtlsTransportChannelWrapper::SetSslMaxProtocolVersion(
    rtc::SSLProtocolVersion version) {
  if (dtls_active_) {
    LOG(LS_ERROR) << "Not changing max. protocol version "
                  << "while DTLS is negotiating";
    return false;
  }

  ssl_max_version_ = version;
  return true;
}

bool DtlsTransportChannelWrapper::SetSslRole(rtc::SSLRole role) {
  if (dtls_state() == DTLS_TRANSPORT_CONNECTED) {
    if (ssl_role_ != role) {
      LOG(LS_ERROR) << "SSL Role can't be reversed after the session is setup.";
      return false;
    }
    return true;
  }

  ssl_role_ = role;
  return true;
}

bool DtlsTransportChannelWrapper::GetSslRole(rtc::SSLRole* role) const {
  *role = ssl_role_;
  return true;
}

bool DtlsTransportChannelWrapper::GetSslCipherSuite(int* cipher) {
  if (dtls_state() != DTLS_TRANSPORT_CONNECTED) {
    return false;
  }

  return dtls_->GetSslCipherSuite(cipher);
}

bool DtlsTransportChannelWrapper::SetRemoteFingerprint(
    const std::string& digest_alg,
    const uint8_t* digest,
    size_t digest_len) {
  rtc::Buffer remote_fingerprint_value(digest, digest_len);

  // Once we have the local certificate, the same remote fingerprint can be set
  // multiple times.
  if (dtls_active_ && remote_fingerprint_value_ == remote_fingerprint_value &&
      !digest_alg.empty()) {
    // This may happen during renegotiation.
    LOG_J(LS_INFO, this) << "Ignoring identical remote DTLS fingerprint";
    return true;
  }

  // If the other side doesn't support DTLS, turn off |dtls_active_|.
  if (digest_alg.empty()) {
    RTC_DCHECK(!digest_len);
    LOG_J(LS_INFO, this) << "Other side didn't support DTLS.";
    dtls_active_ = false;
    return true;
  }

  // Otherwise, we must have a local certificate before setting remote
  // fingerprint.
  if (!dtls_active_) {
    LOG_J(LS_ERROR, this) << "Can't set DTLS remote settings in this state.";
    return false;
  }

  // At this point we know we are doing DTLS
  remote_fingerprint_value_ = std::move(remote_fingerprint_value);
  remote_fingerprint_algorithm_ = digest_alg;

  bool reconnect = dtls_;

  if (!SetupDtls()) {
    set_dtls_state(DTLS_TRANSPORT_FAILED);
    return false;
  }

  if (reconnect) {
    Reconnect();
  }

  return true;
}

bool DtlsTransportChannelWrapper::GetRemoteSSLCertificate(
    rtc::SSLCertificate** cert) const {
  if (!dtls_) {
    return false;
  }

  return dtls_->GetPeerCertificate(cert);
}

bool DtlsTransportChannelWrapper::SetupDtls() {
  StreamInterfaceChannel* downward = new StreamInterfaceChannel(channel_);

  dtls_.reset(rtc::SSLStreamAdapter::Create(downward));
  if (!dtls_) {
    LOG_J(LS_ERROR, this) << "Failed to create DTLS adapter.";
    delete downward;
    return false;
  }

  downward_ = downward;

  dtls_->SetIdentity(local_certificate_->identity()->GetReference());
  dtls_->SetMode(rtc::SSL_MODE_DTLS);
  dtls_->SetMaxProtocolVersion(ssl_max_version_);
  dtls_->SetServerRole(ssl_role_);
  dtls_->SignalEvent.connect(this, &DtlsTransportChannelWrapper::OnDtlsEvent);
  if (!dtls_->SetPeerCertificateDigest(
          remote_fingerprint_algorithm_,
          reinterpret_cast<unsigned char*>(remote_fingerprint_value_.data()),
          remote_fingerprint_value_.size())) {
    LOG_J(LS_ERROR, this) << "Couldn't set DTLS certificate digest.";
    return false;
  }

  // Set up DTLS-SRTP, if it's been enabled.
  if (!srtp_ciphers_.empty()) {
    if (!dtls_->SetDtlsSrtpCryptoSuites(srtp_ciphers_)) {
      LOG_J(LS_ERROR, this) << "Couldn't set DTLS-SRTP ciphers.";
      return false;
    }
  } else {
    LOG_J(LS_INFO, this) << "Not using DTLS-SRTP.";
  }

  LOG_J(LS_INFO, this) << "DTLS setup complete.";
  return true;
}

bool DtlsTransportChannelWrapper::SetSrtpCryptoSuites(
    const std::vector<int>& ciphers) {
  if (srtp_ciphers_ == ciphers)
    return true;

  if (dtls_state() == DTLS_TRANSPORT_CONNECTING) {
    LOG(LS_WARNING) << "Ignoring new SRTP ciphers while DTLS is negotiating";
    return true;
  }

  if (dtls_state() == DTLS_TRANSPORT_CONNECTED) {
    // We don't support DTLS renegotiation currently. If new set of srtp ciphers
    // are different than what's being used currently, we will not use it.
    // So for now, let's be happy (or sad) with a warning message.
    int current_srtp_cipher;
    if (!dtls_->GetDtlsSrtpCryptoSuite(&current_srtp_cipher)) {
      LOG(LS_ERROR) << "Failed to get the current SRTP cipher for DTLS channel";
      return false;
    }
    const std::vector<int>::const_iterator iter =
        std::find(ciphers.begin(), ciphers.end(), current_srtp_cipher);
    if (iter == ciphers.end()) {
      std::string requested_str;
      for (size_t i = 0; i < ciphers.size(); ++i) {
        requested_str.append(" ");
        requested_str.append(rtc::SrtpCryptoSuiteToName(ciphers[i]));
        requested_str.append(" ");
      }
      LOG(LS_WARNING) << "Ignoring new set of SRTP ciphers, as DTLS "
                      << "renegotiation is not supported currently "
                      << "current cipher = " << current_srtp_cipher << " and "
                      << "requested = " << "[" << requested_str << "]";
    }
    return true;
  }

  if (!VERIFY(dtls_state() == DTLS_TRANSPORT_NEW)) {
    return false;
  }

  srtp_ciphers_ = ciphers;
  return true;
}

bool DtlsTransportChannelWrapper::GetSrtpCryptoSuite(int* cipher) {
  if (dtls_state() != DTLS_TRANSPORT_CONNECTED) {
    return false;
  }

  return dtls_->GetDtlsSrtpCryptoSuite(cipher);
}


// Called from upper layers to send a media packet.
int DtlsTransportChannelWrapper::SendPacket(
    const char* data, size_t size,
    const rtc::PacketOptions& options, int flags) {
  if (!dtls_active_) {
    // Not doing DTLS.
    return channel_->SendPacket(data, size, options);
  }

  switch (dtls_state()) {
    case DTLS_TRANSPORT_NEW:
      // Can't send data until the connection is active.
      // TODO(ekr@rtfm.com): assert here if dtls_ is NULL?
      return -1;
    case DTLS_TRANSPORT_CONNECTING:
      // Can't send data until the connection is active.
      return -1;
    case DTLS_TRANSPORT_CONNECTED:
      if (flags & PF_SRTP_BYPASS) {
        ASSERT(!srtp_ciphers_.empty());
        if (!IsRtpPacket(data, size)) {
          return -1;
        }

        return channel_->SendPacket(data, size, options);
      } else {
        return (dtls_->WriteAll(data, size, NULL, NULL) == rtc::SR_SUCCESS)
                   ? static_cast<int>(size)
                   : -1;
      }
    case DTLS_TRANSPORT_FAILED:
    case DTLS_TRANSPORT_CLOSED:
      // Can't send anything when we're closed.
      return -1;
    default:
      ASSERT(false);
      return -1;
  }
}

// The state transition logic here is as follows:
// (1) If we're not doing DTLS-SRTP, then the state is just the
//     state of the underlying impl()
// (2) If we're doing DTLS-SRTP:
//     - Prior to the DTLS handshake, the state is neither receiving nor
//       writable
//     - When the impl goes writable for the first time we
//       start the DTLS handshake
//     - Once the DTLS handshake completes, the state is that of the
//       impl again
void DtlsTransportChannelWrapper::OnWritableState(TransportChannel* channel) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  ASSERT(channel == channel_);
  LOG_J(LS_VERBOSE, this)
      << "DTLSTransportChannelWrapper: channel writable state changed to "
      << channel_->writable();

  if (!dtls_active_) {
    // Not doing DTLS.
    // Note: SignalWritableState fired by set_writable.
    set_writable(channel_->writable());
    return;
  }

  switch (dtls_state()) {
    case DTLS_TRANSPORT_NEW:
      // This should never fail:
      // Because we are operating in a nonblocking mode and all
      // incoming packets come in via OnReadPacket(), which rejects
      // packets in this state, the incoming queue must be empty. We
      // ignore write errors, thus any errors must be because of
      // configuration and therefore are our fault.
      // Note that in non-debug configurations, failure in
      // MaybeStartDtls() changes the state to DTLS_TRANSPORT_FAILED.
      VERIFY(MaybeStartDtls());
      break;
    case DTLS_TRANSPORT_CONNECTED:
      // Note: SignalWritableState fired by set_writable.
      set_writable(channel_->writable());
      break;
    case DTLS_TRANSPORT_CONNECTING:
      // Do nothing.
      break;
    case DTLS_TRANSPORT_FAILED:
    case DTLS_TRANSPORT_CLOSED:
      // Should not happen. Do nothing.
      break;
  }
}

void DtlsTransportChannelWrapper::OnReceivingState(TransportChannel* channel) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  ASSERT(channel == channel_);
  LOG_J(LS_VERBOSE, this)
      << "DTLSTransportChannelWrapper: channel receiving state changed to "
      << channel_->receiving();
  if (!dtls_active_ || dtls_state() == DTLS_TRANSPORT_CONNECTED) {
    // Note: SignalReceivingState fired by set_receiving.
    set_receiving(channel_->receiving());
  }
}

void DtlsTransportChannelWrapper::OnReadPacket(
    TransportChannel* channel, const char* data, size_t size,
    const rtc::PacketTime& packet_time, int flags) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  ASSERT(channel == channel_);
  ASSERT(flags == 0);

  if (!dtls_active_) {
    // Not doing DTLS.
    SignalReadPacket(this, data, size, packet_time, 0);
    return;
  }

  switch (dtls_state()) {
    case DTLS_TRANSPORT_NEW:
      if (dtls_) {
        // Drop packets received before DTLS has actually started.
        LOG_J(LS_INFO, this) << "Dropping packet received before DTLS started.";
      } else {
        // Currently drop the packet, but we might in future
        // decide to take this as evidence that the other
        // side is ready to do DTLS and start the handshake
        // on our end.
        LOG_J(LS_WARNING, this) << "Received packet before we know if we are "
                                << "doing DTLS or not; dropping.";
      }
      break;

    case DTLS_TRANSPORT_CONNECTING:
    case DTLS_TRANSPORT_CONNECTED:
      // We should only get DTLS or SRTP packets; STUN's already been demuxed.
      // Is this potentially a DTLS packet?
      if (IsDtlsPacket(data, size)) {
        if (!HandleDtlsPacket(data, size)) {
          LOG_J(LS_ERROR, this) << "Failed to handle DTLS packet.";
          return;
        }
      } else {
        // Not a DTLS packet; our handshake should be complete by now.
        if (dtls_state() != DTLS_TRANSPORT_CONNECTED) {
          LOG_J(LS_ERROR, this) << "Received non-DTLS packet before DTLS "
                                << "complete.";
          return;
        }

        // And it had better be a SRTP packet.
        if (!IsRtpPacket(data, size)) {
          LOG_J(LS_ERROR, this) << "Received unexpected non-DTLS packet.";
          return;
        }

        // Sanity check.
        ASSERT(!srtp_ciphers_.empty());

        // Signal this upwards as a bypass packet.
        SignalReadPacket(this, data, size, packet_time, PF_SRTP_BYPASS);
      }
      break;
    case DTLS_TRANSPORT_FAILED:
    case DTLS_TRANSPORT_CLOSED:
      // This shouldn't be happening. Drop the packet.
      break;
  }
}

void DtlsTransportChannelWrapper::OnSentPacket(
    TransportChannel* channel,
    const rtc::SentPacket& sent_packet) {
  ASSERT(rtc::Thread::Current() == worker_thread_);

  SignalSentPacket(this, sent_packet);
}

void DtlsTransportChannelWrapper::OnReadyToSend(TransportChannel* channel) {
  if (writable()) {
    SignalReadyToSend(this);
  }
}

void DtlsTransportChannelWrapper::OnDtlsEvent(rtc::StreamInterface* dtls,
                                              int sig, int err) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  ASSERT(dtls == dtls_.get());
  if (sig & rtc::SE_OPEN) {
    // This is the first time.
    LOG_J(LS_INFO, this) << "DTLS handshake complete.";
    if (dtls_->GetState() == rtc::SS_OPEN) {
      // The check for OPEN shouldn't be necessary but let's make
      // sure we don't accidentally frob the state if it's closed.
      set_dtls_state(DTLS_TRANSPORT_CONNECTED);
      set_writable(true);
    }
  }
  if (sig & rtc::SE_READ) {
    char buf[kMaxDtlsPacketLen];
    size_t read;
    if (dtls_->Read(buf, sizeof(buf), &read, NULL) == rtc::SR_SUCCESS) {
      SignalReadPacket(this, buf, read, rtc::CreatePacketTime(0), 0);
    }
  }
  if (sig & rtc::SE_CLOSE) {
    ASSERT(sig == rtc::SE_CLOSE);  // SE_CLOSE should be by itself.
    set_writable(false);
    if (!err) {
      LOG_J(LS_INFO, this) << "DTLS channel closed";
      set_dtls_state(DTLS_TRANSPORT_CLOSED);
    } else {
      LOG_J(LS_INFO, this) << "DTLS channel error, code=" << err;
      set_dtls_state(DTLS_TRANSPORT_FAILED);
    }
  }
}

bool DtlsTransportChannelWrapper::MaybeStartDtls() {
  if (dtls_ && channel_->writable()) {
    if (dtls_->StartSSLWithPeer()) {
      LOG_J(LS_ERROR, this) << "Couldn't start DTLS handshake";
      set_dtls_state(DTLS_TRANSPORT_FAILED);
      return false;
    }
    LOG_J(LS_INFO, this)
      << "DtlsTransportChannelWrapper: Started DTLS handshake";
    set_dtls_state(DTLS_TRANSPORT_CONNECTING);
  }
  return true;
}

// Called from OnReadPacket when a DTLS packet is received.
bool DtlsTransportChannelWrapper::HandleDtlsPacket(const char* data,
                                                   size_t size) {
  // Sanity check we're not passing junk that
  // just looks like DTLS.
  const uint8_t* tmp_data = reinterpret_cast<const uint8_t*>(data);
  size_t tmp_size = size;
  while (tmp_size > 0) {
    if (tmp_size < kDtlsRecordHeaderLen)
      return false;  // Too short for the header

    size_t record_len = (tmp_data[11] << 8) | (tmp_data[12]);
    if ((record_len + kDtlsRecordHeaderLen) > tmp_size)
      return false;  // Body too short

    tmp_data += record_len + kDtlsRecordHeaderLen;
    tmp_size -= record_len + kDtlsRecordHeaderLen;
  }

  // Looks good. Pass to the SIC which ends up being passed to
  // the DTLS stack.
  return downward_->OnPacketReceived(data, size);
}

void DtlsTransportChannelWrapper::OnGatheringState(
    TransportChannelImpl* channel) {
  ASSERT(channel == channel_);
  SignalGatheringState(this);
}

void DtlsTransportChannelWrapper::OnCandidateGathered(
    TransportChannelImpl* channel,
    const Candidate& c) {
  ASSERT(channel == channel_);
  SignalCandidateGathered(this, c);
}

void DtlsTransportChannelWrapper::OnRoleConflict(
    TransportChannelImpl* channel) {
  ASSERT(channel == channel_);
  SignalRoleConflict(this);
}

void DtlsTransportChannelWrapper::OnRouteChange(
    TransportChannel* channel, const Candidate& candidate) {
  ASSERT(channel == channel_);
  SignalRouteChange(this, candidate);
}

void DtlsTransportChannelWrapper::OnConnectionRemoved(
    TransportChannelImpl* channel) {
  ASSERT(channel == channel_);
  SignalConnectionRemoved(this);
}

void DtlsTransportChannelWrapper::Reconnect() {
  set_dtls_state(DTLS_TRANSPORT_NEW);
  set_writable(false);
  if (channel_->writable()) {
    OnWritableState(channel_);
  }
}

}  // namespace cricket
