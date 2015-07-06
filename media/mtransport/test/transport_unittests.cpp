/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// Original author: ekr@rtfm.com

#include <iostream>
#include <string>
#include <map>
#include <algorithm>

#include "mozilla/UniquePtr.h"

#include "sigslot.h"

#include "logging.h"
#include "nspr.h"
#include "nss.h"
#include "ssl.h"
#include "sslproto.h"

#include "nsThreadUtils.h"
#include "nsXPCOM.h"

#include "databuffer.h"
#include "dtlsidentity.h"
#include "nricectx.h"
#include "nricemediastream.h"
#include "transportflow.h"
#include "transportlayer.h"
#include "transportlayerdtls.h"
#include "transportlayerice.h"
#include "transportlayerlog.h"
#include "transportlayerloopback.h"

#include "mtransport_test_utils.h"
#include "runnable_utils.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

using namespace mozilla;
MOZ_MTLOG_MODULE("mtransport")

MtransportTestUtils *test_utils;


const uint8_t kTlsChangeCipherSpecType = 0x14;
const uint8_t kTlsHandshakeType =        0x16;

const uint8_t kTlsHandshakeCertificate = 0x0b;
const uint8_t kTlsHandshakeServerKeyExchange = 0x0c;

const uint8_t kTlsFakeChangeCipherSpec[] = {
  kTlsChangeCipherSpecType,  // Type
  0xfe, 0xff, // Version
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, // Fictitious sequence #
  0x00, 0x01, // Length
  0x01 // Value
};

// Layer class which can't be initialized.
class TransportLayerDummy : public TransportLayer {
 public:
  TransportLayerDummy(bool allow_init, bool *destroyed)
      : allow_init_(allow_init),
        destroyed_(destroyed) {
    *destroyed_ = false;
  }

  virtual ~TransportLayerDummy() {
    *destroyed_ = true;
  }

  virtual nsresult InitInternal() {
    return allow_init_ ? NS_OK : NS_ERROR_FAILURE;
  }

  virtual TransportResult SendPacket(const unsigned char *data, size_t len) {
    MOZ_CRASH();  // Should never be called.
    return 0;
  }

  TRANSPORT_LAYER_ID("lossy")

 private:
  bool allow_init_;
  bool *destroyed_;
};

class TransportLayerLossy;

class Inspector {
 public:
  virtual ~Inspector() {}

  virtual void Inspect(TransportLayer* layer,
                       const unsigned char *data, size_t len) = 0;
};

// Class to simulate various kinds of network lossage
class TransportLayerLossy : public TransportLayer {
 public:
  TransportLayerLossy() : loss_mask_(0), packet_(0), inspector_(nullptr) {}
  ~TransportLayerLossy () {}

  virtual TransportResult SendPacket(const unsigned char *data, size_t len) {
    MOZ_MTLOG(ML_NOTICE, LAYER_INFO << "SendPacket(" << len << ")");

    if (loss_mask_ & (1 << (packet_ % 32))) {
      MOZ_MTLOG(ML_NOTICE, "Dropping packet");
      ++packet_;
      return len;
    }
    if (inspector_) {
      inspector_->Inspect(this, data, len);
    }

    ++packet_;

    return downward_->SendPacket(data, len);
  }

  void SetLoss(uint32_t packet) {
    loss_mask_ |= (1 << (packet & 32));
  }

  void SetInspector(UniquePtr<Inspector> inspector) {
    inspector_ = Move(inspector);
  }

  void StateChange(TransportLayer *layer, State state) {
    TL_SET_STATE(state);
  }

  void PacketReceived(TransportLayer *layer, const unsigned char *data,
                      size_t len) {
    SignalPacketReceived(this, data, len);
  }

  TRANSPORT_LAYER_ID("lossy")

 protected:
  virtual void WasInserted() {
    downward_->SignalPacketReceived.
        connect(this,
                &TransportLayerLossy::PacketReceived);
    downward_->SignalStateChange.
        connect(this,
                &TransportLayerLossy::StateChange);

    TL_SET_STATE(downward_->state());
  }

 private:
  uint32_t loss_mask_;
  uint32_t packet_;
  UniquePtr<Inspector> inspector_;
};

// Process DTLS Records
#define CHECK_LENGTH(expected) \
  do { \
    EXPECT_GE(remaining(), expected); \
    if (remaining() < expected) return false; \
  } while(0)

class TlsParser {
 public:
  TlsParser(const unsigned char *data, size_t len)
      : buffer_(data, len), offset_(0) {}

  bool Read(unsigned char* val) {
    if (remaining() < 1) {
      return false;
    }
    *val = *ptr();
    consume(1);
    return true;
  }

  // Read an integral type of specified width.
  bool Read(uint32_t *val, size_t len) {
    if (len > sizeof(uint32_t))
      return false;

    *val = 0;

    for (size_t i=0; i<len; ++i) {
      unsigned char tmp;

      if (!Read(&tmp))
        return false;

      (*val) = ((*val) << 8) + tmp;
    }

    return true;
  }

