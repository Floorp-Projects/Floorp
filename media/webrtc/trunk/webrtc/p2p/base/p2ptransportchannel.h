/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// P2PTransportChannel wraps up the state management of the connection between
// two P2P clients.  Clients have candidate ports for connecting, and
// connections which are combinations of candidates from each end (Alice and
// Bob each have candidates, one candidate from Alice and one candidate from
// Bob are used to make a connection, repeat to make many connections).
//
// When all of the available connections become invalid (non-writable), we
// kick off a process of determining more candidates and more connections.
//
#ifndef WEBRTC_P2P_BASE_P2PTRANSPORTCHANNEL_H_
#define WEBRTC_P2P_BASE_P2PTRANSPORTCHANNEL_H_

#include <map>
#include <string>
#include <vector>
#include "webrtc/p2p/base/candidate.h"
#include "webrtc/p2p/base/p2ptransport.h"
#include "webrtc/p2p/base/portallocator.h"
#include "webrtc/p2p/base/portinterface.h"
#include "webrtc/p2p/base/transport.h"
#include "webrtc/p2p/base/transportchannelimpl.h"
#include "webrtc/base/asyncpacketsocket.h"
#include "webrtc/base/sigslot.h"

namespace cricket {

// Adds the port on which the candidate originated.
class RemoteCandidate : public Candidate {
 public:
  RemoteCandidate(const Candidate& c, PortInterface* origin_port)
      : Candidate(c), origin_port_(origin_port) {}

  PortInterface* origin_port() { return origin_port_; }

