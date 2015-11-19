/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_PORT_H_
#define WEBRTC_P2P_BASE_PORT_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "webrtc/p2p/base/candidate.h"
#include "webrtc/p2p/base/packetsocketfactory.h"
#include "webrtc/p2p/base/portinterface.h"
#include "webrtc/p2p/base/stun.h"
#include "webrtc/p2p/base/stunrequest.h"
#include "webrtc/p2p/base/transport.h"
#include "webrtc/base/asyncpacketsocket.h"
#include "webrtc/base/network.h"
#include "webrtc/base/proxyinfo.h"
#include "webrtc/base/ratetracker.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/socketaddress.h"
#include "webrtc/base/thread.h"

namespace cricket {

class Connection;
class ConnectionRequest;

extern const char LOCAL_PORT_TYPE[];
extern const char STUN_PORT_TYPE[];
extern const char PRFLX_PORT_TYPE[];
extern const char RELAY_PORT_TYPE[];

extern const char UDP_PROTOCOL_NAME[];
extern const char TCP_PROTOCOL_NAME[];
extern const char SSLTCP_PROTOCOL_NAME[];

// RFC 6544, TCP candidate encoding rules.
extern const int DISCARD_PORT;
extern const char TCPTYPE_ACTIVE_STR[];
extern const char TCPTYPE_PASSIVE_STR[];
extern const char TCPTYPE_SIMOPEN_STR[];

// The length of time we wait before timing out readability on a connection.
const uint32 CONNECTION_READ_TIMEOUT = 30 * 1000;   // 30 seconds

// The length of time we wait before timing out writability on a connection.
const uint32 CONNECTION_WRITE_TIMEOUT = 15 * 1000;  // 15 seconds

// The length of time we wait before we become unwritable.
const uint32 CONNECTION_WRITE_CONNECT_TIMEOUT = 5 * 1000;  // 5 seconds

// The number of pings that must fail to respond before we become unwritable.
const uint32 CONNECTION_WRITE_CONNECT_FAILURES = 5;

// This is the length of time that we wait for a ping response to come back.
const int CONNECTION_RESPONSE_TIMEOUT = 5 * 1000;   // 5 seconds

enum RelayType {
  RELAY_GTURN,   // Legacy google relay service.
  RELAY_TURN     // Standard (TURN) relay service.
};

enum IcePriorityValue {
  // The reason we are choosing Relay preference 2 is because, we can run
  // Relay from client to server on UDP/TCP/TLS. To distinguish the transport
  // protocol, we prefer UDP over TCP over TLS.
  // For UDP ICE_TYPE_PREFERENCE_RELAY will be 2.
  // For TCP ICE_TYPE_PREFERENCE_RELAY will be 1.
  // For TLS ICE_TYPE_PREFERENCE_RELAY will be 0.
  // Check turnport.cc for setting these values.
  ICE_TYPE_PREFERENCE_RELAY = 2,
  ICE_TYPE_PREFERENCE_HOST_TCP = 90,
  ICE_TYPE_PREFERENCE_SRFLX = 100,
  ICE_TYPE_PREFERENCE_PRFLX = 110,
  ICE_TYPE_PREFERENCE_HOST = 126
};

const char* ProtoToString(ProtocolType proto);
bool StringToProto(const char* value, ProtocolType* proto);

struct ProtocolAddress {
  rtc::SocketAddress address;
  ProtocolType proto;
  bool secure;

