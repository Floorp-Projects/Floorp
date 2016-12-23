/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef tls_agent_h_
#define tls_agent_h_

#include "prio.h"
#include "ssl.h"

#include <functional>
#include <iostream>

#include "test_io.h"
#include "tls_filter.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

extern bool g_ssl_gtest_verbose;

namespace nss_test {

#define LOG(msg) std::cerr << role_str() << ": " << msg << std::endl
#define LOGV(msg)                      \
  do {                                 \
    if (g_ssl_gtest_verbose) LOG(msg); \
  } while (false)

enum SessionResumptionMode {
  RESUME_NONE = 0,
  RESUME_SESSIONID = 1,
  RESUME_TICKET = 2,
  RESUME_BOTH = RESUME_SESSIONID | RESUME_TICKET
};

class TlsAgent;

const extern std::vector<SSLNamedGroup> kAllDHEGroups;
const extern std::vector<SSLNamedGroup> kECDHEGroups;
const extern std::vector<SSLNamedGroup> kFFDHEGroups;
const extern std::vector<SSLNamedGroup> kFasterDHEGroups;

typedef std::function<SECStatus(TlsAgent* agent, bool checksig, bool isServer)>
    AuthCertificateCallbackFunction;

typedef std::function<void(TlsAgent* agent)> HandshakeCallbackFunction;

typedef std::function<int32_t(TlsAgent* agent, const SECItem* srvNameArr,
                              PRUint32 srvNameArrSize)>
    SniCallbackFunction;

class TlsAgent : public PollTarget {
 public:
  enum Role { CLIENT, SERVER };
  enum State { STATE_INIT, STATE_CONNECTING, STATE_CONNECTED, STATE_ERROR };

  static const std::string kClient;     // the client key is sign only
  static const std::string kRsa2048;    // bigger sign and encrypt for either
  static const std::string kServerRsa;  // both sign and encrypt
  static const std::string kServerRsaSign;
  static const std::string kServerRsaPss;
  static const std::string kServerRsaDecrypt;
  static const std::string kServerRsaChain;  // A cert that requires a chain.
  static const std::string kServerEcdsa256;
  static const std::string kServerEcdsa384;
  static const std::string kServerEcdsa521;
  static const std::string kServerEcdhEcdsa;
  static const std::string kServerEcdhRsa;
  static const std::string kServerDsa;

  TlsAgent(const std::string& name, Role role, Mode mode);
  virtual ~TlsAgent();

  bool Init() {
    pr_fd_ = DummyPrSocket::CreateFD(role_str(), mode_);
    if (!pr_fd_) return false;

    adapter_ = DummyPrSocket::GetAdapter(pr_fd_);
    if (!adapter_) return false;

    return true;
  }

  void SetPeer(TlsAgent* peer) { adapter_->SetPeer(peer->adapter_); }

  void SetPacketFilter(TlsRecordFilter* filter) {
    filter->SetAgent(this);
    adapter_->SetPacketFilter(filter);
  }

  void SetPacketFilter(PacketFilter* filter) {
    adapter_->SetPacketFilter(filter);
  }

  void DeletePacketFilter() { adapter_->SetPacketFilter(nullptr); }

  void StartConnect(PRFileDesc* model = nullptr);
  void CheckKEA(SSLKEAType kea_type, SSLNamedGroup group,
                size_t kea_size = 0) const;
  void CheckAuthType(SSLAuthType auth_type,
                     SSLSignatureScheme sig_scheme) const;

  void DisableAllCiphers();
  void EnableCiphersByAuthType(SSLAuthType authType);
  void EnableCiphersByKeyExchange(SSLKEAType kea);
  void EnableGroupsByKeyExchange(SSLKEAType kea);
  void EnableGroupsByAuthType(SSLAuthType authType);
  void EnableSingleCipher(uint16_t cipher);

  void Handshake();
  // Marks the internal state as CONNECTING in anticipation of renegotiation.
  void PrepareForRenegotiate();
  // Prepares for renegotiation, then actually triggers it.
  void StartRenegotiate();
  bool ConfigServerCert(const std::string& name, bool updateKeyBits = false,
                        const SSLExtraServerCertData* serverCertData = nullptr);
  bool ConfigServerCertWithChain(const std::string& name);
  bool EnsureTlsSetup(PRFileDesc* modelSocket = nullptr);

