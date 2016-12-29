/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_CLIENT_BASICPORTALLOCATOR_H_
#define WEBRTC_P2P_CLIENT_BASICPORTALLOCATOR_H_

#include <string>
#include <vector>

#include "webrtc/p2p/base/portallocator.h"
#include "webrtc/base/messagequeue.h"
#include "webrtc/base/network.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread.h"

namespace cricket {

class BasicPortAllocator : public PortAllocator {
 public:
  BasicPortAllocator(rtc::NetworkManager* network_manager,
                     rtc::PacketSocketFactory* socket_factory);
  explicit BasicPortAllocator(rtc::NetworkManager* network_manager);
  BasicPortAllocator(rtc::NetworkManager* network_manager,
                     rtc::PacketSocketFactory* socket_factory,
                     const ServerAddresses& stun_servers);
  BasicPortAllocator(rtc::NetworkManager* network_manager,
                     const ServerAddresses& stun_servers,
                     const rtc::SocketAddress& relay_server_udp,
                     const rtc::SocketAddress& relay_server_tcp,
                     const rtc::SocketAddress& relay_server_ssl);
  virtual ~BasicPortAllocator();

  void SetIceServers(
      const ServerAddresses& stun_servers,
      const std::vector<RelayServerConfig>& turn_servers) override {
    stun_servers_ = stun_servers;
    turn_servers_ = turn_servers;
  }

  // Set to kDefaultNetworkIgnoreMask by default.
  void SetNetworkIgnoreMask(int network_ignore_mask) override {
    // TODO(phoglund): implement support for other types than loopback.
    // See https://code.google.com/p/webrtc/issues/detail?id=4288.
    // Then remove set_network_ignore_list from NetworkManager.
    network_ignore_mask_ = network_ignore_mask;
  }

  int network_ignore_mask() const { return network_ignore_mask_; }

  rtc::NetworkManager* network_manager() { return network_manager_; }

  // If socket_factory() is set to NULL each PortAllocatorSession
  // creates its own socket factory.
  rtc::PacketSocketFactory* socket_factory() { return socket_factory_; }

  const ServerAddresses& stun_servers() const {
    return stun_servers_;
  }

  const std::vector<RelayServerConfig>& turn_servers() const {
    return turn_servers_;
  }
  virtual void AddTurnServer(const RelayServerConfig& turn_server) {
    turn_servers_.push_back(turn_server);
  }

  PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag,
      const std::string& ice_pwd) override;

 private:
  void Construct();

  rtc::NetworkManager* network_manager_;
  rtc::PacketSocketFactory* socket_factory_;
  ServerAddresses stun_servers_;
  std::vector<RelayServerConfig> turn_servers_;
  bool allow_tcp_listen_;
  int network_ignore_mask_ = rtc::kDefaultNetworkIgnoreMask;
};

struct PortConfiguration;
class AllocationSequence;

class BasicPortAllocatorSession : public PortAllocatorSession,
                                  public rtc::MessageHandler {
 public:
  BasicPortAllocatorSession(BasicPortAllocator* allocator,
                            const std::string& content_name,
                            int component,
                            const std::string& ice_ufrag,
                            const std::string& ice_pwd);
  ~BasicPortAllocatorSession();

  virtual BasicPortAllocator* allocator() { return allocator_; }
  rtc::Thread* network_thread() { return network_thread_; }
  rtc::PacketSocketFactory* socket_factory() { return socket_factory_; }

  void StartGettingPorts() override;
  void StopGettingPorts() override;
  void ClearGettingPorts() override;
  bool IsGettingPorts() override { return running_; }

 protected:
  // Starts the process of getting the port configurations.
  virtual void GetPortConfigurations();

  // Adds a port configuration that is now ready.  Once we have one for each
  // network (or a timeout occurs), we will start allocating ports.
  virtual void ConfigReady(PortConfiguration* config);

  // MessageHandler.  Can be overriden if message IDs do not conflict.
  void OnMessage(rtc::Message* message) override;

 private:
  class PortData {
   public:
    PortData() : port_(NULL), sequence_(NULL), state_(STATE_INIT) {}
    PortData(Port* port, AllocationSequence* seq)
    : port_(port), sequence_(seq), state_(STATE_INIT) {
    }

    Port* port() { return port_; }
    AllocationSequence* sequence() { return sequence_; }
    bool ready() const { return state_ == STATE_READY; }
    bool complete() const {
      // Returns true if candidate allocation has completed one way or another.
      return ((state_ == STATE_COMPLETE) || (state_ == STATE_ERROR));
    }

    void set_ready() { ASSERT(state_ == STATE_INIT); state_ = STATE_READY; }
    void set_complete() {
      state_ = STATE_COMPLETE;
    }
    void set_error() {
      ASSERT(state_ == STATE_INIT || state_ == STATE_READY);
      state_ = STATE_ERROR;
    }

   private:
    enum State {
      STATE_INIT,      // No candidates allocated yet.
      STATE_READY,     // At least one candidate is ready for process.
      STATE_COMPLETE,  // All candidates allocated and ready for process.
      STATE_ERROR      // Error in gathering candidates.
    };
    Port* port_;
    AllocationSequence* sequence_;
    State state_;
  };

  void OnConfigReady(PortConfiguration* config);
  void OnConfigStop();
  void AllocatePorts();
  void OnAllocate();
  void DoAllocate();
  void OnNetworksChanged();
  void OnAllocationSequenceObjectsCreated();
  void DisableEquivalentPhases(rtc::Network* network,
                               PortConfiguration* config,
                               uint32_t* flags);
  void AddAllocatedPort(Port* port, AllocationSequence* seq,
                        bool prepare_address);
  void OnCandidateReady(Port* port, const Candidate& c);
  void OnPortComplete(Port* port);
  void OnPortError(Port* port);
  void OnProtocolEnabled(AllocationSequence* seq, ProtocolType proto);
  void OnPortDestroyed(PortInterface* port);
  void OnShake();
  void MaybeSignalCandidatesAllocationDone();
  void OnPortAllocationComplete(AllocationSequence* seq);
  PortData* FindPort(Port* port);
  void GetNetworks(std::vector<rtc::Network*>* networks);

  bool CheckCandidateFilter(const Candidate& c);

  BasicPortAllocator* allocator_;
  rtc::Thread* network_thread_;
  rtc::scoped_ptr<rtc::PacketSocketFactory> owned_socket_factory_;
  rtc::PacketSocketFactory* socket_factory_;
  bool allocation_started_;
  bool network_manager_started_;
  bool running_;  // set when StartGetAllPorts is called
  bool allocation_sequences_created_;
  std::vector<PortConfiguration*> configs_;
  std::vector<AllocationSequence*> sequences_;
  std::vector<PortData> ports_;

  friend class AllocationSequence;
};

