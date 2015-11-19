/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/base/dtlstransportchannel.h"

#include "webrtc/p2p/base/common.h"
#include "webrtc/base/buffer.h"
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

static bool IsDtlsPacket(const char* data, size_t len) {
  const uint8* u = reinterpret_cast<const uint8*>(data);
  return (len >= kDtlsRecordHeaderLen && (u[0] > 19 && u[0] < 64));
}
static bool IsRtpPacket(const char* data, size_t len) {
  const uint8* u = reinterpret_cast<const uint8*>(data);
  return (len >= kMinRtpPacketLen && (u[0] & 0xC0) == 0x80);
}

rtc::StreamResult StreamInterfaceChannel::Read(void* buffer,
                                                     size_t buffer_len,
                                                     size_t* read,
                                                     int* error) {
  if (state_ == rtc::SS_CLOSED)
    return rtc::SR_EOS;
  if (state_ == rtc::SS_OPENING)
    return rtc::SR_BLOCK;

  return fifo_.Read(buffer, buffer_len, read, error);
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
  // We force a read event here to ensure that we don't overflow our FIFO.
  // Under high packet rate this can occur if we wait for the FIFO to post its
  // own SE_READ.
  bool ret = (fifo_.WriteAll(data, size, NULL, NULL) == rtc::SR_SUCCESS);
  if (ret) {
    SignalEvent(this, rtc::SE_READ, 0);
  }
  return ret;
}

void StreamInterfaceChannel::OnEvent(rtc::StreamInterface* stream,
                                     int sig, int err) {
  SignalEvent(this, sig, err);
}

DtlsTransportChannelWrapper::DtlsTransportChannelWrapper(
                                           Transport* transport,
                                           TransportChannelImpl* channel)
    : TransportChannelImpl(channel->content_name(), channel->component()),
      transport_(transport),
      worker_thread_(rtc::Thread::Current()),
      channel_(channel),
      downward_(NULL),
      dtls_state_(STATE_NONE),
      local_identity_(NULL),
      ssl_role_(rtc::SSL_CLIENT) {
  channel_->SignalReadableState.connect(this,
      &DtlsTransportChannelWrapper::OnReadableState);
  channel_->SignalWritableState.connect(this,
      &DtlsTransportChannelWrapper::OnWritableState);
  channel_->SignalReadPacket.connect(this,
      &DtlsTransportChannelWrapper::OnReadPacket);
  channel_->SignalReadyToSend.connect(this,
      &DtlsTransportChannelWrapper::OnReadyToSend);
  channel_->SignalRequestSignaling.connect(this,
      &DtlsTransportChannelWrapper::OnRequestSignaling);
  channel_->SignalCandidateReady.connect(this,
      &DtlsTransportChannelWrapper::OnCandidateReady);
  channel_->SignalCandidatesAllocationDone.connect(this,
      &DtlsTransportChannelWrapper::OnCandidatesAllocationDone);
  channel_->SignalRoleConflict.connect(this,
      &DtlsTransportChannelWrapper::OnRoleConflict);
  channel_->SignalRouteChange.connect(this,
      &DtlsTransportChannelWrapper::OnRouteChange);
  channel_->SignalConnectionRemoved.connect(this,
      &DtlsTransportChannelWrapper::OnConnectionRemoved);
}

DtlsTransportChannelWrapper::~DtlsTransportChannelWrapper() {
}

void DtlsTransportChannelWrapper::Connect() {
  // We should only get a single call to Connect.
  ASSERT(dtls_state_ == STATE_NONE ||
         dtls_state_ == STATE_OFFERED ||
         dtls_state_ == STATE_ACCEPTED);
  channel_->Connect();
}

void DtlsTransportChannelWrapper::Reset() {
  channel_->Reset();
  set_writable(false);
  set_readable(false);

  // Re-call SetupDtls()
  if (!SetupDtls()) {
    LOG_J(LS_ERROR, this) << "Error re-initializing DTLS";
    dtls_state_ = STATE_CLOSED;
    return;
  }

  dtls_state_ = STATE_ACCEPTED;
}