  ProtocolAddress(const rtc::SocketAddress& a, ProtocolType p)
      : address(a), proto(p), secure(false) { }
  ProtocolAddress(const rtc::SocketAddress& a, ProtocolType p, bool sec)
      : address(a), proto(p), secure(sec) { }
};

typedef std::set<rtc::SocketAddress> ServerAddresses;

// Represents a local communication mechanism that can be used to create
// connections to similar mechanisms of the other client.  Subclasses of this
// one add support for specific mechanisms like local UDP ports.
class Port : public PortInterface, public rtc::MessageHandler,
             public sigslot::has_slots<> {
 public:
  Port(rtc::Thread* thread,
       rtc::PacketSocketFactory* factory,
       rtc::Network* network,
       const rtc::IPAddress& ip,
       const std::string& username_fragment,
       const std::string& password);
  Port(rtc::Thread* thread,
       const std::string& type,
       rtc::PacketSocketFactory* factory,
       rtc::Network* network,
       const rtc::IPAddress& ip,
       uint16 min_port,
       uint16 max_port,
       const std::string& username_fragment,
       const std::string& password);
  virtual ~Port();

  virtual const std::string& Type() const { return type_; }
  virtual rtc::Network* Network() const { return network_; }

  // This method will set the flag which enables standard ICE/STUN procedures
  // in STUN connectivity checks. Currently this method does
  // 1. Add / Verify MI attribute in STUN binding requests.
  // 2. Username attribute in STUN binding request will be RFRAF:LFRAG,
  // as opposed to RFRAGLFRAG.
  virtual void SetIceProtocolType(IceProtocolType protocol) {
    ice_protocol_ = protocol;
  }
  virtual IceProtocolType IceProtocol() const { return ice_protocol_; }

  // Methods to set/get ICE role and tiebreaker values.
  IceRole GetIceRole() const { return ice_role_; }
  void SetIceRole(IceRole role) { ice_role_ = role; }

  void SetIceTiebreaker(uint64 tiebreaker) { tiebreaker_ = tiebreaker; }
  uint64 IceTiebreaker() const { return tiebreaker_; }

  virtual bool SharedSocket() const { return shared_socket_; }
  void ResetSharedSocket() { shared_socket_ = false; }

  // The thread on which this port performs its I/O.
  rtc::Thread* thread() { return thread_; }

  // The factory used to create the sockets of this port.
  rtc::PacketSocketFactory* socket_factory() const { return factory_; }
  void set_socket_factory(rtc::PacketSocketFactory* factory) {
    factory_ = factory;
  }

  // For debugging purposes.
  const std::string& content_name() const { return content_name_; }
  void set_content_name(const std::string& content_name) {
    content_name_ = content_name;
  }

  int component() const { return component_; }
  void set_component(int component) { component_ = component; }

  bool send_retransmit_count_attribute() const {
    return send_retransmit_count_attribute_;
  }
  void set_send_retransmit_count_attribute(bool enable) {
    send_retransmit_count_attribute_ = enable;
  }

  // Identifies the generation that this port was created in.
  uint32 generation() { return generation_; }
  void set_generation(uint32 generation) { generation_ = generation; }

  // ICE requires a single username/password per content/media line. So the
  // |ice_username_fragment_| of the ports that belongs to the same content will
  // be the same. However this causes a small complication with our relay
  // server, which expects different username for RTP and RTCP.
  //
  // To resolve this problem, we implemented the username_fragment(),
  // which returns a different username (calculated from
  // |ice_username_fragment_|) for RTCP in the case of ICEPROTO_GOOGLE. And the
  // username_fragment() simply returns |ice_username_fragment_| when running
  // in ICEPROTO_RFC5245.
  //
  // As a result the ICEPROTO_GOOGLE will use different usernames for RTP and
  // RTCP. And the ICEPROTO_RFC5245 will use same username for both RTP and
  // RTCP.
  const std::string username_fragment() const;
  const std::string& password() const { return password_; }

  // Fired when candidates are discovered by the port. When all candidates
  // are discovered that belong to port SignalAddressReady is fired.
  sigslot::signal2<Port*, const Candidate&> SignalCandidateReady;

  // Provides all of the above information in one handy object.
  virtual const std::vector<Candidate>& Candidates() const {
    return candidates_;
  }

  // SignalPortComplete is sent when port completes the task of candidates
  // allocation.
  sigslot::signal1<Port*> SignalPortComplete;
  // This signal sent when port fails to allocate candidates and this port
  // can't be used in establishing the connections. When port is in shared mode
  // and port fails to allocate one of the candidates, port shouldn't send
  // this signal as other candidates might be usefull in establishing the
  // connection.
  sigslot::signal1<Port*> SignalPortError;

  // Returns a map containing all of the connections of this port, keyed by the
  // remote address.
  typedef std::map<rtc::SocketAddress, Connection*> AddressMap;
  const AddressMap& connections() { return connections_; }

  // Returns the connection to the given address or NULL if none exists.
  virtual Connection* GetConnection(
      const rtc::SocketAddress& remote_addr);

  // Called each time a connection is created.
  sigslot::signal2<Port*, Connection*> SignalConnectionCreated;

  // In a shared socket mode each port which shares the socket will decide
  // to accept the packet based on the |remote_addr|. Currently only UDP
  // port implemented this method.
  // TODO(mallinath) - Make it pure virtual.
  virtual bool HandleIncomingPacket(
      rtc::AsyncPacketSocket* socket, const char* data, size_t size,
      const rtc::SocketAddress& remote_addr,
      const rtc::PacketTime& packet_time) {
    ASSERT(false);
    return false;
  }

  // Sends a response message (normal or error) to the given request.  One of
  // these methods should be called as a response to SignalUnknownAddress.
  // NOTE: You MUST call CreateConnection BEFORE SendBindingResponse.
  virtual void SendBindingResponse(StunMessage* request,
                                   const rtc::SocketAddress& addr);
  virtual void SendBindingErrorResponse(
      StunMessage* request, const rtc::SocketAddress& addr,
      int error_code, const std::string& reason);

  void set_proxy(const std::string& user_agent,
                 const rtc::ProxyInfo& proxy) {
    user_agent_ = user_agent;
    proxy_ = proxy;
  }
  const std::string& user_agent() { return user_agent_; }
  const rtc::ProxyInfo& proxy() { return proxy_; }

  virtual void EnablePortPackets();

  // Called if the port has no connections and is no longer useful.
  void Destroy();

  virtual void OnMessage(rtc::Message *pmsg);

  // Debugging description of this port
  virtual std::string ToString() const;
  const rtc::IPAddress& ip() const { return ip_; }
  uint16 min_port() { return min_port_; }
  uint16 max_port() { return max_port_; }

  // Timeout shortening function to speed up unit tests.
  void set_timeout_delay(int delay) { timeout_delay_ = delay; }

  // This method will return local and remote username fragements from the
  // stun username attribute if present.
  bool ParseStunUsername(const StunMessage* stun_msg,
                         std::string* local_username,
                         std::string* remote_username,
                         IceProtocolType* remote_protocol_type) const;
  void CreateStunUsername(const std::string& remote_username,
                          std::string* stun_username_attr_str) const;

  bool MaybeIceRoleConflict(const rtc::SocketAddress& addr,
                            IceMessage* stun_msg,
                            const std::string& remote_ufrag);

  // Called when the socket is currently able to send.
  void OnReadyToSend();

  // Called when the Connection discovers a local peer reflexive candidate.
  // Returns the index of the new local candidate.
  size_t AddPrflxCandidate(const Candidate& local);

  // Returns if RFC 5245 ICE protocol is used.
  bool IsStandardIce() const;

  // Returns if Google ICE protocol is used.
  bool IsGoogleIce() const;

  // Returns if Hybrid ICE protocol is used.
  bool IsHybridIce() const;

  void set_candidate_filter(uint32 candidate_filter) {
    candidate_filter_ = candidate_filter;
  }

 protected:
  enum {
    MSG_CHECKTIMEOUT = 0,
    MSG_FIRST_AVAILABLE
  };

  void set_type(const std::string& type) { type_ = type; }

  void AddAddress(const rtc::SocketAddress& address,
                  const rtc::SocketAddress& base_address,
                  const rtc::SocketAddress& related_address,
                  const std::string& protocol, const std::string& tcptype,
                  const std::string& type, uint32 type_preference,
                  uint32 relay_preference, bool final);

  // Adds the given connection to the list.  (Deleting removes them.)
  void AddConnection(Connection* conn);

  // Called when a packet is received from an unknown address that is not
  // currently a connection.  If this is an authenticated STUN binding request,
  // then we will signal the client.
  void OnReadPacket(const char* data, size_t size,
                    const rtc::SocketAddress& addr,
                    ProtocolType proto);

  // If the given data comprises a complete and correct STUN message then the
  // return value is true, otherwise false. If the message username corresponds
  // with this port's username fragment, msg will contain the parsed STUN
  // message.  Otherwise, the function may send a STUN response internally.
  // remote_username contains the remote fragment of the STUN username.
  bool GetStunMessage(const char* data, size_t size,
                      const rtc::SocketAddress& addr,
                      IceMessage** out_msg, std::string* out_username);

  // Checks if the address in addr is compatible with the port's ip.
  bool IsCompatibleAddress(const rtc::SocketAddress& addr);

  // Returns default DSCP value.
  rtc::DiffServCodePoint DefaultDscpValue() const {
    // No change from what MediaChannel set.
    return rtc::DSCP_NO_CHANGE;
  }

  uint32 candidate_filter() { return candidate_filter_; }

 private:
  void Construct();
  // Called when one of our connections deletes itself.
  void OnConnectionDestroyed(Connection* conn);

  // Checks if this port is useless, and hence, should be destroyed.
  void CheckTimeout();

  rtc::Thread* thread_;
  rtc::PacketSocketFactory* factory_;
  std::string type_;
  bool send_retransmit_count_attribute_;
  rtc::Network* network_;
  rtc::IPAddress ip_;
  uint16 min_port_;
  uint16 max_port_;
  std::string content_name_;
  int component_;
  uint32 generation_;
  // In order to establish a connection to this Port (so that real data can be
  // sent through), the other side must send us a STUN binding request that is
  // authenticated with this username_fragment and password.
  // PortAllocatorSession will provide these username_fragment and password.
  //
  // Note: we should always use username_fragment() instead of using
  // |ice_username_fragment_| directly. For the details see the comment on
  // username_fragment().
  std::string ice_username_fragment_;
  std::string password_;
  std::vector<Candidate> candidates_;
  AddressMap connections_;
  int timeout_delay_;
  bool enable_port_packets_;
  IceProtocolType ice_protocol_;
  IceRole ice_role_;
  uint64 tiebreaker_;
  bool shared_socket_;
  // Information to use when going through a proxy.
  std::string user_agent_;
  rtc::ProxyInfo proxy_;

  // Candidate filter is pushed down to Port such that each Port could
  // make its own decision on how to create candidates. For example,
  // when IceTransportsType is set to relay, both RelayPort and
  // TurnPort will hide raddr to avoid local address leakage.
  uint32 candidate_filter_;

  friend class Connection;
};

// Represents a communication link between a port on the local client and a
// port on the remote client.
class Connection : public rtc::MessageHandler,
    public sigslot::has_slots<> {
 public:
  // States are from RFC 5245. http://tools.ietf.org/html/rfc5245#section-5.7.4
  enum State {
    STATE_WAITING = 0,  // Check has not been performed, Waiting pair on CL.
    STATE_INPROGRESS,   // Check has been sent, transaction is in progress.
    STATE_SUCCEEDED,    // Check already done, produced a successful result.
    STATE_FAILED        // Check for this connection failed.
  };

  virtual ~Connection();

  // The local port where this connection sends and receives packets.
  Port* port() { return port_; }
  const Port* port() const { return port_; }

  // Returns the description of the local port
  virtual const Candidate& local_candidate() const;

  // Returns the description of the remote port to which we communicate.
  const Candidate& remote_candidate() const { return remote_candidate_; }

  // Returns the pair priority.
  uint64 priority() const;

  enum ReadState {
    STATE_READ_INIT    = 0,  // we have yet to receive a ping
    STATE_READABLE     = 1,  // we have received pings recently
    STATE_READ_TIMEOUT = 2,  // we haven't received pings in a while
  };

  ReadState read_state() const { return read_state_; }
  bool readable() const { return read_state_ == STATE_READABLE; }

  enum WriteState {
    STATE_WRITABLE          = 0,  // we have received ping responses recently
    STATE_WRITE_UNRELIABLE  = 1,  // we have had a few ping failures
    STATE_WRITE_INIT        = 2,  // we have yet to receive a ping response
    STATE_WRITE_TIMEOUT     = 3,  // we have had a large number of ping failures
  };

  WriteState write_state() const { return write_state_; }
  bool writable() const { return write_state_ == STATE_WRITABLE; }

  // Determines whether the connection has finished connecting.  This can only
  // be false for TCP connections.
  bool connected() const { return connected_; }

  // Estimate of the round-trip time over this connection.
  uint32 rtt() const { return rtt_; }

  size_t sent_total_bytes();
  size_t sent_bytes_second();
  // Used to track how many packets are discarded in the application socket due
  // to errors.
  size_t sent_discarded_packets();
  size_t sent_total_packets();
  size_t recv_total_bytes();
  size_t recv_bytes_second();
  sigslot::signal1<Connection*> SignalStateChange;

  // Sent when the connection has decided that it is no longer of value.  It
  // will delete itself immediately after this call.
  sigslot::signal1<Connection*> SignalDestroyed;

  // The connection can send and receive packets asynchronously.  This matches
  // the interface of AsyncPacketSocket, which may use UDP or TCP under the
  // covers.
  virtual int Send(const void* data, size_t size,
                   const rtc::PacketOptions& options) = 0;

  // Error if Send() returns < 0
  virtual int GetError() = 0;

  sigslot::signal4<Connection*, const char*, size_t,
                   const rtc::PacketTime&> SignalReadPacket;

  sigslot::signal1<Connection*> SignalReadyToSend;

  // Called when a packet is received on this connection.
  void OnReadPacket(const char* data, size_t size,
                    const rtc::PacketTime& packet_time);

  // Called when the socket is currently able to send.
  void OnReadyToSend();

  // Called when a connection is determined to be no longer useful to us.  We
  // still keep it around in case the other side wants to use it.  But we can
  // safely stop pinging on it and we can allow it to time out if the other
  // side stops using it as well.
  bool pruned() const { return pruned_; }
  void Prune();

  bool use_candidate_attr() const { return use_candidate_attr_; }
  void set_use_candidate_attr(bool enable);

  void set_remote_ice_mode(IceMode mode) {
    remote_ice_mode_ = mode;
  }

  // Makes the connection go away.
  void Destroy();

  // Checks that the state of this connection is up-to-date.  The argument is
  // the current time, which is compared against various timeouts.
  void UpdateState(uint32 now);

  // Called when this connection should try checking writability again.
  uint32 last_ping_sent() const { return last_ping_sent_; }
  void Ping(uint32 now);

  // Called whenever a valid ping is received on this connection.  This is
  // public because the connection intercepts the first ping for us.
  uint32 last_ping_received() const { return last_ping_received_; }
  void ReceivedPing();

  // Debugging description of this connection
  std::string ToDebugId() const;
  std::string ToString() const;
  std::string ToSensitiveString() const;

  bool reported() const { return reported_; }
  void set_reported(bool reported) { reported_ = reported;}

  // This flag will be set if this connection is the chosen one for media
  // transmission. This connection will send STUN ping with USE-CANDIDATE
  // attribute.
  sigslot::signal1<Connection*> SignalUseCandidate;
  // Invoked when Connection receives STUN error response with 487 code.
  void HandleRoleConflictFromPeer();

  State state() const { return state_; }

  IceMode remote_ice_mode() const { return remote_ice_mode_; }

  // Update the ICE password of the remote candidate if |ice_ufrag| matches
  // the candidate's ufrag, and the candidate's passwrod has not been set.
  void MaybeSetRemoteIceCredentials(const std::string& ice_ufrag,
                                    const std::string& ice_pwd);

  // If |remote_candidate_| is peer reflexive and is equivalent to
  // |new_candidate| except the type, update |remote_candidate_| to
  // |new_candidate|.
  void MaybeUpdatePeerReflexiveCandidate(const Candidate& new_candidate);

 protected:
  // Constructs a new connection to the given remote port.
  Connection(Port* port, size_t index, const Candidate& candidate);

  // Called back when StunRequestManager has a stun packet to send
  void OnSendStunPacket(const void* data, size_t size, StunRequest* req);

  // Callbacks from ConnectionRequest
  void OnConnectionRequestResponse(ConnectionRequest* req,
                                   StunMessage* response);
  void OnConnectionRequestErrorResponse(ConnectionRequest* req,
                                        StunMessage* response);
  void OnConnectionRequestTimeout(ConnectionRequest* req);

  // Changes the state and signals if necessary.
  void set_read_state(ReadState value);
  void set_write_state(WriteState value);
  void set_state(State state);
  void set_connected(bool value);

  // Checks if this connection is useless, and hence, should be destroyed.
  void CheckTimeout();

  void OnMessage(rtc::Message *pmsg);

  Port* port_;
  size_t local_candidate_index_;
  Candidate remote_candidate_;
  ReadState read_state_;
  WriteState write_state_;
  bool connected_;
  bool pruned_;
  // By default |use_candidate_attr_| flag will be true,
  // as we will be using agrressive nomination.
  // But when peer is ice-lite, this flag "must" be initialized to false and
  // turn on when connection becomes "best connection".
  bool use_candidate_attr_;
  IceMode remote_ice_mode_;
  StunRequestManager requests_;
  uint32 rtt_;
  uint32 last_ping_sent_;      // last time we sent a ping to the other side
  uint32 last_ping_received_;  // last time we received a ping from the other
                               // side
  uint32 last_data_received_;
  uint32 last_ping_response_received_;
  std::vector<uint32> pings_since_last_response_;

  rtc::RateTracker recv_rate_tracker_;
  rtc::RateTracker send_rate_tracker_;
  uint32 sent_packets_discarded_;
  uint32 sent_packets_total_;

 private:
  void MaybeAddPrflxCandidate(ConnectionRequest* request,
                              StunMessage* response);

  bool reported_;
  State state_;

  friend class Port;
  friend class ConnectionRequest;
};

// ProxyConnection defers all the interesting work to the port
class ProxyConnection : public Connection {
 public:
  ProxyConnection(Port* port, size_t index, const Candidate& candidate);

  virtual int Send(const void* data, size_t size,
                   const rtc::PacketOptions& options);
  virtual int GetError() { return error_; }

 private:
  int error_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_PORT_H_
