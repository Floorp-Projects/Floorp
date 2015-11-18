/*
 *  Copyright 2009 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_P2P_BASE_FAKESESSION_H_
#define WEBRTC_P2P_BASE_FAKESESSION_H_

#include <map>
#include <string>
#include <vector>

#include "webrtc/p2p/base/session.h"
#include "webrtc/p2p/base/transport.h"
#include "webrtc/p2p/base/transportchannel.h"
#include "webrtc/p2p/base/transportchannelimpl.h"
#include "webrtc/base/buffer.h"
#include "webrtc/base/fakesslidentity.h"
#include "webrtc/base/messagequeue.h"
#include "webrtc/base/sigslot.h"
#include "webrtc/base/sslfingerprint.h"

namespace cricket {

class FakeTransport;

struct PacketMessageData : public rtc::MessageData {
  PacketMessageData(const char* data, size_t len) : packet(data, len) {
  }
  rtc::Buffer packet;
};

// Fake transport channel class, which can be passed to anything that needs a
// transport channel. Can be informed of another FakeTransportChannel via
// SetDestination.
class FakeTransportChannel : public TransportChannelImpl,
                             public rtc::MessageHandler {
 public:
  explicit FakeTransportChannel(Transport* transport,
                                const std::string& content_name,
                                int component)
      : TransportChannelImpl(content_name, component),
        transport_(transport),
        dest_(NULL),
        state_(STATE_INIT),
        async_(false),
        identity_(NULL),
        do_dtls_(false),
        role_(ICEROLE_UNKNOWN),
        tiebreaker_(0),
        ice_proto_(ICEPROTO_HYBRID),
        remote_ice_mode_(ICEMODE_FULL),
        dtls_fingerprint_("", NULL, 0),
        ssl_role_(rtc::SSL_CLIENT),
        connection_count_(0) {
  }
  ~FakeTransportChannel() {
    Reset();
  }

  uint64 IceTiebreaker() const { return tiebreaker_; }
  TransportProtocol protocol() const { return ice_proto_; }
  IceMode remote_ice_mode() const { return remote_ice_mode_; }
  const std::string& ice_ufrag() const { return ice_ufrag_; }
  const std::string& ice_pwd() const { return ice_pwd_; }
  const std::string& remote_ice_ufrag() const { return remote_ice_ufrag_; }
  const std::string& remote_ice_pwd() const { return remote_ice_pwd_; }
  const rtc::SSLFingerprint& dtls_fingerprint() const {
    return dtls_fingerprint_;
  }

  void SetAsync(bool async) {
    async_ = async;
  }

  virtual Transport* GetTransport() {
    return transport_;
  }

  virtual TransportChannelState GetState() const {
    if (connection_count_ == 0) {
      return TransportChannelState::STATE_FAILED;
    }

    if (connection_count_ == 1) {
      return TransportChannelState::STATE_COMPLETED;
    }

    return TransportChannelState::STATE_FAILED;
  }

  virtual void SetIceRole(IceRole role) { role_ = role; }
  virtual IceRole GetIceRole() const { return role_; }
  virtual void SetIceTiebreaker(uint64 tiebreaker) { tiebreaker_ = tiebreaker; }
  virtual bool GetIceProtocolType(IceProtocolType* type) const {
    *type = ice_proto_;
    return true;
  }
  virtual void SetIceProtocolType(IceProtocolType type) { ice_proto_ = type; }
  virtual void SetIceCredentials(const std::string& ice_ufrag,
                                 const std::string& ice_pwd) {
    ice_ufrag_ = ice_ufrag;
    ice_pwd_ = ice_pwd;
  }
  virtual void SetRemoteIceCredentials(const std::string& ice_ufrag,
                                       const std::string& ice_pwd) {
    remote_ice_ufrag_ = ice_ufrag;
    remote_ice_pwd_ = ice_pwd;
  }

  virtual void SetRemoteIceMode(IceMode mode) { remote_ice_mode_ = mode; }
  virtual bool SetRemoteFingerprint(const std::string& alg, const uint8* digest,
                                    size_t digest_len) {
    dtls_fingerprint_ = rtc::SSLFingerprint(alg, digest, digest_len);
    return true;
  }
  virtual bool SetSslRole(rtc::SSLRole role) {
    ssl_role_ = role;
    return true;
  }
  virtual bool GetSslRole(rtc::SSLRole* role) const {
    *role = ssl_role_;
    return true;
  }

  virtual void Connect() {
    if (state_ == STATE_INIT) {
      state_ = STATE_CONNECTING;
    }
  }
  virtual void Reset() {
    if (state_ != STATE_INIT) {
      state_ = STATE_INIT;
      if (dest_) {
        dest_->state_ = STATE_INIT;
        dest_->dest_ = NULL;
        dest_ = NULL;
      }
    }
  }

  void SetWritable(bool writable) {
    set_writable(writable);
  }

  void SetDestination(FakeTransportChannel* dest) {
    if (state_ == STATE_CONNECTING && dest) {
      // This simulates the delivery of candidates.
      dest_ = dest;
      dest_->dest_ = this;
      if (identity_ && dest_->identity_) {
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
      dest_ = NULL;
      state_ = STATE_CONNECTING;
      set_writable(false);
    }
  }

  void SetConnectionCount(size_t connection_count) {
    size_t old_connection_count = connection_count_;
    connection_count_ = connection_count;
    if (connection_count_ < old_connection_count)
      SignalConnectionRemoved(this);
  }

  virtual int SendPacket(const char* data, size_t len,
                         const rtc::PacketOptions& options, int flags) {
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
    return static_cast<int>(len);
  }
  virtual int SetOption(rtc::Socket::Option opt, int value) {
    return true;
  }
  virtual bool GetOption(rtc::Socket::Option opt, int* value) {
    return true;
  }
  virtual int GetError() {
    return 0;
  }

  virtual void OnSignalingReady() {
  }
  virtual void OnCandidate(const Candidate& candidate) {
  }

  virtual void OnMessage(rtc::Message* msg) {
    PacketMessageData* data = static_cast<PacketMessageData*>(
        msg->pdata);
    dest_->SignalReadPacket(dest_, data->packet.data(), data->packet.size(),
                            rtc::CreatePacketTime(0), 0);
    delete data;
  }

  bool SetLocalIdentity(rtc::SSLIdentity* identity) {
    identity_ = identity;
    return true;
  }


  void SetRemoteCertificate(rtc::FakeSSLCertificate* cert) {
    remote_cert_ = cert;
  }

  virtual bool IsDtlsActive() const {
    return do_dtls_;
  }

  virtual bool SetSrtpCiphers(const std::vector<std::string>& ciphers) {
    srtp_ciphers_ = ciphers;
    return true;
  }

  virtual bool GetSrtpCipher(std::string* cipher) {
    if (!chosen_srtp_cipher_.empty()) {
      *cipher = chosen_srtp_cipher_;
      return true;
    }
    return false;
  }

  virtual bool GetSslCipher(std::string* cipher) {
    return false;
  }

  virtual bool GetLocalIdentity(rtc::SSLIdentity** identity) const {
    if (!identity_)
      return false;

    *identity = identity_->GetReference();
    return true;
  }

  virtual bool GetRemoteCertificate(rtc::SSLCertificate** cert) const {
    if (!remote_cert_)
      return false;

    *cert = remote_cert_->GetReference();
    return true;
  }

  virtual bool ExportKeyingMaterial(const std::string& label,
                                    const uint8* context,
                                    size_t context_len,
                                    bool use_context,
                                    uint8* result,
                                    size_t result_len) {
    if (!chosen_srtp_cipher_.empty()) {
      memset(result, 0xff, result_len);
      return true;
    }

    return false;
  }

  virtual void NegotiateSrtpCiphers() {
    for (std::vector<std::string>::const_iterator it1 = srtp_ciphers_.begin();
        it1 != srtp_ciphers_.end(); ++it1) {
      for (std::vector<std::string>::const_iterator it2 =
              dest_->srtp_ciphers_.begin();
          it2 != dest_->srtp_ciphers_.end(); ++it2) {
        if (*it1 == *it2) {
          chosen_srtp_cipher_ = *it1;
          dest_->chosen_srtp_cipher_ = *it2;
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

 private:
  enum State { STATE_INIT, STATE_CONNECTING, STATE_CONNECTED };
  Transport* transport_;
  FakeTransportChannel* dest_;
  State state_;
  bool async_;
  rtc::SSLIdentity* identity_;
  rtc::FakeSSLCertificate* remote_cert_;
  bool do_dtls_;
  std::vector<std::string> srtp_ciphers_;
  std::string chosen_srtp_cipher_;
  IceRole role_;
  uint64 tiebreaker_;
  IceProtocolType ice_proto_;
  std::string ice_ufrag_;
  std::string ice_pwd_;
  std::string remote_ice_ufrag_;
  std::string remote_ice_pwd_;
  IceMode remote_ice_mode_;
  rtc::SSLFingerprint dtls_fingerprint_;
  rtc::SSLRole ssl_role_;
  size_t connection_count_;
};

// Fake transport class, which can be passed to anything that needs a Transport.
// Can be informed of another FakeTransport via SetDestination (low-tech way
// of doing candidates)
class FakeTransport : public Transport {
 public:
  typedef std::map<int, FakeTransportChannel*> ChannelMap;
  FakeTransport(rtc::Thread* signaling_thread,
                rtc::Thread* worker_thread,
                const std::string& content_name,
                PortAllocator* alllocator = NULL)
      : Transport(signaling_thread, worker_thread,
                  content_name, "test_type", NULL),
      dest_(NULL),
      async_(false),
      identity_(NULL) {
  }
  ~FakeTransport() {
    DestroyAllChannels();
  }

  const ChannelMap& channels() const { return channels_; }

  void SetAsync(bool async) { async_ = async; }
  void SetDestination(FakeTransport* dest) {
    dest_ = dest;
    for (ChannelMap::iterator it = channels_.begin(); it != channels_.end();
         ++it) {
      it->second->SetLocalIdentity(identity_);
      SetChannelDestination(it->first, it->second);
    }
  }

  void SetWritable(bool writable) {
    for (ChannelMap::iterator it = channels_.begin(); it != channels_.end();
         ++it) {
      it->second->SetWritable(writable);
    }
  }

  void set_identity(rtc::SSLIdentity* identity) {
    identity_ = identity;
  }

  using Transport::local_description;
  using Transport::remote_description;

 protected:
  virtual TransportChannelImpl* CreateTransportChannel(int component) {
    if (channels_.find(component) != channels_.end()) {
      return NULL;
    }
    FakeTransportChannel* channel =
        new FakeTransportChannel(this, content_name(), component);
    channel->SetAsync(async_);
    SetChannelDestination(component, channel);
    channels_[component] = channel;
    return channel;
  }
  virtual void DestroyTransportChannel(TransportChannelImpl* channel) {
    channels_.erase(channel->component());
    delete channel;
  }
  virtual void SetIdentity_w(rtc::SSLIdentity* identity) {
    identity_ = identity;
  }
  virtual bool GetIdentity_w(rtc::SSLIdentity** identity) {
    if (!identity_)
      return false;

    *identity = identity_->GetReference();
    return true;
  }

 private:
  FakeTransportChannel* GetFakeChannel(int component) {
    ChannelMap::iterator it = channels_.find(component);
    return (it != channels_.end()) ? it->second : NULL;
  }
  void SetChannelDestination(int component,
                             FakeTransportChannel* channel) {
    FakeTransportChannel* dest_channel = NULL;
    if (dest_) {
      dest_channel = dest_->GetFakeChannel(component);
      if (dest_channel) {
        dest_channel->SetLocalIdentity(dest_->identity_);
      }
    }
    channel->SetDestination(dest_channel);
  }

  // Note, this is distinct from the Channel map owned by Transport.
  // This map just tracks the FakeTransportChannels created by this class.
  ChannelMap channels_;
  FakeTransport* dest_;
  bool async_;
  rtc::SSLIdentity* identity_;
};

// Fake session class, which can be passed into a BaseChannel object for
// test purposes. Can be connected to other FakeSessions via Connect().
class FakeSession : public BaseSession {
 public:
  explicit FakeSession()
      : BaseSession(rtc::Thread::Current(),
                    rtc::Thread::Current(),
                    NULL, "", "", true),
      fail_create_channel_(false) {
  }
  explicit FakeSession(bool initiator)
      : BaseSession(rtc::Thread::Current(),
                    rtc::Thread::Current(),
                    NULL, "", "", initiator),
      fail_create_channel_(false) {
  }
  FakeSession(rtc::Thread* worker_thread, bool initiator)
      : BaseSession(rtc::Thread::Current(),
                    worker_thread,
                    NULL, "", "", initiator),
      fail_create_channel_(false) {
  }

  FakeTransport* GetTransport(const std::string& content_name) {
    return static_cast<FakeTransport*>(
        BaseSession::GetTransport(content_name));
  }

  void Connect(FakeSession* dest) {
    // Simulate the exchange of candidates.
    CompleteNegotiation();
    dest->CompleteNegotiation();
    for (TransportMap::const_iterator it = transport_proxies().begin();
        it != transport_proxies().end(); ++it) {
      static_cast<FakeTransport*>(it->second->impl())->SetDestination(
          dest->GetTransport(it->first));
    }
  }

  virtual TransportChannel* CreateChannel(
      const std::string& content_name,
      int component) {
    if (fail_create_channel_) {
      return NULL;
    }
    return BaseSession::CreateChannel(content_name, component);
  }

  void set_fail_channel_creation(bool fail_channel_creation) {
    fail_create_channel_ = fail_channel_creation;
  }

  // TODO: Hoist this into Session when we re-work the Session code.
  void set_ssl_identity(rtc::SSLIdentity* identity) {
    for (TransportMap::const_iterator it = transport_proxies().begin();
        it != transport_proxies().end(); ++it) {
      // We know that we have a FakeTransport*

      static_cast<FakeTransport*>(it->second->impl())->set_identity
          (identity);
    }
  }

 protected:
  virtual Transport* CreateTransport(const std::string& content_name) {
    return new FakeTransport(signaling_thread(), worker_thread(), content_name);
  }

  void CompleteNegotiation() {
    for (TransportMap::const_iterator it = transport_proxies().begin();
        it != transport_proxies().end(); ++it) {
      it->second->CompleteNegotiation();
      it->second->ConnectChannels();
    }
  }

 private:
  bool fail_create_channel_;
};

}  // namespace cricket

#endif  // WEBRTC_P2P_BASE_FAKESESSION_H_
