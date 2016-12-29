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
#include "webrtc/p2p/base/faketransportcontroller.h"
#include "webrtc/base/common.h"
#include "webrtc/base/dscp.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/ssladapter.h"
#include "webrtc/base/sslidentity.h"
#include "webrtc/base/sslstreamadapter.h"
#include "webrtc/base/stringutils.h"

#define MAYBE_SKIP_TEST(feature)                    \
  if (!(rtc::SSLStreamAdapter::feature())) {  \
    LOG(LS_INFO) << "Feature disabled... skipping"; \
    return;                                         \
  }

static const char kIceUfrag1[] = "TESTICEUFRAG0001";
static const char kIcePwd1[] = "TESTICEPWD00000000000001";
static const size_t kPacketNumOffset = 8;
static const size_t kPacketHeaderLen = 12;
static const int kFakePacketId = 0x1234;

static bool IsRtpLeadByte(uint8_t b) {
  return ((b & 0xC0) == 0x80);
}

using cricket::ConnectionRole;

enum Flags { NF_REOFFER = 0x1, NF_EXPECT_FAILURE = 0x2 };

class DtlsTestClient : public sigslot::has_slots<> {
 public:
  DtlsTestClient(const std::string& name)
      : name_(name),
        packet_size_(0),
        use_dtls_srtp_(false),
        ssl_max_version_(rtc::SSL_PROTOCOL_DTLS_12),
        negotiated_dtls_(false),
        received_dtls_client_hello_(false),
        received_dtls_server_hello_(false) {}
  void CreateCertificate(rtc::KeyType key_type) {
    certificate_ =
        rtc::RTCCertificate::Create(rtc::scoped_ptr<rtc::SSLIdentity>(
            rtc::SSLIdentity::Generate(name_, key_type)));
  }
  const rtc::scoped_refptr<rtc::RTCCertificate>& certificate() {
    return certificate_;
  }
  void SetupSrtp() {
    ASSERT(certificate_);
    use_dtls_srtp_ = true;
  }
  void SetupMaxProtocolVersion(rtc::SSLProtocolVersion version) {
    ASSERT(!transport_);
    ssl_max_version_ = version;
  }
  void SetupChannels(int count, cricket::IceRole role) {
    transport_.reset(new cricket::DtlsTransport<cricket::FakeTransport>(
        "dtls content name", nullptr, certificate_));
    transport_->SetAsync(true);
    transport_->SetIceRole(role);
    transport_->SetIceTiebreaker(
        (role == cricket::ICEROLE_CONTROLLING) ? 1 : 2);

    for (int i = 0; i < count; ++i) {
      cricket::DtlsTransportChannelWrapper* channel =
          static_cast<cricket::DtlsTransportChannelWrapper*>(
              transport_->CreateChannel(i));
      ASSERT_TRUE(channel != NULL);
      channel->SetSslMaxProtocolVersion(ssl_max_version_);
      channel->SignalWritableState.connect(this,
        &DtlsTestClient::OnTransportChannelWritableState);
      channel->SignalReadPacket.connect(this,
        &DtlsTestClient::OnTransportChannelReadPacket);
      channel->SignalSentPacket.connect(
          this, &DtlsTestClient::OnTransportChannelSentPacket);
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
    Negotiate(certificate_, certificate_ ? peer->certificate_ : nullptr, action,
              local_role, remote_role, flags);
  }

  // Allow any DTLS configuration to be specified (including invalid ones).
  void Negotiate(const rtc::scoped_refptr<rtc::RTCCertificate>& local_cert,
                 const rtc::scoped_refptr<rtc::RTCCertificate>& remote_cert,
                 cricket::ContentAction action,
                 ConnectionRole local_role,
                 ConnectionRole remote_role,
                 int flags) {
    rtc::scoped_ptr<rtc::SSLFingerprint> local_fingerprint;
    rtc::scoped_ptr<rtc::SSLFingerprint> remote_fingerprint;
    if (local_cert) {
      std::string digest_algorithm;
      ASSERT_TRUE(local_cert->ssl_certificate().GetSignatureDigestAlgorithm(
          &digest_algorithm));
      ASSERT_FALSE(digest_algorithm.empty());
      local_fingerprint.reset(rtc::SSLFingerprint::Create(
          digest_algorithm, local_cert->identity()));
      ASSERT_TRUE(local_fingerprint.get() != NULL);
      EXPECT_EQ(rtc::DIGEST_SHA_256, digest_algorithm);
    }
    if (remote_cert) {
      std::string digest_algorithm;
      ASSERT_TRUE(remote_cert->ssl_certificate().GetSignatureDigestAlgorithm(
          &digest_algorithm));
      ASSERT_FALSE(digest_algorithm.empty());
      remote_fingerprint.reset(rtc::SSLFingerprint::Create(
          digest_algorithm, remote_cert->identity()));
      ASSERT_TRUE(remote_fingerprint.get() != NULL);
      EXPECT_EQ(rtc::DIGEST_SHA_256, digest_algorithm);
    }

    if (use_dtls_srtp_ && !(flags & NF_REOFFER)) {
      // SRTP ciphers will be set only in the beginning.
      for (std::vector<cricket::DtlsTransportChannelWrapper*>::iterator it =
           channels_.begin(); it != channels_.end(); ++it) {
        std::vector<int> ciphers;
        ciphers.push_back(rtc::SRTP_AES128_CM_SHA1_80);
        ASSERT_TRUE((*it)->SetSrtpCryptoSuites(ciphers));
      }
    }

    cricket::TransportDescription local_desc(
        std::vector<std::string>(), kIceUfrag1, kIcePwd1, cricket::ICEMODE_FULL,
        local_role,
        // If remote if the offerer and has no DTLS support, answer will be
        // without any fingerprint.
        (action == cricket::CA_ANSWER && !remote_cert)
            ? nullptr
            : local_fingerprint.get(),
        cricket::Candidates());

    cricket::TransportDescription remote_desc(
        std::vector<std::string>(), kIceUfrag1, kIcePwd1, cricket::ICEMODE_FULL,
        remote_role, remote_fingerprint.get(), cricket::Candidates());

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
    negotiated_dtls_ = (local_cert && remote_cert);
  }

  bool Connect(DtlsTestClient* peer) {
    transport_->ConnectChannels();
    transport_->SetDestination(peer->transport_.get());
    return true;
  }

  bool all_channels_writable() const {
    if (channels_.empty()) {
      return false;
    }
    for (cricket::DtlsTransportChannelWrapper* channel : channels_) {
      if (!channel->writable()) {
        return false;
      }
    }
    return true;
  }

  void CheckRole(rtc::SSLRole role) {
    if (role == rtc::SSL_CLIENT) {
      ASSERT_FALSE(received_dtls_client_hello_);
      ASSERT_TRUE(received_dtls_server_hello_);
    } else {
      ASSERT_TRUE(received_dtls_client_hello_);
      ASSERT_FALSE(received_dtls_server_hello_);
    }
  }

  void CheckSrtp(int expected_crypto_suite) {
    for (std::vector<cricket::DtlsTransportChannelWrapper*>::iterator it =
           channels_.begin(); it != channels_.end(); ++it) {
      int crypto_suite;

      bool rv = (*it)->GetSrtpCryptoSuite(&crypto_suite);
      if (negotiated_dtls_ && expected_crypto_suite) {
        ASSERT_TRUE(rv);

        ASSERT_EQ(crypto_suite, expected_crypto_suite);
      } else {
        ASSERT_FALSE(rv);
      }
    }
  }

  void CheckSsl(int expected_cipher) {
    for (std::vector<cricket::DtlsTransportChannelWrapper*>::iterator it =
           channels_.begin(); it != channels_.end(); ++it) {
      int cipher;

      bool rv = (*it)->GetSslCipherSuite(&cipher);
      if (negotiated_dtls_ && expected_cipher) {
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
                   static_cast<uint32_t>(sent));

      // Only set the bypass flag if we've activated DTLS.
      int flags = (certificate_ && srtp) ? cricket::PF_SRTP_BYPASS : 0;
      rtc::PacketOptions packet_options;
      packet_options.packet_id = kFakePacketId;
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

  bool VerifyPacket(const char* data, size_t size, uint32_t* out_num) {
    if (size != packet_size_ ||
        (data[0] != 0 && static_cast<uint8_t>(data[0]) != 0x80)) {
      return false;
    }
    uint32_t packet_num = rtc::GetBE32(data + kPacketNumOffset);
    for (size_t i = kPacketHeaderLen; i < size; ++i) {
      if (static_cast<uint8_t>(data[i]) != (packet_num & 0xff)) {
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
    uint32_t packet_num = rtc::GetBE32(data + kPacketNumOffset);
    int num_matches = 0;
    for (size_t i = kPacketNumOffset; i < size; ++i) {
      if (static_cast<uint8_t>(data[i]) == (packet_num & 0xff)) {
        ++num_matches;
      }
    }
    return (num_matches < ((static_cast<int>(size) - 5) / 10));
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
    uint32_t packet_num = 0;
    ASSERT_TRUE(VerifyPacket(data, size, &packet_num));
    received_.insert(packet_num);
    // Only DTLS-SRTP packets should have the bypass flag set.
    int expected_flags =
        (certificate_ && IsRtpLeadByte(data[0])) ? cricket::PF_SRTP_BYPASS : 0;
    ASSERT_EQ(expected_flags, flags);
  }

  void OnTransportChannelSentPacket(cricket::TransportChannel* channel,
                                    const rtc::SentPacket& sent_packet) {
    sent_packet_ = sent_packet;
  }

  rtc::SentPacket sent_packet() const { return sent_packet_; }

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
  rtc::scoped_refptr<rtc::RTCCertificate> certificate_;
  rtc::scoped_ptr<cricket::FakeTransport> transport_;
  std::vector<cricket::DtlsTransportChannelWrapper*> channels_;
  size_t packet_size_;
  std::set<int> received_;
  bool use_dtls_srtp_;
  rtc::SSLProtocolVersion ssl_max_version_;
  bool negotiated_dtls_;
  bool received_dtls_client_hello_;
  bool received_dtls_server_hello_;
  rtc::SentPacket sent_packet_;
};


class DtlsTransportChannelTest : public testing::Test {
 public:
  DtlsTransportChannelTest()
      : client1_("P1"),
        client2_("P2"),
        channel_ct_(1),
        use_dtls_(false),
        use_dtls_srtp_(false),
        ssl_expected_version_(rtc::SSL_PROTOCOL_DTLS_12) {}

  void SetChannelCount(size_t channel_ct) {
    channel_ct_ = static_cast<int>(channel_ct);
  }
  void SetMaxProtocolVersions(rtc::SSLProtocolVersion c1,
                              rtc::SSLProtocolVersion c2) {
    client1_.SetupMaxProtocolVersion(c1);
    client2_.SetupMaxProtocolVersion(c2);
    ssl_expected_version_ = std::min(c1, c2);
  }
  void PrepareDtls(bool c1, bool c2, rtc::KeyType key_type) {
    if (c1) {
      client1_.CreateCertificate(key_type);
    }
    if (c2) {
      client2_.CreateCertificate(key_type);
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

    EXPECT_TRUE_WAIT(
        client1_.all_channels_writable() && client2_.all_channels_writable(),
        10000);
    if (!client1_.all_channels_writable() || !client2_.all_channels_writable())
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
      client1_.CheckSrtp(rtc::SRTP_AES128_CM_SHA1_80);
      client2_.CheckSrtp(rtc::SRTP_AES128_CM_SHA1_80);
    } else {
      client1_.CheckSrtp(rtc::SRTP_INVALID_CRYPTO_SUITE);
      client2_.CheckSrtp(rtc::SRTP_INVALID_CRYPTO_SUITE);
    }
    client1_.CheckSsl(rtc::SSLStreamAdapter::GetDefaultSslCipherForTest(
        ssl_expected_version_, rtc::KT_DEFAULT));
    client2_.CheckSsl(rtc::SSLStreamAdapter::GetDefaultSslCipherForTest(
        ssl_expected_version_, rtc::KT_DEFAULT));

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
  rtc::SSLProtocolVersion ssl_expected_version_;
};

// Test that transport negotiation of ICE, no DTLS works properly.
TEST_F(DtlsTransportChannelTest, TestChannelSetupIce) {
  Negotiate();
  cricket::FakeTransportChannel* channel1 = client1_.GetFakeChannel(0);
  cricket::FakeTransportChannel* channel2 = client2_.GetFakeChannel(0);
  ASSERT_TRUE(channel1 != NULL);
  ASSERT_TRUE(channel2 != NULL);
  EXPECT_EQ(cricket::ICEROLE_CONTROLLING, channel1->GetIceRole());
  EXPECT_EQ(1U, channel1->IceTiebreaker());
  EXPECT_EQ(kIceUfrag1, channel1->ice_ufrag());
  EXPECT_EQ(kIcePwd1, channel1->ice_pwd());
  EXPECT_EQ(cricket::ICEROLE_CONTROLLED, channel2->GetIceRole());
  EXPECT_EQ(2U, channel2->IceTiebreaker());
}

// Connect without DTLS, and transfer some data.
TEST_F(DtlsTransportChannelTest, TestTransfer) {
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, false);
}

// Connect without DTLS, and transfer some data.
TEST_F(DtlsTransportChannelTest, TestOnSentPacket) {
  ASSERT_TRUE(Connect());
  EXPECT_EQ(client1_.sent_packet().send_time_ms, -1);
  TestTransfer(0, 1000, 100, false);
  EXPECT_EQ(kFakePacketId, client1_.sent_packet().packet_id);
  EXPECT_GE(client1_.sent_packet().send_time_ms, 0);
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

#if defined(MEMORY_SANITIZER)
// Fails under MemorySanitizer:
// See https://code.google.com/p/webrtc/issues/detail?id=5381.
#define MAYBE_TestTransferDtls DISABLED_TestTransferDtls
#else
#define MAYBE_TestTransferDtls TestTransferDtls
#endif
// Connect with DTLS, and transfer some data.
TEST_F(DtlsTransportChannelTest, MAYBE_TestTransferDtls) {
  MAYBE_SKIP_TEST(HaveDtls);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, false);
}

#if defined(MEMORY_SANITIZER)
// Fails under MemorySanitizer:
// See https://code.google.com/p/webrtc/issues/detail?id=5381.
#define MAYBE_TestTransferDtlsTwoChannels DISABLED_TestTransferDtlsTwoChannels
#else
#define MAYBE_TestTransferDtlsTwoChannels TestTransferDtlsTwoChannels
#endif
// Create two channels with DTLS, and transfer some data.
TEST_F(DtlsTransportChannelTest, MAYBE_TestTransferDtlsTwoChannels) {
  MAYBE_SKIP_TEST(HaveDtls);
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, false);
  TestTransfer(1, 1000, 100, false);
}

// Connect with A doing DTLS and B not, and transfer some data.
TEST_F(DtlsTransportChannelTest, TestTransferDtlsRejected) {
  PrepareDtls(true, false, rtc::KT_DEFAULT);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, false);
}

// Connect with B doing DTLS and A not, and transfer some data.
TEST_F(DtlsTransportChannelTest, TestTransferDtlsNotOffered) {
  PrepareDtls(false, true, rtc::KT_DEFAULT);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, false);
}

// Create two channels with DTLS 1.0 and check ciphers.
TEST_F(DtlsTransportChannelTest, TestDtls12None) {
  MAYBE_SKIP_TEST(HaveDtls);
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  SetMaxProtocolVersions(rtc::SSL_PROTOCOL_DTLS_10, rtc::SSL_PROTOCOL_DTLS_10);
  ASSERT_TRUE(Connect());
}

// Create two channels with DTLS 1.2 and check ciphers.
TEST_F(DtlsTransportChannelTest, TestDtls12Both) {
  MAYBE_SKIP_TEST(HaveDtls);
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  SetMaxProtocolVersions(rtc::SSL_PROTOCOL_DTLS_12, rtc::SSL_PROTOCOL_DTLS_12);
  ASSERT_TRUE(Connect());
}

// Create two channels with DTLS 1.0 / DTLS 1.2 and check ciphers.
TEST_F(DtlsTransportChannelTest, TestDtls12Client1) {
  MAYBE_SKIP_TEST(HaveDtls);
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  SetMaxProtocolVersions(rtc::SSL_PROTOCOL_DTLS_12, rtc::SSL_PROTOCOL_DTLS_10);
  ASSERT_TRUE(Connect());
}

// Create two channels with DTLS 1.2 / DTLS 1.0 and check ciphers.
TEST_F(DtlsTransportChannelTest, TestDtls12Client2) {
  MAYBE_SKIP_TEST(HaveDtls);
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  SetMaxProtocolVersions(rtc::SSL_PROTOCOL_DTLS_10, rtc::SSL_PROTOCOL_DTLS_12);
  ASSERT_TRUE(Connect());
}

#if defined(MEMORY_SANITIZER)
// Fails under MemorySanitizer:
// See https://code.google.com/p/webrtc/issues/detail?id=5381.
#define MAYBE_TestTransferDtlsSrtp DISABLED_TestTransferDtlsSrtp
#else
#define MAYBE_TestTransferDtlsSrtp TestTransferDtlsSrtp
#endif
// Connect with DTLS, negotiate DTLS-SRTP, and transfer SRTP using bypass.
TEST_F(DtlsTransportChannelTest, MAYBE_TestTransferDtlsSrtp) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  PrepareDtlsSrtp(true, true);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, true);
}