  void SetupClientAuth();
  void RequestClientAuth(bool requireAuth);
  bool GetClientAuthCredentials(CERTCertificate** cert,
                                SECKEYPrivateKey** priv) const;

  void ConfigureSessionCache(SessionResumptionMode mode);
  void SetSessionTicketsEnabled(bool en);
  void SetSessionCacheEnabled(bool en);
  void Set0RttEnabled(bool en);
  void SetShortHeadersEnabled();
  void SetVersionRange(uint16_t minver, uint16_t maxver);
  void GetVersionRange(uint16_t* minver, uint16_t* maxver);
  void CheckPreliminaryInfo();
  void ResetPreliminaryInfo();
  void SetExpectedVersion(uint16_t version);
  void SetServerKeyBits(uint16_t bits);
  void ExpectReadWriteError();
  void EnableFalseStart();
  void ExpectResumption();
  void ExpectShortHeaders();
  void SetSignatureSchemes(const SSLSignatureScheme* schemes, size_t count);
  void EnableAlpn(const uint8_t* val, size_t len);
  void CheckAlpn(SSLNextProtoState expected_state,
                 const std::string& expected = "") const;
  void EnableSrtp();
  void CheckSrtp() const;
  void CheckErrorCode(int32_t expected) const;
  void WaitForErrorCode(int32_t expected, uint32_t delay) const;
  // Send data on the socket, encrypting it.
  void SendData(size_t bytes, size_t blocksize = 1024);
  void SendBuffer(const DataBuffer& buf);
  // Send data directly to the underlying socket, skipping the TLS layer.
  void SendDirect(const DataBuffer& buf);
  void ReadBytes();
  void ResetSentBytes();  // Hack to test drops.
  void EnableExtendedMasterSecret();
  void CheckExtendedMasterSecret(bool expected);
  void CheckEarlyDataAccepted(bool expected);
  void DisableRollbackDetection();
  void EnableCompression();
  void SetDowngradeCheckVersion(uint16_t version);
  void CheckSecretsDestroyed();
  void ConfigNamedGroups(const std::vector<SSLNamedGroup>& groups);
  void DisableECDHEServerKeyReuse();
  bool GetPeerChainLength(size_t* count);

  const std::string& name() const { return name_; }

  Role role() const { return role_; }
  std::string role_str() const { return role_ == SERVER ? "server" : "client"; }

  State state() const { return state_; }

  const CERTCertificate* peer_cert() const {
    return SSL_PeerCertificate(ssl_fd_);
  }

  const char* state_str() const { return state_str(state()); }

  static const char* state_str(State state) { return states[state]; }

  PRFileDesc* ssl_fd() const { return ssl_fd_; }
  DummyPrSocket* adapter() { return adapter_; }

  bool is_compressed() const {
    return info_.compressionMethod != ssl_compression_null;
  }
  uint16_t server_key_bits() const { return server_key_bits_; }
  uint16_t min_version() const { return vrange_.min; }
  uint16_t max_version() const { return vrange_.max; }
  uint16_t version() const {
    EXPECT_EQ(STATE_CONNECTED, state_);
    return info_.protocolVersion;
  }

  bool cipher_suite(uint16_t* cipher_suite) const {
    if (state_ != STATE_CONNECTED) return false;

    *cipher_suite = info_.cipherSuite;
    return true;
  }

  std::string cipher_suite_name() const {
    if (state_ != STATE_CONNECTED) return "UNKNOWN";

    return csinfo_.cipherSuiteName;
  }

  std::vector<uint8_t> session_id() const {
    return std::vector<uint8_t>(info_.sessionID,
                                info_.sessionID + info_.sessionIDLength);
  }

  bool auth_type(SSLAuthType* auth_type) const {
    if (state_ != STATE_CONNECTED) return false;

    *auth_type = info_.authType;
    return true;
  }

  bool kea_type(SSLKEAType* kea_type) const {
    if (state_ != STATE_CONNECTED) return false;

    *kea_type = info_.keaType;
    return true;
  }

  size_t received_bytes() const { return recv_ctr_; }
  PRErrorCode error_code() const { return error_code_; }

  bool can_falsestart_hook_called() const {
    return can_falsestart_hook_called_;
  }

