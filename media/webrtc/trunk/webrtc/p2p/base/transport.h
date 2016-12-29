/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// A Transport manages a set of named channels of the same type.
//
// Subclasses choose the appropriate class to instantiate for each channel;
// however, this base class keeps track of the channels by name, watches their
// state changes (in order to update the manager's state), and forwards
// requests to begin connecting or to reset to each of the channels.
//
// On Threading:  Transport performs work solely on the worker thread, and so
// its methods should only be called on the worker thread.
//
// Note: Subclasses must call DestroyChannels() in their own destructors.
// It is not possible to do so here because the subclass destructor will
// already have run.

#ifndef WEBRTC_P2P_BASE_TRANSPORT_H_
#define WEBRTC_P2P_BASE_TRANSPORT_H_

#include <map>
#include <string>
#include <vector>
#include "webrtc/p2p/base/candidate.h"
#include "webrtc/p2p/base/constants.h"
#include "webrtc/p2p/base/sessiondescription.h"
#include "webrtc/p2p/base/transportinfo.h"
#include "webrtc/base/messagequeue.h"
#include "webrtc/base/rtccertificate.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/sslstreamadapter.h"

namespace cricket {

class PortAllocator;
class TransportChannel;
class TransportChannelImpl;

typedef std::vector<Candidate> Candidates;

// TODO(deadbeef): Unify with PeerConnectionInterface::IceConnectionState
// once /talk/ and /webrtc/ are combined, and also switch to ENUM_NAME naming
// style.
enum IceConnectionState {
  kIceConnectionConnecting = 0,
  kIceConnectionFailed,
  kIceConnectionConnected,  // Writable, but still checking one or more
                            // connections
  kIceConnectionCompleted,
};

enum DtlsTransportState {
  // Haven't started negotiating.
  DTLS_TRANSPORT_NEW = 0,
  // Have started negotiating.
  DTLS_TRANSPORT_CONNECTING,
  // Negotiated, and has a secure connection.
  DTLS_TRANSPORT_CONNECTED,
  // Transport is closed.
  DTLS_TRANSPORT_CLOSED,
  // Failed due to some error in the handshake process.
  DTLS_TRANSPORT_FAILED,
};

// TODO(deadbeef): Unify with PeerConnectionInterface::IceConnectionState
// once /talk/ and /webrtc/ are combined, and also switch to ENUM_NAME naming
// style.
enum IceGatheringState {
  kIceGatheringNew = 0,
  kIceGatheringGathering,
  kIceGatheringComplete,
};

// Stats that we can return about the connections for a transport channel.
// TODO(hta): Rename to ConnectionStats
struct ConnectionInfo {
  ConnectionInfo()
      : best_connection(false),
        writable(false),
        receiving(false),
        timeout(false),
        new_connection(false),
        rtt(0),
        sent_total_bytes(0),
        sent_bytes_second(0),
        sent_discarded_packets(0),
        sent_total_packets(0),
        recv_total_bytes(0),
        recv_bytes_second(0),
        key(NULL) {}

  bool best_connection;        // Is this the best connection we have?
  bool writable;               // Has this connection received a STUN response?
  bool receiving;              // Has this connection received anything?
  bool timeout;                // Has this connection timed out?
  bool new_connection;         // Is this a newly created connection?
  size_t rtt;                  // The STUN RTT for this connection.
  size_t sent_total_bytes;     // Total bytes sent on this connection.
  size_t sent_bytes_second;    // Bps over the last measurement interval.
  size_t sent_discarded_packets;  // Number of outgoing packets discarded due to
                                  // socket errors.
  size_t sent_total_packets;  // Number of total outgoing packets attempted for
                              // sending.

  size_t recv_total_bytes;     // Total bytes received on this connection.
  size_t recv_bytes_second;    // Bps over the last measurement interval.
  Candidate local_candidate;   // The local candidate for this connection.
  Candidate remote_candidate;  // The remote candidate for this connection.
  void* key;                   // A static value that identifies this conn.
};

// Information about all the connections of a channel.
typedef std::vector<ConnectionInfo> ConnectionInfos;

// Information about a specific channel
struct TransportChannelStats {
  int component = 0;
  ConnectionInfos connection_infos;
  int srtp_crypto_suite = rtc::SRTP_INVALID_CRYPTO_SUITE;
  int ssl_cipher_suite = rtc::TLS_NULL_WITH_NULL_NULL;
};

// Information about all the channels of a transport.
// TODO(hta): Consider if a simple vector is as good as a map.
typedef std::vector<TransportChannelStats> TransportChannelStatsList;

// Information about the stats of a transport.
struct TransportStats {
  std::string transport_name;
  TransportChannelStatsList channel_stats;
};

// Information about ICE configuration.
struct IceConfig {
  // The ICE connection receiving timeout value.
  // TODO(honghaiz): Remove suffix _ms to be consistent.
  int receiving_timeout_ms = -1;
  // Time interval in milliseconds to ping a backup connection when the ICE
  // channel is strongly connected.
  int backup_connection_ping_interval = -1;
  // If true, the most recent port allocator session will keep on running.
  bool gather_continually = false;
};

bool BadTransportDescription(const std::string& desc, std::string* err_desc);

bool IceCredentialsChanged(const std::string& old_ufrag,
                           const std::string& old_pwd,
                           const std::string& new_ufrag,
                           const std::string& new_pwd);

class Transport : public sigslot::has_slots<> {
 public:
  Transport(const std::string& name, PortAllocator* allocator);
  virtual ~Transport();