#if defined(MEMORY_SANITIZER)
// Fails under MemorySanitizer:
// See https://code.google.com/p/webrtc/issues/detail?id=5381.
#define MAYBE_TestTransferDtlsInvalidSrtpPacket \
  DISABLED_TestTransferDtlsInvalidSrtpPacket
#else
#define MAYBE_TestTransferDtlsInvalidSrtpPacket \
  TestTransferDtlsInvalidSrtpPacket
#endif
// Connect with DTLS-SRTP, transfer an invalid SRTP packet, and expects -1
// returned.
TEST_F(DtlsTransportChannelTest, MAYBE_TestTransferDtlsInvalidSrtpPacket) {
  MAYBE_SKIP_TEST(HaveDtls);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  PrepareDtlsSrtp(true, true);
  ASSERT_TRUE(Connect());
  int result = client1_.SendInvalidSrtpPacket(0, 100);
  ASSERT_EQ(-1, result);
}

#if defined(MEMORY_SANITIZER)
// Fails under MemorySanitizer:
// See https://code.google.com/p/webrtc/issues/detail?id=5381.
#define MAYBE_TestTransferDtlsSrtpRejected DISABLED_TestTransferDtlsSrtpRejected
#else
#define MAYBE_TestTransferDtlsSrtpRejected TestTransferDtlsSrtpRejected
#endif
// Connect with DTLS. A does DTLS-SRTP but B does not.
TEST_F(DtlsTransportChannelTest, MAYBE_TestTransferDtlsSrtpRejected) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  PrepareDtlsSrtp(true, false);
  ASSERT_TRUE(Connect());
}

