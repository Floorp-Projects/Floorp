/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <set>

#include "webrtc/p2p/base/dtlstransport.h"
#include "webrtc/p2p/base/fakesession.h"
#include "webrtc/base/common.h"
#include "webrtc/base/dscp.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/sslidentity.h"
#include "webrtc/base/sslstreamadapter.h"
#include "webrtc/base/stringutils.h"
#include "webrtc/base/thread.h"

#define MAYBE_SKIP_TEST(feature)                    \
  if (!(rtc::SSLStreamAdapter::feature())) {  \
    LOG(LS_INFO) << "Feature disabled... skipping"; \
    return;                                         \
  }

static const char AES_CM_128_HMAC_SHA1_80[] = "AES_CM_128_HMAC_SHA1_80";
static const char kIceUfrag1[] = "TESTICEUFRAG0001";
static const char kIcePwd1[] = "TESTICEPWD00000000000001";
static const size_t kPacketNumOffset = 8;
static const size_t kPacketHeaderLen = 12;

static bool IsRtpLeadByte(uint8 b) {
  return ((b & 0xC0) == 0x80);
}

using cricket::ConnectionRole;

enum Flags { NF_REOFFER = 0x1, NF_EXPECT_FAILURE = 0x2 };

class DtlsTestClient : public sigslot::has_slots<> {
 public:
  DtlsTestClient(const std::string& name,
                 rtc::Thread* signaling_thread,
                 rtc::Thread* worker_thread) :
      name_(name),
      signaling_thread_(signaling_thread),
      worker_thread_(worker_thread),
      protocol_(cricket::ICEPROTO_GOOGLE),
      packet_size_(0),
      use_dtls_srtp_(false),
      negotiated_dtls_(false),
      received_dtls_client_hello_(false),
      received_dtls_server_hello_(false) {
  }
  void SetIceProtocol(cricket::TransportProtocol proto) {
    protocol_ = proto;
  }
  void CreateIdentity() {
    identity_.reset(rtc::SSLIdentity::Generate(name_));
  }
  rtc::SSLIdentity* identity() { return identity_.get(); }
  void SetupSrtp() {
    ASSERT(identity_.get() != NULL);
    use_dtls_srtp_ = true;
  }
  void SetupChannels(int count, cricket::IceRole role) {
    transport_.reset(new cricket::DtlsTransport<cricket::FakeTransport>(
        signaling_thread_, worker_thread_, "dtls content name", NULL,
        identity_.get()));
    transport_->SetAsync(true);
    transport_->SetIceRole(role);
    transport_->SetIceTiebreaker(
        (role == cricket::ICEROLE_CONTROLLING) ? 1 : 2);
    transport_->SignalWritableState.connect(this,
        &DtlsTestClient::OnTransportWritableState);

    for (int i = 0; i < count; ++i) {
      cricket::DtlsTransportChannelWrapper* channel =
          static_cast<cricket::DtlsTransportChannelWrapper*>(
              transport_->CreateChannel(i));
      ASSERT_TRUE(channel != NULL);
      channel->SignalWritableState.connect(this,
        &DtlsTestClient::OnTransportChannelWritableState);
      channel->SignalReadPacket.connect(this,
        &DtlsTestClient::OnTransportChannelReadPacket);
      channels_.push_back(channel);

      // Hook the raw packets so that we can verify they are encrypted.
      channel->channel()->SignalReadPacket.connect(
          this, &DtlsTestClient::OnFakeTransportChannelReadPacket);
    }
  }

  cricket::Transport* transport() { return transport_.get(); }

  cricket::FakeTransportChannel* GetFakeChannel(int component) {
    cricket::TransportChannelImpl* ch = transport_->GetChannel(component);
    cricket::DtlsTransportChannelWrapper* wrapper =
        static_cast<cricket::DtlsTransportChannelWrapper*>(ch);
    return (wrapper) ?
        static_cast<cricket::FakeTransportChannel*>(wrapper->channel()) : NULL;
  }

  // Offer DTLS if we have an identity; pass in a remote fingerprint only if
  // both sides support DTLS.
  void Negotiate(DtlsTestClient* peer, cricket::ContentAction action,
                 ConnectionRole local_role, ConnectionRole remote_role,
                 int flags) {
    Negotiate(identity_.get(), (identity_) ? peer->identity_.get() : NULL,
              action, local_role, remote_role, flags);
  }