  void SetHandshakeCallback(HandshakeCallbackFunction handshake_callback) {
    handshake_callback_ = handshake_callback;
  }

  void SetAuthCertificateCallback(
      AuthCertificateCallbackFunction auth_certificate_callback) {
    auth_certificate_callback_ = auth_certificate_callback;
  }

  void SetSniCallback(SniCallbackFunction sni_callback) {
    sni_callback_ = sni_callback;
  }

 private:
  const static char* states[];

  void SetState(State state);

  // Dummy auth certificate hook.
  static SECStatus AuthCertificateHook(void* arg, PRFileDesc* fd,
                                       PRBool checksig, PRBool isServer) {
    TlsAgent* agent = reinterpret_cast<TlsAgent*>(arg);
    agent->CheckPreliminaryInfo();
    agent->auth_certificate_hook_called_ = true;
    if (agent->auth_certificate_callback_) {
      return agent->auth_certificate_callback_(agent, checksig ? true : false,
                                               isServer ? true : false);
    }
    return SECSuccess;
  }

  // Client auth certificate hook.
  static SECStatus ClientAuthenticated(void* arg, PRFileDesc* fd,
                                       PRBool checksig, PRBool isServer) {
    TlsAgent* agent = reinterpret_cast<TlsAgent*>(arg);
    EXPECT_TRUE(agent->expect_client_auth_);
    EXPECT_EQ(PR_TRUE, isServer);
    if (agent->auth_certificate_callback_) {
      return agent->auth_certificate_callback_(agent, checksig ? true : false,
                                               isServer ? true : false);
    }
    return SECSuccess;
  }

  static SECStatus GetClientAuthDataHook(void* self, PRFileDesc* fd,
                                         CERTDistNames* caNames,
                                         CERTCertificate** cert,
                                         SECKEYPrivateKey** privKey);

  static void ReadableCallback(PollTarget* self, Event event) {
    TlsAgent* agent = static_cast<TlsAgent*>(self);
    if (event == TIMER_EVENT) {
      agent->timer_handle_ = nullptr;
    }
    agent->ReadableCallback_int();
  }

  void ReadableCallback_int() {
    LOGV("Readable");
    switch (state_) {
      case STATE_CONNECTING:
        Handshake();
        break;
      case STATE_CONNECTED:
        ReadBytes();
        break;
      default:
        break;
    }
  }

  static PRInt32 SniHook(PRFileDesc* fd, const SECItem* srvNameArr,
                         PRUint32 srvNameArrSize, void* arg) {
    TlsAgent* agent = reinterpret_cast<TlsAgent*>(arg);
    agent->CheckPreliminaryInfo();
    agent->sni_hook_called_ = true;
    EXPECT_EQ(1UL, srvNameArrSize);
    if (agent->sni_callback_) {
      return agent->sni_callback_(agent, srvNameArr, srvNameArrSize);
    }
    return 0;  // First configuration.
  }

  static SECStatus CanFalseStartCallback(PRFileDesc* fd, void* arg,
                                         PRBool* canFalseStart) {
    TlsAgent* agent = reinterpret_cast<TlsAgent*>(arg);
    agent->CheckPreliminaryInfo();
    EXPECT_TRUE(agent->falsestart_enabled_);
    EXPECT_FALSE(agent->can_falsestart_hook_called_);
    agent->can_falsestart_hook_called_ = true;
    *canFalseStart = true;
    return SECSuccess;
  }

  static void HandshakeCallback(PRFileDesc* fd, void* arg) {
    TlsAgent* agent = reinterpret_cast<TlsAgent*>(arg);
    agent->handshake_callback_called_ = true;
    agent->Connected();
    if (agent->handshake_callback_) {
      agent->handshake_callback_(agent);
    }
  }

  void DisableLameGroups();
  void ConfigStrongECGroups(bool en);
  void ConfigAllDHGroups(bool en);
  void CheckCallbacks() const;
  void Connected();