  bool Read(unsigned char* val, size_t len) {
    if (remaining() < len) {
      return false;
    }

    if (val) {
      memcpy(val, ptr(), len);
    }
    consume(len);

    return true;
  }

 private:
  size_t remaining() const { return buffer_.len() - offset_; }
  const uint8_t *ptr() const { return buffer_.data() + offset_; }
  void consume(size_t len) { offset_ += len; }

  DataBuffer buffer_;
  size_t offset_;
};

class DtlsRecordParser {
 public:
  DtlsRecordParser(const unsigned char *data, size_t len)
      : buffer_(data, len), offset_(0) {}

  bool NextRecord(uint8_t* ct, nsAutoPtr<DataBuffer>* buffer) {
    if (!remaining())
      return false;

    CHECK_LENGTH(13U);
    const uint8_t *ctp = reinterpret_cast<const uint8_t *>(ptr());
    consume(11); // ct + version + length

    const uint16_t *tmp = reinterpret_cast<const uint16_t*>(ptr());
    size_t length = ntohs(*tmp);
    consume(2);

    CHECK_LENGTH(length);
    DataBuffer* db = new DataBuffer(ptr(), length);
    consume(length);

    *ct = *ctp;
    *buffer = db;

    return true;
  }

 private:
  size_t remaining() const { return buffer_.len() - offset_; }
  const uint8_t *ptr() const { return buffer_.data() + offset_; }
  void consume(size_t len) { offset_ += len; }

  DataBuffer buffer_;
  size_t offset_;
};


// Inspector that parses out DTLS records and passes
// them on.
class DtlsRecordInspector : public Inspector {
 public:
  virtual void Inspect(TransportLayer* layer,
                       const unsigned char *data, size_t len) {
    DtlsRecordParser parser(data, len);

    uint8_t ct;
    nsAutoPtr<DataBuffer> buf;
    while(parser.NextRecord(&ct, &buf)) {
      OnRecord(layer, ct, buf->data(), buf->len());
    }
  }

  virtual void OnRecord(TransportLayer* layer,
                        uint8_t content_type,
                        const unsigned char *record,
                        size_t len) = 0;
};

// Inspector that injects arbitrary packets based on
// DTLS records of various types.
class DtlsInspectorInjector : public DtlsRecordInspector {
 public:
  DtlsInspectorInjector(uint8_t packet_type, uint8_t handshake_type,
                    const unsigned char *data, size_t len) :
      packet_type_(packet_type),
      handshake_type_(handshake_type),
      injected_(false) {
    data_.reset(new unsigned char[len]);
    memcpy(data_.get(), data, len);
    len_ = len;
  }

  virtual void OnRecord(TransportLayer* layer,
                        uint8_t content_type,
                        const unsigned char *data, size_t len) {
    // Only inject once.
    if (injected_) {
      return;
    }

    // Check that the first byte is as requested.
    if (content_type != packet_type_) {
      return;
    }

    if (handshake_type_ != 0xff) {
      // Check that the packet is plausibly long enough.
      if (len < 1) {
        return;
      }

      // Check that the handshake type is as requested.
      if (data[0] != handshake_type_) {
        return;
      }
    }

    layer->SendPacket(data_.get(), len_);
  }

 private:
  uint8_t packet_type_;
  uint8_t handshake_type_;
  bool injected_;
  UniquePtr<unsigned char[]> data_;
  size_t len_;
};

// Make a copy of the first instance of a message.
class DtlsInspectorRecordHandshakeMessage : public DtlsRecordInspector {
 public:
  explicit DtlsInspectorRecordHandshakeMessage(uint8_t handshake_type)
      : handshake_type_(handshake_type),
        buffer_() {}

  virtual void OnRecord(TransportLayer* layer,
                        uint8_t content_type,
                        const unsigned char *data, size_t len) {
    // Only do this once.
    if (buffer_.len()) {
      return;
    }

    // Check that the first byte is as requested.
    if (content_type != kTlsHandshakeType) {
      return;
    }

    TlsParser parser(data, len);
    unsigned char message_type;
    // Read the handshake message type.
    if (!parser.Read(&message_type)) {
      return;
    }
    if (message_type != handshake_type_) {
      return;
    }

    uint32_t length;
    if (!parser.Read(&length, 3)) {
      return;
    }

    uint32_t message_seq;
    if (!parser.Read(&message_seq, 2)) {
      return;
    }

    uint32_t fragment_offset;
    if (!parser.Read(&fragment_offset, 3)) {
      return;
    }

    uint32_t fragment_length;
    if (!parser.Read(&fragment_length, 3)) {
      return;
    }

    if ((fragment_offset != 0) || (fragment_length != length)) {
      // This shouldn't happen because all current tests where we
      // are using this code don't fragment.
      return;
    }

    buffer_.Allocate(length);
    if (!parser.Read(buffer_.data(), length)) {
      return;
    }
  }

  const DataBuffer& buffer() { return buffer_; }

 private:
  uint8_t handshake_type_;
  DataBuffer buffer_;
};