#if defined(MEMORY_SANITIZER)
// Fails under MemorySanitizer:
// See https://code.google.com/p/webrtc/issues/detail?id=5381.
#define MAYBE_TestTransferDtlsSrtpNotOffered \
  DISABLED_TestTransferDtlsSrtpNotOffered
#else
#define MAYBE_TestTransferDtlsSrtpNotOffered TestTransferDtlsSrtpNotOffered
#endif
// Connect with DTLS. B does DTLS-SRTP but A does not.
TEST_F(DtlsTransportChannelTest, MAYBE_TestTransferDtlsSrtpNotOffered) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  PrepareDtlsSrtp(false, true);
  ASSERT_TRUE(Connect());
}

#if defined(MEMORY_SANITIZER)
// Fails under MemorySanitizer:
// See https://code.google.com/p/webrtc/issues/detail?id=5381.
#define MAYBE_TestTransferDtlsSrtpTwoChannels \
  DISABLED_TestTransferDtlsSrtpTwoChannels
#else
#define MAYBE_TestTransferDtlsSrtpTwoChannels TestTransferDtlsSrtpTwoChannels
#endif
// Create two channels with DTLS, negotiate DTLS-SRTP, and transfer bypass SRTP.
TEST_F(DtlsTransportChannelTest, MAYBE_TestTransferDtlsSrtpTwoChannels) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  PrepareDtlsSrtp(true, true);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, true);
  TestTransfer(1, 1000, 100, true);
}