  // Allow any DTLS configuration to be specified (including invalid ones).
  void Negotiate(rtc::SSLIdentity* local_identity,
                 rtc::SSLIdentity* remote_identity,
                 cricket::ContentAction action,
                 ConnectionRole local_role,
                 ConnectionRole remote_role,
                 int flags) {
    rtc::scoped_ptr<rtc::SSLFingerprint> local_fingerprint;
    rtc::scoped_ptr<rtc::SSLFingerprint> remote_fingerprint;
    if (local_identity) {
      local_fingerprint.reset(rtc::SSLFingerprint::Create(
          rtc::DIGEST_SHA_1, local_identity));
      ASSERT_TRUE(local_fingerprint.get() != NULL);
    }
    if (remote_identity) {
      remote_fingerprint.reset(rtc::SSLFingerprint::Create(
          rtc::DIGEST_SHA_1, remote_identity));
      ASSERT_TRUE(remote_fingerprint.get() != NULL);
    }

    if (use_dtls_srtp_ && !(flags & NF_REOFFER)) {
      // SRTP ciphers will be set only in the beginning.
      for (std::vector<cricket::DtlsTransportChannelWrapper*>::iterator it =
           channels_.begin(); it != channels_.end(); ++it) {
        std::vector<std::string> ciphers;
        ciphers.push_back(AES_CM_128_HMAC_SHA1_80);
        ASSERT_TRUE((*it)->SetSrtpCiphers(ciphers));
      }
    }

    std::string transport_type = (protocol_ == cricket::ICEPROTO_GOOGLE) ?
        cricket::NS_GINGLE_P2P : cricket::NS_JINGLE_ICE_UDP;
    cricket::TransportDescription local_desc(
        transport_type, std::vector<std::string>(), kIceUfrag1, kIcePwd1,
        cricket::ICEMODE_FULL, local_role,
         // If remote if the offerer and has no DTLS support, answer will be
        // without any fingerprint.
        (action == cricket::CA_ANSWER && !remote_identity) ?
            NULL : local_fingerprint.get(),
        cricket::Candidates());

    cricket::TransportDescription remote_desc(
        transport_type, std::vector<std::string>(), kIceUfrag1, kIcePwd1,
        cricket::ICEMODE_FULL, remote_role, remote_fingerprint.get(),
        cricket::Candidates());

    bool expect_success = (flags & NF_EXPECT_FAILURE) ? false : true;
    // If |expect_success| is false, expect SRTD or SLTD to fail when
    // content action is CA_ANSWER.
    if (action == cricket::CA_OFFER) {
      ASSERT_TRUE(transport_->SetLocalTransportDescription(
          local_desc, cricket::CA_OFFER, NULL));
      ASSERT_EQ(expect_success, transport_->SetRemoteTransportDescription(
          remote_desc, cricket::CA_ANSWER, NULL));
    } else {
      ASSERT_TRUE(transport_->SetRemoteTransportDescription(
          remote_desc, cricket::CA_OFFER, NULL));
      ASSERT_EQ(expect_success, transport_->SetLocalTransportDescription(
          local_desc, cricket::CA_ANSWER, NULL));
    }
    negotiated_dtls_ = (local_identity && remote_identity);
  }

  bool Connect(DtlsTestClient* peer) {
    transport_->ConnectChannels();
    transport_->SetDestination(peer->transport_.get());
    return true;
  }

  bool writable() const { return transport_->writable(); }

  void CheckRole(rtc::SSLRole role) {
    if (role == rtc::SSL_CLIENT) {
      ASSERT_FALSE(received_dtls_client_hello_);
      ASSERT_TRUE(received_dtls_server_hello_);
    } else {
      ASSERT_TRUE(received_dtls_client_hello_);
      ASSERT_FALSE(received_dtls_server_hello_);
    }
  }

  void CheckSrtp(const std::string& expected_cipher) {
    for (std::vector<cricket::DtlsTransportChannelWrapper*>::iterator it =
           channels_.begin(); it != channels_.end(); ++it) {
      std::string cipher;

      bool rv = (*it)->GetSrtpCipher(&cipher);
      if (negotiated_dtls_ && !expected_cipher.empty()) {
        ASSERT_TRUE(rv);

        ASSERT_EQ(cipher, expected_cipher);
      } else {
        ASSERT_FALSE(rv);
      }
    }
  }