bool DtlsTransportChannelWrapper::SetLocalIdentity(
    rtc::SSLIdentity* identity) {
  if (dtls_state_ != STATE_NONE) {
    if (identity == local_identity_) {
      // This may happen during renegotiation.
      LOG_J(LS_INFO, this) << "Ignoring identical DTLS identity";
      return true;
    } else {
      LOG_J(LS_ERROR, this) << "Can't change DTLS local identity in this state";
      return false;
    }
  }

  if (identity) {
    local_identity_ = identity;
    dtls_state_ = STATE_OFFERED;
  } else {
    LOG_J(LS_INFO, this) << "NULL DTLS identity supplied. Not doing DTLS";
  }

  return true;
}

bool DtlsTransportChannelWrapper::GetLocalIdentity(
    rtc::SSLIdentity** identity) const {
  if (!local_identity_)
    return false;

  *identity = local_identity_->GetReference();
  return true;
}

bool DtlsTransportChannelWrapper::SetSslRole(rtc::SSLRole role) {
  if (dtls_state_ == STATE_OPEN) {
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

bool DtlsTransportChannelWrapper::GetSslCipher(std::string* cipher) {
  if (dtls_state_ != STATE_OPEN) {
    return false;
  }

  return dtls_->GetSslCipher(cipher);
}

bool DtlsTransportChannelWrapper::SetRemoteFingerprint(
    const std::string& digest_alg,
    const uint8* digest,
    size_t digest_len) {

  rtc::Buffer remote_fingerprint_value(digest, digest_len);

  if (dtls_state_ != STATE_NONE &&
      remote_fingerprint_value_ == remote_fingerprint_value &&
      !digest_alg.empty()) {
    // This may happen during renegotiation.
    LOG_J(LS_INFO, this) << "Ignoring identical remote DTLS fingerprint";
    return true;
  }

  // Allow SetRemoteFingerprint with a NULL digest even if SetLocalIdentity
  // hasn't been called.
  if (dtls_state_ > STATE_OFFERED ||
      (dtls_state_ == STATE_NONE && !digest_alg.empty())) {
    LOG_J(LS_ERROR, this) << "Can't set DTLS remote settings in this state.";
    return false;
  }

  if (digest_alg.empty()) {
    LOG_J(LS_INFO, this) << "Other side didn't support DTLS.";
    dtls_state_ = STATE_NONE;
    return true;
  }

  // At this point we know we are doing DTLS
  remote_fingerprint_value.TransferTo(&remote_fingerprint_value_);
  remote_fingerprint_algorithm_ = digest_alg;

  if (!SetupDtls()) {
    dtls_state_ = STATE_CLOSED;
    return false;
  }

  dtls_state_ = STATE_ACCEPTED;
  return true;
}

bool DtlsTransportChannelWrapper::GetRemoteCertificate(
    rtc::SSLCertificate** cert) const {
  if (!dtls_)
    return false;

  return dtls_->GetPeerCertificate(cert);
}

bool DtlsTransportChannelWrapper::SetupDtls() {
  StreamInterfaceChannel* downward =
      new StreamInterfaceChannel(worker_thread_, channel_);

  dtls_.reset(rtc::SSLStreamAdapter::Create(downward));
  if (!dtls_) {
    LOG_J(LS_ERROR, this) << "Failed to create DTLS adapter.";
    delete downward;
    return false;
  }

  downward_ = downward;

  dtls_->SetIdentity(local_identity_->GetReference());
  dtls_->SetMode(rtc::SSL_MODE_DTLS);
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
    if (!dtls_->SetDtlsSrtpCiphers(srtp_ciphers_)) {
      LOG_J(LS_ERROR, this) << "Couldn't set DTLS-SRTP ciphers.";
      return false;
    }
  } else {
    LOG_J(LS_INFO, this) << "Not using DTLS.";
  }

  LOG_J(LS_INFO, this) << "DTLS setup complete.";
  return true;
}

bool DtlsTransportChannelWrapper::SetSrtpCiphers(
    const std::vector<std::string>& ciphers) {
  if (srtp_ciphers_ == ciphers)
    return true;

  if (dtls_state_ == STATE_STARTED) {
    LOG(LS_WARNING) << "Ignoring new SRTP ciphers while DTLS is negotiating";
    return true;
  }

  if (dtls_state_ == STATE_OPEN) {
    // We don't support DTLS renegotiation currently. If new set of srtp ciphers
    // are different than what's being used currently, we will not use it.
    // So for now, let's be happy (or sad) with a warning message.
    std::string current_srtp_cipher;
    if (!dtls_->GetDtlsSrtpCipher(&current_srtp_cipher)) {
      LOG(LS_ERROR) << "Failed to get the current SRTP cipher for DTLS channel";
      return false;
    }
    const std::vector<std::string>::const_iterator iter =
        std::find(ciphers.begin(), ciphers.end(), current_srtp_cipher);
    if (iter == ciphers.end()) {
      std::string requested_str;
      for (size_t i = 0; i < ciphers.size(); ++i) {
        requested_str.append(" ");
        requested_str.append(ciphers[i]);
        requested_str.append(" ");
      }
      LOG(LS_WARNING) << "Ignoring new set of SRTP ciphers, as DTLS "
                      << "renegotiation is not supported currently "
                      << "current cipher = " << current_srtp_cipher << " and "
                      << "requested = " << "[" << requested_str << "]";
    }
    return true;
  }

  if (dtls_state_ != STATE_NONE &&
      dtls_state_ != STATE_OFFERED &&
      dtls_state_ != STATE_ACCEPTED) {
    ASSERT(false);
    return false;
  }

  srtp_ciphers_ = ciphers;
  return true;
}

bool DtlsTransportChannelWrapper::GetSrtpCipher(std::string* cipher) {
  if (dtls_state_ != STATE_OPEN) {
    return false;
  }

  return dtls_->GetDtlsSrtpCipher(cipher);
}


// Called from upper layers to send a media packet.
int DtlsTransportChannelWrapper::SendPacket(
    const char* data, size_t size,
    const rtc::PacketOptions& options, int flags) {
  int result = -1;

  switch (dtls_state_) {
    case STATE_OFFERED:
      // We don't know if we are doing DTLS yet, so we can't send a packet.
      // TODO(ekr@rtfm.com): assert here?
      result = -1;
      break;

    case STATE_STARTED:
    case STATE_ACCEPTED:
      // Can't send data until the connection is active
      result = -1;
      break;

    case STATE_OPEN:
      if (flags & PF_SRTP_BYPASS) {
        ASSERT(!srtp_ciphers_.empty());
        if (!IsRtpPacket(data, size)) {
          result = -1;
          break;
        }

        result = channel_->SendPacket(data, size, options);
      } else {
        result = (dtls_->WriteAll(data, size, NULL, NULL) ==
          rtc::SR_SUCCESS) ? static_cast<int>(size) : -1;
      }
      break;
      // Not doing DTLS.
    case STATE_NONE:
      result = channel_->SendPacket(data, size, options);
      break;

    case STATE_CLOSED:  // Can't send anything when we're closed.
      return -1;
  }

  return result;
}

// The state transition logic here is as follows:
// (1) If we're not doing DTLS-SRTP, then the state is just the
//     state of the underlying impl()
// (2) If we're doing DTLS-SRTP:
//     - Prior to the DTLS handshake, the state is neither readable or
//       writable
//     - When the impl goes writable for the first time we
//       start the DTLS handshake
//     - Once the DTLS handshake completes, the state is that of the
//       impl again
void DtlsTransportChannelWrapper::OnReadableState(TransportChannel* channel) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  ASSERT(channel == channel_);
  LOG_J(LS_VERBOSE, this)
      << "DTLSTransportChannelWrapper: channel readable state changed.";

  if (dtls_state_ == STATE_NONE || dtls_state_ == STATE_OPEN) {
    set_readable(channel_->readable());
    // Note: SignalReadableState fired by set_readable.
  }
}