#if defined(MEMORY_SANITIZER)
// Fails under MemorySanitizer:
// See https://code.google.com/p/webrtc/issues/detail?id=5381.
#define MAYBE_TestTransferDtlsSrtpDemux DISABLED_TestTransferDtlsSrtpDemux
#else
#define MAYBE_TestTransferDtlsSrtpDemux TestTransferDtlsSrtpDemux
#endif
// Create a single channel with DTLS, and send normal data and SRTP data on it.
TEST_F(DtlsTransportChannelTest, MAYBE_TestTransferDtlsSrtpDemux) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  PrepareDtlsSrtp(true, true);
  ASSERT_TRUE(Connect());
  TestTransfer(0, 1000, 100, false);
  TestTransfer(0, 1000, 100, true);
}

#if defined(MEMORY_SANITIZER)
// Fails under MemorySanitizer:
// See https://code.google.com/p/webrtc/issues/detail?id=5381.
#define MAYBE_TestTransferDtlsAnswererIsPassive \
  DISABLED_TestTransferDtlsAnswererIsPassive
#else
#define MAYBE_TestTransferDtlsAnswererIsPassive \
  TestTransferDtlsAnswererIsPassive
#endif
// Testing when the remote is passive.
TEST_F(DtlsTransportChannelTest, MAYBE_TestTransferDtlsAnswererIsPassive) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
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
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  NegotiateWithLegacy();
  rtc::SSLRole channel1_role;
  rtc::SSLRole channel2_role;
  EXPECT_TRUE(client1_.transport()->GetSslRole(&channel1_role));
  EXPECT_TRUE(client2_.transport()->GetSslRole(&channel2_role));
  EXPECT_EQ(rtc::SSL_SERVER, channel1_role);
  EXPECT_EQ(rtc::SSL_CLIENT, channel2_role);
}

