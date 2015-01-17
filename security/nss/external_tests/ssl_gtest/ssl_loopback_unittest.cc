#include "prio.h"
#include "prerror.h"
#include "prlog.h"
#include "pk11func.h"
#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"
#include "keyhi.h"

#include <memory>

#include "test_io.h"
#include "tls_parser.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"
#include "gtest_utils.h"

extern std::string g_working_dir_path;

namespace nss_test {

#define LOG(a) std::cerr << name_ << ": " << a << std::endl;

// Inspector that parses out DTLS records and passes
// them on.
class TlsRecordInspector : public Inspector {
 public:
  virtual void Inspect(DummyPrSocket* adapter, const void* data, size_t len) {
    TlsRecordParser parser(static_cast<const unsigned char*>(data), len);

    uint8_t content_type;
    std::auto_ptr<DataBuffer> buf;
    while (parser.NextRecord(&content_type, &buf)) {
      OnRecord(adapter, content_type, buf->data(), buf->len());
    }
  }

  virtual void OnRecord(DummyPrSocket* adapter, uint8_t content_type,
                        const unsigned char* record, size_t len) = 0;
};

// Inspector that injects arbitrary packets based on
// DTLS records of various types.
class TlsInspectorInjector : public TlsRecordInspector {
 public:
  TlsInspectorInjector(uint8_t packet_type, uint8_t handshake_type,
                       const unsigned char* data, size_t len)
      : packet_type_(packet_type),
        handshake_type_(handshake_type),
        injected_(false),
        data_(data, len) {}

  virtual void OnRecord(DummyPrSocket* adapter, uint8_t content_type,
                        const unsigned char* data, size_t len) {
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

    adapter->WriteDirect(data_.data(), data_.len());
  }

 private:
  uint8_t packet_type_;
  uint8_t handshake_type_;
  bool injected_;
  DataBuffer data_;
};

// Make a copy of the first instance of a message.
class TlsInspectorRecordHandshakeMessage : public TlsRecordInspector {
 public:
  TlsInspectorRecordHandshakeMessage(uint8_t handshake_type)
      : handshake_type_(handshake_type), buffer_() {}