  void CheckSsl(const std::string& expected_cipher) {
    for (std::vector<cricket::DtlsTransportChannelWrapper*>::iterator it =
           channels_.begin(); it != channels_.end(); ++it) {
      std::string cipher;

      bool rv = (*it)->GetSslCipher(&cipher);
      if (negotiated_dtls_ && !expected_cipher.empty()) {
        ASSERT_TRUE(rv);

        ASSERT_EQ(cipher, expected_cipher);
      } else {
        ASSERT_FALSE(rv);
      }
    }
  }

  void SendPackets(size_t channel, size_t size, size_t count, bool srtp) {
    ASSERT(channel < channels_.size());
    rtc::scoped_ptr<char[]> packet(new char[size]);
    size_t sent = 0;
    do {
      // Fill the packet with a known value and a sequence number to check
      // against, and make sure that it doesn't look like DTLS.
      memset(packet.get(), sent & 0xff, size);
      packet[0] = (srtp) ? 0x80 : 0x00;
      rtc::SetBE32(packet.get() + kPacketNumOffset,
                         static_cast<uint32>(sent));

      // Only set the bypass flag if we've activated DTLS.
      int flags = (identity_.get() && srtp) ? cricket::PF_SRTP_BYPASS : 0;
      rtc::PacketOptions packet_options;
      int rv = channels_[channel]->SendPacket(
          packet.get(), size, packet_options, flags);
      ASSERT_GT(rv, 0);
      ASSERT_EQ(size, static_cast<size_t>(rv));
      ++sent;
    } while (sent < count);
  }

  int SendInvalidSrtpPacket(size_t channel, size_t size) {
    ASSERT(channel < channels_.size());
    rtc::scoped_ptr<char[]> packet(new char[size]);
    // Fill the packet with 0 to form an invalid SRTP packet.
    memset(packet.get(), 0, size);

    rtc::PacketOptions packet_options;
    return channels_[channel]->SendPacket(
        packet.get(), size, packet_options, cricket::PF_SRTP_BYPASS);
  }

  void ExpectPackets(size_t channel, size_t size) {
    packet_size_ = size;
    received_.clear();
  }

  size_t NumPacketsReceived() {
    return received_.size();
  }

  bool VerifyPacket(const char* data, size_t size, uint32* out_num) {
    if (size != packet_size_ ||
        (data[0] != 0 && static_cast<uint8>(data[0]) != 0x80)) {
      return false;
    }
    uint32 packet_num = rtc::GetBE32(data + kPacketNumOffset);
    for (size_t i = kPacketHeaderLen; i < size; ++i) {
      if (static_cast<uint8>(data[i]) != (packet_num & 0xff)) {
        return false;
      }
    }
    if (out_num) {
      *out_num = packet_num;
    }
    return true;
  }
  bool VerifyEncryptedPacket(const char* data, size_t size) {
    // This is an encrypted data packet; let's make sure it's mostly random;
    // less than 10% of the bytes should be equal to the cleartext packet.
    if (size <= packet_size_) {
      return false;
    }
    uint32 packet_num = rtc::GetBE32(data + kPacketNumOffset);
    int num_matches = 0;
    for (size_t i = kPacketNumOffset; i < size; ++i) {
      if (static_cast<uint8>(data[i]) == (packet_num & 0xff)) {
        ++num_matches;
      }
    }
    return (num_matches < ((static_cast<int>(size) - 5) / 10));
  }

  // Transport callbacks
  void OnTransportWritableState(cricket::Transport* transport) {
    LOG(LS_INFO) << name_ << ": is writable";
  }

  // Transport channel callbacks
  void OnTransportChannelWritableState(cricket::TransportChannel* channel) {
    LOG(LS_INFO) << name_ << ": Channel '" << channel->component()
                 << "' is writable";
  }

  void OnTransportChannelReadPacket(cricket::TransportChannel* channel,
                                    const char* data, size_t size,
                                    const rtc::PacketTime& packet_time,
                                    int flags) {
    uint32 packet_num = 0;
    ASSERT_TRUE(VerifyPacket(data, size, &packet_num));
    received_.insert(packet_num);
    // Only DTLS-SRTP packets should have the bypass flag set.
    int expected_flags = (identity_.get() && IsRtpLeadByte(data[0])) ?
        cricket::PF_SRTP_BYPASS : 0;
    ASSERT_EQ(expected_flags, flags);
  }