  const std::string name_;
  Mode mode_;
  uint16_t server_key_bits_;
  PRFileDesc* pr_fd_;
  DummyPrSocket* adapter_;
  PRFileDesc* ssl_fd_;
  Role role_;
  State state_;
  Poller::Timer* timer_handle_;
  bool falsestart_enabled_;
  uint16_t expected_version_;
  uint16_t expected_cipher_suite_;
  bool expect_resumption_;
  bool expect_client_auth_;
  bool can_falsestart_hook_called_;
  bool sni_hook_called_;
  bool auth_certificate_hook_called_;
  bool handshake_callback_called_;
  SSLChannelInfo info_;
  SSLCipherSuiteInfo csinfo_;
  SSLVersionRange vrange_;
  PRErrorCode error_code_;
  size_t send_ctr_;
  size_t recv_ctr_;
  bool expect_readwrite_error_;
  HandshakeCallbackFunction handshake_callback_;
  AuthCertificateCallbackFunction auth_certificate_callback_;
  SniCallbackFunction sni_callback_;
  bool expect_short_headers_;
};

inline std::ostream& operator<<(std::ostream& stream,
                                const TlsAgent::State& state) {
  return stream << TlsAgent::state_str(state);
}

class TlsAgentTestBase : public ::testing::Test {
 public:
  static ::testing::internal::ParamGenerator<std::string> kTlsRolesAll;

  TlsAgentTestBase(TlsAgent::Role role, Mode mode)
      : agent_(nullptr), fd_(nullptr), role_(role), mode_(mode) {}
  ~TlsAgentTestBase() {
    if (fd_) {
      PR_Close(fd_);
    }
  }

  void SetUp();
  void TearDown();

  static void MakeRecord(Mode mode, uint8_t type, uint16_t version,
                         const uint8_t* buf, size_t len, DataBuffer* out,
                         uint64_t seq_num = 0);
  void MakeRecord(uint8_t type, uint16_t version, const uint8_t* buf,
                  size_t len, DataBuffer* out, uint64_t seq_num = 0) const;
  void MakeHandshakeMessage(uint8_t hs_type, const uint8_t* data, size_t hs_len,
                            DataBuffer* out, uint64_t seq_num = 0) const;
  void MakeHandshakeMessageFragment(uint8_t hs_type, const uint8_t* data,
                                    size_t hs_len, DataBuffer* out,
                                    uint64_t seq_num, uint32_t fragment_offset,
                                    uint32_t fragment_length) const;
  static void MakeTrivialHandshakeRecord(uint8_t hs_type, size_t hs_len,
                                         DataBuffer* out);
  static inline TlsAgent::Role ToRole(const std::string& str) {
    return str == "CLIENT" ? TlsAgent::CLIENT : TlsAgent::SERVER;
  }

  static inline Mode ToMode(const std::string& str) {
    return str == "TLS" ? STREAM : DGRAM;
  }

  void Init(const std::string& server_name = TlsAgent::kServerRsa);
  void Reset(const std::string& server_name = TlsAgent::kServerRsa);

 protected:
  void EnsureInit();
  void ProcessMessage(const DataBuffer& buffer, TlsAgent::State expected_state,
                      int32_t error_code = 0);

  TlsAgent* agent_;
  PRFileDesc* fd_;
  TlsAgent::Role role_;
  Mode mode_;
};

class TlsAgentTest : public TlsAgentTestBase,
                     public ::testing::WithParamInterface<
                         std::tuple<std::string, std::string>> {
 public:
  TlsAgentTest()
      : TlsAgentTestBase(ToRole(std::get<0>(GetParam())),
                         ToMode(std::get<1>(GetParam()))) {}
};

class TlsAgentTestClient : public TlsAgentTestBase,
                           public ::testing::WithParamInterface<std::string> {
 public:
  TlsAgentTestClient()
      : TlsAgentTestBase(TlsAgent::CLIENT, ToMode(GetParam())) {}
};

class TlsAgentStreamTestClient : public TlsAgentTestBase {
 public:
  TlsAgentStreamTestClient() : TlsAgentTestBase(TlsAgent::CLIENT, STREAM) {}
};

class TlsAgentStreamTestServer : public TlsAgentTestBase {
 public:
  TlsAgentStreamTestServer() : TlsAgentTestBase(TlsAgent::SERVER, STREAM) {}
};

class TlsAgentDgramTestClient : public TlsAgentTestBase {
 public:
  TlsAgentDgramTestClient() : TlsAgentTestBase(TlsAgent::CLIENT, DGRAM) {}
};

}  // namespace nss_test

#endif