#if defined(MEMORY_SANITIZER)
// Fails under MemorySanitizer:
// See https://code.google.com/p/webrtc/issues/detail?id=5381.
#define MAYBE_TestDtlsReOfferFromOfferer DISABLED_TestDtlsReOfferFromOfferer
#else
#define MAYBE_TestDtlsReOfferFromOfferer TestDtlsReOfferFromOfferer
#endif
// Testing re offer/answer after the session is estbalished. Roles will be
// kept same as of the previous negotiation.
TEST_F(DtlsTransportChannelTest, MAYBE_TestDtlsReOfferFromOfferer) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
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

#if defined(MEMORY_SANITIZER)
// Fails under MemorySanitizer:
// See https://code.google.com/p/webrtc/issues/detail?id=5381.
#define MAYBE_TestDtlsReOfferFromAnswerer DISABLED_TestDtlsReOfferFromAnswerer
#else
#define MAYBE_TestDtlsReOfferFromAnswerer TestDtlsReOfferFromAnswerer
#endif
TEST_F(DtlsTransportChannelTest, MAYBE_TestDtlsReOfferFromAnswerer) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
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

#if defined(MEMORY_SANITIZER)
// Fails under MemorySanitizer:
// See https://code.google.com/p/webrtc/issues/detail?id=5381.
#define MAYBE_TestDtlsRoleReversal DISABLED_TestDtlsRoleReversal
#else
#define MAYBE_TestDtlsRoleReversal TestDtlsRoleReversal
#endif
// Test that any change in role after the intial setup will result in failure.
TEST_F(DtlsTransportChannelTest, MAYBE_TestDtlsRoleReversal) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  PrepareDtlsSrtp(true, true);
  ASSERT_TRUE(Connect(cricket::CONNECTIONROLE_ACTPASS,
                      cricket::CONNECTIONROLE_PASSIVE));

  // Renegotiate from client2 with actpass and client1 as active.
  Renegotiate(&client2_, cricket::CONNECTIONROLE_ACTPASS,
              cricket::CONNECTIONROLE_ACTIVE,
              NF_REOFFER | NF_EXPECT_FAILURE);
}