 private:
  PortInterface* origin_port_;
};

// P2PTransportChannel manages the candidates and connection process to keep
// two P2P clients connected to each other.
class P2PTransportChannel : public TransportChannelImpl,
                            public rtc::MessageHandler {
 public:
  P2PTransportChannel(const std::string& content_name,
                      int component,
                      P2PTransport* transport,
                      PortAllocator *allocator);
  virtual ~P2PTransportChannel();

  // From TransportChannelImpl:
  virtual Transport* GetTransport() { return transport_; }
  virtual TransportChannelState GetState() const;
  virtual void SetIceRole(IceRole role);
  virtual IceRole GetIceRole() const { return ice_role_; }
  virtual void SetIceTiebreaker(uint64 tiebreaker);
  virtual bool GetIceProtocolType(IceProtocolType* type) const;
  virtual void SetIceProtocolType(IceProtocolType type);
  virtual void SetIceCredentials(const std::string& ice_ufrag,
                                 const std::string& ice_pwd);
  virtual void SetRemoteIceCredentials(const std::string& ice_ufrag,
                                       const std::string& ice_pwd);
  virtual void SetRemoteIceMode(IceMode mode);
  virtual void Connect();
  virtual void Reset();
  virtual void OnSignalingReady();
  virtual void OnCandidate(const Candidate& candidate);

  // From TransportChannel:
  virtual int SendPacket(const char *data, size_t len,
                         const rtc::PacketOptions& options, int flags);
  virtual int SetOption(rtc::Socket::Option opt, int value);
  virtual bool GetOption(rtc::Socket::Option opt, int* value);
  virtual int GetError() { return error_; }
  virtual bool GetStats(std::vector<ConnectionInfo>* stats);

  const Connection* best_connection() const { return best_connection_; }
  void set_incoming_only(bool value) { incoming_only_ = value; }

  // Note: This is only for testing purpose.
  // |ports_| should not be changed from outside.
  const std::vector<PortInterface *>& ports() { return ports_; }

  IceMode remote_ice_mode() const { return remote_ice_mode_; }

  // DTLS methods.
  virtual bool IsDtlsActive() const { return false; }

  // Default implementation.
  virtual bool GetSslRole(rtc::SSLRole* role) const {
    return false;
  }

  virtual bool SetSslRole(rtc::SSLRole role) {
    return false;
  }

  // Set up the ciphers to use for DTLS-SRTP.
  virtual bool SetSrtpCiphers(const std::vector<std::string>& ciphers) {
    return false;
  }

  // Find out which DTLS-SRTP cipher was negotiated.
  virtual bool GetSrtpCipher(std::string* cipher) {
    return false;
  }

  // Find out which DTLS cipher was negotiated.
  virtual bool GetSslCipher(std::string* cipher) {
    return false;
  }

  // Returns false because the channel is not encrypted by default.
  virtual bool GetLocalIdentity(rtc::SSLIdentity** identity) const {
    return false;
  }

  virtual bool GetRemoteCertificate(rtc::SSLCertificate** cert) const {
    return false;
  }

  // Allows key material to be extracted for external encryption.
  virtual bool ExportKeyingMaterial(
      const std::string& label,
      const uint8* context,
      size_t context_len,
      bool use_context,
      uint8* result,
      size_t result_len) {
    return false;
  }

  virtual bool SetLocalIdentity(rtc::SSLIdentity* identity) {
    return false;
  }

  // Set DTLS Remote fingerprint. Must be after local identity set.
  virtual bool SetRemoteFingerprint(
    const std::string& digest_alg,
    const uint8* digest,
    size_t digest_len) {
    return false;
  }

  // Helper method used only in unittest.
  rtc::DiffServCodePoint DefaultDscpValue() const;

  // Public for unit tests.
  Connection* FindNextPingableConnection();

 private:
  rtc::Thread* thread() { return worker_thread_; }
  PortAllocatorSession* allocator_session() {
    return allocator_sessions_.back();
  }

  void Allocate();
  void UpdateConnectionStates();
  void RequestSort();
  void SortConnections();
  void SwitchBestConnectionTo(Connection* conn);
  void UpdateChannelState();
  void HandleWritable();
  void HandleNotWritable();
  void HandleAllTimedOut();

  Connection* GetBestConnectionOnNetwork(rtc::Network* network) const;
  bool CreateConnections(const Candidate &remote_candidate,
                         PortInterface* origin_port, bool readable);
  bool CreateConnection(PortInterface* port, const Candidate& remote_candidate,
                        PortInterface* origin_port, bool readable);
  bool FindConnection(cricket::Connection* connection) const;

  uint32 GetRemoteCandidateGeneration(const Candidate& candidate);
  bool IsDuplicateRemoteCandidate(const Candidate& candidate);
  void RememberRemoteCandidate(const Candidate& remote_candidate,
                               PortInterface* origin_port);
  bool IsPingable(Connection* conn);
  void PingConnection(Connection* conn);
  void AddAllocatorSession(PortAllocatorSession* session);
  void AddConnection(Connection* connection);

  void OnPortReady(PortAllocatorSession *session, PortInterface* port);
  void OnCandidatesReady(PortAllocatorSession *session,
                         const std::vector<Candidate>& candidates);
  void OnCandidatesAllocationDone(PortAllocatorSession* session);
  void OnUnknownAddress(PortInterface* port,
                        const rtc::SocketAddress& addr,
                        ProtocolType proto,
                        IceMessage* stun_msg,
                        const std::string& remote_username,
                        bool port_muxed);
  void OnPortDestroyed(PortInterface* port);
  void OnRoleConflict(PortInterface* port);

  void OnConnectionStateChange(Connection* connection);
  void OnReadPacket(Connection *connection, const char *data, size_t len,
                    const rtc::PacketTime& packet_time);
  void OnReadyToSend(Connection* connection);
  void OnConnectionDestroyed(Connection *connection);

  void OnUseCandidate(Connection* conn);

  virtual void OnMessage(rtc::Message *pmsg);
  void OnSort();
  void OnPing();

  P2PTransport* transport_;
  PortAllocator *allocator_;
  rtc::Thread *worker_thread_;
  bool incoming_only_;
  bool waiting_for_signaling_;
  int error_;
  std::vector<PortAllocatorSession*> allocator_sessions_;
  std::vector<PortInterface *> ports_;
  std::vector<Connection *> connections_;
  Connection* best_connection_;
  // Connection selected by the controlling agent. This should be used only
  // at controlled side when protocol type is RFC5245.
  Connection* pending_best_connection_;
  std::vector<RemoteCandidate> remote_candidates_;
  bool sort_dirty_;  // indicates whether another sort is needed right now
  bool was_writable_;
  typedef std::map<rtc::Socket::Option, int> OptionMap;
  OptionMap options_;
  std::string ice_ufrag_;
  std::string ice_pwd_;
  std::string remote_ice_ufrag_;
  std::string remote_ice_pwd_;
  IceProtocolType protocol_type_;
  IceMode remote_ice_mode_;
  IceRole ice_role_;
  uint64 tiebreaker_;
  uint32 remote_candidate_generation_;

  DISALLOW_EVIL_CONSTRUCTORS(P2PTransportChannel);
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_P2PTRANSPORTCHANNEL_H_