  // Hook into the raw packet stream to make sure DTLS packets are encrypted.
  void OnFakeTransportChannelReadPacket(cricket::TransportChannel* channel,
                                        const char* data, size_t size,
                                        const rtc::PacketTime& time,
                                        int flags) {
    // Flags shouldn't be set on the underlying TransportChannel packets.
    ASSERT_EQ(0, flags);

    // Look at the handshake packets to see what role we played.
    // Check that non-handshake packets are DTLS data or SRTP bypass.
    if (negotiated_dtls_) {
      if (data[0] == 22 && size > 17) {
        if (data[13] == 1) {
          received_dtls_client_hello_ = true;
        } else if (data[13] == 2) {
          received_dtls_server_hello_ = true;
        }
      } else if (!(data[0] >= 20 && data[0] <= 22)) {
        ASSERT_TRUE(data[0] == 23 || IsRtpLeadByte(data[0]));
        if (data[0] == 23) {
          ASSERT_TRUE(VerifyEncryptedPacket(data, size));
        } else if (IsRtpLeadByte(data[0])) {
          ASSERT_TRUE(VerifyPacket(data, size, NULL));
        }
      }
    }
  }

 private:
  std::string name_;
  rtc::Thread* signaling_thread_;
  rtc::Thread* worker_thread_;
  cricket::TransportProtocol protocol_;
  rtc::scoped_ptr<rtc::SSLIdentity> identity_;
  rtc::scoped_ptr<cricket::FakeTransport> transport_;
  std::vector<cricket::DtlsTransportChannelWrapper*> channels_;
  size_t packet_size_;
  std::set<int> received_;
  bool use_dtls_srtp_;
  bool negotiated_dtls_;
  bool received_dtls_client_hello_;
  bool received_dtls_server_hello_;
};


class DtlsTransportChannelTest : public testing::Test {
 public:
  DtlsTransportChannelTest() :
      client1_("P1", rtc::Thread::Current(),
               rtc::Thread::Current()),
      client2_("P2", rtc::Thread::Current(),
               rtc::Thread::Current()),
      channel_ct_(1),
      use_dtls_(false),
      use_dtls_srtp_(false) {
  }

  void SetChannelCount(size_t channel_ct) {
    channel_ct_ = static_cast<int>(channel_ct);
  }
  void PrepareDtls(bool c1, bool c2) {
    if (c1) {
      client1_.CreateIdentity();
    }
    if (c2) {
      client2_.CreateIdentity();
    }
    if (c1 && c2)
      use_dtls_ = true;
  }
  void PrepareDtlsSrtp(bool c1, bool c2) {
    if (!use_dtls_)
      return;

    if (c1)
      client1_.SetupSrtp();
    if (c2)
      client2_.SetupSrtp();

    if (c1 && c2)
      use_dtls_srtp_ = true;
  }

  bool Connect(ConnectionRole client1_role, ConnectionRole client2_role) {
    Negotiate(client1_role, client2_role);

    bool rv = client1_.Connect(&client2_);
    EXPECT_TRUE(rv);
    if (!rv)
      return false;

    EXPECT_TRUE_WAIT(client1_.writable() && client2_.writable(), 10000);
    if (!client1_.writable() || !client2_.writable())
      return false;

    // Check that we used the right roles.
    if (use_dtls_) {
      rtc::SSLRole client1_ssl_role =
          (client1_role == cricket::CONNECTIONROLE_ACTIVE ||
           (client2_role == cricket::CONNECTIONROLE_PASSIVE &&
            client1_role == cricket::CONNECTIONROLE_ACTPASS)) ?
              rtc::SSL_CLIENT : rtc::SSL_SERVER;

      rtc::SSLRole client2_ssl_role =
          (client2_role == cricket::CONNECTIONROLE_ACTIVE ||
           (client1_role == cricket::CONNECTIONROLE_PASSIVE &&
            client2_role == cricket::CONNECTIONROLE_ACTPASS)) ?
              rtc::SSL_CLIENT : rtc::SSL_SERVER;

      client1_.CheckRole(client1_ssl_role);
      client2_.CheckRole(client2_ssl_role);
    }

    // Check that we negotiated the right ciphers.
    if (use_dtls_srtp_) {
      client1_.CheckSrtp(AES_CM_128_HMAC_SHA1_80);
      client2_.CheckSrtp(AES_CM_128_HMAC_SHA1_80);
    } else {
      client1_.CheckSrtp("");
      client2_.CheckSrtp("");
    }
    client1_.CheckSsl(rtc::SSLStreamAdapter::GetDefaultSslCipher());
    client2_.CheckSsl(rtc::SSLStreamAdapter::GetDefaultSslCipher());

    return true;
  }