class TlsServerKeyExchangeECDHE {
 public:
  bool Parse(const unsigned char* data, size_t len) {
    TlsParser parser(data, len);

    uint8_t curve_type;
    if (!parser.Read(&curve_type)) {
      return false;
    }

    if (curve_type != 3) {  // named_curve
      return false;
    }

    uint32_t named_curve;
    if (!parser.Read(&named_curve, 2)) {
      return false;
    }

    uint32_t point_length;
    if (!parser.Read(&point_length, 1)) {
      return false;
    }

    public_key_.Allocate(point_length);
    if (!parser.Read(public_key_.data(), point_length)) {
      return false;
    }

    return true;
  }

  DataBuffer public_key_;
};

namespace {
class TransportTestPeer : public sigslot::has_slots<> {
 public:
  TransportTestPeer(nsCOMPtr<nsIEventTarget> target, std::string name)
      : name_(name), target_(target),
        received_(0), flow_(new TransportFlow(name)),
        loopback_(new TransportLayerLoopback()),
        logging_(new TransportLayerLogging()),
        lossy_(new TransportLayerLossy()),
        dtls_(new TransportLayerDtls()),
        identity_(DtlsIdentity::Generate()),
        ice_ctx_(NrIceCtx::Create(name,
                                  name == "P2" ?
                                  TransportLayerDtls::CLIENT :
                                  TransportLayerDtls::SERVER)),
        streams_(), candidates_(),
        peer_(nullptr),
        gathering_complete_(false),
        enabled_cipersuites_(),
        disabled_cipersuites_(),
        reuse_dhe_key_(false) {
    std::vector<NrIceStunServer> stun_servers;
    UniquePtr<NrIceStunServer> server(NrIceStunServer::Create(
        std::string((char *)"stun.services.mozilla.com"), 3478));
    stun_servers.push_back(*server);
    EXPECT_TRUE(NS_SUCCEEDED(ice_ctx_->SetStunServers(stun_servers)));

    dtls_->SetIdentity(identity_);
    dtls_->SetRole(name == "P2" ?
                   TransportLayerDtls::CLIENT :
                   TransportLayerDtls::SERVER);

    nsresult res = identity_->ComputeFingerprint("sha-1",
                                             fingerprint_,
                                             sizeof(fingerprint_),
                                             &fingerprint_len_);
    EXPECT_TRUE(NS_SUCCEEDED(res));
    EXPECT_EQ(20u, fingerprint_len_);
  }

  ~TransportTestPeer() {
    test_utils->sts_target()->Dispatch(
      WrapRunnable(this, &TransportTestPeer::DestroyFlow),
      NS_DISPATCH_SYNC);
  }


  void DestroyFlow() {
    if (flow_) {
      loopback_->Disconnect();
      flow_ = nullptr;
    }
    ice_ctx_ = nullptr;
  }

  void DisconnectDestroyFlow() {
    loopback_->Disconnect();
    disconnect_all();  // Disconnect from the signals;
     flow_ = nullptr;
  }

  void SetDtlsAllowAll() {
    nsresult res = dtls_->SetVerificationAllowAll();
    ASSERT_TRUE(NS_SUCCEEDED(res));
  }

  void SetAlpn(std::string str, bool withDefault, std::string extra = "") {
    std::set<std::string> alpn;
    alpn.insert(str); // the one we want to select
    if (!extra.empty()) {
      alpn.insert(extra);
    }
    nsresult res = dtls_->SetAlpn(alpn, withDefault ? str : "");
    ASSERT_EQ(NS_OK, res);
  }

  const std::string& GetAlpn() const {
    return dtls_->GetNegotiatedAlpn();
  }

  void SetDtlsPeer(TransportTestPeer *peer, int digests, unsigned int damage) {
    unsigned int mask = 1;

    for (int i=0; i<digests; i++) {
      unsigned char fingerprint_to_set[TransportLayerDtls::kMaxDigestLength];

      memcpy(fingerprint_to_set,
             peer->fingerprint_,
             peer->fingerprint_len_);
      if (damage & mask)
        fingerprint_to_set[0]++;

      nsresult res = dtls_->SetVerificationDigest(
          "sha-1",
          fingerprint_to_set,
          peer->fingerprint_len_);

      ASSERT_TRUE(NS_SUCCEEDED(res));

      mask <<= 1;
    }
  }

  void SetupSrtp() {
    // this mimics the setup we do elsewhere
    std::vector<uint16_t> srtp_ciphers;
    srtp_ciphers.push_back(SRTP_AES128_CM_HMAC_SHA1_80);
    srtp_ciphers.push_back(SRTP_AES128_CM_HMAC_SHA1_32);

    SetSrtpCiphers(srtp_ciphers);
 }

  void SetSrtpCiphers(std::vector<uint16_t>& srtp_ciphers) {
    ASSERT_TRUE(NS_SUCCEEDED(dtls_->SetSrtpCiphers(srtp_ciphers)));
  }