void DtlsTransportChannelWrapper::OnWritableState(TransportChannel* channel) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  ASSERT(channel == channel_);
  LOG_J(LS_VERBOSE, this)
      << "DTLSTransportChannelWrapper: channel writable state changed.";

  switch (dtls_state_) {
    case STATE_NONE:
    case STATE_OPEN:
      set_writable(channel_->writable());
      // Note: SignalWritableState fired by set_writable.
      break;

    case STATE_OFFERED:
      // Do nothing
      break;

    case STATE_ACCEPTED:
      if (!MaybeStartDtls()) {
        // This should never happen:
        // Because we are operating in a nonblocking mode and all
        // incoming packets come in via OnReadPacket(), which rejects
        // packets in this state, the incoming queue must be empty. We
        // ignore write errors, thus any errors must be because of
        // configuration and therefore are our fault.
        // Note that in non-debug configurations, failure in
        // MaybeStartDtls() changes the state to STATE_CLOSED.
        ASSERT(false);
      }
      break;

    case STATE_STARTED:
      // Do nothing
      break;

    case STATE_CLOSED:
      // Should not happen. Do nothing
      break;
  }
}

void DtlsTransportChannelWrapper::OnReadPacket(
    TransportChannel* channel, const char* data, size_t size,
    const rtc::PacketTime& packet_time, int flags) {
  ASSERT(rtc::Thread::Current() == worker_thread_);
  ASSERT(channel == channel_);
  ASSERT(flags == 0);

  switch (dtls_state_) {
    case STATE_NONE:
      // We are not doing DTLS
      SignalReadPacket(this, data, size, packet_time, 0);
      break;

    case STATE_OFFERED:
      // Currently drop the packet, but we might in future
      // decide to take this as evidence that the other
      // side is ready to do DTLS and start the handshake
      // on our end
      LOG_J(LS_WARNING, this) << "Received packet before we know if we are "
                              << "doing DTLS or not; dropping.";
      break;

    case STATE_ACCEPTED:
      // Drop packets received before DTLS has actually started
      LOG_J(LS_INFO, this) << "Dropping packet received before DTLS started.";
      break;

    case STATE_STARTED:
    case STATE_OPEN:
      // We should only get DTLS or SRTP packets; STUN's already been demuxed.
      // Is this potentially a DTLS packet?
      if (IsDtlsPacket(data, size)) {
        if (!HandleDtlsPacket(data, size)) {
          LOG_J(LS_ERROR, this) << "Failed to handle DTLS packet.";
          return;
        }
      } else {
        // Not a DTLS packet; our handshake should be complete by now.
        if (dtls_state_ != STATE_OPEN) {
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
    case STATE_CLOSED:
      // This shouldn't be happening. Drop the packet
      break;
  }
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
      dtls_state_ = STATE_OPEN;

      set_readable(true);
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
    if (!err) {
      LOG_J(LS_INFO, this) << "DTLS channel closed";
    } else {
      LOG_J(LS_INFO, this) << "DTLS channel error, code=" << err;
    }

    set_readable(false);
    set_writable(false);
    dtls_state_ = STATE_CLOSED;
  }
}

bool DtlsTransportChannelWrapper::MaybeStartDtls() {
  if (channel_->writable()) {
    if (dtls_->StartSSLWithPeer()) {
      LOG_J(LS_ERROR, this) << "Couldn't start DTLS handshake";
      dtls_state_ = STATE_CLOSED;
      return false;
    }
    LOG_J(LS_INFO, this)
      << "DtlsTransportChannelWrapper: Started DTLS handshake";

    dtls_state_ = STATE_STARTED;
  }
  return true;
}

// Called from OnReadPacket when a DTLS packet is received.
bool DtlsTransportChannelWrapper::HandleDtlsPacket(const char* data,
                                                   size_t size) {
  // Sanity check we're not passing junk that
  // just looks like DTLS.
  const uint8* tmp_data = reinterpret_cast<const uint8* >(data);
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

void DtlsTransportChannelWrapper::OnRequestSignaling(
    TransportChannelImpl* channel) {
  ASSERT(channel == channel_);
  SignalRequestSignaling(this);
}

void DtlsTransportChannelWrapper::OnCandidateReady(
    TransportChannelImpl* channel, const Candidate& c) {
  ASSERT(channel == channel_);
  SignalCandidateReady(this, c);
}

void DtlsTransportChannelWrapper::OnCandidatesAllocationDone(
    TransportChannelImpl* channel) {
  ASSERT(channel == channel_);
  SignalCandidatesAllocationDone(this);
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

}  // namespace cricket