  bool Connect() {
    // By default, Client1 will be Server and Client2 will be Client.
    return Connect(cricket::CONNECTIONROLE_ACTPASS,
                   cricket::CONNECTIONROLE_ACTIVE);
  }

  void Negotiate() {
    Negotiate(cricket::CONNECTIONROLE_ACTPASS, cricket::CONNECTIONROLE_ACTIVE);
  }

  void Negotiate(ConnectionRole client1_role, ConnectionRole client2_role) {
    client1_.SetupChannels(channel_ct_, cricket::ICEROLE_CONTROLLING);
    client2_.SetupChannels(channel_ct_, cricket::ICEROLE_CONTROLLED);
    // Expect success from SLTD and SRTD.
    client1_.Negotiate(&client2_, cricket::CA_OFFER,
                       client1_role, client2_role, 0);
    client2_.Negotiate(&client1_, cricket::CA_ANSWER,
                       client2_role, client1_role, 0);
  }

  // Negotiate with legacy client |client2|. Legacy client doesn't use setup
  // attributes, except NONE.
  void NegotiateWithLegacy() {
    client1_.SetupChannels(channel_ct_, cricket::ICEROLE_CONTROLLING);
    client2_.SetupChannels(channel_ct_, cricket::ICEROLE_CONTROLLED);
    // Expect success from SLTD and SRTD.
    client1_.Negotiate(&client2_, cricket::CA_OFFER,
                       cricket::CONNECTIONROLE_ACTPASS,
                       cricket::CONNECTIONROLE_NONE, 0);
    client2_.Negotiate(&client1_, cricket::CA_ANSWER,
                       cricket::CONNECTIONROLE_ACTIVE,
                       cricket::CONNECTIONROLE_NONE, 0);
  }

  void Renegotiate(DtlsTestClient* reoffer_initiator,
                   ConnectionRole client1_role, ConnectionRole client2_role,
                   int flags) {
    if (reoffer_initiator == &client1_) {
      client1_.Negotiate(&client2_, cricket::CA_OFFER,
                         client1_role, client2_role, flags);
      client2_.Negotiate(&client1_, cricket::CA_ANSWER,
                         client2_role, client1_role, flags);
    } else {
      client2_.Negotiate(&client1_, cricket::CA_OFFER,
                         client2_role, client1_role, flags);
      client1_.Negotiate(&client2_, cricket::CA_ANSWER,
                         client1_role, client2_role, flags);
    }
  }

  void TestTransfer(size_t channel, size_t size, size_t count, bool srtp) {
    LOG(LS_INFO) << "Expect packets, size=" << size;
    client2_.ExpectPackets(channel, size);
    client1_.SendPackets(channel, size, count, srtp);
    EXPECT_EQ_WAIT(count, client2_.NumPacketsReceived(), 10000);
  }

 protected:
  DtlsTestClient client1_;
  DtlsTestClient client2_;
  int channel_ct_;
  bool use_dtls_;
  bool use_dtls_srtp_;
};

// Test that transport negotiation of ICE, no DTLS works properly.
TEST_F(DtlsTransportChannelTest, TestChannelSetupIce) {
  client1_.SetIceProtocol(cricket::ICEPROTO_RFC5245);
  client2_.SetIceProtocol(cricket::ICEPROTO_RFC5245);
  Negotiate();
  cricket::FakeTransportChannel* channel1 = client1_.GetFakeChannel(0);
  cricket::FakeTransportChannel* channel2 = client2_.GetFakeChannel(0);
  ASSERT_TRUE(channel1 != NULL);
  ASSERT_TRUE(channel2 != NULL);
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel1->GetIceRole());
  EXPECT_EQ(1U, channel1->IceTiebreaker());
  EXPECT_EQ(cricket::ICEPROTO_RFC5245, channel1->protocol());
  EXPECT_EQ(kIceUfrag1, channel1->ice_ufrag());
  EXPECT_EQ(kIcePwd1, channel1->ice_pwd());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED, channel2->GetIceRole());
  EXPECT_EQ(2U, channel2->IceTiebreaker());
  EXPECT_EQ(cricket::ICEPROTO_RFC5245, channel2->protocol());
}

