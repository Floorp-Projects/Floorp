/*
 *  Copyright 2004 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/p2p/client/basicportallocator.h"

#include <string>
#include <vector>

#include "webrtc/p2p/base/basicpacketsocketfactory.h"
#include "webrtc/p2p/base/common.h"
#include "webrtc/p2p/base/port.h"
#include "webrtc/p2p/base/relayport.h"
#include "webrtc/p2p/base/stunport.h"
#include "webrtc/p2p/base/tcpport.h"
#include "webrtc/p2p/base/turnport.h"
#include "webrtc/p2p/base/udpport.h"
#include "webrtc/base/common.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"

using rtc::CreateRandomId;
using rtc::CreateRandomString;

namespace {

enum {
  MSG_CONFIG_START,
  MSG_CONFIG_READY,
  MSG_ALLOCATE,
  MSG_ALLOCATION_PHASE,
  MSG_SHAKE,
  MSG_SEQUENCEOBJECTS_CREATED,
  MSG_CONFIG_STOP,
};

const int PHASE_UDP = 0;
const int PHASE_RELAY = 1;
const int PHASE_TCP = 2;
const int PHASE_SSLTCP = 3;

const int kNumPhases = 4;

const int SHAKE_MIN_DELAY = 45 * 1000;  // 45 seconds
const int SHAKE_MAX_DELAY = 90 * 1000;  // 90 seconds

int ShakeDelay() {
  int range = SHAKE_MAX_DELAY - SHAKE_MIN_DELAY + 1;
  return SHAKE_MIN_DELAY + CreateRandomId() % range;
}

}  // namespace

namespace cricket {

const uint32 DISABLE_ALL_PHASES =
  PORTALLOCATOR_DISABLE_UDP
  | PORTALLOCATOR_DISABLE_TCP
  | PORTALLOCATOR_DISABLE_STUN
  | PORTALLOCATOR_DISABLE_RELAY;

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
                     uint32 flags);
  ~AllocationSequence();
  bool Init();
  void Clear();

  State state() const { return state_; }

  // Disables the phases for a new sequence that this one already covers for an
  // equivalent network setup.
  void DisableEquivalentPhases(rtc::Network* network,
      PortConfiguration* config, uint32* flags);

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

 private:
  typedef std::vector<ProtocolType> ProtocolList;

  bool IsFlagSet(uint32 flag) {
    return ((flags_ & flag) != 0);
  }
  void CreateUDPPorts();
  void CreateTCPPorts();
  void CreateStunPorts();
  void CreateRelayPorts();
  void CreateGturnPort(const RelayServerConfig& config);
  void CreateTurnPort(const RelayServerConfig& config);

  void OnReadPacket(rtc::AsyncPacketSocket* socket,
                    const char* data, size_t size,
                    const rtc::SocketAddress& remote_addr,
                    const rtc::PacketTime& packet_time);

  void OnPortDestroyed(PortInterface* port);

  BasicPortAllocatorSession* session_;
  rtc::Network* network_;
  rtc::IPAddress ip_;
  PortConfiguration* config_;
  State state_;
  uint32 flags_;
  ProtocolList protocols_;
  rtc::scoped_ptr<rtc::AsyncPacketSocket> udp_socket_;
  // There will be only one udp port per AllocationSequence.
  UDPPort* udp_port_;
  std::vector<TurnPort*> turn_ports_;
  int phase_;
};

// BasicPortAllocator
BasicPortAllocator::BasicPortAllocator(
    rtc::NetworkManager* network_manager,
    rtc::PacketSocketFactory* socket_factory)
    : network_manager_(network_manager),
      socket_factory_(socket_factory) {
  ASSERT(socket_factory_ != NULL);
  Construct();
}

BasicPortAllocator::BasicPortAllocator(
    rtc::NetworkManager* network_manager)
    : network_manager_(network_manager),
      socket_factory_(NULL) {
  Construct();
}

BasicPortAllocator::BasicPortAllocator(
    rtc::NetworkManager* network_manager,
    rtc::PacketSocketFactory* socket_factory,
    const ServerAddresses& stun_servers)
    : network_manager_(network_manager),
      socket_factory_(socket_factory),
      stun_servers_(stun_servers) {
  ASSERT(socket_factory_ != NULL);
  Construct();
}

BasicPortAllocator::BasicPortAllocator(
    rtc::NetworkManager* network_manager,
    const ServerAddresses& stun_servers,
    const rtc::SocketAddress& relay_address_udp,
    const rtc::SocketAddress& relay_address_tcp,
    const rtc::SocketAddress& relay_address_ssl)
    : network_manager_(network_manager),
      socket_factory_(NULL),
      stun_servers_(stun_servers) {

  RelayServerConfig config(RELAY_GTURN);
  if (!relay_address_udp.IsNil())
    config.ports.push_back(ProtocolAddress(relay_address_udp, PROTO_UDP));
  if (!relay_address_tcp.IsNil())
    config.ports.push_back(ProtocolAddress(relay_address_tcp, PROTO_TCP));
  if (!relay_address_ssl.IsNil())
    config.ports.push_back(ProtocolAddress(relay_address_ssl, PROTO_SSLTCP));

  if (!config.ports.empty())
    AddRelay(config);

  Construct();
}

void BasicPortAllocator::Construct() {
  allow_tcp_listen_ = true;
}

BasicPortAllocator::~BasicPortAllocator() {
}

PortAllocatorSession *BasicPortAllocator::CreateSessionInternal(
    const std::string& content_name, int component,
    const std::string& ice_ufrag, const std::string& ice_pwd) {
  return new BasicPortAllocatorSession(
      this, content_name, component, ice_ufrag, ice_pwd);
}


// BasicPortAllocatorSession
BasicPortAllocatorSession::BasicPortAllocatorSession(
    BasicPortAllocator *allocator,
    const std::string& content_name,
    int component,
    const std::string& ice_ufrag,
    const std::string& ice_pwd)
    : PortAllocatorSession(content_name, component,
                           ice_ufrag, ice_pwd, allocator->flags()),
      allocator_(allocator), network_thread_(NULL),
      socket_factory_(allocator->socket_factory()),
      allocation_started_(false),
      network_manager_started_(false),
      running_(false),
      allocation_sequences_created_(false) {
  allocator_->network_manager()->SignalNetworksChanged.connect(
      this, &BasicPortAllocatorSession::OnNetworksChanged);
  allocator_->network_manager()->StartUpdating();
}

BasicPortAllocatorSession::~BasicPortAllocatorSession() {
  allocator_->network_manager()->StopUpdating();
  if (network_thread_ != NULL)
    network_thread_->Clear(this);

  for (uint32 i = 0; i < sequences_.size(); ++i) {
    // AllocationSequence should clear it's map entry for turn ports before
    // ports are destroyed.
    sequences_[i]->Clear();
  }

  std::vector<PortData>::iterator it;
  for (it = ports_.begin(); it != ports_.end(); it++)
    delete it->port();

  for (uint32 i = 0; i < configs_.size(); ++i)
    delete configs_[i];

  for (uint32 i = 0; i < sequences_.size(); ++i)
    delete sequences_[i];
}

void BasicPortAllocatorSession::StartGettingPorts() {
  network_thread_ = rtc::Thread::Current();
  if (!socket_factory_) {
    owned_socket_factory_.reset(
        new rtc::BasicPacketSocketFactory(network_thread_));
    socket_factory_ = owned_socket_factory_.get();
  }

  running_ = true;
  network_thread_->Post(this, MSG_CONFIG_START);

  if (flags() & PORTALLOCATOR_ENABLE_SHAKER)
    network_thread_->PostDelayed(ShakeDelay(), this, MSG_SHAKE);
}

void BasicPortAllocatorSession::StopGettingPorts() {
  ASSERT(rtc::Thread::Current() == network_thread_);
  running_ = false;
  network_thread_->Clear(this, MSG_ALLOCATE);
  for (uint32 i = 0; i < sequences_.size(); ++i)
    sequences_[i]->Stop();
  network_thread_->Post(this, MSG_CONFIG_STOP);
}

void BasicPortAllocatorSession::OnMessage(rtc::Message *message) {
  switch (message->message_id) {
  case MSG_CONFIG_START:
    ASSERT(rtc::Thread::Current() == network_thread_);
    GetPortConfigurations();
    break;

  case MSG_CONFIG_READY:
    ASSERT(rtc::Thread::Current() == network_thread_);
    OnConfigReady(static_cast<PortConfiguration*>(message->pdata));
    break;

  case MSG_ALLOCATE:
    ASSERT(rtc::Thread::Current() == network_thread_);
    OnAllocate();
    break;

  case MSG_SHAKE:
    ASSERT(rtc::Thread::Current() == network_thread_);
    OnShake();
    break;
  case MSG_SEQUENCEOBJECTS_CREATED:
    ASSERT(rtc::Thread::Current() == network_thread_);
    OnAllocationSequenceObjectsCreated();
    break;
  case MSG_CONFIG_STOP:
    ASSERT(rtc::Thread::Current() == network_thread_);
    OnConfigStop();
    break;
  default:
    ASSERT(false);
  }
}

void BasicPortAllocatorSession::GetPortConfigurations() {
  PortConfiguration* config = new PortConfiguration(allocator_->stun_servers(),
                                                    username(),
                                                    password());

  for (size_t i = 0; i < allocator_->relays().size(); ++i) {
    config->AddRelay(allocator_->relays()[i]);
  }
  ConfigReady(config);
}

void BasicPortAllocatorSession::ConfigReady(PortConfiguration* config) {
  network_thread_->Post(this, MSG_CONFIG_READY, config);
}

// Adds a configuration to the list.
void BasicPortAllocatorSession::OnConfigReady(PortConfiguration* config) {
  if (config)
    configs_.push_back(config);

  AllocatePorts();
}

void BasicPortAllocatorSession::OnConfigStop() {
  ASSERT(rtc::Thread::Current() == network_thread_);

  // If any of the allocated ports have not completed the candidates allocation,
  // mark those as error. Since session doesn't need any new candidates
  // at this stage of the allocation, it's safe to discard any new candidates.
  bool send_signal = false;
  for (std::vector<PortData>::iterator it = ports_.begin();
       it != ports_.end(); ++it) {
    if (!it->complete()) {
      // Updating port state to error, which didn't finish allocating candidates
      // yet.
      it->set_error();
      send_signal = true;
    }
  }

  // Did we stop any running sequences?
  for (std::vector<AllocationSequence*>::iterator it = sequences_.begin();
       it != sequences_.end() && !send_signal; ++it) {
    if ((*it)->state() == AllocationSequence::kStopped) {
      send_signal = true;
    }
  }

  // If we stopped anything that was running, send a done signal now.
  if (send_signal) {
    MaybeSignalCandidatesAllocationDone();
  }
}

void BasicPortAllocatorSession::AllocatePorts() {
  ASSERT(rtc::Thread::Current() == network_thread_);
  network_thread_->Post(this, MSG_ALLOCATE);
}

void BasicPortAllocatorSession::OnAllocate() {
  if (network_manager_started_)
    DoAllocate();

  allocation_started_ = true;
}

// For each network, see if we have a sequence that covers it already.  If not,
// create a new sequence to create the appropriate ports.
void BasicPortAllocatorSession::DoAllocate() {
  bool done_signal_needed = false;
  std::vector<rtc::Network*> networks;

  // If the adapter enumeration is disabled, we'll just bind to any address
  // instead of specific NIC. This is to ensure the same routing for http
  // traffic by OS is also used here to avoid any local or public IP leakage
  // during stun process.
  if (flags() & PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION) {
    allocator_->network_manager()->GetAnyAddressNetworks(&networks);
  } else {
    allocator_->network_manager()->GetNetworks(&networks);
  }
  if (networks.empty()) {
    LOG(LS_WARNING) << "Machine has no networks; no ports will be allocated";
    done_signal_needed = true;
  } else {
    for (uint32 i = 0; i < networks.size(); ++i) {
      PortConfiguration* config = NULL;
      if (configs_.size() > 0)
        config = configs_.back();

      uint32 sequence_flags = flags();
      if ((sequence_flags & DISABLE_ALL_PHASES) == DISABLE_ALL_PHASES) {
        // If all the ports are disabled we should just fire the allocation
        // done event and return.
        done_signal_needed = true;
        break;
      }

      // Disables phases that are not specified in this config.
      if (!config || config->StunServers().empty()) {
        // No STUN ports specified in this config.
        sequence_flags |= PORTALLOCATOR_DISABLE_STUN;
      }
      if (!config || config->relays.empty()) {
        // No relay ports specified in this config.
        sequence_flags |= PORTALLOCATOR_DISABLE_RELAY;
      }

      if (!(sequence_flags & PORTALLOCATOR_ENABLE_IPV6) &&
          networks[i]->GetBestIP().family() == AF_INET6) {
        // Skip IPv6 networks unless the flag's been set.
        continue;
      }

      // Disable phases that would only create ports equivalent to
      // ones that we have already made.
      DisableEquivalentPhases(networks[i], config, &sequence_flags);

      if ((sequence_flags & DISABLE_ALL_PHASES) == DISABLE_ALL_PHASES) {
        // New AllocationSequence would have nothing to do, so don't make it.
        continue;
      }

      AllocationSequence* sequence =
          new AllocationSequence(this, networks[i], config, sequence_flags);
      if (!sequence->Init()) {
        delete sequence;
        continue;
      }
      done_signal_needed = true;
      sequence->SignalPortAllocationComplete.connect(
          this, &BasicPortAllocatorSession::OnPortAllocationComplete);
      if (running_)
        sequence->Start();
      sequences_.push_back(sequence);
    }
  }
  if (done_signal_needed) {
    network_thread_->Post(this, MSG_SEQUENCEOBJECTS_CREATED);
  }
}

void BasicPortAllocatorSession::OnNetworksChanged() {
  network_manager_started_ = true;
  if (allocation_started_)
    DoAllocate();
}

void BasicPortAllocatorSession::DisableEquivalentPhases(
    rtc::Network* network, PortConfiguration* config, uint32* flags) {
  for (uint32 i = 0; i < sequences_.size() &&
      (*flags & DISABLE_ALL_PHASES) != DISABLE_ALL_PHASES; ++i) {
    sequences_[i]->DisableEquivalentPhases(network, config, flags);
  }
}

void BasicPortAllocatorSession::AddAllocatedPort(Port* port,
                                                 AllocationSequence * seq,
                                                 bool prepare_address) {
  if (!port)
    return;

  LOG(LS_INFO) << "Adding allocated port for " << content_name();
  port->set_content_name(content_name());
  port->set_component(component_);
  port->set_generation(generation());
  if (allocator_->proxy().type != rtc::PROXY_NONE)
    port->set_proxy(allocator_->user_agent(), allocator_->proxy());
  port->set_send_retransmit_count_attribute((allocator_->flags() &
      PORTALLOCATOR_ENABLE_STUN_RETRANSMIT_ATTRIBUTE) != 0);

  // Push down the candidate_filter to individual port.
  uint32 candidate_filter = allocator_->candidate_filter();

  // When adapter enumeration is disabled, disable CF_HOST at port level so
  // local address is not leaked by stunport in the candidate's related address.
  if (flags() & PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION) {
    candidate_filter &= ~CF_HOST;
  }
  port->set_candidate_filter(candidate_filter);

  PortData data(port, seq);
  ports_.push_back(data);

  port->SignalCandidateReady.connect(
      this, &BasicPortAllocatorSession::OnCandidateReady);
  port->SignalPortComplete.connect(this,
      &BasicPortAllocatorSession::OnPortComplete);
  port->SignalDestroyed.connect(this,
      &BasicPortAllocatorSession::OnPortDestroyed);
  port->SignalPortError.connect(
      this, &BasicPortAllocatorSession::OnPortError);
  LOG_J(LS_INFO, port) << "Added port to allocator";

  if (prepare_address)
    port->PrepareAddress();
}

void BasicPortAllocatorSession::OnAllocationSequenceObjectsCreated() {
  allocation_sequences_created_ = true;
  // Send candidate allocation complete signal if we have no sequences.
  MaybeSignalCandidatesAllocationDone();
}

void BasicPortAllocatorSession::OnCandidateReady(
    Port* port, const Candidate& c) {
  ASSERT(rtc::Thread::Current() == network_thread_);
  PortData* data = FindPort(port);
  ASSERT(data != NULL);
  // Discarding any candidate signal if port allocation status is
  // already in completed state.
  if (data->complete())
    return;

  // Send candidates whose protocol is enabled.
  std::vector<Candidate> candidates;
  ProtocolType pvalue;
  bool candidate_allowed_to_send = CheckCandidateFilter(c);
  if (StringToProto(c.protocol().c_str(), &pvalue) &&
      data->sequence()->ProtocolEnabled(pvalue) &&
      candidate_allowed_to_send) {
    candidates.push_back(c);
  }

  if (!candidates.empty()) {
    SignalCandidatesReady(this, candidates);
  }

  // Moving to READY state as we have atleast one candidate from the port.
  // Since this port has atleast one candidate we should forward this port
  // to listners, to allow connections from this port.
  // Also we should make sure that candidate gathered from this port is allowed
  // to send outside.
  if (!data->ready() && candidate_allowed_to_send) {
    data->set_ready();
    SignalPortReady(this, port);
  }
}

void BasicPortAllocatorSession::OnPortComplete(Port* port) {
  ASSERT(rtc::Thread::Current() == network_thread_);
  PortData* data = FindPort(port);
  ASSERT(data != NULL);

  // Ignore any late signals.
  if (data->complete())
    return;

  // Moving to COMPLETE state.
  data->set_complete();
  // Send candidate allocation complete signal if this was the last port.
  MaybeSignalCandidatesAllocationDone();
}

void BasicPortAllocatorSession::OnPortError(Port* port) {
  ASSERT(rtc::Thread::Current() == network_thread_);
  PortData* data = FindPort(port);
  ASSERT(data != NULL);
  // We might have already given up on this port and stopped it.
  if (data->complete())
    return;

  // SignalAddressError is currently sent from StunPort/TurnPort.
  // But this signal itself is generic.
  data->set_error();
  // Send candidate allocation complete signal if this was the last port.
  MaybeSignalCandidatesAllocationDone();
}

void BasicPortAllocatorSession::OnProtocolEnabled(AllocationSequence* seq,
                                                  ProtocolType proto) {
  std::vector<Candidate> candidates;
  for (std::vector<PortData>::iterator it = ports_.begin();
       it != ports_.end(); ++it) {
    if (it->sequence() != seq)
      continue;

    const std::vector<Candidate>& potentials = it->port()->Candidates();
    for (size_t i = 0; i < potentials.size(); ++i) {
      if (!CheckCandidateFilter(potentials[i]))
        continue;
      ProtocolType pvalue;
      if (!StringToProto(potentials[i].protocol().c_str(), &pvalue))
        continue;
      if (pvalue == proto) {
        candidates.push_back(potentials[i]);
      }
    }
  }

  if (!candidates.empty()) {
    SignalCandidatesReady(this, candidates);
  }
}

bool BasicPortAllocatorSession::CheckCandidateFilter(const Candidate& c) {
  uint32 filter = allocator_->candidate_filter();

  // When binding to any address, before sending packets out, the getsockname
  // returns all 0s, but after sending packets, it'll be the NIC used to
  // send. All 0s is not a valid ICE candidate address and should be filtered
  // out.
  if (c.address().IsAnyIP()) {
    return false;
  }

  if (c.type() == RELAY_PORT_TYPE) {
    return ((filter & CF_RELAY) != 0);
  } else if (c.type() == STUN_PORT_TYPE) {
    return ((filter & CF_REFLEXIVE) != 0);
  } else if (c.type() == LOCAL_PORT_TYPE) {
    if ((filter & CF_REFLEXIVE) && !c.address().IsPrivateIP()) {
      // We allow host candidates if the filter allows server-reflexive
      // candidates and the candidate is a public IP. Because we don't generate
      // server-reflexive candidates if they have the same IP as the host
      // candidate (i.e. when the host candidate is a public IP), filtering to
      // only server-reflexive candidates won't work right when the host
      // candidates have public IPs.
      return true;
    }

    // This is just to prevent the case when binding to any address (all 0s), if
    // somehow the host candidate address is not all 0s. Either because local
    // installed proxy changes the address or a packet has been sent for any
    // reason before getsockname is called.
    if (flags() & PORTALLOCATOR_DISABLE_ADAPTER_ENUMERATION) {
      LOG(LS_WARNING) << "Received non-0 host address: "
                      << c.address().ToString()
                      << " when adapter enumeration is disabled";
      return false;
    }

    return ((filter & CF_HOST) != 0);
  }
  return false;
}

void BasicPortAllocatorSession::OnPortAllocationComplete(
    AllocationSequence* seq) {
  // Send candidate allocation complete signal if all ports are done.
  MaybeSignalCandidatesAllocationDone();
}

void BasicPortAllocatorSession::MaybeSignalCandidatesAllocationDone() {
  // Send signal only if all required AllocationSequence objects
  // are created.
  if (!allocation_sequences_created_)
    return;

  // Check that all port allocation sequences are complete.
  for (std::vector<AllocationSequence*>::iterator it = sequences_.begin();
       it != sequences_.end(); ++it) {
    if ((*it)->state() == AllocationSequence::kRunning)
      return;
  }

  // If all allocated ports are in complete state, session must have got all
  // expected candidates. Session will trigger candidates allocation complete
  // signal.
  for (std::vector<PortData>::iterator it = ports_.begin();
       it != ports_.end(); ++it) {
    if (!it->complete())
      return;
  }
  LOG(LS_INFO) << "All candidates gathered for " << content_name_ << ":"
               << component_ << ":" << generation();
  SignalCandidatesAllocationDone(this);
}

void BasicPortAllocatorSession::OnPortDestroyed(
    PortInterface* port) {
  ASSERT(rtc::Thread::Current() == network_thread_);
  for (std::vector<PortData>::iterator iter = ports_.begin();
       iter != ports_.end(); ++iter) {
    if (port == iter->port()) {
      ports_.erase(iter);
      LOG_J(LS_INFO, port) << "Removed port from allocator ("
                           << static_cast<int>(ports_.size()) << " remaining)";
      return;
    }
  }
  ASSERT(false);
}

void BasicPortAllocatorSession::OnShake() {
  LOG(INFO) << ">>>>> SHAKE <<<<< >>>>> SHAKE <<<<< >>>>> SHAKE <<<<<";

  std::vector<Port*> ports;
  std::vector<Connection*> connections;

  for (size_t i = 0; i < ports_.size(); ++i) {
    if (ports_[i].ready())
      ports.push_back(ports_[i].port());
  }

  for (size_t i = 0; i < ports.size(); ++i) {
    Port::AddressMap::const_iterator iter;
    for (iter = ports[i]->connections().begin();
         iter != ports[i]->connections().end();
         ++iter) {
      connections.push_back(iter->second);
    }
  }

  LOG(INFO) << ">>>>> Destroying " << ports.size() << " ports and "
            << connections.size() << " connections";

  for (size_t i = 0; i < connections.size(); ++i)
    connections[i]->Destroy();

  if (running_ || (ports.size() > 0) || (connections.size() > 0))
    network_thread_->PostDelayed(ShakeDelay(), this, MSG_SHAKE);
}

BasicPortAllocatorSession::PortData* BasicPortAllocatorSession::FindPort(
    Port* port) {
  for (std::vector<PortData>::iterator it = ports_.begin();
       it != ports_.end(); ++it) {
    if (it->port() == port) {
      return &*it;
    }
  }
  return NULL;
}

// AllocationSequence

AllocationSequence::AllocationSequence(BasicPortAllocatorSession* session,
                                       rtc::Network* network,
                                       PortConfiguration* config,
                                       uint32 flags)
    : session_(session),
      network_(network),
      ip_(network->GetBestIP()),
      config_(config),
      state_(kInit),
      flags_(flags),
      udp_socket_(),
      udp_port_(NULL),
      phase_(0) {
}

bool AllocationSequence::Init() {
  if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET) &&
      !IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_UFRAG)) {
    LOG(LS_ERROR) << "Shared socket option can't be set without "
                  << "shared ufrag.";
    ASSERT(false);
    return false;
  }

  if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET)) {
    udp_socket_.reset(session_->socket_factory()->CreateUdpSocket(
        rtc::SocketAddress(ip_, 0), session_->allocator()->min_port(),
        session_->allocator()->max_port()));
    if (udp_socket_) {
      udp_socket_->SignalReadPacket.connect(
          this, &AllocationSequence::OnReadPacket);
    }
    // Continuing if |udp_socket_| is NULL, as local TCP and RelayPort using TCP
    // are next available options to setup a communication channel.
  }
  return true;
}

void AllocationSequence::Clear() {
  udp_port_ = NULL;
  turn_ports_.clear();
}

AllocationSequence::~AllocationSequence() {
  session_->network_thread()->Clear(this);
}

void AllocationSequence::DisableEquivalentPhases(rtc::Network* network,
    PortConfiguration* config, uint32* flags) {
  if (!((network == network_) && (ip_ == network->GetBestIP()))) {
    // Different network setup; nothing is equivalent.
    return;
  }

  // Else turn off the stuff that we've already got covered.

  // Every config implicitly specifies local, so turn that off right away.
  *flags |= PORTALLOCATOR_DISABLE_UDP;
  *flags |= PORTALLOCATOR_DISABLE_TCP;

  if (config_ && config) {
    if (config_->StunServers() == config->StunServers()) {
      // Already got this STUN servers covered.
      *flags |= PORTALLOCATOR_DISABLE_STUN;
    }
    if (!config_->relays.empty()) {
      // Already got relays covered.
      // NOTE: This will even skip a _different_ set of relay servers if we
      // were to be given one, but that never happens in our codebase. Should
      // probably get rid of the list in PortConfiguration and just keep a
      // single relay server in each one.
      *flags |= PORTALLOCATOR_DISABLE_RELAY;
    }
  }
}

void AllocationSequence::Start() {
  state_ = kRunning;
  session_->network_thread()->Post(this, MSG_ALLOCATION_PHASE);
}

void AllocationSequence::Stop() {
  // If the port is completed, don't set it to stopped.
  if (state_ == kRunning) {
    state_ = kStopped;
    session_->network_thread()->Clear(this, MSG_ALLOCATION_PHASE);
  }
}

void AllocationSequence::OnMessage(rtc::Message* msg) {
  ASSERT(rtc::Thread::Current() == session_->network_thread());
  ASSERT(msg->message_id == MSG_ALLOCATION_PHASE);

  const char* const PHASE_NAMES[kNumPhases] = {
    "Udp", "Relay", "Tcp", "SslTcp"
  };

  // Perform all of the phases in the current step.
  LOG_J(LS_INFO, network_) << "Allocation Phase="
                           << PHASE_NAMES[phase_];

  switch (phase_) {
    case PHASE_UDP:
      CreateUDPPorts();
      CreateStunPorts();
      EnableProtocol(PROTO_UDP);
      break;

    case PHASE_RELAY:
      CreateRelayPorts();
      break;

    case PHASE_TCP:
      CreateTCPPorts();
      EnableProtocol(PROTO_TCP);
      break;

    case PHASE_SSLTCP:
      state_ = kCompleted;
      EnableProtocol(PROTO_SSLTCP);
      break;

    default:
      ASSERT(false);
  }

  if (state() == kRunning) {
    ++phase_;
    session_->network_thread()->PostDelayed(
        session_->allocator()->step_delay(),
        this, MSG_ALLOCATION_PHASE);
  } else {
    // If all phases in AllocationSequence are completed, no allocation
    // steps needed further. Canceling  pending signal.
    session_->network_thread()->Clear(this, MSG_ALLOCATION_PHASE);
    SignalPortAllocationComplete(this);
  }
}

void AllocationSequence::EnableProtocol(ProtocolType proto) {
  if (!ProtocolEnabled(proto)) {
    protocols_.push_back(proto);
    session_->OnProtocolEnabled(this, proto);
  }
}

bool AllocationSequence::ProtocolEnabled(ProtocolType proto) const {
  for (ProtocolList::const_iterator it = protocols_.begin();
       it != protocols_.end(); ++it) {
    if (*it == proto)
      return true;
  }
  return false;
}

void AllocationSequence::CreateUDPPorts() {
  if (IsFlagSet(PORTALLOCATOR_DISABLE_UDP)) {
    LOG(LS_VERBOSE) << "AllocationSequence: UDP ports disabled, skipping.";
    return;
  }

  // TODO(mallinath) - Remove UDPPort creating socket after shared socket
  // is enabled completely.
  UDPPort* port = NULL;
  if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET) && udp_socket_) {
    port = UDPPort::Create(session_->network_thread(),
                           session_->socket_factory(), network_,
                           udp_socket_.get(),
                           session_->username(), session_->password(),
                           session_->allocator()->origin());
  } else {
    port = UDPPort::Create(session_->network_thread(),
                           session_->socket_factory(),
                           network_, ip_,
                           session_->allocator()->min_port(),
                           session_->allocator()->max_port(),
                           session_->username(), session_->password(),
                           session_->allocator()->origin());
  }

  if (port) {
    // If shared socket is enabled, STUN candidate will be allocated by the
    // UDPPort.
    if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET)) {
      udp_port_ = port;
      port->SignalDestroyed.connect(this, &AllocationSequence::OnPortDestroyed);

      // If STUN is not disabled, setting stun server address to port.
      if (!IsFlagSet(PORTALLOCATOR_DISABLE_STUN)) {
        // If config has stun_servers, use it to get server reflexive candidate
        // otherwise use first TURN server which supports UDP.
        if (config_ && !config_->StunServers().empty()) {
          LOG(LS_INFO) << "AllocationSequence: UDPPort will be handling the "
                       <<  "STUN candidate generation.";
          port->set_server_addresses(config_->StunServers());
        } else if (config_ &&
                   config_->SupportsProtocol(RELAY_TURN, PROTO_UDP)) {
          port->set_server_addresses(config_->GetRelayServerAddresses(
              RELAY_TURN, PROTO_UDP));
          LOG(LS_INFO) << "AllocationSequence: TURN Server address will be "
                       << " used for generating STUN candidate.";
        }
      }
    }

    session_->AddAllocatedPort(port, this, true);
  }
}

void AllocationSequence::CreateTCPPorts() {
  if (IsFlagSet(PORTALLOCATOR_DISABLE_TCP)) {
    LOG(LS_VERBOSE) << "AllocationSequence: TCP ports disabled, skipping.";
    return;
  }

  Port* port = TCPPort::Create(session_->network_thread(),
                               session_->socket_factory(),
                               network_, ip_,
                               session_->allocator()->min_port(),
                               session_->allocator()->max_port(),
                               session_->username(), session_->password(),
                               session_->allocator()->allow_tcp_listen());
  if (port) {
    session_->AddAllocatedPort(port, this, true);
    // Since TCPPort is not created using shared socket, |port| will not be
    // added to the dequeue.
  }
}

void AllocationSequence::CreateStunPorts() {
  if (IsFlagSet(PORTALLOCATOR_DISABLE_STUN)) {
    LOG(LS_VERBOSE) << "AllocationSequence: STUN ports disabled, skipping.";
    return;
  }

  if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET)) {
    return;
  }

  // If BasicPortAllocatorSession::OnAllocate left STUN ports enabled then we
  // ought to have an address for them here.
  ASSERT(config_ && !config_->StunServers().empty());
  if (!(config_ && !config_->StunServers().empty())) {
    LOG(LS_WARNING)
        << "AllocationSequence: No STUN server configured, skipping.";
    return;
  }

  StunPort* port = StunPort::Create(session_->network_thread(),
                                session_->socket_factory(),
                                network_, ip_,
                                session_->allocator()->min_port(),
                                session_->allocator()->max_port(),
                                session_->username(), session_->password(),
                                config_->StunServers(),
                                session_->allocator()->origin());
  if (port) {
    session_->AddAllocatedPort(port, this, true);
    // Since StunPort is not created using shared socket, |port| will not be
    // added to the dequeue.
  }
}

void AllocationSequence::CreateRelayPorts() {
  if (IsFlagSet(PORTALLOCATOR_DISABLE_RELAY)) {
     LOG(LS_VERBOSE) << "AllocationSequence: Relay ports disabled, skipping.";
     return;
  }

  // If BasicPortAllocatorSession::OnAllocate left relay ports enabled then we
  // ought to have a relay list for them here.
  ASSERT(config_ && !config_->relays.empty());
  if (!(config_ && !config_->relays.empty())) {
    LOG(LS_WARNING)
        << "AllocationSequence: No relay server configured, skipping.";
    return;
  }

  PortConfiguration::RelayList::const_iterator relay;
  for (relay = config_->relays.begin();
       relay != config_->relays.end(); ++relay) {
    if (relay->type == RELAY_GTURN) {
      CreateGturnPort(*relay);
    } else if (relay->type == RELAY_TURN) {
      CreateTurnPort(*relay);
    } else {
      ASSERT(false);
    }
  }
}

void AllocationSequence::CreateGturnPort(const RelayServerConfig& config) {
  // TODO(mallinath) - Rename RelayPort to GTurnPort.
  RelayPort* port = RelayPort::Create(session_->network_thread(),
                                      session_->socket_factory(),
                                      network_, ip_,
                                      session_->allocator()->min_port(),
                                      session_->allocator()->max_port(),
                                      config_->username, config_->password);
  if (port) {
    // Since RelayPort is not created using shared socket, |port| will not be
    // added to the dequeue.
    // Note: We must add the allocated port before we add addresses because
    //       the latter will create candidates that need name and preference
    //       settings.  However, we also can't prepare the address (normally
    //       done by AddAllocatedPort) until we have these addresses.  So we
    //       wait to do that until below.
    session_->AddAllocatedPort(port, this, false);

    // Add the addresses of this protocol.
    PortList::const_iterator relay_port;
    for (relay_port = config.ports.begin();
         relay_port != config.ports.end();
         ++relay_port) {
      port->AddServerAddress(*relay_port);
      port->AddExternalAddress(*relay_port);
    }
    // Start fetching an address for this port.
    port->PrepareAddress();
  }
}

void AllocationSequence::CreateTurnPort(const RelayServerConfig& config) {
  PortList::const_iterator relay_port;
  for (relay_port = config.ports.begin();
       relay_port != config.ports.end(); ++relay_port) {
    TurnPort* port = NULL;
    // Shared socket mode must be enabled only for UDP based ports. Hence
    // don't pass shared socket for ports which will create TCP sockets.
    // TODO(mallinath) - Enable shared socket mode for TURN ports. Disabled
    // due to webrtc bug https://code.google.com/p/webrtc/issues/detail?id=3537
    if (IsFlagSet(PORTALLOCATOR_ENABLE_SHARED_SOCKET) &&
        relay_port->proto == PROTO_UDP) {
      port = TurnPort::Create(session_->network_thread(),
                              session_->socket_factory(),
                              network_, udp_socket_.get(),
                              session_->username(), session_->password(),
                              *relay_port, config.credentials, config.priority,
                              session_->allocator()->origin());
      turn_ports_.push_back(port);
      // Listen to the port destroyed signal, to allow AllocationSequence to
      // remove entrt from it's map.
      port->SignalDestroyed.connect(this, &AllocationSequence::OnPortDestroyed);
    } else {
      port = TurnPort::Create(session_->network_thread(),
                              session_->socket_factory(),
                              network_, ip_,
                              session_->allocator()->min_port(),
                              session_->allocator()->max_port(),
                              session_->username(),
                              session_->password(),
                              *relay_port, config.credentials, config.priority,
                              session_->allocator()->origin());
    }
    ASSERT(port != NULL);
    session_->AddAllocatedPort(port, this, true);
  }
}

void AllocationSequence::OnReadPacket(
    rtc::AsyncPacketSocket* socket, const char* data, size_t size,
    const rtc::SocketAddress& remote_addr,
    const rtc::PacketTime& packet_time) {
  ASSERT(socket == udp_socket_.get());

  bool turn_port_found = false;

  // Try to find the TurnPort that matches the remote address. Note that the
  // message could be a STUN binding response if the TURN server is also used as
  // a STUN server. We don't want to parse every message here to check if it is
  // a STUN binding response, so we pass the message to TurnPort regardless of
  // the message type. The TurnPort will just ignore the message since it will
  // not find any request by transaction ID.
  for (std::vector<TurnPort*>::const_iterator it = turn_ports_.begin();
       it != turn_ports_.end(); ++it) {
    TurnPort* port = *it;
    if (port->server_address().address == remote_addr) {
      port->HandleIncomingPacket(socket, data, size, remote_addr, packet_time);
      turn_port_found = true;
      break;
    }
  }

  if (udp_port_) {
    const ServerAddresses& stun_servers = udp_port_->server_addresses();

    // Pass the packet to the UdpPort if there is no matching TurnPort, or if
    // the TURN server is also a STUN server.
    if (!turn_port_found ||
        stun_servers.find(remote_addr) != stun_servers.end()) {
      udp_port_->HandleIncomingPacket(
          socket, data, size, remote_addr, packet_time);
    }
  }
}

void AllocationSequence::OnPortDestroyed(PortInterface* port) {
  if (udp_port_ == port) {
    udp_port_ = NULL;
    return;
  }

  auto it = std::find(turn_ports_.begin(), turn_ports_.end(), port);
  if (it != turn_ports_.end()) {
    turn_ports_.erase(it);
  } else {
    LOG(LS_ERROR) << "Unexpected OnPortDestroyed for nonexistent port.";
    ASSERT(false);
  }
}

// PortConfiguration
PortConfiguration::PortConfiguration(
    const rtc::SocketAddress& stun_address,
    const std::string& username,
    const std::string& password)
    : stun_address(stun_address), username(username), password(password) {
  if (!stun_address.IsNil())
    stun_servers.insert(stun_address);
}

PortConfiguration::PortConfiguration(const ServerAddresses& stun_servers,
                                     const std::string& username,
                                     const std::string& password)
    : stun_servers(stun_servers),
      username(username),
      password(password) {
  if (!stun_servers.empty())
    stun_address = *(stun_servers.begin());
}

ServerAddresses PortConfiguration::StunServers() {
  if (!stun_address.IsNil() &&
      stun_servers.find(stun_address) == stun_servers.end()) {
    stun_servers.insert(stun_address);
  }
  return stun_servers;
}

void PortConfiguration::AddRelay(const RelayServerConfig& config) {
  relays.push_back(config);
}

bool PortConfiguration::SupportsProtocol(
    const RelayServerConfig& relay, ProtocolType type) const {
  PortList::const_iterator relay_port;
  for (relay_port = relay.ports.begin();
        relay_port != relay.ports.end();
        ++relay_port) {
    if (relay_port->proto == type)
      return true;
  }
  return false;
}

bool PortConfiguration::SupportsProtocol(RelayType turn_type,
                                         ProtocolType type) const {
  for (size_t i = 0; i < relays.size(); ++i) {
    if (relays[i].type == turn_type &&
        SupportsProtocol(relays[i], type))
      return true;
  }
  return false;
}

ServerAddresses PortConfiguration::GetRelayServerAddresses(
    RelayType turn_type, ProtocolType type) const {
  ServerAddresses servers;
  for (size_t i = 0; i < relays.size(); ++i) {
    if (relays[i].type == turn_type && SupportsProtocol(relays[i], type)) {
      servers.insert(relays[i].ports.front().address);
    }
  }
  return servers;
}

}  // namespace cricket