  void ConnectSocket_s(TransportTestPeer *peer) {
    nsresult res;
    res = loopback_->Init();
    ASSERT_EQ((nsresult)NS_OK, res);

    loopback_->Connect(peer->loopback_);

    ASSERT_EQ((nsresult)NS_OK, flow_->PushLayer(loopback_));
    ASSERT_EQ((nsresult)NS_OK, flow_->PushLayer(logging_));
    ASSERT_EQ((nsresult)NS_OK, flow_->PushLayer(lossy_));
    ASSERT_EQ((nsresult)NS_OK, flow_->PushLayer(dtls_));

    if (dtls_->state() != TransportLayer::TS_ERROR) {
      // Don't execute these blocks if DTLS didn't initialize.
      TweakCiphers(dtls_->internal_fd());
      if (reuse_dhe_key_) {
        // TransportLayerDtls automatically sets this pref to false
        // so set it back for test.
        // This is pretty gross. Dig directly into the NSS FD. The problem
        // is that we are testing a feature which TransaportLayerDtls doesn't
        // expose.
        SECStatus rv = SSL_OptionSet(dtls_->internal_fd(),
                                     SSL_REUSE_SERVER_ECDHE_KEY, PR_TRUE);
        ASSERT_EQ(SECSuccess, rv);
      }
    }

    flow_->SignalPacketReceived.connect(this, &TransportTestPeer::PacketReceived);
  }

  void TweakCiphers(PRFileDesc* fd) {
    for (auto it = enabled_cipersuites_.begin();
         it != enabled_cipersuites_.end(); ++it) {
      SSL_CipherPrefSet(fd, *it, PR_TRUE);
    }
    for (auto it = disabled_cipersuites_.begin();
         it != disabled_cipersuites_.end(); ++it) {
      SSL_CipherPrefSet(fd, *it, PR_FALSE);
    }
  }

  void ConnectSocket(TransportTestPeer *peer) {
    RUN_ON_THREAD(test_utils->sts_target(),
                  WrapRunnable(this, & TransportTestPeer::ConnectSocket_s,
                               peer),
                  NS_DISPATCH_SYNC);
  }

  void InitIce() {
    nsresult res;

    // Attach our slots
    ice_ctx_->SignalGatheringStateChange.
        connect(this, &TransportTestPeer::GatheringStateChange);

    char name[100];
    snprintf(name, sizeof(name), "%s:stream%d", name_.c_str(),
             (int)streams_.size());

    // Create the media stream
    mozilla::RefPtr<NrIceMediaStream> stream =
        ice_ctx_->CreateStream(static_cast<char *>(name), 1);

    ASSERT_TRUE(stream != nullptr);
    ice_ctx_->SetStream(streams_.size(), stream);
    streams_.push_back(stream);

    // Listen for candidates
    stream->SignalCandidate.
        connect(this, &TransportTestPeer::GotCandidate);

    // Create the transport layer
    ice_ = new TransportLayerIce(name);
    ice_->SetParameters(ice_ctx_, stream, 1);

    // Assemble the stack
    nsAutoPtr<std::queue<mozilla::TransportLayer *> > layers(
      new std::queue<mozilla::TransportLayer *>);
    layers->push(ice_);
    layers->push(dtls_);

    test_utils->sts_target()->Dispatch(
      WrapRunnableRet(&res, flow_, &TransportFlow::PushLayers, layers),
      NS_DISPATCH_SYNC);

    ASSERT_EQ((nsresult)NS_OK, res);

    // Listen for media events
    flow_->SignalPacketReceived.connect(this, &TransportTestPeer::PacketReceived);
    flow_->SignalStateChange.connect(this, &TransportTestPeer::StateChanged);

    // Start gathering
    test_utils->sts_target()->Dispatch(
        WrapRunnableRet(&res, ice_ctx_, &NrIceCtx::StartGathering),
        NS_DISPATCH_SYNC);
    ASSERT_TRUE(NS_SUCCEEDED(res));
  }

  void ConnectIce(TransportTestPeer *peer) {
    peer_ = peer;

    // If gathering is already complete, push the candidates over
    if (gathering_complete_)
      GatheringComplete();
  }

  // New candidate
  void GotCandidate(NrIceMediaStream *stream, const std::string &candidate) {
    std::cerr << "Got candidate " << candidate << std::endl;
    candidates_[stream->name()].push_back(candidate);
  }

  void GatheringStateChange(NrIceCtx* ctx,
                            NrIceCtx::GatheringState state) {
    (void)ctx;
    if (state == NrIceCtx::ICE_CTX_GATHER_COMPLETE) {
      GatheringComplete();
    }
  }