// Test that transport negotiation of GICE, no DTLS works properly.
TEST_F(DtlsTransportChannelTest, TestChannelSetupGice) {
  client1_.SetIceProtocol(cricket::ICEPROTO_GOOGLE);
  client2_.SetIceProtocol(cricket::ICEPROTO_GOOGLE);
  Negotiate();
  cricket::FakeTransportChannel* channel1 = client1_.GetFakeChannel(0);
  cricket::FakeTransportChannel* channel2 = client2_.GetFakeChannel(0);
  ASSERT_TRUE(channel1 != NULL);
  ASSERT_TRUE(channel2 != NULL);
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel1->GetIceRole());
  EXPECT_EQ(1U, channel1->IceTiebreaker());
  EXPECT_EQ(cricket::ICEPROTO_GOOGLE, channel1->protocol());
  EXPECT_EQ(kIceUfrag1, channel1->ice_ufrag());
  EXPECT_EQ(kIcePwd1, channel1->ice_pwd());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED, channel2->GetIceRole());
  EXPECT_EQ(2U, channel2->IceTiebreaker());
  EXPECT_EQ(cricket::ICEPROTO_GOOGLE, channel2->protocol());
}

// Connect without DTLS, and transfer some data.
TEST_F(DtlsTransportChannelTest, TestTransfer) {
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, false);
}

// Create two channels without DTLS, and transfer some data.
TEST_F(DtlsTransportChannelTest, TestTransferTwoChannels) {
  SetChannelCount(2);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, false);
  TestTransfer(1, 1000, 100, false);
}

// Connect without DTLS, and transfer SRTP data.
TEST_F(DtlsTransportChannelTest, TestTransferSrtp) {
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, true);
}

// Create two channels without DTLS, and transfer SRTP data.
TEST_F(DtlsTransportChannelTest, TestTransferSrtpTwoChannels) {
  SetChannelCount(2);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, true);
  TestTransfer(1, 1000, 100, true);
}

// Connect with DTLS, and transfer some data.
TEST_F(DtlsTransportChannelTest, TestTransferDtls) {
  MAYBE_SKIP_TEST(HaveDtls);
  PrepareDtls(true, true);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, false);
}

// Create two channels with DTLS, and transfer some data.
TEST_F(DtlsTransportChannelTest, TestTransferDtlsTwoChannels) {
  MAYBE_SKIP_TEST(HaveDtls);
  SetChannelCount(2);
  PrepareDtls(true, true);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, false);
  TestTransfer(1, 1000, 100, false);
}

// Connect with A doing DTLS and B not, and transfer some data.
TEST_F(DtlsTransportChannelTest, TestTransferDtlsRejected) {
  PrepareDtls(true, false);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, false);
}

// Connect with B doing DTLS and A not, and transfer some data.
TEST_F(DtlsTransportChannelTest, TestTransferDtlsNotOffered) {
  PrepareDtls(false, true);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, false);
}

// Connect with DTLS, negotiate DTLS-SRTP, and transfer SRTP using bypass.
TEST_F(DtlsTransportChannelTest, TestTransferDtlsSrtp) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  PrepareDtls(true, true);
  PrepareDtlsSrtp(true, true);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, true);
}

// Connect with DTLS-SRTP, transfer an invalid SRTP packet, and expects -1
// returned.
TEST_F(DtlsTransportChannelTest, TestTransferDtlsInvalidSrtpPacket) {
  MAYBE_SKIP_TEST(HaveDtls);
  PrepareDtls(true, true);
  PrepareDtlsSrtp(true, true);
  ASSERT_TRUE(Connect());
  int result = client1_.SendInvalidSrtpPacket(0, 100);
  ASSERT_EQ(-1, result);
}

// Connect with DTLS. A does DTLS-SRTP but B does not.
TEST_F(DtlsTransportChannelTest, TestTransferDtlsSrtpRejected) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  PrepareDtls(true, true);
  PrepareDtlsSrtp(true, false);
  ASSERT_TRUE(Connect());
}

// Connect with DTLS. B does DTLS-SRTP but A does not.
TEST_F(DtlsTransportChannelTest, TestTransferDtlsSrtpNotOffered) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  PrepareDtls(true, true);
  PrepareDtlsSrtp(false, true);
  ASSERT_TRUE(Connect());
}

