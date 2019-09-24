/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ssl.h"
#include "sslerr.h"
#include "sslproto.h"

#include <memory>

#include "tls_connect.h"
#include "tls_filter.h"

namespace nss_test {

class TlsCipherOrderTest : public TlsConnectTestBase {
 protected:
  virtual void ConfigureTLS() {
    EnsureTlsSetup();
    ConfigureVersion(SSL_LIBRARY_VERSION_TLS_1_3);
  }

  virtual SECStatus BuildTestLists(std::vector<uint16_t> &cs_initial_list,
                                   std::vector<uint16_t> &cs_new_list) {
    // This is the current CipherSuites order of enabled CipherSuites as defined
    // in ssl3con.c
    const PRUint16 *kCipherSuites = SSL_GetImplementedCiphers();

    for (unsigned int i = 0; i < kNumImplementedCiphers; i++) {
      PRBool pref = PR_FALSE, policy = PR_FALSE;
      SECStatus rv;
      rv = SSL_CipherPolicyGet(kCipherSuites[i], &policy);
      if (rv != SECSuccess) {
        return SECFailure;
      }
      rv = SSL_CipherPrefGetDefault(kCipherSuites[i], &pref);
      if (rv != SECSuccess) {
        return SECFailure;
      }
      if (pref && policy) {
        cs_initial_list.push_back(kCipherSuites[i]);
      }
    }

    // We will test set function with the first 15 enabled ciphers.
    const PRUint16 kNumCiphersToSet = 15;
    for (unsigned int i = 0; i < kNumCiphersToSet; i++) {
      cs_new_list.push_back(cs_initial_list[i]);
    }
    cs_new_list[0] = cs_initial_list[1];
    cs_new_list[1] = cs_initial_list[0];
    return SECSuccess;
  }