  // Gathering complete, so send our candidates and start
  // connecting on the other peer.
  void GatheringComplete() {
    nsresult res;

    // Don't send to the other side
    if (!peer_) {
      gathering_complete_ = true;
      return;
    }

    // First send attributes
    test_utils->sts_target()->Dispatch(
      WrapRunnableRet(&res, peer_->ice_ctx_,
                      &NrIceCtx::ParseGlobalAttributes,
                      ice_ctx_->GetGlobalAttributes()),
      NS_DISPATCH_SYNC);
    ASSERT_TRUE(NS_SUCCEEDED(res));

    for (size_t i=0; i<streams_.size(); ++i) {
      test_utils->sts_target()->Dispatch(
        WrapRunnableRet(&res, peer_->streams_[i], &NrIceMediaStream::ParseAttributes,
                        candidates_[streams_[i]->name()]), NS_DISPATCH_SYNC);

      ASSERT_TRUE(NS_SUCCEEDED(res));
    }

    // Start checks on the other peer.
    test_utils->sts_target()->Dispatch(
      WrapRunnableRet(&res, peer_->ice_ctx_, &NrIceCtx::StartChecks),
      NS_DISPATCH_SYNC);
    ASSERT_TRUE(NS_SUCCEEDED(res));
  }

  TransportResult SendPacket(const unsigned char* data, size_t len) {
    TransportResult ret;
    test_utils->sts_target()->Dispatch(
      WrapRunnableRet(&ret, flow_, &TransportFlow::SendPacket, data, len),
      NS_DISPATCH_SYNC);

    return ret;
  }


  void StateChanged(TransportFlow *flow, TransportLayer::State state) {
    if (state == TransportLayer::TS_OPEN) {
      std::cerr << "Now connected" << std::endl;
    }
  }

  void PacketReceived(TransportFlow * flow, const unsigned char* data,
                      size_t len) {
    std::cerr << "Received " << len << " bytes" << std::endl;
    ++received_;
  }

  void SetLoss(uint32_t loss) {
    lossy_->SetLoss(loss);
  }

  void SetInspector(UniquePtr<Inspector> inspector) {
    lossy_->SetInspector(Move(inspector));
  }

  void SetInspector(Inspector* in) {
    UniquePtr<Inspector> inspector(in);

    lossy_->SetInspector(Move(inspector));
  }

  void SetCipherSuiteChanges(const std::vector<uint16_t>& enableThese,
                             const std::vector<uint16_t>& disableThese) {
    disabled_cipersuites_ = disableThese;
    enabled_cipersuites_ = enableThese;
  }

  void SetReuseECDHEKey() {
    reuse_dhe_key_ = true;
  }

  TransportLayer::State state() {
    TransportLayer::State tstate;

    RUN_ON_THREAD(test_utils->sts_target(),
                  WrapRunnableRet(&tstate, flow_, &TransportFlow::state));

    return tstate;
  }

  bool connected() {
    return state() == TransportLayer::TS_OPEN;
  }

  bool failed() {
    return state() == TransportLayer::TS_ERROR;
  }

  size_t received() { return received_; }

  uint16_t cipherSuite() const {
    nsresult rv;
    uint16_t cipher;
    RUN_ON_THREAD(test_utils->sts_target(),
                  WrapRunnableRet(&rv, dtls_, &TransportLayerDtls::GetCipherSuite,
                                  &cipher));

    if (NS_FAILED(rv)) {
      return TLS_NULL_WITH_NULL_NULL; // i.e., not good
    }
    return cipher;
  }

  uint16_t srtpCipher() const {
    nsresult rv;
    uint16_t cipher;
    RUN_ON_THREAD(test_utils->sts_target(),
                  WrapRunnableRet(&rv, dtls_, &TransportLayerDtls::GetSrtpCipher,
                                  &cipher));
    if (NS_FAILED(rv)) {
      return 0; // the SRTP equivalent of TLS_NULL_WITH_NULL_NULL
    }
    return cipher;
  }

 private:
  std::string name_;
  nsCOMPtr<nsIEventTarget> target_;
  size_t received_;
    mozilla::RefPtr<TransportFlow> flow_;
  TransportLayerLoopback *loopback_;
  TransportLayerLogging *logging_;
  TransportLayerLossy *lossy_;
  TransportLayerDtls *dtls_;
  TransportLayerIce *ice_;
  mozilla::RefPtr<DtlsIdentity> identity_;
  mozilla::RefPtr<NrIceCtx> ice_ctx_;
  std::vector<mozilla::RefPtr<NrIceMediaStream> > streams_;
  std::map<std::string, std::vector<std::string> > candidates_;
  TransportTestPeer *peer_;
  bool gathering_complete_;
  unsigned char fingerprint_[TransportLayerDtls::kMaxDigestLength];
  size_t fingerprint_len_;
  std::vector<uint16_t> enabled_cipersuites_;
  std::vector<uint16_t> disabled_cipersuites_;
  bool reuse_dhe_key_;
};


class TransportTest : public ::testing::Test {
 public:
  TransportTest() {
    fds_[0] = nullptr;
    fds_[1] = nullptr;
  }

  ~TransportTest() {
    delete p1_;
    delete p2_;

    //    Can't detach these
    //    PR_Close(fds_[0]);
    //    PR_Close(fds_[1]);
  }

  void DestroyPeerFlows() {
    p1_->DisconnectDestroyFlow();
    p2_->DisconnectDestroyFlow();
  }

  void SetUp() {
    nsresult rv;
    target_ = do_GetService(NS_SOCKETTRANSPORTSERVICE_CONTRACTID, &rv);
    ASSERT_TRUE(NS_SUCCEEDED(rv));

    Reset();
  }