// Create two channels with DTLS, negotiate DTLS-SRTP, and transfer bypass SRTP.
TEST_F(DtlsTransportChannelTest, TestTransferDtlsSrtpTwoChannels) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  SetChannelCount(2);
  PrepareDtls(true, true);
  PrepareDtlsSrtp(true, true);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, true);
  TestTransfer(1, 1000, 100, true);
}

// Create a single channel with DTLS, and send normal data and SRTP data on it.
TEST_F(DtlsTransportChannelTest, TestTransferDtlsSrtpDemux) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  PrepareDtls(true, true);
  PrepareDtlsSrtp(true, true);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, false);
  TestTransfer(0, 1000, 100, true);
}

// Testing when the remote is passive.
TEST_F(DtlsTransportChannelTest, TestTransferDtlsAnswererIsPassive) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  SetChannelCount(2);
  PrepareDtls(true, true);
  PrepareDtlsSrtp(true, true);
  ASSERT_TRUE(Connect(cricket::CONNECTIONROLE_ACTPASS,
                      cricket::CONNECTIONROLE_PASSIVE));
  TestTransfer(0, 1000, 100, true);
  TestTransfer(1, 1000, 100, true);
}

// Testing with the legacy DTLS client which doesn't use setup attribute.
// In this case legacy is the answerer.
TEST_F(DtlsTransportChannelTest, TestDtlsSetupWithLegacyAsAnswerer) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  PrepareDtls(true, true);
  NegotiateWithLegacy();
  rtc::SSLRole channel1_role;
  rtc::SSLRole channel2_role;
  EXPECT_TRUE(client1_.transport()->GetSslRole(&channel1_role));
  EXPECT_TRUE(client2_.transport()->GetSslRole(&channel2_role));
  EXPECT_EQ(rtc::SSL_SERVER, channel1_role);
  EXPECT_EQ(rtc::SSL_CLIENT, channel2_role);
}

// Testing re offer/answer after the session is estbalished. Roles will be
// kept same as of the previous negotiation.
TEST_F(DtlsTransportChannelTest, TestDtlsReOfferFromOfferer) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  SetChannelCount(2);
  PrepareDtls(true, true);
  PrepareDtlsSrtp(true, true);
  // Initial role for client1 is ACTPASS and client2 is ACTIVE.
  ASSERT_TRUE(Connect(cricket::CONNECTIONROLE_ACTPASS,
                      cricket::CONNECTIONROLE_ACTIVE));
  TestTransfer(0, 1000, 100, true);
  TestTransfer(1, 1000, 100, true);
  // Using input roles for the re-offer.
  Renegotiate(&client1_, cricket::CONNECTIONROLE_ACTPASS,
              cricket::CONNECTIONROLE_ACTIVE, NF_REOFFER);
  TestTransfer(0, 1000, 100, true);
  TestTransfer(1, 1000, 100, true);
}

TEST_F(DtlsTransportChannelTest, TestDtlsReOfferFromAnswerer) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  SetChannelCount(2);
  PrepareDtls(true, true);
  PrepareDtlsSrtp(true, true);
  // Initial role for client1 is ACTPASS and client2 is ACTIVE.
  ASSERT_TRUE(Connect(cricket::CONNECTIONROLE_ACTPASS,
                      cricket::CONNECTIONROLE_ACTIVE));
  TestTransfer(0, 1000, 100, true);
  TestTransfer(1, 1000, 100, true);
  // Using input roles for the re-offer.
  Renegotiate(&client2_, cricket::CONNECTIONROLE_PASSIVE,
              cricket::CONNECTIONROLE_ACTPASS, NF_REOFFER);
  TestTransfer(0, 1000, 100, true);
  TestTransfer(1, 1000, 100, true);
}

// Test that any change in role after the intial setup will result in failure.
TEST_F(DtlsTransportChannelTest, TestDtlsRoleReversal) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  SetChannelCount(2);
  PrepareDtls(true, true);
  PrepareDtlsSrtp(true, true);
  ASSERT_TRUE(Connect(cricket::CONNECTIONROLE_ACTPASS,
                      cricket::CONNECTIONROLE_PASSIVE));

  // Renegotiate from client2 with actpass and client1 as active.
  Renegotiate(&client2_, cricket::CONNECTIONROLE_ACTPASS,
              cricket::CONNECTIONROLE_ACTIVE,
              NF_REOFFER | NF_EXPECT_FAILURE);
}

