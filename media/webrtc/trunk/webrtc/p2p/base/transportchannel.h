/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_TRANSPORTCHANNEL_H_
#define WEBRTC_P2P_BASE_TRANSPORTCHANNEL_H_

#include <string>
#include <vector>

#include "webrtc/p2p/base/candidate.h"
#include "webrtc/p2p/base/transport.h"
#include "webrtc/p2p/base/transportdescription.h"
#include "webrtc/base/asyncpacketsocket.h"
#include "webrtc/base/basictypes.h"
#include "webrtc/base/dscp.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/socket.h"
#include "webrtc/base/sslidentity.h"
#include "webrtc/base/sslstreamadapter.h"

namespace cricket {

class Candidate;

// Flags for SendPacket/SignalReadPacket.
enum PacketFlags {
  PF_NORMAL       = 0x00,  // A normal packet.
  PF_SRTP_BYPASS  = 0x01,  // An encrypted SRTP packet; bypass any additional
                           // crypto provided by the transport (e.g. DTLS)
};

// Used to indicate channel's connection state.
enum TransportChannelState { STATE_CONNECTING, STATE_COMPLETED, STATE_FAILED };

// A TransportChannel represents one logical stream of packets that are sent
// between the two sides of a session.
class TransportChannel : public sigslot::has_slots<> {
 public:
  explicit TransportChannel(const std::string& content_name, int component)
      : content_name_(content_name),
        component_(component),
        readable_(false), writable_(false) {}
  virtual ~TransportChannel() {}

  // TODO(guoweis) - Make this pure virtual once all subclasses of
  // TransportChannel have this defined.
  virtual TransportChannelState GetState() const {
    return TransportChannelState::STATE_CONNECTING;
  }

  // TODO(mallinath) - Remove this API, as it's no longer useful.
  // Returns the session id of this channel.
  virtual const std::string SessionId() const { return std::string(); }

  const std::string& content_name() const { return content_name_; }
  int component() const { return component_; }

  // Returns the readable and states of this channel.  Each time one of these
  // states changes, a signal is raised.  These states are aggregated by the
  // TransportManager.
  bool readable() const { return readable_; }
  bool writable() const { return writable_; }
  sigslot::signal1<TransportChannel*> SignalReadableState;
  sigslot::signal1<TransportChannel*> SignalWritableState;
  // Emitted when the TransportChannel's ability to send has changed.
  sigslot::signal1<TransportChannel*> SignalReadyToSend;

  // Attempts to send the given packet.  The return value is < 0 on failure.
  // TODO: Remove the default argument once channel code is updated.
  virtual int SendPacket(const char* data, size_t len,
                         const rtc::PacketOptions& options,
                         int flags = 0) = 0;

  // Sets a socket option on this channel.  Note that not all options are
  // supported by all transport types.
  virtual int SetOption(rtc::Socket::Option opt, int value) = 0;
  // TODO(pthatcher): Once Chrome's MockTransportChannel implments
  // this, remove the default implementation.
  virtual bool GetOption(rtc::Socket::Option opt, int* value) { return false; }

  // Returns the most recent error that occurred on this channel.
  virtual int GetError() = 0;

  // Returns the current stats for this connection.
  virtual bool GetStats(ConnectionInfos* infos) = 0;

  // Is DTLS active?
  virtual bool IsDtlsActive() const = 0;

  // Default implementation.
  virtual bool GetSslRole(rtc::SSLRole* role) const = 0;

  // Sets up the ciphers to use for DTLS-SRTP.
  virtual bool SetSrtpCiphers(const std::vector<std::string>& ciphers) = 0;

  // Finds out which DTLS-SRTP cipher was negotiated.
  virtual bool GetSrtpCipher(std::string* cipher) = 0;

  // Finds out which DTLS cipher was negotiated.
  virtual bool GetSslCipher(std::string* cipher) = 0;

  // Gets a copy of the local SSL identity, owned by the caller.
  virtual bool GetLocalIdentity(rtc::SSLIdentity** identity) const = 0;

  // Gets a copy of the remote side's SSL certificate, owned by the caller.
  virtual bool GetRemoteCertificate(rtc::SSLCertificate** cert) const = 0;

  // Allows key material to be extracted for external encryption.
  virtual bool ExportKeyingMaterial(const std::string& label,
      const uint8* context,
      size_t context_len,
      bool use_context,
      uint8* result,
      size_t result_len) = 0;

  // Signalled each time a packet is received on this channel.
  sigslot::signal5<TransportChannel*, const char*,
                   size_t, const rtc::PacketTime&, int> SignalReadPacket;

  // This signal occurs when there is a change in the way that packets are
  // being routed, i.e. to a different remote location. The candidate
  // indicates where and how we are currently sending media.
  sigslot::signal2<TransportChannel*, const Candidate&> SignalRouteChange;

  // Invoked when the channel is being destroyed.
  sigslot::signal1<TransportChannel*> SignalDestroyed;

  // Debugging description of this transport channel.
  std::string ToString() const;

 protected:
  // Sets the readable state, signaling if necessary.
  void set_readable(bool readable);

  // Sets the writable state, signaling if necessary.
  void set_writable(bool writable);


 private:
  // Used mostly for debugging.
  std::string content_name_;
  int component_;
  bool readable_;
  bool writable_;

  DISALLOW_EVIL_CONSTRUCTORS(TransportChannel);
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_TRANSPORTCHANNEL_H_