  void Reset() {
    p1_ = new TransportTestPeer(target_, "P1");
    p2_ = new TransportTestPeer(target_, "P2");
  }

  void SetupSrtp() {
    p1_->SetupSrtp();
    p2_->SetupSrtp();
  }

  void SetDtlsPeer(int digests = 1, unsigned int damage = 0) {
    p1_->SetDtlsPeer(p2_, digests, damage);
    p2_->SetDtlsPeer(p1_, digests, damage);
  }

  void SetDtlsAllowAll() {
    p1_->SetDtlsAllowAll();
    p2_->SetDtlsAllowAll();
  }

  void SetAlpn(std::string first, std::string second,
               bool withDefaults = true) {
    if (!first.empty()) {
      p1_->SetAlpn(first, withDefaults, "bogus");
    }
    if (!second.empty()) {
      p2_->SetAlpn(second, withDefaults);
    }
  }

  void CheckAlpn(std::string first, std::string second) {
    ASSERT_EQ(first, p1_->GetAlpn());
    ASSERT_EQ(second, p2_->GetAlpn());
  }

  void ConnectSocket() {
    ConnectSocketInternal();
    ASSERT_TRUE_WAIT(p1_->connected(), 10000);
    ASSERT_TRUE_WAIT(p2_->connected(), 10000);

    ASSERT_EQ(p1_->cipherSuite(), p2_->cipherSuite());
    ASSERT_EQ(p1_->srtpCipher(), p2_->srtpCipher());
  }

  void ConnectSocketExpectFail() {
    ConnectSocketInternal();
    ASSERT_TRUE_WAIT(p1_->failed(), 10000);
    ASSERT_TRUE_WAIT(p2_->failed(), 10000);
  }

  void ConnectSocketExpectState(TransportLayer::State s1,
                                TransportLayer::State s2) {
    ConnectSocketInternal();
    ASSERT_EQ_WAIT(s1, p1_->state(), 10000);
    ASSERT_EQ_WAIT(s2, p2_->state(), 10000);
  }

  void InitIce() {
    p1_->InitIce();
    p2_->InitIce();
  }

  void ConnectIce() {
    p1_->InitIce();
    p2_->InitIce();
    p1_->ConnectIce(p2_);
    p2_->ConnectIce(p1_);
    ASSERT_TRUE_WAIT(p1_->connected(), 10000);
    ASSERT_TRUE_WAIT(p2_->connected(), 10000);
  }

  void TransferTest(size_t count) {
    unsigned char buf[1000];

    for (size_t i= 0; i<count; ++i) {
      memset(buf, count & 0xff, sizeof(buf));
      TransportResult rv = p1_->SendPacket(buf, sizeof(buf));
      ASSERT_TRUE(rv > 0);
    }

    std::cerr << "Received == " << p2_->received() << std::endl;
    ASSERT_TRUE_WAIT(count == p2_->received(), 10000);
  }

 protected:
  void ConnectSocketInternal() {
    test_utils->sts_target()->Dispatch(
      WrapRunnable(p1_, &TransportTestPeer::ConnectSocket, p2_),
      NS_DISPATCH_SYNC);
    test_utils->sts_target()->Dispatch(
      WrapRunnable(p2_, &TransportTestPeer::ConnectSocket, p1_),
      NS_DISPATCH_SYNC);
  }