// Records configuration information useful in creating ports.
// TODO(deadbeef): Rename "relay" to "turn_server" in this struct.
struct PortConfiguration : public rtc::MessageData {
  // TODO(jiayl): remove |stun_address| when Chrome is updated.
  rtc::SocketAddress stun_address;
  ServerAddresses stun_servers;
  std::string username;
  std::string password;

  typedef std::vector<RelayServerConfig> RelayList;
  RelayList relays;

  // TODO(jiayl): remove this ctor when Chrome is updated.
  PortConfiguration(const rtc::SocketAddress& stun_address,
                    const std::string& username,
                    const std::string& password);

  PortConfiguration(const ServerAddresses& stun_servers,
                    const std::string& username,
                    const std::string& password);

  // Returns addresses of both the explicitly configured STUN servers,
  // and TURN servers that should be used as STUN servers.
  ServerAddresses StunServers();

  // Adds another relay server, with the given ports and modifier, to the list.
  void AddRelay(const RelayServerConfig& config);

  // Determines whether the given relay server supports the given protocol.
  bool SupportsProtocol(const RelayServerConfig& relay,
                        ProtocolType type) const;
  bool SupportsProtocol(RelayType turn_type, ProtocolType type) const;
  // Helper method returns the server addresses for the matching RelayType and
  // Protocol type.
  ServerAddresses GetRelayServerAddresses(
      RelayType turn_type, ProtocolType type) const;
};

class UDPPort;
class TurnPort;

// Performs the allocation of ports, in a sequenced (timed) manner, for a given
// network and IP address.
class AllocationSequence : public rtc::MessageHandler,
                           public sigslot::has_slots<> {
 public:
  enum State {
    kInit,       // Initial state.
    kRunning,    // Started allocating ports.
    kStopped,    // Stopped from running.
    kCompleted,  // All ports are allocated.

    // kInit --> kRunning --> {kCompleted|kStopped}
  };
  AllocationSequence(BasicPortAllocatorSession* session,
                     rtc::Network* network,
                     PortConfiguration* config,
                     uint32_t flags);
  ~AllocationSequence();
  bool Init();
  void Clear();
  void OnNetworkRemoved();

  State state() const { return state_; }
  const rtc::Network* network() const { return network_; }
  bool network_removed() const { return network_removed_; }

  // Disables the phases for a new sequence that this one already covers for an
  // equivalent network setup.
  void DisableEquivalentPhases(rtc::Network* network,
                               PortConfiguration* config,
                               uint32_t* flags);

  // Starts and stops the sequence.  When started, it will continue allocating
  // new ports on its own timed schedule.
  void Start();
  void Stop();

  // MessageHandler
  void OnMessage(rtc::Message* msg);

  void EnableProtocol(ProtocolType proto);
  bool ProtocolEnabled(ProtocolType proto) const;

  // Signal from AllocationSequence, when it's done with allocating ports.
  // This signal is useful, when port allocation fails which doesn't result
  // in any candidates. Using this signal BasicPortAllocatorSession can send
  // its candidate discovery conclusion signal. Without this signal,
  // BasicPortAllocatorSession doesn't have any event to trigger signal. This
  // can also be achieved by starting timer in BPAS.
  sigslot::signal1<AllocationSequence*> SignalPortAllocationComplete;

 protected:
  // For testing.
  void CreateTurnPort(const RelayServerConfig& config);

 private:
  typedef std::vector<ProtocolType> ProtocolList;

  bool IsFlagSet(uint32_t flag) { return ((flags_ & flag) != 0); }
  void CreateUDPPorts();
  void CreateTCPPorts();
  void CreateStunPorts();
  void CreateRelayPorts();
  void CreateGturnPort(const RelayServerConfig& config);

  void OnReadPacket(rtc::AsyncPacketSocket* socket,
                    const char* data,
                    size_t size,
                    const rtc::SocketAddress& remote_addr,
                    const rtc::PacketTime& packet_time);

  void OnPortDestroyed(PortInterface* port);

  BasicPortAllocatorSession* session_;
  bool network_removed_ = false;
  rtc::Network* network_;
  rtc::IPAddress ip_;
  PortConfiguration* config_;
  State state_;
  uint32_t flags_;
  ProtocolList protocols_;
  rtc::scoped_ptr<rtc::AsyncPacketSocket> udp_socket_;
  // There will be only one udp port per AllocationSequence.
  UDPPort* udp_port_;
  std::vector<TurnPort*> turn_ports_;
  int phase_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_CLIENT_BASICPORTALLOCATOR_H_