  // Returns the name of this transport.
  const std::string& name() const { return name_; }

  // Returns the port allocator object for this transport.
  PortAllocator* port_allocator() { return allocator_; }

  bool ready_for_remote_candidates() const {
    return local_description_set_ && remote_description_set_;
  }

  // Returns whether the client has requested the channels to connect.
  bool connect_requested() const { return connect_requested_; }

  void SetIceRole(IceRole role);
  IceRole ice_role() const { return ice_role_; }

  void SetIceTiebreaker(uint64_t IceTiebreaker) { tiebreaker_ = IceTiebreaker; }
  uint64_t IceTiebreaker() { return tiebreaker_; }

  void SetIceConfig(const IceConfig& config);

  // Must be called before applying local session description.
  virtual void SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {}

  // Get a copy of the local certificate provided by SetLocalCertificate.
  virtual bool GetLocalCertificate(
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate) {
    return false;
  }

  // Get a copy of the remote certificate in use by the specified channel.
  bool GetRemoteSSLCertificate(rtc::SSLCertificate** cert);

  // Create, destroy, and lookup the channels of this type by their components.
  TransportChannelImpl* CreateChannel(int component);

  TransportChannelImpl* GetChannel(int component);

  bool HasChannel(int component) {
    return (NULL != GetChannel(component));
  }
  bool HasChannels();

  void DestroyChannel(int component);

  // Set the local TransportDescription to be used by TransportChannels.
  bool SetLocalTransportDescription(const TransportDescription& description,
                                    ContentAction action,
                                    std::string* error_desc);

  // Set the remote TransportDescription to be used by TransportChannels.
  bool SetRemoteTransportDescription(const TransportDescription& description,
                                     ContentAction action,
                                     std::string* error_desc);

  // Tells all current and future channels to start connecting.
  void ConnectChannels();

  // Tells channels to start gathering candidates if necessary.
  // Should be called after ConnectChannels() has been called at least once,
  // which will happen in SetLocalTransportDescription.
  void MaybeStartGathering();

  // Resets all of the channels back to their initial state.  They are no
  // longer connecting.
  void ResetChannels();

  // Destroys every channel created so far.
  void DestroyAllChannels();

  bool GetStats(TransportStats* stats);

  // Called when one or more candidates are ready from the remote peer.
  bool AddRemoteCandidates(const std::vector<Candidate>& candidates,
                           std::string* error);

  // If candidate is not acceptable, returns false and sets error.
  // Call this before calling OnRemoteCandidates.
  virtual bool VerifyCandidate(const Candidate& candidate,
                               std::string* error);

  virtual bool GetSslRole(rtc::SSLRole* ssl_role) const { return false; }

  // Must be called before channel is starting to connect.
  virtual bool SetSslMaxProtocolVersion(rtc::SSLProtocolVersion version) {
    return false;
  }

 protected:
  // These are called by Create/DestroyChannel above in order to create or
  // destroy the appropriate type of channel.
  virtual TransportChannelImpl* CreateTransportChannel(int component) = 0;
  virtual void DestroyTransportChannel(TransportChannelImpl* channel) = 0;

  // The current local transport description, for use by derived classes
  // when performing transport description negotiation.
  const TransportDescription* local_description() const {
    return local_description_.get();
  }

  // The current remote transport description, for use by derived classes
  // when performing transport description negotiation.
  const TransportDescription* remote_description() const {
    return remote_description_.get();
  }

  // Pushes down the transport parameters from the local description, such
  // as the ICE ufrag and pwd.
  // Derived classes can override, but must call the base as well.
  virtual bool ApplyLocalTransportDescription(TransportChannelImpl* channel,
                                              std::string* error_desc);

  // Pushes down remote ice credentials from the remote description to the
  // transport channel.
  virtual bool ApplyRemoteTransportDescription(TransportChannelImpl* ch,
                                               std::string* error_desc);

  // Negotiates the transport parameters based on the current local and remote
  // transport description, such as the ICE role to use, and whether DTLS
  // should be activated.
  // Derived classes can negotiate their specific parameters here, but must call
  // the base as well.
  virtual bool NegotiateTransportDescription(ContentAction local_role,
                                             std::string* error_desc);

  // Pushes down the transport parameters obtained via negotiation.
  // Derived classes can set their specific parameters here, but must call the
  // base as well.
  virtual bool ApplyNegotiatedTransportDescription(
      TransportChannelImpl* channel,
      std::string* error_desc);

 private:
  // Candidate component => TransportChannelImpl*
  typedef std::map<int, TransportChannelImpl*> ChannelMap;

  // Helper function that invokes the given function on every channel.
  typedef void (TransportChannelImpl::* TransportChannelFunc)();
  void CallChannels(TransportChannelFunc func);

  const std::string name_;
  PortAllocator* const allocator_;
  bool channels_destroyed_ = false;
  bool connect_requested_ = false;
  IceRole ice_role_ = ICEROLE_UNKNOWN;
  uint64_t tiebreaker_ = 0;
  IceMode remote_ice_mode_ = ICEMODE_FULL;
  IceConfig ice_config_;
  rtc::scoped_ptr<TransportDescription> local_description_;
  rtc::scoped_ptr<TransportDescription> remote_description_;
  bool local_description_set_ = false;
  bool remote_description_set_ = false;

  ChannelMap channels_;

  RTC_DISALLOW_COPY_AND_ASSIGN(Transport);
};


}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_TRANSPORT_H_
