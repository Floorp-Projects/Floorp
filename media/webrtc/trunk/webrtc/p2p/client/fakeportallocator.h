/*
 *  Copyright 2010 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_CLIENT_FAKEPORTALLOCATOR_H_
#define WEBRTC_P2P_CLIENT_FAKEPORTALLOCATOR_H_

#include <string>
#include "webrtc/p2p/base/basicpacketsocketfactory.h"
#include "webrtc/p2p/base/portallocator.h"
#include "webrtc/p2p/base/udpport.h"
#include "webrtc/base/scoped_ptr.h"

namespace rtc {
class SocketFactory;
class Thread;
}

namespace cricket {

class TestUDPPort : public UDPPort {
 public:
  static TestUDPPort* Create(rtc::Thread* thread,
                             rtc::PacketSocketFactory* factory,
                             rtc::Network* network,
                             const rtc::IPAddress& ip,
                             uint16_t min_port,
                             uint16_t max_port,
                             const std::string& username,
                             const std::string& password,
                             const std::string& origin,
                             bool emit_localhost_for_anyaddress) {
    TestUDPPort* port = new TestUDPPort(thread, factory, network, ip, min_port,
                                        max_port, username, password, origin,
                                        emit_localhost_for_anyaddress);
    if (!port->Init()) {
      delete port;
      port = nullptr;
    }
    return port;
  }
  void SendBindingResponse(StunMessage* request,
                           const rtc::SocketAddress& addr) override {
    UDPPort::SendBindingResponse(request, addr);
    sent_binding_response_ = true;
  }
  bool sent_binding_response() { return sent_binding_response_; }
  void set_sent_binding_response(bool response) {
    sent_binding_response_ = response;
  }

 protected:
  TestUDPPort(rtc::Thread* thread,
              rtc::PacketSocketFactory* factory,
              rtc::Network* network,
              const rtc::IPAddress& ip,
              uint16_t min_port,
              uint16_t max_port,
              const std::string& username,
              const std::string& password,
              const std::string& origin,
              bool emit_localhost_for_anyaddress)
      : UDPPort(thread,
                factory,
                network,
                ip,
                min_port,
                max_port,
                username,
                password,
                origin,
                emit_localhost_for_anyaddress) {}

  bool sent_binding_response_ = false;
};

class FakePortAllocatorSession : public PortAllocatorSession {
 public:
  FakePortAllocatorSession(rtc::Thread* worker_thread,
                           rtc::PacketSocketFactory* factory,
                           const std::string& content_name,
                           int component,
                           const std::string& ice_ufrag,
                           const std::string& ice_pwd)
      : PortAllocatorSession(content_name, component, ice_ufrag, ice_pwd,
                             cricket::kDefaultPortAllocatorFlags),
        worker_thread_(worker_thread),
        factory_(factory),
        network_("network", "unittest",
                 rtc::IPAddress(INADDR_LOOPBACK), 8),
        port_(), running_(false),
        port_config_count_(0) {
    network_.AddIP(rtc::IPAddress(INADDR_LOOPBACK));
  }

  virtual void StartGettingPorts() {
    if (!port_) {
      port_.reset(TestUDPPort::Create(worker_thread_, factory_, &network_,
                                      network_.GetBestIP(), 0, 0, username(),
                                      password(), std::string(), false));
      AddPort(port_.get());
    }
    ++port_config_count_;
    running_ = true;
  }

  virtual void StopGettingPorts() { running_ = false; }
  virtual bool IsGettingPorts() { return running_; }
  virtual void ClearGettingPorts() {}

  int port_config_count() { return port_config_count_; }

  void AddPort(cricket::Port* port) {
    port->set_component(component_);
    port->set_generation(0);
    port->SignalPortComplete.connect(
        this, &FakePortAllocatorSession::OnPortComplete);
    port->PrepareAddress();
    SignalPortReady(this, port);
  }
  void OnPortComplete(cricket::Port* port) {
    SignalCandidatesReady(this, port->Candidates());
    SignalCandidatesAllocationDone(this);
  }

 private:
  rtc::Thread* worker_thread_;
  rtc::PacketSocketFactory* factory_;
  rtc::Network network_;
  rtc::scoped_ptr<cricket::Port> port_;
  bool running_;
  int port_config_count_;
};

class FakePortAllocator : public cricket::PortAllocator {
 public:
  FakePortAllocator(rtc::Thread* worker_thread,
                    rtc::PacketSocketFactory* factory)
      : worker_thread_(worker_thread), factory_(factory) {
    if (factory_ == NULL) {
      owned_factory_.reset(new rtc::BasicPacketSocketFactory(
          worker_thread_));
      factory_ = owned_factory_.get();
    }
  }

  void SetIceServers(
      const ServerAddresses& stun_servers,
      const std::vector<RelayServerConfig>& turn_servers) override {
    stun_servers_ = stun_servers;
    turn_servers_ = turn_servers;
  }

  void SetNetworkIgnoreMask(int network_ignore_mask) override {}

  const ServerAddresses& stun_servers() const { return stun_servers_; }

  const std::vector<RelayServerConfig>& turn_servers() const {
    return turn_servers_;
  }

  virtual cricket::PortAllocatorSession* CreateSessionInternal(
      const std::string& content_name,
      int component,
      const std::string& ice_ufrag,
      const std::string& ice_pwd) override {
    return new FakePortAllocatorSession(
        worker_thread_, factory_, content_name, component, ice_ufrag, ice_pwd);
  }

 private:
  rtc::Thread* worker_thread_;
  rtc::PacketSocketFactory* factory_;
  rtc::scoped_ptr<rtc::BasicPacketSocketFactory> owned_factory_;
  ServerAddresses stun_servers_;
  std::vector<RelayServerConfig> turn_servers_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_CLIENT_FAKEPORTALLOCATOR_H_