  PRFileDesc *fds_[2];
  TransportTestPeer *p1_;
  TransportTestPeer *p2_;
  nsCOMPtr<nsIEventTarget> target_;
};


TEST_F(TransportTest, TestNoDtlsVerificationSettings) {
  ConnectSocketExpectFail();
}

TEST_F(TransportTest, TestConnect) {
  SetDtlsPeer();
  ConnectSocket();

  // check that we got the right suite
  ASSERT_EQ(TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, p1_->cipherSuite());

  // no SRTP on this one
  ASSERT_EQ(0, p1_->srtpCipher());
}

TEST_F(TransportTest, TestConnectSrtp) {
  SetupSrtp();
  SetDtlsPeer();
  ConnectSocket();

  ASSERT_EQ(TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, p1_->cipherSuite());

  // SRTP is on
  ASSERT_EQ(SRTP_AES128_CM_HMAC_SHA1_80, p1_->srtpCipher());
}


TEST_F(TransportTest, TestConnectDestroyFlowsMainThread) {
  SetDtlsPeer();
  ConnectSocket();
  DestroyPeerFlows();
}

TEST_F(TransportTest, TestConnectAllowAll) {
  SetDtlsAllowAll();
  ConnectSocket();
}

TEST_F(TransportTest, TestConnectAlpn) {
  SetDtlsPeer();
  SetAlpn("a", "a");
  ConnectSocket();
  CheckAlpn("a", "a");
}

TEST_F(TransportTest, TestConnectAlpnMismatch) {
  SetDtlsPeer();
  SetAlpn("something", "different");
  ConnectSocketExpectFail();
}

TEST_F(TransportTest, TestConnectAlpnServerDefault) {
  SetDtlsPeer();
  SetAlpn("def", "");
  // server allows default, client doesn't support
  ConnectSocket();
  CheckAlpn("def", "");
}

TEST_F(TransportTest, TestConnectAlpnClientDefault) {
  SetDtlsPeer();
  SetAlpn("", "clientdef");
  // client allows default, but server will ignore the extension
  ConnectSocket();
  CheckAlpn("", "clientdef");
}

TEST_F(TransportTest, TestConnectClientNoAlpn) {
  SetDtlsPeer();
  // Here the server has ALPN, but no default is allowed.
  // Reminder: p1 == server, p2 == client
  SetAlpn("server-nodefault", "", false);
  // The server doesn't see the extension, so negotiates without it.
  // But then the server is forced to close when it discovers that ALPN wasn't
  // negotiated; the client sees a close.
  ConnectSocketExpectState(TransportLayer::TS_ERROR,
                           TransportLayer::TS_CLOSED);
}

TEST_F(TransportTest, TestConnectServerNoAlpn) {
  SetDtlsPeer();
  SetAlpn("", "client-nodefault", false);
  // The client aborts; the server doesn't realize this is a problem and just
  // sees the close.
  ConnectSocketExpectState(TransportLayer::TS_CLOSED,
                           TransportLayer::TS_ERROR);
}

TEST_F(TransportTest, TestConnectNoDigest) {
  SetDtlsPeer(0, 0);

  ConnectSocketExpectFail();
}

TEST_F(TransportTest, TestConnectBadDigest) {
  SetDtlsPeer(1, 1);

  ConnectSocketExpectFail();
}

TEST_F(TransportTest, TestConnectTwoDigests) {
  SetDtlsPeer(2, 0);

  ConnectSocket();
}

TEST_F(TransportTest, TestConnectTwoDigestsFirstBad) {
  SetDtlsPeer(2, 1);

  ConnectSocket();
}

TEST_F(TransportTest, TestConnectTwoDigestsSecondBad) {
  SetDtlsPeer(2, 2);

  ConnectSocket();
}

TEST_F(TransportTest, TestConnectTwoDigestsBothBad) {
  SetDtlsPeer(2, 3);

  ConnectSocketExpectFail();
}

TEST_F(TransportTest, TestConnectInjectCCS) {
  SetDtlsPeer();
  p2_->SetInspector(MakeUnique<DtlsInspectorInjector>(
      kTlsHandshakeType,
      kTlsHandshakeCertificate,
      kTlsFakeChangeCipherSpec,
      sizeof(kTlsFakeChangeCipherSpec)));

  ConnectSocket();
}


TEST_F(TransportTest, TestConnectVerifyNewECDHE) {
  SetDtlsPeer();
  DtlsInspectorRecordHandshakeMessage *i1 = new
    DtlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  p1_->SetInspector(i1);
  ConnectSocket();
  TlsServerKeyExchangeECDHE dhe1;
  ASSERT_TRUE(dhe1.Parse(i1->buffer().data(), i1->buffer().len()));

  Reset();
  SetDtlsPeer();
  DtlsInspectorRecordHandshakeMessage *i2 = new
    DtlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  p1_->SetInspector(i2);
  ConnectSocket();
  TlsServerKeyExchangeECDHE dhe2;
  ASSERT_TRUE(dhe2.Parse(i2->buffer().data(), i2->buffer().len()));

  // Now compare these two to see if they are the same.
  ASSERT_FALSE((dhe1.public_key_.len() == dhe2.public_key_.len()) &&
               (!memcmp(dhe1.public_key_.data(), dhe2.public_key_.data(),
                        dhe1.public_key_.len())));
}

TEST_F(TransportTest, TestConnectVerifyReusedECDHE) {
  SetDtlsPeer();
  DtlsInspectorRecordHandshakeMessage *i1 = new
    DtlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  p1_->SetInspector(i1);
  p1_->SetReuseECDHEKey();
  ConnectSocket();
  TlsServerKeyExchangeECDHE dhe1;
  ASSERT_TRUE(dhe1.Parse(i1->buffer().data(), i1->buffer().len()));

  Reset();
  SetDtlsPeer();
  DtlsInspectorRecordHandshakeMessage *i2 = new
    DtlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);

  p1_->SetInspector(i2);
  p1_->SetReuseECDHEKey();

  ConnectSocket();
  TlsServerKeyExchangeECDHE dhe2;
  ASSERT_TRUE(dhe2.Parse(i2->buffer().data(), i2->buffer().len()));

  // Now compare these two to see if they are the same.
  ASSERT_EQ(dhe1.public_key_.len(), dhe2.public_key_.len());
  ASSERT_TRUE(!memcmp(dhe1.public_key_.data(), dhe2.public_key_.data(),
                      dhe1.public_key_.len()));
}

TEST_F(TransportTest, TestTransfer) {
  SetDtlsPeer();
  ConnectSocket();
  TransferTest(1);
}

TEST_F(TransportTest, TestConnectLoseFirst) {
  SetDtlsPeer();
  p1_->SetLoss(0);
  ConnectSocket();
  TransferTest(1);
}

