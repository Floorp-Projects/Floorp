/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef tls_connect_h_
#define tls_connect_h_

#include <tuple>

#include "sslproto.h"
#include "sslt.h"

#include "tls_agent.h"

#define GTEST_HAS_RTTI 0
#include "gtest/gtest.h"

namespace nss_test {

// A generic TLS connection test base.
class TlsConnectTestBase : public ::testing::Test {
 public:
  static ::testing::internal::ParamGenerator<std::string> kTlsModesStream;
  static ::testing::internal::ParamGenerator<std::string> kTlsModesDatagram;
  static ::testing::internal::ParamGenerator<std::string> kTlsModesAll;
  static ::testing::internal::ParamGenerator<uint16_t> kTlsV10;
  static ::testing::internal::ParamGenerator<uint16_t> kTlsV11;
  static ::testing::internal::ParamGenerator<uint16_t> kTlsV12;
  static ::testing::internal::ParamGenerator<uint16_t> kTlsV10V11;
  static ::testing::internal::ParamGenerator<uint16_t> kTlsV11V12;
  static ::testing::internal::ParamGenerator<uint16_t> kTlsV10ToV12;
  static ::testing::internal::ParamGenerator<uint16_t> kTlsV13;
  static ::testing::internal::ParamGenerator<uint16_t> kTlsV11Plus;
  static ::testing::internal::ParamGenerator<uint16_t> kTlsV12Plus;
  static ::testing::internal::ParamGenerator<uint16_t> kTlsVAll;

  static inline Mode ToMode(const std::string& str) {
    return str == "TLS" ? STREAM : DGRAM;
  }

  TlsConnectTestBase(Mode mode, uint16_t version);
  virtual ~TlsConnectTestBase();

  void SetUp();
  void TearDown();

  // Initialize client and server.
  void Init();
  // Clear the statistics.
  void ClearStats();
  // Clear the server session cache.
  void ClearServerCache();
  // Make sure TLS is configured for a connection.
  void EnsureTlsSetup();
  // Reset
  void Reset();
  // Reset, and update the server name
  void Reset(const std::string& server_name);

  // Run the handshake.
  void Handshake();
  // Connect and check that it works.
  void Connect();
  // Check that the connection was successfully established.
  void CheckConnected();
  // Connect and expect it to fail.
  void ConnectExpectFail();
  void ConnectWithCipherSuite(uint16_t cipher_suite);
  void CheckKeys(SSLKEAType kea_type, SSLAuthType auth_type,
                 size_t kea_size = 0) const;

  void SetExpectedVersion(uint16_t version);
  // Expect resumption of a particular type.
  void ExpectResumption(SessionResumptionMode expected);
  void DisableAllCiphers();
  void EnableOnlyStaticRsaCiphers();
  void EnableOnlyDheCiphers();
  void EnableSomeEcdhCiphers();
  void EnableExtendedMasterSecret();
  void ConfigureSessionCache(SessionResumptionMode client,
                             SessionResumptionMode server);
  void EnableAlpn();
  void EnableAlpn(const uint8_t* val, size_t len);
  void EnsureModelSockets();
  void CheckAlpn(const std::string& val);
  void EnableSrtp();
  void CheckSrtp() const;
  void SendReceive();
  void SetupForZeroRtt();
  void ZeroRttSendReceive(
      bool expect_readable,
      std::function<bool()> post_clienthello_check = nullptr);
  void Receive(size_t amount);
  void ExpectExtendedMasterSecret(bool expected);
  void ExpectEarlyDataAccepted(bool expected);

 protected:
  Mode mode_;
  TlsAgent* client_;
  TlsAgent* server_;
  TlsAgent* client_model_;
  TlsAgent* server_model_;
  uint16_t version_;
  SessionResumptionMode expected_resumption_mode_;
  std::vector<std::vector<uint8_t>> session_ids_;

  // A simple value of "a", "b".  Note that the preferred value of "a" is placed
  // at the end, because the NSS API follows the now defunct NPN specification,
  // which places the preferred (and default) entry at the end of the list.
  // NSS will move this final entry to the front when used with ALPN.
  const uint8_t alpn_dummy_val_[4] = { 0x01, 0x62, 0x01, 0x61 };

 private:
  void CheckResumption(SessionResumptionMode expected);
  void CheckExtendedMasterSecret();
  void CheckEarlyDataAccepted();

  bool expect_extended_master_secret_;
  bool expect_early_data_accepted_;
};

// A non-parametrized TLS test base.
class TlsConnectTest : public TlsConnectTestBase {
 public:
 TlsConnectTest() : TlsConnectTestBase(STREAM, 0) {}
};

// A non-parametrized DTLS-only test base.
class DtlsConnectTest : public TlsConnectTestBase {
 public:
  DtlsConnectTest() : TlsConnectTestBase(DGRAM, 0) {}
};

// A TLS-only test base.
class TlsConnectStream : public TlsConnectTestBase,
                         public ::testing::WithParamInterface<uint16_t> {
 public:
  TlsConnectStream() : TlsConnectTestBase(STREAM, GetParam()) {}
};

// A TLS-only test base for tests before 1.3
class TlsConnectStreamPre13 : public TlsConnectStream {
};

// A DTLS-only test base.
class TlsConnectDatagram : public TlsConnectTestBase,
                           public ::testing::WithParamInterface<uint16_t> {
 public:
  TlsConnectDatagram() : TlsConnectTestBase(DGRAM, GetParam()) {}
};

// A generic test class that can be either STREAM or DGRAM and a single version
// of TLS.  This is configured in ssl_loopback_unittest.cc.  All uses of this
// should use TEST_P().
class TlsConnectGeneric
  : public TlsConnectTestBase,
    public ::testing::WithParamInterface<std::tuple<std::string, uint16_t>> {
 public:
  TlsConnectGeneric();
};

// A Pre TLS 1.2 generic test.
class TlsConnectPre12
  : public TlsConnectTestBase,
    public ::testing::WithParamInterface<std::tuple<std::string, uint16_t>> {
 public:
  TlsConnectPre12();
};

// A TLS 1.2 only generic test.
class TlsConnectTls12
  : public TlsConnectTestBase,
    public ::testing::WithParamInterface<std::string> {
 public:
  TlsConnectTls12();
};

// A TLS 1.2+ generic test.
class TlsConnectTls12Plus
  : public TlsConnectTestBase,
    public ::testing::WithParamInterface<std::tuple<std::string, uint16_t>> {
 public:
  TlsConnectTls12Plus();
};

#ifdef NSS_ENABLE_TLS_1_3
// A TLS 1.3 only generic test.
class TlsConnectTls13
  : public TlsConnectTestBase,
    public ::testing::WithParamInterface<std::string> {
 public:
  TlsConnectTls13();
};

class TlsConnectDatagram13
  : public TlsConnectTestBase {
 public:
  TlsConnectDatagram13()
      : TlsConnectTestBase(DGRAM, SSL_LIBRARY_VERSION_TLS_1_3) {}
};
#endif

// A variant that is used only with Pre13.
class TlsConnectGenericPre13 : public TlsConnectGeneric {
};

} // namespace nss_test

#endif
