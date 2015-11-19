/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_SESSION_H_
#define WEBRTC_P2P_BASE_SESSION_H_

#include <list>
#include <map>
#include <string>
#include <vector>

#include "webrtc/p2p/base/candidate.h"
#include "webrtc/p2p/base/port.h"
#include "webrtc/p2p/base/transport.h"
#include "webrtc/base/refcount.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/scoped_ref_ptr.h"
#include "webrtc/base/socketaddress.h"

namespace cricket {

class BaseSession;
class P2PTransportChannel;
class Transport;
class TransportChannel;
class TransportChannelProxy;
class TransportChannelImpl;

typedef rtc::RefCountedObject<rtc::scoped_ptr<Transport> >
TransportWrapper;

// Bundles a Transport and ChannelMap together. ChannelMap is used to
// create transport channels before receiving or sending a session
// initiate, and for speculatively connecting channels.  Previously, a
// session had one ChannelMap and transport.  Now, with multiple
// transports per session, we need multiple ChannelMaps as well.

typedef std::map<int, TransportChannelProxy*> ChannelMap;

class TransportProxy : public sigslot::has_slots<> {
 public:
  TransportProxy(
      rtc::Thread* worker_thread,
      const std::string& sid,
      const std::string& content_name,
      TransportWrapper* transport)
      : worker_thread_(worker_thread),
        sid_(sid),
        content_name_(content_name),
        transport_(transport),
        connecting_(false),
        negotiated_(false),
        sent_candidates_(false),
        candidates_allocated_(false),
        local_description_set_(false),
        remote_description_set_(false) {
    transport_->get()->SignalCandidatesReady.connect(
        this, &TransportProxy::OnTransportCandidatesReady);
  }
  ~TransportProxy();

  const std::string& content_name() const { return content_name_; }
  // TODO(juberti): It's not good form to expose the object you're wrapping,
  // since callers can mutate it. Can we make this return a const Transport*?
  Transport* impl() const { return transport_->get(); }

  const std::string& type() const;
  bool negotiated() const { return negotiated_; }
  const Candidates& sent_candidates() const { return sent_candidates_; }
  const Candidates& unsent_candidates() const { return unsent_candidates_; }
  bool candidates_allocated() const { return candidates_allocated_; }
  void set_candidates_allocated(bool allocated) {
    candidates_allocated_ = allocated;
  }

  TransportChannel* GetChannel(int component);
  TransportChannel* CreateChannel(int component);
  bool HasChannel(int component);
  void DestroyChannel(int component);

  void AddSentCandidates(const Candidates& candidates);
  void AddUnsentCandidates(const Candidates& candidates);
  void ClearSentCandidates() { sent_candidates_.clear(); }
  void ClearUnsentCandidates() { unsent_candidates_.clear(); }

  // Start the connection process for any channels, creating impls if needed.
  void ConnectChannels();
  // Hook up impls to the proxy channels. Doesn't change connect state.
  void CompleteNegotiation();

  // Mux this proxy onto the specified proxy's transport.
  bool SetupMux(TransportProxy* proxy);

  // Simple functions that thunk down to the same functions on Transport.
  void SetIceRole(IceRole role);
  void SetIdentity(rtc::SSLIdentity* identity);
  bool SetLocalTransportDescription(const TransportDescription& description,
                                    ContentAction action,
                                    std::string* error_desc);
  bool SetRemoteTransportDescription(const TransportDescription& description,
                                     ContentAction action,
                                     std::string* error_desc);
  void OnSignalingReady();
  bool OnRemoteCandidates(const Candidates& candidates, std::string* error);

  // Called when a transport signals that it has new candidates.
  void OnTransportCandidatesReady(cricket::Transport* transport,
                                  const Candidates& candidates) {
    SignalCandidatesReady(this, candidates);
  }

  bool local_description_set() const {
    return local_description_set_;
  }
  bool remote_description_set() const {
    return remote_description_set_;
  }

  // Handles sending of ready candidates and receiving of remote candidates.
  sigslot::signal2<TransportProxy*,
                         const std::vector<Candidate>&> SignalCandidatesReady;