  virtual void OnRecord(DummyPrSocket* adapter, uint8_t content_type,
                        const unsigned char* data, size_t len) {
    // Only do this once.
    if (buffer_.len()) {
      return;
    }

    // Check that the first byte is as requested.
    if (content_type != kTlsHandshakeType) {
      return;
    }

    TlsParser parser(data, len);
    while (parser.remaining()) {
      unsigned char message_type;
      // Read the content type.
      if (!parser.Read(&message_type)) {
        // Malformed.
        return;
      }

      // Read the record length.
      uint32_t length;
      if (!parser.Read(&length, 3)) {
        // Malformed.
        return;
      }

      if (adapter->mode() == DGRAM) {
        // DTLS
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
      }

      unsigned char* dest = nullptr;

      if (message_type == handshake_type_) {
        buffer_.Allocate(length);
        dest = buffer_.data();
      }

      if (!parser.Read(dest, length)) {
        // Malformed
        return;
      }

      if (dest) return;
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

class TlsAgent : public PollTarget {
 public:
  enum Role { CLIENT, SERVER };
  enum State { INIT, CONNECTING, CONNECTED, ERROR };

  TlsAgent(const std::string& name, Role role, Mode mode)
      : name_(name),
        mode_(mode),
        pr_fd_(nullptr),
        adapter_(nullptr),
        ssl_fd_(nullptr),
        role_(role),
        state_(INIT) {
      memset(&info_, 0, sizeof(info_));
      memset(&csinfo_, 0, sizeof(csinfo_));
  }

  ~TlsAgent() {
    if (pr_fd_) {
      PR_Close(pr_fd_);
    }

    if (ssl_fd_) {
      PR_Close(ssl_fd_);
    }
  }

  bool Init() {
    pr_fd_ = DummyPrSocket::CreateFD(name_, mode_);
    if (!pr_fd_) return false;

    adapter_ = DummyPrSocket::GetAdapter(pr_fd_);
    if (!adapter_) return false;

    return true;
  }

  void SetPeer(TlsAgent* peer) { adapter_->SetPeer(peer->adapter_); }

  void SetInspector(Inspector* inspector) { adapter_->SetInspector(inspector); }

  void StartConnect() {
    ASSERT_TRUE(EnsureTlsSetup());

    SECStatus rv;
    rv = SSL_ResetHandshake(ssl_fd_, role_ == SERVER ? PR_TRUE : PR_FALSE);
    ASSERT_EQ(SECSuccess, rv);
    SetState(CONNECTING);
  }

  void EnableSomeECDHECiphers() {
    ASSERT_TRUE(EnsureTlsSetup());

    const uint32_t EnabledCiphers[] = {TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
                                       TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA};

    for (size_t i = 0; i < PR_ARRAY_SIZE(EnabledCiphers); ++i) {
      SECStatus rv = SSL_CipherPrefSet(ssl_fd_, EnabledCiphers[i], PR_TRUE);
      ASSERT_EQ(SECSuccess, rv);
    }
  }

  bool EnsureTlsSetup() {
    // Don't set up twice
    if (ssl_fd_) return true;

    if (adapter_->mode() == STREAM) {
      ssl_fd_ = SSL_ImportFD(nullptr, pr_fd_);
    } else {
      ssl_fd_ = DTLS_ImportFD(nullptr, pr_fd_);
    }

    EXPECT_NE(nullptr, ssl_fd_);
    if (!ssl_fd_) return false;
    pr_fd_ = nullptr;

    if (role_ == SERVER) {
      CERTCertificate* cert = PK11_FindCertFromNickname(name_.c_str(), nullptr);
      EXPECT_NE(nullptr, cert);
      if (!cert) return false;

      SECKEYPrivateKey* priv = PK11_FindKeyByAnyCert(cert, nullptr);
      EXPECT_NE(nullptr, priv);
      if (!priv) return false;  // Leak cert.

      SECStatus rv = SSL_ConfigSecureServer(ssl_fd_, cert, priv, kt_rsa);
      EXPECT_EQ(SECSuccess, rv);
      if (rv != SECSuccess) return false;  // Leak cert and key.

      SECKEY_DestroyPrivateKey(priv);
      CERT_DestroyCertificate(cert);
    } else {
      SECStatus rv = SSL_SetURL(ssl_fd_, "server");
      EXPECT_EQ(SECSuccess, rv);
      if (rv != SECSuccess) return false;
    }

    SECStatus rv = SSL_AuthCertificateHook(ssl_fd_, AuthCertificateHook,
                                           reinterpret_cast<void*>(this));
    EXPECT_EQ(SECSuccess, rv);
    if (rv != SECSuccess) return false;

    return true;
  }

  void SetSessionTicketsEnabled(bool en) {
    ASSERT_TRUE(EnsureTlsSetup());

    SECStatus rv = SSL_OptionSet(ssl_fd_, SSL_ENABLE_SESSION_TICKETS,
                                  en ? PR_TRUE : PR_FALSE);
    ASSERT_EQ(SECSuccess, rv);
  }

  void SetSessionCacheEnabled(bool en) {
    ASSERT_TRUE(EnsureTlsSetup());

    SECStatus rv = SSL_OptionSet(ssl_fd_, SSL_NO_CACHE,
                                  en ? PR_FALSE : PR_TRUE);
    ASSERT_EQ(SECSuccess, rv);
  }

  void SetVersionRange(uint16_t minver, uint16_t maxver) {
    SSLVersionRange range = {minver, maxver};
    ASSERT_EQ(SECSuccess, SSL_VersionRangeSet(ssl_fd_, &range));
  }

  State state() const { return state_; }

  const char* state_str() const { return state_str(state()); }

  const char* state_str(State state) const { return states[state]; }

  PRFileDesc* ssl_fd() { return ssl_fd_; }

  bool version(uint16_t* version) const {
    if (state_ != CONNECTED) return false;

    *version = info_.protocolVersion;

    return true;
  }

  bool cipher_suite(int16_t* cipher_suite) const {
    if (state_ != CONNECTED) return false;

    *cipher_suite = info_.cipherSuite;
    return true;
  }

  std::string cipher_suite_name() const {
    if (state_ != CONNECTED) return "UNKNOWN";

    return csinfo_.cipherSuiteName;
  }

  void CheckKEAType(SSLKEAType type) const {
    ASSERT_EQ(CONNECTED, state_);
    ASSERT_EQ(type, csinfo_.keaType);
  }

  void CheckVersion(uint16_t version) const {
    ASSERT_EQ(CONNECTED, state_);
    ASSERT_EQ(version, info_.protocolVersion);
  }

  void Handshake() {
    SECStatus rv = SSL_ForceHandshake(ssl_fd_);
    if (rv == SECSuccess) {
      LOG("Handshake success");
      SECStatus rv = SSL_GetChannelInfo(ssl_fd_, &info_, sizeof(info_));
      ASSERT_EQ(SECSuccess, rv);

      rv = SSL_GetCipherSuiteInfo(info_.cipherSuite, &csinfo_, sizeof(csinfo_));
      ASSERT_EQ(SECSuccess, rv);

      SetState(CONNECTED);
      return;
    }

    int32_t err = PR_GetError();
    switch (err) {
      case PR_WOULD_BLOCK_ERROR:
        LOG("Would have blocked");
        // TODO(ekr@rtfm.com): set DTLS timeouts
        Poller::Instance()->Wait(READABLE_EVENT, adapter_, this,
                                 &TlsAgent::ReadableCallback);
        return;
        break;

      // TODO(ekr@rtfm.com): needs special case for DTLS
      case SSL_ERROR_RX_MALFORMED_HANDSHAKE:
      default:
        LOG("Handshake failed with error " << err);
        SetState(ERROR);
        return;
    }
  }

  std::vector<uint8_t> GetSessionId() {
    return std::vector<uint8_t>(info_.sessionID,
                                info_.sessionID + info_.sessionIDLength);
  }

 private:
  const static char* states[];

  void SetState(State state) {
    if (state_ == state) return;

    LOG("Changing state from " << state_str(state_) << " to "
                               << state_str(state));
    state_ = state;
  }

  // Dummy auth certificate hook.
  static SECStatus AuthCertificateHook(void* arg, PRFileDesc* fd,
                                       PRBool checksig, PRBool isServer) {
    return SECSuccess;
  }

  static void ReadableCallback(PollTarget* self, Event event) {
    TlsAgent* agent = static_cast<TlsAgent*>(self);
    agent->ReadableCallback_int(event);
  }

  void ReadableCallback_int(Event event) {
    LOG("Readable");
    Handshake();
  }

  const std::string name_;
  Mode mode_;
  PRFileDesc* pr_fd_;
  DummyPrSocket* adapter_;
  PRFileDesc* ssl_fd_;
  Role role_;
  State state_;
  SSLChannelInfo info_;
  SSLCipherSuiteInfo csinfo_;
};

const char* TlsAgent::states[] = {"INIT", "CONNECTING", "CONNECTED", "ERROR"};

class TlsConnectTestBase : public ::testing::Test {
 public:
  TlsConnectTestBase(Mode mode)
      : mode_(mode),
        client_(new TlsAgent("client", TlsAgent::CLIENT, mode_)),
        server_(new TlsAgent("server", TlsAgent::SERVER, mode_)) {}

  ~TlsConnectTestBase() {
    delete client_;
    delete server_;
  }

  void SetUp() {
    // Configure a fresh session cache.
    SSL_ConfigServerSessionIDCache(1024, 0, 0, g_working_dir_path.c_str());

    Init();
  }

  void TearDown() {
    client_ = nullptr;
    server_ = nullptr;

    SSL_ShutdownServerSessionIDCache();
  }

  void Init() {
    ASSERT_TRUE(client_->Init());
    ASSERT_TRUE(server_->Init());

    client_->SetPeer(server_);
    server_->SetPeer(client_);
  }

  void Reset() {
    delete client_;
    delete server_;

    client_ = new TlsAgent("client", TlsAgent::CLIENT, mode_);
    server_ = new TlsAgent("server", TlsAgent::SERVER, mode_);

    Init();
  }

  void EnsureTlsSetup() {
    ASSERT_TRUE(client_->EnsureTlsSetup());
    ASSERT_TRUE(server_->EnsureTlsSetup());
  }

  void Connect() {
    server_->StartConnect();  // Server
    client_->StartConnect();  // Client
    client_->Handshake();
    server_->Handshake();

    ASSERT_TRUE_WAIT(client_->state() != TlsAgent::CONNECTING &&
                         server_->state() != TlsAgent::CONNECTING,
                     5000);
    ASSERT_EQ(TlsAgent::CONNECTED, server_->state());

    int16_t cipher_suite1, cipher_suite2;
    bool ret = client_->cipher_suite(&cipher_suite1);
    ASSERT_TRUE(ret);
    ret = server_->cipher_suite(&cipher_suite2);
    ASSERT_TRUE(ret);
    ASSERT_EQ(cipher_suite1, cipher_suite2);

    std::cerr << "Connected with cipher suite " << client_->cipher_suite_name()
              << std::endl;

    // Check and store session ids.
    std::vector<uint8_t> sid_c1 = client_->GetSessionId();
    ASSERT_EQ(32, sid_c1.size());
    std::vector<uint8_t> sid_s1 = server_->GetSessionId();
    ASSERT_EQ(32, sid_s1.size());
    ASSERT_EQ(sid_c1, sid_s1);
    session_id_ = sid_c1;
  }

  void EnableSomeECDHECiphers() {
    client_->EnableSomeECDHECiphers();
    server_->EnableSomeECDHECiphers();
  }

 protected:
  Mode mode_;
  TlsAgent* client_;
  TlsAgent* server_;
  std::vector<uint8_t> session_id_;
};

class TlsConnectTest : public TlsConnectTestBase {
 public:
  TlsConnectTest() : TlsConnectTestBase(STREAM) {}
};

class DtlsConnectTest : public TlsConnectTestBase {
 public:
  DtlsConnectTest() : TlsConnectTestBase(DGRAM) {}
};

class TlsConnectGeneric : public TlsConnectTestBase,
                          public ::testing::WithParamInterface<std::string> {
 public:
  TlsConnectGeneric()
      : TlsConnectTestBase((GetParam() == "TLS") ? STREAM : DGRAM) {
    std::cerr << "Variant: " << GetParam() << std::endl;
  }
};

TEST_P(TlsConnectGeneric, SetupOnly) {}

TEST_P(TlsConnectGeneric, Connect) {
  Connect();

  // Check that we negotiated the expected version.
  if (mode_ == STREAM) {
    client_->CheckVersion(SSL_LIBRARY_VERSION_TLS_1_0);
  } else {
    client_->CheckVersion(SSL_LIBRARY_VERSION_TLS_1_1);
  }
}

TEST_P(TlsConnectGeneric, ConnectResumed) {
  Connect();
  std::vector<uint8_t> old_sid = session_id_;

  Reset();
  Connect();
  ASSERT_EQ(old_sid, session_id_) << "Session was not resumed when it should have been";
}

TEST_P(TlsConnectGeneric, ConnectNotResumed) {
  Connect();
  std::vector<uint8_t> old_sid = session_id_;

  Reset();
  client_->SetSessionCacheEnabled(false);
  Connect();

  ASSERT_NE(old_sid, session_id_) << "Session was resumed when it should not have been";
}

TEST_P(TlsConnectGeneric, ConnectTLS_1_1_Only) {
  EnsureTlsSetup();
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_1);

  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_1,
                           SSL_LIBRARY_VERSION_TLS_1_1);

  Connect();

  client_->CheckVersion(SSL_LIBRARY_VERSION_TLS_1_1);
}

TEST_P(TlsConnectGeneric, ConnectTLS_1_2_Only) {
  EnsureTlsSetup();
  client_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  server_->SetVersionRange(SSL_LIBRARY_VERSION_TLS_1_2,
                           SSL_LIBRARY_VERSION_TLS_1_2);
  Connect();
  client_->CheckVersion(SSL_LIBRARY_VERSION_TLS_1_2);
}

TEST_F(TlsConnectTest, ConnectECDHE) {
  EnableSomeECDHECiphers();
  Connect();
  client_->CheckKEAType(ssl_kea_ecdh);
}

TEST_F(TlsConnectTest, ConnectECDHETwiceReuseKey) {
  EnableSomeECDHECiphers();
  TlsInspectorRecordHandshakeMessage* i1 =
      new TlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  server_->SetInspector(i1);
  Connect();
  client_->CheckKEAType(ssl_kea_ecdh);
  TlsServerKeyExchangeECDHE dhe1;
  ASSERT_TRUE(dhe1.Parse(i1->buffer().data(), i1->buffer().len()));

  // Restart
  Reset();
  TlsInspectorRecordHandshakeMessage* i2 =
      new TlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  server_->SetInspector(i2);
  EnableSomeECDHECiphers();
  client_->SetSessionCacheEnabled(false);
  Connect();
  client_->CheckKEAType(ssl_kea_ecdh);

  TlsServerKeyExchangeECDHE dhe2;
  ASSERT_TRUE(dhe2.Parse(i2->buffer().data(), i2->buffer().len()));

  // Make sure they are the same.
  ASSERT_EQ(dhe1.public_key_.len(), dhe2.public_key_.len());
  ASSERT_TRUE(!memcmp(dhe1.public_key_.data(), dhe2.public_key_.data(),
                      dhe1.public_key_.len()));
}

TEST_F(TlsConnectTest, ConnectECDHETwiceNewKey) {
  EnableSomeECDHECiphers();
  SECStatus rv =
      SSL_OptionSet(server_->ssl_fd(), SSL_REUSE_SERVER_ECDHE_KEY, PR_FALSE);
  ASSERT_EQ(SECSuccess, rv);
  TlsInspectorRecordHandshakeMessage* i1 =
      new TlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  server_->SetInspector(i1);
  Connect();
  client_->CheckKEAType(ssl_kea_ecdh);
  TlsServerKeyExchangeECDHE dhe1;
  ASSERT_TRUE(dhe1.Parse(i1->buffer().data(), i1->buffer().len()));

  // Restart
  Reset();
  EnableSomeECDHECiphers();
  rv = SSL_OptionSet(server_->ssl_fd(), SSL_REUSE_SERVER_ECDHE_KEY, PR_FALSE);
  ASSERT_EQ(SECSuccess, rv);
  TlsInspectorRecordHandshakeMessage* i2 =
      new TlsInspectorRecordHandshakeMessage(kTlsHandshakeServerKeyExchange);
  server_->SetInspector(i2);
  client_->SetSessionCacheEnabled(false);
  Connect();
  client_->CheckKEAType(ssl_kea_ecdh);

  TlsServerKeyExchangeECDHE dhe2;
  ASSERT_TRUE(dhe2.Parse(i2->buffer().data(), i2->buffer().len()));

  // Make sure they are different.
  ASSERT_FALSE((dhe1.public_key_.len() == dhe2.public_key_.len()) &&
               (!memcmp(dhe1.public_key_.data(), dhe2.public_key_.data(),
                        dhe1.public_key_.len())));
}

INSTANTIATE_TEST_CASE_P(Variants, TlsConnectGeneric,
                        ::testing::Values("TLS", "DTLS"));

}  // namespace nspr_test