#if defined(MEMORY_SANITIZER)
// Fails under MemorySanitizer:
// See https://code.google.com/p/webrtc/issues/detail?id=5381.
#define MAYBE_TestDtlsReOfferWithDifferentSetupAttr \
  DISABLED_TestDtlsReOfferWithDifferentSetupAttr
#else
#define MAYBE_TestDtlsReOfferWithDifferentSetupAttr \
  TestDtlsReOfferWithDifferentSetupAttr
#endif
// Test that using different setup attributes which results in similar ssl
// role as the initial negotiation will result in success.
TEST_F(DtlsTransportChannelTest, MAYBE_TestDtlsReOfferWithDifferentSetupAttr) {
  MAYBE_SKIP_TEST(HaveDtlsSrtp);
  SetChannelCount(2);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
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
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  PrepareDtlsSrtp(true, true);
  Negotiate();

  Renegotiate(&client1_, cricket::CONNECTIONROLE_ACTPASS,
              cricket::CONNECTIONROLE_ACTIVE, NF_REOFFER);
  bool rv = client1_.Connect(&client2_);
  EXPECT_TRUE(rv);
  EXPECT_TRUE_WAIT(
      client1_.all_channels_writable() && client2_.all_channels_writable(),
      10000);

  TestTransfer(0, 1000, 100, true);
  TestTransfer(1, 1000, 100, true);
}