 private:
  TransportChannelProxy* GetChannelProxy(int component) const;

  // Creates a new channel on the Transport which causes the reference
  // count to increment.
  void CreateChannelImpl(int component);
  void CreateChannelImpl_w(int component);

  // Manipulators of transportchannelimpl in channel proxy.
  void SetChannelImplFromTransport(TransportChannelProxy* proxy, int component);
  void SetChannelImplFromTransport_w(TransportChannelProxy* proxy,
                                     int component);
  void ReplaceChannelImpl(TransportChannelProxy* proxy,
                          TransportChannelImpl* impl);
  void ReplaceChannelImpl_w(TransportChannelProxy* proxy,
                            TransportChannelImpl* impl);

  rtc::Thread* const worker_thread_;
  const std::string sid_;
  const std::string content_name_;
  rtc::scoped_refptr<TransportWrapper> transport_;
  bool connecting_;
  bool negotiated_;
  ChannelMap channels_;
  Candidates sent_candidates_;
  Candidates unsent_candidates_;
  bool candidates_allocated_;
  bool local_description_set_;
  bool remote_description_set_;
};

typedef std::map<std::string, TransportProxy*> TransportMap;

// Statistics for all the transports of this session.
typedef std::map<std::string, TransportStats> TransportStatsMap;
typedef std::map<std::string, std::string> ProxyTransportMap;

// TODO(pthatcher): Think of a better name for this.  We already have
// a TransportStats in transport.h.  Perhaps TransportsStats?
struct SessionStats {
  ProxyTransportMap proxy_to_transport;
  TransportStatsMap transport_stats;
};

// A BaseSession manages general session state. This includes negotiation
// of both the application-level and network-level protocols:  the former
// defines what will be sent and the latter defines how it will be sent.  Each
// network-level protocol is represented by a Transport object.  Each Transport
// participates in the network-level negotiation.  The individual streams of
// packets are represented by TransportChannels.  The application-level protocol
// is represented by SessionDecription objects.
class BaseSession : public sigslot::has_slots<>,
                    public rtc::MessageHandler {
 public:
  enum {
    MSG_TIMEOUT = 0,
    MSG_ERROR,
    MSG_STATE,
  };

  enum State {
    STATE_INIT = 0,
    STATE_SENTINITIATE,       // sent initiate, waiting for Accept or Reject
    STATE_RECEIVEDINITIATE,   // received an initiate. Call Accept or Reject
    STATE_SENTPRACCEPT,       // sent provisional Accept
    STATE_SENTACCEPT,         // sent accept. begin connecting transport
    STATE_RECEIVEDPRACCEPT,   // received provisional Accept, waiting for Accept
    STATE_RECEIVEDACCEPT,     // received accept. begin connecting transport
    STATE_SENTMODIFY,         // sent modify, waiting for Accept or Reject
    STATE_RECEIVEDMODIFY,     // received modify, call Accept or Reject
    STATE_SENTREJECT,         // sent reject after receiving initiate
    STATE_RECEIVEDREJECT,     // received reject after sending initiate
    STATE_SENTREDIRECT,       // sent direct after receiving initiate
    STATE_SENTTERMINATE,      // sent terminate (any time / either side)
    STATE_RECEIVEDTERMINATE,  // received terminate (any time / either side)
    STATE_INPROGRESS,         // session accepted and in progress
    STATE_DEINIT,             // session is being destroyed
  };

  enum Error {
    ERROR_NONE = 0,       // no error
    ERROR_TIME = 1,       // no response to signaling
    ERROR_RESPONSE = 2,   // error during signaling
    ERROR_NETWORK = 3,    // network error, could not allocate network resources
    ERROR_CONTENT = 4,    // channel errors in SetLocalContent/SetRemoteContent
    ERROR_TRANSPORT = 5,  // transport error of some kind
  };

  // Convert State to a readable string.
  static std::string StateToString(State state);

  BaseSession(rtc::Thread* signaling_thread,
              rtc::Thread* worker_thread,
              PortAllocator* port_allocator,
              const std::string& sid,
              const std::string& content_type,
              bool initiator);
  virtual ~BaseSession();

  // These are const to allow them to be called from const methods.
  rtc::Thread* signaling_thread() const { return signaling_thread_; }
  rtc::Thread* worker_thread() const { return worker_thread_; }
  PortAllocator* port_allocator() const { return port_allocator_; }

  // The ID of this session.
  const std::string& id() const { return sid_; }

  // TODO(juberti): This data is largely redundant, as it can now be obtained
  // from local/remote_description(). Remove these functions and members.
  // Returns the XML namespace identifying the type of this session.
  const std::string& content_type() const { return content_type_; }
  // Returns the XML namespace identifying the transport used for this session.
  const std::string& transport_type() const { return transport_type_; }

  // Indicates whether we initiated this session.
  bool initiator() const { return initiator_; }

  // Returns the application-level description given by our client.
  // If we are the recipient, this will be NULL until we send an accept.
  const SessionDescription* local_description() const;

  // Returns the application-level description given by the other client.
  // If we are the initiator, this will be NULL until we receive an accept.
  const SessionDescription* remote_description() const;

  SessionDescription* remote_description();

  // Takes ownership of SessionDescription*
  void set_local_description(const SessionDescription* sdesc);

  // Takes ownership of SessionDescription*
  void set_remote_description(SessionDescription* sdesc);

  const SessionDescription* initiator_description() const;

  // Returns the current state of the session.  See the enum above for details.
  // Each time the state changes, we will fire this signal.
  State state() const { return state_; }
  sigslot::signal2<BaseSession* , State> SignalState;

  // Returns the last error in the session.  See the enum above for details.
  // Each time the an error occurs, we will fire this signal.
  Error error() const { return error_; }
  const std::string& error_desc() const { return error_desc_; }
  sigslot::signal2<BaseSession* , Error> SignalError;

  // Updates the state, signaling if necessary.
  virtual void SetState(State state);

  // Updates the error state, signaling if necessary.
  // TODO(ronghuawu): remove the SetError method that doesn't take |error_desc|.
  virtual void SetError(Error error, const std::string& error_desc);

  // Fired when the remote description is updated, with the updated
  // contents.
  sigslot::signal2<BaseSession* , const ContentInfos&>
      SignalRemoteDescriptionUpdate;

  // Fired when SetState is called (regardless if there's a state change), which
  // indicates the session description might have be updated.
  sigslot::signal2<BaseSession*, ContentAction> SignalNewLocalDescription;

  // Fired when SetState is called (regardless if there's a state change), which
  // indicates the session description might have be updated.
  sigslot::signal2<BaseSession*, ContentAction> SignalNewRemoteDescription;

  // Returns the transport that has been negotiated or NULL if
  // negotiation is still in progress.
  virtual Transport* GetTransport(const std::string& content_name);

  // Creates a new channel with the given names.  This method may be called
  // immediately after creating the session.  However, the actual
  // implementation may not be fixed until transport negotiation completes.
  // This will usually be called from the worker thread, but that
  // shouldn't be an issue since the main thread will be blocked in
  // Send when doing so.
  virtual TransportChannel* CreateChannel(const std::string& content_name,
                                          int component);

  // Returns the channel with the given names.
  virtual TransportChannel* GetChannel(const std::string& content_name,
                                       int component);

  // Destroys the channel with the given names.
  // This will usually be called from the worker thread, but that
  // shouldn't be an issue since the main thread will be blocked in
  // Send when doing so.
  virtual void DestroyChannel(const std::string& content_name,
                              int component);

  rtc::SSLIdentity* identity() { return identity_; }

 protected:
  // Specifies the identity to use in this session.
  bool SetIdentity(rtc::SSLIdentity* identity);

  bool PushdownTransportDescription(ContentSource source,
                                    ContentAction action,
                                    std::string* error_desc);
  void set_initiator(bool initiator) { initiator_ = initiator; }

  const TransportMap& transport_proxies() const { return transports_; }
  // Get a TransportProxy by content_name or transport. NULL if not found.
  TransportProxy* GetTransportProxy(const std::string& content_name);
  void DestroyTransportProxy(const std::string& content_name);
  // TransportProxy is owned by session.  Return proxy just for convenience.
  TransportProxy* GetOrCreateTransportProxy(const std::string& content_name);
  // Creates the actual transport object. Overridable for testing.
  virtual Transport* CreateTransport(const std::string& content_name);

  void OnSignalingReady();
  void SpeculativelyConnectAllTransportChannels();
  // Helper method to provide remote candidates to the transport.
  bool OnRemoteCandidates(const std::string& content_name,
                          const Candidates& candidates,
                          std::string* error);

  // This method will mux transport channels by content_name.
  // First content is used for muxing.
  bool MaybeEnableMuxingSupport();

  // Called when a transport requests signaling.
  virtual void OnTransportRequestSignaling(Transport* transport) {
  }

  // Called when the first channel of a transport begins connecting.  We use
  // this to start a timer, to make sure that the connection completes in a
  // reasonable amount of time.
  virtual void OnTransportConnecting(Transport* transport) {
  }

  // Called when a transport changes its writable state.  We track this to make
  // sure that the transport becomes writable within a reasonable amount of
  // time.  If this does not occur, we signal an error.
  virtual void OnTransportWritable(Transport* transport) {
  }
  virtual void OnTransportReadable(Transport* transport) {
  }

  // Called when a transport has found its steady-state connections.
  virtual void OnTransportCompleted(Transport* transport) {
  }

  // Called when a transport has failed permanently.
  virtual void OnTransportFailed(Transport* transport) {
  }

  // Called when a transport signals that it has new candidates.
  virtual void OnTransportProxyCandidatesReady(TransportProxy* proxy,
                                               const Candidates& candidates) {
  }

  virtual void OnTransportRouteChange(
      Transport* transport,
      int component,
      const cricket::Candidate& remote_candidate) {
  }

  virtual void OnTransportCandidatesAllocationDone(Transport* transport);

  // Called when all transport channels allocated required candidates.
  // This method should be used as an indication of candidates gathering process
  // is completed and application can now send local candidates list to remote.
  virtual void OnCandidatesAllocationDone() {
  }

  // Handles the ice role change callback from Transport. This must be
  // propagated to all the transports.
  virtual void OnRoleConflict();

  // Handles messages posted to us.
  virtual void OnMessage(rtc::Message *pmsg);

 protected:
  bool IsCandidateAllocationDone() const;

  State state_;
  Error error_;
  std::string error_desc_;

  // Fires the new description signal according to the current state.
  virtual void SignalNewDescription();
  // This method will delete the Transport and TransportChannelImpls
  // and replace those with the Transport object of the first
  // MediaContent in bundle_group.
  bool BundleContentGroup(const ContentGroup* bundle_group);

 private:
  // Helper methods to push local and remote transport descriptions.
  bool PushdownLocalTransportDescription(
      const SessionDescription* sdesc, ContentAction action,
      std::string* error_desc);
  bool PushdownRemoteTransportDescription(
      const SessionDescription* sdesc, ContentAction action,
      std::string* error_desc);

  void MaybeCandidateAllocationDone();

  // Log session state.
  void LogState(State old_state, State new_state);

  // Returns true and the TransportInfo of the given |content_name|
  // from |description|. Returns false if it's not available.
  static bool GetTransportDescription(const SessionDescription* description,
                                      const std::string& content_name,
                                      TransportDescription* info);

  // Gets the ContentAction and ContentSource according to the session state.
  bool GetContentAction(ContentAction* action, ContentSource* source);

  rtc::Thread* const signaling_thread_;
  rtc::Thread* const worker_thread_;
  PortAllocator* const port_allocator_;
  const std::string sid_;
  const std::string content_type_;
  const std::string transport_type_;
  bool initiator_;
  rtc::SSLIdentity* identity_;
  rtc::scoped_ptr<const SessionDescription> local_description_;
  rtc::scoped_ptr<SessionDescription> remote_description_;
  uint64 ice_tiebreaker_;
  // This flag will be set to true after the first role switch. This flag
  // will enable us to stop any role switch during the call.
  bool role_switch_;
  TransportMap transports_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_SESSION_H_