 public:
  TlsCipherOrderTest() : TlsConnectTestBase(ssl_variant_stream, 0) {}
  const unsigned int kNumImplementedCiphers = SSL_GetNumImplementedCiphers();
};

const PRUint16 kCSUnsupported[] = {20196, 10101};
const PRUint16 kNumCSUnsupported = PR_ARRAY_SIZE(kCSUnsupported);
const PRUint16 kCSEmpty[] = {0};

// Get the active CipherSuites odered as they were compiled
TEST_F(TlsCipherOrderTest, CipherOrderGet) {
  std::vector<uint16_t> initial_cs_order;
  std::vector<uint16_t> new_cs_order;
  SECStatus result = BuildTestLists(initial_cs_order, new_cs_order);
  ASSERT_EQ(result, SECSuccess);
  ConfigureTLS();

  std::vector<uint16_t> current_cs_order(SSL_GetNumImplementedCiphers() + 1);
  unsigned int current_num_active_cs = 0;
  result = SSL_CipherSuiteOrderGet(client_->ssl_fd(), current_cs_order.data(),
                                   &current_num_active_cs);
  ASSERT_EQ(result, SECSuccess);
  ASSERT_EQ(current_num_active_cs, initial_cs_order.size());
  for (unsigned int i = 0; i < initial_cs_order.size(); i++) {
    EXPECT_EQ(initial_cs_order[i], current_cs_order[i]);
  }
  // Get the chosen CipherSuite during the Handshake without any modification.
  Connect();
  SSLChannelInfo channel;
  result = SSL_GetChannelInfo(client_->ssl_fd(), &channel, sizeof channel);
  ASSERT_EQ(result, SECSuccess);
  EXPECT_EQ(channel.cipherSuite, initial_cs_order[0]);
}

// The "server" used for gtests honor only its ciphersuites order.
// So, we apply the new set for the server instead of client.
// This is enough to test the effect of SSL_CipherSuiteOrderSet function.
TEST_F(TlsCipherOrderTest, CipherOrderSet) {
  std::vector<uint16_t> initial_cs_order;
  std::vector<uint16_t> new_cs_order;
  SECStatus result = BuildTestLists(initial_cs_order, new_cs_order);
  ASSERT_EQ(result, SECSuccess);
  ConfigureTLS();

  // change the server_ ciphersuites order.
  result = SSL_CipherSuiteOrderSet(server_->ssl_fd(), new_cs_order.data(),
                                   new_cs_order.size());
  ASSERT_EQ(result, SECSuccess);

  // The function expect an array. We are using vector for VStudio
  // compatibility.
  std::vector<uint16_t> current_cs_order(SSL_GetNumImplementedCiphers() + 1);
  unsigned int current_num_active_cs = 0;
  result = SSL_CipherSuiteOrderGet(server_->ssl_fd(), current_cs_order.data(),
                                   &current_num_active_cs);
  ASSERT_EQ(result, SECSuccess);
  ASSERT_EQ(current_num_active_cs, new_cs_order.size());
  for (unsigned int i = 0; i < new_cs_order.size(); i++) {
    ASSERT_EQ(new_cs_order[i], current_cs_order[i]);
  }

  Connect();
  SSLChannelInfo channel;
  // changes in server_ order reflect in client chosen ciphersuite.
  result = SSL_GetChannelInfo(client_->ssl_fd(), &channel, sizeof channel);
  ASSERT_EQ(result, SECSuccess);
  EXPECT_EQ(channel.cipherSuite, new_cs_order[0]);
}

// Duplicate socket configuration from a model.
TEST_F(TlsCipherOrderTest, CipherOrderCopySocket) {
  std::vector<uint16_t> initial_cs_order;
  std::vector<uint16_t> new_cs_order;
  SECStatus result = BuildTestLists(initial_cs_order, new_cs_order);
  ASSERT_EQ(result, SECSuccess);
  ConfigureTLS();

  // Use the existing sockets for this test.
  result = SSL_CipherSuiteOrderSet(client_->ssl_fd(), new_cs_order.data(),
                                   new_cs_order.size());
  ASSERT_EQ(result, SECSuccess);

  std::vector<uint16_t> current_cs_order(SSL_GetNumImplementedCiphers() + 1);
  unsigned int current_num_active_cs = 0;
  result = SSL_CipherSuiteOrderGet(server_->ssl_fd(), current_cs_order.data(),
                                   &current_num_active_cs);
  ASSERT_EQ(result, SECSuccess);
  ASSERT_EQ(current_num_active_cs, initial_cs_order.size());
  for (unsigned int i = 0; i < current_num_active_cs; i++) {
    ASSERT_EQ(initial_cs_order[i], current_cs_order[i]);
  }

  // Import/Duplicate configurations from client_ to server_
  PRFileDesc *rv = SSL_ImportFD(client_->ssl_fd(), server_->ssl_fd());
  EXPECT_NE(nullptr, rv);

  result = SSL_CipherSuiteOrderGet(server_->ssl_fd(), current_cs_order.data(),
                                   &current_num_active_cs);
  ASSERT_EQ(result, SECSuccess);
  ASSERT_EQ(current_num_active_cs, new_cs_order.size());
  for (unsigned int i = 0; i < new_cs_order.size(); i++) {
    EXPECT_EQ(new_cs_order.data()[i], current_cs_order[i]);
  }
}

// If the infomed num of elements is lower than the actual list size, only the
// first "informed num" elements will be considered. The rest is ignored.
TEST_F(TlsCipherOrderTest, CipherOrderSetLower) {
  std::vector<uint16_t> initial_cs_order;
  std::vector<uint16_t> new_cs_order;
  SECStatus result = BuildTestLists(initial_cs_order, new_cs_order);
  ASSERT_EQ(result, SECSuccess);
  ConfigureTLS();

  result = SSL_CipherSuiteOrderSet(client_->ssl_fd(), new_cs_order.data(),
                                   new_cs_order.size() - 1);
  ASSERT_EQ(result, SECSuccess);

  std::vector<uint16_t> current_cs_order(SSL_GetNumImplementedCiphers() + 1);
  unsigned int current_num_active_cs = 0;
  result = SSL_CipherSuiteOrderGet(client_->ssl_fd(), current_cs_order.data(),
                                   &current_num_active_cs);
  ASSERT_EQ(result, SECSuccess);
  ASSERT_EQ(current_num_active_cs, new_cs_order.size() - 1);
  for (unsigned int i = 0; i < new_cs_order.size() - 1; i++) {
    ASSERT_EQ(new_cs_order.data()[i], current_cs_order[i]);
  }
}

// Testing Errors Controls
TEST_F(TlsCipherOrderTest, CipherOrderSetControls) {
  std::vector<uint16_t> initial_cs_order;
  std::vector<uint16_t> new_cs_order;
  SECStatus result = BuildTestLists(initial_cs_order, new_cs_order);
  ASSERT_EQ(result, SECSuccess);
  ConfigureTLS();

  // Create a new vector with diplicated entries
  std::vector<uint16_t> repeated_cs_order(SSL_GetNumImplementedCiphers() + 1);
  std::copy(initial_cs_order.begin(), initial_cs_order.end(),
            repeated_cs_order.begin());
  repeated_cs_order[0] = repeated_cs_order[1];

  // Repeated ciphersuites in the list
  result = SSL_CipherSuiteOrderSet(client_->ssl_fd(), repeated_cs_order.data(),
                                   initial_cs_order.size());
  EXPECT_EQ(result, SECFailure);

  // Zero size for the sent list
  result = SSL_CipherSuiteOrderSet(client_->ssl_fd(), new_cs_order.data(), 0);
  EXPECT_EQ(result, SECFailure);

  // Wrong size, greater than actual
  result = SSL_CipherSuiteOrderSet(client_->ssl_fd(), new_cs_order.data(),
                                   SSL_GetNumImplementedCiphers() + 1);
  EXPECT_EQ(result, SECFailure);

  // Wrong ciphersuites, not implemented
  result = SSL_CipherSuiteOrderSet(client_->ssl_fd(), kCSUnsupported,
                                   kNumCSUnsupported);
  EXPECT_EQ(result, SECFailure);

  // Null list
  result =
      SSL_CipherSuiteOrderSet(client_->ssl_fd(), nullptr, new_cs_order.size());
  EXPECT_EQ(result, SECFailure);

  // Empty list
  result =
      SSL_CipherSuiteOrderSet(client_->ssl_fd(), kCSEmpty, new_cs_order.size());
  EXPECT_EQ(result, SECFailure);

  // Confirm that the controls are working, as the current ciphersuites
  // remained untouched
  std::vector<uint16_t> current_cs_order(SSL_GetNumImplementedCiphers() + 1);
  unsigned int current_num_active_cs = 0;
  result = SSL_CipherSuiteOrderGet(client_->ssl_fd(), current_cs_order.data(),
                                   &current_num_active_cs);
  ASSERT_EQ(result, SECSuccess);
  ASSERT_EQ(current_num_active_cs, initial_cs_order.size());
  for (unsigned int i = 0; i < initial_cs_order.size(); i++) {
    ASSERT_EQ(initial_cs_order[i], current_cs_order[i]);
  }
}
}  // namespace nss_test