TEST_F(TransportTest, TestConnectIce) {
  SetDtlsPeer();
  ConnectIce();
}

TEST_F(TransportTest, TestTransferIce) {
  SetDtlsPeer();
  ConnectIce();
  TransferTest(1);
}

// test the default configuration against a peer that supports only
// one of the mandatory-to-implement suites, which should succeed
static void ConfigureOneCipher(TransportTestPeer* peer, uint16_t suite) {
  std::vector<uint16_t> justOne;
  justOne.push_back(suite);
  std::vector<uint16_t> everythingElse(SSL_GetImplementedCiphers(),
                                       SSL_GetImplementedCiphers()
                                       + SSL_GetNumImplementedCiphers());
  std::remove(everythingElse.begin(), everythingElse.end(), suite);
  peer->SetCipherSuiteChanges(justOne, everythingElse);
}

TEST_F(TransportTest, TestCipherMismatch) {
  SetDtlsPeer();
  ConfigureOneCipher(p1_, TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256);
  ConfigureOneCipher(p2_, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA);
  ConnectSocketExpectFail();
}

TEST_F(TransportTest, TestCipherMandatoryOnlyGcm) {
  SetDtlsPeer();
  ConfigureOneCipher(p1_, TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256);
  ConnectSocket();
  ASSERT_EQ(TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256, p1_->cipherSuite());
}

TEST_F(TransportTest, TestCipherMandatoryOnlyCbc) {
  SetDtlsPeer();
  ConfigureOneCipher(p1_, TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA);
  ConnectSocket();
  ASSERT_EQ(TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA, p1_->cipherSuite());
}

TEST_F(TransportTest, TestSrtpMismatch) {
  std::vector<uint16_t> setA;
  setA.push_back(SRTP_AES128_CM_HMAC_SHA1_80);
  std::vector<uint16_t> setB;
  setB.push_back(SRTP_AES128_CM_HMAC_SHA1_32);

  p1_->SetSrtpCiphers(setA);
  p2_->SetSrtpCiphers(setB);
  SetDtlsPeer();
  ConnectSocket();

  ASSERT_EQ(0, p1_->srtpCipher());
  ASSERT_EQ(0, p2_->srtpCipher());
}

// NSS doesn't support DHE suites on the server end.
// This checks to see if we barf when that's the only option available.
TEST_F(TransportTest, TestDheOnlyFails) {
  SetDtlsPeer();

  // p2_ is the client
  // setting this on p1_ (the server) causes NSS to assert
  ConfigureOneCipher(p2_, TLS_DHE_RSA_WITH_AES_128_CBC_SHA);
  ConnectSocketExpectFail();
}

TEST(PushTests, LayerFail) {
  mozilla::RefPtr<TransportFlow> flow = new TransportFlow();
  nsresult rv;
  bool destroyed1, destroyed2;

  rv = flow->PushLayer(new TransportLayerDummy(true, &destroyed1));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  rv = flow->PushLayer(new TransportLayerDummy(false, &destroyed2));
  ASSERT_TRUE(NS_FAILED(rv));

  ASSERT_EQ(TransportLayer::TS_ERROR, flow->state());
  ASSERT_EQ(true, destroyed1);
  ASSERT_EQ(true, destroyed2);

  rv = flow->PushLayer(new TransportLayerDummy(true, &destroyed1));
  ASSERT_TRUE(NS_FAILED(rv));
  ASSERT_EQ(true, destroyed1);
}

TEST(PushTests, LayersFail) {
  mozilla::RefPtr<TransportFlow> flow = new TransportFlow();
  nsresult rv;
  bool destroyed1, destroyed2, destroyed3;

  rv = flow->PushLayer(new TransportLayerDummy(true, &destroyed1));
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  nsAutoPtr<std::queue<TransportLayer *> > layers(
      new std::queue<TransportLayer *>());

  layers->push(new TransportLayerDummy(true, &destroyed2));
  layers->push(new TransportLayerDummy(false, &destroyed3));

  rv = flow->PushLayers(layers);
  ASSERT_TRUE(NS_FAILED(rv));

  ASSERT_EQ(TransportLayer::TS_ERROR, flow->state());
  ASSERT_EQ(true, destroyed1);
  ASSERT_EQ(true, destroyed2);
  ASSERT_EQ(true, destroyed3);

  layers = new std::queue<TransportLayer *>();
  layers->push(new TransportLayerDummy(true, &destroyed2));
  layers->push(new TransportLayerDummy(true, &destroyed3));
  rv = flow->PushLayers(layers);

  ASSERT_TRUE(NS_FAILED(rv));
  ASSERT_EQ(true, destroyed2);
  ASSERT_EQ(true, destroyed3);
}

}  // end namespace

int main(int argc, char **argv)
{
  test_utils = new MtransportTestUtils();

  NSS_NoDB_Init(nullptr);
  NSS_SetDomesticPolicy();
  // Start the tests
  ::testing::InitGoogleTest(&argc, argv);

  int rv = RUN_ALL_TESTS();
  delete test_utils;
  return rv;
}