// Test Certificates state after negotiation but before connection.
TEST_F(DtlsTransportChannelTest, TestCertificatesBeforeConnect) {
  MAYBE_SKIP_TEST(HaveDtls);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  Negotiate();

  rtc::scoped_refptr<rtc::RTCCertificate> certificate1;
  rtc::scoped_refptr<rtc::RTCCertificate> certificate2;
  rtc::scoped_ptr<rtc::SSLCertificate> remote_cert1;
  rtc::scoped_ptr<rtc::SSLCertificate> remote_cert2;

  // After negotiation, each side has a distinct local certificate, but still no
  // remote certificate, because connection has not yet occurred.
  ASSERT_TRUE(client1_.transport()->GetLocalCertificate(&certificate1));
  ASSERT_TRUE(client2_.transport()->GetLocalCertificate(&certificate2));
  ASSERT_NE(certificate1->ssl_certificate().ToPEMString(),
            certificate2->ssl_certificate().ToPEMString());
  ASSERT_FALSE(
      client1_.transport()->GetRemoteSSLCertificate(remote_cert1.accept()));
  ASSERT_FALSE(remote_cert1 != NULL);
  ASSERT_FALSE(
      client2_.transport()->GetRemoteSSLCertificate(remote_cert2.accept()));
  ASSERT_FALSE(remote_cert2 != NULL);
}

#if defined(MEMORY_SANITIZER)
// Fails under MemorySanitizer:
// See https://code.google.com/p/webrtc/issues/detail?id=5381.
#define MAYBE_TestCertificatesAfterConnect DISABLED_TestCertificatesAfterConnect
#else
#define MAYBE_TestCertificatesAfterConnect TestCertificatesAfterConnect
#endif
// Test Certificates state after connection.
TEST_F(DtlsTransportChannelTest, MAYBE_TestCertificatesAfterConnect) {
  MAYBE_SKIP_TEST(HaveDtls);
  PrepareDtls(true, true, rtc::KT_DEFAULT);
  ASSERT_TRUE(Connect());

  rtc::scoped_refptr<rtc::RTCCertificate> certificate1;
  rtc::scoped_refptr<rtc::RTCCertificate> certificate2;
  rtc::scoped_ptr<rtc::SSLCertificate> remote_cert1;
  rtc::scoped_ptr<rtc::SSLCertificate> remote_cert2;

  // After connection, each side has a distinct local certificate.
  ASSERT_TRUE(client1_.transport()->GetLocalCertificate(&certificate1));
  ASSERT_TRUE(client2_.transport()->GetLocalCertificate(&certificate2));
  ASSERT_NE(certificate1->ssl_certificate().ToPEMString(),
            certificate2->ssl_certificate().ToPEMString());

  // Each side's remote certificate is the other side's local certificate.
  ASSERT_TRUE(
      client1_.transport()->GetRemoteSSLCertificate(remote_cert1.accept()));
  ASSERT_EQ(remote_cert1->ToPEMString(),
            certificate2->ssl_certificate().ToPEMString());
  ASSERT_TRUE(
      client2_.transport()->GetRemoteSSLCertificate(remote_cert2.accept()));
  ASSERT_EQ(remote_cert2->ToPEMString(),
            certificate1->ssl_certificate().ToPEMString());
}
