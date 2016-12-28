/*
 *  Copyright 2009 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_FAKETRANSPORTCONTROLLER_H_
#define WEBRTC_P2P_BASE_FAKETRANSPORTCONTROLLER_H_

#include <map>
#include <string>
#include <vector>

#include "webrtc/p2p/base/transport.h"
#include "webrtc/p2p/base/transportchannel.h"
#include "webrtc/p2p/base/transportcontroller.h"
#include "webrtc/p2p/base/transportchannelimpl.h"
#include "webrtc/base/bind.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/fakesslidentity.h"
#include "webrtc/base/messagequeue.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/sslfingerprint.h"
#include "webrtc/base/thread.h"

namespace cricket {

class FakeTransport;

namespace {
struct PacketMessageData : public rtc::MessageData {
  PacketMessageData(const char* data, size_t len) : packet(data, len) {}
  rtc::Buffer packet;
};
}  // namespace

// Fake transport channel class, which can be passed to anything that needs a
// transport channel. Can be informed of another FakeTransportChannel via
// SetDestination.
// TODO(hbos): Move implementation to .cc file, this and other classes in file.
class FakeTransportChannel : public TransportChannelImpl,
                             public rtc::MessageHandler {
 public:
  explicit FakeTransportChannel(Transport* transport,
                                const std::string& name,
                                int component)
      : TransportChannelImpl(name, component),
        transport_(transport),
        dtls_fingerprint_("", nullptr, 0) {}
  ~FakeTransportChannel() { Reset(); }

  uint64_t IceTiebreaker() const { return tiebreaker_; }
  IceMode remote_ice_mode() const { return remote_ice_mode_; }
  const std::string& ice_ufrag() const { return ice_ufrag_; }
  const std::string& ice_pwd() const { return ice_pwd_; }
  const std::string& remote_ice_ufrag() const { return remote_ice_ufrag_; }
  const std::string& remote_ice_pwd() const { return remote_ice_pwd_; }
  const rtc::SSLFingerprint& dtls_fingerprint() const {
    return dtls_fingerprint_;
  }

  // If async, will send packets by "Post"-ing to message queue instead of
  // synchronously "Send"-ing.
  void SetAsync(bool async) { async_ = async; }

  Transport* GetTransport() override { return transport_; }

  TransportChannelState GetState() const override {
    if (connection_count_ == 0) {
      return had_connection_ ? TransportChannelState::STATE_FAILED
                             : TransportChannelState::STATE_INIT;
    }

    if (connection_count_ == 1) {
      return TransportChannelState::STATE_COMPLETED;
    }

    return TransportChannelState::STATE_CONNECTING;
  }

  void SetIceRole(IceRole role) override { role_ = role; }
  IceRole GetIceRole() const override { return role_; }
  void SetIceTiebreaker(uint64_t tiebreaker) override {
    tiebreaker_ = tiebreaker;
  }
  void SetIceCredentials(const std::string& ice_ufrag,
                         const std::string& ice_pwd) override {
    ice_ufrag_ = ice_ufrag;
    ice_pwd_ = ice_pwd;
  }
  void SetRemoteIceCredentials(const std::string& ice_ufrag,
                               const std::string& ice_pwd) override {
    remote_ice_ufrag_ = ice_ufrag;
    remote_ice_pwd_ = ice_pwd;
  }

  void SetRemoteIceMode(IceMode mode) override { remote_ice_mode_ = mode; }
  bool SetRemoteFingerprint(const std::string& alg,
                            const uint8_t* digest,
                            size_t digest_len) override {
    dtls_fingerprint_ = rtc::SSLFingerprint(alg, digest, digest_len);
    return true;
  }
  bool SetSslRole(rtc::SSLRole role) override {
    ssl_role_ = role;
    return true;
  }
  bool GetSslRole(rtc::SSLRole* role) const override {
    *role = ssl_role_;
    return true;
  }

  void Connect() override {
    if (state_ == STATE_INIT) {
      state_ = STATE_CONNECTING;
    }
  }

  void MaybeStartGathering() override {
    if (gathering_state_ == kIceGatheringNew) {
      gathering_state_ = kIceGatheringGathering;
      SignalGatheringState(this);
    }
  }

  IceGatheringState gathering_state() const override {
    return gathering_state_;
  }

  void Reset() {
    if (state_ != STATE_INIT) {
      state_ = STATE_INIT;
      if (dest_) {
        dest_->state_ = STATE_INIT;
        dest_->dest_ = nullptr;
        dest_ = nullptr;
      }
    }
  }

  void SetWritable(bool writable) { set_writable(writable); }

  void SetDestination(FakeTransportChannel* dest) {
    if (state_ == STATE_CONNECTING && dest) {
      // This simulates the delivery of candidates.
      dest_ = dest;
      dest_->dest_ = this;
      if (local_cert_ && dest_->local_cert_) {
        do_dtls_ = true;
        dest_->do_dtls_ = true;
        NegotiateSrtpCiphers();
      }
      state_ = STATE_CONNECTED;
      dest_->state_ = STATE_CONNECTED;
      set_writable(true);
      dest_->set_writable(true);
    } else if (state_ == STATE_CONNECTED && !dest) {
      // Simulates loss of connectivity, by asymmetrically forgetting dest_.
      dest_ = nullptr;
      state_ = STATE_CONNECTING;
      set_writable(false);
    }
  }

  void SetConnectionCount(size_t connection_count) {
    size_t old_connection_count = connection_count_;
    connection_count_ = connection_count;
    if (connection_count)
      had_connection_ = true;
    if (connection_count_ < old_connection_count)
      SignalConnectionRemoved(this);
  }

  void SetCandidatesGatheringComplete() {
    if (gathering_state_ != kIceGatheringComplete) {
      gathering_state_ = kIceGatheringComplete;
      SignalGatheringState(this);
    }
  }

  void SetReceiving(bool receiving) { set_receiving(receiving); }

  void SetIceConfig(const IceConfig& config) override {
    receiving_timeout_ = config.receiving_timeout_ms;
    gather_continually_ = config.gather_continually;
  }

  int receiving_timeout() const { return receiving_timeout_; }
  bool gather_continually() const { return gather_continually_; }

  int SendPacket(const char* data,
                 size_t len,
                 const rtc::PacketOptions& options,
                 int flags) override {
    if (state_ != STATE_CONNECTED) {
      return -1;
    }

    if (flags != PF_SRTP_BYPASS && flags != 0) {
      return -1;
    }

    PacketMessageData* packet = new PacketMessageData(data, len);
    if (async_) {
      rtc::Thread::Current()->Post(this, 0, packet);
    } else {
      rtc::Thread::Current()->Send(this, 0, packet);
    }
    rtc::SentPacket sent_packet(options.packet_id, rtc::Time());
    SignalSentPacket(this, sent_packet);
    return static_cast<int>(len);
  }
  int SetOption(rtc::Socket::Option opt, int value) override { return true; }
  bool GetOption(rtc::Socket::Option opt, int* value) override { return true; }
  int GetError() override { return 0; }

  void AddRemoteCandidate(const Candidate& candidate) override {
    remote_candidates_.push_back(candidate);
  }
  const Candidates& remote_candidates() const { return remote_candidates_; }

  void OnMessage(rtc::Message* msg) override {
    PacketMessageData* data = static_cast<PacketMessageData*>(msg->pdata);
    dest_->SignalReadPacket(dest_, data->packet.data<char>(),
                            data->packet.size(), rtc::CreatePacketTime(0), 0);
    delete data;
  }

  bool SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) {
    local_cert_ = certificate;
    return true;
  }

  void SetRemoteSSLCertificate(rtc::FakeSSLCertificate* cert) {
    remote_cert_ = cert;
  }

  bool IsDtlsActive() const override { return do_dtls_; }

  bool SetSrtpCryptoSuites(const std::vector<int>& ciphers) override {
    srtp_ciphers_ = ciphers;
    return true;
  }

  bool GetSrtpCryptoSuite(int* crypto_suite) override {
    if (chosen_crypto_suite_ != rtc::SRTP_INVALID_CRYPTO_SUITE) {
      *crypto_suite = chosen_crypto_suite_;
      return true;
    }
    return false;
  }

  bool GetSslCipherSuite(int* cipher_suite) override { return false; }

  rtc::scoped_refptr<rtc::RTCCertificate> GetLocalCertificate() const {
    return local_cert_;
  }

  bool GetRemoteSSLCertificate(rtc::SSLCertificate** cert) const override {
    if (!remote_cert_)
      return false;

    *cert = remote_cert_->GetReference();
    return true;
  }

  bool ExportKeyingMaterial(const std::string& label,
                            const uint8_t* context,
                            size_t context_len,
                            bool use_context,
                            uint8_t* result,
                            size_t result_len) override {
    if (chosen_crypto_suite_ != rtc::SRTP_INVALID_CRYPTO_SUITE) {
      memset(result, 0xff, result_len);
      return true;
    }

    return false;
  }

  void NegotiateSrtpCiphers() {
    for (std::vector<int>::const_iterator it1 = srtp_ciphers_.begin();
         it1 != srtp_ciphers_.end(); ++it1) {
      for (std::vector<int>::const_iterator it2 = dest_->srtp_ciphers_.begin();
           it2 != dest_->srtp_ciphers_.end(); ++it2) {
        if (*it1 == *it2) {
          chosen_crypto_suite_ = *it1;
          dest_->chosen_crypto_suite_ = *it2;
          return;
        }
      }
    }
  }

  bool GetStats(ConnectionInfos* infos) override {
    ConnectionInfo info;
    infos->clear();
    infos->push_back(info);
    return true;
  }

  void set_ssl_max_protocol_version(rtc::SSLProtocolVersion version) {
    ssl_max_version_ = version;
  }
  rtc::SSLProtocolVersion ssl_max_protocol_version() const {
    return ssl_max_version_;
  }

 private:
  enum State { STATE_INIT, STATE_CONNECTING, STATE_CONNECTED };
  Transport* transport_;
  FakeTransportChannel* dest_ = nullptr;
  State state_ = STATE_INIT;
  bool async_ = false;
  Candidates remote_candidates_;
  rtc::scoped_refptr<rtc::RTCCertificate> local_cert_;
  rtc::FakeSSLCertificate* remote_cert_ = nullptr;
  bool do_dtls_ = false;
  std::vector<int> srtp_ciphers_;
  int chosen_crypto_suite_ = rtc::SRTP_INVALID_CRYPTO_SUITE;
  int receiving_timeout_ = -1;
  bool gather_continually_ = false;
  IceRole role_ = ICEROLE_UNKNOWN;
  uint64_t tiebreaker_ = 0;
  std::string ice_ufrag_;
  std::string ice_pwd_;
  std::string remote_ice_ufrag_;
  std::string remote_ice_pwd_;
  IceMode remote_ice_mode_ = ICEMODE_FULL;
  rtc::SSLProtocolVersion ssl_max_version_ = rtc::SSL_PROTOCOL_DTLS_12;
  rtc::SSLFingerprint dtls_fingerprint_;
  rtc::SSLRole ssl_role_ = rtc::SSL_CLIENT;
  size_t connection_count_ = 0;
  IceGatheringState gathering_state_ = kIceGatheringNew;
  bool had_connection_ = false;
};

// Fake transport class, which can be passed to anything that needs a Transport.
// Can be informed of another FakeTransport via SetDestination (low-tech way
// of doing candidates)
class FakeTransport : public Transport {
 public:
  typedef std::map<int, FakeTransportChannel*> ChannelMap;

  explicit FakeTransport(const std::string& name) : Transport(name, nullptr) {}

  // Note that we only have a constructor with the allocator parameter so it can
  // be wrapped by a DtlsTransport.
  FakeTransport(const std::string& name, PortAllocator* allocator)
      : Transport(name, nullptr) {}

  ~FakeTransport() { DestroyAllChannels(); }

  const ChannelMap& channels() const { return channels_; }

  // If async, will send packets by "Post"-ing to message queue instead of
  // synchronously "Send"-ing.
  void SetAsync(bool async) { async_ = async; }
  void SetDestination(FakeTransport* dest) {
    dest_ = dest;
    for (const auto& kv : channels_) {
      kv.second->SetLocalCertificate(certificate_);
      SetChannelDestination(kv.first, kv.second);
    }
  }

  void SetWritable(bool writable) {
    for (const auto& kv : channels_) {
      kv.second->SetWritable(writable);
    }
  }

  void SetLocalCertificate(
      const rtc::scoped_refptr<rtc::RTCCertificate>& certificate) override {
    certificate_ = certificate;
  }
  bool GetLocalCertificate(
      rtc::scoped_refptr<rtc::RTCCertificate>* certificate) override {
    if (!certificate_)
      return false;

    *certificate = certificate_;
    return true;
  }

  bool GetSslRole(rtc::SSLRole* role) const override {
    if (channels_.empty()) {
      return false;
    }
    return channels_.begin()->second->GetSslRole(role);
  }

  bool SetSslMaxProtocolVersion(rtc::SSLProtocolVersion version) override {
    ssl_max_version_ = version;
    for (const auto& kv : channels_) {
      kv.second->set_ssl_max_protocol_version(ssl_max_version_);
    }
    return true;
  }
  rtc::SSLProtocolVersion ssl_max_protocol_version() const {
    return ssl_max_version_;
  }

  using Transport::local_description;
  using Transport::remote_description;

 protected:
  TransportChannelImpl* CreateTransportChannel(int component) override {
    if (channels_.find(component) != channels_.end()) {
      return nullptr;
    }
    FakeTransportChannel* channel =
        new FakeTransportChannel(this, name(), component);
    channel->set_ssl_max_protocol_version(ssl_max_version_);
    channel->SetAsync(async_);
    SetChannelDestination(component, channel);
    channels_[component] = channel;
    return channel;
  }

  void DestroyTransportChannel(TransportChannelImpl* channel) override {
    channels_.erase(channel->component());
    delete channel;
  }

 private:
  FakeTransportChannel* GetFakeChannel(int component) {
    auto it = channels_.find(component);
    return (it != channels_.end()) ? it->second : nullptr;
  }

  void SetChannelDestination(int component, FakeTransportChannel* channel) {
    FakeTransportChannel* dest_channel = nullptr;
    if (dest_) {
      dest_channel = dest_->GetFakeChannel(component);
      if (dest_channel) {
        dest_channel->SetLocalCertificate(dest_->certificate_);
      }
    }
    channel->SetDestination(dest_channel);
  }

  // Note, this is distinct from the Channel map owned by Transport.
  // This map just tracks the FakeTransportChannels created by this class.
  // It's mainly needed so that we can access a FakeTransportChannel directly,
  // even if wrapped by a DtlsTransportChannelWrapper.
  ChannelMap channels_;
  FakeTransport* dest_ = nullptr;
  bool async_ = false;
  rtc::scoped_refptr<rtc::RTCCertificate> certificate_;
  rtc::SSLProtocolVersion ssl_max_version_ = rtc::SSL_PROTOCOL_DTLS_12;
};

// Fake TransportController class, which can be passed into a BaseChannel object
// for test purposes. Can be connected to other FakeTransportControllers via
// Connect().
//
// This fake is unusual in that for the most part, it's implemented with the
// real TransportController code, but with fake TransportChannels underneath.
class FakeTransportController : public TransportController {
 public:
  FakeTransportController()
      : TransportController(rtc::Thread::Current(),
                            rtc::Thread::Current(),
                            nullptr),
        fail_create_channel_(false) {}

  explicit FakeTransportController(IceRole role)
      : TransportController(rtc::Thread::Current(),
                            rtc::Thread::Current(),
                            nullptr),
        fail_create_channel_(false) {
    SetIceRole(role);
  }

  explicit FakeTransportController(rtc::Thread* worker_thread)
      : TransportController(rtc::Thread::Current(), worker_thread, nullptr),
        fail_create_channel_(false) {}

  FakeTransportController(rtc::Thread* worker_thread, IceRole role)
      : TransportController(rtc::Thread::Current(), worker_thread, nullptr),
        fail_create_channel_(false) {
    SetIceRole(role);
  }

  FakeTransport* GetTransport_w(const std::string& transport_name) {
    return static_cast<FakeTransport*>(
        TransportController::GetTransport_w(transport_name));
  }

  void Connect(FakeTransportController* dest) {
    worker_thread()->Invoke<void>(
        rtc::Bind(&FakeTransportController::Connect_w, this, dest));
  }

  TransportChannel* CreateTransportChannel_w(const std::string& transport_name,
                                             int component) override {
    if (fail_create_channel_) {
      return nullptr;
    }
    return TransportController::CreateTransportChannel_w(transport_name,
                                                         component);
  }

  void set_fail_channel_creation(bool fail_channel_creation) {
    fail_create_channel_ = fail_channel_creation;
  }

 protected:
  Transport* CreateTransport_w(const std::string& transport_name) override {
    return new FakeTransport(transport_name);
  }

  void Connect_w(FakeTransportController* dest) {
    // Simulate the exchange of candidates.
    ConnectChannels_w();
    dest->ConnectChannels_w();
    for (auto& kv : transports()) {
      FakeTransport* transport = static_cast<FakeTransport*>(kv.second);
      transport->SetDestination(dest->GetTransport_w(kv.first));
    }
  }

  void ConnectChannels_w() {
    for (auto& kv : transports()) {
      FakeTransport* transport = static_cast<FakeTransport*>(kv.second);
      transport->ConnectChannels();
      transport->MaybeStartGathering();
    }
  }

 private:
  bool fail_create_channel_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_FAKETRANSPORTCONTROLLER_H_