// Test that using different setup attributes which results in similar ssl
// role as the initial negotiation will result in success.
TEST_F(DtlsTransportChannelTest, TestDtlsReOfferWithDifferentSetupAttr) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  SetChannelCount(2);
  PrepareDtls(true, true);
  PrepareDtlsSrtp(true, true);
  ASSERT_TRUE(Connect(cricket::CONNECTIONROLE_ACTPASS,
                      cricket::CONNECTIONROLE_PASSIVE));
  // Renegotiate from client2 with actpass and client1 as active.
  Renegotiate(&client2_, cricket::CONNECTIONROLE_ACTIVE,
              cricket::CONNECTIONROLE_ACTPASS, NF_REOFFER);
  TestTransfer(0, 1000, 100, true);
  TestTransfer(1, 1000, 100, true);
}

// Test that re-negotiation can be started before the clients become connected
// in the first negotiation.
TEST_F(DtlsTransportChannelTest, TestRenegotiateBeforeConnect) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  SetChannelCount(2);
  PrepareDtls(true, true);
  PrepareDtlsSrtp(true, true);
  Negotiate();

  Renegotiate(&client1_, cricket::CONNECTIONROLE_ACTPASS,
              cricket::CONNECTIONROLE_ACTIVE, NF_REOFFER);
  bool rv = client1_.Connect(&client2_);
  EXPECT_TRUE(rv);
  EXPECT_TRUE_WAIT(client1_.writable() && client2_.writable(), 10000);

  TestTransfer(0, 1000, 100, true);
  TestTransfer(1, 1000, 100, true);
}

// Test Certificates state after negotiation but before connection.
TEST_F(DtlsTransportChannelTest, TestCertificatesBeforeConnect) {
  MAYBE_SKIP_TEST(HaveDtls);
  PrepareDtls(true, true);
  Negotiate();

  rtc::scoped_ptr<rtc::SSLIdentity> identity1;
  rtc::scoped_ptr<rtc::SSLIdentity> identity2;
  rtc::scoped_ptr<rtc::SSLCertificate> remote_cert1;
  rtc::scoped_ptr<rtc::SSLCertificate> remote_cert2;

  // After negotiation, each side has a distinct local certificate, but still no
  // remote certificate, because connection has not yet occurred.
  ASSERT_TRUE(client1_.transport()->GetIdentity(identity1.accept()));
  ASSERT_TRUE(client2_.transport()->GetIdentity(identity2.accept()));
  ASSERT_NE(identity1->certificate().ToPEMString(),
            identity2->certificate().ToPEMString());
  ASSERT_FALSE(
      client1_.transport()->GetRemoteCertificate(remote_cert1.accept()));
  ASSERT_FALSE(remote_cert1 != NULL);
  ASSERT_FALSE(
      client2_.transport()->GetRemoteCertificate(remote_cert2.accept()));
  ASSERT_FALSE(remote_cert2 != NULL);
}

// Test Certificates state after connection.
TEST_F(DtlsTransportChannelTest, TestCertificatesAfterConnect) {
  MAYBE_SKIP_TEST(HaveDtls);
  PrepareDtls(true, true);
  ASSERT_TRUE(Connect());

  rtc::scoped_ptr<rtc::SSLIdentity> identity1;
  rtc::scoped_ptr<rtc::SSLIdentity> identity2;
  rtc::scoped_ptr<rtc::SSLCertificate> remote_cert1;
  rtc::scoped_ptr<rtc::SSLCertificate> remote_cert2;

  // After connection, each side has a distinct local certificate.
  ASSERT_TRUE(client1_.transport()->GetIdentity(identity1.accept()));
  ASSERT_TRUE(client2_.transport()->GetIdentity(identity2.accept()));
  ASSERT_NE(identity1->certificate().ToPEMString(),
            identity2->certificate().ToPEMString());

  // Each side's remote certificate is the other side's local certificate.
  ASSERT_TRUE(
      client1_.transport()->GetRemoteCertificate(remote_cert1.accept()));
  ASSERT_EQ(remote_cert1->ToPEMString(),
            identity2->certificate().ToPEMString());
  ASSERT_TRUE(
      client2_.transport()->GetRemoteCertificate(remote_cert2.accept()));
  ASSERT_EQ(remote_cert2->ToPEMString(),
            identity1->certificate().ToPEMString());
}
