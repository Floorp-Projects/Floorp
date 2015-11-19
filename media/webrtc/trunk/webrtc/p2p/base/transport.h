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
// On Threading:  Transport performs work on both the signaling and worker
// threads.  For subclasses, the rule is that all signaling related calls will
// be made on the signaling thread and all channel related calls (including
// signaling for a channel) will be made on the worker thread.  When
// information needs to be sent between the two threads, this class should do
// the work (e.g., OnRemoteCandidate).
//
// Note: Subclasses must call DestroyChannels() in their own constructors.
// It is not possible to do so here because the subclass constructor will
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
#include "webrtc/base/criticalsection.h"
#include "webrtc/base/messagequeue.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/sslstreamadapter.h"

namespace rtc {
class Thread;
}

namespace cricket {

class PortAllocator;
class TransportChannel;
class TransportChannelImpl;

typedef std::vector<Candidate> Candidates;

// For "writable" and "readable", we need to differentiate between
// none, all, and some.
enum TransportState {
  TRANSPORT_STATE_NONE = 0,
  TRANSPORT_STATE_SOME,
  TRANSPORT_STATE_ALL
};

// Stats that we can return about the connections for a transport channel.
// TODO(hta): Rename to ConnectionStats
struct ConnectionInfo {
  ConnectionInfo()
      : best_connection(false),
        writable(false),
        readable(false),
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
  bool readable;               // Has this connection received a STUN request?
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
  int component;
  ConnectionInfos connection_infos;
  std::string srtp_cipher;
  std::string ssl_cipher;
};

// Information about all the channels of a transport.
// TODO(hta): Consider if a simple vector is as good as a map.
typedef std::vector<TransportChannelStats> TransportChannelStatsList;

// Information about the stats of a transport.
struct TransportStats {
  std::string content_name;
  TransportChannelStatsList channel_stats;
};

bool BadTransportDescription(const std::string& desc, std::string* err_desc);

bool IceCredentialsChanged(const std::string& old_ufrag,
                           const std::string& old_pwd,
                           const std::string& new_ufrag,
                           const std::string& new_pwd);

class Transport : public rtc::MessageHandler,
                  public sigslot::has_slots<> {
 public:
  Transport(rtc::Thread* signaling_thread,
            rtc::Thread* worker_thread,
            const std::string& content_name,
            const std::string& type,
            PortAllocator* allocator);
  virtual ~Transport();

  // Returns the signaling thread. The app talks to Transport on this thread.
  rtc::Thread* signaling_thread() { return signaling_thread_; }
  // Returns the worker thread. The actual networking is done on this thread.
  rtc::Thread* worker_thread() { return worker_thread_; }

  // Returns the content_name of this transport.
  const std::string& content_name() const { return content_name_; }
  // Returns the type of this transport.
  const std::string& type() const { return type_; }

  // Returns the port allocator object for this transport.
  PortAllocator* port_allocator() { return allocator_; }

  // Returns the readable and states of this manager.  These bits are the ORs
  // of the corresponding bits on the managed channels.  Each time one of these
  // states changes, a signal is raised.
  // TODO: Replace uses of readable() and writable() with
  // any_channels_readable() and any_channels_writable().
  bool readable() const { return any_channels_readable(); }
  bool writable() const { return any_channels_writable(); }
  bool was_writable() const { return was_writable_; }
  bool any_channels_readable() const {
    return (readable_ == TRANSPORT_STATE_SOME ||
            readable_ == TRANSPORT_STATE_ALL);
  }
  bool any_channels_writable() const {
    return (writable_ == TRANSPORT_STATE_SOME ||
            writable_ == TRANSPORT_STATE_ALL);
  }
  bool all_channels_readable() const {
    return (readable_ == TRANSPORT_STATE_ALL);
  }
  bool all_channels_writable() const {
    return (writable_ == TRANSPORT_STATE_ALL);
  }
  sigslot::signal1<Transport*> SignalReadableState;
  sigslot::signal1<Transport*> SignalWritableState;
  sigslot::signal1<Transport*> SignalCompleted;
  sigslot::signal1<Transport*> SignalFailed;

  // Returns whether the client has requested the channels to connect.
  bool connect_requested() const { return connect_requested_; }

  void SetIceRole(IceRole role);
  IceRole ice_role() const { return ice_role_; }

  void SetIceTiebreaker(uint64 IceTiebreaker) { tiebreaker_ = IceTiebreaker; }
  uint64 IceTiebreaker() { return tiebreaker_; }

  // Must be called before applying local session description.
  void SetIdentity(rtc::SSLIdentity* identity);

  // Get a copy of the local identity provided by SetIdentity.
  bool GetIdentity(rtc::SSLIdentity** identity);

  // Get a copy of the remote certificate in use by the specified channel.
  bool GetRemoteCertificate(rtc::SSLCertificate** cert);

  TransportProtocol protocol() const { return protocol_; }

  // Create, destroy, and lookup the channels of this type by their components.
  TransportChannelImpl* CreateChannel(int component);
  // Note: GetChannel may lead to race conditions, since the mutex is not held
  // after the pointer is returned.
  TransportChannelImpl* GetChannel(int component);
  // Note: HasChannel does not lead to race conditions, unlike GetChannel.
  bool HasChannel(int component) {
    return (NULL != GetChannel(component));
  }
  bool HasChannels();
  void DestroyChannel(int component);

  // Set the local TransportDescription to be used by TransportChannels.
  // This should be called before ConnectChannels().
  bool SetLocalTransportDescription(const TransportDescription& description,
                                    ContentAction action,
                                    std::string* error_desc);

  // Set the remote TransportDescription to be used by TransportChannels.
  bool SetRemoteTransportDescription(const TransportDescription& description,
                                     ContentAction action,
                                     std::string* error_desc);

  // Tells all current and future channels to start connecting.  When the first
  // channel begins connecting, the following signal is raised.
  void ConnectChannels();
  sigslot::signal1<Transport*> SignalConnecting;

  // Resets all of the channels back to their initial state.  They are no
  // longer connecting.
  void ResetChannels();

  // Destroys every channel created so far.
  void DestroyAllChannels();

  bool GetStats(TransportStats* stats);

  // Before any stanza is sent, the manager will request signaling.  Once
  // signaling is available, the client should call OnSignalingReady.  Once
  // this occurs, the transport (or its channels) can send any waiting stanzas.
  // OnSignalingReady invokes OnTransportSignalingReady and then forwards this
  // signal to each channel.
  sigslot::signal1<Transport*> SignalRequestSignaling;
  void OnSignalingReady();

  // Handles sending of ready candidates and receiving of remote candidates.
  sigslot::signal2<Transport*,
                   const std::vector<Candidate>&> SignalCandidatesReady;

  sigslot::signal1<Transport*> SignalCandidatesAllocationDone;
  void OnRemoteCandidates(const std::vector<Candidate>& candidates);

  // If candidate is not acceptable, returns false and sets error.
  // Call this before calling OnRemoteCandidates.
  virtual bool VerifyCandidate(const Candidate& candidate,
                               std::string* error);

  // Signals when the best connection for a channel changes.
  sigslot::signal3<Transport*,
                   int,  // component
                   const Candidate&> SignalRouteChange;

  // Forwards the signal from TransportChannel to BaseSession.
  sigslot::signal0<> SignalRoleConflict;

  virtual bool GetSslRole(rtc::SSLRole* ssl_role) const;

 protected:
  // These are called by Create/DestroyChannel above in order to create or
  // destroy the appropriate type of channel.
  virtual TransportChannelImpl* CreateTransportChannel(int component) = 0;
  virtual void DestroyTransportChannel(TransportChannelImpl* channel) = 0;

  // Informs the subclass that we received the signaling ready message.
  virtual void OnTransportSignalingReady() {}

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

  virtual void SetIdentity_w(rtc::SSLIdentity* identity) {}

  virtual bool GetIdentity_w(rtc::SSLIdentity** identity) {
    return false;
  }

  // Pushes down the transport parameters from the local description, such
  // as the ICE ufrag and pwd.
  // Derived classes can override, but must call the base as well.
  virtual bool ApplyLocalTransportDescription_w(TransportChannelImpl* channel,
                                                std::string* error_desc);

  // Pushes down remote ice credentials from the remote description to the
  // transport channel.
  virtual bool ApplyRemoteTransportDescription_w(TransportChannelImpl* ch,
                                                 std::string* error_desc);

  // Negotiates the transport parameters based on the current local and remote
  // transport description, such at the version of ICE to use, and whether DTLS
  // should be activated.
  // Derived classes can negotiate their specific parameters here, but must call
  // the base as well.
  virtual bool NegotiateTransportDescription_w(ContentAction local_role,
                                               std::string* error_desc);

  // Pushes down the transport parameters obtained via negotiation.
  // Derived classes can set their specific parameters here, but must call the
  // base as well.
  virtual bool ApplyNegotiatedTransportDescription_w(
      TransportChannelImpl* channel, std::string* error_desc);

  virtual bool GetSslRole_w(rtc::SSLRole* ssl_role) const {
    return false;
  }

 private:
  struct ChannelMapEntry {
    ChannelMapEntry() : impl_(NULL), candidates_allocated_(false), ref_(0) {}
    explicit ChannelMapEntry(TransportChannelImpl *impl)
        : impl_(impl),
          candidates_allocated_(false),
          ref_(0) {
    }

    void AddRef() { ++ref_; }
    void DecRef() {
      ASSERT(ref_ > 0);
      --ref_;
    }
    int ref() const { return ref_; }

    TransportChannelImpl* get() const { return impl_; }
    TransportChannelImpl* operator->() const  { return impl_; }
    void set_candidates_allocated(bool status) {
      candidates_allocated_ = status;
    }
    bool candidates_allocated() const { return candidates_allocated_; }

  private:
    TransportChannelImpl *impl_;
    bool candidates_allocated_;
    int ref_;
  };

  // Candidate component => ChannelMapEntry
  typedef std::map<int, ChannelMapEntry> ChannelMap;

  // Called when the state of a channel changes.
  void OnChannelReadableState(TransportChannel* channel);
  void OnChannelWritableState(TransportChannel* channel);

  // Called when a channel requests signaling.
  void OnChannelRequestSignaling(TransportChannelImpl* channel);

  // Called when a candidate is ready from remote peer.
  void OnRemoteCandidate(const Candidate& candidate);
  // Called when a candidate is ready from channel.
  void OnChannelCandidateReady(TransportChannelImpl* channel,
                               const Candidate& candidate);
  void OnChannelRouteChange(TransportChannel* channel,
                            const Candidate& remote_candidate);
  void OnChannelCandidatesAllocationDone(TransportChannelImpl* channel);
  // Called when there is ICE role change.
  void OnRoleConflict(TransportChannelImpl* channel);
  // Called when the channel removes a connection.
  void OnChannelConnectionRemoved(TransportChannelImpl* channel);

  // Dispatches messages to the appropriate handler (below).
  void OnMessage(rtc::Message* msg);

  // These are versions of the above methods that are called only on a
  // particular thread (s = signaling, w = worker).  The above methods post or
  // send a message to invoke this version.
  TransportChannelImpl* CreateChannel_w(int component);
  void DestroyChannel_w(int component);
  void ConnectChannels_w();
  void ResetChannels_w();
  void DestroyAllChannels_w();
  void OnRemoteCandidate_w(const Candidate& candidate);
  void OnChannelReadableState_s();
  void OnChannelWritableState_s();
  void OnChannelRequestSignaling_s();
  void OnConnecting_s();
  void OnChannelRouteChange_s(const TransportChannel* channel,
                              const Candidate& remote_candidate);
  void OnChannelCandidatesAllocationDone_s();

  // Helper function that invokes the given function on every channel.
  typedef void (TransportChannelImpl::* TransportChannelFunc)();
  void CallChannels_w(TransportChannelFunc func);

  // Computes the OR of the channel's read or write state (argument picks).
  TransportState GetTransportState_s(bool read);

  void OnChannelCandidateReady_s();

  void SetIceRole_w(IceRole role);
  void SetRemoteIceMode_w(IceMode mode);
  bool SetLocalTransportDescription_w(const TransportDescription& desc,
                                      ContentAction action,
                                      std::string* error_desc);
  bool SetRemoteTransportDescription_w(const TransportDescription& desc,
                                       ContentAction action,
                                       std::string* error_desc);
  bool GetStats_w(TransportStats* infos);
  bool GetRemoteCertificate_w(rtc::SSLCertificate** cert);

  // Sends SignalCompleted if we are now in that state.
  void MaybeCompleted_w();

  rtc::Thread* const signaling_thread_;
  rtc::Thread* const worker_thread_;
  const std::string content_name_;
  const std::string type_;
  PortAllocator* const allocator_;
  bool destroyed_;
  TransportState readable_;
  TransportState writable_;
  bool was_writable_;
  bool connect_requested_;
  IceRole ice_role_;
  uint64 tiebreaker_;
  TransportProtocol protocol_;
  IceMode remote_ice_mode_;
  rtc::scoped_ptr<TransportDescription> local_description_;
  rtc::scoped_ptr<TransportDescription> remote_description_;

  // TODO(tommi): Make sure we only use this on the worker thread.
  ChannelMap channels_;
  // Buffers the ready_candidates so that SignalCanidatesReady can
  // provide them in multiples.
  std::vector<Candidate> ready_candidates_;
  // Protects changes to channels and messages
  rtc::CriticalSection crit_;

  DISALLOW_EVIL_CONSTRUCTORS(Transport);
};

// Extract a TransportProtocol from a TransportDescription.
TransportProtocol TransportProtocolFromDescription(
    const TransportDescription* desc);

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_TRANSPORT_H_
